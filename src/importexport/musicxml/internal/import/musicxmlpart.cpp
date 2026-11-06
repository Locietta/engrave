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

#include "musicxmlpart.h"

#include "engraving/dom/interval.h"
#include "engraving/dom/mscore.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::iex::musicxml;

static const std::vector<String> vocal_instrument_names = {u"Voice",    u"Soprano", u"Mezzo-Soprano", u"Alto", u"Tenor",
                                                           u"Baritone", u"Bass",    u"Women",         u"Men"};

static const std::vector<String> percussion_instrument_names = {
    u"Percussion", u"Timpani", u"Glockenspiel", u"Xylophone",  u"Vibraphone",  u"Marimba",   u"Bell", u"Drum", u"Cymbal",
    u"Triangle",   u"Claves",  u"Wood Blocks",  u"Tambourine", u"Finger Snap", u"Hand Clap", u"Slap", u"Stamp"};

int MusicXmlOctaveShiftList::octave_shift(const Fraction f) const {
    if (empty()) {
        return 0;
    }
    auto i = upper_bound(f);
    if (i == begin()) {
        return 0;
    }
    --i;
    return i->second;
}

void MusicXmlOctaveShiftList::add_octave_shift(const int shift, const Fraction f) {
    IF_ASSERT_FAILED(Fraction(0, 1) <= f) { return; }

    //LOGD("addOctaveShift(shift %d f %s)", shift, muPrintable(f.print()));
    auto i = find(f);
    if (i == end()) {
        //LOGD("addOctaveShift: not found, inserting");
        insert({f, shift});
    } else {
        //LOGD("addOctaveShift: found %d, adding", (*this)[f]);
        (*this)[f] += shift;
        //LOGD("addOctaveShift: res %d", (*this)[f]);
    }
}

void MusicXmlOctaveShiftList::calc_octave_shift_shifts() {
    /*
    for (auto i = cbegin(); i != cend(); ++i)
          LOGD(" [%s : %d]", muPrintable((*i).first.print()), (*i).second);
     */

    // to each MusicXmlOctaveShiftList entry, add the sum of all previous ones
    int current_shift = 0;
    for (auto i = begin(); i != end(); ++i) {
        current_shift += i->second;
        i->second = current_shift;
    }

    /*
    for (auto i = cbegin(); i != cend(); ++i)
          LOGD(" [%s : %d]", muPrintable((*i).first.print()), (*i).second);
     */
}

MusicXmlPart::MusicXmlPart(String id, String name) : id_(id), name_(name) { octave_shifts_.resize(::mu::engraving::max_staves); }

void MusicXmlPart::add_measure_number_and_duration(String measure_number, Fraction measure_duration) {
    measure_numbers_.push_back(measure_number);
    measure_durations_.push_back(measure_duration);
}

void MusicXmlPart::set_max_staff(const int staff) {
    if (staff > max_staff_) {
        max_staff_ = staff;
    }
}

Fraction MusicXmlPart::measure_duration(size_t i) const {
    if (i < measure_durations_.size()) {
        return measure_durations_.at(i);
    }
    return Fraction(0, 0); // return invalid fraction
}

String MusicXmlPart::to_string() const {
    String res = String(u"part id '%1' name '%2' print %3 abbr '%4' print %5 maxStaff %6\n")
                     .arg(id_, name_)
                     .arg(print_name_)
                     .arg(abbr_)
                     .arg(print_abbr_, max_staff_);

    for (const auto &p : voicelist) {
        res += String(u"voice %1 map staff data %2\n").arg(String::number(p.first + 1), p.second.to_string());
    }

    for (size_t i = 0; i < measure_numbers_.size(); ++i) {
        if (i > 0) {
            res += u"\n";
        }
        res += String(u"measure %1 duration %2 (%3)")
                   .arg(measure_numbers_.at(i), measure_durations_.at(i).to_string())
                   .arg(measure_durations_.at(i).ticks());
    }

    return res;
}

Interval MusicXmlPart::interval(const Fraction f) const { return intervals.interval(f); }

int MusicXmlPart::octave_shift(const staff_idx_t staff, const Fraction f) const {
    if (::mu::engraving::max_staves <= staff) {
        return 0;
    }
    if (f < Fraction(0, 1)) {
        return 0;
    }
    return octave_shifts_[staff].octave_shift(f);
}

void MusicXmlPart::add_octave_shift(const staff_idx_t staff, const int shift, const Fraction f) {
    if (::mu::engraving::max_staves <= staff) {
        return;
    }
    if (f < Fraction(0, 1)) {
        return;
    }
    octave_shifts_[staff].add_octave_shift(shift, f);
}

void MusicXmlPart::calc_octave_shifts() {
    for (staff_idx_t i = 0; i < ::mu::engraving::max_staves; ++i) {
        octave_shifts_[i].calc_octave_shift_shifts();
    }
}

//---------------------------------------------------------
//   staffNumberToIndex
//---------------------------------------------------------

/**
 This handles the mapping from MusicXML staff number to the index
 in a Part's Staff list.
 In most cases, this is a simple decrement from the 1-based staff number
 to the 0-based index.
 However, in some parts some MusicXML staves are discarded, and a mapping
 must be stored from MusicXML staff number to index. When this mapping is
 defined (i.e. size() != 0), it is used. See MusicXmlParserPass1::attributes()
 for more information.
 */

int MusicXmlPart::staff_number_to_index(const int staff_number) const {
    if (staff_number_to_index_.size() == 0) {
        return staff_number - 1;
    } else if (muse::contains(staff_number_to_index_, staff_number)) {
        return staff_number_to_index_.at(staff_number);
    } else {
        return -1;
    }
}

bool MusicXmlPart::is_vocal_staff() const {
    return std::find(::vocal_instrument_names.begin(), ::vocal_instrument_names.end(), name_) != ::vocal_instrument_names.end();
}

bool MusicXmlPart::is_percussion_staff() const {
    for (const String &name : ::percussion_instrument_names) {
        if (name_.contains(name)) {
            return true;
        }
    }
    return false;
}
