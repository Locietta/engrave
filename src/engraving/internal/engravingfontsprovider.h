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

#include <memory>
#include <string>
#include <vector>

#include "engravingfont.h"
#include "global/io/iodevice.h"
#include "global/io/path.h"

namespace mu::engraving {
struct EngravingFont;

struct EngravingFontsProvider {

    explicit EngravingFontsProvider(muse::draw::FontProvider &font_provider) : font_provider_(font_provider) {}

    void deinit();

    void add_internal_font(const std::string &name, const std::string &family, const muse::io::PathT &file_path);
    void add_external_font(const std::string &name, const std::string &family, const muse::io::PathT &file_path,
                           const muse::io::PathT &metadata_path);
    void add_external_font(const std::string &name, const std::string &family, std::shared_ptr<muse::io::IODevice> font,
                           std::shared_ptr<muse::io::IODevice> metadata);
    EngravingFontPtr font_by_name(const std::string &name) const;
    std::vector<EngravingFontPtr> fonts() const;

    void set_fallback_font(const std::string &name);
    EngravingFontPtr fallback_font() const;
    bool is_fallback_font(const EngravingFont *f) const;

    void clear_external_fonts();

    void load_all_fonts();

private:
    std::shared_ptr<EngravingFont> do_font_by_name(const std::string &name) const;
    std::shared_ptr<EngravingFont> do_fallback_font() const;

    struct Fallback {
        std::string name;
        std::shared_ptr<EngravingFont> font;
    };

    muse::draw::FontProvider &font_provider_;
    mutable Fallback fallback_;
    std::vector<std::shared_ptr<EngravingFont>> symbol_fonts_;
    std::unordered_map<std::string, std::shared_ptr<EngravingFont>> external_symbol_fonts_;
};
} // namespace mu::engraving
