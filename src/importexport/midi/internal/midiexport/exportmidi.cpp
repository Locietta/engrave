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
#include <tuple>
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
constexpr double k_pitch_bend_range_rpn_semitones = 12.0;
constexpr double k_pitch_bend_range_default_semitones = 2.0;
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

int pitch_bend_from_semitones(double semitones, double range_semitones) {
    const long offset = std::lround(semitones * (static_cast<double>(k_pitch_bend_center) / range_semitones));
    return std::clamp(k_pitch_bend_center + static_cast<int>(offset), 0, k_pitch_bend_max);
}

int pitch_bend_from_level(muse::mpe::pitch_level_t pitch_level, double range_semitones) {
    const double semitones = static_cast<double>(pitch_level) / static_cast<double>(muse::mpe::pitch_level_step);
    return pitch_bend_from_semitones(semitones, range_semitones);
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
// meta/setup -> note-off -> bend center reset -> dynamic channel setup -> controllers/active bend -> note-on.
enum class EventOrder : uint8_t {
    META_OR_SETUP = 0,
    NOTE_OFF = 1,
    BEND_RESET = 2,
    DYN_SETUP = 3,
    CONTROL = 4,
    NOTE_ON = 5,
};

struct PendingEvent {
    int tick = 0;
    EventOrder order = EventOrder::CONTROL;
    MidiEvent event;
    bool discard = false;
};

struct BendPoint {
    int tick = 0;
    int value = k_pitch_bend_center;
};

struct LoweredNote {
    std::size_t track_idx = 0;
    int port = 0;
    uint8_t base_channel = 0;
    uint8_t assigned_channel = 0;
    const InstrChannel *instr_channel = nullptr;
    uint8_t midi_pitch = 0;
    uint8_t velocity = 0;
    int start_tick = 0;
    int end_tick = 0;
    int bend_end_tick = 0;
    bool has_start = false;
    bool has_end = false;
    bool has_non_center_bend = false;
    bool use_snd = false;
    muse::mpe::timestamp_t actual_timestamp = 0;
    muse::mpe::duration_t actual_duration = 0;
    std::span<const muse::mpe::DynamicPoint> dynamic_curve;
    std::vector<BendPoint> bend_points;
    std::optional<int> bend_reset_tick;
};

const InstrChannel *resolve_track_channel(const MasterScore *master, const Instrument *instr,
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

// Reconcile overlapping same-pitch notes across all tracks sharing (port, channel, pitch)
// so an earlier note's release never cuts off a later sounding unison prematurely.
void reconcile_unisons(const std::vector<MidiTrack> &tracks, std::vector<std::vector<PendingEvent>> &pending) {
    struct NoteRef {
        int tick = 0;
        EventOrder order = EventOrder::NOTE_OFF;
        std::size_t track_idx = 0;
        std::size_t event_idx = 0;
    };

    std::vector<NoteRef> refs;
    for (std::size_t ti = 0; ti < pending.size(); ++ti) {
        check_budget();
        const auto &list = pending[ti];
        for (std::size_t ei = 0; ei < list.size(); ++ei) {
            if (!list[ei].discard && list[ei].event.type() == ME_NOTEON) {
                check_budget(sizeof(NoteRef));
                refs.push_back(NoteRef{list[ei].tick, list[ei].order, ti, ei});
            }
        }
    }
    if (refs.empty()) return;

    std::sort(refs.begin(), refs.end(), [](const NoteRef &a, const NoteRef &b) {
        return std::tie(a.tick, a.order, a.track_idx, a.event_idx) < std::tie(b.tick, b.order, b.track_idx, b.event_idx);
    });

    struct UnisonKey {
        int port = 0;
        uint8_t channel = 0;
        uint8_t pitch = 0;

        bool operator<(const UnisonKey &o) const { return std::tie(port, channel, pitch) < std::tie(o.port, o.channel, o.pitch); }
    };

    struct UnisonState {
        std::uint32_t active_count = 0;
        std::size_t active_track = 0;
        std::size_t active_event_idx = 0;
        int last_on_tick = -1;
    };

    muse::FlatMap<UnisonKey, UnisonState> states;
    std::vector<std::pair<std::size_t, PendingEvent>> additions;

    for (const NoteRef &ref : refs) {
        check_budget();
        PendingEvent &pe = pending[ref.track_idx][ref.event_idx];
        const UnisonKey key{tracks[ref.track_idx].out_port(), pe.event.channel(), pe.event.pitch()};
        UnisonState &st = states[key];

        if (pe.event.velo() == 0) {
            if (st.active_count == 0 || --st.active_count > 0) {
                pe.discard = true;
            } else if (ref.track_idx != st.active_track) {
                // Hoist the final note-off onto the track that struck the active note.
                pe.discard = true;
                check_budget(sizeof(std::pair<std::size_t, PendingEvent>));
                additions.emplace_back(st.active_track, PendingEvent{pe.tick, EventOrder::NOTE_OFF, pe.event, false});
            }
        } else {
            if (st.active_count > 0) {
                if (st.last_on_tick == pe.tick) {
                    // Simultaneous unison attack on the same (port, channel, pitch): keep the louder attack
                    // and hold until the longest note releases.
                    MidiEvent &active_ev = pending[st.active_track][st.active_event_idx].event;
                    if (pe.event.velo() > active_ev.velo()) {
                        active_ev.set_velo(pe.event.velo());
                    }
                    pe.discard = true;
                    ++st.active_count;
                } else {
                    // Staggered overlap: close the earlier note right before restriking.
                    check_budget(sizeof(std::pair<std::size_t, PendingEvent>));
                    additions.emplace_back(st.active_track, PendingEvent{pe.tick, EventOrder::NOTE_OFF,
                                                                         MidiEvent(ME_NOTEON, key.channel, key.pitch, 0), false});
                    ++st.active_count;
                    st.active_track = ref.track_idx;
                    st.active_event_idx = ref.event_idx;
                    st.last_on_tick = pe.tick;
                }
            } else {
                st.active_count = 1;
                st.active_track = ref.track_idx;
                st.active_event_idx = ref.event_idx;
                st.last_on_tick = pe.tick;
            }
        }
    }

    for (auto &[track_idx, ev] : additions) {
        check_budget(sizeof(PendingEvent));
        pending[track_idx].push_back(std::move(ev));
    }
}

} // namespace

bool ExportMidi::write(MidiIODevice *device, bool midi_expand_repeats, bool export_rp_ns) {
    if (!score_ || !device) return false;
    check_budget();

    MasterScore *master = score_->master_score();
    if (!master) return false;

    const double pitch_bend_range_semitones = export_rp_ns ? k_pitch_bend_range_rpn_semitones : k_pitch_bend_range_default_semitones;

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
        auto charge_working = [](std::uint64_t bytes) { check_budget(bytes); };
        const muse::FnRef<void(std::uint64_t)> charge_ref =
            muse::io::active_write_budget ? muse::FnRef<void(std::uint64_t)>{charge_working} : muse::FnRef<void(std::uint64_t)>{};
        playback = builder.build(master, [] { check_budget(); }, charge_ref);
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
    const auto emit = [&](std::size_t track_idx, int tick, EventOrder order, MidiEvent ev) {
        check_budget(sizeof(PendingEvent));
        pending[track_idx].push_back(PendingEvent{std::max(0, tick), order, std::move(ev), false});
    };

    const auto emit_channel_setup = [&](std::size_t track_idx, int tick, EventOrder order, uint8_t channel, const InstrChannel *ch,
                                        bool include_bank, bool reset_and_rpn) {
        if (reset_and_rpn) {
            emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_RESET_ALL_CTRL, 0));
            if (channel != 9 && export_rp_ns) {
                emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_LRPN, 0));
                emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_HRPN, 0));
                emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_HDATA, 12));
                emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_LRPN, 127));
                emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_HRPN, 127));
            }
        }
        if (!ch) return;
        if (include_bank && ch->bank() != 0) {
            emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_HBANK, static_cast<uchar>((ch->bank() >> 7) & 0x7f)));
            emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_LBANK, static_cast<uchar>(ch->bank() & 0x7f)));
        }
        if (ch->program() != -1) {
            emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_PROGRAM, static_cast<uchar>(ch->program())));
        }
        emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_VOLUME, static_cast<uchar>(ch->volume())));
        emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_PANPOT, static_cast<uchar>(ch->pan())));
        emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_REVERB_SEND, static_cast<uchar>(ch->reverb())));
        emit(track_idx, tick, order, MidiEvent(ME_CONTROLLER, channel, CTRL_CHORUS_SEND, static_cast<uchar>(ch->chorus())));
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

    // Reserve all statically mapped channels per MIDI port so dynamic bend channels never collide with them.
    muse::FlatMap<int, uint16_t> static_port_channels;
    for (const Part *part : master->parts()) {
        if (!part) continue;
        for (const auto &pair : part->instruments()) {
            for (const InstrChannel *instr_chan : pair.second->channel()) {
                const InstrChannel *ch = master->playback_channel(instr_chan);
                if (!ch) continue;
                const int ch_port = master->midi_port(ch->channel());
                const uint8_t channel = static_cast<uint8_t>(master->midi_channel(ch->channel()) & 0x0f);
                static_port_channels[ch_port] |= static_cast<uint16_t>(1u << channel);
            }
        }
    }

    muse::FlatSet<std::pair<std::size_t, uint8_t>> configured_track_channels;
    muse::FlatMap<std::pair<staff_idx_t, int>, std::size_t> overflow_port_tracks;

    const auto resolve_target_track = [&](staff_idx_t local_staff, int port, uint8_t channel, const InstrChannel *ch) -> std::size_t {
        if (tracks[local_staff].out_port() == port) {
            return local_staff;
        }
        const auto key = std::pair<staff_idx_t, int>{local_staff, port};
        auto it = overflow_port_tracks.find(key);
        std::size_t target_idx = 0;
        if (it == overflow_port_tracks.end()) {
            check_budget(sizeof(MidiTrack) + sizeof(std::vector<PendingEvent>));
            target_idx = tracks.size();
            tracks.emplace_back();
            pending.emplace_back();
            tracks[target_idx].set_out_port(port);
            tracks[target_idx].set_out_channel(channel);
            overflow_port_tracks.emplace(key, target_idx);

            const Staff *staff = score_->staff(local_staff);
            if (staff) {
                muse::ByteArray part_name = staff->part_name().to_utf8();
                const size_t len = part_name.size() + 1;
                std::vector<unsigned char> data(part_name.const_data(), part_name.const_data() + len);
                MidiEvent name_ev;
                name_ev.set_type(ME_META);
                name_ev.set_meta_type(META_TRACK_NAME);
                name_ev.set_e_data(std::move(data));
                name_ev.set_len(static_cast<int>(len));
                emit(target_idx, 0, EventOrder::META_OR_SETUP, std::move(name_ev));
            }
            if (port >= 0 && port <= 127) {
                MidiEvent port_ev;
                port_ev.set_type(ME_META);
                port_ev.set_meta_type(META_PORT_CHANGE);
                port_ev.set_len(1);
                port_ev.set_e_data({static_cast<unsigned char>(port)});
                emit(target_idx, 0, EventOrder::META_OR_SETUP, std::move(port_ev));
            }
        } else {
            target_idx = it->second;
        }
        if (ch && configured_track_channels.insert({target_idx, channel}).second) {
            emit_channel_setup(target_idx, 0, EventOrder::META_OR_SETUP, channel, ch, /*include_bank=*/ch->bank() != 0,
                               /*reset_and_rpn=*/true);
        }
        return target_idx;
    };

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
                    const double effective_bps = std::max(bps * tempo_multiplier.val, 1e-6);
                    const int tempo = static_cast<int>(std::lrint(1'000'000.0 / effective_bps));
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

            // Top-staff channel setup for channels sharing this staff's MIDI port.
            if (staff->is_top()) {
                for (const auto &pair : part->instruments()) {
                    for (const InstrChannel *instr_chan : pair.second->channel()) {
                        const InstrChannel *ch = master->playback_channel(instr_chan);
                        if (!ch) continue;
                        if (master->midi_port(ch->channel()) != tracks[staff_idx].out_port()) continue;
                        const uint8_t channel = static_cast<uint8_t>(master->midi_channel(ch->channel()) & 0x0f);
                        if (!configured_track_channels.insert({staff_idx, channel}).second) continue;
                        emit_channel_setup(staff_idx, 0, EventOrder::META_OR_SETUP, channel, ch, /*include_bank=*/false,
                                           /*reset_and_rpn=*/true);
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

    // Phase A: Lower MPE TrackEvents into notes, pedals, and expression curves (auxiliary Metronome & ChordSymbols muted by default).
    std::vector<LoweredNote> lowered_notes;
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

        muse::FlatMap<muse::mpe::layer_idx_t, std::span<const muse::mpe::DynamicPoint>> layer_curves;
        if (use_snd) {
            for (const muse::mpe::DynamicLayer &layer : events.dynamic_layers) {
                layer_curves.emplace(layer.layer, events.layer_points(layer));
            }
        }

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
            std::size_t track_idx = 0;
            uint8_t channel = 0;
            int tick = 0;
            uint8_t value = 0;

            bool operator<(const PedalKey &o) const {
                return std::tie(track_idx, channel, tick, value) < std::tie(o.track_idx, o.channel, o.tick, o.value);
            }
        };

        muse::FlatSet<PedalKey> emitted_sustain;

        struct SostenutoSpan {
            muse::mpe::timestamp_t start = 0;
            muse::mpe::duration_t duration = 0;
        };

        muse::FlatMap<std::pair<std::size_t, uint8_t>, std::vector<SostenutoSpan>> sostenuto_spans;
        muse::FlatMap<muse::mpe::layer_idx_t, muse::FlatSet<std::pair<std::size_t, uint8_t>>> dynamic_layer_targets;

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

            const InstrChannel *ch = resolve_track_channel(master, instr, articulations);
            if (!ch) continue;
            const int port = master->midi_port(ch->channel());
            const uint8_t channel = static_cast<uint8_t>(master->midi_channel(ch->channel()) & 0x0f);
            const std::size_t target_track = resolve_target_track(local_staff, port, channel, ch);

            const muse::mpe::layer_idx_t layer =
                muse::mpe::make_layer_idx(static_cast<muse::mpe::staff_layer_idx_t>(note.staff_layer), note.voice_layer);
            dynamic_layer_targets[layer].emplace(target_track, channel);

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

            LoweredNote ln;
            ln.track_idx = target_track;
            ln.port = port;
            ln.base_channel = channel;
            ln.assigned_channel = channel;
            ln.instr_channel = ch;
            ln.midi_pitch = midi_pitch;
            ln.velocity = velocity;
            ln.start_tick = start_tick;
            ln.end_tick = end_tick;
            ln.bend_end_tick = end_tick;
            ln.has_start = has_start;
            ln.has_end = has_end;
            ln.use_snd = use_snd;
            ln.actual_timestamp = note.actual_timestamp;
            ln.actual_duration = note.actual_duration;
            if (use_snd) {
                if (const auto curve_it = layer_curves.find(layer); curve_it != layer_curves.end()) {
                    ln.dynamic_curve = curve_it->second;
                }
            }

            bool emitted_bend = false;
            for (const muse::mpe::ArticulationRecord &meta : articulations) {
                if (meta.type == muse::mpe::ArticulationType::STANDARD) continue;

                if (!pitch_curve.empty() && is_bend_articulation(meta.type)) {
                    emitted_bend = true;
                    const muse::mpe::timestamp_t note_end_us = note.actual_timestamp + note.actual_duration;
                    const muse::mpe::timestamp_t bend_end_us = std::min(meta.timestamp + meta.overall_duration, note_end_us);
                    const int raw_bend_end_tick = time_map.timestamp_to_smf(bend_end_us, true);
                    const int bend_end_tick = has_end ? std::max(raw_bend_end_tick, start_tick + 1) : raw_bend_end_tick;

                    int prev_bend = -1;
                    for (std::size_t p = 0; p + 1 < pitch_curve.size(); ++p) {
                        const auto &curr = pitch_curve[p];
                        const auto &next = pitch_curve[p + 1];
                        const int curr_val = pitch_bend_from_level(curr.value, pitch_bend_range_semitones);
                        const int next_val = pitch_bend_from_level(next.value, pitch_bend_range_semitones);
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
                            const int pt_tick = time_map.timestamp_to_smf(pt_us, false);
                            if (pt_us < bend_end_us && pt_tick < bend_end_tick && bend_val != prev_bend) {
                                ln.bend_points.push_back(BendPoint{pt_tick, bend_val});
                                if (bend_val != k_pitch_bend_center) {
                                    ln.has_non_center_bend = true;
                                }
                                prev_bend = bend_val;
                            }
                        }
                    }
                    ln.bend_reset_tick = bend_end_tick;
                    ln.bend_end_tick = std::max(ln.bend_end_tick, bend_end_tick);
                    continue;
                }

                if (meta.type == muse::mpe::ArticulationType::PEDAL) {
                    if (use_snd && meta.has_start() && !meta.has_end()) continue;
                    if (meta.has_start()) {
                        const int t = time_map.timestamp_to_smf(meta.timestamp, false);
                        if (emitted_sustain.insert(PedalKey{target_track, channel, t, 127}).second) {
                            emit(target_track, t, EventOrder::CONTROL, MidiEvent(ME_CONTROLLER, channel, CTRL_SUSTAIN, 127));
                        }
                    }
                    if (meta.has_end()) {
                        const int t = time_map.timestamp_to_smf(meta.timestamp + meta.overall_duration, true);
                        if (emitted_sustain.insert(PedalKey{target_track, channel, t, 0}).second) {
                            emit(target_track, t, EventOrder::CONTROL, MidiEvent(ME_CONTROLLER, channel, CTRL_SUSTAIN, 0));
                        }
                    }
                    continue;
                }

                if (meta.type == muse::mpe::ArticulationType::LAISSEZ_VIBRER) {
                    const muse::mpe::timestamp_t ts =
                        note.actual_timestamp + static_cast<muse::mpe::timestamp_t>(note.actual_duration * 0.1);
                    sostenuto_spans[{target_track, channel}].push_back(SostenutoSpan{ts, meta.overall_duration});
                    continue;
                }
            }

            if (!emitted_bend && channel != 9 && std::abs(tuning_semitones) > 1e-4) {
                const int tune_val = pitch_bend_from_semitones(tuning_semitones, pitch_bend_range_semitones);
                if (tune_val != k_pitch_bend_center) {
                    ln.bend_points.push_back(BendPoint{start_tick, tune_val});
                    ln.has_non_center_bend = true;
                    if (has_end) {
                        ln.bend_reset_tick = end_tick;
                        ln.bend_end_tick = std::max(ln.bend_end_tick, end_tick);
                    }
                }
            }

            check_budget(sizeof(LoweredNote) + ln.bend_points.size() * sizeof(BendPoint));
            lowered_notes.push_back(std::move(ln));
        }

        // Emit merged overlapping/nested sostenuto pedal spans.
        for (auto &[key, spans] : sostenuto_spans) {
            if (spans.empty()) continue;
            std::sort(spans.begin(), spans.end(), [](const SostenutoSpan &a, const SostenutoSpan &b) {
                if (a.start != b.start) return a.start < b.start;
                return a.duration < b.duration;
            });
            muse::mpe::timestamp_t cur_start = spans[0].start;
            muse::mpe::timestamp_t cur_end = spans[0].start + spans[0].duration;
            for (std::size_t i = 1; i < spans.size(); ++i) {
                const muse::mpe::timestamp_t s = spans[i].start;
                const muse::mpe::timestamp_t e = s + spans[i].duration;
                if (s < cur_end || s == cur_start) {
                    cur_end = std::max(cur_end, e);
                } else {
                    emit(key.first, time_map.timestamp_to_smf(cur_start, false), EventOrder::CONTROL,
                         MidiEvent(ME_CONTROLLER, key.second, CTRL_SOSTENUTO, 127));
                    emit(key.first, time_map.timestamp_to_smf(cur_end, true), EventOrder::CONTROL,
                         MidiEvent(ME_CONTROLLER, key.second, CTRL_SOSTENUTO, 0));
                    cur_start = s;
                    cur_end = e;
                }
            }
            emit(key.first, time_map.timestamp_to_smf(cur_start, false), EventOrder::CONTROL,
                 MidiEvent(ME_CONTROLLER, key.second, CTRL_SOSTENUTO, 127));
            emit(key.first, time_map.timestamp_to_smf(cur_end, true), EventOrder::CONTROL,
                 MidiEvent(ME_CONTROLLER, key.second, CTRL_SOSTENUTO, 0));
        }

        // Emit CC11 expression curves across every (track, channel) target used by each layer.
        if (use_snd && !dynamic_layer_targets.empty()) {
            for (const muse::mpe::DynamicLayer &layer : events.dynamic_layers) {
                check_budget();
                const auto target_it = dynamic_layer_targets.find(layer.layer);
                if (target_it == dynamic_layer_targets.end()) continue;
                const std::span<const muse::mpe::DynamicPoint> curve = events.layer_points(layer);
                if (curve.empty()) continue;

                std::optional<int> last_level;
                muse::mpe::resample_curve(curve, k_dynamic_step_us, [&](muse::mpe::timestamp_t t, muse::real_t normalized) -> bool {
                    const int level = expression_level(muse::mpe::dynamic_level_from_normalized(normalized));
                    if (last_level == level) return true;
                    last_level = level;
                    const int tick = time_map.timestamp_to_smf(t, false);
                    for (const auto &[target_track, channel] : target_it->second) {
                        emit(target_track, tick, EventOrder::CONTROL,
                             MidiEvent(ME_CONTROLLER, channel, CTRL_EXPRESSION, static_cast<uchar>(level)));
                    }
                    return true;
                });
            }
        }
    }

    // Phase B: Isolate overlapping bent notes onto free melodic channels so shared-channel pitch bends
    // never detune concurrent unbent notes or collide on pitch-wheel center resets.
    struct TickInterval {
        int start = 0;
        int end = 0;
    };

    muse::FlatMap<int, std::vector<std::size_t>> bent_notes_by_port;
    muse::FlatMap<std::pair<int, uint8_t>, std::vector<TickInterval>> unbent_spans;
    for (std::size_t i = 0; i < lowered_notes.size(); ++i) {
        const LoweredNote &n = lowered_notes[i];
        if (!n.has_start) continue;
        if (n.has_non_center_bend && n.base_channel != 9) {
            bent_notes_by_port[n.port].push_back(i);
        } else {
            const int end = n.has_end ? n.end_tick : n.start_tick + 1;
            unbent_spans[{n.port, n.base_channel}].push_back(TickInterval{n.start_tick, end});
        }
    }

    struct DynamicChannelState {
        int free_at_tick = 0;
        bool initialized = false;
        const InstrChannel *last_instr = nullptr;
    };

    for (auto &[port, indices] : bent_notes_by_port) {
        check_budget();
        std::sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
            if (lowered_notes[a].start_tick != lowered_notes[b].start_tick) {
                return lowered_notes[a].start_tick < lowered_notes[b].start_tick;
            }
            return a < b;
        });

        const uint16_t reserved =
            static_cast<uint16_t>((static_port_channels.contains(port) ? static_port_channels[port] : 0u) | (1u << 9));
        std::array<int, 16> base_bend_busy_until{};
        std::array<DynamicChannelState, 16> dyn_channels{};

        for (const std::size_t idx : indices) {
            check_budget();
            LoweredNote &n = lowered_notes[idx];
            const int n_end = n.has_end ? std::max(n.end_tick, n.bend_end_tick) : std::max(n.start_tick + 1, n.bend_end_tick);

            bool needs_dynamic = base_bend_busy_until[n.base_channel] > n.start_tick;
            if (!needs_dynamic) {
                if (const auto it = unbent_spans.find({port, n.base_channel}); it != unbent_spans.end()) {
                    for (const TickInterval &u : it->second) {
                        if (u.start < n_end && n.start_tick < u.end) {
                            needs_dynamic = true;
                            break;
                        }
                    }
                }
            }

            if (!needs_dynamic) {
                base_bend_busy_until[n.base_channel] = std::max(base_bend_busy_until[n.base_channel], n_end);
                continue;
            }

            std::optional<uint8_t> allocated;
            for (uint8_t c = 0; c < 16; ++c) {
                if ((reserved & (1u << c)) != 0) continue;
                if (dyn_channels[c].free_at_tick <= n.start_tick) {
                    allocated = c;
                    break;
                }
            }

            if (!allocated.has_value()) {
                base_bend_busy_until[n.base_channel] = std::max(base_bend_busy_until[n.base_channel], n_end);
                continue;
            }

            const uint8_t dyn_ch = *allocated;
            DynamicChannelState &dstate = dyn_channels[dyn_ch];
            n.assigned_channel = dyn_ch;
            dstate.free_at_tick = n_end;

            const bool first_init = !dstate.initialized;
            const bool instr_changed = dstate.last_instr != n.instr_channel;
            if (first_init || instr_changed) {
                emit_channel_setup(n.track_idx, n.start_tick, EventOrder::DYN_SETUP, dyn_ch, n.instr_channel,
                                   /*include_bank=*/true, /*reset_and_rpn=*/first_init);
                dstate.initialized = true;
                dstate.last_instr = n.instr_channel;
            }

            if (n.use_snd && !n.dynamic_curve.empty()) {
                const int init_level = expression_level(
                    muse::mpe::dynamic_level_from_normalized(muse::mpe::evaluate_curve_at(n.dynamic_curve, n.actual_timestamp)));
                emit(n.track_idx, n.start_tick, EventOrder::CONTROL,
                     MidiEvent(ME_CONTROLLER, dyn_ch, CTRL_EXPRESSION, static_cast<uchar>(init_level)));
                if (n.has_end && n.actual_duration > 0) {
                    const muse::mpe::timestamp_t end_us = n.actual_timestamp + n.actual_duration;
                    int last_level = init_level;
                    muse::mpe::resample_curve(n.dynamic_curve, k_dynamic_step_us,
                                              [&](muse::mpe::timestamp_t t, muse::real_t normalized) -> bool {
                                                  if (t <= n.actual_timestamp) return true;
                                                  if (t >= end_us) return false;
                                                  const int level = expression_level(muse::mpe::dynamic_level_from_normalized(normalized));
                                                  if (level != last_level) {
                                                      last_level = level;
                                                      emit(n.track_idx, time_map.timestamp_to_smf(t, false), EventOrder::CONTROL,
                                                           MidiEvent(ME_CONTROLLER, dyn_ch, CTRL_EXPRESSION, static_cast<uchar>(level)));
                                                  }
                                                  return true;
                                              });
                }
            }
        }
    }

    for (const LoweredNote &n : lowered_notes) {
        check_budget();
        if (n.has_start) {
            emit(n.track_idx, n.start_tick, EventOrder::NOTE_ON, MidiEvent(ME_NOTEON, n.assigned_channel, n.midi_pitch, n.velocity));
        }
        if (n.has_end) {
            emit(n.track_idx, n.end_tick, EventOrder::NOTE_OFF, MidiEvent(ME_NOTEON, n.assigned_channel, n.midi_pitch, 0));
        }
        for (const BendPoint &bp : n.bend_points) {
            emit(n.track_idx, bp.tick, EventOrder::CONTROL, make_pitch_bend(n.assigned_channel, bp.value));
        }
        if (n.bend_reset_tick.has_value()) {
            emit(n.track_idx, *n.bend_reset_tick, EventOrder::BEND_RESET, make_pitch_bend(n.assigned_channel, k_pitch_bend_center));
        }
    }

    // Phase C: Reconcile overlapping same-pitch unisons across all tracks sharing (port, channel, pitch).
    reconcile_unisons(tracks, pending);

    // Phase D: Sort each track canonically by (tick, order) and populate MidiFile.
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        check_budget();
        auto &list = pending[i];
        std::stable_sort(list.begin(), list.end(), [](const PendingEvent &a, const PendingEvent &b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return static_cast<uint8_t>(a.order) < static_cast<uint8_t>(b.order);
        });
        for (const PendingEvent &item : list) {
            if (item.discard) continue;
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
