// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <string_view>

namespace muse::textscan {

constexpr bool is_digit(char16_t c) { return c >= u'0' && c <= u'9'; }

constexpr bool is_upper(char16_t c) { return c >= u'A' && c <= u'Z'; }

constexpr bool is_lower(char16_t c) { return c >= u'a' && c <= u'z'; }

constexpr bool is_space(char16_t c) { return c == u' ' || (c >= u'\t' && c <= u'\r'); }

constexpr bool is_musicxml_space(char16_t c) { return is_space(c) || c == u'\u00A0'; }

constexpr bool is_line_break(char16_t c) { return c == u'\n' || c == u'\r' || c == u'\u2028' || c == u'\u2029'; }

constexpr bool is_word(char16_t c) { return is_digit(c) || is_upper(c) || is_lower(c) || c == u'_'; }

constexpr char16_t ascii_lower(char16_t c) { return is_upper(c) ? c + (u'a' - u'A') : c; }

constexpr bool starts_with_ignore_case(std::u16string_view text, std::u16string_view prefix) {
    return text.size() >= prefix.size() && std::ranges::equal(text.substr(0, prefix.size()), prefix, {}, ascii_lower, ascii_lower);
}

constexpr std::u16string_view trim_ascii_start(std::u16string_view text) {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    return text;
}

constexpr size_t digit_count(std::u16string_view text) { return std::ranges::find_if_not(text, is_digit) - text.begin(); }

} // namespace muse::textscan
