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

#ifndef MU_ENGRAVING_SYMBOL_H
#define MU_ENGRAVING_SYMBOL_H

#include <memory>

#include "draw/types/font.h"

#include "../iengravingfontsprovider.h"

#include "bsymbol.h"

namespace mu::engraving {
class Segment;
class IEngravingFont;

//---------------------------------------------------------
//   @@ Symbol
///    Symbol constructed from builtin symbol.
//
//   @P symbol       string       the SMuFL name of the symbol
//---------------------------------------------------------

class Symbol : public BSymbol {
    OBJECT_ALLOCATOR(engraving, Symbol)
    DECLARE_CLASSOF(ElementType::SYMBOL)

public:
    Symbol(const ElementType &type, EngravingObject *parent, ElementFlags f = ElementFlag::MOVABLE);
    Symbol(EngravingObject *parent, ElementFlags f = ElementFlag::MOVABLE);
    Symbol(const Symbol &);

    Symbol &operator=(const Symbol &) = delete;

    Symbol *clone() const override { return new Symbol(*this); }

    void set_sym(SymId s, const std::shared_ptr<IEngravingFont> &sf = nullptr);

    SymId sym() const { return sym_; }

    const std::shared_ptr<IEngravingFont> &score_font() const { return score_font_; }

    double symbols_size() const { return symbols_size_; }

    double sym_angle() const { return sym_angle_; }

    AsciiStringView sym_name() const;

    int subtype() const override { return int(sym_); }

    muse::TranslatableString subtype_user_name() const override;

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;
    virtual PropertyValue property_default(Pid) const override;

    double base_line() const override { return 0.0; }

    virtual Segment *segment() const { return (Segment *) ownership_parent(); }

    void reset() override;

protected:
    SymId sym_ = SymId::NO_SYM;
    std::shared_ptr<IEngravingFont> score_font_ = nullptr;
    double symbols_size_ = 1.0;
    double sym_angle_ = 0.0;
};

//---------------------------------------------------------
//   @@ FSymbol
///    Symbol constructed from a font glyph (i.e. a text character or emoji).
//---------------------------------------------------------

class FSymbol final : public BSymbol {
    OBJECT_ALLOCATOR(engraving, FSymbol)
    DECLARE_CLASSOF(ElementType::FSYMBOL)

public:
    FSymbol(DummyParentOr<EngravingItem> parent);
    FSymbol(const FSymbol &);

    FSymbol *clone() const override { return new FSymbol(*this); }

    String to_string() const;

    double base_line() const override { return 0.0; }

    Segment *segment() const { return (Segment *) ownership_parent(); }

    const muse::draw::Font &font() const { return font_; }

    char32_t code() const { return code_; }

    void set_font(const muse::draw::Font &f);

    void set_code(char32_t val) { code_ = val; }

private:
    muse::draw::Font font_;
    char32_t code_ = 0; // character code point (Unicode)
};
} // namespace mu::engraving
#endif
