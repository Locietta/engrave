// SPDX-License-Identifier: MIT
#include "json-dom.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <glaze/json/read.hpp>

namespace json_schema {

namespace {

std::string escape_token(std::string_view token) {
    std::string escaped;
    escaped.reserve(token.size());
    for (const char c : token) {
        if (c == '~') {
            escaped += "~0";
        } else if (c == '/') {
            escaped += "~1";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

std::string unescape_token(std::string_view token) {
    std::string unescaped;
    unescaped.reserve(token.size());
    for (std::size_t i = 0; i < token.size(); ++i) {
        if (token[i] != '~') {
            unescaped += token[i];
            continue;
        }
        if (i + 1 == token.size() || (token[i + 1] != '0' && token[i + 1] != '1')) {
            throw parse_error("escape character '~' must be followed with '0' or '1'");
        }
        unescaped += token[++i] == '0' ? '~' : '/';
    }
    return unescaped;
}

bool is_array_index(std::string_view token) noexcept {
    if (token.empty()) return false;
    if (token.size() > 1 && token[0] == '0') return false;
    return std::all_of(token.begin(), token.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// A token that names an array element (not "-"); throws like nlohmann's
// json_pointer::array_index.
std::size_t array_index(const std::string &token) {
    if (token.size() > 1 && token[0] == '0') {
        throw parse_error("array index '" + token + "' must not begin with '0'");
    }
    if (!is_array_index(token)) {
        throw parse_error("array index '" + token + "' is not a number");
    }
    std::size_t index = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), index);
    if (error != std::errc{} || end != token.data() + token.size()) {
        throw out_of_range("array index " + token + " exceeds size_type");
    }
    return index;
}

template <class Json>
Json *find_impl(Json &root, const json_pointer &pointer) noexcept {
    Json *node = &root;
    for (const auto &token : pointer.tokens()) {
        if (node->is_object()) {
            auto &object = node->get_object();
            const auto found = object.find(token);
            if (found == object.end()) return nullptr;
            node = &found->second;
        } else if (node->is_array()) {
            if (!is_array_index(token)) return nullptr;
            std::size_t index = 0;
            const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), index);
            auto &array = node->get_array();
            if (error != std::errc{} || index >= array.size()) return nullptr;
            node = &array[index];
        } else {
            return nullptr;
        }
    }
    return node;
}

template <class Json>
Json &at_impl(Json &root, const json_pointer &pointer) {
    Json *node = &root;
    for (const auto &token : pointer.tokens()) {
        if (node->is_object()) {
            auto &object = node->get_object();
            const auto found = object.find(token);
            if (found == object.end()) throw out_of_range("key '" + token + "' not found");
            node = &found->second;
        } else if (node->is_array()) {
            auto &array = node->get_array();
            if (token == "-") {
                throw out_of_range("array index '-' (" + std::to_string(array.size()) + ") is out of range");
            }
            const std::size_t index = array_index(token);
            if (index >= array.size()) throw out_of_range("array index " + token + " is out of range");
            node = &array[index];
        } else {
            throw out_of_range("unresolved reference token '" + token + "'");
        }
    }
    return *node;
}

// Number kinds in nlohmann's comparison order.
enum class Kind { Null, Boolean, Number, Object, Array, String };

Kind kind(const json &value) noexcept {
    if (value.is_null()) return Kind::Null;
    if (value.is_boolean()) return Kind::Boolean;
    if (value.is_number()) return Kind::Number;
    if (value.is_object()) return Kind::Object;
    if (value.is_array()) return Kind::Array;
    return Kind::String;
}

std::partial_ordering compare_numbers(const json &left, const json &right) noexcept {
    if (left.holds<std::uint64_t>() && right.holds<std::uint64_t>()) {
        return left.get<std::uint64_t>() <=> right.get<std::uint64_t>();
    }
    if (left.holds<std::int64_t>() && right.holds<std::int64_t>()) {
        return left.get<std::int64_t>() <=> right.get<std::int64_t>();
    }
    if (left.holds<double>() || right.holds<double>()) {
        return left.as_number() <=> right.as_number();
    }
    // One unsigned and one signed integer: nlohmann compares the unsigned
    // value cast to the signed type, so values above INT64_MAX wrap (2^64-1
    // equals -1).  Reproduced, since the validator's enum and const results
    // depend on it.
    if (left.holds<std::int64_t>()) {
        return left.get<std::int64_t>() <=> static_cast<std::int64_t>(right.get<std::uint64_t>());
    }
    return static_cast<std::int64_t>(left.get<std::uint64_t>()) <=> right.get<std::int64_t>();
}

struct ReadOpts : glz::opts {
    bool validate_trailing_whitespace = true;
};

// nlohmann::json's serializer (dump(indent, ' ', ensure_ascii = false)):
// sorted members, ": " and newlines only when indenting, "{}"/"[]" for empty
// containers, \uXXXX (lower-case hex) for control characters without a short
// escape, and floating numbers in Grisu2 style ("5.0", "0.5", "1e+20").
void dump_string(std::string &out, std::string_view text) {
    static constexpr char hex[] = "0123456789abcdef";
    out += '"';
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (byte < 0x20) {
                    out += "\\u00";
                    out += hex[byte >> 4];
                    out += hex[byte & 0xF];
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

void dump_double(std::string &out, double value) {
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }
    if (std::signbit(value)) {
        out += '-';
        value = -value;
    }
    if (value == 0) {
        out += "0.0";
        return;
    }
    // Shortest round-trip digits d.ddd and exponent E (value = 0.ddd... * 10^(E+1)).
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific);
    const std::string_view scientific(buffer, static_cast<std::size_t>(result.ptr - buffer));
    const std::size_t e = scientific.find('e');
    std::string digits;
    for (const char c : scientific.substr(0, e)) {
        if (c != '.') digits += c;
    }
    int exponent = 0;
    std::from_chars(scientific.data() + e + (scientific[e + 1] == '+' ? 2 : 1), scientific.data() + scientific.size(), exponent);
    const int k = static_cast<int>(digits.size());
    const int n = exponent + 1; // the decimal point sits after n digits
    constexpr int min_exp = -4;
    constexpr int max_exp = std::numeric_limits<double>::digits10;
    if (k <= n && n <= max_exp) {
        out += digits;
        out.append(static_cast<std::size_t>(n - k), '0');
        out += ".0";
    } else if (0 < n && n <= max_exp) {
        out += digits.substr(0, static_cast<std::size_t>(n));
        out += '.';
        out += digits.substr(static_cast<std::size_t>(n));
    } else if (min_exp < n && n <= 0) {
        out += "0.";
        out.append(static_cast<std::size_t>(-n), '0');
        out += digits;
    } else {
        out += digits[0];
        if (k > 1) {
            out += '.';
            out += digits.substr(1);
        }
        out += 'e';
        int exp = n - 1;
        out += exp < 0 ? '-' : '+';
        exp = exp < 0 ? -exp : exp;
        if (exp < 10) out += '0';
        out += std::to_string(exp);
    }
}

template <class Integer>
void dump_integer(std::string &out, Integer value) {
    char buffer[24];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out.append(buffer, result.ptr);
}

void dump_value(std::string &out, const json &value, int indent, int level) {
    const bool pretty = indent >= 0;
    if (value.is_object()) {
        const auto &object = value.get_object();
        if (object.empty()) {
            out += "{}";
            return;
        }
        out += '{';
        if (pretty) out += '\n';
        bool first = true;
        for (const auto &[key, member] : object) {
            if (!first) out += pretty ? ",\n" : ",";
            first = false;
            if (pretty) out.append(static_cast<std::size_t>(indent * (level + 1)), ' ');
            dump_string(out, key);
            out += pretty ? ": " : ":";
            dump_value(out, member, indent, level + 1);
        }
        if (pretty) {
            out += '\n';
            out.append(static_cast<std::size_t>(indent * level), ' ');
        }
        out += '}';
    } else if (value.is_array()) {
        const auto &array = value.get_array();
        if (array.empty()) {
            out += "[]";
            return;
        }
        out += '[';
        if (pretty) out += '\n';
        bool first = true;
        for (const auto &element : array) {
            if (!first) out += pretty ? ",\n" : ",";
            first = false;
            if (pretty) out.append(static_cast<std::size_t>(indent * (level + 1)), ' ');
            dump_value(out, element, indent, level + 1);
        }
        if (pretty) {
            out += '\n';
            out.append(static_cast<std::size_t>(indent * level), ' ');
        }
        out += ']';
    } else if (value.is_string()) {
        dump_string(out, value.get_string());
    } else if (value.is_boolean()) {
        out += value.get_boolean() ? "true" : "false";
    } else if (value.holds<std::uint64_t>()) {
        dump_integer(out, value.get<std::uint64_t>());
    } else if (value.holds<std::int64_t>()) {
        dump_integer(out, value.get<std::int64_t>());
    } else if (value.holds<double>()) {
        dump_double(out, value.get<double>());
    } else {
        out += "null";
    }
}

} // namespace

json_pointer::json_pointer(std::string_view text) {
    if (text.empty()) return;
    if (text.front() != '/') {
        throw parse_error("JSON pointer must be empty or begin with '/' - was: '" + std::string(text) + "'");
    }
    std::size_t start = 1;
    while (true) {
        const std::size_t slash = text.find('/', start);
        tokens_.push_back(unescape_token(text.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start)));
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
}

std::string json_pointer::to_string() const {
    std::string text;
    for (const auto &token : tokens_) {
        text += '/';
        text += escape_token(token);
    }
    return text;
}

json_pointer &json_pointer::operator/=(const json_pointer &suffix) {
    tokens_.insert(tokens_.end(), suffix.tokens_.begin(), suffix.tokens_.end());
    return *this;
}

json_pointer &json_pointer::operator/=(std::string token) {
    tokens_.push_back(std::move(token));
    return *this;
}

json_pointer &json_pointer::operator/=(std::size_t index) { return *this /= std::to_string(index); }

json_pointer json_pointer::parent_pointer() const {
    json_pointer parent = *this;
    if (!parent.tokens_.empty()) parent.tokens_.pop_back();
    return parent;
}

void json_pointer::pop_back() {
    if (tokens_.empty()) throw out_of_range("JSON pointer has no parent");
    tokens_.pop_back();
}

const std::string &json_pointer::back() const {
    if (tokens_.empty()) throw out_of_range("JSON pointer has no parent");
    return tokens_.back();
}

const json *find(const json &root, const json_pointer &pointer) noexcept { return find_impl(root, pointer); }
json *find(json &root, const json_pointer &pointer) noexcept { return find_impl(root, pointer); }
const json &at(const json &root, const json_pointer &pointer) { return at_impl(root, pointer); }
json &at(json &root, const json_pointer &pointer) { return at_impl(root, pointer); }

json &get_or_create(json &root, const json_pointer &pointer) {
    json *node = &root;
    for (const auto &token : pointer.tokens()) {
        if (node->is_null()) {
            const bool numeric = !token.empty() && std::all_of(token.begin(), token.end(), [](char c) { return c >= '0' && c <= '9'; });
            if (numeric || token == "-") {
                *node = json::array_t{};
            } else {
                *node = json::object_t{};
            }
        }
        if (node->is_object()) {
            node = &(*node)[token];
        } else if (node->is_array()) {
            auto &array = node->get_array();
            const std::size_t index = token == "-" ? array.size() : array_index(token);
            if (index >= array.max_size()) throw out_of_range("array index " + token + " is out of range");
            if (index >= array.size()) array.resize(index + 1);
            node = &array[index];
        } else {
            throw out_of_range("unresolved reference token '" + token + "'");
        }
    }
    return *node;
}

json parse(std::string_view text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    // glaze reads a null-terminated buffer; a string_view (Document::create's
    // bounded data) need not be one, so parse an owned copy.
    const std::string buffer(text);
    json value{};
    glz::context context{};
    const auto error = glz::read<ReadOpts{}>(value, buffer, context);
    if (error) throw parse_error("parse error: " + glz::format_error(error, buffer));
    return value;
}

std::string dump(const json &value, int indent) {
    std::string out;
    dump_value(out, value, indent < 0 ? -1 : indent, 0);
    return out;
}

std::string_view type_name(const json &value) noexcept {
    switch (kind(value)) {
        case Kind::Null: return "null";
        case Kind::Boolean: return "boolean";
        case Kind::Number: return "number";
        case Kind::Object: return "object";
        case Kind::Array: return "array";
        case Kind::String: return "string";
    }
    return "null";
}

bool equal(const json &left, const json &right) noexcept { return compare(left, right) == std::partial_ordering::equivalent; }

std::partial_ordering compare(const json &left, const json &right) noexcept {
    const Kind left_kind = kind(left);
    const Kind right_kind = kind(right);
    if (left_kind != right_kind) return left_kind <=> right_kind;
    switch (left_kind) {
        case Kind::Null: return std::partial_ordering::equivalent;
        case Kind::Boolean: return left.get<bool>() <=> right.get<bool>();
        case Kind::Number: return compare_numbers(left, right);
        case Kind::String: return left.get<std::string>() <=> right.get<std::string>();
        case Kind::Array: {
            const auto &a = left.get_array();
            const auto &b = right.get_array();
            for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
                const auto order = compare(a[i], b[i]);
                if (order != std::partial_ordering::equivalent) return order;
            }
            return a.size() <=> b.size();
        }
        case Kind::Object: {
            const auto &a = left.get_object();
            const auto &b = right.get_object();
            auto a_it = a.begin();
            auto b_it = b.begin();
            for (; a_it != a.end() && b_it != b.end(); ++a_it, ++b_it) {
                if (const auto order = a_it->first <=> b_it->first; order != 0) return order;
                const auto order = compare(a_it->second, b_it->second);
                if (order != std::partial_ordering::equivalent) return order;
            }
            return a.size() <=> b.size();
        }
    }
    return std::partial_ordering::unordered;
}

} // namespace json_schema
