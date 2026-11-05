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

#ifndef MU_ENGRAVING_INSTRUMENTNAME_H
#define MU_ENGRAVING_INSTRUMENTNAME_H

#include "textbase.h"

namespace mu::engraving {
enum class InstrumentNameType : char { LONG, SHORT };
enum class InstrumentNameRole : char { STAFF, SHARED_STAFF, PART, GROUP };

class System;
class SysStaff;

//---------------------------------------------------------
//   InstrumentName
//---------------------------------------------------------

class InstrumentName final : public TextBase {
    OBJECT_ALLOCATOR(engraving, InstrumentName)
    DECLARE_CLASSOF(ElementType::INSTRUMENT_NAME)

public:
    InstrumentName(DummyParentOr<System>);

    InstrumentName *clone() const override { return new InstrumentName(*this); }

    InstrumentNameType instrumentNameType() const { return instrumentNameType_; }

    void setInstrumentNameType(InstrumentNameType v);

    InstrumentNameRole instrumentNameRole() const { return instrumentNameRole_; }

    void setInstrumentNameRole(InstrumentNameRole v) { instrumentNameRole_ = v; }

    System *system() const { return toSystem(ownershipParent()); }

    SysStaff *sysStaff() const { return sysStaff_; }

    void setSysStaff(SysStaff *s) { sysStaff_ = s; }

    double largestStaffSpatium() const;

    bool setProperty(Pid propertyId, const PropertyValue &) override;

    bool positionRelativeToNoteheadRest() const override { return false; }

    staff_idx_t effectiveStaffIdx() const override;

    struct LayoutData : public TextBase::LayoutData {
    public:
        int column() const { return column_; }

        void setColumn(int v) { column_ = v; }

        staff_idx_t endIdxOfGroup() const { return endIdxOfGroup_; }

        void setEndIdxOfGroup(staff_idx_t v) { endIdxOfGroup_ = v; }

    private:
        int column_ = 0;
        staff_idx_t endIdxOfGroup_ = muse::nidx; // one-after last spanned staff (for GROUP types)
    };
    DECLARE_LAYOUTDATA_METHODS(InstrumentName)

private:

    InstrumentNameType instrumentNameType_ = InstrumentNameType::LONG;
    InstrumentNameRole instrumentNameRole_ = InstrumentNameRole::PART;
    SysStaff *sysStaff_ = nullptr;
};
} // namespace mu::engraving
#endif
