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

#include "flacencoder.h"

#include <algorithm>
#include <array>
#include <vector>

#include "FLAC++/encoder.h"

#include "../dsp/audiomathutils.h"
#include "../offlinecancellation.h"
#include "audio/common/audioerrors.h"
#include "global/io/iodevice.h"
#include "log.h"

namespace muse::audio::encode {
struct FlacMetadataGuard {
    std::array<FLAC__StreamMetadata *, 2> blocks{};

    FlacMetadataGuard() = default;
    FlacMetadataGuard(const FlacMetadataGuard &) = delete;
    FlacMetadataGuard &operator=(const FlacMetadataGuard &) = delete;

    void clear() noexcept {
        for (FLAC__StreamMetadata *&block : blocks) {
            if (block) {
                FLAC__metadata_object_delete(block);
                block = nullptr;
            }
        }
    }

    ~FlacMetadataGuard() noexcept { clear(); }
};

struct FlacHandler final {
    struct Stream final : FLAC::Encoder::Stream {
        explicit Stream(io::IODevice &out_dev) : out_dev_{&out_dev} { DO_ASSERT(out_dev_); }

    protected:
        FLAC__StreamEncoderWriteStatus write_callback(const FLAC__byte buffer[], const size_t bytes, uint32_t, uint32_t) override {
            if (out_dev_->write(buffer, bytes) != bytes) {
                return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
            }

            return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
        }

        FLAC__StreamEncoderSeekStatus seek_callback(const FLAC__uint64 absolute_byte_offset) override {
            if (!out_dev_->seek(absolute_byte_offset)) {
                return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
            }

            return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
        }

        FLAC__StreamEncoderTellStatus tell_callback(FLAC__uint64 *absolute_byte_offset) override {
            *absolute_byte_offset = out_dev_->pos();
            return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
        }

    private:
        io::IODevice *out_dev_;
    };

    explicit FlacHandler(io::IODevice &out_dev) : stream(out_dev) {}

    // Declared before stream so libFLAC finishes tearing down before metadata blocks are freed.
    FlacMetadataGuard metadata;
    Stream stream;
};

FlacEncoder::FlacEncoder(const SoundTrackFormat &format, io::IODevice &dst_device)
    : format_(format), flac_{std::make_unique<FlacHandler>(dst_device)} {}

FlacEncoder::~FlacEncoder() noexcept = default;

bool FlacEncoder::begin(const samples_t total_samples_number) {
    int bits_per_sample = 0;
    switch (format_.sample_format) {
        case AudioSampleFormat::INT16: bits_per_sample = 16; break;
        case AudioSampleFormat::INT24: bits_per_sample = 24; break;
        default: return false;
    }

    auto &stream = flac_->stream;
    if (!stream.set_verify(true) || !stream.set_compression_level(0) || !stream.set_channels(format_.output_spec.audio_channel_count) ||
        !stream.set_sample_rate(format_.output_spec.sample_rate) || !stream.set_bits_per_sample(bits_per_sample) ||
        !stream.set_total_samples_estimate(total_samples_number)) {
        return false;
    }

    flac_->metadata.clear();
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    auto &metadata = flac_->metadata.blocks;
    metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
    metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING);
    FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", "Some Artist");
    FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false);
    FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "YEAR", "1984");
    FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, /*copy=*/false);

    metadata[1]->length = 1234; /* set the padding length */
    stream.set_metadata(metadata.data(), static_cast<uint32_t>(metadata.size()));

    if (stream.init() != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        return false;
    }

    return true;
}

size_t FlacEncoder::encode(const samples_t samples_per_channel, const float *input) {
    int bits_per_sample = 0;
    switch (format_.sample_format) {
        case AudioSampleFormat::INT16: bits_per_sample = 16; break;
        case AudioSampleFormat::INT24: bits_per_sample = 24; break;
        default: return 0;
    }

    const auto channels = format_.output_spec.audio_channel_count;
    if (channels == 0 || samples_per_channel == 0) {
        return 0;
    }

    constexpr uint32_t block_frames = 1024;
    std::vector<FLAC__int32> block_buffer(static_cast<size_t>(block_frames) * channels);

    for (samples_t frame_start = 0; frame_start < samples_per_channel;) {
        if (OfflineCancellation::requested()) return 0;
        const uint32_t frames_count = static_cast<uint32_t>(std::min<samples_t>(block_frames, samples_per_channel - frame_start));

        for (size_t frame = 0; frame < frames_count; ++frame) {
            for (audioch_t c = 0; c < channels; ++c) {
                const size_t i = (static_cast<size_t>(frame_start) + frame) * channels + c;
                block_buffer[static_cast<size_t>(frame) * channels + c] =
                    dsp::convert_float_samples<FLAC__int32>(input[i], bits_per_sample);
            }
        }

        if (!flac_->stream.process_interleaved(block_buffer.data(), frames_count)) {
            return 0;
        }

        frame_start += frames_count;
    }

    return static_cast<size_t>(samples_per_channel) * channels;
}

Ret FlacEncoder::end() { return flac_->stream.finish() ? make_ok() : make_ret(Err::ERROR_ENCODE, "FLAC finalization failed"); }
} // namespace muse::audio::encode
