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
#ifndef MUSE_DRAW_FONT_H
#define MUSE_DRAW_FONT_H

#include "global/types/flags.h"
#include "global/types/string.h"

namespace muse::draw {
struct Font {
    struct FontFamily {
        FontFamily() = default;

        FontFamily(const String &id) : id_(id.to_lower()) {}

        FontFamily(const char16_t *id) : FontFamily(String(id)) {}

        FontFamily(const std::string &id) : FontFamily(String::from_std_string(id)) {}

        inline bool valid() const { return !id_.empty(); }

        const String &id() const { return id_; }

        inline bool operator==(const FontFamily &o) const { return id_ == o.id_; }

        inline bool operator<(const FontFamily &o) const { return id_ < o.id_; }

    private:
        String id_;
    };

    enum class Type { UNDEFINED = 0, UNKNOWN, ICON, TEXT, MUSIC_SYMBOL_TEXT, MUSIC_SYMBOL, TABLATURE, HARMONY };

    Font() = default;
    Font(const FontFamily &family, Type type);

    enum class Style {
        //Undefined   = -1,
        NORMAL = 0,
        BOLD = 1 << 0,
        ITALIC = 1 << 1,
        UNDERLINE = 1 << 2,
        STRIKE = 1 << 3
    };

    enum class Hinting { PREFER_DEFAULT_HINTING = 0, PREFER_NO_HINTING = 1, PREFER_VERTICAL_HINTING = 2, PREFER_FULL_HINTING = 3 };

    enum Weight {
        THIN = 0,         // 100
        EXTRA_LIGHT = 12, // 200
        LIGHT = 25,       // 300
        NORMAL = 50,      // 400
        MEDIUM = 57,      // 500
        DEMI_BOLD = 63,   // 600
        BOLD = 75,        // 700
        EXTRA_BOLD = 81,  // 800
        BLACK = 87        // 900
    };

    void set_family(const FontFamily &family, Type type);
    const FontFamily &family() const;
    Type type() const;

    double point_size_f() const;
    void set_point_size_f(double s);

    int pixel_size() const;
    void set_pixel_size(int s);

    Weight weight() const;
    void set_weight(Weight w);

    bool bold() const;
    void set_bold(bool arg);
    bool italic() const;
    void set_italic(bool arg);
    bool underline() const;
    void set_underline(bool arg);
    bool strike() const;
    void set_strike(bool arg);

    void set_no_font_merging(bool arg);
    bool no_font_merging() const;

    Hinting hinting() const;
    void set_hinting(Hinting hinting);

    bool operator==(const Font &other) const;

    bool operator!=(const Font &other) const { return !this->operator==(other); }

    static bool g_disable_font_merging;

private:

    FontFamily family_;
    Type type_ = Type::UNDEFINED;
    double point_size_f_ = -1.0;
    int pixel_size_ = -1;
    Weight weight_ = Weight::NORMAL;
    muse::Flags<Style> style_{Style::NORMAL};
    bool no_font_merging_ = false;
    Hinting hinting_ = Hinting::PREFER_DEFAULT_HINTING;
};
} // namespace muse::draw

#endif // MUSE_DRAW_FONT_H
