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

#pragma once

#include "engrave/src/runtime/resource_filesystem.h"
#include "engraving/dom/score.h"

namespace mu::engraving {
struct ScoreComp {
    static bool save_compare_score(Score *, const String &save_name, const String &compare_with_local_path);
    static bool save_compare_mime_data(muse::ByteArray mime_data, const String &save_name, const String &compare_with_local_path);
    static bool compare_files(const String &full_path1, const String &full_path2);
};
} // namespace mu::engraving
