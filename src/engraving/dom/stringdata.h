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

#include <cstddef>
#include <vector>
#include <map>

#include "../types/fraction.h"
#include "../types/types.h"

namespace mu::engraving {
class Chord;
class Note;

class Staff;

//---------------------------------------------------------
//   StringData
//---------------------------------------------------------

// defines the string of an instrument
struct instrString {
    instrString(int p = 0, bool o = false, int s = 0) : pitch(p), open(o), startFret(s) {}

    int pitch = 0;     // the pitch of the string
    bool open = false; // true: string is open | false: string is fretted
    int startFret = 0; // banjo 5th string starts on 5th fret
    bool useFlat = false;

    bool operator==(const instrString &d) const { return d.pitch == pitch && d.open == open; }
};

class StringData {
public:
    StringData() {}

    StringData(int numFrets, int numStrings, int strings[], bool useFlats = false);
    StringData(int numFrets, std::vector<instrString> &strings);

    bool isNull() const;

    void set(const StringData &src);
    bool convertPitch(int pitch, int pitchOffset, int *string, int *fret, const CapoParams &capo = {}) const;
    bool convertPitch(int pitch, const Staff *staff, int *string, int *fret) const;
    bool convertPitch(int pitch, const Staff *staff, const Fraction &tick, int *string, int *fret) const;
    int fret(int pitch, int string, const Staff *staff) const;
    int fret(int pitch, int string, const Staff *staff, const Fraction &tick) const;
    void fretChords(Chord *chord) const;
    int getPitch(int string, int fret, int pitchOffset) const;
    int getPitch(int string, int fret, const Staff *staff) const;
    int getPitch(int string, int fret, const Staff *staff, const Fraction &tick) const;
    static int pitchOffsetAt(const Staff *staff);
    static int pitchOffsetAt(const Staff *staff, const Fraction &tick);
    static int pitchOffsetAt(const Staff *staff, const Fraction &tick, int string);

    size_t strings() const { return stringTable_.size(); }

    int frettedStrings() const;

    const std::vector<instrString> &stringList() const { return stringTable_; }

    std::vector<instrString> &stringList() { return stringTable_; }

    int frets() const { return frets_; }

    void setFrets(int val) { frets_ = val; }

    bool operator==(const StringData &d) const { return d.frets_ == frets_ && d.stringTable_ == stringTable_; }

    void configBanjo5thString();
    int adjustBanjo5thFret(int fret) const;
    bool isFiveStringBanjo() const;

    bool useFlats() const { return useFlats_; }

private:

    int fret(int pitch, int string, int pitchOffset) const;
    void sortChordNotes(std::map<int, Note *> &sortedNotes, const Chord *chord, int *count) const;
    void sortChordNotesUseSameString(const Chord *chord) const;
    bool hasPendingPitchChange(const Chord *chord) const;
    void updateFretsOnSameStrings(const Chord *chord) const;
    void preferBassStringForNegativeFret(const Chord *chord) const;
    void reassignNegativeFretNotes(const Chord *chord) const;
    bool tryResolveStringConflictWithOutOfRangeFret(const Note *note, int numStrings, std::vector<int> &bUsed, int &nNewString,
                                                    int &nNewFret) const;

    //      std::vector<int>  stringTable { 40, 45, 50, 55, 59, 64 };   // guitar is default
    //      int         _frets = 19;
    std::vector<instrString> stringTable_; // no strings by default
    int frets_ = 0;

    static bool bFretting;
    bool useFlats_ = false;
};
} // namespace mu::engraving
