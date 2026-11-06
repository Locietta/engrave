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
#include "mscwriter.h"

#include <vector>
#include <exception>

#include "containers.h"
#include "io/buffer.h"
#include "io/writebudget.h"
#include "io/file.h"
#include "io/fileinfo.h"
#include "io/dir.h"
#include "serialization/xmlstreamwriter.h"
#include "serialization/zipwriter.h"
#include "serialization/textstream.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace mu::engraving;

MscWriter::MscWriter(const Params &params) : params_(params) {}

MscWriter::~MscWriter() { close(); }

void MscWriter::set_params(const Params &params) {
    IF_ASSERT_FAILED(!is_opened()) { return; }

    if (writer_) {
        had_error_ = writer_->has_error();
        delete writer_;
        writer_ = nullptr;
    }

    params_ = params;
}

const MscWriter::Params &MscWriter::params() const { return params_; }

Ret MscWriter::open() { return writer()->open(params_.device, params_.file_path); }

void MscWriter::close() {
    if (writer_) {
        if (writer_->is_opened()) {
            // A failed bounded write may unwind through this destructor. Do
            // not build archive metadata while an exception or budget failure
            // is already in flight: its text writer can interrupt as well.
            if (std::uncaught_exceptions() == 0 && (!active_write_budget || active_write_budget->failure == WriteBudget::Failure::NONE)) {
                write_meta();
            }
            writer_->close();
        }

        had_error_ = writer_->has_error();
        delete writer_;
        writer_ = nullptr;
    }
}

bool MscWriter::is_opened() const { return writer_ ? writer_->is_opened() : false; }

bool MscWriter::has_error() const { return writer_ ? writer_->has_error() : had_error_; }

MscWriter::IWriter *MscWriter::writer() const {
    if (!writer_) {
        switch (params_.mode) {
            case MscIoMode::ZIP: writer_ = new ZipFileWriter(); break;
            case MscIoMode::DIR: writer_ = new DirWriter(); break;
            case MscIoMode::XML_FILE: writer_ = new XmlFileWriter(); break;
            case MscIoMode::UNKNOWN: UNREACHABLE; break;
        }
    }

    return writer_;
}

bool MscWriter::add_file_data(const String &file_name, const ByteArray &data) {
    if (!writer()->add_file_data(file_name, data)) {
        LOGE() << "failed write file: " << file_name;
        return false;
    }

    meta_.add_file(file_name);

    return true;
}

void MscWriter::write_style_file(const ByteArray &data) { add_file_data(u"score_style.mss", data); }

String MscWriter::main_file_name() const {
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

void MscWriter::write_score_file(const ByteArray &data) { add_file_data(main_file_name(), data); }

void MscWriter::add_excerpt_style_file(const String &excerpt_file_name, const ByteArray &data) {
    String file_name = excerpt_file_name + u".mss";
    add_file_data(u"Excerpts/" + excerpt_file_name + u"/" + file_name, data);
}

void MscWriter::add_excerpt_file(const String &excerpt_file_name, const ByteArray &data) {
    String file_name = excerpt_file_name + u".mscx";
    add_file_data(u"Excerpts/" + excerpt_file_name + u"/" + file_name, data);
}

void MscWriter::write_chord_list_file(const ByteArray &data) { add_file_data(u"chordlist.xml", data); }

void MscWriter::write_thumbnail_file(const ByteArray &data) { add_file_data(u"Thumbnails/thumbnail.png", data); }

void MscWriter::add_image_file(const String &file_name, const ByteArray &data) { add_file_data(u"Pictures/" + file_name, data); }

void MscWriter::write_audio_settings_json_file(const ByteArray &data, const muse::io::PathT &path_prefix) {
    add_file_data(path_prefix.to_string() + u"audiosettings.json", data);
}

void MscWriter::write_view_settings_json_file(const ByteArray &data, const muse::io::PathT &path_prefix) {
    add_file_data(path_prefix.to_string() + u"viewsettings.json", data);
}

void MscWriter::write_automation_json_file(const muse::ByteArray &data) { add_file_data(u"automation.json", data); }

void MscWriter::write_meta() {
    if (meta_.is_written) {
        return;
    }

    write_container(meta_.files);

    meta_.is_written = true;
}

void MscWriter::write_container(const std::vector<String> &paths) {
    ByteArray data;
    auto buf = Buffer::opened(IODevice::WRITE_ONLY, &data);
    XmlStreamWriter xml(&buf);
    xml.start_document();
    xml.start_element("container");
    xml.start_element("rootfiles");

    for (const String &f : paths) {
        xml.element("rootfile", {{"full-path", f}});
    }

    xml.end_element();
    xml.end_element();
    xml.flush();

    add_file_data(u"META-INF/container.xml", data);
}

bool MscWriter::Meta::contains(const String &file) const {
    if (std::find(files.begin(), files.end(), file) != files.end()) {
        return true;
    }
    return false;
}

void MscWriter::Meta::add_file(const String &file) {
    if (!contains(file)) {
        files.push_back(file);
    }
}

// =======================================================================
// Writers
// =======================================================================

MscWriter::ZipFileWriter::~ZipFileWriter() { delete zip_; }

Ret MscWriter::ZipFileWriter::open(io::IODevice *device, const PathT &) {
    IF_ASSERT_FAILED(device) { return make_ret(Ret::Code::INTERNAL_ERROR); }
    device_ = device;

    if (!device_->is_open()) {
        if (!device_->open(IODevice::WRITE_ONLY)) {
            LOGE() << "failed to open device for writing";
            return make_ret(device_->error(), device_->error_string());
        }
    }

    zip_ = new ZipWriter(device_);

    return true;
}

void MscWriter::ZipFileWriter::close() {
    if (zip_) {
        zip_->close();
    }

    if (device_) {
        device_->close();
    }
}

bool MscWriter::ZipFileWriter::is_opened() const { return device_ ? device_->is_open() : false; }

bool MscWriter::ZipFileWriter::has_error() const { return (device_ ? device_->has_error() : false) || (zip_ ? zip_->has_error() : false); }

bool MscWriter::ZipFileWriter::add_file_data(const String &file_name, const ByteArray &data) {
    IF_ASSERT_FAILED(zip_) { return false; }

    zip_->add_file(file_name.to_std_string(), data);
    if (zip_->has_error()) {
        LOGE() << "failed write files to zip";
        return false;
    }

    return true;
}

Ret MscWriter::DirWriter::open(io::IODevice *device, const muse::io::PathT &file_path) {
    IF_ASSERT_FAILED(!device) {
        has_error_ = true;
        return make_ret(Ret::Code::INTERNAL_ERROR);
    }

    if (file_path.empty()) {
        LOGE() << "file path is empty";
        has_error_ = true;
        return false;
    }

    root_path_ = container_path(file_path);

    Dir dir(root_path_);
    Ret ret = dir.remove_recursively();
    if (!ret) {
        LOGE() << "failed clear dir: " << dir.absolute_path();
        has_error_ = true;
        return ret;
    }

    ret = dir.mkpath(dir.absolute_path());
    if (!ret) {
        LOGE() << "failed make path: " << dir.absolute_path();
        has_error_ = true;
        return ret;
    }

    return true;
}

void MscWriter::DirWriter::close() {
    // noop
}

bool MscWriter::DirWriter::is_opened() const { return FileInfo::exists(root_path_); }

bool MscWriter::DirWriter::has_error() const { return has_error_; }

bool MscWriter::DirWriter::add_file_data(const String &file_name, const ByteArray &data) {
    muse::io::PathT file_path = root_path_ + "/" + file_name;

    Dir file_dir(FileInfo(file_path).absolute_path());
    if (!file_dir.exists()) {
        if (!file_dir.mkpath(file_dir.absolute_path())) {
            LOGE() << "failed make path: " << file_dir.absolute_path();
            has_error_ = true;
            return false;
        }
    }

    const Ret ret = File::write_file(file_path, data);
    if (!ret) {
        LOGE() << "failed to write file: " << file_path;
        has_error_ = true;
        return false;
    }

    return true;
}

MscWriter::XmlFileWriter::~XmlFileWriter() { delete stream_; }

Ret MscWriter::XmlFileWriter::open(io::IODevice *device, const PathT &) {
    IF_ASSERT_FAILED(device) { return make_ret(Ret::Code::INTERNAL_ERROR); }
    device_ = device;

    if (!device_->is_open()) {
        if (!device_->open(IODevice::WRITE_ONLY)) {
            LOGE() << "failed to open device for writing";
            return make_ret(device_->error(), device_->error_string());
        }
    }

    stream_ = new TextStream(device_);

    // Write header
    *stream_ << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    *stream_ << "<files>\n";

    return true;
}

void MscWriter::XmlFileWriter::close() {
    if (stream_) {
        *stream_ << "</files>\n";
        stream_->flush();
        device_->close();
    }
}

bool MscWriter::XmlFileWriter::is_opened() const { return device_ ? device_->is_open() : false; }

bool MscWriter::XmlFileWriter::has_error() const { return device_ ? device_->has_error() : false; }

bool MscWriter::XmlFileWriter::add_file_data(const String &file_name, const ByteArray &data) {
    if (!stream_) {
        return false;
    }

    static const std::vector<String> supported_exts = {u"mscx", u"json", u"mss"};
    String ext = FileInfo::suffix(file_name);
    if (!muse::contains(supported_exts, ext)) {
        MUSE_NOT_SUPPORTED << file_name;
        return true; // not error
    }

    TextStream &ts = *stream_;
    ts << "<file name=\"" << file_name << "\">\n";
    ts << "<![CDATA[";
    ts << data;
    ts << "]]>\n";
    ts << "</file>\n";

    return true;
}
