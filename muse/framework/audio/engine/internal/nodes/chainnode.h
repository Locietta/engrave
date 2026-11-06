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

#include "audionode.h"

#include <vector>

#include "global/containers.h"

namespace muse::audio {
//! NOTE Just combines several nodes into one entity
template <typename T>
struct ChainNode : AudioNode<T> {

    void add(IAudioNodePtr node);
    void remove(IAudioNodePtr node);
    void clear();

protected:

    void do_self_process(float *, samples_t) override {}

    void do_add(IAudioNodePtr node);
    virtual void rebuild();

    std::vector<IAudioNodePtr> nodes_;
};

template <typename T>
void ChainNode<T>::rebuild() {
    IAudioNodePtr next = this->shared_from_this();
    for (auto &node : nodes_) {
        node->disconnect_all();
        node->connect(next);
        next = node;
    }
}

template <typename T>
void ChainNode<T>::add(IAudioNodePtr node) {
    IF_ASSERT_FAILED(node) { return; }

    do_add(node);
    rebuild();
}

template <typename T>
void ChainNode<T>::do_add(IAudioNodePtr node) {
    IF_ASSERT_FAILED(node) { return; }

    node->set_output_spec(AudioNode<T>::output_spec());
    node->set_mode(AudioNode<T>::mode());
    nodes_.push_back(node);
}

template <typename T>
void ChainNode<T>::remove(IAudioNodePtr node) {
    IF_ASSERT_FAILED(node) { return; }

    bool removed = muse::remove(nodes_, node);
    if (removed) {
        node->disconnect_all();
        rebuild();
    }
}

template <typename T>
void ChainNode<T>::clear() {
    for (auto &node : nodes_) {
        node->disconnect_all();
    }
    nodes_.clear();
}
} // namespace muse::audio
