/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#include "global/types/string.h"

#include "engraving/dom/interval.h"
#include "engraving/types/types.h"

namespace mu::engraving {
struct Arpeggio;
} // namespace mu::engraving

namespace mu::iex::musicxml {
const int max_number_level = 16; // maximum number of overlapping MusicXML objects

enum class MusicXmlExporterSoftware : char { DOLET6, DOLET8, DORICO, FINALE, NOTEFLIGHT, SIBELIUS, OTHER };

//---------------------------------------------------------
//   MusicXmlStartStop
//---------------------------------------------------------

enum class MusicXmlStartStop : char { NONE, START, STOP };

struct MusicXmlArpeggioDesc {
    engraving::Arpeggio *arp;
    int no;

    MusicXmlArpeggioDesc(engraving::Arpeggio *arp, int no) : arp(arp), no(no) {}
};

typedef std::multimap<int, MusicXmlArpeggioDesc> ArpeggioMap;

//---------------------------------------------------------
//   MusicXmlInstrument
//---------------------------------------------------------

/**
 A single instrument in a MusicXML part.
 Used for both a drum part and a (non-drum) multi-instrument part
 */

struct MusicXmlInstrument {
    int unpitched;                     // midi-unpitched read from MusicXML
    muse::String name;                 // instrument-name read from MusicXML
    muse::String sound;                // instrument-sound read from MusicXML
    muse::String virt_lib;             // virtual-library read from MusicXML
    muse::String virt_name;            // virtual-name read from MusicXML
    int midi_channel;                  // midi-channel read from MusicXML
    int midi_port;                     // port read from MusicXML
    int midi_program;                  // midi-program read from MusicXML
    int midi_volume;                   // volume read from MusicXML
    int midi_pan;                      // pan value read from MusicXML
    engraving::NoteHeadGroup notehead; // notehead symbol set
    int line = 0;                      // place notehead onto this line
    engraving::DirectionV stem_direction;

    muse::String to_string() const;

    MusicXmlInstrument() // required by std::map
        : unpitched(-1), name(), midi_channel(-1), midi_port(-1), midi_program(-1), midi_volume(100), midi_pan(63),
          notehead(engraving::NoteHeadGroup::HEAD_INVALID), line(0), stem_direction(engraving::DirectionV::AUTO) {}

    MusicXmlInstrument(muse::String s)
        : unpitched(-1), name(s), midi_channel(-1), midi_port(-1), midi_program(-1), midi_volume(100), midi_pan(63),
          notehead(engraving::NoteHeadGroup::HEAD_NORMAL), line(0), stem_direction(engraving::DirectionV::AUTO) {}

    /*
    MusicXmlInstrument(int p, String s, NoteHead::Group nh, int l, Direction d)
          : unpitched(p), name(s), midiChannel(-1), midiPort(-1), midiProgram(-1), midiVolume(100), midiPan(63),
          notehead(nh), line(l), stemDirection(d) {}
     */
};

typedef std::map<muse::String, MusicXmlInstrument> MusicXmlInstruments;

struct MusicXmlIntervalList : std::map<engraving::Fraction, engraving::Interval> {
    MusicXmlIntervalList() {}

    engraving::Interval interval(const engraving::Fraction f) const;
};

struct MusicXmlInstrList : std::map<engraving::Fraction, muse::String> {
    MusicXmlInstrList() {}

    const muse::String instrument(const engraving::Fraction f) const;
    void set_instrument(const muse::String instr, const engraving::Fraction f);
};

struct LyricNumberHandler {
    LyricNumberHandler() {}

    void add_number(const muse::String &number);
    muse::String to_string() const;
    int get_lyric_no(const muse::String &number) const;
    void determine_lyric_nos();
private:
    std::map<muse::String, int> number_to_no_;
};
} // namespace mu::iex::musicxml
