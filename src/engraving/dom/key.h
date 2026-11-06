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

#include <array>
#include <vector>

#include "../types/types.h"

namespace mu::engraving {
struct Score;

enum class AccidentalVal : signed char;

//---------------------------------------------------------
//   KeySym
//    position of one symbol in KeySig
//---------------------------------------------------------

struct KeySym {
    SymId sym = SymId::NO_SYM;
    int line = 0;           // relative line position (first staffline: line == 0, first gap: line == 1, ...)
    Spatium x_pos = 0.0_sp; // x position in staff spatium units
};

//---------------------------------------------------------
//   CustDef
//    definition of one symbol in Custom KeySig
//---------------------------------------------------------

struct CustDef {
    int degree = 0; // scale degree
    SymId sym = SymId::NO_SYM;
    Spatium x_alt = 0.0_sp; // x position alteration in spatium units (default symbol position is based on index)
    int oct_alt = 0;        // octave alteration
};

//---------------------------------------------------------
//   KeySigEvent
//---------------------------------------------------------

struct KeySigEvent {
    KeySigEvent() = default;

    bool operator==(const KeySigEvent &e) const;

    bool operator!=(const KeySigEvent &e) const { return !(*this == e); }

    void set_concert_key(Key v);
    void set_key(Key v);
    void set_custom(bool val);
    void print() const;

    Key concert_key() const { return concert_key_; }

    Key key() const { return key_; }

    KeyMode mode() const { return mode_; }

    void set_mode(KeyMode m) { mode_ = m; }

    bool custom() const { return custom_; }

    bool is_valid() const { return key_ != Key::INVALID; }

    bool is_atonal() const { return mode_ == KeyMode::NONE; }

    void set_for_instrument_change(bool for_instrument_change) { for_instrument_change_ = for_instrument_change; }

    bool for_instrument_change() const { return for_instrument_change_; }

    void init_from_subtype(int);      // for backward compatibility
    int deg_in_key(int degree) const; // return "absolute degree"
    SymId sym_in_key(SymId sym, int degree) const;

    std::vector<CustDef> &custom_key_defs() { return custom_key_defs_; }

    const std::vector<CustDef> &custom_key_defs() const { return custom_key_defs_; }

private:

    void enforce_limits(bool transposing = false); // if true, enforce only trnasposing, otherways both

    Key concert_key_ = Key::INVALID; // -7 -> +7
    Key key_ = Key::INVALID;         // actual key, depends on staff transposition
    KeyMode mode_ = KeyMode::UNKNOWN;
    bool custom_ = false;
    bool for_instrument_change_ = false;
    std::vector<CustDef> custom_key_defs_;
};

//---------------------------------------------------------
//   AccidentalState
///   Contains a state for every absolute staff line.
//---------------------------------------------------------

static const int tie_context_constant = 0x10;
static const int min_acc_state = 0;
static const int max_acc_state = 75;

struct AccidentalState {
    AccidentalState() {}

    void init(Key key);
    void init(const KeySigEvent &);
    AccidentalVal accidental_val(int line, bool &error) const;
    AccidentalVal accidental_val(int line) const;
    bool force_restate_accidental(int line) const;
    bool tie_context(int line) const;
    void set_accidental_val(int line, AccidentalVal val, bool input_tie_context = false);
    void set_force_restate_accidental(int line, bool force_restate);

private:

    uint8_t state_[::mu::engraving::max_acc_state] = {}; // (0 -- 4) | TIE_CONTEXT
    std::array<bool, ::mu::engraving::max_acc_state> force_restate_accidental_;
};

struct Interval;

enum class PreferSharpFlat : char;
extern Interval calculate_interval(Key key1, Key key2);
} // namespace mu::engraving
