/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <memory>

#include "dom/linkedobjects.h"
#include "sourcechange.h"

namespace mu::engraving {
struct ElementMembershipInverse {
    JournalTarget target;
};

struct ElementReplacementInverse {
    JournalTarget before;
    JournalTarget after;
};

struct ElementParentInverse {
    JournalTarget target;
    JournalTarget parent;
    track_idx_t track;
};

struct SegmentParentInverse {
    JournalTarget target;
    JournalTarget parent;
    Fraction tick;
};

struct LinkInverse {
    JournalTarget target;
    LinkedObjects *group = nullptr;
    std::unique_ptr<LinkedObjects> owned_group;
    size_t position = 0;
    bool adding = false;
    bool new_group = false;
    bool applied = false;
};

LinkInverse capture_link(EngravingObject &object, EngravingObject *peer);
void apply_link(LinkInverse &inverse);
void restore_link(LinkInverse &inverse);
void dispose_element_membership(const ElementMembershipInverse &inverse, bool adding, bool committed);
} // namespace mu::engraving
