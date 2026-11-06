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
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "io/iodevice.h"

//! NOTE: midievent.h still uses the Qt typedef `uchar`; provide the same
//! type without pulling Qt into the MIDI core. Redefining it is harmless if
//! the typedef is already visible.
using uchar = unsigned char;

#include "../midishared/midievent.h"

namespace mu::iex::midi {
using MidiIODevice = muse::io::IODevice;

struct MidiTrack {
private:
    std::multimap<int, MidiEvent> events_value;
    int out_channel_value;
    int out_port_value;
    bool drum_track_value;

public:
    MidiTrack();
    ~MidiTrack();

    bool empty() const;

    const std::multimap<int, MidiEvent> &events() const { return events_value; }

    std::multimap<int, MidiEvent> &events() { return events_value; }

    int out_channel() const { return out_channel_value; }

    void set_out_channel(int n);

    int out_port() const { return out_port_value; }

    void set_out_port(int n) { out_port_value = n; }

    bool drum_track() const { return drum_track_value; }

    void insert(int tick, const MidiEvent &);
    void merge_note_on_off();
};

//---------------------------------------------------------
//   MidiFile
//---------------------------------------------------------

struct MidiFile {
private:
    MidiIODevice *fp;
    std::vector<MidiTrack> tracks_value;
    int division_value;
    bool is_division_in_tps_value; ///< ticks per second, alternative - ticks per beat
    int format_value;              ///< midi file format (0-2)
    bool no_running_status;        ///< do not use running status on output

    // values used during read()
    int status;      ///< running status
    int sstatus;     ///< running status (not reset after meta or sysex events)
    int click;       ///< current tick position in file
    int64_t cur_pos; ///< current file byte position

    //! Set when a low-level write() fails. The per-event helpers cannot return
    //! a status, so the top-level write() surfaces the error instead.
    bool write_error = false;

    void write_event(const MidiEvent &event);

protected:
    // write
    bool write(const void *, int64_t);
    void write_short(int);
    void write_long(int);
    bool write_track(const MidiTrack &);
    void putvl(unsigned);

    void put(unsigned char c) { write(&c, 1); }

    void write_status(int type, int channel);

    // read
    void read(void *, int64_t);
    int getvl();
    int read_short();
    int read_long();
    bool read_event(MidiEvent *);
    bool read_track();

    void reset_running_status() { status = -1; }

public:
    MidiFile();
    bool read(MidiIODevice *);
    bool write(MidiIODevice *);

    std::vector<MidiTrack> &tracks() { return tracks_value; }

    const std::vector<MidiTrack> &tracks() const { return tracks_value; }

    int format() const { return format_value; }

    void set_format(int fmt) { format_value = fmt; }

    int division() const { return division_value; }

    bool is_division_in_tps() const { return is_division_in_tps_value; }

    void set_division(int val) { division_value = val; }

    void separate_channel();
};
} // namespace mu::iex::midi
