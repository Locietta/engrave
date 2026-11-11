/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Cairo implementation of muse::draw::CairoPainter.
 *
 * Text uses the font bytes actually registered in music::RuntimeFontsDatabase.  FreeType
 * memory faces are cached for the process lifetime, Cairo FT font faces are
 * created on top of them, and HarfBuzz performs shaping so glyph advances and
 * mark offsets are honoured.
 */

#include "cairo_painter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <cairo/cairo-ft.h>
#include <cairo/cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb-ft.h>
#include <hb.h>

#include "draw/internal/fontfaceutils.h"
#include "engrave/src/runtime/fonts_database.h"

namespace muse::draw {
namespace {

// ---------------------------------------------------------------- utilities

bool is_finite(double v) { return std::isfinite(v); }

bool finite_rect(const RectF &r) { return is_finite(r.x()) && is_finite(r.y()) && is_finite(r.width()) && is_finite(r.height()); }

bool finite_point(const PointF &p) { return is_finite(p.x()) && is_finite(p.y()); }

bool finite_transform(const Transform &t) {
    return is_finite(t.m11()) && is_finite(t.m12()) && is_finite(t.m13()) && is_finite(t.m21()) && is_finite(t.m22()) &&
           is_finite(t.m23()) && is_finite(t.m31()) && is_finite(t.m32()) && is_finite(t.m33());
}

bool nearly_equal(double a, double b) {
    const double diff = std::abs(a - b);
    const double scale = std::max(1.0, std::max(std::abs(a), std::abs(b)));
    return diff <= 1e-9 * scale;
}

bool transform_nearly_equal(const Transform &a, const Transform &b) {
    return nearly_equal(a.m11(), b.m11()) && nearly_equal(a.m12(), b.m12()) && nearly_equal(a.m13(), b.m13()) &&
           nearly_equal(a.m21(), b.m21()) && nearly_equal(a.m22(), b.m22()) && nearly_equal(a.m23(), b.m23()) &&
           nearly_equal(a.m31(), b.m31()) && nearly_equal(a.m32(), b.m32()) && nearly_equal(a.m33(), b.m33());
}

bool is_space_char(char16_t c) { return muse::Char::is_space(c); }

void unique_coordinates(std::vector<double> &values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end(),
                             [](double a, double b) { return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)}); }),
                 values.end());
}

//! Exact (floating-point) rectangle subtraction on a rectilinear grid.  The
//! result is a set of disjoint rectangles representing background minus the
//! union of masks.  This keeps PDF/SVG clipping vector-accurate instead of
//! snapping to integer device pixels, and handles overlapping masks.
std::vector<cairo_rectangle_t> subtract_rects(const cairo_rectangle_t &background, const std::vector<cairo_rectangle_t> &masks) {
    std::vector<cairo_rectangle_t> result;
    if (!(background.width > 0.0) || !(background.height > 0.0)) {
        return result;
    }

    const double background_top = background.y;
    const double background_bottom = background.y + background.height;

    std::vector<double> ys;
    ys.push_back(background_top);
    ys.push_back(background_bottom);
    for (const cairo_rectangle_t &mask : masks) {
        if (!(mask.width > 0.0) || !(mask.height > 0.0)) {
            continue;
        }
        const double top = std::max(background_top, mask.y);
        const double bottom = std::min(background_bottom, mask.y + mask.height);
        if (bottom > top) {
            ys.push_back(top);
            ys.push_back(bottom);
        }
    }
    unique_coordinates(ys);
    if (ys.size() < 2) {
        return result;
    }

    const double background_left = background.x;
    const double background_right = background.x + background.width;

    std::map<std::pair<double, double>, size_t> previous_spans;
    for (size_t yi = 0; yi + 1 < ys.size(); ++yi) {
        const double y0 = ys[yi];
        const double y1 = ys[yi + 1];
        if (!(y1 > y0)) {
            continue;
        }

        const double cy = (y0 + y1) / 2.0;
        std::vector<std::pair<double, double>> blocked;
        for (const cairo_rectangle_t &mask : masks) {
            if (mask.width <= 0.0 || mask.height <= 0.0) {
                continue;
            }
            if (cy >= mask.y && cy <= mask.y + mask.height) {
                blocked.emplace_back(mask.x, mask.x + mask.width);
            }
        }
        std::sort(blocked.begin(), blocked.end());

        std::vector<std::pair<double, double>> merged;
        for (const std::pair<double, double> &interval : blocked) {
            if (merged.empty() || interval.first > merged.back().second) {
                merged.push_back(interval);
            } else {
                merged.back().second = std::max(merged.back().second, interval.second);
            }
        }

        std::map<std::pair<double, double>, size_t> current_spans;
        auto emit_run = [&](double x0, double x1) {
            if (!(x1 > x0)) {
                return;
            }
            const std::pair<double, double> span(x0, x1);
            auto previous = previous_spans.find(span);
            if (previous != previous_spans.end()) {
                cairo_rectangle_t &rect = result[previous->second];
                if (std::abs(rect.y + rect.height - y0) <= 1e-9 * std::max(1.0, std::abs(y0))) {
                    rect.height = y1 - rect.y;
                    current_spans[span] = previous->second;
                    return;
                }
            }
            cairo_rectangle_t rect;
            rect.x = x0;
            rect.y = y0;
            rect.width = x1 - x0;
            rect.height = y1 - y0;
            result.push_back(rect);
            current_spans[span] = result.size() - 1;
        };

        double cursor = background_left;
        for (const std::pair<double, double> &interval : merged) {
            if (interval.second <= cursor) {
                continue;
            }
            if (interval.first > cursor) {
                emit_run(cursor, std::min(interval.first, background_right));
            }
            cursor = std::max(cursor, interval.second);
            if (cursor >= background_right) {
                break;
            }
        }
        if (cursor < background_right) {
            emit_run(cursor, background_right);
        }

        previous_spans.swap(current_spans);
    }

    return result;
}

std::u16string normalize_text(const std::u16string &in, bool expand_tabs) {
    std::u16string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        const char16_t c = in[i];
        if (c == u'\r') {
            if (i + 1 < in.size() && in[i + 1] == u'\n') {
                ++i;
            }
            out.push_back(u'\n');
        } else if (c == u'\t' && expand_tabs) {
            out.append(8, u' ');
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::vector<std::u16string> split_paragraphs(const std::u16string &text) {
    std::vector<std::u16string> out;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == u'\n') {
            out.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    out.push_back(text.substr(start));
    return out;
}

std::u16string join_with_spaces(const std::vector<std::u16string> &paragraphs) {
    std::u16string out;
    for (size_t i = 0; i < paragraphs.size(); ++i) {
        if (i > 0) {
            out.push_back(u' ');
        }
        out += paragraphs[i];
    }
    return out;
}

uint64_t fnv1a(const uint8_t *data, size_t size) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i) {
        h ^= static_cast<uint64_t>(data[i]);
        h *= 1099511628211ull;
    }
    return h;
}

struct PngReadClosure {
    const uint8_t *data = nullptr;
    size_t size = 0;
    size_t pos = 0;
};

cairo_status_t read_png_stream(void *closure, unsigned char *data, unsigned int length) {
    PngReadClosure *c = static_cast<PngReadClosure *>(closure);
    if (!c || !data) {
        return CAIRO_STATUS_READ_ERROR;
    }
    if (length > c->size - c->pos) {
        return CAIRO_STATUS_READ_ERROR;
    }
    std::memcpy(data, c->data + c->pos, length);
    c->pos += length;
    return CAIRO_STATUS_SUCCESS;
}

struct CairoStateGuard {
    explicit CairoStateGuard(cairo_t *context) : cr(context) { cairo_save(cr); }

    ~CairoStateGuard() { cairo_restore(cr); }

    CairoStateGuard(const CairoStateGuard &) = delete;
    CairoStateGuard &operator=(const CairoStateGuard &) = delete;

    cairo_t *cr;
};

// ------------------------------------------------------------ font resources

struct FtFaceEntry {
    FontDataKey key;
    std::vector<uint8_t> data;
    FT_Face ft = nullptr;
    hb_face_t *hb = nullptr;
    unsigned int ft_upem = 1000;
    unsigned int hb_upem = 1000;
    int ascender = 0;
    int descender = 0;
    int height = 0;
    int underline_position_value = 0;
    int underline_thickness_value = 0;
    // Lazily created; mutable so text drawing can use a const face entry.
    mutable std::array<cairo_font_face_t *, 4> cairo_faces{nullptr, nullptr, nullptr, nullptr};
    mutable std::array<FT_Face, 4> variant_faces{nullptr, nullptr, nullptr, nullptr};

    ~FtFaceEntry() {
        for (cairo_font_face_t *&f : cairo_faces) {
            if (f) {
                cairo_font_face_destroy(f);
                f = nullptr;
            }
        }
        for (FT_Face variant : variant_faces) {
            if (variant) FT_Done_Face(variant);
        }
        if (hb) {
            hb_face_destroy(hb);
            hb = nullptr;
        }
        if (ft) {
            FT_Done_Face(ft);
            ft = nullptr;
        }
    }

    cairo_font_face_t *cairo_face(bool synth_bold, bool synth_italic) const {
        const size_t index = (synth_bold ? 1u : 0u) | (synth_italic ? 2u : 0u);
        cairo_font_face_t *&slot = cairo_faces[index];
        if (slot) {
            return slot;
        }

        // Cairo can reuse a face for the same FT_Face; synthesis flags must not
        // mutate the face used by a previously rendered plain style.
        if (!variant_faces[index] && FT_New_Memory_Face(ft->glyph->library, data.data(), static_cast<FT_Long>(data.size()), ft->face_index,
                                                        &variant_faces[index]) != 0) {
            throw std::runtime_error("CairoPainter: cannot create independent font style face");
        }
        cairo_font_face_t *face = cairo_ft_font_face_create_for_ft_face(variant_faces[index], 0);
        cairo_status_t status = cairo_font_face_status(face);
        if (status != CAIRO_STATUS_SUCCESS) {
            cairo_font_face_destroy(face);
            throw std::runtime_error(std::string("CairoPainter: cannot create Cairo FT font face: ") + cairo_status_to_string(status));
        }

        if (synth_bold || synth_italic) {
            unsigned int flags = 0;
            if (synth_bold) {
                flags |= CAIRO_FT_SYNTHESIZE_BOLD;
            }
            if (synth_italic) {
                flags |= CAIRO_FT_SYNTHESIZE_OBLIQUE;
            }
            cairo_ft_font_face_set_synthesize(face, flags);
            status = cairo_font_face_status(face);
            if (status != CAIRO_STATUS_SUCCESS) {
                cairo_font_face_destroy(face);
                throw std::runtime_error(std::string("CairoPainter: cannot configure synthesized font face: ") +
                                         cairo_status_to_string(status));
            }
        }

        slot = face;
        return slot;
    }
};

struct FontCache {
    std::shared_ptr<FtFaceEntry> get(const Font &font, const music::RuntimeFontsDatabase &db) {
        const FontDataKey requested = data_key_for_font(font);
        if (!requested.valid()) {
            throw std::runtime_error("CairoPainter: text drawing requires a non-empty font family");
        }

        const CacheKey request_key = std::make_pair(requested, font.type());

        std::lock_guard<std::mutex> lock(mutex_);
        const auto active = db.active_table();
        if (active != active_) {
            // A published registry can change both fallback resolution and the
            // bytes selected for an existing family/style. Only request aliases
            // expire; resolved faces remain alive for Cairo surfaces using them.
            by_request_.clear();
            active_ = active;
        }
        auto request_it = by_request_.find(request_key);
        if (request_it != by_request_.end()) {
            return request_it->second;
        }

        const FontData font_data = db.font_data(requested, font.type());
        if (!font_data.valid()) {
            throw std::runtime_error("CairoPainter: music::RuntimeFontsDatabase returned no font data for family '" +
                                     requested.family().id().to_std_string() + "'");
        }

        const music::FontRecord *record = music::find_record(*active, font_data.key);
        const ActualKey actual_key(font_data.key, font.type(), record ? record->bytes : nullptr);
        auto actual_it = by_actual_.find(actual_key);
        if (actual_it != by_actual_.end()) {
            by_request_[request_key] = actual_it->second;
            return actual_it->second;
        }

        if (!ft_library_) {
            if (FT_Init_FreeType(&ft_library_) != 0) {
                throw std::runtime_error("CairoPainter: FT_Init_FreeType failed");
            }
        }

        if (font_data.data.size() > static_cast<size_t>(std::numeric_limits<FT_Long>::max())) {
            throw std::runtime_error("CairoPainter: font file is too large for FreeType");
        }

        auto entry = std::make_shared<FtFaceEntry>();
        entry->key = font_data.key;
        entry->data.assign(font_data.data.const_data(), font_data.data.const_data() + font_data.data.size());
        if (entry->data.empty()) {
            throw std::runtime_error("CairoPainter: empty font file");
        }

        FT_Error ft_error =
            new_matching_memory_face(ft_library_, entry->data.data(), static_cast<FT_Long>(entry->data.size()), font_data.key, &entry->ft);
        if (ft_error != 0 || !entry->ft) {
            throw std::runtime_error("CairoPainter: FT_New_Memory_Face failed for family '" + font_data.key.family().id().to_std_string() +
                                     "'");
        }

        entry->ft_upem = entry->ft->units_per_EM ? static_cast<unsigned int>(entry->ft->units_per_EM) : 1000u;
        entry->ascender = static_cast<int>(entry->ft->ascender);
        entry->descender = static_cast<int>(entry->ft->descender);
        entry->height = static_cast<int>(entry->ft->height);
        entry->underline_position_value = static_cast<int>(entry->ft->underline_position);
        entry->underline_thickness_value = static_cast<int>(entry->ft->underline_thickness);

        entry->hb = hb_ft_face_create_referenced(entry->ft);
        if (!entry->hb) {
            throw std::runtime_error("CairoPainter: hb_ft_face_create_referenced failed");
        }
        entry->hb_upem = hb_face_get_upem(entry->hb);
        if (!entry->hb_upem) {
            entry->hb_upem = entry->ft_upem;
        }

        by_actual_[actual_key] = entry;
        by_request_[request_key] = entry;
        return entry;
    }

private:
    using CacheKey = std::pair<FontDataKey, Font::Type>;
    // Retain immutable runtime byte identity so replacements get new faces,
    // reactivation reuses old faces, and an identity can never be recycled.
    using ActualKey = std::tuple<FontDataKey, Font::Type, std::shared_ptr<const muse::ByteArray>>;

    std::shared_ptr<const music::ActiveTable> active_;
    FT_Library ft_library_ = nullptr;
    std::mutex mutex_;
    std::map<ActualKey, std::shared_ptr<FtFaceEntry>> by_actual_;
    std::map<CacheKey, std::shared_ptr<FtFaceEntry>> by_request_;
};

FontCache &font_cache() {
    // Deliberately process-lifetime: Cairo/FreeType font faces must outlive all
    // painters, and avoiding static destruction removes a shutdown-order hazard.
    static FontCache *cache = new FontCache();
    return *cache;
}

struct ShapedLine {
    std::u16string text;
    std::vector<cairo_glyph_t> glyphs;
    std::vector<unsigned int> clusters;
    double width = 0.0;
};

double ascent_for(const FtFaceEntry &entry, double pixel_size) {
    return std::max(0.0, double(entry.ascender) * pixel_size / double(entry.ft_upem));
}

double descent_for(const FtFaceEntry &entry, double pixel_size) {
    return std::max(0.0, -double(entry.descender) * pixel_size / double(entry.ft_upem));
}

double line_height_for(const FtFaceEntry &entry, double pixel_size) {
    const double asc = ascent_for(entry, pixel_size);
    const double desc = descent_for(entry, pixel_size);
    double height = double(entry.height) * pixel_size / double(entry.ft_upem);
    height = std::max(height, asc + desc);
    if (!(height > 0.0)) {
        height = pixel_size;
    }
    return height;
}

} // namespace

// ------------------------------------------------------------------- Impl

struct CairoPainter::Impl {
    struct State {
        Font font;
        Pen pen;
        Brush brush = Brush(BrushStyle::NO_BRUSH);
        Transform world_transform;
        Transform view_transform;
        Transform transform;
        RectF window;
        RectF viewport;
        bool has_view = false;
        //! Once setTransform() has been used, its matrix is authoritative.
        //! muse::draw::Painter composes window/viewport itself and passes the
        //! complete matrix here; before that, window/viewport are composed for
        //! callers that use this provider directly.
        bool transform_explicit = false;
        bool antialiasing = false;
        CompositionMode composition = CompositionMode::SOURCE_OVER;
        bool clipping_enabled = true;
        bool has_user_clip = false;
        std::vector<cairo_rectangle_t> user_clip_rects;
    };

    struct CachedSurface {
        size_t size = 0;
        uint64_t hash = 0;
        cairo_surface_t *surface = nullptr;
    };

    cairo_t *cr = nullptr;
    const music::RuntimeFontsDatabase &fonts;
    double dpi = 1200.0;
    bool active = true;
    State state;
    std::vector<State> stack;
    std::vector<cairo_rectangle_t> base_clip;
    // PDF/SVG surfaces may report CLIP_NOT_REPRESENTABLE even for an unclipped
    // context.  In that case we conservatively assume the surface bounds.
    bool base_clip_known = true;
    std::map<unsigned int, CachedSurface> surfaces;

    Impl(cairo_t *context, const music::RuntimeFontsDatabase &fonts_database, double device_dpi);
    ~Impl();

    bool is_active() const;
    void begin_target(const std::string &name);
    void before_end_target_hook(Painter *painter);
    bool end_target(bool end_draw);
    void begin_object(const std::string &name);
    void end_object();

    void set_antialiasing(bool arg);
    void set_composition_mode(CompositionMode mode);
    void set_window(const RectF &window);
    void set_viewport(const RectF &viewport);
    void set_font(const Font &font);
    void set_pen(const Pen &pen);
    void set_no_pen();
    void set_brush(const Brush &brush);
    void save();
    void restore();
    void set_transform(const Transform &transform);

    void draw_path(const PainterPath &path);
    void draw_polygon(const PointF *points, size_t point_count, PolygonMode mode);
    void draw_text(const PointF &point, const String &text);
    void draw_text(const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text);
    void draw_symbol(const PointF &point, char32_t ucs4_code);
    void draw_pixmap(const PointF &point, const Pixmap &pm);
    void draw_tiled_pixmap(const RectF &rect, const Pixmap &pm, const PointF &offset);

    bool has_clipping() const;
    void set_clip_rect(const RectF &rect);
    void set_mask(const RectF &background, const std::vector<RectF> &mask_rects);
    void set_clipping(bool enable);

private:
    void ensure_ok(const char *what) const;
    void validate_affine(const Transform &transform) const;
    void apply_transform();
    void update_view_transform();
    void set_source_color(const Color &color);
    double font_size_pixels(const Font &font) const;
    std::shared_ptr<FtFaceEntry> face_for(const Font &font);

    void build_path(const PainterPath &path);
    void apply_pen();
    void apply_brush_source();

    std::vector<cairo_rectangle_t> copy_clip_rects();
    cairo_rectangle_t device_bounds(const RectF &rect) const;
    void reset_to_base_clip();
    void apply_device_clip_rects(const std::vector<cairo_rectangle_t> &rects);
    void clear_user_clip();

    ShapedLine shape_text(const FtFaceEntry &entry, const Font &font, const std::u16string &text);
    double measure_text(const FtFaceEntry &entry, const Font &font, const std::u16string &text);
    std::vector<std::u16string> wrap_paragraph(const FtFaceEntry &entry, const Font &font, const std::u16string &paragraph,
                                               double max_width, bool anywhere);
    std::vector<ShapedLine> layout_lines(const FtFaceEntry &entry, const Font &font, const std::u16string &text, const RectF &rect,
                                         TextFlags text_flags);
    void apply_font_state(const FtFaceEntry &entry, const Font &font, double pixel_size);
    void draw_text_decorations(const FtFaceEntry &entry, const Font &font, double pixel_size, double x, double baseline, double width);
    void draw_shaped_line(const FtFaceEntry &entry, const Font &font, const ShapedLine &line, double x, double baseline, bool justify,
                          double target_width);
    void draw_text_at_point(const Font &font, const PointF &point, const String &text);
    void draw_text_in_rect(const Font &font, const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text);

    cairo_surface_t *pixmap_surface(const Pixmap &pm);
};

// --------------------------------------------------------------- lifecycle

CairoPainter::Impl::Impl(cairo_t *context, const music::RuntimeFontsDatabase &fonts_database, double device_dpi)
    : cr(context), fonts(fonts_database), dpi(device_dpi) {
    if (!cr) {
        throw std::invalid_argument("CairoPainter: cairo_t context must not be null");
    }
    if (!is_finite(dpi) || dpi <= 0.0) {
        throw std::invalid_argument("CairoPainter: device DPI must be a positive finite number");
    }

    const cairo_status_t status = cairo_status(cr);
    if (status != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error(std::string("CairoPainter: cairo context is in error state: ") + cairo_status_to_string(status));
    }

    state.font = Font();
    state.pen = Pen();
    state.brush = Brush(BrushStyle::NO_BRUSH);

    // Adopt a transform the caller applied to the cairo_t before handing it
    // over, mirroring QPainterProvider's initial transform read.
    cairo_matrix_t initial_matrix;
    cairo_get_matrix(cr, &initial_matrix);
    const bool identity_matrix = initial_matrix.xx == 1.0 && initial_matrix.yx == 0.0 && initial_matrix.xy == 0.0 &&
                                 initial_matrix.yy == 1.0 && initial_matrix.x0 == 0.0 && initial_matrix.y0 == 0.0;
    if (!identity_matrix) {
        state.transform =
            Transform(initial_matrix.xx, initial_matrix.yx, initial_matrix.xy, initial_matrix.yy, initial_matrix.x0, initial_matrix.y0);
        state.world_transform = state.transform;
        state.transform_explicit = true;
    }

    try {
        base_clip = copy_clip_rects();
        base_clip_known = true;
    } catch (const std::runtime_error &) {
        // Some vector backends (PDF/SVG) do not expose their clip as a
        // rectangle list.  A fresh cairo_t's only clip is the surface bounds,
        // so treating it as "no extra base clip" is the safe interpretation.
        base_clip.clear();
        base_clip_known = false;
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    ensure_ok("initialize");
}

CairoPainter::Impl::~Impl() {
    for (auto &item : surfaces) {
        if (item.second.surface) {
            cairo_surface_destroy(item.second.surface);
            item.second.surface = nullptr;
        }
    }
}

void CairoPainter::Impl::ensure_ok(const char *what) const {
    const cairo_status_t status = cairo_status(cr);
    if (status != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error(std::string("CairoPainter: ") + what + ": " + cairo_status_to_string(status));
    }
}

bool CairoPainter::Impl::is_active() const { return active; }

void CairoPainter::Impl::begin_target(const std::string &) {}

void CairoPainter::Impl::before_end_target_hook(Painter *) {}

bool CairoPainter::Impl::end_target(bool end_draw) {
    if (!active) {
        return true;
    }
    if (end_draw) {
        active = false;
    }
    return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

void CairoPainter::Impl::begin_object(const std::string &) {}

void CairoPainter::Impl::end_object() {}

// --------------------------------------------------------------- transform

void CairoPainter::Impl::validate_affine(const Transform &transform) const {
    if (!finite_transform(transform)) {
        throw std::runtime_error("CairoPainter: non-finite transform");
    }
    if (std::abs(transform.m13()) > 1e-12 || std::abs(transform.m23()) > 1e-12 || std::abs(transform.m33() - 1.0) > 1e-12) {
        throw std::runtime_error("CairoPainter: projective transforms are not supported by Cairo");
    }
}

void CairoPainter::Impl::apply_transform() {
    validate_affine(state.transform);

    cairo_matrix_t matrix;
    matrix.xx = state.transform.m11();
    matrix.yx = state.transform.m12();
    matrix.xy = state.transform.m21();
    matrix.yy = state.transform.m22();
    matrix.x0 = state.transform.dx();
    matrix.y0 = state.transform.dy();

    cairo_set_matrix(cr, &matrix);
    ensure_ok("setTransform");
}

void CairoPainter::Impl::update_view_transform() {
    const bool valid = finite_rect(state.window) && finite_rect(state.viewport) && state.window.width() > 0.0 &&
                       state.window.height() > 0.0 && state.viewport.width() > 0.0 && state.viewport.height() > 0.0;

    if (!valid) {
        state.has_view = false;
        if (!state.transform_explicit) {
            state.transform = state.world_transform;
            apply_transform();
        }
        return;
    }

    const double scale_x = state.viewport.width() / state.window.width();
    const double scale_y = state.viewport.height() / state.window.height();
    if (!is_finite(scale_x) || !is_finite(scale_y)) {
        state.has_view = false;
        if (!state.transform_explicit) {
            state.transform = state.world_transform;
            apply_transform();
        }
        return;
    }

    const Transform view_transform(scale_x, 0.0, 0.0, scale_y, state.viewport.x() - state.window.x() * scale_x,
                                   state.viewport.y() - state.window.y() * scale_y);
    if (!finite_transform(view_transform)) {
        state.has_view = false;
        if (!state.transform_explicit) {
            state.transform = state.world_transform;
            apply_transform();
        }
        return;
    }

    state.view_transform = view_transform;
    state.has_view = true;

    if (!state.transform_explicit) {
        const Transform effective = state.world_transform * view_transform;
        if (!finite_transform(effective)) {
            throw std::runtime_error("CairoPainter: window/viewport composition produced non-finite values");
        }
        state.transform = effective;
        apply_transform();
    }
}

void CairoPainter::Impl::set_transform(const Transform &transform) {
    if (!active) {
        return;
    }

    // muse::draw::Painter temporarily feeds a viewport transform computed with
    // a zero-sized window; that transform is singular/NaN.  Ignore it rather
    // than poisoning the cairo context.  The real transform follows immediately.
    if (!finite_transform(transform)) {
        return;
    }
    validate_affine(transform);

    if (!state.transform_explicit && state.has_view) {
        // Two legitimate callers exist:
        //  * muse::draw::Painter passes world * viewport, and has already
        //    written its own window/viewport into our state as well;
        //  * a direct caller passes only the world transform after setting
        //    window/viewport.
        // If the incoming matrix already equals the composition we would
        // produce, accept it as complete.  Otherwise treat it as the world
        // transform and compose it with the current window/viewport.
        const Transform composed = state.world_transform * state.view_transform;
        if (transform_nearly_equal(transform, composed) || transform_nearly_equal(transform, state.transform)) {
            state.world_transform = transform;
            state.transform = transform;
            state.transform_explicit = true;
            apply_transform();
            return;
        }

        const Transform effective = transform * state.view_transform;
        if (!finite_transform(effective)) {
            throw std::runtime_error("CairoPainter: transform composition produced non-finite values");
        }
        state.world_transform = transform;
        state.transform = effective;
        apply_transform();
        return;
    }

    // Once an explicit transform has been accepted as the complete matrix, it
    // stays authoritative.  This matches QPainterProvider and prevents double
    // composition when Painter subsequently calls setWindow()/setViewport().
    state.world_transform = transform;
    state.transform = transform;
    state.transform_explicit = true;
    apply_transform();
}

void CairoPainter::Impl::set_window(const RectF &window) {
    if (!active) {
        return;
    }
    state.window = window;
    update_view_transform();
}

void CairoPainter::Impl::set_viewport(const RectF &viewport) {
    if (!active) {
        return;
    }
    state.viewport = viewport;
    update_view_transform();
}

// --------------------------------------------------------- paint state

void CairoPainter::Impl::set_antialiasing(bool arg) {
    if (!active) {
        return;
    }
    state.antialiasing = arg;
    cairo_set_antialias(cr, arg ? CAIRO_ANTIALIAS_GRAY : CAIRO_ANTIALIAS_NONE);
    ensure_ok("setAntialiasing");
}

void CairoPainter::Impl::set_composition_mode(CompositionMode mode) {
    if (!active) {
        return;
    }

    cairo_operator_t op;
    switch (mode) {
        case CompositionMode::SOURCE_OVER: op = CAIRO_OPERATOR_OVER; break;
        case CompositionMode::HARD_LIGHT: op = CAIRO_OPERATOR_HARD_LIGHT; break;
        default: throw std::runtime_error("CairoPainter: unsupported composition mode " + std::to_string(static_cast<int>(mode)));
    }

    state.composition = mode;
    cairo_set_operator(cr, op);
    ensure_ok("setCompositionMode");
}

void CairoPainter::Impl::set_font(const Font &font) {
    if (!active) {
        return;
    }
    state.font = font;
}

void CairoPainter::Impl::set_pen(const Pen &pen) {
    if (!active) {
        return;
    }
    state.pen = pen;
}

void CairoPainter::Impl::set_no_pen() {
    if (!active) {
        return;
    }
    state.pen = Pen(PenStyle::NO_PEN);
}

void CairoPainter::Impl::set_brush(const Brush &brush) {
    if (!active) {
        return;
    }
    state.brush = brush;
}

void CairoPainter::Impl::save() {
    if (!active) {
        return;
    }
    stack.push_back(state);
    cairo_save(cr);
    ensure_ok("save");
}

void CairoPainter::Impl::restore() {
    if (!active || stack.empty()) {
        return;
    }
    cairo_restore(cr);
    ensure_ok("restore");
    state = std::move(stack.back());
    stack.pop_back();
}

void CairoPainter::Impl::set_source_color(const Color &color) {
    const Color c = color.is_valid() ? color : Color::black;
    const double alpha = std::clamp(c.alpha(), 0, 255) / 255.0;
    const double red = std::clamp(c.red(), 0, 255) / 255.0;
    const double green = std::clamp(c.green(), 0, 255) / 255.0;
    const double blue = std::clamp(c.blue(), 0, 255) / 255.0;
    cairo_set_source_rgba(cr, red, green, blue, alpha);
}

double CairoPainter::Impl::font_size_pixels(const Font &font) const {
    if (font.pixel_size() > 0) {
        return double(font.pixel_size());
    }
    if (font.point_size_f() > 0.0) {
        const double pixel_size = font.point_size_f() * dpi / 72.0;
        if (is_finite(pixel_size) && pixel_size > 0.0) {
            return pixel_size;
        }
    }
    throw std::runtime_error("CairoPainter: font has neither a point size nor a pixel size");
}

std::shared_ptr<FtFaceEntry> CairoPainter::Impl::face_for(const Font &font) { return font_cache().get(font, fonts); }

void CairoPainter::Impl::apply_pen() {
    if (state.pen.style() == PenStyle::NO_PEN) {
        return;
    }

    set_source_color(state.pen.color());

    double width = state.pen.width_f();
    if (!is_finite(width) || width < 0.0) {
        width = 0.0;
    }
    // Cairo interprets a zero line width as a one-device-pixel hairline, which
    // matches Qt's cosmetic pen behaviour.
    cairo_set_line_width(cr, width);

    switch (state.pen.cap_style()) {
        case PenCapStyle::FLAT_CAP: cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT); break;
        case PenCapStyle::SQUARE_CAP: cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE); break;
        case PenCapStyle::ROUND_CAP: cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); break;
        default: throw std::runtime_error("CairoPainter: unsupported pen cap style");
    }

    switch (state.pen.join_style()) {
        case PenJoinStyle::MITER_JOIN: cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER); break;
        case PenJoinStyle::BEVEL_JOIN: cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL); break;
        case PenJoinStyle::ROUND_JOIN: cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND); break;
        default: throw std::runtime_error("CairoPainter: unsupported pen join style");
    }
    // QPen's default miter limit is 2.0; Cairo's default is 10.0.
    cairo_set_miter_limit(cr, 2.0);

    switch (state.pen.style()) {
        case PenStyle::NO_PEN: break;
        case PenStyle::SOLID_LINE: cairo_set_dash(cr, nullptr, 0, 0.0); break;
        case PenStyle::DASH_LINE:
        case PenStyle::DOT_LINE:
        case PenStyle::DASH_DOT_LINE:
        case PenStyle::DASH_DOT_DOT_LINE:
        case PenStyle::CUSTOM_DASH_LINE: {
            const std::vector<double> pattern = state.pen.dash_pattern();
            if (pattern.empty()) {
                cairo_set_dash(cr, nullptr, 0, 0.0);
                break;
            }

            // QPen dash patterns are expressed in multiples of the pen width.
            // Cairo expects user-space lengths.  A zero width is a cosmetic pen:
            // its pattern is effectively in device pixels, so convert through the
            // current CTM scale.
            double unit = width;
            if (!(unit > 0.0)) {
                cairo_matrix_t ctm;
                cairo_get_matrix(cr, &ctm);
                const double scale_x = std::hypot(ctm.xx, ctm.yx);
                const double scale_y = std::hypot(ctm.xy, ctm.yy);
                const double scale = (scale_x + scale_y) / 2.0;
                unit = (is_finite(scale) && scale > 0.0) ? 1.0 / scale : 1.0;
            }
            std::vector<double> dashes;
            dashes.reserve(pattern.size());
            bool all_zero = true;
            for (double value : pattern) {
                double dash = value * unit;
                if (!is_finite(dash) || dash < 0.0) {
                    dash = 0.0;
                }
                if (dash > 0.0) {
                    all_zero = false;
                }
                dashes.push_back(dash);
            }

            if (all_zero || dashes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
                cairo_set_dash(cr, nullptr, 0, 0.0);
            } else {
                cairo_set_dash(cr, dashes.data(), static_cast<int>(dashes.size()), 0.0);
            }
        } break;
        default: throw std::runtime_error("CairoPainter: unsupported pen style " + std::to_string(static_cast<int>(state.pen.style())));
    }

    ensure_ok("pen");
}

void CairoPainter::Impl::apply_brush_source() {
    switch (state.brush.style()) {
        case BrushStyle::NO_BRUSH: return;
        case BrushStyle::SOLID_PATTERN: set_source_color(state.brush.color()); return;
        default: throw std::runtime_error("CairoPainter: unsupported brush style " + std::to_string(static_cast<int>(state.brush.style())));
    }
}

// --------------------------------------------------------------- clipping

std::vector<cairo_rectangle_t> CairoPainter::Impl::copy_clip_rects() {
    std::unique_ptr<cairo_rectangle_list_t, decltype(&cairo_rectangle_list_destroy)> list(cairo_copy_clip_rectangle_list(cr),
                                                                                          cairo_rectangle_list_destroy);
    if (!list) {
        throw std::runtime_error("CairoPainter: cairo_copy_clip_rectangle_list failed");
    }

    if (list->status != CAIRO_STATUS_SUCCESS) {
        const std::string message = cairo_status_to_string(list->status);

        throw std::runtime_error("CairoPainter: clip region is not representable as rectangles: " + message);
    }
    if (list->num_rectangles > 0 && !list->rectangles) {

        throw std::runtime_error("CairoPainter: clip rectangle list is missing its rectangles");
    }

    // cairo_copy_clip_rectangle_list() reports rectangles in the *current user
    // space*.  We cache them for re-application under an identity matrix, so
    // convert them to device space with the CTM that was active when the clip
    // was created.
    cairo_matrix_t ctm;
    cairo_get_matrix(cr, &ctm);

    std::vector<cairo_rectangle_t> rects;
    rects.reserve(list->num_rectangles > 0 ? static_cast<size_t>(list->num_rectangles) : 0u);
    for (int i = 0; i < list->num_rectangles; ++i) {
        const cairo_rectangle_t &user_rect = list->rectangles[i];
        double min_x = std::numeric_limits<double>::max();
        double min_y = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double max_y = std::numeric_limits<double>::lowest();

        const double xs[2] = {user_rect.x, user_rect.x + user_rect.width};
        const double ys[2] = {user_rect.y, user_rect.y + user_rect.height};
        for (double x : xs) {
            for (double y : ys) {
                double px = x;
                double py = y;
                cairo_matrix_transform_point(&ctm, &px, &py);
                if (!is_finite(px) || !is_finite(py)) {

                    throw std::runtime_error("CairoPainter: clip rectangle maps to non-finite device coordinates");
                }
                min_x = std::min(min_x, px);
                min_y = std::min(min_y, py);
                max_x = std::max(max_x, px);
                max_y = std::max(max_y, py);
            }
        }

        cairo_rectangle_t device_rect;
        device_rect.x = min_x;
        device_rect.y = min_y;
        device_rect.width = max_x - min_x;
        device_rect.height = max_y - min_y;
        rects.push_back(device_rect);
    }

    return rects;
}

cairo_rectangle_t CairoPainter::Impl::device_bounds(const RectF &rect) const {
    cairo_matrix_t ctm;
    cairo_get_matrix(cr, &ctm);

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();

    const double xs[2] = {rect.left(), rect.right()};
    const double ys[2] = {rect.top(), rect.bottom()};
    for (double x : xs) {
        for (double y : ys) {
            double px = x;
            double py = y;
            cairo_matrix_transform_point(&ctm, &px, &py);
            if (!is_finite(px) || !is_finite(py)) {
                throw std::runtime_error("CairoPainter: clip rectangle maps to non-finite device coordinates");
            }
            min_x = std::min(min_x, px);
            min_y = std::min(min_y, py);
            max_x = std::max(max_x, px);
            max_y = std::max(max_y, py);
        }
    }

    cairo_rectangle_t out;
    out.x = min_x;
    out.y = min_y;
    out.width = max_x - min_x;
    out.height = max_y - min_y;
    return out;
}

void CairoPainter::Impl::reset_to_base_clip() {
    cairo_matrix_t saved;
    cairo_get_matrix(cr, &saved);

    cairo_matrix_t identity;
    cairo_matrix_init_identity(&identity);
    cairo_set_matrix(cr, &identity);
    cairo_reset_clip(cr);
    cairo_new_path(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);

    if (base_clip_known) {
        if (base_clip.empty()) {
            // A degenerate rectangle produces an empty clip region: nothing draws.
            cairo_rectangle(cr, 0.0, 0.0, 0.0, 0.0);
        } else {
            for (const cairo_rectangle_t &rect : base_clip) {
                cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
            }
        }
        cairo_clip(cr);
    }
    // If the backend did not expose its clip, leave the default surface clip.

    cairo_set_matrix(cr, &saved);
    ensure_ok("reset clip");
}

void CairoPainter::Impl::apply_device_clip_rects(const std::vector<cairo_rectangle_t> &rects) {
    cairo_matrix_t saved;
    cairo_get_matrix(cr, &saved);

    cairo_identity_matrix(cr);
    cairo_new_path(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    if (rects.empty()) {
        cairo_rectangle(cr, 0.0, 0.0, 0.0, 0.0);
    } else {
        for (const cairo_rectangle_t &rect : rects) {
            cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
        }
    }
    cairo_clip(cr);
    cairo_set_matrix(cr, &saved);
    ensure_ok("apply clip");
}

void CairoPainter::Impl::clear_user_clip() {
    reset_to_base_clip();
    state.has_user_clip = false;
    state.user_clip_rects.clear();
}

void CairoPainter::Impl::set_clip_rect(const RectF &rect) {
    if (!active) {
        return;
    }

    const RectF r = rect.normalized();
    if (!finite_rect(r) || !r.is_valid()) {
        clear_user_clip();
        return;
    }

    // Compute the cached device-space bounds before mutating the clip so a
    // failure leaves the context unchanged.
    const cairo_rectangle_t device_rect = device_bounds(r);
    const bool clipping_enabled = state.clipping_enabled;

    reset_to_base_clip();
    cairo_new_path(cr);
    cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);

    if (!clipping_enabled) {
        reset_to_base_clip();
    }
    ensure_ok("setClipRect");

    // The clip currently applied by Cairo is the exact transformed path; this
    // cached form is exact for axis-aligned transforms and a bounding box
    // otherwise, and is used when setClipping() is re-enabled.
    state.user_clip_rects.clear();
    state.user_clip_rects.push_back(device_rect);
    state.has_user_clip = true;
}

void CairoPainter::Impl::set_mask(const RectF &background, const std::vector<RectF> &mask_rects) {
    if (!active) {
        return;
    }

    if (mask_rects.empty()) {
        clear_user_clip();
        return;
    }

    const RectF bg = background.normalized();
    if (!finite_rect(bg) || !bg.is_valid()) {
        clear_user_clip();
        return;
    }

    const cairo_rectangle_t background_device = device_bounds(bg);
    std::vector<cairo_rectangle_t> mask_devices;
    mask_devices.reserve(mask_rects.size());
    for (const RectF &raw_rect : mask_rects) {
        const RectF rect = raw_rect.normalized();
        if (!finite_rect(rect) || !rect.is_valid()) {
            continue;
        }
        const cairo_rectangle_t mask_device = device_bounds(rect);
        if (mask_device.width > 0.0 && mask_device.height > 0.0) {
            mask_devices.push_back(mask_device);
        }
    }

    std::vector<cairo_rectangle_t> clip_rects = subtract_rects(background_device, mask_devices);
    const bool clipping_enabled = state.clipping_enabled;

    reset_to_base_clip();
    apply_device_clip_rects(clip_rects);
    if (!clipping_enabled) {
        reset_to_base_clip();
    }
    ensure_ok("setMask");

    state.user_clip_rects = std::move(clip_rects);
    state.has_user_clip = true;
}

void CairoPainter::Impl::set_clipping(bool enable) {
    if (!active) {
        return;
    }
    if (state.clipping_enabled == enable) {
        return;
    }

    if (enable && state.has_user_clip) {
        reset_to_base_clip();
        apply_device_clip_rects(state.user_clip_rects);
    } else {
        reset_to_base_clip();
    }
    ensure_ok("setClipping");
    state.clipping_enabled = enable;
}

bool CairoPainter::Impl::has_clipping() const { return state.clipping_enabled && state.has_user_clip; }

// ------------------------------------------------------------------- paths

void CairoPainter::Impl::build_path(const PainterPath &path) {
    bool have_current = false;
    bool have_ctrl1 = false;
    bool have_ctrl2 = false;
    PointF ctrl1;
    PointF ctrl2;

    for (size_t i = 0; i < path.element_count(); ++i) {
        const PainterPath::Element e = path.element_at(i);
        switch (e.type) {
            case PainterPath::ElementType::MOVE_TO_ELEMENT:
                cairo_move_to(cr, e.x, e.y);
                have_current = true;
                have_ctrl1 = false;
                have_ctrl2 = false;
                break;
            case PainterPath::ElementType::LINE_TO_ELEMENT:
                if (!have_current) {
                    cairo_move_to(cr, e.x, e.y);
                    have_current = true;
                } else {
                    cairo_line_to(cr, e.x, e.y);
                }
                have_ctrl1 = false;
                have_ctrl2 = false;
                break;
            case PainterPath::ElementType::CURVE_TO_ELEMENT:
                ctrl1 = PointF(e.x, e.y);
                have_ctrl1 = true;
                have_ctrl2 = false;
                break;
            case PainterPath::ElementType::CURVE_TO_DATA_ELEMENT:
                if (!have_ctrl1) {
                    if (!have_current) {
                        cairo_move_to(cr, e.x, e.y);
                        have_current = true;
                    } else {
                        cairo_line_to(cr, e.x, e.y);
                    }
                } else if (!have_ctrl2) {
                    ctrl2 = PointF(e.x, e.y);
                    have_ctrl2 = true;
                } else {
                    if (!have_current) {
                        cairo_move_to(cr, ctrl1.x(), ctrl1.y());
                        have_current = true;
                    }
                    cairo_curve_to(cr, ctrl1.x(), ctrl1.y(), ctrl2.x(), ctrl2.y(), e.x, e.y);
                    have_ctrl1 = false;
                    have_ctrl2 = false;
                }
                break;
        }
    }
}

void CairoPainter::Impl::draw_path(const PainterPath &path) {
    if (!active) {
        return;
    }
    ensure_ok("drawPath");
    if (path.is_empty()) {
        return;
    }

    const bool has_brush = state.brush.style() != BrushStyle::NO_BRUSH;
    const bool has_pen = state.pen.style() != PenStyle::NO_PEN;
    if (!has_brush && !has_pen) {
        return;
    }

    cairo_new_path(cr);
    build_path(path);

    if (has_brush) {
        cairo_set_fill_rule(cr,
                            path.fill_rule() == PainterPath::FillRule::WINDING_FILL ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
        apply_brush_source();
        if (has_pen) {
            cairo_fill_preserve(cr);
        } else {
            cairo_fill(cr);
        }
    }

    if (has_pen) {
        apply_pen();
        cairo_stroke(cr);
    }
    ensure_ok("drawPath");
}

void CairoPainter::Impl::draw_polygon(const PointF *points, size_t point_count, PolygonMode mode) {
    if (!active) {
        return;
    }
    ensure_ok("drawPolygon");
    if (!points || point_count == 0) {
        return;
    }

    bool fill = true;
    cairo_fill_rule_t fill_rule = CAIRO_FILL_RULE_EVEN_ODD;
    switch (mode) {
        case PolygonMode::ODD_EVEN:
            fill = true;
            fill_rule = CAIRO_FILL_RULE_EVEN_ODD;
            break;
        case PolygonMode::WINDING:
            fill = true;
            fill_rule = CAIRO_FILL_RULE_WINDING;
            break;
        case PolygonMode::CONVEX:
            fill = true;
            fill_rule = CAIRO_FILL_RULE_WINDING;
            break;
        case PolygonMode::POLYLINE: fill = false; break;
        default: throw std::runtime_error("CairoPainter: unsupported polygon mode " + std::to_string(static_cast<int>(mode)));
    }

    const bool has_brush = fill && state.brush.style() != BrushStyle::NO_BRUSH;
    const bool has_pen = state.pen.style() != PenStyle::NO_PEN;
    if (!has_brush && !has_pen) {
        return;
    }

    cairo_new_path(cr);
    cairo_move_to(cr, points[0].x(), points[0].y());
    for (size_t i = 1; i < point_count; ++i) {
        cairo_line_to(cr, points[i].x(), points[i].y());
    }
    if (fill) {
        cairo_close_path(cr);
    }

    if (has_brush) {
        cairo_set_fill_rule(cr, fill_rule);
        apply_brush_source();
        if (has_pen) {
            cairo_fill_preserve(cr);
        } else {
            cairo_fill(cr);
        }
    }
    if (has_pen) {
        apply_pen();
        cairo_stroke(cr);
    }
    ensure_ok("drawPolygon");
}

// -------------------------------------------------------------------- text

ShapedLine CairoPainter::Impl::shape_text(const FtFaceEntry &entry, const Font &font, const std::u16string &text) {
    ShapedLine line;
    line.text = text;
    if (text.empty()) {
        return line;
    }
    if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("CairoPainter: text is too long");
    }

    const double pixel_size = font_size_pixels(font);
    const double scale = pixel_size / double(entry.hb_upem ? entry.hb_upem : entry.ft_upem);

    hb_font_t *hb_font = hb_font_create(entry.hb);
    if (!hb_font) {
        throw std::runtime_error("CairoPainter: hb_font_create failed");
    }
    hb_font_set_scale(hb_font, static_cast<int>(entry.hb_upem), static_cast<int>(entry.hb_upem));

    hb_buffer_t *buffer = hb_buffer_create();
    if (!buffer) {
        hb_font_destroy(hb_font);
        throw std::runtime_error("CairoPainter: hb_buffer_create failed");
    }

    hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16_t *>(text.data()), static_cast<int>(text.size()), 0,
                        static_cast<int>(text.size()));
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(hb_font, buffer, nullptr, 0);

    unsigned int count = 0;
    const hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &count);
    const hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, nullptr);
    if (count > 0 && (!infos || !positions)) {
        hb_buffer_destroy(buffer);
        hb_font_destroy(hb_font);
        throw std::runtime_error("CairoPainter: HarfBuzz returned an incomplete glyph buffer");
    }

    line.glyphs.reserve(count);
    line.clusters.reserve(count);

    double pen_x = 0.0;
    double pen_y = 0.0;
    for (unsigned int i = 0; i < count; ++i) {
        const double x_offset_value = double(positions[i].x_offset) * scale;
        const double y_offset_value = double(positions[i].y_offset) * scale;

        cairo_glyph_t glyph;
        glyph.index = static_cast<unsigned long>(infos[i].codepoint);
        glyph.x = pen_x + x_offset_value;
        glyph.y = pen_y - y_offset_value;
        line.glyphs.push_back(glyph);
        line.clusters.push_back(static_cast<unsigned int>(infos[i].cluster));

        pen_x += double(positions[i].x_advance) * scale;
        // HarfBuzz y advances point up; Cairo user space points down.
        pen_y -= double(positions[i].y_advance) * scale;
    }

    if (pen_x < 0.0) {
        // RTL shaping can produce a negative pen position.  Keep the line
        // origin at zero and draw the visual run to the right of it.
        for (cairo_glyph_t &glyph : line.glyphs) {
            glyph.x -= pen_x;
        }
        line.width = -pen_x;
    } else {
        line.width = pen_x;
    }

    hb_buffer_destroy(buffer);
    hb_font_destroy(hb_font);
    return line;
}

double CairoPainter::Impl::measure_text(const FtFaceEntry &entry, const Font &font, const std::u16string &text) {
    return shape_text(entry, font, text).width;
}

std::vector<std::u16string> CairoPainter::Impl::wrap_paragraph(const FtFaceEntry &entry, const Font &font, const std::u16string &paragraph,
                                                               double max_width, bool anywhere) {
    std::vector<std::u16string> lines;
    if (paragraph.empty()) {
        lines.push_back(paragraph);
        return lines;
    }

    size_t start = 0;
    while (start < paragraph.size()) {
        while (start < paragraph.size() && is_space_char(paragraph[start])) {
            ++start;
        }
        if (start >= paragraph.size()) {
            break;
        }

        size_t end = start;
        size_t last_space = std::u16string::npos;
        while (end < paragraph.size()) {
            if (is_space_char(paragraph[end])) {
                last_space = end;
            }
            ++end;

            const double width = measure_text(entry, font, paragraph.substr(start, end - start));
            if (width > max_width) {
                if (end > start + 1) {
                    --end;
                }
                break;
            }
        }

        if (end >= paragraph.size()) {
            lines.push_back(paragraph.substr(start));
            break;
        }

        size_t break_at = end;
        if (!anywhere && last_space != std::u16string::npos && last_space > start) {
            break_at = last_space;
        }
        if (break_at <= start) {
            break_at = start + 1;
        }

        lines.push_back(paragraph.substr(start, break_at - start));
        start = break_at;
        while (start < paragraph.size() && is_space_char(paragraph[start])) {
            ++start;
        }
    }

    if (lines.empty()) {
        lines.push_back(std::u16string());
    }
    return lines;
}

std::vector<ShapedLine> CairoPainter::Impl::layout_lines(const FtFaceEntry &entry, const Font &font, const std::u16string &text,
                                                         const RectF &rect, TextFlags text_flags) {
    bool wrap = text_flags.test_flag(TEXT_WORD_WRAP) || text_flags.test_flag(TEXT_WRAP_ANYWHERE);
    const bool anywhere = text_flags.test_flag(TEXT_WRAP_ANYWHERE);
    const double max_width = rect.width();

    std::vector<std::u16string> paragraphs = split_paragraphs(text);
    if (text_flags.test_flag(TEXT_SINGLE_LINE)) {
        paragraphs = {join_with_spaces(paragraphs)};
        wrap = false;
    }

    std::vector<ShapedLine> lines;
    for (const std::u16string &paragraph : paragraphs) {
        if (wrap && max_width > 0.0) {
            const std::vector<std::u16string> wrapped = wrap_paragraph(entry, font, paragraph, max_width, anywhere);
            for (const std::u16string &line : wrapped) {
                lines.push_back(shape_text(entry, font, line));
            }
        } else {
            lines.push_back(shape_text(entry, font, paragraph));
        }
    }
    return lines;
}

void CairoPainter::Impl::apply_font_state(const FtFaceEntry &entry, const Font &font, double pixel_size) {
    const bool face_bold = (entry.ft->style_flags & FT_STYLE_FLAG_BOLD) != 0;
    const bool face_italic = (entry.ft->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
    const bool synth_bold = font.bold() && !entry.key.bold() && !face_bold;
    const bool synth_italic = font.italic() && !entry.key.italic() && !face_italic;

    cairo_set_font_face(cr, entry.cairo_face(synth_bold, synth_italic));
    cairo_set_font_size(cr, pixel_size);

    cairo_font_options_t *options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, state.antialiasing ? CAIRO_ANTIALIAS_GRAY : CAIRO_ANTIALIAS_NONE);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
    cairo_set_font_options(cr, options);
    cairo_font_options_destroy(options);

    ensure_ok("font");
}

void CairoPainter::Impl::draw_text_decorations(const FtFaceEntry &entry, const Font &font, double pixel_size, double x, double baseline,
                                               double width) {
    if (!font.underline() && !font.strike()) {
        return;
    }
    if (!(width > 0.0)) {
        return;
    }

    const double scale = pixel_size / double(entry.ft_upem);
    const double ascent = ascent_for(entry, pixel_size);
    double thickness = double(entry.underline_thickness_value) * scale;
    if (!(thickness > 0.0)) {
        thickness = std::max(1.0, pixel_size * 0.05);
    }

    CairoStateGuard guard(cr);
    set_source_color(state.pen.color());
    cairo_set_dash(cr, nullptr, 0, 0.0);
    cairo_set_line_width(cr, thickness);
    cairo_new_path(cr);

    if (font.underline()) {
        const double y = baseline - double(entry.underline_position_value) * scale;
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, x + width, y);
        cairo_stroke(cr);
    }

    if (font.strike()) {
        const double y = baseline - 0.3 * ascent;
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, x + width, y);
        cairo_stroke(cr);
    }

    ensure_ok("text decoration");
}

void CairoPainter::Impl::draw_shaped_line(const FtFaceEntry &entry, const Font &font, const ShapedLine &line, double x, double baseline,
                                          bool justify, double target_width) {
    if (line.glyphs.empty()) {
        return;
    }
    if (line.glyphs.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("CairoPainter: glyph run is too long");
    }

    const double pixel_size = font_size_pixels(font);
    apply_font_state(entry, font, pixel_size);
    set_source_color(state.pen.color());

    std::vector<char> is_space(line.glyphs.size(), 0);
    size_t space_count = 0;
    for (size_t i = 0; i < line.glyphs.size(); ++i) {
        const size_t cluster = line.clusters[i];
        if (cluster < line.text.size() && is_space_char(line.text[cluster])) {
            is_space[i] = 1;
            ++space_count;
        }
    }

    double extra = 0.0;
    if (justify && target_width > line.width && space_count > 0) {
        extra = (target_width - line.width) / double(space_count);
    }

    std::vector<cairo_glyph_t> glyphs = line.glyphs;
    double shift = 0.0;
    for (size_t i = 0; i < glyphs.size(); ++i) {
        glyphs[i].x += x + shift;
        glyphs[i].y += baseline;
        if (is_space[i]) {
            shift += extra;
        }
    }

    cairo_show_glyphs(cr, glyphs.data(), static_cast<int>(glyphs.size()));
    ensure_ok("drawText");

    const double drawn_width = line.width + extra * double(space_count);
    draw_text_decorations(entry, font, pixel_size, x, baseline, drawn_width);
}

void CairoPainter::Impl::draw_text_at_point(const Font &font, const PointF &point, const String &text) {
    const std::shared_ptr<FtFaceEntry> entry = face_for(font);
    const double pixel_size = font_size_pixels(font);
    const double line_height = line_height_for(*entry, pixel_size);

    const std::vector<std::u16string> paragraphs = split_paragraphs(normalize_text(text.to_std_u16_string(), true));

    double baseline = point.y();
    for (const std::u16string &paragraph : paragraphs) {
        const ShapedLine line = shape_text(*entry, font, paragraph);
        draw_shaped_line(*entry, font, line, point.x(), baseline, false, 0.0);
        baseline += line_height;
    }
}

void CairoPainter::Impl::draw_text_in_rect(const Font &font, const RectF &rect, Alignment alignment, TextFlags text_flags,
                                           const String &text) {
    const std::shared_ptr<FtFaceEntry> entry = face_for(font);
    const double pixel_size = font_size_pixels(font);
    const std::u16string prepared = normalize_text(text.to_std_u16_string(), true);
    std::vector<ShapedLine> lines = layout_lines(*entry, font, prepared, rect, text_flags);
    if (lines.empty()) {
        return;
    }

    const double ascent = ascent_for(*entry, pixel_size);
    const double descent = descent_for(*entry, pixel_size);
    const double line_height = line_height_for(*entry, pixel_size);
    const int line_count = static_cast<int>(lines.size());

    double first_baseline;
    if (alignment.test_flag(ALIGN_TOP)) {
        first_baseline = rect.y() + ascent;
    } else if (alignment.test_flag(ALIGN_BOTTOM)) {
        first_baseline = rect.bottom() - descent - double(line_count - 1) * line_height;
    } else {
        const double block_height = double(line_count - 1) * line_height + ascent + descent;
        first_baseline = rect.y() + (rect.height() - block_height) / 2.0 + ascent;
    }

    std::unique_ptr<CairoStateGuard> clip_guard;
    if (!text_flags.test_flag(TEXT_DONT_CLIP) && rect.is_valid()) {
        clip_guard = std::make_unique<CairoStateGuard>(cr);
        cairo_new_path(cr);
        cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
        cairo_clip(cr);
    }

    for (int i = 0; i < line_count; ++i) {
        const ShapedLine &line = lines[static_cast<size_t>(i)];

        double x = rect.x();
        if (alignment.test_flag(ALIGN_RIGHT)) {
            x = rect.right() - line.width;
        } else if (alignment.test_flag(ALIGN_H_CENTER)) {
            x = rect.x() + (rect.width() - line.width) / 2.0;
        }

        const bool justify = alignment.test_flag(ALIGN_JUSTIFY) && (i + 1) < line_count &&
                             (text_flags.test_flag(TEXT_WORD_WRAP) || text_flags.test_flag(TEXT_WRAP_ANYWHERE));

        draw_shaped_line(*entry, font, line, x, first_baseline + double(i) * line_height, justify, rect.width());
    }
}

void CairoPainter::Impl::draw_text(const PointF &point, const String &text) {
    if (!active) {
        return;
    }
    ensure_ok("drawText");
    if (text.empty()) {
        return;
    }
    if (!finite_point(point)) {
        throw std::invalid_argument("CairoPainter: drawText point is not finite");
    }

    draw_text_at_point(state.font, point, text);
    ensure_ok("drawText");
}

void CairoPainter::Impl::draw_text(const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text) {
    if (!active) {
        return;
    }
    ensure_ok("drawText");
    if (text.empty()) {
        return;
    }

    const RectF r = rect.normalized();
    if (!finite_rect(r)) {
        throw std::invalid_argument("CairoPainter: drawText rectangle is not finite");
    }
    if (!r.is_valid()) {
        return;
    }

    draw_text_in_rect(state.font, r, alignment, text_flags, text);
    ensure_ok("drawText");
}

void CairoPainter::Impl::draw_symbol(const PointF &point, char32_t ucs4_code) {
    if (!active) {
        return;
    }
    draw_text(point, String::from_ucs4(ucs4_code));
}

// ----------------------------------------------------------------- pixmaps

cairo_surface_t *CairoPainter::Impl::pixmap_surface(const Pixmap &pm) {
    if (pm.is_null()) {
        throw std::runtime_error("CairoPainter: cannot draw an empty Pixmap");
    }

    const ByteArray data = pm.data();
    const uint64_t hash = fnv1a(data.const_data(), data.size());
    const unsigned int key = pm.key();

    auto existing = surfaces.find(key);
    if (existing != surfaces.end()) {
        if (existing->second.size == data.size() && existing->second.hash == hash) {
            return existing->second.surface;
        }
        if (existing->second.surface) {
            cairo_surface_destroy(existing->second.surface);
        }
        surfaces.erase(existing);
    }

#if CAIRO_HAS_PNG_FUNCTIONS
    PngReadClosure closure;
    closure.data = data.const_data();
    closure.size = data.size();
    closure.pos = 0;

    cairo_surface_t *surface = cairo_image_surface_create_from_png_stream(&read_png_stream, &closure);
    if (!surface) {
        throw std::runtime_error("CairoPainter: cannot allocate a Cairo image surface for Pixmap");
    }
    const cairo_status_t status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        const std::string message = cairo_status_to_string(status);
        cairo_surface_destroy(surface);
        throw std::runtime_error("CairoPainter: cannot decode PNG Pixmap: " + message);
    }

    if (cairo_image_surface_get_width(surface) <= 0 || cairo_image_surface_get_height(surface) <= 0) {
        cairo_surface_destroy(surface);
        throw std::runtime_error("CairoPainter: decoded PNG has an empty image surface");
    }

    CachedSurface cached;
    cached.size = data.size();
    cached.hash = hash;
    cached.surface = surface;
    surfaces[key] = cached;
    return surface;
#else
    throw std::runtime_error("CairoPainter: this Cairo build has no PNG decoder; Pixmap drawing is unsupported");
#endif
}

void CairoPainter::Impl::draw_pixmap(const PointF &point, const Pixmap &pm) {
    if (!active) {
        return;
    }
    ensure_ok("drawPixmap");
    if (!finite_point(point)) {
        throw std::invalid_argument("CairoPainter: drawPixmap point is not finite");
    }

    cairo_surface_t *surface = pixmap_surface(pm);
    const double surface_width = double(cairo_image_surface_get_width(surface));
    const double surface_height = double(cairo_image_surface_get_height(surface));
    if (!(surface_width > 0.0) || !(surface_height > 0.0)) {
        throw std::runtime_error("CairoPainter: invalid Pixmap image surface size");
    }

    const double draw_width = pm.width() > 0 ? double(pm.width()) : surface_width;
    const double draw_height = pm.height() > 0 ? double(pm.height()) : surface_height;
    const double scale_x = draw_width / surface_width;
    const double scale_y = draw_height / surface_height;

    CairoStateGuard guard(cr);
    cairo_translate(cr, point.x(), point.y());
    cairo_scale(cr, scale_x, scale_y);

    cairo_pattern_t *pattern = cairo_pattern_create_for_surface(surface);
    if (!pattern) {
        throw std::runtime_error("CairoPainter: cannot allocate a Cairo source pattern for Pixmap");
    }
    if (cairo_pattern_status(pattern) != CAIRO_STATUS_SUCCESS) {
        const std::string message = cairo_status_to_string(cairo_pattern_status(pattern));
        cairo_pattern_destroy(pattern);
        throw std::runtime_error("CairoPainter: cannot create Pixmap source pattern: " + message);
    }
    cairo_pattern_set_filter(pattern, CAIRO_FILTER_GOOD);
    cairo_set_source(cr, pattern);
    cairo_pattern_destroy(pattern);

    cairo_new_path(cr);
    cairo_rectangle(cr, 0.0, 0.0, draw_width, draw_height);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_fill(cr);
    ensure_ok("drawPixmap");
}

void CairoPainter::Impl::draw_tiled_pixmap(const RectF &rect, const Pixmap &pm, const PointF &offset) {
    if (!active) {
        return;
    }
    ensure_ok("drawTiledPixmap");
    if (!finite_rect(rect) || !finite_point(offset)) {
        throw std::invalid_argument("CairoPainter: drawTiledPixmap arguments are not finite");
    }

    const RectF r = rect.normalized();
    if (!r.is_valid()) {
        return;
    }

    cairo_surface_t *surface = pixmap_surface(pm);
    const double surface_width = double(cairo_image_surface_get_width(surface));
    const double surface_height = double(cairo_image_surface_get_height(surface));
    if (!(surface_width > 0.0) || !(surface_height > 0.0)) {
        throw std::runtime_error("CairoPainter: invalid Pixmap image surface size");
    }

    const double draw_width = pm.width() > 0 ? double(pm.width()) : surface_width;
    const double draw_height = pm.height() > 0 ? double(pm.height()) : surface_height;
    const double scale_x = draw_width / surface_width;
    const double scale_y = draw_height / surface_height;

    CairoStateGuard guard(cr);
    cairo_new_path(cr);
    cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
    cairo_clip(cr);

    // Qt anchors the tile at rectangle.topLeft() + offset.
    cairo_translate(cr, r.x() + offset.x(), r.y() + offset.y());
    cairo_scale(cr, scale_x, scale_y);

    cairo_pattern_t *pattern = cairo_pattern_create_for_surface(surface);
    if (!pattern) {
        throw std::runtime_error("CairoPainter: cannot allocate a tiled Cairo pattern for Pixmap");
    }
    if (cairo_pattern_status(pattern) != CAIRO_STATUS_SUCCESS) {
        const std::string message = cairo_status_to_string(cairo_pattern_status(pattern));
        cairo_pattern_destroy(pattern);
        throw std::runtime_error("CairoPainter: cannot create tiled Pixmap pattern: " + message);
    }
    cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
    cairo_pattern_set_filter(pattern, CAIRO_FILTER_GOOD);
    cairo_set_source(cr, pattern);
    cairo_pattern_destroy(pattern);

    cairo_paint(cr);
    ensure_ok("drawTiledPixmap");
}

// -------------------------------------------------------- CairoPainter glue

CairoPainter::CairoPainter(cairo_t *context, const music::RuntimeFontsDatabase &fonts_database, double device_dpi)
    : impl_(std::make_unique<Impl>(context, fonts_database, device_dpi)) {}

CairoPainter::~CairoPainter() = default;

cairo_t *CairoPainter::cairo_context() const { return impl_->cr; }

bool CairoPainter::is_active() const { return impl_->is_active(); }

void CairoPainter::begin_target(const std::string &name) { impl_->begin_target(name); }

void CairoPainter::before_end_target_hook(Painter *painter) { impl_->before_end_target_hook(painter); }

bool CairoPainter::end_target(bool end_draw) { return impl_->end_target(end_draw); }

void CairoPainter::begin_object(const std::string &name) { impl_->begin_object(name); }

void CairoPainter::end_object() { impl_->end_object(); }

void CairoPainter::set_antialiasing(bool arg) { impl_->set_antialiasing(arg); }

void CairoPainter::set_composition_mode(CompositionMode mode) { impl_->set_composition_mode(mode); }

void CairoPainter::set_window(const RectF &window) { impl_->set_window(window); }

void CairoPainter::set_viewport(const RectF &viewport) { impl_->set_viewport(viewport); }

void CairoPainter::set_font(const Font &font) { impl_->set_font(font); }

const Font &CairoPainter::font() const { return impl_->state.font; }

void CairoPainter::set_pen(const Pen &pen) { impl_->set_pen(pen); }

void CairoPainter::set_no_pen() { impl_->set_no_pen(); }

const Pen &CairoPainter::pen() const { return impl_->state.pen; }

void CairoPainter::set_brush(const Brush &brush) { impl_->set_brush(brush); }

const Brush &CairoPainter::brush() const { return impl_->state.brush; }

void CairoPainter::save() { impl_->save(); }

void CairoPainter::restore() { impl_->restore(); }

double CairoPainter::device_logical_dpi() const { return impl_->dpi; }

void CairoPainter::set_transform(const Transform &transform) { impl_->set_transform(transform); }

const Transform &CairoPainter::transform() const {
    // Matches QPainterProvider: return the world transform last supplied via
    // setTransform().  When muse::draw::Painter supplies a complete
    // world*viewport matrix this is that matrix; for direct users it is the
    // world part before window/viewport composition.
    return impl_->state.world_transform;
}

void CairoPainter::draw_path(const PainterPath &path) { impl_->draw_path(path); }

void CairoPainter::draw_polygon(const PointF *points, size_t point_count, PolygonMode mode) {
    impl_->draw_polygon(points, point_count, mode);
}

void CairoPainter::draw_text(const PointF &point, const String &text) { impl_->draw_text(point, text); }

void CairoPainter::draw_text(const RectF &rect, Alignment alignment, TextFlags text_flags, const String &text) {
    impl_->draw_text(rect, alignment, text_flags, text);
}

void CairoPainter::draw_symbol(const PointF &point, char32_t ucs4_code) { impl_->draw_symbol(point, ucs4_code); }

void CairoPainter::draw_pixmap(const PointF &point, const Pixmap &pm) { impl_->draw_pixmap(point, pm); }

void CairoPainter::draw_tiled_pixmap(const RectF &rect, const Pixmap &pm, const PointF &offset) {
    impl_->draw_tiled_pixmap(rect, pm, offset);
}

bool CairoPainter::has_clipping() const { return impl_->has_clipping(); }

void CairoPainter::set_clip_rect(const RectF &rect) { impl_->set_clip_rect(rect); }

void CairoPainter::set_mask(const RectF &background, const std::vector<RectF> &mask_rects) { impl_->set_mask(background, mask_rects); }

void CairoPainter::set_clipping(bool enable) { impl_->set_clipping(enable); }
} // namespace muse::draw
