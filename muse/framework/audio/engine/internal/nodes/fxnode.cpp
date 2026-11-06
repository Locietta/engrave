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

FxNode::FxNode(IFxProcessorPtr fx_processor, PlayheadPositionPtr playhead_position)
    : fx_processor_(fx_processor), playhead_position_(playhead_position) {
    assert(fx_processor_ && "FxNode requires a non-null IFxProcessor");

    set_name(std::string("Fx[") + fx_processor_->name() + "]");
}

void FxNode::set_playhead_position(PlayheadPositionPtr playhead_position) { playhead_position_ = playhead_position; }

void FxNode::on_mode_changed(const ProcessMode mode) { fx_processor_->set_mode(mode); }

const AudioFxParams &FxNode::params() const { return fx_processor_->params(); }

bool FxNode::should_process_during_silence() const { return fx_processor_->should_process_during_silence(); }

void FxNode::on_output_spec_changed(const OutputSpec &spec) { fx_processor_->set_output_spec(spec); }

void FxNode::do_self_process(float *buffer, samples_t samples_per_channel) {
    samples_t pos = playhead_position_ ? playhead_position_->current_position().samples() : 0;
    fx_processor_->process(buffer, samples_per_channel, pos);
}
