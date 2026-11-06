/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "aacencoder.h"

#include <algorithm>
#include <vector>

#include "FDK_audio.h"
#include "aacenc_lib.h"

#include "../dsp/audiomathutils.h"
#include "../offlinecancellation.h"
#include "audio/common/audioerrors.h"
#include "global/io/iodevice.h"
#include "log.h"

using namespace muse::audio;
using namespace muse::audio::encode;

namespace muse::audio::encode {
struct AacEncoderHandler {
    HANDLE_AACENCODER handle = nullptr;
    AACENC_InfoStruct info = {};
    std::vector<INT_PCM> input_buffer;
    std::vector<unsigned char> output_buffer;
    size_t frame_length = 0;
    int channels = 0;

    ~AacEncoderHandler() {
        if (handle) {
            aacEncClose(&handle);
        }
    }

    bool init(const SoundTrackFormat &format) {
        channels = format.output_spec.audio_channel_count;
        if (aacEncOpen(&handle, 0, channels) != AACENC_OK) {
            LOGE() << "Failed to open AAC encoder";
            return false;
        }

        if (aacEncoder_SetParam(handle, AACENC_AOT, AOT_AAC_LC) != AACENC_OK) {
            LOGE() << "Failed to set AAC AOT";
            return false;
        }

        if (aacEncoder_SetParam(handle, AACENC_SAMPLERATE, format.output_spec.sample_rate) != AACENC_OK) {
            LOGE() << "Failed to set sample rate";
            return false;
        }

        CHANNEL_MODE mode = MODE_1;
        switch (channels) {
            case 1: mode = MODE_1; break;
            case 2: mode = MODE_2; break;
            case 3: mode = MODE_1_2; break;
            case 4: mode = MODE_1_2_1; break;
            case 5: mode = MODE_1_2_2; break;
            case 6: mode = MODE_1_2_2_1; break;
            default: LOGE() << "Unsupported channel count:" << channels; return false;
        }

        if (aacEncoder_SetParam(handle, AACENC_CHANNELMODE, mode) != AACENC_OK) {
            LOGE() << "Failed to set channel mode";
            return false;
        }

        if (aacEncoder_SetParam(handle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
            LOGE() << "Failed to set channel order (WAV)";
            return false;
        }

        if (aacEncoder_SetParam(handle, AACENC_BITRATE, format.bit_rate * 1000) != AACENC_OK) {
            LOGE() << "Failed to set bitrate";
            return false;
        }

        if (aacEncoder_SetParam(handle, AACENC_TRANSMUX, TT_MP4_ADTS) != AACENC_OK) {
            LOGE() << "Failed to set ADTS transport";
            return false;
        }

        if (aacEncoder_SetParam(handle, AACENC_AFTERBURNER, 1) != AACENC_OK) {
            LOGE() << "Failed to set afterburner";
            return false;
        }

        if (aacEncEncode(handle, nullptr, nullptr, nullptr, nullptr) != AACENC_OK) {
            LOGE() << "Failed to initialize AAC encoder";
            return false;
        }

        if (aacEncInfo(handle, &info) != AACENC_OK) {
            LOGE() << "Failed to get AAC encoder info";
            return false;
        }

        frame_length = info.frameLength;
        output_buffer.resize(info.maxOutBufBytes);
        input_buffer.resize(channels * frame_length);

        return true;
    }
};
} // namespace muse::audio::encode

AacEncoder::AacEncoder(const SoundTrackFormat &format, io::IODevice &dst_device)
    : AbstractAudioEncoder(format), handler_{std::make_unique<AacEncoderHandler>()}, dst_device_{&dst_device} {
    DO_ASSERT(dst_device_);
}

AacEncoder::~AacEncoder() noexcept = default;

bool AacEncoder::begin(const samples_t /*totalSamplesNumber*/) {
    if (!format_.is_valid()) {
        return false;
    }

    return handler_->init(format_);
}

size_t AacEncoder::encode(samples_t samples_per_channel, const float *input) {
    IF_ASSERT_FAILED(handler_ && dst_device_) { return 0; }

    const int channels = handler_->channels;
    const size_t frame_length = handler_->frame_length;
    const size_t samples_per_frame = frame_length * channels;

    size_t input_offset = 0;
    const size_t total_samples = samples_per_channel * static_cast<size_t>(channels);

    while (input_offset < total_samples) {
        if (OfflineCancellation::requested()) return 0;
        const size_t samples_to_process = std::min(samples_per_frame, total_samples - input_offset);

        for (size_t i = 0; i < samples_to_process; ++i) {
            handler_->input_buffer[i] = dsp::convert_float_samples<INT_PCM>(input[input_offset + i], 16);
        }

        AACENC_BufDesc in_buf_desc = {};
        AACENC_BufDesc out_buf_desc = {};
        AACENC_InArgs in_args = {};
        AACENC_OutArgs out_args = {};

        int in_identifier = IN_AUDIO_DATA;
        int in_size = static_cast<int>(samples_to_process * sizeof(INT_PCM));
        int in_elem_size = sizeof(INT_PCM);
        void *in_ptr = handler_->input_buffer.data();

        int out_identifier = OUT_BITSTREAM_DATA;
        int out_size = static_cast<int>(handler_->output_buffer.size());
        int out_elem_size = 1;
        void *out_ptr = handler_->output_buffer.data();

        in_buf_desc.numBufs = 1;
        in_buf_desc.bufs = &in_ptr;
        in_buf_desc.bufferIdentifiers = &in_identifier;
        in_buf_desc.bufSizes = &in_size;
        in_buf_desc.bufElSizes = &in_elem_size;

        out_buf_desc.numBufs = 1;
        out_buf_desc.bufs = &out_ptr;
        out_buf_desc.bufferIdentifiers = &out_identifier;
        out_buf_desc.bufSizes = &out_size;
        out_buf_desc.bufElSizes = &out_elem_size;

        in_args.numInSamples = static_cast<int>(samples_to_process);

        const AACENC_ERROR err = aacEncEncode(handler_->handle, &in_buf_desc, &out_buf_desc, &in_args, &out_args);

        if (err == AACENC_ENCODE_EOF) {
            LOGE() << "AAC encoding unexpected EOF";
            return 0;
        }
        if (err != AACENC_OK) {
            LOGE() << "AAC encoding failed:" << err;
            return 0;
        }

        if (out_args.numOutBytes > 0) {
            const size_t written = dst_device_->write(handler_->output_buffer.data(), static_cast<size_t>(out_args.numOutBytes));
            if (written != static_cast<size_t>(out_args.numOutBytes)) {
                return 0;
            }
        }

        input_offset += samples_to_process;

        if (samples_to_process < samples_per_frame) {
            break;
        }
    }

    if (input_offset != total_samples) {
        return 0;
    }

    return static_cast<size_t>(samples_per_channel) * static_cast<size_t>(channels);
}

muse::Ret AacEncoder::end() {
    IF_ASSERT_FAILED(handler_ && dst_device_) { return make_ret(Err::ERROR_ENCODE, "AAC encoder is not initialized"); }

    AACENC_BufDesc in_buf_desc = {};
    AACENC_BufDesc out_buf_desc = {};
    AACENC_InArgs in_args = {};
    AACENC_OutArgs out_args = {};

    int in_identifier = IN_AUDIO_DATA;
    int in_size = 0;
    int in_elem_size = sizeof(INT_PCM);
    void *in_ptr = nullptr;

    int out_identifier = OUT_BITSTREAM_DATA;
    int out_size = static_cast<int>(handler_->output_buffer.size());
    int out_elem_size = 1;
    void *out_ptr = handler_->output_buffer.data();

    in_buf_desc.numBufs = 1;
    in_buf_desc.bufs = &in_ptr;
    in_buf_desc.bufferIdentifiers = &in_identifier;
    in_buf_desc.bufSizes = &in_size;
    in_buf_desc.bufElSizes = &in_elem_size;

    out_buf_desc.numBufs = 1;
    out_buf_desc.bufs = &out_ptr;
    out_buf_desc.bufferIdentifiers = &out_identifier;
    out_buf_desc.bufSizes = &out_size;
    out_buf_desc.bufElSizes = &out_elem_size;

    in_args.numInSamples = -1;

    while (true) {
        if (OfflineCancellation::requested()) return make_ret(Ret::Code::CANCEL);
        AACENC_ERROR err = aacEncEncode(handler_->handle, &in_buf_desc, &out_buf_desc, &in_args, &out_args);

        if (err == AACENC_ENCODE_EOF) {
            break;
        }
        if (err != AACENC_OK) {
            return make_ret(Err::ERROR_ENCODE, "AAC finalization failed: " + std::to_string(static_cast<int>(err)));
        }

        if (out_args.numOutBytes > 0) {
            const size_t written = dst_device_->write(handler_->output_buffer.data(), static_cast<size_t>(out_args.numOutBytes));
            if (written != static_cast<size_t>(out_args.numOutBytes)) return make_ret(Err::ERROR_ENCODE, "AAC finalization write failed");
        }
    }

    return muse::make_ok();
}
