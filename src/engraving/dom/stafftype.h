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

#include <array>

#include "draw/types/font.h"

#include "stafflabel.h"
#include "../style/styledef.h"
#include "../types/types.h"

namespace mu::engraving {
class Chord;
class ChordRest;
class MStyle;
class Score;
class Staff;

//---------------------------------------------------------
//   TablatureFont
//---------------------------------------------------------

constexpr int NUM_OF_DIGITFRETS = 100;       // the max fret number which can be rendered with numbers
constexpr int NUM_OF_LETTERFRETS = 17;       // the max fret number which can be rendered with letters
constexpr int NUM_OF_BASSSTRING_SLASHES = 5; // the max number of slashes supported for French bass strings notation
                                             // (currently, only 3 slashes are used at most; another two are
                                             // foreseen for future customizability)

// default values for 'grid'-like beaming to use with value symbols in stemless TAB
constexpr Spatium GRID_BEAM_DEF_WIDTH = 0.25_sp; // all values in sp
constexpr Spatium GRID_STEM_DEF_HEIGHT = 1.75_sp;
constexpr Spatium GRID_STEM_DEF_WIDTH = 0.125_sp;

struct TablatureFretFont {
    TablatureFretFont();

    String family;                                           // the family of the physical font to use
    String displayName;                                      // the name to display to the user
    double defSize = 9.0;                                    // the default size of the font
    double defYOffset = 0.0;                                 // the default Y displacement
    Char xChar = u'X';                                       // the char to use for 'x'
    std::array<String, NUM_OF_BASSSTRING_SLASHES> slashChar; // the char used to draw one or more '/' symbols
    std::array<String, NUM_OF_DIGITFRETS> displayDigit;      // the string to draw for digit frets
    std::array<Char, NUM_OF_LETTERFRETS> displayLetter;      // the char to use for letter frets
};

enum class TabVal : char {
    VAL_LONGA = 0,
    VAL_BREVIS,
    VAL_SEMIBREVIS,
    VAL_MINIMA,
    VAL_SEMIMINIMA,
    VAL_FUSA,
    VAL_SEMIFUSA,
    VAL_32,
    VAL_64,
    VAL_128,
    VAL_256,
    VAL_512,
    VAL_1024,
    NUM_OF
};

enum class TablatureMinimStyle : char {
    NONE = 0, // do not draw half notes at all
    SHORTER,  // draw half notes with a shorter stem
    SLASHED   // draw half notes with stem with two slashes
};

enum class TablatureSymbolRepeat : char {
    NEVER = 0, // never repeat the same duration symbol
    SYSTEM,    // repeat at the beginning of a new system
    MEASURE,   // repeat at the beginning of a new measure
    ALWAYS     // always repeat
};

struct TablatureDurationFont {
    TablatureDurationFont();

    String family;                                 // the family of the physical font to use
    String displayName;                            // the name to display to the user
    double defSize = 15.0;                         // the default size of the font
    double defYOffset = 0.0;                       // the default Y displacement
    Spatium gridBeamWidth = GRID_BEAM_DEF_WIDTH;   // the width of the 'grid'-style beam (in sp)
    Spatium gridStemHeight = GRID_STEM_DEF_HEIGHT; // the height of the 'grid'-style stem (in sp)
    Spatium gridStemWidth = GRID_STEM_DEF_WIDTH;   // the width of the 'grid'-style stem (in sp)
    // the note value with no beaming in 'grid'-style beaming
    DurationType zeroBeamLevel = DurationType::V_QUARTER;
    Char displayDot = u'l';                 // the char to use to draw a dot
    Char displayValue[int(TabVal::NUM_OF)]; // the char to use to draw a duration value
};

enum class ShowTiedFret : unsigned char {
    TIE_AND_FRET,
    TIE,
    NONE,
};

enum class ParenthesizeTiedFret : unsigned char {
    START_OF_SYSTEM,
    START_OF_MEASURE,
    NEVER,
};

//---------------------------------------------------------
//   StaffType
//---------------------------------------------------------

class StaffType {
    // The default color of a new staff type, set by initStaffTypes().
    static Color s_defaultColor;

public:
    StaffType();

    StaffType(StaffGroup sg, StaffTypes staffType, int lines, int stpOff, double lineDist, bool genClef, bool showBarLines, bool stemless,
              bool genTimeSig, bool genKeySig, bool showLedgerLiness, bool invisible, const Color &color);

    StaffType(StaffGroup sg, StaffTypes staffType, int lines, int stpOff, double lineDist, bool genClef, bool showBarLines, bool stemless,
              bool genTimesig, bool invisible, const Color &color, const String &durFontName, double genDur, bool fretFontUseTextStyle,
              const String &fretFontName, TablatureSymbolRepeat symRepeat, bool linesThrough, TablatureMinimStyle minimStyle, bool onLines,
              bool showRests, bool stemsDown, bool stemThrough, bool upsideDown, bool showTabFingering, bool useNumbers, bool showBackTied);

    virtual ~StaffType() = default;

    bool operator==(const StaffType &) const;

    const MStyle &style() const;

    const Score *score() const { return score_; }

    StaffGroup group() const { return group_; }

    void setGroup(StaffGroup g) { group_ = g; }

    StaffTypes type() const;

    void setType(StaffTypes type) { staffType_ = type; }

    String staffTypeName() const;
    String xmlName() const;
    String translatedGroupName() const;

    const StaffLabel &staffLabel() const { return staffLabel_; }

    StaffLabel &staffLabel() { return staffLabel_; }

    const String &longName() const { return staffLabel_.longName(); }

    const String &shortName() const { return staffLabel_.shortName(); }

    void setLongName(const String &s) { staffLabel_.setLongName(s); }

    void setShortName(const String &s) { staffLabel_.setShortName(s); }

    void setLines(int val) { lines_ = val; }

    int lines() const { return lines_; }

    int middleLine() const;
    int bottomLine() const;

    void setStepOffset(int v) { stepOffset_ = v; }

    int stepOffset() const { return stepOffset_; }

    void setLineDistance(const Spatium &val) { lineDistance_ = val; }

    Spatium lineDistance() const { return lineDistance_; }

    void setGenClef(bool val) { genClef_ = val; }

    bool genClef() const { return genClef_; }

    void setShowBarlines(bool val) { showBarlines_ = val; }

    bool showBarlines() const { return showBarlines_; }

    double userMag() const { return userMag_; }

    bool isSmall() const { return small_; }

    bool invisible() const { return invisible_; }

    const Color &color() const { return color_; }

    void setUserMag(double val) { userMag_ = val; }

    void setSmall(bool val) { small_ = val; }

    void setInvisible(bool val) { invisible_ = val; }

    void setColor(const Color &val) { color_ = val; }

    Spatium yoffset() const { return yoffset_; }

    void setYoffset(Spatium val) { yoffset_ = val; }

    double spatium() const;

    void setStemless(bool val) { stemless_ = val; }

    bool stemless() const { return stemless_; }

    bool genTimesig() const { return genTimesig_; }

    void setGenTimesig(bool val) { genTimesig_ = val; }

    // static function to deal with presets
    static const StaffType *getDefaultPreset(StaffGroup grp);
    static const StaffType *preset(StaffTypes idx);
    static const StaffType *presetFromXmlName(const String &xmlName);

    void setGenKeysig(bool val) { genKeysig_ = val; }

    bool genKeysig() const { return genKeysig_; }

    void setShowLedgerLines(bool val) { showLedgerLines_ = val; }

    bool showLedgerLines() const { return showLedgerLines_; }

    void setNoteHeadScheme(NoteHeadScheme s) { noteHeadScheme_ = s; }

    NoteHeadScheme noteHeadScheme() const { return noteHeadScheme_; }

    void setInstrumentLabelVisibility(InstrumentLabelVisibility v) { instrumentLabelVisibility_ = v; }

    InstrumentLabelVisibility instrumentLabelVisibility() const { return instrumentLabelVisibility_; }

    String fretString(int fret, int string, bool deadNote) const; // returns a string with the text for fret
    String durationString(DurationType type, int dots) const;

    // functions to cope with historic TAB's peculiarities, like upside-down, bass string notations
    int physStringToVisual(int strg) const;                    // return the string in visual order from physical string
    int visualStringToPhys(int line) const;                    // return the string in physical order from visual string
    Spatium physStringToYOffset(int strg) const;               // return the string Y offset (in sp, chord-relative)
    String tabBassStringPrefix(int strg, bool *hasFret) const; // return a string with the prefix, if any, identifying a bass string
    int numOfTabLedgerLines(int string) const;

    // properties getters (some getters require updated metrics)
    double durationBoxH() const;
    double durationBoxY() const;

    const muse::draw::Font &durationFont() const { return durationFont_; }

    const TablatureDurationFont &tabDurationFont() const { return durationFonts_[durationFontIdx_]; }

    const String &durationFontName() const { return durationFonts_[durationFontIdx_].displayName; }

    double durationFontSize() const { return durationFontSize_; }

    double durationFontUserY() const { return durationFontUserY_; }

    double durationFontYOffset() const;

    double durationGridYOffset() const { return durationGridYOffset_; }

    double fretBoxH() const { return fretBoxH_; }

    double deadFretBoxH() const { return deadFretBoxH_; }

    double fretBoxY() const;
    double deadFretBoxY() const;

    // 2 methods to return the size of a box masking lines under a fret mark
    double fretMaskH() const;
    double fretMaskY() const;

    const muse::draw::Font &fretFont() const { return fretFont_; }

    const String fretFontName() const { return fretFontInfo_.displayName; }

    double fretFontSize() const { return fretFontSize_; }

    double fretFontUserY() const { return fretFontUserY_; }

    double fretFontYOffset() const;

    bool genDurations() const { return genDurations_; }

    bool linesThrough() const { return linesThrough_; }

    TablatureMinimStyle minimStyle() const { return minimStyle_; }

    TablatureSymbolRepeat symRepeat() const { return symRepeat_; }

    bool onLines() const { return onLines_; }

    bool showRests() const { return showRests_; }

    bool stemsDown() const { return stemsDown_; }

    bool stemThrough() const { return stemsThrough_; }

    bool upsideDown() const { return upsideDown_; }

    bool showTabFingering() const { return showTabFingering_; }

    bool useNumbers() const { return useNumbers_; }

    bool showBackTied() const { return showBackTied_; }

    bool fretUseTextStyle() const { return fretUseTextStyle_; }

    TextStyleType fretTextStyle() const { return fretTextStyle_; }

    size_t fretPresetIdx() const { return fretPresetIdx_; }

    // properties setters (setting some props invalidates metrics)
    void setDurationFontName(const String &);
    void setDurationFontSize(double);

    void setDurationFontUserY(double val) { durationFontUserY_ = val; }

    void setFretFontSize(double);

    void setFretFontUserY(double val) { fretFontUserY_ = val; }

    void setGenDurations(bool val) { genDurations_ = val; }

    void setLinesThrough(bool val) { linesThrough_ = val; }

    void setMinimStyle(TablatureMinimStyle val) { minimStyle_ = val; }

    void setSymbolRepeat(TablatureSymbolRepeat val) { symRepeat_ = val; }

    void setOnLines(bool);

    void setShowRests(bool val) { showRests_ = val; }

    void setStemsDown(bool val) { stemsDown_ = val; }

    void setStemsThrough(bool val) { stemsThrough_ = val; }

    void setUpsideDown(bool val) { upsideDown_ = val; }

    void setShowTabFingering(bool val) { showTabFingering_ = val; }

    void setUseNumbers(bool val);

    void setShowBackTied(bool val) { showBackTied_ = val; }

    void setScore(Score *score);

    void setFretUseTextStyle(bool val) { fretUseTextStyle_ = val; }

    void setFretTextStyle(const TextStyleType &val);
    void setFretPresetIdx(size_t idx);
    void setFretPreset(const String &str);

    bool isTabStaff() const { return group_ == StaffGroup::TAB; }

    bool isDrumStaff() const { return group_ == StaffGroup::PERCUSSION; }

    bool isSimpleTabStaff() const;
    bool isCommonTabStaff() const;
    bool isHiddenElementOnTab(Sid commonTabStyle, Sid simpleTabStyle) const;

    void styleChanged();

    // static functions for font config files
    static std::vector<String> tabFontNames(bool bDuration);
    static bool tabFontData(bool bDuration, size_t nIdx, double &pSize, double &pYOff);

    static void initStaffTypes(const Color &defaultColor);

    static const std::vector<StaffType> &presets() { return presets_; }

private:

    friend class TabDurationSymbol;

    Score *score_ = nullptr;

    double defaultSpatium() const;
    void setDurationMetrics();
    void setFretMetrics();

    static void initTabFonts();

    StaffGroup group_ = StaffGroup::STANDARD;

    StaffTypes staffType_ = StaffTypes::STANDARD;

    StaffLabel staffLabel_;

    double userMag_ = 1.0; // allowed 0.1 - 10.0
    Spatium yoffset_;
    bool small_ = false;
    bool invisible_ = false;
    Color color_;

    int lines_ = 5;
    int stepOffset_ = 0;
    Spatium lineDistance_ = 1_sp;

    bool showBarlines_ = true;
    bool showLedgerLines_ = true;
    bool stemless_ = false; // do not show stems

    bool genClef_ = true;    // create clef at beginning of system
    bool genTimesig_ = true; // whether time signature is shown or not
    bool genKeysig_ = true;  // create key signature at beginning of system

    // Standard: configurable properties
    NoteHeadScheme noteHeadScheme_ = NoteHeadScheme::HEAD_NORMAL;
    InstrumentLabelVisibility instrumentLabelVisibility_ = InstrumentLabelVisibility::AUTO;

    // TAB: configurable propertiesm
    double durationFontSize_ = 15.0; // the size (in points) for the duration symbol font
    double durationFontUserY_ = 0.0; // the vertical offset (spatium units) for the duration symb. font
    // user configurable
    double fretFontSize_ = 10.0; // the size (in points) for the fret marks font
    double fretFontUserY_ = 0.0; // additional vert. offset of fret marks with respect to
    // the string line (spatium unit); user configurable
    bool genDurations_ = false;                                  // whether duration symbols are drawn or not
    bool linesThrough_ = false;                                  // whether lines for strings and stems may pass through fret marks or not
    TablatureMinimStyle minimStyle_ = TablatureMinimStyle::NONE; // how to draw minim stems (stem-and-beam durations only)
    TablatureSymbolRepeat symRepeat_ = TablatureSymbolRepeat::NEVER; // if and when to repeat the same duration symbol
    bool onLines_ = true;                                            // whether fret marks are drawn on the string lines or between them
    bool showRests_ = false;                                         // whether to draw rests or not
    bool stemsDown_ = true;                                          // stems are drawn downward (stem-and-beam durations only)
    bool stemsThrough_ = true;      // stems are drawn through the staff rather than beside it (stem-and-beam durations only)
    bool upsideDown_ = false;       // whether lines are drawn with highest string at top (false) or at bottom (true)
    bool showTabFingering_ = false; // Allow fingering in tablature staff (true) or not (false)
    bool useNumbers_ = true;        // true: use numbers ('0' - ...) for frets | false: use letters ('a' - ...)
    bool showBackTied_ = true;      // whether back-tied notes are shown or not

    // TAB: internally managed variables
    // Note: values in RASTER UNITS are independent from score scaling and
    //    must be multiplied by magS() to be used in contexts using sp units
    double durationBoxH_ = 0.0;
    double durationBoxY_ = 0.0;     // the height and the y rect.coord. (relative to staff top line)
                                    // of a box bounding all duration symbols (raster units) internally computed:
                                    // depends upon _onString and the metrics of the duration font
    muse::draw::Font durationFont_; // font used to draw dur. symbols; cached for efficiency
    size_t durationFontIdx_ = 0;    // the index of current dur. font in dur. font array
    double durationYOffset_ = 0.0;  // the vertical offset to draw duration symbols with respect to the
    // string lines (raster units); internally computed: depends upon _onString and duration font
    double durationGridYOffset_ = 0.0; // the vertical offset to draw the bottom of duration grid with respect to the
    // string lines (raster units); internally computed: depends upon _onstring and duration font
    double fretBoxH_ = 0.0;
    double fretBoxY_ = 0.0; // the height and the y rect.coord. (relative to staff line)
    double deadFretBoxH_ = 0.0;
    double deadFretBoxY_ = 0.0;
    // of a box bounding all fret characters (raster units) internally computed:
    // depends upon _onString, _useNumbers and the metrics of the fret font
    bool fretUseTextStyle_ = false;
    TextStyleType fretTextStyle_ = TextStyleType::TAB_FRET_NUMBER;
    muse::draw::Font fretFont_; // font used to draw fret marks; cached for efficiency
    TablatureFretFont fretFontInfo_;
    size_t fretPresetIdx_ = 0; // the index of current fret font in fret font array
    double fretYOffset_ = 0.0; // the vertical offset to draw fret marks with respect to the string lines;
    double deadFretYOffset_ = 0.0;
    // (raster units); internally computed: depends upon _onString, _useNumbers
    // and the metrics of the fret font

    // the array of configured fonts
    static std::vector<TablatureFretFont> fretFonts_;
    static std::vector<TablatureDurationFont> durationFonts_;
    static std::vector<StaffType> presets_;
};
} // namespace mu::engraving
