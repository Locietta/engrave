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

#include "../ifontprovider.h"

#include "ifontsdatabase.h"
#include "ifontsengine.h"

namespace muse::draw {
struct FontProvider : IFontProvider {
private:
    IFontsDatabase &database_;
    IFontsEngine &engine_;

public:
    FontProvider(IFontsDatabase &database, IFontsEngine &engine) : database_(database), engine_(engine) {}

    int add_symbol_font(const String &family, const io::PathT &path) override;
    int add_symbol_font(const String &family, std::shared_ptr<io::IODevice> device) override;

    double line_spacing(const Font &f) const override;
    double x_height(const Font &f) const override;
    double height(const Font &f) const override;
    double cap_height(const Font &f) const override;
    double ascent(const Font &f) const override;
    double descent(const Font &f) const override;

    double underline_pos(const Font &f) const override;
    double line_width(const Font &f) const override;
    double strike_out_pos(const Font &f) const override;

    bool in_font(const Font &f, char32_t ucs4) const override;

    // Text
    double horizontal_advance(const Font &f, const String &string) const override;
    double horizontal_advance(const Font &f, char32_t ucs4) const override;

    RectF bounding_rect(const Font &f, const String &string) const override;
    RectF bounding_rect(const Font &f, char32_t ucs4) const override;

    RectF tight_bounding_rect(const Font &f, const String &string) const override;
};
} // namespace muse::draw
