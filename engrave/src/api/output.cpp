#include <format>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/render/score_render.h"
#include "engrave/src/score/export_options.h"
#include "engrave/src/score/score_positions.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/score.h"
#include "engraving/engravingproject.h"
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

} // namespace

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
    if (options.cancelled) backend.cancelled = music::make_unique_fn<bool() const>(options.cancelled);
    backend.first_page = first;
    backend.last_page = last;
    const std::string format = format_name;
    RenderOutput output;
    if (options.format == RenderFormat::PDF) {
        auto data = music::render_score(*impl_->engine->context, &score, format, backend);
        if (!data) return std::unexpected(std::move(data.error()));
        output.document = bytes_of(*data);
        return output;
    }
    output.pages.reserve(static_cast<std::size_t>(last - first + 1));
    for (std::int32_t page = first; page <= last; ++page) {
        backend.page = page;
        auto data = music::render_score(*impl_->engine->context, &score, format, backend);
        if (!data) return std::unexpected(std::move(data.error()));
        output.pages.push_back(bytes_of(*data));
    }
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
