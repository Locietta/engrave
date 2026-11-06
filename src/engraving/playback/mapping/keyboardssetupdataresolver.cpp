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

#include "keyboardssetupdataresolver.h"

#include <unordered_set>

using namespace mu::engraving;
using namespace muse::mpe;

PlaybackSetupData KeyboardsSetupDataResolver::do_resolve(const Instrument *instrument) {
    static const std::unordered_map<std::string, PlaybackSetupData> setup_data_map = {
        {"celesta", {SoundId::CELESTA, SoundCategory::KEYBOARDS}},
        {"clavichord", {SoundId::CLAVICHORD, SoundCategory::KEYBOARDS, {SoundSubCategory::BAROQUE}}},
        {"clavinet", {SoundId::CLAVICHORD, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC}}},
        {"ondes-martenot", {SoundId::ONDES_MARTENOT, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC}}},
        {"harpsichord", {SoundId::HARPSICHORD, SoundCategory::KEYBOARDS}},
        {"virginal", {SoundId::VIRGINAL, SoundCategory::KEYBOARDS, {SoundSubCategory::BAROQUE}}},
        {"electric-piano", {SoundId::PIANO, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC}}},
        {"grand-piano", {SoundId::PIANO, SoundCategory::KEYBOARDS, {SoundSubCategory::GRAND}}},
        {"honky-tonk-piano", {SoundId::PIANO, SoundCategory::KEYBOARDS, {SoundSubCategory::HONKY_TONK}}},
        {"piano", {SoundId::PIANO, SoundCategory::KEYBOARDS}},
        {"toy-piano", {SoundId::PIANO, SoundCategory::KEYBOARDS, {SoundSubCategory::TOY}}},
        {"upright-piano", {SoundId::PIANO, SoundCategory::KEYBOARDS, {SoundSubCategory::UPRIGHT}}},

        {"hammond-organ", {SoundId::ORGAN, SoundCategory::KEYBOARDS, {SoundSubCategory::HAMMOND}}},
        {"organ", {SoundId::ORGAN, SoundCategory::KEYBOARDS}},
        {"percussive-organ", {SoundId::ORGAN, SoundCategory::KEYBOARDS, {SoundSubCategory::PERCUSSIVE}}},
        {"pipe-organ", {SoundId::ORGAN, SoundCategory::KEYBOARDS, {SoundSubCategory::PIPED}}},
        {"rotary-organ", {SoundId::ORGAN, SoundCategory::KEYBOARDS, {SoundSubCategory::ROTARY}}},
        {"harmonium", {SoundId::ORGAN, SoundCategory::KEYBOARDS, {SoundSubCategory::REED}}},
        {"reed-organ", {SoundId::ORGAN, SoundCategory::KEYBOARDS, {SoundSubCategory::REED}}},

        {"brass-synthesizer", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::BRASS}}},
        {"string-synthesizer", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::STRING}}},
        {"mallet-synthesizer", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC}}},
        {"atmosphere-synth",
         {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_ATMOSPHERE}}},
        {"brightness-synth",
         {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_BRIGHTNESS}}},
        {"crystal-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_CRYSTAL}}},
        {"echoes-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_ECHOES}}},
        {"goblins-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_GOBLINS}}},
        {"rain-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_RAIN}}},
        {"effect-synth",
         {SoundId::SYNTHESIZER,
          SoundCategory::KEYBOARDS,
          {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_ATMOSPHERE, SoundSubCategory::FX_BRIGHTNESS, SoundSubCategory::FX_CRYSTAL,
           SoundSubCategory::FX_ECHOES, SoundSubCategory::FX_GOBLINS, SoundSubCategory::FX_RAIN}}},
        {"sine-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::SINE_WAVE}}},
        {"square-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::SQUARE_WAVE}}},
        {"saw-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::SAWTOOTH_WAVE}}},
        {"new-age-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::NEW_AGE}}},
        {"pad-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PAD}}},
        {"warm-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::WARM}}},
        {"poly-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::POLYSYNTH}}},
        {"choir-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::CHOIR}}},
        {"metallic-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::METALLIC}}},
        {"halo-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::HALO}}},
        {"sweep-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::SWEEP}}},
        {"soundtrack-synth",
         {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_SOUND_TRACK}}},
        {"sci-fi-synth", {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_SCI_FI}}},
    };

    auto search = setup_data_map.find(instrument->id().to_std_string());
    if (search == setup_data_map.cend()) {
        static PlaybackSetupData empty;
        return empty;
    }

    return search->second;
}
