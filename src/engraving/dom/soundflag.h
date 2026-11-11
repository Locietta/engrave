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

#pragma once

#include "draw/types/font.h"
#include "engravingitem.h"

namespace mu::engraving {
struct SoundFlag final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::SOUND_FLAG)

public:
    explicit SoundFlag(DummyParentOr<EngravingItem> parent);

    SoundFlag *clone() const override;

    void set_selected(bool f) override;

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid id, const PropertyValue &value) override;
    PropertyValue property_default(Pid id) const override;

    bool can_be_excluded_from_other_parts() const override { return false; }

    using PresetCodes = StringList;
    using PlayingTechniqueCode = String;

    const PresetCodes &sound_presets() const;
    void set_sound_presets(const PresetCodes &sound_presets);

    const PlayingTechniqueCode &playing_technique() const;
    void set_playing_technique(const PlayingTechniqueCode &technique);

    bool play() const;
    void set_play(bool play);

    bool apply_to_all_staves() const;
    void set_apply_to_all_staves(bool apply);

    void clear();

    bool should_hide() const;

    void undo_change_sound_flag(const PresetCodes &presets, const PlayingTechniqueCode &technique);

    char16_t icon_code() const;
    muse::draw::Font icon_font() const;
    Color icon_background_color() const;

private:
    PresetCodes sound_presets_;
    PlayingTechniqueCode playing_technique_;

    muse::draw::Font icon_font_;
    bool icon_font_valid_ = false;

    bool play_ = true;
    bool apply_to_all_staves_ = true;
};
} // namespace mu::engraving
