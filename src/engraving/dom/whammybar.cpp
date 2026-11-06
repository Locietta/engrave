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

#include "whammybar.h"

#include "score.h"
#include "system.h"
#include "text.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle whammy_bar_style{
    {Sid::WHAMMY_BAR_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::WHAMMY_BAR_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::WHAMMY_BAR_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::WHAMMY_BAR_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::WHAMMY_BAR_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::WHAMMY_BAR_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::WHAMMY_BAR_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::WHAMMY_BAR_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::WHAMMY_BAR_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::WHAMMY_BAR_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::WHAMMY_BAR_TEXT_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::WHAMMY_BAR_TEXT_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::WHAMMY_BAR_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::WHAMMY_BAR_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::WHAMMY_BAR_POSITION, Pid::END_TEXT_POSITION},
    {Sid::WHAMMY_BAR_HOOK_HEIGHT, Pid::BEGIN_HOOK_HEIGHT},
    {Sid::WHAMMY_BAR_HOOK_HEIGHT, Pid::END_HOOK_HEIGHT},
    {Sid::WHAMMY_BAR_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::WHAMMY_BAR_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::WHAMMY_BAR_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::WHAMMY_BAR_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
    {Sid::WHAMMY_BAR_END_HOOK_TYPE, Pid::END_HOOK_TYPE},
    {Sid::WHAMMY_BAR_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::WHAMMY_BAR_TEXT, Pid::BEGIN_TEXT},
    {Sid::WHAMMY_BAR_PLACEMENT, Pid::PLACEMENT},

    {Sid::WHAMMY_BAR_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::WHAMMY_BAR_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::WHAMMY_BAR_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
};

WhammyBarSegment::WhammyBarSegment(WhammyBar *sp)
    : TextLineBaseSegment(ElementType::WHAMMY_BAR_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

//---------------------------------------------------------
//   WhammyBar
//---------------------------------------------------------

WhammyBar::WhammyBar(DummyParentOr<EngravingItem> parent) : ChordTextLineBase(ElementType::WHAMMY_BAR, parent) {
    init_element_style(&::mu::engraving::whammy_bar_style);
    reset_property(Pid::LINE_VISIBLE);

    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::BEGIN_TEXT);
    reset_property(Pid::CONTINUE_TEXT_PLACE);
    reset_property(Pid::CONTINUE_TEXT);
    reset_property(Pid::END_TEXT_PLACE);
    reset_property(Pid::END_TEXT);
}

static const ElementStyle whammy_bar_segment_style{
    {Sid::LET_RING_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment *WhammyBar::create_line_segment() {
    WhammyBarSegment *wb = new WhammyBarSegment(this);
    wb->set_track(track());
    wb->init_element_style(&::mu::engraving::whammy_bar_segment_style);
    return wb;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue WhammyBar::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::LINE_WIDTH: return style().style_v(Sid::WHAMMY_BAR_LINE_WIDTH);

        case Pid::LINE_STYLE: return style().style_v(Sid::WHAMMY_BAR_LINE_STYLE);

        case Pid::LINE_VISIBLE: return true;

        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PropertyValue::from_value(PointF(0, 0));

        case Pid::BEGIN_FONT_STYLE: return style().style_v(Sid::WHAMMY_BAR_FONT_STYLE);

        case Pid::BEGIN_TEXT:
        case Pid::CONTINUE_TEXT: return style().style_v(Sid::WHAMMY_BAR_TEXT).value<String>(); // TODO: fix the style

        case Pid::END_TEXT: return "";

        case Pid::BEGIN_HOOK_TYPE: return HookType::NONE;

        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::AUTO;

        case Pid::TEXT_STYLE: return TextStyleType::WHAMMY_BAR;

        case Pid::BEGIN_FILLED_ARROW_HEIGHT: // No arrow endings for whammy bar
        case Pid::BEGIN_FILLED_ARROW_WIDTH:
        case Pid::END_FILLED_ARROW_HEIGHT:
        case Pid::END_FILLED_ARROW_WIDTH:
        case Pid::BEGIN_LINE_ARROW_HEIGHT:
        case Pid::BEGIN_LINE_ARROW_WIDTH:
        case Pid::END_LINE_ARROW_HEIGHT:
        case Pid::END_LINE_ARROW_WIDTH: return 0.0;

        default: return TextLineBase::property_default(property_id);
    }
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid WhammyBar::get_property_style(Pid id) const {
    switch (id) {
        case Pid::PLACEMENT: return Sid::WHAMMY_BAR_PLACEMENT;
        case Pid::BEGIN_FONT_FACE: return Sid::WHAMMY_BAR_FONT_FACE;
        case Pid::BEGIN_FONT_SIZE:
        case Pid::CONTINUE_FONT_SIZE:
        case Pid::END_FONT_SIZE: return Sid::WHAMMY_BAR_FONT_SIZE;
        case Pid::BEGIN_FONT_STYLE:
        case Pid::CONTINUE_FONT_STYLE:
        case Pid::END_FONT_STYLE: return Sid::WHAMMY_BAR_FONT_STYLE;
        case Pid::BEGIN_TEXT_ALIGN:
        case Pid::CONTINUE_TEXT_ALIGN:
        case Pid::END_TEXT_ALIGN: return Sid::WHAMMY_BAR_TEXT_ALIGN;
        case Pid::BEGIN_HOOK_HEIGHT:
        case Pid::END_HOOK_HEIGHT: return Sid::WHAMMY_BAR_HOOK_HEIGHT;
        default: break;
    }
    return TextLineBase::get_property_style(id);
}

Sid WhammyBar::default_pos_sid() const { return place_above() ? Sid::WHAMMY_BAR_POS_ABOVE : Sid::WHAMMY_BAR_POS_BELOW; }
} // namespace mu::engraving
