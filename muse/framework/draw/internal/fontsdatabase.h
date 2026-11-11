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
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "draw/types/fontstypes.h"
#include "global/io/iodevice.h"
#include "global/io/path.h"

namespace muse::draw {
struct FontsDatabase {
    FontsDatabase() = default;

    void set_default_font(Font::Type type, const FontDataKey &key);
    void insert_substitution(const String &f1, const String &substitute_name);
    void remove_substitutions(const String &f1, const std::vector<String> &substitute_names);

    int add_font(const FontDataKey &key, const io::PathT &path);
    int add_font(const FontDataKey &key, std::shared_ptr<io::IODevice> device);
    void remove_font(const FontDataKey &key);

    FontDataKey actual_font(const FontDataKey &require_key, Font::Type type) const;
    std::vector<FontDataKey> substitution_fonts(const FontDataKey &require_key) const;
    FontData font_data(const FontDataKey &require_key, Font::Type type) const;

    uint64_t revision() const;

private:

    struct FontInfo {
        int id = -1;
        FontDataKey key;
        std::shared_ptr<io::IODevice> device;

        bool valid() const { return id > -1; }
    };

    const FontDataKey &default_font(Font::Type type) const;
    const FontInfo &font_info(const FontDataKey &key) const;
    void release(const FontInfo &fi);

    std::map<Font::Type, FontDataKey> defaults_;
    std::map<FontDataKey, std::vector<FontDataKey>> family_substitutions_;
    std::map<FontDataKey, FontInfo> fonts_;
    uint64_t revision_ = 0;
};
} // namespace muse::draw
