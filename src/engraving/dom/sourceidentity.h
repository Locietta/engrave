// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <vector>

#include "engravingitem.h"
#include "measure.h"
#include "segment.h"

namespace mu::engraving {
// S4 removes display/source reparenting. Until then, read source ownership
// through the real measure chain even when an existing MMR is stale or disabled.
inline Fraction source_tick(const EngravingItem &item) {
    const Measure *measure = item.find_measure();
    if (!measure || !measure->is_mm_rest()) return item.tick();
    if (!item.is_segment() && item.ownership_parent() == measure)
        return item.is_marker() ? measure->mm_rest_first()->tick() : measure->mm_rest_last()->tick();
    const Fraction relative = item.tick() - measure->tick();
    return relative == measure->ticks() ? measure->mm_rest_last()->end_tick() : measure->mm_rest_first()->tick() + relative;
}

inline std::vector<EngravingItem *> source_measure_elements(const Measure &measure, const Measure *display) {
    std::vector<EngravingItem *> result(measure.el().begin(), measure.el().end());
    if (display) {
        for (auto *item : display->el()) {
            const auto *owner = item->is_marker() ? display->mm_rest_first() : display->mm_rest_last();
            if (owner == &measure) result.push_back(item);
        }
    }
    return result;
}

inline std::vector<EngravingItem *> source_annotations(const Segment &segment) {
    std::vector<EngravingItem *> result(segment.annotations().begin(), segment.annotations().end());
    // MMR layout moves annotations only from the first underlying CR segment.
    const Measure *measure = segment.measure();
    if (segment.is_chord_rest_type() && segment.rtick().is_zero() && measure->mm_rest()) {
        if (const auto *display = measure->mm_rest()->first(SegmentType::CHORD_REST))
            result.insert(result.end(), display->annotations().begin(), display->annotations().end());
    }
    return result;
}
} // namespace mu::engraving
