/* SPDX-License-Identifier: GPL-3.0-only */
#include <array>

#include "dom/chord.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "global/hashmap.h"
#include "infrastructure/domchildren.h"
#include "rendering/score/layoutcontext.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

struct LayoutContext::GraceGroups {
    using Groups = std::array<std::unique_ptr<GraceNotesGroup>, 2>;
    muse::HashMap<const Chord *, Groups> chords;
};

LayoutContext::LayoutContext(Score *score) : score_(score), configuration_(this), dom_(this) {
    if (score) {
        state_.set_first_system_indent(score->style().style_b(Sid::ENABLE_INDENTATION_ON_FIRST_SYSTEM));
    }
}

LayoutContext::~LayoutContext() {
    if (grace_groups_) {
        clear_grace_attachments(score_);
    }
}

GraceNotesGroup &LayoutContext::update_grace_group(Chord *chord, bool after) {
    if (!grace_groups_) {
        grace_groups_ = std::make_unique<GraceGroups>();
    }
    auto &group = grace_groups_->chords[chord][after ? 1 : 0];
    if (!group) {
        group = std::make_unique<GraceNotesGroup>(chord);
    }
    const auto notes = after ? chord->grace_notes_after() : chord->grace_notes_before();
    group->assign(notes.begin(), notes.end());
    return *group;
}

const GraceNotesGroup *LayoutContext::grace_group(const Chord *chord, bool after) const {
    if (!grace_groups_) {
        return nullptr;
    }
    const auto it = grace_groups_->chords.find(chord);
    return it == grace_groups_->chords.end() ? nullptr : it->second[after ? 1 : 0].get();
}

void LayoutContext::clear_grace_attachments(EngravingObject *object) const {
    if (!object) {
        return;
    }
    if (object->is_segment()) {
        Segment *segment = to_segment(object);
        for (track_idx_t track = 0; track < segment->pre_appended_items().size(); ++track) {
            EngravingItem *item = segment->pre_appended_item(track);
            if (!item || !item->is_grace_notes_group()) {
                continue;
            }
            const GraceNotesGroup *group = to_grace_notes_group(item);
            if (grace_group(group->parent(), false) == group || grace_group(group->parent(), true) == group) {
                segment->clear_pre_appended(track);
            }
        }
    }
    if (object->is_engraving_item()) {
        auto *item = to_engraving_item(object);
        if (item->has_layout_data()) {
            for_each_layout_payload_child(*item, *item->ldata(), [&](EngravingItem *child) {
                if (child) clear_grace_attachments(child);
            });
        }
    }
    // Traverse live ownership, not saved Segment pointers: layout may have moved
    // or deleted an attached segment. This also reaches retained undo objects.
    for (EngravingObject *child : object->children()) {
        clear_grace_attachments(child);
    }
}
