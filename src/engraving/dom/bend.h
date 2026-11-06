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
#include "draw/types/font.h"
#include "engravingitem.h"

namespace mu::engraving {
struct Note;
struct Factory;

//---------------------------------------------------------
//   @@ Bend
//---------------------------------------------------------

/**********************************************************
 *    OBSOLETE CLASS
 *    Used to represent bends before version 4.2. Now
 *    replaced by the GuitarBend class.
 *********************************************************/

enum class BendType : unsigned char { BEND = 0, BEND_RELEASE, BEND_RELEASE_BEND, PREBEND, PREBEND_RELEASE, CUSTOM };

struct Bend final : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, Bend)
    DECLARE_CLASSOF(ElementType::BEND)

public:
    Bend *clone() const override { return new Bend(*this); }

    Note *note() const { return to_note(ownership_parent()); }

    static const char *label[13];

    PitchValues &points() { return points_; }

    const PitchValues &points() const { return points_; }

    void add_point(const PitchValue &pv) { points_.push_back(pv); }

    void set_points(const PitchValues &p) { points_ = p; }

    bool play_bend() const { return play_bend_; }

    void set_play_bend(bool v) { play_bend_ = v; }

    String font_face() const { return font_face_; }

    void set_font_face(String f) { font_face_ = f; }

    double font_size() const { return font_size_; }

    void set_font_size(double s) { font_size_ = s; }

    FontStyle font_style() const { return font_style_; }

    void set_font_style(FontStyle s) { font_style_ = s; }

    Spatium line_width() const { return line_width_; }

    void set_line_width(Spatium w) { line_width_ = w; }

    muse::draw::Font font(double) const;

    // property methods
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    struct LayoutData : public EngravingItem::LayoutData {
        // cache
        PointF note_pos;
        double note_width = 0.0;
        double note_height = 0.0;
    };
    DECLARE_LAYOUTDATA_METHODS(Bend)

private:
    friend struct Factory;

    Bend(DummyParentOr<Note> parent);

    BendType parse_bend_type_from_curve() const;
    void update_points_by_bend_type(const BendType bend_type);

    bool play_bend_ = true;
    PitchValues points_;

    String font_face_;
    double font_size_ = 0.0;
    FontStyle font_style_ = FontStyle::UNDEFINED;
    Spatium line_width_;
};
} // namespace mu::engraving
