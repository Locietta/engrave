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

#include "containers.h"

#include "iengravingconfiguration.h" // IWYU pragma: keep

#include "types/typesconv.h"

#include "barline.h"
#include "clef.h"
#include "cleflist.h"
#include "excerpt.h"
#include "factory.h"
#include "instrtemplate.h"
#include "linkedobjects.h"
#include "masterscore.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "ottava.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "stafftype.h"
#include "timesig.h"
#include "utils.h"

#include "../editing/editstaffbrackets.h"
#include "../editing/editcapo.h"
#include "../editing/transpose.h"

// #define DEBUG_CLEFS

#include "log.h"

using namespace mu;
using namespace mu::engraving;

#ifdef DEBUG_CLEFS
#define DUMP_CLEFS(s) dumpClefs(s)
#else
#define DUMP_CLEFS(s)
#endif

namespace mu::engraving {
//---------------------------------------------------------
//   Staff
//---------------------------------------------------------

Staff::Staff(Part *parent) : EngravingItem(ElementType::STAFF, parent) {
    color_ = configuration()->defaultColor();
    initFromStaffType(0);
}

//---------------------------------------------------------
//   copy ctor
//---------------------------------------------------------

Staff::Staff(const Staff &staff) : EngravingItem(staff) {
    init(&staff);
    part_ = staff.part_;
}

//---------------------------------------------------------
//   clone
//---------------------------------------------------------

Staff *Staff::clone() const { return new Staff(*this); }

//---------------------------------------------------------
//   idx
//---------------------------------------------------------

staff_idx_t Staff::idx() const { return muse::indexOf(score()->staves(), (Staff *) this); }

Part *Staff::originPart(const Fraction &tick) const {
    // Hides EngravingItem::originPart, whose track and tick are meaningless for a staff
    // Always returns part of voice 0
    if (!part_ || !part_->isSharedPart()) {
        return part_;
    }

    return part_->originPart(staff2track(idx(), 0), tick);
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void Staff::triggerLayout() const { score()->setLayoutAll(idx(), this); }

void Staff::triggerLayout(const Fraction &tick) { score()->setLayout(tick, idx()); }

Staff *Staff::findLinkedInScore(const Score *score) const {
    for (Staff *staff : score->staves()) {
        if (staff->id() == id()) {
            return staff;
        }
    }

    return nullptr;
}

track_idx_t Staff::getLinkedTrackInStaff(const Staff *linkedStaff, const track_idx_t originalTrack) const {
    IF_ASSERT_FAILED(linkedStaff && originalTrack != muse::nidx) { return muse::nidx; }

    Score *thisScore = score();
    Score *linkedStaffScore = linkedStaff->score();
    staff_idx_t linkedStaffIdx = linkedStaff->idx();

    if (thisScore == linkedStaffScore) {
        // For staves linked within the same score, voices are always mapped 1 to 1
        voice_idx_t voice = track2voice(originalTrack);
        return staff2track(linkedStaffIdx, voice);
    }

    // NOTE 1: if linkedStaff has a different id than *this, it means that there isn't direct voice mapping between
    // the two. Example: if we have guitar+TAB in the score, with corresponding guitar+TAB in the part, *this
    // may be the notation-staff of the score and linkedStaff may be the TAB-staff of the part. In that case, the
    // correct voice mapping must be obtained by looking for the staff with same id in thisScore.

    track_idx_t refTrack = originalTrack;
    if (linkedStaff->id() != id()) {
        Staff *correspondingStaffInThisScore = linkedStaff->findLinkedInScore(thisScore);
        if (!correspondingStaffInThisScore) {
            return muse::nidx;
        }
        voice_idx_t originalVoice = track2voice(originalTrack);
        refTrack = staff2track(correspondingStaffInThisScore->idx(), originalVoice);
    }

    // NOTE 2: TracksMap is a map from a track in the *score* to track(s) in the *part*.
    // If we are in the master score, refTrack corresponds to one of the keys in the map, so we can retrieve
    // the linked tracks by simply querying the map by key. However, if we are in a part, we need to search for
    // refTrack among the *values* of the map, and the corresponding key gives us the linked track in the score.
    // If linkedStaff is *also* in a part, we need to do it in two steps: first find the linked track in the score,
    // then use it to find the linked track in linkeStaff's part.

    if (thisScore->isMaster()) {
        const TracksMap &tracksMap = linkedStaffScore->excerpt()->tracksMapping();
        std::vector<track_idx_t> linkedTracks = muse::values(tracksMap, refTrack);
        for (track_idx_t track : linkedTracks) {
            if (track2staff(track) == linkedStaffIdx) {
                return track;
            }
        }

        return muse::nidx;
    }

    const TracksMap &thisTracksMap = thisScore->excerpt()->tracksMapping();
    track_idx_t linkedTrackInScore = muse::nidx;
    for (auto pair : thisTracksMap) {
        track_idx_t trackInScore = pair.first;
        std::vector<track_idx_t> tracksInPart = muse::values(thisTracksMap, trackInScore);
        for (track_idx_t trackInPart : tracksInPart) {
            if (trackInPart == refTrack) {
                linkedTrackInScore = trackInScore;
                break;
            }
        }
        if (linkedTrackInScore != muse::nidx) {
            break;
        }
    }

    if (linkedStaffScore->isMaster() || linkedTrackInScore == muse::nidx) {
        return linkedTrackInScore;
    }

    const TracksMap &linkedTracksMap = linkedStaffScore->excerpt()->tracksMapping();
    std::vector<track_idx_t> linkedTracks = muse::values(linkedTracksMap, linkedTrackInScore);
    for (track_idx_t track : linkedTracks) {
        if (track2staff(track) == linkedStaffIdx) {
            return track;
        }
    }

    return muse::nidx;
}

bool Staff::trackHasLinksInVoiceZero(track_idx_t track) {
    for (Staff *linkedStaff : staffList()) {
        track_idx_t linkedTrack = getLinkedTrackInStaff(linkedStaff, track);
        if (linkedTrack != muse::nidx && track2voice(linkedTrack) == 0) {
            return true;
        }
    }

    return false;
}

void Staff::undoSetShowMeasureNumbers(bool show) {
    bool isTopStave = score()->staves().front() == this;
    if (show) {
        undoChangeProperty(Pid::SHOW_MEASURE_NUMBERS, isTopStave ? AutoOnOff::AUTO : AutoOnOff::ON);
    } else {
        undoChangeProperty(Pid::SHOW_MEASURE_NUMBERS, isTopStave ? AutoOnOff::OFF : AutoOnOff::AUTO);
    }
}

bool Staff::shouldShowMeasureNumbers() const {
    MeasureNumberPlacement placementMode = style().styleV(Sid::measureNumberPlacementMode).value<MeasureNumberPlacement>();
    switch (placementMode) {
        case MeasureNumberPlacement::ABOVE_SYSTEM: return score()->staves().front() == this;
        case MeasureNumberPlacement::BELOW_SYSTEM: return score()->staves().back() == this;
        case MeasureNumberPlacement::ON_SYSTEM_OBJECT_STAVES: {
            bool isTopStave = score()->staves().front() == this;
            return (isTopStave && showMeasureNumbers_ != AutoOnOff::OFF) || (isSystemObjectStaff() && showMeasureNumbers_ == AutoOnOff::ON);
        }
        case MeasureNumberPlacement::ON_ALL_STAVES: return show();
    }

    return false;
}

bool Staff::isLastOfScore() const { return score()->staves().empty() ? false : score()->staves().back() == this; }

bool Staff::isSystemObjectStaff() const { return score() && muse::contains(score()->systemObjectStaves(), const_cast<Staff *>(this)); }

bool Staff::hasSystemObjectsBelowBottomStaff() const {
    return isSystemObjectStaff() && isLastOfScore() && style().styleB(Sid::systemObjectsBelowBottomStaff);
}

String Staff::individualStaffNameLong(const Fraction &tick) const {
    IF_ASSERT_FAILED(part_) { return String(); }

    return staffType(tick)->longName();
}

String Staff::individualStaffNameShort(const Fraction &tick) const {
    IF_ASSERT_FAILED(part_) { return String(); }

    return staffType(tick)->shortName();
}

bool Staff::playbackVoice(int voice) const { return playbackVoice_[voice]; }

void Staff::setPlaybackVoice(int voice, bool val) { playbackVoice_[voice] = val; }

const std::array<bool, VOICES> &Staff::visibilityVoices() const { return visibilityVoices_; }

bool Staff::isVoiceVisible(voice_idx_t voice) const {
    if (voice >= VOICES) {
        return false;
    }

    return visibilityVoices_[voice];
}

void Staff::setVoiceVisible(voice_idx_t voice, bool visible) {
    if (voice >= VOICES) {
        return;
    }

    visibilityVoices_[voice] = visible;
}

bool Staff::canDisableVoice() const {
    auto voices = visibilityVoices();
    int countOfVisibleVoices = 0;
    for (size_t i = 0; i < voices.size(); ++i) {
        if (voices[i]) {
            countOfVisibleVoices++;
        }
    }

    return countOfVisibleVoices > 1;
}

void Staff::updateVisibilityVoices(const Staff *masterStaff, const TracksMap &tracks) {
    if (tracks.empty()) {
        visibilityVoices_ = {true, true, true, true};
        return;
    }

    std::array<bool, VOICES> voices{false, false, false, false};

    staff_idx_t masterStaffIdx = masterStaff->idx();
    staff_idx_t staffIdx = idx();

    voice_idx_t voiceIndex = 0;
    for (voice_idx_t voice = 0; voice < VOICES; voice++) {
        std::vector<track_idx_t> masterStaffTracks = muse::values(tracks, masterStaffIdx * VOICES + voice % VOICES);
        bool isVoiceVisible = muse::contains(masterStaffTracks, staffIdx * VOICES + voiceIndex % VOICES);
        if (isVoiceVisible) {
            voices[voice] = true;
            voiceIndex++;
        }
    }

    visibilityVoices_ = voices;
}

bool Staff::reflectTranspositionInLinkedTab() const { return reflectTranspositionInLinkedTab_; }

void Staff::setReflectTranspositionInLinkedTab(bool reflect) { reflectTranspositionInLinkedTab_ = reflect; }

//---------------------------------------------------------
//   partName
//---------------------------------------------------------

String Staff::partName() const { return part_->partName(); }

//---------------------------------------------------------
//   Staff::clefType
//---------------------------------------------------------

ClefTypeList Staff::clefType(const Fraction &tick) const {
    ClefTypeList ct = clefs_.clef(tick.ticks());
    if (ct.concertClef == ClefType::INVALID) {
        // Clef compatibility based on instrument (override StaffGroup)
        StaffGroup staffGroup = staffType(tick)->group();
        if (staffGroup != StaffGroup::TAB) {
            staffGroup = originPart(tick)->instrument(tick)->useDrumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
        }

        switch (staffGroup) {
            case StaffGroup::TAB: {
                ClefType sct = ClefType(style().styleI(Sid::tabClef));
                ct = staffType(tick)->lines() <= 4 ? ClefTypeList(sct == ClefType::TAB ? ClefType::TAB4 : ClefType::TAB4_SERIF) :
                                                     ClefTypeList(sct == ClefType::TAB ? ClefType::TAB : ClefType::TAB_SERIF);
            } break;
            case StaffGroup::STANDARD: ct = defaultClefType(); break;
            case StaffGroup::PERCUSSION: ct = ClefTypeList(ClefType::PERC); break;
        }
    }
    return ct;
}

//---------------------------------------------------------
//   Staff::clef
//---------------------------------------------------------

ClefType Staff::clef(const Fraction &tick) const {
    ClefTypeList c = clefType(tick);
    return style().styleB(Sid::concertPitch) ? c.concertClef : c.transposingClef;
}

//---------------------------------------------------------
//   Staff::nextClefTick
//
//    return the tick of next clef after tick
//    return last tick of score if not found
//---------------------------------------------------------

Fraction Staff::nextClefTick(const Fraction &tick) const {
    Fraction t = Fraction::fromTicks(clefs_.nextClefTick(tick.ticks()));
    return t != Fraction(-1, 1) ? t : score()->endTick();
}

//---------------------------------------------------------
//   Staff::currentClefTick
//
//    return the tick position of the clef currently
//    in effect at tick
//    return 0, if no such clef
//---------------------------------------------------------

Fraction Staff::currentClefTick(const Fraction &tick) const { return Fraction::fromTicks(clefs_.currentClefTick(tick.ticks())); }

String Staff::staffName() const { return TConv::translatedUserName(clefType(Fraction()).transposingClef); }

#ifndef NDEBUG
//---------------------------------------------------------
//   dumpClef
//---------------------------------------------------------

void Staff::dumpClefs(const char *title) const {
    LOGD("(%zd): %s", clefs_.size(), title);
    for (auto &i : clefs_) {
        LOGD("  %d: %d %d", i.first, int(i.second.concertClef), int(i.second.transposingClef));
    }
}

//---------------------------------------------------------
//   dumpKeys
//---------------------------------------------------------

void Staff::dumpKeys(const char *title) const {
    LOGD("(%zd): %s", keys_.size(), title);
    for (auto &i : keys_) {
        LOGD("  %d: %d", i.first, int(i.second.key()));
    }
}

//---------------------------------------------------------
//   dumpTimeSigs
//---------------------------------------------------------

void Staff::dumpTimeSigs(const char *title) const {
    LOGD("size (%zd) staffIdx %zu: %s", timesigs_.size(), idx(), title);
    for (auto &i : timesigs_) {
        LOGD("  %d: %d/%d", i.first, i.second->sig().numerator(), i.second->sig().denominator());
    }
}

#endif

//---------------------------------------------------------
//   setClef
//---------------------------------------------------------

void Staff::setClef(Clef *clef) {
    if (clef->generated()) {
        return;
    }
    Fraction tick = clef->segment()->tick();
    for (Segment *s = clef->segment()->next1(); s && s->tick() == tick; s = s->next1()) {
        if ((s->segmentType() == SegmentType::Clef || s->segmentType() == SegmentType::HeaderClef) && s->element(clef->track()) &&
            !s->element(clef->track())->generated()) {
            // adding this clef has no effect on the clefs list
            return;
        }
    }
    clefs_.setClef(clef->segment()->tick().ticks(), clef->clefTypeList());
    DUMP_CLEFS("setClef");
}

//---------------------------------------------------------
//   removeClef
//---------------------------------------------------------

void Staff::removeClef(const Clef *clef) {
    if (clef->generated()) {
        return;
    }
    Fraction tick = clef->segment()->tick();
    for (Segment *s = clef->segment()->next1(); s && s->tick() == tick; s = s->next1()) {
        if ((s->segmentType() == SegmentType::Clef || s->segmentType() == SegmentType::HeaderClef) && s->element(clef->track()) &&
            !s->element(clef->track())->generated()) {
            // removal of this clef has no effect on the clefs list
            return;
        }
    }
    clefs_.erase(clef->segment()->tick().ticks());
    for (Segment *s = clef->segment()->prev1(); s && s->tick() == tick; s = s->prev1()) {
        if ((s->segmentType() == SegmentType::Clef || s->segmentType() == SegmentType::HeaderClef) && s->element(clef->track()) &&
            !s->element(clef->track())->generated()) {
            // a previous clef at the same tick position gets valid
            clefs_.setClef(tick.ticks(), toClef(s->element(clef->track()))->clefTypeList());
            break;
        }
    }
    DUMP_CLEFS("removeClef");
}

//---------------------------------------------------------
//   timeStretch
//---------------------------------------------------------

Fraction Staff::timeStretch(const Fraction &tick) const {
    TimeSig *timesig = timeSig(tick);
    return timesig ? timesig->stretch() : Fraction(1, 1);
}

//---------------------------------------------------------
//   timeSig
//    lookup time signature before or at tick
//---------------------------------------------------------

TimeSig *Staff::timeSig(const Fraction &tick) const {
    auto i = timesigs_.upper_bound(tick.ticks());
    if (i != timesigs_.begin()) {
        --i;
    }
    if (i == timesigs_.end()) {
        return 0;
    } else if (tick < Fraction::fromTicks(i->first)) {
        return 0;
    }
    return i->second;
}

//---------------------------------------------------------
//   nextTimeSig
//    lookup time signature at tick or after
//---------------------------------------------------------

TimeSig *Staff::nextTimeSig(const Fraction &tick) const {
    auto i = timesigs_.lower_bound(tick.ticks());
    return (i == timesigs_.end()) ? 0 : i->second;
}

//---------------------------------------------------------
//   currentTimeSigTick
//
//    return the tick position of the time sig currently
//    in effect at tick
//---------------------------------------------------------

Fraction Staff::currentTimeSigTick(const Fraction &tick) const {
    if (timesigs_.empty()) {
        return Fraction(0, 1);
    }
    auto i = timesigs_.upper_bound(tick.ticks());
    if (i == timesigs_.begin()) {
        return Fraction(0, 1);
    }
    --i;
    return Fraction::fromTicks(i->first);
}

//---------------------------------------------------------
//   group
//---------------------------------------------------------

const Groups &Staff::group(const Fraction &tick) const {
    TimeSig *ts = timeSig(tick);
    if (ts) {
        if (!ts->groups().empty()) {
            return ts->groups();
        }
        return Groups::endings(ts->sig());
    }
    Measure *m = score()->tick2measure(tick);
    return Groups::endings(m ? m->timesig() : Fraction(4, 4));
}

//---------------------------------------------------------
//   addTimeSig
//---------------------------------------------------------

void Staff::addTimeSig(TimeSig *timesig) {
    if (timesig->segment()->segmentType() == SegmentType::TimeSig) {
        timesigs_[timesig->segment()->tick().ticks()] = timesig;
    }
    //      dumpTimeSigs("after addTimeSig");
}

//---------------------------------------------------------
//   removeTimeSig
//---------------------------------------------------------

void Staff::removeTimeSig(TimeSig *timesig) {
    if (timesig->segment()->segmentType() == SegmentType::TimeSig) {
        if (timesigs_[timesig->segment()->tick().ticks()] == timesig) {
            timesigs_.erase(timesig->segment()->tick().ticks());
        }
    }
    //      dumpTimeSigs("after removeTimeSig");
}

//---------------------------------------------------------
//   clearTimeSig
//---------------------------------------------------------

void Staff::clearTimeSig() { timesigs_.clear(); }

//---------------------------------------------------------
//   Staff::transpose
//
//    actual staff transposioton at tick
//    (taking key into account)
//---------------------------------------------------------

Interval Staff::transpose(const Fraction &tick) const {
    // get real transposition

    const Part *p = originPart(tick);
    Interval v = p->instrument(tick)->transpose();
    if (v.isZero()) {
        return v;
    }
    Key cKey = concertKey(tick);
    v.flip();
    Key tKey = Transpose::transposeKey(cKey, v, p->preferSharpFlat());
    v.flip();

    int chromatic = (7 * (static_cast<int>(cKey) - static_cast<int>(tKey))) % 12;
    if (chromatic < 0) {
        chromatic += 12;
    }
    int diatonic = (4 * (static_cast<int>(cKey) - static_cast<int>(tKey))) % 7;
    if (diatonic < 0) {
        diatonic += 7;
    }

    if (v.chromatic < 0 || v.diatonic < 0) {
        chromatic -= 12;
        diatonic -= 7;
    }

    v.chromatic = v.chromatic - (v.chromatic % 12) + (chromatic % 12);
    v.diatonic = v.diatonic - (v.diatonic % 7) + (diatonic % 7);

    return v;
}

//---------------------------------------------------------
//   Staff::keySigEvent
//
//    locates the key sig currently in effect at tick
//---------------------------------------------------------

KeySigEvent Staff::keySigEvent(const Fraction &tick) const { return keys_.key(tick.ticks()); }

//---------------------------------------------------------
//   setKey
//---------------------------------------------------------

void Staff::setKey(const Fraction &tick, KeySigEvent k) { keys_.setKey(tick.ticks(), k); }

//---------------------------------------------------------
//   removeKey
//---------------------------------------------------------

void Staff::removeKey(const Fraction &tick) { keys_.erase(tick.ticks()); }

//---------------------------------------------------------
//   prevkey
//---------------------------------------------------------

KeySigEvent Staff::prevKey(const Fraction &tick) const { return keys_.prevKey(tick.ticks()); }

//---------------------------------------------------------
//   Staff::nextKeyTick
//
//    return the tick at which the key sig after tick is located
//    return 0, if no such a key sig
//---------------------------------------------------------

Fraction Staff::nextKeyTick(const Fraction &tick) const {
    Fraction t = Fraction::fromTicks(keys_.nextKeyTick(tick.ticks()));
    return t != Fraction(-1, 1) ? t : score()->endTick();
}

//---------------------------------------------------------
//   Staff::currentKeyTick
//
//    return the tick position of the key currently
//    in effect at tick
//    return 0, if no such a key sig
//---------------------------------------------------------

Fraction Staff::currentKeyTick(const Fraction &tick) const { return Fraction::fromTicks(keys_.currentKeyTick(tick.ticks())); }

//---------------------------------------------------------
//   height
//---------------------------------------------------------

double Staff::staffHeight() const {
    Fraction tick = Fraction(0, 1);
    return (lines(tick) - 1) * spatium(tick) * staffType(tick)->lineDistance().val();
}

double Staff::staffHeight(const Fraction &tick) const { return (lines(tick) - 1) * spatium(tick) * staffType(tick)->lineDistance().val(); }

//---------------------------------------------------------
//   spatium
//---------------------------------------------------------

double Staff::spatium(const Fraction &tick) const { return style().spatium() * staffMag(tick); }

double Staff::spatium(const EngravingItem *e) const { return style().spatium() * staffMag(e); }

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Staff::staffMag(const StaffType *stt) const { return (stt->isSmall() ? style().styleD(Sid::smallStaffMag) : 1.0) * stt->userMag(); }

double Staff::staffMag(const Fraction &tick) const { return staffMag(staffType(tick)); }

double Staff::staffMag(const EngravingItem *element) const { return staffMag(staffTypeForElement(element)); }

//---------------------------------------------------------
//   swing
//---------------------------------------------------------

SwingParameters Staff::swing(const Fraction &tick) const {
    SwingParameters sp;
    int swingUnit = 0;
    muse::ByteArray ba = style().styleSt(Sid::swingUnit).toAscii();
    DurationType unit = TConv::fromXml(ba.constChar(), DurationType::V_INVALID);
    int swingRatio = style().styleI(Sid::swingRatio);
    if (unit == DurationType::V_EIGHTH) {
        swingUnit = Constants::DIVISION / 2;
    } else if (unit == DurationType::V_16TH) {
        swingUnit = Constants::DIVISION / 4;
    } else if (unit == DurationType::V_ZERO) {
        swingUnit = 0;
    }
    sp.swingRatio = swingRatio;
    sp.swingUnit = swingUnit;
    if (swingMap_.empty()) {
        return sp;
    }

    auto it = muse::findLessOrEqual(swingMap_, tick.ticks());
    return it == swingMap_.cend() ? sp : it->second;
}

const CapoParams &Staff::capo(const Fraction &tick) const {
    static const CapoParams dummy;
    if (capoMap_.empty()) {
        return dummy;
    }

    auto it = muse::findLessOrEqual(capoMap_, tick.ticks());
    return it == capoMap_.cend() ? dummy : it->second;
}

void Staff::insertCapoParams(const Fraction &tick, const CapoParams &params, bool ignoreNotationUpdate) {
    if (ignoreNotationUpdate) {
        capoMap_.insert_or_assign(tick.ticks(), params);
        return;
    }
    auto isNeedUpdate = [](const CapoParams &oldParams, const CapoParams &newParams) -> bool {
        return !(oldParams.active == newParams.active && oldParams.transposeMode == newParams.transposeMode &&
                 oldParams.fretPosition == newParams.fretPosition && oldParams.ignoredStrings == newParams.ignoredStrings);
    };

    int startTick = tick.ticks();
    int endTick = -1;

    if (auto it = capoMap_.find(startTick); it == capoMap_.end()) {
        auto result = capoMap_.insert({startTick, params});
        if (const auto nextIt = std::next(result.first); nextIt != capoMap_.end()) {
            endTick = nextIt->first;
        }
        if (result.first != capoMap_.begin()) {
            const auto prevIt = std::prev(result.first);
            CapoParams oldParams = prevIt->second;
            // We don't need to apply any changes if the previous capo is inactive
            if (oldParams.active) {
                EditCapo::updateNotationForCapoChange(oldParams, params, this, startTick, endTick);
            }
            // This is an undo action
        } else if (CapoParams::TransposeMode::PLAYBACK_ONLY != params.transposeMode && params.active) {
            CapoParams oldParams;
            oldParams.transposeMode = CapoParams::TransposeMode::PLAYBACK_ONLY;
            oldParams.fretPosition = params.fretPosition;
            oldParams.active = params.active;
            oldParams.ignoredStrings = params.ignoredStrings;
            EditCapo::updateNotationForCapoChange(oldParams, params, this, startTick, endTick);
        }
    } else {
        CapoParams oldParams = it->second;
        if (!isNeedUpdate(oldParams, params)) {
            return;
        }
        auto result = capoMap_.insert_or_assign(startTick, params);
        if (const auto nextIt = std::next(result.first); nextIt != capoMap_.end()) {
            endTick = nextIt->first;
        }
        EditCapo::updateNotationForCapoChange(oldParams, params, this, startTick, endTick);
    }
}

void Staff::removeCapoParams(const mu::engraving::Fraction &tick) {
    const int startTick = tick.ticks();

    const auto it = capoMap_.find(startTick);
    IF_ASSERT_FAILED(it != capoMap_.end()) {
        LOGE() << "Key must exist in capo map!";
        return;
    }

    const auto nextCapoIt = std::next(it);
    const int endTick = nextCapoIt == capoMap_.end() ? -1 : nextCapoIt->first;

    CapoParams revertParams;
    revertParams.fretPosition = it->second.fretPosition;

    CapoParams oldParams = it->second;
    // If this capo is inactive, it is treated as PLAYBACK_ONLY mode capo
    if (!oldParams.active) {
        oldParams.transposeMode = CapoParams::TransposeMode::PLAYBACK_ONLY;
    }

    if (it != capoMap_.begin()) {
        revertParams = std::prev(it)->second;
        // If not active, treat as PLAYBACK_ONLY
        if (!revertParams.active) {
            revertParams.transposeMode = CapoParams::TransposeMode::PLAYBACK_ONLY;
        }
    }
    EditCapo::updateNotationForCapoChange(oldParams, revertParams, this, startTick, endTick);

    capoMap_.erase(startTick);
}

bool Staff::shouldMergeMatchingRests() const {
    return mergeMatchingRests() == AutoOnOff::ON ||
           (mergeMatchingRests() == AutoOnOff::AUTO && style().value(Sid::mergeMatchingRests).toBool());
}

//---------------------------------------------------------
//   channel
//---------------------------------------------------------

int Staff::channel(const Fraction &tick, voice_idx_t voice) const {
    const std::map<int, int> &map = channelList_[voice];
    if (map.empty()) {
        return 0;
    }

    auto it = muse::findLessOrEqual(map, tick.ticks());
    return it == map.cend() ? 0 : it->second;
}

//---------------------------------------------------------
//   middleLine
//    returns logical line number of middle staff line
//---------------------------------------------------------

int Staff::middleLine(const Fraction &tick) const { return staffType(tick)->middleLine(); }

//---------------------------------------------------------
//   bottomLine
//    returns logical line number of bottom staff line
//---------------------------------------------------------

int Staff::bottomLine(const Fraction &tick) const { return staffType(tick)->bottomLine(); }

//---------------------------------------------------------
//   stemless
//---------------------------------------------------------

bool Staff::stemless(const Fraction &tick) const { return staffType(tick)->stemless(); }

//---------------------------------------------------------
//   setSlashStyle
//---------------------------------------------------------

void Staff::setSlashStyle(const Fraction &tick, bool val) { staffType(tick)->setStemless(val); }

//---------------------------------------------------------
//   isPrimaryStaff
///   if there are linked staves, the primary staff is
///   the one who is played back and it's not a tab staff
///   because we don't have enough information  to play
///   e.g ornaments. NOTE: it's not necessarily the top staff!
//---------------------------------------------------------

bool Staff::isPrimaryStaff() const {
    if (!links_) {
        return true;
    }

    std::vector<const Staff *> linkedStavesInThisScore;
    std::vector<const Staff *> linkedNonTabStavesInThisScore;

    for (const EngravingObject *linked : *links_) {
        const Staff *staff = toStaff(linked);

        if (staff->score() == score()) {
            linkedStavesInThisScore.push_back(staff);

            if (!staff->isTabStaff(Fraction(0, 1))) {
                linkedNonTabStavesInThisScore.push_back(staff);
            }
        }
    }

    IF_ASSERT_FAILED(!linkedStavesInThisScore.empty()) { return true; }

    if (!linkedNonTabStavesInThisScore.empty()) {
        return linkedNonTabStavesInThisScore.front() == this;
    }

    return linkedStavesInThisScore.front() == this;
}

//---------------------------------------------------------
//   staffType
//---------------------------------------------------------

const StaffType *Staff::staffType(const Fraction &tick) const { return &staffTypeList_.staffType(tick); }

const StaffType *Staff::constStaffType(const Fraction &tick) const { return &staffTypeList_.staffType(tick); }

StaffType *Staff::staffType(const Fraction &tick) { return &staffTypeList_.staffType(tick); }

const StaffType *Staff::staffTypeForElement(const EngravingItem *e) const {
    if (staffTypeList_.uniqueStaffType()) {
        // if one staff type spans for the entire staff, optimize by omitting a call to `tick()`
        return &staffTypeList_.staffType({0, 1});
    }
    // Handle items at the last tick of measures as StaffTypeList::staffType rounds up
    const Measure *measure = e->findMeasure();
    const Fraction tick = measure ? measure->tick() : e->tick();
    return &staffTypeList_.staffType(tick);
}

bool Staff::isStaffTypeStartFrom(const Fraction &tick) const { return staffTypeList_.isStaffTypeStartFrom(tick); }

void Staff::moveStaffType(const Fraction &from, const Fraction &to) {
    staffTypeList_.moveStaffType(from, to);
    staffTypeListChanged(from);
}

std::pair<int, int> Staff::staffTypeRange(const Fraction &tick) const { return staffTypeList_.staffTypeRange(tick); }

//---------------------------------------------------------
//   staffTypeListChanged
//    Signal that the staffTypeList has changed at
//    position tick. Update layout range.
//---------------------------------------------------------

void Staff::staffTypeListChanged(const Fraction &tick) {
    std::pair<int, int> range = staffTypeList_.staffTypeRange(tick);

    if (range.first < 0) {
        triggerLayout(Fraction(0, 1));
    } else {
        triggerLayout(Fraction::fromTicks(range.first));
    }

    if (range.second < 0) {
        // When reading a score and there is a Staff Change on the first
        // measure, there are no measures yet and nothing to layout.
        if (score()->lastMeasure()) {
            triggerLayout(score()->lastMeasure()->endTick());
        }
    } else {
        triggerLayout(Fraction::fromTicks(range.second));
    }
}

//---------------------------------------------------------
//   setStaffType
//---------------------------------------------------------

StaffType *Staff::setStaffType(const Fraction &tick, const StaffType &nst) {
    StaffType *stt = staffTypeList_.setStaffType(tick, nst);
    stt->setScore(score());
    return stt;
}

//---------------------------------------------------------
//   setStaffType
//---------------------------------------------------------

void Staff::removeStaffType(const Fraction &tick) {
    double old = spatium(tick);
    const bool removed = staffTypeList_.removeStaffType(tick);
    if (!removed) {
        return;
    }
    setLocalSpatium(old, spatium(tick), tick);
    staffTypeListChanged(tick);
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void Staff::init(const InstrumentTemplate *t, const StaffType *staffType, staff_idx_t templateStaffIdx) {
    // Initialises a staff from an InstrumentTemplate and StaffType. The staff MUST be added to the score already.
    assert(idx() != muse::nidx);
    // set staff-type-independent parameters
    const StaffType *pst = staffType ? staffType : t->staffTypePreset;
    if (!pst) {
        pst = StaffType::getDefaultPreset(t->staffGroup);
    }

    const staff_idx_t staffIdx = idx() != muse::nidx ? idx() : score()->nstaves();

    StaffType *stt = setStaffType(Fraction(0, 1), *pst);
    if (templateStaffIdx >= MAX_STAVES) {
        stt->setSmall(false);
    } else {
        stt->setSmall(t->smallStaff[templateStaffIdx]);
        EditStaffBrackets::setBracketType(score(), staffIdx, 0, t->bracket[templateStaffIdx]);
        EditStaffBrackets::setBracketSpan(score(), staffIdx, 0, t->bracketSpan[templateStaffIdx]);
        setBarLineSpan(t->barlineSpan[templateStaffIdx]);
    }
    setDefaultClefType(t->clefType(templateStaffIdx));
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void Staff::init(const Staff *s) {
    id_ = s->id_;

    setStaffType(Fraction(0, 1), s->staffTypeList_.staffType(Fraction(0, 1)));
    for (const auto &stPair : s->staffTypeList_.staffTypeChanges()) {
        const StaffType &st = stPair.second;
        StaffType newStaffType(st);
        setStaffType(Fraction::fromTicks(stPair.first), newStaffType);
    }

    setDefaultClefType(s->defaultClefType());
    barLineFrom_ = s->barLineFrom_;
    barLineTo_ = s->barLineTo_;
    hideWhenEmpty_ = s->hideWhenEmpty_;
    cutaway_ = s->cutaway_;
    showIfEntireSystemEmpty_ = s->showIfEntireSystemEmpty_;
    hideSystemBarLine_ = s->hideSystemBarLine_;
    mergeMatchingRests_ = s->mergeMatchingRests_;
    color_ = s->color_;
    userDist_ = s->userDist_;
    visibilityVoices_ = s->visibilityVoices_;
    capoMap_ = s->capoMap_;
}

const ID &Staff::id() const { return id_; }

void Staff::setId(const ID &id) { id_ = id; }

//---------------------------------------------------------
//   initFromStaffType
//---------------------------------------------------------

void Staff::initFromStaffType(const StaffType *staffType) {
    // get staff type if given (if none, get default preset for default staff group)
    if (!staffType) {
        staffType = StaffType::getDefaultPreset(StaffGroup::STANDARD);
    }

    // use selected staff type
    setStaffType(Fraction(0, 1), *staffType);
}

//---------------------------------------------------------
//   show
//---------------------------------------------------------

bool Staff::show() const { return part_->show() && visible(); }

//---------------------------------------------------------
//   genKeySig
//---------------------------------------------------------

bool Staff::genKeySig() {
    if (constStaffType(Fraction(0, 1))->group() == StaffGroup::TAB) {
        return false;
    } else {
        return constStaffType(Fraction(0, 1))->genKeysig();
    }
}

//---------------------------------------------------------
//   showLedgerLines
//---------------------------------------------------------

bool Staff::showLedgerLines(const Fraction &tick) const { return staffType(tick)->showLedgerLines(); }

//---------------------------------------------------------
//   color
//---------------------------------------------------------

Color Staff::color(const Fraction &tick) const { return staffType(tick)->color(); }

//---------------------------------------------------------
//   setColor
//---------------------------------------------------------

void Staff::setColor(const Fraction &tick, const Color &val) { staffType(tick)->setColor(val); }

//---------------------------------------------------------
//   updateOttava
//---------------------------------------------------------

void Staff::updateOttava() {
    staff_idx_t staffIdx = idx();
    pitchOffsetMap_.clear();
    for (auto i : score()->spanner()) {
        const Spanner *s = i.second;
        if (s->isOttava() && s->staffIdx() == staffIdx && s->playSpanner()) {
            const Ottava *o = toOttava(s);
            pitchOffsetMap_.insert_or_assign(o->tick().ticks(), o->pitchShift());
            pitchOffsetMap_.insert_or_assign(o->tick2().ticks(), 0);
        }
    }
}

int Staff::pitchOffset(const Fraction &tick) const {
    if (pitchOffsetMap_.empty()) {
        return 0;
    }

    const auto it = muse::findLessOrEqual(pitchOffsetMap_, tick.ticks());
    return it == pitchOffsetMap_.cend() ? 0 : it->second;
}

//---------------------------------------------------------
//   insertTime
//---------------------------------------------------------

void Staff::insertTime(const Fraction &tick, const Fraction &len) {
    if (len.isZero()) {
        return;
    }

    // move all keys and clefs >= tick

    if (len < Fraction(0, 1)) {
        // remove entries between tickpos >= tick and tickpos < (tick+len)
        keys_.erase(keys_.lower_bound(tick.ticks()), keys_.lower_bound((tick - len).ticks()));
        clefs_.erase(clefs_.lower_bound(tick.ticks()), clefs_.lower_bound((tick - len).ticks()));
    }

    KeyList kl2;
    for (auto i = keys_.lower_bound(tick.ticks()); i != keys_.end();) {
        KeySigEvent kse = i->second;
        Fraction t = Fraction::fromTicks(i->first);
        if (t + len < score()->endTick()) {
            keys_.erase(i++);
            kl2[(t + len).ticks()] = kse;
        } else {
            ++i;
        }
    }
    keys_.insert(kl2.begin(), kl2.end());

    // check if there is a clef at the end of measure
    // before tick
    Clef *clef = 0;
    Measure *m = score()->tick2measure(tick);
    if (m && (m->tick() == tick) && (m->prevMeasure())) {
        m = m->prevMeasure();
        Segment *s = m->findSegment(SegmentType::Clef, tick);
        if (s) {
            track_idx_t track = idx() * VOICES;
            clef = toClef(s->element(track));
        }
    }

    ClefList cl2;
    for (auto i = clefs_.lower_bound(tick.ticks()); i != clefs_.end();) {
        ClefTypeList ctl = i->second;
        Fraction t = Fraction::fromTicks(i->first);
        if (clef && tick == t) {
            ++i;
            continue;
        }
        clefs_.erase(i++);
        cl2.setClef((t + len).ticks(), ctl);
    }
    clefs_.insert(cl2.begin(), cl2.end());

    // check if there is a clef at the end of measure
    // before tick: do not remove from clefs list

    if (clef) {
        setClef(clef);
    }

    updateOttava();
    DUMP_CLEFS("  insertTime");
}

//---------------------------------------------------------
//   staffList
//    return list of linked staves
//---------------------------------------------------------

std::vector<Staff *> Staff::staffList() const {
    std::vector<Staff *> staffList;
    if (links_) {
        staffList.reserve(links_->size());
        for (EngravingObject *e : *links_) {
            staffList.push_back(toStaff(e));
        }
    } else {
        staffList.push_back(const_cast<Staff *>(this));
    }
    return staffList;
}

Staff *Staff::primaryStaff() const {
    const LinkedObjects *linkedElements = links();
    if (!linkedElements) {
        return nullptr;
    }

    for (auto linkedElement : *linkedElements) {
        Staff *staff = toStaff(linkedElement);
        if (staff && staff->isPrimaryStaff()) {
            return staff;
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   rstaff
//---------------------------------------------------------

staff_idx_t Staff::rstaff() const { return muse::indexOf(part_->staves(), this); }

//---------------------------------------------------------
//   isTop
//---------------------------------------------------------

bool Staff::isTop() const {
    if (part_->staves().empty()) {
        return false;
    }

    return part_->staves().front() == this;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Staff::getProperty(Pid id) const {
    switch (id) {
        case Pid::SMALL: return staffType(Fraction(0, 1))->isSmall();
        case Pid::MAG: return staffType(Fraction(0, 1))->userMag();
        case Pid::LINE_DISTANCE: return staffType(Fraction(0, 1))->lineDistance();
        case Pid::STAFF_INVISIBLE: return staffType(Fraction(0, 1))->invisible();
        case Pid::HIDE_WHEN_EMPTY: return hideWhenEmpty_;
        case Pid::STAFF_COLOR: return PropertyValue::fromValue(staffType(Fraction(0, 1))->color());
        case Pid::PLAYBACK_VOICE1: return playbackVoice(0);
        case Pid::PLAYBACK_VOICE2: return playbackVoice(1);
        case Pid::PLAYBACK_VOICE3: return playbackVoice(2);
        case Pid::PLAYBACK_VOICE4: return playbackVoice(3);
        case Pid::STAFF_BARLINE_SPAN: return barLineSpan();
        case Pid::STAFF_BARLINE_SPAN_FROM: return barLineFrom();
        case Pid::STAFF_BARLINE_SPAN_TO: return barLineTo();
        case Pid::STAFF_USERDIST: return userDist();
        case Pid::GENERATED: return false;
        case Pid::SHOW_MEASURE_NUMBERS: return showMeasureNumbers_;
        case Pid::SHOW_IF_ENTIRE_SYSTEM_EMPTY: return showIfEntireSystemEmpty_;
        case Pid::VISIBLE: return visible();
        case Pid::STAFF_CUTAWAY: return cutaway();
        case Pid::STAFF_HIDE_SYSTEM_BARLINE: return hideSystemBarLine();
        case Pid::STAFF_MERGE_MATCHING_RESTS: return int(mergeMatchingRests());
        case Pid::STAFF_REFLECT_TRANSPOSITION: return reflectTranspositionInLinkedTab();
        default: LOGD("unhandled id <%s>", propertyName(id)); return PropertyValue();
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Staff::setProperty(Pid id, const PropertyValue &v) {
    switch (id) {
        case Pid::SMALL: {
            double _spatium = spatium(Fraction(0, 1));
            staffType(Fraction(0, 1))->setSmall(v.toBool());
            setLocalSpatium(_spatium, spatium(Fraction(0, 1)), Fraction(0, 1));
            break;
        }
        case Pid::MAG: {
            double _spatium = spatium(Fraction(0, 1));
            staffType(Fraction(0, 1))->setUserMag(v.toReal());
            setLocalSpatium(_spatium, spatium(Fraction(0, 1)), Fraction(0, 1));
            break;
        }
        case Pid::LINE_DISTANCE: {
            staffType(Fraction(0, 1))->setLineDistance(v.value<Spatium>());
            break;
        }
        case Pid::HIDE_WHEN_EMPTY: setHideWhenEmpty(v.value<AutoOnOff>()); break;
        case Pid::STAFF_COLOR: setColor(Fraction(0, 1), v.value<Color>()); break;
        case Pid::PLAYBACK_VOICE1: setPlaybackVoice(0, v.toBool()); break;
        case Pid::PLAYBACK_VOICE2: setPlaybackVoice(1, v.toBool()); break;
        case Pid::PLAYBACK_VOICE3: setPlaybackVoice(2, v.toBool()); break;
        case Pid::PLAYBACK_VOICE4: setPlaybackVoice(3, v.toBool()); break;
        case Pid::STAFF_BARLINE_SPAN: {
            setBarLineSpan(v.toBool());
            // update non-generated barlines
            track_idx_t track = idx() * VOICES;
            std::vector<EngravingItem *> blList;
            for (Measure *m = score()->firstMeasure(); m; m = m->nextMeasure()) {
                Segment *s = m->getSegmentR(SegmentType::EndBarLine, m->ticks());
                if (s && s->element(track)) {
                    blList.push_back(s->element(track));
                }
                if (Measure *mm = m->mmRest()) {
                    Segment *ss = mm->getSegmentR(SegmentType::EndBarLine, mm->ticks());
                    if (ss && ss->element(track)) {
                        blList.push_back(ss->element(track));
                    }
                }
            }
            for (EngravingItem *e : blList) {
                if (e && e->isBarLine() && !e->generated()) {
                    toBarLine(e)->setSpanStaff(barLineSpan());
                }
            }
        } break;
        case Pid::STAFF_BARLINE_SPAN_FROM: setBarLineFrom(v.toInt()); break;
        case Pid::STAFF_BARLINE_SPAN_TO: setBarLineTo(v.toInt()); break;
        case Pid::STAFF_USERDIST: setUserDist(v.value<Spatium>()); break;
        case Pid::SHOW_MEASURE_NUMBERS: showMeasureNumbers_ = v.value<AutoOnOff>(); break;
        case Pid::SHOW_IF_ENTIRE_SYSTEM_EMPTY: showIfEntireSystemEmpty_ = v.toBool(); break;
        case Pid::VISIBLE:
            setVisible(v.toBool());
            masterScore()->rebuildMidiMapping();
            break;
        case Pid::STAFF_CUTAWAY: setCutaway(v.toBool()); break;
        case Pid::STAFF_HIDE_SYSTEM_BARLINE: setHideSystemBarLine(v.toBool()); break;
        case Pid::STAFF_MERGE_MATCHING_RESTS: setMergeMatchingRests(AutoOnOff(v.toInt())); break;
        case Pid::STAFF_REFLECT_TRANSPOSITION: setReflectTranspositionInLinkedTab(v.toBool()); break;
        default: LOGD("unhandled id <%s>", propertyName(id)); break;
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Staff::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::SMALL: return false;
        case Pid::MAG: return 1.0;
        case Pid::HIDE_WHEN_EMPTY: return AutoOnOff::AUTO;
        case Pid::STAFF_COLOR: return PropertyValue::fromValue(configuration()->defaultColor());
        case Pid::PLAYBACK_VOICE1:
        case Pid::PLAYBACK_VOICE2:
        case Pid::PLAYBACK_VOICE3:
        case Pid::PLAYBACK_VOICE4: return true;
        case Pid::STAFF_BARLINE_SPAN: return false;
        case Pid::STAFF_BARLINE_SPAN_FROM:
        case Pid::STAFF_BARLINE_SPAN_TO: return 0;
        case Pid::STAFF_USERDIST: return 0.0_sp;
        case Pid::SHOW_MEASURE_NUMBERS: return AutoOnOff::AUTO;
        case Pid::SHOW_IF_ENTIRE_SYSTEM_EMPTY: return false;
        case Pid::VISIBLE: return true;
        case Pid::STAFF_CUTAWAY: return false;
        case Pid::STAFF_HIDE_SYSTEM_BARLINE: return false;
        case Pid::STAFF_MERGE_MATCHING_RESTS: return int(AutoOnOff::AUTO);
        case Pid::STAFF_REFLECT_TRANSPOSITION: return true;
        default: LOGD("unhandled id <%s>", propertyName(id)); return PropertyValue();
    }
}

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void Staff::setLocalSpatium(double oldVal, double newVal, Fraction tick) {
    const int intEndTick = staffTypeList_.staffTypeRange(tick).second;
    const Fraction etick = (intEndTick == -1) ? score()->lastMeasure()->endTick() : Fraction::fromTicks(intEndTick);

    staff_idx_t staffIdx = idx();
    track_idx_t startTrack = staffIdx * VOICES;
    track_idx_t endTrack = startTrack + VOICES;
    for (Segment *s = score()->tick2rightSegment(tick); s && s->tick() < etick; s = s->next1()) {
        for (EngravingItem *e : s->annotations()) {
            if (e->track() >= startTrack && e->track() < endTrack) {
                e->localSpatiumChanged(oldVal, newVal);
            }
        }
        for (track_idx_t track = startTrack; track < endTrack; ++track) {
            if (s->element(track)) {
                s->element(track)->localSpatiumChanged(oldVal, newVal);
            }
        }
    }
    auto spanners = score()->spannerMap().findContained(tick.ticks(), etick.ticks());
    for (auto interval : spanners) {
        Spanner *spanner = interval.value;
        if (spanner->staffIdx() == staffIdx) {
            for (auto k : spanner->spannerSegments()) {
                k->localSpatiumChanged(oldVal, newVal);
            }
        }
    }
}

//---------------------------------------------------------
//   isPitchedStaff
//---------------------------------------------------------

bool Staff::isPitchedStaff(const Fraction &tick) const {
    //return staffType(tick)->group() == StaffGroup::STANDARD;
    return staffType(tick)->group() != StaffGroup::TAB && !originPart(tick)->instrument(tick)->useDrumset();
}

//---------------------------------------------------------
//   isTabStaff
//---------------------------------------------------------

bool Staff::isTabStaff(const Fraction &tick) const { return staffType(tick)->group() == StaffGroup::TAB; }

//---------------------------------------------------------
//   isDrumStaff
//---------------------------------------------------------

bool Staff::isDrumStaff(const Fraction &tick) const {
    //check for instrument instead of staffType (for pitched to unpitched instr. changes)
    return originPart(tick)->instrument(tick)->useDrumset();
}

//---------------------------------------------------------
//   lines
//---------------------------------------------------------

int Staff::lines(const Fraction &tick) const { return staffType(tick)->lines(); }

//---------------------------------------------------------
//   setLines
//---------------------------------------------------------

void Staff::setLines(const Fraction &tick, int val) { staffType(tick)->setLines(val); }

//---------------------------------------------------------
//   lineDistance
//    distance between staff lines
//---------------------------------------------------------

double Staff::lineDistance(const Fraction &tick) const { return staffType(tick)->lineDistance().val(); }

//---------------------------------------------------------
//   invisible
//---------------------------------------------------------

bool Staff::isLinesInvisible(const Fraction &tick) const { return staffType(tick)->invisible(); }

//---------------------------------------------------------
//   setInvisible
//---------------------------------------------------------

void Staff::setIsLinesInvisible(const Fraction &tick, bool val) { staffType(tick)->setInvisible(val); }
} // namespace mu::engraving
