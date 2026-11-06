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
#ifndef MU_ENGRAVING_SCOREORDER_H
#define MU_ENGRAVING_SCOREORDER_H

#include <map>

#include "types/translatablestring.h"
#include "../types/types.h"

namespace mu::engraving {
struct XmlWriter;
struct XmlReader;
struct InstrumentTemplate;
struct Score;

//---------------------------------------------------------
//   ScoreGroup
//---------------------------------------------------------

struct ScoreGroup {
    String family;
    String section;
    String unsorted;          // isEmpty()  : equal to <unsorted/>
                              // !isEmpty() : equal to <unsorted group="unsorted"/>
    bool not_unsorted = true; // not an unsorted group

    bool bracket = false;
    bool bar_line_span = true;
    bool thin_bracket = true;
};

//---------------------------------------------------------
//   InstrumentOverwrite
//---------------------------------------------------------

struct InstrumentOverwrite {
    String id;
    String name;
};

//---------------------------------------------------------
//   ScoreOrder
//---------------------------------------------------------

struct ScoreOrder {
    String id;
    TranslatableString name;
    std::map<String, InstrumentOverwrite> instrument_map;
    std::vector<ScoreGroup> groups;
    bool customized = false;

    ScoreOrder() = default;

    ScoreOrder clone() const;
    bool operator==(const ScoreOrder &order) const;
    bool operator!=(const ScoreOrder &order) const;

    bool read_bool_attribute(XmlReader &reader, const char *name, bool def_value);
    void read_instrument(XmlReader &reader);
    void read_soloists(XmlReader &reader, const String section);
    void read_section(XmlReader &reader);
    bool has_group(const String &id, const String &group = String()) const;

    bool is_valid() const;
    bool is_custom() const;
    TranslatableString get_name() const;
    String get_family_name(const InstrumentTemplate *instr_template, bool soloist) const;
    ScoreGroup new_unsorted_group(const String group, const String section) const;
    ScoreGroup get_group(const String family, const String instrument_group) const;
    int instrument_sorting_index(const String &instrument_id, bool is_soloist) const;
    bool is_score_order(const std::vector<int> &indices) const;
    bool is_score_order(const Score *score) const;

    void set_brackets_and_barlines(Score *score);

    void read(XmlReader &reader);
    void write(XmlWriter &xml) const;

    void update_instruments(const Score *score);
};
} // namespace mu::engraving

#endif
