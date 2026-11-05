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
#include <array>

#include "../types/types.h"

namespace mu::engraving {
class Score;

enum class AccidentalVal : signed char;

//---------------------------------------------------------
//   KeySym
//    position of one symbol in KeySig
//---------------------------------------------------------

struct KeySym {
    SymId sym = SymId::noSym;
    int line = 0;          // relative line position (first staffline: line == 0, first gap: line == 1, ...)
    Spatium xPos = 0.0_sp; // x position in staff spatium units
};

//---------------------------------------------------------
//   CustDef
//    definition of one symbol in Custom KeySig
//---------------------------------------------------------

struct CustDef {
    int degree = 0; // scale degree
    SymId sym = SymId::noSym;
    Spatium xAlt = 0.0_sp; // x position alteration in spatium units (default symbol position is based on index)
    int octAlt = 0;        // octave alteration
};

//---------------------------------------------------------
//   KeySigEvent
//---------------------------------------------------------

class KeySigEvent {
public:
    KeySigEvent() = default;

    bool operator==(const KeySigEvent &e) const;

    bool operator!=(const KeySigEvent &e) const { return !(*this == e); }

    void setConcertKey(Key v);
    void setKey(Key v);
    void setCustom(bool val);
    void print() const;

    Key concertKey() const { return concertKey_; }

    Key key() const { return key_; }

    KeyMode mode() const { return mode_; }

    void setMode(KeyMode m) { mode_ = m; }

    bool custom() const { return custom_; }

    bool isValid() const { return key_ != Key::INVALID; }

    bool isAtonal() const { return mode_ == KeyMode::NONE; }

    void setForInstrumentChange(bool forInstrumentChange) { forInstrumentChange_ = forInstrumentChange; }

    bool forInstrumentChange() const { return forInstrumentChange_; }

    void initFromSubtype(int);      // for backward compatibility
    int degInKey(int degree) const; // return "absolute degree"
    SymId symInKey(SymId sym, int degree) const;

    std::vector<CustDef> &customKeyDefs() { return customKeyDefs_; }

    const std::vector<CustDef> &customKeyDefs() const { return customKeyDefs_; }

private:

    void enforceLimits(bool transposing = false); // if true, enforce only trnasposing, otherways both

    Key concertKey_ = Key::INVALID; // -7 -> +7
    Key key_ = Key::INVALID;        // actual key, depends on staff transposition
    KeyMode mode_ = KeyMode::UNKNOWN;
    bool custom_ = false;
    bool forInstrumentChange_ = false;
    std::vector<CustDef> customKeyDefs_;
};

//---------------------------------------------------------
//   AccidentalState
///   Contains a state for every absolute staff line.
//---------------------------------------------------------

static const int TIE_CONTEXT = 0x10;
static const int MIN_ACC_STATE = 0;
static const int MAX_ACC_STATE = 75;

class AccidentalState {
public:
    AccidentalState() {}

    void init(Key key);
    void init(const KeySigEvent &);
    AccidentalVal accidentalVal(int line, bool &error) const;
    AccidentalVal accidentalVal(int line) const;
    bool forceRestateAccidental(int line) const;
    bool tieContext(int line) const;
    void setAccidentalVal(int line, AccidentalVal val, bool tieContext = false);
    void setForceRestateAccidental(int line, bool forceRestate);

private:

    uint8_t state_[MAX_ACC_STATE] = {}; // (0 -- 4) | TIE_CONTEXT
    std::array<bool, MAX_ACC_STATE> forceRestateAccidental_;
};

struct Interval;

enum class PreferSharpFlat : char;
extern Interval calculateInterval(Key key1, Key key2);
} // namespace mu::engraving
