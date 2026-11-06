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
#ifndef MU_ENGRAVING_DEFAULTSTYLE_H
#define MU_ENGRAVING_DEFAULTSTYLE_H

#include "global/io/path.h"
#include "style.h"

namespace mu::engraving {
struct DefaultStyle {
    static DefaultStyle *instance();

    void init(const muse::io::PathT &default_style_file_path, const muse::io::PathT &part_style_file_path, const SizeF &default_page_size);

    static const MStyle &base_style();

    static const MStyle &default_style();
    static const MStyle *default_style_for_parts();

    static const MStyle &resolve_style_defaults(const int defaults_version);

private:
    DefaultStyle() = default;

    static bool do_load_style(MStyle *style, const muse::io::PathT &file_path);

    MStyle base_style_;    // builtin initial style
    MStyle default_style_; // builtin modified by preferences
    MStyle *default_style_for_parts_ = nullptr;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_DEFAULTSTYLE_H
