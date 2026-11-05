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

void MscWriter::setParams(const Params &params) {
    IF_ASSERT_FAILED(!isOpened()) { return; }

    if (writer_) {
        hadError_ = writer_->hasError();
        delete writer_;
        writer_ = nullptr;
    }

    params_ = params;
}

const MscWriter::Params &MscWriter::params() const { return params_; }

Ret MscWriter::open() { return writer()->open(params_.device, params_.filePath); }

void MscWriter::close() {
    if (writer_) {
        if (writer_->isOpened()) {
            // A failed bounded write may unwind through this destructor. Do
            // not build archive metadata while an exception or budget failure
            // is already in flight: its text writer can interrupt as well.
            if (std::uncaught_exceptions() == 0 && (!activeWriteBudget || activeWriteBudget->failure == WriteBudget::Failure::None)) {
                writeMeta();
            }
            writer_->close();
        }

        hadError_ = writer_->hasError();
        delete writer_;
        writer_ = nullptr;
    }
}

bool MscWriter::isOpened() const { return writer_ ? writer_->isOpened() : false; }

bool MscWriter::hasError() const { return writer_ ? writer_->hasError() : hadError_; }

MscWriter::IWriter *MscWriter::writer() const {
    if (!writer_) {
        switch (params_.mode) {
            case MscIoMode::Zip: writer_ = new ZipFileWriter(); break;
            case MscIoMode::Dir: writer_ = new DirWriter(); break;
            case MscIoMode::XmlFile: writer_ = new XmlFileWriter(); break;
            case MscIoMode::Unknown: UNREACHABLE; break;
        }
    }

    return writer_;
}

bool MscWriter::addFileData(const String &fileName, const ByteArray &data) {
    if (!writer()->addFileData(fileName, data)) {
        LOGE() << "failed write file: " << fileName;
        return false;
    }

    meta_.addFile(fileName);

    return true;
}

void MscWriter::writeStyleFile(const ByteArray &data) { addFileData(u"score_style.mss", data); }

String MscWriter::mainFileName() const {
    if (!params_.mainFileName.isEmpty()) {
        return params_.mainFileName;
    }

    String name = u"score.mscx";
    if (params_.filePath.empty()) {
        return name;
    }

    String completeBaseName = FileInfo(params_.filePath).completeBaseName();
    if (completeBaseName.isEmpty()) {
        return name;
    }

    return completeBaseName + u".mscx";
}

void MscWriter::writeScoreFile(const ByteArray &data) { addFileData(mainFileName(), data); }

void MscWriter::addExcerptStyleFile(const String &excerptFileName, const ByteArray &data) {
    String fileName = excerptFileName + u".mss";
    addFileData(u"Excerpts/" + excerptFileName + u"/" + fileName, data);
}

void MscWriter::addExcerptFile(const String &excerptFileName, const ByteArray &data) {
    String fileName = excerptFileName + u".mscx";
    addFileData(u"Excerpts/" + excerptFileName + u"/" + fileName, data);
}

void MscWriter::writeChordListFile(const ByteArray &data) { addFileData(u"chordlist.xml", data); }

void MscWriter::writeThumbnailFile(const ByteArray &data) { addFileData(u"Thumbnails/thumbnail.png", data); }

void MscWriter::addImageFile(const String &fileName, const ByteArray &data) { addFileData(u"Pictures/" + fileName, data); }

void MscWriter::writeAudioSettingsJsonFile(const ByteArray &data, const muse::io::path_t &pathPrefix) {
    addFileData(pathPrefix.toString() + u"audiosettings.json", data);
}

void MscWriter::writeViewSettingsJsonFile(const ByteArray &data, const muse::io::path_t &pathPrefix) {
    addFileData(pathPrefix.toString() + u"viewsettings.json", data);
}

void MscWriter::writeAutomationJsonFile(const muse::ByteArray &data) { addFileData(u"automation.json", data); }

void MscWriter::writeMeta() {
    if (meta_.isWritten) {
        return;
    }

    writeContainer(meta_.files);

    meta_.isWritten = true;
}

void MscWriter::writeContainer(const std::vector<String> &paths) {
    ByteArray data;
    auto buf = Buffer::opened(IODevice::WriteOnly, &data);
    XmlStreamWriter xml(&buf);
    xml.startDocument();
    xml.startElement("container");
    xml.startElement("rootfiles");

    for (const String &f : paths) {
        xml.element("rootfile", {{"full-path", f}});
    }

    xml.endElement();
    xml.endElement();
    xml.flush();

    addFileData(u"META-INF/container.xml", data);
}

bool MscWriter::Meta::contains(const String &file) const {
    if (std::find(files.begin(), files.end(), file) != files.end()) {
        return true;
    }
    return false;
}

void MscWriter::Meta::addFile(const String &file) {
    if (!contains(file)) {
        files.push_back(file);
    }
}

// =======================================================================
// Writers
// =======================================================================

MscWriter::ZipFileWriter::~ZipFileWriter() { delete zip_; }

Ret MscWriter::ZipFileWriter::open(io::IODevice *device, const path_t &) {
    IF_ASSERT_FAILED(device) { return make_ret(Ret::Code::InternalError); }
    device_ = device;

    if (!device_->isOpen()) {
        if (!device_->open(IODevice::WriteOnly)) {
            LOGE() << "failed to open device for writing";
            return make_ret(device_->error(), device_->errorString());
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

bool MscWriter::ZipFileWriter::isOpened() const { return device_ ? device_->isOpen() : false; }

bool MscWriter::ZipFileWriter::hasError() const { return (device_ ? device_->hasError() : false) || (zip_ ? zip_->hasError() : false); }

bool MscWriter::ZipFileWriter::addFileData(const String &fileName, const ByteArray &data) {
    IF_ASSERT_FAILED(zip_) { return false; }

    zip_->addFile(fileName.toStdString(), data);
    if (zip_->hasError()) {
        LOGE() << "failed write files to zip";
        return false;
    }

    return true;
}

Ret MscWriter::DirWriter::open(io::IODevice *device, const muse::io::path_t &filePath) {
    IF_ASSERT_FAILED(!device) {
        hasError_ = true;
        return make_ret(Ret::Code::InternalError);
    }

    if (filePath.empty()) {
        LOGE() << "file path is empty";
        hasError_ = true;
        return false;
    }

    rootPath_ = containerPath(filePath);

    Dir dir(rootPath_);
    Ret ret = dir.removeRecursively();
    if (!ret) {
        LOGE() << "failed clear dir: " << dir.absolutePath();
        hasError_ = true;
        return ret;
    }

    ret = dir.mkpath(dir.absolutePath());
    if (!ret) {
        LOGE() << "failed make path: " << dir.absolutePath();
        hasError_ = true;
        return ret;
    }

    return true;
}

void MscWriter::DirWriter::close() {
    // noop
}

bool MscWriter::DirWriter::isOpened() const { return FileInfo::exists(rootPath_); }

bool MscWriter::DirWriter::hasError() const { return hasError_; }

bool MscWriter::DirWriter::addFileData(const String &fileName, const ByteArray &data) {
    muse::io::path_t filePath = rootPath_ + "/" + fileName;

    Dir fileDir(FileInfo(filePath).absolutePath());
    if (!fileDir.exists()) {
        if (!fileDir.mkpath(fileDir.absolutePath())) {
            LOGE() << "failed make path: " << fileDir.absolutePath();
            hasError_ = true;
            return false;
        }
    }

    const Ret ret = File::writeFile(filePath, data);
    if (!ret) {
        LOGE() << "failed to write file: " << filePath;
        hasError_ = true;
        return false;
    }

    return true;
}

MscWriter::XmlFileWriter::~XmlFileWriter() { delete stream_; }

Ret MscWriter::XmlFileWriter::open(io::IODevice *device, const path_t &) {
    IF_ASSERT_FAILED(device) { return make_ret(Ret::Code::InternalError); }
    device_ = device;

    if (!device_->isOpen()) {
        if (!device_->open(IODevice::WriteOnly)) {
            LOGE() << "failed to open device for writing";
            return make_ret(device_->error(), device_->errorString());
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

bool MscWriter::XmlFileWriter::isOpened() const { return device_ ? device_->isOpen() : false; }

bool MscWriter::XmlFileWriter::hasError() const { return device_ ? device_->hasError() : false; }

bool MscWriter::XmlFileWriter::addFileData(const String &fileName, const ByteArray &data) {
    if (!stream_) {
        return false;
    }

    static const std::vector<String> supportedExts = {u"mscx", u"json", u"mss"};
    String ext = FileInfo::suffix(fileName);
    if (!muse::contains(supportedExts, ext)) {
        NOT_SUPPORTED << fileName;
        return true; // not error
    }

    TextStream &ts = *stream_;
    ts << "<file name=\"" << fileName << "\">\n";
    ts << "<![CDATA[";
    ts << data;
    ts << "]]>\n";
    ts << "</file>\n";

    return true;
}
