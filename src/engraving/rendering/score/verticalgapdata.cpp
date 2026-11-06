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

#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/system.h"
#include "realfn.h"
#include "style/style.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

VerticalGapData::VerticalGapData(const MStyle *style, bool first, System *sys, const Staff *st, SysStaff *sst, Spacer *next_spacer,
                                 double y)
    : style(style), system(sys), sys_staff(sst), staff(st), fixed_height_(first) {
    if (fixed_height_) {
        normalised_spacing_ = style->style_absolute(Sid::STAFF_UPPER_BORDER);
        max_actual_spacing_ = normalised_spacing_;
    } else {
        normalised_spacing_ = system->y() + (sys_staff ? sys_staff->bbox().y() : 0.0) - y;
        max_actual_spacing_ = style->style_absolute(Sid::MAX_STAFF_SPREAD);

        Spacer *spacer{staff ? system->up_spacer(staff->idx(), next_spacer) : nullptr};

        if (spacer) {
            fixed_spacer_ = spacer->spacer_type() == SpacerType::FIXED;
            normalised_spacing_ = std::max(normalised_spacing_, spacer->absolute_gap());
            if (fixed_spacer_) {
                max_actual_spacing_ = normalised_spacing_;
            }
        }
    }
}

//---------------------------------------------------------
//   updateFactor
//---------------------------------------------------------

void VerticalGapData::update_factor(double factor) {
    if (fixed_height_) {
        return;
    }
    double f = std::max(factor, factor_);
    normalised_spacing_ *= factor_ / f;
    factor_ = f;
}

//---------------------------------------------------------
//   addSpaceBetweenSections
//---------------------------------------------------------

void VerticalGapData::add_space_between_sections() {
    update_factor(style->style_d(Sid::SPREAD_SYSTEM));
    if (!(fixed_height_ | fixed_spacer_)) {
        max_actual_spacing_ = style->style_absolute(Sid::MAX_SYSTEM_SPREAD) / factor_;
    }
}

//---------------------------------------------------------
//   addSpaceAroundVBox
//---------------------------------------------------------

void VerticalGapData::add_space_around_v_box(bool above) {
    fixed_height_ = true;
    factor_ = 1.0;
    normalised_spacing_ = above ? style->style_absolute(Sid::FRAME_SYSTEM_DISTANCE) : style->style_absolute(Sid::SYSTEM_FRAME_DISTANCE);
    max_actual_spacing_ = normalised_spacing_ / factor_;
}

//---------------------------------------------------------
//   addSpaceAroundNormalBracket
//---------------------------------------------------------

void VerticalGapData::add_space_around_normal_bracket() { update_factor(style->style_d(Sid::SPREAD_SQUARE_BRACKET)); }

//---------------------------------------------------------
//   addSpaceAroundCurlyBracket
//---------------------------------------------------------

void VerticalGapData::add_space_around_curly_bracket() { update_factor(style->style_d(Sid::SPREAD_CURLY_BRACKET)); }

//---------------------------------------------------------
//   insideCurlyBracket
//---------------------------------------------------------

void VerticalGapData::inside_curly_bracket() { max_actual_spacing_ = style->style_absolute(Sid::MAX_AKKOLADE_DISTANCE) / factor_; }

//---------------------------------------------------------
//   factor
//---------------------------------------------------------

double VerticalGapData::factor() const { return factor_; }

//---------------------------------------------------------
//   spacing
//    return normalised spacing
//---------------------------------------------------------

double VerticalGapData::spacing() const { return normalised_spacing_ + added_normalised_space_; }

//---------------------------------------------------------
//   addedSpace
//---------------------------------------------------------

double VerticalGapData::actual_added_space() const { return added_normalised_space_ * factor(); }

//---------------------------------------------------------
//   addSpacing
//---------------------------------------------------------

double VerticalGapData::add_spacing(double step) {
    if (fixed_height_ | fixed_spacer_) {
        return 0.0;
    }
    if (normalised_spacing_ >= max_actual_spacing_) {
        normalised_spacing_ = max_actual_spacing_;
        step = 0.0;
    } else {
        double new_spacing{normalised_spacing_ + added_normalised_space_ + step};
        if ((new_spacing >= max_actual_spacing_)) {
            step = max_actual_spacing_ - normalised_spacing_ - added_normalised_space_;
        }
    }
    added_normalised_space_ += step;
    last_step_ = step;
    return step;
}

//---------------------------------------------------------
//   isFixedHeight
//---------------------------------------------------------

bool VerticalGapData::is_fixed_height() const { return fixed_height_ || muse::real_is_null(normalised_spacing_ - max_actual_spacing_); }

//---------------------------------------------------------
//   undoLastAddSpacing
//---------------------------------------------------------

void VerticalGapData::undo_last_add_spacing() {
    added_normalised_space_ -= last_step_;
    last_step_ = 0.0;
}

//---------------------------------------------------------
//   addFillSpacing
//---------------------------------------------------------

double VerticalGapData::add_fill_spacing(double step, double max_fill) {
    if (fixed_spacer_) {
        return 0.0;
    }
    double act_step{((step + fill_spacing_ / factor_) > max_fill) ? (max_fill - fill_spacing_ / factor_) : step};
    double res = add_spacing(act_step);
    fill_spacing_ += res * factor_;
    return res;
}

void VerticalGapData::set_normalised_spacing(double new_normalised_spacing) { normalised_spacing_ = new_normalised_spacing; }

//---------------------------------------------------------
//   deleteAll
//---------------------------------------------------------

void VerticalGapDataList::delete_all() {
    for (auto vsd : *this) {
        delete vsd;
    }
}

//---------------------------------------------------------
//   sumStretchFactor
//---------------------------------------------------------

double VerticalGapDataList::sum_stretch_factor() const {
    double sum{0.0};
    for (VerticalGapData *vsd : *this) {
        if (!vsd->is_fixed_height()) {
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
        if (vgd->is_fixed_height()) {
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
