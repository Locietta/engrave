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

#ifndef MU_ENGRAVING_REHEARSALMARK_H
#define MU_ENGRAVING_REHEARSALMARK_H

#include "textbase.h"

namespace mu::engraving {
//---------------------------------------------------------
//   @@ RehearsalMark
//---------------------------------------------------------

struct RehearsalMark final : TextBase {
private:
    OBJECT_ALLOCATOR(engraving, RehearsalMark)
    DECLARE_CLASSOF(ElementType::REHEARSAL_MARK)

public:
    enum class Type : unsigned char { MAIN = 0, ADDITIONAL };

    RehearsalMark(DummyParentOr<Segment> parent);

    bool allow_time_anchor() const override { return false; }

    RehearsalMark *clone() const override { return new RehearsalMark(*this); }

    Segment *segment() const { return (Segment *) ownership_parent(); }

    PropertyValue property_default(Pid id) const override;

    void set_type(Type type);

    Type type() const { return type_; }

    void style_changed() override;

    bool position_relative_to_notehead_rest() const override { return false; }

private:
    void apply_type_style();

    Type type_ = Type::MAIN;
};
} // namespace mu::engraving
#endif
