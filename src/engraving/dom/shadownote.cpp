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

#include "shadownote.h"

#include "draw/types/pen.h"

#include "accidental.h"
#include "articulation.h"
#include "hook.h"
#include "measure.h"
#include "rest.h"
#include "score.h"
#include "stafftype.h"

using namespace mu;
using namespace muse::draw;

namespace mu::engraving {
//---------------------------------------------------------
//   ShadowNote
//---------------------------------------------------------

ShadowNote::ShadowNote(Score *s) : EngravingItem(ElementType::SHADOW_NOTE, s), noteheadSymbol_(SymId::noSym) {
    lineIndex_ = 1000;
    duration_ = TDuration(DurationType::V_INVALID);
    isRest_ = false;
}

bool ShadowNote::isValid() const { return noteheadSymbol_ != SymId::noSym; }

void ShadowNote::setState(SymId noteSymbol, TDuration duration, bool rest, bool beyondScore, AccidentalType accidentalType,
                          const std::set<SymId> &articulationIds) {
    noteheadSymbol_ = noteSymbol;
    duration_ = duration;
    isRest_ = rest;
    beyondScore_ = beyondScore;
    accidentalType_ = accidentalType;
    articulationIds_ = articulationIds;
}

bool ShadowNote::hasStem() const { return !isRest_ && duration_.hasStem(); }

bool ShadowNote::hasFlag() const { return hasStem() && duration_.hooks() > 0; }

SymId ShadowNote::flagSym() const {
    if (!hasStem()) {
        return SymId::noSym;
    }

    bool straight = style().styleB(Sid::useStraightNoteFlags);
    int hooks = computeUp() ? duration_.hooks() : -duration_.hooks();
    return Hook::symIdForHookIndex(hooks, straight);
}

AccidentalType ShadowNote::accidentalType() const { return accidentalType_; }

const std::set<SymId> &ShadowNote::articulationIds() const { return articulationIds_; }

bool ShadowNote::ledgerLinesVisible() const { return !isRest_ && lineIndex_ < 100 && lineIndex_ > -100; }

//---------------------------------------------------------
//   computeUp
//---------------------------------------------------------

bool ShadowNote::computeUp() const {
    const StaffType *staffType = this->staffType();
    bool tabStaff = staffType && staffType->isTabStaff();

    if (tabStaff && (staffType->stemless() || !staffType->stemThrough())) {
        // if TAB staff with no stems or stem beside staff
        return true;
    }

    if (score()->tick2measure(tick_)->hasVoices(staffIdx()) || voice() != 0) {
        return !(voice() & 1);
    } else {
        return lineIndex_ > (staffType ? staffType->middleLine() : 4);
    }
}

// draw articulations, laying them out in a fashion similar to
// ChordLayout::layoutArticulations
void ShadowNote::drawArticulations(Painter *painter) const {
    // Approximate bounding box of note, including stem, relative to notehead
    double noteheadWidth = symWidth(noteheadSymbol_);
    double noteheadHeight = symHeight(noteheadSymbol_);
    double ms = spatium();
    double x1 = noteheadWidth * .5 - (ms * mag());
    double x2 = x1 + 2 * ms * mag();
    bool up = !computeUp();
    double y01 = -noteheadHeight * .5;
    double y02 = -y01;
    if (hasStem()) {
        // Stem set at 3.5 spatium in SingleDraw::draw(const ShadowNote* item ...
        if (up) {
            y02 = ms * 3.5;
        } else {
            y01 = -ms * 3.5;
        }
    }
    RectF boundRect = RectF(PointF(x1, y01), PointF(x2, y02));

    // Place articulations kept close to the note first, near the notehead
    // Articulations close to the note are defined in Articulation::layoutCloseToNote
    bool wasStaccato = false; // keep track for special accent-staccato kerning
    SymId prevArticulation = mu::engraving::SymId::noSym;
    for (const SymId &artic : articulationIds_) {
        String articName = Articulation::symId2ArticulationName(artic);
        if (articName == u"staccato") {
            wasStaccato = true;
            drawCloseArticulation(painter, artic, boundRect, up, prevArticulation);
            prevArticulation = artic;
        } else {
            if (articName == u"tenuto") {
                drawCloseArticulation(painter, artic, boundRect, up, prevArticulation);
                prevArticulation = artic;
            }
            wasStaccato = false;
        }
    }

    // Adjust bounding box to include the whole of the staff segment,
    // then place articulations not closeToNote
    ms *= .5;
    double y1 = -lineIndex_ * ms;
    double y2 = (this->staffType()->bottomLine() - lineIndex_) * ms;
    if (boundRect.y() > y1) {
        boundRect.setTop(y1);
        if (up) {
            wasStaccato = false;
        }
    }
    if (boundRect.bottom() < y2) {
        boundRect.setBottom(y2);
        if (!up) {
            wasStaccato = false;
        }
    }

    for (const SymId &artic : articulationIds_) {
        String articName = Articulation::symId2ArticulationName(artic);
        if (articName == u"staccato" || articName == u"tenuto") {
            continue;
        } else {
            if (articName.contains(u"marcato")) {
                // Marcato always above staff
                drawFarArticulation(painter, artic, boundRect, true, false);
            } else if (articName == u"accent") {
                // Accents get special kerning if adjacent to staccato
                drawFarArticulation(painter, artic, boundRect, up, wasStaccato);
            } else {
                // Other articulations (e.g., laissez-vibrer)
                drawFarArticulation(painter, artic, boundRect, up, false);
            }
            wasStaccato = false;
        }
    }
}

void ShadowNote::drawCloseArticulation(muse::draw::Painter *painter, const SymId &artic, RectF &boundRect, bool up,
                                       const SymId &prevArticulation) const {
    PointF coord;
    double spacing = spatium();
    SymId articSym = artic;
    double articHeight = symHeight(articSym);

    // Center the articulation on the note-head
    coord.rx() = boundRect.x() + (boundRect.width() - symWidth(artic)) / 2;

    // Place articulation above or below boundRect and adjust boundRect accordingly
    double minDist = 0.4 * spacing;
    if (up) {
        // If operating inside the staff, center articulation between lines
        if (lineIndex_ > 1) {
            coord.ry() = (articHeight - spacing * (3 - (lineIndex_ & 1))) / 2;
            if (prevArticulation != mu::engraving::SymId::noSym) {
                if (lineIndex_ > 2) {
                    coord.ry() -= spacing;
                } else {
                    coord.ry() -= minDist + symHeight(prevArticulation);
                }
            }
        } else {
            double topY = boundRect.y();
            coord.ry() = topY - minDist;
        }
        boundRect.setTop(coord.ry() - articHeight);
    } else {
        if (lineIndex_ < this->staffType()->bottomLine() - 1) {
            coord.ry() = (articHeight + spacing * (3 - (lineIndex_ & 1))) / 2;
            if (prevArticulation != mu::engraving::SymId::noSym) {
                if (lineIndex_ < this->staffType()->bottomLine() - 2) {
                    coord.ry() += spacing;
                } else {
                    coord.ry() += minDist + symHeight(prevArticulation);
                }
            }
        } else {
            double bottomY = boundRect.bottom();
            coord.ry() = bottomY + minDist + articHeight;
        }
        boundRect.setBottom(coord.ry());
    }

    drawSymbol(articSym, painter, coord);
}

void ShadowNote::drawFarArticulation(muse::draw::Painter *painter, const SymId &artic, RectF &boundRect, bool up,
                                     bool accentStaccatoKern) const {
    PointF coord;
    double spacing = spatium();
    SymId articSym = artic;
    double articHeight = symHeight(articSym);

    // If a LaissezVibrer, place beside the note and widen the bounding box
    if (articSym == SymId::articLaissezVibrerAbove || articSym == SymId::articLaissezVibrerBelow) {
        articSym = up ? SymId::articLaissezVibrerAbove : SymId::articLaissezVibrerBelow;
        const int upDir = up ? -1 : 1;
        const double noteHeight = symHeight(noteheadSymbol_);

        double center = 0.5 * symWidth(noteheadSymbol_);
        double visualInset = 0.1 * spacing;

        coord.rx() = center + visualInset - symBbox(articSym).left();
        coord.ry() = (noteHeight / 2 + 0.2 * spacing) * upDir;
        boundRect.setWidth(boundRect.width() + symWidth(articSym));
    } else {
        // If on the stem side of a note (e.g., marcato on a low note), move artic over for stem,
        // else, center it on the notehead;
        coord.rx() = boundRect.x() + (boundRect.width() - symWidth(artic)) / 2;
        if (up == computeUp() && hasStem()) {
            coord.rx() += 0.25 * symWidth(noteheadSymbol_);
        }

        // Place articulation above or below boundRect and adjust boundRect accordingly
        if (up) {
            double topY = boundRect.y();
            coord.ry() = topY - (accentStaccatoKern ? 0.2 : 0.4) * spacing;
            boundRect.setTop(coord.ry() - articHeight);
        } else {
            double bottomY = boundRect.bottom();
            coord.ry() = bottomY + (accentStaccatoKern ? 0.2 : 0.4) * spacing + articHeight;
            boundRect.setBottom(coord.ry());
        }
    }

    drawSymbol(articSym, painter, coord);
}
} // namespace mu::engraving
