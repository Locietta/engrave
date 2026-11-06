/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "log.h"
#include "musicxmltupletstate.h"

#include "engraving/dom/tuplet.h"

#include "internal/shared/musicxmltypes.h"

using namespace mu::engraving;
using namespace mu::iex::musicxml;

//---------------------------------------------------------
//   determineTupletFractionAndFullDuration
//---------------------------------------------------------

/**
 Split duration into two factors where fullDuration is note sized
 (i.e. the denominator is a power of 2), 1/2 < fraction <= 1/1
 and fraction * fullDuration equals duration.
 */

void MusicXmlTupletState::determine_tuplet_fraction_and_full_duration(const Fraction duration, Fraction &fraction,
                                                                      Fraction &full_duration) {
    fraction = duration;
    full_duration = Fraction(1, 1);
    // move denominator's powers of 2 from fraction to fullDuration
    while (fraction.denominator() % 2 == 0) {
        fraction *= 2;
        fraction.reduce();
        full_duration *= Fraction(1, 2);
    }
    // move numerator's powers of 2 from fraction to fullDuration
    while (fraction.numerator() % 2 == 0) {
        fraction *= Fraction(1, 2);
        fraction.reduce();
        full_duration *= 2;
        full_duration.reduce();
    }
    // make sure 1/2 < fraction <= 1/1
    while (fraction <= Fraction(1, 2)) {
        full_duration *= Fraction(1, 2);
        fraction *= 2;
    }
    full_duration.reduce();
    fraction.reduce();

    /*
    Examples (note result when denominator is not a power of two):
    3:2 tuplet of 1/4 results in fraction 1/1 and fullDuration 1/2
    2:3 tuplet of 1/4 results in fraction 3/1 and fullDuration 1/4
    4:3 tuplet of 1/4 results in fraction 3/1 and fullDuration 1/4
    3:4 tuplet of 1/4 results in fraction 1/1 and fullDuration 1/1

     Bring back fraction in 1/2 .. 1/1 range.
     */

    if (fraction > Fraction(1, 1) && fraction.denominator() == 1) {
        full_duration *= fraction;
        full_duration.reduce();
        fraction = Fraction(1, 1);
    }

    /*
    LOGD("duration %s fraction %s fullDuration %s",
           muPrintable(duration.toString()),
           muPrintable(fraction.toString()),
           muPrintable(fullDuration.toString())
           );
    */
}

//---------------------------------------------------------
//   missingTupletDuration
//---------------------------------------------------------

Fraction MusicXmlTupletState::missing_tuplet_duration(const Fraction duration) {
    Fraction tuplet_fraction;
    Fraction tuplet_full_duration;

    determine_tuplet_fraction_and_full_duration(duration, tuplet_fraction, tuplet_full_duration);
    Fraction missing = (Fraction(1, 1) - tuplet_fraction) * tuplet_full_duration;

    return missing;
}

//---------------------------------------------------------
//   isTupletFilled
//---------------------------------------------------------

/**
 Determine if the tuplet is completely filled,
 because either (1) it is at least the same duration
 as the specified number of the specified normal type notes
 or (2) the duration adds up to a normal note duration.

 Example (1): a 3:2 tuplet with a 1/4 and a 1/8 note
 is filled if normal type is 1/8,
 it is not filled if normal type is 1/4.

 Example (2): a 3:2 tuplet with a 1/4 and a 1/8 note is filled.
 */

bool MusicXmlTupletState::is_tuplet_filled(const TDuration normal_type, const Fraction time_mod) {
    UNUSED(time_mod);
    bool res = false;
    /*
    const auto normalNotes = state.normalNotes_;
    LOGD("duration %s normalType %s timeMod %s normalNotes %d actualNotes %d",
           muPrintable(state.duration_.toString()),
           muPrintable(normalType.fraction().toString()),
           muPrintable(timeMod.toString()),
           normalNotes,
           actualNotes
           );
    */

    if (normal_type.is_valid()) {
        int matched_normal_type = int(normal_type.type());
        int matched_normal_count = actual_notes;
        // match the types
        match_type_and_count(matched_normal_type, matched_normal_count);
        // ... result scenario (1)
        res = smallest_note_count >= matched_normal_count;
        /*
        LOGD("normalType valid tupletType %d tupletCount %d matchedNormalType %d matchedNormalCount %d res %d",
               tupletType,
               tupletCount,
               matchedNormalType,
               matchedNormalCount,
               res
               );
         */
    } else {
        // ... result scenario (2)
        res = smallest_note_count >= actual_notes;
        /*
        LOGD("normalType not valid tupletCount %d actualNotes %d res %d",
               tupletCount,
               actualNotes,
               res
               );
         */
    }
    return res;
}

//---------------------------------------------------------
//   smallestTypeAndCount
//---------------------------------------------------------

/**
 Determine the smallest note type and the number of those
 present in a ChordRest.
 For a note without dots the type equals the note type
 and count is one.
 For a single dotted note the type equals half the note type
 and count is three.
 A double dotted note is similar.
 Note: code assumes when duration().type() is incremented,
 the note length is divided by two, checked by tupletAssert().
 */

void MusicXmlTupletState::smallest_type_and_count(const TDuration dur_type, int &type, int &count) {
    type = int(dur_type.type());
    count = 1;
    switch (dur_type.dots()) {
        case 0:
            // nothing to do
            break;
        case 1:
            type += 1; // next-smaller type
            count = 3;
            break;
        case 2:
            type += 2; // next-next-smaller type
            count = 7;
            break;
        default: LOGD("smallestTypeAndCount() does not support more than 2 dots");
    }
}

//---------------------------------------------------------
//   matchTypeAndCount
//---------------------------------------------------------

/**
 Given two note types and counts, if the types are not equal,
 make them equal by successively doubling the count of the
 largest type.
 */

void MusicXmlTupletState::match_type_and_count(int &note_type, int &note_count) {
    while (smallest_note_type < note_type) {
        smallest_note_type++;
        smallest_note_count *= 2;
    }
    while (note_type < smallest_note_type) {
        note_type++;
        note_count *= 2;
    }
}

//---------------------------------------------------------
//   addDurationToTuplet
//---------------------------------------------------------

/**
 Add duration to tuplet duration
 Determine type and number of smallest notes in the tuplet
 */

void MusicXmlTupletState::add_duration_to_tuplet(const Fraction dur, const Fraction time_mod) {
    /*
    LOGD("1 duration %s timeMod %s -> state.tupletType %d state.tupletCount %d state.actualNotes %d state.normalNotes %d",
           muPrintable(duration.print()),
           muPrintable(timeMod.print()),
           tupletType_,
           tupletCount_,
           actualNotes_,
           normalNotes_
           );
    */
    if (duration <= Fraction(0, 1)) {
        // first note: init variables
        actual_notes = time_mod.denominator();
        normal_notes = time_mod.numerator();
        smallest_type_and_count(dur / time_mod, smallest_note_type, smallest_note_count);
    } else {
        int note_type = 0;
        int note_count = 0;
        smallest_type_and_count(dur / time_mod, note_type, note_count);
        // match the types
        match_type_and_count(note_type, note_count);
        smallest_note_count += note_count;
    }
    duration += dur;
    /*
    LOGD("2 duration %s -> state.tupletType %d state.tupletCount %d state.actualNotes %d state.normalNotes %d",
           muPrintable(duration.print()),
           tupletType_,
           tupletCount_,
           actualNotes_,
           normalNotes_
           );
    */
}

//---------------------------------------------------------
//   determineTupletAction
//---------------------------------------------------------

/**
 Update tuplet state using parse result tupletDesc.
 Tuplets with <actual-notes> and <normal-notes> but without <tuplet>
 are handled correctly.
 TODO Nested tuplets are not (yet) supported.
 */

MusicXmlTupletFlags MusicXmlTupletState::determine_tuplet_action(const Fraction note_duration, const Fraction time_mod,
                                                                 const MusicXmlStartStop tuplet_start_stop, const TDuration normal_type,
                                                                 Fraction &missing_previous_duration, Fraction &missing_current_duration) {
    const int act_notes = time_mod.denominator();
    const int nor_notes = time_mod.numerator();
    MusicXmlTupletFlags res = MusicXmlTupletFlag::NONE;

    // check for unexpected termination of previous tuplet
    if (in_tuplet && time_mod == Fraction(1, 1)) {
        // recover by simply stopping the current tuplet first
        if (!is_tuplet_filled(normal_type, time_mod)) {
            missing_previous_duration = missing_tuplet_duration(duration);
            //LOGD("tuplet incomplete, missing %s", muPrintable(missingPreviousDuration.print()));
        }
        *this = {};
        res |= MusicXmlTupletFlag::STOP_PREVIOUS;
    }

    // check for obvious errors
    if (in_tuplet && tuplet_start_stop == MusicXmlStartStop::START) {
        LOGD("tuplet already started");
        // recover by simply stopping the current tuplet first
        if (!is_tuplet_filled(normal_type, time_mod)) {
            missing_previous_duration = missing_tuplet_duration(duration);
            //LOGD("tuplet incomplete, missing %s", muPrintable(missingPreviousDuration.print()));
        }
        *this = {};
        res |= MusicXmlTupletFlag::STOP_PREVIOUS;
    }
    if (tuplet_start_stop == MusicXmlStartStop::STOP && !in_tuplet) {
        LOGD("tuplet stop but no tuplet started"); // TODO
        // recovery handled later (automatically, no special case needed)
    }

    // Tuplet are either started by the tuplet start
    // or when the time modification is first found.
    if (!in_tuplet) {
        if (tuplet_start_stop == MusicXmlStartStop::START || (!in_tuplet && (act_notes != 1 || nor_notes != 1))) {
            if (tuplet_start_stop != MusicXmlStartStop::START) {
                implicit = true;
            } else {
                implicit = false;
            }
            // create a new tuplet
            in_tuplet = true;
            res |= MusicXmlTupletFlag::START_NEW;
        }
    }

    // Add chord to the current tuplet.
    // Must also check for actual/normal notes to prevent
    // adding one chord too much if tuplet stop is missing.
    if (in_tuplet && !(act_notes == 1 && nor_notes == 1)) {
        add_duration_to_tuplet(note_duration, time_mod);
        res |= MusicXmlTupletFlag::ADD_CHORD;
    }

    // Tuplets are stopped by the tuplet stop
    // or when the tuplet is filled completely
    // (either with knowledge of the normal type
    // or as a last resort calculated based on
    // actual and normal notes plus total duration)
    // or when the time-modification is not found.

    if (in_tuplet) {
        if (tuplet_start_stop == MusicXmlStartStop::STOP || (implicit && is_tuplet_filled(normal_type, time_mod)) ||
            (act_notes == 1 && nor_notes == 1)) { // incorrect ??? check scenario incomplete tuplet w/o start
            if (act_notes > nor_notes && !is_tuplet_filled(normal_type, time_mod)) {
                missing_current_duration = missing_tuplet_duration(duration);
                LOGD("current tuplet incomplete, missing %s", muPrintable(missing_current_duration.to_string()));
            }

            *this = {};
            res |= MusicXmlTupletFlag::STOP_CURRENT;
        }
    }

    return res;
}
