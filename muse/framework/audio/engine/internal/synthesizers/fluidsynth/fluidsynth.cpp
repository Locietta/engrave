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

#include "audio/engine/internal/offlinecancellation.h"
#include "audio/engine/internal/offlinetiming.h"
#include "fluidsynth.h"

#include <fluidsynth.h>

#include "audio/common/audioerrors.h"
#include "audio/common/audiotypes.h"

#include "sfcachedloader.h"

#include "log.h"

using namespace muse;
using namespace muse::midi;
using namespace muse::audio;
using namespace muse::audio::synth;
using namespace muse::mpe;

void muse::audio::synth::releaseSoundFontCache() { SoundFontCache::instance()->clearUnused(); }

static constexpr bool FLUID_DEBUG = false;

static constexpr double FLUID_GLOBAL_VOLUME_GAIN = 4.8;
static constexpr int DEFAULT_MIDI_VOLUME = 100;
static constexpr msecs_t MIN_NOTE_LENGTH = msecs_t::make(10);

/// @note
///  Fluid does not support MONO, so they start counting audio channels from 1, which means "1 pair of audio channels"
/// @see https://www.fluidsynth.org/api/settings_synth.html
static constexpr unsigned int FLUID_AUDIO_CHANNELS_PAIR = 1;
static constexpr unsigned int FLUID_AUDIO_CHANNELS_COUNT = FLUID_AUDIO_CHANNELS_PAIR * 2;

static constexpr bool STAFF_TO_MIDIOUT_CHANNEL = true;

static constexpr midi::channel_t MIDI_CHANNEL_COUNT = 16;

struct muse::audio::synth::Fluid {
    fluid_settings_t *settings = nullptr;
    fluid_synth_t *synth = nullptr;

    ~Fluid() {
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
    }
};

FluidSynth::FluidSynth(const AudioSourceParams &params, const engine::IAudioEngineConfiguration &config, midi::IMidiOutPort *midiOutPort)
    : AbstractSynthesizer(params, config) {
    fluid_ = std::make_shared<Fluid>();
    midiOutPort_ = midiOutPort;
}

bool FluidSynth::isValid() const { return fluid_->synth != nullptr; }

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
                if (FLUID_DEBUG) {
                    LOGD() << message;
                }
            } break;
        }

#undef LOG_TAG
#define LOG_TAG CLASSFUNC

        if (level < FLUID_DBG) {
            bool debugme = true;
            (void) debugme;
        }
    };

    fluid_set_log_function(FLUID_PANIC, fluid_log_out, nullptr);
    fluid_set_log_function(FLUID_ERR, fluid_log_out, nullptr);
    fluid_set_log_function(FLUID_WARN, fluid_log_out, nullptr);
    fluid_set_log_function(FLUID_INFO, fluid_log_out, nullptr);
    fluid_set_log_function(FLUID_DBG, fluid_log_out, nullptr);

    outputSpec_ = spec;

    fluid_->settings = new_fluid_settings();
    fluid_settings_setnum(fluid_->settings, "synth.gain", FLUID_GLOBAL_VOLUME_GAIN);
    fluid_settings_setint(fluid_->settings, "synth.audio-channels", FLUID_AUDIO_CHANNELS_PAIR); // 1 pair of audio channels
    fluid_settings_setint(fluid_->settings, "synth.lock-memory", 0);
    fluid_settings_setint(fluid_->settings, "synth.threadsafe-api", 0);
    fluid_settings_setint(fluid_->settings, "synth.midi-channels", MIDI_CHANNEL_COUNT);
    fluid_settings_setint(fluid_->settings, "synth.dynamic-sample-loading", 1);
    fluid_settings_setint(fluid_->settings, "synth.polyphony", 512);

    if (spec.sampleRate > 0) {
        fluid_settings_setnum(fluid_->settings, "synth.sample-rate", static_cast<double>(spec.sampleRate));
    }

    fluid_settings_setint(fluid_->settings, "synth.min-note-length", MIN_NOTE_LENGTH);

    fluid_settings_setint(fluid_->settings, "synth.chorus.active", 0);
    fluid_settings_setint(fluid_->settings, "synth.reverb.active", 0);
    fluid_settings_setint(fluid_->settings, "synth.iir-lowpass-filter.active", config()->useSoundFontLowPassFilter() ? 1 : 0);

    fluid_settings_setstr(fluid_->settings, "audio.sample-format", "float");

    createFluidInstance();

    sequencer_.setOnOffStreamFlushed([this]() { flushSoundRequested_ = true; });

    LOGD() << "synth inited\n";
    return true;
}

void FluidSynth::createFluidInstance() {
    OfflineTimer timer(OfflineTiming::current().create);
    fluid_->synth = new_fluid_synth(fluid_->settings);

    fluid_sfloader_t *sfloader = new_fluid_sfloader(loadSoundFont, delete_fluid_sfloader);

    fluid_sfloader_set_data(sfloader, fluid_->settings);
    fluid_synth_add_sfloader(fluid_->synth, sfloader);
}

void FluidSynth::doFlushSound() {
    IF_ASSERT_FAILED(fluid_->synth) { return; }

    fluid_synth_all_notes_off(fluid_->synth, -1);

    const int channelCount = static_cast<int>(sequencer_.channels().channelCount());
    for (int i = 0; i < channelCount; ++i) {
        setControllerValue(i, midi::SUSTAIN_PEDAL_CONTROLLER, 0);
        setControllerValue(i, midi::SOSTENUTO_PEDAL_CONTROLLER, 0);
        setPitchBend(i, 8192);
    }

    if (midiOutPort_ && midiOutPort_->isConnected()) {
        // Send all notes off to connected midi ports.
        // Room for improvement:
        // - We could record which groups/channels we sent something or which channels were scheduled in the sequencer.
        // - We could send all events at once.
        int lowerBound = 0;
        int upperBound = 0;
        if (STAFF_TO_MIDIOUT_CHANNEL) {
            int channel = sequencer_.lastStaff();
            if (channel < 0) {
                return;
            }
            channel = channel % 16;
            lowerBound = channel;
            upperBound = channel + 1;
        } else {
            upperBound = min(channelCount, 16);
        }
        for (int i = lowerBound; i < upperBound; i++) {
            muse::midi::Event e(muse::midi::Event::Opcode::ControlChange, muse::midi::Event::MessageType::ChannelVoice20);
            e.setChannel(i);
            e.setIndex(123); // CC#123 = All notes off
            midiOutPort_->sendEvent(e);
        }
        for (int i = lowerBound; i < upperBound; i++) {
            muse::midi::Event e(muse::midi::Event::Opcode::ControlChange, muse::midi::Event::MessageType::ChannelVoice20);
            e.setChannel(i);
            e.setIndex(midi::SUSTAIN_PEDAL_CONTROLLER);
            e.setData(0);
            midiOutPort_->sendEvent(e);
        }
        for (int i = lowerBound; i < upperBound; i++) {
            muse::midi::Event e(muse::midi::Event::Opcode::PitchBend, muse::midi::Event::MessageType::ChannelVoice20);
            e.setChannel(i);
            e.setData(0x80000000);
            midiOutPort_->sendEvent(e);
        }
    }
}

bool FluidSynth::handleEvent(const midi::Event &event) {
    int ret = FLUID_OK;
    switch (event.opcode()) {
        case Event::Opcode::NoteOn: {
            // fluid_synth_noteon expects 0...127
            ret = fluid_synth_noteon(fluid_->synth, event.channel(), event.note(), event.velocity7());
            tuning_.add(event.note(), event.pitchTuningCents());
        } break;
        case Event::Opcode::NoteOff: {
            ret = fluid_synth_noteoff(fluid_->synth, event.channel(), event.note());
            tuning_.add(event.note(), event.pitchTuningCents());
        } break;
        case Event::Opcode::ControlChange: {
            ret = fluid_synth_cc(fluid_->synth, event.channel(), event.index(), event.data7());
        } break;
        case Event::Opcode::ProgramChange: {
            fluid_synth_program_change(fluid_->synth, event.channel(), event.program());
        } break;
        case Event::Opcode::PitchBend: {
            ret = setPitchBend(event.channel(), event.pitchBend14());
        } break;
        default: {
            LOGD() << "not supported event type: " << event.opcodeString();
            ret = FLUID_FAILED;
        }
    }

    if (midiOutPort_) {
        if (STAFF_TO_MIDIOUT_CHANNEL && event.isChannelVoice()) {
            int staff = sequencer_.lastStaff();
            if (staff >= 0) {
                int channel = staff % 16;
                midi::Event me(event);
                me.setChannel(channel);
                midiOutPort_->sendEvent(me);
            }
        } else {
            midiOutPort_->sendEvent(event);
        }
    }

    return ret == FLUID_OK;
}

void FluidSynth::setMode(const ProcessMode mode) {
    if (mode_ == mode) {
        return;
    }

    AbstractSynthesizer::setMode(mode);

    sequencer_.setActive(isModePlaying(mode));
    updateExpressionLevels();
}

void FluidSynth::setOutputSpec(const OutputSpec &spec) {
    if (outputSpec_ == spec) {
        return;
    }

    outputSpec_ = spec;

    if (fluid_->settings) {
        fluid_settings_setnum(fluid_->settings, "synth.sample-rate", static_cast<double>(spec.sampleRate));
    }

    if (fluid_->synth) {
        delete_fluid_synth(fluid_->synth);
    }

    createFluidInstance();
    addSoundFonts(std::vector<io::path_t>(sfontPaths_.cbegin(), sfontPaths_.cend()));

    if (setupData_.isValid()) {
        setupSound(setupData_);
    }
}

Ret FluidSynth::addSoundFonts(const std::vector<io::path_t> &sfonts) {
    OfflineTimer timer(OfflineTiming::current().fonts);
    IF_ASSERT_FAILED(fluid_->synth) { return make_ret(Err::SynthNotInited); }

    bool ok = true;
    for (const io::path_t &sfont : sfonts) {
        if (fluid_synth_sfload(fluid_->synth, sfont.c_str(), 0) == FLUID_FAILED) {
            LOGE() << "failed load soundfont: " << sfont;
            ok = false;
            continue;
        }

        LOGI() << "success load soundfont: " << sfont;
        sfontPaths_.insert(sfont);
    }

    return ok ? make_ret(Err::NoError) : make_ret(Err::SoundFontFailedLoad);
}

void FluidSynth::setPreset(const std::optional<midi::Program> &preset) { preset_ = preset; }

std::string FluidSynth::name() const { return "Fluid"; }

AudioSourceType FluidSynth::type() const { return AudioSourceType::Fluid; }

void FluidSynth::setupSound(const PlaybackSetupData &setupData) {
    IF_ASSERT_FAILED(fluid_->synth) { return; }

    fluid_synth_activate_key_tuning(fluid_->synth, 0, 0, "standard", NULL, true);

    auto setupChannel = [this](const midi::channel_t channelIdx, const midi::Program &program) {
        if (OfflineCancellation::requested()) return;
        fluid_synth_set_interp_method(fluid_->synth, channelIdx, FLUID_INTERP_DEFAULT);
        fluid_synth_pitch_wheel_sens(fluid_->synth, channelIdx, 24);
        fluid_synth_bank_select(fluid_->synth, channelIdx, program.bank);
        fluid_synth_program_change(fluid_->synth, channelIdx, program.program);
        fluid_synth_cc(fluid_->synth, channelIdx, 7, DEFAULT_MIDI_VOLUME);
        fluid_synth_cc(fluid_->synth, channelIdx, muse::midi::EXPRESSION_CONTROLLER, sequencer_.naturalExpressionLevel());
        fluid_synth_cc(fluid_->synth, channelIdx, 74, 0);
        fluid_synth_set_portamento_mode(fluid_->synth, channelIdx, FLUID_CHANNEL_PORTAMENTO_MODE_EACH_NOTE);
        fluid_synth_set_legato_mode(fluid_->synth, channelIdx, FLUID_CHANNEL_LEGATO_MODE_RETRIGGER);
        fluid_synth_activate_tuning(fluid_->synth, channelIdx, 0, 0, 0);
    };

    sequencer_.setOnChannelAdded(setupChannel);

    sequencer_.init(setupData, preset_, setupData.supportsSingleNoteDynamics, MIDI_CHANNEL_COUNT);

    for (const auto &voice : sequencer_.channels().data()) {
        for (const auto &pair : voice.second) {
            const ChannelMap::ChannelMapping &channelMapping = pair.second;
            setupChannel(channelMapping.first, channelMapping.second);
        }
    }
}

void FluidSynth::setupEvents(const mpe::PlaybackData &playbackData) { sequencer_.load(playbackData); }

const mpe::PlaybackData &FluidSynth::playbackData() const { return sequencer_.playbackData(); }

void FluidSynth::flushSound() {
    sequencer_.flushOffstream();
    flushSoundRequested_ = true;
}

TimePosition FluidSynth::playbackPosition() const {
    return TimePosition::fromTime(muse::usecs_to_secs(sequencer_.playbackPosition().raw()), outputSpec_.sampleRate);
}

void FluidSynth::setPlaybackPosition(const TimePosition &position) {
    IF_ASSERT_FAILED(position.isValid()) { return; }

    //! NOTE Don't trust that msecs_t is used everywhere here,
    // in fact, usecs_t (microseconds) is stored there.
    const usecs_t usecs = muse::secs_to_usecs(position.time());
    sequencer_.setPlaybackPosition(msecs_t(usecs.raw()));

    if (sequencer_.isActive()) {
        updateExpressionLevels();
    }
}

samples_t FluidSynth::process(float *buffer, samples_t samplesPerChannel) {
    IF_ASSERT_FAILED(samplesPerChannel > 0) { return 0; }

    if (OfflineCancellation::requested()) return 0;
    if (flushSoundRequested_) {
        doFlushSound();
        flushSoundRequested_ = false;
    }

    const msecs_t nextMsecs = samplesToMsecs(samplesPerChannel, outputSpec_.sampleRate);
    const FluidSequencer::EventSequenceMap sequences = sequencer_.movePlaybackForward(nextMsecs);
    samples_t sampleOffset = 0;

    for (auto it = sequences.cbegin(); it != sequences.cend(); ++it) {
        if (OfflineCancellation::requested()) return 0;
        samples_t durationInSamples = samplesPerChannel - sampleOffset;

        auto nextIt = std::next(it);
        if (nextIt != sequences.cend()) {
            msecs_t duration = nextIt->first - it->first;
            durationInSamples = microSecsToSamples(duration, outputSpec_.sampleRate);
        }

        IF_ASSERT_FAILED(sampleOffset + durationInSamples <= samplesPerChannel) { break; }

        if (!processSequence(it->second, durationInSamples, buffer + sampleOffset * FLUID_AUDIO_CHANNELS_COUNT)) {
            return 0;
        }

        sampleOffset += durationInSamples;
    }

    return samplesPerChannel;
}

bool FluidSynth::processSequence(const FluidSequencer::EventSequence &sequence, const samples_t samples, float *buffer) {
    OfflineCancellation::SynthesisScope synthesisScope;
    if (!sequence.empty()) {
        tuning_.reset();
    }

    for (const FluidSequencer::EventType &event : sequence) {
        if (OfflineCancellation::requested()) return false;
        handleEvent(std::get<midi::Event>(event));
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
        float *out = buffer + offset * FLUID_AUDIO_CHANNELS_COUNT;
        if (fluid_synth_write_float(fluid_->synth, count, out, 0, FLUID_AUDIO_CHANNELS_COUNT, out, 1, FLUID_AUDIO_CHANNELS_COUNT) !=
            FLUID_OK)
            return false;
        offset += count;
    }
    return true;
}

void FluidSynth::updateExpressionLevels() {
    IF_ASSERT_FAILED(fluid_->synth) { return; }

    std::vector<FluidSequencer::ChannelExpressionLevel> levels;
    if (sequencer_.isActive()) {
        levels = sequencer_.currentExpressionLevels();
    }

    const int naturalLevel = sequencer_.naturalExpressionLevel();
    const midi::channel_t channelCount = sequencer_.channels().channelCount();

    if (levels.empty()) {
        for (midi::channel_t i = 0; i < channelCount; ++i) {
            fluid_synth_cc(fluid_->synth, i, muse::midi::EXPRESSION_CONTROLLER, naturalLevel);
        }
        return;
    }

    std::vector<bool> covered(channelCount, false);

    for (const FluidSequencer::ChannelExpressionLevel &expressionLevel : levels) {
        fluid_synth_cc(fluid_->synth, expressionLevel.channel, muse::midi::EXPRESSION_CONTROLLER, expressionLevel.level);
        if (expressionLevel.channel < channelCount) {
            covered[expressionLevel.channel] = true;
        }
    }

    for (midi::channel_t i = 0; i < channelCount; ++i) {
        if (!covered[i]) {
            fluid_synth_cc(fluid_->synth, i, muse::midi::EXPRESSION_CONTROLLER, naturalLevel);
        }
    }
}

int FluidSynth::setControllerValue(int channel, int ctrl, int value) {
    int currentValue = 0;
    fluid_synth_get_cc(fluid_->synth, channel, ctrl, &currentValue);

    if (value == currentValue) {
        return FLUID_OK;
    }

    return fluid_synth_cc(fluid_->synth, channel, ctrl, value);
}

int FluidSynth::setPitchBend(int channel, int pitchBend) {
    int currentValue = 0;
    fluid_synth_get_pitch_bend(fluid_->synth, channel, &currentValue);

    if (pitchBend == currentValue) {
        return FLUID_OK;
    }

    return fluid_synth_pitch_bend(fluid_->synth, channel, pitchBend);
}
