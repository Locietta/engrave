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

#include <utility>

#include "global/io/file.h"
#include "log.h"

using namespace muse;
using namespace muse::draw;

void FontsDatabase::set_default_font(Font::Type type, const FontDataKey &key) {
    defaults_[type] = key;
    ++revision_;
}

void FontsDatabase::insert_substitution(const String &family_name, const String &substitute_name) {
    FontDataKey family_key(family_name);
    FontDataKey substitute_key(substitute_name);
    family_substitutions_[family_key].push_back(substitute_key);

    ++revision_;
}

void FontsDatabase::remove_substitutions(const String &family_name, const std::vector<String> &substitute_names) {
    auto it = family_substitutions_.find(FontDataKey(family_name));
    if (it == family_substitutions_.end()) {
        return;
    }

    std::vector<FontDataKey> &substitutes = it->second;
    size_t removed = 0;
    for (const String &substitute_name : substitute_names) {
        removed += std::erase(substitutes, FontDataKey(substitute_name));
    }

    if (removed == 0) {
        return;
    }

    if (substitutes.empty()) {
        family_substitutions_.erase(it);
    }

    ++revision_;
}

const FontDataKey &FontsDatabase::default_font(Font::Type type) const {
    auto it = defaults_.find(type);
    if (it != defaults_.end()) {
        return it->second;
    }

    it = defaults_.find(Font::Type::UNKNOWN);
    IF_ASSERT_FAILED(it != defaults_.end()) {
        static FontDataKey null;
        return null;
    }
    return it->second;
}

int FontsDatabase::add_font(const FontDataKey &key, const io::PathT &path) { return add_font(key, std::make_shared<io::File>(path)); }

int FontsDatabase::add_font(const FontDataKey &key, std::shared_ptr<io::IODevice> device) {
    if (!device) {
        LOGE() << "no device for font: " << key.family().id();
        return -1;
    }

    const std::string file_path = device->meta("file_path");
    ByteArray data;
    if (file_path.empty()) {
        if (device->open(io::IODevice::READ_ONLY)) {
            data = device->read_all();
        }
        if (data.empty()) {
            LOGE() << "failed read font: " << key.family().id();
            return -1;
        }
    } else if (!io::File::exists(file_path)) {
        LOGW() << "font file not found: " << file_path;
        return -1;
    }

    const int id = ++font_id_;

    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        release(it->second);
        fonts_.erase(it);
    }

    fonts_.insert({key, FontInfo{id, key, std::move(device)}});
    ++revision_;

    return id;
}

void FontsDatabase::remove_font(const FontDataKey &key) {
    auto it = fonts_.find(key);
    if (it == fonts_.end()) {
        return;
    }

    release(it->second);
    fonts_.erase(it);
    ++revision_;
}

void FontsDatabase::release([[maybe_unused]] const FontInfo &fi) {}

FontDataKey FontsDatabase::actual_font(const FontDataKey &require_key, Font::Type type) const {
    if (font_info(require_key).valid()) {
        return require_key;
    }

    FontDataKey def = default_font(type);
    LOGW() << "not found required font: " << require_key.family().id() << ", will be using default: " << def.family().id();
    return def;
}

std::vector<FontDataKey> FontsDatabase::substitution_fonts(const FontDataKey &require_key) const {
    auto family_it = family_substitutions_.find(FontDataKey(require_key.family()));
    if (family_it != family_substitutions_.end()) {
        return family_it->second;
    }

    static std::vector<FontDataKey> null;
    return null;
}

FontData FontsDatabase::font_data(const FontDataKey &require_key, Font::Type type) const {
    FontDataKey key = actual_font(require_key, type);
    const FontInfo &info = font_info(key);
    if (!info.valid()) {
        return FontData();
    }

    if (!info.device->is_open() && !info.device->open(io::IODevice::READ_ONLY)) {
        LOGE() << "failed open font: " << key.family().id();
        return FontData();
    }

    info.device->seek(0);
    ByteArray data = info.device->read_all();
    if (data.empty()) {
        LOGE() << "failed read font: " << key.family().id();
        return FontData();
    }

    return FontData{key, std::move(data)};
}

uint64_t FontsDatabase::revision() const { return revision_; }

const FontsDatabase::FontInfo &FontsDatabase::font_info(const FontDataKey &key) const {
    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        return it->second;
    }

    static FontInfo null;
    return null;
}
