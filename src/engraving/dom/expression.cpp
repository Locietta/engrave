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
#include "expression.h"

#include "chord.h"
#include "dynamic.h"
#include "note.h"
#include "segment.h"
#include "score.h"
#include "stafftext.h"

namespace mu::engraving {
static const ElementStyle expression_style{
    {Sid::EXPRESSION_PLACEMENT, Pid::PLACEMENT},
    {Sid::EXPRESSION_MIN_DISTANCE, Pid::MIN_DISTANCE},
    {Sid::SNAP_TO_DYNAMICS, Pid::SNAP_TO_DYNAMICS},
};

Expression::Expression(DummyParentOr<Segment> parent)
    : TextBase(ElementType::EXPRESSION, parent, TextStyleType::EXPRESSION, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::expression_style);
}

Expression::Expression(const Expression &expression) : TextBase(expression) { snap_to_dynamics_value = expression.snap_to_dynamics_value; }

PropertyValue Expression::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::EXPRESSION;
        default: return TextBase::property_default(id);
    }
}

double Expression::compute_dynamic_expression_distance(const Dynamic *snapped_dyn) const {
    IF_ASSERT_FAILED(snapped_dyn) { return 0.0; }
    // We are essentially faking the kerning behaviour of dynamic VS expression text
    // There's no other way to do this because the dynamic is a different font.
    String dynamic_text_string = snapped_dyn->xml_text();
    String f = String::from_std_string("<sym>dynamicForte</sym>");
    double distance = (dynamic_text_string.ends_with(f) ? 0.2 : 0.5) * spatium();
    distance *= 0.5 * (snapped_dyn->symbol_scale() + (size() / 10));
    return distance;
}

bool Expression::accept_drop(EditData &ed) const { return ed.drop_element->is_dynamic() || TextBase::accept_drop(ed); }

EngravingItem *Expression::drop(Transaction &tx, EditData &ed) {
    EngravingItem *item = ed.drop_element;
    if (item->is_dynamic()) {
        Dynamic *snapped_dyn = snapped_dynamic();
        if (snapped_dyn) {
            return snapped_dyn->drop(tx, ed);
        }

        item->set_track(track());
        item->set_ownership_parent(segment());
        score()->undo_add_element(item);
        item->undo_change_property(Pid::PLACEMENT, placement(), PropertyFlags::UNSTYLED);
        return item;
    }

    return TextBase::drop(tx, ed);
}

PropertyValue Expression::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SNAP_TO_DYNAMICS: return snap_to_dynamics_value;
        default: return TextBase::get_property(property_id);
    }
}

bool Expression::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SNAP_TO_DYNAMICS:
            if (snap_to_dynamics_value == false && v.to_bool() == true) {
                reset_property(Pid::OFFSET);
            }
            set_snap_to_dynamics(v.to_bool());
            break;
        default:
            if (!TextBase::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

void Expression::map_properties_from_old_expressions(StaffText *staff_text) {
    if (staff_text->min_distance() != property_default(Pid::MIN_DISTANCE).value<Spatium>()) {
        set_min_distance(staff_text->min_distance());
        set_property_flags(Pid::MIN_DISTANCE, PropertyFlags::UNSTYLED);
    }
    if (staff_text->placement() != property_default(Pid::PLACEMENT).value<PlacementV>()) {
        set_placement(staff_text->placement());
        set_property_flags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
    }
    if (!staff_text->offset().is_null()) {
        set_offset(staff_text->offset());
        set_snap_to_dynamics(false);
        set_property_flags(Pid::SNAP_TO_DYNAMICS, PropertyFlags::UNSTYLED);
    }
    if (staff_text->frame_type() != FrameType::NO_FRAME) {
        set_frame_type(staff_text->frame_type());
        set_frame_width(staff_text->frame_width());
        set_frame_color(staff_text->frame_color());
        set_bg_color(staff_text->bg_color());
        set_frame_round(staff_text->frame_round());
    }
}

Dynamic *Expression::snapped_dynamic() const {
    EngravingItem *item = ldata()->item_snapped_before();
    return item && item->is_dynamic() ? to_dynamic(item) : nullptr;
}

void Expression::reset() {
    undo_reset_property(Pid::DIRECTION);
    undo_reset_property(Pid::CENTER_BETWEEN_STAVES);
    TextBase::reset();
    Dynamic *snapped_dyn = snapped_dynamic();
    if (snapped_dyn && !snapped_dyn->offset().is_null()) {
        snapped_dyn->reset();
    }
}
} // namespace mu::engraving
