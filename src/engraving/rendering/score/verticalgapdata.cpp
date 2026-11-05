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
#include "verticalgapdata.h"

#include "realfn.h"

#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/system.h"

#include "style/style.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

VerticalGapData::VerticalGapData(const MStyle *style, bool first, System *sys, const Staff *st, SysStaff *sst, Spacer *nextSpacer, double y)
    : style(style), system(sys), sysStaff(sst), staff(st), fixedHeight_(first) {
    if (fixedHeight_) {
        normalisedSpacing_ = style->styleAbsolute(Sid::staffUpperBorder);
        maxActualSpacing_ = normalisedSpacing_;
    } else {
        normalisedSpacing_ = system->y() + (sysStaff ? sysStaff->bbox().y() : 0.0) - y;
        maxActualSpacing_ = style->styleAbsolute(Sid::maxStaffSpread);

        Spacer *spacer{staff ? system->upSpacer(staff->idx(), nextSpacer) : nullptr};

        if (spacer) {
            fixedSpacer_ = spacer->spacerType() == SpacerType::FIXED;
            normalisedSpacing_ = std::max(normalisedSpacing_, spacer->absoluteGap());
            if (fixedSpacer_) {
                maxActualSpacing_ = normalisedSpacing_;
            }
        }
    }
}

//---------------------------------------------------------
//   updateFactor
//---------------------------------------------------------

void VerticalGapData::updateFactor(double factor) {
    if (fixedHeight_) {
        return;
    }
    double f = std::max(factor, factor_);
    normalisedSpacing_ *= factor_ / f;
    factor_ = f;
}

//---------------------------------------------------------
//   addSpaceBetweenSections
//---------------------------------------------------------

void VerticalGapData::addSpaceBetweenSections() {
    updateFactor(style->styleD(Sid::spreadSystem));
    if (!(fixedHeight_ | fixedSpacer_)) {
        maxActualSpacing_ = style->styleAbsolute(Sid::maxSystemSpread) / factor_;
    }
}

//---------------------------------------------------------
//   addSpaceAroundVBox
//---------------------------------------------------------

void VerticalGapData::addSpaceAroundVBox(bool above) {
    fixedHeight_ = true;
    factor_ = 1.0;
    normalisedSpacing_ = above ? style->styleAbsolute(Sid::frameSystemDistance) : style->styleAbsolute(Sid::systemFrameDistance);
    maxActualSpacing_ = normalisedSpacing_ / factor_;
}

//---------------------------------------------------------
//   addSpaceAroundNormalBracket
//---------------------------------------------------------

void VerticalGapData::addSpaceAroundNormalBracket() { updateFactor(style->styleD(Sid::spreadSquareBracket)); }

//---------------------------------------------------------
//   addSpaceAroundCurlyBracket
//---------------------------------------------------------

void VerticalGapData::addSpaceAroundCurlyBracket() { updateFactor(style->styleD(Sid::spreadCurlyBracket)); }

//---------------------------------------------------------
//   insideCurlyBracket
//---------------------------------------------------------

void VerticalGapData::insideCurlyBracket() { maxActualSpacing_ = style->styleAbsolute(Sid::maxAkkoladeDistance) / factor_; }

//---------------------------------------------------------
//   factor
//---------------------------------------------------------

double VerticalGapData::factor() const { return factor_; }

//---------------------------------------------------------
//   spacing
//    return normalised spacing
//---------------------------------------------------------

double VerticalGapData::spacing() const { return normalisedSpacing_ + addedNormalisedSpace_; }

//---------------------------------------------------------
//   addedSpace
//---------------------------------------------------------

double VerticalGapData::actualAddedSpace() const { return addedNormalisedSpace_ * factor(); }

//---------------------------------------------------------
//   addSpacing
//---------------------------------------------------------

double VerticalGapData::addSpacing(double step) {
    if (fixedHeight_ | fixedSpacer_) {
        return 0.0;
    }
    if (normalisedSpacing_ >= maxActualSpacing_) {
        normalisedSpacing_ = maxActualSpacing_;
        step = 0.0;
    } else {
        double newSpacing{normalisedSpacing_ + addedNormalisedSpace_ + step};
        if ((newSpacing >= maxActualSpacing_)) {
            step = maxActualSpacing_ - normalisedSpacing_ - addedNormalisedSpace_;
        }
    }
    addedNormalisedSpace_ += step;
    lastStep_ = step;
    return step;
}

//---------------------------------------------------------
//   isFixedHeight
//---------------------------------------------------------

bool VerticalGapData::isFixedHeight() const { return fixedHeight_ || muse::RealIsNull(normalisedSpacing_ - maxActualSpacing_); }

//---------------------------------------------------------
//   undoLastAddSpacing
//---------------------------------------------------------

void VerticalGapData::undoLastAddSpacing() {
    addedNormalisedSpace_ -= lastStep_;
    lastStep_ = 0.0;
}

//---------------------------------------------------------
//   addFillSpacing
//---------------------------------------------------------

double VerticalGapData::addFillSpacing(double step, double maxFill) {
    if (fixedSpacer_) {
        return 0.0;
    }
    double actStep{((step + fillSpacing_ / factor_) > maxFill) ? (maxFill - fillSpacing_ / factor_) : step};
    double res = addSpacing(actStep);
    fillSpacing_ += res * factor_;
    return res;
}

void VerticalGapData::setNormalisedSpacing(double newNormalisedSpacing) { normalisedSpacing_ = newNormalisedSpacing; }

//---------------------------------------------------------
//   deleteAll
//---------------------------------------------------------

void VerticalGapDataList::deleteAll() {
    for (auto vsd : *this) {
        delete vsd;
    }
}

//---------------------------------------------------------
//   sumStretchFactor
//---------------------------------------------------------

double VerticalGapDataList::sumStretchFactor() const {
    double sum{0.0};
    for (VerticalGapData *vsd : *this) {
        if (!vsd->isFixedHeight()) {
            sum += vsd->factor();
        }
    }
    return sum;
}

//---------------------------------------------------------
//   smallest
//---------------------------------------------------------

double VerticalGapDataList::smallest(double limit) const {
    VerticalGapData *vdp{nullptr};
    for (VerticalGapData *vgd : *this) {
        if (vgd->isFixedHeight()) {
            continue;
        }
        if ((std::ceil(limit) == std::ceil(vgd->spacing()))) {
            continue;
        }
        if (!vdp || (vgd->spacing() < vdp->spacing())) {
            vdp = vgd;
        }
    }
    return vdp ? vdp->spacing() : 0.0;
}
