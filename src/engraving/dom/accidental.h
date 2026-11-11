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

#include <vector>

#include "engravingitem.h"

namespace mu::engraving {
struct Factory;
struct Note;
struct Transaction;
enum class AccidentalVal : signed char;

// NOTE: keep this in sync with accList array in accidentals.cpp
enum class AccidentalType : unsigned char {
    ///.\{
    NONE,
    FLAT,
    NATURAL,
    SHARP,
    SHARP2,
    FLAT2,
    SHARP3,
    FLAT3,
    NATURAL_FLAT,
    NATURAL_SHARP,
    SHARP_SHARP,

    // Gould arrow quartertone
    FLAT_ARROW_UP,
    FLAT_ARROW_DOWN,
    NATURAL_ARROW_UP,
    NATURAL_ARROW_DOWN,
    SHARP_ARROW_UP,
    SHARP_ARROW_DOWN,
    SHARP2_ARROW_UP,
    SHARP2_ARROW_DOWN,
    FLAT2_ARROW_UP,
    FLAT2_ARROW_DOWN,
    ARROW_DOWN,
    ARROW_UP,

    // Stein-Zimmermann
    MIRRORED_FLAT,
    MIRRORED_FLAT2,
    SHARP_SLASH,
    SHARP_SLASH4,

    // Arel-Ezgi-Uzdilek (AEU)
    FLAT_SLASH2,
    FLAT_SLASH,
    SHARP_SLASH3,
    SHARP_SLASH2,

    // Extended Helmholtz-Ellis accidentals (just intonation)
    DOUBLE_FLAT_ONE_ARROW_DOWN,
    FLAT_ONE_ARROW_DOWN,
    NATURAL_ONE_ARROW_DOWN,
    SHARP_ONE_ARROW_DOWN,
    DOUBLE_SHARP_ONE_ARROW_DOWN,
    DOUBLE_FLAT_ONE_ARROW_UP,

    FLAT_ONE_ARROW_UP,
    NATURAL_ONE_ARROW_UP,
    SHARP_ONE_ARROW_UP,
    DOUBLE_SHARP_ONE_ARROW_UP,
    DOUBLE_FLAT_TWO_ARROWS_DOWN,
    FLAT_TWO_ARROWS_DOWN,

    NATURAL_TWO_ARROWS_DOWN,
    SHARP_TWO_ARROWS_DOWN,
    DOUBLE_SHARP_TWO_ARROWS_DOWN,
    DOUBLE_FLAT_TWO_ARROWS_UP,
    FLAT_TWO_ARROWS_UP,
    NATURAL_TWO_ARROWS_UP,

    SHARP_TWO_ARROWS_UP,
    DOUBLE_SHARP_TWO_ARROWS_UP,
    DOUBLE_FLAT_THREE_ARROWS_DOWN,
    FLAT_THREE_ARROWS_DOWN,
    NATURAL_THREE_ARROWS_DOWN,
    SHARP_THREE_ARROWS_DOWN,

    DOUBLE_SHARP_THREE_ARROWS_DOWN,
    DOUBLE_FLAT_THREE_ARROWS_UP,
    FLAT_THREE_ARROWS_UP,
    NATURAL_THREE_ARROWS_UP,
    SHARP_THREE_ARROWS_UP,
    DOUBLE_SHARP_THREE_ARROWS_UP,

    LOWER_ONE_SEPTIMAL_COMMA,
    RAISE_ONE_SEPTIMAL_COMMA,
    LOWER_TWO_SEPTIMAL_COMMAS,
    RAISE_TWO_SEPTIMAL_COMMAS,
    LOWER_ONE_UNDECIMAL_QUARTERTONE,
    RAISE_ONE_UNDECIMAL_QUARTERTONE,

    LOWER_ONE_TRIDECIMAL_QUARTERTONE,
    RAISE_ONE_TRIDECIMAL_QUARTERTONE,

    DOUBLE_FLAT_EQUAL_TEMPERED,
    FLAT_EQUAL_TEMPERED,
    NATURAL_EQUAL_TEMPERED,
    SHARP_EQUAL_TEMPERED,
    DOUBLE_SHARP_EQUAL_TEMPERED,
    QUARTER_FLAT_EQUAL_TEMPERED,
    QUARTER_SHARP_EQUAL_TEMPERED,

    FLAT_17,
    SHARP_17,
    FLAT_19,
    SHARP_19,
    FLAT_23,
    SHARP_23,
    FLAT_31,
    SHARP_31,
    FLAT_53,
    SHARP_53,
    EQUALS_ALMOST,
    EQUALS,
    TILDE,

    // Persian
    SORI,
    KORON,

    // Wyschnegradsky
    TEN_TWELFTH_FLAT,
    TEN_TWELFTH_SHARP,
    ELEVEN_TWELFTH_FLAT,
    ELEVEN_TWELFTH_SHARP,
    ONE_TWELFTH_FLAT,
    ONE_TWELFTH_SHARP,
    TWO_TWELFTH_FLAT,
    TWO_TWELFTH_SHARP,
    THREE_TWELFTH_FLAT,
    THREE_TWELFTH_SHARP,
    FOUR_TWELFTH_FLAT,
    FOUR_TWELFTH_SHARP,
    FIVE_TWELFTH_FLAT,
    FIVE_TWELFTH_SHARP,
    SIX_TWELFTH_FLAT,
    SIX_TWELFTH_SHARP,
    SEVEN_TWELFTH_FLAT,
    SEVEN_TWELFTH_SHARP,
    EIGHT_TWELFTH_FLAT,
    EIGHT_TWELFTH_SHARP,
    NINE_TWELFTH_FLAT,
    NINE_TWELFTH_SHARP,

    // (Spartan) Sagittal
    SAGITTAL_5V7KD,
    SAGITTAL_5V7KU,
    SAGITTAL_5CD,
    SAGITTAL_5CU,
    SAGITTAL_7CD,
    SAGITTAL_7CU,
    SAGITTAL_25SDD,
    SAGITTAL_25SDU,
    SAGITTAL_35MDD,
    SAGITTAL_35MDU,
    SAGITTAL_11MDD,
    SAGITTAL_11MDU,
    SAGITTAL_11LDD,
    SAGITTAL_11LDU,
    SAGITTAL_35LDD,
    SAGITTAL_35LDU,
    SAGITTAL_FLAT25SU,
    SAGITTAL_SHARP25SD,
    SAGITTAL_FLAT7CU,
    SAGITTAL_SHARP7CD,
    SAGITTAL_FLAT5CU,
    SAGITTAL_SHARP5CD,
    SAGITTAL_FLAT5V7KU,
    SAGITTAL_SHARP5V7KD,
    SAGITTAL_FLAT,
    SAGITTAL_SHARP,

    // Turkish folk music
    ONE_COMMA_FLAT,
    ONE_COMMA_SHARP,
    TWO_COMMA_FLAT,
    TWO_COMMA_SHARP,
    THREE_COMMA_FLAT,
    THREE_COMMA_SHARP,
    FOUR_COMMA_FLAT,
    //FOUR_COMMA_SHARP,
    FIVE_COMMA_SHARP,

    END
    ///\}
};

//---------------------------------------------------------
//   AccidentalBracket
//---------------------------------------------------------

enum class AccidentalBracket : char {
    NONE,
    PARENTHESIS,
    BRACKET,
    BRACE, //! Deprecated; removed from the UI and kept here only for compatibility purposes
};

//---------------------------------------------------------
//   @@ Accidental
//   @P role        enum  (Accidental.AUTO, .USER) (read only)
//   @P isSmall     bool
//---------------------------------------------------------

struct Accidental final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::ACCIDENTAL)

public:

    Accidental *clone() const override { return new Accidental(*this); }

    Note *note() const;

    TranslatableString subtype_user_name() const override;
    void set_subtype(const AsciiStringView &s);

    int subtype() const override { return (int) accidental_type_; }

    void set_accidental_type(AccidentalType t);

    AccidentalType accidental_type() const { return accidental_type_; }

    void set_role(AccidentalRole r) { role_ = r; }

    AccidentalRole role() const { return role_; }

    AccidentalBracket bracket() const { return bracket_; }

    void set_bracket(AccidentalBracket val) { bracket_ = val; }

    bool parent_note_has_parentheses() const;

    bool is_small() const { return is_small_; }

    void set_small(bool val) { is_small_ = val; }

    SymId sym_id() const;

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;

    static AccidentalVal subtype2value(AccidentalType); // return effective pitch offset
    static SymId subtype2symbol(AccidentalType);
    static AsciiStringView subtype2name(AccidentalType);
    static AccidentalType value2subtype(AccidentalVal);
    static AccidentalType value2_microtonal_subtype(AccidentalVal val, int quarter_off);
    static AccidentalType name2subtype(const AsciiStringView &);

    static bool is_microtonal(AccidentalType t) { return t > AccidentalType::FLAT3; }

    static double subtype2cent_offset(AccidentalType);
    static AccidentalType cent_offset2_subtype(double cent_offset);

    int stacking_order() const { return ldata()->stacking_number + stacking_order_offset_; }

    int line() const;

    void compute_mag();

    struct LayoutData : public EngravingItem::LayoutData {
        struct Sym {
            SymId sym;
            double x;
            double y;

            Sym(SymId input_sym, double input_x, double input_y) : sym(input_sym), x(input_x), y(input_y) {}
        };

        std::vector<Sym> syms;

        ld_field<int> stacking_number = {"[Accidental] stackingNumber", 0};
        ld_field<int> vertical_subgroup = {"[Accidental] verticalSubgroup", 0};
        ld_field<int> column = {"[Accidental] column", 0};
        ld_field<std::vector<Accidental *>> octaves = {"[Accidental] octaves", std::vector<Accidental *>{}};
        ld_field<std::vector<Accidental *>> seconds = {"[Accidental] seconds", std::vector<Accidental *>{}};

        bool is_valid() const override { return EngravingItem::LayoutData::is_valid() && !syms.empty(); }
    };
    DECLARE_LAYOUTDATA_METHODS(Accidental)

    int stacking_order_offset() const { return stacking_order_offset_; }

    void set_stacking_order_offset(int v) { stacking_order_offset_ = v; }

private:

    friend struct Factory;

    Accidental(DummyParentOr<EngravingItem> parent);

    AccidentalType accidental_type_ = AccidentalType::NONE;
    AccidentalBracket bracket_ = AccidentalBracket::NONE;
    AccidentalRole role_ = AccidentalRole::AUTO;
    bool is_small_ = false;
    int stacking_order_offset_ = 0;
};

extern AccidentalVal sym2accidental_val(SymId id);
} // namespace mu::engraving
