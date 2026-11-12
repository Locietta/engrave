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

#include "wavencoder.h"

#include <limits>

#include "../dsp/audiomathutils.h"
#include "audio/common/audioerrors.h"
#include "global/io/iodevice.h"
#include "global/types/bytearray.h"
#include "log.h"

namespace muse::audio::encode {
namespace {
template <typename T>
bool write_tag_data(io::IODevice &out_dev, const T value, size_t offset = 0) {
    if (offset != 0 && !out_dev.seek(offset)) {
        return false;
    }

    // little endian
    return out_dev.write(reinterpret_cast<const std::uint8_t *>(&value), sizeof(T)) == sizeof(T);
}

struct WavHeader {
    uint32_t chunk_size = 0;
    uint16_t audio_channels_number = 0;
    uint16_t bits_per_sample = 0;
    uint32_t samples_per_channel = 0;
    uint16_t code = 0;
    uint32_t sample_rate = 0;

    void write(io::IODevice &out_dev) {
        const uint32_t bytes_per_sample = bits_per_sample / 8;
        const uint32_t sample_data_length = audio_channels_number * samples_per_channel * bytes_per_sample;
        const uint32_t header_length = 20 + chunk_size + 8;
        const uint32_t file_length = header_length + sample_data_length;
        const uint32_t overall_size = file_length - 8;
        const uint32_t bytes_per_frame = audio_channels_number * bytes_per_sample;
        const uint32_t bytes_per_sec = audio_channels_number * sample_rate * bytes_per_sample;

        out_dev.write(reinterpret_cast<const uint8_t *>("RIFF"), 4); // chunk ID

        write_tag_data<uint32_t>(out_dev, overall_size);
        out_dev.write(reinterpret_cast<const uint8_t *>("WAVE"), 4); // WAVEID
        out_dev.write(reinterpret_cast<const uint8_t *>("fmt "), 4); // chunk ID

        write_tag_data<uint32_t>(out_dev, chunk_size);
        write_tag_data<uint16_t>(out_dev, code);
        write_tag_data<uint16_t>(out_dev, audio_channels_number);
        write_tag_data<uint32_t>(out_dev, sample_rate);
        write_tag_data<uint32_t>(out_dev, bytes_per_sec);
        write_tag_data<uint16_t>(out_dev, bytes_per_frame);
        write_tag_data<uint16_t>(out_dev, bits_per_sample);

        uint16_t ext = 0;
        if (chunk_size > 16) {
            write_tag_data<uint16_t>(out_dev, ext); // cbSize (extension size)
        }
        out_dev.write(reinterpret_cast<const uint8_t *>("data"), 4); // chunk ID

        write_tag_data<uint32_t>(out_dev, sample_data_length);
    }
};
} // namespace

WavEncoder::WavEncoder(const SoundTrackFormat &format, io::IODevice &dst_device) : format_(format), dst_device_{&dst_device} {
    DO_ASSERT(dst_device_);
}

bool WavEncoder::begin(const samples_t) {
    header_written_ = false;
    pcm_start_pos_ = 0;
    return true;
}

bool WavEncoder::write_placeholder_header() {
    WavHeader header;
    header.chunk_size = 18; // 18 is 2 bytes more to include cbsize field / extension size

    switch (format_.sample_format) {
        case AudioSampleFormat::INT16:
            header.bits_per_sample = 16;
            header.code = 1; // PCM
            break;
        case AudioSampleFormat::INT24:
            header.bits_per_sample = 24;
            header.code = 1; // PCM
            break;
        case AudioSampleFormat::FLOAT32:
            header.bits_per_sample = 32;
            header.code = 3; // IEEE_FLOAT
            break;
        case AudioSampleFormat::UNDEFINED:
        default: return false;
    }

    header.audio_channels_number = format_.output_spec.audio_channel_count;
    header.sample_rate = format_.output_spec.sample_rate;
    header.samples_per_channel = 0;

    header.write(*dst_device_);
    return true;
}

bool WavEncoder::patch_header_sizes() {
    const size_t end_pos = dst_device_->pos();
    if (end_pos < pcm_start_pos_ || end_pos < 8) {
        return false;
    }

    const uint64_t data_bytes = end_pos - pcm_start_pos_;
    if (data_bytes > std::numeric_limits<uint32_t>::max() || end_pos - 8 > std::numeric_limits<uint32_t>::max()) {
        LOGE() << "WAV export: file size exceeds 32-bit chunk limits";
        return false;
    }

    const uint32_t data_size = static_cast<uint32_t>(data_bytes);
    const uint32_t riff_chunk_size = static_cast<uint32_t>(end_pos - 8);

    bool ok = write_tag_data<uint32_t>(*dst_device_, riff_chunk_size, 4); // RIFF chunk size at offset 4
    ok = write_tag_data<uint32_t>(*dst_device_, data_size, 42) && ok;     // data chunk size at offset 42
    return dst_device_->seek(end_pos) && ok;
}

size_t WavEncoder::encode(const samples_t samples_per_channel, const float *input) {
    if (!header_written_) {
        if (!write_placeholder_header()) {
            return 0;
        }
        pcm_start_pos_ = dst_device_->pos();
        header_written_ = true;
    }

    const int channels = format_.output_spec.audio_channel_count;

    if (format_.sample_format == AudioSampleFormat::FLOAT32) {
        for (samples_t sample_idx = 0; sample_idx < samples_per_channel; ++sample_idx) {
            for (audioch_t audio_ch_num = 0; audio_ch_num < channels; ++audio_ch_num) {
                const int idx = static_cast<int>(sample_idx) * channels + audio_ch_num;
                if (dst_device_->write(reinterpret_cast<const std::uint8_t *>(input + idx), 4) != 4) {
                    return 0;
                }
            }
        }
    } else {
        int bits_per_sample = 0;
        switch (format_.sample_format) {
            case AudioSampleFormat::INT16: bits_per_sample = 16; break;
            case AudioSampleFormat::INT24: bits_per_sample = 24; break;
            default: return 0;
        }

        const int bytes_to_write = bits_per_sample / 8;

        for (samples_t sample_idx = 0; sample_idx < samples_per_channel; ++sample_idx) {
            for (audioch_t audio_ch_num = 0; audio_ch_num < channels; ++audio_ch_num) {
                const int idx = static_cast<int>(sample_idx) * channels + audio_ch_num;

                const int32_t sample_int = dsp::convert_float_samples<int32_t>(input[idx], bits_per_sample);

                if (dst_device_->write(reinterpret_cast<const std::uint8_t *>(&sample_int), bytes_to_write) !=
                    static_cast<size_t>(bytes_to_write)) {
                    return 0;
                }
            }
        }
    }

    return static_cast<size_t>(samples_per_channel) * static_cast<size_t>(channels);
}

Ret WavEncoder::end() {
    if (header_written_ && !patch_header_sizes()) return make_ret(Err::ERROR_ENCODE, "WAV header finalization failed");
    return make_ok();
}
} // namespace muse::audio::encode
