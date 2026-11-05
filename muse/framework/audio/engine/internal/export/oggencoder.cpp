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

#include "oggencoder.h"

#ifdef SYSTEM_OPUSENC
#include <opus/opusenc.h>
#else
#include "opusenc.h"
#endif

#include <algorithm>
#include <climits>

#include "global/io/iodevice.h"
#include "audio/common/audioerrors.h"

#include "log.h"

using namespace muse::audio;
using namespace muse::audio::encode;

OggEncoder::OggEncoder(const SoundTrackFormat &format, io::IODevice &dstDevice) : AbstractAudioEncoder(format), dstDevice_{&dstDevice} {
    DO_ASSERT(dstDevice_);
}

OggEncoder::~OggEncoder() noexcept {
    if (opusEncoder_) {
        ope_encoder_destroy(opusEncoder_);
    }
}

bool OggEncoder::begin(const samples_t) {
    OggOpusComments *comments = ope_comments_create();
    if (!comments) {
        return false;
    }

    int error = 0;

    const OpusEncCallbacks callbacks{[](void *userData, const unsigned char *ptr, opus_int32 len) -> int {
                                         auto *dstDevice = static_cast<io::IODevice *>(userData);
                                         IF_ASSERT_FAILED(dstDevice) { return 1; }

                                         const auto numBytesToWrite = static_cast<std::size_t>(len);
                                         if (dstDevice->write(ptr, numBytesToWrite) != numBytesToWrite) {
                                             return 1;
                                         }

                                         return 0;
                                     },
                                     [](void *) -> int { return 0; }};

    opusEncoder_ = ope_encoder_create_callbacks(&callbacks, dstDevice_, comments, format_.outputSpec.sampleRate,
                                                format_.outputSpec.audioChannelCount, 0, &error);
    ope_comments_destroy(comments);

    if (error != OPE_OK || !opusEncoder_) {
        if (opusEncoder_) {
            ope_encoder_destroy(opusEncoder_);
            opusEncoder_ = nullptr;
        }
        return false;
    }

    ope_encoder_ctl(opusEncoder_, OPUS_SET_BITRATE(format_.bitRate * 1000));
    // libopusenc otherwise draws the Ogg stream serial from the process-global rand(),
    // which makes identical exports differ by page headers. A single-stream file only
    // needs a serial that is constant within the file.
    ope_encoder_ctl(opusEncoder_, OPE_SET_SERIALNO(0x4D757365));

    return true;
}

size_t OggEncoder::encode(const samples_t samplesPerChannel, const float *input) {
    IF_ASSERT_FAILED(opusEncoder_) { return 0; }

    if (samplesPerChannel == 0) {
        return 0;
    }

    const int n = static_cast<int>(std::min(samplesPerChannel, static_cast<samples_t>(INT_MAX)));
    const int code = ope_encoder_write_float(opusEncoder_, input, n);

    return code == OPE_OK ? static_cast<size_t>(n) : 0;
}

muse::Ret OggEncoder::end() {
    IF_ASSERT_FAILED(opusEncoder_) { return make_ret(Err::ErrorEncode, "Opus encoder is not initialized"); }

    const int code = ope_encoder_drain(opusEncoder_);
    return code == OPE_OK ? muse::make_ok() : make_ret(Err::ErrorEncode, "Opus finalization failed: " + std::to_string(code));
}
