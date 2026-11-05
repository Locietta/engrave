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
    explicit StreamingFileDevice(std::filesystem::path path, std::optional<std::uint64_t> maxBytes = {},
                                 std::optional<std::uint64_t> maxWriteBytes = {}, std::function<bool()> cancelled = {})
        : path_(std::move(path)), maxBytes_(maxBytes), maxWriteBytes_(maxWriteBytes), cancelled_(std::move(cancelled)) {}

    ~StreamingFileDevice() override { close(); }

    bool limitExceeded() const { return limitExceeded_; }

protected:
    bool doOpen(OpenMode mode) override {
        if (mode != WriteOnly) {
            setError(static_cast<int>(io::Err::FSWriteError), "audio output is write-only");
            return false;
        }

        // The path is UTF-8; std::filesystem::path keeps it away from the
        // process ANSI code page on Windows.
        stream_.open(path_, std::ios::binary | std::ios::trunc);
        if (!stream_.is_open()) {
            setError(static_cast<int>(io::Err::FSWriteError), "cannot open audio output for writing");
            return false;
        }

        size_ = 0;
        streamPos_ = 0;
        written_ = 0;
        limitExceeded_ = false;
        return true;
    }

    void doClose() override {
        if (stream_.is_open()) {
            stream_.flush();
            const bool flushed = stream_.good();
            stream_.close();
            if (!flushed || stream_.fail()) {
                setError(static_cast<int>(io::Err::FSWriteError), "cannot finalize audio output");
            }
        }
    }

    size_t dataSize() const override { return size_; }

    const uint8_t *rawData() const override { return nullptr; }

    bool resizeData(size_t size) override {
        if (limitExceeded_ || (maxBytes_ && size > *maxBytes_)) {
            limitExceeded_ = true;
            return false;
        }
        if (size > static_cast<size_t>(std::numeric_limits<std::streamoff>::max())) return false;
        size_ = std::max(size_, size);
        return true;
    }

    size_t writeData(const uint8_t *data, size_t len) override {
        if (cancelled_ && cancelled_()) return 0;
        // IODevice can overwrite already allocated space without resizeData.
        // Check both limits before touching the file, including final header
        // rewrites and writes after a previously rejected operation.
        if (limitExceeded_ || (maxBytes_ && (pos() > *maxBytes_ || len > *maxBytes_ - pos())) ||
            (maxWriteBytes_ && (written_ > *maxWriteBytes_ || len > *maxWriteBytes_ - written_))) {
            limitExceeded_ = true;
            return 0;
        }
        if (!stream_.is_open() || len > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())) {
            setError(static_cast<int>(io::Err::FSWriteError), "invalid stream write");
            return 0;
        }

        // Only seek for the non-sequential writes (the WAV header patch);
        // std::ofstream::seekp flushes the put buffer, so doing it per write
        // would turn a buffered stream into one syscall per sample.
        if (streamPos_ != pos()) {
            stream_.seekp(static_cast<std::streamoff>(pos()));
            if (!stream_) {
                setError(static_cast<int>(io::Err::FSWriteError), "cannot seek output");
                return 0;
            }
        }

        // Bound each file write, including writes issued inside codec flushes.
        for (size_t offset = 0; offset < len;) {
            if (offset && cancelled_ && cancelled_()) return 0;
            const size_t count = std::min<size_t>(4096, len - offset);
            stream_.write(reinterpret_cast<const char *>(data + offset), static_cast<std::streamsize>(count));
            if (!stream_) {
                setError(static_cast<int>(io::Err::FSWriteError), "cannot write output");
                return 0;
            }
            offset += count;
        }

        streamPos_ = pos() + len;
        written_ = len > std::numeric_limits<std::uint64_t>::max() - written_ ? std::numeric_limits<std::uint64_t>::max() : written_ + len;
        return len;
    }

private:
    std::filesystem::path path_;
    std::ofstream stream_;
    size_t size_ = 0;
    size_t streamPos_ = 0;
    std::optional<std::uint64_t> maxBytes_;
    std::optional<std::uint64_t> maxWriteBytes_;
    std::uint64_t written_ = 0;
    bool limitExceeded_ = false;
    std::function<bool()> cancelled_;
};
} // namespace muse::io
