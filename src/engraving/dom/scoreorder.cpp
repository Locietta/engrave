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
#include "scoreorder.h"

#include "../editing/editstaffbrackets.h"
#include "dom/bracketitem.h"
#include "dom/instrtemplate.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/staff.h"
#include "log.h"
#include "rw/xmlreader.h"
#include "rw/xmlwriter.h"

using namespace mu;

namespace mu::engraving {
static const String soloists_id(u"<soloists>");
static const String unsorted_id(u"<unsorted>");

//---------------------------------------------------------
//   clone
//---------------------------------------------------------

ScoreOrder ScoreOrder::clone() const {
    ScoreOrder new_order;

    new_order.id = id;
    new_order.name = name;
    new_order.customized = customized;
    new_order.instrument_map = instrument_map;

    for (const ScoreGroup &sg : groups) {
        ScoreGroup new_group;

        new_group.family = sg.family;
        new_group.section = sg.section;
        new_group.unsorted = sg.unsorted;
        new_group.not_unsorted = sg.not_unsorted;
        new_group.bracket = sg.bracket;
        new_group.bar_line_span = sg.bar_line_span;
        new_group.thin_bracket = sg.thin_bracket;

        new_order.groups.push_back(new_group);
    }

    return new_order;
}

//---------------------------------------------------------
//   readBoolAttribute
//---------------------------------------------------------

bool ScoreOrder::operator==(const ScoreOrder &order) const { return id == order.id; }

bool ScoreOrder::operator!=(const ScoreOrder &order) const { return !(*this == order); }

bool ScoreOrder::read_bool_attribute(XmlReader &reader, const char *attr_name, bool defvalue) {
    if (!reader.has_attribute(attr_name)) {
        return defvalue;
    }
    String attr{reader.attribute(attr_name)};
    if (attr.to_lower() == "false") {
        return false;
    } else if (attr.to_lower() == "true") {
        return true;
    }
    LOGD("invalid value \"%s\" for attribute \"%s\", using default \"%d\"", muPrintable(attr), attr_name, defvalue);
    return defvalue;
}

//---------------------------------------------------------
//   readInstrument
//---------------------------------------------------------

void ScoreOrder::read_instrument(XmlReader &reader) {
    String instrument_id{reader.attribute("id")};
    if (!mu::engraving::search_template(instrument_id)) {
        LOGD("cannot find instrument templates for <%s>", muPrintable(instrument_id));
        reader.skip_current_element();
        return;
    }
    while (reader.read_next_start_element()) {
        if (reader.name() == "family") {
            InstrumentOverwrite io;
            io.id = reader.attribute("id");
            io.name = reader.read_text();
            instrument_map.insert({instrument_id, io});
        } else {
            reader.unknown();
        }
    }
}

//---------------------------------------------------------
//   readSoloists
//---------------------------------------------------------

void ScoreOrder::read_soloists(XmlReader &reader, const String section) {
    reader.skip_current_element();
    if (has_group(::mu::engraving::soloists_id)) {
        return;
    }
    ScoreGroup sg;
    sg.family = String(::mu::engraving::soloists_id);
    sg.section = section;
    groups.push_back(sg);
}

//---------------------------------------------------------
//   readSection
//---------------------------------------------------------

void ScoreOrder::read_section(XmlReader &reader) {
    String section_id{reader.attribute("id")};
    bool bar_line_span = read_bool_attribute(reader, "barLineSpan", true);
    bool thin_brackets = read_bool_attribute(reader, "thinBrackets", true);
    while (reader.read_next_start_element()) {
        if (reader.name() == "family") {
            ScoreGroup sg;
            sg.family = reader.read_text();
            sg.section = section_id;
            sg.bracket = true;
            sg.bar_line_span = bar_line_span;
            sg.thin_bracket = thin_brackets;
            groups.push_back(sg);
        } else if (reader.name() == "unsorted") {
            String group{reader.attribute("group", String())};

            if (has_group(::mu::engraving::unsorted_id, group)) {
                reader.skip_current_element();
                return;
            }

            ScoreGroup sg;
            sg.family = String(::mu::engraving::unsorted_id);
            sg.section = section_id;
            sg.unsorted = group;
            sg.not_unsorted = false;
            sg.bracket = true;
            sg.bar_line_span = read_bool_attribute(reader, "barLineSpan", true);
            sg.thin_bracket = read_bool_attribute(reader, "thinBrackets", true);
            groups.push_back(sg);
            reader.skip_current_element();
        } else {
            reader.unknown();
        }
    }
}

//---------------------------------------------------------
//   hasGroup
//---------------------------------------------------------

bool ScoreOrder::has_group(const String &family_id, const String &group) const {
    for (const ScoreGroup &sg : groups) {
        if ((sg.family == family_id) && (group == sg.unsorted)) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   isValid
//---------------------------------------------------------

bool ScoreOrder::is_valid() const { return !id.empty(); }

//---------------------------------------------------------
//   isCustom
//---------------------------------------------------------

bool ScoreOrder::is_custom() const { return id == String(u"custom"); }

//---------------------------------------------------------
//   getName
//---------------------------------------------------------

TranslatableString ScoreOrder::get_name() const {
    return customized ? TranslatableString("engraving/scoreorder", "%1 (Customized)").arg(name) : name;
}

//---------------------------------------------------------
//   getFamilyName
//---------------------------------------------------------

String ScoreOrder::get_family_name(const InstrumentTemplate *instr_template, bool soloist) const {
    if (!instr_template) {
        return String(u"<unsorted>");
    }

    if (soloist) {
        return String(u"<soloists>");
    } else if (muse::contains(instrument_map, instr_template->id)) {
        return instrument_map.at(instr_template->id).id;
    } else if (instr_template->family) {
        return instr_template->family->id;
    }
    return String(u"<unsorted>");
}

//---------------------------------------------------------
//   newUnsortedGroup
//---------------------------------------------------------

ScoreGroup ScoreOrder::new_unsorted_group(const String group, const String section) const {
    ScoreGroup sg;
    sg.family = String(::mu::engraving::unsorted_id);
    sg.section = section;
    sg.unsorted = group;
    sg.not_unsorted = false;
    sg.bracket = false;
    sg.bar_line_span = false;
    sg.thin_bracket = false;
    return sg;
}

//---------------------------------------------------------
//   getGroup
//---------------------------------------------------------

ScoreGroup ScoreOrder::get_group(const String family, const String instrument_group) const {
    static const String unsorted_value = String(u"<unsorted>");

    ScoreGroup unsorted_score_group;
    for (const ScoreGroup &sg : groups) {
        if ((sg.family == unsorted_value) && sg.unsorted.is_empty()) {
            unsorted_score_group = sg;
            break;
        }
    }

    if (family.is_empty()) {
        return unsorted_score_group;
    }

    for (const ScoreGroup &sg : groups) {
        if (sg.family == family) {
            return sg;
        }
        if ((sg.family == unsorted_value) && (sg.unsorted == instrument_group)) {
            unsorted_score_group = sg;
        }
    }
    return unsorted_score_group;
}

//---------------------------------------------------------
//   instrumentSortingIndex
//---------------------------------------------------------

int ScoreOrder::instrument_sorting_index(const String &instrument_id, bool is_soloist) const {
    static const String soloists_group(u"<soloists>");
    static const String unsorted_group(u"<unsorted>");

    enum class Priority : unsigned char { UNDEFINED, UNSORTED, UNSORTED_GROUP, FAMILY, SOLOIST };

    InstrumentIndex ii{search_template_index_for_id(instrument_id)};
    if (!ii.instr_template) {
        return 0;
    }

    String family = muse::contains(instrument_map, instrument_id) ? instrument_map.at(instrument_id).id : ii.instr_template->family_id();

    size_t index = groups.size();

    auto calculate_index = [instrument_id, &ii](size_t index) { return index * ii.template_count + ii.instr_template->sequence_order; };

    Priority priority = Priority::UNDEFINED;

    for (size_t i = 0; i < groups.size(); ++i) {
        const ScoreGroup &sg = groups.at(i);

        if ((sg.family == soloists_group) && is_soloist) {
            return static_cast<int>(calculate_index(i));
        } else if ((priority < Priority::FAMILY) && (sg.family == family)) {
            index = i;
            priority = Priority::FAMILY;
        } else if ((priority < Priority::UNSORTED_GROUP) && (sg.family == unsorted_group) && (sg.unsorted == ii.instr_template->group_id)) {
            index = i;
            priority = Priority::UNSORTED_GROUP;
        } else if ((priority < Priority::UNSORTED) && (sg.family == unsorted_group) && (sg.unsorted.empty())) {
            index = i;
            priority = Priority::UNSORTED;
        }
    }

    return static_cast<int>(calculate_index(index));
}

//---------------------------------------------------------
//   isScoreOrder
//---------------------------------------------------------

bool ScoreOrder::is_score_order(const std::vector<int> &indices) const {
    if (is_custom()) {
        return true;
    }

    int prv_index{-1};
    for (int cur_index : indices) {
        if (cur_index < prv_index) {
            return false;
        }
        prv_index = cur_index;
    }
    return true;
}

bool ScoreOrder::is_score_order(const Score *score) const {
    std::vector<int> indices;
    indices.reserve(score->parts().size());
    for (const Part *part : score->parts()) {
        indices.push_back(instrument_sorting_index(part->instrument()->id(), part->soloist()));
    }

    return is_score_order(indices);
}

//---------------------------------------------------------
//   setBracketsAndBarlines
//---------------------------------------------------------

void ScoreOrder::set_brackets_and_barlines(Score *score) {
    if (groups.size() <= 0) {
        return;
    }

    bool prv_thn_bracket{false};
    bool prv_bar_line_span{false};
    String prv_section;
    int prv_instrument{-1};
    Staff *prv_staff{nullptr};

    Staff *thk_bracket_staff{nullptr};
    Staff *thn_bracket_staff{nullptr};
    int thk_bracket_span{0};
    int thn_bracket_span{0};

    for (Part *part : score->parts()) {
        InstrumentIndex ii = search_template_index_for_id(part->instrument()->id());
        const InstrumentTemplate *instr_template = ii.instr_template;
        if (!instr_template) {
            continue;
        }

        String family{get_family_name(instr_template, part->soloist())};
        const ScoreGroup sg = get_group(family, instrument_groups[ii.group_index]->id);

        staff_idx_t staff_idx{0};
        const bool is_multi_staff_instrument = instr_template->staff_count > 1;
        const size_t part_staff_count = part->nstaves();

        const int template_bracket_span =
            std::accumulate(instr_template->bracket_span, instr_template->bracket_span + ::mu::engraving::max_staves, 0);
        const bool bracket_spans_all_staves = template_bracket_span == (int) instr_template->staff_count;
        const int template_barline_span = std::count(instr_template->barline_span.begin(), instr_template->barline_span.end(), true);
        const bool barline_spans_all_staves = template_barline_span == (int) instr_template->staff_count - 1;
        for (Staff *staff : part->staves()) {
            // Create copy, because the original is modified while we are iterating over it
            std::vector<BracketItem *> brackets = score->brackets(staff->idx());

            for (BracketItem *bi : brackets) {
                if (bi->bracket_type() == BracketType::GROUP) {
                    continue;
                }
                score->undo(new RemoveBracket(staff, bi->column(), bi->bracket_type(), bi->bracket_span()));
            }
            staff->undo_change_property(Pid::STAFF_BARLINE_SPAN, false);

            // Reset template braces & barline spans
            // The number of staves could differ from the template
            // Use template spans if span doesn't cover whole set of staves (eg. organ), otherwise cover all staves
            if (part_staff_count > 1) {
                size_t bracket_span = bracket_spans_all_staves ? part_staff_count : instr_template->bracket_span[staff_idx];
                EditStaffBrackets::undo_add_bracket(score, staff, 0, instr_template->bracket[staff_idx], bracket_span);

                int barline_span = instr_template->barline_span[barline_spans_all_staves - 1 ? 0 : staff_idx];
                staff->undo_change_property(Pid::STAFF_BARLINE_SPAN, barline_span);
            }

            if ((prv_section.is_empty() || (sg.section != prv_section)) && score->parts().size() > 1) {
                if (thk_bracket_staff && (thk_bracket_span > 1)) {
                    EditStaffBrackets::undo_add_bracket(score, thk_bracket_staff, 0, BracketType::NORMAL, thk_bracket_span);
                }
                if (!staff_idx) {
                    thk_bracket_staff = sg.bracket ? staff : nullptr;
                    thk_bracket_span = 0;
                }
            }
            if (sg.bracket && !staff_idx) {
                thk_bracket_span += static_cast<int>(part->nstaves());
            }

            if (prv_instrument == -1 || (ii.instr_index != prv_instrument) || is_multi_staff_instrument) {
                if (thn_bracket_staff && (thn_bracket_span > 1)) {
                    EditStaffBrackets::undo_add_bracket(score, thn_bracket_staff, 1, BracketType::SQUARE, thn_bracket_span);
                }
                if (ii.instr_index != prv_instrument || is_multi_staff_instrument) {
                    thn_bracket_staff = nullptr;
                    thn_bracket_span = 0;
                }
            }
            if (!thn_bracket_staff && sg.thin_bracket && !is_multi_staff_instrument && !staff_idx) {
                thn_bracket_staff = staff;
                thn_bracket_span = 0;
            }

            if (is_multi_staff_instrument) {
                if (staff_idx < instr_template->staff_count) {
                    ++staff_idx;
                }
                prv_staff = nullptr;
            } else {
                if (sg.thin_bracket && !staff_idx) {
                    thn_bracket_span += static_cast<int>(part->nstaves());
                }
                if (prv_staff) {
                    const bool new_barline_span = prv_bar_line_span && (!prv_section.is_empty() && (sg.section == prv_section));
                    prv_staff->undo_change_property(Pid::STAFF_BARLINE_SPAN, new_barline_span);
                }
                prv_staff = staff;
                ++staff_idx;
            }
            prv_section = sg.section;
            prv_bar_line_span = sg.bar_line_span;
            prv_thn_bracket = sg.thin_bracket;
        }

        prv_instrument = ii.instr_index;
    }

    if (thk_bracket_staff && (thk_bracket_span > 1)) {
        EditStaffBrackets::undo_add_bracket(score, thk_bracket_staff, 0, BracketType::NORMAL, thk_bracket_span);
    }
    if (thn_bracket_staff && (thn_bracket_span > 1) && prv_thn_bracket) {
        EditStaffBrackets::undo_add_bracket(score, thn_bracket_staff, 1, BracketType::SQUARE, thn_bracket_span);
    }
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void ScoreOrder::read(XmlReader &reader) {
    id = reader.attribute("id");
    const String section_id;
    while (reader.read_next_start_element()) {
        if (reader.name() == "name") {
            name = TranslatableString("engraving/scoreorder", reader.read_text());
        } else if (reader.name() == "section") {
            read_section(reader);
        } else if (reader.name() == "instrument") {
            read_instrument(reader);
        } else if (reader.name() == "family") {
            ScoreGroup sg;
            sg.family = reader.read_text();
            sg.section = section_id;
            sg.bracket = false;
            sg.bar_line_span = false;
            sg.thin_bracket = false;
            groups.push_back(sg);
        } else if (reader.name() == "soloists") {
            read_soloists(reader, section_id);
        } else if (reader.name() == "unsorted") {
            String group{reader.attribute("group", String())};

            if (!has_group(::mu::engraving::unsorted_id, group)) {
                groups.push_back(new_unsorted_group(group, section_id));
            }

            reader.skip_current_element();
        } else {
            reader.unknown();
        }
    }

    String group;
    if (!has_group(::mu::engraving::unsorted_id, group)) {
        groups.push_back(new_unsorted_group(group, id));
    }
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void ScoreOrder::write(XmlWriter &xml) const {
    if (!is_valid()) {
        return;
    }

    xml.start_element("Order", {{"id", id}});
    xml.tag("name", name.str);

    for (const auto &p : instrument_map) {
        xml.start_element("instrument", {{"id", p.first}});
        xml.tag("family", {{"id", p.second.id}}, p.second.name);
        xml.end_element();
    }

    String section{u""};
    for (const ScoreGroup &sg : groups) {
        if (sg.section != section) {
            if (!section.is_empty()) {
                xml.end_element();
            }
            if (!sg.section.is_empty()) {
                xml.start_element("section", {{"id", sg.section},
                                              {"brackets", sg.bracket ? "true" : "false"},
                                              {"barLineSpan", sg.bar_line_span ? "true" : "false"},
                                              {"thinBrackets", sg.thin_bracket ? "true" : "false"}});
            }
            section = sg.section;
        }
        if (sg.family == ::mu::engraving::soloists_id) {
            xml.tag("soloists");
        } else if (sg.not_unsorted) {
            xml.tag("family", sg.family);
        } else if (sg.unsorted.is_empty()) {
            xml.tag("unsorted");
        } else {
            xml.tag("unsorted", {{"group", sg.unsorted}});
        }
    }
    if (!section.is_empty()) {
        xml.end_element();
    }
    xml.end_element();
}

//---------------------------------------------------------
//   updateInstruments
//---------------------------------------------------------

void ScoreOrder::update_instruments(const Score *score) {
    for (const Part *part : score->parts()) {
        InstrumentIndex ii = search_template_index_for_id(part->instrument()->id());
        if (!ii.instr_template || !ii.instr_template->family) {
            continue;
        }

        InstrumentOverwrite io;
        io.id = ii.instr_template->family->id;
        io.name = ii.instr_template->family->name;
        instrument_map.emplace(ii.instr_template->id, std::move(io));
    }
}
} // namespace mu::engraving
