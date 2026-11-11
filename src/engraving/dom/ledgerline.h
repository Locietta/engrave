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

#ifndef MU_ENGRAVING_LEDGERLINE_H
#define MU_ENGRAVING_LEDGERLINE_H

#include "engravingitem.h"

namespace mu::engraving {
struct Chord;

//---------------------------------------------------------
//    @@ LedgerLine
///     Graphic representation of a ledger line.
//!
//!    parent:     Chord
//!    x-origin:   Chord
//!    y-origin:   SStaff
//---------------------------------------------------------

struct LedgerLine final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::LEDGER_LINE)

public:
    LedgerLine(DummyParentOr<EngravingItem>);
    ~LedgerLine();
    LedgerLine &operator=(const LedgerLine &) = delete;

    LedgerLine *clone() const override { return new LedgerLine(*this); }

    Chord *chord() const { return to_chord(ownership_parent()); }

    double len() const { return len_; }

    void set_len(double v) { len_ = v; }

    void set_vertical(bool v) { vertical_ = v; }

    bool vertical() const { return vertical_; }

    void spatium_changed(double /*oldValue*/, double /*newValue*/) override;

    struct LayoutData : public EngravingItem::LayoutData {
        double line_width = 0.0;
    };

    DECLARE_LAYOUTDATA_METHODS(LedgerLine);

private:

    double len_ = 0.0;
    bool vertical_ = false;
};
} // namespace mu::engraving
#endif
