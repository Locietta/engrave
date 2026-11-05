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

void FxChain::setFxList(const std::vector<IFxProcessorPtr> &fxList) {
    clear();

    for (auto it = fxList.rbegin(); it != fxList.rend(); ++it) {
        FxNodePtr node = std::make_shared<FxNode>(*it);
        doAdd(node);
    }

    rebuild();
    updateShouldProcessDuringSilence();

    if (!name().empty() && !nodes_.empty()) {
        LOGD() << name() << ": " << nodes_.front()->dump();
    }
}

void FxChain::setFxChainSpec(const AudioFxChain &fxChainSpec) {
    if (fxChainSpec_ == fxChainSpec) {
        return;
    }

    auto findFxNode = [this](const std::pair<AudioFxChainOrder, AudioFxParams> &params) -> FxNodePtr {
        for (auto &node : nodes_) {
            FxNodePtr fx = std::dynamic_pointer_cast<FxNode>(node);
            IF_ASSERT_FAILED(fx) { continue; }

            if (fx->params().chainOrder != params.first) {
                continue;
            }

            if (fx->params().resourceMeta == params.second.resourceMeta) {
                return fx;
            }
        }

        return nullptr;
    };

    fxChainSpec_ = fxChainSpec;
    for (auto it = fxChainSpec_.begin(); it != fxChainSpec_.end();) {
        if (FxNodePtr fx = findFxNode(*it)) {
            fx->setBypassed(!it->second.active);
            ++it;
        } else {
            it = fxChainSpec_.erase(it);
        }
    }

    updateShouldProcessDuringSilence();
}

const AudioFxChain &FxChain::fxChainSpec() const { return fxChainSpec_; }

void FxChain::setPlayheadPosition(PlayheadPositionPtr playheadPosition) {
    for (auto &node : nodes_) {
        FxNodePtr fx = std::dynamic_pointer_cast<FxNode>(node);
        IF_ASSERT_FAILED(fx) { continue; }

        fx->setPlayheadPosition(playheadPosition);
    }
}

void FxChain::rebuild() {
    for (size_t i = 1; i < nodes_.size(); ++i) {
        IAudioNodePtr &prev = nodes_.at(i - 1);
        IAudioNodePtr &curr = nodes_.at(i);
        curr->disconnectAll();
        curr->connect(prev); // prev->input = curr
    }
}

void FxChain::doSelfProcess(float *buffer, samples_t samplesPerChannel) {
    if (!nodes_.empty()) {
        nodes_.front()->process(buffer, samplesPerChannel);
    }
}

void FxChain::updateShouldProcessDuringSilence() {
    bool shouldProcessDuringSilence = false;
    for (const auto &node : nodes_) {
        FxNodePtr fx = std::dynamic_pointer_cast<FxNode>(node);
        IF_ASSERT_FAILED(fx) { continue; }

        if (fx->shouldProcessDuringSilence()) {
            shouldProcessDuringSilence = true;
            break;
        }
    }

    if (shouldProcessDuringSilence_ != shouldProcessDuringSilence) {
        shouldProcessDuringSilence_ = shouldProcessDuringSilence;
    }
}

bool FxChain::shouldProcessDuringSilence() const { return shouldProcessDuringSilence_; }
