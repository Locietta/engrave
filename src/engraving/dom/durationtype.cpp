/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "durationtype.h"

#include "log.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "sig.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   dots
//---------------------------------------------------------

static int get_dots(int base, int rest, int *dots) {
    if (base < 16) {
        return rest;
    }
    *dots = 0;
    if (rest >= base / 2) {
        *dots = *dots + 1;
        rest -= base / 2;
    }
    if (rest >= base / 4) {
        *dots = *dots + 1;
        rest -= base / 4;
    }
    if (rest >= base / 8) {
        *dots = *dots + 1;
        rest -= base / 8;
    }
    if (rest >= base / 16) {
        *dots = *dots + 1;
        rest -= base / 16;
    }
    if (*dots > ::mu::engraving::max_dots) {
        *dots = ::mu::engraving::max_dots;
    }
    return rest;
}

//---------------------------------------------------------
//   setDots
//---------------------------------------------------------

void TDuration::set_dots(int v) {
    if (v > ::mu::engraving::max_dots) {
        v = ::mu::engraving::max_dots;
    }
    if (v < 0) {
        v = 0;
    }
    dots_ = v;
}

//---------------------------------------------------------
//   setVal
//---------------------------------------------------------

void TDuration::set_val(int ticks) {
    if (ticks == 0) {
        val_ = DurationType::V_MEASURE;
    } else {
        TDuration dt;
        for (int i = 0; i < int(DurationType::V_ZERO); ++i) {
            dt.set_type(DurationType(i));
            int t = dt.ticks().ticks();
            if (ticks / t) {
                int remain = ticks % t;
                const int smallest_dot_divisor = 1 << ::mu::engraving::max_dots;
                if ((t - remain) < (t / smallest_dot_divisor)) {
                    val_ = DurationType(i - 1);
                    return;
                }
                val_ = DurationType(i);
                get_dots(t, remain, &dots_);
                return;
            }
        }
        LOGD("2: no duration type for ticks %d", ticks);
        val_ = DurationType::V_QUARTER; // fallback default value
    }
}

//---------------------------------------------------------
//   ticks
//---------------------------------------------------------

Fraction TDuration::ticks() const {
    Fraction t;
    switch (val_) {
        case DurationType::V_QUARTER: t = Fraction(1, 4); break;
        case DurationType::V_1024TH: t = Fraction(1, 1024); break;
        case DurationType::V_512TH: t = Fraction(1, 512); break;
        case DurationType::V_256TH: t = Fraction(1, 256); break;
        case DurationType::V_128TH: t = Fraction(1, 128); break;
        case DurationType::V_64TH: t = Fraction(1, 64); break;
        case DurationType::V_32ND: t = Fraction(1, 32); break;
        case DurationType::V_16TH: t = Fraction(1, 16); break;
        case DurationType::V_EIGHTH: t = Fraction(1, 8); break;
        case DurationType::V_HALF: t = Fraction(1, 2); break;
        case DurationType::V_WHOLE: t = Fraction(1, 1); break;
        case DurationType::V_BREVE: t = Fraction(2, 1); break;
        case DurationType::V_LONG: t = Fraction(4, 1); break;
        case DurationType::V_ZERO:
        case DurationType::V_MEASURE: return Fraction(0, 1);
        default:
        case DurationType::V_INVALID: return Fraction(-1, 1);
    }
    Fraction tmp = t;
    for (int i = 0; i < dots_; ++i) {
        tmp *= Fraction(1, 2);
        t += tmp;
    }
    return t;
}

//---------------------------------------------------------
//   headType
//---------------------------------------------------------

NoteHeadType TDuration::head_type() const {
    NoteHeadType head_type = NoteHeadType::HEAD_WHOLE;
    switch (val_) {
        case DurationType::V_1024TH:
        case DurationType::V_512TH:
        case DurationType::V_256TH:
        case DurationType::V_128TH:
        case DurationType::V_64TH:
        case DurationType::V_32ND:
        case DurationType::V_16TH:
        case DurationType::V_EIGHTH:
        case DurationType::V_QUARTER: head_type = NoteHeadType::HEAD_QUARTER; break;
        case DurationType::V_HALF: head_type = NoteHeadType::HEAD_HALF; break;
        case DurationType::V_MEASURE:
        case DurationType::V_WHOLE: head_type = NoteHeadType::HEAD_WHOLE; break;
        case DurationType::V_BREVE: head_type = NoteHeadType::HEAD_BREVIS; break;
        case DurationType::V_LONG: head_type = NoteHeadType::HEAD_BREVIS; break;
        default:
        case DurationType::V_INVALID:
        case DurationType::V_ZERO: head_type = NoteHeadType::HEAD_QUARTER; break;
    }
    return head_type;
}

//---------------------------------------------------------
//   hooks
//---------------------------------------------------------

int TDuration::hooks() const {
    static constexpr int table[] = {// V_LONG, V_BREVE, V_WHOLE, V_HALF, V_QUARTER, V_EIGHTH, V_16TH,
                                    0, 0, 0, 0, 0, 1, 2,
                                    // V_32ND, V_64TH, V_128TH, V_256TH, V_512TH, V_1024TH,
                                    3, 4, 5, 6, 7, 8,
                                    // V_ZERO, V_MEASURE, V_INVALID
                                    0, 0, 0};
    return table[int(val_)];
}

//---------------------------------------------------------
//   hasStem
//---------------------------------------------------------

bool TDuration::has_stem() const {
    switch (val_) {
        case DurationType::V_1024TH:
        case DurationType::V_512TH:
        case DurationType::V_256TH:
        case DurationType::V_128TH:
        case DurationType::V_64TH:
        case DurationType::V_32ND:
        case DurationType::V_16TH:
        case DurationType::V_EIGHTH:
        case DurationType::V_QUARTER:
        case DurationType::V_HALF:
        case DurationType::V_LONG: return true;
        default: return false;
    }
}

//---------------------------------------------------------
//   shiftType
//    If stepDotted = false, duration type will inc/dec by
//    nSteps with _dots remaining same.
//
//    If stepDotted = true, duration will round toward zero
//    to next single-dotted or undotted duration and then
//    will included dotted durations when stepping
//---------------------------------------------------------

void TDuration::shift_type(int n_steps, bool step_dotted) {
    if (val_ == DurationType::V_MEASURE || val_ == DurationType::V_INVALID || val_ == DurationType::V_ZERO) {
        set_type(DurationType::V_INVALID);
    } else {
        int new_value;
        int new_dots;
        if (step_dotted) {
            // figure out the new duration in terms of the number of single dotted or undotted steps from DurationType::V_LONG
            int round_down_single_dots = (dots_ > 0) ? -1 : 0;
            int new_val_as_num_single_dot_steps = int(val_) * 2 + round_down_single_dots + n_steps;

            // convert that new duration back into terms of DurationType integer value and number of dots
            new_dots = new_val_as_num_single_dot_steps % 2;             // odd means there is a dot
            new_value = new_val_as_num_single_dot_steps / 2 + new_dots; // if new duration has a dot, then that
        } else {
            new_dots = dots_;
            new_value = int(val_) + n_steps;
        }

        if ((new_value < int(DurationType::V_LONG)) || (new_value > int(DurationType::V_1024TH)) ||
            ((new_value >= int(DurationType::V_1024TH)) && (new_dots >= 1)) ||
            ((new_value >= int(DurationType::V_512TH)) && (new_dots >= 2)) ||
            ((new_value >= int(DurationType::V_256TH)) && (new_dots >= 3)) ||
            ((new_value >= int(DurationType::V_128TH)) && (new_dots >= 4))) {
            set_type(DurationType::V_INVALID);
        } else {
            set_type(DurationType(new_value));
            set_dots(new_dots);
        }
    }
}

//---------------------------------------------------------
//   operator<
//---------------------------------------------------------

bool TDuration::operator<(const TDuration &t) const {
    if (t.val_ < val_) {
        return true;
    }
    if (t.val_ == val_) {
        if (dots_ < t.dots_) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   operator>=
//---------------------------------------------------------

bool TDuration::operator>=(const TDuration &t) const {
    if (t.val_ > val_) {
        return true;
    }
    if (t.val_ == val_) {
        if (dots_ >= t.dots_) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   operator<=
//---------------------------------------------------------

bool TDuration::operator<=(const TDuration &t) const {
    if (t.val_ < val_) {
        return true;
    }
    if (t.val_ == val_) {
        if (dots_ <= t.dots_) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   operator>
//---------------------------------------------------------

bool TDuration::operator>(const TDuration &t) const {
    if (t.val_ > val_) {
        return true;
    }
    if (t.val_ == val_) {
        if (dots_ > t.dots_) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   fraction
//---------------------------------------------------------

Fraction TDuration::fraction() const {
    int z = 1;
    unsigned n;
    switch (val_) {
        case DurationType::V_1024TH: n = 1024; break;
        case DurationType::V_512TH: n = 512; break;
        case DurationType::V_256TH: n = 256; break;
        case DurationType::V_128TH: n = 128; break;
        case DurationType::V_64TH: n = 64; break;
        case DurationType::V_32ND: n = 32; break;
        case DurationType::V_16TH: n = 16; break;
        case DurationType::V_EIGHTH: n = 8; break;
        case DurationType::V_QUARTER: n = 4; break;
        case DurationType::V_HALF: n = 2; break;
        case DurationType::V_WHOLE: n = 1; break;
        case DurationType::V_BREVE:
            z = 2;
            n = 1;
            break;
        case DurationType::V_LONG:
            z = 4;
            n = 1;
            break;
        case DurationType::V_ZERO:
            z = 0;
            n = 1;
            break;
        default:
            z = 0;
            n = 0;
            break; // zero+invalid fraction
    }

    //dots multiplier is (2^(n + 1) - 1)/(2^n) where n is the number of dots
    int dot_n = (1 << (static_cast<char>(dots_) + 1)) - 1;
    int dot_d = 1 << static_cast<char>(dots_);

    return Fraction(z * dot_n, n * dot_d);
}

// Longest TDuration that fits into Fraction. Must fit exactly if truncate = false.
TDuration::TDuration(const Fraction &l, bool truncate, int max_dots, DurationType max_type) {
#ifdef NDEBUG
    UNUSED(truncate);
#endif
    set_type(max_type); // use maxType to avoid testing all types if you know that l is smaller than a certain DurationType
    set_dots(max_dots);
    truncate_to_fraction(l, max_dots);
    assert(truncate || (fraction() - l).numerator() == 0); // check for exact fit
}

//---------------------------------------------------------
//   truncateToFraction
//---------------------------------------------------------

void TDuration::truncate_to_fraction(const Fraction &l, int max_dots) {
    // try to fit in l by reducing number of duration dots
    if (set_dots_to_fit_fraction(l, dots_)) {
        return;
    }

    // that wasn't enough so now change type too
    for (shift_type(1); is_valid(); shift_type(1)) {
        if (set_dots_to_fit_fraction(l, max_dots)) {
            return; // duration fits in l
        }
    }
}

//---------------------------------------------------------
//   setDotsToFitFraction
//---------------------------------------------------------

bool TDuration::set_dots_to_fit_fraction(const Fraction &l, int max_dots) {
    for (; max_dots >= 0; max_dots--) {
        dots_ = max_dots; // ensures _dots >= 0 if function returns false.
        if ((fraction() - l).numerator() <= 0) {
            return true; // duration fits in l
        }
    }

    return false; // doesn't fit by changing dots alone (type needs to be changed too)
}

//---------------------------------------------------------
//   operator -=
//---------------------------------------------------------

TDuration &TDuration::operator-=(const TDuration &t) {
    Fraction f1 = fraction() - t.fraction();
    TDuration d(f1);
    val_ = d.val_;
    dots_ = d.dots_;
    return *this;
}

//---------------------------------------------------------
//   operator +=
//---------------------------------------------------------

TDuration &TDuration::operator+=(const TDuration &t) {
    Fraction f1 = fraction() + t.fraction();
    TDuration d(f1);
    val_ = d.val_;
    dots_ = d.dots_;
    return *this;
}

//---------------------------------------------------------
//   toDurationList
//    - l is in local time
//    - returned durations are in local time
//---------------------------------------------------------

std::vector<TDuration> to_duration_list(Fraction l, bool use_dots, int max_dots, bool print_rest_remains) {
    std::vector<TDuration> d_list;
    d_list.reserve(8);

    if (!use_dots) {
        max_dots = 0;
    }

    for (TDuration dd(l, true, max_dots); dd.is_valid() && l.numerator() > 0; dd = TDuration(l, true, max_dots, dd.type())) {
        d_list.push_back(dd);
        l -= dd.fraction();
    }

    if (print_rest_remains && l.numerator() != 0) {
        LOGD("toDurationList:: rest remains %d/%d", l.numerator(), l.denominator());
    }

    return d_list;
}

//---------------------------------------------------------
//   toRhythmicDurationList
//    - l and rtickStart are in local (stretched) time
//    - returned durations are in local time
//---------------------------------------------------------

std::vector<TDuration> to_rhythmic_duration_list(const Fraction &l, bool is_rest, Fraction rtick_start, const TimeSigFrac &nominal,
                                                 Measure *msr, int max_dots, const Fraction &time_stretch) {
    IF_ASSERT_FAILED(l > Fraction(0, 1)) { return {}; }

    std::vector<TDuration> d_list;
    d_list.reserve(8);

    if (msr->is_anacrusis()) {
        rtick_start += msr->anacrusis_offset() * time_stretch;
    } else if (is_rest && l == msr->ticks() * time_stretch) {
        TDuration d = TDuration(DurationType::V_MEASURE);
        d_list.push_back(d);
        return d_list;
    }

    if (nominal.is_compound()) {
        split_compound_beats_for_list(&d_list, l, is_rest, rtick_start, nominal, max_dots);
    } else {
        populate_rhythmic_list(&d_list, l, is_rest, rtick_start, nominal, max_dots);
    }

    return d_list;
}

//---------------------------------------------------------
//   populateRhythmicList
//---------------------------------------------------------

void populate_rhythmic_list(std::vector<TDuration> *d_list, const Fraction &l, bool is_rest, const Fraction &rtick_start,
                            const TimeSigFrac &nominal, int max_dots) {
    Fraction rtick_end = rtick_start + l;

    bool need_to_split = false; // do we need to split?
    int rtick_split = 0;        // tick to split on if we need to

    // CHECK AT SUBBEAT LEVEL

    int start_level = nominal.rtick2subbeat_level(rtick_start.ticks());
    int end_level = nominal.rtick2subbeat_level(rtick_end.ticks());
    int strongest_level_crossed =
        nominal.strongest_subbeat_level_in_range(rtick_start.ticks(), rtick_end.ticks(), &rtick_split); // sets rtickSplit

    if ((start_level < 0) || (end_level < 0) || (strongest_level_crossed < 0)) {
        // Beyond maximum subbeat level so just split into largest possible durations.
        std::vector<TDuration> d_list2 = to_duration_list(l, max_dots > 0, max_dots, false);
        d_list->insert(d_list->end(), d_list2.begin(), d_list2.end());
        return;
    }

    // split if we cross something stronger than where we start and end
    if ((strongest_level_crossed < start_level) && (strongest_level_crossed < end_level)) {
        need_to_split = true;
    }
    // but don't split for level 1 syncopation (allow eight-note, quarter, quarter... to cross unstressed beats)
    if (start_level == end_level && strongest_level_crossed == start_level - 1) {
        need_to_split = false;
    }
    // nor for the next simplest case of level 2 syncopation (allow sixteenth-note, eighth, eighth... to cross unstressed beats)
    if (start_level == end_level && strongest_level_crossed == start_level - 2) {
        // but disallow sixteenth-note, quarter, quarter...
        int ticks_to_next = nominal.ticks_to_next_subbeat(rtick_start.ticks(), start_level - 1);
        int ticks_past_prev = nominal.ticks_past_subbeat(rtick_start.ticks(), start_level - 1);
        need_to_split = ticks_to_next != ticks_past_prev;
    }

    if (!need_to_split && strongest_level_crossed == 0) {
        // NOW CHECK AT DENOMINATOR UNIT LEVEL AND BEAT LEVEL
        BeatType start_beat = nominal.rtick2beat_type(rtick_start.ticks());
        BeatType end_beat = nominal.rtick2beat_type(rtick_end.ticks());

        int d_units_crossed = 0; // number of timeSig denominator units the note/rest crosses
        // if there is a choice of which beat to split on, should we use the first or last?
        bool use_last = start_beat <= BeatType::SIMPLE_UNSTRESSED; // split on the later beat if starting on a beat

        BeatType strongest_beat_crossed =
            nominal.strongest_beat_in_range(rtick_start.ticks(), rtick_end.ticks(), &d_units_crossed, &rtick_split, use_last);

        need_to_split = force_rhythmic_split(is_rest, start_beat, end_beat, d_units_crossed, strongest_beat_crossed, nominal);
    }

    if (!need_to_split) {
        // CHECK THERE IS A DURATION THAT FITS
        // crossed beats/subbeats were not important so try to avoid splitting
        TDuration d = TDuration(l, true, max_dots);
        if (d.fraction() == l) {
            // we can use a single duration - no need to split!
            d_list->push_back(l);
            return;
        }
        // no single TDuration fits so must split anyway
    }

    // Prevent infinite recursion if there is no splitting point other than the start and end ticks
    IF_ASSERT_FAILED(rtick_start.ticks() < rtick_split && rtick_split < rtick_end.ticks()) {
        std::vector<TDuration> d_list2 = to_duration_list(l, max_dots > 0, max_dots, false);
        d_list->insert(d_list->end(), d_list2.begin(), d_list2.end());
        return;
    }

    // Split on the strongest beat or subbeat crossed
    Fraction left_split = Fraction::from_ticks(rtick_split) - rtick_start;
    Fraction right_split = l - left_split;

    // Recurse to see if we need to split further before adding to list
    populate_rhythmic_list(d_list, left_split, is_rest, rtick_start, nominal, max_dots);
    populate_rhythmic_list(d_list, right_split, is_rest, Fraction::from_ticks(rtick_split), nominal, max_dots);
}

//---------------------------------------------------------
//   splitCompoundBeatsForList
//    Split compound notes/rests where they enter a compound beat.
//---------------------------------------------------------

void split_compound_beats_for_list(std::vector<TDuration> *d_list, const Fraction &l, bool is_rest, const Fraction &rtick_start,
                                   const TimeSigFrac &nominal, int max_dots) {
    Fraction rtick_end = rtick_start + l;

    BeatType start_beat = nominal.rtick2beat_type(rtick_start.ticks());
    BeatType end_beat = nominal.rtick2beat_type(rtick_end.ticks());

    if (start_beat > BeatType::COMPOUND_UNSTRESSED) {
        // Not starting on a compound beat so mustn't extend into next compound beat
        int split_ticks = nominal.ticks_to_next_beat(rtick_start.ticks());

        if ((rtick_end - rtick_start).ticks() > split_ticks) {
            // Duration extends into next beat so must split
            Fraction left_split = Fraction::from_ticks(split_ticks);
            Fraction right_split = l - left_split;
            populate_rhythmic_list(d_list, left_split, is_rest, rtick_start, nominal, max_dots); // this side is ok to proceed
            split_compound_beats_for_list(d_list, right_split, is_rest, rtick_start + Fraction::from_ticks(split_ticks), nominal,
                                          max_dots); // not checked yet
            return;
        }
    }

    if (end_beat > BeatType::COMPOUND_UNSTRESSED) {
        // Not ending on a compound beat so mustn't extend into previous compound beat
        int split_ticks = nominal.ticks_past_beat(rtick_end.ticks());

        if ((rtick_end - rtick_start).ticks() > split_ticks) {
            // Duration extends into previous beat so must split
            Fraction right_split = Fraction::from_ticks(split_ticks);
            Fraction left_split = l - right_split;
            populate_rhythmic_list(d_list, left_split, is_rest, rtick_start, nominal, max_dots); // must add leftSplit to list first
            populate_rhythmic_list(d_list, right_split, is_rest, rtick_end - Fraction::from_ticks(split_ticks), nominal, max_dots);
            return;
        }
    }

    // Duration either starts and ends on compound beats, or it remains within a single compound beat
    populate_rhythmic_list(d_list, l, is_rest, rtick_start, nominal, max_dots);
}

//---------------------------------------------------------
//   forceRhythmicSplit
//    Where to split notes and force a tie to indicate rhythm.
//
//    The function assumes the following:
//      * Note/rest has already been split at measure boundaries.
//      * Full measure rest was used if possible
//      * Note/rest already split where it enters a compound beat.
//
//    Usage: Set crossedBeat to the strongest BeatType crossed by
//    the note. Split note if function returns true. Repeat with
//    the two new notes, and so on, until function returns false.
//
//    Note: If no single TDuration can fill the gap then the note
//    *has* to be split, regardless of what this function returns.
//    Non-rhythmic splits should still occur on the strongest beat.
//
//    Implementation: When comparing BeatTypes use <, <=, >, >=
//    instead of == and != as appropriate (see sig.h). E.g. to match
//    all full beats: "<= SIMPLE_UNSTRESSED". This will match for
//    all compound and simple full beats, and not any subbeats.
//---------------------------------------------------------

bool force_rhythmic_split(bool is_rest, BeatType start_beat, BeatType end_beat, int d_units_crossed, BeatType strongest_beat_crossed,
                          const TimeSigFrac &nominal) {
    // Assumption: Notes were split at measure boundary before this function was
    // called. (Necessary because timeSig might be different in next measure.)
    assert(strongest_beat_crossed != BeatType::DOWNBEAT);
    // Assumption: compound notes have already been split where they enter a compound beat.
    // (Necessary because the split beat is not always the strongest beat in this case.)
    assert(!nominal.is_compound() || strongest_beat_crossed >= BeatType::COMPOUND_SUBBEAT ||
           (start_beat <= BeatType::COMPOUND_UNSTRESSED && end_beat <= BeatType::COMPOUND_UNSTRESSED));

    // SPECIAL CASES

    // nothing can cross a stressed beat in an irregular time signature
    if (strongest_beat_crossed <= BeatType::SIMPLE_STRESSED && !nominal.is_triple() && !nominal.is_duple()) {
        return true;
    }
    if (is_rest) {
        // rests must not cross the middle of a bar with numerator == 2 (e.g. 2/4 bar) even though the beat is unstressed
        if (strongest_beat_crossed <= BeatType::SIMPLE_UNSTRESSED && nominal.numerator() == 2) {
            return true;
        }
        // rests must not cross a beat in a triple meter - simple (e.g. 3/4) or compound (e.g. 9/8)
        if (strongest_beat_crossed <= BeatType::SIMPLE_UNSTRESSED && nominal.is_triple()) {
            return true;
        }
    }

    // GENERAL RULES

    if (nominal.is_compound()) {
        return force_rhythmic_split_compound(is_rest, start_beat, end_beat, d_units_crossed, strongest_beat_crossed);
    } else {
        return force_rhythmic_split_simple(is_rest, start_beat, end_beat, d_units_crossed, strongest_beat_crossed);
    }
}

//---------------------------------------------------------
//   forceRhythmicSplitCompound
//---------------------------------------------------------

bool force_rhythmic_split_compound(bool is_rest, BeatType start_beat, BeatType end_beat, int d_units_crossed,
                                   BeatType strongest_beat_crossed) {
    switch (strongest_beat_crossed) {
        case BeatType::COMPOUND_STRESSED:
            // Assumption: compound notes have already been split where they enter a compound beat.
            assert(start_beat <= BeatType::COMPOUND_UNSTRESSED && end_beat <= BeatType::COMPOUND_UNSTRESSED);
            // Notes are guaranteed to and start on a compound beat so we can pretend we have a simple measure.
            return force_rhythmic_split_simple(is_rest, start_beat, end_beat, d_units_crossed / 3, BeatType::SIMPLE_STRESSED);
        case BeatType::COMPOUND_UNSTRESSED:
            // Same assumption as before
            assert(start_beat <= BeatType::COMPOUND_UNSTRESSED && end_beat <= BeatType::COMPOUND_UNSTRESSED);
            // No further conditions since note are guaranteed to start and end on a compound beat.
            return false;
        case BeatType::COMPOUND_SUBBEAT:
            // don't split anything that takes up a full compound beat
            if (start_beat <= BeatType::COMPOUND_UNSTRESSED && end_beat <= BeatType::COMPOUND_UNSTRESSED) {
                return false;
            }
            // split rests that don't start on a compound beat
            if (is_rest && start_beat > BeatType::COMPOUND_UNSTRESSED) {
                return true;
            }
            // Remaining groupings within compound triplets are the same as for simple triple (3/4, 3/8, etc.)
            return force_rhythmic_split_simple(is_rest, start_beat, end_beat, d_units_crossed, BeatType::SIMPLE_UNSTRESSED);
        default: // BeatType::SUBBEAT
            return force_rhythmic_split_simple(is_rest, start_beat, end_beat, d_units_crossed, strongest_beat_crossed);
    }
}

//---------------------------------------------------------
//   forceRhythmicSplitSimple
//    Implementation: This function is also called for compound
//    measures so be careful when comparing BeatTypes. Use <, <=,
//    >, >= instead of == and != when appropriate. (See sig.h)
//---------------------------------------------------------

bool force_rhythmic_split_simple(bool is_rest, BeatType start_beat, BeatType end_beat, int beats_crossed, BeatType strongest_beat_crossed) {
    switch (strongest_beat_crossed) {
        case BeatType::SIMPLE_STRESSED:
            // Must split rests on a stressed beat.
            if (is_rest) {
                return true;
            }
            // Don't split notes that start or end on a stressed beat. Enables double-dotting in 4/4.
            // (Don't remove this to disable double-dotting, instead set maxDots = 1 elsewhere.)
            if (start_beat <= BeatType::SIMPLE_STRESSED || end_beat <= BeatType::SIMPLE_STRESSED) {
                return false;
            }
            // Don't split notes that both start and end on unstressed beats or stronger.
            if (start_beat <= BeatType::SIMPLE_UNSTRESSED && end_beat <= BeatType::SIMPLE_UNSTRESSED) {
                return false;
            }
            // anything else must split on stressed beat.
            return true;
        case BeatType::SIMPLE_UNSTRESSED:
            // Don't split notes or rests if starting and ending on stressed beat.
            if (start_beat <= BeatType::SIMPLE_STRESSED && end_beat <= BeatType::SIMPLE_STRESSED) {
                return false;
            }
            // Split rests that don't start or end on a beat. Notes may cross 1 unstressed beat.
            if (start_beat == BeatType::SUBBEAT || end_beat == BeatType::SUBBEAT) {
                return is_rest || (beats_crossed > 1);
            }
            return false;
        default: // BeatType::SUBBEAT
            return false;
    }
}

//---------------------------------------------------------
//   setType
//---------------------------------------------------------

void TDuration::set_type(DurationType t) {
    val_ = t;
    if (val_ == DurationType::V_MEASURE) {
        dots_ = 0;
    }
}

//---------------------------------------------------------
//   isValid
//---------------------------------------------------------

bool TDuration::is_valid(Fraction f) {
    TDuration t;
    t.set_type(DurationType::V_LONG);
    t.set_dots(4);
    t.truncate_to_fraction(f, 4);
    return t.is_valid() && (t.fraction() - f).numerator() == 0;
}
} // namespace mu::engraving
