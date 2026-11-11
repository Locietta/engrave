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

#include "textlinebase.h"

#include "engraving/rendering/score/scorerenderer.h"
#include "factory.h"
#include "log.h"
#include "score.h"
#include "system.h"
#include "text.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   TextLineBaseSegment
//---------------------------------------------------------

TextLineBaseSegment::TextLineBaseSegment(const ElementType &type, TextLineBase *sp, ElementFlags f) : LineSegment(type, sp, f) {
    text_ = Factory::create_text(this, TextStyleType::DEFAULT);
    end_text_ = Factory::create_text(this, TextStyleType::DEFAULT);
    text_->set_flag(ElementFlag::MOVABLE, false);
    end_text_->set_flag(ElementFlag::MOVABLE, false);
}

TextLineBaseSegment::TextLineBaseSegment(const TextLineBaseSegment &seg) : LineSegment(seg) {
    text_ = seg.text_->clone();
    end_text_ = seg.end_text_->clone();
    text_->set_ownership_parent(this);
    end_text_->set_ownership_parent(this);
    // set the right _text
    renderer()->layout_text_line_base_segment(this);
}

TextLineBaseSegment::~TextLineBaseSegment() {
    delete text_;
    delete end_text_;
}

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void TextLineBaseSegment::set_selected(bool f) {
    SpannerSegment::set_selected(f);
    text_->set_selected(f);
    end_text_->set_selected(f);
}

RectF TextLineBaseSegment::bounding_box_of_line(const PointF &p1, const PointF &p2, double lw2, bool is_dotted_line) {
    if (is_dotted_line) {
        return RectF(p1, p2).normalized().adjusted(-lw2, -lw2, lw2, lw2);
    }

    PointF a = lw2 * (p2 - p1).normalized();
    PointF b(-a.y(), a.x());
    return RectF(p1 - b, p1 + b).normalized().united(RectF(p2 - b, p2 + b).normalized());
}

bool TextLineBaseSegment::set_property(Pid id, const PropertyValue &v) {
    if (id == Pid::COLOR) {
        Color color = v.value<Color>();

        if (text_) {
            text_->set_color(color);
        }

        if (end_text_) {
            end_text_->set_color(color);
        }
    }

    return LineSegment::set_property(id, v);
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void TextLineBaseSegment::spatium_changed(double ov, double nv) {
    LineSegment::spatium_changed(ov, nv);

    text_line_base()->spatium_changed(ov, nv);
    text_->spatium_changed(ov, nv);
    end_text_->spatium_changed(ov, nv);
}

static constexpr std::array<Pid, 45> text_line_base_property_id = {{Pid::LINE_VISIBLE,
                                                                    Pid::BEGIN_HOOK_TYPE,
                                                                    Pid::BEGIN_HOOK_HEIGHT,
                                                                    Pid::BEGIN_LINE_ARROW_HEIGHT,
                                                                    Pid::BEGIN_LINE_ARROW_WIDTH,
                                                                    Pid::BEGIN_FILLED_ARROW_HEIGHT,
                                                                    Pid::BEGIN_FILLED_ARROW_WIDTH,
                                                                    Pid::END_HOOK_TYPE,
                                                                    Pid::END_HOOK_HEIGHT,
                                                                    Pid::END_LINE_ARROW_HEIGHT,
                                                                    Pid::END_LINE_ARROW_WIDTH,
                                                                    Pid::END_FILLED_ARROW_HEIGHT,
                                                                    Pid::END_FILLED_ARROW_WIDTH,
                                                                    Pid::GAP_BETWEEN_TEXT_AND_LINE,
                                                                    Pid::BEGIN_TEXT,
                                                                    Pid::BEGIN_TEXT_ALIGN,
                                                                    Pid::BEGIN_TEXT_POSITION,
                                                                    Pid::BEGIN_TEXT_PLACE,
                                                                    Pid::BEGIN_FONT_FACE,
                                                                    Pid::BEGIN_FONT_SIZE,
                                                                    Pid::BEGIN_FONT_STYLE,
                                                                    Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE,
                                                                    Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE,
                                                                    Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE,
                                                                    Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE,
                                                                    Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE,
                                                                    Pid::END_TEXT_MUSIC_SYMBOLS_SIZE,
                                                                    Pid::BEGIN_TEXT_OFFSET,
                                                                    Pid::CONTINUE_TEXT,
                                                                    Pid::CONTINUE_TEXT_ALIGN,
                                                                    Pid::CONTINUE_TEXT_POSITION,
                                                                    Pid::CONTINUE_TEXT_PLACE,
                                                                    Pid::CONTINUE_FONT_FACE,
                                                                    Pid::CONTINUE_FONT_SIZE,
                                                                    Pid::CONTINUE_FONT_STYLE,
                                                                    Pid::CONTINUE_TEXT_OFFSET,
                                                                    Pid::END_TEXT,
                                                                    Pid::END_TEXT_ALIGN,
                                                                    Pid::END_TEXT_POSITION,
                                                                    Pid::END_TEXT_PLACE,
                                                                    Pid::END_FONT_FACE,
                                                                    Pid::END_FONT_SIZE,
                                                                    Pid::END_FONT_STYLE,
                                                                    Pid::END_TEXT_OFFSET,
                                                                    Pid::TEXT_SIZE_SPATIUM_DEPENDENT}};

const std::array<Pid, 45> &TextLineBase::text_line_base_property_ids() { return ::mu::engraving::text_line_base_property_id; }

void TextLineBase::reset() {
    String begin_text = begin_text_value;
    String cont_text = continue_text_value;
    String end_text = end_text_value;
    bool begin_styled = is_styled(Pid::BEGIN_TEXT);
    bool cont_styled = is_styled(Pid::CONTINUE_TEXT);
    bool end_styled = is_styled(Pid::END_TEXT);

    SLine::reset();
    if (!begin_styled) {
        undo_change_property(Pid::BEGIN_TEXT, begin_text, PropertyFlags::UNSTYLED);
    }
    if (!cont_styled) {
        undo_change_property(Pid::CONTINUE_TEXT, cont_text, PropertyFlags::UNSTYLED);
    }
    if (!end_styled) {
        undo_change_property(Pid::END_TEXT, end_text, PropertyFlags::UNSTYLED);
    }
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *TextLineBaseSegment::property_delegate(Pid pid) const {
    if (pid == Pid::TEXT_STYLE) {
        return spanner();
    }

    for (Pid id : ::mu::engraving::text_line_base_property_id) {
        if (pid == id) {
            return spanner();
        }
    }
    return LineSegment::property_delegate(pid);
}

//---------------------------------------------------------
//   TextLineBase
//---------------------------------------------------------

TextLineBase::TextLineBase(const ElementType &type, EngravingObject *parent, ElementFlags f) : SLine(type, parent, f) {
    set_begin_hook_height(1.9_sp);
    set_end_hook_height(1.9_sp);
    set_gap_between_text_and_line(0.5_sp);
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void TextLineBase::spatium_changed(double /*ov*/, double /*nv*/) {}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TextLineBase::get_property(Pid id) const {
    switch (id) {
        case Pid::BEGIN_TEXT: return begin_text();
        case Pid::ALIGN: // Redirect to begin text...
        case Pid::BEGIN_TEXT_ALIGN: return PropertyValue::from_value(begin_text_align());
        case Pid::CONTINUE_TEXT_ALIGN: return PropertyValue::from_value(continue_text_align());
        case Pid::END_TEXT_ALIGN: return PropertyValue::from_value(end_text_align());
        case Pid::POSITION: // Redirect to begin text...
        case Pid::BEGIN_TEXT_POSITION: return PropertyValue::from_value(begin_text_position());
        case Pid::CONTINUE_TEXT_POSITION: return PropertyValue::from_value(continue_text_position());
        case Pid::END_TEXT_POSITION: return PropertyValue::from_value(end_text_position());
        case Pid::BEGIN_TEXT_PLACE: return begin_text_place_value;
        case Pid::BEGIN_HOOK_TYPE: return begin_hook_type_value;
        case Pid::BEGIN_HOOK_HEIGHT: return begin_hook_height_value;
        case Pid::BEGIN_LINE_ARROW_HEIGHT: return begin_line_arrow_height_value;
        case Pid::BEGIN_LINE_ARROW_WIDTH: return begin_line_arrow_width_value;
        case Pid::BEGIN_FILLED_ARROW_HEIGHT: return begin_filled_arrow_height_value;
        case Pid::BEGIN_FILLED_ARROW_WIDTH: return begin_filled_arrow_width_value;
        case Pid::FONT_FACE: // Redirect to begin text...
        case Pid::BEGIN_FONT_FACE: return begin_font_family_value;
        case Pid::FONT_SIZE: // Redirect to begin text...
        case Pid::BEGIN_FONT_SIZE: return begin_font_size_value;
        case Pid::FONT_STYLE: // Redirect to begin text...
        case Pid::BEGIN_FONT_STYLE: return int(begin_font_style_value);
        case Pid::MUSIC_SYMBOL_SIZE: // Redirect to begin text...
        case Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE: return begin_text_music_symbols_size_value;
        case Pid::MUSICAL_SYMBOLS_SCALE: // Redirect to begin text...
        case Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE: return begin_text_musical_symbols_scale_value;
        case Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE: return continue_text_musical_symbols_scale_value;
        case Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE: return continue_text_music_symbols_size_value;
        case Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE: return end_text_musical_symbols_scale_value;
        case Pid::END_TEXT_MUSIC_SYMBOLS_SIZE: return end_text_music_symbols_size_value;
        case Pid::BEGIN_TEXT_OFFSET: return begin_text_offset_value;
        case Pid::CONTINUE_TEXT: return continue_text();
        case Pid::CONTINUE_TEXT_PLACE: return continue_text_place_value;
        case Pid::CONTINUE_FONT_FACE: return continue_font_family_value;
        case Pid::CONTINUE_FONT_SIZE: return continue_font_size_value;
        case Pid::CONTINUE_FONT_STYLE: return int(continue_font_style_value);
        case Pid::CONTINUE_TEXT_OFFSET: return continue_text_offset_value;
        case Pid::END_TEXT: return end_text();
        case Pid::END_TEXT_PLACE: return end_text_place_value;
        case Pid::END_HOOK_TYPE: return end_hook_type_value;
        case Pid::END_HOOK_HEIGHT: return end_hook_height_value;
        case Pid::END_LINE_ARROW_HEIGHT: return end_line_arrow_height_value;
        case Pid::END_LINE_ARROW_WIDTH: return end_line_arrow_width_value;
        case Pid::END_FILLED_ARROW_HEIGHT: return end_filled_arrow_height_value;
        case Pid::END_FILLED_ARROW_WIDTH: return end_filled_arrow_width_value;
        case Pid::GAP_BETWEEN_TEXT_AND_LINE: return gap_between_text_and_line_value;
        case Pid::END_FONT_FACE: return end_font_family_value;
        case Pid::END_FONT_SIZE: return end_font_size_value;
        case Pid::END_FONT_STYLE: return int(end_font_style_value);
        case Pid::END_TEXT_OFFSET: return end_text_offset_value;
        case Pid::LINE_VISIBLE: return line_visible();
        case Pid::TEXT_SIZE_SPATIUM_DEPENDENT: return text_size_spatium_dependent();
        default: return SLine::get_property(id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TextLineBase::set_property(Pid id, const PropertyValue &v) {
    switch (id) {
        case Pid::BEGIN_TEXT_PLACE: begin_text_place_value = v.value<TextPlace>(); break;
        case Pid::BEGIN_TEXT_ALIGN: begin_text_align_value = v.value<Align>(); break;
        case Pid::BEGIN_TEXT_POSITION: begin_text_position_value = v.value<AlignH>(); break;
        case Pid::BEGIN_FONT_FACE: set_begin_font_family(v.value<String>()); break;
        case Pid::BEGIN_FONT_SIZE:
            if (v.to_real() <= 0) {
                ASSERT_X(String(u"font size is %1").arg(v.to_real()));
            }
            set_begin_font_size(v.to_real());
            break;
        case Pid::BEGIN_FONT_STYLE: set_begin_font_style(FontStyle(v.to_int())); break;
        case Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE: set_begin_text_musical_symbols_scale(v.to_real()); break;
        case Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE: set_begin_text_music_symbols_size(v.to_real()); break;
        case Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE: set_continue_text_musical_symbols_scale(v.to_real()); break;
        case Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE: set_continue_text_music_symbols_size(v.to_real()); break;
        case Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE: set_end_text_musical_symbols_scale(v.to_real()); break;
        case Pid::END_TEXT_MUSIC_SYMBOLS_SIZE: set_end_text_music_symbols_size(v.to_real()); break;

        case Pid::CONTINUE_TEXT_ALIGN: continue_text_align_value = v.value<Align>(); break;
        case Pid::END_TEXT_ALIGN: end_text_align_value = v.value<Align>(); break;
        case Pid::CONTINUE_TEXT_POSITION: continue_text_position_value = v.value<AlignH>(); break;
        case Pid::END_TEXT_POSITION: end_text_position_value = v.value<AlignH>(); break;
        case Pid::CONTINUE_TEXT_PLACE: continue_text_place_value = v.value<TextPlace>(); break;
        case Pid::END_TEXT_PLACE: end_text_place_value = v.value<TextPlace>(); break;
        case Pid::CONTINUE_FONT_FACE: set_continue_font_family(v.value<String>()); break;
        case Pid::CONTINUE_FONT_SIZE: set_continue_font_size(v.to_real()); break;
        case Pid::CONTINUE_FONT_STYLE: set_continue_font_style(FontStyle(v.to_int())); break;
        case Pid::END_FONT_FACE: set_end_font_family(v.value<String>()); break;
        case Pid::END_FONT_SIZE: set_end_font_size(v.to_real()); break;
        case Pid::END_FONT_STYLE: set_end_font_style(FontStyle(v.to_int())); break;

        case Pid::BEGIN_HOOK_HEIGHT: begin_hook_height_value = v.value<Spatium>(); break;
        case Pid::END_HOOK_HEIGHT: end_hook_height_value = v.value<Spatium>(); break;
        case Pid::BEGIN_HOOK_TYPE: begin_hook_type_value = v.value<HookType>(); break;
        case Pid::END_HOOK_TYPE: end_hook_type_value = v.value<HookType>(); break;

        case Pid::BEGIN_FILLED_ARROW_HEIGHT: begin_filled_arrow_height_value = v.value<Spatium>(); break;
        case Pid::BEGIN_FILLED_ARROW_WIDTH: begin_filled_arrow_width_value = v.value<Spatium>(); break;
        case Pid::END_FILLED_ARROW_HEIGHT: end_filled_arrow_height_value = v.value<Spatium>(); break;
        case Pid::END_FILLED_ARROW_WIDTH: end_filled_arrow_width_value = v.value<Spatium>(); break;
        case Pid::BEGIN_LINE_ARROW_HEIGHT: begin_line_arrow_height_value = v.value<Spatium>(); break;
        case Pid::BEGIN_LINE_ARROW_WIDTH: begin_line_arrow_width_value = v.value<Spatium>(); break;
        case Pid::END_LINE_ARROW_HEIGHT: end_line_arrow_height_value = v.value<Spatium>(); break;
        case Pid::END_LINE_ARROW_WIDTH: end_line_arrow_width_value = v.value<Spatium>(); break;
        case Pid::BEGIN_TEXT: set_begin_text(v.value<String>()); break;
        case Pid::BEGIN_TEXT_OFFSET: set_begin_text_offset(v.value<PointF>()); break;
        case Pid::GAP_BETWEEN_TEXT_AND_LINE: gap_between_text_and_line_value = v.value<Spatium>(); break;
        case Pid::CONTINUE_TEXT_OFFSET: set_continue_text_offset(v.value<PointF>()); break;
        case Pid::END_TEXT_OFFSET: set_end_text_offset(v.value<PointF>()); break;
        case Pid::CONTINUE_TEXT: set_continue_text(v.value<String>()); break;
        case Pid::END_TEXT: set_end_text(v.value<String>()); break;
        case Pid::LINE_VISIBLE: set_line_visible(v.to_bool()); break;
        case Pid::TEXT_SIZE_SPATIUM_DEPENDENT: set_text_size_spatium_dependent(v.to_bool()); break;

        /// In >=4.6 text line begin, continue, and end text should all have the same style. This preserves backwards
        /// compatibility and will allow us to style these text items separately in the future if desired

        // These are TextBase properties - updating these should update begin, continue, and end text...
        case Pid::ALIGN: {
            const Align align(v.value<Align>());
            begin_text_align_value = align;
            continue_text_align_value = align;
            end_text_align_value = align;
            break;
        }
        case Pid::POSITION: {
            const AlignH align_h(v.value<AlignH>());
            begin_text_position_value = align_h;
            continue_text_position_value = align_h;
            end_text_position_value = align_h;
            break;
        }
        case Pid::FONT_FACE: {
            const String face(v.value<String>());
            set_begin_font_family(face);
            set_continue_font_family(face);
            set_end_font_family(face);
            break;
        }
        case Pid::FONT_SIZE: {
            const double size(v.to_real());
            if (size <= 0) {
                ASSERT_X(String(u"font size is %1").arg(size));
            }
            set_begin_font_size(size);
            set_continue_font_size(size);
            set_end_font_size(size);
            break;
        }
        case Pid::FONT_STYLE: {
            const FontStyle style = FontStyle(v.to_int());
            set_begin_font_style(style);
            set_continue_font_style(style);
            set_end_font_style(style);
            break;
        }
        case Pid::MUSIC_SYMBOL_SIZE: {
            const double size = v.to_real();
            set_begin_text_music_symbols_size(size);
            set_continue_text_music_symbols_size(size);
            set_end_text_music_symbols_size(size);
            break;
        }
        case Pid::MUSICAL_SYMBOLS_SCALE: {
            const double scale = v.to_real();
            set_begin_text_musical_symbols_scale(scale);
            set_continue_text_musical_symbols_scale(scale);
            set_end_text_musical_symbols_scale(scale);
            break;
        }

        default: return SLine::set_property(id, v);
    }
    trigger_layout();
    return true;
}

PropertyValue TextLineBase::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::GAP_BETWEEN_TEXT_AND_LINE: return 0.5_sp;

        // Redirect to begin text for these...
        case Pid::ALIGN: return property_default(Pid::BEGIN_TEXT_ALIGN);
        case Pid::POSITION: return property_default(Pid::BEGIN_TEXT_POSITION);
        case Pid::FONT_FACE: return property_default(Pid::BEGIN_FONT_FACE);
        case Pid::FONT_SIZE: return property_default(Pid::BEGIN_FONT_SIZE);
        case Pid::FONT_STYLE: return property_default(Pid::BEGIN_FONT_STYLE);
        case Pid::MUSICAL_SYMBOLS_SCALE: return property_default(Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE);
        case Pid::MUSIC_SYMBOL_SIZE: return property_default(Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE);

        default: return SLine::property_default(property_id);
    }
}

PropertyFlags TextLineBase::property_flags(Pid id) const {
    switch (id) {
        case Pid::ALIGN: return property_flags(Pid::BEGIN_TEXT_ALIGN);
        case Pid::POSITION: return property_flags(Pid::BEGIN_TEXT_POSITION);
        case Pid::FONT_FACE: return property_flags(Pid::BEGIN_FONT_FACE);
        case Pid::FONT_SIZE: return property_flags(Pid::BEGIN_FONT_SIZE);
        case Pid::FONT_STYLE: return property_flags(Pid::BEGIN_FONT_STYLE);
        case Pid::MUSICAL_SYMBOLS_SCALE: return property_flags(Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE);
        case Pid::MUSIC_SYMBOL_SIZE: return property_flags(Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE);

        default: return SLine::property_flags(id);
    }
}

void TextLineBase::set_property_flags(Pid id, PropertyFlags f) {
    switch (id) {
        case Pid::ALIGN: {
            set_property_flags(Pid::BEGIN_TEXT_ALIGN, f);
            set_property_flags(Pid::CONTINUE_TEXT_ALIGN, f);
            set_property_flags(Pid::END_TEXT_ALIGN, f);
            break;
        }
        case Pid::POSITION: {
            set_property_flags(Pid::BEGIN_TEXT_POSITION, f);
            set_property_flags(Pid::CONTINUE_TEXT_POSITION, f);
            set_property_flags(Pid::END_TEXT_POSITION, f);
            break;
        }
        case Pid::FONT_FACE: {
            set_property_flags(Pid::BEGIN_FONT_FACE, f);
            set_property_flags(Pid::CONTINUE_FONT_FACE, f);
            set_property_flags(Pid::END_FONT_FACE, f);
            break;
        }
        case Pid::FONT_SIZE: {
            set_property_flags(Pid::BEGIN_FONT_SIZE, f);
            set_property_flags(Pid::CONTINUE_FONT_SIZE, f);
            set_property_flags(Pid::END_FONT_SIZE, f);
            break;
        }
        case Pid::FONT_STYLE: {
            set_property_flags(Pid::BEGIN_FONT_STYLE, f);
            set_property_flags(Pid::CONTINUE_FONT_STYLE, f);
            set_property_flags(Pid::END_FONT_STYLE, f);
            break;
        }
        case Pid::MUSICAL_SYMBOLS_SCALE: {
            set_property_flags(Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE, f);
            set_property_flags(Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE, f);
            set_property_flags(Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE, f);
            break;
        }
        case Pid::MUSIC_SYMBOL_SIZE: {
            set_property_flags(Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE, f);
            set_property_flags(Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE, f);
            set_property_flags(Pid::END_TEXT_MUSIC_SYMBOLS_SIZE, f);
            break;
        }

        default: SLine::set_property_flags(id, f);
    }
}
} // namespace mu::engraving
