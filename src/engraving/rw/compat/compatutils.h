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
#include <set>

#include "../../dom/articulation.h"

namespace mu::engraving {
struct Dynamic;
struct Excerpt;
struct HarmonyInfo;
struct MasterScore;
struct Score;
enum class DynamicType : unsigned char;
enum class SymId;
} // namespace mu::engraving

namespace mu::engraving::compat {
struct CompatUtils {
    static void assign_initial_part_to_excerpts(const std::vector<Excerpt *> &excerpts);
    static void do_compatibility_conversions(MasterScore *master_score);
    static ArticulationAnchor translate_to_new_articulation_anchor(int anchor);
    static double convert_chord_ext_mod_units(double val);
    static void set_harmony_root_tpc_from_function(HarmonyInfo *info, const Harmony *h, const muse::String &s);
    static const std::set<SymId> ornament_ids;
    static const std::map<Sid, Sid> align_vals_to_convert;
    static void set_position_styles_from_align(MStyle *style, std::vector<Sid> ignore_sids = {});
    static void set_text_line_text_position_from_align(TextLineBase *tl);
    static void reset_hook_height_sign(TextLineBase *tl);
    static void set_music_symbol_size470(MStyle &style);
    static Spatium convert_pre470_frame_radius(double frame_radius);
    static void convert_pre470_image_size(Image *image);
    static void do_migrate_note_parens(EngravingItem *item);
    static PointF get_adjusted_offset(EngravingItem *item, PointF offset);
    static void migrate_offset500(EngravingItem *item, PropertyValue &offset);
    static void migrate_offset_pre302(EngravingItem *item, int msc_version);

private:
    static Sid position_style_from_align(Sid align);
    static void replace_staff_text_with_play_technique_annotation(MasterScore *score);
    static void replace_old_with_new_ornaments(MasterScore *score);
    static void replace_old_with_new_expressions(MasterScore *score);
    static void reconstruct_type_of_custom_dynamics(MasterScore *score);
    static void split_articulations(MasterScore *score);
    static DynamicType reconstruct_dynamic_type_from_string(Dynamic *dynamic);
    static void reset_rest_vertical_offset(MasterScore *master_score);
    static void reset_articulation_offsets(MasterScore *master_score);
    static void reset_stem_lengths_for_two_note_trems(MasterScore *master_score);
    static void replace_staff_text_with_capo(MasterScore *master_score);
    static void add_missing_init_key_for_transposing_instrument(MasterScore *score);
    static void reset_frames_exclusion_from_parts(MasterScore *master_score);
    static void map_header_footer_styles(MasterScore *master_score);
    static NoteLine *create_note_line_from_text_line(TextLine *text_line);
    static void convert_text_line_to_note_anchored_line(MasterScore *master_score);
    static void convert_laissez_vib_artic_to_tie(MasterScore *master_score);
    static void remove_mm_rest_elements(MasterScore *master_score);
};
} // namespace mu::engraving::compat
