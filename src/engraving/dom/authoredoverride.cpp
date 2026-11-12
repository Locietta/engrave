/* SPDX-License-Identifier: GPL-3.0-only */
#include "authoredoverride.h"

#include "beam.h"
#include "chord.h"
#include "engravingitem.h"
#include "guitarbend.h"
#include "hammeronpulloff.h"
#include "hook.h"
#include "note.h"
#include "parenthesis.h"
#include "rest.h"
#include "spanner.h"
#include "stem.h"
#include "stemslash.h"
#include "style/style.h"
#include "style/textstyle.h"
#include "text.h"
#include "textlinebase.h"
#include "tremolotwochord.h"
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

bool number_style_property(Pid pid);

bool supported_property(AuthoredChildRole role, Pid pid) {
    if (role == AuthoredChildRole::NONE || pid == Pid::END) return false;
    if (role == AuthoredChildRole::TUPLET_NUMBER && number_style_property(pid)) return false;
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

bool number_style_property(Pid pid) {
    switch (pid) {
        case Pid::FONT_FACE:
        case Pid::FONT_SIZE:
        case Pid::FONT_STYLE:
        case Pid::ALIGN:
        case Pid::POSITION:
        case Pid::SIZE_SPATIUM_DEPENDENT: return true;
        default: return false;
    }
}

Tuplet *number_style_owner(const EngravingObject &object, Pid pid) {
    if (!number_style_property(pid) || !object.is_engraving_item()) return nullptr;
    const auto &item = static_cast<const EngravingItem &>(object);
    if (item.authored_mirror) return nullptr;
    auto *parent = item.ownership_parent();
    if (!parent || !parent->is_tuplet()) return nullptr;
    auto *tuplet = static_cast<Tuplet *>(parent);
    return tuplet->number() == &item ? tuplet : nullptr;
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
    if (parent->is_spanner() && item.is_spanner_segment()) {
        const auto &segments = static_cast<const Spanner *>(parent)->spanner_segments();
        for (size_t i = 0; i < segments.size(); ++i)
            if (segments[i] == &item) return {AuthoredChildRole::SPANNER_SEGMENT, static_cast<uint32_t>(i)};
    }
    if (parent->is_spanner_segment() && item.is_text_base()) {
        const auto &segment = static_cast<const SpannerSegment &>(*parent);
        const auto key = segment.authored_child.role == AuthoredChildRole::NONE ? inferred_key(segment) : segment.authored_child;
        if (parent->is_guitar_bend_segment() && static_cast<const GuitarBendSegment *>(parent)->bend_text() == &item)
            return {AuthoredChildRole::SEGMENT_TEXT, key.ordinal};
        if (parent->is_hammer_on_pull_off_segment()) {
            const auto &texts = static_cast<const HammerOnPullOffSegment *>(parent)->hopo_text();
            for (size_t i = 0; i < texts.size(); ++i)
                if (texts[i] == &item) return {AuthoredChildRole::SEGMENT_TEXT, key.ordinal, 0, static_cast<uint32_t>(i)};
        }
    }
    if (item.is_parenthesis()) {
        if (parent->is_chord()) {
            const auto &groups = static_cast<const Chord *>(parent)->note_parentheses();
            for (size_t i = 0; i < groups.size(); ++i) {
                if (groups[i]->left_paren() == &item) return {AuthoredChildRole::PAREN_LEFT, static_cast<uint32_t>(i), 0, 1};
                if (groups[i]->right_paren() == &item) return {AuthoredChildRole::PAREN_RIGHT, static_cast<uint32_t>(i), 0, 1};
            }
        }
        if (parent->is_engraving_item()) {
            const auto *owner = static_cast<const EngravingItem *>(parent);
            if (owner->left_paren() == &item) return {AuthoredChildRole::PAREN_LEFT};
            if (owner->right_paren() == &item) return {AuthoredChildRole::PAREN_RIGHT};
        }
    }
    if (parent->is_tuplet() && static_cast<const Tuplet *>(parent)->number() == &item) return {AuthoredChildRole::TUPLET_NUMBER};
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

bool fragment_owner(const EngravingItem &item) { return item.is_beam() || item.type() == ElementType::TREMOLO_TWOCHORD; }

AuthoredChildKey child_key(const EngravingItem &item) {
    if (item.type() == ElementType::TREMOLO_TWOCHORD && item.authored_child.role == AuthoredChildRole::NONE)
        return {AuthoredChildRole::BEAM_FRAGMENT, 0, static_cast<uint8_t>(static_cast<const TremoloTwoChord &>(item).direction_idx())};
    if (item.is_beam() && item.authored_child.role == AuthoredChildRole::NONE) {
        const auto &beam = static_cast<const Beam &>(item);
        uint32_t ordinal = beam.beam_fragments().empty() ? 0 : static_cast<uint32_t>(beam.beam_fragments().size() - 1);
        bool found = false;
        for (const auto &entry : beam.authored_overrides.entries) {
            if (entry.child.role == AuthoredChildRole::BEAM_FRAGMENT && entry.child.direction == beam.direction_idx() &&
                entry.pid == Pid::BEAM_POS) {
                ordinal = found ? std::max(ordinal, entry.child.ordinal) : entry.child.ordinal;
                found = true;
            }
        }
        return {AuthoredChildRole::BEAM_FRAGMENT, ordinal, static_cast<uint8_t>(beam.direction_idx())};
    }
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
    if (item.is_text_base()) {
        for (const auto &property : *text_style(static_cast<const TextBase &>(item).text_style_type()))
            if (property.pid == pid) return PropertyFlags::STYLED;
    }
    return PropertyFlags::NOSTYLE;
}

bool fixed_spatium_property(Pid pid) {
    return pid == Pid::OFFSET2 || pid == Pid::SLUR_UOFF1 || pid == Pid::SLUR_UOFF2 || pid == Pid::SLUR_UOFF3 || pid == Pid::SLUR_UOFF4 ||
           pid == Pid::BEND_VERTEX_OFF;
}

PropertyValue mirror_default(const EngravingItem &item, Pid pid) {
    if (pid == Pid::TEXT_STYLE) {
        if (item.authored_child.role == AuthoredChildRole::TUPLET_NUMBER) return TextStyleType::TUPLET;
        if (item.type() == ElementType::GUITAR_BEND_TEXT) return TextStyleType::BEND;
        if (item.type() == ElementType::HAMMER_ON_PULL_OFF_TEXT) return TextStyleType::HAMMER_ON_PULL_OFF;
    }
    return item.property_default(pid);
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
    if (item.is_text_base()) {
        for (const auto &property : *text_style(static_cast<const TextBase &>(item).text_style_type())) {
            if (property.pid == pid) {
                item.property_flags_list()[index] = flags;
                return;
            }
            ++index;
        }
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
            case Pid::TEXT_STYLE:
                if (item.is_text_base() && static_cast<TextBase &>(item).text_style_type() != value.value<TextStyleType>())
                    item.set_property(pid, value);
                break;
            default: item.set_property(pid, value); break;
        }
    }
    mirror_flags(item, pid, flags);
}

void prepare_carrier(EngravingItem &item, AuthoredOverrideTable &table) {
    if (table.carrier(item.authored_child)) return;
    if (item.authored_child.role == AuthoredChildRole::SEGMENT_TEXT) {
        auto *segment = static_cast<SpannerSegment *>(item.ownership_parent());
        if (segment->authored_child.role == AuthoredChildRole::NONE)
            segment->authored_child = {AuthoredChildRole::SPANNER_SEGMENT, item.authored_child.ordinal};
        prepare_carrier(*segment, table);
    }
    if (item.authored_child.role == AuthoredChildRole::DOT) {
        const auto *owner = authored_override_owner(item);
        const auto &dots = owner->is_note() ? static_cast<const Note *>(owner)->dots() : static_cast<const Rest *>(owner)->dot_list();
        for (uint32_t i = 0; i < item.authored_child.ordinal && i < dots.size(); ++i) {
            const AuthoredChildKey key{AuthoredChildRole::DOT, i};
            if (table.carrier(key)) continue;
            auto id = dots[i]->eid();
            if (!id.is_valid()) id = EID::new_unique();
            table.set_carrier({key, ElementType::NOTEDOT, id});
        }
    }
    auto eid = item.eid();
    if (!eid.is_valid()) eid = EID::new_unique();
    table.set_carrier({item.authored_child, item.type(), eid, EID::invalid(), muse::nidx});
    if (item.authored_child.role == AuthoredChildRole::SPANNER_SEGMENT) {
        const auto subtype = static_cast<const SpannerSegment &>(item).spanner_segment_type();
        table.set({item.authored_child, Pid::SUBTYPE, int(subtype), PropertyFlags::NOSTYLE});
    }
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
        case ElementType::TREMOLO_TWOCHORD: return &static_cast<TremoloTwoChord *>(owner)->authored_overrides;
        default: return owner->is_spanner() ? &static_cast<Spanner *>(owner)->authored_overrides : nullptr;
    }
}

const AuthoredOverrideTable *authored_override_table(const EngravingItem *owner) {
    return authored_override_table(const_cast<EngravingItem *>(owner));
}

EngravingItem *authored_override_owner(const EngravingItem &item) {
    if (fragment_owner(item) && item.authored_child.role == AuthoredChildRole::NONE) return const_cast<EngravingItem *>(&item);
    auto *parent = item.ownership_parent();
    if (!parent || !parent->is_engraving_item()) return nullptr;
    auto *owner = static_cast<EngravingItem *>(parent);
    if (item.authored_child.role == AuthoredChildRole::SEGMENT_TEXT && owner->is_spanner_segment()) {
        owner = static_cast<SpannerSegment *>(owner)->spanner();
    }
    return authored_override_table(owner) ? owner : nullptr;
}

bool authored_override_handles(const EngravingObject &object, Pid pid) {
    if (object.is_spanner_segment()) {
        // These are logical spanner properties delegated by the legacy segment
        // adapter (or unsupported by non-text segments), not local overrides.
        const auto &properties = TextLineBase::text_line_base_property_ids();
        if (pid == Pid::TEXT_STYLE || std::find(properties.begin(), properties.end(), pid) != properties.end()) return false;
    }
    if (object.is_engraving_item() && fragment_owner(static_cast<const EngravingItem &>(object)) && pid != Pid::BEAM_POS) return false;
    auto *item = bound_item(object);
    return item && supported_property(child_key(*item).role, pid) && authored_override_owner(*item);
}

std::optional<PropertyValue> authored_override_value(const EngravingObject &object, Pid pid) {
    if (auto *owner = number_style_owner(object, pid)) return owner->get_property(pid);
    if (!authored_override_handles(object, pid)) return std::nullopt;
    const auto &item = *bound_item(object);
    const auto *table = authored_override_table(authored_override_owner(item));
    if (const auto *entry = table->find(child_key(item), pid)) return effective_value(item, *entry);
    return std::nullopt;
}

std::optional<PropertyFlags> authored_override_flags(const EngravingObject &object, Pid pid) {
    if (auto *owner = number_style_owner(object, pid)) return owner->property_flags(pid);
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
    if (!fragment_owner(item)) prepare_carrier(item, table);
    AuthoredOverrideEntry entry{child_key(item), pid, value, flags};
    if (property_type(pid) == PType::POINT || property_type(pid) == PType::ABSOLUTE) {
        entry.unit = property_type(pid) == PType::ABSOLUTE || fixed_spatium_property(pid) || item.offset_is_spatium_dependent() ?
                         AuthoredOverrideUnit::SPATIUM :
                         AuthoredOverrideUnit::MILLIMETRE;
        const double scale = unit_scale(item, entry);
        const auto normalize = [pid, scale](const PropertyValue &v) -> PropertyValue {
            if (!v.is_valid()) return v;
            if (property_type(pid) == PType::POINT) return v.value<PointF>() / scale;
            return v.to_double() / scale;
        };
        entry.value = normalize(value);
    }
    table.set(std::move(entry));
}

bool set_authored_override(EngravingItem &item, Pid pid, const PropertyValue &value) {
    if (auto *owner = number_style_owner(item, pid)) return owner->set_property(pid, value);
    if (!authored_override_handles(item, pid)) return false;
    if (item.authored_child.role == AuthoredChildRole::NONE) item.authored_child = inferred_key(item);
    const auto flags = *authored_override_flags(item, pid);
    remember_authored_property(item, pid, value, flags);
    if (pid == Pid::SIZE_SPATIUM_DEPENDENT) {
        auto &table = *authored_override_table(authored_override_owner(item));
        for (auto &entry : table.entries) {
            if (entry.child != item.authored_child || property_type(entry.pid) != PType::POINT || fixed_spatium_property(entry.pid))
                continue;
            const auto physical = effective_value(item, entry).value<PointF>();
            entry.unit = value.to_bool() ? AuthoredOverrideUnit::SPATIUM : AuthoredOverrideUnit::MILLIMETRE;
            entry.value = physical / unit_scale(item, entry);
        }
    }
    mirror_property(item, pid, value, flags);
    item.trigger_layout();
    return true;
}

bool set_authored_override_flags(EngravingObject &object, Pid pid, PropertyFlags flags) {
    if (auto *owner = number_style_owner(object, pid)) {
        owner->set_property_flags(pid, flags);
        return true;
    }
    if (!authored_override_handles(object, pid)) return false;
    auto &item = *bound_item(object);
    if (item.authored_child.role == AuthoredChildRole::NONE) item.authored_child = inferred_key(item);
    auto &table = *authored_override_table(authored_override_owner(item));
    if (const auto *entry = table.find(child_key(item), pid)) {
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
    if (item.authored_child.role != AuthoredChildRole::NONE && item.authored_child != key) {
        if (const auto *table = authored_override_table(authored_override_owner(item))) {
            for (const auto &entry : table->entries) {
                if (entry.child == item.authored_child && !table->find(key, entry.pid))
                    mirror_property(item, entry.pid, mirror_default(item, entry.pid), default_flags(item, entry.pid));
            }
        }
    }
    item.authored_child = key;
    apply_authored_overrides(item);
}

void apply_authored_overrides(EngravingItem &item) {
    if (fragment_owner(item) && item.authored_child.role == AuthoredChildRole::NONE) return;
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
    // Legacy point values use the final carrier unit policy, even when its tag
    // followed an offset. Source values are already normalized wire coordinates.
    for (auto &entry : table->entries) {
        if (entry.child == item.authored_child && property_type(entry.pid) == PType::POINT)
            entry.unit = fixed_spatium_property(entry.pid) || item.offset_is_spatium_dependent() ? AuthoredOverrideUnit::SPATIUM :
                                                                                                   AuthoredOverrideUnit::MILLIMETRE;
    }
    const auto *old = table->carrier(child_key(item));
    auto eid = item.eid();
    if (!eid.is_valid()) eid = old ? old->legacy_eid : EID::new_unique();
    table->set_carrier({item.authored_child, item.type(), eid, old ? old->linked_to : EID::invalid(), old ? old->track : muse::nidx});
}

bool has_authored_override(const EngravingItem &item, Pid pid) {
    const auto *table = authored_override_table(authored_override_owner(item));
    return table && table->find(child_key(item), pid);
}

void reset_authored_override(EngravingItem &item, Pid pid) {
    auto &table = *authored_override_table(authored_override_owner(item));
    table.erase(child_key(item), pid);
    // Carrier identity and dormant legacy ordinal survive a property reset.
    if (!fragment_owner(item)) mirror_property(item, pid, mirror_default(item, pid), default_flags(item, pid));
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
