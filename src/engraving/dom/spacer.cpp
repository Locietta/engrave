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

#include "spacer.h"

#include "../editing/editdata.h"
#include "../editing/elementeditdata.h"

#include "measure.h"
#include "score.h"
#include "system.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   LayoutBreak
//---------------------------------------------------------

Spacer::Spacer(DummyParentOr<Measure> parent) : EngravingItem(ElementType::SPACER, parent) {
    spacer_type_ = SpacerType::UP;
    gap_ = 0.0_sp;
    z_ = -10; // Ensure behind notation
}

Spacer::Spacer(const Spacer &s) : EngravingItem(s) {
    gap_ = s.gap_;
    spacer_type_ = s.spacer_type_;
}

//---------------------------------------------------------
//   setGap
//---------------------------------------------------------

void Spacer::set_gap(Spatium sp) { gap_ = sp; }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Spacer::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SPACE: return gap();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Spacer::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SPACE: set_gap(v.value<Spatium>()); break;
        default:
            if (!EngravingItem::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    set_generated(false);
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Spacer::property_default(Pid id) const {
    switch (id) {
        case Pid::SPACE: return 0.0_sp;
        default: return EngravingItem::property_default(id);
    }
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void Spacer::trigger_layout() const {
    if (!ownership_parent()) {
        return;
    }

    Measure *m = measure();
    if (!m) {
        EngravingItem::trigger_layout();
        return;
    }

    Score *s = score();
    System *system = m->system();
    if (!s || !system || s->nstaves() == 0) {
        m->trigger_layout();
        return;
    }

    // Most spacer edits can stay local to the measure and are much cheaper.
    // Escalate only for down/fixed spacers on the last non-vbox system of the page,
    // where edits can repaginate and affect page-end spacing.
    if (spacer_type() == SpacerType::UP) {
        m->trigger_layout();
        return;
    }

    System *next_system = m->next_non_v_box_system();
    if (next_system && next_system->page() == system->page()) {
        m->trigger_layout();
        return;
    }

    // For the last notation system on a page, include the next system boundary
    // so page-end spacing can be recomputed without relaying out the whole score.
    Measure *first_measure = system->first_measure();
    if (!first_measure) {
        m->trigger_layout();
        return;
    }

    Fraction start_tick = first_measure->tick();
    Fraction end_tick = s->end_tick();
    if (next_system) {
        if (Measure *next_first_measure = next_system->first_measure()) {
            end_tick = next_first_measure->tick();
        }
    }

    s->set_layout(start_tick, end_tick, 0, s->nstaves() - 1, this);
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Spacer::subtype_user_name() const {
    switch (spacer_type_) {
        case SpacerType::UP: return TranslatableString("engraving/spacertype", "Staff spacer up");
        case SpacerType::DOWN: return TranslatableString("engraving/spacertype", "Staff spacer down");
        case SpacerType::FIXED: return TranslatableString("engraving/spacertype", "Staff spacer fixed down");
    }
    return TranslatableString::untranslatable("Unknown spacer");
}
} // namespace mu::engraving
