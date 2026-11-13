/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <algorithm>
#include <compare>
#include <cstdint>
#include <optional>
#include <vector>

#include "../infrastructure/eid.h"
#include "property.h"

namespace mu::engraving {
struct EngravingItem;
struct EngravingObject;

enum class AuthoredChildRole : uint8_t {
    NONE,
    STEM,
    HOOK,
    STEM_SLASH,
    DOT,
    PAREN_LEFT,
    PAREN_RIGHT,
    TUPLET_NUMBER,
    BEAM_FRAGMENT,
    SPANNER_SEGMENT,
    SEGMENT_TEXT
};

// Direction qualifies the legacy fragment ordinal; it is not a system/page key.
struct AuthoredChildKey {
    AuthoredChildRole role = AuthoredChildRole::NONE;
    uint8_t direction = 0;
    uint32_t ordinal = 0;
    uint32_t subordinal = 0;
    AuthoredChildKey() = default;

    AuthoredChildKey(AuthoredChildRole role, uint32_t ordinal = 0, uint8_t direction = 0, uint32_t subordinal = 0)
        : role(role), direction(direction), ordinal(ordinal), subordinal(subordinal) {}

    auto operator<=>(const AuthoredChildKey &other) const {
        if (role != other.role) return role <=> other.role;
        if (ordinal != other.ordinal) return ordinal <=> other.ordinal;
        if (subordinal != other.subordinal) return subordinal <=> other.subordinal;
        return direction <=> other.direction;
    }

    bool operator==(const AuthoredChildKey &) const = default;
};

enum class AuthoredOverrideUnit : uint8_t { NATIVE, SPATIUM, MILLIMETRE };

struct AuthoredOverrideEntry {
    AuthoredChildKey child;
    Pid pid = Pid::END;
    PropertyValue value;
    PropertyFlags flags = PropertyFlags::NOSTYLE;
    AuthoredOverrideUnit unit = AuthoredOverrideUnit::NATIVE;
};

struct AuthoredOverrideCarrier {
    AuthoredChildKey child;
    ElementType type = ElementType::INVALID;
    EID legacy_eid = EID::invalid();
    track_idx_t track = muse::nidx;
};

struct AuthoredOverrideTable {
    std::vector<AuthoredOverrideEntry> entries;
    std::vector<AuthoredOverrideCarrier> carriers;

    const AuthoredOverrideEntry *find(AuthoredChildKey child, Pid pid) const;
    void set(AuthoredOverrideEntry entry);
    void erase(AuthoredChildKey child, Pid pid);
    const AuthoredOverrideCarrier *carrier(AuthoredChildKey child) const;
    void set_carrier(AuthoredOverrideCarrier carrier);
    void erase_carrier(AuthoredChildKey child);
    bool has_entries(AuthoredChildKey child) const;
};

AuthoredOverrideTable *authored_override_table(EngravingItem *owner);
const AuthoredOverrideTable *authored_override_table(const EngravingItem *owner);
EngravingItem *authored_override_owner(const EngravingItem &item);
bool authored_override_handles(const EngravingObject &item, Pid pid);
std::optional<PropertyValue> authored_override_value(const EngravingObject &item, Pid pid);
std::optional<PropertyFlags> authored_override_flags(const EngravingObject &item, Pid pid);
bool set_authored_override(EngravingItem &item, Pid pid, const PropertyValue &value);
bool set_authored_override_flags(EngravingObject &item, Pid pid, PropertyFlags flags);
void bind_authored_child(EngravingItem &item, AuthoredChildKey key);
void apply_authored_overrides(EngravingItem &item);
void remember_authored_property(EngravingItem &item, Pid pid, const PropertyValue &value, PropertyFlags flags);
void finish_authored_read(EngravingItem &item);
bool has_authored_override(const EngravingItem &item, Pid pid);
void reset_authored_override(EngravingItem &item, Pid pid);
void clone_authored_overrides(AuthoredOverrideTable &destination, const AuthoredOverrideTable &source);

// A per-display-object scope, not a document/global layout mode.
struct AuthoredOverrideMirror {
    EngravingItem &item;
    bool previous;
    explicit AuthoredOverrideMirror(EngravingItem &item);
    ~AuthoredOverrideMirror();
};
} // namespace mu::engraving
