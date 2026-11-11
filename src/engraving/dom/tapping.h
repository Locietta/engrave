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

#include "articulation.h"
#include "engraving/types/types.h"
#include "slur.h"
#include "textbase.h"

namespace mu::engraving {
struct TappingHalfSlur;
struct TappingText;

struct Tapping : Articulation {
private:
    DECLARE_CLASSOF(ElementType::TAPPING)

public:
    Tapping(const Tapping &);

    Tapping *clone() const override { return new Tapping(*this); }

    struct LayoutData : public Articulation::LayoutData {};
    DECLARE_LAYOUTDATA_METHODS(Tapping)

    TappingHand hand() const { return hand_; }

    void set_hand(TappingHand h) { hand_ = h; }

    TappingText *text() const { return text_; }

    void set_text(TappingText *text) { text_ = text; }

    void set_selected(bool f) override;

    TappingHalfSlur *half_slur_above() const { return half_slur_above_; }

    void set_half_slur_above(TappingHalfSlur *s) { half_slur_above_ = s; }

    TappingHalfSlur *half_slur_below() const { return half_slur_below_; }

    void set_half_slur_below(TappingHalfSlur *s) { half_slur_below_ = s; }

    LHTappingShowItems lh_show_items() const;

    void style_changed() override;

    double mag() const override;

    int subtype() const override;

    TranslatableString type_user_name() const override;
    TranslatableString subtype_user_name() const override;

protected:
    friend struct mu::engraving::Factory;
    Tapping(DummyParentOr<ChordRest> parent);

private:
    TappingHand hand_ = TappingHand::INVALID;
    TappingText *text_ = nullptr;
    TappingHalfSlur *half_slur_above_ = nullptr;
    TappingHalfSlur *half_slur_below_ = nullptr;
};

struct TappingHalfSlurSegment : SlurSegment {
private:
    DECLARE_CLASSOF(ElementType::TAPPING_HALF_SLUR_SEGMENT)

public:
    TappingHalfSlurSegment(TappingHalfSlur *sp);
    TappingHalfSlurSegment(const TappingHalfSlurSegment &other);

    TappingHalfSlurSegment *clone() const override { return new TappingHalfSlurSegment(*this); }

    TappingHalfSlur *tapping_half_slur() const { return to_tapping_half_slur(spanner()); }
};

struct TappingHalfSlur : Slur {
private:
    DECLARE_CLASSOF(ElementType::TAPPING_HALF_SLUR)

    bool is_half_slur_above() const { return is_half_slur_above_; }

    void set_is_half_slur_above(bool v) { is_half_slur_above_ = v; }

public:
    TappingHalfSlur(EngravingItem *parent);
    TappingHalfSlur(const TappingHalfSlur &);

    TappingHalfSlur *clone() const override { return new TappingHalfSlur(*this); }

    TappingHalfSlurSegment *new_slur_tie_segment() override { return new TappingHalfSlurSegment(this); }

    Tapping *tapping() const { return to_tapping(ownership_parent()); }

protected:
    bool is_in_spanner_map() const override { return false; }

private:
    bool is_half_slur_above_ = true;
};

struct TappingText final : TextBase {
private:
    DECLARE_CLASSOF(ElementType::TAPPING_TEXT)

public:
    TappingText(Tapping *parent = nullptr);
    TappingText(const TappingText &t);

    TappingText *clone() const override { return new TappingText(*this); }

    Tapping *tapping() const { return to_tapping(ownership_parent()); }

    Color cur_color(const rendering::PaintOptions &opt) const override;

    bool position_relative_to_notehead_rest() const override { return true; }
};
} // namespace mu::engraving
