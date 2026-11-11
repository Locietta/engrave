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

#include "exchangevoices.h"

#include "../dom/excerpt.h"
#include "../dom/measure.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/staff.h"
#include "../types/types.h"
#include "clonevoice.h"
#include "log.h"
#include "transaction/undoablecommand.h"

using namespace mu::engraving;

namespace {
struct ExchangeVoicesInMeasure : UndoableCommand {

public:
    ExchangeVoicesInMeasure(Measure *measure, track_idx_t src_track, track_idx_t dst_track, staff_idx_t staff)
        : measure_(measure), src_track_(src_track), dst_track_(dst_track), staff_(staff) {}

    void undo() override { do_exchange(dst_track_, src_track_); }

    void redo() override { do_exchange(src_track_, dst_track_); }

    UNDO_TYPE(CommandType::EXCHANGE_VOICE)
    UNDO_NAME("ExchangeVoice")
    UNDO_CHANGED_OBJECTS({measure_})

private:
    void do_exchange(track_idx_t src_track, track_idx_t dst_track);

    Measure *measure_ = nullptr;
    track_idx_t src_track_ = muse::nidx;
    track_idx_t dst_track_ = muse::nidx;
    staff_idx_t staff_ = muse::nidx;
};

void ExchangeVoicesInMeasure::do_exchange(track_idx_t src_track, track_idx_t dst_track) {
    for (Segment *s = measure_->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
        s->swap_elements(src_track, dst_track);
    }

    Fraction start = measure_->tick();
    Fraction end = start + measure_->ticks();
    auto spanners = measure_->score()->spanner_map().find_overlapping(start.ticks(), end.ticks() - 1);
    for (auto i = spanners.begin(); i < spanners.end(); i++) {
        Spanner *sp = i->value;
        Fraction sp_start = sp->tick();
        Fraction sp_end = sp_start + sp->ticks();
        LOGD("Start %d End %d Diff %d \n Measure Start %d End %d", sp_start.ticks(), sp_end.ticks(), (sp_end - sp_start).ticks(),
             start.ticks(), end.ticks());
        if (sp->is_slur() && (sp_start >= start || sp_end < end)) {
            if (sp->track() == src_track && sp_start >= start) {
                sp->set_track(dst_track);
            } else if (sp->track() == dst_track && sp_start >= start) {
                sp->set_track(src_track);
            }
            if (sp->track2() == src_track && sp_end < end) {
                sp->set_track2(dst_track);
            } else if (sp->track2() == dst_track && sp_end < end) {
                sp->set_track2(src_track);
            }
        }
    }
    measure_->check_multi_voices(staff_); // probably true, but check for invisible notes & rests
}
} // namespace

void ExchangeVoices::exchange_voices_in_selection(Score *score, voice_idx_t src_voice, voice_idx_t dst_voice) {
    if (!score->selection().is_range()) {
        MScore::set_error(MsError::NO_STAFF_SELECTED);
        return;
    }
    Fraction t1 = score->selection().tick_start();
    Fraction t2 = score->selection().tick_end();

    Measure *m1 = score->tick2measure(t1);
    Measure *m2 = score->tick2measure(t2);

    if (score->excerpt()) {
        return;
    }

    if (t2 > m2->tick()) {
        m2 = m2->next_measure();
    }

    for (;;) {
        exchange_voices(score, m1, src_voice, dst_voice, score->selection().staff_start(), score->selection().staff_end());
        m1 = m1->next_measure();
        if ((m1 == 0) || (m2 && (m1->tick() == m2->tick()))) {
            break;
        }
    }
}

void ExchangeVoices::exchange_voices(Score *score, Measure *measure, voice_idx_t src_voice, voice_idx_t dst_voice, staff_idx_t src_staff,
                                     staff_idx_t dst_staff) {
    Fraction tick = measure->tick();

    for (staff_idx_t staff_idx = src_staff; staff_idx < dst_staff; ++staff_idx) {
        track_idx_t src_staff_track = staff_idx * ::mu::engraving::voices;
        track_idx_t src_track = src_staff_track + src_voice;
        track_idx_t dst_track = src_staff_track + dst_voice;
        int track_diff = static_cast<int>(dst_voice - src_voice);

        //handle score and complete measures first
        score->undo(new ExchangeVoicesInMeasure(measure, src_track, dst_track, staff_idx));

        for (Staff *st : score->staff(staff_idx)->staff_list()) {
            track_idx_t staff_track = st->idx() * ::mu::engraving::voices;
            Measure *measure2 = st->score()->tick2measure(tick);
            Excerpt *ex = st->score()->excerpt();

            if (ex) {
                const TracksMap &tracks = ex->tracks_mapping();
                std::vector<track_idx_t> src_track_list = muse::values(tracks, src_track);
                std::vector<track_idx_t> dst_track_list = muse::values(tracks, dst_track);

                for (track_idx_t src_track2 : src_track_list) {
                    // don't care about other linked staves
                    if (!(staff_track <= src_track2) || !(src_track2 < staff_track + ::mu::engraving::voices)) {
                        continue;
                    }

                    track_idx_t temp_track = src_track;
                    std::vector<track_idx_t> test_tracks = muse::values(tracks, temp_track + track_diff);
                    bool has_voice = false;
                    for (track_idx_t test_track : test_tracks) {
                        if (staff_track <= test_track && test_track < staff_track + ::mu::engraving::voices &&
                            muse::contains(dst_track_list, test_track)) {
                            has_voice = true;
                            // voice is simply exchangeable now (deal directly)
                            score->undo(new ExchangeVoicesInMeasure(measure2, src_track2, test_track, staff_track / 4));
                        }
                    }

                    // only source voice is in this staff
                    if (!has_voice) {
                        CloneVoice::clone_voice(measure->first(), measure2->end_tick(), measure2->first(), temp_track, src_track2, false,
                                                true);
                        muse::remove(src_track_list, src_track2);
                    }
                }

                for (track_idx_t dst_track2 : dst_track_list) {
                    // don't care about other linked staves
                    if (!(staff_track <= dst_track2) || !(dst_track2 < staff_track + ::mu::engraving::voices)) {
                        continue;
                    }

                    track_idx_t temp_track = dst_track;
                    std::vector<track_idx_t> test_tracks = muse::values(tracks, temp_track - track_diff);
                    bool has_voice = false;
                    for (track_idx_t test_track : test_tracks) {
                        if (staff_track <= test_track && test_track < staff_track + ::mu::engraving::voices &&
                            muse::contains(src_track_list, test_track)) {
                            has_voice = true;
                        }
                    }

                    // only destination voice is in this staff
                    if (!has_voice) {
                        CloneVoice::clone_voice(measure->first(), measure2->end_tick(), measure2->first(), temp_track, dst_track2, false,
                                                true);
                        muse::remove(dst_track_list, dst_track2);
                    }
                }
            } else if (src_staff_track != staff_track) {
                // linked staff in same score (all voices present can be assumed)
                score->undo(new ExchangeVoicesInMeasure(measure2, staff_track + src_voice, staff_track + dst_voice, st->idx()));
            }
        }
    }

    // make sure voice 0 is complete

    if (src_voice == 0 || dst_voice == 0) {
        for (staff_idx_t staff_idx = src_staff; staff_idx < dst_staff; ++staff_idx) {
            // check for complete timeline of voice 0
            Fraction ctick = measure->tick();
            track_idx_t track = staff_idx * ::mu::engraving::voices;
            for (Segment *s = measure->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                ChordRest *cr = to_chord_rest(s->element(track));
                if (!cr) {
                    continue;
                }
                if (cr->is_rest()) {
                    Rest *r = to_rest(cr);
                    if (r->is_gap()) {
                        r->undo_change_property(Pid::GAP, false);
                    }
                }
                if (ctick < s->tick()) {
                    score->set_rest(ctick, track, s->tick() - ctick, false, 0); // fill gap
                }
                ctick = cr->end_tick();
            }
            Fraction etick = measure->end_tick();
            if (ctick < etick) {
                score->set_rest(ctick, track, etick - ctick, false, 0); // fill gap
            }
        }
    }
}
