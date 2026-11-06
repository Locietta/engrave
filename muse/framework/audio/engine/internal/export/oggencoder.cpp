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

OggEncoder::OggEncoder(const SoundTrackFormat &format, io::IODevice &dst_device) : AbstractAudioEncoder(format), dst_device_{&dst_device} {
    DO_ASSERT(dst_device_);
}

OggEncoder::~OggEncoder() noexcept {
    if (opus_encoder_) {
        ope_encoder_destroy(opus_encoder_);
    }
}

bool OggEncoder::begin(const samples_t) {
    OggOpusComments *comments = ope_comments_create();
    if (!comments) {
        return false;
    }

    int error = 0;

    const OpusEncCallbacks callbacks{[](void *user_data, const unsigned char *ptr, opus_int32 len) -> int {
                                         auto *dst_device = static_cast<io::IODevice *>(user_data);
                                         IF_ASSERT_FAILED(dst_device) { return 1; }

                                         const auto num_bytes_to_write = static_cast<std::size_t>(len);
                                         if (dst_device->write(ptr, num_bytes_to_write) != num_bytes_to_write) {
                                             return 1;
                                         }

                                         return 0;
                                     },
                                     [](void *) -> int { return 0; }};

    opus_encoder_ = ope_encoder_create_callbacks(&callbacks, dst_device_, comments, format_.output_spec.sample_rate,
                                                 format_.output_spec.audio_channel_count, 0, &error);
    ope_comments_destroy(comments);

    if (error != OPE_OK || !opus_encoder_) {
        if (opus_encoder_) {
            ope_encoder_destroy(opus_encoder_);
            opus_encoder_ = nullptr;
        }
        return false;
    }

    ope_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(format_.bit_rate * 1000));
    // libopusenc otherwise draws the Ogg stream serial from the process-global rand(),
    // which makes identical exports differ by page headers. A single-stream file only
    // needs a serial that is constant within the file.
    ope_encoder_ctl(opus_encoder_, OPE_SET_SERIALNO(0x4D757365));

    return true;
}

size_t OggEncoder::encode(const samples_t samples_per_channel, const float *input) {
    IF_ASSERT_FAILED(opus_encoder_) { return 0; }

    if (samples_per_channel == 0) {
        return 0;
    }

    const int n = static_cast<int>(std::min(samples_per_channel, static_cast<samples_t>(INT_MAX)));
    const int code = ope_encoder_write_float(opus_encoder_, input, n);

    return code == OPE_OK ? static_cast<size_t>(n) : 0;
}

muse::Ret OggEncoder::end() {
    IF_ASSERT_FAILED(opus_encoder_) { return make_ret(Err::ERROR_ENCODE, "Opus encoder is not initialized"); }

    const int code = ope_encoder_drain(opus_encoder_);
    return code == OPE_OK ? muse::make_ok() : make_ret(Err::ERROR_ENCODE, "Opus finalization failed: " + std::to_string(code));
}
