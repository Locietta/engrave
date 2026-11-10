// SPDX-License-Identifier: GPL-3.0-only
// The engine's JSON DOM and text serialization.
//
// glaze is used as the in-memory generic DOM; the text writer lives here so the
// engine's own output stays byte-identical to the captures in tests/data/golden
// (object ordering, double formatting and string escaping included).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/json/generic.hpp>
#include <glaze/json/read.hpp>

#include "engrave/src/score/json_number.h"

namespace music {

// Insertion-ordered DOM: the semantic dump and the harness evidence keep the
// order in which their fields are built.
using Json = glz::generic_u64;
// Lexicographically ordered DOM: the legacy serializer's default json
// (metadata, positions).
using SortedJson = glz::generic_sorted_u64;

template <class J>
J make_array() {
    return J(typename J::array_t{});
}

template <class J>
J make_object() {
    return J(typename J::object_t{});
}

template <class J, class... V>
J json_array_of(V &&...values) {
    J array = make_array<J>();
    (array.get_array().emplace_back(J(std::forward<V>(values))), ...);
    return array;
}

template <class J, class V>
void array_push(J &array, V &&value) {
    array.get_array().emplace_back(std::forward<V>(value));
}

template <class J>
void erase_key(J &object, std::string_view key) {
    object.get_object().erase(key);
}

// Values of an object, with a default for a missing key (the old
// json::value(key, default) contract).
template <class T, class J>
T json_value(const J &object, std::string_view key, T fallback) {
    if (!object.template holds<typename J::object_t>()) return fallback;
    const auto &values = object.get_object();
    const auto found = values.find(key);
    if (found == values.end()) return fallback;
    return found->second.template as<T>();
}

// Object merge in source order: existing keys keep their position, new keys
// are appended (json::update semantics).
template <class J>
void merge_object(J &target, const J &source) {
    for (const auto &[key, value] : source.get_object()) target[key] = value;
}

// Non-throwing parse: std::nullopt where json::parse(text, nullptr, false)
// returned a discarded value.
template <class J>
std::optional<J> parse_json(std::string_view text) {
    auto parsed = glz::read_json<J>(text);
    if (!parsed) return std::nullopt;
    return std::optional<J>(std::move(*parsed));
}

// Deep equality; the ordering map of the two DOMs must be the same.
template <template <class> class MapType>
bool json_equal(const glz::generic_json<glz::num_mode::u64, MapType> &left, const glz::generic_json<glz::num_mode::u64, MapType> &right) {
    using J = glz::generic_json<glz::num_mode::u64, MapType>;
    if (left.is_number() && right.is_number()) {
        if (left.template holds<uint64_t>() && right.template holds<uint64_t>()) {
            return left.template get<uint64_t>() == right.template get<uint64_t>();
        }
        if (left.template holds<int64_t>() && right.template holds<int64_t>()) {
            return left.template get<int64_t>() == right.template get<int64_t>();
        }
        if (left.template holds<double>() || right.template holds<double>()) return left.as_number() == right.as_number();
        // One unsigned, one signed value.
        const auto unsigned_value = left.template holds<uint64_t>() ? left.template get<uint64_t>() : right.template get<uint64_t>();
        const auto signed_value = left.template holds<int64_t>() ? left.template get<int64_t>() : right.template get<int64_t>();
        return signed_value >= 0 && unsigned_value == static_cast<uint64_t>(signed_value);
    }
    if (left.template holds<std::string>() && right.template holds<std::string>()) {
        return left.template get<std::string>() == right.template get<std::string>();
    }
    if (left.template holds<bool>() && right.template holds<bool>()) {
        return left.template get<bool>() == right.template get<bool>();
    }
    if (left.template holds<std::nullptr_t>() && right.template holds<std::nullptr_t>()) return true;
    if (left.is_array() && right.is_array()) {
        const auto &a = left.get_array();
        const auto &b = right.get_array();
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (!json_equal<MapType>(a[i], b[i])) return false;
        return true;
    }
    if (left.is_object() && right.is_object()) {
        const auto &a = left.get_object();
        const auto &b = right.get_object();
        if (a.size() != b.size()) return false;
        for (const auto &[key, value] : a) {
            const auto found = b.find(key);
            if (found == b.end() || !json_equal<MapType>(value, found->second)) return false;
        }
        return true;
    }
    return false;
}

namespace json_detail {

inline void append_string(std::string &out, std::string_view text) {
    static constexpr char hex[] = "0123456789abcdef";
    out.push_back('"');
    for (const unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\f': out += "\\f"; break;
            case '\r': out += "\\r"; break;
            default:
                if (c <= 0x1f) {
                    out += "\\u00";
                    out.push_back(hex[c >> 4]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}

inline void append_indent(std::string &out, int indent, int depth) {
    out.push_back('\n');
    out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(depth), ' ');
}

template <template <class> class MapType>
void append_value(std::string &out, const glz::generic_json<glz::num_mode::u64, MapType> &value, int indent, int depth) {
    if (value.template holds<std::nullptr_t>()) {
        out += "null";
        return;
    }
    if (value.template holds<bool>()) {
        out += value.template get<bool>() ? "true" : "false";
        return;
    }
    if (value.template holds<uint64_t>()) {
        out += std::to_string(value.template get<uint64_t>());
        return;
    }
    if (value.template holds<int64_t>()) {
        out += std::to_string(value.template get<int64_t>());
        return;
    }
    if (value.template holds<double>()) {
        out += json_number::format_double(value.template get<double>());
        return;
    }
    if (value.template holds<std::string>()) {
        append_string(out, value.template get<std::string>());
        return;
    }
    const bool pretty = indent >= 0;
    if (value.is_array()) {
        const auto &array = value.get_array();
        out.push_back('[');
        for (std::size_t i = 0; i < array.size(); ++i) {
            if (i) out.push_back(',');
            if (pretty) append_indent(out, indent, depth + 1);
            append_value(out, array[i], indent, depth + 1);
        }
        if (pretty && !array.empty()) append_indent(out, indent, depth);
        out.push_back(']');
        return;
    }
    if (value.is_object()) {
        const auto &object = value.get_object();
        out.push_back('{');
        bool first = true;
        for (const auto &[key, item] : object) {
            if (!first) out.push_back(',');
            first = false;
            if (pretty) append_indent(out, indent, depth + 1);
            append_string(out, key);
            out.push_back(':');
            if (pretty) out.push_back(' ');
            append_value(out, item, indent, depth + 1);
        }
        if (pretty && !object.empty()) append_indent(out, indent, depth);
        out.push_back('}');
        return;
    }
    out += "null";
}

} // namespace json_detail

// Serialize the DOM; indent < 0 is the compact form, 0..n the indented form
// (json::dump with the negative/zero-based indent argument).
template <template <class> class MapType>
std::string dump_json(const glz::generic_json<glz::num_mode::u64, MapType> &value, int indent = -1) {
    std::string out;
    json_detail::append_value(out, value, indent, 0);
    return out;
}

} // namespace music
