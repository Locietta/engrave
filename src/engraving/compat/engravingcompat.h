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

namespace mu::engraving {
struct MasterScore;
struct EngravingItem;
struct System;
} // namespace mu::engraving

namespace mu::engraving::compat {
struct EngravingCompat {
    static void do_pre_layout_compat_if_needed(MasterScore *score);
    static void do_post_layout_compat_if_needed(MasterScore *score);

private:
    static void correct_pedal_end_points(MasterScore *score);
    static void undo_staff_text_exclude_from_part(MasterScore *master_score);
    static void migrate_dynamic_pos_on_vocal_staves(MasterScore *master_score);
    static void reset_ineffective_center_between_staves(MasterScore *master_score);
    static void reset_marker_left_font_size(MasterScore *master_score);
    static void reset_rest_vertical_offsets(MasterScore *master_score);
    static void adjust_v_box_distances(MasterScore *master_score);
    static void pre470_text_compat(MasterScore *master_score);
    static void migrate_note_parens(MasterScore *master_score);
    static void migrate_offset500(MasterScore *master_score);
    static void migrate_offset_after_autoplace(MasterScore *master_score);

    static bool relayout_user_modified_cross_staff_beams(MasterScore *master_score);
    static bool set_lyric_line_visibility(MasterScore *master_score);

    static void reset_all_elements_positions(MasterScore *master_score);
    static void reset_all_cross_beams(MasterScore *master_score);

    static void apply_pre400_style_compat(MasterScore *master_score);
};

struct AlignmentMigration500 {
    static void migrate_snapped_and_same_item_type_alignment(MasterScore *master_score);
    static void migrate_hopo_letter_alignment(MasterScore *master_score);
private:
    static bool row_item_is_above(const EngravingItem *item);
    static void align_item_offset_group(const std::vector<EngravingItem *> &group);
};
} // namespace mu::engraving::compat
