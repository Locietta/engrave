// SPDX-License-Identifier: GPL-3.0-only
#include "notes.h"

#include "dom/chord.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/tie.h"

using namespace mu::engraving;

bool mu::engraving::normalize_cross_measure(Chord &value, bool on) {
    Chord *chord = &value;
    bool refresh_stem = false;
    if (!on) {
        if (chord->cross_measure() != CrossMeasure::UNKNOWN) {
            chord->set_cross_measure(CrossMeasure::UNKNOWN);
            refresh_stem = true;
        }
        return refresh_stem;
    }
    if (chord->cross_measure() == CrossMeasure::UNKNOWN) {
        CrossMeasure temp_cross = CrossMeasure::NONE; // assume no cross-measure modification
        // if chord has only one note and note is tied forward
        if (chord->notes().size() == 1 && chord->notes().at(0)->tie_for()) {
            Chord *tied_chord = chord->notes().at(0)->tie_for()->end_note()->chord();
            // if tied note belongs to another measure and to a single-note chord
            if (tied_chord->measure() != chord->measure() && tied_chord->notes().size() == 1) {
                // get total duration
                std::vector<TDuration> dur_list =
                    to_duration_list(chord->actual_duration_type().fraction() + tied_chord->actual_duration_type().fraction(), true);
                // if duration can be expressed as a single duration
                // apply cross-measure modification
                if (dur_list.size() == 1) {
                    temp_cross = CrossMeasure::FIRST;
                    chord->set_cross_measure(temp_cross);
                    chord->set_cross_measure_duration_type(dur_list[0]);
                    refresh_stem = true;
                }
            }
            chord->set_cross_measure(temp_cross);
            tied_chord->set_cross_measure(temp_cross == CrossMeasure::FIRST ? CrossMeasure::SECOND : CrossMeasure::NONE);
        }
    }
    return refresh_stem;
}
