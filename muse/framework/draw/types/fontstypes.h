/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#ifndef MUSE_DRAW_FONTSTYPES_H
#define MUSE_DRAW_FONTSTYPES_H

#include <span>
#include <string>

#include "font.h"
#include "geometry.h"
#include "global/stringutils.h"
#include "global/types/bytearray.h"

namespace muse::draw {
using glyph_idx_t = uint32_t;

static constexpr double dpi_f = 5.0;
static constexpr double dpi = 72.0 * ::muse::draw::dpi_f;

struct FontDataKey {
    FontDataKey() = default;

    FontDataKey(const Font::FontFamily &fa) : family_(fa), bold_(false), italic_(false) {}

    FontDataKey(const Font::FontFamily &fa, bool bo, bool it) : family_(fa), bold_(bo), italic_(it) {}

    inline bool valid() const { return family_.valid(); }

    const Font::FontFamily &family() const { return family_; }

    bool bold() const { return bold_; }

    bool italic() const { return italic_; }

    inline bool operator==(const FontDataKey &o) const { return bold_ == o.bold_ && italic_ == o.italic_ && family_ == o.family_; }

    inline bool operator!=(const FontDataKey &o) const { return !this->operator==(o); }

    inline bool operator<(const FontDataKey &o) const {
        if (bold_ != o.bold_) {
            return bold_ < o.bold_;
        }

        if (italic_ != o.italic_) {
            return italic_ < o.italic_;
        }

        return family_ < o.family_;
    }

private:
    Font::FontFamily family_;
    bool bold_ = false;
    bool italic_ = false;
};

inline FontDataKey data_key_for_font(const Font &f) { return FontDataKey(f.family(), f.bold(), f.italic()); }

struct FontData {
    FontDataKey key;
    // Borrowed immutable resource; the font registry must outlive its faces.
    std::span<const uint8_t> data;

    inline bool valid() const { return key.valid() && !data.empty(); }
};

struct FaceKey {
    FontDataKey data_key;
    Font::Type type = Font::Type::UNDEFINED;
    int pixel_size = 0;

    FaceKey() = default;

    FaceKey(const FontDataKey &dk, Font::Type t, int ps) : data_key(dk), type(t), pixel_size(ps) {}

    inline bool operator==(const FaceKey &o) const { return type == o.type && data_key == o.data_key && pixel_size == o.pixel_size; }

    inline bool operator!=(const FaceKey &o) const { return !this->operator==(o); }

    inline bool operator<(const FaceKey &o) const {
        if (type != o.type) {
            return type < o.type;
        } else if (data_key != o.data_key) {
            return data_key < o.data_key;
        } else {
            return pixel_size < o.pixel_size;
        }
    }
};

inline int pixel_size_for_font(const Font &f) {
    if (f.pixel_size() > 0) {
        return f.pixel_size();
    } else {
        return f.point_size_f() * ::muse::draw::dpi / 72.0;
    }
}

inline FaceKey face_key_for_font(const Font &f) { return FaceKey(data_key_for_font(f), f.type(), pixel_size_for_font(f)); }

struct Sdf {
    muse::ByteArray bitmap;
    uint32_t width = 0;
    uint32_t height = 0;
    float threshold = 0.;
    size_t hash = 0;
};

struct GlyphImage {
    muse::RectF rect;
    Sdf sdf;
    float range = 0.f;

    bool is_null() const { return rect.is_null(); }
};

struct FontParams {
    std::string name;
    Font::Type type = Font::Type::UNDEFINED;
    bool bold = false;
    bool italic = false;
    float point_size = 0.0f;
    bool underline = false;
    bool strike = false;
};
} // namespace muse::draw

#endif // MUSE_DRAW_FONTSTYPES_H
