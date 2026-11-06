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

#include "pedal.h"

#include "chord.h"
#include "chordrest.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "rest.h"
#include "score.h"
#include "system.h"
#include "text.h"

using namespace mu;

namespace mu::engraving {
static const ElementStyle pedal_style{
    {Sid::PEDAL_TEXT, Pid::BEGIN_TEXT},
    {Sid::PEDAL_CONTINUE_TEXT, Pid::CONTINUE_TEXT},
    {Sid::PEDAL_ROSETTE_END_TEXT, Pid::END_TEXT},
    {Sid::PEDAL_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::PEDAL_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::PEDAL_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::PEDAL_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::PEDAL_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::PEDAL_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::PEDAL_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::PEDAL_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::PEDAL_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::PEDAL_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::PEDAL_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::PEDAL_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::PEDAL_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::PEDAL_TEXT_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::PEDAL_TEXT_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::PEDAL_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::PEDAL_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::PEDAL_POSITION, Pid::END_TEXT_POSITION},
    {Sid::PEDAL_HOOK_HEIGHT, Pid::BEGIN_HOOK_HEIGHT},
    {Sid::PEDAL_HOOK_HEIGHT, Pid::END_HOOK_HEIGHT},
    {Sid::PEDAL_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::PEDAL_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::PEDAL_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::PEDAL_PLACEMENT, Pid::PLACEMENT},
    {Sid::PEDAL_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::PEDAL_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
    {Sid::PEDAL_END_LINE_ARROW_HEIGHT, Pid::END_LINE_ARROW_HEIGHT},
    {Sid::PEDAL_END_LINE_ARROW_WIDTH, Pid::END_LINE_ARROW_WIDTH},
    {Sid::PEDAL_BEGIN_LINE_ARROW_HEIGHT, Pid::BEGIN_LINE_ARROW_HEIGHT},
    {Sid::PEDAL_BEGIN_LINE_ARROW_WIDTH, Pid::BEGIN_LINE_ARROW_WIDTH},
    {Sid::PEDAL_END_FILLED_ARROW_HEIGHT, Pid::END_FILLED_ARROW_HEIGHT},
    {Sid::PEDAL_END_FILLED_ARROW_WIDTH, Pid::END_FILLED_ARROW_WIDTH},
    {Sid::PEDAL_BEGIN_FILLED_ARROW_HEIGHT, Pid::BEGIN_FILLED_ARROW_HEIGHT},
    {Sid::PEDAL_BEGIN_FILLED_ARROW_WIDTH, Pid::BEGIN_FILLED_ARROW_WIDTH},
};

const String Pedal::pedal_symbol = u"<sym>keyboardPedalPed</sym>";
const String Pedal::star_symbol = u"<sym>keyboardPedalUp</sym>";

PedalSegment::PedalSegment(Pedal *sp) : TextLineBaseSegment(ElementType::PEDAL_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

Sid Pedal::get_property_style(Pid pid) const {
    switch (pid) {
        case Pid::END_TEXT: return line_visible() ? Sid::PEDAL_END_TEXT : Sid::PEDAL_ROSETTE_END_TEXT;
        case Pid::BEGIN_TEXT: return begin_hook_type() == HookType::NONE ? Sid::PEDAL_TEXT : Sid::PEDAL_HOOK_TEXT;
        case Pid::CONTINUE_TEXT: return begin_hook_type() == HookType::NONE ? Sid::PEDAL_CONTINUE_TEXT : Sid::PEDAL_CONTINUE_HOOK_TEXT;
        default: return TextLineBase::get_property_style(pid);
    }
}

//---------------------------------------------------------
//   Pedal
//---------------------------------------------------------

Pedal::Pedal(DummyParentOr<EngravingItem> parent) : TextLineBase(ElementType::PEDAL, parent) {
    init_element_style(&::mu::engraving::pedal_style);
    set_line_visible(true);

    reset_property(Pid::LINE_WIDTH);
    reset_property(Pid::LINE_STYLE);

    reset_property(Pid::BEGIN_HOOK_TYPE);
    reset_property(Pid::END_HOOK_TYPE);

    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::LINE_VISIBLE);
    reset_property(Pid::END_TEXT);
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

static const ElementStyle pedal_segment_style{
    {Sid::PEDAL_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

LineSegment *Pedal::create_line_segment() {
    PedalSegment *p = new PedalSegment(this);
    p->set_track(track());
    p->init_element_style(&::mu::engraving::pedal_segment_style);
    return p;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

engraving::PropertyValue Pedal::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::LINE_WIDTH: return style().style_s(Sid::PEDAL_LINE_WIDTH); // return point, not spatium

        case Pid::LINE_STYLE: return style().style_v(Sid::PEDAL_LINE_STYLE);

        case Pid::BEGIN_TEXT:
        case Pid::CONTINUE_TEXT:
        case Pid::END_TEXT: return style().style_v(get_property_style(property_id));

        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::LEFT;

        case Pid::BEGIN_TEXT_OFFSET:
        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PropertyValue::from_value(PointF(0, 0));

        case Pid::BEGIN_HOOK_TYPE:
        case Pid::END_HOOK_TYPE: return HookType::NONE;

        case Pid::LINE_VISIBLE: return true;

        case Pid::PLACEMENT: return style().style_v(Sid::PEDAL_PLACEMENT);

        case Pid::TEXT_STYLE: return TextStyleType::PEDAL;

        default: return TextLineBase::property_default(property_id);
    }
}

bool Pedal::set_property(Pid property_id, const PropertyValue &v) {
    // Update style flag for text
    if (property_id == Pid::BEGIN_HOOK_TYPE) {
        set_begin_hook_type(v.value<HookType>());

        PropertyFlags begin_text_style_flag =
            begin_text() == property_default(Pid::BEGIN_TEXT).value<String>() ? PropertyFlags::STYLED : PropertyFlags::UNSTYLED;
        set_property_flags(Pid::BEGIN_TEXT, begin_text_style_flag);
        PropertyFlags continue_text_style_flag =
            continue_text() == property_default(Pid::CONTINUE_TEXT).value<String>() ? PropertyFlags::STYLED : PropertyFlags::UNSTYLED;
        set_property_flags(Pid::CONTINUE_TEXT, continue_text_style_flag);
    } else if (property_id == Pid::LINE_VISIBLE) {
        set_line_visible(v.to_bool());
        PropertyFlags end_text_style_flag =
            end_text() == property_default(Pid::END_TEXT).value<String>() ? PropertyFlags::STYLED : PropertyFlags::UNSTYLED;
        set_property_flags(Pid::END_TEXT, end_text_style_flag);
    } else {
        return TextLineBase::set_property(property_id, v);
    }

    trigger_layout();
    return true;
}

Pedal *Pedal::find_next_in_staff() const {
    Fraction end_tick = tick2();
    auto spanners = score()->spanner_map().find_overlapping(end_tick.ticks(), score()->end_tick().ticks());
    for (auto element : spanners) {
        Spanner *spanner = element.value;
        if (spanner->is_pedal() && spanner != this && spanner->staff_idx() == staff_idx() && spanner->tick() == end_tick) {
            return to_pedal(spanner);
        }
    }

    return nullptr;
}

bool Pedal::connect45_hook_to_next() const {
    if (end_hook_type() != HookType::HOOK_45) {
        return false;
    }

    Pedal *next_pedal = find_next_in_staff();

    return next_pedal && next_pedal->tick() == tick2() && next_pedal->begin_hook_type() == HookType::HOOK_45;
}

//---------------------------------------------------------
//   linePos
//    return System() coordinates
//---------------------------------------------------------

PointF Pedal::line_pos(Grip grip, System **sys) const {
    bool start = grip == Grip::START;

    if (start) {
        Segment *start_seg = start_segment();
        if (!start_seg) {
            return PointF();
        }
        *sys = start_seg->measure()->system();
        double x = start_seg->x() + start_seg->measure()->x();
        if (begin_text() == "<sym>keyboardPedalPed</sym>") {
            x -= 0.5 * spatium();
        } else if (begin_hook_type() == HookType::HOOK_90 || begin_hook_type() == HookType::HOOK_90T) {
            x += 0.5 * absolute_from_spatium(line_width());
        } else if (begin_hook_type() == HookType::HOOK_45) {
            EngravingItem *item = start_element();
            if (item && item->is_chord()) {
                Note *down_note = to_chord(item)->down_note();
                x += 0.5 * down_note->head_width();
            } else if (item && item->is_rest()) {
                x += to_rest(item)->center_x();
            }
        }
        return PointF(x, 0.0);
    }

    Segment *end_seg = end_segment();
    if (!end_seg) {
        return PointF();
    }

    Pedal *next_pedal = find_next_in_staff();

    if (next_pedal && end_hook_type() == HookType::HOOK_45) {
        *sys = end_seg->measure()->system();
        double x = end_seg->x() + end_seg->measure()->x();
        EngravingItem *item = end_element();
        if (item && item->is_chord()) {
            Note *down_note = to_chord(item)->down_note();
            x += 0.5 * down_note->head_width();
        } else if (item && item->is_rest()) {
            x += to_rest(item)->center_x();
        }
        return PointF(x, 0.0);
    }

    if (end_seg->rtick() == Fraction(0, 1)) {
        Segment *prev_seg = end_seg->prev1(SegmentType::END_BAR_LINE);
        if (prev_seg) {
            end_seg = prev_seg;
        }
    }

    *sys = end_seg->measure()->system();
    double x = end_seg->x() + end_seg->measure()->x();

    x -= (end_seg->is_chord_rest_type() && next_pedal ? 1.25 : 0.75) * spatium();

    return PointF(x, 0.0);
}

Sid Pedal::default_pos_sid() const { return place_above() ? Sid::PEDAL_POS_ABOVE : Sid::PEDAL_POS_BELOW; }
} // namespace mu::engraving
