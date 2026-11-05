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
#include "fontprovider.h"

#include <memory>

#include "log.h"

using namespace muse;
using namespace muse::draw;

int FontProvider::addSymbolFont(const muse::String &family, const io::path_t &path) {
    return database_.addFont(FontDataKey(Font::FontFamily(family)), path);
}

int FontProvider::addSymbolFont(const muse::String &family, std::shared_ptr<io::IODevice> device) {
    return database_.addFont(FontDataKey(Font::FontFamily(family)), std::move(device));
}

double FontProvider::lineSpacing(const muse::draw::Font &f) const { return engine_.lineSpacing(f); }

double FontProvider::xHeight(const muse::draw::Font &f) const { return engine_.xHeight(f); }

double FontProvider::height(const muse::draw::Font &f) const { return engine_.height(f); }

double FontProvider::ascent(const muse::draw::Font &f) const { return engine_.ascent(f); }

double FontProvider::capHeight(const muse::draw::Font &f) const { return engine_.capHeight(f); }

double FontProvider::descent(const muse::draw::Font &f) const { return engine_.descent(f); }

double FontProvider::underlinePos(const muse::draw::Font &f) const { return engine_.underlinePos(f); }

double FontProvider::lineWidth(const muse::draw::Font &f) const { return engine_.lineWidth(f); }

double FontProvider::strikeOutPos(const muse::draw::Font &f) const { return engine_.strikeOutPos(f); }

bool FontProvider::inFont(const muse::draw::Font &f, char32_t ucs4) const { return engine_.inFont(f, ucs4); }

// Text
double FontProvider::horizontalAdvance(const muse::draw::Font &f, const muse::String &string) const {
    return engine_.horizontalAdvance(f, string.toStdU32String());
}

double FontProvider::horizontalAdvance(const muse::draw::Font &f, char32_t ucs4) const { return engine_.horizontalAdvance(f, ucs4); }

RectF FontProvider::boundingRect(const muse::draw::Font &f, const muse::String &string) const {
    return engine_.boundingRect(f, string.toStdU32String());
}

RectF FontProvider::boundingRect(const muse::draw::Font &f, char32_t ucs4) const { return engine_.boundingRect(f, ucs4); }

RectF FontProvider::tightBoundingRect(const muse::draw::Font &f, const muse::String &string) const {
    return engine_.tightBoundingRect(f, string.toStdU32String());
}
