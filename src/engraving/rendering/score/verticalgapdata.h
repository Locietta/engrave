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
#ifndef MU_ENGRAVING_VERTICALGAPDATALIST_DEV_H
#define MU_ENGRAVING_VERTICALGAPDATALIST_DEV_H

#include <vector>

namespace mu::engraving {
class MStyle;
class Spacer;
class Staff;
class SysStaff;
class System;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
//---------------------------------------------------------
//   VerticalStretchData
//    helper class for spreading staves over a page
//---------------------------------------------------------

class VerticalGapData {
public:
    const MStyle *style = nullptr;
    System *system = nullptr;
    SysStaff *sysStaff = nullptr;
    const Staff *staff = nullptr;

    VerticalGapData(const MStyle *style, bool first, System *sys, const Staff *st, SysStaff *sst, Spacer *nextSpacer, double y);

    void addSpaceBetweenSections();
    void addSpaceAroundVBox(bool above);
    void addSpaceAroundNormalBracket();
    void addSpaceAroundCurlyBracket();
    void insideCurlyBracket();

    double factor() const;
    double spacing() const;
    double actualAddedSpace() const;

    double addSpacing(double step);
    bool isFixedHeight() const;
    void undoLastAddSpacing();
    double addFillSpacing(double step, double maxFill);

    void setNormalisedSpacing(double newNormalisedSpacing);

private:
    void updateFactor(double factor);

    bool fixedHeight_ = false;
    bool fixedSpacer_ = false;
    double factor_ = 1.0;
    double normalisedSpacing_ = 0.0;
    double maxActualSpacing_ = 0.0;
    double addedNormalisedSpace_ = 0.0;
    double fillSpacing_ = 0.0;
    double lastStep_ = 0.0;
};

//---------------------------------------------------------
//   VerticalStretchDataList
//    helper class for spreading staves over a page
//---------------------------------------------------------

class VerticalGapDataList : public std::vector<VerticalGapData *> {
public:
    void deleteAll();
    double sumStretchFactor() const;
    double smallest(double limit = -1.0) const;
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_VERTICALGAPDATALIST_DEV_H
