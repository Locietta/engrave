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
#ifndef MUSE_GLOBAL_TRANSLATABLESTRING_H
#define MUSE_GLOBAL_TRANSLATABLESTRING_H

#include <cstring>
#include <type_traits>

#include "translation.h"
#include "types/string.h"

namespace muse {
//! Note: in order to make the string visible for `lupdate`,
//! you must write TranslatableString(...) explicitly.
struct TranslatableString {
    const char *context = nullptr;
    String str;
    const char *disambiguation = nullptr;
    int default_n = -1;

    TranslatableString() = default;

    TranslatableString(const char *context, const char *str, const char *disambiguation = nullptr, int n = -1)
        : context(context), str(String::from_utf8(str)), disambiguation(disambiguation), default_n(n) {}

    TranslatableString(const char *context, const String &str, const char *disambiguation = nullptr, int n = -1)
        : context(context), str(str), disambiguation(disambiguation), default_n(n) {}

    static TranslatableString untranslatable(const char *str) { return TranslatableString(nullptr, str); }

    static TranslatableString untranslatable(const String &str) { return TranslatableString(nullptr, str); }

    inline bool is_empty() const { return str.empty() && !formatted_; }

    inline bool is_translatable() const { return context && context[0]; }

    inline String translated() const { return translated(default_n); }

    inline String translated(int n) const {
        return !formatted_ && !str.empty() && is_translatable() ? mtrc(context, str, disambiguation, n) : str;
    }

    template <typename... Args>
    TranslatableString arg(const Args &...args) const {
        if (is_empty()) {
            return *this;
        }

        const auto resolve = []<typename T>(const T &value) -> decltype(auto) {
            if constexpr (std::is_same_v<T, TranslatableString>) {
                return value.translated();
            } else {
                return value;
            }
        };

        TranslatableString result;
        result.context = context;
        result.disambiguation = disambiguation;
        result.default_n = default_n;
        result.str = translated().arg(resolve(args)...);
        result.formatted_ = true;
        return result;
    }

    bool operator==(const TranslatableString &other) const {
        return (context == other.context || (context && other.context && std::strcmp(context, other.context) == 0)) && str == other.str &&
               (disambiguation == other.disambiguation ||
                (disambiguation && other.disambiguation && std::strcmp(disambiguation, other.disambiguation) == 0)) &&
               default_n == other.default_n && formatted_ == other.formatted_;
    }

    bool operator!=(const TranslatableString &other) const { return !operator==(other); }

private:
    // Formatted text can be empty or contain user data that must bypass translation.
    bool formatted_ = false;
};
} // namespace muse

#endif // MUSE_GLOBAL_TRANSLATABLESTRING_H
