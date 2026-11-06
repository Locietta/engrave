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

#ifndef MU_ENGRAVING_FERMATA_H
#define MU_ENGRAVING_FERMATA_H

#include "../types/types.h"
#include "engravingitem.h"

namespace mu::engraving {
struct ChordRest;
struct Factory;
struct Measure;
struct Page;
struct Segment;
struct System;

//---------------------------------------------------------
//    Fermata
//---------------------------------------------------------

struct Fermata final : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, Fermata)
    DECLARE_CLASSOF(ElementType::FERMATA)

public:

    Fermata(const Fermata &) = default;
    Fermata &operator=(const Fermata &) = delete;

    Fermata *clone() const override { return new Fermata(*this); }

    double mag() const override;

    SymId sym_id() const { return sym_id_; }

    void set_sym_id(SymId id) { sym_id_ = id; }

    void set_sym_id_and_time_stretch(SymId id);
    FermataType fermata_type() const;
    int subtype() const override;
    TranslatableString subtype_user_name() const override;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;
    void reset_property(Pid id) override;

    Segment *segment() const { return to_segment(ownership_parent()); }

    Measure *measure() const;
    System *system() const;
    Page *page() const;

    double time_stretch() const { return time_stretch_; }

    void set_time_stretch(double val) { time_stretch_ = val; }

    bool play() const { return play_; }

    void set_play(bool val) { play_ = val; }

    Sid default_pos_sid() const override;

private:

    friend struct Factory;
    Fermata(EngravingObject *parent);

    SymId sym_id_ = SymId::NO_SYM;
    double time_stretch_ = -1.0;
    bool play_ = true;
};
} // namespace mu::engraving
#endif
