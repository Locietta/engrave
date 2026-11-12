/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <cstdint>
#include <limits>
#include <span>

#include <amc/vector.hpp>

#include "dom/chordline.h"
#include "editing/editdata.h"
#include "infrastructure/sourcehandle.h"

namespace mu::engraving {
struct MasterScore;
struct Score;

// Source lifetime, including a detached object retained by the active journal.
// The document owns the journal and outlives every target stored in it.
struct JournalTarget {
    MasterScore *document = nullptr;
    SourceHandle handle;

    static JournalTarget capture(EngravingObject &object);
    EngravingObject &object() const;
};

inline constexpr uint32_t no_source_payload = std::numeric_limits<uint32_t>::max();

struct PropertyInverse {
    JournalTarget target;
    Pid id;
    PropertyValue before;
    PropertyFlags flags = PropertyFlags::NOSTYLE;
    uint32_t appearance = no_source_payload;
};

struct StyleValue {
    Sid id;
    PropertyValue value;
};

struct StyleValuesInverse {
    JournalTarget target;
    uint32_t begin = 0;
    uint32_t count = 0;
    uint32_t spatium_sources = no_source_payload;
};

struct OffsetBefore {
    JournalTarget target;
    PointF value;
};

struct ArpeggioLengthsBefore {
    JournalTarget target;
    double first;
    double second;
};

struct BeamPositionBefore {
    JournalTarget target;
    uint32_t fragment;
    int direction;
    double first;
    double second;
};

struct SlurGripsBefore {
    JournalTarget target;
    std::array<PointF, int(Grip::GRIPS)> values;
};

struct ChordLineControlsBefore {
    JournalTarget target;
    std::array<PointF, 4> values;
};

// Only authored override values touched by the existing spatium callbacks.
// LayoutData, generated paths and other rebuildable geometry are not retained.
struct SpatiumSources {
    amc::vector<OffsetBefore> offsets;
    amc::vector<OffsetBefore> spanner_ends;
    amc::vector<ArpeggioLengthsBefore> arpeggios;
    amc::vector<BeamPositionBefore> beams;
    amc::vector<SlurGripsBefore> slurs;
    amc::vector<ChordLineControlsBefore> chord_lines;
};

// Each family pays for its own payloads. Journal entries keep only kind/index;
// chord-line appearance records are allocated only for appearance changes.
struct SourceChangePayloads {
    amc::vector<PropertyInverse> properties;
    amc::vector<ChordLine::AppearanceState> appearances;
    amc::vector<StyleValuesInverse> styles;
    amc::vector<StyleValue> style_values;
    amc::vector<SpatiumSources> spatium_sources;
};

PropertyInverse capture_property(EngravingObject &object, Pid id, SourceChangePayloads &payloads);
void apply_property(EngravingObject &object, Pid id, const PropertyValue &value, PropertyFlags flags, bool reset_path);
void restore_property(const PropertyInverse &inverse, const SourceChangePayloads &payloads);

StyleValuesInverse capture_style_values(Score &score, std::span<const StyleValue> values, SourceChangePayloads &payloads);
void apply_style_values(Score &score, std::span<const StyleValue> values, const SpatiumSources *restore_spatium = nullptr);
void restore_style_values(const StyleValuesInverse &inverse, const SourceChangePayloads &payloads);
void restore_spatium_sources(const SpatiumSources &sources);

// Test-only failure seam after one existing spatium callback in the score walk.
using SpatiumItemHook = void (*)(const EngravingItem *);
void set_spatium_item_hook(SpatiumItemHook hook);
SpatiumItemHook spatium_item_hook();
} // namespace mu::engraving
