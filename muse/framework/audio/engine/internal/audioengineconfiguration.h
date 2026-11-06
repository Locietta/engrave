/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "../iaudioengineconfiguration.h"
#include "audio/common/audiotypes.h"

namespace muse::audio::engine {
struct AudioEngineConfiguration : IAudioEngineConfiguration {
    AudioEngineConfiguration() = default;

    void set_config(const AudioEngineConfig &conf) override;

    bool auto_process_online_sounds_in_background() const override;

    bool is_lazy_processing_of_online_sounds_enabled() const override;
    void set_is_lazy_processing_of_online_sounds_enabled(bool enabled) override;

    bool use_sound_font_low_pass_filter() const override;

    AudioInputParams default_audio_input_params() const override;

private:

    AudioEngineConfig conf_;
};
} // namespace muse::audio::engine
