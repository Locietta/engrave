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
#include "accidental.h"
#include "engravingitem.h"
#include "pitchspelling.h"

namespace mu::engraving {
struct Factory;

//---------------------------------------------------------
//   @@ Ambitus
//---------------------------------------------------------

struct Ambitus final : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, Ambitus)
    DECLARE_CLASSOF(ElementType::AMBITUS)

public:
    ~Ambitus();

    static constexpr NoteHeadGroup noteheadgroup_default = NoteHeadGroup::HEAD_NORMAL;
    static constexpr NoteHeadType noteheadtype_default = NoteHeadType::HEAD_AUTO;
    static constexpr DirectionH direction_default = DirectionH::AUTO;
    static constexpr bool hasline_default = true;
    static constexpr Spatium linewidth_default = 0.12_sp;
    static constexpr Spatium lineoffset_default = 0.8_sp; // the distance between notehead and line

    Ambitus *clone() const override { return new Ambitus(*this); }

    double mag() const override;

    void init_from(Ambitus *a);

    // getters and setters
    NoteHeadGroup note_head_group() const { return note_head_group_; }

    NoteHeadType note_head_type() const { return note_head_type_; }

    DirectionH direction() const { return direction_; }

    bool has_line() const { return has_line_; }

    Spatium line_width() const { return line_width_; }

    int top_octave() const { return (top_pitch_ / ::mu::engraving::pitch_delta_octave) - 1; }

    int bottom_octave() const { return (bottom_pitch_ / ::mu::engraving::pitch_delta_octave) - 1; }

    int top_pitch() const { return top_pitch_; }

    int bottom_pitch() const { return bottom_pitch_; }

    int top_tpc() const { return top_tpc_; }

    int bottom_tpc() const { return bottom_tpc_; }

    Accidental *top_accidental() const { return top_accidental_; }

    Accidental *bottom_accidental() const { return bottom_accidental_; }

    void set_note_head_group(NoteHeadGroup val) { note_head_group_ = val; }

    void set_note_head_type(NoteHeadType val) { note_head_type_ = val; }

    void set_direction(DirectionH val) { direction_ = val; }

    void set_has_line(bool val) { has_line_ = val; }

    void set_line_width(Spatium val) { line_width_ = val; }

    void set_top_pitch(int val, bool apply_logic = true);
    void set_bottom_pitch(int val, bool apply_logic = true);
    void set_top_tpc(int val, bool apply_logic = true);
    void set_bottom_tpc(int val, bool apply_logic = true);

    // some utility functions
    Segment *segment() const { return (Segment *) ownership_parent(); }

    SymId note_head() const;
    double head_width() const;

    // re-implemented virtual functions
    void scan_elements(std::function<void(EngravingItem *)> func) override;
    void set_track(track_idx_t val) override;

    void remove(EngravingItem *) override;

    // properties
    PropertyValue get_property(Pid) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    static AccidentalType accidental_type(int tpc, Key key);
    static int staff_line(int tpc, int pitch, ClefType clf);

    struct LayoutData : public EngravingItem::LayoutData {
        PointF top_pos;    // position of top note symbol
        PointF bottom_pos; // position of bottom note symbol
        LineF line;        // the drawn line
    };
    DECLARE_LAYOUTDATA_METHODS(Ambitus)

private:

    friend struct Factory;
    Ambitus(DummyParentOr<Segment> parent);
    Ambitus(const Ambitus &a);

    void normalize();

    struct Ranges {
        int top_tpc = Tpc::TPC_INVALID;
        int bottom_tpc = Tpc::TPC_INVALID;
        int top_pitch = ::mu::engraving::invalid_pitch;
        int bottom_pitch = ::mu::engraving::max_pitch + 1;
    };

    Ranges estimate_ranges() const; // scan staff up to next section break and update range pitches

    NoteHeadGroup note_head_group_ = ::mu::engraving::Ambitus::noteheadgroup_default;
    NoteHeadType note_head_type_ = ::mu::engraving::Ambitus::noteheadtype_default;
    DirectionH direction_ = ::mu::engraving::Ambitus::direction_default;
    bool has_line_ = ::mu::engraving::Ambitus::hasline_default;
    Spatium line_width_;
    Accidental *top_accidental_ = nullptr;
    Accidental *bottom_accidental_ = nullptr;
    int top_pitch_ = ::mu::engraving::invalid_pitch, bottom_pitch_ = ::mu::engraving::invalid_pitch;
    int top_tpc_ = Tpc::TPC_INVALID, bottom_tpc_ = Tpc::TPC_INVALID;
};
} // namespace mu::engraving
