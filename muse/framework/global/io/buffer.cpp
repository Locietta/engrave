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
#include "buffer.h"
#include "writebudget.h"

#include <cstring>
#include <utility>

#include "global/log.h"

using namespace muse;
using namespace muse::io;

Buffer Buffer::opened(const OpenMode mode, ByteArray *ba) {
    Buffer buf(ba);
    DO_ASSERT(buf.open(mode));

    return buf;
}

Buffer Buffer::opened(const OpenMode mode, ByteArray &&ba) {
    Buffer buf(std::move(ba));
    DO_ASSERT(buf.open(mode));

    return buf;
}

Buffer::Buffer() { ref_ = &ba_; }

Buffer::Buffer(size_t size) {
    ba_.resize(size);
    ref_ = &ba_;
}

Buffer::Buffer(const uint8_t *data, size_t size) {
    ba_ = ByteArray(data, size);
    ref_ = &ba_;
}

Buffer::Buffer(ByteArray *ba) : ref_(ba) {
    if (!ref_) {
        ref_ = &ba_;
    }
}

Buffer::Buffer(ByteArray &&ba) {
    ba_ = std::move(ba);
    ref_ = &ba_;
}

const ByteArray &Buffer::data() const { return *ref_; }

ByteArray Buffer::do_read_all() {
    IF_ASSERT_FAILED(is_open_mode_readable()) { return ByteArray(); }

    if (pos() != 0) {
        return IODevice::do_read_all();
    }

    seek(size());
    return data();
}

bool Buffer::do_open(OpenMode) { return true; }

size_t Buffer::data_size() const { return ref_->size(); }

const uint8_t *Buffer::raw_data() const { return ref_->const_data(); }

bool Buffer::resize_data(size_t size) {
    if (active_write_budget && size > ref_->size() && !active_write_budget->charge(size - ref_->size())) return false;
    ref_->resize(size);
    return true;
}

size_t Buffer::write_data(const uint8_t *data, size_t len) {
    if (active_write_budget && !active_write_budget->poll()) return 0;
    std::memcpy(ref_->data() + pos(), data, len);
    return len;
}
