/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <array>

#include "sourcechange.h"

namespace mu::engraving {
struct MStaff;
struct Measure;

using MStaffChildren = std::array<SourceHandle, 5>;

struct MStaffReference {
    MStaff *object;
    JournalTarget owner;
    MStaffChildren children;
    bool retained = false;
};

struct MStaffMembershipInverse {
    JournalTarget measure;
    uint32_t reference;
    staff_idx_t index;
    bool adding;
};

struct StaffTracksInverse {
    JournalTarget measure;
    staff_idx_t first;
    staff_idx_t last;
    bool adding;
};

MStaffChildren capture_mstaff_children(MStaff &row, MasterScore &document);
void apply_mstaff_membership(MStaffReference &row, const MStaffMembershipInverse &entry, bool adding);
void dispose_detached_mstaff(MStaff &row, const JournalTarget &owner, const MStaffChildren &children);
void apply_staff_tracks(Measure &measure, staff_idx_t first, staff_idx_t last, bool adding);
} // namespace mu::engraving
