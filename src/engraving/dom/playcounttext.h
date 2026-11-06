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

#pragma once

#include "textbase.h"

namespace mu::engraving {
class PlayCountText final : public TextBase {
    OBJECT_ALLOCATOR(engraving, PlayCountText)
    DECLARE_CLASSOF(ElementType::PLAY_COUNT_TEXT)

public:
    PlayCountText *clone() const override { return new PlayCountText(*this); }

    Segment *segment() const { return to_segment(ownership_parent()); }

    BarLine *barline() const;

    bool allow_time_anchor() const override { return false; }

    String play_count_custom_text() const { return play_count_custom_text_; }

    void set_play_count_custom_text(const String &v) { play_count_custom_text_ = v; }

    void set_play_count_text_setting(const AutoCustomHide &v) { play_count_text_setting_ = v; }

    AutoCustomHide play_count_text_setting() const { return play_count_text_setting_; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;

    bool position_relative_to_notehead_rest() const override { return false; }

private:
    friend class Factory;
    PlayCountText(DummyParentOr<Segment> parent, TextStyleType tid = TextStyleType::REPEAT_PLAY_COUNT);

    AutoCustomHide play_count_text_setting_ = AutoCustomHide::AUTO;
    String play_count_custom_text_ = u"";
};
} // namespace mu::engraving
