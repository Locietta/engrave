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

#include "fxnode.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

FxNode::FxNode(IFxProcessorPtr fxProcessor, PlayheadPositionPtr playheadPosition)
    : fxProcessor_(fxProcessor), playheadPosition_(playheadPosition) {
    assert(fxProcessor_ && "FxNode requires a non-null IFxProcessor");

    setName(std::string("Fx[") + fxProcessor_->name() + "]");
}

void FxNode::setPlayheadPosition(PlayheadPositionPtr playheadPosition) { playheadPosition_ = playheadPosition; }

void FxNode::onModeChanged(const ProcessMode mode) { fxProcessor_->setMode(mode); }

const AudioFxParams &FxNode::params() const { return fxProcessor_->params(); }

bool FxNode::shouldProcessDuringSilence() const { return fxProcessor_->shouldProcessDuringSilence(); }

void FxNode::onOutputSpecChanged(const OutputSpec &spec) { fxProcessor_->setOutputSpec(spec); }

void FxNode::doSelfProcess(float *buffer, samples_t samplesPerChannel) {
    samples_t pos = playheadPosition_ ? playheadPosition_->currentPosition().samples() : 0;
    fxProcessor_->process(buffer, samplesPerChannel, pos);
}
