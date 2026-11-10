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

#ifndef MU_ENGRAVING_SCORERW_H
#define MU_ENGRAVING_SCORERW_H

#include <functional>

#include "engraving/dom/masterscore.h"
#include "engraving/engravingerrors.h"
#include "types/bytearray.h"
#include "types/string.h"

namespace mu::engraving {
struct ScoreRW {
    ScoreRW() = default;

    static void set_root_path(const String &path);
    static String root_path();

    // The engine services the test scores are built with; the test environment
    // sets them for the lifetime of its engine context.
    static void set_services(const EngravingServices *services);
    static const EngravingServices &services();

    using ImportFunc = std::function<Err(MasterScore *score, const muse::io::PathT &path)>;
    using ExportFunc = std::function<Err(Score *score, const muse::io::PathT &path)>;

    static MasterScore *read_score(const String &path, bool is_absolute_path = false, ImportFunc import_func = nullptr);
    // Save names are file names, not paths: a relative name resolves into a private
    // scratch directory (created on first use, removed by clear_scratch()) so tests
    // never write into the working directory. An absolute name is used as given.
    static bool save_score(Score *score, const String &name);
    static bool save_score(Score *score, const String &name, ExportFunc export_func);
    static muse::ByteArray write_element(const EngravingItem *element);
    static EngravingItem *write_read_element(EngravingItem *element);
    static bool save_mime_data(muse::ByteArray mime_data, const String &save_name);

    // The path a save name resolves to; callers that read a saved file back use this.
    static String scratch_path(const String &name);
    // Removes the scratch directory and everything under it; the next save recreates it.
    static void clear_scratch();

private:
    static String root_path_;
    static const EngravingServices *services_;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_SCORERW_H
