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

#include "tappinglayout.h"

#include "dom/chord.h"
#include "dom/slur.h"
#include "dom/staff.h"
#include "dom/system.h"
#include "slurtielayout.h"
#include "textlayout.h"
#include "tlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void TappingLayout::layout_tapping(Tapping *item, Tapping::LayoutData *ldata, LayoutContext &ctx) {
    IF_ASSERT_FAILED(item->hand() != TappingHand::INVALID) { return; }

    const MStyle &style = item->style();
    bool tab_staff = item->staff_type()->is_tab_staff();

    if (item->hand() == TappingHand::LEFT) {
        layout_left_hand_tapping(item, ldata, style, tab_staff, ctx);
    } else {
        layout_right_hand_tapping(item, ldata, style, tab_staff, ctx);
    }

    if (ldata->sym_id != SymId::NO_SYM) {
        ldata->set_shape(Shape(item->sym_bbox(ldata->sym_id), item));
    } else if (TappingText *text = item->text()) {
        RectF text_bbox = text->ldata()->bbox();
        text->set_pos(PointF(-text_bbox.x(), 0.0));
        ldata->set_shape(Shape(text_bbox.translated(text->pos()), item));
    }

    if (tab_staff) {
        // Slightly increase vertical padding on TAB staves
        item->set_min_distance(item->staff()->line_distance(item->tick()) * style.style_s(Sid::ARTICULATION_MIN_DISTANCE));
    }
}

void TappingLayout::layout_left_hand_tapping(Tapping *item, Tapping::LayoutData *ldata, const MStyle &style, bool tab_staff,
                                             LayoutContext &ctx) {
    update_half_slurs(item, style, tab_staff, ctx);

    LHTappingSymbol lh_sym = tab_staff ? style.style_v(Sid::LH_TAPPING_SYMBOL_TAB).value<LHTappingSymbol>() :
                                         style.style_v(Sid::LH_TAPPING_SYMBOL_NORMAL_STAVE).value<LHTappingSymbol>();

    bool dont_show_symbols = item->lh_show_items() == LHTappingShowItems::HALF_SLUR;

    if (lh_sym != LHTappingSymbol::DOT || dont_show_symbols) {
        ldata->sym_id = SymId::NO_SYM;
    }
    if (lh_sym != LHTappingSymbol::CIRCLED_T || dont_show_symbols) {
        delete item->text();
        item->set_text(nullptr);
    }

    if (dont_show_symbols) {
        return;
    }

    if (lh_sym == LHTappingSymbol::DOT) {
        ldata->sym_id = SymId::WIND_CLOSED_HOLE;
        ldata->set_mag(0.5);
    } else if (lh_sym == LHTappingSymbol::CIRCLED_T) {
        ldata->set_mag(1.0);
        TappingText *text = item->text();
        if (!text) {
            text = new TappingText(item);
        }
        item->set_text(text);
        text->set_track(item->track());
        text->set_xml_text("T");
        text->set_frame_type(FrameType::CIRCLE);
        text->set_align(Align(AlignH::LEFT, AlignV::BASELINE));
        TextLayout::layout_base_text_base(text, ctx);
        // Move the circle up very slightly to look better centered on the T
        text->mutldata()->frame.translate(0.0, -0.02 * text->height());
    }
}

void TappingLayout::update_half_slurs(Tapping *item, const MStyle &style, bool tab_staff, LayoutContext &ctx) {
    Chord *chord = item->parent() && item->parent()->is_chord() ? to_chord(item->parent()) : nullptr;
    if (!chord) {
        return;
    }

    bool show_half_slur_above = item->lh_show_items() != LHTappingShowItems::SYMBOL;
    if (show_half_slur_above) {
        TappingHalfSlur *half_slur_above = item->half_slur_above();
        if (!half_slur_above) {
            half_slur_above = new TappingHalfSlur(item);
            item->set_half_slur_above(half_slur_above);
            half_slur_above->set_selected(item->selected());
        }

        half_slur_above->set_ownership_parent(item);
        half_slur_above->set_is_half_slur_above(true);
        half_slur_above->set_track(item->track());
        half_slur_above->set_tick(item->tick());
        half_slur_above->set_ticks(Fraction(0, 1));
        half_slur_above->set_start_element(chord);
        half_slur_above->set_end_element(chord);
        if (tab_staff) {
            half_slur_above->set_slur_direction(DirectionV::UP);
        }

        layout_half_slur(item, half_slur_above, ctx);
    } else if (item->half_slur_above()) {
        delete item->half_slur_above();
        item->set_half_slur_above(nullptr);
    }

    bool show_half_slur_below = tab_staff && show_half_slur_above && style.style_b(Sid::LH_TAPPING_SLUR_TOP_AND_BOTTOM_NOTE_ON_TAB) &&
                                to_chord(item->parent())->notes().size() > 1;
    if (show_half_slur_below) {
        TappingHalfSlur *half_slur_below = item->half_slur_below();
        if (!half_slur_below) {
            half_slur_below = new TappingHalfSlur(item);
            item->set_half_slur_below(half_slur_below);
        }

        half_slur_below->set_ownership_parent(item);
        half_slur_below->set_is_half_slur_above(false);
        half_slur_below->set_track(item->track());
        half_slur_below->set_tick(item->tick());
        half_slur_below->set_ticks(Fraction(0, 1));
        half_slur_below->set_start_element(chord);
        half_slur_below->set_end_element(chord);
        half_slur_below->set_slur_direction(DirectionV::DOWN);

        layout_half_slur(item, half_slur_below, ctx);
    } else if (item->half_slur_below()) {
        delete item->half_slur_below();
        item->set_half_slur_below(nullptr);
    }
}

void TappingLayout::layout_half_slur(Tapping *item, TappingHalfSlur *slur, LayoutContext &ctx) {
    System *system = to_system(item->find_ancestor(ElementType::SYSTEM));
    IF_ASSERT_FAILED(system) { return; }

    for (SpannerSegment *seg : slur->spanner_segments()) {
        seg->move_to_system(nullptr);
    }

    Skyline &skyline = system->staff(item->staff_idx())->skyline();
    TappingHalfSlurSegment *slur_seg = to_tapping_half_slur_segment(SlurTieLayout::layout_system(slur, system, ctx));

    Shape seg_shape = slur_seg->shape();

    RectF mask(seg_shape.bbox());
    mask.set_width(0.5 * mask.width());
    const double mask_pad = 0.25 * item->spatium();
    mask.adjust(-mask_pad, -mask_pad, 0.0, mask_pad);
    slur_seg->mutldata()->set_mask(mask);

    Shape masked_shape;
    double rounding_margin = 0.05 * item->spatium(); // otherwise rounding errors may affect the threshold
    for (ShapeElement &el : seg_shape.elements()) {
        if (el.left() > mask.right() - rounding_margin) {
            masked_shape.add(el);
        }
    }
    slur_seg->mutldata()->set_shape(masked_shape);

    skyline.add(masked_shape.translated(slur_seg->pos()));
}

void TappingLayout::layout_right_hand_tapping(Tapping *item, Tapping::LayoutData *ldata, const MStyle &style, bool tab_staff,
                                              LayoutContext &ctx) {
    RHTappingSymbol rh_sym = tab_staff ? style.style_v(Sid::RH_TAPPING_SYMBOL_TAB).value<RHTappingSymbol>() :
                                         style.style_v(Sid::RH_TAPPING_SYMBOL_NORMAL_STAVE).value<RHTappingSymbol>();

    if (rh_sym != RHTappingSymbol::PLUS) {
        ldata->sym_id = SymId::NO_SYM;
    }
    if (rh_sym != RHTappingSymbol::T) {
        delete item->text();
        item->set_text(nullptr);
    }

    if (rh_sym == RHTappingSymbol::PLUS) {
        ldata->sym_id = SymId::PLUCKED_LEFT_HAND_PIZZICATO;
    } else if (rh_sym == RHTappingSymbol::T) {
        TappingText *text = item->text();
        if (!text) {
            text = new TappingText(item);
        }
        item->set_text(text);
        text->set_track(item->track());
        text->set_xml_text("T");
        text->set_align(Align(AlignH::LEFT, AlignV::BASELINE));
        TextLayout::layout_base_text_base(text, ctx);
    }
}
