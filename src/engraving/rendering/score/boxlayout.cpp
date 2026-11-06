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
#include "boxlayout.h"
#include "dom/fret.h"
#include "dom/harmony.h"
#include "dom/system.h"
#include "dom/text.h"
#include "draw/fontmetrics.h"

#include "tlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;
using namespace muse::draw;

void BoxLayout::layout_box(const Box *item, Box::LayoutData *ldata, const LayoutContext &ctx) {
    switch (item->type()) {
        case ElementType::HBOX:
            BoxLayout::layout_h_box(static_cast<const HBox *>(item), static_cast<HBox::LayoutData *>(ldata), ctx);
            break;
        case ElementType::VBOX:
            BoxLayout::layout_v_box(static_cast<const VBox *>(item), static_cast<VBox::LayoutData *>(ldata), ctx);
            break;
        case ElementType::FBOX:
            BoxLayout::layout_f_box(static_cast<const FBox *>(item), static_cast<FBox::LayoutData *>(ldata), ctx);
            break;
        case ElementType::TBOX:
            BoxLayout::layout_t_box(static_cast<const TBox *>(item), static_cast<TBox::LayoutData *>(ldata), ctx);
            break;
        default: UNREACHABLE; break;
    }
}

void BoxLayout::layout_base_box(const Box *item, Box::LayoutData *ldata, const LayoutContext &ctx) {
    TLayout::layout_base_measure_base(item, ldata, ctx);
}

void BoxLayout::layout_h_box(const HBox *item, HBox::LayoutData *ldata, const LayoutContext &ctx) {
    if (item->ownership_parent() && item->ownership_parent()->is_v_box()) {
        const VBox *parent_v_box = to_v_box(item->ownership_parent());

        LD_CONDITION(parent_v_box->ldata()->is_set_bbox());

        double x = parent_v_box->left_margin() * ::mu::engraving::dpmm;
        double y = parent_v_box->top_margin() * ::mu::engraving::dpmm;
        double w = item->absolute_from_spatium(item->box_width());
        double h =
            parent_v_box->ldata()->bbox().height() - (parent_v_box->top_margin() + parent_v_box->bottom_margin()) * ::mu::engraving::dpmm;
        ldata->set_pos(x, y);
        ldata->set_bbox(0.0, 0.0, w, h);
    } else if (item->system()) {
        const System *parent_system = item->system();

        LD_CONDITION(parent_system->ldata()->is_set_bbox());

        if (!ldata->is_set_pos()) {
            ldata->set_pos(PointF());
        }
        ldata->set_bbox(item->absolute_from_spatium(item->top_gap()), 0.0, item->absolute_from_spatium(item->box_width()),
                        parent_system->ldata()->bbox().height());
    } else {
        ldata->set_pos(PointF());
        ldata->set_bbox(0.0, 0.0, 50, 50);
    }
    layout_base_box(item, ldata, ctx);
}

void BoxLayout::layout_h_box2(HBox *item, const LayoutContext &ctx) { layout_base_box(item, item->mutldata(), ctx); }

void BoxLayout::layout_v_box(const VBox *item, VBox::LayoutData *ldata, const LayoutContext &ctx) {
    ldata->set_pos(PointF());

    if (item->system()) {
        const System *parent_system = item->system();

        LD_CONDITION(parent_system->ldata()->is_set_bbox());

        ldata->set_bbox(0.0, 0.0, parent_system->ldata()->bbox().width(), item->absolute_from_spatium(item->box_height()));
    } else {
        ldata->set_bbox(0.0, 0.0, 50, 50);
    }

    for (EngravingItem *e : item->el()) {
        TLayout::layout_item(e, const_cast<LayoutContext &>(ctx));
    }
    bool box_auto_size = item->get_property(Pid::BOX_AUTOSIZE).to_bool();
    bool height_changed = false;
    if (box_auto_size) {
        double content_height = item->content_rect().height();

        if (content_height < item->min_height()) {
            content_height = item->min_height();
        }

        ldata->set_height(content_height);
        height_changed = true;
    }

    if (box_auto_size && MScore::no_images) {
        // adjustLayoutWithoutImages
        double calculated_v_box_height = 0;
        const int padding = item->size_is_spatium_dependent() ? ctx.conf().spatium() : ctx.conf().style().default_spatium();
        ElementList elist = item->el();
        for (EngravingItem *e : elist) {
            if (e->is_text()) {
                Text *txt = to_text(e);
                Text::LayoutData *txt_ld = txt->mutldata();

                LD_CONDITION(txt_ld->is_set_bbox());

                RectF bbox = txt_ld->bbox();
                bbox.move_top(0.0);
                txt_ld->set_bbox(bbox);
                calculated_v_box_height += txt_ld->bbox().height() + padding;
            }
        }

        ldata->set_height(calculated_v_box_height);
        height_changed = true;
    }
    if (height_changed) {
        for (EngravingItem *e : item->el()) {
            TLayout::layout_item(e, const_cast<LayoutContext &>(ctx));
        }
    }

    layout_base_box(item, ldata, ctx);
}

void BoxLayout::layout_f_box(const FBox *item, FBox::LayoutData *ldata, const LayoutContext &ctx) {
    Score *score = item->score();

    if (item->needs_rebuild()) {
        for (EngravingItem *element : item->el()) {
            if (!element || !element->is_fret_diagram()) {
                continue;
            }

            FretDiagram *diagram = to_fret_diagram(element);
            //! NOTE: We need to layout the diagrams to get the harmony names to show in the UI
            TLayout::layout_item(diagram, const_cast<LayoutContext &>(ctx));
            if (!diagram->visible()) {
                //! but we don't need to draw them, so let's add a skip
                diagram->mutldata()->set_is_skip_draw(true);
                diagram->harmony()->mutldata()->set_is_skip_draw(true);
                continue;
            }
        }

        /// FBox::init requires all chord symbols in the score have valid ldata
        /// Perform layout
        for (mu::engraving::Segment *segment = score->first_segment(mu::engraving::SegmentType::CHORD_REST); segment;
             segment = segment->next1(mu::engraving::SegmentType::CHORD_REST)) {
            for (EngravingItem *seg_item : segment->annotations()) {
                if (!seg_item || !seg_item->part()) {
                    continue;
                }
                if (!(seg_item->is_harmony() || seg_item->is_fret_diagram())) {
                    continue;
                }

                Harmony *harmony = seg_item->is_harmony() ? to_harmony(seg_item) : to_fret_diagram(seg_item)->harmony();
                if (!harmony || harmony->harmony_type() != HarmonyType::STANDARD || harmony->ldata()->is_valid()) {
                    continue;
                }

                TLayout::layout_harmony(harmony, harmony->mutldata(), ctx);
            }
        }

        const_cast<FBox *>(item)->init();
        const_cast<FBox *>(item)->set_needs_rebuild(false);
    }

    const System *parent_system = item->system();
    LD_CONDITION(parent_system->ldata()->is_set_bbox());

    ldata->set_pos(PointF());

    std::vector<FretDiagram *> fret_diagrams;
    for (EngravingItem *element : item->el()) {
        if (!element || !element->is_fret_diagram() || !element->visible()) {
            continue;
        }

        fret_diagrams.emplace_back(to_fret_diagram(element));
    }

    //! NOTE: layout fret diagrams and calculate sizes

    const size_t total_diagrams = fret_diagrams.size();
    double max_fret_diagram_width = 0.0;

    for (size_t i = 0; i < total_diagrams; ++i) {
        FretDiagram *fret_diagram = fret_diagrams[i];
        fret_diagram->set_user_mag(item->diagram_scale());

        Harmony *harmony = fret_diagram->harmony();
        harmony->mutldata()->set_mag(item->text_scale());

        TLayout::layout_item(fret_diagram, const_cast<LayoutContext &>(ctx));

        //! reset the skip which was added above
        fret_diagram->mutldata()->set_is_skip_draw(false);
        harmony->mutldata()->set_is_skip_draw(false);

        double width = fret_diagram->ldata()->bbox().width();
        max_fret_diagram_width = std::max(max_fret_diagram_width, width);
    }

    //! NOTE: table view layout

    const double spatium = item->spatium();
    const size_t chords_per_row = item->chords_per_row();
    const double row_gap = item->row_gap().val() * spatium;
    const double column_gap = item->column_gap().val() * spatium;

    //! The height of each row is determined by the height of the tallest cell in that row
    const size_t num_rows = chords_per_row > 0 ? (total_diagrams + chords_per_row - 1) / chords_per_row : 0;
    std::vector<double> row_heights;
    row_heights.reserve(num_rows);
    std::vector<double> harmony_heights;
    harmony_heights.reserve(num_rows);
    std::vector<double> harmony_baselines;
    harmony_baselines.reserve(num_rows);
    for (size_t i = 0; i < total_diagrams; i += chords_per_row) {
        size_t items_in_row = std::min(chords_per_row, total_diagrams - i);
        double max_row_height = 0.0;
        double max_harmony_height = 0.0;
        double harmony_baseline = 0.0;

        for (size_t j = 0; j < items_in_row; ++j) {
            FretDiagram *fret_diagram = fret_diagrams[i + j];

            RectF fret_rect = fret_diagram->ldata()->bbox();

            const Harmony::LayoutData *harmony_ldata = fret_diagram->harmony()->ldata();
            RectF harmony_rect = harmony_ldata->bbox().translated(harmony_ldata->pos());

            double height = fret_rect.united(harmony_rect).height();
            max_row_height = std::max(max_row_height, height);
            max_harmony_height = std::max(max_harmony_height, -harmony_rect.top());
            harmony_baseline = std::min(harmony_baseline, harmony_ldata->pos().y());
        }

        row_heights.push_back(max_row_height);
        harmony_heights.push_back(max_harmony_height);
        harmony_baselines.push_back(harmony_baseline);
    }

    const double cell_width = max_fret_diagram_width;
    const size_t columns = std::min(total_diagrams, chords_per_row);

    const double total_table_width = cell_width * columns + (columns - 1) * column_gap;

    AlignH align_h = item->content_horizontal_alignment();
    const double left_margin = item->get_property(Pid::LEFT_MARGIN).to_double() * spatium;
    const double right_margin = item->get_property(Pid::RIGHT_MARGIN).to_double() * spatium;
    const double top_margin = item->get_property(Pid::TOP_MARGIN).to_double() * spatium;
    const double bottom_margin = item->get_property(Pid::BOTTOM_MARGIN).to_double() * spatium;

    const double width = item->system()->width();

    const double start_x = align_h == AlignH::HCENTER ? (width - total_table_width) / 2 :
                           align_h == AlignH::RIGHT   ? width - total_table_width :
                                                        0.0;
    const double start_y = top_margin;

    double bottom_y = 0.0;

    for (size_t i = 0; i < total_diagrams; ++i) {
        FretDiagram *fret_diagram = fret_diagrams[i];
        Harmony *harmony = fret_diagram->harmony();

        size_t row = i / chords_per_row;
        size_t col = i % chords_per_row;

        size_t items_in_row = std::min(chords_per_row, total_diagrams - row * chords_per_row);
        double row_offset_x = align_h == AlignH::HCENTER ?
                                  (total_table_width - (items_in_row * cell_width + (items_in_row - 1) * column_gap)) / 2 :
                              align_h == AlignH::RIGHT ? total_table_width - (items_in_row * cell_width + (items_in_row - 1) * column_gap) -
                                                             right_margin + spatium :
                                                         left_margin + spatium;

        double x = start_x + row_offset_x + col * (cell_width + column_gap);

        double y = start_y;
        for (size_t r = 0; r < row; ++r) {
            y += row_heights[r] + row_gap;
        }

        double common_baseline = harmony_baselines[row];
        double this_baseline = harmony->ldata()->pos().y();
        double baseline_off = common_baseline - this_baseline;
        harmony->mutldata()->move_y(baseline_off);

        double fret_diagram_x = x;
        double fret_diagram_y = y + harmony_heights[row];

        fret_diagram->mutldata()->set_pos(PointF(fret_diagram_x, fret_diagram_y));

        bottom_y = std::max(bottom_y, fret_diagram->mutldata()->bbox().translated(fret_diagram->mutldata()->pos()).bottom());
    }

    double height = bottom_y + bottom_margin;
    if (muse::real_is_null(height)) {
        height = item->min_height();
    }

    ldata->set_bbox(0.0, 0.0, width, height);
}

void BoxLayout::layout_t_box(const TBox *item, TBox::LayoutData *ldata, const LayoutContext &ctx) {
    const System *parent_system = item->system();

    LD_CONDITION(parent_system->ldata()->is_set_bbox());

    ldata->set_pos(PointF());
    ldata->set_bbox(0.0, 0.0, parent_system->ldata()->bbox().width(), 0);

    TLayout::layout_text(item->text(), item->text()->mutldata());

    Text::LayoutData *text_ld = item->text()->mutldata();

    double h = 0.0;
    if (item->text()->empty()) {
        h = FontMetrics::ascent(item->text()->font());
    } else {
        h = text_ld->bbox().height();
    }
    double y = item->top_margin() * ::mu::engraving::dpmm;
    text_ld->set_pos(item->left_margin() * ::mu::engraving::dpmm, y);
    h += item->top_margin() * ::mu::engraving::dpmm + item->bottom_margin() * ::mu::engraving::dpmm;
    ldata->set_bbox(0.0, 0.0, item->system()->width(), h);

    TLayout::layout_base_measure_base(item, ldata, ctx); // layout LayoutBreak's
}
