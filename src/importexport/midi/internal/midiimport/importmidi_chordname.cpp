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
#include "importmidi_chordname.h"
#include "importmidi_inner.h"
#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "importmidi_operations.h"
#include "../midishared/midifile.h"

#include <string>
#include <vector>

#include "global/types/string.h"

#include "engraving/dom/masterscore.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/factory.h"

// From XF Format Specifications V 2.01 (January 13, 1999, YAMAHA CORPORATION)

using namespace mu::engraving;

namespace mu::iex::midi { namespace MidiChordName {
int read_first_half(uchar byte) { return (byte >> 4) & 0xf; }

int read_second_half(uchar byte) { return byte & 0xf; }

muse::String read_chord_root(uchar byte) {
    static const std::vector<std::string> inversions = {"bbb", "bb", "b", "", "#", "##", "###"};
    static const std::vector<std::string> notes = {"", "C", "D", "E", "F", "G", "A", "B"};

    muse::String chord_root;

    const size_t note_index = read_second_half(byte);
    if (note_index < notes.size()) {
        chord_root += muse::String::from_utf8(notes[note_index]);
    }

    const size_t inversion_index = read_first_half(byte);
    if (inversion_index < inversions.size()) {
        chord_root += muse::String::from_utf8(inversions[inversion_index]);
    }

    return chord_root;
}

muse::String read_chord_type(uchar chord_type_index) {
    static const std::vector<std::string> chord_types = {
        "",  // Maj
        "6", // Maj6
        "Maj7",
        "Maj7#11", // Maj7(#11)
        "Maj9",    // Maj(9)
        "Maj9",    // Maj7(9)
        "6(9)",    // Maj6(9)
        "aug",
        "m",      // min
        "m6",     // min6
        "m7",     // min7
        "m7b5",   // min7b5
        "m9",     // min(9)
        "m9",     // min7(9)
        "m7(11)", // min7(11)
        "mMaj7",  // minMaj7
        "mMaj9",  // minMaj7(9)
        "dim",     "dim7",
        "7", // 7th
        "7sus4",   "7b5",
        "9",    // 7(9)
        "7#11", // 7(#11)
        "7(13)",
        "7b9",    // 7(b9)
        "7b13",   // 7(b13)
        "7#9",    // 7(#9)
        "Maj7#5", // Maj7aug
        "7#5",    // 7aug
        "",       // 1+8
        "5",      // 1+5
        "sus4",
        "sus2", // 1+2+5
        "N.C."  // cc
    };

    if (chord_type_index < chord_types.size()) {
        return muse::String::from_utf8(chord_types[chord_type_index]);
    }
    return muse::String();
}

muse::String read_chord_name(const MidiEvent &e) {
    if (e.type() != ME_META || e.meta_type() != META_SPECIFIC) {
        return muse::String();
    }
    if (e.len() < 4) {
        return muse::String();
    }

    const uchar *data = e.edata();
    if (data[0] != 0x43 || data[1] != 0x7B || data[2] != 0x01) {
        return muse::String();
    }

    muse::String chord_name;

    if (e.len() >= 4) {
        chord_name += read_chord_root(data[3]);
    }
    if (e.len() >= 5) {
        chord_name += read_chord_type(data[4]);
    }

    if (e.len() >= 6) {
        const muse::String chord_root = read_chord_root(data[5]);
        if (!chord_root.is_empty()) {
            muse::String chord_type;
            if (e.len() >= 7) {
                chord_type = read_chord_type(data[6]);
            }
            if (chord_root + chord_type == chord_name) {
                chord_name += u"/" + chord_root;
            } else {
                chord_name += u"/" + chord_root + chord_type;
            }
        }
    }

    return chord_name;
}

muse::String find_chord_name(const std::vector<MidiNote> &notes, const std::multimap<ReducedFraction, muse::String> &chord_names) {
    for (const MidiNote &note : notes) {
        const auto range = chord_names.equal_range(note.orig_on_time);
        if (range.second == range.first) {
            continue;
        }
        // found chord names (usually only one)
        muse::String chord_name;
        for (auto it = range.first; it != range.second; ++it) {
            if (it != range.first) {
                chord_name += u"\n" + it->second;
            } else {
                chord_name += it->second;
            }
        }
        return chord_name;
    }
    return muse::String();
}

void find_chord_names(const std::multimap<int, MTrack> &tracks) {
    auto &data = *midi_import_operations.data();

    for (const auto &track : tracks) {
        for (const auto &event : track.second.mtrack->events()) {
            const MidiEvent &e = event.second;
            const muse::String chord_name = read_chord_name(e);
            if (!chord_name.is_empty()) {
                const auto time = ReducedFraction::from_ticks(event.first);
                data.chord_names.insert({time, chord_name});
            }
        }
    }
}

// all notes should be already placed to the score

void set_chord_names(std::vector<MTrack> &tracks) {
    const auto &data = *midi_import_operations.data();
    if (data.chord_names.empty() || !data.track_opers.show_chord_names.value()) {
        return;
    }

    // chords here can have on time very different from the original one
    // before quantization, so we look for original on times
    // that are stored in notes
    std::set<ReducedFraction> used_times; // don't use one tick for chord name twice

    for (MTrack &track : tracks) {
        for (const auto &chord : track.chords) {
            if (used_times.find(chord.first) != used_times.end()) {
                continue;
            }

            const MidiChord &c = chord.second;
            const muse::String chord_name = find_chord_name(c.notes, data.chord_names);

            if (chord_name.is_empty()) {
                continue;
            }

            Staff *staff = track.staff;
            Score *score = staff->score();
            const ReducedFraction &on_time = chord.first;
            used_times.insert(on_time);

            Measure *measure = score->tick2measure(on_time.fraction());
            Segment *seg = measure->get_segment(SegmentType::CHORD_REST, on_time.fraction());
            const track_idx_t t = staff->idx() * ::mu::engraving::voices;

            Harmony *h = mu::engraving::Factory::create_harmony(seg);
            h->set_harmony(chord_name);
            h->set_track(t);
            seg->add(h);
        }
    }
}
}} // namespace mu::iex::midi::MidiChordName
