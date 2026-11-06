/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#include "draw/ipaintprovider.h"

namespace mu::engraving::rendering::score {
struct PaintDebugger : muse::draw::IPaintProvider {
    PaintDebugger(muse::draw::IPaintProviderPtr real);

    muse::draw::IPaintProviderPtr real_provider() const;
    void set_debug_pen_color(const muse::draw::Color &c);
    void restore_pen_color();

    bool is_active() const override;
    void begin_target(const std::string &name) override;
    void before_end_target_hook(muse::draw::Painter *painter) override;
    bool end_target(bool end_draw = false) override;

    void begin_object(const std::string &name) override;
    void end_object() override;

    void set_antialiasing(bool arg) override;
    void set_composition_mode(muse::draw::CompositionMode mode) override;
    void set_window(const muse::RectF &window) override;
    void set_viewport(const muse::RectF &viewport) override;

    void set_font(const muse::draw::Font &font) override;
    const muse::draw::Font &font() const override;

    void set_pen(const muse::draw::Pen &pen) override;
    void set_no_pen() override;
    const muse::draw::Pen &pen() const override;

    void set_brush(const muse::draw::Brush &brush) override;
    const muse::draw::Brush &brush() const override;

    void save() override;
    void restore() override;

    double device_logical_dpi() const override;

    void set_transform(const muse::draw::Transform &transform) override;
    const muse::draw::Transform &transform() const override;

    void draw_path(const muse::draw::PainterPath &path) override;
    void draw_polygon(const muse::PointF *points, size_t point_count, muse::draw::PolygonMode mode) override;

    void draw_text(const muse::PointF &point, const muse::String &text) override;
    void draw_text(const muse::RectF &rect, muse::draw::Alignment alignment, muse::draw::TextFlags text_flags,
                   const muse::String &text) override;

    void draw_symbol(const muse::PointF &point, char32_t ucs4_code) override;

    void draw_pixmap(const muse::PointF &p, const muse::draw::Pixmap &pm) override;
    void draw_tiled_pixmap(const muse::RectF &rect, const muse::draw::Pixmap &pm, const muse::PointF &offset = muse::PointF()) override;

    bool has_clipping() const override;

    void set_clip_rect(const muse::RectF &rect) override;
    void set_mask(const muse::RectF &background, const std::vector<muse::RectF> &mask_rects) override;
    void set_clipping(bool enable) override;

private:
    muse::draw::IPaintProviderPtr real_ = nullptr;

    muse::draw::Color original_pen_color_;
    muse::draw::Color debug_pen_color_;
};
} // namespace mu::engraving::rendering::score
