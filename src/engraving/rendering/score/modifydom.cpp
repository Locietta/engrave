/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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
#include "modifydom.h"

#include "dom/chord.h"
#include "dom/hook.h"
#include "dom/keysig.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/ornament.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/spannermap.h"
#include "dom/staff.h"
#include "dom/timesig.h"
#include "dom/trill.h"
#include "dom/utils.h"
#include "editing/addremoveelement.h"
#include "rendering/score/chordlayout.h"

using namespace mu::engraving::rendering::score;

void ModifyDom::set_cross_measure(const Measure *measure, LayoutContext &ctx) {
    bool cross_measure = ctx.conf().style_b(Sid::CROSS_MEASURE_VALUES);
    const DomAccessor &dom = ctx.dom();
    for (staff_idx_t staff_idx = 0; staff_idx < dom.nstaves(); ++staff_idx) {
        const Staff *staff = dom.staff(staff_idx);
        if (!staff->show()) {
            continue;
        }

        track_idx_t start_track = staff_idx * ::mu::engraving::voices;
        track_idx_t end_track = start_track + ::mu::engraving::voices;

        for (const Segment &segment : measure->segments()) {
            if (!segment.is_just_type(SegmentType::CHORD_REST)) {
                continue;
            }

            for (track_idx_t t = start_track; t < end_track; ++t) {
                ChordRest *cr = segment.cr(t);
                if (!cr) {
                    continue;
                }
                if (cr->is_chord()) {
                    Chord *chord = to_chord(cr);
                    if (!chord->is_grace()) {
                        ChordLayout::cross_measure_setup(chord, cross_measure, ctx);
                    }
                }
            }
        }
    }
}

void ModifyDom::connect_tremolo(Measure *m) { m->connect_tremolo(); }

void ModifyDom::cmd_update_notes(const Measure *measure, const DomAccessor &dom) {
    for (staff_idx_t staff_idx = 0; staff_idx < dom.nstaves(); ++staff_idx) {
        const Staff *staff = dom.staff(staff_idx);
        if (!staff->show()) {
            continue;
        }

        AccidentalState as; // list of already set accidentals for this measure
        // initAccidentalState
        {
            as.init(staff->key_sig_event(measure->tick()));

            // Trills may carry an accidental into this measure that requires a force-restate
            int ticks = measure->tick().ticks();
            auto spanners = dom.spanner_map().find_overlapping(ticks, ticks, true);
            for (auto iter : spanners) {
                Spanner *spanner = iter.value;
                if (spanner->staff_idx() != staff_idx || !spanner->is_trill() || spanner->tick() == measure->tick() ||
                    spanner->tick2() == measure->tick()) {
                    continue;
                }
                Ornament *ornament = to_trill(spanner)->ornament();
                Note *trill_note = ornament ? ornament->note_above() : nullptr;
                if (trill_note && trill_note->accidental() && ornament->show_accidental() == OrnamentShowAccidental::DEFAULT) {
                    int line = abs_step(trill_note->tpc(), trill_note->epitch());
                    as.set_force_restate_accidental(line, true);
                }
            }
        }

        const TrackRange track_range = staff->part()->track_range();
        track_idx_t main_track = staff_idx * ::mu::engraving::voices;

        for (const Segment &segment : measure->segments()) {
            if (segment.is_just_type(SegmentType::KEY_SIG)) {
                KeySig *ks = item_cast<KeySig *>(segment.element(main_track));
                if (ks) {
                    Fraction tick = segment.tick();
                    as.init(staff->key_sig_event(tick));
                }
            } else if (segment.is_just_type(SegmentType::CHORD_REST)) {
                for (track_idx_t t = track_range.start_track; t < track_range.end_track; ++t) {
                    Chord *chord = item_cast<Chord *>(segment.element(t), CastMode::MAYBE_BAD); // maybe Rest
                    if (chord) {
                        chord->cmd_update_notes(&as, staff_idx);
                    }
                }
            }
        }
    }
}

void ModifyDom::create_stems(const Measure *measure, LayoutContext &ctx) {
    const DomAccessor &dom = ctx.dom();
    for (staff_idx_t staff_idx = 0; staff_idx < dom.nstaves(); ++staff_idx) {
        const Staff *staff = dom.staff(staff_idx);
        if (!staff->show()) {
            continue;
        }

        track_idx_t start_track = staff_idx * ::mu::engraving::voices;
        track_idx_t end_track = start_track + ::mu::engraving::voices;

        for (const Segment &segment : measure->segments()) {
            if (!segment.is_just_type(SegmentType::CHORD_REST)) {
                continue;
            }

            for (track_idx_t t = start_track; t < end_track; ++t) {
                ChordRest *cr = segment.cr(t);
                if (!cr) {
                    continue;
                }

                auto create_stems = [](Chord *chord) {
                    if (!chord->should_have_stem()) {
                        chord->remove_stem();
                        return;
                    }

                    if (!chord->stem()) {
                        chord->create_stem();
                    }
                };

                if (cr->is_chord()) {
                    Chord *chord = to_chord(cr);

                    for (Chord *c : chord->grace_notes()) {
                        create_stems(c);
                    }

                    create_stems(chord); // create stems needed to calculate spacing
                    // stem direction can change later during beam processing
                }
            }
        }
    }
}

void ModifyDom::set_track_for_chord_grace_notes(Measure *measure, const DomAccessor &dom) {
    for (staff_idx_t staff_idx = 0; staff_idx < dom.nstaves(); ++staff_idx) {
        track_idx_t start_track = staff_idx * ::mu::engraving::voices;
        track_idx_t end_track = start_track + ::mu::engraving::voices;

        for (const Segment &segment : measure->segments()) {
            if (!segment.is_just_type(SegmentType::CHORD_REST)) {
                continue;
            }

            for (track_idx_t t = start_track; t < end_track; ++t) {
                ChordRest *cr = segment.cr(t);
                if (!cr) {
                    continue;
                }

                if (cr->is_chord()) {
                    Chord *chord = to_chord(cr);
                    for (Chord *c : chord->grace_notes()) {
                        c->set_track(t);
                    }
                }
            }
        }
    }
}

void ModifyDom::sort_measure_segments(Measure *measure, LayoutContext &ctx) {
    measure = measure->covering_mm_rest_or_this();

    // Move segments between measure which need to move

    // Move key and time signature segments to the correct measure.
    // Depending on configuration, this could mean moving segments at the end of this measure to the start of the following measure
    // or moving segments from the start of the following measure to the end of this measure

    // Loop through clef, key, time sigs in this bar.  If any are found and shouldn't be in this bar, move to the next

    auto change_applies_to_repeat_and_continuation = [&](const Segment &seg) -> bool {
        // Check if the change applies to the beginning of the repeat section as well as the continuation
        const std::vector<Measure *> measures = find_following_repeat_measures(measure);
        for (const Measure *repeat_measure : measures) {
            if (repeat_measure == measure->next_measure_mm()) {
                continue;
            }
            const Fraction start_tick = repeat_measure->tick();
            for (track_idx_t track = 0; track < ctx.dom().nstaves(); track += ::mu::engraving::voices) {
                const Staff *staff = ctx.dom().staff(track2staff(track));

                if (seg.is_key_sig_type()) {
                    const Key continuation_key = staff->key(seg.tick());
                    const Key repeat_key = staff->key(start_tick);
                    const Key key_before_continuation = staff->key(seg.tick() - Fraction::eps());
                    return key_before_continuation != continuation_key && continuation_key == repeat_key;
                }

                if (seg.is_time_sig_type()) {
                    const TimeSig *continuation_time_sig = staff->time_sig(seg.tick());
                    const TimeSig *repeat_time_sig = staff->time_sig(start_tick);
                    const TimeSig *time_sig_before_continuation = staff->time_sig(seg.tick() - Fraction::eps());
                    return continuation_time_sig && repeat_time_sig && time_sig_before_continuation &&
                           time_sig_before_continuation->sig() != continuation_time_sig->sig() &&
                           continuation_time_sig->sig() == repeat_time_sig->sig();
                }

                if (seg.is_clef_type()) {
                    const ClefType continuation_clef = staff->clef(seg.tick());
                    const ClefType repeat_clef = staff->clef(start_tick);
                    const ClefType clef_before_continuation = staff->clef(seg.tick() - Fraction::eps());
                    if (clef_before_continuation != continuation_clef && continuation_clef == repeat_clef) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    auto clef_seg_barline_position = [&](const Segment &seg) -> ClefToBarlinePosition {
        for (EngravingItem *el : seg.elist()) {
            if (!el || !el->is_clef()) {
                continue;
            }
            const Clef *clef = to_clef(el);
            if (clef->clef_to_barline_position() != ClefToBarlinePosition::AUTO) {
                return clef->clef_to_barline_position();
            }
        }
        return ClefToBarlinePosition::AUTO;
    };

    MeasureBase *next_mb = measure ? measure->next_mm() : nullptr;

    if (!next_mb || !next_mb->is_measure()) {
        return;
    }

    Measure *next_measure = to_measure(next_mb);

    const bool sigs_should_be_in_this_measure = ((measure->repeat_end() && ctx.conf().style_b(Sid::CHANGES_BEFORE_BARLINE_REPEATS)) ||
                                                 (measure->repeat_jump() && ctx.conf().style_b(Sid::CHANGES_BEFORE_BARLINE_OTHER_JUMPS)));
    std::vector<Segment *> segs_to_remove;

    std::vector<Segment *> segs_to_move_to_next_measure;
    for (Segment &seg : measure->segments()) {
        if (seg.tick() != measure->end_tick() || seg.is_chord_rest_type()) {
            continue;
        }

        // Move clefs at the end of this measure into the next measure
        if (seg.is_clef_type()) {
            ClefToBarlinePosition pos = clef_seg_barline_position(seg);
            // Clef position explicitly set by user
            if (pos == ClefToBarlinePosition::BEFORE) {
                continue;
            } else if (pos == ClefToBarlinePosition::AFTER) {
                segs_to_move_to_next_measure.push_back(&seg);
                continue;
            }

            // AUTO on normal barline
            if (!measure->repeat_end()) {
                continue;
            }

            // Otherwise, pos is AUTO
            // Place changes before barline & clefs before repeats style settings
            if ((!sigs_should_be_in_this_measure && !change_applies_to_repeat_and_continuation(seg)) ||
                (measure->repeat_end() && !ctx.conf().style_b(Sid::PLACE_CLEFS_BEFORE_REPEATS))) {
                segs_to_move_to_next_measure.push_back(&seg);
                continue;
            }
        }

        // Move key sigs and time sigs at the end of this measure into the next measure
        if ((seg.is_key_sig_type() || seg.is_time_sig_type()) && !seg.header() && !seg.trailer() &&
            (!sigs_should_be_in_this_measure || !change_applies_to_repeat_and_continuation(seg))) {
            if (next_measure && next_measure->find_segment_r(seg.segment_type(), Fraction(0, 1))) {
                segs_to_remove.push_back(&seg);
                continue;
            }
            segs_to_move_to_next_measure.push_back(&seg);
        }
    }

    // No next measure, so these segments are useless
    if (!next_measure) {
        for (Segment *seg : segs_to_move_to_next_measure) {
            ctx.mut_dom().do_undo_remove_element(seg);
        }
        return;
    }

    std::vector<Segment *> segs_to_move_to_this_measure;
    for (Segment &seg : next_measure->segments()) {
        if (seg.tick() != next_measure->tick() || seg.is_chord_rest_type()) {
            continue;
        }

        // Move clefs at the beginning of the next measure into this measure
        if (seg.is_clef_type()) {
            ClefToBarlinePosition pos = clef_seg_barline_position(seg);
            // Clef position explicitly set by user
            if (pos == ClefToBarlinePosition::BEFORE) {
                segs_to_move_to_this_measure.push_back(&seg);
                continue;
            } else if (pos == ClefToBarlinePosition::AFTER) {
                continue;
            }

            if (!measure->repeat_end()) {
                segs_to_move_to_this_measure.push_back(&seg);
                continue;
            }

            // Otherwise, pos is AUTO
            // Place changes before barline & clefs before repeats style settings
            if ((sigs_should_be_in_this_measure && change_applies_to_repeat_and_continuation(seg)) || !measure->repeat_end() ||
                ctx.conf().style_b(Sid::PLACE_CLEFS_BEFORE_REPEATS)) {
                segs_to_move_to_this_measure.push_back(&seg);
            }
        }

        // Move key sigs and time sigs at the start of the next measure to the end of this measure
        if ((seg.is_time_sig_type() || seg.is_key_sig_type()) && !seg.header() && !seg.trailer() && sigs_should_be_in_this_measure &&
            change_applies_to_repeat_and_continuation(seg)) {
            if (measure->find_segment_r(seg.segment_type(), measure->ticks())) {
                segs_to_remove.push_back(&seg);
                continue;
            }
            segs_to_move_to_this_measure.push_back(&seg);
        }
    }

    for (Segment *seg : segs_to_remove) {
        // Don't add duplicate segs to a measure
        ctx.mut_dom().do_undo_remove_element(seg);

        // Remove from underlying measures
        Measure *seg_measure = seg->measure();
        if (seg_measure && seg_measure->is_mm_rest()) {
            Measure *target = nullptr;
            // Segments at the start of the measure -> mmRestFirst, end -> mmRestLast
            if (seg->rtick() == Fraction(0, 1) || seg->tick() == seg_measure->tick()) {
                target = seg_measure->mm_rest_first();
            } else {
                target = seg_measure->mm_rest_last();
            }
            if (target && target != seg_measure) {
                Segment *counterpart = target->find_segment_r(seg->segment_type(), seg->rtick());
                if (counterpart) {
                    ctx.mut_dom().do_undo_remove_element(counterpart);
                }
            }
        }
    }

    for (Segment *seg : segs_to_move_to_next_measure) {
        Fraction orig_rtick = seg->rtick();

        seg->undo_change_property(Pid::END_OF_MEASURE_CHANGE, false);
        measure->score()->undo(new ChangeSegmentParent(seg, next_measure, Fraction(0, 1)));

        // Move in underlying segments
        if (measure->is_mm_rest()) {
            Measure *from_mm_last = measure->mm_rest_last();
            if (from_mm_last) {
                Segment *underlying_seg = from_mm_last->find_segment_r(seg->segment_type(), orig_rtick);
                if (underlying_seg) {
                    underlying_seg->undo_change_property(Pid::END_OF_MEASURE_CHANGE, false);
                    if (next_measure->is_mm_rest() && next_measure->mm_rest_first()) {
                        next_measure->score()->undo(new ChangeSegmentParent(underlying_seg, next_measure->mm_rest_first(), Fraction(0, 1)));
                    } else {
                        ctx.mut_dom().do_undo_remove_element(underlying_seg);
                    }
                }
            }
        }

        // If the destination is an MMRest but the source is not, create underlying segment
        if (next_measure->is_mm_rest() && !measure->is_mm_rest()) {
            Measure *mm_first = next_measure->mm_rest_first();
            if (mm_first && mm_first != next_measure) {
                Segment *mm_seg = mm_first->undo_get_segment_r(seg->segment_type(), Fraction(0, 1));
                mm_seg->undo_change_property(Pid::END_OF_MEASURE_CHANGE, false);
                for (track_idx_t track = 0; track < ctx.dom().ntracks(); track += ::mu::engraving::voices) {
                    EngravingItem *e = seg->element(track);
                    if (!e) {
                        continue;
                    }
                    if (!mm_seg->element(track)) {
                        bool generated = e->generated();
                        EngravingItem *e_clone = generated ? e->clone() : e->linked_clone();
                        e_clone->set_generated(generated);
                        e_clone->set_ownership_parent(mm_seg);
                        ctx.mut_dom().do_undo_add_element(e_clone);
                    }
                }
            }
        }
    }

    for (Segment *seg : segs_to_move_to_this_measure) {
        Fraction orig_rtick = seg->rtick();

        seg->undo_change_property(Pid::END_OF_MEASURE_CHANGE, true);
        measure->score()->undo(new ChangeSegmentParent(seg, measure, measure->ticks()));

        // Move in underlying segments
        if (next_measure->is_mm_rest()) {
            Measure *from_mm_first = next_measure->mm_rest_first();
            if (from_mm_first) {
                Segment *underlying_seg = from_mm_first->find_segment_r(seg->segment_type(), orig_rtick);
                if (underlying_seg) {
                    underlying_seg->undo_change_property(Pid::END_OF_MEASURE_CHANGE, true);
                    if (measure->is_mm_rest() && measure->mm_rest_last()) {
                        measure->score()->undo(
                            new ChangeSegmentParent(underlying_seg, measure->mm_rest_last(), measure->mm_rest_last()->ticks()));
                    } else {
                        ctx.mut_dom().do_undo_remove_element(underlying_seg);
                    }
                }
            }
        }

        // If the destination is an MMRest but the source is not, create underlying segment
        if (measure->is_mm_rest() && !next_measure->is_mm_rest()) {
            Measure *mm_last = measure->mm_rest_last();
            if (mm_last && mm_last != measure) {
                Segment *mm_seg = mm_last->undo_get_segment_r(seg->segment_type(), measure->ticks());
                mm_seg->undo_change_property(Pid::END_OF_MEASURE_CHANGE, true);
                // Clone per-track elements from the moved segment into the underlying mmrest segment
                for (track_idx_t track = 0; track < ctx.dom().ntracks(); track += ::mu::engraving::voices) {
                    EngravingItem *e = seg->element(track);
                    if (!e) {
                        continue;
                    }
                    if (!mm_seg->element(track)) {
                        bool generated = e->generated();
                        EngravingItem *e_clone = generated ? e->clone() : e->linked_clone();
                        e_clone->set_generated(generated);
                        e_clone->set_ownership_parent(mm_seg);
                        ctx.mut_dom().do_undo_add_element(e_clone);
                    }
                }
            }
        }
    }

    // Check end of measure changes for top and underlying measures
    measure->check_end_of_measure_change();
    if (measure->is_mm_rest()) {
        Measure *mm_last = measure->mm_rest_last();
        if (mm_last) {
            mm_last->check_end_of_measure_change();
        }
    }
    if (next_measure->is_mm_rest()) {
        Measure *mm_first = next_measure->mm_rest_first();
        if (mm_first) {
            mm_first->check_end_of_measure_change();
        }
    }

    // Sort segments at start of next measure
    remove_and_add_begin_segments(next_measure);
    if (next_measure->is_mm_rest()) {
        Measure *mm_first = next_measure->mm_rest_first();
        if (mm_first) {
            remove_and_add_begin_segments(mm_first);
        }
    }

    // Sort segments at start of first measure
    Measure *prev_measure = measure->prev_measure();
    if (prev_measure && prev_measure == ctx.dom().first_measure()) {
        remove_and_add_begin_segments(prev_measure);
        if (prev_measure->is_mm_rest()) {
            Measure *mm_first = prev_measure->mm_rest_first();
            if (mm_first) {
                remove_and_add_begin_segments(mm_first);
            }
        }
    }
}

void ModifyDom::remove_and_add_begin_segments(Measure *measure) {
    std::vector<Segment *> segs_to_sort;
    for (Segment &seg : measure->segments()) {
        if (seg.rtick() != Fraction(0, 1) || seg.is_chord_rest_type() || seg.is_end_bar_line_type()) {
            continue;
        }

        segs_to_sort.push_back(&seg);
    }

    // Re-add the segments. They will be placed in their correct positions
    for (Segment *seg : segs_to_sort) {
        measure->segments().remove(seg);
    }
    for (Segment *seg : segs_to_sort) {
        measure->add(seg);
    }
}
