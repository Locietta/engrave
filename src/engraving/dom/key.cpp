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

#include "key.h"

#include "accidental.h"
#include "part.h"
#include "pitchspelling.h"
#include "score.h"
#include "utils.h"

#include "log.h"

using namespace mu;

namespace mu::engraving {
// transposition table for microtonal accidentals
const SymId accTable[] = {
    SymId::noSym,
    // standard accidentals
    SymId::accidentalTripleFlat, SymId::accidentalDoubleFlat, SymId::accidentalFlat, SymId::accidentalNatural, SymId::accidentalSharp,
    SymId::accidentalDoubleSharp, SymId::accidentalTripleSharp, SymId::noSym,
    //  natural sharp
    SymId::accidentalNatural, SymId::accidentalNaturalSharp, SymId::accidentalSharp, SymId::noSym,
    //  natural flat
    SymId::accidentalDoubleFlat, SymId::accidentalNaturalFlat, SymId::accidentalNatural, SymId::noSym,
    //  natural sharp sharp
    SymId::accidentalSharp, SymId::accidentalSharpSharp, SymId::accidentalTripleSharp, SymId::noSym,
    // Gould quarter tone
    //  arrow down
    SymId::accidentalFiveQuarterTonesFlatArrowDown, SymId::accidentalThreeQuarterTonesFlatArrowDown,
    SymId::accidentalQuarterToneFlatNaturalArrowDown, SymId::accidentalQuarterToneSharpArrowDown,
    SymId::accidentalThreeQuarterTonesSharpArrowDown, SymId::accidentalFiveQuarterTonesFlatArrowDown, SymId::noSym,
    //  arrow up
    SymId::accidentalThreeQuarterTonesFlatArrowUp, SymId::accidentalQuarterToneFlatArrowUp, SymId::accidentalQuarterToneSharpNaturalArrowUp,
    SymId::accidentalThreeQuarterTonesSharpArrowUp, SymId::accidentalFiveQuarterTonesSharpArrowUp, SymId::noSym,
    // Stein-Zimmermann
    //  basic
    SymId::accidentalThreeQuarterTonesFlatZimmermann, SymId::accidentalQuarterToneFlatStein, SymId::accidentalQuarterToneSharpStein,
    SymId::accidentalThreeQuarterTonesSharpStein, SymId::noSym,
    //  narrow (as variant of above)
    SymId::accidentalNarrowReversedFlatAndFlat, SymId::accidentalNarrowReversedFlat, SymId::accidentalQuarterToneSharpStein,
    // Extended Helmholtz-Ellis (just intonation)
    //  one syntonic comma down arrow
    SymId::accidentalDoubleFlatOneArrowDown, SymId::accidentalFlatOneArrowDown, SymId::accidentalNaturalOneArrowDown,
    SymId::accidentalSharpOneArrowDown, SymId::accidentalDoubleSharpOneArrowDown, SymId::noSym,
    //   one syntonic comma up arrow
    SymId::accidentalDoubleFlatOneArrowUp, SymId::accidentalFlatOneArrowUp, SymId::accidentalNaturalOneArrowUp,
    SymId::accidentalSharpOneArrowUp, SymId::accidentalDoubleSharpOneArrowUp, SymId::noSym,
    //   two syntonic commas down
    SymId::accidentalDoubleFlatTwoArrowsDown, SymId::accidentalFlatTwoArrowsDown, SymId::accidentalNaturalTwoArrowsDown,
    SymId::accidentalSharpTwoArrowsDown, SymId::accidentalDoubleSharpTwoArrowsDown, SymId::noSym,
    //   two syntonic commas up
    SymId::accidentalDoubleFlatTwoArrowsUp, SymId::accidentalFlatTwoArrowsUp, SymId::accidentalNaturalTwoArrowsUp,
    SymId::accidentalSharpTwoArrowsUp, SymId::accidentalDoubleSharpTwoArrowsUp, SymId::noSym,
    //   three syntonic commas down
    SymId::accidentalDoubleFlatThreeArrowsDown, SymId::accidentalFlatThreeArrowsDown, SymId::accidentalNaturalThreeArrowsDown,
    SymId::accidentalSharpThreeArrowsDown, SymId::accidentalDoubleSharpThreeArrowsDown, SymId::noSym,
    //   three syntonic commas up
    SymId::accidentalDoubleFlatThreeArrowsUp, SymId::accidentalFlatThreeArrowsUp, SymId::accidentalNaturalThreeArrowsUp,
    SymId::accidentalSharpThreeArrowsUp, SymId::accidentalDoubleSharpThreeArrowsUp, SymId::noSym,
    //   equal tempered semitone
    SymId::accidentalDoubleFlatEqualTempered, SymId::accidentalFlatEqualTempered, SymId::accidentalNaturalEqualTempered,
    SymId::accidentalSharpEqualTempered, SymId::accidentalDoubleSharpEqualTempered, SymId::noSym};

//---------------------------------------------------------
//   enforceLimits - ensure _key
//   is within acceptable limits (-7 .. +7).
//   see KeySig::layout()
//---------------------------------------------------------

void KeySigEvent::enforceLimits(bool transposing) {
    if (key_ < Key::MIN) {
        key_ = Key::MIN;
        LOGD("key < -7");
    } else if (key_ > Key::MAX) {
        key_ = Key::MAX;
        LOGD("key > 7");
    }

    if (transposing) {
        return;
    }

    if (concertKey_ < Key::MIN) {
        concertKey_ = Key::MIN;
        LOGD("key < -7");
    } else if (concertKey_ > Key::MAX) {
        concertKey_ = Key::MAX;
        LOGD("key > 7");
    }
}

//---------------------------------------------------------
//   print
//---------------------------------------------------------

void KeySigEvent::print() const {
    LOGD("<KeySigEvent: ");
    if (!isValid()) {
        LOGD("invalid>");
    } else {
        if (isAtonal()) {
            LOGD("atonal>");
        } else if (custom()) {
            LOGD("custom>");
        } else {
            LOGD("accidental %d>", int(key_));
        }
    }
}

//---------------------------------------------------------
//   setKey
//---------------------------------------------------------

void KeySigEvent::setKey(Key v) {
    key_ = v;
    enforceLimits(true);
}

//---------------------------------------------------------
//   setConcertKey
//---------------------------------------------------------

void KeySigEvent::setConcertKey(Key v) {
    key_ = v;
    concertKey_ = v;
    enforceLimits();
}

//---------------------------------------------------------
//   setCustom
//---------------------------------------------------------

void KeySigEvent::setCustom(bool val) {
    custom_ = val;
    if (key_ == Key::INVALID) {
        concertKey_ = Key::C;
        key_ = Key::C;
    }
}

//---------------------------------------------------------
//   KeySigEvent::operator==
//---------------------------------------------------------

bool KeySigEvent::operator==(const KeySigEvent &e) const {
    if (e.custom_ != custom_ || e.mode_ != mode_ || e.forInstrumentChange_ != forInstrumentChange_) {
        return false;
    }
    if (custom_ && !isAtonal()) {
        if (e.customKeyDefs_.size() != customKeyDefs_.size()) {
            return false;
        }
        for (size_t i = 0; i < customKeyDefs_.size(); ++i) {
            // check note and sym, don't care xAlt and octAlt
            if (e.customKeyDefs_[i].degree != customKeyDefs_[i].degree || e.customKeyDefs_[i].sym != customKeyDefs_[i].sym) {
                return false;
            }
            // TODO: position matters // does it?
        }
        return true;
    }
    return e.concertKey_ == concertKey_ && e.key_ == key_;
}

//---------------------------------------------------------
//   calculateInterval
//    Calculates the interval to move from one key to another
//---------------------------------------------------------

Interval calculateInterval(Key key1, Key key2) {
    int chromatic = 7 * ((int) key2 - (int) key1);
    chromatic = chromatic % 12;
    if (chromatic < 0) {
        chromatic += 12;
    }
    return Interval(chromatic);
}

//---------------------------------------------------------
//   initFromSubtype
//    for backward compatibility
//---------------------------------------------------------

void KeySigEvent::initFromSubtype(int st) {
    //anatoly-os: legacy code. I don't understand why it is so overcomplicated.
    //Did refactoring to avoid exception on MSVC, but left the same logic.
    struct {
        int _key : 4;
        int _naturalType : 4;
        unsigned _customType : 16;
        bool _custom : 1;
        bool _invalid : 1;
    } a;

    a._key = (st & 0xf);
    a._naturalType = (st >> 4) & 0xf;
    a._customType = (st >> 8) & 0xffff;
    a._custom = (st >> 24) & 0x1;
    a._invalid = (st >> 25) & 0x1;
    //end of legacy code

    key_ = Key(a._key);
    concertKey_ = key_;
    //      _customType     = a._customType;
    custom_ = a._custom;
    if (a._invalid) {
        key_ = Key::INVALID;
    }
    enforceLimits();
}

//---------------------------------------------------------
//   accidentalVal
//---------------------------------------------------------

AccidentalVal AccidentalState::accidentalVal(int line, bool &error) const {
    if (line < MIN_ACC_STATE || line >= MAX_ACC_STATE) {
        error = true;
        return AccidentalVal::NATURAL;
    }
    return AccidentalVal((state_[line] & 0x0f) + int(AccidentalVal::MIN));
}

//---------------------------------------------------------
//   init
//    preset lines list with accidentals for given key
//---------------------------------------------------------

static const int ACC_STATE_NATURAL = int(AccidentalVal::NATURAL) - int(AccidentalVal::MIN);
static const int ACC_STATE_FLAT = int(AccidentalVal::FLAT) - int(AccidentalVal::MIN);
static const int ACC_STATE_SHARP = int(AccidentalVal::SHARP) - int(AccidentalVal::MIN);

void AccidentalState::init(Key key) {
    memset(state_, ACC_STATE_NATURAL, MAX_ACC_STATE);
    // The numerical value of key tells us the number of sharps (or flats, if negative) in the key signature
    if (key > 0 && key <= Key::MAX) {
        for (int i = 0; i < int(key); ++i) {
            // First F#, then C#, then G#, etc.
            int idx = tpc2step(Tpc::TPC_F_S + i);
            for (int octave = 0; octave < (11 * 7); octave += 7) {
                int j = idx + octave;
                if (j >= MAX_ACC_STATE) {
                    break;
                }
                state_[j] = ACC_STATE_SHARP;
            }
        }
    } else if (key < 0 && key >= Key::MIN) {
        for (int i = 0; i > int(key); --i) {
            // First Bb, then Eb, then Ab, etc.
            int idx = tpc2step(Tpc::TPC_B_B + i);
            for (int octave = 0; octave < (11 * 7); octave += 7) {
                int j = idx + octave;
                if (j >= MAX_ACC_STATE) {
                    break;
                }
                state_[j] = ACC_STATE_FLAT;
            }
        }
    }
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void AccidentalState::init(const KeySigEvent &keySig) {
    init(keySig.key());
    if (keySig.custom()) {
        for (const CustDef &d : keySig.customKeyDefs()) {
            SymId sym = keySig.symInKey(d.sym, d.degree);
            int degree = keySig.degInKey(d.degree);
            AccidentalVal a = sym2accidentalVal(sym);
            for (int octave = 0; octave < (11 * 7); octave += 7) {
                int i = degree + octave;
                if (i >= MAX_ACC_STATE) {
                    break;
                }
                state_[i] = static_cast<uint8_t>(int(a) - int(AccidentalVal::MIN));
            }
        }
    }
    forceRestateAccidental_.fill(false);
}

//---------------------------------------------------------
//   accidentalVal
//---------------------------------------------------------

AccidentalVal AccidentalState::accidentalVal(int line) const {
    assert(line >= MIN_ACC_STATE && line < MAX_ACC_STATE);
    return AccidentalVal((state_[line] & 0x0f) + int(AccidentalVal::MIN));
}

bool AccidentalState::forceRestateAccidental(int line) const {
    assert(line >= MIN_ACC_STATE && line < MAX_ACC_STATE);
    return forceRestateAccidental_[line];
}

//---------------------------------------------------------
//   tieContext
//---------------------------------------------------------

bool AccidentalState::tieContext(int line) const {
    assert(line >= MIN_ACC_STATE && line < MAX_ACC_STATE);
    return state_[line] & TIE_CONTEXT;
}

//---------------------------------------------------------
//   setAccidentalVal
//---------------------------------------------------------

void AccidentalState::setAccidentalVal(int line, AccidentalVal val, bool tieContext) {
    assert(line >= MIN_ACC_STATE && line < MAX_ACC_STATE);
    // casts needed to work around a bug in Xcode 4.2 on Mac, see #25910
    assert(int(val) >= int(AccidentalVal::MIN) && int(val) <= int(AccidentalVal::MAX));
    state_[line] = (int(val) - int(AccidentalVal::MIN)) | (tieContext ? TIE_CONTEXT : 0);
}

void AccidentalState::setForceRestateAccidental(int line, bool forceRestate) {
    assert(line >= MIN_ACC_STATE && line < MAX_ACC_STATE);
    forceRestateAccidental_[line] = forceRestate;
}

//---------------------------------------------------------
//   degInKey
//    degree to "absolute degree"
//    first degree in F major is F: 0 -> 3, ...
//---------------------------------------------------------

int KeySigEvent::degInKey(int degree) const { return (degree + ((static_cast<int>(key()) + 7) * 4)) % 7; }

//---------------------------------------------------------
//   symInKey
//---------------------------------------------------------

SymId KeySigEvent::symInKey(SymId sym, int degree) const {
    degree = degInKey(degree);
    int keyval = static_cast<int>(key());
    int accIndex = std::distance(std::begin(accTable), std::find(std::begin(accTable), std::end(accTable), sym));

    // non transposed key
    if (keyval == 0 || std::abs(keyval) > 7) {
        return sym;
    }

    // sym is not transposable (it is not in table)
    if (!(accIndex < std::end(accTable) - std::begin(accTable))) {
        return SymId::noSym;
    }

    for (int i = 1; i <= std::abs(keyval); ++i) {
        if ((degree * 2 + 2) % 7 == (keyval < 0 ? 8 - i : i) % 7) {
            accIndex += keyval < 0 ? -1 : 1;
        }
    }
    return accTable[accIndex];
}
} // namespace mu::engraving
