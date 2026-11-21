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

#include "range.h"

#include "barline.h"
#include "chord.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/masterscore.h"
#include "engraving/infrastructure/eidregister.h"
#include "excerpt.h"
#include "factory.h"
#include "linkedobjects.h"
#include "log.h"
#include "marker.h"
#include "measure.h"
#include "measurerepeat.h"
#include "note.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "slur.h"
#include "spacer.h"
#include "staff.h"
#include "tie.h"
#include "tremolosinglechord.h"
#include "tremolotwochord.h"
#include "tuplet.h"
#include "utils.h"
#include "volta.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   cleanupTuplet
//---------------------------------------------------------

static void cleanup_tuplet(Tuplet *t) {
    const auto elements(t->elements());
    t->clear();
    for (DurationElement *e : elements) {
        e->set_tuplet(nullptr); // prevent deleting the top tuplet by its children
        if (e->is_tuplet()) {
            cleanup_tuplet(to_tuplet(e));
        }
        delete e;
    }
}

static bool is_note_anchored_line(const Spanner *sp) { return sp && (sp->is_glissando() || sp->is_guitar_bend() || sp->is_note_line()); }

static bool chord_anchors_note_line(const Chord &chord) {
    auto anchors = [](const Chord *c) {
        return std::ranges::any_of(c->notes(), [](const Note *n) {
            return std::ranges::any_of(n->spanner_for(), is_note_anchored_line) ||
                   std::ranges::any_of(n->spanner_back(), is_note_anchored_line);
        });
    };
    return anchors(&chord) || std::ranges::any_of(chord.grace_notes(), anchors);
}

static bool anchors_note_line(const Chord &chord) {
    if (!chord.links()) {
        return chord_anchors_note_line(chord);
    }
    return std::ranges::any_of(
        *chord.links(), [](const EngravingObject *obj) { return obj && obj->is_chord() && chord_anchors_note_line(*to_chord(obj)); });
}

static bool has_grace_notes_in_links(const Chord &chord) {
    if (!chord.links()) {
        return !chord.grace_notes().empty();
    }
    return std::ranges::any_of(*chord.links(),
                               [](const EngravingObject *obj) { return obj && obj->is_chord() && !to_chord(obj)->grace_notes().empty(); });
}

static Spanner *clone_note_spanner(const Spanner &src) {
    Spanner *dst = to_spanner(src.clone());
    dst->set_position_linked_to_master(src.is_position_linked_to_master());
    dst->set_appearance_linked_to_master(src.is_appearance_linked_to_master());
    dst->set_exclude_from_other_parts(src.exclude_from_other_parts());
    return dst;
}

static bool attached_in_score(EngravingObject *item, Score *score, const EIDRegister &registry) {
    if (!item || item->score() != score) {
        return false;
    }
    if (registry.live_item(registry.handle_of(score))) {
        return registry.live_item(registry.handle_of(item)) == item;
    }
    for (EngravingObject *cur = item; cur != score; cur = cur->ownership_parent()) {
        if (!cur || !source_attached(*cur)) {
            return false;
        }
    }
    return true;
}

static void record_written_chord(TrackList::WrittenChordMap *written_chords, const Chord *src, Chord *dst) {
    if (!written_chords || !src || !dst) {
        return;
    }
    auto &span = (*written_chords)[src];
    if (!span.first) {
        span.first = dst;
    }
    span.last = dst;
}

//---------------------------------------------------------
//   TrackList
//---------------------------------------------------------

TrackList::~TrackList() {
    for (EngravingItem *e : *this) {
        if (e->is_tuplet()) {
            Tuplet *t = to_tuplet(e);
            cleanup_tuplet(t);
        } else {
            delete e;
        }
    }
}

//---------------------------------------------------------
//   appendTuplet
//---------------------------------------------------------

void TrackList::append_tuplet(Tuplet *src_tuplet, Tuplet *dst_tuplet) {
    for (DurationElement *de : src_tuplet->elements()) {
        DurationElement *e = to_duration_element(de->clone());
        e->move_to_dummy();
        dst_tuplet->add(e);
        if (de->is_chord()) {
            range_->record_read_chord(to_chord(de), to_chord(e));
        }
        if (de->is_tuplet()) {
            Tuplet *st = to_tuplet(de);
            Tuplet *dt = to_tuplet(e);
            append_tuplet(st, dt);
        } else if (de->ownership_parent() && de->ownership_parent()->is_segment()) {
            Segment *seg = to_segment(de->ownership_parent());
            for (EngravingItem *ee : seg->annotations()) {
                bool add_sys_object = ee->system_flag() && !ee->is_linked() && ee->track() == 0 && e->track() == 0;
                if (add_sys_object || (!ee->system_flag() && ee->track() == e->track())) {
                    range_->annotations_.push_back({seg->tick(), ee->clone()});
                }
            }
        }
    }
}

//---------------------------------------------------------
//   combineTuplet
//---------------------------------------------------------

void TrackList::combine_tuplet(Tuplet *dst, Tuplet *src) {
    dst->set_ticks(dst->ticks() * 2);
    dst->set_base_len(dst->base_len().shift(-1));

    // try to combine tie'd notes
    unsigned idx = 0;
    if (dst->elements().back()->is_chord() && src->elements().front()->is_chord()) {
        Chord *chord = to_chord(src->elements().front());
        bool akkumulate_chord = true;
        for (Note *n : chord->notes()) {
            if (!n->tie_back_non_partial() || !n->tie_back()->generated()) {
                akkumulate_chord = false;
                break;
            }
        }
        if (akkumulate_chord && range_->can_absorb_continuation(to_chord(dst->elements().back()), chord)) {
            Chord *bc = to_chord(dst->elements().back());
            bc->set_ticks(bc->ticks() + chord->ticks());

            // forward ties
            int i = 0;
            for (Note *n : bc->notes()) {
                n->set_tie_for(chord->notes()[i]->tie_for());
                ++i;
            }
            range_->record_read_chord(chord, bc);
            idx = 1; // skip first src element
        }
    }

    for (; idx < src->elements().size(); ++idx) {
        DurationElement *de = src->elements()[idx];
        DurationElement *e = to_duration_element(de->clone());
        e->move_to_dummy();
        dst->add(e);
        if (de->is_chord()) {
            range_->record_read_chord(to_chord(de), to_chord(e));
        }
        if (de->is_tuplet()) {
            Tuplet *st = to_tuplet(de);
            Tuplet *dt = to_tuplet(e);
            append_tuplet(st, dt);
        }
    }
}

//---------------------------------------------------------
//   append
//---------------------------------------------------------

void TrackList::append(EngravingItem *e) {
    if (e->is_duration_element()) {
        duration_ += to_duration_element(e)->ticks();

        bool accumulate_rest = e->is_rest() && !empty() && back()->is_rest();
        Segment *s = accumulate_rest ? to_rest(e)->segment() : 0;

        if (s && !s->score()->is_spanner_start_end(s->tick(), e->track()) && !s->annotations().size()) {
            // akkumulate rests
            Rest *rest = to_rest(back());
            Fraction du = rest->ticks();
            du += to_rest(e)->ticks();
            rest->set_ticks(du);
        } else {
            EngravingItem *element = 0;
            if (e->is_tuplet()) {
                Tuplet *src = to_tuplet(e);
                if (src->generated() && !empty() && back()->is_tuplet()) {
                    Tuplet *b = to_tuplet(back());
                    combine_tuplet(b, src);
                } else {
                    element = e->clone();
                    element->move_to_dummy();
                    Tuplet *dst = to_tuplet(element);
                    append_tuplet(src, dst);
                }
            } else {
                element = e->clone();
                ChordRest *src = to_chord_rest(e);
                Segment *s1 = src->segment();
                for (EngravingItem *ee : s1->annotations()) {
                    bool add_sys_object = ee->system_flag() && !ee->is_linked() && ee->track() == 0 && e->track() == 0;
                    if (add_sys_object || (!ee->system_flag() && ee->track() == e->track())) {
                        range_->annotations_.push_back({s1->tick(), ee->clone()});
                    }
                }
                if (e->is_chord()) {
                    Chord *chord = to_chord(e);

                    bool akkumulate_chord = true;
                    for (Note *n : chord->notes()) {
                        if (!n->tie_back_non_partial() || !n->tie_back()->generated()) {
                            akkumulate_chord = false;
                            break;
                        }
                    }
                    if (akkumulate_chord && !empty() && back()->is_chord() && range_->can_absorb_continuation(to_chord(back()), chord)) {
                        Chord *bc = to_chord(back());
                        const Fraction du = bc->ticks() + chord->ticks();
                        bc->set_ticks(du);

                        // forward ties
                        int idx = 0;
                        for (Note *n : bc->notes()) {
                            n->set_tie_for(chord->notes()[idx]->tie_for());
                            ++idx;
                        }
                        delete element;
                        element = nullptr;
                        range_->record_read_chord(chord, bc);
                    } else {
                        range_->record_read_chord(chord, to_chord(element));
                    }
                }
            }
            if (element) {
                element->set_selected(false);
                std::vector<EngravingItem *>::push_back(element);
            }
        }
    } else {
        EngravingItem *c = e->clone();
        c->move_to_dummy(); // the clone is unattached until it is written back into a segment
        std::vector<EngravingItem *>::push_back(c);
    }
}

//---------------------------------------------------------
//   appendGap
//---------------------------------------------------------

void TrackList::append_gap(const Fraction &du, Score *score) {
    if (du.is_zero()) {
        return;
    }
    EngravingItem *e = empty() ? 0 : back();
    if (e && e->is_rest()) {
        Rest *rest = to_rest(back());
        Fraction dd = rest->ticks();
        dd += du;
        duration_ += du;
        rest->set_ticks(dd);
    } else {
        Rest *rest = Factory::create_rest(score->dummy());
        rest->set_ticks(du);
        std::vector<EngravingItem *>::push_back(rest);
        duration_ += du;
    }
}

//---------------------------------------------------------
//   truncate
//    reduce len of last gap by f
//---------------------------------------------------------

bool TrackList::truncate(const Fraction &f) {
    if (empty()) {
        return true;
    }
    EngravingItem *e = back();
    if (!e->is_rest()) {
        return false;
    }
    Rest *r = to_rest(e);
    if (r->ticks() < f) {
        return false;
    }
    if (r->ticks() == f) {
        this->pop_back();
        delete r;
    } else {
        r->set_ticks(r->ticks() - f);
    }
    duration_ -= f;
    return true;
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void TrackList::read(const Segment *fs, const Segment *es) {
    Fraction tick = fs->tick();

    const Segment *s;
    for (s = fs; s && (s != es); s = s->next1()) {
        if (!s->enabled()) {
            continue;
        }
        EngravingItem *e = s->element(track_);
        if (!e || e->generated()) {
            for (EngravingItem *ee : s->annotations()) {
                if (ee->system_flag() && ee->track() != 0) {
                    // Only process the top system object
                    continue;
                }
                if (ee->track() == track_) {
                    range_->annotations_.push_back({s->tick(), ee->clone()});
                }
            }
            continue;
        }
        if (e->is_measure_repeat()) {
            // TODO: copy previous measure contents?
            MeasureRepeat *rm = to_measure_repeat(e);
            Rest *r = Factory::copy_rest(*rm);
            //! TODO Perhaps there is a bug.
            //! Previously, the element changed its type (because there was a virtual method that returned the type).
            //! This code has been added for compatibility reasons to maintain the same behavior.
            r->hack_to_rest_type();
            r->reset();
            append(r);
            tick += r->ticks();
        } else if (e->is_chord_rest()) {
            DurationElement *de = to_duration_element(e);
            Fraction gap = s->tick() - tick;
            if (de->tuplet()) {
                Tuplet *t = de->top_tuplet();
                s = skip_tuplet(t); // continue with first chord/rest after tuplet
                de = t;
            }

            if (gap.is_not_zero()) {
                append_gap(gap, e->score());
                tick += gap;
            }
            append(de);
            tick += de->ticks();
        } else if (e->is_bar_line()) {
            BarLine *bl = to_bar_line(e);
            if (bl->bar_line_type() != BarLineType::NORMAL) {
                append(e);
            }
        } else {
            append(e);
        }
    }
    const Fraction end_tick = es ? es->tick() : fs->score()->last_measure()->end_tick();
    Fraction gap = end_tick - tick;
    if (gap.is_not_zero()) {
        append_gap(gap, fs->score());
    }

    //
    // connect ties
    //
    size_t n = size();
    for (size_t i = 0; i < n; ++i) {
        EngravingItem *e = at(i);
        if (!e->is_chord()) {
            continue;
        }
        Chord *chord = to_chord(e);
        for (Note *n1 : chord->notes()) {
            Tie *tie = n1->tie_for_non_partial();
            if (!tie) {
                continue;
            }
            for (size_t k = i + 1; k < n; ++k) {
                EngravingItem *ee = at(k);
                if (!ee->is_chord()) {
                    continue;
                }
                Chord *c2 = to_chord(ee);
                bool found = false;
                for (Note *n2 : c2->notes()) {
                    if (n1->pitch() == n2->pitch()) {
                        tie->set_end_note(n2);
                        n2->set_tie_back(tie);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    LOGD("Tied note not found");
                }
                break;
            }
        }
    }
}

//---------------------------------------------------------
//   checkRest
//---------------------------------------------------------

static bool check_rest(Fraction &rest, Measure *&m, const Fraction &d) {
    if (rest.is_zero()) {
        if (m->next_measure()) {
            m = m->next_measure();
            rest = m->ticks();
        } else {
            LOGW("premature end of measure list, rest %d/%d", d.numerator(), d.denominator());
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   writeTuplet
//    measure - current measure
//    rest    - available time in measure
//---------------------------------------------------------

Tuplet *TrackList::write_tuplet(Tuplet *parent, Tuplet *tuplet, Measure *&measure, Fraction &rest, WrittenChordMap *written_chords) const {
    Score *score = measure->score();
    Tuplet *dt = tuplet->clone();
    dt->set_ownership_parent(measure);
    Fraction du = tuplet->ticks();
    if (du > rest) {
        // we must split the tuplet
        dt->set_ticks(du * Fraction(1, 2));
        dt->set_base_len(tuplet->base_len().shift(1));
    }
    if (parent) {
        parent->add(dt);
    }

    for (DurationElement *e : tuplet->elements()) {
        Fraction duration = e->global_ticks();
        Tuplet *tt = dt;
        Fraction ratio = Fraction(1, 1);
        while (tt) {
            ratio *= tt->ratio();
            tt = tt->tuplet();
        }

        bool firstpart = true;
        while (duration > Fraction(0, 1)) {
            if (rest.is_zero()) {
                if (measure->next_measure()) {
                    measure = measure->next_measure();
                    rest = measure->ticks();
                    if (e != tuplet->elements().back()) {
                        // create second part of split tuplet
                        dt = dt->clone();
                        dt->set_generated(true);
                        dt->set_ownership_parent(measure);
                        Tuplet *pt = dt;
                        while (parent) {
                            Tuplet *tt1 = parent->clone();
                            tt1->set_generated(true);
                            tt1->set_ownership_parent(measure);
                            tt1->add(pt);
                            pt = tt1;
                            parent = parent->tuplet();
                        }
                    }
                } else {
                    ASSERT_X(String(u"premature end of measure list in track %1, rest %2/%3")
                                 .arg(track_)
                                 .arg(duration.numerator(), duration.denominator()));
                }
            }
            if (e->is_chord_rest()) {
                Fraction dd = std::min(rest, duration) * ratio;
                std::vector<TDuration> dl = to_duration_list(dd, false);
                for (const TDuration &k : dl) {
                    Segment *segment = measure->undo_get_segment_r(SegmentType::CHORD_REST, measure->ticks() - rest);
                    Fraction gd = k.fraction() / ratio;
                    ChordRest *cr = to_chord_rest(e->clone());
                    if (!firstpart) {
                        cr->remove_markings(true);
                    }
                    cr->set_score(score);
                    cr->set_track(track_);
                    segment->add(cr);
                    cr->set_ticks(k.fraction());
                    cr->set_duration_type(k);
                    rest -= gd;
                    duration -= gd;

                    if (cr->is_chord()) {
                        record_written_chord(written_chords, to_chord(e), to_chord(cr));
                        for (Note *note : to_chord(cr)->notes()) {
                            if (!duration.is_zero() && !note->tie_for()) {
                                Tie *tie = Factory::create_tie(note);
                                tie->set_generated(true);
                                note->add(tie);
                            }
                        }
                    }
                    dt->add(cr);
                    firstpart = false;
                }
            } else if (e->is_tuplet()) {
                Tuplet *tt1 = to_tuplet(e);
                Tuplet *ttt = write_tuplet(dt, tt1, measure, rest, written_chords);
                dt = ttt->tuplet();
                parent = dt->tuplet();
                duration = Fraction();
            }
            firstpart = false;
        }
    }
    return dt;
}

//---------------------------------------------------------
//   write
//    rewrite notes into measure list measure
//---------------------------------------------------------

bool TrackList::write(Score *score, const Fraction &tick, WrittenChordMap *written_chords) const {
    if ((track_ % ::mu::engraving::voices) && size() == 1 && at(0)->is_rest()) { // don’t write rests in voice > 0
        return true;
    }
    Measure *measure = score->tick2measure(tick);
    Measure *m = measure;
    Fraction remains = m->end_tick() - tick;
    Segment *segment = 0;

    for (EngravingItem *e : *this) {
        if (e->is_duration_element()) {
            Fraction duration = to_duration_element(e)->ticks();
            if (!check_rest(remains, m, duration)) { // go to next measure, if necessary
                MScore::set_error(MsError::CORRUPTED_MEASURE);
                return false;
            }
            if (duration > remains && e->is_tuplet()) {
                // experimental: allow tuplet split in the middle
                if (duration != remains * 2) {
                    MScore::set_error(MsError::CANNOT_SPLIT_TUPLET);
                    return false;
                }
            }
            bool firstpart = true;
            while (duration > Fraction(0, 1)) {
                if ((e->is_rest() || e->is_measure_repeat()) && (duration >= remains || e == back()) && (remains == m->ticks())) {
                    //
                    // handle full measure rest
                    //
                    Segment *seg = m->get_segment_r(SegmentType::CHORD_REST, m->ticks() - remains);
                    if ((track_ % ::mu::engraving::voices) == 0) {
                        // write only for voice 1
                        Rest *r = Factory::create_rest(seg, DurationType::V_MEASURE);
                        // ideally we should be using stretchedLen
                        // but this is not valid during rewrite when adding time signatures
                        // since the time signature has not been added yet
                        //Fraction stretchedLen = m->stretchedLen(staff);
                        //r->setTicks(stretchedLen);
                        r->set_ticks(m->ticks());
                        r->set_track(track_);
                        seg->add(r);
                    }
                    duration -= m->ticks();
                    remains.set(0, 1);
                } else if (e->is_chord_rest()) {
                    Fraction du = std::min(remains, duration);
                    std::vector<TDuration> dl = to_duration_list(du, e->is_chord());
                    if (dl.empty()) {
                        MScore::set_error(MsError::CORRUPTED_MEASURE);
                        return false;
                    }
                    for (const TDuration &k : dl) {
                        segment = m->undo_get_segment_r(SegmentType::CHORD_REST, m->ticks() - remains);
                        ChordRest *cr = to_chord_rest(e->clone());
                        if (!firstpart) {
                            cr->remove_markings(true);
                        }
                        cr->set_track(track_);
                        cr->set_score(score);
                        Fraction gd = k.fraction();
                        cr->set_ticks(gd);
                        cr->set_duration_type(k);

                        segment->add(cr);
                        duration -= gd;
                        remains -= gd;

                        if (cr->is_chord()) {
                            record_written_chord(written_chords, to_chord(e), to_chord(cr));

                            TremoloTwoChord *tremolo = to_chord(cr)->tremolo_two_chord();
                            if (!firstpart && tremolo) { // remove partial two-note tremolo
                                if (to_chord(e)->tremolo_two_chord()->chord1() == to_chord(e)) {
                                    tremolo->set_chords(to_chord(cr), nullptr);
                                } else {
                                    tremolo->set_chords(nullptr, to_chord(cr));
                                }
                                to_chord(cr)->set_tremolo_two_chord(nullptr);
                                delete tremolo;
                            }
                            for (Note *note : to_chord(cr)->notes()) {
                                if (!duration.is_zero() && !note->tie_for()) {
                                    Tie *tie = Factory::create_tie(note);
                                    tie->set_generated(true);
                                    note->add(tie);
                                }
                            }
                        }
                    }
                } else if (e->is_tuplet()) {
                    write_tuplet(0, to_tuplet(e), m, remains, written_chords);
                    duration = Fraction();
                }
                firstpart = false;
                if (duration > Fraction(0, 1)) {
                    if (!check_rest(remains, m, duration)) { // go to next measure, if necessary
                        MScore::set_error(MsError::CORRUPTED_MEASURE);
                        return false;
                    }
                }
            }
        } else if (e->is_bar_line()) {
        } else if (e->is_clef()) {
            Segment *seg;
            if (remains == m->ticks() && m->tick() > Fraction(0, 1)) {
                Measure *pm = m->prev_measure();
                seg = pm->get_segment_r(SegmentType::CLEF, pm->ticks());
            } else if (remains != m->ticks()) {
                seg = m->get_segment_r(SegmentType::CLEF, m->ticks() - remains);
            } else {
                seg = m->get_segment_r(SegmentType::HEADER_CLEF, Fraction(0, 1));
            }
            EngravingItem *ne = e->clone();
            ne->set_score(score);
            ne->set_track(track_);
            seg->add(ne);
        } else if (e->is_key_sig()) {
            Segment *seg;
            // No remains -> Next Measure if it does exist
            if ((remains == Fraction(0, 1)) && m->next_measure()) {
                seg = m->next_measure()->get_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
            } else {
                seg = m->get_segment_r(SegmentType::KEY_SIG, Fraction());
            }
            EngravingItem *ne = e->clone();
            ne->set_score(score);
            ne->set_track(track_);
            seg->add(ne);
        } else {
            if (!m) {
                break;
            }
            // add the element in its own segment;
            // but KeySig has to be at start of (current) measure

            Segment *seg = m->get_segment_r(Segment::segment_type(e->type()), m->ticks() - remains);
            EngravingItem *ne = e->clone();
            ne->set_score(score);
            ne->set_track(track_);
            seg->add(ne);
        }
    }

    //
    // connect ties from measure->first() to segment
    //

    for (Segment *s = measure->first(); s; s = s->next1()) {
        EngravingItem *e = s->element(track_);
        if (!e || !e->is_chord()) {
            continue;
        }
        Chord *chord = to_chord(e);
        for (Note *n : chord->notes()) {
            Tie *tie = n->tie_for_non_partial();
            if (!tie) {
                continue;
            }
            Note *nn = search_tie_note(n);
            if (nn) {
                tie->set_end_note(nn);
                nn->set_tie_back(tie);
            } else {
                score->do_undo_remove_element(tie);
            }
        }
        if (s == segment) {
            break;
        }
    }
    return true;
}

//---------------------------------------------------------
//   ScoreRange
//---------------------------------------------------------

ScoreRange::~ScoreRange() {
    muse::delete_all(tracks_);
    delete_note_spanners();
    delete_bar_lines();
    delete_spacers();
    delete_jumps_and_markers();
}

void ScoreRange::record_read_chord(const Chord *src, const Chord *dst) {
    if (!src || !dst) {
        return;
    }
    read_chords_[src] = dst;
    if (anchors_note_line(*src)) {
        line_anchor_chords_.insert(dst);
    }
}

// A generated-tie continuation merged into the chord before it loses its own position and grace notes,
// so a note-owned line on either chord would end up on the wrong note.
bool ScoreRange::can_absorb_continuation(const Chord *range_chord, const Chord *continuation) const {
    return !has_grace_notes_in_links(*continuation) && !anchors_note_line(*continuation) && !line_anchor_chords_.contains(range_chord);
}

bool ScoreRange::NoteEndpointRef::matches(const Note &note) const {
    return note.pitch() == pitch && note.string() == string && note.tpc1() == tpc1 && note.tpc2() == tpc2;
}

bool ScoreRange::make_note_endpoint_ref(Note *note, NoteEndpointRef &out) {
    if (!note || !note->chord() || !note->staff()) return false;
    const auto *chord = note->chord();
    const auto *main = chord->is_grace() ? to_chord(chord->ownership_parent()) : chord;
    if (!main) return false;

    const Chord *primary = main;
    auto range = read_chords_.find(main);
    if (range == read_chords_.end() && main->links()) {
        for (auto *linked : *main->links()) {
            if (!linked || !linked->is_chord()) continue;
            range = read_chords_.find(to_chord(linked));
            if (range != read_chords_.end()) {
                primary = range->first;
                break;
            }
        }
    }

    auto &registry = *note->master_score()->eid_register();
    if (range == read_chords_.end()) {
        out.external_note = registry.enroll(*note);
        return out.external_note.is_valid();
    }

    out.range_chord = range->second;
    out.staff = registry.enroll(*chord->staff());
    out.is_grace = chord->is_grace();
    const Note *primary_note = main == primary ? note : to_note(note->find_linked_in_staff(primary->staff()));
    if (!primary_note) {
        if (!out.is_grace || chord->find_linked_in_staff(primary->staff())) return false;

        // clone_staff2 only copies graces from the primary staff. Save a local
        // grace once even when several lines use it.
        const auto existing = std::ranges::find(local_graces_, chord, &LocalGraceBackup::source);
        out.local_grace = static_cast<size_t>(existing - local_graces_.begin());
        if (existing == local_graces_.end()) {
            auto *backup = chord->clone();
            backup->move_to_dummy();
            local_graces_.push_back({chord, out.range_chord, out.staff, backup});
        }
        primary_note = note;
    } else if (out.is_grace) {
        const auto &graces = primary->grace_notes();
        const auto grace = std::ranges::find(graces, primary_note->chord());
        if (grace == graces.end()) return false;
        out.grace_index = static_cast<size_t>(grace - graces.begin());
    }

    out.pitch = primary_note->pitch();
    out.string = primary_note->string();
    out.tpc1 = primary_note->tpc1();
    out.tpc2 = primary_note->tpc2();
    for (const auto *candidate : primary_note->chord()->notes()) {
        if (candidate == primary_note) return out.staff.is_valid();
        if (out.matches(*candidate)) ++out.note_ordinal;
    }
    return false;
}

void ScoreRange::backup_note_spanners(const Fraction &stick, const Fraction &etick) {
    Score *score = first_->score();
    const std::vector<staff_idx_t> unique_staves = score->unique_staves();
    ankerl::unordered_dense::set<const EngravingObject *> seen;
    auto collect_from_chord = [&](const Chord *chord) {
        for (Note *note : chord->notes()) {
            for (const auto *list : {&note->spanner_for(), &note->spanner_back()}) {
                for (Spanner *sp : *list) {
                    if (!is_note_anchored_line(sp) || seen.contains(sp)) {
                        continue;
                    }
                    NoteSpannerBackup group;
                    for (EngravingObject *link_obj : sp->link_list()) {
                        seen.insert(link_obj);
                        if (!link_obj || !link_obj->is_engraving_item() || !to_engraving_item(link_obj)->is_spanner()) {
                            continue;
                        }
                        Spanner *copy_sp = to_spanner(link_obj);
                        if (!is_note_anchored_line(copy_sp)) {
                            continue;
                        }
                        EngravingItem *start_el = copy_sp->start_element();
                        EngravingItem *end_el = copy_sp->end_element();
                        if (!start_el || !start_el->is_note() || !end_el || !end_el->is_note()) {
                            continue;
                        }
                        Note *start_note = to_note(start_el);
                        Note *end_note = to_note(end_el);
                        NoteSpannerCopy copy;
                        if (!make_note_endpoint_ref(start_note, copy.start) || !make_note_endpoint_ref(end_note, copy.end)) {
                            continue;
                        }
                        copy.spanner = clone_note_spanner(*copy_sp);
                        copy.spanner->move_to_dummy();
                        copy.spanner->set_start_element(nullptr);
                        copy.spanner->set_end_element(nullptr);
                        copy.single_note = (start_note == end_note);
                        group.copies.push_back(copy);
                    }
                    if (!group.copies.empty()) {
                        note_spanners_.push_back(std::move(group));
                    }
                }
            }
        }
    };

    ankerl::unordered_dense::set<const Staff *> visited_staves;
    for (staff_idx_t staff_idx : unique_staves) {
        Staff *primary = score->staff(staff_idx);
        if (!primary) {
            continue;
        }
        for (Staff *st : primary->staff_list()) {
            if (!st || !visited_staves.insert(st).second) {
                continue;
            }
            Score *st_score = st->score();
            Measure *fm = st_score ? st_score->tick2measure(stick) : nullptr;
            const track_idx_t s_track = staff2track(st->idx());
            const track_idx_t e_track = s_track + ::mu::engraving::voices;
            for (const Segment *seg = fm ? fm->first(SegmentType::CHORD_REST) : nullptr; seg && seg->tick() < etick;
                 seg = seg->next1(SegmentType::CHORD_REST)) {
                if (!seg->enabled() || seg->tick() < stick) {
                    continue;
                }
                for (track_idx_t track = s_track; track < e_track; ++track) {
                    const EngravingItem *el = seg->element(track);
                    if (!el || !el->is_chord()) {
                        continue;
                    }
                    const Chord *chord = to_chord(el);
                    for (const Chord *grace : chord->grace_notes()) {
                        collect_from_chord(grace);
                    }
                    collect_from_chord(chord);
                }
            }
        }
    }
}

void ScoreRange::restore_note_spanners(Score *score, const TrackList::WrittenChordMap &written_chords) const {
    EIDRegister *registry = score->master_score()->eid_register();
    ankerl::unordered_dense::set<const Score *> valid_scores;
    valid_scores.insert(score);
    for (const Score *linked_score : score->score_list()) {
        valid_scores.insert(linked_score);
    }

    const auto resolve_staff = [&](SourceHandle handle) -> Staff * {
        auto *staff_obj = registry->item_from_handle(handle);
        if (!staff_obj || !staff_obj->is_staff()) return nullptr;
        auto *staff = to_staff(staff_obj);
        const auto *staff_score = staff->score();
        return staff_score && valid_scores.contains(staff_score) && muse::contains(staff_score->staves(), staff) ? staff : nullptr;
    };

    std::vector<Chord *> restored_graces(local_graces_.size());
    std::vector<size_t> grace_order;
    for (size_t i = 0; i < local_graces_.size(); ++i) grace_order.push_back(i);
    std::ranges::stable_sort(grace_order, {}, [&](size_t i) { return local_graces_[i].grace->grace_index(); });
    for (size_t i : grace_order) {
        const auto &backup = local_graces_[i];
        auto *staff = resolve_staff(backup.staff);
        const auto written = written_chords.find(backup.range_chord);
        if (!staff || written == written_chords.end() || !written->second.first) continue;
        auto *main = to_chord(written->second.first->find_linked_in_staff(staff));
        if (!main) continue;
        auto *grace = backup.grace->clone();
        grace->set_ownership_parent(main);
        grace->set_track(main->track());
        grace->set_grace_index(std::min(backup.grace->grace_index(), main->grace_notes().size()));
        staff->score()->do_undo_add_element(grace);
        restored_graces[i] = grace;
    }

    auto resolve_endpoint = [&](const NoteEndpointRef &ref, bool use_first_part) -> Note * {
        if (ref.range_chord) {
            auto *staff = resolve_staff(ref.staff);
            if (!staff) return nullptr;
            const auto written = written_chords.find(ref.range_chord);
            if (written == written_chords.end()) return nullptr;
            Chord *anchor = (use_first_part || ref.is_grace) ? written->second.first : written->second.last;
            if (ref.local_grace) {
                anchor = restored_graces[*ref.local_grace];
            } else if (anchor && ref.is_grace) {
                anchor = ref.grace_index < anchor->grace_notes().size() ? anchor->grace_notes()[ref.grace_index] : nullptr;
            }
            if (!anchor) return nullptr;
            size_t ordinal = 0;
            for (auto *note : anchor->notes()) {
                if (ref.matches(*note) && ordinal++ == ref.note_ordinal) {
                    return note->staff() == staff ? note : to_note(note->find_linked_in_staff(staff));
                }
            }
            return nullptr;
        }

        EngravingObject *obj = registry->item_from_handle(ref.external_note);
        if (!obj || !obj->is_note()) {
            return nullptr;
        }
        Score *note_score = obj->score();
        if (!note_score || !valid_scores.contains(note_score) || !attached_in_score(obj, note_score, *registry)) {
            return nullptr;
        }
        Note *note = to_note(obj);
        return (note->chord() && note->find_measure()) ? note : nullptr;
    };

    // The originals went with the measures the range was read from (Score::undo_remove_measures).
    for (const NoteSpannerBackup &group : note_spanners_) {
        Spanner *first_restored = nullptr;
        for (const NoteSpannerCopy &copy : group.copies) {
            Note *new_start = resolve_endpoint(copy.start, copy.single_note);
            Note *new_end = resolve_endpoint(copy.end, true);
            if (!new_start || !new_end || new_start->score() != new_end->score() || (!copy.single_note && new_start == new_end) ||
                new_end->tick() < new_start->tick()) {
                continue;
            }
            Score *target_score = new_start->score();
            Spanner *nsp = clone_note_spanner(*copy.spanner);
            nsp->set_note_span(new_start, new_end);
            if (first_restored) {
                target_score->apply_link(nsp, first_restored);
            } else {
                first_restored = nsp;
            }
            target_score->do_undo_add_element(nsp);
        }
    }
}

void ScoreRange::delete_note_spanners() {
    for (const NoteSpannerBackup &group : note_spanners_) {
        for (const NoteSpannerCopy &copy : group.copies) {
            delete copy.spanner;
        }
    }
    note_spanners_.clear();
    for (const auto &backup : local_graces_) delete backup.grace;
    local_graces_.clear();
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void ScoreRange::read(Segment *first, Segment *last, bool read_spanner) {
    first_ = first;
    last_ = last;
    Score *score = first->score();
    std::vector<track_idx_t> sl = score->unique_staves();

    track_idx_t start_track = 0;
    track_idx_t end_track = score->nstaves() * ::mu::engraving::voices;

    spanner_.clear();
    read_chords_.clear();
    line_anchor_chords_.clear();
    delete_note_spanners();

    const Fraction stick = first->tick();
    const Fraction etick = last ? last->tick() : score->last_measure()->end_tick();

    if (read_spanner) {
        for (auto i : first->score()->spanner()) {
            Spanner *s = i.second;
            if (s->system_flag() && s->track() != 0) {
                // Only process the top system object
                continue;
            }

            if (s->tick() >= stick && s->tick() < etick && s->track() >= start_track && s->track() < end_track) {
                Spanner *ns = to_spanner(s->clone());
                ns->move_to_dummy();
                ns->set_start_element(0);
                ns->set_end_element(0);
                ns->set_tick(ns->tick() - stick);
                spanner_.push_back(ns);
            }
        }
    }
    tracks_.reserve(sl.size() * ::mu::engraving::voices);
    for (track_idx_t staff_idx : sl) {
        track_idx_t s_track = staff_idx * ::mu::engraving::voices;
        track_idx_t e_track = s_track + ::mu::engraving::voices;
        for (track_idx_t track = s_track; track < e_track; ++track) {
            TrackList *dl = new TrackList(this);
            dl->set_track(track);
            dl->read(first, last);
            tracks_.push_back(dl);
        }
    }
    if (read_spanner) {
        backup_note_spanners(stick, etick);
    }
    read_chords_.clear();
    line_anchor_chords_.clear();

    // Make sure incoming ties are restored
    Measure *m1 = first->measure();
    for (track_idx_t track = start_track; track < end_track; track++) {
        Chord *start_chord = m1->find_chord(first->tick(), track);
        if (!start_chord) {
            continue;
        }
        for (Note *note : start_chord->notes()) {
            Tie *tie = note->tie_back();
            if (!tie) {
                continue;
            }
            start_ties_.push_back(tie->clone());
            score->do_undo_remove_element(tie);
        }
    }

    backup_bar_lines(first, last);
    backup_breaks(first, last);
    backup_repeats(first, last);
    backup_spacers(first, last);
    backup_jumps_and_markers(first, last);
}

//---------------------------------------------------------
//   restoreVolta
//---------------------------------------------------------

void ScoreRange::restore_volta(Score *score, const Fraction &tick, Volta *v) const {
    bool volta_start_ok = false;
    bool volta_end_ok = false;
    Fraction measure_size;

    if (v->volta_type() == Volta::Type::OPEN) {
        volta_end_ok = true;
    }

    // Check if we should keep this volta
    for (Measure *m = score->tick2measure(tick); m; m = m->next_measure()) {
        if (m->tick() == (v->tick() + tick)) {
            volta_start_ok = true;
            measure_size = m->end_tick() - m->tick();
        }
        if (m->end_tick() == (v->tick2() + tick)) {
            volta_end_ok = true;
            // No need to continue looking for
            break;
        }
        // Last Measure
        if (m->section_break() || (m->next_measure() && (m->next_measure()->first(SegmentType::TIME_SIG)))) {
            break;
        }
    }
    bool should_keep_volta = volta_start_ok && volta_end_ok;

    if (should_keep_volta) {
        // Volta start
        v->set_tick(v->tick() + tick);

        // Review Volta endings with open Voltas
        if (v->volta_type() == Volta::Type::OPEN) {
            Fraction volta_former_ticks = v->ticks();
            Fraction volta_new_ticks = volta_former_ticks;

            if (volta_former_ticks < measure_size) {
                volta_new_ticks = measure_size;
            } else if (volta_former_ticks > measure_size) {
                if ((volta_former_ticks / measure_size).reduced().denominator() != 1) {
                    volta_new_ticks = (std::floor((volta_former_ticks / measure_size).to_double()) + 1) * measure_size;
                }
            }
            if (volta_former_ticks != volta_new_ticks) {
                v->set_ticks(volta_new_ticks);
            }
        }
        score->undo_add_element(v);
    }
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

bool ScoreRange::write(Score *score, const Fraction &tick) const {
    TrackList::WrittenChordMap written_chords;
    for (TrackList *dl : tracks_) {
        track_idx_t track = dl->track();
        if (!dl->write(score, tick, &written_chords)) {
            return false;
        }
        if ((track % ::mu::engraving::voices) == ::mu::engraving::voices - 1) {
            // clone staff if appropriate after all voices have been copied
            staff_idx_t staff_idx = track / ::mu::engraving::voices;
            Staff *ostaff = score->staff(staff_idx);
            const LinkedObjects *linked_staves = ostaff->links();
            if (linked_staves) {
                for (auto le : *linked_staves) {
                    Staff *nstaff = to_staff(le);
                    if (nstaff == ostaff) {
                        continue;
                    }
                    Excerpt::clone_staff2(ostaff, nstaff, tick, (tick + dl->ticks()));
                }
            }
        }
        ++track;
    }
    restore_note_spanners(score, written_chords);
    for (Spanner *s : spanner_) {
        if (s->is_volta()) {
            restore_volta(score, tick, to_volta(s));
            continue;
        }
        s->set_tick(s->tick() + tick);
        if (s->is_slur()) {
            Slur *slur = to_slur(s);
            if (slur->start_cr() && slur->start_cr()->is_grace()) {
                Chord *sc = slur->start_chord();
                size_t idx = sc->grace_index();
                Chord *dc = to_chord(score->find_cr(s->tick(), s->track()));
                s->set_start_element(dc->grace_notes()[idx]);
            }
            if (slur->end_cr() && slur->end_cr()->is_grace()) {
                Chord *sc = slur->end_chord();
                size_t idx = sc->grace_index();
                Chord *dc = to_chord(score->find_cr(s->tick2(), s->track2()));
                s->set_end_element(dc->grace_notes()[idx]);
            }
        }
        if (s->anchor() == Spanner::Anchor::MEASURE) {
            Fraction start_tick = s->tick();
            Measure *start_measure = score->tick2measure_mm(start_tick);
            Fraction start_measure_tick = start_measure->tick();
            Fraction end_tick = s->tick2();
            Measure *end_measure = score->tick2measure_mm(end_tick);
            Fraction end_measure_tick = end_measure->tick();
            if (start_measure_tick != start_tick) {
                s->set_tick(start_measure_tick);
            }
            if (end_measure_tick != end_tick) {
                s->set_tick2(end_measure_tick != start_measure_tick ? end_measure_tick : end_measure->end_tick());
            }
        }
        score->undo_add_element(s);
    }
    for (const Annotation &a : annotations_) {
        Measure *tm = score->tick2measure(a.tick);
        Segment *op = to_segment(a.e->ownership_parent());
        Fraction dest_tick = a.e->is_rehearsal_mark() ? tm->tick() : a.tick; // Ensure reharsal mark can only go at measure start
        Segment *s = tm->undo_get_segment(op->segment_type(), dest_tick);
        if (s) {
            a.e->set_ownership_parent(s);
            score->undo_add_element(a.e);
        }
    }

    // Restore ties to the beginning of the first measure
    for (Tie *tie : start_ties_) {
        Note *end_note = search_tie_note(tie->start_note());
        if (!end_note) {
            delete tie;
            continue;
        }
        tie->set_end_note(end_note);
        score->do_undo_add_element(tie);
    }

    restore_bar_lines(score, tick);
    restore_breaks(score, tick);
    restore_repeats(score, tick);
    restore_spacers(score, tick);
    restore_jumps_and_markers(score, tick);
    return true;
}

//---------------------------------------------------------
//   fill
//---------------------------------------------------------

void ScoreRange::fill(const Fraction &f) {
    const Fraction old_duration = ticks();
    Fraction old_end_tick = first_->tick() + old_duration;
    for (auto t : tracks_) {
        t->append_gap(f, first_->score());
    }

    Fraction diff = ticks() - old_duration;
    for (Spanner *sp : spanner_) {
        if (sp->tick2() >= old_end_tick && sp->tick() < old_end_tick) {
            sp->set_ticks(sp->ticks() + diff);
        }
    }
}

//---------------------------------------------------------
//   truncate
//    reduce len of last gap by f
//---------------------------------------------------------

bool ScoreRange::truncate(const Fraction &f) {
    for (TrackList *dl : tracks_) {
        if (dl->empty()) {
            continue;
        }
        EngravingItem *e = dl->back();
        if (!e->is_rest()) {
            return false;
        }
        Rest *r = to_rest(e);
        if (r->ticks() < f) {
            return false;
        }
    }
    for (TrackList *dl : tracks_) {
        dl->truncate(f);
    }
    return true;
}

//---------------------------------------------------------
//   ticks
//---------------------------------------------------------

Fraction ScoreRange::ticks() const { return tracks_.empty() ? Fraction() : tracks_.front()->ticks(); }

//---------------------------------------------------------
//   backupBarLines
//---------------------------------------------------------
void ScoreRange::backup_bar_lines(Segment *first, Segment *last) {
    for (Segment *s = first; s && (!last || s->is_before(last) || s == last); s = s->next1()) {
        if (!s->is_type(SegmentType::BAR_LINE_TYPES)) {
            continue;
        }
        for (EngravingItem *e : s->elist()) {
            if (e && !e->generated()) {
                BarLinesBackup bl_backup;
                bl_backup.s_position = s->tick();
                bl_backup.former_measure_start_or_end = s->rtick().is_zero() || s->rtick() == s->measure()->ticks();
                bl_backup.bl = to_bar_line(e)->clone();
                bar_lines_.push_back(bl_backup);
            }
        }
    }
}

//---------------------------------------------------------
//   insertBarLine
//---------------------------------------------------------
bool ScoreRange::insert_bar_line(Measure *m, const BarLinesBackup &bar_line) const {
    //---------------------------------------------------------
    //   addBarLine
    //---------------------------------------------------------
    auto add_bar_line = [&](Measure *m, BarLine *bl, SegmentType st, Fraction pos) {
        bool middle = (pos != m->tick()) && (pos != m->end_tick());
        Segment *seg = m->undo_get_segment(st, pos);
        if (seg) {
            BarLineType blt = bl->bar_line_type();
            // get existing bar line if it does exist
            BarLine *nbl = to_bar_line(seg->element(bl->track()));
            if (!nbl) {
                // no suitable bar line: create a new one
                nbl = Factory::create_bar_line(seg);
                nbl->set_ownership_parent(seg);
                nbl->set_track(bl->track());
                // A BL in the middle of a Measure does have SpanStaff to false
                nbl->set_span_staff(middle ? false : bl->span_staff());
                m->score()->add_element(nbl);
            } else {
                // We change BarLineType if necessary to keep END_START repeats if in the middle of a meassure
                if ((nbl->bar_line_type() == BarLineType::END_REPEAT) && (bl->bar_line_type() == BarLineType::START_REPEAT) && middle) {
                    blt = BarLineType::END_START_REPEAT;
                }
            }
            nbl->set_generated(false);
            nbl->set_bar_line_type(blt);
            nbl->set_visible(bl->visible());
            nbl->set_color(bl->color());

            // We check if the same BL type is in every Stave
            bool bl_across_staves = false;
            // if there is only one stave or spanStaff
            if ((m->score()->nstaves() == 1) || bl->span_staff()) {
                bl_across_staves = true;
            }
            // If we are in the last stave
            else if (bl->staff_idx() == (m->score()->nstaves() - 1)) {
                bool same_bl = true;
                // We check if and every previous stave has the same type of BL
                for (size_t i = 0; i < (m->score()->nstaves() - 1); ++i) {
                    BarLine *sbl = to_bar_line(seg->element(staff2track(i)));
                    if (!sbl || (sbl->bar_line_type() != blt)) {
                        same_bl = false;
                        break;
                    }
                }
                bl_across_staves = same_bl;

                if (bl_across_staves && !middle) {
                    // Set Spanstaff to true if not in the middle and there is the same BL across staves
                    for (size_t i = 0; i < (m->score()->nstaves() - 1); ++i) {
                        BarLine *sbl = to_bar_line(seg->element(staff2track(i)));
                        if (sbl) {
                            sbl->set_span_staff(middle ? false : true);
                        }
                    }
                }
            }

            // Adding Set repeats
            if ((pos == m->tick()) && (bl->bar_line_type() == BarLineType::START_REPEAT) && bl_across_staves) {
                m->set_repeat_start(true);
            } else if ((pos == m->end_tick()) && (bl->bar_line_type() == BarLineType::END_REPEAT) && bl_across_staves) {
                m->set_repeat_end(true);
            }
        }
    };

    bool processed = false;

    // if END_START_REPEAT AND at the end of a Measure
    if ((bar_line.s_position == m->end_tick()) && (bar_line.bl->bar_line_type() == BarLineType::END_START_REPEAT)) {
        // Create END_REPEAT and START_REPEAT (and ignore return value)
        bar_line.bl->set_bar_line_type(BarLineType::END_REPEAT);
        insert_bar_line(m, bar_line);
        // Start Repeat into the next measure
        if (m->next_measure()) {
            bar_line.bl->set_bar_line_type(BarLineType::START_REPEAT);
            insert_bar_line(m->next_measure(), bar_line);
        }

        // Restore initial value just in case
        bar_line.bl->set_bar_line_type(BarLineType::END_START_REPEAT);
        processed = true;
    } else {
        // First position
        if (bar_line.s_position == m->tick()) {
            // Just Start Repeat at the left of the Measure
            if (bar_line.bl->bar_line_type() == BarLineType::START_REPEAT) {
                add_bar_line(m, bar_line.bl, SegmentType::START_REPEAT_BAR_LINE, bar_line.s_position);
                processed = true;
            }
        }
        // Last position
        else if (bar_line.s_position == m->end_tick()) {
            // Avoid Start Repeat at the end of the Measure
            if (bar_line.bl->bar_line_type() != BarLineType::START_REPEAT) {
                add_bar_line(m, bar_line.bl, SegmentType::END_BAR_LINE, bar_line.s_position);
                processed = true;
            }
        }
        // Middle
        else {
            // We don't create BL in the Middle
            processed = true;
        }
    }
    return processed;
}

//---------------------------------------------------------
//   restoreBarLines
//---------------------------------------------------------

void ScoreRange::restore_bar_lines(Score *score, const Fraction &tick) const {
    for (const BarLinesBackup &bbl : bar_lines_) {
        for (Measure *m = score->tick2measure(tick); m; m = m->next_measure()) {
            // if inserted within a suitable measure ... to the next barline
            if (((bbl.s_position >= m->tick()) && (bbl.s_position <= m->end_tick())) && (insert_bar_line(m, bbl))) {
                break;
            }
            if (m->tick() > bbl.s_position) {
                break;
            }
        }
    }
}

//---------------------------------------------------------
//   backupBreaks
//---------------------------------------------------------
void ScoreRange::backup_breaks(Segment *first, Segment *last) {
    Measure *fm = first->measure();
    Measure *lm = last ? last->measure() : first->score()->last_measure();
    for (Measure *m = fm; m && m != lm->next_measure(); m = m->next_measure()) {
        if (m->line_break()) {
            BreaksBackup b_backup;
            b_backup.s_position = m->end_tick();
            b_backup.l_break_type = LayoutBreakType::LINE;
            breaks_.push_back(b_backup);
        } else if (m->page_break()) {
            BreaksBackup b_backup;
            b_backup.s_position = m->end_tick();
            b_backup.l_break_type = LayoutBreakType::PAGE;
            breaks_.push_back(b_backup);
        }
    }
}

//---------------------------------------------------------
//   restoreBreaks
//---------------------------------------------------------

void ScoreRange::restore_breaks(Score *score, const Fraction &tick) const {
    // Break list
    for (const BreaksBackup &bb : breaks_) {
        // Look for suitable measure
        for (Measure *m = score->tick2measure(tick); m; m = m->next_measure()) {
            // We keep them as long as they are in the measure after the start tick
            if ((bb.s_position > m->tick()) && (bb.s_position <= m->end_tick())) {
                m->undo_set_break(true, bb.l_break_type);
                break;
            }
            if (m->tick() > bb.s_position) {
                break;
            }
        }
    }
}

void ScoreRange::backup_repeats(Segment *first, Segment *last) {
    Measure *fm = first->measure();
    Measure *lm = last ? last->measure() : first->score()->last_measure();
    for (Measure *m = fm; m && m != lm->next_measure(); m = m->next_measure()) {
        if (m->repeat_start()) {
            StartEndRepeatBackup repeat_backup;
            repeat_backup.s_position = m->tick();
            repeat_backup.is_start_repeat = true;
            start_end_repeats_.push_back(repeat_backup);
        }
        if (m->repeat_end()) {
            StartEndRepeatBackup repeat_backup;
            repeat_backup.s_position = m->end_tick();
            repeat_backup.is_start_repeat = false;
            start_end_repeats_.push_back(repeat_backup);
        }
    }
}

void ScoreRange::restore_repeats(Score *score, const Fraction &tick) const {
    Fraction ref_tick = tick.is_zero() ? tick : tick - Fraction::eps(); // start checking one measure before
    for (const StartEndRepeatBackup &start_end_repeat : start_end_repeats_) {
        for (Measure *m = score->tick2measure(ref_tick); m; m = m->next_measure()) {
            Fraction m_tick = m->tick();
            if (start_end_repeat.is_start_repeat && start_end_repeat.s_position == m_tick) {
                m->undo_change_property(Pid::REPEAT_START, true);
            } else if (!start_end_repeat.is_start_repeat && start_end_repeat.s_position == m->end_tick()) {
                m->undo_change_property(Pid::REPEAT_END, true);
            }
            if (m_tick > start_end_repeat.s_position) {
                break;
            }
        }
    }
}

//---------------------------------------------------------
//   deleteBarLines
//---------------------------------------------------------

void ScoreRange::delete_bar_lines() {
    for (const BarLinesBackup &bbl : bar_lines_) {
        delete bbl.bl;
    }
    bar_lines_.clear();
}

//---------------------------------------------------------
//   backupSpacers
//---------------------------------------------------------

void ScoreRange::backup_spacers(Segment *first, Segment *last) {
    Measure *fm = first->measure();
    Measure *lm = last ? last->measure() : first->score()->last_measure();

    for (Measure *m = fm; m && m != lm->next_measure(); m = m->next_measure()) {
        staff_idx_t n_staves = m->score()->nstaves();

        for (staff_idx_t i = 0; i < n_staves; ++i) {
            if (m->vspacer_up(i)) {
                SpacerBackup sb;
                sb.s = m->vspacer_up(i)->clone();
                sb.s_position = m->tick() + ((m->end_tick() - m->tick()) / 2);
                sb.staff_idx = i;
                spacers_.push_back(sb);
            }
            if (m->vspacer_down(i)) {
                SpacerBackup sb;
                sb.s = m->vspacer_down(i)->clone();
                sb.s_position = m->tick() + ((m->end_tick() - m->tick()) / 2);
                sb.staff_idx = i;
                spacers_.push_back(sb);
            }
        }
    }
}

//---------------------------------------------------------
//   restoreSpacers
//---------------------------------------------------------
void ScoreRange::restore_spacers(Score *score, const Fraction &tick) const {
    //---------------------------------------------------------
    //   addSpacer
    //---------------------------------------------------------
    auto add_spacer = [&](Measure *m, Spacer *s, staff_idx_t staff_idx) {
        // We only add an element if there isn't a previous one of the same Type (UP/DOWN)
        if ((s->spacer_type() == SpacerType::UP && !m->vspacer_up(staff_idx)) ||
            ((s->spacer_type() == SpacerType::DOWN || s->spacer_type() == SpacerType::FIXED) && !m->vspacer_down(staff_idx))) {
            Spacer *ns = Factory::create_spacer(m);
            ns->set_spacer_type(s->spacer_type());
            ns->set_gap(s->gap());
            ns->set_track(staff_idx * ::mu::engraving::voices);
            m->add(ns);
        }
    };

    // Spacers list
    for (const SpacerBackup &sb : spacers_) {
        // Look for suitable measure
        for (Measure *m = score->tick2measure(tick); m; m = m->next_measure()) {
            if (sb.s_position >= m->tick() && sb.s_position <= m->end_tick()) {
                add_spacer(m, sb.s, sb.staff_idx);
                break;
            }
            if (m->tick() > sb.s_position) {
                break;
            }
        }
    }
}

//---------------------------------------------------------
//   deleteSpacers
//---------------------------------------------------------
void ScoreRange::delete_spacers() {
    // Spacers list
    for (const SpacerBackup &sb : spacers_) {
        delete sb.s;
    }
    spacers_.clear();
}

//---------------------------------------------------------
//   endOfMeasureElement
//---------------------------------------------------------

bool ScoreRange::end_of_measure_element(EngravingItem *e) const {
    bool result = false;

    if (e->is_marker() &&
        ((muse::contains(Marker::right_markers, to_marker(e)->marker_type()) || to_marker(e)->marker_type() == MarkerType::FINE))) {
        result = true;
    } else if (e->is_jump()) {
        result = true;
    }
    return result;
}

//---------------------------------------------------------
//   backupJumpsAndMarkers
//---------------------------------------------------------

void ScoreRange::backup_jumps_and_markers(Segment *first, Segment *last) {
    Measure *fm = first->measure();
    Measure *lm = last ? last->measure() : first->score()->last_measure();

    for (Measure *m = fm; m && m != lm->next_measure(); m = m->next_measure()) {
        // Backup Markers and Jumps (Measures's son)
        for (EngravingItem *e : m->el()) {
            if (e && (e->is_marker() || e->is_jump())) {
                JumpsMarkersBackup m_jm_backup;
                m_jm_backup.s_position = (end_of_measure_element(e) ? m->end_tick() : m->tick());
                m_jm_backup.e = e->clone();
                jumps_markers_.push_back(m_jm_backup);
            }
        }
    }
}

//---------------------------------------------------------
//   restoreJumpsAndMarkers
//---------------------------------------------------------
void ScoreRange::restore_jumps_and_markers(Score *score, const Fraction &tick) const {
    //---------------------------------------------------------
    //   addJumpMarker
    //---------------------------------------------------------
    auto add_jump_marker = [&](Measure *m, EngravingItem *e) {
        // We add every element as a Measure could have as many elements (even of the same type) as the users decides
        EngravingItem *ce = e->clone();
        ce->set_ownership_parent(m);
        ce->set_track(0);
        m->add(ce);
    };

    for (const JumpsMarkersBackup &jmb : jumps_markers_) {
        for (Measure *m = score->tick2measure(tick); m; m = m->next_measure()) {
            // Markers: we keep them as long as they are in the measure before the final tick
            // Jumps: we keep them as long as they are in the measure after the start tick
            if ((end_of_measure_element(jmb.e) && jmb.s_position > m->tick() && jmb.s_position <= m->end_tick()) ||
                (!end_of_measure_element(jmb.e) && jmb.s_position >= m->tick() && jmb.s_position < m->end_tick())) {
                add_jump_marker(m, jmb.e);
                LOGI() << "tpacebes hago restore";
            }
            if (m->tick() > jmb.s_position) {
                break;
            }
        }
    }
}

//---------------------------------------------------------
//   deleteJumpsAndMarkers
//---------------------------------------------------------

void ScoreRange::delete_jumps_and_markers() {
    for (const JumpsMarkersBackup &jmb : jumps_markers_) {
        delete jmb.e;
    }
    jumps_markers_.clear();
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void TrackList::dump() const {
    LOGD("elements %zu, duration %d/%d", size(), duration_.numerator(), duration_.denominator());
    for (EngravingItem *e : *this) {
        if (e->is_duration_element()) {
            Fraction du = to_duration_element(e)->ticks();
            LOGD("   %s  %d/%d", e->type_name(), du.numerator(), du.denominator());
        } else {
            LOGD("   %s", e->type_name());
        }
    }
}
} // namespace mu::engraving
