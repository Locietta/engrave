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

#include <memory>
#include <vector>
#include <map>

#include "ifontsdatabase.h"

namespace muse::draw {
class FontsDatabase : public IFontsDatabase {
public:
    FontsDatabase() = default;

    void set_default_font(Font::Type type, const FontDataKey &key) override;
    void insert_substitution(const String &f1, const String &substitute_name) override;
    void remove_substitutions(const String &f1, const std::vector<String> &substitute_names) override;

    int add_font(const FontDataKey &key, const io::PathT &path) override;
    int add_font(const FontDataKey &key, std::shared_ptr<io::IODevice> device) override;
    void remove_font(const FontDataKey &key) override;

    FontDataKey actual_font(const FontDataKey &require_key, Font::Type type) const override;
    std::vector<FontDataKey> substitution_fonts(const FontDataKey &require_key) const override;
    FontData font_data(const FontDataKey &require_key, Font::Type type) const override;

    uint64_t revision() const override;

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
