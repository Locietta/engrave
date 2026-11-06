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

#include "pickscrape.h"

#include "score.h"
#include "system.h"
#include "text.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle pick_scrape_style{
    {Sid::PALM_MUTE_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::PALM_MUTE_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::PALM_MUTE_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::PALM_MUTE_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::PALM_MUTE_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::PALM_MUTE_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::PALM_MUTE_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::PALM_MUTE_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::PALM_MUTE_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::PALM_MUTE_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::PALM_MUTE_TEXT_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::PALM_MUTE_TEXT_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::PALM_MUTE_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::PALM_MUTE_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::PALM_MUTE_POSITION, Pid::END_TEXT_POSITION},
    {Sid::PALM_MUTE_HOOK_HEIGHT, Pid::BEGIN_HOOK_HEIGHT},
    {Sid::PALM_MUTE_HOOK_HEIGHT, Pid::END_HOOK_HEIGHT},
    {Sid::PALM_MUTE_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::PALM_MUTE_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::PALM_MUTE_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::PALM_MUTE_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
    {Sid::PALM_MUTE_END_HOOK_TYPE, Pid::END_HOOK_TYPE},
    {Sid::PALM_MUTE_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::PALM_MUTE_PLACEMENT, Pid::PLACEMENT},
    {Sid::PALM_MUTE_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::PALM_MUTE_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::PALM_MUTE_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
};

PickScrapeSegment::PickScrapeSegment(PickScrape *sp)
    : TextLineBaseSegment(ElementType::PICK_SCRAPE_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

//---------------------------------------------------------
//   PickScrape
//---------------------------------------------------------

PickScrape::PickScrape(DummyParentOr<EngravingItem> parent) : ChordTextLineBase(ElementType::PICK_SCRAPE, parent) {
    init_element_style(&::mu::engraving::pick_scrape_style);
    reset_property(Pid::LINE_VISIBLE);

    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::BEGIN_TEXT);
    reset_property(Pid::CONTINUE_TEXT_PLACE);
    reset_property(Pid::CONTINUE_TEXT);
    reset_property(Pid::END_TEXT_PLACE);
    reset_property(Pid::END_TEXT);
}

static const ElementStyle pick_scrape_segment_style{
    {Sid::PALM_MUTE_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment *PickScrape::create_line_segment() {
    PickScrapeSegment *wb = new PickScrapeSegment(this);
    wb->set_track(track());
    wb->init_element_style(&::mu::engraving::pick_scrape_segment_style);
    return wb;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue PickScrape::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::LINE_WIDTH: return style().style_v(Sid::PALM_MUTE_LINE_WIDTH);

        case Pid::ALIGN: return Align(AlignH::LEFT, AlignV::BASELINE);

        case Pid::LINE_STYLE: return style().style_v(Sid::PALM_MUTE_LINE_STYLE);

        case Pid::LINE_VISIBLE: return true;

        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PropertyValue::from_value(PointF(0, 0));

        case Pid::BEGIN_FONT_STYLE: return style().style_v(Sid::PALM_MUTE_FONT_STYLE);

        case Pid::BEGIN_TEXT:
        case Pid::CONTINUE_TEXT: return PropertyValue::from_value("P.S."); // TODO: fix the style

        case Pid::END_TEXT: return "";

        case Pid::BEGIN_HOOK_TYPE: return HookType::NONE;

        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::AUTO;

        case Pid::TEXT_STYLE: return TextStyleType::PALM_MUTE;

        case Pid::BEGIN_FILLED_ARROW_HEIGHT: // No arrow endings for pick scrape
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

Sid PickScrape::get_property_style(Pid id) const {
    switch (id) {
        case Pid::PLACEMENT: return Sid::PALM_MUTE_PLACEMENT;
        case Pid::BEGIN_FONT_FACE: return Sid::PALM_MUTE_FONT_FACE;
        case Pid::BEGIN_FONT_SIZE:
        case Pid::CONTINUE_FONT_SIZE:
        case Pid::END_FONT_SIZE: return Sid::PALM_MUTE_FONT_SIZE;
        case Pid::BEGIN_FONT_STYLE:
        case Pid::CONTINUE_FONT_STYLE:
        case Pid::END_FONT_STYLE: return Sid::PALM_MUTE_FONT_STYLE;
        case Pid::BEGIN_TEXT_ALIGN:
        case Pid::CONTINUE_TEXT_ALIGN:
        case Pid::END_TEXT_ALIGN: return Sid::PALM_MUTE_TEXT_ALIGN;
        case Pid::BEGIN_HOOK_HEIGHT:
        case Pid::END_HOOK_HEIGHT: return Sid::PALM_MUTE_HOOK_HEIGHT;
        default: break;
    }
    return TextLineBase::get_property_style(id);
}

Sid PickScrape::default_pos_sid() const { return place_above() ? Sid::PALM_MUTE_POS_ABOVE : Sid::PALM_MUTE_POS_BELOW; }
} // namespace mu::engraving
