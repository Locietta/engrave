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

#include "layoutbreak.h"

#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "log.h"
#include "measurebase.h"
#include "score.h"
#include "types/typesconv.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   sectionBreakStyle
//---------------------------------------------------------

static const ElementStyle section_break_style{{Sid::SECTION_PAUSE, Pid::PAUSE}};

//---------------------------------------------------------
//   LayoutBreak
//---------------------------------------------------------

LayoutBreak::LayoutBreak(DummyParentOr<MeasureBase> parent) : EngravingItem(ElementType::LAYOUT_BREAK, parent, ElementFlag::SYSTEM) {
    pause_ = 0.;
    start_with_long_names_ = false;
    start_with_measure_one_ = false;
    first_system_indentation_ = false;
    show_courtesy_ = false;
    layout_break_type_ = LayoutBreakType::PAGE;

    init_element_style(&::mu::engraving::section_break_style);

    reset_property(Pid::PAUSE);
    reset_property(Pid::START_WITH_LONG_NAMES);
    reset_property(Pid::START_WITH_MEASURE_ONE);
    reset_property(Pid::FIRST_SYSTEM_INDENTATION);
    reset_property(Pid::SHOW_COURTESY);
}

LayoutBreak::LayoutBreak(const LayoutBreak &lb) : EngravingItem(lb) {
    layout_break_type_ = lb.layout_break_type_;
    pause_ = lb.pause_;
    start_with_long_names_ = lb.start_with_long_names_;
    start_with_measure_one_ = lb.start_with_measure_one_;
    first_system_indentation_ = lb.first_system_indentation_;
    show_courtesy_ = lb.show_courtesy_;
}

void LayoutBreak::set_ownership_parent(DummyParentOr<MeasureBase> parent) { EngravingItem::set_ownership_parent(parent); }

char16_t LayoutBreak::icon_code() const {
    switch (layout_break_type_) {
        case LayoutBreakType::LINE: return 0xF483;
        case LayoutBreakType::PAGE: return 0xF484;
        case LayoutBreakType::SECTION: return 0xF485;
        case LayoutBreakType::NOBREAK: return 0xF486;
        default: return 0x000;
    }
}

//---------------------------------------------------------
//   setLayoutBreakType
//---------------------------------------------------------

void LayoutBreak::set_layout_break_type(LayoutBreakType val) { layout_break_type_ = val; }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue LayoutBreak::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::LAYOUT_BREAK: return layout_break_type_;
        case Pid::PAUSE: return pause_;
        case Pid::START_WITH_LONG_NAMES: return start_with_long_names_;
        case Pid::START_WITH_MEASURE_ONE: return start_with_measure_one_;
        case Pid::FIRST_SYSTEM_INDENTATION: return first_system_indentation_;
        case Pid::SHOW_COURTESY: return show_courtesy_;
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool LayoutBreak::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::LAYOUT_BREAK: set_layout_break_type(v.value<LayoutBreakType>()); break;
        case Pid::PAUSE: set_pause(v.to_double()); break;
        case Pid::START_WITH_LONG_NAMES: set_start_with_long_names(v.to_bool()); break;
        case Pid::START_WITH_MEASURE_ONE: set_start_with_measure_one(v.to_bool()); break;
        case Pid::FIRST_SYSTEM_INDENTATION: set_first_system_indentation(v.to_bool()); break;
        case Pid::SHOW_COURTESY: set_show_courtesy(v.to_bool()); break;
        default:
            if (!EngravingItem::set_property(property_id, v)) {
                return false;
            }
            break;
    }

    if (property_id == Pid::START_WITH_MEASURE_ONE) {
        trigger_layout_to_end();
    } else {
        trigger_layout();
        if (ownership_parent() && measure()->next()) {
            measure()->next()->trigger_layout();
        }
    }

    set_generated(false);
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue LayoutBreak::property_default(Pid id) const {
    switch (id) {
        case Pid::LAYOUT_BREAK: return PropertyValue(); // LAYOUT_BREAK_LINE;
        case Pid::PAUSE: return style().style_d(Sid::SECTION_PAUSE);
        case Pid::START_WITH_LONG_NAMES: return true;
        case Pid::START_WITH_MEASURE_ONE: return true;
        case Pid::FIRST_SYSTEM_INDENTATION: return true;
        case Pid::SHOW_COURTESY: return false;
        default: return EngravingItem::property_default(id);
    }
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString LayoutBreak::subtype_user_name() const { return TConv::user_name(layout_break_type()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

Font LayoutBreak::font() const {
    Font font(configuration()->icons_font_family(), Font::Type::ICON);
    font.set_point_size_f(::mu::engraving::ui_icons_default_font_size * mag_s());
    return font;
}
} // namespace mu::engraving
