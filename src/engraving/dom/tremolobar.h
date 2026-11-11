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

struct TremoloBar final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::TREMOLOBAR)

public:

    TremoloBar *clone() const override { return new TremoloBar(*this); }

    PitchValues &points() { return points_; }

    const PitchValues &points() const { return points_; }

    void set_points(const PitchValues &p) { points_ = p; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    double user_mag() const { return user_mag_; }

    void set_user_mag(double m) { user_mag_ = m; }

    void set_line_width(Spatium v) { lw_ = v; }

    Spatium line_width() const { return lw_; }

    bool play() const { return play_; }

    void set_play(bool val) { play_ = val; }

    struct LayoutData : public EngravingItem::LayoutData {
        PolygonF polygon;
    };
    DECLARE_LAYOUTDATA_METHODS(TremoloBar)

private:

    friend struct Factory;
    TremoloBar(DummyParentOr<EngravingItem> parent);

    TremoloBarType parse_tremolo_bar_type_from_curve(const PitchValues &curve) const;
    void update_points_by_tremolo_bar_type(const TremoloBarType type);

    Spatium lw_;
    double user_mag_ = 1.0; // allowed 0.1 - 10.0
    bool play_ = true;

    PitchValues points_;
};
} // namespace mu::engraving
#endif
