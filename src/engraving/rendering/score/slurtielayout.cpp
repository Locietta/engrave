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
#include "slurtielayout.h"

#include "chordlayout.h"
#include "dom/chord.h"
#include "dom/clef.h"
#include "dom/engravingitem.h"
#include "dom/guitarbend.h"
#include "dom/hook.h"
#include "dom/laissezvib.h"
#include "dom/ledgerline.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/parenthesis.h"
#include "dom/partialtie.h"
#include "dom/score.h"
#include "dom/slur.h"
#include "dom/staff.h"
#include "dom/stafftype.h"
#include "dom/stem.h"
#include "dom/system.h"
#include "dom/tie.h"
#include "dom/tremolotwochord.h"
#include "draw/types/transform.h"
#include "editing/editchord.h"
#include "global/stringutils.h"
#include "iengravingconfiguration.h" // IWYU pragma: keep
#include "iengravingfont.h"
#include "log.h"
#include "stemlayout.h"
#include "tlayout.h"
#include "tremololayout.h"

using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

SpannerSegment *SlurTieLayout::layout_system(Slur *item, System *system, LayoutContext &ctx) {
    const double horizontal_tie_clearance = 0.35 * item->spatium();
    const double tie_clearance = 0.65 * item->spatium();
    const double continued_slur_offset_y = item->spatium() * .4;
    const double continued_slur_max_diff = 2.5 * item->spatium();
    Fraction stick = system->first_measure()->tick();
    Fraction etick = system->last_measure()->end_tick();

    SlurSegment *slur_segment =
        to_slur_segment(TLayout::get_next_layout_system_segment(item, system, [item]() { return item->new_slur_tie_segment(); }));

    SpannerSegmentType sst;
    if (item->tick() >= stick) {
        //
        // this is the first call to layoutSystem,
        // processing the first line segment
        //
        if (item->track2() == muse::nidx) {
            item->set_track2(item->track());
        }
        if (item->start_cr() == 0 || item->start_cr()->measure() == 0) {
            LOGD("Slur::layout(): track %zu-%zu  %p - %p tick %d-%d null start anchor", item->track(), item->track2(), item->start_cr(),
                 item->end_cr(), item->tick().ticks(), item->tick2().ticks());
            return slur_segment;
        }
        if (item->ticks().is_zero() && !(item->start_cr() && item->start_cr()->is_grace()) &&
            !(item->end_cr() && item->end_cr()->is_grace()) && !item->is_incoming() && !item->is_outgoing() &&
            !item->is_tapping_half_slur()) {
            LOGD("Slur::layout(): track %zu tick %d zero-length slur, skipping", item->track(), item->tick().ticks());
            return slur_segment;
        }
        if (item->end_cr() == 0) { // sanity check
            item->set_end_element(item->start_cr());
            item->set_tick2(item->tick());
        }
        compute_up(item, ctx);
        sst = item->tick2() < etick ? SpannerSegmentType::SINGLE : SpannerSegmentType::BEGIN;
    } else if (item->tick() < stick && item->tick2() >= etick) {
        sst = SpannerSegmentType::MIDDLE;
    } else {
        sst = SpannerSegmentType::END;
    }
    slur_segment->set_spanner_segment_type(sst);

    SlurTiePos s_pos;
    slur_pos(item, &s_pos, ctx);
    PointF p1, p2;
    // adjust for ties
    p1 = s_pos.p1;
    p2 = s_pos.p2;
    bool constrain_left_anchor = false;

    const bool incoming_partial_slur = item->is_incoming();
    const bool outgoing_partial_slur = item->is_outgoing();

    // start anchor, either on the start chordrest or at the beginning of the system
    if ((sst == SpannerSegmentType::SINGLE || sst == SpannerSegmentType::BEGIN) && !incoming_partial_slur) {
        Chord *sc = item->start_cr()->is_chord() ? to_chord(item->start_cr()) : nullptr;

        // on chord
        if (sc) {
            Tie *tie = (item->up() ? sc->up_note() : sc->down_note())->tie_for();
            PointF end_point = PointF();
            if (tie && (tie->is_inside() || tie->up() != item->up() || tie_segment_should_be_skipped(tie))) {
                // there is a tie that starts on this chordrest
                tie = nullptr;
            }
            if (tie && !tie->segments_empty()) {
                end_point = tie->segment_at(0)->ups(Grip::START).pos();
            }
            bool adjusted_vertically = false;
            if (tie) {
                if (item->up() && tie->up()) {
                    if (end_point.y() - p1.y() < tie_clearance) {
                        p1.ry() = end_point.y() - tie_clearance;
                        adjusted_vertically = true;
                    }
                } else if (!item->up() && !tie->up()) {
                    if (p1.y() - end_point.y() < tie_clearance) {
                        p1.ry() = end_point.y() + tie_clearance;
                        adjusted_vertically = true;
                    }
                }
            }
            Tie *tie_back = sc->up_note()->tie_back();
            if (!adjusted_vertically && tie_back && (!tie_back->is_inside() || tie_back->is_partial_tie()) &&
                tie_back->up() == item->up() && !tie_segment_should_be_skipped(tie_back)) {
                // there is a tie that ends on this chordrest
                tie = tie_back;
                if (!tie->segments_empty()) {
                    end_point = tie->segment_at(static_cast<int>(tie->nsegments()) - 1)->ups(Grip::END).pos();
                    if (std::abs(end_point.y() - p1.y()) < tie_clearance) {
                        p1.rx() += horizontal_tie_clearance;
                    }
                }
            }
        }
    } else if (sst == SpannerSegmentType::END || sst == SpannerSegmentType::MIDDLE || incoming_partial_slur) {
        // beginning of system
        Measure *measure = item->start_cr()->measure();
        ChordRest *first_cr = incoming_partial_slur ? measure->first_chord_rest(item->track()) : system->first_chord_rest(item->track2());
        const bool same_staff = first_cr && first_cr->v_staff_idx() == slur_segment->v_staff_idx();
        double y = p1.y();
        if (first_cr && first_cr == item->end_cr()) {
            constrain_left_anchor = true;
        }
        if (first_cr && first_cr->is_chord() && same_staff) {
            Chord *chord = to_chord(first_cr);
            Shape chord_shape = chord->shape();
            chord_shape.remove_types({ElementType::ACCIDENTAL});
            y = item->up() ? chord_shape.top() : chord_shape.bottom();
            bool is_above_stem = chord->stem() && chord->up() == item->up();
            if (!is_above_stem) {
                y += continued_slur_offset_y * (item->up() ? -1 : 1);
            }
        }

        double segment_x = incoming_partial_slur ? measure->first_note_rest_segment_x(true) : system->first_note_rest_segment_x(true);
        p1 = PointF(segment_x, y);

        // adjust for ties at the start of the system
        ChordRest *cr = incoming_partial_slur ? measure->first_chord_rest(item->track()) : system->first_chord_rest(item->track());
        const bool tie_same_staff = cr && cr->v_staff_idx() == item->staff_idx();
        if (cr && cr->is_chord() && cr->tick() >= stick && cr->tick() <= etick && tie_same_staff) {
            Chord *c = to_chord(cr);
            Tie *tie = nullptr;
            PointF end_point;
            Tie *tie_back = c->up_note()->tie_back();
            if (tie_back && (tie_back->is_partial_tie() || !tie_back->is_inside()) && tie_back->up() == item->up() &&
                !tie_segment_should_be_skipped(tie_back)) {
                // there is a tie that ends on this chordrest
                if (!tie_back->segments_empty()) { //Checks for spanner segment esxists
                    tie = tie_back;
                    end_point = tie->back_segment()->ups(Grip::START).pos();
                }
            }
            if (tie) {
                if (tie->is_partial_tie()) {
                    p1.rx() = end_point.rx();
                }
                if (item->up() && tie->up()) {
                    if (end_point.y() - p1.y() < tie_clearance) {
                        p1.ry() = end_point.y() - tie_clearance;
                    }
                } else if (!item->up() && !tie->up()) {
                    if (p1.y() - end_point.y() < tie_clearance) {
                        p1.ry() = end_point.y() + tie_clearance;
                    }
                }
            }
        }
    }

    // end anchor
    if ((sst == SpannerSegmentType::SINGLE || sst == SpannerSegmentType::END) && !outgoing_partial_slur) {
        Chord *ec = item->end_cr()->is_chord() ? to_chord(item->end_cr()) : nullptr;

        // on chord
        if (ec) {
            Tie *tie = (item->up() ? ec->up_note() : ec->down_note())->tie_back();
            PointF end_point;
            if (tie && (tie->is_inside() || tie->up() != item->up() || tie_segment_should_be_skipped(tie))) {
                tie = nullptr;
            }
            bool adjusted_vertically = false;
            if (tie && !tie->segments_empty()) {
                end_point = tie->segment_at(0)->ups(Grip::END).pos();
                if (item->up() && tie->up()) {
                    if (end_point.y() - p2.y() < tie_clearance) {
                        p2.ry() = end_point.y() - tie_clearance;
                        adjusted_vertically = true;
                    }
                } else if (!item->up() && !tie->up()) {
                    if (p2.y() - end_point.y() < tie_clearance) {
                        p2.ry() = end_point.y() + tie_clearance;
                        adjusted_vertically = true;
                    }
                }
            }
            Tie *tie_for = ec->up_note()->tie_for();
            if (!adjusted_vertically && tie_for && (tie_for->is_partial_tie() || !tie_for->is_inside()) && tie_for->up() == item->up() &&
                !tie_segment_should_be_skipped(tie_for)) {
                // there is a tie that starts on this chordrest
                if (!tie_for->segments_empty() && std::abs(tie_for->front_segment()->ups(Grip::START).pos().y() - p2.y()) < tie_clearance) {
                    p2.rx() -= horizontal_tie_clearance;
                }
            }
        }
    } else {
        // at end of system
        Measure *measure = item->end_cr()->measure();
        ChordRest *last_cr = outgoing_partial_slur ? measure->last_chord_rest(item->track()) : system->last_chord_rest(item->track());
        double y = p1.y();
        const bool same_staff = last_cr && last_cr->v_staff_idx() == slur_segment->v_staff_idx();
        if (last_cr && last_cr == item->start_cr()) {
            y += 0.25 * item->spatium() * (item->up() ? -1 : 1);
        } else if (last_cr && last_cr->is_chord() && same_staff) {
            Chord *chord = to_chord(last_cr);
            Shape chord_shape = chord->shape();
            chord_shape.remove_types({ElementType::ACCIDENTAL});
            y = item->up() ? chord_shape.top() : chord_shape.bottom();
            bool is_above_stem = chord->stem() && chord->up() == item->up();
            if (!is_above_stem) {
                y += continued_slur_offset_y * (item->up() ? -1 : 1);
            }
            double diff = item->up() ? y - p1.y() : p1.y() - y;
            if (diff > continued_slur_max_diff) {
                y = p1.y() + (y > p1.y() ? continued_slur_max_diff : -continued_slur_max_diff);
            }
        }

        double ending_x =
            ending_x_for_open_ended_slur_tie(outgoing_partial_slur ? measure : system->last_measure(), item->start_cr()->v_staff_idx());
        p2 = PointF(ending_x, y);

        // adjust for ties at the end of the system
        ChordRest *cr = outgoing_partial_slur ? measure->last_chord_rest(item->track()) : system->last_chord_rest(item->track());
        const bool tie_same_staff = cr && cr->v_staff_idx() == item->staff_idx();

        if (cr && cr->is_chord() && cr->tick() >= stick && cr->tick() <= etick && tie_same_staff) {
            Chord *c = to_chord(cr);
            Tie *tie = nullptr;
            PointF end_point;
            Tie *tie_for = c->up_note()->tie_for();
            if (tie_for && (tie_for->is_partial_tie() || !tie_for->is_inside()) && tie_for->up() == item->up() &&
                !tie_segment_should_be_skipped(tie_for)) {
                // there is a tie that starts on this chordrest
                if (!tie_for->segments_empty()) { //Checks is spanner segment exists
                    tie = tie_for;
                    end_point = tie->segment_at(0)->ups(Grip::END).pos();
                }

                if (outgoing_partial_slur && tie->type() == ElementType::TIE && tie->nsegments() == 1) {
                    // For partial slurs ending midway through a tie, get top of the tie shape at the slur's end X
                    const TieSegment *tie_seg = tie->front_segment();
                    const Shape tie_shape = tie_seg->shape().translate(tie_seg->pos());
                    if (item->up() && tie->up()) {
                        end_point.ry() = p2.y() + tie_shape.top_distance(p2);
                    } else if (!item->up() && !tie->up()) {
                        end_point.ry() = p2.y() - tie_shape.bottom_distance(p2);
                    }
                }
            }
            if (tie) {
                if (item->up() && tie->up()) {
                    if (end_point.y() - p2.y() < tie_clearance) {
                        p2.ry() = end_point.y() - tie_clearance;
                    }
                } else if (!item->up() && !tie->up()) {
                    if (p2.y() - end_point.y() < tie_clearance) {
                        p2.ry() = end_point.y() + tie_clearance;
                    }
                }
            }
        }
    }
    if (constrain_left_anchor) {
        p1.ry() = p2.y() + (0.25 * item->spatium() * (item->up() ? -1 : 1));
    }

    if (item->is_tapping_half_slur()) {
        p1 = p2 - PointF(2.5 * item->spatium(), 0.0);
    }

    bool is_hanging_slur =
        sst == SpannerSegmentType::BEGIN || sst == SpannerSegmentType::END || incoming_partial_slur || outgoing_partial_slur;
    if (is_hanging_slur && ctx.conf().style_b(Sid::ANGLE_HANGING_SLURS_AWAY_FROM_STAFF)) {
        adjust_slur_floating_end_point_angles(slur_segment, p1, p2, incoming_partial_slur, outgoing_partial_slur);
    }

    layout_segment(slur_segment, p1, p2);

    return slur_segment;
}

void SlurTieLayout::adjust_slur_floating_end_point_angles(SlurSegment *slur_seg, PointF &p1, PointF &p2, bool incoming_partial,
                                                          bool outgoing_partial) {
    bool start_is_hanging = slur_seg->spanner_segment_type() == SpannerSegmentType::END || incoming_partial;
    bool end_is_hanging = slur_seg->spanner_segment_type() == SpannerSegmentType::BEGIN || outgoing_partial;

    IF_ASSERT_FAILED(start_is_hanging != end_is_hanging) { return; }

    bool up = slur_seg->slur()->up();
    const double height_diff = 1.0 * slur_seg->spatium();

    if (start_is_hanging) {
        double y_cur = p1.y();
        p1.set_y(up ? std::min(y_cur, p2.y() - height_diff) : std::max(y_cur, p2.y() + height_diff));
    } else if (end_is_hanging) {
        double y_cur = p2.y();
        p2.set_y(up ? std::min(y_cur, p1.y() - height_diff) : std::max(y_cur, p1.y() + height_diff));
    }
}

double SlurTieLayout::ending_x_for_open_ended_slur_tie(const Measure *measure, staff_idx_t staff_idx) {
    double ending_x = measure->ending_x_for_open_ended_lines();

    const SegmentType segment_types_to_avoid = SegmentType::TIME_SIG_TYPES | SegmentType::KEY_SIG_TYPES | SegmentType::CLEF_TYPES;

    // Find the first segment at the end of the measure which meets the criteria for the SlurTie to stop before it:
    const Segment *end_segment = measure->find_segment_r(segment_types_to_avoid, measure->ticks());
    const EngravingItem *end_item = nullptr;
    while (end_segment) {
        if (end_segment->is_type(segment_types_to_avoid) && end_segment->enabled()) {
            end_item = end_segment->element(staff2track(staff_idx));
            if (end_item && end_item->visible() && end_item->add_to_skyline()) {
                break;
            }
        }
        end_segment = end_segment->next();
    }

    if (!end_segment || !end_item) {
        return ending_x;
    }

    const double margin = end_item->style().style_absolute(Sid::LINE_END_TO_BARLINE_DISTANCE);
    const double item_left_x = measure->x() + end_segment->x() + end_item->ldata()->shape().translated(end_item->pos()).left();

    return std::min(ending_x, item_left_x - margin);
}

//---------------------------------------------------------
//   slurPos
//    calculate position of start- and endpoint of slur
//    relative to System() position
//---------------------------------------------------------

void SlurTieLayout::slur_pos(Slur *item, SlurTiePos *sp, LayoutContext &ctx) {
    item->stem_floated().reset();
    double spatium_value = (item->staff_type() ? item->staff_type()->line_distance().val() : 1.0) * item->spatium();
    const double stem_side_inset = 0.5;
    const double stem_offset_x = 0.35;
    const double beam_clearance = 0.35;
    const double beam_anchor_inset = 0.15;
    const double straight_stem_x_offset = 0.5; // how far down a straight stem a slur attaches (percent)
    const double min_offset = 0.2;
    // hack alert!! -- fakeCutout
    // The fakeCutout const describes the slope of a line from the top of the stem to the full width of the hook.
    // this is necessary because hooks don't have SMuFL cutouts
    // Gonville and MuseJazz have really weirdly-shaped hooks compared to Leland and Bravura and Emmentaler,
    // so we need to adjust the slope of our hook-avoidance line. this will be unnecessary when hooks have
    // SMuFL anchors
    const std::string font_name = muse::strings::to_lower(ctx.engraving_font()->name());
    bool bulky_hook = font_name == "gonville" || font_name == "musejazz";
    const double fake_cutout_slope = bulky_hook ? 1.5 : 1.0;

    if (item->end_cr() == 0) {
        sp->p1 = item->start_cr()->page_pos();
        sp->p1.rx() += item->start_cr()->width();
        sp->p2 = sp->p1;
        sp->p2.rx() += 5 * spatium_value;
        sp->system1 = item->start_cr()->measure()->system();
        sp->system2 = sp->system1;
        return;
    }

    bool use_tablature = item->staff() && item->staff()->is_tab_staff(item->end_cr()->tick());
    bool staff_has_stems = true; // assume staff uses stems
    const StaffType *stt = 0;
    if (use_tablature) {
        stt = item->staff()->staff_type(item->tick());
        staff_has_stems = stt->stem_through(); // if tab with stems beside, stems do not count for slur pos
    }

    // start and end cr, chord, and note
    ChordRest *scr = item->start_cr();
    ChordRest *ecr = item->end_cr();
    Chord *sc = 0;
    Note *note1 = 0;
    if (scr->is_chord()) {
        sc = to_chord(scr);
        note1 = item->up() ? sc->up_note() : sc->down_note();
    }
    Chord *ec = 0;
    Note *note2 = 0;
    if (ecr->is_chord()) {
        ec = to_chord(ecr);
        note2 = item->up() ? ec->up_note() : ec->down_note();
    }

    sp->system1 = scr->measure()->system();
    sp->system2 = ecr->measure()->system();

    if (sp->system1 == 0) {
        LOGD("no system1");
        return;
    }

    sp->p1 = scr->pos() + scr->segment()->pos() + scr->measure()->pos();
    sp->p2 = ecr->pos() + ecr->segment()->pos() + ecr->measure()->pos();
    if (scr->is_grace()) {
        sp->p1 += scr->layout_parent()->pos();
    }
    if (ecr->is_grace()) {
        sp->p2 += ecr->layout_parent()->pos();
    }

    // adjust for cross-staff
    if (scr->v_staff_idx() != item->v_staff_idx() && sp->system1) {
        double diff = sp->system1->staff(scr->v_staff_idx())->y() - sp->system1->staff(item->v_staff_idx())->y();
        sp->p1.ry() += diff;
    }
    if (ecr->v_staff_idx() != item->v_staff_idx() && sp->system2) {
        double diff = sp->system2->staff(ecr->v_staff_idx())->y() - sp->system2->staff(item->v_staff_idx())->y();
        sp->p2.ry() += diff;
    }

    // account for centering or other adjustments (other than mirroring)
    if (note1 && !note1->ldata()->mirror.value()) {
        sp->p1.rx() += note1->x();
    }
    if (note2 && !note2->ldata()->mirror.value()) {
        sp->p2.rx() += note2->x();
    }

    PointF po = PointF();

    Stem *stem1 = sc && staff_has_stems ? sc->stem() : 0;
    Stem *stem2 = ec && staff_has_stems ? ec->stem() : 0;

    enum class SlurAnchor : char { NONE, STEM };
    SlurAnchor sa1 = SlurAnchor::NONE;
    SlurAnchor sa2 = SlurAnchor::NONE;
    if (staff_has_stems) {
        if (sc && sc->hook() && sc->up() == item->up()) {
            sa1 = SlurAnchor::STEM;
        }
        if (scr->up() == ecr->up() && scr->up() == item->up()) {
            if (stem1 && !stem_side_start_for_beam(item)) {
                sa1 = SlurAnchor::STEM;
            }
            if (stem2 && !stem_side_end_for_beam(item)) {
                sa2 = SlurAnchor::STEM;
            }
        } else if (ecr->segment()->system() != scr->segment()->system()) {
            // in the case of continued slurs, we anchor to stem when necessary
            if (scr->up() == item->up() && stem1 && !scr->beam()) {
                sa1 = SlurAnchor::STEM;
            }
            if (ecr->up() == item->up() && stem2 && !ecr->beam()) {
                sa2 = SlurAnchor::STEM;
            }
        }
    }

    double up_value = item->up() ? -1.0 : 1.0;
    double hw1 = note1 ? note1->tab_head_width(stt) : scr->width() * scr->mag(); // if stt == 0, tabHeadWidth()
    double hw2 = note2 ? note2->tab_head_width(stt) : ecr->width() * ecr->mag(); // defaults to headWidth()
    PointF pt;
    switch (sa1) {
        case SlurAnchor::STEM: //sc can't be null
        {
            // place slur starting point at stem end point
            pt = StemLayout::stem_pos(sc) - sc->page_pos() + sc->stem()->ldata()->line.p2();
            if (use_tablature) { // in tabs, stems are centred on note:
                pt.rx() =
                    hw1 * 0.5 +
                    (note1 ?
                         note1->bbox_x_shift() :
                         0.0); // skip half notehead to touch stem, anatoly-os: incorrect. half notehead width is not always the stem position
            }
            // clear the stem (x)
            // allow slight overlap (y)
            // don't allow overlap with hook if not disabling the autoplace checks against start/end segments in SlurSegment::layoutSegment()
            double yadj = -stem_side_inset * sc->intrinsic_mag();
            yadj *= spatium_value * up_value;
            double offset = std::max(stem_offset_x * sc->intrinsic_mag(), min_offset);
            pt += PointF(offset * spatium_value, yadj);
            // account for articulations
            fix_articulations(item, pt, sc, up_value, true);
            // adjust for hook
            double fake_cutout = 0.0;
            if (!ctx.conf().style_b(Sid::USE_STRAIGHT_NOTE_FLAGS)) {
                Hook *hook = sc->hook();
                // regular flags

                if (hook && hook->ldata()->bbox().translated(hook->pos()).contains(pt)) {
                    // TODO: in the utopian far future where all hooks have SMuFL cutouts, this fakeCutout business will no
                    // longer be used. for the time being fakeCutout describes a point on the line y=mx+b, out from the top of the stem
                    // where y = yadj, m = fakeCutoutSlope, and x = y/m + fakeCutout
                    fake_cutout = std::min(0.0, std::abs(yadj) - (hook->width() / fake_cutout_slope));
                    pt.rx() = StemLayout::stem_pos_x(sc) - fake_cutout;
                }
            } else {
                Hook *hook = sc->hook();
                // straight flags
                if (hook && hook->ldata()->bbox().translated(hook->pos()).contains(pt)) {
                    double hook_width = hook->width() * hook->mag();
                    pt.rx() = (hook_width * straight_stem_x_offset) + (hook->pos().x() + sc->x());
                    if (item->up()) {
                        pt.ry() = sc->down_note()->pos().y() - stem1->height() - (beam_clearance * spatium_value * .7);
                    } else {
                        pt.ry() = sc->up_note()->pos().y() + stem1->height() + (beam_clearance * spatium_value * .7);
                    }
                }
            }
            sp->p1 += pt;
        } break;
        case SlurAnchor::NONE: break;
    }
    switch (sa2) {
        case SlurAnchor::STEM: //ec can't be null
        {
            pt = StemLayout::stem_pos(ec) - ec->page_pos() + ec->stem()->ldata()->line.p2();
            if (use_tablature) {
                pt.rx() = hw2 * 0.5;
            }
            // don't allow overlap with beam
            double yadj;
            if (ec->beam() && ec->beam()->elements().front() != ec) {
                yadj = 0.75;
            } else if (ec->tremolo_two_chord() && ec->tremolo_two_chord()->chord2() == ec) {
                yadj = 0.75;
            } else {
                yadj = -stem_side_inset;
            }
            yadj *= spatium_value * up_value;
            double offset = std::max(stem_offset_x * ec->intrinsic_mag(), min_offset);
            pt += PointF(-offset * spatium_value, yadj);
            // account for articulations
            fix_articulations(item, pt, ec, up_value, true);
            sp->p2 += pt;
        } break;
        case SlurAnchor::NONE: break;
    }

    //
    // default position:
    //    horizontal: middle of notehead
    //    vertical:   _spatium * .4 above/below notehead
    //
    //------p1
    // Compute x0, y0 and stemPos
    if (sa1 == SlurAnchor::NONE || sa2 == SlurAnchor::NONE) { // need stemPos if sa2 == SlurAnchor::NONE
        bool stem_pos = false;                                // p1 starts at chord stem side

        // default positions
        po.rx() = hw1 * .5 + (note1 ? note1->bbox_x_shift() : 0.0);
        if (note1) {
            po.ry() = note1->pos().y();
        } else if (item->up()) {
            po.ry() = scr->ldata()->bbox().top();
        } else {
            po.ry() = scr->ldata()->bbox().top() + scr->height();
        }
        double offset = use_tablature ? 0.75 : 0.9;
        po.ry() += scr->intrinsic_mag() * spatium_value * offset * up_value;

        // adjustments for stem and/or beam
        TremoloTwoChord *trem = sc ? sc->tremolo_two_chord() : nullptr;
        if (stem1 || trem) { //sc not null
            Beam *beam1 = sc->beam();
            if (stem1 && beam1 && (beam1->elements().back() != sc) && (sc->up() == item->up())) {
                TLayout::layout_beam(beam1, ctx);
                // start chord is beamed but not the last chord of beam group
                // and slur direction is same as start chord (stem side)

                // in these cases, layout start of slur to stem
                double beam_width_sp = ctx.conf().style_s(Sid::BEAM_WIDTH).val() * beam1->mag_s();
                double offset2 = std::max(beam_clearance * sc->intrinsic_mag(), min_offset) * spatium_value;
                double sh = stem1->length() + (beam_width_sp / 2) + offset2;
                if (item->up()) {
                    po.ry() = StemLayout::stem_pos(sc).y() - sc->page_pos().y() - sh;
                } else {
                    po.ry() = StemLayout::stem_pos(sc).y() - sc->page_pos().y() + sh;
                }
                po.rx() = StemLayout::stem_pos_x(sc) + (beam_anchor_inset * spatium_value * sc->intrinsic_mag()) +
                          (stem1->line_width_mag() / 2 * up_value);

                // account for articulations
                fix_articulations(item, po, sc, up_value, true);

                // force end of slur to layout to stem as well,
                // if start and end chords have same stem direction
                stem_pos = true;
            } else if (trem && trem->chord2() != sc && sc->up() == item->up()) {
                TremoloLayout::layout(trem, ctx);
                Note *note = item->up() ? sc->up_note() : sc->down_note();
                double stem_height = stem1 ? stem1->length() : default_stem_length_start(trem);
                double offset2 = std::max(beam_clearance * sc->intrinsic_mag(), min_offset) * spatium_value;
                double sh = stem_height + offset2;

                if (item->up()) {
                    po.ry() = StemLayout::stem_pos(sc).y() - sc->page_pos().y() - sh;
                } else {
                    po.ry() = StemLayout::stem_pos(sc).y() - sc->page_pos().y() + sh;
                }
                if (!stem1) {
                    po.rx() = note->notehead_center_x();
                } else {
                    po.rx() = StemLayout::stem_pos_x(sc) + (beam_anchor_inset * spatium_value * sc->intrinsic_mag()) +
                              (stem1->line_width_mag() / 2. * up_value);
                }
                fix_articulations(item, po, sc, up_value, true);

                stem_pos = true;
            } else {
                // start chord is not beamed or is last chord of beam group
                // or slur direction is opposite that of start chord

                // at this point slur is in default position relative to note on slur side
                // but we may need to make further adjustments

                // if stem and slur are both up
                // we need to clear stem horizontally
                double stem_offset_mag = stem_offset_x * sc->intrinsic_mag();
                if (sc->up() && item->up()) {
                    // stems in tab staves come from the middle of the head, which means it's much easier
                    // to just subtract an offset from the notehead center (which po already is)
                    if (use_tablature) {
                        po.rx() += stem_offset_mag * spatium_value;
                    } else {
                        po.rx() = hw1 + spatium_value * stem_offset_mag;
                    }
                }

                //
                // handle case: stem up   - stem down
                //              stem down - stem up
                //
                if ((sc->up() != ecr->up()) && (sc->up() == item->up())) {
                    item->stem_floated().left = true;
                    // start and end chord have opposite direction
                    // and slur direction is same as start chord
                    // (so slur starts on stem side)

                    // float the start point along the stem to follow direction of movement
                    // see for example Gould p. 111

                    // get position of note on slur side for start & end chords
                    Note *n1 = sc->up() ? sc->up_note() : sc->down_note();
                    Note *n2 = 0;
                    if (ec) {
                        n2 = ec->up() ? ec->up_note() : ec->down_note();
                    }

                    // differential in note positions
                    double yd = (n2 ? n2->pos().y() : ecr->pos().y()) - n1->pos().y();
                    yd *= .5;

                    // float along stem according to differential
                    double sh = stem1 ? stem1->height() : 0.0;
                    if (item->up() && yd < 0.0) {
                        po.ry() = std::max(po.y() + yd, sc->down_note()->pos().y() - sh - spatium_value);
                    } else if (!item->up() && yd > 0.0) {
                        po.ry() = std::min(po.y() + yd, sc->up_note()->pos().y() + sh + spatium_value);
                    }

                    // account for articulations
                    fix_articulations(item, po, sc, up_value, true);

                    // we may wish to force end to align to stem as well,
                    // if it is in same direction
                    // (but it won't be, so this assignment should have no effect)
                    stem_pos = true;
                } else {
                    // avoid articulations
                    fix_articulations(item, po, sc, up_value, sc->up() == item->up());
                }
            }
        } else if (sc) {
            // avoid articulations
            fix_articulations(item, po, sc, up_value, sc->up() == item->up());
        }

        // TODO: offset start position if there is another slur ending on this cr

        if (sa1 == SlurAnchor::NONE) {
            sp->p1 += po;
        }

        //------p2
        if (sa2 == SlurAnchor::NONE) {
            // default positions
            po.rx() = hw2 * .5 + (note2 ? note2->bbox_x_shift() : 0.0);
            if (note2) {
                po.ry() = note2->pos().y();
            } else if (item->up()) {
                po.ry() = item->end_cr()->ldata()->bbox().top();
            } else {
                po.ry() = item->end_cr()->ldata()->bbox().top() + item->end_cr()->height();
            }
            double offset2 = use_tablature ? 0.75 : 0.9;
            po.ry() += ecr->intrinsic_mag() * spatium_value * offset2 * up_value;

            // adjustments for stem and/or beam
            TremoloTwoChord *trem2 = ec ? ec->tremolo_two_chord() : nullptr;
            if (stem2 || trem2) { //ec can't be null
                Beam *beam2 = ec->beam();
                if ((stem_pos && (scr->up() == ec->up())) ||
                    (beam2 && (!beam2->elements().empty()) && (beam2->elements().front() != ec) && (ec->up() == item->up()) && sc &&
                     (sc->note_type() == NoteType::NORMAL)) ||
                    (trem2 && ec->up() == item->up())) {
                    if (beam2) {
                        TLayout::layout_beam(beam2, ctx);
                    }
                    if (trem2) {
                        TremoloLayout::layout(trem2, ctx);
                    }
                    // slur start was laid out to stem and start and end have same direction
                    // OR
                    // end chord is beamed but not the first chord of beam group
                    // and slur direction is same as end chord (stem side)
                    // and start chordrest is not a grace chord

                    // in these cases, layout end of slur to stem
                    double beam_width_sp = beam2 ? ctx.conf().style_s(Sid::BEAM_WIDTH).val() : 0;
                    Note *note = item->up() ? ec->up_note() : ec->down_note();
                    double stem_height = stem2 ? stem2->length() + (beam_width_sp / 2) : default_stem_length_end(trem2);
                    double offset3 = std::max(beam_clearance * ec->intrinsic_mag(), min_offset) * spatium_value;
                    double sh = stem_height + offset3;

                    if (item->up()) {
                        po.ry() = StemLayout::stem_pos(ec).y() - ec->page_pos().y() - sh;
                    } else {
                        po.ry() = StemLayout::stem_pos(ec).y() - ec->page_pos().y() + sh;
                    }
                    if (!stem2) {
                        // tremolo whole notes
                        po.set_x(note->notehead_center_x());
                    } else {
                        po.set_x(StemLayout::stem_pos_x(ec) + (stem2->line_width_mag() / 2 * up_value) -
                                 (beam_anchor_inset * spatium_value * ec->intrinsic_mag()));
                    }

                    // account for articulations
                    fix_articulations(item, po, ec, up_value, true);
                } else {
                    // slur was not aligned to stem or start and end have different direction
                    // AND
                    // end chord is not beamed or is first chord of beam group
                    // or slur direction is opposite that of end chord

                    // if stem and slur are both down,
                    // we need to clear stem horizontally
                    double stem_offset_mag = stem_offset_x * ec->intrinsic_mag();
                    if (!ec->up() && !item->up()) {
                        // stems in tab staves come from the middle of the head, which means it's much easier
                        // to just subtract an offset from the notehead center (which po already is)
                        if (use_tablature) {
                            po.rx() -= stem_offset_mag * spatium_value;
                        } else {
                            po.rx() = -spatium_value * stem_offset_mag + note2->x();
                        }
                    } else if (use_tablature && item->up() && ec->up()) {
                        // same as above
                        po.rx() -= spatium_value * stem_offset_mag;
                    }

                    //
                    // handle case: stem up   - stem down
                    //              stem down - stem up
                    //
                    if ((scr->up() != ec->up()) && (ec->up() == item->up())) {
                        item->stem_floated().right = true;
                        // start and end chord have opposite direction
                        // and slur direction is same as end chord
                        // (so slur end on stem side)

                        // float the end point along the stem to follow direction of movement
                        // see for example Gould p. 111

                        Note *n1 = 0;
                        if (sc) {
                            n1 = sc->up() ? sc->up_note() : sc->down_note();
                        }
                        Note *n2 = ec->up() ? ec->up_note() : ec->down_note();

                        double yd = n2->pos().y() - (n1 ? n1->pos().y() : item->start_cr()->pos().y());
                        yd *= .5;

                        double mh = stem2 ? stem2->height() : 0.0;
                        if (item->up() && yd > 0.0) {
                            po.ry() = std::max(po.y() - yd, ec->down_note()->pos().y() - mh - spatium_value);
                        } else if (!item->up() && yd < 0.0) {
                            po.ry() = std::min(po.y() - yd, ec->up_note()->pos().y() + mh + spatium_value);
                        }

                        // account for articulations
                        fix_articulations(item, po, ec, up_value, true);
                    } else {
                        // avoid articulations
                        fix_articulations(item, po, ec, up_value, ec->up() == item->up());
                    }
                }
            } else if (ec) {
                // avoid articulations
                fix_articulations(item, po, ec, up_value, ec->up() == item->up());
            }
            // TODO: offset start position if there is another slur ending on this cr
            sp->p2 += po;
        }
    }

    if (item->staff_type()->is_tab_staff()) {
        SlurTieLayout::avoid_pre_bends_on_tab(sc, ec, sp);
    }

    if (item->is_tapping_half_slur()) {
        adjust_for_tapping_half_slurs(to_tapping_half_slur(item), sp, note2);
    }
}

void SlurTieLayout::adjust_for_tapping_half_slurs(TappingHalfSlur *item, SlurTiePos *sp, Note *end_note) {
    if (item->staff_type()->is_tab_staff()) {
        return;
    }

    int staff_lines = end_note->staff()->lines(end_note->tick());
    bool note_is_inside_staff = end_note->line() > 1 && end_note->line() < 2 * (staff_lines - 1) - 1;
    bool note_is_on_space = end_note->line() % 2 != 0;
    if (note_is_inside_staff && note_is_on_space) {
        sp->p2.ry() += (item->up() ? 0.2 : -0.2) * item->spatium();
    }
}

void SlurTieLayout::fix_articulations(Slur *item, PointF &pt, Chord *c, double up, bool stem_side) {
    //
    // handle special case of tenuto and staccato
    // yo = current offset of slur from chord position
    // return unchanged position, or position of outmost "close" articulation
    //
    double slur_tip_to_artic_vert_dist = c->spatium() * 0.5 * up;
    double slur_tip_inward_adjust = 0.1 * item->spatium();
    bool start = item->start_cr() && item->start_cr() == c;
    bool end = item->end_cr() && item->end_cr() == c;
    for (Articulation *a : c->articulations()) {
        if (!a->layout_close_to_note() || !a->add_to_skyline()) {
            continue;
        }
        // skip if articulation on stem side but slur is not or vice versa
        if ((a->up() == c->up()) != stem_side) {
            continue;
        }
        // Correct x-position inwards
        Note *note = c->up() ? c->down_note() : c->up_note();
        pt.rx() = a->x() + 0.5 * a->width() - note->x();
        if (start) {
            pt.rx() += slur_tip_inward_adjust;
        } else if (end) {
            pt.rx() -= slur_tip_inward_adjust;
        }
        // Adjust y-position
        if (a->up()) {
            pt.ry() = std::min(pt.y(), a->y() + a->height() / 2 * up + slur_tip_to_artic_vert_dist);
        } else {
            pt.ry() = std::max(pt.y(), a->y() + a->height() / 2 * up + slur_tip_to_artic_vert_dist);
        }
    }
}

void SlurTieLayout::adjust_end_points(SlurSegment *slur_seg) {
    double spatium = slur_seg->spatium();
    double lw = (slur_seg->staff_type() ? slur_seg->staff_type()->line_distance().val() : 1.0) * spatium;
    const double staff_line_margin = 0.175 + (0.5 * slur_seg->style().style_s(Sid::STAFF_LINE_WIDTH).val() * (spatium / lw));
    PointF p1 = slur_seg->ups(Grip::START).p;
    PointF p2 = slur_seg->ups(Grip::END).p;

    double y1sp = p1.y() / lw;
    double y2sp = p2.y() / lw;

    // point 1
    const Staff *staff = slur_seg->staff();
    int lines = staff ? staff->lines(slur_seg->tick()) : 0;
    auto adjust_point = [staff_line_margin](bool up, double ysp) {
        double y1offset = ysp - floor(ysp);
        double adjust = 0;
        if (up) {
            if (y1offset < staff_line_margin) {
                // endpoint too close to the line above
                adjust = -(y1offset + staff_line_margin);
            } else if (y1offset > 1 - staff_line_margin) {
                // endpoint too close to the line below
                adjust = -(y1offset - (1 - staff_line_margin));
            }
        } else {
            if (y1offset < staff_line_margin) {
                // endpoint too close to the line above
                adjust = staff_line_margin - y1offset;
            }
            if (y1offset > 1 - staff_line_margin) {
                // endpoint too close to the line below
                adjust = (1 - y1offset) + staff_line_margin;
            }
        }
        return adjust;
    };
    if (y1sp > -staff_line_margin && y1sp < (lines - 1) + staff_line_margin) {
        slur_seg->ups(Grip::START).p.ry() += adjust_point(slur_seg->slur()->up(), y1sp) * lw;
    }
    if (y2sp > -staff_line_margin && y2sp < (lines - 1) + staff_line_margin) {
        slur_seg->ups(Grip::END).p.ry() += adjust_point(slur_seg->slur()->up(), y2sp) * lw;
    }
}

void SlurTieLayout::avoid_collisions(SlurSegment *slur_seg, PointF &pp1, PointF &p2, PointF &p3, PointF &p4,
                                     Transform &to_system_coordinates, double &slur_angle) {
    Slur *slur = slur_seg->slur();
    double spatium = slur_seg->spatium();
    double slur_length = std::abs(p2.x() / spatium);
    bool slur_up = slur->up();
    int up_sign = slur_up ? -1 : 1;

    ChordRest *start_cr = slur->start_cr();
    ChordRest *end_cr = slur->end_cr();

    if (!start_cr || !end_cr) {
        return;
    }

    Shape seg_shapes = get_segment_shapes(slur_seg, start_cr, end_cr); // Shape of the music under this slur
    add_min_clearance_to_shapes(seg_shapes, spatium, slur_up, start_cr, end_cr);

    if (seg_shapes.empty()) {
        return;
    }

    const double arc_clearance =
        -up_sign * compute_arc_clearance(spatium, slur_length, slur_angle); // Collision clearance at the center of the slur

    // balance: determines how much endpoint adjustment VS shape adjustment we will do.
    // 0 = end point is fixed, only the shape can be adjusted,
    // 1 = shape is fixed, only end the point can be adjusted.
    double left_balance = 0.0;
    double right_balance = 0.0;
    compute_adjustment_balance(slur_seg, start_cr, end_cr, left_balance, right_balance);

    static constexpr unsigned max_iter = 30; // Max iterations allowed

    double step = compute_adjustment_step(up_sign, spatium, slur_length);

    // Divide slur in several rectangles to localize collisions
    const unsigned npoints = 20;
    std::vector<RectF> slur_rects;
    slur_rects.reserve(npoints);

    // Define separate collision areas (left-mid-center)
    struct SlurCollision {
        bool left = false;
        bool mid = false;
        bool right = false;

        void reset() {
            left = false;
            mid = false;
            right = false;
        }
    };

    SlurCollision collision;

    // CHECK FOR COLLISIONS
    unsigned iter = 0;
    do {
        collision.reset();
        // Update tranform because pp1 may change
        to_system_coordinates.reset();
        to_system_coordinates.translate(pp1.x(), pp1.y());
        to_system_coordinates.rotate_radians(slur_angle);
        // Create rectangles
        slur_rects.clear();
        CubicBezier clearance_bezier(PointF(0, 0), p3 + PointF(0.0, arc_clearance), p4 + PointF(0.0, arc_clearance), p2);
        for (unsigned i = 0; i < npoints - 1; i++) {
            PointF clearance_point1 = clearance_bezier.point_at_percent(double(i) / double(npoints));
            PointF clearance_point2 = clearance_bezier.point_at_percent(double(i + 1) / double(npoints));
            clearance_point1 = to_system_coordinates.map(clearance_point1);
            clearance_point2 = to_system_coordinates.map(clearance_point2);
            slur_rects.push_back(RectF(clearance_point1, clearance_point2));
        }
        // Check collisions
        for (unsigned i = 0; i < slur_rects.size(); i++) {
            bool left_section = i < slur_rects.size() / 3;
            bool mid_section = i >= slur_rects.size() / 3 && i < 2 * slur_rects.size() / 3;
            bool right_section = i >= 2 * slur_rects.size() / 3;
            if ((left_section && collision.left) || (mid_section && collision.mid) ||
                (right_section && collision.right)) { // If a collision is already found in this section, no need to check again
                continue;
            }
            bool intersection =
                slur->up() ? !Shape(slur_rects[i]).clears_vertically(seg_shapes) : !seg_shapes.clears_vertically(slur_rects[i]);
            if (intersection) {
                if (left_section) {
                    collision.left = true;
                }
                if (mid_section) {
                    collision.mid = true;
                }
                if (right_section) {
                    collision.right = true;
                }
            }
        }

        // In the even iterations, adjust the shape
        if (iter % 2 == 0) {
            static constexpr double shape_prefer_center_factor = 2.0;
            if (collision.left) {
                double shape_left_step = (1 - left_balance) * step / shape_prefer_center_factor;
                // Move left Bezier point up(/down) and outwards
                p3 += PointF(-abs(shape_left_step), shape_left_step);
                // and a bit also the right point to compensate asymmetry
                p4 += PointF(std::abs(shape_left_step), shape_left_step) / 2.0;
            }
            if (collision.mid) { // Move both Bezier points up(/down)
                double shape_left_step = (1 - left_balance) * step;
                double shape_right_step = (1 - right_balance) * step;
                p3 += PointF(0.0, (shape_left_step + shape_right_step) / 2);
                p4 += PointF(0.0, (shape_left_step + shape_right_step) / 2);
            }
            if (collision.right) {
                double shape_right_step = (1 - right_balance) * step / shape_prefer_center_factor;
                // Move right Bezier point up(/down) and outwards
                p4 += PointF(std::abs(shape_right_step), shape_right_step);
                // and a bit also the left point to compensate asymmetry
                p3 += PointF(-abs(shape_right_step), shape_right_step) / 2.0;
            }
        } else if (!slur_seg->is_end_points_edited()) {
            // In the odd iterations, adjust the end points
            // Slurs steeper than 45° are gently compensated
            static constexpr double steep_limit = M_PI / 4;
            // If the collision is in mid region, move more the endpoint which reduces the slur slant
            static constexpr double slant_reduction_angle = 5 * M_PI / 180;
            static constexpr double slant_reduction_length = 8;
            PointF p2_sys_coords = to_system_coordinates.map(p2);
            double cur_slant = (p2_sys_coords.y() - pp1.y()) / (p2_sys_coords.x() - pp1.x());
            if (collision.left || collision.mid || slur_angle < -steep_limit) {
                double end_point_left_step = left_balance * step;
                if (!collision.left && collision.mid && std::abs(cur_slant) > slant_reduction_angle &&
                    slur_length > slant_reduction_length) {
                    double slant_reduction_ratio = std::abs(cur_slant) / slant_reduction_angle;
                    bool left_is_outer = (slur_up && cur_slant > 0) || (!slur_up && cur_slant < 0);
                    end_point_left_step *= left_is_outer ? (1 / slant_reduction_ratio) : slant_reduction_ratio;
                }
                // Lift the left end point, i.e. tilt the slur around p2
                double step_x = sin(slur_angle) * end_point_left_step;
                double step_y = cos(slur_angle) * end_point_left_step;
                PointF pp1delta = PointF(step_x, step_y);
                pp1 += PointF(0.0, end_point_left_step);
                p3 += pp1delta * (p2.x() - p3.x()) / p2.x();
                p4 += pp1delta * (p2.x() - p4.x()) / p2.x();
                // All points are expressed with respect to pp1, so we need
                // to subtract pp1delta to avoid the whole slur moving up
                p2 -= pp1delta;
                p3 -= pp1delta;
                p4 -= pp1delta;
            }
            if (collision.right || collision.mid || slur_angle > steep_limit) {
                double end_point_right_step = right_balance * step;
                if (!collision.right && collision.mid && std::abs(cur_slant) > slant_reduction_angle &&
                    slur_length > slant_reduction_length) {
                    double slant_reduction_ratio = std::abs(cur_slant) / slant_reduction_angle;
                    bool right_is_outer = (slur_up && cur_slant < 0) || (!slur_up && cur_slant > 0);
                    end_point_right_step *= right_is_outer ? (1 / slant_reduction_ratio) : slant_reduction_ratio;
                }
                // Lift the right end point, i.e. tilt the slur around p1
                double step_x = sin(slur_angle) * end_point_right_step;
                double step_y = cos(slur_angle) * end_point_right_step;
                PointF p2delta = PointF(step_x, step_y);
                p2 += p2delta;
                p3 += p2delta * p3.x() / p2.x();
                p4 += p2delta * p4.x() / p2.x();
            }
        }
        // Enforce non-ugliness rules
        // 1) Slur cannot be taller than it is wide
        const double max_relative_height = std::abs(p2.x());
        p3 = slur->up() ? PointF(p3.x(), std::max(p3.y(), -max_relative_height)) : PointF(p3.x(), std::min(p3.y(), max_relative_height));
        p4 = slur->up() ? PointF(p4.x(), std::max(p4.y(), -max_relative_height)) : PointF(p4.x(), std::min(p4.y(), max_relative_height));
        // 2) Tangent rule: p3 and p4 cannot be further left than p1 nor further right than p2
        PointF p3_sys_coord = to_system_coordinates.map(p3);
        PointF p4_sys_coord = to_system_coordinates.map(p4);
        PointF p2_sys_coord = to_system_coordinates.map(p2);
        p3_sys_coord = PointF(std::max(pp1.x(), p3_sys_coord.x()), p3_sys_coord.y());
        p3_sys_coord = PointF(std::min(p2_sys_coord.x(), p3_sys_coord.x()), p3_sys_coord.y());
        p4_sys_coord = PointF(std::max(pp1.x(), p4_sys_coord.x()), p4_sys_coord.y());
        p4_sys_coord = PointF(std::min(p2_sys_coord.x(), p4_sys_coord.x()), p4_sys_coord.y());
        p3 = to_system_coordinates.inverted().map(p3_sys_coord);
        p4 = to_system_coordinates.inverted().map(p4_sys_coord);

        ++iter;
    } while ((collision.left || collision.mid || collision.right) && iter < max_iter);
}

Shape SlurTieLayout::get_segment_shapes(SlurSegment *slur_seg, ChordRest *start_cr, ChordRest *end_cr) {
    Shape seg_shapes;

    // Determine start and end segments for collision checks
    Segment *start_seg = nullptr;
    if (slur_seg->is_single_begin_type()) {
        if (start_cr->is_chord() && to_chord(start_cr)->is_grace_after()) {
            // if this is a grace-note-after, the shape is stored the *appended* segment
            Chord *parent = to_chord(start_cr->ownership_parent());
            if (parent) {
                start_seg = parent->grace_notes_after().appended_segment();
            }
        } else {
            start_seg = start_cr->segment(); // first of the slur
        }
    } else {
        start_seg = slur_seg->system()->first_measure()->find_first_r(SegmentType::CHORD_REST, Fraction(0, 0)); // first of the system
    }

    Segment *end_seg = nullptr;
    if (slur_seg->is_single_end_type()) {
        if (end_cr->is_chord() && to_chord(end_cr)->is_grace_after()) {
            // if this is a grace-note-after, the shape is stored the *appended* segment
            Chord *parent = to_chord(end_cr->ownership_parent());
            if (parent) {
                end_seg = parent->grace_notes_after().appended_segment();
            }
        } else {
            end_seg = end_cr->segment(); // last of the slur
        }
    } else {
        end_seg = slur_seg->system()->last_measure()->last(); // last of the system
    }

    if (!start_seg || !end_seg) {
        return seg_shapes;
    }

    for (Segment *seg = start_seg; seg && (seg->is_before(end_seg) || seg == end_seg); seg = seg->next1enabled()) {
        if (seg->is_type(SegmentType::BAR_LINE_TYPES) || seg->is_breath_type() || seg->has_time_sig_above_staves() ||
            seg->is_time_tick_type()) {
            continue;
        }
        seg_shapes.add(get_segment_shape(slur_seg, seg, start_cr, end_cr));
    }

    return seg_shapes;
}

Shape SlurTieLayout::get_segment_shape(SlurSegment *slur_seg, Segment *seg, ChordRest *start_cr, ChordRest *end_cr) {
    Slur *slur = slur_seg->slur();
    staff_idx_t start_staff_idx = start_cr->staff_idx();
    staff_idx_t end_staff_idx = end_cr->staff_idx();
    Shape seg_shape = seg->staff_shape(start_staff_idx).translated(seg->pos() + seg->measure()->pos());

    bool cross_staff_slur = slur->is_cross_staff() && seg != start_cr->segment();

    // If cross-staff, also add the shape of second staff
    if (cross_staff_slur) {
        end_staff_idx = (end_cr->staff_idx() != start_staff_idx) ? end_cr->staff_idx() : end_cr->v_staff_idx();
        SysStaff *start_staff = slur_seg->system()->staves().at(start_staff_idx);
        SysStaff *end_staff = slur_seg->system()->staves().at(end_staff_idx);
        double dist = end_staff->y() - start_staff->y();
        Shape second_staff_shape = seg->staff_shape(end_staff_idx).translated(seg->pos() + seg->measure()->pos()); // translate horizontally
        second_staff_shape.translate(PointF(0.0, dist));                                                           // translate vertically
        seg_shape.add(second_staff_shape);
    }

    for (track_idx_t track = staff2track(start_staff_idx); track < staff2track(end_staff_idx, ::mu::engraving::voices); ++track) {
        EngravingItem *e = seg->element(track);
        if (!e || !e->is_chord_rest()) {
            continue;
        }
        if (e->v_staff_idx() != slur_seg->v_staff_idx() && !cross_staff_slur) {
            continue;
        }
        // Gets tie and 2 note tremolo shapes
        if (e->is_chord()) {
            Chord *chord = to_chord(e);
            if (chord->tremolo_two_chord()) {
                seg_shape.add(chord->tremolo_two_chord()->shape());
            }
            for (Note *note : to_chord(e)->notes()) {
                Tie *tie_for = note->tie_for();
                Tie *tie_back = note->tie_back();
                if (tie_for && tie_for->up() == slur->up() && !tie_for->segments_empty()) {
                    TieSegment *tie_segment = tie_for->front_segment();
                    if (tie_segment->is_single_begin_type()) {
                        seg_shape.add(tie_segment->shape());
                    }
                }
                if (tie_back && tie_back->up() == slur->up() && !tie_back->segments_empty()) {
                    TieSegment *tie_segment = tie_back->back_segment();
                    if (tie_segment->is_end_type()) {
                        seg_shape.add(tie_segment->shape());
                    }
                }
            }
        }
    }

    // Remove items that the slur shouldn't try to avoid
    seg_shape.remove_if([&](ShapeElement &shape_el) {
        if (!shape_el.item() || !shape_el.item()->ownership_parent() || !shape_el.item()->visible()) {
            return true;
        }
        const EngravingItem *item = shape_el.item();
        const EngravingObject *parent = item->ownership_parent();
        // Don't remove arpeggio starting on a different voice and ending on the same voice as endCR when slur is on the outside
        if ((item->is_arpeggio() || item->is_chord_bracket()) && (end_cr->track() == to_arpeggio(item)->end_track()) &&
            end_cr->tick() == item->tick() && (!slur->up() && to_arpeggio(item)->span() > 1)) {
            return false;
        }

        // Its own startCR or items belonging to it, lyrics, fingering, ledger lines, articulation on endCR
        if (item == start_cr || parent == start_cr || item->is_text_base() || item->is_ledger_line() ||
            (item->is_articulation_family() && parent == end_cr) || item->is_bend() || item->is_tapping_half_slur_segment()) {
            return true;
        }
        // Ornament accidentals on start or end chord
        if (item->is_accidental() && parent->is_ornament()) {
            const EngravingObject *parent_parent = parent->ownership_parent();
            if (parent_parent && (parent_parent == start_cr || parent_parent == end_cr)) {
                return true;
            }
        }
        // Items that are on the start segment but in a different voice
        if ((item->tick() == start_cr->tick() && item->track() != start_cr->track()) ||
            (item->tick() == end_cr->tick() && item->track() != end_cr->track())) {
            return true;
        }
        // Edge-case: multiple voices and slur is on the inside
        if (item->v_staff_idx() == start_cr->staff_idx() &&
            ((!slur->up() && item->track() > start_cr->track())       // slur-down: ignore lower voices
             || (slur->up() && item->track() < start_cr->track()))) { // slur-up: ignore higher voices
            return true;
        }
        // Remove arpeggios spanning more than 1 voice starting on endCR's voice when the slur is on the inside
        if ((item->is_arpeggio() || item->is_chord_bracket()) &&
            (end_cr->track() != item->track() || (!slur->up() && to_arpeggio(item)->span() > 1))) {
            return true;
        }
        // Ignore big time signatures
        if (item->is_time_sig() && to_time_sig(item)->time_sig_placement() != TimeSigPlacement::NORMAL) {
            return true;
        }
        // Ignore fermatas
        if (item->is_fermata()) {
            return true;
        }
        // Ignore fret diagrams
        if (item->is_fret_diagram()) {
            return true;
        }

        if (item->is_note() && to_note(item)->should_hide_fret()) {
            return true;
        }

        return false;
    });

    return seg_shape;
}

void SlurTieLayout::add_min_clearance_to_shapes(Shape &seg_shapes, double spatium, bool slur_up, const ChordRest *start_cr,
                                                const ChordRest *end_cr) {
    const double note_clearance = 0.4 * spatium;
    const double articulation_clearance = 0.20 * spatium;
    const double item_clearance = 0.1 * spatium;

    for (ShapeElement &shape_el : seg_shapes.elements()) {
        const EngravingItem *item = shape_el.item();
        if (!item || item->parent() == start_cr || item->parent() == end_cr) {
            continue;
        }

        double clearance = 0.0;
        switch (item->type()) {
            case ElementType::NOTE: clearance = note_clearance; break;
            case ElementType::ARTICULATION:
            case ElementType::ORNAMENT:
            case ElementType::TAPPING: clearance = articulation_clearance; break;
            default: clearance = item_clearance; break;
        }

        slur_up ? shape_el.adjust(0.0, -clearance, 0.0, 0.0) : shape_el.adjust(0.0, 0.0, 0.0, clearance);
    }
}

double SlurTieLayout::compute_arc_clearance(double spatium, double slur_length, double slur_angle) {
    static constexpr double clearance_to_length_ratio = 0.04;
    static constexpr double max_clearance = 0.5;
    static constexpr double min_clearance = 0.1;

    double clearance = clearance_to_length_ratio * slur_length * std::pow(cos(slur_angle), 2);
    clearance = std::clamp(clearance, min_clearance, max_clearance);

    return clearance * spatium;
}

void SlurTieLayout::compute_adjustment_balance(SlurSegment *slur_seg, const ChordRest *start_cr, const ChordRest *end_cr,
                                               double &left_balance, double &right_balance) {
    Slur *slur = slur_seg->slur();
    if (slur_seg->is_single_begin_type() && !slur->stem_floated().left) {
        if (slur_seg->is_begin_type() || has_articulation_above(slur_seg, start_cr) || slur->is_cross_staff()) {
            left_balance = 0.1;
        } else {
            left_balance = 0.4;
        }
    } else {
        left_balance = 0.9;
    }

    if (slur_seg->is_single_end_type() && !slur->stem_floated().right) {
        if (slur_seg->is_end_type() || has_articulation_above(slur_seg, end_cr) || slur->is_cross_staff()) {
            right_balance = 0.1;
        } else {
            right_balance = 0.4;
        }
    } else {
        right_balance = 0.9;
    }
}

bool SlurTieLayout::has_articulation_above(SlurSegment *slur_seg, const ChordRest *chord_rest) {
    IF_ASSERT_FAILED(chord_rest) { return false; }

    if (!chord_rest->is_chord()) {
        return false;
    }

    const Chord *chord = to_chord(chord_rest);

    bool slur_up = slur_seg->slur()->up();
    for (const Articulation *artic : chord->articulations()) {
        if (!artic->layout_close_to_note() && artic->up() == slur_up) {
            return true;
        }
    }

    if (Ornament *ornament = chord->find_ornament()) {
        if (ornament->up() == slur_up) {
            return true;
        }
    }

    return false;
}

double SlurTieLayout::compute_adjustment_step(int up_sign, double spatium, double slur_length) {
    double step = up_sign * 0.30 * spatium;

    static constexpr double long_slur_limit = 16.0; // in spaces
    if (slur_length > long_slur_limit) {
        step *= slur_length / long_slur_limit;
        step = std::min(step, 1.5 * spatium);
    }

    return step;
}

bool SlurTieLayout::stem_side_for_beam(Slur *slur, bool start) {
    // determines if the anchor point is exempted from the stem inset due to beams or tremolos.
    if (!slur) {
        return false;
    }

    ChordRest *cr = start ? slur->start_cr() : slur->end_cr();
    Chord *c = to_chord(cr);
    bool adjust_for_beam = cr && cr->beam() && cr->up() == slur->up();
    if (start) {
        adjust_for_beam = adjust_for_beam && cr->beam()->elements().back() != cr;
    } else {
        adjust_for_beam = adjust_for_beam && cr->beam()->elements().front() != cr;
    }
    if (adjust_for_beam) {
        return true;
    }

    bool adjust_for_trem = false;
    TremoloTwoChord *trem = c ? c->tremolo_two_chord() : nullptr;
    adjust_for_trem = trem && trem->up() == slur->up();
    if (start) {
        adjust_for_trem = adjust_for_trem && trem->chord2() != c;
    } else {
        adjust_for_trem = adjust_for_trem && trem->chord1() != c;
    }
    return adjust_for_trem;
}

bool SlurTieLayout::is_over_beams(Slur *slur) {
    // returns true if all the chords spanned by the slur are beamed, and all beams are on the same side of the slur
    const ChordRest *start_cr = slur->start_cr();
    const ChordRest *end_cr = slur->end_cr();
    if (!start_cr || !end_cr) {
        return false;
    }
    if (start_cr->track() != end_cr->track() || start_cr->tick() >= end_cr->tick()) {
        return false;
    }
    size_t track = start_cr->track();
    Segment *seg = start_cr->segment();
    while (seg && seg->tick() <= end_cr->tick()) {
        if (!seg->is_chord_rest_type() || !seg->elist().at(track) || !seg->elist().at(track)->is_chord_rest()) {
            return false;
        }
        ChordRest *cr = to_chord_rest(seg->elist().at(track));
        bool has_beam = cr->beam() && cr->up() == slur->up();
        bool has_trem = false;
        if (cr->is_chord()) {
            Chord *c = to_chord(cr);
            has_trem = c->tremolo_two_chord() && c->up() == slur->up();
        }
        if (!(has_beam || has_trem)) {
            return false;
        }
        if ((!seg->next() || seg->next()->is_end_bar_line_type()) && seg->measure()->next_measure()) {
            seg = seg->measure()->next_measure()->first();
        } else {
            seg = seg->next();
        }
    }
    return true;
}

void SlurTieLayout::avoid_pre_bends_on_tab(const Chord *sc, const Chord *ec, SlurTiePos *sp) {
    GuitarBend *bend_on_start = nullptr;
    GuitarBend *bend_on_end = nullptr;
    if (sc) {
        for (Note *note : sc->notes()) {
            GuitarBend *bf = note->bend_for();
            GuitarBend *bb = note->bend_back();
            if (bf && !bf->segments_empty() && bf->bend_type() == GuitarBendType::PRE_BEND && !bf->angled_pre_bend()) {
                bend_on_start = bf;
            } else if (bb && !bb->segments_empty() && bb->bend_type() == GuitarBendType::PRE_BEND && !bb->angled_pre_bend()) {
                bend_on_start = bb;
            }
            if (bend_on_start) {
                break;
            }
        }
    }
    if (ec) {
        for (Note *note : ec->notes()) {
            GuitarBend *bf = note->bend_for();
            GuitarBend *bb = note->bend_back();
            if (bf && !bf->segments_empty() && bf->bend_type() == GuitarBendType::PRE_BEND && !bf->angled_pre_bend()) {
                bend_on_end = bf;
            } else if (bb && !bb->segments_empty() && bb->bend_type() == GuitarBendType::PRE_BEND && !bb->angled_pre_bend()) {
                bend_on_end = bb;
            }
            if (bend_on_end) {
                break;
            }
        }
    }

    if (bend_on_start) {
        sp->p1.rx() = std::max(sp->p1.x(), bend_on_start->front_segment()->pos().x() + 0.33 * sc->spatium());
    }
    if (bend_on_end) {
        sp->p2.rx() = std::min(sp->p2.x(), bend_on_end->front_segment()->pos().x() - 0.33 * ec->spatium());
    }
}

bool SlurTieLayout::tie_segment_should_be_skipped(const Tie *item) {
    Note *start_note = item->start_note();
    StaffType *st = item->staff()->staff_type(start_note ? start_note->tick() : Fraction(0, 1));
    if (!st || !st->is_tab_staff()) {
        return false;
    }

    if (start_note && start_note->is_continuation_of_bend()) {
        return true;
    }

    ShowTiedFret show_tied_fret = item->style().value(Sid::TAB_SHOW_TIED_FRET).value<ShowTiedFret>();

    return show_tied_fret == ShowTiedFret::NONE;
}

TieSegment *SlurTieLayout::layout_tie_for(Tie *item, System *system) {
    item->set_pos(0, 0);

    if (!item->start_note()) {
        LOGD("no start note");
        return nullptr;
    }

    // do not layout ties in tablature if not showing back-tied fret marks
    if (tie_segment_should_be_skipped(item)) {
        if (!item->segments_empty()) {
            item->erase_spanner_segments();
        }

        return nullptr;
    }

    if (item->is_partial_tie()) {
        return layout_partial_tie(to_partial_tie(item));
    }

    calculate_direction(item);
    calculate_is_inside(item);

    SlurTiePos s_pos;
    s_pos.p1 = compute_default_start_or_end_point(item, Grip::START);

    compute_start_and_end_system(item, s_pos);

    int segment_count = s_pos.system1 == s_pos.system2 ? 1 : 2;
    if (segment_count == 2) {
        s_pos.p2 = PointF(ending_x_for_open_ended_slur_tie(system->last_measure(), item->start_note()->v_staff_idx()), s_pos.p1.y());
    } else {
        s_pos.p2 = compute_default_start_or_end_point(item, Grip::END);
    }

    correct_for_cross_staff(item, s_pos, s_pos.system1 != s_pos.system2 ? SpannerSegmentType::BEGIN : SpannerSegmentType::SINGLE);
    force_horizontal(item, s_pos);

    item->fixup_segments(segment_count);
    TieSegment *segment = item->segment_at(0);
    segment->set_track(item->track());
    segment->set_spanner_segment_type(s_pos.system1 != s_pos.system2 ? SpannerSegmentType::BEGIN : SpannerSegmentType::SINGLE);
    segment->move_to_system(system); // Needed to populate System.spannerSegments
    segment->reset_adjustment_offset();
    segment->mutldata()->all_jump_points_inactive = item->all_jump_points_inactive();

    const Chord *start_chord = item->start_note()->chord();
    item->set_tick(start_chord->tick()); // Why is this here?? (M.S.)

    if (segment->autoplace() && !segment->is_edited()) {
        adjust_x(segment, s_pos, Grip::START);
        if (segment->is_single_type()) {
            adjust_x(segment, s_pos, Grip::END);
        }
    }

    adjust_yfor_ledger_lines(segment, s_pos);

    segment->ups(Grip::START).p = s_pos.p1;
    segment->ups(Grip::END).p = s_pos.p2;

    if (segment->autoplace() && !segment->is_edited()) {
        adjust_y(segment);
    } else {
        compute_bezier(segment);
    }

    add_line_attach_points(segment); // add attach points to start and end note
    return segment;
}

TieSegment *SlurTieLayout::layout_tie_back(Tie *item, System *system, LayoutContext &ctx) {
    Chord *chord = item->end_note() ? item->end_note()->chord() : nullptr;

    if (item->staff_type() && item->staff_type()->is_tab_staff()) {
        // On TAB, the presence of this tie may require to add a parenthesis
        ChordLayout::layout(chord, ctx);
    }
    // do not layout ties in tablature if not showing back-tied fret marks
    if (tie_segment_should_be_skipped(item)) {
        if (!item->segments_empty()) {
            item->erase_spanner_segments();
        }

        return nullptr;
    }

    if (item->is_partial_tie()) {
        return layout_partial_tie(to_partial_tie(item));
    }

    SlurTiePos s_pos;
    compute_start_and_end_system(item, s_pos);
    s_pos.p2 = compute_default_start_or_end_point(item, Grip::END);

    double x = system ? system->first_note_rest_segment_x(true) : 0;
    double y = s_pos.p2.y();
    s_pos.p1 = PointF(x, y);

    item->fixup_segments(2);
    TieSegment *segment = item->segment_at(1);
    segment->set_track(item->track());
    segment->move_to_system(system);
    segment->reset_adjustment_offset();

    if (chord) {
        segment->set_staff_move(static_cast<int>(chord->v_staff_idx() - segment->staff_idx()));
    }

    adjust_y(segment);
    segment->set_spanner_segment_type(SpannerSegmentType::END);

    if (segment->autoplace() && !segment->is_edited()) {
        adjust_x(segment, s_pos, Grip::END);
    }

    adjust_yfor_ledger_lines(segment, s_pos);

    segment->ups(Grip::START).p = s_pos.p1;
    segment->ups(Grip::END).p = s_pos.p2;

    if (segment->autoplace() && !segment->is_edited()) {
        adjust_y(segment);
    } else {
        compute_bezier(segment);
    }

    add_line_attach_points(segment);
    return segment;
}

void SlurTieLayout::compute_start_and_end_system(Tie *item, SlurTiePos &slur_tie_pos) {
    Chord *start_chord = item->start_note() ? item->start_note()->chord() : nullptr;
    Chord *end_chord = item->end_note() ? item->end_note()->chord() : nullptr;

    System *start_system = start_chord ? start_chord->measure()->system() : nullptr;

    System *end_system = end_chord ? end_chord->measure()->system() : start_system;

    if (!start_system && !end_system) {
        if (start_chord) {
            Measure *m = start_chord->measure();
            LOGD("No system: measure is %d has %d count %d", m->is_mm_rest(), m->has_mm_rest(), m->mm_rest_count());
        } else {
            LOGD("No start or end system for tie at ") << item->tick().to_string();
        }
    }

    slur_tie_pos.system1 = start_system;
    slur_tie_pos.system2 = end_system;
}

PointF SlurTieLayout::compute_default_start_or_end_point(const Tie *tie, Grip start_or_end) {
    if (start_or_end != Grip::START && start_or_end != Grip::END) {
        return PointF();
    }

    bool start = start_or_end == Grip::START;

    Note *note = start ? tie->start_note() : tie->end_note();
    Chord *chord = note ? note->chord() : nullptr;

    if (!chord) {
        return PointF();
    }

    PointF result = note->pos() + chord->pos() + chord->segment()->pos() + chord->measure()->pos();

    const bool up = tie->up();
    const bool inside = tie->is_inside();
    const bool note_is_hidden_fret = note->should_hide_fret();
    const int up_sign = up ? -1 : 1;
    const int left_right_sign = start ? +1 : -1;
    const double note_width = note->width();
    const double note_height = note->height();
    const double spatium = tie->spatium();

    double base_x = (inside && !note_is_hidden_fret) ? (start ? note_width : 0.0) : note_optical_center_for_tie(note, up);
    double base_y = inside ? 0.0 : up_sign * note_height / 2;

    result += PointF(base_x, base_y);

    double visual_inset_sp = 0.0;
    if (inside || note->head_group() == NoteHeadGroup::HEAD_SLASH || note_is_hidden_fret) {
        visual_inset_sp = 0.2;
    } else if (note->has_another_straight_above_or_below(up)) {
        visual_inset_sp = 0.45;
    } else {
        visual_inset_sp = 0.1;
    }

    double visual_inset = visual_inset_sp * spatium * left_right_sign;
    const double y_offset = 0.20 * spatium * up_sign; // TODO: style

    result += PointF(visual_inset, y_offset);

    return result;
}

double SlurTieLayout::note_optical_center_for_tie(const Note *note, bool up) {
    if (note->head_group() == NoteHeadGroup::HEAD_SLASH) {
        double single_slash_width = note->sym_bbox(SymId::NOTEHEAD_SLASH_HORIZONTAL_ENDS).width();
        double note_width = note->head_width();
        double center = 0.20 * note->spatium() + 0.5 * (note_width - single_slash_width);
        return up ? note->ldata()->bbox().right() - center : note->ldata()->bbox().left() + center;
    }
    SymId sym_id = note->ldata()->cached_notehead_sym.value();
    PointF cut_out_left = note->sym_smufl_anchor(sym_id, up ? SmuflAnchorId::CUT_OUT_NW : SmuflAnchorId::CUT_OUT_SW);
    PointF cut_out_right = note->sym_smufl_anchor(sym_id, up ? SmuflAnchorId::CUT_OUT_NE : SmuflAnchorId::CUT_OUT_SE);

    if (cut_out_left.is_null() || cut_out_right.is_null()) {
        return 0.5 * note->width();
    }

    return 0.5 * (cut_out_left.x() + cut_out_right.x());
}

void SlurTieLayout::create_slur_segments(Slur *item, LayoutContext &ctx) {
    const ChordRest *start_cr = item->start_cr();
    const ChordRest *end_cr = item->end_cr();
    assert(start_cr && end_cr);

    const System *start_sys = start_cr->measure()->system();
    const System *end_sys = end_cr->measure()->system();

    const std::vector<System *> &systems = ctx.dom().systems();
    system_idx_t start_sys_idx = muse::index_of(systems, start_sys);
    system_idx_t end_sys_idx = muse::index_of(systems, end_sys);
    if (start_sys_idx == muse::nidx || end_sys_idx == muse::nidx) {
        return;
    }

    int segments_needed = 0;
    for (system_idx_t i = start_sys_idx; i <= end_sys_idx; ++i) {
        if (systems.at(i)->vbox()) {
            continue;
        }
        ++segments_needed;
    }
    int current_segments = int(item->spanner_segments().size());

    if (current_segments != segments_needed) {
        item->fixup_segments(segments_needed);
    }

    int seg_idx = 0;
    for (system_idx_t i = start_sys_idx; i <= end_sys_idx; ++i) {
        System *system = systems.at(i);
        if (system->vbox()) {
            continue;
        }
        SlurSegment *line_segm = item->segment_at(seg_idx++);
        line_segm->move_to_system(system);
        if (start_sys_idx == end_sys_idx) {
            line_segm->set_spanner_segment_type(SpannerSegmentType::SINGLE);
        } else if (i == start_sys_idx) {
            line_segm->set_spanner_segment_type(SpannerSegmentType::BEGIN);
        } else if (i > 0 && i != end_sys_idx) {
            line_segm->set_spanner_segment_type(SpannerSegmentType::MIDDLE);
        } else if (i == end_sys_idx) {
            line_segm->set_spanner_segment_type(SpannerSegmentType::END);
        }
    }
}

void SlurTieLayout::adjust_overlapping_slurs(const std::list<SpannerSegment *> &spanner_segments) {
    std::vector<SlurSegment *> segments;
    for (SpannerSegment *seg : spanner_segments) {
        if (seg->is_slur_segment()) {
            segments.push_back(to_slur_segment(seg));
        }
    }
    if (segments.size() <= 1) {
        return;
    }

    //how far vertically an endpoint should adjust to avoid other slur endpoints:
    const double spatium = segments.front()->spatium();
    const double slur_collision_vert_offset = 0.65 * spatium;
    const double slur_collision_horiz_offset = 0.2 * spatium;
    const double fuzzy_horiz_compare = 0.25 * spatium;
    auto compare = [fuzzy_horiz_compare](double x1, double x2) { return std::abs(x1 - x2) < fuzzy_horiz_compare; };
    for (SlurSegment *slur1 : segments) {
        for (SlurSegment *slur2 : segments) {
            if (slur2 == slur1) {
                continue;
            }
            if (slur1->slur()->end_chord() == slur2->slur()->start_chord() &&
                compare(slur1->ups(Grip::END).p.y(), slur2->ups(Grip::START).p.y())) {
                slur1->ups(Grip::END).p.rx() -= slur_collision_horiz_offset;
                slur2->ups(Grip::START).p.rx() += slur_collision_horiz_offset;
                SlurTieLayout::compute_bezier(slur1);
                SlurTieLayout::compute_bezier(slur2);
                continue;
            }

            SlurTieSegment *slur_tie2 = to_slur_tie_segment(slur2);

            // slurs don't collide with themselves or slurs on other staves
            if (slur1->v_staff_idx() != slur_tie2->v_staff_idx()) {
                continue;
            }
            // slurs which don't overlap don't need to be checked
            if (slur1->ups(Grip::END).p.x() < slur_tie2->ups(Grip::START).p.x() ||
                slur_tie2->ups(Grip::END).p.x() < slur1->ups(Grip::START).p.x() || slur1->slur()->up() != slur_tie2->slur_tie()->up()) {
                continue;
            }
            // START POINT
            if (compare(slur1->ups(Grip::START).p.x(), slur_tie2->ups(Grip::START).p.x())) {
                if (slur1->ups(Grip::END).p.x() > slur_tie2->ups(Grip::END).p.x() || slur_tie2->is_tie_segment()) {
                    // slur1 is the "outside" slur
                    slur1->ups(Grip::START).p.ry() += slur_collision_vert_offset * (slur1->slur()->up() ? -1 : 1);
                    SlurTieLayout::compute_bezier(slur1);
                }
            }
            // END POINT
            if (compare(slur1->ups(Grip::END).p.x(), slur_tie2->ups(Grip::END).p.x())) {
                // slurs have the same endpoint
                if (slur1->ups(Grip::START).p.x() < slur_tie2->ups(Grip::START).p.x() || slur_tie2->is_tie_segment()) {
                    // slur1 is the "outside" slur
                    slur1->ups(Grip::END).p.ry() += slur_collision_vert_offset * (slur1->slur()->up() ? -1 : 1);
                    SlurTieLayout::compute_bezier(slur1);
                }
            }
        }
    }
}

LaissezVibSegment *SlurTieLayout::create_laissez_vib_segment(LaissezVib *item) {
    Chord *start_chord = item->start_note()->chord();
    item->set_tick(start_chord->tick());

    calculate_direction(item);
    calculate_is_inside(item);

    item->fixup_segments(1);
    LaissezVibSegment *segment = item->segment_at(0);
    segment->set_spanner_segment_type(SpannerSegmentType::SINGLE);
    segment->set_track(item->track());
    segment->move_to_system(item->start_note()->chord()->segment()->measure()->system());
    segment->reset_adjustment_offset();

    return segment;
}

void SlurTieLayout::calculate_laissez_vib_x(LaissezVibSegment *segment, SlurTiePos &s_pos, bool smufl) {
    LaissezVib *lv = segment->laissez_vib();

    compute_start_and_end_system(lv, s_pos);
    s_pos.p1 = compute_default_start_or_end_point(lv, Grip::START);

    correct_for_cross_staff(lv, s_pos, SpannerSegmentType::SINGLE);

    if (segment->autoplace() && !segment->is_edited()) {
        adjust_x(segment, s_pos, Grip::START);
    }

    if (smufl) {
        LaissezVibSegment::LayoutData *ldata = segment->mutldata();
        ldata->symbol = lv->sym_id();
        ldata->set_bbox(segment->sym_bbox(ldata->symbol));
        ldata->set_shape(Shape(ldata->bbox(), segment));
        ldata->set_mag(segment->laissez_vib()->start_note()->mag());
    }

    const double width = smufl ? segment->width() : lv->absolute_from_spatium(lv->min_length());

    s_pos.p2 = PointF(s_pos.p1.x() + width, s_pos.p1.y());
}

void SlurTieLayout::calculate_laissez_vib_y(LaissezVibSegment *segment, SlurTiePos &s_pos) {
    LaissezVib *lv = segment->laissez_vib();

    adjust_yfor_ledger_lines(segment, s_pos);

    Note *note = lv->start_note();
    Chord *chord = note->chord();

    const NoteParenthesisInfo *note_paren_info = chord->find_note_parenthesis_info(note);

    Parenthesis *paren = nullptr;
    if (note_paren_info) {
        paren = note_paren_info->left_paren();
    }

    const bool avoid_stem = chord->stem() && chord->stem()->visible() && chord->up() == lv->up();
    if (paren && (!lv->is_outer_tie_of_chord(Grip::START) || avoid_stem)) {
        RectF paren_bbox = paren->ldata()->bbox().translated(paren->system_pos());
        double adj = s_pos.p1.y() - (lv->up() ? paren_bbox.top() : paren_bbox.bottom());
        const double paren_lv_padding = 0.1 * segment->spatium();
        adj += paren_lv_padding * (lv->up() ? 1.0 : -1.0);
        s_pos.p1.ry() -= adj;
        s_pos.p2.ry() -= adj;
    }

    segment->ups(Grip::START).p = s_pos.p1;
    segment->ups(Grip::END).p = s_pos.p2;

    if (segment->autoplace() && !segment->is_edited()) {
        adjust_y(segment);
    } else {
        compute_bezier(segment);
    }
}

PartialTieSegment *SlurTieLayout::create_partial_tie_segment(PartialTie *item) {
    Chord *chord = item->is_outgoing() ? item->start_note()->chord() : item->end_note()->chord();
    item->set_tick(chord->tick());

    calculate_direction(item);
    calculate_is_inside(item);

    item->fixup_segments(1);
    PartialTieSegment *segment = item->segment_at(0);
    segment->set_spanner_segment_type(SpannerSegmentType::SINGLE);
    segment->move_to_system(chord->segment()->measure()->system());
    segment->set_track(item->track());
    segment->reset_adjustment_offset();
    segment->mutldata()->all_jump_points_inactive = item->all_jump_points_inactive();

    return segment;
}

PartialTieSegment *SlurTieLayout::layout_partial_tie(PartialTie *item) {
    const bool outgoing = item->is_outgoing();
    PartialTieSegment *segment = create_partial_tie_segment(item);
    SlurTiePos s_pos;

    compute_start_and_end_system(item, s_pos);
    if (outgoing) {
        s_pos.p1 = compute_default_start_or_end_point(item, Grip::START);
    } else {
        s_pos.p2 = compute_default_start_or_end_point(item, Grip::END);
    }

    if (segment->autoplace() && !segment->is_edited()) {
        adjust_x(segment, s_pos, outgoing ? Grip::START : Grip::END);
    }

    set_partial_tie_end_pos(item, s_pos);

    correct_for_cross_staff(item, s_pos, SpannerSegmentType::SINGLE);

    adjust_yfor_ledger_lines(segment, s_pos);

    segment->ups(Grip::START).p = s_pos.p1;
    segment->ups(Grip::END).p = s_pos.p2;

    if (segment->autoplace() && !segment->is_edited()) {
        adjust_y(segment);
    } else {
        compute_bezier(segment);
    }

    add_line_attach_points(segment);

    return segment;
}

void SlurTieLayout::set_partial_tie_end_pos(PartialTie *item, SlurTiePos &s_pos) {
    const bool outgoing = item->is_outgoing();

    const Chord *chord = item->parent_note()->chord();
    const Segment *seg = chord->segment();
    const Measure *measure = seg->measure();
    const System *system = measure->system();

    if (seg->measure()->is_first_in_system() && !outgoing) {
        s_pos.p1 = PointF((system ? system->first_note_rest_segment_x(true) : 0), s_pos.p2.y());
        return;
    }

    auto should_skip_segment = [](const Segment *adj_seg, staff_idx_t staff) {
        bool inactive_or_invisible =
            !adj_seg->is_active() || !adj_seg->enabled() || adj_seg->all_elements_invisible() || !adj_seg->has_elements(staff);
        bool is_above_staff = adj_seg->is_breath_type() || adj_seg->has_time_sig_above_staves();
        return inactive_or_invisible || is_above_staff;
    };

    const Segment *adj_seg = outgoing ? seg->next1() : seg->prev1();
    while (adj_seg && should_skip_segment(adj_seg, item->v_staff_idx())) {
        adj_seg = outgoing ? adj_seg->next1() : adj_seg->prev1();
    }

    double width_to_segment = 0.0;
    if (adj_seg) {
        EngravingItem *element = adj_seg->element(staff2track(item->v_staff_idx()));
        double el_pos = 0.0;
        double element_width = 0.0;
        if (element) {
            const Shape &elem_shape = element->shape();
            el_pos = element->pos().x() + elem_shape.bbox().x();
            element_width = elem_shape.bbox().width();
        }

        const double el_pos_in_system_coords = adj_seg->x_pos_in_system_coords() + el_pos;
        width_to_segment = outgoing ? el_pos_in_system_coords - s_pos.p1.x() : s_pos.p2.x() - (el_pos_in_system_coords + element_width);
        bool incoming_from_barline =
            !outgoing && element && element->is_bar_line() && to_bar_line(element)->bar_line_type() != BarLineType::START_REPEAT;
        width_to_segment -=
            item->style().style_absolute(incoming_from_barline ? Sid::BARLINE_TO_LINE_START_DISTANCE : Sid::LINE_END_TO_BARLINE_DISTANCE);
    }

    if (outgoing) {
        s_pos.p2 = PointF(s_pos.p1.x() + width_to_segment, s_pos.p1.y());
    } else {
        s_pos.p1 = PointF(s_pos.p2.x() - width_to_segment, s_pos.p2.y());
    }
}

void SlurTieLayout::layout_laissez_vib_chord(Chord *chord, LayoutContext &ctx) {
    // Laissez vib ties should all end at the same rightmost point while honouring each tie's minimum length
    // Ties drawn by MuseScore can start at differing points
    // SMuFL symbols will also start at the same point because they are of fixed length
    double chord_lv_end_point = -DBL_MAX;
    std::map<LaissezVibSegment *, SlurTiePos> lv_segments_with_positions;
    const bool smufl_layout = ctx.conf().style_b(Sid::LAISSEZ_VIB_USE_SMUFL_SYM);

    for (const Note *note : chord->notes()) {
        LaissezVib *lv = note->laissez_vib();
        if (!lv) {
            continue;
        }
        SlurTiePos s_pos;
        LaissezVibSegment *lv_seg = create_laissez_vib_segment(lv);

        calculate_laissez_vib_x(lv_seg, s_pos, smufl_layout);

        chord_lv_end_point = std::max(chord_lv_end_point, s_pos.p2.x());

        lv_segments_with_positions.insert({lv_seg, s_pos});
    }

    for (auto &seg_with_pos : lv_segments_with_positions) {
        LaissezVibSegment *lv_seg = seg_with_pos.first;
        Note *note = lv_seg->laissez_vib()->start_note();
        SlurTiePos s_pos = seg_with_pos.second;
        const double x_diff = chord_lv_end_point - s_pos.p2.x();
        s_pos.p2.set_x(chord_lv_end_point);
        if (smufl_layout) {
            s_pos.p1.set_x(s_pos.p1.x() + x_diff);
        }

        calculate_laissez_vib_y(lv_seg, s_pos);

        LaissezVibSegment::LayoutData *ldata = lv_seg->mutldata();
        if (smufl_layout) {
            ldata->set_bbox(lv_seg->sym_bbox(ldata->symbol));
            ldata->set_shape(Shape(ldata->bbox(), lv_seg));
            ldata->set_pos(s_pos.p1);
        }

        double y_origin = s_pos.system1->staff(chord->staff_idx())->y();
        double y_moved = s_pos.system1->staff(chord->v_staff_idx())->y();
        double y_diff = y_moved - y_origin;

        const PointF chord_pos = chord->pos() + chord->segment()->pos() + chord->measure()->pos() + PointF(0.0, y_diff);
        const PointF note_pos = chord_pos + note->pos();
        ldata->pos_relative_to_note = s_pos.p1 - note_pos;

        TLayout::fill_note_shape(note, note->mutldata());
    }
}

void SlurTieLayout::correct_for_cross_staff(Tie *tie, SlurTiePos &s_pos, SpannerSegmentType type) {
    Chord *start_chord = tie->start_note() ? tie->start_note()->chord() : nullptr;
    Chord *end_chord = tie->end_note() ? tie->end_note()->chord() : nullptr;

    if (!start_chord) {
        return;
    }

    bool start_staff_diff = start_chord->v_staff_idx() != tie->staff_idx();
    bool end_staff_diff = end_chord ? end_chord->v_staff_idx() != tie->staff_idx() : false;
    double cur_y1 = s_pos.p1.y();
    double cur_y2 = s_pos.p2.y();

    if (type == SpannerSegmentType::BEGIN) {
        // Cross-system start tie
        if (start_staff_diff && s_pos.system1) {
            double y_origin = s_pos.system1->staff(tie->staff_idx())->y();
            double y_moved = s_pos.system1->staff(start_chord->v_staff_idx())->y();
            double y_diff = y_moved - y_origin;
            s_pos.p1.set_y(cur_y1 + y_diff);
            s_pos.p2.set_y(cur_y2 + y_diff);
        }
    } else if (type == SpannerSegmentType::SINGLE) {
        // Same system single tie
        if (start_staff_diff && s_pos.system1) {
            double y_origin = s_pos.system1->staff(tie->staff_idx())->y();
            double y_moved = s_pos.system1->staff(start_chord->v_staff_idx())->y();
            double y_diff = y_moved - y_origin;
            s_pos.p1.set_y(cur_y1 + y_diff);
            if (tie->is_laissez_vib()) {
                s_pos.p2.set_y(cur_y2 + y_diff);
                return;
            }
        }
        if (!end_chord) {
            return;
        }
        if (end_staff_diff && s_pos.system2) {
            double y_origin = s_pos.system2->staff(tie->staff_idx())->y();
            double y_moved = s_pos.system2->staff(end_chord->v_staff_idx())->y();
            double y_diff = y_moved - y_origin;
            s_pos.p2.set_y(cur_y2 + y_diff);
        }
    }
}

void SlurTieLayout::force_horizontal(Tie *tie, SlurTiePos &s_pos) {
    Note *start_note = tie->start_note();
    Note *end_note = tie->end_note();

    if (start_note && end_note && start_note->line() == end_note->line() && start_note->v_staff_idx() == end_note->v_staff_idx()) {
        double y1 = s_pos.p1.y();
        double y2 = s_pos.p2.y();
        double outer_y = tie->up() ? std::min(y1, y2) : std::max(y1, y2);
        s_pos.p1.set_y(outer_y);
        s_pos.p2.set_y(outer_y);
    }
}

void SlurTieLayout::adjust_x(TieSegment *tie_segment, SlurTiePos &s_pos, Grip start_or_end) {
    bool start = start_or_end == Grip::START;

    PointF &tie_point = start ? s_pos.p1 : s_pos.p2;
    double resulting_x = tie_point.x();

    Tie *tie = tie_segment->tie();
    Note *note = start ? tie->start_note() : tie->end_note();
    const Note *other_note = start ? tie->end_note() : tie->start_note();
    if (!note || note->should_hide_fret()) {
        return;
    }

    Chord *chord = note->chord();
    const Chord *other_chord = other_note ? other_note->chord() : nullptr;
    const Segment *chord_seg = chord->segment();
    const double spatium = tie_segment->spatium();
    const double padding = 0.20 * spatium * (start ? 1 : -1); // TODO: style
    const bool is_grace =
        (chord && chord->is_grace()) || ((other_chord && other_chord->is_grace()) && chord->segment() == other_chord->segment());

    PointF system_pos = chord_seg->pos() + chord->measure()->pos() + (is_grace ? chord->pos() : PointF());
    if (chord->v_staff_idx() != tie_segment->v_staff_idx()) {
        System *system = tie_segment->system();
        double y_diff = system->staff(chord->v_staff_idx())->y() - system->staff(tie->staff_idx())->y();
        system_pos += PointF(0.0, y_diff);
    }

    const bool is_outer_tie_of_chord = tie->is_outer_tie_of_chord(start_or_end);

    TieDotsPlacement dots_placement = tie->style().style_v(Sid::TIE_DOTS_PLACEMENT).value<TieDotsPlacement>();
    bool clear_all_dots = start && dots_placement == TieDotsPlacement::AFTER_DOTS && !is_outer_tie_of_chord && chord->dots() > 0 &&
                          !ChordLayout::chord_has_dots_all_invisible(chord);
    if (clear_all_dots) {
        double dots_outer_edge = -DBL_MAX;
        for (const Note *n : chord->notes()) {
            for (const NoteDot *dot : n->dots()) {
                dots_outer_edge = std::max(dots_outer_edge, dot->x() + dot->width() + n->x());
            }
        }
        dots_outer_edge += system_pos.x();
        resulting_x = dots_outer_edge + padding;
        tie_segment->add_adjustment_offset(PointF(resulting_x - tie_point.x(), 0.0), start_or_end);
        tie_point.set_x(resulting_x);
        return;
    }

    if (is_outer_tie_of_chord) {
        Tie *other_tie = start ? note->tie_back() : note->tie_for();
        bool avoid_other_tie = other_tie && other_tie->up() == tie->up() && !other_tie->is_inside();
        if (avoid_other_tie) {
            resulting_x += 0.1 * spatium * (start ? 1 : -1);
        }
    }

    const bool avoid_stem = chord->stem() && chord->stem()->visible() && chord->up() == tie->up();

    if (is_outer_tie_of_chord && !avoid_stem) {
        tie_segment->add_adjustment_offset(PointF(resulting_x - tie_point.x(), 0.0), start_or_end);
        tie_point.set_x(resulting_x);
        return;
    }

    Shape shape = is_grace ? chord->shape().translate(system_pos) : chord_seg->staff_shape(chord->v_staff_idx()).translated(system_pos);
    bool ignore_dot = start && (is_outer_tie_of_chord || dots_placement == TieDotsPlacement::BEFORE_DOTS);
    bool ignore_lv_seg = tie_segment->is_laissez_vib_segment();
    bool ignore_arpeggio = note == chord->down_note() && !tie->up();
    bool ignore_paren = tie_segment->is_laissez_vib_segment() && start;
    auto ignore_accidental = [&](const Accidental *acc) {
        bool acc_is_inward_of_tie = tie->up() ? acc->line() >= note->line() : acc->line() <= note->line();
        return !start && is_outer_tie_of_chord && acc_is_inward_of_tie;
    };

    static const std::set<ElementType> ignored_types = {ElementType::HOOK,   ElementType::STEM_SLASH, ElementType::LEDGER_LINE,
                                                        ElementType::LYRICS, ElementType::HARMONY,    ElementType::FRET_DIAGRAM};
    shape.remove_if([&](ShapeElement &s) {
        bool remove = !s.item() || s.item() == note || muse::contains(ignored_types, s.item()->type()) ||
                      (s.item()->is_note_dot() && ignore_dot) ||
                      (s.item()->is_accidental() && ignore_accidental(to_accidental(s.item()))) ||
                      (s.item()->is_laissez_vib_segment() && ignore_lv_seg) ||
                      ((s.item()->is_arpeggio() || s.item()->is_chord_bracket()) && ignore_arpeggio) ||
                      (s.item()->is_parenthesis() && ignore_paren) || !s.item()->add_to_skyline();
        return remove;
    });

    const double arc_side_margin = 0.3 * spatium;
    const double points_side_margin = 0.15 * spatium;
    const double y_below = tie_point.y() - (tie->up() ? arc_side_margin : points_side_margin);
    const double y_above = tie_point.y() + (tie->up() ? points_side_margin : arc_side_margin);
    double point_to_clear = start ? shape.right_most_edge_at_height(y_below, y_above) : shape.left_most_edge_at_height(y_below, y_above);

    point_to_clear += padding;

    resulting_x = start ? std::max(resulting_x, point_to_clear) : std::min(resulting_x, point_to_clear);

    adjust_xfor_ledger_lines(tie_segment, start, chord, note, system_pos, padding, resulting_x);

    tie_segment->add_adjustment_offset(PointF(resulting_x - tie_point.x(), 0.0), start_or_end);
    tie_point.set_x(resulting_x);
}

void SlurTieLayout::adjust_xfor_ledger_lines(TieSegment *tie_segment, bool start, Chord *chord, Note *note, const PointF &chord_system_pos,
                                             double padding, double &resulting_x) {
    if (tie_segment->tie()->is_inside() || chord->ledger_lines().empty()) {
        return;
    }

    bool is_outer_note = note == chord->up_note() || note == chord->down_note();
    if (is_outer_note) {
        return;
    }

    bool ledgers_above = false;
    bool ledgers_below = false;
    for (LedgerLine *ledger : chord->ledger_lines()) {
        if (ledger->y() < 0.0) {
            ledgers_above = true;
        } else {
            ledgers_below = true;
        }
        if (ledgers_above && ledgers_below) {
            break;
        }
    }

    int note_line = note->line();
    bool is_odd_line = note_line % 2 != 0;
    bool is_above_staff = note_line <= 0;
    bool is_below_staff = note_line >= 2 * (note->staff()->lines(note->tick()) - 1);
    bool is_inside_staff = !is_above_staff && !is_below_staff;
    if (is_odd_line || is_inside_staff || (is_above_staff && !ledgers_above) || (is_below_staff && !ledgers_below)) {
        return;
    }

    Shape note_shape = note->shape();
    note_shape.remove_if([&](ShapeElement &s) {
        return !s.item()->add_to_skyline() || s.item()->is_note_dot() ||
               (tie_segment->is_laissez_vib_segment() && s.item()->is_laissez_vib_segment());
    });
    note_shape.translate(note->pos() + chord_system_pos);
    double x_note_edge = (start ? note_shape.right() : note_shape.left()) + padding;

    resulting_x = start ? std::max(resulting_x, x_note_edge) : std::min(resulting_x, x_note_edge);
}

void SlurTieLayout::adjust_yfor_ledger_lines(TieSegment *tie_segment, SlurTiePos &s_pos) {
    Tie *tie = tie_segment->tie();
    Note *note = tie_segment->is_single_begin_type() ? tie->start_note() : tie->end_note();
    if (!note) {
        return;
    }

    Chord *chord = note->chord();
    if (chord->ledger_lines().empty()) {
        return;
    }
    PointF chord_system_pos = chord->pos() + chord->segment()->pos() + chord->segment()->measure()->pos();
    PointF &tie_point = tie_segment->is_single_begin_type() ? s_pos.p1 : s_pos.p2;
    double spatium = tie->spatium();
    int up_sign = tie->up() ? -1 : 1;
    double margin = 0.4 * spatium;

    for (LedgerLine *ledger : chord->ledger_lines()) {
        PointF ledger_pos = ledger->pos() + chord_system_pos;
        double y_diff = up_sign * (ledger_pos.y() - tie_point.y());
        bool collision = y_diff > 0 && y_diff < margin;
        if (collision) {
            s_pos.p1 += PointF(0.0, -up_sign * (margin - y_diff));
            s_pos.p2 += PointF(0.0, -up_sign * (margin - y_diff));
            compute_bezier(tie_segment);
            break;
        }
    }
}

void SlurTieLayout::adjust_y(TieSegment *tie_segment) {
    Staff *staff = tie_segment->score() ? tie_segment->score()->staff(tie_segment->v_staff_idx()) : nullptr;
    if (!staff) {
        return;
    }

    const double spatium = tie_segment->spatium();
    TieSegment::LayoutData *ldata = tie_segment->mutldata();

    ldata->set_pos(PointF());
    ldata->move_y(tie_segment->staff_offset_y());

    Fraction tick = tie_segment->tick();

    compute_bezier(tie_segment);

    bool up = tie_segment->tie()->up();
    int up_sign = up ? -1 : 1;

    const double staff_line_dist = staff->line_distance(tick) * spatium;
    const double staff_line_thickness = tie_segment->style().style_absolute(Sid::STAFF_LINE_WIDTH) * staff->staff_mag(tick);

    // 1. Check for bad end point protrusion

    const double end_point_y = tie_segment->ups(Grip::START).p.y();
    const int closest_line_to_endpoints = up ? floor(end_point_y / staff_line_dist) : ceil(end_point_y / staff_line_dist);
    const bool is_end_inside_staff = closest_line_to_endpoints >= 0 && closest_line_to_endpoints < staff->lines(tick);
    const bool is_end_inside_ledger_lines = !is_end_inside_staff && !tie_segment->tie()->is_outer_tie_of_chord(Grip::START);

    const double half_line_thickness_correction = 0.5 * staff_line_thickness * up_sign;
    const double protrusion = std::abs(end_point_y - (closest_line_to_endpoints * spatium - half_line_thickness_correction));
    const double bad_intersection_limit = 0.15 * spatium; // TODO: style

    bool bad_intersection = protrusion < bad_intersection_limit && (is_end_inside_staff || is_end_inside_ledger_lines);
    if (bad_intersection) {
        double corrected_y = closest_line_to_endpoints * spatium + half_line_thickness_correction + bad_intersection_limit * up_sign;
        tie_segment->add_adjustment_offset(PointF(0.0, corrected_y - end_point_y), Grip::START);
        tie_segment->add_adjustment_offset(PointF(0.0, corrected_y - end_point_y), Grip::END);
        tie_segment->ups(Grip::START).p.set_y(corrected_y);
        tie_segment->ups(Grip::END).p.set_y(corrected_y);
        compute_bezier(tie_segment);
    }

    // 2. Check for bad arc protrusion

    RectF tie_segment_b_box = tie_segment->ldata()->bbox();
    double tie_length = tie_segment_b_box.width();
    double tie_height = tie_segment_b_box.height();
    double mid_thickness = tie_segment->ldata()->mid_thickness() * 2;
    double y_outer_apogee = up ? tie_segment_b_box.top() : tie_segment_b_box.bottom();
    double y_inner_apogee = y_outer_apogee - mid_thickness * up_sign;
    double y_mid_apogee = 0.5 * (y_outer_apogee + y_inner_apogee);

    int closest_line_to_arc = round(y_mid_apogee / staff_line_dist);
    bool is_arc_inside_staff = closest_line_to_arc >= 0 && closest_line_to_arc < staff->lines(tick);
    if (!is_arc_inside_staff) {
        return;
    }

    double outward_margin = -up_sign * (y_outer_apogee - (closest_line_to_arc * staff_line_dist - half_line_thickness_correction));
    double inward_margin = up_sign * (y_inner_apogee - (closest_line_to_arc * staff_line_dist + half_line_thickness_correction));
    const double bad_arc_intersection_limit = tie_length < 3 * spatium ? 0.1 * spatium : 0.15 * spatium;

    bool increase_arc = outward_margin - 0.5 * bad_arc_intersection_limit < inward_margin;
    bool correct_outwards = inward_margin < bad_arc_intersection_limit && increase_arc;
    bool correct_inwards = outward_margin < bad_arc_intersection_limit && !increase_arc;

    if (!correct_inwards && !correct_outwards) {
        return;
    }

    bool is_small_tie = tie_length < 2.0 * spatium && tie_height < 0.7 * spatium;
    // For ties this small, try to fit them within a staff space before changing their arc
    if (is_small_tie) {
        Tie *tie2 = tie_segment->tie();
        bool is_inside = tie2->is_inside();
        bool is_outer_of_chord = tie2->is_outer_tie_of_chord(Grip::START) || tie2->is_outer_tie_of_chord(Grip::END);
        bool has_tied_second_inside = tie2->has_tied_second_inside();
        if (!is_inside && !is_outer_of_chord && !has_tied_second_inside && !has_end_point_above_note(tie_segment)) {
            double current_y = tie_segment->ups(Grip::START).p.y();
            double y_correction = -up_sign * (bad_arc_intersection_limit - outward_margin);
            tie_segment->add_adjustment_offset(PointF(0.0, y_correction), Grip::START);
            tie_segment->add_adjustment_offset(PointF(0.0, y_correction), Grip::END);
            tie_segment->ups(Grip::START).p.set_y(current_y + y_correction);
            tie_segment->ups(Grip::END).p.set_y(current_y + y_correction);
            compute_bezier(tie_segment);
            return;
        } else {
            correct_outwards = true;
        }
    }

    double arc_correction = correct_outwards ? (bad_arc_intersection_limit - inward_margin) : (bad_arc_intersection_limit - outward_margin);
    double max_arc_correction = 0.75 * tie_height;
    double rest = arc_correction > max_arc_correction ? arc_correction - max_arc_correction : 0.0;
    arc_correction = std::min(arc_correction, max_arc_correction);
    if (correct_outwards) {
        if (rest > 0) {
            rest *= up_sign;
            double current_y = tie_segment->ups(Grip::START).p.y();
            tie_segment->add_adjustment_offset(PointF(0.0, rest), Grip::START);
            tie_segment->add_adjustment_offset(PointF(0.0, rest), Grip::END);
            tie_segment->ups(Grip::START).p.set_y(current_y + rest);
            tie_segment->ups(Grip::END).p.set_y(current_y + rest);
        }
        compute_bezier(tie_segment, PointF(0.0, -arc_correction));
    } else if (correct_inwards) {
        compute_bezier(tie_segment, PointF(0.0, arc_correction));
    }
}

bool SlurTieLayout::has_end_point_above_note(TieSegment *tie_segment) {
    Note *start_note = tie_segment->tie()->start_note();
    Note *end_note = tie_segment->tie()->end_note();

    if ((tie_segment->is_single_begin_type() && !start_note) || (tie_segment->is_single_end_type() && !end_note)) {
        return false;
    }

    Chord *start_chord = start_note->chord();
    PointF start_note_pos = start_note->pos() + start_chord->pos() + start_chord->segment()->pos() + start_chord->measure()->pos();

    Chord *end_chord = end_note->chord();
    PointF end_note_pos = end_note->pos() + end_chord->pos() + end_chord->segment()->pos() + end_chord->measure()->pos();

    PointF tie_start_pos = tie_segment->ups(Grip::START).pos();
    PointF tie_end_pos = tie_segment->ups(Grip::END).pos();

    return (tie_start_pos.x() < start_note_pos.x() + start_note->width() && !start_note->should_hide_fret()) ||
           (tie_end_pos.x() > end_note_pos.x() && !end_note->should_hide_fret());
}

void SlurTieLayout::resolve_vertical_tie_collisions(const std::vector<TieSegment *> &stacked_ties) {
    if (stacked_ties.size() < 2) {
        return;
    }

    std::list<TieSegment *> downward_ties;
    std::list<TieSegment *> upward_ties;
    for (TieSegment *tie_segment : stacked_ties) {
        if (!tie_segment->tie()->up()) {
            downward_ties.push_front(tie_segment);
        } else {
            upward_ties.push_back(tie_segment);
        }
    }

    auto fix_tie_collision = [](TieSegment *this_tie, TieSegment *next_tie) {
        double spatium = this_tie->spatium();
        bool up = this_tie->tie()->up();
        int up_sign = up ? -1 : 1;

        double this_tie_outer_y = up ? this_tie->ldata()->bbox().top() : this_tie->ldata()->bbox().bottom();
        double next_tie_inner_y =
            (up ? next_tie->ldata()->bbox().top() : next_tie->ldata()->bbox().bottom()) - up_sign * 2 * next_tie->ldata()->mid_thickness();
        double clearance_margin = 0.15 * spatium;
        bool collision = up_sign * (next_tie_inner_y - this_tie_outer_y) < clearance_margin;
        if (!collision) {
            return;
        }

        if (!this_tie->score()) {
            return;
        }
        Staff *staff = this_tie->score() ? this_tie->score()->staff(this_tie->v_staff_idx()) : nullptr;
        if (!staff) {
            return;
        }

        Fraction tick = this_tie->tick();
        double y_mid_point = 0.5 * (this_tie_outer_y + next_tie_inner_y);
        double half_line_dist = 0.5 * staff->line_distance(tick) * spatium;
        int mid_line = round(y_mid_point / half_line_dist);
        bool inside_staff = mid_line >= -1 && mid_line <= 2 * (staff->lines(tick) - 1) + 1;
        if (inside_staff) {
            y_mid_point = mid_line * half_line_dist;
        }

        double this_tie_y_correction = y_mid_point - up_sign * 0.5 * clearance_margin - this_tie_outer_y;
        double next_tie_y_correction = y_mid_point + up_sign * 0.5 * clearance_margin - next_tie_inner_y;

        double this_shoulder_off = this_tie->adjustment_offset(Grip::BEZIER1).y();
        double next_shoulder_off = next_tie->adjustment_offset(Grip::BEZIER1).y();
        // Subtract it otherwise it gets summed twice
        this_tie->add_adjustment_offset(PointF(0.0, -this_shoulder_off), Grip::BEZIER1);
        this_tie->add_adjustment_offset(PointF(0.0, -this_shoulder_off), Grip::BEZIER2);
        next_tie->add_adjustment_offset(PointF(0.0, -next_shoulder_off), Grip::BEZIER1);
        next_tie->add_adjustment_offset(PointF(0.0, -next_shoulder_off), Grip::BEZIER2);

        compute_bezier(this_tie, PointF(0.0, -up_sign * (this_shoulder_off + this_tie_y_correction)));
        compute_bezier(next_tie, PointF(0.0, -up_sign * (next_shoulder_off + next_tie_y_correction)));
    };

    if (upward_ties.size() >= 2) {
        for (auto it = upward_ties.begin(); std::next(it, 1) != upward_ties.end(); ++it) {
            TieSegment *this_tie = *it;
            TieSegment *next_tie = *(std::next(it, 1));
            fix_tie_collision(this_tie, next_tie);
        }
    }

    if (downward_ties.size() >= 2) {
        for (auto it = downward_ties.begin(); std::next(it, 1) != downward_ties.end(); ++it) {
            TieSegment *this_tie = *it;
            TieSegment *next_tie = *(std::next(it, 1));
            fix_tie_collision(this_tie, next_tie);
        }
    }
}

void SlurTieLayout::compute_up(Slur *slur, LayoutContext &ctx) {
    switch (slur->slur_direction()) {
        case DirectionV::UP: slur->set_up(true); break;
        case DirectionV::DOWN: slur->set_up(false); break;
        case DirectionV::AUTO: {
            //
            // assumption:
            // slurs have only chords or rests as start/end elements
            //
            ChordRest *chord_rest1 = slur->start_cr();
            ChordRest *chord_rest2 = slur->end_cr();
            if (chord_rest1 == 0 || chord_rest2 == 0) {
                slur->set_up(true);
                break;
            }
            Chord *chord1 = slur->start_cr()->is_chord() ? to_chord(slur->start_cr()) : 0;
            Chord *chord2 = slur->end_cr()->is_chord() ? to_chord(slur->end_cr()) : 0;
            if (chord2 && !chord2->staff()->is_drum_staff(chord2->tick()) &&
                slur->start_cr()->measure()->system() != slur->end_cr()->measure()->system()) {
                // HACK: if the end chord is in a different system, it may have never been laid out yet.
                // But we need to know its direction to decide slur direction, so need to compute it here.
                for (Note *note : chord2->notes()) {
                    note->update_line(); // because chord direction is based on note lines
                }

                ChordLayout::compute_up(chord2, ctx);
            }

            if (chord1 && chord1->beam() && chord1->beam()->cross()) {
                // TODO: stem direction is not finalized, so we cannot use it here
                slur->set_up(true);
                break;
            }

            slur->set_up(!(chord_rest1->up()));

            // Check if multiple voices
            bool multiple_voices = false;
            Measure *m1 = chord_rest1->measure();
            while (m1 && m1->tick() <= chord_rest2->tick()) {
                if ((m1->has_voices(chord_rest1->staff_idx(), slur->tick(), slur->ticks() + chord_rest2->ticks())) && chord1) {
                    multiple_voices = true;
                    break;
                }
                m1 = m1->next_measure();
            }
            if (multiple_voices) {
                // slurs go on the stem side
                if (chord_rest1->voice() > 0 || chord_rest2->voice() > 0) {
                    slur->set_up(false);
                } else {
                    slur->set_up(true);
                }
            } else if (chord1 && chord2 && !chord1->is_grace() && is_direction_mixture(chord1, chord2, ctx)) {
                // slurs go above if there are mixed direction stems between c1 and c2
                // but grace notes are exceptions
                slur->set_up(true);
            } else if (chord1 && chord2 && chord1->is_grace() && chord2 != chord1->parent() && is_direction_mixture(chord1, chord2, ctx)) {
                slur->set_up(true);
            }
        } break;
    }
}

void SlurTieLayout::compute_bezier(TieSegment *tie_seg, PointF shoulder_offset) {
    const PointF tie_start = tie_seg->ups(Grip::START).p + tie_seg->ups(Grip::START).off;
    const PointF tie_end = tie_seg->ups(Grip::END).p + tie_seg->ups(Grip::END).off;
    if (!muse::real_is_equal_or_more(tie_end.x(), tie_start.x())) {
        return;
    }

    PointF tie_end_normalized = tie_end - tie_start; // normalize to zero
    if (muse::real_is_null(tie_end_normalized.x())) {
        return;
    }

    const double tie_angle =
        atan(tie_end_normalized.y() / tie_end_normalized.x()); // angle required from tie start to tie end--zero if horizontal
    Transform t;
    t.rotate_radians(-tie_angle); // rotate so that we are working with horizontal ties regardless of endpoint height difference
    tie_end_normalized = t.map(tie_end_normalized); // apply that rotation
    shoulder_offset = t.map(shoulder_offset);       // also apply to shoulderOffset

    const double spatium_value = tie_seg->spatium();
    double tie_length_in_sp = tie_end_normalized.x() / spatium_value;

    const double min_shoulder_height = tie_seg->min_shoulder_height();
    const double max_shoulder_height = tie_seg->max_shoulder_height();
    double shoulder_h = min_shoulder_height + spatium_value * 0.3 * sqrt(std::abs(tie_length_in_sp - 1));
    shoulder_h = std::clamp(shoulder_h, min_shoulder_height, max_shoulder_height);

    shoulder_h -= shoulder_offset.y();

    PointF shoulder_adjust_offset = tie_seg->tie()->up() ? PointF(0.0, shoulder_offset.y()) : PointF(0.0, -shoulder_offset.y());
    tie_seg->add_adjustment_offset(shoulder_adjust_offset, Grip::BEZIER1);
    tie_seg->add_adjustment_offset(shoulder_adjust_offset, Grip::BEZIER2);

    if (!tie_seg->tie()->up()) {
        shoulder_h = -shoulder_h;
    }

    double shoulder_w = 0.6; // TODO: style

    const double tie_width = tie_end_normalized.x();
    const double bezier1_x = (tie_width - tie_width * shoulder_w) * .5 + shoulder_offset.x();
    const double bezier2_x = bezier1_x + tie_width * shoulder_w + shoulder_offset.x();

    const PointF tie_drag = PointF(tie_width * .5, 0.0);

    const PointF bezier1(bezier1_x, -shoulder_h);
    const PointF bezier2(bezier2_x, -shoulder_h);

    compute_mid_thickness(tie_seg, tie_length_in_sp);

    PointF tie_thickness(0.0, 0.0);
    if (tie_seg->tie()->style_type() == SlurStyleType::SOLID) {
        tie_thickness.set_y(tie_seg->ldata()->mid_thickness());
    }

    const PointF bezier1_offset = t.map(tie_seg->ups(Grip::BEZIER1).off);
    const PointF bezier2_offset = t.map(tie_seg->ups(Grip::BEZIER2).off);

    //-----------------------------------calculate p6
    const PointF bezier1_final = bezier1 + bezier1_offset;
    const PointF bezier2_final = bezier2 + bezier2_offset;

    const PointF tie_shoulder = 0.5 * (bezier1_final + bezier2_final);
    //-----------------------------------

    PainterPath path = PainterPath();
    path.move_to(PointF());
    path.cubic_to(bezier1 + bezier1_offset - tie_thickness, bezier2 + bezier2_offset - tie_thickness, tie_end_normalized);
    if (tie_seg->tie()->style_type() == SlurStyleType::SOLID) {
        path.cubic_to(bezier2 + bezier2_offset + tie_thickness, bezier1 + bezier1_offset + tie_thickness, PointF());
    }

    // translate back
    t.reset();
    t.translate(tie_start.x(), tie_start.y());
    t.rotate_radians(tie_angle);
    path = t.map(path);
    tie_seg->mutldata()->path.set_value(path);

    tie_seg->ups(Grip::BEZIER1).p = t.map(bezier1);
    tie_seg->ups(Grip::BEZIER2).p = t.map(bezier2);
    tie_seg->ups(Grip::END).p = t.map(tie_end_normalized) - tie_seg->ups(Grip::END).off;
    tie_seg->ups(Grip::DRAG).p = t.map(tie_drag);
    tie_seg->ups(Grip::SHOULDER).p = t.map(tie_shoulder);

    fill_shape(tie_seg, tie_length_in_sp);
}

void SlurTieLayout::compute_bezier(SlurSegment *slur_seg, PointF shoulder_offset) {
    /* ************************************************
     * LEGEND: pp1 = start point
     *         pp2 = end point
     *         p2 = end point (in slur coordinates)
     *         p3 = first bezier point (in slur coord.)
     *         p4 = second bezier point (in slur coord.)
     *         p5 = whole slur drag point (in slur coord.)
     *         p6 = shoulder drag point (in slur coord.)
     * REMEMBER! ups().pos() = ups().p + ups().off
     * ***********************************************/
    // Avoid bad staff line intersections
    if (slur_seg->autoplace()) {
        adjust_end_points(slur_seg);
    }
    // If end point adjustment is locked, restore the endpoints to
    // where they were before
    if (slur_seg->is_end_points_edited()) {
        slur_seg->ups(Grip::START).off += slur_seg->end_point_off1();
        slur_seg->ups(Grip::END).off += slur_seg->end_point_off2();
    }
    // Get start and end points (have been calculated before)
    PointF pp1 = slur_seg->ups(Grip::START).p + slur_seg->ups(Grip::START).off;
    PointF pp2 = slur_seg->ups(Grip::END).p + slur_seg->ups(Grip::END).off;
    // Keep track of the original value before it gets changed
    PointF oldp1 = pp1;
    PointF oldp2 = pp2;

    // Set up coordinate transforms
    // CAUTION: transform operations are applies in reverse order to how
    // they are added to the transformation.
    double slur_angle = atan((pp2.y() - pp1.y()) / (pp2.x() - pp1.x()));
    Transform rotate;
    rotate.rotate_radians(-slur_angle);
    Transform to_slur_coordinates;
    to_slur_coordinates.rotate_radians(-slur_angle);
    to_slur_coordinates.translate(-pp1.x(), -pp1.y());
    Transform to_system_coordinates = to_slur_coordinates.inverted();
    // Transform p2 and shoulder offset
    PointF p2 = to_slur_coordinates.map(pp2);
    shoulder_offset = rotate.map(shoulder_offset);

    // COMPUTE DEFAULT SLUR SHAPE
    // Compute default shoulder height and width
    double spatium_value = slur_seg->spatium();
    double shoulder_w; // expressed as fraction of slur-length
    double d = p2.x() / spatium_value;

    if (d < 0) {
        //! NOTE A negative d means that end point is before the start point.
        //! This only exists as a temporary state when horizontal spacing hasn't yet been computed,
        //! and it makes no sense for any of the following calculations
        return;
    }

    if (d < 2) {
        shoulder_w = 0.60;
    } else if (d < 10) {
        shoulder_w = 0.5;
    } else if (d < 18) {
        shoulder_w = 0.6;
    } else {
        shoulder_w = 0.7;
    }

    double shoulder_h = compute_shoulder_height(slur_seg, d, shoulder_offset);

    double c = p2.x();
    double c1 = (c - c * shoulder_w) * .5 + shoulder_offset.x();
    double c2 = c1 + c * shoulder_w + shoulder_offset.x();
    PointF p3(c1, -shoulder_h);
    PointF p4(c2, -shoulder_h);
    // Set Bezier points default position
    slur_seg->ups(Grip::BEZIER1).p = to_system_coordinates.map(p3);
    slur_seg->ups(Grip::BEZIER2).p = to_system_coordinates.map(p4);
    // Add offsets
    p3 += shoulder_offset + rotate.map(slur_seg->ups(Grip::BEZIER1).off);
    p4 += shoulder_offset + rotate.map(slur_seg->ups(Grip::BEZIER2).off);
    slur_seg->ups(Grip::BEZIER1).off += rotate.inverted().map(shoulder_offset);
    slur_seg->ups(Grip::BEZIER2).off += rotate.inverted().map(shoulder_offset);

    // ADAPT SLUR SHAPE AND ENDPOINT POSITION
    // to clear collisions with underlying items
    if (slur_seg->autoplace() && !slur_seg->is_tapping_half_slur_segment()) {
        avoid_collisions(slur_seg, pp1, p2, p3, p4, to_system_coordinates, slur_angle);
    }

    // Re-check end points for bad staff line collisions
    slur_seg->ups(Grip::START).p = pp1 - slur_seg->ups(Grip::START).off;
    slur_seg->ups(Grip::END).p = to_system_coordinates.map(p2) - slur_seg->ups(Grip::END).off;
    adjust_end_points(slur_seg);
    PointF newpp1 = slur_seg->ups(Grip::START).p + slur_seg->ups(Grip::START).off;
    PointF difference = rotate.map(newpp1 - pp1);
    pp1 = newpp1;
    pp2 = slur_seg->ups(Grip::END).p + slur_seg->ups(Grip::END).off;
    p3 -= difference;
    p4 -= difference;
    // Keep track of how much the end points position has changed
    if (!slur_seg->is_end_points_edited()) {
        slur_seg->set_end_point_off1(pp1 - oldp1);
        slur_seg->set_end_point_off2(pp2 - oldp2);
    } else {
        slur_seg->set_end_point_off1(PointF());
        slur_seg->set_end_point_off2(PointF());
    }
    // Recompute the transformation because pp1 and pp1 may have changed
    to_slur_coordinates.reset();
    to_slur_coordinates.rotate_radians(-slur_angle);
    to_slur_coordinates.translate(-pp1.x(), -pp1.y());
    to_system_coordinates = to_slur_coordinates.inverted();
    p2 = to_slur_coordinates.map(pp2);

    // Calculate p5 and p6
    PointF p5 = 0.5 * p2;        // mid-point between pp1 and p2
    PointF p6 = 0.5 * (p3 + p4); // mid-point between p3 and p4

    // Update all slur points after collision avoidance
    slur_seg->ups(Grip::BEZIER1).p = to_system_coordinates.map(p3) - slur_seg->ups(Grip::BEZIER1).off;
    slur_seg->ups(Grip::BEZIER2).p = to_system_coordinates.map(p4) - slur_seg->ups(Grip::BEZIER2).off;
    slur_seg->ups(Grip::DRAG).p = to_system_coordinates.map(p5);
    slur_seg->ups(Grip::SHOULDER).p = to_system_coordinates.map(p6);

    // Set slur thickness
    compute_mid_thickness(slur_seg, p2.x() / slur_seg->spatium());

    PointF thick(0.0, 0.0);
    if (slur_seg->slur()->style_type() == SlurStyleType::SOLID) {
        thick.set_y(slur_seg->ldata()->mid_thickness());
    }

    // Set path
    PainterPath path = PainterPath();
    path.move_to(PointF());
    path.cubic_to(p3 - thick, p4 - thick, p2);
    if (slur_seg->slur()->style_type() == SlurStyleType::SOLID) {
        path.cubic_to(p4 + thick, p3 + thick, PointF());
    }

    path = to_system_coordinates.map(path);
    slur_seg->mutldata()->path.set_value(path);

    fill_shape(slur_seg, p2.x() / slur_seg->spatium());
}

double SlurTieLayout::compute_shoulder_height(SlurSegment *slur_seg, double slur_length_in_sp, PointF shoulder_offset) {
    double spatium = slur_seg->spatium();

    if (slur_seg->is_tapping_half_slur_segment()) {
        double shoulder_h = (slur_seg->staff_type()->is_tab_staff() ? 1.0 : 0.6) * spatium;
        return slur_seg->slur()->up() ? shoulder_h : -shoulder_h;
    }

    double shoulder_h = sqrt(slur_length_in_sp / 4) * spatium;

    static constexpr double shoulder_reduction = 0.75;
    if (is_over_beams(slur_seg->slur())) {
        shoulder_h *= shoulder_reduction;
    }

    shoulder_h -= shoulder_offset.y();
    if (!slur_seg->slur()->up()) {
        shoulder_h = -shoulder_h;
    }

    return shoulder_h;
}

double SlurTieLayout::default_stem_length_start(TremoloTwoChord *tremolo) {
    return TremoloLayout::extended_stem_len_with_two_note_tremolo(tremolo, tremolo->chord1()->default_stem_length(),
                                                                  tremolo->chord2()->default_stem_length())
        .first;
}

double SlurTieLayout::default_stem_length_end(TremoloTwoChord *tremolo) {
    return TremoloLayout::extended_stem_len_with_two_note_tremolo(tremolo, tremolo->chord1()->default_stem_length(),
                                                                  tremolo->chord2()->default_stem_length())
        .second;
}

bool SlurTieLayout::is_direction_mixture(const Chord *c1, const Chord *c2, LayoutContext &ctx) {
    if (c1->track() != c2->track()) {
        return false;
    }
    const bool up = c1->up();
    if (c2->is_grace() && c2->up() != up) {
        return true;
    }
    if (c1->is_grace_before() && c2->is_grace_after() && c1->ownership_parent() == c2->ownership_parent()) {
        const Chord *main_chord = to_chord(c1->ownership_parent());
        if (main_chord->stem() && main_chord->up() != up) {
            return true;
        }
    }
    const track_idx_t track = c1->track();
    for (const Segment *seg = c1->segment(); seg && seg->tick() <= c2->tick(); seg = seg->next1(SegmentType::CHORD_REST)) {
        if ((c1->is_grace() || c2->is_grace_before()) && seg->tick() == c2->tick()) {
            // if slur ends at a grace-note-before, we don't need to look at the main note
            return false;
        }
        EngravingItem *e = seg->element(track);
        if (!e || !e->is_chord()) {
            continue;
        }
        Chord *c = to_chord(e);
        const Measure *m = c->measure();
        const System *c1_sys = c1->measure()->system();
        const System *c_sys = m->system();
        if (!c->staff()->is_drum_staff(c->tick()) && c1_sys && c_sys && c1_sys != c_sys) {
            // This chord is on a different system and may not have been laid out yet
            for (Note *note : c->notes()) {
                note->update_line(); // because chord direction is based on note lines
            }
            ChordLayout::compute_up(c, ctx);
        }

        if (c->up() != up) {
            return true;
        }
    }
    return false;
}

bool SlurTieLayout::should_hide_slur_segment(SlurSegment *item) {
    if (item->is_hammer_on_pull_off_segment()) {
        return false;
    }

    if (item->configuration()->specific_slurs_layout_workaround()) {
        Slur *slur = item->slur();
        if (slur->connected_element() == Slur::ConnectedElement::GLISSANDO) {
            return false;
        }
    }

    const StaffType *st_type = item->staff_type();
    if (st_type && st_type->is_hidden_element_on_tab(Sid::SLUR_SHOW_TAB_COMMON, Sid::SLUR_SHOW_TAB_SIMPLE)) {
        return true;
    }

    return false;
}

void SlurTieLayout::add_line_attach_points(TieSegment *segment) {
    // Add tie attach point to start and end note of segment
    Tie *tie = segment->tie();
    Note *start_note = tie->start_note();
    Note *end_note = tie->end_note();

    const bool single_or_begin =
        segment->spanner_segment_type() == SpannerSegmentType::SINGLE || segment->spanner_segment_type() == SpannerSegmentType::BEGIN;

    const bool single_or_end =
        segment->spanner_segment_type() == SpannerSegmentType::SINGLE || segment->spanner_segment_type() == SpannerSegmentType::END;

    if (start_note && single_or_begin) {
        start_note->add_start_line_attach_point(segment->ups(Grip::START).pos(), tie);
    }
    if (end_note && single_or_end) {
        end_note->add_end_line_attach_point(segment->ups(Grip::END).pos(), tie);
    }
}

void SlurTieLayout::add_line_attach_points(PartialTieSegment *segment) {
    // Add tie attach point to parent note
    PartialTie *tie = segment->partial_tie();
    Note *note = tie ? tie->note() : nullptr;
    if (!note) {
        return;
    }

    const bool is_outgoing = tie->is_outgoing();

    if (is_outgoing) {
        note->add_start_line_attach_point(segment->ups(Grip::START).pos(), tie);
    } else {
        note->add_end_line_attach_point(segment->ups(Grip::END).pos(), tie);
    }
}

void SlurTieLayout::calculate_direction(Tie *item) {
    if (!item->start_note() && !item->end_note()) {
        return;
    }
    const bool tie_has_both_notes = item->start_note() && item->end_note();

    const Note *primary_note = item->start_note() ? item->start_note() : item->end_note();
    const Chord *primary_chord = primary_note->chord();
    const Measure *primary_measure = primary_chord->measure();

    const Note *secondary_note = tie_has_both_notes ? item->end_note() : nullptr;
    const Chord *secondary_chord = secondary_note ? secondary_note->chord() : nullptr;
    const Measure *secondary_measure = secondary_chord ? secondary_chord->measure() : nullptr;

    if (item->slur_direction() == DirectionV::AUTO) {
        std::vector<Note *> notes = primary_chord->notes();
        size_t n = notes.size();
        StaffType *st = item->staff()->staff_type(primary_note ? primary_note->tick() : Fraction(0, 1));
        bool simple_exception = st && st->is_simple_tab_staff();
        // if there are multiple voices, the tie direction goes on stem side
        if (primary_measure->has_voices(primary_chord->staff_idx(), primary_chord->tick(), primary_chord->actual_ticks())) {
            item->set_up(simple_exception ? is_up_voice(primary_chord->voice()) : primary_chord->up());
        } else if (tie_has_both_notes &&
                   secondary_measure->has_voices(secondary_chord->staff_idx(), secondary_chord->tick(), secondary_chord->actual_ticks())) {
            item->set_up(simple_exception ? is_up_voice(secondary_chord->voice()) : secondary_chord->up());
        } else if (n == 1) {
            //
            // single note
            //
            if (tie_has_both_notes && primary_chord->up() != secondary_chord->up()) {
                // if stem direction is mixed, always up
                item->set_up(true);
            } else {
                item->set_up(!primary_chord->up());
            }
        } else {
            //
            // chords
            //
            // first, find pivot point in chord (below which all ties curve down and above which all ties curve up)
            Note *pivot_point = nullptr;
            bool multiple_pivots = false;
            for (size_t i = 0; i < n - 1; ++i) {
                if (!notes[i]->tie_for()) {
                    continue; // don't include notes that don't have ties
                }
                for (size_t j = i + 1; j < n; ++j) {
                    if (!notes[j]->tie_for()) {
                        continue;
                    }
                    int note_diff = compare_notes_pos(notes[i], notes[j]);
                    if (!multiple_pivots && std::abs(note_diff) <= 1) {
                        // TODO: Fix unison ties somehow--if noteDiff == 0 then we need to determine which of the unison is 'lower'
                        if (pivot_point) {
                            multiple_pivots = true;
                            pivot_point = nullptr;
                        } else {
                            pivot_point = note_diff < 0 ? notes[i] : notes[j];
                        }
                    }
                }
            }
            if (!pivot_point) {
                // if the pivot point was not found (either there are no unisons/seconds or there are more than one),
                // determine if this note is in the lower or upper half of this chord
                int notes_above = 0, ties_above = 0;
                int notes_below = 0, ties_below = 0;
                int unison_ties = 0;
                for (size_t i = 0; i < n; ++i) {
                    if (notes[i] == primary_note) {
                        // skip counting if this note is the current note or if this note doesn't have a tie
                        continue;
                    }
                    int note_diff = compare_notes_pos(primary_note, notes[i]);
                    if (note_diff == 0) { // unison
                        if (notes[i]->tie_for()) {
                            unison_ties++;
                        }
                    }
                    if (note_diff < 0) { // the note is above startNote
                        notes_above++;
                        if (notes[i]->tie_for()) {
                            ties_above++;
                        }
                    }
                    if (note_diff > 0) { // the note is below startNote
                        notes_below++;
                        if (notes[i]->tie_for()) {
                            ties_below++;
                        }
                    }
                }

                if (ties_above == 0 && ties_below == 0 && unison_ties == 0) {
                    // this is the only tie in the chord.
                    if (notes_above == notes_below) {
                        item->set_up(!primary_chord->up());
                    } else {
                        item->set_up(notes_above < notes_below);
                    }
                } else if (ties_above == ties_below) {
                    // this note is dead center, so its tie should go counter to the stem direction
                    item->set_up(!primary_chord->up());
                } else {
                    item->set_up(ties_above < ties_below);
                }
            } else if (pivot_point == primary_note) {
                // the current note is the lower of the only second or unison in the chord; tie goes down.
                item->set_up(false);
            } else {
                // if lower than the pivot, tie goes down, otherwise up
                int note_diff = compare_notes_pos(primary_note, pivot_point);
                item->set_up(note_diff >= 0);
            }
        }
    } else {
        item->set_up(item->slur_direction() == DirectionV::UP ? true : false);
    }
}

void SlurTieLayout::calculate_is_inside(Tie *item) {
    if (item->tie_placement() != TiePlacement::AUTO) {
        item->set_is_inside(item->tie_placement() == TiePlacement::INSIDE);
        return;
    }

    const Note *start_n = item->start_note();
    const Chord *start_chord = start_n ? start_n->chord() : nullptr;
    const Note *end_n = item->end_note();
    const Chord *end_chord = end_n ? end_n->chord() : nullptr;

    if (!start_chord && !end_chord) {
        item->set_is_inside(false);
        return;
    }

    const bool start_is_single_note = start_chord ? start_chord->notes().size() <= 1 : false;
    const bool end_is_single_note = end_chord ? end_chord->notes().size() <= 1 : false;

    const bool should_place_inside =
        start_chord && end_chord ? start_is_single_note && end_is_single_note : start_is_single_note || end_is_single_note;

    if (should_place_inside) {
        item->set_is_inside(item->style().style_v(Sid::TIE_PLACEMENT_SINGLE_NOTE).value<TiePlacement>() == TiePlacement::INSIDE);
    } else {
        item->set_is_inside(item->style().style_v(Sid::TIE_PLACEMENT_CHORD).value<TiePlacement>() == TiePlacement::INSIDE);
    }
}

void SlurTieLayout::layout_segment(SlurSegment *item, const PointF &p1, const PointF &p2) {
    SlurSegment::LayoutData *ldata = item->mutldata();
    if (should_hide_slur_segment(item)) {
        ldata->set_is_skip_draw(true);
        return;
    }

    ldata->set_is_skip_draw(false);

    ldata->set_pos(PointF());
    item->ups(Grip::START).p = p1;
    item->ups(Grip::END).p = p2;
    item->set_extra_height(0.0);

    //Adjust Y pos to staff type yOffset before other calculations
    ldata->move_y(item->staff_offset_y());

    compute_bezier(item);
}

void SlurTieLayout::compute_mid_thickness(SlurTieSegment *slur_tie_seg, double slur_tie_length_in_sp) {
    Staff *staff = slur_tie_seg->score() ? slur_tie_seg->score()->staff(slur_tie_seg->v_staff_idx()) : nullptr;
    const double mag = staff ? staff->staff_mag(slur_tie_seg->slur_tie()->tick()) : 1.0;
    const double scaling_factor = slur_tie_seg->slur_tie()->scaling_factor();

    if (slur_tie_seg->slur_tie()->style_type() != SlurStyleType::SOLID) {
        slur_tie_seg->mutldata()->mid_thickness.set_value(0.5 * slur_tie_seg->dotted_width() * mag * scaling_factor);
        return;
    }

    const double end_width = slur_tie_seg->end_width();
    const double mid_width = slur_tie_seg->mid_width();
    const double min_tie_length = mag * slur_tie_seg->style().style_s(Sid::MIN_TIE_LENGTH).val();
    const double short_tie_limit = mag * 4.0;
    const double min_tie_thickness = mag * (0.15 * slur_tie_seg->spatium() - end_width);
    const double normal_thickness = mag * (mid_width - end_width);

    bool invalid = muse::real_is_equal_or_more(min_tie_length, short_tie_limit);

    double final_thickness;
    if (slur_tie_length_in_sp > short_tie_limit || invalid) {
        final_thickness = normal_thickness;
    } else {
        const double A = 1 / (short_tie_limit - min_tie_length);
        const double B = normal_thickness - min_tie_thickness;
        const double C = short_tie_limit * min_tie_thickness - min_tie_length * normal_thickness;
        final_thickness = A * (B * slur_tie_length_in_sp + C);
    }

    final_thickness = std::min(final_thickness, normal_thickness * scaling_factor);

    slur_tie_seg->mutldata()->mid_thickness.set_value(final_thickness);
}

void SlurTieLayout::fill_shape(SlurTieSegment *slur_tie_seg, double slur_tie_length_in_sp) {
    Shape shape(Shape::Type::COMPOSITE);
    PointF start_point = slur_tie_seg->ups(Grip::START).pos();

    double mid_thickness = 2 * slur_tie_seg->ldata()->mid_thickness();
    int nb_shapes = round(5.0 * slur_tie_length_in_sp);
    nb_shapes = std::max(nb_shapes, 20);
    nb_shapes = std::min(nb_shapes, 50);
    const CubicBezier b(start_point, slur_tie_seg->ups(Grip::BEZIER1).pos(), slur_tie_seg->ups(Grip::BEZIER2).pos(),
                        slur_tie_seg->ups(Grip::END).pos());
    for (int i = 1; i <= nb_shapes; i++) {
        double percent = pow(sin(0.5 * M_PI * (double(i) / double(nb_shapes))), 2);
        const PointF point = b.point_at_percent(percent);
        RectF re = RectF(start_point, point).normalized();
        double approx_thickness_at_percent = mid_thickness;
        if (slur_tie_seg->slur_tie()->style_type() == SlurStyleType::SOLID) {
            approx_thickness_at_percent *= (1 - 2 * std::abs(0.5 - percent));
        }
        if (re.height() < approx_thickness_at_percent) {
            double adjust = (approx_thickness_at_percent - re.height()) * .5;
            re.adjust(0.0, -adjust, 0.0, adjust);
        }
        shape.add(re, slur_tie_seg);
        start_point = point;
    }

    slur_tie_seg->mutldata()->set_shape(shape);
}
