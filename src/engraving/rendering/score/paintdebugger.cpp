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
#include "paintdebugger.h"

using namespace mu;
using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

PaintDebugger::PaintDebugger(IPaintProviderPtr real) : real_(real) {}

IPaintProviderPtr PaintDebugger::real_provider() const { return real_; }

void PaintDebugger::set_debug_pen_color(const Color &c) {
    debug_pen_color_ = c;
    const Pen &original_pen = pen();
    original_pen_color_ = original_pen.color();
    Pen p = original_pen;
    p.set_color(c);
    set_pen(p);
}

void PaintDebugger::restore_pen_color() {
    Pen p = pen();
    p.set_color(original_pen_color_);
    set_pen(p);
    debug_pen_color_ = Color();
}

void PaintDebugger::begin_target(const std::string &name) { real_->begin_target(name); }

void PaintDebugger::before_end_target_hook(Painter *p) { real_->before_end_target_hook(p); }

bool PaintDebugger::end_target(bool end_draw) { return real_->end_target(end_draw); }

bool PaintDebugger::is_active() const { return real_->is_active(); }

void PaintDebugger::begin_object(const std::string &name) { real_->begin_object(name); }

void PaintDebugger::end_object() { real_->end_object(); }

void PaintDebugger::set_antialiasing(bool arg) { real_->set_antialiasing(arg); }

void PaintDebugger::set_composition_mode(CompositionMode mode) { real_->set_composition_mode(mode); }

void PaintDebugger::set_window(const RectF &window) { real_->set_window(window); }

void PaintDebugger::set_viewport(const RectF &viewport) { real_->set_viewport(viewport); }

void PaintDebugger::set_font(const Font &f) { real_->set_font(f); }

const Font &PaintDebugger::font() const { return real_->font(); }

void PaintDebugger::set_pen(const Pen &pen) {
    Pen p = pen;
    if (debug_pen_color_.is_valid()) {
        p.set_color(debug_pen_color_);
    }
    real_->set_pen(p);
}

void PaintDebugger::set_no_pen() { real_->set_no_pen(); }

const Pen &PaintDebugger::pen() const { return real_->pen(); }

void PaintDebugger::set_brush(const Brush &brush) { real_->set_brush(brush); }

const Brush &PaintDebugger::brush() const { return real_->brush(); }

void PaintDebugger::save() { real_->save(); }

void PaintDebugger::restore() { real_->restore(); }

void PaintDebugger::set_transform(const Transform &transform) { real_->set_transform(transform); }

const Transform &PaintDebugger::transform() const { return real_->transform(); }

// drawing functions

void PaintDebugger::draw_path(const PainterPath &path) { real_->draw_path(path); }

void PaintDebugger::draw_polygon(const PointF *points, size_t point_count, PolygonMode mode) {
    real_->draw_polygon(points, point_count, mode);
}

void PaintDebugger::draw_text(const PointF &point, const String &text) { real_->draw_text(point, text); }

void PaintDebugger::draw_text(const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text) {
    real_->draw_text(rect, alignment, text_flags, text);
}

void PaintDebugger::draw_symbol(const PointF &point, char32_t ucs4_code) { real_->draw_symbol(point, ucs4_code); }

void PaintDebugger::draw_pixmap(const PointF &p, const Pixmap &pm) { real_->draw_pixmap(p, pm); }

void PaintDebugger::draw_tiled_pixmap(const RectF &rect, const Pixmap &pm, const PointF &offset) {
    real_->draw_tiled_pixmap(rect, pm, offset);
}

bool PaintDebugger::has_clipping() const { return real_->has_clipping(); }

void PaintDebugger::set_clip_rect(const RectF &rect) { real_->set_clip_rect(rect); }

void PaintDebugger::set_mask(const RectF &background, const std::vector<RectF> &mask_rects) { real_->set_mask(background, mask_rects); }

void PaintDebugger::set_clipping(bool enable) { real_->set_clipping(enable); }

double mu::engraving::rendering::score::PaintDebugger::device_logical_dpi() const { return real_->device_logical_dpi(); }
