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
#include "pageformat.h"

#include "dom/mscore.h"
#include "rw/xmlreader.h"
#include "style/style.h"

namespace mu::engraving::compat {
//---------------------------------------------------------
//   read
//  <page-layout>
//      <page-height>
//      <page-width>
//      <landscape>1</landscape>
//      <page-margins type="both">
//         <left-margin>28.3465</left-margin>
//         <right-margin>28.3465</right-margin>
//         <top-margin>28.3465</top-margin>
//         <bottom-margin>56.6929</bottom-margin>
//         </page-margins>
//      </page-layout>
//---------------------------------------------------------

void PageFormat::read206(XmlReader &e) {
    double odd_right_margin = 0.0;
    double even_right_margin = 0.0;
    AsciiStringView type;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "page-margins") {
            type = e.ascii_attribute("type", "both");
            double lm = 0.0, rm = 0.0, tm = 0.0, bm = 0.0;
            while (e.read_next_start_element()) {
                const AsciiStringView t(e.name());
                double val = e.read_double() * 0.5 / ::mu::engraving::ppi;
                if (t == "left-margin") {
                    lm = val;
                } else if (t == "right-margin") {
                    rm = val;
                } else if (t == "top-margin") {
                    tm = val;
                } else if (t == "bottom-margin") {
                    bm = val;
                } else {
                    e.unknown();
                }
            }
            twosided_value = type == "odd" || type == "even";
            if (type == "odd" || type == "both") {
                odd_left_margin_value = lm;
                odd_right_margin = rm;
                odd_top_margin_value = tm;
                odd_bottom_margin_value = bm;
            }
            if (type == "even" || type == "both") {
                even_left_margin_value = lm;
                even_right_margin = rm;
                even_top_margin_value = tm;
                even_bottom_margin_value = bm;
            }
        } else if (tag == "page-height") {
            size_value.set_height(e.read_double() * 0.5 / ::mu::engraving::ppi);
        } else if (tag == "page-width") {
            size_value.set_width(e.read_double() * .5 / ::mu::engraving::ppi);
        } else {
            e.unknown();
        }
    }
    double w1 = size_value.width() - odd_left_margin_value - odd_right_margin;
    double w2 = size_value.width() - even_left_margin_value - even_right_margin;
    printable_width_value = std::min(w1, w2); // silently adjust right margins
}

void init_page_format(MStyle *style, PageFormat *pf) {
    SizeF sz;
    sz.set_width(style->value(Sid::PAGE_WIDTH).to_real());
    sz.set_height(style->value(Sid::PAGE_HEIGHT).to_real());
    pf->set_size(sz);
    pf->set_printable_width(style->value(Sid::PAGE_PRINTABLE_WIDTH).to_real());
    pf->set_even_left_margin(style->value(Sid::PAGE_EVEN_LEFT_MARGIN).to_real());
    pf->set_odd_left_margin(style->value(Sid::PAGE_ODD_LEFT_MARGIN).to_real());
    pf->set_even_top_margin(style->value(Sid::PAGE_EVEN_TOP_MARGIN).to_real());
    pf->set_even_bottom_margin(style->value(Sid::PAGE_EVEN_BOTTOM_MARGIN).to_real());
    pf->set_odd_top_margin(style->value(Sid::PAGE_ODD_TOP_MARGIN).to_real());
    pf->set_odd_bottom_margin(style->value(Sid::PAGE_ODD_BOTTOM_MARGIN).to_real());
    pf->set_twosided(style->value(Sid::PAGE_TWOSIDED).to_bool());
}

void set_page_format(MStyle *style, const PageFormat &pf) {
    style->set(Sid::PAGE_WIDTH, pf.size().width());
    style->set(Sid::PAGE_HEIGHT, pf.size().height());
    style->set(Sid::PAGE_PRINTABLE_WIDTH, pf.printable_width());
    style->set(Sid::PAGE_EVEN_LEFT_MARGIN, pf.even_left_margin());
    style->set(Sid::PAGE_ODD_LEFT_MARGIN, pf.odd_left_margin());
    style->set(Sid::PAGE_EVEN_TOP_MARGIN, pf.even_top_margin());
    style->set(Sid::PAGE_EVEN_BOTTOM_MARGIN, pf.even_bottom_margin());
    style->set(Sid::PAGE_ODD_TOP_MARGIN, pf.odd_top_margin());
    style->set(Sid::PAGE_ODD_BOTTOM_MARGIN, pf.odd_bottom_margin());
    style->set(Sid::PAGE_TWOSIDED, pf.twosided());
}

void read_page_format206(MStyle *style, XmlReader &e) {
    PageFormat pf;
    init_page_format(style, &pf);
    pf.read206(e);
    set_page_format(style, pf);
}
} // namespace mu::engraving::compat
