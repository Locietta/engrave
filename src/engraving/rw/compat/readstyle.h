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

#ifndef MU_ENGRAVING_READSTYLE_H
#define MU_ENGRAVING_READSTYLE_H

#include "types/bytearray.h"
#include "types/string.h"

namespace mu::engraving {
struct Score;
struct XmlReader;
struct MStyle;
} // namespace mu::engraving

namespace mu::engraving::compat {
struct ReadStyleHook {
    ReadStyleHook(Score *score, const muse::ByteArray &score_data, const muse::String &complete_base_name);

    void setup_default_style();

    void read_style_tag(XmlReader &e);

    static int style_default_by_msc_version(const int msc_ver);
    static void setup_default_style(Score *score);
    static void read_style_tag(Score *score, XmlReader &e);
    static bool read_style_properties(MStyle *style, XmlReader &e);

private:
    Score *score_ = nullptr;
    const muse::ByteArray &score_data_;
    const muse::String &complete_base_name_;
};
} // namespace mu::engraving::compat

#endif // MU_ENGRAVING_READSTYLE_H
