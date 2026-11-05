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

#include <map>
#include <vector>

#include "engravingitem.h"

#include "draw/types/color.h"

#include "cleflist.h"
#include "groups.h"
#include "keylist.h"
#include "mscore.h"
#include "stafftypelist.h"

#include "../types/types.h"

namespace mu::engraving {
class BracketItem;
class Clef;
class Factory;
class InstrumentTemplate;
class KeyList;
class Note;
class Part;
class Score;
class StaffType;
class TimeSig;

enum class Key : signed char;

//---------------------------------------------------------
//    Staff
///    Global staff data not directly related to drawing.
//---------------------------------------------------------

class Staff final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Staff)

public:
    Staff *clone() const override;

    void init(const InstrumentTemplate *, const StaffType *staffType, staff_idx_t templateStaffIdx);
    void initFromStaffType(const StaffType *staffType);
    void init(const Staff *);

    const ID &id() const;
    void setId(const ID &id);

    bool isTop() const;
    String partName() const;
    staff_idx_t rstaff() const;
    staff_idx_t idx() const;

    Part *part() const { return part_; }

    void setPart(Part *p) { part_ = p; }

    Part *originPart(const Fraction &tick) const;

    ClefList &clefList() { return clefs_; }

    ClefTypeList clefType(const Fraction &) const;

    ClefTypeList defaultClefType() const { return defaultClefType_; }

    void setDefaultClefType(const ClefTypeList &l) { defaultClefType_ = l; }

    ClefType clef(const Fraction &) const;
    Fraction nextClefTick(const Fraction &) const;
    Fraction currentClefTick(const Fraction &) const;

    String staffName() const;

    void setClef(Clef *);
    void removeClef(const Clef *);

    void addTimeSig(TimeSig *);
    void removeTimeSig(TimeSig *);
    void clearTimeSig();
    Fraction timeStretch(const Fraction &) const;
    TimeSig *timeSig(const Fraction &) const;
    TimeSig *nextTimeSig(const Fraction &) const;
    Fraction currentTimeSigTick(const Fraction &) const;

    const Groups &group(const Fraction &) const;

    Interval transpose(const Fraction &tick) const;

    KeyList *keyList() { return &keys_; }

    Key key(const Fraction &tick) const { return keySigEvent(tick).key(); }

    Key concertKey(const Fraction &tick) const { return keySigEvent(tick).concertKey(); }

    KeySigEvent keySigEvent(const Fraction &) const;
    Fraction nextKeyTick(const Fraction &) const;
    Fraction currentKeyTick(const Fraction &) const;
    KeySigEvent prevKey(const Fraction &) const;
    void setKey(const Fraction &, KeySigEvent);
    void removeKey(const Fraction &);

    bool show() const;
    bool stemless(const Fraction &) const;

    bool cutaway() const { return cutaway_; }

    void setCutaway(bool val) { cutaway_ = val; }

    bool showIfEntireSystemEmpty() const { return showIfEntireSystemEmpty_; }

    void setShowIfEntireSystemEmpty(bool val) { showIfEntireSystemEmpty_ = val; }

    bool hideSystemBarLine() const { return hideSystemBarLine_; }

    void setHideSystemBarLine(bool val) { hideSystemBarLine_ = val; }

    AutoOnOff hideWhenEmpty() const { return hideWhenEmpty_; }

    void setHideWhenEmpty(AutoOnOff v) { hideWhenEmpty_ = v; }

    AutoOnOff mergeMatchingRests() const { return mergeMatchingRests_; }

    void setMergeMatchingRests(AutoOnOff val) { mergeMatchingRests_ = val; }

    bool shouldMergeMatchingRests() const;

    bool barLineSpan() const { return barLineSpan_; }

    int barLineFrom() const { return barLineFrom_; }

    int barLineTo() const { return barLineTo_; }

    void setBarLineSpan(const bool val) { barLineSpan_ = val; }

    void setBarLineFrom(int val) { barLineFrom_ = val; }

    void setBarLineTo(int val) { barLineTo_ = val; }

    double staffHeight() const;
    double staffHeight(const Fraction &tick) const;

    int channel(const Fraction &, voice_idx_t voice) const;

    void clearChannelList(voice_idx_t voice) { channelList_[voice].clear(); }

    void insertIntoChannelList(voice_idx_t voice, const Fraction &tick, int channelId) {
        channelList_[voice].insert({tick.ticks(), channelId});
    }

    SwingParameters swing(const Fraction &) const;

    void clearSwingMap() { swingMap_.clear(); }

    void insertIntoSwingMap(const Fraction &tick, SwingParameters sp) { swingMap_.insert({tick.ticks(), sp}); }

    const CapoParams &capo(const Fraction &) const;
    void insertCapoParams(const Fraction &tick, const CapoParams &params, bool ignoreNotationUpdate);
    void removeCapoParams(const Fraction &tick);

    //==== staff type helper function
    const StaffType *staffType(const Fraction & = Fraction(0, 1)) const;
    const StaffType *constStaffType(const Fraction &) const;
    const StaffType *staffTypeForElement(const EngravingItem *) const;
    bool isStaffTypeStartFrom(const Fraction & = Fraction(0, 1)) const;
    void moveStaffType(const Fraction &from, const Fraction &to);
    StaffType *staffType(const Fraction &);
    StaffType *setStaffType(const Fraction &, const StaffType &);
    void removeStaffType(const Fraction &);
    void staffTypeListChanged(const Fraction &);

    std::pair<int, int> staffTypeRange(const Fraction &) const;

    bool isPitchedStaff(const Fraction &) const;
    bool isTabStaff(const Fraction &) const;
    bool isDrumStaff(const Fraction &) const;

    int lines(const Fraction &) const;
    void setLines(const Fraction &, int lines);
    double lineDistance(const Fraction &) const;

    bool isLinesInvisible(const Fraction &) const;
    void setIsLinesInvisible(const Fraction &, bool val);

    void setSlashStyle(const Fraction &, bool val);
    int middleLine(const Fraction &) const;
    int bottomLine(const Fraction &) const;

    double staffMag(const Fraction &) const;
    double staffMag(const EngravingItem *element) const;
    double spatium(const Fraction &) const;
    double spatium(const EngravingItem *) const;
    //===========

    int pitchOffset(const Fraction &tick) const;
    void updateOttava();

    std::vector<Staff *> staffList() const;
    Staff *primaryStaff() const;
    bool isPrimaryStaff() const;

    Spatium userDist() const { return userDist_; }

    void setUserDist(Spatium val) { userDist_ = val; }

    void setLocalSpatium(double oldVal, double newVal, Fraction tick);
    bool genKeySig();
    bool showLedgerLines(const Fraction &) const;

    using EngravingItem::color;
    using EngravingItem::setColor;
    Color color(const Fraction &) const;
    void setColor(const Fraction &, const Color &val);
    void insertTime(const Fraction &, const Fraction &len);

    PropertyValue getProperty(Pid) const override;
    bool setProperty(Pid, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    bool playbackVoice(int voice) const;
    void setPlaybackVoice(int voice, bool val);

    const std::array<bool, VOICES> &visibilityVoices() const;
    bool isVoiceVisible(voice_idx_t voice) const;
    bool canDisableVoice() const;

    bool reflectTranspositionInLinkedTab() const;
    void setReflectTranspositionInLinkedTab(bool reflect);

#ifndef NDEBUG
    void dumpClefs(const char *title) const;
    void dumpKeys(const char *title) const;
    void dumpTimeSigs(const char *) const;
#else
    void dumpClefs(const char *) const {}

    void dumpKeys(const char *) const {}

    void dumpTimeSigs(const char *) const {}
#endif

    void triggerLayout() const override;
    void triggerLayout(const Fraction &tick);

    Staff *findLinkedInScore(const Score *score) const override;

    track_idx_t getLinkedTrackInStaff(const Staff *linkedStaff, const track_idx_t strack) const;
    bool trackHasLinksInVoiceZero(track_idx_t track);

    void undoSetShowMeasureNumbers(bool show);
    bool shouldShowMeasureNumbers() const;

    bool isLastOfScore() const;
    bool isSystemObjectStaff() const;
    bool hasSystemObjectsBelowBottomStaff() const;

    String individualStaffNameLong(const Fraction &tick) const;
    String individualStaffNameShort(const Fraction &tick) const;

private:

    friend class Factory;
    Staff(Part *parent);
    Staff(const Staff &staff);

    double staffMag(const StaffType *) const;

    friend class Excerpt;
    void setVoiceVisible(voice_idx_t voice, bool visible);
    void updateVisibilityVoices(const Staff *masterStaff, const TracksMap &tracks);

    ID id_ = INVALID_ID;
    Part *part_ = nullptr;

    ClefList clefs_;
    ClefTypeList defaultClefType_;

    KeyList keys_;
    std::map<int, TimeSig *> timesigs_;

    bool barLineSpan_ = false; // true - span barline to next staff
    int barLineFrom_ = 0;      // line of start staff to draw the barline from (0 = staff top line, ...)
    int barLineTo_ = 0;        // line of end staff to draw the bar line to (0= staff bottom line, ...)

    bool cutaway_ = false;
    bool showIfEntireSystemEmpty_ = false;           // show this staff if system is empty and hideEmptyStaves is true
    bool hideSystemBarLine_ = false;                 // no system barline if not preceded by staff with barline
    AutoOnOff mergeMatchingRests_ = AutoOnOff::AUTO; // merge matching rests in multiple voices
    AutoOnOff hideWhenEmpty_ = AutoOnOff::AUTO;      // hide empty staves

    Color color_;
    Spatium userDist_{0.0_sp}; ///< user edited extra distance

    StaffTypeList staffTypeList_;

    std::map<int, int> channelList_[VOICES];
    std::map<int, SwingParameters> swingMap_;
    std::map<int, CapoParams> capoMap_;
    std::map<int, int> pitchOffsetMap_;
    bool playbackVoice_[VOICES]{true, true, true, true};
    std::array<bool, VOICES> visibilityVoices_{true, true, true, true};

    bool reflectTranspositionInLinkedTab_ = true;

    AutoOnOff showMeasureNumbers_ = AutoOnOff::AUTO;
};
} // namespace mu::engraving
