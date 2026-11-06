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

int FontProvider::add_symbol_font(const muse::String &family, const io::PathT &path) {
    return database_.add_font(FontDataKey(Font::FontFamily(family)), path);
}

int FontProvider::add_symbol_font(const muse::String &family, std::shared_ptr<io::IODevice> device) {
    return database_.add_font(FontDataKey(Font::FontFamily(family)), std::move(device));
}

double FontProvider::line_spacing(const muse::draw::Font &f) const { return engine_.line_spacing(f); }

double FontProvider::x_height(const muse::draw::Font &f) const { return engine_.x_height(f); }

double FontProvider::height(const muse::draw::Font &f) const { return engine_.height(f); }

double FontProvider::ascent(const muse::draw::Font &f) const { return engine_.ascent(f); }

double FontProvider::cap_height(const muse::draw::Font &f) const { return engine_.cap_height(f); }

double FontProvider::descent(const muse::draw::Font &f) const { return engine_.descent(f); }

double FontProvider::underline_pos(const muse::draw::Font &f) const { return engine_.underline_pos(f); }

double FontProvider::line_width(const muse::draw::Font &f) const { return engine_.line_width(f); }

double FontProvider::strike_out_pos(const muse::draw::Font &f) const { return engine_.strike_out_pos(f); }

bool FontProvider::in_font(const muse::draw::Font &f, char32_t ucs4) const { return engine_.in_font(f, ucs4); }

// Text
double FontProvider::horizontal_advance(const muse::draw::Font &f, const muse::String &string) const {
    return engine_.horizontal_advance(f, string.to_std_u32_string());
}

double FontProvider::horizontal_advance(const muse::draw::Font &f, char32_t ucs4) const { return engine_.horizontal_advance(f, ucs4); }

RectF FontProvider::bounding_rect(const muse::draw::Font &f, const muse::String &string) const {
    return engine_.bounding_rect(f, string.to_std_u32_string());
}

RectF FontProvider::bounding_rect(const muse::draw::Font &f, char32_t ucs4) const { return engine_.bounding_rect(f, ucs4); }

RectF FontProvider::tight_bounding_rect(const muse::draw::Font &f, const muse::String &string) const {
    return engine_.tight_bounding_rect(f, string.to_std_u32_string());
}
