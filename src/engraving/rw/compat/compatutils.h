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

#include "../../dom/articulation.h"
#include "global/flatmap.h"

namespace mu::engraving {
struct MasterScore;
} // namespace mu::engraving

namespace mu::engraving::compat {
struct CompatUtils {
    static void do_compatibility_conversions(MasterScore *master_score);
    static ArticulationAnchor translate_to_new_articulation_anchor(int anchor);
    static double convert_chord_ext_mod_units(double val);
    static const muse::FlatMap<Sid, Sid> align_vals_to_convert;
    static void set_position_styles_from_align(MStyle *style, std::vector<Sid> ignore_sids = {});
    static void set_text_line_text_position_from_align(TextLineBase *tl);
    static void set_music_symbol_size470(MStyle &style);
    static Spatium convert_pre470_frame_radius(double frame_radius);
    static void convert_pre470_image_size(Image *image);
    static void do_migrate_note_parens(EngravingItem *item);
    static PointF get_adjusted_offset(EngravingItem *item, PointF offset);
    static void migrate_offset500(EngravingItem *item, PropertyValue &offset);

private:
    static Sid position_style_from_align(Sid align);
    static void remove_mm_rest_elements(MasterScore *master_score);
};
} // namespace mu::engraving::compat
