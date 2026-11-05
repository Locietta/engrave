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

#ifndef MU_ENGRAVING_TREMOLOBAR_H
#define MU_ENGRAVING_TREMOLOBAR_H

#include "engravingitem.h"

namespace mu::engraving {
//---------------------------------------------------------
//   @@ TremoloBar
//
//   @P userMag    double
//   @P lineWidth  double
//   @P play       bool         play tremolo bar
//---------------------------------------------------------

enum class TremoloBarType : unsigned char { DIP = 0, DIVE, RELEASE_UP, INVERTED_DIP, RETURN, RELEASE_DOWN, CUSTOM };

class TremoloBar final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, TremoloBar)
    DECLARE_CLASSOF(ElementType::TREMOLOBAR)

public:

    TremoloBar *clone() const override { return new TremoloBar(*this); }

    PitchValues &points() { return points_; }

    const PitchValues &points() const { return points_; }

    void setPoints(const PitchValues &p) { points_ = p; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    double userMag() const { return userMag_; }

    void setUserMag(double m) { userMag_ = m; }

    void setLineWidth(Spatium v) { lw_ = v; }

    Spatium lineWidth() const { return lw_; }

    bool play() const { return play_; }

    void setPlay(bool val) { play_ = val; }

    struct LayoutData : public EngravingItem::LayoutData {
        PolygonF polygon;
    };
    DECLARE_LAYOUTDATA_METHODS(TremoloBar)

private:

    friend class Factory;
    TremoloBar(DummyParentOr<EngravingItem> parent);

    TremoloBarType parseTremoloBarTypeFromCurve(const PitchValues &curve) const;
    void updatePointsByTremoloBarType(const TremoloBarType type);

    Spatium lw_;
    double userMag_ = 1.0; // allowed 0.1 - 10.0
    bool play_ = true;

    PitchValues points_;
};
} // namespace mu::engraving
#endif
