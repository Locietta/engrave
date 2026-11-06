#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <engrave/export_warning.h>
#include <engrave/handle.h>

namespace engrave {

// Score file formats. Capabilities lists which of them the engine reads and
// which it writes.
enum struct Format {
    MSCZ,       // MuseScore archive
    MSCX,       // uncompressed MuseScore XML
    MUSIC_XML,  // uncompressed MusicXML
    MXL,        // compressed MusicXML
    MIDI,       // Standard MIDI File, including karaoke (.kar)
    GUITAR_PRO, // Guitar Pro 1-7 and Power Tab
    MNX,        // MNX JSON, schema 27; import only (.mnx or recognised JSON content)
};

std::string_view to_string(Format format) noexcept;

// Default cumulative score-writer allowance when max_bytes is set without an
// explicit max_working_bytes. Saturates instead of wrapping on large inputs.
inline constexpr std::uint64_t default_score_capture_working_bytes(std::uint64_t max_bytes) noexcept {
    constexpr std::uint64_t base = 1024 * 1024;
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    return max_bytes > (maximum - base) / 128 ? maximum : base + max_bytes * 128;
}

// Limits for a capture. max_bytes counts the returned score bytes or the
// element/event payload of Positions; an empty optional imposes no limit.
// The callback returns true to stop and is called on the document's thread;
// it must not call back into the engine. Exceptions from it reach the caller.
struct CaptureOptions {
    std::optional<std::uint64_t> max_bytes;
    std::function<bool()> cancelled;
    // Optional cumulative working-buffer allowance for score exports. When a
    // score byte limit is set and this is unset, the default is 1 MiB plus 128
    // times that limit (saturated at UINT64_MAX). It may reject an export whose
    // final compressed bytes would fit. See Document::export_score for what is
    // and is not covered by the workspace estimate.
    std::optional<std::uint64_t> max_working_bytes;
};

struct ExportOptions {
    Format format = Format::MSCZ;
    bool midi_expand_repeats = true;
    bool midi_rpns = true;
    // Master by default; an excerpt is exported as an independent score.
    ScoreSel score;
    CaptureOptions capture;
    // Native MSCX/MSCZ only. 5.00 is the current writer; 4.70 requests the
    // restricted compatibility writer. Unknown versions are Unsupported.
    std::string native_version = "5.00";
};

struct ExportOutput {
    std::vector<std::byte> bytes;
    std::vector<ExportWarning> warnings;
};

// Exact position export in raw 1200-DPI layout units. Coordinates are page-local;
// y and height span the system for measures, and height spans it for segments.
// Segment x/y preserve the legacy truncation to integer units. IDs are local to
// this result, unrelated to ElementId. Page indices are zero-based.
enum struct PositionKind { MEASURE, SEGMENT };

struct PositionElement {
    std::int32_t id = 0;
    double x = 0, y = 0, sx = 0, sy = 0;
    std::int32_t page = 0;
};

struct PositionEvent {
    std::int32_t elid = 0;
    // Repeat-expanded playback time in milliseconds, rounded as the legacy writer.
    std::int32_t position = 0;
};

struct Positions {
    // Size of the first page, matching the legacy pageSize field.
    double page_width = 0, page_height = 0;
    std::vector<PositionElement> elements;
    std::vector<PositionEvent> events;
};

// Dynamic result payload (not allocator overhead or the fixed Positions
// object). The same accounting is used by CaptureOptions::max_bytes.
inline std::uint64_t position_payload_bytes(const Positions &positions) noexcept {
    return static_cast<std::uint64_t>(positions.elements.size()) * sizeof(PositionElement) +
           static_cast<std::uint64_t>(positions.events.size()) * sizeof(PositionEvent);
}

// Index storage used by a bounded position capture; see Document::positions
// for the platform and allocator assumptions behind this estimate.
inline constexpr std::uint64_t position_capture_index_bound(std::uint64_t max_bytes) noexcept {
    return (max_bytes / sizeof(PositionElement)) * (sizeof(void *) + sizeof(std::size_t));
}

// What render() draws.
enum struct RenderFormat {
    PDF, // one document holding the requested pages
    PNG, // one image per page
    SVG, // one drawing per page
};

std::string_view to_string(RenderFormat format) noexcept;

// The renderer's own bounds, the same ones the A2 host enforced: PNG takes 36 to
// 1200 dpi, no page side may exceed 32767 pixels and no page more than 64e6
// pixels. A page beyond them is a ResourceLimit.
inline constexpr std::int32_t min_render_dpi = 36;
inline constexpr std::int32_t max_render_dpi = 1200;
inline constexpr std::int64_t max_render_pixels = 64'000'000;

struct RenderOptions {
    ScoreSel score;
    RenderFormat format = RenderFormat::PDF;
    // Inclusive, 0-based page range. Any negative last_page means the last page.
    std::int32_t first_page = 0;
    std::int32_t last_page = -1;
    // PNG: 36 to 1200 dpi. PDF and SVG draw at 72 dpi and accept only the
    // default 144 sentinel (which is ignored).
    std::int32_t dpi = 144;
    // Draw the page's own background (its colour from the score's style) in PNG
    // and SVG; a PDF always prints it. Off by default, like A2's export.
    bool background = false;
    // PNG only: leave a transparent page transparent instead of painting white
    // behind it.
    bool transparent = true;
    // Same callback contract as CaptureOptions. Polled before drawing, between
    // pages, before encoding, and after encoding. Cancellation returns no output.
    std::function<bool()> cancelled;
};

// What render() produced: exactly one of the two is filled, according to the
// requested format.
struct RenderOutput {
    std::vector<std::byte> document;           // Pdf: the whole document
    std::vector<std::vector<std::byte>> pages; // Png, Svg: one per page, in page order
};

} // namespace engrave
