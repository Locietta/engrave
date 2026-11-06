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

#include <stack>

#include "global/types/string.h"
#include "ipaintprovider.h"
#include "types/color.h"
#include "types/drawtypes.h"
#include "types/font.h"
#include "types/geometry.h"
#include "types/pen.h"
#include "types/pixmap.h"

struct QPaintDevice;

namespace muse::draw {
struct Painter {
    Painter(IPaintProviderPtr provider, const std::string &name);

    ~Painter();

    IPaintProviderPtr provider() const;
    void set_provider(const IPaintProviderPtr &p, bool reinit = true);

    bool is_active() const;
    bool end_draw();

    //! NOTE These are methods for debugging and automated testing.
    void begin_object(const std::string &name);
    void end_object();

    // state
    void set_antialiasing(bool arg);
    void set_composition_mode(CompositionMode mode);

    void set_font(const Font &font);
    const Font &font() const;

    void set_pen(const Pen &pen);
    inline void set_pen(const Color &color);
    void set_no_pen();
    const Pen &pen() const;

    void set_brush(const Brush &brush);
    const Brush &brush() const;

    void set_world_transform(const Transform &matrix, bool combine = false);
    const Transform &world_transform() const;
    void scale(double sx, double sy);
    void rotate(double angle);
    void translate(double dx, double dy);
    inline void translate(const PointF &offset);

    RectF window() const;
    void set_window(const RectF &window);
    RectF viewport() const;
    void set_viewport(const RectF &viewport);

    void save();
    void restore();

    double device_logical_dpi() const;

    // drawing
    void fill_path(const PainterPath &path, const Brush &brush);
    void draw_path(const PainterPath &path);
    void stroke_path(const PainterPath &path, const Pen &pen);

    void draw_lines(const LineF *lines, size_t line_count);
    void draw_lines(const PointF *point_pairs, size_t line_count);
    inline void draw_line(const LineF &line);
    inline void draw_line(const PointF &p1, const PointF &p2);
    inline void draw_line(double x1, double y1, double x2, double y2);
    inline void draw_lines(const std::vector<LineF> &lines);

    inline void draw_rect(const RectF &rect);
    inline void draw_rect(double x1, double y1, double w, double h);

    void draw_rects(const RectF *rects, size_t rect_count);

    void draw_rounded_rect(const RectF &rect, double x_radius, double y_radius);

    void draw_ellipse(const RectF &rect);
    inline void draw_ellipse(const PointF &center, double rx, double ry);

    void draw_polyline(const PointF *points, size_t point_count);
    inline void draw_polyline(const PolygonF &polyline);

    void draw_polygon(const PointF *points, size_t point_count, FillRule fill_rule = FillRule::ODD_EVEN_FILL);
    inline void draw_polygon(const PolygonF &polygon, FillRule fill_rule = FillRule::ODD_EVEN_FILL);

    void draw_convex_polygon(const PointF *points, size_t point_count);
    inline void draw_convex_polygon(const PolygonF &polygon);

    void draw_arc(const RectF &rect, int a, int alen);

    void draw_text(const PointF &point, const String &text);
    inline void draw_text(double x, double y, const String &text);

    void draw_text(const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text);

    void draw_symbol(const PointF &point, char32_t ucs4_code);

    void fill_rect(const RectF &rect, const Brush &brush);

    void draw_pixmap(const PointF &point, const Pixmap &pm);
    void draw_tiled_pixmap(const RectF &rect, const Pixmap &pm, const PointF &offset = PointF());

    bool has_clipping() const;

    void set_clip_rect(const RectF &rect);
    void set_mask(const RectF &background, const std::vector<RectF> &mask_rects);
    void set_clipping(bool enable);

    //! NOTE Provider for tests.
    //! We're not ready to use DI (ModuleIoC) here yet
    static IPaintProviderPtr extended;

private:

    struct State {
        RectF window;
        RectF viewport;
        bool is_vx_f = false;
        Transform view_transform;
        bool is_wx_f = false;
        Transform world_transform; // World transformation matrix, not window and viewport
        Transform transform;       // Complete transformation matrix
    };

    void init();
    State &editable_state();
    const State &state() const;
    void update_view_transform();
    void update_matrix();

    void apply_font_size_scaling();

    bool end_target(bool end_draw);

    IPaintProviderPtr provider_;
    std::string name_;
    std::stack<State> states_;
};

inline void Painter::set_pen(const Color &color) { set_pen(Pen(color.is_valid() ? color : Color::black)); }

inline void Painter::translate(const PointF &offset) { translate(offset.x(), offset.y()); }

inline void Painter::draw_line(const LineF &l) { draw_lines(&l, 1); }

inline void Painter::draw_line(const PointF &p1, const PointF &p2) { draw_line(LineF(p1, p2)); }

inline void Painter::draw_line(double x1, double y1, double x2, double y2) {
    LineF l(PointF(x1, y1), PointF(x2, y2));
    draw_lines(&l, 1);
}

inline void Painter::draw_lines(const std::vector<LineF> &lines) { draw_lines(lines.data(), lines.size()); }

inline void Painter::draw_rect(const RectF &rect) { draw_rects(&rect, 1); }

inline void Painter::draw_rect(double x, double y, double w, double h) {
    RectF r(x, y, w, h);
    draw_rects(&r, 1);
}

inline void Painter::draw_ellipse(const PointF &center, double rx, double ry) {
    draw_ellipse(RectF(center.x() - rx, center.y() - ry, 2 * rx, 2 * ry));
}

inline void Painter::draw_polyline(const PolygonF &polyline) { draw_polyline(polyline.data(), polyline.size()); }

inline void Painter::draw_polygon(const PolygonF &polygon, FillRule fill_rule) { draw_polygon(polygon.data(), polygon.size(), fill_rule); }

inline void Painter::draw_convex_polygon(const PolygonF &poly) { draw_convex_polygon(poly.data(), poly.size()); }

inline void Painter::draw_text(double x, double y, const String &text) { draw_text(PointF(x, y), text); }

struct PainterItemMarker {
    PainterItemMarker(Painter *p, const std::string &name) : painter_(p) {
        if (!enabled) {
            return;
        }
        p->begin_object(name);
    }

    ~PainterItemMarker() {
        if (!enabled) {
            return;
        }
        painter_->end_object();
    }

    static bool enabled;

private:
    Painter *painter_ = nullptr;
};
} // namespace muse::draw

#define TRACE_ITEM_DRAW
#define TRACE_DRAW_ITEM
#define TRACE_ITEM_DRAW_C(painter, objName)
