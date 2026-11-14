/* SPDX-License-Identifier: GPL-3.0-only */
#include "authoredoverride.h"

#include "beam.h"
#include "chord.h"
#include "engravingitem.h"
#include "guitarbend.h"
#include "hammeronpulloff.h"
#include "hook.h"
#include "masterscore.h"
#include "note.h"
#include "parenthesis.h"
#include "rest.h"
#include "slurtie.h"
#include "spanner.h"
#include "stafftype.h"
#include "stem.h"
#include "stemslash.h"
#include "style/style.h"
#include "style/textstyle.h"
#include "symbol.h"
#include "tapping.h"
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
        case Pid::STEM_DIRECTION: return false;
        case Pid::SLUR_DIRECTION:
        case Pid::SLUR_STYLE_TYPE:
        case Pid::MASK_SLURTIE: return nested_spanner_role(role);
        case Pid::SPANNER_TICK:
        case Pid::SPANNER_TICKS:
        case Pid::SPANNER_TRACK2: return false;
        case Pid::PLAY: return nested_spanner_role(role);
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
    if (item.is_guitar_bend_hold() || item.is_tapping_half_slur()) return item.authored_child;
    if (parent->is_spanner() && item.is_spanner_segment()) {
        const auto &segments = static_cast<const Spanner *>(parent)->spanner_segments();
        for (size_t i = 0; i < segments.size(); ++i)
            if (segments[i] == &item) {
                const auto key = static_cast<const EngravingItem *>(parent)->authored_child;
                return nested_spanner_role(key.role) ? AuthoredChildKey{key.role, static_cast<uint32_t>(i), 0, 1} :
                                                       AuthoredChildKey{AuthoredChildRole::SPANNER_SEGMENT, static_cast<uint32_t>(i)};
            }
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
            const auto &groups = static_cast<const Chord *>(parent)->layout_parentheses();
            for (size_t i = 0; i < groups.size(); ++i) {
                if (groups[i].left == &item) return {AuthoredChildRole::PAREN_LEFT, static_cast<uint32_t>(i), 0, 1};
                if (groups[i].right == &item) return {AuthoredChildRole::PAREN_RIGHT, static_cast<uint32_t>(i), 0, 1};
            }
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

PropertyValue output_default(const EngravingItem &item, Pid pid) {
    if (pid == Pid::TEXT_STYLE) {
        if (item.authored_child.role == AuthoredChildRole::TUPLET_NUMBER) return TextStyleType::TUPLET;
        if (item.type() == ElementType::GUITAR_BEND_TEXT) return TextStyleType::BEND;
        if (item.type() == ElementType::HAMMER_ON_PULL_OFF_TEXT) return TextStyleType::HAMMER_ON_PULL_OFF;
    }
    if (item.is_tapping_half_slur() && pid == Pid::SLUR_DIRECTION) {
        if (item.authored_child.role == AuthoredChildRole::TAPPING_SLUR_BELOW) return DirectionV::DOWN;
        return item.staff_type() && item.staff_type()->is_tab_staff() ? DirectionV::UP : DirectionV::AUTO;
    }
    if (item.is_spanner_segment() && (pid == Pid::AUTOPLACE || pid == Pid::PLACEMENT || pid == Pid::POSITION_LINKED_TO_MASTER ||
                                      pid == Pid::APPEARANCE_LINKED_TO_MASTER))
        return item.ownership_parent()->get_property(pid);
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

void output_flags(EngravingItem &item, Pid pid, PropertyFlags flags) {
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

} // namespace

bool nested_spanner_role(AuthoredChildRole role) {
    return role == AuthoredChildRole::HOLD_LINE || role == AuthoredChildRole::TAPPING_SLUR_ABOVE ||
           role == AuthoredChildRole::TAPPING_SLUR_BELOW;
}

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
        case ElementType::TAPPING: return &static_cast<Tapping *>(owner)->authored_overrides;
        case ElementType::CHORD: return &static_cast<Chord *>(owner)->authored_overrides;
        case ElementType::NOTE: return &static_cast<Note *>(owner)->authored_overrides;
        case ElementType::MMREST:
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
    if (item.is_spanner_segment() && nested_spanner_role(owner->authored_child.role)) {
        owner = owner->ownership_parent_item();
    } else if (item.authored_child.role == AuthoredChildRole::SEGMENT_TEXT && owner->is_spanner_segment()) {
        owner = static_cast<SpannerSegment *>(owner)->spanner();
    }
    return authored_override_table(owner) ? owner : nullptr;
}

namespace {
bool valid_child_key(const EngravingItem &owner, AuthoredChildKey key) {
    switch (key.role) {
        case AuthoredChildRole::STEM:
        case AuthoredChildRole::HOOK:
        case AuthoredChildRole::STEM_SLASH: return owner.is_chord();
        case AuthoredChildRole::DOT: return owner.is_note() || owner.is_rest();
        case AuthoredChildRole::PAREN_LEFT:
        case AuthoredChildRole::PAREN_RIGHT:
            return owner.is_chord() && key.subordinal == 1 &&
                   (key.ordinal < static_cast<const Chord &>(owner).note_parentheses().size() ||
                    (key.ordinal == UINT32_MAX && authored_override_table(&owner)->carrier(key)));
        case AuthoredChildRole::TUPLET_NUMBER: return owner.is_tuplet();
        case AuthoredChildRole::BEAM_FRAGMENT: return owner.is_beam() || owner.type() == ElementType::TREMOLO_TWOCHORD;
        case AuthoredChildRole::SPANNER_SEGMENT: return owner.is_spanner();
        case AuthoredChildRole::SEGMENT_TEXT: return owner.is_guitar_bend() || owner.is_hammer_on_pull_off();
        case AuthoredChildRole::HOLD_LINE: return owner.is_guitar_bend();
        case AuthoredChildRole::TAPPING_SLUR_ABOVE:
        case AuthoredChildRole::TAPPING_SLUR_BELOW: return owner.is_tapping();
        default: return false;
    }
}

EngravingObject *routed_owner(EngravingItem &owner, AuthoredChildKey key, Pid pid) {
    if (key.role == AuthoredChildRole::STEM && pid == Pid::USER_LEN && owner.is_chord()) return &owner;
    if (key.role == AuthoredChildRole::TUPLET_NUMBER && number_style_property(pid) && owner.is_tuplet()) return &owner;
    if (key.role == AuthoredChildRole::SPANNER_SEGMENT && owner.is_spanner()) {
        const auto &properties = TextLineBase::text_line_base_property_ids();
        if (pid == Pid::TEXT_STYLE || std::find(properties.begin(), properties.end(), pid) != properties.end()) return &owner;
    }
    return nullptr;
}

bool child_spatium_dependent(const EngravingItem &owner, AuthoredChildKey key) {
    if (key.role == AuthoredChildRole::TUPLET_NUMBER && owner.is_tuplet()) return owner.get_property(Pid::SIZE_SPATIUM_DEPENDENT).to_bool();
    if (const auto *table = authored_override_table(&owner))
        if (const auto *entry = table->find(key, Pid::SIZE_SPATIUM_DEPENDENT)) return entry->value.to_bool();
    if (key.role == AuthoredChildRole::SEGMENT_TEXT) {
        const auto *table = authored_override_table(&owner);
        const auto *carrier = table ? table->carrier(key) : nullptr;
        const auto *style = table ? table->find(key, Pid::TEXT_STYLE) : nullptr;
        const auto type = style                                                            ? style->value.value<TextStyleType>() :
                          carrier && carrier->type == ElementType::HAMMER_ON_PULL_OFF_TEXT ? TextStyleType::HAMMER_ON_PULL_OFF :
                                                                                             TextStyleType::BEND;
        for (const auto &property : *text_style(type))
            if (property.pid == Pid::SIZE_SPATIUM_DEPENDENT) return owner.style().style_b(property.sid);
    }
    return true;
}

ElementType carrier_type(const EngravingItem &owner, AuthoredChildKey key) {
    switch (key.role) {
        case AuthoredChildRole::STEM: return ElementType::STEM;
        case AuthoredChildRole::HOOK: return ElementType::HOOK;
        case AuthoredChildRole::STEM_SLASH: return ElementType::STEM_SLASH;
        case AuthoredChildRole::DOT: return ElementType::NOTEDOT;
        case AuthoredChildRole::PAREN_LEFT:
        case AuthoredChildRole::PAREN_RIGHT: return ElementType::PARENTHESIS;
        case AuthoredChildRole::TUPLET_NUMBER: return ElementType::TEXT;
        case AuthoredChildRole::SEGMENT_TEXT:
            return owner.is_guitar_bend() ? ElementType::GUITAR_BEND_TEXT : ElementType::HAMMER_ON_PULL_OFF_TEXT;
        case AuthoredChildRole::HOLD_LINE: return key.subordinal ? ElementType::GUITAR_BEND_HOLD_SEGMENT : ElementType::GUITAR_BEND_HOLD;
        case AuthoredChildRole::TAPPING_SLUR_ABOVE:
        case AuthoredChildRole::TAPPING_SLUR_BELOW:
            return key.subordinal ? ElementType::TAPPING_HALF_SLUR_SEGMENT : ElementType::TAPPING_HALF_SLUR;
        case AuthoredChildRole::SPANNER_SEGMENT:
            if (owner.is_hammer_on_pull_off()) return ElementType::HAMMER_ON_PULL_OFF_SEGMENT;
            if (owner.is_laissez_vib()) return ElementType::LAISSEZ_VIB_SEGMENT;
            if (owner.is_partial_tie()) return ElementType::PARTIAL_TIE_SEGMENT;
            if (owner.is_tie()) return ElementType::TIE_SEGMENT;
            if (owner.is_slur()) return ElementType::SLUR_SEGMENT;
            return ElementType::INVALID;
        default: return ElementType::INVALID;
    }
}

void ensure_source_carrier(EngravingItem &owner, AuthoredOverrideTable &table, AuthoredChildKey key) {
    if (key.role == AuthoredChildRole::BEAM_FRAGMENT) return;
    if (key.role == AuthoredChildRole::SEGMENT_TEXT) ensure_source_carrier(owner, table, {AuthoredChildRole::SPANNER_SEGMENT, key.ordinal});
    if (nested_spanner_role(key.role) && key.subordinal == 1) ensure_source_carrier(owner, table, {key.role});
    if (table.carrier(key)) return;
    if (key.role == AuthoredChildRole::DOT)
        for (uint32_t i = 0; i < key.ordinal; ++i) ensure_source_carrier(owner, table, {AuthoredChildRole::DOT, i});
    table.set_carrier({key, carrier_type(owner, key), owner.master_score()->eid_register()->new_eid(false)});
}
} // namespace

std::optional<PropertyValue> child_override(const EngravingItem &owner, AuthoredChildKey key, Pid pid) {
    if (!valid_child_key(owner, key)) return std::nullopt;
    if (key.role == AuthoredChildRole::STEM && pid == Pid::USER_LEN && owner.is_chord())
        return static_cast<const Chord &>(owner).stem_user_length();
    if (auto *routed = routed_owner(const_cast<EngravingItem &>(owner), key, pid)) return routed->get_property(pid);
    const auto *table = authored_override_table(&owner);
    const auto *entry = table ? table->find(key, pid) : nullptr;
    return entry ? std::optional(effective_value(owner, *entry)) : std::nullopt;
}

bool set_child_override(EngravingItem &owner, AuthoredChildKey key, Pid pid, const PropertyValue &value, PropertyFlags flags) {
    if (!valid_child_key(owner, key)) return false;
    if (key.role == AuthoredChildRole::STEM && pid == Pid::USER_LEN && owner.is_chord()) {
        static_cast<Chord &>(owner).set_stem_user_length(value.value<Spatium>());
        ensure_source_carrier(owner, *authored_override_table(&owner), key);
        owner.trigger_layout();
        return true;
    }
    if (auto *routed = routed_owner(owner, key, pid)) {
        const bool result = routed->set_property(pid, value);
        routed->set_property_flags(pid, flags);
        return result;
    }
    auto *table = authored_override_table(&owner);
    if ((key.role == AuthoredChildRole::PAREN_LEFT || key.role == AuthoredChildRole::PAREN_RIGHT) && key.subordinal != 1) return false;
    if (!table || !supported_property(key.role, pid)) return false;
    if ((key.role == AuthoredChildRole::PAREN_LEFT || key.role == AuthoredChildRole::PAREN_RIGHT) && key.subordinal == 1 &&
        owner.is_chord() && key.ordinal >= static_cast<Chord &>(owner).note_parentheses().size() && !table->carrier(key))
        return false;
    ensure_source_carrier(owner, *table, key);
    AuthoredOverrideEntry entry{key, pid, value, flags};
    if (property_type(pid) == PType::POINT || property_type(pid) == PType::ABSOLUTE) {
        entry.unit = property_type(pid) == PType::ABSOLUTE || fixed_spatium_property(pid) || child_spatium_dependent(owner, key) ?
                         AuthoredOverrideUnit::SPATIUM :
                         AuthoredOverrideUnit::MILLIMETRE;
        const double scale = unit_scale(owner, entry);
        if (value.is_valid())
            entry.value = property_type(pid) == PType::POINT ? PropertyValue(value.value<PointF>() / scale) :
                                                               PropertyValue(value.to_double() / scale);
    }
    table->set(std::move(entry));
    if (pid == Pid::SIZE_SPATIUM_DEPENDENT) {
        for (auto &point : table->entries) {
            if (point.child != key || property_type(point.pid) != PType::POINT || fixed_spatium_property(point.pid)) continue;
            const auto physical = effective_value(owner, point).value<PointF>();
            point.unit = value.to_bool() ? AuthoredOverrideUnit::SPATIUM : AuthoredOverrideUnit::MILLIMETRE;
            point.value = physical / unit_scale(owner, point);
        }
    }
    owner.trigger_layout();
    return true;
}

bool reset_child_override(EngravingItem &owner, AuthoredChildKey key, Pid pid) {
    if (!valid_child_key(owner, key)) return false;
    if (key.role == AuthoredChildRole::STEM && pid == Pid::USER_LEN && owner.is_chord()) {
        static_cast<Chord &>(owner).set_stem_user_length(0.0_sp);
    } else if (auto *routed = routed_owner(owner, key, pid)) {
        routed->reset_property(pid);
        return true;
    } else {
        auto *table = authored_override_table(&owner);
        if (!table || !supported_property(key.role, pid)) return false;
        // Carrier identity and dormant legacy ordinal survive a property reset.
        table->erase(key, pid);
    }
    owner.trigger_layout();
    return true;
}

void set_layout_child_key(EngravingItem &item, AuthoredChildKey key) {
    if (item.is_spanner_segment()) {
        const auto parent_key = static_cast<const EngravingItem *>(item.ownership_parent())->authored_child;
        if (nested_spanner_role(parent_key.role)) key = {parent_key.role, key.ordinal, 0, 1};
    }
    item.authored_child = key;
    apply_authored_overrides(item);
}

void set_child_output_property(EngravingItem &item, Pid pid, const PropertyValue &value, PropertyFlags flags) {
    if (!value.is_valid() || pid == Pid::SUBTYPE) return;
    auto *text = item.is_text_base() ? static_cast<TextBase *>(&item) : nullptr;
    auto *line = item.is_s_line() ? static_cast<SLine *>(&item) : nullptr;
    auto *slur = item.is_slur_tie() ? static_cast<SlurTie *>(&item) : nullptr;
    auto *symbol = item.is_symbol() || item.is_hook() ? static_cast<Symbol *>(&item) : nullptr;
    switch (pid) {
        case Pid::COLOR:
            if (line)
                line->set_line_color(value.value<Color>());
            else
                item.EngravingItem::set_color(value.value<Color>());
            break;
        case Pid::VISIBLE: item.EngravingItem::set_visible(value.to_bool()); break;
        case Pid::Z: item.EngravingItem::set_z(value.to_int()); break;
        case Pid::OFFSET: item.set_offset(value.value<PointF>()); break;
        case Pid::MIN_DISTANCE: item.set_min_distance(value.value<Spatium>()); break;
        case Pid::AUTOPLACE: item.EngravingItem::set_autoplace(value.to_bool()); break;
        case Pid::PLACEMENT: item.set_placement(value.value<PlacementV>()); break;
        case Pid::SIZE_SPATIUM_DEPENDENT: item.set_size_is_spatium_dependent(value.to_bool()); break;
        case Pid::POSITION_LINKED_TO_MASTER: item.set_position_linked_to_master(value.to_bool()); break;
        case Pid::APPEARANCE_LINKED_TO_MASTER: item.set_appearance_linked_to_master(value.to_bool()); break;
        case Pid::HORIZONTAL_DIRECTION:
            if (item.is_parenthesis()) static_cast<Parenthesis &>(item).set_direction(value.value<DirectionH>());
            break;
        case Pid::OFFSET2:
            if (item.is_spanner_segment()) static_cast<SpannerSegment &>(item).set_user_off2(value.value<PointF>());
            break;
        case Pid::SLUR_UOFF1:
        case Pid::SLUR_UOFF2:
        case Pid::SLUR_UOFF3:
        case Pid::SLUR_UOFF4:
            if (item.is_slur_tie_segment()) {
                const auto grip = pid == Pid::SLUR_UOFF1 ? Grip::START :
                                  pid == Pid::SLUR_UOFF2 ? Grip::BEZIER1 :
                                  pid == Pid::SLUR_UOFF3 ? Grip::BEZIER2 :
                                                           Grip::END;
                static_cast<SlurTieSegment &>(item).ups(grip).off = value.value<PointF>();
            }
            break;
        case Pid::BEND_VERTEX_OFF:
            if (item.is_guitar_bend_segment()) static_cast<GuitarBendSegment &>(item).set_vertex_point_off(value.value<PointF>());
            break;
        case Pid::LINE_WIDTH:
            if (item.is_stem())
                static_cast<Stem &>(item).set_line_width(value.value<Spatium>());
            else if (line)
                line->set_line_width(value.type() == PType::SPATIUM ? value.value<Spatium>() :
                                                                      Spatium::from_absolute(value.to_double(), item.spatium()));
            break;
        case Pid::LINE_STYLE:
            if (line) line->set_line_style(value.value<LineType>());
            break;
        case Pid::DIAGONAL:
            if (line) line->set_diagonal(value.to_bool());
            break;
        case Pid::DASH_LINE_LEN:
            if (line) line->set_dash_line_len(value.to_double());
            break;
        case Pid::DASH_GAP_LEN:
            if (line) line->set_dash_gap_len(value.to_double());
            break;
        case Pid::PLAY:
            if (item.is_spanner()) static_cast<Spanner &>(item).set_play_spanner(value.to_bool());
            break;
        case Pid::SLUR_DIRECTION:
            if (slur) slur->set_slur_direction(value.value<DirectionV>());
            break;
        case Pid::SLUR_STYLE_TYPE:
            if (slur) slur->set_style_type(value.value<SlurStyleType>());
            break;
        case Pid::MASK_SLURTIE:
            if (slur) slur->set_mask_slur_tie(value.value<AutoOnOff>());
            break;
        case Pid::SYMBOL:
            if (symbol) symbol->set_sym(value.value<SymId>());
            break;
        case Pid::SYMBOLS_SIZE:
            if (symbol) symbol->set_symbols_size(value.to_double());
            break;
        case Pid::SYMBOL_ANGLE:
            if (symbol) symbol->set_sym_angle(value.to_double());
            break;
        case Pid::SCORE_FONT:
            if (symbol) symbol->set_score_font(item.score()->engraving_fonts()->font_by_name(value.value<String>().to_std_string()));
            break;
        case Pid::TEXT_STYLE:
            if (text) text->set_text_style_type(value.value<TextStyleType>());
            break;
        case Pid::FONT_FACE:
            if (text) text->set_output_format(FormatId::FONT_FAMILY, value.value<String>());
            break;
        case Pid::FONT_SIZE:
            if (text) text->set_output_format(FormatId::FONT_SIZE, value.to_double());
            break;
        case Pid::FONT_STYLE:
            if (text) {
                const auto style = FontStyle(value.to_int());
                text->set_output_format(FormatId::BOLD, bool(style & FontStyle::BOLD));
                text->set_output_format(FormatId::ITALIC, bool(style & FontStyle::ITALIC));
                text->set_output_format(FormatId::UNDERLINE, bool(style & FontStyle::UNDERLINE));
                text->set_output_format(FormatId::STRIKE, bool(style & FontStyle::STRIKE));
            }
            break;
        case Pid::TEXT:
            if (text) text->set_xml_text(value.value<String>());
            break;
        case Pid::ALIGN:
            if (text) text->set_align(value.value<Align>());
            break;
        case Pid::POSITION:
            if (text) text->set_position(value.value<AlignH>());
            break;
        case Pid::TEXT_LINE_SPACING:
            if (text) text->set_text_line_spacing(value.to_double());
            break;
        case Pid::FRAME_TYPE:
            if (text) text->set_frame_type(FrameType(value.to_int()));
            break;
        case Pid::FRAME_WIDTH:
            if (text) text->set_frame_width(value.value<Spatium>());
            break;
        case Pid::FRAME_PADDING:
            if (text) text->set_padding_width(value.value<Spatium>());
            break;
        case Pid::FRAME_ROUND:
            if (text) text->set_frame_round(value.value<Spatium>());
            break;
        case Pid::FRAME_FG_COLOR:
            if (text) text->set_frame_color(value.value<Color>());
            break;
        case Pid::FRAME_BG_COLOR:
            if (text) text->set_bg_color(value.value<Color>());
            break;
        case Pid::TEXT_SCRIPT_ALIGN:
            if (text) text->set_output_format(FormatId::VALIGN, value.to_int());
            break;
        case Pid::DIRECTION:
            if (text) text->set_direction(value.value<DirectionV>());
            break;
        case Pid::CENTER_BETWEEN_STAVES:
            if (text) text->set_center_between_staves(value.value<AutoOnOff>());
            break;
        case Pid::VOICE_ASSIGNMENT:
            if (text) text->set_voice_assignment(value.value<VoiceAssignment>());
            break;
        case Pid::MUSIC_SYMBOL_SIZE:
            if (text) text->set_symbol_size(value.to_double());
            break;
        case Pid::MUSICAL_SYMBOLS_SCALE:
            if (text) text->set_symbol_scale(value.to_double());
            break;
        default: break;
    }
    output_flags(item, pid, flags);
}

void apply_authored_overrides(EngravingItem &item) {
    if (item.is_parenthesis() && (!item.ownership_parent() || !item.ownership_parent()->is_chord())) return;
    if (fragment_owner(item) && item.authored_child.role == AuthoredChildRole::NONE) return;
    const auto key = inferred_key(item);
    if (key.role != AuthoredChildRole::NONE) item.authored_child = key;
    const auto *owner = authored_override_owner(item);
    const auto *table = authored_override_table(owner);
    if (!table || item.authored_child.role == AuthoredChildRole::NONE) return;
    if (item.is_parenthesis() && !valid_child_key(*owner, item.authored_child)) return;
    // Reused output must not retain values from a reset or a previous ordinal.
    for (auto pid :
         {Pid::OFFSET, Pid::MIN_DISTANCE, Pid::AUTOPLACE, Pid::PLACEMENT, Pid::POSITION_LINKED_TO_MASTER, Pid::APPEARANCE_LINKED_TO_MASTER,
          Pid::OFFSET2, Pid::SLUR_UOFF1, Pid::SLUR_UOFF2, Pid::SLUR_UOFF3, Pid::SLUR_UOFF4, Pid::BEND_VERTEX_OFF}) {
        set_child_output_property(item, pid, output_default(item, pid), default_flags(item, pid));
    }
    for (const auto &property : *item.styled_properties())
        set_child_output_property(item, property.pid, output_default(item, property.pid), PropertyFlags::STYLED);
    if (!item.is_text_base()) item.set_size_is_spatium_dependent(true);
    item.EngravingItem::set_z(item.is_spanner_segment() ? item.ownership_parent_item()->z() : default_z(item.type()));
    if (item.is_hook())
        for (auto pid : {Pid::SYMBOLS_SIZE, Pid::SYMBOL_ANGLE, Pid::SCORE_FONT})
            set_child_output_property(item, pid, output_default(item, pid), default_flags(item, pid));
    if (item.is_text_base()) {
        auto &text = static_cast<TextBase &>(item);
        const auto style = output_default(item, Pid::TEXT_STYLE);
        set_child_output_property(item, Pid::TEXT_STYLE, style, PropertyFlags::NOSTYLE);
        for (const auto &property : *text_style(text.text_style_type())) {
            if (property.pid == Pid::TEXT || property.pid == Pid::TEXT_STYLE) continue;
            set_child_output_property(item, property.pid, text.property_default(property.pid), PropertyFlags::STYLED);
        }
        if (item.authored_child.role == AuthoredChildRole::TUPLET_NUMBER) static_cast<const Tuplet *>(owner)->apply_number_style();
    }
    if (nested_spanner_role(item.authored_child.role) && item.authored_child.subordinal == 0)
        for (auto pid : {Pid::COLOR, Pid::VISIBLE, Pid::PLAY, Pid::LINE_WIDTH, Pid::LINE_STYLE, Pid::DIAGONAL, Pid::DASH_LINE_LEN,
                         Pid::DASH_GAP_LEN, Pid::SLUR_DIRECTION, Pid::SLUR_STYLE_TYPE, Pid::MASK_SLURTIE})
            set_child_output_property(item, pid, output_default(item, pid), default_flags(item, pid));
    if (!nested_spanner_role(item.authored_child.role) || item.authored_child.subordinal != 0) {
        const auto *defaults_owner = item.is_spanner_segment() ? item.ownership_parent_item() : owner;
        item.EngravingItem::set_color(item.authored_child.role == AuthoredChildRole::SEGMENT_TEXT ?
                                          item.property_default(Pid::COLOR).value<Color>() :
                                          defaults_owner->get_property(Pid::COLOR).value<Color>());
        bool visible = item.authored_child.role == AuthoredChildRole::SEGMENT_TEXT ? true :
                       defaults_owner->is_note() ? static_cast<const Note *>(defaults_owner)->display_visible() :
                                                   defaults_owner->visible();
        if (item.is_parenthesis() && owner->is_chord() && item.authored_child.subordinal == 1) {
            const auto &groups = static_cast<const Chord *>(owner)->layout_parentheses();
            if (item.authored_child.ordinal < groups.size())
                visible = std::ranges::any_of(groups[item.authored_child.ordinal].info.notes(),
                                              [](const Note *note) { return note->display_visible(); });
        }
        item.EngravingItem::set_visible(visible);
    }
    for (const auto &entry : table->entries)
        if (entry.child == item.authored_child) set_child_output_property(item, entry.pid, effective_value(item, entry), entry.flags);
}

void clone_authored_overrides(AuthoredOverrideTable &destination, const AuthoredOverrideTable &source, EIDRegister &registry) {
    destination = source;
    for (auto &carrier : destination.carriers) {
        const bool group = carrier.child.subordinal == 1 &&
                           (carrier.child.role == AuthoredChildRole::PAREN_LEFT || carrier.child.role == AuthoredChildRole::PAREN_RIGHT);
        carrier.legacy_eid = registry.new_eid(group);
    }
}

} // namespace mu::engraving
