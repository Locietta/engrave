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

#pragma once

#include <memory>

#include "types/brush.h"
#include "types/drawtypes.h"
#include "types/geometry.h"
#include "types/font.h"
#include "types/pen.h"
#include "types/pixmap.h"
#include "types/transform.h"
#include "types/painterpath.h"

namespace muse::draw {
class Painter;

class IPaintProvider {
public:
    virtual ~IPaintProvider() = default;

    virtual bool is_active() const = 0;
    virtual void begin_target(const std::string &name) = 0;
    virtual void before_end_target_hook(Painter *painter) = 0;
    virtual bool end_target(bool end_draw = false) = 0;
    virtual void begin_object(const std::string &name) = 0;
    virtual void end_object() = 0;

    virtual void set_antialiasing(bool arg) = 0;
    virtual void set_composition_mode(CompositionMode mode) = 0;
    virtual void set_window(const RectF &window) = 0;
    virtual void set_viewport(const RectF &viewport) = 0;

    virtual void set_font(const Font &font) = 0;
    virtual const Font &font() const = 0;

    virtual void set_pen(const Pen &pen) = 0;
    virtual void set_no_pen() = 0;
    virtual const Pen &pen() const = 0;

    virtual void set_brush(const Brush &brush) = 0;
    virtual const Brush &brush() const = 0;

    virtual void save() = 0;
    virtual void restore() = 0;

    virtual double device_logical_dpi() const = 0;

    virtual void set_transform(const Transform &transform) = 0;
    virtual const Transform &transform() const = 0;

    // drawing functions
    virtual void draw_path(const PainterPath &path) = 0;
    virtual void draw_polygon(const PointF *points, size_t point_count, PolygonMode mode) = 0;

    virtual void draw_text(const PointF &point, const String &text) = 0;
    virtual void draw_text(const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text) = 0;

    virtual void draw_symbol(const PointF &point, char32_t ucs4_code) = 0;

    virtual void draw_pixmap(const PointF &point, const Pixmap &pm) = 0;
    virtual void draw_tiled_pixmap(const RectF &rect, const Pixmap &pm, const PointF &offset = PointF()) = 0;

    virtual bool has_clipping() const = 0;

    virtual void set_clip_rect(const RectF &rect) = 0;
    virtual void set_mask(const RectF &background, const std::vector<RectF> &mask_rects) = 0;
    virtual void set_clipping(bool enable) = 0;
};

using IPaintProviderPtr = std::shared_ptr<IPaintProvider>;
} // namespace muse::draw
