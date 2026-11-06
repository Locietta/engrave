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

#include "harmonicmark.h"

#include "score.h"
#include "system.h"
#include "text.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle harmonic_mark_style{
    {Sid::LET_RING_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::LET_RING_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::LET_RING_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::LET_RING_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::LET_RING_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::LET_RING_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::LET_RING_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::LET_RING_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::LET_RING_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::LET_RING_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::LET_RING_TEXT_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::LET_RING_TEXT_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::LET_RING_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::LET_RING_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::LET_RING_POSITION, Pid::END_TEXT_POSITION},
    {Sid::LET_RING_HOOK_HEIGHT, Pid::BEGIN_HOOK_HEIGHT},
    {Sid::LET_RING_HOOK_HEIGHT, Pid::END_HOOK_HEIGHT},
    {Sid::LET_RING_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::LET_RING_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::LET_RING_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::LET_RING_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
    {Sid::LET_RING_END_HOOK_TYPE, Pid::END_HOOK_TYPE},
    {Sid::LET_RING_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::OTTAVA8_VA_PLACEMENT, Pid::PLACEMENT},

    {Sid::LET_RING_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::LET_RING_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::LET_RING_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
};

HarmonicMarkSegment::HarmonicMarkSegment(HarmonicMark *sp)
    : TextLineBaseSegment(ElementType::HARMONIC_MARK_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

//---------------------------------------------------------
//   HarmonicMark
//---------------------------------------------------------

HarmonicMark::HarmonicMark(DummyParentOr<EngravingItem> parent) : ChordTextLineBase(ElementType::HARMONIC_MARK, parent) {
    init_element_style(&::mu::engraving::harmonic_mark_style);
    reset_property(Pid::LINE_VISIBLE);

    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::BEGIN_TEXT);
    reset_property(Pid::CONTINUE_TEXT_PLACE);
    reset_property(Pid::CONTINUE_TEXT);
    reset_property(Pid::END_TEXT_PLACE);
    reset_property(Pid::END_TEXT);
}

bool HarmonicMark::set_property(Pid property_id, const PropertyValue &value) {
    if (property_id == Pid::BEGIN_TEXT || property_id == Pid::CONTINUE_TEXT) {
        text_ = value.value<String>();
    }

    return TextLineBase::set_property(property_id, value);
}

static const ElementStyle harmonic_mark_segment_style{
    {Sid::LET_RING_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment *HarmonicMark::create_line_segment() {
    HarmonicMarkSegment *hm = new HarmonicMarkSegment(this);
    hm->set_track(track());
    hm->init_element_style(&::mu::engraving::harmonic_mark_segment_style);
    return hm;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue HarmonicMark::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::LINE_WIDTH: return style().style_v(Sid::LET_RING_LINE_WIDTH);

        case Pid::ALIGN: return Align(AlignH::LEFT, AlignV::BASELINE);

        case Pid::LINE_STYLE: return style().style_v(Sid::LET_RING_LINE_STYLE);

        case Pid::LINE_VISIBLE: return true;

        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PropertyValue::from_value(PointF(0, 0));

        case Pid::BEGIN_FONT_STYLE: return style().style_v(Sid::LET_RING_FONT_STYLE);

        case Pid::BEGIN_TEXT:
        case Pid::CONTINUE_TEXT: return PropertyValue::from_value(text_); // TODO: fix the style
        case Pid::END_TEXT: return "";

        case Pid::BEGIN_HOOK_TYPE: return HookType::NONE;

        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::AUTO;

        case Pid::TEXT_STYLE: return TextStyleType::LET_RING;

        case Pid::BEGIN_FILLED_ARROW_HEIGHT: // No arrow endings for harmonic mark
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

Sid HarmonicMark::get_property_style(Pid id) const {
    switch (id) {
        case Pid::PLACEMENT: return Sid::OTTAVA8_VA_PLACEMENT;
        case Pid::BEGIN_FONT_FACE: return Sid::LET_RING_FONT_FACE;
        case Pid::BEGIN_FONT_SIZE:
        case Pid::CONTINUE_FONT_SIZE:
        case Pid::END_FONT_SIZE: return Sid::LET_RING_FONT_SIZE;
        case Pid::BEGIN_FONT_STYLE:
        case Pid::CONTINUE_FONT_STYLE:
        case Pid::END_FONT_STYLE: return Sid::LET_RING_FONT_STYLE;
        case Pid::BEGIN_TEXT_ALIGN:
        case Pid::CONTINUE_TEXT_ALIGN:
        case Pid::END_TEXT_ALIGN: return Sid::LET_RING_TEXT_ALIGN;
        case Pid::BEGIN_HOOK_HEIGHT:
        case Pid::END_HOOK_HEIGHT: return Sid::LET_RING_HOOK_HEIGHT;
        default: break;
    }
    return TextLineBase::get_property_style(id);
}

Sid HarmonicMark::default_pos_sid() const { return place_above() ? Sid::LET_RING_POS_ABOVE : Sid::LET_RING_POS_BELOW; }
} // namespace mu::engraving
