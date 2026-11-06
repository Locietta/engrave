#include <cstdio>
#include <cstdlib>

#include "engrave/src/api/state.h"
#include <engrave/audio.h>
#include <engrave/error.h>
#include <engrave/output.h>

namespace engrave {

void check(bool condition, std::string_view message) noexcept {
    if (condition) [[likely]]
        return;
    std::fprintf(stderr, "engrave: contract violation: %.*s\n", static_cast<int>(message.size()), message.data());
    std::fflush(stderr);
    std::abort();
}

std::string utf8(const std::filesystem::path &path) {
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
}

std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::INVALID_ARGUMENT: return "InvalidArgument";
        case ErrorCode::INVALID_INPUT: return "InvalidInput";
        case ErrorCode::UNSUPPORTED_FORMAT: return "UnsupportedFormat";
        case ErrorCode::NOT_FOUND: return "NotFound";
        case ErrorCode::STALE_HANDLE: return "StaleHandle";
        case ErrorCode::RESOURCE_LIMIT: return "ResourceLimit";
        case ErrorCode::UNSUPPORTED: return "Unsupported";
        case ErrorCode::IO: return "Io";
        case ErrorCode::REVISION_CONFLICT: return "RevisionConflict";
        case ErrorCode::EDIT_REJECTED: return "EditRejected";
        case ErrorCode::POISONED: return "Poisoned";
        case ErrorCode::CANCELLED: return "Cancelled";
    }
    return "Unknown";
}

std::string_view to_string(Format format) noexcept {
    switch (format) {
        case Format::MSCZ: return "MSCZ";
        case Format::MSCX: return "MSCX";
        case Format::MUSIC_XML: return "MusicXML";
        case Format::MXL: return "MXL";
        case Format::MIDI: return "MIDI";
        case Format::GUITAR_PRO: return "Guitar Pro";
        case Format::MNX: return "MNX";
    }
    return "Unknown";
}

std::string_view to_string(RenderFormat format) noexcept {
    switch (format) {
        case RenderFormat::PDF: return "PDF";
        case RenderFormat::PNG: return "PNG";
        case RenderFormat::SVG: return "SVG";
    }
    return "Unknown";
}

std::string_view to_string(AudioFormat format) noexcept {
    switch (format) {
        case AudioFormat::WAV: return "WAV";
        case AudioFormat::FLAC: return "FLAC";
        case AudioFormat::OGG: return "OGG";
        case AudioFormat::OPUS: return "Opus";
        case AudioFormat::MP3: return "MP3";
        case AudioFormat::AAC: return "AAC";
    }
    return "Unknown";
}

} // namespace engrave
