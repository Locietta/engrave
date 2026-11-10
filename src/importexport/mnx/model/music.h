// SPDX-License-Identifier: GPL-3.0-only
// Musical values computed from MNX model objects: durations, time signatures and pitches.
#pragma once

#include "mnx_model.h"
#include "rational.h"

namespace mu::iex::mnxio::model {

// The length of a note value in whole notes (a quarter is 1/4), dots included.
Rational duration_of(const NoteValue &value);
// `multiple` times the note value.
Rational duration_of(const NoteValueQuantity &quantity);
// The bar length of a time signature in whole notes.
Rational duration_of(const Time &time);
// The time ratio a tuplet applies to its content (outer / inner). The inner value must not be
// zero (build_index checks).
Rational tuplet_ratio(const Tuplet &tuplet);

// The number of flags (or beams) a note value carries.
int flag_count(NoteValueBase base);

// A spelled pitch.
struct SpelledPitch {
    Step step = Step::C;
    int octave = 4;
    int alter = 0;
    bool operator==(const SpelledPitch &) const = default;
};

SpelledPitch spelled(const Pitch &pitch);
// Whether two pitches sound the same (identical or enharmonically equivalent in 12-EDO).
bool same_pitch(const Pitch &lhs, const Pitch &rhs);
// The pitch as 12-EDO half-steps from middle C's octave base, for comparing sounded heights.
int semitones(const Pitch &pitch);

// The key signature, in fifths, a transposing part shows for a concert key.
int transposed_key(const PartTransposition &transposition, int concert_fifths);
// The written pitch of a note in a part with this transposition when the concert key is
// `concert_fifths`. Without a transposition the written pitch is the stored pitch.
SpelledPitch written_pitch(const Note &note, const std::optional<PartTransposition> &transposition, int concert_fifths);

} // namespace mu::iex::mnxio::model
