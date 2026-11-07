/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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
#include "utils.h"

#include "translation.h"

using namespace muse;

[[maybe_unused]]
static constexpr const char *note_names_with_octaves[] = {
    // octave -1
    MUSE_TRANSLATE_NOOP("global/pitchName", "C-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B-1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯-1"),
    // octave 0
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B0"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯0"),
    // octave 1
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B1"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯1"),
    // octave 2
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B2"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯2"),
    // octave 3
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B3"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯3"),
    // octave 4
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B4"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯4"),
    // octave 5
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B5"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯5"),
    // octave 6
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B6"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯6"),
    // octave 7
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B7"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯7"),
    // octave 8
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♯8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♭8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "A♯8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♭8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B8"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "B♯8"),
    // octave 9
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♭9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "C♯9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♭9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "D♯9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♭9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "E♯9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♭9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "F♯9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G♭9"),
    MUSE_TRANSLATE_NOOP("global/pitchName", "G9"),
};

static constexpr const char *sharp_notes[] = {MUSE_TRANSLATE_NOOP("global/noteName", "C"),  MUSE_TRANSLATE_NOOP("global/noteName", "C♯"),
                                              MUSE_TRANSLATE_NOOP("global/noteName", "D"),  MUSE_TRANSLATE_NOOP("global/noteName", "D♯"),
                                              MUSE_TRANSLATE_NOOP("global/noteName", "E"),  MUSE_TRANSLATE_NOOP("global/noteName", "F"),
                                              MUSE_TRANSLATE_NOOP("global/noteName", "F♯"), MUSE_TRANSLATE_NOOP("global/noteName", "G"),
                                              MUSE_TRANSLATE_NOOP("global/noteName", "G♯"), MUSE_TRANSLATE_NOOP("global/noteName", "A"),
                                              MUSE_TRANSLATE_NOOP("global/noteName", "A♯"), MUSE_TRANSLATE_NOOP("global/noteName", "B")};

static constexpr const char *flat_notes[] = {MUSE_TRANSLATE_NOOP("global/noteName", "C"),  MUSE_TRANSLATE_NOOP("global/noteName", "D♭"),
                                             MUSE_TRANSLATE_NOOP("global/noteName", "D"),  MUSE_TRANSLATE_NOOP("global/noteName", "E♭"),
                                             MUSE_TRANSLATE_NOOP("global/noteName", "E"),  MUSE_TRANSLATE_NOOP("global/noteName", "F"),
                                             MUSE_TRANSLATE_NOOP("global/noteName", "G♭"), MUSE_TRANSLATE_NOOP("global/noteName", "G"),
                                             MUSE_TRANSLATE_NOOP("global/noteName", "A♭"), MUSE_TRANSLATE_NOOP("global/noteName", "A"),
                                             MUSE_TRANSLATE_NOOP("global/noteName", "B♭"), MUSE_TRANSLATE_NOOP("global/noteName", "B")};

std::string muse::pitch_to_string(int pitch, bool addoctave, bool use_flats /* = false */) {
    if (pitch < 0 || pitch > 127) {
        return std::string();
    }

    auto source = use_flats ? ::flat_notes : ::sharp_notes;

    int i = pitch % 12;
    if (addoctave) {
        int octave = (pitch / 12) - 1;
        std::string key = std::string(source[i]) + std::to_string(octave);
        return muse::trc("global/pitchName", key.c_str());
    }
    return muse::trc("global/noteName", source[i]);
}
