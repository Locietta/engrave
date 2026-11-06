/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "implodeexplode.h"

#include "../dom/chord.h"
#include "../dom/factory.h"
#include "../dom/measure.h"
#include "../dom/mscore.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/staff.h"
#include "../dom/utils.h"
#include "../rw/xmlreader.h"

#include "clonevoice.h"
#include "noteinput.h"
#include "paste.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

bool ImplodeExplode::explode(Score *score) {
    size_t src_staff = score->selection().staff_start();
    size_t last_staff = score->selection().staff_end();
    size_t src_track = src_staff * ::mu::engraving::voices;

    // reset selection to top staff only
    // force complete measures
    Segment *start_segment = score->selection().start_segment();
    Segment *end_segment = score->selection().end_segment();
    Measure *start_measure = start_segment->measure();
    Measure *end_measure = nullptr;
    if (!end_segment) {
        end_measure = score->last_measure();
    } else if (end_segment->tick() == end_segment->measure()->tick()) {
        end_measure = end_segment->measure()->prev_measure() ? end_segment->measure()->prev_measure() : score->first_measure();
    } else {
        end_measure = end_segment->measure();
    }

    Fraction l_tick = end_measure->end_tick();
    bool voice = false;

    for (Measure *m = start_measure; m && m->tick() != l_tick; m = m->next_measure()) {
        if (m->has_voices(src_staff)) {
            voice = true;
            break;
        }
    }
    // force complete measures
    score->deselect_all();
    score->select(start_measure, SelectType::RANGE, src_staff);
    score->select(end_measure, SelectType::RANGE, src_staff);
    start_segment = score->selection().start_segment();
    end_segment = score->selection().end_segment();
    if (!voice) {
        if (src_staff == last_staff - 1) {
            // only one staff was selected up front - determine number of staves
            // loop through all chords looking for maximum number of notes
            int n = 0;
            for (Segment *s = start_segment; s && s != end_segment; s = s->next1()) {
                EngravingItem *e = s->element(src_track);
                if (e && e->is_chord()) {
                    Chord *c = to_chord(e);
                    n = std::max(n, int(c->notes().size()));
                    for (Chord *grace_chord : c->grace_notes()) {
                        n = std::max(n, int(grace_chord->notes().size()));
                    }
                }
            }
            last_staff = std::min(score->nstaves(), src_staff + n);
        }

        // Check that all source and dest measures have the same time stretch - allows explode within a local time signature,
        // but don't yet support it between differing local time signatures.
        Fraction time_stretch(1, 0);
        for (Measure *m = start_measure; m && m->tick() <= end_measure->tick(); m = m->next_measure()) {
            for (size_t staff_idx = src_staff; staff_idx < last_staff; ++staff_idx) {
                Fraction m_time_stretch = score->staff(staff_idx)->time_stretch(m->tick());
                if (!time_stretch.is_valid()) {
                    time_stretch = m_time_stretch;
                } else if (time_stretch != m_time_stretch) {
                    MScore::set_error(MsError::CANNOT_EXPLODE_IMPLODE_LOCAL_TIMESIG);
                    return false;
                }
            }
        }

        const muse::ByteArray mime_data(score->selection().mime_data());
        // copy to all destination staves
        Segment *first_cr_segment = start_measure->tick2segment(start_measure->tick());
        for (size_t i = 1; src_staff + i < last_staff; ++i) {
            track_idx_t track = (src_staff + i) * ::mu::engraving::voices;
            ChordRest *cr = to_chord_rest(first_cr_segment->element(track));
            if (cr) {
                XmlReader e(mime_data);
                Paste::paste_staff(score->transaction_manager()->current_or_dummy_transaction(), score, e, cr->segment(), cr->staff_idx());
            }
        }

        auto do_explode = [score](Chord *c, size_t last_staff, size_t src_staff, size_t i) -> void {
            std::vector<Note *> notes = c->notes();
            size_t nnotes = notes.size();
            // keep note "i" from top, which is backwards from nnotes - 1
            // reuse notes if there are more instruments than notes
            size_t staves_per_note = std::max((last_staff - src_staff) / nnotes, static_cast<size_t>(1));
            size_t keep_index = static_cast<size_t>(std::max(static_cast<int>(nnotes) - 1 - static_cast<int>(i / staves_per_note), 0));
            Note *keep_note = c->notes()[keep_index];
            for (Note *n : notes) {
                if (n != keep_note) {
                    score->undo_remove_element(n);
                }
            }
        };

        // loop through each staff removing all but one note from each chord
        for (size_t i = 0; src_staff + i < last_staff; ++i) {
            track_idx_t track = (src_staff + i) * ::mu::engraving::voices;
            for (Segment *s = start_segment; s && s != end_segment; s = s->next1()) {
                EngravingItem *e = s->element(track);
                if (e && e->is_chord()) {
                    Chord *c = to_chord(e); //chord, laststaff, srcstaff
                    do_explode(c, last_staff, src_staff, i);
                    for (Chord *grace_chord : c->grace_notes()) {
                        do_explode(grace_chord, last_staff, src_staff, i);
                    }
                }
            }
        }
    } else {
        track_idx_t s_tracks[::mu::engraving::voices];
        track_idx_t d_tracks[::mu::engraving::voices];
        if (src_staff == last_staff - 1) {
            last_staff = std::min(score->nstaves(), src_staff + ::mu::engraving::voices);
        }

        for (voice_idx_t i = 0; i < ::mu::engraving::voices; i++) {
            s_tracks[i] = muse::nidx;
            d_tracks[i] = muse::nidx;
        }
        size_t matched_track_count = 0;

        for (Segment *seg = start_segment; seg && seg->tick() < l_tick; seg = seg->next1()) {
            for (track_idx_t candidate_src_track = src_track;
                 candidate_src_track < src_track + ::mu::engraving::voices && matched_track_count != ::mu::engraving::voices;
                 candidate_src_track++) {
                bool is_unclaimed_src_track = true;
                for (voice_idx_t claimed_idx = 0; claimed_idx < ::mu::engraving::voices; claimed_idx++) {
                    if (candidate_src_track == s_tracks[claimed_idx]) {
                        is_unclaimed_src_track = false;
                        break;
                    }
                }

                if (!seg->measure()->has_voice(candidate_src_track) || seg->measure()->is_only_gap_rests(candidate_src_track) ||
                    !is_unclaimed_src_track) {
                    continue;
                }
                s_tracks[matched_track_count] = candidate_src_track;

                for (size_t candidate_dst_track = src_track + matched_track_count * ::mu::engraving::voices;
                     candidate_dst_track < last_staff * ::mu::engraving::voices; candidate_dst_track++) {
                    if (candidate_src_track == candidate_dst_track) {
                        d_tracks[matched_track_count] = candidate_dst_track;
                        break;
                    }
                    for (Measure *m = seg->measure(); m && m->tick() < l_tick; m = m->next_measure()) {
                        if (!m->has_voice(candidate_dst_track) ||
                            (m->has_voice(candidate_dst_track) && m->is_only_rests(candidate_dst_track))) {
                            d_tracks[matched_track_count] = candidate_dst_track;
                        } else {
                            d_tracks[matched_track_count] = muse::nidx;
                            break;
                        }
                    }
                    if (d_tracks[matched_track_count] != muse::nidx) {
                        break;
                    }
                }
                matched_track_count++;
            }
        }

        IF_ASSERT_FAILED(matched_track_count > 0) { return false; }
        last_staff = track2staff(d_tracks[matched_track_count - 1]) + 1;

        // Check that all source and dest measures have the same time stretch - allows explode within a local time signature,
        // but don't yet support it between differing local time signatures.
        Fraction time_stretch(1, 0);
        for (Measure *m = start_measure; m && m->tick() <= end_measure->tick(); m = m->next_measure()) {
            for (size_t staff_idx = src_staff; staff_idx < last_staff; ++staff_idx) {
                Fraction m_time_stretch = score->staff(staff_idx)->time_stretch(m->tick());
                if (!time_stretch.is_valid()) {
                    time_stretch = m_time_stretch;
                } else if (time_stretch != m_time_stretch) {
                    MScore::set_error(MsError::CANNOT_EXPLODE_IMPLODE_LOCAL_TIMESIG);
                    return false;
                }
            }
        }

        for (track_idx_t i = src_track, j = 0; i < last_staff * ::mu::engraving::voices && j < ::mu::engraving::voices;
             i += ::mu::engraving::voices, j++) {
            track_idx_t strack = s_tracks[j % ::mu::engraving::voices];
            track_idx_t dtrack = d_tracks[j % ::mu::engraving::voices];
            if (strack != muse::nidx && strack != dtrack && dtrack != muse::nidx) {
                CloneVoice::clone_voice(start_segment, l_tick, start_segment, strack, dtrack, true, false);
            }
        }
    }

    // select exploded region
    score->deselect_all();
    score->select(start_measure, SelectType::RANGE, src_staff);
    score->select(end_measure, SelectType::RANGE, last_staff - 1);

    return true;
}

bool ImplodeExplode::implode(Score *score) {
    staff_idx_t dst_staff = score->selection().staff_start();
    staff_idx_t end_staff = score->selection().staff_end();
    track_idx_t dst_track = dst_staff * ::mu::engraving::voices;
    track_idx_t start_track = dst_staff * ::mu::engraving::voices;
    track_idx_t end_track = end_staff * ::mu::engraving::voices;
    Segment *start_segment = score->selection().start_segment();
    Segment *end_segment = score->selection().end_segment();
    Measure *start_measure = start_segment->measure();
    Measure *end_measure = end_segment ? end_segment->measure() : score->last_measure();
    Fraction start_tick = start_segment->tick();
    Fraction end_tick = end_segment ? end_segment->tick() : score->last_measure()->end_tick();
    assert(start_measure && end_measure);

    // Check that all source and dest measures have the same time stretch - allows implode within a local time signature,
    // but don't yet support it between differing local time signatures.
    Fraction time_stretch(1, 0);
    for (Measure *m = start_measure; m && m->tick() <= end_measure->tick(); m = m->next_measure()) {
        for (size_t staff_idx = dst_staff; staff_idx < end_staff; ++staff_idx) {
            Fraction m_time_stretch = score->staff(staff_idx)->time_stretch(m->tick());
            if (!time_stretch.is_valid()) {
                time_stretch = m_time_stretch;
            } else if (time_stretch != m_time_stretch) {
                MScore::set_error(MsError::CANNOT_EXPLODE_IMPLODE_LOCAL_TIMESIG);
                return false;
            }
        }
    }

    // if single staff selected, combine voices
    // otherwise combine staves
    if (dst_staff == end_staff - 1) {
        // loop through segments adding notes to chord on top staff
        for (Segment *s = start_segment; s && s != end_segment; s = s->next1()) {
            if (!s->is_chord_rest_type()) {
                continue;
            }
            EngravingItem *dst = s->element(dst_track);
            if (dst && dst->is_chord()) {
                Chord *dst_chord = to_chord(dst);
                // see if we are tying in to this chord
                Chord *tied = 0;
                for (Note *n : dst_chord->notes()) {
                    if (n->tie_back_non_partial()) {
                        tied = n->tie_back()->start_note()->chord();
                        break;
                    }
                }
                // loop through each subsequent staff (or track within staff)
                // looking for notes to add
                for (track_idx_t src_track = start_track + 1; src_track < end_track; src_track++) {
                    EngravingItem *src = s->element(src_track);
                    if (src && src->is_chord()) {
                        Chord *src_chord = to_chord(src);
                        // when combining voices, skip if not same duration
                        if (src_chord->ticks() != dst_chord->ticks()) {
                            continue;
                        }
                        // add notes
                        for (Note *n : src_chord->notes()) {
                            NoteVal nv(n->pitch());
                            nv.tpc1 = n->tpc1();
                            // skip duplicates
                            if (dst_chord->find_note(nv.pitch)) {
                                continue;
                            }
                            bool force_accidental = n->accidental() && n->accidental()->role() == AccidentalRole::USER;
                            Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();
                            Note *nn = NoteInput::add_note(tx, score, dst_chord, nv, force_accidental);
                            // move articulations
                            for (Articulation *artic : src_chord->articulations()) {
                                if (dst_chord->has_articulation(artic)) {
                                    continue;
                                }
                                score->undo_change_parent(artic, dst_chord, dst_chord->staff_idx());
                            }
                            // add tie to this note if original chord was tied
                            if (tied) {
                                // find note to tie to
                                for (Note *tn : tied->notes()) {
                                    if (nn->pitch() == tn->pitch() && nn->tpc() == tn->tpc() && !tn->tie_for()) {
                                        // found note to tie
                                        Tie *tie = Factory::create_tie(tn);
                                        tie->set_start_note(tn);
                                        tie->set_end_note(nn);
                                        tie->set_tick(tie->start_note()->tick());
                                        tie->set_tick2(tie->end_note()->tick());
                                        tie->set_track(tn->track());
                                        score->undo_add_element(tie);
                                    }
                                }
                            }
                        }
                    }
                    // delete chordrest from source track if possible
                    if (src && src->voice()) {
                        score->undo_remove_element(src);
                    }
                }
            }
            // TODO - use first voice that actually has a note and implode remaining voices on it?
            // see https://musescore.org/en/node/174111
            else if (dst) {
                // destination track has something, but it isn't a chord
                // remove rests from other voices if in "voice mode"
                for (voice_idx_t i = 1; i < ::mu::engraving::voices; ++i) {
                    EngravingItem *e = s->element(dst_track + i);
                    if (e && e->is_rest()) {
                        score->undo_remove_element(e);
                    }
                }
            }
        }
        // delete orphaned spanners (TODO: figure out solution to reconnect orphaned spanners to their cloned notes)
        score->check_spanner(start_tick, end_tick);
    } else {
        track_idx_t tracks[::mu::engraving::voices];
        for (voice_idx_t i = 0; i < ::mu::engraving::voices; i++) {
            tracks[i] = muse::nidx;
        }
        voice_idx_t full = 0;

        // identify tracks to combine, storing the source track numbers in tracks[]
        // first four non-empty tracks to win
        for (track_idx_t track = start_track; track < end_track && full < ::mu::engraving::voices; ++track) {
            Measure *m = start_measure;
            do {
                if (m->has_voice(track) && !m->is_only_rests(track)) {
                    tracks[full++] = track;
                    break;
                }
            } while ((m != end_measure) && (m = m->next_measure()));
        }

        // clone source tracks into destination
        for (track_idx_t i = dst_track; i < dst_track + ::mu::engraving::voices; i++) {
            track_idx_t strack = tracks[i % ::mu::engraving::voices];
            if (strack != muse::nidx && strack != i) {
                CloneVoice::clone_voice(start_segment, end_tick, start_segment, strack, i, false, false);
            }
        }
    }

    // select destination staff only
    score->deselect_all();
    score->select(start_measure, SelectType::RANGE, dst_staff);
    score->select(end_measure, SelectType::RANGE, dst_staff);

    return true;
}
