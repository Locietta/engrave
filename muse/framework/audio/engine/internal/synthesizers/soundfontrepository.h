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

#include <functional>

#include "audio/engine/isoundfontrepository.h"

namespace muse::audio::synth {
class SoundFontRepository : public ISoundFontRepository {
public:
    void load_sound_fonts(const std::vector<SoundFontUri> &uris) override;
    void add_sound_font(const SoundFontUri &uri) override;
    void add_sound_font_data(const SoundFontUri &uri, const ByteArray &data) override;

    bool is_sound_font_loaded(const std::string &name) const override;
    bool is_loading_sound_fonts() const override;

    const SoundFontsMap &sound_fonts() const override;

    // Called synchronously after the SoundFonts changed, where upstream sent
    // soundFontsChanged; the audio services refresh the synth resolvers here.
    void set_on_sound_fonts_changed(std::function<void()> on_sound_fonts_changed);

private:
    void notify_sound_fonts_changed();

    void do_add_sound_font(const SoundFontUri &uri, const SoundFontsMap *cache = nullptr, std::function<void()> on_finished = nullptr);

    SoundFontsMap sound_fonts_;

    size_t loading_sound_font_count_ = 0;
    std::function<void()> on_sound_fonts_changed_;
};
} // namespace muse::audio::synth
