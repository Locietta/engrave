/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Cairo implementation of muse::draw::CairoPainter.
 *
 * The caller owns the cairo_t passed to the constructor; this class never
 * destroys it.  A cairo_t is not a QPaintDevice, so this backend keeps its own
 * font/pen/brush/window/viewport/clip state and mirrors it into Cairo.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "draw/types/brush.h"
#include "draw/types/drawtypes.h"
#include "draw/types/font.h"
#include "draw/types/geometry.h"
#include "draw/types/painterpath.h"
#include "draw/types/pen.h"
#include "draw/types/pixmap.h"
#include "draw/types/transform.h"

struct _cairo;
typedef struct _cairo cairo_t;

namespace music {
struct RuntimeFontsDatabase;
} // namespace music

namespace muse::draw {
struct Painter;

struct CairoPainter {
    /*! deviceDpi is the logical DPI of the Cairo target (the value
     *  Painter::deviceLogicalDpi() must report).  For MuseScore engraving
     *  coordinates the baseline is 1200.
     *
     *  Font point sizes are converted to user-space pixels as
     *  pointSize * deviceDpi / 72.  muse::draw::Painter::applyFontSizeScaling()
     *  first multiplies the point size by 1200 / deviceDpi, so engraving text
     *  ends up at the intended points * 1200 / 72 in score coordinates.
     *
     *  Ownership of context stays with the caller; this object never calls
     *  cairo_destroy().  The cairo_t must outlive the CairoPainter.  Text is
     *  drawn with the font bytes of fontsDatabase, which must outlive it too.
     */
    CairoPainter(cairo_t *context, const music::RuntimeFontsDatabase &fonts_database, double device_dpi = 1200.0);
    ~CairoPainter();

    CairoPainter(const CairoPainter &) = delete;
    CairoPainter &operator=(const CairoPainter &) = delete;

    cairo_t *cairo_context() const;

    bool is_active() const;
    void begin_target(const std::string &name);
    void before_end_target_hook(Painter *painter);
    bool end_target(bool end_draw = false);

    void begin_object(const std::string &name);
    void end_object();

    void set_antialiasing(bool arg);
    void set_composition_mode(CompositionMode mode);
    void set_window(const RectF &window);
    void set_viewport(const RectF &viewport);

    void set_font(const Font &font);
    const Font &font() const;

    void set_pen(const Pen &pen);
    void set_no_pen();
    const Pen &pen() const;

    void set_brush(const Brush &brush);
    const Brush &brush() const;

    void save();
    void restore();

    double device_logical_dpi() const;

    /*! setTransform() installs the complete world*viewport matrix, matching
     *  QPainterProvider.  muse::draw::Painter already composes window/viewport
     *  and passes the result here.  For direct users, window/viewport set
     *  before the first setTransform() are composed automatically; after an
     *  explicit transform the matrix is authoritative.
     */
    void set_transform(const Transform &transform);
    const Transform &transform() const;

    void draw_path(const PainterPath &path);
    void draw_polygon(const PointF *points, size_t point_count, PolygonMode mode);

    void draw_text(const PointF &point, const String &text);
    void draw_text(const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text);

    void draw_symbol(const PointF &point, char32_t ucs4_code);

    void draw_pixmap(const PointF &point, const Pixmap &pm);
    void draw_tiled_pixmap(const RectF &rect, const Pixmap &pm, const PointF &offset = PointF());

    bool has_clipping() const;

    void set_clip_rect(const RectF &rect);
    void set_mask(const RectF &background, const std::vector<RectF> &mask_rects);
    void set_clipping(bool enable);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/*! Convenience spelling for callers that name the provider after the file. */
using CairoPaintProvider = CairoPainter;
using CairoPainterPtr = std::shared_ptr<CairoPainter>;
} // namespace muse::draw
