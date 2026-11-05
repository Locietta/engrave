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

#include <string>

#include "global/types/bytearray.h"
#include "global/stringutils.h"
#include "font.h"
#include "geometry.h"

namespace muse::draw {
using glyph_idx_t = uint32_t;

static constexpr double DPI_F = 5.0;
static constexpr double DPI = 72.0 * DPI_F;

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

inline FontDataKey dataKeyForFont(const Font &f) { return FontDataKey(f.family(), f.bold(), f.italic()); }

struct FontData {
    FontDataKey key;
    muse::ByteArray data;

    inline bool valid() const { return key.valid() && !data.empty(); }
};

struct FaceKey {
    FontDataKey dataKey;
    Font::Type type = Font::Type::Undefined;
    int pixelSize = 0;

    FaceKey() = default;

    FaceKey(const FontDataKey &dk, Font::Type t, int ps) : dataKey(dk), type(t), pixelSize(ps) {}

    inline bool operator==(const FaceKey &o) const { return type == o.type && dataKey == o.dataKey && pixelSize == o.pixelSize; }

    inline bool operator!=(const FaceKey &o) const { return !this->operator==(o); }

    inline bool operator<(const FaceKey &o) const {
        if (type != o.type) {
            return type < o.type;
        } else if (dataKey != o.dataKey) {
            return dataKey < o.dataKey;
        } else {
            return pixelSize < o.pixelSize;
        }
    }
};

inline int pixelSizeForFont(const Font &f) {
    if (f.pixelSize() > 0) {
        return f.pixelSize();
    } else {
        return f.pointSizeF() * DPI / 72.0;
    }
}

inline FaceKey faceKeyForFont(const Font &f) { return FaceKey(dataKeyForFont(f), f.type(), pixelSizeForFont(f)); }

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

    bool isNull() const { return rect.isNull(); }
};

struct FontParams {
    std::string name;
    Font::Type type = Font::Type::Undefined;
    bool bold = false;
    bool italic = false;
    float pointSize = 0.0f;
    bool underline = false;
    bool strike = false;
};
} // namespace muse::draw

#endif // MUSE_DRAW_FONTSTYPES_H
