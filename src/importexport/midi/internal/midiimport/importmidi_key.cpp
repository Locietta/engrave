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
#include "importmidi_key.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "engraving/dom/factory.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keylist.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/staff.h"
#include "engraving/editing/transpose.h"
#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "importmidi_inner.h"
#include "importmidi_operations.h"

// This simple key detection algorithm is from thesis
// "Inferring Score Level Musical Information From Low-Level Musical Data", 2004
// by Jürgen Kilian

using namespace mu::engraving;

namespace mu::iex::midi { namespace MidiKey {
struct KeyData {
    KeyData(Key key, int count) : key_(key), count_(count) {}

    Key key() const { return key_; }

    bool operator<(const KeyData &second) const {
        // choose key with max sum count of transitions
        if (count_ > second.count_) {
            return true;
        } else if (count_ < second.count_) {
            return false;
        }
        // if equal - prefer key with less accitential count
        return std::abs((int) key_) < std::abs((int) second.key_);
    }

private:
    Key key_;
    int count_;
};

void assign_key_list_to_staff(const KeyList &kl, Staff *staff) {
    Score *score = staff->score();
    const track_idx_t track = staff->idx() * ::mu::engraving::voices;
    Key pkey = Key::C;

    for (auto it = kl.begin(); it != kl.end(); ++it) {
        const int tick = it->first;
        Key key = it->second.key();
        if ((key == Key::C) && (key == pkey)) { // don’t insert unnecessary C key // really?
            continue;
        }
        pkey = key;
        Measure *m = score->tick2measure(Fraction::from_ticks(tick));
        if (!m) {
            continue;
        }
        Segment *seg = m->get_segment(SegmentType::KEY_SIG, Fraction::from_ticks(tick));
        KeySig *ks = Factory::create_key_sig(seg);
        ks->set_track(track);
        ks->set_generated(false);
        ks->set_key(key);
        ks->mutldata()->set_mag(staff->staff_mag(Fraction::from_ticks(tick)));
        seg->add(ks);
    }
}

Key find_key(const std::vector<MTrack> &tracks) {
    const int octave = 12;
    std::vector<int> counts(octave);

    for (const auto &track : tracks) {
        if (track.mtrack->drum_track()) {
            continue;
        }
        for (auto it = track.chords.begin(); it != track.chords.end(); ++it) {
            const auto next = std::next(it);
            if (next == track.chords.end()) {
                continue;
            }

            for (const auto &note1 : it->second.notes) {
                for (const auto &note2 : next->second.notes) {
                    if (std::abs(note1.pitch - note2.pitch) == 1) {
                        ++counts[std::min(note1.pitch, note2.pitch) % octave];
                    }
                }
            }
        }
    }

    std::vector<KeyData> keys = {{Key::C_B, counts[3] + counts[10]}, {Key::G_B, counts[10] + counts[5]}, {Key::D_B, counts[5] + counts[0]},
                                 {Key::A_B, counts[0] + counts[7]},  {Key::E_B, counts[7] + counts[2]},  {Key::B_B, counts[2] + counts[9]},
                                 {Key::F, counts[9] + counts[4]},    {Key::C, counts[4] + counts[11]},   {Key::G, counts[11] + counts[6]},
                                 {Key::D, counts[6] + counts[1]},    {Key::A, counts[1] + counts[8]},    {Key::E, counts[8] + counts[3]},
                                 {Key::B, counts[3] + counts[10]},   {Key::F_S, counts[10] + counts[5]}, {Key::C_S, counts[5] + counts[0]}};

    std::sort(keys.begin(), keys.end());

    return keys[0].key();
}

void recognize_main_key_sig(std::vector<MTrack> &tracks) {
    bool need_to_find_key = false;

    const auto &opers = midi_import_operations;
    const bool is_human = opers.data()->track_opers.is_human_performance.value();
    if (is_human) {
        need_to_find_key = true;
    }

    if (!need_to_find_key) {
        for (const MTrack &track : tracks) {
            if (track.mtrack->drum_track()) {
                continue;
            }
            if (!track.has_key) {
                need_to_find_key = true;
                break;
            }
        }
    }
    if (!need_to_find_key) {
        return;
    }

    const Key key = find_key(tracks);

    for (MTrack &track : tracks) {
        if (track.mtrack->drum_track()) {
            continue;
        }

        if (!track.has_key || is_human) {
            KeySigEvent ke;
            Interval v = track.staff->part()->instrument()->transpose();
            Score *score = track.staff->score();
            ke.set_concert_key(key);
            if (!v.is_zero() && !score->style().style_b(Sid::CONCERT_PITCH)) {
                v.flip();
                ke.set_key(Transpose::transpose_key(key, v));
            }

            KeyList &staff_key_list = *track.staff->key_list();
            staff_key_list[0] = ke;
            assign_key_list_to_staff(staff_key_list, track.staff);
        }
    }
}
}} // namespace mu::iex::midi::MidiKey
