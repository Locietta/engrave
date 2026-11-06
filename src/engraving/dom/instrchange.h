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

#ifndef MU_ENGRAVING_INSTRCHANGE_H
#define MU_ENGRAVING_INSTRCHANGE_H

#include "instrument.h"
#include "textbase.h"

namespace mu::engraving {
struct Clef;

//---------------------------------------------------------
//   @@ InstrumentChange
//---------------------------------------------------------

struct InstrumentChange final : TextBase {
private:
    OBJECT_ALLOCATOR(engraving, InstrumentChange)
    DECLARE_CLASSOF(ElementType::INSTRUMENT_CHANGE)

public:
    InstrumentChange(EngravingObject *parent);
    InstrumentChange(const Instrument &, EngravingObject *parent);
    InstrumentChange(const InstrumentChange &);
    ~InstrumentChange();

    InstrumentChange *clone() const override { return new InstrumentChange(*this); }

    Instrument *instrument() const { return instrument_; }

    void set_instrument(Instrument *i) { instrument_ = i; }

    void set_instrument(Instrument &&i) { *instrument_ = i; }

    void set_instrument(const Instrument &i);
    void setup_instrument(const Instrument *instrument, bool update_text = true);

    std::vector<KeySig *> key_sigs(bool all = false) const;
    std::vector<Clef *> clefs() const;

    bool init() const { return init_; }

    void set_init(bool init) { init_ = init; }

    Segment *segment() const { return to_segment(ownership_parent()); }

    PropertyValue property_default(Pid) const override;

    bool position_relative_to_notehead_rest() const override { return true; }

    bool place_multiple() const override { return false; }

private:

    Instrument *instrument_ = nullptr; // Staff holds ownership if part of score
    bool init_ = false;                // Set if the instrument has been set by the user, as there is no other way to tell.
};
} // namespace mu::engraving
#endif
