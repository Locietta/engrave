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

#ifndef MU_ENGRAVING_TEXT_H
#define MU_ENGRAVING_TEXT_H

#include "textbase.h"

namespace mu::engraving {
//---------------------------------------------------------
//   Text
//---------------------------------------------------------

struct Text final : TextBase {
private:
    OBJECT_ALLOCATOR(engraving, Text)
    DECLARE_CLASSOF(ElementType::TEXT)

public:
    Text(DummyParentOr<EngravingItem> parent, TextStyleType tid = TextStyleType::DEFAULT);

    Text *clone() const override { return new Text(*this); }

    EngravingObject *property_delegate(Pid) const override;
    PropertyValue property_default(Pid id) const override;
    PropertyValue get_property(Pid id) const override;

    static String read_xml_text(XmlReader &r, Score *score);

    void set_layout_to_parent_width(bool v) { layout_to_parent_width_ = v; }

    bool has_voice_assignment_properties() const override;
    VoiceAssignment voice_assignment() const;

    bool collect_for_drawing() const override;

    bool position_relative_to_notehead_rest() const override;
};
} // namespace mu::engraving

#endif
