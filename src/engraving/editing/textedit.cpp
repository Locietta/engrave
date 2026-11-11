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

#include "textedit.h"

#include "../dom/fret.h"
#include "../dom/lyrics.h"
#include "../dom/masterscore.h"
#include "../dom/score.h"
#include "../dom/symbol.h"
#include "../dom/utils.h"
#include "../rendering/iscorerenderer.h"
#include "engraving/internal/engravingfont.h"
#include "log.h"
#include "navigation.h"
#include "transaction/transaction.h"
#include "transaction/undostack.h"
#include "types/symnames.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   ~TextEditData
//---------------------------------------------------------

TextEditData::~TextEditData() {
    if (delete_text) {
        for (EngravingObject *se : text_base_->link_list()) {
            to_text_base(se)->delete_later();
        }
    }
}

void ChangeTextProperties::restore_selection() {
    TextCursor &tc = cursor();
    tc.text()->cursor()->set_select_line(tc.select_line());
    tc.text()->cursor()->set_select_column(tc.select_column());
    tc.text()->cursor()->set_row(tc.row());
    tc.text()->cursor()->set_column(tc.column());
}

ChangeTextProperties::ChangeTextProperties(const TextCursor *tc, Pid prop_id, const PropertyValue &prop_val, PropertyFlags flags)
    : TextEditUndoableCommand(*tc) {
    property_id_ = prop_id;
    property_val_ = prop_val;
    flags_ = flags;
    if (property_id_ == Pid::FONT_STYLE) {
        existing_style_ = static_cast<FontStyle>(cursor().text()->get_property(prop_id).to_int());
    }
}

void ChangeTextProperties::undo() {
    cursor().text()->reset_formatting();
    cursor().text()->set_xml_text(xml_text_);
    restore_selection();
    cursor().text()->renderer()->layout_text1(cursor().text());
}

void ChangeTextProperties::redo() {
    xml_text_ = cursor().text()->xml_text();
    restore_selection();
    cursor().text()->set_property_flags(property_id_, flags_);

    if (property_id_ == Pid::FONT_STYLE) {
        FontStyle set_style = static_cast<FontStyle>(property_val_.to_int());
        TextCursor *tc = cursor().text()->cursor();
        // user turned on bold/italic/underline/strike for text where it's not set, or turned it off for text where it is set,
        // note this logic only works because the user can only click one at a time
        if ((set_style & FontStyle::BOLD) != (existing_style_ & FontStyle::BOLD)) {
            tc->set_format(FormatId::BOLD, set_style & FontStyle::BOLD);
        }
        if ((set_style & FontStyle::ITALIC) != (existing_style_ & FontStyle::ITALIC)) {
            tc->set_format(FormatId::ITALIC, set_style & FontStyle::ITALIC);
        }
        if ((set_style & FontStyle::UNDERLINE) != (existing_style_ & FontStyle::UNDERLINE)) {
            tc->set_format(FormatId::UNDERLINE, set_style & FontStyle::UNDERLINE);
        }
        if ((set_style & FontStyle::STRIKE) != (existing_style_ & FontStyle::STRIKE)) {
            tc->set_format(FormatId::STRIKE, set_style & FontStyle::STRIKE);
        }
    } else {
        cursor().text()->set_property(property_id_, property_val_);
    }
}
