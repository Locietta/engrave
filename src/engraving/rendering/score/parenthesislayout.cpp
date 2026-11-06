/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "log.h"
#include "dom/chord.h"
#include "dom/harmony.h"
#include "dom/ledgerline.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/parenthesis.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/slurtie.h"
#include "dom/staff.h"
#include "dom/timesig.h"
#include "horizontalspacing.h"
#include "parenthesislayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

static constexpr Spatium max_mid_thickness = 0.2_sp;

void ParenthesisLayout::layout_parentheses(const EngravingItem *parent, const LayoutContext &ctx) {
    // Layout parentheses surrounding an engraving item. Handle padding and placement
    Parenthesis *left_paren = parent->left_paren();
    Parenthesis *right_paren = parent->right_paren();

    if (!(left_paren || right_paren)) {
        return;
    }
    if (left_paren) {
        layout_parenthesis(left_paren, left_paren->mutldata(), ctx);
    }

    if (right_paren) {
        layout_parenthesis(right_paren, right_paren->mutldata(), ctx);
    }

    bool item_add_to_skyline = parent->autoplace() && !parent->ldata()->is_skip_draw();
    Shape dummy_item_shape = get_parent_shape(parent);

    layout_parentheses(left_paren, right_paren, dummy_item_shape, item_add_to_skyline, ctx);
}

void ParenthesisLayout::layout_chord_parentheses(const Chord *chord, const LayoutContext &ctx) {
    for (const NoteParenthesisInfo *paren_notes_info : chord->note_parentheses()) {
        Parenthesis *left_paren = paren_notes_info->left_paren();
        Parenthesis *right_paren = paren_notes_info->right_paren();

        if (!(left_paren || right_paren)) {
            return;
        }
        if (left_paren) {
            layout_parenthesis(left_paren, left_paren->mutldata(), ctx);
        }

        if (right_paren) {
            layout_parenthesis(right_paren, right_paren->mutldata(), ctx);
        }

        bool item_add_to_skyline = chord->autoplace() && !chord->ldata()->is_skip_draw();
        Shape dummy_item_shape = get_parent_shape(chord);

        layout_parentheses(left_paren, right_paren, dummy_item_shape, item_add_to_skyline, ctx);
    }
}

void ParenthesisLayout::layout_parentheses(Parenthesis *left_paren, Parenthesis *right_paren, Shape &dummy_item_shape,
                                           bool item_add_to_skyline, const LayoutContext &ctx) {
    UNUSED(ctx);
    if (!left_paren || !right_paren) {
        // 1 parenthesis
        Parenthesis *paren = left_paren ? left_paren : right_paren;
        Parenthesis::LayoutData *ldata = paren->mutldata();
        const bool left_bracket = paren->direction() == DirectionH::LEFT;
        double min_dist = 0.0;
        if (!left_bracket && item_add_to_skyline) {
            // Space against existing item shape
            min_dist =
                HorizontalSpacing::min_horizontal_distance(dummy_item_shape, paren->shape().translated(ldata->pos()), paren->spatium());
        } else if (item_add_to_skyline) {
            // Space following item shape against this
            min_dist =
                -HorizontalSpacing::min_horizontal_distance(paren->shape().translated(ldata->pos()), dummy_item_shape, paren->spatium());
        }
        paren->mutldata()->move_x(min_dist);

        return;
    }

    // 2 parentheses
    assert(left_paren && right_paren);

    if (!item_add_to_skyline) {
        return;
    }

    Parenthesis::LayoutData *left_ldata = left_paren->mutldata();
    Parenthesis::LayoutData *right_ldata = right_paren->mutldata();

    const double item_left_x = dummy_item_shape.bbox().x();
    const double parent_width = dummy_item_shape.bbox().width();
    const double item_right_x = item_left_x + parent_width;

    const double left_paren_padding = HorizontalSpacing::min_horizontal_distance(left_paren->shape().translated(left_ldata->pos()),
                                                                                 dummy_item_shape, left_paren->spatium());
    left_ldata->move_x(-left_paren_padding);
    dummy_item_shape.add(left_paren->shape().translate(left_ldata->pos() + left_paren->staff_offset()));

    const double right_paren_padding = HorizontalSpacing::min_horizontal_distance(
        dummy_item_shape, right_paren->shape().translated(right_ldata->pos()), right_paren->spatium());
    right_ldata->move_x(right_paren_padding);

    // If the right parenthesis has been padded against the left parenthesis, this means the parenthesis -> parenthesis padding distance
    // is larger than the width of the item the parentheses surrounds. In this case, the result is visually unbalanced.  Move both parens
    // to the left (relative to the segment) in order to centre the item: (b  ) -> ( b )
    const double item_width = parent_width;
    double paren_padding = compute_internal_parenthesis_padding(left_paren, right_paren);

    if (item_width >= paren_padding) {
        return;
    }

    // Move parentheses to place item in the middle
    const double left_paren_x = left_ldata->pos().x() + left_paren->ldata()->bbox().x() + left_paren->ldata()->mid_point_thickness;
    const double right_paren_x =
        right_ldata->pos().x() + right_paren->ldata()->bbox().x() + right_paren->width() - right_paren->ldata()->mid_point_thickness;

    const double left_paren_to_item = item_left_x - left_paren_x;
    const double item_to_right_paren = right_paren_x - item_right_x;
    const double paren_to_item_dist = (left_paren_to_item + item_to_right_paren) / 2;

    left_ldata->move_x(-(paren_to_item_dist - left_paren_to_item));
    right_ldata->move_x(-(item_to_right_paren - paren_to_item_dist));
}

void ParenthesisLayout::layout_parenthesis(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx) {
    set_layout_values(item, ldata, ctx);

    if (ldata->sym_id == SymId::NO_SYM) {
        create_path_and_shape(item, ldata);
    } else {
        create_smufl_shape(item, ldata);
    }
}

bool ParenthesisLayout::is_internal_paren_padding(const EngravingItem *item1, const EngravingItem *item2) {
    const Chord *chord1 = to_chord(item1->find_ancestor(ElementType::CHORD));
    const Chord *chord2 = to_chord(item2->find_ancestor(ElementType::CHORD));
    const EngravingObject *parent1 = item1->ownership_parent();
    const EngravingObject *parent2 = item2->ownership_parent();

    bool internal_padding = (item1->is_parenthesis() && parent1 == item2) || (item2->is_parenthesis() && parent2 == item1) ||
                            parent1 == parent2 || (chord1 && chord2 && chord1 == chord2);

    return internal_padding;
}

double ParenthesisLayout::compute_external_parenthesis_padding(const EngravingItem *item1, const EngravingItem *item2) {
    EngravingItem *parent = item1->is_parenthesis() ? item1->ownership_parent_item() : item2->ownership_parent_item();

    ElementType type1 = item1->type();
    ElementType type2 = item2->type();

    const ParenPaddingTable &padding_table = parent->score()->padding_tables().parenthesis_padding_table_for(parent);

    double padding = padding_table.padding(type1, type2);

    double scaling = (item1->mag() + item2->mag()) / 2;
    padding *= scaling;
    return padding;
}

double ParenthesisLayout::compute_parenthesis_padding(const EngravingItem *item1, const EngravingItem *item2) {
    if (is_internal_paren_padding(item1, item2)) {
        return compute_internal_parenthesis_padding(item1, item2);
    }

    return compute_external_parenthesis_padding(item1, item2);
}

double ParenthesisLayout::compute_internal_parenthesis_padding(const EngravingItem *item1, const EngravingItem *item2) {
    bool paren_first = item1->is_parenthesis();
    const EngravingItem *other = item1->is_parenthesis() ? item2 : item1;
    const double spatium = item1->style().spatium();
    double padding = 0.0;

    switch (other->type()) {
        case ElementType::NOTE:
            if (item1->staff_type() && item1->staff_type()->is_tab_staff()) {
                padding = item1->style().style_s(Sid::TAB_FRET_PADDING).val() * spatium;
            } else {
                padding = 0.3 * spatium;
            }
            break;
        case ElementType::ARTICULATION:
        case ElementType::ACCIDENTAL:
        case ElementType::STEM: padding = 0.3 * spatium; break;
        case ElementType::NOTEDOT: padding = 0.15 * spatium; break;
        case ElementType::LEDGER_LINE: padding = 0.3 * spatium; break;
        case ElementType::HOOK: padding = 0.3 * spatium; break;
        case ElementType::KEYSIG: padding = (paren_first ? 0.35 : 0.25) * spatium; break;
        case ElementType::TIMESIG: padding = (paren_first ? 0.2 : 0.25) * spatium; break;
        case ElementType::CLEF: padding = (paren_first ? 0.2 : 0.25) * spatium; break;
        case ElementType::HARMONY: padding = 0.2 * spatium; break;
        case ElementType::PARENTHESIS: padding = spatium; break;
        default: padding = 0.1 * spatium; break;
    }

    double scaling = (item1->mag() + item2->mag()) / 2;
    padding *= scaling;
    return padding;
}

void ParenthesisLayout::create_path_and_shape(Parenthesis *item, Parenthesis::LayoutData *ldata) {
    const double spatium = item->spatium();
    const bool left_bracket = item->direction() == DirectionH::LEFT;

    const double start_y = ldata->start_y;
    const double height = ldata->height;
    const double x_pos = ldata->pos().x();
    const double mag = ldata->mag();
    const double max_mid_point_thickness = ::max_mid_thickness.to_absolute(spatium);
    const double mid_point_thickness = std::min(ldata->mid_point_thickness.value(), max_mid_point_thickness);

    const double height_in_spatium = height / spatium;
    const double shoulder_y_offset = 0.2 * height;

    if (std::isinf(height)) {
        LOGE() << "Error: parenthesis height is infinite";
        return;
    }

    // Control width of parentheses. We don't want tall parens to be too wide, nor do we want parens at a small scale to lose their curve too much
    double shoulder_x =
        !muse::real_is_null(ldata->shoulder_width()) ? ldata->shoulder_width() : 0.2 * std::pow(height, 0.95) * std::pow(mag, 0.1);

    const double min_shoulder_x = 0.25 * spatium * ldata->intrinsic_mag();
    shoulder_x = std::max(shoulder_x, min_shoulder_x);

    PointF start = PointF(x_pos, start_y);
    const PointF end = PointF(x_pos, start.y() + height);
    const PointF end_normalised = end - start;

    const int direction = left_bracket ? -1 : 1;
    const double shoulder_for_x = direction * shoulder_x + mid_point_thickness * direction;
    const double shoulder_back_x = direction * shoulder_x + mid_point_thickness * direction * -1;

    const PointF bezier1for = PointF(shoulder_for_x, shoulder_y_offset);
    const PointF bezier2for = PointF(shoulder_for_x, end_normalised.y() - shoulder_y_offset);
    const PointF bezier1back = PointF(shoulder_back_x, end_normalised.y() - shoulder_y_offset);
    const PointF bezier2back = PointF(shoulder_back_x, shoulder_y_offset);

    PainterPath path = PainterPath();
    path.move_to(PointF());
    path.cubic_to(bezier1for, bezier2for, end_normalised);
    path.cubic_to(bezier1back, bezier2back, PointF());

    ldata->path = path;

    // Fill shape
    Shape shape(Shape::Type::COMPOSITE);

    PointF start_point = PointF();
    double mid_thickness = 2 * mid_point_thickness;
    int nb_shapes = round(2.0 * height_in_spatium);
    nb_shapes = std::clamp(nb_shapes, 3, 10);
    PointF bezier1mid = bezier1for - PointF(mid_point_thickness * direction, 0.0);
    PointF bezier2mid = bezier2for - PointF(mid_point_thickness * direction, 0.0);
    const CubicBezier b(start_point, bezier1mid, bezier2mid, end_normalised);
    for (int i = 1; i <= nb_shapes; i++) {
        double percent = pow(sin(0.5 * M_PI * (double(i) / double(nb_shapes))), 2);
        const PointF point = b.point_at_percent(percent);
        RectF re = RectF(start_point, point).normalized();
        double approx_thickness_at_percent = (1 - 2 * std::abs(0.5 - percent)) * mid_thickness;
        if (re.width() < approx_thickness_at_percent) {
            double adjust = (approx_thickness_at_percent - re.width()) * .5;
            re.adjust(-adjust, 0.0, adjust, 0.0);
        }
        shape.add(re, item);
        start_point = point;
    }

    ldata->set_shape(shape);

    item->set_pos(start);
}

void ParenthesisLayout::create_smufl_shape(Parenthesis *item, Parenthesis::LayoutData *ldata) {
    Shape bbox = Shape(item->sym_bbox(ldata->sym_id), item);
    double scale = item->ldata()->sym_scale;
    bbox.scale(SizeF(scale, scale));
    ldata->set_shape(bbox);
}

void ParenthesisLayout::set_layout_values(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx) {
    if (!item->ownership_parent()) {
        return;
    }

    ldata->set_pos(PointF());
    ldata->reset();
    ldata->path.reset();
    ldata->sym_id.reset();

    // Set ldata values based on parent
    switch (item->ownership_parent()->type()) {
        case ElementType::CHORD: set_chord_values(item, ldata); break;
        // height & startY are set in MeasureLayout for clef, timesig and keysig
        // TODO: create generic way of matching height & startY between parentheses on different items
        case ElementType::CLEF: set_clef_values(item, ldata); break;
        case ElementType::TIMESIG: set_time_sig_values(item, ldata, ctx); break;
        case ElementType::KEYSIG: break;
        case ElementType::HARMONY: set_harmony_values(item, ldata, ctx); break;
        default: set_default_values(item, ldata); break;
    }
}

void ParenthesisLayout::set_clef_values(Parenthesis *item, Parenthesis::LayoutData *ldata) {
    const EngravingItem *parent = item->ownership_parent_item();
    const Fraction tick = parent->tick();
    const Measure *measure = item->find_measure();

    double offset = -parent->pos().y();

    if (!measure || tick != measure->end_tick()) {
        ldata->start_y.mut_value() += offset;
        return;
    }

    const double spatium = item->spatium();
    const Staff *staff = parent->staff();
    const Fraction tick_prev = tick - Fraction::eps();
    const StaffType *st = staff->staff_type(tick);
    const StaffType *st_prev = !tick_prev.negative() ? item->staff()->staff_type(tick_prev) : nullptr;

    offset += (st->yoffset().val() - (st_prev ? st_prev->yoffset().val() : 0)) * spatium;
    ldata->start_y.mut_value() += offset;
}

void ParenthesisLayout::set_time_sig_values(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx) {
    const TimeSig *parent_ts = to_time_sig(item->ownership_parent());
    if (ctx.conf().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() == TimeSigPlacement::NORMAL) {
        return;
    }

    double y_offset = -parent_ts->pos().y();
    ldata->start_y.mut_value() += y_offset;
}

void ParenthesisLayout::set_chord_values(Parenthesis *item, Parenthesis::LayoutData *ldata) {
    Chord *chord = to_chord(item->ownership_parent());

    ldata->set_mag(chord->mag());
    ldata->intrinsic_mag = chord->intrinsic_mag(); // Scaling information not linked to staff size. item->spatium is NOT scaled by this

    Shape notes_shape;

    const NoteParenthesisInfo *paren_info = chord->find_note_parenthesis_info(item);
    IF_ASSERT_FAILED(paren_info) {
        LOGD() << "No parenthesis info found for this chord";
        return;
    }

    const std::vector<Note *> &notes = paren_info->notes();

    assert(!notes.empty());

    for (const Note *note : notes) {
        notes_shape.add(get_note_shape(note, item).translated(note->pos()));
    }

    const StaffType *st = chord->staff_type();
    const bool is_tab = st->is_tab_staff();

    auto calculate_mid_point_thickness = [item, ldata, is_tab]() -> double {
        const double sp = item->spatium();
        const double sp0 = item->default_spatium();

        IF_ASSERT_FAILED(sp > 0.0 && sp0 > 0.0) {
            LOGE() << "spatium or default spatium is 0";
            return 0.0;
        }

        const double exponent = is_tab ? 0.36 : 0.33;
        const double exponent_complement = 1.0 - exponent;

        const double normalized_height = ldata->height / sp;

        // Scale factor that keeps the normalized formula compatible with the
        // original formula at the default spatium.
        const double scale_normalization = sp / std::pow(sp0, exponent_complement);
        const double normalized_thickness = std::pow(normalized_height, exponent) * scale_normalization * ldata->intrinsic_mag();

        return normalized_thickness;
    };

    if (is_tab) {
        ldata->start_y = notes_shape.top();
        ldata->height = notes_shape.bbox().height();
    } else {
        ldata->start_y = notes_shape.top() - 0.25 * item->spatium() * ldata->intrinsic_mag();
        ldata->height = notes_shape.bbox().height() + 0.5 * item->spatium() * ldata->intrinsic_mag();
    }
    ldata->mid_point_thickness.set_value(calculate_mid_point_thickness());
    ldata->end_point_thickness.set_value(0.05);
}

void ParenthesisLayout::set_harmony_values(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx) {
    const double spatium = item->spatium();
    Harmony *parent = to_harmony(item->ownership_parent());
    RectF bbox = parent->ldata()->bbox();

    double end_point_thickness = 0.03;
    double extension = 0.1 * spatium * (parent->size() / 10.0);
    double bottom = parent->base_line() + extension;

    double default_cap_height = muse::draw::FontMetrics::cap_height(parent->font());

    double top_cap_height = DBL_MAX;
    const TextSegment *root_text_seg = nullptr;
    if (!parent->ldata()->render_item_list.value().empty()) {
        for (const HarmonyRenderItem *render_item : parent->ldata()->render_item_list.value()) {
            if (const TextSegment *ts = dynamic_cast<const TextSegment *>(render_item)) {
                top_cap_height = std::min(top_cap_height, ts->pos().y() - ts->cap_height());

                root_text_seg = !root_text_seg ? ts : root_text_seg;
            }
        }
    } else {
        top_cap_height = default_cap_height;
    }

    double top = top_cap_height - extension;
    double height = bottom - top;

    if (ctx.conf().style_b(Sid::HARMONY_PAREN_USE_SMUFL_SYM)) {
        ldata->sym_id = item->direction() == DirectionH::LEFT ? SymId::CSYM_PARENS_LEFT_TALL : SymId::CSYM_PARENS_RIGHT_TALL;
        double sym_height = item->sym_height(ldata->sym_id);
        ldata->sym_scale = height / sym_height;

        return;
    }

    double root_cap_height = root_text_seg ? root_text_seg->cap_height() : default_cap_height;
    double scale = (height - 2 * extension) / root_cap_height;

    ldata->set_mag(parent->mag());
    ldata->start_y = top;
    ldata->height = height;
    static constexpr double height_to_width_ratio = 20;
    ldata->mid_point_thickness.set_value(ldata->height / height_to_width_ratio * ldata->mag() * 1 / std::sqrt(scale));
    ldata->end_point_thickness.set_value(end_point_thickness);

    double shoulder = 0.2 * ldata->height * std::pow(ldata->mag(), 0.1) * 1 / std::sqrt(scale);
    ldata->shoulder_width = shoulder;

    const double padding_value = spatium * 0.2;
    ldata->set_pos_x(item->direction() == DirectionH::RIGHT ? bbox.right() + padding_value : bbox.left() - padding_value);
}

void ParenthesisLayout::set_default_values(Parenthesis *item, Parenthesis::LayoutData *ldata) {
    const double spatium = item->spatium();
    EngravingItem *parent = item->layout_parent();
    RectF bbox = parent->ldata()->bbox();

    ldata->set_mag(parent->mag());
    ldata->start_y = bbox.top() - 0.25 * spatium;
    ldata->height = bbox.height() + 0.5 * spatium;
    ldata->mid_point_thickness.set_value(ldata->height / 60 * ldata->mag()); // 0.1sp for a height of 6sp
    const double padding_value = spatium * 0.2;
    ldata->set_pos_x(item->direction() == DirectionH::RIGHT ? bbox.right() + padding_value : bbox.left() - padding_value);
}

Shape ParenthesisLayout::get_parent_shape(const EngravingItem *parent) {
    Shape parent_shape = parent->shape();

    bool is_chord = parent->is_chord();

    parent_shape.remove_if([is_chord](ShapeElement &s) {
        return !s.item() || s.item()->is_parenthesis() || (s.item()->is_laissez_vib_segment() && is_chord) ||
               (s.item()->is_hook() && is_chord) || (s.item()->is_stem() && is_chord) || (s.item()->is_arpeggio() && is_chord) ||
               (s.item()->is_chord_bracket() && is_chord);
    });

    return parent_shape;
}

Shape ParenthesisLayout::get_note_shape(const Note *note, Parenthesis *paren) {
    IF_ASSERT_FAILED(note && paren) { return Shape(); }
    Shape note_shape = note->shape();
    note_shape.remove_if([paren](ShapeElement &s) {
        return s.item() == paren || s.item()->is_bend() || s.item()->is_parenthesis() || s.item()->is_accidental() ||
               s.item()->is_note_dot() || s.item()->is_laissez_vib_segment();
    });

    return note_shape;
}
