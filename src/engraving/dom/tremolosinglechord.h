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

#include "../types/types.h"
#include "beam.h"
#include "chord.h"
#include "draw/types/painterpath.h"
#include "durationtype.h"
#include "engravingitem.h"

namespace mu::engraving {
struct Chord;

struct TremoloSingleChord final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::TREMOLO_SINGLECHORD)

public:

    TremoloSingleChord &operator=(const TremoloSingleChord &) = delete;

    TremoloSingleChord *clone() const override { return new TremoloSingleChord(*this); }

    ~TremoloSingleChord() override;

    Chord *chord() const { return to_chord(ownership_parent()); }

    int subtype() const override { return static_cast<int>(tremolo_type_); }

    TranslatableString subtype_user_name() const override;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void set_tremolo_type(TremoloType t);

    TremoloType tremolo_type() const { return tremolo_type_; }

    double min_height() const;

    double chord_mag() const;

    TDuration duration_type() const;
    void set_duration_type(TDuration d);

    Fraction tremolo_len() const;

    bool is_buzz_roll() const { return tremolo_type_ == TremoloType::BUZZ_ROLL; }

    int lines() const { return lines_; }

    void spatium_changed(double old_value, double new_value) override;
    void local_spatium_changed(double old_value, double new_value) override;
    void style_changed() override;
    staff_idx_t v_staff_idx() const override;

    bool play_tremolo() const { return play_tremolo_; }

    void set_play_tremolo(bool v) { play_tremolo_ = v; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;

    bool is_movable() const override { return true; }

    muse::draw::PainterPath base_path(double stretch = 0) const;

    const muse::draw::PainterPath &path() const { return path_; }

    void set_path(const muse::draw::PainterPath &p) { path_ = p; }

    void compute_shape();

private:
    friend struct Factory;

    TremoloSingleChord(DummyParentOr<Chord> parent);
    TremoloSingleChord(const TremoloSingleChord &);

    TremoloType tremolo_type_ = TremoloType::INVALID_TREMOLO;
    TDuration duration_type_;
    muse::draw::PainterPath path_;
    bool play_tremolo_ = true;

    int lines_ = 0; // derived from _subtype
};
} // namespace mu::engraving
