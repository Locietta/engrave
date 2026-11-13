/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "sourcechange.h"

namespace mu::engraving {
struct Measure;
struct MeasureBase;

enum class MMRestLink { ASSOCIATION, NEXT, PREVIOUS };

// Transitional display topology. No EIDs are assigned to these retained views.
// Creation ownership is separate from restoring an existing overlay's links.
struct MMRestLinkInverse {
    JournalTarget target;
    SourceHandle before;
    MMRestLink link;
};

MeasureBase *mm_rest_link(const Measure &target, MMRestLink link);
void validate_mm_rest_link(const Measure &target, MeasureBase *value, MMRestLink link);
void apply_mm_rest_link(Measure &target, MeasureBase *value, MMRestLink link);
void restore_mm_rest_link(const MMRestLinkInverse &inverse);

// Existing overlay lifetimes must be retained by their owner across the batch.
// New overlays require the creation guard before migrating layout callers.
void record_mm_rest_association(Measure &source, Measure *overlay);
void record_mm_rest_next(Measure &overlay, MeasureBase *next);
void record_mm_rest_previous(Measure &overlay, MeasureBase *previous);
} // namespace mu::engraving
