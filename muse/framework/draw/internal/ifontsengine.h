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
#pragma once

#include <string>

#include "global/io/path.h"
#include "types/font.h"
#include "types/geometry.h"
#include "types/fontstypes.h"

namespace muse::draw {
struct IFontsEngine {
    virtual ~IFontsEngine() = default;

    virtual void set_render_cache_dir_path(const io::PathT &path, const std::string &revision = std::string()) = 0;
    virtual void clear_loaded_faces() = 0;

    virtual double line_spacing(const Font &f) const = 0;
    virtual double x_height(const Font &f) const = 0;
    virtual double height(const Font &f) const = 0;
    virtual double cap_height(const Font &ff) const = 0;
    virtual double ascent(const Font &f) const = 0;
    virtual double descent(const Font &f) const = 0;

    virtual double underline_pos(const Font &f) const = 0;
    virtual double line_width(const Font &f) const = 0;
    virtual double strike_out_pos(const Font &f) const = 0;

    virtual bool in_font(const Font &f, char32_t ucs4) const = 0;

    virtual double horizontal_advance(const Font &f, const char32_t &ch) const = 0;
    virtual double horizontal_advance(const Font &f, const std::u32string &text) const = 0;

    virtual RectF bounding_rect(const Font &f, const char32_t &ch) const = 0;
    virtual RectF bounding_rect(const Font &f, const std::u32string &text) const = 0;
    virtual RectF tight_bounding_rect(const Font &f, const std::u32string &text) const = 0;

    // Draw
    virtual std::vector<GlyphImage> render(const Font &f, const std::u32string &text) const = 0;
};
} // namespace muse::draw
