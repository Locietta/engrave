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
    IF_ASSERT_FAILED(soundFlag_) { return; }

    SoundFlag::PresetCodes presets = soundFlag_->soundPresets();
    SoundFlag::PlayingTechniqueCode technique = soundFlag_->playingTechnique();

    soundFlag_->setSoundPresets(presets_);
    soundFlag_->setPlayingTechnique(playingTechnique_);

    presets_ = std::move(presets);
    playingTechnique_ = std::move(technique);
}
