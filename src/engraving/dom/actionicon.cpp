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
    icon_font_ = Font(configuration()->icons_font_family(), Font::Type::ICON);
    icon_font_.set_point_size_f(::mu::engraving::ui_icons_default_font_size);
}

ActionIcon *ActionIcon::clone() const { return new ActionIcon(*this); }

void ActionIcon::set_action_type(ActionIconType val) { action_type_ = val; }

ActionIconType ActionIcon::action_type() const { return action_type_; }

const muse::TranslatableString &ActionIcon::title() const { return TConv::user_name(action_type_); }

char16_t ActionIcon::icon() const { return TConv::icon_code(action_type_); }

engraving::PropertyValue ActionIcon::get_property(Pid pid) const {
    switch (pid) {
        case Pid::ACTION: return PropertyValue(action_type_);
        default: break;
    }
    return EngravingItem::get_property(pid);
}

bool ActionIcon::set_property(Pid pid, const PropertyValue &v) {
    switch (pid) {
        case Pid::ACTION:
            action_type_ = v.value<ActionIconType>();
            trigger_layout();
            break;
        default: return EngravingItem::set_property(pid, v);
    }
    return true;
}
