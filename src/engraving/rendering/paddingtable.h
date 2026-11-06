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
struct PaddingVector : std::array<T, ::mu::engraving::tot_element_types> {
    T &operator[](size_t i) { return std::array<T, ::mu::engraving::tot_element_types>::operator[](i); }

    T &operator[](ElementType et) { return std::array<T, ::mu::engraving::tot_element_types>::operator[](static_cast<size_t>(et)); }

    const T &at(ElementType et) const { return std::array<T, ::mu::engraving::tot_element_types>::at(static_cast<size_t>(et)); }
};

struct PaddingTable : public PaddingVector<PaddingVector<double>> {
public:
    void update(const MStyle &style);

private:
    void init(double min_pad_unit);
};

struct ParenPaddingTable {
public:
    virtual ~ParenPaddingTable() = default;

    virtual void update(const MStyle &style) = 0;
    double padding(ElementType type1, ElementType type2) const;

protected:
    void init(double min_pad_unit);
    PaddingVector<double> paren_before_;
    PaddingVector<double> paren_after_;
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
        note_paren_padding_table.update(style);
        key_sig_paren_padding_table.update(style);
        time_sig_paren_padding_table.update(style);
        clef_paren_padding_table.update(style);
    }

    const ParenPaddingTable &get_table_for(const EngravingItem *item) const;

private:
    NoteParenPaddingTable note_paren_padding_table;
    KeySigParenPaddingTable key_sig_paren_padding_table;
    TimeSigParenPaddingTable time_sig_paren_padding_table;
    ClefParenPaddingTable clef_paren_padding_table;
};

struct PaddingTables {
    const PaddingTable &horizontal_padding_table() const { return horizontal_padding_table_; }

    const ParenPaddingTable &parenthesis_padding_table_for(const EngravingItem *item) const {
        return paren_padding_tables_.get_table_for(item);
    }

    void update(const MStyle &style) {
        horizontal_padding_table_.update(style);
        paren_padding_tables_.update(style);
    }

private:
    PaddingTable horizontal_padding_table_;
    ParenPaddingTables paren_padding_tables_;
};
} // namespace mu::engraving
