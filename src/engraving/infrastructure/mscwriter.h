/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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
#pragma once
#include <utility>
#include <variant>

#include "io/iodevice.h"
#include "io/path.h"
#include "mscio.h"
#include "types/ret.h"
#include "types/string.h"

namespace muse {
struct ZipWriter;
struct TextStream;
} // namespace muse

namespace mu::engraving {
struct MscWriter {

    struct Params {
        muse::io::IODevice *device = nullptr;
        muse::io::PathT file_path;
        muse::String main_file_name;
        MscIoMode mode = MscIoMode::ZIP;
    };

    MscWriter() = default;
    MscWriter(const Params &params);
    ~MscWriter();

    void set_params(const Params &params);
    const Params &params() const;

    muse::Ret open();
    void close();
    bool is_opened() const;
    bool has_error() const;

    void write_style_file(const muse::ByteArray &data);
    void write_score_file(const muse::ByteArray &data);
    void add_excerpt_style_file(const muse::String &excerpt_file_name, const muse::ByteArray &data);
    void add_excerpt_file(const muse::String &excerpt_file_name, const muse::ByteArray &data);
    void write_chord_list_file(const muse::ByteArray &data);
    void write_thumbnail_file(const muse::ByteArray &data);
    void add_image_file(const muse::String &file_name, const muse::ByteArray &data);
    void write_audio_settings_json_file(const muse::ByteArray &data, const muse::io::PathT &path_prefix = "");
    void write_view_settings_json_file(const muse::ByteArray &data, const muse::io::PathT &path_prefix = "");
    void write_automation_json_file(const muse::ByteArray &data);

private:

    struct ZipFileWriter {
        ~ZipFileWriter();
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path);
        void close();
        bool is_opened() const;
        bool has_error() const;
        bool add_file_data(const muse::String &file_name, const muse::ByteArray &data);

    private:
        muse::io::IODevice *device_ = nullptr;
        muse::ZipWriter *zip_ = nullptr;
    };

    struct DirWriter {
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path);
        void close();
        bool is_opened() const;
        bool has_error() const;
        bool add_file_data(const muse::String &file_name, const muse::ByteArray &data);
    private:
        muse::io::PathT root_path_;
        bool has_error_ = false;
    };

    struct XmlFileWriter {
        ~XmlFileWriter();
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path);
        void close();
        bool is_opened() const;
        bool has_error() const;
        bool add_file_data(const muse::String &file_name, const muse::ByteArray &data);
    private:
        muse::io::IODevice *device_ = nullptr;
        muse::TextStream *stream_ = nullptr;
    };

    struct Meta {
        std::vector<muse::String> files;
        bool is_written = false;

        bool contains(const muse::String &file) const;
        void add_file(const muse::String &file);
    };

    struct Writer {
        template <typename T>
        explicit Writer(std::in_place_type_t<T> type) : implementation(type) {}

        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) {
            return std::visit([&](auto &backend) -> muse::Ret { return backend.open(device, file_path); }, implementation);
        }

        void close() {
            return std::visit([&](auto &backend) -> void { return backend.close(); }, implementation);
        }

        bool is_opened() const {
            return std::visit([&](auto &backend) -> bool { return backend.is_opened(); }, implementation);
        }

        bool has_error() const {
            return std::visit([&](auto &backend) -> bool { return backend.has_error(); }, implementation);
        }

        bool add_file_data(const muse::String &file_name, const muse::ByteArray &data) {
            return std::visit([&](auto &backend) -> bool { return backend.add_file_data(file_name, data); }, implementation);
        }

        std::variant<ZipFileWriter, DirWriter, XmlFileWriter> implementation;
    };

    Writer *writer() const;

    bool add_file_data(const muse::String &file_name, const muse::ByteArray &data);

    void write_meta();
    void write_container(const std::vector<muse::String> &paths);

    muse::String main_file_name() const;

    Params params_;
    mutable Writer *writer_ = nullptr;
    Meta meta_;
    bool had_error_ = false;
};
} // namespace mu::engraving
