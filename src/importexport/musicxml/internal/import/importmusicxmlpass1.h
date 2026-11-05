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

#include "global/serialization/xmlstreamreader.h"
#include "musicxmltupletstate.h"
#include "musicxmlpart.h"
#include "engraving/engravingerrors.h"

namespace mu::engraving {
class Fraction;
class Score;
class Part;
class VBox;
} // namespace mu::engraving

namespace mu::iex::musicxml {
class VoiceOverlapDetector;

//---------------------------------------------------------
//   PageFormat
//---------------------------------------------------------

struct PageFormat {
    engraving::SizeF size;       // automatically initialized (to invalid)
    double printableWidth = 5;   // _width - left margin - right margin
    double evenLeftMargin = 0.2; // values in inch
    double oddLeftMargin = 0.2;
    double evenTopMargin = 0.2;
    double evenBottomMargin = 0.2;
    double oddTopMargin = 0.2;
    double oddBottomMargin = 0.2;
    bool twosided = false;
};

typedef std::pair<int, int> StartStop;
typedef std::vector<StartStop> StartStopList;

//---------------------------------------------------------
//   MusicXmlOctaveShiftDesc
//---------------------------------------------------------

struct MusicXmlOctaveShiftDesc {
    enum class Type : char { UP, DOWN, STOP, NONE };
    Type tp;
    short size;
    engraving::Fraction time;
    short num;

    MusicXmlOctaveShiftDesc() : tp(Type::NONE), size(0), num(-1) {}

    MusicXmlOctaveShiftDesc(Type _tp, short _size, engraving::Fraction _tm) : tp(_tp), size(_size), time(_tm), num(-1) {}
};

//---------------------------------------------------------
//   MusicXmlPartGroup
//---------------------------------------------------------

struct MusicXmlPartGroup {
    int span = 0;
    int start = 0;
    engraving::BracketType type = engraving::BracketType::NO_BRACKET;
    bool barlineSpan = false;
    muse::draw::Color color;
    size_t column = 0;
};

typedef std::vector<MusicXmlPartGroup *> MusicXmlPartGroupList;
typedef std::map<muse::String, engraving::Part *> PartMap;
typedef std::map<int, MusicXmlPartGroup *> MusicXmlPartGroupMap;

//---------------------------------------------------------
//   CreditWords
//    a single parsed MusicXML credit-words element
//---------------------------------------------------------

struct CreditWords {
    int page = 0;
    muse::String type;
    double defaultX = 0.0;
    double defaultY = 0.0;
    double fontSize = 0.0;
    muse::String justify;
    muse::String hAlign;
    muse::String vAlign;
    muse::String words;

    CreditWords(int p, muse::String tp, double dx, double dy, double fs, muse::String j, muse::String ha, muse::String va, muse::String w) {
        page = p;
        type = tp;
        defaultX = dx;
        defaultY = dy;
        fontSize = fs;
        justify = j;
        hAlign = ha;
        vAlign = va;
        words = w;
    }
};

typedef std::vector<CreditWords *> CreditWordsList;

//---------------------------------------------------------
//   declarations
//---------------------------------------------------------

bool isLikelyCreditText(const muse::String &text, const bool caseInsensitive);
bool isLikelySubtitleText(const muse::String &text, const bool caseInsensitive);

//---------------------------------------------------------
//   MusicXmlParserPass1
//---------------------------------------------------------

class MusicXmlLogger;
class IMusicXmlConfiguration;

class MusicXmlParserPass1 {
public:
    MusicXmlParserPass1(engraving::Score *score, const IMusicXmlConfiguration &configuration, MusicXmlLogger *logger);

    const IMusicXmlConfiguration *configuration() const { return &configuration_; }

    void initPartState(const muse::String &partId);
    engraving::Err parse(const muse::ByteArray &data);
    engraving::Err parse();

    muse::String errors() const { return errors_; }

    void scorePartwise();
    void identification();
    void credit(CreditWordsList &credits);
    void defaults();
    void pageLayout(PageFormat &pf, const double conversion);
    void partList(MusicXmlPartGroupList &partGroupList);
    void partGroup(const int scoreParts, MusicXmlPartGroupList &partGroupList, MusicXmlPartGroupMap &partGroups,
                   muse::String &curPartGroupName);
    void scorePart(const muse::String &curPartGroupName);
    void scoreInstrument(const muse::String &partId, const muse::String &curPartGroupName);
    void setStyle(const muse::String &type, const double val);
    void midiInstrument(const muse::String &partId);
    void part();
    void measure(const muse::String &partId, const engraving::Fraction cTime, engraving::Fraction &mdur, VoiceOverlapDetector &vod,
                 const int measureNr);
    void print(const int measureNr);
    void attributes(const muse::String &partId, const engraving::Fraction cTime);
    void clef(const muse::String &partId);
    void time(const engraving::Fraction cTime);
    void transpose(const muse::String &partId, const engraving::Fraction &tick);
    void divisions();
    void direction(const muse::String &partId, const engraving::Fraction &cTime);
    void directionType(const engraving::Fraction cTime, std::vector<MusicXmlOctaveShiftDesc> &starts,
                       std::vector<MusicXmlOctaveShiftDesc> &stops);
    void handleOctaveShift(const engraving::Fraction &cTime, const muse::String &type, short size, MusicXmlOctaveShiftDesc &desc);
    void notations(MusicXmlStartStop &tupletStartStop);
    void note(const muse::String &partId, const engraving::Fraction &cTime, engraving::Fraction &missingPrev, engraving::Fraction &dura,
              engraving::Fraction &missingCurr, VoiceOverlapDetector &vod, MusicXmlTupletStates &tupletStates);
    void notePrintSpacingNo(engraving::Fraction &dura);
    engraving::Fraction calcTicks(const int &intTicks, const int &_divisions, const muse::XmlStreamReader *xmlReader);

    engraving::Fraction calcTicks(const int &intTicks) { return calcTicks(intTicks, divs_, &e_); }

    void duration(engraving::Fraction &dura, muse::XmlStreamReader &e);

    void duration(engraving::Fraction &dura) { duration(dura, e_); }

    void forward(engraving::Fraction &dura);
    void backup(engraving::Fraction &dura);
    void timeModification(engraving::Fraction &timeMod);
    void pitch(int &step, float &alter, int &oct);
    void rest();
    void skipLogCurrElem();
    bool determineMeasureLength(std::vector<engraving::Fraction> &ml) const;
    VoiceList getVoiceList(const muse::String &id) const;
    bool determineStaffMoveVoice(const muse::String &id, const int mxStaff, const int &mxVoice, int &msMove, int &msTrack,
                                 int &msVoice) const;
    int voiceToInt(const muse::String &voice);
    engraving::track_idx_t trackForPart(const muse::String &id) const;
    bool hasPart(const muse::String &id) const;

    engraving::Part *getPart(const muse::String &id) const { return muse::value(partMap_, id); }

    MusicXmlPart getMusicXmlPart(const muse::String &id) const { return muse::value(parts_, id); }

    size_t nparts() const { return parts_.size(); }

    MusicXmlInstruments getInstruments(const muse::String &id) const { return muse::value(instruments_, id); }

    void setDrumsetDefault(const muse::String &id, const muse::String &instrId, const engraving::NoteHeadGroup hg, const int line,
                           const engraving::DirectionV sd);
    MusicXmlInstrList getInstrList(const muse::String &id) const;
    MusicXmlIntervalList getIntervals(const muse::String &id) const;
    engraving::Fraction getMeasureStart(const size_t i) const;
    int octaveShift(const muse::String &id, const engraving::staff_idx_t staff, const engraving::Fraction &f) const;

    const CreditWordsList &credits() const { return credits_; }

    bool hasBeamingInfo() const { return hasBeamingInfo_; }

    bool isVocalStaff(const muse::String &partId) const { return parts_.at(partId).isVocalStaff(); }

    bool isPercussionStaff(const muse::String &partId) const { return parts_.at(partId).isPercussionStaff(); }

    static engraving::VBox *createAndAddVBoxForCreditWords(engraving::Score *score, engraving::Fraction tick);
    void createDefaultHeader(engraving::Score *const score);
    void createMeasuresAndVboxes(engraving::Score *const score, const std::vector<engraving::Fraction> &ml,
                                 const std::vector<engraving::Fraction> &ms, const std::set<int> &systemStartMeasureNrs,
                                 const std::set<int> &pageStartMeasureNrs, const CreditWordsList &crWords, const muse::Size &pageSize);

    void setHasInferredHeaderText(bool b) { hasInferredHeaderText_ = b; }

    bool hasInferredHeaderText() const { return hasInferredHeaderText_; }

    int maxDiff() const { return maxDiff_; }

    void insertAdjustedDuration(engraving::Fraction key, engraving::Fraction value) { adjustedDurations_.insert({key, value}); }

    std::map<engraving::Fraction, engraving::Fraction> &adjustedDurations() { return adjustedDurations_; }

    void insertSeenDenominator(int val) { seenDenominators_.emplace(val); }

    MusicXmlExporterSoftware exporterSoftware() const { return exporterSoftware_; }

    bool sibOrDolet() const;
    bool dolet() const;

private:
    // functions
    void addError(const muse::String &error); // Add an error to be shown in the GUI
    void setExporterSoftware(muse::String &exporter);
    void setExporterStyles();

    // generic pass 1 data
    muse::XmlStreamReader e_;
    MusicXmlExporterSoftware exporterSoftware_ = MusicXmlExporterSoftware::OTHER; // Software which exported the file
    int divs_ = 0;                                                                // Current MusicXML divisions value
    std::map<muse::String, MusicXmlPart> parts_;                                  // Parts data, mapped on part id
    std::set<int> systemStartMeasureNrs_;                                         // Measure numbers of measures starting a page
    std::set<int> pageStartMeasureNrs_;                                           // Measure numbers of measures starting a page
    std::vector<engraving::Fraction> measureLength_;                              // Length of each measure
    std::vector<engraving::Fraction> measureStart_;                               // Start time of each measure
    CreditWordsList credits_;                                                     // All credits collected
    PartMap partMap_;                                                             // TODO merge into MusicXmlPart ??
    std::map<muse::String, MusicXmlInstruments> instruments_;                     // instruments for each part, mapped on part id
    engraving::Score *score_ = nullptr;                                           // MuseScore score
    const IMusicXmlConfiguration &configuration_;
    MusicXmlLogger *logger_ = nullptr; // Error logger
    muse::String errors_;              // Errors to present to the user
    bool hasBeamingInfo_ = false;      // Whether the score supports or contains beaming info
    bool hasInferredHeaderText_ = false;

    // part specific data (TODO: move to part-specific class)
    engraving::Fraction timeSigDura_;                     // Measure duration according to last timesig read
    std::map<int, MusicXmlOctaveShiftDesc> octaveShifts_; // Pending octave-shifts
    muse::Size pageSize_;                                 // Page width read from defaults

    const int maxDiff_ = 5;                                                // Duration rounding tick threshold;
    std::map<engraving::Fraction, engraving::Fraction> adjustedDurations_; // Rounded durations
    std::set<int> seenDenominators_;                                       // Denominators seen. Used for rounding errors.
};
} // namespace mu::iex::musicxml
