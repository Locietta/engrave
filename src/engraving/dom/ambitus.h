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

#include "accidental.h"
#include "engravingitem.h"

#include "pitchspelling.h"

#include "../types/types.h"

namespace mu::engraving {
class Factory;

//---------------------------------------------------------
//   @@ Ambitus
//---------------------------------------------------------

class Ambitus final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Ambitus)
    DECLARE_CLASSOF(ElementType::AMBITUS)

public:
    ~Ambitus();

    static constexpr NoteHeadGroup NOTEHEADGROUP_DEFAULT = NoteHeadGroup::HEAD_NORMAL;
    static constexpr NoteHeadType NOTEHEADTYPE_DEFAULT = NoteHeadType::HEAD_AUTO;
    static constexpr DirectionH DIRECTION_DEFAULT = DirectionH::AUTO;
    static constexpr bool HASLINE_DEFAULT = true;
    static constexpr Spatium LINEWIDTH_DEFAULT = 0.12_sp;
    static constexpr Spatium LINEOFFSET_DEFAULT = 0.8_sp; // the distance between notehead and line

    Ambitus *clone() const override { return new Ambitus(*this); }

    double mag() const override;

    void initFrom(Ambitus *a);

    // getters and setters
    NoteHeadGroup noteHeadGroup() const { return noteHeadGroup_; }

    NoteHeadType noteHeadType() const { return noteHeadType_; }

    DirectionH direction() const { return direction_; }

    bool hasLine() const { return hasLine_; }

    Spatium lineWidth() const { return lineWidth_; }

    int topOctave() const { return (topPitch_ / PITCH_DELTA_OCTAVE) - 1; }

    int bottomOctave() const { return (bottomPitch_ / PITCH_DELTA_OCTAVE) - 1; }

    int topPitch() const { return topPitch_; }

    int bottomPitch() const { return bottomPitch_; }

    int topTpc() const { return topTpc_; }

    int bottomTpc() const { return bottomTpc_; }

    Accidental *topAccidental() const { return topAccidental_; }

    Accidental *bottomAccidental() const { return bottomAccidental_; }

    void setNoteHeadGroup(NoteHeadGroup val) { noteHeadGroup_ = val; }

    void setNoteHeadType(NoteHeadType val) { noteHeadType_ = val; }

    void setDirection(DirectionH val) { direction_ = val; }

    void setHasLine(bool val) { hasLine_ = val; }

    void setLineWidth(Spatium val) { lineWidth_ = val; }

    void setTopPitch(int val, bool applyLogic = true);
    void setBottomPitch(int val, bool applyLogic = true);
    void setTopTpc(int val, bool applyLogic = true);
    void setBottomTpc(int val, bool applyLogic = true);

    // some utility functions
    Segment *segment() const { return (Segment *) ownershipParent(); }

    SymId noteHead() const;
    double headWidth() const;

    // re-implemented virtual functions
    void scanElements(std::function<void(EngravingItem *)> func) override;
    void setTrack(track_idx_t val) override;

    void remove(EngravingItem *) override;

    // properties
    PropertyValue getProperty(Pid) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    static AccidentalType accidentalType(int tpc, Key key);
    static int staffLine(int tpc, int pitch, ClefType clf);

    struct LayoutData : public EngravingItem::LayoutData {
        PointF topPos;    // position of top note symbol
        PointF bottomPos; // position of bottom note symbol
        LineF line;       // the drawn line
    };
    DECLARE_LAYOUTDATA_METHODS(Ambitus)

private:

    friend class Factory;
    Ambitus(DummyParentOr<Segment> parent);
    Ambitus(const Ambitus &a);

    void normalize();

    struct Ranges {
        int topTpc = Tpc::TPC_INVALID;
        int bottomTpc = Tpc::TPC_INVALID;
        int topPitch = INVALID_PITCH;
        int bottomPitch = MAX_PITCH + 1;
    };

    Ranges estimateRanges() const; // scan staff up to next section break and update range pitches

    NoteHeadGroup noteHeadGroup_ = NOTEHEADGROUP_DEFAULT;
    NoteHeadType noteHeadType_ = NOTEHEADTYPE_DEFAULT;
    DirectionH direction_ = DIRECTION_DEFAULT;
    bool hasLine_ = HASLINE_DEFAULT;
    Spatium lineWidth_;
    Accidental *topAccidental_ = nullptr;
    Accidental *bottomAccidental_ = nullptr;
    int topPitch_ = INVALID_PITCH, bottomPitch_ = INVALID_PITCH;
    int topTpc_ = Tpc::TPC_INVALID, bottomTpc_ = Tpc::TPC_INVALID;
};
} // namespace mu::engraving
