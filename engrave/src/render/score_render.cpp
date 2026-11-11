#include "score_render.h"

#include <cmath>
#include <memory>
#include <stdexcept>

#include <cairo/cairo-pdf.h>
#include <cairo/cairo-svg.h>
#include <cairo/cairo.h>

#include "cairo_painter.h"
#include "draw/painter.h"
#include "engrave/src/runtime/engine_context.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/score.h"
#include "engraving/rendering/iscorerenderer.h"

namespace music {
namespace {
cairo_status_t append_bytes(void *context, const unsigned char *bytes, unsigned length) noexcept {
    try {
        static_cast<std::string *>(context)->append(reinterpret_cast<const char *>(bytes), length);
        return CAIRO_STATUS_SUCCESS;
    } catch (...) {
        return CAIRO_STATUS_WRITE_ERROR;
    }
}

void check(cairo_status_t status) {
    if (status != CAIRO_STATUS_SUCCESS) throw std::runtime_error(cairo_status_to_string(status));
}
} // namespace

std::string render_score(const EngineContext &engine, mu::engraving::Score *score, const std::string &format, int page) {
    ExportOptions options;
    options.page = page;
    return render_score(engine, score, format, options);
}

std::string render_score(const EngineContext &engine, mu::engraving::Score *score, const std::string &format,
                         const ExportOptions &options) {
    const auto poll = [&] {
        if (options.cancelled && options.cancelled()) throw RenderCancelled{};
    };
    poll();
    using namespace mu::engraving;
    if (!score || !score->npages()) throw std::runtime_error("Score has no pages");
    const int npages = static_cast<int>(score->npages());
    if (options.page < 0 || (format != "pdf" && options.page >= npages)) throw std::invalid_argument("Page out of range");
    // A PDF holds a range of pages; PNG and SVG render options.page alone.
    const int first_page_value = options.first_page;
    const int last_page_value = options.last_page >= 0 ? options.last_page : npages - 1;
    if (format == "pdf" && (first_page_value < 0 || last_page_value < first_page_value || last_page_value >= npages))
        throw std::invalid_argument("Page range out of range");
    const auto &renderer = score->renderer();
    if (!renderer) throw std::runtime_error("Score renderer unavailable");
    const auto inches = renderer->page_size_inch(score);
    int dpi = 72;
    if (format == "png") {
        if (!std::isfinite(static_cast<double>(options.png_dpi)) || options.png_dpi < 36 || options.png_dpi > 1200)
            throw std::invalid_argument("PNG DPI must be between 36 and 1200");
        dpi = options.png_dpi;
    }
    const double width = inches.width() * dpi, height = inches.height() * dpi;
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 || width > 32767 || height > 32767 ||
        width * height > 64e6)
        throw RenderLimitExceeded("Page dimensions exceed rendering limit");
    std::string output;
    cairo_surface_t *raw = nullptr;
    if (format == "pdf")
        raw = cairo_pdf_surface_create_for_stream(append_bytes, &output, width, height);
    else if (format == "svg")
        raw = cairo_svg_surface_create_for_stream(append_bytes, &output, width, height);
    else if (format == "png")
        raw = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height)));
    else
        throw std::invalid_argument("Unsupported render format");
    std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface(raw, cairo_surface_destroy);
    check(cairo_surface_status(surface.get()));
    std::unique_ptr<cairo_t, decltype(&cairo_destroy)> context(cairo_create(surface.get()), cairo_destroy);
    check(cairo_status(context.get()));
    // Vector surfaces start with an unbounded clip, which Cairo cannot enumerate.
    cairo_rectangle(context.get(), 0, 0, std::ceil(width), std::ceil(height));
    cairo_clip(context.get());
    if (format == "png" && !options.transparent) {
        cairo_save(context.get());
        cairo_set_source_rgb(context.get(), 1.0, 1.0, 1.0);
        cairo_paint(context.get());
        cairo_restore(context.get());
        check(cairo_status(context.get()));
    }
    auto provider = std::make_shared<muse::draw::CairoPainter>(context.get(), engine.fonts_database(), dpi);
    const bool was_printing = score->printing();

    struct Restore {
        Score *score;
        bool printing;

        ~Restore() { score->set_printing(printing); }
    } restore{score, was_printing};

    {
        muse::draw::Painter painter(provider, "native-export");
        rendering::IScoreRenderer::ScorePaintOptions paint_options;
        paint_options.is_printing = true;
        paint_options.device_dpi = dpi;
        paint_options.from_page = format == "pdf" ? first_page_value : options.page;
        paint_options.to_page = format == "pdf" ? last_page_value : options.page;
        paint_options.on_new_page = [&] {
            poll(); // previous page drawn, before encoding it
            cairo_show_page(context.get());
            check(cairo_status(context.get()));
            poll(); // before drawing the next page
        };
        paint_options.print_page_background = format == "pdf" ? true : options.draw_page_background;
        renderer->paint_score(&painter, score, paint_options);
        if (!painter.end_draw()) throw std::runtime_error("Painter did not finish");
    }
    poll(); // final page drawn, before encoding
    check(cairo_status(context.get()));
    if (format == "png")
        check(cairo_surface_write_to_png_stream(surface.get(), append_bytes, &output));
    else
        cairo_show_page(context.get());
    cairo_surface_finish(surface.get());
    check(cairo_surface_status(surface.get()));
    poll();
    return output;
}
} // namespace music
