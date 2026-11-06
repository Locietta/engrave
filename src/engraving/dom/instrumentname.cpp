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

#include "instrumentname.h"

#include "measure.h"
#include "part.h"
#include "score.h"
#include "staff.h"
#include "style/style.h"
#include "system.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   longInstrumentStyle
//---------------------------------------------------------

static const ElementStyle long_instrument_style{};

//---------------------------------------------------------
//   shortInstrumentStyle
//---------------------------------------------------------

static const ElementStyle short_instrument_style{};

//---------------------------------------------------------
//   InstrumentName
//---------------------------------------------------------

InstrumentName::InstrumentName(DummyParentOr<System> s)
    : TextBase(ElementType::INSTRUMENT_NAME, s, TextStyleType::INSTRUMENT_LONG, ElementFlag::NOTHING) {
    set_flag(ElementFlag::MOVABLE, false);
    set_instrument_name_type(InstrumentNameType::LONG);
}

double InstrumentName::largest_staff_spatium() const {
    if (system_flag() || (layout_parent() && layout_parent()->system_flag())) {
        return style().spatium();
    }

    // Get spatium for instrument names from largest staff of part,
    // instead of staff it is attached to
    Part *p = part();
    if (!part()) {
        return style().spatium();
    }
    double largest_spatium = 0;
    for (Staff *s : p->staves()) {
        double sp = s->spatium(tick());
        if (sp > largest_spatium) {
            largest_spatium = sp;
        }
    }
    return largest_spatium;
}

//---------------------------------------------------------
//   setInstrumentNameType
//---------------------------------------------------------

void InstrumentName::set_instrument_name_type(InstrumentNameType st) {
    instrument_name_type_ = st;
    if (st == InstrumentNameType::SHORT) {
        set_text_style_type(TextStyleType::INSTRUMENT_SHORT);
        init_element_style(&::mu::engraving::short_instrument_style);
    } else {
        set_text_style_type(TextStyleType::INSTRUMENT_LONG);
        init_element_style(&::mu::engraving::long_instrument_style);
    }
}

bool InstrumentName::set_property(Pid id, const PropertyValue &v) {
    bool rv = true;
    switch (id) {
        case Pid::VISIBLE:
            // not supported
            break;
        default: rv = TextBase::set_property(id, v); break;
    }
    return rv;
}

mu::engraving::staff_idx_t mu::engraving::InstrumentName::effective_staff_idx() const {
    if (sys_staff_->show() || instrument_name_role_ == InstrumentNameRole::STAFF) {
        return staff_idx();
    } else if (instrument_name_role_ == InstrumentNameRole::PART) {
        return system()->first_visible_sys_staff_of_part(score()->staff(staff_idx())->part());
    } else {
        staff_idx_t cur_idx = staff_idx();
        Instrument *instr = score()->staff(cur_idx)->part()->instrument(system()->first()->tick());
        return system()->first_visible_sys_staff_with_instrument(instr->id(), cur_idx);
    }
}
} // namespace mu::engraving
