/* SPDX-License-Identifier: GPL-3.0-only */
#include "authoredoverride.h"

#include "beam.h"
#include "chord.h"
#include "engravingitem.h"
#include "hook.h"
#include "note.h"
#include "parenthesis.h"
#include "rest.h"
#include "spanner.h"
#include "stem.h"
#include "stemslash.h"
#include "style/style.h"
#include "text.h"
#include "tuplet.h"

namespace mu::engraving {
namespace {
auto entry_position(auto &entries, AuthoredChildKey key, Pid pid) {
    return std::lower_bound(entries.begin(), entries.end(), std::pair(key, pid), [](const auto &entry, const auto &value) {
        return entry.child < value.first || (entry.child == value.first && entry.pid < value.second);
    });
}

auto carrier_position(auto &carriers, AuthoredChildKey key) {
    return std::lower_bound(carriers.begin(), carriers.end(), key, [](const auto &entry, auto value) { return entry.child < value; });
}

bool common_property(Pid pid) {
    switch (pid) {
        case Pid::COLOR:
        case Pid::VISIBLE:
        case Pid::Z:
        case Pid::OFFSET:
        case Pid::MIN_DISTANCE:
        case Pid::AUTOPLACE:
        case Pid::PLACEMENT:
        case Pid::SIZE_SPATIUM_DEPENDENT:
        case Pid::POSITION_LINKED_TO_MASTER:
        case Pid::APPEARANCE_LINKED_TO_MASTER: return true;
        default: return false;
    }
}

bool supported_property(AuthoredChildRole role, Pid pid) {
    if (role == AuthoredChildRole::NONE || pid == Pid::END) return false;
    switch (pid) {
        case Pid::SELECTED:
        case Pid::GENERATED:
        case Pid::TRACK:
        case Pid::VOICE:
        case Pid::TICK:
        case Pid::STEM_DIRECTION:
        case Pid::SLUR_DIRECTION:
        case Pid::SLUR_STYLE_TYPE:
        case Pid::MASK_SLURTIE:
        case Pid::SPANNER_TICK:
        case Pid::SPANNER_TICKS:
        case Pid::SPANNER_TRACK2:
        case Pid::PLAY:
        case Pid::EXCLUDE_FROM_OTHER_PARTS: return false;
        case Pid::USER_LEN: return role != AuthoredChildRole::STEM; // existing typed owner field
        default: return true;
    }
}

AuthoredChildKey inferred_key(const EngravingItem &item) {
    auto *parent = item.ownership_parent();
    if (!parent) return {};
    if (parent->is_chord()) {
        const auto *chord = static_cast<const Chord *>(parent);
        if (item.type() == ElementType::STEM && chord->stem() == &item) return {AuthoredChildRole::STEM};
        if (item.type() == ElementType::HOOK && static_cast<const EngravingObject *>(chord->hook()) == &item)
            return {AuthoredChildRole::HOOK};
        if (item.type() == ElementType::STEM_SLASH && static_cast<const EngravingObject *>(chord->stem_slash()) == &item)
            return {AuthoredChildRole::STEM_SLASH};
    }
    if (item.type() == ElementType::NOTEDOT) {
        const auto *dots = parent->is_note() ? &static_cast<const Note *>(parent)->dots() :
                           parent->is_rest() ? &static_cast<const Rest *>(parent)->dot_list() :
                                               nullptr;
        if (dots)
            for (size_t i = 0; i < dots->size(); ++i)
                if ((*dots)[i] == &item) return {AuthoredChildRole::DOT, static_cast<uint32_t>(i)};
    }

    return {};
}

AuthoredChildKey child_key(const EngravingItem &item) {
    return item.authored_child.role == AuthoredChildRole::NONE ? inferred_key(item) : item.authored_child;
}

EngravingItem *bound_item(const EngravingObject &object) {
    if (!object.is_engraving_item()) return nullptr;
    auto *item = static_cast<EngravingItem *>(const_cast<EngravingObject *>(&object));
    return child_key(*item).role == AuthoredChildRole::NONE || item->authored_mirror ? nullptr : item;
}

PropertyFlags default_flags(const EngravingItem &item, Pid pid) {
    for (const auto &property : *item.styled_properties())
        if (property.pid == pid) return PropertyFlags::STYLED;
    return PropertyFlags::NOSTYLE;
}

double unit_scale(const EngravingItem &item, const AuthoredOverrideEntry &entry) {
    return entry.unit == AuthoredOverrideUnit::SPATIUM    ? item.style().spatium() :
           entry.unit == AuthoredOverrideUnit::MILLIMETRE ? ::mu::engraving::dpmm :
                                                            1.0;
}

PropertyValue effective_value(const EngravingItem &item, const AuthoredOverrideEntry &entry) {
    if (entry.flags == PropertyFlags::STYLED) return item.property_default(entry.pid);
    if (entry.unit == AuthoredOverrideUnit::NATIVE) return entry.value;
    const double scale = unit_scale(item, entry);
    if (property_type(entry.pid) == PType::POINT) return entry.value.value<PointF>() * scale;
    return entry.value.to_double() * scale;
}

void mirror_flags(EngravingItem &item, Pid pid, PropertyFlags flags) {
    size_t index = 0;
    for (const auto &property : *item.styled_properties()) {
        if (property.pid == pid) {
            item.property_flags_list()[index] = flags;
            return;
        }
        ++index;
    }
}

void mirror_property(EngravingItem &item, Pid pid, const PropertyValue &value, PropertyFlags flags) {
    AuthoredOverrideMirror scope(item);
    if (value.is_valid() && pid != Pid::SUBTYPE) {
        switch (pid) {
            case Pid::COLOR: item.EngravingItem::set_color(value.value<Color>()); break;
            case Pid::VISIBLE: item.EngravingItem::set_visible(value.to_bool()); break;
            case Pid::Z: item.set_z(value.to_int()); break;
            case Pid::OFFSET: item.set_offset(value.value<PointF>()); break;
            case Pid::MIN_DISTANCE: item.set_min_distance(value.value<Spatium>()); break;
            case Pid::AUTOPLACE: item.set_autoplace(value.to_bool()); break;
            case Pid::PLACEMENT: item.set_placement(value.value<PlacementV>()); break;
            case Pid::SIZE_SPATIUM_DEPENDENT: item.set_size_is_spatium_dependent(value.to_bool()); break;
            case Pid::POSITION_LINKED_TO_MASTER: item.set_position_linked_to_master(value.to_bool()); break;
            case Pid::APPEARANCE_LINKED_TO_MASTER: item.set_appearance_linked_to_master(value.to_bool()); break;
            default: item.set_property(pid, value); break;
        }
    }
    mirror_flags(item, pid, flags);
}

void prepare_carrier(EngravingItem &item, AuthoredOverrideTable &table) {
    if (table.carrier(item.authored_child)) return;
    auto eid = item.eid();
    if (!eid.is_valid()) eid = EID::new_unique();
    table.set_carrier({item.authored_child, item.type(), eid, EID::invalid(), muse::nidx});
}
} // namespace

const AuthoredOverrideEntry *AuthoredOverrideTable::find(AuthoredChildKey child, Pid pid) const {
    const auto it = entry_position(entries, child, pid);
    return it != entries.end() && it->child == child && it->pid == pid ? &*it : nullptr;
}

void AuthoredOverrideTable::set(AuthoredOverrideEntry entry) {
    auto it = entry_position(entries, entry.child, entry.pid);
    if (it != entries.end() && it->child == entry.child && it->pid == entry.pid)
        *it = std::move(entry);
    else
        entries.insert(it, std::move(entry));
}

void AuthoredOverrideTable::erase(AuthoredChildKey child, Pid pid) {
    auto it = entry_position(entries, child, pid);
    if (it != entries.end() && it->child == child && it->pid == pid) entries.erase(it);
}

const AuthoredOverrideCarrier *AuthoredOverrideTable::carrier(AuthoredChildKey child) const {
    const auto it = carrier_position(carriers, child);
    return it != carriers.end() && it->child == child ? &*it : nullptr;
}

void AuthoredOverrideTable::set_carrier(AuthoredOverrideCarrier carrier) {
    auto it = carrier_position(carriers, carrier.child);
    if (it != carriers.end() && it->child == carrier.child)
        *it = std::move(carrier);
    else
        carriers.insert(it, std::move(carrier));
}

void AuthoredOverrideTable::erase_carrier(AuthoredChildKey child) {
    auto it = carrier_position(carriers, child);
    if (it != carriers.end() && it->child == child) carriers.erase(it);
}

bool AuthoredOverrideTable::has_entries(AuthoredChildKey child) const {
    auto it = entry_position(entries, child, Pid::SUBTYPE);
    return it != entries.end() && it->child == child;
}

AuthoredOverrideTable *authored_override_table(EngravingItem *owner) {
    if (!owner) return nullptr;
    switch (owner->type()) {
        case ElementType::CHORD: return &static_cast<Chord *>(owner)->authored_overrides;
        case ElementType::NOTE: return &static_cast<Note *>(owner)->authored_overrides;
        case ElementType::REST: return &static_cast<Rest *>(owner)->authored_overrides;
        case ElementType::TUPLET: return &static_cast<Tuplet *>(owner)->authored_overrides;
        case ElementType::BEAM: return &static_cast<Beam *>(owner)->authored_overrides;
        default: return owner->is_spanner() ? &static_cast<Spanner *>(owner)->authored_overrides : nullptr;
    }
}

const AuthoredOverrideTable *authored_override_table(const EngravingItem *owner) {
    return authored_override_table(const_cast<EngravingItem *>(owner));
}

EngravingItem *authored_override_owner(const EngravingItem &item) {
    auto *parent = item.ownership_parent();
    if (!parent || !parent->is_engraving_item()) return nullptr;
    auto *owner = static_cast<EngravingItem *>(parent);
    if (item.authored_child.role == AuthoredChildRole::SEGMENT_TEXT && owner->is_spanner_segment()) {
        owner = static_cast<SpannerSegment *>(owner)->spanner();
    }
    return authored_override_table(owner) ? owner : nullptr;
}

bool authored_override_handles(const EngravingObject &object, Pid pid) {
    auto *item = bound_item(object);
    return item && supported_property(child_key(*item).role, pid) && authored_override_owner(*item);
}

std::optional<PropertyValue> authored_override_value(const EngravingObject &object, Pid pid) {
    if (!authored_override_handles(object, pid)) return std::nullopt;
    const auto &item = *bound_item(object);
    const auto *table = authored_override_table(authored_override_owner(item));
    if (const auto *entry = table->find(child_key(item), pid)) return effective_value(item, *entry);
    return std::nullopt;
}

std::optional<PropertyFlags> authored_override_flags(const EngravingObject &object, Pid pid) {
    if (!authored_override_handles(object, pid)) return std::nullopt;
    const auto &item = *bound_item(object);
    const auto *table = authored_override_table(authored_override_owner(item));
    if (const auto *entry = table->find(child_key(item), pid)) return entry->flags;
    return default_flags(item, pid);
}

void remember_authored_property(EngravingItem &item, Pid pid, const PropertyValue &value, PropertyFlags flags) {
    if (!authored_override_handles(item, pid)) return;
    if (item.authored_child.role == AuthoredChildRole::NONE) item.authored_child = inferred_key(item);
    auto &table = *authored_override_table(authored_override_owner(item));
    prepare_carrier(item, table);
    const auto default_value = item.property_default(pid);
    const auto sid = item.get_property_style(pid);
    AuthoredOverrideEntry entry{item.authored_child, pid, value, flags, default_value, sid};
    if (property_type(pid) == PType::POINT || property_type(pid) == PType::ABSOLUTE) {
        entry.unit = item.offset_is_spatium_dependent() ? AuthoredOverrideUnit::SPATIUM : AuthoredOverrideUnit::MILLIMETRE;
        const double scale = unit_scale(item, entry);
        const auto normalize = [pid, scale](const PropertyValue &v) -> PropertyValue {
            if (!v.is_valid()) return v;
            if (property_type(pid) == PType::POINT) return v.value<PointF>() / scale;
            return v.to_double() / scale;
        };
        entry.value = normalize(value);
        entry.default_value = normalize(default_value);
    }
    table.set(std::move(entry));
}

bool set_authored_override(EngravingItem &item, Pid pid, const PropertyValue &value) {
    if (!authored_override_handles(item, pid)) return false;
    if (item.authored_child.role == AuthoredChildRole::NONE) item.authored_child = inferred_key(item);
    const auto flags = *authored_override_flags(item, pid);
    remember_authored_property(item, pid, value, flags);
    mirror_property(item, pid, value, flags);
    item.trigger_layout();
    return true;
}

bool set_authored_override_flags(EngravingObject &object, Pid pid, PropertyFlags flags) {
    if (!authored_override_handles(object, pid)) return false;
    auto &item = *bound_item(object);
    if (item.authored_child.role == AuthoredChildRole::NONE) item.authored_child = inferred_key(item);
    auto &table = *authored_override_table(authored_override_owner(item));
    if (const auto *entry = table.find(item.authored_child, pid)) {
        auto updated = *entry;
        updated.flags = flags;
        table.set(std::move(updated));
    } else if (flags != PropertyFlags::STYLED) {
        remember_authored_property(item, pid, item.get_property(pid), flags);
    }
    mirror_flags(item, pid, flags);
    return true;
}

void bind_authored_child(EngravingItem &item, AuthoredChildKey key) {
    item.authored_child = key;
    apply_authored_overrides(item);
}

void apply_authored_overrides(EngravingItem &item) {
    const auto key = inferred_key(item);
    if (key.role != AuthoredChildRole::NONE) item.authored_child = key;
    const auto *table = authored_override_table(authored_override_owner(item));
    if (!table || item.authored_child.role == AuthoredChildRole::NONE) return;
    for (const auto &entry : table->entries) {
        if (entry.child == item.authored_child) mirror_property(item, entry.pid, effective_value(item, entry), entry.flags);
    }
}

void finish_authored_read(EngravingItem &item) {
    auto *table = authored_override_table(authored_override_owner(item));
    if (!table || item.authored_child.role == AuthoredChildRole::NONE) return;
    const auto *old = table->carrier(child_key(item));
    auto eid = item.eid();
    if (!eid.is_valid()) eid = old ? old->legacy_eid : EID::new_unique();
    table->set_carrier({item.authored_child, item.type(), eid, old ? old->linked_to : EID::invalid(), old ? old->track : muse::nidx});
}

AuthoredOverrideSnapshot snapshot_authored_override(const EngravingItem &item, Pid pid) {
    AuthoredOverrideSnapshot result;
    if (const auto *table = authored_override_table(authored_override_owner(item))) {
        if (const auto *entry = table->find(child_key(item), pid)) result.entry = *entry;
        if (const auto *carrier = table->carrier(child_key(item))) result.carrier = *carrier;
    }
    return result;
}

void restore_authored_override(EngravingItem &item, Pid pid, const AuthoredOverrideSnapshot &snapshot) {
    auto &table = *authored_override_table(authored_override_owner(item));
    if (snapshot.entry)
        table.set(*snapshot.entry);
    else
        table.erase(item.authored_child, pid);
    if (snapshot.carrier)
        table.set_carrier(*snapshot.carrier);
    else if (!table.has_entries(item.authored_child))
        table.erase_carrier(item.authored_child);
    const auto value = snapshot.entry ? effective_value(item, *snapshot.entry) : item.property_default(pid);
    const auto flags = snapshot.entry ? snapshot.entry->flags : default_flags(item, pid);
    mirror_property(item, pid, value, flags);
    item.trigger_layout();
}

void reset_authored_override(EngravingItem &item, Pid pid) {
    auto &table = *authored_override_table(authored_override_owner(item));
    table.erase(item.authored_child, pid);
    if (!table.has_entries(item.authored_child)) table.erase_carrier(item.authored_child);
    mirror_property(item, pid, item.property_default(pid), default_flags(item, pid));
    item.trigger_layout();
}

void clone_authored_overrides(AuthoredOverrideTable &destination, const AuthoredOverrideTable &source) {
    destination = source;
    for (auto &carrier : destination.carriers) {
        carrier.legacy_eid = EID::new_unique();
        carrier.linked_to = EID::invalid();
    }
}

AuthoredOverrideMirror::AuthoredOverrideMirror(EngravingItem &value) : item(value), previous(value.authored_mirror) {
    if (child_key(item).role != AuthoredChildRole::NONE) item.authored_mirror = true;
}

AuthoredOverrideMirror::~AuthoredOverrideMirror() { item.authored_mirror = previous; }
} // namespace mu::engraving
