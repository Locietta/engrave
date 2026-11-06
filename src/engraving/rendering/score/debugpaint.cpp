/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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
#include "debugpaint.h"

#include "iengravingconfiguration.h"
#include "iengravingcontextconfiguration.h" // IWYU pragma: keep

//#include "accessibility/accessibleitem.h"
#include "dom/page.h"
#include "dom/score.h"
#include "dom/system.h"
#include "dom/measurebase.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/segment.h"

#include "tdraw.h"

#include "log.h"

using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

static const Color debug_eltree_selected_color(164, 0, 0);

/// Generates a seemingly random but stable color based on a pointer address.
/// If we would use really random colors, they would change on every redraw.
/// Additional advantage: being able to see when an element is replaced with
/// a different element during layout.
static Color color_for_pointer(const void *ptr) {
    static constexpr uint32_t rgb_total_max = 600;
    static constexpr uint32_t mask = 0xFFFF;
    static constexpr double max = mask + 1.0;

    auto hash = std::hash<const void *>{}(ptr);
    uint32_t cmpnt1 = hash & mask;
    uint32_t cmpnt2 = (hash >> 16) & mask;

    uint32_t r = std::clamp(uint32_t(cmpnt1 / max * 255), 30u, 255u);
    uint32_t g = std::clamp(uint32_t(cmpnt2 / max * 255), 30u, 255u);
    uint32_t b = std::clamp(rgb_total_max - r - g, 30u, 255u);

    return Color(r, g, b, 128);
}

void DebugPaint::paint_element_debug(Painter &painter, const EngravingItem *item) {
    // Elements tree (its diagnostic selection is a desktop tool)
    const bool is_diagnostic_selected = false;

    PointF pos(item->page_pos());
    painter.translate(pos);

    RectF bbox = item->ldata()->bbox();

    if (item->is_type(ElementType::SEGMENT)) {
        if (muse::real_is_null(bbox.height())) {
            bbox.set_height(10.0);
            LOGD() << "Segment bbox height is null";
        }
    }

    if (!bbox.is_empty()) {
        // Draw shape
        if (item->configuration()->debugging_options().color_element_shapes && !item->is_page() && !item->is_system() &&
            !item->is_staff_lines() && !item->is_box()) {
            PainterPath path;
            path.set_fill_rule(PainterPath::FillRule::WINDING_FILL);

            Shape shape = item->shape();
            for (const RectF &rect : shape.elements()) {
                path.add_rect(rect);
            }

            painter.set_pen(PenStyle::NO_PEN);
            painter.set_brush(color_for_pointer(item));
            painter.draw_path(path);
        }

        if (item->configuration()->debugging_options().show_element_masks) {
            PainterPath path;
            path.set_fill_rule(PainterPath::FillRule::WINDING_FILL);
            for (const ShapeElement &el : item->ldata()->mask().elements()) {
                path.add_rect(el);
            }

            painter.set_pen(Color::black);
            Brush brush(Color::black);
            brush.set_style(BrushStyle::B_DIAG_PATTERN);
            painter.set_brush(brush);
            painter.draw_path(path);
        }

        if (item->is_note() && item->configuration()->debugging_options().show_line_attach_points) {
            painter.set_pen(Color::red_color);
            Brush brush(Color::red_color);
            painter.set_brush(brush);
            double radius = 0.1 * item->spatium();
            for (const LineAttachPoint &lap : to_note(item)->line_attach_points()) {
                PointF point = lap.pos();
                painter.draw_ellipse(RectF(point.x() - radius, point.y() - radius, 2 * radius, 2 * radius));
            }
        }

        // Draw bbox
        if (is_diagnostic_selected || item->configuration()->debugging_options().show_element_bounding_rects) {
            double scaling = painter.world_transform().m11() / item->context_configuration()->gui_scaling();
            Pen border_pen(::debug_eltree_selected_color, (item->selected() ? 2.0 : 1.0) / scaling);

            painter.set_pen(border_pen);
            painter.set_brush(BrushStyle::NO_BRUSH);
            painter.draw_rect(bbox);
        }
    }

    painter.translate(-pos);
}

void DebugPaint::paint_page_debug(Painter &painter, const Page *page, const std::vector<EngravingItem *> &items) {
    if (items.empty()) {
        return;
    }

    auto options = items.front()->configuration()->debugging_options();
    if (!options.any_enabled()) {
        return;
    }

    IF_ASSERT_FAILED(page) { return; }

    Score *score = page->score();

    IF_ASSERT_FAILED(score) { return; }

    double scaling = painter.world_transform().m11() / items.front()->context_configuration()->gui_scaling();

    painter.save();

    for (const EngravingItem *item : items) {
        paint_element_debug(painter, item);
    }

    if (options.show_system_bounding_rects) {
        painter.set_brush(BrushStyle::NO_BRUSH);
        painter.set_pen(Pen(Color::black, 3.0 / scaling));

        for (const System *system : page->systems()) {
            PointF pt(system->ldata()->pos());
            double h = system->height() + system->min_bottom() + system->min_top();
            painter.translate(pt);
            RectF rect(0.0, -system->min_top(), system->width(), h);
            painter.draw_rect(rect);
            painter.translate(-pt);
        }
    }

    if (options.show_skylines) {
        for (const System *system : page->systems()) {
            for (const SysStaff *ss : system->staves()) {
                if (!ss->show()) {
                    continue;
                }

                PointF pt(system->ldata()->pos().x(), system->ldata()->pos().y() + ss->y());
                painter.translate(pt);
                ss->skyline().paint(painter, 3);
                painter.translate(-pt);
            }
        }
    }

    if (options.color_segment_shapes) {
        painter.set_pen(PenStyle::NO_PEN);

        for (const System *system : page->systems()) {
            for (const MeasureBase *mb : system->measures()) {
                if (!mb->is_measure()) {
                    continue;
                }

                const Measure *m = to_measure(mb);

                for (const Segment *s = m->first(); s; s = s->next()) {
                    if (!s->enabled()) {
                        continue;
                    }

                    painter.set_brush(color_for_pointer(s));

                    for (staff_idx_t i = 0; i < score->nstaves(); ++i) {
                        PainterPath path;
                        path.set_fill_rule(PainterPath::FillRule::WINDING_FILL);

                        Shape shape = s->shapes().at(i);
                        for (const RectF &rect : shape.elements()) {
                            path.add_rect(rect);
                        }

                        PointF pt(s->pos().x() + m->pos().x() + system->pos().x(), system->staff_ypage(i));
                        painter.translate(pt);
                        painter.draw_path(path);
                        painter.translate(-pt);
                    }
                }
            }
        }
    }

    if (options.show_segment_shapes) {
        painter.set_brush(BrushStyle::NO_BRUSH);
        Pen pen(Color(238, 238, 144), 2.0 / scaling);

        for (const System *system : page->systems()) {
            for (const MeasureBase *mb : system->measures()) {
                if (!mb->is_measure()) {
                    continue;
                }

                const Measure *m = to_measure(mb);

                for (const Segment *s = m->first(); s; s = s->next()) {
                    pen.set_style(s->enabled() ? PenStyle::SOLID_LINE : PenStyle::DOT_LINE);
                    painter.set_pen(pen);

                    for (staff_idx_t i = 0; i < score->nstaves(); ++i) {
                        PointF pt(s->pos().x() + m->pos().x() + system->pos().x(), system->staff_ypage(i));
                        painter.translate(pt);
                        s->shapes().at(i).paint(painter);
                        painter.translate(-pt);
                    }
                }
            }
        }
    }

    if (options.mark_empty_staff_visibility_overrides) {
        double spatium_value = score->style().spatium();

        for (const System *system : page->systems()) {
            for (const MeasureBase *mb : system->measures()) {
                if (!mb->is_measure()) {
                    continue;
                }

                const Measure *m = to_measure(mb);

                for (staff_idx_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
                    AutoOnOff hide_if_empty = m->hide_staff_if_empty(staff_idx);

                    Color fill_color;
                    switch (hide_if_empty) {
                        case AutoOnOff::ON: fill_color = Color(255, 0, 0, 128); break;
                        case AutoOnOff::OFF: fill_color = Color(0, 0, 255, 128); break;
                        default: continue;
                    }

                    painter.set_brush(fill_color);
                    painter.set_pen(PenStyle::NO_PEN);

                    painter.draw_rect(m->staff_page_bounding_rect(staff_idx).adjusted(0, -spatium_value, 0, spatium_value));
                }
            }
        }
    }

    if (options.mark_corrupted_measures && score->has_corrupted_measures()) {
        painter.set_pen(Pen(Color::red_color, 4.0));
        painter.set_brush(BrushStyle::NO_BRUSH);

        double spatium_value = score->style().spatium();

        for (const System *system : page->systems()) {
            for (const MeasureBase *mb : system->measures()) {
                if (!mb->is_measure()) {
                    continue;
                }

                const Measure *m = to_measure(mb);
                for (size_t staff_idx = 0; staff_idx < score->nstaves(); staff_idx++) {
                    if (m->corrupted(staff_idx)) {
                        painter.draw_rect(m->staff_page_bounding_rect(staff_idx).adjusted(0, -spatium_value, 0, spatium_value));
                    }
                }
            }
        }
    }

    painter.restore();
}
