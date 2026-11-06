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

#include "measurerepeat.h"

#include "log.h"
#include "measure.h"
#include "staff.h"
#include "style/style.h"
#include "translation.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   MeasureRepeat
//---------------------------------------------------------

MeasureRepeat::MeasureRepeat(DummyParentOr<Segment> parent) : Rest(ElementType::MEASURE_REPEAT, parent), num_measures_(0) {
    // however many measures the group, the element itself is always exactly the duration of its containing measure
    set_duration_type(DurationType::V_MEASURE);
}

void MeasureRepeat::set_num_measures(int n) {
    IF_ASSERT_FAILED(n <= ::mu::engraving::MeasureRepeat::max_num_measures) {
        num_measures_ = ::mu::engraving::MeasureRepeat::max_num_measures;
        return;
    }

    num_measures_ = n;
}

//---------------------------------------------------------
//   firstMeasureOfGroup
//---------------------------------------------------------

Measure *MeasureRepeat::first_measure_of_group() const { return measure()->first_of_measure_repeat_group(staff_idx()); }

const Measure *MeasureRepeat::referring_measure(const Measure *measure) const {
    IF_ASSERT_FAILED(measure) { return nullptr; }

    const Measure *first_measure = first_measure_of_group();
    if (!first_measure) {
        return nullptr;
    }

    const Measure *referring_measure = first_measure->prev_measure();

    int measures_back = num_measures_ - measure->measure_repeat_count(staff_idx());

    for (int i = 0; i < measures_back && referring_measure; ++i) {
        referring_measure = referring_measure->prev_measure();
    }

    return referring_measure;
}

//---------------------------------------------------------
//   numberRect
///   returns the measure repeat number's bounding rectangle
//---------------------------------------------------------

PointF MeasureRepeat::number_position(const RectF &number_bbox) const {
    double x = (sym_bbox(ldata()->sym_id).width() - number_bbox.width()) * .5;
    // -pos().y(): relative to topmost staff line
    // - 0.5 * r.height(): relative to the baseline of the number symbol
    // (rather than the center)
    double staff_top = -pos().y();
    // Single line staff barlines extend above top of staff
    if (staff_type() && staff_type()->lines() == 1) {
        staff_top -= 2.0 * spatium();
    }
    double y_number_pos = absolute_from_spatium(style().style_s(Sid::MEASURE_REPEAT_NUMBER_POS) + number_offset_);
    double y = std::min(staff_top, -sym_bbox(ldata()->sym_id).height() / 2) + y_number_pos - 0.5 * number_bbox.height();

    return PointF(x, y);
}

RectF MeasureRepeat::number_rect() const {
    RectF r = sym_bbox(ldata()->number_sym);
    r.translate(number_position(r));
    return r;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue MeasureRepeat::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::MEASURE_REPEAT_NUMBER_OFFSET: return 0.0_sp;
        default: return Rest::property_default(property_id);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue MeasureRepeat::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SUBTYPE: return num_measures();
        case Pid::MEASURE_REPEAT_NUMBER_OFFSET: return number_offset_;
        default: return Rest::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool MeasureRepeat::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SUBTYPE: set_num_measures(v.to_int()); break;
        case Pid::MEASURE_REPEAT_NUMBER_OFFSET:
            set_number_offset(v.value<Spatium>());
            trigger_layout();
            break;
        default: return Rest::set_property(property_id, v);
    }
    return true;
}

//---------------------------------------------------------
//   ticks
//---------------------------------------------------------

Fraction MeasureRepeat::ticks() const {
    if (measure()) {
        return measure()->stretched_len(staff());
    }
    return Fraction(0, 1);
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString MeasureRepeat::subtype_user_name() const {
    return muse::TranslatableString("engraving", "%Ln measure(s)", nullptr, num_measures());
}
} // namespace mu::engraving
