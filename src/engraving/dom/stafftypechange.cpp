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
    ownsStaffType_ = lb.ownsStaffType_;
    if (lb.ownsStaffType_ && lb.staffType_) {
        staffType_ = new StaffType(*lb.staffType_);
    } else {
        staffType_ = lb.staffType_;
    }
}

StaffTypeChange::~StaffTypeChange() {
    if (staffType_ && ownsStaffType_) {
        delete staffType_;
    }
}

void StaffTypeChange::setStaffType(StaffType *st, bool owned) {
    if (staffType_ && ownsStaffType_) {
        delete staffType_;
    }

    staffType_ = st;
    ownsStaffType_ = owned && (st != nullptr);
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void StaffTypeChange::spatiumChanged(double, double) { lw_ = spatium() * 0.3; }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue StaffTypeChange::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::STEP_OFFSET: return staffType_->stepOffset();
        case Pid::STAFF_LINES: return staffType_->lines();
        case Pid::LINE_DISTANCE: return staffType_->lineDistance();
        case Pid::STAFF_SHOW_BARLINES: return staffType_->showBarlines();
        case Pid::STAFF_SHOW_LEDGERLINES: return staffType_->showLedgerLines();
        case Pid::STAFF_STEMLESS: return staffType_->stemless();
        case Pid::HEAD_SCHEME: return staffType_->noteHeadScheme();
        case Pid::STAFF_INSTRUMENT_LABEL_VISIBILITY: return staffType_->instrumentLabelVisibility();
        case Pid::STAFF_GEN_CLEF: return staffType_->genClef();
        case Pid::STAFF_GEN_TIMESIG: return staffType_->genTimesig();
        case Pid::STAFF_GEN_KEYSIG: return staffType_->genKeysig();
        case Pid::MAG: return staffType_->userMag();
        case Pid::SMALL: return staffType_->isSmall();
        case Pid::STAFF_INVISIBLE: return staffType_->invisible();
        case Pid::STAFF_COLOR: return PropertyValue::fromValue(staffType_->color());
        case Pid::STAFF_YOFFSET: return staffType_->yoffset();
        case Pid::STAFF_LONG_NAME: return staffType_->longName();
        case Pid::STAFF_SHORT_NAME: return staffType_->shortName();
        default: return EngravingItem::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool StaffTypeChange::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::STEP_OFFSET: staffType_->setStepOffset(v.toInt()); break;
        case Pid::STAFF_LINES: staffType_->setLines(v.toInt()); break;
        case Pid::LINE_DISTANCE: staffType_->setLineDistance(v.value<Spatium>()); break;
        case Pid::STAFF_SHOW_BARLINES: staffType_->setShowBarlines(v.toBool()); break;
        case Pid::STAFF_SHOW_LEDGERLINES: staffType_->setShowLedgerLines(v.toBool()); break;
        case Pid::STAFF_STEMLESS: staffType_->setStemless(v.toBool()); break;
        case Pid::HEAD_SCHEME: staffType_->setNoteHeadScheme(v.value<NoteHeadScheme>()); break;
        case Pid::STAFF_INSTRUMENT_LABEL_VISIBILITY: staffType_->setInstrumentLabelVisibility(v.value<InstrumentLabelVisibility>()); break;
        case Pid::STAFF_GEN_CLEF: staffType_->setGenClef(v.toBool()); break;
        case Pid::STAFF_GEN_TIMESIG: staffType_->setGenTimesig(v.toBool()); break;
        case Pid::STAFF_GEN_KEYSIG: staffType_->setGenKeysig(v.toBool()); break;
        case Pid::MAG: {
            double _spatium = spatium();
            staffType_->setUserMag(v.toDouble());
            Staff *_staff = staff();
            if (_staff) {
                _staff->setLocalSpatium(_spatium, spatium(), tick());
            }
        } break;
        case Pid::SMALL: {
            double _spatium = spatium();
            staffType_->setSmall(v.toBool());
            Staff *_staff = staff();
            if (_staff) {
                _staff->setLocalSpatium(_spatium, spatium(), tick());
            }
        } break;
        case Pid::STAFF_INVISIBLE: staffType_->setInvisible(v.toBool()); break;
        case Pid::STAFF_COLOR: staffType_->setColor(v.value<Color>()); break;
        case Pid::STAFF_YOFFSET: staffType_->setYoffset(v.value<Spatium>()); break;
        case Pid::STAFF_LONG_NAME: staffType_->setLongName(v.value<String>()); break;
        case Pid::STAFF_SHORT_NAME: staffType_->setShortName(v.value<String>()); break;
        default:
            if (!EngravingItem::setProperty(propertyId, v)) {
                return false;
            }
            break;
    }

    if (ownershipParent()) {
        staff()->staffTypeListChanged(measure()->tick());
    }
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue StaffTypeChange::propertyDefault(Pid id) const {
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
        case Pid::STAFF_COLOR: return PropertyValue::fromValue(configuration()->defaultColor());
        case Pid::STAFF_YOFFSET: return 0.0_sp;
        case Pid::STAFF_LONG_NAME: return String();
        case Pid::STAFF_SHORT_NAME: return String();
        default: return EngravingItem::propertyDefault(id);
    }
}
