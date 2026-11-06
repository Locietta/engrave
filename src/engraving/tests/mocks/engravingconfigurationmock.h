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

#include <gmock/gmock.h>

#include "engraving/iengravingconfiguration.h"

namespace mu::engraving {
class EngravingConfigurationMock : public IEngravingConfiguration {
public:
    MOCK_METHOD(muse::io::PathT, app_data_path, (), (const, override));

    MOCK_METHOD(muse::io::PathT, default_style_file_path, (), (const, override));
    MOCK_METHOD(void, set_default_style_file_path, (const muse::io::PathT &), (override));

    MOCK_METHOD(muse::io::PathT, part_style_file_path, (), (const, override));
    MOCK_METHOD(void, set_part_style_file_path, (const muse::io::PathT &), (override));

    MOCK_METHOD(SizeF, default_page_size, (), (const, override));

    MOCK_METHOD(bool, can_layout_icons, (), (const, override));
    MOCK_METHOD(String, icons_font_family, (), (const, override));

    MOCK_METHOD(Color, default_color, (), (const, override));
    MOCK_METHOD(Color, displayed_default_color, (bool), (const, override));
    MOCK_METHOD(void, set_displayed_default_color, (Color, bool), (override));
    MOCK_METHOD(void, reset_displayed_default_colors, (), (override));

    MOCK_METHOD(Color, indicator_icon_inverted_selection_color, (), (const, override));
    MOCK_METHOD(Color, lasso_color, (), (const, override));
    MOCK_METHOD(Color, warning_color, (), (const, override));
    MOCK_METHOD(Color, warning_selected_color, (), (const, override));
    MOCK_METHOD(Color, critical_color, (), (const, override));
    MOCK_METHOD(Color, critical_background_color, (), (const, override));
    MOCK_METHOD(Color, critical_selected_color, (), (const, override));
    MOCK_METHOD(Color, thumbnail_background_color, (), (const, override));
    MOCK_METHOD(Color, note_background_color, (), (const, override));
    MOCK_METHOD(Color, font_primary_color, (), (const, override));
    MOCK_METHOD(Color, voice_color, (voice_idx_t), (const, override));

    MOCK_METHOD(Color, selection_color, (engraving::voice_idx_t, bool, bool), (const, override));
    MOCK_METHOD(void, set_selection_color, (engraving::voice_idx_t, Color), (override));

    MOCK_METHOD(bool, dynamics_apply_to_all_voices, (), (const, override));
    MOCK_METHOD(void, set_dynamics_apply_to_all_voices, (bool), (override));

    MOCK_METHOD(bool, auto_update_fretboard_diagrams, (), (const, override));
    MOCK_METHOD(void, set_auto_update_fretboard_diagrams, (bool), (override));

    MOCK_METHOD(Color, formatting_color, (), (const, override));

    MOCK_METHOD(Color, frame_color, (), (const, override));

    MOCK_METHOD(Color, score_grey_color, (), (const, override));

    MOCK_METHOD(Color, invisible_color, (), (const, override));

    MOCK_METHOD(Color, unlinked_color, (), (const, override));

    MOCK_METHOD(Color, highlight_selection_color, (engraving::voice_idx_t), (const, override));

    MOCK_METHOD(const DebuggingOptions &, debugging_options, (), (const, override));
    MOCK_METHOD(void, set_debugging_options, (const DebuggingOptions &), (override));

    MOCK_METHOD(bool, allow_reading_images_from_outside_mscz, (), (const, override));

    MOCK_METHOD(bool, guitar_pro_import_experimental, (), (const, override));
    MOCK_METHOD(bool, negative_frets_allowed, (), (const, override));
    MOCK_METHOD(void, set_guitar_pro_multivoice_enabled, (bool), (override));
    MOCK_METHOD(bool, guitar_pro_multivoice_enabled, (), (const, override));
    MOCK_METHOD(bool, min_distance_for_partial_skyline_calculated, (), (const, override));
    MOCK_METHOD(bool, specific_slurs_layout_workaround, (), (const, override));
    MOCK_METHOD(bool, prefer_same_string_for_transpose, (), (const, override));
    MOCK_METHOD(void, set_prefer_same_string_for_transpose, (bool), (override));
    MOCK_METHOD(bool, keep_dead_notes_unchanged_on_transpose, (), (const, override));
};
} // namespace mu::engraving
