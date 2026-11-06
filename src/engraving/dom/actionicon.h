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

#pragma once

#include "engravingitem.h"

#include "draw/types/font.h"
#include "global/types/translatablestring.h"

#include "../types/types.h"

namespace mu::engraving {
//! Dummy element, used for drag&drop
class ActionIcon final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, ActionIcon)
    DECLARE_CLASSOF(ElementType::ACTION_ICON)

public:
    ActionIcon(DummyParentOr<EngravingItem> score);
    ~ActionIcon() override = default;

    ActionIcon *clone() const override;

    void set_action_type(ActionIconType val);
    ActionIconType action_type() const;

    const muse::TranslatableString &title() const;
    char16_t icon() const;

    const muse::draw::Font &icon_font() const { return icon_font_; }

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;

private:
    ActionIconType action_type_ = ActionIconType::UNDEFINED;
    muse::TranslatableString title_;
    muse::draw::Font icon_font_;
};
} // namespace mu::engraving
