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

#include "mmrest.h"

#include "measure.h"
#include "part.h"
#include "score.h"
#include "system.h"

using namespace mu::engraving;

//---------------------------------------------------------
//    MMRest
//--------------------------------------------------------

MMRest::MMRest(DummyParentOr<Segment> s) : Rest(ElementType::MMREST, s) {
    number_visible_ = true;
    set_placement(PlacementV::ABOVE);
}

MMRest::MMRest(const MMRest &r, bool link) : Rest(r, link) { number_visible_ = r.number_visible_; }

bool MMRest::should_show_number_by_default() const {
    bool should_show = is_old_style() && ldata()->number == 1 ? style().style_b(Sid::SINGLE_MEASURE_MM_REST_SHOW_NUMBER) : true;

    const Part *item_part = part();
    const System *system = measure()->system();
    bool is_top_staff_of_part_and_centering_is_active = item_part->nstaves() > 1 && staff() == item_part->staves().front() &&
                                                        (system && system->next_visible_staff(staff_idx()) != muse::nidx) &&
                                                        style().style_b(Sid::MM_REST_BETWEEN_STAVES);

    return should_show && !is_top_staff_of_part_and_centering_is_active;
}

bool MMRest::show_number() const { return should_show_number_by_default() && number_visible_; }

bool MMRest::is_old_style() const {
    int number = ldata()->number;
    return (style().style_b(Sid::OLD_STYLE_MULTI_MEASURE_RESTS) && number <= style().style_i(Sid::MM_REST_OLD_STYLE_MAX_MEASURES)) ||
           (number == 1 && style().style_b(Sid::SINGLE_MEASURE_MM_REST_USE_NORMAL_REST));
}

RectF MMRest::number_rect() const { return sym_bbox(ldata()->number_sym); }

PointF MMRest::number_pos() const {
    RectF num_b_box = number_rect();
    double x = 0.5 * (ldata()->rest_width - num_b_box.width());
    double y = -pos().y() + y_number_pos() - 0.5 * num_b_box.height();
    return PointF(x, y);
}

double MMRest::y_number_pos() const { return ldata()->y_number_pos + absolute_from_spatium(number_offset_); }

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue MMRest::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::MMREST_NUMBER_OFFSET: return 0.0_sp;
        case Pid::MMREST_NUMBER_VISIBLE: return true;
        default: return Rest::property_default(property_id);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue MMRest::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::MMREST_NUMBER_OFFSET: return number_offset_;
        case Pid::MMREST_NUMBER_VISIBLE: return number_visible_;
        default: return Rest::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool MMRest::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::MMREST_NUMBER_OFFSET:
            number_offset_ = v.value<Spatium>();
            trigger_layout();
            break;
        case Pid::MMREST_NUMBER_VISIBLE:
            number_visible_ = v.to_bool();
            trigger_layout();
            break;
        default: return Rest::set_property(property_id, v);
    }
    return true;
}
