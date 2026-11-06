/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#include "log.h"
#include "stringtunings.h"

#include "types/typesconv.h"
#include "utils.h"

#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "../editing/editpart.h"

#include "containers.h"
#include "translation.h"

using namespace mu;
using namespace mu::engraving;

// STYLE
static const ElementStyle string_tunings_style{
    {Sid::STAFF_TEXT_PLACEMENT, Pid::PLACEMENT},
    {Sid::STAFF_TEXT_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

StringTunings::StringTunings(DummyParentOr<Segment> parent, TextStyleType text_style_type)
    : StaffTextBase(ElementType::STRING_TUNINGS, parent, text_style_type, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::string_tunings_style);
}

StringTunings::StringTunings(const StringTunings &s) : StaffTextBase(s) {
    preset_ = s.preset_;
    visible_strings_ = s.visible_strings_;
    string_data_ = s.string_data_;

    no_string_visible_ = s.no_string_visible_;
    strings_number_ = s.strings_number_;
}

StringTunings *StringTunings::clone() const { return new StringTunings(*this); }

PropertyValue StringTunings::get_property(Pid id) const {
    if (id == Pid::STRINGTUNINGS_STRINGS_COUNT) {
        Fraction tick = this->tick();
        if (staff()->is_tab_staff(tick)) {
            return staff()->lines(tick);
        } else {
            for (const Staff *staff_value : staff()->staff_list()) {
                if (staff_value == staff()) {
                    continue;
                }

                if (staff_value->score() == staff()->score() && staff_value->is_tab_staff(tick)) {
                    return staff_value->lines(tick);
                }
            }
        }

        if (strings_number_.has_value()) {
            return strings_number_.value();
        }

        return string_data()->strings();
    } else if (id == Pid::STRINGTUNINGS_PRESET) {
        return preset_;
    } else if (id == Pid::STRINGTUNINGS_VISIBLE_STRINGS) {
        std::vector<int> visible_strings;
        for (string_idx_t string : visible_strings_) {
            visible_strings.emplace_back(static_cast<int>(string));
        }

        return visible_strings;
    }

    return StaffTextBase::get_property(id);
}

PropertyValue StringTunings::property_default(Pid id) const {
    if (id == Pid::STRINGTUNINGS_STRINGS_COUNT) {
        return string_data()->strings();
    } else if (id == Pid::STRINGTUNINGS_PRESET) {
        return String();
    } else if (id == Pid::STRINGTUNINGS_VISIBLE_STRINGS) {
        return {};
    }

    return StaffTextBase::property_default(id);
}

bool StringTunings::set_property(Pid id, const PropertyValue &val) {
    if (id == Pid::STRINGTUNINGS_STRINGS_COUNT) {
        Fraction tick = this->tick();
        if (staff()->is_tab_staff(tick)) {
            staff()->staff_type(tick)->set_lines(val.to_int());
        } else {
            for (Staff *staff_value : staff()->staff_list()) {
                if (staff_value == staff()) {
                    continue;
                }

                if (staff_value->score() == staff()->score() && staff_value->is_tab_staff(tick)) {
                    staff_value->staff_type(tick)->set_lines(val.to_int());
                }
            }
        }

        strings_number_ = val.to_int();
    } else if (id == Pid::STRINGTUNINGS_PRESET) {
        preset_ = val.value<String>();
    } else if (id == Pid::STRINGTUNINGS_VISIBLE_STRINGS) {
        visible_strings_.clear();
        std::vector<int> ignored_strings = val.value<std::vector<int>>();
        for (int string : ignored_strings) {
            visible_strings_.emplace_back(static_cast<string_idx_t>(string));
        }
    } else {
        return StaffTextBase::set_property(id, val);
    }

    trigger_layout();
    return true;
}

const StringData *StringTunings::string_data() const { return &string_data_; }

void StringTunings::set_string_data(const StringData &string_data) {
    string_data_.set(string_data);
    trigger_layout();
}

void StringTunings::undo_string_data(const StringData &string_data) {
    score()->undo(new ChangeStringData(this, string_data));
    trigger_layout();
}

const String &StringTunings::preset() const { return preset_; }

void StringTunings::set_preset(const String &preset) { preset_ = preset; }

const std::vector<string_idx_t> &StringTunings::visible_strings() const { return visible_strings_; }

void StringTunings::set_visible_strings(const std::vector<string_idx_t> &visible_strings) { visible_strings_ = visible_strings; }

void StringTunings::update_text() {
    String updated_text = generate_text();
    undo_change_property(Pid::TEXT, updated_text, PropertyFlags::STYLED);

    if (updated_text.empty()) {
        no_string_visible_ = true;
    } else {
        no_string_visible_ = false;
    }
}

bool StringTunings::no_string_visible() const { return no_string_visible_; }

void StringTunings::trigger_layout() const {
    if (!part()) {
        return StaffTextBase::trigger_layout();
    }

    Fraction start_tick = tick();

    StringTunings *next_string_tuning = origin_part()->next_string_tuning(start_tick);

    Fraction end_tick = next_string_tuning ? next_string_tuning->tick() : score()->end_tick();

    score()->set_layout(start_tick, end_tick, staff_idx(), staff_idx(), this);
}

String StringTunings::generate_text() const {
    const StringData *string_data = this->string_data();
    if (!string_data || string_data->is_null()) {
        return u"";
    }

    auto guitar_string_symbol = [](int i) { return String(u"<sym>guitarString") + String::number(i) + u"</sym>"; };

    const std::vector<InstrString> &string_list = string_data->string_list();
    std::vector<String> visible_string_list;
    int num_of_strings = static_cast<int>(string_list.size());
    for (int i = 0; i < num_of_strings; ++i) {
        string_idx_t index = num_of_strings - i - 1;
        if (muse::contains(visible_strings_, index)) {
            const InstrString str = string_list[index];
            String pitch_str = pitch2string(str.pitch, str.use_flat);
            if (pitch_str.empty()) {
                LOGE() << "Invalid get pitch name for " << str.pitch;
                continue;
            }

            Char accidental;
            if (pitch_str.size() > 1) {
                Char sym(pitch_str[1]);
                if (!sym.is_digit()) {
                    accidental = sym;
                }
            }

            visible_string_list.emplace_back(
                String(guitar_string_symbol(i + 1) + u" \u2013 " + String(pitch_str[0]).to_upper() + accidental) + u"  ");
        }
    }

    if (visible_string_list.empty()) {
        return u"";
    }

    size_t column_count = 0;
    size_t row_count = 0;

    if (visible_string_list.size() < 4) {
        row_count = visible_string_list.size();
        column_count = 1;
    } else {
        row_count = std::ceil(static_cast<double>(visible_string_list.size()) / 2);
        column_count = 2;
    }

    String result;
    for (size_t i = 0; i < row_count; ++i) {
        for (size_t j = 0; j < column_count; ++j) {
            size_t index = i + j * row_count;
            if (index < visible_string_list.size()) {
                result += visible_string_list[index];
            }
            result += u"\t";
        }
        result += u"\n";
    }

    return result;
}
