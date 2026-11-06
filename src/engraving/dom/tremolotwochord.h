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

#include "beambase.h"
#include "engravingitem.h"

#include "durationtype.h"
#include "draw/types/painterpath.h"
#include "../types/types.h"
#include "beam.h"
#include "chord.h"

namespace mu::engraving {
struct Chord;

struct TremoloTwoChord final : BeamBase {
private:
    OBJECT_ALLOCATOR(engraving, TremoloTwoChord)
    DECLARE_CLASSOF(ElementType::TREMOLO_TWOCHORD)

public:

    TremoloTwoChord &operator=(const TremoloTwoChord &) = delete;

    TremoloTwoChord *clone() const override { return new TremoloTwoChord(*this); }

    ~TremoloTwoChord() override;

    Chord *chord() const { return to_chord(ownership_parent()); }

    int subtype() const override { return static_cast<int>(tremolo_type_); }

    TranslatableString subtype_user_name() const override;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void set_tremolo_type(TremoloType t);

    TremoloType tremolo_type() const { return tremolo_type_; }

    double min_height() const;
    void reset() override;

    PointF chord_beam_anchor(const ChordRest *chord, ChordBeamAnchorType anchor_type) const;

    double chord_mag() const;

    Chord *chord1() const { return chord1_; }

    void set_chord1(Chord *ch) { chord1_ = ch; }

    Chord *chord2() const { return chord2_; }

    void set_chord2(Chord *ch) { chord2_ = ch; }

    void set_chords(Chord *c1, Chord *c2) {
        chord1_ = c1;
        chord2_ = c2;
    }

    PairF beam_pos() const;

    TDuration duration_type() const;
    void set_duration_type(TDuration d);

    Fraction tremolo_len() const;

    int lines() const { return lines_; }

    bool cross_staff_beam_between() const;

    void trigger_layout() const override;

    TremoloStyle tremolo_style() const { return style_; }

    void set_tremolo_style(TremoloStyle v) { style_ = v; }

    void set_direction(DirectionV v) override;

    void set_beam_fragment(const BeamFragment &bf) { beam_fragment_ = bf; }

    const BeamFragment &beam_fragment() const { return beam_fragment_; }

    BeamFragment &beam_fragment() { return beam_fragment_; }

    bool play_tremolo() const { return play_tremolo_; }

    void set_play_tremolo(bool v) { play_tremolo_ = v; }

    bool custom_style_applicable() const;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;

    bool is_movable() const override { return true; }

    void clear_beam_segments() override;

    int max_cr_move() const override;
    int min_cr_move() const override;

    //! NOTE for palettes
    muse::draw::PainterPath base_path(double stretch = 0) const;

    const muse::draw::PainterPath &path() const { return path_; }

    void set_path(const muse::draw::PainterPath &p) { path_ = p; }

    void compute_shape();
    //! -----------------

private:
    friend struct Factory;

    TremoloTwoChord(DummyParentOr<Chord> parent);
    TremoloTwoChord(const TremoloTwoChord &);

    void set_beam_pos(const PairF &bp);

    TremoloType tremolo_type_ = TremoloType::INVALID_TREMOLO;
    Chord *chord1_ = nullptr;
    Chord *chord2_ = nullptr;
    TDuration duration_type_;
    bool play_tremolo_ = true;

    int lines_ = 0; // derived from _subtype
    TremoloStyle style_ = TremoloStyle::DEFAULT;
    // for _startAnchor and _slope, once we decide to change trems so that they can
    // continue from one system to the other (or indeed, one measure to the other)
    // we will want a vector of fragments similar to Beam's _beamFragments structure.
    // for now, a single fragment is sufficient
    BeamFragment beam_fragment_;

    //! NOTE for palette
    PainterPath path_;
};
} // namespace mu::engraving
