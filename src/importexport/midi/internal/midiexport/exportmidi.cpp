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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "engraving/dom/chordrest.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/key.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/sig.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/playback/flatplaybackscore.h"
#include "engraving/playback/playbackwalk.h"
#include "global/btree.h"
#include "global/defer.h"
#include "global/flatmap.h"
#include "global/flatset.h"
#include "global/interpolation.h"
#include "global/io/file.h"
#include "global/io/writebudget.h"
#include "log.h"
#include "mpe/automationpoint.h"
#include "mpe/flatevents.h"
#include "mpe/mpetypes.h"

using namespace mu::engraving;

namespace mu::iex::midi {
namespace {

constexpr int k_pitch_bend_center = 8192;
constexpr int k_pitch_bend_max = 16383;
constexpr double k_pitch_bend_range_semitones = 12.0;
constexpr muse::mpe::timestamp_t k_dynamic_step_us = 30000;

void check_budget(std::uint64_t bytes = 0) {
    if (auto *budget = muse::io::active_write_budget) {
        const bool ok = bytes ? budget->charge(bytes) : budget->poll();
        if (!ok) throw muse::io::WriteBudgetStop{};
    }
}

bool is_bend_articulation(muse::mpe::ArticulationType type) {
    using AT = muse::mpe::ArticulationType;
    switch (type) {
        case AT::BRASS_BEND:
        case AT::MULTIBEND:
        case AT::SLIDE_OUT_UP:
        case AT::CONTINUOUS_GLISSANDO:
        case AT::FALL:
        case AT::QUICK_FALL:
        case AT::DOIT:
        case AT::PLOP:
        case AT::SCOOP:
        case AT::SLIDE_OUT_DOWN:
        case AT::SLIDE_IN_ABOVE:
        case AT::SLIDE_IN_BELOW: return true;
        default: return false;
    }
}

int expression_level(muse::mpe::dynamic_level_t dynamic_level) {
    using namespace muse::mpe;
    static constexpr dynamic_level_t k_min_level = dynamic_level_from_type(muse::mpe::DynamicType::PPP);
    static constexpr dynamic_level_t k_max_level = dynamic_level_from_type(muse::mpe::DynamicType::FFF);
    static constexpr int k_min_volume = 16;
    static constexpr int k_max_volume = 127;
    static constexpr int k_volume_step = 16;

    if (dynamic_level <= k_min_level) return k_min_volume;
    if (dynamic_level >= k_max_level) return k_max_volume;

    float step_count = static_cast<float>(dynamic_level - k_min_level) / static_cast<float>(dynamic_level_step);
    if (dynamic_level == dynamic_level_from_type(muse::mpe::DynamicType::NATURAL)) {
        step_count -= 0.5f;
    }
    const int result = static_cast<int>(std::lround(k_min_volume + step_count * k_volume_step));
    return std::clamp(result, k_min_volume, k_max_volume);
}

int note_velocity(const muse::mpe::NoteRecord &note, std::span<const muse::mpe::CurvePoint> expression_curve,
                  bool supports_single_note_dynamics) {
    if (note.velocity_override.has_value()) {
        return std::clamp(static_cast<int>(std::lround(*note.velocity_override * 127.0f)), 1, 127);
    }
    if (supports_single_note_dynamics) {
        const float fraction = expression_curve.empty() ? 0.5f : muse::mpe::velocity_fraction(expression_curve);
        return std::clamp(static_cast<int>(std::lround(fraction * 127.0f)), 1, 127);
    }
    const muse::mpe::dynamic_level_t level =
        expression_curve.empty() ? note.nominal_dynamic_level : muse::mpe::max_amplitude_level(expression_curve);
    return std::clamp(expression_level(level), 1, 127);
}

uint8_t note_pitch_index(muse::mpe::pitch_level_t pitch_level) {
    const float steps =
        muse::mpe::zero_pitch_level_midi_equivalent + static_cast<float>(pitch_level) / static_cast<float>(muse::mpe::pitch_level_step);
    return static_cast<uint8_t>(std::clamp(std::floor(steps + 0.5f), 0.0f, 127.0f));
}

double note_tuning_semitones(muse::mpe::pitch_level_t pitch_level, uint8_t midi_pitch) {
    const int semitones = static_cast<int>(midi_pitch) - muse::mpe::zero_pitch_level_midi_equivalent;
    const muse::mpe::pitch_level_t delta = pitch_level - semitones * muse::mpe::pitch_level_step;
    return static_cast<double>(delta) / static_cast<double>(muse::mpe::pitch_level_step);
}

int pitch_bend_from_semitones(double semitones) {
    const long offset = std::lround(semitones * (static_cast<double>(k_pitch_bend_center) / k_pitch_bend_range_semitones));
    return std::clamp(k_pitch_bend_center + static_cast<int>(offset), 0, k_pitch_bend_max);
}

int pitch_bend_from_level(muse::mpe::pitch_level_t pitch_level) {
    const double semitones = static_cast<double>(pitch_level) / static_cast<double>(muse::mpe::pitch_level_step);
    return pitch_bend_from_semitones(semitones);
}

MidiEvent make_pitch_bend(uint8_t channel, int value) {
    return MidiEvent(ME_PITCHBEND, channel, static_cast<uchar>(value & 0x7f), static_cast<uchar>((value >> 7) & 0x7f));
}

// MIDI files cannot hold a zero-tick pause, so caesura/fermata pauses from
// TempoTimeline are expanded into extra measure-aligned ticks with temporary
// tempo events, keeping barlines aligned and mapping MPE timestamps to SMF ticks.
struct MidiTimeMap {
    struct PauseEntry {
        int utick = 0;
        double start_nominal_secs = 0.0;
        int offset_before = 0;
        int offset_after = 0;
    };

    const TempoTimeline &timeline;
    std::vector<PauseEntry> pauses;
    muse::BTreeMap<int, double> tempo_events;

    MidiTimeMap(const MasterScore *master, const TempoTimeline &tl, bool expand_repeats) : timeline(tl) {
        const TimeSigMap *sigmap = master->sigmap();
        const RepeatList &repeats = master->repeat_list(expand_repeats);
        int cumulative_offset = 0;

        for (const TempoTimePoint &point : timeline.points()) {
            check_budget(sizeof(PauseEntry));
            if (point.pause <= 0.0) {
                tempo_events[point.utick + cumulative_offset] = point.bps;
                continue;
            }

            const int written_tick = repeats.utick2tick(point.utick);
            const Fraction ts(sigmap->timesig(written_tick).timesig());
            const double quarters_per_measure = (4.0 * ts.numerator()) / ts.denominator();
            const int pause_ticks = static_cast<int>(quarters_per_measure * Constants::division);
            const int offset_before = cumulative_offset;
            const int offset_after = cumulative_offset + pause_ticks;

            tempo_events[point.utick + offset_before] = quarters_per_measure / point.pause;
            pauses.push_back(PauseEntry{point.utick, point.time - point.pause, offset_before, offset_after});
            cumulative_offset = offset_after;
            tempo_events[point.utick + offset_after] = point.bps;
        }
    }

    int offset_at_utick(int utick) const {
        if (pauses.empty()) return 0;
        const auto it =
            std::upper_bound(pauses.begin(), pauses.end(), utick, [](int t, const PauseEntry &entry) { return t < entry.utick; });
        return it == pauses.begin() ? 0 : std::prev(it)->offset_after;
    }

    int utick_to_smf(int utick) const { return std::max(0, utick + offset_at_utick(utick)); }

    int timestamp_to_smf(muse::mpe::timestamp_t us, bool prefer_before_pause = false) const {
        const double secs = static_cast<double>(us) / 1'000'000.0;
        const int utick = timeline.utime2utick(secs);
        if (pauses.empty()) return std::max(0, utick);

        if (prefer_before_pause) {
            const double nominal_secs = secs * timeline.tempo_multiplier().val;
            const auto it =
                std::lower_bound(pauses.begin(), pauses.end(), utick, [](const PauseEntry &entry, int t) { return entry.utick < t; });
            if (it != pauses.end() && it->utick == utick && nominal_secs <= it->start_nominal_secs + 1e-6) {
                return std::max(0, utick + it->offset_before);
            }
        }
        return utick_to_smf(utick);
    }
};

// Priority ensures canonical SMF ordering at equal ticks:
// meta/setup -> note-off -> controllers/pitch-bend -> note-on.
enum class EventOrder : uint8_t {
    META_OR_SETUP = 0,
    NOTE_OFF = 1,
    CONTROL = 2,
    NOTE_ON = 3,
};

struct PendingEvent {
    int tick = 0;
    EventOrder order = EventOrder::CONTROL;
    MidiEvent event;
};

const InstrChannel *resolve_track_channel(const MasterScore *master, const Part *part, const Instrument *instr,
                                          std::span<const muse::mpe::ArticulationRecord> articulations) {
    if (!instr || instr->channel().empty()) return nullptr;
    auto find_named = [&](const char *name) -> const InstrChannel * {
        for (const InstrChannel *ch : instr->channel()) {
            if (ch && ch->name() == String::from_ascii(name)) {
                return master->playback_channel(ch);
            }
        }
        return nullptr;
    };
    for (const muse::mpe::ArticulationRecord &art : articulations) {
        using AT = muse::mpe::ArticulationType;
        if (art.type == AT::PIZZICATO || art.type == AT::SNAP_PIZZICATO) {
            if (const InstrChannel *ch = find_named("pizzicato")) return ch;
        } else if (art.type == AT::MUTE) {
            if (const InstrChannel *ch = find_named("mute")) return ch;
        } else if (art.type == AT::PALM_MUTE) {
            if (const InstrChannel *ch = find_named(InstrChannel::palm_mute_name)) return ch;
        }
    }
    return master->playback_channel(instr->channel(0));
}

} // namespace

bool ExportMidi::write(MidiIODevice *device, bool midi_expand_repeats, bool export_rp_ns) {
    if (!score_ || !device) return false;
    check_budget();

    MasterScore *master = score_->master_score();
    if (!master) return false;

    midi_file_.set_division(Constants::division);
    midi_file_.set_format(1);
    std::vector<MidiTrack> &tracks = midi_file_.tracks();
    tracks.clear();
    tracks.resize(score_->nstaves());

    struct RestoreRepeats {
        MasterScore *master;
        bool expanded;

        ~RestoreRepeats() { master->set_expand_repeats(expanded); }
    } restore{master, master->expand_repeats()};

    // Ensure Staff::swing_map_ reflects any SystemText/StaffText swing markings loaded from MSCX.
    master->update_swing();

    // Build the flat repeat-expanded MPE store on the master score (excerpt staves
    // are linked to master staves, so building on master preserves primary staves).
    PlaybackScore playback;
    DEFER { playback.release(); };
    try {
        muse::mpe::ArticulationProfileBundle profiles;
        profiles.repository = profiles_;
        PlaybackScoreBuilder builder{std::move(profiles)};
        builder.play_repeats = midi_expand_repeats;
        builder.metronome_enabled = false;
        playback = builder.build(master, [] { check_budget(); });
    } catch (const muse::mpe::StoreLimit &) {
        if (auto *budget = muse::io::active_write_budget) {
            budget->failure = muse::io::WriteBudget::Failure::LIMIT;
            throw muse::io::WriteBudgetStop{};
        }
        return false;
    }

    const TempoTimeline &timeline = master->tempo_timeline(midi_expand_repeats);
    const RepeatList &repeats = master->repeat_list(midi_expand_repeats);
    const MidiTimeMap time_map(master, timeline, midi_expand_repeats);

    std::vector<std::vector<PendingEvent>> pending(tracks.size());
    const auto emit = [&](staff_idx_t local_staff, int tick, EventOrder order, MidiEvent ev) {
        check_budget(sizeof(PendingEvent));
        pending[local_staff].push_back(PendingEvent{std::max(0, tick), order, std::move(ev)});
    };

    // Map master staff indices to this score's staff indices (filters excerpts cleanly).
    muse::FlatMap<ID, staff_idx_t> master_staff_by_id;
    for (const Staff *st : master->staves()) {
        if (st) master_staff_by_id.emplace(st->id(), st->idx());
    }
    muse::FlatMap<staff_idx_t, staff_idx_t> master_to_local_staff;
    for (staff_idx_t local_idx = 0; local_idx < score_->nstaves(); ++local_idx) {
        const Staff *st = score_->staff(local_idx);
        if (!st) continue;
        if (const auto it = master_staff_by_id.find(st->id()); it != master_staff_by_id.end()) {
            master_to_local_staff.emplace(it->second, local_idx);
        }
    }

    // Track metadata, key signatures, time signatures, tempo map, port/channel setup, lyrics, markers.
    if (!tracks.empty()) {
        for (staff_idx_t staff_idx = 0; staff_idx < score_->nstaves(); ++staff_idx) {
            check_budget();
            Staff *staff = score_->staff(staff_idx);
            Part *part = staff->part();
            tracks[staff_idx].set_out_port(part->midi_port());
            tracks[staff_idx].set_out_channel(part->midi_channel());

            // Track name
            muse::ByteArray part_name = staff->part_name().to_utf8();
            size_t len = part_name.size() + 1;
            std::vector<unsigned char> data(part_name.const_data(), part_name.const_data() + len);
            MidiEvent name_ev;
            name_ev.set_type(ME_META);
            name_ev.set_meta_type(META_TRACK_NAME);
            name_ev.set_e_data(std::move(data));
            name_ev.set_len(static_cast<int>(len));
            emit(staff_idx, 0, EventOrder::META_OR_SETUP, std::move(name_ev));

            // Time signatures and tempo changes on Track 0
            if (staff_idx == 0) {
                TimeSigMap *sigmap = master->sigmap();
                for (const RepeatSegment *rs : repeats) {
                    const int start_tick = rs->tick;
                    const int end_tick = rs->end_tick();
                    const int tick_offset = rs->utick - rs->tick;
                    for (auto is = sigmap->lower_bound(start_tick), es = sigmap->lower_bound(end_tick); is != es; ++is) {
                        const Fraction ts(is->second.timesig());
                        int denom_pow = 2;
                        switch (ts.denominator()) {
                            case 1: denom_pow = 0; break;
                            case 2: denom_pow = 1; break;
                            case 4: denom_pow = 2; break;
                            case 8: denom_pow = 3; break;
                            case 16: denom_pow = 4; break;
                            case 32: denom_pow = 5; break;
                            default: LOGD("ExportMidi: unknown time signature %s", muPrintable(ts.to_string())); break;
                        }
                        MidiEvent ts_ev;
                        ts_ev.set_type(ME_META);
                        ts_ev.set_meta_type(META_TIME_SIGNATURE);
                        ts_ev.set_len(4);
                        ts_ev.set_e_data({static_cast<unsigned char>(ts.numerator()), static_cast<unsigned char>(denom_pow), 24, 8});
                        emit(0, time_map.utick_to_smf(is->first + tick_offset), EventOrder::META_OR_SETUP, std::move(ts_ev));
                    }
                }

                const BeatsPerSecond tempo_multiplier = timeline.tempo_multiplier();
                for (const auto &[tick, bps] : time_map.tempo_events) {
                    const int tempo = static_cast<int>(std::lrint((1.0 / bps * tempo_multiplier.val) * 1'000'000.0));
                    MidiEvent tempo_ev;
                    tempo_ev.set_type(ME_META);
                    tempo_ev.set_meta_type(META_TEMPO);
                    tempo_ev.set_len(3);
                    tempo_ev.set_e_data({static_cast<unsigned char>(tempo >> 16), static_cast<unsigned char>(tempo >> 8),
                                         static_cast<unsigned char>(tempo)});
                    emit(0, tick, EventOrder::META_OR_SETUP, std::move(tempo_ev));
                }
            }

            // Key signatures
            KeyList *keys = staff->key_list();
            bool initial_key_sig_found = false;
            for (const RepeatSegment *rs : repeats) {
                const int start_tick = rs->tick;
                const int end_tick = start_tick + rs->len();
                const int tick_offset = rs->utick - rs->tick;
                for (auto ik = keys->lower_bound(start_tick), ek = keys->lower_bound(end_tick); ik != ek; ++ik) {
                    MidiEvent key_ev;
                    key_ev.set_type(ME_META);
                    key_ev.set_meta_type(META_KEY_SIGNATURE);
                    key_ev.set_len(2);
                    key_ev.set_e_data({static_cast<unsigned char>(ik->second.concert_key()), 0});
                    const int tick = ik->first + tick_offset;
                    emit(staff_idx, time_map.utick_to_smf(tick), EventOrder::META_OR_SETUP, std::move(key_ev));
                    if (tick == 0) initial_key_sig_found = true;
                }
            }
            if (!initial_key_sig_found) {
                MidiEvent key_ev;
                key_ev.set_type(ME_META);
                key_ev.set_meta_type(META_KEY_SIGNATURE);
                key_ev.set_len(2);
                key_ev.set_e_data({0, 0});
                emit(staff_idx, 0, EventOrder::META_OR_SETUP, std::move(key_ev));
            }

            // Top-staff channel setup (reset, optional RPN pitch-bend range, program, mix CCs)
            if (staff->is_top()) {
                muse::FlatSet<uint8_t> configured_channels;
                for (const auto &pair : part->instruments()) {
                    for (const InstrChannel *instr_chan : pair.second->channel()) {
                        const InstrChannel *ch = master->playback_channel(instr_chan);
                        if (!ch) continue;
                        const uint8_t channel = static_cast<uint8_t>(master->midi_channel(ch->channel()) & 0x0f);
                        if (!configured_channels.insert(channel).second) continue;

                        emit(staff_idx, 0, EventOrder::META_OR_SETUP, MidiEvent(ME_CONTROLLER, channel, CTRL_RESET_ALL_CTRL, 0));
                        if (channel != 9 && export_rp_ns) {
                            emit(staff_idx, 0, EventOrder::META_OR_SETUP, MidiEvent(ME_CONTROLLER, channel, CTRL_LRPN, 0));
                            emit(staff_idx, 0, EventOrder::META_OR_SETUP, MidiEvent(ME_CONTROLLER, channel, CTRL_HRPN, 0));
                            emit(staff_idx, 0, EventOrder::META_OR_SETUP, MidiEvent(ME_CONTROLLER, channel, CTRL_HDATA, 12));
                            emit(staff_idx, 0, EventOrder::META_OR_SETUP, MidiEvent(ME_CONTROLLER, channel, CTRL_LRPN, 127));
                            emit(staff_idx, 0, EventOrder::META_OR_SETUP, MidiEvent(ME_CONTROLLER, channel, CTRL_HRPN, 127));
                        }
                        if (ch->program() != -1) {
                            emit(staff_idx, 0, EventOrder::META_OR_SETUP,
                                 MidiEvent(ME_CONTROLLER, channel, CTRL_PROGRAM, static_cast<uchar>(ch->program())));
                        }
                        emit(staff_idx, 0, EventOrder::META_OR_SETUP,
                             MidiEvent(ME_CONTROLLER, channel, CTRL_VOLUME, static_cast<uchar>(ch->volume())));
                        emit(staff_idx, 0, EventOrder::META_OR_SETUP,
                             MidiEvent(ME_CONTROLLER, channel, CTRL_PANPOT, static_cast<uchar>(ch->pan())));
                        emit(staff_idx, 0, EventOrder::META_OR_SETUP,
                             MidiEvent(ME_CONTROLLER, channel, CTRL_REVERB_SEND, static_cast<uchar>(ch->reverb())));
                        emit(staff_idx, 0, EventOrder::META_OR_SETUP,
                             MidiEvent(ME_CONTROLLER, channel, CTRL_CHORUS_SEND, static_cast<uchar>(ch->chorus())));
                    }
                }
            }

            if (tracks[staff_idx].out_port() >= 0 && tracks[staff_idx].out_port() <= 127) {
                MidiEvent port_ev;
                port_ev.set_type(ME_META);
                port_ev.set_meta_type(META_PORT_CHANGE);
                port_ev.set_len(1);
                port_ev.set_e_data({static_cast<unsigned char>(tracks[staff_idx].out_port())});
                emit(staff_idx, 0, EventOrder::META_OR_SETUP, std::move(port_ev));
            }

            // Lyrics (staff-scoped) and RehearsalMarks (Track 0)
            const track_idx_t start_track = staff2track(staff_idx);
            const track_idx_t end_track = staff2track(staff_idx + 1);
            for (const RepeatSegment *rs : repeats) {
                check_budget();
                const int end_tick = rs->end_tick();
                const int tick_offset = rs->utick - rs->tick;

                // Measure pointers in RepeatSegment belong to master; walk this score's matching measures.
                Measure *first_m = score_->tick2measure(Fraction::from_ticks(rs->tick));
                if (!first_m) continue;

                for (Segment *seg = first_m->first(SegmentType::CHORD_REST); seg && seg->tick().ticks() < end_tick;
                     seg = seg->next1(SegmentType::CHORD_REST)) {
                    for (track_idx_t t = start_track; t < end_track; ++t) {
                        ChordRest *cr = to_chord_rest(seg->element(t));
                        if (!cr) continue;
                        for (const auto &lyric : cr->lyrics()) {
                            const LyricsSyllabic syllabic = lyric->syllabic();
                            muse::ByteArray lyric_text = lyric->plain_text().to_utf8();
                            if ((syllabic == LyricsSyllabic::SINGLE || syllabic == LyricsSyllabic::END) &&
                                (lyric_text.empty() || lyric_text[lyric_text.size() - 1] != ' ')) {
                                lyric_text.push_back(' ');
                            }
                            const size_t l_len = lyric_text.size() + 1;
                            std::vector<unsigned char> l_data(lyric_text.const_data(), lyric_text.const_data() + l_len);
                            MidiEvent l_ev;
                            l_ev.set_type(ME_META);
                            l_ev.set_meta_type(META_LYRIC);
                            l_ev.set_e_data(std::move(l_data));
                            l_ev.set_len(static_cast<int>(l_len));
                            emit(staff_idx, time_map.utick_to_smf(cr->tick().ticks() + tick_offset), EventOrder::META_OR_SETUP,
                                 std::move(l_ev));
                        }
                    }
                }

                if (staff_idx == 0) {
                    for (Segment *seg = first_m->first(SegmentType::DURATION); seg && seg->tick().ticks() < end_tick;
                         seg = seg->next1(SegmentType::DURATION)) {
                        for (EngravingItem *e : seg->annotations()) {
                            if (!e->is_rehearsal_mark()) continue;
                            RehearsalMark *r = to_rehearsal_mark(e);
                            muse::ByteArray r_text = r->plain_text().to_utf8();
                            const size_t r_len = r_text.size() + 1;
                            std::vector<unsigned char> r_data(r_text.const_data(), r_text.const_data() + r_len);
                            MidiEvent m_ev;
                            m_ev.set_type(ME_META);
                            m_ev.set_meta_type(META_MARKER);
                            m_ev.set_e_data(std::move(r_data));
                            m_ev.set_len(static_cast<int>(r_len));
                            emit(0, time_map.utick_to_smf(r->segment()->tick().ticks() + tick_offset), EventOrder::META_OR_SETUP,
                                 std::move(m_ev));
                        }
                    }
                }
            }
        }
    }

    // Lower MPE TrackEvents onto staff MidiTracks (auxiliary Metronome & ChordSymbols muted by default).
    for (const auto &[track_id, events] : playback.tracks) {
        check_budget();
        if (track_id == playback_metronome_track_id() || is_playback_chord_symbols_track(track_id)) {
            continue;
        }
        if (events.notes.empty()) {
            continue;
        }

        const Part *part = master->part_by_id(track_id.part_id);
        if (!part) continue;
        const Instrument *instr = part->instrument_by_id(track_id.instrument_id);
        if (!instr) instr = part->instrument();
        if (!instr || instr->channel().empty()) continue;

        const bool use_snd = events.setup.supports_single_note_dynamics;

        // Sort note indices by group_timestamp if not already ascending.
        const std::size_t total_notes = events.notes.size();
        bool ascending = true;
        for (std::size_t i = 1; i < total_notes; ++i) {
            if (events.notes[i].group_timestamp < events.notes[i - 1].group_timestamp) {
                ascending = false;
                break;
            }
        }
        std::vector<std::size_t> order;
        if (!ascending) {
            check_budget(total_notes * sizeof(std::size_t));
            order.resize(total_notes);
            for (std::size_t i = 0; i < total_notes; ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                return events.notes[a].group_timestamp < events.notes[b].group_timestamp;
            });
        }

        // Deduplicate pedal CCs shared by notes in the same chord and merge sostenuto intervals.
        struct PedalKey {
            staff_idx_t staff = 0;
            uint8_t channel = 0;
            int tick = 0;
            uint8_t value = 0;

            bool operator<(const PedalKey &o) const {
                return std::tie(staff, channel, tick, value) < std::tie(o.staff, o.channel, o.tick, o.value);
            }
        };

        muse::FlatSet<PedalKey> emitted_sustain;

        struct SostenutoSpan {
            muse::mpe::timestamp_t start = 0;
            muse::mpe::duration_t duration = 0;
        };

        muse::FlatMap<std::pair<staff_idx_t, uint8_t>, std::vector<SostenutoSpan>> sostenuto_spans;
        muse::FlatMap<muse::mpe::layer_idx_t, std::pair<staff_idx_t, uint8_t>> dynamic_layer_targets;

        for (std::size_t idx = 0; idx < total_notes; ++idx) {
            check_budget();
            const muse::mpe::NoteRecord &note = events.notes[ascending ? idx : order[idx]];
            const auto staff_it = master_to_local_staff.find(static_cast<staff_idx_t>(note.staff_layer));
            if (staff_it == master_to_local_staff.end()) {
                continue; // Excerpt excludes this staff.
            }
            const staff_idx_t local_staff = staff_it->second;
            const std::span<const muse::mpe::ArticulationRecord> articulations = events.note_articulations(note);
            const std::span<const muse::mpe::CurvePoint> pitch_curve = events.pitch_curve(note);
            const std::span<const muse::mpe::CurvePoint> expr_curve = events.expression_curve(note);

            const InstrChannel *ch = resolve_track_channel(master, part, instr, articulations);
            if (!ch) continue;
            const uint8_t channel = static_cast<uint8_t>(master->midi_channel(ch->channel()) & 0x0f);

            const muse::mpe::layer_idx_t layer =
                muse::mpe::make_layer_idx(static_cast<muse::mpe::staff_layer_idx_t>(note.staff_layer), note.voice_layer);
            dynamic_layer_targets.emplace(layer, std::pair<staff_idx_t, uint8_t>{local_staff, channel});

            const uint8_t midi_pitch = note_pitch_index(note.nominal_pitch_level);
            const double tuning_semitones = note_tuning_semitones(note.nominal_pitch_level, midi_pitch);
            const uint8_t velocity = static_cast<uint8_t>(note_velocity(note, expr_curve, use_snd));

            const bool has_start = note.actual_duration > 0;
            const bool has_end = note.actual_duration != muse::mpe::infinite_duration;
            const int start_tick = time_map.timestamp_to_smf(note.actual_timestamp, false);
            int end_tick = has_end ? time_map.timestamp_to_smf(note.actual_timestamp + note.actual_duration, true) : start_tick;
            if (has_start && has_end && end_tick <= start_tick) {
                end_tick = start_tick + 1;
            }

            if (has_start) {
                emit(local_staff, start_tick, EventOrder::NOTE_ON, MidiEvent(ME_NOTEON, channel, midi_pitch, velocity));
            }
            if (has_end) {
                emit(local_staff, end_tick, EventOrder::NOTE_OFF, MidiEvent(ME_NOTEON, channel, midi_pitch, 0));
            }

            bool emitted_bend = false;
            for (const muse::mpe::ArticulationRecord &meta : articulations) {
                if (meta.type == muse::mpe::ArticulationType::STANDARD) continue;

                if (!pitch_curve.empty() && is_bend_articulation(meta.type)) {
                    emitted_bend = true;
                    const muse::mpe::timestamp_t note_end_us = note.actual_timestamp + note.actual_duration;
                    const muse::mpe::timestamp_t bend_end_us = std::min(meta.timestamp + meta.overall_duration, note_end_us);
                    const int bend_end_tick = time_map.timestamp_to_smf(bend_end_us, true);

                    int prev_bend = -1;
                    for (std::size_t p = 0; p + 1 < pitch_curve.size(); ++p) {
                        const auto &curr = pitch_curve[p];
                        const auto &next = pitch_curve[p + 1];
                        const int curr_val = pitch_bend_from_level(curr.value);
                        const int next_val = pitch_bend_from_level(next.value);
                        const muse::mpe::timestamp_t curr_us =
                            meta.timestamp +
                            static_cast<muse::mpe::timestamp_t>(meta.overall_duration * muse::mpe::percentage_to_factor(curr.at));
                        const muse::mpe::timestamp_t next_us =
                            meta.timestamp +
                            static_cast<muse::mpe::timestamp_t>(meta.overall_duration * muse::mpe::percentage_to_factor(next.at));

                        constexpr muse::mpe::pitch_level_t k_point_weight = muse::mpe::pitch_level_step / 25;
                        const std::size_t point_count =
                            std::max<std::size_t>(1, static_cast<std::size_t>(std::abs(next.value - curr.value) / k_point_weight));
                        const auto points = muse::interpolation::lerp(
                            muse::interpolation::Point{static_cast<double>(curr_us), static_cast<double>(curr_val)},
                            muse::interpolation::Point{static_cast<double>(next_us), static_cast<double>(next_val)}, point_count);

                        for (const auto &pt : points) {
                            const auto pt_us = static_cast<muse::mpe::timestamp_t>(std::llround(pt.x));
                            const int bend_val = std::clamp(static_cast<int>(std::lround(pt.y)), 0, k_pitch_bend_max);
                            if (pt_us < bend_end_us && bend_val != prev_bend) {
                                emit(local_staff, time_map.timestamp_to_smf(pt_us, false), EventOrder::CONTROL,
                                     make_pitch_bend(channel, bend_val));
                                prev_bend = bend_val;
                            }
                        }
                    }
                    emit(local_staff, bend_end_tick, EventOrder::CONTROL, make_pitch_bend(channel, k_pitch_bend_center));
                    continue;
                }

                if (meta.type == muse::mpe::ArticulationType::PEDAL) {
                    if (use_snd && meta.has_start() && !meta.has_end()) continue;
                    if (meta.has_start()) {
                        const int t = time_map.timestamp_to_smf(meta.timestamp, false);
                        if (emitted_sustain.insert(PedalKey{local_staff, channel, t, 127}).second) {
                            emit(local_staff, t, EventOrder::CONTROL, MidiEvent(ME_CONTROLLER, channel, CTRL_SUSTAIN, 127));
                        }
                    }
                    if (meta.has_end()) {
                        const int t = time_map.timestamp_to_smf(meta.timestamp + meta.overall_duration, true);
                        if (emitted_sustain.insert(PedalKey{local_staff, channel, t, 0}).second) {
                            emit(local_staff, t, EventOrder::CONTROL, MidiEvent(ME_CONTROLLER, channel, CTRL_SUSTAIN, 0));
                        }
                    }
                    continue;
                }

                if (meta.type == muse::mpe::ArticulationType::LAISSEZ_VIBRER) {
                    const muse::mpe::timestamp_t ts =
                        note.actual_timestamp + static_cast<muse::mpe::timestamp_t>(note.actual_duration * 0.1);
                    sostenuto_spans[{local_staff, channel}].push_back(SostenutoSpan{ts, meta.overall_duration});
                    continue;
                }
            }

            if (!emitted_bend && channel != 9 && std::abs(tuning_semitones) > 1e-4) {
                emit(local_staff, start_tick, EventOrder::CONTROL, make_pitch_bend(channel, pitch_bend_from_semitones(tuning_semitones)));
                if (has_end) {
                    emit(local_staff, end_tick, EventOrder::CONTROL, make_pitch_bend(channel, k_pitch_bend_center));
                }
            }
        }

        // Emit merged sostenuto pedal CCs.
        for (auto &[key, spans] : sostenuto_spans) {
            std::sort(spans.begin(), spans.end(), [](const SostenutoSpan &a, const SostenutoSpan &b) { return a.start < b.start; });
            for (std::size_t i = 0; i < spans.size(); ++i) {
                const int on_tick = time_map.timestamp_to_smf(spans[i].start, false);
                const muse::mpe::timestamp_t end_us = spans[i].start + spans[i].duration;
                emit(key.first, on_tick, EventOrder::CONTROL, MidiEvent(ME_CONTROLLER, key.second, CTRL_SOSTENUTO, 127));
                if (i + 1 == spans.size() || end_us <= spans[i + 1].start) {
                    emit(key.first, time_map.timestamp_to_smf(end_us, true), EventOrder::CONTROL,
                         MidiEvent(ME_CONTROLLER, key.second, CTRL_SOSTENUTO, 0));
                }
            }
        }

        // Emit CC11 expression curves for single-note-dynamics instruments.
        if (use_snd && !dynamic_layer_targets.empty()) {
            for (const muse::mpe::DynamicLayer &layer : events.dynamic_layers) {
                check_budget();
                const auto target_it = dynamic_layer_targets.find(layer.layer);
                if (target_it == dynamic_layer_targets.end()) continue;
                const staff_idx_t local_staff = target_it->second.first;
                const uint8_t channel = target_it->second.second;
                const std::span<const muse::mpe::DynamicPoint> curve = events.layer_points(layer);
                if (curve.empty()) continue;

                std::optional<int> last_level;
                muse::mpe::resample_curve(curve, k_dynamic_step_us, [&](muse::mpe::timestamp_t t, muse::real_t normalized) -> bool {
                    const int level = expression_level(muse::mpe::dynamic_level_from_normalized(normalized));
                    if (last_level == level) return true;
                    last_level = level;
                    emit(local_staff, time_map.timestamp_to_smf(t, false), EventOrder::CONTROL,
                         MidiEvent(ME_CONTROLLER, channel, CTRL_EXPRESSION, static_cast<uchar>(level)));
                    return true;
                });
            }
        }
    }

    // Sort each staff track canonically by (tick, order) and populate MidiFile.
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        check_budget();
        auto &list = pending[i];
        std::stable_sort(list.begin(), list.end(), [](const PendingEvent &a, const PendingEvent &b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return static_cast<uint8_t>(a.order) < static_cast<uint8_t>(b.order);
        });
        for (const PendingEvent &item : list) {
            check_budget();
            tracks[i].insert(item.tick, item.event);
        }
    }

    return !midi_file_.write(device);
}

bool ExportMidi::write(const MidiPath &name, bool midi_expand_repeats, bool export_rp_ns) {
    muse::io::File file(name);
    if (!file.open(muse::io::IODevice::WRITE_ONLY)) {
        return false;
    }
    return write(&file, midi_expand_repeats, export_rp_ns);
}
} // namespace mu::iex::midi
