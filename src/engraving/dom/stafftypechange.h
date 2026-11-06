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

#ifndef MU_ENGRAVING_STAFFTYPECHANGE_H
#define MU_ENGRAVING_STAFFTYPECHANGE_H

#include "engravingitem.h"

namespace mu::engraving {
class StaffType;

//---------------------------------------------------------
//   @@ StaffTypeChange
//---------------------------------------------------------

class StaffTypeChange final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, StaffTypeChange)
    DECLARE_CLASSOF(ElementType::STAFFTYPE_CHANGE)

public:
    ~StaffTypeChange() override;

    StaffTypeChange *clone() const override { return new StaffTypeChange(*this); }

    const StaffType *staff_type() const { return staff_type_; }

    void set_staff_type(StaffType *st, bool owned);

    double lw() const { return lw_; }

    Measure *measure() const { return to_measure(ownership_parent()); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

private:

    friend class Factory;
    StaffTypeChange(DummyParentOr<MeasureBase> parent);
    StaffTypeChange(const StaffTypeChange &);

    void spatium_changed(double old_value, double new_value) override;

    StaffType *staff_type_ = nullptr;
    bool owns_staff_type_ = false;
    double lw_ = 0.0;
};
} // namespace mu::engraving

#endif
