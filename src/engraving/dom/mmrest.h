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

#include "rest.h"
#include "utils.h"

namespace mu::engraving {
struct MMRest final : Rest {
private:
    OBJECT_ALLOCATOR(engraving, MMRest)
    DECLARE_CLASSOF(ElementType::MMREST)

public:
    MMRest(DummyParentOr<Segment> s);
    MMRest(const MMRest &, bool link = false);

    MMRest *clone() const override { return new MMRest(*this, false); }

    EngravingItem *linked_clone() override { return new MMRest(*this, true); }

    bool should_show_number_by_default() const;
    bool show_number() const;

    PropertyValue property_default(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;
    PropertyValue get_property(Pid) const override;

    RectF number_rect() const override;
    PointF number_pos() const;

    void set_number_offset(const Spatium y) { number_offset_ = y; }

    Spatium number_offset() const { return number_offset_; }

    double y_number_pos() const;

    bool is_old_style() const;

    struct LayoutData : public Rest::LayoutData {
        int number = 0; // number of measures represented
        double y_number_pos = 0.0;
        SymIdList number_sym;
        SymIdList rest_syms;     // stores symbols when using old-style rests
        double syms_width = 0.0; // width of symbols with spacing when using old-style

        ld_field<double> rest_width = {"[MMRest] restWidth", 0.0}; // width of multimeasure rest

        void set_number_sym(int n) { number_sym = time_sig_sym_ids_from_string(String::number(n)); }
    };
    DECLARE_LAYOUTDATA_METHODS(MMRest)

private:
    Spatium number_offset_ = 0.0_sp; // vertical offset of number relative to its style position
    bool number_visible_ = false;    // show or hide number
};
} // namespace mu::engraving
