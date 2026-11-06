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
#include "vibrato.h"

#include <cmath>

#include "iengravingfont.h"
#include "log.h"
#include "score.h"
#include "system.h"
#include "trill.h"
#include "types/typesconv.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
VibratoSegment::VibratoSegment(Vibrato *sp) : LineSegment(ElementType::VIBRATO_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {}

//---------------------------------------------------------
//   symbolLine
//---------------------------------------------------------

void VibratoSegment::symbol_line(SymId start, SymId fill) {
    double x1 = 0;
    double x2 = pos2().x();
    double w = x2 - x1;
    double mag = mag_s();
    IEngravingFontPtr f = score()->engraving_font();

    symbols_.clear();
    symbols_.push_back(start);
    double w1 = f->advance(start, mag);
    double w2 = f->advance(fill, mag);
    int n = lrint((w - w1) / w2);
    for (int i = 0; i < n; ++i) {
        symbols_.push_back(fill);
    }
    RectF r(f->bbox(symbols_, mag));
    setbbox(r);
}

void VibratoSegment::symbol_line(SymId start, SymId fill, SymId end) {
    double x1 = 0;
    double x2 = pos2().x();
    double w = x2 - x1;
    double mag = mag_s();
    IEngravingFontPtr f = score()->engraving_font();

    symbols_.clear();
    symbols_.push_back(start);
    double w1 = f->bbox(start, mag).width();
    double w2 = f->width(fill, mag);
    double w3 = f->width(end, mag);
    int n = lrint((w - w1 - w3) / w2);
    for (int i = 0; i < n; ++i) {
        symbols_.push_back(fill);
    }
    symbols_.push_back(end);
    RectF r(f->bbox(symbols_, mag));
    setbbox(r);
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *VibratoSegment::property_delegate(Pid pid) const {
    if (pid == Pid::VIBRATO_TYPE || pid == Pid::PLACEMENT) {
        return spanner();
    }
    return LineSegment::property_delegate(pid);
}

//---------------------------------------------------------
//   vibratoStyle
//---------------------------------------------------------

static const ElementStyle vibrato_style{
    {Sid::VIBRATO_PLACEMENT, Pid::PLACEMENT},
};

//---------------------------------------------------------
//   Vibrato
//---------------------------------------------------------

Vibrato::Vibrato(DummyParentOr<EngravingItem> parent) : SLine(ElementType::VIBRATO, parent) {
    init_element_style(&::mu::engraving::vibrato_style);
    vibrato_type_ = VibratoType::GUITAR_VIBRATO;
}

Vibrato::~Vibrato() {}

static const ElementStyle vibrato_segment_style{
    {Sid::VIBRATO_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment *Vibrato::create_line_segment() {
    VibratoSegment *seg = new VibratoSegment(this);
    seg->set_track(track());
    seg->set_color(line_color());
    seg->init_element_style(&::mu::engraving::vibrato_segment_style);
    return seg;
}

PointF Vibrato::line_pos(Grip grip, System **system) const { return Trill::trill_line_pos(this, grip, system); }

//---------------------------------------------------------
//   vibratoTypeName
//---------------------------------------------------------

String Vibrato::vibrato_type_user_name() const { return TConv::translated_user_name(vibrato_type()); }

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Vibrato::subtype_user_name() const { return TConv::user_name(vibrato_type()); }

Sid Vibrato::default_pos_sid() const { return place_above() ? Sid::VIBRATO_POS_ABOVE : Sid::VIBRATO_POS_BELOW; }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Vibrato::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::VIBRATO_TYPE: return int(vibrato_type());
        default: break;
    }
    return SLine::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Vibrato::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::VIBRATO_TYPE: set_vibrato_type(VibratoType(val.to_int())); break;
        case Pid::COLOR:
            set_color(val.value<Color>());
            set_line_color(val.value<Color>());
            break;
        default: return SLine::set_property(property_id, val);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Vibrato::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::VIBRATO_TYPE: return static_cast<int>(VibratoType::GUITAR_VIBRATO);
        case Pid::PLACEMENT: return style().style_v(Sid::VIBRATO_PLACEMENT);
        default: return SLine::property_default(property_id);
    }
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

} // namespace mu::engraving
