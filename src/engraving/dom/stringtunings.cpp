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
static const ElementStyle STRING_TUNINGS_STYLE{
    {Sid::staffTextPlacement, Pid::PLACEMENT},
    {Sid::staffTextMinDistance, Pid::MIN_DISTANCE},
};

StringTunings::StringTunings(DummyParentOr<Segment> parent, TextStyleType textStyleType)
    : StaffTextBase(ElementType::STRING_TUNINGS, parent, textStyleType, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    initElementStyle(&STRING_TUNINGS_STYLE);
}

StringTunings::StringTunings(const StringTunings &s) : StaffTextBase(s) {
    preset_ = s.preset_;
    visibleStrings_ = s.visibleStrings_;
    stringData_ = s.stringData_;

    noStringVisible_ = s.noStringVisible_;
    stringsNumber_ = s.stringsNumber_;
}

StringTunings *StringTunings::clone() const { return new StringTunings(*this); }

PropertyValue StringTunings::getProperty(Pid id) const {
    if (id == Pid::STRINGTUNINGS_STRINGS_COUNT) {
        Fraction tick = this->tick();
        if (staff()->isTabStaff(tick)) {
            return staff()->lines(tick);
        } else {
            for (const Staff *_staff : staff()->staffList()) {
                if (_staff == staff()) {
                    continue;
                }

                if (_staff->score() == staff()->score() && _staff->isTabStaff(tick)) {
                    return _staff->lines(tick);
                }
            }
        }

        if (stringsNumber_.has_value()) {
            return stringsNumber_.value();
        }

        return stringData()->strings();
    } else if (id == Pid::STRINGTUNINGS_PRESET) {
        return preset_;
    } else if (id == Pid::STRINGTUNINGS_VISIBLE_STRINGS) {
        std::vector<int> visibleStrings;
        for (string_idx_t string : visibleStrings_) {
            visibleStrings.emplace_back(static_cast<int>(string));
        }

        return visibleStrings;
    }

    return StaffTextBase::getProperty(id);
}

PropertyValue StringTunings::propertyDefault(Pid id) const {
    if (id == Pid::STRINGTUNINGS_STRINGS_COUNT) {
        return stringData()->strings();
    } else if (id == Pid::STRINGTUNINGS_PRESET) {
        return String();
    } else if (id == Pid::STRINGTUNINGS_VISIBLE_STRINGS) {
        return {};
    }

    return StaffTextBase::propertyDefault(id);
}

bool StringTunings::setProperty(Pid id, const PropertyValue &val) {
    if (id == Pid::STRINGTUNINGS_STRINGS_COUNT) {
        Fraction tick = this->tick();
        if (staff()->isTabStaff(tick)) {
            staff()->staffType(tick)->setLines(val.toInt());
        } else {
            for (Staff *_staff : staff()->staffList()) {
                if (_staff == staff()) {
                    continue;
                }

                if (_staff->score() == staff()->score() && _staff->isTabStaff(tick)) {
                    _staff->staffType(tick)->setLines(val.toInt());
                }
            }
        }

        stringsNumber_ = val.toInt();
    } else if (id == Pid::STRINGTUNINGS_PRESET) {
        preset_ = val.value<String>();
    } else if (id == Pid::STRINGTUNINGS_VISIBLE_STRINGS) {
        visibleStrings_.clear();
        std::vector<int> ignoredStrings = val.value<std::vector<int>>();
        for (int string : ignoredStrings) {
            visibleStrings_.emplace_back(static_cast<string_idx_t>(string));
        }
    } else {
        return StaffTextBase::setProperty(id, val);
    }

    triggerLayout();
    return true;
}

const StringData *StringTunings::stringData() const { return &stringData_; }

void StringTunings::setStringData(const StringData &stringData) {
    stringData_.set(stringData);
    triggerLayout();
}

void StringTunings::undoStringData(const StringData &stringData) {
    score()->undo(new ChangeStringData(this, stringData));
    triggerLayout();
}

const String &StringTunings::preset() const { return preset_; }

void StringTunings::setPreset(const String &preset) { preset_ = preset; }

const std::vector<string_idx_t> &StringTunings::visibleStrings() const { return visibleStrings_; }

void StringTunings::setVisibleStrings(const std::vector<string_idx_t> &visibleStrings) { visibleStrings_ = visibleStrings; }

void StringTunings::updateText() {
    String updatedText = generateText();
    undoChangeProperty(Pid::TEXT, updatedText, PropertyFlags::STYLED);

    if (updatedText.empty()) {
        noStringVisible_ = true;
    } else {
        noStringVisible_ = false;
    }
}

bool StringTunings::noStringVisible() const { return noStringVisible_; }

void StringTunings::triggerLayout() const {
    if (!part()) {
        return StaffTextBase::triggerLayout();
    }

    Fraction startTick = tick();

    StringTunings *nextStringTuning = originPart()->nextStringTuning(startTick);

    Fraction endTick = nextStringTuning ? nextStringTuning->tick() : score()->endTick();

    score()->setLayout(startTick, endTick, staffIdx(), staffIdx(), this);
}

String StringTunings::generateText() const {
    const StringData *stringData = this->stringData();
    if (!stringData || stringData->isNull()) {
        return u"";
    }

    auto guitarStringSymbol = [](int i) { return String(u"<sym>guitarString") + String::number(i) + u"</sym>"; };

    const std::vector<instrString> &stringList = stringData->stringList();
    std::vector<String> visibleStringList;
    int numOfStrings = static_cast<int>(stringList.size());
    for (int i = 0; i < numOfStrings; ++i) {
        string_idx_t index = numOfStrings - i - 1;
        if (muse::contains(visibleStrings_, index)) {
            const instrString str = stringList[index];
            String pitchStr = pitch2string(str.pitch, str.useFlat);
            if (pitchStr.empty()) {
                LOGE() << "Invalid get pitch name for " << str.pitch;
                continue;
            }

            Char accidental;
            if (pitchStr.size() > 1) {
                Char sym(pitchStr[1]);
                if (!sym.isDigit()) {
                    accidental = sym;
                }
            }

            visibleStringList.emplace_back(String(guitarStringSymbol(i + 1) + u" \u2013 " + String(pitchStr[0]).toUpper() + accidental) +
                                           u"  ");
        }
    }

    if (visibleStringList.empty()) {
        return u"";
    }

    size_t columnCount = 0;
    size_t rowCount = 0;

    if (visibleStringList.size() < 4) {
        rowCount = visibleStringList.size();
        columnCount = 1;
    } else {
        rowCount = std::ceil(static_cast<double>(visibleStringList.size()) / 2);
        columnCount = 2;
    }

    String result;
    for (size_t i = 0; i < rowCount; ++i) {
        for (size_t j = 0; j < columnCount; ++j) {
            size_t index = i + j * rowCount;
            if (index < visibleStringList.size()) {
                result += visibleStringList[index];
            }
            result += u"\t";
        }
        result += u"\n";
    }

    return result;
}
