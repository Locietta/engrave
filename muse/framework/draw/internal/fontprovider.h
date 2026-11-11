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

#include "draw/types/font.h"
#include "draw/types/geometry.h"
#include "engrave/src/runtime/fonts_database.h"
#include "engrave/src/runtime/fonts_engine_proxy.h"
#include "global/io/iodevice.h"
#include "global/io/path.h"
#include "global/types/string.h"

namespace muse::draw {
struct FontProvider {

private:
    music::RuntimeFontsDatabase &database_;
    music::RuntimeFontsEngine &engine_;

public:
    FontProvider(music::RuntimeFontsDatabase &database, music::RuntimeFontsEngine &engine) : database_(database), engine_(engine) {}

    int add_symbol_font(const String &family, const io::PathT &path);
    int add_symbol_font(const String &family, std::shared_ptr<io::IODevice> device);

    double line_spacing(const Font &f) const;
    double x_height(const Font &f) const;
    double height(const Font &f) const;
    double cap_height(const Font &f) const;
    double ascent(const Font &f) const;
    double descent(const Font &f) const;

    double underline_pos(const Font &f) const;
    double line_width(const Font &f) const;
    double strike_out_pos(const Font &f) const;

    bool in_font(const Font &f, char32_t ucs4) const;

    // Text
    double horizontal_advance(const Font &f, const String &string) const;
    double horizontal_advance(const Font &f, char32_t ucs4) const;

    RectF bounding_rect(const Font &f, const String &string) const;
    RectF bounding_rect(const Font &f, char32_t ucs4) const;

    RectF tight_bounding_rect(const Font &f, const String &string) const;
};
} // namespace muse::draw
