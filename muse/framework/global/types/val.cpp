/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "val.h"

#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

#include "log.h"

using namespace muse;

static const std::string val_true("true");
static const std::string val_false("false");

Val::Val(bool val) : val_(val), type_(Type::BOOL) {}

Val::Val(int val) : val_(val), type_(Type::INT) {}

Val::Val(int64_t val) : val_(val), type_(Type::INT64) {}

Val::Val(double val) : val_(val), type_(Type::DOUBLE) {}

Val::Val(const std::string &str) : val_(str), type_(Type::STRING) {}

Val::Val(const char *str) : val_(std::string(str)), type_(Type::STRING) {}

Val::Val(const io::PathT &path) : val_(path.to_std_string()), type_(Type::STRING) {}

Val::Val(const ValList &list) : val_(list), type_(Type::LIST) {}

Val::Val(const ValMap &map) : val_(map), type_(Type::MAP) {}

Val::Type Val::value_type() const {
    // std::monostate, bool, int, int64_t, double, std::string, ValList, ValMap
    switch (val_.index()) {
        case 0: return Type::UNDEFINED;
        case 1: return Type::BOOL;
        case 2: return Type::INT;
        case 3: return Type::INT64;
        case 4: return Type::DOUBLE;
        case 5: return Type::STRING;
        case 6: return Type::LIST;
        case 7: return Type::MAP;
    }
    return Type::UNDEFINED;
}

void Val::set_type(Type t) { type_ = t; }

Val::Type Val::type() const { return type_; }

bool Val::is_null() const { return val_.index() == 0; }

std::optional<bool> Val::do_to_bool() const {
    switch (value_type()) {
        case Type::BOOL: return std::get<bool>(val_);
        case Type::INT: return muse::is_zero(std::get<int>(val_)) ? false : true;
        case Type::INT64: return muse::is_zero(std::get<int64_t>(val_)) ? false : true;
        case Type::DOUBLE: return muse::is_zero(std::get<double>(val_)) ? false : true;
        case Type::STRING: {
            std::string str = std::get<std::string>(val_);
            return !str.empty() && str != ::val_false && str != "0" && str != "0.0";
        }
        case Type::LIST: return std::get<ValList>(val_).size() > 0;
        case Type::MAP: return std::get<ValMap>(val_).size() > 0;
        default: break;
    }
    return std::nullopt;
}

bool Val::to_bool() const {
    auto value = do_to_bool();
    return value ? *value : false;
}

std::optional<int> Val::do_to_int() const {
    switch (value_type()) {
        case Type::BOOL: return std::get<bool>(val_) ? 1 : 0;
        case Type::INT: return std::get<int>(val_);
        case Type::INT64: return static_cast<int>(std::get<int64_t>(val_));
        case Type::DOUBLE: return static_cast<int>(std::get<double>(val_));
        case Type::STRING: {
            std::string str = std::get<std::string>(val_);
            if (str == ::val_true) {
                return 1;
            }
            try {
                return std::stoi(str);
            } catch (const std::exception &e) {
                return std::nullopt;
            }
        }
        default: break;
    }
    return std::nullopt;
}

int Val::to_int() const {
    auto value = do_to_int();
    return value ? *value : 0;
}

std::optional<int64_t> Val::do_to_int64() const {
    switch (value_type()) {
        case Type::BOOL: return std::get<bool>(val_) ? 1 : 0;
        case Type::INT: return std::get<int>(val_);
        case Type::INT64: return std::get<int64_t>(val_);
        case Type::DOUBLE: return static_cast<int64_t>(std::get<double>(val_));
        case Type::STRING: {
            std::string str = std::get<std::string>(val_);
            if (str == ::val_true) {
                return 1LL;
            }
            try {
                return std::stoll(str);
            } catch (const std::exception &e) {
                return std::nullopt;
            }
        }
        default: break;
    }
    return std::nullopt;
}

int64_t Val::to_int64() const {
    auto value = do_to_int64();
    return value ? *value : 0;
}

std::optional<double> Val::do_to_double() const {
    switch (value_type()) {
        case Type::BOOL: return std::get<bool>(val_) ? 1.0 : 0.0;
        case Type::INT: return static_cast<double>(std::get<int>(val_));
        case Type::INT64: return static_cast<double>(std::get<int64_t>(val_));
        case Type::DOUBLE: return std::get<double>(val_);
        case Type::STRING: {
            std::string str = std::get<std::string>(val_);
            if (str == ::val_true) {
                return 1.0;
            }
            try {
                return std::stod(str);
            } catch (const std::exception &e) {
                return std::nullopt;
            }
        }
        default: break;
    }
    return std::nullopt;
}

double Val::to_double() const {
    auto value = do_to_double();
    return value ? *value : 0.0;
}

float Val::to_float() const { return static_cast<float>(to_double()); }

static std::string double_to_string(double n, int prec = 6) {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(prec) << n;
    std::string s = stream.str();

    // remove extra '0'
    size_t corrected_idx = s.size() - 1;
    if (s.back() == '0') {
        for (int i = static_cast<int>(s.size() - 1); i > 0; --i) {
            if (s.at(i) != '0') {
                corrected_idx = i;
                break;
            }
        }
    }

    if (s.at(corrected_idx) == '.') {
        --corrected_idx;
    }

    return std::string(s.c_str(), corrected_idx + 1);
}

std::optional<std::string> Val::do_to_string() const {
    switch (value_type()) {
        case Type::BOOL: return to_bool() ? ::val_true : ::val_false;
        case Type::INT: return std::to_string(std::get<int>(val_));
        case Type::INT64: return std::to_string(std::get<int64_t>(val_));
        case Type::DOUBLE: return double_to_string(std::get<double>(val_));
        case Type::STRING: return std::get<std::string>(val_);
        case Type::LIST: {
            std::string str;
            if (!ValJson::to_string(std::get<ValList>(val_), str)) {
                return std::nullopt;
            }
            return str;
        }
        case Type::MAP: {
            std::string str;
            if (!ValJson::to_string(std::get<ValMap>(val_), str)) {
                return std::nullopt;
            }
            return str;
        }
        default: break;
    }
    return std::nullopt;
}

std::string Val::to_string() const {
    auto value = do_to_string();
    return value ? *value : std::string();
}

std::optional<io::PathT> Val::do_to_path() const {
    if (value_type() == Type::STRING) {
        return std::get<std::string>(val_);
    }
    return std::nullopt;
}

io::PathT Val::to_path() const {
    auto value = do_to_path();
    return value ? *value : io::PathT();
}

std::optional<ValList> Val::do_to_list() const {
    switch (value_type()) {
        case Type::LIST: return std::get<ValList>(val_);
        case Type::STRING: {
            ValList list;
            if (!ValJson::from_string(std::get<std::string>(val_), list)) {
                return std::nullopt;
            }
            return list;
        }
        default: break;
    }

    return std::nullopt;
}

ValList Val::to_list() const {
    auto value = do_to_list();
    return value ? *value : ValList();
}

std::optional<ValMap> Val::do_to_map() const {
    switch (value_type()) {
        case Type::MAP: return std::get<ValMap>(val_);
        case Type::STRING: {
            ValMap map;
            if (!ValJson::from_string(std::get<std::string>(val_), map)) {
                return std::nullopt;
            }
            return map;
        }
        default: break;
    }
    return std::nullopt;
}

ValMap Val::to_map() const {
    auto value = do_to_map();
    return value ? *value : ValMap();
}

bool Val::operator==(const Val &v) const { return v.type_ == type_ && v.val_ == val_; }

bool Val::operator<(const Val &v) const {
    if (val_.index() != v.val_.index()) {
        return false;
    }

    return val_ < v.val_;
}

static void append_quoted(std::ostream &out, std::string_view text) {
    static const char *hex = "0123456789abcdef";
    out << '"';
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
                } else {
                    out << static_cast<char>(c);
                }
                break;
        }
    }
    out << '"';
}

static bool append_value(std::ostream &out, const Val &value) {
    const std::optional<std::string> text = value.to_type<std::string>();
    if (!text) {
        return false;
    }
    if (value.type() == Val::Type::STRING) {
        append_quoted(out, *text);
        return true;
    }
    out << *text;
    return true;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static std::optional<std::string> unquote(std::string_view token) {
    if (token.size() < 2 || token.front() != '"') {
        return std::nullopt;
    }

    std::string result;
    bool escaped = false;
    for (size_t i = 1; i < token.size(); ++i) {
        const char c = token[i];
        if (escaped) {
            switch (c) {
                case '"':
                case '\\':
                case '/': result.push_back(c); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    if (i + 4 >= token.size()) {
                        return std::nullopt;
                    }
                    int code = 0;
                    for (int h = 0; h < 4; ++h) {
                        const int digit = hex_digit(token[i + 1 + h]);
                        if (digit < 0) {
                            return std::nullopt;
                        }
                        code = (code << 4) | digit;
                    }
                    if (code >= 0xd800 && code <= 0xdfff) {
                        return std::nullopt;
                    }
                    if (code < 0x80) {
                        result.push_back(static_cast<char>(code));
                    } else if (code < 0x800) {
                        result.push_back(static_cast<char>(0xc0 | (code >> 6)));
                        result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                    } else {
                        result.push_back(static_cast<char>(0xe0 | (code >> 12)));
                        result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                        result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                    }
                    i += 4;
                    break;
                }
                default: return std::nullopt;
            }
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            if (i != token.size() - 1) {
                return std::nullopt;
            }
            return result;
        }
        result.push_back(c);
    }
    return std::nullopt;
}

//! NOTE Quoted text is not structural: commas, colons and brackets inside it are data.
static bool skip_quoted(bool &in_string, bool &escaped, char c) {
    if (in_string) {
        if (escaped) {
            escaped = false;
            return true;
        }
        if (c == '\\') {
            escaped = true;
            return true;
        }
        if (c == '"') {
            in_string = false;
        }
        return true;
    }
    if (c == '"') {
        in_string = true;
        return true;
    }
    return false;
}

bool ValJson::to_string(const ValList &list, std::string &str) {
    std::stringstream stream;
    stream << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        if (!append_value(stream, list.at(i))) {
            return false;
        }
        if (i + 1 < list.size()) {
            stream << ", ";
        }
    }
    stream << "]";
    str = stream.str();
    return true;
}

bool ValJson::to_string(const ValMap &map, std::string &str) {
    std::stringstream stream;
    stream << "{";
    std::string sep;
    for (const auto &item : map) {
        stream << sep;
        append_quoted(stream, item.first);
        stream << ": ";
        if (!append_value(stream, item.second)) {
            return false;
        }
        sep = ", ";
    }
    stream << "}";
    str = stream.str();
    return true;
}

bool ValJson::from_string(const std::string &str, ValList &list) { return parse_list(str, list); }

bool ValJson::from_string(const std::string &str, ValMap &map) { return parse_map(str, map); }

std::string_view ValJson::trim(std::string_view str) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }
    return str;
}

bool ValJson::is_integer_token(std::string_view str) {
    if (str.empty()) {
        return false;
    }
    size_t i = (str.front() == '-' || str.front() == '+') ? 1 : 0;
    if (i == str.size()) {
        return false;
    }
    for (; i < str.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
            return false;
        }
    }
    return true;
}

bool ValJson::is_double_token(std::string_view str) {
    if (str.empty()) {
        return false;
    }
    size_t i = (str.front() == '-' || str.front() == '+') ? 1 : 0;
    bool has_dot = false;
    bool has_digit = false;
    for (; i < str.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(str[i]);
        if (std::isdigit(c)) {
            has_digit = true;
        } else if (str[i] == '.' && !has_dot) {
            has_dot = true;
        } else {
            return false;
        }
    }
    return has_dot && has_digit;
}

bool ValJson::split_top_level(std::string_view str, std::vector<std::string_view> &out) {
    out.clear();
    if (str.empty()) {
        return true;
    }

    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    size_t start = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        const char c = str[i];
        if (skip_quoted(in_string, escaped, c)) {
            continue;
        }
        if (c == '[' || c == '{') {
            ++depth;
        } else if (c == ']' || c == '}') {
            if (depth == 0) {
                return false;
            }
            --depth;
        } else if (c == ',' && depth == 0) {
            out.push_back(str.substr(start, i - start));
            start = i + 1;
        }
    }

    if (in_string || escaped || depth != 0) {
        return false;
    }

    out.push_back(str.substr(start));
    return true;
}

size_t ValJson::top_level_colon(std::string_view str) {
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (size_t i = 0; i < str.size(); ++i) {
        const char c = str[i];
        if (skip_quoted(in_string, escaped, c)) {
            continue;
        }
        if (c == '[' || c == '{') {
            ++depth;
        } else if (c == ']' || c == '}') {
            if (depth == 0) {
                return std::string_view::npos;
            }
            --depth;
        } else if (c == ':' && depth == 0) {
            return i;
        }
    }
    return std::string_view::npos;
}

std::optional<Val> ValJson::parse_value(std::string_view token) {
    token = trim(token);
    if (token.empty()) {
        return std::nullopt;
    }

    if (token.front() == '"') {
        auto text = unquote(token);
        if (!text) {
            return std::nullopt;
        }
        return Val(*text);
    }

    if (token.front() == '[') {
        ValList list;
        if (!parse_list(token, list)) {
            return std::nullopt;
        }
        return Val(list);
    }

    if (token.front() == '{') {
        ValMap map;
        if (!parse_map(token, map)) {
            return std::nullopt;
        }
        return Val(map);
    }

    if (token == ::val_true) {
        return Val(true);
    }
    if (token == ::val_false) {
        return Val(false);
    }

    const std::string text(token);
    if (is_integer_token(token)) {
        try {
            size_t consumed = 0;
            const long long value = std::stoll(text, &consumed);
            if (consumed != text.size()) {
                return std::nullopt;
            }
            if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) {
                return Val(static_cast<int>(value));
            }
            return Val(static_cast<int64_t>(value));
        } catch (const std::exception &) {
            return Val(text);
        }
    }

    if (is_double_token(token)) {
        try {
            size_t consumed = 0;
            const double value = std::stod(text, &consumed);
            if (consumed != text.size()) {
                return std::nullopt;
            }
            return Val(value);
        } catch (const std::exception &) {
            return Val(text);
        }
    }

    return Val(text);
}

bool ValJson::parse_list(std::string_view str, ValList &list) {
    str = trim(str);
    if (str.size() < 2 || str.front() != '[' || str.back() != ']') {
        return false;
    }

    std::vector<std::string_view> parts;
    if (!split_top_level(trim(str.substr(1, str.size() - 2)), parts)) {
        return false;
    }

    ValList result;
    result.reserve(parts.size());
    for (const std::string_view part : parts) {
        auto value = parse_value(part);
        if (!value) {
            return false;
        }
        result.push_back(*value);
    }

    list = std::move(result);
    return true;
}

bool ValJson::parse_map(std::string_view str, ValMap &map) {
    str = trim(str);
    if (str.size() < 2 || str.front() != '{' || str.back() != '}') {
        return false;
    }

    std::vector<std::string_view> parts;
    if (!split_top_level(trim(str.substr(1, str.size() - 2)), parts)) {
        return false;
    }

    ValMap result;
    for (const std::string_view part : parts) {
        const std::string_view entry = trim(part);
        const size_t colon = top_level_colon(entry);
        if (colon == std::string_view::npos) {
            return false;
        }

        const std::string_view raw_key = trim(entry.substr(0, colon));
        if (raw_key.empty()) {
            return false;
        }
        std::string key;
        if (raw_key.front() == '"') {
            auto parsed_key = unquote(raw_key);
            if (!parsed_key) {
                return false;
            }
            key = std::move(*parsed_key);
        } else {
            key = std::string(raw_key);
        }

        auto value = parse_value(entry.substr(colon + 1));
        if (!value) {
            return false;
        }
        result[std::move(key)] = *value;
    }

    map = std::move(result);
    return true;
}
