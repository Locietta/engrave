/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#ifndef MUSE_AUDIO_FLUIDSEQUENCER_H
#define MUSE_AUDIO_FLUIDSEQUENCER_H

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "../../abstracteventsequencer.h"
#include "midi/midievent.h"
#include "mpe/events.h"
#include "soundmapping.h"

namespace muse::audio::synth {
struct FluidSequencer : engine::AbstractEventSequencer<midi::Event> {
    void init(const mpe::PlaybackSetupData &setup_data, const std::optional<midi::Program> &program_override, bool use_dynamic_events,
              midi::channel_t max_channels);

    struct ChannelExpressionLevel {
        midi::channel_t channel = 0;
        int level = 0;
    };

    bool uses_dynamic_events() const;
    std::vector<ChannelExpressionLevel> current_expression_levels() const;
    int natural_expression_level() const;

    const ChannelMap &channels() const;
    void set_on_channel_added(std::function<void(midi::channel_t, const midi::Program &)> on_channel_added);
    int last_staff() const;

private:
    void update_main_stream_events(const mpe::PlaybackEventsMap &events, const mpe::DynamicAutomationLayers &dynamics) override;

    using SostenutoTimeAndDurations = std::array<engine::Buf<mpe::TimestampAndDuration>, k_fluid_channel_count>;

    bool add_playback_events(engine::MidiTimeline &destination, const mpe::PlaybackEventsMap &events, bool is_main_stream);
    bool add_dynamic_events(engine::MidiTimeline &destination, const mpe::DynamicAutomationLayers &layers);
    bool add_note_event(engine::MidiTimeline &destination, const mpe::NoteEvent &note_event,
                        SostenutoTimeAndDurations &sostenuto_time_and_durations, bool is_main_stream);
    void add_pedal_event(engine::MidiTimeline &destination, const mpe::ArticulationMeta &meta, midi::channel_t channel_idx);
    void add_control_change_event(engine::MidiTimeline &destination, mpe::timestamp_t timestamp, const mpe::ControllerChangeEvent &event);
    void add_control_change(engine::MidiTimeline &destination, mpe::timestamp_t timestamp, int midi_control_idx,
                            midi::channel_t channel_idx, std::uint32_t value);
    bool add_pitch_curve(engine::MidiTimeline &destination, const mpe::NoteEvent &note_event, const mpe::ArticulationMeta &art_meta,
                         midi::channel_t channel_idx);
    void add_pitch_bend(engine::MidiTimeline &destination, mpe::timestamp_t timestamp, midi::channel_t channel_idx, std::uint32_t value);
    bool add_sostenuto_events(engine::MidiTimeline &destination, const SostenutoTimeAndDurations &sostenuto_time_and_durations);

    midi::channel_t resolve_channel(const mpe::NoteEvent &note_event) const;
    midi::note_idx_t note_index(mpe::pitch_level_t pitch_level) const;
    midi::tuning_t note_tuning(const mpe::NoteEvent &note_event, int note_idx) const;
    midi::velocity_t note_velocity(const mpe::NoteEvent &note_event) const;
    int expression_level(mpe::dynamic_level_t dynamic_level) const;
    int pitch_bend_level(mpe::pitch_level_t pitch_level) const;

    mutable ChannelMap channels_;
    bool use_dynamic_events_ = false;
    int last_staff_ = -1;
};
} // namespace muse::audio::synth

#endif // MUSE_AUDIO_FLUIDSEQUENCER_H
