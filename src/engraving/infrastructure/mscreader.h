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

#include "io/iodevice.h"
#include "io/path.h"
#include "mscio.h"
#include "types/ret.h"
#include "types/string.h"

namespace muse {
struct ZipReader;
} // namespace muse

namespace mu::engraving {
struct MscReader {

    struct Params {
        muse::io::IODevice *device = nullptr;
        muse::io::PathT file_path;
        muse::String main_file_name;
        MscIoMode mode = MscIoMode::ZIP;
    };

    MscReader() = default;
    MscReader(const Params &params);
    ~MscReader();

    void set_params(const Params &params);
    const Params &params() const;

    muse::Ret open();
    void close();
    bool is_opened() const;
    bool is_container() const;

    muse::ByteArray read_style_file() const;
    muse::ByteArray read_score_file() const;

    std::vector<muse::String> excerpt_file_names() const;
    muse::ByteArray read_excerpt_style_file(const muse::String &excerpt_file_name) const;
    muse::ByteArray read_excerpt_file(const muse::String &excerpt_file_name) const;

    muse::ByteArray read_chord_list_file() const;
    muse::ByteArray read_thumbnail_file() const;

    std::vector<muse::String> image_file_names() const;
    muse::ByteArray read_image_file(const muse::String &file_name) const;

    muse::ByteArray read_audio_settings_json_file(const muse::io::PathT &path_prefix = "") const;
    muse::ByteArray read_view_settings_json_file(const muse::io::PathT &path_prefix = "") const;
    muse::ByteArray read_automation_json_file() const;

private:

    struct IReader {
        virtual ~IReader() = default;

        virtual muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) = 0;
        virtual void close() = 0;
        virtual bool is_opened() const = 0;
        //! NOTE In the case of reading from a directory,
        //! it may happen that we are not reading a container (a directory with a certain structure),
        //! but only one file among others (`.mscx` from MU 3.x)
        virtual bool is_container() const = 0;
        virtual muse::StringList file_list() const = 0;
        virtual bool file_exists(const muse::String &file_name) const = 0;
        virtual muse::ByteArray file_data(const muse::String &file_name) const = 0;
    };

    struct ZipFileReader : public IReader {
        ~ZipFileReader() override;
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) override;
        void close() override;
        bool is_opened() const override;
        bool is_container() const override;
        muse::StringList file_list() const override;
        bool file_exists(const muse::String &file_name) const override;
        muse::ByteArray file_data(const muse::String &file_name) const override;
    private:
        muse::io::IODevice *device_ = nullptr;
        bool self_device_owner_ = false;
        muse::ZipReader *zip_ = nullptr;
    };

    struct DirReader : public IReader {
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) override;
        void close() override;
        bool is_opened() const override;
        bool is_container() const override;
        muse::StringList file_list() const override;
        bool file_exists(const muse::String &file_name) const override;
        muse::ByteArray file_data(const muse::String &file_name) const override;
    private:
        muse::io::PathT root_path_;
    };

    struct XmlFileReader : public IReader {
        muse::Ret open(muse::io::IODevice *device, const muse::io::PathT &file_path) override;
        void close() override;
        bool is_opened() const override;
        bool is_container() const override;
        muse::StringList file_list() const override;
        bool file_exists(const muse::String &file_name) const override;
        muse::ByteArray file_data(const muse::String &file_name) const override;
    private:
        muse::io::IODevice *device_ = nullptr;
        bool self_device_owner_ = false;
    };

    IReader *reader() const;
    bool file_exists(const muse::String &file_name) const;
    muse::ByteArray file_data(const muse::String &file_name) const;

    muse::String main_file_name() const;

    Params params_;
    mutable IReader *reader_ = nullptr;
};
} // namespace mu::engraving
