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
const std::vector<Sid> &pageStyles() {
    static const std::vector<Sid> styles{Sid::pageWidth,
                                         Sid::pageHeight,
                                         Sid::pagePrintableWidth,
                                         Sid::pageEvenTopMargin,
                                         Sid::pageEvenBottomMargin,
                                         Sid::pageEvenLeftMargin,
                                         Sid::pageOddTopMargin,
                                         Sid::pageOddBottomMargin,
                                         Sid::pageOddLeftMargin,
                                         Sid::pageTwosided,
                                         Sid::spatium};

    return styles;
}

PageSizeGetAccessor::PageSizeGetAccessor(const MStyle &style) : style_(style) {}

double PageSizeGetAccessor::width() const { return style_.styleD(Sid::pageWidth); }

double PageSizeGetAccessor::height() const { return style_.styleD(Sid::pageHeight); }

double PageSizeGetAccessor::printableWidth() const { return style_.styleD(Sid::pagePrintableWidth); }

double PageSizeGetAccessor::evenTopMargin() const { return style_.styleD(Sid::pageEvenTopMargin); }

double PageSizeGetAccessor::evenBottomMargin() const { return style_.styleD(Sid::pageEvenBottomMargin); }

double PageSizeGetAccessor::evenLeftMargin() const { return style_.styleD(Sid::pageEvenLeftMargin); }

double PageSizeGetAccessor::oddTopMargin() const { return style_.styleD(Sid::pageOddTopMargin); }

double PageSizeGetAccessor::oddBottomMargin() const { return style_.styleD(Sid::pageOddBottomMargin); }

double PageSizeGetAccessor::oddLeftMargin() const { return style_.styleD(Sid::pageOddLeftMargin); }

double PageSizeGetAccessor::twosided() const { return style_.styleD(Sid::pageTwosided); }

double PageSizeGetAccessor::spatium() const { return style_.styleD(Sid::spatium); }

PageSizeSetAccessor::PageSizeSetAccessor(MStyle &style) : style_(style) {}

double PageSizeSetAccessor::width() const { return style_.styleD(Sid::pageWidth); }

double PageSizeSetAccessor::height() const { return style_.styleD(Sid::pageHeight); }

double PageSizeSetAccessor::printableWidth() const { return style_.styleD(Sid::pagePrintableWidth); }

double PageSizeSetAccessor::evenTopMargin() const { return style_.styleD(Sid::pageEvenTopMargin); }

double PageSizeSetAccessor::evenBottomMargin() const { return style_.styleD(Sid::pageEvenBottomMargin); }

double PageSizeSetAccessor::evenLeftMargin() const { return style_.styleD(Sid::pageEvenLeftMargin); }

double PageSizeSetAccessor::oddTopMargin() const { return style_.styleD(Sid::pageOddTopMargin); }

double PageSizeSetAccessor::oddBottomMargin() const { return style_.styleD(Sid::pageOddBottomMargin); }

double PageSizeSetAccessor::oddLeftMargin() const { return style_.styleD(Sid::pageOddLeftMargin); }

double PageSizeSetAccessor::twosided() const { return style_.styleD(Sid::pageTwosided); }

double PageSizeSetAccessor::spatium() const { return style_.styleD(Sid::spatium); }

void PageSizeSetAccessor::setWidth(double v) { style_.set(Sid::pageWidth, v); }

void PageSizeSetAccessor::setHeight(double v) { style_.set(Sid::pageHeight, v); }

void PageSizeSetAccessor::setPrintableWidth(double v) { style_.set(Sid::pagePrintableWidth, v); }

void PageSizeSetAccessor::setEvenTopMargin(double v) { style_.set(Sid::pageEvenTopMargin, v); }

void PageSizeSetAccessor::setEvenBottomMargin(double v) { style_.set(Sid::pageEvenBottomMargin, v); }

void PageSizeSetAccessor::setEvenLeftMargin(double v) { style_.set(Sid::pageEvenLeftMargin, v); }

void PageSizeSetAccessor::setOddTopMargin(double v) { style_.set(Sid::pageOddTopMargin, v); }

void PageSizeSetAccessor::setOddBottomMargin(double v) { style_.set(Sid::pageOddBottomMargin, v); }

void PageSizeSetAccessor::setOddLeftMargin(double v) { style_.set(Sid::pageOddLeftMargin, v); }

void PageSizeSetAccessor::setTwosided(double v) { style_.set(Sid::pageTwosided, v); }

void PageSizeSetAccessor::setSpatium(double v) { style_.set(Sid::spatium, v); }
} // namespace mu::engraving
