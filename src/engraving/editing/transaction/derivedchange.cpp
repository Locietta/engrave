/* SPDX-License-Identifier: GPL-3.0-only */
#include "derivedchange.h"

#include <algorithm>
#include <stdexcept>

#include "dom/score.h"
#include "dom/staff.h"
#include "dom/textline.h"
#include "editing/editstaffbrackets.h"

namespace mu::engraving {
BracketPropertyInverse capture_bracket_property(BracketItem &item, Pid id) {
    const auto &brackets = item.score()->brackets(item.start_staff_idx());
    const auto level = static_cast<size_t>(std::ranges::find(brackets, &item) - brackets.begin());
    if (level == brackets.size()) throw std::logic_error("a bracket property target is absent from its staff");
    BracketPropertyInverse result{
        JournalTarget::capture(*item.start_staff()), level, level, id, item.get_property(id), item.property_flags(id), {}};
    for (auto *bracket : brackets) result.columns.push_back(bracket->column());
    return result;
}

void apply_bracket_property(BracketPropertyInverse &inverse, const PropertyValue &value, PropertyFlags flags) {
    auto &staff = *to_staff(&inverse.staff.object());
    auto &score = *staff.score();
    auto &item = *score.brackets(staff.idx()).at(inverse.level);
    // Bracket items are replaceable display objects, not source identities.
    // Legacy bracket/staff commands may recreate them before this inverse runs.
    // Save the location before a setter that may delete or move the item.
    item.set_property_flags(inverse.id, flags);
    item.set_property(inverse.id, value);
    if (inverse.id == Pid::BRACKET_COLUMN) inverse.applied_level = static_cast<size_t>(value.to_int());
}

void restore_bracket_property(BracketPropertyInverse &inverse) {
    auto &staff = *to_staff(&inverse.staff.object());
    EditStaffBrackets::fill_brackets(staff.score(), staff.idx(), inverse.applied_level);
    auto &brackets = staff.score()->brackets(staff.idx());
    auto &item = *brackets.at(inverse.applied_level);
    item.set_property_flags(inverse.id, inverse.flags);
    // applied_level stays at the original slot if padding allocation failed
    // before the forward move. No pointer to a replaceable bracket is retained.
    item.set_property(inverse.id, inverse.before);
    while (brackets.size() > inverse.columns.size() && brackets.back()->bracket_type() == BracketType::NO_BRACKET) {
        delete brackets.back();
        brackets.pop_back();
    }
    if (!inverse.columns.empty()) EditStaffBrackets::fill_brackets(staff.score(), staff.idx(), inverse.columns.size() - 1);
    for (size_t i = 0; i < inverse.columns.size(); ++i) brackets[i]->set_column(inverse.columns[i]);
    staff.score()->set_layout_all();
}

TextLinePropertyInverse capture_text_line_property(EngravingObject &object) {
    TextLinePropertyInverse result{
        {JournalTarget::capture(object), Pid::SYSTEM_FLAG, object.get_property(Pid::SYSTEM_FLAG), object.property_flags(Pid::SYSTEM_FLAG)},
        {}};
    if (object.is_text_line()) {
        for (bool system : {false, true})
            for (const auto &property : TextLine::style_for_system(system))
                if (std::ranges::none_of(result.styled, [&](const auto &before) { return before.id == property.pid; }))
                    result.styled.push_back({property.pid, object.get_property(property.pid), object.property_flags(property.pid)});
    }
    return result;
}

void apply_text_line_property(EngravingObject &object, const PropertyValue &value) {
    apply_property(object, Pid::SYSTEM_FLAG, value, PropertyFlags::NOSTYLE, false);
    if (object.is_text_line()) to_text_line(&object)->init_style();
}

void restore_text_line_property(const TextLinePropertyInverse &inverse) {
    auto &object = inverse.property.target.object();
    apply_text_line_property(object, inverse.property.before);
    for (const auto &property : inverse.styled) {
        object.set_property(property.id, property.value);
        object.set_property_flags(property.id, property.flags);
    }
    object.set_property_flags(Pid::SYSTEM_FLAG, inverse.property.flags);
}
} // namespace mu::engraving
