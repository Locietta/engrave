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
#include "textline.h"

#include "../editing/editproperty.h"
#include "score.h"
#include "system.h"
#include "text.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   textLineSegmentStyle
//---------------------------------------------------------

static const ElementStyle text_line_segment_style{
    {Sid::TEXT_LINE_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   systemTextLineSegmentStyle
//---------------------------------------------------------

static const ElementStyle system_text_line_segment_style{
    {Sid::SYSTEM_TEXT_LINE_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   textLineStyle
//---------------------------------------------------------

static const ElementStyle text_line_style{
    {Sid::TEXT_LINE_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::TEXT_LINE_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::TEXT_LINE_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::TEXT_LINE_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::TEXT_LINE_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::TEXT_LINE_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::TEXT_LINE_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::TEXT_LINE_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::TEXT_LINE_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::TEXT_LINE_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::TEXT_LINE_TEXT_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::TEXT_LINE_TEXT_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::TEXT_LINE_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::TEXT_LINE_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::TEXT_LINE_POSITION, Pid::END_TEXT_POSITION},
    {Sid::TEXT_LINE_HOOK_HEIGHT, Pid::BEGIN_HOOK_HEIGHT},
    {Sid::TEXT_LINE_HOOK_HEIGHT, Pid::END_HOOK_HEIGHT},
    {Sid::TEXT_LINE_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::TEXT_LINE_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::TEXT_LINE_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::TEXT_LINE_PLACEMENT, Pid::PLACEMENT},
    {Sid::TEXT_LINE_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::TEXT_LINE_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
    {Sid::TEXT_LINE_END_LINE_ARROW_HEIGHT, Pid::END_LINE_ARROW_HEIGHT},
    {Sid::TEXT_LINE_END_LINE_ARROW_WIDTH, Pid::END_LINE_ARROW_WIDTH},
    {Sid::TEXT_LINE_BEGIN_LINE_ARROW_HEIGHT, Pid::BEGIN_LINE_ARROW_HEIGHT},
    {Sid::TEXT_LINE_BEGIN_LINE_ARROW_WIDTH, Pid::BEGIN_LINE_ARROW_WIDTH},
    {Sid::TEXT_LINE_END_FILLED_ARROW_HEIGHT, Pid::END_FILLED_ARROW_HEIGHT},
    {Sid::TEXT_LINE_END_FILLED_ARROW_WIDTH, Pid::END_FILLED_ARROW_WIDTH},
    {Sid::TEXT_LINE_BEGIN_FILLED_ARROW_HEIGHT, Pid::BEGIN_FILLED_ARROW_HEIGHT},
    {Sid::TEXT_LINE_BEGIN_FILLED_ARROW_WIDTH, Pid::BEGIN_FILLED_ARROW_WIDTH},
    {Sid::TEXT_LINE_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::TEXT_LINE_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::TEXT_LINE_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
};

//---------------------------------------------------------
//   systemTextLineStyle
//---------------------------------------------------------

static const ElementStyle system_text_line_style{
    {Sid::SYSTEM_TEXT_LINE_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::SYSTEM_TEXT_LINE_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::SYSTEM_TEXT_LINE_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::SYSTEM_TEXT_LINE_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::SYSTEM_TEXT_LINE_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::SYSTEM_TEXT_LINE_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::SYSTEM_TEXT_LINE_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::SYSTEM_TEXT_LINE_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::SYSTEM_TEXT_LINE_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::SYSTEM_TEXT_LINE_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::SYSTEM_TEXT_LINE_TEXT_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::SYSTEM_TEXT_LINE_TEXT_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::SYSTEM_TEXT_LINE_HOOK_HEIGHT, Pid::BEGIN_HOOK_HEIGHT},
    {Sid::SYSTEM_TEXT_LINE_HOOK_HEIGHT, Pid::END_HOOK_HEIGHT},
    {Sid::SYSTEM_TEXT_LINE_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::SYSTEM_TEXT_LINE_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::SYSTEM_TEXT_LINE_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::SYSTEM_TEXT_LINE_PLACEMENT, Pid::PLACEMENT},
    {Sid::SYSTEM_TEXT_LINE_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::SYSTEM_TEXT_LINE_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::SYSTEM_TEXT_LINE_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::SYSTEM_TEXT_LINE_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::SYSTEM_TEXT_LINE_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
};

//---------------------------------------------------------
//   TextLineSegment
//---------------------------------------------------------

TextLineSegment::TextLineSegment(TextLine *sp, bool system)
    : TextLineBaseSegment(ElementType::TEXTLINE_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    set_system_flag(system);
    init_style();

    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *TextLineSegment::property_delegate(Pid pid) const {
    if (pid == Pid::SYSTEM_FLAG) {
        return static_cast<TextLine *>(spanner());
    }
    return TextLineBaseSegment::property_delegate(pid);
}

//---------------------------------------------------------
//   TextLine
//---------------------------------------------------------

TextLine::TextLine(DummyParentOr<EngravingItem> parent, bool system) : TextLineBase(ElementType::TEXTLINE, parent) {
    set_system_flag(system);

    init_style();

    set_begin_text(u"");
    set_continue_text(u"");
    set_end_text(u"");
    set_begin_text_offset(PointF(0, 0));
    set_continue_text_offset(PointF(0, 0));
    set_end_text_offset(PointF(0, 0));
    set_line_visible(true);

    set_begin_hook_type(HookType::NONE);
    set_end_hook_type(HookType::NONE);
    set_begin_hook_height(1.5_sp);
    set_end_hook_height(1.5_sp);
    set_gap_between_text_and_line(0.5_sp);

    set_begin_filled_arrow_height(1.0_sp);
    set_begin_filled_arrow_width(0.85_sp);
    set_end_filled_arrow_height(1.0_sp);
    set_end_filled_arrow_width(0.85_sp);

    set_begin_line_arrow_height(1.0_sp);
    set_begin_line_arrow_width(0.5_sp);
    set_end_line_arrow_height(1.0_sp);
    set_end_line_arrow_width(0.5_sp);

    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::CONTINUE_TEXT_PLACE);
    reset_property(Pid::END_TEXT_PLACE);
}

TextLine::TextLine(const TextLine &tl) : TextLineBase(tl) {}

//---------------------------------------------------------
//   initStyle
//---------------------------------------------------------

void TextLine::init_style() {
    if (system_flag()) {
        init_element_style(&::mu::engraving::system_text_line_style);
    } else {
        init_element_style(&::mu::engraving::text_line_style);
    }
}

void TextLineSegment::init_style() {
    if (system_flag()) {
        init_element_style(&::mu::engraving::system_text_line_segment_style);
    } else {
        init_element_style(&::mu::engraving::text_line_segment_style);
    }
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment *TextLine::create_line_segment() {
    TextLineSegment *seg = new TextLineSegment(this, system_flag());
    seg->set_track(track());
    // note-anchored line segments are relative to system not to staff
    if (anchor() == Spanner::Anchor::NOTE) {
        seg->set_flag(ElementFlag::ON_STAFF, false);
    }
    seg->init_style();

    return seg;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TextLine::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::BEGIN_TEXT:
        case Pid::CONTINUE_TEXT:
        case Pid::END_TEXT: return "";
        case Pid::LINE_VISIBLE: return true;
        case Pid::BEGIN_TEXT_OFFSET:
        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PointF(0, 0);
        case Pid::BEGIN_HOOK_TYPE:
        case Pid::END_HOOK_TYPE: return HookType::NONE;
        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::LEFT;
        case Pid::TEXT_STYLE: return system_flag() ? TextStyleType::SYSTEM_TEXTLINE : TextStyleType::TEXTLINE;
        default: return TextLineBase::property_default(property_id);
    }
}

//---------------------------------------------------------
//   allowTimeAnchor
//---------------------------------------------------------
bool TextLine::allow_time_anchor() const { return !(anchor() == Spanner::Anchor::NOTE); }

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TextLine::set_property(Pid id, const engraving::PropertyValue &v) {
    switch (id) {
        case Pid::PLAY: break;
        case Pid::PLACEMENT: set_placement(v.value<PlacementV>()); break;
        default: return TextLineBase::set_property(id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TextLine::get_property(Pid id) const {
    switch (id) {
        case Pid::PLAY: return PropertyValue();
        default: break;
    }
    return TextLineBase::get_property(id);
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void TextLine::undo_change_property(Pid id, const engraving::PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::SYSTEM_FLAG) {
        score()->undo(new ChangeTextLineProperty(this, v));
        for (SpannerSegment *s : spanner_segments()) {
            score()->undo(new ChangeTextLineProperty(s, v));
            trigger_layout();
        }
        return;
    }
    TextLineBase::undo_change_property(id, v, ps);
}

Sid TextLine::default_pos_sid() const {
    if (system_flag()) {
        return place_above() ? Sid::SYSTEM_TEXT_LINE_POS_ABOVE : Sid::SYSTEM_TEXT_LINE_POS_BELOW;
    }
    return place_above() ? Sid::TEXT_LINE_POS_ABOVE : Sid::TEXT_LINE_POS_BELOW;
}
} // namespace mu::engraving
