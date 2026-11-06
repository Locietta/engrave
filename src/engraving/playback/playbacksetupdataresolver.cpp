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

#include "playbacksetupdataresolver.h"

#include "log.h"
#include "mapping/keyboardssetupdataresolver.h"
#include "mapping/percussionssetupdataresolver.h"
#include "mapping/stringssetupdataresolver.h"
#include "mapping/voicessetupdataresolver.h"
#include "mapping/windssetupdataresolver.h"

using namespace mu::engraving;
using namespace muse::mpe;

static const PlaybackSetupData piano_setup_data = {SoundId::PIANO, SoundCategory::KEYBOARDS};

void PlaybackSetupDataResolver::resolve_setup_data(const Instrument *instrument, PlaybackSetupData &result) const {
    if (!instrument->sound_id().empty()) {
        result = PlaybackSetupData::from_string(instrument->sound_id());
        result.supports_single_note_dynamics = instrument->single_note_dynamics();
        result.music_xml_sound_id = std::make_optional(instrument->music_xml_id().to_std_string());
        return;
    }

    if (KeyboardsSetupDataResolver::resolve(instrument, result)) {
        return;
    }

    if (StringsSetupDataResolver::resolve(instrument, result)) {
        return;
    }

    if (WindsSetupDataResolver::resolve(instrument, result)) {
        return;
    }

    if (PercussionsSetupDataResolver::resolve(instrument, result)) {
        return;
    }

    if (VoicesSetupDataResolver::resolve(instrument, result)) {
        return;
    }

    LOGW() << "Unable to resolve setup data for instrument, id: " << instrument->id() << ", family: " << instrument->family()
           << "; fallback to piano";

    result = ::piano_setup_data;
}

void PlaybackSetupDataResolver::resolve_chord_symbols_setup_data(const Instrument *instrument, PlaybackSetupData &result) const {
    if (instrument->has_strings()) {
        static const PlaybackSetupData guitar_setup_data = {
            SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::NYLON, SoundSubCategory::PLUCKED}};

        result = guitar_setup_data;
    } else {
        result = ::piano_setup_data;
    }
}

void PlaybackSetupDataResolver::resolve_metronome_setup_data(PlaybackSetupData &result) const {
    static const PlaybackSetupData metronome_setup_data = {SoundId::BLOCK, SoundCategory::PERCUSSIONS, {SoundSubCategory::WOODEN}};

    result = metronome_setup_data;
}
