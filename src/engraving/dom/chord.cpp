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

#include "chord.h"

#include <cmath>
#include <vector>

#include "containers.h"

#include "../editing/addremoveelement.h"
#include "../editing/editchord.h"
#include "../editing/editnote.h"
#include "../editing/navigation.h"

#include "accidental.h"
#include "arpeggio.h"
#include "chordbracket.h"
#include "articulation.h"
#include "beam.h"
#include "chordline.h"
#include "dom/parenthesis.h"
#include "drumset.h"
#include "factory.h"
#include "guitarbend.h"
#include "hook.h"
#include "key.h"
#include "ledgerline.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "notedot.h"
#include "noteevent.h"
#include "noteline.h"
#include "ornament.h"
#include "part.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "stafftype.h"
#include "stem.h"
#include "stemslash.h"
#include "stringdata.h"
#include "system.h"
#include "tie.h"
#include "tremolosinglechord.h"
#include "tremolotwochord.h"
#include "trill.h"
#include "tuplet.h"
#include "utils.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
NoteParenthesisInfo::NoteParenthesisInfo(Parenthesis *lParen, Parenthesis *rParen, std::vector<Note *> nList)
    : leftParen_(lParen), rightParen_(rParen), notes_(nList) {
    std::sort(notes_.begin(), notes_.end(), noteIsBefore);
}

NoteParenthesisInfo::~NoteParenthesisInfo() {
    delete leftParen_;
    delete rightParen_;
}

void NoteParenthesisInfo::insertNote(Note *note) {
    notes_.insert(std::upper_bound(notes_.begin(), notes_.end(), note, noteIsBefore), note);
}

void NoteParenthesisInfo::removeNote(Note *note) { muse::remove(notes_, note); }

//---------------------------------------------------------
//   upNote
//---------------------------------------------------------

Note *Chord::upNote() const {
    assert(!notes_.empty());

    Note *result = notes_.back();
    const Staff *stf = staff();
    if (!stf) {
        return result;
    }

    const StaffType *st = stf->staffTypeForElement(this);
    if (st->isDrumStaff()) {
        for (Note *n : notes_) {
            if (n->line() < result->line()) {
                result = n;
            }
        }
    } else if (st->isTabStaff()) {
        int line = st->lines(); // start at bottom line
        int noteLine;
        // scan each note: if TAB strings are not in sequential order,
        // visual order of notes might not correspond to pitch order
        for (Note *n : notes_) {
            if (n->shouldHideFret()) {
                continue;
            }
            noteLine = st->physStringToVisual(n->string());
            if (noteLine < line) {
                line = noteLine;
                result = n;
            }
        }
    }
    return result;
}

//---------------------------------------------------------
//   downNote
//---------------------------------------------------------

Note *Chord::downNote() const {
    assert(!notes_.empty());

    Note *result = notes_.front();
    const Staff *stf = staff();
    if (!stf) {
        return result;
    }

    const StaffType *st = stf->staffTypeForElement(this);
    if (st->isDrumStaff()) {
        for (Note *n : notes_) {
            if (n->line() > result->line()) {
                result = n;
            }
        }
    } else if (st->isTabStaff()) {
        int line = 0; // start at top line
        int noteLine;
        // scan each note: if TAB strings are not in sequential order,
        // visual order of notes might not correspond to pitch order
        for (Note *n : notes_) {
            if (n->shouldHideFret()) {
                continue;
            }
            noteLine = st->physStringToVisual(n->string());
            if (noteLine > line) {
                line = noteLine;
                result = n;
            }
        }
    }
    return result;
}

//---------------------------------------------------------
//   upLine / downLine
//---------------------------------------------------------

int Chord::upLine() const { return onTabStaff() ? upString() * 2 : upNote()->line(); }

int Chord::downLine() const { return onTabStaff() ? downString() * 2 : downNote()->line(); }

//---------------------------------------------------------
//   upString / downString
//
//    return the topmost / bottommost string used by chord
//    Top and bottom refer to the DRAWN position, not the position in the instrument
//    (i.e., upside-down TAB are taken into account)
//
//    If no staff, always return 0
//    If staff is not a TAB, always returns TOP and BOTTOM staff lines
//---------------------------------------------------------

int Chord::upString() const {
    // if no staff or staff not a TAB, return 0 (=topmost line)
    const Staff *st = staff();
    if (!st) {
        return 0;
    }

    const StaffType *tab = st->staffTypeForElement(this);
    if (!tab->isTabStaff()) {
        return 0;
    }

    int line = tab->lines() - 1; // start at bottom line
    int noteLine;
    // scan each note: if TAB strings are not in sequential order,
    // visual order of notes might not correspond to pitch order
    size_t n = notes_.size();
    for (size_t i = 0; i < n; ++i) {
        noteLine = tab->physStringToVisual(notes_.at(i)->string());
        if (noteLine < line) {
            line = noteLine;
        }
    }
    return line;
}

int Chord::downString() const {
    const Staff *st = staff();
    if (!st) { // if no staff, return 0
        return 0;
    }

    const StaffType *tab = st->staffTypeForElement(this);
    if (!tab->isTabStaff()) { // if staff not a TAB, return bottom line
        return st->lines(tick()) - 1;
    }

    int line = 0; // start at top line
    int noteLine;
    size_t n = notes_.size();
    for (size_t i = 0; i < n; ++i) {
        noteLine = tab->physStringToVisual(notes_.at(i)->string());
        if (noteLine > line) {
            line = noteLine;
        }
    }
    return line;
}

//---------------------------------------------------------
//   noteDistances
//---------------------------------------------------------

std::vector<int> Chord::noteDistances() const {
    const StaffType *staffType = this->staffType();
    assert(staffType);
    bool isTabStaff = staffType->isTabStaff();
    int staffMiddleLine = staffType->middleLine();

    std::vector<int> distances;
    distances.reserve(notes_.size());
    for (Note *note : notes_) {
        int noteLine = isTabStaff ? note->string() : note->line();
        distances.push_back(noteLine - staffMiddleLine);
    }
    return distances;
}

//---------------------------------------------------------
//   Chord
//---------------------------------------------------------

Chord::Chord(DummyParentOr<Segment> parent) : ChordRest(ElementType::CHORD, parent) {
    stem_ = 0;
    hook_ = 0;
    stemDirection_ = DirectionV::AUTO;
    arpeggio_ = 0;
    spanArpeggio_ = 0;
    endsNoteAnchoredLine_ = false;
    noteType_ = NoteType::NORMAL;
    stemSlash_ = 0;
    noStem_ = false;
    showStemSlash_ = noteType_ == NoteType::ACCIACCATURA;
    playEventType_ = PlayEventType::Auto;
    spaceLw_ = 0.;
    spaceRw_ = 0.;
    crossMeasure_ = CrossMeasure::UNKNOWN;
    graceIndex_ = 0;
    combineVoice_ = AutoOnOff::AUTO;
}

Chord::Chord(const Chord &c, bool link) : ChordRest(c, link) {
    if (link) {
        score()->undo(new Link(this, const_cast<Chord *>(&c)));
    }

    for (Note *onote : c.notes_) {
        Note *nnote = Factory::copyNote(*onote, link);
        add(nnote);
    }
    for (Chord *gn : c.graceNotes()) {
        Chord *nc = new Chord(*gn, link);
        add(nc);
    }
    for (Articulation *a : c.articulations_) { // make deep copy
        Articulation *na = a->clone();
        if (link) {
            score()->undo(new Link(na, a));
        }
        na->setOwnershipParent(this);
        na->setTrack(track());
        articulations_.push_back(na);
    }
    stem_ = 0;
    hook_ = 0;
    endsNoteAnchoredLine_ = false;
    arpeggio_ = 0;
    stemSlash_ = 0;

    spanArpeggio_ = c.spanArpeggio_;
    graceIndex_ = c.graceIndex_;
    noStem_ = c.noStem_;
    showStemSlash_ = c.showStemSlash_;
    playEventType_ = c.playEventType_;
    stemDirection_ = c.stemDirection_;
    noteType_ = c.noteType_;
    crossMeasure_ = CrossMeasure::UNKNOWN;
    combineVoice_ = c.combineVoice_;

    if (c.stem_) {
        add(Factory::copyStem(*(c.stem_)));
    }
    if (c.hook_) {
        add(new Hook(*(c.hook_)));
    }
    if (c.stemSlash_) {
        add(Factory::copyStemSlash(*(c.stemSlash_)));
    }
    if (c.arpeggio_) {
        Arpeggio *a = c.arpeggio_->isChordBracket() ? new ChordBracket(*toChordBracket(c.arpeggio_)) : new Arpeggio(*(c.arpeggio_));
        add(a);
        if (link) {
            score()->undo(new Link(a, const_cast<Arpeggio *>(c.arpeggio_)));
        }
    }

    if (c.tremoloSingleChord_) {
        TremoloSingleChord *t = Factory::copyTremoloSingleChord(*(c.tremoloSingleChord_));
        if (link) {
            score()->undo(new Link(t, const_cast<TremoloSingleChord *>(c.tremoloSingleChord_)));
        }
        add(t);
    } else if (c.tremoloTwoChord_) {
        if (c.tremoloTwoChord_->chord1() == &c) {
            TremoloTwoChord *t = Factory::copyTremoloTwoChord(*(c.tremoloTwoChord_));
            if (link) {
                score()->undo(new Link(t, const_cast<TremoloTwoChord *>(c.tremoloTwoChord_)));
            }
            t->setChords(this, nullptr);
            add(t);
        }
    }

    for (EngravingItem *e : c.el()) {
        if (e->isChordLine()) {
            ChordLine *cl = toChordLine(e);
            ChordLine *ncl = Factory::copyChordLine(*cl);
            add(ncl);
            if (cl->note()) {
                ncl->setNote(findNote(cl->note()->pitch()));
            }
            if (link) {
                score()->undo(new Link(ncl, cl));
            }
        }
    }

    for (EngravingItem *e : c.el()) {
        if (e->isChordBracket()) {
            EngravingItem *clonedChordBracket = e->linkedClone();
            add(clonedChordBracket);
        }
    }

    if (!c.noteParentheses().empty()) {
        for (const NoteParenthesisInfo *info : c.noteParentheses()) {
            Parenthesis *newLeftParen = toParenthesis(info->leftParen()->clone());
            newLeftParen->setOwnershipParent(this);
            Parenthesis *newRightParen = toParenthesis(info->rightParen()->clone());
            newRightParen->setOwnershipParent(this);

            if (link && !info->leftParen()->generated()) {
                score()->undo(new Link(newLeftParen, info->leftParen()));
            }
            if (link && !info->rightParen()->generated()) {
                score()->undo(new Link(newRightParen, info->rightParen()));
            }

            std::vector<Note *> newNotes;
            newNotes.reserve(info->notes().size());
            for (Note *note : info->notes()) {
                newNotes.push_back(findNote(note->pitch()));
            }

            noteParens_.push_back(new NoteParenthesisInfo(newLeftParen, newRightParen, newNotes));
        }
    }
}

//---------------------------------------------------------
//   undoUnlink
//---------------------------------------------------------

void Chord::undoUnlink() {
    ChordRest::undoUnlink();
    for (Note *n : notes_) {
        n->undoUnlink();
    }
    for (Chord *gn : graceNotes()) {
        gn->undoUnlink();
    }
    for (Articulation *a : articulations_) {
        a->undoUnlink();
    }
    /*      if (_glissando)
            _glissando->undoUnlink(); */
    if (arpeggio_) {
        arpeggio_->undoUnlink();
    }

    if (tremoloSingleChord()) {
        tremoloSingleChord()->undoUnlink();
    }

    for (EngravingItem *e : el()) {
        if (e->isChordLine()) {
            e->undoUnlink();
        }
    }
}

//---------------------------------------------------------
//   ~Chord
//---------------------------------------------------------

Chord::~Chord() {
    muse::DeleteAll(articulations_);

    if (tremoloTwoChord()) {
        if (tremoloTwoChord()->chord1() == this) {
            tremoloTwoChord()->setChord1(nullptr);
        } else if (tremoloTwoChord()->chord2() == this) {
            tremoloTwoChord()->setChord2(nullptr);
        }
    }

    delete arpeggio_;
    delete stemSlash_;
    delete stem_;
    delete hook_;
    muse::DeleteAll(ledgerLines_);
    muse::DeleteAll(graceNotes_);
    muse::DeleteAll(notes_);
    muse::DeleteAll(noteParens_);
}

//---------------------------------------------------------
//   noteHeadWidth
//---------------------------------------------------------

double Chord::noteHeadWidth() const { return score()->noteHeadWidth() * mag(); }

//---------------------------------------------------------
//   rightEdge
//---------------------------------------------------------

double Chord::rightEdge() const {
    double right = 0.0;
    for (Note *n : notes()) {
        right = std::max(right, x() + n->x() + n->bboxRightPos());
    }

    return right;
}

TremoloType Chord::tremoloType() const {
    if (tremoloSingleChord()) {
        return tremoloSingleChord()->tremoloType();
    } else if (tremoloTwoChord()) {
        return tremoloTwoChord()->tremoloType();
    } else {
        return TremoloType::INVALID_TREMOLO;
    }
}

TremoloTwoChord *Chord::tremoloTwoChord() const { return tremoloTwoChord_; }

TremoloSingleChord *Chord::tremoloSingleChord() const { return tremoloSingleChord_; }

void Chord::setTremoloTwoChord(TremoloTwoChord *tr, bool applyLogic) {
    if (tremoloTwoChord_ && tr && tr == tremoloTwoChord_) {
        return;
    }

    if (tremoloTwoChord_) {
        TDuration d;
        const Fraction f = ticks();
        if (f.numerator() > 0) {
            d = TDuration(f);
        } else {
            d = tremoloTwoChord_->durationType();
            const int dots = d.dots();
            d = d.shift(1);
            d.setDots(dots);
        }

        setDurationType(d);
        Chord *other = tremoloTwoChord_->chord1() == this ? tremoloTwoChord_->chord2() : tremoloTwoChord_->chord1();
        tremoloTwoChord_ = nullptr;
        if (other) {
            other->setTremoloTwoChord(nullptr);
        }
    }
    tremoloSingleChord_ = nullptr;

    if (tr && applyLogic) {
        TDuration d = tr->durationType();
        if (!d.isValid()) {
            d = durationType();
            const int dots = d.dots();
            d = d.shift(-1);
            d.setDots(dots);
            tr->setDurationType(d);
        }

        setDurationType(d);
        Chord *other = tr->chord1() == this ? tr->chord2() : tr->chord1();
        tremoloTwoChord_ = tr;
        if (other) {
            other->setTremoloTwoChord(tr);
        }
    }

    tremoloTwoChord_ = tr;
}

void Chord::setTremoloSingleChord(TremoloSingleChord *tr) {
    if (tremoloSingleChord_ && tr && tr == tremoloSingleChord_) {
        return;
    }

    if (tremoloTwoChord_) {
        TDuration d;
        const Fraction f = ticks();
        if (f.numerator() > 0) {
            d = TDuration(f);
        } else {
            d = tremoloTwoChord_->durationType();
            const int dots = d.dots();
            d = d.shift(1);
            d.setDots(dots);
        }

        setDurationType(d);
        Chord *other = tremoloTwoChord_->chord1() == this ? tremoloTwoChord_->chord2() : tremoloTwoChord_->chord1();
        tremoloTwoChord_ = nullptr;
        if (other) {
            other->setTremoloTwoChord(nullptr);
        }
    }

    tremoloSingleChord_ = tr;
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Chord::add(EngravingItem *e) {
    if (e->ownershipParent() != this) {
        e->setOwnershipParent(this);
    }
    e->setTrack(track());
    switch (e->type()) {
        case ElementType::NOTE: {
            Note *note = toNote(e);
            bool tab = staff() && staff()->isTabStaff(tick());
            bool found = false;

            for (unsigned idx = 0; idx < notes_.size(); ++idx) {
                if (tab) {
                    // _notes should be sorted by string
                    if (note->string() > notes_[idx]->string()) {
                        notes_.insert(notes_.begin() + idx, note);
                        found = true;
                        break;
                    }
                } else {
                    // _notes should be sorted by line position,
                    // but it's often not yet possible since line is unknown
                    // use pitch instead, and line as a second sort criteria.
                    if (note->pitch() <= notes_[idx]->pitch()) {
                        if (note->pitch() == notes_[idx]->pitch() && note->line() >= notes_[idx]->line()) {
                            notes_.insert(notes_.begin() + idx + 1, note);
                        } else {
                            notes_.insert(notes_.begin() + idx, note);
                        }
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                notes_.push_back(note);
            }
            note->connectTiedNotes();
            if (voice() && measure() && note->visible()) {
                measure()->setHasVoices(staffIdx(), true);
            }
        } break;
        case ElementType::ARPEGGIO: arpeggio_ = toArpeggio(e); break;
        case ElementType::CHORD_BRACKET: addEl(e); break;
        case ElementType::TREMOLO_TWOCHORD: setTremoloTwoChord(item_cast<TremoloTwoChord *>(e)); break;
        case ElementType::TREMOLO_SINGLECHORD: setTremoloSingleChord(item_cast<TremoloSingleChord *>(e)); break;
        case ElementType::GLISSANDO: endsNoteAnchoredLine_ = true; break;
        case ElementType::STEM:
            assert(!stem_);
            stem_ = toStem(e);
            break;
        case ElementType::HOOK: hook_ = toHook(e); break;
        case ElementType::CHORDLINE: addEl(e); break;
        case ElementType::STEM_SLASH:
            assert(!stemSlash_);
            stemSlash_ = toStemSlash(e);
            break;
        case ElementType::CHORD: {
            Chord *gc = toChord(e);
            assert(gc->noteType() != NoteType::NORMAL);
            size_t idx = gc->graceIndex();
            gc->setFlag(ElementFlag::MOVABLE, true);
            graceNotes_.insert(graceNotes_.begin() + idx, gc);
        } break;
        case ElementType::LEDGER_LINE: ASSERT_X("Chord::add ledgerline"); break;
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING: {
            Articulation *a = toArticulation(e);
            if (a->layoutCloseToNote()) {
                if (a->isStaccato()) {
                    // staccato is always the first articulation
                    articulations_.insert(articulations_.begin(), a);
                } else {
                    auto i = articulations_.begin();
                    while (i != articulations_.end() && (*i)->layoutCloseToNote()) {
                        i++;
                    }
                    articulations_.insert(i, a);
                }
            } else {
                articulations_.push_back(a);
            }
        } break;
        default: ChordRest::add(e); return;
    }

    e->added();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Chord::remove(EngravingItem *e) {
    if (!e) {
        return;
    }

    switch (e->type()) {
        case ElementType::NOTE: {
            Note *note = toNote(e);
            auto i = std::find(notes_.begin(), notes_.end(), note);
            if (i != notes_.end()) {
                notes_.erase(i);
                note->disconnectTiedNotes();
                for (Spanner *s : note->spannerBack()) {
                    note->removeSpannerBack(s);
                }
                for (Spanner *s : note->spannerFor()) {
                    note->removeSpannerFor(s);
                }
                EditChord::removeChordParentheses(this, {note});
            } else {
                LOGD("Chord::remove() note %p not found!", e);
            }
            if (voice() && measure() && note->visible()) {
                measure()->checkMultiVoices(staffIdx());
            }
        } break;

        case ElementType::ARPEGGIO:
            if (spanArpeggio_ == arpeggio_) {
                spanArpeggio_ = nullptr;
            }
            arpeggio_ = nullptr;
            break;
        case ElementType::CHORD_BRACKET: removeEl(e); break;
        case ElementType::TREMOLO_TWOCHORD: setTremoloTwoChord(nullptr); break;
        case ElementType::TREMOLO_SINGLECHORD: setTremoloSingleChord(nullptr); break;
        case ElementType::GLISSANDO: endsNoteAnchoredLine_ = false; break;
        case ElementType::STEM: stem_ = 0; break;
        case ElementType::HOOK: hook_ = 0; break;
        case ElementType::STEM_SLASH:
            assert(stemSlash_);
            if (stemSlash_->selected() && score()) {
                score()->deselect(stemSlash_);
            }
            stemSlash_ = 0;
            break;
        case ElementType::CHORDLINE: removeEl(e); break;
        case ElementType::CHORD: {
            auto i = std::find(graceNotes_.begin(), graceNotes_.end(), toChord(e));
            IF_ASSERT_FAILED(i != graceNotes_.end()) { break; }
            Chord *grace = *i;
            grace->setGraceIndex(i - graceNotes_.begin());
            graceNotes_.erase(i);
        } break;
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING: {
            Articulation *a = toArticulation(e);
            if (!muse::remove(articulations_, a)) {
                LOGD("Chord::remove(): articulation not found");
            }
        } break;

        default: ChordRest::remove(e); return;
    }

    e->removed();
}

//---------------------------------------------------------
//   maxHeadWidth
//---------------------------------------------------------

double Chord::maxHeadWidth() const {
    // determine max head width in chord
    double hw = 0;
    for (const Note *n : notes_) {
        double t = n->headWidth();
        if (t > hw) {
            hw = t;
        }
    }
    return hw;
}

//---------------------------------------------------------
//   selectedNote
//---------------------------------------------------------

Note *Chord::selectedNote() const {
    Note *note = 0;
    size_t n = notes_.size();
    for (size_t i = 0; i < n; ++i) {
        Note *currentNote = notes_.at(i);
        if (currentNote->selected()) {
            if (note) {
                return 0;
            }
            note = currentNote;
        }
    }
    return note;
}

//---------------------------------------------------------
//   upPos
//---------------------------------------------------------

double Chord::upPos() const { return upNote()->pos().y(); }

//---------------------------------------------------------
//   downPos
//---------------------------------------------------------

double Chord::downPos() const { return downNote()->pos().y(); }

bool Chord::allNotesTiedToNext() const {
    Chord *tiedChord = nullptr;
    for (Note *note : notes_) {
        if (!note->tieFor()) {
            return false;
        }

        Note *endNote = note->tieFor()->endNote();
        Chord *endChord = endNote ? endNote->chord() : nullptr;
        if (!endChord) {
            return false;
        }

        if (!tiedChord) {
            tiedChord = endChord;
            continue;
        }

        if (endChord != tiedChord) {
            return false;
        }
    }

    return true;
}

bool Chord::allElementsInvisible() const {
    for (EngravingObject *child : getChildren()) {
        if (toEngravingItem(child)->visible()) {
            return false;
        }
    }

    for (Chord *grace : graceNotes_) {
        if (!grace->allElementsInvisible()) {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------
//   processSiblings
//---------------------------------------------------------

void Chord::processSiblings(std::function<void(EngravingItem *)> func, bool includeTemporarySiblings) const {
    if (hook_) {
        func(hook_);
    }
    if (stem_) {
        func(stem_);
    }
    if (stemSlash_) {
        func(stemSlash_);
    }
    if (arpeggio_) {
        func(arpeggio_);
    }
    if (tremoloTwoChord_) {
        func(tremoloTwoChord_);
    }
    if (tremoloSingleChord_) {
        func(tremoloSingleChord_);
    }
    if (includeTemporarySiblings) {
        for (LedgerLine *ll : ledgerLines_) {
            func(ll);
        }
    }
    for (Articulation *a : articulations_) {
        func(a);
    }
    for (Note *note : notes_) {
        func(note);
    }
    for (Chord *chord : graceNotes_) { // process grace notes last, needed for correct shape calculation
        func(chord);
    }
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void Chord::setTrack(track_idx_t val) {
    ChordRest::setTrack(val);
    processSiblings([val](EngravingItem *e) { e->setTrack(val); }, true);
}

bool Chord::shouldCombineVoice() const {
    return combineVoice() == AutoOnOff::ON || (combineVoice() == AutoOnOff::AUTO && style().styleB(Sid::combineVoice));
}

bool Chord::combineVoice(const Chord *chord1, const Chord *chord2) { return chord1->shouldCombineVoice() && chord2->shouldCombineVoice(); }

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void Chord::setScore(Score *s) {
    ChordRest::setScore(s);
    processSiblings([s](EngravingItem *e) { e->setScore(s); }, true);
}

Fraction Chord::endTickIncludingTied() const {
    const Chord *lastTied = this;
    while (lastTied) {
        const Chord *next = lastTied->nextTiedChord();
        if (next) {
            lastTied = next;
        } else {
            break;
        }
    }
    return lastTied->endTick();
}

Chord *Chord::prev() const {
    ChordRest *prev = Navigation::prevChordRest(const_cast<Chord *>(this));
    if (prev && prev->isChord()) {
        return toChord(prev);
    }
    return nullptr;
}

Chord *Chord::next() const {
    ChordRest *next = Navigation::nextChordRest(const_cast<Chord *>(this));
    if (next && next->isChord()) {
        return toChord(next);
    }
    return nullptr;
}

void Chord::resizeLedgerLinesTo(size_t newSize) {
    int ledgerLineCountDiff = static_cast<int>(newSize - ledgerLines_.size());
    if (ledgerLineCountDiff > 0) {
        for (int i = 0; i < ledgerLineCountDiff; ++i) {
            ledgerLines_.push_back(new LedgerLine(this));
        }
    } else {
        for (int i = 0; i < std::abs(ledgerLineCountDiff); ++i) {
            delete ledgerLines_.back();
            ledgerLines_.pop_back();
        }
    }

    assert(ledgerLines_.size() == newSize);
}

void Chord::setBeamExtension(double extension) {
    if (stem_) {
        double baseLength = stem_->absoluteFromSpatium(stem_->baseLength());
        stem_->setBaseLength(std::max(Spatium::fromAbsolute(baseLength + extension, spatium()), 0.0_sp));
        defaultStemLength_ = std::max(defaultStemLength_ + extension, stem_->absoluteFromSpatium(stem_->baseLength()));
    }
}

bool Chord::shouldHaveStem() const {
    const Staff *staff = this->staff();
    const StaffType *staffType = staff ? staff->staffTypeForElement(this) : nullptr;

    return !noStem_ && durationType().hasStem() &&
           !(durationType().type() == DurationType::V_HALF && staffType && staffType->isTabStaff() &&
             staffType->minimStyle() == TablatureMinimStyle::NONE) &&
           !(measure() && measure()->stemless(staffIdx())) && !(staffType && staffType->isTabStaff() && staffType->stemless());
}

bool Chord::shouldHaveHook() const { return shouldHaveStem() && durationType().hooks() > 0 && !beam() && !tremoloTwoChord(); }

void Chord::createStem() {
    Stem *stem = Factory::createStem(this);
    stem->setGenerated(true);
    //! score()->undoAddElement calls add(), which assigns this created stem to _stem
    score()->doUndoAddElement(stem);
}

void Chord::removeStem() {
    if (stem_) {
        score()->undoRemoveElement(stem_);
    }
    if (hook_) {
        score()->undoRemoveElement(hook_);
    }
    if (stemSlash_) {
        score()->undoRemoveElement(stemSlash_);
    }
}

void Chord::createHook() {
    Hook *hook = new Hook(this);
    hook->setGenerated(true);
    score()->doUndoAddElement(hook);
}

//---------------------------------------------------------
//    underBeam: true, if grace note is placed under a beam.
//---------------------------------------------------------

bool Chord::underBeam() const {
    if (noteType_ == NoteType::NORMAL) {
        return false;
    }
    const Chord *cr = toChord(ownershipParent());
    Beam *beam = cr->beam();
    if (!beam || !cr->beam()->up()) {
        return false;
    }
    size_t s = beam->elements().size();
    if (isGraceBefore()) {
        if (beam->elements()[0] != cr) {
            return true;
        }
    }
    if (isGraceAfter()) {
        if (beam->elements()[s - 1] != cr) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   cmdUpdateNotes
//---------------------------------------------------------

void Chord::cmdUpdateNotes(AccidentalState *as, staff_idx_t staffIdx) {
    // TAB_STAFF is different, as each note has to be fretted
    // in the context of the all of the chords of the whole segment

    const Staff *st = staff();
    StaffGroup staffGroup = st->staffTypeForElement(this)->group();
    if (staffGroup == StaffGroup::TAB) {
        Fraction tick = this->tick();
        const StringData *stringData = originPart()->stringData(tick, st->idx());
        for (Chord *ch : graceNotes()) {
            stringData->fretChords(ch);
        }
        stringData->fretChords(this);
        return;
    } else {
        // if not tablature, use instrument->useDrumset to set staffGroup (to allow pitched to unpitched in same staff)
        staffGroup = originPart()->instrument(this->tick())->useDrumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
    }

    // PITCHED_ and PERCUSSION_STAFF can go note by note

    if (staffGroup == StaffGroup::STANDARD) {
        const std::vector<Chord *> gnb(graceNotesBefore());
        for (Chord *ch : gnb) {
            if (ch->vStaffIdx() != staffIdx) {
                continue;
            }
            std::vector<Note *> notes(ch->notes()); // we need a copy!
            for (Note *note : notes) {
                note->updateAccidental(as);
            }
            ch->sortNotes();
        }
        if (vStaffIdx() == staffIdx) {
            std::vector<Note *> lnotes(notes()); // we need a copy!
            for (Note *note : lnotes) {
                if (note->tieBackNonPartial() && note->tpc() == note->tieBack()->startNote()->tpc()) {
                    // same pitch
                    if (note->accidental() && note->accidental()->role() == AccidentalRole::AUTO) {
                        // not courtesy
                        // TODO: remove accidental only if note is not
                        // on new system
                        score()->undoRemoveElement(note->accidental());
                    }
                }
                note->updateAccidental(as);
            }
            for (Articulation *art : articulations_) {
                if (!art->isOrnament()) {
                    continue;
                }
                Ornament *ornament = toOrnament(art);
                ornament->computeNotesAboveAndBelow(as);
            }
            for (Spanner *spanner : startingSpanners()) {
                if (spanner->isTrill()) {
                    Ornament *ornament = toTrill(spanner)->ornament();
                    if (ornament) {
                        ornament->setOwnershipParent(this);
                        ornament->computeNotesAboveAndBelow(as);
                    }
                }
            }
            sortNotes();
        }
        const std::vector<Chord *> gna(graceNotesAfter());
        for (Chord *ch : gna) {
            if (ch->vStaffIdx() != staffIdx) {
                continue;
            }
            std::vector<Note *> notes(ch->notes()); // we need a copy!
            for (Note *note : notes) {
                note->updateAccidental(as);
            }
            ch->sortNotes();
        }
    } else if (staffGroup == StaffGroup::PERCUSSION) {
        const Instrument *instrument = originPart()->instrument(this->tick());
        const Drumset *drumset = instrument->drumset();
        if (!drumset) {
            LOGW("no drumset");
        }
        updatePercussionNotes(this, drumset);
        sortNotes();
    }
}

//---------------------------------------------------------
//   pagePos
//---------------------------------------------------------

PointF Chord::pagePos() const {
    if (!isGrace()) {
        return EngravingItem::pagePos();
    }

    PointF p(pos());
    if (!ownershipParent()) {
        return p;
    }
    p.rx() = pageX();

    const Chord *pc = toChord(ownershipParent());
    System *system = pc->segment()->system();
    if (!system) {
        return p;
    }
    p.ry() += system->staffYpage(vStaffIdx()) + staffOffsetY();
    return p;
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Chord::scanElements(std::function<void(EngravingItem *)> func) {
    for (Articulation *a : articulations_) {
        a->scanElements(func);
    }
    if (hook_) {
        func(hook_);
    }
    if (stem_) {
        func(stem_);
    }
    if (stemSlash_) {
        func(stemSlash_);
    }
    if (arpeggio_) {
        func(arpeggio_);
    }
    if (tremoloTwoChord_ && (tremoloChordType() != TremoloChordType::TremoloSecondChord)) {
        func(tremoloTwoChord_);
    }
    if (tremoloSingleChord_) {
        func(tremoloSingleChord_);
    }
    const Staff *st = staff();
    if ((st && st->showLedgerLines(tick())) || !st) { // also for palette
        for (LedgerLine *ll : ledgerLines_) {
            func(ll);
        }
    }
    for (Note *note : notes_) {
        note->scanElements(func);
    }
    for (Chord *chord : graceNotes_) {
        chord->scanElements(func);
    }
    for (EngravingItem *e : el()) {
        e->scanElements(func);
    }

    for (auto &p : noteParens_) {
        p->leftParen()->scanElements(func);
        p->rightParen()->scanElements(func);
    }
    ChordRest::scanElements(func);
}

NoteParenthesisInfo *Chord::findNoteParenthesisInfo(const Parenthesis *paren) {
    for (NoteParenthesisInfo *infoPtr : noteParens_) {
        if (paren == infoPtr->leftParen() || paren == infoPtr->rightParen()) {
            return infoPtr;
        }
    }

    return nullptr;
}

const NoteParenthesisInfo *Chord::findNoteParenthesisInfo(const Note *note) const {
    for (const NoteParenthesisInfo *infoPtr : noteParens_) {
        for (const Note *parenNote : infoPtr->notes()) {
            if (parenNote == note) {
                return infoPtr;
            }
        }
    }

    DO_ASSERT(u"Parentheses are not in chord");
    return nullptr;
}

void Chord::addNoteParenthesisInfo(NoteParenthesisInfo *noteParenInfo) { noteParens_.push_back(noteParenInfo); }

void Chord::removeNoteParenthesisInfo(const NoteParenthesisInfo *noteParenInfo) {
    if (noteParens_.empty()) {
        return;
    }

    if (!noteParenInfo) {
        return;
    }

    auto it = std::find_if(noteParens_.begin(), noteParens_.end(),
                           [noteParenInfo](const NoteParenthesisInfo *ptr) { return ptr == noteParenInfo; });

    if (it != noteParens_.end()) {
        noteParens_.erase(it);
    }
}

void Chord::addNoteToParenthesisInfo(Note *note, const Parenthesis *paren) {
    NoteParenthesisInfo *noteParenInfo = findNoteParenthesisInfo(paren);

    if (!noteParenInfo) {
        return;
    }

    noteParenInfo->insertNote(note);
}

void Chord::removeNoteFromParenthesisInfo(Note *note, const Parenthesis *paren) {
    NoteParenthesisInfo *noteParenInfo = findNoteParenthesisInfo(paren);

    if (!noteParenInfo) {
        return;
    }

    noteParenInfo->removeNote(note);
}

//---------------------------------------------------------
//   isChordPlayable
//   @note Now every related to chord element has it's own "PLAY" property,
//         However, there is no way to control these properties outside the scope of the chord since the new Properties panel.
//         So we'll use a chord as a proxy entity for "PLAY" property handling
//---------------------------------------------------------

bool Chord::isChordPlayable() const {
    if (!notes_.empty()) {
        if (notes_.front()->isPreBendOrDiveStart()) {
            return false;
        }

        return notes_.front()->getProperty(Pid::PLAY).toBool();
    } else if (tremoloTwoChord()) {
        return tremoloTwoChord()->getProperty(Pid::PLAY).toBool();
    } else if (tremoloSingleChord()) {
        return tremoloSingleChord()->getProperty(Pid::PLAY).toBool();
    } else if (arpeggio_) {
        return arpeggio_->getProperty(Pid::PLAY).toBool();
    }

    return false;
}

//---------------------------------------------------------
//   setIsChordPlayable
//---------------------------------------------------------

void Chord::setIsChordPlayable(const bool isPlayable) {
    for (Note *note : notes_) {
        note->undoChangeProperty(Pid::PLAY, isPlayable);
    }

    if (arpeggio_) {
        arpeggio_->undoChangeProperty(Pid::PLAY, isPlayable);
    }

    if (tremoloTwoChord()) {
        tremoloTwoChord()->undoChangeProperty(Pid::PLAY, isPlayable);
    }

    if (tremoloSingleChord()) {
        tremoloSingleChord()->undoChangeProperty(Pid::PLAY, isPlayable);
    }

    triggerLayout();
}

//---------------------------------------------------------
//   findNote
//---------------------------------------------------------

Note *Chord::findNote(int pitch, int skip) const {
    size_t ns = notes_.size();
    for (size_t i = 0; i < ns; ++i) {
        Note *n = notes_.at(i);
        if (n->pitch() == pitch) {
            if (skip == 0) {
                return n;
            } else {
                --skip;
            }
        }
    }
    return 0;
}

void Chord::undoChangeSpanArpeggio(Arpeggio *a) {
    if (spanArpeggio_ == a) {
        return;
    }

    // TODO: change arpeggio for links
    score()->undo(new ChangeSpanArpeggio(this, a));
}

ChordLine *Chord::chordLine() const {
    for (EngravingItem *item : el()) {
        if (item && item->isChordLine()) {
            return toChordLine(item);
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Chord::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.dropElement;
    switch (e->type()) {
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING: {
            Articulation *atr = toArticulation(e);
            Articulation *oa = hasArticulation(atr);
            if (oa) {
                delete atr;
                atr = 0;
                // if attribute is already there, remove
                // score()->cmdRemove(oa); // unexpected behaviour?
                score()->select(oa, SelectType::SINGLE, 0);
            } else {
                // intuit anchor for this accidental based on other accidentals in the chord
                int aboveBelow = 0;
                bool mixed = false;
                for (Articulation *a : articulations_) {
                    PropertyFlags pf = a->propertyFlags(Pid::ARTICULATION_ANCHOR);
                    if (pf == PropertyFlags::STYLED) {
                        continue;
                    }
                    if (a->anchor() == ArticulationAnchor::TOP) {
                        if (aboveBelow < 0) {
                            mixed = true;
                            break;
                        }
                        ++aboveBelow;
                    } else if (a->anchor() == ArticulationAnchor::BOTTOM) {
                        if (aboveBelow > 0) {
                            mixed = true;
                            break;
                        }
                        --aboveBelow;
                    }
                }
                if (!mixed && aboveBelow != 0) {
                    if (aboveBelow > 0) {
                        atr->setAnchor(ArticulationAnchor::TOP);
                    } else {
                        atr->setAnchor(ArticulationAnchor::BOTTOM);
                    }
                    atr->setPropertyFlags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
                }
                atr->setOwnershipParent(this);
                atr->setTrack(track());

                // Immediately add if the chord has no existing articulations, toggle otherwise...
                if (articulations_.empty()) {
                    score()->undoAddElement(atr);
                } else {
                    EditChord::toggleArticulation(score(), this, atr);
                }
            }
            return atr;
        }

        case ElementType::CHORDLINE:
            e->setOwnershipParent(this);
            e->setTrack(track());
            score()->undoAddElement(e);
            break;

        case ElementType::TREMOLO_SINGLECHORD:
            if (tremoloSingleChord()) {
                bool sameType = (e->subtype() == tremoloSingleChord()->subtype());
                score()->undoRemoveElement(tremoloSingleChord());
                if (sameType) {
                    delete e;
                    return nullptr;
                }
            }
            e->setOwnershipParent(this);
            e->setTrack(track());
            score()->undoAddElement(e);
            break;

        case ElementType::TREMOLO_TWOCHORD: {
            TremoloTwoChord *t = item_cast<TremoloTwoChord *>(e);
            Segment *s = segment()->next();
            while (s) {
                if (s->element(track()) && s->element(track())->isChord()) {
                    break;
                }
                s = s->next();
            }
            if (s == 0) {
                LOGD("no segment for second note of tremolo found");
                delete e;
                return 0;
            }
            Chord *ch2 = toChord(s->element(track()));
            if (ch2->ticks() != ticks()) {
                LOGD("no matching chord for second note of tremolo found");
                delete e;
                return 0;
            }
            t->setChords(this, ch2);
        }
            if (tremoloTwoChord()) {
                bool sameType = (e->subtype() == tremoloTwoChord()->subtype());
                score()->undoRemoveElement(tremoloTwoChord());
                if (sameType) {
                    delete e;
                    return 0;
                }
            }
            e->setOwnershipParent(this);
            e->setTrack(track());
            score()->undoAddElement(e);
            break;

        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET: {
            Arpeggio *a = toArpeggio(e);
            if (arpeggio() && a->isArpeggio()) {
                score()->undoRemoveElement(arpeggio());
            }
            a->setTrack(track());
            a->setOwnershipParent(this);
            score()->undoAddElement(a);
        }
            return e;

        default: return ChordRest::drop(tx, data);
    }
    return 0;
}

void Chord::setColor(const Color &color) {
    ChordRest::setColor(color);

    for (Note *note : notes_) {
        note->undoChangeProperty(Pid::COLOR, PropertyValue::fromValue(color));
    }
}

//---------------------------------------------------------
//   setStemDirection
//---------------------------------------------------------

void Chord::setStemDirection(DirectionV d) { stemDirection_ = d; }

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void Chord::localSpatiumChanged(double oldValue, double newValue) {
    ChordRest::localSpatiumChanged(oldValue, newValue);
    for (EngravingItem *e : graceNotes()) {
        e->localSpatiumChanged(oldValue, newValue);
    }
    if (hook_) {
        hook_->localSpatiumChanged(oldValue, newValue);
    }
    if (stem_) {
        stem_->localSpatiumChanged(oldValue, newValue);
    }
    if (stemSlash_) {
        stemSlash_->localSpatiumChanged(oldValue, newValue);
    }
    if (arpeggio_) {
        arpeggio_->localSpatiumChanged(oldValue, newValue);
    }

    if (tremoloSingleChord_) {
        tremoloSingleChord_->localSpatiumChanged(oldValue, newValue);
    } else if (tremoloTwoChord_ && (tremoloChordType() != TremoloChordType::TremoloSecondChord)) {
        tremoloTwoChord_->localSpatiumChanged(oldValue, newValue);
    }

    for (EngravingItem *e : articulations()) {
        e->localSpatiumChanged(oldValue, newValue);
    }
    for (Note *note : notes()) {
        note->localSpatiumChanged(oldValue, newValue);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Chord::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::NO_STEM: return noStem();
        case Pid::SHOW_STEM_SLASH: return showStemSlash();
        case Pid::STEM_DIRECTION: return PropertyValue::fromValue<DirectionV>(stemDirection());
        case Pid::PLAY: return isChordPlayable();
        case Pid::COMBINE_VOICE: return PropertyValue::fromValue<AutoOnOff>(combineVoice());
        case Pid::VISIBLE: return true; // Chord cannot be set invisible, only its elements can
        default: return ChordRest::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Chord::propertyDefault(Pid propertyId) const {
    switch (propertyId) {
        case Pid::NO_STEM: return false;
        case Pid::SHOW_STEM_SLASH: return noteType() == NoteType::ACCIACCATURA;
        case Pid::STEM_DIRECTION: return PropertyValue::fromValue<DirectionV>(DirectionV::AUTO);
        case Pid::PLAY: return true;
        case Pid::COMBINE_VOICE: return AutoOnOff::AUTO;
        default: return ChordRest::propertyDefault(propertyId);
    }
}

bool Chord::isUserModified() const {
    if (showStemSlash() != propertyDefault(Pid::SHOW_STEM_SLASH).toBool()) {
        return true;
    }

    return EngravingItem::isUserModified();
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Chord::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::NO_STEM: setNoStem(v.toBool()); break;
        case Pid::SHOW_STEM_SLASH: requestShowStemSlash(v.toBool()); break;
        case Pid::STEM_DIRECTION: setStemDirection(v.value<DirectionV>()); break;
        case Pid::PLAY: setIsChordPlayable(v.toBool()); break;
        case Pid::COMBINE_VOICE: setCombineVoice(v.value<AutoOnOff>()); break;
        default: return ChordRest::setProperty(propertyId, v);
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   hasArticulation
//---------------------------------------------------------

Articulation *Chord::hasArticulation(const Articulation *aa) {
    for (Articulation *a : articulations_) {
        if (a->subtype() == aa->subtype()) {
            return a;
        }
    }
    return 0;
}

Tapping *Chord::tapping() const {
    std::vector<Tapping *> tappings;
    tappings.reserve(1);
    for (Articulation *a : articulations_) {
        if (a->isTapping()) {
            tappings.push_back(toTapping(a));
        }
    }
    DO_ASSERT(tappings.size() <= 1);

    return tappings.size() > 0 ? tappings.front() : nullptr;
}

void Chord::updateArticulations(const std::set<SymId> &newArticulationIds, ArticulationsUpdateMode updateMode) {
    Articulation *staccato = nullptr;
    Articulation *accent = nullptr;
    Articulation *marcato = nullptr;
    Articulation *tenuto = nullptr;
    ArticulationAnchor overallAnchor = ArticulationAnchor::AUTO;
    bool mixedDirections = false;
    if (!articulations_.empty()) {
        std::vector<Articulation *> articsToRemove;
        // split all articulations
        for (Articulation *artic : articulations_) {
            if (!artic->isDouble()) {
                continue;
            }
            auto splitSyms = splitArticulations({artic->symId()});
            for (const SymId &id : splitSyms) {
                Articulation *newArticulation = Factory::createArticulation(this);
                newArticulation->setSymId(id);
                newArticulation->setAnchor(artic->anchor());
                newArticulation->setPropertyFlags(Pid::ARTICULATION_ANCHOR, artic->propertyFlags(Pid::ARTICULATION_ANCHOR));
                if (!hasArticulation(newArticulation)) {
                    EditChord::toggleArticulation(score(), this, newArticulation);
                } else {
                    delete newArticulation;
                }
            }
            articsToRemove.push_back(artic);
        }
        for (Articulation *artic : articsToRemove) {
            score()->undoRemoveElement(artic);
        }
        // now we have guaranteed no more combined artics
        // take an inventory of which articulations are already present
        for (Articulation *artic : articulations_) {
            PropertyFlags pf = artic->propertyFlags(Pid::ARTICULATION_ANCHOR);
            if (!mixedDirections && pf == PropertyFlags::UNSTYLED && artic->anchor() != ArticulationAnchor::AUTO) {
                if (overallAnchor != ArticulationAnchor::AUTO && artic->anchor() != overallAnchor) {
                    mixedDirections = true;
                    overallAnchor = ArticulationAnchor::AUTO;
                } else {
                    overallAnchor = artic->anchor();
                }
            }
            if (artic->isStaccato()) {
                staccato = artic;
            } else if (artic->isAccent()) {
                accent = artic;
            } else if (artic->isMarcato()) {
                marcato = artic;
            } else if (artic->isTenuto()) {
                tenuto = artic;
            }
        }
    }
    std::set<SymId> newArtics;
    // get symbol id's for correct direction
    for (const SymId &id : newArticulationIds) {
        if (id == SymId::articAccentAbove || id == SymId::articAccentBelow) {
            if (marcato && updateMode == ArticulationsUpdateMode::Insert) {
                // adding an accent, which replaces marcato
                score()->undoRemoveElement(marcato);
            }
        } else if (id == SymId::articMarcatoAbove || id == SymId::articMarcatoBelow) {
            if (accent && updateMode == ArticulationsUpdateMode::Insert) {
                // adding a marcato, which replaces accent
                score()->undoRemoveElement(accent);
            }
        }
        newArtics.insert(id);
    }

    // newArtics now contains the articulations in the correct direction
    if (updateMode == ArticulationsUpdateMode::Remove) {
        // remove articulations from _articulations that are found in newArtics
        for (const SymId &id : newArtics) {
            switch (id) {
                case SymId::articAccentAbove:
                case SymId::articAccentBelow:
                    score()->undoRemoveElement(accent);
                    accent = nullptr;
                    break;
                case SymId::articMarcatoAbove:
                case SymId::articMarcatoBelow:
                    score()->undoRemoveElement(marcato);
                    marcato = nullptr;
                    break;
                case SymId::articTenutoAbove:
                case SymId::articTenutoBelow:
                    score()->undoRemoveElement(tenuto);
                    tenuto = nullptr;
                    break;
                case SymId::articStaccatoAbove:
                case SymId::articStaccatoBelow:
                    score()->undoRemoveElement(staccato);
                    staccato = nullptr;
                    break;
                default: break;
            }
        }
    } else {
        // add articulations from newArtics that are not found in articulations_
        for (const SymId &id : newArtics) {
            Articulation *newArticulation = Factory::createArticulation(this);
            newArticulation->setSymId(id);
            if (overallAnchor != ArticulationAnchor::AUTO) {
                newArticulation->setAnchor(overallAnchor);
                newArticulation->setPropertyFlags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
            }
            if (!hasArticulation(newArticulation)) {
                EditChord::toggleArticulation(score(), this, newArticulation);
            } else {
                delete newArticulation;
            }
        }
    }
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Chord::reset() {
    undoChangeProperty(Pid::STEM_DIRECTION, DirectionV::AUTO);
    ChordRest::reset();
}

//---------------------------------------------------------
//   slash
//---------------------------------------------------------

bool Chord::slash() {
    Note *n = upNote();
    return n->fixed();
}

//---------------------------------------------------------
//   setSlash
//---------------------------------------------------------

void Chord::setSlash(bool flag, bool stemless) {
    int line = 0;
    NoteHeadGroup head = NoteHeadGroup::HEAD_SLASH;

    if (!flag) {
        // restore to normal
        undoChangeProperty(Pid::NO_STEM, false);
        undoChangeProperty(Pid::SMALL, false);
        undoChangeProperty(Pid::OFFSET, PointF());
        for (Note *n : notes_) {
            n->undoChangeProperty(Pid::HEAD_GROUP, NoteHeadGroup::HEAD_NORMAL);
            n->undoChangeProperty(Pid::FIXED, false);
            n->undoChangeProperty(Pid::FIXED_LINE, 0);
            n->undoChangeProperty(Pid::PLAY, true);
            n->undoChangeProperty(Pid::VISIBLE, true);
            for (NoteDot *dot : n->dots()) {
                dot->undoChangeProperty(Pid::VISIBLE, true);
            }
            if (staff()->isDrumStaff(tick())) {
                const Drumset *ds = originPart()->instrument(tick())->drumset();
                int pitch = n->pitch();
                if (ds && ds->isValid(pitch)) {
                    undoChangeProperty(Pid::STEM_DIRECTION, ds->stemDirection(pitch));
                    n->undoChangeProperty(Pid::HEAD_GROUP, ds->noteHead(pitch));
                }
            }
        }
        return;
    }

    // make stemless if asked
    if (stemless) {
        undoChangeProperty(Pid::NO_STEM, true);
        undoChangeProperty(Pid::BEAM_MODE, BeamMode::NONE);
    }

    const StaffType *staffType = this->staffType();

    // voice-dependent attributes - line, size, offset, head
    if (track() % VOICES < 2) {
        // use middle line
        line = staffType->middleLine();
        undoChangeProperty(Pid::STEM_DIRECTION, PropertyValue::fromValue<DirectionV>(DirectionV::DOWN));
    } else {
        // set small
        undoChangeProperty(Pid::SMALL, true);
        // set outside the staff
        double y = 0.0;
        if (track() % 2) {
            line = staffType->bottomLine() + 1;
            y = 0.5 * spatium();
            undoChangeProperty(Pid::STEM_DIRECTION, PropertyValue::fromValue<DirectionV>(DirectionV::DOWN));
        } else {
            line = -1;
            if (!staffType->isDrumStaff()) {
                y = -0.5 * spatium();
            }
            undoChangeProperty(Pid::STEM_DIRECTION, PropertyValue::fromValue<DirectionV>(DirectionV::UP));
        }
        // for non-drum staves, add an additional offset
        // for drum staves, no offset, but use normal head
        if (!staffType->isDrumStaff()) {
            mutldata()->moveY(y);
        } else {
            head = NoteHeadGroup::HEAD_NORMAL;
        }
    }

    size_t ns = notes_.size();
    for (size_t i = 0; i < ns; ++i) {
        Note *n = notes_[i];
        n->undoChangeProperty(Pid::HEAD_GROUP, head);
        n->undoChangeProperty(Pid::FIXED, true);
        n->undoChangeProperty(Pid::FIXED_LINE, line);
        n->undoChangeProperty(Pid::PLAY, false);
        // hide all but first notehead
        if (i) {
            n->undoChangeProperty(Pid::VISIBLE, false);
            for (NoteDot *dot : n->dots()) {
                dot->undoChangeProperty(Pid::VISIBLE, false);
            }
        }
    }
}

//---------------------------------------------------------
//  updateEndsNoteAnchoredLine
//    sets/resets the chord endsNoteAnchoredLine_ according any note anchored line
//    end into this chord or no.
//---------------------------------------------------------

void Chord::updateEndsNoteAnchoredLine() {
    endsNoteAnchoredLine_ = false; // assume no note anchored line ends here
    // scan all chord notes for note anchored lines ending on this chord
    for (Note *note : notes()) {
        for (Spanner *sp : note->spannerBack()) {
            if (sp->isGlissando() || (sp->isNoteLine() && toNoteLine(sp)->enforceMinLength())) {
                endsNoteAnchoredLine_ = true;
                return;
            }
        }
    }
}

//---------------------------------------------------------
//   removeMarkings
//    - this is normally called after cloning a chord to tie a note over the barline
//    - there is no special undo handling; the assumption is that undo will simply remove the cloned chord
//    - two note tremolos are converted into simple notes
//    - single note tremolos are optionally retained
//---------------------------------------------------------

void Chord::removeMarkings(bool keepTremolo) {
    if (tremoloSingleChord_ && !keepTremolo) {
        remove(tremoloSingleChord_);
    }

    if (tremoloTwoChord_ && !keepTremolo) {
        remove(tremoloTwoChord_);
    }

    if (arpeggio()) {
        remove(arpeggio());
    }

    if (spanArpeggio_) {
        spanArpeggio_ = nullptr;
    }

    muse::DeleteAll(graceNotes());
    graceNotes().clear();
    muse::DeleteAll(articulations());
    articulations().clear();
    for (Note *n : notes()) {
        for (EngravingItem *e : n->el()) {
            n->remove(e);
        }
    }
    ChordRest::removeMarkings(keepTremolo);
}

//---------------------------------------------------------
//   intrinsicMag
//   returns the INTRINSIC mag of the chord (i.e. NOT scaled
//   by staff size)
//---------------------------------------------------------

double Chord::intrinsicMag() const {
    double m = 1.0;
    if (isSmall()) {
        m *= style().styleD(Sid::smallNoteMag);
    }
    if (noteType_ != NoteType::NORMAL) {
        bool tabGraceSizeException =
            staffType()->isTabStaff() && isPreBendOrGraceBendStart() && !style().styleB(Sid::useCueSizeFretForGraceBends);
        if (!tabGraceSizeException) {
            m *= style().styleD(Sid::graceNoteMag);
        }
    }
    return m;
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Chord::mag() const {
    const Staff *st = staff();
    return (st ? st->staffMag(this) : 1.0) * intrinsicMag();
}

//---------------------------------------------------------
//   segment
//---------------------------------------------------------

Segment *Chord::segment() const {
    EngravingObject *e = ownershipParent();
    for (; e && !e->isSegment(); e = e->ownershipParent()) {}
    return toSegment(e);
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

Measure *Chord::measure() const {
    EngravingObject *e = ownershipParent();
    for (; e && !e->isMeasure(); e = e->ownershipParent()) {}
    return toMeasure(e);
}

//---------------------------------------------------------
//   graceNotesBefore
//---------------------------------------------------------

GraceNotesGroup &Chord::graceNotesBefore(bool filterUnplayable) const {
    graceNotesBefore_.clear();
    for (Chord *c : graceNotes_) {
        assert(c->noteType() != NoteType::NORMAL && c->noteType() != NoteType::INVALID);
        if (c->noteType() & (NoteType::ACCIACCATURA | NoteType::APPOGGIATURA | NoteType::GRACE4 | NoteType::GRACE16 | NoteType::GRACE32)) {
            if (filterUnplayable && !c->isChordPlayable()) {
                continue;
            }

            graceNotesBefore_.push_back(c);
        }
    }
    return graceNotesBefore_;
}

//---------------------------------------------------------
//   graceNotesAfter
//---------------------------------------------------------

GraceNotesGroup &Chord::graceNotesAfter(bool filterUnplayable) const {
    graceNotesAfter_.clear();
    for (int i = static_cast<int>(graceNotes_.size()) - 1; i >= 0; i--) {
        Chord *c = graceNotes_[i];
        assert(c->noteType() != NoteType::NORMAL && c->noteType() != NoteType::INVALID);
        if (c->noteType() & (NoteType::GRACE8_AFTER | NoteType::GRACE16_AFTER | NoteType::GRACE32_AFTER)) {
            if (filterUnplayable && !c->isChordPlayable()) {
                continue;
            }

            graceNotesAfter_.push_back(c);
        }
    }
    return graceNotesAfter_;
}

Chord *Chord::graceNoteAt(size_t idx) const {
    if (idx > graceNotes_.size()) {
        return nullptr;
    }

    return graceNotes_.at(idx);
}

//---------------------------------------------------------
//   allGraceChordsOfMainChord
//   returns a list containing all grace notes (chords) attached to the main chord and the main chord itself, in order
//---------------------------------------------------------
std::vector<Chord *> Chord::allGraceChordsOfMainChord() {
    Chord *mainChord = isGrace() ? toChord(ownershipParent()) : this;
    std::vector<Chord *> chords = {mainChord};
    const GraceNotesGroup &gnBefore = mainChord->graceNotesBefore();
    const GraceNotesGroup &gnAfter = mainChord->graceNotesAfter();
    chords.insert(chords.begin(), gnBefore.begin(), gnBefore.end());
    chords.insert(chords.end(), gnAfter.begin(), gnAfter.end());
    return chords;
}

//---------------------------------------------------------
//   setShowStemSlashInAdvance
//---------------------------------------------------------

void Chord::setShowStemSlashInAdvance() {
    if (noteType_ == NoteType::NORMAL) {
        return;
    }
    if (isGraceBefore()) {
        GraceNotesGroup &graceBefore = toChord(ownershipParent())->graceNotesBefore();
        Chord *grace = graceBefore.empty() ? nullptr : graceBefore.front();
        if (grace && grace->beamMode() != BeamMode::NONE && grace->beamMode() != BeamMode::BEGIN) {
            grace->requestShowStemSlash(showStemSlash());
        }
    }
    if (isGraceAfter()) {
        GraceNotesGroup &graceAfter = toChord(ownershipParent())->graceNotesAfter();
        Chord *grace = graceAfter.empty() ? nullptr : graceAfter.back();
        if (grace && grace->beamMode() != BeamMode::NONE) {
            grace->requestShowStemSlash(showStemSlash());
        }
    }
}

//---------------------------------------------------------
//   requestShowStemSlash
//---------------------------------------------------------

void Chord::requestShowStemSlash(bool show) {
    if (noteType_ == NoteType::NORMAL) {
        return;
    }
    if (beam()) {
        for (ChordRest *chordRest : beam()->elements()) {
            if (chordRest->isChord()) {
                Chord *chord = toChord(chordRest);
                chord->setShowStemSlash(show);
            }
        }
    } else {
        setShowStemSlash(show);
    }
}

//---------------------------------------------------------
//   sortNotes
//---------------------------------------------------------

void Chord::sortNotes() { std::sort(notes().begin(), notes().end(), noteIsBefore); }

//---------------------------------------------------------
//   nextTiedChord
//    Return next chord if all notes in this chord are tied to it.
//    Set backwards=true to return the previous chord instead.
//
//    Note: the next chord might have extra notes that are not tied
//    back to this one. Set sameSize=true to return 0 in this case.
//---------------------------------------------------------

Chord *Chord::nextTiedChord(bool backwards, bool sameSize) const {
    Segment *nextSeg = backwards ? segment()->prev1(SegmentType::ChordRest) : segment()->next1(SegmentType::ChordRest);
    if (!nextSeg) {
        return 0;
    }
    ChordRest *nextCR = nextSeg->nextChordRest(track(), backwards);
    if (!nextCR || !nextCR->isChord()) {
        return 0;
    }
    Chord *next = toChord(nextCR);
    if (sameSize && notes().size() != next->notes().size()) {
        return 0; // sizes don't match so some notes can't be tied
    }
    if (tuplet() != next->tuplet()) {
        return 0; // next chord belongs to a different tuplet
    }
    for (Note *n : notes_) {
        Tie *tie = backwards ? n->tieBack() : n->tieFor();
        if (!tie) {
            return 0; // not tied
        }
        Note *nn = backwards ? tie->startNote() : tie->endNote();
        if (!nn || nn->chord() != next) {
            return 0; // tied to note in wrong voice, or tied over rest
        }
    }
    return next; // all notes in this chord are tied to notes in next chord
}

//---------------------------------------------------------
//   setNoteType
//---------------------------------------------------------

void Chord::setNoteType(NoteType t) {
    noteType_ = t;
    setProperty(Pid::SHOW_STEM_SLASH, propertyDefault(Pid::SHOW_STEM_SLASH));
}

//---------------------------------------------------------
//   toGraceAfter
//---------------------------------------------------------

void Chord::toGraceAfter() {
    switch (noteType()) {
        case NoteType::APPOGGIATURA: setNoteType(NoteType::GRACE8_AFTER); break;
        case NoteType::GRACE16: setNoteType(NoteType::GRACE16_AFTER); break;
        case NoteType::GRACE32: setNoteType(NoteType::GRACE32_AFTER); break;
        default: break;
    }
}

bool Chord::isPreBendOrGraceBendStart() const {
    if (!isGrace()) {
        return false;
    }

    for (const Note *note : notes_) {
        GuitarBend *gb = note->bendFor();
        if (gb && (gb->bendType() == GuitarBendType::PRE_BEND || gb->bendType() == GuitarBendType::GRACE_NOTE_BEND ||
                   gb->bendType() == GuitarBendType::PRE_DIVE)) {
            return true;
        }
    }

    return false;
}

bool Chord::isGraceBendEnd() const {
    if (isGrace() || graceNotes_.empty()) {
        return false;
    }

    for (const Note *note : notes_) {
        GuitarBend *bendBack = note->bendBack();
        if (bendBack && bendBack->bendType() == GuitarBendType::GRACE_NOTE_BEND) {
            return true;
        }
    }

    return false;
}

void Chord::setIsTrillCueNote(bool v) { isTrillCueNote_ = v; }

//---------------------------------------------------------
//   tremoloChordType
//---------------------------------------------------------

TremoloChordType Chord::tremoloChordType() const {
    if (tremoloTwoChord_) {
        if (tremoloTwoChord_->chord1() == this) {
            return TremoloChordType::TremoloFirstChord;
        } else if (tremoloTwoChord_->chord2() == this) {
            return TremoloChordType::TremoloSecondChord;
        } else {
            ASSERT_X(String(u"Chord::tremoloChordType(): inconsistency"));
        }
    } else if (tremoloSingleChord_) {
        return TremoloChordType::TremoloSingle;
    }

    return TremoloChordType::TremoloNone;
}

//---------------------------------------------------------
//   nextElement
//---------------------------------------------------------

EngravingItem *Chord::nextElement() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().front();
    }

    switch (e->type()) {
        case ElementType::SYMBOL:
        case ElementType::IMAGE:
        case ElementType::FINGERING:
        case ElementType::TEXT:
        case ElementType::BEND: {
            Note *n = toNote(e->ownershipParent());
            if (n == notes_.front()) {
                if (arpeggio_) {
                    return arpeggio_;
                } else if (tremoloTwoChord_) {
                    return tremoloTwoChord_;
                } else if (tremoloSingleChord_) {
                    return tremoloSingleChord_;
                }
                break;
            }
            for (auto &i : notes_) {
                if (i == n) {
                    return *(&i - 1);
                }
            }
            break;
        }

        case ElementType::GUITAR_BEND_SEGMENT:
        case ElementType::GLISSANDO_SEGMENT:
        case ElementType::NOTELINE_SEGMENT:
        case ElementType::LAISSEZ_VIB_SEGMENT:
        case ElementType::PARTIAL_TIE_SEGMENT:
        case ElementType::TIE_SEGMENT: {
            SpannerSegment *s = toSpannerSegment(e);
            Spanner *sp = s->spanner();
            EngravingItem *elSt = sp->startElement();
            assert(elSt->isNote());
            Note *n = toNote(elSt);
            assert(n != NULL);
            if (n == notes_.front()) {
                if (arpeggio_) {
                    return arpeggio_;
                } else if (tremoloTwoChord_) {
                    return tremoloTwoChord_;
                } else if (tremoloSingleChord_) {
                    return tremoloSingleChord_;
                }
                break;
            }
            for (auto &i : notes_) {
                if (i == n) {
                    return *(&i - 1);
                }
            }
            break;
        }
        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET:
            if (tremoloTwoChord_) {
                return tremoloTwoChord_;
            } else if (tremoloSingleChord_) {
                return tremoloSingleChord_;
            }
            break;

        case ElementType::ACCIDENTAL:
            e = toAccidental(e)->note();
            // fall through

        case ElementType::NOTE: {
            if (e == notes_.front()) {
                if (arpeggio_) {
                    return arpeggio_;
                } else if (tremoloTwoChord_) {
                    return tremoloTwoChord_;
                } else if (tremoloSingleChord_) {
                    return tremoloSingleChord_;
                }
                break;
            }
            for (auto &i : notes_) {
                if (i == e) {
                    return *(&i - 1);
                }
            }
        } break;

        case ElementType::CHORD: return notes_.back();

        default: break;
    }

    return ChordRest::nextElement();
}

//---------------------------------------------------------
//   prevElement
//---------------------------------------------------------

EngravingItem *Chord::prevElement() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().back();
    }
    switch (e->type()) {
        case ElementType::NOTE: {
            if (isGrace()) {
                ChordRest *prev = Navigation::prevChordRest(this);
                if (prev) {
                    if (prev->isChord()) {
                        return toChord(prev)->notes().back();
                    }
                    return prev;
                }
            }

            GraceNotesGroup &graceNotesBefore = this->graceNotesBefore();
            if (!graceNotesBefore.empty()) {
                if (Chord *graceNotesBeforeLastChord = graceNotesBefore.back()) {
                    if (e == graceNotesBeforeLastChord->notes().back()) {
                        break;
                    }

                    Note *prevNote = graceNotesBeforeLastChord->notes().back();
                    if (prevNote->isPreBendOrDiveStart() || prevNote->isGraceBendStart()) {
                        return prevNote->bendFor()->frontSegment();
                    }

                    ChordRest *prev = Navigation::prevChordRest(this);
                    if (prev) {
                        if (prev->isChord()) {
                            return toChord(prev)->notes().back();
                        }
                        return prev;
                    }
                }
            }

            if (e == notes_.back()) {
                break;
            }
            Note *prevNote = nullptr;
            for (auto &i : notes_) {
                if (i == e) {
                    prevNote = *(&i + 1);
                }
            }
            EngravingItem *next = prevNote->lastElementBeforeSegment();
            return next;
        }

        case ElementType::CHORD: return notes_.front();

        case ElementType::TREMOLO_TWOCHORD:
        case ElementType::TREMOLO_SINGLECHORD:
            if (arpeggio_) {
                return arpeggio_;
            }
        // fall through
        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET: {
            Note *n = notes_.front();
            EngravingItem *elN = n->lastElementBeforeSegment();
            assert(elN != NULL);
            return elN;
        }

        default: break;
    }
    return ChordRest::prevElement();
}

//---------------------------------------------------------
//   lastElementBeforeSegment
//---------------------------------------------------------

EngravingItem *Chord::lastElementBeforeSegment() {
    if (tremoloSingleChord_) {
        return tremoloSingleChord_;
    } else if (tremoloTwoChord_) {
        return tremoloTwoChord_;
    } else if (arpeggio_) {
        return arpeggio_;
    } else {
        Note *n = notes_.front();
        EngravingItem *elN = n->lastElementBeforeSegment();
        assert(elN != NULL);
        return elN;
    }
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Chord::nextSegmentElement() {
    for (track_idx_t v = track() + 1; staffIdx() == v / VOICES; ++v) {
        EngravingItem *e = segment()->element(v);
        if (e) {
            if (e->isChord()) {
                return toChord(e)->notes().back();
            }

            return e;
        }
    }

    return ChordRest::nextSegmentElement();
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Chord::prevSegmentElement() {
    EngravingItem *el = score()->selection().element();
    if (!el && !score()->selection().elements().empty()) {
        el = score()->selection().elements().front();
    }
    EngravingItem *e = segment()->lastInPrevSegments(el->staffIdx());
    if (e) {
        if (e->isChord()) {
            return toChord(e)->notes().front();
        }
        return e;
    }

    return ChordRest::prevSegmentElement();
}

//---------------------------------------------------------
//   accessibleExtraInfo
//---------------------------------------------------------

void Chord::undoChangeProperty(Pid id, const PropertyValue &newValue) { undoChangeProperty(id, newValue, propertyFlags(id)); }

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void Chord::undoChangeProperty(Pid id, const PropertyValue &newValue, PropertyFlags ps) {
    if (id == Pid::VISIBLE) {
        processSiblings([=](EngravingItem *element) { element->undoChangeProperty(id, newValue, ps); }, false);
    }

    ChordRest::undoChangeProperty(id, newValue, ps);
}

std::set<SymId> Chord::articulationSymbolIds() const {
    std::set<SymId> result;
    for (const Articulation *artic : articulations_) {
        result.insert(artic->symId());
    }

    return result;
}

//---------------------------------------------------------
//   getNoteEventLists
//    Get contents of all NoteEventLists for all notes in
//    the chord.
//---------------------------------------------------------

std::vector<NoteEventList> Chord::getNoteEventLists() {
    std::vector<NoteEventList> ell;
    if (notes().empty()) {
        return ell;
    }
    ell.reserve(notes().size());
    for (size_t i = 0; i < notes().size(); ++i) {
        ell.push_back(NoteEventList(notes()[i]->playEvents()));
    }
    return ell;
}

//---------------------------------------------------------
//   setNoteEventLists
//    Set contents of all NoteEventLists for all notes in
//    the chord.
//---------------------------------------------------------

void Chord::setNoteEventLists(std::vector<NoteEventList> &ell) {
    if (notes().empty()) {
        return;
    }
    IF_ASSERT_FAILED(ell.size() == notes().size()) { return; }
    for (size_t i = 0; i < ell.size(); i++) {
        notes()[i]->setPlayEvents(ell[i]);
    }
}

//---------------------------------------------------------
//   styleChanged
//---------------------------------------------------------
void Chord::styleChanged() {
    auto updateElementsStyle = [](EngravingItem *e) { e->styleChanged(); };
    scanElements(updateElementsStyle);
}

void Chord::computeKerningExceptions() {
    allowKerningAbove_ = true;
    allowKerningBelow_ = true;
    for (Articulation *art : articulations_) {
        if (art->up()) {
            allowKerningAbove_ = false;
        } else {
            allowKerningBelow_ = false;
        }
    }
    if ((startEndSlurs_.startUp || startEndSlurs_.endUp) && !ldata()->up) {
        allowKerningAbove_ = false;
    }
    if ((startEndSlurs_.startDown || startEndSlurs_.endDown) && ldata()->up) {
        allowKerningBelow_ = false;
    }
}

Ornament *Chord::findOrnament(bool forPlayback) const {
    for (Articulation *art : articulations_) {
        if (art->isOrnament()) {
            return toOrnament(art);
        }
    }
    for (Spanner *spanner : startingSpanners_) {
        if (spanner->isTrill()) {
            return toTrill(spanner)->ornament();
        }
    }
    if (forPlayback) {
        // TODO: cleanup.
        // We shouldn't do this kind of special cases, and the DOM shouldn't know anything about playback.
        // This should be in a different function that returns the ornament from the Trill ending on this chord. [MS]
        for (Spanner *spanner : endingSpanners_) {
            if (spanner->isTrill()) {
                return toTrill(spanner)->ornament();
            }
        }
    }
    return nullptr;
}

//---------------------------------
// firstGraceOrNote
//---------------------------------
Note *Chord::firstGraceOrNote() {
    GraceNotesGroup &graceNotesBefore = this->graceNotesBefore();
    if (!graceNotesBefore.empty()) {
        if (Chord *graceNotesBeforeFirstChord = graceNotesBefore.front()) {
            return graceNotesBeforeFirstChord->notes().front();
        }
    }

    return this->notes().back();
}

//---------------------------------
// GRACE NOTES
//---------------------------------

GraceNotesGroup::GraceNotesGroup(DummyParentOr<Chord> c) : EngravingItem(ElementType::GRACE_NOTES_GROUP, c), _parent(c.as<Chord>()) {}

void GraceNotesGroup::setPos(double x, double y) {
    EngravingItem::setPos(x, y);
    for (unsigned i = 0; i < this->size(); ++i) {
        Chord *chord = this->at(i);
        chord->mutldata()->move(PointF(x, y));
    }
}

void GraceNotesGroup::addToShape() {
    for (Chord *grace : *this) {
        const PointF yOffset = grace->staffOffset();
        staff_idx_t staffIdx = grace->staffIdx();
        staff_idx_t vStaffIdx = grace->vStaffIdx();
        Shape &s = _appendedSegment->staffShape(staffIdx);
        s.add(grace->shape(LD_ACCESS::PASS).translate(grace->pos() + yOffset));
        if (vStaffIdx != staffIdx) {
            // Cross-staff grace notes add their shape to both the origin and the destination staff
            Shape &s2 = _appendedSegment->staffShape(vStaffIdx);
            s2.add(grace->shape().translate(grace->pos() + yOffset));
        }
    }
}
} // namespace mu::engraving
