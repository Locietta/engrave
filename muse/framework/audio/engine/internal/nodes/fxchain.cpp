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

#include "fxchain.h"

#include "fxnode.h"
#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

void FxChain::set_fx_list(const std::vector<IFxProcessorPtr> &fx_list) {
    clear();

    for (auto it = fx_list.rbegin(); it != fx_list.rend(); ++it) {
        FxNodePtr node = std::make_shared<FxNode>(*it);
        do_add(node);
    }

    rebuild();
    update_should_process_during_silence();

    if (!name().empty() && !nodes_.empty()) {
        LOGD() << name() << ": " << nodes_.front()->dump();
    }
}

void FxChain::set_fx_chain_spec(const AudioFxChain &fx_chain_spec) {
    if (fx_chain_spec_ == fx_chain_spec) {
        return;
    }

    auto find_fx_node = [this](const std::pair<AudioFxChainOrder, AudioFxParams> &params) -> FxNodePtr {
        for (auto &node : nodes_) {
            FxNodePtr fx = std::dynamic_pointer_cast<FxNode>(node);
            IF_ASSERT_FAILED(fx) { continue; }

            if (fx->params().chain_order != params.first) {
                continue;
            }

            if (fx->params().resource_meta == params.second.resource_meta) {
                return fx;
            }
        }

        return nullptr;
    };

    fx_chain_spec_ = fx_chain_spec;
    for (auto it = fx_chain_spec_.begin(); it != fx_chain_spec_.end();) {
        if (FxNodePtr fx = find_fx_node(*it)) {
            fx->set_bypassed(!it->second.active);
            ++it;
        } else {
            it = fx_chain_spec_.erase(it);
        }
    }

    update_should_process_during_silence();
}

const AudioFxChain &FxChain::fx_chain_spec() const { return fx_chain_spec_; }

void FxChain::set_playhead_position(PlayheadPositionPtr playhead_position) {
    for (auto &node : nodes_) {
        FxNodePtr fx = std::dynamic_pointer_cast<FxNode>(node);
        IF_ASSERT_FAILED(fx) { continue; }

        fx->set_playhead_position(playhead_position);
    }
}

void FxChain::rebuild() {
    for (size_t i = 1; i < nodes_.size(); ++i) {
        IAudioNodePtr &prev = nodes_.at(i - 1);
        IAudioNodePtr &curr = nodes_.at(i);
        curr->disconnect_all();
        curr->connect(prev); // prev->input = curr
    }
}

void FxChain::do_self_process(float *buffer, samples_t samples_per_channel) {
    if (!nodes_.empty()) {
        nodes_.front()->process(buffer, samples_per_channel);
    }
}

void FxChain::update_should_process_during_silence() {
    bool should_process_during_silence = false;
    for (const auto &node : nodes_) {
        FxNodePtr fx = std::dynamic_pointer_cast<FxNode>(node);
        IF_ASSERT_FAILED(fx) { continue; }

        if (fx->should_process_during_silence()) {
            should_process_during_silence = true;
            break;
        }
    }

    if (should_process_during_silence_ != should_process_during_silence) {
        should_process_during_silence_ = should_process_during_silence;
    }
}

bool FxChain::should_process_during_silence() const { return should_process_during_silence_; }
