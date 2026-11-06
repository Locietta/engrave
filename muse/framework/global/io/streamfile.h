// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include "iodevice.h"
#include "ioretcodes.h"

namespace muse::io {
class StreamingFileDevice final : public IODevice {
public:
    explicit StreamingFileDevice(std::filesystem::path path, std::optional<std::uint64_t> max_bytes = {},
                                 std::optional<std::uint64_t> max_write_bytes = {}, std::function<bool()> cancelled = {})
        : path_(std::move(path)), max_bytes_(max_bytes), max_write_bytes_(max_write_bytes), cancelled_(std::move(cancelled)) {}

    ~StreamingFileDevice() override { close(); }

    bool limit_exceeded() const { return limit_exceeded_; }

protected:
    bool do_open(OpenMode mode) override {
        if (mode != WRITE_ONLY) {
            set_error(static_cast<int>(io::Err::FS_WRITE_ERROR), "audio output is write-only");
            return false;
        }

        // The path is UTF-8; std::filesystem::path keeps it away from the
        // process ANSI code page on Windows.
        stream_.open(path_, std::ios::binary | std::ios::trunc);
        if (!stream_.is_open()) {
            set_error(static_cast<int>(io::Err::FS_WRITE_ERROR), "cannot open audio output for writing");
            return false;
        }

        size_ = 0;
        stream_pos_ = 0;
        written_ = 0;
        limit_exceeded_ = false;
        return true;
    }

    void do_close() override {
        if (stream_.is_open()) {
            stream_.flush();
            const bool flushed = stream_.good();
            stream_.close();
            if (!flushed || stream_.fail()) {
                set_error(static_cast<int>(io::Err::FS_WRITE_ERROR), "cannot finalize audio output");
            }
        }
    }

    size_t data_size() const override { return size_; }

    const uint8_t *raw_data() const override { return nullptr; }

    bool resize_data(size_t size) override {
        if (limit_exceeded_ || (max_bytes_ && size > *max_bytes_)) {
            limit_exceeded_ = true;
            return false;
        }
        if (size > static_cast<size_t>(std::numeric_limits<std::streamoff>::max())) return false;
        size_ = std::max(size_, size);
        return true;
    }

    size_t write_data(const uint8_t *data, size_t len) override {
        if (cancelled_ && cancelled_()) return 0;
        // IODevice can overwrite already allocated space without resizeData.
        // Check both limits before touching the file, including final header
        // rewrites and writes after a previously rejected operation.
        if (limit_exceeded_ || (max_bytes_ && (pos() > *max_bytes_ || len > *max_bytes_ - pos())) ||
            (max_write_bytes_ && (written_ > *max_write_bytes_ || len > *max_write_bytes_ - written_))) {
            limit_exceeded_ = true;
            return 0;
        }
        if (!stream_.is_open() || len > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
            set_error(static_cast<int>(io::Err::FS_WRITE_ERROR), "invalid stream write");
            return 0;
        }

        // Only seek for the non-sequential writes (the WAV header patch);
        // std::ofstream::seekp flushes the put buffer, so doing it per write
        // would turn a buffered stream into one syscall per sample.
        if (stream_pos_ != pos()) {
            stream_.seekp(static_cast<std::streamoff>(pos()));
            if (!stream_) {
                set_error(static_cast<int>(io::Err::FS_WRITE_ERROR), "cannot seek output");
                return 0;
            }
        }

        // Bound each file write, including writes issued inside codec flushes.
        for (size_t offset = 0; offset < len;) {
            if (offset && cancelled_ && cancelled_()) return 0;
            const size_t count = std::min<size_t>(4096, len - offset);
            stream_.write(reinterpret_cast<const char *>(data + offset), static_cast<std::streamsize>(count));
            if (!stream_) {
                set_error(static_cast<int>(io::Err::FS_WRITE_ERROR), "cannot write output");
                return 0;
            }
            offset += count;
        }

        stream_pos_ = pos() + len;
        written_ = len > std::numeric_limits<std::uint64_t>::max() - written_ ? std::numeric_limits<std::uint64_t>::max() : written_ + len;
        return len;
    }

private:
    std::filesystem::path path_;
    std::ofstream stream_;
    size_t size_ = 0;
    size_t stream_pos_ = 0;
    std::optional<std::uint64_t> max_bytes_;
    std::optional<std::uint64_t> max_write_bytes_;
    std::uint64_t written_ = 0;
    bool limit_exceeded_ = false;
    std::function<bool()> cancelled_;
};
} // namespace muse::io
