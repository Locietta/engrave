#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "engrave/src/score/export_options.h"
#include <engrave/error.h>

struct _cairo_surface;

namespace mu::engraving {
struct Score;
} // namespace mu::engraving

namespace music {
struct EngineContext;

engrave::Result<std::string> render_score(const EngineContext &engine, mu::engraving::Score *score, const std::string &format, int page);
engrave::Result<std::string> render_score(const EngineContext &engine, mu::engraving::Score *score, const std::string &format,
                                          const ExportOptions &options);

// One page drawn for PNG and not yet deflated. It owns its image surface
// alone, so encode() may run on any one thread while the engine draws on.
struct PngRaster {
    PngRaster() = default;
    PngRaster(PngRaster &&other) noexcept;
    PngRaster &operator=(PngRaster &&other) noexcept;
    ~PngRaster();

    enum class EncodeStatus { OK, LIMIT_EXCEEDED, STOPPED };

    struct EncodedPage {
        EncodeStatus status = EncodeStatus::OK;
        std::string bytes;
    };

    // Raw pixel memory; bounds how many pages may wait to be encoded.
    std::size_t bytes() const noexcept;
    // Deflates the page and releases the surface; stops when shared_bytes + chunk
    // exceeds max_bytes or shared_stop becomes true, and throws std::runtime_error
    // on a Cairo failure.
    EncodedPage encode(std::optional<std::uint64_t> max_bytes = {}, std::atomic<std::uint64_t> *shared_bytes = nullptr,
                       const std::atomic<bool> *shared_stop = nullptr);

    int page = 0, dpi = 0;
    double raster_seconds = 0, encode_seconds = 0;
private:
    friend engrave::Result<PngRaster> rasterize_png(const EngineContext &, mu::engraving::Score *, const ExportOptions &);
    _cairo_surface *surface_ = nullptr;
};

// Draws options.page exactly as render_score(..., "png", options) does, polling
// options.cancelled the same way, but stops before encoding.
engrave::Result<PngRaster> rasterize_png(const EngineContext &engine, mu::engraving::Score *score, const ExportOptions &options);

bool render_timing_enabled();
} // namespace music
