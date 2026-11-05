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

/**
 \file
 Implementation of class Selection plus other selection related functions.
*/

#include "global/containers.h"

#include "../types/types.h"

#include "rw/rwregister.h"

#include "accidental.h"
#include "arpeggio.h"
#include "articulation.h"
#include "beam.h"
#include "box.h"
#include "breath.h"
#include "chord.h"
#include "dynamic.h"
#include "engravingitem.h"
#include "expression.h"
#include "figuredbass.h"
#include "fingering.h"
#include "fret.h"
#include "guitarbend.h"
#include "harmony.h"
#include "harppedaldiagram.h"
#include "hook.h"
#include "laissezvib.h"
#include "linkedobjects.h"
#include "lyrics.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "notedot.h"
#include "pagelockindicator.h"
#include "part.h"
#include "partialtie.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "select.h"
#include "staff.h"
#include "stafftextbase.h"
#include "stem.h"
#include "stemslash.h"
#include "sticking.h"
#include "system.h"
#include "tie.h"
#include "tremolosinglechord.h"
#include "tremolotwochord.h"
#include "tuplet.h"

#include "utils.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

// ====================================================
// Selection
// ====================================================

Selection::Selection(Score *s) {
    score_ = s;
    state_ = SelState::NONE;
    startSegment_ = 0;
    endSegment_ = 0;
    startBox_ = 0;
    endBox_ = 0;
    activeSegment_ = 0;
    staffStart_ = 0;
    staffEnd_ = 0;
    activeTrack_ = 0;
    currentTick_ = Fraction(-1, 1);
    currentTrack_ = 0;
}

Fraction Selection::tickStart() const {
    switch (state_) {
        case SelState::RANGE: return startSegment_ ? startSegment_->tick() : Fraction(-1, 1);
        case SelState::LIST: {
            ChordRest *cr = firstChordRest();
            return (cr) ? cr->tick() : Fraction(-1, 1);
        }
        default: return Fraction(-1, 1);
    }
}

Fraction Selection::tickEnd() const {
    switch (state_) {
        case SelState::RANGE: {
            if (endSegment_) {
                return endSegment_->tick();
            } else { // endsegment == 0 if end of score
                Measure *m = score_->lastMeasure();
                return m->endTick();
            }
            break;
        }
        case SelState::LIST: {
            ChordRest *cr = lastChordRest();
            return (cr) ? cr->segment()->tick() : Fraction(-1, 1);
            break;
        }
        default: return Fraction(-1, 1);
    }
}

bool Selection::isStartActive() const { return activeSegment() && activeSegment()->tick() == tickStart(); }

bool Selection::isEndActive() const { return activeSegment() && activeSegment()->tick() == tickEnd(); }

EngravingItem *Selection::element() const { return ((state() != SelState::RANGE) && (el_.size() == 1)) ? el_[0] : 0; }

ChordRest *Selection::cr() const {
    EngravingItem *e = element();
    if (!e) {
        return 0;
    }
    if (e->isNote()) {
        e = toNote(e)->chord();
    }
    if (e->isChordRest()) {
        return toChordRest(e);
    }
    return 0;
}

ChordRest *Selection::currentCR() const {
    // no selection yet - start at very beginning, not first cr
    if (currentTick_ == Fraction(-1, 1)) {
        return nullptr;
    }
    Segment *s = score()->tick2rightSegment(currentTick_, true);
    if (!s) {
        return nullptr;
    }
    track_idx_t track = currentTrack_;
    // staff may have been removed - start at top
    if (track >= score()->ntracks()) {
        track = 0;
    }
    EngravingItem *e = s->element(track);
    if (e && e->isChordRest()) {
        return toChordRest(e);
    } else {
        return nullptr;
    }
}

ChordRest *Selection::activeCR() const {
    if (state_ != SelState::RANGE || !activeSegment_) {
        return nullptr;
    }
    if (activeSegment_ == startSegment_) {
        return firstChordRest(activeTrack_);
    } else {
        return lastChordRest(activeTrack_);
    }
}

ChordRest *Selection::firstChordRestInRange(track_idx_t preferredTrack) const {
    IF_ASSERT_FAILED(isRange()) { return nullptr; }

    Segment *firstCRSegment = nullptr;

    for (Segment *currSeg = startSegment_; currSeg && (currSeg != endSegment_); currSeg = currSeg->next1MM()) {
        if (!currSeg->enabled() || !currSeg->isChordRestType()) {
            continue;
        }
        if (preferredTrack == muse::nidx) {
            // no track specified, use the first CR segment we find...
            firstCRSegment = currSeg;
            break;
        }
        EngravingItem *e = currSeg->element(preferredTrack);
        if (e && e->isChordRest()) {
            return toChordRest(e);
        }
    }

    if (!firstCRSegment) {
        return nullptr;
    }

    for (track_idx_t track = staff2track(staffStart()); track < staff2track(staffEnd()); ++track) {
        EngravingItem *e = firstCRSegment->element(track);
        if (e && e->isChordRest()) {
            return toChordRest(e);
        }
    }
    return nullptr;
}

ChordRest *Selection::lastChordRestInRange(track_idx_t preferredTrack) const {
    IF_ASSERT_FAILED(isRange()) { return nullptr; }

    Segment *lastCRSegment = nullptr;

    Segment *lastSegInScore = score_->lastMeasureMM() ? score_->lastMeasureMM()->last() : nullptr;
    Segment *currSeg = endSegment_ ? endSegment_->prev1MM() : lastSegInScore;
    while (currSeg) {
        if (!currSeg->enabled() || !currSeg->isChordRestType()) {
            currSeg = currSeg->prev1MM();
            continue;
        }
        if (preferredTrack == muse::nidx) {
            // no track specified, use the first CR segment we find...
            lastCRSegment = currSeg;
            break;
        }
        EngravingItem *e = currSeg->element(preferredTrack);
        if (e && e->isChordRest()) {
            return toChordRest(e);
        }
        currSeg = currSeg->prev1MM();
        if (currSeg == startSegment_->prev1MM()) {
            break;
        }
    }

    if (!lastCRSegment) {
        return nullptr;
    }

    for (track_idx_t track = staff2track(staffStart()); track < staff2track(staffEnd()); ++track) {
        EngravingItem *e = lastCRSegment->element(track);
        if (e && e->isChordRest()) {
            return toChordRest(e);
        }
    }

    return nullptr;
}

ChordRest *Selection::firstChordRest(track_idx_t track) const {
    if (isRange()) {
        return firstChordRestInRange(track);
    }
    if (el_.size() == 1) {
        EngravingItem *el = el_[0];
        if (el->isNote()) {
            return toChordRest(el->ownershipParent());
        } else if (el->isChordRest()) {
            return toChordRest(el);
        }
        return nullptr;
    }
    ChordRest *cr = nullptr;
    for (EngravingItem *el : el_) {
        if (el->isNote()) {
            el = toNote(el)->chord();
        }
        if (el->isChordRest()) {
            if (track != muse::nidx && el->track() != track) {
                continue;
            }
            if (cr) {
                if (toChordRest(el)->tick() < cr->tick()) {
                    cr = toChordRest(el);
                }
            } else {
                cr = toChordRest(el);
            }
        }
    }
    return cr;
}

ChordRest *Selection::lastChordRest(track_idx_t track) const {
    if (isRange()) {
        return lastChordRestInRange(track);
    }
    if (el_.size() == 1) {
        EngravingItem *el = el_[0];
        if (el) {
            if (el->isNote()) {
                return toChordRest(el->ownershipParent());
            } else if (el->isChordRest()) {
                return toChordRest(el);
            }
        }
        return nullptr;
    }
    ChordRest *cr = nullptr;
    for (EngravingItem *el : el_) {
        if (el->isNote()) {
            el = toNote(el)->chord();
        }
        if (el->isChordRest() && toChordRest(el)->segment()->isChordRestType()) {
            if (track != muse::nidx && el->track() != track) {
                continue;
            }
            if (cr) {
                if (toChordRest(el)->tick() >= cr->tick()) {
                    cr = toChordRest(el);
                }
            } else {
                cr = toChordRest(el);
            }
        }
    }
    return cr;
}

Measure *Selection::findMeasure() const {
    Measure *m = 0;
    if (el_.size() > 0) {
        EngravingItem *el = el_[0];
        m = toMeasure(el->findMeasure());
    }
    return m;
}

MeasureBase *Selection::startMeasureBase() const {
    EngravingItem *selectionElement = element();
    if (selectionElement) {
        if (selectionElement->isBox()) {
            return toMeasureBase(selectionElement);
        }
        MeasureBase *mb = selectionElement->findMeasureBase();
        if (mb) {
            return mb;
        }
    }

    if (startBox_) {
        return startBox_;
    }

    if (tickStart().negative()) { // Tick is not set
        return nullptr;
    }

    MeasureBase *mb = score_->tick2measureBase(tickStart());
    if (mb && mb->isMeasure()) {
        mb = toMeasure(mb)->coveringMMRestOrThis();
    }
    return mb;
}

MeasureBase *Selection::endMeasureBase() const {
    EngravingItem *selectionElement = element();
    if (selectionElement) {
        if (selectionElement->isBox()) {
            return toMeasureBase(selectionElement);
        }
        MeasureBase *mb = selectionElement->findMeasureBase();
        if (mb) {
            return mb;
        }
    }

    if (endBox_) {
        return endBox_;
    }

    if (tickEnd().negative()) { // Tick is not set
        return nullptr;
    }

    MeasureBase *mb = score_->tick2measureBase(tickEnd() - Fraction::eps());
    if (mb && mb->isMeasure()) {
        mb = toMeasure(mb)->coveringMMRestOrThis();
    }
    return mb;
}

std::vector<System *> Selection::selectedSystems() const {
    EngravingItem *el = element();
    if (el && (el->isSystemLockIndicator() || el->isPageLockIndicator() /*TODO: || el->isStaffVisibilityIndicator*/)) {
        return {const_cast<System *>(toIndicatorIcon(el)->system())};
    }

    const MeasureBase *startMB = startMeasureBase();
    const MeasureBase *endMB = endMeasureBase();
    if (!startMB || !endMB) {
        return {};
    }

    std::vector<System *> systems;
    for (const MeasureBase *mb = startMB; mb && mb->isBeforeOrEqual(endMB); mb = mb->nextMM()) {
        if (!mb->isMeasure() && !mb->isHBox()) {
            continue;
        }
        System *sys = mb->system();
        IF_ASSERT_FAILED(sys) { continue; }
        if (systems.empty() || sys != systems.back()) {
            systems.push_back(sys);
        }
    }

    return systems;
}

std::vector<Page *> mu::engraving::Selection::pagesContainingSelection() const {
    EngravingItem *el = element();
    if (el && (el->isPageLockIndicator())) {
        return {const_cast<Page *>(toPageLockIndicator(el)->page())};
    }

    std::vector<System *> systems = selectedSystems();
    std::vector<Page *> pages;
    for (System *system : systems) {
        Page *page = system->page();
        if (page && std::find(pages.begin(), pages.end(), page) == pages.end()) {
            pages.push_back(page);
        }
    }

    return pages;
}

void Selection::deselectAll() {
    if (state_ == SelState::RANGE) {
        score_->setUpdateAll();
    }
    clear();
    updateState();
}

static RectF changeSelection(EngravingItem *e, bool b) {
    RectF r = e->canvasBoundingRect();
    e->setSelected(b);
    r.unite(e->canvasBoundingRect());
    return r;
}

void Selection::clear() {
    IF_ASSERT_FAILED(!isLocked()) {
        LOGE() << "selection locked, reason: " << lockReason();
        return;
    }

    for (EngravingItem *e : el_) {
        e->score()->addRefresh(changeSelection(e, false));
        if (e->isSpanner()) { // TODO: only visible elements should be selectable?
            Spanner *sp = toSpanner(e);
            for (auto s : sp->spannerSegments()) {
                e->score()->addRefresh(changeSelection(s, false));
            }
        }
    }
    el_.clear();
    startSegment_ = 0;
    endSegment_ = 0;
    startBox_ = 0;
    endBox_ = 0;
    activeSegment_ = 0;
    staffStart_ = 0;
    staffEnd_ = 0;
    activeTrack_ = 0;
    setState(SelState::NONE);
}

void Selection::remove(EngravingItem *el) {
    const bool removed = muse::remove(el_, el);
    el->setSelected(false);
    if (removed) {
        updateState();
    }
}

void Selection::add(EngravingItem *el) {
    IF_ASSERT_FAILED(!isLocked()) {
        LOGE() << "selection locked, reason: " << lockReason();
        return;
    }
    el_.push_back(el);
    update();
}

void Selection::appendFiltered(EngravingItem *e) {
    IF_ASSERT_FAILED(!isLocked()) {
        LOGE() << "selection locked, reason: " << lockReason();
        return;
    }
    if (canSelectVoice(e->voice()) && canSelect(e)) {
        el_.push_back(e);
    }
}

void Selection::appendFiltered(const std::unordered_set<EngravingItem *> &elems) {
    IF_ASSERT_FAILED(!isLocked()) {
        LOGE() << "selection locked, reason: " << lockReason();
        return;
    }

    for (EngravingItem *elem : elems) {
        IF_ASSERT_FAILED(!elem->isSpannerSegment()) {
            LOGE() << "Append whole spanners instead of spanner segments";
            continue;
        }

        // Special handling for bends...
        if (elem->isGuitarBend()) {
            GuitarBend *bend = toGuitarBend(elem);
            appendGuitarBend(bend);
            continue;
        }

        // Special handling for spanners...
        if (elem->isSpanner() && !elem->isPartialTie() && !elem->isLaissezVib()) {
            const Spanner *spanner = toSpanner(elem);
            if (!noteAnchoredSpannerIsInRange(spanner, tickStart(), tickEnd())) {
                continue;
            }
            for (SpannerSegment *spannerSeg : spanner->spannerSegments()) {
                appendFiltered(spannerSeg);
            }
            continue;
        }

        // Special handling for grace notes...
        if (elem->isChord() && toChord(elem)->isGrace()) {
            appendChordRest(toChordRest(elem));
            continue;
        }

        appendFiltered(elem);
    }
}

void Selection::appendChordRest(ChordRest *cr) {
    IF_ASSERT_FAILED(!isLocked()) {
        LOGE() << "selection locked, reason: " << lockReason();
        return;
    }

    if (!canSelectVoice(cr->voice())) {
        return;
    }

    const std::unordered_set<EngravingItem *> crAnchored = collectElementsAnchoredToChordRest(cr);
    appendFiltered(crAnchored);

    if (cr->isRestFamily()) {
        appendFiltered(cr);
        Rest *r = toRest(cr);
        for (int i = 0; i < r->dots(); ++i) {
            appendFiltered(r->dot(i));
        }
        return;
    }

    IF_ASSERT_FAILED(cr->isChord()) { return; }

    Chord *chord = toChord(cr);

    const size_t totalNotesInChord = chord->notes().size();
    const bool isSingleNote = totalNotesInChord == 1;

    size_t totalAppendedNotes = 0;
    for (size_t noteIdx = 0; noteIdx < totalNotesInChord; ++noteIdx) {
        Note *note = chord->notes().at(noteIdx);

        const std::unordered_set<EngravingItem *> noteAnchored = collectElementsAnchoredToNote(note, true, false);
        appendFiltered(noteAnchored);

        if (chord->isGrace() && !canSelect(chord)) {
            continue;
        }

        //! Hack Explainer: Due to the fact that this method is called while we're still in the process of "building" our
        //! selection, we can't know for certain whether the selection as a whole will contain multi-note Chords (and thus
        //! whether the "includeSingleNotes" flag should apply - see includeSingleNotes in select.h). For this reason, this
        //! method ALWAYS appends single note chords. If single note chords should be ommitted from a selection, we simply
        //! don't call this method (see usage of appendChordRest in Selection::updateSelectedElements).
        if (/*hack*/ !isSingleNote && !canSelectNoteIdx(noteIdx, totalNotesInChord, /*hack*/ true)) {
            continue;
        }

        el_.push_back(note);
        ++totalAppendedNotes;

        if (note->accidental()) {
            el_.push_back(note->accidental());
        }
        for (EngravingItem *el : note->el()) {
            if (el->isFingering()) {
                // Slight hack (already handled, see collectElementsAnchoredToNote)...
                continue;
            }
            el_.push_back(el);
        }
        for (NoteDot *dot : note->dots()) {
            el_.push_back(dot);
        }
    }

    //! NOTE: Beams, stems, etc should only be added if all notes in chord are selected...
    if (totalAppendedNotes < totalNotesInChord) {
        return;
    }
    if (chord->beam() && !muse::contains(el_, static_cast<EngravingItem *>(chord->beam()))) {
        el_.push_back(chord->beam());
    }
    if (chord->stem()) {
        el_.push_back(chord->stem());
    }
    if (chord->hook()) {
        el_.push_back(chord->hook());
    }
    if (chord->stemSlash()) {
        el_.push_back(chord->stemSlash());
    }
}

void Selection::appendTupletHierarchy(Tuplet *innermostTuplet) {
    if (!canSelectVoice(innermostTuplet->voice()) || muse::contains(el_, static_cast<EngravingItem *>(innermostTuplet))) {
        return;
    }

    //! NOTE: Not hugely efficient since canSelectTuplet will recursively check all contained tuplets...
    if (!selectionFilter().canSelectTuplet(innermostTuplet, tickStart(), tickEnd(), rangeContainsMultiNoteChords())) {
        return;
    }

    el_.push_back(innermostTuplet);

    // Recursively append upwards/outwards
    Tuplet *outerTuplet = innermostTuplet->tuplet();
    if (outerTuplet) {
        appendTupletHierarchy(outerTuplet);
    }
}

void Selection::appendGuitarBend(GuitarBend *guitarBend) {
    if (!guitarBend || !canSelectVoice(guitarBend->voice())) {
        return;
    }

    el_.push_back(guitarBend);

    if (GuitarBendHold *hold = guitarBend->holdLine()) {
        if (hold->tick2() < tickEnd()) {
            el_.push_back(hold);
        }
    }

    if (GuitarBendSegment *bendSeg = toGuitarBendSegment(guitarBend->frontSegment())) {
        if (GuitarBendText *bendText = bendSeg->bendText()) {
            el_.push_back(bendText);
        }
    }
}

void Selection::updateSelectedElements() {
    IF_ASSERT_FAILED(!isLocked()) {
        LOGE() << "selection locked, reason: " << lockReason();
        return;
    }
    if (state_ != SelState::RANGE) {
        rangeContainsMultiNoteChords_ = false;
        update();
        return;
    }

    // TODO: Quite a lot of nesting - should clean this up...
    if (state_ == SelState::RANGE && plannedTick1_ != Fraction(-1, 1) && plannedTick2_ != Fraction(-1, 1)) {
        const staff_idx_t staffStart = staffStart_;
        const staff_idx_t staffEnd = staffEnd_;
        Box *startBox = startBox_;
        Box *endBox = endBox_;

        deselectAll();

        Segment *s1 = nullptr;
        Segment *s2 = nullptr;

        if (startBox && endBox && plannedTick1_ == plannedTick2_) {
            // Box-only range...
            if (plannedTick1_ == score_->endTick()) {
                s1 = score_->lastSegmentMM();
            } else {
                s1 = score_->tick2segmentMM(plannedTick1_, true, SegmentType::Duration);
                s2 = s1;
            }
        } else {
            s1 = score_->tick2segmentMM(plannedTick1_);
            s2 = score_->tick2segmentMM(plannedTick2_, /* first */ true /* HACK */);
            if (s2 && s2->measure()->isMMRest()) {
                s2 = s2->prev1MM(); // HACK
            }
            // These hacks are needed to prevent https://musescore.org/node/173381.
            // This should exclude any segments belonging to MM-rest range from the selection.
            if (s1 && s2 && s1->tick() + s1->ticks() > s2->tick()) {
                // can happen with MM rests as tick2measure returns only the first segment for them.

                plannedTick1_ = Fraction(-1, 1);
                plannedTick2_ = Fraction(-1, 1);
                return;
            }

            if (s2 && s2 == s2->measure()->first()) {
                // we want the last segment of the previous measure (unless it's part of a MMrest)
                Measure *prevMeasure = s2->measure()->prevMeasure();
                if (!(prevMeasure && prevMeasure != prevMeasure->coveringMMRestOrThis())) {
                    s2 = s2->prev1();
                }
            }
        }

        setRange(s1, s2, staffStart, staffEnd, startBox, endBox);

        plannedTick1_ = Fraction(-1, 1);
        plannedTick2_ = Fraction(-1, 1);
    }

    for (EngravingItem *e : el_) {
        e->setSelected(false);
    }
    el_.clear();

    // assert:
    size_t staves = score_->nstaves();
    if (staffStart_ == muse::nidx || staffStart_ >= staves || staffEnd_ == muse::nidx || staffEnd_ > staves || staffStart_ >= staffEnd_) {
        LOGD("updateSelectedElements: bad staff selection %zu - %zu, staves %zu", staffStart_, staffEnd_, staves);
        staffStart_ = 0;
        staffEnd_ = 0;
    }
    track_idx_t startTrack = staffStart_ * VOICES;
    track_idx_t endTrack = staffEnd_ * VOICES;

    //! NOTE: See appendChord/appendChordRest - we should include single notes if the selection consists solely of
    //! single notes, even if the "include single notes" filter flag is false...
    std::vector<Chord *> singleNoteChords;
    size_t totalChordsFound = 0;

    //! NOTE: We also need to delay the appending of tuplets. We should only display tuplets as selected
    //! if all of their contained elements are selected...
    std::unordered_set<Tuplet *> innerTuplets;

    for (track_idx_t st = startTrack; st < endTrack; ++st) {
        for (Segment *s = startSegment_; s && (s != endSegment_); s = s->next1MM()) {
            if (!s->enabled() || s->isEndBarLineType()) { // do not select end bar line
                continue;
            }
            for (EngravingItem *e : s->annotations()) {
                if (e->track() != st) {
                    continue;
                }
                if (e->isFretDiagram()) {
                    FretDiagram *fd = toFretDiagram(e);
                    if (Harmony *harm = fd->harmony()) {
                        appendFiltered(harm);
                    }
                }
                appendFiltered(e);
            }
            EngravingItem *e = s->element(st);
            if (!e || e->generated() || e->isTimeSig() || e->isKeySig()) {
                continue;
            }

            if (!e->isChordRest()) {
                appendFiltered(e);
                continue;
            }

            ChordRest *cr = toChordRest(e);
            if (Tuplet *tuplet = cr->tuplet()) {
                innerTuplets.emplace(tuplet);
            }

            if (!cr->isChord()) {
                appendChordRest(cr);
                continue;
            }

            Chord *chord = toChord(cr);
            const std::vector<Note *> notes = chord->notes();
            if (notes.size() == 1) {
                singleNoteChords.emplace_back(chord);
            } else {
                appendChordRest(chord);
            }
            ++totalChordsFound;
        }
    }

    rangeContainsMultiNoteChords_ = totalChordsFound > singleNoteChords.size();

    for (Chord *singleNoteChord : singleNoteChords) {
        if (!rangeContainsMultiNoteChords_ || selectionFilter().includeSingleNotes()) {
            appendChordRest(singleNoteChord);
        } else {
            // Include elements anchored to the note even if the note itself isn't included...
            const Note *note = singleNoteChord->notes().front();
            const std::unordered_set<EngravingItem *> noteAnchored = collectElementsAnchoredToNote(note, true, false);
            appendFiltered(noteAnchored);
            const std::unordered_set<EngravingItem *> crAnchored = collectElementsAnchoredToChordRest(singleNoteChord);
            appendFiltered(crAnchored);
        }
    }

    for (Tuplet *tuplet : innerTuplets) {
        appendTupletHierarchy(tuplet);
    }

    const Fraction rangeStart = tickStart();
    const Fraction rangeEnd = tickEnd();

    //! NOTE: Ties are NOT handled in here - these are note anchored and handled in appendChordRest...
    for (auto i = score_->spanner().begin(); i != score_->spanner().end(); ++i) {
        Spanner *sp = (*i).second;
        // ignore spanners belonging to other tracks
        if (sp->track() < startTrack || sp->track() >= endTrack) {
            continue;
        }

        const bool startAndEndCalculated = sp->startElement() && sp->endElement();
        if (!startAndEndCalculated || !canSelectVoice(sp->track()) || sp->isVolta()) {
            continue;
        }

        //! NOTE: isZeroLengthAtStart (for lack of a better name) covers an edge case that may occur if sp spans from
        //! a grace note to its parent note or vice versa. In this scenario sp->tick == sp->tick2 == rangeStart and
        //! we should include the spanner in our selection...
        const bool overlapsSelectionRange = rangeStart < sp->tick2() && rangeEnd > sp->tick();
        const bool isZeroLengthAtStart = sp->tick() == sp->tick2() && sp->tick() == rangeStart;
        if (!overlapsSelectionRange && !isZeroLengthAtStart) {
            continue;
        }

        for (auto seg : sp->spannerSegments()) {
            appendFiltered(seg);
        }
    }
    update();
    score_->setSelectionChanged(true);
}

bool Selection::rangeContainsMultiNoteChords() const {
    IF_ASSERT_FAILED(state_ == SelState::RANGE) { return false; }
    return rangeContainsMultiNoteChords_;
}

void Selection::setStartSegment(Segment *s) {
    startSegment_ = s;
    validateBoxesAndSegments();
}

void Selection::setEndSegment(Segment *s) {
    endSegment_ = s;
    validateBoxesAndSegments();
}

void Selection::validateBoxesAndSegments() {
    if (startBox_) {
        IF_ASSERT_FAILED(startSegment_ && startSegment_->tick() == startBox_->tick()) { startBox_ = nullptr; }
    }

    if (endBox_) {
        const Fraction endTick = endSegment_ ? endSegment_->tick() : score_->endTick();
        IF_ASSERT_FAILED(endTick == endBox_->tick()) { endBox_ = nullptr; }
    }
}

void Selection::setRange(Segment *startSegment, Segment *endSegment, staff_idx_t staffStart, staff_idx_t staffEnd, Box *startBox,
                         Box *endBox) {
    assert(staffEnd > staffStart && staffEnd <= score_->nstaves());
    assert(!(endSegment && !startSegment));

    startSegment_ = startSegment;
    endSegment_ = endSegment;
    startBox_ = startBox;
    endBox_ = endBox;
    validateBoxesAndSegments();

    activeSegment_ = endSegment;
    staffStart_ = staffStart;
    staffEnd_ = staffEnd;
    activeTrack_ = staff2track(staffStart);

    if (state_ == SelState::RANGE) {
        score_->setSelectionChanged(true);
    } else {
        setState(SelState::RANGE);
    }
}

//---------------------------------------------------------
//   setRangeTicks
//    sets the range to be selected on next
//    updateSelectedElements() call. Can be used if some
//    segment structure changes are expected (e.g. if
//    creating MM rests is pending).
//---------------------------------------------------------

void Selection::setRangeTicks(const Fraction &tick1, const Fraction &tick2, staff_idx_t staffStart, staff_idx_t staffEnd, Box *startBox,
                              Box *endBox) {
    assert(staffEnd > staffStart && staffEnd <= score_->nstaves());

    plannedTick1_ = tick1;
    plannedTick2_ = tick2;
    startSegment_ = endSegment_ = activeSegment_ = nullptr;
    startBox_ = startBox;
    endBox_ = endBox;
    staffStart_ = staffStart;
    staffEnd_ = staffEnd;
    activeTrack_ = staff2track(staffStart);

    if (state_ == SelState::RANGE) {
        score_->setSelectionChanged(true);
    } else {
        setState(SelState::RANGE);
    }
}

//---------------------------------------------------------
//   update
///   Set select flag for all Elements in select list.
//---------------------------------------------------------

void Selection::update() {
    for (EngravingItem *e : el_) {
        e->setSelected(true); // also tells accessibility that e has focus
    }

    ChordRest *cr = activeCR();
    if (!cr) {
        updateState();
        return;
    }

    // Only one element can have focus at a time, and currently the final element in el_ has
    // focus. That's ok for a LIST selection (because it corresponds to the last element the
    // user clicked on) but for range selections, we should focus something in activeCR...
    EngravingItem *toSelectAgain = nullptr;
    if (cr->isChord()) {
        // Use the top selected note in the chord...
        const std::vector<Note *> notes = toChord(cr)->notes();
        const size_t noteCount = notes.size();
        for (size_t noteIdx = noteCount - 1; noteIdx < noteCount; --noteIdx) {
            Note *note = notes.at(noteIdx);
            if (selectionFilter().canSelectNoteIdx(noteIdx, notes.size(), rangeContainsMultiNoteChords())) {
                toSelectAgain = note;
                break;
            }
        }
    } else {
        toSelectAgain = cr;
    }

    if (!toSelectAgain || !toSelectAgain->selected()) {
        // Means we have a range with no selected elements...
        updateState();
        return;
    }

    toSelectAgain->setSelected(true); // HACK: select it again so accessibility thinks it has focus

    updateState();
}

void Selection::dump() {
    LOGD("Selection dump: ");
    switch (state_) {
        case SelState::NONE: LOGD("NONE"); return;
        case SelState::RANGE: LOGD("RANGE"); break;
        case SelState::LIST: LOGD("LIST"); break;
    }
    for (const EngravingItem *e : el_) {
        LOGD("  %p %s", e, e->typeName());
    }
}

bool Selection::rangeIsValid() const {
    if (!isRange()) {
        return false;
    }

    //! NOTE: startSegment_ being non-null and endSegment_ being null is a
    //! valid case. It means we've selected the last segment of the final measure...
    if (!startSegment_) {
        return false;
    }

    if (endSegment_ && endSegment_->tick() <= startSegment_->tick()) {
        const bool isBoxOnlyRange = startBox_ && endBox_ && tickStart() == tickEnd() && startBox_->isBeforeOrEqual(endBox_);
        if (!isBoxOnlyRange) {
            return false;
        }
    }

    const size_t totalStaves = score_->nstaves();
    if (staffStart_ == muse::nidx || staffStart_ >= totalStaves) {
        return false;
    }
    if (staffEnd_ == muse::nidx || staffEnd_ > totalStaves) {
        return false;
    }

    return staffStart_ < staffEnd_;
}

//---------------------------------------------------------
//   updateState
///   update selection and input state
//---------------------------------------------------------

void Selection::updateState() {
    //! NOTE: Range can be valid even if no elements are selectable in that range...
    if (el_.size() == 0 && !rangeIsValid()) {
        setState(SelState::NONE);
    } else if (state_ == SelState::NONE) {
        setState(SelState::LIST);
    }

    const EngravingItem *e = element();
    if (!e) {
        return;
    }

    if (e->isSpannerSegment()) {
        currentTick_ = toSpannerSegment(e)->spanner()->tick();
    } else {
        currentTick_ = e->tick();
    }
    // ignore system elements (e.g., frames)
    if (e->track() != muse::nidx) {
        currentTrack_ = e->track();
    }
}

void Selection::setState(SelState s) {
    if (state_ == s) {
        return;
    }

    state_ = s;
    score_->setSelectionChanged(true);
}

String Selection::mimeType() const {
    switch (state_) {
        case SelState::LIST: return isSingle() ? String::fromAscii(mimeSymbolFormat) : String::fromAscii(mimeSymbolListFormat);
        case SelState::RANGE: return String::fromAscii(mimeStaffListFormat);
        case SelState::NONE:
        default: break;
    }

    return String();
}

muse::ByteArray Selection::mimeData() const {
    muse::ByteArray a;
    switch (state_) {
        case SelState::LIST:
            if (isSingle()) {
                a = element()->mimeData();
            } else {
                a = symbolListMimeData();
            }
            break;
        case SelState::NONE: break;
        case SelState::RANGE: a = staffMimeData(); break;
    }
    return a;
}

muse::ByteArray Selection::staffMimeData() const {
    return rw::RWRegister::writer()->writeStaffSelection(score_, selectionFilter(), staffStart(), staffEnd(), tickStart(), tickEnd(),
                                                         startSegment_, endSegment_);
}

muse::ByteArray Selection::symbolListMimeData() const {
    struct MapData {
        EngravingItem *e;
        Segment *s;
    };

    track_idx_t topTrack = 1000000;
    track_idx_t bottomTrack = 0;
    Segment *firstSeg = 0;
    Fraction firstTick = Fraction(0x7FFFFFFF, 1);
    Segment *seg = 0;
    std::multimap<int64_t, MapData> map;

    // scan selection element list, inserting relevant elements in a tick-sorted map
    for (EngravingItem *e : el_) {
        switch (e->type()) {
            case ElementType::ARTICULATION:
            case ElementType::ORNAMENT:
            case ElementType::TAPPING:
            case ElementType::ARPEGGIO:
            case ElementType::CHORD_BRACKET:
            case ElementType::TREMOLO_SINGLECHORD: {
                // ignore articulations not attached to chords/rest or segment
                if (!e->ownershipParent()->isChordRest()) {
                    continue;
                }
                ChordRest *cr = toChordRest(e->ownershipParent());
                seg = cr->segment();
            } break;
            case ElementType::FERMATA: seg = toFermata(e)->segment(); break;
            case ElementType::BREATH: seg = toBreath(e)->segment(); break;
            case ElementType::PLAYTECH_ANNOTATION:
            case ElementType::CAPO:
            case ElementType::STRING_TUNINGS:
            case ElementType::STAFF_TEXT:
            case ElementType::STAVE_SHARING_LABEL: seg = toStaffTextBase(e)->segment(); break;
            case ElementType::EXPRESSION: seg = toExpression(e)->segment(); break;
            case ElementType::STICKING: seg = toSticking(e)->segment(); break;
            case ElementType::FIGURED_BASS: seg = toFiguredBass(e)->segment(); break;
            case ElementType::LYRICS: seg = toLyrics(e)->segment(); break;
            case ElementType::DYNAMIC: seg = toDynamic(e)->segment(); break;
            case ElementType::HARMONY:
            case ElementType::FRET_DIAGRAM:
                // ignore chord symbols or fret diagrams not attached to segment
                if (e->ownershipParent()->isSegment()) {
                    seg = toSegment(e->ownershipParent());
                    break;
                }
                continue;
            case ElementType::HARP_DIAGRAM: seg = toHarpPedalDiagram(e)->segment(); break;
            case ElementType::SLUR_SEGMENT:
            case ElementType::HAIRPIN_SEGMENT:
            case ElementType::OTTAVA_SEGMENT:
            case ElementType::TRILL_SEGMENT:
            case ElementType::LET_RING_SEGMENT:
            case ElementType::VIBRATO_SEGMENT:
            case ElementType::PALM_MUTE_SEGMENT:
            case ElementType::WHAMMY_BAR_SEGMENT:
            case ElementType::RASGUEADO_SEGMENT:
            case ElementType::HARMONIC_MARK_SEGMENT:
            case ElementType::PICK_SCRAPE_SEGMENT:
            case ElementType::TEXTLINE_SEGMENT:
            case ElementType::PEDAL_SEGMENT: e = toSpannerSegment(e)->spanner(); [[fallthrough]];
            case ElementType::SLUR:
            case ElementType::HAMMER_ON_PULL_OFF:
            case ElementType::HAIRPIN:
            case ElementType::OTTAVA:
            case ElementType::TRILL:
            case ElementType::LET_RING:
            case ElementType::VIBRATO:
            case ElementType::PALM_MUTE:
            case ElementType::WHAMMY_BAR:
            case ElementType::RASGUEADO:
            case ElementType::HARMONIC_MARK:
            case ElementType::PICK_SCRAPE:
            case ElementType::TEXTLINE:
            case ElementType::PEDAL: seg = toSpanner(e)->startSegment(); break;
            default:
                // Elements of other types are ignored. To allow copying them,
                // add support for them here and in `Score::pasteSymbols`.
                continue;
        }
        track_idx_t track = e->track();
        if (track < topTrack) {
            topTrack = track;
        }
        if (track > bottomTrack) {
            bottomTrack = track;
        }
        if (seg->tick() < firstTick) {
            firstSeg = seg;
            firstTick = seg->tick();
        }
        MapData mapData = {e, seg};
        map.insert(std::pair<int64_t, MapData>(((int64_t) track << 32) + seg->tick().ticks(), mapData));
    }

    // scan the map, computing for each element a relative track and tick offset,
    // and the number of CR segments to skip
    std::vector<SelectedSymbol> symbols;
    track_idx_t currTrack = muse::nidx;
    for (auto iter = map.cbegin(); iter != map.cend(); ++iter) {
        int numSegs;
        track_idx_t track = static_cast<track_idx_t>(iter->first >> 32);
        if (currTrack != track) {
            currTrack = track;
            seg = firstSeg;
        }
        int tickOffset = static_cast<int>(iter->first & 0xFFFFFFFF) - firstTick.ticks();
        numSegs = 0;
        // with figured bass, we need to look for the proper segment
        // not only according to ChordRest elements, but also annotations
        if (iter->second.e->isFiguredBass()) {
            bool done = false;
            for (; seg; seg = seg->next1()) {
                if (seg->isChordRestType()) {
                    // if no ChordRest in right track, look in annotations
                    if (seg->element(currTrack) == nullptr) {
                        for (EngravingItem *el : seg->annotations()) {
                            // do annotations include our element?
                            if (el == iter->second.e) {
                                done = true;
                                break;
                            }
                            // do annotations include any f.b.?
                            if (el->isFiguredBass() && el->track() == track) {
                                numSegs++; //yes: it counts as a step
                                break;
                            }
                        }
                        if (done) {
                            break;
                        }
                        continue; // segment is not relevant: no ChordRest nor f.b.
                    } else {
                        if (iter->second.s == seg) {
                            break;
                        }
                    }
                    numSegs++;
                }
            }
        } else {
            while (seg && iter->second.s != seg) {
                seg = seg->nextCR(currTrack);
                numSegs++;
            }
        }
        symbols.push_back({track, tickOffset, numSegs, iter->second.e});
    }

    return rw::RWRegister::writer()->writeSymbolListSelection(topTrack, bottomTrack, symbols);
}

std::vector<EngravingItem *> Selection::elements(ElementType type) const {
    std::vector<EngravingItem *> result;

    for (EngravingItem *element : el_) {
        if (element->type() == type) {
            result.push_back(element);
        }
    }

    return result;
}

std::vector<Note *> Selection::noteList(track_idx_t selTrack) const {
    std::vector<Note *> nl;

    if (state_ == SelState::LIST) {
        for (EngravingItem *e : el_) {
            if (e->isNote()) {
                nl.push_back(toNote(e));
            }
        }
    } else if (state_ == SelState::RANGE) {
        for (staff_idx_t staffIdx = staffStart(); staffIdx < staffEnd(); ++staffIdx) {
            track_idx_t startTrack = staffIdx * VOICES;
            track_idx_t endTrack = startTrack + VOICES;
            for (Segment *seg = startSegment_; seg && seg != endSegment_; seg = seg->next1()) {
                if (!(seg->segmentType() & (SegmentType::ChordRest))) {
                    continue;
                }
                for (track_idx_t track = startTrack; track < endTrack; ++track) {
                    if (!canSelectVoice(track)) {
                        continue;
                    }
                    EngravingItem *e = seg->element(track);
                    if (e == 0 || !e->isChord() || (selTrack != muse::nidx && selTrack != track)) {
                        continue;
                    }
                    Chord *c = toChord(e);
                    const std::vector<Note *> notes = c->notes();
                    for (size_t noteIdx = 0; noteIdx < notes.size(); ++noteIdx) {
                        Note *note = notes.at(noteIdx);
                        if (selectionFilter().canSelectNoteIdx(noteIdx, notes.size(), rangeContainsMultiNoteChords())) {
                            nl.push_back(note);
                        }
                    }
                    for (Chord *g : c->graceNotes()) {
                        nl.insert(nl.end(), g->notes().begin(), g->notes().end());
                    }
                }
            }
        }
    }
    return nl;
}

//---------------------------------------------------------
//   checkStartForPartialCopy
//     return no error if element is NOT a tuplet or is start of a tuplet/tremolo
//     return error if element is part of a tuplet/tremolo, but not the start
//---------------------------------------------------------

static MsError checkStartForPartialCopy(EngravingItem *e) {
    if (!e || !e->isChordRest()) {
        return MsError::MS_NO_ERROR;
    }

    const ChordRest *cr = toChordRest(e);
    if (cr->tuplet()) {
        // check that complete tuplet is selected, all the way up to top level
        Tuplet *tuplet = cr->tuplet();
        while (tuplet) {
            if (tuplet->elements().front() != e) {
                return MsError::SOURCE_PARTIAL_TUPLET;
            }
            e = tuplet;
            tuplet = tuplet->tuplet();
        }
    }

    if (cr->isChord()) {
        const Chord *chord = toChord(cr);
        const TremoloTwoChord *ttc = chord ? chord->tremoloTwoChord() : nullptr;
        if (ttc && ttc->chord2() == chord) {
            return MsError::SOURCE_PARTIAL_TREMOLO;
        }
    }

    return MsError::MS_NO_ERROR;
}

//---------------------------------------------------------
//   checkEndForPartialCopy
//     return no error if element is NOT a tuplet or is end of a tuplet/tremolo
//     return error if element is part of a tuplet/tremolo, but not the end
//---------------------------------------------------------

static MsError checkEndForPartialCopy(EngravingItem *e, const Fraction &endTick) {
    if (!e || !e->isChordRest()) {
        return MsError::MS_NO_ERROR;
    }

    const ChordRest *cr = toChordRest(e);
    if (cr->tuplet()) {
        // check that complete tuplet is selected, all the way up to top level
        Tuplet *tuplet = cr->tuplet();
        while (tuplet) {
            if (tuplet->elements().back() != e) {
                return MsError::SOURCE_PARTIAL_TUPLET;
            }
            e = tuplet;
            tuplet = tuplet->tuplet();
        }
        // also check that the selection extends to the end of the top-level tuplet
        tuplet = toTuplet(e);
        if (tuplet->endTick() > endTick) {
            return MsError::SOURCE_PARTIAL_TUPLET;
        }
    }

    if (cr->isChord()) {
        const Chord *chord = toChord(cr);
        const TremoloTwoChord *ttc = chord ? chord->tremoloTwoChord() : nullptr;
        if (ttc && ttc->chord1() == chord) {
            return MsError::SOURCE_PARTIAL_TREMOLO;
        }
    }

    return MsError::MS_NO_ERROR;
}

//---------------------------------------------------------
//   canCopy
//    return false if range selection intersects a tuplet
//    or a tremolo, or a local time signature, or only part
//    of a measure repeat group
//---------------------------------------------------------

bool Selection::canCopy() const {
    if (state_ != SelState::RANGE) {
        return true;
    }

    Fraction endTick = endSegment_ ? endSegment_->tick() : score_->lastSegment()->tick();
    Fraction timeStretch(1, 0);

    for (staff_idx_t staffIdx = staffStart_; staffIdx != staffEnd_; ++staffIdx) {
        for (voice_idx_t voice = 0; voice < VOICES; ++voice) {
            track_idx_t track = staffIdx * VOICES + voice;
            if (!canSelectVoice(track)) {
                continue;
            }

            // check first cr in track within selection
            ChordRest *check = startSegment_->nextChordRest(track);
            if (check && check->tick() < endTick) {
                const MsError err = checkStartForPartialCopy(check);
                if (err != MsError::MS_NO_ERROR) {
                    MScore::setError(err);
                    return false;
                }
            }

            if (!endSegment_) {
                continue;
            }

            // find last segment in the selection.
            // Note that _endSegment is the first segment after the selection

            Segment *endSegmentSelection = startSegment_;
            while (endSegmentSelection->nextCR(track) && (endSegmentSelection->nextCR(track)->tick() < endSegment_->tick())) {
                endSegmentSelection = endSegmentSelection->nextCR(track);
            }

            const MsError err = checkEndForPartialCopy(endSegmentSelection->element(track), endTick);
            if (err != MsError::MS_NO_ERROR) {
                MScore::setError(err);
                return false;
            }
        }

        // Check that all selected measures have the same time stretch - allows copy/paste within a local time signature,
        // but don't yet support it between differing local time signatures.
        for (Measure *m = startSegment_->measure(); m && m->tick() < endTick; m = m->nextMeasure()) {
            Fraction mTimeStretch = score_->staff(staffIdx)->timeStretch(m->tick());
            if (!timeStretch.isValid()) {
                timeStretch = mTimeStretch;
            } else if (timeStretch != mTimeStretch) {
                return false;
            }
        }

        // check if selection starts or ends partway through measure repeat group
        const ChordRest *firstCR = firstChordRest();
        const ChordRest *lastCR = lastChordRest();
        if ((firstCR && firstCR->measure()->isMeasureRepeatGroupWithPrevM(staffIdx)) ||
            (lastCR && lastCR->measure()->isMeasureRepeatGroupWithNextM(staffIdx))) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   measureBaseRange
//    return false if no measure range selected
//---------------------------------------------------------

bool Selection::measureBaseRange(MeasureBase **mb1, MeasureBase **mb2) const {
    if (!isRange()) {
        return false;
    }

    if (startBox_) {
        *mb1 = startBox_;
    } else {
        *mb1 = startSegment()->measure();
    }

    Segment *s2 = endSegment();
    if (endBox_) {
        *mb2 = endBox_;
    } else {
        *mb2 = s2 ? s2->measure() : score_->lastMeasure();
    }

    if (*mb1 == *mb2) {
        return true;
    }

    // if selection extends to last segment of a measure,
    // then endSegment() will point to next measure
    // this won't normally happen because end barlines are excluded from range selection
    // but just in case, detect this and back up one measure
    if (*mb2 && s2) {
        if ((*mb2)->isMeasure() && (*mb2)->tick() == s2->tick()) {
            *mb2 = (*mb2)->prevMeasure();
        }
    }
    return true;
}

//---------------------------------------------------------
//   uniqueElements
//    Return list of selected elements.
//    If some elements are linked, only one of the linked
//    elements show up in the list.
//---------------------------------------------------------

const std::list<EngravingItem *> Selection::uniqueElements() const {
    std::list<EngravingItem *> l;

    for (EngravingItem *e : elements()) {
        bool alreadyThere = false;
        for (EngravingItem *ee : l) {
            if ((ee->links() && ee->links()->contains(e)) || e == ee) {
                alreadyThere = true;
                break;
            }
        }
        if (!alreadyThere) {
            l.push_back(e);
        }
    }
    return l;
}

//---------------------------------------------------------
//   uniqueNotes
//    Return list of selected notes.
//    If some notes are linked, only one of the linked
//    elements show up in the list.
//---------------------------------------------------------

std::vector<Note *> Selection::uniqueNotes(track_idx_t track, bool tied) const {
    std::vector<Note *> l;

    auto addNote = [&l](Note *note) {
        bool alreadyThere = false;
        for (Note *n : l) {
            if ((n->links() && n->links()->contains(note)) || n == note) {
                alreadyThere = true;
                break;
            }
        }
        if (!alreadyThere) {
            l.push_back(note);
        }
    };

    for (Note *nn : noteList(track)) {
        if (!tied) {
            addNote(nn);
            continue;
        }

        for (Note *note : nn->tiedNotes()) {
            addNote(note);
        }
    }
    return l;
}

bool Selection::elementsSelected(const ElementTypeSet &types) const {
    for (const EngravingItem *element : el_) {
        if (muse::contains(types, element->type())) {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------
//   extendRangeSelection
//    Extends the range selection to contain the given
//    chord rest.
//---------------------------------------------------------

void Selection::extendRangeSelection(ChordRest *cr) {
    extendRangeSelection(cr->segment(), cr->nextSegmentAfterCR(SegmentType::ChordRest | SegmentType::EndBarLine | SegmentType::Clef),
                         cr->staffIdx(), cr->tick(), cr->tick());
}

//---------------------------------------------------------
//   extendRangeSelection
//    Extends the range selection to contain the given
//    segment. SegAfter should represent the segment
//    that is after seg. Tick and etick represent
//    the start and end tick of an element. Useful when
//    extending by a chord rest.
//---------------------------------------------------------

void Selection::extendRangeSelection(Segment *seg, Segment *segAfter, staff_idx_t staffIdx, const Fraction &tick, const Fraction &etick,
                                     Box *box) {
    bool activeSegmentIsStart = false;
    staff_idx_t activeStaff = activeTrack_ / VOICES;

    // TODO: Quite a lot of nesting - should clean this up...
    if (!box) {
        if (staffIdx < staffStart_) {
            staffStart_ = staffIdx;
        } else if (staffIdx >= staffEnd_) {
            staffEnd_ = staffIdx + 1;
        } else if (staffEnd_ - staffStart_ > 1) {                            // at least 2 staff selected
            if (staffIdx == staffStart_ + 1 && activeStaff == staffStart_) { // going down
                staffStart_ = staffIdx;
            } else if (staffIdx == staffEnd_ - 2 && activeStaff == staffEnd_ - 1) { // going up
                staffEnd_ = staffIdx + 1;
            }
        }
    }

    // start/end tick may not have changed, but a Box at the same tick may still lie outside the range...
    bool extendsBackwards = tick < tickStart();
    bool extendsForwards = etick > tickEnd();
    if (box) {
        if (box->tick() == tickStart()) {
            extendsBackwards = !startBox_ || box->isBefore(startBox_);
        }
        if (box->tick() == tickEnd()) {
            extendsForwards = !endBox_ || endBox_->isBefore(box);
        }
    }

    if (extendsBackwards) {
        // Range extends backwards...
        startSegment_ = seg;
        startBox_ = box;
        activeSegmentIsStart = true;
    } else if (extendsForwards) {
        // Range extends forwards...
        endSegment_ = segAfter;
        endBox_ = box;
    } else {
        if (activeSegment_ == startSegment_) {
            startSegment_ = seg;
            startBox_ = box;
            activeSegmentIsStart = true;
        } else {
            endSegment_ = segAfter;
            endBox_ = box;
        }
    }
    validateBoxesAndSegments();

    activeSegment_ = activeSegmentIsStart ? startSegment_ : endSegment_;
    score_->setSelectionChanged(true);
    assert(!(endSegment_ && !startSegment_));
}

SelectionFilter Selection::selectionFilter() const { return score_->selectionFilter(); }

bool Selection::canSelectNoteIdx(size_t noteIdx, size_t totalNotesInChord, bool selectionContainsMultiNoteChords) const {
    return selectionFilter().canSelectNoteIdx(noteIdx, totalNotesInChord, selectionContainsMultiNoteChords);
}
