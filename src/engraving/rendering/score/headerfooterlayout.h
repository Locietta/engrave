/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "layoutcontext.h"

namespace mu::engraving {
class Page;
class System;
class Text;
class TextBlock;
class TextFragment;
class CharFormat;

enum class TextStyleType : unsigned char;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class HeaderFooterLayout {
public:
    static void layout_header_footer(LayoutContext &ctx, Page *page);

    /// How much the header extends into the page (i.e., not in the margins)
    static double header_extension(const LayoutContext &ctx, const Page *page);

    /// How much the footer extends into the page (i.e., not in the margins)
    static double footer_extension(const LayoutContext &ctx, const Page *page);

    static bool score_has_timestamp_headers_footers(const Score *score);

private:
    static void create_update_header_text(LayoutContext &ctx, Page *page, int area, const String &s);
    static void create_update_footer_text(LayoutContext &ctx, Page *page, int area, const String &s);

    /// Returns false if text is empty and should be removed
    static bool update_header_footer_text(LayoutContext &ctx, Page *page, Text *text, const String &s);

    static void remove_header_text(Page *page, int area);
    static void remove_footer_text(Page *page, int area);

    static TextBlock replace_text_macros(LayoutContext &ctx, const Page *page, const TextBlock &tb);
    static CharFormat format_for_macro(const LayoutContext &ctx, const String &macro);
    static void append_formatted_string(std::list<TextFragment> &fragments, const String &s, const CharFormat &default_format,
                                        const CharFormat &macro_format);

    static bool contains_timestamp_macros(const String &text);

    static bool must_show_headers_for_page(const LayoutContext &ctx, const Page *page);
    static bool must_show_footers_for_page(const LayoutContext &ctx, const Page *page);
};
} // namespace mu::engraving::rendering::score
