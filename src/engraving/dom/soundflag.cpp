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

#include "iengravingconfiguration.h" // IWYU pragma: keep

#include "../editing/editsoundflag.h"
#include "linkedobjects.h"
#include "mscore.h"
#include "score.h"

using namespace muse::draw;
using namespace mu::engraving;

SoundFlag::SoundFlag(DummyParentOr<EngravingItem> parent) : EngravingItem(ElementType::SOUND_FLAG, parent) {
    String fontFamily = configuration()->iconsFontFamily();
    iconFontValid_ = !fontFamily.empty();
    iconFont_ = Font(fontFamily, Font::Type::Icon);
    iconFont_.setPointSizeF(UI_ICONS_DEFAULT_FONT_SIZE);

    //! draw on top of all elements
    setZ(INT_MAX);
}

SoundFlag *SoundFlag::clone() const { return new SoundFlag(*this); }

void SoundFlag::setSelected(bool f) {
    EngravingItem *parent = ownershipParentItem();
    if (parent) {
        parent->setSelected(f);
    }

    EngravingItem::setSelected(f);
}

PropertyValue SoundFlag::getProperty(Pid id) const {
    switch (id) {
        case Pid::PLAY: return play_;
        case Pid::VISIBLE:
        case Pid::AUTOPLACE:
        case Pid::SMALL: return PropertyValue();
        case Pid::APPLY_TO_ALL_STAVES: return applyToAllStaves_;
        default: return EngravingItem::getProperty(id);
    }
}

bool SoundFlag::setProperty(Pid id, const PropertyValue &value) {
    switch (id) {
        case Pid::PLAY: play_ = value.toBool(); return true;
        case Pid::VISIBLE:
        case Pid::AUTOPLACE:
        case Pid::SMALL: return false;
        case Pid::APPLY_TO_ALL_STAVES: applyToAllStaves_ = value.toBool(); return true;
        default: return EngravingItem::setProperty(id, value);
    }
}

PropertyValue SoundFlag::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::PLAY: return true;
        case Pid::VISIBLE:
        case Pid::AUTOPLACE:
        case Pid::SMALL: return PropertyValue();
        case Pid::APPLY_TO_ALL_STAVES: return true;
        default: return EngravingItem::propertyDefault(id);
    }
}

const SoundFlag::PresetCodes &SoundFlag::soundPresets() const { return soundPresets_; }

void SoundFlag::setSoundPresets(const PresetCodes &soundPresets) { soundPresets_ = soundPresets; }

const SoundFlag::PlayingTechniqueCode &SoundFlag::playingTechnique() const { return playingTechnique_; }

void SoundFlag::setPlayingTechnique(const PlayingTechniqueCode &technique) { playingTechnique_ = technique; }

bool SoundFlag::play() const { return play_; }

void mu::engraving::SoundFlag::setPlay(bool play) { play_ = play; }

bool SoundFlag::applyToAllStaves() const { return applyToAllStaves_; }

void SoundFlag::setApplyToAllStaves(bool apply) { applyToAllStaves_ = apply; }

void SoundFlag::clear() {
    if (soundPresets_.empty() && playingTechnique_.empty()) {
        return;
    }

    soundPresets_.clear();
    playingTechnique_.clear();

    triggerLayout();
}

bool SoundFlag::shouldHide() const {
    if (!iconFontValid_) {
        return true;
    }

    if (const Score *score = this->score()) {
        if (!score->showSoundFlags()) {
            return true;
        }

        if (score->printing()) {
            return true;
        }
    }

    if (!soundPresets_.empty() || !playingTechnique_.empty()) {
        return false;
    }

    if (selected()) {
        return false;
    }

    const EngravingItem *parent = ownershipParentItem();
    if (parent && parent->selected() && score()->selection().isSingle()) {
        return false;
    }

    return true;
}

void SoundFlag::undoChangeSoundFlag(const PresetCodes &presets, const PlayingTechniqueCode &technique) {
    if (soundPresets_ == presets && playingTechnique_ == technique) {
        return;
    }

    score()->undo(new ChangeSoundFlag(this, presets, technique));
    triggerLayout();

    const LinkedObjects *links = this->links();
    if (!links) {
        return;
    }

    for (EngravingObject *obj : *links) {
        if (obj->isSoundFlag()) {
            SoundFlag *linkedSoundFlag = toSoundFlag(obj);
            score()->undo(new ChangeSoundFlag(linkedSoundFlag, presets, technique));
            linkedSoundFlag->triggerLayout();
        }
    }
}

char16_t SoundFlag::iconCode() const { return 0xEF4E; }

Font SoundFlag::iconFont() const {
    Font font = iconFont_;
    font.setPointSizeF(UI_ICONS_DEFAULT_FONT_SIZE * magS());
    return font;
}

Color SoundFlag::iconBackgroundColor() const {
    Color color = curColor(true, {});
    if (!selected()) {
        color = Color("#CFD5DD");
        color.setAlpha(128);
    }

    return color;
}
