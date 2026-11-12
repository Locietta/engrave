/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "sourcechange.h"

namespace mu::engraving {
struct TimeInsertionInverse {
    JournalTarget target;
    Fraction tick;
    Fraction length;
    bool unmanaged = false;
};

struct MeasureMembershipInverse {
    JournalTarget first;
    JournalTarget last;
    bool adding;
    bool move_staff_type_changes;
};

struct MeasureLengthInverse {
    JournalTarget target;
    Fraction before;
};

struct MeasureRepeatCountInverse {
    JournalTarget target;
    int before;
    staff_idx_t staff;
};
} // namespace mu::engraving
