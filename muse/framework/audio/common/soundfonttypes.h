/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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
#ifndef MUSE_AUDIO_SOUNDFONTTYPES_H
#define MUSE_AUDIO_SOUNDFONTTYPES_H

#include <map>
#include <string>
#include <vector>

#include "global/io/path.h"
#include "global/types/uri.h"
#include "midi/miditypes.h"

namespace muse::audio::synth {
using SoundFontUri = Uri;
using SoundFontPath = io::PathT;

inline bool is_sound_font(const io::PathT &file_path) {
    std::string ext = io::suffix(file_path);
    return ext == "sf2" || ext == "sf3";
}

struct SoundFontPreset {
    midi::Program program;
    std::string name;
};

struct SoundFontMeta {
    std::string name;
    SoundFontPath path;
    std::vector<SoundFontPreset> presets;
};

using SoundFontsMap = std::map<SoundFontUri, SoundFontMeta>;

inline const String soundfont_name_attribute(u"soundFontName");
inline const String preset_name_attribute(u"presetName");
inline const String preset_bank_attribute(u"presetBank");
inline const String preset_program_attribute(u"presetProgram");
} // namespace muse::audio::synth

#endif // MUSE_AUDIO_SOUNDFONTTYPES_H
