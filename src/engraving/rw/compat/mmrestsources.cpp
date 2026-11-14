// SPDX-License-Identifier: GPL-3.0-only
#include "mmrestsources.h"

#include "engraving/dom/marker.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurenumber.h"
#include "engraving/dom/mmrestrange.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"

namespace mu::engraving::rw {
void restore_mm_rest_sources(Measure &source, Measure &legacy) {
    Measure *first = &source;
    Measure *display = &legacy;
    // The owning real measure and saved count are authoritative on load.
    // Display prev/next/ticks can be stale in a source-only transaction snapshot.
    Measure *last = first;
    for (int i = 1; i < display->mm_rest_count() && last->next_measure(); ++i) last = last->next_measure();
    for (staff_idx_t staff = 0; staff < display->mstaves().size(); ++staff) {
        auto *row = display->mstaves()[staff];
        if (auto *number = row->measure_number(); number && !number->generated() && !first->mstaves()[staff]->measure_number()) {
            display->remove(number);
            first->add(number);
        }
        if (auto *range = row->mm_range_text(); range && !range->generated() && !first->mstaves()[staff]->mm_range_text()) {
            display->remove(range);
            first->add(range);
        }
    }
    const auto items = display->el();
    for (EngravingItem *item : items) {
        if (item->generated() || item->is_layout_copy()) continue;
        // Built-in stop/jump markers apply after the last real measure.
        // Legacy MMR layout also collected right-positioned custom markers
        // from that measure; left markers came from the first measure.
        const auto *marker = item->is_marker() ? to_marker(item) : nullptr;
        const bool at_start = marker && !marker->is_right_marker() && marker->align() != AlignH::RIGHT;
        Measure *target = at_start ? first : last;
        display->remove(item);
        target->add(item);
    }
    for (Segment &segment : display->segments()) {
        const bool at_start = segment.rtick().is_zero();
        Measure *target_measure = at_start ? first : last;
        const Fraction tick = at_start ? Fraction(0, 1) : last->ticks();
        auto target_segment = [&] {
            Segment *target = target_measure->get_segment_r(segment.segment_type(), tick);
            target->set_end_of_measure_change(segment.end_of_measure_change());
            return target;
        };
        const auto elements = segment.elist();
        for (EngravingItem *item : elements) {
            if (!item || item->is_chord_rest() || item->generated() || item->is_layout_copy()) continue;
            Segment *target = target_segment();
            segment.remove(item);
            target->add(item);
        }
        const auto annotations = segment.annotations();
        for (EngravingItem *item : annotations) {
            if (item->generated() || item->is_layout_copy()) continue;
            Segment *target = target_segment();
            segment.remove(item);
            target->add(item);
        }
    }
}
} // namespace mu::engraving::rw
