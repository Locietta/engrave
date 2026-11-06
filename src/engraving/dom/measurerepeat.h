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

#ifndef MU_ENGRAVING_MEASUREREPEAT_H
#define MU_ENGRAVING_MEASUREREPEAT_H

#include "rest.h"
#include "utils.h"

namespace mu::engraving {
struct Segment;

struct MeasureRepeat final : Rest {
private:
    OBJECT_ALLOCATOR(engraving, MeasureRepeat)
    DECLARE_CLASSOF(ElementType::MEASURE_REPEAT)

public:
    MeasureRepeat(DummyParentOr<Segment> parent);
    MeasureRepeat(const MeasureRepeat &) = default;
    MeasureRepeat &operator=(const MeasureRepeat &) = delete;

    MeasureRepeat *clone() const override { return new MeasureRepeat(*this); }

    EngravingItem *linked_clone() override { return EngravingItem::linked_clone(); }

    static constexpr int max_num_measures = 4;

    void set_num_measures(int n);

    int num_measures() const { return num_measures_; }

    void set_number_offset(const Spatium y) { number_offset_ = y; }

    Spatium number_offset() const { return number_offset_; }

    Measure *first_measure_of_group() const;
    const Measure *referring_measure(const Measure *measure) const;

    Fraction ticks() const override;

    Fraction actual_ticks() const { return Rest::ticks(); }

    PropertyValue property_default(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;
    PropertyValue get_property(Pid) const override;

    int subtype() const override { return num_measures_; }

    muse::TranslatableString subtype_user_name() const override;

    bool place_multiple() const override { return num_measures() == 1; } // prevent overlapping additions with range selection

    RectF number_rect() const override;

    PointF number_position(const RectF &number_bbox) const;

    struct LayoutData : public Rest::LayoutData {
        SymId sym_id = SymId::NO_SYM;
        SymIdList number_sym;

        void set_sym_id(SymId id) { sym_id = id; }

        void set_number_sym(int n) { number_sym = time_sig_sym_ids_from_string(String::number(n)); }

        void set_number_sym(const String &s) { number_sym = time_sig_sym_ids_from_string(s); }

        void clear_number_sym() { number_sym.clear(); }

        LineF extender_line_left = LineF();
        LineF extender_line_right = LineF();
    };
    DECLARE_LAYOUTDATA_METHODS(MeasureRepeat)

private:
    int num_measures_ = 0;
    Spatium number_offset_ = 0.0_sp; // vertical offset of number relative to its style position
};
} // namespace mu::engraving
#endif
