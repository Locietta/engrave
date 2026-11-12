/* SPDX-License-Identifier: GPL-3.0-only */
#include "timingchange.h"

#include "dom/score.h"
#include "transaction.h"

namespace mu::engraving {
void Score::record_time_insertion(Score *target, Fraction tick, Fraction length, bool unmanaged) {
    transaction_manager()->current_or_dummy_transaction().insert_time(*target, tick, length, unmanaged);
}

void Score::record_measures(MeasureBase *first, MeasureBase *last, bool adding, bool move_staff_type_changes) {
    transaction_manager()->current_or_dummy_transaction().change_measures(first, last, adding, move_staff_type_changes);
}

void Score::record_measure_length(Measure *measure, Fraction length) {
    transaction_manager()->current_or_dummy_transaction().change_measure_length(measure, length);
}

void Score::record_measure_repeat_count(Measure *measure, int count, staff_idx_t staff) {
    transaction_manager()->current_or_dummy_transaction().change_measure_repeat_count(measure, count, staff);
}
} // namespace mu::engraving
