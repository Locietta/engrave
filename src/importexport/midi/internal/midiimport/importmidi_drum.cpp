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
#include "importmidi_drum.h"

#include <vector>

#include <ankerl/unordered_dense.h>
#include <gtl/btree.hpp>

#include "../midishared/midifile.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/score.h"
#include "engraving/dom/staff.h"
#include "engraving/editing/editstaffbrackets.h"
#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_operations.h"
#include "importmidi_timeline.h"

namespace mu::iex::midi { namespace MidiDrum {
#ifndef NDEBUG

bool have_non_zero_voices(const MidiTimeline<MidiChord> &chords) {
    for (const auto &chord_event : chords) {
        if (chord_event.second.voice != 0) {
            return false;
        }
    }
    return true;
}

#endif

void split_chord(MidiTimeline<MidiChord>::iterator &chord_it, const ankerl::unordered_dense::set<int> &notes_to_move,
                 MidiTimeline<MidiChord> &chords) {
    MidiChord &chord = chord_it->second;
    auto &notes = chord.notes;

    if (notes_to_move.size() == notes.size()) { // just move chord to another voice
        chord.voice = 1;
    } else { // split chord
        MidiChord new_chord(chord);
        new_chord.notes.clear();
        new_chord.voice = 1;
        std::vector<MidiNote> updated_old_notes;

        for (int i = 0; i != static_cast<int>(notes.size()); ++i) {
            if (notes_to_move.find(i) != notes_to_move.end()) {
                new_chord.notes.push_back(notes[i]);
            } else {
                updated_old_notes.push_back(notes[i]);
            }
        }
        notes = updated_old_notes;

        assert(!notes.empty());
        assert(!new_chord.notes.empty());

        chord_it = chords.insert({chord_it->first, new_chord});
    }
}

void split_drum_voices(gtl::btree_multimap<int, MTrack> &tracks) {
    for (auto &track : tracks) {
        MTrack &mtrack = track.second;
        auto &chords = mtrack.chords;
        if (chords.empty()) {
            continue;
        }
        const engraving::Drumset *const drumset = mtrack.mtrack->drum_track() ? engraving::sm_drumset : 0;
        if (!drumset) {
            continue;
        }
        // all chords of drum track should have voice == 0
        // because allowedVoices == V_1 (see MidiImportOperations)
        // also, all chords should have different onTime values
#ifndef NDEBUG
        assert(MChord::are_on_time_values_different(chords));
        assert(have_non_zero_voices(chords));
#endif
        for (auto chord_it = chords.begin(); chord_it != chords.end(); ++chord_it) {
            auto &notes = chord_it->second.notes;
            // search for the drumset pitches with voice = 1
            ankerl::unordered_dense::set<int> notes_to_move;
            for (int i = 0; i != static_cast<int>(notes.size()); ++i) {
                const int pitch = notes[i].pitch;
                if (drumset->is_valid(pitch) && drumset->voice(pitch) == 1) {
                    notes_to_move.insert(i);
                }
            }
            if (notes_to_move.empty()) {
                continue;
            }

            split_chord(chord_it, notes_to_move, chords);
        }
    }
}

MTrack &get_new_track(gtl::btree_map<int, MTrack> &new_tracks, const MTrack &drum_track, int pitch) {
    auto new_track_it = new_tracks.find(pitch);
    if (new_track_it == new_tracks.end()) {
        new_track_it = new_tracks.insert({pitch, drum_track}).first;
        MTrack &new_track = new_track_it->second;
        // chords are copied and then cleared -
        // not very efficient way but it's more safe for possible
        // future additions of new fields in MTrack
        new_track.chords.clear();
        new_track.name = engraving::sm_drumset->name(pitch);

        assert(new_track.tuplets.empty());
    }
    return new_track_it->second;
}

gtl::btree_map<int, MTrack> split_drum_track(const MTrack &drum_track) {
    gtl::btree_map<int, MTrack> new_tracks; // <percussion note pitch, track>
    if (drum_track.chords.empty()) {
        return new_tracks;
    }

    for (const auto &chord_event : drum_track.chords) {
        const auto &on_time = chord_event.first;
        const MidiChord &chord = chord_event.second;

        for (const auto &note : chord.notes) {
            MidiChord new_chord(chord);
            new_chord.notes.clear();
            new_chord.notes.push_back(note);
            MTrack &new_track = get_new_track(new_tracks, drum_track, note.pitch);
            new_track.chords.insert({on_time, new_chord});
        }
    }

    return new_tracks;
}

void split_drum_tracks(gtl::btree_multimap<int, MTrack> &tracks) {
    gtl::btree_multimap<int, MTrack> split;
    const auto &opers = midi_import_operations.data()->track_opers;
    for (auto it = tracks.begin(); it != tracks.end();) {
        const int key = it->first;
        auto &track = it->second;
        if (!track.mtrack->drum_track() || track.chords.empty() || !opers.do_staff_split.value(track.index_of_operation)) {
            split.emplace(key, std::move(track));
            ++it;
            continue;
        }
        auto drums = split_drum_track(track);
        ++it;
        while (it != tracks.end() && it->first == key) {
            split.emplace(key, std::move(it->second));
            ++it;
        }
        for (auto drum = drums.rbegin(); drum != drums.rend(); ++drum) {
            split.emplace(key, drum->second);
        }
    }
    tracks.swap(split);
}

void set_bracket(engraving::Staff *&staff, int &counter) {
    if (staff && counter > 1) {
        engraving::Score *score = staff->score();
        engraving::EditStaffBrackets::set_bracket_type(score, staff->idx(), 0, engraving::BracketType::NORMAL);
        engraving::EditStaffBrackets::set_bracket_span(score, staff->idx(), 0, counter);
    }
    if (counter) {
        counter = 0;
    }
    if (staff) {
        staff = nullptr;
    }
}

void set_staff_bracket_for_drums(std::vector<MTrack> &tracks) {
    int counter = 0;
    engraving::Staff *first_drum_staff = nullptr;
    int op_index = -1;

    for (const MTrack &track : tracks) {
        if (track.mtrack->drum_track()) {
            if (op_index != track.index_of_operation) {
                op_index = track.index_of_operation;
                set_bracket(first_drum_staff, counter);
                first_drum_staff = track.staff;
            }
            ++counter;
        } else {
            set_bracket(first_drum_staff, counter);
        }
    }
    set_bracket(first_drum_staff, counter);
}
}} // namespace mu::iex::midi::MidiDrum
