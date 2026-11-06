/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
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

#include "clonevoice.h"

#include "../dom/beam.h"
#include "../dom/chord.h"
#include "../dom/factory.h"
#include "../dom/interval.h"
#include "../dom/linkedobjects.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/spanner.h"
#include "../dom/staff.h"
#include "../dom/tiemap.h"
#include "../dom/tremolotwochord.h"
#include "../dom/tupletmap.h"
#include "log.h"
#include "transpose.h"

using namespace mu::engraving;

static void do_clone_voice(Score *dest_score, track_idx_t src_track, track_idx_t dst_track, Segment *source_seg, const Fraction &l_tick,
                           bool link = true) {
    Score *source_score = source_seg->score();
    Fraction start = source_seg->tick();
    TieMap tie_map;
    TupletMap tuplet_map; // tuplets cannot cross measure boundaries
    TremoloTwoChord *tremolo = nullptr;

    auto maybe_linked_clone = [link](EngravingItem *item) -> EngravingItem * {
        // If we want to maintain the link (exchange voice) create a linked clone
        // If we want new, unlinked elements (implode/explode) create a clone
        if (link) {
            return item->linked_clone();
        } else {
            return item->clone();
        }
    };

    for (Segment *oseg = source_seg; oseg && oseg->tick() < l_tick; oseg = oseg->next1()) {
        Segment *ns = 0; //create segment later, on demand
        Measure *dm = dest_score->tick2measure(oseg->tick());

        EngravingItem *oe = oseg->element(src_track);

        if (oe && !oe->generated() && oe->is_chord_rest()) {
            ChordRest *ocr = to_chord_rest(oe);
            ChordRest *ncr = to_chord_rest(maybe_linked_clone(ocr));
            ncr->set_score(dest_score);
            ncr->set_track(dst_track);

            //Don't clone gaps to a first voice
            if (!(ncr->track() % ::mu::engraving::voices) && ncr->is_rest()) {
                to_rest(ncr)->set_gap(false);
            }

            //Handle beams
            if (ocr->beam() && !ocr->beam()->empty() && ocr->beam()->elements().front() == ocr) {
                Beam *nb = ocr->beam()->clone();
                nb->clear();
                nb->set_track(dst_track);
                nb->set_score(dest_score);
                nb->add(ncr);
                ncr->set_beam(nb);
            }

            // clone Tuplets
            Tuplet *ot = ocr->tuplet();
            if (ot) {
                ot->set_track(src_track);
                Tuplet *nt = tuplet_map.find_new(ot);
                if (nt == 0) {
                    nt = to_tuplet(maybe_linked_clone(ot));
                    nt->set_track(dst_track);
                    nt->set_ownership_parent(dm);
                    tuplet_map.add(ot, nt);

                    Tuplet *nt1 = nt;
                    while (ot->tuplet()) {
                        Tuplet *nt2 = tuplet_map.find_new(ot->tuplet());
                        if (nt2 == 0) {
                            nt2 = to_tuplet(maybe_linked_clone(ot->tuplet()));
                            nt2->set_track(dst_track);
                            nt2->set_ownership_parent(dm);
                            tuplet_map.add(ot->tuplet(), nt2);
                        }
                        nt2->add(nt1);
                        nt1->set_tuplet(nt2);
                        ot = ot->tuplet();
                        nt1 = nt2;
                    }
                }
                nt->add(ncr);
                ncr->set_tuplet(nt);
            }

            // clone additional settings
            if (oe->is_rest()) {
                Rest *ore = to_rest(ocr);
                // If we would clone a full measure rest just don't clone this rest
                if (ore->is_full_measure_rest() && (dst_track % ::mu::engraving::voices)) {
                    continue;
                }
            }

            auto clone_chord = [&](Chord *old_chord, Chord *new_chord) {
                size_t n = old_chord->notes().size();
                for (size_t i = 0; i < n; ++i) {
                    Note *on = old_chord->notes().at(i);
                    Note *nn = new_chord->notes().at(i);
                    staff_idx_t idx = track2staff(dst_track);
                    Fraction tick = oseg->tick();
                    Interval v = dest_score->staff(idx) ? dest_score->staff(idx)->transpose(tick) : Interval();
                    nn->set_tpc1(on->tpc1());
                    if (v.is_zero()) {
                        nn->set_tpc2(on->tpc1());
                    } else {
                        v.flip();
                        nn->set_tpc2(Transpose::transpose_tpc(nn->tpc1(), v, true));
                    }

                    if (on->tie_for()) {
                        Tie *tie = to_tie(maybe_linked_clone(on->tie_for()));
                        tie->set_score(dest_score);
                        nn->set_tie_for(tie);
                        tie->set_start_note(nn);
                        tie->set_track(nn->track());
                        tie->set_end_note(nn);
                        tie_map.add(on->tie_for(), tie);
                    }
                    if (on->tie_back()) {
                        Tie *tie = tie_map.find_new(on->tie_back());
                        if (tie) {
                            nn->set_tie_back(tie);
                            tie->set_end_note(nn);
                        } else {
                            LOGD("cloneVoices: cannot find tie");
                        }
                    }
                    // add back spanners (going back from end to start spanner element
                    // makes sure the 'other' spanner anchor element is already set up)
                    // 'on' is the old spanner end note and 'nn' is the new spanner end note
                    for (Spanner *old_sp : on->spanner_back()) {
                        Note *new_start = Spanner::start_element_from_spanner(old_sp, nn);
                        if (new_start) {
                            Spanner *new_sp = to_spanner(maybe_linked_clone(old_sp));
                            new_sp->set_note_span(new_start, nn);
                            dest_score->do_undo_add_element(new_sp);
                        } else {
                            LOGD("cloneVoices: cannot find spanner start note");
                        }
                    }
                }
                // two note tremolo
                if (old_chord->tremolo_two_chord()) {
                    if (old_chord == old_chord->tremolo_two_chord()->chord1()) {
                        if (tremolo) {
                            LOGD("unconnected two note tremolo");
                        }
                        tremolo = item_cast<TremoloTwoChord *>(maybe_linked_clone(old_chord->tremolo_two_chord()));
                        tremolo->set_score(new_chord->score());
                        tremolo->set_ownership_parent(new_chord);
                        tremolo->set_track(new_chord->track());
                        tremolo->set_chords(new_chord, nullptr);
                        new_chord->set_tremolo_two_chord(tremolo);
                    } else if (old_chord == old_chord->tremolo_two_chord()->chord2()) {
                        if (!tremolo) {
                            LOGD("first note for two note tremolo missing");
                        } else {
                            tremolo->set_chords(tremolo->chord1(), new_chord);
                            new_chord->set_tremolo_two_chord(tremolo);
                        }
                    } else {
                        LOGD("inconsistent two note tremolo");
                    }
                }
            };
            if (oe->is_chord()) {
                clone_chord(to_chord(ocr), to_chord(ncr));
                for (size_t i = 0; i < to_chord(ocr)->grace_notes().size(); ++i) {
                    Chord *ogc = to_chord(ocr)->grace_notes().at(i);
                    Chord *ngc = to_chord(ncr)->grace_notes().at(i);
                    clone_chord(ogc, ngc);
                }
            }

            // Add element
            if (link) {
                // To current staff only (exchange voice)
                if (!ns) {
                    ns = dm->undo_get_segment(oseg->segment_type(), oseg->tick());
                }
                ncr->set_ownership_parent(ns);
                dest_score->do_undo_add_element(ncr);
            } else {
                // To all linked staves (implode/explode)
                dest_score->undo_add_cr(ncr, dm, oseg->tick());
            }
        }

        Segment *tst = dm->segments().first_cr_segment();
        if (src_track % ::mu::engraving::voices && !(dst_track % ::mu::engraving::voices) && (!tst || (!tst->element(dst_track)))) {
            Rest *rest = Factory::create_rest(dest_score->dummy());
            rest->set_ticks(dm->ticks());
            rest->set_duration_type(DurationType::V_MEASURE);
            rest->set_track(dst_track);
            if (link) {
                Segment *segment = dm->undo_get_segment(SegmentType::CHORD_REST, dm->tick());
                rest->set_ownership_parent(segment);
                dest_score->do_undo_add_element(rest);
            } else {
                dest_score->undo_add_cr(rest, dm, dm->tick());
            }
        }

        const std::vector<EngravingItem *> annotations = oseg->annotations();
        for (EngravingItem *annotation : annotations) {
            if (!annotation->element_applies_to_track(src_track)) {
                continue;
            }

            EngravingItem *new_annotation = maybe_linked_clone(annotation);
            new_annotation->set_track(dst_track);

            if (!ns) {
                ns = dm->undo_get_segment(oseg->segment_type(), oseg->tick());
            }
            new_annotation->set_ownership_parent(ns);

            // Add element
            if (link) {
                // To current staff only (exchange voice)
                dest_score->do_undo_add_element(new_annotation);
            } else {
                // To all linked staves (implode/explode)
                dest_score->undo_add_element(new_annotation);
            }
        }
    }

    // Find and add corresponding slurs and hairpins
    static const std::set<ElementType> spanners_to_copy{ElementType::SLUR, ElementType::HAMMER_ON_PULL_OFF, ElementType::HAIRPIN};
    auto spanners = source_score->spanner_map().find_overlapping(start.ticks(), l_tick.ticks());
    for (auto i = spanners.begin(); i < spanners.end(); i++) {
        Spanner *sp = i->value;
        Fraction sp_start = sp->tick();
        Fraction sp_end = sp_start + sp->ticks();

        if (muse::contains(spanners_to_copy, sp->type()) && (sp_start >= start && sp_end < l_tick)) {
            if (!sp->element_applies_to_track(src_track)) {
                continue;
            }
            Spanner *ns = to_spanner(maybe_linked_clone(sp));

            ns->set_score(dest_score);
            ns->set_ownership_parent(0);
            ns->set_track(dst_track);
            ns->set_track2(dst_track);

            // set start/end element for slur
            ChordRest *cr1 = sp->start_cr();
            ChordRest *cr2 = sp->end_cr();

            ns->set_start_element(0);
            ns->set_end_element(0);
            if (cr1 && cr1->links()) {
                for (EngravingObject *e : *cr1->links()) {
                    ChordRest *cr = to_chord_rest(e);
                    if (cr == cr1) {
                        continue;
                    }
                    if ((cr->score() == dest_score) && (cr->tick() == ns->tick()) && cr->track() == dst_track) {
                        ns->set_start_element(cr);
                        break;
                    }
                }
            }
            if (cr2 && cr2->links()) {
                for (EngravingObject *e : *cr2->links()) {
                    ChordRest *cr = to_chord_rest(e);
                    if (cr == cr2) {
                        continue;
                    }
                    if ((cr->score() == dest_score) && (cr->tick() == ns->tick2()) && cr->track() == dst_track) {
                        ns->set_end_element(cr);
                        break;
                    }
                }
            }
            if (link) {
                dest_score->do_undo_add_element(ns);
            } else {
                dest_score->undo_add_element(ns);
            }
        }
    }
}

void CloneVoice::clone_voice(Segment *source_seg,    // first source segment
                             const Fraction &l_tick, // last tick to clone
                             Segment *dest_seg,      // first destination segment
                             track_idx_t strack, track_idx_t dtrack, bool delete_source,
                             bool link // if true  add elements in destination segment only
                                       // if false add elements in every linked staff
) {
    Score *s = dest_seg->score();
    Fraction ticks = dest_seg->tick() + l_tick - source_seg->tick();

    // Clear destination voice (in case of not linked and otrack = -1 we would delete our source
    for (Segment *seg = dest_seg; seg && seg->tick() < ticks; seg = seg->next1()) {
        EngravingItem *el = seg->element(dtrack);
        if (el && el->is_chord_rest()) {
            link ? s->do_undo_remove_element(el) : s->undo_remove_element(el);
        }
    }

    do_clone_voice(s, strack, dtrack, source_seg, ticks, link);

    if (delete_source) {
        auto spanners = s->spanner_map().find_overlapping(source_seg->tick().ticks(), l_tick.ticks());
        for (auto i = spanners.begin(); i < spanners.end(); i++) {
            Spanner *sp = i->value;
            if (sp->is_slur() && (sp->track() == strack || sp->track2() == strack)) {
                link ? s->do_undo_remove_element(sp) : s->undo_remove_element(sp);
            }
        }
        for (Segment *seg = dest_seg; seg && seg->tick() < ticks; seg = seg->next1()) {
            EngravingItem *el = seg->element(strack);
            if (el && el->is_chord_rest()) {
                link ? s->do_undo_remove_element(el) : s->undo_remove_element(el);
            }

            const std::vector<EngravingItem *> annotations = seg->annotations();
            for (EngravingItem *annotation : annotations) {
                if (annotation && annotation->track() == strack) {
                    if (annotation->has_voice_assignment_properties()) {
                        VoiceAssignment voice_assignment = annotation->get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>();
                        if (voice_assignment != VoiceAssignment::CURRENT_VOICE_ONLY) {
                            continue;
                        }
                    }
                    link ? s->do_undo_remove_element(annotation) : s->undo_remove_element(annotation);
                }
            }
        }
        // Set rests if first voice in a staff
        if (!(strack % ::mu::engraving::voices)) {
            // TODO: take `link` into account?
            s->set_rest(dest_seg->tick(), strack, ticks, false, nullptr);
        }
    }
}
