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

#include "restlayout.h"

#include "beamlayout.h"
#include "chordlayout.h"
#include "dom/beam.h"
#include "dom/measure.h"
#include "dom/staff.h"
#include "dom/system.h"
#include "log.h"
#include "tlayout.h"

using namespace muse;
using namespace mu::engraving;

namespace mu::engraving::rendering::score {
void RestLayout::layout_rest(const Rest *item, Rest::LayoutData *ldata, const LayoutContext &ctx) {
    if (item->is_gap()) {
        if (item->debug_draw_gap()) {
            ldata->sym = item->get_symbol(item->duration_type().type(), 16, 5);
            fill_shape(item, ldata, ctx.conf());
            ldata->set_pos(PointF(0.0, 10 * item->spatium()));
        } else {
            ldata->reset();
        }
        return;
    }

    //! NOTE The types are listed here explicitly to show what types there are (see Rest::add method)
    //! and accordingly show what depends on.
    for (EngravingItem *e : item->el()) {
        switch (e->type()) {
            case ElementType::DEAD_SLAPPED: {
                DeadSlapped *ds = item_cast<DeadSlapped *>(e);
                LD_INDEPENDENT;
                TLayout::layout_dead_slapped(ds, ds->mutldata());
            } break;
            case ElementType::SYMBOL: {
                Symbol *s = item_cast<Symbol *>(e);
                // LD_X not clear yet
                TLayout::layout_symbol(s, s->mutldata(), ctx);
            } break;
            case ElementType::IMAGE: {
                Image *im = item_cast<Image *>(e);
                LD_INDEPENDENT;
                TLayout::layout_image(im, im->mutldata());
            } break;
            default: UNREACHABLE;
        }
    }

    if (item->dead_slapped()) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);

    double spatium = item->spatium();

    ldata->set_pos_x(0.0);
    const StaffType *stt = item->staff_type();
    if (stt && stt->is_tab_staff()) {
        // if rests are shown and note values are shown as duration symbols
        if (stt->show_rests() && stt->gen_durations()) {
            DurationType type = item->duration_type().type();
            int dots = item->duration_type().dots();
            // if rest is whole measure, convert into actual type and dot values
            if (type == DurationType::V_MEASURE && item->measure()) {
                Fraction ticks = item->measure()->ticks();
                TDuration dur = TDuration(ticks).type();
                type = dur.type();
                dots = dur.dots();
            }
            // symbol needed; if not exist, create, if exists, update duration
            if (!item->tab_dur()) {
                const_cast<Rest *>(item)->set_tab_dur(new TabDurationSymbol(const_cast<Rest *>(item), stt, type, dots));
            } else {
                item->tab_dur()->set_duration(type, dots, stt);
            }
            // needed?        _tabDur->setTrack(track());
            TLayout::layout_tab_duration_symbol(item->tab_dur(), item->tab_dur()->mutldata());
            ldata->set_bbox(item->tab_dur()->ldata()->bbox());
            ldata->set_pos(0.0, 0.0); // no rest is drawn: reset any position might be set for it
            return;
        }
        // if no rests or no duration symbols, delete any dur. symbol and chain into standard staff mngmt
        // this is to ensure horiz space is reserved for rest, even if they are not displayed
        // Rest::draw() will skip their drawing, if not needed
        if (item->tab_dur()) {
            delete item->tab_dur();
            const_cast<Rest *>(item)->set_tab_dur(nullptr);
        }
    }

    const_cast<Rest *>(item)->set_dot_line(Rest::get_dotline(item->duration_type().type()));

    double y_off = item->offset().y();
    const Staff *stf = item->staff();
    const StaffType *st = stf ? stf->staff_type_for_element(item) : 0;
    double line_dist = st ? st->line_distance().val() : 1.0;
    int user_line = real_is_null(y_off) ? 0 : lrint(y_off / (line_dist * spatium));
    int lines = st ? st->lines() : 5;

    int natural_line = compute_natural_line(lines);       // Measured in 1sp steps
    int voice_offset = compute_voice_offset(item, ldata); // Measured in 1sp steps
    int whole_rest_offset = compute_whole_or_breve_rest_offset(item, voice_offset, lines);
    int final_line = natural_line + voice_offset + whole_rest_offset;

    ldata->sym = item->get_symbol(item->duration_type().type(), final_line + user_line, lines);

    ldata->set_pos_y(final_line * line_dist * spatium);
    if (!item->should_not_be_drawn()) {
        fill_shape(item, ldata, ctx.conf());
    }

    const_cast<Rest *>(item)->check_dots();
    double visible_x = item->sym_width_no_ledger_lines(ldata) + ctx.conf().style_absolute(Sid::DOT_NOTE_DISTANCE) * item->mag();
    double visible_dx = ctx.conf().style_absolute(Sid::DOT_DOT_DISTANCE) * item->mag();
    double invisible_x = item->sym_width_no_ledger_lines(ldata);
    double y = item->dot_line() * item->spatium() * .5;
    for (NoteDot *dot : item->dot_list()) {
        NoteDot::LayoutData *dotldata = dot->mutldata();
        TLayout::layout_note_dot(dot, dotldata);
        if (dot->visible()) {
            dotldata->set_pos(visible_x, y);
            visible_x += visible_dx;
        } else {
            invisible_x += 0.1 * item->spatium();
            dotldata->set_pos(invisible_x, y);
            invisible_x += item->sym_width(SymId::AUGMENTATION_DOT) * dot->mag();
        }
    }
}

void RestLayout::fill_shape(const Rest *item, Rest::LayoutData *ldata, const LayoutConfiguration &conf) {
    switch (item->type()) {
        case ElementType::REST: fill_shape(to_rest(item), static_cast<Rest::LayoutData *>(ldata)); break;
        case ElementType::MMREST: fill_shape(static_cast<const MMRest *>(item), static_cast<MMRest::LayoutData *>(ldata), conf); break;
        default: UNREACHABLE; return;
    }
}

void RestLayout::resolve_vertical_rest_conflicts(LayoutContext &ctx, Segment *segment, staff_idx_t staff_idx) {
    std::vector<Rest *> rests;
    std::vector<Chord *> chords;

    collect_chords_and_rest(segment, staff_idx, chords, rests);

    if (rests.empty()) {
        return;
    }

    collect_chords_overlapping_rests(segment, staff_idx, chords);

    for (Rest *rest : rests) {
        rest->vertical_clearance().reset();
    }

    const Staff *staff = ctx.dom().staff(staff_idx);
    if (!chords.empty()) {
        resolve_rest_vs_chord(rests, chords, staff, segment);
    }

    if (rests.size() < 2) {
        return;
    }

    resolve_rest_vs_rest(rests, staff, segment, ctx);
}

void RestLayout::resolve_rest_vs_chord(std::vector<Rest *> &rests, std::vector<Chord *> &chords, const Staff *staff, Segment *segment) {
    Fraction tick = segment->tick();
    int lines = staff->lines(tick);
    double spatium = staff->spatium(tick);
    double line_distance = staff->line_distance(tick) * spatium;

    for (Rest *rest : rests) {
        if (!rest->visible() || !rest->autoplace()) {
            continue;
        }
        bool is_whole_or_half = rest->is_whole_rest() || rest->duration_type() == DurationType::V_HALF;
        double min_rest_to_chord_clearance = is_whole_or_half ? 0.55 * spatium : 0.35 * spatium;
        RestVerticalClearance &rest_vertical_clearance = rest->vertical_clearance();
        for (Chord *chord : chords) {
            if (!chord->visible() || !chord->autoplace()) {
                continue;
            }

            bool rest_above = rest->voice() < chord->voice() || (chord->slash() && !(rest->voice() % 2));
            int up_sign = rest_above ? -1 : 1;
            double rest_y_offset = rest->offset().y();
            bool ignore_y_offset = (rest_above && rest_y_offset > 0) || (!rest_above && rest_y_offset < 0);
            PointF offset = ignore_y_offset ? PointF(0, rest_y_offset) : PointF(0, 0);

            Shape chord_shape = chord->shape().translate(chord->pos());
            chord_shape.remove_invisibles();
            chord_shape.remove_types({ElementType::ARPEGGIO});
            if (chord_shape.empty()) {
                continue;
            }

            double clearance = 0.0;
            Shape rest_shape = rest->shape().translate(rest->pos() - offset);
            if (chord->segment() == rest->segment()) {
                clearance = rest_above ? rest_shape.vertical_clearance(chord_shape) : chord_shape.vertical_clearance(rest_shape);
            } else {
                Note *limit_note = rest_above ? chord->up_note() : chord->down_note();
                RectF note_shape = limit_note->sym_bbox(limit_note->note_head()).translated(limit_note->pos());
                clearance = rest_above ? note_shape.top() - rest_shape.bottom() : rest_shape.top() - note_shape.bottom();
                min_rest_to_chord_clearance = 0.0;
            }

            double margin = clearance - min_rest_to_chord_clearance;
            int margin_in_steps = floor(margin / line_distance);
            if (rest_above) {
                rest_vertical_clearance.set_below(margin_in_steps);
            } else {
                rest_vertical_clearance.set_above(margin_in_steps);
            }
            if (margin > 0) {
                continue;
            }

            rest->vertical_clearance().set_locked(true);
            bool out_above_staff = rest_above && rest_shape.bottom() + margin < min_rest_to_chord_clearance;
            bool out_below_staff = !rest_above && rest_shape.top() - margin > (lines - 1) * line_distance - min_rest_to_chord_clearance;
            bool use_half_space_steps = (out_above_staff || out_below_staff) && !is_whole_or_half;
            double y_move;
            if (use_half_space_steps) {
                int steps = ceil(std::abs(margin) / (line_distance / 2));
                y_move = steps * line_distance / 2 * up_sign;
                rest->mutldata()->move_y(y_move);
            } else {
                int steps = ceil(std::abs(margin) / line_distance);
                y_move = steps * line_distance * up_sign;
                rest->mutldata()->move_y(y_move);
            }
            for (Rest *merged_rest : rest->ldata()->merged_rests) {
                merged_rest->mutldata()->move_y(y_move);
            }
            if (is_whole_or_half) {
                update_symbol(rest, rest->mutldata()); // Because it may need to use the symbol with ledger line now
            }
        }
    }
}

void RestLayout::resolve_rest_vs_rest(std::vector<Rest *> &rests, const Staff *staff, Segment *segment, LayoutContext &ctx,
                                      bool consider_beams) {
    if (rests.empty()) {
        return;
    }

    Fraction tick = segment->tick();
    double spatium = staff->spatium(tick);
    double line_distance = staff->line_distance(tick) * spatium;
    int lines = staff->lines(tick);
    const double min_rest_to_rest_clearance = 0.55 * spatium;

    for (size_t i = 0; i < rests.size() - 1; ++i) {
        Rest *rest1 = rests[i];
        if (!rest1->visible() || !rest1->autoplace()) {
            continue;
        }

        RestVerticalClearance &rest1_clearance = rest1->vertical_clearance();
        Shape shape1 = rest1->shape().translate(rest1->pos() - rest1->offset());

        Rest *rest2 = rests[i + 1];
        if (!rest2->visible() || !rest2->autoplace()) {
            continue;
        }

        if (muse::contains(rest1->ldata()->merged_rests, rest2) || muse::contains(rest2->ldata()->merged_rests, rest1)) {
            continue;
        }

        Shape shape2 = rest2->shape().translate(rest2->pos() - rest2->offset());
        RestVerticalClearance &rest2_clearance = rest2->vertical_clearance();

        double clearance;
        bool first_above = rest1->voice() < rest2->voice();
        if (first_above) {
            clearance = shape1.vertical_clearance(shape2);
        } else {
            clearance = shape2.vertical_clearance(shape1);
        }
        double margin = clearance - min_rest_to_rest_clearance;
        int margin_in_steps = floor(margin / line_distance);
        if (first_above) {
            rest1_clearance.set_below(margin_in_steps);
            rest2_clearance.set_above(margin_in_steps);
        } else {
            rest1_clearance.set_above(margin_in_steps);
            rest2_clearance.set_below(margin_in_steps);
        }

        if (margin > 0) {
            continue;
        }

        int steps = ceil(std::abs(margin) / line_distance);
        // Move the two rests away from each other
        int step1 = floor(double(steps) / 2);
        int step2 = ceil(double(steps) / 2);
        int max_step1 = first_above ? rest1_clearance.above() : rest1_clearance.below();
        int max_step2 = first_above ? rest2_clearance.below() : rest2_clearance.above();
        max_step1 = std::max(max_step1, 0);
        max_step2 = std::max(max_step2, 0);
        if (step1 > max_step1) {
            step2 += step1 - max_step1; // First rest is locked, try move the second more
        }
        if (step2 > max_step2) {
            step1 += step2 - max_step2; // Second rest is locked, try move the first more
        }
        step1 = std::min(step1, max_step1);
        step2 = std::min(step2, max_step2);
        rest1->mutldata()->move_y(step1 * line_distance * (first_above ? -1 : 1));
        rest2->mutldata()->move_y(step2 * line_distance * (first_above ? 1 : -1));

        Beam *beam1 = rest1->beam();
        Beam *beam2 = rest2->beam();
        if (beam1 && beam2 && consider_beams) {
            shape1 = rest1->shape().translate(rest1->pos() - rest1->offset());
            shape2 = rest2->shape().translate(rest2->pos() - rest2->offset());

            ChordRest *beam1_start = beam1->elements().front();
            ChordRest *beam1_end = beam1->elements().back();
            double y1_start = BeamLayout::chord_beam_anchor_y(beam1, beam1_start) - beam1_start->page_pos().y();
            double y1_end = BeamLayout::chord_beam_anchor_y(beam1, beam1_end) - beam1_end->page_pos().y();
            double beam1_ymid = 0.5 * (y1_start + y1_end);

            ChordRest *beam2_start = beam2->elements().front();
            ChordRest *beam2_end = beam2->elements().back();
            double y2_start = BeamLayout::chord_beam_anchor_y(beam2, beam2_start) - beam2_start->page_pos().y();
            double y2_end = BeamLayout::chord_beam_anchor_y(beam2, beam2_end) - beam2_end->page_pos().y();
            double beam2_ymid = 0.5 * (y2_start + y2_end);

            double center_y = 0.5 * (beam1_ymid + beam2_ymid);

            double upper_bound = shape1.bottom();
            double lower_bound = shape2.top();
            int steps2 = 0;
            if (center_y < upper_bound) {
                steps2 = floor((center_y - upper_bound) / line_distance);
            } else if (center_y > lower_bound) {
                steps2 = ceil((center_y - lower_bound) / line_distance);
            }
            double move_y = steps2 * line_distance;
            rest1->mutldata()->move_y(move_y);
            rest2->mutldata()->move_y(move_y);
            shape1.translate(PointF(0.0, move_y));
            shape2.translate(PointF(0.0, move_y));

            double half_line_distance = 0.5 * line_distance;
            if (shape1.bottom() < -half_line_distance) {
                rest1->mutldata()->move_y(half_line_distance);
            } else if (center_y >= (lines - 1) * line_distance + half_line_distance) {
                rest2->mutldata()->move_y(-half_line_distance);
            }

            rest1->vertical_clearance().set_locked(true);
            rest2->vertical_clearance().set_locked(true);
            TLayout::layout_beam(beam1, ctx);
            TLayout::layout_beam(beam2, ctx);
        }

        bool rest1_is_whole_or_half = rest1->is_whole_rest() || rest1->duration_type() == DurationType::V_HALF;
        bool rest2_is_whole_or_half = rest2->is_whole_rest() || rest2->duration_type() == DurationType::V_HALF;

        if (rest1_is_whole_or_half) {
            update_symbol(rest1, rest1->mutldata());
        }
        if (rest2_is_whole_or_half) {
            update_symbol(rest2, rest2->mutldata());
        }
    }
}

void RestLayout::align_rests(const System *system, LayoutContext &ctx) {
    RestGroups rest_groups = compute_rest_groups(system, ctx);

    for (RestGroup &group : rest_groups) {
        if (group.size() <= 1) {
            continue;
        }

        Rest *first_rest = group.front();
        const bool align_upwards = first_rest->voice() == 0;
        const double line_dist = first_rest->staff()->line_distance(first_rest->tick()) * first_rest->spatium();

        double y_outer_rest = align_upwards ? DBL_MAX : -DBL_MAX;
        for (Rest *rest : group) {
            double y_rest = rest->ldata()->pos().y();
            y_outer_rest = align_upwards ? std::min(y_outer_rest, y_rest) : std::max(y_outer_rest, y_rest);
        }
        for (Rest *rest : group) {
            double y_cur = rest->ldata()->pos().y();
            double y_result = y_outer_rest;
            double rest_vert_clearance =
                line_dist * (align_upwards ? rest->vertical_clearance().above() : rest->vertical_clearance().below());
            double vert_padding = line_dist;
            rest_vert_clearance = std::max(0.0, rest_vert_clearance - vert_padding);
            double required_move = y_outer_rest - y_cur;
            if (std::abs(required_move) > rest_vert_clearance) {
                y_result = align_upwards ? y_cur - rest_vert_clearance : y_cur + rest_vert_clearance;
            }
            rest->mutldata()->set_pos_y(y_result);
            if (rest->is_whole_rest() || rest->duration_type() == DurationType::V_HALF) {
                update_symbol(rest, rest->mutldata());
            }
        }
    }
}

RestGroups RestLayout::compute_rest_groups(const System *system, LayoutContext &ctx) {
    RestGroups rest_groups;

    for (staff_idx_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        for (const MeasureBase *mb : system->measures()) {
            if (!(mb->is_measure() && to_measure(mb)->has_voices(staff_idx))) {
                continue;
            }

            const Measure *measure = to_measure(mb);
            track_idx_t s_track = staff2track(staff_idx);

            InterruptionPoints interruption_points = compute_interruption_points(measure, staff_idx);

            for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
                rest_groups.push_back(RestGroup());

                std::list<Fraction> &interruption_points_on_this_voice = interruption_points[voice];
                track_idx_t track = s_track + voice;

                for (const Segment *segment = measure->first(SegmentType::CHORD_REST); segment;
                     segment = segment->next(SegmentType::CHORD_REST)) {
                    if (interruption_points_on_this_voice.size() > 0 && segment->rtick() >= interruption_points_on_this_voice.front()) {
                        rest_groups.push_back(RestGroup());
                        interruption_points_on_this_voice.pop_front();
                    }

                    EngravingItem *item = segment->element(track);
                    if (!(item && item->is_rest() && item->visible())) {
                        continue;
                    }
                    Rest *rest = to_rest(item);
                    if (rest->staff_move() == 0 && !rest->is_gap() && rest->align_with_other_rests()) {
                        rest_groups.back().push_back(rest);
                    }
                }
            }
        }
    }

    return rest_groups;
}

InterruptionPoints RestLayout::compute_interruption_points(const Measure *measure, staff_idx_t staff_idx) {
    // Start with sets to get automatic uniqueness and ordering
    std::array<std::set<Fraction>, ::mu::engraving::voices> interruption_point_sets;

    track_idx_t s_track = staff_idx * ::mu::engraving::voices;
    track_idx_t e_track = s_track + ::mu::engraving::voices;

    // Compute all-voices interruptions
    for (const Segment *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
        for (track_idx_t track = s_track; track < e_track; ++track) {
            EngravingItem *item = segment->element(track);
            if (!item) {
                continue;
            }
            const bool gap_rest = item->is_rest() && to_rest(item)->is_gap();
            // TODO: The fact that "merged" rests exists in both voices and are just graphically overlapped is a messy way of
            // doing it and way too fragile, because it means that any logic that may move one rest can break the "merging".
            // A more solid way of merging rests would be to *delete* the second voice rests, i.e. turn them into gap rests [M.S.].
            const bool has_merged_rest = item->is_rest() && !to_rest(item)->ldata()->merged_rests.empty();
            const bool invisible = item->is_chord() ? to_chord(item)->all_elements_invisible() : !item->visible();
            if (gap_rest || has_merged_rest || invisible) {
                for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
                    interruption_point_sets[voice].insert(segment->rtick());
                    interruption_point_sets[voice].insert(segment->rtick() + to_chord_rest(item)->actual_ticks());
                }
                break;
            }
        }
    }

    auto interrupt_here = [&](ChordRest *chord_rest) {
        Fraction cr_ticks = chord_rest->ticks();
        if (cr_ticks > Fraction(1, 8)) {
            return true;
        }

        BeamMode beam_mode_chord_rest = Groups::base_beam_mode(chord_rest);
        switch (beam_mode_chord_rest) {
            case BeamMode::BEGIN:
                // Beam certainly starts here
                return true;
            case BeamMode::BEGIN16:
            case BeamMode::BEGIN32:
            case BeamMode::MID:
            case BeamMode::END:
                // Beam certainly doesn not start here
                return false;
            default: break;
        }

        // If BeamMode is AUTO or NONE check measure beaming
        const Groups &groups = chord_rest->staff()->group(chord_rest->tick());
        BeamMode beam_mode_measure = groups.beam_mode(chord_rest->rtick().ticks(), chord_rest->duration_type().type());
        return beam_mode_measure == BeamMode::BEGIN;
    };

    // Compute voice-spefic interruptions
    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        track_idx_t track = s_track + voice;
        for (const Segment *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
            if (ChordRest *this_cr = to_chord_rest(segment->element(track))) {
                if (this_cr->is_rest()) {
                    const Segment *prev_segment = segment->prev_with_elements_on_track(track);
                    bool has_adjacent_rest = prev_segment && prev_segment->element(track)->is_rest();
                    if (has_adjacent_rest) {
                        continue;
                    }
                }
                if (interrupt_here(this_cr)) {
                    interruption_point_sets[voice].insert(segment->rtick());
                }
            }
        }
    }

    // Convert sets to lists
    InterruptionPoints interruption_points;
    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        const std::set<Fraction> &set = interruption_point_sets[voice];
        interruption_points[voice] = std::list(set.begin(), set.end());
    }

    return interruption_points;
}

void RestLayout::check_full_measure_rest_collisions(const System *system, LayoutContext &ctx) {
    for (staff_idx_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        for (MeasureBase *mb : system->measures()) {
            if (!(mb->is_measure() && to_measure(mb)->has_voices(staff_idx))) {
                continue;
            }

            Measure *measure = to_measure(mb);
            Rest *full_measure_rest = nullptr;

            Segment *first_cr_seg = measure->first(SegmentType::CHORD_REST);
            IF_ASSERT_FAILED(first_cr_seg) { continue; }
            track_idx_t start_track = staff2track(staff_idx);
            track_idx_t end_track = start_track + ::mu::engraving::voices;
            for (track_idx_t track = start_track; track < end_track; ++track) {
                EngravingItem *item = first_cr_seg->element(track);
                if (item && item->is_rest() && to_rest(item)->is_full_measure_rest()) {
                    full_measure_rest = to_rest(item);
                    break;
                }
            }

            if (!full_measure_rest) {
                continue;
            }

            double x_rest = full_measure_rest->page_pos().x() - system->page_pos().x();
            Shape rest_shape = full_measure_rest->shape().translate(PointF(x_rest, full_measure_rest->ldata()->pos().y()));

            Shape measure_shape;
            for (const Segment &segment : measure->segments()) {
                if (!segment.is_active()) {
                    continue;
                }
                double x_segment = segment.page_pos().x() - system->page_pos().x();
                measure_shape.add(segment.staff_shape(staff_idx).translated(PointF(x_segment, 0.0)));
            }
            measure_shape.remove_if([](const ShapeElement &shape_el) {
                const EngravingItem *shape_item = shape_el.item();
                return shape_item && ((shape_item->is_rest() && to_rest(shape_item)->is_full_measure_rest()) || shape_item->is_bar_line() ||
                                      shape_item->is_accidental() || shape_item->is_fermata());
            });

            if (measure_shape.size() == 0) {
                continue;
            }

            const double spatium = full_measure_rest->spatium();
            const double line_dist = full_measure_rest->staff()->line_distance(full_measure_rest->tick()) * spatium;
            const double min_horizontal_distance = 4 * spatium;
            const double min_vert_clearance = 0.75 * spatium;

            bool align_above = full_measure_rest->voice() == 0;
            double vertical_clearance = align_above ? rest_shape.vertical_clearance(measure_shape, min_horizontal_distance) :
                                                      measure_shape.vertical_clearance(rest_shape, min_horizontal_distance);

            if (vertical_clearance < min_vert_clearance) {
                double diff = min_vert_clearance - vertical_clearance;
                int step_moves = std::ceil(diff / line_dist);
                double y_move = step_moves * line_dist;
                full_measure_rest->mutldata()->move_y(align_above ? -y_move : y_move);
                update_symbol(full_measure_rest, full_measure_rest->mutldata());
            }
        }
    }
}

void RestLayout::fill_shape(const Rest *item, Rest::LayoutData *ldata) {
    Shape shape(Shape::Type::COMPOSITE);

    if ((!item->is_gap() || item->debug_draw_gap()) && !item->should_not_be_drawn()) {
        shape.add(ChordLayout::chord_rest_shape(item));
        shape.add(item->sym_bbox(ldata->sym), item);
        for (const NoteDot *dot : item->dot_list()) {
            if (dot->add_to_skyline()) {
                shape.add(item->sym_bbox(SymId::AUGMENTATION_DOT).translated(dot->pos()), dot);
            }
        }
    }

    for (const EngravingItem *e : item->el()) {
        if (e->add_to_skyline()) {
            shape.add(e->shape().translate(e->pos()));
        }
    }

    ldata->set_shape(shape);
}

void RestLayout::fill_shape(const MMRest *item, MMRest::LayoutData *ldata, const LayoutConfiguration &conf) {
    Shape shape(Shape::Type::COMPOSITE);

    double v_stroke_height = conf.style_absolute(Sid::MM_REST_H_BAR_V_STROKE_HEIGHT);
    shape.add(RectF(0.0, -(v_stroke_height * .5), ldata->rest_width, v_stroke_height), item);
    if (item->show_number()) {
        shape.add(item->number_rect().translated(item->number_pos()), item);
    }

    ldata->set_shape(shape);
}

int RestLayout::compute_natural_line(int lines) {
    int line = (lines % 2) ? floor(double(lines) / 2) : ceil(double(lines) / 2);
    return line;
}

int RestLayout::compute_voice_offset(const Rest *item, Rest::LayoutData *ldata) {
    ldata->merged_rests.clear();
    Segment *s = item->segment();
    Measure *measure = item->measure();
    bool offset_voices = s && measure && (item->voice() > 0 || measure->has_voices(item->staff_idx(), item->tick(), item->actual_ticks()));
    if (offset_voices && item->voice() == 0) {
        // do not offset voice 1 rest if there exists a matching invisible rest in voice 2;
        EngravingItem *e = s->element(item->track() + 1);
        if (e && e->is_rest() && !e->visible() && !to_rest(e)->is_gap()) {
            Rest *r = to_rest(e);
            if (r->global_ticks() == item->global_ticks()) {
                offset_voices = false;
            }
        }
    }

    if (offset_voices && item->voice() < 2) {
        // in slash notation voices 1 and 2 are not offset outside the staff
        // if the staff contains slash notation then only offset rests in voices 3 and 4
        track_idx_t base_track = item->staff_idx() * ::mu::engraving::voices;
        for (voice_idx_t v = 0; v < ::mu::engraving::voices; ++v) {
            EngravingItem *e = s->element(base_track + v);
            if (e && e->is_chord() && to_chord(e)->slash()) {
                offset_voices = false;
                break;
            }
        }
    }

    if (offset_voices && item->staff()->should_merge_matching_rests()) {
        // automatically merge matching rests if nothing in any other voice
        // this is not always the right thing to do do, but is useful in choral music
        // and can be enabled via a staff property
        bool match_found = false;
        track_idx_t base_track = item->staff_idx() * ::mu::engraving::voices;
        for (voice_idx_t v = 0; v < ::mu::engraving::voices; ++v) {
            if (v == item->voice()) {
                continue;
            }
            EngravingItem *e = s->element(base_track + v);
            // try to find match in any other voice
            if (e) {
                if (e->is_rest()) {
                    Rest *r = to_rest(e);
                    if (r->global_ticks() == item->global_ticks() && r->duration_type() == item->duration_type()) {
                        match_found = true;
                        ldata->merged_rests.push_back(r);
                        continue;
                    }
                }
                // no match found; no sense looking for anything else
                match_found = false;
                break;
            }
        }
        if (match_found) {
            offset_voices = false;
        }
    }

    if (!offset_voices) {
        return 0;
    }

    bool up = item->voice() == 0 || item->voice() == 2;
    int up_sign = up ? -1 : 1;
    int voice_line_offset = item->style().style_b(Sid::MULTI_VOICE_REST_TWO_SPACE_OFFSET) ? 2 : 1;

    return voice_line_offset * up_sign;
}

int RestLayout::compute_whole_or_breve_rest_offset(const Rest *item, int voice_offset, int lines) {
    int line_move = 0;
    if (item->is_whole_rest()) {
        bool move_to_line_above =
            (lines > 5) || ((lines > 1 || voice_offset == -1 || voice_offset == 2) && !(voice_offset == -2 || voice_offset == 1));
        if (move_to_line_above) {
            line_move = -1;
        }
    } else if (item->is_breve_rest() && lines == 1) {
        line_move = 1;
    }

    return line_move;
}

void RestLayout::update_symbol(const Rest *item, Rest::LayoutData *ldata) {
    Fraction t = item->tick();
    Staff *st = item->staff();
    double line_distance = st->line_distance(t) * item->spatium();
    int lines = st->lines(t);

    double y = item->pos().y();
    int line = floor(y / line_distance);

    ldata->sym = item->get_symbol(item->duration_type().type(), line, lines);
}
} // namespace mu::engraving::rendering::score
