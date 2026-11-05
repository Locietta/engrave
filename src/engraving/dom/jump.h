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

#ifndef MU_ENGRAVING_JUMP_H
#define MU_ENGRAVING_JUMP_H

#include "textbase.h"

#include "../types/types.h"

namespace mu::engraving {
//---------------------------------------------------------
//   @@ Jump
///    Jump label
//
//   @P continueAt  string
//   @P jumpTo      string
// not used?
//      jumpType    enum (Jump.DC, .DC_AL_FINE, .DC_AL_CODA, .DS_AL_CODA, .DS_AL_FINE, .DS, USER) (read only)
//   @P playUntil   string
//---------------------------------------------------------

class Jump final : public TextBase {
    OBJECT_ALLOCATOR(engraving, Jump)
    DECLARE_CLASSOF(ElementType::JUMP)

public:

    Jump(DummyParentOr<Measure> parent);

    void setJumpType(JumpType t);
    JumpType jumpType() const;
    String jumpTypeUserName() const;

    Jump *clone() const override { return new Jump(*this); }

    int subtype() const override { return int(jumpType()); }

    TranslatableString subtypeUserName() const override;

    Measure *measure() const { return toMeasure(ownershipParent()); }

    String jumpTo() const { return jumpTo_; }

    String playUntil() const { return playUntil_; }

    String continueAt() const { return continueAt_; }

    void setJumpTo(const String &s) { jumpTo_ = s; }

    void setPlayUntil(const String &s) { playUntil_ = s; }

    void setContinueAt(const String &s) { continueAt_ = s; }

    bool playRepeats() const { return playRepeats_; }

    void setPlayRepeats(bool val) { playRepeats_ = val; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    bool positionRelativeToNoteheadRest() const override { return false; }

private:
    String jumpTo_;
    String playUntil_;
    String continueAt_;
    bool playRepeats_ = false;
};

struct JumpTypeTableItem {
    JumpType type;
    AsciiStringView text;
    AsciiStringView jumpTo;
    AsciiStringView playUntil;
    AsciiStringView continueAt;
};

extern const std::vector<JumpTypeTableItem> jumpTypeTable;
} // namespace mu::engraving

#endif
