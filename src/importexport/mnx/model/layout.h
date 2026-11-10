// SPDX-License-Identifier: GPL-3.0-only
// System layouts flattened into staff and group spans. Ported from mnxdom's LayoutHelpers.h
// (MIT, Robert Patterson).
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "mnx_model.h"

namespace mu::iex::mnxio::model {

// A staff of a part.
struct StaffKey {
    std::string part;
    int staff = 1;
    bool operator==(const StaffKey &) const = default;
};

// The part staves a layout staff shows, in source order, or nothing when a part staff appears
// more than once without a distinct voice on every occurrence.
std::optional<std::vector<StaffKey>> staff_keys(const Staff &staff);

// The layout's staves in visual (depth-first) order.
std::vector<const Staff *> flatten_staves(const SystemLayout &layout);

// How a group joins the barlines of its staves.
enum struct BarlineOverride {
    NONE,
    UNIFIED,
    MENSURSTRICH,
};

// Whether every staff in the group comes from one part.
bool is_part_group(const StaffGroup &group);
BarlineOverride barline_override(const StaffGroup &group);

// A staff or group of a flattened layout, covering staff indices [start, end].
struct LayoutSpan {
    enum struct Kind { STAFF, GROUP };
    Kind kind = Kind::STAFF;
    std::size_t start = 0;
    std::size_t end = 0;
    // The content tree level: groups at their container's depth, staves one deeper, so root-level
    // groups have depth 0 and ungrouped staves depth 1.
    std::size_t depth = 0;
    std::optional<std::string> label;
    std::optional<StaffLabelref> labelref;
    std::optional<StaffSymbol> symbol;
    BarlineOverride barline = BarlineOverride::NONE;
    std::optional<std::vector<StaffKey>> sources; // staff spans only
};

// The spans of a layout sorted by start index, then depth (outer groups first). Groups without
// staves are skipped.
std::vector<LayoutSpan> layout_spans(const SystemLayout &layout);
// A layout synthesized from the parts: each part's staves, braced and unified when it has more
// than one.
std::vector<LayoutSpan> default_layout_spans(const Root &root);
// The layout that shows every part staff once, in part order, if there is one.
const SystemLayout *full_score_layout(const Root &root);

} // namespace mu::iex::mnxio::model
