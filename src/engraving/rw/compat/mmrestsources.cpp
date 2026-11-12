// SPDX-License-Identifier: GPL-3.0-only
#include "mmrestsources.h"

#include "engraving/dom/measure.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"

namespace mu::engraving::rw {
void restore_mm_rest_sources(Score &score) {
    for (Measure *first = score.first_measure(); first; first = first->next_measure()) {
        Measure *display = first->mm_rest();
        if (!display) continue;
        // The owning real measure and saved count are authoritative on load.
        // Display prev/next/ticks can be stale in a source-only transaction snapshot.
        Measure *last = first;
        for (int i = 1; i < display->mm_rest_count() && last->next_measure(); ++i) last = last->next_measure();
        const auto items = display->el();
        for (EngravingItem *item : items) {
            if (item->generated() || item->is_layout_copy()) continue;
            Measure *target = item->is_marker() ? first : last;
            display->remove(item);
            target->add(item);
        }
        for (Segment &segment : display->segments()) {
            const bool at_start = segment.rtick().is_zero();
            Measure *target_measure = at_start ? first : last;
            const Fraction tick = at_start ? Fraction(0, 1) : last->ticks();
            auto target_segment = [&] {
                Segment *target = target_measure->get_segment_r(segment.segment_type(), tick);
                target->set_enabled(segment.enabled());
                target->set_header(segment.header());
                target->set_trailer(segment.trailer());
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
}
} // namespace mu::engraving::rw
