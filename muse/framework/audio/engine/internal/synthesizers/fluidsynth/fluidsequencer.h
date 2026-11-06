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

#include "midi/midievent.h"
#include "mpe/events.h"

#include "../../abstracteventsequencer.h"
#include "soundmapping.h"

namespace muse::audio::synth {
class FluidSequencer : public engine::AbstractEventSequencer<midi::Event> {
public:
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
    void update_off_stream_events(const mpe::PlaybackEventsMap &events) override;

    using SostenutoTimeAndDurations = std::map<midi::channel_t, std::vector<mpe::TimestampAndDuration>>;

    void add_playback_events(EventSequenceMap &destination, const mpe::PlaybackEventsMap &events, const bool is_main_stream);
    void add_dynamic_events(EventSequenceMap &destination, const mpe::DynamicAutomationLayers &layers);
    void add_note_event(EventSequenceMap &destination, const mpe::NoteEvent &note_event,
                        SostenutoTimeAndDurations &sostenuto_time_and_durations, const bool is_main_stream);
    void add_pedal_event(EventSequenceMap &destination, const mpe::ArticulationMeta &meta, const midi::channel_t channel_idx);
    void add_control_change_event(EventSequenceMap &destination, const mpe::timestamp_t timestamp, const mpe::ControllerChangeEvent &event);
    void add_control_change(EventSequenceMap &destination, const mpe::timestamp_t timestamp, const int midi_control_idx,
                            const midi::channel_t channel_idx, const uint32_t value);
    void add_pitch_curve(EventSequenceMap &destination, const mpe::NoteEvent &note_event, const mpe::ArticulationMeta &art_meta,
                         const midi::channel_t channel_idx);
    void add_pitch_bend(EventSequenceMap &destination, const mpe::timestamp_t timestamp, const midi::channel_t channel_idx,
                        const uint32_t value);
    void add_sostenuto_events(EventSequenceMap &destination, const SostenutoTimeAndDurations &sostenuto_time_and_durations);

    midi::channel_t resolve_channel(const mpe::NoteEvent &note_event) const;
    midi::note_idx_t note_index(const mpe::pitch_level_t pitch_level) const;
    midi::tuning_t note_tuning(const mpe::NoteEvent &note_event, const int note_idx) const;
    midi::velocity_t note_velocity(const mpe::NoteEvent &note_event) const;
    int expression_level(const mpe::dynamic_level_t dynamic_level) const;
    int pitch_bend_level(const mpe::pitch_level_t pitch_level) const;

    mutable ChannelMap channels_;
    bool use_dynamic_events_ = false;
    int last_staff_ = -1;
};
} // namespace muse::audio::synth

#endif // MUSE_AUDIO_FLUIDSEQUENCER_H
