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

#include "fermata.h"

#include "chord.h"
#include "chordrest.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "page.h"
#include "score.h"
#include "staff.h"
#include "stafftype.h"
#include "system.h"
#include "types/symnames.h"
#include "types/translatablestring.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   fermataStyle
//---------------------------------------------------------

static const ElementStyle fermata_style{
    {Sid::FERMATA_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   Fermata
//---------------------------------------------------------

Fermata::Fermata(EngravingObject *parent) : EngravingItem(ElementType::FERMATA, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    set_placement(PlacementV::ABOVE);
    sym_id_ = SymId::NO_SYM;
    set_play(true);
    init_element_style(&::mu::engraving::fermata_style);
}

//---------------------------------------------------------
//   subtype
//---------------------------------------------------------

int Fermata::subtype() const {
    String s = String::from_ascii(SymNames::name_for_sym_id(sym_id_).ascii());
    if (s.ends_with(u"Below")) {
        return int(SymNames::sym_id_by_name(s.left(s.size() - 5) + u"Above"));
    } else {
        return int(sym_id_);
    }
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Fermata::subtype_user_name() const { return SymNames::user_name_for_sym_id(sym_id()); }

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

Measure *Fermata::measure() const {
    Segment *s = segment();
    return to_measure(s ? s->ownership_parent() : 0);
}

//---------------------------------------------------------
//   system
//---------------------------------------------------------

System *Fermata::system() const {
    Measure *m = measure();
    return m ? m->system() : nullptr;
}

//---------------------------------------------------------
//   page
//---------------------------------------------------------

Page *Fermata::page() const {
    System *s = system();
    return s ? s->page() : nullptr;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Fermata::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SYMBOL: return PropertyValue::from_value(sym_id_);
        case Pid::TIME_STRETCH: return time_stretch();
        case Pid::PLAY: return play();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Fermata::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SYMBOL: set_sym_id_and_time_stretch(v.value<SymId>()); break;
        case Pid::PLACEMENT: {
            PlacementV p = v.value<PlacementV>();
            if (p != placement()) {
                String s = String::from_ascii(SymNames::name_for_sym_id(sym_id_).ascii());
                bool up = place_above();
                if (s.ends_with(up ? u"Above" : u"Below")) {
                    String s2 = s.left(s.size() - 5) + (up ? u"Below" : u"Above");
                    sym_id_ = SymNames::sym_id_by_name(s2);
                }
                set_placement(p);
            }
        } break;
        case Pid::PLAY: set_play(v.to_bool()); break;
        case Pid::TIME_STRETCH: set_time_stretch(v.to_double()); break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Fermata::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::PLACEMENT: return track() & 1 ? PlacementV::BELOW : PlacementV::ABOVE;
        case Pid::TIME_STRETCH:
            switch (fermata_type()) {
                case FermataType::VERY_SHORT: return 1.25;
                case FermataType::SHORT:
                case FermataType::SHORT_HENZE: return 1.5;
                case FermataType::NORMAL:
                case FermataType::UNDEFINED: return 2.0;
                case FermataType::LONG:
                case FermataType::LONG_HENZE: return 3.0;
                case FermataType::VERY_LONG: return 4.0;
            }
            break;
        case Pid::PLAY: return true;
        default: break;
    }
    return EngravingItem::property_default(property_id);
}

//---------------------------------------------------------
//   resetProperty
//---------------------------------------------------------

void Fermata::reset_property(Pid id) {
    switch (id) {
        case Pid::TIME_STRETCH: set_property(id, property_default(id)); return;

        default: break;
    }
    EngravingItem::reset_property(id);
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Fermata::mag() const {
    double m = staff() ? staff()->staff_mag(tick()) * style().style_d(Sid::ARTICULATION_MAG) : 1.0;
    if (segment() && segment()->is_chord_rest_type() && segment()->element(track())) {
        m *= to_chord_rest(segment()->element(track()))->intrinsic_mag();
    }
    return m;
}

void Fermata::set_sym_id_and_time_stretch(SymId id) {
    sym_id_ = id;
    time_stretch_ = time_stretch_ == -1 ? property_default(Pid::TIME_STRETCH).value<double>() : -1;
}

FermataType Fermata::fermata_type() const {
    static const std::unordered_map<SymId, FermataType> fermata_types = {
        {SymId::FERMATA_ABOVE, FermataType::NORMAL},
        {SymId::FERMATA_BELOW, FermataType::NORMAL},
        {SymId::FERMATA_LONG_ABOVE, FermataType::LONG},
        {SymId::FERMATA_LONG_BELOW, FermataType::LONG},
        {SymId::FERMATA_LONG_HENZE_ABOVE, FermataType::LONG_HENZE},
        {SymId::FERMATA_LONG_HENZE_BELOW, FermataType::LONG_HENZE},
        {SymId::FERMATA_VERY_LONG_ABOVE, FermataType::VERY_LONG},
        {SymId::FERMATA_VERY_LONG_BELOW, FermataType::VERY_LONG},
        {SymId::FERMATA_SHORT_HENZE_ABOVE, FermataType::SHORT_HENZE},
        {SymId::FERMATA_SHORT_HENZE_BELOW, FermataType::SHORT_HENZE},
        {SymId::FERMATA_VERY_SHORT_ABOVE, FermataType::VERY_SHORT},
        {SymId::FERMATA_VERY_SHORT_BELOW, FermataType::VERY_SHORT},
        {SymId::FERMATA_SHORT_ABOVE, FermataType::SHORT},
        {SymId::FERMATA_SHORT_BELOW, FermataType::SHORT},
    };

    auto search = fermata_types.find(sym_id());

    if (search != fermata_types.cend()) {
        return search->second;
    }

    return FermataType::UNDEFINED;
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

Sid Fermata::default_pos_sid() const { return place_above() ? Sid::FERMATA_POS_ABOVE : Sid::FERMATA_POS_BELOW; }
} // namespace mu::engraving
