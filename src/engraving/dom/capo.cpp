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

static const ElementStyle CAPO_STYLE{
    {Sid::staffTextPlacement, Pid::PLACEMENT},
    {Sid::staffTextMinDistance, Pid::MIN_DISTANCE},
};

Capo::Capo(DummyParentOr<Segment> parent, TextStyleType textStyleType)
    : StaffTextBase(ElementType::CAPO, parent, textStyleType, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    initElementStyle(&CAPO_STYLE);

    params_.active = true;
    params_.fretPosition = 1;
}

Capo *Capo::clone() const { return new Capo(*this); }

PropertyValue Capo::getProperty(Pid id) const {
    if (id == Pid::ACTIVE) {
        return params_.active;
    } else if (id == Pid::CAPO_FRET_POSITION) {
        return params_.fretPosition;
    } else if (id == Pid::CAPO_IGNORED_STRINGS) {
        std::vector<int> ignoredStrings;
        ignoredStrings.reserve(params_.ignoredStrings.size());
        for (string_idx_t string : params_.ignoredStrings) {
            ignoredStrings.push_back(static_cast<int>(string));
        }
        return ignoredStrings;
    } else if (id == Pid::CAPO_GENERATE_TEXT) {
        return shouldAutomaticallyGenerateText_;
    } else if (id == Pid::CAPO_TRANSPOSE_MODE) {
        return params_.transposeMode;
    }

    return StaffTextBase::getProperty(id);
}

PropertyValue Capo::propertyDefault(Pid id) const {
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

    return StaffTextBase::propertyDefault(id);
}

bool Capo::setProperty(Pid id, const PropertyValue &val) {
    if (id == Pid::ACTIVE) {
        params_.active = val.toBool();
    } else if (id == Pid::CAPO_FRET_POSITION) {
        params_.fretPosition = val.toInt();
    } else if (id == Pid::CAPO_IGNORED_STRINGS) {
        params_.ignoredStrings.clear();
        auto ignoredStrings = val.value<std::vector<int>>();
        for (int string : ignoredStrings) {
            params_.ignoredStrings.insert(static_cast<string_idx_t>(string));
        }
    } else if (id == Pid::CAPO_GENERATE_TEXT) {
        shouldAutomaticallyGenerateText_ = val.toBool();

        if (!shouldAutomaticallyGenerateText_) {
            setXmlText(customText_);
        }
    } else if (id == Pid::CAPO_TRANSPOSE_MODE) {
        params_.transposeMode = val.value<CapoParams::TransposeMode>();
    } else {
        return StaffTextBase::setProperty(id, val);
    }

    triggerLayout();

    if (Score *s = score()) {
        s->updateCapo();
    }

    return true;
}

void Capo::setXmlText(const String &text) {
    if (!shouldAutomaticallyGenerateText_) {
        customText_ = text;
    }

    StaffTextBase::setXmlText(text);
}

const CapoParams &Capo::params() const { return params_; }

void Capo::setParams(const CapoParams &params) { params_ = params; }

bool Capo::shouldAutomaticallyGenerateText() const { return shouldAutomaticallyGenerateText_; }

muse::String Capo::generateText(size_t stringCount) const {
    if (!params_.active || params_.fretPosition == 0) {
        return muse::mtrc("engraving", "No capo");
    }

    if (params_.ignoredStrings.empty()) {
        return muse::mtrc("engraving", "Capo %1").arg(params_.fretPosition);
    }

    StringList stringsToApply;

    for (string_idx_t idx = 0; idx < stringCount; ++idx) {
        if (muse::contains(params_.ignoredStrings, idx)) {
            continue;
        }

        stringsToApply.emplace_back(String::number(idx + 1));
    }

    if (stringsToApply.empty()) {
        return muse::mtrc("engraving", "No capo");
    }

    String text = muse::mtrc("engraving", "Partial capo:\nFret %1 on strings %2").arg(params_.fretPosition).arg(stringsToApply.join(u", "));

    return text;
}

void Capo::added() {
    StaffTextBase::added();

    if (Score *s = score()) {
        s->updateCapo();
    }
}

void Capo::removed() {
    StaffTextBase::removed();

    if (Score *s = score()) {
        s->updateCapo();
    }
}
