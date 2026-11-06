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

#include "accidental.h"
#include "durationtype.h"
#include "../types/types.h"

namespace mu::engraving {
//---------------------------------------------------------
//   ShadowNote
//---------------------------------------------------------

/**
 Graphic representation of a shadow note,
 which shows the note insert position in note entry mode.
*/

class ShadowNote final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, ShadowNote)
    DECLARE_CLASSOF(ElementType::SHADOW_NOTE)

public:
    ShadowNote(Score *);

    ShadowNote *clone() const override { return new ShadowNote(*this); }

    bool is_valid() const;

    Fraction tick() const override { return tick_; }

    void set_tick(Fraction t) { tick_ = t; }

    int line_index() const { return line_index_; }

    void set_line_index(int n) { line_index_ = n; }

    bool is_rest() const { return is_rest_; }

    const TDuration &duration() const { return duration_; }

    void set_state(SymId note_symbol, TDuration duration, bool is_rest, bool beyond_score,
                   AccidentalType accidental_type = AccidentalType::NONE, const std::set<SymId> &articulation_ids = {});

    void draw_articulations(muse::draw::Painter *painter) const;
    void draw_close_articulation(muse::draw::Painter *painter, const SymId &articulation, RectF &bound_rect, bool up,
                                 const SymId &prev_articulation) const;
    void draw_far_articulation(muse::draw::Painter *painter, const SymId &articulation, RectF &bound_rect, bool up,
                               bool accent_staccato_kern) const;

    bool compute_up() const;

    SymId notehead_symbol() const { return notehead_symbol_; }

    bool has_stem() const;
    bool has_flag() const;
    SymId flag_sym() const;
    AccidentalType accidental_type() const;
    const std::set<SymId> &articulation_ids() const;

    bool ledger_lines_visible() const;

    bool is_beyond_score() const { return beyond_score_; }

private:

    Fraction tick_;
    int line_index_ = -1;
    SymId notehead_symbol_ = SymId::NO_SYM;
    TDuration duration_;
    bool is_rest_ = false;
    AccidentalType accidental_type_ = AccidentalType::NONE;
    std::set<SymId> articulation_ids_;
    bool beyond_score_ = false;
};
} // namespace mu::engraving
