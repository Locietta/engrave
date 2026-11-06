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

#ifndef MU_ENGRAVING_INOUTDATA_H
#define MU_ENGRAVING_INOUTDATA_H

#include <map>
#include <optional>
#include <vector>

#include "linksindexer.h"
#include "write/writecontext.h"

namespace mu::engraving {
struct LinkedObjects;
} // namespace mu::engraving

namespace mu::engraving::rw {
struct ReadLinks {
    std::map<int /*staffIndex*/, std::vector<std::pair<LinkedObjects *, Location>>> staff_linked_elements; // one list per staff
    LinksIndexer links_indexer;
};

struct ReadInOutData {
    // for master - out
    // for except - in
    ReadLinks links;
    std::optional<double> overridden_spatium = std::nullopt;
    PropertyIdSet properties_to_skip;
    bool force_page_mode = false;

    // out
    SettingsCompat settings_compat;
    std::optional<double> original_spatium = std::nullopt;
};

struct WriteInOutData {
    write::WriteContext ctx;

    WriteInOutData(const Score *s) : ctx(s) {}
};
} // namespace mu::engraving::rw

#endif // MU_ENGRAVING_INOUTDATA_H
