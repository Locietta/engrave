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

//  This file contains the implementation of an pitch spelling
//  algorithmus from Emilios Cambouropoulos as published in:
//  "Automatic Pitch Spelling: From Numbers to Sharps and Flats"

#include "pitchspelling.h"

#include "key.h"
#include "log.h"
#include "translation.h"
#include "types/typesconv.h"

namespace mu::engraving {
//---------------------------------------------------------
//   tpcIsValid
//---------------------------------------------------------

bool tpc_is_valid(int val) { return val >= Tpc::TPC_MIN && val <= Tpc::TPC_MAX; }

//---------------------------------------------------------
//   step2tpc
//---------------------------------------------------------

int step2tpc(int step, AccidentalVal alter) {
    //    TPC - tonal pitch classes
    //    "line of fifth's" LOF

    static const int spellings[] = {
        // bbb  bb  b   -   #  ##  ###
        -7, 0,  7,  14, 21, 28, 35, // C
        -5, 2,  9,  16, 23, 30, 37, // D
        -3, 4,  11, 18, 25, 32, 39, // E
        -8, -1, 6,  13, 20, 27, 34, // F
        -6, 1,  8,  15, 22, 29, 36, // G
        -4, 3,  10, 17, 24, 31, 38, // A
        -2, 5,  12, 19, 26, 33, 40, // B
    };

    int i = (step * ::mu::engraving::tpcs_per_step) + (int(alter) - int(AccidentalVal::MIN));
    assert(i >= 0 && (i < int(sizeof(spellings) / sizeof(*spellings))));
    return spellings[i];
}

static const int tpc_by_step_and_key[int(Key::NUM_OF)][::mu::engraving::step_delta_octave] = {
    // step          C             D             E             F             G             A             B        Key
    {Tpc::TPC_C_B, Tpc::TPC_D_B, Tpc::TPC_E_B, Tpc::TPC_F_B, Tpc::TPC_G_B, Tpc::TPC_A_B, Tpc::TPC_B_B}, // Cb
    {Tpc::TPC_C_B, Tpc::TPC_D_B, Tpc::TPC_E_B, Tpc::TPC_F, Tpc::TPC_G_B, Tpc::TPC_A_B, Tpc::TPC_B_B},   // Gb
    {Tpc::TPC_C, Tpc::TPC_D_B, Tpc::TPC_E_B, Tpc::TPC_F, TPC_G_B, Tpc::TPC_A_B, Tpc::TPC_B_B},          // Db
    {Tpc::TPC_C, Tpc::TPC_D_B, Tpc::TPC_E_B, Tpc::TPC_F, Tpc::TPC_G, Tpc::TPC_A_B, Tpc::TPC_B_B},       // Ab
    {Tpc::TPC_C, Tpc::TPC_D, Tpc::TPC_E_B, Tpc::TPC_F, Tpc::TPC_G, Tpc::TPC_A_B, Tpc::TPC_B_B},         // Eb
    {Tpc::TPC_C, Tpc::TPC_D, Tpc::TPC_E_B, Tpc::TPC_F, Tpc::TPC_G, Tpc::TPC_A, Tpc::TPC_B_B},           // B
    {Tpc::TPC_C, Tpc::TPC_D, Tpc::TPC_E, Tpc::TPC_F, Tpc::TPC_G, Tpc::TPC_A, Tpc::TPC_B_B},             // F
    {Tpc::TPC_C, Tpc::TPC_D, Tpc::TPC_E, Tpc::TPC_F, Tpc::TPC_G, Tpc::TPC_A, Tpc::TPC_B},               // C
    {Tpc::TPC_C, Tpc::TPC_D, Tpc::TPC_E, Tpc::TPC_F_S, Tpc::TPC_G, Tpc::TPC_A, Tpc::TPC_B},             // G
    {Tpc::TPC_C_S, Tpc::TPC_D, Tpc::TPC_E, Tpc::TPC_F_S, Tpc::TPC_G, Tpc::TPC_A, Tpc::TPC_B},           // D
    {Tpc::TPC_C_S, Tpc::TPC_D, Tpc::TPC_E, Tpc::TPC_F_S, Tpc::TPC_G_S, Tpc::TPC_A, Tpc::TPC_B},         // A
    {Tpc::TPC_C_S, Tpc::TPC_D_S, Tpc::TPC_E, Tpc::TPC_F_S, Tpc::TPC_G_S, Tpc::TPC_A, Tpc::TPC_B},       // E
    {Tpc::TPC_C_S, Tpc::TPC_D_S, Tpc::TPC_E, Tpc::TPC_F_S, Tpc::TPC_G_S, Tpc::TPC_A_S, Tpc::TPC_B},     // H
    {Tpc::TPC_C_S, Tpc::TPC_D_S, Tpc::TPC_E_S, Tpc::TPC_F_S, Tpc::TPC_G_S, Tpc::TPC_A_S, Tpc::TPC_B},   // F#
    {Tpc::TPC_C_S, Tpc::TPC_D_S, Tpc::TPC_E_S, Tpc::TPC_F_S, Tpc::TPC_G_S, Tpc::TPC_A_S, Tpc::TPC_B_S}, // C#
};

int step2tpc_by_key(int step, Key key) {
    while (step < 0) {
        step += ::mu::engraving::step_delta_octave;
    }

    return ::mu::engraving::tpc_by_step_and_key[int(clamp_key(key)) - int(Key::MIN)][step % ::mu::engraving::step_delta_octave];
}

//---------------------------------------------------------
//   tpc2step
//---------------------------------------------------------

int tpc2step(int tpc) {
    // 14 - C
    // 15 % 7 = 1
    //                                            f  c  g  d  a  e  b
    static const int steps[::mu::engraving::step_delta_octave] = {3, 0, 4, 1, 5, 2, 6};
    // TODO: optimize -TCP_MIN
    return steps[(tpc - Tpc::TPC_MIN) % ::mu::engraving::step_delta_octave];
    // without a table, could also be rendered as:
    //      return ((tpc-Tpc::TPC_MIN) * STEP_DELTA_TPC) / STEP_DELTA_OCTAVE + TPC_FIRST_STEP;
}

//---------------------------------------------------------
//   tpc2stepByKey
//---------------------------------------------------------

int tpc2step_by_key(int tpc, Key key, int &alter) {
    alter = tpc2alter_by_key(tpc, key);
    return tpc2step(tpc);
}

//---------------------------------------------------------
//   step2tpc
//---------------------------------------------------------

int step2tpc(const String &step_name, AccidentalVal alter) {
    if (step_name.is_empty()) {
        return Tpc::TPC_INVALID;
    }
    int r;
    switch (step_name.at(0).to_lower().to_ascii()) {
        case 'c': r = 0; break;
        case 'd': r = 1; break;
        case 'e': r = 2; break;
        case 'f': r = 3; break;
        case 'g': r = 4; break;
        case 'a': r = 5; break;
        case 'b': r = 6; break;
        default: return Tpc::TPC_INVALID;
    }
    return step2tpc(r, alter);
}

//---------------------------------------------------------
//   step2deltaPitchByKey
//
// returns the delta pitch above natural C for the given step in the given key
// step: 0 - 6
// key: -7 - +7
//---------------------------------------------------------

static const int pitch_by_step_and_key[int(Key::NUM_OF)][::mu::engraving::step_delta_octave] = {
    // step  C   D   E   F   G   A   B           Key
    {-1, 1, 3, 4, 6, 8, 10}, // Cb
    {-1, 1, 3, 5, 6, 8, 10}, // Gb
    {0, 1, 3, 5, 6, 8, 10},  // Db
    {0, 1, 3, 5, 7, 8, 10},  // Ab
    {0, 2, 3, 5, 7, 8, 10},  // Eb
    {0, 2, 3, 5, 7, 9, 10},  // B
    {0, 2, 4, 5, 7, 9, 10},  // F
    {0, 2, 4, 5, 7, 9, 11},  // C
    {0, 2, 4, 6, 7, 9, 11},  // G
    {1, 2, 4, 6, 7, 9, 11},  // D
    {1, 2, 4, 6, 8, 9, 11},  // A
    {1, 3, 4, 6, 8, 9, 11},  // E
    {1, 3, 4, 6, 8, 10, 11}, // H
    {1, 3, 5, 6, 8, 10, 11}, // F#
    {1, 3, 5, 6, 8, 10, 12}, // C#
};

int step2delta_pitch_by_key(int step, Key key) {
    while (step < ::mu::engraving::min_step) {
        step += ::mu::engraving::step_delta_octave;
    }
    return ::mu::engraving::pitch_by_step_and_key[int(clamp_key(key)) - int(Key::MIN)][step % ::mu::engraving::step_delta_octave];
}

//---------------------------------------------------------
//   tpc2pitch
//---------------------------------------------------------

int tpc2pitch(int tpc) {
    assert(tpc_is_valid(tpc));

    static int pitches[] = {
        //step: F   C   G   D   A   E   B
        2, -3, 4,  -1, 6,  1, 8,  // bbb
        3, -2, 5,  0,  7,  2, 9,  // bb
        4, -1, 6,  1,  8,  3, 10, // b
        5, 0,  7,  2,  9,  4, 11, // -
        6, 1,  8,  3,  10, 5, 12, // #
        7, 2,  9,  4,  11, 6, 13, // ##
        8, 3,  10, 5,  12, 7, 14  // ###
    };
    return pitches[tpc - Tpc::TPC_MIN];
}

//---------------------------------------------------------
//   tpc2alterByKey
//
// returns the alteration (-3 to 3) of a given tpc in the given key
// to understand the formula:
//    in the highest key (C#Maj), each of the first 7 tpcs (Fbb to Bbb; tpc-Tpc::TPC_MIN: 0 to 7)
//          is 3 semitones below its key degree (alter = -3)
//    the second 7 tpcs (Fb to Bb; tpc-Tpc::TPC_MIN: 8 to 13) are 2 semitones below (alter = -2) and so on up to 1
//    thus, for C#Maj:
// (1)      (tpc-Tpc::TPC_MIN) - 0         =  0 to 34 (for tcp-TCP_MIN from 0 to 34)
// (2)      (tpc-Tpc::TPC_MIN) - 0) / 7    =  0 to 4  (for each settuple of tcp's) and finally
// (3)      (tcp-Tpc::TPC_MIN) - 0) / 7 -3 = -3 to 1  (for each settuple of tcp's)
//          where 0 = Key::C_S - Key::MAX
//    for each previous key, the result of (1) increases by 1 and the classes of alter are shifted 1 TPC 'up':
//          F#Maj: Fbb-Ebb => -3, Bbb to Eb => -2 and so on
//          BMaj:  Fbb-Abb => -3, Ebb to Ab => -2 and so on
//          and so on
//    thus, for any 'key', the formula is:
//          ((tcp-Tpc::TPC_MIN) - (key-Key::MAX)) / TCP_DELTA_SEMITONE - 3
//---------------------------------------------------------

int tpc2alter_by_key(int tpc, Key key) {
    return (tpc - int(key) - int(Tpc::TPC_MIN) + int(Key::MAX)) / ::mu::engraving::tpc_delta_semitone - (int(AccidentalVal::MAX) + 1);
}

//---------------------------------------------------------
//   tpc2name
//    return note name
//---------------------------------------------------------

String tpc2name(int tpc, NoteSpellingType note_spelling, NoteCaseType note_case, bool explicit_accidental, bool full) {
    String s;
    String acc;
    tpc2name(tpc, note_spelling, note_case, s, acc, explicit_accidental, full);
    return s + (explicit_accidental ? u" " : u"") + acc;
}

//---------------------------------------------------------
//   tpc2name
//---------------------------------------------------------

void tpc2name(int tpc, NoteSpellingType note_spelling, NoteCaseType note_case, String &s, String &acc, bool explicit_accidental,
              bool full) {
    AccidentalVal acc_val;
    tpc2name(tpc, note_spelling, note_case, s, acc_val);
    switch (acc_val) {
        case AccidentalVal::FLAT3:
            if (explicit_accidental) {
                acc = muse::mtrc("engraving", TConv::user_name(AccidentalVal::FLAT3, full));
            } else if (note_spelling == NoteSpellingType::GERMAN_PURE) {
                switch (tpc) {
                    case TPC_A_BBB: acc = u"sasas"; break;
                    case TPC_E_BBB: acc = u"seses"; break;
                    default: acc = u"eseses";
                }
            } else {
                acc = u"bbb";
            }
            break;
        case AccidentalVal::FLAT2:
            if (explicit_accidental) {
                acc = muse::mtrc("engraving", TConv::user_name(AccidentalVal::FLAT2, full));
            } else if (note_spelling == NoteSpellingType::GERMAN_PURE) {
                switch (tpc) {
                    case TPC_A_BB: acc = u"sas"; break;
                    case TPC_E_BB: acc = u"ses"; break;
                    default: acc = u"eses";
                }
            } else {
                acc = u"bb";
            }
            break;
        case AccidentalVal::FLAT:
            if (explicit_accidental) {
                acc = muse::mtrc("engraving", TConv::user_name(AccidentalVal::FLAT, full));
            } else if (note_spelling == NoteSpellingType::GERMAN_PURE) {
                acc = (tpc == TPC_A_B || tpc == TPC_E_B) ? u"s" : u"es";
            } else {
                acc = u"b";
            }
            break;
        case AccidentalVal::NATURAL: acc = u""; break;
        case AccidentalVal::SHARP:
            if (explicit_accidental) {
                acc = muse::mtrc("engraving", TConv::user_name(AccidentalVal::SHARP, full));
            } else {
                acc = (note_spelling == NoteSpellingType::GERMAN_PURE) ? u"is" : u"#";
            }
            break;
        case AccidentalVal::SHARP2:
            if (explicit_accidental) {
                acc = muse::mtrc("engraving", TConv::user_name(AccidentalVal::SHARP2, full));
            } else {
                acc = (note_spelling == NoteSpellingType::GERMAN_PURE) ? u"isis" : u"##";
            }
            break;
        case AccidentalVal::SHARP3:
            if (explicit_accidental) {
                acc = muse::mtrc("engraving", TConv::user_name(AccidentalVal::SHARP3, full));
            } else {
                acc = (note_spelling == NoteSpellingType::GERMAN_PURE) ? u"isisis" : u"###";
            }
            break;
        default:
            LOGD("tpc2name(%d): acc %d", tpc, static_cast<int>(acc_val));
            acc = u"";
            break;
    }
}

//---------------------------------------------------------
//   tpc2name
//---------------------------------------------------------

void tpc2name(int tpc, NoteSpellingType note_spelling, NoteCaseType note_case, String &s, AccidentalVal &acc) {
    const char names[] = "FCGDAEB";
    const char gnames[] = "FCGDAEH";
    const String snames[] = {u"Fa", u"Do", u"Sol", u"Re", u"La", u"Mi", u"Si"};

    acc = tpc2alter(tpc);
    int idx = (tpc - Tpc::TPC_MIN) % ::mu::engraving::tpc_delta_semitone;
    switch (note_spelling) {
        case NoteSpellingType::GERMAN:
        case NoteSpellingType::GERMAN_PURE:
            s = String(Char::from_ascii(gnames[idx]));
            if (s == "H" && acc == AccidentalVal::FLAT) {
                s = u"B";
                if (note_spelling == NoteSpellingType::GERMAN_PURE) {
                    acc = AccidentalVal::NATURAL;
                }
            }
            break;
        case NoteSpellingType::SOLFEGGIO: s = snames[idx]; break;
        case NoteSpellingType::FRENCH:
            s = snames[idx];
            if (s == u"Re") {
                s = u"Ré";
            }
            break;
        default: s = String(Char::from_ascii(names[idx])); break;
    }
    switch (note_case) {
        case NoteCaseType::LOWER: s = s.to_lower(); break;
        case NoteCaseType::UPPER: s = s.to_upper(); break;
        case NoteCaseType::CAPITAL:
        case NoteCaseType::AUTO:
        default: break;
    }
}

//---------------------------------------------------------
//   tpc2stepName
//---------------------------------------------------------

Char tpc2step_name(int tpc) {
    static const String names = u"FCGDAEB";
    return names.at((tpc - Tpc::TPC_MIN) % 7);
}

String tpc_user_name(int tpc, int pitch, bool explicit_accidental, bool full) {
    String pitch_str = tpc2name(tpc, NoteSpellingType::STANDARD, NoteCaseType::AUTO, explicit_accidental, full);
    if (!explicit_accidental) {
        pitch_str.replace(u"b", u"♭");
        pitch_str.replace(u"#", u"♯");
    }

    const String octave_str = String::number(((pitch - static_cast<int>(tpc2alter(tpc))) / ::mu::engraving::pitch_delta_octave) - 1);

    return pitch_str + octave_str;
}

//---------------------------------------------------------
//   pitch2tpc2
//---------------------------------------------------------

// pitch2tpc2(pitch, false) replaced by pitch2tpc(pitch, Key::C, Prefer::FLATS)
// pitch2tpc2(pitch, true) replaced by pitch2tpc(pitch, Key::C, Prefer::SHARPS)

//---------------------------------------------------------
//   pitch2tpc
//    preferred pitch spelling depending on key
//    key -7 to +7
//
// The value of prefer sets the preferred mix of flats and sharps
// for pitches that are non-diatonic in the key specified, by
// positioning the window along the tpc sequence.
//
// Scale tones are the range shown in [ ].
// A value of 8 (Prefer::FLATS) specifies 5b 2b 6b 3b 7b [4 1 5 2 6 3 7]
// A value of 11 (Prefer::NEAREST) specifies 3b 7b [4 1 5 2 6 3 7] 4# 1# 5#
// A value of 13 (Prefer::SHARPS) specifies [4 1 5 2 6 3 7] 4# 1# 5# 2# 6#
//
// Examples for Prefer::NEAREST (n indicates explicit natural):
// C major will use Eb Bb [F C G D A E B] F# C# G#.
// E major will use Gn Dn [A E B F# C# G# D#] A# E# B#.
// F# major will use An En [B F# C# G# D# A# E#] B# Fx Cx.
// Eb major will use Gb Db [Ab Eb Bb F C G D] An En Bn.
// Gb major will use Bbb Fb [Cb Gb Db Ab Eb Bb F] Cn Gn Dn.
//---------------------------------------------------------

int pitch2tpc(int pitch, Key key, Prefer prefer) { return (pitch * 7 + 26 - (int(prefer) + int(key))) % 12 + (int(prefer) + int(key)); }

//---------------------------------------------------------
//   pitch2absStepByKey
//    absolute step (C0 = 0, D0 = 1, ... C1 = 7, D2 = 8, ...) for a pitch/tpc according to key
//    if pAlter not null, returns in it the alteration with respect to the corresponding key degree (-3 to 3)
//    (for instance, an F in GMaj yields alteration -1 i.e. 1 semitone below corresp. deg. of GMaj which is F#)
//    key: between Key::MIN and Key::MAX
//---------------------------------------------------------

int pitch2abs_step_by_key(int pitch, int tpc, Key key, int &alter) {
    // sanitize input data
    pitch = clamp_pitch_octaved(pitch);
    tpc = clamp_enharmonic(tpc);
    key = clamp_key(key);

    int octave = (pitch - int(tpc2alter(tpc))) / ::mu::engraving::pitch_delta_octave;
    int step = tpc2step(tpc);
    alter = tpc2alter_by_key(tpc, key);
    return octave * ::mu::engraving::step_delta_octave + step;
}

//---------------------------------------------------------
//   absStep2pitchByKey
//    the default pitch for the given absolute step in the given key
//---------------------------------------------------------

int abs_step2pitch_by_key(int step, Key key) {
    // sanitize input data
    if (step < ::mu::engraving::min_step) {
        step += ::mu::engraving::step_delta_octave;
    }
    if (step > ::mu::engraving::max_step) {
        step -= ::mu::engraving::step_delta_octave;
    }
    key = clamp_key(key);

    int octave = step / ::mu::engraving::step_delta_octave;
    int delta_pitch = step2delta_pitch_by_key(step % ::mu::engraving::step_delta_octave, key);
    return octave * ::mu::engraving::pitch_delta_octave + delta_pitch;
}

//---------------------------------------------------------
//   tpc2degree
//    the scale degree of a TPC for a given Key
//---------------------------------------------------------

int tpc2degree(int tpc, Key key) {
    static const String names(u"CDEFGAB");
    static const String scales(u"CGDAEBFCGDAEBFC");
    Char scale = scales.at(int(key) + 7);
    Char step_name = tpc2step_name(tpc);
    return (names.index_of(step_name) - names.index_of(scale) + 28) % 7;
}

//---------------------------------------------------------
//   tpcInterval
///   Finds tpc of a note based on an altered interval
///   from a starting note
//---------------------------------------------------------

int tpc_interval(int start_tpc, int interval, int alter) {
    assert(interval > 0);
    static const int intervals[7] = {//          1  2  3   4  5  6  7
                                     0, 2, 4, -1, 1, 3, 5};

    //ensure that we don't have anything more than double sharp or double flat
    //(I know, breaking some convention, but it's the best we can do for now)
    return clamp_enharmonic(start_tpc + intervals[(interval - 1) % 7] + alter * ::mu::engraving::tpc_delta_semitone);
}

//---------------------------------------------------------
//   step2pitchInterval
///   Finds pitch between notes a specified altered interval away
///
///   For example:
///         step = 3, alter = 0 means major 3rd
///         step = 5, alter = -1 means diminished 5
///         step = 6, alter = 2 means augmented sixth
//---------------------------------------------------------

int step2pitch_interval(int step, int alter) {
    assert(step > 0);
    static const int intervals[7] = {//          1  2  3  4  5  6  7
                                     0, 2, 4, 5, 7, 9, 11};

    return intervals[(step - 1) % 7] + alter;
}

void tpc2_function(int tpc, Key key, String &acc_name, String &step_name) {
    if (key == Key::INVALID) {
        LOGD() << "Invalid key";
        return;
    }
    int step = tpc2degree(tpc, key);
    static const String step_names = u"1234567";
    assert(step < (int) step_names.size());
    step_name = step_names.at(step);

    int alter = tpc2alter_by_key(tpc, key);
    int accidental_no = std::abs(alter);
    acc_name = String();
    bool flat = alter < 0;
    for (int i = 0; i < accidental_no; i++) {
        acc_name.append(flat ? u"b" : u"#");
    }
}

String tpc2_function(int tpc, Key key) {
    String acc_str;
    String step_str;
    tpc2_function(tpc, key, acc_str, step_str);
    return acc_str + step_str;
}

//----------------------------------------------
//   function2Tpc
///   might be temporary, just used to parse nashville notation now
///
//----------------------------------------------

int function2_tpc(const String &s, Key key) {
    size_t idx = 0;
    return function2_tpc(s, key, idx);
}

int function2_tpc(const String &s, Key key, size_t &idx) {
    // TODO - PHV: allow for alternate spellings
    int alter = 0;
    int step;

    if (!s.is_empty() && s.at(0).is_digit()) {
        step = s.at(0).digit_value();
        idx = 1;
    } else if (s.size() > 1) {
        constexpr int num_len = 1;
        size_t acc_idx = s.size() - num_len;
        String acc = s.left(acc_idx);
        String num = s.right(num_len);
        if (num.size() > num_len || num.empty() || !num.at(0).is_digit()) {
            return Tpc::TPC_INVALID;
        }

        step = num.at(0).digit_value();
        idx = num_len;
        if (acc.starts_with(u"bb")) {
            alter = -2;
            idx += 2;
        } else if (acc.starts_with(u"b")) {
            alter = -1;
            idx += 1;
        } else if (acc.starts_with(u"#")) {
            alter = 1;
            idx += 1;
        } else if (acc.starts_with(u"##")) {
            alter = 2;
            idx += 2;
        }
    } else {
        return Tpc::TPC_INVALID;
    }

    int key_tpc = int(key) + 14; //tpc of key (ex. F# major would be Tpc::F_S)
    int tpc = tpc_interval(key_tpc, step, alter);
    return tpc;
}

//---------------------------------------------------------
//   convertNote
//    convert something like "C#" into tpc 21
//---------------------------------------------------------
int convert_note(const String &s, NoteSpellingType note_spelling, NoteCaseType &note_case, size_t &idx) {
    bool use_german = false;
    bool use_solfeggio = false;
    static const int spellings[] = {
        // bb  b   -   #  ##
        0,  7,  14, 21, 28, // C
        2,  9,  16, 23, 30, // D
        4,  11, 18, 25, 32, // E
        -1, 6,  13, 20, 27, // F
        1,  8,  15, 22, 29, // G
        3,  10, 17, 24, 31, // A
        5,  12, 19, 26, 33, // B
    };
    if (s.empty()) {
        return Tpc::TPC_INVALID;
    }
    note_case = s.at(0).is_lower() ? NoteCaseType::LOWER : NoteCaseType::CAPITAL;
    int acci;
    switch (note_spelling) {
        case NoteSpellingType::SOLFEGGIO:
        case NoteSpellingType::FRENCH:
            use_solfeggio = true;
            if (s.starts_with(u"sol", muse::CASE_INSENSITIVE)) {
                acci = 3;
            } else {
                acci = 2;
            }
            break;
        case NoteSpellingType::GERMAN:
        case NoteSpellingType::GERMAN_PURE: use_german = true;
        // fall through
        default: acci = 1;
    }
    idx = acci;
    int alter = 0;
    size_t n = s.size();
    String acc = s.right(n - acci);
    if (!acc.empty()) {
        if (acc.starts_with(u"bb")) {
            alter = -2;
            idx += 2;
        } else if (acc.starts_with(u"b")) {
            alter = -1;
            idx += 1;
        } else if (use_german && acc.starts_with(u"eses")) {
            alter = -2;
            idx += 4;
        } else if (use_german && (acc.starts_with(u"ses") || acc.starts_with(u"sas"))) {
            alter = -2;
            idx += 3;
        } else if (use_german && acc.starts_with(u"es")) {
            alter = -1;
            idx += 2;
        } else if (use_german && acc.starts_with(u"s") && !acc.starts_with(u"su")) {
            alter = -1;
            idx += 1;
        } else if (acc.starts_with(u"##")) {
            alter = 2;
            idx += 2;
        } else if (acc.starts_with(u"x")) {
            alter = 2;
            idx += 1;
        } else if (acc.starts_with(u"#")) {
            alter = 1;
            idx += 1;
        } else if (use_german && acc.starts_with(u"isis")) {
            alter = 2;
            idx += 4;
        } else if (use_german && acc.starts_with(u"is")) {
            alter = 1;
            idx += 2;
        }
    }
    int r;
    if (use_german) {
        switch (s.at(0).to_lower().to_ascii()) {
            case 'c': r = 0; break;
            case 'd': r = 1; break;
            case 'e': r = 2; break;
            case 'f': r = 3; break;
            case 'g': r = 4; break;
            case 'a': r = 5; break;
            case 'h': r = 6; break;
            case 'b':
                if (alter && alter != -1) {
                    return Tpc::TPC_INVALID;
                }
                r = 6;
                alter = -1;
                break;
            default: return Tpc::TPC_INVALID;
        }
    } else if (use_solfeggio) {
        if (s.size() < 2) {
            return Tpc::TPC_INVALID;
        }
        if (s.at(1).is_upper()) {
            note_case = NoteCaseType::UPPER;
        }
        String ss = s.to_lower().left(2);
        if (ss == "do") {
            r = 0;
        } else if (ss == "re" || ss == "ré") {
            r = 1;
        } else if (ss == "mi") {
            r = 2;
        } else if (ss == "fa") {
            r = 3;
        } else if (ss == "so") { // sol, but only check first 2 characters
            r = 4;
        } else if (ss == "la") {
            r = 5;
        } else if (ss == "si") {
            r = 6;
        } else {
            return Tpc::TPC_INVALID;
        }
    } else {
        switch (s.at(0).to_lower().to_ascii()) {
            case 'c': r = 0; break;
            case 'd': r = 1; break;
            case 'e': r = 2; break;
            case 'f': r = 3; break;
            case 'g': r = 4; break;
            case 'a': r = 5; break;
            case 'b': r = 6; break;
            default: return Tpc::TPC_INVALID;
        }
    }
    r = spellings[r * 5 + alter + 2];
    return r;
}

int clamp_enharmonic(int tpc, bool use_double_sharps_flats) {
    while (tpc > (use_double_sharps_flats ? Tpc::TPC_MAX : Tpc::TPC_B_S)) {
        tpc -= ::mu::engraving::tpc_delta_enharmonic;
    }
    while (tpc < (use_double_sharps_flats ? Tpc::TPC_MIN : Tpc::TPC_F_B)) {
        tpc += ::mu::engraving::tpc_delta_enharmonic;
    }
    return tpc;
}

int clamp_pitch(int pitch) { return std::clamp(pitch, ::mu::engraving::min_pitch, ::mu::engraving::max_pitch); }

int clamp_pitch_octaved(int pitch) {
    while (pitch > ::mu::engraving::max_pitch) {
        pitch -= ::mu::engraving::pitch_delta_octave;
    }
    while (pitch < ::mu::engraving::min_pitch) {
        pitch += ::mu::engraving::pitch_delta_octave;
    }
    return pitch;
}

Key clamp_key(Key key, PreferSharpFlat prefer) {
    Key smallest = Key::G_B;
    Key largest = Key::F_S;

    if (prefer != PreferSharpFlat::AUTO) {
        smallest = (prefer == PreferSharpFlat::SHARPS) ? Key::A_B : Key::MIN;
        largest = (prefer == PreferSharpFlat::FLATS) ? Key::E : Key::MAX;
    }

    while (key < smallest) {
        key += Key::DELTA_ENHARMONIC;
    }
    while (key > largest) {
        key -= Key::DELTA_ENHARMONIC;
    }

    return key;
}

int key2_tpc(Key key) { return (int) key + ::mu::engraving::key_to_tpc_offset; }

Key tpc2_key(int tpc) { return Key(tpc - ::mu::engraving::key_to_tpc_offset); }
} // namespace mu::engraving
