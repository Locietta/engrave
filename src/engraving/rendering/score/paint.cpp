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
#include "paint.h"

#include "debugpaint.h"
#include "dom/engravingitem.h"
#include "dom/page.h"
#include "dom/score.h"
#include "draw/painter.h"
#include "log.h"
#include "tdraw.h"

using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void Paint::paint_score(Painter *painter, Score *score, const IScoreRenderer::ScorePaintOptions &opt) {
    TRACEFUNC;
    if (!score) {
        return;
    }

    const std::vector<Page *> &pages = score->pages();
    if (pages.empty()) {
        return;
    }

    //! NOTE This is DPI of paint device,  ex screen, image, printer and etc.
    //! Should be set, but if not set, we will use our default DPI.
    const int device_dpi = opt.device_dpi > 0 ? opt.device_dpi : mu::engraving::dpi;

    //! NOTE Depending on the view mode,
    //! if the view mode is PAGE, then this is one page size (ex A4),
    //! if someone a continuous mode, then this is the size of the entire score.
    SizeF page_size = page_size_inch(score);

    // Setup Painter
    painter->set_antialiasing(true);

    //! NOTE To draw on the screen, no need to adjust the viewport,
    //! to draw on others (pdf, png, printer), we need to set the viewport
    if (opt.is_set_viewport) {
        painter->set_viewport(RectF(0.0, 0.0, std::lrint(page_size.width() * device_dpi), std::lrint(page_size.height() * device_dpi)));
        painter->set_window(
            RectF(0.0, 0.0, std::lrint(page_size.width() * engraving::dpi), std::lrint(page_size.height() * engraving::dpi)));
    }

    // Setup score draw system
    mu::engraving::MScore::pdf_printing = opt.is_printing;

    const bool was_printing = score->printing();
    score->set_printing(opt.is_printing);

    // Setup page counts
    int from_page = opt.from_page >= 0 ? opt.from_page : 0;
    int to_page = (opt.to_page >= 0 && opt.to_page < int(pages.size())) ? opt.to_page : (int(pages.size()) - 1);

    for (int copy = 0; copy < opt.copy_count; ++copy) {
        bool first_page = true;
        for (int pi = from_page; pi <= to_page; ++pi) {
            Page *page = pages.at(pi);

            PointF page_pos = page->pos();
            RectF page_rect = page->ldata()->bbox();

            //! NOTE Trim page margins, if need
            if (opt.trim_margin_pixel_size >= 0) {
                double trim_margin = static_cast<double>(opt.trim_margin_pixel_size);
                page_rect = page->tbbox().adjusted(-trim_margin, -trim_margin, trim_margin, trim_margin);
            }

            //! NOTE Check draw rect, usually for optimisation drawing on screen (draw only what we see)
            RectF draw_rect;
            RectF page_abs_rect = page_rect.translated(page_pos);
            if (opt.frame_rect.is_valid()) {
                if (page_abs_rect.right() < opt.frame_rect.left()) {
                    continue;
                }

                if (page_abs_rect.left() > opt.frame_rect.right()) {
                    break;
                }

                draw_rect = opt.frame_rect;
            } else {
                draw_rect = page_abs_rect;
            }

            //! NOTE Notify about new page (usually for paged paint device, ex pdf, printer)
            if (!first_page) {
                if (opt.on_new_page) {
                    opt.on_new_page();
                }
            }
            first_page = false;

            painter->begin_object("page_" + std::to_string(pi));

            if (opt.is_printing && painter->has_clipping()) {
                // prevent elements from being drawn off the edge of the page (e.g. too many staves),
                // for now only in "Publish" mode (opt.isPrinting == true)
                painter->set_clip_rect(page_abs_rect.intersected(draw_rect));
            }

            if (opt.is_multi_page) {
                painter->translate(page_pos);
            } else if (opt.trim_margin_pixel_size >= 0) {
                painter->translate(-page_rect.top_left());
            }

            // Draw page sheet
            if (opt.on_paint_page_sheet) {
                opt.on_paint_page_sheet(painter, page, page_rect);
            } else if (opt.print_page_background) {
                painter->fill_rect(page_rect, Color::white);
            }

            // Draw page elements
            bool disable_clipping = false;

            if (!painter->has_clipping()) {
                painter->set_clipping(true);
                painter->set_clip_rect(page_rect);
                disable_clipping = true;
            }

            std::vector<EngravingItem *> elements = page->items(draw_rect.translated(-page_pos));
            paint_items(*painter, elements, opt);

            if (disable_clipping) {
                painter->set_clipping(false);
            }

            if (!opt.is_printing) {
                DebugPaint::paint_page_debug(*painter, page, elements);
            }

            painter->end_object(); // page

            if (opt.is_multi_page) {
                painter->translate(-page_pos);
            } else if (opt.trim_margin_pixel_size >= 0) {
                painter->translate(page_rect.top_left());
            }

            if ((copy + 1) < opt.copy_count) {
                //! NOTE Notify about new page (usually for paged paint device, ex pdf, printer)
                //! for next copy
                if (opt.on_new_page) {
                    opt.on_new_page();
                }
            }
        }
    }

    score->set_printing(was_printing);
}

SizeF Paint::page_size_inch(const Score *score) {
    if (!score) {
        return SizeF();
    }

    //! NOTE If now it is not PAGE view mode,
    //! then the page sizes will differ from the standard sizes (in PAGE view mode)
    if (score->npages() > 0) {
        const Page *page = score->pages().front();
        return SizeF(page->ldata()->bbox().width() / mu::engraving::dpi, page->ldata()->bbox().height() / mu::engraving::dpi);
    }

    return SizeF(score->style().style_d(Sid::PAGE_WIDTH), score->style().style_d(Sid::PAGE_HEIGHT));
}

SizeF Paint::page_size_inch(const Score *score, const IScoreRenderer::ScorePaintOptions &opt) {
    if (!score) {
        return SizeF();
    }

    int page_no = opt.from_page >= 0 ? opt.from_page : 0;
    if (page_no >= int(score->npages())) {
        return SizeF();
    }

    const Page *page = score->pages().at(page_no);

    RectF page_rect = page->ldata()->bbox();

    //! NOTE Trim page margins, if need
    if (opt.trim_margin_pixel_size >= 0) {
        double trim_margin = static_cast<double>(opt.trim_margin_pixel_size);
        page_rect = page->tbbox().adjusted(-trim_margin, -trim_margin, trim_margin, trim_margin);
    }

    return page_rect.size() / mu::engraving::dpi;
}

void Paint::paint_item(Painter &painter, const EngravingItem *item, const PaintOptions &opt) {
    TRACEFUNC;
    if (item->ldata()->is_skip_draw()) {
        return;
    }
    item->item_discovered = false;
    PointF item_position(item->page_pos());

    painter.translate(item_position);
    TDraw::draw_item(item, &painter, opt);
    painter.translate(-item_position);
}

void Paint::paint_items(Painter &painter, const std::vector<EngravingItem *> &items, const PaintOptions &opt) {
    TRACEFUNC;
    std::vector<EngravingItem *> sorted_items(items.begin(), items.end());

    std::sort(sorted_items.begin(), sorted_items.end(), mu::engraving::element_less_than);

    for (const EngravingItem *item : sorted_items) {
        if (!item->is_interaction_available()) {
            continue;
        }

        paint_item(painter, item, opt);
    }
}
