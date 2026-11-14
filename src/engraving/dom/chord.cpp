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
#include <stdexcept>
#include <vector>

#include "../editing/addremoveelement.h"
#include "../editing/editchord.h"
#include "../editing/editnote.h"
#include "../editing/editvisibility.h"
#include "../editing/navigation.h"
#include "accidental.h"
#include "arpeggio.h"
#include "articulation.h"
#include "beam.h"
#include "chordbracket.h"
#include "chordline.h"
#include "containers.h"
#include "dom/parenthesis.h"
#include "drumset.h"
#include "factory.h"
#include "guitarbend.h"
#include "hook.h"
#include "infrastructure/eidregister.h"
#include "key.h"
#include "ledgerline.h"
#include "log.h"
#include "masterscore.h"
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

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
NoteParenthesisInfo::NoteParenthesisInfo(std::vector<Note *> n_list, bool generated) : generated_(generated), notes_(std::move(n_list)) {
    std::sort(notes_.begin(), notes_.end(), note_is_before);
}

void NoteParenthesisInfo::insert_note(Note *note) {
    notes_.insert(std::upper_bound(notes_.begin(), notes_.end(), note, note_is_before), note);
}

void NoteParenthesisInfo::remove_note(Note *note) { muse::remove(notes_, note); }

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

//   upLine / downLine

int Chord::up_line() const { return on_tab_staff() ? up_string() * 2 : up_note()->line(); }

int Chord::down_line() const { return on_tab_staff() ? down_string() * 2 : down_note()->line(); }

//   upString / downString
//
//    return the topmost / bottommost string used by chord
//    Top and bottom refer to the DRAWN position, not the position in the instrument
//    (i.e., upside-down TAB are taken into account)
//
//    If no staff, always return 0
//    If staff is not a TAB, always returns TOP and BOTTOM staff lines

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

Chord::Chord(DummyParentOr<Segment> parent) : ChordRest(ElementType::CHORD, parent) {
    stem_direction_ = DirectionV::AUTO;
    arpeggio_ = 0;
    span_arpeggio_ = 0;
    ends_note_anchored_line_ = false;
    note_type_ = NoteType::NORMAL;
    no_stem_ = false;
    show_stem_slash_ = note_type_ == NoteType::ACCIACCATURA;
    play_event_type_ = PlayEventType::AUTO;
    cross_measure_ = CrossMeasure::UNKNOWN;
    grace_index_ = 0;
    combine_voice_ = AutoOnOff::AUTO;
}

Chord::Chord(const Chord &c, bool link) : ChordRest(c, link) {
    clone_authored_overrides(authored_overrides, c.authored_overrides, *master_score()->eid_register());
    // The legacy pair retained its source track when an excerpt remapped the chord.
    for (auto &carrier : authored_overrides.carriers)
        if (carrier.child.subordinal == 1 && carrier.track == muse::nidx &&
            (carrier.child.role == AuthoredChildRole::PAREN_LEFT || carrier.child.role == AuthoredChildRole::PAREN_RIGHT))
            carrier.track = c.track();
    if (link) {
        score()->apply_link(this, const_cast<Chord *>(&c));
    }

    for (Note *onote : c.notes_) {
        Note *nnote = Factory::copy_note(*onote, link);
        add(nnote);
    }
    for (Chord *gn : c.grace_notes()) {
        Chord *nc = new Chord(*gn, link);
        nc->set_grace_index(grace_notes_.size());
        add(nc);
    }
    for (Articulation *a : c.articulations_) { // make deep copy
        Articulation *na = a->clone();
        if (link) {
            score()->apply_link(na, a);
        }
        na->set_ownership_parent(this);
        na->set_track(track());
        articulations_.push_back(na);
    }
    ends_note_anchored_line_ = false;
    arpeggio_ = 0;

    span_arpeggio_ = c.span_arpeggio_;
    grace_index_ = c.grace_index_;
    stem_override_ = c.stem_override_;
    no_stem_ = c.no_stem_;
    show_stem_slash_ = c.show_stem_slash_;
    play_event_type_ = c.play_event_type_;
    stem_direction_ = c.stem_direction_;
    note_type_ = c.note_type_;
    cross_measure_ = CrossMeasure::UNKNOWN;
    combine_voice_ = c.combine_voice_;

    if (c.arpeggio_) {
        Arpeggio *a = c.arpeggio_->is_chord_bracket() ? new ChordBracket(*to_chord_bracket(c.arpeggio_)) : new Arpeggio(*(c.arpeggio_));
        add(a);
        if (link) {
            score()->apply_link(a, const_cast<Arpeggio *>(c.arpeggio_));
        }
    }

    if (c.tremolo_single_chord_) {
        TremoloSingleChord *t = Factory::copy_tremolo_single_chord(*(c.tremolo_single_chord_));
        if (link) {
            score()->apply_link(t, const_cast<TremoloSingleChord *>(c.tremolo_single_chord_));
        }
        add(t);
    } else if (c.tremolo_two_chord_) {
        if (c.tremolo_two_chord_->chord1() == &c) {
            TremoloTwoChord *t = Factory::copy_tremolo_two_chord(*(c.tremolo_two_chord_));
            if (link) {
                score()->apply_link(t, const_cast<TremoloTwoChord *>(c.tremolo_two_chord_));
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
                score()->apply_link(ncl, cl);
            }
        }
    }

    for (EngravingItem *e : c.el()) {
        if (e->is_chord_bracket()) {
            EngravingItem *cloned_chord_bracket = link ? e->linked_clone() : e->clone();
            add(cloned_chord_bracket);
        }
    }

    if (!c.note_parentheses().empty()) {
        for (const NoteParenthesisInfo *info : c.note_parentheses()) {
            std::vector<Note *> new_notes;
            new_notes.reserve(info->notes().size());
            for (Note *note : info->notes()) {
                new_notes.push_back(find_note(note->pitch()));
            }

            note_parens_.push_back(new NoteParenthesisInfo(new_notes, info->generated()));
        }
    }
}

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

//   ~Chord

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
    muse::delete_all(grace_notes_);
    muse::delete_all(notes_);
    muse::delete_all(note_parens_);
}

double Chord::note_head_width() const { return score()->note_head_width() * mag(); }

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
        if (tr->chord1() == this) source_child_added(*this, tr);
        return;
    }
    source_child_removed(*this, tremolo_two_chord_);
    source_child_removed(*this, tremolo_single_chord_);

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
    if (tr && tr->chord1() == this) source_child_added(*this, tr);
}

void Chord::set_tremolo_single_chord(TremoloSingleChord *tr) {
    if (tremolo_single_chord_ && tr && tr == tremolo_single_chord_) {
        source_child_added(*this, tr);
        return;
    }
    source_child_removed(*this, tremolo_single_chord_);
    source_child_removed(*this, tremolo_two_chord_);

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
    source_child_added(*this, tr);
}

void Chord::set_arpeggio(Arpeggio *a) {
    if (arpeggio_ != a) source_child_removed(*this, arpeggio_);
    arpeggio_ = a;
    source_child_added(*this, a);
}

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
            if (source_object(*this) && voice() && measure() && note->visible()) {
                measure()->set_has_voices(staff_idx(), true);
            }
        } break;
        case ElementType::ARPEGGIO: arpeggio_ = to_arpeggio(e); break;
        case ElementType::CHORD_BRACKET: add_el(e); break;
        case ElementType::TREMOLO_TWOCHORD: set_tremolo_two_chord(item_cast<TremoloTwoChord *>(e)); break;
        case ElementType::TREMOLO_SINGLECHORD: set_tremolo_single_chord(item_cast<TremoloSingleChord *>(e)); break;
        case ElementType::GLISSANDO: ends_note_anchored_line_ = true; break;
        case ElementType::CHORDLINE: add_el(e); break;
        case ElementType::CHORD: {
            Chord *gc = to_chord(e);
            assert(gc->note_type() != NoteType::NORMAL);
            size_t idx = gc->grace_index();
            gc->set_flag(ElementFlag::MOVABLE, true);
            grace_notes_.insert(grace_notes_.begin() + idx, gc);
            for (size_t i = idx; i < grace_notes_.size(); ++i) grace_notes_[i]->set_grace_index(i);
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

    source_child_added(*this, e);

    e->added();
}

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
                if (has_layout_data())
                    for (auto &group : mutldata()->parentheses) group.info.remove_note(note);
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
            if (source_object(*this) && voice() && measure() && note->visible()) {
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
        case ElementType::CHORDLINE: remove_el(e); break;
        case ElementType::CHORD: {
            auto i = std::find(grace_notes_.begin(), grace_notes_.end(), to_chord(e));
            IF_ASSERT_FAILED(i != grace_notes_.end()) { break; }
            Chord *grace = *i;
            grace->set_grace_index(i - grace_notes_.begin());
            grace_notes_.erase(i);
            for (size_t idx = 0; idx < grace_notes_.size(); ++idx) grace_notes_[idx]->set_grace_index(idx);
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

    source_child_removed(*this, e);

    e->removed();
}

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

double Chord::up_pos() const { return up_note()->pos().y(); }

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

void Chord::process_siblings(std::function<void(EngravingItem *)> func, bool include_temporary_siblings) const {
    if (include_temporary_siblings) {
        if (stem()) func(stem());
        if (hook()) func(hook());
        if (stem_slash()) func(stem_slash());
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
        for (LedgerLine *ll : ledger_lines()) {
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

Chord::LayoutData::~LayoutData() {
    delete stem;
    delete hook;
    delete stem_slash;
    for (auto &group : parentheses) {
        delete group.left;
        delete group.right;
    }
    muse::delete_all(ledger_lines);
}

void Chord::resize_ledger_lines_to(size_t new_size) {
    auto &lines = mutldata()->ledger_lines;
    // Reserve ownership slots before parenting shells. A failed growth must not
    // leave an attached ledger line outside the payload's owning vector.
    lines.reserve(new_size);
    while (lines.size() < new_size) {
        auto *line = new LedgerLine(this);
        line->set_generated(true);
        lines.push_back(line);
    }
    while (lines.size() > new_size) {
        assert(lines.back()->generated());
        delete lines.back();
        lines.pop_back();
    }
}

void Chord::set_beam_extension(double extension) {
    if (stem()) {
        double base_length = stem()->absolute_from_spatium(stem()->base_length());
        stem()->set_base_length(std::max(Spatium::from_absolute(base_length + extension, spatium()), 0.0_sp));
        mutldata()->default_stem_length =
            std::max(ldata()->default_stem_length + extension, stem()->absolute_from_spatium(stem()->base_length()));
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

//    underBeam: true, if grace note is placed under a beam.

bool Chord::under_beam() const {
    if (note_type_ == NoteType::NORMAL) {
        return false;
    }
    const Chord *cr = to_chord(ownership_parent());
    Beam *beam = cr->beam();
    if (!beam || !cr->beam()->up()) {
        return false;
    }
    size_t s = beam->rendered_elements().size();
    if (is_grace_before()) {
        if (beam->rendered_elements()[0] != cr) {
            return true;
        }
    }
    if (is_grace_after()) {
        if (beam->rendered_elements()[s - 1] != cr) {
            return true;
        }
    }
    return false;
}

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

void Chord::scan_elements(std::function<void(EngravingItem *)> func) {
    for (Articulation *a : articulations_) {
        a->scan_elements(func);
    }
    // BSP insertion order breaks ties in the paint sort; preserve the legacy scan order.
    if (hook()) func(hook());
    if (stem()) func(stem());
    if (stem_slash()) func(stem_slash());
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
        for (LedgerLine *ll : ledger_lines()) {
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

    for (const auto &p : layout_parentheses()) {
        p.left->scan_elements(func);
        p.right->scan_elements(func);
    }
    ChordRest::scan_elements(func);
}

const std::vector<Chord::ParenthesisGroup> &Chord::layout_parentheses() const {
    static const std::vector<ParenthesisGroup> empty;
    return has_layout_data() ? ldata()->parentheses : empty;
}

const Chord::ParenthesisGroup *Chord::layout_parenthesis_group(const Note *note) const {
    for (const auto &group : layout_parentheses())
        if (std::ranges::find(group.info.notes(), note) != group.info.notes().end()) return &group;
    return nullptr;
}

const Chord::ParenthesisGroup *Chord::layout_parenthesis_group(const Parenthesis *paren) const {
    for (const auto &group : layout_parentheses())
        if (paren == group.left || paren == group.right) return &group;
    return nullptr;
}

const NoteParenthesisInfo *Chord::matching_parenthesis_group(const NoteParenthesisInfo &group) const {
    for (auto it = note_parens_.rbegin(); it != note_parens_.rend(); ++it) {
        const auto &notes = (*it)->notes();
        if (!notes.empty() && notes.size() == group.notes().size() && std::ranges::all_of(group.notes(), [&](const Note *note) {
                return std::ranges::any_of(
                    notes, [&](const Note *candidate) { return candidate == note || note->is_linked(const_cast<Note *>(candidate)); });
            }))
            return *it;
    }
    return nullptr;
}

size_t Chord::parenthesis_group_index(const NoteParenthesisInfo *info) const {
    return std::find(note_parens_.begin(), note_parens_.end(), info) - note_parens_.begin();
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

void Chord::add_note_parenthesis_info(NoteParenthesisInfo *note_paren_info) {
    for (auto role : {AuthoredChildRole::PAREN_LEFT, AuthoredChildRole::PAREN_RIGHT}) {
        const AuthoredChildKey key{role, static_cast<uint32_t>(note_parens_.size()), 0, 1};
        if (!authored_overrides.carrier(key))
            authored_overrides.set_carrier({key, ElementType::PARENTHESIS, master_score()->eid_register()->new_eid(false)});
    }
    note_parens_.push_back(note_paren_info);
}

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
        const auto index = it - note_parens_.begin();
        const auto grouped = [](const auto &entry) {
            return entry.child.subordinal == 1 &&
                   (entry.child.role == AuthoredChildRole::PAREN_LEFT || entry.child.role == AuthoredChildRole::PAREN_RIGHT);
        };
        std::erase_if(authored_overrides.entries, [&](const auto &entry) { return grouped(entry) && entry.child.ordinal == index; });
        std::erase_if(authored_overrides.carriers, [&](const auto &entry) { return grouped(entry) && entry.child.ordinal == index; });
        for (auto &entry : authored_overrides.entries)
            if (grouped(entry) && entry.child.ordinal > index) --entry.child.ordinal;
        for (auto &entry : authored_overrides.carriers)
            if (grouped(entry) && entry.child.ordinal > index) --entry.child.ordinal;
        note_parens_.erase(it);
    }
}

void Chord::add_note_to_parenthesis_info(Note *note, NoteParenthesisInfo *note_paren_info) {

    if (!note_paren_info) {
        return;
    }

    note_paren_info->insert_note(note);
}

void Chord::remove_note_from_parenthesis_info(Note *note, const NoteParenthesisInfo *group) {
    auto *note_paren_info = const_cast<NoteParenthesisInfo *>(group);

    if (!note_paren_info) {
        return;
    }

    note_paren_info->remove_note(note);
}

//   @note Now every related to chord element has it's own "PLAY" property,
//         However, there is no way to control these properties outside the scope of the chord since the new Properties panel.
//         So we'll use a chord as a proxy entity for "PLAY" property handling

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
    apply_change_span_arpeggio(this, a);
}

ChordLine *Chord::chord_line() const {
    for (EngravingItem *item : el()) {
        if (item && item->is_chord_line()) {
            return to_chord_line(item);
        }
    }

    return nullptr;
}

void Chord::set_color(const Color &color) {
    ChordRest::set_color(color);

    for (Note *note : notes_) {
        note->undo_change_property(Pid::COLOR, PropertyValue::from_value(color));
    }
}

void Chord::set_stem_direction(DirectionV d) { stem_direction_ = d; }

void Chord::local_spatium_changed(double old_value, double new_value) {
    ChordRest::local_spatium_changed(old_value, new_value);
    for (EngravingItem *e : grace_notes()) {
        e->local_spatium_changed(old_value, new_value);
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

void Chord::reset() {
    undo_change_property(Pid::STEM_DIRECTION, DirectionV::AUTO);
    ChordRest::reset();
}

bool Chord::slash() {
    Note *n = up_note();
    return n->fixed();
}

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
            EditVisibility::change_dots_visible(n, dots(), true);
            n->undo_change_property(Pid::VISIBLE, true);
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
            EditVisibility::change_dots_visible(n, dots(), false);
            n->undo_change_property(Pid::VISIBLE, false);
        }
    }
}

//    sets/resets the chord endsNoteAnchoredLine_ according any note anchored line
//    end into this chord or no.

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

//    - this is normally called after cloning a chord to tie a note over the barline
//    - there is no special undo handling; the assumption is that undo will simply remove the cloned chord
//    - two note tremolos are converted into simple notes
//    - single note tremolos are optionally retained

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

    muse::delete_all(grace_notes_);
    remove_all_grace_notes();
    muse::delete_all(articulations());
    articulations().clear();
    for (Note *n : notes()) {
        for (EngravingItem *e : n->el()) {
            n->remove(e);
        }
    }
    ChordRest::remove_markings(keep_tremolo);
}

//   returns the INTRINSIC mag of the chord (i.e. NOT scaled
//   by staff size)

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

double Chord::mag() const {
    const Staff *st = staff();
    return (st ? st->staff_mag(this) : 1.0) * intrinsic_mag();
}

Segment *Chord::segment() const {
    EngravingObject *e = ownership_parent();
    for (; e && !e->is_segment(); e = e->ownership_parent()) {}
    return to_segment(e);
}

Measure *Chord::measure() const {
    EngravingObject *e = ownership_parent();
    for (; e && !e->is_measure(); e = e->ownership_parent()) {}
    return to_measure(e);
}

void GraceNotesView::Iterator::skip_unmatched() {
    while (index < source->size()) {
        const Chord *chord = **this;
        const NoteType type = chord->note_type();
        assert(type != NoteType::NORMAL && type != NoteType::INVALID);
        const bool matches =
            after ?
                bool(type & (NoteType::GRACE8_AFTER | NoteType::GRACE16_AFTER | NoteType::GRACE32_AFTER)) :
                bool(type & (NoteType::ACCIACCATURA | NoteType::APPOGGIATURA | NoteType::GRACE4 | NoteType::GRACE16 | NoteType::GRACE32));
        if (matches && (!filter_unplayable || chord->is_chord_playable())) {
            return;
        }
        ++index;
    }
}

GraceNotesView::Iterator &GraceNotesView::Iterator::operator++() {
    ++index;
    skip_unmatched();
    return *this;
}

Chord *GraceNotesView::back() const {
    Chord *last = nullptr;
    for (Chord *chord : *this) {
        last = chord;
    }
    assert(last);
    return last;
}

Chord *GraceNotesView::at(size_t index) const {
    for (Chord *chord : *this) {
        if (index-- == 0) {
            return chord;
        }
    }
    throw std::out_of_range("grace note index");
}

GraceNotesView Chord::grace_notes_before(bool filter_unplayable) const { return {&grace_notes_, false, filter_unplayable}; }

GraceNotesView Chord::grace_notes_after(bool filter_unplayable) const { return {&grace_notes_, true, filter_unplayable}; }

void Chord::remove_all_grace_notes() {
    for (auto *grace : grace_notes_) source_child_removed(*this, grace);
    grace_notes_.clear();
}

Chord *Chord::grace_note_at(size_t idx) const {
    if (idx > grace_notes_.size()) {
        return nullptr;
    }

    return grace_notes_.at(idx);
}

//   returns a list containing all grace notes (chords) attached to the main chord and the main chord itself, in order
std::vector<Chord *> Chord::all_grace_chords_of_main_chord() {
    Chord *main_chord = is_grace() ? to_chord(ownership_parent()) : this;
    std::vector<Chord *> chords = {main_chord};
    const auto gn_before = main_chord->grace_notes_before();
    const auto gn_after = main_chord->grace_notes_after();
    chords.insert(chords.begin(), gn_before.begin(), gn_before.end());
    chords.insert(chords.end(), gn_after.begin(), gn_after.end());
    return chords;
}

void Chord::set_show_stem_slash_in_advance() {
    if (note_type_ == NoteType::NORMAL) {
        return;
    }
    if (is_grace_before()) {
        const auto grace_before = to_chord(ownership_parent())->grace_notes_before();
        Chord *grace = grace_before.empty() ? nullptr : grace_before.front();
        if (grace && grace->beam_mode() != BeamMode::NONE && grace->beam_mode() != BeamMode::BEGIN) {
            grace->request_show_stem_slash(show_stem_slash());
        }
    }
    if (is_grace_after()) {
        const auto grace_after = to_chord(ownership_parent())->grace_notes_after();
        Chord *grace = grace_after.empty() ? nullptr : grace_after.back();
        if (grace && grace->beam_mode() != BeamMode::NONE) {
            grace->request_show_stem_slash(show_stem_slash());
        }
    }
}

void Chord::request_show_stem_slash(bool show) {
    if (note_type_ == NoteType::NORMAL) {
        return;
    }
    // A prior operation in the batch can invalidate the retained rendered group.
    auto *group = beam();
    if (!group && has_layout_data()) group = ldata()->rendered_beam.value_or(nullptr);
    if (group) {
        const auto members = static_cast<const Beam *>(group)->rendered_elements();
        for (ChordRest *chord_rest : members) {
            if (chord_rest->is_chord()) {
                Chord *chord = to_chord(chord_rest);
                chord->set_show_stem_slash(show);
            }
        }
    } else {
        set_show_stem_slash(show);
    }
}

void Chord::sort_notes() { std::sort(notes().begin(), notes().end(), note_is_before); }

//    Return next chord if all notes in this chord are tied to it.
//    Set backwards=true to return the previous chord instead.
//
//    Note: the next chord might have extra notes that are not tied
//    back to this one. Set sameSize=true to return 0 in this case.

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

void Chord::set_note_type(NoteType t) {
    note_type_ = t;
    set_property(Pid::SHOW_STEM_SLASH, property_default(Pid::SHOW_STEM_SLASH));
}

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

void Chord::undo_change_property(Pid id, const PropertyValue &new_value) { undo_change_property(id, new_value, property_flags(id)); }

void Chord::undo_change_property(Pid id, const PropertyValue &new_value, PropertyFlags ps) {
    if (id == Pid::VISIBLE) {
        for (auto role : {AuthoredChildRole::STEM, AuthoredChildRole::HOOK, AuthoredChildRole::STEM_SLASH})
            EditVisibility::change_chord_child_visible(this, role, new_value.to_bool());
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

//    Get contents of all NoteEventLists for all notes in
//    the chord.

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

//    Set contents of all NoteEventLists for all notes in
//    the chord.

void Chord::set_note_event_lists(std::vector<NoteEventList> &ell) {
    if (notes().empty()) {
        return;
    }
    IF_ASSERT_FAILED(ell.size() == notes().size()) { return; }
    for (size_t i = 0; i < ell.size(); i++) {
        notes()[i]->set_play_events(ell[i]);
    }
}

void Chord::style_changed() {
    auto update_elements_style = [](EngravingItem *e) { e->style_changed(); };
    scan_elements(update_elements_style);
}

void Chord::compute_kerning_exceptions() {
    mutldata()->allow_kerning_above = true;
    mutldata()->allow_kerning_below = true;
    for (Articulation *art : articulations_) {
        if (art->up()) {
            mutldata()->allow_kerning_above = false;
        } else {
            mutldata()->allow_kerning_below = false;
        }
    }
    if ((ldata()->start_end_slurs.start_up || ldata()->start_end_slurs.end_up) && !ldata()->up) {
        mutldata()->allow_kerning_above = false;
    }
    if ((ldata()->start_end_slurs.start_down || ldata()->start_end_slurs.end_down) && ldata()->up) {
        mutldata()->allow_kerning_below = false;
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

Note *Chord::first_grace_or_note() {
    const auto grace_notes_before = this->grace_notes_before();
    if (!grace_notes_before.empty()) {
        if (Chord *grace_notes_before_first_chord = grace_notes_before.front()) {
            return grace_notes_before_first_chord->notes().front();
        }
    }

    return this->notes().back();
}

// GRACE NOTES

GraceNotesGroup::GraceNotesGroup(DummyParentOr<Chord> c) : EngravingItem(ElementType::GRACE_NOTES_GROUP, c), parent_value(c.as<Chord>()) {
    // LayoutContext owns this object. The source chord must never delete it.
    move_to_dummy();
}

EngravingItem *GraceNotesGroup::layout_parent() const { return parent_value; }

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
