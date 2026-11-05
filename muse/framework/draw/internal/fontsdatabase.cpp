/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#include "fontsdatabase.h"

#include "global/io/file.h"

#include "log.h"

using namespace muse;
using namespace muse::draw;

static int s_fontID = -1;

void FontsDatabase::setDefaultFont(Font::Type type, const FontDataKey &key) {
    defaults_[type] = key;
    ++revision_;
}

void FontsDatabase::insertSubstitution(const String &familyName, const String &substituteName) {
    FontDataKey familyKey(familyName);
    FontDataKey substituteKey(substituteName);
    familySubstitutions_[familyKey].push_back(substituteKey);

    ++revision_;
}

void FontsDatabase::removeSubstitutions(const String &familyName, const std::vector<String> &substituteNames) {
    auto it = familySubstitutions_.find(FontDataKey(familyName));
    if (it == familySubstitutions_.end()) {
        return;
    }

    std::vector<FontDataKey> &substitutes = it->second;
    size_t removed = 0;
    for (const String &substituteName : substituteNames) {
        removed += std::erase(substitutes, FontDataKey(substituteName));
    }

    if (removed == 0) {
        return;
    }

    if (substitutes.empty()) {
        familySubstitutions_.erase(it);
    }

    ++revision_;
}

const FontDataKey &FontsDatabase::defaultFont(Font::Type type) const {
    auto it = defaults_.find(type);
    if (it != defaults_.end()) {
        return it->second;
    }

    it = defaults_.find(Font::Type::Unknown);
    IF_ASSERT_FAILED(it != defaults_.end()) {
        static FontDataKey null;
        return null;
    }
    return it->second;
}

int FontsDatabase::addFont(const FontDataKey &key, const io::path_t &path) { return addFont(key, std::make_shared<io::File>(path)); }

int FontsDatabase::addFont(const FontDataKey &key, std::shared_ptr<io::IODevice> device) {
    if (!device) {
        LOGE() << "no device for font: " << key.family().id();
        return -1;
    }

    const std::string filePath = device->meta("file_path");
    ByteArray data;
    if (filePath.empty()) {
        if (device->open(io::IODevice::ReadOnly)) {
            data = device->readAll();
        }
        if (data.empty()) {
            LOGE() << "failed read font: " << key.family().id();
            return -1;
        }
    } else if (!io::File::exists(filePath)) {
        LOGW() << "font file not found: " << filePath;
        return -1;
    }

    const int id = ++s_fontID;

    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        release(it->second);
        fonts_.erase(it);
    }

    fonts_.insert({key, FontInfo{id, key, std::move(device)}});
    ++revision_;

    return id;
}

void FontsDatabase::removeFont(const FontDataKey &key) {
    auto it = fonts_.find(key);
    if (it == fonts_.end()) {
        return;
    }

    release(it->second);
    fonts_.erase(it);
    ++revision_;
}

void FontsDatabase::release(const FontInfo &fi) { UNUSED(fi); }

FontDataKey FontsDatabase::actualFont(const FontDataKey &requireKey, Font::Type type) const {
    if (fontInfo(requireKey).valid()) {
        return requireKey;
    }

    FontDataKey def = defaultFont(type);
    LOGW() << "not found required font: " << requireKey.family().id() << ", will be using default: " << def.family().id();
    return def;
}

std::vector<FontDataKey> FontsDatabase::substitutionFonts(const FontDataKey &requireKey) const {
    auto familyIt = familySubstitutions_.find(FontDataKey(requireKey.family()));
    if (familyIt != familySubstitutions_.end()) {
        return familyIt->second;
    }

    static std::vector<FontDataKey> null;
    return null;
}

FontData FontsDatabase::fontData(const FontDataKey &requireKey, Font::Type type) const {
    FontDataKey key = actualFont(requireKey, type);
    const FontInfo &info = fontInfo(key);
    if (!info.valid()) {
        return FontData();
    }

    if (!info.device->isOpen() && !info.device->open(io::IODevice::ReadOnly)) {
        LOGE() << "failed open font: " << key.family().id();
        return FontData();
    }

    info.device->seek(0);
    ByteArray data = info.device->readAll();
    if (data.empty()) {
        LOGE() << "failed read font: " << key.family().id();
        return FontData();
    }

    return FontData{key, data};
}

uint64_t FontsDatabase::revision() const { return revision_; }

const FontsDatabase::FontInfo &FontsDatabase::fontInfo(const FontDataKey &key) const {
    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        return it->second;
    }

    static FontInfo null;
    return null;
}
