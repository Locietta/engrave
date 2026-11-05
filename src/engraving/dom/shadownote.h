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

#include "engravingitem.h"

#include "accidental.h"
#include "durationtype.h"
#include "../types/types.h"

namespace mu::engraving {
//---------------------------------------------------------
//   ShadowNote
//---------------------------------------------------------

/**
 Graphic representation of a shadow note,
 which shows the note insert position in note entry mode.
*/

class ShadowNote final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, ShadowNote)
    DECLARE_CLASSOF(ElementType::SHADOW_NOTE)

public:
    ShadowNote(Score *);

    ShadowNote *clone() const override { return new ShadowNote(*this); }

    bool isValid() const;

    Fraction tick() const override { return tick_; }

    void setTick(Fraction t) { tick_ = t; }

    int lineIndex() const { return lineIndex_; }

    void setLineIndex(int n) { lineIndex_ = n; }

    bool isRest() const { return isRest_; }

    const TDuration &duration() const { return duration_; }

    void setState(SymId noteSymbol, TDuration duration, bool isRest, bool beyondScore, AccidentalType accidentalType = AccidentalType::NONE,
                  const std::set<SymId> &articulationIds = {});

    void drawArticulations(muse::draw::Painter *painter) const;
    void drawCloseArticulation(muse::draw::Painter *painter, const SymId &articulation, RectF &boundRect, bool up,
                               const SymId &prevArticulation) const;
    void drawFarArticulation(muse::draw::Painter *painter, const SymId &articulation, RectF &boundRect, bool up,
                             bool accentStaccatoKern) const;

    bool computeUp() const;

    SymId noteheadSymbol() const { return noteheadSymbol_; }

    bool hasStem() const;
    bool hasFlag() const;
    SymId flagSym() const;
    AccidentalType accidentalType() const;
    const std::set<SymId> &articulationIds() const;

    bool ledgerLinesVisible() const;

    bool isBeyondScore() const { return beyondScore_; }

private:

    Fraction tick_;
    int lineIndex_ = -1;
    SymId noteheadSymbol_ = SymId::noSym;
    TDuration duration_;
    bool isRest_ = false;
    AccidentalType accidentalType_ = AccidentalType::NONE;
    std::set<SymId> articulationIds_;
    bool beyondScore_ = false;
};
} // namespace mu::engraving
