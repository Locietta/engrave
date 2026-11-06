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
struct IEngravingConfiguration {
    virtual ~IEngravingConfiguration() = default;

    virtual muse::io::PathT app_data_path() const = 0;

    virtual muse::io::PathT default_style_file_path() const = 0;
    virtual void set_default_style_file_path(const muse::io::PathT &path) = 0;

    virtual muse::io::PathT part_style_file_path() const = 0;
    virtual void set_part_style_file_path(const muse::io::PathT &path) = 0;

    virtual SizeF default_page_size() const = 0;

    virtual bool can_layout_icons() const = 0;
    virtual String icons_font_family() const = 0;

    /* The default used for engraving items' actual color (saved and printed).
     * Can be overridden by changing the color property or style. */
    virtual Color default_color() const = 0;

    /* The color that will be used to display the engraving items that have the
     * default color. Affects the appearance of the elements in the score editor,
     * but it's not printed nor saved. */
    virtual Color displayed_default_color(bool inverted) const = 0;
    virtual void set_displayed_default_color(Color color, bool inverted) = 0;
    virtual void reset_displayed_default_colors() = 0;

    virtual Color indicator_icon_inverted_selection_color() const = 0;
    virtual Color lasso_color() const = 0;
    virtual Color warning_color() const = 0;
    virtual Color warning_selected_color() const = 0;
    virtual Color critical_color() const = 0;
    virtual Color critical_background_color() const = 0;
    virtual Color critical_selected_color() const = 0;
    virtual Color thumbnail_background_color() const = 0;
    virtual Color note_background_color() const = 0;
    virtual Color font_primary_color() const = 0;
    virtual Color voice_color(voice_idx_t voice_idx) const = 0;

    virtual Color selection_color(voice_idx_t voice_index = 0, bool item_visible = true,
                                  bool item_is_unlinked_from_score = false) const = 0;
    virtual void set_selection_color(voice_idx_t voice_index, Color color) = 0;

    virtual bool dynamics_apply_to_all_voices() const = 0;
    virtual void set_dynamics_apply_to_all_voices(bool v) = 0;

    virtual bool auto_update_fretboard_diagrams() const = 0;
    virtual void set_auto_update_fretboard_diagrams(bool v) = 0;

    virtual Color formatting_color() const = 0;

    virtual Color invisible_color() const = 0;

    virtual Color unlinked_color() const = 0;

    virtual Color frame_color() const = 0;

    virtual Color score_grey_color() const = 0;

    virtual Color highlight_selection_color(voice_idx_t voice_index = 0) const = 0;

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

    virtual const DebuggingOptions &debugging_options() const = 0;
    virtual void set_debugging_options(const DebuggingOptions &options) = 0;

    virtual bool allow_reading_images_from_outside_mscz() const = 0;

    /// these configurations will be removed after solving https://github.com/musescore/MuseScore/issues/14294
    virtual bool guitar_pro_import_experimental() const = 0;
    virtual bool negative_frets_allowed() const = 0;
    virtual void set_guitar_pro_multivoice_enabled(bool multi_voice) = 0;
    virtual bool guitar_pro_multivoice_enabled() const = 0;
    virtual bool min_distance_for_partial_skyline_calculated() const = 0;
    virtual bool specific_slurs_layout_workaround() const = 0;
    virtual bool prefer_same_string_for_transpose() const = 0;
    virtual void set_prefer_same_string_for_transpose(bool prefer_same_string) = 0;
    virtual bool keep_dead_notes_unchanged_on_transpose() const = 0;
};
} // namespace mu::engraving
