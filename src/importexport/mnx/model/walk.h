// SPDX-License-Identifier: GPL-3.0-only
// Walking a sequence's content with MNX timing. Ported from mnxdom's WalkSequenceContent.h
// (MIT, Robert Patterson).
#pragma once

#include <functional>

#include "mnx_model.h"
#include "rational.h"

namespace mu::iex::mnxio::model {

struct WalkContext {
    Rational elapsed;      // metric time from the start of the sequence
    Rational ratio{1};     // accumulated tuplet (and tremolo) time ratio
    bool in_grace = false; // inside grace note content
};

enum struct WalkControl {
    CONTINUE,
    SKIP_CHILDREN, // do not descend into this item's content
    STOP,          // end the walk; walk_sequence returns false
};

struct WalkHooks {
    // Called for every item of a content list (a sequence's or a tuplet's) before its children or
    // its time are taken. The events inside grace notes and tremolos are not content items; they
    // reach only on_event.
    std::function<WalkControl(const SequenceItem &item, WalkContext &context)> on_item;
    // Called for each event with its start and actual (ratio-scaled) duration; false stops.
    std::function<bool(const Event &event, const Rational &start, const Rational &duration, WalkContext &context)> on_event;
    // Called when the sequence is a full-measure rest; false stops.
    std::function<bool(const FullMeasureRest &rest, const Rational &start, const Rational &duration, WalkContext &context)> on_full_measure;
    // Called for every content list item after its children and its time.
    std::function<void(const SequenceItem &item, WalkContext &context)> on_after_item;
};

// Walks `sequence` in document order. Grace notes take no time; tuplets scale their content by
// outer / inner; a multi-note tremolo scales its events by 1 / multiple and always spans its
// outer value. `full_measure_length` is the length of a full-measure rest (the bar length).
// The document must have passed build_index. Returns false if a hook stopped the walk.
bool walk_sequence(const Sequence &sequence, const Rational &full_measure_length, const WalkHooks &hooks, WalkContext *context = nullptr);

} // namespace mu::iex::mnxio::model
