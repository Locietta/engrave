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
#include "tremololayout.h"

#include "beamtremololayout.h"
#include "chordlayout.h"
#include "dom/chord.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/staff.h"
#include "dom/stem.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "global/types/number.h"
#include "stemlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void TremoloLayout::layout(TremoloTwoChord *item, const LayoutContext &ctx) {
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    item->set_chord1(to_chord(item->ownership_parent()));

    Note *anchor1 = item->chord1()->up() ? item->chord1()->up_note() : item->chord1()->down_note();
    Stem *stem = item->chord1()->stem();
    double x, y, h;
    if (stem) {
        x = stem->pos().x() + stem->width() / 2 * (item->chord1()->up() ? -1.0 : 1.0);
        y = stem->pos().y();
        h = stem->length();
    } else {
        // center tremolo above note
        x = anchor1->x() + anchor1->head_width() * 0.5;
        y = anchor1->y();
        h = (ctx.conf().style_absolute(Sid::TREMOLO_NOTE_SIDE_PADDING) + item->ldata()->bbox().height()) * item->chord1()->intrinsic_mag();
    }

    layout_two_notes_tremolo(item, ctx, x, y, h, item->spatium());
}

void TremoloLayout::layout(TremoloSingleChord *item, const LayoutContext &ctx) {
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    item->compute_shape(); // set bbox
    item->set_path(item->base_path());

    Note *anchor1 = item->chord()->up() ? item->chord()->up_note() : item->chord()->down_note();
    Stem *stem = item->chord()->stem();
    double x, y, h;
    if (stem) {
        x = stem->pos().x() + stem->width() / 2 * (item->chord()->up() ? -1.0 : 1.0);
        y = stem->pos().y();
        h = stem->length();
    } else {
        // center tremolo above note
        x = anchor1->x() + anchor1->head_width() * 0.5;

        bool has_mirrored_note = false;
        for (Note *n : item->chord()->notes()) {
            if (n->ldata()->mirror.value()) {
                has_mirrored_note = true;
                break;
            }
        }

        if (has_mirrored_note) {
            x = StemLayout::StemLayout::stem_pos_x(item->chord());
        }

        y = anchor1->y();
        h = (ctx.conf().style_absolute(Sid::TREMOLO_NOTE_SIDE_PADDING) + item->ldata()->bbox().height()) * item->chord()->intrinsic_mag();
    }

    layout_one_note_tremolo(item, ctx, x, y, h, item->spatium());
}

//---------------------------------------------------------
//   layoutOneNoteTremolo
//---------------------------------------------------------

void TremoloLayout::layout_one_note_tremolo(TremoloSingleChord *item, const LayoutContext &ctx, double x, double y, double h,
                                            double spatium) {
    const StaffType *staff_type = item->staff_type();

    if (staff_type && staff_type->is_tab_staff()) {
        x = ChordLayout::center_x(item->chord());
    }

    bool up = item->chord()->up();
    int up_value = up ? -1 : 1;
    double mag = item->chord()->intrinsic_mag();
    spatium *= mag;

    double y_offset = h - ctx.conf().style_absolute(Sid::TREMOLO_OUT_SIDE_PADDING) * mag;

    int beams = item->chord()->beams();
    if (item->chord()->hook()) {
        // allow for space at the hook side of the stem (yOffset)
        // straight flags and traditional flags have different requirements because of their slopes
        // away from the stem. Straight flags have a shallower slope and a lot more space in general
        // so we can place the trem higher in that case
        bool straight_flags = ctx.conf().style_b(Sid::USE_STRAIGHT_NOTE_FLAGS);
        if (straight_flags) {
            y_offset -= 0.75 * spatium;
        } else {
            // up-hooks and down-hooks are shaped differently
            y_offset -= up ? 1.5 * spatium : 1.0 * spatium;
        }
        // we need an additional offset for beams > 2 since those beams extend outwards and we don't want to adjust for that
        double beam_offset = straight_flags ? 0.75 : 0.5;
        y_offset -= beams >= 2 ? beam_offset * spatium : 0.0;
    } else if (beams && staff_type && !staff_type->is_simple_tab_staff()) {
        y_offset -= (beams * (ctx.conf().style_b(Sid::USE_WIDE_BEAMS) ? 1.0 : 0.75) - 0.25) * spatium;
    }
    y_offset -= item->is_buzz_roll() && up ? 0.5 * spatium : 0.0;
    y_offset -= up ? 0.0 : item->min_height() * spatium / mag;
    y_offset *= up_value;
    y += y_offset;

    if (up) {
        double height = item->is_buzz_roll() ? 0 : item->min_height();
        double stave_height = (((item->staff()->lines(item->tick()) - 1) - height) * spatium / mag);
        y = std::min(y, stave_height);
    } else {
        y = std::max(y, 0.0);
    }
    item->set_pos(x, y);
}

void TremoloLayout::calc_is_up(TremoloTwoChord *item) {
    if (item->user_modified()) {
        bool start_chord_up = ChordLayout::is_chord_pos_below_trem(item->chord1(), item);
        bool end_chord_up = ChordLayout::is_chord_pos_below_trem(item->chord2(), item);

        item->set_up(start_chord_up || end_chord_up);
        item->chord1()->set_up(start_chord_up);
        item->chord2()->set_up(end_chord_up);
        return;
    }

    int up = 0;
    bool is_up = item->up();
    bool has_voices = item->chord1()->measure()->has_voices(item->chord1()->staff_idx(), item->chord1()->tick(),
                                                            item->chord2()->tick() - item->chord1()->tick());
    if (item->chord1()->stem_direction() == DirectionV::AUTO && item->chord2()->stem_direction() == DirectionV::AUTO &&
        item->chord1()->staff_move() == item->chord2()->staff_move() && !has_voices) {
        std::vector<int> chord1_distances = item->chord1()->note_distances();
        std::vector<int> chord2_distances = item->chord2()->note_distances();
        std::vector<int> note_distances;
        note_distances.reserve(chord1_distances.size() + chord2_distances.size());
        for (int distance : chord1_distances) {
            note_distances.push_back(distance);
        }
        for (int distance : chord2_distances) {
            note_distances.push_back(distance);
        }
        std::sort(note_distances.begin(), note_distances.end());
        up = ChordLayout::compute_auto_stem_direction(note_distances);

        is_up = up > 0;
    } else if (item->chord1()->stem_direction() != DirectionV::AUTO) {
        is_up = item->chord1()->stem_direction() == DirectionV::UP;
    } else if (item->chord2()->stem_direction() != DirectionV::AUTO) {
        is_up = item->chord2()->stem_direction() == DirectionV::UP;
    } else if (item->chord1()->staff_move() > 0 || item->chord2()->staff_move() > 0) {
        is_up = false;
    } else if (item->chord1()->staff_move() < 0 || item->chord2()->staff_move() < 0) {
        is_up = true;
    } else if (has_voices) {
        is_up = item->chord1()->track() % 2 == 0;
    }

    item->set_up(is_up);
    item->chord1()->set_up(item->chord1()->staff_move() == 0 ? is_up : !is_up); // if on a different staff, flip stem dir
    item->chord2()->set_up(item->chord2()->staff_move() == 0 ? is_up : !is_up);
}

//---------------------------------------------------------
//   layoutTwoNotesTremolo
//---------------------------------------------------------

void TremoloLayout::layout_two_notes_tremolo(TremoloTwoChord *item, const LayoutContext &ctx, double x, double y, double h,
                                             double spatium) {
    UNUSED(x);
    UNUSED(y);
    UNUSED(h);
    UNUSED(spatium);

    TremoloTwoChord::LayoutData *ldata = item->mutldata();

    // make sure both stems are in the same direction
    if (item->chord1()->beam() && item->chord1()->beam() == item->chord2()->beam()) {
        Beam *beam = item->chord1()->beam();
        item->set_up(beam->up());
        item->do_set_direction(beam->direction());
        // stem stuff is already taken care of by the beams
    } else {
        TremoloLayout::calc_is_up(item);

        if (!item->user_modified()) {
            // user modified trems will be dealt with later
            ChordLayout::layout_stem(item->chord1(), ctx);
            ChordLayout::layout_stem(item->chord2(), ctx);
        }
    }

    BeamTremoloLayout::setup_l_data(item, item->mutldata(), ctx);
    item->set_start_anchor(BeamTremoloLayout::chord_beam_anchor(item->ldata(), item->chord1(), ChordBeamAnchorType::START));
    item->set_end_anchor(BeamTremoloLayout::chord_beam_anchor(item->ldata(), item->chord2(), ChordBeamAnchorType::END));

    // deal with manual adjustments here and return
    if (item->user_modified()) {
        int idx = item->direction_idx();
        double start_y = item->beam_fragment().py1[idx];
        double end_y = item->beam_fragment().py2[idx];
        if (ctx.conf().style_b(Sid::SNAP_CUSTOM_BEAMS_TO_GRID)) {
            const double quarter_space = item->EngravingItem::spatium() / 4;
            start_y = round(start_y / quarter_space) * quarter_space;
            end_y = round(end_y / quarter_space) * quarter_space;
        }
        start_y += item->page_pos().y();
        end_y += item->page_pos().y();
        item->start_anchor().set_y(start_y);
        item->end_anchor().set_y(end_y);
        item->mutldata()->set_anchors(item->start_anchor(), item->end_anchor());

        ChordLayout::layout_stem(item->chord1(), ctx);
        ChordLayout::layout_stem(item->chord2(), ctx);

        create_beam_segments(item, ctx);
        return;
    }
    ldata->set_pos_y(0.);
    std::vector<ChordRest *> chord_rests{item->chord1(), item->chord2()};
    std::vector<BeamBase::NotePosition> notes;
    double mag = 0.0;

    notes.clear();
    for (ChordRest *cr : chord_rests) {
        double m = cr->is_small() ? ctx.conf().style_d(Sid::SMALL_NOTE_MAG) : 1.0;
        mag = std::max(mag, m);
        if (cr->is_chord()) {
            Chord *chord = to_chord(cr);
            //int i = chord->staffMove();
            //_minMove = std::min(_minMove, i); todo: investigate this
            //_maxMove = std::max(_maxMove, i);

            for (int distance : chord->note_distances()) {
                notes.push_back(BeamBase::NotePosition(distance, chord->v_staff_idx()));
            }
        }
    }

    std::sort(notes.begin(), notes.end());
    ldata->set_mag(mag);
    BeamTremoloLayout::calculate_anchors(item, item->mutldata(), ctx, chord_rests, notes);
    item->set_start_anchor(item->ldata()->start_anchor);
    item->set_end_anchor(item->ldata()->end_anchor);

    int idx = item->direction_idx();
    item->beam_fragment().py1[idx] = item->start_anchor().y() - item->page_pos().y();
    item->beam_fragment().py2[idx] = item->end_anchor().y() - item->page_pos().y();
    create_beam_segments(item, ctx);
}

//---------------------------------------------------------
//   extendedStemLenWithTwoNotesTremolo
//    Goal: To extend stem of one of the chords to make the tremolo less steep
//    Returns a modified pair of stem lengths of two chords
//---------------------------------------------------------

std::pair<double, double> TremoloLayout::extended_stem_len_with_two_note_tremolo(TremoloTwoChord *tremolo, double stem_len1,
                                                                                 double stem_len2) {
    const double spatium = tremolo->spatium();
    Chord *c1 = tremolo->chord1();
    Chord *c2 = tremolo->chord2();
    Stem *s1 = c1->stem();
    Stem *s2 = c2->stem();
    const double sgn1 = c1->up() ? -1.0 : 1.0;
    const double sgn2 = c2->up() ? -1.0 : 1.0;
    const double stem_tip_distance =
        (s1 && s2) ? (s2->page_pos().y() + stem_len2) - (s1->page_pos().y() + stem_len1) :
                     (StemLayout::stem_pos(c2).y() + (sgn2 * stem_len2)) - (StemLayout::stem_pos(c1).y() + (sgn1 * stem_len1));

    // same staff & same direction: extend one of the stems
    if (c1->staff_move() == c2->staff_move() && c1->up() == c2->up()) {
        const bool stem1_higher = stem_tip_distance > 0.0;
        if (std::abs(stem_tip_distance) > 1.0 * spatium) {
            if ((c1->up() && !stem1_higher) || (!c1->up() && stem1_higher)) {
                return {stem_len1 + (std::abs(stem_tip_distance) - spatium), stem_len2};
            } else { /* if ((c1->up() && stem1Higher) || (!c1->up() && !stem1Higher)) */
                return {stem_len1, stem_len2 + (std::abs(stem_tip_distance) - spatium)};
            }
        }
    }

    return {stem_len1, stem_len2};
}

void TremoloLayout::create_beam_segments(TremoloTwoChord *item, const LayoutContext &ctx) {
    // TODO: This should be a style setting, to replace tremoloStrokeLengthMultiplier
    static constexpr double stem_gap_sp = 1.0;
    const bool default_style = (!item->custom_style_applicable()) || (item->tremolo_style() == TremoloStyle::DEFAULT);

    item->clear_beam_segments();

    bool is_grace = item->chord1()->is_grace();
    const PointF page_pos = item->page_pos();
    PointF start_anchor = item->ldata()->start_anchor - PointF(0.0, page_pos.y());
    PointF end_anchor = item->ldata()->end_anchor - PointF(0.0, page_pos.y());

    // inset trem from stems for default style
    const double slope = muse::divide(end_anchor.y() - start_anchor.y(), end_anchor.x() - start_anchor.x(), 0.0);

    double gap_sp = stem_gap_sp;
    if (default_style || item->tremolo_style() == TremoloStyle::TRADITIONAL_ALTERNATE) {
        // we can eat into the stemGapSp margin if the anchorpoints are sufficiently close together
        double width_sp = (end_anchor.x() - start_anchor.x()) / item->spatium() - (stem_gap_sp * 2);
        if (!muse::real_is_equal_or_more(width_sp, 0.6)) {
            // tremolo beam is too short; we can eat into the gap spacing a little
            gap_sp = std::max(stem_gap_sp - ((0.6 - width_sp) * 0.5), 0.4);
        }
    } else {
        gap_sp = 0.0;
    }
    BeamSegment *main_stroke = new BeamSegment(item);
    PointF x_offset = PointF(gap_sp * item->spatium(), 0);
    PointF y_offset = PointF(0, gap_sp * item->spatium() * slope);
    if (item->tremolo_style() == TremoloStyle::TRADITIONAL_ALTERNATE) {
        main_stroke->line = LineF(start_anchor, end_anchor);
        start_anchor += x_offset;
        end_anchor -= x_offset;
        start_anchor += y_offset;
        end_anchor -= y_offset;
    } else {
        start_anchor += x_offset;
        end_anchor -= x_offset;
        start_anchor += y_offset;
        end_anchor -= y_offset;
        main_stroke->line = LineF(start_anchor, end_anchor);
    }
    main_stroke->level = 0;

    item->beam_segments().push_back(main_stroke);

    LineF line = main_stroke->line;
    double half_width = 0.5 * ctx.conf().style_absolute(Sid::BEAM_WIDTH);
    double bbox_top = std::min(line.y1(), line.y2()) - half_width;
    double bbox_bottom = std::max(line.y1(), line.y2()) + half_width;

    RectF bbox = RectF(line.x1(), bbox_top, line.x2() - line.x1(), bbox_bottom - bbox_top);

    PointF beam_offset = PointF(0., (item->up() ? 1 : -1) * item->spatium() * (ctx.conf().style_b(Sid::USE_WIDE_BEAMS) ? 1. : 0.75));
    beam_offset.set_y(beam_offset.y() * item->mag() * (is_grace ? ctx.conf().style_d(Sid::GRACE_NOTE_MAG) : 1.));
    for (int i = 1; i < item->lines(); ++i) {
        BeamSegment *stroke = new BeamSegment(item);
        stroke->level = i;
        stroke->line = LineF(start_anchor + (beam_offset * (double) i), end_anchor + (beam_offset * (double) i));
        item->beam_segments().push_back(stroke);
        bbox.unite(bbox.translated(0., beam_offset.y() * (double) i));
    }
    item->setbbox(bbox);

    // size stems properly
    if (item->chord1()->stem() && item->chord2()->stem() && !(item->chord1()->beam() && item->chord1()->beam() == item->chord2()->beam())) {
        // we don't need to do anything if these chords are part of the same beam--their stems are taken care of
        // by the beam layout
        int beam_spacing = ctx.conf().style_b(Sid::USE_WIDE_BEAMS) ? 4 : 3;
        for (ChordRest *cr : {item->chord1(), item->chord2()}) {
            Chord *chord = to_chord(cr);
            double addition = 0.0;
            if (cr->up() != item->up() && item->lines() > 1) {
                // need to adjust further for beams on the opposite side
                addition += (item->lines() - 1.) * beam_spacing / 4. * item->spatium() * item->mag();
            }
            // calling extendStem with addition 0.0 still sizes the stem to the manually adjusted height of the trem.
            BeamTremoloLayout::extend_stem(item->ldata(), chord, addition);
        }
    }
}
