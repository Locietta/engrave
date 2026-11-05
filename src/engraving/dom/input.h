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

#ifndef MU_ENGRAVING_INPUT_H
#define MU_ENGRAVING_INPUT_H

#include <set>

#include "accidental.h"
#include "chord.h"
#include "durationtype.h"
#include "mscore.h"
#include "noteval.h"

#include "../types/types.h"

namespace mu::engraving {
class ChordRest;
class Drumset;
class EngravingItem;
class Note;
class Score;
class Segment;
class Selection;
class Slur;
class Staff;

// no ordinal for the visual repres. of string
// (topmost in TAB varies according to visual order and presence of bass strings)
static constexpr int VISUAL_INVALID_STRING_INDEX = -100;

//---------------------------------------------------------
//   NoteEntryMethod
//---------------------------------------------------------

enum class NoteEntryMethod : char { UNKNOWN, BY_NOTE_NAME, BY_DURATION, REPITCH, RHYTHM, REALTIME_AUTO, REALTIME_MANUAL, TIMEWISE };

//---------------------------------------------------------
//   InputState
//---------------------------------------------------------

class InputState {
public:
    bool isValid() const;

    ChordRest *cr() const;

    Fraction tick() const;

    void setDuration(const TDuration &d) { duration_ = d; }

    TDuration duration() const { return duration_; }

    void setDots(int n);

    Fraction ticks() const { return duration_.ticks(); }

    Segment *segment() const { return segment_; }

    void setSegment(Segment *s);

    Segment *lastSegment() const { return lastSegment_; }

    void setLastSegment(Segment *s) { lastSegment_ = s; }

    Staff *staff() const;
    staff_idx_t staffIdx() const;

    Drumset *drumset() const;

    int drumNote() const { return drumNote_; }

    void setDrumNote(int v) { drumNote_ = v; }

    // Used in the input-by-duration mode
    const NoteValList &notes() const { return notes_; }

    void setNotes(const NoteValList &v) { notes_ = v; }

    voice_idx_t voice() const { return track_ == muse::nidx ? 0 : (track_ % VOICES); }

    void setVoice(voice_idx_t v);

    track_idx_t track() const { return track_; }

    void setTrack(track_idx_t v) {
        prevTrack_ = track_;
        track_ = v;
    }

    track_idx_t prevTrack() const { return prevTrack_; }

    int string() const { return string_; }

    void setString(int val) { string_ = val; }

    StaffGroup staffGroup() const;

    bool rest() const { return rest_; }

    void setRest(bool v) { rest_ = v; }

    NoteType noteType() const { return noteType_; }

    void setNoteType(NoteType t) { noteType_ = t; }

    BeamMode beamMode() const { return beamMode_; }

    void setBeamMode(BeamMode m) { beamMode_ = m; }

    bool noteEntryMode() const { return noteEntryMode_; }

    void setNoteEntryMode(bool v) { noteEntryMode_ = v; }

    NoteEntryMethod noteEntryMethod() const { return noteEntryMethod_; }

    void setNoteEntryMethod(NoteEntryMethod m) { noteEntryMethod_ = m; }

    bool usingNoteEntryMethod(NoteEntryMethod m) const { return m == noteEntryMethod(); }

    AccidentalType accidentalType() const { return accidentalType_; }

    void setAccidentalType(AccidentalType val) { accidentalType_ = val; }

    const std::set<SymId> &articulationIds() const { return articulationIds_; }

    void setArticulationIds(const std::set<SymId> &ids) { articulationIds_ = ids; }

    Slur *slur() const { return slur_; }

    void setSlur(Slur *s) { slur_ = s; }

    void update(Selection &selection);
    void moveInputPos(EngravingItem *e);
    void moveToNextInputPos();

    bool beyondScore() const { return beyondScore_; }

    void setBeyondScore(bool val) { beyondScore_ = val; }

    // TODO: unify with Selection::cr()?
    static Note *note(EngravingItem *);
    static ChordRest *chordRest(EngravingItem *);

private:

    Segment *nextInputPos() const;

    track_idx_t track_ = 0;
    track_idx_t prevTrack_ = 0; // used for navigation

    int drumNote_ = -1;
    int string_ = VISUAL_INVALID_STRING_INDEX; // visual string selected for input (TAB staves only)

    NoteValList notes_;

    Segment *lastSegment_ = nullptr;
    Segment *segment_ = nullptr; // current segment

    bool noteEntryMode_ = false;
    NoteEntryMethod noteEntryMethod_ = NoteEntryMethod::BY_NOTE_NAME;

    TDuration duration_ = DurationType::V_INVALID; // currently duration
    bool rest_ = false;                            // rest mode

    NoteType noteType_ = NoteType::NORMAL;
    BeamMode beamMode_ = BeamMode::AUTO;

    AccidentalType accidentalType_ = AccidentalType::NONE;
    Slur *slur_ = nullptr;

    std::set<SymId> articulationIds_;

    bool beyondScore_ = false;
};
} // namespace mu::engraving
#endif
