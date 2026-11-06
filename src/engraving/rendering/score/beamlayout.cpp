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
#include "beamlayout.h"

#include <unordered_map>
#include <set>

#include "containers.h"

#include "dom/beam.h"
#include "dom/drumset.h"
#include "dom/part.h"
#include "dom/tremolotwochord.h"
#include "dom/tremolosinglechord.h"
#include "dom/chord.h"
#include "dom/factory.h"
#include "dom/measure.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "dom/system.h"
#include "dom/timesig.h"
#include "dom/tuplet.h"
#include "dom/utils.h"
#include "dom/note.h"

#include "layoutcontext.h"
#include "tlayout.h"
#include "chordlayout.h"
#include "beamtremololayout.h"
#include "tremololayout.h"
#include "restlayout.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void BeamLayout::layout(Beam *item, const LayoutContext &ctx) {
    TRACEFUNC;

    Beam::LayoutData *ldata = item->mutldata();
    // all of the beam layout code depends on _elements being in order by tick
    // this may not be the case if two cr's were recently swapped.
    std::sort(item->elements().begin(), item->elements().end(),
              [](const ChordRest *a, const ChordRest *b) -> bool { return a->tick() < b->tick(); });
    System *system = item->system();

    std::vector<ChordRest *> crl;

    size_t n = 0;
    for (ChordRest *cr : item->elements()) {
        auto new_system = cr->measure()->system();
        if (new_system && new_system != system) {
            SpannerSegmentType st;
            if (n == 0) {
                st = SpannerSegmentType::BEGIN;
            } else {
                st = SpannerSegmentType::MIDDLE;
            }
            ++n;
            if (item->beam_fragments().size() < n) {
                item->beam_fragments().push_back(new BeamFragment);
            }
            layout2(item, ctx, crl, st, static_cast<int>(n) - 1);
            crl.clear();
            system = cr->measure()->system();
        }
        crl.push_back(cr);
    }

    Shape beam_shape(Shape::Type::COMPOSITE);

    if (!crl.empty()) {
        SpannerSegmentType st;
        if (n == 0) {
            st = SpannerSegmentType::SINGLE;
        } else {
            st = SpannerSegmentType::END;
        }
        if (item->beam_fragments().size() < (n + 1)) {
            item->beam_fragments().push_back(new BeamFragment);
        }
        layout2(item, ctx, crl, st, static_cast<int>(n));

        for (const BeamSegment *bs : item->beam_segments()) {
            beam_shape.add(bs->shape());
        }
    }

    ldata->set_shape(beam_shape);

    // The beam may have changed shape. one-note trems within this beam need to be layed out here
    for (ChordRest *cr : item->elements()) {
        ChordLayout::compute_up(cr, ctx);
        if (cr->is_chord() && to_chord(cr)->tremolo_single_chord()) {
            TremoloLayout::layout(to_chord(cr)->tremolo_single_chord(), ctx);
        }
    }
}

void BeamLayout::layout_if_need(Beam *item, const LayoutContext &ctx) {
    if (!item->ldata()->is_valid()) {
        BeamLayout::layout(item, ctx);
    }
}

//---------------------------------------------------------
//    - remove beam segments
//    - detach from system
//    - calculate stem direction and set chord
//---------------------------------------------------------
void BeamLayout::layout1(Beam *item, LayoutContext &ctx) {
    Beam::LayoutData *ldata = item->mutldata();

    const StaffType *staff_type = item->staff_type();
    item->set_tab((staff_type && staff_type->is_tab_staff()) ? staff_type : nullptr);
    item->set_is_beside_tab_staff(item->tab() && !item->tab()->stemless() && !item->tab()->stem_through());

    // TAB's with stem beside staves have special layout
    if (item->is_beside_tab_staff()) {
        item->set_up(!item->tab()->stems_down());
        item->set_slope(0.0);
        item->set_cross(false);
        item->set_min_move(0);
        item->set_max_move(0);
        for (ChordRest *cr : item->elements()) {
            if (cr->is_chord()) {
                item->set_up(cr->up());
                break;
            }
        }
        return;
    }

    if (item->staff()->is_drum_staff(item->tick())) {
        if (item->direction() != DirectionV::AUTO) {
            item->set_up(item->direction() == DirectionV::UP);
        } else if (item->is_grace()) {
            item->set_up(true);
        } else {
            // search through the beam for the first chord with explicit stem direction and use that.
            // if there is no explicit stem direction, default to the direction of the first stem.
            bool first_up = false;
            bool first_chord = true;
            for (ChordRest *cr : item->elements()) {
                if (!cr->is_chord()) {
                    item->set_up(first_up);
                    continue;
                }
                Chord *chord = to_chord(cr);
                DirectionV cr_direction = to_chord(cr)->stem_direction();
                if (cr_direction != DirectionV::AUTO) {
                    item->set_up(cr_direction == DirectionV::UP);
                    break;
                }
                if (first_chord) {
                    const Staff *staff = item->staff();
                    const Part *part = staff ? staff->part() : nullptr;
                    const Drumset *ds = part ? part->instrument(item->tick())->drumset() : nullptr;
                    first_up = ds ? ds->stem_direction(chord->up_note()->pitch()) == DirectionV::UP : chord->up();
                    first_chord = false;
                }
                item->set_up(first_up);
            }
        }
        for (ChordRest *cr : item->elements()) {
            ChordLayout::compute_up(cr, ctx);
            if (cr->is_chord()) {
                ChordLayout::layout_stem(to_chord(cr), ctx);
            }
        }
        return;
    }

    item->set_min_move(std::numeric_limits<int>::max());
    item->set_max_move(std::numeric_limits<int>::min());
    double mag = 0.0;

    item->note_positions().clear();
    staff_idx_t staff_idx = muse::nidx;
    for (ChordRest *cr : item->elements()) {
        double m = cr->is_small() ? ctx.conf().style_d(Sid::SMALL_NOTE_MAG) : 1.0;
        mag = std::max(mag, m);
        if (cr->is_chord()) {
            Chord *chord = to_chord(cr);
            staff_idx = chord->v_staff_idx();
            int i = chord->staff_move();
            item->set_min_move(std::min(item->min_cr_move(), i));
            item->set_max_move(std::max(item->max_cr_move(), i));

            for (int distance : chord->note_distances()) {
                item->note_positions().push_back(BeamBase::NotePosition(distance, cr->v_staff_idx()));
            }
        }
    }

    std::sort(item->note_positions().begin(), item->note_positions().end());
    ldata->set_mag(mag);

    item->set_cross(item->min_cr_move() != item->max_cr_move());

    //
    // determine beam stem direction
    //
    if (item->elements().empty()) {
        return;
    }
    ChordRest *first_note = item->elements().front();
    Measure *measure = first_note->measure();
    bool has_multiple_voices = measure->has_voices(first_note->staff_idx(), item->tick(), item->ticks());
    if (compute_up_for_moved_cross(item)) {
    } else if (item->direction() != DirectionV::AUTO) {
        item->set_up(item->direction() == DirectionV::UP);
    } else if (item->max_cr_move() > 0) {
        item->set_up(false);
    } else if (item->min_cr_move() < 0) {
        item->set_up(true);
    } else if (item->is_grace()) {
        if (has_multiple_voices) {
            item->set_up(first_note->track() % 2 == 0);
        } else {
            item->set_up(true);
        }
    } else if (item->note_positions().size()) {
        if (has_multiple_voices) {
            item->set_up(first_note->track() % 2 == 0);
        } else {
            if (const Chord *chord = item->find_chord_with_custom_stem_direction()) {
                item->set_up(chord->stem_direction() == DirectionV::UP);
            } else {
                std::set<BeamBase::NotePosition> note_pos_set(item->note_positions().begin(), item->note_positions().end());
                std::vector<int> note_lines;
                note_lines.reserve(note_pos_set.size());
                for (BeamBase::NotePosition pos : note_pos_set) {
                    note_lines.push_back(pos.line);
                }
                item->set_up(ChordLayout::compute_auto_stem_direction(note_lines) > 0);
            }
        }
    } else {
        item->set_up(true);
    }

    int middle_staff_line = first_note->staff_type()->middle_line();
    for (size_t i = 0; i < item->note_positions().size(); i++) {
        item->note_positions()[i].line += middle_staff_line;
    }

    bool is_entirely_moved = false;
    if (item->min_cr_move() == item->max_cr_move() && item->min_cr_move() != 0) {
        is_entirely_moved = true;
        item->set_staff_idx(staff_idx);
        if (item->direction() == DirectionV::AUTO) {
            item->set_up(item->max_cr_move() > 0);
        }
    } else if (item->elements().size()) {
        item->set_staff_idx(item->elements().at(0)->staff_idx());
    }
    item->set_full_cross(is_entirely_moved);

    item->set_slope(0.0);

    for (ChordRest *cr : item->elements()) {
        bool prev_up = cr->up();
        ChordLayout::compute_up(cr, ctx);
        if (cr->is_chord() && cr->up() != prev_up) {
            ChordLayout::layout_chords1(ctx, cr->segment(), cr->staff_idx());
            cr->segment()->create_shape(cr->staff_idx());
            if (to_chord(cr)->is_grace()) {
                ChordLayout::layout_stem(to_chord(cr), ctx);
            }
        }
    }
}

void BeamLayout::layout2(Beam *item, const LayoutContext &ctx, const std::vector<ChordRest *> &chord_rests, SpannerSegmentType, int frag) {
    TRACEFUNC;

    BeamTremoloLayout::setup_l_data(item, item->mutldata(), ctx);
    Chord *start_chord = nullptr;
    Chord *end_chord = nullptr;
    if (chord_rests.empty()) {
        return;
    }
    for (ChordRest *chord_rest : chord_rests) {
        if (chord_rest->is_chord()) {
            if (!start_chord) {
                start_chord = to_chord(chord_rest);
                end_chord = start_chord;
            } else {
                end_chord = to_chord(chord_rest);
            }
            ChordLayout::layout_stem(to_chord(chord_rest), ctx);
        }
    }
    if (!start_chord) {
        // we were passed a vector of only rests. we don't support beams across only rests
        // this beam will be deleted in LayoutBeams
        return;
    }

    item->set_beam_spacing(ctx.conf().style_b(Sid::USE_WIDE_BEAMS) ? 4 : 3);
    item->set_beam_dist((item->beam_spacing() / 4.0) * item->spatium() * item->mag());
    item->set_beam_width(item->absolute_from_spatium(ctx.conf().style_s(Sid::BEAM_WIDTH)) * item->mag());

    item->set_start_anchor(BeamTremoloLayout::chord_beam_anchor(item->ldata(), start_chord, ChordBeamAnchorType::START));
    item->set_end_anchor(BeamTremoloLayout::chord_beam_anchor(item->ldata(), end_chord, ChordBeamAnchorType::END));

    if (item->is_grace()) {
        item->set_beam_dist(item->beam_dist() * ctx.conf().style_d(Sid::GRACE_NOTE_MAG));
        item->set_beam_width(item->beam_width() * ctx.conf().style_d(Sid::GRACE_NOTE_MAG));
    }

    int fragment_index = item->direction_idx();
    if (item->user_modified()) {
        BeamTremoloLayout::setup_l_data(item, item->mutldata(), ctx);
        double start_y = item->beam_fragments()[frag]->py1[fragment_index];
        double end_y = item->beam_fragments()[frag]->py2[fragment_index];
        if (ctx.conf().style_b(Sid::SNAP_CUSTOM_BEAMS_TO_GRID)) {
            const double quarter_space = item->spatium() / 4;
            start_y = round(start_y / quarter_space) * quarter_space;
            end_y = round(end_y / quarter_space) * quarter_space;
        }
        start_y += item->page_pos().y();
        end_y += item->page_pos().y();
        item->start_anchor().set_y(start_y);
        item->end_anchor().set_y(end_y);
        item->mutldata()->set_anchors(item->start_anchor(), item->end_anchor());
        item->compute_and_set_slope();
        create_beam_segments(item, ctx, chord_rests);
        BeamLayout::set_trem_anchors(item, ctx);
        return;
    }

    // anchor represents the middle of the beam, not the tip of the stem
    // location depends on _isBesideTabStaff

    if (!item->is_beside_tab_staff()) {
        BeamTremoloLayout::setup_l_data(item, item->mutldata(), ctx);
        BeamTremoloLayout::calculate_anchors(item, item->mutldata(), ctx, chord_rests, item->note_positions());
        item->set_start_anchor(item->ldata()->start_anchor);
        item->set_end_anchor(item->ldata()->end_anchor);
        item->compute_and_set_slope();
        item->set_beam_dist(item->ldata()->beam_dist);
    } else {
        item->set_slope(0.0);
        Chord *start_chord2 = nullptr;
        for (ChordRest *cr : chord_rests) {
            if (cr->is_chord()) {
                start_chord2 = to_chord(cr);
                break;
            }
        }
        BeamTremoloLayout::setup_l_data(item, item->mutldata(), ctx);
        double x1 = BeamTremoloLayout::chord_beam_anchor_x(item->ldata(), chord_rests.front(), ChordBeamAnchorType::START);
        double x2 = BeamTremoloLayout::chord_beam_anchor_x(item->ldata(), chord_rests.back(), ChordBeamAnchorType::END);
        double y = BeamTremoloLayout::chord_beam_anchor_y(item->ldata(), start_chord2);
        item->start_anchor() = PointF(x1, y);
        item->end_anchor() = PointF(x2, y);
        item->mutldata()->set_anchors(item->start_anchor(), item->end_anchor());
    }

    item->beam_fragments()[frag]->py1[fragment_index] = item->start_anchor().y() - item->page_pos().y();
    item->beam_fragments()[frag]->py2[fragment_index] = item->end_anchor().y() - item->page_pos().y();

    create_beam_segments(item, ctx, chord_rests);
    set_trem_anchors(item, ctx);
}

bool BeamLayout::is_start_of_non_cross_beam(ChordRest *cr) {
    Beam *b = cr->beam();
    if (b && b->elements().front() == cr) {
        if (b->cross()) {
            return false;
        }

        return true;
    }

    return false;
}

bool BeamLayout::is_start_of_cross_beam(ChordRest *cr) {
    Beam *b = cr->beam();
    if (b && b->elements().front() == cr) {
        if (b->cross()) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   restoreBeams
//---------------------------------------------------------

void BeamLayout::restore_beams(Measure *m, LayoutContext &ctx) {
    for (Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
        for (EngravingItem *e : s->elist()) {
            if (e && e->is_chord_rest()) {
                ChordRest *cr = to_chord_rest(e);
                Beam *b = cr->beam();
                if (b && !b->elements().empty() && b->elements().front() == cr) {
                    TLayout::layout_beam(b, ctx);
                    b->add_skyline(m->system()->staff(b->staff_idx())->skyline());
                }
            }
        }
    }
}

bool BeamLayout::measure_may_have_beams_joined_into_next(const Measure *measure) {
    const MeasureBase *next = measure->next();
    if (!(next && next->is_measure())) {
        return false;
    }

    const Measure *next_measure = to_measure(next);
    const Segment *first_cr_seg = next_measure->find_first_r(SegmentType::CHORD_REST, Fraction(0, 1));
    if (!first_cr_seg) {
        return false;
    }

    for (const EngravingItem *item : first_cr_seg->elist()) {
        if (!item) {
            continue;
        }
        BeamMode beam_mode = to_chord_rest(item)->beam_mode();
        if (beam_mode == BeamMode::MID || beam_mode == BeamMode::BEGIN16 || beam_mode == BeamMode::BEGIN32) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   breakCrossMeasureBeams
//---------------------------------------------------------

void BeamLayout::break_cross_measure_beams(Measure *measure, LayoutContext &ctx) {
    MeasureBase *mb_next = measure->next();
    if (!mb_next || !mb_next->is_measure()) {
        return;
    }

    Measure *next = to_measure(mb_next);
    const size_t ntracks = ctx.dom().ntracks();
    Segment *fst_seg = next->first(SegmentType::CHORD_REST);
    if (!fst_seg) {
        return;
    }

    for (size_t track = 0; track < ntracks; ++track) {
        const Staff *stf = ctx.dom().staff(track2staff(track));

        // don’t compute beams for invisible staves and tablature without stems
        if (!stf->show() || (stf->is_tab_staff(measure->tick()) && stf->staff_type(measure->tick())->stemless())) {
            continue;
        }

        EngravingItem *e = fst_seg->element(static_cast<int>(track));
        if (!e || !e->is_chord_rest()) {
            continue;
        }

        ChordRest *cr = to_chord_rest(e);
        Beam *beam = cr->beam();
        if (!beam || beam->elements().front()->measure() == next) { // no beam or not cross-measure beam
            continue;
        }

        std::vector<ChordRest *> m_elements;
        std::vector<ChordRest *> next_elements;

        for (ChordRest *beam_cr_value : beam->elements()) {
            if (beam_cr_value->tick() < next->tick()) {
                m_elements.push_back(beam_cr_value);
            } else {
                next_elements.push_back(beam_cr_value);
            }
        }

        if (m_elements.size() == 1) {
            m_elements[0]->remove_delete_beam(false);
        }

        Beam *new_beam = nullptr;
        if (next_elements.size() > 1) {
            new_beam = Factory::create_beam(ctx.mut_dom().score());
            new_beam->set_generated(true);
            new_beam->set_track(track);
        }

        const bool next_beamed = bool(new_beam);
        for (ChordRest *next_cr : next_elements) {
            next_cr->remove_delete_beam(next_beamed);
            if (new_beam) {
                new_beam->add(next_cr);
            }
        }

        if (new_beam) {
            layout1(new_beam, ctx);
        }
    }
}

static bool beam_no_continue(BeamMode mode) { return mode == BeamMode::END || mode == BeamMode::NONE || mode == BeamMode::INVALID; }

#define beamModeMid(a) (a == BeamMode::MID || a == BeamMode::BEGIN16 || a == BeamMode::BEGIN32)

//---------------------------------------------------------
//   beamGraceNotes
//---------------------------------------------------------

void BeamLayout::beam_grace_notes(LayoutContext &ctx, Chord *main_note, bool after) {
    ChordRest *a1 = 0; // start of (potential) beam
    Beam *beam = 0;    // current beam
    BeamMode bm = BeamMode::AUTO;
    std::vector<Chord *> grace_notes = after ? main_note->grace_notes_after() : main_note->grace_notes_before();

    if (beam) {
        beam->set_is_grace(true);
    }

    for (ChordRest *cr : grace_notes) {
        bm = Groups::base_beam_mode(cr);
        if ((cr->duration_type().type() <= DurationType::V_QUARTER) || (bm == BeamMode::NONE)) {
            if (beam) {
                beam->set_is_grace(true);
                layout1(beam, ctx);
                beam = 0;
            }
            if (a1) {
                a1->remove_delete_beam(false);
                a1 = 0;
            }
            cr->remove_delete_beam(false);
            continue;
        }
        if (beam) {
            bool beam_end = bm == BeamMode::BEGIN;
            if (!beam_end) {
                cr->replace_beam(beam);
                cr = 0;
                beam_end = (bm == BeamMode::END);
            }
            if (beam_end) {
                beam->set_is_grace(true);
                layout1(beam, ctx);
                beam = 0;
            }
        }
        if (!cr) {
            continue;
        }
        if (a1 == 0) {
            a1 = cr;
        } else {
            if (!beamModeMid(bm) && (bm == BeamMode::BEGIN)) {
                a1->remove_delete_beam(false);
                a1 = cr;
            } else {
                beam = a1->beam();
                if (beam == 0 || beam->elements().front() != a1) {
                    beam = Factory::create_beam(ctx.mut_dom().score());
                    beam->set_generated(true);
                    beam->set_track(main_note->track());
                    a1->replace_beam(beam);
                }
                cr->replace_beam(beam);
                a1 = 0;
            }
        }
    }
    if (beam) {
        beam->set_is_grace(true);
        layout1(beam, ctx);
    } else if (a1) {
        a1->remove_delete_beam(false);
    }
}

void BeamLayout::create_beams(LayoutContext &ctx, Measure *measure) {
    TRACEFUNC;

    for (track_idx_t track = 0; track < ctx.dom().ntracks(); ++track) {
        const Staff *stf = ctx.dom().staff(track2staff(track));

        // don’t compute beams for invisible staves and tablature without stems
        if (!stf->show() || (stf->is_tab_staff(measure->tick()) && stf->staff_type(measure->tick())->stemless())) {
            continue;
        }

        ChordRest *a1 = nullptr; // start of (potential) beam
        bool first_cr = true;
        Beam *beam = nullptr; // current beam
        BeamMode bm = BeamMode::AUTO;
        ChordRest *prev = nullptr;
        std::unordered_map<int, TDuration> beat_subdivision;

        // if this measure is simple meter (actually X/4),
        // then perform a prepass to determine the subdivision of each beat

        beat_subdivision.clear();
        TimeSig *ts = stf->time_sig(measure->tick());
        const SegmentType st = SegmentType::CHORD_REST;
        if (ts && ts->denominator() == 4) {
            Fraction stretch = ts->stretch();
            for (Segment *s = measure->first(st); s; s = s->next(st)) {
                ChordRest *mcr = to_chord_rest(s->element(track));
                if (mcr == 0) {
                    continue;
                }
                int beat = (mcr->rtick() * stretch).ticks() / Constants::division;
                if (muse::contains(beat_subdivision, beat)) {
                    beat_subdivision[beat] = std::min(beat_subdivision[beat], mcr->duration_type());
                } else {
                    beat_subdivision[beat] = mcr->duration_type();
                }
            }
        }

        for (Segment *segment = measure->first(st); segment; segment = segment->next(st)) {
            ChordRest *cr = segment->cr(track);
            if (!cr) {
                continue;
            }

            if (cr->is_chord()) {
                Chord *chord = to_chord(cr);
                for (Chord *c : chord->grace_notes()) {
                    c->set_beamlet(nullptr); // Will be defined during beam layout
                }
            }
            cr->set_beamlet(nullptr); // Will be defined during beam layout

            if (first_cr) {
                first_cr = false;
                // Handle cross-measure beams
                BeamMode mode = cr->beam_mode();
                if (mode == BeamMode::MID || mode == BeamMode::END || mode == BeamMode::BEGIN16 || mode == BeamMode::BEGIN32) {
                    ChordRest *prev_cr = ctx.mut_dom().find_cr(measure->tick() - Fraction::eps(), track);
                    if (prev_cr) {
                        Beam *prev_beam = prev_cr->beam();
                        const Measure *pm = prev_cr->measure();
                        if (!beam_no_continue(prev_cr->beam_mode()) && !pm->line_break() && !pm->page_break() && !pm->section_break() &&
                            ctx.state().prev_measure() &&
                            !(prev_cr->is_chord() && prev_cr->duration_type().type() <= DurationType::V_QUARTER)) {
                            beam = prev_beam;
                            if (prev_cr->is_chord()) {
                                if (Hook *hook = to_chord(prev_cr)->hook()) {
                                    ctx.mut_dom().do_undo_remove_element(hook);
                                    prev_cr->segment()->staff_shape(prev_cr->v_staff_idx()).remove_if([hook](const ShapeElement &el) {
                                        return el.item() == hook;
                                    });
                                }
                            }
                            //a1 = beam ? beam->elements().front() : prevCR;
                            a1 = beam ? nullptr : prev_cr; // when beam is found, a1 is no longer required.
                        } else if (prev_beam && prev_beam == cr->beam() && prev_beam->elements().front() == prev_cr) {
                            // remove the beam from the previous chordrest because we do not currently
                            // support cross-system beams
                            prev_cr->remove_delete_beam(false);
                        }
                    }
                }
            }

            bool has_all_rest = cr->beam() && cr->beam()->has_all_rests();
            bool is_a_rest_beam_start = cr->is_rest() && cr->beam_mode() == BeamMode::BEGIN;
            if (has_all_rest && !is_a_rest_beam_start) {
                cr->remove_delete_beam(false);
            }

            // handle grace notes
            // (tied chords?)
            if (cr->is_chord()) {
                Chord *chord = to_chord(cr);
                beam_grace_notes(ctx, chord, false); // grace before
                beam_grace_notes(ctx, chord, true);  // grace after
            }

            bm = Groups::actual_beam_mode(cr, prev, &beat_subdivision);

            prev = cr;

            // Rests of any duration can be beamed over, if required
            bool can_be_beamed = cr->duration_type().type() > DurationType::V_QUARTER || cr->is_rest();
            if (!can_be_beamed || (bm == BeamMode::NONE)) {
                bool remove_beam = true;
                if (beam) {
                    layout1(beam, ctx);
                    remove_beam = (beam->elements().size() <= 1 || beam->has_all_rests());
                    beam = 0;
                }
                if (a1) {
                    if (remove_beam) {
                        a1->remove_delete_beam(false);
                    }
                    a1 = 0;
                }
                cr->remove_delete_beam(false);
                continue;
            }

            if (beam) {
                bool beam_end = (bm == BeamMode::BEGIN);
                if (!beam_end) {
                    cr->replace_beam(beam);
                    cr = 0;
                    beam_end = (bm == BeamMode::END);
                }
                if (beam_end) {
                    layout1(beam, ctx);
                    beam = 0;
                }
            }
            if (!cr) {
                continue;
            }

            if (a1 == 0) {
                a1 = cr;
            } else {
                if (!beamModeMid(bm) && (bm == BeamMode::BEGIN || (a1->segment()->segment_type() != cr->segment()->segment_type()) ||
                                         (a1->end_tick() < cr->tick()))) {
                    a1->remove_delete_beam(false);
                    a1 = cr;
                } else {
                    beam = a1->beam();
                    if (beam == 0 || beam->elements().front() != a1) {
                        beam = Factory::create_beam(ctx.mut_dom().score());
                        beam->set_generated(true);
                        beam->set_track(track);
                        a1->replace_beam(beam);
                    }
                    cr->replace_beam(beam);
                    a1 = 0;
                }
            }
        }
        if (beam) {
            layout1(beam, ctx);
        } else if (a1) {
            Fraction next_tick = a1->end_tick();
            Measure *m = (next_tick >= measure->end_tick() ? measure->next_measure() : measure);
            ChordRest *next_cr = (m ? m->find_chord_rest(next_tick, track) : nullptr);
            Beam *b = a1->beam();
            if (!(b && !b->elements().empty() && !b->has_all_rests() && b->elements().front() == a1 && next_cr &&
                  beamModeMid(next_cr->beam_mode()))) {
                a1->remove_delete_beam(false);
            }
        }
    }
}

/************************************************************
 * layoutNonCrossBeams()
 * layout all non-cross-staff beams starting on this segment
 * **********************************************************/

void BeamLayout::layout_non_cross_beams(ChordRest *cr, LayoutContext &ctx) {
    if (cr->is_chord()) {
        for (Chord *grace : to_chord(cr)->grace_notes()) {
            layout_non_cross_beams(grace, ctx);
        }
    }

    if (!BeamLayout::is_start_of_non_cross_beam(cr)) {
        return;
    }

    Beam *beam = cr->beam();

    TLayout::layout_beam(beam, ctx);

    if (!beam->trem_anchors().empty()) {
        // there are inset tremolos in here
        for (ChordRest *beam_cr_value : beam->elements()) {
            if (!beam_cr_value->is_chord()) {
                continue;
            }
            Chord *c = to_chord(beam_cr_value);
            if (c->tremolo_two_chord()) {
                TremoloLayout::layout(c->tremolo_two_chord(), ctx);
            }
        }
    }

    for (ChordRest *beam_cr_value : beam->elements()) {
        if (beam_cr_value->is_rest() && beam_cr_value->v_staff_idx() == beam->staff_idx()) {
            vertical_adjust_beamed_rests(to_rest(beam_cr_value), beam, ctx);
        }
    }
}

void BeamLayout::vertical_adjust_beamed_rests(Rest *rest, Beam *beam, LayoutContext &ctx) {
    const double spatium = rest->spatium();
    static constexpr Fraction rest32nd(1, 32);
    const bool up = beam->up();
    const BeamMode rest_beam_mode = rest->beam_mode();
    const bool first_rest = beam->elements().front() == rest && (rest_beam_mode == BeamMode::BEGIN || rest_beam_mode == BeamMode::BEGIN16 ||
                                                                 rest_beam_mode == BeamMode::BEGIN32 || rest_beam_mode == BeamMode::MID);

    double rest_to_beam_padding;
    if (rest->ticks() <= rest32nd) {
        rest_to_beam_padding = 0.2 * spatium;
    } else {
        rest_to_beam_padding = 0.35 * spatium;
    }

    const Shape beam_shape = beam->shape().translate(beam->page_pos());
    const Shape rest_shape = rest->shape().translate(rest->page_pos() - rest->offset());
    const double min_beam_to_rest_x_dist = up && first_rest ? 0.1 * spatium : 0.0;

    const double rest_to_beam_clearance =
        up ? beam_shape.vertical_clearance(rest_shape, min_beam_to_rest_x_dist) : rest_shape.vertical_clearance(beam_shape);
    const double line_distance = rest->staff()->line_distance(rest->tick()) * spatium;

    int clearance_in_steps = std::ceil(rest_to_beam_clearance / line_distance);
    up ? rest->vertical_clearance().set_above(clearance_in_steps) : rest->vertical_clearance().set_below(clearance_in_steps);

    if (rest_to_beam_clearance > rest_to_beam_padding) {
        return;
    }

    const bool rest_is_locked = rest->vertical_clearance().locked();
    if (!rest_is_locked) {
        double overlap = (rest_to_beam_padding - rest_to_beam_clearance);
        int line_moves = ceil(overlap / line_distance);
        line_moves *= up ? 1 : -1;
        double y_move = line_moves * line_distance;
        rest->mutldata()->move_y(y_move);
        for (Rest *merged_rest : rest->ldata()->merged_rests) {
            merged_rest->mutldata()->move_y(y_move);
        }

        Segment *segment = rest->segment();
        staff_idx_t staff_idx = rest->v_staff_idx();
        const Staff *staff = ctx.dom().staff(staff_idx);
        std::vector<Chord *> chords;
        std::vector<Rest *> rests;
        collect_chords_and_rest(segment, staff_idx, chords, rests);
        RestLayout::resolve_rest_vs_chord(rests, chords, staff, segment);
        RestLayout::resolve_rest_vs_rest(rests, staff, segment, ctx, /*considerBeams*/ true);
    }

    TLayout::layout_beam(beam, ctx);

    rest->segment()->create_shape(rest->v_staff_idx());
}

void BeamLayout::check_cross_pos_and_stem_consistency(Beam *beam, LayoutContext &ctx) {
    if (!beam->cross() || !beam->user_modified()) {
        return;
    }

    int correct_cross_staff_idx = 0;
    bool inconsistency_found = false;

    for (ChordRest *cr : beam->elements()) {
        if (!cr->is_chord()) {
            continue;
        }
        Chord *chord = to_chord(cr);
        bool current_up = chord->up();
        bool actual_up = ChordLayout::is_chord_pos_below_beam(chord, beam);
        if (actual_up != current_up) {
            inconsistency_found = true;
            chord->set_up(actual_up);
            ChordLayout::layout_chords1(ctx, chord->segment(), chord->staff_idx());
            chord->segment()->create_shape(chord->staff_idx());
        }
        if (actual_up) {
            correct_cross_staff_idx = std::min(correct_cross_staff_idx, chord->staff_move());
        } else {
            correct_cross_staff_idx = std::max(correct_cross_staff_idx, chord->staff_move() + 1);
        }
    }

    int beam_cross_staff_idx = beam->cross_staff_idx();
    if (beam_cross_staff_idx != correct_cross_staff_idx) {
        inconsistency_found = true;
        int cur_cross_staff_move = beam->cross_staff_move();
        int diff = correct_cross_staff_idx - beam_cross_staff_idx;
        beam->set_property(Pid::BEAM_CROSS_STAFF_MOVE, cur_cross_staff_move + diff);
    }

    if (inconsistency_found) {
        BeamLayout::layout(beam, ctx);
    }
}

void BeamLayout::create_beam_segments(Beam *item, const LayoutContext &ctx, const std::vector<ChordRest *> &chord_rests) {
    item->clear_beam_segments();

    bool level_has_beam = false;
    int level = 0;
    constexpr size_t no_last_chord = std::numeric_limits<size_t>::max();
    size_t num_cr = chord_rests.size();
    bool french_style_beams = ctx.conf().style_b(Sid::FRENCH_STYLE_BEAMS);
    do {
        level_has_beam = false;
        ChordRest *start_cr = nullptr;
        ChordRest *end_cr = nullptr;
        bool break_beam = false;
        bool previous_break16 = false;
        bool previous_break32 = false;
        size_t last_chord_index = no_last_chord;

        for (size_t i = 0; i < num_cr; i++) {
            ChordRest *chord_rest = chord_rests[i];
            ChordRest *prev_chord_rest = i < 1 ? nullptr : chord_rests[i - 1];

            if (level < chord_rest->beams() && !chord_rest->is_rest()) {
                level_has_beam = true;
            }
            bool is_broken16 = false;
            bool is_broken32 = false;

            // updates isBroken16 and isBroken32
            item->calc_beam_breaks(chord_rest, prev_chord_rest, level, is_broken16, is_broken32);
            break_beam = is_broken16 || is_broken32;
            bool skip_rest = false;
            BeamMode beam_mode = chord_rest->beam_mode();
            if (chord_rest->is_rest() && level >= chord_rest->beams() &&
                (beam_mode == BeamMode::MID || beam_mode == BeamMode::BEGIN16 || beam_mode == BeamMode::BEGIN32)) {
                // if the level is lower than both the previous and next real chords, don't break here
                size_t next_chord_index = i + 1;
                while (next_chord_index < num_cr && chord_rests[next_chord_index]->is_rest()) {
                    ++next_chord_index;
                }
                if (last_chord_index < num_cr && next_chord_index < num_cr) {
                    Chord *last_chord = to_chord(chord_rests[last_chord_index]);
                    Chord *next_chord = to_chord(chord_rests[next_chord_index]);
                    // we have both chords (not rests) on either side of this rest, so if the level is lower than both we can
                    // just continue the beam through here
                    // this creates situations where we can have the second beam of a pair of 16th notes continuing through
                    // an 8th rest, for example, even though an 8th note would very obviously break the beam
                    // this is desired for BeamType::MID on rests.
                    if (level < last_chord->beams() && level < next_chord->beams()) {
                        skip_rest = (is_broken16 && level < 1) || (is_broken32 && level < 2) || (!is_broken16 && !is_broken32);
                    }
                }
            }
            if ((level < chord_rest->beams() && !break_beam) || skip_rest) {
                end_cr = chord_rest;
                if (!start_cr || (start_cr->is_rest() && start_cr != item->elements().front())) {
                    start_cr = chord_rest;
                }
            } else if (level >= chord_rest->beams() && chord_rest->is_rest() && !break_beam) {
                // This rest has duration longer than the beam value, but it may still go
                // under the beam if the next chords or rests are beamed
                continue;
            } else {
                size_t beamlet_index = static_cast<size_t>(i) - 1;
                if (last_chord_index < item->elements().size() && (chord_rest->is_rest() || (end_cr && end_cr->is_rest()))) {
                    // we broke the beam on this chordrest, but the last cr of the beam segment can't end on a rest
                    // so it ends on lastChord
                    ChordRest *last_cr = to_chord_rest(item->elements()[last_chord_index]);
                    if (last_cr && start_cr && last_cr->tick() >= start_cr->tick()) {
                        end_cr = last_cr;
                        beamlet_index = last_chord_index;
                        last_chord_index = no_last_chord;
                    }
                }
                if (start_cr && end_cr && level_has_beam) {
                    if (start_cr == end_cr && start_cr->is_chord()) {
                        bool is_beamlet_before = calc_is_beamlet_before(item, to_chord(start_cr), static_cast<int>(beamlet_index), level,
                                                                        previous_break16, previous_break32);
                        create_beamlet_segment(item, ctx, to_chord(start_cr), is_beamlet_before, level);
                    } else if (start_cr != end_cr) {
                        create_beam_segment(item, start_cr, end_cr, level, french_style_beams);
                    }
                }
                bool set_cr = chord_rest && chord_rest->is_chord() && break_beam && level < chord_rest->beams();
                start_cr = set_cr ? chord_rest : nullptr;
                end_cr = set_cr ? chord_rest : nullptr;
            }
            previous_break16 = is_broken16;
            previous_break32 = is_broken32;
            if (chord_rest && chord_rest->is_chord()) {
                last_chord_index = i;
            }
        }

        // if the beam ends on the last chord
        if (start_cr && (end_cr || break_beam)) {
            if ((start_cr == end_cr || !end_cr) && start_cr->is_chord()) {
                // this chord is either the last chord, or the first (followed by a rest)
                bool is_before = !(start_cr == chord_rests.front());
                create_beamlet_segment(item, ctx, to_chord(start_cr), is_before, level);
            } else {
                create_beam_segment(item, start_cr, end_cr, level, french_style_beams);
            }
        }
        level++;
    } while (level_has_beam);
}

bool BeamLayout::calc_is_beamlet_before(const Beam *item, Chord *chord, int i, int level, bool is_after16_break, bool is_after32_break) {
    // if first or last chord in beam group
    if (i == 0) {
        return false;
    } else if (i == static_cast<int>(item->elements().size()) - 1) {
        return true;
    }
    // if first or last chord in tuplet
    Tuplet *tuplet = chord->tuplet();
    if (tuplet && chord == tuplet->elements().front()) {
        return false;
    } else if (tuplet && chord == tuplet->elements().back()) {
        return true;
    }

    // next note has a beam break
    ChordRest *next_chord_rest = item->elements()[i + 1];
    ChordRest *curr_chord_rest = item->elements()[i];
    ChordRest *prev_chord_rest = item->elements()[i - 1];
    if (next_chord_rest->is_chord()) {
        bool next_break16 = false;
        bool next_break32 = false;
        bool curr_break16 = false;
        bool curr_break32 = false;
        item->calc_beam_breaks(curr_chord_rest, prev_chord_rest, prev_chord_rest->beams(), curr_break16, curr_break32);
        item->calc_beam_breaks(next_chord_rest, curr_chord_rest, level, next_break16, next_break32);
        if ((next_break16 && level >= 1) || (!curr_break16 && next_break32 && level >= 2)) {
            return true;
        }
    }

    // if previous or next chord has more beams, point in that direction
    int previous_chord_level = -1;
    int next_chord_level = -1;
    int previous_offset = 1;
    while (i - previous_offset >= 0) {
        ChordRest *previous = item->elements()[i - previous_offset];
        if (previous->is_chord()) {
            previous_chord_level = to_chord(previous)->beams();
            if (is_after16_break) {
                previous_chord_level = std::min(previous_chord_level, 1);
            } else if (is_after32_break) {
                previous_chord_level = std::min(previous_chord_level, 2);
            }
            break;
        }
        ++previous_offset;
    }

    int next_offset = 1;
    while (i + next_offset < static_cast<int>(item->elements().size())) {
        ChordRest *next = item->elements()[i + next_offset];
        if (next->is_chord()) {
            next_chord_level = to_chord(next)->beams();
            break;
        }
        ++next_offset;
    }
    int chord_level_difference = next_chord_level - previous_chord_level;
    if (chord_level_difference != 0) {
        return chord_level_difference < 0;
    }

    // if the chord ends a subdivision of the beat
    Fraction base_tick = tuplet ? tuplet->tick() : chord->measure()->tick();
    Fraction stretch = chord->staff()->time_stretch(base_tick);
    Fraction tick_next = (next_chord_rest->tick() - base_tick) * stretch;
    if (tuplet) {
        // for tuplets with odd ratios, apply ratio
        // for tuplets with even ratios, use actual beat
        Fraction ratio = tuplet->ratio();
        if (ratio.numerator() & 1) {
            tick_next *= ratio;
        }
    }

    static const int beam_tuplet_tolerance = 6;
    int tick_large_size = chord->ticks().ticks() * 2;
    int remainder = tick_next.ticks() % tick_large_size;
    if (remainder <= beam_tuplet_tolerance || (tick_large_size - remainder) <= beam_tuplet_tolerance) {
        return true;
    }

    // default case
    return false;
}

static inline int compute_stem_length_adjustment_steps(const ChordRest *cr, bool is_start_or_end, bool is_above, bool french_style_beams,
                                                       int level, int beams_above, int beams_below) {
    if (is_above == cr->up()) { // beam segment on opposite side
        if (!french_style_beams || is_start_or_end) {
            // lengthen
            return level - (is_above ? beams_below : beams_above);
        }
    } else { // beamlet on the note side
        if (french_style_beams && !is_start_or_end) {
            // shorten
            return (is_above ? beams_below : beams_above) - level;
        }
    }

    return 0;
}

void BeamLayout::create_beam_segment(Beam *item, ChordRest *start_cr, ChordRest *end_cr, int level, bool french_style_beams) {
    const bool is_first_subgroup = start_cr == item->elements().front();
    const bool is_last_subgroup = end_cr == item->elements().back();
    const bool first_up = start_cr->up();
    const bool last_up = end_cr->up();
    bool overall_up = item->up();
    if (is_first_subgroup == is_last_subgroup) {
        // this subgroup is either the only one in the beam, or in the middle
        if (first_up == last_up) {
            // the "outside notes" of this subgroup go the same direction so use them
            // to determine the side of the beams
            overall_up = first_up;
        } else {
            // no perfect way to solve this problem, for now we'll base it on the number of
            // up and down stemmed notes in this subgroup
            int up_stems, down_stems;
            up_stems = down_stems = 0;
            for (ChordRest *cr : item->elements()) {
                if (!cr->is_chord() || cr->tick() < start_cr->tick()) {
                    continue;
                }
                if (cr->tick() > end_cr->tick()) {
                    break;
                }

                ++(to_chord(cr)->up() ? up_stems : down_stems);

                if (cr == end_cr) {
                    break;
                }
            }
            if (up_stems == down_stems) {
                // we are officially bamboozled. for now we can just use the default
                // direction based on the staff we're on I guess
                overall_up = item->up();
            } else {
                // use the direction with the most stems
                overall_up = up_stems > down_stems;
            }
        }
    } else if (is_first_subgroup) {
        overall_up = last_up;
    } else if (is_last_subgroup) {
        overall_up = first_up;
    }

    const double start_x = BeamTremoloLayout::chord_beam_anchor_x(item->ldata(), start_cr, ChordBeamAnchorType::START);
    const double end_x = BeamTremoloLayout::chord_beam_anchor_x(item->ldata(), end_cr, ChordBeamAnchorType::END);

    double start_y = item->slope() * (start_x - item->start_anchor().x()) + item->start_anchor().y() - item->page_pos().y();
    double end_y = item->slope() * (end_x - item->start_anchor().x()) + item->start_anchor().y() - item->page_pos().y();

    int beams_below = 0; // how many beams below level 0?
    int beams_above = 0; // how many beams above level 0?

    // avoid adjusting for beams on opposite side of level 0
    if (level != 0) {
        for (const BeamSegment *beam : item->beam_segments()) {
            if (beam->level == 0 || beam->end_tick < start_cr->tick() || beam->start_tick > end_cr->tick()) {
                continue;
            }
            ++(beam->above ? beams_above : beams_below);
        }

        const int up_value = overall_up ? -1 : 1;
        const int extra_beam_adjust = overall_up ? beams_above : beams_below;
        const double vertical_offset = item->beam_dist() * (level - extra_beam_adjust) * up_value;

        if (muse::real_is_equal(item->grow_left(), item->grow_right())) {
            start_y -= vertical_offset * item->grow_left();
            end_y -= vertical_offset * item->grow_left();
        } else {
            // Feathered beams
            double start_proportion_along_x = (start_x - item->start_anchor().x()) / (item->end_anchor().x() - item->start_anchor().x());
            double end_proportion_along_x = (end_x - item->start_anchor().x()) / (item->end_anchor().x() - item->start_anchor().x());

            double grow1 = start_proportion_along_x * (item->grow_right() - item->grow_left()) + item->grow_left();
            double grow2 = end_proportion_along_x * (item->grow_right() - item->grow_left()) + item->grow_left();

            start_y -= vertical_offset * grow1;
            end_y -= vertical_offset * grow2;
        }
    }

    BeamSegment *b = new BeamSegment(item);
    b->above = !overall_up;
    b->level = level;
    b->line = LineF(start_x, start_y, end_x, end_y);
    b->start_tick = start_cr->tick();
    b->end_tick = end_cr->tick();
    item->beam_segments().push_back(b);

    if (level > 0) {
        ++(b->above ? beams_above : beams_below);
    }

    // extend stems properly
    for (ChordRest *cr : item->elements()) {
        if (!cr->is_chord() || cr->tick() < start_cr->tick()) {
            continue;
        }
        if (cr->tick() > end_cr->tick()) {
            break;
        }

        Chord *chord = to_chord(cr);
        if (!chord->stem()) {
            continue;
        }

        double up_value = chord->up() ? -1.0 : 1.0;

        if (level == 0) {
            BeamTremoloLayout::extend_stem(item->ldata(), chord, 0.0);
            // HACK: extend the bounding box of the stem (NOT the stem itself) by half beam width.
            // This way the bbox of the stem covers also the beam position. Hugely helps with all the collision checks.
            chord->stem()->mutldata()->beam_correction = up_value * item->beam_width() / 2;
        } else {
            bool is_start_or_end = cr == start_cr || cr == end_cr;
            int visual_stem_length_adjustment_steps =
                compute_stem_length_adjustment_steps(cr, is_start_or_end, b->above, french_style_beams, level, beams_above, beams_below);
            if (visual_stem_length_adjustment_steps != 0) {
                double grow = item->grow_left();
                if (!muse::real_is_equal(item->grow_left(), item->grow_right())) {
                    double anchor_x = BeamTremoloLayout::chord_beam_anchor_x(item->ldata(), chord, ChordBeamAnchorType::MIDDLE);
                    double proportion_along_x = (anchor_x - item->start_anchor().x()) / (item->end_anchor().x() - item->start_anchor().x());
                    grow = proportion_along_x * (item->grow_right() - item->grow_left()) + item->grow_left();
                }

                double visual_stem_length_adjustment = grow * visual_stem_length_adjustment_steps * item->beam_dist();

                if (french_style_beams) {
                    // Even if we visually shorten the stem, we need the 'original' length for collisions.
                    int bbox_stem_length_adjustment_steps =
                        compute_stem_length_adjustment_steps(cr, is_start_or_end, b->above,
                                                             /*frenchStyleBeams*/ false, level, beams_above, beams_below);
                    chord->stem()->mutldata()->beam_correction =
                        up_value * item->beam_width() / 2 +
                        up_value * grow * (bbox_stem_length_adjustment_steps - visual_stem_length_adjustment_steps) * item->beam_dist();
                }
                BeamTremoloLayout::extend_stem(item->ldata(), chord, visual_stem_length_adjustment);
            }
        }

        if (cr == end_cr) {
            break;
        }
    }
}

void BeamLayout::create_beamlet_segment(Beam *item, const LayoutContext &ctx, ChordRest *cr, bool is_before, int level) {
    const double start_x =
        BeamTremoloLayout::chord_beam_anchor_x(item->ldata(), cr, is_before ? ChordBeamAnchorType::END : ChordBeamAnchorType::START);

    const double beamlet_length = ctx.conf().style_absolute(Sid::BEAM_MIN_LEN) * cr->mag();

    const double end_x = start_x + (is_before ? -beamlet_length : beamlet_length);

    double start_y = item->slope() * (start_x - item->start_anchor().x()) + item->start_anchor().y() - item->page_pos().y();
    double end_y = item->slope() * (end_x - start_x) + start_y;

    // how many beams past level 0 (i.e. beams on the other side of level 0 for this subgroup)
    int extra_beam_adjust = 0;

    // avoid adjusting for beams on opposite side of level 0
    for (const BeamSegment *beam : item->beam_segments()) {
        if (beam->level == 0 || beam->line.x2() < start_x || beam->line.x1() > end_x) {
            continue;
        }

        if (cr->up() == beam->above) {
            extra_beam_adjust++;
        }
    }

    const int up_value = cr->up() ? -1 : 1;
    const double vertical_offset = item->beam_dist() * (level - extra_beam_adjust) * up_value;

    if (muse::real_is_equal(item->grow_left(), item->grow_right())) {
        start_y -= vertical_offset * item->grow_left();
        end_y -= vertical_offset * item->grow_left();
    } else {
        // Feathered beams
        double start_proportion_along_x = (start_x - item->start_anchor().x()) / (item->end_anchor().x() - item->start_anchor().x());
        double end_proportion_along_x = (end_x - item->start_anchor().x()) / (item->end_anchor().x() - item->start_anchor().x());

        double grow1 = start_proportion_along_x * (item->grow_right() - item->grow_left()) + item->grow_left();
        double grow2 = end_proportion_along_x * (item->grow_right() - item->grow_left()) + item->grow_left();

        start_y -= vertical_offset * grow1;
        end_y -= vertical_offset * grow2;
    }

    BeamSegment *b = new BeamSegment(item);
    b->above = !cr->up();
    b->level = level;
    b->line = LineF(start_x, start_y, end_x, end_y);
    b->is_beamlet = true;
    b->is_before = is_before;
    cr->set_beamlet(b);

    ChordLayout::fill_shape(cr, cr->mutldata(), ctx.conf());
    //! NOTE Moved from ChordRest::setBeamlet
    cr->segment()->create_shape(cr->v_staff_idx());
    item->beam_segments().push_back(b);
}

bool BeamLayout::compute_up_for_moved_cross(Beam *item) {
    if (!item->cross()) {
        return false;
    }
    bool is_above_all_chords = true;
    bool is_below_all_chords = true;
    for (ChordRest *cr : item->elements()) {
        if (cr->is_below_cross_beam(item)) {
            is_below_all_chords = false;
        } else {
            is_above_all_chords = false;
        }
    }

    if (is_above_all_chords) {
        item->set_up(true);
        return true;
    }

    if (is_below_all_chords) {
        item->set_up(false);
        return true;
    }

    return false;
}

PointF BeamLayout::chord_beam_anchor(const Beam *item, const ChordRest *chord, ChordBeamAnchorType anchor_type) {
    return BeamTremoloLayout::chord_beam_anchor(item->ldata(), chord, anchor_type);
}

double BeamLayout::chord_beam_anchor_y(const Beam *item, const ChordRest *chord) {
    return BeamTremoloLayout::chord_beam_anchor_y(item->ldata(), chord);
}

void BeamLayout::set_trem_anchors(Beam *item, const LayoutContext &ctx) {
    item->trem_anchors().clear();
    for (ChordRest *cr : item->elements()) {
        if (!cr || !cr->is_chord()) {
            continue;
        }
        Chord *c = to_chord(cr);
        TremoloTwoChord *t = c ? c->tremolo_two_chord() : nullptr;
        if (t && t->chord1() == c && t->chord2()->beam() == item) {
            // there is an inset tremolo here!
            // figure out up / down
            bool trem_up = t->up();
            if (item->user_modified()) {
                trem_up = c->up();
            } else if (item->cross() && t->chord1()->staff_move() == t->chord2()->staff_move()) {
                trem_up = t->chord1()->staff_move() == item->max_cr_move();
            }
            TremAnchor trem_anchor;
            trem_anchor.chord1 = c;
            int regular_beams = c->beams(); // non-tremolo strokes
            double scale = t->spatium() * t->mag();

            // find the left-side anchor
            double width = item->end_anchor().x() - item->start_anchor().x();
            double height = item->end_anchor().y() - item->start_anchor().y();
            double x = chord_beam_anchor(item, c, ChordBeamAnchorType::MIDDLE).x();
            double proportion_along_x = (x - item->start_anchor().x()) / width;
            double y = item->start_anchor().y() + (proportion_along_x * height);
            y += regular_beams * (ctx.conf().style_b(Sid::USE_WIDE_BEAMS) ? 1.0 : 0.75) * scale * (trem_up ? 1. : -1.);
            trem_anchor.y1 = y;
            // find the right-side anchor
            x = chord_beam_anchor(item, t->chord2(), ChordBeamAnchorType::MIDDLE).x();
            proportion_along_x = (x - item->start_anchor().x()) / width;
            y = item->start_anchor().y() + (proportion_along_x * height);
            y += regular_beams * (ctx.conf().style_b(Sid::USE_WIDE_BEAMS) ? 1.0 : 0.75) * scale * (trem_up ? 1. : -1.);
            trem_anchor.y2 = y;
            item->trem_anchors().push_back(trem_anchor);
        }
    }
}
