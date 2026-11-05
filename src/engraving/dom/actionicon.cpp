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

#include "actionicon.h"

#include "iengravingconfiguration.h" // IWYU pragma: keep

#include "mscore.h"
#include "property.h"
#include "types/typesconv.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

ActionIcon::ActionIcon(DummyParentOr<EngravingItem> parent) : EngravingItem(ElementType::ACTION_ICON, parent) {
    iconFont_ = Font(configuration()->iconsFontFamily(), Font::Type::Icon);
    iconFont_.setPointSizeF(UI_ICONS_DEFAULT_FONT_SIZE);
}

ActionIcon *ActionIcon::clone() const { return new ActionIcon(*this); }

void ActionIcon::setActionType(ActionIconType val) { actionType_ = val; }

ActionIconType ActionIcon::actionType() const { return actionType_; }

const muse::TranslatableString &ActionIcon::title() const { return TConv::userName(actionType_); }

char16_t ActionIcon::icon() const { return TConv::iconCode(actionType_); }

engraving::PropertyValue ActionIcon::getProperty(Pid pid) const {
    switch (pid) {
        case Pid::ACTION: return PropertyValue(actionType_);
        default: break;
    }
    return EngravingItem::getProperty(pid);
}

bool ActionIcon::setProperty(Pid pid, const PropertyValue &v) {
    switch (pid) {
        case Pid::ACTION:
            actionType_ = v.value<ActionIconType>();
            triggerLayout();
            break;
        default: return EngravingItem::setProperty(pid, v);
    }
    return true;
}
