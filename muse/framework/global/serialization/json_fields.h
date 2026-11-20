/*
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <cmath>
#include <limits>
#include <string>
#include <string_view>

#include <glaze/json/generic.hpp>

#include "types/bytearray.h"

namespace muse::json {

inline std::string_view text(const ByteArray &bytes) { return {bytes.const_char(), bytes.size()}; }

// These formats accept partial settings. A missing member and a value of the
// wrong type usually share a fallback, although some callers need presence.
template <class J>
const J *find(const J &value, std::string_view key) {
    const auto *object = value.template get_if<typename J::object_t>();
    if (!object) return nullptr;
    const auto it = object->find(key);
    return it == object->end() ? nullptr : &it->second;
}

template <class J>
const J &member(const J &value, std::string_view key) {
    static const J missing{};
    const J *field = find(value, key);
    return field ? *field : missing;
}

template <class T, class J>
const T &get_or_empty(const J &value) {
    static const T empty{};
    const T *typed = value.template get_if<T>();
    return typed ? *typed : empty;
}

template <class J>
const auto &object(const J &value) {
    return get_or_empty<typename J::object_t>(value);
}

template <class J>
const auto &array(const J &value) {
    return get_or_empty<typename J::array_t>(value);
}

template <class J>
int rounded_int(const J &value) {
    const double rounded = std::round(get_or_empty<double>(value));
    if (rounded >= std::numeric_limits<int>::min() && rounded <= std::numeric_limits<int>::max()) {
        return static_cast<int>(rounded);
    }
    return std::numeric_limits<int>::min();
}

struct PrettyPrint : glz::opts {
    bool prettify = true;
    char indentation_char = ' ';
    uint8_t indentation_width = 2;
    bool escape_control_characters = true;
};

struct IgnoreUnknown : glz::opts {
    constexpr IgnoreUnknown() { error_on_unknown_keys = false; }

    bool validate_skipped = true;
};

inline ByteArray serialized_bytes(std::string text) {
    // Slash and DEL only occur inside JSON strings. Existing archives and
    // profiles escape both, and the pretty-printed document ends in a newline.
    if (auto special = text.find_first_of("/\x7f"); special != std::string::npos) {
        std::string escaped;
        size_t start = 0;
        do {
            escaped.append(text, start, special - start);
            escaped += text[special] == '/' ? "\\/" : "\\u007f";
            start = special + 1;
            special = text.find_first_of("/\x7f", start);
        } while (special != std::string::npos);
        escaped.append(text, start);
        text = std::move(escaped);
    }
    text += '\n';
    return ByteArray(text.data(), text.size());
}

} // namespace muse::json
