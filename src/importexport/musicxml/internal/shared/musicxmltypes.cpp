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

#include "musicxmltypes.h"

#include "engraving/dom/interval.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::iex::musicxml;

String MusicXmlInstrument::to_string() const {
    return String(u"chan %1 prog %2 vol %3 pan %4 unpitched %5 name '%6' sound '%7' head %8 line %9 stemDir %10")
        .arg(midi_channel)
        .arg(midi_program)
        .arg(midi_volume)
        .arg(midi_pan)
        .arg(unpitched)
        .arg(name, sound)
        .arg(int(notehead))
        .arg(line)
        .arg(int(stem_direction));
}

//---------------------------------------------------------
//   interval
//---------------------------------------------------------

Interval MusicXmlIntervalList::interval(const Fraction f) const {
    if (empty()) {
        return {};
    }
    auto i = upper_bound(f);
    if (i == begin()) {
        return {};
    }
    --i;
    return i->second;
}

//---------------------------------------------------------
//   instrument
//---------------------------------------------------------

const String MusicXmlInstrList::instrument(const Fraction f) const {
    if (empty()) {
        return String();
    }
    auto i = upper_bound(f);
    if (i == begin()) {
        return String();
    }
    --i;
    return i->second;
}

//---------------------------------------------------------
//   setInstrument
//---------------------------------------------------------

void MusicXmlInstrList::set_instrument(const String instr, const Fraction f) {
    // TODO determine how to handle multiple instrument changes at the same time
    // current implementation keeps the first one
    if (!insert({f, instr}).second) {
        LOGD() << "element already exists, instr: " << instr << ", tick: " << f.to_string() << "(" << f.ticks() << ")";
    }
    //(*this)[f] = instr;
}

//---------------------------------------------------------
//   LyricNumberHandler
//   collect lyric numbering information and determine order
//
//   MusicXML lyrics may contain name and number attributes,
//   plus position information (typically default-y).
//   Name and number are simply tokens with no specified usage.
//   Default-y cannot easily be used to determine the lyrics
//   line, as it tends to differ per system depending on the
//   actual notes present.
//
//   Simply collecting all possible lyric number attributes
//   within a MusicXML part and assigning lyrics position
//   based on alphabetically sorting works well for all
//   common MusicXML files.
//---------------------------------------------------------

//---------------------------------------------------------
//   addNumber
//---------------------------------------------------------

void LyricNumberHandler::add_number(const String &number) {
    if (number_to_no_.find(number) == number_to_no_.end()) {
        number_to_no_[number] = -1; // unassigned
    }
}

//---------------------------------------------------------
//   toString
//---------------------------------------------------------

String LyricNumberHandler::to_string() const {
    String res;
    for (const auto &p : number_to_no_) {
        if (!res.is_empty()) {
            res += u" ";
        }
        res += String(u"%1:%2").arg(p.first).arg(p.second);
    }
    return res;
}

//---------------------------------------------------------
//   getLyricNo
//---------------------------------------------------------

int LyricNumberHandler::get_lyric_no(const String &number) const {
    const auto it = number_to_no_.find(number);
    return it == number_to_no_.end() ? 0 : it->second;
}

//---------------------------------------------------------
//   determineLyricNos
//---------------------------------------------------------

void LyricNumberHandler::determine_lyric_nos() {
    int i = 0;
    for (auto &p : number_to_no_) {
        p.second = i;
        ++i;
    }
}
