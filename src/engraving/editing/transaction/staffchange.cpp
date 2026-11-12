/* SPDX-License-Identifier: GPL-3.0-only */
#include "staffchange.h"

#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/staff.h"
#include "editing/editstaffbrackets.h"
#include "transaction.h"

namespace mu::engraving {
amc::vector<BracketValue> capture_brackets(const Staff &staff) {
    amc::vector<BracketValue> values;
    for (auto *item : staff.score()->brackets(staff.idx()))
        values.push_back({item->bracket_type(), item->column(), item->bracket_span(), item->label(), item->visible(), item->show_text(),
                          item->show_bracket()});
    return values;
}

void restore_brackets(Staff &staff, const amc::vector<BracketValue> &values) {
    auto &score = *staff.score();
    auto &items = score.brackets(staff.idx());
    if (!values.empty()) EditStaffBrackets::fill_brackets(&score, staff.idx(), values.size() - 1);
    while (items.size() > values.size()) {
        delete items.back();
        items.pop_back();
    }
    for (size_t i = 0; i < values.size(); ++i) {
        auto &item = *items[i];
        const auto &value = values[i];
        item.set_bracket_type(value.type);
        item.set_column(value.column);
        item.set_bracket_span(value.span);
        item.label() = value.label;
        item.set_visible(value.visible);
        item.set_property(Pid::GROUP_BRACKET_SHOW_TEXT, value.show_text);
        item.set_property(Pid::GROUP_BRACKET_SHOW_BRACKET, value.show_bracket);
    }
}

void apply_staff_membership(Staff &staff, staff_idx_t index, bool adding) {
    if (adding)
        staff.score()->insert_staff(&staff, index);
    else
        staff.score()->remove_staff(&staff);
}

void restore_staff_membership(const StaffMembershipInverse &inverse) {
    auto &staff = *to_staff(&inverse.target.object());
    apply_staff_membership(staff, inverse.index, !inverse.adding);
    if (!inverse.adding) {
        restore_brackets(staff, inverse.brackets);
        if (inverse.system_position != muse::nidx) staff.score()->add_system_object_staff(&staff, inverse.system_position);
    }
}

void dispose_staff_membership(const StaffMembershipInverse &inverse, bool committed) {
    if (inverse.adding == committed) return;
    auto *object = inverse.target.document->eid_register()->item_from_handle(inverse.target.handle);
    if (!object) return;
    auto *staff = to_staff(object);
    // Score ownership survives detaching its excerpt from the master.
    if (muse::contains(staff->score()->staves(), staff)) return;
    delete staff;
}

void apply_system_staff(Staff &staff, bool adding) {
    if (adding)
        staff.score()->add_system_object_staff(&staff);
    else
        staff.score()->remove_system_object_staff(&staff);
}

void restore_system_staff(const SystemStaffInverse &inverse) {
    auto &staff = *to_staff(&inverse.target.object());
    staff.score()->remove_system_object_staff(&staff);
    if (inverse.position != muse::nidx) staff.score()->add_system_object_staff(&staff, inverse.position);
}

void apply_staff_hide(Measure &measure, staff_idx_t staff, AutoOnOff value) {
    measure.set_hide_staff_if_empty(staff, value);
    measure.trigger_layout(staff);
}

void apply_staff_brackets(Staff &staff, size_t level, BracketType type, size_t span, bool adding) {
    EditStaffBrackets::set_bracket_type(staff.score(), staff.idx(), level, adding ? type : BracketType::NO_BRACKET);
    if (adding) EditStaffBrackets::set_bracket_span(staff.score(), staff.idx(), level, span);
    staff.trigger_layout();
}

void Score::record_bracket_add(Staff *staff, size_t level, BracketType type, size_t span) {
    transaction_manager()->current_or_dummy_transaction().change_staff_brackets(staff, level, type, span, true);
}

void Score::record_bracket_remove(Staff *staff, size_t level) {
    transaction_manager()->current_or_dummy_transaction().change_staff_brackets(staff, level, BracketType::NO_BRACKET, 0, false);
}

void Score::record_staff_membership(Staff *staff, staff_idx_t index, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_staff_membership(staff, index, adding);
}

void Score::record_system_staff(Staff *staff, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_system_staff(staff, adding);
}

void Score::record_staff_order(const std::vector<staff_idx_t> &order) {
    transaction_manager()->current_or_dummy_transaction().change_staff_order(this, order);
}

void Score::record_staff_hide(Measure *measure, staff_idx_t staff, AutoOnOff value) {
    transaction_manager()->current_or_dummy_transaction().change_staff_hide(measure, staff, value);
}
} // namespace mu::engraving
