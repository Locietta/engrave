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
#include "translation.h"

#include "log.h"
#include "types/bytearray.h"
#include "types/string.h"

using namespace muse;

static String translate([[maybe_unused]] const char *context, const char *key, [[maybe_unused]] const char *disambiguation,
                        [[maybe_unused]] int n) {
    return String::from_utf8(key);
}

std::string muse::trc(const char *context, const char *key, const char *disambiguation, int n) {
    return translate(context, key, disambiguation, n).to_std_string();
}

String muse::mtrc(const char *context, const char *key, const char *disambiguation, int n) {
    return translate(context, key, disambiguation, n);
}

String muse::mtrc(const char *context, const String &key, const char *disambiguation, int n) {
    ByteArray keyutf8 = key.to_utf8();
    return translate(context, keyutf8.const_char(), disambiguation, n);
}

String muse::mtrc(const char *context, const String &key, const String &disambiguation, int n) {
    ByteArray keyutf8 = key.to_utf8();
    ByteArray disutf8 = disambiguation.to_utf8();
    return translate(context, keyutf8.const_char(), disutf8.empty() ? nullptr : disutf8.const_char(), n);
}
