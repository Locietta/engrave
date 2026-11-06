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
#ifndef MU_ENGRAVING_READPAGEFORMAT_H
#define MU_ENGRAVING_READPAGEFORMAT_H

#include "draw/types/geometry.h"

namespace mu::engraving {
struct MStyle;
struct XmlReader;
} // namespace mu::engraving

namespace mu::engraving::compat {
//---------------------------------------------------------
//   @@ PageFormat
//---------------------------------------------------------

struct PageFormat {
private:
    muse::SizeF size_value;
    double printable_width_value;  // _width - left margin - right margin
    double even_left_margin_value; // values in inch
    double odd_left_margin_value;
    double even_top_margin_value;
    double even_bottom_margin_value;
    double odd_top_margin_value;
    double odd_bottom_margin_value;
    bool twosided_value;

public:
    PageFormat() {}

    const muse::SizeF &size() const { return size_value; } // size in inch

    double width() const { return size_value.width(); }

    double height() const { return size_value.height(); }

    void set_size(const muse::SizeF &s) { size_value = s; }

    void read206(XmlReader &);

    double even_left_margin() const { return even_left_margin_value; }

    double odd_left_margin() const { return odd_left_margin_value; }

    double even_top_margin() const { return even_top_margin_value; }

    double even_bottom_margin() const { return even_bottom_margin_value; }

    double odd_top_margin() const { return odd_top_margin_value; }

    double odd_bottom_margin() const { return odd_bottom_margin_value; }

    double printable_width() const { return printable_width_value; }

    void set_even_left_margin(double val) { even_left_margin_value = val; }

    void set_odd_left_margin(double val) { odd_left_margin_value = val; }

    void set_even_top_margin(double val) { even_top_margin_value = val; }

    void set_even_bottom_margin(double val) { even_bottom_margin_value = val; }

    void set_odd_top_margin(double val) { odd_top_margin_value = val; }

    void set_odd_bottom_margin(double val) { odd_bottom_margin_value = val; }

    void set_printable_width(double val) { printable_width_value = val; }

    bool twosided() const { return twosided_value; }

    void set_twosided(bool val) { twosided_value = val; }

    // convenience functions
    double even_right_margin() const { return size_value.width() - printable_width_value - even_left_margin_value; }

    double odd_right_margin() const { return size_value.width() - printable_width_value - odd_left_margin_value; }
};

void init_page_format(MStyle *style, PageFormat *pf);
void set_page_format(MStyle *style, const PageFormat &pf);
void read_page_format206(MStyle *style, XmlReader &e);
} // namespace mu::engraving::compat
#endif // MU_ENGRAVING_READPAGEFORMAT_H
