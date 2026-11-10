// SPDX-License-Identifier: GPL-3.0-only
// Hand-written types and glaze helpers used by the generated MNX model (mnx_model.h).
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <glaze/glaze.hpp>

namespace mu::iex::mnxio::model {

// An MNX fraction, written as [numerator, denominator].
struct Fraction {
    int numerator = 0;
    int denominator = 1;
    bool operator==(const Fraction &) const = default;
};

// String types the schema constrains with a pattern; reading rejects other values.
struct Id : std::string {
    using std::string::string;

    Id(std::string value) : std::string(std::move(value)) {}
};

struct SimpleColor : std::string {
    using std::string::string;

    SimpleColor(std::string value) : std::string(std::move(value)) {}
};

// ^[\x21-\x7E]{1,256}$
constexpr bool is_valid_id(std::string_view text) {
    if (text.empty() || text.size() > 256) return false;
    for (const char c : text)
        if (c < 0x21 || c > 0x7E) return false;
    return true;
}

// ^#[0-9a-f]{6}$
constexpr bool is_valid_simple_color(std::string_view text) {
    if (text.size() != 7 || text[0] != '#') return false;
    for (const char c : text.substr(1))
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

namespace detail {

// Options for reading a document: unknown and missing keys are errors, skipped values
// (comments and vendor extensions) must still be valid JSON, and nothing may follow the root.
struct ReadOpts : glz::opts {
    bool validate_skipped = true;
    bool validate_trailing_whitespace = true;
};

inline constexpr ReadOpts read_opts{{.error_on_unknown_keys = true, .error_on_missing_keys = true}};
// The same, for a slice of the input that is not null-terminated.
inline constexpr ReadOpts slice_opts{{.null_terminated = false, .error_on_unknown_keys = true, .error_on_missing_keys = true}};
inline constexpr glz::opts peek_opts{.null_terminated = false, .error_on_unknown_keys = false};

struct TypeKey {
    std::optional<std::string_view> type;
};

template <class Variant, std::size_t... I>
void emplace_index(Variant &value, std::size_t index, std::index_sequence<I...>) {
    std::ignore = ((index == I ? (value.template emplace<I>(), true) : false) || ...);
}

inline void fail(auto &ctx, auto &it, const char *at, glz::error_code code, std::string_view message) {
    ctx.error = code;
    ctx.custom_error_message = message;
    it = at;
}

// Reads one element of an anyOf list. The alternative is chosen by the element's "type" key
// (`tags[i]` selects alternative i); an element without one is alternative `untagged`, or an
// error when there is none. The element is read again into its alternative, and an error
// there is reported at its position in the whole input.
template <auto Opts, class Variant>
void read_tagged(Variant &value, auto &ctx, auto &it, auto end, std::span<const std::string_view> tags,
                 std::optional<std::size_t> untagged) {
    if (glz::skip_ws<Opts>(ctx, it, end)) return;
    if (*it != '{') {
        ctx.error = glz::error_code::expected_brace;
        return;
    }
    glz::raw_json_view raw{};
    glz::parse<glz::JSON>::op<Opts>(raw, ctx, it, end);
    if (bool(ctx.error)) return;
    const char *begin = raw.str.data();

    TypeKey key{};
    if (const auto peeked = glz::read<peek_opts>(key, raw.str)) {
        fail(ctx, it, begin + peeked.count, peeked.ec, peeked.custom_error_message);
        return;
    }
    std::size_t index = tags.size();
    if (key.type) {
        for (std::size_t i = 0; i < tags.size(); ++i)
            if (tags[i] == *key.type) index = i;
        if (index == tags.size()) {
            fail(ctx, it, begin, glz::error_code::constraint_violated, "unknown \"type\" for this list");
            return;
        }
    } else if (untagged) {
        index = *untagged;
    } else {
        fail(ctx, it, begin, glz::error_code::missing_key, "the element has no \"type\"");
        return;
    }
    if (value.index() != index) emplace_index(value, index, std::make_index_sequence<std::variant_size_v<Variant>>{});
    std::visit(
        [&](auto &alternative) {
            if (const auto ec = glz::read<slice_opts>(alternative, raw.str))
                fail(ctx, it, begin + ec.count, ec.ec, ec.custom_error_message);
        },
        value);
}

// Reads a string and rejects it unless `valid` accepts it.
template <auto Opts, auto Valid>
void read_checked_string(std::string &value, auto &ctx, auto &it, auto end, std::string_view message) {
    if (glz::skip_ws<Opts>(ctx, it, end)) return;
    const char *begin = it;
    glz::parse<glz::JSON>::op<Opts>(value, ctx, it, end);
    if (!bool(ctx.error) && !Valid(value)) fail(ctx, it, begin, glz::error_code::constraint_violated, message);
}

// Reads an enum written as one of a fixed set of JSON values (`Value` is int or string_view).
template <auto Opts, class Enum, class Value, std::size_t N>
void read_enum(Enum &value, auto &ctx, auto &it, auto end, const std::array<std::pair<Value, Enum>, N> &table, std::string_view message) {
    if (glz::skip_ws<Opts>(ctx, it, end)) return;
    const char *begin = it;
    std::conditional_t<std::is_same_v<Value, int>, int, std::string> raw{};
    glz::parse<glz::JSON>::op<Opts>(raw, ctx, it, end);
    if (bool(ctx.error)) return;
    for (const auto &[json, e] : table) {
        if (json == raw) {
            value = e;
            return;
        }
    }
    fail(ctx, it, begin, glz::error_code::constraint_violated, message);
}

template <class Enum, class Value, std::size_t N>
constexpr Value enum_value(Enum value, const std::array<std::pair<Value, Enum>, N> &table) {
    for (const auto &[json, e] : table)
        if (e == value) return json;
    return table[0].first;
}

} // namespace detail
} // namespace mu::iex::mnxio::model

template <>
struct glz::meta<mu::iex::mnxio::model::detail::TypeKey> {
    using T = mu::iex::mnxio::model::detail::TypeKey;
    static constexpr auto value = glz::object("type", &T::type);
};

template <>
struct glz::meta<mu::iex::mnxio::model::Fraction> {
    using T = mu::iex::mnxio::model::Fraction;
    static constexpr auto value = glz::array(&T::numerator, &T::denominator);
};

template <>
struct glz::from<glz::JSON, mu::iex::mnxio::model::Id> {
    template <auto Opts>
    static void op(auto &value, is_context auto &&ctx, auto &&it, auto end) {
        mu::iex::mnxio::model::detail::read_checked_string<Opts, mu::iex::mnxio::model::is_valid_id>(
            value, ctx, it, end, "an id must be 1 to 256 printable ASCII characters without spaces");
    }
};

template <>
struct glz::to<glz::JSON, mu::iex::mnxio::model::Id> {
    template <auto Opts>
    static void op(auto &value, is_context auto &&ctx, auto &&...args) {
        serialize<JSON>::op<Opts>(static_cast<const std::string &>(value), ctx, args...);
    }
};

template <>
struct glz::from<glz::JSON, mu::iex::mnxio::model::SimpleColor> {
    template <auto Opts>
    static void op(auto &value, is_context auto &&ctx, auto &&it, auto end) {
        mu::iex::mnxio::model::detail::read_checked_string<Opts, mu::iex::mnxio::model::is_valid_simple_color>(
            value, ctx, it, end, "a color must be #rrggbb in lowercase hex");
    }
};

template <>
struct glz::to<glz::JSON, mu::iex::mnxio::model::SimpleColor> {
    template <auto Opts>
    static void op(auto &value, is_context auto &&ctx, auto &&...args) {
        serialize<JSON>::op<Opts>(static_cast<const std::string &>(value), ctx, args...);
    }
};
