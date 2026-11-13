/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <memory>

#include "sourcechange.h"

namespace mu::engraving {
struct Measure;
struct MeasureBase;
struct JournalBatch;
struct Transaction;

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

struct CreatedMMRest {
    JournalTarget source;
    SourceHandle object;
    std::unique_ptr<Measure> owned;
};

void dispose_created_mm_rest(CreatedMMRest &reference, bool committed);

// Keeps incomplete topology out of subsequent native saves, including when
// layout runs without a batch. A batch retains the new lifetime for its inverses.
struct MMRestCreation {
    MMRestCreation();
    ~MMRestCreation();
    MMRestCreation(MMRestCreation &&other) noexcept;
    MMRestCreation &operator=(MMRestCreation &&other) noexcept;
    MMRestCreation(const MMRestCreation &) = delete;
    MMRestCreation &operator=(const MMRestCreation &) = delete;

    Measure *get() const;
    void complete();

private:
    friend struct JournalBatch;
    friend struct Transaction;
    explicit MMRestCreation(CreatedMMRest reference);
    MMRestCreation(JournalTarget source, SourceHandle object);
    void cancel() noexcept;

    CreatedMMRest local_;
    bool completed_ = false;
};

MMRestCreation begin_mm_rest_creation(Measure &source, Fraction length);
} // namespace mu::engraving
