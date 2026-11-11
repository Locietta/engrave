/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "editposition.h"

#include "../dom/engravingitem.h"
#include "engraving/rendering/score/scorerenderer.h"

using namespace mu::engraving;

void EditPosition::freeze_items_positions(Transaction &tx, const std::vector<EngravingItem *> &items) {
    for (EngravingItem *item : items) {
        freeze_item_position(tx, item);
    }
}

void EditPosition::freeze_item_position(Transaction &, EngravingItem *item) {
    if (item->generated() || !autoplace_applies_to_type(item->type()) || !item->autoplace()) {
        return;
    }

    PointF old_pos = item->pos();

    PropertyFlags autoplace_pf = item->property_flags(Pid::AUTOPLACE);
    if (autoplace_pf == PropertyFlags::STYLED) {
        autoplace_pf = PropertyFlags::UNSTYLED;
    }

    item->undo_change_property(Pid::AUTOPLACE, false, autoplace_pf);

    item->renderer()->layout_item(item);

    PropertyFlags offset_pf = item->property_flags(Pid::OFFSET);
    if (offset_pf == PropertyFlags::STYLED) {
        offset_pf = PropertyFlags::UNSTYLED;
    }

    item->undo_change_property(Pid::OFFSET, old_pos - item->ldata()->pos(), offset_pf);
}
