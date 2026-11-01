#pragma once

#include <optional>
#include <string>

#include <engrave/handle.h>

namespace engrave {

// These identifiers are part of the public export contract.
namespace export_warning_code {
inline constexpr char part_identity_omitted[] = "engrave.470.part_identity_omitted";
inline constexpr char page_lock[] = "engrave.470.page_lock";
inline constexpr char shared_part[] = "engrave.470.shared_part";
inline constexpr char stave_sharing_label[] = "engrave.470.stave_sharing_label";
inline constexpr char group_bracket[] = "engrave.470.group_bracket";
inline constexpr char excerpt[] = "engrave.470.excerpt";
inline constexpr char automation[] = "engrave.470.automation";
inline constexpr char custom_chord_list[] = "engrave.470.custom_chord_list";
inline constexpr char unsupported_style[] = "engrave.470.unsupported_style";
inline constexpr char unsupported_construct[] = "engrave.470.unsupported_construct";
}

// Codes are stable protocol identifiers; messages are for people and may change.
// An object id, when present, identifies the source document object, not an
// object in the exported file. A range lock is identified by its first measure.
struct ExportWarning {
    std::string code;
    std::string message;
    std::optional<ElementId> object;
};

} // namespace engrave
