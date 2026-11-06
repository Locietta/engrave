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
#include "painter.h"

#include "types/brush.h"
#include "types/painterpath.h"

#include "log.h"

using namespace muse;
using namespace muse::draw;

IPaintProviderPtr Painter::extended;
bool PainterItemMarker::enabled = true;

Painter::Painter(IPaintProviderPtr provider, const std::string &name) : provider_(provider), name_(name) { init(); }

Painter::~Painter() { end_target(false); }

void Painter::init() {
    provider_->begin_target(name_);
    if (extended) {
        extended->begin_target(name_);
    }

    State st;
    st.world_transform = provider_->transform();
    st.is_wx_f = true;
    st.transform = st.world_transform;

    states_ = std::stack<State>();
    states_.push(std::move(st));
}

IPaintProviderPtr Painter::provider() const { return provider_; }

void Painter::set_provider(const IPaintProviderPtr &p, bool reinit) {
    provider_ = p;
    if (reinit) {
        init();
    }
}

bool Painter::end_draw() { return end_target(true); }

bool Painter::end_target(bool end_draw) {
    provider_->before_end_target_hook(this);
    if (extended) {
        extended->before_end_target_hook(this);
    }

    bool ok = provider_->end_target(end_draw);
    if (extended) {
        extended->end_target(end_draw);
    }
    return ok;
}

bool Painter::is_active() const { return provider_->is_active(); }

void Painter::begin_object(const std::string &name) {
    provider_->begin_object(name);
    if (extended) {
        extended->begin_object(name);
    }
}

void Painter::end_object() {
    provider_->end_object();
    if (extended) {
        extended->end_object();
    }
}

void Painter::set_antialiasing(bool arg) {
    provider_->set_antialiasing(arg);
    if (extended) {
        extended->set_antialiasing(arg);
    }
}

void Painter::set_composition_mode(CompositionMode mode) {
    provider_->set_composition_mode(mode);
    if (extended) {
        extended->set_composition_mode(mode);
    }
}

void Painter::set_font(const Font &font) {
    provider_->set_font(font);
    if (extended) {
        extended->set_font(font);
    }
}

const Font &Painter::font() const { return provider_->font(); }

void Painter::set_pen(const Pen &pen) {
    provider_->set_pen(pen);
    if (extended) {
        extended->set_pen(pen);
    }
}

void Painter::set_no_pen() { set_pen(Pen(PenStyle::NO_PEN)); }

const Pen &Painter::pen() const { return provider_->pen(); }

void Painter::set_brush(const Brush &brush) {
    provider_->set_brush(brush);
    if (extended) {
        extended->set_brush(brush);
    }
}

const Brush &Painter::brush() const { return provider_->brush(); }

void Painter::save() {
    State new_st = states_.top();
    states_.push(new_st);

    provider_->save();
    if (extended) {
        extended->save();
    }
}

void Painter::restore() {
    if (states_.size() > 0) {
        states_.pop();
    }

    provider_->restore();
    if (extended) {
        extended->restore();
    }
}

double Painter::device_logical_dpi() const { return provider_->device_logical_dpi(); }

void Painter::set_world_transform(const Transform &matrix, bool combine) {
    State &st = editable_state();
    if (combine) {
        st.world_transform = matrix * st.world_transform; // combines
    } else {
        st.world_transform = matrix; // set new matrix
    }
    st.is_wx_f = true;
    update_matrix();
}

const Transform &Painter::world_transform() const { return state().world_transform; }

void Painter::scale(double sx, double sy) {
    State &st = editable_state();
    st.world_transform.scale(sx, sy);
    st.is_wx_f = true;
    update_matrix();
}

void Painter::rotate(double angle) {
    State &st = editable_state();
    st.world_transform.rotate(angle);
    st.is_wx_f = true;
    update_matrix();
}

void Painter::translate(double dx, double dy) {
    State &st = editable_state();
    st.world_transform.translate(dx, dy);
    st.is_wx_f = true;
    update_matrix();
}

RectF Painter::window() const { return state().window; }

void Painter::set_window(const RectF &window) {
    State &st = editable_state();
    st.window = window;
    st.is_vx_f = true;
    update_view_transform();

    // for debug purpose
    provider_->set_window(window);
    if (extended) {
        extended->set_window(window);
    }
}

RectF Painter::viewport() const { return state().viewport; }

void Painter::set_viewport(const RectF &viewport) {
    State &st = editable_state();
    st.viewport = viewport;
    st.is_vx_f = true;
    update_view_transform();

    // for debug purpose
    provider_->set_viewport(viewport);
    if (extended) {
        extended->set_viewport(viewport);
    }
}

// drawing functions

void Painter::fill_path(const PainterPath &path, const Brush &brush) {
    Pen old_pen = this->pen();
    Brush old_brush = this->brush();
    set_pen(Pen(PenStyle::NO_PEN));
    set_brush(brush);

    draw_path(path);

    set_pen(old_pen);
    set_brush(old_brush);
}

void Painter::stroke_path(const PainterPath &path, const Pen &pen) {
    Pen old_pen = this->pen();
    Brush old_brush = this->brush();
    set_pen(pen);
    set_brush(BrushStyle::NO_BRUSH);

    draw_path(path);

    set_pen(old_pen);
    set_brush(old_brush);
}

void Painter::draw_path(const PainterPath &path) {
    provider_->draw_path(path);
    if (extended) {
        extended->draw_path(path);
    }
}

void Painter::draw_lines(const LineF *lines, size_t line_count) {
    for (size_t i = 0; i < line_count; ++i) {
        PointF pts[2] = {lines[i].p1(), lines[i].p2()};
        if (pts[0] == pts[1]) {
            LOGE() << "draw point not implemented";
        }
        draw_polyline(pts, 2);
    }
}

void Painter::draw_lines(const PointF *point_pairs, size_t line_count) {
    static_assert(sizeof(LineF) == 2 * sizeof(PointF), "must be: sizeof(LineF) == 2 * sizeof(PointF)");

    draw_lines((const LineF *) point_pairs, line_count);
}

void Painter::draw_rects(const RectF *rects, size_t rect_count) {
    for (size_t i = 0; i < rect_count; ++i) {
        PainterPath path;
        path.add_rect(rects[i]);
        if (path.is_empty()) {
            continue;
        }
        draw_path(path);
    }
}

void Painter::draw_ellipse(const RectF &rect) {
    PainterPath path;
    path.add_ellipse(rect);
    provider_->draw_path(path);
    if (extended) {
        extended->draw_path(path);
    }
}

void Painter::draw_polyline(const PointF *points, size_t point_count) {
    provider_->draw_polygon(points, point_count, PolygonMode::POLYLINE);
    if (extended) {
        extended->draw_polygon(points, point_count, PolygonMode::POLYLINE);
    }
}

void Painter::draw_polygon(const PointF *points, size_t point_count, FillRule fill_rule) {
    PolygonMode mode = (fill_rule == FillRule::ODD_EVEN_FILL) ? PolygonMode::ODD_EVEN : PolygonMode::WINDING;
    provider_->draw_polygon(points, point_count, mode);
    if (extended) {
        extended->draw_polygon(points, point_count, mode);
    }
}

void Painter::draw_convex_polygon(const PointF *points, size_t point_count) {
    provider_->draw_polygon(points, point_count, PolygonMode::CONVEX);
    if (extended) {
        extended->draw_polygon(points, point_count, PolygonMode::CONVEX);
    }
}

void Painter::draw_arc(const RectF &r, int a, int alen) {
    //! NOTE Copied from QPainter source code

    RectF rect = r.normalized();

    PainterPath path;
    path.arc_move_to(rect, a / 16.0);
    path.arc_to(rect, a / 16.0, alen / 16.0);
    stroke_path(path, pen());
}

void Painter::draw_rounded_rect(const RectF &rect, double x_radius, double y_radius) {
    if (x_radius <= 0 || y_radius <= 0) { // draw normal rectangle
        draw_rect(rect);
        return;
    }

    PainterPath path;
    path.add_rounded_rect(rect, x_radius, y_radius);
    draw_path(path);
}

void Painter::apply_font_size_scaling() {
    static constexpr double mu_engraving_dpi = 1200; // Same as mu::engraving::DPI. TODO: pass as parameter.
    Font f = font();
    double scaled_point_size = f.point_size_f() * mu_engraving_dpi / device_logical_dpi();
    f.set_point_size_f(scaled_point_size);
    set_font(f);
}

void Painter::draw_text(const PointF &point, const String &text) {
    apply_font_size_scaling();

    provider_->draw_text(point, text);
    if (extended) {
        extended->draw_text(point, text);
    }
}

void Painter::draw_text(const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text) {
    apply_font_size_scaling();

    provider_->draw_text(rect, alignment, text_flags, text);
    if (extended) {
        extended->draw_text(rect, alignment, text_flags, text);
    }
}

void Painter::draw_symbol(const PointF &point, char32_t ucs4_code) {
    apply_font_size_scaling();

    provider_->draw_symbol(point, ucs4_code);
    if (extended) {
        extended->draw_symbol(point, ucs4_code);
    }
}

void Painter::fill_rect(const RectF &rect, const Brush &brush) {
    Pen old_pen = this->pen();
    Brush old_brush = this->brush();
    set_pen(Pen(PenStyle::NO_PEN));
    set_brush(brush);

    draw_rect(rect);

    set_brush(old_brush);
    set_pen(old_pen);
}

void Painter::draw_pixmap(const PointF &point, const Pixmap &pm) {
    provider_->draw_pixmap(point, pm);
    if (extended) {
        extended->draw_pixmap(point, pm);
    }
}

void Painter::draw_tiled_pixmap(const RectF &rect, const Pixmap &pm, const PointF &offset) {
    provider_->draw_tiled_pixmap(rect, pm, offset);
    if (extended) {
        extended->draw_tiled_pixmap(rect, pm, offset);
    }
}

Painter::State &Painter::editable_state() { return states_.top(); }

const Painter::State &Painter::state() const { return states_.top(); }

void Painter::update_view_transform() {
    State &st = editable_state();
    if (st.window.is_null()) {
        st.view_transform = Transform();
        update_matrix();
        return;
    }

    double scale_w = double(st.viewport.width()) / double(st.window.width());
    double scale_h = double(st.viewport.height()) / double(st.window.height());
    st.view_transform =
        Transform(scale_w, 0, 0, scale_h, st.viewport.x() - st.window.x() * scale_w, st.viewport.y() - st.window.y() * scale_h);
    update_matrix();
}

void Painter::update_matrix() {
    Painter::State &st = editable_state();
    st.transform = st.is_wx_f ? st.world_transform : Transform();
    if (st.is_vx_f) {
        st.transform *= st.view_transform;
    }

    provider_->set_transform(st.transform);
    if (extended) {
        extended->set_transform(st.transform);
    }
}

bool Painter::has_clipping() const { return provider_->has_clipping(); }

void Painter::set_clip_rect(const RectF &rect) { provider_->set_clip_rect(rect); }

void Painter::set_mask(const RectF &background, const std::vector<RectF> &mask_rects) { provider_->set_mask(background, mask_rects); }

void Painter::set_clipping(bool enable) { provider_->set_clipping(enable); }
