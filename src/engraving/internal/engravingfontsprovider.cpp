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
    symbolFonts_.clear();
    externalSymbolFonts_.clear();
    fallback_.font = nullptr;
}

void EngravingFontsProvider::addInternalFont(const std::string &name, const std::string &family, const muse::io::path_t &filePath) {
    muse::io::path_t basePath = muse::io::dirpath(filePath);
    muse::io::path_t metadataPath = basePath + "/metadata.json";
    std::shared_ptr<EngravingFont> f = std::make_shared<EngravingFont>(fontProvider_, *this, name, family, filePath, metadataPath);
    symbolFonts_.push_back(f);
    fallback_.font = nullptr;
}

void EngravingFontsProvider::addExternalFont(const std::string &name, const std::string &family, const muse::io::path_t &filePath,
                                             const muse::io::path_t &metadataPath) {
    addExternalFont(name, family, std::make_shared<muse::io::File>(filePath), std::make_shared<muse::io::File>(metadataPath));
}

void EngravingFontsProvider::addExternalFont(const std::string &name, const std::string &family, std::shared_ptr<muse::io::IODevice> font,
                                             std::shared_ptr<muse::io::IODevice> metadata) {
    UNUSED(family);
    std::shared_ptr<EngravingFont> f =
        std::make_shared<EngravingFont>(fontProvider_, *this, name, family, std::move(font), std::move(metadata));
    externalSymbolFonts_.emplace(muse::strings::toLower(name), f);
}

std::shared_ptr<EngravingFont> EngravingFontsProvider::doFontByName(const std::string &name) const {
    // External fonts should have higher priority than internal fonts
    std::string name_lo = muse::strings::toLower(name);
    auto it = externalSymbolFonts_.find(name_lo);
    if (it != externalSymbolFonts_.end()) {
        return it->second;
    }
    for (const std::shared_ptr<EngravingFont> &f : symbolFonts_) {
        if (muse::strings::toLower(f->name()) == name_lo) {
            return f;
        }
    }
    return nullptr;
}

IEngravingFontPtr EngravingFontsProvider::fontByName(const std::string &name) const {
    std::shared_ptr<EngravingFont> font = doFontByName(name);
    if (!font) {
        font = doFallbackFont();
    }

    font->ensureLoad();
    return font;
}

std::vector<IEngravingFontPtr> EngravingFontsProvider::fonts() const {
    std::vector<IEngravingFontPtr> fs;
    for (const std::shared_ptr<EngravingFont> &f : symbolFonts_) {
        std::string name_lo = muse::strings::toLower(f->name());
        if (externalSymbolFonts_.find(name_lo) == externalSymbolFonts_.end()) {
            fs.push_back(f);
        }
    }
    for (const auto &[_, f] : externalSymbolFonts_) {
        fs.push_back(f);
    }
    return fs;
}

void EngravingFontsProvider::setFallbackFont(const std::string &name) {
    fallback_.name = name;
    fallback_.font = nullptr;
}

std::shared_ptr<EngravingFont> EngravingFontsProvider::doFallbackFont() const {
    if (!fallback_.font) {
        fallback_.font = doFontByName(fallback_.name);
        IF_ASSERT_FAILED(fallback_.font) { return nullptr; }
    }

    return fallback_.font;
}

IEngravingFontPtr EngravingFontsProvider::fallbackFont() const {
    std::shared_ptr<EngravingFont> font = doFallbackFont();
    font->ensureLoad();
    return font;
}

bool EngravingFontsProvider::isFallbackFont(const IEngravingFont *f) const { return doFallbackFont().get() == f; }

void EngravingFontsProvider::clearExternalFonts() { externalSymbolFonts_.clear(); }

void EngravingFontsProvider::loadAllFonts() {
    for (std::shared_ptr<EngravingFont> &f : symbolFonts_) {
        f->ensureLoad();
    }
    for (const auto &[_, f] : externalSymbolFonts_) {
        f->ensureLoad();
    }
}
