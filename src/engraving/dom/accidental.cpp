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

#include "accidental.h"

#include "../editing/editnote.h"
#include "log.h"
#include "note.h"
#include "score.h"
#include "types/symnames.h"
#include "types/translatablestring.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   Acc
//---------------------------------------------------------

struct Acc {
    AccidentalVal offset; // semitone offset
    double cent_offset;
    SymId sym;

    Acc(AccidentalVal o, double o2, SymId s) : offset(o), cent_offset(o2), sym(s) {}
};

// NOTE: keep this in sync with AccidentalType enum in types.h, watch out for isMicrotonal()
static const Acc acc_list[] = {
    Acc(AccidentalVal::NATURAL, 0, SymId::NO_SYM),                 // NONE
    Acc(AccidentalVal::FLAT, 0, SymId::ACCIDENTAL_FLAT),           // FLAT
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_NATURAL),     // NATURAL
    Acc(AccidentalVal::SHARP, 0, SymId::ACCIDENTAL_SHARP),         // SHARP
    Acc(AccidentalVal::SHARP2, 0, SymId::ACCIDENTAL_DOUBLE_SHARP), // SHARP2
    Acc(AccidentalVal::FLAT2, 0, SymId::ACCIDENTAL_DOUBLE_FLAT),   // FLAT2
    Acc(AccidentalVal::SHARP3, 0, SymId::ACCIDENTAL_TRIPLE_SHARP), // SHARP3
    Acc(AccidentalVal::FLAT3, 0, SymId::ACCIDENTAL_TRIPLE_FLAT),   // FLAT3
    Acc(AccidentalVal::FLAT, 0, SymId::ACCIDENTAL_NATURAL_FLAT),   // NATURAL_FLAT
    Acc(AccidentalVal::SHARP, 0, SymId::ACCIDENTAL_NATURAL_SHARP), // NATURAL_SHARP
    Acc(AccidentalVal::SHARP2, 0, SymId::ACCIDENTAL_SHARP_SHARP),  // SHARP_SHARP

    // Gould arrow quartertone
    Acc(AccidentalVal::NATURAL, -50, SymId::ACCIDENTAL_QUARTER_TONE_FLAT_ARROW_UP),           // FLAT_ARROW_UP
    Acc(AccidentalVal::NATURAL, -150, SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ARROW_DOWN), // FLAT_ARROW_DOWN
    Acc(AccidentalVal::NATURAL, 50, SymId::ACCIDENTAL_QUARTER_TONE_SHARP_NATURAL_ARROW_UP),   // NATURAL_ARROW_UP
    Acc(AccidentalVal::NATURAL, -50, SymId::ACCIDENTAL_QUARTER_TONE_FLAT_NATURAL_ARROW_DOWN), // NATURAL_ARROW_DOWN
    Acc(AccidentalVal::NATURAL, 150, SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_ARROW_UP),   // SHARP_ARROW_UP
    Acc(AccidentalVal::NATURAL, 50, SymId::ACCIDENTAL_QUARTER_TONE_SHARP_ARROW_DOWN),         // SHARP_ARROW_DOWN
    Acc(AccidentalVal::NATURAL, 250, SymId::ACCIDENTAL_FIVE_QUARTER_TONES_SHARP_ARROW_UP),    // SHARP2_ARROW_UP
    Acc(AccidentalVal::NATURAL, 150, SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_ARROW_DOWN), // SHARP2_ARROW_DOWN
    Acc(AccidentalVal::NATURAL, -150, SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ARROW_UP),   // FLAT2_ARROW_UP
    Acc(AccidentalVal::NATURAL, -250, SymId::ACCIDENTAL_FIVE_QUARTER_TONES_FLAT_ARROW_DOWN),  // FLAT2_ARROW_DOWN
    Acc(AccidentalVal::NATURAL, -50, SymId::ACCIDENTAL_ARROW_DOWN),                           // ARROW_DOWN
    Acc(AccidentalVal::NATURAL, 50, SymId::ACCIDENTAL_ARROW_UP),                              // ARROW_UP

    // Stein-Zimmermann
    Acc(AccidentalVal::NATURAL, -50, SymId::ACCIDENTAL_QUARTER_TONE_FLAT_STEIN),              // MIRRORED_FLAT
    Acc(AccidentalVal::NATURAL, -150, SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ZIMMERMANN), // MIRRORED_FLAT2
    Acc(AccidentalVal::NATURAL, 50, SymId::ACCIDENTAL_QUARTER_TONE_SHARP_STEIN),              // SHARP_SLASH
    Acc(AccidentalVal::NATURAL, 150, SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_STEIN),      // SHARP_SLASH4

    // Arel-Ezgi-Uzdilek (AEU)
    Acc(AccidentalVal::NATURAL, -89, SymId::ACCIDENTAL_BUYUK_MUCENNEB_FLAT), // FLAT_SLASH2
    Acc(AccidentalVal::NATURAL, -44, SymId::ACCIDENTAL_BAKIYE_FLAT),         // FLAT_SLASH
    Acc(AccidentalVal::NATURAL, 56, SymId::ACCIDENTAL_KUCUK_MUCENNEB_SHARP), // SHARP_SLASH3
    Acc(AccidentalVal::NATURAL, 89, SymId::ACCIDENTAL_BUYUK_MUCENNEB_SHARP), // SHARP_SLASH2

    // Extended Helmholtz-Ellis accidentals (just intonation)
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_FLAT_ONE_ARROW_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_FLAT_ONE_ARROW_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_NATURAL_ONE_ARROW_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_SHARP_ONE_ARROW_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_SHARP_ONE_ARROW_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_FLAT_ONE_ARROW_UP),

    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_FLAT_ONE_ARROW_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_NATURAL_ONE_ARROW_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_SHARP_ONE_ARROW_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_SHARP_ONE_ARROW_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_FLAT_TWO_ARROWS_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_FLAT_TWO_ARROWS_DOWN),

    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_NATURAL_TWO_ARROWS_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_SHARP_TWO_ARROWS_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_SHARP_TWO_ARROWS_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_FLAT_TWO_ARROWS_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_FLAT_TWO_ARROWS_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_NATURAL_TWO_ARROWS_UP),

    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_SHARP_TWO_ARROWS_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_SHARP_TWO_ARROWS_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_FLAT_THREE_ARROWS_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_FLAT_THREE_ARROWS_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_NATURAL_THREE_ARROWS_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_SHARP_THREE_ARROWS_DOWN),

    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_SHARP_THREE_ARROWS_DOWN),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_FLAT_THREE_ARROWS_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_FLAT_THREE_ARROWS_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_NATURAL_THREE_ARROWS_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_SHARP_THREE_ARROWS_UP),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_SHARP_THREE_ARROWS_UP),

    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_LOWER_ONE_SEPTIMAL_COMMA),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_RAISE_ONE_SEPTIMAL_COMMA),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_LOWER_TWO_SEPTIMAL_COMMAS),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_RAISE_TWO_SEPTIMAL_COMMAS),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_LOWER_ONE_UNDECIMAL_QUARTERTONE),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_RAISE_ONE_UNDECIMAL_QUARTERTONE),

    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_LOWER_ONE_TRIDECIMAL_QUARTERTONE),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_RAISE_ONE_TRIDECIMAL_QUARTERTONE),

    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_FLAT_EQUAL_TEMPERED),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_FLAT_EQUAL_TEMPERED),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_NATURAL_EQUAL_TEMPERED),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_SHARP_EQUAL_TEMPERED),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_DOUBLE_SHARP_EQUAL_TEMPERED),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_QUARTER_FLAT_EQUAL_TEMPERED),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_QUARTER_SHARP_EQUAL_TEMPERED),

    Acc(AccidentalVal::NATURAL, -6.8, SymId::ACCIDENTAL_COMBINING_LOWER17_SCHISMA),
    Acc(AccidentalVal::NATURAL, 6.8, SymId::ACCIDENTAL_COMBINING_RAISE17_SCHISMA),
    Acc(AccidentalVal::NATURAL, -3.4, SymId::ACCIDENTAL_COMBINING_LOWER19_SCHISMA),
    Acc(AccidentalVal::NATURAL, 3.4, SymId::ACCIDENTAL_COMBINING_RAISE19_SCHISMA),
    Acc(AccidentalVal::NATURAL, -16.5, SymId::ACCIDENTAL_COMBINING_LOWER23_LIMIT29_LIMIT_COMMA),
    Acc(AccidentalVal::NATURAL, 16.5, SymId::ACCIDENTAL_COMBINING_RAISE23_LIMIT29_LIMIT_COMMA),
    Acc(AccidentalVal::NATURAL, -1.7, SymId::ACCIDENTAL_COMBINING_LOWER31_SCHISMA),
    Acc(AccidentalVal::NATURAL, 1.7, SymId::ACCIDENTAL_COMBINING_RAISE31_SCHISMA),
    Acc(AccidentalVal::NATURAL, -10.9, SymId::ACCIDENTAL_COMBINING_LOWER53_LIMIT_COMMA),
    Acc(AccidentalVal::NATURAL, 10.9, SymId::ACCIDENTAL_COMBINING_RAISE53_LIMIT_COMMA),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_ENHARMONIC_ALMOST_EQUAL_TO),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_ENHARMONIC_EQUALS),
    Acc(AccidentalVal::NATURAL, 0, SymId::ACCIDENTAL_ENHARMONIC_TILDE),

    // Persian
    Acc(AccidentalVal::NATURAL, 33, SymId::ACCIDENTAL_SORI),   // SORI
    Acc(AccidentalVal::NATURAL, -67, SymId::ACCIDENTAL_KORON), // KORON

    // Wyschnegradsky
    Acc(AccidentalVal::NATURAL, -167, SymId::ACCIDENTAL_WYSCHNEGRADSKY10_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 167, SymId::ACCIDENTAL_WYSCHNEGRADSKY10_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -183, SymId::ACCIDENTAL_WYSCHNEGRADSKY11_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 183, SymId::ACCIDENTAL_WYSCHNEGRADSKY11_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -17, SymId::ACCIDENTAL_WYSCHNEGRADSKY1_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 17, SymId::ACCIDENTAL_WYSCHNEGRADSKY1_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -33, SymId::ACCIDENTAL_WYSCHNEGRADSKY2_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 33, SymId::ACCIDENTAL_WYSCHNEGRADSKY2_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -50, SymId::ACCIDENTAL_WYSCHNEGRADSKY3_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 50, SymId::ACCIDENTAL_WYSCHNEGRADSKY3_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -67, SymId::ACCIDENTAL_WYSCHNEGRADSKY4_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 67, SymId::ACCIDENTAL_WYSCHNEGRADSKY4_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -83, SymId::ACCIDENTAL_WYSCHNEGRADSKY5_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 83, SymId::ACCIDENTAL_WYSCHNEGRADSKY5_TWELFTHS_SHARP),
    Acc(AccidentalVal::FLAT, 0, SymId::ACCIDENTAL_WYSCHNEGRADSKY6_TWELFTHS_FLAT),
    Acc(AccidentalVal::SHARP, 0, SymId::ACCIDENTAL_WYSCHNEGRADSKY6_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -117, SymId::ACCIDENTAL_WYSCHNEGRADSKY7_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 117, SymId::ACCIDENTAL_WYSCHNEGRADSKY7_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -133, SymId::ACCIDENTAL_WYSCHNEGRADSKY8_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 133, SymId::ACCIDENTAL_WYSCHNEGRADSKY8_TWELFTHS_SHARP),
    Acc(AccidentalVal::NATURAL, -150, SymId::ACCIDENTAL_WYSCHNEGRADSKY9_TWELFTHS_FLAT),
    Acc(AccidentalVal::NATURAL, 150, SymId::ACCIDENTAL_WYSCHNEGRADSKY9_TWELFTHS_SHARP),

    // the most important (Spartan) Sagittal accidentals
    Acc(AccidentalVal::NATURAL, -17, SymId::ACC_SAGITTAL5V7_KLEISMA_DOWN),
    Acc(AccidentalVal::NATURAL, 17, SymId::ACC_SAGITTAL5V7_KLEISMA_UP),
    Acc(AccidentalVal::NATURAL, -17, SymId::ACC_SAGITTAL5_COMMA_DOWN),
    Acc(AccidentalVal::NATURAL, 17, SymId::ACC_SAGITTAL5_COMMA_UP),
    Acc(AccidentalVal::NATURAL, -33, SymId::ACC_SAGITTAL7_COMMA_DOWN),
    Acc(AccidentalVal::NATURAL, 33, SymId::ACC_SAGITTAL7_COMMA_UP),
    Acc(AccidentalVal::NATURAL, -33, SymId::ACC_SAGITTAL25_SMALL_DIESIS_DOWN),
    Acc(AccidentalVal::NATURAL, 33, SymId::ACC_SAGITTAL25_SMALL_DIESIS_UP),
    Acc(AccidentalVal::NATURAL, -50, SymId::ACC_SAGITTAL35_MEDIUM_DIESIS_DOWN),
    Acc(AccidentalVal::NATURAL, 50, SymId::ACC_SAGITTAL35_MEDIUM_DIESIS_UP),
    Acc(AccidentalVal::NATURAL, -50, SymId::ACC_SAGITTAL11_MEDIUM_DIESIS_DOWN),
    Acc(AccidentalVal::NATURAL, 50, SymId::ACC_SAGITTAL11_MEDIUM_DIESIS_UP),
    Acc(AccidentalVal::NATURAL, -50, SymId::ACC_SAGITTAL11_LARGE_DIESIS_DOWN),
    Acc(AccidentalVal::NATURAL, 50, SymId::ACC_SAGITTAL11_LARGE_DIESIS_UP),
    Acc(AccidentalVal::NATURAL, -50, SymId::ACC_SAGITTAL35_LARGE_DIESIS_DOWN),
    Acc(AccidentalVal::NATURAL, 50, SymId::ACC_SAGITTAL35_LARGE_DIESIS_UP),
    Acc(AccidentalVal::NATURAL, -67, SymId::ACC_SAGITTAL_FLAT25_S_UP),
    Acc(AccidentalVal::NATURAL, 67, SymId::ACC_SAGITTAL_SHARP25_S_DOWN),
    Acc(AccidentalVal::NATURAL, -67, SymId::ACC_SAGITTAL_FLAT7_C_UP),
    Acc(AccidentalVal::NATURAL, 67, SymId::ACC_SAGITTAL_SHARP7_C_DOWN),
    Acc(AccidentalVal::NATURAL, -83, SymId::ACC_SAGITTAL_FLAT5_C_UP),
    Acc(AccidentalVal::NATURAL, 83, SymId::ACC_SAGITTAL_SHARP5_C_DOWN),
    Acc(AccidentalVal::NATURAL, -83, SymId::ACC_SAGITTAL_FLAT5V7K_UP),
    Acc(AccidentalVal::NATURAL, 83, SymId::ACC_SAGITTAL_SHARP5V7K_DOWN),
    Acc(AccidentalVal::NATURAL, -100, SymId::ACC_SAGITTAL_FLAT),
    Acc(AccidentalVal::NATURAL, 100, SymId::ACC_SAGITTAL_SHARP),

    // Turkish folk music accidentals
    Acc(AccidentalVal::NATURAL, -22.2, SymId::ACCIDENTAL1_COMMA_FLAT),
    Acc(AccidentalVal::NATURAL, 22.2, SymId::ACCIDENTAL1_COMMA_SHARP),
    Acc(AccidentalVal::NATURAL, -44.4, SymId::ACCIDENTAL2_COMMA_FLAT),
    Acc(AccidentalVal::NATURAL, 44.4, SymId::ACCIDENTAL2_COMMA_SHARP),
    Acc(AccidentalVal::NATURAL, -66.7, SymId::ACCIDENTAL3_COMMA_FLAT),
    Acc(AccidentalVal::NATURAL, 66.7, SymId::ACCIDENTAL3_COMMA_SHARP),
    Acc(AccidentalVal::NATURAL, -88.9, SymId::ACCIDENTAL4_COMMA_FLAT),
    //Acc(AccidentalVal::NATURAL,  88.9, SymId::accidentalSharp),
    Acc(AccidentalVal::NATURAL, 111.1, SymId::ACCIDENTAL5_COMMA_SHARP),
};

//---------------------------------------------------------
//   sym2accidentalVal
//---------------------------------------------------------

AccidentalVal sym2accidental_val(SymId id) {
    for (const Acc &a : ::mu::engraving::acc_list) {
        if (a.sym == id) {
            return a.offset;
        }
    }
    return AccidentalVal::NATURAL;
}

//---------------------------------------------------------
//   Accidental
//---------------------------------------------------------

Accidental::Accidental(DummyParentOr<EngravingItem> parent) : EngravingItem(ElementType::ACCIDENTAL, parent, ElementFlag::MOVABLE) {}

Note *Accidental::note() const {
    EngravingObject *owner = ownership_parent();
    return owner && owner->is_note() ? to_note(owner) : nullptr;
}

//---------------------------------------------------------
//   subTypeUserName
//---------------------------------------------------------

TranslatableString Accidental::subtype_user_name() const { return SymNames::user_name_for_sym_id(sym_id()); }

//---------------------------------------------------------
//   symbol
//---------------------------------------------------------

SymId Accidental::sym_id() const { return ::mu::engraving::acc_list[int(accidental_type())].sym; }

bool Accidental::parent_note_has_parentheses() const {
    const EngravingObject *owner = ownership_parent();
    return owner && owner->is_note() && to_note(owner)->parenthesis_info();
}

//---------------------------------------------------------
//   subtype2value
//    returns the resulting pitch offset
//---------------------------------------------------------

AccidentalVal Accidental::subtype2value(AccidentalType st) { return ::mu::engraving::acc_list[int(st)].offset; }

//---------------------------------------------------------
//   subtype2name
//---------------------------------------------------------

AsciiStringView Accidental::subtype2name(AccidentalType st) { return SymNames::name_for_sym_id(::mu::engraving::acc_list[int(st)].sym); }

//---------------------------------------------------------
//   subtype2symbol
//---------------------------------------------------------

SymId Accidental::subtype2symbol(AccidentalType st) { return ::mu::engraving::acc_list[int(st)].sym; }

//---------------------------------------------------------
//   subtype2centoffset
//---------------------------------------------------------

double Accidental::subtype2cent_offset(AccidentalType st) { return ::mu::engraving::acc_list[int(st)].cent_offset; }

AccidentalType Accidental::cent_offset2_subtype(double cent_offset) {
    for (int i = 0; i < static_cast<int>(AccidentalType::END); ++i) {
        if (muse::real_is_equal(::mu::engraving::acc_list[i].cent_offset, cent_offset)) {
            return static_cast<AccidentalType>(i);
        }
    }

    return AccidentalType::NONE;
}

AccidentalType Accidental::value2_microtonal_subtype(AccidentalVal val, int quarter_off) {
    if (quarter_off == 0) {
        return value2subtype(val);
    }

    if (quarter_off < -1 || quarter_off > 1) {
        // TODO: more general implementation
        return value2subtype(val);
    }

    switch (val) {
        case AccidentalVal::NATURAL: return quarter_off == 1 ? AccidentalType::SHARP_ARROW_DOWN : AccidentalType::FLAT_ARROW_UP;
        case AccidentalVal::FLAT: return quarter_off == 1 ? AccidentalType::FLAT_ARROW_UP : AccidentalType::FLAT_ARROW_DOWN;
        case AccidentalVal::SHARP: return quarter_off == 1 ? AccidentalType::SHARP_ARROW_UP : AccidentalType::SHARP_ARROW_DOWN;
        case AccidentalVal::FLAT2: return quarter_off == 1 ? AccidentalType::FLAT2_ARROW_UP : AccidentalType::FLAT2_ARROW_DOWN;
        case AccidentalVal::SHARP2: return quarter_off == 1 ? AccidentalType::SHARP2_ARROW_UP : AccidentalType::SHARP2_ARROW_DOWN;
        default: return value2subtype(val);
    }
}

int Accidental::line() const {
    Note *n = note();
    return n ? n->line() : 0;
}

//---------------------------------------------------------
//   name2subtype
//---------------------------------------------------------

AccidentalType Accidental::name2subtype(const AsciiStringView &tag) {
    SymId sym_id = SymNames::sym_id_by_name(tag);
    if (sym_id == SymId::NO_SYM) {
        // LOGD("no symbol found");
    } else {
        int i = 0;
        for (const Acc &acc : ::mu::engraving::acc_list) {
            if (acc.sym == sym_id) {
                return AccidentalType(i);
            }
            ++i;
        }
    }
    return AccidentalType::NONE;
}

//---------------------------------------------------------
//   setSubtype
//---------------------------------------------------------

void Accidental::set_subtype(const AsciiStringView &tag) { set_accidental_type(name2subtype(tag)); }

void Accidental::set_accidental_type(AccidentalType t) {
    accidental_type_ = t;
    if (note()) {
        note()->set_cent_offset(Accidental::subtype2cent_offset(t));
    }
}

void Accidental::compute_mag() {
    double m = layout_parent() ? layout_parent()->mag() : 1.0;
    if (is_small()) {
        m *= style().style_d(Sid::SMALL_NOTE_MAG);
    }
    mutldata()->set_mag(m);
}

//---------------------------------------------------------
//   value2subtype
//---------------------------------------------------------

AccidentalType Accidental::value2subtype(AccidentalVal v) {
    switch (v) {
        case AccidentalVal::NATURAL: return AccidentalType::NONE;
        case AccidentalVal::SHARP: return AccidentalType::SHARP;
        case AccidentalVal::SHARP2: return AccidentalType::SHARP2;
        case AccidentalVal::SHARP3: return AccidentalType::SHARP3;
        case AccidentalVal::FLAT: return AccidentalType::FLAT;
        case AccidentalVal::FLAT2: return AccidentalType::FLAT2;
        case AccidentalVal::FLAT3: return AccidentalType::FLAT3;
        default: ASSERT_X(u"value2subtype: illegal accidental val: " + String::number(int(v)));
    }
    return AccidentalType::NONE;
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool Accidental::accept_drop(EditData &data) const {
    const EngravingItem *e = data.drop_element;

    if (e->is_accidental()) {
        return note();
    }

    return false;
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Accidental::drop(Transaction &, EditData &data) {
    EngravingItem *e = data.drop_element;
    switch (e->type()) {
        case ElementType::ACCIDENTAL: EditNote::change_accidental(score(), note(), to_accidental(e)->accidental_type()); break;

        default: break;
    }
    delete e;
    return nullptr;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Accidental::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::ACCIDENTAL_TYPE: return int(accidental_type_);
        case Pid::SMALL: return is_small_;
        case Pid::ACCIDENTAL_BRACKET: return int(bracket());
        case Pid::ACCIDENTAL_ROLE: return role();
        case Pid::ACCIDENTAL_STACKING_ORDER_OFFSET: return stacking_order_offset();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Accidental::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::ACCIDENTAL_TYPE: return int(AccidentalType::NONE);
        case Pid::SMALL: return false;
        case Pid::ACCIDENTAL_BRACKET: return int(AccidentalBracket::NONE);
        case Pid::ACCIDENTAL_ROLE: return AccidentalRole::AUTO;
        case Pid::ACCIDENTAL_STACKING_ORDER_OFFSET: return 0;
        default: return EngravingItem::property_default(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Accidental::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::ACCIDENTAL_TYPE: set_accidental_type(AccidentalType(v.to_int())); break;
        case Pid::SMALL: is_small_ = v.to_bool(); break;
        case Pid::ACCIDENTAL_BRACKET: bracket_ = AccidentalBracket(v.to_int()); break;
        case Pid::ACCIDENTAL_ROLE: role_ = v.value<AccidentalRole>(); break;
        case Pid::ACCIDENTAL_STACKING_ORDER_OFFSET: set_stacking_order_offset(v.to_int()); break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

} // namespace mu::engraving
