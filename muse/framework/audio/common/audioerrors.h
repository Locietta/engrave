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
#ifndef MUSE_AUDIO_AUDIOERRORS_H
#define MUSE_AUDIO_AUDIOERRORS_H

#include "global/types/ret.h"

#undef NO_ERROR

namespace muse::audio {
enum class Err {
    UNDEFINED = int(Ret::Code::UNDEFINED),
    NO_ERROR = int(Ret::Code::OK),
    UNKNOWN_ERROR = int(Ret::Code::AUDIO_FIRST),

    // driver
    DRIVER_NOT_FOUND = 301,
    DRIVER_OPEN_FAILED = 302,

    // engine
    ENGINE_INVALID_PARAMETER = 310,

    AUDIO_STREAM_NOT_PRESENT = 320,

    // synth
    SYNTH_NOT_INITED = 330,
    NO_LOADED_SOUND_FONTS = 331,
    SOUND_FONT_FAILED_LOAD = 332,
    SOUND_FONT_FAILED_UNLOAD = 333,
    UNKNOWN_SYNTH_TYPE = 334,

    // common
    INVALID_TRACK_ID = 340,
    INVALID_MIXER_CHANNEL_ID = 341,
    INVALID_SEQUENCE_ID = 342,
    INVALID_SETUP_DATA = 343,
    INVALID_AUDIO_SOURCE = 344,
    INVALID_AUDIO_FILE_PATH = 345,
    INVALID_FX_PARAMS = 346,
    INVALID_AUDIO_SOURCE_PARAMS = 347,
    DISABLED_AUDIO_EXPORT = 348,
    NO_AUDIO_TO_EXPORT = 349,
    ERROR_ENCODE = 350,
    UNKNOWN_PLUGIN_TYPE = 351,
    INVALID_AUDIO_OUTPUT = 352,
    INVALID_SYNTH = 353,
    AUDIO_CONTEXT_ALREADY_EXISTS = 354,
    INVALID_CONTEXT = 355,
    INVALID_RPC_DATA = 356,
    NO_SIGNAL_NODE = 357,

    // clock
    INVALID_TIME_LOOP = 360,

    // online sounds
    ONLINE_SOUNDS_PROCESSING_ERROR = 361,
    ONLINE_SOUNDS_LIMIT_REACHED = 362,
};

inline Ret make_ret(Err e) { return Ret(static_cast<int>(e)); }

inline Ret make_ret(Err e, const std::string &text) { return Ret(static_cast<int>(e), text); }
} // namespace muse::audio

#endif // MUSE_AUDIO_AUDIOERRORS_H
