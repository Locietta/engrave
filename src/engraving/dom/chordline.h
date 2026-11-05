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

#include "engravingitem.h"
#include "draw/types/painterpath.h"

#include "../types/types.h"

namespace mu::engraving {
class Factory;

class Chord;
class Note;

//---------------------------------------------------------
//   @@ ChordLine
///    bezier line attached to top note of a chord
///    implements fall, doit, plop, bend
//---------------------------------------------------------

class ChordLine final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, ChordLine)
    DECLARE_CLASSOF(ElementType::CHORDLINE)

public:

    ChordLine *clone() const override { return new ChordLine(*this); }

    Chord *chord() const { return (Chord *) (ownershipParent()); }

    void setChordLineType(ChordLineType);

    ChordLineType chordLineType() const { return chordLineType_; }

    bool isStraight() const { return straight_; }

    void setStraight(bool straight) { straight_ = straight; }

    bool isWavy() const { return wavy_; }

    void setWavy(bool wavy) { wavy_ = wavy; }

    void setLengthX(double length) { lengthX_ = length; }

    double lengthX() const { return lengthX_; }

    void setLengthY(double length) { lengthY_ = length; }

    double lengthY() const { return lengthY_; }

    void setModified(bool m) { modified_ = m; }

    bool modified() const { return modified_; }

    const TranslatableString &chordLineTypeName() const;

    int subtype() const override;
    TranslatableString subtypeUserName() const override;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;
    void resetProperty(Pid) override;

    bool isToTheLeft() const { return chordLineType_ == ChordLineType::PLOP || chordLineType_ == ChordLineType::SCOOP; }

    bool isBelow() const { return chordLineType_ == ChordLineType::SCOOP || chordLineType_ == ChordLineType::FALL; }

    bool playChordLine() const { return playChordLine_; }

    void setPlayChordLine(bool val) { playChordLine_ = val; }

    void setNote(Note *note);

    Note *note() const { return note_; }

    SymId waveSym() const;

    // Physical drawing-unit sources, independent of the derived layout cache.
    bool hasSourceOffsets() const { return hasSourceOffsets_; }

    bool hasDefaultPath() const { return !modified_ && (!hasSourceOffsets_ || sourceOffsets_ == std::array<PointF, 4>{}); }

    const std::array<PointF, 4> &sourceOffsets() const { return sourceOffsets_; }

    void readSourceOffsets(const std::array<PointF, 4> &xml, double unit);
    std::array<PointF, 4> xmlSourceOffsets() const;
    PainterPath sourceOffsetPath(double spatium, double mag) const;
    void spatiumChanged(double oldValue, double newValue) override;
    void localSpatiumChanged(double oldValue, double newValue) override;

    static constexpr std::array<Pid, 8> appearanceProperties{
        Pid::COLOR,           Pid::VISIBLE, Pid::Z, Pid::SIZE_SPATIUM_DEPENDENT, Pid::CHORD_LINE_TYPE, Pid::CHORD_LINE_STRAIGHT,
        Pid::CHORD_LINE_WAVY, Pid::PATH};

    struct AppearanceState {
        std::array<PropertyValue, 8> values;
        std::array<PropertyFlags, 8> flags;
        bool linked;
        bool offsets;
        bool modified;
        double lengthX;
        double lengthY;
        std::array<PointF, 4> controls;
    };

    AppearanceState appearanceState() const;
    void restoreAppearance(const AppearanceState &);

    struct LayoutData : public EngravingItem::LayoutData {
        muse::draw::PainterPath path;
    };
    DECLARE_LAYOUTDATA_METHODS(ChordLine)

private:

    friend class Factory;

    ChordLine(DummyParentOr<Chord> parent);
    ChordLine(const ChordLine &);

    std::array<PointF, 4> offsetPoints(const std::array<PointF, 4> &, double spatium, double mag) const;
    void scaleOffsets(double oldValue, double newValue);
    bool hasSourceOffsets_ = false;
    std::array<PointF, 4> sourceOffsets_{};

    bool straight_ = false;
    bool wavy_ = false;

    ChordLineType chordLineType_ = ChordLineType::NOTYPE;
    bool modified_ = false;
    double lengthX_ = 0.0;
    double lengthY_ = 0.0;
    bool playChordLine_ = true;
    Note *note_ = nullptr;
};
} // namespace mu::engraving
