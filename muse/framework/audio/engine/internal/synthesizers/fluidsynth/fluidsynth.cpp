/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#include "fluidsynth.h"

#include <fluidsynth.h>

#include "audio/common/audioerrors.h"
#include "audio/common/audiotypes.h"
#include "audio/engine/internal/offlinecancellation.h"
#include "audio/engine/internal/offlinetiming.h"
#include "audio/engine/internal/playbackcapture.h"
#include "log.h"
#include "sfcachedloader.h"

#undef NO_ERROR

using namespace muse;
using namespace muse::midi;
using namespace muse::audio;
using namespace muse::audio::synth;
using namespace muse::mpe;

void muse::audio::synth::release_sound_font_cache() { SoundFontCache::instance()->clear_unused(); }

static constexpr bool fluid_debug = false;

static constexpr double fluid_global_volume_gain = 4.8;
static constexpr int default_midi_volume = 100;
static constexpr msecs_t min_note_length = msecs_t::make(10);

/// @note
///  Fluid does not support MONO, so they start counting audio channels from 1, which means "1 pair of audio channels"
/// @see https://www.fluidsynth.org/api/settings_synth.html
static constexpr unsigned int fluid_audio_channels_pair = 1;
static constexpr unsigned int fluid_audio_channels_count = ::fluid_audio_channels_pair * 2;

static constexpr bool staff_to_midiout_channel = true;

struct muse::audio::synth::Fluid {
    fluid_settings_t *settings = nullptr;
    fluid_synth_t *synth = nullptr;

    ~Fluid() {
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }
};

FluidSynth::FluidSynth(const AudioSourceParams &params, const engine::IAudioEngineConfiguration &config, midi::IMidiOutPort *midi_out_port)
    : AbstractSynthesizer(params, config) {
    fluid_ = std::make_shared<Fluid>();
    midi_out_port_ = midi_out_port;
}

bool FluidSynth::is_valid() const { return fluid_->synth != nullptr; }

Ret FluidSynth::init(const OutputSpec &spec) {
    auto fluid_log_out = [](int level, const char *message, void *) {
#undef LOG_TAG
#define LOG_TAG "FluidSynth"
        switch (level) {
            case FLUID_PANIC:
            case FLUID_ERR: {
                LOGE() << message;
            } break;
            case FLUID_WARN: {
                LOGW() << message;
            } break;
            case FLUID_INFO: {
                LOGI() << message;
            } break;
            case FLUID_DBG: {
                if (::fluid_debug) {
                    LOGD() << message;
                }
            } break;
        }

#undef LOG_TAG
#define LOG_TAG CLASSFUNC
    };

    fluid_set_log_function(FLUID_PANIC, fluid_log_out, nullptr);
    fluid_set_log_function(FLUID_ERR, fluid_log_out, nullptr);
    fluid_set_log_function(FLUID_WARN, fluid_log_out, nullptr);
    fluid_set_log_function(FLUID_INFO, fluid_log_out, nullptr);
    fluid_set_log_function(FLUID_DBG, fluid_log_out, nullptr);

    output_spec_ = spec;

    fluid_->settings = new_fluid_settings();
    fluid_settings_setnum(fluid_->settings, "synth.gain", ::fluid_global_volume_gain);
    fluid_settings_setint(fluid_->settings, "synth.audio-channels", ::fluid_audio_channels_pair); // 1 pair of audio channels
    fluid_settings_setint(fluid_->settings, "synth.lock-memory", 0);
    fluid_settings_setint(fluid_->settings, "synth.threadsafe-api", 0);
    fluid_settings_setint(fluid_->settings, "synth.midi-channels", k_fluid_channel_count);
    fluid_settings_setint(fluid_->settings, "synth.dynamic-sample-loading", 1);
    fluid_settings_setint(fluid_->settings, "synth.polyphony", 512);

    if (spec.sample_rate > 0) {
        fluid_settings_setnum(fluid_->settings, "synth.sample-rate", static_cast<double>(spec.sample_rate));
    }

    fluid_settings_setint(fluid_->settings, "synth.min-note-length", ::min_note_length);

    fluid_settings_setint(fluid_->settings, "synth.chorus.active", 0);
    fluid_settings_setint(fluid_->settings, "synth.reverb.active", 0);
    fluid_settings_setint(fluid_->settings, "synth.iir-lowpass-filter.active", config()->use_sound_font_low_pass_filter() ? 1 : 0);

    fluid_settings_setstr(fluid_->settings, "audio.sample-format", "float");

    create_fluid_instance();

    sequencer_.set_on_off_stream_flushed([this]() { flush_sound_requested_ = true; });

    LOGD() << "synth inited\n";
    return true;
}

void FluidSynth::create_fluid_instance() {
    OfflineTimer timer(OfflineTiming::current().create);
    fluid_->synth = new_fluid_synth(fluid_->settings);

    fluid_sfloader_t *sfloader = new_fluid_sfloader(load_sound_font, delete_fluid_sfloader);

    fluid_sfloader_set_data(sfloader, fluid_->settings);
    fluid_synth_add_sfloader(fluid_->synth, sfloader);
}

void FluidSynth::do_flush_sound() {
    IF_ASSERT_FAILED(fluid_->synth) { return; }

    fluid_synth_all_notes_off(fluid_->synth, -1);

    const int channel_count = static_cast<int>(sequencer_.channels().channel_count());
    for (int i = 0; i < channel_count; ++i) {
        set_controller_value(i, midi::sustain_pedal_controller, 0);
        set_controller_value(i, midi::sostenuto_pedal_controller, 0);
        set_pitch_bend(i, 8192);
    }

    if (midi_out_port_ && midi_out_port_->is_connected()) {
        // Send all notes off to connected midi ports.
        // Room for improvement:
        // - We could record which groups/channels we sent something or which channels were scheduled in the sequencer.
        // - We could send all events at once.
        int lower_bound = 0;
        int upper_bound = 0;
        if (::staff_to_midiout_channel) {
            int channel = sequencer_.last_staff();
            if (channel < 0) {
                return;
            }
            channel = channel % k_fluid_channel_count;
            lower_bound = channel;
            upper_bound = channel + 1;
        } else {
            upper_bound = min(channel_count, static_cast<int>(k_fluid_channel_count));
        }
        for (int i = lower_bound; i < upper_bound; i++) {
            muse::midi::Event e(muse::midi::Event::Opcode::CONTROL_CHANGE, muse::midi::Event::MessageType::CHANNEL_VOICE20);
            e.set_channel(i);
            e.set_index(123); // CC#123 = All notes off
            midi_out_port_->send_event(e);
        }
        for (int i = lower_bound; i < upper_bound; i++) {
            muse::midi::Event e(muse::midi::Event::Opcode::CONTROL_CHANGE, muse::midi::Event::MessageType::CHANNEL_VOICE20);
            e.set_channel(i);
            e.set_index(midi::sustain_pedal_controller);
            e.set_data(0);
            midi_out_port_->send_event(e);
        }
        for (int i = lower_bound; i < upper_bound; i++) {
            muse::midi::Event e(muse::midi::Event::Opcode::PITCH_BEND, muse::midi::Event::MessageType::CHANNEL_VOICE20);
            e.set_channel(i);
            e.set_data(0x80000000);
            midi_out_port_->send_event(e);
        }
    }
}

bool FluidSynth::handle_event(const midi::Event &event) {
    int ret = FLUID_OK;
    switch (event.opcode()) {
        case Event::Opcode::NOTE_ON: {
            // fluid_synth_noteon expects 0...127
            ret = fluid_synth_noteon(fluid_->synth, event.channel(), event.note(), event.velocity7());
            tuning_.add(event.note(), event.pitch_tuning_cents());
        } break;
        case Event::Opcode::NOTE_OFF: {
            ret = fluid_synth_noteoff(fluid_->synth, event.channel(), event.note());
            tuning_.add(event.note(), event.pitch_tuning_cents());
        } break;
        case Event::Opcode::CONTROL_CHANGE: {
            ret = fluid_synth_cc(fluid_->synth, event.channel(), event.index(), event.data7());
        } break;
        case Event::Opcode::PROGRAM_CHANGE: {
            fluid_synth_program_change(fluid_->synth, event.channel(), event.program());
        } break;
        case Event::Opcode::PITCH_BEND: {
            ret = set_pitch_bend(event.channel(), event.pitch_bend14());
        } break;
        default: {
            LOGD() << "not supported event type: " << event.opcode_string();
            ret = FLUID_FAILED;
        }
    }

    if (midi_out_port_) {
        if (::staff_to_midiout_channel && event.is_channel_voice()) {
            int staff = sequencer_.last_staff();
            if (staff >= 0) {
                int channel = staff % k_fluid_channel_count;
                midi::Event me(event);
                me.set_channel(channel);
                midi_out_port_->send_event(me);
            }
        } else {
            midi_out_port_->send_event(event);
        }
    }

    return ret == FLUID_OK;
}

void FluidSynth::set_mode(const ProcessMode mode) {
    if (mode_ == mode) {
        return;
    }

    AbstractSynthesizer::set_mode(mode);

    sequencer_.set_active(is_mode_playing(mode));
    update_expression_levels();
}

void FluidSynth::set_output_spec(const OutputSpec &spec) {
    if (output_spec_ == spec) {
        return;
    }

    output_spec_ = spec;

    if (fluid_->settings) {
        fluid_settings_setnum(fluid_->settings, "synth.sample-rate", static_cast<double>(spec.sample_rate));
    }

    if (fluid_->synth) {
        delete_fluid_synth(fluid_->synth);
    }

    create_fluid_instance();
    add_sound_fonts(std::vector<io::PathT>(sfont_paths_.cbegin(), sfont_paths_.cend()));

    if (setup_data_.is_valid()) {
        setup_sound(setup_data_);
    }
}

Ret FluidSynth::add_sound_fonts(const std::vector<io::PathT> &sfonts) {
    OfflineTimer timer(OfflineTiming::current().fonts);
    IF_ASSERT_FAILED(fluid_->synth) { return make_ret(Err::SYNTH_NOT_INITED); }

    bool ok = true;
    for (const io::PathT &sfont : sfonts) {
        if (fluid_synth_sfload(fluid_->synth, sfont.c_str(), 0) == FLUID_FAILED) {
            LOGE() << "failed load soundfont: " << sfont;
            ok = false;
            continue;
        }

        LOGI() << "success load soundfont: " << sfont;
        sfont_paths_.insert(sfont);
    }

    return ok ? make_ret(Err::NO_ERROR) : make_ret(Err::SOUND_FONT_FAILED_LOAD);
}

void FluidSynth::set_preset(const std::optional<midi::Program> &preset) { preset_ = preset; }

std::string FluidSynth::name() const { return "Fluid"; }

AudioSourceType FluidSynth::type() const { return AudioSourceType::FLUID; }

void FluidSynth::setup_sound(const PlaybackSetupData &setup_data) {
    IF_ASSERT_FAILED(fluid_->synth) { return; }

#ifdef ENGRAVE_PLAYBACK_HARNESS
#define FLUID_SETUP(function, ...) test::PlaybackCapture::setup_call(&sequencer_, #function, function, fluid_->synth, __VA_ARGS__)
#else
#define FLUID_SETUP(function, ...) function(fluid_->synth, __VA_ARGS__)
#endif
    FLUID_SETUP(fluid_synth_activate_key_tuning, 0, 0, "standard", nullptr, true);

    auto setup_channel = [this](const midi::channel_t channel_idx, const midi::Program &program) {
        if (OfflineCancellation::requested()) return;
        FLUID_SETUP(fluid_synth_set_interp_method, channel_idx, FLUID_INTERP_DEFAULT);
        FLUID_SETUP(fluid_synth_pitch_wheel_sens, channel_idx, 24);
        FLUID_SETUP(fluid_synth_bank_select, channel_idx, program.bank);
        FLUID_SETUP(fluid_synth_program_change, channel_idx, program.program);
        FLUID_SETUP(fluid_synth_cc, channel_idx, 7, ::default_midi_volume);
        FLUID_SETUP(fluid_synth_cc, channel_idx, muse::midi::expression_controller, sequencer_.natural_expression_level());
        FLUID_SETUP(fluid_synth_cc, channel_idx, 74, 0);
        FLUID_SETUP(fluid_synth_set_portamento_mode, channel_idx, FLUID_CHANNEL_PORTAMENTO_MODE_EACH_NOTE);
        FLUID_SETUP(fluid_synth_set_legato_mode, channel_idx, FLUID_CHANNEL_LEGATO_MODE_RETRIGGER);
        FLUID_SETUP(fluid_synth_activate_tuning, channel_idx, 0, 0, 0);
    };

#undef FLUID_SETUP

    sequencer_.set_on_channel_added(setup_channel);

    sequencer_.init(setup_data, preset_, setup_data.supports_single_note_dynamics, k_fluid_channel_count);

    for (const auto &voice : sequencer_.channels().data()) {
        for (const auto &pair : voice.second) {
            const ChannelMap::ChannelMapping &channel_mapping = pair.second;
            setup_channel(channel_mapping.first, channel_mapping.second);
        }
    }
}

void FluidSynth::setup_events(const mpe::PlaybackData &playback_data) { sequencer_.load(playback_data); }

void FluidSynth::flush_sound() {
    sequencer_.flush_offstream();
    flush_sound_requested_ = true;
}

TimePosition FluidSynth::playback_position() const {
    return TimePosition::from_time(muse::usecs_to_secs(sequencer_.playback_position().raw()), output_spec_.sample_rate);
}

void FluidSynth::set_playback_position(const TimePosition &position) {
    IF_ASSERT_FAILED(position.is_valid()) { return; }

    //! NOTE Don't trust that msecs_t is used everywhere here,
    // in fact, usecs_t (microseconds) is stored there.
    const usecs_t usecs = muse::secs_to_usecs(position.time());
    sequencer_.set_playback_position(msecs_t(usecs.raw()));

    if (sequencer_.is_active()) {
        update_expression_levels();
    }
}

samples_t FluidSynth::process(float *buffer, samples_t samples_per_channel) {
    IF_ASSERT_FAILED(samples_per_channel > 0) { return 0; }

    if (OfflineCancellation::requested()) return 0;
    if (flush_sound_requested_) {
        do_flush_sound();
        flush_sound_requested_ = false;
    }

    const msecs_t next_msecs = samples_to_msecs(samples_per_channel, output_spec_.sample_rate);
    const engine::Buf<engine::ChunkSlice> &plan = sequencer_.move_playback_forward(next_msecs);
#ifdef ENGRAVE_PLAYBACK_HARNESS
    if (test::PlaybackCapture::synth)
        *test::PlaybackCapture::synth << "process " << test::PlaybackCapture::id(&sequencer_) << ' ' << samples_per_channel << ' '
                                      << sequencer_.playback_position().raw() << '\n';
#endif
    samples_t sample_offset = 0;

    for (std::uint32_t i = 0; i < plan.size(); ++i) {
        if (OfflineCancellation::requested()) return 0;
        const engine::ChunkSlice &slice = plan[i];
        const std::span<const engine::TimedMidiEvent> events = sequencer_.slice_events(slice);
        samples_t duration_in_samples = samples_per_channel - sample_offset;

        if (i + 1 < plan.size()) {
            msecs_t duration = plan[i + 1].at - slice.at;
            duration_in_samples = micro_secs_to_samples(duration, output_spec_.sample_rate);
        }

        IF_ASSERT_FAILED(sample_offset + duration_in_samples <= samples_per_channel) { break; }

#ifdef ENGRAVE_PLAYBACK_HARNESS
        if (test::PlaybackCapture::synth) {
            auto &out = *test::PlaybackCapture::synth;
            out << "slice " << slice.at << ' ' << sample_offset << ' ' << duration_in_samples << ' ' << events.size() << '\n';
            for (const engine::TimedMidiEvent &item : events) {
                const auto *words = item.event.midi20_words();
                out << "slice_midi " << words[0] << ' ' << words[1] << ' ' << words[2] << ' ' << words[3] << '\n';
            }
        }
#endif
        if (!process_sequence(events, duration_in_samples, buffer + sample_offset * ::fluid_audio_channels_count)) {
            return 0;
        }

        sample_offset += duration_in_samples;
    }

    return samples_per_channel;
}

bool FluidSynth::process_sequence(const std::span<const engine::TimedMidiEvent> sequence, const samples_t samples, float *buffer) {
    OfflineCancellation::SynthesisScope synthesis_scope;
    if (!sequence.empty()) {
        tuning_.reset();
    }

    for (const engine::TimedMidiEvent &item : sequence) {
        if (OfflineCancellation::requested()) return false;
        handle_event(item.event);
    }

    if (OfflineCancellation::requested()) return false;
    fluid_synth_tune_notes(fluid_->synth, 0, 0, tuning_.size(), tuning_.keys.data(), tuning_.pitches.data(), true);

    if (samples == 0) {
        return true;
    }

    // FluidSynth retains its 64-frame internal buffer across calls. Slice only
    // the sink copy, leaving event timing and upstream 4096-frame DSP unchanged.
    for (samples_t offset = 0; offset < samples;) {
        if (OfflineCancellation::requested()) return false;
        const samples_t count = std::min<samples_t>(64, samples - offset);
        float *out = buffer + offset * ::fluid_audio_channels_count;
        if (fluid_synth_write_float(fluid_->synth, count, out, 0, ::fluid_audio_channels_count, out, 1, ::fluid_audio_channels_count) !=
            FLUID_OK)
            return false;
        offset += count;
    }
    return true;
}

void FluidSynth::update_expression_levels() {
    IF_ASSERT_FAILED(fluid_->synth) { return; }

    std::vector<FluidSequencer::ChannelExpressionLevel> levels;
    if (sequencer_.is_active()) {
        levels = sequencer_.current_expression_levels();
    }

    const int natural_level = sequencer_.natural_expression_level();
    const midi::channel_t channel_count = sequencer_.channels().channel_count();

    if (levels.empty()) {
        for (midi::channel_t i = 0; i < channel_count; ++i) {
            fluid_synth_cc(fluid_->synth, i, muse::midi::expression_controller, natural_level);
        }
        return;
    }

    std::vector<bool> covered(channel_count, false);

    for (const FluidSequencer::ChannelExpressionLevel &expression_level : levels) {
        fluid_synth_cc(fluid_->synth, expression_level.channel, muse::midi::expression_controller, expression_level.level);
        if (expression_level.channel < channel_count) {
            covered[expression_level.channel] = true;
        }
    }

    for (midi::channel_t i = 0; i < channel_count; ++i) {
        if (!covered[i]) {
            fluid_synth_cc(fluid_->synth, i, muse::midi::expression_controller, natural_level);
        }
    }
}

int FluidSynth::set_controller_value(int channel, int ctrl, int value) {
    int current_value = 0;
    fluid_synth_get_cc(fluid_->synth, channel, ctrl, &current_value);

    if (value == current_value) {
        return FLUID_OK;
    }

    return fluid_synth_cc(fluid_->synth, channel, ctrl, value);
}

int FluidSynth::set_pitch_bend(int channel, int pitch_bend) {
    int current_value = 0;
    fluid_synth_get_pitch_bend(fluid_->synth, channel, &current_value);

    if (pitch_bend == current_value) {
        return FLUID_OK;
    }

    return fluid_synth_pitch_bend(fluid_->synth, channel, pitch_bend);
}
