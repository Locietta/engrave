#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <engrave/export_warning.h>

namespace engrave {

// Failures the immediate caller can act on are returned as Result<T>. Engine
// exceptions during an edit can become EditRejected after successful recovery;
// allocation, callbacks, output failures and failed recovery can throw.
// Contract violations (a call from another thread, a second Engine) abort.
enum struct ErrorCode {
    INVALID_ARGUMENT,   // an option the caller passed is invalid
    INVALID_INPUT,      // the score data is malformed or the importer rejected it
    UNSUPPORTED_FORMAT, // the data matches no format the engine reads
    NOT_FOUND,          // a file, template or element does not exist
    STALE_HANDLE,       // the element an id or address names is no longer part of the score
    RESOURCE_LIMIT,     // an input, capture, or output exceeds a size limit
    UNSUPPORTED,        // a valid request the engine does not implement
    IO,                 // a file I/O operation failed
    REVISION_CONFLICT,  // an edit expected another revision of the document
    EDIT_REJECTED,      // the engine refused an operation; the batch was rolled back
    POISONED,           // an earlier failure left the document unusable; close it
    CANCELLED,          // the caller interrupted an in-progress capture
};

std::string_view to_string(ErrorCode code) noexcept;

struct Error {
    ErrorCode code = ErrorCode::INVALID_ARGUMENT;
    std::string message;
    std::optional<std::size_t> op_index; // the failing operation of an edit batch
    // Present when an export is refused to prevent unreported loss.
    std::vector<ExportWarning> export_warnings;
};

template <typename T>
using Result = std::expected<T, Error>;

enum struct Severity {
    INFO,
    WARNING,
};

// A problem that did not stop the call, e.g. a score that failed the engine's
// sanity check on load.
struct Diagnostic {
    Severity severity = Severity::INFO;
    std::string message;
};

} // namespace engrave
