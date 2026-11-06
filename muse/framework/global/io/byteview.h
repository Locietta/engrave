// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <span>

#include "iodevice.h"

namespace muse::io {
// A synchronous reader over caller-owned bytes. The span must outlive the
// device and every consumer. Writes are rejected; no mutable alias is exposed.
struct ByteView final : IODevice {
    explicit ByteView(std::span<const uint8_t> bytes) : bytes_(bytes) {}

protected:
    bool do_open(OpenMode mode) override { return mode == READ_ONLY; }

    size_t data_size() const override { return bytes_.size(); }

    const uint8_t *raw_data() const override { return bytes_.data(); }

    bool resize_data(size_t) override { return false; }

    size_t write_data(const uint8_t *, size_t) override { return 0; }

private:
    std::span<const uint8_t> bytes_;
};
} // namespace muse::io
