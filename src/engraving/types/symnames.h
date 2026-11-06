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

#include "symid.h"
#include "types/string.h"
#include "types/translatablestring.h"

namespace mu::engraving {
struct SymNames {
    static muse::AsciiStringView name_for_sym_id(SymId id);
    static const muse::TranslatableString &user_name_for_sym_id(SymId id);
    static muse::String translated_user_name_for_sym_id(SymId id);

    static SymId sym_id_by_name(const muse::AsciiStringView &name, SymId def = SymId::NO_SYM);
    static SymId sym_id_by_name(const muse::String &name, SymId def = SymId::NO_SYM);
    static SymId sym_id_by_old_name(const muse::AsciiStringView &old_name);
    static SymId sym_id_by_user_name(const muse::String &user_name);

private:
    static void load_name_to_sym_id_hash();

    static const std::array<muse::AsciiStringView, size_t(SymId::LAST_SYM) + 1> s_sym_names;
    static const std::array<muse::TranslatableString, size_t(SymId::LAST_SYM) + 1> s_sym_user_names;

    //! Will be initialized when first used
    static std::map<muse::AsciiStringView, SymId> s_name_to_sym_id_hash;
    static const std::map<muse::AsciiStringView, SymId> s_old_name_to_sym_id_hash;
};
} // namespace mu::engraving
