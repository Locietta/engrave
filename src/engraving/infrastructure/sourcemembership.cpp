#include "dom/accidental.h"
#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/beam.h"
#include "dom/box.h"
#include "dom/chord.h"
#include "dom/engravingitem.h"
#include "dom/lyrics.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/text.h"
#include "dom/tie.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/tuplet.h"
#include "sourcehandle.h"

namespace mu::engraving {

bool source_object(const EngravingObject &item) {
    // Output remains outside membership even when it is parented to source.
    // Automatic beam ownership is still handled by the separate beam slice.
    for (const EngravingObject *node = &item; node; node = node->ownership_parent()) {
        switch (node->type()) {
            case ElementType::SYSTEM:
            case ElementType::PAGE:
            case ElementType::BRACKET:
            case ElementType::BRACKET_ITEM:
            case ElementType::INSTRUMENT_NAME:
            case ElementType::MMREST_RANGE:
            case ElementType::MEASURE_NUMBER:
            case ElementType::STAFF_LINES:
            case ElementType::LEDGER_LINE:
            case ElementType::STEM:
            case ElementType::STEM_SLASH:
            case ElementType::HOOK:
            case ElementType::NOTEDOT:
            case ElementType::GUITAR_BEND_TEXT:
            case ElementType::GRACE_NOTES_GROUP:
            case ElementType::FIGURED_BASS_ITEM:
            case ElementType::PARENTHESIS:
            case ElementType::ROOT_ITEM:
            case ElementType::DUMMY: return false;
            default: break;
        }
        if (node->is_segment()) {
            auto *owner = node->ownership_parent();
            if (!owner || !owner->is_measure() || to_measure(owner)->owns_output_segment(to_segment(node))) return false;
        }
        if (node->ownership_parent() && node->ownership_parent()->is_segment() && node->is_engraving_item() &&
            to_segment(node->ownership_parent())->owns_generated(to_engraving_item(node)))
            return false;
        if (node->is_spanner_segment()) return false;
        if (node->is_measure() && to_measure(node)->is_mm_rest()) return false;
        if (node->is_text() && node->ownership_parent() && node->ownership_parent()->is_tuplet()) return false;
        if (node->is_engraving_item() && to_engraving_item(node)->is_layout_copy()) return false;
    }
    return true;
}

static EIDRegister *active_register(EngravingObject &owner) {
    Score *score = owner.score();
    if (!score || !Score::is_alive(score)) return nullptr;
    MasterScore *master = score->master_score();
    if (!master || !Score::is_alive(master)) return nullptr;
    auto *registry = master->eid_register();
    return registry->membership_ready() ? registry : nullptr;
}

// Copies and imports sometimes initialize owned fields directly. Adoption of a
// newly inserted subtree enrolls those actual fields, never constructor children_.
static void set_subtree_live(EIDRegister &registry, EngravingObject &item, bool live, bool detaching_score = false);

template <typename Function>
static void for_each_source_child(EngravingObject &item, const Function &adopt, bool live) {
    if (item.is_score()) {
        auto &score = *to_score(&item);
        for (auto *part : score.parts()) adopt(part);
        for (auto *spanner : score.spanner_list()) adopt(spanner);
        for (auto *base = score.first(); base; base = base->next()) adopt(base);
        return;
    }
    if (item.is_part() || item.is_shared_part()) {
        // Part copies briefly retain borrowed staff references before the clone
        // builder replaces them. Those references are not ownership transfers.
        for (auto *staff : to_part(&item)->staves())
            if (staff->part() == &item) adopt(staff);
        return;
    }
    if (item.is_measure_base()) {
        auto &base = *to_measure_base(&item);
        for (auto *child : base.el()) adopt(child);
        if (item.is_measure()) {
            auto &measure = *to_measure(&item);
            for (auto *row : measure.mstaves()) {
                adopt(row->vspacer_up());
                adopt(row->vspacer_down());
            }
            for (auto *segment = measure.first(); segment; segment = segment->next()) {
                adopt(segment);
                for (auto *element : segment->elist()) {
                    if (!element || !element->is_chord_rest()) continue;
                    for (auto *tuplet = to_chord_rest(element)->tuplet(); tuplet; tuplet = tuplet->tuplet()) adopt(tuplet);
                }
            }
        }
        if (item.is_t_box()) adopt(to_t_box(&item)->text());
        return;
    }
    if (item.is_segment()) {
        auto &segment = *to_segment(&item);
        for (auto *child : segment.elist())
            if (child) adopt(child);
        for (auto *child : segment.annotations()) adopt(child);
        return;
    }
    if (item.is_chord_rest()) {
        auto &rest = *to_chord_rest(&item);
        for (auto *lyric : rest.lyrics()) adopt(lyric);
        for (auto *child : rest.el()) adopt(child);
        if (auto *measure = rest.measure(); live && measure) {
            for (auto *tuplet = rest.tuplet(); tuplet; tuplet = tuplet->tuplet()) source_child_added(*measure, tuplet);
        }
        if (live && rest.beam()) source_child_added(*rest.score(), rest.beam());
        if (item.is_chord()) {
            auto &chord = *to_chord(&item);
            for (auto *note : chord.notes()) adopt(note);
            for (auto *grace : chord.grace_notes()) adopt(grace);
            for (auto *articulation : chord.articulations()) adopt(articulation);
            adopt(chord.arpeggio());
            adopt(chord.tremolo_single_chord());
            // A two-chord tremolo is owned by its first chord only.
            if (auto *tremolo = chord.tremolo_two_chord(); tremolo && tremolo->chord1() == &chord) adopt(tremolo);
        }
        return;
    }
    if (item.is_note()) {
        auto &note = *to_note(&item);
        adopt(note.accidental());
        adopt(note.tie_for());
        if (auto *tie = note.tie_back(); tie && tie->is_partial_tie() && !tie->start_element()) adopt(tie);
        for (auto *child : note.el()) adopt(child);
        for (auto *spanner : note.spanner_for())
            if (spanner->anchor() == Spanner::Anchor::NOTE) adopt(spanner);
        return;
    }
    if (item.is_tuplet() || item.is_beam() || item.is_spanner()) return;
    // Remaining composites (e.g. fret-diagram harmonies and symbol leaves) have
    // an explicit child enumeration. This is scoped to the inserted subtree.
    if (item.is_engraving_item()) {
        item.scan_elements([&](EngravingItem *child) {
            if (child == &item || !source_object(*child)) return;
            auto *owner = child->ownership_parent();
            if (!owner) return;
            for (auto *ancestor = owner; ancestor; ancestor = ancestor->ownership_parent()) {
                if (ancestor == &item) {
                    adopt(child);
                    break;
                }
            }
        });
    }
}

static void set_subtree_live(EIDRegister &registry, EngravingObject &item, bool live, bool detaching_score) {
    detaching_score |= !live && item.is_score();
    if (detaching_score && item.is_chord_rest()) {
        if (auto *beam = to_chord_rest(&item)->beam()) registry.set_live(*beam, false);
    }
    registry.set_live(item, live);
    for_each_source_child(
        item,
        [&](EngravingObject *child) {
            if (child && source_object(*child)) set_subtree_live(registry, *child, live, detaching_score);
        },
        live);
}

void source_child_added(EngravingObject &owner, EngravingObject *item) {
    if (!item || item == &owner) return;
    if (item->score() && owner.score() && item->score()->master_score() != owner.score()->master_score()) return;
    auto *registry = active_register(owner);
    if (registry && source_object(owner) && source_object(*item))
        set_subtree_live(*registry, *item, registry->live_item(registry->handle_of(&owner)) != nullptr);
}

void source_child_removed(EngravingObject &owner, EngravingObject *item) {
    if (!item) return;
    if (auto *registry = active_register(owner); registry && source_object(owner) && source_object(*item))
        set_subtree_live(*registry, *item, false);
}

void touch_source_subtree(EngravingObject &item) {
    if (item.is_segment() && source_object(item)) {
        auto *measure = to_segment(&item)->measure();
        measure->mutldata()->rendered.clear();
        measure->mutldata()->segment_output_invalid = true;
        measure->mutldata()->set_need_layout(true);
        measure->score()->invalidate_layout(measure->tick(), measure->end_tick());
    }
    auto *registry = active_register(item);
    if (!registry || !registry->tracking_ids()) return;
    registry->touch(item);
    if (item.is_chord_rest()) {
        if (auto *beam = to_chord_rest(&item)->beam()) registry->touch(*beam);
    }
    for_each_source_child(
        item,
        [](EngravingObject *child) {
            if (child && source_object(*child)) touch_source_subtree(*child);
        },
        false);
}

bool source_attached(EngravingObject &item) {
    auto *registry = active_register(item);
    if (registry && registry->live_item(registry->handle_of(&item))) return true;
    auto *owner = item.ownership_parent();
    if (!owner || !source_object(*owner)) return false;
    bool found = false;
    for_each_source_child(*owner, [&](EngravingObject *child) { found |= child == &item; }, false);
    return found;
}

void source_collection_cleared(EngravingObject &owner, SourceCollection collection) {
    if (!active_register(owner)) return;
    if (collection == SourceCollection::STAVES) {
        for (auto *staff : to_part(&owner)->staves())
            if (staff->part() == &owner) source_child_removed(owner, staff);
    } else if (collection == SourceCollection::SEGMENTS) {
        for (auto *segment = to_measure(&owner)->first(); segment; segment = segment->next()) source_child_removed(owner, segment);
    } else {
        for (auto *base = to_score(&owner)->first(); base; base = base->next()) source_child_removed(owner, base);
    }
}

} // namespace mu::engraving
