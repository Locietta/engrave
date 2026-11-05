/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include <array>

#include "../types/types.h"

namespace mu::engraving {
class EngravingItem;
class MStyle;

template <typename T>
struct PaddingVector : std::array<T, TOT_ELEMENT_TYPES> {
    T &operator[](size_t i) { return std::array<T, TOT_ELEMENT_TYPES>::operator[](i); }

    T &operator[](ElementType et) { return std::array<T, TOT_ELEMENT_TYPES>::operator[](static_cast<size_t>(et)); }

    const T &at(ElementType et) const { return std::array<T, TOT_ELEMENT_TYPES>::at(static_cast<size_t>(et)); }
};

struct PaddingTable : public PaddingVector<PaddingVector<double>> {
public:
    void update(const MStyle &style);

private:
    void init(double minPadUnit);
};

struct ParenPaddingTable {
public:
    virtual ~ParenPaddingTable() = default;

    virtual void update(const MStyle &style) = 0;
    double padding(ElementType type1, ElementType type2) const;

protected:
    void init(double minPadUnit);
    PaddingVector<double> parenBefore_;
    PaddingVector<double> parenAfter_;
};

struct NoteParenPaddingTable : public ParenPaddingTable {
    void update(const MStyle &style) override;
};

struct KeySigParenPaddingTable : public ParenPaddingTable {
    void update(const MStyle &style) override;
};

struct TimeSigParenPaddingTable : public ParenPaddingTable {
    void update(const MStyle &style) override;
};

struct ClefParenPaddingTable : public ParenPaddingTable {
    void update(const MStyle &style) override;
};

struct ParenPaddingTables {
    void update(const MStyle &style) {
        noteParenPaddingTable.update(style);
        keySigParenPaddingTable.update(style);
        timeSigParenPaddingTable.update(style);
        clefParenPaddingTable.update(style);
    }

    const ParenPaddingTable &getTableFor(const EngravingItem *item) const;

private:
    NoteParenPaddingTable noteParenPaddingTable;
    KeySigParenPaddingTable keySigParenPaddingTable;
    TimeSigParenPaddingTable timeSigParenPaddingTable;
    ClefParenPaddingTable clefParenPaddingTable;
};

struct PaddingTables {
    const PaddingTable &horizontalPaddingTable() const { return horizontalPaddingTable_; }

    const ParenPaddingTable &parenthesisPaddingTableFor(const EngravingItem *item) const { return parenPaddingTables_.getTableFor(item); }

    void update(const MStyle &style) {
        horizontalPaddingTable_.update(style);
        parenPaddingTables_.update(style);
    }

private:
    PaddingTable horizontalPaddingTable_;
    ParenPaddingTables parenPaddingTables_;
};
} // namespace mu::engraving
