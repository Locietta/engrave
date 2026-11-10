#pragma once

#include <optional>
#include <vector>

// Grouped grace timing of the MIDI-file exporter: a principal chord with BOTH a before-group and an after-group.
// Shared by compatmidirender.cpp (which lays out the events) and compatmidirenderinternal.cpp (which turns them into
// ticks and must keep them on the exact layout boundaries).

namespace mu::engraving {
struct Chord;

// Exact chord-relative tick boundaries, 0 .. principal_ticks. Rounding the per-mille NoteEvent units (1/1000 of the
// principal) cannot hit every tick of a principal over 1000 ticks, so the exporter snaps events to these boundaries.
struct GroupedGraceLayout {
    int principal_ticks = 0;
    std::vector<int> before_bounds; // one more entry than before-graces: 0 .. end of the before-group, in grace order
    std::vector<int> after_bounds;  // one more entry than after-graces: start of the after-group .. principal_ticks

    int principal_begin() const { return before_bounds.back(); }

    int principal_end() const { return after_bounds.front(); }
};

// std::nullopt unless the grouped rule applies (see compatmidirender.cpp). A principal with a single group, drumset
// flams, bent graces, and chords whose trill or articulation already plays the graces keep the legacy timing.
std::optional<GroupedGraceLayout> grouped_grace_layout(Chord *chord);

inline bool grace_groups_share_principal(Chord *chord) { return grouped_grace_layout(chord).has_value(); }
} // namespace mu::engraving
