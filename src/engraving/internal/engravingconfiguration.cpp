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
#include "engravingconfiguration.h"

#include <cstdlib>

#include "global/settings.h"
#include "draw/types/color.h"
#include "dom/mscore.h"
#include "translation.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;

static Val color_value(const Color &color) { return Val(color.to_string()); }

static Color color_from_value(const Val &value) { return Color(value.to_string().c_str()); }

static const Settings::Key default_style_file_path("engraving", "engraving/style/defaultStyleFile");
static const Settings::Key part_style_file_path("engraving", "engraving/style/partStyleFile");

static const Settings::Key displayed_default_color("engraving", "engraving/colors/displayedDefaultColor");
static const Settings::Key inverted_displayed_default_color("engraving", "engraving/colors/invertedDisplayedDefaultColor");
static const Settings::Key all_voices_color("engraving", "engraving/colors/allVoicesColor");
static const Settings::Key formatting_color("engraving", "engraving/colors/formattingColor");
static const Settings::Key frame_color("engraving", "engraving/colors/frameColor");
static const Settings::Key score_grey_color("engraving", "engraving/color/scoreGreyColor");
static const Settings::Key invisible_color("engraving", "engraving/colors/invisibleColor");
static const Settings::Key unlinked_color("engraving", "engraving/colors/unlinkedColor");

static const Settings::Key dynamics_apply_to_all_voices("engraving", "score/dynamicsApplyToAllVoices");
static const Settings::Key fretboard_diagrams_auto_update("engraving", "score/fretboardDiagramsAutoUpdate");

struct VoiceColor {
    Settings::Key key;
    Color color;
};

static VoiceColor voice_colors[::mu::engraving::voices + 1];

static const Color unlinked_item_color = "#FF9300";
static const Color default_displayed_inverted_color = "#CBCBCD";

EngravingConfiguration::EngravingConfiguration()
    : cached_displayed_default_color_(Color::black), cached_inverted_displayed_default_color_(::default_displayed_inverted_color) {}

void EngravingConfiguration::init() {
    static const Color default_voice_colors[::mu::engraving::voices + 1]{
        "#0065BF", "#007F00", "#C53F00", "#C31989",
        "#6038FC", // "all voices"
    };

    settings()->set_default_value(::default_style_file_path, Val(muse::io::PathT()));

    settings()->set_default_value(::part_style_file_path, Val(muse::io::PathT()));

    settings()->set_default_value(::displayed_default_color, color_value(default_color()));
    settings()->set_description(::displayed_default_color, muse::trc("engraving", "Displayed default color"));
    settings()->set_can_be_manually_edited(::displayed_default_color, true);
    cached_displayed_default_color_ = color_from_value(settings()->value(::displayed_default_color));

    settings()->set_default_value(::inverted_displayed_default_color, color_value(Color(::default_displayed_inverted_color)));
    settings()->set_description(::inverted_displayed_default_color,
                                muse::trc("engraving", "Displayed default color when score colors are inverted"));
    settings()->set_can_be_manually_edited(::inverted_displayed_default_color, true);
    cached_inverted_displayed_default_color_ = color_from_value(settings()->value(::inverted_displayed_default_color));

    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        Settings::Key key("engraving", "engraving/colors/voice" + std::to_string(voice + 1));

        settings()->set_default_value(key, color_value(default_voice_colors[voice]));
        settings()->set_description(key, "Voice " + std::to_string(voice + 1) + " color");
        settings()->set_can_be_manually_edited(key, true);

        Color current_color = color_from_value(settings()->value(key));
        voice_colors[voice] = VoiceColor{std::move(key), current_color};
    }

    static constexpr int all_voices_idx = ::mu::engraving::voices;
    settings()->set_default_value(::all_voices_color, color_value(default_voice_colors[all_voices_idx]));
    settings()->set_description(::all_voices_color, muse::trc("engraving", "All voices color"));
    settings()->set_can_be_manually_edited(::all_voices_color, true);
    Color current_color = color_from_value(settings()->value(::all_voices_color));
    voice_colors[all_voices_idx] = VoiceColor{std::move(::all_voices_color), current_color};

    settings()->set_default_value(::dynamics_apply_to_all_voices, Val(true));

    settings()->set_default_value(::fretboard_diagrams_auto_update, Val(true));

    settings()->set_default_value(::frame_color, color_value(Color("#A0A0A4")));
    settings()->set_description(::frame_color, muse::trc("engraving", "Frame color"));
    settings()->set_can_be_manually_edited(::frame_color, true);

    settings()->set_default_value(::score_grey_color, color_value(Color("#A0A0A4")));
    settings()->set_description(::score_grey_color, muse::trc("engraving", "Score grey color"));
    settings()->set_can_be_manually_edited(::score_grey_color, false);

    settings()->set_default_value(::formatting_color, color_value(Color("#C31989")));
    settings()->set_description(::formatting_color, muse::trc("engraving", "Formatting color"));
    settings()->set_can_be_manually_edited(::formatting_color, true);

    settings()->set_default_value(::invisible_color, color_value(Color("#808080")));
    settings()->set_description(::invisible_color, muse::trc("engraving", "Invisible color"));
    settings()->set_can_be_manually_edited(::invisible_color, true);

    settings()->set_default_value(::unlinked_color, color_value(Color(::unlinked_item_color)));
    settings()->set_description(::unlinked_color, muse::trc("engraving", "Desynchronized color"));
    settings()->set_can_be_manually_edited(::unlinked_color, true);
}

muse::io::PathT EngravingConfiguration::app_data_path() const {
    // The engine has no application data directory.
    return muse::io::PathT();
}

muse::io::PathT EngravingConfiguration::default_style_file_path() const { return settings()->value(::default_style_file_path).to_path(); }

void EngravingConfiguration::set_default_style_file_path(const muse::io::PathT &path) {
    settings()->set_shared_value(::default_style_file_path, Val(path.to_std_string()));
}

muse::io::PathT EngravingConfiguration::part_style_file_path() const { return settings()->value(::part_style_file_path).to_path(); }

void EngravingConfiguration::set_part_style_file_path(const muse::io::PathT &path) {
    settings()->set_shared_value(::part_style_file_path, Val(path.to_std_string()));
}

static bool default_page_size_is_letter() {
    // try PAPERSIZE environment variable
    const char *papersize = getenv("PAPERSIZE");
    if (papersize) {
        return strcmp(papersize, "letter") == 0;
    }
    return false;
}

SizeF EngravingConfiguration::default_page_size() const {
    // Needs to be determined only once, therefore static
    static SizeF size = default_page_size_is_letter() ? SizeF(8.5, 11.0) : SizeF(210.0 / 25.4, 297.0 / 25.4);

    return size;
}

bool EngravingConfiguration::can_layout_icons() const { return false; }

muse::String EngravingConfiguration::icons_font_family() const { return String(); }

Color EngravingConfiguration::default_color() const { return Color::black; }

Color EngravingConfiguration::displayed_default_color(bool inverted) const {
    if (inverted) {
        return cached_inverted_displayed_default_color_;
    }
    return cached_displayed_default_color_;
}

void EngravingConfiguration::set_displayed_default_color(Color color, bool inverted) {
    if (inverted) {
        settings()->set_shared_value(::inverted_displayed_default_color, color_value(color));
        cached_inverted_displayed_default_color_ = color;
    } else {
        settings()->set_shared_value(::displayed_default_color, color_value(color));
        cached_displayed_default_color_ = color;
    }
}

void EngravingConfiguration::reset_displayed_default_colors() {
    settings()->set_shared_value(::displayed_default_color, settings()->default_value(::displayed_default_color));
    settings()->set_shared_value(::inverted_displayed_default_color, settings()->default_value(::inverted_displayed_default_color));
    cached_displayed_default_color_ = color_from_value(settings()->value(::displayed_default_color));
    cached_inverted_displayed_default_color_ = color_from_value(settings()->value(::inverted_displayed_default_color));
}

Color EngravingConfiguration::indicator_icon_inverted_selection_color() const {
    Color color = voice_color(0);
    color.apply_tint(0.35f);
    return color;
}

Color EngravingConfiguration::lasso_color() const { return "#00323200"; }

Color EngravingConfiguration::warning_color() const { return "#808000"; }

Color EngravingConfiguration::warning_selected_color() const { return "#565600"; }

Color EngravingConfiguration::critical_color() const { return Color::red_color; }

Color EngravingConfiguration::critical_background_color() const { return Color::red_color; }

Color EngravingConfiguration::critical_selected_color() const { return "#8B0000"; }

Color EngravingConfiguration::thumbnail_background_color() const { return Color::white; }

Color EngravingConfiguration::note_background_color() const { return Color::white; }

Color EngravingConfiguration::font_primary_color() const { return Color::black; }

Color EngravingConfiguration::voice_color(voice_idx_t voice_idx) const { return voice_colors[voice_idx].color; }

Color EngravingConfiguration::selection_color(voice_idx_t voice, bool item_visible, bool item_is_unlinked_from_score) const {
    Color color = item_is_unlinked_from_score ? unlinked_color() : voice_colors[voice].color;

    if (item_visible) {
        return color;
    }

    constexpr float tint = .6f; // Between 0 and 1. Higher means lighter, lower means darker

    color.apply_tint(tint);

    return color;
}

void EngravingConfiguration::set_selection_color(voice_idx_t voice_index, Color color) {
    settings()->set_shared_value(voice_colors[voice_index].key, color_value(color));
    voice_colors[voice_index].color = color;
}

Color EngravingConfiguration::highlight_selection_color(voice_idx_t voice) const {
    Color color = selection_color(voice);
    color.apply_tint(0.35f);
    return color;
}

bool EngravingConfiguration::dynamics_apply_to_all_voices() const { return settings()->value(::dynamics_apply_to_all_voices).to_bool(); }

void EngravingConfiguration::set_dynamics_apply_to_all_voices(bool v) {
    settings()->set_shared_value(::dynamics_apply_to_all_voices, Val(v));
}

bool EngravingConfiguration::auto_update_fretboard_diagrams() const {
    return settings()->value(::fretboard_diagrams_auto_update).to_bool();
}

void EngravingConfiguration::set_auto_update_fretboard_diagrams(bool v) {
    settings()->set_shared_value(::fretboard_diagrams_auto_update, Val(v));
}

Color EngravingConfiguration::formatting_color() const { return color_from_value(settings()->value(::formatting_color)); }

Color EngravingConfiguration::frame_color() const { return color_from_value(settings()->value(::frame_color)); }

Color EngravingConfiguration::score_grey_color() const { return color_from_value(settings()->value(::score_grey_color)); }

Color EngravingConfiguration::invisible_color() const { return color_from_value(settings()->value(::invisible_color)); }

Color EngravingConfiguration::unlinked_color() const { return color_from_value(settings()->value(::unlinked_color)); }

const IEngravingConfiguration::DebuggingOptions &EngravingConfiguration::debugging_options() const { return debugging_options_; }

void EngravingConfiguration::set_debugging_options(const DebuggingOptions &options) { debugging_options_ = options; }

bool EngravingConfiguration::allow_reading_images_from_outside_mscz() const { return false; }

bool EngravingConfiguration::guitar_pro_import_experimental() const {
    // The experimental Guitar Pro import is a desktop preference the engine does not offer.
    return false;
}

bool EngravingConfiguration::negative_frets_allowed() const { return guitar_pro_import_experimental(); }

void EngravingConfiguration::set_guitar_pro_multivoice_enabled(bool multi_voice) { multi_voice_ = multi_voice; }

bool EngravingConfiguration::guitar_pro_multivoice_enabled() const { return multi_voice_; }

bool EngravingConfiguration::min_distance_for_partial_skyline_calculated() const { return guitar_pro_import_experimental(); }

bool EngravingConfiguration::specific_slurs_layout_workaround() const { return guitar_pro_import_experimental(); }

bool EngravingConfiguration::prefer_same_string_for_transpose() const { return false; }

void EngravingConfiguration::set_prefer_same_string_for_transpose(bool /*preferSameString*/) {}

bool EngravingConfiguration::keep_dead_notes_unchanged_on_transpose() const { return false; }
