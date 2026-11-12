/* SPDX-License-Identifier: GPL-3.0-only */
#include "measurestaffchange.h"

#include <stdexcept>

#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurenumber.h"
#include "dom/mmrestrange.h"
#include "dom/spacer.h"
#include "dom/stafflines.h"
#include "transaction.h"

namespace mu::engraving {
MStaffChildren capture_mstaff_children(MStaff &row, MasterScore &document) {
    const std::array<EngravingObject *, 5> objects{row.measure_number(), row.mm_range_text(), row.lines(), row.vspacer_up(),
                                                   row.vspacer_down()};
    MStaffChildren result{};
    for (size_t i = 0; i < objects.size(); ++i) {
        if (!objects[i]) continue;
        if (objects[i]->master_score() != &document) throw std::logic_error("measure staff children cannot span documents");
        result[i] = document.eid_register()->enroll(*objects[i]);
    }
    return result;
}

void apply_mstaff_membership(MStaffReference &reference, const MStaffMembershipInverse &entry, bool adding) {
    auto *row = reference.object;
    if (!row) throw std::logic_error("measure staff lifetime ended before rollback");
    auto &measure = *to_measure(&entry.measure.object());
    reference.owner = entry.measure;
    const auto found = muse::index_of(measure.mstaves(), row);
    if (adding) {
        // A failed insertion may not have transferred collection ownership yet.
        if (found == muse::nidx) measure.insert_m_staff(row, entry.index);
    } else {
        // Display children can be replaced while the row is attached. Capture
        // their current lifetimes on every detach, including inverse replay.
        reference.children = capture_mstaff_children(*row, *reference.owner.document);
        if (found != muse::nidx) measure.remove_m_staff(row, found);
    }
}

void dispose_detached_mstaff(MStaff &row, const JournalTarget &owner, const MStaffChildren &children) {
    auto &registry = *owner.document->eid_register();
    auto *measure = registry.item_from_handle(owner.handle);
    if (measure && muse::contains(to_measure(measure)->mstaves(), &row)) return;
    // A deleted owner moves children to its dummy parent; excerpt destruction
    // can destroy them before this detached row. Never inspect retired objects.
    if (registry.item_from_handle(children[0]) != row.measure_number()) row.set_measure_number(nullptr);
    if (registry.item_from_handle(children[1]) != row.mm_range_text()) row.set_mm_range_text(nullptr);
    if (registry.item_from_handle(children[2]) != row.lines()) row.set_lines(nullptr);
    if (registry.item_from_handle(children[3]) != row.vspacer_up()) row.set_vspacer_up(nullptr);
    if (registry.item_from_handle(children[4]) != row.vspacer_down()) row.set_vspacer_down(nullptr);
    delete &row;
}

void apply_staff_tracks(Measure &measure, staff_idx_t first, staff_idx_t last, bool adding) {
    if (adding)
        measure.insert_staves(first, last);
    else
        measure.remove_staves(first, last);
}

void Score::record_mstaff(Measure *measure, MStaff *row, staff_idx_t index, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_mstaff(measure, row, index, adding);
}

void Score::record_mstaff_add(Measure *measure, std::unique_ptr<MStaff> row, staff_idx_t index) {
    transaction_manager()->current_or_dummy_transaction().add_mstaff(measure, std::move(row), index);
}

void Score::record_staff_tracks(Measure *measure, staff_idx_t first, staff_idx_t last, bool adding) {
    transaction_manager()->current_or_dummy_transaction().change_staff_tracks(measure, first, last, adding);
}
} // namespace mu::engraving
