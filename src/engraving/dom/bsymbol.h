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

#include <vector>

#include "engravingitem.h"

namespace mu::engraving {
struct Transaction;

//---------------------------------------------------------
//   @@ BSymbol
///    base class for Symbol and Image
//---------------------------------------------------------

struct BSymbol : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, BSymbol)
public:

    Segment *segment() const { return (Segment *) ownership_parent(); }

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    BSymbol &operator=(const BSymbol &) = delete;

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;
    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    Align align() const { return align_; }

    void set_align(Align a) { align_ = a; }

    const std::vector<EngravingItem *> &leafs() const { return leafs_; }

    PointF page_pos() const override;
    PointF canvas_pos() const override;

protected:
    BSymbol(const ElementType &type, EngravingObject *parent, ElementFlags f = ElementFlag::NOTHING);
    BSymbol(const BSymbol &);

private:

    std::vector<EngravingItem *> leafs_;
    Align align_ = {AlignH::LEFT, AlignV::BASELINE};
};
} // namespace mu::engraving
