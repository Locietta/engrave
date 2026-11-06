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

#include "palmmute.h"

#include "chordrest.h"
#include "part.h"
#include "score.h"
#include "staff.h"
#include "stafftype.h"
#include "system.h"
#include "text.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle palm_mute_style{
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
    {Sid::PALM_MUTE_END_LINE_ARROW_HEIGHT, Pid::END_LINE_ARROW_HEIGHT},
    {Sid::PALM_MUTE_END_LINE_ARROW_WIDTH, Pid::END_LINE_ARROW_WIDTH},
    {Sid::PALM_MUTE_BEGIN_LINE_ARROW_HEIGHT, Pid::BEGIN_LINE_ARROW_HEIGHT},
    {Sid::PALM_MUTE_BEGIN_LINE_ARROW_WIDTH, Pid::BEGIN_LINE_ARROW_WIDTH},
    {Sid::PALM_MUTE_END_FILLED_ARROW_HEIGHT, Pid::END_FILLED_ARROW_HEIGHT},
    {Sid::PALM_MUTE_END_FILLED_ARROW_WIDTH, Pid::END_FILLED_ARROW_WIDTH},
    {Sid::PALM_MUTE_BEGIN_FILLED_ARROW_HEIGHT, Pid::BEGIN_FILLED_ARROW_HEIGHT},
    {Sid::PALM_MUTE_BEGIN_FILLED_ARROW_WIDTH, Pid::BEGIN_FILLED_ARROW_WIDTH},
    {Sid::PALM_MUTE_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::PALM_MUTE_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::PALM_MUTE_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
};

PalmMuteSegment::PalmMuteSegment(PalmMute *sp)
    : TextLineBaseSegment(ElementType::PALM_MUTE_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

//---------------------------------------------------------
//   PalmMute
//---------------------------------------------------------

PalmMute::PalmMute(DummyParentOr<EngravingItem> parent) : ChordTextLineBase(ElementType::PALM_MUTE, parent) {
    init_element_style(&::mu::engraving::palm_mute_style);
    reset_property(Pid::LINE_VISIBLE);

    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::BEGIN_TEXT);
    reset_property(Pid::CONTINUE_TEXT_PLACE);
    reset_property(Pid::CONTINUE_TEXT);
    reset_property(Pid::END_TEXT_PLACE);
    reset_property(Pid::END_TEXT);
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

static const ElementStyle palm_mute_segment_style{
    {Sid::PALM_MUTE_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

LineSegment *PalmMute::create_line_segment() {
    PalmMuteSegment *pms = new PalmMuteSegment(this);
    pms->set_track(track());
    pms->init_element_style(&::mu::engraving::palm_mute_segment_style);
    return pms;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue PalmMute::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::LINE_WIDTH: return style().style_v(Sid::PALM_MUTE_LINE_WIDTH);

        case Pid::ALIGN: return Align(AlignH::LEFT, AlignV::BASELINE);

        case Pid::LINE_STYLE: return style().style_v(Sid::PALM_MUTE_LINE_STYLE);

        case Pid::LINE_VISIBLE: return true;

        case Pid::BEGIN_TEXT_OFFSET:
        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PropertyValue::from_value(PointF(0, 0));

        case Pid::BEGIN_TEXT:
        case Pid::CONTINUE_TEXT: return style().style_v(Sid::PALM_MUTE_TEXT);
        case Pid::END_TEXT: return "";

        case Pid::BEGIN_HOOK_TYPE: return HookType::NONE;

        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::AUTO;

        case Pid::TEXT_STYLE: return TextStyleType::PALM_MUTE;

        default: return TextLineBase::property_default(property_id);
    }
}

void PalmMute::set_channel() {
    EngravingItem *start_el = start_element();
    EngravingItem *end_el = end_element();

    if (!start_el || !end_el) {
        return;
    }

    if (!start_el->is_chord_rest() || !end_el->is_chord_rest()) {
        return;
    }

    ChordRest *start_cr = to_chord_rest(start_el);
    ChordRest *end_cr = to_chord_rest(end_el);

    Instrument *instrument = start_cr->origin_part()->instrument(start_cr->tick());
    int idx = instrument->channel_idx(String::from_utf8(InstrChannel::palm_mute_name));
    if (idx > 0) {
        staff()->insert_into_channel_list(voice(), start_cr->tick(), idx);
        staff()->insert_into_channel_list(voice(), end_cr->end_tick(), 0);
    }
}

Sid PalmMute::default_pos_sid() const { return place_above() ? Sid::PALM_MUTE_POS_ABOVE : Sid::PALM_MUTE_POS_BELOW; }
} // namespace mu::engraving
