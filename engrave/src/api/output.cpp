#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/render/score_render.h"
#include "engrave/src/runtime/worker_threads.h"
#include "engrave/src/score/export_options.h"
#include "engrave/src/score/score_positions.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/score.h"
#include "engraving/engravingproject.h"
#include "global/pollwait.h"
#include <engrave/document.h>

namespace engrave {
namespace {

std::unexpected<Error> fail(ErrorCode code, std::string message) { return std::unexpected(Error{code, std::move(message)}); }

std::vector<std::byte> bytes_of(const std::string &data) {
    const auto *first = reinterpret_cast<const std::byte *>(data.data());
    return std::vector<std::byte>(first, first + data.size());
}

const char *backend_name(RenderFormat format) {
    switch (format) {
        case RenderFormat::PDF: return "pdf";
        case RenderFormat::PNG: return "png";
        case RenderFormat::SVG: return "svg";
    }
    return nullptr;
}

// Raw ARGB32 surfaces handed to encoders at once, beyond the page being drawn;
// a single page larger than this still encodes alone.
constexpr std::size_t k_max_queued_png_bytes = 256u << 20;

// Deflate threads for one multi-page PNG render. Pages are drawn in order on
// the calling thread, which alone polls cancellation; workers only encode the
// surfaces handed to them and see the stop flag. The destructor stops and joins
// the workers and frees every surface not yet encoded.
struct PngEncoders {
    explicit PngEncoders(std::size_t pages, std::optional<std::uint64_t> max_bytes)
        : encoded(pages), encode_seconds(pages), max_bytes(max_bytes) {}

    ~PngEncoders() { stop_and_join(); }

    PngEncoders(const PngEncoders &) = delete;
    PngEncoders &operator=(const PngEncoders &) = delete;

    std::size_t start(std::size_t count, int first) {
        for (std::size_t i = 0; i < count; ++i) {
            try {
                threads.emplace_back([this, first] { loop(first); });
            } catch (const std::system_error &) {
                break;
            }
        }
        return threads.size();
    }

    void stop_and_join() {
        stop.store(true, std::memory_order_relaxed);
        { const std::scoped_lock lock(mutex); }
        work.notify_all();
        for (auto &thread : threads) thread.join();
        threads.clear();
        queue.clear();
    }

    void loop(int first) {
        for (;;) {
            music::PngRaster raster;
            {
                std::unique_lock lock(mutex);
                work.wait(lock, [&] { return stop.load(std::memory_order_relaxed) || closed || !queue.empty(); });
                if (stop.load(std::memory_order_relaxed) || queue.empty()) return;
                raster = std::move(queue.front());
                queue.pop_front();
            }
            const std::size_t bytes = raster.bytes();
            const auto index = static_cast<std::size_t>(raster.page - first);
            std::vector<std::byte> page_bytes;
            auto status = music::PngRaster::EncodeStatus::OK;
            double seconds = 0.0;
            std::exception_ptr failure;
            try {
                auto result = raster.encode(max_bytes, &encoded_bytes, &stop);
                status = result.status;
                if (status == music::PngRaster::EncodeStatus::OK) {
                    page_bytes = bytes_of(result.bytes);
                    seconds = raster.encode_seconds;
                }
            } catch (...) {
                failure = std::current_exception();
            }
            {
                const std::scoped_lock lock(mutex);
                --in_flight;
                in_flight_bytes -= bytes;
                if (failure) {
                    if (!error) error = std::move(failure);
                    stop.store(true, std::memory_order_relaxed);
                } else if (status == music::PngRaster::EncodeStatus::LIMIT_EXCEEDED) {
                    limit_exceeded = true;
                    stop.store(true, std::memory_order_relaxed);
                } else if (status == music::PngRaster::EncodeStatus::OK && !stop.load(std::memory_order_relaxed)) {
                    encoded[index] = std::move(page_bytes);
                    encode_seconds[index] = seconds;
                }
            }
            progress.signal();
        }
    }

    // Written under mutex while workers run; read after the workers have joined.
    std::vector<std::vector<std::byte>> encoded;
    std::vector<double> encode_seconds;
    std::optional<std::uint64_t> max_bytes;
    std::mutex mutex;
    std::condition_variable work;
    muse::PollWait progress;
    std::vector<std::thread> threads;
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> encoded_bytes{0};
    // Guarded by mutex. in_flight counts surfaces queued or being encoded.
    std::deque<music::PngRaster> queue;
    std::size_t in_flight = 0, in_flight_bytes = 0;
    bool closed = false;
    bool limit_exceeded = false;
    std::exception_ptr error;
};

// Empty when no worker thread could start.
std::optional<Result<RenderOutput>> render_png_pages(const music::EngineContext &context, mu::engraving::Score &score,
                                                     music::ExportOptions &backend, std::int32_t first, std::int32_t last,
                                                     std::size_t workers, std::optional<std::uint64_t> max_bytes) {
    const auto count = static_cast<std::size_t>(last - first + 1);
    PngEncoders encoders(count, max_bytes);
    workers = encoders.start(workers, first);
    if (workers == 0) return std::nullopt;
    const auto poll = [&] { return backend.cancelled && backend.cancelled(); };
    const auto cancelled = [] { return fail(ErrorCode::CANCELLED, "render cancelled"); };
    std::vector<double> raster_seconds(count);
    std::vector<int> dpis(count);
    // Waits, polling cancellation, until `ready` holds or an encoder stopped;
    // false when the render was cancelled.
    const auto wait_until = [&](std::unique_lock<std::mutex> &lock, const auto &ready) {
        while (!encoders.error && !encoders.limit_exceeded && !ready()) {
            encoders.progress.wait(lock);
            if (encoders.error || encoders.limit_exceeded || ready()) break;
            lock.unlock();
            const bool stop = poll();
            lock.lock();
            if (stop) return false;
        }
        return true;
    };
    for (std::int32_t page = first; page <= last; ++page) {
        if (encoders.stop.load(std::memory_order_relaxed)) break;
        backend.page = page;
        auto raster = music::rasterize_png(context, &score, backend);
        if (!raster) return std::unexpected(std::move(raster.error()));
        const auto index = static_cast<std::size_t>(page - first);
        raster_seconds[index] = raster->raster_seconds;
        dpis[index] = raster->dpi;
        const std::size_t bytes = raster->bytes();
        std::unique_lock lock(encoders.mutex);
        const bool proceed = wait_until(lock, [&] {
            return encoders.in_flight < workers && (encoders.in_flight == 0 || encoders.in_flight_bytes + bytes <= k_max_queued_png_bytes);
        });
        if (!proceed) return cancelled();
        if (encoders.error || encoders.limit_exceeded) break;
        ++encoders.in_flight;
        encoders.in_flight_bytes += bytes;
        encoders.queue.push_back(std::move(*raster));
        lock.unlock();
        encoders.work.notify_one();
    }
    {
        std::unique_lock lock(encoders.mutex);
        encoders.closed = true;
        lock.unlock();
        encoders.work.notify_all();
        lock.lock();
        if (!wait_until(lock, [&] { return encoders.in_flight == 0; })) return cancelled();
    }
    encoders.stop_and_join();
    if (const auto &hook = api::on_png_encoders_joined_for_test()) hook();
    if (poll()) return cancelled();
    if (encoders.error) std::rethrow_exception(encoders.error);
    if (encoders.limit_exceeded) return fail(ErrorCode::RESOURCE_LIMIT, "render exceeds output byte limit");
    RenderOutput output;
    output.pages = std::move(encoders.encoded);
    if (music::render_timing_enabled()) {
        for (std::size_t i = 0; i < count; ++i)
            std::fprintf(stderr, "render_seconds format=png page=%d dpi=%d raster=%.9f encode=%.9f bytes=%llu\n",
                         first + static_cast<int>(i), dpis[i], raster_seconds[i], encoders.encode_seconds[i],
                         static_cast<unsigned long long>(output.pages[i].size()));
    }
    return output;
}

} // namespace

namespace api {
std::function<void()> &on_png_encoders_joined_for_test() {
    thread_local std::function<void()> hook;
    return hook;
}
} // namespace api

Result<std::int32_t> Document::page_count(const ScoreSel &selection) {
    impl_->engine->check_thread();
    const Result<mu::engraving::Score *> selected = select_score(*impl_->project->master_score(), selection);
    if (!selected) return std::unexpected(selected.error());
    impl_->ensure_layout(**selected);
    return static_cast<std::int32_t>((*selected)->npages());
}

Result<RenderOutput> Document::render(const RenderOptions &options) {
    impl_->engine->check_thread();
    const char *format_name = backend_name(options.format);
    if (!format_name) return fail(ErrorCode::INVALID_ARGUMENT, "Unsupported render format");
    const Result<mu::engraving::Score *> selected = select_score(*impl_->project->master_score(), options.score);
    if (!selected) return std::unexpected(selected.error());
    mu::engraving::Score &score = **selected;
    impl_->ensure_layout(score);

    if (options.dpi < min_render_dpi || options.dpi > max_render_dpi) {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    std::format("the dpi must be between {} and {}, not {}", min_render_dpi, max_render_dpi, options.dpi));
    }
    // the other renderers draw at 72 dpi (PDF points, SVG units) and take no dpi.
    if (options.format != RenderFormat::PNG && options.dpi != RenderOptions{}.dpi) {
        return fail(ErrorCode::INVALID_ARGUMENT, std::format("dpi applies to PNG, not to {}", to_string(options.format)));
    }
    const auto npages = static_cast<std::int32_t>(score.npages());
    if (npages == 0) return fail(ErrorCode::INVALID_ARGUMENT, "the score has no pages");
    const std::int32_t first = options.first_page;
    const std::int32_t last = options.last_page < 0 ? npages - 1 : options.last_page;
    if (first < 0 || last < first || last >= npages) {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    std::format("the page range {}..{} is outside the score's 0..{}", first, last, npages - 1));
    }

    music::ExportOptions backend;
    backend.png_dpi = options.dpi;
    backend.draw_page_background = options.background;
    backend.transparent = options.transparent;
    backend.cancelled = options.cancelled;
    backend.first_page = first;
    backend.last_page = last;
    const std::string format = format_name;
    RenderOutput output;
    if (options.format == RenderFormat::PDF) {
        backend.max_bytes = options.max_bytes;
        auto data = music::render_score(*impl_->engine->context, &score, format, backend);
        if (!data) return std::unexpected(std::move(data.error()));
        output.document = bytes_of(*data);
        return output;
    }
    const auto started = std::chrono::steady_clock::now();
    std::size_t workers = 1;
    const auto report_wall = [&] {
        if (options.format != RenderFormat::PNG || last == first || !music::render_timing_enabled()) return;
        std::fprintf(stderr, "render_wall format=png pages=%d-%d dpi=%d workers=%llu seconds=%.9f\n", first, last, options.dpi,
                     static_cast<unsigned long long>(workers),
                     std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
    };
    if (options.format == RenderFormat::PNG && last > first) {
        workers = std::min(music::render_worker_threads(impl_->engine->audio_threads), static_cast<std::size_t>(last - first + 1));
        if (workers > 1) {
            if (auto encoded = render_png_pages(*impl_->engine->context, score, backend, first, last, workers, options.max_bytes)) {
                if (*encoded) report_wall();
                return std::move(*encoded);
            }
            workers = 1;
        }
    }
    output.pages.reserve(static_cast<std::size_t>(last - first + 1));
    std::uint64_t total_bytes = 0;
    for (std::int32_t page = first; page <= last; ++page) {
        backend.page = page;
        if (options.max_bytes) backend.max_bytes = *options.max_bytes - total_bytes;
        auto data = music::render_score(*impl_->engine->context, &score, format, backend);
        if (!data) return std::unexpected(std::move(data.error()));
        total_bytes += data->size();
        output.pages.push_back(bytes_of(*data));
    }
    report_wall();
    return output;
}

Result<Positions> Document::positions(PositionKind kind, const ScoreSel &selection) { return positions(kind, selection, {}); }

Result<Positions> Document::positions(PositionKind kind, const ScoreSel &selection, const CaptureOptions &capture) {
    impl_->engine->check_thread();
    if (kind != PositionKind::MEASURE && kind != PositionKind::SEGMENT) return fail(ErrorCode::INVALID_ARGUMENT, "unknown position kind");
    const auto cancelled = [&] { return capture.cancelled && capture.cancelled(); };
    if (cancelled()) return fail(ErrorCode::CANCELLED, "position capture cancelled");
    const auto selected = select_score(*impl_->project->master_score(), selection);
    if (!selected) return std::unexpected(selected.error());
    impl_->ensure_layout(**selected);
    if ((*selected)->npages() == 0) return fail(ErrorCode::INVALID_ARGUMENT, "the score has no pages");

    if (cancelled()) return fail(ErrorCode::CANCELLED, "position capture cancelled");
    std::size_t element_count = 0, event_count = 0;
    std::uint64_t bytes = 0;
    bool limit_exceeded = false;
    const auto count = [&](std::size_t amount, std::size_t &items) {
        if (cancelled()) return false;
        const auto limit = capture.max_bytes.value_or(std::numeric_limits<std::uint64_t>::max());
        if (bytes > limit || amount > limit - bytes) {
            limit_exceeded = true;
            return false;
        }
        bytes += amount;
        ++items;
        return true;
    };
    const bool segments = kind == PositionKind::SEGMENT;
    const auto size = music::visit_score_positions(
        *selected, segments,
        {[&](int, double, double, double, double, int) { return count(sizeof(PositionElement), element_count); },
         [&](int, int) { return count(sizeof(PositionEvent), event_count); }},
        false);
    if (limit_exceeded) return fail(ErrorCode::RESOURCE_LIMIT, "position capture byte limit exceeded");
    if (!size) return fail(ErrorCode::CANCELLED, "position capture cancelled");
    Positions result;
    result.page_width = size->first;
    result.page_height = size->second;
    result.elements.reserve(element_count);
    result.events.reserve(event_count);
    const auto emitted = music::visit_score_positions(*selected, segments,
                                                      {[&](int id, double x, double y, double sx, double sy, int page) {
                                                           if (cancelled()) return false;
                                                           result.elements.push_back({id, x, y, sx, sy, page});
                                                           return true;
                                                       },
                                                       [&](int id, int time) {
                                                           if (cancelled()) return false;
                                                           result.events.push_back({id, time});
                                                           return true;
                                                       }},
                                                      true, element_count);
    if (!emitted || cancelled()) return fail(ErrorCode::CANCELLED, "position capture cancelled");
    return result;
}

} // namespace engrave
