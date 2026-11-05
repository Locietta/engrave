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

#include "io/file.h"
#include "io/fileinfo.h"
#include "io/dir.h"
#include "serialization/zipreader.h"
#include "serialization/xmlstreamreader.h"
#include "engraving/engravingerrors.h"

#include "log.h"

//! NOTE The current implementation resolves files by extension.
//! This will probably be changed in the future.

using namespace muse;
using namespace muse::io;
using namespace mu;
using namespace mu::engraving;

MscReader::MscReader(const Params &params) : params_(params) {}

MscReader::~MscReader() { close(); }

void MscReader::setParams(const Params &params) {
    IF_ASSERT_FAILED(!isOpened()) { return; }

    if (reader_) {
        delete reader_;
        reader_ = nullptr;
    }

    params_ = params;
}

const MscReader::Params &MscReader::params() const { return params_; }

Ret MscReader::open() { return reader()->open(params_.device, params_.filePath); }

void MscReader::close() {
    if (reader_) {
        reader_->close();

        delete reader_;
        reader_ = nullptr;
    }
}

bool MscReader::isOpened() const { return reader_ ? reader_->isOpened() : false; }

bool MscReader::isContainer() const { return reader_ ? reader_->isContainer() : false; }

MscReader::IReader *MscReader::reader() const {
    if (!reader_) {
        switch (params_.mode) {
            case MscIoMode::Zip: reader_ = new ZipFileReader(); break;
            case MscIoMode::Dir: reader_ = new DirReader(); break;
            case MscIoMode::XmlFile: reader_ = new XmlFileReader(); break;
            case MscIoMode::Unknown: UNREACHABLE; break;
        }
    }

    return reader_;
}

bool MscReader::fileExists(const String &fileName) const { return reader()->fileExists(fileName); }

ByteArray MscReader::fileData(const String &fileName) const { return reader()->fileData(fileName); }

ByteArray MscReader::readStyleFile() const {
    if (!fileExists(u"score_style.mss")) {
        return ByteArray();
    }
    return fileData(u"score_style.mss");
}

String MscReader::mainFileName() const {
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

ByteArray MscReader::readScoreFile() const {
    String mscxFileName = mainFileName();
    ByteArray data = fileData(mscxFileName);
    if (data.empty() && reader()->isContainer()) {
        StringList files = reader()->fileList();
        for (const String &name : files) {
            // mscx file in the root dir
            if (!name.contains(u'/') && name.endsWith(u".mscx", muse::CaseInsensitive)) {
                mscxFileName = name;
                break;
            }
        }
    }

    return fileData(mscxFileName);
}

std::vector<String> MscReader::excerptFileNames() const {
    if (!reader()->isContainer()) {
        NOT_SUPPORTED << " not container";
        return std::vector<String>();
    }

    std::vector<String> names;
    StringList files = reader()->fileList();
    for (const String &filePath : files) {
        if (filePath.startsWith(u"Excerpts/") && filePath.endsWith(u".mscx", muse::CaseInsensitive)) {
            names.push_back(FileInfo(filePath).completeBaseName());
        }
    }
    return names;
}

ByteArray MscReader::readExcerptStyleFile(const String &excerptFileName) const {
    String fileName = excerptFileName + u".mss";
    return fileData(u"Excerpts/" + excerptFileName + u"/" + fileName);
}

ByteArray MscReader::readExcerptFile(const String &excerptFileName) const {
    String fileName = excerptFileName + u".mscx";
    return fileData(u"Excerpts/" + excerptFileName + u"/" + fileName);
}

ByteArray MscReader::readChordListFile() const {
    if (!fileExists(u"chordlist.xml")) {
        return ByteArray();
    }
    return fileData(u"chordlist.xml");
}

ByteArray MscReader::readThumbnailFile() const { return fileData(u"Thumbnails/thumbnail.png"); }

ByteArray MscReader::readImageFile(const String &fileName) const { return fileData(u"Pictures/" + fileName); }

std::vector<String> MscReader::imageFileNames() const {
    if (!reader()->isContainer()) {
        // NOT_SUPPORTED << " not container";
        return std::vector<String>();
    }

    std::vector<String> names;
    StringList files = reader()->fileList();
    for (const String &filePath : files) {
        if (filePath.startsWith(u"Pictures/")) {
            names.push_back(FileInfo(filePath).fileName());
        }
    }
    return names;
}

ByteArray MscReader::readAudioSettingsJsonFile(const muse::io::path_t &pathPrefix) const {
    return fileData(pathPrefix.toString() + u"audiosettings.json");
}

ByteArray MscReader::readViewSettingsJsonFile(const muse::io::path_t &pathPrefix) const {
    return fileData(pathPrefix.toString() + u"viewsettings.json");
}

muse::ByteArray MscReader::readAutomationJsonFile() const {
    if (!fileExists(u"automation.json")) {
        return ByteArray();
    }

    return fileData(u"automation.json");
}

// =======================================================================
// Readers
// =======================================================================

MscReader::ZipFileReader::~ZipFileReader() {
    delete zip_;
    if (selfDeviceOwner_) {
        delete device_;
    }
}

Ret MscReader::ZipFileReader::open(IODevice *device, const path_t &filePath) {
    device_ = device;
    if (!device_) {
        if (!FileInfo::exists(filePath)) {
            LOGE() << "path does not exist: " << filePath;
            return make_ret(Err::FileNotFound, filePath);
        }

        device_ = new File(filePath);
        selfDeviceOwner_ = true;
    }

    if (!device_->isOpen()) {
        if (!device_->open(IODevice::ReadOnly)) {
            LOGE() << "failed open file: " << filePath;
            return make_ret(Err::FileOpenError, filePath);
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

bool MscReader::ZipFileReader::isOpened() const { return device_ ? device_->isOpen() : false; }

bool MscReader::ZipFileReader::isContainer() const { return true; }

StringList MscReader::ZipFileReader::fileList() const {
    IF_ASSERT_FAILED(zip_) { return StringList(); }

    StringList files;
    std::vector<ZipReader::FileInfo> fileInfoList = zip_->fileInfoList();
    if (zip_->hasError()) {
        LOGE() << "failed read meta";
    }

    for (const ZipReader::FileInfo &fi : fileInfoList) {
        if (fi.isFile) {
            files << fi.filePath.toString();
        }
    }

    return files;
}

bool MscReader::ZipFileReader::fileExists(const String &fileName) const {
    IF_ASSERT_FAILED(zip_) { return false; }

    return zip_->fileExists(fileName.toStdString());
}

ByteArray MscReader::ZipFileReader::fileData(const String &fileName) const {
    IF_ASSERT_FAILED(zip_) { return ByteArray(); }

    ByteArray data = zip_->fileData(fileName.toStdString());
    if (zip_->hasError()) {
        LOGE() << "failed read data for filename " << fileName;
        return ByteArray();
    }
    return data;
}

Ret MscReader::DirReader::open(IODevice *device, const path_t &filePath) {
    if (device) {
        NOT_SUPPORTED;
        return false;
    }

    if (!FileInfo::exists(filePath)) {
        LOGE() << "path does not exist: " << filePath;
        return make_ret(Err::FileNotFound, filePath);
    }

    rootPath_ = containerPath(filePath);

    return muse::make_ok();
}

void MscReader::DirReader::close() {
    // noop
}

bool MscReader::DirReader::isOpened() const { return FileInfo::exists(rootPath_); }

bool MscReader::DirReader::isContainer() const {
    //! NOTE We will assume that if there is `/META-INF/container.xml` in the root directory,
    //! then we read from the container (a directory with a certain structure)
    return FileInfo::exists(rootPath_ + "/META-INF/container.xml");
}

StringList MscReader::DirReader::fileList() const {
    RetVal<io::paths_t> rv = Dir::scanFiles(rootPath_, {}, ScanMode::FilesInCurrentDirAndSubdirs);
    if (!rv.ret) {
        LOGE() << "failed scan dir: " << rootPath_ << ", err: " << rv.ret.toString();
        return StringList();
    }

    StringList files;
    for (const muse::io::path_t &p : rv.val) {
        String filePath = p.toString();
        files << filePath.mid(rootPath_.size() + 1);
    }

    return files;
}

bool MscReader::DirReader::fileExists(const String &fileName) const {
    muse::io::path_t filePath = rootPath_ + "/" + fileName;
    return File::exists(filePath);
}

ByteArray MscReader::DirReader::fileData(const String &fileName) const {
    muse::io::path_t filePath = rootPath_ + "/" + fileName;
    File file(filePath);
    if (!file.open(IODevice::ReadOnly)) {
        LOGE() << "failed open file: " << filePath;
        return ByteArray();
    }

    return file.readAll();
}

Ret MscReader::XmlFileReader::open(IODevice *device, const path_t &filePath) {
    device_ = device;
    if (!device_) {
        if (!FileInfo::exists(filePath)) {
            LOGE() << "path does not exist: " << filePath;
            return make_ret(Err::FileNotFound, filePath);
        }

        device_ = new File(filePath);
        selfDeviceOwner_ = true;
    }

    if (!device_->isOpen()) {
        if (!device_->open(IODevice::ReadOnly)) {
            LOGE() << "failed open file: " << filePath;
            return make_ret(Err::FileOpenError, filePath);
        }
    }

    return muse::make_ok();
}

void MscReader::XmlFileReader::close() {
    if (device_) {
        device_->close();
    }
}

bool MscReader::XmlFileReader::isOpened() const { return device_ ? device_->isOpen() : false; }

bool MscReader::XmlFileReader::isContainer() const { return true; }

StringList MscReader::XmlFileReader::fileList() const {
    if (!device_) {
        return StringList();
    }

    StringList files;

    device_->seek(0);
    XmlStreamReader xml(device_);
    while (xml.readNextStartElement()) {
        if (xml.name() != "files") {
            xml.skipCurrentElement();
            continue;
        }

        while (xml.readNextStartElement()) {
            if (xml.name() != "file") {
                xml.skipCurrentElement();
                continue;
            }

            String fileName = xml.attribute("name");
            files << fileName;
            xml.skipCurrentElement();
        }
    }

    return files;
}

bool MscReader::XmlFileReader::fileExists(const String &fileName) const {
    if (!device_) {
        return false;
    }

    device_->seek(0);
    XmlStreamReader xml(device_);
    while (xml.readNextStartElement()) {
        if ("files" != xml.name()) {
            xml.skipCurrentElement();
            continue;
        }

        while (xml.readNextStartElement()) {
            if ("file" != xml.name()) {
                xml.skipCurrentElement();
                continue;
            }

            if (fileName == xml.attribute("name")) {
                return true;
            }
        }
    }

    return false;
}

ByteArray MscReader::XmlFileReader::fileData(const String &fileName) const {
    if (!device_) {
        return ByteArray();
    }

    device_->seek(0);
    XmlStreamReader xml(device_);
    while (xml.readNextStartElement()) {
        if (xml.name() != "files") {
            xml.skipCurrentElement();
            continue;
        }

        while (xml.readNextStartElement()) {
            if (xml.name() != "file") {
                xml.skipCurrentElement();
                continue;
            }

            String file = xml.attribute("name");
            if (file != fileName) {
                xml.skipCurrentElement();
                continue;
            }

            String cdata = xml.readText();
            ByteArray ba = cdata.trimmed().toUtf8();
            return ba;
        }
    }

    return ByteArray();
}
