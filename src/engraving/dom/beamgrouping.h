/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "chordrest.h"
#include "segment.h"

namespace mu::engraving {
struct BeamGroupStep {
    ChordRest *pending = nullptr;
    ChordRest *discard = nullptr;
    ChordRest *first = nullptr;
    bool finish_before = false;
    bool finish_after = false;
    bool detach = false;
    bool append = false;
};

// Source-only transitions shared by rendering and authored-beam edits. END closes
// an established beam, but does not close the initial pair or a pending rest.
inline BeamGroupStep beam_group_step(ChordRest *pending, bool active, ChordRest *cr, BeamMode mode, bool grace) {
    BeamGroupStep step;
    step.pending = pending;
    const bool beamable = grace || cr->is_rest() || cr->duration_type().type() > DurationType::V_QUARTER;
    if (!beamable || mode == BeamMode::NONE) {
        step.finish_before = active;
        step.discard = pending;
        step.pending = nullptr;
        step.detach = true;
        return step;
    }
    if (active) {
        if (mode == BeamMode::BEGIN) {
            step.finish_before = true;
        } else {
            step.append = true;
            step.finish_after = mode == BeamMode::END;
            return step;
        }
    }
    if (!pending) {
        step.pending = cr;
    } else {
        const bool mid = mode == BeamMode::MID || mode == BeamMode::BEGIN16 || mode == BeamMode::BEGIN32;
        if (!mid && (mode == BeamMode::BEGIN || (!grace && (pending->segment()->segment_type() != cr->segment()->segment_type() ||
                                                            pending->end_tick() < cr->tick())))) {
            step.discard = pending;
            step.pending = cr;
        } else {
            step.first = pending;
            step.pending = nullptr;
            step.append = true;
        }
    }
    return step;
}
} // namespace mu::engraving
