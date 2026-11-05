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

    EngravingItem *elementBase() const override;

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void reset() override;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    staff_idx_t vStaffIdx() const override;

    Chord *chord() const { return toChord(ownershipParent()); }

    bool up() const;

    Spatium baseLength() const { return baseLength_; }

    void setBaseLength(Spatium baseLength);

    Spatium userLength() const { return userLength_; }

    void setUserLength(Spatium userLength) { userLength_ = userLength; }

    Spatium lineWidth() const { return lineWidth_; }

    double lineWidthMag() const;

    void setLineWidth(Spatium lineWidth) { lineWidth_ = lineWidth; }

    PointF flagPosition() const;

    double length() const { return absoluteFromSpatium(baseLength_ + userLength_); }

    struct LayoutData : public EngravingItem::LayoutData {
        LineF line;
        double beamCorrection = 0.0;
    };
    DECLARE_LAYOUTDATA_METHODS(Stem)

private:
    friend class Factory;
    Stem(DummyParentOr<Chord> parent);

    Spatium baseLength_ = 0.0_sp;

    Spatium userLength_ = 0.0_sp;
    Spatium lineWidth_ = 0.0_sp;
};
} // namespace mu::engraving
