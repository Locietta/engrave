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

#include "ambitus.h"

#include "translation.h"

#include "accidental.h"
#include "chord.h"
#include "factory.h"
#include "measure.h"
#include "note.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "utils.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   Ambitus
//---------------------------------------------------------

Ambitus::Ambitus(DummyParentOr<Segment> parent) : EngravingItem(ElementType::AMBITUS, parent, ElementFlag::ON_STAFF) {
    noteHeadGroup_ = NOTEHEADGROUP_DEFAULT;
    noteHeadType_ = NOTEHEADTYPE_DEFAULT;
    direction_ = DIRECTION_DEFAULT;
    hasLine_ = HASLINE_DEFAULT;
    lineWidth_ = LINEWIDTH_DEFAULT;
    topPitch_ = INVALID_PITCH;
    bottomPitch_ = INVALID_PITCH;
    topTpc_ = Tpc::TPC_INVALID;
    bottomTpc_ = Tpc::TPC_INVALID;

    topAccidental_ = Factory::createAccidental(this, false);
    bottomAccidental_ = Factory::createAccidental(this, false);
}

Ambitus::Ambitus(const Ambitus &a) : EngravingItem(a) {
    noteHeadGroup_ = a.noteHeadGroup_;
    noteHeadType_ = a.noteHeadType_;
    direction_ = a.direction_;
    hasLine_ = a.hasLine_;
    lineWidth_ = a.lineWidth_;

    topAccidental_ = a.topAccidental_->clone();
    topAccidental_->setOwnershipParent(this);

    bottomAccidental_ = a.bottomAccidental_->clone();
    bottomAccidental_->setOwnershipParent(this);

    topPitch_ = a.topPitch_;
    topTpc_ = a.topTpc_;
    bottomPitch_ = a.bottomPitch_;
    bottomTpc_ = a.bottomTpc_;
}

Ambitus::~Ambitus() {
    delete topAccidental_;
    delete bottomAccidental_;
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Ambitus::mag() const { return staff() ? staff()->staffMag(tick()) : 1.0; }

//---------------------------------------------------------
//   initFrom
//---------------------------------------------------------

void Ambitus::initFrom(Ambitus *a) {
    noteHeadGroup_ = a->noteHeadGroup_;
    noteHeadType_ = a->noteHeadType_;
    direction_ = a->direction_;
    hasLine_ = a->hasLine_;
    lineWidth_ = a->lineWidth_;
    topPitch_ = a->topPitch_;
    bottomPitch_ = a->bottomPitch_;
    topTpc_ = a->topTpc_;
    bottomTpc_ = a->bottomTpc_;
}

//---------------------------------------------------------
//   setTrack
//
//    when the Ambitus element is assigned a track,
//    initialize top and bottom 'notes' to top and bottom staff lines
//---------------------------------------------------------

void Ambitus::setTrack(track_idx_t t) {
    EngravingItem::setTrack(t);

    // if not initialized and there is a segment and a staff,
    // initialize pitches and tpc's to first and last staff line
    // (for use in palettes)
    if (!pitchIsValid(topPitch_) || !tpcIsValid(topTpc_) || !pitchIsValid(bottomPitch_) || !tpcIsValid(bottomTpc_)) {
        if (segment() && staff()) {
            Ambitus::Ranges ranges = estimateRanges();
            topTpc_ = ranges.topTpc;
            bottomTpc_ = ranges.bottomTpc;
            topPitch_ = ranges.topPitch;
            bottomPitch_ = ranges.bottomPitch;

            topAccidental_->setTrack(t);
            bottomAccidental_->setTrack(t);
        }
    }
}

//---------------------------------------------------------
//   setTop/BottomPitch
//
//    setting either pitch requires to adjust the corresponding tpc
//---------------------------------------------------------

void Ambitus::setTopPitch(int val, bool applyLogic) {
    if (!applyLogic) {
        topPitch_ = val;
        return;
    }

    // if pitch difference is not an integer number of octaves, adjust tpc
    // (to avoid 'wild' tpc changes with octave changes)
    if ((val - topPitch()) % PITCH_DELTA_OCTAVE != 0) {
        Key key = (staff() && segment()) ? staff()->key(segment()->tick()) : Key::C;
        topTpc_ = pitch2tpc(val, key, Prefer::NEAREST);
    }
    topPitch_ = val;
    normalize();
}

void Ambitus::setBottomPitch(int val, bool applyLogic) {
    if (!applyLogic) {
        bottomPitch_ = val;
        return;
    }

    // if pitch difference is not an integer number of octaves, adjust tpc
    // (to avoid 'wild' tpc changes with octave changes)
    if ((val - bottomPitch()) % PITCH_DELTA_OCTAVE != 0) {
        Key key = (staff() && segment()) ? staff()->key(segment()->tick()) : Key::C;
        bottomTpc_ = pitch2tpc(val, key, Prefer::NEAREST);
    }
    bottomPitch_ = val;
    normalize();
}

//---------------------------------------------------------
//   setTop/BottomTpc
//
//    setting either tpc requires to adjust the corresponding pitch
//    (but remaining in the same octave)
//---------------------------------------------------------

void Ambitus::setTopTpc(int val, bool applyLogic) {
    topTpc_ = val;

    if (!applyLogic) {
        return;
    }

    int octave = topPitch() / PITCH_DELTA_OCTAVE;
    int newOctavedPitch = (tpc2pitch(val) + PITCH_DELTA_OCTAVE) % PITCH_DELTA_OCTAVE;
    topPitch_ = (octave * PITCH_DELTA_OCTAVE) + newOctavedPitch;
    normalize();
}

void Ambitus::setBottomTpc(int val, bool applyLogic) {
    bottomTpc_ = val;

    if (!applyLogic) {
        return;
    }

    int octave = bottomPitch() / PITCH_DELTA_OCTAVE;
    int newOctavedPitch = (tpc2pitch(val) + PITCH_DELTA_OCTAVE) % PITCH_DELTA_OCTAVE;
    bottomPitch_ = (octave * PITCH_DELTA_OCTAVE) + newOctavedPitch;
    normalize();
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Ambitus::scanElements(std::function<void(EngravingItem *)> func) {
    func(this);
    if (topAccidental_->accidentalType() != AccidentalType::NONE) {
        func(topAccidental_);
    }

    if (bottomAccidental_->accidentalType() != AccidentalType::NONE) {
        func(bottomAccidental_);
    }
}

//---------------------------------------------------------
//   noteHead
//---------------------------------------------------------

SymId Ambitus::noteHead() const {
    int hg = 1;
    NoteHeadType ht = NoteHeadType::HEAD_QUARTER;

    if (noteHeadType_ != NoteHeadType::HEAD_AUTO) {
        ht = noteHeadType_;
    }

    SymId t = Note::noteHead(hg, noteHeadGroup_, ht);
    if (t == SymId::noSym) {
        LOGD("invalid notehead %d/%d", int(noteHeadGroup_), int(noteHeadType_));
        t = Note::noteHead(0, NoteHeadGroup::HEAD_NORMAL, ht);
    }
    return t;
}

//---------------------------------------------------------
//   headWidth
//
//    returns the width of the notehead symbol
//---------------------------------------------------------

double Ambitus::headWidth() const { return symWidth(noteHead()); }

//---------------------------------------------------------
//   normalize
//
//    makes sure topPitch is not < bottomPitch
//---------------------------------------------------------

void Ambitus::normalize() {
    if (topPitch_ < bottomPitch_) {
        std::swap(topPitch_, bottomPitch_);
        std::swap(topTpc_, bottomTpc_);
    }
}

//---------------------------------------------------------
//   updateRange
//
//    scans the staff contents up to next section break to update the range pitches/tpc's
//---------------------------------------------------------

Ambitus::Ranges Ambitus::estimateRanges() const {
    Ambitus::Ranges result;

    Segment *s = segment() ? segment()->measure()->findSegment(SegmentType::ChordRest, segment()->tick()) : nullptr;
    for (; s && !s->measure()->sectionBreak(); s = s->nextCR()) {
        for (track_idx_t t = track(); t < track() + VOICES; t++) {
            EngravingItem *e = s->element(t);
            if (!e || !e->isChord()) {
                continue;
            }
            Chord *chord = toChord(e);
            for (Note *n : chord->notes()) {
                if (!n->play()) {
                    continue;
                }
                int pitch = n->epitch() + n->ottaveCapoFret(); // written pitch, accounting for octave offset
                if (pitch > result.topPitch) {
                    result.topPitch = pitch;
                    result.topTpc = n->tpc();
                }
                if (pitch < result.bottomPitch) {
                    result.bottomPitch = pitch;
                    result.bottomTpc = n->tpc();
                }
            }
        }
    }

    return result;
}

void Ambitus::remove(EngravingItem *e) {
    if (e->isAccidental()) {
        //! NOTE Do nothing (removing _topAccid or _bottomAccid)
        return;
    }

    EngravingItem::remove(e);
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Ambitus::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::HEAD_GROUP: return noteHeadGroup();
        case Pid::HEAD_TYPE: return noteHeadType();
        case Pid::MIRROR_HEAD: return direction();
        case Pid::GHOST: // recycled property = _hasLine
            return hasLine();
        case Pid::LINE_WIDTH: return lineWidth();
        case Pid::TPC1: return topTpc();
        case Pid::FBPARENTHESIS1: // recycled property = _bottomTpc
            return bottomTpc();
        case Pid::PITCH: return topPitch();
        case Pid::FBPARENTHESIS2: // recycled property = _bottomPitch
            return bottomPitch();
        case Pid::FBPARENTHESIS3: // recycled property = octave of _topPitch
            return topOctave();
        case Pid::FBPARENTHESIS4: // recycled property = octave of _bottomPitch
            return bottomOctave();
        default: return EngravingItem::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Ambitus::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::HEAD_GROUP: setNoteHeadGroup(v.value<NoteHeadGroup>()); break;
        case Pid::HEAD_TYPE: setNoteHeadType(v.value<NoteHeadType>()); break;
        case Pid::MIRROR_HEAD: setDirection(v.value<DirectionH>()); break;
        case Pid::GHOST: // recycled property = _hasLine
            setHasLine(v.toBool());
            break;
        case Pid::LINE_WIDTH: setLineWidth(v.value<Spatium>()); break;
        case Pid::TPC1: setTopTpc(v.toInt()); break;
        case Pid::FBPARENTHESIS1: // recycled property = _bottomTpc
            setBottomTpc(v.toInt());
            break;
        case Pid::PITCH: setTopPitch(v.toInt()); break;
        case Pid::FBPARENTHESIS2: // recycled property = _bottomPitch
            setBottomPitch(v.toInt());
            break;
        case Pid::FBPARENTHESIS3: // recycled property = octave of _topPitch
            setTopPitch(topPitch() % PITCH_DELTA_OCTAVE + (v.toInt() + 1) * PITCH_DELTA_OCTAVE);
            break;
        case Pid::FBPARENTHESIS4: // recycled property = octave of _bottomPitch
            setBottomPitch(bottomPitch() % PITCH_DELTA_OCTAVE + (v.toInt() + 1) * PITCH_DELTA_OCTAVE);
            break;
        case Pid::COLOR:
        case Pid::VISIBLE:
            topAccidental_->setProperty(propertyId, v);
            bottomAccidental_->setProperty(propertyId, v);
            EngravingItem::setProperty(propertyId, v);
            break;
        default: return EngravingItem::setProperty(propertyId, v);
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Ambitus::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::HEAD_GROUP: return NOTEHEADGROUP_DEFAULT;
        case Pid::HEAD_TYPE: return NOTEHEADTYPE_DEFAULT;
        case Pid::MIRROR_HEAD: return DIRECTION_DEFAULT;
        case Pid::GHOST: return HASLINE_DEFAULT;
        case Pid::LINE_WIDTH: return LINEWIDTH_DEFAULT;
        case Pid::TPC1: return estimateRanges().topTpc;
        case Pid::FBPARENTHESIS1: return estimateRanges().bottomTpc;
        case Pid::PITCH: return estimateRanges().topPitch;
        case Pid::FBPARENTHESIS2: return estimateRanges().bottomPitch;
        case Pid::FBPARENTHESIS3: return int(estimateRanges().topPitch / PITCH_DELTA_OCTAVE) - 1;
        case Pid::FBPARENTHESIS4: return int(estimateRanges().bottomPitch / PITCH_DELTA_OCTAVE) - 1;
        default: return EngravingItem::propertyDefault(id);
    }
    //return QVariant();
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Ambitus::nextSegmentElement() { return segment()->firstInNextSegments(staffIdx()); }

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Ambitus::prevSegmentElement() { return segment()->lastInPrevSegments(staffIdx()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

AccidentalType Ambitus::accidentalType(int tpc, Key key) {
    AccidentalType accidType = AccidentalType::NONE;
    // if (13 <= (tpc - key) <= 19) there is no accidental)
    if (tpc - int(key) >= 13 && tpc - int(key) <= 19) {
        accidType = AccidentalType::NONE;
    } else {
        AccidentalVal accidVal = tpc2alter(tpc);
        accidType = Accidental::value2subtype(accidVal);
        if (accidType == AccidentalType::NONE) {
            accidType = AccidentalType::NATURAL;
        }
    }

    return accidType;
}

//! TODO Seems to need to be moved to utilities
int Ambitus::staffLine(int tpc, int pitch, ClefType clf) {
    int line = absStep(tpc, pitch);
    line = relStep(line, clf);
    return line;
}
