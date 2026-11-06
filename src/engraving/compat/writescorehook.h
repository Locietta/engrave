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
#ifndef MU_ENGRAVING_WRITESCOREHOOK_H
#define MU_ENGRAVING_WRITESCOREHOOK_H

namespace mu::engraving {
struct Score;
struct XmlWriter;
} // namespace mu::engraving

namespace mu::engraving::write {
struct WriteContext;
} // namespace mu::engraving::write

namespace mu::engraving::compat {
struct WriteScoreHook {
    WriteScoreHook() = default;

    void on_write_style302(Score *score, XmlWriter &xml, bool inline_style = false);
    void on_write_excerpts302(Score *score, XmlWriter &xml, write::WriteContext &ctx);
};
} // namespace mu::engraving::compat
#endif // MU_ENGRAVING_WRITESCOREHOOK_H
