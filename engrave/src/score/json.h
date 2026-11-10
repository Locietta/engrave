// SPDX-License-Identifier: GPL-3.0-only
// The engine's JSON DOM and text serialization.
//
// glaze is used as the in-memory generic DOM and as the writer; this header
// only fixes the writer options the engine relies on (valid JSON string
// escaping and the indentation widths its callers pass to dump_json).
#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glaze/json/generic.hpp>
#include <glaze/json/read.hpp>
#include <glaze/json/write.hpp>

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

namespace json_detail {

// nlohmann rejected trailing content and skipped one leading UTF-8 BOM; glaze
// needs both behaviours opted in explicitly.
struct ReadOpts : glz::opts {
    bool validate_trailing_whitespace = true;
};

inline std::string_view without_bom(std::string_view text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    return text;
}

} // namespace json_detail

template <class J>
std::expected<J, glz::error_ctx> parse_json_expected(std::string_view text) {
    text = json_detail::without_bom(text);
    J value{};
    glz::context context{};
    const auto error = glz::read<json_detail::ReadOpts{}>(value, text, context);
    if (error) return std::unexpected(error);
    return value;
}

// Non-throwing parse: std::nullopt where json::parse(text, nullptr, false)
// returned a discarded value.
template <class J>
std::optional<J> parse_json(std::string_view text) {
    auto parsed = parse_json_expected<J>(text);
    if (!parsed) return std::nullopt;
    return std::optional<J>(std::move(*parsed));
}

// Deep equality; both DOMs must use the same ordering map.
template <template <class> class MapType>
inline constexpr bool insertion_ordered_map = std::same_as<MapType<int>, glz::ordered_small_map<int>>;

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
        if constexpr (insertion_ordered_map<MapType>) {
            // Insertion order is semantic for ordered_json: the harness evidence
            // compares the same members in the same positions.
            auto a_it = a.begin();
            auto b_it = b.begin();
            for (; a_it != a.end(); ++a_it, ++b_it) {
                if (a_it->first != b_it->first || !json_equal<MapType>(a_it->second, b_it->second)) return false;
            }
            return true;
        } else {
            for (const auto &[key, value] : a) {
                const auto found = b.find(key);
                if (found == b.end() || !json_equal<MapType>(value, found->second)) return false;
            }
            return true;
        }
    }
    return false;
}

namespace json_detail {

// Control characters have no two-byte escape, so glaze needs this opt-in to
// write them as \uXXXX instead of emitting an invalid JSON string.
struct CompactOpts : glz::opts {
    bool escape_control_characters = true;
};

template <uint8_t Width>
struct PrettyOpts : glz::opts {
    bool prettify = true;
    char indentation_char = ' ';
    uint8_t indentation_width = Width;
    bool escape_control_characters = true;
};

template <class J, class Opts>
std::string write_with(const J &value) {
    auto text = glz::write<Opts{}>(value);
    return text ? std::move(*text) : std::string{};
}

} // namespace json_detail

// Serialize the DOM; indent < 0 is the compact form, otherwise the number of
// spaces per level of the old dump(indent) argument.
template <template <class> class MapType>
std::string dump_json(const glz::generic_json<glz::num_mode::u64, MapType> &value, int indent = -1) {
    using J = glz::generic_json<glz::num_mode::u64, MapType>;
    if (indent < 0) return json_detail::write_with<J, json_detail::CompactOpts>(value);
    switch (indent) {
        case 0: return json_detail::write_with<J, json_detail::PrettyOpts<0>>(value);
        case 1: return json_detail::write_with<J, json_detail::PrettyOpts<1>>(value);
        case 2: return json_detail::write_with<J, json_detail::PrettyOpts<2>>(value);
        case 3: return json_detail::write_with<J, json_detail::PrettyOpts<3>>(value);
        case 4: return json_detail::write_with<J, json_detail::PrettyOpts<4>>(value);
        default: return json_detail::write_with<J, json_detail::PrettyOpts<2>>(value);
    }
}

} // namespace music
