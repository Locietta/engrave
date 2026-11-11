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

#include "engravingfontsprovider.h"

#include "global/stringutils.h"
#include "io/file.h"
#include "log.h"

using namespace mu;
using namespace mu::engraving;

void EngravingFontsProvider::deinit() {
    symbol_fonts_.clear();
    external_symbol_fonts_.clear();
    fallback_.font = nullptr;
}

void EngravingFontsProvider::add_internal_font(const std::string &name, const std::string &family, const muse::io::PathT &file_path) {
    muse::io::PathT base_path = muse::io::dirpath(file_path);
    muse::io::PathT metadata_path = base_path + "/metadata.json";
    std::shared_ptr<EngravingFont> f = std::make_shared<EngravingFont>(font_provider_, *this, name, family, file_path, metadata_path);
    symbol_fonts_.push_back(f);
    fallback_.font = nullptr;
}

void EngravingFontsProvider::add_external_font(const std::string &name, const std::string &family, const muse::io::PathT &file_path,
                                               const muse::io::PathT &metadata_path) {
    add_external_font(name, family, std::make_shared<muse::io::File>(file_path), std::make_shared<muse::io::File>(metadata_path));
}

void EngravingFontsProvider::add_external_font(const std::string &name, const std::string &family, std::shared_ptr<muse::io::IODevice> font,
                                               std::shared_ptr<muse::io::IODevice> metadata) {
    std::shared_ptr<EngravingFont> f =
        std::make_shared<EngravingFont>(font_provider_, *this, name, family, std::move(font), std::move(metadata));
    external_symbol_fonts_.emplace(muse::strings::to_lower(name), f);
}

std::shared_ptr<EngravingFont> EngravingFontsProvider::do_font_by_name(const std::string &name) const {
    // External fonts should have higher priority than internal fonts
    std::string name_lo = muse::strings::to_lower(name);
    auto it = external_symbol_fonts_.find(name_lo);
    if (it != external_symbol_fonts_.end()) {
        return it->second;
    }
    for (const std::shared_ptr<EngravingFont> &f : symbol_fonts_) {
        if (muse::strings::to_lower(f->name()) == name_lo) {
            return f;
        }
    }
    return nullptr;
}

EngravingFontPtr EngravingFontsProvider::font_by_name(const std::string &name) const {
    std::shared_ptr<EngravingFont> font = do_font_by_name(name);
    if (!font) {
        font = do_fallback_font();
    }

    font->ensure_load();
    return font;
}

std::vector<EngravingFontPtr> EngravingFontsProvider::fonts() const {
    std::vector<EngravingFontPtr> fs;
    for (const std::shared_ptr<EngravingFont> &f : symbol_fonts_) {
        std::string name_lo = muse::strings::to_lower(f->name());
        if (external_symbol_fonts_.find(name_lo) == external_symbol_fonts_.end()) {
            fs.push_back(f);
        }
    }
    for (const auto &[_, f] : external_symbol_fonts_) {
        fs.push_back(f);
    }
    return fs;
}

void EngravingFontsProvider::set_fallback_font(const std::string &name) {
    fallback_.name = name;
    fallback_.font = nullptr;
}

std::shared_ptr<EngravingFont> EngravingFontsProvider::do_fallback_font() const {
    if (!fallback_.font) {
        fallback_.font = do_font_by_name(fallback_.name);
        IF_ASSERT_FAILED(fallback_.font) { return nullptr; }
    }

    return fallback_.font;
}

EngravingFontPtr EngravingFontsProvider::fallback_font() const {
    std::shared_ptr<EngravingFont> font = do_fallback_font();
    font->ensure_load();
    return font;
}

bool EngravingFontsProvider::is_fallback_font(const EngravingFont *f) const { return do_fallback_font().get() == f; }

void EngravingFontsProvider::clear_external_fonts() { external_symbol_fonts_.clear(); }

void EngravingFontsProvider::load_all_fonts() {
    for (std::shared_ptr<EngravingFont> &f : symbol_fonts_) {
        f->ensure_load();
    }
    for (const auto &[_, f] : external_symbol_fonts_) {
        f->ensure_load();
    }
}
