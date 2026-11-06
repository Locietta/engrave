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

#include "engravingitem.h"

namespace mu::engraving {
struct Chord;
struct Factory;
struct Transaction;

//---------------------------------------------------------
//   @@ Arpeggio
//---------------------------------------------------------

enum class AnchorRebaseDirection : unsigned char { UP, DOWN };

struct Arpeggio : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, Arpeggio)
    DECLARE_CLASSOF(ElementType::ARPEGGIO)

public:

    ~Arpeggio() override;

    Arpeggio *clone() const override { return new Arpeggio(*this); }

    ArpeggioType arpeggio_type() const { return arpeggio_type_; }

    void set_arpeggio_type(ArpeggioType v) { arpeggio_type_ = v; }

    const TranslatableString &arpeggio_type_name() const;

    int subtype() const override { return int(arpeggio_type_); }

    TranslatableString subtype_user_name() const override;

    Chord *chord() const { return (Chord *) ownership_parent(); }

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void reset() override;

    int span() const { return span_; }

    void set_span(int val) { span_ = val; }

    track_idx_t end_track() const { return track() + span_ - 1; }

    bool cross_staff() const;
    staff_idx_t v_staff_idx() const override;
    void find_and_attach_to_chords();
    void detach_from_chords(track_idx_t strack, track_idx_t etrack);
    void rebase_start_anchor(AnchorRebaseDirection direction);
    void rebase_end_anchor(AnchorRebaseDirection direction);

    double user_len1() const { return user_len1_; }

    double user_len2() const { return user_len2_; }

    void set_user_len1(double v) { user_len1_ = v; }

    void set_user_len2(double v) { user_len2_ = v; }

    bool play_arpeggio() const { return play_arpeggio_; }

    void set_play_arpeggio(bool p) { play_arpeggio_ = p; }

    double stretch() const { return stretch_; }

    void set_stretch(double val) { stretch_ = val; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;

    struct LayoutData : public EngravingItem::LayoutData {
        // cache
        double top = 0.0;
        double bottom = 0.0;
        double mag_s = 0.0;
        double max_chord_pad = 0.0;
        double min_chord_x = 0.0;

        // out
        SymIdList symbols;
        RectF syms_b_box;
        double arpeggio_height = -1.0;
    };
    DECLARE_LAYOUTDATA_METHODS(Arpeggio)

protected:
    friend struct Factory;

    Arpeggio(DummyParentOr<Chord> parent, ElementType type = ElementType::ARPEGGIO);

private:
    void spatium_changed(double /*oldValue*/, double /*newValue*/) override;

    ArpeggioType arpeggio_type_ = ArpeggioType::NORMAL;
    double user_len1_ = 0.0;
    double user_len2_ = 0.0;

    int span_ = 1; // how many voices the arpeggio spans
    bool play_arpeggio_ = true;

    double stretch_ = 1.0;
};
} // namespace mu::engraving
