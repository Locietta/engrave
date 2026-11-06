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

#include "ottava.h"

#include "chordrest.h"
#include "log.h"
#include "measure.h"
#include "score.h"
#include "staff.h"
#include "system.h"
#include "text.h"
#include "types/translatablestring.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   ottavaStyle
//---------------------------------------------------------

static const ElementStyle ottava_style{
    {Sid::OTTAVA_NUMBERS_ONLY, Pid::NUMBERS_ONLY},
    {Sid::OTTAVA8_VA_PLACEMENT, Pid::PLACEMENT},
    {Sid::OTTAVA8_VA_TEXT, Pid::BEGIN_TEXT},
    {Sid::OTTAVA8_VA_CONTINUE_TEXT, Pid::CONTINUE_TEXT},
    {Sid::OTTAVA_HOOK_ABOVE, Pid::END_HOOK_HEIGHT},
    {Sid::OTTAVA_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::OTTAVA_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::OTTAVA_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::OTTAVA_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::OTTAVA_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::OTTAVA_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::OTTAVA_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::OTTAVA_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::OTTAVA_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::OTTAVA_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::OTTAVA_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::OTTAVA_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::OTTAVA_TEXT_ALIGN_ABOVE, Pid::BEGIN_TEXT_ALIGN},
    {Sid::OTTAVA_TEXT_ALIGN_ABOVE, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::OTTAVA_TEXT_ALIGN_ABOVE, Pid::END_TEXT_ALIGN},
    {Sid::OTTAVA_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::OTTAVA_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::OTTAVA_POSITION, Pid::END_TEXT_POSITION},
    {Sid::OTTAVA_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::OTTAVA_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::OTTAVA_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::OTTAVA_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::OTTAVA_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
    {Sid::OTTAVA_END_LINE_ARROW_HEIGHT, Pid::END_LINE_ARROW_HEIGHT},
    {Sid::OTTAVA_END_LINE_ARROW_WIDTH, Pid::END_LINE_ARROW_WIDTH},
    {Sid::OTTAVA_BEGIN_LINE_ARROW_HEIGHT, Pid::BEGIN_LINE_ARROW_HEIGHT},
    {Sid::OTTAVA_BEGIN_LINE_ARROW_WIDTH, Pid::BEGIN_LINE_ARROW_WIDTH},
    {Sid::OTTAVA_END_FILLED_ARROW_HEIGHT, Pid::END_FILLED_ARROW_HEIGHT},
    {Sid::OTTAVA_END_FILLED_ARROW_WIDTH, Pid::END_FILLED_ARROW_WIDTH},
    {Sid::OTTAVA_BEGIN_FILLED_ARROW_HEIGHT, Pid::BEGIN_FILLED_ARROW_HEIGHT},
    {Sid::OTTAVA_BEGIN_FILLED_ARROW_WIDTH, Pid::BEGIN_FILLED_ARROW_WIDTH},
};

OttavaSegment::OttavaSegment(Ottava *sp)
    : TextLineBaseSegment(ElementType::OTTAVA_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *OttavaSegment::property_delegate(Pid pid) const {
    if (pid == Pid::OTTAVA_TYPE || pid == Pid::NUMBERS_ONLY) {
        return spanner();
    }
    return TextLineBaseSegment::property_delegate(pid);
}

//---------------------------------------------------------
//   setOttavaType
//---------------------------------------------------------

void Ottava::set_ottava_type(OttavaType val) {
    if (ottava_type_ == val) {
        return;
    }
    ottava_type_ = val;
    style_changed();
}

//---------------------------------------------------------
//   setNumbersOnly
//---------------------------------------------------------

void Ottava::set_numbers_only(bool val) { numbers_only_ = val; }

//---------------------------------------------------------
//   setPlacement
//---------------------------------------------------------

void Ottava::set_placement(PlacementV p) { TextLineBase::set_placement(p); }

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void OttavaSegment::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::OTTAVA_TYPE || id == Pid::NUMBERS_ONLY) {
        EngravingObject::undo_change_property(id, v, ps);
    } else {
        EngravingObject::undo_change_property(id, v, ps);
    }
}

void Ottava::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::OTTAVA_TYPE || id == Pid::NUMBERS_ONLY) {
        TextLineBase::undo_change_property(id, v, ps);
        style_changed(); // these properties may change style settings
    } else {
        TextLineBase::undo_change_property(id, v, ps);
    }
}

Sid Ottava::get_property_style(Pid pid) const {
    static_assert(int(OttavaType::OTTAVA_22MB) - int(OttavaType::OTTAVA_8VA) == 5);

    static const std::vector<Sid> ss = {
        Sid::OTTAVA8_VA_PLACEMENT,  Sid::OTTAVA8_V_ANO_TEXT,  Sid::OTTAVA8_V_ANO_CONTINUE_TEXT,
        Sid::OTTAVA8_VB_PLACEMENT,  Sid::OTTAVA8_V_BNO_TEXT,  Sid::OTTAVA8_V_BNO_CONTINUE_TEXT,
        Sid::OTTAVA15_MA_PLACEMENT, Sid::OTTAVA15_M_ANO_TEXT, Sid::OTTAVA15_M_ANO_CONTINUE_TEXT,
        Sid::OTTAVA15_MB_PLACEMENT, Sid::OTTAVA15_M_BNO_TEXT, Sid::OTTAVA15_M_BNO_CONTINUE_TEXT,
        Sid::OTTAVA22_MA_PLACEMENT, Sid::OTTAVA22_M_ANO_TEXT, Sid::OTTAVA22_M_ANO_CONTINUE_TEXT,
        Sid::OTTAVA22_MB_PLACEMENT, Sid::OTTAVA22_M_BNO_TEXT, Sid::OTTAVA22_M_BNO_CONTINUE_TEXT,

        Sid::OTTAVA8_VA_PLACEMENT,  Sid::OTTAVA8_VA_TEXT,     Sid::OTTAVA8_VA_CONTINUE_TEXT,
        Sid::OTTAVA8_VB_PLACEMENT,  Sid::OTTAVA8_VB_TEXT,     Sid::OTTAVA8_VB_CONTINUE_TEXT,
        Sid::OTTAVA15_MA_PLACEMENT, Sid::OTTAVA15_MA_TEXT,    Sid::OTTAVA15_MA_CONTINUE_TEXT,
        Sid::OTTAVA15_MB_PLACEMENT, Sid::OTTAVA15_MB_TEXT,    Sid::OTTAVA15_MB_CONTINUE_TEXT,
        Sid::OTTAVA22_MA_PLACEMENT, Sid::OTTAVA22_MA_TEXT,    Sid::OTTAVA22_MA_CONTINUE_TEXT,
        Sid::OTTAVA22_MB_PLACEMENT, Sid::OTTAVA22_MB_TEXT,    Sid::OTTAVA22_MB_CONTINUE_TEXT,
    };

    size_t idx = size_t(ottava_type_) * 3 + (numbers_only_ ? 0 : ss.size() / 2);
    switch (pid) {
        case Pid::PLACEMENT: return ss[idx];
        case Pid::BEGIN_TEXT: return ss[idx + 1];    // BEGIN_TEXT
        case Pid::CONTINUE_TEXT: return ss[idx + 2]; // CONTINUE_TEXT
        case Pid::END_HOOK_HEIGHT:
            if (is_styled(Pid::PLACEMENT)) {
                return style().style_v(ss[idx]).value<PlacementV>() == PlacementV::ABOVE ? Sid::OTTAVA_HOOK_ABOVE : Sid::OTTAVA_HOOK_BELOW;
            } else {
                return place_above() ? Sid::OTTAVA_HOOK_ABOVE : Sid::OTTAVA_HOOK_BELOW;
            }
        case Pid::BEGIN_TEXT_ALIGN:
        case Pid::CONTINUE_TEXT_ALIGN:
        case Pid::END_TEXT_ALIGN: return place_above() ? Sid::OTTAVA_TEXT_ALIGN_ABOVE : Sid::OTTAVA_TEXT_ALIGN_BELOW;
        default: return TextLineBase::get_property_style(pid);
    }
}

//---------------------------------------------------------
//   Ottava
//---------------------------------------------------------

Ottava::Ottava(DummyParentOr<EngravingItem> parent)
    : TextLineBase(ElementType::OTTAVA, parent, ElementFlag::ON_STAFF | ElementFlag::MOVABLE) {
    ottava_type_ = OttavaType::OTTAVA_8VA;
    numbers_only_ = false;
    set_begin_text_place(TextPlace::LEFT);
    set_continue_text_place(TextPlace::LEFT);
    set_end_hook_type(HookType::HOOK_90);
    set_line_visible(true);
    set_begin_hook_height(0_sp);
    set_end_text(u"");

    init_element_style(&::ottava_style);
}

Ottava::Ottava(const Ottava &o) : TextLineBase(o) {
    set_ottava_type(o.ottava_type_);
    numbers_only_ = o.numbers_only_;
}

//---------------------------------------------------------
//   pitchShift
//---------------------------------------------------------

int Ottava::pitch_shift() const { return ::mu::engraving::ottava_default[int(ottava_type_)].shift; }

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

static const ElementStyle ottava_segment_style{
    {Sid::OTTAVA_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

LineSegment *Ottava::create_line_segment() {
    OttavaSegment *os = new OttavaSegment(this);
    os->set_track(track());
    os->init_element_style(&::ottava_segment_style);
    return os;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Ottava::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::OTTAVA_TYPE: return int(ottava_type());

        case Pid::NUMBERS_ONLY: return numbers_only_;

        case Pid::END_TEXT_PLACE: // HACK
            return TextPlace::LEFT;

        default: break;
    }
    return TextLineBase::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Ottava::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::PLAY:
            set_play_spanner(val.to_bool());
            staff()->update_ottava();
            break;

        case Pid::OTTAVA_TYPE: set_ottava_type(OttavaType(val.to_int())); break;

        case Pid::NUMBERS_ONLY: numbers_only_ = val.to_bool(); break;

        case Pid::SPANNER_TICKS:
            set_ticks(val.value<Fraction>());
            staff()->update_ottava();
            break;

        case Pid::SPANNER_TICK:
            set_tick(val.value<Fraction>());
            staff()->update_ottava();
            break;

        default:
            if (!TextLineBase::set_property(property_id, val)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Ottava::property_default(Pid pid) const {
    switch (pid) {
        case Pid::OTTAVA_TYPE: return PropertyValue();
        case Pid::END_HOOK_TYPE: return HookType::HOOK_90;
        case Pid::LINE_VISIBLE: return true;
        case Pid::BEGIN_TEXT_OFFSET:
        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PropertyValue::from_value(PointF());
        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::LEFT;
        case Pid::BEGIN_HOOK_TYPE: return HookType::NONE;
        case Pid::BEGIN_HOOK_HEIGHT: return 0_sp;
        case Pid::END_TEXT: return String();
        case Pid::PLACEMENT: return style_value(Pid::PLACEMENT, get_property_style(Pid::PLACEMENT));
        case Pid::TEXT_STYLE: return TextStyleType::OTTAVA;

        default: return TextLineBase::property_default(pid);
    }
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Ottava::subtype_user_name() const { return ::mu::engraving::ottava_default[int(ottava_type())].user_name; }

void OttavaSegment::rebase_offsets_on_anchor_changed(Grip grip, const PointF &old_pos, System *sys) {
    if (grip == Grip::MIDDLE || grip == Grip::END) {
        ottava()->compute_end_element();
    }
    LineSegment::rebase_offsets_on_anchor_changed(grip, old_pos, sys);
}

//---------------------------------------------------------
//   ottavaTypeName
//---------------------------------------------------------

const char *Ottava::ottava_type_name(OttavaType type) { return ::mu::engraving::ottava_default[int(type)].name; }

PointF Ottava::line_pos(Grip grip, System **system) const {
    if (grip == Grip::START) {
        return TextLineBase::line_pos(grip, system);
    }

    bool extend_to_end_of_duration = false; // TODO: style
    if (extend_to_end_of_duration) {
        return SLine::line_pos(grip, system);
    }

    ChordRest *end_cr = end_element() && end_element()->is_chord_rest() ? to_chord_rest(end_element()) : nullptr;
    if (!end_cr) {
        return PointF();
    }

    Segment *seg = end_cr->segment();

    *system = seg->measure()->system();

    // End 1sp after the right edge of the end chord, but don't overlap followig segments
    Shape staff_shape = seg->staff_shape(end_cr->v_staff_idx());
    staff_shape.remove_if([](ShapeElement &el) { return el.height() == 0; });
    double x = staff_shape.right() + seg->x() + seg->measure()->x() + spatium();
    Segment *following_c_rseg = score()->tick2segment(end_cr->end_tick(), true, SegmentType::CHORD_REST);
    if (following_c_rseg && following_c_rseg->system() == seg->system()) {
        x = std::min(x, following_c_rseg->x() + following_c_rseg->measure()->x());
    }

    x -= 0.5 * absolute_from_spatium(line_width());

    return PointF(x, 0.0);
}

void Ottava::do_compute_end_element() {
    set_end_element(score()->find_chord_rest_ending_before_tick_in_staff(tick2(), track2staff(track())));
}

Sid Ottava::default_pos_sid() const { return place_above() ? Sid::OTTAVA_POS_ABOVE : Sid::OTTAVA_POS_BELOW; }
