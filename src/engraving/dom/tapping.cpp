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

#include "tapping.h"

#include "stafftype.h"
#include "style/style.h"
#include "text.h"
#include "types/typesconv.h"

namespace mu::engraving {
Tapping::Tapping(DummyParentOr<ChordRest> parent) : Articulation(parent, ElementType::TAPPING) {}

Tapping::Tapping(const Tapping &other) : Articulation(other) { hand_ = other.hand_; }

void Tapping::set_selected(bool f) {
    if (text_) {
        text_->set_selected(f);
    }
    if (half_slur_above_) {
        half_slur_above_->set_selected(f);
    }
    if (half_slur_below_) {
        half_slur_below_->set_selected(f);
    }

    EngravingItem::set_selected(f);
}

LHTappingShowItems Tapping::lh_show_items() const {
    DO_ASSERT(hand_ == TappingHand::LEFT);

    const MStyle &s = style();
    bool tab_staff = staff_type()->is_tab_staff();

    return s.style_v(tab_staff ? Sid::LH_TAPPING_SHOW_ITEMS_TAB : Sid::LH_TAPPING_SHOW_ITEMS_NORMAL_STAVE).value<LHTappingShowItems>();
}

void Tapping::style_changed() {
    if (text_) {
        text_->style_changed();
    }
    Articulation::style_changed();
}

double Tapping::mag() const { return ldata()->mag() * Articulation::mag(); }

int Tapping::subtype() const { return static_cast<int>(hand_); }

TranslatableString Tapping::type_user_name() const { return EngravingItem::type_user_name(); }

TranslatableString Tapping::subtype_user_name() const {
    return hand_ == TappingHand::LEFT ? TranslatableString("engraving", "Left-hand") : TranslatableString("engraving", "Right-hand");
}

TappingHalfSlur::TappingHalfSlur(EngravingItem *parent) : Slur(parent, ElementType::TAPPING_HALF_SLUR) {}

TappingHalfSlur::TappingHalfSlur(const TappingHalfSlur &other) : Slur(other) {}

TappingHalfSlurSegment::TappingHalfSlurSegment(TappingHalfSlur *sp) : SlurSegment(sp, ElementType::TAPPING_HALF_SLUR_SEGMENT) {}

TappingHalfSlurSegment::TappingHalfSlurSegment(const TappingHalfSlurSegment &other) : SlurSegment(other) {}

static ElementStyle tap_text_style;

TappingText::TappingText(Tapping *parent)
    : TextBase(ElementType::TAPPING_TEXT, parent, TextStyleType::HAMMER_ON_PULL_OFF,
               ElementFlag::MOVABLE | ElementFlag::GENERATED | ElementFlag::ON_STAFF) {
    init_element_style(&tap_text_style);
}

TappingText::TappingText(const TappingText &t) : TextBase(t) {}

Color TappingText::cur_color(const rendering::PaintOptions &opt) const {
    if (Tapping *t = tapping()) {
        return t->cur_color(opt);
    }

    return EngravingItem::cur_color(opt);
}
} // namespace mu::engraving
