#include "score_render.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

#include <cairo/cairo-pdf.h>
#include <cairo/cairo-svg.h>
#include <cairo/cairo.h>

#include "cairo_painter.h"
#include "draw/painter.h"
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/runtime/runtime_assets.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/score.h"
#include "engraving/rendering/score/scorerenderer.h"

namespace music {
namespace {
struct PaintCancelled {
    std::exception_ptr callback_error;
};

struct PaintLimitExceeded {};

struct BoundedStream {
    std::string bytes;
    std::optional<std::uint64_t> max_bytes;
    std::atomic<std::uint64_t> *shared_bytes = nullptr;
    const std::atomic<bool> *shared_stop = nullptr;
    bool limit_exceeded = false;
    bool stopped = false;
};

cairo_status_t append_bytes(void *context, const unsigned char *bytes, unsigned length) noexcept {
    auto &stream = *static_cast<BoundedStream *>(context);
    if (stream.shared_stop && stream.shared_stop->load(std::memory_order_relaxed)) {
        stream.stopped = true;
        return CAIRO_STATUS_WRITE_ERROR;
    }
    if (stream.max_bytes) {
        if (stream.shared_bytes) {
            std::uint64_t used = stream.shared_bytes->load(std::memory_order_relaxed);
            for (;;) {
                if (length > *stream.max_bytes || used > *stream.max_bytes - length) {
                    stream.limit_exceeded = true;
                    return CAIRO_STATUS_WRITE_ERROR;
                }
                if (stream.shared_bytes->compare_exchange_weak(used, used + length, std::memory_order_relaxed)) break;
            }
        } else if (length > *stream.max_bytes || stream.bytes.size() > *stream.max_bytes - length) {
            stream.limit_exceeded = true;
            return CAIRO_STATUS_WRITE_ERROR;
        }
    }
    try {
        stream.bytes.append(reinterpret_cast<const char *>(bytes), length);
        return CAIRO_STATUS_SUCCESS;
    } catch (...) {
        return CAIRO_STATUS_WRITE_ERROR;
    }
}

void check(cairo_status_t status) {
    if (status != CAIRO_STATUS_SUCCESS) throw std::runtime_error(cairo_status_to_string(status));
}

// ENGRAVE_RENDER_TIMING splits a successful export into drawing (surface setup
// and painting) and encoding (PNG deflate, or Cairo page emission and surface
// finish across the exported page or PDF range), one stderr line per export.
struct RenderTiming {
    using Clock = std::chrono::steady_clock;

    static bool enabled() {
        static const bool value = std::getenv("ENGRAVE_RENDER_TIMING") != nullptr;
        return value;
    }

    Clock::time_point start = enabled() ? Clock::now() : Clock::time_point();
    double encode_seconds = 0;

    Clock::time_point begin_encode() const { return enabled() ? Clock::now() : Clock::time_point(); }

    void end_encode(Clock::time_point since) {
        if (enabled()) encode_seconds += std::chrono::duration<double>(Clock::now() - since).count();
    }

    double elapsed() const { return enabled() ? std::chrono::duration<double>(Clock::now() - start).count() : 0.0; }

    void report(const std::string &format, int first_page, int last_page, int dpi, std::size_t bytes) const {
        if (!enabled()) return;
        const double total = std::chrono::duration<double>(Clock::now() - start).count();
        const double raster = std::max(0.0, total - encode_seconds);
        if (format == "pdf") {
            std::fprintf(stderr, "render_seconds format=%s pages=%d-%d dpi=%d raster=%.9f encode=%.9f bytes=%llu\n", format.c_str(),
                         first_page, last_page, dpi, raster, encode_seconds, static_cast<unsigned long long>(bytes));
        } else {
            std::fprintf(stderr, "render_seconds format=%s page=%d dpi=%d raster=%.9f encode=%.9f bytes=%llu\n", format.c_str(), first_page,
                         dpi, raster, encode_seconds, static_cast<unsigned long long>(bytes));
        }
    }
};

// A drawn PNG page handed out before encoding.
struct RasterOut {
    cairo_surface_t *surface = nullptr;
    int dpi = 0;
    double seconds = 0;
};

engrave::Result<std::string> render_impl(const EngineContext &engine, mu::engraving::Score *score, const std::string &format,
                                         const ExportOptions &options, RasterOut *raster) {
    const auto poll = [&] { return options.cancelled && options.cancelled(); };
    if (poll()) return std::unexpected(engrave::Error{engrave::ErrorCode::CANCELLED, "render cancelled"});
    using namespace mu::engraving;
    if (!score || !score->npages()) return std::unexpected(engrave::Error{engrave::ErrorCode::INVALID_ARGUMENT, "Score has no pages"});
    const int npages = static_cast<int>(score->npages());
    if (options.page < 0 || (format != "pdf" && options.page >= npages))
        return std::unexpected(engrave::Error{engrave::ErrorCode::INVALID_ARGUMENT, "Page out of range"});
    // A PDF holds a range of pages; PNG and SVG render options.page alone.
    const int first_page_value = options.first_page;
    const int last_page_value = options.last_page >= 0 ? options.last_page : npages - 1;
    if (format == "pdf" && (first_page_value < 0 || last_page_value < first_page_value || last_page_value >= npages))
        return std::unexpected(engrave::Error{engrave::ErrorCode::INVALID_ARGUMENT, "Page range out of range"});
    const auto &renderer = score->renderer();
    if (!renderer) throw std::runtime_error("Score renderer unavailable");
    const auto inches = renderer->page_size_inch(score);
    int dpi = 72;
    if (format == "png") {
        if (options.png_dpi < 36 || options.png_dpi > 1200)
            return std::unexpected(engrave::Error{engrave::ErrorCode::INVALID_ARGUMENT, "PNG DPI must be between 36 and 1200"});
        dpi = options.png_dpi;
    }
    const double width = inches.width() * dpi, height = inches.height() * dpi;
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 || width > 32767 || height > 32767 ||
        width * height > 64e6)
        return std::unexpected(engrave::Error{engrave::ErrorCode::RESOURCE_LIMIT, "Page dimensions exceed rendering limit"});
    BoundedStream stream;
    stream.max_bytes = options.max_bytes;
    RenderTiming timing;
    cairo_surface_t *raw = nullptr;
    if (format == "pdf")
        raw = cairo_pdf_surface_create_for_stream(append_bytes, &stream, width, height);
    else if (format == "svg")
        raw = cairo_svg_surface_create_for_stream(append_bytes, &stream, width, height);
    else if (format == "png")
        raw = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height)));
    else
        return std::unexpected(engrave::Error{engrave::ErrorCode::INVALID_ARGUMENT, "Unsupported render format"});
    std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface(raw, cairo_surface_destroy);
    if (stream.limit_exceeded) {
        if (poll()) return std::unexpected(engrave::Error{engrave::ErrorCode::CANCELLED, "render cancelled"});
        return std::unexpected(engrave::Error{engrave::ErrorCode::RESOURCE_LIMIT, "render exceeds output byte limit"});
    }
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
    auto provider = std::make_shared<muse::draw::CairoPainter>(context.get(), engine.assets().cairo_fonts(), dpi);
    {
        muse::draw::Painter painter(provider, "native-export");
        rendering::score::ScoreRenderer::ScorePaintOptions paint_options;
        paint_options.is_printing = true;
        paint_options.device_dpi = dpi;
        paint_options.from_page = format == "pdf" ? first_page_value : options.page;
        paint_options.to_page = format == "pdf" ? last_page_value : options.page;
        const auto poll_page = [&] {
            bool cancelled;
            try {
                cancelled = poll();
            } catch (...) {
                throw PaintCancelled{std::current_exception()};
            }
            if (cancelled) throw PaintCancelled{};
        };
        paint_options.on_new_page = [&] {
            poll_page(); // previous page drawn, before encoding it
            const auto encode_start = timing.begin_encode();
            cairo_show_page(context.get());
            timing.end_encode(encode_start);
            if (stream.limit_exceeded) throw PaintLimitExceeded{};
            check(cairo_status(context.get()));
            poll_page(); // before drawing the next page
        };
        paint_options.print_page_background = format == "pdf" ? true : options.draw_page_background;
        try {
            renderer->paint_score(&painter, score, paint_options);
        } catch (const PaintCancelled &stop) {
            if (stop.callback_error) std::rethrow_exception(stop.callback_error);
            return std::unexpected(engrave::Error{engrave::ErrorCode::CANCELLED, "render cancelled"});
        } catch (const PaintLimitExceeded &) {
            if (poll()) return std::unexpected(engrave::Error{engrave::ErrorCode::CANCELLED, "render cancelled"});
            return std::unexpected(engrave::Error{engrave::ErrorCode::RESOURCE_LIMIT, "render exceeds output byte limit"});
        } catch (const std::invalid_argument &error) {
            return std::unexpected(engrave::Error{engrave::ErrorCode::INVALID_ARGUMENT, error.what()});
        }
        if (!painter.end_draw()) throw std::runtime_error("Painter did not finish");
    }
    if (poll())
        return std::unexpected(engrave::Error{engrave::ErrorCode::CANCELLED, "render cancelled"}); // final page drawn, before encoding
    if (stream.limit_exceeded)
        return std::unexpected(engrave::Error{engrave::ErrorCode::RESOURCE_LIMIT, "render exceeds output byte limit"});
    check(cairo_status(context.get()));
    if (raster) {
        cairo_surface_flush(surface.get());
        check(cairo_surface_status(surface.get()));
        // The extra reference outlives this function's context and surface owners.
        raster->surface = cairo_surface_reference(surface.get());
        raster->dpi = dpi;
        raster->seconds = timing.elapsed();
        return std::string();
    }
    const auto final_encode_start = timing.begin_encode();
    cairo_status_t write_status = CAIRO_STATUS_SUCCESS;
    if (format == "png")
        write_status = cairo_surface_write_to_png_stream(surface.get(), append_bytes, &stream);
    else
        cairo_show_page(context.get());
    cairo_surface_finish(surface.get());
    timing.end_encode(final_encode_start);
    if (stream.limit_exceeded) {
        if (poll()) return std::unexpected(engrave::Error{engrave::ErrorCode::CANCELLED, "render cancelled"});
        return std::unexpected(engrave::Error{engrave::ErrorCode::RESOURCE_LIMIT, "render exceeds output byte limit"});
    }
    check(write_status);
    check(cairo_surface_status(surface.get()));
    const int report_first = format == "pdf" ? first_page_value : options.page;
    const int report_last = format == "pdf" ? last_page_value : options.page;
    timing.report(format, report_first, report_last, dpi, stream.bytes.size());
    if (poll()) return std::unexpected(engrave::Error{engrave::ErrorCode::CANCELLED, "render cancelled"});
    return std::move(stream.bytes);
}
} // namespace

engrave::Result<std::string> render_score(const EngineContext &engine, mu::engraving::Score *score, const std::string &format, int page) {
    ExportOptions options;
    options.page = page;
    return render_score(engine, score, format, options);
}

engrave::Result<std::string> render_score(const EngineContext &engine, mu::engraving::Score *score, const std::string &format,
                                          const ExportOptions &options) {
    return render_impl(engine, score, format, options, nullptr);
}

engrave::Result<PngRaster> rasterize_png(const EngineContext &engine, mu::engraving::Score *score, const ExportOptions &options) {
    RasterOut out;
    auto drawn = render_impl(engine, score, "png", options, &out);
    if (!drawn) return std::unexpected(std::move(drawn.error()));
    PngRaster raster;
    raster.surface_ = out.surface;
    raster.page = options.page;
    raster.dpi = out.dpi;
    raster.raster_seconds = out.seconds;
    return raster;
}

PngRaster::PngRaster(PngRaster &&other) noexcept
    : page(other.page), dpi(other.dpi), raster_seconds(other.raster_seconds), encode_seconds(other.encode_seconds),
      surface_(std::exchange(other.surface_, nullptr)) {}

PngRaster &PngRaster::operator=(PngRaster &&other) noexcept {
    if (this != &other) {
        if (surface_) cairo_surface_destroy(surface_);
        surface_ = std::exchange(other.surface_, nullptr);
        page = other.page;
        dpi = other.dpi;
        raster_seconds = other.raster_seconds;
        encode_seconds = other.encode_seconds;
    }
    return *this;
}

PngRaster::~PngRaster() {
    if (surface_) cairo_surface_destroy(surface_);
}

std::size_t PngRaster::bytes() const noexcept {
    if (!surface_) return 0;
    return static_cast<std::size_t>(cairo_image_surface_get_stride(surface_)) *
           static_cast<std::size_t>(cairo_image_surface_get_height(surface_));
}

PngRaster::EncodedPage PngRaster::encode(std::optional<std::uint64_t> max_bytes, std::atomic<std::uint64_t> *shared_bytes,
                                         const std::atomic<bool> *shared_stop) {
    const std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface(std::exchange(surface_, nullptr),
                                                                                     cairo_surface_destroy);
    if (!surface) throw std::runtime_error("PNG page has no surface");
    const auto start = RenderTiming::enabled() ? RenderTiming::Clock::now() : RenderTiming::Clock::time_point();
    BoundedStream stream;
    stream.max_bytes = max_bytes;
    stream.shared_bytes = shared_bytes;
    stream.shared_stop = shared_stop;
    const cairo_status_t write_status = cairo_surface_write_to_png_stream(surface.get(), append_bytes, &stream);
    cairo_surface_finish(surface.get());
    if (RenderTiming::enabled()) encode_seconds = std::chrono::duration<double>(RenderTiming::Clock::now() - start).count();
    if (stream.limit_exceeded) return {EncodeStatus::LIMIT_EXCEEDED, {}};
    if (stream.stopped) return {EncodeStatus::STOPPED, {}};
    check(write_status);
    check(cairo_surface_status(surface.get()));
    return {EncodeStatus::OK, std::move(stream.bytes)};
}

bool render_timing_enabled() { return RenderTiming::enabled(); }
} // namespace music
