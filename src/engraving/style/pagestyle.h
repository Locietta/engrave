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
#pragma once

#include <vector>

#include "styledef.h"

namespace mu::engraving {
const std::vector<Sid> &page_styles();

class MStyle;

class PageSizeGetAccessor {
public:
    PageSizeGetAccessor(const MStyle &style);

    double width() const;
    double height() const;
    double printable_width() const;
    double even_top_margin() const;
    double even_bottom_margin() const;
    double even_left_margin() const;
    double odd_top_margin() const;
    double odd_bottom_margin() const;
    double odd_left_margin() const;
    double twosided() const;
    double spatium() const;

private:
    const MStyle &style_;
};

class PageSizeSetAccessor {
public:
    PageSizeSetAccessor(MStyle &style);

    double width() const;
    double height() const;
    double printable_width() const;
    double even_top_margin() const;
    double even_bottom_margin() const;
    double even_left_margin() const;
    double odd_top_margin() const;
    double odd_bottom_margin() const;
    double odd_left_margin() const;
    double twosided() const;
    double spatium() const;

    void set_width(double v);
    void set_height(double v);
    void set_printable_width(double v);
    void set_even_top_margin(double v);
    void set_even_bottom_margin(double v);
    void set_even_left_margin(double v);
    void set_odd_top_margin(double v);
    void set_odd_bottom_margin(double v);
    void set_odd_left_margin(double v);
    void set_twosided(double v);
    void set_spatium(double v);

private:
    MStyle &style_;
};
} // namespace mu::engraving
