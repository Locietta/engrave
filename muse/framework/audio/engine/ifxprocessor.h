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

#pragma once

#include <memory>

#include "audio/common/audiotypes.h"

namespace muse::audio {
class IFxProcessor {
public:
    virtual ~IFxProcessor() = default;

    virtual AudioFxType type() const = 0;
    virtual std::string name() const = 0;

    virtual const AudioFxParams &params() const = 0;

    virtual void set_output_spec(const OutputSpec &spec) = 0;

    virtual bool active() const = 0;
    virtual void set_active(bool active) = 0;

    virtual void set_mode(const ProcessMode mode) = 0;

    virtual bool should_process_during_silence() const = 0;

    virtual void process(float *buffer, samples_t sample_count, samples_t playback_position_samples = 0) = 0;
};

using IFxProcessorPtr = std::shared_ptr<IFxProcessor>;
} // namespace muse::audio
