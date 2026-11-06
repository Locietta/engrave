/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "playcounttext.h"

#include "../editing/transaction/undostack.h"
#include "../types/typesconv.h"
#include "barline.h"
#include "measure.h"
#include "score.h"

using namespace mu::engraving;

static ElementStyle play_count_style{
    {Sid::REPEAT_PLAY_COUNT_PLACEMENT, Pid::PLACEMENT},
    {Sid::REPEAT_PLAY_COUNT_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

PlayCountText::PlayCountText(DummyParentOr<Segment> parent, TextStyleType tid)
    : TextBase(ElementType::PLAY_COUNT_TEXT, parent, tid, ElementFlag::SYSTEM | ElementFlag::ON_STAFF) {
    init_element_style(&play_count_style);
}

PropertyValue PlayCountText::get_property(Pid id) const {
    switch (id) {
        case Pid::PLAY_COUNT_TEXT_SETTING: return play_count_text_setting_;
        case Pid::PLAY_COUNT_TEXT: return play_count_custom_text_;
        default: return TextBase::get_property(id);
    }
}

bool PlayCountText::set_property(Pid id, const PropertyValue &v) {
    Segment *s = segment();
    Measure *m = s ? s->measure() : nullptr;

    switch (id) {
        case Pid::PLAY_COUNT_TEXT_SETTING:
            play_count_text_setting_ = v.value<AutoCustomHide>();
            if (play_count_text_setting() == AutoCustomHide::CUSTOM && play_count_custom_text().is_empty()) {
                int repeat_count = m ? m->repeat_count() : 2;
                String text = TConv::translated_user_name(style().style_v(Sid::REPEAT_PLAY_COUNT_PRESET).value<RepeatPlayCountPreset>())
                                  .arg(repeat_count);
                undo_change_property(Pid::PLAY_COUNT_TEXT, text);
            }
            break;
        case Pid::PLAY_COUNT_TEXT: play_count_custom_text_ = v.value<String>(); break;
        default: return TextBase::set_property(id, v);
    }
    trigger_layout();
    return true;
}

PropertyValue PlayCountText::property_default(Pid property_id) const {
    Segment *s = segment();
    Measure *m = s ? s->measure() : nullptr;

    switch (property_id) {
        case Pid::PLAY_COUNT_TEXT_SETTING: return AutoCustomHide::AUTO;
        case Pid::PLAY_COUNT_TEXT: {
            int repeat_count = m ? m->repeat_count() : 2;
            return TConv::translated_user_name(style().style_v(Sid::REPEAT_PLAY_COUNT_PRESET).value<RepeatPlayCountPreset>())
                .arg(repeat_count);
        }
        default: return TextBase::property_default(property_id);
    }
}

mu::engraving::BarLine *mu::engraving::PlayCountText::barline() const {
    Segment *seg = segment();
    EngravingItem *el = seg ? seg->element(track()) : nullptr;

    return to_bar_line(el);
}
