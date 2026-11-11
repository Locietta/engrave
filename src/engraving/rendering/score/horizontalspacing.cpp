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
#include "horizontalspacing.h"

#include <cfloat>

#include "chordbracketlayout.h"
#include "dom/barline.h"
#include "dom/beam.h"
#include "dom/chord.h"
#include "dom/chordbracket.h"
#include "dom/engravingitem.h"
#include "dom/glissando.h"
#include "dom/lyrics.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/staff.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/system.h"
#include "dom/tie.h"
#include "dom/timesig.h"
#include "log.h"
#include "parenthesislayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

double HorizontalSpacing::compute_spacing_for_full_system(System *system, double stretch_reduction, double squeeze_factor,
                                                          bool override_min_measure_width) {

    if (system->score()->all_staves_invisible()) {
        return 0.0;
    }

    HorizontalSpacingContext ctx;
    ctx.system = system;
    ctx.spatium = system->spatium();

    ctx.stretch_reduction = stretch_reduction;
    ctx.squeeze_factor = squeeze_factor;
    ctx.spacing_density = system->style().style_d(Sid::SPACING_DENSITY);
    ctx.override_min_measure_width = override_min_measure_width;

    ctx.x_cur = system->left_margin();
    ctx.x_left_barrier = system->left_margin();
    ctx.system_is_full = true;

    std::vector<Measure *> measure_group;
    measure_group.reserve(system->measures().size());

    for (MeasureBase *mb : system->measures()) {
        if (mb->is_h_box()) {
            space_measure_group(measure_group, ctx);
            measure_group.clear();

            mb->mutldata()->set_pos_x(ctx.x_cur);
            mb->compute_min_width();
            ctx.x_cur += mb->width();
        } else {
            measure_group.push_back(to_measure(mb));
        }
    }

    space_measure_group(measure_group, ctx);

    return ctx.x_cur;
}

double HorizontalSpacing::update_spacing_for_last_added_measure(System *system, bool start_of_continuous_layout_region) {

    if (system->score()->all_staves_invisible()) {
        return 0.0;
    }

    HorizontalSpacingContext ctx;
    ctx.system = system;
    ctx.spatium = system->spatium();
    ctx.x_left_barrier = system->left_margin();
    ctx.spacing_density = system->style().style_d(Sid::SPACING_DENSITY);

    size_t measure_count = system->measures().size();
    IF_ASSERT_FAILED(measure_count > 0) { return 0.0; }

    MeasureBase *second_to_last = measure_count >= 2 ? system->measures()[measure_count - 2] : nullptr;
    MeasureBase *last = system->measures()[measure_count - 1];

    if (second_to_last) {
        ctx.x_cur = second_to_last->x();
        if (second_to_last->is_h_box() || last->is_h_box() || start_of_continuous_layout_region) {
            ctx.x_cur += second_to_last->width();
        }
    } else {
        ctx.x_cur = system->left_margin();
    }

    if (last->is_h_box()) {
        last->mutldata()->set_pos_x(ctx.x_cur);
        last->compute_min_width();
        ctx.x_cur += last->width();
    } else if (!second_to_last || second_to_last->is_h_box() || start_of_continuous_layout_region) {
        space_measure_group({to_measure(last)}, ctx);
    } else {
        ctx.start_meas = to_measure(last);
        space_measure_group({to_measure(second_to_last), to_measure(last)}, ctx);
    }

    return ctx.x_cur;
}

void HorizontalSpacing::squeeze_system_to_fit(System *system, double &cur_sys_width, double target_sys_width) {

    Measure *first_measure = system->first_measure();
    if (!first_measure) {
        return;
    }

    static constexpr double stretch_reduction_step = 0.33;
    static constexpr double squeeze_step = 0.2;

    double squeeze_factor = 1 - squeeze_step;              // Reduces shape paddings
    double stretch_reduction = 1 - stretch_reduction_step; // Reduces duration stretch

    while (cur_sys_width > target_sys_width && muse::real_is_equal_or_more(squeeze_factor, 0.0)) {
        cur_sys_width = HorizontalSpacing::compute_spacing_for_full_system(system, stretch_reduction, squeeze_factor, true);
        squeeze_factor -= squeeze_step;
        stretch_reduction -= stretch_reduction_step;
    }

    if (cur_sys_width < target_sys_width) {
        return;
    }

    // Things don't fit without collisions, so give up and allow collisions
    double smaller_step = 0.25 * squeeze_step;
    double width_reduction = 1 - smaller_step;
    while (cur_sys_width > target_sys_width && muse::real_is_equal_or_more(width_reduction, 0.0)) {
        for (MeasureBase *mb : system->measures()) {
            if (!mb->is_measure()) {
                continue;
            }

            Measure *m = to_measure(mb);
            double prev_width = m->width();
            for (Segment &segment : m->segments()) {
                if (!segment.is_chord_rest_type()) {
                    continue;
                }
                double cur_segment_width = segment.width();
                segment.set_width(cur_segment_width * width_reduction);
            }
            m->respace_segments();
            cur_sys_width += m->width() - prev_width;
        }
        width_reduction -= smaller_step;
    }
}

void HorizontalSpacing::justify_system(System *system, double cur_sys_width, double target_system_width) {
    double rest = target_system_width - cur_sys_width;
    if (muse::real_is_null(rest)) {
        return;
    }

    IF_ASSERT_FAILED(rest > 0) { return; }

    std::vector<Spring> springs;

    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &s : to_measure(mb)->segments()) {
            if (s.is_chord_rest_type() && s.ticks() > Fraction(0, 1) && s.visible() && s.enabled() && !s.all_elements_invisible()) {
                double spring_const = 1 / s.stretch();
                double width = s.width() - s.width_offset();
                double pre_tension = width * spring_const;
                springs.emplace_back(spring_const, width, pre_tension, &s);
            }
        }
    }

    stretch_segments_to_width(springs, rest);

    double x = system->left_margin();
    for (MeasureBase *mb : system->measures()) {
        if (mb->is_measure()) {
            to_measure(mb)->respace_segments();
        }
        mb->mutldata()->set_pos_x(x);
        x += mb->width();
    }
}

void HorizontalSpacing::space_measure_group(const std::vector<Measure *> &measure_group, HorizontalSpacingContext &ctx) {
    if (measure_group.empty()) {
        return;
    }

    measure_group.front()->mutldata()->set_pos_x(ctx.x_cur);

    std::vector<Segment *> seg_list;
    seg_list.reserve(2 * measure_group.size());

    int start_seg_idx = -1;
    for (Measure *measure : measure_group) {
        if (measure == ctx.start_meas) {
            start_seg_idx = int(seg_list.size());
        }
        for (Segment &segment : measure->segments()) {
            seg_list.push_back(&segment);
        }
    }

    IF_ASSERT_FAILED(!seg_list.empty()) { return; }

    std::vector<SegmentPosition> segment_positions = space_segments(seg_list, start_seg_idx, ctx);
    move_right_aligned_segments(segment_positions, ctx);

    set_positions_and_widths(segment_positions);

    if (!ctx.override_min_measure_width) {
        enforce_minimum_measure_widths(measure_group);
    }

    Measure *last_meas = measure_group.back();
    ctx.x_cur = last_meas->x() + last_meas->width();
}

std::vector<HorizontalSpacing::SegmentPosition> HorizontalSpacing::space_segments(const std::vector<Segment *> &seg_list, int start_seg_idx,
                                                                                  HorizontalSpacingContext &ctx) {
    std::vector<SegmentPosition> placed_segments;
    placed_segments.reserve(seg_list.size());

    for (size_t i = 0; i < seg_list.size(); ++i) {
        Segment *cur_seg = seg_list[i];
        if (ignore_segment_for_spacing(cur_seg)) {
            placed_segments.emplace_back(cur_seg, ctx.x_cur, /*ignoreForSpacing*/ true);
            continue;
        }

        if (static_cast<int>(i) < start_seg_idx) {
            placed_segments.emplace_back(cur_seg, cur_seg->x_pos_in_system_coords());
            ctx.x_cur = cur_seg->x_pos_in_system_coords() + cur_seg->width();
            continue;
        }

        cur_seg->clear_width_offset();

        if (placed_segments.empty() || ignore_all_segments_for_spacing(placed_segments)) {
            double x_first = get_first_segment_x_pos(cur_seg, ctx);
            ctx.x_cur += x_first;
            placed_segments.emplace_back(cur_seg, ctx.x_cur);
        } else {
            space_against_previous_segments(cur_seg, placed_segments, ctx);
        }

        double leading_space = cur_seg->extra_leading_space().to_absolute(ctx.spatium);
        placed_segments.back().x_pos_in_system_coords += leading_space;

        if (cur_seg->is_chord_rest_type()) {
            bool is_first_cr_of_system = cur_seg->rtick().is_zero() && cur_seg->measure()->is_first_in_system();
            if (is_first_cr_of_system) {
                double x_min_system_header_dist =
                    ctx.system->left_margin() + cur_seg->style().style_absolute(Sid::SYSTEM_HEADER_MIN_START_OF_SYSTEM_DISTANCE);
                placed_segments.back().x_pos_in_system_coords =
                    std::max(placed_segments.back().x_pos_in_system_coords, x_min_system_header_dist);
                ctx.x_cur = std::max(ctx.x_cur, x_min_system_header_dist);
            }

            double chord_rest_seg_width = chord_rest_segment_natural_width(cur_seg, ctx);

            Segment *next_seg = i < seg_list.size() - 1 ? seg_list[i + 1] : nullptr;
            if (next_seg) {
                double next_seg_leading_space = next_seg->extra_leading_space().to_absolute(ctx.spatium);
                if (!muse::real_is_null(next_seg_leading_space)) {
                    next_seg_leading_space = std::max(next_seg_leading_space, -chord_rest_seg_width);
                    cur_seg->add_width_offset(next_seg_leading_space);
                }
                if (next_seg->is_chord_rest_type()) {
                    apply_cross_beam_spacing_correction(cur_seg, next_seg, chord_rest_seg_width);
                }
            }

            ctx.x_cur += chord_rest_seg_width;
        }
    }

    if (ctx.system_is_full) {
        check_lyrics_against_right_margin(placed_segments, ctx);
        check_large_time_sig_against_right_margin(placed_segments);
    }

    return placed_segments;
}

bool HorizontalSpacing::ignore_segment_for_spacing(const Segment *segment) {
    return !segment->enabled() || !segment->is_active() || segment->all_elements_invisible();
}

bool HorizontalSpacing::ignore_all_segments_for_spacing(const std::vector<SegmentPosition> &segment_positions) {
    for (const SegmentPosition &seg_pos : segment_positions) {
        if (!seg_pos.ignore_for_spacing) {
            return false;
        }
    }

    return true;
}

void HorizontalSpacing::space_against_previous_segments(Segment *segment, std::vector<SegmentPosition> &prev_seg_positions,
                                                        HorizontalSpacingContext &ctx) {
    double x = -DBL_MAX;
    int prev_cr_segments_count = 0;

    if (segment->measure()->is_first_in_system()) {
        check_lyrics_against_left_margin(segment, x, ctx);
    }

    bool segment_has_time_sig_above_staff = segment->has_time_sig_above_staves();

    for (size_t i = prev_seg_positions.size(); i > 0; --i) {
        const SegmentPosition &prev_seg_pos = prev_seg_positions[i - 1];
        if (prev_seg_pos.ignore_for_spacing) {
            continue;
        }

        Segment *prev_seg = prev_seg_pos.segment;
        double x_prev_seg = prev_seg_pos.x_pos_in_system_coords;

        if (prev_seg->is_chord_rest_type()) {
            ++prev_cr_segments_count;
        }

        bool time_sig_above_barline_case =
            segment_has_time_sig_above_staff && prev_seg->is_end_bar_line_type() && prev_seg->tick() == segment->tick();
        bool time_sig_above_key_sig_case =
            segment_has_time_sig_above_staff && prev_seg->is_type(SegmentType::KEY_SIG) && prev_seg->tick() == segment->tick();
        bool time_sig_above_repeat_key_sig_case = segment_has_time_sig_above_staff &&
                                                  prev_seg->is_type(SegmentType::KEY_SIG_REPEAT_ANNOUNCE) &&
                                                  prev_seg->tick() == segment->tick();

        if (time_sig_above_barline_case) {
            x = x_prev_seg + prev_seg->min_right() -
                0.5 * prev_seg->style().style_absolute(Sid::BAR_WIDTH); // align to the preceding barline
        } else if (time_sig_above_key_sig_case) {
            x = x_prev_seg + segment->min_left(); // align to the preceding keySig
        } else if (time_sig_above_repeat_key_sig_case) {
            x = x_prev_seg + prev_seg->min_right();
        } else {
            double min_hor_dist = min_horizontal_distance(prev_seg, segment, ctx.squeeze_factor);
            min_hor_dist = std::max(min_hor_dist, space_lyrics_against_barlines(prev_seg, segment, ctx));
            double x_non_collision = x_prev_seg + min_hor_dist;
            x = std::max(x, x_non_collision);
        }

        if (x > ctx.x_cur || time_sig_above_barline_case) {
            if (prev_cr_segments_count > 0) {
                double space_increase = (x - ctx.x_cur) / prev_cr_segments_count;
                double x_movement = space_increase;
                for (size_t j = i; j < prev_seg_positions.size(); ++j) {
                    SegmentPosition &seg_pos = prev_seg_positions[j];
                    if (seg_pos.ignore_for_spacing) {
                        continue;
                    }
                    seg_pos.x_pos_in_system_coords += x_movement;
                    if (seg_pos.segment->is_chord_rest_type()) {
                        x_movement += space_increase;
                    }
                }
            }

            ctx.x_cur = x;
        }

        if (stop_checking_previous_segments(prev_seg_pos, SegmentPosition(segment, ctx.x_cur))) {
            break;
        }
    }

    double x_seg = segment->is_right_aligned() ? x : ctx.x_cur;

    prev_seg_positions.emplace_back(segment, x_seg);
}

bool HorizontalSpacing::stop_checking_previous_segments(const SegmentPosition &prev_seg_pos, const SegmentPosition &cur_seg_pos) {
    Segment *prev_seg = prev_seg_pos.segment;
    Segment *cur_seg = cur_seg_pos.segment;

    Fraction prev_seg_end_tick = prev_seg->tick() + prev_seg->ticks();
    Fraction cur_seg_tick = cur_seg->tick();

    if (prev_seg_end_tick >= cur_seg_tick || cur_seg->has_time_sig_above_staves()) {
        return false;
    }

    if (!cur_seg->is_chord_rest_type()) {
        return true;
    }

    bool cur_has_lyrics_or_harmony = false;
    double cur_x = cur_seg_pos.x_pos_in_system_coords;
    double cur_min_left = DBL_MAX;
    for (const Shape &shape : cur_seg->shapes()) {
        for (const ShapeElement &shape_el : shape.elements()) {
            cur_min_left = std::min(cur_min_left, shape_el.left());
            if (const EngravingItem *item = shape_el.item()) {
                if (item->is_lyrics() || item->is_harmony()) {
                    cur_has_lyrics_or_harmony = true;
                }
            }
        }
    }
    cur_min_left += cur_x;

    bool prev_has_lyrics_or_harmony = false;
    double prev_x = prev_seg_pos.x_pos_in_system_coords;
    double prev_min_right = -DBL_MAX;
    for (const Shape &shape : prev_seg->shapes()) {
        for (const ShapeElement &shape_el : shape.elements()) {
            prev_min_right = std::max(prev_min_right, shape_el.right());
            if (const EngravingItem *item = shape_el.item()) {
                if (item->is_lyrics() || item->is_harmony()) {
                    prev_has_lyrics_or_harmony = true;
                }
            }
        }
    }
    prev_min_right += prev_x;

    if (cur_has_lyrics_or_harmony == prev_has_lyrics_or_harmony && cur_min_left > prev_min_right) {
        return true;
    }

    if (prev_seg->measure() != cur_seg->measure() && prev_seg->measure() != cur_seg->measure()->prev_mm()) {
        return true;
    }

    return false;
}

void HorizontalSpacing::check_lyrics_against_left_margin(Segment *segment, double &x, HorizontalSpacingContext &ctx) {
    for (const Shape &staff_shape : segment->shapes()) {
        for (const ShapeElement &shape_el : staff_shape.elements()) {
            if (shape_el.item() && shape_el.item()->is_lyrics()) {
                x = std::max(x, ctx.x_left_barrier - shape_el.left());
            }
        }
    }
}

void HorizontalSpacing::check_lyrics_against_right_margin(std::vector<SegmentPosition> &seg_positions,
                                                          const HorizontalSpacingContext &ctx) {
    const double system_edge = seg_positions.back().x_pos_in_system_coords + seg_positions.back().segment->min_right();
    const MStyle &style = ctx.system->style();
    const bool lyrics_dash_force = style.style_b(Sid::LYRICS_DASH_FORCE);
    const bool lyrics_melisma_force = style.style_b(Sid::LYRICS_MELISMA_FORCE);
    const double min_space_for_dash =
        lyrics_dash_force ? style.style_absolute(Sid::LYRICS_DASH_PAD) + style.style_absolute(Sid::LYRICS_DASH_MIN_LENGTH) +
                                style.style_absolute(Sid::LINE_END_TO_BARLINE_DISTANCE) + seg_positions.back().segment->min_right() :
                            0.0;
    const double min_space_for_melisma =
        lyrics_melisma_force ? style.style_absolute(Sid::LYRICS_MELISMA_PAD) + style.style_absolute(Sid::LYRICS_MELISMA_MIN_LENGTH) +
                                   style.style_absolute(Sid::LINE_END_TO_BARLINE_DISTANCE) + seg_positions.back().segment->min_right() :
                               0.0;

    int chord_rest_segments_count = 0;

    for (size_t i = seg_positions.size(); i > 1; --i) {
        SegmentPosition &seg_pos = seg_positions[i - 1];
        double x = seg_pos.x_pos_in_system_coords;
        Segment *seg = seg_pos.segment;
        if (!seg->measure()->is_last_in_system()) {
            break;
        }
        if (seg->is_chord_rest_type()) {
            ++chord_rest_segments_count;
        } else {
            continue;
        }
        double x_max_lyrics = x;
        for (const Shape &shape : seg->shapes()) {
            for (const ShapeElement &shape_el : shape.elements()) {
                if (shape_el.item() && shape_el.item()->is_lyrics()) {
                    const Lyrics *lyrics = to_lyrics(shape_el.item());
                    bool has_melisma = lyrics->separator() && lyrics->separator()->is_end_melisma();
                    bool has_dash = lyrics->syllabic() == LyricsSyllabic::BEGIN || lyrics->syllabic() == LyricsSyllabic::MIDDLE;
                    double right_edge = x + shape_el.right();
                    if (has_dash) {
                        right_edge += min_space_for_dash;
                    } else if (has_melisma) {
                        right_edge += min_space_for_melisma;
                    }
                    x_max_lyrics = std::max(x_max_lyrics, right_edge);
                }
            }
        }
        if (x_max_lyrics > system_edge) {
            double lyrics_overshoot = x_max_lyrics - system_edge;
            double space_increase = lyrics_overshoot / chord_rest_segments_count;
            double x_movement = space_increase;
            for (size_t j = i; j < seg_positions.size(); ++j) {
                SegmentPosition &sp = seg_positions[j];
                sp.x_pos_in_system_coords += x_movement;
                if (sp.segment->is_chord_rest_type()) {
                    x_movement += space_increase;
                }
            }
        }
    }
}

double HorizontalSpacing::space_lyrics_against_barlines(Segment *first_seg, Segment *second_seg, const HorizontalSpacingContext &ctx) {
    if (!(first_seg->is_type(SegmentType::CHORD_REST) && second_seg->is_type(SegmentType::BAR_LINE_TYPES)) &&
        !(first_seg->is_type(SegmentType::BAR_LINE_TYPES) && second_seg->is_type(SegmentType::CHORD_REST))) {
        return 0.0;
    }

    double w = 0.0;

    bool cr_seg_is_before = first_seg->is_chord_rest_type();
    Segment *cr_segment = cr_seg_is_before ? first_seg : second_seg;
    Segment *barline_segment = cr_seg_is_before ? second_seg : first_seg;

    staff_idx_t nstaves = cr_segment->score()->nstaves();

    for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
        if (!ctx.system->staff(staff_idx)->show()) {
            continue;
        }

        staff_idx_t next_staff = ctx.system->next_visible_staff(staff_idx);
        if (next_staff == muse::nidx) {
            continue;
        }

        BarLine *barline = to_bar_line(barline_segment->element(staff2track(staff_idx)));
        if (!barline || !barline->span_staff()) {
            continue;
        }

        Shape barline_shape = barline->shape().translate(barline->pos());

        track_idx_t start_track = staff2track(staff_idx);
        track_idx_t end_track = staff2track(next_staff) + ::mu::engraving::voices;
        for (track_idx_t track = start_track; track < end_track; ++track) {
            ChordRest *chord_rest = to_chord_rest(cr_segment->element(track));
            if (!chord_rest) {
                continue;
            }

            for (Lyrics *lyrics : chord_rest->lyrics()) {
                if (!(lyrics->avoid_barlines() && lyrics->visible() && lyrics->autoplace())) {
                    continue;
                }
                staff_idx_t lyrics_staff_idx = lyrics->staff_idx();
                if ((lyrics_staff_idx == staff_idx && lyrics->place_below()) || (lyrics_staff_idx == next_staff && lyrics->place_above())) {
                    Shape lyrics_shape = lyrics->shape().translate(lyrics->pos());
                    double min_dist =
                        cr_seg_is_before ? lyrics_shape.right() - barline_shape.left() : barline_shape.right() - lyrics_shape.left();
                    const double padding = 0.3 * lyrics->font_metrics().x_height();
                    min_dist += padding;
                    w = std::max(w, min_dist);
                }
            }
        }
    }

    return w;
}

void HorizontalSpacing::check_large_time_sig_against_right_margin(std::vector<SegmentPosition> &seg_positions) {
    SegmentPosition &cautionary_ts_seg_pos = seg_positions.back();
    if (!cautionary_ts_seg_pos.segment->has_time_sig_above_staves()) {
        return;
    }

    const MStyle &style = cautionary_ts_seg_pos.segment->style();
    TimeSigVSMargin time_sig_vs_margin = style.style_b(Sid::TIME_SIG_CENTER_ON_BARLINE) ?
                                             style.style_v(Sid::TIME_SIG_VS_MARGIN_CENTERED).value<TimeSigVSMargin>() :
                                             style.style_v(Sid::TIME_SIG_VS_MARGIN_NON_CENTERED).value<TimeSigVSMargin>();
    if (time_sig_vs_margin != TimeSigVSMargin::RIGHT_ALIGN_TO_BARLINE) {
        return;
    }

    double x_max = 0.0;
    for (auto iter = seg_positions.rbegin(); iter != seg_positions.rend(); ++iter) {
        if (iter->segment->is_type(SegmentType::END_BAR_LINE | SegmentType::KEY_SIG_ANNOUNCE)) {
            x_max = std::max(x_max, iter->x_pos_in_system_coords + iter->segment->min_right());
        }
    }

    if (muse::real_is_null(x_max)) {
        return;
    }

    double cur_ts_pos = cautionary_ts_seg_pos.x_pos_in_system_coords;
    double x_max_pos = x_max - cautionary_ts_seg_pos.segment->min_right();
    if (x_max_pos < cur_ts_pos) {
        cautionary_ts_seg_pos.x_pos_in_system_coords = x_max_pos;
    }
}

void HorizontalSpacing::move_right_aligned_segments(std::vector<SegmentPosition> &placed_segments, const HorizontalSpacingContext &ctx) {
    for (size_t i = 0; i < placed_segments.size(); ++i) {
        Segment *segment = placed_segments[i].segment;
        if (!segment->is_right_aligned()) {
            continue;
        }

        double x = DBL_MAX;

        for (size_t j = i + 1; j < placed_segments.size(); ++j) {
            SegmentPosition &seg_pos = placed_segments[j];
            if (seg_pos.ignore_for_spacing) {
                continue;
            }

            Segment *following_seg = seg_pos.segment;
            if (following_seg->has_time_sig_above_staves()) {
                continue;
            }
            if (following_seg->measure() != segment->measure() && following_seg->rtick().is_not_zero()) {
                break;
            }
            double following_seg_x = seg_pos.x_pos_in_system_coords;
            double min_dist = min_horizontal_distance(segment, following_seg, ctx.squeeze_factor);

            if (following_seg->is_right_aligned() && muse::real_is_null(min_dist)) {
                continue;
            }

            x = std::min(x, following_seg_x - min_dist);
        }

        if (x != DBL_MAX) {
            placed_segments[i].x_pos_in_system_coords = x;
            SegmentPosition &next_seg_pos = placed_segments[i + 1];
            for (size_t j = i + 2; next_seg_pos.ignore_for_spacing && j < placed_segments.size(); ++j) {
                next_seg_pos = placed_segments[j];
            }
            double next_seg_x = next_seg_pos.x_pos_in_system_coords;
            Segment *prev_cr_seg = segment->prev(SegmentType::CHORD_REST);
            if (prev_cr_seg) {
                prev_cr_seg->add_width_offset(-next_seg_x + x);
            }
        }
    }
}

void HorizontalSpacing::check_collisions_with_cross_staff_stems(const Segment *this_seg, const Segment *next_seg, staff_idx_t staff_idx,
                                                                double &cur_min_dist) {
    std::vector<ChordRest *> items_to_check;
    items_to_check.reserve(::mu::engraving::voices);

    for (EngravingItem *el : next_seg->elist()) {
        if (el && el->v_staff_idx() == staff_idx) {
            ChordRest *cr = to_chord_rest(el);
            if (cr->beam() && cr->beam()->cross()) {
                items_to_check.push_back(to_chord_rest(el));
            }
        }
    }

    for (ChordRest *cr : items_to_check) {
        Chord *potential_colliding_chord = nullptr;
        for (ChordRest *beam_element : cr->beam()->elements()) {
            if (beam_element->is_chord() && beam_element->parent() == this_seg) {
                potential_colliding_chord = to_chord(beam_element);
                break;
            }
        }

        if (!potential_colliding_chord || potential_colliding_chord->up() != cr->up() || !potential_colliding_chord->stem()) {
            return;
        }

        bool check_stem_collision = (cr->up() && potential_colliding_chord->v_staff_idx() > cr->v_staff_idx()) ||
                                    (!cr->up() && potential_colliding_chord->v_staff_idx() < cr->v_staff_idx());
        if (!check_stem_collision) {
            return;
        }

        Stem *stem = potential_colliding_chord->stem();
        Shape prev_stem_shape = stem->shape().translated(stem->pos() + potential_colliding_chord->pos());
        // We don't know where this stem is vertically, but we know from the beam arrangement that
        // it's going to be crossed anyway, so we can space to it as if it had infinite height.
        prev_stem_shape.adjust(0.0, -100000, 0.0, 100000);

        const Shape &staff_shape = next_seg->staff_shape(staff_idx);

        double min_dist = min_horizontal_distance(prev_stem_shape, staff_shape, stem->spatium());

        cur_min_dist = std::max(cur_min_dist, min_dist);
    }
}

double HorizontalSpacing::chord_rest_segment_natural_width(Segment *segment, HorizontalSpacingContext &ctx) {
    double duration_stretch = compute_segment_duration_stretch(segment, segment->prev(SegmentType::CHORD_REST));

    Measure *measure = segment->measure();
    double user_stretch = std::clamp(measure->user_stretch(), 0.1, 10.0); // TODO: enforce via UI, not here

    double seg_total_stretch = duration_stretch * user_stretch;
    segment->set_stretch(seg_total_stretch);

    static constexpr Spatium default_quarter_note_space_spatium = 3.5_sp;
    double default_quarter_note_space_value = default_quarter_note_space_spatium.to_absolute(measure->spatium());

    double natural_width = default_quarter_note_space_value * seg_total_stretch * ctx.stretch_reduction / ctx.spacing_density;

    return natural_width;
}

double HorizontalSpacing::compute_segment_duration_stretch(const Segment *cur_seg, const Segment *prev_seg) {
    if (cur_seg->is_mm_rest_segment()) {
        return duration_stretch_for_mm_rests(cur_seg);
    }

    Fraction seg_ticks = cur_seg->ticks();
    Fraction shortest_cr = cur_seg->shortest_chord_rest();
    Fraction prev_shortest_cr = prev_seg ? prev_seg->shortest_chord_rest() : Fraction(0, 1);

    bool has_adjacent = cur_seg->is_chord_rest_type() && shortest_cr == seg_ticks;
    bool prev_has_adjacent = prev_seg && (prev_seg->is_chord_rest_type() && prev_shortest_cr == prev_seg->ticks());

    double dur_stretch;
    const MStyle &style = cur_seg->style();
    double slope = style.style_d(Sid::MEASURE_SPACING);

    if (has_adjacent || cur_seg->measure()->is_mm_rest()) {
        dur_stretch = duration_stretch_for_ticks(slope, seg_ticks);
    } else {
        // Polyrythms
        if (prev_seg && !prev_has_adjacent && prev_shortest_cr < shortest_cr) {
            dur_stretch = duration_stretch_for_ticks(slope, prev_shortest_cr) * (seg_ticks / prev_shortest_cr).to_double();
        } else {
            dur_stretch = duration_stretch_for_ticks(slope, shortest_cr) * (seg_ticks / shortest_cr).to_double();
        }
    }

    if (style.style_b(Sid::SCALE_RYTHMIC_SPACING_FOR_SMALL_NOTES) && needs_cue_size_spacing(cur_seg)) {
        dur_stretch *= cur_seg->style().style_d(Sid::SMALL_NOTE_MAG);
    }

    return dur_stretch;
}

double HorizontalSpacing::duration_stretch_for_mm_rests(const Segment *segment) {
    static constexpr Fraction quarter = Fraction(1, 4);
    static constexpr int min_mmrest_count = 2;

    const MStyle &style = segment->style();
    const Measure *measure = segment->measure();

    Fraction time_sig = measure->timesig();
    bool constant_width = style.style_b(Sid::MM_REST_CONSTANT_WIDTH);
    int mm_rest_width_increment_cap = style.style_i(Sid::MM_REST_MAX_WIDTH_INCREASE);

    Fraction base_duration = constant_width ? style.style_i(Sid::MM_REST_REFERENCE_WIDTH) * quarter : time_sig;
    Fraction duration_increment = constant_width ? quarter : Fraction(1, time_sig.denominator());
    int increment_count = std::max(measure->mm_rest_count() - min_mmrest_count, 0);
    increment_count = std::min(increment_count, mm_rest_width_increment_cap);

    Fraction result_duration = base_duration + increment_count * duration_increment;

    return duration_stretch_for_ticks(style.style_d(Sid::MEASURE_SPACING), result_duration);
}

double HorizontalSpacing::duration_stretch_for_ticks(double slope, const Fraction &ticks) {
    static constexpr Fraction reference_duration = Fraction(1, 4);

    Fraction duration_ratio = ticks / reference_duration;

    return pow(slope, log2(duration_ratio.to_double()));
}

bool HorizontalSpacing::needs_cue_size_spacing(const Segment *segment) {
    IF_ASSERT_FAILED(segment->is_chord_rest_type()) { return false; }

    bool has_cue_sized_cr = false;
    for (EngravingItem *item : segment->elist()) {
        if (!item) {
            continue;
        }
        const ChordRest *cr = to_chord_rest(item);
        if (cr->is_small()) {
            has_cue_sized_cr = true;
        } else if (cr->actual_ticks() == segment->ticks()) {
            return false;
        }
    }

    return has_cue_sized_cr;
}

void HorizontalSpacing::apply_cross_beam_spacing_correction(Segment *this_seg, Segment *next_seg, double &width) {
    CrossBeamSpacing cross_beam_spacing = compute_cross_beam_spacing(this_seg, next_seg);

    const Score *score = this_seg->score();
    const PaddingTable &padding_table = score->padding_tables().horizontal_padding_table();
    const MStyle &style = score->style();

    double displacement = score->note_head_width() - style.style_absolute(Sid::STEM_WIDTH);

    if (cross_beam_spacing.up_down && cross_beam_spacing.can_be_adjusted) {
        this_seg->add_width_offset(displacement);
        width += displacement;
    } else if (cross_beam_spacing.down_up && cross_beam_spacing.can_be_adjusted) {
        this_seg->add_width_offset(-displacement);
        width -= displacement;
    }

    if (cross_beam_spacing.up_down) {
        if (cross_beam_spacing.has_opposing_beamlets) {
            double min_beamlet_clearance =
                style.style_absolute(Sid::BEAM_MIN_LEN) * 2.0 + padding_table.at(ElementType::BEAM).at(ElementType::BEAM);
            width = std::max(width, displacement + min_beamlet_clearance);
        } else {
            width = std::max(width, 2 * displacement);
        }
    }

    if (cross_beam_spacing.prevent_cross_staff_kerning) {
        double padding = cross_beam_spacing.ensure_min_stem_distance ? padding_table.at(ElementType::STEM).at(ElementType::STEM) :
                                                                       style.style_absolute(Sid::MIN_NOTE_DISTANCE);
        width = std::max(width, score->note_head_width() + padding);
    } else if (cross_beam_spacing.ensure_min_stem_distance) {
        width = std::max(width, padding_table.at(ElementType::STEM).at(ElementType::STEM));
    }
}

double HorizontalSpacing::min_stem_dist_on_non_adjacent_cross(const Segment *this_seg, const Segment *next_seg) {
    // Extreme edge-case of chords that are a) adjacent in a cross-staff beam but b) on non-adjacent segments
    // (which causes them to escape the previous cross-staff spacing checks) c) stemmed up->down (see #27786)
    const PaddingTable &padding_table = this_seg->score()->padding_tables().horizontal_padding_table();
    double dist = -DBL_MAX;
    for (EngravingItem *item : next_seg->elist()) {
        if (!item || !item->is_chord()) {
            continue;
        }

        Chord *chord = to_chord(item);
        if (!chord->stem() || chord->up() || !(chord->beam() && chord->beam()->cross())) {
            continue;
        }

        Beam *beam = chord->beam();
        std::vector<ChordRest *> beam_elements = beam->elements();
        Chord *prev_chord_on_beam = nullptr;
        for (size_t i = 0; i < beam_elements.size(); ++i) {
            if (beam_elements[i] == chord) {
                if (i > 0) {
                    ChordRest *prev_chord_rest = beam_elements[i - 1];
                    prev_chord_on_beam = prev_chord_rest->is_chord() ? to_chord(prev_chord_rest) : nullptr;
                }
                break;
            }
        }

        if (!prev_chord_on_beam || prev_chord_on_beam->parent() != this_seg || !prev_chord_on_beam->up() || !prev_chord_on_beam->stem()) {
            continue;
        }

        double min_stem_dist = prev_chord_on_beam->x() + prev_chord_on_beam->stem()->x() - (chord->x() + chord->stem()->x());
        min_stem_dist += padding_table.at(ElementType::STEM).at(ElementType::STEM);

        dist = std::max(dist, min_stem_dist);
    }

    return dist;
}

HorizontalSpacing::CrossBeamSpacing HorizontalSpacing::compute_cross_beam_spacing(Segment *this_seg, Segment *next_seg) {
    CrossBeamSpacing cross_beam_type;

    if (!this_seg->is_chord_rest_type() || !next_seg || !next_seg->is_chord_rest_type()) {
        return cross_beam_type;
    }

    bool prevent_cross_staff_kerning = false;
    bool ensure_min_stem_distance = false;
    for (EngravingItem *e : this_seg->elist()) {
        if (!e || !e->is_chord() || !e->visible() || !e->staff()->visible()) {
            continue;
        }

        Chord *this_chord = to_chord(e);
        ChordRest *next_cr = to_chord_rest(next_seg->element(this_chord->track()));
        Chord *next_chord = next_cr && next_cr->is_chord() ? to_chord(next_cr) : nullptr;
        if (!next_chord) {
            continue;
        }

        int this_staff_move = this_chord->staff_move();
        int next_staff_move = next_chord->staff_move();
        if (this_staff_move == next_staff_move) {
            continue;
        }

        prevent_cross_staff_kerning = this_staff_move > next_staff_move;
        ensure_min_stem_distance = (this_staff_move > next_staff_move && this_chord->up() && !next_chord->up()) ||
                                   (this_staff_move < next_staff_move && this_chord->up() == next_chord->up());
    }

    cross_beam_type.prevent_cross_staff_kerning = prevent_cross_staff_kerning;
    cross_beam_type.ensure_min_stem_distance = ensure_min_stem_distance;

    bool up_down = false;
    bool down_up = false;
    bool can_be_adjusted = true;
    bool has_opposing_beamlets = false;

    // Spacing can be adjusted for cross-beam cases only if there aren't
    // chords in other voices in this or next segment.
    for (EngravingItem *e : this_seg->elist()) {
        if (!e || !e->is_chord_rest() || !e->staff()->visible()) {
            continue;
        }
        ChordRest *this_cr = to_chord_rest(e);
        if (!this_cr->visible() || this_cr->is_full_measure_rest() || (this_cr->is_rest() && to_rest(this_cr)->is_gap())) {
            continue;
        }
        Beam *beam = this_cr->beam();
        if (!beam && this_cr->actual_ticks() <= this_seg->ticks()) {
            can_be_adjusted = false;
        }
        if (!beam || (beam->elements().size() == 2 && this_cr->up())) {
            continue;
        }
        for (EngravingItem *ee : next_seg->elist()) {
            if (!ee || !ee->is_chord_rest() || !ee->staff()->visible()) {
                continue;
            }
            ChordRest *next_cr = to_chord_rest(ee);
            if (!next_cr->visible() || next_cr->is_full_measure_rest() || (this_cr->is_rest() && to_rest(this_cr)->is_gap())) {
                continue;
            }
            if (!next_cr->beam()) {
                can_be_adjusted = false;
                continue;
            }
            if (next_cr->beam() != beam) {
                continue;
            }
            if (this_cr->up() == next_cr->up()) {
                return cross_beam_type;
            }
            if (this_cr->up() && !next_cr->up()) {
                if (this_cr->beamlet() && next_cr->beamlet()) {
                    has_opposing_beamlets = true;
                }
                up_down = true;
            }
            if (!this_cr->up() && next_cr->up()) {
                down_up = true;
            }
            if (up_down && down_up) {
                return cross_beam_type;
            }
        }
    }

    cross_beam_type.up_down = up_down;
    cross_beam_type.down_up = down_up;
    cross_beam_type.can_be_adjusted = can_be_adjusted;
    cross_beam_type.has_opposing_beamlets = has_opposing_beamlets;

    return cross_beam_type;
}

double HorizontalSpacing::compute_min_measure_width(Measure *m) {
    const MStyle &style = m->style();
    double min_width = style.style_absolute(Sid::MIN_MEASURE_WIDTH);
    double max_sys_width = m->system()->width() - m->system()->left_margin();

    if (max_sys_width <= 0) {
        max_sys_width = min_width;
    }
    min_width = std::min(min_width, max_sys_width);

    if (m->ticks() < m->timesig()) {
        min_width *= (m->ticks() / m->timesig()).to_double();
    }

    Segment *first_cr_segment = m->find_first_r(SegmentType::CHORD_REST, Fraction(0, 1));
    if (!first_cr_segment) {
        return min_width;
    }
    if (first_cr_segment == m->first_enabled()) {
        return min_width;
    }

    double start_position = first_cr_segment->x() - first_cr_segment->min_left();
    if (first_cr_segment->has_accidentals()) {
        start_position -= style.style_absolute(Sid::BAR_ACCIDENTAL_DISTANCE);
    } else {
        start_position -= style.style_absolute(Sid::BAR_NOTE_DISTANCE);
    }

    min_width += start_position;
    min_width = std::min(min_width, max_sys_width);

    return min_width;
}

void HorizontalSpacing::enforce_minimum_measure_widths(const std::vector<Measure *> measure_group) {
    for (size_t i = 0; i < measure_group.size(); ++i) {
        Measure *measure = measure_group[i];
        if (measure->is_mm_rest()) {
            continue; // minimum mmRest width is already enforced during spacing
        }
        double min_width = compute_min_measure_width(measure);
        double diff = min_width - measure->width();
        if (diff > 0) {
            stretch_measure_to_target_width(measure, min_width);
            for (size_t j = i + 1; j < measure_group.size(); ++j) {
                measure_group[j]->mutldata()->move_x(diff);
            }
        }
    }
}

void HorizontalSpacing::stretch_measure_to_target_width(Measure *m, double target_width) {
    IF_ASSERT_FAILED(target_width > m->width()) { return; }

    std::vector<Spring> springs;
    for (Segment &s : m->segments()) {
        if (s.is_chord_rest_type() && s.visible() && s.enabled() && !s.all_elements_invisible()) {
            double spring_const = 1 / s.stretch();
            double width = s.width() - s.width_offset();
            double pre_tension = width * spring_const;
            springs.emplace_back(spring_const, width, pre_tension, &s);
        }
    }

    stretch_segments_to_width(springs, target_width - m->width());

    m->respace_segments();
}

void HorizontalSpacing::stretch_segments_to_width(std::vector<Spring> &springs, double width) {
    if (springs.empty() || muse::real_is_equal_or_less(width, 0.0)) {
        return;
    }

    std::sort(springs.begin(), springs.end(), [](const Spring &a, const Spring &b) { return a.pre_tension < b.pre_tension; });
    double inverse_spring_const = 0.0;
    double force = 0.0;

    //! NOTE springs.cbegin() != springs.cend() because of the emptiness check at the top
    auto spring = springs.cbegin();
    do {
        inverse_spring_const += 1 / spring->spring_const;
        width += spring->width;
        force = width / inverse_spring_const;
        ++spring;
    } while (spring != springs.cend() && !(force < spring->pre_tension));

    for (const Spring &spring2 : springs) {
        if (force > spring2.pre_tension) {
            double new_width = force / spring2.spring_const;
            spring2.segment->set_width(new_width + spring2.segment->width_offset());
        }
    }
}

void HorizontalSpacing::set_positions_and_widths(const std::vector<SegmentPosition> &segment_positions) {
    size_t segments_size = segment_positions.size();
    for (size_t i = 0; i < segments_size; ++i) {
        const SegmentPosition &seg_pos = segment_positions[i];
        Segment *cur_seg = seg_pos.segment;
        double cur_x = seg_pos.x_pos_in_system_coords;

        if (seg_pos.ignore_for_spacing) {
            cur_seg->set_width(0.0);
            continue;
        }

        Segment *next_seg = cur_seg;
        double next_x = cur_x;
        for (size_t j = i + 1; j < segments_size; ++j) {
            const SegmentPosition &next_seg_pos = segment_positions[j];
            if (!next_seg_pos.ignore_for_spacing) {
                next_seg = next_seg_pos.segment;
                next_x = next_seg_pos.x_pos_in_system_coords;
                break;
            }
        }

        Measure *cur_seg_measure = cur_seg->measure();
        Measure *next_seg_measure = next_seg->measure();

        bool leading_non_cr_exception = ((next_seg->is_key_sig_type() || next_seg->is_time_sig_type() || next_seg->is_clef_type()) &&
                                         !cur_seg->is_type(SegmentType::BAR_LINE_TYPES));
        bool compute_width_by_difference_from_next =
            cur_seg_measure == next_seg_measure || next_seg->is_start_repeat_bar_line_type() || leading_non_cr_exception;

        double seg_width = compute_width_by_difference_from_next ? next_x - cur_x : cur_seg->min_right();

        cur_seg->set_width(seg_width);

        if (cur_seg_measure != next_seg_measure) {
            cur_seg_measure->set_width(cur_x + seg_width - cur_seg_measure->x());
            next_seg_measure->mutldata()->set_pos_x(cur_x + seg_width);
        }

        cur_seg->set_x_pos_in_system_coords(cur_x);

        if (next_seg == cur_seg) {
            double next_seg_width = 0.0;
            if (next_seg->has_time_sig_above_staves()) {
                assert(i > 0);
                if (next_seg->make_space_for_time_sig_above_staves()) {
                    next_seg_width = next_seg->min_right();
                }
                Segment *prev_seg = segment_positions[i - 1].segment;
                double x_diff = segment_positions[i - 1].x_pos_in_system_coords - cur_x;
                double min_space_for_prev_seg = x_diff + prev_seg->min_right();
                if (prev_seg->trailer()) {
                    min_space_for_prev_seg += prev_seg->style().style_absolute(Sid::SYSTEM_TRAILER_RIGHT_MARGIN);
                }
                next_seg_width = std::max(next_seg_width, min_space_for_prev_seg);
            } else if (next_seg->trailer()) {
                next_seg_width = next_seg->min_right() + next_seg->style().style_absolute(Sid::SYSTEM_TRAILER_RIGHT_MARGIN);
            } else {
                next_seg_width = next_seg->min_right();
            }
            next_seg->set_width(next_seg_width);
            next_seg_measure->set_width(next_x + next_seg_width - next_seg_measure->x());
            next_seg->set_x_pos_in_system_coords(next_x);
        }
    }
}

double HorizontalSpacing::get_first_segment_x_pos(Segment *segment, HorizontalSpacingContext &ctx) {
    const MStyle &style = segment->style();

    double x = 0.0;
    switch (segment->segment_type()) {
        case SegmentType::BEGIN_BAR_LINE: return 0.0;
        case SegmentType::CHORD_REST: {
            Shape left_barrier(RectF(0.0, -0.5 * DBL_MAX, 0.0, DBL_MAX));
            x = min_left(segment, left_barrier);
            x += style.style_absolute(segment->has_accidentals() ? Sid::BAR_ACCIDENTAL_DISTANCE : Sid::BAR_NOTE_DISTANCE);
            x = std::max(x, style.style_absolute(Sid::SYSTEM_HEADER_MIN_START_OF_SYSTEM_DISTANCE));
            break;
        }
        case SegmentType::CLEF:
        case SegmentType::HEADER_CLEF: x = style.style_absolute(Sid::CLEF_LEFT_MARGIN); break;
        case SegmentType::KEY_SIG: x = style.style_absolute(Sid::KEYSIG_LEFT_MARGIN); break;
        case SegmentType::TIME_SIG: x = style.style_absolute(Sid::TIMESIG_LEFT_MARGIN); break;
        default: x = 0.0;
    }

    return x * ctx.squeeze_factor;
}

double HorizontalSpacing::min_horizontal_distance(const Shape &f, const Shape &s, double spatium, double squeeze_factor) {
    double dist = -DBL_MAX; // min real
    double absolute_min_padding = 0.1 * spatium * squeeze_factor;
    for (const ShapeElement &r2 : s.elements()) {
        if (r2.is_null()) {
            continue;
        }

        const EngravingItem *item2 = r2.item();
        double by1 = r2.top();
        double by2 = r2.bottom();
        for (const ShapeElement &r1 : f.elements()) {
            if (r1.is_null()) {
                continue;
            }

            const EngravingItem *item1 = r1.item();

            double ay1 = r1.top();
            double ay2 = r1.bottom();
            double vertical_clearance = compute_vertical_clearance(item1, item2, spatium) * squeeze_factor;
            bool intersection = mu::engraving::intersects(ay1, ay2, by1, by2, vertical_clearance);
            double padding = 0;
            KerningType kerning_type = KerningType::NON_KERNING;
            if (item1 && item2) {
                padding = compute_padding(item1, item2);
                padding *= squeeze_factor;
                padding = std::max(padding, absolute_min_padding);
                kerning_type = compute_kerning(item1, item2);
            }

            if (kerning_type == KerningType::ALLOW_COLLISION) {
                continue;
            }

            if (kerning_type == KerningType::NON_KERNING || intersection ||
                (r1.width() == 0 || r2.width() == 0) // Temporary hack: shapes of zero-width are assumed to collide with everyghin
                || (!item1 && item2 && item2->is_lyrics())) {
                dist = std::max(dist, r1.right() - r2.left() + padding);
                continue;
            }

            switch (kerning_type) {
                case KerningType::KERN_UNTIL_LEFT_EDGE: dist = std::max(dist, r1.left() - r2.left()); break;
                case KerningType::KERN_UNTIL_CENTER: dist = std::max(dist, r1.left() + 0.5 * r1.width() - r2.left()); break;
                case KerningType::KERN_UNTIL_RIGHT_EDGE: dist = std::max(dist, r1.right() - r2.left()); break;
                default: break;
            }
        }
    }
    return dist;
}

// Logic moved from Shape
double HorizontalSpacing::shape_spatium(const Shape &s) {
    for (auto it = s.elements().begin(); it != s.elements().end(); ++it) {
        if (it->item()) {
            return it->item()->spatium();
        }
    }
    return 0.0;
}

//---------------------------------------------------------
//   minHorizontalDistance
//    calculate the minimum layout distance to Segment ns
//---------------------------------------------------------

double HorizontalSpacing::min_horizontal_distance(const Segment *f, const Segment *ns, double squeeze_factor) {
    if (f->segment_type() & SegmentType::BAR_LINE_TYPES) {
        if (ns->is_start_repeat_bar_line_type()) {
            if (f->is_begin_bar_line_type()) {
                return 0.0;
            }
            double prev_barline_width = f->min_right();
            double thick_barline_width = f->style().style_absolute(Sid::END_BAR_WIDTH);
            return std::max(prev_barline_width - thick_barline_width, 0.0);
        }
        if (ns->is_mm_rest_segment()) {
            return f->min_right() + f->style().style_absolute(Sid::BAR_NOTE_DISTANCE);
        }
    }

    if (f->is_header_clef_type() && ns->has_time_sig_above_staves()) {
        Segment *possible_key_sig = f->next_active();
        if (possible_key_sig && possible_key_sig->is_key_sig_type() && !ignore_segment_for_spacing(possible_key_sig)) {
            return 0.0;
        }
        return f->min_right() + ns->min_left() + f->style().style_absolute(Sid::HEADER_TO_LINE_START_DISTANCE);
    }

    bool system_header_gap = needs_header_spacing_exceptions(f, ns);

    double ww = -DBL_MAX; // can remain negative
    double d = 0.0;
    Score *score = f->score();
    for (unsigned staff_idx = 0; staff_idx < f->shapes().size(); ++staff_idx) {
        if (score->staff(staff_idx) && !score->staff(staff_idx)->show()) {
            continue;
        }

        const Shape &fshape = f->staff_shape(staff_idx);
        double sp = shape_spatium(fshape);
        d = min_horizontal_distance(fshape, ns->staff_shape(staff_idx), sp, squeeze_factor);
        if (system_header_gap) {
            // first chordrest of a staff should clear the widest header for any staff
            // so make sure segment is as wide as it needs to be
            d = std::max(d, f->staff_shape(staff_idx).right());
        }

        // Multi-staff ChordBrackets are stored on their owner chords
        // and are therefore not fully represented in every visually spanned staff Shape.
        // Add the missing bracket collisions before combining the per-staff minimum distances.
        if (f->is_chord_rest_type() || ns->is_chord_rest_type()) {
            ChordBracketLayout::update_horizontal_spacing(f, ns, staff_idx, squeeze_factor, d);
        }

        if (f->is_chord_rest_type() && ns->is_chord_rest_type()) {
            check_collisions_with_cross_staff_stems(f, ns, staff_idx, d);
        }

        ww = std::max(ww, d);
    }
    double w = std::max(ww, 0.0); // non-negative

    // Header exceptions that need additional space (more than the padding)
    double absolute_min_header_dist = 1.5 * f->spatium() * squeeze_factor;
    if (system_header_gap) {
        if (f->is_time_sig_type()) {
            double time_sig_header_dist = squeeze_factor * f->style().style_absolute(Sid::SYSTEM_HEADER_TIME_SIG_DISTANCE);
            w = std::max(w, f->min_right() + time_sig_header_dist);
        } else {
            double header_dist = squeeze_factor * f->style().style_absolute(Sid::SYSTEM_HEADER_DISTANCE);
            w = std::max(w, f->min_right() + header_dist);
        }
        if (ns && ns->is_start_repeat_bar_line_type()) {
            // Align the thin barline of the start repeat to the header
            w -= f->style().style_absolute(Sid::END_BAR_WIDTH) + f->style().style_absolute(Sid::END_BAR_DISTANCE);
        }
        double diff = w - f->min_right() - ns->min_left();
        if (diff < absolute_min_header_dist) {
            w += absolute_min_header_dist - diff;
        }
    }

    // Multimeasure rest exceptions that need special handling
    if (f->measure() && f->measure()->is_mm_rest()) {
        const PaddingTable &padding_table = f->score()->padding_tables().horizontal_padding_table();
        if (ns->is_chord_rest_type()) {
            double min_dist = f->min_right();
            if (f->is_clef_type()) {
                min_dist += padding_table.at(ElementType::CLEF).at(ElementType::REST);
            } else if (f->is_key_sig_type()) {
                min_dist += padding_table.at(ElementType::KEYSIG).at(ElementType::REST);
            } else if (f->is_time_sig_type()) {
                min_dist += padding_table.at(ElementType::TIMESIG).at(ElementType::REST);
            }
            w = std::max(w, min_dist);
        } else if (f->is_chord_rest_type()) {
            double min_width = f->style().style_absolute(Sid::MIN_MM_REST_WIDTH);
            if (!f->style().style_b(Sid::OLD_STYLE_MULTI_MEASURE_RESTS)) {
                min_width += f->style().style_absolute(Sid::MULTI_MEASURE_REST_MARGIN);
            }
            w = std::max(w, min_width);
        }
    }

    // Allocate space to ensure minimum length of hanging ties or gliss at start of system
    // These only occur when one segment is a ChordRest and the other isn't
    // Allocate space to ensure minimum length of partial ties
    if (f->is_chord_rest_type() == ns->is_chord_rest_type()) {
        w = std::max(w, min_stem_dist_on_non_adjacent_cross(f, ns));
        return w;
    }

    const bool repeat_seg = f->is_start_repeat_bar_line_type() || ns->is_start_repeat_bar_line_type();
    const bool end_of_system = f->is_chord_rest_type() && !(ns->is_chord_rest_type() || ns->is_start_repeat_bar_line_type()) &&
                               (f->measure()->is_last_in_system() || f->measure()->next()->is_h_box());
    const bool system_end = repeat_seg || end_of_system;

    compute_hanging_line_width(f, ns, w, system_header_gap, system_end);

    return w;
}

bool HorizontalSpacing::needs_header_spacing_exceptions(const Segment *seg, const Segment *next_seg) {
    static const std::unordered_set<SegmentType> header_segment_types = {
        SegmentType::HEADER_CLEF, SegmentType::KEY_SIG,  SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE,
        SegmentType::AMBITUS,     SegmentType::TIME_SIG, SegmentType::TIME_SIG_START_REPEAT_ANNOUNCE};

    return muse::contains(header_segment_types, seg->segment_type()) && seg->rtick().is_zero() && !seg->has_time_sig_above_staves() &&
           (next_seg->measure()->is_first_in_system() || next_seg->measure()->prev()->is_h_box()) &&
           (next_seg->is_start_repeat_bar_line_type() || next_seg->is_chord_rest_type());
}

//---------------------------------------------------------
//   minLeft
//    Calculate minimum distance needed to the left shape
//    sl. Sl is the same for all staves.
//---------------------------------------------------------

double HorizontalSpacing::min_left(const Segment *seg, const Shape &ls) {
    double distance = 0.0;
    double sp = shape_spatium(ls);
    for (const Shape &sh : seg->shapes()) {
        double d = min_horizontal_distance(ls, sh, sp, 1.0);
        if (d > distance) {
            distance = d;
        }
    }
    return distance;
}

double HorizontalSpacing::compute_padding(const EngravingItem *item1, const EngravingItem *item2) {
    if (item1->is_parenthesis() || item2->is_parenthesis()) {
        return ParenthesisLayout::compute_parenthesis_padding(item1, item2);
    }
    const PaddingTable &padding_table = item1->score()->padding_tables().horizontal_padding_table();
    ElementType type1 = item1->type();
    ElementType type2 = item2->type();

    double padding = padding_table.at(type1).at(type2);
    double scaling = (item1->mag() + item2->mag()) / 2;

    if (type1 == ElementType::NOTE && is_special_note_padding_type(type2)) {
        compute_note_padding(to_note(item1), item2, padding, scaling);
    } else if (type1 == ElementType::LYRICS && is_special_lyrics_padding_type(type2)) {
        compute_lyrics_padding(to_lyrics(item1), item2, padding);
    } else if (type2 == ElementType::CHORD_BRACKET) {
        compute_chord_bracket_padding(item1, to_chord_bracket(item2), padding);
    } else {
        padding *= scaling;
    }

    if (!item1->is_ledger_line() && item2->is_rest()) {
        compute_ledger_rest_padding(to_rest(item2), padding);
    }

    return padding;
}

bool HorizontalSpacing::is_special_note_padding_type(ElementType type) {
    switch (type) {
        case ElementType::NOTE:
        case ElementType::REST:
        case ElementType::STEM: return true;
        default: return false;
    }
}

void HorizontalSpacing::compute_note_padding(const Note *note, const EngravingItem *item2, double &padding, double scaling) {
    const MStyle &style = note->style();

    bool same_voice_note_or_stem = (item2->is_note() || item2->is_stem()) && note->track() == item2->track();
    bool are_adjacent_by_duration = (note->rtick() + note->chord()->actual_ticks()) == item2->rtick();
    if (same_voice_note_or_stem || are_adjacent_by_duration) {
        bool intersection = note->shape().translate(note->pos()).intersects(item2->shape().translate(item2->pos()));
        if (intersection) {
            padding = std::max(padding, static_cast<double>(style.style_absolute(Sid::MIN_NOTE_DISTANCE)));
        }
    }

    padding *= scaling;

    if (!(item2->is_note() || item2->is_rest())) {
        return;
    }

    if (note->is_grace() && item2->is_note() && to_note(item2)->is_grace()) {
        // Grace-to-grace
        padding = std::max(padding, static_cast<double>(style.style_absolute(Sid::GRACE_TO_GRACE_NOTE_DIST)));
    } else if (note->is_grace() && (item2->is_rest() || (item2->is_note() && !to_note(item2)->is_grace()))) {
        // Grace-to-main
        padding = std::max(padding, static_cast<double>(style.style_absolute(Sid::GRACE_TO_MAIN_NOTE_DIST)));
    } else if (!note->is_grace() && item2->is_note() && to_note(item2)->is_grace()) {
        // Main-to-grace
        padding = std::max(padding, static_cast<double>(style.style_absolute(Sid::GRACE_TO_MAIN_NOTE_DIST)));
    }

    if (!note->fret_string().empty() && item2->is_note()) {      // This is a TAB fret mark
        static constexpr double tab_fret_padding_increase = 1.5; // TODO: style?
        padding *= tab_fret_padding_increase;
    }

    if (!item2->is_note()) {
        return;
    }

    const Note *note2 = to_note(item2);
    if (note->line_attach_points().empty() || note2->line_attach_points().empty()) {
        return;
    }

    // Allocate space for minTieLength, minGlissandoLength & minBendLength
    for (LineAttachPoint la_point1 : note->line_attach_points()) {
        if (!la_point1.line()->add_to_skyline()) {
            continue;
        }
        for (LineAttachPoint la_point2 : note2->line_attach_points()) {
            if (la_point1.line() != la_point2.line()) {
                continue;
            }

            double min_end_points_distance = 0.0;
            if (la_point1.line()->is_tie()) {
                min_end_points_distance = style.style_absolute(Sid::MIN_TIE_LENGTH);
            } else if (la_point1.line()->is_glissando()) {
                bool straight = to_glissando(la_point1.line())->glissando_type() == GlissandoType::STRAIGHT;
                double min_glissando_length = straight ? style.style_absolute(Sid::MIN_STRAIGHT_GLISSANDO_LENGTH) :
                                                         style.style_absolute(Sid::MIN_WIGGLY_GLISSANDO_LENGTH);
                min_end_points_distance = min_glissando_length;
            } else if (la_point1.line()->is_guitar_bend()) {
                double min_bend_length = 2 * note->spatium(); // TODO: style
                min_end_points_distance = min_bend_length;
            } else if (la_point1.line()->is_note_line()) {
                min_end_points_distance = style.style_absolute(Sid::MIN_STRAIGHT_GLISSANDO_LENGTH);
            }

            double lap_padding = (la_point1.pos().x() - note->width()) + min_end_points_distance - la_point2.pos().x();
            lap_padding *= scaling;

            padding = std::max(padding, lap_padding);
        }
    }
}

void HorizontalSpacing::compute_ledger_rest_padding(const Rest *rest2, double &padding) {
    SymId rest_sym = rest2->ldata()->sym();
    switch (rest_sym) {
        case SymId::REST_WHOLE_LEGER_LINE:
        case SymId::REST_DOUBLE_WHOLE_LEGER_LINE:
        case SymId::REST_HALF_LEGER_LINE: padding += rest2->ldata()->bbox().left(); return;
        default: return;
    }
}

bool HorizontalSpacing::is_special_lyrics_padding_type(ElementType type) {
    switch (type) {
        case ElementType::NOTE:
        case ElementType::REST:
        case ElementType::LYRICS: return true;
        default: return false;
    }
}

void HorizontalSpacing::compute_lyrics_padding(const Lyrics *lyrics1, const EngravingItem *item2, double &padding) {
    const MStyle &style = lyrics1->style();

    bool leave_space_for_melisma =
        lyrics1->separator() && lyrics1->separator()->is_end_melisma() && style.style_b(Sid::LYRICS_MELISMA_FORCE);
    if (leave_space_for_melisma) {
        double space_for_melisma = style.style_absolute(Sid::LYRICS_MELISMA_MIN_LENGTH) + 2 * style.style_absolute(Sid::LYRICS_MELISMA_PAD);
        padding = std::max(padding, space_for_melisma);
        return;
    }

    if (item2->is_lyrics()) {
        LyricsSyllabic syllabic_type = lyrics1->syllabic();
        bool leave_space_for_dash =
            (syllabic_type == LyricsSyllabic::BEGIN || syllabic_type == LyricsSyllabic::MIDDLE) && style.style_b(Sid::LYRICS_DASH_FORCE);
        if (leave_space_for_dash) {
            double space_for_dash = style.style_absolute(Sid::LYRICS_DASH_MIN_LENGTH) + 2 * style.style_absolute(Sid::LYRICS_DASH_PAD);
            padding = std::max(padding, space_for_dash);
        }
    }
}

void HorizontalSpacing::compute_chord_bracket_padding(const EngravingItem *item1, const ChordBracket *chord_bracket, double &padding) {
    const Chord *chord = chord_bracket->chord();
    const Chord *item_chord = to_chord(item1->find_ancestor(ElementType::CHORD));
    if (chord && item_chord && chord->segment() == item_chord->segment() && chord->part() == item_chord->part()) {
        // Padding a right-handed chord bracket to chords in the same Segment and part: use the same padding values as the left-handed case
        padding = item1->score()->padding_tables().horizontal_padding_table().at(ElementType::CHORD_BRACKET).at(item1->type());
    }
}

KerningType HorizontalSpacing::compute_kerning(const EngravingItem *item1, const EngravingItem *item2) {
    if (item1->is_articulation_or_fermata() || item2->is_articulation_or_fermata()) {
        return compute_articulation_and_fermata_kerning(item1, item2);
    }

    if (ignore_items(item1, item2)) {
        return KerningType::ALLOW_COLLISION;
    }

    if (is_same_voice_kerning_limited(item1) && is_same_voice_kerning_limited(item2) && item1->track() == item2->track()) {
        return KerningType::NON_KERNING;
    }

    if ((is_never_kernable(item1) || is_never_kernable(item2)) && !(is_always_kernable(item1) || is_always_kernable(item2))) {
        return KerningType::NON_KERNING;
    }

    return do_compute_kerning_type(item1, item2);
}

double HorizontalSpacing::compute_vertical_clearance(const EngravingItem *item1, const EngravingItem *item2, double spatium) {
    // To be possibly expanded to more cases
    if (item2 && item2->is_accidental()) {
        return 0.1 * spatium;
    } else if ((item1 && item1->is_parenthesis()) || (item2 && item2->is_parenthesis())) {
        return 0.1 * spatium;
    }

    return 0.2 * spatium;
}

bool HorizontalSpacing::is_same_voice_kerning_limited(const EngravingItem *item) {
    ElementType type = item->type();

    switch (type) {
        case ElementType::NOTE:
        case ElementType::NOTEDOT:
        case ElementType::REST:
        case ElementType::STEM:
        case ElementType::CHORDLINE:
        case ElementType::BREATH: return true;
        default: return false;
    }
}

bool HorizontalSpacing::is_never_kernable(const EngravingItem *item) {
    ElementType type = item->type();

    switch (type) {
        case ElementType::CLEF:
            if (to_clef(item)->is_mid_measure_clef()) {
                return false;
            }
        // fall through
        case ElementType::KEYSIG:
        case ElementType::BAR_LINE: return true;
        case ElementType::TIMESIG: return !to_time_sig(item)->is_above_staves();
        default: return false;
    }
}

bool HorizontalSpacing::is_always_kernable(const EngravingItem *item) {
    return item->is_text_base() || item->is_chord_line() || item->is_parenthesis() || item->is_fret_diagram();
}

bool HorizontalSpacing::ignore_items(const EngravingItem *item1, const EngravingItem *item2) {
    if (item1->is_rest() && to_rest(item1)->is_full_measure_rest()) {
        // Full-measure rest must ignore cross-stave notes
        return item1->staff_idx() != item2->staff_idx();
    }

    return false;
}

KerningType HorizontalSpacing::do_compute_kerning_type(const EngravingItem *item1, const EngravingItem *item2) {
    ElementType type1 = item1->type();
    switch (type1) {
        case ElementType::BAR_LINE:
            return item2->is_lyrics() || item2->is_harmony() || item2->is_fret_diagram() ? KerningType::ALLOW_COLLISION :
                                                                                           KerningType::NON_KERNING;
        case ElementType::CHORDLINE: return item2->is_bar_line() ? KerningType::ALLOW_COLLISION : KerningType::KERNING;
        case ElementType::HARMONY:
        case ElementType::FRET_DIAGRAM:
            return item2->is_harmony() || item2->is_fret_diagram() ? KerningType::NON_KERNING : KerningType::KERNING;
        case ElementType::LYRICS: return compute_lyrics_kerning_type(to_lyrics(item1), item2);
        case ElementType::NOTE: return compute_note_kerning_type(to_note(item1), item2);
        case ElementType::CLEF: return item2->is_note() ? KerningType::KERN_UNTIL_RIGHT_EDGE : KerningType::KERNING;
        case ElementType::STEM_SLASH: return compute_stem_slash_kerning_type(to_stem_slash(item1), item2);
        case ElementType::PARENTHESIS: return item2->is_bar_line() ? KerningType::NON_KERNING : KerningType::KERNING;
        case ElementType::GUITAR_BEND_SEGMENT:
            return item2->is_note() || item2->is_rest() ? KerningType::NON_KERNING : KerningType::KERNING;
        default: return KerningType::KERNING;
    }
}

KerningType HorizontalSpacing::compute_note_kerning_type(const Note *note, const EngravingItem *item2) {
    if (item2->is_clef()) {
        return KerningType::KERN_UNTIL_RIGHT_EDGE;
    }

    EngravingObject *next_parent = item2->ownership_parent();
    if (next_parent && next_parent->is_note() && to_note(next_parent)->is_trill_cue_note()) {
        return KerningType::NON_KERNING;
    }

    Chord *c = note->chord();
    if (!c) {
        return KerningType::KERNING;
    }
    if (item2->is_lyrics() && c->is_melisma_end()) {
        Note *melisma_end_note = c->up() ? c->down_note() : c->up_note();
        return note == melisma_end_note ? KerningType::NON_KERNING : KerningType::KERNING;
    }
    if (c->allow_kerning_above() && c->allow_kerning_below()) {
        return KerningType::KERNING;
    }

    if (c->up() && note->ldata()->pos().x() > 0) {
        // Offset seconds can always be kerned into
        return KerningType::KERNING;
    }

    bool kerning_above = item2->canvas_pos().y() < note->canvas_pos().y();
    if (kerning_above && !c->allow_kerning_above()) {
        return KerningType::NON_KERNING;
    }
    if (!kerning_above && !c->allow_kerning_below()) {
        return KerningType::NON_KERNING;
    }

    return KerningType::KERNING;
}

KerningType HorizontalSpacing::compute_stem_slash_kerning_type(const StemSlash *stem_slash, const EngravingItem *item2) {
    if (!stem_slash->chord() || !stem_slash->chord()->beam() || !item2->ownership_parent()) {
        return KerningType::KERNING;
    }

    EngravingObject *next_parent = item2->ownership_parent();
    Chord *next_chord = nullptr;
    if (next_parent->is_chord()) {
        next_chord = to_chord(next_parent);
    } else if (next_parent->is_note()) {
        next_chord = to_chord(next_parent->ownership_parent());
    }
    if (!next_chord) {
        return KerningType::KERNING;
    }

    if (next_chord->beam() && next_chord->beam() == stem_slash->chord()->beam()) {
        // Stem slash is allowed to collide with items from the same grace notes group
        return KerningType::ALLOW_COLLISION;
    }

    return KerningType::KERNING;
}

KerningType HorizontalSpacing::compute_lyrics_kerning_type(const Lyrics *lyrics1, const EngravingItem *item2) {
    if (item2->is_lyrics()) {
        const Lyrics *lyrics2 = to_lyrics(item2);
        if (lyrics1->verse() == lyrics2->verse()) {
            return KerningType::NON_KERNING;
        }
    }

    if ((item2->is_note() || item2->is_rest()) && lyrics1->style().style_b(Sid::LYRICS_MELISMA_FORCE)) {
        LyricsLine *melisma_line = lyrics1->separator();
        if (melisma_line && melisma_line->is_end_melisma() && item2->tick() >= melisma_line->tick2()) {
            return KerningType::NON_KERNING;
        }
    }

    return KerningType::ALLOW_COLLISION;
}

KerningType HorizontalSpacing::compute_articulation_and_fermata_kerning(const EngravingItem *item1, const EngravingItem *item2) {
    if (item1->is_articulation_or_fermata()) {
        if (item2->is_articulation_or_fermata()) {
            bool first_above = item1->is_articulation_family() ? to_articulation(item1)->up() : item1->place_above();
            bool second_above = item2->is_articulation_family() ? to_articulation(item2)->up() : item2->place_above();
            if (first_above == second_above) {
                return KerningType::NON_KERNING;
            }
        }
    }

    return KerningType::KERNING;
}

void HorizontalSpacing::compute_hanging_line_width(const Segment *first_seg, const Segment *next_seg, double &width, bool system_header_gap,
                                                   bool system_end) {
    const MStyle &style = first_seg->style();
    const Segment *cr_seg = first_seg->is_chord_rest_type() ? first_seg : next_seg;
    const Segment *other_seg = first_seg->is_chord_rest_type() ? next_seg : first_seg;
    const bool incoming = !first_seg->is_chord_rest_type();
    const Measure *cr_measure = cr_seg->measure();
    const size_t ntracks = cr_seg->score()->ntracks();

    std::vector<double> tie_lengths;

    for (track_idx_t track = 0; track < ntracks; track++) {
        // Hanging lines only occur at start or end of a measure
        const ChordRest *cr = incoming ? cr_measure->first_chord_rest(track) : cr_measure->last_chord_rest(track);
        if (!cr || !cr->is_chord() || cr->segment() != cr_seg) {
            continue;
        }

        const double header_line_margin = system_header_gap ? other_seg->style().style_absolute(Sid::HEADER_TO_LINE_START_DISTANCE) :
                                                              other_seg->style().style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION);
        const double end_system_margin = style.style_absolute(Sid::LINE_END_TO_BARLINE_DISTANCE);

        for (const Note *note : to_chord(cr)->notes()) {
            const bool line_back = note->spanner_back().size() || (note->tie_back() && !note->tie_back()->segments_empty());
            const bool line_for = note->spanner_for().size() || (note->tie_for() && !note->tie_for()->segments_empty());
            if (!(line_back || line_for) || note->line_attach_points().empty()) {
                continue;
            }

            for (const LineAttachPoint &lap : note->line_attach_points()) {
                if (lap.start() == incoming) {
                    continue;
                }
                const Spanner *attached_line = to_spanner(lap.line());
                if (!attached_line->add_to_skyline()) {
                    continue;
                }

                // Partial ties are adjusted wherever they are in the system, other spanners are only adjusted over system breaks
                if (!(system_end || system_header_gap) && !attached_line->is_partial_tie()) {
                    continue;
                }

                double min_length = 0.0;
                if (attached_line->is_glissando()) {
                    bool straight = to_glissando(attached_line)->glissando_type() == GlissandoType::STRAIGHT;
                    min_length = straight ? style.style_absolute(Sid::MIN_STRAIGHT_GLISSANDO_LENGTH) :
                                            style.style_absolute(Sid::MIN_WIGGLY_GLISSANDO_LENGTH);
                } else if (attached_line->is_note_line()) {
                    min_length = style.style_absolute(Sid::MIN_STRAIGHT_GLISSANDO_LENGTH);
                }

                const double note_pos_x = note->pos().x() + to_chord(cr)->pos().x();
                const double line_note_end_pos = (incoming ? width : 0.0) + note_pos_x + lap.pos().x();
                const double line_seg_end_pos =
                    (incoming ? other_seg->min_right() + header_line_margin : width + other_seg->min_left() - end_system_margin);

                const double line_start_point_x = incoming ? line_seg_end_pos : line_note_end_pos;
                const double line_end_point_x = incoming ? line_note_end_pos : line_seg_end_pos;
                const double line_length = line_end_point_x - line_start_point_x;

                if (attached_line->is_tie()) {
                    tie_lengths.push_back(line_length);
                    continue;
                }

                if (line_length < min_length) {
                    width += min_length - line_length;
                }
            }
        }
    }

    if (tie_lengths.empty()) {
        return;
    }

    const double max_length = *std::max_element(tie_lengths.begin(), tie_lengths.end());
    const double old_width = width;
    for (double &tie_length : tie_lengths) {
        // Adjust for new width
        tie_length += width - old_width;
        const double min_length = muse::real_is_equal(tie_length, max_length) ? style.style_absolute(Sid::MIN_HANGING_TIE_LENGTH) :
                                                                                style.style_absolute(Sid::MIN_TIE_LENGTH);

        if (tie_length < min_length) {
            width += min_length - tie_length;
        }
    }
}
