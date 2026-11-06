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

#include "tremolosinglechord.h"

#include "draw/types/transform.h"

#include "types/typesconv.h"

#include "style/style.h"

#include "rendering/score/beamtremololayout.h"

#include "beam.h"
#include "chord.h"
#include "stem.h"
#include "system.h"
#include "stafftype.h"

#include "log.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
TremoloSingleChord::TremoloSingleChord(DummyParentOr<Chord> parent)
    : EngravingItem(ElementType::TREMOLO_SINGLECHORD, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {}

TremoloSingleChord::TremoloSingleChord(const TremoloSingleChord &t) : EngravingItem(t) {
    set_tremolo_type(t.tremolo_type());
    duration_type_ = t.duration_type_;
}

TremoloSingleChord::~TremoloSingleChord() {
    //
    // delete all references from chords
    //
    if (chord() && chord()->tremolo_single_chord() == this) {
        chord()->set_tremolo_single_chord(nullptr);
    }
}

double TremoloSingleChord::chord_mag() const { return chord() ? chord()->intrinsic_mag() : 1.0; }

double TremoloSingleChord::min_height() const {
    const double sw = style().style_s(Sid::TREMOLO_LINE_WIDTH).val() * chord_mag();
    const double td = style().style_s(Sid::TREMOLO_DISTANCE).val() * chord_mag();
    return (lines() - 1) * td + sw;
}

//---------------------------------------------------------
//   setTremoloType
//---------------------------------------------------------

void TremoloSingleChord::set_tremolo_type(TremoloType t) {
    tremolo_type_ = t;
    switch (tremolo_type()) {
        case TremoloType::R16:
        case TremoloType::C16: lines_ = 2; break;
        case TremoloType::R32:
        case TremoloType::C32: lines_ = 3; break;
        case TremoloType::R64:
        case TremoloType::C64: lines_ = 4; break;
        case TremoloType::R128:
        case TremoloType::C128: lines_ = 5; break;
        case TremoloType::R256:
        case TremoloType::C256: lines_ = 6; break;
        default: lines_ = 1; break;
    }

    style_changed();
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void TremoloSingleChord::spatium_changed(double old_value, double new_value) {
    EngravingItem::spatium_changed(old_value, new_value);
    compute_shape();
}

//---------------------------------------------------------
//   localSpatiumChanged
//    the scale of a staff changed
//---------------------------------------------------------

void TremoloSingleChord::local_spatium_changed(double old_value, double new_value) {
    EngravingItem::local_spatium_changed(old_value, new_value);
    compute_shape();
}

//---------------------------------------------------------
//   styleChanged
//    the scale of a staff changed
//---------------------------------------------------------

void TremoloSingleChord::style_changed() {
    EngravingItem::style_changed();
    compute_shape();
}

staff_idx_t TremoloSingleChord::v_staff_idx() const { return chord() ? chord()->v_staff_idx() : EngravingItem::v_staff_idx(); }

//---------------------------------------------------------
//   basePath
//---------------------------------------------------------

PainterPath TremoloSingleChord::base_path(double /*stretch*/) const {
    if (is_buzz_roll()) {
        return PainterPath();
    }

    const double sp = spatium() * chord_mag();

    double w2 = sp * style().style_s(Sid::TREMOLO_WIDTH).val() * .5;
    double lw = sp * style().style_s(Sid::TREMOLO_LINE_WIDTH).val();
    double td = sp * style().style_s(Sid::TREMOLO_DISTANCE).val();

    PainterPath ppath;

    // first line
    ppath.add_rect(-w2, 0.0, 2.0 * w2, lw);
    double ty = td;

    // other lines
    for (int i = 1; i < lines_; i++) {
        ppath.add_rect(-w2, ty, 2.0 * w2, lw);
        ty += td;
    }

    Transform shear_transform;
    shear_transform.shear(0.0, -(lw / 2.0) / w2);
    ppath = shear_transform.map(ppath);

    return ppath;
}

void TremoloSingleChord::compute_shape() {
    if (is_buzz_roll()) {
        setbbox(sym_bbox(SymId::BUZZ_ROLL));
    } else {
        path_ = base_path();
        setbbox(path_.bounding_rect());
    }
}

TDuration TremoloSingleChord::duration_type() const { return duration_type_; }

void TremoloSingleChord::set_duration_type(TDuration d) {
    if (duration_type_ == d) {
        return;
    }

    duration_type_ = d;
    style_changed();
}

//---------------------------------------------------------
//   tremoloLen
//---------------------------------------------------------

Fraction TremoloSingleChord::tremolo_len() const {
    Fraction f;
    switch (lines()) {
        case 1: f.set(1, 8); break;
        case 2: f.set(1, 16); break;
        case 3: f.set(1, 32); break;
        case 4: f.set(1, 64); break;
        case 5: f.set(1, 128); break;
        case 6: f.set(1, 256); break;
    }
    return f;
}

//---------------------------------------------------------
//   subtypeName
//---------------------------------------------------------

TranslatableString TremoloSingleChord::subtype_user_name() const { return TConv::user_name(tremolo_type()); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TremoloSingleChord::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::TREMOLO_TYPE: return int(tremolo_type_);
        case Pid::PLAY: return play_tremolo_;
        default: break;
    }
    return EngravingItem::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TremoloSingleChord::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::TREMOLO_TYPE: set_tremolo_type(TremoloType(val.to_int())); break;
        case Pid::PLAY: set_play_tremolo(val.to_bool()); break;
        default: return EngravingItem::set_property(property_id, val);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TremoloSingleChord::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::PLAY: return true;
        default: return EngravingItem::property_default(property_id);
    }
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void TremoloSingleChord::scan_elements(std::function<void(EngravingItem *)> func) {
    if (chord() && chord()->tremolo_chord_type() == TremoloChordType::TREMOLO_SECOND_CHORD) {
        return;
    }
    EngravingItem::scan_elements(func);
}
} // namespace mu::engraving
