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

#include "fluidsequencer.h"
#include "audio/engine/internal/offlinecancellation.h"

#include <set>

#include "global/interpolation.h"

#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::synth;
using namespace muse::midi;
using namespace muse::mpe;

static constexpr uint32_t ctrl_on = 127;
static constexpr uint32_t ctrl_off = 0;

void FluidSequencer::init(const PlaybackSetupData &setup_data, const std::optional<midi::Program> &program_override,
                          bool use_dynamic_events, const channel_t max_channels) {
    TRACEFUNC;

    channels_.init(setup_data, program_override, max_channels);
    use_dynamic_events_ = use_dynamic_events;
}

bool FluidSequencer::uses_dynamic_events() const { return use_dynamic_events_; }

std::vector<FluidSequencer::ChannelExpressionLevel> FluidSequencer::current_expression_levels() const {
    std::vector<ChannelExpressionLevel> result;

    if (!use_dynamic_events_) {
        return result;
    }

    for (const auto &[layer_idx, curve] : playback_data_.dynamics) {
        if (curve.empty()) {
            continue;
        }

        auto voice_it = channels_.data().find(layer_idx);
        if (voice_it == channels_.data().cend()) {
            continue;
        }

        const int level = expression_level(mpe::dynamic_level_from_normalized(mpe::evaluate_curve_at(curve, playback_position_)));
        for (const auto &[_, mapping] : voice_it->second) {
            result.push_back({mapping.first, level});
        }
    }

    return result;
}

int FluidSequencer::natural_expression_level() const {
    static const int natural_exp_lvl = expression_level(dynamic_level_from_type(DynamicType::NATURAL));
    return natural_exp_lvl;
}

void FluidSequencer::update_main_stream_events(const mpe::PlaybackEventsMap &events, const mpe::DynamicAutomationLayers &dynamics) {
    main_stream_events_.clear();

    if (on_main_stream_flushed_) {
        on_main_stream_flushed_();
    }

    add_playback_events(main_stream_events_, events, true /*isMainStream*/);

    if (use_dynamic_events_) {
        add_dynamic_events(main_stream_events_, dynamics);
    }

    update_main_sequence_iterator();
}

void FluidSequencer::update_off_stream_events(const mpe::PlaybackEventsMap &events) {
    add_playback_events(off_stream_events_, events, false /*isMainStream*/);
    update_off_sequence_iterator();
}

const ChannelMap &FluidSequencer::channels() const { return channels_; }

void FluidSequencer::set_on_channel_added(std::function<void(midi::channel_t, const midi::Program &)> on_channel_added) {
    channels_.on_channel_added = std::move(on_channel_added);
}

int FluidSequencer::last_staff() const { return last_staff_; }

void FluidSequencer::add_playback_events(EventSequenceMap &destination, const mpe::PlaybackEventsMap &events, const bool is_main_stream) {
    OfflineCancellation::SequencerScope phase;
    SostenutoTimeAndDurations sostenuto_time_and_durations;

    for (const auto &pair : events) {
        for (const mpe::PlaybackEvent &event : pair.second) {
            if (OfflineCancellation::requested()) return;
            if (std::holds_alternative<mpe::NoteEvent>(event)) {
                add_note_event(destination, std::get<mpe::NoteEvent>(event), sostenuto_time_and_durations, is_main_stream);
            } else if (std::holds_alternative<mpe::ControllerChangeEvent>(event)) {
                add_control_change_event(destination, pair.first, std::get<mpe::ControllerChangeEvent>(event));
            }
        }
    }

    add_sostenuto_events(destination, sostenuto_time_and_durations);
}

void FluidSequencer::add_dynamic_events(EventSequenceMap &destination, const mpe::DynamicAutomationLayers &layers) {
    constexpr mpe::timestamp_t step_interval_us = 30000;

    for (const auto &[layer_idx, curve] : layers) {
        if (OfflineCancellation::requested()) return;
        auto voice_it = channels_.data().find(layer_idx);
        if (voice_it == channels_.data().cend()) {
            continue;
        }

        std::set<channel_t> channels;
        for (const auto &[_, mapping] : voice_it->second) {
            channels.insert(mapping.first);
        }

        std::optional<int> last_level;

        mpe::resample_curve(curve, step_interval_us, [&](mpe::timestamp_t t, muse::real_t normalized) {
            const int level = expression_level(mpe::dynamic_level_from_normalized(normalized));
            if (last_level == level) {
                return;
            }
            last_level = level;

            for (const channel_t channel_idx : channels) {
                midi::Event event(Event::Opcode::CONTROL_CHANGE, Event::MessageType::CHANNEL_VOICE10);
                event.set_channel(channel_idx);
                event.set_index(midi::expression_controller);
                event.set_data(level);
                destination[t].emplace_back(event);
            }
        });
    }
}

void FluidSequencer::add_note_event(EventSequenceMap &destination, const mpe::NoteEvent &note_event,
                                    SostenutoTimeAndDurations &sostenuto_time_and_durations, const bool is_main_stream) {
    const ArrangementContext &arrangement_ctx = note_event.arrangement_ctx();
    const channel_t channel_idx = resolve_channel(note_event);
    const note_idx_t note_idx = note_index(note_event.pitch_ctx().nominal_pitch_level);
    const velocity_t velocity = note_velocity(note_event);
    const tuning_t tuning = note_tuning(note_event, note_idx);

    // Assumption: 1:1 mapping between staff and instrument and FluidSynth and FluidSequencer instances.
    // So staffLayerIndex is constant. It changes only when moving staffs.
    last_staff_ = note_event.arrangement_ctx().staff_layer_index;

    if (arrangement_ctx.has_start()) {
        if (use_dynamic_events_ && !is_main_stream) {
            midi::Event expression_event(Event::Opcode::CONTROL_CHANGE, Event::MessageType::CHANNEL_VOICE10);
            expression_event.set_channel(channel_idx);
            expression_event.set_index(midi::expression_controller);
            expression_event.set_data(expression_level(note_event.expression_ctx().nominal_dynamic_level));
            destination[arrangement_ctx.actual_timestamp].emplace_back(std::move(expression_event));
        }

        midi::Event note_on(Event::Opcode::NOTE_ON, Event::MessageType::CHANNEL_VOICE20);
        note_on.set_channel(channel_idx);
        note_on.set_note(note_idx);
        note_on.set_velocity16(velocity);
        note_on.set_pitch_note(note_idx, tuning);

        destination[arrangement_ctx.actual_timestamp].emplace_back(std::move(note_on));
    }

    if (arrangement_ctx.has_end()) {
        midi::Event note_off(Event::Opcode::NOTE_OFF, Event::MessageType::CHANNEL_VOICE20);
        note_off.set_channel(channel_idx);
        note_off.set_note(note_idx);
        note_off.set_pitch_note(note_idx, tuning);

        const timestamp_t timestamp_to = arrangement_ctx.actual_timestamp + note_event.arrangement_ctx().actual_duration;
        destination[timestamp_to].emplace_back(std::move(note_off));
    }

    for (const auto &art_pair : note_event.expression_ctx().articulations) {
        if (art_pair.first == ArticulationType::STANDARD) {
            continue;
        }

        const mpe::ArticulationMeta &meta = art_pair.second.meta;

        if (!note_event.pitch_ctx().pitch_curve.empty() && muse::contains(::muse::audio::synth::bend_supported_types, meta.type)) {
            add_pitch_curve(destination, note_event, meta, channel_idx);
            continue;
        }

        if (muse::contains(::muse::audio::synth::sustain_pedal_cc_supported_types, meta.type)) {
            add_pedal_event(destination, meta, channel_idx);
            continue;
        }

        if (muse::contains(::muse::audio::synth::sostenuto_pedal_cc_supported_types, meta.type)) {
            const mpe::timestamp_t timestamp =
                arrangement_ctx.actual_timestamp + arrangement_ctx.actual_duration * 0.1; // add offset for Sostenuto to take effect
            sostenuto_time_and_durations[channel_idx].push_back(TimestampAndDuration{timestamp, meta.overall_duration});
            continue;
        }
    }
}

void FluidSequencer::add_pedal_event(EventSequenceMap &destination, const mpe::ArticulationMeta &meta, const channel_t channel_idx) {
    //! NOTE: Endless pedals are not currently supported by SND instrument
    //! Otherwise, we will get infinite sustain
    if (use_dynamic_events_ && meta.has_start() && !meta.has_end()) {
        return;
    }

    if (meta.has_start()) {
        add_control_change(destination, meta.timestamp, midi::sustain_pedal_controller, channel_idx, ::ctrl_on);
    }

    if (meta.has_end()) {
        add_control_change(destination, meta.timestamp + meta.overall_duration, midi::sustain_pedal_controller, channel_idx, ::ctrl_off);
    }
}

void FluidSequencer::add_control_change_event(EventSequenceMap &destination, const mpe::timestamp_t timestamp,
                                              const mpe::ControllerChangeEvent &event) {
    const channel_t last_channel_idx = channels_.channel_count();

    for (channel_t channel_idx = 0; channel_idx < last_channel_idx; ++channel_idx) {
        switch (event.type) {
            case mpe::ControllerChangeEvent::MODULATION:
                add_control_change(destination, timestamp, midi::modwheel_controller, channel_idx,
                                   static_cast<uint32_t>(event.val * 127.f));
                break;
            case mpe::ControllerChangeEvent::SUSTAIN_PEDAL_ON_OFF:
                add_control_change(destination, timestamp, midi::sustain_pedal_controller, channel_idx,
                                   static_cast<uint32_t>(event.val * 127.f));
                break;
            case mpe::ControllerChangeEvent::PITCH_BEND:
                add_pitch_bend(destination, timestamp, channel_idx, static_cast<uint32_t>(event.val * 16383.f));
                break;
            case mpe::ControllerChangeEvent::UNDEFINED: break;
        }
    }
}

void FluidSequencer::add_control_change(EventSequenceMap &destination, const mpe::timestamp_t timestamp, const int midi_control_idx,
                                        const channel_t channel_idx, const uint32_t value) {
    EventSequence &events = destination[timestamp];
    for (const EventType &e : events) {
        const midi::Event &midi_event = std::get<midi::Event>(e);
        if (midi_event.opcode() == Event::Opcode::CONTROL_CHANGE && midi_event.channel() == channel_idx &&
            midi_event.index() == static_cast<uint8_t>(midi_control_idx) && midi_event.data() == value) {
            return;
        }
    }

    midi::Event cc(Event::Opcode::CONTROL_CHANGE, Event::MessageType::CHANNEL_VOICE10);
    cc.set_index(midi_control_idx);
    cc.set_channel(channel_idx);
    cc.set_data(value);

    events.emplace_back(cc);
}

void FluidSequencer::add_pitch_curve(EventSequenceMap &destination, const mpe::NoteEvent &note_event, const mpe::ArticulationMeta &art_meta,
                                     const channel_t channel_idx) {
    const timestamp_t note_timestamp_to = note_event.arrangement_ctx().actual_timestamp + note_event.arrangement_ctx().actual_duration;
    const timestamp_t pitch_bend_timestamp_to = std::min(art_meta.timestamp + art_meta.overall_duration, note_timestamp_to);

    add_pitch_bend(destination, pitch_bend_timestamp_to, channel_idx, 8192);

    auto curr_it = note_event.pitch_ctx().pitch_curve.cbegin();
    auto next_it = std::next(curr_it);
    auto end_it = note_event.pitch_ctx().pitch_curve.cend();

    int prev_bend_value = -1;

    for (; next_it != end_it; curr_it = next_it, next_it = std::next(curr_it)) {
        const int curr_value = pitch_bend_level(curr_it->second);
        const int next_value = pitch_bend_level(next_it->second);

        const timestamp_t curr_time = art_meta.timestamp + art_meta.overall_duration * percentage_to_factor(curr_it->first);
        const timestamp_t next_time = art_meta.timestamp + art_meta.overall_duration * percentage_to_factor(next_it->first);

        using namespace muse::interpolation;
        const Point curr_point{static_cast<double>(curr_time), static_cast<double>(curr_value)};
        const Point next_point{static_cast<double>(next_time), static_cast<double>(next_value)};

        //! NOTE: Increasing this number results in fewer points being interpolated
        constexpr mpe::pitch_level_t point_weight = mpe::pitch_level_step / 25;
        size_t point_count = std::abs(next_it->second - curr_it->second) / point_weight;
        point_count = std::max(point_count, size_t(1));

        const std::vector<Point> points = lerp(curr_point, next_point, point_count);

        for (const Point &point : points) {
            const timestamp_t time = static_cast<timestamp_t>(std::round(point.x));
            const int bend_value = static_cast<int>(std::round(point.y));

            if (time < pitch_bend_timestamp_to && bend_value != prev_bend_value) {
                add_pitch_bend(destination, time, channel_idx, bend_value);
            }

            prev_bend_value = bend_value;
        }
    }
}

void FluidSequencer::add_pitch_bend(EventSequenceMap &destination, const mpe::timestamp_t timestamp, const midi::channel_t channel_idx,
                                    const uint32_t value) {
    midi::Event event(Event::Opcode::PITCH_BEND, Event::MessageType::CHANNEL_VOICE10);
    event.set_channel(channel_idx);
    event.set_data(value);
    destination[timestamp].push_back(event);
}

void FluidSequencer::add_sostenuto_events(EventSequenceMap &destination, const SostenutoTimeAndDurations &sostenuto_time_and_durations) {
    for (const auto &channel_pair : sostenuto_time_and_durations) {
        if (OfflineCancellation::requested()) return;
        for (size_t i = 0; i < channel_pair.second.size(); ++i) {
            const TimestampAndDuration &current_tn_d = channel_pair.second.at(i);
            const timestamp_t timestamp_to = current_tn_d.timestamp + current_tn_d.duration;

            add_control_change(destination, current_tn_d.timestamp, midi::sostenuto_pedal_controller, channel_pair.first, ::ctrl_on);

            if (i == channel_pair.second.size() - 1) {
                add_control_change(destination, timestamp_to, midi::sostenuto_pedal_controller, channel_pair.first, ::ctrl_off);
                continue;
            }

            const TimestampAndDuration &next_tn_d = channel_pair.second.at(i + 1);
            if (timestamp_to <= next_tn_d.timestamp) { // handle potential overlap
                add_control_change(destination, timestamp_to, midi::sostenuto_pedal_controller, channel_pair.first, ::ctrl_off);
            }
        }
    }
}

channel_t FluidSequencer::resolve_channel(const mpe::NoteEvent &note_event) const {
    return channels_.resolve_channel_for_event(note_event);
}

note_idx_t FluidSequencer::note_index(const mpe::pitch_level_t pitch_level) const {
    float step_count = mpe::zero_pitch_level_midi_equivalent + pitch_level / static_cast<float>(mpe::pitch_level_step);

    return std::clamp(step_count, 0.f, 127.f);
}

tuning_t FluidSequencer::note_tuning(const mpe::NoteEvent &note_event, const int note_idx) const {
    int semitones_count = note_idx - mpe::zero_pitch_level_midi_equivalent;

    mpe::pitch_level_t tuning_pitch_level = note_event.pitch_ctx().nominal_pitch_level - semitones_count * mpe::pitch_level_step;

    return tuning_pitch_level / static_cast<float>(mpe::pitch_level_step);
}

velocity_t FluidSequencer::note_velocity(const mpe::NoteEvent &note_event) const {
    static constexpr midi::velocity_t max_supported_velocity = std::numeric_limits<midi::velocity_t>::max();

    const mpe::ExpressionContext &expression_ctx = note_event.expression_ctx();

    if (expression_ctx.velocity_override.has_value()) {
        velocity_t velocity = real_round(expression_ctx.velocity_override.value() * max_supported_velocity, 0);
        return std::clamp<velocity_t>(velocity, 0, max_supported_velocity);
    }

    if (use_dynamic_events_) {
        float fraction = expression_ctx.expression_curve.empty() ? 0.5f : expression_ctx.expression_curve.velocity_fraction();
        velocity_t result = real_round(fraction * max_supported_velocity, 0);
        return std::clamp<velocity_t>(result, 0, max_supported_velocity);
    }

    dynamic_level_t dynamic_level = expression_ctx.expression_curve.empty() ? expression_ctx.nominal_dynamic_level :
                                                                              expression_ctx.expression_curve.max_amplitude_level();
    return expression_level(dynamic_level) << 9; // midi::Event::scaleUp(7,16)
}

int FluidSequencer::expression_level(const mpe::dynamic_level_t dynamic_level) const {
    static constexpr mpe::dynamic_level_t min_supported_dynamics_level = mpe::dynamic_level_from_type(DynamicType::PPP);
    static constexpr mpe::dynamic_level_t max_supported_dynamics_level = mpe::dynamic_level_from_type(DynamicType::FFF);
    static constexpr int min_supported_volume = 16;  // MIDI equivalent for PPP
    static constexpr int max_supported_volume = 127; // MIDI equivalent for FFF
    static constexpr int volume_step = 16;

    if (dynamic_level <= min_supported_dynamics_level) {
        return min_supported_volume;
    }

    if (dynamic_level >= max_supported_dynamics_level) {
        return max_supported_volume;
    }

    float step_count = ((dynamic_level - min_supported_dynamics_level) / static_cast<float>(mpe::dynamic_level_step));

    if (dynamic_level == mpe::dynamic_level_from_type(DynamicType::NATURAL)) {
        step_count -= 0.5;
    }

    dynamic_level_t result = real_round(min_supported_volume + (step_count * volume_step), 0);

    return std::clamp((int) result, min_supported_volume, max_supported_volume);
}

int FluidSequencer::pitch_bend_level(const mpe::pitch_level_t pitch_level) const {
    static constexpr int pitch_bend_semitone_step = 4096 / 12;

    float pitch_level_steps = pitch_level / static_cast<float>(mpe::pitch_level_step);

    int offset = pitch_level_steps * pitch_bend_semitone_step;

    return std::clamp(8192 + offset, 0, 16383);
}
