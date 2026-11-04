/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "editproperty.h"

#include <algorithm>

#include "../dom/bracketitem.h"
#include "../dom/score.h"
#include "../dom/staff.h"
#include "../dom/textline.h"

#include "log.h"
#define LOG_UNDO() \
    if (0) LOGD()

using namespace mu::engraving;

//---------------------------------------------------------
//   ChangeProperty::flip
//---------------------------------------------------------

void ChangeProperty::flip() {
    LOG_UNDO() << element->typeName() << int(id) << "(" << propertyName(id) << ")" << element->getProperty(id) << "->" << property;

    PropertyValue v = element->getProperty(id);
    PropertyFlags ps = element->propertyFlags(id);

    if (element->isChordLine() && (std::find(ChordLine::appearanceProperties.begin(), ChordLine::appearanceProperties.end(), id) !=
                                       ChordLine::appearanceProperties.end() ||
                                   id == Pid::APPEARANCE_LINKED_TO_MASTER)) {
        auto *line = toChordLine(element);
        auto restore = chordLineAppearance;
        chordLineAppearance = line->appearanceState();
        const auto next = property;
        const auto nextFlags = flags;
        property = v;
        flags = ps;
        if (restore) {
            line->restoreAppearance(*restore);
            line->setPropertyFlags(id, nextFlags);
        } else {
            if (resetChordLinePath && id == Pid::PATH) {
                line->resetProperty(id);
            } else {
                line->setProperty(id, next);
            }
            line->setPropertyFlags(id, nextFlags);
        }
        return;
    }

    element->setProperty(id, property);
    element->setPropertyFlags(id, flags);

    property = v;
    flags = ps;
}

std::vector<EngravingObject *> ChangeProperty::objectItems() const { return compoundObjects(element); }

//---------------------------------------------------------
//   ChangeBracketProperty::flip
//---------------------------------------------------------

void ChangeBracketProperty::flip() {
    if (!staff) {
        return;
    }

    element = staff->score()->brackets(staff->idx())[level];
    ChangeProperty::flip();
    level = toBracketItem(element)->column();
}

//---------------------------------------------------------
//   ChangeTextLineProperty::flip
//---------------------------------------------------------

void ChangeTextLineProperty::flip() {
    ChangeProperty::flip();
    if (element->isTextLine()) {
        toTextLine(element)->initStyle();
    }
}
