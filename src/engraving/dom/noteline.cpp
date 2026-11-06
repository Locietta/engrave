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

#include "note.h"
#include "noteline.h"
#include "text.h"

namespace mu::engraving {
static const ElementStyle note_line_style{
    {Sid::NOTE_LINE_PLACEMENT, Pid::PLACEMENT},
    {Sid::NOTE_LINE_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::NOTE_LINE_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::NOTE_LINE_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::NOTE_LINE_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::NOTE_LINE_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::NOTE_LINE_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::NOTE_LINE_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::NOTE_LINE_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::NOTE_LINE_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::NOTE_LINE_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::NOTE_LINE_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::NOTE_LINE_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::NOTE_LINE_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::NOTE_LINE_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::NOTE_LINE_POSITION, Pid::END_TEXT_POSITION},
    {Sid::NOTE_LINE_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
    {Sid::NOTE_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::NOTE_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::NOTE_LINE_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::NOTE_LINE_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::NOTE_LINE_END_LINE_ARROW_HEIGHT, Pid::END_LINE_ARROW_HEIGHT},
    {Sid::NOTE_LINE_END_LINE_ARROW_WIDTH, Pid::END_LINE_ARROW_WIDTH},
    {Sid::NOTE_LINE_BEGIN_LINE_ARROW_HEIGHT, Pid::BEGIN_LINE_ARROW_HEIGHT},
    {Sid::NOTE_LINE_BEGIN_LINE_ARROW_WIDTH, Pid::BEGIN_LINE_ARROW_WIDTH},
    {Sid::NOTE_LINE_END_FILLED_ARROW_HEIGHT, Pid::END_FILLED_ARROW_HEIGHT},
    {Sid::NOTE_LINE_END_FILLED_ARROW_WIDTH, Pid::END_FILLED_ARROW_WIDTH},
    {Sid::NOTE_LINE_BEGIN_FILLED_ARROW_HEIGHT, Pid::BEGIN_FILLED_ARROW_HEIGHT},
    {Sid::NOTE_LINE_BEGIN_FILLED_ARROW_WIDTH, Pid::BEGIN_FILLED_ARROW_WIDTH},
    {Sid::NOTE_LINE_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::NOTE_LINE_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::NOTE_LINE_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
};

NoteLineSegment::NoteLineSegment(NoteLine *sp) : TextLineBaseSegment(ElementType::NOTELINE_SEGMENT, sp, ElementFlag::MOVABLE) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

EngravingObject *NoteLineSegment::property_delegate(Pid pid) const {
    if (pid == Pid::NOTELINE_PLACEMENT) {
        return note_line();
    }

    return TextLineBaseSegment::property_delegate(pid);
}

NoteLine::NoteLine(EngravingObject *parent) : TextLineBase(ElementType::NOTELINE, parent, ElementFlag::MOVABLE) {
    init_element_style(&::mu::engraving::note_line_style);

    static const std::array<Pid, 18> properties_to_initialise{Pid::BEGIN_TEXT,
                                                              Pid::CONTINUE_TEXT,
                                                              Pid::END_TEXT,
                                                              Pid::LINE_VISIBLE,
                                                              Pid::BEGIN_TEXT_OFFSET,
                                                              Pid::CONTINUE_TEXT_OFFSET,
                                                              Pid::END_TEXT_OFFSET,
                                                              Pid::BEGIN_HOOK_TYPE,
                                                              Pid::END_HOOK_TYPE,
                                                              Pid::BEGIN_TEXT_PLACE,
                                                              Pid::CONTINUE_TEXT_PLACE,
                                                              Pid::END_TEXT_PLACE,
                                                              Pid::BEGIN_HOOK_HEIGHT,
                                                              Pid::END_HOOK_HEIGHT,
                                                              Pid::DIAGONAL,
                                                              Pid::NOTELINE_PLACEMENT,
                                                              Pid::GAP_BETWEEN_TEXT_AND_LINE,
                                                              Pid::SYSTEM_FLAG};

    for (const Pid &pid : properties_to_initialise) {
        reset_property(pid);
    }
}

NoteLine::NoteLine(const NoteLine &nl) : TextLineBase(nl) {}

LineSegment *NoteLine::create_line_segment() {
    NoteLineSegment *seg = new NoteLineSegment(this);
    seg->set_track(track());
    return seg;
}

PropertyValue NoteLine::property_default(Pid property_id) const {
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
        case Pid::BEGIN_HOOK_HEIGHT:
        case Pid::END_HOOK_HEIGHT: return 1.5_sp;
        case Pid::PLACEMENT: return PlacementV::ABOVE;
        case Pid::DIAGONAL: return true;
        case Pid::NOTELINE_PLACEMENT: return NoteLineEndPlacement::OFFSET_ENDS;
        case Pid::GAP_BETWEEN_TEXT_AND_LINE: return 0.5_sp;
        case Pid::SYSTEM_FLAG: return false;
        case Pid::TEXT_STYLE: return TextStyleType::NOTELINE;
        default: return TextLineBase::property_default(property_id);
    }
}

PropertyValue NoteLine::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::PLAY: return PropertyValue();
        case Pid::PLACEMENT: return PlacementV::ABOVE;
        case Pid::NOTELINE_PLACEMENT: return line_end_placement_;
        default: return TextLineBase::get_property(property_id);
    }
}

bool NoteLine::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::PLAY: break;
        case Pid::NOTELINE_PLACEMENT: set_line_end_placement(val.value<NoteLineEndPlacement>()); break;
        default: return TextLineBase::set_property(property_id, val);
    }

    return true;
}

void NoteLine::reset() {
    undo_reset_property(Pid::NOTELINE_PLACEMENT);
    TextLineBase::reset();
}

Sid NoteLine::default_pos_sid() const { return Sid::NOSTYLE; }
} // namespace mu::engraving
