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

#include "editmeasures.h"

#include "../dom/chord.h"
#include "../dom/chordrest.h"
#include "../dom/clef.h"
#include "../dom/engravingitem.h"
#include "../dom/instrchange.h"
#include "../dom/key.h"
#include "../dom/keysig.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/page.h"
#include "../dom/part.h"
#include "../dom/repeatlist.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/spanner.h"
#include "../dom/staff.h"
#include "../dom/stafftypechange.h"
#include "../dom/system.h"
#include "../dom/tie.h"
#include "../dom/utils.h"
#include "dom/masterscore.h"
#include "log.h"

using namespace muse;
using namespace mu::engraving;

//---------------------------------------------------------
//   getCourtesyClefs
//    remember clefs at the end of previous measure
//---------------------------------------------------------

std::vector<Clef *> InsertRemoveMeasures::get_courtesy_clefs(Measure *m) {
    Score *score = m->score();
    std::vector<Clef *> start_clefs;
    if (m->prev() && m->prev()->is_measure()) {
        Measure *prev_measure = to_measure(m->prev());
        const Segment *clef_seg = prev_measure->find_segment_r(SegmentType::CLEF | SegmentType::HEADER_CLEF, prev_measure->ticks());
        if (clef_seg) {
            for (size_t st = 0; st < score->nstaves(); ++st) {
                EngravingItem *clef = clef_seg->element(staff2track(static_cast<int>(st)));
                if (clef && clef->is_clef()) {
                    start_clefs.push_back(to_clef(clef));
                }
            }
        }
    }
    return start_clefs;
}

//---------------------------------------------------------
//   insertMeasures
//---------------------------------------------------------

void InsertRemoveMeasures::insert_measures() {
    Score *score = fm->score();

    std::vector<Clef *> clefs;
    std::vector<Clef *> prev_measure_clefs;
    std::vector<KeySig *> keys;
    Segment *fs = nullptr;
    Segment *ls = nullptr;
    std::vector<RepeatSegmentInfo> old_segments;
    if (fm->is_measure()) {
        // Snapshot before the structural edit below invalidates it
        score->master_score()->update_repeat_list(false);
        old_segments = score->repeat_segment_info_list(/*expandRepeats*/ true);

        score->invalidate_repeat_list();
        fs = to_measure(fm)->first();
        ls = to_measure(lm)->last();
        for (Segment *s = fs; s && s != ls; s = s->next1()) {
            if (!s->enabled() || !(s->segment_type() & (SegmentType::CLEF | SegmentType::HEADER_CLEF | SegmentType::KEY_SIG))) {
                continue;
            }
            for (track_idx_t track = 0; track < score->ntracks(); track += ::mu::engraving::voices) {
                EngravingItem *e = s->element(track);
                if (!e || e->generated()) {
                    continue;
                }
                if (e->is_clef()) {
                    clefs.push_back(to_clef(e));
                } else if (e->is_key_sig()) {
                    keys.push_back(to_key_sig(e));
                }
            }
        }
        prev_measure_clefs = get_courtesy_clefs(to_measure(fm));
    }
    score->measures()->insert(fm, lm);

    if (fm->is_measure()) {
        score->update_ticks_and_time_sig_map();
        score->insert_time(fm->tick(), lm->end_tick() - fm->tick(), old_segments);

        // move ownership of Instrument back to part
        for (Segment *s = fs; s && s != ls; s = s->next1()) {
            for (EngravingItem *e : s->annotations()) {
                if (e->is_instrument_change()) {
                    e->part()->set_instrument(to_instrument_change(e)->instrument(), s->tick());
                }
            }
        }
        for (Clef *clef : prev_measure_clefs) {
            clef->staff()->set_clef(clef);
        }
        for (Clef *clef : clefs) {
            clef->staff()->set_clef(clef);
        }
        for (KeySig *key : keys) {
            key->staff()->set_key(key->segment()->tick(), key->key_sig_event());
        }
    }

    score->set_layout_all();

    // move subsequent StaffTypeChanges
    Fraction tick_start = fm->tick();
    Fraction tick_end = lm->end_tick();
    if (move_stc_value && tick_end > tick_start) {
        for (Staff *staff : score->staves()) {
            // loop backwards until the insert point
            auto st_range = staff->staff_type_range(score->last_measure()->tick());
            int move_tick = st_range.first;

            while (move_tick >= tick_start.ticks() && move_tick > 0) {
                Fraction tick = Fraction::from_ticks(move_tick);
                Fraction tick_new = tick + tick_end - tick_start;

                // measures were inserted already so icon is at differnt place, as staffTypeChange itslef
                Measure *measure = score->tick2measure(tick_new);
                bool st_icon = false;

                staff->move_staff_type(tick, tick_new);

                for (EngravingItem *el : measure->el()) {
                    if (el && el->is_staff_type_change() && el->track() == staff->idx() * ::mu::engraving::voices) {
                        StaffTypeChange *stc = to_staff_type_change(el);
                        stc->set_staff_type(staff->staff_type(tick_new), false);
                        st_icon = true;
                        break;
                    }
                }

                if (!st_icon) {
                    LOGD() << "StaffTypeChange icon is missing in measure " << measure->measure_number();
                }

                st_range = staff->staff_type_range(tick);
                move_tick = st_range.first;
            }
        }
    }

    //
    // connect ties
    //

    if (!fm->is_measure() || !fm->prev_measure()) {
        return;
    }
    Measure *m = fm->prev_measure();
    for (Segment *seg = m->first(); seg; seg = seg->next()) {
        for (track_idx_t track = 0; track < score->ntracks(); ++track) {
            EngravingItem *e = seg->element(track);
            if (!e || !e->is_chord()) {
                continue;
            }
            Chord *chord = to_chord(e);
            for (Note *n : chord->notes()) {
                Tie *tie = n->tie_for_non_partial();
                if (!tie) {
                    continue;
                }
                if (!tie->end_note() || tie->end_note()->chord()->segment()->measure() != m) {
                    Note *nn = search_tie_note(n);
                    if (nn) {
                        tie->set_end_note(nn);
                        nn->set_tie_back(tie);
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------
//   removeMeasures
//---------------------------------------------------------

void InsertRemoveMeasures::remove_measures() {
    Score *score = fm->score();

    Fraction tick1 = fm->tick();
    Fraction tick2 = lm->end_tick();

    // Snapshot before the structural edit below invalidates it
    std::vector<RepeatSegmentInfo> old_segments;
    if (fm->is_measure()) {
        score->master_score()->update_repeat_list(false);
        old_segments = score->repeat_segment_info_list(/*expandRepeats*/ true);
    }

    if (fm->is_measure() && lm->is_measure()) {
        // remove beams from chordrests in affected area, they will be rebuilt later but we need
        // to avoid situations where notes from deleted measures remain in beams
        // when undoing, we need to check the previous measure as well as there could be notes in there
        // that need to have their beams recalculated (esp. when adding time signature)
        MeasureBase *prev = fm->prev();
        Segment *first = to_measure(prev && prev->is_measure() ? prev : fm)->first();
        for (Segment *s = first; s && s != to_measure(lm)->last(); s = s->next1()) {
            if (!s) {
                break;
            }
            if (!s->is_chord_rest_type()) {
                continue;
            }

            for (track_idx_t track = 0; track < score->ntracks(); ++track) {
                EngravingItem *e = s->element(track);
                if (e && e->is_chord_rest()) {
                    to_chord_rest(e)->remove_delete_beam(false);
                }
            }
        }
    }

    std::vector<System *> system_list;
    for (MeasureBase *mb = lm;; mb = mb->prev()) {
        System *system = mb->system();
        if (system) {
            if (!muse::contains(system_list, system)) {
                system_list.push_back(system);
            }
            system->remove_measure(mb);
        }
        if (mb == fm) {
            break;
        }
    }

    // move subsequent StaffTypeChanges
    if (move_stc_value) {
        for (Staff *staff : score->staves()) {
            Fraction tick_start = tick1;
            Fraction tick_end = tick2;

            // loop trhu, until the last one
            auto st_range = staff->staff_type_range(tick_end);
            int move_tick = st_range.first == tick_end.ticks() ? st_range.first : st_range.second;

            while (move_tick != -1) {
                Fraction tick = Fraction::from_ticks(move_tick);
                Fraction new_tick = tick + tick_start - tick_end;

                Measure *measure = score->tick2measure(tick);
                bool st_icon = false;

                staff->move_staff_type(tick, new_tick);

                for (EngravingItem *el : measure->el()) {
                    if (el && el->is_staff_type_change() && el->track() == staff->idx() * ::mu::engraving::voices) {
                        StaffTypeChange *stc = to_staff_type_change(el);
                        stc->set_staff_type(staff->staff_type(new_tick), false);
                        st_icon = true;
                        break;
                    }
                }

                if (!st_icon) {
                    LOGD() << "StaffTypeChange icon is missing in measure " << measure->measure_number();
                }

                st_range = staff->staff_type_range(tick);
                move_tick = st_range.second;
            }
        }
    }

    score->measures()->remove(fm, lm);

    if (fm->is_measure()) {
        score->update_ticks_and_time_sig_map();
        score->invalidate_repeat_list();

        // check if there is a clef at the end of last measure
        // remove clef from staff cleflist

        if (lm->is_measure()) {
            Measure *m = to_measure(lm);
            Segment *s = m->find_segment(SegmentType::CLEF, tick2);
            if (s) {
                for (EngravingItem *e : s->elist()) {
                    Clef *clef = to_clef(e);
                    if (clef) {
                        score->staff(clef->staff_idx())->remove_clef(clef);
                    }
                }
            }
        }

        // remember clefs at the end of previous measure
        const auto clefs = get_courtesy_clefs(to_measure(fm));

        score->insert_time(tick1, -(tick2 - tick1), old_segments);

        // Restore clefs that were backed up. Events for them could be lost
        // as a result of the recent insertTime() call.
        for (Clef *clef : clefs) {
            clef->staff()->set_clef(clef);
        }

        std::set<Spanner *> spanners_copy = score->unmanaged_spanners();
        for (Spanner *sp : spanners_copy) {
            if ((sp->tick() >= tick1 && sp->tick() < tick2) || (sp->tick2() >= tick1 && sp->tick2() < tick2)) {
                sp->remove_unmanaged();
            }
        }
    }

    // remove empty systems

    for (System *s : system_list) {
        if (s->measures().empty()) {
            Page *page = s->page();
            if (page) {
                // erase system from page
                muse::remove(page->systems(), s);
                s->set_page(nullptr);
            }
            // erase system from score
            muse::remove(score->systems(), s);
            // finally delete system
            s->delete_later();

            if (page && page->systems().empty()) {
                // if page is empty, delete it as well
                muse::remove(score->pages(), page);
                page->delete_later();
            }
        }
    }

    score->set_layout_all();
}

//---------------------------------------------------------
//   ChangeMeasureLen
//---------------------------------------------------------

ChangeMeasureLen::ChangeMeasureLen(Measure *m, Fraction l) {
    measure = m;
    len = l;
}

void ChangeMeasureLen::flip() {
    Fraction o_len = measure->ticks();

    //
    // move EndBarLine and TimeSigAnnounce
    // to end of measure:
    //

    for (Segment *s = measure->first(); s; s = s->next()) {
        if (!s->is_end_bar_line_type() && !s->is_time_sig_announce_type()) {
            continue;
        }
        s->set_rtick(len);
        measure->remove(s);
    }
    measure->set_ticks(len);
    measure->score()->update_ticks_and_time_sig_map();
    measure->score()->invalidate_repeat_list();
    len = o_len;
}

//---------------------------------------------------------
//   ChangeMMRest
//---------------------------------------------------------

void ChangeMMRest::flip() {
    Measure *mmr = m->mm_rest();
    m->set_mm_rest(mmrest);
    mmrest = mmr;
}

//---------------------------------------------------------
//   ChangeMeasureRepeatCount
//---------------------------------------------------------

void ChangeMeasureRepeatCount::flip() {
    int old_count = m->measure_repeat_count(staff_idx_value);
    m->set_measure_repeat_count(count, staff_idx_value);
    count = old_count;
}

void ChangeMMRestNext::flip() {
    assert(mmrest_->is_mm_rest());
    MeasureBase *old_next = mmrest_->next();

    mmrest_->set_next(next_);
    next_ = old_next;
}

void ChangeMMRestPrev::flip() {
    assert(mmrest_->is_mm_rest());
    MeasureBase *old_prev = mmrest_->prev();

    mmrest_->set_prev(prev_);
    prev_ = old_prev;
}
