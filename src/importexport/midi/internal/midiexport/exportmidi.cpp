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

#include "exportmidi.h"

#include "engraving/dom/chordrest.h"
#include "engraving/dom/key.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/sig.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/synthesizerstate.h"
#include "engraving/dom/tempotimeline.h"

#include "engraving/compat/midi/event.h"
#include "engraving/compat/midi/compatmidirender.h"

#include "log.h"

#include "io/file.h"

using namespace mu::engraving;

namespace mu::iex::midi {
//---------------------------------------------------------
//   writeHeader
//---------------------------------------------------------

void ExportMidi::write_header(const CompatMidiRendererInternal::Context &context) {
    if (midi_file_.tracks().empty()) {
        return;
    }
    MidiTrack &track = midi_file_.tracks().front();

    //--------------------------------------------
    //    write track names
    //--------------------------------------------

    int staff_idx = 0;
    for (auto &track1 : midi_file_.tracks()) {
        Staff *staff = score_->staff(staff_idx);

        muse::ByteArray part_name = staff->part_name().to_utf8();
        size_t len = part_name.size() + 1;
        std::vector<unsigned char> data(part_name.const_data(), part_name.const_data() + len);

        MidiEvent ev;
        ev.set_type(ME_META);
        ev.set_meta_type(META_TRACK_NAME);
        ev.set_e_data(std::move(data));
        ev.set_len(static_cast<int>(len));

        track1.insert(0, ev);

        ++staff_idx;
    }

    //--------------------------------------------
    //    write time signature
    //--------------------------------------------

    TimeSigMap *sigmap = score_->sigmap();
    for (const RepeatSegment *rs : score_->repeat_list()) {
        int start_tick = rs->tick;
        int end_tick = rs->end_tick();
        int tick_offset = rs->utick - rs->tick;

        auto bs = sigmap->lower_bound(start_tick);
        auto es = sigmap->lower_bound(end_tick);

        for (auto is = bs; is != es; ++is) {
            SigEvent se = is->second;
            Fraction ts(se.timesig());
            int n;
            switch (ts.denominator()) {
                case 1: n = 0; break;
                case 2: n = 1; break;
                case 4: n = 2; break;
                case 8: n = 3; break;
                case 16: n = 4; break;
                case 32: n = 5; break;
                default:
                    n = 2;
                    LOGD("ExportMidi: unknown time signature %s", muPrintable(ts.to_string()));
                    break;
            }

            MidiEvent ev;
            ev.set_type(ME_META);
            ev.set_meta_type(META_TIME_SIGNATURE);
            ev.set_len(4);
            ev.set_e_data({static_cast<unsigned char>(ts.numerator()), static_cast<unsigned char>(n), 24, 8});
            track.insert(CompatMidiRender::tick(context, is->first + tick_offset), ev);
        }
    }

    //---------------------------------------------------
    //    write key signatures
    //    assume every staff corresponds to a midi track
    //---------------------------------------------------

    staff_idx = 0;
    for (auto &track1 : midi_file_.tracks()) {
        Staff *staff = score_->staff(staff_idx);
        KeyList *keys = staff->key_list();

        bool initial_key_sig_found = false;
        for (const RepeatSegment *rs : score_->repeat_list()) {
            int start_tick = rs->tick;
            int end_tick = start_tick + rs->len();
            int tick_offset = rs->utick - rs->tick;

            auto sk = keys->lower_bound(start_tick);
            auto ek = keys->lower_bound(end_tick);

            for (auto ik = sk; ik != ek; ++ik) {
                MidiEvent ev;
                ev.set_type(ME_META);
                Key key = ik->second.concert_key(); // -7 -- +7
                ev.set_meta_type(META_KEY_SIGNATURE);
                ev.set_len(2);
                ev.set_e_data({static_cast<unsigned char>(key), 0 /* major */});
                int tick = ik->first + tick_offset;
                track1.insert(CompatMidiRender::tick(context, tick), ev);
                if (tick == 0) {
                    initial_key_sig_found = true;
                }
            }
        }

        // fall back write a default C keysig if no initial keysig found
        if (!initial_key_sig_found) {
            MidiEvent ev;
            ev.set_type(ME_META);
            ev.set_meta_type(META_KEY_SIGNATURE);
            ev.set_len(2);
            ev.set_e_data({0 /* key */, 0 /* major */});
            track1.insert(0, ev);
        }

        ++staff_idx;
    }

    //--------------------------------------------
    //    write tempo changes from PauseMap
    //     don't need to unwind or add pauses as this was done already
    //--------------------------------------------

    if (!context.apply_caesuras) {
        return;
    }

    const std::map<int, double> &tempo_events = context.pause_map->tempo_events();
    BeatsPerSecond tempo_multiplier = score_->tempo_timeline().tempo_multiplier();
    for (const auto &[tick, bps] : tempo_events) {
        MidiEvent ev;
        ev.set_type(ME_META);
        //
        // compute midi tempo: microseconds / quarter note
        //
        int tempo = lrint((1.0 / bps * tempo_multiplier.val) * 1000000.0);

        ev.set_meta_type(META_TEMPO);
        ev.set_len(3);
        ev.set_e_data({static_cast<unsigned char>(tempo >> 16), static_cast<unsigned char>(tempo >> 8), static_cast<unsigned char>(tempo)});
        track.insert(tick, ev);
    }
}

//---------------------------------------------------------
//  write
//    export midi file
//    return false on error
//
//    The 3rd and 4th versions of write create a temporary, uninitialized synth state
//    so we can render the midi - it should fall back correctly to the defaults, with a warning.
//    These should only be used for tests. When actually rendering midi as a user action,
//    make sure to use the 1st and 2nd versions, passing the global musescore synth state
//    from mscore->synthesizerState() as the synthState parameter.
//---------------------------------------------------------

bool ExportMidi::write(MidiIODevice *device, bool midi_expand_repeats, bool export_rp_ns, const SynthesizerState &synth_state) {
    midi_file_.set_division(Constants::division);
    midi_file_.set_format(1);
    std::vector<MidiTrack> &tracks = midi_file_.tracks();

    for (size_t i = 0; i < score_->nstaves(); ++i) {
        tracks.push_back(MidiTrack());
    }

    EventsHolder events;
    CompatMidiRendererInternal::Context context;
    context.each_string_has_channel = false;
    context.instruments_have_effects = false;
    context.harmony_channel_setting = CompatMidiRendererInternal::HarmonyChannelSetting::DEFAULT;
    context.snd_controller = CompatMidiRender::get_controller_for_snd(score_, synth_state.cc_to_use());
    context.use_default_articulations = false;
    context.apply_caesuras = true;

    CompatMidiRender::render_score(score_, events, context, midi_expand_repeats);

    write_header(context);

    staff_idx_t staff_idx = 0;
    for (auto &track : tracks) {
        Staff *staff = score_->staff(staff_idx);
        Part *part = staff->part();

        track.set_out_port(part->midi_port());
        track.set_out_channel(part->midi_channel());

        // Pass through the all instruments in the part
        for (const auto &pair : part->instruments()) {
            // Pass through the all channels of the instrument
            // "normal", "pizzicato", "tremolo" for Strings,
            // "normal", "mute" for Trumpet
            for (const InstrChannel *instr_chan : pair.second->channel()) {
                const InstrChannel *ch = part->master_score()->playback_channel(instr_chan);
                char port = part->master_score()->midi_port(ch->channel());
                char channel = part->master_score()->midi_channel(ch->channel());

                if (staff->is_top()) {
                    track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_RESET_ALL_CTRL, 0));
                    // We need this to get the correct pitch of bends
                    // Hidden under preferences because some software
                    // crashes when receiving RPNs: https://musescore.org/en/node/37431
                    if (channel != 9 && export_rp_ns) {
                        // set pitch bend sensitivity to 12 semitones:
                        track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_LRPN, 0));
                        track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_HRPN, 0));
                        track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_HDATA, 12));

                        // reset fine tuning
                        /*track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_LRPN, 1));
                        track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_HRPN, 0));
                        track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_HDATA, 64));*/

                        // deactivate rpn
                        track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_LRPN, 127));
                        track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_HRPN, 127));
                    }

                    if (ch->program() != -1) {
                        track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_PROGRAM, ch->program()));
                    }
                    track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_VOLUME, ch->volume()));
                    track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_PANPOT, ch->pan()));
                    track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_REVERB_SEND, ch->reverb()));
                    track.insert(0, MidiEvent(ME_CONTROLLER, channel, CTRL_CHORUS_SEND, ch->chorus()));
                }

                // Export port to MIDI META event
                if (track.out_port() >= 0 && track.out_port() <= 127) {
                    MidiEvent ev;
                    ev.set_type(ME_META);
                    ev.set_meta_type(META_PORT_CHANGE);
                    ev.set_len(1);
                    ev.set_e_data({static_cast<unsigned char>(track.out_port())});
                    track.insert(0, ev);
                }

                for (size_t e = 0; e < events.size(); ++e) {
                    auto &multimap = events[e];
                    for (auto &item : multimap) {
                        const NPlayEvent &event = item.second;
                        if (event.is_muted()) {
                            continue;
                        }
                        if (event.discard() == staff_idx + 1 && event.velo() > 0) {
                            // turn note off so we can restrike it in another track
                            track.insert(CompatMidiRender::tick(context, item.first), MidiEvent(ME_NOTEON, channel, event.pitch(), 0));
                        }

                        staff_idx_t equivalent_staff_idx = staff_idx;
                        for (Staff *st : score_->master_score()->staves()) {
                            if (staff->id() == st->id()) {
                                equivalent_staff_idx = st->idx();
                            }
                        }

                        if (event.get_originating_staff() != equivalent_staff_idx) {
                            continue;
                        }

                        if (event.discard() && event.velo() == 0) {
                            // ignore noteoff but restrike noteon
                            continue;
                        }

                        if (!export_rp_ns && event.type() == ME_CONTROLLER && event.portamento()) {
                            // ignore portamento control events if exportRPN isn't switched on
                            continue;
                        }

                        char event_port = score_->master_score()->midi_port(event.channel());
                        char event_channel = score_->master_score()->midi_channel(event.channel());
                        if (port != event_port || channel != event_channel) {
                            continue;
                        }

                        if (event.type() == ME_NOTEON) {
                            // use the note values instead of the event values if portamento is suppressed
                            if (!export_rp_ns && event.portamento()) {
                                track.insert(CompatMidiRender::tick(context, item.first),
                                             MidiEvent(ME_NOTEON, channel, event.note()->pitch(), event.velo()));
                            } else {
                                track.insert(CompatMidiRender::tick(context, item.first),
                                             MidiEvent(ME_NOTEON, channel, event.pitch(), event.velo()));
                            }
                        } else if (event.type() == ME_CONTROLLER) {
                            track.insert(CompatMidiRender::tick(context, item.first),
                                         MidiEvent(ME_CONTROLLER, channel, event.controller(), event.value()));
                        } else if (event.type() == ME_PITCHBEND) {
                            track.insert(CompatMidiRender::tick(context, item.first),
                                         MidiEvent(ME_PITCHBEND, channel, event.data_a(), event.data_b()));
                        } else {
                            LOGD("writeMidi: unknown midi event 0x%02x", event.type());
                        }
                    }
                }
            }
        }

        // Export lyrics and RehearsalMarks as Meta events
        const TrackRange track_range = part->track_range();
        for (const RepeatSegment *rs : score_->repeat_list()) {
            int end_tick = rs->end_tick();
            int tick_offset = rs->utick - rs->tick;

            // export Lyrics
            SegmentType st = SegmentType::CHORD_REST;
            for (Segment *seg = rs->first_measure()->first(st); seg && seg->tick().ticks() < end_tick; seg = seg->next1(st)) {
                for (track_idx_t i = track_range.start_track; i < track_range.end_track; ++i) {
                    ChordRest *cr = to_chord_rest(seg->element(i));
                    if (cr) {
                        for (const auto &lyric : cr->lyrics()) {
                            LyricsSyllabic syllabic = lyric->syllabic();
                            muse::ByteArray lyric_text = lyric->plain_text().to_utf8();
                            if ((syllabic == LyricsSyllabic::SINGLE || syllabic == LyricsSyllabic::END) &&
                                (lyric_text.empty() || lyric_text[lyric_text.size() - 1] != ' ')) {
                                lyric_text.push_back(' ');
                            }

                            size_t len = lyric_text.size() + 1;
                            std::vector<unsigned char> data(lyric_text.const_data(), lyric_text.const_data() + len);

                            MidiEvent ev;
                            ev.set_type(ME_META);
                            ev.set_meta_type(META_LYRIC);
                            ev.set_e_data(std::move(data));
                            ev.set_len(static_cast<int>(len));

                            int tick = cr->tick().ticks() + tick_offset;
                            track.insert(CompatMidiRender::tick(context, tick), ev);
                        }
                    }
                }
            }

            // export RehearsalMarks only for first track
            if (staff_idx == 0) {
                for (Segment *seg = rs->first_measure()->first(SegmentType::DURATION); seg && seg->tick().ticks() < end_tick;
                     seg = seg->next1(SegmentType::DURATION)) {
                    for (EngravingItem *e : seg->annotations()) {
                        if (e->is_rehearsal_mark()) {
                            RehearsalMark *r = to_rehearsal_mark(e);
                            muse::ByteArray r_text = r->plain_text().to_utf8();
                            size_t len = r_text.size() + 1;
                            std::vector<unsigned char> data(r_text.const_data(), r_text.const_data() + len);

                            MidiEvent ev;
                            ev.set_type(ME_META);
                            ev.set_meta_type(META_MARKER);
                            ev.set_e_data(std::move(data));
                            ev.set_len(static_cast<int>(len));

                            int tick = r->segment()->tick().ticks() + tick_offset;
                            track.insert(CompatMidiRender::tick(context, tick), ev);
                        }
                    }
                }
            }
        }
        ++staff_idx;
    }
    return !midi_file_.write(device);
}

bool ExportMidi::write(const MidiPath &name, bool midi_expand_repeats, bool export_rp_ns, const SynthesizerState &synth_state) {
    muse::io::File file(name);
    if (!file.open(muse::io::IODevice::WRITE_ONLY)) {
        return false;
    }
    return write(&file, midi_expand_repeats, export_rp_ns, synth_state);
}

bool ExportMidi::write(MidiIODevice *device, bool midi_expand_repeats, bool export_rp_ns) {
    SynthesizerState ss;
    return write(device, midi_expand_repeats, export_rp_ns, ss);
}

bool ExportMidi::write(const MidiPath &name, bool midi_expand_repeats, bool export_rp_ns) {
    SynthesizerState ss;
    return write(name, midi_expand_repeats, export_rp_ns, ss);
}
} // namespace mu::iex::midi
