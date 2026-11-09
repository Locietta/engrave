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

#include "mp3encoder.h"

#include <cmath>

#ifdef SYSTEM_LAME
#include <lame/lame.h>
#else
#include "lame.h"
#endif

#include "audio/common/audioerrors.h"
#include "global/io/iodevice.h"
#include "log.h"

using namespace muse::audio;
using namespace muse::audio::encode;

struct LameHandler {
    explicit LameHandler(const SoundTrackFormat &format) {
        DO_ASSERT(flags);

        lame_set_errorf(flags, [](const char *msg, va_list /*ap*/) { LOGE() << msg; });
        lame_set_debugf(flags, [](const char *msg, va_list /*ap*/) { LOGD() << msg; });
        lame_set_msgf(flags, [](const char *msg, va_list /*ap*/) { LOGI() << msg; });

        lame_set_num_channels(flags, format.output_spec.audio_channel_count);
        lame_set_in_samplerate(flags, format.output_spec.sample_rate);
        lame_set_brate(flags, format.bit_rate);
    }

    ~LameHandler() noexcept { lame_close(flags); }

    bool init() { return lame_init_params(flags) >= 0; }

    lame_global_flags *flags = lame_init();
};

Mp3Encoder::Mp3Encoder(const SoundTrackFormat &format, io::IODevice &dst_device)
    : format_(format), handler_{std::make_unique<LameHandler>(format)}, dst_device_{&dst_device} {
    DO_ASSERT(dst_device_);
}

Mp3Encoder::~Mp3Encoder() noexcept = default;

bool Mp3Encoder::begin(const samples_t /*totalSamplesNumber*/) {
    // LAME (lame.h): mp3buf for one encode call — worst case ≈ 1.25 * num_samples_per_channel + 7200 bytes;
    // flush needs at least 7200. Same buffer is used for encode + lame_encode_flush, hence margin + ceil.
    const samples_t n = format_.output_spec.samples_per_channel > 0 ? format_.output_spec.samples_per_channel : 4096;
    const double sz = 7200.0 + 1.25 * static_cast<double>(n) + 7200.0;
    output_buffer_.resize(static_cast<size_t>(std::ceil(sz)) + 512);

    if (!handler_->init()) {
        return false;
    }

    return true;
}

size_t Mp3Encoder::encode(const samples_t samples_per_channel, const float *input) {
    if (samples_per_channel > format_.output_spec.samples_per_channel) {
        LOGE() << "Chunk size exceeds buffer capacity";
        return 0;
    }

    const int encoded_bytes = lame_encode_buffer_interleaved_ieee_float(handler_->flags, input, static_cast<int>(samples_per_channel),
                                                                        output_buffer_.data(), static_cast<int>(output_buffer_.size()));

    if (encoded_bytes < 0) {
        LOGE() << "LAME encoder failed: " << encoded_bytes;
        return 0;
    }

    if (encoded_bytes > 0) {
        const size_t written = dst_device_->write(output_buffer_.data(), static_cast<size_t>(encoded_bytes));
        if (written != static_cast<size_t>(encoded_bytes)) {
            return 0;
        }
    }

    return samples_per_channel;
}

muse::Ret Mp3Encoder::end() {
    const int encoded_bytes = lame_encode_flush(handler_->flags, output_buffer_.data(), static_cast<int>(output_buffer_.size()));
    if (encoded_bytes < 0) return make_ret(Err::ERROR_ENCODE, "MP3 finalization failed: " + std::to_string(encoded_bytes));
    if (encoded_bytes > 0 &&
        dst_device_->write(output_buffer_.data(), static_cast<size_t>(encoded_bytes)) != static_cast<size_t>(encoded_bytes))
        return make_ret(Err::ERROR_ENCODE, "MP3 finalization write failed");
    return muse::make_ok();
}
