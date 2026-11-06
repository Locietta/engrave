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

#include "marker.h"

#include "dom/staff.h"
#include "dom/system.h"
#include "types/typesconv.h"

#include "measure.h"
#include "score.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   markerStyle
//---------------------------------------------------------

static const ElementStyle marker_style{
    {Sid::REPEAT_LEFT_PLACEMENT, Pid::PLACEMENT},
    {Sid::REPEAT_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   MarkerTypeTable
//---------------------------------------------------------

const std::vector<MarkerTypeTableItem> marker_type_table{
    {MarkerType::SEGNO, "<sym>segno</sym>", "segno", false},
    {MarkerType::VARSEGNO, "<sym>segnoSerpent1</sym>", "varsegno", false},
    {MarkerType::CODA, "<sym>coda</sym>", "codab", false},
    {MarkerType::VARCODA, "<sym>codaSquare</sym>", "varcoda", false},
    {MarkerType::CODETTA, "<sym>coda</sym><sym>coda</sym>", "codetta", false},
    {MarkerType::FINE, "Fine", "fine", true},
    {MarkerType::TOCODA, "To Coda", "coda", true},
    {MarkerType::TOCODASYM, "To <sym>coda</sym>", "coda", true},
    {MarkerType::DA_CODA, "Da Coda", "coda", true},
    {MarkerType::DA_DBLCODA, "Da Doppia Coda", "coda", true},
};

//---------------------------------------------------------
//   Marker
//---------------------------------------------------------

Marker::Marker(DummyParentOr<EngravingItem> parent) : Marker(parent, TextStyleType::REPEAT_LEFT) { reset_property(Pid::MUSIC_SYMBOL_SIZE); }

Marker::Marker(DummyParentOr<EngravingItem> parent, TextStyleType tid)
    : TextBase(ElementType::MARKER, parent, tid, ElementFlag::MOVABLE | ElementFlag::ON_STAFF | ElementFlag::SYSTEM) {
    init_element_style(&::mu::engraving::marker_style);
    reset_property(Pid::MUSIC_SYMBOL_SIZE);
    marker_type_ = MarkerType::FINE;
}

//---------------------------------------------------------
//   setMarkerType
//---------------------------------------------------------

void Marker::set_marker_type(MarkerType t) {
    bool change_label = get_property(Pid::LABEL) == property_default(Pid::LABEL);
    marker_type_ = t;
    for (const MarkerTypeTableItem &p : ::mu::engraving::marker_type_table) {
        if (p.type == t) {
            if (empty()) {
                set_xml_text(String::from_ascii(p.text.ascii()));
            }
            if (change_label) {
                set_label(String::from_ascii(p.label.ascii()));
            }
            TextStyleType ts = p.right_aligned ? TextStyleType::REPEAT_RIGHT : TextStyleType::REPEAT_LEFT;
            if (text_style_type() != ts) {
                init_text_style_type(ts);
            }

            break;
        }
    }
}

//---------------------------------------------------------
//   markerTypeUserName
//---------------------------------------------------------

String Marker::marker_type_user_name() const { return TConv::translated_user_name(marker_type_); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Marker::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::LABEL: return label();
        case Pid::MARKER_TYPE: return marker_type();
        case Pid::MARKER_CENTER_ON_SYMBOL: return center_on_symbol();
        default: break;
    }
    return TextBase::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Marker::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::LABEL: set_label(v.value<String>()); break;
        case Pid::MARKER_TYPE: set_marker_type(v.value<MarkerType>()); break;
        case Pid::MARKER_CENTER_ON_SYMBOL: set_center_on_symbol(v.to_bool()); break;
        default:
            if (!TextBase::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Marker::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::LABEL:
            for (const MarkerTypeTableItem &p : ::mu::engraving::marker_type_table) {
                if (marker_type_ == p.type) {
                    return String::from_ascii(p.label.ascii());
                }
            }
            return String();
        case Pid::MARKER_TYPE: return MarkerType::FINE;
        case Pid::PLACEMENT: return PlacementV::ABOVE;
        case Pid::MARKER_CENTER_ON_SYMBOL: return true;
        case Pid::MUSIC_SYMBOL_SIZE: return 18.0;
        default: break;
    }
    return TextBase::property_default(property_id);
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Marker::next_segment_element() {
    Segment *seg;
    if (marker_type() == MarkerType::FINE) {
        seg = measure()->last();
        return seg->first_element_for_navigation(staff_idx());
    }
    Measure *prev_measure = measure()->prev_measure_mm();
    if (prev_measure) {
        seg = prev_measure->last();
        return seg->first_element_for_navigation(staff_idx());
    }
    return EngravingItem::next_segment_element();
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Marker::prev_segment_element() {
    //it's the same barline
    return next_segment_element();
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

String Marker::symbol_string() const {
    // Returns the coda/segno symbol if present
    const static std::array repeat_symbol_names{
        String(u"<sym>coda</sym>"),          String(u"<sym>codaSquare</sym>"),    String(u"<sym>codaJapanes</sym>"),
        String(u"<sym>segno</sym>"),         String(u"<sym>segnoSerpent1</sym>"), String(u"<sym>segnoSerpent2</sym>"),
        String(u"<sym>segnoJapanese</sym>"),
    };

    for (const String &sym : repeat_symbol_names) {
        if (xml_text().contains(sym)) {
            return sym;
        }
    }

    return String();
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Marker::subtype_user_name() const { return TConv::user_name(marker_type_); }
} // namespace mu::engraving
