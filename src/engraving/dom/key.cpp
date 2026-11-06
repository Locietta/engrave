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
const SymId acc_table[] = {
    SymId::NO_SYM,
    // standard accidentals
    SymId::ACCIDENTAL_TRIPLE_FLAT, SymId::ACCIDENTAL_DOUBLE_FLAT, SymId::ACCIDENTAL_FLAT, SymId::ACCIDENTAL_NATURAL,
    SymId::ACCIDENTAL_SHARP, SymId::ACCIDENTAL_DOUBLE_SHARP, SymId::ACCIDENTAL_TRIPLE_SHARP, SymId::NO_SYM,
    //  natural sharp
    SymId::ACCIDENTAL_NATURAL, SymId::ACCIDENTAL_NATURAL_SHARP, SymId::ACCIDENTAL_SHARP, SymId::NO_SYM,
    //  natural flat
    SymId::ACCIDENTAL_DOUBLE_FLAT, SymId::ACCIDENTAL_NATURAL_FLAT, SymId::ACCIDENTAL_NATURAL, SymId::NO_SYM,
    //  natural sharp sharp
    SymId::ACCIDENTAL_SHARP, SymId::ACCIDENTAL_SHARP_SHARP, SymId::ACCIDENTAL_TRIPLE_SHARP, SymId::NO_SYM,
    // Gould quarter tone
    //  arrow down
    SymId::ACCIDENTAL_FIVE_QUARTER_TONES_FLAT_ARROW_DOWN, SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ARROW_DOWN,
    SymId::ACCIDENTAL_QUARTER_TONE_FLAT_NATURAL_ARROW_DOWN, SymId::ACCIDENTAL_QUARTER_TONE_SHARP_ARROW_DOWN,
    SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_ARROW_DOWN, SymId::ACCIDENTAL_FIVE_QUARTER_TONES_FLAT_ARROW_DOWN, SymId::NO_SYM,
    //  arrow up
    SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ARROW_UP, SymId::ACCIDENTAL_QUARTER_TONE_FLAT_ARROW_UP,
    SymId::ACCIDENTAL_QUARTER_TONE_SHARP_NATURAL_ARROW_UP, SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_ARROW_UP,
    SymId::ACCIDENTAL_FIVE_QUARTER_TONES_SHARP_ARROW_UP, SymId::NO_SYM,
    // Stein-Zimmermann
    //  basic
    SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ZIMMERMANN, SymId::ACCIDENTAL_QUARTER_TONE_FLAT_STEIN,
    SymId::ACCIDENTAL_QUARTER_TONE_SHARP_STEIN, SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_STEIN, SymId::NO_SYM,
    //  narrow (as variant of above)
    SymId::ACCIDENTAL_NARROW_REVERSED_FLAT_AND_FLAT, SymId::ACCIDENTAL_NARROW_REVERSED_FLAT, SymId::ACCIDENTAL_QUARTER_TONE_SHARP_STEIN,
    // Extended Helmholtz-Ellis (just intonation)
    //  one syntonic comma down arrow
    SymId::ACCIDENTAL_DOUBLE_FLAT_ONE_ARROW_DOWN, SymId::ACCIDENTAL_FLAT_ONE_ARROW_DOWN, SymId::ACCIDENTAL_NATURAL_ONE_ARROW_DOWN,
    SymId::ACCIDENTAL_SHARP_ONE_ARROW_DOWN, SymId::ACCIDENTAL_DOUBLE_SHARP_ONE_ARROW_DOWN, SymId::NO_SYM,
    //   one syntonic comma up arrow
    SymId::ACCIDENTAL_DOUBLE_FLAT_ONE_ARROW_UP, SymId::ACCIDENTAL_FLAT_ONE_ARROW_UP, SymId::ACCIDENTAL_NATURAL_ONE_ARROW_UP,
    SymId::ACCIDENTAL_SHARP_ONE_ARROW_UP, SymId::ACCIDENTAL_DOUBLE_SHARP_ONE_ARROW_UP, SymId::NO_SYM,
    //   two syntonic commas down
    SymId::ACCIDENTAL_DOUBLE_FLAT_TWO_ARROWS_DOWN, SymId::ACCIDENTAL_FLAT_TWO_ARROWS_DOWN, SymId::ACCIDENTAL_NATURAL_TWO_ARROWS_DOWN,
    SymId::ACCIDENTAL_SHARP_TWO_ARROWS_DOWN, SymId::ACCIDENTAL_DOUBLE_SHARP_TWO_ARROWS_DOWN, SymId::NO_SYM,
    //   two syntonic commas up
    SymId::ACCIDENTAL_DOUBLE_FLAT_TWO_ARROWS_UP, SymId::ACCIDENTAL_FLAT_TWO_ARROWS_UP, SymId::ACCIDENTAL_NATURAL_TWO_ARROWS_UP,
    SymId::ACCIDENTAL_SHARP_TWO_ARROWS_UP, SymId::ACCIDENTAL_DOUBLE_SHARP_TWO_ARROWS_UP, SymId::NO_SYM,
    //   three syntonic commas down
    SymId::ACCIDENTAL_DOUBLE_FLAT_THREE_ARROWS_DOWN, SymId::ACCIDENTAL_FLAT_THREE_ARROWS_DOWN, SymId::ACCIDENTAL_NATURAL_THREE_ARROWS_DOWN,
    SymId::ACCIDENTAL_SHARP_THREE_ARROWS_DOWN, SymId::ACCIDENTAL_DOUBLE_SHARP_THREE_ARROWS_DOWN, SymId::NO_SYM,
    //   three syntonic commas up
    SymId::ACCIDENTAL_DOUBLE_FLAT_THREE_ARROWS_UP, SymId::ACCIDENTAL_FLAT_THREE_ARROWS_UP, SymId::ACCIDENTAL_NATURAL_THREE_ARROWS_UP,
    SymId::ACCIDENTAL_SHARP_THREE_ARROWS_UP, SymId::ACCIDENTAL_DOUBLE_SHARP_THREE_ARROWS_UP, SymId::NO_SYM,
    //   equal tempered semitone
    SymId::ACCIDENTAL_DOUBLE_FLAT_EQUAL_TEMPERED, SymId::ACCIDENTAL_FLAT_EQUAL_TEMPERED, SymId::ACCIDENTAL_NATURAL_EQUAL_TEMPERED,
    SymId::ACCIDENTAL_SHARP_EQUAL_TEMPERED, SymId::ACCIDENTAL_DOUBLE_SHARP_EQUAL_TEMPERED, SymId::NO_SYM};

//---------------------------------------------------------
//   enforceLimits - ensure _key
//   is within acceptable limits (-7 .. +7).
//   see KeySig::layout()
//---------------------------------------------------------

void KeySigEvent::enforce_limits(bool transposing) {
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

    if (concert_key_ < Key::MIN) {
        concert_key_ = Key::MIN;
        LOGD("key < -7");
    } else if (concert_key_ > Key::MAX) {
        concert_key_ = Key::MAX;
        LOGD("key > 7");
    }
}

//---------------------------------------------------------
//   print
//---------------------------------------------------------

void KeySigEvent::print() const {
    LOGD("<KeySigEvent: ");
    if (!is_valid()) {
        LOGD("invalid>");
    } else {
        if (is_atonal()) {
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

void KeySigEvent::set_key(Key v) {
    key_ = v;
    enforce_limits(true);
}

//---------------------------------------------------------
//   setConcertKey
//---------------------------------------------------------

void KeySigEvent::set_concert_key(Key v) {
    key_ = v;
    concert_key_ = v;
    enforce_limits();
}

//---------------------------------------------------------
//   setCustom
//---------------------------------------------------------

void KeySigEvent::set_custom(bool val) {
    custom_ = val;
    if (key_ == Key::INVALID) {
        concert_key_ = Key::C;
        key_ = Key::C;
    }
}

//---------------------------------------------------------
//   KeySigEvent::operator==
//---------------------------------------------------------

bool KeySigEvent::operator==(const KeySigEvent &e) const {
    if (e.custom_ != custom_ || e.mode_ != mode_ || e.for_instrument_change_ != for_instrument_change_) {
        return false;
    }
    if (custom_ && !is_atonal()) {
        if (e.custom_key_defs_.size() != custom_key_defs_.size()) {
            return false;
        }
        for (size_t i = 0; i < custom_key_defs_.size(); ++i) {
            // check note and sym, don't care xAlt and octAlt
            if (e.custom_key_defs_[i].degree != custom_key_defs_[i].degree || e.custom_key_defs_[i].sym != custom_key_defs_[i].sym) {
                return false;
            }
            // TODO: position matters // does it?
        }
        return true;
    }
    return e.concert_key_ == concert_key_ && e.key_ == key_;
}

//---------------------------------------------------------
//   calculateInterval
//    Calculates the interval to move from one key to another
//---------------------------------------------------------

Interval calculate_interval(Key key1, Key key2) {
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

void KeySigEvent::init_from_subtype(int st) {
    //anatoly-os: legacy code. I don't understand why it is so overcomplicated.
    //Did refactoring to avoid exception on MSVC, but left the same logic.
    struct {
        int key_value : 4;
        int natural_type : 4;
        unsigned custom_type : 16;
        bool custom_value : 1;
        bool invalid : 1;
    } a;

    a.key_value = (st & 0xf);
    a.natural_type = (st >> 4) & 0xf;
    a.custom_type = (st >> 8) & 0xffff;
    a.custom_value = (st >> 24) & 0x1;
    a.invalid = (st >> 25) & 0x1;
    //end of legacy code

    key_ = Key(a.key_value);
    concert_key_ = key_;
    //      _customType     = a._customType;
    custom_ = a.custom_value;
    if (a.invalid) {
        key_ = Key::INVALID;
    }
    enforce_limits();
}

//---------------------------------------------------------
//   accidentalVal
//---------------------------------------------------------

AccidentalVal AccidentalState::accidental_val(int line, bool &error) const {
    if (line < ::mu::engraving::min_acc_state || line >= ::mu::engraving::max_acc_state) {
        error = true;
        return AccidentalVal::NATURAL;
    }
    return AccidentalVal((state_[line] & 0x0f) + int(AccidentalVal::MIN));
}

//---------------------------------------------------------
//   init
//    preset lines list with accidentals for given key
//---------------------------------------------------------

static const int acc_state_natural = int(AccidentalVal::NATURAL) - int(AccidentalVal::MIN);
static const int acc_state_flat = int(AccidentalVal::FLAT) - int(AccidentalVal::MIN);
static const int acc_state_sharp = int(AccidentalVal::SHARP) - int(AccidentalVal::MIN);

void AccidentalState::init(Key key) {
    memset(state_, ::mu::engraving::acc_state_natural, ::mu::engraving::max_acc_state);
    // The numerical value of key tells us the number of sharps (or flats, if negative) in the key signature
    if (key > 0 && key <= Key::MAX) {
        for (int i = 0; i < int(key); ++i) {
            // First F#, then C#, then G#, etc.
            int idx = tpc2step(Tpc::TPC_F_S + i);
            for (int octave = 0; octave < (11 * 7); octave += 7) {
                int j = idx + octave;
                if (j >= ::mu::engraving::max_acc_state) {
                    break;
                }
                state_[j] = ::mu::engraving::acc_state_sharp;
            }
        }
    } else if (key < 0 && key >= Key::MIN) {
        for (int i = 0; i > int(key); --i) {
            // First Bb, then Eb, then Ab, etc.
            int idx = tpc2step(Tpc::TPC_B_B + i);
            for (int octave = 0; octave < (11 * 7); octave += 7) {
                int j = idx + octave;
                if (j >= ::mu::engraving::max_acc_state) {
                    break;
                }
                state_[j] = ::mu::engraving::acc_state_flat;
            }
        }
    }
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void AccidentalState::init(const KeySigEvent &key_sig) {
    init(key_sig.key());
    if (key_sig.custom()) {
        for (const CustDef &d : key_sig.custom_key_defs()) {
            SymId sym = key_sig.sym_in_key(d.sym, d.degree);
            int degree = key_sig.deg_in_key(d.degree);
            AccidentalVal a = sym2accidental_val(sym);
            for (int octave = 0; octave < (11 * 7); octave += 7) {
                int i = degree + octave;
                if (i >= ::mu::engraving::max_acc_state) {
                    break;
                }
                state_[i] = static_cast<uint8_t>(int(a) - int(AccidentalVal::MIN));
            }
        }
    }
    force_restate_accidental_.fill(false);
}

//---------------------------------------------------------
//   accidentalVal
//---------------------------------------------------------

AccidentalVal AccidentalState::accidental_val(int line) const {
    assert(line >= ::mu::engraving::min_acc_state && line < ::mu::engraving::max_acc_state);
    return AccidentalVal((state_[line] & 0x0f) + int(AccidentalVal::MIN));
}

bool AccidentalState::force_restate_accidental(int line) const {
    assert(line >= ::mu::engraving::min_acc_state && line < ::mu::engraving::max_acc_state);
    return force_restate_accidental_[line];
}

//---------------------------------------------------------
//   tieContext
//---------------------------------------------------------

bool AccidentalState::tie_context(int line) const {
    assert(line >= ::mu::engraving::min_acc_state && line < ::mu::engraving::max_acc_state);
    return state_[line] & ::mu::engraving::tie_context_constant;
}

//---------------------------------------------------------
//   setAccidentalVal
//---------------------------------------------------------

void AccidentalState::set_accidental_val(int line, AccidentalVal val, bool tie_context) {
    assert(line >= ::mu::engraving::min_acc_state && line < ::mu::engraving::max_acc_state);
    // casts needed to work around a bug in Xcode 4.2 on Mac, see #25910
    assert(int(val) >= int(AccidentalVal::MIN) && int(val) <= int(AccidentalVal::MAX));
    state_[line] = (int(val) - int(AccidentalVal::MIN)) | (tie_context ? ::mu::engraving::tie_context_constant : 0);
}

void AccidentalState::set_force_restate_accidental(int line, bool force_restate) {
    assert(line >= ::mu::engraving::min_acc_state && line < ::mu::engraving::max_acc_state);
    force_restate_accidental_[line] = force_restate;
}

//---------------------------------------------------------
//   degInKey
//    degree to "absolute degree"
//    first degree in F major is F: 0 -> 3, ...
//---------------------------------------------------------

int KeySigEvent::deg_in_key(int degree) const { return (degree + ((static_cast<int>(key()) + 7) * 4)) % 7; }

//---------------------------------------------------------
//   symInKey
//---------------------------------------------------------

SymId KeySigEvent::sym_in_key(SymId sym, int degree) const {
    degree = deg_in_key(degree);
    int keyval = static_cast<int>(key());
    int acc_index = std::distance(std::begin(::mu::engraving::acc_table),
                                  std::find(std::begin(::mu::engraving::acc_table), std::end(::mu::engraving::acc_table), sym));

    // non transposed key
    if (keyval == 0 || std::abs(keyval) > 7) {
        return sym;
    }

    // sym is not transposable (it is not in table)
    if (!(acc_index < std::end(::mu::engraving::acc_table) - std::begin(::mu::engraving::acc_table))) {
        return SymId::NO_SYM;
    }

    for (int i = 1; i <= std::abs(keyval); ++i) {
        if ((degree * 2 + 2) % 7 == (keyval < 0 ? 8 - i : i) % 7) {
            acc_index += keyval < 0 ? -1 : 1;
        }
    }
    return ::mu::engraving::acc_table[acc_index];
}
} // namespace mu::engraving
