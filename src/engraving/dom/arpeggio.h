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
class Chord;
class Factory;
class Transaction;

//---------------------------------------------------------
//   @@ Arpeggio
//---------------------------------------------------------

enum class AnchorRebaseDirection : unsigned char { UP, DOWN };

class Arpeggio : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Arpeggio)
    DECLARE_CLASSOF(ElementType::ARPEGGIO)

public:

    ~Arpeggio() override;

    Arpeggio *clone() const override { return new Arpeggio(*this); }

    ArpeggioType arpeggioType() const { return arpeggioType_; }

    void setArpeggioType(ArpeggioType v) { arpeggioType_ = v; }

    const TranslatableString &arpeggioTypeName() const;

    int subtype() const override { return int(arpeggioType_); }

    TranslatableString subtypeUserName() const override;

    Chord *chord() const { return (Chord *) ownershipParent(); }

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void reset() override;

    int span() const { return span_; }

    void setSpan(int val) { span_ = val; }

    track_idx_t endTrack() const { return track() + span_ - 1; }

    bool crossStaff() const;
    staff_idx_t vStaffIdx() const override;
    void findAndAttachToChords();
    void detachFromChords(track_idx_t strack, track_idx_t etrack);
    void rebaseStartAnchor(AnchorRebaseDirection direction);
    void rebaseEndAnchor(AnchorRebaseDirection direction);

    double userLen1() const { return userLen1_; }

    double userLen2() const { return userLen2_; }

    void setUserLen1(double v) { userLen1_ = v; }

    void setUserLen2(double v) { userLen2_ = v; }

    bool playArpeggio() const { return playArpeggio_; }

    void setPlayArpeggio(bool p) { playArpeggio_ = p; }

    double Stretch() const { return stretch_; }

    void setStretch(double val) { stretch_ = val; }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid propertyId) const override;

    struct LayoutData : public EngravingItem::LayoutData {
        // cache
        double top = 0.0;
        double bottom = 0.0;
        double magS = 0.0;
        double maxChordPad = 0.0;
        double minChordX = 0.0;

        // out
        SymIdList symbols;
        RectF symsBBox;
        double arpeggioHeight = -1.0;
    };
    DECLARE_LAYOUTDATA_METHODS(Arpeggio)

protected:
    friend class Factory;

    Arpeggio(DummyParentOr<Chord> parent, ElementType type = ElementType::ARPEGGIO);

private:
    void spatiumChanged(double /*oldValue*/, double /*newValue*/) override;

    ArpeggioType arpeggioType_ = ArpeggioType::NORMAL;
    double userLen1_ = 0.0;
    double userLen2_ = 0.0;

    int span_ = 1; // how many voices the arpeggio spans
    bool playArpeggio_ = true;

    double stretch_ = 1.0;
};
} // namespace mu::engraving
