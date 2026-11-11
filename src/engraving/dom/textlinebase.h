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

#ifndef MU_ENGRAVING_TEXTLINEBASE_H
#define MU_ENGRAVING_TEXTLINEBASE_H

#include "../types/types.h"
#include "line.h"
#include "property.h"

namespace mu::engraving {
struct EngravingItem;
struct Text;
struct TextLineBase;

//---------------------------------------------------------
//   @@ TextLineBaseSegment
//---------------------------------------------------------

struct TextLineBaseSegment : LineSegment {

public:
    TextLineBaseSegment(const ElementType &type, TextLineBase *, ElementFlags f = ElementFlag::NOTHING);
    TextLineBaseSegment(const TextLineBaseSegment &);
    ~TextLineBaseSegment();

    TextLineBase *text_line_base() const { return (TextLineBase *) spanner(); }

    void set_selected(bool f) override;

    void spatium_changed(double /*oldValue*/, double /*newValue*/) override;

    EngravingObject *property_delegate(Pid) const override;

    bool set_property(Pid id, const PropertyValue &v) override;

    bool two_lines() const { return two_lines_; }

    void set_two_lines(bool val) { two_lines_ = val; }

    Text *text() const { return text_; }

    Text *end_text() const { return end_text_; }

    struct LayoutData : public EngravingItem::LayoutData {
        std::array<PointF, 6> points;
        PolygonF joined_hairpin = PolygonF();
        PolygonF begin_arrow = PolygonF();
        PolygonF end_arrow = PolygonF();
        int npoints = 0;
        double line_length = 0;
    };
    DECLARE_LAYOUTDATA_METHODS(TextLineBaseSegment)

    static RectF bounding_box_of_line(const PointF &p1, const PointF &p2, double lw2, bool is_dotted_line);

protected:

    Text *text_ = nullptr;
    Text *end_text_ = nullptr;

    bool two_lines_ = false;
};

//---------------------------------------------------------
//   @@ TextLineBase
//---------------------------------------------------------

struct TextLineBase : SLine {
private:

    M_PROPERTY(bool, line_visible, set_line_visible)
    M_PROPERTY2(HookType, begin_hook_type, set_begin_hook_type, HookType::NONE)
    M_PROPERTY2(HookType, end_hook_type, set_end_hook_type, HookType::NONE)
    M_PROPERTY(Spatium, begin_hook_height, set_begin_hook_height)
    M_PROPERTY(Spatium, end_hook_height, set_end_hook_height)
    M_PROPERTY(Spatium, begin_line_arrow_height, set_begin_line_arrow_height)
    M_PROPERTY(Spatium, begin_line_arrow_width, set_begin_line_arrow_width)
    M_PROPERTY(Spatium, begin_filled_arrow_height, set_begin_filled_arrow_height)
    M_PROPERTY(Spatium, begin_filled_arrow_width, set_begin_filled_arrow_width)
    M_PROPERTY(Spatium, end_line_arrow_height, set_end_line_arrow_height)
    M_PROPERTY(Spatium, end_line_arrow_width, set_end_line_arrow_width)
    M_PROPERTY(Spatium, end_filled_arrow_height, set_end_filled_arrow_height)
    M_PROPERTY(Spatium, end_filled_arrow_width, set_end_filled_arrow_width)
    M_PROPERTY(Spatium, gap_between_text_and_line, set_gap_between_text_and_line)

    M_PROPERTY2(TextPlace, begin_text_place, set_begin_text_place, TextPlace::AUTO)
    M_PROPERTY(String, begin_text, set_begin_text)
    M_PROPERTY(Align, begin_text_align, set_begin_text_align)
    M_PROPERTY(AlignH, begin_text_position, set_begin_text_position)
    M_PROPERTY(String, begin_font_family, set_begin_font_family)
    M_PROPERTY(double, begin_font_size, set_begin_font_size)
    M_PROPERTY(FontStyle, begin_font_style, set_begin_font_style)
    M_PROPERTY2(double, begin_text_musical_symbols_scale, set_begin_text_musical_symbols_scale, 1.0)
    M_PROPERTY2(double, begin_text_music_symbols_size, set_begin_text_music_symbols_size, 12.0)
    M_PROPERTY(PointF, begin_text_offset, set_begin_text_offset)

    M_PROPERTY2(TextPlace, continue_text_place, set_continue_text_place, TextPlace::AUTO)
    M_PROPERTY2(double, continue_text_musical_symbols_scale, set_continue_text_musical_symbols_scale, 1.0)
    M_PROPERTY2(double, continue_text_music_symbols_size, set_continue_text_music_symbols_size, 12.0)
    M_PROPERTY(String, continue_text, set_continue_text)
    M_PROPERTY(Align, continue_text_align, set_continue_text_align)
    M_PROPERTY(AlignH, continue_text_position, set_continue_text_position)
    M_PROPERTY(String, continue_font_family, set_continue_font_family)
    M_PROPERTY(double, continue_font_size, set_continue_font_size)
    M_PROPERTY(FontStyle, continue_font_style, set_continue_font_style)
    M_PROPERTY(PointF, continue_text_offset, set_continue_text_offset)

    M_PROPERTY2(TextPlace, end_text_place, set_end_text_place, TextPlace::AUTO)
    M_PROPERTY2(double, end_text_musical_symbols_scale, set_end_text_musical_symbols_scale, 1.0)
    M_PROPERTY2(double, end_text_music_symbols_size, set_end_text_music_symbols_size, 12.0)
    M_PROPERTY(String, end_text, set_end_text)
    M_PROPERTY(Align, end_text_align, set_end_text_align)
    M_PROPERTY(AlignH, end_text_position, set_end_text_position)
    M_PROPERTY(String, end_font_family, set_end_font_family)
    M_PROPERTY(double, end_font_size, set_end_font_size)
    M_PROPERTY(FontStyle, end_font_style, set_end_font_style)
    M_PROPERTY(PointF, end_text_offset, set_end_text_offset)
    M_PROPERTY(bool, text_size_spatium_dependent, set_text_size_spatium_dependent)

public:
    TextLineBase(const ElementType &type, EngravingObject *parent, ElementFlags = ElementFlag::NOTHING);

    void spatium_changed(double /*oldValue*/, double /*newValue*/) override;

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;
    PropertyFlags property_flags(Pid) const override;
    void set_property_flags(Pid, PropertyFlags) override;

    static const std::array<Pid, 45> &text_line_base_property_ids();

    void reset() override;

protected:
    friend struct TextLineBaseSegment;
};

inline bool is_system_text_line(const EngravingItem *element) { return element && element->is_text_line_base() && element->system_flag(); }
} // namespace mu::engraving

#endif
