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

#include "../offlinerendersettings.h"
#include "audio/common/audiotypes.h"

namespace muse::audio::engine {
struct AudioEngineConfiguration {
    AudioEngineConfiguration() = default;

    OfflineRenderSettings render_settings() const { return {default_audio_input_params(), use_sound_font_low_pass_filter()}; }

    void set_config(const AudioEngineConfig &conf);

    bool auto_process_online_sounds_in_background() const;

    bool is_lazy_processing_of_online_sounds_enabled() const;
    void set_is_lazy_processing_of_online_sounds_enabled(bool enabled);

    bool use_sound_font_low_pass_filter() const;

    AudioInputParams default_audio_input_params() const;

private:

    AudioEngineConfig conf_;
};
} // namespace muse::audio::engine
