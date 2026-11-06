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

#include "io/file.h"
#include "serialization/json.h"

#include "types/symnames.h"

#include "dom/mscore.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace mu::engraving;

std::array<Smufl::Code, size_t(SymId::LAST_SYM) + 1> Smufl::s_sym_id_codes{{}};

const Smufl::Code Smufl::code(SymId id) { return s_sym_id_codes.at(static_cast<size_t>(id)); }

char32_t Smufl::smufl_code(SymId id) { return s_sym_id_codes.at(static_cast<size_t>(id)).smufl_code; }

bool Smufl::init() {
    bool ok = load_glyph_names_json();

    return ok;
}

bool Smufl::load_glyph_names_json() {
    File file(":/fonts/smufl/glyphnames.json");
    if (!file.open(IODevice::READ_ONLY)) {
        LOGE() << "could not open glyph names JSON file.";
        return false;
    }

    std::string error;
    JsonObject glyph_names_json = JsonDocument::from_json(file.read_all(), &error).root_object();
    file.close();

    if (!error.empty()) {
        LOGE() << "JSON parse error in glyph names file: " << error;
        return false;
    }

    IF_ASSERT_FAILED(!glyph_names_json.empty()) {
        LOGE() << "Could not read glyph names JSON";
        return false;
    }

    for (size_t i = 0; i < s_sym_id_codes.size(); ++i) {
        SymId sym = static_cast<SymId>(i);
        if (sym == SymId::NO_SYM || sym == SymId::LAST_SYM) {
            continue;
        }

        std::string name(SymNames::name_for_sym_id(sym).ascii());
        JsonObject sym_obj = glyph_names_json.value(name).to_object();
        if (!sym_obj.is_valid()) {
            continue;
        }

        bool ok;
        char32_t code = sym_obj.value("codepoint").to_string().mid(2).to_u_int(&ok, 16);
        if (ok) {
            s_sym_id_codes[i].smufl_code = code;
        } else if (MScore::debug_mode) {
            LOGD() << "could not read codepoint for glyph " << name;
        }

        char32_t alernative_code = sym_obj.value("alternateCodepoint").to_string().mid(2).to_u_int(&ok, 16);
        if (ok) {
            s_sym_id_codes[i].music_sym_block_code = alernative_code;
        } else if (MScore::debug_mode) {
            LOGD() << "could not read alternate codepoint for glyph " << name;
        }
    }
    return true;
}

const std::map<String, std::vector<SymId>> &Smufl::smufl_ranges() {
    static std::map<String, std::vector<SymId>> ranges = read_smufl_ranges_json();
    return ranges;
}

std::map<String, std::vector<SymId>> Smufl::read_smufl_ranges_json() {
    std::map<String, std::vector<SymId>> ranges;
    std::vector<SymId> all_symbols;

    File fi(":/fonts/smufl/ranges.json");
    if (!fi.open(IODevice::READ_ONLY)) {
        LOGE() << "failed open: " << fi.file_path();
        return ranges;
    }

    std::string error;
    JsonObject o = JsonDocument::from_json(fi.read_all(), &error).root_object();
    if (!error.empty()) {
        LOGE() << "failed parse, err: " << error << ", file: " << fi.file_path();
        return ranges;
    }

    for (const std::string &s : o.keys()) {
        const JsonObject range = o.value(s).to_object();
        const String desc = range.value("description").to_string();
        const JsonArray glyphs = range.value("glyphs").to_array();

        if (glyphs.empty()) {
            continue;
        }
        std::vector<SymId> range_symbols;
        for (size_t i = 0; i < glyphs.size(); ++i) {
            String sym_name = glyphs.at(i).to_string();
            SymId sym_id = SymNames::sym_id_by_name(sym_name);
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
