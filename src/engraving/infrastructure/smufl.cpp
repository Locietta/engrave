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
#include "smufl.h"

#include <gtl/btree.hpp>

#include "dom/mscore.h"
#include "io/file.h"
#include "log.h"
#include "serialization/json_fields.h"
#include "types/symnames.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace mu::engraving;

namespace json = muse::json;

std::array<Smufl::Code, size_t(SymId::LAST_SYM) + 1> Smufl::s_sym_id_codes{{}};
gtl::btree_map<String, std::vector<SymId>> Smufl::s_ranges;

const Smufl::Code Smufl::code(SymId id) { return s_sym_id_codes.at(static_cast<size_t>(id)); }

char32_t Smufl::smufl_code(SymId id) { return s_sym_id_codes.at(static_cast<size_t>(id)).smufl_code; }

bool Smufl::init() {
    const bool ok = load_glyph_names_json();
    s_ranges = read_smufl_ranges_json();
    return ok;
}

bool Smufl::load_glyph_names_json() {
    s_sym_id_codes.fill({});
    File file(":/fonts/smufl/glyphnames.json");
    if (!file.open(IODevice::READ_ONLY)) {
        LOGE() << "could not open glyph names JSON file.";
        return false;
    }

    const auto bytes = file.read_all();
    file.close();
    const auto source = json::text(bytes);
    const auto glyph_names = glz::read_json<glz::generic>(source);
    if (!glyph_names) {
        LOGE() << "JSON parse error in glyph names file: " << glz::format_error(glyph_names.error(), source);
        return false;
    }

    IF_ASSERT_FAILED(!json::object(*glyph_names).empty()) {
        LOGE() << "Could not read glyph names JSON";
        return false;
    }

    for (size_t i = 0; i < s_sym_id_codes.size(); ++i) {
        SymId sym = static_cast<SymId>(i);
        if (sym == SymId::NO_SYM || sym == SymId::LAST_SYM) {
            continue;
        }

        std::string name(SymNames::name_for_sym_id(sym).ascii());
        const auto *glyph = json::find(*glyph_names, name);
        if (!glyph || !glyph->is_object()) {
            continue;
        }

        bool ok;
        char32_t code =
            String::from_std_string(json::get_or_empty<std::string>(json::member(*glyph, "codepoint"))).mid(2).to_u_int(&ok, 16);
        if (ok) {
            s_sym_id_codes[i].smufl_code = code;
        } else if (MScore::debug_mode) {
            LOGD() << "could not read codepoint for glyph " << name;
        }

        char32_t alternate_code =
            String::from_std_string(json::get_or_empty<std::string>(json::member(*glyph, "alternateCodepoint"))).mid(2).to_u_int(&ok, 16);
        if (ok) {
            s_sym_id_codes[i].music_sym_block_code = alternate_code;
        } else if (MScore::debug_mode) {
            LOGD() << "could not read alternate codepoint for glyph " << name;
        }
    }
    return true;
}

const gtl::btree_map<String, std::vector<SymId>> &Smufl::smufl_ranges() { return s_ranges; }

gtl::btree_map<String, std::vector<SymId>> Smufl::read_smufl_ranges_json() {
    gtl::btree_map<String, std::vector<SymId>> ranges;
    std::vector<SymId> all_symbols;

    File fi(":/fonts/smufl/ranges.json");
    if (!fi.open(IODevice::READ_ONLY)) {
        LOGE() << "failed open: " << fi.file_path();
        return ranges;
    }

    const auto bytes = fi.read_all();
    const auto source = json::text(bytes);
    const auto parsed = glz::read_json<glz::generic_sorted>(source);
    if (!parsed) {
        LOGE() << "Failed to parse " << fi.file_path() << ": " << glz::format_error(parsed.error(), source);
        return ranges;
    }

    for (const auto &[name, range] : json::object(*parsed)) {
        const String desc = String::from_std_string(json::get_or_empty<std::string>(json::member(range, "description")));
        const auto &glyphs = json::array(json::member(range, "glyphs"));

        if (glyphs.empty()) {
            continue;
        }
        std::vector<SymId> range_symbols;
        for (const auto &glyph : glyphs) {
            const auto &sym_name = json::get_or_empty<std::string>(glyph);
            SymId sym_id = SymNames::sym_id_by_name(String::from_std_string(sym_name));
            if (sym_id != SymId::NO_SYM) {
                range_symbols.push_back(sym_id);
                all_symbols.push_back(sym_id);
            }
        }
        ranges.insert({desc, range_symbols});
    }

    ranges.insert(
        {String::from_ascii(::mu::engraving::Smufl::smufl_all_symbols), all_symbols}); // TODO: make translatable as well as ranges.json

    return ranges;
}
