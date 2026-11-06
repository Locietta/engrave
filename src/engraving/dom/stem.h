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

#include "engravingitem.h"

namespace mu::engraving {
class Chord;
class Transaction;

class Stem final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Stem)
    DECLARE_CLASSOF(ElementType::STEM)

public:

    Stem(const Stem &) = default;
    Stem &operator=(const Stem &) = delete;

    Stem *clone() const override { return new Stem(*this); }

    EngravingItem *element_base() const override;

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void reset() override;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    staff_idx_t v_staff_idx() const override;

    Chord *chord() const { return to_chord(ownership_parent()); }

    bool up() const;

    Spatium base_length() const { return base_length_; }

    void set_base_length(Spatium base_length);

    Spatium user_length() const { return user_length_; }

    void set_user_length(Spatium user_length) { user_length_ = user_length; }

    Spatium line_width() const { return line_width_; }

    double line_width_mag() const;

    void set_line_width(Spatium line_width) { line_width_ = line_width; }

    PointF flag_position() const;

    double length() const { return absolute_from_spatium(base_length_ + user_length_); }

    struct LayoutData : public EngravingItem::LayoutData {
        LineF line;
        double beam_correction = 0.0;
    };
    DECLARE_LAYOUTDATA_METHODS(Stem)

private:
    friend class Factory;
    Stem(DummyParentOr<Chord> parent);

    Spatium base_length_ = 0.0_sp;

    Spatium user_length_ = 0.0_sp;
    Spatium line_width_ = 0.0_sp;
};
} // namespace mu::engraving
