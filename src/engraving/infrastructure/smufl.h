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

#include <array>
#include <map>

#include "../types/symid.h"
#include "types/string.h"

namespace mu::engraving {
struct Smufl {

    static bool init();

    struct Code {
        char32_t smufl_code = 0;
        char32_t music_sym_block_code = 0;

        bool is_valid() const { return smufl_code != 0 || music_sym_block_code != 0; }
    };

    static const Code code(SymId id);
    static char32_t smufl_code(SymId id);

    static const std::map<muse::String, std::vector<SymId>> &smufl_ranges();
    static constexpr const char *smufl_all_symbols = "All symbols";

private:

    static bool load_glyph_names_json();
    static std::map<muse::String, std::vector<SymId>> read_smufl_ranges_json();

    static std::array<Code, size_t(SymId::LAST_SYM) + 1> s_sym_id_codes;
};
} // namespace mu::engraving
