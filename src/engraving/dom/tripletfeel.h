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

#include "systemtext.h"

namespace mu::engraving {
enum class TripletFeelType : unsigned char { NONE, TRIPLET_8TH, TRIPLET_16TH, DOTTED_8TH, DOTTED_16TH, SCOTTISH_8TH, SCOTTISH_16TH };

//---------------------------------------------------------
//   TripletFeel
//---------------------------------------------------------

struct TripletFeel final : SystemText {
private:
    DECLARE_CLASSOF(ElementType::TRIPLET_FEEL)

public:
    TripletFeel(DummyParentOr<Segment> parent, TripletFeelType triplet_fill_type = TripletFeelType::NONE);
    void set_triplet_property();

    TripletFeel *clone() const override { return new TripletFeel(*this); }

    TripletFeelType get_triplet_feel_type() const { return triplet_feel_type_; }

    PropertyValue property_default(Pid property_id) const override;
    Sid get_property_style(Pid id) const override;
    TranslatableString subtype_user_name() const override;


private:

    constexpr static int eight_division = 2;
    constexpr static int sixteenth_division = 4;
    constexpr static int triplet_ratio = 66;
    constexpr static int dotted_ratio = 75;
    constexpr static int scottish_ratio = 25;
    constexpr static int zero_ratio = 0;

    TripletFeelType triplet_feel_type_ = TripletFeelType::NONE;
};
} // namespace mu::engraving
