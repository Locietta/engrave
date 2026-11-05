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

#include "../shared/musicxmltypes.h"
#include "musicxmlvoicedesc.h"

namespace mu::iex::musicxml {
class MusicXmlOctaveShiftList : public std::map<engraving::Fraction, int> {
public:
    MusicXmlOctaveShiftList() {}

    int octaveShift(const engraving::Fraction f) const;
    void addOctaveShift(const int shift, const engraving::Fraction f);
    void calcOctaveShiftShifts();
};

class MusicXmlPart {
public:
    MusicXmlPart(muse::String id = {}, muse::String name = {});
    void addMeasureNumberAndDuration(muse::String measureNumber, engraving::Fraction measureDuration);

    muse::String getId() const { return id_; }

    muse::String toString() const;
    VoiceList voicelist; // the voice map information TODO: make private
    engraving::Fraction measureDuration(size_t i) const;

    size_t nMeasures() const { return measureDurations_.size(); }

    MusicXmlInstrList _instrList;    // TODO: make private
    MusicXmlIntervalList _intervals; ///< Transpositions
    engraving::Interval interval(const engraving::Fraction f) const;
    int octaveShift(const engraving::staff_idx_t staff, const engraving::Fraction f) const;
    void addOctaveShift(const engraving::staff_idx_t staff, const int shift, const engraving::Fraction f);
    void calcOctaveShifts();

    void setName(muse::String nm) { name_ = nm; }

    muse::String getName() const { return name_; }

    void setPrintName(const bool b) { printName_ = b; }

    bool getPrintName() const { return printName_; }

    void setAbbr(muse::String ab) { abbr_ = ab; }

    muse::String getAbbr() const { return abbr_; }

    void setPrintAbbr(const bool b) { printAbbr_ = b; }

    bool getPrintAbbr() const { return printAbbr_; }

    std::map<int, int> staffNumberToIndex() const { return staffNumberToIndex_; }

    int staffNumberToIndex(const int staffNumber) const;

    void insertStaffNumberToIndex(const int staffNumber, const int staffIndex) { staffNumberToIndex_.insert({staffNumber, staffIndex}); }

    LyricNumberHandler &lyricNumberHandler() { return lyricNumberHandler_; }

    const LyricNumberHandler &lyricNumberHandler() const { return lyricNumberHandler_; }

    void setMaxStaff(const int staff);

    int maxStaff() const { return maxStaff_; }

    bool isVocalStaff() const;
    bool isPercussionStaff() const;

    void hasLyrics(bool b) { hasLyrics_ = b; }
private:
    muse::String id_;
    muse::String name_;
    bool printName_ = true;
    muse::String abbr_;
    bool printAbbr_ = true;
    std::vector<muse::String> measureNumbers_;          // MusicXML measure number attribute
    std::vector<engraving::Fraction> measureDurations_; // duration in fraction for every measure
    std::vector<MusicXmlOctaveShiftList> octaveShifts_; // octave shift list for every staff
    LyricNumberHandler lyricNumberHandler_;
    int maxStaff_ = -1; // maximum staff value found (0 based), -1 = none
    bool hasLyrics_ = false;
    std::map<int, int> staffNumberToIndex_; // Mapping from staff number to index in staff list.
    // Only for when staves are discarded in MusicXmlParserPass1::attributes.
};
} // namespace mu::iex::musicxml
