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

#include "../types/types.h"
#include "textbase.h"

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

struct Jump final : TextBase {
private:
    DECLARE_CLASSOF(ElementType::JUMP)

public:

    Jump(DummyParentOr<Measure> parent);

    void set_jump_type(JumpType t);
    JumpType jump_type() const;
    String jump_type_user_name() const;

    Jump *clone() const override { return new Jump(*this); }

    int subtype() const override { return int(jump_type()); }

    TranslatableString subtype_user_name() const override;

    Measure *measure() const { return to_measure(ownership_parent()); }

    String jump_to() const { return jump_to_; }

    String play_until() const { return play_until_; }

    String continue_at() const { return continue_at_; }

    void set_jump_to(const String &s) { jump_to_ = s; }

    void set_play_until(const String &s) { play_until_ = s; }

    void set_continue_at(const String &s) { continue_at_ = s; }

    bool play_repeats() const { return play_repeats_; }

    void set_play_repeats(bool val) { play_repeats_ = val; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    bool position_relative_to_notehead_rest() const override { return false; }

private:
    String jump_to_;
    String play_until_;
    String continue_at_;
    bool play_repeats_ = false;
};

struct JumpTypeTableItem {
    JumpType type;
    AsciiStringView text;
    AsciiStringView jump_to;
    AsciiStringView play_until;
    AsciiStringView continue_at;
};

extern const std::vector<JumpTypeTableItem> jump_type_table;
} // namespace mu::engraving

#endif
