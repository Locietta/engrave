/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include "capo.h"

#include "score.h"

#include "translation.h"

using namespace mu::engraving;

static const ElementStyle capo_style{
    {Sid::STAFF_TEXT_PLACEMENT, Pid::PLACEMENT},
    {Sid::STAFF_TEXT_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

Capo::Capo(DummyParentOr<Segment> parent, TextStyleType text_style_type)
    : StaffTextBase(ElementType::CAPO, parent, text_style_type, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::capo_style);

    params_.active = true;
    params_.fret_position = 1;
}

Capo *Capo::clone() const { return new Capo(*this); }

PropertyValue Capo::get_property(Pid id) const {
    if (id == Pid::ACTIVE) {
        return params_.active;
    } else if (id == Pid::CAPO_FRET_POSITION) {
        return params_.fret_position;
    } else if (id == Pid::CAPO_IGNORED_STRINGS) {
        std::vector<int> ignored_strings;
        ignored_strings.reserve(params_.ignored_strings.size());
        for (string_idx_t string : params_.ignored_strings) {
            ignored_strings.push_back(static_cast<int>(string));
        }
        return ignored_strings;
    } else if (id == Pid::CAPO_GENERATE_TEXT) {
        return should_automatically_generate_text_;
    } else if (id == Pid::CAPO_TRANSPOSE_MODE) {
        return params_.transpose_mode;
    }

    return StaffTextBase::get_property(id);
}

PropertyValue Capo::property_default(Pid id) const {
    if (id == Pid::ACTIVE) {
        return true;
    } else if (id == Pid::CAPO_FRET_POSITION) {
        return 1;
    } else if (id == Pid::CAPO_IGNORED_STRINGS) {
        return std::vector<int>();
    } else if (id == Pid::CAPO_GENERATE_TEXT) {
        return true;
    } else if (id == Pid::CAPO_TRANSPOSE_MODE) {
        return CapoParams::TransposeMode::PLAYBACK_ONLY;
    }

    return StaffTextBase::property_default(id);
}

bool Capo::set_property(Pid id, const PropertyValue &val) {
    if (id == Pid::ACTIVE) {
        params_.active = val.to_bool();
    } else if (id == Pid::CAPO_FRET_POSITION) {
        params_.fret_position = val.to_int();
    } else if (id == Pid::CAPO_IGNORED_STRINGS) {
        params_.ignored_strings.clear();
        auto ignored_strings = val.value<std::vector<int>>();
        for (int string : ignored_strings) {
            params_.ignored_strings.insert(static_cast<string_idx_t>(string));
        }
    } else if (id == Pid::CAPO_GENERATE_TEXT) {
        should_automatically_generate_text_ = val.to_bool();

        if (!should_automatically_generate_text_) {
            set_xml_text(custom_text_);
        }
    } else if (id == Pid::CAPO_TRANSPOSE_MODE) {
        params_.transpose_mode = val.value<CapoParams::TransposeMode>();
    } else {
        return StaffTextBase::set_property(id, val);
    }

    trigger_layout();

    if (Score *s = score()) {
        s->update_capo();
    }

    return true;
}

void Capo::set_xml_text(const String &text) {
    if (!should_automatically_generate_text_) {
        custom_text_ = text;
    }

    StaffTextBase::set_xml_text(text);
}

const CapoParams &Capo::params() const { return params_; }

void Capo::set_params(const CapoParams &params) { params_ = params; }

bool Capo::should_automatically_generate_text() const { return should_automatically_generate_text_; }

muse::String Capo::generate_text(size_t string_count) const {
    if (!params_.active || params_.fret_position == 0) {
        return muse::mtrc("engraving", "No capo");
    }

    if (params_.ignored_strings.empty()) {
        return muse::mtrc("engraving", "Capo %1").arg(params_.fret_position);
    }

    StringList strings_to_apply;

    for (string_idx_t idx = 0; idx < string_count; ++idx) {
        if (muse::contains(params_.ignored_strings, idx)) {
            continue;
        }

        strings_to_apply.emplace_back(String::number(idx + 1));
    }

    if (strings_to_apply.empty()) {
        return muse::mtrc("engraving", "No capo");
    }

    String text =
        muse::mtrc("engraving", "Partial capo:\nFret %1 on strings %2").arg(params_.fret_position).arg(strings_to_apply.join(u", "));

    return text;
}

void Capo::added() {
    StaffTextBase::added();

    if (Score *s = score()) {
        s->update_capo();
    }
}

void Capo::removed() {
    StaffTextBase::removed();

    if (Score *s = score()) {
        s->update_capo();
    }
}
