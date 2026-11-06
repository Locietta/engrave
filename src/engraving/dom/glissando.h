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
#include "line.h"
#include "property.h"
#include "../types/types.h"

namespace mu::engraving {
struct Glissando;
struct Note;
enum class GlissandoType : unsigned char;

//---------------------------------------------------------
//   @@ GlissandoSegment
//---------------------------------------------------------

struct GlissandoSegment final : LineSegment {
private:
    OBJECT_ALLOCATOR(engraving, GlissandoSegment)
    DECLARE_CLASSOF(ElementType::GLISSANDO_SEGMENT)

public:
    GlissandoSegment(Glissando *sp);

    Glissando *glissando() const { return to_glissando(spanner()); }

    GlissandoSegment *clone() const override { return new GlissandoSegment(*this); }

    EngravingObject *property_delegate(Pid) const override;
};

//---------------------------------------------------------
//   Glissando
//---------------------------------------------------------

struct Glissando final : SLine {
private:
    OBJECT_ALLOCATOR(engraving, Glissando)
    DECLARE_CLASSOF(ElementType::GLISSANDO)

    M_PROPERTY(String, text, set_text)
    M_PROPERTY(GlissandoType, glissando_type, set_glissando_type)
    M_PROPERTY(GlissandoStyle, glissando_style, set_glissando_style)
    M_PROPERTY(bool, glissando_shift, set_glissando_shift)
    M_PROPERTY(String, font_face, set_font_face)
    M_PROPERTY(double, font_size, set_font_size)
    M_PROPERTY(bool, show_text, set_show_text)
    M_PROPERTY(FontStyle, font_style, set_font_style)
    M_PROPERTY(int, ease_in, set_ease_in)
    M_PROPERTY(int, ease_out, set_ease_out)

public:
    static constexpr double gliss_palette_width = 4.0;
    static constexpr double gliss_palette_height = 4.0;

    Glissando(DummyParentOr<EngravingItem> parent);
    Glissando(const Glissando &);

    Anchor anchor() const override { return Anchor::NOTE; }

    static Note *guess_initial_note(Chord *chord);

    std::optional<bool> is_harp_gliss() const { return is_harp_gliss_; }

    void set_is_harp_gliss(std::optional<bool> v) { is_harp_gliss_ = v; }

    // overridden inherited methods
    Glissando *clone() const override { return new Glissando(*this); }

    LineSegment *create_line_segment() override;

    bool allow_time_anchor() const override { return false; }

    // property/style methods
    Sid get_property_style(Pid id) const override;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    TranslatableString subtype_user_name() const override;

    static bool pitch_steps(const Spanner *spanner, std::vector<int> &pitch_offsets);

protected:
    bool is_in_spanner_map() const override { return false; }

private:

    std::optional<bool> is_harp_gliss_ = std::nullopt;
};
} // namespace mu::engraving
