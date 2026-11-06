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

#include "timesig.h"

#include <functional>

#include "../editing/edittimesig.h"
#include "../editing/transaction/transaction.h"
#include "log.h"
#include "measure.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "style/style.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle ts_style{
    {Sid::TIME_SIG_NORMAL_SCALE, Pid::SCALE},
};

//---------------------------------------------------------
//   TimeSig
//    Constructs an invalid time signature element.
//    After construction first call setTrack() then
//    call setSig().
//    Layout() is static and called in setSig().
//---------------------------------------------------------

TimeSig::TimeSig(DummyParentOr<Segment> parent)
    : EngravingItem(ElementType::TIMESIG, parent, ElementFlag::ON_STAFF | ElementFlag::MOVABLE | ElementFlag::PLACE_ABOVE) {
    init_element_style(&::mu::engraving::ts_style);

    show_courtesy_sig_ = true;
    stretch_.set(1, 1);
    sig_.set(0, 1); // initialize to invalid
    time_sig_type_ = TimeSigType::NORMAL;
    large_parentheses_ = false;
    set_min_distance(0.5_sp); // TODO: style
}

void TimeSig::set_ownership_parent(DummyParentOr<Segment> parent) { EngravingItem::set_ownership_parent(parent); }

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double TimeSig::mag() const { return time_sig_placement() == TimeSigPlacement::NORMAL && staff() ? staff()->staff_mag(this) : 1.0; }

//---------------------------------------------------------
//   setSig
//    custom text has to be set after setSig()
//---------------------------------------------------------

void TimeSig::set_sig(const Fraction &f, TimeSigType st) {
    sig_ = f;
    time_sig_type_ = st;
    large_parentheses_ = false;
    numerator_string_.clear();
    denominator_string_.clear();
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool TimeSig::accept_drop(EditData &data) const { return data.drop_element->is_time_sig(); }

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *TimeSig::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.drop_element;
    if (e->is_time_sig()) {
        // change timesig applies to all staves, can't simply set subtype
        // for this one only
        // ownership of e is transferred to EditTimeSig::addTimeSig

        if (tick() != measure()->end_tick()) {
            EditTimeSig::add_time_sig(tx, score(), measure(), staff_idx(), to_time_sig(e), false);
            return nullptr;
        }

        // This is a timesig at the end of a measure.
        if (*to_time_sig(e) == *this) {
            delete e;
            return nullptr;
        }

        if (!measure()->next_measure()) {
            return nullptr;
        }

        // Apply change to next measure
        EditTimeSig::add_time_sig(tx, score(), measure()->next_measure(), staff_idx(), to_time_sig(e), false);
        return nullptr;
    }
    delete e;
    return nullptr;
}

//---------------------------------------------------------
//   setNumeratorString
//    setSig() has to be called first
//---------------------------------------------------------

void TimeSig::set_numerator_string(const String &a) {
    if (time_sig_type_ == TimeSigType::NORMAL) {
        numerator_string_ = a;
    }
}

//---------------------------------------------------------
//   setDenominatorString
//    setSig() has to be called first
//---------------------------------------------------------

void TimeSig::set_denominator_string(const String &a) {
    if (time_sig_type_ == TimeSigType::NORMAL) {
        denominator_string_ = a;
    }
}

//---------------------------------------------------------
//   setFrom
//---------------------------------------------------------

void TimeSig::set_from(const TimeSig *ts) {
    time_sig_type_ = ts->time_sig_type();
    numerator_string_ = ts->numerator_string_;
    denominator_string_ = ts->denominator_string_;
    sig_ = ts->sig_;
    stretch_ = ts->stretch_;
}

//---------------------------------------------------------
//   ssig
//---------------------------------------------------------

String TimeSig::ssig() const { return String(u"%1/%2").arg(sig_.numerator()).arg(sig_.denominator()); }

//---------------------------------------------------------
//   setSSig
//---------------------------------------------------------

void TimeSig::set_s_sig(const String &s) {
    StringList sl = s.split(u'/');
    if (sl.size() == 2) {
        sig_.set_numerator(sl[0].to_int());
        sig_.set_denominator(sl[1].to_int());
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TimeSig::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SHOW_COURTESY: return show_courtesy_sig();
        case Pid::NUMERATOR_STRING: return numerator_string();
        case Pid::DENOMINATOR_STRING: return denominator_string();
        case Pid::GROUP_NODES: return groups().nodes();
        case Pid::TIMESIG: return PropertyValue::from_value(sig_);
        case Pid::TIMESIG_STRETCH: return PropertyValue::from_value(stretch());
        case Pid::TIMESIG_TYPE: return int(time_sig_type_);
        case Pid::SCALE: return scale_;
        case Pid::IS_COURTESY: return is_courtesy_value;

        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TimeSig::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SHOW_COURTESY:
            if (generated()) {
                return false;
            }
            set_show_courtesy_sig(v.to_bool());
            break;
        case Pid::NUMERATOR_STRING: set_numerator_string(v.value<String>()); break;
        case Pid::DENOMINATOR_STRING: set_denominator_string(v.value<String>()); break;
        case Pid::GROUP_NODES: set_groups(v.value<GroupNodes>()); break;
        case Pid::TIMESIG: set_sig(v.value<Fraction>()); break;
        case Pid::TIMESIG_STRETCH: set_stretch(v.value<Fraction>()); break;
        case Pid::TIMESIG_TYPE: time_sig_type_ = (TimeSigType) (v.to_int()); break;
        case Pid::SCALE: scale_ = v.value<ScaleF>(); break;
        case Pid::IS_COURTESY: is_courtesy_value = v.to_bool(); break;
        default:
            if (!EngravingItem::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout_all(); // TODO
    set_generated(false);
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TimeSig::property_default(Pid id) const {
    switch (id) {
        case Pid::SHOW_COURTESY: return true;
        case Pid::NUMERATOR_STRING: return String();
        case Pid::DENOMINATOR_STRING: return String();
        case Pid::TIMESIG: return PropertyValue::from_value(Fraction(4, 4));
        case Pid::TIMESIG_TYPE: return int(TimeSigType::NORMAL);
        case Pid::PLACEMENT: return PlacementV::ABOVE;
        case Pid::IS_COURTESY: return false;
        default: return EngravingItem::property_default(id);
    }
}

PointF TimeSig::staff_offset() const {
    const Segment *seg = segment();
    const Measure *meas = seg ? seg->measure() : nullptr;
    const Fraction ts_tick = meas ? meas->tick() : tick();
    const StaffType *st = staff()->const_staff_type(ts_tick);
    const double y_offset = st ? st->yoffset().val() * spatium() : 0.0;
    return PointF(0.0, y_offset);
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *TimeSig::next_segment_element() { return segment()->first_in_next_segments(staff_idx()); }

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *TimeSig::prev_segment_element() { return segment()->last_in_prev_segments(staff_idx()); }

//---------------------------------------------------------
//   subtype
//---------------------------------------------------------

int TimeSig::subtype() const {
    size_t h1 = std::hash<int>{}(numerator());
    size_t h2 = std::hash<int>{}(denominator());
    size_t h3 = std::hash<TimeSigType>{}(time_sig_type());

    return static_cast<int>(h1 ^ (h2 << 1) ^ (h3 << 2));
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString TimeSig::subtype_user_name() const {
    switch (time_sig_type()) {
        case TimeSigType::FOUR_FOUR: return TranslatableString("engraving/timesig", "Common time");
        case TimeSigType::ALLA_BREVE: return TranslatableString("engraving/timesig", "Cut time");
        case TimeSigType::CUT_BACH: return TranslatableString("engraving/timesig", "Cut time (Bach)");
        case TimeSigType::CUT_TRIPLE: return TranslatableString("engraving/timesig", "Cut triple time (9/8)");
        default: return TranslatableString("engraving/timesig", "%1/%2 time").arg(numerator(), denominator());
    }
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

void TimeSig::init_element_style(const ElementStyle *element_stype) {
    EngravingItem::init_element_style(element_stype);

    scale_ = property_default(Pid::SCALE).value<ScaleF>();
}

void TimeSig::style_changed() {
    if (is_styled(Pid::SCALE)) {
        scale_ = property_default(Pid::SCALE).value<ScaleF>();
    }
    EngravingItem::style_changed();
}

Sid TimeSig::get_property_style(Pid id) const {
    if (id == Pid::SCALE) {
        switch (time_sig_placement()) {
            case TimeSigPlacement::NORMAL: return Sid::TIME_SIG_NORMAL_SCALE;
            case TimeSigPlacement::ABOVE_STAVES: return Sid::TIME_SIG_ABOVE_SCALE;
            case TimeSigPlacement::ACROSS_STAVES: return Sid::TIME_SIG_ACROSS_SCALE;
            default: return Sid::NOSTYLE;
        }
    }

    return EngravingItem::get_property_style(id);
}

TimeSigPlacement TimeSig::time_sig_placement() const { return style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>(); }

TimeSigStyle TimeSig::time_sig_style() const {
    switch (time_sig_placement()) {
        case TimeSigPlacement::NORMAL: return style().style_v(Sid::TIME_SIG_NORMAL_STYLE).value<TimeSigStyle>();
        case TimeSigPlacement::ABOVE_STAVES: return style().style_v(Sid::TIME_SIG_ABOVE_STYLE).value<TimeSigStyle>();
        case TimeSigPlacement::ACROSS_STAVES: return style().style_v(Sid::TIME_SIG_ACROSS_STYLE).value<TimeSigStyle>();
        default: return TimeSigStyle::NORMAL;
    }
}

double TimeSig::num_dist() const {
    switch (time_sig_placement()) {
        case TimeSigPlacement::NORMAL: return style().style_absolute(Sid::TIME_SIG_NORMAL_NUM_DIST);
        case TimeSigPlacement::ABOVE_STAVES: return style().style_absolute(Sid::TIME_SIG_ABOVE_NUM_DIST);
        case TimeSigPlacement::ACROSS_STAVES: return style().style_absolute(Sid::TIME_SIG_ACROSS_NUM_DIST);
        default: return 0.0;
    }
}

double TimeSig::y_pos() const {
    switch (time_sig_placement()) {
        case TimeSigPlacement::NORMAL: return style().style_absolute(Sid::TIME_SIG_NORMAL_Y);
        case TimeSigPlacement::ABOVE_STAVES:
            return (staff()->has_system_objects_below_bottom_staff() ? -1.0 : 1.0) * style().style_absolute(Sid::TIME_SIG_ABOVE_Y);
        case TimeSigPlacement::ACROSS_STAVES: return style().style_absolute(Sid::TIME_SIG_ACROSS_Y);
        default: return 0.0;
    }
}

bool TimeSig::show_on_this_staff() const {
    return time_sig_placement() == TimeSigPlacement::NORMAL || staff_idx() == 0 || staff()->is_system_object_staff();
}

bool TimeSig::is_above_staves() const { return time_sig_placement() == TimeSigPlacement::ABOVE_STAVES; }

bool TimeSig::is_across_staves() const { return time_sig_placement() == TimeSigPlacement::ACROSS_STAVES; }

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool TimeSig::operator==(const TimeSig &ts) const {
    return (time_sig_type() == ts.time_sig_type()) && (sig().identical(ts.sig())) && (stretch() == ts.stretch()) &&
           (groups() == ts.groups()) && (numerator_string_ == ts.numerator_string_) && (denominator_string_ == ts.denominator_string_);
}

void TimeSig::added() {
    IF_ASSERT_FAILED(score()) { return; }

    score()->update_ticks_and_time_sig_map_later();
}

void TimeSig::removed() {
    IF_ASSERT_FAILED(score()) { return; }

    score()->update_ticks_and_time_sig_map_later();
}
} // namespace mu::engraving
