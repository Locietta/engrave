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

#include "stafftype.h"

#include "draw/fontmetrics.h"
#include "translation.h"

#include "iengravingconfiguration.h"

#include "style/defaultstyle.h"
#include "style/style.h"
#include "style/textstyle.h"
#include "types/typesconv.h"

#include "mscore.h"
#include "score.h"
#include "staff.h"

using namespace muse::draw;
using namespace muse::io;
using namespace mu::engraving;

namespace mu::engraving {
// HISTORIC TAB BASS STRING NOTATION
// The following constants refer to the specifics of bass string notation in historic
//    (Renaiss./Baroque French and Italian) tablatures.

// how much to lower a bass string note with slashes with respect to line distance (in fraction of line distance)
constexpr double STAFFTYPE_TAB_BASSSLASH_YOFFSET = 0.33;

// The following constants could ideally be customizable values;
//    they are currently constants to simplify implementation;
// Note that these constants do not constrain which strings of an instrument are
//    physically frettable (which is defined in the instrument itself) but fix the
//    number of bass strings for which the notation is able to express a fret number
//    rather than simply a string ordinal.
constexpr int NUM_OF_BASSSTRINGS_WITH_LETTER = 4; // the max number of bass strings frettable with letter notation (French)
constexpr int NUM_OF_BASSSTRINGS_WITH_NUMBER = 2; // the max number of bass strings frettable with number notation (Italian)

//---------------------------------------------------------
//   StaffTypeTablature
//---------------------------------------------------------

constexpr double TAB_DEFAULT_DUR_YOFFS = -1.0;

Color StaffType::s_defaultColor = Color::BLACK;

std::vector<TablatureFretFont> StaffType::fretFonts_ = {};
std::vector<TablatureDurationFont> StaffType::durationFonts_ = {};

//---------------------------------------------------------
//   StaffType
//---------------------------------------------------------

StaffType::StaffType() {
    color_ = s_defaultColor;
    // set reasonable defaults for type-specific members */
    symRepeat_ = TablatureSymbolRepeat::NEVER;
    setDurationFontName(durationFonts_[0].displayName);
    setFretPresetIdx(0);
}

StaffType::StaffType(StaffGroup sg, StaffTypes staffType, int lines, int stpOff, double lineDist, bool genClef, bool showBarLines,
                     bool stemless, bool genTimeSig, bool genKeySig, bool showLedgerLines, bool invisible, const Color &color)
    : group_(sg), staffType_(staffType), invisible_(invisible), color_(color), lines_(lines), stepOffset_(stpOff),
      lineDistance_(Spatium(lineDist)), showBarlines_(showBarLines), showLedgerLines_(showLedgerLines), stemless_(stemless),
      genClef_(genClef), genTimesig_(genTimeSig), genKeysig_(genKeySig) {}

StaffType::StaffType(StaffGroup sg, StaffTypes staffType, int lines, int stpOff, double lineDist, bool genClef, bool showBarLines,
                     bool stemless, bool genTimesig, bool invisible, const Color &color, const String &durFontName, double genDur,
                     bool fretFontUseTextStyle, const String &fretFontName, TablatureSymbolRepeat symRepeat, bool linesThrough,
                     TablatureMinimStyle minimStyle, bool onLines, bool showRests, bool stemsDown, bool stemThrough, bool upsideDown,
                     bool showTabFingering, bool useNumbers, bool showBackTied) {
    UNUSED(invisible);
    color_ = color;
    group_ = sg;
    staffType_ = staffType;
    setLines(lines);
    setStepOffset(stpOff);
    setLineDistance(Spatium(lineDist));
    setGenClef(genClef);
    setShowBarlines(showBarLines);
    setStemless(stemless);
    setGenTimesig(genTimesig);
    setGenKeysig(sg != StaffGroup::TAB);
    setDurationFontName(durFontName);
    setGenDurations(genDur);
    setFretUseTextStyle(fretFontUseTextStyle);
    if (fretFontUseTextStyle) {
        setFretTextStyle(TextStyleType::TAB_FRET_NUMBER);
    } else {
        setFretPreset(fretFontName);
    }
    setSymbolRepeat(symRepeat);
    setLinesThrough(linesThrough);
    setMinimStyle(minimStyle);
    setOnLines(onLines);
    setShowRests(showRests);
    setStemsDown(stemsDown);
    setStemsThrough(stemThrough);
    setUpsideDown(upsideDown);
    setShowTabFingering(showTabFingering);
    setUseNumbers(useNumbers);
    setShowBackTied(showBackTied);
}

//---------------------------------------------------------
//   translatedGroupName
//---------------------------------------------------------

String StaffType::translatedGroupName() const { return TConv::translatedUserName(group_); }

int StaffType::middleLine() const { return lines_ - 1 - stepOffset_; }

int StaffType::bottomLine() const { return (lines_ - 1) * 2; }

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool StaffType::operator==(const StaffType &st) const {
    bool equal = true;

    equal &= (group_ == st.group_);
    equal &= (staffType_ == st.staffType_);
    equal &= (staffLabel_ == st.staffLabel_);
    equal &= (userMag_ == st.userMag_);
    equal &= (yoffset_ == st.yoffset_);
    equal &= (small_ == st.small_);
    equal &= (invisible_ == st.invisible_);
    equal &= (color_ == st.color_);
    equal &= (lines_ == st.lines_);
    equal &= (stepOffset_ == st.stepOffset_);
    equal &= (lineDistance_ == st.lineDistance_);
    equal &= (showBarlines_ == st.showBarlines_);
    equal &= (showLedgerLines_ == st.showLedgerLines_);
    equal &= (stemless_ == st.stemless_);
    equal &= (genClef_ == st.genClef_);
    equal &= (genTimesig_ == st.genTimesig_);
    equal &= (genKeysig_ == st.genKeysig_);
    equal &= (noteHeadScheme_ == st.noteHeadScheme_);
    equal &= (instrumentLabelVisibility_ == st.instrumentLabelVisibility_);
    equal &= (durationFontSize_ == st.durationFontSize_);
    equal &= (durationFontUserY_ == st.durationFontUserY_);
    equal &= (fretFontSize_ == st.fretFontSize_);
    equal &= (fretFontUserY_ == st.fretFontUserY_);
    equal &= (genDurations_ == st.genDurations_);
    equal &= (linesThrough_ == st.linesThrough_);
    equal &= (minimStyle_ == st.minimStyle_);
    equal &= (symRepeat_ == st.symRepeat_);
    equal &= (onLines_ == st.onLines_);
    equal &= (showRests_ == st.showRests_);
    equal &= (stemsDown_ == st.stemsDown_);
    equal &= (stemsThrough_ == st.stemsThrough_);
    equal &= (upsideDown_ == st.upsideDown_);
    equal &= (showTabFingering_ == st.showTabFingering_);
    equal &= (useNumbers_ == st.useNumbers_);
    equal &= (showBackTied_ == st.showBackTied_);
    equal &= (durationBoxH_ == st.durationBoxH_);
    equal &= (durationBoxY_ == st.durationBoxY_);
    equal &= (durationFont_ == st.durationFont_);
    equal &= (durationFontIdx_ == st.durationFontIdx_);
    equal &= (durationYOffset_ == st.durationYOffset_);
    equal &= (durationGridYOffset_ == st.durationGridYOffset_);
    equal &= (fretBoxH_ == st.fretBoxH_);
    equal &= (deadFretBoxH_ == st.deadFretBoxH_);
    equal &= (fretBoxY_ == st.fretBoxY_);
    equal &= (deadFretBoxY_ == st.deadFretBoxY_);
    equal &= (fretFont_ == st.fretFont_);
    equal &= (fretFontInfo_.family == st.fretFontInfo_.family);
    equal &= (fretYOffset_ == st.fretYOffset_);
    equal &= (fretUseTextStyle_ == st.fretUseTextStyle_);
    equal &= (fretTextStyle_ == st.fretTextStyle_);
    equal &= (fretPresetIdx_ == st.fretPresetIdx_);

    return equal;
}

StaffTypes StaffType::type() const { return staffType_; }

String StaffType::staffTypeName() const { return TConv::translatedUserName(staffType_); }

String StaffType::xmlName() const { return String::fromAscii(TConv::toXml(staffType_).ascii()); }

//---------------------------------------------------------
//   isSimpleTabStaff
//---------------------------------------------------------

bool StaffType::isSimpleTabStaff() const {
    if (!isTabStaff()) {
        return false;
    }

    StaffTypes stType = type();

    switch (stType) {
        case StaffTypes::TAB_4SIMPLE:
        case StaffTypes::TAB_5SIMPLE:
        case StaffTypes::TAB_6SIMPLE:
        case StaffTypes::TAB_7SIMPLE:
        case StaffTypes::TAB_8SIMPLE:
        case StaffTypes::TAB_9SIMPLE:
        case StaffTypes::TAB_10SIMPLE:
        case StaffTypes::TAB_ITALIAN:
        case StaffTypes::TAB_FRENCH: return true;

        default: break;
    }

    return false;
}

//---------------------------------------------------------
//   isCommonTabStaff
//---------------------------------------------------------

bool StaffType::isCommonTabStaff() const { return !isTabStaff() ? false : !isSimpleTabStaff(); }

//---------------------------------------------------------
//   isHiddenElementOnTab
//---------------------------------------------------------

bool StaffType::isHiddenElementOnTab(Sid commonTabStyle, Sid simpleTabStyle) const {
    return (isCommonTabStaff() && !style().styleB(commonTabStyle)) || (isSimpleTabStaff() && !style().styleB(simpleTabStyle));
}

void StaffType::styleChanged() {
    if (!fretUseTextStyle_) {
        return;
    }
    setFretTextStyle(fretTextStyle_);
}

//---------------------------------------------------------
//   setOnLines
//---------------------------------------------------------

void StaffType::setOnLines(bool val) {
    onLines_ = val;
    setDurationMetrics();
    setFretMetrics();
}

void StaffType::setUseNumbers(bool val) {
    useNumbers_ = val;
    setFretMetrics();
}

void StaffType::setFretTextStyle(const TextStyleType &val) {
    fretTextStyle_ = val;
    fretFontInfo_ = TablatureFretFont();
    fretFont_ = Font();

    const TextStyle *ts = textStyle(fretTextStyle_);

    for (const TextStyleProperty property : *ts) {
        switch (property.type) {
            case TextStylePropertyType::FontFace: {
                String fontName = style().styleSt(property.sid);
                fretFontInfo_.family = fontName;
                fretFont_.setFamily(fontName, Font::Type::Tablature);
            } break;
            case TextStylePropertyType::FontSize: {
                double fontSize = style().styleD(property.sid);
                setFretFontSize(fontSize);
            } break;
            case TextStylePropertyType::FontStyle: {
                FontStyle fStyle = style().styleV(property.sid).value<FontStyle>();
                fretFont_.setBold(fStyle & FontStyle::Bold);
                fretFont_.setItalic(fStyle & FontStyle::Italic);
                fretFont_.setUnderline(fStyle & FontStyle::Underline);
                fretFont_.setStrike(fStyle & FontStyle::Strike);
            } break;
            default: continue;
        }
    }

    PointF offset = style().styleV(ts->offsetSids.above).value<PointF>();
    setFretFontUserY(offset.y());

    setFretMetrics();
}

void StaffType::setFretPresetIdx(size_t idx) {
    // Clear all previous formatting
    fretFont_ = Font();
    if (idx >= fretFonts_.size()) {
        fretPresetIdx_ = 0;
        fretFontInfo_ = fretFonts_[0];
    } else {
        fretPresetIdx_ = idx;
        fretFontInfo_ = fretFonts_[idx];
    }

    fretFont_.setFamily(fretFontInfo_.family, Font::Type::Tablature);
    setFretFontSize(fretFontInfo_.defSize);
    setFretFontUserY(fretFontInfo_.defYOffset);
    setFretMetrics();
}

void StaffType::setFretPreset(const String &name) {
    String locName = name;
    // convert old names for two built-in fonts which have changed of name
    if (name == "MuseScore Tab Late Renaiss") {
        locName = u"MuseScore Phalèse";
    }
    size_t idx = 0;
    for (idx = 0; idx < fretFonts_.size(); idx++) {
        if (fretFonts_[idx].displayName == locName) {
            break;
        }
    }

    setFretPresetIdx(idx);
}

//---------------------------------------------------------
//   setDurationMetrics
//    checks whether the internally computed metrics are is still valid and re-computes them, if not
//---------------------------------------------------------

void StaffType::setScore(Score *score) {
    score_ = score;
    if (score_ && group_ == StaffGroup::TAB) {
        setDurationMetrics();
        setFretMetrics();
    }
}

void StaffType::setDurationMetrics() {
    if (!score_) {
        return;
    }

    // FontMetrics returns results unreliably rounded to integral pixels;
    // use a scaled up font and then scale computed values down
    Font font(durationFont());
    font.setPointSizeF(durationFontSize_);
    FontMetrics fm(font);
    String txt(durationFonts_[durationFontIdx_].displayValue, size_t(TabVal::NUM_OF));
    RectF bb(fm.tightBoundingRect(txt));
    // raise symbols by a default margin and, if marks are above lines, by half the line distance
    // (converted from spatium units to raster units)
    durationGridYOffset_ = (TAB_DEFAULT_DUR_YOFFS - (onLines_ ? 0.0 : lineDistance().val() * 0.5)) * defaultSpatium();
    // this is the bottomest point of any duration sign
    durationYOffset_ = durationGridYOffset_;
    // move symbols so that the lowest margin 'sits' on the base line:
    // move down by the whole part above (negative) the base line
    // ( -bb.y() ) then up by the whole height ( -bb.height() )
    durationYOffset_ -= (bb.height() + bb.y()) / 100.0;
    durationBoxH_ = bb.height() / 100.0;
    durationBoxY_ = durationGridYOffset_ - bb.height() / 100.0;
}

void StaffType::setFretMetrics() {
    if (!score_) {
        return;
    }

    FontMetrics fm(fretFont());
    RectF bb;
    // compute vertical displacement
    if (useNumbers_) {
        // compute total height of used characters
        String txt;
        for (int idx = 0; idx < 10; idx++) { // use only first 10 digits
            txt.append(fretFontInfo_.displayDigit[idx]);
        }
        bb = fm.tightBoundingRect(txt);
        // for numbers: centre on '0': move down by the whole part above (negative)
        // the base line ( -bb.y() ) then up by half the whole height ( -bb.height()/2 )
        RectF bx(fm.tightBoundingRect(fretFontInfo_.displayDigit[0]));
        fretYOffset_ = -(bx.y() + bx.height() / 2.0);
        // _fretYOffset = -(bb.y() + bb.height()/2.0);  // <- using bbox of all chars
    } else {
        // compute total height of used characters
        const String txt(fretFontInfo_.displayLetter.data(), NUM_OF_LETTERFRETS);
        bb = fm.tightBoundingRect(txt);
        // for letters: centre on the 'a' ascender, by moving down half of the part above the base line in bx
        RectF bx(fm.tightBoundingRect(fretFontInfo_.displayLetter[0]));
        fretYOffset_ = -bx.y() / 2.0;
    }

    // Calculate position for dead fret marks - these must be centred separately based on their glyph
    RectF deadBb = fm.tightBoundingRect(fretFontInfo_.xChar);
    double lineThickness = style().styleS(Sid::staffLineWidth).val() * defaultSpatium() * 0.5;
    deadFretYOffset_ = -deadBb.y() / 2.0 + lineThickness;

    // if on string, we are done; if between strings, raise by half line distance
    if (!onLines_) {
        double lineAdj = lineDistance().val() * defaultSpatium() * 0.5;
        fretYOffset_ -= lineAdj;
        deadFretYOffset_ -= lineAdj;
    }

    // from _fretYOffset, compute _fretBoxH and _fretBoxY
    fretBoxH_ = bb.height();
    fretBoxY_ = bb.y() + fretYOffset_;

    deadFretBoxH_ = deadBb.height();
    deadFretBoxY_ = deadBb.y() + deadFretYOffset_;
}

//---------------------------------------------------------
//   setDurationFontName
//---------------------------------------------------------

void StaffType::setDurationFontName(const String &name) {
    size_t idx;
    for (idx = 0; idx < durationFonts_.size(); idx++) {
        if (durationFonts_[idx].displayName == name) {
            break;
        }
    }
    if (idx >= durationFonts_.size()) {
        idx = 0; // if name not found, use first font
    }
    durationFontIdx_ = idx;
    durationFont_.setFamily(durationFonts_[idx].family, Font::Type::Tablature);
    setDurationFontSize(durationFonts_[idx].defSize);
    setDurationFontUserY(durationFonts_[idx].defYOffset);
    setDurationMetrics();
}

//---------------------------------------------------------
//   durationBoxH / durationBoxY
//---------------------------------------------------------

double StaffType::defaultSpatium() const { return StyleDef::styleValues[static_cast<size_t>(Sid::spatium)].defaultValue.toDouble(); }

double StaffType::durationBoxH() const {
    if (!genDurations_ && !stemless_) {
        return 0.0;
    }
    return durationBoxH_;
}

double StaffType::durationBoxY() const {
    if (!genDurations_ && !stemless_) {
        return 0.0;
    }
    return durationBoxY_ + durationFontUserY_ * defaultSpatium();
}

double StaffType::durationFontYOffset() const { return durationYOffset_ + durationFontUserY_ * defaultSpatium(); }

double StaffType::fretBoxY() const { return fretBoxY_ + fretFontUserY_ * defaultSpatium(); }

double StaffType::deadFretBoxY() const { return deadFretBoxY_ + fretFontUserY_ * defaultSpatium(); }

double StaffType::fretMaskH() const { return lineDistance_.val() * defaultSpatium(); }

double StaffType::fretMaskY() const { return (onLines_ ? -0.5 : -1.0) * lineDistance_.val() * defaultSpatium(); }

double StaffType::fretFontYOffset() const { return fretYOffset_ + fretFontUserY_ * defaultSpatium(); }

//---------------------------------------------------------
//   setDurationFontSize / setFretFontSize
//---------------------------------------------------------

void StaffType::setDurationFontSize(double val) {
    durationFontSize_ = val;
    durationFont_.setPointSizeF(val);
    setDurationMetrics();
}

void StaffType::setFretFontSize(double val) {
    fretFontSize_ = val;
    fretFont_.setPointSizeF(val);
    setFretMetrics();
}

//---------------------------------------------------------
//   fretString / durationString
//
//    construct the text string for a given fret / duration
//---------------------------------------------------------

static const String UNKNOWN_FRET = String(u"?");

String StaffType::fretString(int fret, int string, bool deadNote) const {
    if (fret == INVALID_FRET_INDEX) {
        return UNKNOWN_FRET;
    }
    if (deadNote) {
        return String(fretFontInfo_.xChar);
    } else {
        bool hasFret;
        String text = tabBassStringPrefix(string, &hasFret);
        if (!hasFret) {  // if the notation does not allow to fret this string,
            return text; // return the prefix only
        }
        // otherwise, add to prefix the relevant digit/letter string
        return text + (useNumbers_ ? (fret >= NUM_OF_DIGITFRETS ? UNKNOWN_FRET : fretFontInfo_.displayDigit[fret]) :
                                     (fret >= NUM_OF_LETTERFRETS ? UNKNOWN_FRET : fretFontInfo_.displayLetter[fret]));
    }
}

String StaffType::durationString(DurationType type, int dots) const {
    String s = durationFonts_[durationFontIdx_].displayValue[size_t(type)];
    for (int count = 0; count < dots; count++) {
        s.append(durationFonts_[durationFontIdx_].displayDot);
    }
    return s;
}

//---------------------------------------------------------
//    tabBassStringPrefix
//
//    returns a String (possibly empty) with the prefix identifying a bass string in TAB's;
//    can deal with non-bass strings (i.e. regular TAB lines).
//
//    Implements the specifics of historic notations for bass lines (i.e. strings outside
//    the lines of the tab), both Italian and French.
//
//    strg   the instrument physical string ordinal (0 = topmost string, may exceed the number
//                of lines actually present in the TAB to reference a bass string)
//    bool   pntr to a bool receiving the info if notation allows to express a fret number or not
//                (this is potentially different from the fact that the instrument string itself can be fretted or not)
//---------------------------------------------------------

String StaffType::tabBassStringPrefix(int strg, bool *hasFret) const {
    *hasFret = true; // assume notation allows to fret this string
    int bassStrgIdx = (strg >= lines_ ? strg - lines_ + 1 : 0);
    if (useNumbers_) {
        // if above the max bass string which can be fretted with number notation
        // return a number with the string index
        if (bassStrgIdx > NUM_OF_BASSSTRINGS_WITH_NUMBER) {
            *hasFret = false;
            return fretFontInfo_.displayDigit[strg + 1];
        }
        // if a frettable bass string, return an empty string
        return String();
    } else {
        // bass string notation
        // if above the max bass string which can be fretted with letter notation
        // return a number with the bass string index itself
        if (bassStrgIdx > NUM_OF_BASSSTRINGS_WITH_LETTER) {
            *hasFret = false;
            return fretFontInfo_.displayDigit[bassStrgIdx - 1];
        }
        // if a frettable bass string, return a character with the relevant num. of slashes;
        // note that the number of slashes is bassStrgIdx-1 (1st bass has no slash)
        // and slashChar[] is 0-based (slashChar[0] => 1 slash, ...), whence the -2
        String prefix = bassStrgIdx > 1 ? String(fretFontInfo_.slashChar[bassStrgIdx - 2]) : String();
        return prefix;
    }
}

//---------------------------------------------------------
//   numOfLedgerLines
//
//    in TAB's, returns the number of ledgerlines needed by bass lines in some TAB styles.
//
//    Returns 0 if staff is not a TAB, if a TAB but style does not use ledger lines
//    or ledger lines do not apply to the given string.
//---------------------------------------------------------

int StaffType::numOfTabLedgerLines(int string) const {
    if (group_ != StaffGroup::TAB || !useNumbers_) {
        return 0;
    }

    int numOfLedgers = string < 0 ? -string : string - lines_ + 1;
    return numOfLedgers >= 1 && numOfLedgers <= NUM_OF_BASSSTRINGS_WITH_NUMBER ? numOfLedgers : 0;
}

//---------------------------------------------------------
//   physStringToVisual / visualStringToPhys
//
//    returns the string ordinal in visual order (top to down) from a string ordinal in physical order
//    or vice-versa: manages upsideDown
//---------------------------------------------------------

int StaffType::physStringToVisual(int strg) const {
    if (strg < 0) { // if above top string, return top string
        strg = 0;
    }
    //      // NO: bass strings may exist, which are in addition to tab string lines
    //      if (strg >= _lines)                 // if physical string has no visual representation,
    //            strg = _lines - 1;            // reduce to nearest visual line
    // if TAB upside down, flip around top line
    return upsideDown_ ? lines_ - 1 - strg : strg;
}

int StaffType::visualStringToPhys(int line) const {
    // if TAB upside down, reverse string number
    line = (upsideDown_ ? lines_ - 1 - line : line);

    if (line < 0) { // if above top string, reduce to top string
        line = 0;
    }
    // NO: bass strings may exist, which are in addition to tab string lines
    //      if (line >= _lines)
    //            line = _lines - 1;
    return line;
}

//---------------------------------------------------------
//   physStringToYOffset
//
//    returns the string Y offset from a string ordinal in physical order:
//    manages upsideDown and extra bass strings.
//
//    The returned values is in sp. and is relative to the staff top line.
//
//    Note: the difference with physStringToVisual() is that this function takes into account
//          peculiarities of bass string notations.
//---------------------------------------------------------

Spatium StaffType::physStringToYOffset(int strg) const {
    double yOffset = strg; // the y offset of the visual string, as a multiple of line distance
    if (yOffset < 0) {     // if above top physical string, limit to top string
        yOffset = 0;
    }
    if (yOffset >= lines_) { // if physical string 'below' tab lines,
        yOffset = lines_;    // reduce to first string 'below' tab body
        if (!useNumbers_) {  // with letters, add some space for the slashes ascender
            yOffset = onLines_ ? lines_ : lines_ + STAFFTYPE_TAB_BASSSLASH_YOFFSET;
        }
    }
    // if TAB upside down, flip around top line
    yOffset = upsideDown_ ? (double) (lines_ - 1) - yOffset : yOffset;
    return yOffset * lineDistance_;
}

//---------------------------------------------------------
//   STATIC FUNCTIONS FOR FONT CONFIGURATION MANAGEMENT
//---------------------------------------------------------

TablatureFretFont::TablatureFretFont() {
    // Set up defaults
    for (size_t i = 0; i < NUM_OF_DIGITFRETS; i++) {
        displayDigit.at(i) = String::number(i);
    }

    for (size_t i = 0; i < NUM_OF_LETTERFRETS; i++) {
        displayLetter[i] = Char(u'a' + static_cast<char16_t>(i));
    }

    for (size_t i = 0; i < NUM_OF_BASSSTRING_SLASHES; i++) {
        for (size_t j = 0; j < i; j++) {
            slashChar.at(i).append(u"/");
        }
    }
}

TablatureDurationFont::TablatureDurationFont() {
    for (size_t i = 0; i < size_t(TabVal::NUM_OF); i++) {
        displayValue[i] = Char(u'a' + static_cast<char16_t>(i));
    }
}

//---------------------------------------------------------
//   Tablature font presets
//---------------------------------------------------------

static TablatureFretFont makeFretFontPreset(const String &family, const String &displayName) {
    // Create fret font from a regular font
    TablatureFretFont f;
    f.family = family;
    f.displayName = displayName;
    return f;
}

static TablatureFretFont makeMuseScoreFretFontPreset(const String &family, const String &displayName) {
    // Create fret font from a specialist MuseScore tablature font
    TablatureFretFont f;
    f.family = family;
    f.displayName = displayName;
    f.defSize = 10.0;
    f.displayDigit[10] = String(u"X");
    for (size_t i = 0; i < NUM_OF_BASSSTRING_SLASHES; ++i) {
        f.slashChar[i] = String(Char(u'A' + static_cast<char16_t>(i)));
    }
    return f;
}

static TablatureDurationFont makeDurationFontPreset(const String &family, const String &displayName,
                                                    Spatium beamWidth = GRID_BEAM_DEF_WIDTH, Spatium stemHeight = GRID_STEM_DEF_HEIGHT,
                                                    Spatium stemWidth = GRID_STEM_DEF_WIDTH,
                                                    DurationType zeroBeamLevel = DurationType::V_QUARTER) {
    TablatureDurationFont f;
    f.family = family;
    f.displayName = displayName;
    f.gridBeamWidth = beamWidth;
    f.gridStemHeight = stemHeight;
    f.gridStemWidth = stemWidth;
    f.zeroBeamLevel = zeroBeamLevel;
    return f;
}

void StaffType::initTabFonts() {
    // keep displayName strings and the order/count of these presets unchanged:
    // saved scores reference a preset by displayName (<460) or by
    // positional index (>=460)
    fretFonts_ = {
        makeFretFontPreset(u"FreeSans", u"MuseScore Tab Sans"),
        makeFretFontPreset(u"FreeSerif", u"MuseScore Tab Serif"),
        makeMuseScoreFretFontPreset(u"MuseScoreTabRenaiss", u"MuseScore Tab Renaiss"),
        makeMuseScoreFretFontPreset(u"MuseScoreTabPhalese", u"MuseScore Phalèse"),
        makeMuseScoreFretFontPreset(u"MuseScoreTabBonneuilDeVisee", u"MuseScore Bonneuil-de Visée"),
        makeMuseScoreFretFontPreset(u"MuseScoreTabBonneuilGaultier", u"MuseScore Bonneuil-Gaultier"),
        makeMuseScoreFretFontPreset(u"MuseScoreTabDowland", u"MuseScore Dowland"),
        makeMuseScoreFretFontPreset(u"MuseScoreTabLuteDidactic", u"MuseScore Lute Didactic"),
    };

    durationFonts_ = {
        makeDurationFontPreset(u"MuseScoreTabModern", u"MuseScore Tab Modern", 0.5_sp, 3.0_sp, 0.1_sp, DurationType::V_QUARTER),
        makeDurationFontPreset(u"MuseScoreTabItalian", u"MuseScore Tab Italian", 0.15_sp, 1.75_sp, 0.15_sp, DurationType::V_WHOLE),
        makeDurationFontPreset(u"MuseScoreTabFrench", u"MuseScore Tab French", 0.30_sp, 3.125_sp, 0.21_sp, DurationType::V_QUARTER),
        makeDurationFontPreset(u"MuseScoreTabFrenchBaroqueHeadless", u"MuseScore French Baroque (headless)"),
        makeDurationFontPreset(u"MuseScoreTabFrenchBaroque", u"MuseScore French Baroque"),
    };
}

//---------------------------------------------------------
//   fontNames
//
//    returns a list of display names for the fonts  configured to work with Tablatures;
//    the index of a name in the list can be used to retrieve the font data with fontData()
//---------------------------------------------------------

std::vector<String> StaffType::tabFontNames(bool bDuration) {
    std::vector<String> names;
    if (bDuration) {
        names.reserve(durationFonts_.size());
        for (const TablatureDurationFont &f : durationFonts_) {
            names.push_back(f.displayName);
        }
    } else {
        names.reserve(fretFonts_.size());
        for (const TablatureFretFont &f : fretFonts_) {
            names.push_back(f.displayName);
        }
    }
    return names;
}

//---------------------------------------------------------
//   fontData
//
//    retrieves data about a Tablature font.
//    returns: true if idx is valid | false if it is not
//---------------------------------------------------------

bool StaffType::tabFontData(bool bDuration, size_t nIdx, double &pSize, double &pYOff) {
    if (bDuration) {
        if (nIdx < durationFonts_.size()) {
            TablatureDurationFont f = durationFonts_.at(nIdx);
            pSize = f.defSize;
            pYOff = f.defYOffset;
            return true;
        }
    } else {
        TablatureFretFont f = nIdx < fretFonts_.size() ? fretFonts_.at(nIdx) : TablatureFretFont();
        pSize = f.defSize;
        pYOff = f.defYOffset;
        return true;
    }
    return false;
}

const MStyle &StaffType::style() const {
    if (!score_) {
        return DefaultStyle::defaultStyle();
    }

    return score_->style();
}

//---------------------------------------------------------
//   spatium
//---------------------------------------------------------

double StaffType::spatium() const { return style().spatium() * (isSmall() ? style().styleD(Sid::smallStaffMag) : 1.0) * userMag(); }

//=========================================================
//
//   BUILT-IN STAFF TYPES and STAFF TYPE PRESETS
//
//=========================================================

static const int _defaultPreset[STAFF_GROUP_MAX] = {
    0, // default pitched preset is "stdNormal"
    4, // default percussion preset is "perc5lines"
    5  // default tab preset is "tab6StrCommon"
};

//---------------------------------------------------------
//   Static functions for StaffType presets
//---------------------------------------------------------

const StaffType *StaffType::preset(StaffTypes idx) {
    if (int(idx) < 0 || int(idx) >= int(presets_.size())) {
        return &presets_[0];
    }

    return &presets_[int(idx)];
}

const StaffType *StaffType::presetFromXmlName(const String &xmlName) {
    for (size_t i = 0; i < presets_.size(); ++i) {
        if (presets_[i].xmlName() == xmlName) {
            return &presets_[i];
        }
    }

    return nullptr;
}

const StaffType *StaffType::getDefaultPreset(StaffGroup grp) {
    int _idx = _defaultPreset[int(grp)];
    return &presets_[_idx];
}

//---------------------------------------------------------
//   initStaffTypes
//---------------------------------------------------------

std::vector<StaffType> StaffType::presets_;

/* *INDENT-OFF* */
void StaffType::initStaffTypes(const Color &defaultColor) {
    s_defaultColor = defaultColor;
    initTabFonts(); // set up TAB font presets, before initStaffTypes()

    // keep in sync with enum class StaffTypes
    presets_ = {
        //                       group,              staff type,                    lin stpOff  dist clef   bars stmless time  key    ledger invis     color
        StaffType(StaffGroup::STANDARD, StaffTypes::STANDARD, 5, 0, 1, true, true, false, true, true, true, false, defaultColor),
        StaffType(StaffGroup::PERCUSSION, StaffTypes::PERC_1LINE, 1, 0, 1, true, true, false, true, false, true, false, defaultColor),
        StaffType(StaffGroup::PERCUSSION, StaffTypes::PERC_2LINE, 2, 0, 1, true, true, false, true, false, true, false, defaultColor),
        StaffType(StaffGroup::PERCUSSION, StaffTypes::PERC_3LINE, 3, 0, 1, true, true, false, true, false, true, false, defaultColor),
        StaffType(StaffGroup::PERCUSSION, StaffTypes::PERC_5LINE, 5, 0, 1, true, true, false, true, false, true, false, defaultColor),

        //                 group            staff type,               lin stpOff dist clef   bars stemless time  invis     color       duration font       genDur textStyle fret font                               duration symbol repeat       thru    minim style                  onLin  rests  stmDn  stmThr upsDn  sTFing nums  bkTied
        StaffType(StaffGroup::TAB, StaffTypes::TAB_6SIMPLE, 6, 0, 1.5, true, true, true, false, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_6COMMON, 6, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_6FULL, 6, 0, 1.5, true, true, false, true, false, defaultColor, u"MuseScore Tab Modern",
                  false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false, TablatureMinimStyle::SLASHED, true, true, true,
                  true, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_4SIMPLE, 4, 0, 1.5, true, true, true, false, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_4COMMON, 4, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_4FULL, 4, 0, 1.5, true, true, false, true, false, defaultColor, u"MuseScore Tab Modern",
                  false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false, TablatureMinimStyle::SLASHED, true, true, true,
                  true, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_5SIMPLE, 5, 0, 1.5, true, true, true, false, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_5COMMON, 5, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_5FULL, 5, 0, 1.5, true, true, false, true, false, defaultColor, u"MuseScore Tab Modern",
                  false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false, TablatureMinimStyle::SLASHED, true, true, true,
                  true, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_UKULELE, 4, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_BALALAJKA, 3, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_DULCIMER, 3, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, true, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_ITALIAN, 6, 0, 1.5, false, true, true, true, false, defaultColor,
                  u"MuseScore Tab Italian", true, false, u"MuseScore Tab Renaiss", TablatureSymbolRepeat::NEVER, true,
                  TablatureMinimStyle::NONE, true, true, false, false, true, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_FRENCH, 6, 0, 1.5, false, true, true, true, false, defaultColor, u"MuseScore Tab French",
                  true, false, u"MuseScore Tab Renaiss", TablatureSymbolRepeat::NEVER, true, TablatureMinimStyle::NONE, false, false, false,
                  false, false, false, false, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_7COMMON, 7, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_8COMMON, 8, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_9COMMON, 9, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_10COMMON, 10, 0, 1.5, true, true, false, true, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_7SIMPLE, 7, 0, 1.5, true, true, true, false, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_8SIMPLE, 8, 0, 1.5, true, true, true, false, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_9SIMPLE, 9, 0, 1.5, true, true, true, false, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_10SIMPLE, 10, 0, 1.5, true, true, true, false, false, defaultColor,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
    };
}

/* *INDENT-ON* */
} // namespace mu::engraving
