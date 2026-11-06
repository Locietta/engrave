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

#include "tremolotwochord.h"

#include "chord.h"
#include "draw/types/transform.h"
#include "log.h"
#include "rendering/score/beamtremololayout.h"
#include "stafftype.h"
#include "stem.h"
#include "style/style.h"
#include "system.h"
#include "types/typesconv.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   tremoloStyle
//---------------------------------------------------------

static const ElementStyle tremolo_style{{Sid::TREMOLO_STYLE, Pid::TREMOLO_STYLE}};

//---------------------------------------------------------
//   Tremolo
//---------------------------------------------------------

TremoloTwoChord::TremoloTwoChord(DummyParentOr<Chord> parent) : BeamBase(ElementType::TREMOLO_TWOCHORD, parent, ElementFlag::MOVABLE) {
    init_element_style(&::mu::engraving::tremolo_style);
}

TremoloTwoChord::TremoloTwoChord(const TremoloTwoChord &t) : BeamBase(t) {
    set_tremolo_type(t.tremolo_type());
    chord1_ = t.chord1();
    chord2_ = t.chord2();
    duration_type_ = t.duration_type_;
}

TremoloTwoChord::~TremoloTwoChord() {
    //
    // delete all references from chords
    //
    if (chord1_ && chord1_->tremolo_two_chord() == this) {
        chord1_->set_tremolo_two_chord(nullptr);
    }
    if (chord2_ && chord2_->tremolo_two_chord() == this) {
        chord2_->set_tremolo_two_chord(nullptr);
    }

    clear_beam_segments();
}

//---------------------------------------------------------
//   chordMag
//---------------------------------------------------------

double TremoloTwoChord::chord_mag() const { return chord() ? chord()->intrinsic_mag() : 1.0; }

//---------------------------------------------------------
//   minHeight
//---------------------------------------------------------

double TremoloTwoChord::min_height() const {
    const double sw = style().style_s(Sid::TREMOLO_LINE_WIDTH).val() * chord_mag();
    const double td = style().style_s(Sid::TREMOLO_DISTANCE).val() * chord_mag();
    return (lines() - 1) * td + sw;
}

//---------------------------------------------------------
//   chordBeamAnchor
//---------------------------------------------------------

PointF TremoloTwoChord::chord_beam_anchor(const ChordRest *chord, ChordBeamAnchorType anchor_type) const {
    return rendering::score::BeamTremoloLayout::chord_beam_anchor(this->ldata(), chord, anchor_type);
}

//---------------------------------------------------------
//   setTremoloType
//---------------------------------------------------------

void TremoloTwoChord::set_tremolo_type(TremoloType t) {
    IF_ASSERT_FAILED(is_tremolo_two_chord(t)) { return; }
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
//   reset
//---------------------------------------------------------

void TremoloTwoChord::reset() {
    EngravingItem::reset();
    if (user_modified()) {
        //undoChangeProperty(Pid::BEAM_POS, PropertyValue::fromValue(beamPos()));
        undo_change_property(Pid::USER_MODIFIED, false);
    }
    undo_change_property(Pid::STEM_DIRECTION, DirectionV::AUTO);
    reset_property(Pid::BEAM_NO_SLOPE);
}

//---------------------------------------------------------
//   setBeamDirection
//---------------------------------------------------------

void TremoloTwoChord::set_direction(DirectionV d) {
    if (direction() == d) {
        return;
    }

    do_set_direction(d);

    if (d != DirectionV::AUTO) {
        set_up(d == DirectionV::UP);
    }
    if (chord1_) {
        chord1_->set_stem_direction(d);
    }
    if (chord2_) {
        chord2_->set_stem_direction(d);
    }
}

//---------------------------------------------------------
//   crossStaffBeamBetween
//    Return true if tremolo is two-note cross-staff and beams between staves
//---------------------------------------------------------

bool TremoloTwoChord::cross_staff_beam_between() const {
    return ((chord1_->staff_move() > chord2_->staff_move()) && chord1_->up() && !chord2_->up()) ||
           ((chord1_->staff_move() < chord2_->staff_move()) && !chord1_->up() && chord2_->up());
}

TDuration TremoloTwoChord::duration_type() const { return duration_type_; }

void TremoloTwoChord::set_duration_type(TDuration d) {
    if (duration_type_ == d) {
        return;
    }

    duration_type_ = d;
    style_changed();
}

//---------------------------------------------------------
//   tremoloLen
//---------------------------------------------------------

Fraction TremoloTwoChord::tremolo_len() const {
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
//   setBeamPos
//---------------------------------------------------------

void TremoloTwoChord::set_beam_pos(const PairF &bp) {
    int idx = direction_idx();
    set_user_modified(true);
    set_generated(false);

    double spatium_value = spatium();
    beam_fragment_.py1[idx] = bp.first * spatium_value;
    beam_fragment_.py2[idx] = bp.second * spatium_value;
}

//---------------------------------------------------------
//   beamPos
//---------------------------------------------------------

PairF TremoloTwoChord::beam_pos() const {
    int idx = direction_idx();
    double spatium_value = spatium();
    return PairF(beam_fragment_.py1[idx] / spatium_value, beam_fragment_.py2[idx] / spatium_value);
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void TremoloTwoChord::trigger_layout() const {
    if (chord1_ && chord2_) {
        to_chord_rest(chord1_)->trigger_layout();
        to_chord_rest(chord2_)->trigger_layout();
    } else {
        EngravingItem::trigger_layout();
    }
}

//---------------------------------------------------------
//   subtypeName
//---------------------------------------------------------

TranslatableString TremoloTwoChord::subtype_user_name() const { return TConv::user_name(tremolo_type()); }

//---------------------------------------------------------
//   customStyleApplicable
//---------------------------------------------------------

bool TremoloTwoChord::custom_style_applicable() const {
    return (duration_type().type() == DurationType::V_HALF) && (staff_type()->group() != StaffGroup::TAB);
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TremoloTwoChord::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::TREMOLO_TYPE: return int(tremolo_type_);
        case Pid::TREMOLO_STYLE: return int(style_);
        case Pid::PLAY: return play_tremolo_;
        default: break;
    }
    return BeamBase::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TremoloTwoChord::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::TREMOLO_TYPE: set_tremolo_type(TremoloType(val.to_int())); break;
        case Pid::TREMOLO_STYLE:
            if (custom_style_applicable()) {
                set_tremolo_style(TremoloStyle(val.to_int()));
            }
            break;
        case Pid::BEAM_POS:
            if (user_modified()) {
                set_beam_pos(val.value<PairF>());
            }
            break;
        case Pid::PLAY: set_play_tremolo(val.to_bool()); break;
        default: return BeamBase::set_property(property_id, val);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TremoloTwoChord::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::TREMOLO_STYLE: return style().style_i(Sid::TREMOLO_STYLE);
        case Pid::PLAY: return true;
        default: return BeamBase::property_default(property_id);
    }
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void TremoloTwoChord::scan_elements(std::function<void(EngravingItem *)> func) {
    if (chord() && chord()->tremolo_chord_type() == TremoloChordType::TREMOLO_SECOND_CHORD) {
        return;
    }
    EngravingItem::scan_elements(func);
}

void TremoloTwoChord::clear_beam_segments() {
    BeamSegment *chord1_segment = chord1_ ? chord1_->beamlet() : nullptr;
    BeamSegment *chord2_segment = chord2_ ? chord2_->beamlet() : nullptr;

    if (chord1_segment || chord2_segment) {
        for (BeamSegment *segment : beam_segments_) {
            if (chord1_segment && chord1_segment == segment) {
                chord1_->set_beamlet(nullptr);
            } else if (chord2_segment && chord2_segment == segment) {
                chord2_->set_beamlet(nullptr);
            }
        }
    }

    BeamBase::clear_beam_segments();
}

int TremoloTwoChord::max_cr_move() const { return std::max(chord1_->staff_move(), chord2_->staff_move()); }

int TremoloTwoChord::min_cr_move() const { return std::min(chord1_->staff_move(), chord2_->staff_move()); }

// used for palettes
PainterPath TremoloTwoChord::base_path(double stretch) const {
    bool trad_alternate = style_ == TremoloStyle::TRADITIONAL_ALTERNATE;
    if (trad_alternate && muse::real_is_equal(stretch, 0.)) {
        // this shape will have to be constructed after the stretch
        // is known
        return PainterPath();
    }

    // TODO: This should be a style setting, to replace tremoloStrokeLengthMultiplier
    static constexpr Spatium stem_gap_sp = 0.65_sp;

    const double sp = spatium() * chord_mag();

    // overall width of two-note tremolos should not be changed if chordMag() isn't 1.0
    double w2 = sp * style().style_s(Sid::TREMOLO_WIDTH).val() * .5 / chord_mag();
    double lw = sp * style().style_s(Sid::TREMOLO_LINE_WIDTH).val();
    double td = sp * style().style_s(Sid::TREMOLO_DISTANCE).val();

    PainterPath ppath;

    // first line
    ppath.add_rect(-w2, 0.0, 2.0 * w2, lw);
    double ty = td;

    // other lines
    for (int i = 1; i < lines_; i++) {
        if (trad_alternate) {
            double stem_width1 = chord1_->stem()->line_width_mag() / stretch;
            double stem_width2 = chord2_->stem()->line_width_mag() / stretch;
            double inset = stem_gap_sp.to_absolute(spatium()) / stretch;

            ppath.add_rect(-w2 + inset + stem_width1, ty, 2.0 * w2 - (inset * 2.) - (stem_width2 + stem_width1), lw);
        } else {
            ppath.add_rect(-w2, ty, 2.0 * w2, lw);
        }
        ty += td;
    }

    if (!chord()) {
        // for the palette or for one-note tremolos
        Transform shear_transform;
        shear_transform.shear(0.0, -(lw / 2.0) / w2);
        ppath = shear_transform.map(ppath);
    }

    return ppath;
}

void TremoloTwoChord::compute_shape() {
    path_ = base_path();
    setbbox(path_.bounding_rect());
}
} // namespace mu::engraving
