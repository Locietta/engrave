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

#include "harmonylayout.h"

#include "dom/factory.h"
#include "dom/fret.h"
#include "dom/harmony.h"
#include "dom/staff.h"
#include "draw/fontmetrics.h"
#include "log.h"
#include "parenthesislayout.h"
#include "textlayout.h"
#include "tlayout.h"

using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void HarmonyLayout::layout_harmony(Harmony *item, Harmony::LayoutData *ldata, const LayoutContext &ctx) {
    if (!item->ownership_parent()) {
        ldata->set_pos(0.0, 0.0);
        const_cast<Harmony *>(item)->set_offset(0.0, 0.0);
    }

    if (!item->cursor()->editing()) {
        render(item, ldata, ctx);
    }

    if (ldata->layout_invalid) {
        item->create_blocks(ldata);
    }

    if (ldata->blocks.empty()) {
        ldata->blocks.push_back(TextBlock());
    }

    PointF position_point = calculate_bounding_rect(item, ldata, ctx);

    if (item->is_polychord()) {
        for (LineF &line : ldata->polychord_divider_lines.mut_value()) {
            line.set_p1(PointF(ldata->bbox().left(), line.y1()));
            line.set_p2(PointF(ldata->bbox().right(), line.y2()));
        }
    }

    if (item->has_frame()) {
        item->layout_frame(ldata);
    }

    ldata->set_pos(position_point);

    if (!item->cursor()->editing() && !ldata->render_item_list.value().empty()) {
        ParenthesisLayout::layout_parentheses(item, ctx);
    }
}

PointF HarmonyLayout::calculate_bounding_rect(const Harmony *item, Harmony::LayoutData *ldata, const LayoutContext &ctx) {
    const double ypos = (item->place_below() && item->staff()) ? item->staff()->staff_height(item->tick()) : 0.0;
    const FretDiagram *fd =
        (item->ownership_parent() && item->ownership_parent()->is_fret_diagram()) ? to_fret_diagram(item->ownership_parent()) : nullptr;
    const bool align_to_fret_diagram = fd && fd->visible();

    const double standard_note_width = item->sym_width(SymId::NOTEHEAD_BLACK);

    double new_pos_x = 0.0;
    double new_pos_y = 0.0;

    if (item->ldata()->render_item_list().empty()) {
        TextLayout::layout_base_text_base1(item, ldata);

        if (align_to_fret_diagram) {
            new_pos_y = ldata->pos().y();
        } else {
            new_pos_y = ypos - ((item->align() == AlignV::BOTTOM) ? -ldata->bbox().height() : 0.0);
        }
    } else {
        layout_modifier_parentheses(item, ctx);
        RectF bb;
        RectF h_align_box;
        for (HarmonyRenderItem *render_item : item->ldata()->render_item_list()) {
            RectF ts_bbox = render_item->tight_bounding_rect().translated(render_item->x(), render_item->y());
            bb.unite(ts_bbox);

            if (render_item->align()) {
                h_align_box.unite(ts_bbox);
            }
        }

        double xx = 0.0;
        if (align_to_fret_diagram) {
            switch (ctx.conf().style_v(Sid::CHORD_ALIGNMENT_TO_FRETBOARD).value<AlignH>()) {
                case AlignH::LEFT:
                case AlignH::JUSTIFY: xx = -h_align_box.left(); break;
                case AlignH::HCENTER: xx = -(h_align_box.center().x()); break;
                case AlignH::RIGHT: xx = -h_align_box.right(); break;
            }
        } else {
            switch (item->position()) {
                case AlignH::LEFT:
                case AlignH::JUSTIFY: xx = -h_align_box.left(); break;
                case AlignH::HCENTER: xx = -(h_align_box.center().x()); break;
                case AlignH::RIGHT: xx = -h_align_box.right(); break;
            }
        }

        double yy = -bb.y(); // Align::TOP
        if (item->align() == AlignV::VCENTER) {
            yy = -bb.y() / 2.0;
        } else if (item->align() == AlignV::BASELINE) {
            yy = item->base_line();
        } else if (item->align() == AlignV::BOTTOM) {
            yy = -bb.height() - bb.y();
        }

        if (align_to_fret_diagram) {
            double nut_line_width = fd->ldata()->nut_line_width;
            new_pos_y = yy - ctx.conf().style_absolute(Sid::HARMONY_FRET_DIST) - nut_line_width;
        } else {
            new_pos_y = ypos;
        }

        for (HarmonyRenderItem *render_item : item->ldata()->render_item_list()) {
            render_item->set_offset(PointF(xx, yy));
        }

        ldata->polychord_divider_offset = yy;

        ldata->set_bbox(bb.translated(xx, yy));
        ldata->harmony_height = ldata->bbox().height();
    }

    if (align_to_fret_diagram) {
        switch (ctx.conf().style_v(Sid::CHORD_ALIGNMENT_TO_FRETBOARD).value<AlignH>()) {
            case AlignH::LEFT:
            case AlignH::JUSTIFY: new_pos_x = 0.0; break;
            case AlignH::HCENTER: new_pos_x = 0.5 * fd->main_width(); break;
            case AlignH::RIGHT: new_pos_x = fd->main_width(); break;
        }
    } else {
        switch (item->position()) {
            case AlignH::LEFT:
            case AlignH::JUSTIFY: new_pos_x = 0.0; break;
            case AlignH::HCENTER: new_pos_x = standard_note_width * 0.5; break;
            case AlignH::RIGHT: new_pos_x = standard_note_width; break;
        }
    }

    if (fd && !fd->visible()) {
        // Translate to base position around note
        new_pos_x -= fd->pos().x();
    }

    PointF new_pos = PointF(new_pos_x, new_pos_y);

    if (!align_to_fret_diagram) {
        PointF default_pos = item->default_pos();
        new_pos += default_pos;
    }

    return new_pos;
}

void HarmonyLayout::layout_modifier_parentheses(const Harmony *item, const LayoutContext &ctx) {
    const double spatium = item->spatium();
    const std::vector<HarmonyRenderItem *> &item_list = item->ldata()->render_item_list();
    const double smufl_parens = ctx.conf().style_b(Sid::HARMONY_PAREN_USE_SMUFL_SYM);
    // Layout parentheses
    std::vector<ChordSymbolParen *> opening_paren_stack;
    double last_text_seg_height = 0.0;
    double last_text_seg_top = 0.0;
    double root_ref_height = 0.0;
    double root_cap_height = 0.0;
    double paren_extension = 0.1 * spatium * (item->size() / 10.0);
    for (HarmonyRenderItem *render_item : item_list) {
        if (ChordSymbolParen *cur_paren = dynamic_cast<ChordSymbolParen *>(render_item)) {
            if (cur_paren->paren_item->direction() == DirectionH::LEFT) {
                // Opening paren
                opening_paren_stack.push_back(cur_paren);
                if (ctx.conf().style_b(Sid::HARMONY_PAREN_USE_SMUFL_SYM)) {
                    cur_paren->paren_item->mutldata()->sym_id = SymId::CSYM_PARENS_LEFT_TALL;
                }
            } else {
                // Closing paren
                ChordSymbolParen *opening_paren = opening_paren_stack.empty() ? nullptr : opening_paren_stack.back();
                if (!opening_paren) {
                    continue;
                }
                opening_paren_stack.pop_back();
                cur_paren->top = opening_paren->top;
                cur_paren->bottom = opening_paren->bottom;

                // Layout parenthesis pair
                double start_y = opening_paren->top - paren_extension;
                double height = (opening_paren->bottom - opening_paren->top) + 2 * paren_extension;
                if (std::isinf(height)) {
                    height = last_text_seg_height;
                }
                if (muse::real_is_equal(DBL_MAX, start_y)) {
                    start_y = last_text_seg_top;
                }

                if (smufl_parens) {
                    double sym_height = item->sym_height(SymId::CSYM_PARENS_RIGHT_TALL);
                    double scale = height / sym_height;
                    cur_paren->paren_item->mutldata()->sym_id = SymId::CSYM_PARENS_RIGHT_TALL;
                    cur_paren->paren_item->mutldata()->sym_scale = scale;
                    opening_paren->paren_item->mutldata()->sym_scale = scale;

                    cur_paren->sety(start_y + height);
                    opening_paren->sety(start_y + height);

                    ParenthesisLayout::create_smufl_shape(opening_paren->paren_item, opening_paren->paren_item->mutldata());
                    ParenthesisLayout::create_smufl_shape(cur_paren->paren_item, cur_paren->paren_item->mutldata());
                } else {
                    const double scale = (height - 2 * paren_extension) / root_cap_height;
                    const double mag = opening_paren->paren_item->ldata()->mag();
                    static constexpr double height_to_width_ratio = 20;
                    const double mid_point_thickness = height / height_to_width_ratio * mag * 1 / std::sqrt(scale);
                    const double end_point_thickness = 0.03;
                    const double shoulder = 0.2 * height * std::pow(mag, 0.1) * 1 / std::sqrt(scale);

                    opening_paren->paren_item->mutldata()->start_y = start_y;
                    opening_paren->sety(start_y);
                    opening_paren->paren_item->mutldata()->height = height;
                    opening_paren->paren_item->mutldata()->mid_point_thickness.set_value(mid_point_thickness);
                    opening_paren->paren_item->mutldata()->end_point_thickness.set_value(end_point_thickness);
                    opening_paren->paren_item->mutldata()->shoulder_width = shoulder;

                    cur_paren->paren_item->mutldata()->start_y = start_y;
                    cur_paren->sety(start_y);
                    cur_paren->paren_item->mutldata()->height = height;
                    cur_paren->paren_item->mutldata()->mid_point_thickness.set_value(mid_point_thickness);
                    cur_paren->paren_item->mutldata()->end_point_thickness.set_value(end_point_thickness);
                    cur_paren->paren_item->mutldata()->shoulder_width = shoulder;

                    ParenthesisLayout::create_path_and_shape(opening_paren->paren_item, opening_paren->paren_item->mutldata());
                    ParenthesisLayout::create_path_and_shape(cur_paren->paren_item, cur_paren->paren_item->mutldata());
                }

                double closing_pos = opening_paren->closing_paren_pos;
                if (muse::real_is_equal(-DBL_MAX, closing_pos)) {
                    closing_pos = opening_paren->x() + opening_paren->bounding_rect().width();
                }
                cur_paren->setx(closing_pos);

                // Outer parens must always be the same length or longer than inner parens
                for (ChordSymbolParen *outer_paren : opening_paren_stack) {
                    outer_paren->top = std::min(opening_paren->top, outer_paren->top);
                    outer_paren->bottom = std::max(opening_paren->bottom, outer_paren->bottom);
                    outer_paren->closing_paren_pos = std::max(opening_paren->closing_paren_pos, outer_paren->closing_paren_pos);
                }
            }
        } else if (TextSegment *text_seg = dynamic_cast<TextSegment *>(render_item)) {
            // Set top paren height
            last_text_seg_height = text_seg->height();
            last_text_seg_top = text_seg->tight_bounding_rect().translated(text_seg->pos()).y();
            if (muse::real_is_null(root_ref_height)) {
                root_ref_height = text_seg->height();
                double top = text_seg->tight_bounding_rect().translated(text_seg->pos()).y();
                double bottom = text_seg->pos().y();
                double height = (bottom - top) + 2 * paren_extension;

                root_ref_height = height;
                root_cap_height = text_seg->cap_height();
            }
            if (!opening_paren_stack.empty()) {
                ChordSymbolParen *top_paren = opening_paren_stack.back();
                if (text_seg->font().type() != Font::Type::MUSIC_SYMBOL_TEXT) {
                    top_paren->top = std::min(top_paren->top, text_seg->tight_bounding_rect().translated(text_seg->pos()).y());
                    top_paren->bottom = std::max(top_paren->bottom, text_seg->pos().y());
                }
                top_paren->closing_paren_pos = std::max(top_paren->closing_paren_pos, text_seg->x() + text_seg->width());
                continue;
            }
        }
    }

    auto compute_padding = [](HarmonyRenderItem *item1, HarmonyRenderItem *item2) {
        bool item1_paren = item1->type() == HarmonyRenderItemType::PAREN;
        bool item2_paren = item2->type() == HarmonyRenderItemType::PAREN;

        double padding = 0.0;
        if (item1_paren && item2_paren) {
            padding = std::min(item1->right_padding(), item2->left_padding());
        } else {
            padding = std::max(item1->right_padding(), item2->left_padding());
        }

        double scaling = (item1->height() + item2->height()) / 2;
        padding *= scaling;

        return padding;
    };

    // Create space in text for parentheses
    // TODO - STACKED C7((#11)#13)
    // C7(#11(#13))
    double additional_space = 0.0;
    bool prev_paren = false;

    for (size_t i = 0; i < item_list.size(); i++) {
        HarmonyRenderItem *render_item = item_list.at(i);
        double padding = i != 0 ? compute_padding(item_list.at(i - 1), render_item) : 0.0;

        if (ChordSymbolParen *paren = dynamic_cast<ChordSymbolParen *>(render_item)) {
            if (paren->paren_item->direction() == DirectionH::LEFT) {
                double height_to_ref_ratio = std::max(paren->height(), root_ref_height) / root_ref_height;
                bool scale_paren_width = !smufl_parens && !prev_paren && !muse::real_is_equal(height_to_ref_ratio, 1.0);
                const double paren_padding_scaler = 0.6;
                double scaled_paren_width = paren->paren_item->width() * paren_padding_scaler / height_to_ref_ratio;

                double paren_width = scale_paren_width ? scaled_paren_width : paren->paren_item->width();

                double paren_adjust = additional_space + padding + (smufl_parens ? 0.0 : paren_width);
                paren->movex(paren_adjust);
                additional_space += paren_width + padding;
            } else if (paren->paren_item->direction() == DirectionH::RIGHT) {
                paren->movex(additional_space + padding);
                additional_space += paren->paren_item->width() + padding;
            }
            prev_paren = true;
        } else if (TextSegment *ts = dynamic_cast<TextSegment *>(render_item)) {
            additional_space += padding;
            ts->movex(additional_space);
            prev_paren = false;
        }
    }
}

//---------------------------------------------------------
//   render
//    construct Chord Symbol
//---------------------------------------------------------

void HarmonyLayout::render(Harmony *item, Harmony::LayoutData *ldata, const LayoutContext &ctx) {
    for (const HarmonyRenderItem *render_item : ldata->render_item_list()) {
        delete render_item;
    }
    ldata->render_item_list.mut_value().clear();
    if (item->harmony_type() == HarmonyType::ROMAN) {
        render_roman_numeral(item, ldata);
        return;
    }

    // Render standard or Nashville chords

    ChordList *chord_list = item->score()->chord_list();

    ldata->font_list.mut_value().clear();
    for (const ChordFont &cf : chord_list->fonts) {
        Font ff(item->font());
        ff.set_family(item->family(), Font::Type::HARMONY);

        double mag = item->mag() * cf.mag;
        ff.set_point_size_f(ff.point_size_f() * mag);
        if (cf.music_symbol_text) {
            ff.set_family(cf.family, Font::Type::MUSIC_SYMBOL_TEXT);
        } else if (!(cf.family.is_empty() || cf.family == "default")) {
            ff.set_family(cf.family, Font::Type::HARMONY);
        }
        ldata->font_list.mut_value().push_back(ff);
    }
    if (ldata->font_list.mut_value().empty()) {
        Font ff(item->font());
        ff.set_family(item->family(), Font::Type::HARMONY);

        ldata->font_list.mut_value().push_back(ff);
    }

    ldata->polychord_divider_lines.reset();
    HarmonyRenderCtx harmony_ctx;

    // Map of text segments and their final width
    std::multimap<double, std::vector<HarmonyRenderItem *>> chord_text_segments;

    for (size_t i = item->chords().size(); i > 0; i--) {
        harmony_ctx.info = item->chords().at(i - 1);
        render_single_harmony(item, ldata, harmony_ctx, ctx);

        chord_text_segments.emplace(std::pair<double, std::vector<HarmonyRenderItem *>>{harmony_ctx.x(), harmony_ctx.render_item_list});
        ldata->render_item_list.mut_value().insert(ldata->render_item_list.mut_value().end(), harmony_ctx.render_item_list.begin(),
                                                   harmony_ctx.render_item_list.end());

        // Measure divider spacing from lowest baseline and highest cap-height in segments
        double root_baseline = harmony_ctx.render_item_list.empty() ? -DBL_MAX : harmony_ctx.render_item_list.front()->y();
        double bottom_baseline = -DBL_MAX;
        for (const HarmonyRenderItem *render_item : harmony_ctx.render_item_list) {
            if (const TextSegment *ts = dynamic_cast<const TextSegment *>(render_item)) {
                bottom_baseline = std::max(bottom_baseline, ts->y());
            }
        }

        double diff = root_baseline - bottom_baseline;
        if (bottom_baseline > root_baseline) {
            for (HarmonyRenderItem *render_item : harmony_ctx.render_item_list) {
                if (TextSegment *ts = dynamic_cast<TextSegment *>(render_item)) {
                    ts->movey(diff);
                }
            }
        }
        if (i == item->chords().size()) {
            // Set baseline for bottom chord
            ldata->baseline = -diff;
        }

        double top_cap_height = DBL_MAX;
        for (const HarmonyRenderItem *render_item : harmony_ctx.render_item_list) {
            if (const TextSegment *ts = dynamic_cast<const TextSegment *>(render_item)) {
                top_cap_height = std::min(top_cap_height, ts->y() - ts->cap_height());
            }
        }

        harmony_ctx.render_item_list.clear();
        if (item->chords().size() == 1 || i == 1) {
            break;
        }

        assert(!muse::real_is_equal(top_cap_height, DBL_MAX));

        harmony_ctx.setx(0);
        harmony_ctx.sety(top_cap_height);

        double line_y = harmony_ctx.y() - item->absolute_from_spatium(ctx.conf().style().style_s(Sid::POLYCHORD_DIVIDER_SPACING)) -
                        item->absolute_from_spatium(ctx.conf().style().style_s(Sid::POLYCHORD_DIVIDER_THICKNESS)) / 2;
        LineF line = LineF(PointF(0.0, line_y), PointF(0.0, line_y));
        ldata->polychord_divider_lines.mut_value().push_back(line);

        harmony_ctx.movey(item->absolute_from_spatium(-ctx.conf().style().style_s(Sid::POLYCHORD_DIVIDER_SPACING)) * 2.0);
        harmony_ctx.movey(item->absolute_from_spatium(-ctx.conf().style().style_s(Sid::POLYCHORD_DIVIDER_THICKNESS)));
    }

    // Align polychords

    if (item->align() == AlignH::LEFT) {
        return;
    }

    double longest_line = 0.0;

    for (const auto &[width, _] : chord_text_segments) {
        if (width > longest_line) {
            longest_line = width;
        }
    }

    for (auto &text_segs : chord_text_segments) {
        double width = text_segs.first;
        std::vector<HarmonyRenderItem *> &segs = text_segs.second;

        double diff = longest_line - width;

        if (muse::real_is_null(diff)) {
            continue;
        }

        // For centre align adjust by .5* difference, for right align adjust by full difference
        if (item->align() == AlignH::HCENTER) {
            diff *= 0.5;
        }

        for (HarmonyRenderItem *seg : segs) {
            seg->movex(diff);
        }
    }
}

void HarmonyLayout::render_roman_numeral(Harmony *item, Harmony::LayoutData *ldata) {
    HarmonyRenderCtx harmony_ctx;
    if (item->chords().empty()) {
        return;
    }
    HarmonyInfo *info = item->chords().front();

    render(item, ldata, info->text_name(), harmony_ctx);
    ldata->render_item_list.mut_value().insert(ldata->render_item_list.mut_value().end(), harmony_ctx.render_item_list.begin(),
                                               harmony_ctx.render_item_list.end());
    ldata->baseline = 0.0;
}

void HarmonyLayout::do_render_single_harmony(Harmony *item, Harmony::LayoutData *ldata, HarmonyRenderCtx &harmony_ctx, int root_tpc,
                                             int bass_tpc, const LayoutContext &ctx) {
    HarmonyInfo *info = harmony_ctx.info;
    ChordList *chord_list = info->chord_list();
    if (!chord_list) {
        return;
    }

    const MStyle &style = ctx.conf().style();

    NoteCaseType root_case = item->root_render_case(info);
    NoteCaseType bass_case = item->bass_render_case();

    NoteSpellingType spelling = style.style_v(Sid::CHORD_SYMBOL_SPELLING).value<NoteSpellingType>();

    const ChordDescription *cd = info->get_description();
    const bool stack_modifiers = style.style_b(Sid::VERTICALLY_STACK_MODIFIERS) && !item->do_not_stack_modifiers();

    if (item->harmony_type() == HarmonyType::STANDARD && tpc_is_valid(root_tpc)) {
        // render root
        render(item, ldata, chord_list->render_list_root, harmony_ctx, ctx, root_tpc, spelling, root_case);
        // render extension
        if (cd) {
            render(item, ldata, stack_modifiers ? cd->render_list_stacked : cd->render_list, harmony_ctx, ctx, 0);
        }
    } else if (item->harmony_type() == HarmonyType::NASHVILLE && tpc_is_valid(root_tpc)) {
        // render function
        render(item, ldata, chord_list->render_list_function, harmony_ctx, ctx, root_tpc, spelling, bass_case);
        double adjust = chord_list->nominal_adjust();
        harmony_ctx.movey(adjust * item->mag_s() * item->spatium() * .2);
        // render extension
        if (cd) {
            render(item, ldata, stack_modifiers ? cd->render_list_stacked : cd->render_list, harmony_ctx, ctx, 0);
        }
    } else {
        render(item, ldata, info->text_name(), harmony_ctx);
    }

    // render bass
    if (tpc_is_valid(bass_tpc)) {
        const std::vector<RenderActionPtr> &bass_note_chord_list =
            style.style_b(Sid::CHORD_BASS_NOTE_STAGGER) ? chord_list->render_list_bass_offset : chord_list->render_list_bass;

        static const std::wregex pattern_69 = std::wregex(L"6[,/]?9");
        const bool is69 = info->text_name().contains(pattern_69);
        const bool has_modifier_stack =
            stack_modifiers && (info->parsed_chord() ? info->parsed_chord()->modifier_list().size() > 1 : false);

        if (has_modifier_stack || is69) {
            render(item, ldata, {std::make_shared<RenderActionMove>(0.05, 0.0)}, harmony_ctx, ctx, bass_tpc, spelling, bass_case,
                   item->bass_scale());
        }
        render(item, ldata, bass_note_chord_list, harmony_ctx, ctx, bass_tpc, spelling, bass_case, item->bass_scale());
    }
}

void HarmonyLayout::render_single_harmony(Harmony *item, Harmony::LayoutData *ldata, HarmonyRenderCtx &harmony_ctx,
                                          const LayoutContext &ctx) {
    harmony_ctx.h_align = true;
    const MStyle &style = ctx.conf().style();
    HarmonyInfo *info = harmony_ctx.info;

    int root_tpc = info->root_tpc();
    int bass_tpc = info->bass_tpc();

    DisplayCapoChordType display_capo = style.style_v(Sid::DISPLAY_CAPO_CHORDS).value<DisplayCapoChordType>();

    // render single if no capo or both

    if (display_capo == DisplayCapoChordType::BOTH || display_capo == DisplayCapoChordType::CONCERT) {
        do_render_single_harmony(item, ldata, harmony_ctx, root_tpc, bass_tpc, ctx);
    }

    int capo = style.style_i(Sid::CAPO_POSITION);
    if (capo == 0) {
        return;
    }

    int capo_root_tpc = Tpc::TPC_INVALID;
    int capo_bass_tpc = Tpc::TPC_INVALID;
    if (tpc_is_valid(root_tpc) && capo > 0 && capo < 12) {
        int tpc_offset[] = {0, 5, -2, 3, -4, 1, 6, -1, 4, -3, 2, -5};
        capo_root_tpc = root_tpc + tpc_offset[capo];
        capo_bass_tpc = bass_tpc;

        if (tpc_is_valid(capo_bass_tpc)) {
            capo_bass_tpc += tpc_offset[capo];
        }

        /*
         * For guitarists, avoid x and bb in Root or Bass,
         * and also avoid E#, B#, Cb and Fb in Root.
         */
        if (capo_root_tpc < 8 || (tpc_is_valid(capo_bass_tpc) && capo_bass_tpc < 6)) {
            capo_root_tpc += 12;
            if (tpc_is_valid(capo_bass_tpc)) {
                capo_bass_tpc += 12;
            }
        } else if (capo_root_tpc > 24 || (tpc_is_valid(capo_bass_tpc) && capo_bass_tpc > 26)) {
            capo_root_tpc -= 12;
            if (tpc_is_valid(capo_bass_tpc)) {
                capo_bass_tpc -= 12;
            }
        }
    }

    // Render parens if both
    if (display_capo == DisplayCapoChordType::BOTH) {
        RenderActionParenPtr p = std::make_shared<RenderActionParenLeft>();
        render_action_paren(item, p, harmony_ctx);
    }
    if (display_capo == DisplayCapoChordType::BOTH || display_capo == DisplayCapoChordType::TRANSPOSED) {
        // render capo if both or capo
        do_render_single_harmony(item, ldata, harmony_ctx, capo_root_tpc, capo_bass_tpc, ctx);
    }
    if (display_capo == DisplayCapoChordType::BOTH) {
        RenderActionParenPtr p = std::make_shared<RenderActionParenRight>();
        render_action_paren(item, p, harmony_ctx);
    }
}

//---------------------------------------------------------
//   render
//---------------------------------------------------------

void HarmonyLayout::render(Harmony *item, Harmony::LayoutData *ldata, const String &s, HarmonyRenderCtx &harmony_ctx) {
    if (s.is_empty()) {
        return;
    }

    Font f = item->harmony_type() != HarmonyType::ROMAN ? ldata->font_list.value().front() : item->font();
    TextSegment *ts = new TextSegment(s, f, harmony_ctx.x(), harmony_ctx.y(), harmony_ctx.h_align);
    harmony_ctx.render_item_list.push_back(ts);
    harmony_ctx.movex(ts->width());
}

void HarmonyLayout::render(Harmony *item, Harmony::LayoutData *ldata, SymId sym, HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx) {
    if (sym == SymId::NO_SYM) {
        return;
    }

    Font f = item->harmony_type() != HarmonyType::ROMAN ? ldata->font_list.value().front() : item->font();
    f.set_family(ctx.conf().style().style_st(Sid::MUSICAL_TEXT_FONT), Font::Type::MUSIC_SYMBOL_TEXT);

    String s = ctx.conf().engraving_font()->to_string(sym);

    TextSegment *ts = new TextSegment(s, f, harmony_ctx.x(), harmony_ctx.y(), harmony_ctx.h_align);
    harmony_ctx.render_item_list.push_back(ts);
    harmony_ctx.movex(ts->width());
}

void HarmonyLayout::render(Harmony *item, Harmony::LayoutData *ldata, const std::vector<RenderActionPtr> &render_list,
                           HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx, int tpc, NoteSpellingType note_spelling,
                           NoteCaseType note_case, double note_mag) {
    harmony_ctx.stack = {};
    harmony_ctx.tpc = tpc;
    harmony_ctx.note_spelling = note_spelling;
    harmony_ctx.note_case = note_case;
    harmony_ctx.scale = note_mag;

    for (const RenderActionPtr &a : render_list) {
        render_action(item, ldata, a, harmony_ctx, ctx);
    }
}

void HarmonyLayout::render_action(Harmony *item, Harmony::LayoutData *ldata, const RenderActionPtr &a, HarmonyRenderCtx &harmony_ctx,
                                  const LayoutContext &ctx) {
    switch (a->action_type()) {
        case RenderAction::RenderActionType::SET:
            render_action_set(item, ldata, std::static_pointer_cast<RenderActionSet>(a), harmony_ctx, ctx);
            break;
        case RenderAction::RenderActionType::MOVE:
            render_action_move(item, std::static_pointer_cast<RenderActionMove>(a), harmony_ctx);
            break;
        case RenderAction::RenderActionType::MOVEXHEIGHT:
            render_action_move_x_height(item, std::static_pointer_cast<RenderActionMoveXHeight>(a), harmony_ctx);
            break;
        case RenderAction::RenderActionType::PUSH: render_action_push(harmony_ctx); break;
        case RenderAction::RenderActionType::POP: render_action_pop(std::static_pointer_cast<RenderActionPop>(a), harmony_ctx); break;
        case RenderAction::RenderActionType::NOTE: render_action_note(item, ldata, harmony_ctx); break;
        case RenderAction::RenderActionType::ACCIDENTAL: render_action_acc(item, ldata, harmony_ctx, ctx); break;
        case RenderAction::RenderActionType::STOPHALIGN: render_action_align(harmony_ctx); break;
        case RenderAction::RenderActionType::SCALE: render_action_scale(std::static_pointer_cast<RenderActionScale>(a), harmony_ctx); break;
        case RenderAction::RenderActionType::PAREN:
            render_action_paren(item, std::static_pointer_cast<RenderActionParen>(a), harmony_ctx);
            break;
        default: LOGD("unknown render action %d", static_cast<int>(a->action_type()));
    }
}

void HarmonyLayout::render_action_push(HarmonyRenderCtx &harmony_ctx) { harmony_ctx.stack.push(harmony_ctx.pos); }

void HarmonyLayout::render_action_pop(const RenderActionPopPtr &a, HarmonyRenderCtx &harmony_ctx) {
    if (harmony_ctx.stack.empty()) {
        LOGD("RenderAction::RenderActionType::POP: stack empty");
        return;
    }

    PointF pt = harmony_ctx.stack.top();
    harmony_ctx.stack.pop();
    harmony_ctx.pos = PointF(a->pop_x() ? pt.x() : harmony_ctx.x(), a->pop_y() ? pt.y() : harmony_ctx.y());
}

void HarmonyLayout::render_action_note(Harmony *item, Harmony::LayoutData *ldata, HarmonyRenderCtx &harmony_ctx) {
    if (!tpc_is_valid(harmony_ctx.tpc)) {
        return;
    }
    const ChordList *chord_list = item->score()->chord_list();
    const Staff *st = item->staff();
    const Key key = st ? st->key(item->tick()) : Key::INVALID;

    String c;
    AccidentalVal acc;

    if (item->harmony_type() == HarmonyType::STANDARD) {
        tpc2name(harmony_ctx.tpc, harmony_ctx.note_spelling, harmony_ctx.note_case, c, acc);
    } else if (item->harmony_type() == HarmonyType::NASHVILLE) {
        String acc_str;
        tpc2_function(harmony_ctx.tpc, key, acc_str, c);
    }

    if (c.empty()) {
        return;
    }

    String lookup = u"note" + c;
    ChordSymbol cs = chord_list->symbol(lookup);
    if (!cs.is_valid()) {
        cs = chord_list->symbol(c);
    }
    String text = cs.is_valid() ? cs.value : c;
    muse::draw::Font font = cs.is_valid() ? ldata->font_list.value()[cs.font_idx] : ldata->font_list.value().front();
    font.set_point_size_f(font.point_size_f() * harmony_ctx.scale);

    TextSegment *ts = new TextSegment(text, font, harmony_ctx.x(), harmony_ctx.y(), harmony_ctx.h_align);
    harmony_ctx.render_item_list.push_back(ts);
    harmony_ctx.movex(ts->width());
}

void HarmonyLayout::render_action_acc(Harmony *item, Harmony::LayoutData *ldata, HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx) {
    if (!tpc_is_valid(harmony_ctx.tpc)) {
        return;
    }
    const ChordList *chord_list = item->score()->chord_list();
    const Staff *st = item->staff();
    const Key key = st ? st->key(item->tick()) : Key::INVALID;

    String c;
    String acc;
    String context = u"accidental";

    if (item->harmony_type() == HarmonyType::STANDARD) {
        tpc2name(harmony_ctx.tpc, harmony_ctx.note_spelling, harmony_ctx.note_case, c, acc);
    } else if (item->harmony_type() == HarmonyType::NASHVILLE) {
        tpc2_function(harmony_ctx.tpc, key, acc, c);
    }

    if (acc.empty()) {
        return;
    }

    // Try to find token & execute renderlist
    ChordToken tok = chord_list->token(acc, ChordTokenClass::ACCIDENTAL);
    if (tok.is_valid()) {
        for (const RenderActionPtr &a : tok.render_list) {
            render_action(item, ldata, a, harmony_ctx, ctx);
        }
        return;
    }

    // No valid token, find symbol

    // German spelling - use special symbol for accidental in TPC_B_B
    // to allow it to be rendered as either Bb or B
    if (harmony_ctx.tpc == Tpc::TPC_B_B && harmony_ctx.note_spelling == NoteSpellingType::GERMAN) {
        context = u"german_B";
    }
    String lookup = context + acc;
    ChordSymbol cs = chord_list->symbol(lookup);
    if (!cs.is_valid()) {
        cs = chord_list->symbol(acc);
    }
    String text = cs.is_valid() ? cs.value : c;
    muse::draw::Font font = cs.is_valid() ? ldata->font_list.value()[cs.font_idx] : ldata->font_list.value().front();
    font.set_point_size_f(font.point_size_f() * harmony_ctx.scale);

    TextSegment *ts = new TextSegment(text, font, harmony_ctx.x(), harmony_ctx.y(), harmony_ctx.h_align);
    harmony_ctx.render_item_list.push_back(ts);
    harmony_ctx.movex(ts->width());
}

void HarmonyLayout::render_action_align(HarmonyRenderCtx &harmony_ctx) { harmony_ctx.h_align = false; }

void HarmonyLayout::render_action_scale(const RenderActionScalePtr &a, HarmonyRenderCtx &harmony_ctx) { harmony_ctx.scale *= a->scale(); }

void HarmonyLayout::render_action_paren(Harmony *item, const RenderActionParenPtr &a, HarmonyRenderCtx &harmony_ctx) {
    Parenthesis *p = Factory::create_parenthesis(item);
    p->set_direction(a->direction());
    p->set_color(item->color());
    p->set_follow_parent_color(true);
    p->set_generated(true);

    ChordSymbolParen *paren_item = new ChordSymbolParen(p, harmony_ctx.h_align, harmony_ctx.x(), harmony_ctx.y());
    harmony_ctx.render_item_list.push_back(paren_item);
}

void HarmonyLayout::kern_characters(const Harmony *item, const String &text, HarmonyRenderCtx &harmony_ctx, const LayoutContext &ctx) {
    if (harmony_ctx.render_item_list.empty()) {
        return;
    }
    // Character pair and distance to move the second
    // Blank strings will apply the kerning no matter the following character
    // TODO - move this information to the XML file
    static const std::map<std::pair<String, String>, double> kerned_characters{
        {{u"A", u"\uE870"}, -0.4},      // dim
        {{u"A", u"\uE871"}, -0.3},      // half-dim
        {{u"\uE873", u"\uE870"}, -0.4}, // triangle - dim
        {{u"\uE873", u"\uE871"}, -0.3}, // triangle - half-dim
        {{u"A", u"/"}, 0.1},

        {{u"A", u"\uE18E"}, -0.15},      // dim JAZZ
        {{u"A", u"\uE18F"}, -0.15},      // half-dim JAZZ
        {{u"\uE18A", u"\uE18E"}, -0.15}, // triangle - dim JAZZ
        {{u"\uE18A", u"\uE18F"}, -0.15}, // triangle - half-dim JAZZ

        {{u"\u266D", u""}, -0.15},  // b JAZZ
        {{u"\u266E", u""}, -0.15},  // natural JAZZ
        {{u"\u266F", u""}, -0.15},  // # JAZZ
        {{u"\u1D12A", u""}, -0.15}, // ## JAZZ
        {{u"\u1D12B", u""}, -0.15}, // bb JAZZ
    };

    HarmonyRenderItem *prev_seg = harmony_ctx.render_item_list.back();
    TextSegment *ts = dynamic_cast<TextSegment *>(prev_seg);
    if (!ts) {
        return;
    }

    for (auto &kern_info : kerned_characters) {
        const std::pair<String, String> kern_pair = kern_info.first;
        bool end_char = ts->text().ends_with(kern_pair.first);
        bool start_char = text.starts_with(kern_pair.second);
        // VERY DIRTY HACK ALERT
        // "Match any" should only be applied to the Jazz preset currently.
        // Remove the jazz condition when these kern values are moved to the XML files
        if (kern_pair.second.is_empty() && ctx.conf().style_v(Sid::CHORD_STYLE).value<ChordStylePreset>() != ChordStylePreset::JAZZ) {
            continue;
        }
        bool start_match_any = kern_pair.second.is_empty();
        if ((end_char && start_char) || (end_char && start_match_any)) {
            const FontMetrics fm = FontMetrics(item->font());
            const double scale = harmony_ctx.scale * item->mag();
            harmony_ctx.pos = harmony_ctx.pos + PointF(kern_info.second, 0.0) * FontMetrics::cap_height(item->font()) * scale;
            break;
        }
    }
}

void HarmonyLayout::render_action_set(Harmony *item, Harmony::LayoutData *ldata, const RenderActionSetPtr &a, HarmonyRenderCtx &harmony_ctx,
                                      const LayoutContext &ctx) {
    const ChordList *chord_list = item->score()->chord_list();
    const ChordSymbol cs = chord_list->symbol(a->text());
    const String text = cs.is_valid() ? cs.value : a->text();
    muse::draw::Font font = cs.is_valid() ? ldata->font_list.value()[cs.font_idx] : ldata->font_list.value().front();
    font.set_point_size_f(font.point_size_f() * harmony_ctx.scale);
    if (item->harmony_type() == HarmonyType::NASHVILLE) {
        double nmag = chord_list->nominal_mag();
        font.set_point_size_f(font.point_size_f() * nmag);
    }

    kern_characters(item, text, harmony_ctx, ctx);

    TextSegment *ts = new TextSegment(text, font, harmony_ctx.x(), harmony_ctx.y(), harmony_ctx.h_align);
    harmony_ctx.movex(ts->width());

    if (a->render_text()) {
        harmony_ctx.render_item_list.push_back(ts);
        return;
    }

    delete ts;
}

void HarmonyLayout::render_action_move(Harmony *item, const RenderActionMovePtr &a, HarmonyRenderCtx &harmony_ctx) {
    const double scale = (a->scaled() ? harmony_ctx.scale : 1.0) * item->mag();
    harmony_ctx.pos = harmony_ctx.pos + a->vec() * FontMetrics::cap_height(item->font()) * scale;
}

void HarmonyLayout::render_action_move_x_height(Harmony *item, const RenderActionMoveXHeightPtr &a, HarmonyRenderCtx &harmony_ctx) {
    const int direction = a->up() ? -1 : 1;
    const double scale = a->scaled() ? harmony_ctx.scale : 1.0;
    const FontMetrics fm = FontMetrics(item->font());
    harmony_ctx.movey(direction * fm.x_height() * scale);
}
