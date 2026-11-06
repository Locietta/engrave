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
#ifndef IMPORTMIDI_OPERATIONS_H
#define IMPORTMIDI_OPERATIONS_H

#include <cstdint>
#include <string>
#include <vector>

#include "global/types/bytearray.h"
#include "global/types/string.h"

#include "importmidi_inner.h"
#include "importmidi_operation.h"
#include "../midishared/midifile.h"

namespace mu::engraving {
class InstrumentTemplate;
} // namespace mu::engraving

namespace mu::iex::midi {
class ReducedFraction;

namespace MidiCharset {
std::string default_charset();
} // namespace MidiCharset

namespace Quantize {
MidiOperations::QuantValue default_quant_value_from_preferences();
} // namespace Quantize

namespace MidiOperations {
// operation types are in importmidi_operation.h

// to add an operation one need to add code also to:
//   - importmidi_operation.h,
//   - importmidi_opmodel.cpp (2 places),
//   - importmidi_trmodel.cpp (2 places),
// and - other importmidi files where algorithm requires it

template <typename T>
class TrackOp {
public:
    explicit TrackOp(T input_default_value) : operation{{-1, input_default_value}}, can_redefine_default_later_value(true) {}

    const T &value(int track_index) const {
        const auto it = operation.find(track_index);
        if (it == operation.end()) {
            return operation.find(-1)->second;
        }
        return it->second;
    }

    void set_value(int track_index, T value) {
        Q_ASSERT_X(track_index >= 0, "TrackOperation", "Invalid track index");

        if (value != this->value(track_index)) {
            operation[track_index] = value;
        }
    }

    const T &default_value() const { return operation.find(-1)->second; }

    bool can_redefine_default_later() const { return can_redefine_default_later_value; }

    void set_default_value(T value, bool input_can_redefine_default_later = true) {
        Q_ASSERT_X(can_redefine_default_later_value, "TrackOp::setDefaultValue", "Cannot set default value");

        if (!can_redefine_default_later_value) {
            return;
        }
        operation[-1] = value;
        can_redefine_default_later_value = input_can_redefine_default_later;
    }

    void clear() {
        T default_val = default_value();
        operation.clear();
        operation[-1] = default_val;
    }

private:
    // <track index, operation value>
    // if track index == -1 then it's default value (for all tracks)
    std::map<int, T> operation;
    bool can_redefine_default_later_value;
};

template <typename T>
class Op {
public:

    explicit Op(T input_default_value)
        : value_value(input_default_value), value_was_set(false), default_value_value{input_default_value},
          can_redefine_default_later_value(true) {}

    T value() const {
        if (!value_was_set) {
            return default_value_value;
        }
        return value_value;
    }

    void set_value(T value) {
        if (value != this->value()) {
            value_value = value;
            value_was_set = true;
        }
    }

    T default_value() const { return default_value_value; }

    bool can_redefine_default_later() const { return can_redefine_default_later_value; }

    void set_default_value(T value, bool input_can_redefine_default_later = true) {
        Q_ASSERT_X(can_redefine_default_later_value, "Op::setDefaultValue", "Cannot set default value");

        if (!can_redefine_default_later_value) {
            return;
        }
        default_value_value = value;
        can_redefine_default_later_value = input_can_redefine_default_later;
    }

private:
    T value_value;
    bool value_was_set;
    T default_value_value;
    bool can_redefine_default_later_value;
};

// values that can be changed

struct Opers {
    // data that cannot be changed by the user
    TrackOp<int> channel = TrackOp<int>(int());
    TrackOp<std::string> staff_name = TrackOp<std::string>(std::string()); // will be converted to unicode later
    TrackOp<muse::String> midi_instr_name = TrackOp<muse::String>(muse::String());
    TrackOp<std::vector<const engraving::InstrumentTemplate *>> ms_instr_list =
        TrackOp<std::vector<const engraving::InstrumentTemplate *>>(std::vector<const engraving::InstrumentTemplate *>());
    TrackOp<bool> is_drum_track = TrackOp<bool>(false);

    // operations for all tracks
    Op<bool> is_human_performance = Op<bool>(false);
    Op<bool> search_pickup_measure = Op<bool>(true);
    Op<bool> measure_count2x_less = Op<bool>(false);
    Op<bool> show_tempo_text = Op<bool>(true);
    Op<bool> show_chord_names = Op<bool>(true);
    Op<TimeSigNumerator> time_sig_numerator = Op<TimeSigNumerator>(TimeSigNumerator::FOUR);
    Op<TimeSigDenominator> time_sig_denominator = Op<TimeSigDenominator>(TimeSigDenominator::FOUR);

    // operations for individual tracks
    TrackOp<int> track_index_after_reorder = TrackOp<int>(0);
    TrackOp<bool> do_import = TrackOp<bool>(true);
    TrackOp<QuantValue> quant_value = TrackOp<QuantValue>(Quantize::default_quant_value_from_preferences());
    TrackOp<bool> search_tuplets = TrackOp<bool>(true);
    TrackOp<bool> search2plets = TrackOp<bool>(false);
    TrackOp<bool> search3plets = TrackOp<bool>(true);
    TrackOp<bool> search4plets = TrackOp<bool>(true);
    TrackOp<bool> search5plets = TrackOp<bool>(true);
    TrackOp<bool> search7plets = TrackOp<bool>(true);
    TrackOp<bool> search9plets = TrackOp<bool>(true);
    TrackOp<bool> use_dots = TrackOp<bool>(true);
    TrackOp<bool> simplify_durations = TrackOp<bool>(true); // for drum tracks - remove rests and ties
    TrackOp<bool> show_staccato = TrackOp<bool>(true);
    TrackOp<bool> do_staff_split = TrackOp<bool>(false); // for drum tracks - split by voices
    TrackOp<VoiceCount> max_voice_count = TrackOp<VoiceCount>(VoiceCount::V_4);
    TrackOp<bool> change_clef = TrackOp<bool>(true);
    TrackOp<Swing> swing = TrackOp<Swing>(Swing::NONE);
    TrackOp<bool> remove_drum_rests = TrackOp<bool>(true);
    TrackOp<int> lyric_track_index = TrackOp<int>(-1); // empty lyric
    TrackOp<int> ms_instr_index = TrackOp<int>(-1);    // for empty instrument list
};

struct HumanBeatData {
    std::set<ReducedFraction> beat_set;
    // to adapt human beats to a different time sig, if necessary
    int added_first_beats = 0;
    int added_last_beats = 0;
    ReducedFraction first_chord_tick;
    ReducedFraction last_chord_tick;
    ReducedFraction time_sig;
    bool measure_count2x_less = false;
};

struct FileData {
    MidiFile midi_file;
    std::vector<MTrack> tracks;
    int processings_of_opened_file = 0;
    int64_t file_modification_time = 0;
    bool has_tempo_text = false;
    muse::ByteArray h_header_data;
    muse::ByteArray v_header_data;
    int track_count = 0;
    Opers track_opers;
    std::string charset = MidiCharset::default_charset();
    // after the user applies MIDI import operations this value should be set
    // to false.
    // tracks of <tick, lyric fragment> from karaoke files: there can be
    // multiple lyric tracks, and lyric track count != MIDI track count in
    // general.
    std::vector<std::multimap<ReducedFraction, std::string>> lyric_tracks;
    std::multimap<ReducedFraction, muse::String> chord_names;
    HumanBeatData human_beat_data;
};

class Data {
public:
    FileData *data();
    const FileData *data() const;

    void add_new_midi_file(const std::string &file_name);
    int current_track() const;
    void set_midi_file_data(const std::string &file_name, const MidiFile &midi_file);
    void exclude_midi_file(const std::string &file_name);
    bool has_midi_file(const std::string &file_name);
    const MidiFile *midi_file(const std::string &file_name);
    std::vector<std::string> all_midi_files() const;
    void set_operations_file(const std::string &file_name);


private:
    friend class CurrentTrackSetter;
    friend class CurrentMidiFileSetter;

    std::string current_midi_file;
    std::string midi_operations_file;
    int current_track_value = -1;

    std::map<std::string, FileData> data_value; // <file name, tracks data>
};

// scoped setter of current track
class CurrentTrackSetter {
public:
    CurrentTrackSetter(Data &opers, int track) : opers_value(opers) {
        old_value = opers_value.current_track_value;
        opers_value.current_track_value = track;
    }

    ~CurrentTrackSetter() { opers_value.current_track_value = old_value; }

private:
    Data &opers_value;
    int old_value;
    // disallow heap allocation - for stack-only usage
    void *operator new(size_t);           // standard new
    void *operator new(size_t, void *);   // placement new
    void *operator new[](size_t);         // array new
    void *operator new[](size_t, void *); // placement array new
};

// scoped setter of current MIDI file
class CurrentMidiFileSetter {
public:
    CurrentMidiFileSetter(Data &opers, const std::string &file_name) : opers_value(opers) {
        old_value = opers_value.current_midi_file;
        opers_value.current_midi_file = file_name;
    }

    ~CurrentMidiFileSetter() { opers_value.current_midi_file = old_value; }

private:
    Data &opers_value;
    std::string old_value;
    // disallow heap allocation - for stack-only usage
    void *operator new(size_t);           // standard new
    void *operator new(size_t, void *);   // placement new
    void *operator new[](size_t);         // array new
    void *operator new[](size_t, void *); // placement array new
};
} // namespace MidiOperations

extern MidiOperations::Data midi_import_operations;
} // namespace mu::iex::midi

#endif // IMPORTMIDI_OPERATIONS_H
