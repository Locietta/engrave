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
#pragma once

#include "textbase.h"

namespace mu::engraving {
struct Dynamic;
struct Transaction;

struct Expression final : TextBase {
private:
    M_PROPERTY(bool, snap_to_dynamics, set_snap_to_dynamics)
    DECLARE_CLASSOF(ElementType::EXPRESSION)

public:
    Expression(DummyParentOr<Segment> parent);
    Expression(const Expression &expression);

    Expression *clone() const override { return new Expression(*this); }

    Segment *segment() const { return to_segment(ownership_parent()); }

    PropertyValue property_default(Pid id) const override;

    double compute_dynamic_expression_distance(const Dynamic *snapped_dyn) const;

    bool accept_drop(EditData &ed) const override;
    EngravingItem *drop(Transaction &tx, EditData &ed) override;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    void map_properties_from_old_expressions(StaffText *staff_text);

    Dynamic *snapped_dynamic() const;

    bool has_voice_assignment_properties() const override { return true; }

    void reset() override;

    bool position_relative_to_notehead_rest() const override { return true; }
};
} // namespace mu::engraving
