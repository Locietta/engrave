/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#include "voicessetupdataresolver.h"

#include "playback/utils/sortedtable.h"

using namespace mu::engraving;
using namespace muse::mpe;
using namespace muse;

PlaybackSetupData VoicesSetupDataResolver::do_resolve(const Instrument *instrument) {
    static const auto setup_data_map = mu::engraving::make_sorted_table<std::string, mpe::PlaybackSetupData>({
        {"boy-soprano", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::SOPRANO, SoundSubCategory::BOY}}},
        {"soprano", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::SOPRANO}}},
        {"soprano-c-clef", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::SOPRANO}}},
        {"mezzo-soprano", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::SOPRANO}}},
        {"mezzo-soprano-c-clef", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::SOPRANO}}},
        {"countertenor", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::COUNTER_TENOR}}},
        {"alto", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::ALTO}}},
        {"alto-c-clef", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::ALTO}}},
        {"contralto", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::CONTRA_ALTO}}},
        {"tenor", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::TENOR}}},
        {"tenor-c-clef", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::TENOR}}},
        {"baritone", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::BARITONE}}},
        {"baritone-c-clef", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::BARITONE}}},
        {"bass", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::BASS}}},
        {"voice", {SoundId::CHOIR, SoundCategory::VOICES}},
        {"women", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::FEMALE}}},
        {"men", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::MALE}}},
    });

    auto search = setup_data_map.find(instrument->id().to_std_string());
    if (search == setup_data_map.cend()) {
        static PlaybackSetupData empty;
        return empty;
    }

    return search->second;
}
