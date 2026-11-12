/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <vector>

#include "derivedchange.h"

namespace mu::engraving {
struct Staff;
struct Measure;

// BracketItem lifetimes are replaceable. Preserve authored values by staff slot,
// including placeholder columns and properties that the legacy commands lost.
struct BracketValue {
    BracketType type;
    size_t column;
    size_t span;
    StaffLabel label;
    bool visible;
    bool show_text;
    bool show_bracket;

    bool operator==(const BracketValue &) const = default;
};

struct StaffMembershipInverse {
    JournalTarget target;
    staff_idx_t index;
    size_t system_position;
    bool adding;
    amc::vector<BracketValue> brackets;
};

struct SystemStaffInverse {
    JournalTarget target;
    size_t position;
    bool adding;
};

struct StaffSortInverse {
    JournalTarget target;
    std::vector<staff_idx_t> before;
};

struct StaffHideInverse {
    JournalTarget target;
    staff_idx_t index;
    AutoOnOff before;
};

amc::vector<BracketValue> capture_brackets(const Staff &staff);
void restore_brackets(Staff &staff, const amc::vector<BracketValue> &values);
void apply_staff_membership(Staff &staff, staff_idx_t index, bool adding);
void restore_staff_membership(const StaffMembershipInverse &inverse);
void dispose_staff_membership(const StaffMembershipInverse &inverse, bool committed);
void apply_system_staff(Staff &staff, bool adding);
void restore_system_staff(const SystemStaffInverse &inverse);
void apply_staff_hide(Measure &measure, staff_idx_t staff, AutoOnOff value);
} // namespace mu::engraving
