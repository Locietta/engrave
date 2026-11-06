/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#include "editsoundflag.h"

#include "../dom/soundflag.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   ChangeSoundFlag
//---------------------------------------------------------

void ChangeSoundFlag::flip() {
    IF_ASSERT_FAILED(sound_flag_) { return; }

    SoundFlag::PresetCodes presets = sound_flag_->sound_presets();
    SoundFlag::PlayingTechniqueCode technique = sound_flag_->playing_technique();

    sound_flag_->set_sound_presets(presets_);
    sound_flag_->set_playing_technique(playing_technique_);

    presets_ = std::move(presets);
    playing_technique_ = std::move(technique);
}
