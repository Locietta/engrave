// SetProperty, ResetProperty and SetStyle: the engine's own undoable property
// and style changes.

#include "engraving/dom/property.h"

#include <format>
#include <string>

#include "engrave/src/api/batch.h"
#include "engrave/src/api/pid_support.h"
#include "engrave/src/api/value_codec.h"
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/score.h"
#include "engraving/style/style.h"
#include "engraving/style/styledef.h"

namespace engrave::api {
namespace {

using mu::engraving::EngravingObject;
using mu::engraving::Pid;
using mu::engraving::PropertyFlags;

std::unexpected<Error> invalid(std::string message) { return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::move(message)}); }

Result<Pid> property_id(const std::string &name) {
    const Pid pid = pid_of(name);
    if (pid == Pid::END) return invalid(std::format("no property '{}'", name));
    return pid;
}

// The element of a step, provided its type lets an edit set the property.
Result<EngravingObject *> settable_object(const EditContext &context, const Target &target, Pid pid) {
    auto object = context.object(target);
    if (!object) return object;
    const std::optional<bool> listed = settable((*object)->type_name(), pid);
    if (!listed) return invalid(std::format("a {} has no property '{}'", (*object)->type_name(), name_of(pid)));
    if (!*listed)
        return invalid(std::format("'{}' of a {} is changed by structural operations only", name_of(pid), (*object)->type_name()));
    return object;
}

} // namespace

Result<Step> prepare(EditContext &context, const SetProperty &op) {
    auto target = context.target(op.target);
    if (!target) return std::unexpected(target.error());
    const auto pid = property_id(op.property);
    if (!pid) return std::unexpected(pid.error());
    auto value = from_value(mu::engraving::property_type(*pid), op.value);
    if (!value) return std::unexpected(value.error());
    return [target = *target, pid = *pid, value = *value](EditContext &context) -> Result<void> {
        const auto object = settable_object(context, target, pid);
        if (!object) return std::unexpected(object.error());
        // A styled property that is given a value of its own stops following the style.
        const PropertyFlags flags = (*object)->property_flags(pid);
        (*object)->undo_change_property(pid, value, flags == PropertyFlags::STYLED ? PropertyFlags::UNSTYLED : flags);
        return {};
    };
}

Result<Step> prepare(EditContext &context, const ResetProperty &op) {
    auto target = context.target(op.target);
    if (!target) return std::unexpected(target.error());
    const auto pid = property_id(op.property);
    if (!pid) return std::unexpected(pid.error());
    return [target = *target, pid = *pid](EditContext &context) -> Result<void> {
        const auto object = settable_object(context, target, pid);
        if (!object) return std::unexpected(object.error());
        (*object)->undo_reset_property(pid);
        return {};
    };
}

Result<Step> prepare(EditContext &context, const SetStyle &op) {
    if (auto score = context.score(op.score); !score) return std::unexpected(score.error());
    const mu::engraving::Sid sid = mu::engraving::MStyle::style_idx(muse::String::from_std_string(op.style));
    if (sid == mu::engraving::Sid::NOSTYLE) return invalid(std::format("no style '{}'", op.style));
    auto value = from_value(mu::engraving::MStyle::value_type(sid), op.value);
    if (!value) return std::unexpected(value.error());
    // The score is looked up again when the step runs: an earlier operation may
    // have removed the excerpt.
    return [selection = op.score, sid, value = *value](EditContext &context) -> Result<void> {
        auto score = context.score(selection);
        if (!score) return std::unexpected(score.error());
        (*score)->undo_change_style_val(sid, value);
        return {};
    };
}

} // namespace engrave::api
