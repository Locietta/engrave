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

#include "audio/common/soundfonttypes.h"

namespace muse::audio::synth {
struct ISoundFontRepository {
    virtual ~ISoundFontRepository() = default;

    virtual void load_sound_fonts(const std::vector<SoundFontUri> &uris) = 0;
    virtual void add_sound_font(const SoundFontUri &uri) = 0;
    virtual void add_sound_font_data(const SoundFontUri &uri, const ByteArray &data) = 0;

    virtual bool is_sound_font_loaded(const std::string &name) const = 0;
    virtual bool is_loading_sound_fonts() const = 0;

    virtual const SoundFontsMap &sound_fonts() const = 0;
};
} // namespace muse::audio::synth
