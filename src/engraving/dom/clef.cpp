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

/**
 \file
 Implementation of classes Clef (partial) and ClefList (complete).
*/

#include "clef.h"

#include "../editing/editclef.h"
#include "../editing/editproperty.h"
#include "../editing/transaction/transaction.h"
#include "../types/typesconv.h"
#include "ambitus.h"
#include "factory.h"
#include "measure.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "translation.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
// table must be in sync with enum ClefType in types.h
const ClefInfo ClefInfo::clef_table[] = {
    //                     line pOff|-lines for sharps---||---lines for flats--   |  symbol                | valid in staff group
    {ClefType::G, 2, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::G_CLEF, StaffGroup::STANDARD},
    {ClefType::G15_MB, 2, 31, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::G_CLEF15MB, StaffGroup::STANDARD},
    {ClefType::G8_VB, 2, 38, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::G_CLEF8VB, StaffGroup::STANDARD},
    {ClefType::G8_VA, 2, 52, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::G_CLEF8VA, StaffGroup::STANDARD},
    {ClefType::G15_MA, 2, 59, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::G_CLEF15MA, StaffGroup::STANDARD},
    {ClefType::G8_VB_O, 2, 38, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::G_CLEF8VB_OLD, StaffGroup::STANDARD},
    {ClefType::G8_VB_P, 2, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::G_CLEF8VB_PARENS, StaffGroup::STANDARD},
    {ClefType::G_1, 1, 47, {2, 5, 1, 4, 7, 3, 6, 6, 3, 7, 4, 8, 5, 9}, SymId::G_CLEF, StaffGroup::STANDARD},

    {ClefType::C1, 1, 43, {5, 1, 4, 0, 3, -1, 2, 2, 6, 3, 7, 4, 8, 5}, SymId::C_CLEF, StaffGroup::STANDARD},
    {ClefType::C2, 2, 41, {3, 6, 2, 5, 1, 4, 0, 0, 4, 1, 5, 2, 6, 3}, SymId::C_CLEF, StaffGroup::STANDARD},
    {ClefType::C3, 3, 39, {1, 4, 0, 3, 6, 2, 5, 5, 2, 6, 3, 7, 4, 8}, SymId::C_CLEF, StaffGroup::STANDARD},
    {ClefType::C4, 4, 37, {6, 2, 5, 1, 4, 0, 3, 3, 0, 4, 1, 5, 2, 6}, SymId::C_CLEF, StaffGroup::STANDARD},
    {ClefType::C5, 5, 35, {4, 0, 3, -1, 2, 5, 1, 1, 5, 2, 6, 3, 7, 4}, SymId::C_CLEF, StaffGroup::STANDARD},
    {ClefType::C_19C, 2, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::C_CLEF_SQUARE, StaffGroup::STANDARD},
    {ClefType::C1_F18C, 1, 43, {5, 1, 4, 0, 3, -1, 2, 2, 6, 3, 7, 4, 8, 5}, SymId::C_CLEF_FRENCH, StaffGroup::STANDARD},
    {ClefType::C3_F18C, 3, 39, {1, 4, 0, 3, 6, 2, 5, 5, 2, 6, 3, 7, 4, 8}, SymId::C_CLEF_FRENCH, StaffGroup::STANDARD},
    {ClefType::C4_F18C, 4, 37, {6, 2, 5, 1, 4, 0, 3, 3, 0, 4, 1, 5, 2, 6}, SymId::C_CLEF_FRENCH, StaffGroup::STANDARD},
    {ClefType::C1_F20C, 1, 43, {5, 1, 4, 0, 3, -1, 2, 2, 6, 3, 7, 4, 8, 5}, SymId::C_CLEF_FRENCH20_C, StaffGroup::STANDARD},
    {ClefType::C3_F20C, 3, 39, {1, 4, 0, 3, 6, 2, 5, 5, 2, 6, 3, 7, 4, 8}, SymId::C_CLEF_FRENCH20_C, StaffGroup::STANDARD},
    {ClefType::C4_F20C, 4, 37, {6, 2, 5, 1, 4, 0, 3, 3, 0, 4, 1, 5, 2, 6}, SymId::C_CLEF_FRENCH20_C, StaffGroup::STANDARD},

    {ClefType::F, 4, 33, {2, 5, 1, 4, 7, 3, 6, 6, 3, 7, 4, 8, 5, 9}, SymId::F_CLEF, StaffGroup::STANDARD},
    {ClefType::F15_MB, 4, 19, {2, 5, 1, 4, 7, 3, 6, 6, 3, 7, 4, 8, 5, 9}, SymId::F_CLEF15MB, StaffGroup::STANDARD},
    {ClefType::F8_VB, 4, 26, {2, 5, 1, 4, 7, 3, 6, 6, 3, 7, 4, 8, 5, 9}, SymId::F_CLEF8VB, StaffGroup::STANDARD},
    {ClefType::F_8VA, 4, 40, {2, 5, 1, 4, 7, 3, 6, 6, 3, 7, 4, 8, 5, 9}, SymId::F_CLEF8VA, StaffGroup::STANDARD},
    {ClefType::F_15MA, 4, 47, {2, 5, 1, 4, 7, 3, 6, 6, 3, 7, 4, 8, 5, 9}, SymId::F_CLEF15MA, StaffGroup::STANDARD},
    {ClefType::F_B, 3, 35, {4, 0, 3, -1, 2, 5, 1, 1, 5, 2, 6, 3, 7, 4}, SymId::F_CLEF, StaffGroup::STANDARD},
    {ClefType::F_C, 5, 31, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::F_CLEF, StaffGroup::STANDARD},
    {ClefType::F_F18C, 4, 33, {2, 5, 1, 4, 7, 3, 6, 6, 3, 7, 4, 8, 5, 9}, SymId::F_CLEF_FRENCH, StaffGroup::STANDARD},
    {ClefType::F_19C, 4, 33, {2, 5, 1, 4, 7, 3, 6, 6, 3, 7, 4, 8, 5, 9}, SymId::F_CLEF19TH_CENTURY, StaffGroup::STANDARD},

    {ClefType::PERC, 2, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::UNPITCHED_PERCUSSION_CLEF1, StaffGroup::PERCUSSION},
    {ClefType::PERC2, 2, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::UNPITCHED_PERCUSSION_CLEF2, StaffGroup::PERCUSSION},

    {ClefType::TAB, 5, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::SIX_STRING_TAB_CLEF, StaffGroup::TAB},
    {ClefType::TAB4, 5, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::FOUR_STRING_TAB_CLEF, StaffGroup::TAB},
    {ClefType::TAB_SERIF, 5, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::SIX_STRING_TAB_CLEF_SERIF, StaffGroup::TAB},
    {ClefType::TAB4_SERIF, 5, 45, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::FOUR_STRING_TAB_CLEF_SERIF, StaffGroup::TAB},

    {ClefType::C4_8VB, 4, 30, {6, 2, 5, 1, 4, 0, 3, 3, 0, 4, 1, 5, 2, 6}, SymId::C_CLEF8VB, StaffGroup::STANDARD},
    {ClefType::G8_VB_C, 2, 38, {0, 3, -1, 2, 5, 1, 4, 4, 1, 5, 2, 6, 3, 7}, SymId::G_CLEF8VB_C_CLEF, StaffGroup::STANDARD},
};

//---------------------------------------------------------
//   Clef
//---------------------------------------------------------

Clef::Clef(DummyParentOr<Segment> parent) : EngravingItem(ElementType::CLEF, parent, ElementFlag::ON_STAFF) {
    clef_to_barline_position_ = ClefToBarlinePosition::AUTO;
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Clef::mag() const {
    double mag = staff() ? staff()->staff_mag(this) : 1.0;
    if (is_small_) {
        mag *= style().style_d(Sid::SMALL_CLEF_MAG);
    }
    return mag;
}

//---------------------------------------------------------
//   setSmall
//---------------------------------------------------------

void Clef::set_small(bool val) { is_small_ = val; }

//---------------------------------------------------------
//   setClefType
//---------------------------------------------------------

void Clef::set_clef_type(ClefType i) {
    if (concert_pitch()) {
        clef_types_.concert_clef = i;
        if (clef_types_.transposing_clef == ClefType::INVALID) {
            clef_types_.transposing_clef = i;
        }
    } else {
        clef_types_.transposing_clef = i;
        if (clef_types_.concert_clef == ClefType::INVALID) {
            clef_types_.concert_clef = i;
        }
    }
}

//---------------------------------------------------------
//   setConcertClef
//---------------------------------------------------------

void Clef::set_concert_clef(ClefType val) { clef_types_.concert_clef = val; }

//---------------------------------------------------------
//   setTransposingClef
//---------------------------------------------------------

void Clef::set_transposing_clef(ClefType val) { clef_types_.transposing_clef = val; }

//---------------------------------------------------------
//   clefType
//---------------------------------------------------------

ClefType Clef::clef_type() const {
    if (concert_pitch()) {
        return clef_types_.concert_clef;
    } else {
        return clef_types_.transposing_clef;
    }
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void Clef::spatium_changed(double old_value, double new_value) { EngravingItem::spatium_changed(old_value, new_value); }

//---------------------------------------------------------
//   undoSetShowCourtesy
//---------------------------------------------------------

void Clef::undo_set_show_courtesy(bool v) { undo_change_property(Pid::SHOW_COURTESY, v); }

//---------------------------------------------------------
//   otherClef
//    try to locate the 'other clef' of a courtesy / main pair
//---------------------------------------------------------

Clef *Clef::other_clef() {
    // if not in a clef-segment-measure hierarchy, do nothing
    if (!ownership_parent() || !ownership_parent()->is_segment()) {
        return nullptr;
    }
    Segment *segm = to_segment(ownership_parent());
    if (!segm->ownership_parent() || !segm->ownership_parent()->is_measure()) {
        return 0;
    }
    Measure *meas = to_measure(segm->ownership_parent());
    Measure *other_meas = nullptr;
    Segment *other_segm = nullptr;
    Fraction segm_tick = segm->tick();
    SegmentType type = SegmentType::CLEF;
    if (segm_tick == meas->tick() && segm->segment_type() == SegmentType::HEADER_CLEF) { // if clef segm is measure-initial
        other_meas = meas->prev_measure();                                               // look for a previous measure
    } else if (segm_tick == meas->tick() + meas->ticks()) {                              // if clef segm is measure-final
        other_meas = meas->next_measure();                                               // look for a next measure
        type = SegmentType::HEADER_CLEF;
    }
    if (!other_meas) {
        return nullptr;
    }
    // look for a clef segment in the 'other' measure at the same tick of this clef segment
    other_segm = other_meas->find_segment(type, segm_tick);
    if (!other_segm) {
        return nullptr;
    }
    // if any 'other' segment found, look for a clef in the same track as this
    return to_clef(other_segm->element(track()));
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Clef::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::CLEF_TYPE_CONCERT: return clef_types_.concert_clef;
        case Pid::CLEF_TYPE_TRANSPOSING: return clef_types_.transposing_clef;
        case Pid::SHOW_COURTESY: return show_courtesy();
        case Pid::SMALL: return is_small();
        case Pid::CLEF_TO_BARLINE_POS: return clef_to_barline_position_;
        case Pid::IS_COURTESY: return is_courtesy_;
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Clef::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::CLEF_TYPE_CONCERT: set_concert_clef(v.value<ClefType>()); break;
        case Pid::CLEF_TYPE_TRANSPOSING: set_transposing_clef(v.value<ClefType>()); break;
        case Pid::SHOW_COURTESY:
            show_courtesy_ = v.to_bool();
            if (show_courtesy_ && is_header() && selected()) {
                Clef *courtesy_clef = other_clef();
                if (courtesy_clef) {
                    score()->deselect(this);
                    score()->select(courtesy_clef, SelectType::ADD, staff_idx());
                }
            }
            break;
        case Pid::SMALL: set_small(v.to_bool()); break;
        case Pid::CLEF_TO_BARLINE_POS: {
            const auto new_clef_to_bl_pos = v.value<ClefToBarlinePosition>();

            if (new_clef_to_bl_pos != clef_to_barline_position_ && !is_header()) {
                change_clef_to_barline_pos(new_clef_to_bl_pos);
            }
            break;
        }
        case Pid::IS_COURTESY: is_courtesy_ = v.to_bool(); break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

void Clef::change_clef_to_barline_pos(ClefToBarlinePosition new_pos) {
    clef_to_barline_position_ = new_pos;

    if (!ownership_parent()) {
        return;
    }

    Segment *seg = segment();

    staff_idx_t n_staves = score()->nstaves();
    for (staff_idx_t staff_index = 0; staff_index < n_staves; ++staff_index) {
        Clef *clef = to_clef(seg->element(staff_index * ::mu::engraving::voices));
        if (clef) {
            clef->clef_to_barline_position_ = new_pos;
        }
    }
}

void Clef::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::SHOW_COURTESY) {
        if (v.to_bool() != show_courtesy_) {
            score()->undo(new ChangeProperty(this, id, v, ps));
            Clef *paired_clef = other_clef();
            if (paired_clef) {
                score()->undo(new ChangeProperty(paired_clef, id, v, ps));
            }
            if (!segment()->is_header_clef_type()) {
                set_generated(false);
            }
        }
    } else {
        EngravingObject::undo_change_property(id, v, ps);
    }
}

bool Clef::is_header() const {
    const Segment *seg = segment();
    return seg && seg->is_header_clef_type();
}

bool Clef::is_mid_measure_clef() const { return segment() && segment()->rtick().is_not_zero(); }

void Clef::manage_exclusion_from_parts(bool exclude) {
    if (exclude) {
        EngravingItem::manage_exclusion_from_parts(exclude);
    } else {
        Measure *measure = find_measure();
        EngravingItem *next_el = measure && rtick() == measure->ticks() ? measure->next() : nullptr;
        if (!next_el) {
            next_el = next_element();
        }
        Transaction &tx = score()->transaction_manager()->current_or_dummy_transaction();
        EditClef::undo_change_clef(tx, score(), staff(), next_el, clef_type(), false, this);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Clef::property_default(Pid id) const {
    switch (id) {
        case Pid::CLEF_TYPE_CONCERT: return ClefType::INVALID;
        case Pid::CLEF_TYPE_TRANSPOSING: return ClefType::INVALID;
        case Pid::SHOW_COURTESY: return true;
        case Pid::SMALL: return false;
        case Pid::CLEF_TO_BARLINE_POS: return ClefToBarlinePosition::AUTO;
        case Pid::IS_COURTESY: return false;
        default: return EngravingItem::property_default(id);
    }
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Clef::next_segment_element() { return segment()->first_in_next_segments(staff_idx()); }

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Clef::prev_segment_element() { return segment()->last_in_prev_segments(staff_idx()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

TranslatableString Clef::subtype_user_name() const { return TConv::user_name(clef_type()); }

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void Clef::clear() {
    LayoutData *ldata = mutldata();
    ldata->clear_bbox();
    ldata->sym_id = SymId::NO_SYM;
    Clef *paired_clef = other_clef();
    if (selected() && score()->selection().is_list() && !is_header() && paired_clef) {
        score()->deselect(this);
        score()->select(paired_clef, SelectType::ADD, staff_idx());
    }
}
} // namespace mu::engraving
