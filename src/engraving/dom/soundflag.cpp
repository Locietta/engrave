/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#include "soundflag.h"

#include <climits>

#include "../editing/editsoundflag.h"
#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "linkedobjects.h"
#include "mscore.h"
#include "score.h"

using namespace muse::draw;
using namespace mu::engraving;

SoundFlag::SoundFlag(DummyParentOr<EngravingItem> parent) : EngravingItem(ElementType::SOUND_FLAG, parent) {
    String font_family = configuration()->icons_font_family();
    icon_font_valid_ = !font_family.empty();
    icon_font_ = Font(font_family, Font::Type::ICON);
    icon_font_.set_point_size_f(::mu::engraving::ui_icons_default_font_size);

    //! draw on top of all elements
    set_z(INT_MAX);
}

SoundFlag *SoundFlag::clone() const { return new SoundFlag(*this); }

void SoundFlag::set_selected(bool f) {
    EngravingItem *parent = ownership_parent_item();
    if (parent) {
        parent->set_selected(f);
    }

    EngravingItem::set_selected(f);
}

PropertyValue SoundFlag::get_property(Pid id) const {
    switch (id) {
        case Pid::PLAY: return play_;
        case Pid::VISIBLE:
        case Pid::AUTOPLACE:
        case Pid::SMALL: return PropertyValue();
        case Pid::APPLY_TO_ALL_STAVES: return apply_to_all_staves_;
        default: return EngravingItem::get_property(id);
    }
}

bool SoundFlag::set_property(Pid id, const PropertyValue &value) {
    switch (id) {
        case Pid::PLAY: play_ = value.to_bool(); return true;
        case Pid::VISIBLE:
        case Pid::AUTOPLACE:
        case Pid::SMALL: return false;
        case Pid::APPLY_TO_ALL_STAVES: apply_to_all_staves_ = value.to_bool(); return true;
        default: return EngravingItem::set_property(id, value);
    }
}

PropertyValue SoundFlag::property_default(Pid id) const {
    switch (id) {
        case Pid::PLAY: return true;
        case Pid::VISIBLE:
        case Pid::AUTOPLACE:
        case Pid::SMALL: return PropertyValue();
        case Pid::APPLY_TO_ALL_STAVES: return true;
        default: return EngravingItem::property_default(id);
    }
}

const SoundFlag::PresetCodes &SoundFlag::sound_presets() const { return sound_presets_; }

void SoundFlag::set_sound_presets(const PresetCodes &sound_presets) { sound_presets_ = sound_presets; }

const SoundFlag::PlayingTechniqueCode &SoundFlag::playing_technique() const { return playing_technique_; }

void SoundFlag::set_playing_technique(const PlayingTechniqueCode &technique) { playing_technique_ = technique; }

bool SoundFlag::play() const { return play_; }

void mu::engraving::SoundFlag::set_play(bool play) { play_ = play; }

bool SoundFlag::apply_to_all_staves() const { return apply_to_all_staves_; }

void SoundFlag::set_apply_to_all_staves(bool apply) { apply_to_all_staves_ = apply; }

void SoundFlag::clear() {
    if (sound_presets_.empty() && playing_technique_.empty()) {
        return;
    }

    sound_presets_.clear();
    playing_technique_.clear();

    trigger_layout();
}

bool SoundFlag::should_hide() const {
    if (!icon_font_valid_) {
        return true;
    }

    if (const Score *score = this->score()) {
        if (!score->show_sound_flags()) {
            return true;
        }

        if (score->printing()) {
            return true;
        }
    }

    if (!sound_presets_.empty() || !playing_technique_.empty()) {
        return false;
    }

    if (selected()) {
        return false;
    }

    const EngravingItem *parent = ownership_parent_item();
    if (parent && parent->selected() && score()->selection().is_single()) {
        return false;
    }

    return true;
}

void SoundFlag::undo_change_sound_flag(const PresetCodes &presets, const PlayingTechniqueCode &technique) {
    if (sound_presets_ == presets && playing_technique_ == technique) {
        return;
    }

    score()->undo(new ChangeSoundFlag(this, presets, technique));
    trigger_layout();

    const LinkedObjects *links = this->links();
    if (!links) {
        return;
    }

    for (EngravingObject *obj : *links) {
        if (obj->is_sound_flag()) {
            SoundFlag *linked_sound_flag = to_sound_flag(obj);
            score()->undo(new ChangeSoundFlag(linked_sound_flag, presets, technique));
            linked_sound_flag->trigger_layout();
        }
    }
}

char16_t SoundFlag::icon_code() const { return 0xEF4E; }

Font SoundFlag::icon_font() const {
    Font font = icon_font_;
    font.set_point_size_f(::mu::engraving::ui_icons_default_font_size * mag_s());
    return font;
}

Color SoundFlag::icon_background_color() const {
    Color color = cur_color(true, {});
    if (!selected()) {
        color = Color("#CFD5DD");
        color.set_alpha(128);
    }

    return color;
}
