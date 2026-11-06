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

#include "stafftypechange.h"

#include "iengravingconfiguration.h" // IWYU pragma: keep

#include "score.h"
#include "measure.h"
#include "system.h"
#include "staff.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   StaffTypeChange
//---------------------------------------------------------

StaffTypeChange::StaffTypeChange(DummyParentOr<MeasureBase> parent) : EngravingItem(ElementType::STAFFTYPE_CHANGE, parent) {
    lw_ = spatium() * 0.3;
}

StaffTypeChange::StaffTypeChange(const StaffTypeChange &lb) : EngravingItem(lb) {
    lw_ = lb.lw_;
    owns_staff_type_ = lb.owns_staff_type_;
    if (lb.owns_staff_type_ && lb.staff_type_) {
        staff_type_ = new StaffType(*lb.staff_type_);
    } else {
        staff_type_ = lb.staff_type_;
    }
}

StaffTypeChange::~StaffTypeChange() {
    if (staff_type_ && owns_staff_type_) {
        delete staff_type_;
    }
}

void StaffTypeChange::set_staff_type(StaffType *st, bool owned) {
    if (staff_type_ && owns_staff_type_) {
        delete staff_type_;
    }

    staff_type_ = st;
    owns_staff_type_ = owned && (st != nullptr);
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void StaffTypeChange::spatium_changed(double, double) { lw_ = spatium() * 0.3; }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue StaffTypeChange::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::STEP_OFFSET: return staff_type_->step_offset();
        case Pid::STAFF_LINES: return staff_type_->lines();
        case Pid::LINE_DISTANCE: return staff_type_->line_distance();
        case Pid::STAFF_SHOW_BARLINES: return staff_type_->show_barlines();
        case Pid::STAFF_SHOW_LEDGERLINES: return staff_type_->show_ledger_lines();
        case Pid::STAFF_STEMLESS: return staff_type_->stemless();
        case Pid::HEAD_SCHEME: return staff_type_->note_head_scheme();
        case Pid::STAFF_INSTRUMENT_LABEL_VISIBILITY: return staff_type_->instrument_label_visibility();
        case Pid::STAFF_GEN_CLEF: return staff_type_->gen_clef();
        case Pid::STAFF_GEN_TIMESIG: return staff_type_->gen_timesig();
        case Pid::STAFF_GEN_KEYSIG: return staff_type_->gen_keysig();
        case Pid::MAG: return staff_type_->user_mag();
        case Pid::SMALL: return staff_type_->is_small();
        case Pid::STAFF_INVISIBLE: return staff_type_->invisible();
        case Pid::STAFF_COLOR: return PropertyValue::from_value(staff_type_->color());
        case Pid::STAFF_YOFFSET: return staff_type_->yoffset();
        case Pid::STAFF_LONG_NAME: return staff_type_->long_name();
        case Pid::STAFF_SHORT_NAME: return staff_type_->short_name();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool StaffTypeChange::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::STEP_OFFSET: staff_type_->set_step_offset(v.to_int()); break;
        case Pid::STAFF_LINES: staff_type_->set_lines(v.to_int()); break;
        case Pid::LINE_DISTANCE: staff_type_->set_line_distance(v.value<Spatium>()); break;
        case Pid::STAFF_SHOW_BARLINES: staff_type_->set_show_barlines(v.to_bool()); break;
        case Pid::STAFF_SHOW_LEDGERLINES: staff_type_->set_show_ledger_lines(v.to_bool()); break;
        case Pid::STAFF_STEMLESS: staff_type_->set_stemless(v.to_bool()); break;
        case Pid::HEAD_SCHEME: staff_type_->set_note_head_scheme(v.value<NoteHeadScheme>()); break;
        case Pid::STAFF_INSTRUMENT_LABEL_VISIBILITY:
            staff_type_->set_instrument_label_visibility(v.value<InstrumentLabelVisibility>());
            break;
        case Pid::STAFF_GEN_CLEF: staff_type_->set_gen_clef(v.to_bool()); break;
        case Pid::STAFF_GEN_TIMESIG: staff_type_->set_gen_timesig(v.to_bool()); break;
        case Pid::STAFF_GEN_KEYSIG: staff_type_->set_gen_keysig(v.to_bool()); break;
        case Pid::MAG: {
            double spatium_value = spatium();
            staff_type_->set_user_mag(v.to_double());
            Staff *staff_value = staff();
            if (staff_value) {
                staff_value->set_local_spatium(spatium_value, spatium(), tick());
            }
        } break;
        case Pid::SMALL: {
            double spatium_value = spatium();
            staff_type_->set_small(v.to_bool());
            Staff *staff_value = staff();
            if (staff_value) {
                staff_value->set_local_spatium(spatium_value, spatium(), tick());
            }
        } break;
        case Pid::STAFF_INVISIBLE: staff_type_->set_invisible(v.to_bool()); break;
        case Pid::STAFF_COLOR: staff_type_->set_color(v.value<Color>()); break;
        case Pid::STAFF_YOFFSET: staff_type_->set_yoffset(v.value<Spatium>()); break;
        case Pid::STAFF_LONG_NAME: staff_type_->set_long_name(v.value<String>()); break;
        case Pid::STAFF_SHORT_NAME: staff_type_->set_short_name(v.value<String>()); break;
        default:
            if (!EngravingItem::set_property(property_id, v)) {
                return false;
            }
            break;
    }

    if (ownership_parent()) {
        staff()->staff_type_list_changed(measure()->tick());
    }
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue StaffTypeChange::property_default(Pid id) const {
    switch (id) {
        case Pid::STEP_OFFSET: return 0;
        case Pid::STAFF_LINES: return 5;
        case Pid::LINE_DISTANCE: return 1.0_sp;
        case Pid::STAFF_SHOW_BARLINES: return true;
        case Pid::STAFF_SHOW_LEDGERLINES: return true;
        case Pid::STAFF_STEMLESS: return false;
        case Pid::HEAD_SCHEME: return NoteHeadScheme::HEAD_NORMAL;
        case Pid::STAFF_INSTRUMENT_LABEL_VISIBILITY: return InstrumentLabelVisibility::AUTO;
        case Pid::STAFF_GEN_CLEF: return true;
        case Pid::STAFF_GEN_TIMESIG: return true;
        case Pid::STAFF_GEN_KEYSIG: return true;
        case Pid::MAG: return 1.0;
        case Pid::SMALL: return false;
        case Pid::STAFF_INVISIBLE: return false;
        case Pid::STAFF_COLOR: return PropertyValue::from_value(configuration()->default_color());
        case Pid::STAFF_YOFFSET: return 0.0_sp;
        case Pid::STAFF_LONG_NAME: return String();
        case Pid::STAFF_SHORT_NAME: return String();
        default: return EngravingItem::property_default(id);
    }
}
