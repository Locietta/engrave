/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include <memory>
#include <string>
#include <sstream>

#include "audio/common/audiotypes.h"

#include "log.h"

// #define AUDIONODE_LOGGING_ENABLED

#ifdef AUDIONODE_LOGGING_ENABLED
#define ANODE_LOG(name) LOGDA() << "[" << name << "] "
#else
#define ANODE_LOG(name) LOGN()
#endif

//! NOTE Nodes are in the engine de facto namespace,
// but the engine is not specified here only
// so that the class names are shorter when
// debugging in the call stack (the full names are specified there)

namespace muse::audio {
template <typename T>
class AudioNode;

class IAudioNode : public std::enable_shared_from_this<IAudioNode> {
public:
    virtual ~IAudioNode() = default;

    virtual void set_name(const std::string &name) = 0;
    virtual const std::string &name() const = 0;

    virtual void set_output_spec(const OutputSpec &spec) = 0;
    virtual const OutputSpec &output_spec() const = 0;
    virtual void set_mode(const ProcessMode mode) = 0;
    virtual ProcessMode mode() const = 0;

    // turns off this node and all connected ones (like mute)
    virtual void set_enabled(bool enabled) = 0;
    virtual bool enabled() const = 0;

    // turns off only this node (like bypass)
    virtual void set_bypassed(bool bypassed) = 0;
    virtual bool bypassed() const = 0;

    virtual IAudioNode *connect(std::shared_ptr<IAudioNode> other) = 0;
    virtual IAudioNode *disconnect(std::shared_ptr<IAudioNode> other) = 0;
    virtual void disconnect_all() = 0;

    virtual void process(float *buffer, samples_t samples_per_channel) = 0;

    virtual std::string dump() const = 0;

protected:

    template <typename T>
    friend class AudioNode;

    virtual void on_output_spec_changed(const OutputSpec &spec) = 0;
    virtual void on_mode_changed(const ProcessMode mode) = 0;
    virtual void on_enabled_changed(bool enabled) = 0;
    virtual void on_bypassed_changed(bool bypassed) = 0;

    virtual bool do_add_node(std::shared_ptr<IAudioNode> other) = 0;
    virtual bool do_remove_node(std::shared_ptr<IAudioNode> other) = 0;

    virtual void do_self_process(float *buffer, samples_t samples_per_channel) = 0;
};

using IAudioNodePtr = std::shared_ptr<IAudioNode>;

//! NOTE A template base node class.
// The tag is primarily used to simplify debugging
// (for example, to make it easier to understand the call stack).
template <typename T>
class AudioNode : public IAudioNode {
public:

    void set_name(const std::string &name) override;
    const std::string &name() const override;

    void set_output_spec(const OutputSpec &spec) final override;
    const OutputSpec &output_spec() const final override;
    void set_mode(const ProcessMode mode) final override;
    ProcessMode mode() const final override;

    // turns off this node and all connected ones (like mute)
    void set_enabled(bool enabled) final override;
    bool enabled() const final override;

    // turns off only this node (like bypass)
    void set_bypassed(bool bypassed) final override;
    bool bypassed() const final override;

    IAudioNode *connect(std::shared_ptr<IAudioNode> other) final override;
    IAudioNode *disconnect(std::shared_ptr<IAudioNode> other) final override;
    void disconnect_all() final override;

    //! NOTE This method needs to be overridden directly
    // to reduce the stack count for easier debugging.
    virtual void process(float *buffer, samples_t samples_per_channel) override;

    std::string dump() const override;

protected:

    AudioNode();

    virtual void on_output_spec_changed(const OutputSpec &spec) override;
    virtual void on_mode_changed(const ProcessMode mode) override;
    virtual void on_enabled_changed(bool enabled) override;
    virtual void on_bypassed_changed(bool bypassed) override;

    virtual bool do_add_node(std::shared_ptr<IAudioNode> other) override;
    virtual bool do_remove_node(std::shared_ptr<IAudioNode> other) override;

    // virtual void doSelfProcess(float* buffer, samples_t samplesPerChannel) = 0;

    std::string name_;
    OutputSpec output_spec_;
    ProcessMode mode_ = ProcessMode::UNDEFINED;
    bool enabled_ = true;
    bool bypassed_ = false;

    std::vector<std::shared_ptr<IAudioNode>> connected_to_;
    std::shared_ptr<IAudioNode> input_ = nullptr;
};

template <typename T>
AudioNode<T>::AudioNode() : name_(T::name) {}

template <typename T>
void AudioNode<T>::set_name(const std::string &name) {
    name_ = name;
}

template <typename T>
const std::string &AudioNode<T>::name() const {
    return name_;
}

template <typename T>
void AudioNode<T>::set_output_spec(const OutputSpec &spec) {
    if (output_spec_ == spec) {
        return;
    }
    output_spec_ = spec;
    ANODE_LOG(name()) << "onOutputSpecChanged: " << spec.dump();
    on_output_spec_changed(spec);
    if (input_) {
        input_->set_output_spec(spec);
    }
}

template <typename T>
const OutputSpec &AudioNode<T>::output_spec() const {
    return output_spec_;
}

template <typename T>
void AudioNode<T>::on_output_spec_changed(const OutputSpec & /*spec*/) {}

template <typename T>
void AudioNode<T>::set_mode(const ProcessMode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    ANODE_LOG(name()) << "onModeChanged: " << to_string(mode);
    on_mode_changed(mode);
    if (input_) {
        input_->set_mode(mode);
    }
}

template <typename T>
ProcessMode AudioNode<T>::mode() const {
    return mode_;
}

template <typename T>
void AudioNode<T>::on_mode_changed(const ProcessMode /*mode*/) {}

template <typename T>
void AudioNode<T>::set_enabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    ANODE_LOG(name()) << "onEnabledChanged: " << enabled_;
    on_enabled_changed(enabled);
    if (input_) {
        input_->set_enabled(enabled);
    }
}

template <typename T>
bool AudioNode<T>::enabled() const {
    return enabled_;
}

template <typename T>
void AudioNode<T>::on_enabled_changed(bool /*enabled*/) {}

template <typename T>
void AudioNode<T>::set_bypassed(bool bypassed) {
    if (bypassed_ == bypassed) {
        return;
    }
    bypassed_ = bypassed;
    ANODE_LOG(name()) << "onBypassedChanged: " << bypassed_;
    on_bypassed_changed(bypassed);
}

template <typename T>
bool AudioNode<T>::bypassed() const {
    return bypassed_;
}

template <typename T>
void AudioNode<T>::on_bypassed_changed(bool /*bypassed*/) {}

template <typename T>
IAudioNode *AudioNode<T>::connect(std::shared_ptr<IAudioNode> other) {
    IF_ASSERT_FAILED(other) { return this; }
    bool ok = other->do_add_node(shared_from_this());
    if (ok) {
        connected_to_.push_back(other);
        ANODE_LOG(name()) << "connected to: " << other->name();
    }
    return this;
}

template <typename T>
IAudioNode *AudioNode<T>::disconnect(std::shared_ptr<IAudioNode> other) {
    IF_ASSERT_FAILED(other) { return this; }
    bool ok = other->do_remove_node(shared_from_this());
    if (ok) {
        muse::remove(connected_to_, other);
        ANODE_LOG(name()) << "disconnected from: " << other->name();
    }
    return this;
}

template <typename T>
void AudioNode<T>::disconnect_all() {
    auto copy = connected_to_;
    for (auto &connected_to : copy) {
        disconnect(connected_to);
    }
}

template <typename T>
bool AudioNode<T>::do_add_node(std::shared_ptr<IAudioNode> other) {
    IF_ASSERT_FAILED(other) { return false; }

    IF_ASSERT_FAILED(!input_) {
        LOGE() << "already connected to another node";
        return false;
    }

    input_ = other;
    return true;
}

template <typename T>
bool AudioNode<T>::do_remove_node(std::shared_ptr<IAudioNode> other) {
    IF_ASSERT_FAILED(other) { return false; }

    IF_ASSERT_FAILED(input_ == other) {
        LOGE() << "not connected to this node";
        return false;
    }

    input_ = nullptr;
    return true;
}

template <typename T>
void AudioNode<T>::process(float *buffer, samples_t samples_per_channel) {
    if (!enabled_) {
        return;
    }

    if (input_) {
        input_->process(buffer, samples_per_channel);
    }

    if (bypassed_) {
        return;
    }

    do_self_process(buffer, samples_per_channel);
}

template <typename T>
std::string AudioNode<T>::dump() const {
    std::stringstream ss;
    ss << name();
    if (input_) {
        ss << " <-- " << input_->dump();
    }
    return ss.str();
}
} // namespace muse::audio
