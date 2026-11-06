/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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
#pragma once

#include <cstdint>
#include <string>
#include <map>

#include "global/types/bytearray.h"

namespace muse::io {
struct IODevice {

    enum OpenMode { UNKNOWN, READ_ONLY, WRITE_ONLY, READ_WRITE, APPEND };

    IODevice() = default;
    virtual ~IODevice() = default;

    bool open(OpenMode m = READ_ONLY);
    void close();

    bool is_open() const;
    OpenMode open_mode() const;
    bool is_readable() const;
    bool is_writeable() const;

    size_t size() const;
    size_t pos() const;

    bool seek(size_t pos);

    size_t read(uint8_t *data, size_t len);
    ByteArray read(size_t count);
    ByteArray read_all();

    const uint8_t *read_data();

    size_t write(const uint8_t *data, size_t len);
    size_t write(const ByteArray &ba);

    const std::string &meta(const std::string &key) const;
    void set_meta(const std::string &key, const std::string &val);

    bool has_error() const;
    int error() const;
    const std::string &error_string() const;

protected:

    virtual bool do_open(OpenMode m) = 0;

    virtual void do_close() {}

    virtual ByteArray do_read_all();
    virtual size_t data_size() const = 0;
    virtual const uint8_t *raw_data() const = 0;
    virtual bool resize_data(size_t size) = 0;
    virtual size_t write_data(const uint8_t *data, size_t len) = 0;

    bool is_open_mode_readable() const;
    bool is_open_mode_writeable() const;

    void set_error(int error, const std::string &error_string);

private:

    const uint8_t *cdata_offsetted() const;

    OpenMode mode_ = OpenMode::UNKNOWN;
    size_t pos_ = 0;
    std::map<std::string, std::string> meta_;

    int error_ = 0;
    std::string error_string_;
};
} // namespace muse::io
