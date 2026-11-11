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
#include "mscreader.h"

#include "engraving/engravingerrors.h"
#include "io/dir.h"
#include "io/file.h"
#include "io/fileinfo.h"
#include "log.h"
#include "serialization/xmlstreamreader.h"
#include "serialization/zipreader.h"

//! NOTE The current implementation resolves files by extension.
//! This will probably be changed in the future.

using namespace muse;
using namespace muse::io;
using namespace mu;
using namespace mu::engraving;

MscReader::MscReader(const Params &params) : params_(params) {}

MscReader::~MscReader() { close(); }

void MscReader::set_params(const Params &params) {
    IF_ASSERT_FAILED(!is_opened()) { return; }

    if (reader_) {
        delete reader_;
        reader_ = nullptr;
    }

    params_ = params;
}

const MscReader::Params &MscReader::params() const { return params_; }

Ret MscReader::open() { return reader()->open(params_.device, params_.file_path); }

void MscReader::close() {
    if (reader_) {
        reader_->close();

        delete reader_;
        reader_ = nullptr;
    }
}

bool MscReader::is_opened() const { return reader_ ? reader_->is_opened() : false; }

bool MscReader::is_container() const { return reader_ ? reader_->is_container() : false; }

MscReader::Reader *MscReader::reader() const {
    if (!reader_) {
        switch (params_.mode) {
            case MscIoMode::ZIP: reader_ = new Reader(std::in_place_type<ZipFileReader>); break;
            case MscIoMode::DIR: reader_ = new Reader(std::in_place_type<DirReader>); break;
            case MscIoMode::XML_FILE: reader_ = new Reader(std::in_place_type<XmlFileReader>); break;
            case MscIoMode::UNKNOWN: UNREACHABLE; break;
        }
    }

    return reader_;
}

bool MscReader::file_exists(const String &file_name) const { return reader()->file_exists(file_name); }

ByteArray MscReader::file_data(const String &file_name) const { return reader()->file_data(file_name); }

ByteArray MscReader::read_style_file() const {
    if (!file_exists(u"score_style.mss")) {
        return ByteArray();
    }
    return file_data(u"score_style.mss");
}

String MscReader::main_file_name() const {
    if (!params_.main_file_name.is_empty()) {
        return params_.main_file_name;
    }

    String name = u"score.mscx";
    if (params_.file_path.empty()) {
        return name;
    }

    String complete_base_name = FileInfo(params_.file_path).complete_base_name();
    if (complete_base_name.is_empty()) {
        return name;
    }

    return complete_base_name + u".mscx";
}

ByteArray MscReader::read_score_file() const {
    String mscx_file_name = main_file_name();
    ByteArray data = file_data(mscx_file_name);
    if (data.empty() && reader()->is_container()) {
        StringList files = reader()->file_list();
        for (const String &name : files) {
            // mscx file in the root dir
            if (!name.contains(u'/') && name.ends_with(u".mscx", muse::CASE_INSENSITIVE)) {
                mscx_file_name = name;
                break;
            }
        }
    }

    return file_data(mscx_file_name);
}

std::vector<String> MscReader::excerpt_file_names() const {
    if (!reader()->is_container()) {
        MUSE_NOT_SUPPORTED << " not container";
        return std::vector<String>();
    }

    std::vector<String> names;
    StringList files = reader()->file_list();
    for (const String &file_path : files) {
        if (file_path.starts_with(u"Excerpts/") && file_path.ends_with(u".mscx", muse::CASE_INSENSITIVE)) {
            names.push_back(FileInfo(file_path).complete_base_name());
        }
    }
    return names;
}

ByteArray MscReader::read_excerpt_style_file(const String &excerpt_file_name) const {
    String file_name = excerpt_file_name + u".mss";
    return file_data(u"Excerpts/" + excerpt_file_name + u"/" + file_name);
}

ByteArray MscReader::read_excerpt_file(const String &excerpt_file_name) const {
    String file_name = excerpt_file_name + u".mscx";
    return file_data(u"Excerpts/" + excerpt_file_name + u"/" + file_name);
}

ByteArray MscReader::read_chord_list_file() const {
    if (!file_exists(u"chordlist.xml")) {
        return ByteArray();
    }
    return file_data(u"chordlist.xml");
}

ByteArray MscReader::read_thumbnail_file() const { return file_data(u"Thumbnails/thumbnail.png"); }

ByteArray MscReader::read_image_file(const String &file_name) const { return file_data(u"Pictures/" + file_name); }

std::vector<String> MscReader::image_file_names() const {
    if (!reader()->is_container()) {
        // NOT_SUPPORTED << " not container";
        return std::vector<String>();
    }

    std::vector<String> names;
    StringList files = reader()->file_list();
    for (const String &file_path : files) {
        if (file_path.starts_with(u"Pictures/")) {
            names.push_back(FileInfo(file_path).file_name());
        }
    }
    return names;
}

ByteArray MscReader::read_audio_settings_json_file(const muse::io::PathT &path_prefix) const {
    return file_data(path_prefix.to_string() + u"audiosettings.json");
}

ByteArray MscReader::read_view_settings_json_file(const muse::io::PathT &path_prefix) const {
    return file_data(path_prefix.to_string() + u"viewsettings.json");
}

muse::ByteArray MscReader::read_automation_json_file() const {
    if (!file_exists(u"automation.json")) {
        return ByteArray();
    }

    return file_data(u"automation.json");
}

// =======================================================================
// Readers
// =======================================================================

MscReader::ZipFileReader::~ZipFileReader() {
    delete zip_;
    if (self_device_owner_) {
        delete device_;
    }
}

Ret MscReader::ZipFileReader::open(IODevice *device, const PathT &file_path) {
    device_ = device;
    if (!device_) {
        if (!FileInfo::exists(file_path)) {
            LOGE() << "path does not exist: " << file_path;
            return make_ret(Err::FILE_NOT_FOUND, file_path);
        }

        device_ = new File(file_path);
        self_device_owner_ = true;
    }

    if (!device_->is_open()) {
        if (!device_->open(IODevice::READ_ONLY)) {
            LOGE() << "failed open file: " << file_path;
            return make_ret(Err::FILE_OPEN_ERROR, file_path);
        }
    }

    zip_ = new ZipReader(device_);

    return true;
}

void MscReader::ZipFileReader::close() {
    if (zip_) {
        zip_->close();
    }

    if (device_) {
        device_->close();
    }
}

bool MscReader::ZipFileReader::is_opened() const { return device_ ? device_->is_open() : false; }

bool MscReader::ZipFileReader::is_container() const { return true; }

StringList MscReader::ZipFileReader::file_list() const {
    IF_ASSERT_FAILED(zip_) { return StringList(); }

    StringList files;
    std::vector<ZipReader::FileInfo> file_info_list = zip_->file_info_list();
    if (zip_->has_error()) {
        LOGE() << "failed read meta";
    }

    for (const ZipReader::FileInfo &fi : file_info_list) {
        if (fi.is_file) {
            files << fi.file_path.to_string();
        }
    }

    return files;
}

bool MscReader::ZipFileReader::file_exists(const String &file_name) const {
    IF_ASSERT_FAILED(zip_) { return false; }

    return zip_->file_exists(file_name.to_std_string());
}

ByteArray MscReader::ZipFileReader::file_data(const String &file_name) const {
    IF_ASSERT_FAILED(zip_) { return ByteArray(); }

    ByteArray data = zip_->file_data(file_name.to_std_string());
    if (zip_->has_error()) {
        LOGE() << "failed read data for filename " << file_name;
        return ByteArray();
    }
    return data;
}

Ret MscReader::DirReader::open(IODevice *device, const PathT &file_path) {
    if (device) {
        MUSE_NOT_SUPPORTED;
        return false;
    }

    if (!FileInfo::exists(file_path)) {
        LOGE() << "path does not exist: " << file_path;
        return make_ret(Err::FILE_NOT_FOUND, file_path);
    }

    root_path_ = container_path(file_path);

    return muse::make_ok();
}

void MscReader::DirReader::close() {
    // noop
}

bool MscReader::DirReader::is_opened() const { return FileInfo::exists(root_path_); }

bool MscReader::DirReader::is_container() const {
    //! NOTE We will assume that if there is `/META-INF/container.xml` in the root directory,
    //! then we read from the container (a directory with a certain structure)
    return FileInfo::exists(root_path_ + "/META-INF/container.xml");
}

StringList MscReader::DirReader::file_list() const {
    RetVal<io::paths_t> rv = Dir::scan_files(root_path_, {}, ScanMode::FILES_IN_CURRENT_DIR_AND_SUBDIRS);
    if (!rv.ret) {
        LOGE() << "failed scan dir: " << root_path_ << ", err: " << rv.ret.to_string();
        return StringList();
    }

    StringList files;
    for (const muse::io::PathT &p : rv.val) {
        String file_path = p.to_string();
        files << file_path.mid(root_path_.size() + 1);
    }

    return files;
}

bool MscReader::DirReader::file_exists(const String &file_name) const {
    muse::io::PathT file_path = root_path_ + "/" + file_name;
    return File::exists(file_path);
}

ByteArray MscReader::DirReader::file_data(const String &file_name) const {
    muse::io::PathT file_path = root_path_ + "/" + file_name;
    File file(file_path);
    if (!file.open(IODevice::READ_ONLY)) {
        LOGE() << "failed open file: " << file_path;
        return ByteArray();
    }

    return file.read_all();
}

Ret MscReader::XmlFileReader::open(IODevice *device, const PathT &file_path) {
    device_ = device;
    if (!device_) {
        if (!FileInfo::exists(file_path)) {
            LOGE() << "path does not exist: " << file_path;
            return make_ret(Err::FILE_NOT_FOUND, file_path);
        }

        device_ = new File(file_path);
        self_device_owner_ = true;
    }

    if (!device_->is_open()) {
        if (!device_->open(IODevice::READ_ONLY)) {
            LOGE() << "failed open file: " << file_path;
            return make_ret(Err::FILE_OPEN_ERROR, file_path);
        }
    }

    return muse::make_ok();
}

void MscReader::XmlFileReader::close() {
    if (device_) {
        device_->close();
    }
}

bool MscReader::XmlFileReader::is_opened() const { return device_ ? device_->is_open() : false; }

bool MscReader::XmlFileReader::is_container() const { return true; }

StringList MscReader::XmlFileReader::file_list() const {
    if (!device_) {
        return StringList();
    }

    StringList files;

    device_->seek(0);
    XmlStreamReader xml(device_);
    while (xml.read_next_start_element()) {
        if (xml.name() != "files") {
            xml.skip_current_element();
            continue;
        }

        while (xml.read_next_start_element()) {
            if (xml.name() != "file") {
                xml.skip_current_element();
                continue;
            }

            String file_name = xml.attribute("name");
            files << file_name;
            xml.skip_current_element();
        }
    }

    return files;
}

bool MscReader::XmlFileReader::file_exists(const String &file_name) const {
    if (!device_) {
        return false;
    }

    device_->seek(0);
    XmlStreamReader xml(device_);
    while (xml.read_next_start_element()) {
        if ("files" != xml.name()) {
            xml.skip_current_element();
            continue;
        }

        while (xml.read_next_start_element()) {
            if ("file" != xml.name()) {
                xml.skip_current_element();
                continue;
            }

            if (file_name == xml.attribute("name")) {
                return true;
            }
        }
    }

    return false;
}

ByteArray MscReader::XmlFileReader::file_data(const String &file_name) const {
    if (!device_) {
        return ByteArray();
    }

    device_->seek(0);
    XmlStreamReader xml(device_);
    while (xml.read_next_start_element()) {
        if (xml.name() != "files") {
            xml.skip_current_element();
            continue;
        }

        while (xml.read_next_start_element()) {
            if (xml.name() != "file") {
                xml.skip_current_element();
                continue;
            }

            String file = xml.attribute("name");
            if (file != file_name) {
                xml.skip_current_element();
                continue;
            }

            String cdata = xml.read_text();
            ByteArray ba = cdata.trimmed().to_utf8();
            return ba;
        }
    }

    return ByteArray();
}
