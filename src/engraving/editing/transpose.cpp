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

#include "transpose.h"

#include "../dom/factory.h"
#include "../dom/fret.h"
#include "../dom/harmony.h"
#include "../dom/keysig.h"
#include "../dom/linkedobjects.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/part.h"
#include "../dom/segment.h"
#include "../dom/staff.h"
#include "../dom/utils.h"
#include "editkeysig.h"
#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "global/btree.h"
#include "log.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

//    key -   -7(Cb) - +7(C#)

void Transpose::transpose_keys(Transaction &tx, Score *score, staff_idx_t staff_start, staff_idx_t staff_end, const Fraction &ts,
                               const Fraction &tick_end, bool flip) {
    Fraction tick_start(ts);
    if (tick_start < Fraction(0, 1)) { // -1 and 0 are valid values to indicate start of score
        tick_start = Fraction(0, 1);
    }
    for (staff_idx_t staff_idx = staff_start; staff_idx < staff_end; ++staff_idx) {
        Staff *st = score->staff(staff_idx);
        if (st->staff_type(tick_start)->group() == StaffGroup::PERCUSSION) {
            continue;
        }

        for (Segment *s = score->first_segment(SegmentType::KEY_SIG); s; s = s->next1(SegmentType::KEY_SIG)) {
            if (s->tick() < tick_start) {
                continue;
            }
            if (tick_end != Fraction(-1, 1) && s->tick() >= tick_end) {
                break;
            }
            KeySig *ks = to_key_sig(s->element(staff_idx * ::mu::engraving::voices));
            if (!ks || ks->generated()) {
                continue;
            }
            if (!ks->is_atonal()) {
                Interval v = st->part()->instrument(s->tick())->transpose();
                KeySigEvent ke = st->key_sig_event(s->tick());
                PreferSharpFlat pref = ks->part()->prefer_sharp_flat();
                Key n_key = ke.concert_key();
                if (flip && !v.is_zero()) {
                    v.flip();
                    n_key = transpose_key(ke.concert_key(), v, pref);
                }

                ke.set_key(n_key);
                apply_change_key_sig(ks, ke, ks->show_courtesy());
            }
        }
    }
}

void Transpose::transposition_changed(Transaction &tx, Score *score, Part *part, Interval old_v, Fraction tick_start, Fraction tick_end) {
    if (tick_start == Fraction(-1, 1)) {
        tick_start = Fraction(0, 1);
    }

    // transpose keys first
    muse::BTreeSet<Score *> scores;
    for (Staff *ls : part->staff(0)->staff_list()) {
        // TODO: special handling for linked staves within a score
        // could be useful for capo
        Score *ls_score = ls->score();
        if (muse::contains(scores, ls_score)) {
            continue;
        }
        scores.insert(ls_score);
        Part *lp = ls->part();
        if (!ls_score->style().style_b(Sid::CONCERT_PITCH)) {
            const TrackRange track_range = lp->track_range();
            transpose_keys(tx, ls_score, track2staff(track_range.start_track), track2staff(track_range.end_track), tick_start, tick_end,
                           true);
        }
    }

    // now transpose notes and chord symbols
    for (Segment *s = score->first_segment(SegmentType::DURATION); s; s = s->next1(SegmentType::DURATION)) {
        if (s->tick() < tick_start) {
            continue;
        }
        if (tick_end != Fraction(-1, 1) && s->tick() >= tick_end) {
            break;
        }
        Interval v = part->staff(0)->transpose(s->tick());
        v.flip();
        Interval diff_v(old_v.diatonic + v.diatonic, old_v.chromatic + v.chromatic);
        for (Staff *st : part->staves()) {
            if (st->staff_type(tick_start)->group() == StaffGroup::PERCUSSION) {
                continue;
            }
            track_idx_t t1 = st->idx() * ::mu::engraving::voices;
            track_idx_t t2 = t1 + ::mu::engraving::voices;
            for (track_idx_t track = t1; track < t2; ++track) {
                EngravingItem *e = s->element(track);
                if (e && e->is_chord()) {
                    Chord *c = to_chord(e);
                    for (Chord *gc : c->grace_notes()) {
                        for (Note *n : gc->notes()) {
                            int tpc = transpose_tpc(n->tpc1(), v, true);
                            n->undo_change_property(Pid::TPC2, tpc);
                        }
                    }
                    for (Note *n : c->notes()) {
                        int tpc = transpose_tpc(n->tpc1(), v, true);
                        n->undo_change_property(Pid::TPC2, tpc);
                    }
                }
                // find chord symbols
                for (EngravingItem *element : s->annotations()) {
                    if (element->track() != track || !element->is_harmony()) {
                        continue;
                    }
                    Harmony *h = to_harmony(element);
                    for (EngravingObject *score_element : h->link_list()) {
                        if (!score_element->style().style_b(Sid::CONCERT_PITCH)) {
                            undo_transpose_harmony(tx, to_harmony(score_element), diff_v, false);
                        }
                    }
                }
            }
        }
    }
}

Key Transpose::transpose_key(Key key, const Interval &interval, PreferSharpFlat prefer) {
    int key_as_tpc = key2_tpc(key);
    Key new_key = tpc2_key(transpose_tpc(key_as_tpc, interval, false));

    // ignore prefer for octave transposing instruments
    if (interval.chromatic % ::mu::engraving::pitch_delta_octave == 0 && interval.diatonic % ::mu::engraving::step_delta_octave == 0) {
        prefer = PreferSharpFlat::NONE;
    }

    return clamp_key(new_key, prefer);
}

//
// transposes a pitch spelling by a given interval.
// option to enharmonically reduce tpc using double alterations

int Transpose::transpose_tpc(int tpc, Interval interval, bool use_double_sharps_flats) {
    int delta_tpc =
        (interval.chromatic * ::mu::engraving::tpc_delta_semitone) - (interval.diatonic * ::mu::engraving::tpc_delta_enharmonic);
    if (!tpc_is_valid(tpc) || delta_tpc == 0) { // perfect unison & perfect octave
        return tpc;
    }
    return clamp_enharmonic(tpc + delta_tpc, use_double_sharps_flats);
}

namespace {
static void apply_transpose_harmony(Harmony *harmony, Interval interval, bool use_double_sharps_flats) {
    note_change(harmony);

    Transpose::do_undo_transpose_harmony(harmony, interval, use_double_sharps_flats);
}
} // namespace

void Transpose::undo_transpose_harmony(Transaction &tx, Harmony *harmony, Interval interval, bool double_sharp_flat) {
    apply_transpose_harmony(harmony, interval, double_sharp_flat);
}

void Transpose::do_undo_transpose_harmony(Harmony *harmony, Interval interval, bool use_double_sharps_flats) {
    harmony->realized_harmony().set_dirty(true); // harmony should be re-realized after transposition

    for (HarmonyInfo *info : harmony->chords()) {
        info->set_root_tpc(Transpose::transpose_tpc(info->root_tpc(), interval, use_double_sharps_flats));
        info->set_bass_tpc(Transpose::transpose_tpc(info->bass_tpc(), interval, use_double_sharps_flats));
    }

    harmony->set_xml_text(harmony->harmony_name());
    harmony->trigger_layout();
}

namespace {

} // namespace
