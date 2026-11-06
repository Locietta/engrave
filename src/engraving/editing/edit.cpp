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

#include <map>
#include <set>

#include "../dom/accidental.h"
#include "../dom/arpeggio.h"
#include "../dom/articulation.h"
#include "../dom/barline.h"
#include "../dom/beam.h"
#include "../dom/box.h"
#include "../dom/bracket.h"
#include "../dom/breath.h"
#include "../dom/chord.h"
#include "../dom/chordbracket.h"
#include "../dom/chordline.h"
#include "../dom/clef.h"
#include "../dom/dynamic.h"
#include "../dom/excerpt.h"
#include "../dom/expression.h"
#include "../dom/factory.h"
#include "../dom/figuredbass.h"
#include "../dom/fingering.h"
#include "../dom/fret.h"
#include "../dom/glissando.h"
#include "../dom/guitarbend.h"
#include "../dom/hammeronpulloff.h"
#include "../dom/harmony.h"
#include "../dom/harppedaldiagram.h"
#include "../dom/hook.h"
#include "../dom/instrchange.h"
#include "../dom/instrumentname.h"
#include "../dom/key.h"
#include "../dom/keylist.h"
#include "../dom/keysig.h"
#include "../dom/laissezvib.h"
#include "../dom/layoutbreak.h"
#include "../dom/linkedobjects.h"
#include "../dom/lyrics.h"
#include "../dom/marker.h"
#include "../dom/masterscore.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/note.h"
#include "../dom/noteline.h"
#include "../dom/ottava.h"
#include "../dom/pagelockindicator.h"
#include "../dom/part.h"
#include "../dom/partialtie.h"
#include "../dom/playcounttext.h"
#include "../dom/range.h"
#include "../dom/rehearsalmark.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/sig.h"
#include "../dom/slur.h"
#include "../dom/staff.h"
#include "../dom/stafftext.h"
#include "../dom/stem.h"
#include "../dom/sticking.h"
#include "../dom/stringtunings.h"
#include "../dom/system.h"
#include "../dom/systemlockindicator.h"
#include "../dom/systemtext.h"
#include "../dom/tapping.h"
#include "../dom/tempotext.h"
#include "../dom/text.h"
#include "../dom/textline.h"
#include "../dom/tie.h"
#include "../dom/tiemap.h"
#include "../dom/timesig.h"
#include "../dom/tremolosinglechord.h"
#include "../dom/tremolotwochord.h"
#include "../dom/tuplet.h"
#include "../dom/tupletmap.h"
#include "../dom/utils.h"
#include "../dom/volta.h"
#include "../infrastructure/messagebox.h"
#include "addremoveelement.h"
#include "editchord.h"
#include "editclef.h"
#include "editkeysig.h"
#include "editmeasures.h"
#include "editnote.h"
#include "editpagelocks.h"
#include "editpart.h"
#include "editproperty.h"
#include "editrehearsalmark.h"
#include "editscoreproperties.h"
#include "editspanner.h"
#include "editstaff.h"
#include "editstaffbrackets.h"
#include "editstyle.h"
#include "editsystemlocks.h"
#include "edittimesig.h"
#include "edittremolo.h"
#include "inserttime.h"
#include "log.h"
#include "navigation.h"
#include "splitjoinmeasure.h"
#include "transaction/transaction.h"
#include "transaction/undostack.h"
#include "transpose.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static ChordRest *chord_or_rest(EngravingItem *el) {
    if (el) {
        if (el->is_note()) {
            return to_note(el)->chord();
        } else if (el->is_rest_family()) {
            return to_rest(el);
        } else if (el->is_chord()) {
            return to_chord(el);
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   getSelectedNote
//---------------------------------------------------------

Note *Score::get_selected_note() {
    EngravingItem *el = selection().element();
    if (el && el->is_note()) {
        return to_note(el);
    }
    MScore::set_error(MsError::NO_NOTE_SELECTED);
    return 0;
}

//---------------------------------------------------------
//   getSelectedChordRest
//---------------------------------------------------------

ChordRest *Score::get_selected_chord_rest() const {
    ChordRest *cr = chord_or_rest(selection().element());
    if (!cr) {
        MScore::set_error(MsError::NO_NOTE_REST_SELECTED);
    }

    return cr;
}

//---------------------------------------------------------
//   getSelectedChordRest2
//---------------------------------------------------------

void Score::get_selected_start_end_chord_rests(ChordRest *&cr1, ChordRest *&cr2) const {
    cr1 = 0;
    cr2 = 0;
    for (EngravingItem *e : selection().elements()) {
        if (e->is_note()) {
            e = to_note(e)->chord();
        }
        if (e->is_chord_rest()) {
            ChordRest *cr = to_chord_rest(e);
            if (cr1 == 0 || (cr1)->tick() > cr->tick()) {
                cr1 = cr;
            }
            if (cr2 == 0 || (cr2)->tick() < cr->tick()) {
                cr2 = cr;
            }
        }
    }
    if (cr1 == 0) {
        MScore::set_error(MsError::NO_NOTE_REST_SELECTED);
    }
    if (cr1 == cr2) {
        cr2 = 0;
    }
}

//---------------------------------------------------------
//   getSelectedChordRests
//---------------------------------------------------------

std::set<ChordRest *> Score::get_selected_chord_rests() const {
    std::set<ChordRest *> set;
    for (EngravingItem *e : selection().elements()) {
        if (e->is_note()) {
            e = to_note(e)->chord();
        }
        if (e->is_chord_rest()) {
            set.insert(to_chord_rest(e));
        }
    }
    return set;
}

//---------------------------------------------------------
//   pos
//---------------------------------------------------------

Fraction Score::pos() {
    EngravingItem *el = selection().element();
    if (selection().active_cr()) {
        el = selection().active_cr();
    }
    if (el) {
        switch (el->type()) {
            case ElementType::NOTE: el = to_note(el)->chord();
            // fall through
            case ElementType::MEASURE_REPEAT:
            case ElementType::REST:
            case ElementType::MMREST:
            case ElementType::CHORD: return to_chord_rest(el)->tick();
            default: break;
        }
    }
    return Fraction(0, 1);
}

Tuplet *Score::add_tuplet(ChordRest *destination_chord_rest, Fraction ratio, TupletNumberType number_type, TupletBracketType bracket_type) {
    if (destination_chord_rest->duration_type() < TDuration(DurationType::V_512TH) &&
        destination_chord_rest->duration_type() != TDuration(DurationType::V_MEASURE)) {
        return nullptr;
    }

    Measure *measure = destination_chord_rest->measure();
    if (measure && measure->is_mm_rest()) {
        return nullptr;
    }

    Fraction f(destination_chord_rest->ticks());

    f.reduce(); //measure duration might not be reduced

    Fraction ratio_value;
    ratio_value.set_numerator(ratio.numerator() != -1 ? ratio.numerator() : f.numerator());
    ratio_value.set_denominator(ratio.denominator() != -1 ? ratio.denominator() : f.numerator());

    Fraction fr = f * Fraction(1, ratio_value.denominator());
    if (!TDuration::is_valid(fr)) {
        MessageBox().warning(muse::mtrc("engraving", "Cannot create tuplet with ratio %1 for duration %2")
                                 .arg(ratio_value.to_string(), f.to_string())
                                 .to_std_string(),
                             std::string(), {MessageBox::OK});
        return nullptr;
    }

    return add_tuplet(destination_chord_rest, ratio_value, TDuration(fr), number_type, bracket_type);
}

//---------------------------------------------------------
//   canAddTuplet
//    Whether addTuplet() can create a tuplet with this ratio
//    and base length at chordRest
//---------------------------------------------------------

bool Score::can_add_tuplet(const ChordRest *chord_rest, const Fraction &ratio, const TDuration &base_len) const {
    if (!chord_rest || !base_len.is_valid() || base_len.is_measure() || ratio.numerator() <= 0 || ratio.denominator() <= 0) {
        return false;
    }

    const Measure *measure = chord_rest->measure();
    if (!measure || measure->is_mm_rest() || chord_rest->is_grace()) {
        return false;
    }

    // changeCRlen() can't change the length of a measure repeat
    const Fraction ticks = base_len.fraction() * ratio.denominator();
    if (chord_rest->is_measure_repeat() && ticks != chord_rest->ticks()) {
        return false;
    }

    // the tuplet must not cross a barline or the end of a parent tuplet
    Tuplet *parent = chord_rest->tuplet();
    const Fraction time_stretch = chord_rest->staff()->time_stretch(chord_rest->tick());
    const Fraction end = chord_rest->tick() + actual_ticks(ticks, parent, time_stretch);
    const Fraction limit = parent ? parent->tick() + parent->actual_ticks() : measure->end_tick();

    return end <= limit;
}

//---------------------------------------------------------
//   addTuplet
//    Create a tuplet of ratio.denominator() * baseLen, starting
//    at destinationChordRest and replacing everything in that
//    time in its track. The tuplet can be longer or shorter than
//    destinationChordRest, but must not cross a barline or the end
//    of a parent tuplet.
//---------------------------------------------------------

Tuplet *Score::add_tuplet(ChordRest *destination_chord_rest, const Fraction &ratio, const TDuration &base_len, TupletNumberType number_type,
                          TupletBracketType bracket_type) {
    if (!can_add_tuplet(destination_chord_rest, ratio, base_len)) {
        const Fraction duration = base_len.fraction() * ratio.denominator();
        MessageBox().warning(muse::mtrc("engraving", "Cannot create tuplet with ratio %1 for duration %2")
                                 .arg(ratio.to_string(), duration.to_string())
                                 .to_std_string(),
                             std::string(), {MessageBox::OK});
        return nullptr;
    }

    Measure *measure = destination_chord_rest->measure();
    Tuplet *ot = destination_chord_rest->tuplet();
    const Fraction f = base_len.fraction() * ratio.denominator();

    // Shorten a longer chord/rest first, so that the remainder is filled with rests. Shorten it to
    // the base length, since changeCRlen() only shortens correctly to a single note value
    if (destination_chord_rest->ticks() > f) {
        change_c_rlen(destination_chord_rest, base_len.fraction());
    }

    // Clear the rest of the time the tuplet will take
    if (destination_chord_rest->ticks() < f) {
        make_gap(destination_chord_rest->segment(), destination_chord_rest->track(), f, ot, /*keepChord*/ true);
    }

    Tuplet *tuplet = Factory::create_tuplet(measure);
    tuplet->set_ratio(ratio);

    tuplet->set_number_type(number_type);
    if (tuplet->number_type() == TupletNumberType(tuplet->style().style_i(Sid::TUPLET_NUMBER_TYPE))) {
        tuplet->set_property_flags(Pid::NUMBER_TYPE, PropertyFlags::STYLED);
    } else {
        tuplet->set_property_flags(Pid::NUMBER_TYPE, PropertyFlags::UNSTYLED);
    }

    tuplet->set_bracket_type(bracket_type);
    if (tuplet->bracket_type() == TupletBracketType(tuplet->style().style_i(Sid::TUPLET_BRACKET_TYPE))) {
        tuplet->set_property_flags(Pid::BRACKET_TYPE, PropertyFlags::STYLED);
    } else {
        tuplet->set_property_flags(Pid::BRACKET_TYPE, PropertyFlags::UNSTYLED);
    }

    tuplet->set_ticks(f);
    tuplet->set_base_len(base_len);

    tuplet->set_track(destination_chord_rest->track());
    tuplet->set_tick(destination_chord_rest->tick());

    if (ot) {
        tuplet->set_tuplet(ot);
    }

    cmd_create_tuplet(destination_chord_rest, tuplet);

    return tuplet;
}

//---------------------------------------------------------
//   selectNewTuplet
//    Select where input continues in a new tuplet, and use
//    its base length as the input duration
//---------------------------------------------------------

//---------------------------------------------------------
//   addRest
//    create one Rest at tick with duration d
//    create segment if necessary
//---------------------------------------------------------

Rest *Score::add_rest(const Fraction &tick, track_idx_t track, TDuration d, Tuplet *tuplet) {
    Measure *measure = tick2measure(tick);
    Rest *rest = Factory::create_rest(this->dummy(), d);
    if (d.type() == DurationType::V_MEASURE) {
        rest->set_ticks(measure->stretched_len(staff(track2staff(track))));
    } else {
        rest->set_ticks(d.fraction());
    }
    rest->set_track(track);
    rest->set_tuplet(tuplet);
    undo_add_cr(rest, measure, tick);
    return rest;
}

//---------------------------------------------------------
//   addRest
//---------------------------------------------------------

Rest *Score::add_rest(Segment *s, track_idx_t track, TDuration d, Tuplet *tuplet) {
    Rest *rest = Factory::create_rest(s, d);
    if (d.type() == DurationType::V_MEASURE) {
        rest->set_ticks(s->measure()->stretched_len(staff(track / ::mu::engraving::voices)));
    } else {
        rest->set_ticks(d.fraction());
    }
    rest->set_track(track);
    rest->set_tuplet(tuplet);
    undo_add_cr(rest, tick2measure(s->tick()), s->tick());
    return rest;
}

//---------------------------------------------------------
//   addChord
//    Create one Chord at tick with duration d
//    - create segment if necessary.
//    - Use chord "oc" as prototype;
//    - if "genTie" then tie to chord "oc"
//---------------------------------------------------------

Chord *Score::add_chord(const Fraction &tick, TDuration d, Chord *oc, bool gen_tie, Tuplet *tuplet) {
    Measure *measure = tick2measure(tick);
    if (measure->end_tick() <= tick) {
        LOGD("Score::addChord(): end of score?");
        return 0;
    }

    Chord *chord = Factory::create_chord(this->dummy());
    chord->set_tuplet(tuplet);
    chord->set_track(oc->track());
    chord->set_duration_type(d);
    chord->set_ticks(d.fraction());
    chord->set_stem_direction(oc->stem_direction());

    for (Note *n : oc->notes()) {
        Note *nn = Factory::create_note(chord);
        nn->set_pitch(n->pitch());
        nn->set_tpc1(n->tpc1());
        nn->set_tpc2(n->tpc2());
        chord->add(nn);
    }
    undo_add_cr(chord, measure, tick);

    //
    // now as both chords are in place
    // (have segments as parent) we can add ties:
    //
    if (gen_tie) {
        size_t n = oc->notes().size();
        for (size_t i = 0; i < n; ++i) {
            Note *n1 = oc->notes()[i];
            Note *n2 = chord->notes()[i];
            Tie *tie = Factory::create_tie(n1);
            tie->set_start_note(n1);
            tie->set_end_note(n2);
            tie->set_tick(tie->start_note()->tick());
            tie->set_tick2(tie->end_note()->tick());
            tie->set_track(n1->track());
            undo_add_element(tie);
        }
    }

    return chord;
}

//---------------------------------------------------------
//   addClone
//---------------------------------------------------------

ChordRest *Score::add_clone(ChordRest *cr, const Fraction &tick, const TDuration &d) {
    ChordRest *newcr;
    // change a MeasureRepeat into an Rest
    if (cr->is_measure_repeat()) {
        newcr = Factory::copy_rest(*to_rest(cr));
        to_rest(newcr)->hack_to_rest_type();
    } else {
        newcr = to_chord_rest(cr->clone());
    }
    newcr->mutldata()->set_pos_x(0.0);
    newcr->set_duration_type(d);
    newcr->set_ticks(d.is_measure() ? cr->measure()->stretched_len(cr->staff()) : d.fraction());
    newcr->set_tuplet(cr->tuplet());
    newcr->set_selected(false);
    if (newcr->is_chord()) {
        muse::delete_all(to_chord(newcr)->grace_notes());
        to_chord(newcr)->remove_all_grace_notes();
    }

    undo_add_cr(newcr, cr->measure(), tick);
    return newcr;
}

//---------------------------------------------------------
//   setRest
//    sets rests and returns the first one
//    "l" is in local (stretched) time
//---------------------------------------------------------

Rest *Score::set_rest(const Fraction &input_tick, track_idx_t track, const Fraction &input_l, bool use_dots, Tuplet *tuplet,
                      bool use_full_measure_rest) {
    std::vector<Rest *> rests = set_rests(input_tick, track, input_l, use_dots, tuplet, use_full_measure_rest);
    return rests.empty() ? nullptr : rests.front();
}

//---------------------------------------------------------
//   setRests
//    create one or more rests to fill "l"
//    "l" is in local (stretched) time
//---------------------------------------------------------

std::vector<Rest *> Score::set_rests(const Fraction &input_tick, track_idx_t track, const Fraction &input_l, bool use_dots, Tuplet *tuplet,
                                     bool use_full_measure_rest) {
    Fraction l = input_l;
    Fraction tick = input_tick;
    Measure *measure = tick2measure(tick);
    std::vector<Rest *> rests;
    Staff *staff = Score::staff(track / ::mu::engraving::voices);

    while (!l.is_zero()) {
        //
        // divide into measures
        //
        Fraction f;
        if (tuplet) {
            f = tuplet->base_len().fraction() * tuplet->ratio().numerator();
            for (DurationElement *de : tuplet->elements()) {
                if (de->tick() >= tick) {
                    break;
                }
                f -= de->ticks();
            }
            //
            // restrict to tuplet len
            //
            if (f < l) {
                l = f;
            }
        } else {
            if (measure->tick() < tick) {
                f = measure->end_tick() - tick;
            } else {
                f = measure->ticks();
            }
            f *= staff->time_stretch(tick);
        }

        if (f > l) {
            f = l;
        }

        // Don't fill a full measure with rests on a non-zero voice, *unless* it has links in voice zero
        bool full_measure = tick == measure->tick() && f == measure->stretched_len(staff);
        bool empty_non_zero_voice = track2voice(track) != 0 && !measure->has_voice(track);
        if (empty_non_zero_voice && full_measure && !staff->track_has_links_in_voice_zero(track)) {
            l -= f;
            measure = measure->next_measure();
            if (!measure) {
                break;
            }
            tick = measure->tick();
            continue;
        }

        if ((measure->timesig() == measure->ticks()) // not in pickup measure
            && full_measure && !tuplet && use_full_measure_rest) {
            Rest *rest = add_rest(tick, track, TDuration(DurationType::V_MEASURE), tuplet);
            tick += rest->actual_ticks();
            rests.push_back(rest);
        } else {
            //
            // compute list of durations which will fit l
            //
            std::vector<TDuration> d_list;
            if (tuplet || f == Fraction(0, 1)) {
                d_list = to_duration_list(l, use_dots);
                std::reverse(d_list.begin(), d_list.end());
            } else {
                Fraction time_stretch = staff->time_stretch(tick);
                d_list = to_rhythmic_duration_list(f, true, (tick - measure->tick()) * time_stretch, sigmap()->timesig(tick).nominal(),
                                                   measure, use_dots ? 1 : 0, time_stretch);
            }
            if (d_list.empty()) {
                return rests;
            }

            Rest *rest = 0;
            for (const TDuration &d : d_list) {
                rest = add_rest(tick, track, d, tuplet);
                // If we're filling an empty non-zero voice make these gaps
                rest->set_gap(empty_non_zero_voice && !full_measure);
                rests.push_back(rest);
                tick += rest->actual_ticks();
            }
        }
        l -= f;

        measure = measure->next_measure();
        if (!measure) {
            break;
        }
        tick = measure->tick();
    }
    return rests;
}

Slur *Score::add_slur(ChordRest *first_chord_rest, ChordRest *second_chord_rest, const Slur *slur_template) {
    if (!second_chord_rest) {
        ChordRestNavigateOptions options;
        options.disable_over_repeats = true;
        second_chord_rest = Navigation::next_chord_rest(first_chord_rest, options);

        if (!second_chord_rest || !second_chord_rest->is_chord()) {
            if (slur_template && slur_template->is_hammer_on_pull_off() && first_chord_rest->is_chord()) {
                Note *end_note = GuitarBend::create_end_note(to_chord(first_chord_rest)->up_note());
                if (end_note) {
                    second_chord_rest = end_note->chord();
                }
            }
            if (!second_chord_rest) {
                second_chord_rest = first_chord_rest;
            }
        } else if (second_chord_rest->is_chord()) {
            bool first_chord_rest_is_tied_to_second =
                first_chord_rest->is_chord() && to_chord(first_chord_rest)->all_notes_tied_to_next() &&
                to_chord(first_chord_rest)->up_note()->tie_for()->end_note()->parent() == second_chord_rest;

            // Follow chain of tied notes and slur until the last
            if (first_chord_rest_is_tied_to_second || !first_chord_rest->is_grace()) {
                while (to_chord(second_chord_rest)->all_notes_tied_to_next()) {
                    second_chord_rest = to_chord(second_chord_rest)->up_note()->tie_for()->end_note()->chord();
                }
            }

            // If the first chord rest is also tied to this chain, slur to the next non-tied note
            if (first_chord_rest_is_tied_to_second) {
                ChordRest *next_candidate = Navigation::next_chord_rest(second_chord_rest, options);
                if (next_candidate) {
                    second_chord_rest = next_candidate;
                }
            }
        }
    }

    Slur *slur = slur_template ? slur_template->clone() : Factory::create_slur(score()->dummy());
    slur->set_score(first_chord_rest->score());
    slur->set_tick(first_chord_rest->tick());
    slur->set_tick2(second_chord_rest->tick());
    slur->set_track(first_chord_rest->track());
    if (second_chord_rest->staff()->part() == first_chord_rest->staff()->part() &&
        !second_chord_rest->staff()->is_linked(first_chord_rest->staff())) {
        slur->set_track2(second_chord_rest->track());
    } else {
        slur->set_track2(first_chord_rest->track());
    }
    slur->set_start_element(first_chord_rest);
    slur->set_end_element(second_chord_rest);

    first_chord_rest->score()->undo_add_element(slur);
    SlurTieSegment *ss = slur->new_slur_tie_segment();
    ss->set_spanner_segment_type(SpannerSegmentType::SINGLE);
    if (first_chord_rest == second_chord_rest && !(slur->is_outgoing() || slur->is_incoming())) {
        ss->set_slur_offset(Grip::END, PointF(3.0 * first_chord_rest->style().spatium(), 0.0));
    }
    slur->add(ss);

    return slur;
}

/// The segment that holds the annotations of `chordRest`. For a multimeasure rest,
/// that is the corresponding segment of the first underlying measure.
static Segment *annotation_segment(const ChordRest *chord_rest) {
    const Measure *measure = chord_rest->measure();
    if (measure && measure->is_mm_rest()) {
        return measure->mm_rest_first()->find_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
    }

    return chord_rest->segment();
}

TextBase *Score::add_text(TextStyleType type, EngravingItem *destination_element) {
    TextBase *text_box = nullptr;

    switch (type) {
        case TextStyleType::TITLE:
        case TextStyleType::SUBTITLE:
        case TextStyleType::COMPOSER:
        case TextStyleType::LYRICIST:
        case TextStyleType::INSTRUMENT_EXCERPT: {
            MeasureBase *frame = nullptr;

            if (destination_element && destination_element->is_v_box()) {
                frame = to_measure_base(destination_element);
            } else {
                frame = first();
                if (!frame || !frame->is_box()) {
                    frame = insert_box(ElementType::VBOX, frame);
                }
            }

            text_box = Factory::create_text(frame, type);
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::FRAME: {
            if (!destination_element) {
                break;
            }
            text_box = Factory::create_text(destination_element, type);
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::REHEARSAL_MARK: {
            ChordRest *chord_rest = chord_or_rest(destination_element);
            if (!chord_rest) {
                break;
            }
            text_box = Factory::create_rehearsal_mark(annotation_segment(chord_rest));
            text_box->set_track(chord_rest->track());
            RehearsalMark *r = to_rehearsal_mark(text_box);
            text_box->set_xml_text(EditRehearsalMark::create_rehearsal_mark_text(this, r));
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::STAFF: {
            ChordRest *chord_rest = chord_or_rest(destination_element);
            if (!chord_rest) {
                break;
            }
            text_box = Factory::create_staff_text(annotation_segment(chord_rest), TextStyleType::STAFF);
            text_box->set_track(chord_rest->track());
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::SYSTEM: {
            ChordRest *chord_rest = chord_or_rest(destination_element);
            if (!chord_rest) {
                break;
            }
            text_box = Factory::create_system_text(annotation_segment(chord_rest), TextStyleType::SYSTEM);
            text_box->set_track(chord_rest->track());
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::DYNAMICS: {
            ChordRest *chord_rest = chord_or_rest(destination_element);
            if (!chord_rest) {
                break;
            }
            text_box = Factory::create_dynamic(annotation_segment(chord_rest));
            text_box->set_track(chord_rest->track());
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::EXPRESSION: {
            ChordRest *chord_rest = chord_or_rest(destination_element);
            if (!chord_rest) {
                break;
            }
            text_box = Factory::create_expression(annotation_segment(chord_rest));
            text_box->set_track(chord_rest->track());
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::INSTRUMENT_CHANGE: {
            ChordRest *chord_rest = chord_or_rest(destination_element);
            if (!chord_rest) {
                break;
            }
            text_box = Factory::create_instrument_change(annotation_segment(chord_rest));
            text_box->set_track(chord_rest->track());
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::STICKING: {
            ChordRest *chord_rest = chord_or_rest(destination_element);
            if (!chord_rest) {
                break;
            }
            text_box = Factory::create_sticking(annotation_segment(chord_rest));
            text_box->set_track(chord_rest->track());
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::FINGERING:
        case TextStyleType::LH_GUITAR_FINGERING:
        case TextStyleType::RH_GUITAR_FINGERING:
        case TextStyleType::STRING_NUMBER: {
            if (!destination_element || !destination_element->is_note()) {
                break;
            }

            const Staff *staff = destination_element->staff();
            bool is_tablature = staff->is_tab_staff(destination_element->tick());
            bool tab_fingering = staff->staff_type(destination_element->tick())->show_tab_fingering();
            if (is_tablature && !tab_fingering) {
                break;
            }

            text_box = Factory::create_fingering(to_note(destination_element), type);
            text_box->set_track(destination_element->track());
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::HARMONY_A:
        case TextStyleType::HARMONY_ROMAN:
        case TextStyleType::HARMONY_NASHVILLE: {
            track_idx_t track = muse::nidx;
            EngravingItem *new_parent = nullptr;
            if (destination_element && destination_element->is_fret_diagram()) {
                FretDiagram *fret_diagram = to_fret_diagram(destination_element);
                track = fret_diagram->track();
                new_parent = fret_diagram->harmony() ? to_engraving_item(fret_diagram->segment()) : to_engraving_item(fret_diagram);
            } else {
                ChordRest *chord_rest = chord_or_rest(destination_element);
                if (chord_rest) {
                    track = chord_rest->track();
                    new_parent = chord_rest->segment();
                }
            }

            if (track == muse::nidx || !new_parent) {
                break;
            }

            Harmony *harmony = Factory::create_harmony(new_parent);
            harmony->set_track(track);

            static const std::map<TextStyleType, HarmonyType> harmony_types = {{TextStyleType::HARMONY_A, HarmonyType::STANDARD},
                                                                               {TextStyleType::HARMONY_ROMAN, HarmonyType::ROMAN},
                                                                               {TextStyleType::HARMONY_NASHVILLE, HarmonyType::NASHVILLE}};
            harmony->set_harmony_type(harmony_types.at(type));

            text_box = harmony;
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::LYRICS_ODD: {
            if (!destination_element ||
                (!destination_element->is_note() && !destination_element->is_lyrics() && !destination_element->is_rest())) {
                break;
            }
            ChordRest *chord_rest = nullptr;
            if (destination_element->is_note()) {
                chord_rest = to_note(destination_element)->chord();
                if (chord_rest->is_grace()) {
                    chord_rest = to_chord_rest(chord_rest->ownership_parent());
                }
            } else if (destination_element->is_lyrics()) {
                chord_rest = to_lyrics(destination_element)->chord_rest();
            } else if (destination_element->is_rest()) {
                chord_rest = to_chord_rest(destination_element);
            } else {
                break;
            }

            int no = static_cast<int>(chord_rest->lyrics().size());
            const auto &spanners = spanner_map().find_overlapping(chord_rest->tick().ticks(), chord_rest->end_tick().ticks());
            for (auto &spanner : spanners) {
                if (!spanner.value->is_partial_lyrics_line() || spanner.start != chord_rest->tick().ticks()) {
                    continue;
                }
                const PartialLyricsLine *line = to_partial_lyrics_line(spanner.value);

                no = std::max(no, line->verse() + 1);
            }

            // Also check how many partial lines there are
            Lyrics *lyrics = Factory::create_lyrics(chord_rest);
            lyrics->set_track(chord_rest->track());
            lyrics->set_property(Pid::VERSE, no);

            text_box = lyrics;
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::TEMPO: {
            ChordRest *chord_rest = chord_or_rest(destination_element);
            if (!chord_rest) {
                break;
            }

            SigEvent event = sigmap()->timesig(chord_rest->tick());
            Fraction f = event.nominal();
            String text(u"<sym>metNoteQuarterUp</sym> = 80");
            switch (f.denominator()) {
                case 1: text = u"<sym>metNoteWhole</sym> = 80"; break;
                case 2: text = u"<sym>metNoteHalfUp</sym> = 80"; break;
                case 4: text = u"<sym>metNoteQuarterUp</sym> = 80"; break;
                case 8:
                    if (f.numerator() % 3 == 0) {
                        text = u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80";
                    } else {
                        text = u"<sym>metNote8thUp</sym> = 80";
                    }
                    break;
                case 16:
                    if (f.numerator() % 3 == 0) {
                        text = u"<sym>metNote8thUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80";
                    } else {
                        text = u"<sym>metNote16thUp</sym> = 80";
                    }
                    break;
                case 32:
                    if (f.numerator() % 3 == 0) {
                        text = u"<sym>metNote16thUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80";
                    } else {
                        text = u"<sym>metNote32ndUp</sym> = 80";
                    }
                    break;
                case 64:
                    if (f.numerator() % 3 == 0) {
                        text = u"<sym>metNote32ndUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80";
                    } else {
                        text = u"<sym>metNote64thUp</sym> = 80";
                    }
                    break;
                case 128:
                    if (f.numerator() % 3 == 0) {
                        text = u"<sym>metNote64ndUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80";
                    } else {
                        text = u"<sym>metNote128thUp</sym> = 80";
                    }
                    break;
                default: break;
            }

            TempoText *tempo_text = Factory::create_tempo_text(chord_rest->segment());
            tempo_text->set_track(0);
            tempo_text->set_xml_text(text);
            tempo_text->set_follow_text(true);
            tempo_text->update_tempo();

            text_box = tempo_text;
            undo_add_element(text_box);
            break;
        }
        case TextStyleType::HARP_PEDAL_DIAGRAM:
        case TextStyleType::HARP_PEDAL_TEXT_DIAGRAM: {
            ChordRest *chord_rest = get_selected_chord_rest();
            if (!chord_rest) {
                break;
            }
            text_box = Factory::create_harp_pedal_diagram(annotation_segment(chord_rest));
            text_box->set_track(chord_rest->track());
            undo_add_element(text_box);
            break;
        }
        default: break;
    }

    return text_box;
}

//---------------------------------------------------------
//   searchNote
//    search for note or rest before or at tick position tick
//    in staff
//---------------------------------------------------------

ChordRest *Score::search_note(const Fraction &tick, track_idx_t track) const {
    ChordRest *ipe = 0;
    SegmentType st = SegmentType::CHORD_REST;
    for (Segment *segment = first_segment(st); segment; segment = segment->next1(st)) {
        ChordRest *cr = segment->cr(track);
        if (!cr) {
            continue;
        }
        if (cr->tick() == tick) {
            return cr;
        }
        if (cr->tick() > tick) {
            return ipe ? ipe : cr;
        }
        ipe = cr;
    }
    return 0;
}

//---------------------------------------------------------
//   cmdAddOttava
//---------------------------------------------------------

void Score::cmd_add_ottava(OttavaType type) {
    const Selection sel = selection(); // copy selection state before the operation.
    // add on each staff if possible
    if (sel.is_range() && sel.staff_start() != sel.staff_end() - 1) {
        for (staff_idx_t staff_idx = sel.staff_start(); staff_idx < sel.staff_end(); ++staff_idx) {
            ChordRest *cr1 = sel.first_chord_rest(staff_idx * ::mu::engraving::voices);
            ChordRest *cr2 = sel.last_chord_rest(staff_idx * ::mu::engraving::voices);
            if (!cr1) {
                continue;
            }
            if (!cr2) {
                cr2 = cr1;
            }
            Ottava *ottava = Factory::create_ottava(this->dummy());
            ottava->set_ottava_type(type);
            if (type == OttavaType::OTTAVA_8VB /*|| type == OttavaType::OTTAVA_15MB || type == OttavaType::OTTAVA_22MB*/) {
                ottava->set_placement(PlacementV::BELOW);
                ottava->style_changed();
            }
            ottava->set_track(cr1->track());
            ottava->set_track2(cr1->track());
            ottava->set_tick(cr1->tick());
            ottava->set_tick2(cr2->end_tick());
            undo_add_element(ottava);
        }
    } else {
        ChordRest *cr1;
        ChordRest *cr2;
        get_selected_start_end_chord_rests(cr1, cr2);
        if (!cr1) {
            return;
        }
        if (!cr2) {
            cr2 = cr1;
        }

        Ottava *ottava = Factory::create_ottava(this->dummy());
        ottava->set_ottava_type(type);
        if (type == OttavaType::OTTAVA_8VB /*|| type == OttavaType::OTTAVA_15MB || type == OttavaType::OTTAVA_22MB*/) {
            ottava->set_placement(PlacementV::BELOW);
            ottava->style_changed();
        }
        ottava->set_track(cr1->track());
        ottava->set_track2(cr1->track());
        ottava->set_tick(cr1->tick());
        ottava->set_tick2(cr2->end_tick());
        undo_add_element(ottava);
        if (!note_entry_mode()) {
            select(ottava, SelectType::SINGLE, 0);
        }
    }
}

//---------------------------------------------------------
//   cmdAddNoteLine
//---------------------------------------------------------

void Score::add_note_line() {
    std::vector<Note *> selected_notes;

    if (selection().is_range()) {
        track_idx_t start_track = selection().staff_start() * ::mu::engraving::voices;
        track_idx_t end_track = selection().staff_end() * ::mu::engraving::voices;

        for (track_idx_t track = start_track; track < end_track; ++track) {
            std::vector<Note *> notes = selection().note_list(track);
            selected_notes.insert(selected_notes.end(), notes.begin(), notes.end());
        }
    } else {
        std::vector<Note *> notes = selection().note_list();
        selected_notes.insert(selected_notes.end(), notes.begin(), notes.end());
    }

    Note *start_note = nullptr;
    Note *end_note = nullptr;

    for (Note *note : selected_notes) {
        if (start_note == nullptr || start_note->chord()->tick() > note->chord()->tick()) {
            start_note = note;
        }
        if (end_note == nullptr || end_note->chord()->tick() < note->chord()->tick()) {
            end_note = note;
        }
    }

    if (!start_note) {
        LOGD("addNoteLine: no first note %p", start_note);
        return;
    }

    if (start_note == end_note) {
        end_note = SLine::guess_final_note(start_note);
    }

    if (!end_note) {
        LOGD("addNoteLine: no last note note %p", end_note);
        return;
    }

    NoteLine *line = Factory::create_note_line(start_note);
    line->set_start_element(start_note);
    line->set_tick(start_note->chord()->tick());
    line->set_end_element(end_note);

    undo_add_element(line);
}

//---------------------------------------------------------
//   deleteItem
//---------------------------------------------------------

void Score::delete_item(EngravingItem *el) {
    if (!el) {
        return;
    }

    if (el->generated()) {
        switch (el->type()) {
            // These types can be removed, even if generated
            case ElementType::BAR_LINE:
            case ElementType::BRACKET:
            case ElementType::CLEF:
            case ElementType::INSTRUMENT_NAME:
            case ElementType::KEYSIG:
            case ElementType::MEASURE_NUMBER:
            case ElementType::SYSTEM_LOCK_INDICATOR:
            case ElementType::PAGE_LOCK_INDICATOR:
            case ElementType::HAMMER_ON_PULL_OFF_TEXT:
            case ElementType::PLAY_COUNT_TEXT:
            case ElementType::LYRICSLINE_SEGMENT: break;
            // All other types cannot be removed if generated
            default: return;
        }
    }
    //      LOGD("%s", el->typeName());

    Transaction &tx = transaction_manager()->current_or_dummy_transaction();

    switch (el->type()) {
        case ElementType::INSTRUMENT_NAME: {
            Part *part = el->part();
            InstrumentName *in = to_instrument_name(el);
            if (in->instrument_name_type() == InstrumentNameType::LONG) {
                tx.push(new ChangeInstrumentLong(Fraction(0, 1), part, String()));
            } else if (in->instrument_name_type() == InstrumentNameType::SHORT) {
                tx.push(new ChangeInstrumentShort(Fraction(0, 1), part, String()));
            }
        } break;

        case ElementType::TIMESIG: {
            // timesig might already be removed
            TimeSig *ts = to_time_sig(el);
            Segment *s = ts->segment();
            Measure *m = s->measure();
            Segment *ns = m->find_segment(s->segment_type(), s->tick());
            if (!ns || (ns->element(ts->track()) != ts)) {
                LOGD("deleteItem: not found");
                break;
            }
            EditTimeSig::remove_time_sig(tx, this, ts);
        } break;

        case ElementType::KEYSIG: {
            KeySig *k = to_key_sig(el);
            Segment *next_cr_seg = k->segment()->next1(SegmentType::CHORD_REST);
            const TrackRange track_range = el->part()->track_range();
            bool ic = next_cr_seg &&
                      next_cr_seg->find_annotation(ElementType::INSTRUMENT_CHANGE, track_range.start_track, track_range.end_track - 1);
            undo_remove_element(k);
            if (ic) {
                KeySigEvent ke = k->key_sig_event();
                ke.set_for_instrument_change(true);
                Key c_key = k->staff()->key_sig_event(k->tick()).concert_key();
                Key t_key = c_key;
                if (!style().style_b(Sid::CONCERT_PITCH)) {
                    Interval v = k->part()->instrument(k->tick())->transpose();
                    v.flip();
                    t_key = Transpose::transpose_key(c_key, v, k->part()->prefer_sharp_flat());
                }
                ke.set_concert_key(c_key);
                ke.set_key(t_key);
                EditKeySig::undo_change_key_sig(tx, this, k->staff(), k->tick(), ke);
            }
            for (size_t i = 0; i < k->part()->nstaves(); i++) {
                for (Staff *staff : k->part()->staff(i)->staff_list()) {
                    KeySigEvent e = staff->key_sig_event(k->tick());
                    staff->score()->update_instrument_change_transpositions(e, staff, k->tick());
                }
            }
        } break;

        case ElementType::NOTE: {
            Chord *chord = to_chord(el->ownership_parent());
            if (chord->notes().size() > 1) {
                undo_remove_element(el);
                break;
            }
            // else fall through
            el = chord;
        }
            // fall through

        case ElementType::CHORD: {
            Chord *chord = to_chord(el);
            remove_chord_rest(chord, false);

            // replace with rest
            if (chord->note_type() == NoteType::NORMAL) {
                Segment *segment = chord->segment();
                Rest *rest = Factory::create_rest(segment, chord->duration_type());
                rest->set_ticks(chord->ticks());
                rest->set_track(el->track());

                undo_add_cr(rest, segment->measure(), segment->tick());

                Tuplet *tuplet = chord->tuplet();
                if (tuplet) {
                    std::list<EngravingObject *> tl = tuplet->link_list();
                    for (EngravingObject *e : rest->link_list()) {
                        DurationElement *de = to_duration_element(e);
                        for (EngravingObject *ee : tl) {
                            Tuplet *t = to_tuplet(ee);
                            if (t->score() == de->score() && t->track() == de->track()) {
                                de->set_tuplet(t);
                                t->add(de);
                                break;
                            }
                        }
                    }
                }
                //select(rest, SelectType::SINGLE, 0);
            } else {
                // remove segment if empty
                Segment *seg = chord->segment();
                if (seg->empty()) {
                    undo_remove_element(seg);
                }
            }
        } break;

        case ElementType::MEASURE_REPEAT: {
            MeasureRepeat *mr = to_measure_repeat(el);
            remove_chord_rest(mr, false);
            Segment *segment = mr->segment();
            Rest *rest = Factory::create_rest(segment);
            rest->set_duration_type(DurationType::V_MEASURE);
            rest->set_ticks(mr->measure()->stretched_len(mr->staff()));
            rest->set_track(mr->track());
            undo_add_cr(rest, segment->measure(), segment->tick());

            // tell measures they're not part of measure repeat group anymore
            Measure *m = mr->first_measure_of_group();
            for (int i = 1; i <= mr->num_measures(); ++i) {
                undo_change_measure_repeat_count(m, 0, mr->staff_idx());
                // don't remove grouping if within measure repeat group on another staff
                bool other_staves_still_need_group = false;
                for (size_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
                    if (m->is_measure_repeat_group_with_next_m(staff_idx) && staff_idx != mr->staff_idx()) {
                        other_staves_still_need_group = true;
                        break;
                    }
                }
                if (!other_staves_still_need_group) {
                    m->undo_set_no_break(false);
                }
                m = m->next_measure();
            }
        }
        // fall through
        case ElementType::MMREST:
        case ElementType::REST:
            //
            // only allow for voices != 0
            //    e.g. voice 0 rests cannot be removed
            //
            {
                Rest *rest = to_rest(el);
                if (rest->tuplet() && rest->tuplet()->elements().empty()) {
                    undo_remove_element(rest->tuplet());
                }
                if ((el->voice() != 0) && !rest->tuplet()) {
                    rest->undo_change_property(Pid::GAP, true);
                    rest->undo_change_property(Pid::BEAM_MODE, BeamMode::AUTO);
                    for (EngravingObject *r : el->link_list()) {
                        Rest *rr = to_rest(r);
                        if (rr->track() % ::mu::engraving::voices) {
                            rr->undo_change_property(Pid::GAP, true);
                        }
                    }

                    // delete them really when only gap rests are in the actual measure.
                    Measure *m = to_rest(el)->measure();
                    track_idx_t track = el->track();
                    if (m->is_only_gap_rests(track) && !el->staff()->track_has_links_in_voice_zero(track)) {
                        static const SegmentType st{SegmentType::CHORD_REST};
                        for (const Segment *s = m->first(st); s; s = s->next(st)) {
                            EngravingItem *del = s->element(track);
                            if (s->segment_type() != st || !del) {
                                continue;
                            }
                            if (to_rest(del)->is_gap()) {
                                undo_remove_element(del);
                            }
                        }
                        check_spanner(m->tick(), m->end_tick());
                    } else {
                        // check if the other rest could be combined
                        Segment *s = to_rest(el)->segment();

                        std::vector<Rest *> rests;
                        // find previous segment with cr in this track
                        EngravingItem *pe = 0;
                        for (Segment *ps = s->prev(SegmentType::CHORD_REST); ps; ps = ps->prev(SegmentType::CHORD_REST)) {
                            EngravingItem *elm = ps->element(track);
                            if (elm && elm->is_rest() && to_rest(elm)->is_gap()) {
                                pe = el;
                                rests.push_back(to_rest(elm));
                            } else if (elm) {
                                break;
                            }
                        }
                        // find next segment with cr in this track
                        Segment *ns;
                        EngravingItem *ne = 0;
                        for (ns = s->next(SegmentType::CHORD_REST); ns; ns = ns->next(SegmentType::CHORD_REST)) {
                            EngravingItem *elm = ns->element(track);
                            if (elm && elm->is_rest() && to_rest(elm)->is_gap()) {
                                ne = elm;
                                rests.push_back(to_rest(elm));
                            } else if (elm) {
                                break;
                            }
                        }

                        Fraction stick = pe ? pe->tick() : s->tick();
                        Fraction ticks = {0, 1};

                        if (ne) {
                            ticks = ne->tick() - stick + to_rest(ne)->actual_ticks();
                        } else if (ns) {
                            ticks = ns->tick() - stick;
                        } else {
                            ticks = m->end_tick() - stick;
                        }

                        if (ticks != m->ticks() && ticks != s->ticks()) {
                            undo_remove_element(rest);
                            for (Rest *r : rests) {
                                undo_remove_element(r);
                            }

                            Fraction f = ticks * el->staff()->time_stretch(el->tick());

                            std::vector<TDuration> d_list = to_duration_list(f, true);
                            if (d_list.empty()) {
                                break;
                            }

                            Fraction cur_tick = stick;
                            for (const TDuration &d : d_list) {
                                Rest *rr = Factory::create_rest(this->dummy());
                                rr->set_ticks(d.fraction());
                                rr->set_duration_type(d);
                                rr->set_track(track);
                                rr->set_gap(true);
                                undo_add_cr(rr, m, cur_tick);
                                cur_tick += rr->actual_ticks();
                            }
                        }
                    }
                }
            }
            break;

        case ElementType::ACCIDENTAL:
            if (el->ownership_parent()->is_note()) {
                EditNote::change_accidental(this, to_note(el->ownership_parent()), AccidentalType::NONE);
            } else {
                undo_remove_element(el);
            }
            break;

        case ElementType::BAR_LINE: {
            BarLine *bl = to_bar_line(el);
            Segment *s = bl->segment();
            Measure *m = s->measure();
            if (s->is_begin_bar_line_type() || s->is_bar_line_type()) {
                undo_remove_element(el);
            } else {
                if (bl->bar_line_type() == BarLineType::START_REPEAT) {
                    Measure *m2 = m->is_mm_rest() ? m->mm_rest_first() : m;
                    for (Score *lscore : score()->score_list()) {
                        Measure *lmeasure = lscore->tick2measure(m2->tick());
                        if (lmeasure) {
                            lmeasure->undo_change_property(Pid::REPEAT_START, false);
                            lmeasure->undo_change_property(Pid::REPEAT_COUNT, 2);
                        }
                    }
                    if (!bl->generated()) {
                        undo_remove_element(el);
                    }
                } else if (bl->bar_line_type() == BarLineType::END_REPEAT) {
                    Measure *m2 = m->is_mm_rest() ? m->mm_rest_last() : m;
                    for (Score *lscore : score()->score_list()) {
                        Measure *lmeasure = lscore->tick2measure(m2->tick());
                        if (lmeasure) {
                            lmeasure->undo_change_property(Pid::REPEAT_END, false);
                            lmeasure->undo_change_property(Pid::REPEAT_COUNT, 2);
                        }
                    }
                    if (!bl->generated()) {
                        undo_remove_element(el);
                    }
                } else {
                    bl->undo_change_property(Pid::BARLINE_TYPE, PropertyValue::from_value(BarLineType::NORMAL));
                }
            }
        } break;

        case ElementType::TUPLET: cmd_delete_tuplet(to_tuplet(el), true); break;

        case ElementType::MEASURE: {
            Measure *m = to_measure(el);
            undo_remove_measures(m, m);
            undo_insert_time(m->tick(), -(m->end_tick() - m->tick()));
        } break;

        case ElementType::BRACKET: EditStaffBrackets::undo_remove_bracket(this, to_bracket(el)); break;
        case ElementType::CLEF: {
            Clef *clef = to_clef(el);
            Measure *m = clef->measure();
            if (clef->generated()) {
                // find the real clef if this is a cautionary one
                if (m && m->prev_measure()) {
                    Fraction tick = m->tick();
                    m = m->prev_measure();
                    Segment *s = m->find_segment(SegmentType::CLEF, tick);
                    if (s && s->element(clef->track())) {
                        clef = to_clef(s->element(clef->track()));
                    }
                }
            }
            undo_remove_element(clef);
        } break;

        case ElementType::MEASURE_NUMBER: {
            Measure *mea = to_measure(el->ownership_parent());
            switch (mea->measure_number_mode()) {
                // If the user tries to remove an automatically generated measure number,
                // we should force the measure not to show any measure number
                case MeasureNumberMode::AUTO:
                    mea->undo_change_property(Pid::MEASURE_NUMBER_MODE, static_cast<int>(MeasureNumberMode::HIDE));
                    break;

                // If the user tries to remove a measure number that he added manually,
                // then we should set the MeasureNumberMode to AUTO only if will not show if set to auto.
                // If after setting the MeasureNumberMode to AUTO, the measure number still shows,
                // We need to force the measure to hide its measure number.
                case MeasureNumberMode::SHOW:
                    if (mea->show_measure_number_in_auto_mode()) {
                        mea->undo_change_property(Pid::MEASURE_NUMBER_MODE, static_cast<int>(MeasureNumberMode::HIDE));
                    } else {
                        mea->undo_change_property(Pid::MEASURE_NUMBER_MODE, static_cast<int>(MeasureNumberMode::AUTO));
                    }
                    break;
                case MeasureNumberMode::HIDE: break;
            }
        } break;
        case ElementType::LYRICSLINE_SEGMENT: {
            el = to_lyrics_line_segment(el)->lyrics_line();
            Lyrics *lyrics = to_lyrics_line(el)->lyrics();
            undo_remove_element(el);
            lyrics->undo_reset_property(Pid::LYRIC_TICKS);
            lyrics->undo_reset_property(Pid::SYLLABIC);

            break;
        }
        case ElementType::OTTAVA_SEGMENT:
        case ElementType::HAIRPIN_SEGMENT:
        case ElementType::TRILL_SEGMENT:
        case ElementType::VIBRATO_SEGMENT:
        case ElementType::TEXTLINE_SEGMENT:
        case ElementType::VOLTA_SEGMENT:
        case ElementType::SLUR_SEGMENT:
        case ElementType::TIE_SEGMENT:
        case ElementType::LAISSEZ_VIB_SEGMENT:
        case ElementType::PARTIAL_TIE_SEGMENT:
        case ElementType::PARTIAL_LYRICSLINE_SEGMENT:
        case ElementType::PEDAL_SEGMENT:
        case ElementType::GLISSANDO_SEGMENT:
        case ElementType::NOTELINE_SEGMENT:
        case ElementType::LET_RING_SEGMENT:
        case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
        case ElementType::PALM_MUTE_SEGMENT:
        case ElementType::WHAMMY_BAR_SEGMENT:
        case ElementType::RASGUEADO_SEGMENT:
        case ElementType::HARMONIC_MARK_SEGMENT:
        case ElementType::PICK_SCRAPE_SEGMENT:
        case ElementType::GUITAR_BEND_SEGMENT:
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT: {
            el = to_spanner_segment(el)->spanner();
            if (el->is_tie()) {
                Tie *tie = to_tie(el);
                if (tie->tie_jump_points()) {
                    tie->undo_remove_ties_from_jump_points();
                }
                if (tie->jump_point()) {
                    tie->update_start_tie_on_removal();
                }
            }
            undo_remove_element(el);

            if (el->is_guitar_bend()) {
                GuitarBend *bend = to_guitar_bend(el);
                if (bend->bend_type() == GuitarBendType::PRE_BEND || bend->bend_type() == GuitarBendType::PRE_DIVE) {
                    delete_item(bend->start_note());
                }
            }
        } break;

        case ElementType::TAPPING_HALF_SLUR_SEGMENT: {
            TappingHalfSlur *half_slur = to_tapping_half_slur(to_tapping_half_slur_segment(el)->spanner());
            Tapping *tapping = to_tapping(half_slur->parent());
            undo_remove_element(tapping);
            break;
        }

        case ElementType::HAMMER_ON_PULL_OFF_TEXT: undo_remove_hopo_text(to_hammer_on_pull_off_text(el)); break;

        case ElementType::STEM_SLASH: // cannot delete this elements
        case ElementType::HOOK:
        case ElementType::GUITAR_BEND_TEXT:
        case ElementType::GUITAR_BEND_HOLD_SEGMENT: LOGD("cannot remove %s", el->type_name()); break;

        case ElementType::TEXT:
            if ((el->ownership_parent() && el->ownership_parent()->is_t_box()) || el->is_t_box()) {
                el->undo_change_property(Pid::TEXT, String());
            } else {
                undo_remove_element(el);
            }
            break;
        case ElementType::PLAY_COUNT_TEXT: {
            PlayCountText *pct = to_play_count_text(el);
            pct->barline()->undo_change_property(Pid::PLAY_COUNT_TEXT_SETTING, AutoCustomHide::HIDE);
        } break;
        case ElementType::INSTRUMENT_CHANGE: {
            InstrumentChange *ic = static_cast<InstrumentChange *>(el);
            Fraction tick_start = ic->segment()->tick();
            Part *part = ic->part();
            Interval old_v = part->staff(0)->transpose(tick_start);
            undo_remove_element(el);
            if (tick_start != Fraction(0, 1)) {
                for (KeySig *key_sig : ic->key_sigs()) {
                    delete_item(key_sig);
                }
            }
            for (Clef *clef : ic->clefs()) {
                delete_item(clef);
            }
            if (part->staff(0)->transpose(tick_start) != old_v) {
                auto i = part->instruments().upper_bound(tick_start.ticks());
                Fraction tick_end;
                if (i == part->instruments().end()) {
                    tick_end = Fraction(-1, 1);
                } else {
                    tick_end = Fraction::from_ticks(i->first);
                }
                Transpose::transposition_changed(tx, this, part, old_v, tick_start, tick_end);
            }
        } break;
        case ElementType::SYSTEM_LOCK_INDICATOR: {
            const RangeLock *system_lock = to_system_lock_indicator(el)->system_lock();
            EditSystemLocks::undo_remove_system_lock(tx, system_lock);
        } break;
        case ElementType::PAGE_LOCK_INDICATOR: {
            const RangeLock *page_lock = to_page_lock_indicator(el)->page_lock();
            EditPageLocks::undo_remove_page_lock(tx, page_lock);
        } break;
        case ElementType::PARENTHESIS: {
            Parenthesis *paren = to_parenthesis(el);
            // Use EditChord::removeChordParentheses when parent is a chord
            if (el->parent() && el->parent()->is_chord()) {
                Chord *chord = to_chord(el->parent());
                NoteParenthesisInfo *paren_info = chord->find_note_parenthesis_info(paren);
                IF_ASSERT_FAILED(paren_info) {
                    LOGD() << "deleteItem: This parenthesis does not belong to this chord";
                    return;
                }
                for (Note *note : paren_info->notes()) {
                    note->undo_change_property(Pid::HIDE_GENERATED_PARENTHESES, true);
                    note->undo_change_property(Pid::HAS_PARENTHESES, ParenthesesMode::NONE);
                }
                EditChord::remove_chord_parentheses(chord, paren_info->notes());
            } else {
                undo_remove_element(el);
            }
        } break;
        default: undo_remove_element(el); break;
    }
}

//---------------------------------------------------------
//   deleteMeasures
//---------------------------------------------------------

void Score::delete_measures(MeasureBase *mb_start, MeasureBase *mb_end, bool preserve_ties) {
    select(0, SelectType::SINGLE, 0);

    if (mb_end->is_measure()) {
        Measure *mb_end_measure = to_measure(mb_end);
        if (mb_end_measure->is_mm_rest()) {
            mb_end = mb_end_measure->mm_rest_last();
        }
    }

    if (!preserve_ties) {
        // we're not preserving slurs and ties where they are, so the endpoints need to be either
        // moved or the slur has to be deleted
        reconnect_slurs(mb_start, mb_end);
    }

    // get the last deleted timesig & keysig in order to restore after deletion
    std::map<staff_idx_t, TimeSig *> last_deleted_time_sigs;

    for (MeasureBase *mb = mb_end;; mb = mb->prev()) {
        if (mb->is_measure()) {
            Measure *m = to_measure(mb);
            Segment *sts = m->find_segment(SegmentType::TIME_SIG, m->tick());

            if (sts) {
                for (staff_idx_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
                    if (!muse::contains(last_deleted_time_sigs, staff_idx)) {
                        if (TimeSig *ts = to_time_sig(sts->element(staff_idx * ::mu::engraving::voices))) {
                            last_deleted_time_sigs.insert({staff_idx, ts});
                        }
                    }
                }
            }

            if (last_deleted_time_sigs.size() == nstaves()) {
                break;
            }
        }
        if (mb == mb_start) {
            break;
        }
    }

    std::vector<KeySigEvent> last_deleted_key_sig_events;

    for (Staff *staff : score()->staves()) {
        KeySigEvent kse = staff->key_sig_event(mb_end->tick());
        last_deleted_key_sig_events.push_back(kse);
    }

    Fraction start_tick = mb_start->tick();
    Fraction end_tick = mb_end->tick();

    for (Score *score : score_list()) {
        Measure *start_measure = score->tick2measure(start_tick);
        Measure *end_measure = score->tick2measure(end_tick);

        score->undo_remove_measures(start_measure, end_measure, preserve_ties);

        // adjust views
        Measure *focus_on = start_measure->prev_measure() ? start_measure->prev_measure() : score->first_measure();

        // insert correct timesig after deletion
        Measure *m_before_sel = start_measure->prev_measure();
        Measure *m_after_sel = m_before_sel ? m_before_sel->next_measure() : score->first_measure();
        if (m_after_sel) {
            Segment *s = m_after_sel->find_segment(SegmentType::TIME_SIG, m_after_sel->tick());

            for (staff_idx_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
                if (s && s->element(staff_idx * ::mu::engraving::voices)) {
                    continue;
                }

                TimeSig *last_deleted_for_this_staff = muse::value(last_deleted_time_sigs, staff_idx, nullptr);
                if (!last_deleted_for_this_staff) {
                    continue;
                }

                if (m_before_sel) {
                    const Staff *staff = this->staff(staff_idx);
                    if (!staff) {
                        continue;
                    }

                    const TimeSig *time_sig = staff->time_sig(m_before_sel->tick());
                    if (!time_sig || time_sig->sig() == last_deleted_for_this_staff->sig()) {
                        continue;
                    }
                }

                if (!s) {
                    s = m_after_sel->undo_get_segment(SegmentType::TIME_SIG, m_after_sel->tick());
                }

                TimeSig *nts = Factory::create_time_sig(s);
                nts->set_track(staff_idx * ::mu::engraving::voices);
                nts->set_from(last_deleted_for_this_staff);
                nts->set_stretch(nts->sig() / m_after_sel->timesig());
                score->undo_add_element(nts);
            }
        }

        // insert correct keysig if necessary
        if (m_after_sel && !m_before_sel) {
            Segment *s = m_after_sel->undo_get_segment(SegmentType::KEY_SIG, m_after_sel->tick());

            bool user_customized_key_sig = false;
            for (EngravingItem *e : s->elist()) {
                if (e && e->is_key_sig() && !to_key_sig(e)->generated()) {
                    user_customized_key_sig = true;
                    break;
                }
            }
            if (user_customized_key_sig) {
                continue;
            }

            bool concert_pitch = score->style().style_b(Sid::CONCERT_PITCH);
            for (Staff *staff : score->staves()) {
                Part *part = staff->part();
                Instrument *instrument = part->instrument(Fraction(0, 1));
                KeySigEvent nkse;
                staff_idx_t staff_idx = staff->idx();

                if (instrument->use_drumset()) {
                    nkse.set_concert_key(Key::C);
                    nkse.set_custom(true);
                    nkse.set_mode(KeyMode::NONE);
                } else {
                    nkse = last_deleted_key_sig_events.at(staff_idx);
                    if (!concert_pitch && !nkse.is_atonal()) {
                        Interval v = instrument->transpose();
                        v.flip();
                        nkse.set_key(Transpose::transpose_key(nkse.concert_key(), v, part->prefer_sharp_flat()));
                    }
                }

                KeySig *nks = (KeySig *) s->element(staff2track(staff_idx));
                if (!nks) {
                    nks = Factory::create_key_sig(s);
                    nks->set_track(staff_idx * ::mu::engraving::voices);
                    nks->set_key_sig_event(nkse);
                    score->undo_add_element(nks);
                } else {
                    nks->set_generated(false);
                    nks->set_key_sig_event(nkse);
                    staff->set_key(m_after_sel->tick(), nkse);
                }
            }
        }
    }

    undo_insert_time(mb_start->tick(), -(mb_end->end_tick() - mb_start->tick()));
    is_.set_segment(0); // invalidate position
}

void Score::reconnect_slurs(MeasureBase *mb_start, MeasureBase *mb_end) {
    // Reconnect or remove slurs that intersect with these deleted measures
    Fraction s_tick = mb_start->tick();
    Fraction e_tick = mb_end->tick();
    for (auto overlapping_slur : spanner_map().find_overlapping(s_tick.ticks(), e_tick.ticks())) {
        Spanner *sp = overlapping_slur.value;
        if (!sp->is_slur()) {
            continue;
        }
        ChordRest *cr1 = sp->start_cr();
        ChordRest *cr2 = sp->end_cr();
        if (!cr1 || !cr2) {
            // this is an invalid slur
            continue;
        }
        MeasureBase *m = mb_start;
        bool adjust1 = false;
        bool adjust2 = false;
        Measure *measure1 = cr1->measure();
        Measure *measure2 = cr2->measure();
        do {
            adjust1 = adjust1 ? true : measure1 == m;
            adjust2 = adjust2 ? true : measure2 == m;
            if (m == mb_end) {
                break;
            } else {
                m = m->next();
            }
        } while (m && !(adjust1 && adjust2));
        if (adjust1 && adjust2) {
            // if both endpoints of this slur are inside this deleted range, remove the slur.
            undo_remove_element(sp);
            continue;
        } else if (adjust1) {
            // endpoint 1 is in deleted range. move endpoint forward to first non-deleted CR after range
            Measure *m_next = mb_end->next() && mb_end->next()->is_measure() ? to_measure(mb_end->next()) : nullptr;
            if (!m_next) {
                undo_remove_element(sp);
                continue;
            }
            ChordRest *first_available_cr = nullptr;
            // search for last cr in this voice
            Segment *seg = m_next->first(SegmentType::CHORD_REST);
            track_idx_t sp_track = sp->track();
            ChordRest *sp_end_cr = sp->end_cr();
            Fraction sp_end_cr_tick = sp_end_cr->tick();
            while (seg) {
                first_available_cr = seg->cr(sp_track);
                if (!first_available_cr || !first_available_cr->is_chord()) {
                    seg = seg->next(SegmentType::CHORD_REST);
                    continue;
                }
                auto graces_before = to_chord(first_available_cr)->grace_notes_before();
                if (first_available_cr->tick() > sp_end_cr_tick) {
                    // we would create a negative-length slur. remove it entirely.
                    undo_remove_element(sp);
                    break;
                } else if (sp_end_cr == first_available_cr) {
                    // this would be a zero-length slur, but a possibility exists that this cr has grace after
                    if (graces_before.empty()) {
                        undo_remove_element(sp);
                        break;
                    }
                }
                if (!graces_before.empty()) {
                    first_available_cr = graces_before.front();
                }
                Spanner *new_sp = to_spanner(sp->clone());
                new_sp->set_start_element(first_available_cr);
                new_sp->set_tick(first_available_cr->tick());
                undo_change_element(sp, new_sp);
                break;
            }
        } else if (adjust2) {
            // endpoint 2 is in deleted range. move endpoint backward to last non-deleted CR before range
            Measure *m_prev = mb_start->prev() && mb_start->prev()->is_measure() ? to_measure(mb_start->prev()) : nullptr;
            if (!m_prev) {
                undo_remove_element(sp);
                continue;
            }
            ChordRest *first_available_cr = nullptr;
            // search for last cr in this voice
            Segment *seg = m_prev->last();
            if (seg->segment_type() != SegmentType::CHORD_REST) {
                seg = seg->prev(SegmentType::CHORD_REST);
            }
            ChordRest *sp_start_cr = sp->start_cr();
            Fraction sp_start_cr_tick = sp_start_cr->tick();
            while (seg) {
                first_available_cr = seg->cr(sp->track2());
                if (!first_available_cr || !first_available_cr->is_chord()) {
                    seg = seg->prev(SegmentType::CHORD_REST);
                    continue;
                }
                auto graces_after = to_chord(first_available_cr)->grace_notes_after();
                if (first_available_cr->tick() < sp_start_cr_tick) {
                    // we would create a negative-length slur. remove it entirely.
                    undo_remove_element(sp);
                    break;
                } else if (sp_start_cr == to_chord_rest(first_available_cr)) {
                    // this would be a zero-length slur, but a possibility exists that this cr has grace after
                    if (graces_after.empty()) {
                        undo_remove_element(sp);
                        break;
                    }
                }
                if (!graces_after.empty()) {
                    first_available_cr = graces_after.back();
                }
                Spanner *new_sp = to_spanner(sp->clone());
                new_sp->set_end_element(first_available_cr);
                new_sp->set_tick2(first_available_cr->tick());
                undo_change_element(sp, new_sp);
                break;
            }
        } else {
            // both of the endpoints are outside of the deleted range. there is no reason to change or delete anything
        }
    }
}

//---------------------------------------------------------
//   deleteSpannersFromRange
///   Deletes spanners in the given range that match the
///   given selection filter.
//---------------------------------------------------------

void Score::delete_or_shorten_out_spanners_from_range(const Fraction &t1, const Fraction &t2, track_idx_t track1, track_idx_t track2,
                                                      const SelectionFilter &filter) {
    static const std::set<ElementType> spanner_types_to_shorten_out{
        ElementType::HAIRPIN,
        ElementType::OTTAVA,
        ElementType::TRILL,
        ElementType::VIBRATO,
    };

    // undoRemoveElement and undoChangeProperty also handle linked elements, so we need
    // to make sure each linked group is handled only once if multiple elements per linked
    // group are found in the range, for example in the case of linked staves.
    std::unordered_set<EngravingObject *> handled_main_elements;

    auto spanners = spanner_.find_overlapping(t1.ticks(), t2.ticks() - 1);
    for (auto i : spanners) {
        Spanner *sp = i.value;
        Fraction sp_start_tick = sp->tick();
        Fraction sp_end_tick = sp->tick2();
        const bool track_valid = sp->track() >= track1 && sp->track() < track2;
        if (!track_valid || sp->is_volta() || sp->system_flag()) {
            continue;
        }

        if (sp->links() && !handled_main_elements.insert(sp->links()->main_element()).second) {
            // already handled this linked group
            continue;
        }

        // Special handling for grace notes...
        const Chord *start_chord = sp->start_chord();
        const bool start_chord_is_selectable_grace = start_chord && start_chord->is_grace() && filter.can_select(start_chord);

        const Chord *end_chord = sp->end_chord();
        const bool end_chord_is_selectable_grace = end_chord && end_chord->is_grace() && filter.can_select(end_chord);

        if (start_chord_is_selectable_grace || end_chord_is_selectable_grace) {
            undo_remove_element(sp);
            continue;
        }

        if (!filter.can_select_voice(sp->track()) || !filter.can_select(sp)) {
            continue;
        }
        if (sp_start_tick >= t1 && sp_start_tick < t2 && sp_end_tick > t1 && sp_end_tick <= t2) {
            undo_remove_element(sp);
        } else if (sp->is_slur() && ((sp_start_tick >= t1 && sp_start_tick < t2) || (sp_end_tick >= t1 && sp_end_tick < t2))) {
            undo_remove_element(sp);
        } else if (muse::contains(spanner_types_to_shorten_out, sp->type())) {
            bool move_start = sp_start_tick >= t1 && sp_start_tick < t2;
            bool move_end = sp_end_tick > t1 && sp_end_tick <= t2;
            if (move_start) {
                Fraction tick_diff = t2 - sp_start_tick;
                sp->undo_change_property(Pid::SPANNER_TICK, t2);
                sp->undo_change_property(Pid::SPANNER_TICKS, sp->ticks() - tick_diff);
            } else if (move_end) {
                Fraction tick_diff = sp_end_tick - t1;
                sp->undo_change_property(Pid::SPANNER_TICKS, sp->ticks() - tick_diff);
            }
        }
    }
}

void Score::delete_slurs_from_range(const Fraction &t1, const Fraction &t2, track_idx_t track_start, track_idx_t track_end,
                                    const SelectionFilter &filter) {
    auto spanners = spanner_.find_overlapping(t1.ticks(), t2.ticks() - 1);
    for (auto i : spanners) {
        Spanner *sp = i.value;
        Fraction sp_start_tick = sp->tick();
        Fraction sp_end_tick = sp->tick2();
        if (!sp->is_slur()) {
            continue;
        }
        if (!filter.can_select_voice(sp->track()) || !filter.can_select(sp)) {
            continue;
        }

        if (sp->track() >= track_start && sp->track() < track_end) {
            if ((sp_start_tick >= t1 && sp_start_tick < t2) || (sp_end_tick >= t1 && sp_end_tick < t2)) {
                undo_remove_element(sp);
            }
        }
    }
}

//---------------------------------------------------------
//   deleteAnnotationsFromRange
///   Deletes annotations in the given range that match the
///   given selection filter.
//---------------------------------------------------------

void Score::delete_annotations_from_range(Segment *s1, Segment *s2, track_idx_t track1, track_idx_t track2, const SelectionFilter &filter) {
    if (!s1) {
        return;
    }
    if (s2 && (*s2) < (*s1)) {
        return;
    }

    for (track_idx_t track = track1; track < track2; ++track) {
        if (!filter.can_select_voice(track)) {
            continue;
        }
        for (Segment *s = s1; s && s != s2; s = s->next1()) {
            const auto annotations = s->annotations(); // make a copy since we alter the list
            for (EngravingItem *annotation : annotations) {
                // skip if not included in selection (eg, filter)
                if (!filter.can_select(annotation)) {
                    continue;
                }
                if (!annotation->system_flag() && annotation->track() == track) {
                    delete_item(annotation);
                }
            }
        }
    }
}

void Score::delete_range_at_track(std::vector<ChordRest *> &crs_to_select, const track_idx_t track, Segment *start_seg,
                                  const Fraction &end_tick, Tuplet *current_tuplet, const SelectionFilter &filter,
                                  bool selection_contains_multi_note_chords) {
    while (start_seg && !(start_seg->is_chord_rest_type() && start_seg->cr(track))) {
        // Range should always start at a ChordRest segment - find the next one for this track...
        start_seg = start_seg->next1(SegmentType::CHORD_REST);
    }

    if (!start_seg) {
        return;
    }

    Fraction rest_start_tick = start_seg->tick();
    Fraction rest_duration;

    // When we find a deselected DurationElement - write rests up to the start of the deselected element (using the existing values for
    // restStartTick and restDuration), skip the deselected element (move restStartTick forward), and reset restDuration...
    const auto found_deselected = [&](const DurationElement *deselected_element) {
        const std::vector<Rest *> rests = set_rests(rest_start_tick, track, rest_duration, /*useDots*/ !current_tuplet, current_tuplet);
        crs_to_select.insert(crs_to_select.end(), rests.begin(), rests.end());
        rest_start_tick = deselected_element->end_tick();
        rest_duration = Fraction();
    };

    for (Segment *s = start_seg; s && (s->tick() < end_tick); s = s->next1()) {
        EngravingItem *e = s->element(track);
        if (!e) {
            continue;
        }

        if (s->is_breath_type()) {
            delete_item(e);
            continue;
        }

        if (!s->is_chord_rest_type()) {
            // do not delete TimeSig/KeySig, it doesn't make sense to do it (except on full system)
            if (!s->is_time_tick_type() && !s->is_time_sig_type() && !s->is_key_sig_type() &&
                !s->is_type(SegmentType::BAR_LINE_TYPES) /*covers all barLine types*/) {
                undo_remove_element(e);
            }
            continue;
        }

        if (e->is_measure_repeat()) {
            // MeasureRepeat has its own special handling, so it is skipped here
            found_deselected(to_duration_element(e));
            delete_item(e);
            continue;
        }

        ChordRest *cr1 = to_chord_rest(e);

        // currentTuplet refers to the tuplet that we're currently performing a delete range "inside". It is possible that currentTuplet contains nested
        // tuplets, in which case we should find the "top-most nested tuplet" (nextTuplet) and:
        //      1. If it is selectable, delete it completely using cmdDeleteTuplet (deletes all elements contained in that tuplet)
        //      2. If it is not selectable, call this method recursively "inside" that tuplet (nextTuplet becomes currentTuplet for the recursive call)

        Tuplet *next_tuplet = nullptr;
        if (cr1->tuplet() && cr1->tuplet() != current_tuplet) {
            next_tuplet = cr1->tuplet();
            while (next_tuplet->tuplet() && next_tuplet->tuplet() != current_tuplet) {
                next_tuplet = next_tuplet->tuplet();
            }
        }

        if (next_tuplet) {
            // The range of nextTuplet will be handled by cmdDeleteTuplet or the recursive call - skip it at this level of recursion...
            const DurationElement *last_in_tuplet = next_tuplet->elements().back();
            while (last_in_tuplet->is_tuplet()) {
                last_in_tuplet = to_tuplet(last_in_tuplet)->elements().back();
            }
            IF_ASSERT_FAILED(last_in_tuplet->is_chord_rest()) { break; }
            s = to_chord_rest(last_in_tuplet)->segment();

            if (filter.can_select_tuplet(next_tuplet, cr1->tick(), end_tick, selection_contains_multi_note_chords)) {
                rest_duration += next_tuplet->ticks();
                cmd_delete_tuplet(next_tuplet, /*replaceWithRest*/ false);
            } else {
                const Fraction recursion_end = std::min(next_tuplet->end_tick(), end_tick);
                delete_range_at_track(crs_to_select, track, cr1->segment(), recursion_end, next_tuplet, filter,
                                      selection_contains_multi_note_chords);
                found_deselected(next_tuplet);
            }

            if (!s) {
                break;
            }

            continue;
        }

        for (EngravingItem *elem : collect_elements_anchored_to_chord_rest(cr1)) {
            if (!elem->is_chord()) {
                if (filter.can_select(elem)) {
                    undo_remove_element(elem);
                }
                continue;
            }
            // Special handling for grace notes...
            const Chord *chord = to_chord(elem);
            IF_ASSERT_FAILED(chord->is_grace()) { continue; }
            std::unordered_set<EngravingItem *> anchored_to_grace = collect_elements_anchored_to_chord_rest(chord);
            for (const Note *grace_note : chord->notes()) {
                const std::unordered_set<EngravingItem *> note_anchored = collect_elements_anchored_to_note(grace_note, true, false);
                anchored_to_grace.insert(note_anchored.begin(), note_anchored.end());
            }
            for (EngravingItem *grace_elem : anchored_to_grace) {
                if (filter.can_select(grace_elem)) {
                    // Delete elements anchored to grace note...
                    undo_remove_element(grace_elem);
                }
            }
            if (filter.can_select(elem)) {
                // Delete grace note itself...
                undo_remove_element(elem);
            }
        }

        if (cr1->is_rest_family()) {
            if (cr1->selected()) {
                rest_duration += cr1->ticks();
                remove_chord_rest(cr1, true);
            } else {
                found_deselected(cr1);
            }
            continue;
        }

        Chord *chord = to_chord(cr1);

        const std::vector<Note *> all_notes = chord->notes();
        std::unordered_set<Note *> notes_to_remove;
        for (size_t note_idx = 0; note_idx < all_notes.size(); ++note_idx) {
            Note *note = all_notes.at(note_idx);
            if (filter.can_select_note_idx(note_idx, all_notes.size(), selection_contains_multi_note_chords)) {
                notes_to_remove.emplace(note);
                //! NOTE: Don't need to remove anchored elements - they won't survive the deletion of their parent note...
                continue;
            }
            for (EngravingItem *elem : collect_elements_anchored_to_note(note, true, false)) {
                if (!filter.can_select(elem)) {
                    continue;
                }
                if (!elem->is_spanner() || elem->is_partial_tie() || elem->is_laissez_vib()) {
                    undo_remove_element(elem);
                    continue;
                }
                if (note_anchored_spanner_is_in_range(to_spanner(elem), cr1->tick(), end_tick)) {
                    undo_remove_element(elem);
                }
            }
            if (!filter.can_select_note_idx(note_idx, all_notes.size(), selection_contains_multi_note_chords)) {
                continue;
            }
            notes_to_remove.emplace(note);
        }

        if (notes_to_remove.size() == all_notes.size()) {
            rest_duration += cr1->ticks();
            remove_chord_rest(cr1, true);
        } else {
            for (Note *note : notes_to_remove) {
                undo_remove_element(note);
            }
            found_deselected(cr1);
        }
    }

    const std::vector<Rest *> rests = set_rests(rest_start_tick, track, rest_duration, /*useDots*/ !current_tuplet, current_tuplet);
    crs_to_select.insert(crs_to_select.end(), rests.begin(), rests.end());
}

//---------------------------------------------------------
//   deleteRange
///   Deletes elements in the given range that match the
///   given selection filter.
///   \return A chord/rest inside the selected range
///   that can be used to establish a selection after this
///   deletion operation.
//---------------------------------------------------------

std::vector<ChordRest *> Score::delete_range(Segment *s1, Segment *s2, track_idx_t track1, track_idx_t track2,
                                             const SelectionFilter &filter, bool selection_contains_multi_note_chords) {
    std::vector<ChordRest *> crs_for_selection;
    IF_ASSERT_FAILED(s1) { return crs_for_selection; }

    // delete content from measures underlying mmrests
    if (s1->measure() && s1->measure()->is_mm_rest()) {
        s1 = s1->measure()->mm_rest_first()->first();
    }
    if (s2 && s2->measure() && s2->measure()->is_mm_rest()) {
        s2 = s2->measure()->mm_rest_last()->last();
    }

    const Fraction start_tick = s1->tick();
    const Fraction end_tick = s2 ? s2->tick() : last_measure()->end_tick();

    delete_or_shorten_out_spanners_from_range(start_tick, end_tick, track1, track2, filter);
    delete_annotations_from_range(s1, s2, track1, track2, filter);

    for (track_idx_t track = track1; track < track2; ++track) {
        if (!filter.can_select_voice(track)) {
            continue;
        }
        delete_range_at_track(crs_for_selection, track, s1, end_tick, /*currentTuplet*/ nullptr, filter,
                              selection_contains_multi_note_chords);
    }

    return crs_for_selection;
}

//---------------------------------------------------------
//   cmdDeleteSelection
//---------------------------------------------------------

void Score::cmd_delete_selection() {
    std::vector<ChordRest *> crs_selected_after_deletion; // select something after deleting notes
    EngravingItem *el_selected_after_deletion = nullptr;

    if (selection().is_range()) {
        crs_selected_after_deletion =
            delete_range(selection().start_segment(), selection().end_segment(), staff2track(selection().staff_start()),
                         staff2track(selection().staff_end()), selection_filter(), selection().range_contains_multi_note_chords());
    } else {
        // deleteItem modifies selection().elements() list,
        // so we need a local copy:
        std::vector<EngravingItem *> el = selection().elements();

        // keep track of linked elements that are deleted implicitly
        // so we don't try to delete them twice if they are also in selection
        std::set<EngravingObject *> deleted_elements;
        // Similarly, deleting one spanner segment, will delete all of them
        // so we don't try to delete them twice if they are also in selection
        std::set<Spanner *> deleted_spanners;

        auto select_cr_at_tick_and_track = [this, &crs_selected_after_deletion](Fraction tick, track_idx_t track) {
            ChordRest *cr = find_cr(tick, track);
            if (cr) {
                crs_selected_after_deletion.push_back(cr);
            }
        };

        for (EngravingItem *e : el) {
            // these are the linked elements we are about to delete
            std::list<EngravingObject *> links;
            if (e->links()) {
                links = *e->links();
            }

            // find location of element to select after deleting notes
            // get tick of element itself if that is valid
            // or of spanner or parent if that is more valid
            Fraction tick = {-1, 1};
            track_idx_t track = muse::nidx;
            if (e->is_note()) {
                tick = to_note(e)->chord()->tick();
            } else if (e->is_rest() || e->is_mm_rest()) {
                tick = to_rest(e)->tick();
            } else if (e->is_measure_repeat()) { // may be attached in different measure than it appears
                tick = to_measure_repeat(e)->first_measure_of_group()->first()->tick();
            } else if (e->is_spanner_segment()) {
                tick = to_spanner_segment(e)->spanner()->tick();
            } else if (e->is_breath()) {
                // we want the tick of the ChordRest that precedes the breath mark (in the same track)
                for (Segment *s = to_breath(e)->segment()->prev(); s; s = s->prev()) {
                    if (s->is_chord_rest_type() && s->element(e->track())) {
                        tick = s->tick();
                        break;
                    }
                }
            } else if (e->is_bar_line() && to_bar_line(e)->bar_line_type() != BarLineType::START_REPEAT) {
                // we want the tick of the ChordRest that precedes the barline (in the same track)
                for (Segment *s = to_bar_line(e)->segment()->prev(); s; s = s->prev()) {
                    if (s->is_chord_rest_type() && s->element(e->track())) {
                        tick = s->tick();
                        break;
                    }
                }
            } else if (e->is_sound_flag()) {
                tick = e->tick();
            } else if (e->is_tuplet()) {
                // the rests replacing the tuplet start at its tick
                tick = to_tuplet(e)->tick();
            } else if (e->ownership_parent() && (e->ownership_parent()->is_segment() || e->ownership_parent()->is_chord() ||
                                                 e->ownership_parent()->is_note() || e->ownership_parent()->is_rest())) {
                tick = e->ownership_parent_item()->tick();
            }
            //else tick < 0
            track = e->track();

            // We should not allow deleting the very first keySig of the piece, because it is
            // logically incorrect and leads to a state of undefined key/transposition.
            // Also instrument change key signatures should be undeletable.
            // The correct action is for the user to set an atonal/custom keySig as needed.
            if (e->is_key_sig()) {
                if (e->tick() == Fraction(0, 1) || to_key_sig(e)->for_instrument_change()) {
                    MScore::set_error(MsError::CANNOT_REMOVE_KEY_SIG);
                    select_cr_at_tick_and_track(tick, track);
                    continue;
                }
            }

            // Don't allow deleting the trill cue note
            if (e->is_note() && to_note(e)->is_trill_cue_note()) {
                select_cr_at_tick_and_track(tick, track);
                continue;
            }

            if (e->is_fret_diagram() || e->is_harmony()) {
                FBox *fbox = to_f_box(e->find_ancestor(ElementType::FBOX));
                if (fbox) {
                    // We can't delete elements inside fret box, instead we hide them
                    if (FretDiagram *fret_diagram = e->is_fret_diagram() ? to_fret_diagram(e) : to_fret_diagram(e->parent())) {
                        fret_diagram->undo_change_property(Pid::VISIBLE, false);
                        continue;
                    }
                } else if (e->is_fret_diagram()) {
                    FretDiagram *fret_diagram = to_fret_diagram(e);
                    Harmony *harmony = fret_diagram->harmony();
                    if (harmony) {
                        undo_change_parent(harmony, fret_diagram->segment(), track2staff(fret_diagram->track()));
                        el_selected_after_deletion = harmony;
                    }
                } else if (e->is_harmony()) {
                    Harmony *harmony = to_harmony(e);
                    if (harmony->ownership_parent_item()->is_fret_diagram()) {
                        el_selected_after_deletion = harmony->ownership_parent_item();
                    }
                }
            }

            // delete element if we have not done so already
            if (deleted_elements.find(e) == deleted_elements.end()) {
                // do not delete two spanner segments from the same spanner
                if (e->is_spanner_segment()) {
                    Spanner *spanner = to_spanner_segment(e)->spanner();
                    if (deleted_spanners.find(spanner) != deleted_spanners.end()) {
                        continue;
                    } else {
                        std::list<EngravingObject *> linked_spanners;
                        if (spanner->links()) {
                            linked_spanners = *spanner->links();
                        } else {
                            linked_spanners.push_back(spanner);
                        }
                        for (EngravingObject *se : linked_spanners) {
                            deleted_spanners.insert(to_spanner(se));
                        }
                    }
                }
                delete_item(e);
            }

            if (!el_selected_after_deletion) {
                select_cr_at_tick_and_track(tick, track);
            }

            // add these linked elements to list of already-deleted elements
            for (EngravingObject *se : links) {
                deleted_elements.insert(se);
            }
        }
    }

    deselect_all();
    // make new selection if appropriate
    if (note_entry_mode()) {
        if (!crs_selected_after_deletion.empty()) {
            is_.set_segment(crs_selected_after_deletion.front()->segment());
        } else {
            crs_selected_after_deletion.push_back(is_.cr());
        }
    }
    if (el_selected_after_deletion) {
        select(el_selected_after_deletion);
    } else if (!crs_selected_after_deletion.empty()) {
        std::vector<EngravingItem *> elements_to_select;
        for (ChordRest *cr : crs_selected_after_deletion) {
            if (cr) {
                if (cr->is_chord()) {
                    elements_to_select.push_back(dynamic_cast<EngravingItem *>(to_chord(cr)->up_note()));
                }
                if (cr->is_rest()) {
                    elements_to_select.push_back(dynamic_cast<EngravingItem *>(cr));
                }
            }
        }
        select(elements_to_select, SelectType::ADD, 0);
    }
}

//---------------------------------------------------------
//   cmdFullMeasureRest
//---------------------------------------------------------

void Score::cmd_full_measure_rest() {
    Segment *s1 = nullptr;
    Segment *s2 = nullptr;
    Fraction stick1 = {-1, 1};
    Fraction stick2 = {-1, 1};
    track_idx_t track1 = muse::nidx;
    track_idx_t track2 = muse::nidx;
    Rest *r = nullptr;

    if (note_entry_mode()) {
        s1 = input_state().segment();
        if (!s1 || s1->rtick().is_not_zero()) {
            return;
        }
        Measure *m = s1->measure();
        s2 = m->last();
        stick1 = s1->tick();
        stick2 = s2->tick();
        track1 = input_state().track();
        track2 = track1 + 1;
    } else if (selection().is_range()) {
        s1 = selection().start_segment();
        s2 = selection().end_segment();
        if (style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS)) {
            // use underlying measures
            if (s1 && s1->measure()->is_mm_rest()) {
                s1 = tick2segment(stick1);
            }
            if (s2 && s2->measure()->is_mm_rest()) {
                s2 = tick2segment(stick2, true);
            }
        }
        stick1 = selection().tick_start();
        stick2 = selection().tick_end();
        Segment *ss1 = s1;
        if (ss1 && ss1->segment_type() != SegmentType::CHORD_REST) {
            ss1 = ss1->next1(SegmentType::CHORD_REST);
        }
        bool full_measure =
            ss1 && (ss1->measure()->first(SegmentType::CHORD_REST) == ss1) &&
            (s2 == 0 || (s2->segment_type() == SegmentType::END_BAR_LINE) || (s2->segment_type() == SegmentType::TIME_SIG_ANNOUNCE) ||
             (s2->segment_type() == SegmentType::KEY_SIG_ANNOUNCE));
        if (!full_measure) {
            return;
        }
        track1 = selection().staff_start() * ::mu::engraving::voices;
        track2 = selection().staff_end() * ::mu::engraving::voices;
    } else if (selection().cr()) {
        ChordRest *cr = selection().cr();
        if (!cr || cr->rtick().is_not_zero()) {
            return;
        }
        Measure *m = cr->measure();
        s1 = m->first();
        s2 = m->last();
        stick1 = s1->tick();
        stick2 = s2->tick();
        track1 = selection().cr()->track();
        track2 = track1 + 1;
    } else {
        return;
    }

    for (track_idx_t track = track1; track < track2; ++track) {
        if (selection().is_range() && !selection_filter().can_select_voice(track)) {
            continue;
        }
        // first pass - remove non-initial rests from empty measures/voices
        for (Segment *s = s1; s != s2; s = s->next1()) {
            if (!(s->measure()->is_only_rests(track))) { // Don't remove anything from measures that contain notes
                continue;
            }
            if (s->segment_type() != SegmentType::CHORD_REST || !s->element(track)) {
                continue;
            }
            ChordRest *cr = to_chord_rest(s->element(track));
            // keep first rest of measure as placeholder (replaced in second pass)
            // but delete all others
            if (s->rtick().is_not_zero()) {
                remove_chord_rest(cr, true);
            }
        }
        // second pass - replace placeholders with full measure rests
        for (Measure *m = s1->measure(); m; m = m->next_measure()) {
            if (m->is_only_rests(track)) {
                ChordRest *cr = m->find_chord_rest(m->tick(), track);
                if (cr) {
                    remove_chord_rest(cr, true);
                    r = add_rest(m->tick(), track, TDuration(DurationType::V_MEASURE), 0);
                } else if (note_entry_mode()) {
                    // might be no cr at input position
                    r = add_rest(m->tick(), track, TDuration(DurationType::V_MEASURE), 0);
                }
            }
            if (s2 && (m == s2->measure())) {
                break;
            }
        }
    }

    // selected range is probably empty now and possibly subsumed by an mmrest
    // so updating selection requires forcing mmrests to be updated first
    s1 = tick2segment_mm(stick1);
    s2 = tick2segment_mm(stick2, true);
    if (selection().is_range() && s1 && s2) {
        selection_.set_start_segment(s1);
        selection_.set_end_segment(s2);
        selection_.update_selected_elements();
    } else if (r) {
        // note entry mode
        select(r, SelectType::SINGLE);
    } else {
        deselect_all();
    }
}

//---------------------------------------------------------
//   cmdCreateTuplet
//    replace cr with tuplet
//---------------------------------------------------------

void Score::cmd_create_tuplet(ChordRest *ocr, Tuplet *tuplet) {
    track_idx_t track = ocr->track();
    Measure *measure = ocr->measure();
    Fraction tick = ocr->tick();
    Fraction an = (tuplet->ticks() * tuplet->ratio()) / tuplet->base_len().fraction();
    if (!an.denominator()) {
        return;
    }

    if (ocr->tuplet()) {
        tuplet->set_tuplet(ocr->tuplet());
    }
    // A tuplet is not attached to a segment: it hangs off the chord rests added below, so the
    // undo stack has to own it, as RemoveElement does for a tuplet when the last of its elements
    // goes. The command comes first, so that an undo drops it only after the chord rests that
    // reference it are gone.
    do_undo_add_element(tuplet);
    remove_chord_rest(ocr, false);

    ChordRest *cr;
    if (ocr->is_chord()) {
        cr = Factory::create_chord(this->dummy());
        to_chord(cr)->set_stem_direction(to_chord(ocr)->stem_direction());
        for (Note *old_note : to_chord(ocr)->notes()) {
            Note *note = Factory::create_note(to_chord(cr));
            note->set_pitch(old_note->pitch());
            note->set_tpc1(old_note->tpc1());
            note->set_tpc2(old_note->tpc2());
            cr->add(note);
        }
    } else {
        cr = Factory::create_rest(this->dummy());
    }

    int actual_notes = an.numerator() / an.denominator();

    tuplet->set_track(track);
    cr->set_tuplet(tuplet);
    cr->set_track(track);
    cr->set_duration_type(tuplet->base_len());
    cr->set_ticks(tuplet->base_len().fraction());

    undo_add_cr(cr, measure, tick);

    Fraction ticks = cr->actual_ticks();

    for (int i = 0; i < (actual_notes - 1); ++i) {
        tick += ticks;
        Rest *rest = Factory::create_rest(this->dummy());
        rest->set_tuplet(tuplet);
        rest->set_track(track);
        rest->set_duration_type(tuplet->base_len());
        rest->set_ticks(tuplet->base_len().fraction());
        undo_add_cr(rest, measure, tick);
    }
}

//---------------------------------------------------------
//   cmdEnterRest
//---------------------------------------------------------

void Score::cmd_enter_rest(const TDuration &d) {
    if (is_.track() == muse::nidx) {
        LOGD("cmdEnterRest: track invalid");
        return;
    }
    start_cmd(TranslatableString("undoableAction", "Enter rest"));
    enter_rest(d);
    end_cmd();
}

//---------------------------------------------------------
//   enterRest
//---------------------------------------------------------

void Score::enter_rest(const TDuration &d, InputState *external_input_state) {
    InputState &is = external_input_state ? (*external_input_state) : is_;

    if (is.beyond_score()) {
        append_measures(1);
        is.move_to_next_input_pos();
    }

    expand_voice(is.segment(), is.track());

    if (!is.cr()) {
        LOGD("cannot enter rest here");
        return;
    }

    const track_idx_t track = is.track();
    NoteVal nval;
    set_note_rest(is.segment(), track, nval, d.fraction(), DirectionV::AUTO, /* forceAccidental */ false, is.articulation_ids(),
                  /* rhythmic */ false, external_input_state);
    is.move_to_next_input_pos();
    if (!is.note_entry_mode() || is.using_note_entry_method(NoteEntryMethod::BY_NOTE_NAME)) {
        is.set_rest(false); // continue with normal note entry
    }
}

//---------------------------------------------------------
//   removeChordRest
//    remove chord or rest
//    remove associated segment if empty
//    remove beam
//    remove slurs
//---------------------------------------------------------

void Score::remove_chord_rest(ChordRest *cr, bool clear_segment) {
    std::set<Segment *> segments;
    for (EngravingObject *e : cr->link_list()) {
        if (cr->is_chord()) {
            std::set<Spanner *> starting_spanners = to_chord(e)->starting_spanners();
            for (Spanner *spanner : starting_spanners) {
                if (spanner->is_trill() || spanner->is_slur()) {
                    do_undo_remove_element(spanner);
                }
            }
        }
        do_undo_remove_element(static_cast<EngravingItem *>(e));
        if (clear_segment) {
            Segment *s = to_chord_rest(e)->segment();
            if (segments.find(s) == segments.end()) {
                segments.insert(s);
            }
        }
    }
    for (Segment *s : segments) {
        if (s->empty()) {
            do_undo_remove_element(s);
        }
    }
    if (cr->beam()) {
        Beam *beam = cr->beam();
        if (beam->generated()) {
            remove_element(beam);
            delete beam;
        } else {
            undo_remove_element(beam);
        }
    }
}

//---------------------------------------------------------
//   cmdDeleteTuplet
//    remove tuplet and replace with rest
//---------------------------------------------------------

void Score::cmd_delete_tuplet(Tuplet *tuplet, bool replace_with_rest) {
    std::vector<DurationElement *> elements = tuplet->elements();
    for (DurationElement *de : elements) {
        if (de->is_chord_rest()) {
            remove_chord_rest(to_chord_rest(de), true);
        } else {
            assert(de->is_tuplet());
            cmd_delete_tuplet(to_tuplet(de), false);
        }
    }
    if (replace_with_rest) {
        set_rest(tuplet->tick(), tuplet->track(), tuplet->ticks(), true, tuplet->tuplet());
    }
}

//---------------------------------------------------------
//   insertMeasure
//    if inserting Measure, act on master score
//    (and add to all linked scores)
//    if inserting Box, add to local score only
//---------------------------------------------------------

MeasureBase *Score::insert_measure(ElementType type, MeasureBase *before_measure, const InsertMeasureOptions &options) {
    MeasureBase *local_insert_measure_base = nullptr;
    if (type == ElementType::MEASURE) {
        if (MeasureBase *master_insert_measure = master_score()->insert_measure(before_measure, options)) {
            local_insert_measure_base = tick2measure(master_insert_measure->tick());
        }
    } else {
        local_insert_measure_base = insert_box(type, before_measure, options);
    }

    if (local_insert_measure_base && options.need_deselect_all) {
        deselect_all();
    }

    return local_insert_measure_base;
}

//---------------------------------------------------------
//   insertBox
//---------------------------------------------------------

MeasureBase *Score::insert_box(ElementType type, MeasureBase *before_measure, const InsertMeasureOptions &options) {
    const bool is_frame = type == ElementType::FBOX || type == ElementType::HBOX || type == ElementType::TBOX || type == ElementType::VBOX;
    if (!is_frame) {
        return nullptr;
    }

    return this->insert_box(to_measure_base(Factory::create_item(type, dummy())), before_measure, options);
}

MeasureBase *Score::insert_box(MeasureBase *box, MeasureBase *before_measure, const InsertMeasureOptions &options) {
    IF_ASSERT_FAILED(box) { return nullptr; }

    ElementType type = box->type();
    const bool is_frame = type == ElementType::FBOX || type == ElementType::HBOX || type == ElementType::TBOX || type == ElementType::VBOX;
    if (!is_frame) {
        return nullptr;
    }

    if (box->score() != this) {
        box->set_score(this);
    }

    Fraction tick;
    bool is_title_frame = type == ElementType::VBOX && before_measure && before_measure == before_measure->score()->first();
    if (before_measure) {
        if (before_measure->is_measure()) {
            Measure *m = to_measure(before_measure);
            if (m->is_mm_rest()) {
                before_measure = m->mm_rest_first();
                if (options.need_deselect_all) {
                    deselect_all();
                }
            }
            for (size_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
                if (m->is_measure_repeat_group_with_prev_m(staff_idx)) {
                    MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
                    return nullptr;
                }
            }
        } else {
            before_measure = before_measure->top(); // don't try to insert in front of nested frame
        }
        tick = before_measure->tick();
    } else {
        tick = last() ? last()->end_tick() : Fraction(0, 1);
    }

    box->set_tick(tick);
    box->set_next(before_measure);
    box->set_prev(before_measure ? before_measure->prev() : last());
    box->set_size_is_spatium_dependent(!is_title_frame);

    if (type == ElementType::FBOX) {
        to_f_box(box)->init();
    }

    undo(new InsertMeasures(box, box));

    if (options.need_deselect_all) {
        deselect_all();
    }

    if (options.clone_box_to_all_parts) {
        box->manage_exclusion_from_parts(/*exclude =*/false);
    }

    return box;
}

void Score::restore_initial_key_sig_and_time_sig() {
    bool concert_pitch = style().style_b(Sid::CONCERT_PITCH);
    static constexpr Fraction start_tick = Fraction(0, 1);

    Measure *first_meas = first_measure();
    for (Staff *staff : staves_) {
        Key concert_key = Key::C;
        Key transposed_key = concert_key;
        const Part *part = staff->part();
        const Instrument *instrument = part->instrument();
        int transpose = -instrument->transpose().chromatic;
        if (!concert_pitch) {
            transposed_key = Transpose::transpose_key(transposed_key, transpose, part->prefer_sharp_flat());
        }

        Segment *key_segment = first_meas->undo_get_segment(SegmentType::KEY_SIG, start_tick);

        KeySigEvent key_sig_event;
        key_sig_event.set_concert_key(concert_key);
        key_sig_event.set_key(transposed_key);
        // use atonal KS for drums
        if (instrument->use_drumset()) {
            key_sig_event.set_custom(true);
            key_sig_event.set_mode(KeyMode::NONE);
        }

        KeySig *new_key_sig = Factory::create_key_sig(key_segment);
        new_key_sig->set_track(staff->idx() * ::mu::engraving::voices);
        key_segment->add(new_key_sig);
        new_key_sig->set_key_sig_event(key_sig_event);
        undo_add_element(new_key_sig);

        staff->set_key(Fraction(0, 1), key_sig_event);

        Segment *time_segment = first_meas->undo_get_segment(SegmentType::TIME_SIG, start_tick);
        TimeSig *new_time_sig = Factory::create_time_sig(time_segment);
        new_time_sig->set_track(staff->idx() * ::mu::engraving::voices);
        new_time_sig->set_sig(Fraction(4, 4));
        time_segment->add(new_time_sig);
        undo_add_element(new_time_sig);
    }

    first_meas->set_timesig(Fraction(4, 4));
    first_meas->set_ticks(Fraction(4, 4));
}

//---------------------------------------------------------
//   checkSpanner
//    check if spanners are still valid as anchors may
//    have changed or be removed.
//    Spanners need to have a start anchor. Slurs need a
//    start and end anchor.
//---------------------------------------------------------

void Score::check_spanner(const Fraction &start_tick, const Fraction &end_tick, bool remove_orphans) {
    std::vector<Spanner *> spanners_to_remove;
    std::vector<Spanner *> spanners_to_shorten;
    auto spanners = spanner_.find_overlapping(start_tick.ticks(), end_tick.ticks());

    // DEBUG: check all spanner
    //        there may be spanners outside of score bc. some measures were deleted

    Fraction last_tick = last_measure()->end_tick();

    for (const auto &i : spanner_.map()) {
        Spanner *s = i.second;

        if (s->is_slur()) {
            Segment *seg = tick2segment_mm(s->tick(), false, SegmentType::CHORD_REST);
            if (!seg || !seg->element(s->track())) {
                spanners_to_remove.push_back(s);
            } else {
                seg = tick2segment_mm(s->tick2(), false, SegmentType::CHORD_REST);
                if (!seg || !seg->element(s->track2())) {
                    spanners_to_remove.push_back(s);
                }
            }
        } else {
            // remove spanner if there is no start element
            s->compute_start_element();
            if (!s->start_element()) {
                spanners_to_remove.push_back(s);
                LOGD("checkSpanner::remove (3)");
            } else {
                if (s->tick2() > last_tick) {
                    spanners_to_shorten.push_back(s); //s->undoChangeProperty(Pid::SPANNER_TICKS, lastTick - s->tick());
                } else {
                    s->compute_end_element();
                }
            }
        }
    }
    if (remove_orphans) {
        for (auto s : spanners_to_remove) { // actually remove scheduled spanners
            do_undo_remove_element(s);
        }
    }
    for (Spanner *s : spanners_to_shorten) { // shorten spanners that extended past end of score
        undo(new ChangeProperty(s, Pid::SPANNER_TICKS, last_tick - s->tick()));
        s->compute_end_element();
    }
}

//---------------------------------------------------------
//   checkTimeDelete
//---------------------------------------------------------

bool Score::check_time_delete(Segment *start_segment, Segment *end_segment) {
    Measure *start_measure = start_segment->measure();
    Measure *end_measure;

    if (end_segment) {
        end_measure = end_segment->prev() ? end_segment->measure() : end_segment->measure()->prev_measure();
    } else {
        end_measure = last_measure();
    }

    Fraction end_tick = end_segment ? end_segment->tick() : end_measure->end_tick();
    Fraction tick = start_segment->tick();
    Fraction etick = (start_measure == end_measure ? end_tick : start_measure->end_tick());

    // check for MeasureRepeat
    bool starts_at_beginning_of_measure = (tick == start_measure->tick());
    bool ends_at_end_of_measure = (end_tick == end_measure->end_tick());
    for (size_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
        if ((start_measure->is_measure_repeat_group(staff_idx) && !starts_at_beginning_of_measure) ||
            (end_measure->is_measure_repeat_group(staff_idx) && !ends_at_end_of_measure) ||
            start_measure->is_measure_repeat_group_with_prev_m(staff_idx) || end_measure->is_measure_repeat_group_with_next_m(staff_idx)) {
            MScore::set_error(MsError::CANNOT_REMOVE_TIME_MEASURE_REPEAT);
            return false;
        }
    }

    bool can_delete_time = true;
    while (can_delete_time) {
        for (size_t track = 0; can_delete_time && track < staves_.size() * ::mu::engraving::voices; ++track) {
            if (start_measure->has_voice(track)) {
                Segment *fs = start_measure->first(SegmentType::CHORD_REST);
                for (Segment *s = fs; s; s = s->next(SegmentType::CHORD_REST)) {
                    if (s->element(track)) {
                        ChordRest *cr = to_chord_rest(s->element(track));
                        Tuplet *t = cr->tuplet();
                        DurationElement *de = t ? to_duration_element(t) : to_duration_element(cr);
                        Fraction f = de->tick() + de->actual_ticks();
                        Fraction cetick = f;
                        if (cetick <= tick) {
                            continue;
                        }
                        if (de->tick() >= etick) {
                            break;
                        }
                        if (t && (t->tick() < tick || cetick > etick)) {
                            can_delete_time = false;
                            break;
                        }
                    }
                }
            }
        }
        if (start_measure == end_measure) {
            break;
        }
        start_measure = end_measure;
        tick = start_measure->tick();
        etick = end_tick;
    }
    if (!can_delete_time) {
        MScore::set_error(MsError::CANNOT_REMOVE_TIME_TUPLET);
        return false;
    }
    return true;
}

//---------------------------------------------------------
//   cmdTimeDelete
///    delete time by decreasing measure length if partial measures are selected
//---------------------------------------------------------

void Score::cmd_time_delete() {
    Transaction &tx = transaction_manager()->current_or_dummy_transaction();

    EngravingItem *e = selection().element();

    if (e && e->is_bar_line() && to_bar_line(e)->segment()->is_end_bar_line_type()) {
        const Measure *m = to_bar_line(e)->segment()->measure();
        const Measure *next = m->next_measure();
        if (next) {
            SplitJoinMeasure::join_measures(tx, master_score_, m->tick(), next->tick());
        }
        return;
    }

    Segment *start_segment = nullptr;
    Segment *end_segment = nullptr;

    if (selection().state() != SelState::RANGE) {
        if (!e) {
            return;
        }

        ChordRest *cr = nullptr;
        if (e->is_note()) {
            cr = to_note(e)->chord();
        } else if (e->is_chord_rest()) {
            cr = to_chord_rest(e);
        } else {
            return;
        }

        start_segment = cr->segment();
        Fraction end_tick = cr->end_tick();
        end_segment = tick2measure(end_tick)->find_segment(SegmentType::CHORD_REST, end_tick);
    } else {
        start_segment = selection().start_segment();
        end_segment = selection().end_segment();
    }

    if (!is_master() && master_score()) {
        Fraction start_tick = start_segment->tick();
        Measure *master_start_meas = master_score()->tick2measure(start_tick);
        Segment *master_start_seg = master_start_meas->find_segment(start_segment->segment_type(), start_segment->tick());
        Segment *master_end_seg = nullptr;

        if (end_segment) {
            Fraction end_tick = end_segment->tick();
            Measure *master_end_meas = master_score()->tick2measure(end_tick);
            if (end_segment->is_end_bar_line_type()) {
                Measure *prev_master_end_measure = master_end_meas->prev_measure();
                master_end_meas = prev_master_end_measure ? prev_master_end_measure : master_end_meas;
            }
            master_end_seg = master_end_meas->find_segment(end_segment->segment_type(), end_segment->tick());
        }

        master_score()->do_time_delete(master_start_seg, master_end_seg);
    } else {
        do_time_delete(start_segment, end_segment);
    }

    if (note_entry_mode()) {
        Segment *current_segment = end_segment;
        ChordRest *cr = nullptr;
        if (!current_segment && last_measure_mm()) {
            // deleted to end of score - get last cr on current track
            current_segment = last_measure_mm()->last();
            if (current_segment) {
                cr = current_segment->next_chord_rest(is_.track(), true);
                if (cr) {
                    current_segment = cr->segment();
                }
            }
        }
        if (!current_segment) {
            // no cr found - append a new measure
            append_measures(1);
            current_segment = last_measure_mm()->first(SegmentType::CHORD_REST);
        }
        is_.set_segment(current_segment);
        cr = is_.cr();
        if (cr) {
            if (cr->is_chord()) {
                select(to_chord(cr)->up_note(), SelectType::SINGLE);
            } else {
                select(cr, SelectType::SINGLE);
            }
        } else {
            // could not find cr to select,
            // may be that there is a "hole" in the current track
            deselect_all();
        }
    } else {
        deselect_all();
    }
}

void Score::do_time_delete(Segment *start_segment, Segment *end_segment) {
    if (!check_time_delete(start_segment, end_segment)) {
        return;
    }

    MeasureBase *mb_start = start_segment->measure();
    if (mb_start->is_measure() && to_measure(mb_start)->is_mm_rest()) {
        mb_start = to_measure(mb_start)->mm_rest_first();
    }
    MeasureBase *mb_end;

    if (end_segment) {
        mb_end = end_segment->prev(SegmentType::CHORD_REST) ? end_segment->measure() : end_segment->measure()->prev_measure();
    } else {
        mb_end = last_measure();
    }

    Fraction end_tick = end_segment ? end_segment->tick() : mb_end->end_tick();

    for (;;) {
        if (mb_start->tick() != start_segment->tick()) {
            Fraction tick = start_segment->tick();
            Fraction len;
            if (mb_end == mb_start) {
                len = end_tick - tick;
            } else {
                len = mb_start->end_tick() - tick;
            }
            do_time_delete_for_measure(to_measure(mb_start), start_segment, len);
            if (mb_start == mb_end) {
                break;
            }
            mb_start = mb_start->next();
        }
        end_tick = end_segment ? end_segment->tick() : mb_end->end_tick();
        if (mb_end->end_tick() != end_tick) {
            Fraction len = end_tick - mb_end->tick();
            do_time_delete_for_measure(to_measure(mb_end), to_measure(mb_end)->first(), len);
            if (mb_start == mb_end) {
                break;
            }
            mb_end = mb_end->prev();
        }
        delete_measures(mb_start, mb_end);
        break;
    }
}

void Score::do_time_delete_for_measure(Measure *m, Segment *start_segment, const Fraction &f) {
    if (f.is_zero()) {
        return;
    }

    const Fraction tick = start_segment->rtick();
    const Fraction len = f;
    const Fraction etick = tick + len;

    Segment *fs = m->first(SegmentType::CHORD_REST);

    for (size_t track = 0; track < staves_.size() * ::mu::engraving::voices; ++track) {
        if (m->has_voice(track)) {
            for (Segment *s = fs; s; s = s->next(SegmentType::CHORD_REST)) {
                if (s->element(track)) {
                    ChordRest *cr = to_chord_rest(s->element(track));
                    Fraction cetick = cr->rtick() + cr->actual_ticks();

                    if (cetick <= tick) {
                        continue;
                    }
                    if (s->rtick() >= etick) {
                        break;
                    }

                    if (cr->is_full_measure_rest()) {
                        if (cr->rtick() >= tick) {
                            // Move full-measure rest from the deleted area
                            undo_remove_element(cr);
                            ChordRest *new_cr = to_chord_rest(cr->clone());
                            new_cr->set_ticks(cr->ticks() - f);
                            undo_add_cr(new_cr, m, m->tick() + etick);
                        } else {
                            cr->undo_change_property(Pid::DURATION, cr->ticks() - f);
                        }
                    }
                    // inside deleted area
                    else if (s->rtick() >= tick && cetick <= etick) {
                        // inside
                        undo_remove_element(cr);
                    } else if (s->rtick() >= tick) {
                        // running out
                        Fraction ff = cetick - etick;
                        undo_remove_element(cr);
                        create_cr_sequence(ff, cr, tick + len);
                    } else if (s->rtick() < tick && cetick <= etick) {
                        // running in
                        Fraction f1 = tick - s->rtick();
                        change_c_rlen(cr, f1, false);
                    } else {
                        // running in/out
                        Fraction f1 = cr->ticks() - f;
                        change_c_rlen(cr, f1, false);
                    }
                }
            }
        }
    }
    const Fraction abstick = start_segment->tick();
    undo_insert_time(abstick, -len);

    std::vector<Segment *> empty_segments;

    for (Score *score : master_score()->score_list()) {
        Measure *local_measure = score->tick2measure(abstick);

        undo(new InsertTime(score, abstick, -len));

        Fraction updated_tick = tick;
        // tuplets store their tick: move those after the removed time back with their segments
        std::set<Tuplet *> tuplets;
        for (Segment *s = local_measure->first(SegmentType::CHORD_REST); s; s = s->next()) {
            if (s->rtick() < etick || s->rtick() == updated_tick) {
                continue;
            }

            if (s->is_chord_rest_type()) {
                for (EngravingItem *e : s->elist()) {
                    for (Tuplet *t = e ? to_chord_rest(e)->tuplet() : nullptr; t && t->tick() >= abstick + len; t = t->tuplet()) {
                        // shared tuplets follow their origin
                        if (t->origin_items().empty()) {
                            tuplets.insert(t);
                        }
                    }
                }
            }
            s->undo_change_property(Pid::TICK, updated_tick);
            updated_tick += s->ticks();

            if (score->is_master()) {
                if (s->is_chord_rest_type() && !s->has_elements()) {
                    empty_segments.push_back(s);
                }
            }
        }

        for (Tuplet *t : tuplets) {
            t->undo_change_property(Pid::TICK, t->tick() - len);
        }

        undo(new ChangeMeasureLen(local_measure, local_measure->ticks() - f));
    }

    for (Segment *s : empty_segments) {
        if (Segment *ns = s->next(SegmentType::CHORD_REST)) {
            // Move annotations from the empty segment.
            // TODO: do we need to preserve annotations at all?
            // Maybe only some types (Tempo etc.)?
            const auto annotations = s->annotations(); // make a copy since we alter the list
            for (EngravingItem *a : annotations) {
                EngravingItem *a1 = a->clone();
                a1->set_ownership_parent(ns);
                undo_remove_element(a);
                undo_add_element(a1);
            }
        }
    }
}

//---------------------------------------------------------
//   undoPropertyChanged
//    return true if an property was actually changed
//---------------------------------------------------------

bool Score::undo_property_changed(EngravingItem *item, Pid prop_id, const PropertyValue &prop_value, PropertyFlags prop_flags) {
    Transaction &tx = master_score()->transaction_manager()->current_or_dummy_transaction();

    bool changed = false;

    const PropertyValue current_prop_value = item->get_property(prop_id);
    const PropertyFlags current_prop_flags = item->property_flags(prop_id);

    if ((current_prop_value != prop_value) || (current_prop_flags != prop_flags)) {
        item->set_property_flags(prop_id, prop_flags);
        tx.push_without_performing(new ChangeProperty(item, prop_id, prop_value, prop_flags));
        changed = true;
    }

    const std::list<EngravingObject *> linked_items = item->link_list_for_property_propagation();

    for (EngravingObject *linked_item : linked_items) {
        if (linked_item == item) {
            continue;
        }
        PropertyPropagation property_propagate = item->property_propagation(to_engraving_item(linked_item), prop_id);
        switch (property_propagate) {
            case PropertyPropagation::PROPAGATE:
                if (linked_item->get_property(prop_id) != current_prop_value) {
                    tx.push(new ChangeProperty(linked_item, prop_id, current_prop_value, prop_flags));
                    changed = true;
                }
                break;
            case PropertyPropagation::UNLINK: item->unlink_property_from_master(prop_id); break;
            default: break;
        }
    }

    return changed;
}

void Score::undo_property_changed(EngravingObject *e, Pid t, const PropertyValue &st, PropertyFlags ps) {
    if (e->get_property(t) != st) {
        Transaction &tx = master_score()->transaction_manager()->current_or_dummy_transaction();
        tx.push_without_performing(new ChangeProperty(e, t, st, ps));
    }
}

void Score::undo_change_style_val(Sid idx, const PropertyValue &v) {
    std::unordered_map<Sid, PropertyValue> map;
    map.emplace(idx, v);

    undo(new ChangeStyleValues(this, std::move(map)));
}

void Score::undo_change_style_values(std::unordered_map<Sid, PropertyValue> values) {
    undo(new ChangeStyleValues(this, std::move(values)));
}

void Score::undo_change_page_number_offset(int po) { undo(new ChangePageNumberOffset(this, po)); }

void Score::undo_change_parent(EngravingItem *element, EngravingItem *parent, staff_idx_t staff_idx, bool change_links_parents) {
    if (!element || !parent) {
        return;
    }

    if (element->ownership_parent() == parent && staff_idx == element->staff_idx()) {
        return;
    }

    Staff *dest_staff = staff(staff_idx);
    bool recreate_item_needed = false;

    const std::list<EngravingObject *> links = element->link_list();
    for (EngravingObject *obj : links) {
        EngravingItem *item = to_engraving_item(obj);
        Score *linked_score = item->score();
        Staff *linked_origin = item->staff();
        Staff *linked_dest = linked_score != this && dest_staff != element->staff() ?
                                 dest_staff->find_linked_in_score(linked_score) :
                                 linked_origin; // don't allow staff-change of linked elements within the same score
        if (!linked_dest && element->system_flag()) {
            linked_dest = linked_score->staff(0);
        }

        if (!linked_score || (!change_links_parents && item != element)) {
            continue;
        }
        if (item == element) {
            // Master score
            undo(new ChangeParent(element, parent, staff_idx));
        } else if (linked_origin && linked_dest) {
            // Part - origin and destination staves are in this part
            EngravingItem *linked_parent;
            if (parent->is_segment()) {
                // Get parent segment in linked score
                Segment *old_seg = to_segment(parent);
                Measure *old_meas = old_seg->measure();
                Measure *new_meas = linked_score->tick2measure(old_meas->tick());
                linked_parent = new_meas->tick2segment(old_seg->tick(), old_seg->segment_type());
                if (!linked_parent && old_seg->is_type(SegmentType::DURATION)) {
                    // A ChordRest segment that exists in the score may not exist in the part.
                    // In that case we create a TimeTick segment as new parent for the linked item.
                    linked_parent = new_meas->get_segment(SegmentType::TIME_TICK, old_seg->tick());
                }
            } else {
                linked_parent = parent->find_linked_in_staff(linked_dest);
            }
            IF_ASSERT_FAILED(linked_parent) { continue; }
            linked_score->undo(new ChangeParent(item, linked_parent, linked_score->staff_idx(linked_dest)));
        } else if (linked_origin && !linked_dest) {
            // Part - move is to a different staff
            // Remove original item
            linked_score->undo_remove_element(item, false);
            recreate_item_needed = true;
        }
    }

    if (recreate_item_needed) {
        // Need to create item in some parts
        const std::list<EngravingObject *> dest_staff_links = dest_staff->link_list();
        for (EngravingObject *obj : dest_staff_links) {
            Staff *linked_dest = to_staff(obj);
            Score *linked_score = linked_dest->score();
            if (linked_dest != dest_staff && !element->find_linked_in_score(linked_score)) {
                // Item should be in this score and isn't already
                EngravingItem *new_item = element->linked_clone();
                EngravingItem *linked_parent;

                if (parent->is_segment()) {
                    // Get parent segment in linked score
                    Segment *old_seg = to_segment(parent);
                    Measure *m = linked_score->tick2measure(old_seg->tick());
                    linked_parent = m->tick2segment(old_seg->tick(), old_seg->segment_type());
                } else {
                    linked_parent = parent->find_linked_in_score(linked_score);
                }

                new_item->set_ownership_parent(linked_parent);
                new_item->set_track(linked_score->staff_idx(linked_dest) * ::mu::engraving::voices);
                new_item->set_offset(PointF());
                linked_parent->undo_add_element(new_item, false);
            }
        }
    }
}

void Score::undo_reset_play_count_text_settings(BarLine *bl) {
    if (!bl || !bl->segment()) {
        return;
    }
    Segment *seg = bl->segment();

    for (staff_idx_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
        BarLine *cur_bl = to_bar_line(seg->element(staff2track(staff_idx)));

        if (!cur_bl) {
            continue;
        }

        cur_bl->undo_reset_property(Pid::PLAY_COUNT_TEXT_SETTING);
        cur_bl->undo_reset_property(Pid::PLAY_COUNT_TEXT);
    }
}

void Score::undo_update_play_count_text(Measure *m) {
    if (!m) {
        return;
    }
    const MStyle &style_value = style();
    const bool show_text = style_value.style_b(Sid::REPEAT_PLAY_COUNT_SHOW);
    const bool single_repeats = style_value.style_b(Sid::REPEAT_PLAY_COUNT_SHOW_SINGLE_REPEATS);
    const int play_count = m->repeat_count();
    const bool show_play_count = show_text && (play_count == 2 ? single_repeats : true) && m->repeat_end();

    Segment *end_bar_seg = m->last(SegmentType::BAR_LINE_TYPES);
    BarLine *top_bl = end_bar_seg ? to_bar_line(end_bar_seg->element(0)) : nullptr;
    if (!top_bl) {
        return;
    }

    PlayCountText *top_play_count_text = to_play_count_text(end_bar_seg->find_annotation(ElementType::PLAY_COUNT_TEXT, 0, 0));
    if (show_play_count) {
        if (!top_play_count_text) {
            top_play_count_text = Factory::create_play_count_text(end_bar_seg);
            top_play_count_text->set_track(0);
            top_play_count_text->set_selected(top_bl->selected());
            undo_add_element(top_play_count_text);
        }
    } else {
        undo_remove_element(top_play_count_text);
        return;
    }
}

Measure *Score::undo_get_measure(const Fraction &tick) {
    Measure *last = last_measure();
    if (!last || last->end_tick() <= tick) {
        Fraction last_tick = last ? last->end_tick() : Fraction(0, 1);
        while (tick >= last_tick) {
            MeasureBase *m = score()->insert_measure(ElementType::MEASURE);
            last_tick += m->ticks();
        }
    }
    return tick2measure(tick);
}

void Score::undo_change_bar_line_type(BarLine *bl, BarLineType bar_type, bool all_staves, bool replace) {
    if (bar_type == bl->bar_line_type()) {
        return;
    }

    Measure *m = bl->measure();
    if (!m) {
        return;
    }

    bool keep_start_repeat = false;

    if (bar_type == BarLineType::START_REPEAT && bl->segment()->segment_type() != SegmentType::BEGIN_BAR_LINE) {
        m = m->next_measure();
        if (!m) { // we were in last measure
            return;
        }
    } else if (bl->bar_line_type() == BarLineType::START_REPEAT) {
        if (m->system() && m->is_first_in_system()) {
            if (bar_type != BarLineType::END_REPEAT) {
                for (Score *lscore : m->score()->score_list()) {
                    Measure *lmeasure = lscore->tick2measure(m->tick());
                    if (lmeasure) {
                        lmeasure->undo_change_property(Pid::REPEAT_START, false);
                    }
                }
            }
            return;
        }

        m = m->prev_measure_mm();
        if (!m) {
            return;
        }

        bl = const_cast<BarLine *>(m->end_bar_line());
        if (!bl) {
            return;
        }

        if (bar_type == BarLineType::END_REPEAT) {
            keep_start_repeat = true;
        }
    }

    // when setting barline type on mmrest, set for underlying measure (and linked staves)
    // createMMRest will then set for the mmrest directly
    Measure *m2 = m->is_mm_rest() ? m->mm_rest_last() : m;

    switch (bar_type) {
        case BarLineType::END:
        case BarLineType::NORMAL:
        case BarLineType::DOUBLE:
        case BarLineType::BROKEN:
        case BarLineType::DOTTED:
        case BarLineType::REVERSE_END:
        case BarLineType::HEAVY:
        case BarLineType::DOUBLE_HEAVY: {
            if (m->next_measure_mm() && m->next_measure_mm()->system() && m->next_measure_mm()->is_first_in_system()) {
                keep_start_repeat = true;
            }

            Segment *segment = bl->segment();
            SegmentType segment_type = segment->segment_type();

            if (segment_type == SegmentType::END_BAR_LINE) {
                bool generated = false;
                if (bl->bar_line_type() == bar_type) {
                    generated = bl->generated(); // no change: keep current status
                } else if (!bl->generated() && (bar_type == BarLineType::NORMAL)) {
                    generated = true; // currently non-generated, changing to normal: assume generated
                }
                if (all_staves) {
                    // use all staves of master score; we will take care of parts in loop through linked staves below
                    Score *m_score = bl->master_score();
                    if (m_score->style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS)) {
                        m2 = m_score->tick2measure_mm(m2->tick());
                        if (!m2) {
                            return;
                        }
                        segment = m2->undo_get_segment(segment->segment_type(), segment->tick());
                        m2 = m2->is_mm_rest() ? m2->mm_rest_last() : m2;
                        if (!m2) {
                            return;
                        }
                    } else {
                        m2 = m_score->tick2measure(m2->tick());
                        if (!m2) {
                            return;
                        }
                        segment = m2->undo_get_segment(segment->segment_type(), segment->tick());
                    }
                }
                const std::vector<EngravingItem *> &elist = all_staves ? segment->elist() : std::vector<EngravingItem *>{bl};
                for (EngravingItem *e : elist) {
                    if (!e || !e->staff() || !e->is_bar_line()) {
                        continue;
                    }

                    // handle linked staves/parts:
                    // barlines themselves are not necessarily linked,
                    // so use staffList to find linked staves
                    BarLine *sbl = to_bar_line(e);
                    for (Staff *lstaff : sbl->staff()->staff_list()) {
                        Score *lscore = lstaff->score();
                        track_idx_t ltrack = lstaff->idx() * ::mu::engraving::voices;

                        // handle mmrests:
                        // set the barline on the underlying measure
                        // this will copied to the mmrest during layout, in createMMRest
                        Measure *lmeasure = lscore->tick2measure(m2->tick());
                        if (!lmeasure) {
                            continue;
                        }

                        lmeasure->undo_change_property(Pid::REPEAT_END, false);

                        Measure *next_measure = lmeasure->next_measure();
                        if (next_measure && !keep_start_repeat) {
                            next_measure->undo_change_property(Pid::REPEAT_START, false);
                        }
                        Segment *lsegment = lmeasure->undo_get_segment_r(SegmentType::END_BAR_LINE, lmeasure->ticks());
                        BarLine *lbl = to_bar_line(lsegment->element(ltrack));
                        if (!lbl) {
                            lbl = Factory::create_bar_line(lsegment);
                            lbl->set_track(ltrack);
                            lbl->set_span_staff(lstaff->bar_line_span());
                            lbl->set_span_from(lstaff->bar_line_from());
                            lbl->set_span_to(lstaff->bar_line_to());
                            lbl->set_bar_line_type(bar_type);
                            lbl->set_generated(generated);
                            lscore->add_element(lbl);
                            if (!generated) {
                                lbl->link_to(sbl);
                            }
                        } else {
                            lscore->undo(new ChangeProperty(lbl, Pid::GENERATED, generated, PropertyFlags::NOSTYLE));
                            lscore->undo(
                                new ChangeProperty(lbl, Pid::BARLINE_TYPE, PropertyValue::from_value(bar_type), PropertyFlags::NOSTYLE));
                            // set generated flag before and after so it sticks on type change and also works on undo/redo
                            lscore->undo(new ChangeProperty(lbl, Pid::GENERATED, generated, PropertyFlags::NOSTYLE));
                            if (lbl != sbl && !generated && !lbl->links()) {
                                lscore->undo(new Link(lbl, sbl));
                            } else if (lbl != sbl && generated && lbl->is_linked(sbl)) {
                                lscore->undo(new Unlink(lbl));
                                while (EngravingItem *linked_on_covering_mm_rest = sbl->find_linked_in_score(lscore)) {
                                    // Edge case: an additional link of this barline exists on the covering mmRest of lbl's measure
                                    lscore->undo(new Unlink(linked_on_covering_mm_rest));
                                    lscore->do_undo_remove_element(linked_on_covering_mm_rest);
                                }
                            }
                        }
                    }
                }
            } else if (segment_type == SegmentType::BEGIN_BAR_LINE) {
                for (Score *lscore : m2->score()->score_list()) {
                    Measure *lmeasure = lscore->tick2measure(m2->tick());
                    Segment *segment1 = lmeasure->undo_get_segment_r(SegmentType::BEGIN_BAR_LINE, Fraction(0, 1));
                    for (EngravingItem *e : segment1->elist()) {
                        if (e) {
                            lscore->undo(new ChangeProperty(e, Pid::GENERATED, false, PropertyFlags::NOSTYLE));
                            lscore->undo(
                                new ChangeProperty(e, Pid::BARLINE_TYPE, PropertyValue::from_value(bar_type), PropertyFlags::NOSTYLE));
                            // set generated flag before and after so it sticks on type change and also works on undo/redo
                            lscore->undo(new ChangeProperty(e, Pid::GENERATED, false, PropertyFlags::NOSTYLE));
                        }
                    }
                }
            }
        } break;
        case BarLineType::START_REPEAT: {
            for (size_t staff_idx = 0; staff_idx < m2->score()->nstaves(); ++staff_idx) {
                if (m2->is_measure_repeat_group_with_prev_m(staff_idx)) {
                    MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
                    return;
                }
            }
            for (Score *lscore : m2->score()->score_list()) {
                Measure *lmeasure = lscore->tick2measure(m2->tick());
                if (lmeasure) {
                    lmeasure->undo_change_property(Pid::REPEAT_START, true);
                    lmeasure = lmeasure->prev_measure();
                    if (lmeasure && replace) {
                        lmeasure->undo_change_property(Pid::REPEAT_END, false);
                    }
                }
            }
        } break;
        case BarLineType::END_REPEAT: {
            for (size_t staff_idx = 0; staff_idx < m2->score()->nstaves(); ++staff_idx) {
                if (m2->is_measure_repeat_group_with_next_m(staff_idx)) {
                    MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
                    return;
                }
            }
            for (Score *lscore : m2->score()->score_list()) {
                Measure *lmeasure = lscore->tick2measure(m2->tick());
                if (lmeasure) {
                    lmeasure->undo_change_property(Pid::REPEAT_END, true);
                    lmeasure = lmeasure->next_measure();
                    if (lmeasure && replace) {
                        lmeasure->undo_change_property(Pid::REPEAT_START, false);
                    }
                }
            }
        } break;
        case BarLineType::END_START_REPEAT: {
            for (size_t staff_idx = 0; staff_idx < m2->score()->nstaves(); ++staff_idx) {
                if (m2->is_measure_repeat_group_with_next_m(staff_idx)) {
                    MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
                    return;
                }
            }
            for (Score *lscore : m2->score()->score_list()) {
                Measure *lmeasure = lscore->tick2measure(m2->tick());
                if (lmeasure) {
                    lmeasure->undo_change_property(Pid::REPEAT_END, true);
                    lmeasure = lmeasure->next_measure();
                    if (lmeasure) {
                        lmeasure->undo_change_property(Pid::REPEAT_START, true);
                    }
                }
            }
        } break;
    }

    undo_update_play_count_text(m);
}

//---------------------------------------------------------
//   undoChangeElement
//---------------------------------------------------------

void Score::undo_change_element(EngravingItem *old_element, EngravingItem *new_element) {
    if (!old_element) {
        undo_add_element(new_element);
    } else {
        const std::list<EngravingObject *> links = old_element->link_list();
        for (EngravingObject *obj : links) {
            EngravingItem *item = to_engraving_item(obj);
            if (item == old_element) {
                undo(new ChangeElement(old_element, new_element));
            } else {
                if (item->score()) {
                    EngravingItem *new_clone = new_element->clone();
                    item->score()->undo(new ChangeElement(item, new_clone));
                }
            }
        }
    }
}

void Score::update_instrument_change_transpositions(KeySigEvent &key, Staff *staff, const Fraction &tick) {
    DO_ASSERT(staff->score() == this);
    if (!key.for_instrument_change()) {
        KeyList *kl = staff->key_list();
        int next_tick = kl->next_key_tick(tick.ticks());

        while (next_tick != -1) {
            KeySigEvent e = kl->key(next_tick);
            if (e.for_instrument_change()) {
                if (key.is_atonal() && !e.is_atonal()) {
                    e.set_mode(KeyMode::NONE);
                    e.set_concert_key(Key::C);
                } else {
                    e.set_mode(key.mode());
                    Interval transpose_interval = staff->part()->instrument(Fraction::from_ticks(next_tick))->transpose();
                    transpose_interval.flip();
                    Key ckey = key.concert_key();
                    Key nkey = Transpose::transpose_key(ckey, transpose_interval, staff->part()->prefer_sharp_flat());
                    e.set_concert_key(ckey);
                    e.set_key(nkey);
                }
                Measure *m = tick2measure(Fraction::from_ticks(next_tick));
                Segment *s = m ? m->tick2segment(Fraction::from_ticks(next_tick), SegmentType::KEY_SIG) : nullptr;
                track_idx_t track = staff->idx() * ::mu::engraving::voices;
                KeySig *key_sig = nullptr;
                EngravingItem *key_sig_elem = s ? s->element(track) : nullptr;
                if (key_sig_elem && key_sig_elem->is_key_sig()) {
                    key_sig = to_key_sig(key_sig_elem);
                }
                if (key_sig) {
                    undo(new ChangeKeySig(key_sig, e, key_sig->show_courtesy()));
                }
                next_tick = kl->next_key_tick(next_tick);
            } else {
                next_tick = -1;
            }
        }
    }
}

//---------------------------------------------------------
//   findLinkedChord
//---------------------------------------------------------

static Chord *find_linked_chord(Chord *c, Staff *nstaff) {
    Excerpt *se = c->score()->excerpt();
    Excerpt *de = nstaff->score()->excerpt();
    track_idx_t strack = c->track();

    if (se) {
        strack = muse::key(se->tracks_mapping(), strack);
    }
    track_idx_t dtrack = nstaff->idx() * ::mu::engraving::voices + strack % ::mu::engraving::voices;

    if (de) {
        std::vector<track_idx_t> l = muse::values(de->tracks_mapping(), strack);
        if (l.empty()) {
            // simply return the first linked chord whose staff is equal to nstaff
            for (EngravingObject *ee : c->link_list()) {
                Chord *ch = to_chord(ee);
                if (ch->staff() == nstaff) {
                    return ch;
                }
            }
            return nullptr;
        }
        for (track_idx_t i : l) {
            if (nstaff->idx() * ::mu::engraving::voices <= i && (nstaff->idx() + 1) * ::mu::engraving::voices > i) {
                dtrack = i;
                break;
            }
        }
    }

    Segment *s = c->segment();
    Measure *nm = nstaff->score()->tick2measure(s->tick());
    Segment *ns = nm->find_segment(s->segment_type(), s->tick());
    EngravingItem *ne = ns ? ns->element(dtrack) : nullptr;
    if (!ne || !ne->is_chord()) {
        return nullptr;
    }
    Chord *nc = to_chord(ne);
    if (c->is_grace()) {
        Chord *pc = to_chord(c->ownership_parent());
        size_t index = 0;
        for (Chord *gc : pc->grace_notes()) {
            if (c == gc) {
                break;
            }
            index++;
        }
        if (index < nc->grace_notes().size()) {
            nc = nc->grace_notes().at(index);
        }
    }
    return nc;
}

//---------------------------------------------------------
//   undoChangeChordRestLen
//---------------------------------------------------------

void Score::undo_change_chord_rest_len(ChordRest *cr, const TDuration &d) {
    cr->undo_change_property(Pid::DURATION_TYPE_WITH_DOTS, d.type_with_dots());
    cr->undo_change_property(Pid::DURATION, d.fraction());
}

//---------------------------------------------------------
//   undoRemovePart
//---------------------------------------------------------

void Score::undo_remove_part(Part *part, size_t part_idx) { undo(new RemovePart(part, part_idx)); }

//---------------------------------------------------------
//   undoInsertPart
//---------------------------------------------------------

void Score::undo_insert_part(Part *part, size_t target_part_index) { undo(new InsertPart(part, target_part_index)); }

//---------------------------------------------------------
//   undoRemoveStaff
//    idx - index of staff in part
//---------------------------------------------------------

void Score::undo_remove_staff(Staff *staff) {
    const staff_idx_t staff_index = staff->idx();
    assert(staff_index != muse::nidx);

    std::vector<Spanner *> spanners_to_remove;

    for (auto it = spanner_.cbegin(); it != spanner_.cend(); ++it) {
        Spanner *spanner = it->second;

        if (allow_remove_when_removing_staves(spanner, staff_index)) {
            spanners_to_remove.push_back(spanner);
        }
    }

    for (Spanner *spanner : unmanaged_spanner_) {
        if (allow_remove_when_removing_staves(spanner, staff_index)) {
            spanners_to_remove.push_back(spanner);
        }
    }

    for (Spanner *spanner : spanners_to_remove) {
        spanner->undo_unlink();
        do_undo_remove_element(spanner);
    }

    //
    //    adjust measures
    //
    for (Measure *m = staff->score()->first_measure(); m; m = m->next_measure()) {
        m->cmd_remove_staves(staff_index, staff_index + 1);
        if (m->has_mm_rest()) {
            m->mm_rest()->cmd_remove_staves(staff_index, staff_index + 1);
        }
    }

    staff->undo_unlink();

    undo(new RemoveStaff(staff));
}

//---------------------------------------------------------
//   undoInsertStaff
//    idx - index of staff in part
//---------------------------------------------------------

void Score::undo_insert_staff(Staff *staff, staff_idx_t ridx, bool create_rests) {
    undo(new InsertStaff(staff, ridx));
    staff_idx_t idx = staff_idx(staff->part()) + ridx;
    for (Measure *m = first_measure(); m; m = m->next_measure()) {
        m->cmd_add_staves(idx, idx + 1, create_rests);
        if (m->has_mm_rest()) {
            m->mm_rest()->cmd_add_staves(idx, idx + 1, false);
        }
    }
    // when newly adding an instrument,
    // this was already set when we created the staff
    // we don't have any better info at this point
    // and it doesn't work to adjust bracket & barlines until all staves are added
    // TODO: adjust brackets only when appropriate
    //adjustBracketsIns(idx, idx+1);
}

//---------------------------------------------------------
//   undoAddElement
//---------------------------------------------------------

void Score::undo_add_element(EngravingItem *element, bool add_to_linked_staves, bool ctrl_modifier, EngravingItem *element_to_relink) {
    Transaction &tx = transaction_manager()->current_or_dummy_transaction();

    Staff *ostaff = element->staff();
    track_idx_t strack = element->track();

    ElementType et = element->type();

    //
    // some elements are replicated for all parts regardless of
    // linking:
    //

    bool is_system_line = is_system_text_line(element);

    if ((et == ElementType::REHEARSAL_MARK) || (et == ElementType::SYSTEM_TEXT) || (et == ElementType::TRIPLET_FEEL) ||
        (et == ElementType::JUMP) || (et == ElementType::MARKER) || (et == ElementType::TEMPO_TEXT) ||
        (et == ElementType::PLAY_COUNT_TEXT) || is_system_line) {
        std::vector<Staff *> staff_list;

        if (!add_to_linked_staves) {
            staff_list.push_back(element->staff());
        } else if (ctrl_modifier && is_system_line) {
            staff_list = ostaff->staff_list();
            element->set_system_flag(false);
        } else {
            for (Score *s : score_list()) {
                staff_list.push_back(s->staff(0)); // system objects always appear on the top staff
                for (Staff *staff : s->system_object_staves()) {
                    IF_ASSERT_FAILED(staff->idx() != muse::nidx) { continue; }

                    staff_list.push_back(staff);
                }
            }
        }

        if (element_to_relink) {
            muse::remove(staff_list, ostaff);
        }

        bool original_added = false;
        for (Staff *staff : staff_list) {
            if (!staff) {
                continue;
            }

            Score *score = staff->score();
            staff_idx_t staff_idx = staff->idx();
            size_t ntrack = staff_idx * ::mu::engraving::voices;
            EngravingItem *ne;

            if (ostaff && staff->score() == ostaff->score() && staff == staff_list.front() && !original_added) {
                // add the element itself to the first system object staff in the score
                ne = element;
                original_added = true;
            } else {
                // add linked clones to the other staves
                ne = element->linked_clone();
                ne->set_score(score);
                ne->set_selected(false);
                ne->set_track(staff_idx * ::mu::engraving::voices + element->voice());
            }

            if (is_system_line) {
                Spanner *nsp = to_spanner(ne);
                Spanner *sp = to_spanner(element);
                long long diff = sp->track2() - sp->track();
                nsp->set_track2(nsp->track() + diff);
                nsp->compute_start_element();
                nsp->compute_end_element();
                do_undo_add_element(nsp);
            } else if (et == ElementType::MARKER || et == ElementType::JUMP) {
                Measure *om = to_measure(element->ownership_parent());
                Measure *m = score->tick2measure(om->tick());
                ne->set_track(ntrack);
                ne->set_ownership_parent(m);
                do_undo_add_element(ne);
            } else if (et == ElementType::MEASURE_NUMBER) {
                to_measure(element->ownership_parent())
                    ->undo_change_property(Pid::MEASURE_NUMBER_MODE, static_cast<int>(MeasureNumberMode::SHOW));
            } else if (et == ElementType::PLAY_COUNT_TEXT) {
                Segment *segment = to_segment(element->ownership_parent());
                Fraction tick = segment->tick();
                Measure *m = score->tick2measure(tick - Fraction::eps());
                Segment *seg = m->undo_get_segment(SegmentType::END_BAR_LINE, tick);
                ne->set_track(ntrack);
                ne->set_ownership_parent(seg);
                do_undo_add_element(ne);
            } else {
                Segment *segment = to_segment(element->ownership_parent());
                Fraction tick = segment->tick();
                Measure *m = score->tick2measure(tick);
                Segment *seg = m->undo_get_segment(SegmentType::CHORD_REST, tick);
                ne->set_track(ntrack);
                ne->set_ownership_parent(seg);
                do_undo_add_element(ne);
            }
        }

        return;
    }

    if (et == ElementType::FINGERING || (et == ElementType::IMAGE && !element->ownership_parent()->is_segment()) ||
        (et == ElementType::SYMBOL && !element->ownership_parent()->is_segment()) || et == ElementType::NOTE || et == ElementType::TEXT ||
        et == ElementType::GLISSANDO || et == ElementType::GUITAR_BEND || et == ElementType::NOTELINE || et == ElementType::BEND ||
        (et == ElementType::CHORD && to_chord(element)->is_grace()) || et == ElementType::LAISSEZ_VIB || et == ElementType::PARTIAL_TIE ||
        et == ElementType::PARENTHESIS) {
        const EngravingObject *parent = element->ownership_parent();
        const LinkedObjects *links = parent ? parent->links() : nullptr;

        // don't link part name
        if (et == ElementType::TEXT) {
            Text *t = to_text(element);
            if (t->text_style_type() == TextStyleType::INSTRUMENT_EXCERPT) {
                links = 0;
            }
        }
        if (links == 0 || !add_to_linked_staves) {
            do_undo_add_element(element);
            return;
        }
        for (EngravingObject *ee : *links) {
            EngravingItem *e = static_cast<EngravingItem *>(ee);
            EngravingItem *ne;
            if (e == parent) {
                ne = element;
            } else {
                if (element->is_glissando() || element->is_guitar_bend() ||
                    element->is_note_line()) { // and other spanners with Anchor::NOTE
                    Note *new_end = Spanner::end_element_from_spanner(to_spanner(element), e);
                    if (new_end) {
                        ne = element->linked_clone();
                        to_spanner(ne)->set_note_span(to_note(e), new_end);
                        if (element->is_guitar_bend() && element->staff_type()->is_tab_staff()) {
                            to_guitar_bend(ne)->end_note()->set_visible(true);
                        }
                    } else { //couldn't find suitable start note
                        continue;
                    }
                } else if (element->is_fingering()) {
                    bool tab_fingering = e->staff()->staff_type(e->tick())->show_tab_fingering();
                    if (e->staff()->is_tab_staff(e->tick()) && !tab_fingering) {
                        continue;
                    }
                    ne = element->linked_clone();
                } else {
                    ne = element->linked_clone();
                }
            }
            ne->set_score(e->score());
            ne->set_selected(false);
            ne->set_ownership_parent(e);
            do_undo_add_element(ne);
        }
        return;
    }

    if (et == ElementType::LAYOUT_BREAK) {
        LayoutBreak *lb = to_layout_break(element);
        if (lb->layout_break_type() == LayoutBreakType::SECTION) {
            do_undo_add_element(lb);
            MeasureBase *m = lb->measure();
            if (m->is_box()) {
                // for frames, use linked frames
                LinkedObjects *links = m->links();
                if (links) {
                    for (EngravingObject *lo : *links) {
                        if (lo->is_box()) {
                            Box *box = to_box(lo);
                            Score *score = box->score();
                            if (score != lb->score()) {
                                EngravingItem *e = lb->linked_clone();
                                e->set_score(score);
                                e->set_ownership_parent(box);
                                do_undo_add_element(e);
                            }
                        }
                    }
                    return;
                } else { // if thera are not linked frames, use previous measure
                    m = m->prev_measure();
                    if (!m) {
                        return;
                    }
                }
            }
            for (Score *s : score_list()) {
                if (s != lb->score()) {
                    EngravingItem *e = lb->linked_clone();
                    e->set_score(s);
                    Measure *nm = s->tick2measure(m->tick());
                    e->set_ownership_parent(nm);
                    do_undo_add_element(e);
                }
            }
            return;
        }
    }

    if (ostaff == 0 ||
        (et != ElementType::ARTICULATION && et != ElementType::ORNAMENT && et != ElementType::TAPPING && et != ElementType::CHORDLINE &&
         et != ElementType::LYRICS && et != ElementType::SLUR && et != ElementType::HAMMER_ON_PULL_OFF && et != ElementType::TIE &&
         et != ElementType::NOTE && et != ElementType::INSTRUMENT_CHANGE && et != ElementType::HAIRPIN && et != ElementType::OTTAVA &&
         et != ElementType::TRILL && et != ElementType::VIBRATO && et != ElementType::TEXTLINE && et != ElementType::PEDAL &&
         et != ElementType::LET_RING && et != ElementType::PALM_MUTE && et != ElementType::WHAMMY_BAR &&
         et != ElementType::PARTIAL_LYRICSLINE && et != ElementType::BREATH && et != ElementType::DYNAMIC &&
         et != ElementType::EXPRESSION && et != ElementType::STAFF_TEXT && et != ElementType::STAVE_SHARING_LABEL &&
         et != ElementType::SYSTEM_TEXT && et != ElementType::TRIPLET_FEEL && et != ElementType::PLAYTECH_ANNOTATION &&
         et != ElementType::CAPO && et != ElementType::STRING_TUNINGS && et != ElementType::STICKING &&
         et != ElementType::TREMOLO_SINGLECHORD && et != ElementType::TREMOLO_TWOCHORD && et != ElementType::ARPEGGIO &&
         et != ElementType::CHORD_BRACKET && et != ElementType::SYMBOL && et != ElementType::IMAGE && et != ElementType::TREMOLOBAR &&
         et != ElementType::FRET_DIAGRAM && et != ElementType::FERMATA && et != ElementType::HARMONY && et != ElementType::HARP_DIAGRAM &&
         et != ElementType::FIGURED_BASS && et != ElementType::CLEF && et != ElementType::AMBITUS && et != ElementType::PLAY_COUNT_TEXT)) {
        do_undo_add_element(element);
        return;
    }

    std::vector<Staff *> staves;
    if (add_to_linked_staves) {
        staves = ostaff->staff_list();
    } else {
        staves.push_back(ostaff);
    }

    if (element_to_relink) {
        muse::remove(staves, ostaff);
    }

    for (Staff *staff : staves) {
        Score *score = staff->score();
        staff_idx_t staff_idx = staff->idx();

        // Some elements in voice 1 of a staff should be copied to every track which has a linked voice in this staff
        static const std::set<ElementType> voic_e1_copy_types = {ElementType::SYMBOL,
                                                                 ElementType::IMAGE,
                                                                 ElementType::TREMOLOBAR,
                                                                 ElementType::DYNAMIC,
                                                                 ElementType::EXPRESSION,
                                                                 ElementType::STAFF_TEXT,
                                                                 ElementType::PLAYTECH_ANNOTATION,
                                                                 ElementType::CAPO,
                                                                 ElementType::STRING_TUNINGS,
                                                                 ElementType::STICKING,
                                                                 ElementType::FRET_DIAGRAM,
                                                                 ElementType::HARMONY,
                                                                 ElementType::HAIRPIN,
                                                                 ElementType::OTTAVA,
                                                                 ElementType::TRILL,
                                                                 ElementType::SLUR,
                                                                 ElementType::HAMMER_ON_PULL_OFF,
                                                                 ElementType::VIBRATO,
                                                                 ElementType::TEXTLINE,
                                                                 ElementType::PEDAL,
                                                                 ElementType::LYRICS,
                                                                 ElementType::PARTIAL_LYRICSLINE,
                                                                 ElementType::CLEF,
                                                                 ElementType::AMBITUS};

        track_idx_t linked_track = ostaff->get_linked_track_in_staff(staff, strack);
        if (linked_track == muse::nidx) {
            if (track2voice(strack) == 0 && muse::contains(voic_e1_copy_types, et)) {
                linked_track = staff2track(staff_idx);
            } else {
                continue;
            }
        }

        EngravingItem *ne;
        if (staff == ostaff) {
            ne = element;
        } else {
            if (staff->rstaff() != ostaff->rstaff()) {
                switch (element->type()) {
                    // exclude certain element types except on corresponding staff in part
                    // this should be same list excluded in cloneStaff()
                    case ElementType::STAFF_TEXT:
                    case ElementType::SYSTEM_TEXT:
                    case ElementType::TRIPLET_FEEL:
                    case ElementType::PLAYTECH_ANNOTATION:
                    case ElementType::CAPO:
                    case ElementType::STRING_TUNINGS:
                    case ElementType::FRET_DIAGRAM:
                    case ElementType::HARMONY:
                    case ElementType::FIGURED_BASS:
                    case ElementType::DYNAMIC:
                    case ElementType::EXPRESSION:
                    case ElementType::LYRICS: // not normally segment-attached
                    case ElementType::PARTIAL_LYRICSLINE:
                    case ElementType::PLAY_COUNT_TEXT:
                    case ElementType::WHAMMY_BAR: continue;
                    default: break;
                }
            }
            ne = element->linked_clone();
            ne->set_score(score);
            ne->set_selected(false);
            ne->set_track(linked_track);

            if (ne->is_fret_diagram()) {
                FretDiagram *fd = to_fret_diagram(ne);
                Harmony *fd_harmony = fd->harmony();
                if (fd_harmony) {
                    // TODO: this stuff should be done by overriding setScore setSelected and setTrack for FretDiagram [M.S.]
                    fd_harmony->set_score(score);
                    fd_harmony->set_selected(false);
                    fd_harmony->set_track(linked_track);
                }
            }
        }

        if (element->is_articulation_family()) {
            Articulation *a = to_articulation(element);
            Segment *segment;
            SegmentType st;
            Measure *m;
            Fraction tick;
            if (a->ownership_parent()->is_chord_rest()) {
                ChordRest *cr = a->chord_rest();
                segment = cr->segment();
                st = SegmentType::CHORD_REST;
                tick = segment->tick();
                m = score->tick2measure(tick);
            } else {
                segment = to_segment(a->ownership_parent()->ownership_parent());
                st = SegmentType::END_BAR_LINE;
                tick = segment->tick();
                m = score->tick2measure(tick);
                if (m->tick() == tick) {
                    m = m->prev_measure();
                }
            }
            Segment *seg = m->find_segment(st, tick);
            if (seg == 0) {
                LOGW("undoAddSegment: segment not found");
                break;
            }
            Articulation *na = to_articulation(ne);
            na->set_track(linked_track);
            if (a->ownership_parent()->is_chord_rest()) {
                ChordRest *cr = a->chord_rest();
                ChordRest *ncr;
                if (cr->is_grace()) {
                    ncr = find_linked_chord(to_chord(cr), score->staff(staff_idx));
                } else {
                    ncr = to_chord_rest(seg->element(linked_track));
                }
                na->set_ownership_parent(ncr);
            } else {
                BarLine *bl = to_bar_line(seg->element(linked_track));
                na->set_ownership_parent(bl);
            }
            do_undo_add_element(na);
        } else if (element->is_chord_line() || element->is_lyrics()) {
            ChordRest *cr = to_chord_rest(element->ownership_parent());
            Segment *segment = cr->segment();
            Fraction tick = segment->tick();
            Measure *m = score->tick2measure(tick);
            Segment *seg = m->find_segment(SegmentType::CHORD_REST, tick);
            if (seg == 0) {
                LOGW("undoAddSegment: segment not found");
                break;
            }
            ne->set_track(linked_track);
            ChordRest *ncr = to_chord_rest(seg->element(linked_track));
            ne->set_ownership_parent(ncr);
            if (element->is_chord_line()) {
                if (cr->is_grace()) {
                    ncr = find_linked_chord(to_chord(cr), score->staff(staff_idx));
                    ne->set_ownership_parent(ncr);
                }
                ChordLine *old_chord_line = to_chord_line(element);
                ChordLine *new_chord_line = to_chord_line(ne);
                // Chordline also needs to know the new note
                Note *new_note = to_chord(ncr)->find_note(old_chord_line->note()->pitch());
                new_chord_line->set_note(new_note);
            }
            do_undo_add_element(ne);
        } else if (element->is_harmony() && element->ownership_parent()->is_fret_diagram()) {
            EngravingItem *parent_fd = element->ownership_parent_item();
            if (parent_fd->score() != score) {
                // Find linked fret diagram
                EngravingItem *linked_fd = parent_fd->find_linked_in_score(score);
                ne->set_ownership_parent(linked_fd);
            }
            do_undo_add_element(ne);
        }
        //
        // elements with Segment as parent
        //
        else if (element->is_symbol() || element->is_image() || element->is_tremolo_bar() || element->is_dynamic() ||
                 element->is_expression() || element->is_staff_text() || element->is_stave_sharing_label() ||
                 element->is_play_tech_annotation() || element->is_capo() || element->is_string_tunings() || element->is_sticking() ||
                 element->is_fret_diagram() || element->is_fermata() ||
                 element->is_harmony() // Case with parent FretDiagram handled above, so we know it has parent Segment
                 || element->is_harp_pedal_diagram() || element->is_figured_bass() || element->is_clef() || element->is_ambitus()) {
            Segment *segment = to_segment(element->ownership_parent());
            Fraction tick = segment->tick();
            Measure *m = score->tick2measure(tick);
            bool add_clef_to_prev_measure = segment->is_type(SegmentType::CLEF) && element->is_clef() && !to_clef(element)->is_header();
            bool add_bl_to_prev_measure = segment->is_type(SegmentType::END_BAR_LINE);
            if (m->tick() == tick && (add_clef_to_prev_measure || add_bl_to_prev_measure) && m->prev_measure()) {
                m = m->prev_measure();
            }
            Segment *seg = m->undo_get_segment(segment->segment_type(), tick);
            ne->set_track(linked_track);
            ne->set_ownership_parent(seg);

            if (ne->is_string_tunings()) {
                StringTunings *string_tunings = to_string_tunings(ne);
                if (string_tunings->string_data()->is_null()) {
                    const StringData *string_data = string_tunings->origin_part()->string_data(tick, staff->idx());
                    int frets = string_data->frets();
                    std::vector<mu::engraving::InstrString> string_list = string_data->string_list();

                    string_tunings->set_string_data(StringData(frets, string_list));
                }
            }

            do_undo_add_element(ne);
            // transpose harmony if necessary
            if (element->is_harmony() && ne != element) {
                Harmony *h = to_harmony(ne);
                if (score->style().style_b(Sid::CONCERT_PITCH) != element->style().style_b(Sid::CONCERT_PITCH)) {
                    Staff *staff_dest = h->staff();
                    Interval interval = staff_dest->transpose(tick);
                    if (!interval.is_zero()) {
                        if (!score->style().style_b(Sid::CONCERT_PITCH)) {
                            interval.flip();
                        }
                        Transpose::undo_transpose_harmony(tx, h, interval);
                    }
                }
            }

            // Match vertical align settings of exiting items
            if (ne->is_harmony() || ne->is_fret_diagram()) {
                bool exclude = ne->exclude_vertical_align();
                for (EngravingItem *item : seg->annotations()) {
                    if ((!item->is_fret_diagram() && !item->is_harmony()) || item->staff_idx() != ne->staff_idx()) {
                        continue;
                    }

                    exclude = item->exclude_vertical_align();
                    break;
                }
                if (exclude != ne->exclude_vertical_align()) {
                    ne->undo_change_property(Pid::EXCLUDE_VERTICAL_ALIGN, exclude);
                }
            }
        } else if (element->is_slur() || element->is_hairpin() || element->is_ottava() || element->is_trill() || element->is_vibrato() ||
                   element->is_text_line() || element->is_pedal() || element->is_let_ring() || element->is_palm_mute() ||
                   element->is_whammy_bar() || element->is_partial_lyrics_line()) {
            Spanner *sp = to_spanner(element);
            Spanner *nsp = to_spanner(ne);
            track_idx_t tr2 = sp->effective_track2();
            int diff = static_cast<int>(tr2 - sp->track());
            nsp->set_track2(linked_track + diff);
            nsp->set_track(linked_track);

            // determine start/end element for slurs
            // this is only necessary if start/end element is
            //   a grace note, otherwise the element can be set to zero
            //   and will later be calculated from tick/track values
            //
            if (element->is_slur() && sp != nsp) {
                if (sp->start_element()) {
                    std::list<EngravingObject *> sel = sp->start_element()->link_list();
                    for (EngravingObject *ee : sel) {
                        EngravingItem *e = static_cast<EngravingItem *>(ee);
                        if (e->score() == nsp->score() && e->track() == nsp->track()) {
                            nsp->set_start_element(e);
                            break;
                        }
                    }
                }
                if (sp->end_element()) {
                    std::list<EngravingObject *> eel = sp->end_element()->link_list();
                    for (EngravingObject *ee : eel) {
                        EngravingItem *e = static_cast<EngravingItem *>(ee);
                        if (e->score() == nsp->score() && e->track() == nsp->track2()) {
                            nsp->set_end_element(e);
                            break;
                        }
                    }
                }
            }

            if (sp->is_text_line() && sp != nsp) {
                EngravingItem *parent = sp->ownership_parent_item();
                if (parent && parent->is_note()) {
                    nsp->set_ownership_parent(parent->find_linked_in_staff(staff));
                }
                EngravingItem *end_el = sp->end_element();
                if (end_el && end_el->is_note()) {
                    nsp->set_end_element(end_el->find_linked_in_staff(staff));
                }
            }

            do_undo_add_element(nsp);
        } else if (et == ElementType::GLISSANDO || et == ElementType::GUITAR_BEND || et == ElementType::NOTELINE) {
            do_undo_add_element(to_spanner(ne));
        } else if (element->is_type(ElementType::TREMOLO_TWOCHORD)) {
            TremoloTwoChord *tremolo = item_cast<TremoloTwoChord *>(element);
            ChordRest *cr1 = to_chord_rest(tremolo->chord1());
            ChordRest *cr2 = to_chord_rest(tremolo->chord2());
            int diff = static_cast<int>(cr2->track() - cr1->track());
            Segment *s1 = cr1->segment();
            Segment *s2 = cr2->segment();
            Measure *m1 = s1->measure();
            Measure *m2 = s2->measure();
            Measure *nm1 = score->tick2measure(m1->tick());
            Measure *nm2 = score->tick2measure(m2->tick());
            Segment *ns1 = nm1->find_segment(s1->segment_type(), s1->tick());
            Segment *ns2 = nm2->find_segment(s2->segment_type(), s2->tick());
            Chord *c1 = to_chord(ns1->element(linked_track));
            Chord *c2 = to_chord(ns2->element(linked_track + diff));
            TremoloTwoChord *ntremolo = item_cast<TremoloTwoChord *>(ne);
            ntremolo->set_chords(c1, c2);
            ntremolo->set_ownership_parent(c1);
            do_undo_add_element(ntremolo);
        } else if (element->is_type(ElementType::TREMOLO_SINGLECHORD)) {
            Chord *cr = to_chord(element->ownership_parent());
            Chord *c1 = find_linked_chord(cr, score->staff(staff_idx));
            ne->set_ownership_parent(c1);
            do_undo_add_element(ne);
        } else if (element->is_arpeggio() || element->is_chord_bracket()) {
            ChordRest *cr = to_chord_rest(element->ownership_parent());
            Segment *s = cr->segment();
            Measure *m = s->measure();
            Measure *nm = score->tick2measure(m->tick());
            Segment *ns = nm->find_segment(s->segment_type(), s->tick());
            Chord *c1 = to_chord(ns->element(linked_track));
            ne->set_ownership_parent(c1);
            do_undo_add_element(ne);
        } else if (element->is_tie()) {
            Tie *tie = to_tie(element);
            Note *n1 = tie->start_note();
            Note *n2 = tie->end_note();
            Chord *cr1 = n1->chord();
            Chord *cr2 = n2 ? n2->chord() : 0;

            // find corresponding notes in linked staff
            // accounting for grace notes and cross-staff notation
            int sm = 0;
            if (cr1->staff_idx() != cr2->staff_idx()) {
                sm = static_cast<int>(cr2->staff_idx() - cr1->staff_idx());
            }
            Chord *c1 = find_linked_chord(cr1, score->staff(staff_idx));
            Chord *c2 = find_linked_chord(cr2, score->staff(staff_idx + sm));

            IF_ASSERT_FAILED(c1) { return; }

            Note *nn1 = c1->find_note(n1->pitch(), n1->unison_index());
            Note *nn2 = c2 ? c2->find_note(n2->pitch(), n2->unison_index()) : 0;

            const track_idx_t track1 = c1->track();
            const track_idx_t track2 = c2 ? c2->track() : track1;

            // create tie
            Tie *ntie = to_tie(ne);
            ntie->erase_spanner_segments();
            ntie->set_track(track1);
            ntie->set_track2(track2);
            ntie->set_start_note(nn1);
            ntie->set_end_note(nn2);
            do_undo_add_element(ntie);
        } else if (element->is_instrument_change()) {
            InstrumentChange *is = to_instrument_change(element);
            Segment *s1 = is->segment();
            Measure *m1 = s1->measure();
            Measure *nm1 = score->tick2measure(m1->tick());
            Segment *ns1 = nm1->find_segment(s1->segment_type(), s1->tick());
            InstrumentChange *nis = to_instrument_change(ne);
            nis->set_ownership_parent(ns1);
            Fraction tick_start = nis->segment()->tick();
            Part *part = nis->part();
            Interval old_v = nis->staff()->transpose(tick_start);
            // ws: instrument should not be changed here
            if (is->instrument()->channel().empty() || is->instrument()->channel(0)->program() == -1) {
                nis->set_instrument(*staff->part()->instrument(s1->tick()));
            } else if (nis != is) {
                nis->set_instrument(*is->instrument());
            }
            do_undo_add_element(nis);
            // transpose root score; parts will follow
            if (score->is_master() && nis->staff()->transpose(tick_start) != old_v) {
                auto i = part->instruments().upper_bound(tick_start.ticks());
                Fraction tick_end = i == part->instruments().end() ? Fraction(-1, 1) : Fraction::from_ticks(i->first);
                Transpose::transposition_changed(tx, this, part, old_v, tick_start, tick_end);
            }
        } else if (element->is_breath()) {
            Breath *breath = to_breath(element);
            Fraction tick = breath->segment()->tick();
            Measure *m = score->tick2measure(tick);
            // breath appears before barline
            if (m->tick() == tick) {
                m = m->prev_measure();
            }
            Segment *seg = m->undo_get_segment(SegmentType::BREATH, tick);
            Breath *nbreath = to_breath(ne);
            nbreath->set_score(score);
            nbreath->set_track(linked_track);
            nbreath->set_ownership_parent(seg);
            do_undo_add_element(nbreath);
        } else {
            LOGW("undoAddElement: unhandled: <%s>", element->type_name());
        }
        ne->style_changed();

        if (element_to_relink) {
            LinkedObjects *links = ne->links();
            if (!links) {
                ne->link_to(element_to_relink);
            } else {
                element_to_relink->set_links(links);
                links->push_back(element_to_relink);
            }
        }
    }
}

FBox *Score::find_fret_box() const {
    for (MeasureBase *measure = first(); measure; measure = measure->next()) {
        if (measure->is_f_box()) {
            return to_f_box(measure);
        }
    }

    return nullptr;
}

void Score::rebuild_fret_box() {
    FBox *fret_box = find_fret_box();
    if (!fret_box) {
        return;
    }

    fret_box->set_needs_rebuild(true);
    fret_box->trigger_layout();
}

//---------------------------------------------------------
//   undoAddCR
//---------------------------------------------------------

void Score::undo_add_cr(ChordRest *cr, Measure *measure, const Fraction &tick) {
    assert(!cr->is_chord() || !(to_chord(cr)->notes()).empty());
    if (!cr->lyrics().empty()) {
        // Add chordrest and lyrics separately for correct
        // handling of adding lyrics to linked staves.
        std::vector<Lyrics *> lyrics;
        std::swap(lyrics, cr->lyrics());
        undo_add_cr(cr, measure, tick);
        for (Lyrics *l : lyrics) {
            undo_add_element(l);
        }
        return;
    }

    Staff *ostaff = cr->staff();
    track_idx_t strack = cr->track();

    SegmentType segment_type = SegmentType::CHORD_REST;

    for (const Staff *staff : ostaff->staff_list()) {
        track_idx_t linked_track = ostaff->get_linked_track_in_staff(staff, strack);

        if (linked_track == muse::nidx) {
            continue;
        }

        const TrackRange track_range = staff->part()->track_range();
        if (linked_track < track_range.start_track || linked_track >= track_range.end_track) {
            continue;
        }

        Score *score = staff->score();
        Measure *m = (score == this) ? measure : score->tick2measure(tick);
        if (!m) {
            LOGD("measure not found");
            break;
        }
        Segment *seg = m->undo_get_segment(segment_type, tick);

        assert(seg->segment_type() == segment_type);

        ChordRest *newcr = (staff == ostaff) ? cr : to_chord_rest(cr->linked_clone());
        newcr->set_score(score);

        newcr->set_track(linked_track);
        newcr->set_ownership_parent(seg);

        if (newcr->is_chord()) {
            Chord *chord = to_chord(newcr);
            // setTpcFromPitch needs to know the note tick position
            for (Note *note : chord->notes()) {
                // if (note->tpc() == Tpc::TPC_INVALID)
                //      note->setTpcFromPitch();
                assert(note->tpc() != Tpc::TPC_INVALID);
            }
        }
        // Climb up the (possibly nested) tuplets from this chordRest
        // Make sure all tuplets are cloned and correctly nested
        DurationElement *element_below = cr;
        Tuplet *tuplet_above = element_below->tuplet();
        while (tuplet_above) {
            DurationElement *linked_element_below = (DurationElement *) element_below->find_linked_in_staff(staff);
            if (!linked_element_below) { // shouldn't happen
                break;
            }
            Tuplet *linked_tuplet = (Tuplet *) tuplet_above->find_linked_in_staff(staff);
            if (!linked_tuplet) {
                linked_tuplet = to_tuplet(tuplet_above->linked_clone());
                linked_tuplet->set_score(score);
                linked_tuplet->set_track(newcr->track());
                linked_tuplet->set_ownership_parent(m);
            }
            linked_element_below->set_tuplet(linked_tuplet);

            element_below = tuplet_above;
            tuplet_above = tuplet_above->tuplet();
        }

        if (newcr->is_rest() && (to_rest(newcr)->is_gap()) && !(to_rest(newcr)->track() % ::mu::engraving::voices)) {
            to_rest(newcr)->set_gap(false);
        }

        do_undo_add_element(newcr);
    }
}

//---------------------------------------------------------
//   undoRemoveElement
//---------------------------------------------------------

void Score::undo_remove_element(EngravingItem *element, bool remove_linked) {
    if (!element) {
        return;
    }
    std::vector<Segment *> segments;
    for (EngravingObject *ee : element->link_list()) {
        EngravingItem *e = static_cast<EngravingItem *>(ee);
        if (e == element || remove_linked) {
            do_undo_remove_element(e);

            if (e->ownership_parent() && (e->ownership_parent()->is_segment())) {
                Segment *s = to_segment(e->ownership_parent());
                if (!muse::contains(segments, s)) {
                    segments.push_back(s);
                }
            }
            if (e->ownership_parent() && e->ownership_parent()->is_system()) {
                e->set_ownership_parent(0); // systems will be regenerated upon redo, so detach
            }
        }
    }

    if (element->is_measure_repeat()) {
        const MeasureRepeat *repeat = to_measure_repeat(element);
        Measure *measure = repeat->first_measure_of_group();
        size_t staff_idx = repeat->staff_idx();

        if (measure) {
            for (int i = 0; i < repeat->num_measures(); ++i) {
                undo_change_measure_repeat_count(measure, 0, staff_idx);
                measure = measure->next_measure();
            }
        }
    }

    for (Segment *s : segments) {
        if (s->empty()) {
            if (s->header() || s->trailer()) { // probably more segment types (system header)
                s->set_enabled(false);
            } else {
                do_undo_remove_element(s);
            }
        }
    }
}

void Score::undo_remove_hopo_text(HammerOnPullOffText *hopo_text) {
    Chord *start_chord = hopo_text->start_chord();
    Chord *end_chord = hopo_text->end_chord();
    IF_ASSERT_FAILED(start_chord && end_chord) { return; }

    HammerOnPullOffSegment *hopo_segment = hopo_text->hopo_segment();
    HammerOnPullOff *hopo = hopo_segment ? hopo_segment->hammer_on_pull_off() : nullptr;
    IF_ASSERT_FAILED(hopo) { return; }

    Chord *hopo_start_chord = to_chord(hopo->start_element());
    Chord *hopo_end_chord = to_chord(hopo->end_element());
    IF_ASSERT_FAILED(hopo_start_chord && hopo_end_chord) { return; }

    if (start_chord == hopo_start_chord && end_chord == hopo_end_chord) {
        undo_remove_element(hopo);
        return;
    }

    Fraction hopo_start_tick = hopo->tick();
    Fraction hopo_end_tick = hopo->tick2();
    Fraction hopo_text_start_tick = start_chord->tick();
    Fraction hopo_text_end_tick = end_chord->tick();

    bool shorten_from_start = (hopo_text_start_tick - hopo_start_tick) < (hopo_end_tick - hopo_text_end_tick);
    if (shorten_from_start) {
        Fraction new_start_tick = hopo_text_end_tick;
        Fraction new_ticks = hopo_end_tick - new_start_tick;
        hopo->undo_change_property(Pid::SPANNER_TICK, new_start_tick);
        hopo->undo_change_property(Pid::SPANNER_TICKS, new_ticks);
        hopo->undo_change_start_end_elements(end_chord, hopo_end_chord);
        if (start_chord != hopo_start_chord) {
            HammerOnPullOff *new_hopo = Factory::create_hammer_on_pull_off(score()->dummy());
            new_hopo->set_track(hopo->track());
            new_hopo->set_tick(hopo_start_tick);
            new_hopo->set_tick2(hopo_text_start_tick);
            new_hopo->set_start_element(hopo_start_chord);
            new_hopo->set_end_element(start_chord);
            score()->undo_add_element(new_hopo);
        }
    } else {
        Fraction new_end_tick = hopo_text_start_tick;
        Fraction new_ticks = new_end_tick - hopo_start_tick;
        hopo->undo_change_property(Pid::SPANNER_TICKS, new_ticks);
        hopo->undo_change_start_end_elements(hopo_start_chord, start_chord);
        if (end_chord != hopo_end_chord) {
            HammerOnPullOff *new_hopo = new HammerOnPullOff(score()->dummy());
            new_hopo->set_track(hopo->track());
            new_hopo->set_tick(hopo_text_end_tick);
            new_hopo->set_tick2(hopo_end_tick);
            new_hopo->set_start_element(end_chord);
            new_hopo->set_end_element(hopo_end_chord);
            score()->undo_add_element(new_hopo);
        }
    }
}

//---------------------------------------------------------
//   undoChangeSpannerElements
//---------------------------------------------------------

void Score::undo_change_spanner_elements(Spanner *spanner, EngravingItem *start_element, EngravingItem *end_element) {
    EngravingItem *old_start_element = spanner->start_element();
    EngravingItem *old_end_element = spanner->end_element();
    track_idx_t start_delta_track = start_element && old_start_element ? start_element->track() - old_start_element->track() : 0;
    track_idx_t end_delta_track = end_element && old_end_element ? end_element->track() - old_end_element->track() : 0;
    // scan all spanners linked to this one
    for (EngravingObject *el : spanner->link_list()) {
        Spanner *sp = to_spanner(el);
        EngravingItem *new_start_element = nullptr;
        EngravingItem *new_end_element = nullptr;
        // if not the current spanner, but one linked to it, determine its new start and end elements
        // as modifications 'parallel' to the modifications of the current spanner's start and end elements
        if (sp != spanner) {
            if (start_element) {
                // determine the track where to expect the 'parallel' start element
                track_idx_t new_track = sp->start_element() ? sp->start_element()->track() + start_delta_track : sp->track();
                // look in elements linked to new start element for an element with
                // same score as linked spanner and appropriate track
                for (EngravingObject *ee : start_element->link_list()) {
                    EngravingItem *e = to_engraving_item(ee);
                    if (e->score() == sp->score() && e->track() == new_track) {
                        new_start_element = e;
                        break;
                    }
                }
            }
            // similarly to determine the 'parallel' end element
            if (end_element) {
                track_idx_t new_track = sp->end_element() ? sp->end_element()->track() + end_delta_track : sp->track2();
                for (EngravingObject *ee : end_element->link_list()) {
                    EngravingItem *e = to_engraving_item(ee);
                    if (e->score() == sp->score() && e->track() == new_track) {
                        new_end_element = e;
                        break;
                    }
                }
            }
        }
        // if current spanner, just use stored start and end elements
        else {
            new_start_element = start_element;
            new_end_element = end_element;
        }
        sp->score()->undo(new ChangeSpannerElements(sp, new_start_element, new_end_element));

        if (sp->is_tie()) {
            to_tie(sp)->update_possible_jump_points();
        }
    }
}

//---------------------------------------------------------
//   undoInsertTime
//   acts on the linked scores as well
//---------------------------------------------------------

void Score::undo_insert_time(const Fraction &tick, const Fraction &len) {
    if (len.is_zero()) {
        return;
    }

    std::vector<Spanner *> spanners_to_adjust;
    for (const auto &i : spanner_.map()) {
        Spanner *s = i.second;
        if (s->tick2() < tick) {
            continue;
        }
        bool append = false;
        if (len > Fraction(0, 1)) {
            if (tick > s->tick() && tick < s->tick2()) {
                append = true;
            } else if (tick <= s->tick()) {
                append = true;
            }
        } else {
            Fraction tick2 = tick - len;
            if (s->tick() >= tick2) {
                append = true;
            } else if (s->tick() >= tick && s->tick2() <= tick2) {
                append = true;
            } else if ((s->tick() <= tick) && (s->tick2() >= tick2)) {
                Fraction t2 = s->tick2() + len;
                if (t2 > s->tick()) {
                    append = true;
                }
            } else if (s->tick() > tick && s->tick2() > tick2) {
                append = true;
            } else if (s->tick() < tick && s->tick2() < tick2) {
                append = true;
            }
        }
        for (const Spanner *ss : spanners_to_adjust) {
            if (muse::contains(ss->link_list(), static_cast<EngravingObject *>(s))) {
                append = false;
                break;
            }
        }
        if (append) {
            spanners_to_adjust.push_back(s);
        }
    }
    for (Spanner *s : spanners_to_adjust) {
        if (len > Fraction(0, 1)) {
            if (tick == s->tick() && s->is_volta()) {
                s->undo_change_property(Pid::SPANNER_TICKS, s->ticks() + len);
            } else if (tick > s->tick() && tick < s->tick2()) {
                //
                //  case a:
                //  +----spanner--------+
                //    +---add---
                //
                s->undo_change_property(Pid::SPANNER_TICKS, s->ticks() + len);
            } else if (tick <= s->tick()) {
                //
                //  case b:
                //       +----spanner--------
                //  +---add---
                // and
                //            +----spanner--------
                //  +---add---+
                EngravingItem *start_element = s->start_element();
                EngravingItem *end_element = s->end_element();
                undo_change_spanner_elements(s, nullptr, nullptr);
                s->undo_change_property(Pid::SPANNER_TICK, s->tick() + len);
                undo_change_spanner_elements(s, start_element, end_element);
            }
        } else {
            Fraction tick2 = tick - len;
            if (s->tick() >= tick2) {
                //
                //  case A:
                //  +----remove---+ +---spanner---+
                //
                Fraction t = s->tick() + len;
                if (t < Fraction(0, 1)) {
                    t = Fraction(0, 1);
                }
                EngravingItem *start_element = s->start_element();
                EngravingItem *end_element = s->end_element();
                undo_change_spanner_elements(s, nullptr, nullptr);
                s->undo_change_property(Pid::SPANNER_TICK, t);
                undo_change_spanner_elements(s, start_element, end_element);
            } else if (s->tick() >= tick && s->tick2() <= tick2) {
                //
                //  case B:
                //    +---spanner---+
                //  +----remove--------+
                //
                undo_remove_element(s);
            } else if ((s->tick() <= tick) && (s->tick2() >= tick2)) {
                //
                //  case C:
                //  +----spanner--------+
                //    +---remove---+
                //
                Fraction t2 = s->tick2() + len;
                if (t2 > s->tick()) {
                    s->undo_change_property(Pid::SPANNER_TICKS, s->ticks() + len);
                }
            } else if (s->tick() > tick && s->tick2() > tick2) {
                //
                //  case D:
                //       +----spanner--------+
                //  +---remove---+
                //
                Fraction d1 = s->tick() - tick;
                Fraction d2 = tick2 - s->tick();
                Fraction le = s->ticks() - d2;
                if (le.is_zero()) {
                    undo_remove_element(s);
                } else {
                    s->undo_change_property(Pid::SPANNER_TICK, s->tick() - d1);
                    s->undo_change_property(Pid::SPANNER_TICKS, le);
                }
            } else if (s->tick() < tick && s->tick2() < tick2) {
                //
                //  case E:
                //       +----spanner--------+
                //                     +---remove---+
                //
                Fraction d = s->tick2() - tick;
                Fraction le = s->ticks() - d;
                if (le.is_zero()) {
                    undo_remove_element(s);
                } else {
                    s->undo_change_property(Pid::SPANNER_TICKS, le);
                }
            }
        }
    }

    undo(new InsertTimeUnmanagedSpanner(this, tick, len));
}

//---------------------------------------------------------
//   undoRemoveMeasures
//---------------------------------------------------------

void Score::undo_remove_measures(Measure *m1, Measure *m2, bool preserve_ties, bool move_staff_type_changes) {
    assert(m1 && m2);

    Transaction &tx = transaction_manager()->current_or_dummy_transaction();

    const Fraction start_tick = m1->tick();
    const Fraction end_tick = m2->end_tick();
    std::set<Spanner *> spanners_to_remove;

    //
    //  handle ties which start before m1 and end in (m1-m2)
    //
    for (Segment *s = m1->first(); s != m2->last(); s = s->next1()) {
        IF_ASSERT_FAILED(s) {
            // This score is corrupted; handle it without crashing,
            // to help the user to fix corruptions by deleting the affected measures
            LOGE() << "Missing segments detected while deleting measures " << m1->measure_number() << " to " << m2->measure_number()
                   << ". This score (" << name() << ") is corrupted. Continuing without deleting measure contents.";
            break;
        }

        if (!s->is_chord_rest_type()) {
            continue;
        }

        // Make sure annotations are removed once, even if this segment contains linked copies of the same annotation
        // (the linked copy would be removed by undoRemoveElement)
        while (!s->annotations().empty()) {
            EngravingItem *annotation = s->annotations().front();
            IF_ASSERT_FAILED(annotation) { continue; }
            undo_remove_element(annotation);
        }

        for (track_idx_t track = 0; track < ntracks(); ++track) {
            EngravingItem *e = s->element(track);
            if (!e || !e->is_chord()) {
                continue;
            }
            Chord *c = to_chord(e);
            for (Note *n : c->notes()) {
                // Remove ties crossing measure range boundaries
                Tie *t = n->tie_back_non_partial();
                if (t && t->start_note() && (t->start_note()->chord()->tick() < start_tick)) {
                    if (preserve_ties) {
                        t->set_end_note(0);
                    } else {
                        undo_remove_element(t);
                    }
                }
                t = n->tie_for_non_partial();
                if (t && t->end_note() && (t->end_note()->chord()->tick() >= end_tick)) {
                    undo_remove_element(t);
                }

                // Do the same for other note-anchored spanners (e.g. glissandi).
                // Delay actual removing to avoid modifying lists inside loops over them.
                for (Spanner *sb : n->spanner_back()) {
                    if (sb->tick() < start_tick) {
                        spanners_to_remove.insert(sb);
                    }
                }
                for (Spanner *sf : n->spanner_for()) {
                    if (sf->tick2() >= end_tick) {
                        spanners_to_remove.insert(sf);
                    }
                }
            }
        }
    }

    for (Spanner *s : spanners_to_remove) {
        undo_remove_element(s);
    }

    // delete staffTypeChanges in removed measures
    if (move_staff_type_changes) {
        for (Measure *m = m1; m && m != m2->next_measure(); m = m->next_measure()) {
            for (size_t i = m->el().size(); i > 0; --i) {
                EngravingItem *el = m->el().at(i - 1);
                if (el && el->is_staff_type_change()) {
                    delete_item(el);
                }
            }
        }
    }

    EditSystemLocks::remove_system_locks_on_remove_measures(tx, this, m1, m2);
    EditPageLocks::remove_page_locks_on_remove_measures(tx, this, m1, m2);

    undo(new RemoveMeasures(m1, m2, move_staff_type_changes));
}

//---------------------------------------------------------
//   undoChangeMeasureRepeatCount
//---------------------------------------------------------

void Score::undo_change_measure_repeat_count(Measure *m, int new_count, staff_idx_t staff_idx) {
    for (Staff *st : staff(staff_idx)->staff_list()) {
        Score *linked_score = st->score();
        staff_idx_t linked_staff_idx = st->idx();
        Measure *linked_measure = linked_score->tick2measure(m->tick());

        int curr_count = linked_measure->measure_repeat_count(linked_staff_idx);
        if (curr_count != new_count) {
            linked_score->undo(new ChangeMeasureRepeatCount(linked_measure, new_count, linked_staff_idx));
        }
    }
}

void Score::do_undo_remove_stale_tie_jump_points(Tie *tie, bool undo) {
    auto remove_tie = [&](Tie *tie) {
        if (undo) {
            const size_t undo_idx = undo_stack()->current_index();
            start_cmd(TranslatableString("engraving", "Remove stale partial tie"));
            undo_remove_element(tie);
            end_cmd();
            if (undo_idx != undo_stack()->current_index() && undo_stack()->current_index() >= 2) {
                // These changes should be merged with the change in repeat structure which caused the ties to become invalid
                // currentIndex returns the next empty index for an undo command
                const size_t penultimate_cmd_idx = undo_stack()->current_index() - 2;
                undo_stack()->merge_transactions(penultimate_cmd_idx);
            }
        } else {
            remove_element(tie);
        }
    };

    std::vector<Tie *> old_ties;
    if (!tie->tie_jump_points()) {
        return;
    }
    for (TieJumpPoint *jump_point : *tie->tie_jump_points()) {
        if (jump_point->following_note()) {
            continue;
        }
        old_ties.push_back(jump_point->end_tie());
    }

    // Update jump points for linked ties
    for (EngravingObject *linked_tie : tie->link_list()) {
        if (!linked_tie || !linked_tie->is_tie()) {
            continue;
        }
        to_tie(linked_tie)->update_possible_jump_points();
    }

    for (Tie *old_tie : old_ties) {
        auto find_end_tie = [&old_tie](const TieJumpPoint *jump_point) { return jump_point->end_tie() == old_tie; };
        if (!old_tie) {
            continue;
        }

        if (std::find_if((*tie->tie_jump_points()).begin(), (*tie->tie_jump_points()).end(), find_end_tie) !=
            (*tie->tie_jump_points()).end()) {
            continue;
        }
        remove_tie(old_tie);
    }

    if (tie->is_partial_tie() && tie->all_jump_points_inactive() && tie->start_note() && tie->start_note()->tie_for() == tie) {
        remove_tie(tie);
    }
}

void Score::do_undo_reset_partial_slur(Slur *slur, bool undo) {
    const size_t undo_idx = undo_stack()->current_index();

    const ChordRest *start_cr = slur ? slur->start_cr() : nullptr;
    // Slurs can only have the same start & end element when they are partial.
    // If they are no longer partial, we should remove them
    const bool should_remove = slur->start_element() == slur->end_element();
    IF_ASSERT_FAILED(start_cr) { LOGE() << "Slur is corrupted"; }
    else if (!start_cr->has_preceding_jump_item() && slur->is_incoming()) {
        if (should_remove) {
            if (undo) {
                start_cmd(TranslatableString("engraving", "Remove invalid incoming partial slur"));
                undo_remove_element(slur);
                end_cmd();
            } else {
                remove_element(slur);
            }
        } else {
            if (undo) {
                start_cmd(TranslatableString("engraving", "Reset incoming partial slur"));
                slur->undo_set_incoming(false);
                end_cmd();
            } else {
                slur->set_incoming(false);
            }
        }
    }

    const ChordRest *end_cr = slur ? slur->end_cr() : nullptr;
    IF_ASSERT_FAILED(end_cr) { LOGE() << "Slur is corrupted"; }
    else if (!end_cr->has_following_jump_item() && slur->is_outgoing()) {
        if (should_remove) {
            if (undo) {
                start_cmd(TranslatableString("engraving", "Remove invalid outgoing partial slur"));
                undo_remove_element(slur);
                end_cmd();
            } else {
                remove_element(slur);
            }
        } else {
            if (undo) {
                start_cmd(TranslatableString("engraving", "Reset outgoing partial slur"));
                slur->undo_set_outgoing(false);
                end_cmd();
            } else {
                slur->set_outgoing(false);
            }
        }
    }

    if (!undo) {
        return;
    }

    if (undo_idx != undo_stack()->current_index() && undo_stack()->current_index() >= 2) {
        // These changes should be merged with the change in repeat structure which caused the ties to become invalid
        // currentIdx returns the next empty index for an undo command
        const size_t penultimate_cmd_idx = undo_stack()->current_index() - 2;
        undo_stack()->merge_transactions(penultimate_cmd_idx);
    }
}

void Score::undo_remove_stale_tie_jump_points(bool undo) {
    size_t tracks = nstaves() * ::mu::engraving::voices;
    Measure *m = first_measure();
    if (!m) {
        return;
    }

    // Copy, invalid slurs could be removed
    auto spanner_map = spanner();
    for (auto &interval : spanner_map) {
        Spanner *sp = interval.second;
        if (!sp || !sp->is_slur()) {
            continue;
        }
        do_undo_reset_partial_slur(to_slur(sp), undo);
    }

    std::set<PartialTie *> incoming_partial_ties;
    SegmentType st = SegmentType::CHORD_REST;
    for (Segment *s = m->first(st); s; s = s->next1(st)) {
        for (track_idx_t i = 0; i < tracks; ++i) {
            EngravingItem *e = s->element(i);
            if (e == 0 || !e->is_chord_rest()) {
                continue;
            }

            if (!to_chord_rest(e)->has_following_jump_item()) {
                // Remove invalid lyrics dashes
                for (Lyrics *lyrics : to_chord_rest(e)->lyrics()) {
                    LyricsLine *separator = lyrics->separator();
                    if ((lyrics->syllabic() == LyricsSyllabic::BEGIN || lyrics->syllabic() == LyricsSyllabic::MIDDLE) && separator &&
                        !separator->next_lyrics() && !separator->is_end_melisma()) {
                        lyrics->set_need_remove_invalid_segments();
                    }
                }
            }

            if (!e->is_chord()) {
                continue;
            }

            Chord *c = to_chord(e);
            for (Note *n : c->notes()) {
                if (n->incoming_partial_tie()) {
                    incoming_partial_ties.emplace(n->incoming_partial_tie());
                }
                if (!n->tie_for()) {
                    continue;
                }

                do_undo_remove_stale_tie_jump_points(n->tie_for());
            }
        }
    }

    for (PartialTie *incoming_pt : incoming_partial_ties) {
        if (incoming_pt->jump_point() || incoming_pt->note()->tie_back() != incoming_pt) {
            continue;
        }
        const size_t undo_idx = undo_stack()->current_index();
        if (undo) {
            start_cmd(TranslatableString("engraving", "Remove stale partial tie"));
            undo_remove_element(incoming_pt);
            end_cmd();
            if (undo_idx != undo_stack()->current_index() && undo_stack()->current_index() >= 2) {
                undo_stack()->merge_transactions(undo_stack()->current_index() - 2);
            }
        } else {
            remove_element(incoming_pt);
        }
    }
}
} // namespace mu::engraving
