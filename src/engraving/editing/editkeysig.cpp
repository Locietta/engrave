/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "editkeysig.h"

#include "addremoveelement.h"
#include "dom/factory.h"
#include "dom/instrument.h"
#include "dom/keysig.h"
#include "dom/measure.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "log.h"
#include "transaction/transaction.h"
#include "transpose.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   ChangeKeySig
//---------------------------------------------------------

ChangeKeySig::ChangeKeySig(KeySig *k, KeySigEvent new_key_sig, bool sc, bool add_evt_to_staff)
    : keysig(k), ks(new_key_sig), show_courtesy(sc), evt_in_staff(add_evt_to_staff) {}

void ChangeKeySig::flip() {
    Segment *segment = keysig->segment();
    Fraction tick = segment->tick();
    Staff *staff = keysig->staff();

    const bool cur_evt_in_staff = (staff->current_key_tick(tick) == tick);
    KeySigEvent cur_key = keysig->key_sig_event();
    const bool cur_show_courtesy = keysig->show_courtesy();

    keysig->set_key_sig_event(ks);
    keysig->set_show_courtesy(show_courtesy);

    // Add/remove the corresponding key events, if appropriate.
    if (evt_in_staff) {
        staff->set_key(tick, ks); // replace
    } else if (cur_evt_in_staff) {
        staff->remove_key(tick); // if nothing to add instead, just remove.
    }
    // If no keysig event corresponds to the key signature then this keysig
    // is probably generated. Otherwise it is probably added manually.
    // Set segment flags according to this, layout will change it if needed.
    segment->set_enabled(evt_in_staff);
    segment->set_header(!evt_in_staff && segment->rtick() == Fraction(0, 1));

    show_courtesy = cur_show_courtesy;
    ks = cur_key;
    evt_in_staff = cur_evt_in_staff;
    keysig->trigger_layout();
    keysig->score()->set_layout(keysig->staff()->next_key_tick(tick), keysig->staff_idx());
}

//---------------------------------------------------------
//   EditKeySig
//---------------------------------------------------------

void EditKeySig::undo_change_key_sig(Transaction &tx, Score *score, Staff *ostaff, const Fraction &tick, KeySigEvent key) {
    KeySig *lks = 0;
    bool needs_update = false;

    for (Staff *staff : ostaff->staff_list()) {
        if (staff->is_drum_staff(tick)) {
            continue;
        }

        Score *staff_score = staff->score();
        Measure *measure = staff_score->tick2measure(tick);
        if (!measure) {
            LOGW("measure for tick %d not found!", tick.ticks());
            continue;
        }
        Segment *s = measure->undo_get_segment(SegmentType::KEY_SIG, tick);

        staff_idx_t staff_idx = staff->idx();
        track_idx_t track = staff_idx * ::mu::engraving::voices;
        KeySig *ks = to_key_sig(s->element(track));

        Interval interval = staff->part()->instrument(tick)->transpose();
        Interval old_staff_interval = staff->transpose(tick);
        KeySigEvent nkey = key;
        bool concert_pitch = staff_score->style().style_b(Sid::CONCERT_PITCH);

        if (interval.chromatic && !concert_pitch && !nkey.is_atonal()) {
            interval.flip();
            nkey.set_key(Transpose::transpose_key(key.concert_key(), interval, staff->part()->prefer_sharp_flat()));
            interval.flip();
        }

        staff_score->update_instrument_change_transpositions(key, staff, tick);
        if (ks) {
            ks->undo_change_property(Pid::GENERATED, false);
            tx.push(new ChangeKeySig(ks, nkey, ks->show_courtesy()));
        } else {
            KeySig *nks = Factory::create_key_sig(s);
            nks->set_track(track);
            nks->set_key_sig_event(nkey);
            staff_score->do_undo_add_element(nks);
            if (lks) {
                tx.push(new Link(lks, nks));
            } else {
                lks = nks;
            }
        }
        if (interval != staff->transpose(tick) || interval != old_staff_interval) {
            needs_update = true;
        }
    }
    if (needs_update) {
        Fraction tick_end = Fraction::from_ticks(ostaff->key_list()->next_key_tick(tick.ticks()));
        Transpose::transposition_changed(tx, score, ostaff->part(), ostaff->transpose(tick), tick, tick_end);
    }
}
