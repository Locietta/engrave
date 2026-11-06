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

#include "navigation.h"

#include "../dom/box.h"
#include "../dom/chord.h"
#include "../dom/engravingitem.h"
#include "../dom/fret.h"
#include "../dom/guitarbend.h"
#include "../dom/hammeronpulloff.h"
#include "../dom/harmony.h"
#include "../dom/lyrics.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/mscore.h"
#include "../dom/note.h"
#include "../dom/pagelockindicator.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/soundflag.h"
#include "../dom/spanner.h"
#include "../dom/staff.h"
#include "../dom/system.h"
#include "../dom/systemlockindicator.h"
#include "../dom/tapping.h"
#include "../style/style.h"

using namespace mu;

namespace mu::engraving {
using ElementPair = std::pair<EngravingItem *, int>; // element and its index as a child

//---------------------------------------------------------
//   isChild1beforeChild2
//    compares if two children are order correctly
//    from top-left to bottom-right
//---------------------------------------------------------

static bool is_child1before_child2(const ElementPair &child1, const ElementPair &child2) {
    assert(child1.first->parent() == child2.first->parent());

    PointF p1 = child1.first->pos() + child1.first->ldata()->bbox().center();
    PointF p2 = child2.first->pos() + child2.first->ldata()->bbox().center();

    // If one child is *clearly* above the other then visit the higher one first
    double vertical_separation = p2.y() - p1.y();
    if (vertical_separation > 10) {
        return true;
    } else if (vertical_separation < -10) {
        return false;
    }

    // Children are roughly aligned vertically. Visit the one on the left first
    double horizontal_separation = p2.x() - p1.x();
    if (horizontal_separation > 0.0) {
        return true;
    } else if (horizontal_separation < 0.0) {
        return false;
    }

    // Children are aligned horizontally so check exact vertical position.
    if (vertical_separation > 0.0) {
        return true;
    } else if (vertical_separation < 0.0) {
        return false;
    }

    // Elements are at exact same position so fall back to using child index to determine order.
    if (child1.second < child2.second) {
        return true;
    }

    return false;
}

//---------------------------------------------------------
//   toChildPairsSet
//    return a set of children ordered
//    on the basis of position
//---------------------------------------------------------

static std::set<ElementPair, decltype(&is_child1before_child2)> to_child_pairs_set(const EngravingItem *element) {
    std::set<ElementPair, decltype(&is_child1before_child2)> children(&is_child1before_child2);
    int index = 0;

    for (EngravingObject *object : element->children()) {
        EngravingItem *engraving_item = to_engraving_item(object);

        if (!engraving_item) {
            continue;
        }

        children.insert(std::pair<EngravingItem *, int>(engraving_item, index));
        index++;
    }

    return children;
}

static EngravingItem *next_element_for_spanner_segment(const SpannerSegment *spanner_segment) {
    Spanner *spanner = spanner_segment->spanner();
    EngravingItem *el_st = spanner->start_element();
    IF_ASSERT_FAILED(el_st->is_note()) { return nullptr; }

    Note *note = to_note(el_st);
    return note->next_element();
}

static EngravingItem *prev_element_for_spanner_segment(const SpannerSegment *spanner_segment) {
    Spanner *spanner = spanner_segment->spanner();
    EngravingItem *el_st = spanner->start_element();
    IF_ASSERT_FAILED(el_st->is_note()) { return nullptr; }

    Note *note = to_note(el_st);
    return note->prev_element();
}

//---------------------------------------------------------
//   nextChordRest
//    return next Chord or Rest
//---------------------------------------------------------

ChordRest *Navigation::next_chord_rest(const ChordRest *cr, const ChordRestNavigateOptions &options) {
    if (!cr) {
        return nullptr;
    }

    if (cr->is_grace()) {
        const Chord *c = to_chord(cr);
        Chord *pc = to_chord(cr->ownership_parent());

        if (options.skip_grace) {
            cr = to_chord_rest(cr->ownership_parent());
        } else if (cr->is_grace_before()) {
            const GraceNotesGroup &group = pc->grace_notes_before();
            auto i = std::find(group.begin(), group.end(), c);
            if (i == group.end()) {
                return nullptr; // unable to find self?
            }
            ++i;
            if (i != group.end()) {
                return *i;
            }
            // if this was last grace note before, return parent
            return pc;
        } else {
            const GraceNotesGroup &group = pc->grace_notes_after();
            auto i = std::find(group.begin(), group.end(), c);
            if (i == group.end()) {
                return nullptr; // unable to find self?
            }
            ++i;
            if (i != group.end()) {
                return *i;
            }
            // if this was last grace note after, fall through to find next main note
            cr = pc;
        }
    } else { // cr is not a grace note
        if (cr->is_chord() && !options.skip_grace) {
            const Chord *c = to_chord(cr);
            if (!c->grace_notes().empty()) {
                const GraceNotesGroup &group = c->grace_notes_after();
                if (!group.empty()) {
                    return group.front();
                }
            }
        }
    }

    track_idx_t track = cr->track();
    SegmentType st = SegmentType::CHORD_REST;
    Segment *cur_seg = cr->segment();
    for (Segment *seg = cur_seg->next1_mm(st); seg; seg = seg->next1_mm(st)) {
        if (options.disable_over_repeats && !segments_are_adjacent(cur_seg, seg)) {
            return nullptr;
        }
        ChordRest *e = to_chord_rest(seg->element(track));
        if (e) {
            if (options.skip_measure_repeat_rests && e->is_rest() && e->measure()->is_measure_repeat_group(track2staff(track))) {
                continue; // these rests are not shown, skip them
            }
            if (e->is_chord() && !options.skip_grace) {
                Chord *c = to_chord(e);
                if (!c->grace_notes().empty()) {
                    const GraceNotesGroup &group = c->grace_notes_before();
                    if (!group.empty()) {
                        return group.front();
                    }
                }
            }
            return e;
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   prevChordRest
//    return previous Chord or Rest
//    if grace is true, include grace notes
//---------------------------------------------------------

ChordRest *Navigation::prev_chord_rest(const ChordRest *cr, const ChordRestNavigateOptions &options) {
    if (!cr) {
        return nullptr;
    }

    if (cr->is_grace()) {
        const Chord *c = to_chord(cr);
        Chord *pc = to_chord(cr->ownership_parent());

        if (options.skip_grace) {
            cr = to_chord_rest(cr->ownership_parent());
        } else if (cr->is_grace_before()) {
            const GraceNotesGroup &group = pc->grace_notes_before();
            auto i = std::find(group.begin(), group.end(), c);
            if (i == group.end()) {
                return nullptr; // unable to find self?
            }
            if (i != group.begin()) {
                return *--i;
            }
            // if this was first grace note before, fall through to find previous main note
            cr = pc;
        } else {
            const GraceNotesGroup &group = pc->grace_notes_after();
            auto i = std::find(group.begin(), group.end(), c);
            if (i == group.end()) {
                return nullptr; // unable to find self?
            }
            if (i != group.begin()) {
                return *--i;
            }
            // if this was first grace note after, return parent
            return pc;
        }
    } else {
        //
        // cr is not a grace note
        if (cr->is_chord() && !options.skip_grace) {
            const Chord *c = to_chord(cr);
            const GraceNotesGroup &group = c->grace_notes_before();
            if (!group.empty()) {
                return group.back();
            }
        }
    }

    track_idx_t track = cr->track();
    SegmentType st = SegmentType::CHORD_REST;
    Segment *cur_seg = cr->segment();
    for (Segment *seg = cr->segment()->prev1_mm(st); seg; seg = seg->prev1_mm(st)) {
        if (options.disable_over_repeats && !segments_are_adjacent(cur_seg, seg)) {
            return nullptr;
        }

        ChordRest *e = to_chord_rest(seg->element(track));
        if (e) {
            if (options.skip_measure_repeat_rests && e->is_rest() && e->measure()->is_measure_repeat_group(track2staff(track))) {
                continue; // these rests are not shown, skip them
            }
            if (e->is_chord() && !options.skip_grace) {
                const GraceNotesGroup &group = to_chord(e)->grace_notes_after();
                if (!group.empty()) {
                    return group.back();
                }
            }
            return e;
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   chordNoteAbove
//    element: Note, Rest, MMRest, or MeasureRepeat
//    return: Note, Rest, MMRest, or MeasureRepeat
//
//    return next higher pitched note in chord
//    move to previous track if at top of chord
//---------------------------------------------------------

EngravingItem *Navigation::chord_note_above(Score *score, EngravingItem *element) {
    EngravingItem *re = 0;
    if (element->is_rest_family()) {
        re = score->prev_track(to_rest(element));
    } else if (element->is_note()) {
        Note *note = to_note(element);
        Chord *chord = note->chord();
        const std::vector<Note *> &notes = chord->notes();
        auto i = std::find(notes.begin(), notes.end(), note);
        ++i;
        if (i != notes.end()) {
            re = *i;
        } else {
            re = score->prev_track(chord);
            if (re->track() == chord->track()) {
                re = element;
            }
        }
    }
    if (re == 0) {
        return 0;
    }
    if (re->is_chord()) {
        re = to_chord(re)->notes().front();
    }
    return re;
}

//---------------------------------------------------------
//   topNoteInChord
//---------------------------------------------------------

Note *Navigation::top_note_in_chord(Note *note) { return note->chord()->up_note(); }

//---------------------------------------------------------
//   chordNoteBelow
//    return next lower pitched note in chord
//    move to next track if at bottom of chord
//---------------------------------------------------------

EngravingItem *Navigation::chord_note_below(Score *score, EngravingItem *element) {
    EngravingItem *re = 0;
    if (element->is_rest_family()) {
        re = score->next_track(to_rest(element));
    } else if (element->is_note()) {
        Note *note = to_note(element);
        Chord *chord = note->chord();
        const std::vector<Note *> &notes = chord->notes();
        auto i = std::find(notes.begin(), notes.end(), note);
        if (i != notes.begin()) {
            --i;
            re = *i;
        } else {
            re = score->next_track(chord);
            if (re->track() == chord->track()) {
                re = element;
            }
        }
    }
    if (re == 0) {
        return 0;
    }
    if (re->is_chord()) {
        re = to_chord(re)->notes().back();
    }
    return re;
}

//---------------------------------------------------------
//   bottomNoteInChord
//---------------------------------------------------------

Note *Navigation::bottom_note_in_chord(Note *note) { return note->chord()->down_note(); }

//---------------------------------------------------------
//   firstElement
//---------------------------------------------------------

EngravingItem *Navigation::first_element(Score *score, bool frame) {
    if (frame) {
        MeasureBase *mb = score->measures()->first();
        if (mb && mb->is_box()) {
            return mb;
        }
    }
    Segment *s = score->first_segment_mm(SegmentType::ALL);
    return s ? s->element(0) : nullptr;
}

//---------------------------------------------------------
//   lastElement
//---------------------------------------------------------

EngravingItem *Navigation::last_element(Score *score, bool frame) {
    if (frame) {
        MeasureBase *mb = score->measures()->last();
        if (mb && mb->is_box()) {
            auto box_children = to_child_pairs_set(mb);
            if (!box_children.empty()) {
                return box_children.rbegin()->first;
            }
            return mb;
        }
    }
    EngravingItem *re = 0;
    Segment *seg = score->last_segment_mm();
    if (!seg) {
        return nullptr;
    }
    while (true) {
        for (size_t i = (score->staves().size() - 1) * ::mu::engraving::voices; i < score->staves().size() * ::mu::engraving::voices; i++) {
            if (seg->element(i)) {
                re = seg->element(i);
            }
        }
        if (re) {
            if (re->is_chord()) {
                return to_chord(re)->notes().front();
            }
            return re;
        }
        seg = seg->prev1_mm(SegmentType::ALL);
    }
}

//---------------------------------------------------------
//   upStaff
//---------------------------------------------------------

ChordRest *Navigation::up_staff(ChordRest *cr) {
    Segment *segment = cr->segment();

    if (cr->staff_idx() == 0) {
        return cr;
    }

    for (int track = (static_cast<int>(cr->staff_idx()) - 1) * ::mu::engraving::voices; track >= 0; --track) {
        EngravingItem *el = segment->element(track);
        if (!el) {
            continue;
        }
        if (el->is_note()) {
            el = to_note(el)->chord();
        }
        if (el->is_chord_rest()) {
            return to_chord_rest(el);
        }
    }
    return 0;
}

//---------------------------------------------------------
//   downStaff
//---------------------------------------------------------

ChordRest *Navigation::down_staff(Score *score, ChordRest *cr) {
    Segment *segment = cr->segment();
    track_idx_t tracks = score->nstaves() * ::mu::engraving::voices;

    if (cr->staff_idx() == score->nstaves() - 1) {
        return cr;
    }

    for (track_idx_t track = (cr->staff_idx() + 1) * ::mu::engraving::voices; track < tracks; --track) {
        EngravingItem *el = segment->element(track);
        if (!el) {
            continue;
        }
        if (el->is_note()) {
            el = to_note(el)->chord();
        }
        if (el->is_chord_rest()) {
            return to_chord_rest(el);
        }
    }
    return 0;
}

//---------------------------------------------------------
//   topStaff
//---------------------------------------------------------

ChordRest *Navigation::top_staff(Score *score, ChordRest *cr) {
    // Go to top-most staff of current or first measure depending upon active selection
    const auto *destination_measure = cr ? cr->measure() : score->first_measure();
    if (destination_measure) {
        // Accommodate for MMRest
        if (score->style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && destination_measure->has_mm_rest()) {
            destination_measure = destination_measure->covering_mm_rest_or_this();
        }
        // Get first ChordRest of top staff
        cr = destination_measure->first()->next_chord_rest(0, false);
    }
    return cr;
}

//---------------------------------------------------------
//   nextTrack
//    returns note at or just before current (cr) position
//    in next track for this measure
//    that contains such an element
//---------------------------------------------------------

ChordRest *Score::next_track(ChordRest *cr, bool skip_measure_repeat_rests) {
    if (!cr) {
        return 0;
    }

    ChordRest *el = nullptr;
    Measure *measure = cr->measure();
    track_idx_t track = cr->track();
    size_t tracks = nstaves() * ::mu::engraving::voices;

    while (!el) {
        // find next non-empty track
        while (++track < tracks) {
            if (measure->has_voice(track)) {
                break;
            }
        }
        // no more tracks, return original element
        if (track == tracks) {
            return cr;
        }
        // treat MeasureRepeat as if it starts in the first measure of its group even if internally it's farther along
        if (cr->is_measure_repeat()) {
            measure = measure->first_of_measure_repeat_group(cr->staff_idx());
            return to_chord_rest(measure->first(SegmentType::CHORD_REST)->element(track));
        }
        // find element at same or previous segment within this track
        for (Segment *segment = cr->segment(); segment; segment = segment->prev(SegmentType::CHORD_REST)) {
            el = to_chord_rest(segment->element(track));
            if (el) {
                break;
            }
        }
    }
    if (skip_measure_repeat_rests && el->is_rest() && measure->is_measure_repeat_group(track2staff(track))) {
        el = measure->measure_repeat_element(track2staff(track));
    }
    return el;
}

//---------------------------------------------------------
//   prevTrack
//    returns ChordRest at or just before current (cr) position
//    in previous track for this measure
//    that contains such an element
//---------------------------------------------------------

ChordRest *Score::prev_track(ChordRest *cr, bool skip_measure_repeat_rests) {
    if (!cr) {
        return 0;
    }

    ChordRest *el = nullptr;
    Measure *measure = cr->measure();
    int track = static_cast<int>(cr->track());

    while (!el) {
        // find next non-empty track
        while (--track >= 0) {
            if (measure->has_voice(track)) {
                break;
            }
        }
        // no more tracks, return original element
        if (track < 0) {
            return cr;
        }
        // treat MeasureRepeat as if it starts in the first measure of its group even if internally it's farther along
        if (cr->is_measure_repeat()) {
            measure = measure->first_of_measure_repeat_group(cr->staff_idx());
            return to_chord_rest(measure->first(SegmentType::CHORD_REST)->element(track));
        }
        // find element at same or previous segment within this track
        for (Segment *segment = cr->segment(); segment; segment = segment->prev(SegmentType::CHORD_REST)) {
            el = to_chord_rest(segment->element(track));
            if (el) {
                break;
            }
        }
    }
    if (skip_measure_repeat_rests && el->is_rest() && measure->is_measure_repeat_group(track2staff(track))) {
        el = measure->measure_repeat_element(track2staff(track));
    }
    return el;
}

//---------------------------------------------------------
//   nextMeasure
//---------------------------------------------------------

ChordRest *Navigation::next_measure(Score *score, ChordRest *element, bool select_behavior, bool mm_rest) {
    if (!element) {
        return nullptr;
    }

    Measure *measure = nullptr;
    if (mm_rest) {
        measure = element->measure()->next_measure_mm();
    } else {
        measure = element->measure()->next_measure();
    }

    Fraction end_tick = element->measure()->last()->next_chord_rest(element->track(), true)->tick();
    bool last = false;

    if (score->selection().is_range()) {
        if (element->tick() != end_tick && score->selection().tick_end() <= end_tick) {
            measure = element->measure();
            last = true;
        } else if (element->tick() == end_tick && score->selection().is_end_active()) {
            last = true;
        }
    } else if (element->tick() != end_tick && select_behavior) {
        measure = element->measure();
        last = true;
    }
    if (!measure) {
        measure = element->measure();
        last = true;
    }
    staff_idx_t staff = element->staff_idx();

    Segment *start_seg = last ? measure->last() : measure->first();
    for (Segment *seg = start_seg; seg; seg = last ? seg->prev() : seg->next()) {
        track_idx_t etrack = (staff + 1) * ::mu::engraving::voices;
        for (track_idx_t track = staff * ::mu::engraving::voices; track < etrack; ++track) {
            EngravingItem *pel = seg->element(track);

            if (pel && pel->is_chord_rest()) {
                return to_chord_rest(pel);
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   prevMeasure
//---------------------------------------------------------

ChordRest *Navigation::prev_measure(Score *score, ChordRest *element, bool mm_rest) {
    if (!element) {
        return nullptr;
    }

    Measure *measure = nullptr;
    if (mm_rest) {
        measure = element->measure()->prev_measure_mm();
    } else {
        measure = element->measure()->prev_measure();
    }

    Fraction start_tick = element->measure()->first()->next_chord_rest(element->track())->tick();
    bool last = false;

    if (score->selection().is_range() && score->selection().is_end_active() && score->selection().start_segment()->tick() <= start_tick) {
        last = true;
    } else if (element->tick() != start_tick) {
        measure = element->measure();
    }
    if (!measure) {
        measure = element->measure();
        last = false;
    }

    staff_idx_t staff = element->staff_idx();

    Segment *start_seg = last ? measure->last() : measure->first();
    for (Segment *seg = start_seg; seg; seg = last ? seg->prev() : seg->next()) {
        track_idx_t etrack = (staff + 1) * ::mu::engraving::voices;
        for (track_idx_t track = staff * ::mu::engraving::voices; track < etrack; ++track) {
            EngravingItem *pel = seg->element(track);

            if (pel && pel->is_chord_rest()) {
                return to_chord_rest(pel);
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   nextElement
//---------------------------------------------------------

EngravingItem *Score::next_element() {
    EngravingItem *e = get_selected_element();
    if (!e) {
        return nullptr;
    }
    staff_idx_t staff_id = e->staff_idx();
    while (e) {
        switch (e->type()) {
            case ElementType::NOTE:
            case ElementType::REST:
            case ElementType::MMREST:
            case ElementType::CHORD:
            case ElementType::TUPLET: {
                EngravingItem *next = e->next_element();
                if (next) {
                    return next;
                } else {
                    break;
                }
            }
            case ElementType::SEGMENT: {
                Segment *s = to_segment(e);
                EngravingItem *next = s->next_element(staff_id);
                if (next) {
                    return next;
                } else {
                    break;
                }
            }
            case ElementType::MEASURE: {
                Measure *m = to_measure(e);
                EngravingItem *next = m->next_element_staff(staff_id);
                if (next) {
                    return next;
                } else {
                    break;
                }
            }
            case ElementType::CLEF:
            case ElementType::KEYSIG:
            case ElementType::TIMESIG:
            case ElementType::BAR_LINE: {
                for (; e && !e->is_segment(); e = e->layout_parent()) {}
                Segment *s = to_segment(e);
                EngravingItem *next = s->next_element(staff_id);
                if (next) {
                    return next;
                } else {
                    return Navigation::first_element(score());
                }
            }
            case ElementType::TAPPING: {
                TappingHalfSlur *half_slur_above = to_tapping(e)->half_slur_above();
                EngravingItem *sel_el = get_selected_element();
                if (half_slur_above && !(sel_el && sel_el->is_tapping_half_slur_segment())) {
                    return half_slur_above->front_segment();
                }
                break;
            }
            case ElementType::HAMMER_ON_PULL_OFF_TEXT: return to_hammer_on_pull_off_text(e)->end_chord()->up_note();
            case ElementType::HAMMER_ON_PULL_OFF_SEGMENT: {
                HammerOnPullOffSegment *hopo_seg = to_hammer_on_pull_off_segment(e);
                if (!hopo_seg->hopo_text().empty()) {
                    return hopo_seg->hopo_text().front();
                }
                [[fallthrough]];
            }
            case ElementType::TAPPING_HALF_SLUR_SEGMENT: {
                TappingHalfSlur *half_slur = to_tapping_half_slur_segment(e)->tapping_half_slur();
                Tapping *tapping = half_slur->tapping();
                if (half_slur->is_half_slur_above() && tapping->half_slur_below()) {
                    return tapping->half_slur_below()->front_segment();
                }
                [[fallthrough]];
            }
            case ElementType::VOLTA_SEGMENT:
            case ElementType::SLUR_SEGMENT:
            case ElementType::TEXTLINE_SEGMENT:
            case ElementType::HAIRPIN_SEGMENT:
            case ElementType::OTTAVA_SEGMENT:
            case ElementType::TRILL_SEGMENT:
            case ElementType::VIBRATO_SEGMENT:
            case ElementType::LET_RING_SEGMENT:
            case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
            case ElementType::PALM_MUTE_SEGMENT:
            case ElementType::WHAMMY_BAR_SEGMENT:
            case ElementType::RASGUEADO_SEGMENT:
            case ElementType::HARMONIC_MARK_SEGMENT:
            case ElementType::PICK_SCRAPE_SEGMENT:
            case ElementType::PEDAL_SEGMENT: {
                SpannerSegment *s = to_spanner_segment(e);
                Spanner *sp = s->spanner();
                Spanner *next_sp = sp->next_spanner(sp, staff_id);
                if (next_sp) {
                    return next_sp->spanner_segments().front();
                }

                Segment *seg = tick2segment(sp->tick());
                if (seg) {
                    Segment *next_segment = seg->next1();
                    while (next_segment) {
                        if (next_segment->is_time_tick_type()) {
                            if (EngravingItem *annotation = next_segment->first_annotation(staff_id)) {
                                return annotation;
                            }
                            if (Spanner *spanner = next_segment->first_spanner(staff_id)) {
                                return spanner->spanner_segments().front();
                            }
                        } else if (EngravingItem *next_el = next_segment->first_element_of_segment(staff_id)) {
                            return next_el;
                        }
                        next_segment = next_segment->next1_mm();
                    }
                }
                break;
            }
            case ElementType::GUITAR_BEND_SEGMENT:
            case ElementType::GUITAR_BEND_HOLD_SEGMENT: {
                GuitarBend *bend = e->is_guitar_bend_segment() ? to_guitar_bend_segment(e)->guitar_bend() :
                                                                 to_guitar_bend_hold_segment(e)->guitar_bend_hold()->guitar_bend();
                if (bend->bend_type() != GuitarBendType::SLIGHT_BEND) {
                    return bend->end_note();
                } else {
                    EngravingItem *next = next_element_for_spanner_segment(to_spanner_segment(e));
                    if (next) {
                        return next;
                    }
                }

                break;
            }
            case ElementType::GLISSANDO_SEGMENT:
            case ElementType::NOTELINE_SEGMENT:
            case ElementType::LAISSEZ_VIB_SEGMENT:
            case ElementType::PARTIAL_TIE_SEGMENT:
            case ElementType::TIE_SEGMENT: {
                EngravingItem *next = next_element_for_spanner_segment(to_spanner_segment(e));
                if (next) {
                    return next;
                } else {
                    break;
                }
            }
            case ElementType::VBOX:
            case ElementType::HBOX:
            case ElementType::TBOX:
            case ElementType::FBOX: {
                auto box_children = to_child_pairs_set(e);

                EngravingItem *selected_element = get_selected_element();

                if ((selected_element->is_v_box() || selected_element->is_h_box() || selected_element->is_t_box()) &&
                    !box_children.empty()) {
                    return box_children.begin()->first;
                }

                if (selected_element->is_f_box()) {
                    for (EngravingItem *child : to_f_box(selected_element)->el()) {
                        if (child->is_fret_diagram() && child->visible()) {
                            return to_fret_diagram(child)->harmony();
                        }
                    }
                }

                for (auto child = box_children.begin(); child != box_children.end(); child++) {
                    if (selected_element != child->first) {
                        continue;
                    }

                    auto target_element = std::next(child);

                    if (target_element != box_children.end()) {
                        return target_element->first;
                    }
                }

                MeasureBase *mb = to_measure_base(e)->next_mm();
                if (!mb) {
                    break;
                } else if (mb->is_measure()) {
                    ChordRest *cr = selection().current_cr();
                    staff_idx_t si = cr ? cr->staff_idx() : 0;
                    return to_measure(mb)->next_element_staff(si);
                } else {
                    return mb;
                }
            }
            case ElementType::LAYOUT_BREAK: {
                staff_id = 0; // otherwise it will equal -1, which breaks the navigation
                break;
            }
            // TODO: case ElementType::STAFF_VISIBILITY_INDICATOR:
            case ElementType::SYSTEM_LOCK_INDICATOR: {
                staff_id = 0;
                SystemLockIndicator *sli = to_system_lock_indicator(e);
                System *system = sli->system();
                if (system && system->page_lock_indicator()) {
                    return system->page_lock_indicator();
                } else {
                    e = to_system_lock_indicator(e)->system_lock()->end_mb();
                }
                continue;
            }
            case ElementType::PAGE_LOCK_INDICATOR: {
                staff_id = 0;
                e = to_page_lock_indicator(e)->page_lock()->end_mb();
                continue;
            }
            case ElementType::SOUND_FLAG:
                if (EngravingItem *parent = to_sound_flag(e)->ownership_parent_item()) {
                    return parent;
                }
                break;
            case ElementType::HARMONY: {
                if (EngravingItem *parent = to_harmony(e)->ownership_parent_item()) {
                    if (parent->is_fret_diagram()) {
                        return parent;
                    }
                }
                break;
            }
            case ElementType::FRET_DIAGRAM: {
                FretDiagram *fret_diagram = to_fret_diagram(e);
                if (fret_diagram->is_in_fret_box()) {
                    const ElementList &diagrams = to_f_box(fret_diagram->ownership_parent())->el();

                    size_t index = muse::index_of(diagrams, fret_diagram);
                    if (index != muse::nidx) {
                        while (++index < diagrams.size()) {
                            FretDiagram *fret_diagram_i = to_fret_diagram(diagrams[index]);
                            if (fret_diagram_i->visible()) {
                                return fret_diagram_i->harmony();
                            }
                        }
                    }
                }
                break;
            }
            default: break;
        }
        e = e->layout_parent();
    }
    return Navigation::last_element(score());
}

//---------------------------------------------------------
//   prevElement
//---------------------------------------------------------

EngravingItem *Score::prev_element() {
    EngravingItem *e = get_selected_element();
    if (!e) {
        return nullptr;
    }
    staff_idx_t staff_id = e->staff_idx();
    while (e) {
        switch (e->type()) {
            case ElementType::NOTE:
            case ElementType::REST:
            case ElementType::MMREST:
            case ElementType::CHORD:
            case ElementType::TUPLET: {
                EngravingItem *prev = e->prev_element();
                if (prev) {
                    return prev;
                } else {
                    break;
                }
            }
            case ElementType::SEGMENT: {
                Segment *s = to_segment(e);
                EngravingItem *prev = s->prev_element(staff_id);
                if (prev) {
                    return prev;
                } else {
                    break;
                }
            }
            case ElementType::MEASURE: {
                Measure *m = to_measure(e);
                return m->prev_element_staff(staff_id);
            }
            case ElementType::CLEF:
            case ElementType::KEYSIG:
            case ElementType::TIMESIG:
            case ElementType::BAR_LINE: {
                for (; e && !e->is_segment(); e = e->layout_parent()) {}
                EngravingItem *previous_element = to_segment(e)->prev_element(staff_id);

                if (!previous_element->is_box()) {
                    return previous_element;
                }

                if (previous_element->is_fret_diagram()) {
                    return previous_element;
                }

                auto box_children = to_child_pairs_set(previous_element);

                if (box_children.size() > 0) {
                    return box_children.rbegin()->first;
                }

                return previous_element;
            }
            case ElementType::HAMMER_ON_PULL_OFF_TEXT: {
                HammerOnPullOffText *hopo_text = to_hammer_on_pull_off_text(e);
                HammerOnPullOffSegment *hopo_segment = to_hammer_on_pull_off_segment(hopo_text->parent());
                DO_ASSERT(hopo_segment);
                if (hopo_segment->hopo_text().size() > 0 && hopo_segment->hopo_text().front() == hopo_text) {
                    return hopo_segment;
                } else {
                    return hopo_text->start_chord()->down_note();
                }
            }
            case ElementType::TAPPING_HALF_SLUR_SEGMENT: {
                TappingHalfSlur *half_slur = to_tapping_half_slur_segment(e)->tapping_half_slur();
                Tapping *tapping = half_slur->tapping();
                if (!half_slur->is_half_slur_above()) {
                    IF_ASSERT_FAILED(tapping->half_slur_above()) { return tapping; }
                    return tapping->half_slur_above()->front_segment();
                } else {
                    return tapping;
                }
            }
            case ElementType::VOLTA_SEGMENT:
            case ElementType::SLUR_SEGMENT:
            case ElementType::HAMMER_ON_PULL_OFF_SEGMENT:
            case ElementType::TEXTLINE_SEGMENT:
            case ElementType::HAIRPIN_SEGMENT:
            case ElementType::OTTAVA_SEGMENT:
            case ElementType::TRILL_SEGMENT:
            case ElementType::VIBRATO_SEGMENT:
            case ElementType::PEDAL_SEGMENT: {
                SpannerSegment *s = to_spanner_segment(e);
                Spanner *sp = s->spanner();
                EngravingItem *st_el = sp->start_element();
                Spanner *prev_sp = sp->prev_spanner(sp, staff_id);
                if (prev_sp) {
                    return prev_sp->spanner_segments().front();
                } else {
                    Segment *start_seg = sp->start_segment();
                    if (EngravingItem *annotation = start_seg->last_annotation(staff_id)) {
                        return annotation;
                    }
                    if (start_seg->is_time_tick_type()) {
                        start_seg = start_seg->prev1_m_menabled();
                        for (; start_seg && start_seg->is_time_tick_type(); start_seg = start_seg->prev1_m_menabled()) {
                            if (Spanner *spanner = start_seg->last_spanner(staff_id)) {
                                return spanner->spanner_segments().front();
                            }
                            if (EngravingItem *annotation = start_seg->last_annotation(staff_id)) {
                                return annotation;
                            }
                        }
                        if (!start_seg) {
                            break;
                        }
                        // Also check first non-timeTick segment encountered.
                        if (Spanner *spanner = start_seg->last_spanner(staff_id)) {
                            return spanner->spanner_segments().front();
                        }
                        if (EngravingItem *annotation = start_seg->last_annotation(staff_id)) {
                            return annotation;
                        }
                    }
                    EngravingItem *el = start_seg->last_element_of_segment(staff_id);
                    if (st_el->is_chord_rest() || st_el->is_note()) {
                        ChordRest *cr = start_seg->cr(st_el->track());
                        if (cr) {
                            EngravingItem *el_cr = cr->last_element_before_segment();
                            if (el_cr) {
                                return el_cr;
                            }
                        }
                    }
                    if (el->is_chord()) {
                        return to_chord(el)->last_element_before_segment();
                    } else if (el->is_note()) {
                        Chord *c = to_note(el)->chord();
                        return c->last_element_before_segment();
                    } else {
                        return el;
                    }
                }
            }
            case ElementType::GUITAR_BEND_SEGMENT:
            case ElementType::GUITAR_BEND_HOLD_SEGMENT: {
                GuitarBend *bend = e->is_guitar_bend_segment() ? to_guitar_bend_segment(e)->guitar_bend() :
                                                                 to_guitar_bend_hold_segment(e)->guitar_bend_hold()->guitar_bend();
                return bend->start_note();
            }
            case ElementType::GLISSANDO_SEGMENT:
            case ElementType::NOTELINE_SEGMENT:
            case ElementType::LAISSEZ_VIB_SEGMENT:
            case ElementType::PARTIAL_TIE_SEGMENT:
            case ElementType::TIE_SEGMENT: {
                EngravingItem *prev = prev_element_for_spanner_segment(to_spanner_segment(e));
                if (prev) {
                    return prev;
                }

                break;
            }
            case ElementType::VBOX:
            case ElementType::HBOX:
            case ElementType::TBOX:
            case ElementType::FBOX: {
                auto box_children = to_child_pairs_set(e);

                EngravingItem *selected_element = get_selected_element();

                for (auto child = box_children.rbegin(); child != box_children.rend(); child++) {
                    if (selected_element != child->first) {
                        continue;
                    }

                    auto target_element = std::next(child);

                    if (target_element != box_children.rend()) {
                        return target_element->first;
                    }
                }

                MeasureBase *mb = to_measure_base(e)->prev_mm();
                if (!mb) {
                    break;
                } else if (mb->is_measure()) {
                    ChordRest *cr = selection().current_cr();
                    staff_idx_t si = cr ? cr->staff_idx() : 0;
                    Segment *s = to_measure(mb)->last();
                    if (s) {
                        return s->last_element_for_navigation(si);
                    }
                } else {
                    return mb;
                }
            } break;
            case ElementType::LAYOUT_BREAK: {
                staff_id = 0; // otherwise it will equal -1, which breaks the navigation
                break;
            }
            // TODO: case ElementType::STAFF_VISIBILITY_INDICATOR:
            case ElementType::SYSTEM_LOCK_INDICATOR: {
                staff_id = 0;
                e = to_system_lock_indicator(e)->system_lock()->end_mb();
                continue;
            }
            case ElementType::PAGE_LOCK_INDICATOR: {
                staff_id = 0;
                PageLockIndicator *pli = to_page_lock_indicator(e);
                System *system = pli->system();
                if (system && !system->system_lock_indicators().empty()) {
                    return system->system_lock_indicators().front();
                } else {
                    e = to_page_lock_indicator(e)->page_lock()->end_mb();
                }
                continue;
            }
            case ElementType::HARMONY: {
                Harmony *harmony = to_harmony(e);
                if (harmony->is_in_fret_box()) {
                    FretDiagram *fret_diagram = to_fret_diagram(harmony->ownership_parent());
                    FBox *fret_box = to_f_box(fret_diagram->ownership_parent());
                    const ElementList &diagrams = fret_box->el();

                    size_t index = muse::index_of(diagrams, fret_diagram);
                    while (--index != muse::nidx) {
                        FretDiagram *fret_diagram_i = to_fret_diagram(diagrams[index]);
                        if (fret_diagram_i->visible()) {
                            return fret_diagram_i;
                        }
                    }

                    return fret_box;
                } else if (harmony->ownership_parent()->is_fret_diagram()) {
                    EngravingItem *prev = harmony->get_parent_seg()->prev_annotation(to_fret_diagram(harmony->ownership_parent()));
                    if (prev) {
                        return prev;
                    }

                    e = to_fret_diagram(harmony->ownership_parent());
                }
                break;
            }
            case ElementType::FRET_DIAGRAM: {
                FretDiagram *fret_diagram = to_fret_diagram(e);
                EngravingItem *harmony = fret_diagram->harmony();
                if (harmony) {
                    return harmony;
                }
                break;
            }
            default: break;
        }
        e = e->layout_parent();
    }
    return Navigation::first_element(score());
}

//---------------------------------------------------------
//   prevLyrics
//    - find the lyric (if any) before this one (not including lines)
//    - currently used to determine the first lyric of a melisma
//---------------------------------------------------------

Lyrics *Navigation::last_lyrics_in_measure(const Segment *seg, const staff_idx_t staff_idx, const int no, const PlacementV &placement) {
    while (seg) {
        const track_idx_t strack = staff_idx * ::mu::engraving::voices;
        const track_idx_t etrack = strack + ::mu::engraving::voices;
        for (track_idx_t track = strack; track < etrack; ++track) {
            EngravingItem *el = seg->element(track);
            Lyrics *prev_lyrics = el && el->is_chord() ? to_chord_rest(el)->lyrics(no, placement) : nullptr;
            if (prev_lyrics) {
                return prev_lyrics;
            }
        }
        seg = seg->prev1(mu::engraving::SegmentType::CHORD_REST);
    }
    return nullptr;
}

Lyrics *Navigation::prev_lyrics(const Lyrics *lyrics) {
    Segment *seg = lyrics->ownership_parent() ? lyrics->segment() : nullptr;
    if (!seg) {
        return nullptr;
    }
    while ((seg = seg->prev1(mu::engraving::SegmentType::CHORD_REST))) {
        const track_idx_t strack = lyrics->staff_idx() * ::mu::engraving::voices;
        const track_idx_t etrack = strack + ::mu::engraving::voices;
        for (track_idx_t track = strack; track < etrack; ++track) {
            EngravingItem *el = seg->element(track);
            Lyrics *prev_lyrics = el && el->is_chord() ? to_chord_rest(el)->lyrics(lyrics->verse(), lyrics->placement()) : nullptr;
            if (prev_lyrics) {
                return prev_lyrics;
            }
        }
    }
    return nullptr;
}

Lyrics *Navigation::next_lyrics(const Lyrics *lyrics) {
    Segment *seg = lyrics->ownership_parent() ? lyrics->segment() : nullptr;
    if (!seg) {
        return nullptr;
    }
    Segment *next_segment = seg;
    while ((next_segment = next_segment->next1(mu::engraving::SegmentType::CHORD_REST))) {
        const track_idx_t strack = lyrics->staff_idx() * ::mu::engraving::voices;
        const track_idx_t etrack = strack + ::mu::engraving::voices;
        for (track_idx_t track = strack; track < etrack; ++track) {
            EngravingItem *el = next_segment->element(track);
            Lyrics *next_lyrics = el && el->is_chord() ? to_chord_rest(el)->lyrics(lyrics->verse(), lyrics->placement()) : nullptr;
            if (next_lyrics) {
                return next_lyrics;
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   move
//    move current selection
//---------------------------------------------------------

//---------------------------------------------------------
//   selectMove
//---------------------------------------------------------

} // namespace mu::engraving
