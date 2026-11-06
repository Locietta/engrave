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

#include "text.h"

#include "log.h"
#include "rw/rwregister.h"
#include "score.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   defaultStyle
//---------------------------------------------------------

static const ElementStyle default_style{
    {Sid::DEFAULT_SYSTEM_FLAG, Pid::SYSTEM_FLAG},
};

static bool style_is_selectable(TextStyleType style) {
    switch (style) {
        case TextStyleType::HEADER:
        case TextStyleType::FOOTER:
        case TextStyleType::COPYRIGHT:
        case TextStyleType::PAGE_NUMBER:
        case TextStyleType::GROUP_BRACKET: return false;
        default: break;
    }
    return true;
}

//---------------------------------------------------------
//   Text
//---------------------------------------------------------

Text::Text(DummyParentOr<EngravingItem> parent, TextStyleType tid)
    : TextBase(ElementType::TEXT, parent, tid, style_is_selectable(tid) ? ElementFlag::NOTHING : ElementFlag::NOT_SELECTABLE) {
    init_element_style(&::mu::engraving::default_style);
}

EngravingObject *Text::property_delegate(Pid id) const {
    if (id == Pid::TEXT_STYLE && parent() && parent()->is_text_line_base_segment()) {
        return parent();
    }

    return nullptr;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

engraving::PropertyValue Text::property_default(Pid id) const {
    if (EngravingObject *item = property_delegate(id)) {
        return item->property_default(id);
    }

    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::DEFAULT;
        default: return TextBase::property_default(id);
    }
}

PropertyValue Text::get_property(Pid id) const {
    switch (id) {
        case Pid::VOICE_ASSIGNMENT:
            if (has_voice_assignment_properties()) {
                return ownership_parent()->get_property(id);
            }
        // fallthrough
        default: return TextBase::get_property(id);
    }
}

String Text::read_xml_text(XmlReader &xml, Score *score) {
    Text t(score->dummy());
    rw::RWRegister::reader()->read_item(&t, xml);
    return t.xml_text();
}

bool Text::has_voice_assignment_properties() const {
    const EngravingItem *parent = ownership_parent_item();
    if (parent && parent->is_text_line_base_segment()) {
        return parent->has_voice_assignment_properties();
    }
    return false;
}

bool mu::engraving::Text::collect_for_drawing() const { return !(parent() && parent()->is_tuplet()); }

bool Text::position_relative_to_notehead_rest() const {
    if (parent()->is_box() || parent()->is_tuplet() || parent()->is_spanner_segment() || parent()->is_bracket()) {
        return false;
    }

    return true;
}
} // namespace mu::engraving
