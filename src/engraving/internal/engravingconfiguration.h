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

#include "engraving/types/types.h"
#include "io/path.h"
#include "types/string.h"

namespace mu::engraving {
struct EngravingConfiguration {
    struct DebuggingOptions {
        bool show_element_bounding_rects = false;
        bool color_element_shapes = false;
        bool show_segment_shapes = false;
        bool color_segment_shapes = false;
        bool show_skylines = false;
        bool show_system_bounding_rects = false;
        bool show_element_masks = false;
        bool show_line_attach_points = false;
        bool mark_empty_staff_visibility_overrides = false;
        bool mark_corrupted_measures = true;
        bool show_gap_rests = false;
        bool show_origin_and_combined_staves = false;

        bool any_enabled() const {
            return show_element_bounding_rects || color_element_shapes || show_segment_shapes || color_segment_shapes || show_skylines ||
                   show_system_bounding_rects || show_element_masks || show_line_attach_points || mark_empty_staff_visibility_overrides ||
                   mark_corrupted_measures || show_gap_rests || show_origin_and_combined_staves;
        }
    };

    bool read_external_images = false;

    EngravingConfiguration();

    void init();

    muse::io::PathT app_data_path() const;

    muse::io::PathT default_style_file_path() const;
    void set_default_style_file_path(const muse::io::PathT &path);

    muse::io::PathT part_style_file_path() const;
    void set_part_style_file_path(const muse::io::PathT &path);

    SizeF default_page_size() const;

    bool can_layout_icons() const;
    String icons_font_family() const;

    Color default_color() const;

    Color displayed_default_color(bool inverted) const;
    void set_displayed_default_color(Color color, bool inverted);
    void reset_displayed_default_colors();

    Color indicator_icon_inverted_selection_color() const;
    Color warning_color() const;
    Color warning_selected_color() const;
    Color critical_color() const;
    Color critical_background_color() const;
    Color critical_selected_color() const;
    Color thumbnail_background_color() const;
    Color note_background_color() const;
    Color font_primary_color() const;
    Color voice_color(voice_idx_t voice_idx) const;

    Color selection_color(voice_idx_t voice_index = 0, bool item_visible = true, bool item_is_unlinked_from_score = false) const;
    void set_selection_color(voice_idx_t voice_index, Color color);

    Color highlight_selection_color(voice_idx_t voice = 0) const;

    bool dynamics_apply_to_all_voices() const;
    void set_dynamics_apply_to_all_voices(bool v);

    bool auto_update_fretboard_diagrams() const;
    void set_auto_update_fretboard_diagrams(bool v);

    Color formatting_color() const;

    Color frame_color() const;

    Color score_grey_color() const;

    Color invisible_color() const;

    Color unlinked_color() const;

    const DebuggingOptions &debugging_options() const;
    void set_debugging_options(const DebuggingOptions &options);

    bool allow_reading_images_from_outside_mscz() const;

    bool guitar_pro_import_experimental() const;
    bool negative_frets_allowed() const;
    void set_guitar_pro_multivoice_enabled(bool multi_voice);
    bool guitar_pro_multivoice_enabled() const;
    bool min_distance_for_partial_skyline_calculated() const;
    bool specific_slurs_layout_workaround() const;
    bool prefer_same_string_for_transpose() const;
    void set_prefer_same_string_for_transpose(bool prefer_same_string);
    bool keep_dead_notes_unchanged_on_transpose() const;

private:

    DebuggingOptions debugging_options_;

    Color cached_displayed_default_color_;
    Color cached_inverted_displayed_default_color_;

    bool multi_voice_ = false;
};
} // namespace mu::engraving
