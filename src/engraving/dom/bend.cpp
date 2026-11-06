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

#include "bend.h"

#include "log.h"
#include "note.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

//---------------------------------------------------------
//   label
//---------------------------------------------------------

const char *Bend::label[13] = {"", "1/4", "1/2", "3/4", "full", "1 1/4", "1 1/2", "1 3/4", "2", "2 1/4", "2 1/2", "2 3/4", "3"};

static const ElementStyle bend_style{
    {Sid::BEND_FONT_FACE, Pid::FONT_FACE},
    {Sid::BEND_FONT_SIZE, Pid::FONT_SIZE},
    {Sid::BEND_FONT_STYLE, Pid::FONT_STYLE},
    {Sid::BEND_LINE_WIDTH, Pid::LINE_WIDTH},
};

static const PitchValues bend_curve = {PitchValue(0, 0), PitchValue(15, 100), PitchValue(60, 100)};

static const PitchValues bend_release_curve = {PitchValue(0, 0), PitchValue(10, 100), PitchValue(20, 100), PitchValue(30, 0),
                                               PitchValue(60, 0)};

static const PitchValues bend_release_bend_curve = {PitchValue(0, 0),  PitchValue(10, 100), PitchValue(20, 100), PitchValue(30, 0),
                                                    PitchValue(40, 0), PitchValue(50, 100), PitchValue(60, 100)};

static const PitchValues prebend_curve = {PitchValue(0, 100), PitchValue(60, 100)};

static const PitchValues prebend_release_curve = {PitchValue(0, 100), PitchValue(15, 100), PitchValue(30, 0), PitchValue(60, 0)};

//---------------------------------------------------------
//   Bend
//---------------------------------------------------------

Bend::Bend(DummyParentOr<Note> parent) : EngravingItem(ElementType::BEND, parent, ElementFlag::MOVABLE) {
    init_element_style(&::bend_style);
}

//---------------------------------------------------------
//   font
//---------------------------------------------------------

Font Bend::font(double sp) const {
    Font f(font_face_, Font::Type::UNKNOWN);
    f.set_bold(font_style_ & FontStyle::BOLD);
    f.set_italic(font_style_ & FontStyle::ITALIC);
    f.set_underline(font_style_ & FontStyle::UNDERLINE);
    f.set_strike(font_style_ & FontStyle::STRIKE);
    double m = font_size_;
    m *= sp / default_spatium();

    f.set_point_size_f(m);
    return f;
}

BendType Bend::parse_bend_type_from_curve() const {
    if (points_ == ::bend_curve) {
        return BendType::BEND;
    } else if (points_ == ::bend_release_curve) {
        return BendType::BEND_RELEASE;
    } else if (points_ == ::bend_release_bend_curve) {
        return BendType::BEND_RELEASE_BEND;
    } else if (points_ == ::prebend_curve) {
        return BendType::PREBEND;
    } else if (points_ == ::prebend_release_curve) {
        return BendType::PREBEND_RELEASE;
    } else {
        return BendType::CUSTOM;
    }
}

void Bend::update_points_by_bend_type(const BendType bend_type) {
    switch (bend_type) {
        case BendType::BEND: points_ = ::bend_curve; break;
        case BendType::BEND_RELEASE: points_ = ::bend_release_curve; break;
        case BendType::BEND_RELEASE_BEND: points_ = ::bend_release_bend_curve; break;
        case BendType::PREBEND: points_ = ::prebend_curve; break;
        case BendType::PREBEND_RELEASE: points_ = ::prebend_release_curve; break;
        default: break;
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Bend::get_property(Pid id) const {
    switch (id) {
        case Pid::FONT_FACE: return font_face_;
        case Pid::FONT_SIZE: return font_size_;
        case Pid::FONT_STYLE: return int(font_style_);
        case Pid::PLAY: return play_bend();
        case Pid::LINE_WIDTH: return line_width_;
        case Pid::BEND_TYPE: return static_cast<int>(parse_bend_type_from_curve());
        case Pid::BEND_CURVE: return points_;
        default: return EngravingItem::get_property(id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Bend::set_property(Pid id, const PropertyValue &v) {
    switch (id) {
        case Pid::FONT_FACE: font_face_ = v.value<String>(); break;
        case Pid::FONT_SIZE: font_size_ = v.to_real(); break;
        case Pid::FONT_STYLE: font_style_ = FontStyle(v.to_int()); break;
        case Pid::PLAY: set_play_bend(v.to_bool()); break;
        case Pid::LINE_WIDTH: line_width_ = v.value<Spatium>(); break;
        case Pid::BEND_TYPE: update_points_by_bend_type(static_cast<BendType>(v.to_int())); break;
        case Pid::BEND_CURVE: set_points(v.value<PitchValues>()); break;
        default: return EngravingItem::set_property(id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Bend::property_default(Pid id) const {
    switch (id) {
        case Pid::PLAY: return true;
        case Pid::BEND_TYPE: return static_cast<int>(BendType::BEND);
        case Pid::BEND_CURVE: return ::bend_curve;
        default: return EngravingItem::property_default(id);
    }
}
