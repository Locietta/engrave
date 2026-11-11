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

#include "../iengravingconfiguration.h"

namespace mu::engraving {
struct EngravingConfiguration : IEngravingConfiguration {
    EngravingConfiguration();

    void init();

    muse::io::PathT app_data_path() const override;

    muse::io::PathT default_style_file_path() const override;
    void set_default_style_file_path(const muse::io::PathT &path) override;

    muse::io::PathT part_style_file_path() const override;
    void set_part_style_file_path(const muse::io::PathT &path) override;

    SizeF default_page_size() const override;

    bool can_layout_icons() const override;
    String icons_font_family() const override;

    Color default_color() const override;

    Color displayed_default_color(bool inverted) const override;
    void set_displayed_default_color(Color color, bool inverted) override;
    void reset_displayed_default_colors() override;

    Color indicator_icon_inverted_selection_color() const override;
    Color warning_color() const override;
    Color warning_selected_color() const override;
    Color critical_color() const override;
    Color critical_background_color() const override;
    Color critical_selected_color() const override;
    Color thumbnail_background_color() const override;
    Color note_background_color() const override;
    Color font_primary_color() const override;
    Color voice_color(voice_idx_t voice_idx) const override;

    Color selection_color(voice_idx_t voice_index = 0, bool item_visible = true, bool item_is_unlinked_from_score = false) const override;
    void set_selection_color(voice_idx_t voice_index, Color color) override;

    Color highlight_selection_color(voice_idx_t voice = 0) const override;

    bool dynamics_apply_to_all_voices() const override;
    void set_dynamics_apply_to_all_voices(bool v) override;

    bool auto_update_fretboard_diagrams() const override;
    void set_auto_update_fretboard_diagrams(bool v) override;

    Color formatting_color() const override;

    Color frame_color() const override;

    Color score_grey_color() const override;

    Color invisible_color() const override;

    Color unlinked_color() const override;

    const DebuggingOptions &debugging_options() const override;
    void set_debugging_options(const DebuggingOptions &options) override;

    bool allow_reading_images_from_outside_mscz() const override;

    bool guitar_pro_import_experimental() const override;
    bool negative_frets_allowed() const override;
    void set_guitar_pro_multivoice_enabled(bool multi_voice) override;
    bool guitar_pro_multivoice_enabled() const override;
    bool min_distance_for_partial_skyline_calculated() const override;
    bool specific_slurs_layout_workaround() const override;
    bool prefer_same_string_for_transpose() const override;
    void set_prefer_same_string_for_transpose(bool prefer_same_string) override;
    bool keep_dead_notes_unchanged_on_transpose() const override;

private:

    DebuggingOptions debugging_options_;

    Color cached_displayed_default_color_;
    Color cached_inverted_displayed_default_color_;

    bool multi_voice_ = false;
};
} // namespace mu::engraving
