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

struct System;
struct SysStaff;

//---------------------------------------------------------
//   InstrumentName
//---------------------------------------------------------

struct InstrumentName final : TextBase {
private:
    OBJECT_ALLOCATOR(engraving, InstrumentName)
    DECLARE_CLASSOF(ElementType::INSTRUMENT_NAME)

public:
    InstrumentName(DummyParentOr<System>);

    InstrumentName *clone() const override { return new InstrumentName(*this); }

    InstrumentNameType instrument_name_type() const { return instrument_name_type_; }

    void set_instrument_name_type(InstrumentNameType v);

    InstrumentNameRole instrument_name_role() const { return instrument_name_role_; }

    void set_instrument_name_role(InstrumentNameRole v) { instrument_name_role_ = v; }

    System *system() const { return to_system(ownership_parent()); }

    SysStaff *sys_staff() const { return sys_staff_; }

    void set_sys_staff(SysStaff *s) { sys_staff_ = s; }

    double largest_staff_spatium() const;

    bool set_property(Pid property_id, const PropertyValue &) override;

    bool position_relative_to_notehead_rest() const override { return false; }

    staff_idx_t effective_staff_idx() const override;

    struct LayoutData : public TextBase::LayoutData {
    public:
        int column() const { return column_; }

        void set_column(int v) { column_ = v; }

        staff_idx_t end_idx_of_group() const { return end_idx_of_group_; }

        void set_end_idx_of_group(staff_idx_t v) { end_idx_of_group_ = v; }

    private:
        int column_ = 0;
        staff_idx_t end_idx_of_group_ = muse::nidx; // one-after last spanned staff (for GROUP types)
    };
    DECLARE_LAYOUTDATA_METHODS(InstrumentName)

private:

    InstrumentNameType instrument_name_type_ = InstrumentNameType::LONG;
    InstrumentNameRole instrument_name_role_ = InstrumentNameRole::PART;
    SysStaff *sys_staff_ = nullptr;
};
} // namespace mu::engraving
#endif
