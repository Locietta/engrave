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
#ifndef MUSE_GLOBAL_VAL_H
#define MUSE_GLOBAL_VAL_H

#include <string>
#include <string_view>
#include <variant>
#include <type_traits>
#include <vector>
#include <map>
#include <optional>

#include "number.h"
#include "io/path.h"

namespace muse {
struct Val;
using ValList = std::vector<Val>;
using ValMap = std::map<std::string, Val>;

struct Val {
    enum class Type { UNDEFINED = 0, BOOL, INT, INT64, DOUBLE, STRING, LIST, MAP, COLOR };

    Val() = default;
    explicit Val(bool val);
    explicit Val(int val);
    explicit Val(int64_t val);
    explicit Val(double val);

    template <typename T>
    explicit Val(NumberT<T> val) : Val{val.raw()} {}

    explicit Val(const std::string &str);
    explicit Val(const char *str);
    explicit Val(const io::PathT &path);
    explicit Val(const ValList &list);
    explicit Val(const ValMap &map);

    template <class E, typename = std::enable_if_t<std::is_enum_v<E>>>
    explicit Val(E val) : Val{static_cast<std::underlying_type_t<E>>(val)} {}

    void set_type(Type t);
    Type type() const;

    bool is_null() const;
    bool to_bool() const;
    int to_int() const;
    int64_t to_int64() const;
    double to_double() const;
    float to_float() const;
    std::string to_string() const;
    io::PathT to_path() const;
    ValList to_list() const;
    ValMap to_map() const;

    template <class E, typename = std::enable_if_t<std::is_enum_v<E>>>
    E to_enum() const {
        return static_cast<E>(to_int());
    }

    template <typename T>
    std::optional<T> to_type() const {
        if constexpr (std::is_same<T, bool>::value) {
            return do_to_bool();
        } else if constexpr (std::is_same<T, int>::value) {
            return do_to_int();
        } else if constexpr (std::is_same<T, int64_t>::value) {
            return do_to_int64();
        } else if constexpr (std::is_same<T, double>::value) {
            return do_to_double();
        } else if constexpr (IsNumber<T>()) {
            auto value = to_type<typename T::value_type>();
            if (!value) {
                return std::nullopt;
            }
            return T(*value);
        } else if constexpr (std::is_same<T, std::string>::value) {
            return do_to_string();
        } else if constexpr (std::is_same<T, io::PathT>::value) {
            return do_to_path();
        } else if constexpr (std::is_same<T, ValList>::value) {
            return do_to_list();
        } else if constexpr (std::is_same<T, ValMap>::value) {
            return do_to_map();
        } else if constexpr (IsVector<T>::value) {
            auto list = do_to_list();
            if (!list) {
                return std::nullopt;
            }
            T result;
            for (const auto &item : *list) {
                auto value = item.to_type<typename T::value_type>();
                if (!value) {
                    return std::nullopt;
                }
                result.push_back(*value);
            }
            return result;
        } else if constexpr (IsStringMap<T>()) {
            auto map = do_to_map();
            if (!map) {
                return std::nullopt;
            }
            T result;
            for (const auto &item : *map) {
                auto value = item.second.to_type<typename T::mapped_type>();
                if (!value) {
                    return std::nullopt;
                }
                result[item.first] = *value;
            }
            return result;
        } else {
            static_assert(std::is_same<T, void>::value, "Unsupported type");
            return std::nullopt;
        }
    }

    template <typename T>
    static Val from_type(const T &value) {
        if constexpr (std::is_same<T, bool>::value) {
            return Val(value);
        } else if constexpr (std::is_same<T, int>::value) {
            return Val(value);
        } else if constexpr (std::is_same<T, int64_t>::value) {
            return Val(value);
        } else if constexpr (std::is_same<T, double>::value) {
            return Val(value);
        } else if constexpr (IsNumber<T>()) {
            return Val(value);
        } else if constexpr (std::is_same<T, std::string>::value) {
            return Val(value);
        } else if constexpr (std::is_same<T, io::PathT>::value) {
            return Val(value);
        } else if constexpr (std::is_same<T, ValList>::value) {
            return Val(value);
        } else if constexpr (std::is_same<T, ValMap>::value) {
            return Val(value);
        } else if constexpr (IsVector<T>::value) {
            ValList list;
            for (const auto &item : value) {
                list.push_back(from_type(item));
            }
            return Val(list);
        } else if constexpr (IsStringMap<T>()) {
            ValMap map;
            for (const auto &item : value) {
                map[item.first] = from_type(item.second);
            }
            return Val(map);
        } else {
            static_assert(std::is_same<T, void>::value, "Unsupported type");
            return Val();
        }
    }

    bool operator==(const Val &v) const;
    bool operator<(const Val &v) const;


private:

    template <typename T>
    struct IsNumber : std::false_type {};

    template <typename T>
    struct IsNumber<NumberT<T>> : std::true_type {};

    template <typename T>
    struct IsVector : std::false_type {};

    template <typename U, typename A>
    struct IsVector<std::vector<U, A>> : std::true_type {};

    template <typename T>
    struct IsStringMap : std::false_type {};

    template <typename U, typename C, typename A>
    struct IsStringMap<std::map<std::string, U, C, A>> : std::true_type {};

    Type value_type() const;

    std::optional<bool> do_to_bool() const;
    std::optional<int> do_to_int() const;
    std::optional<int64_t> do_to_int64() const;
    std::optional<double> do_to_double() const;
    std::optional<std::string> do_to_string() const;
    std::optional<io::PathT> do_to_path() const;
    std::optional<ValList> do_to_list() const;
    std::optional<ValMap> do_to_map() const;

    //! NOTE Should be sync with valueType method
    using Value = std::variant<std::monostate, bool, int, int64_t, double, std::string, ValList, ValMap>;
    Value val_;
    Type type_ = Type::UNDEFINED;
};

//! NOTE Helper class for the serialization/deserialization of lists and maps
struct ValJson {
    static bool to_string(const ValList &list, std::string &str);
    static bool to_string(const ValMap &map, std::string &str);
    static bool from_string(const std::string &str, ValList &list);
    static bool from_string(const std::string &str, ValMap &map);

private:
    static std::string_view trim(std::string_view str);
    static bool is_integer_token(std::string_view str);
    static bool is_double_token(std::string_view str);
    static bool split_top_level(std::string_view str, std::vector<std::string_view> &out);
    static size_t top_level_colon(std::string_view str);
    static std::optional<Val> parse_value(std::string_view token);
    static bool parse_list(std::string_view str, ValList &list);
    static bool parse_map(std::string_view str, ValMap &map);
};
} // namespace muse

#endif // MUSE_GLOBAL_VAL_H
