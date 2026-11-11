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
#include "key.h"

namespace mu::engraving {
struct Factory;
struct Segment;
struct Transaction;

struct KeySig final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::KEYSIG)

    M_PROPERTY2(bool, is_courtesy, set_is_courtesy, false)

public:

    KeySig *clone() const override { return new KeySig(*this); }

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    double mag() const override;

    void set_key(Key concert_key, Key transposed_key);
    void set_key(Key concert_key);

    Segment *segment() const { return (Segment *) ownership_parent(); }

    Measure *measure() const { return ownership_parent() ? (Measure *) ownership_parent()->ownership_parent() : nullptr; }

    Key key() const { return sig_.key(); }

    Key concert_key() const { return sig_.concert_key(); }

    const std::vector<CustDef> &custom_key_defs() const { return sig_.custom_key_defs(); }

    int deg_in_key(int degree) const { return sig_.deg_in_key(degree); }

    SymId sym_in_key(SymId sym, int degree) const { return sig_.sym_in_key(sym, degree); }

    bool is_custom() const { return sig_.custom(); }

    bool is_atonal() const { return sig_.is_atonal(); }

    bool is_change() const;

    const KeySigEvent &key_sig_event() const { return sig_; }

    bool operator==(const KeySig &) const;
    void change_key_sig_event(const KeySigEvent &);

    void set_key_sig_event(const KeySigEvent &e) { sig_ = e; }

    bool show_courtesy() const { return show_courtesy_; }

    void set_show_courtesy(bool v) { show_courtesy_ = v; }

    KeyMode mode() const { return sig_.mode(); }

    void set_mode(KeyMode v) { sig_.set_mode(v); }

    PointF staff_offset() const override;

    bool hide_naturals() const { return hide_naturals_; }

    void set_hide_naturals(bool hide) { hide_naturals_ = hide; }

    void set_for_instrument_change(bool for_instrument_change) { sig_.set_for_instrument_change(for_instrument_change); }

    bool for_instrument_change() const { return sig_.for_instrument_change(); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    using EngravingItem::undo_change_property;
    void undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    PropertyValue property_default(Pid id) const override;
    EngravingObject *property_delegate(Pid) const override;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    int subtype() const override { return int(key()) + 7; }

    muse::TranslatableString subtype_user_name() const override;

    struct LayoutData : public EngravingItem::LayoutData {
        std::vector<KeySym> key_symbols;
    };
    DECLARE_LAYOUTDATA_METHODS(KeySig)

private:
    friend struct Factory;

    KeySig(DummyParentOr<Segment>);
    KeySig(const KeySig &);

    void add_layout(SymId sym, int line);

    bool show_courtesy_;
    bool hide_naturals_; // used in layout to override score style (needed for the Continuous panel)
    KeySigEvent sig_;
};
} // namespace mu::engraving
