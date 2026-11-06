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
#include "pagestyle.h"

#include "style.h"

namespace mu::engraving {
const std::vector<Sid> &page_styles() {
    static const std::vector<Sid> styles{Sid::PAGE_WIDTH,
                                         Sid::PAGE_HEIGHT,
                                         Sid::PAGE_PRINTABLE_WIDTH,
                                         Sid::PAGE_EVEN_TOP_MARGIN,
                                         Sid::PAGE_EVEN_BOTTOM_MARGIN,
                                         Sid::PAGE_EVEN_LEFT_MARGIN,
                                         Sid::PAGE_ODD_TOP_MARGIN,
                                         Sid::PAGE_ODD_BOTTOM_MARGIN,
                                         Sid::PAGE_ODD_LEFT_MARGIN,
                                         Sid::PAGE_TWOSIDED,
                                         Sid::SPATIUM};

    return styles;
}

PageSizeGetAccessor::PageSizeGetAccessor(const MStyle &style) : style_(style) {}

double PageSizeGetAccessor::width() const { return style_.style_d(Sid::PAGE_WIDTH); }

double PageSizeGetAccessor::height() const { return style_.style_d(Sid::PAGE_HEIGHT); }

double PageSizeGetAccessor::printable_width() const { return style_.style_d(Sid::PAGE_PRINTABLE_WIDTH); }

double PageSizeGetAccessor::even_top_margin() const { return style_.style_d(Sid::PAGE_EVEN_TOP_MARGIN); }

double PageSizeGetAccessor::even_bottom_margin() const { return style_.style_d(Sid::PAGE_EVEN_BOTTOM_MARGIN); }

double PageSizeGetAccessor::even_left_margin() const { return style_.style_d(Sid::PAGE_EVEN_LEFT_MARGIN); }

double PageSizeGetAccessor::odd_top_margin() const { return style_.style_d(Sid::PAGE_ODD_TOP_MARGIN); }

double PageSizeGetAccessor::odd_bottom_margin() const { return style_.style_d(Sid::PAGE_ODD_BOTTOM_MARGIN); }

double PageSizeGetAccessor::odd_left_margin() const { return style_.style_d(Sid::PAGE_ODD_LEFT_MARGIN); }

double PageSizeGetAccessor::twosided() const { return style_.style_d(Sid::PAGE_TWOSIDED); }

double PageSizeGetAccessor::spatium() const { return style_.style_d(Sid::SPATIUM); }

PageSizeSetAccessor::PageSizeSetAccessor(MStyle &style) : style_(style) {}

double PageSizeSetAccessor::width() const { return style_.style_d(Sid::PAGE_WIDTH); }

double PageSizeSetAccessor::height() const { return style_.style_d(Sid::PAGE_HEIGHT); }

double PageSizeSetAccessor::printable_width() const { return style_.style_d(Sid::PAGE_PRINTABLE_WIDTH); }

double PageSizeSetAccessor::even_top_margin() const { return style_.style_d(Sid::PAGE_EVEN_TOP_MARGIN); }

double PageSizeSetAccessor::even_bottom_margin() const { return style_.style_d(Sid::PAGE_EVEN_BOTTOM_MARGIN); }

double PageSizeSetAccessor::even_left_margin() const { return style_.style_d(Sid::PAGE_EVEN_LEFT_MARGIN); }

double PageSizeSetAccessor::odd_top_margin() const { return style_.style_d(Sid::PAGE_ODD_TOP_MARGIN); }

double PageSizeSetAccessor::odd_bottom_margin() const { return style_.style_d(Sid::PAGE_ODD_BOTTOM_MARGIN); }

double PageSizeSetAccessor::odd_left_margin() const { return style_.style_d(Sid::PAGE_ODD_LEFT_MARGIN); }

double PageSizeSetAccessor::twosided() const { return style_.style_d(Sid::PAGE_TWOSIDED); }

double PageSizeSetAccessor::spatium() const { return style_.style_d(Sid::SPATIUM); }

void PageSizeSetAccessor::set_width(double v) { style_.set(Sid::PAGE_WIDTH, v); }

void PageSizeSetAccessor::set_height(double v) { style_.set(Sid::PAGE_HEIGHT, v); }

void PageSizeSetAccessor::set_printable_width(double v) { style_.set(Sid::PAGE_PRINTABLE_WIDTH, v); }

void PageSizeSetAccessor::set_even_top_margin(double v) { style_.set(Sid::PAGE_EVEN_TOP_MARGIN, v); }

void PageSizeSetAccessor::set_even_bottom_margin(double v) { style_.set(Sid::PAGE_EVEN_BOTTOM_MARGIN, v); }

void PageSizeSetAccessor::set_even_left_margin(double v) { style_.set(Sid::PAGE_EVEN_LEFT_MARGIN, v); }

void PageSizeSetAccessor::set_odd_top_margin(double v) { style_.set(Sid::PAGE_ODD_TOP_MARGIN, v); }

void PageSizeSetAccessor::set_odd_bottom_margin(double v) { style_.set(Sid::PAGE_ODD_BOTTOM_MARGIN, v); }

void PageSizeSetAccessor::set_odd_left_margin(double v) { style_.set(Sid::PAGE_ODD_LEFT_MARGIN, v); }

void PageSizeSetAccessor::set_twosided(double v) { style_.set(Sid::PAGE_TWOSIDED, v); }

void PageSizeSetAccessor::set_spatium(double v) { style_.set(Sid::SPATIUM, v); }
} // namespace mu::engraving
