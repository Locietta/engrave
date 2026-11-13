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

void mu::engraving::apply_change_sound_flag(SoundFlag *sound_flag_, const SoundFlag::PresetCodes &presets_,
                                            const SoundFlag::PlayingTechniqueCode &playing_technique_) {
    note_change(sound_flag_, CommandType::UNKNOWN);

    IF_ASSERT_FAILED(sound_flag_) { return; }

    sound_flag_->set_sound_presets(presets_);
    sound_flag_->set_playing_technique(playing_technique_);
}
