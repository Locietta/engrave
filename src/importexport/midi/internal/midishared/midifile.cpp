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

#include "midifile.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

#include "containers.h"

#include "log.h"

using namespace mu::engraving;

namespace {
[[noreturn]] void throw_midi_error(const std::string &msg) { throw std::runtime_error(msg); }
} // namespace

namespace mu::iex::midi {
MidiFile::MidiFile() {
    fp = 0;
    division_value = 0;
    is_division_in_tps_value = false;
    format_value = 1;
    no_running_status = false;

    status = 0;
    sstatus = 0;
    click = 0;
    cur_pos = 0;
    write_error = false;
}

//---------------------------------------------------------
//   write
//    returns true on error
//---------------------------------------------------------

bool MidiFile::write(MidiIODevice *out) {
    fp = out;
    write_error = false;
    write("MThd", 4);
    write_long(6);             // header len
    write_short(format_value); // format
    write_short(static_cast<int>(tracks_value.size()));
    write_short(division_value);
    for (const auto &t : tracks_value) {
        if (write_track(t) || write_error) {
            return true;
        }
    }
    return write_error;
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void MidiFile::write_event(const MidiEvent &event) {
    switch (event.type()) {
        case ME_NOTEON:
            write_status(ME_NOTEON, event.channel());
            put(event.pitch());
            put(event.velo());
            break;

        case ME_NOTEOFF:
            write_status(ME_NOTEOFF, event.channel());
            put(event.pitch());
            put(event.velo());
            break;

        case ME_PITCHBEND:
            write_status(ME_PITCHBEND, event.channel());
            put(event.data_a());
            put(event.data_b());
            break;

        case ME_CONTROLLER:
            switch (event.controller()) {
                case CTRL_PROGRAM:
                    write_status(ME_PROGRAM, event.channel());
                    put(event.value() & 0x7f);
                    break;
                case CTRL_PRESS:
                    write_status(ME_AFTERTOUCH, event.channel());
                    put(event.value() & 0x7f);
                    break;
                default:
                    write_status(ME_CONTROLLER, event.channel());
                    put(event.controller());
                    put(event.value() & 0x7f);
                    break;
            }
            break;

        case ME_META:
            put(ME_META);
            put(event.meta_type());
            // Don't null terminate text meta events
            if (event.meta_type() >= 0x1 && event.meta_type() <= 0x14) {
                putvl(event.len() - 1);
                write(event.edata(), event.len() - 1);
            } else {
                putvl(event.len());
                write(event.edata(), event.len());
            }
            reset_running_status(); // really ?!
            break;

        case ME_SYSEX:
            put(ME_SYSEX);
            putvl(event.len() + 1); // including 0xf7
            write(event.edata(), event.len());
            put(ME_ENDSYSEX);
            reset_running_status();
            break;
    }
}

//---------------------------------------------------------
//   writeTrack
//---------------------------------------------------------

bool MidiFile::write_track(const MidiTrack &t) {
    write("MTrk", 4);
    int64_t lenpos = static_cast<int64_t>(fp->pos());
    write_long(0); // dummy len

    status = -1;
    int tick = 0;
    for (const auto &i : t.events()) {
        int ntick = i.first;
        putvl(ntick - tick); // write tick delta
        //
        // if track channel != -1, then use this
        //    channel for all events in this track
        //
        if (t.out_channel() != -1) {
            write_event(i.second);
        }
        tick = ntick;
    }

    //---------------------------------------------------
    //    write "End Of Track" Meta
    //    write Track Len
    //

    putvl(1);
    put(0xff); // Meta
    put(0x2f); // EOT
    putvl(0);  // len 0
    int64_t endpos = static_cast<int64_t>(fp->pos());
    fp->seek(static_cast<size_t>(lenpos));
    write_long(static_cast<int>(endpos - lenpos - 4)); // tracklen
    fp->seek(static_cast<size_t>(endpos));
    return write_error;
}

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

void MidiFile::write_status(int nstat, int c) {
    nstat |= (c & 0xf);
    //
    //  running status; except for Sysex- and Meta Events
    //
    if (no_running_status || (((nstat & 0xf0) != 0xf0) && (nstat != status))) {
        status = nstat;
        put(nstat);
    }
}

//---------------------------------------------------------
//   readMidi
//    return false on error
//---------------------------------------------------------

bool MidiFile::read(MidiIODevice *in) {
    fp = in;
    tracks_value.clear();
    cur_pos = 0;

    // === Read header_chunk = "MThd" + <header_length> + <format> + <n> + <division>
    //
    // "MThd" 4 bytes
    //    the literal string MThd, or in hexadecimal notation: 0x4d546864.
    //    These four characters at the start of the MIDI file
    //    indicate that this is a MIDI file.
    // <header_length> 4 bytes
    //    length of the header chunk (always =6 bytes long - the size of the next
    //    three fields which are considered the header chunk).
    //    Although the header chunk currently always contains 6 bytes of data,
    //    this should not be assumed, this value should always be read and acted upon,
    //    to allow for possible future extension to the standard.
    // <format> 2 bytes
    //    0 = single track file format
    //    1 = multiple track file format
    //    2 = multiple song file format (i.e., a series of type 0 files)
    // <n> 2 bytes
    //    number of track chunks that follow the header chunk
    // <division> 2 bytes
    //    unit of time for delta timing. If the value is positive, then it represents
    //    the units per beat. For example, +96 would mean 96 ticks per beat.
    //    If the value is negative, delta times are in SMPTE compatible units.

    char tmp[4];

    read(tmp, 4);
    int len = read_long();
    if (memcmp(tmp, "MThd", 4) || len < 6) {
        throw_midi_error("bad midifile: MThd expected");
    }

    if (len > 6) {
        throw_midi_error("unsupported MIDI header data size: " + std::to_string(len) + " instead of 6");
    }

    format_value = read_short();
    int ntracks = read_short();

    // ================ Read MIDI division =================
    //
    //                        2 bytes
    //  +-------+---+-------------------+-----------------+
    //  |  bit  |15 | 14              8 | 7             0 |
    //  +-------+---+-------------------------------------+
    //  |       | 0 |       ticks per quarter note        |
    //  | value +---+-------------------------------------+
    //  |       | 1 |  -frames/second   |   ticks/frame   |
    //  +-------+---+-------------------+-----------------+

    unsigned char first_byte = 0;
    read(&first_byte, 1);
    unsigned char second_byte = 0;
    read(&second_byte, 1);
    const int top_bit = (first_byte & 0x80) >> 7;

    if (top_bit == 0) { // ticks per beat
        is_division_in_tps_value = false;
        division_value = (first_byte << 8) | (second_byte & 0xff);
    } else { // ticks per second = fps * ticks per frame
        is_division_in_tps_value = true;
        const int frames_per_second = -static_cast<signed char>(first_byte);
        const int ticks_per_frame = second_byte;
        if (frames_per_second == 29) {
            division_value = static_cast<int>(std::lround(29.97 * ticks_per_frame));
        } else {
            division_value = frames_per_second * ticks_per_frame;
        }
    }

    // =====================================================

    switch (format_value) {
        case 0:
            if (read_track()) {
                return false;
            }
            break;
        case 1:
            for (int i = 0; i < ntracks; i++) {
                if (read_track()) {
                    return false;
                }
            }
            break;
        default:
            throw_midi_error("midi file format " + std::to_string(format_value) + " not implemented");

            // Prevent "unreachable code" warning
            // return false;
    }
    return true;
}

//---------------------------------------------------------
//   readTrack
//    return true on error
//---------------------------------------------------------

bool MidiFile::read_track() {
    char tmp[4];
    read(tmp, 4);
    if (memcmp(tmp, "MTrk", 4)) {
        throw_midi_error("bad midifile: MTrk expected");
    }
    int len = read_long(); // len
    int64_t end_pos = cur_pos + len;
    status = -1;
    sstatus = -1; // running status, will not be reset on meta or sysex
    click = 0;
    tracks_value.push_back(MidiTrack());

    int port = 0;
    tracks_value.back().set_out_port(port);
    tracks_value.back().set_out_channel(-1);

    for (;;) {
        MidiEvent event;
        if (!read_event(&event)) {
            return true;
        }

        // check for end of track:
        if ((event.type() == ME_META) && (event.meta_type() == META_EOT)) {
            break;
        }
        tracks_value.back().insert(click, event);
    }
    if (cur_pos != end_pos) {
        LOGW("bad track len: %lld != %lld, %lld bytes too much\n", static_cast<long long>(end_pos), static_cast<long long>(cur_pos),
             static_cast<long long>(end_pos - cur_pos));
        if (cur_pos < end_pos) {
            LOGW("  skip %lld\n", static_cast<long long>(end_pos - cur_pos));
            fp->seek(static_cast<size_t>(end_pos));
        }
    }
    return false;
}

/*---------------------------------------------------------
 *    read
 *    return false on error
 *---------------------------------------------------------*/

void MidiFile::read(void *p, int64_t len) {
    cur_pos += len;
    const size_t rv = fp->read(static_cast<uint8_t *>(p), static_cast<size_t>(len));
    if (rv != static_cast<size_t>(len)) {
        throw_midi_error("bad midifile: unexpected EOF");
    }
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

bool MidiFile::write(const void *p, int64_t len) {
    const size_t rv = fp->write(static_cast<const uint8_t *>(p), static_cast<size_t>(len));
    if (rv == static_cast<size_t>(len)) {
        return false;
    }
    write_error = true;
    LOGD("write midifile failed: %s", fp->error_string().c_str());
    return true;
}

//---------------------------------------------------------
//   readShort
//---------------------------------------------------------

int MidiFile::read_short() {
    char c;
    int val = 0;
    for (int i = 0; i < 2; ++i) {
        read(&c, 1);
        val <<= 8;
        val += (c & 0xff);
    }
    return val;
}

//---------------------------------------------------------
//   writeShort
//---------------------------------------------------------

void MidiFile::write_short(int i) {
    put(i >> 8);
    put(i);
}

//---------------------------------------------------------
//   readLong
//   writeLong
//---------------------------------------------------------

int MidiFile::read_long() {
    char c;
    int val = 0;
    for (int i = 0; i < 4; ++i) {
        read(&c, 1);
        val <<= 8;
        val += (c & 0xff);
    }
    return val;
}

//---------------------------------------------------------
//   writeLong
//---------------------------------------------------------

void MidiFile::write_long(int i) {
    put(i >> 24);
    put(i >> 16);
    put(i >> 8);
    put(i);
}

/*---------------------------------------------------------
 *    getvl
 *    Read variable-length number (7 bits per byte, MSB first)
 *--------------------------------------------------------*/

int MidiFile::getvl() {
    int l = 0;
    for (int i = 0; i < 16; i++) {
        uchar c;
        read(&c, 1);
        l += (c & 0x7f);
        if (!(c & 0x80)) {
            return l;
        }
        l <<= 7;
    }
    return -1;
}

/*---------------------------------------------------------
 *    putvl
 *    Write variable-length number (7 bits per byte, MSB first)
 *--------------------------------------------------------*/

void MidiFile::putvl(unsigned val) {
    unsigned long buf = val & 0x7f;
    while ((val >>= 7) > 0) {
        buf <<= 8;
        buf |= 0x80;
        buf += (val & 0x7f);
    }
    for (;;) {
        put(buf);
        if (buf & 0x80) {
            buf >>= 8;
        } else {
            break;
        }
    }
}

//---------------------------------------------------------
//   MidiTrack
//---------------------------------------------------------

MidiTrack::MidiTrack() {
    out_channel_value = -1;
    out_port_value = -1;
    drum_track_value = false;
}

MidiTrack::~MidiTrack() {}

//---------------------------------------------------------
//   insert
//---------------------------------------------------------

void MidiTrack::insert(int tick, const MidiEvent &event) { events_value.insert({tick, event}); }

//---------------------------------------------------------
//   readEvent
//    return true on success
//---------------------------------------------------------

bool MidiFile::read_event(MidiEvent *event) {
    uchar me, a, b;

    int nclick = getvl();
    if (nclick == -1) {
        LOGD("readEvent: error 1(getvl)");
        return false;
    }
    click += nclick;
    for (;;) {
        read(&me, 1);
        if (me >= 0xf1 && me <= 0xfe && me != 0xf7) {
            LOGD("MIDI: Unknown Message 0x%02x", me & 0xff);
        } else {
            break;
        }
    }

    int data_len;

    if (me == 0xf0 || me == 0xf7) {
        status = -1; // no running status
        int len = getvl();
        if (len < 1) {
            LOGD("readEvent: error 3");
            return false;
        }
        data_len = len;
        std::vector<unsigned char> data(len + 1);
        read(data.data(), len);
        if (data[len - 1] != 0xf7) {
            LOGD("SYSEX does not end with 0xf7!");
            // more to come?
        } else {
            data_len--; // don't count 0xf7
        }
        event->set_type(ME_SYSEX);
        event->set_e_data(std::move(data));
        event->set_len(data_len);
        return true;
    }

    if (me == ME_META) {
        status = -1; // no running status
        uchar type;
        read(&type, 1);
        data_len = getvl(); // read len
        if (data_len == -1) {
            LOGD("readEvent: error 6");
            return false;
        }
        std::vector<unsigned char> data(data_len + 1);
        if (data_len) {
            read(data.data(), data_len);
        }

        event->set_type(ME_META);
        event->set_meta_type(type);
        event->set_len(data_len);
        event->set_e_data(std::move(data));
        return true;
    }

    if (me & 0x80) { // status byte
        status = me;
        sstatus = status;
        read(&a, 1);
    } else {
        if (status == -1) {
            LOGD("readEvent: no running status, read 0x%02x", me);
            LOGD("sstatus ist 0x%02x", sstatus);
            if (sstatus == -1) {
                return 0;
            }
            status = sstatus;
        }
        a = me;
    }
    int channel = status & 0x0f;
    b = 0;
    switch (status & 0xf0) {
        case ME_NOTEOFF:
        case ME_NOTEON:
        case ME_POLYAFTER:
        case ME_CONTROLLER: // controller
        case ME_PITCHBEND:  // pitch bend
            read(&b, 1);
            break;
    }
    event->set_type(status & 0xf0);
    event->set_channel(channel);
    switch (status & 0xf0) {
        case ME_NOTEOFF:
            event->set_data_a(a & 0x7f);
            event->set_data_b(b & 0x7f);
            break;
        case ME_NOTEON:
            event->set_data_a(a & 0x7f);
            event->set_data_b(b & 0x7f);
            break;
        case ME_POLYAFTER:
            event->set_type(ME_CONTROLLER);
            event->set_controller(CTRL_POLYAFTER);
            event->set_value(((a & 0x7f) << 8) + (b & 0x7f));
            break;
        case ME_CONTROLLER: // controller
            event->set_controller(a & 0x7f);
            event->set_value(b & 0x7f);
            break;
        case ME_PITCHBEND: // pitch bend
            event->set_data_a(a & 0x7f);
            event->set_data_b(b & 0x7f);
            break;
        case ME_PROGRAM: event->set_value(a & 0x7f); break;
        case ME_AFTERTOUCH:
            event->set_type(ME_CONTROLLER);
            event->set_controller(CTRL_PRESS);
            event->set_value(a & 0x7f);
            break;
        default: // f1 f2 f3 f4 f5 f6 f7 f8 f9
            LOGD("BAD STATUS 0x%02x, me 0x%02x", status, me);
            return false;
    }

    if ((a & 0x80) || (b & 0x80)) {
        LOGD("8't bit in data set(%02x %02x): tick %d read 0x%02x  status:0x%02x", a & 0xff, b & 0xff, click, me, status);
        LOGD("readEvent: error 16");
        if (b & 0x80) {
            // Try to fix: interpret as channel byte
            status = b;
            sstatus = status;
            return true;
        }
        return false;
    }
    return true;
}

//---------------------------------------------------------
//   setOutChannel
//---------------------------------------------------------

void MidiTrack::set_out_channel(int n) {
    out_channel_value = n;
    if (out_channel_value == 9) {
        drum_track_value = true;
    }
}

//---------------------------------------------------------
//   mergeNoteOnOffAndFindMidiType
//    - find matching note on / note off events and merge
//      into a note event with tick duration
//---------------------------------------------------------

void MidiTrack::merge_note_on_off() {
    std::multimap<int, MidiEvent> el;

    int hbank = 0xff;
    int lbank = 0xff;
    int rpnh = -1;
    int rpnl = -1;
    int datah = 0;
    int datal = 0;
    int data_type = 0; // 0 : disabled, 0x20000 : rpn, 0x30000 : nrpn;

    for (auto i = events_value.begin(); i != events_value.end(); ++i) {
        MidiEvent &ev = i->second;
        if (ev.type() == ME_INVALID) {
            continue;
        }
        if ((ev.type() != ME_NOTEON) && (ev.type() != ME_NOTEOFF)) {
            if (ev.type() == ME_CONTROLLER) {
                int val = ev.value();
                int cn = ev.controller();
                if (cn == CTRL_HBANK) {
                    hbank = val;
                    ev.set_type(ME_INVALID);
                    continue;
                } else if (cn == CTRL_LBANK) {
                    lbank = val;
                    ev.set_type(ME_INVALID);
                    continue;
                } else if (cn == CTRL_HDATA) {
                    datah = val;

                    // check if a CTRL_LDATA follows
                    // e.g. wie have a 14 bit controller:
                    auto ii = i;
                    ++ii;
                    bool found = false;
                    for (; ii != events_value.end(); ++ii) {
                        MidiEvent &ev1 = ii->second;
                        if (ev1.type() == ME_CONTROLLER) {
                            if (ev1.controller() == CTRL_LDATA) {
                                // handle later
                                found = true;
                            }
                            break;
                        }
                    }
                    if (!found) {
                        if (rpnh == -1 || rpnl == -1) {
                            LOGD("parameter number not defined, data 0x%x", datah);
                            ev.set_type(ME_INVALID);
                            continue;
                        } else {
                            ev.set_controller(data_type | (rpnh << 8) | rpnl);
                            ev.set_value(datah);
                        }
                    } else {
                        ev.set_type(ME_INVALID);
                        continue;
                    }
                } else if (cn == CTRL_LDATA) {
                    datal = val;

                    if (rpnh == -1 || rpnl == -1) {
                        LOGD("parameter number not defined, data 0x%x 0x%x, tick %d, channel %d", datah, datal, i->first, ev.channel());
                        continue;
                    }
                    // assume that the sequence is always
                    //    CTRL_HDATA - CTRL_LDATA
                    // eg. that LDATA is always send last

                    // 14 Bit RPN/NRPN
                    ev.set_controller((data_type + 0x30000) | (rpnh << 8) | rpnl);
                    ev.set_value((datah << 7) | datal);
                } else if (cn == CTRL_HRPN) {
                    rpnh = val;
                    data_type = 0x20000;
                    ev.set_type(ME_INVALID);
                    continue;
                } else if (cn == CTRL_LRPN) {
                    rpnl = val;
                    data_type = 0x20000;
                    ev.set_type(ME_INVALID);
                    continue;
                } else if (cn == CTRL_HNRPN) {
                    rpnh = val;
                    data_type = 0x30000;
                    ev.set_type(ME_INVALID);
                    continue;
                } else if (cn == CTRL_LNRPN) {
                    rpnl = val;
                    data_type = 0x30000;
                    ev.set_type(ME_INVALID);
                    continue;
                } else if (cn == CTRL_PROGRAM) {
                    ev.set_value((hbank << 16) | (lbank << 8) | ev.value());
                    // TODO el.insert(ev);
                    ev.set_type(ME_INVALID);
                    continue;
                }
            } else if (ev.type() == ME_SYSEX) {
                int len = ev.len();
                const uchar *buffer = ev.edata();
                // Yamaha
                if (buffer[0] == 0x43) {
                    int type = buffer[1] & 0xf0;
                    if (type == 0x10) {
                        if (len == 7 && buffer[2] == 0x4c && buffer[3] == 0x08 && buffer[5] == 7) {
                            // part mode
                            // 0 - normal
                            // 1 - DRUM
                            // 2 - DRUM 1
                            // 3 - DRUM 2
                            // 4 - DRUM 3
                            // 5 - DRUM 4
                            if (buffer[6] != 0 && buffer[4] == ev.channel()) {
                                drum_track_value = true;
                            }
                            ev.set_type(ME_INVALID);
                            continue;
                        }
                    }
                }
            }
            el.insert(std::pair<int, MidiEvent>(i->first, ev));
            ev.set_type(ME_INVALID);
            continue;
        }
        int tick = i->first;
        if (ev.type() == ME_NOTEOFF || ev.velo() == 0) {
            LOGD("-extra note off at %d", tick);
            ev.set_type(ME_INVALID);
            continue;
        }
        MidiEvent note(ME_NOTE, ev.channel(), ev.data_a(), ev.data_b());

        auto k = i;
        ++k;
        for (; k != events_value.end(); ++k) {
            MidiEvent &e = k->second;
            if (e.type() != ME_NOTEON && e.type() != ME_NOTEOFF) {
                continue;
            }
            if ((e.type() == ME_NOTEOFF || (e.type() == ME_NOTEON && e.velo() == 0)) && (e.pitch() == note.pitch())) {
                int t = k->first - tick;
                if (t <= 0) {
                    t = 1;
                }
                note.set_len(t);
                e.set_type(ME_INVALID);
                break;
            }
        }
        if (k == events_value.end()) {
            LOGD("-no note-off for note at %d", tick);
            note.set_len(1);
        }
        el.insert(std::pair<int, MidiEvent>(tick, note));
        ev.set_type(ME_INVALID);
    }
    events_value = el;
}

//---------------------------------------------------------
//   separateChannel
//    if a track contains events for different midi channels,
//    then split events into separate tracks
//---------------------------------------------------------

void MidiFile::separate_channel() {
    for (size_t i = 0; i < tracks_value.size(); ++i) {
        // create a list of channels used in current track
        std::vector<int> channel;
        MidiTrack &midi_track = tracks_value[i]; // current track
        for (const auto &ie : midi_track.events()) {
            const MidiEvent &e = ie.second;
            if (e.is_channel_event() && !muse::contains(channel, static_cast<int>(e.channel()))) {
                channel.push_back(e.channel());
            }
        }
        midi_track.set_out_channel(channel.empty() ? 0 : channel[0]);
        size_t nn = channel.size();
        if (nn <= 1) {
            continue;
        }
        std::sort(channel.begin(), channel.end());
        // -- split --
        // insert additional tracks, assign to them found channels
        for (size_t ii = 1; ii < nn; ++ii) {
            MidiTrack t;
            t.set_out_channel(channel[ii]);
            tracks_value.insert(tracks_value.begin() + i + ii, t);
        }

        //! NOTE: Midi track memory area may be invalid after inserting new elements into tracks
        //!       Let's get the actual track data again
        MidiTrack &actual_midi_track = tracks_value[i];

        // extract all different channel events from current track to inserted tracks
        for (auto ie = actual_midi_track.events().begin(); ie != actual_midi_track.events().end();) {
            const MidiEvent &e = ie->second;
            if (e.is_channel_event()) {
                int ch = e.channel();
                size_t idx = muse::index_of(channel, ch);
                MidiTrack &t = tracks_value[i + idx];
                if (&t != &actual_midi_track) {
                    t.insert(ie->first, e);
                    ie = actual_midi_track.events().erase(ie);
                    continue;
                }
            }
            ++ie;
        }
        i += nn - 1;
    }
}
} // namespace mu::iex::midi
