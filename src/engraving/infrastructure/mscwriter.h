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

#include "types/string.h"
#include "types/ret.h"
#include "io/path.h"
#include "io/iodevice.h"
#include "mscio.h"

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

    struct IWriter {
        virtual ~IWriter() = default;

        virtual muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) = 0;
        virtual void close() = 0;
        virtual bool is_opened() const = 0;
        virtual bool has_error() const = 0;
        virtual bool add_file_data(const muse::String &file_name, const muse::ByteArray &data) = 0;
    };

    struct ZipFileWriter : public IWriter {
        ~ZipFileWriter() override;
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) override;
        void close() override;
        bool is_opened() const override;
        bool has_error() const override;
        bool add_file_data(const muse::String &file_name, const muse::ByteArray &data) override;

    private:
        muse::io::IODevice *device_ = nullptr;
        muse::ZipWriter *zip_ = nullptr;
    };

    struct DirWriter : public IWriter {
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) override;
        void close() override;
        bool is_opened() const override;
        bool has_error() const override;
        bool add_file_data(const muse::String &file_name, const muse::ByteArray &data) override;
    private:
        muse::io::PathT root_path_;
        bool has_error_ = false;
    };

    struct XmlFileWriter : public IWriter {
        ~XmlFileWriter() override;
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) override;
        void close() override;
        bool is_opened() const override;
        bool has_error() const override;
        bool add_file_data(const muse::String &file_name, const muse::ByteArray &data) override;
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

    IWriter *writer() const;

    bool add_file_data(const muse::String &file_name, const muse::ByteArray &data);

    void write_meta();
    void write_container(const std::vector<muse::String> &paths);

    muse::String main_file_name() const;

    Params params_;
    mutable IWriter *writer_ = nullptr;
    Meta meta_;
    bool had_error_ = false;
};
} // namespace mu::engraving
