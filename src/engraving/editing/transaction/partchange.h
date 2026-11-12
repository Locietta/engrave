/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <memory>

#include "dom/instrument.h"
#include "dom/stringdata.h"
#include "sourcechange.h"

namespace mu::engraving {
struct Part;
struct Excerpt;
struct SharedPart;
struct StringTunings;

struct PartMembershipInverse {
    JournalTarget target;
    size_t index;
    bool adding;
};

struct PartLabelInverse {
    JournalTarget target;
    Fraction tick;
    String before;
    bool long_name;
};

struct PartInstrumentInverse {
    JournalTarget target;
    std::unique_ptr<Instrument> displaced;
    bool applied = false;
};

struct StringDataInverse {
    JournalTarget target;
    StringData before;
};

struct SharedOriginInverse {
    JournalTarget target;
    JournalTarget origin;
    size_t position;
    bool present;
};

// Excerpts are descriptors, including descriptors without a materialized Score.
// Entries reference this batch-owned retention array by index. Destruction hooks
// invalidate borrowed descriptor addresses before any inverse can dereference them.
struct ExcerptReference {
    Excerpt *object;
    JournalTarget master;
    bool retained = false;
};

struct ExcerptMembershipInverse {
    uint32_t reference;
    size_t index;
    bool adding;
};

void apply_part_membership(Part &part, size_t index, bool adding);
Part &journal_part(const JournalTarget &target);
void dispose_part_membership(const PartMembershipInverse &inverse, bool committed);
void apply_part_label(Part &part, Fraction tick, const String &value, bool long_name);
void exchange_part_instrument(Part &part, std::unique_ptr<Instrument> &held);
void refresh_part_instrument(Part &part);
void restore_part_instrument(PartInstrumentInverse &inverse);
void restore_shared_origin(const SharedOriginInverse &inverse);
} // namespace mu::engraving
