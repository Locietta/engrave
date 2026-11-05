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

#include "key.h"
#include "engravingitem.h"

namespace mu::engraving {
class Factory;
class Segment;
class Transaction;

class KeySig final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, KeySig)
    DECLARE_CLASSOF(ElementType::KEYSIG)

    M_PROPERTY2(bool, isCourtesy, setIsCourtesy, false)

public:

    KeySig *clone() const override { return new KeySig(*this); }

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    double mag() const override;

    void setKey(Key concertKey, Key transposedKey);
    void setKey(Key concertKey);

    Segment *segment() const { return (Segment *) ownershipParent(); }

    Measure *measure() const { return ownershipParent() ? (Measure *) ownershipParent()->ownershipParent() : nullptr; }

    Key key() const { return sig_.key(); }

    Key concertKey() const { return sig_.concertKey(); }

    const std::vector<CustDef> &customKeyDefs() const { return sig_.customKeyDefs(); }

    int degInKey(int degree) const { return sig_.degInKey(degree); }

    SymId symInKey(SymId sym, int degree) const { return sig_.symInKey(sym, degree); }

    bool isCustom() const { return sig_.custom(); }

    bool isAtonal() const { return sig_.isAtonal(); }

    bool isChange() const;

    const KeySigEvent &keySigEvent() const { return sig_; }

    bool operator==(const KeySig &) const;
    void changeKeySigEvent(const KeySigEvent &);

    void setKeySigEvent(const KeySigEvent &e) { sig_ = e; }

    bool showCourtesy() const { return showCourtesy_; }

    void setShowCourtesy(bool v) { showCourtesy_ = v; }

    KeyMode mode() const { return sig_.mode(); }

    void setMode(KeyMode v) { sig_.setMode(v); }

    PointF staffOffset() const override;

    bool hideNaturals() const { return hideNaturals_; }

    void setHideNaturals(bool hide) { hideNaturals_ = hide; }

    void setForInstrumentChange(bool forInstrumentChange) { sig_.setForInstrumentChange(forInstrumentChange); }

    bool forInstrumentChange() const { return sig_.forInstrumentChange(); }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    using EngravingItem::undoChangeProperty;
    void undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    PropertyValue propertyDefault(Pid id) const override;
    EngravingObject *propertyDelegate(Pid) const override;

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    int subtype() const override { return int(key()) + 7; }

    muse::TranslatableString subtypeUserName() const override;

    struct LayoutData : public EngravingItem::LayoutData {
        std::vector<KeySym> keySymbols;
    };
    DECLARE_LAYOUTDATA_METHODS(KeySig)

private:
    friend class Factory;

    KeySig(DummyParentOr<Segment>);
    KeySig(const KeySig &);

    void addLayout(SymId sym, int line);

    bool showCourtesy_;
    bool hideNaturals_; // used in layout to override score style (needed for the Continuous panel)
    KeySigEvent sig_;
};
} // namespace mu::engraving
