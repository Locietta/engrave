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

#include "tremolobar.h"

#include "draw/types/pen.h"
#include "log.h"
#include "score.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   tremoloBarStyle
//---------------------------------------------------------

static const ElementStyle tremolo_bar_style{
    {Sid::TREMOLO_BAR_LINE_WIDTH, Pid::LINE_WIDTH},
};

static const PitchValues dip_curve = {PitchValue(0, 0), PitchValue(30, -100), PitchValue(60, 0)};

static const PitchValues dive_curve = {PitchValue(0, 0), PitchValue(60, -150)};

static const PitchValues release_up_curve = {PitchValue(0, -150), PitchValue(60, 0)};

static const PitchValues inverted_dip_curve = {PitchValue(0, 0), PitchValue(30, 100), PitchValue(60, 0)};

static const PitchValues return_curve = {PitchValue(0, 0), PitchValue(60, 150)};

static const PitchValues release_down_curve = {PitchValue(0, 150), PitchValue(60, 0)};

//---------------------------------------------------------
//   TremoloBar
//---------------------------------------------------------

TremoloBar::TremoloBar(DummyParentOr<EngravingItem> parent)
    : EngravingItem(ElementType::TREMOLOBAR, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::tremolo_bar_style);
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TremoloBar::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::LINE_WIDTH: return line_width();
        case Pid::MAG: return user_mag();
        case Pid::PLAY: return play();
        case Pid::TREMOLOBAR_TYPE: return static_cast<int>(parse_tremolo_bar_type_from_curve(points_));
        case Pid::TREMOLOBAR_CURVE: return points_;
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TremoloBar::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::LINE_WIDTH: set_line_width(v.value<Spatium>()); break;
        case Pid::MAG: set_user_mag(v.to_double()); break;
        case Pid::PLAY: set_play(v.to_bool()); break;
        case Pid::TREMOLOBAR_TYPE: update_points_by_tremolo_bar_type(static_cast<TremoloBarType>(v.to_int())); break;
        case Pid::TREMOLOBAR_CURVE: set_points(v.value<PitchValues>()); break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TremoloBar::property_default(Pid pid) const {
    switch (pid) {
        case Pid::MAG: return 1.0;
        case Pid::PLAY: return true;
        case Pid::TREMOLOBAR_TYPE: return static_cast<int>(TremoloBarType::DIP);
        case Pid::TREMOLOBAR_CURVE: return ::mu::engraving::dip_curve;
        default:
            for (const StyledProperty &p : *styled_properties()) {
                if (p.pid == pid) {
                    if (property_type(pid) == PType::ABSOLUTE) {
                        return style().style_absolute(p.sid);
                    }
                    return style().style_v(p.sid);
                }
            }
            return EngravingItem::property_default(pid);
    }
}

TremoloBarType TremoloBar::parse_tremolo_bar_type_from_curve(const PitchValues &curve) const {
    if (curve == ::mu::engraving::dip_curve) {
        return TremoloBarType::DIP;
    } else if (curve == ::mu::engraving::dive_curve) {
        return TremoloBarType::DIVE;
    } else if (curve == ::mu::engraving::release_up_curve) {
        return TremoloBarType::RELEASE_UP;
    } else if (curve == ::mu::engraving::inverted_dip_curve) {
        return TremoloBarType::INVERTED_DIP;
    } else if (curve == ::mu::engraving::return_curve) {
        return TremoloBarType::RETURN;
    } else if (curve == ::mu::engraving::release_down_curve) {
        return TremoloBarType::RELEASE_DOWN;
    }

    return TremoloBarType::CUSTOM;
}

void TremoloBar::update_points_by_tremolo_bar_type(const TremoloBarType type) {
    switch (type) {
        case TremoloBarType::DIP: points_ = ::mu::engraving::dip_curve; break;
        case TremoloBarType::DIVE: points_ = ::mu::engraving::dive_curve; break;
        case TremoloBarType::RELEASE_UP: points_ = ::mu::engraving::release_up_curve; break;
        case TremoloBarType::INVERTED_DIP: points_ = ::mu::engraving::inverted_dip_curve; break;
        case TremoloBarType::RETURN: points_ = ::mu::engraving::return_curve; break;
        case TremoloBarType::RELEASE_DOWN: points_ = ::mu::engraving::release_down_curve; break;
        case TremoloBarType::CUSTOM: break;
    }
}
} // namespace mu::engraving
