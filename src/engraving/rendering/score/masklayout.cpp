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

#include "masklayout.h"

#include "dom/barline.h"
#include "dom/chord.h"
#include "dom/jump.h"
#include "dom/lyrics.h"
#include "dom/marker.h"
#include "dom/measure.h"
#include "dom/measurenumber.h"
#include "dom/note.h"
#include "dom/page.h"
#include "dom/parenthesis.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "dom/stafflines.h"
#include "dom/system.h"
#include "dom/textlinebase.h"
#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void MaskLayout::compute_masks(LayoutContext &ctx, Page *page) {

    bool mask_barlines = ctx.conf().style_b(Sid::MASK_BARLINES_FOR_TEXT);
    bool mask_slurs = ctx.conf().style_b(Sid::MASK_SLURS);
    bool mask_ties = ctx.conf().style_b(Sid::MASK_TIES);

    for (const System *system : page->systems()) {
        std::vector<TextBase *> all_system_text = collect_all_system_text(system);

        for (MeasureBase *mb : system->measures()) {
            if (!mb->is_measure()) {
                continue;
            }
            Measure *measure = to_measure(mb);

            if (mask_barlines) {
                for (const Segment &seg : measure->segments()) {
                    if (seg.is_type(SegmentType::BAR_LINE_TYPES)) {
                        compute_barline_masks(&seg, system, all_system_text, ctx);
                    }
                }
            }

            staff_idx_t nstaves = ctx.dom().nstaves();
            for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
                if (staff_idx >= system->staves().size() || !system->staff(staff_idx)->show()) {
                    continue;
                }
                const Staff *staff = ctx.dom().staff(staff_idx);
                const StaffType *staff_type = staff->staff_type(measure->tick());
                StaffLines *staff_lines = measure->staff_lines(staff_idx);
                if (staff_type->is_tab_staff()) {
                    mask_tab_string_lines_for_frets(staff_lines, ctx);
                }
            }
        }

        for (SpannerSegment *spanner_seg : system->spanner_segments()) {
            if (!spanner_seg->is_slur_tie_segment() || spanner_seg->is_tapping_half_slur_segment() ||
                !system->staff(spanner_seg->staff_idx())->show() || !spanner_seg->visible()) {
                continue;
            }

            spanner_seg->mutldata()->set_mask(Shape());

            AutoOnOff slur_tie_mask_override = spanner_seg->get_property(Pid::MASK_SLURTIE).value<AutoOnOff>();
            if (slur_tie_mask_override == AutoOnOff::ON ||
                (slur_tie_mask_override == AutoOnOff::AUTO &&
                 ((mask_slurs && spanner_seg->is_slur_segment()) || (mask_ties && spanner_seg->is_tie_segment())))) {
                compute_slur_tie_masks(to_slur_tie_segment(spanner_seg));
            }
        }
    }
}

void MaskLayout::compute_barline_masks(const Segment *barline_sement, const System *system, const std::vector<TextBase *> &all_system_text,
                                       LayoutContext &ctx) {
    if (barline_sement->measure()->is_last_in_system() && barline_sement == barline_sement->measure()->last_enabled()) {
        return;
    }

    staff_idx_t nstaves = ctx.dom().nstaves();

    std::vector<BarLine *> barlines;
    barlines.reserve(nstaves);

    for (staff_idx_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        if (!system->staff(staff_idx)->show()) {
            continue;
        }
        BarLine *barline = to_bar_line(barline_sement->element(staff2track(staff_idx)));
        if (!barline || !barline->span_staff()) {
            continue;
        }
        mask_barline_for_text(barline, all_system_text);
    }
}

void MaskLayout::mask_barline_for_text(BarLine *barline, const std::vector<TextBase *> &all_system_text) {

    PointF barline_pos = barline->page_pos();
    Shape barline_shape = barline->shape().translated(barline_pos);

    Shape mask;
    const double spatium = barline->spatium();

    for (TextBase *text : all_system_text) {
        const double font_size_scale_factor = text->size() / 10.0;
        const double collision_padding = 0.2 * spatium * font_size_scale_factor;
        const bool has_frame = text->frame_type() != FrameType::NO_FRAME;
        const bool use_high_res_shape = !text->is_dynamic() && !text->has_frame();
        const double mask_padding = has_frame ? 0.0 : std::clamp(0.5 * spatium * font_size_scale_factor, 0.1 * spatium, spatium);

        PointF text_pos = text->page_pos();
        if (!barline_shape.intersects(text->ldata()->bbox().translated(text_pos).padded(collision_padding))) {
            continue;
        }

        Shape text_shape = (use_high_res_shape ? text->ldata()->high_res_shape() : text->ldata()->shape()).translated(text_pos);

        Shape filtered_text_shape = create_filtered_item_shape(text_shape, barline_shape, collision_padding);
        if (filtered_text_shape.empty()) {
            continue;
        }

        filtered_text_shape = filtered_text_shape.bbox();
        filtered_text_shape.pad(mask_padding);

        mask.add(filtered_text_shape.translate(-barline_pos));
    }

    if (mask.empty()) {
        barline->mutldata()->set_mask(mask);
        return;
    }

    // Ensure that we don't leave tiny barline fragments. If two masking
    // elements are too close to each other we extend them to join.
    barline_shape.translate(-barline_pos);
    const double min_fragment_lengh = 0.5 * spatium;
    cleanup_mask(barline_shape, mask, min_fragment_lengh);

    barline->mutldata()->set_mask(mask);
}

void MaskLayout::cleanup_mask(const Shape &item_shape, Shape &mask, double min_fragment_length) {
    for (size_t i = 0; i < mask.size(); ++i) {
        ShapeElement &el = mask.elements()[i];

        if (std::abs(el.top() - item_shape.top()) < min_fragment_length) {
            el.adjust(0.0, -min_fragment_length, 0.0, 0.0);
        }
        if (std::abs(item_shape.bottom() - el.bottom()) < min_fragment_length) {
            el.adjust(0.0, 0.0, 0.0, min_fragment_length);
        }
        if (std::abs(el.left() - item_shape.left()) < min_fragment_length) {
            el.adjust(-min_fragment_length, 0.0, 0.0, 0.0);
        }
        if (std::abs(item_shape.right() - el.right()) < min_fragment_length) {
            el.adjust(0.0, 0.0, min_fragment_length, 0.0);
        }

        for (size_t j = i + 1; j < mask.size(); ++j) {
            ShapeElement &other_el = mask.elements()[j];
            if (intersects(el.left(), el.right(), other_el.left(), other_el.right())) {
                bool other_is_below = other_el.y() > el.y();
                double vert_clearance = other_is_below ? other_el.top() - el.bottom() : el.top() - other_el.bottom();
                if (vert_clearance < min_fragment_length) {
                    (other_is_below ? el : other_el).adjust(0.0, 0.0, 0.0, min_fragment_length);
                }
            }
            if (intersects(el.top(), el.bottom(), other_el.top(), el.bottom())) {
                bool other_is_right = other_el.x() > el.x();
                double hor_clearance = other_is_right ? other_el.left() - el.right() : el.left() - other_el.right();
                if (hor_clearance < min_fragment_length) {
                    (other_is_right ? el : other_el).adjust(0.0, 0.0, min_fragment_length, 0.0);
                }
            }
        }
    }
}

std::vector<TextBase *> MaskLayout::collect_all_system_text(const System *system) {

    std::vector<TextBase *> all_text;

    for (const MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        const Measure *measure = to_measure(mb);
        for (staff_idx_t i = 0; i < measure->mstaves().size(); ++i) {
            if (measure->show_measure_number_on_staff(i)) {
                if (MeasureNumber *measure_number = measure->measure_number(i)) {
                    all_text.push_back(measure_number);
                }
            }
        }
        for (EngravingItem *item : measure->el()) {
            const SysStaff *staff = system ? system->staff(item->staff_idx()) : nullptr;
            const bool staff_visible = staff && staff->show();
            if ((item->is_marker() || item->is_jump()) && item->visible() && staff_visible) {
                all_text.push_back(to_text_base(item));
            }
        }
        for (const Segment &s : measure->segments()) {
            if (!s.is_type(SegmentType::DURATION) || !s.enabled()) {
                continue;
            }
            for (EngravingItem *annotation : s.annotations()) {
                const SysStaff *staff = system ? system->staff(annotation->staff_idx()) : nullptr;
                const bool staff_visible = staff && staff->show();
                if (annotation->is_text_base() && annotation->visible() && staff_visible) {
                    all_text.push_back(to_text_base(annotation));
                }
            }
            if (!s.is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *chord_rest : s.elist()) {
                if (!chord_rest || !system->staff(chord_rest->staff_idx())->show()) {
                    continue;
                }
                for (Lyrics *lyr : to_chord_rest(chord_rest)->lyrics()) {
                    if (lyr->visible()) {
                        all_text.push_back(lyr);
                    }
                }
            }
        }
    }

    for (SpannerSegment *spanner_segment : system->spanner_segments()) {
        if (!spanner_segment->is_text_line_base_segment() || !system->staff(spanner_segment->staff_idx())->show() ||
            !spanner_segment->get_property(Pid::VISIBLE).to_bool()) {
            continue;
        }
        TextLineBaseSegment *text_line_base_segment = static_cast<TextLineBaseSegment *>(spanner_segment);
        Text *begin_text = text_line_base_segment->text();
        Text *end_text = text_line_base_segment->end_text();
        if (begin_text && !begin_text->empty()) {
            all_text.push_back(begin_text);
        }
        if (end_text && !end_text->empty()) {
            all_text.push_back(end_text);
        }
    }

    return all_text;
}

void MaskLayout::mask_tab_string_lines_for_frets(StaffLines *staff_lines, const LayoutContext &ctx) {
    staff_idx_t staff_idx = staff_lines->staff_idx();
    bool lines_through = ctx.dom().staff(staff_idx)->staff_type(Fraction())->lines_through();

    PointF staff_lines_pos = staff_lines->page_pos();

    double padding = ctx.conf().style_absolute(Sid::TAB_FRET_PADDING);

    track_idx_t start_track = staff2track(staff_idx);
    track_idx_t end_track = start_track + ::mu::engraving::voices;

    Shape mask;

    auto mask_fret = [&mask, lines_through, padding, staff_lines_pos](Chord *chord) {
        for (Note *note : chord->notes()) {
            if (!note->visible()) {
                continue;
            }
            if (!note->should_hide_fret() && (!lines_through || note->fret_conflict())) {
                Shape note_shape = note->ldata()->bbox();
                note_shape.translate(note->page_pos());

                note_shape.pad(padding);
                mask.add(note_shape.translated(-staff_lines_pos));
            }
        }
    };

    auto mask_parens = [&mask, lines_through, padding, staff_lines_pos](Chord *chord) {
        if (lines_through) {
            return;
        }
        for (const NoteParenthesisInfo *i : chord->note_parentheses()) {
            const Parenthesis *left_paren = i->left_paren();
            const Parenthesis *right_paren = i->right_paren();
            bool all_hidden = false;
            for (const Note *note : i->notes()) {
                if (!note->should_hide_fret()) {
                    all_hidden = false;
                    break;
                }
            }

            if (all_hidden) {
                continue;
            }

            // HACK: Overlapping mask regions which have the same y value and height cause errors
            // Parentheses on single fret marks have the same height as the fret marks
            static const double paren_padding_epsilon = 1.0;
            if (left_paren && left_paren->visible()) {
                Shape left_paren_shape = left_paren->ldata()->bbox().translated(left_paren->page_pos());
                left_paren_shape.pad(padding);
                left_paren_shape.adjust(0, -paren_padding_epsilon, 0, paren_padding_epsilon);
                mask.add(left_paren_shape.translated(-staff_lines_pos));
            }

            if (right_paren && right_paren->visible()) {
                Shape right_paren_shape = right_paren->ldata()->bbox().translated(right_paren->page_pos());
                right_paren_shape.pad(padding);
                right_paren_shape.adjust(0, -paren_padding_epsilon, 0, paren_padding_epsilon);
                mask.add(right_paren_shape.translated(-staff_lines_pos));
            }
        }
    };

    const Measure *measure = staff_lines->measure();
    for (Segment *seg = measure->first(SegmentType::CHORD_REST); seg; seg = seg->next(SegmentType::CHORD_REST)) {
        for (track_idx_t track = start_track; track < end_track; ++track) {
            EngravingItem *el = seg->element(track);
            if (!el || !el->is_chord()) {
                continue;
            }
            mask_fret(to_chord(el));
            mask_parens(to_chord(el));
            for (Chord *grace : to_chord(el)->grace_notes()) {
                mask_fret(grace);
                mask_parens(grace);
            }
        }
    }

    staff_lines->mutldata()->set_mask(mask);
}

void MaskLayout::compute_slur_tie_masks(SlurTieSegment *slur_tie_segment) {

    Spanner *spanner = slur_tie_segment->spanner();
    const Segment *start_seg = spanner->start_segment();
    const Segment *end_seg = spanner->end_segment();
    const EngravingItem *start_element = spanner->start_element();
    const EngravingItem *end_element = spanner->end_element();
    staff_idx_t start_staff_idx = start_element ? start_element->v_staff_idx() : 0;
    staff_idx_t end_staff_idx = end_element ? end_element->v_staff_idx() : spanner->score()->nstaves() - 1;
    if (start_staff_idx > end_staff_idx) {
        std::swap(start_staff_idx, end_staff_idx);
    }

    std::vector<const EngravingItem *> items_to_mask_over;
    for (const Segment *seg = start_seg; seg && seg != end_seg; seg = seg->next1()) {
        if (!seg->enabled() || !seg->is_type(SegmentType::KEY_SIG_TYPES | SegmentType::TIME_SIG_TYPES | SegmentType::CLEF_TYPES) ||
            seg->system() != slur_tie_segment->system()) {
            continue;
        }
        for (staff_idx_t staff_idx = start_staff_idx; staff_idx <= end_staff_idx; ++staff_idx) {
            if (EngravingItem *item = seg->element(
                    staff_idx * ::mu::engraving::voices)) { // Key/Time/Clef-type segments never have elements in any voice other than 0
                items_to_mask_over.push_back(item);
            }
        }
    }

    PointF slur_tie_pos = slur_tie_segment->page_pos();
    Shape slur_tie_shape = slur_tie_segment->shape().translated(slur_tie_pos);

    Shape mask;
    const double spatium = slur_tie_segment->spatium();
    const double mask_padding = spatium * StyleDef::style_values[size_t(Sid::KEYSIG_LEFT_MARGIN)].default_value.to_double() / 2;
    const double horizontal_collision_padding = mask_padding;
    const double vertical_collision_padding = spatium * 0.1;

    for (const EngravingItem *item : items_to_mask_over) {
        PointF item_pos = item->page_pos();
        Shape item_shape = item->shape().translated(item_pos);

        if (!slur_tie_shape.bbox().intersects(item_shape.bbox().adjusted(-horizontal_collision_padding, -vertical_collision_padding,
                                                                         horizontal_collision_padding, vertical_collision_padding))) {
            continue;
        }

        Shape filtered_item_shape =
            create_filtered_item_shape(item_shape, slur_tie_shape, horizontal_collision_padding, vertical_collision_padding);
        if (filtered_item_shape.empty()) {
            continue;
        }

        mask.add(filtered_item_shape.translate(-slur_tie_pos));
    }

    if (mask.empty()) {
        slur_tie_segment->mutldata()->set_mask(mask);
        return;
    }

    slur_tie_shape.translate(-slur_tie_pos);

    // Use the mask's bbox to break the slur/tie only once, regardless of the mask elements in between:
    RectF mask_bbox = mask.bbox();

    // Avoid the slur/tie line being partially masked (i.e. it should always be either fully "cut",
    // or not masked at all, so we extend the mask vertically to encompass the slur/tie's full height):
    mask_bbox.adjust(0.0, slur_tie_shape.top() - mask.top(), 0.0, slur_tie_shape.bottom() - mask.bottom());
    mask_bbox.pad(mask_padding);

    slur_tie_segment->mutldata()->set_mask(mask_bbox);
}

Shape MaskLayout::create_filtered_item_shape(const Shape &overlying_item_shape, const Shape &masked_item_shape,
                                             const double collision_padding) {
    return MaskLayout::create_filtered_item_shape(overlying_item_shape, masked_item_shape, collision_padding, collision_padding);
}

Shape MaskLayout::create_filtered_item_shape(const Shape &overlying_item_shape, const Shape &masked_item_shape,
                                             const double horizontal_collision_padding, const double vertical_collision_padding) {
    Shape filtered_item_shape;
    filtered_item_shape.elements().reserve(overlying_item_shape.elements().size());
    for (const ShapeElement &el : overlying_item_shape.elements()) {
        Shape padded = el.adjusted(-horizontal_collision_padding, -vertical_collision_padding, horizontal_collision_padding,
                                   vertical_collision_padding);
        if (masked_item_shape.intersects(padded)) {
            filtered_item_shape.add(el);
        }
    }
    return filtered_item_shape;
}
