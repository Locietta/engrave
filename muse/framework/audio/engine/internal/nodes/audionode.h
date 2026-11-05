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

    virtual void setName(const std::string &name) = 0;
    virtual const std::string &name() const = 0;

    virtual void setOutputSpec(const OutputSpec &spec) = 0;
    virtual const OutputSpec &outputSpec() const = 0;
    virtual void setMode(const ProcessMode mode) = 0;
    virtual ProcessMode mode() const = 0;

    // turns off this node and all connected ones (like mute)
    virtual void setEnabled(bool enabled) = 0;
    virtual bool enabled() const = 0;

    // turns off only this node (like bypass)
    virtual void setBypassed(bool bypassed) = 0;
    virtual bool bypassed() const = 0;

    virtual IAudioNode *connect(std::shared_ptr<IAudioNode> other) = 0;
    virtual IAudioNode *disconnect(std::shared_ptr<IAudioNode> other) = 0;
    virtual void disconnectAll() = 0;

    virtual void process(float *buffer, samples_t samplesPerChannel) = 0;

    virtual std::string dump() const = 0;

protected:

    template <typename T>
    friend class AudioNode;

    virtual void onOutputSpecChanged(const OutputSpec &spec) = 0;
    virtual void onModeChanged(const ProcessMode mode) = 0;
    virtual void onEnabledChanged(bool enabled) = 0;
    virtual void onBypassedChanged(bool bypassed) = 0;

    virtual bool doAddNode(std::shared_ptr<IAudioNode> other) = 0;
    virtual bool doRemoveNode(std::shared_ptr<IAudioNode> other) = 0;

    virtual void doSelfProcess(float *buffer, samples_t samplesPerChannel) = 0;
};

using IAudioNodePtr = std::shared_ptr<IAudioNode>;

//! NOTE A template base node class.
// The tag is primarily used to simplify debugging
// (for example, to make it easier to understand the call stack).
template <typename T>
class AudioNode : public IAudioNode {
public:

    void setName(const std::string &name) override;
    const std::string &name() const override;

    void setOutputSpec(const OutputSpec &spec) final override;
    const OutputSpec &outputSpec() const final override;
    void setMode(const ProcessMode mode) final override;
    ProcessMode mode() const final override;

    // turns off this node and all connected ones (like mute)
    void setEnabled(bool enabled) final override;
    bool enabled() const final override;

    // turns off only this node (like bypass)
    void setBypassed(bool bypassed) final override;
    bool bypassed() const final override;

    IAudioNode *connect(std::shared_ptr<IAudioNode> other) final override;
    IAudioNode *disconnect(std::shared_ptr<IAudioNode> other) final override;
    void disconnectAll() final override;

    //! NOTE This method needs to be overridden directly
    // to reduce the stack count for easier debugging.
    virtual void process(float *buffer, samples_t samplesPerChannel) override;

    std::string dump() const override;

protected:

    AudioNode();

    virtual void onOutputSpecChanged(const OutputSpec &spec) override;
    virtual void onModeChanged(const ProcessMode mode) override;
    virtual void onEnabledChanged(bool enabled) override;
    virtual void onBypassedChanged(bool bypassed) override;

    virtual bool doAddNode(std::shared_ptr<IAudioNode> other) override;
    virtual bool doRemoveNode(std::shared_ptr<IAudioNode> other) override;

    // virtual void doSelfProcess(float* buffer, samples_t samplesPerChannel) = 0;

    std::string name_;
    OutputSpec outputSpec_;
    ProcessMode mode_ = ProcessMode::Undefined;
    bool enabled_ = true;
    bool bypassed_ = false;

    std::vector<std::shared_ptr<IAudioNode>> connectedTo_;
    std::shared_ptr<IAudioNode> input_ = nullptr;
};

template <typename T>
AudioNode<T>::AudioNode() : name_(T::name) {}

template <typename T>
void AudioNode<T>::setName(const std::string &name) {
    name_ = name;
}

template <typename T>
const std::string &AudioNode<T>::name() const {
    return name_;
}

template <typename T>
void AudioNode<T>::setOutputSpec(const OutputSpec &spec) {
    if (outputSpec_ == spec) {
        return;
    }
    outputSpec_ = spec;
    ANODE_LOG(name()) << "onOutputSpecChanged: " << spec.dump();
    onOutputSpecChanged(spec);
    if (input_) {
        input_->setOutputSpec(spec);
    }
}

template <typename T>
const OutputSpec &AudioNode<T>::outputSpec() const {
    return outputSpec_;
}

template <typename T>
void AudioNode<T>::onOutputSpecChanged(const OutputSpec & /*spec*/) {}

template <typename T>
void AudioNode<T>::setMode(const ProcessMode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    ANODE_LOG(name()) << "onModeChanged: " << to_string(mode);
    onModeChanged(mode);
    if (input_) {
        input_->setMode(mode);
    }
}

template <typename T>
ProcessMode AudioNode<T>::mode() const {
    return mode_;
}

template <typename T>
void AudioNode<T>::onModeChanged(const ProcessMode /*mode*/) {}

template <typename T>
void AudioNode<T>::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    ANODE_LOG(name()) << "onEnabledChanged: " << enabled_;
    onEnabledChanged(enabled);
    if (input_) {
        input_->setEnabled(enabled);
    }
}

template <typename T>
bool AudioNode<T>::enabled() const {
    return enabled_;
}

template <typename T>
void AudioNode<T>::onEnabledChanged(bool /*enabled*/) {}

template <typename T>
void AudioNode<T>::setBypassed(bool bypassed) {
    if (bypassed_ == bypassed) {
        return;
    }
    bypassed_ = bypassed;
    ANODE_LOG(name()) << "onBypassedChanged: " << bypassed_;
    onBypassedChanged(bypassed);
}

template <typename T>
bool AudioNode<T>::bypassed() const {
    return bypassed_;
}

template <typename T>
void AudioNode<T>::onBypassedChanged(bool /*bypassed*/) {}

template <typename T>
IAudioNode *AudioNode<T>::connect(std::shared_ptr<IAudioNode> other) {
    IF_ASSERT_FAILED(other) { return this; }
    bool ok = other->doAddNode(shared_from_this());
    if (ok) {
        connectedTo_.push_back(other);
        ANODE_LOG(name()) << "connected to: " << other->name();
    }
    return this;
}

template <typename T>
IAudioNode *AudioNode<T>::disconnect(std::shared_ptr<IAudioNode> other) {
    IF_ASSERT_FAILED(other) { return this; }
    bool ok = other->doRemoveNode(shared_from_this());
    if (ok) {
        muse::remove(connectedTo_, other);
        ANODE_LOG(name()) << "disconnected from: " << other->name();
    }
    return this;
}

template <typename T>
void AudioNode<T>::disconnectAll() {
    auto copy = connectedTo_;
    for (auto &connectedTo : copy) {
        disconnect(connectedTo);
    }
}

template <typename T>
bool AudioNode<T>::doAddNode(std::shared_ptr<IAudioNode> other) {
    IF_ASSERT_FAILED(other) { return false; }

    IF_ASSERT_FAILED(!input_) {
        LOGE() << "already connected to another node";
        return false;
    }

    input_ = other;
    return true;
}

template <typename T>
bool AudioNode<T>::doRemoveNode(std::shared_ptr<IAudioNode> other) {
    IF_ASSERT_FAILED(other) { return false; }

    IF_ASSERT_FAILED(input_ == other) {
        LOGE() << "not connected to this node";
        return false;
    }

    input_ = nullptr;
    return true;
}

template <typename T>
void AudioNode<T>::process(float *buffer, samples_t samplesPerChannel) {
    if (!enabled_) {
        return;
    }

    if (input_) {
        input_->process(buffer, samplesPerChannel);
    }

    if (bypassed_) {
        return;
    }

    doSelfProcess(buffer, samplesPerChannel);
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
