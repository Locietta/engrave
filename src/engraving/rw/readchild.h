/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "dom/authoredoverride.h"
#include "dom/engravingitem.h"
#include "dom/masterscore.h"
#include "dom/spanner.h"

namespace mu::engraving {
template <typename Context>
AuthoredChildKey &read_child_key(const EngravingItem &item, Context &ctx) {
    return ctx.child_keys[&item];
}

template <typename Context>
EngravingItem *read_child_owner(const EngravingItem &item, Context &ctx) {
    auto *owner = item.ownership_parent_item();
    if (!owner) return nullptr;
    if (item.is_spanner_segment() && nested_spanner_role(read_child_key(*owner, ctx).role))
        owner = owner->ownership_parent_item();
    else if (read_child_key(item, ctx).role == AuthoredChildRole::SEGMENT_TEXT && owner->is_spanner_segment())
        owner = static_cast<SpannerSegment *>(owner)->spanner();
    return authored_override_table(owner) ? owner : nullptr;
}

template <typename Context>
void begin_child_read(EngravingItem &item, AuthoredChildKey key, Context &ctx) {
    if (item.is_spanner_segment()) {
        const auto parent_key = read_child_key(*item.ownership_parent_item(), ctx);
        if (nested_spanner_role(parent_key.role)) key = {parent_key.role, key.ordinal, 0, 1};
    }
    read_child_key(item, ctx) = key;
}

template <typename Context>
void read_child_property(EngravingItem &carrier, Pid pid, const PropertyValue &value, PropertyFlags flags, Context &ctx) {
    auto *owner = read_child_owner(carrier, ctx);
    const auto key = read_child_key(carrier, ctx);
    if (!owner || key.role == AuthoredChildRole::NONE) return;
    auto &table = *authored_override_table(owner);
    if (!table.carrier(key)) table.set_carrier({key, carrier.type(), carrier.master_score()->eid_register()->new_eid(false)});
    std::vector<AuthoredOverrideEntry> points;
    if (pid == Pid::SIZE_SPATIUM_DEPENDENT)
        for (const auto &entry : table.entries)
            if (entry.child == key && property_type(entry.pid) == PType::POINT) points.push_back(entry);
    set_child_override(*owner, key, pid, value, flags);
    for (const auto &entry : points) table.set(entry);
}

template <typename Context>
void finish_child_read(EngravingItem &item, Context &ctx, bool final = true) {
    auto *owner = read_child_owner(item, ctx);
    const auto key = read_child_key(item, ctx);
    if (!owner || key.role == AuthoredChildRole::NONE) return;
    auto &table = *authored_override_table(owner);
    // Legacy wire coordinates use the final carrier policy even when its unit tag followed an offset.
    for (auto &entry : table.entries) {
        if (entry.child != key || property_type(entry.pid) != PType::POINT) continue;
        const bool fixed = entry.pid == Pid::OFFSET2 || entry.pid == Pid::SLUR_UOFF1 || entry.pid == Pid::SLUR_UOFF2 ||
                           entry.pid == Pid::SLUR_UOFF3 || entry.pid == Pid::SLUR_UOFF4 || entry.pid == Pid::BEND_VERTEX_OFF;
        entry.unit = fixed || item.offset_is_spatium_dependent() ? AuthoredOverrideUnit::SPATIUM : AuthoredOverrideUnit::MILLIMETRE;
    }
    const auto *old = table.carrier(key);
    auto eid = item.eid();
    if (!eid.is_valid()) eid = old ? old->legacy_eid : item.master_score()->eid_register()->new_eid(false);
    table.set_carrier({key, item.type(), eid, old ? old->track : muse::nidx});
    if (final) ctx.child_keys.erase(&item);
}
} // namespace mu::engraving
