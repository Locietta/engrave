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
NoteParenthesisInfo::NoteParenthesisInfo(Parenthesis *l_paren, Parenthesis *r_paren, std::vector<Note *> n_list)
    : left_paren_(l_paren), right_paren_(r_paren), notes_(n_list) {
    std::sort(notes_.begin(), notes_.end(), note_is_before);
}

NoteParenthesisInfo::~NoteParenthesisInfo() {
    delete left_paren_;
    delete right_paren_;
}

void NoteParenthesisInfo::insert_note(Note *note) {
    notes_.insert(std::upper_bound(notes_.begin(), notes_.end(), note, note_is_before), note);
}

void NoteParenthesisInfo::remove_note(Note *note) { muse::remove(notes_, note); }

//---------------------------------------------------------
//   upNote
//---------------------------------------------------------

Note *Chord::up_note() const {
    assert(!notes_.empty());

    Note *result = notes_.back();
    const Staff *stf = staff();
    if (!stf) {
        return result;
    }

    const StaffType *st = stf->staff_type_for_element(this);
    if (st->is_drum_staff()) {
        for (Note *n : notes_) {
            if (n->line() < result->line()) {
                result = n;
            }
        }
    } else if (st->is_tab_staff()) {
        int line = st->lines(); // start at bottom line
        int note_line;
        // scan each note: if TAB strings are not in sequential order,
        // visual order of notes might not correspond to pitch order
        for (Note *n : notes_) {
            if (n->should_hide_fret()) {
                continue;
            }
            note_line = st->phys_string_to_visual(n->string());
            if (note_line < line) {
                line = note_line;
                result = n;
            }
        }
    }
    return result;
}

//---------------------------------------------------------
//   downNote
//---------------------------------------------------------

Note *Chord::down_note() const {
    assert(!notes_.empty());

    Note *result = notes_.front();
    const Staff *stf = staff();
    if (!stf) {
        return result;
    }

    const StaffType *st = stf->staff_type_for_element(this);
    if (st->is_drum_staff()) {
        for (Note *n : notes_) {
            if (n->line() > result->line()) {
                result = n;
            }
        }
    } else if (st->is_tab_staff()) {
        int line = 0; // start at top line
        int note_line;
        // scan each note: if TAB strings are not in sequential order,
        // visual order of notes might not correspond to pitch order
        for (Note *n : notes_) {
            if (n->should_hide_fret()) {
                continue;
            }
            note_line = st->phys_string_to_visual(n->string());
            if (note_line > line) {
                line = note_line;
                result = n;
            }
        }
    }
    return result;
}

//---------------------------------------------------------
//   upLine / downLine
//---------------------------------------------------------

int Chord::up_line() const { return on_tab_staff() ? up_string() * 2 : up_note()->line(); }

int Chord::down_line() const { return on_tab_staff() ? down_string() * 2 : down_note()->line(); }

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

int Chord::up_string() const {
    // if no staff or staff not a TAB, return 0 (=topmost line)
    const Staff *st = staff();
    if (!st) {
        return 0;
    }

    const StaffType *tab = st->staff_type_for_element(this);
    if (!tab->is_tab_staff()) {
        return 0;
    }

    int line = tab->lines() - 1; // start at bottom line
    int note_line;
    // scan each note: if TAB strings are not in sequential order,
    // visual order of notes might not correspond to pitch order
    size_t n = notes_.size();
    for (size_t i = 0; i < n; ++i) {
        note_line = tab->phys_string_to_visual(notes_.at(i)->string());
        if (note_line < line) {
            line = note_line;
        }
    }
    return line;
}

int Chord::down_string() const {
    const Staff *st = staff();
    if (!st) { // if no staff, return 0
        return 0;
    }

    const StaffType *tab = st->staff_type_for_element(this);
    if (!tab->is_tab_staff()) { // if staff not a TAB, return bottom line
        return st->lines(tick()) - 1;
    }

    int line = 0; // start at top line
    int note_line;
    size_t n = notes_.size();
    for (size_t i = 0; i < n; ++i) {
        note_line = tab->phys_string_to_visual(notes_.at(i)->string());
        if (note_line > line) {
            line = note_line;
        }
    }
    return line;
}

//---------------------------------------------------------
//   noteDistances
//---------------------------------------------------------

std::vector<int> Chord::note_distances() const {
    const StaffType *staff_type = this->staff_type();
    assert(staff_type);
    bool is_tab_staff = staff_type->is_tab_staff();
    int staff_middle_line = staff_type->middle_line();

    std::vector<int> distances;
    distances.reserve(notes_.size());
    for (Note *note : notes_) {
        int note_line = is_tab_staff ? note->string() : note->line();
        distances.push_back(note_line - staff_middle_line);
    }
    return distances;
}

//---------------------------------------------------------
//   Chord
//---------------------------------------------------------

Chord::Chord(DummyParentOr<Segment> parent) : ChordRest(ElementType::CHORD, parent) {
    stem_ = 0;
    hook_ = 0;
    stem_direction_ = DirectionV::AUTO;
    arpeggio_ = 0;
    span_arpeggio_ = 0;
    ends_note_anchored_line_ = false;
    note_type_ = NoteType::NORMAL;
    stem_slash_ = 0;
    no_stem_ = false;
    show_stem_slash_ = note_type_ == NoteType::ACCIACCATURA;
    play_event_type_ = PlayEventType::AUTO;
    space_lw_ = 0.;
    space_rw_ = 0.;
    cross_measure_ = CrossMeasure::UNKNOWN;
    grace_index_ = 0;
    combine_voice_ = AutoOnOff::AUTO;
}

Chord::Chord(const Chord &c, bool link) : ChordRest(c, link) {
    if (link) {
        score()->undo(new Link(this, const_cast<Chord *>(&c)));
    }

    for (Note *onote : c.notes_) {
        Note *nnote = Factory::copy_note(*onote, link);
        add(nnote);
    }
    for (Chord *gn : c.grace_notes()) {
        Chord *nc = new Chord(*gn, link);
        add(nc);
    }
    for (Articulation *a : c.articulations_) { // make deep copy
        Articulation *na = a->clone();
        if (link) {
            score()->undo(new Link(na, a));
        }
        na->set_ownership_parent(this);
        na->set_track(track());
        articulations_.push_back(na);
    }
    stem_ = 0;
    hook_ = 0;
    ends_note_anchored_line_ = false;
    arpeggio_ = 0;
    stem_slash_ = 0;

    span_arpeggio_ = c.span_arpeggio_;
    grace_index_ = c.grace_index_;
    no_stem_ = c.no_stem_;
    show_stem_slash_ = c.show_stem_slash_;
    play_event_type_ = c.play_event_type_;
    stem_direction_ = c.stem_direction_;
    note_type_ = c.note_type_;
    cross_measure_ = CrossMeasure::UNKNOWN;
    combine_voice_ = c.combine_voice_;

    if (c.stem_) {
        add(Factory::copy_stem(*(c.stem_)));
    }
    if (c.hook_) {
        add(new Hook(*(c.hook_)));
    }
    if (c.stem_slash_) {
        add(Factory::copy_stem_slash(*(c.stem_slash_)));
    }
    if (c.arpeggio_) {
        Arpeggio *a = c.arpeggio_->is_chord_bracket() ? new ChordBracket(*to_chord_bracket(c.arpeggio_)) : new Arpeggio(*(c.arpeggio_));
        add(a);
        if (link) {
            score()->undo(new Link(a, const_cast<Arpeggio *>(c.arpeggio_)));
        }
    }

    if (c.tremolo_single_chord_) {
        TremoloSingleChord *t = Factory::copy_tremolo_single_chord(*(c.tremolo_single_chord_));
        if (link) {
            score()->undo(new Link(t, const_cast<TremoloSingleChord *>(c.tremolo_single_chord_)));
        }
        add(t);
    } else if (c.tremolo_two_chord_) {
        if (c.tremolo_two_chord_->chord1() == &c) {
            TremoloTwoChord *t = Factory::copy_tremolo_two_chord(*(c.tremolo_two_chord_));
            if (link) {
                score()->undo(new Link(t, const_cast<TremoloTwoChord *>(c.tremolo_two_chord_)));
            }
            t->set_chords(this, nullptr);
            add(t);
        }
    }

    for (EngravingItem *e : c.el()) {
        if (e->is_chord_line()) {
            ChordLine *cl = to_chord_line(e);
            ChordLine *ncl = Factory::copy_chord_line(*cl);
            add(ncl);
            if (cl->note()) {
                ncl->set_note(find_note(cl->note()->pitch()));
            }
            if (link) {
                score()->undo(new Link(ncl, cl));
            }
        }
    }

    for (EngravingItem *e : c.el()) {
        if (e->is_chord_bracket()) {
            EngravingItem *cloned_chord_bracket = e->linked_clone();
            add(cloned_chord_bracket);
        }
    }

    if (!c.note_parentheses().empty()) {
        for (const NoteParenthesisInfo *info : c.note_parentheses()) {
            Parenthesis *new_left_paren = to_parenthesis(info->left_paren()->clone());
            new_left_paren->set_ownership_parent(this);
            Parenthesis *new_right_paren = to_parenthesis(info->right_paren()->clone());
            new_right_paren->set_ownership_parent(this);

            if (link && !info->left_paren()->generated()) {
                score()->undo(new Link(new_left_paren, info->left_paren()));
            }
            if (link && !info->right_paren()->generated()) {
                score()->undo(new Link(new_right_paren, info->right_paren()));
            }

            std::vector<Note *> new_notes;
            new_notes.reserve(info->notes().size());
            for (Note *note : info->notes()) {
                new_notes.push_back(find_note(note->pitch()));
            }

            note_parens_.push_back(new NoteParenthesisInfo(new_left_paren, new_right_paren, new_notes));
        }
    }
}

//---------------------------------------------------------
//   undoUnlink
//---------------------------------------------------------

void Chord::undo_unlink() {
    ChordRest::undo_unlink();
    for (Note *n : notes_) {
        n->undo_unlink();
    }
    for (Chord *gn : grace_notes()) {
        gn->undo_unlink();
    }
    for (Articulation *a : articulations_) {
        a->undo_unlink();
    }
    /*      if (_glissando)
            _glissando->undoUnlink(); */
    if (arpeggio_) {
        arpeggio_->undo_unlink();
    }

    if (tremolo_single_chord()) {
        tremolo_single_chord()->undo_unlink();
    }

    for (EngravingItem *e : el()) {
        if (e->is_chord_line()) {
            e->undo_unlink();
        }
    }
}

//---------------------------------------------------------
//   ~Chord
//---------------------------------------------------------

Chord::~Chord() {
    muse::delete_all(articulations_);

    if (tremolo_two_chord()) {
        if (tremolo_two_chord()->chord1() == this) {
            tremolo_two_chord()->set_chord1(nullptr);
        } else if (tremolo_two_chord()->chord2() == this) {
            tremolo_two_chord()->set_chord2(nullptr);
        }
    }

    delete arpeggio_;
    delete stem_slash_;
    delete stem_;
    delete hook_;
    muse::delete_all(ledger_lines_);
    muse::delete_all(grace_notes_);
    muse::delete_all(notes_);
    muse::delete_all(note_parens_);
}

//---------------------------------------------------------
//   noteHeadWidth
//---------------------------------------------------------

double Chord::note_head_width() const { return score()->note_head_width() * mag(); }

//---------------------------------------------------------
//   rightEdge
//---------------------------------------------------------

double Chord::right_edge() const {
    double right = 0.0;
    for (Note *n : notes()) {
        right = std::max(right, x() + n->x() + n->bbox_right_pos());
    }

    return right;
}

TremoloType Chord::tremolo_type() const {
    if (tremolo_single_chord()) {
        return tremolo_single_chord()->tremolo_type();
    } else if (tremolo_two_chord()) {
        return tremolo_two_chord()->tremolo_type();
    } else {
        return TremoloType::INVALID_TREMOLO;
    }
}

TremoloTwoChord *Chord::tremolo_two_chord() const { return tremolo_two_chord_; }

TremoloSingleChord *Chord::tremolo_single_chord() const { return tremolo_single_chord_; }

void Chord::set_tremolo_two_chord(TremoloTwoChord *tr, bool apply_logic) {
    if (tremolo_two_chord_ && tr && tr == tremolo_two_chord_) {
        return;
    }

    if (tremolo_two_chord_) {
        TDuration d;
        const Fraction f = ticks();
        if (f.numerator() > 0) {
            d = TDuration(f);
        } else {
            d = tremolo_two_chord_->duration_type();
            const int dots = d.dots();
            d = d.shift(1);
            d.set_dots(dots);
        }

        set_duration_type(d);
        Chord *other = tremolo_two_chord_->chord1() == this ? tremolo_two_chord_->chord2() : tremolo_two_chord_->chord1();
        tremolo_two_chord_ = nullptr;
        if (other) {
            other->set_tremolo_two_chord(nullptr);
        }
    }
    tremolo_single_chord_ = nullptr;

    if (tr && apply_logic) {
        TDuration d = tr->duration_type();
        if (!d.is_valid()) {
            d = duration_type();
            const int dots = d.dots();
            d = d.shift(-1);
            d.set_dots(dots);
            tr->set_duration_type(d);
        }

        set_duration_type(d);
        Chord *other = tr->chord1() == this ? tr->chord2() : tr->chord1();
        tremolo_two_chord_ = tr;
        if (other) {
            other->set_tremolo_two_chord(tr);
        }
    }

    tremolo_two_chord_ = tr;
}

void Chord::set_tremolo_single_chord(TremoloSingleChord *tr) {
    if (tremolo_single_chord_ && tr && tr == tremolo_single_chord_) {
        return;
    }

    if (tremolo_two_chord_) {
        TDuration d;
        const Fraction f = ticks();
        if (f.numerator() > 0) {
            d = TDuration(f);
        } else {
            d = tremolo_two_chord_->duration_type();
            const int dots = d.dots();
            d = d.shift(1);
            d.set_dots(dots);
        }

        set_duration_type(d);
        Chord *other = tremolo_two_chord_->chord1() == this ? tremolo_two_chord_->chord2() : tremolo_two_chord_->chord1();
        tremolo_two_chord_ = nullptr;
        if (other) {
            other->set_tremolo_two_chord(nullptr);
        }
    }

    tremolo_single_chord_ = tr;
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Chord::add(EngravingItem *e) {
    if (e->ownership_parent() != this) {
        e->set_ownership_parent(this);
    }
    e->set_track(track());
    switch (e->type()) {
        case ElementType::NOTE: {
            Note *note = to_note(e);
            bool tab = staff() && staff()->is_tab_staff(tick());
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
            note->connect_tied_notes();
            if (voice() && measure() && note->visible()) {
                measure()->set_has_voices(staff_idx(), true);
            }
        } break;
        case ElementType::ARPEGGIO: arpeggio_ = to_arpeggio(e); break;
        case ElementType::CHORD_BRACKET: add_el(e); break;
        case ElementType::TREMOLO_TWOCHORD: set_tremolo_two_chord(item_cast<TremoloTwoChord *>(e)); break;
        case ElementType::TREMOLO_SINGLECHORD: set_tremolo_single_chord(item_cast<TremoloSingleChord *>(e)); break;
        case ElementType::GLISSANDO: ends_note_anchored_line_ = true; break;
        case ElementType::STEM:
            assert(!stem_);
            stem_ = to_stem(e);
            break;
        case ElementType::HOOK: hook_ = to_hook(e); break;
        case ElementType::CHORDLINE: add_el(e); break;
        case ElementType::STEM_SLASH:
            assert(!stem_slash_);
            stem_slash_ = to_stem_slash(e);
            break;
        case ElementType::CHORD: {
            Chord *gc = to_chord(e);
            assert(gc->note_type() != NoteType::NORMAL);
            size_t idx = gc->grace_index();
            gc->set_flag(ElementFlag::MOVABLE, true);
            grace_notes_.insert(grace_notes_.begin() + idx, gc);
        } break;
        case ElementType::LEDGER_LINE: ASSERT_X("Chord::add ledgerline"); break;
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING: {
            Articulation *a = to_articulation(e);
            if (a->layout_close_to_note()) {
                if (a->is_staccato()) {
                    // staccato is always the first articulation
                    articulations_.insert(articulations_.begin(), a);
                } else {
                    auto i = articulations_.begin();
                    while (i != articulations_.end() && (*i)->layout_close_to_note()) {
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
            Note *note = to_note(e);
            auto i = std::find(notes_.begin(), notes_.end(), note);
            if (i != notes_.end()) {
                notes_.erase(i);
                note->disconnect_tied_notes();
                for (Spanner *s : note->spanner_back()) {
                    note->remove_spanner_back(s);
                }
                for (Spanner *s : note->spanner_for()) {
                    note->remove_spanner_for(s);
                }
                EditChord::remove_chord_parentheses(this, {note});
            } else {
                LOGD("Chord::remove() note %p not found!", e);
            }
            if (voice() && measure() && note->visible()) {
                measure()->check_multi_voices(staff_idx());
            }
        } break;

        case ElementType::ARPEGGIO:
            if (span_arpeggio_ == arpeggio_) {
                span_arpeggio_ = nullptr;
            }
            arpeggio_ = nullptr;
            break;
        case ElementType::CHORD_BRACKET: remove_el(e); break;
        case ElementType::TREMOLO_TWOCHORD: set_tremolo_two_chord(nullptr); break;
        case ElementType::TREMOLO_SINGLECHORD: set_tremolo_single_chord(nullptr); break;
        case ElementType::GLISSANDO: ends_note_anchored_line_ = false; break;
        case ElementType::STEM: stem_ = 0; break;
        case ElementType::HOOK: hook_ = 0; break;
        case ElementType::STEM_SLASH:
            assert(stem_slash_);
            if (stem_slash_->selected() && score()) {
                score()->deselect(stem_slash_);
            }
            stem_slash_ = 0;
            break;
        case ElementType::CHORDLINE: remove_el(e); break;
        case ElementType::CHORD: {
            auto i = std::find(grace_notes_.begin(), grace_notes_.end(), to_chord(e));
            IF_ASSERT_FAILED(i != grace_notes_.end()) { break; }
            Chord *grace = *i;
            grace->set_grace_index(i - grace_notes_.begin());
            grace_notes_.erase(i);
        } break;
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING: {
            Articulation *a = to_articulation(e);
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

double Chord::max_head_width() const {
    // determine max head width in chord
    double hw = 0;
    for (const Note *n : notes_) {
        double t = n->head_width();
        if (t > hw) {
            hw = t;
        }
    }
    return hw;
}

//---------------------------------------------------------
//   selectedNote
//---------------------------------------------------------

Note *Chord::selected_note() const {
    Note *note = 0;
    size_t n = notes_.size();
    for (size_t i = 0; i < n; ++i) {
        Note *current_note = notes_.at(i);
        if (current_note->selected()) {
            if (note) {
                return 0;
            }
            note = current_note;
        }
    }
    return note;
}

//---------------------------------------------------------
//   upPos
//---------------------------------------------------------

double Chord::up_pos() const { return up_note()->pos().y(); }

//---------------------------------------------------------
//   downPos
//---------------------------------------------------------

double Chord::down_pos() const { return down_note()->pos().y(); }

bool Chord::all_notes_tied_to_next() const {
    Chord *tied_chord = nullptr;
    for (Note *note : notes_) {
        if (!note->tie_for()) {
            return false;
        }

        Note *end_note = note->tie_for()->end_note();
        Chord *end_chord = end_note ? end_note->chord() : nullptr;
        if (!end_chord) {
            return false;
        }

        if (!tied_chord) {
            tied_chord = end_chord;
            continue;
        }

        if (end_chord != tied_chord) {
            return false;
        }
    }

    return true;
}

bool Chord::all_elements_invisible() const {
    for (EngravingObject *child : get_children()) {
        if (to_engraving_item(child)->visible()) {
            return false;
        }
    }

    for (Chord *grace : grace_notes_) {
        if (!grace->all_elements_invisible()) {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------
//   processSiblings
//---------------------------------------------------------

void Chord::process_siblings(std::function<void(EngravingItem *)> func, bool include_temporary_siblings) const {
    if (hook_) {
        func(hook_);
    }
    if (stem_) {
        func(stem_);
    }
    if (stem_slash_) {
        func(stem_slash_);
    }
    if (arpeggio_) {
        func(arpeggio_);
    }
    if (tremolo_two_chord_) {
        func(tremolo_two_chord_);
    }
    if (tremolo_single_chord_) {
        func(tremolo_single_chord_);
    }
    if (include_temporary_siblings) {
        for (LedgerLine *ll : ledger_lines_) {
            func(ll);
        }
    }
    for (Articulation *a : articulations_) {
        func(a);
    }
    for (Note *note : notes_) {
        func(note);
    }
    for (Chord *chord : grace_notes_) { // process grace notes last, needed for correct shape calculation
        func(chord);
    }
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void Chord::set_track(track_idx_t val) {
    ChordRest::set_track(val);
    process_siblings([val](EngravingItem *e) { e->set_track(val); }, true);
}

bool Chord::should_combine_voice() const {
    return combine_voice() == AutoOnOff::ON || (combine_voice() == AutoOnOff::AUTO && style().style_b(Sid::COMBINE_VOICE));
}

bool Chord::combine_voice(const Chord *chord1, const Chord *chord2) {
    return chord1->should_combine_voice() && chord2->should_combine_voice();
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void Chord::set_score(Score *s) {
    ChordRest::set_score(s);
    process_siblings([s](EngravingItem *e) { e->set_score(s); }, true);
}

Fraction Chord::end_tick_including_tied() const {
    const Chord *last_tied = this;
    while (last_tied) {
        const Chord *next = last_tied->next_tied_chord();
        if (next) {
            last_tied = next;
        } else {
            break;
        }
    }
    return last_tied->end_tick();
}

Chord *Chord::prev() const {
    ChordRest *prev = Navigation::prev_chord_rest(const_cast<Chord *>(this));
    if (prev && prev->is_chord()) {
        return to_chord(prev);
    }
    return nullptr;
}

Chord *Chord::next() const {
    ChordRest *next = Navigation::next_chord_rest(const_cast<Chord *>(this));
    if (next && next->is_chord()) {
        return to_chord(next);
    }
    return nullptr;
}

void Chord::resize_ledger_lines_to(size_t new_size) {
    int ledger_line_count_diff = static_cast<int>(new_size - ledger_lines_.size());
    if (ledger_line_count_diff > 0) {
        for (int i = 0; i < ledger_line_count_diff; ++i) {
            ledger_lines_.push_back(new LedgerLine(this));
        }
    } else {
        for (int i = 0; i < std::abs(ledger_line_count_diff); ++i) {
            delete ledger_lines_.back();
            ledger_lines_.pop_back();
        }
    }

    assert(ledger_lines_.size() == new_size);
}

void Chord::set_beam_extension(double extension) {
    if (stem_) {
        double base_length = stem_->absolute_from_spatium(stem_->base_length());
        stem_->set_base_length(std::max(Spatium::from_absolute(base_length + extension, spatium()), 0.0_sp));
        default_stem_length_ = std::max(default_stem_length_ + extension, stem_->absolute_from_spatium(stem_->base_length()));
    }
}

bool Chord::should_have_stem() const {
    const Staff *staff = this->staff();
    const StaffType *staff_type = staff ? staff->staff_type_for_element(this) : nullptr;

    return !no_stem_ && duration_type().has_stem() &&
           !(duration_type().type() == DurationType::V_HALF && staff_type && staff_type->is_tab_staff() &&
             staff_type->minim_style() == TablatureMinimStyle::NONE) &&
           !(measure() && measure()->stemless(staff_idx())) && !(staff_type && staff_type->is_tab_staff() && staff_type->stemless());
}

bool Chord::should_have_hook() const { return should_have_stem() && duration_type().hooks() > 0 && !beam() && !tremolo_two_chord(); }

void Chord::create_stem() {
    Stem *stem = Factory::create_stem(this);
    stem->set_generated(true);
    //! score()->undoAddElement calls add(), which assigns this created stem to _stem
    score()->do_undo_add_element(stem);
}

void Chord::remove_stem() {
    if (stem_) {
        score()->undo_remove_element(stem_);
    }
    if (hook_) {
        score()->undo_remove_element(hook_);
    }
    if (stem_slash_) {
        score()->undo_remove_element(stem_slash_);
    }
}

void Chord::create_hook() {
    Hook *hook = new Hook(this);
    hook->set_generated(true);
    score()->do_undo_add_element(hook);
}

//---------------------------------------------------------
//    underBeam: true, if grace note is placed under a beam.
//---------------------------------------------------------

bool Chord::under_beam() const {
    if (note_type_ == NoteType::NORMAL) {
        return false;
    }
    const Chord *cr = to_chord(ownership_parent());
    Beam *beam = cr->beam();
    if (!beam || !cr->beam()->up()) {
        return false;
    }
    size_t s = beam->elements().size();
    if (is_grace_before()) {
        if (beam->elements()[0] != cr) {
            return true;
        }
    }
    if (is_grace_after()) {
        if (beam->elements()[s - 1] != cr) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   cmdUpdateNotes
//---------------------------------------------------------

void Chord::cmd_update_notes(AccidentalState *as, staff_idx_t staff_idx) {
    // TAB_STAFF is different, as each note has to be fretted
    // in the context of the all of the chords of the whole segment

    const Staff *st = staff();
    StaffGroup staff_group = st->staff_type_for_element(this)->group();
    if (staff_group == StaffGroup::TAB) {
        Fraction tick = this->tick();
        const StringData *string_data = origin_part()->string_data(tick, st->idx());
        for (Chord *ch : grace_notes()) {
            string_data->fret_chords(ch);
        }
        string_data->fret_chords(this);
        return;
    } else {
        // if not tablature, use instrument->useDrumset to set staffGroup (to allow pitched to unpitched in same staff)
        staff_group = origin_part()->instrument(this->tick())->use_drumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
    }

    // PITCHED_ and PERCUSSION_STAFF can go note by note

    if (staff_group == StaffGroup::STANDARD) {
        const std::vector<Chord *> gnb(grace_notes_before());
        for (Chord *ch : gnb) {
            if (ch->v_staff_idx() != staff_idx) {
                continue;
            }
            std::vector<Note *> notes(ch->notes()); // we need a copy!
            for (Note *note : notes) {
                note->update_accidental(as);
            }
            ch->sort_notes();
        }
        if (v_staff_idx() == staff_idx) {
            std::vector<Note *> lnotes(notes()); // we need a copy!
            for (Note *note : lnotes) {
                if (note->tie_back_non_partial() && note->tpc() == note->tie_back()->start_note()->tpc()) {
                    // same pitch
                    if (note->accidental() && note->accidental()->role() == AccidentalRole::AUTO) {
                        // not courtesy
                        // TODO: remove accidental only if note is not
                        // on new system
                        score()->undo_remove_element(note->accidental());
                    }
                }
                note->update_accidental(as);
            }
            for (Articulation *art : articulations_) {
                if (!art->is_ornament()) {
                    continue;
                }
                Ornament *ornament = to_ornament(art);
                ornament->compute_notes_above_and_below(as);
            }
            for (Spanner *spanner : starting_spanners()) {
                if (spanner->is_trill()) {
                    Ornament *ornament = to_trill(spanner)->ornament();
                    if (ornament) {
                        ornament->set_ownership_parent(this);
                        ornament->compute_notes_above_and_below(as);
                    }
                }
            }
            sort_notes();
        }
        const std::vector<Chord *> gna(grace_notes_after());
        for (Chord *ch : gna) {
            if (ch->v_staff_idx() != staff_idx) {
                continue;
            }
            std::vector<Note *> notes(ch->notes()); // we need a copy!
            for (Note *note : notes) {
                note->update_accidental(as);
            }
            ch->sort_notes();
        }
    } else if (staff_group == StaffGroup::PERCUSSION) {
        const Instrument *instrument = origin_part()->instrument(this->tick());
        const Drumset *drumset = instrument->drumset();
        if (!drumset) {
            LOGW("no drumset");
        }
        update_percussion_notes(this, drumset);
        sort_notes();
    }
}

//---------------------------------------------------------
//   pagePos
//---------------------------------------------------------

PointF Chord::page_pos() const {
    if (!is_grace()) {
        return EngravingItem::page_pos();
    }

    PointF p(pos());
    if (!ownership_parent()) {
        return p;
    }
    p.rx() = page_x();

    const Chord *pc = to_chord(ownership_parent());
    System *system = pc->segment()->system();
    if (!system) {
        return p;
    }
    p.ry() += system->staff_ypage(v_staff_idx()) + staff_offset_y();
    return p;
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Chord::scan_elements(std::function<void(EngravingItem *)> func) {
    for (Articulation *a : articulations_) {
        a->scan_elements(func);
    }
    if (hook_) {
        func(hook_);
    }
    if (stem_) {
        func(stem_);
    }
    if (stem_slash_) {
        func(stem_slash_);
    }
    if (arpeggio_) {
        func(arpeggio_);
    }
    if (tremolo_two_chord_ && (tremolo_chord_type() != TremoloChordType::TREMOLO_SECOND_CHORD)) {
        func(tremolo_two_chord_);
    }
    if (tremolo_single_chord_) {
        func(tremolo_single_chord_);
    }
    const Staff *st = staff();
    if ((st && st->show_ledger_lines(tick())) || !st) { // also for palette
        for (LedgerLine *ll : ledger_lines_) {
            func(ll);
        }
    }
    for (Note *note : notes_) {
        note->scan_elements(func);
    }
    for (Chord *chord : grace_notes_) {
        chord->scan_elements(func);
    }
    for (EngravingItem *e : el()) {
        e->scan_elements(func);
    }

    for (auto &p : note_parens_) {
        p->left_paren()->scan_elements(func);
        p->right_paren()->scan_elements(func);
    }
    ChordRest::scan_elements(func);
}

NoteParenthesisInfo *Chord::find_note_parenthesis_info(const Parenthesis *paren) {
    for (NoteParenthesisInfo *info_ptr : note_parens_) {
        if (paren == info_ptr->left_paren() || paren == info_ptr->right_paren()) {
            return info_ptr;
        }
    }

    return nullptr;
}

const NoteParenthesisInfo *Chord::find_note_parenthesis_info(const Note *note) const {
    for (const NoteParenthesisInfo *info_ptr : note_parens_) {
        for (const Note *paren_note : info_ptr->notes()) {
            if (paren_note == note) {
                return info_ptr;
            }
        }
    }

    DO_ASSERT(u"Parentheses are not in chord");
    return nullptr;
}

void Chord::add_note_parenthesis_info(NoteParenthesisInfo *note_paren_info) { note_parens_.push_back(note_paren_info); }

void Chord::remove_note_parenthesis_info(const NoteParenthesisInfo *note_paren_info) {
    if (note_parens_.empty()) {
        return;
    }

    if (!note_paren_info) {
        return;
    }

    auto it = std::find_if(note_parens_.begin(), note_parens_.end(),
                           [note_paren_info](const NoteParenthesisInfo *ptr) { return ptr == note_paren_info; });

    if (it != note_parens_.end()) {
        note_parens_.erase(it);
    }
}

void Chord::add_note_to_parenthesis_info(Note *note, const Parenthesis *paren) {
    NoteParenthesisInfo *note_paren_info = find_note_parenthesis_info(paren);

    if (!note_paren_info) {
        return;
    }

    note_paren_info->insert_note(note);
}

void Chord::remove_note_from_parenthesis_info(Note *note, const Parenthesis *paren) {
    NoteParenthesisInfo *note_paren_info = find_note_parenthesis_info(paren);

    if (!note_paren_info) {
        return;
    }

    note_paren_info->remove_note(note);
}

//---------------------------------------------------------
//   isChordPlayable
//   @note Now every related to chord element has it's own "PLAY" property,
//         However, there is no way to control these properties outside the scope of the chord since the new Properties panel.
//         So we'll use a chord as a proxy entity for "PLAY" property handling
//---------------------------------------------------------

bool Chord::is_chord_playable() const {
    if (!notes_.empty()) {
        if (notes_.front()->is_pre_bend_or_dive_start()) {
            return false;
        }

        return notes_.front()->get_property(Pid::PLAY).to_bool();
    } else if (tremolo_two_chord()) {
        return tremolo_two_chord()->get_property(Pid::PLAY).to_bool();
    } else if (tremolo_single_chord()) {
        return tremolo_single_chord()->get_property(Pid::PLAY).to_bool();
    } else if (arpeggio_) {
        return arpeggio_->get_property(Pid::PLAY).to_bool();
    }

    return false;
}

//---------------------------------------------------------
//   setIsChordPlayable
//---------------------------------------------------------

void Chord::set_is_chord_playable(const bool is_playable) {
    for (Note *note : notes_) {
        note->undo_change_property(Pid::PLAY, is_playable);
    }

    if (arpeggio_) {
        arpeggio_->undo_change_property(Pid::PLAY, is_playable);
    }

    if (tremolo_two_chord()) {
        tremolo_two_chord()->undo_change_property(Pid::PLAY, is_playable);
    }

    if (tremolo_single_chord()) {
        tremolo_single_chord()->undo_change_property(Pid::PLAY, is_playable);
    }

    trigger_layout();
}

//---------------------------------------------------------
//   findNote
//---------------------------------------------------------

Note *Chord::find_note(int pitch, int skip) const {
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

void Chord::undo_change_span_arpeggio(Arpeggio *a) {
    if (span_arpeggio_ == a) {
        return;
    }

    // TODO: change arpeggio for links
    score()->undo(new ChangeSpanArpeggio(this, a));
}

ChordLine *Chord::chord_line() const {
    for (EngravingItem *item : el()) {
        if (item && item->is_chord_line()) {
            return to_chord_line(item);
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Chord::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.drop_element;
    switch (e->type()) {
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING: {
            Articulation *atr = to_articulation(e);
            Articulation *oa = has_articulation(atr);
            if (oa) {
                delete atr;
                atr = 0;
                // if attribute is already there, remove
                // score()->cmdRemove(oa); // unexpected behaviour?
                score()->select(oa, SelectType::SINGLE, 0);
            } else {
                // intuit anchor for this accidental based on other accidentals in the chord
                int above_below = 0;
                bool mixed = false;
                for (Articulation *a : articulations_) {
                    PropertyFlags pf = a->property_flags(Pid::ARTICULATION_ANCHOR);
                    if (pf == PropertyFlags::STYLED) {
                        continue;
                    }
                    if (a->anchor() == ArticulationAnchor::TOP) {
                        if (above_below < 0) {
                            mixed = true;
                            break;
                        }
                        ++above_below;
                    } else if (a->anchor() == ArticulationAnchor::BOTTOM) {
                        if (above_below > 0) {
                            mixed = true;
                            break;
                        }
                        --above_below;
                    }
                }
                if (!mixed && above_below != 0) {
                    if (above_below > 0) {
                        atr->set_anchor(ArticulationAnchor::TOP);
                    } else {
                        atr->set_anchor(ArticulationAnchor::BOTTOM);
                    }
                    atr->set_property_flags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
                }
                atr->set_ownership_parent(this);
                atr->set_track(track());

                // Immediately add if the chord has no existing articulations, toggle otherwise...
                if (articulations_.empty()) {
                    score()->undo_add_element(atr);
                } else {
                    EditChord::toggle_articulation(score(), this, atr);
                }
            }
            return atr;
        }

        case ElementType::CHORDLINE:
            e->set_ownership_parent(this);
            e->set_track(track());
            score()->undo_add_element(e);
            break;

        case ElementType::TREMOLO_SINGLECHORD:
            if (tremolo_single_chord()) {
                bool same_type = (e->subtype() == tremolo_single_chord()->subtype());
                score()->undo_remove_element(tremolo_single_chord());
                if (same_type) {
                    delete e;
                    return nullptr;
                }
            }
            e->set_ownership_parent(this);
            e->set_track(track());
            score()->undo_add_element(e);
            break;

        case ElementType::TREMOLO_TWOCHORD: {
            TremoloTwoChord *t = item_cast<TremoloTwoChord *>(e);
            Segment *s = segment()->next();
            while (s) {
                if (s->element(track()) && s->element(track())->is_chord()) {
                    break;
                }
                s = s->next();
            }
            if (s == 0) {
                LOGD("no segment for second note of tremolo found");
                delete e;
                return 0;
            }
            Chord *ch2 = to_chord(s->element(track()));
            if (ch2->ticks() != ticks()) {
                LOGD("no matching chord for second note of tremolo found");
                delete e;
                return 0;
            }
            t->set_chords(this, ch2);
        }
            if (tremolo_two_chord()) {
                bool same_type = (e->subtype() == tremolo_two_chord()->subtype());
                score()->undo_remove_element(tremolo_two_chord());
                if (same_type) {
                    delete e;
                    return 0;
                }
            }
            e->set_ownership_parent(this);
            e->set_track(track());
            score()->undo_add_element(e);
            break;

        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET: {
            Arpeggio *a = to_arpeggio(e);
            if (arpeggio() && a->is_arpeggio()) {
                score()->undo_remove_element(arpeggio());
            }
            a->set_track(track());
            a->set_ownership_parent(this);
            score()->undo_add_element(a);
        }
            return e;

        default: return ChordRest::drop(tx, data);
    }
    return 0;
}

void Chord::set_color(const Color &color) {
    ChordRest::set_color(color);

    for (Note *note : notes_) {
        note->undo_change_property(Pid::COLOR, PropertyValue::from_value(color));
    }
}

//---------------------------------------------------------
//   setStemDirection
//---------------------------------------------------------

void Chord::set_stem_direction(DirectionV d) { stem_direction_ = d; }

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void Chord::local_spatium_changed(double old_value, double new_value) {
    ChordRest::local_spatium_changed(old_value, new_value);
    for (EngravingItem *e : grace_notes()) {
        e->local_spatium_changed(old_value, new_value);
    }
    if (hook_) {
        hook_->local_spatium_changed(old_value, new_value);
    }
    if (stem_) {
        stem_->local_spatium_changed(old_value, new_value);
    }
    if (stem_slash_) {
        stem_slash_->local_spatium_changed(old_value, new_value);
    }
    if (arpeggio_) {
        arpeggio_->local_spatium_changed(old_value, new_value);
    }

    if (tremolo_single_chord_) {
        tremolo_single_chord_->local_spatium_changed(old_value, new_value);
    } else if (tremolo_two_chord_ && (tremolo_chord_type() != TremoloChordType::TREMOLO_SECOND_CHORD)) {
        tremolo_two_chord_->local_spatium_changed(old_value, new_value);
    }

    for (EngravingItem *e : articulations()) {
        e->local_spatium_changed(old_value, new_value);
    }
    for (Note *note : notes()) {
        note->local_spatium_changed(old_value, new_value);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Chord::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::NO_STEM: return no_stem();
        case Pid::SHOW_STEM_SLASH: return show_stem_slash();
        case Pid::STEM_DIRECTION: return PropertyValue::from_value<DirectionV>(stem_direction());
        case Pid::PLAY: return is_chord_playable();
        case Pid::COMBINE_VOICE: return PropertyValue::from_value<AutoOnOff>(combine_voice());
        case Pid::VISIBLE: return true; // Chord cannot be set invisible, only its elements can
        default: return ChordRest::get_property(property_id);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Chord::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::NO_STEM: return false;
        case Pid::SHOW_STEM_SLASH: return note_type() == NoteType::ACCIACCATURA;
        case Pid::STEM_DIRECTION: return PropertyValue::from_value<DirectionV>(DirectionV::AUTO);
        case Pid::PLAY: return true;
        case Pid::COMBINE_VOICE: return AutoOnOff::AUTO;
        default: return ChordRest::property_default(property_id);
    }
}

bool Chord::is_user_modified() const {
    if (show_stem_slash() != property_default(Pid::SHOW_STEM_SLASH).to_bool()) {
        return true;
    }

    return EngravingItem::is_user_modified();
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Chord::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::NO_STEM: set_no_stem(v.to_bool()); break;
        case Pid::SHOW_STEM_SLASH: request_show_stem_slash(v.to_bool()); break;
        case Pid::STEM_DIRECTION: set_stem_direction(v.value<DirectionV>()); break;
        case Pid::PLAY: set_is_chord_playable(v.to_bool()); break;
        case Pid::COMBINE_VOICE: set_combine_voice(v.value<AutoOnOff>()); break;
        default: return ChordRest::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   hasArticulation
//---------------------------------------------------------

Articulation *Chord::has_articulation(const Articulation *aa) {
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
        if (a->is_tapping()) {
            tappings.push_back(to_tapping(a));
        }
    }
    DO_ASSERT(tappings.size() <= 1);

    return tappings.size() > 0 ? tappings.front() : nullptr;
}

void Chord::update_articulations(const std::set<SymId> &new_articulation_ids, ArticulationsUpdateMode update_mode) {
    Articulation *staccato = nullptr;
    Articulation *accent = nullptr;
    Articulation *marcato = nullptr;
    Articulation *tenuto = nullptr;
    ArticulationAnchor overall_anchor = ArticulationAnchor::AUTO;
    bool mixed_directions = false;
    if (!articulations_.empty()) {
        std::vector<Articulation *> artics_to_remove;
        // split all articulations
        for (Articulation *artic : articulations_) {
            if (!artic->is_double()) {
                continue;
            }
            auto split_syms = split_articulations({artic->sym_id()});
            for (const SymId &id : split_syms) {
                Articulation *new_articulation = Factory::create_articulation(this);
                new_articulation->set_sym_id(id);
                new_articulation->set_anchor(artic->anchor());
                new_articulation->set_property_flags(Pid::ARTICULATION_ANCHOR, artic->property_flags(Pid::ARTICULATION_ANCHOR));
                if (!has_articulation(new_articulation)) {
                    EditChord::toggle_articulation(score(), this, new_articulation);
                } else {
                    delete new_articulation;
                }
            }
            artics_to_remove.push_back(artic);
        }
        for (Articulation *artic : artics_to_remove) {
            score()->undo_remove_element(artic);
        }
        // now we have guaranteed no more combined artics
        // take an inventory of which articulations are already present
        for (Articulation *artic : articulations_) {
            PropertyFlags pf = artic->property_flags(Pid::ARTICULATION_ANCHOR);
            if (!mixed_directions && pf == PropertyFlags::UNSTYLED && artic->anchor() != ArticulationAnchor::AUTO) {
                if (overall_anchor != ArticulationAnchor::AUTO && artic->anchor() != overall_anchor) {
                    mixed_directions = true;
                    overall_anchor = ArticulationAnchor::AUTO;
                } else {
                    overall_anchor = artic->anchor();
                }
            }
            if (artic->is_staccato()) {
                staccato = artic;
            } else if (artic->is_accent()) {
                accent = artic;
            } else if (artic->is_marcato()) {
                marcato = artic;
            } else if (artic->is_tenuto()) {
                tenuto = artic;
            }
        }
    }
    std::set<SymId> new_artics;
    // get symbol id's for correct direction
    for (const SymId &id : new_articulation_ids) {
        if (id == SymId::ARTIC_ACCENT_ABOVE || id == SymId::ARTIC_ACCENT_BELOW) {
            if (marcato && update_mode == ArticulationsUpdateMode::INSERT) {
                // adding an accent, which replaces marcato
                score()->undo_remove_element(marcato);
            }
        } else if (id == SymId::ARTIC_MARCATO_ABOVE || id == SymId::ARTIC_MARCATO_BELOW) {
            if (accent && update_mode == ArticulationsUpdateMode::INSERT) {
                // adding a marcato, which replaces accent
                score()->undo_remove_element(accent);
            }
        }
        new_artics.insert(id);
    }

    // newArtics now contains the articulations in the correct direction
    if (update_mode == ArticulationsUpdateMode::REMOVE) {
        // remove articulations from _articulations that are found in newArtics
        for (const SymId &id : new_artics) {
            switch (id) {
                case SymId::ARTIC_ACCENT_ABOVE:
                case SymId::ARTIC_ACCENT_BELOW:
                    score()->undo_remove_element(accent);
                    accent = nullptr;
                    break;
                case SymId::ARTIC_MARCATO_ABOVE:
                case SymId::ARTIC_MARCATO_BELOW:
                    score()->undo_remove_element(marcato);
                    marcato = nullptr;
                    break;
                case SymId::ARTIC_TENUTO_ABOVE:
                case SymId::ARTIC_TENUTO_BELOW:
                    score()->undo_remove_element(tenuto);
                    tenuto = nullptr;
                    break;
                case SymId::ARTIC_STACCATO_ABOVE:
                case SymId::ARTIC_STACCATO_BELOW:
                    score()->undo_remove_element(staccato);
                    staccato = nullptr;
                    break;
                default: break;
            }
        }
    } else {
        // add articulations from newArtics that are not found in articulations_
        for (const SymId &id : new_artics) {
            Articulation *new_articulation = Factory::create_articulation(this);
            new_articulation->set_sym_id(id);
            if (overall_anchor != ArticulationAnchor::AUTO) {
                new_articulation->set_anchor(overall_anchor);
                new_articulation->set_property_flags(Pid::ARTICULATION_ANCHOR, PropertyFlags::UNSTYLED);
            }
            if (!has_articulation(new_articulation)) {
                EditChord::toggle_articulation(score(), this, new_articulation);
            } else {
                delete new_articulation;
            }
        }
    }
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Chord::reset() {
    undo_change_property(Pid::STEM_DIRECTION, DirectionV::AUTO);
    ChordRest::reset();
}

//---------------------------------------------------------
//   slash
//---------------------------------------------------------

bool Chord::slash() {
    Note *n = up_note();
    return n->fixed();
}

//---------------------------------------------------------
//   setSlash
//---------------------------------------------------------

void Chord::set_slash(bool flag, bool stemless) {
    int line = 0;
    NoteHeadGroup head = NoteHeadGroup::HEAD_SLASH;

    if (!flag) {
        // restore to normal
        undo_change_property(Pid::NO_STEM, false);
        undo_change_property(Pid::SMALL, false);
        undo_change_property(Pid::OFFSET, PointF());
        for (Note *n : notes_) {
            n->undo_change_property(Pid::HEAD_GROUP, NoteHeadGroup::HEAD_NORMAL);
            n->undo_change_property(Pid::FIXED, false);
            n->undo_change_property(Pid::FIXED_LINE, 0);
            n->undo_change_property(Pid::PLAY, true);
            n->undo_change_property(Pid::VISIBLE, true);
            for (NoteDot *dot : n->dots()) {
                dot->undo_change_property(Pid::VISIBLE, true);
            }
            if (staff()->is_drum_staff(tick())) {
                const Drumset *ds = origin_part()->instrument(tick())->drumset();
                int pitch = n->pitch();
                if (ds && ds->is_valid(pitch)) {
                    undo_change_property(Pid::STEM_DIRECTION, ds->stem_direction(pitch));
                    n->undo_change_property(Pid::HEAD_GROUP, ds->note_head(pitch));
                }
            }
        }
        return;
    }

    // make stemless if asked
    if (stemless) {
        undo_change_property(Pid::NO_STEM, true);
        undo_change_property(Pid::BEAM_MODE, BeamMode::NONE);
    }

    const StaffType *staff_type = this->staff_type();

    // voice-dependent attributes - line, size, offset, head
    if (track() % ::mu::engraving::voices < 2) {
        // use middle line
        line = staff_type->middle_line();
        undo_change_property(Pid::STEM_DIRECTION, PropertyValue::from_value<DirectionV>(DirectionV::DOWN));
    } else {
        // set small
        undo_change_property(Pid::SMALL, true);
        // set outside the staff
        double y = 0.0;
        if (track() % 2) {
            line = staff_type->bottom_line() + 1;
            y = 0.5 * spatium();
            undo_change_property(Pid::STEM_DIRECTION, PropertyValue::from_value<DirectionV>(DirectionV::DOWN));
        } else {
            line = -1;
            if (!staff_type->is_drum_staff()) {
                y = -0.5 * spatium();
            }
            undo_change_property(Pid::STEM_DIRECTION, PropertyValue::from_value<DirectionV>(DirectionV::UP));
        }
        // for non-drum staves, add an additional offset
        // for drum staves, no offset, but use normal head
        if (!staff_type->is_drum_staff()) {
            mutldata()->move_y(y);
        } else {
            head = NoteHeadGroup::HEAD_NORMAL;
        }
    }

    size_t ns = notes_.size();
    for (size_t i = 0; i < ns; ++i) {
        Note *n = notes_[i];
        n->undo_change_property(Pid::HEAD_GROUP, head);
        n->undo_change_property(Pid::FIXED, true);
        n->undo_change_property(Pid::FIXED_LINE, line);
        n->undo_change_property(Pid::PLAY, false);
        // hide all but first notehead
        if (i) {
            n->undo_change_property(Pid::VISIBLE, false);
            for (NoteDot *dot : n->dots()) {
                dot->undo_change_property(Pid::VISIBLE, false);
            }
        }
    }
}

//---------------------------------------------------------
//  updateEndsNoteAnchoredLine
//    sets/resets the chord endsNoteAnchoredLine_ according any note anchored line
//    end into this chord or no.
//---------------------------------------------------------

void Chord::update_ends_note_anchored_line() {
    ends_note_anchored_line_ = false; // assume no note anchored line ends here
    // scan all chord notes for note anchored lines ending on this chord
    for (Note *note : notes()) {
        for (Spanner *sp : note->spanner_back()) {
            if (sp->is_glissando() || (sp->is_note_line() && to_note_line(sp)->enforce_min_length())) {
                ends_note_anchored_line_ = true;
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

void Chord::remove_markings(bool keep_tremolo) {
    if (tremolo_single_chord_ && !keep_tremolo) {
        remove(tremolo_single_chord_);
    }

    if (tremolo_two_chord_ && !keep_tremolo) {
        remove(tremolo_two_chord_);
    }

    if (arpeggio()) {
        remove(arpeggio());
    }

    if (span_arpeggio_) {
        span_arpeggio_ = nullptr;
    }

    muse::delete_all(grace_notes());
    grace_notes().clear();
    muse::delete_all(articulations());
    articulations().clear();
    for (Note *n : notes()) {
        for (EngravingItem *e : n->el()) {
            n->remove(e);
        }
    }
    ChordRest::remove_markings(keep_tremolo);
}

//---------------------------------------------------------
//   intrinsicMag
//   returns the INTRINSIC mag of the chord (i.e. NOT scaled
//   by staff size)
//---------------------------------------------------------

double Chord::intrinsic_mag() const {
    double m = 1.0;
    if (is_small()) {
        m *= style().style_d(Sid::SMALL_NOTE_MAG);
    }
    if (note_type_ != NoteType::NORMAL) {
        bool tab_grace_size_exception =
            staff_type()->is_tab_staff() && is_pre_bend_or_grace_bend_start() && !style().style_b(Sid::USE_CUE_SIZE_FRET_FOR_GRACE_BENDS);
        if (!tab_grace_size_exception) {
            m *= style().style_d(Sid::GRACE_NOTE_MAG);
        }
    }
    return m;
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Chord::mag() const {
    const Staff *st = staff();
    return (st ? st->staff_mag(this) : 1.0) * intrinsic_mag();
}

//---------------------------------------------------------
//   segment
//---------------------------------------------------------

Segment *Chord::segment() const {
    EngravingObject *e = ownership_parent();
    for (; e && !e->is_segment(); e = e->ownership_parent()) {}
    return to_segment(e);
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

Measure *Chord::measure() const {
    EngravingObject *e = ownership_parent();
    for (; e && !e->is_measure(); e = e->ownership_parent()) {}
    return to_measure(e);
}

//---------------------------------------------------------
//   graceNotesBefore
//---------------------------------------------------------

GraceNotesGroup &Chord::grace_notes_before(bool filter_unplayable) const {
    grace_notes_before_.clear();
    for (Chord *c : grace_notes_) {
        assert(c->note_type() != NoteType::NORMAL && c->note_type() != NoteType::INVALID);
        if (c->note_type() & (NoteType::ACCIACCATURA | NoteType::APPOGGIATURA | NoteType::GRACE4 | NoteType::GRACE16 | NoteType::GRACE32)) {
            if (filter_unplayable && !c->is_chord_playable()) {
                continue;
            }

            grace_notes_before_.push_back(c);
        }
    }
    return grace_notes_before_;
}

//---------------------------------------------------------
//   graceNotesAfter
//---------------------------------------------------------

GraceNotesGroup &Chord::grace_notes_after(bool filter_unplayable) const {
    grace_notes_after_.clear();
    for (int i = static_cast<int>(grace_notes_.size()) - 1; i >= 0; i--) {
        Chord *c = grace_notes_[i];
        assert(c->note_type() != NoteType::NORMAL && c->note_type() != NoteType::INVALID);
        if (c->note_type() & (NoteType::GRACE8_AFTER | NoteType::GRACE16_AFTER | NoteType::GRACE32_AFTER)) {
            if (filter_unplayable && !c->is_chord_playable()) {
                continue;
            }

            grace_notes_after_.push_back(c);
        }
    }
    return grace_notes_after_;
}

Chord *Chord::grace_note_at(size_t idx) const {
    if (idx > grace_notes_.size()) {
        return nullptr;
    }

    return grace_notes_.at(idx);
}

//---------------------------------------------------------
//   allGraceChordsOfMainChord
//   returns a list containing all grace notes (chords) attached to the main chord and the main chord itself, in order
//---------------------------------------------------------
std::vector<Chord *> Chord::all_grace_chords_of_main_chord() {
    Chord *main_chord = is_grace() ? to_chord(ownership_parent()) : this;
    std::vector<Chord *> chords = {main_chord};
    const GraceNotesGroup &gn_before = main_chord->grace_notes_before();
    const GraceNotesGroup &gn_after = main_chord->grace_notes_after();
    chords.insert(chords.begin(), gn_before.begin(), gn_before.end());
    chords.insert(chords.end(), gn_after.begin(), gn_after.end());
    return chords;
}

//---------------------------------------------------------
//   setShowStemSlashInAdvance
//---------------------------------------------------------

void Chord::set_show_stem_slash_in_advance() {
    if (note_type_ == NoteType::NORMAL) {
        return;
    }
    if (is_grace_before()) {
        GraceNotesGroup &grace_before = to_chord(ownership_parent())->grace_notes_before();
        Chord *grace = grace_before.empty() ? nullptr : grace_before.front();
        if (grace && grace->beam_mode() != BeamMode::NONE && grace->beam_mode() != BeamMode::BEGIN) {
            grace->request_show_stem_slash(show_stem_slash());
        }
    }
    if (is_grace_after()) {
        GraceNotesGroup &grace_after = to_chord(ownership_parent())->grace_notes_after();
        Chord *grace = grace_after.empty() ? nullptr : grace_after.back();
        if (grace && grace->beam_mode() != BeamMode::NONE) {
            grace->request_show_stem_slash(show_stem_slash());
        }
    }
}

//---------------------------------------------------------
//   requestShowStemSlash
//---------------------------------------------------------

void Chord::request_show_stem_slash(bool show) {
    if (note_type_ == NoteType::NORMAL) {
        return;
    }
    if (beam()) {
        for (ChordRest *chord_rest : beam()->elements()) {
            if (chord_rest->is_chord()) {
                Chord *chord = to_chord(chord_rest);
                chord->set_show_stem_slash(show);
            }
        }
    } else {
        set_show_stem_slash(show);
    }
}

//---------------------------------------------------------
//   sortNotes
//---------------------------------------------------------

void Chord::sort_notes() { std::sort(notes().begin(), notes().end(), note_is_before); }

//---------------------------------------------------------
//   nextTiedChord
//    Return next chord if all notes in this chord are tied to it.
//    Set backwards=true to return the previous chord instead.
//
//    Note: the next chord might have extra notes that are not tied
//    back to this one. Set sameSize=true to return 0 in this case.
//---------------------------------------------------------

Chord *Chord::next_tied_chord(bool backwards, bool same_size) const {
    Segment *next_seg = backwards ? segment()->prev1(SegmentType::CHORD_REST) : segment()->next1(SegmentType::CHORD_REST);
    if (!next_seg) {
        return 0;
    }
    ChordRest *next_cr = next_seg->next_chord_rest(track(), backwards);
    if (!next_cr || !next_cr->is_chord()) {
        return 0;
    }
    Chord *next = to_chord(next_cr);
    if (same_size && notes().size() != next->notes().size()) {
        return 0; // sizes don't match so some notes can't be tied
    }
    if (tuplet() != next->tuplet()) {
        return 0; // next chord belongs to a different tuplet
    }
    for (Note *n : notes_) {
        Tie *tie = backwards ? n->tie_back() : n->tie_for();
        if (!tie) {
            return 0; // not tied
        }
        Note *nn = backwards ? tie->start_note() : tie->end_note();
        if (!nn || nn->chord() != next) {
            return 0; // tied to note in wrong voice, or tied over rest
        }
    }
    return next; // all notes in this chord are tied to notes in next chord
}

//---------------------------------------------------------
//   setNoteType
//---------------------------------------------------------

void Chord::set_note_type(NoteType t) {
    note_type_ = t;
    set_property(Pid::SHOW_STEM_SLASH, property_default(Pid::SHOW_STEM_SLASH));
}

//---------------------------------------------------------
//   toGraceAfter
//---------------------------------------------------------

void Chord::to_grace_after() {
    switch (note_type()) {
        case NoteType::APPOGGIATURA: set_note_type(NoteType::GRACE8_AFTER); break;
        case NoteType::GRACE16: set_note_type(NoteType::GRACE16_AFTER); break;
        case NoteType::GRACE32: set_note_type(NoteType::GRACE32_AFTER); break;
        default: break;
    }
}

bool Chord::is_pre_bend_or_grace_bend_start() const {
    if (!is_grace()) {
        return false;
    }

    for (const Note *note : notes_) {
        GuitarBend *gb = note->bend_for();
        if (gb && (gb->bend_type() == GuitarBendType::PRE_BEND || gb->bend_type() == GuitarBendType::GRACE_NOTE_BEND ||
                   gb->bend_type() == GuitarBendType::PRE_DIVE)) {
            return true;
        }
    }

    return false;
}

bool Chord::is_grace_bend_end() const {
    if (is_grace() || grace_notes_.empty()) {
        return false;
    }

    for (const Note *note : notes_) {
        GuitarBend *bend_back = note->bend_back();
        if (bend_back && bend_back->bend_type() == GuitarBendType::GRACE_NOTE_BEND) {
            return true;
        }
    }

    return false;
}

void Chord::set_is_trill_cue_note(bool v) { is_trill_cue_note_ = v; }

//---------------------------------------------------------
//   tremoloChordType
//---------------------------------------------------------

TremoloChordType Chord::tremolo_chord_type() const {
    if (tremolo_two_chord_) {
        if (tremolo_two_chord_->chord1() == this) {
            return TremoloChordType::TREMOLO_FIRST_CHORD;
        } else if (tremolo_two_chord_->chord2() == this) {
            return TremoloChordType::TREMOLO_SECOND_CHORD;
        } else {
            ASSERT_X(String(u"Chord::tremoloChordType(): inconsistency"));
        }
    } else if (tremolo_single_chord_) {
        return TremoloChordType::TREMOLO_SINGLE;
    }

    return TremoloChordType::TREMOLO_NONE;
}

//---------------------------------------------------------
//   nextElement
//---------------------------------------------------------

EngravingItem *Chord::next_element() {
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
            Note *n = to_note(e->ownership_parent());
            if (n == notes_.front()) {
                if (arpeggio_) {
                    return arpeggio_;
                } else if (tremolo_two_chord_) {
                    return tremolo_two_chord_;
                } else if (tremolo_single_chord_) {
                    return tremolo_single_chord_;
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
            SpannerSegment *s = to_spanner_segment(e);
            Spanner *sp = s->spanner();
            EngravingItem *el_st = sp->start_element();
            assert(el_st->is_note());
            Note *n = to_note(el_st);
            assert(n != nullptr);
            if (n == notes_.front()) {
                if (arpeggio_) {
                    return arpeggio_;
                } else if (tremolo_two_chord_) {
                    return tremolo_two_chord_;
                } else if (tremolo_single_chord_) {
                    return tremolo_single_chord_;
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
            if (tremolo_two_chord_) {
                return tremolo_two_chord_;
            } else if (tremolo_single_chord_) {
                return tremolo_single_chord_;
            }
            break;

        case ElementType::ACCIDENTAL:
            e = to_accidental(e)->note();
            // fall through

        case ElementType::NOTE: {
            if (e == notes_.front()) {
                if (arpeggio_) {
                    return arpeggio_;
                } else if (tremolo_two_chord_) {
                    return tremolo_two_chord_;
                } else if (tremolo_single_chord_) {
                    return tremolo_single_chord_;
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

    return ChordRest::next_element();
}

//---------------------------------------------------------
//   prevElement
//---------------------------------------------------------

EngravingItem *Chord::prev_element() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().back();
    }
    switch (e->type()) {
        case ElementType::NOTE: {
            if (is_grace()) {
                ChordRest *prev = Navigation::prev_chord_rest(this);
                if (prev) {
                    if (prev->is_chord()) {
                        return to_chord(prev)->notes().back();
                    }
                    return prev;
                }
            }

            GraceNotesGroup &grace_notes_before = this->grace_notes_before();
            if (!grace_notes_before.empty()) {
                if (Chord *grace_notes_before_last_chord = grace_notes_before.back()) {
                    if (e == grace_notes_before_last_chord->notes().back()) {
                        break;
                    }

                    Note *prev_note = grace_notes_before_last_chord->notes().back();
                    if (prev_note->is_pre_bend_or_dive_start() || prev_note->is_grace_bend_start()) {
                        return prev_note->bend_for()->front_segment();
                    }

                    ChordRest *prev = Navigation::prev_chord_rest(this);
                    if (prev) {
                        if (prev->is_chord()) {
                            return to_chord(prev)->notes().back();
                        }
                        return prev;
                    }
                }
            }

            if (e == notes_.back()) {
                break;
            }
            Note *prev_note = nullptr;
            for (auto &i : notes_) {
                if (i == e) {
                    prev_note = *(&i + 1);
                }
            }
            EngravingItem *next = prev_note->last_element_before_segment();
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
            EngravingItem *el_n = n->last_element_before_segment();
            assert(el_n != nullptr);
            return el_n;
        }

        default: break;
    }
    return ChordRest::prev_element();
}

//---------------------------------------------------------
//   lastElementBeforeSegment
//---------------------------------------------------------

EngravingItem *Chord::last_element_before_segment() {
    if (tremolo_single_chord_) {
        return tremolo_single_chord_;
    } else if (tremolo_two_chord_) {
        return tremolo_two_chord_;
    } else if (arpeggio_) {
        return arpeggio_;
    } else {
        Note *n = notes_.front();
        EngravingItem *el_n = n->last_element_before_segment();
        assert(el_n != nullptr);
        return el_n;
    }
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Chord::next_segment_element() {
    for (track_idx_t v = track() + 1; staff_idx() == v / ::mu::engraving::voices; ++v) {
        EngravingItem *e = segment()->element(v);
        if (e) {
            if (e->is_chord()) {
                return to_chord(e)->notes().back();
            }

            return e;
        }
    }

    return ChordRest::next_segment_element();
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Chord::prev_segment_element() {
    EngravingItem *el = score()->selection().element();
    if (!el && !score()->selection().elements().empty()) {
        el = score()->selection().elements().front();
    }
    EngravingItem *e = segment()->last_in_prev_segments(el->staff_idx());
    if (e) {
        if (e->is_chord()) {
            return to_chord(e)->notes().front();
        }
        return e;
    }

    return ChordRest::prev_segment_element();
}

//---------------------------------------------------------
//   accessibleExtraInfo
//---------------------------------------------------------

void Chord::undo_change_property(Pid id, const PropertyValue &new_value) { undo_change_property(id, new_value, property_flags(id)); }

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void Chord::undo_change_property(Pid id, const PropertyValue &new_value, PropertyFlags ps) {
    if (id == Pid::VISIBLE) {
        process_siblings([=](EngravingItem *element) { element->undo_change_property(id, new_value, ps); }, false);
    }

    ChordRest::undo_change_property(id, new_value, ps);
}

std::set<SymId> Chord::articulation_symbol_ids() const {
    std::set<SymId> result;
    for (const Articulation *artic : articulations_) {
        result.insert(artic->sym_id());
    }

    return result;
}

//---------------------------------------------------------
//   getNoteEventLists
//    Get contents of all NoteEventLists for all notes in
//    the chord.
//---------------------------------------------------------

std::vector<NoteEventList> Chord::get_note_event_lists() {
    std::vector<NoteEventList> ell;
    if (notes().empty()) {
        return ell;
    }
    ell.reserve(notes().size());
    for (size_t i = 0; i < notes().size(); ++i) {
        ell.push_back(NoteEventList(notes()[i]->play_events()));
    }
    return ell;
}

//---------------------------------------------------------
//   setNoteEventLists
//    Set contents of all NoteEventLists for all notes in
//    the chord.
//---------------------------------------------------------

void Chord::set_note_event_lists(std::vector<NoteEventList> &ell) {
    if (notes().empty()) {
        return;
    }
    IF_ASSERT_FAILED(ell.size() == notes().size()) { return; }
    for (size_t i = 0; i < ell.size(); i++) {
        notes()[i]->set_play_events(ell[i]);
    }
}

//---------------------------------------------------------
//   styleChanged
//---------------------------------------------------------
void Chord::style_changed() {
    auto update_elements_style = [](EngravingItem *e) { e->style_changed(); };
    scan_elements(update_elements_style);
}

void Chord::compute_kerning_exceptions() {
    allow_kerning_above_ = true;
    allow_kerning_below_ = true;
    for (Articulation *art : articulations_) {
        if (art->up()) {
            allow_kerning_above_ = false;
        } else {
            allow_kerning_below_ = false;
        }
    }
    if ((start_end_slurs_.start_up || start_end_slurs_.end_up) && !ldata()->up) {
        allow_kerning_above_ = false;
    }
    if ((start_end_slurs_.start_down || start_end_slurs_.end_down) && ldata()->up) {
        allow_kerning_below_ = false;
    }
}

Ornament *Chord::find_ornament(bool for_playback) const {
    for (Articulation *art : articulations_) {
        if (art->is_ornament()) {
            return to_ornament(art);
        }
    }
    for (Spanner *spanner : starting_spanners_) {
        if (spanner->is_trill()) {
            return to_trill(spanner)->ornament();
        }
    }
    if (for_playback) {
        // TODO: cleanup.
        // We shouldn't do this kind of special cases, and the DOM shouldn't know anything about playback.
        // This should be in a different function that returns the ornament from the Trill ending on this chord. [MS]
        for (Spanner *spanner : ending_spanners_) {
            if (spanner->is_trill()) {
                return to_trill(spanner)->ornament();
            }
        }
    }
    return nullptr;
}

//---------------------------------
// firstGraceOrNote
//---------------------------------
Note *Chord::first_grace_or_note() {
    GraceNotesGroup &grace_notes_before = this->grace_notes_before();
    if (!grace_notes_before.empty()) {
        if (Chord *grace_notes_before_first_chord = grace_notes_before.front()) {
            return grace_notes_before_first_chord->notes().front();
        }
    }

    return this->notes().back();
}

//---------------------------------
// GRACE NOTES
//---------------------------------

GraceNotesGroup::GraceNotesGroup(DummyParentOr<Chord> c) : EngravingItem(ElementType::GRACE_NOTES_GROUP, c), parent_value(c.as<Chord>()) {}

void GraceNotesGroup::set_pos(double x, double y) {
    EngravingItem::set_pos(x, y);
    for (unsigned i = 0; i < this->size(); ++i) {
        Chord *chord = this->at(i);
        chord->mutldata()->move(PointF(x, y));
    }
}

void GraceNotesGroup::add_to_shape() {
    for (Chord *grace : *this) {
        const PointF y_offset = grace->staff_offset();
        staff_idx_t staff_idx = grace->staff_idx();
        staff_idx_t v_staff_idx = grace->v_staff_idx();
        Shape &s = appended_segment_value->staff_shape(staff_idx);
        s.add(grace->shape(LdAccess::PASS).translate(grace->pos() + y_offset));
        if (v_staff_idx != staff_idx) {
            // Cross-staff grace notes add their shape to both the origin and the destination staff
            Shape &s2 = appended_segment_value->staff_shape(v_staff_idx);
            s2.add(grace->shape().translate(grace->pos() + y_offset));
        }
    }
}
} // namespace mu::engraving
