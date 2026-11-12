/* SPDX-License-Identifier: GPL-3.0-only */
#include "elementchange.h"

#include <algorithm>
#include <stdexcept>

#include "dom/masterscore.h"
#include "transaction.h"

namespace mu::engraving {
LinkInverse capture_link(EngravingObject &object, EngravingObject *peer) {
    LinkInverse result;
    result.target = JournalTarget::capture(object);
    result.adding = peer != nullptr;
    if (peer) {
        if (peer == &object) throw std::logic_error("an object cannot link to itself");
        if (object.links()) throw std::logic_error("a link target is already linked");
        if (object.master_score() != peer->master_score()) throw std::logic_error("links cannot span documents");
        result.group = peer->links();
        if (!result.group) {
            result.owned_group = std::make_unique<LinkedObjects>();
            result.group = result.owned_group.get();
            result.group->push_back(peer);
            result.new_group = true;
        }
    } else {
        result.group = object.links();
        if (!result.group) throw std::logic_error("an unlink target has no group");
        const auto found = std::find(result.group->begin(), result.group->end(), &object);
        if (found == result.group->end()) throw std::logic_error("an unlink target is absent from its group");
        result.position = static_cast<size_t>(std::distance(result.group->begin(), found));
    }
    return result;
}

void apply_link(LinkInverse &inverse) {
    auto &object = inverse.target.object();
    auto &group = *inverse.group;
    if (inverse.adding) {
        // Allocate the list node before changing either object's group pointer.
        group.push_back(&object);
        if (group.size() == 2) group.front()->set_links(&group);
        object.set_links(&group);
        inverse.owned_group.release();
    } else {
        group.remove(&object);
        if (group.size() <= 1) {
            if (!group.empty()) group.front()->set_links(nullptr);
            inverse.owned_group.reset(&group);
        }
        object.set_links(nullptr);
    }
    inverse.applied = true;
}

void restore_link(LinkInverse &inverse) {
    if (!inverse.applied) return;
    auto &object = inverse.target.object();
    auto &group = *inverse.group;
    if (inverse.adding) {
        group.remove(&object);
        object.set_links(nullptr);
        if (inverse.new_group && group.size() <= 1) {
            if (!group.empty()) group.front()->set_links(nullptr);
            inverse.owned_group.reset(&group);
        }
    } else {
        auto position = group.begin();
        std::advance(position, std::min(inverse.position, group.size()));
        group.insert(position, &object);
        for (auto *member : group) member->set_links(&group);
        inverse.owned_group.release();
    }
    inverse.applied = false;
}

void dispose_element_membership(const ElementMembershipInverse &inverse, bool adding, bool committed) {
    if (adding == committed) return;
    auto *item = inverse.target.document->eid_register()->item_from_handle(inverse.target.handle);
    if (!item) return;
    // A remove/add pair or its rollback may have reattached the same lifetime.
    // Such an object belongs to the final source tree, never to journal disposal.
    if (source_object(*item) && inverse.target.document->eid_register()->live_item(inverse.target.handle)) return;
    delete item;
}

void Score::record_element_add(EngravingItem *item) { transaction_manager()->current_or_dummy_transaction().add_element(item); }

void Score::record_element_remove(EngravingItem *item) { transaction_manager()->current_or_dummy_transaction().remove_element(item); }

void Score::record_element_replace(EngravingItem *before, EngravingItem *after) {
    transaction_manager()->current_or_dummy_transaction().replace_element(before, after);
}

void Score::record_parent_change(EngravingItem *item, EngravingItem *parent, staff_idx_t staff) {
    transaction_manager()->current_or_dummy_transaction().reparent_element(item, parent, staff);
}

void Score::record_segment_parent_change(Segment *item, Measure *parent, Fraction tick) {
    transaction_manager()->current_or_dummy_transaction().reparent_segment(item, parent, tick);
}

void Score::record_link(EngravingObject *item, EngravingObject *peer) {
    transaction_manager()->current_or_dummy_transaction().link_objects(item, peer);
}

void Score::record_unlink(EngravingObject *item) { transaction_manager()->current_or_dummy_transaction().unlink_object(item); }
} // namespace mu::engraving
