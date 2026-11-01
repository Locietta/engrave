#pragma once

#include <optional>
#include <string>
#include <utility>

#include <engrave/handle.h>

namespace engrave {
namespace export_warning_code {
inline constexpr char shared_part[] = "B470_SHARED_PART_DROPPED";
inline constexpr char stave_sharing_label[] = "B470_STAVE_SHARING_LABEL_DROPPED";
inline constexpr char page_lock[] = "B470_PAGE_LOCK_DROPPED";
inline constexpr char group_bracket[] = "B470_GROUP_BRACKET_DROPPED";
inline constexpr char instrument_group_name[] = "B470_INSTRUMENT_GROUP_NAME_DROPPED";
inline constexpr char invalid_tie[] = "B470_INVALID_TIE";
inline constexpr char unsupported_construct[] = "B470_UNSUPPORTED_FEATURE";
inline constexpr char unsupported_target[] = "B470_UNSUPPORTED_TARGET";
// Compatibility names for constructs without a separate frozen warning code.
inline constexpr auto& part_identity_omitted = unsupported_construct;
inline constexpr auto& excerpt = unsupported_construct;
inline constexpr auto& automation = unsupported_construct;
inline constexpr auto& custom_chord_list = unsupported_construct;
inline constexpr auto& unsupported_style = unsupported_construct;
}

struct ExportWarningObject {
    std::string kind;
    // Source EID, documented composite key, or explicitly non-EID locator:...
    // (see WRITE470.md). B470 producers require both nonempty fields.
    std::string id;
    bool operator==(const ExportWarningObject&) const = default;
};

// Retained as a convenience for callers displaying only the identifier.
// Protocol serializers must emit both object.kind and object.id.
inline const std::string& to_string(const ExportWarningObject& object) { return object.id; }

struct ExportWarning {
    std::string code;
    std::string message;
    std::optional<ExportWarningObject> object;

    ExportWarning() = default;

    ExportWarning(std::string code_, std::string message_, std::optional<ElementId> source)
        : code(std::move(code_)), message(std::move(message_)) {
        if (!source) return;
        std::string kind = "score";
        if (code == export_warning_code::shared_part) kind = "shared_part";
        else if (code == export_warning_code::stave_sharing_label) kind = "stave_sharing_label";
        else if (code == export_warning_code::page_lock) kind = "page_lock";
        else if (code == export_warning_code::group_bracket) kind = "group_bracket";
        else if (code == export_warning_code::instrument_group_name) kind = "part";
        else if (code == export_warning_code::invalid_tie) kind = "tie";
        object = ExportWarningObject{std::move(kind), to_string(*source)};
    }
    ExportWarning(std::string code_, std::string message_, ExportWarningObject source)
        : code(std::move(code_)), message(std::move(message_)), object(std::move(source)) {}
};

} // namespace engrave
