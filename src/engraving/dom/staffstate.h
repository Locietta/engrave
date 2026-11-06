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

#ifndef MU_ENGRAVING_STAFFSTATE_H
#define MU_ENGRAVING_STAFFSTATE_H

#include "engravingitem.h"
#include "instrument.h"
#include "draw/types/painterpath.h"

namespace mu::engraving {
class Transaction;

enum class StaffStateType : char { INSTRUMENT, TYPE, VISIBLE, INVISIBLE };

//---------------------------------------------------------
//   @@ StaffState
//---------------------------------------------------------

class StaffState final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, StaffState)
    DECLARE_CLASSOF(ElementType::STAFF_STATE)

public:

    ~StaffState();

    StaffState *clone() const override { return new StaffState(*this); }

    void set_staff_state_type(const String &);

    void set_staff_state_type(StaffStateType st) { staff_state_type_ = st; }

    StaffStateType staff_state_type() const { return staff_state_type_; }

    String staff_state_type_name() const;

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    Instrument *instrument() const { return instrument_; }

    void set_instrument(const Instrument *i) { *instrument_ = *i; }

    void set_instrument(const Instrument &&i) { *instrument_ = i; }

    Segment *segment() { return (Segment *) ownership_parent(); }

    struct LayoutData : public EngravingItem::LayoutData {
        double lw = 0.0;
        muse::draw::PainterPath path;
    };
    DECLARE_LAYOUTDATA_METHODS(StaffState)

private:

    friend class Factory;
    StaffState(DummyParentOr<EngravingItem> parent);
    StaffState(const StaffState &);

    StaffStateType staff_state_type_ = StaffStateType::INVISIBLE;

    Instrument *instrument_ = nullptr;
};
} // namespace mu::engraving
#endif
