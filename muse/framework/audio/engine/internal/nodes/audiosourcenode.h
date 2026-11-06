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

#include "audio/common/audiotypes.h"
#include "audio/common/timeposition.h"

namespace muse::audio {
struct SourceTag {
    static constexpr const char *name = "Source";
};
} // namespace muse::audio

namespace muse::audio::engine {
//! NOTE Abstract Base Audio Source Node
struct AudioSourceNode : AudioNode<SourceTag> {

    virtual void seek(const TimePosition &position, const bool flush_sound = true) = 0;
    virtual void flush() = 0;

    virtual const AudioInputParams &input_params() const = 0;
    virtual void apply_input_params(const AudioInputParams &required_params) = 0;

    virtual void prepare_to_play() = 0;
    virtual bool ready_to_play() const = 0;

    virtual bool has_pending_chunks() const = 0;
    virtual void process_input() = 0;
    virtual InputProcessingProgress input_processing_progress() const = 0;

    virtual void clear_cache() = 0;
};

using AudioSourceNodePtr = std::shared_ptr<AudioSourceNode>;
} // namespace muse::audio::engine
