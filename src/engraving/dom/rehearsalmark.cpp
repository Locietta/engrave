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

#include "rehearsalmark.h"

#include "measure.h"
#include "score.h"
#include "system.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   rehearsalMarkStyle
//---------------------------------------------------------

static const ElementStyle rehearsal_mark_style{
    {Sid::REHEARSAL_MARK_PLACEMENT, Pid::PLACEMENT},
    {Sid::REHEARSAL_MARK_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

static const ElementStyle main_rehearsal_mark_style{
    {Sid::REHEARSAL_MARK_FRAME_TYPE, Pid::FRAME_TYPE},
    {Sid::REHEARSAL_MARK_FONT_SIZE, Pid::FONT_SIZE},
    {Sid::REHEARSAL_MARK_ALIGN, Pid::ALIGN},
};

static const ElementStyle additional_rehearsal_mark_style{
    {Sid::TEMPO_FRAME_TYPE, Pid::FRAME_TYPE},
    {Sid::TEMPO_FONT_SIZE, Pid::FONT_SIZE},
    {Sid::TEMPO_ALIGN, Pid::ALIGN},
};

//---------------------------------------------------------
//   RehearsalMark
//---------------------------------------------------------

RehearsalMark::RehearsalMark(DummyParentOr<Segment> parent)
    : TextBase(ElementType::REHEARSAL_MARK, parent, TextStyleType::REHEARSAL_MARK, ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::rehearsal_mark_style);
    set_system_flag(true);
}

//---------------------------------------------------------
//   setType
//---------------------------------------------------------

void RehearsalMark::set_type(RehearsalMark::Type type) {
    if (type == type_) {
        return;
    }
    type_ = type;
    apply_type_style();
}

void RehearsalMark::apply_type_style() {
    const auto &elem_style_map =
        (type_ == Type::MAIN ? ::mu::engraving::main_rehearsal_mark_style : ::mu::engraving::additional_rehearsal_mark_style);
    for (const auto &elem : elem_style_map) {
        if (property_flags(elem.pid) == PropertyFlags::STYLED) {
            set_property(elem.pid, style().style_v(elem.sid));
        }
    }
}

void RehearsalMark::style_changed() {
    TextBase::style_changed();
    apply_type_style();
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

engraving::PropertyValue RehearsalMark::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::REHEARSAL_MARK;
        case Pid::PLACEMENT: return style().style_v(Sid::REHEARSAL_MARK_PLACEMENT);
        default: return TextBase::property_default(id);
    }
}
} // namespace mu::engraving
