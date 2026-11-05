/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
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

#include "readcontext.h"

#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/score.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::read500;

ReadContext::ReadContext(Score *score, bool pasteMode) : score_(score), _pasteMode(pasteMode) {}

ReadContext::~ReadContext() {
    IF_ASSERT_FAILED(incompleteNoteAnchoredSpanners_.empty()) {
        for (auto &[spanner, startEndEIDs] : incompleteNoteAnchoredSpanners_) {
            delete spanner;
        }

        incompleteNoteAnchoredSpanners_.clear();
    }
}

Score *ReadContext::score() const { return score_; }

void ReadContext::setScore(Score *score) { score_ = score; }

const MStyle &ReadContext::style() const { return score()->style(); }

std::shared_ptr<IEngravingFontsProvider> ReadContext::engravingFonts() const { return score()->engravingFonts(); }

String ReadContext::mscoreVersion() const { return score_->mscoreVersion(); }

int ReadContext::mscVersion() const { return score_->mscVersion(); }

int ReadContext::fileDivision() const { return score_->fileDivision(); }

int ReadContext::fileDivision(int t) const { return score_->fileDivision(t); }

double ReadContext::spatium() const { return score_->style().spatium(); }

void ReadContext::setSpatium(double v) { score_->style().set(Sid::spatium, v); }

DummyParent *ReadContext::dummy() const { return score_->dummy(); }

Staff *ReadContext::staff(staff_idx_t n) { return score_->staff(n); }

bool ReadContext::isSameScore(const EngravingObject *obj) const { return obj->score() == score_; }

Fraction ReadContext::rtick() const { return _curMeasure ? _tick - _curMeasure->tick() : _tick; }

void ReadContext::setTick(const Fraction &f) {
    _tick = f.reduced();
    _intTick = _tick.ticks();
}

void ReadContext::incTick(const Fraction &f) {
    _tick += f;
    _tick.reduce();
    _intTick += f.ticks();
}

Location ReadContext::location(bool forceAbsFrac) const {
    Location l = Location::absolute();
    fillLocation(l, forceAbsFrac);
    return l;
}

//---------------------------------------------------------
//   fillLocation
//    fills location fields which have default values with
//    values relevant for the current reader's position.
//    When in paste mode (or forceAbsFrac is true) absolute
//    fraction values are used and measure number is set to
//    zero.
//---------------------------------------------------------

void ReadContext::fillLocation(Location &l, bool forceAbsFrac) const {
    constexpr Location defaults = Location::absolute();
    const bool absFrac = (pasteMode() || forceAbsFrac);
    if (l.track() == defaults.track()) {
        l.setTrack(static_cast<int>(track()));
    }
    if (l.frac() == defaults.frac()) {
        l.setFrac(absFrac ? tick() : rtick());
    }
    if (l.measure() == defaults.measure()) {
        l.setMeasure(absFrac ? 0 : currentMeasureIndex());
    }
}

//---------------------------------------------------------
//   setLocation
//    sets a new reading location, taking into account its
//    type (absolute or relative).
//---------------------------------------------------------

void ReadContext::setLocation(const Location &l) {
    if (l.isRelative()) {
        Location newLoc = l;
        newLoc.toAbsolute(location());
        int intTicks = l.frac().ticks();
        if (_tick == Fraction::fromTicks(_intTick + intTicks)) {
            _intTick += intTicks;
            setTrack(newLoc.track() - _trackOffset);
            return;
        }
        setLocation(newLoc); // recursion
        return;
    }
    setTrack(l.track() - _trackOffset);
    setTick(l.frac() - _tickOffset);
    if (!pasteMode()) {
        assert(l.measure() == currentMeasureIndex());
        incTick(currentMeasure()->tick());
    }
}

void ReadContext::addPartAudioSettingCompat(PartAudioSettingsCompat partAudioSetting) {
    if (_settingsCompat.audioSettings.count(partAudioSetting.instrumentId.partId) == 0) {
        _settingsCompat.audioSettings.insert({partAudioSetting.instrumentId.partId, partAudioSetting});
    }
}

void ReadContext::addMMRestEndMeasureEID(Measure *mmrest, EID lastMeasureEID) {
    DO_ASSERT(lastMeasureEID.isValid());
    mmRestEndMeasures_.emplace(mmrest, lastMeasureEID);
}

void ReadContext::setMMRestEndMeasures() {
    EIDRegister *eidRegister = score()->masterScore()->eidRegister();
    for (auto &[mmrest, lastMeasureEID] : mmRestEndMeasures_) {
        EngravingObject *linkedElement = eidRegister->itemFromEID(lastMeasureEID);
        IF_ASSERT_FAILED(linkedElement && linkedElement->isMeasure()) {
            LOGE() << "No valid end measure found for MMRest at " << mmrest->tick().toString();
            continue;
        }

        Measure *lastMeasure = toMeasure(linkedElement);
        MeasureBase *nextMB = lastMeasure->next();
        mmrest->setNext(nextMB);
    }
}

void ReadContext::registerPastedEID(const EID &clipboardEid, const EID &fileEid) { pastedEIDs_.emplace(clipboardEid, fileEid); }

EID ReadContext::resolvePastedEID(const EID &clipboardEid) const {
    if (!_pasteMode) {
        return clipboardEid;
    }

    auto it = pastedEIDs_.find(clipboardEid);
    IF_ASSERT_FAILED(it != pastedEIDs_.end()) { return clipboardEid; }

    return it->second;
}

void ReadContext::addNoteAnchoredSpannerStartEl(Spanner *spanner, EID startElementEID) {
    auto it = incompleteNoteAnchoredSpanners_.find(spanner);
    if (it != incompleteNoteAnchoredSpanners_.end()) {
        it->second.first = startElementEID;
    } else {
        incompleteNoteAnchoredSpanners_.emplace(spanner, std::make_pair(startElementEID, EID::invalid()));
    }
}

void ReadContext::addNoteAnchoredSpannerEndEl(Spanner *spanner, EID endElementEID) {
    auto it = incompleteNoteAnchoredSpanners_.find(spanner);
    if (it != incompleteNoteAnchoredSpanners_.end()) {
        it->second.second = endElementEID;
    } else {
        incompleteNoteAnchoredSpanners_.emplace(spanner, std::make_pair(EID::invalid(), endElementEID));
    }
}

void ReadContext::connectNoteAnchoredSpanners() {
    EIDRegister *eidRegister = score()->masterScore()->eidRegister();
    for (auto &[spanner, startEndEIDs] : incompleteNoteAnchoredSpanners_) {
        EID startElementEID = startEndEIDs.first;
        EID endElementEID = startEndEIDs.second;
        EngravingItem *startElement = toEngravingItem(eidRegister->itemFromEID(startElementEID));
        EngravingItem *endElement = toEngravingItem(eidRegister->itemFromEID(endElementEID));
        IF_ASSERT_FAILED(startElement && startElement->isNote()) {
            LOGD() << "Could not find note element for " << spanner->typeName();
            delete spanner;
            continue;
        }
        IF_ASSERT_FAILED(endElement && endElement->isNote()) {
            LOGD() << "Could not find end note for " << spanner->typeName();
            delete spanner;
            continue;
        }

        Note *startNote = toNote(startElement);
        Note *endNote = toNote(endElement);
        spanner->setNoteSpan(startNote, endNote);

        if (spanner->isTie()) {
            Tie *tie = toTie(spanner);
            startNote->setTieFor(tie);
            endNote->setTieBack(tie);
        } else {
            score()->addElement(spanner);
        }
    }
    incompleteNoteAnchoredSpanners_.clear();
}
