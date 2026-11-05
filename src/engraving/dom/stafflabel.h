/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
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

#include "global/types/string.h"

namespace mu::engraving {
class StaffLabel {
public:
    StaffLabel() = default;

    StaffLabel(const muse::String &longName, const muse::String &shortName) : longName_(longName), shortName_(shortName) {}

    bool operator==(const StaffLabel &i) const { return longName_ == i.longName_ && shortName_ == i.shortName_; }

    const muse::String &longName() const { return longName_; }

    const muse::String &shortName() const { return shortName_; }

    void setLongName(const muse::String &s) { longName_ = s; }

    void setShortName(const muse::String &s) { shortName_ = s; }

    bool empty() const { return longName_.empty() && shortName_.empty(); }

private:
    muse::String longName_;
    muse::String shortName_;
};

class InstrumentLabel : public StaffLabel {
public:
    bool operator==(const InstrumentLabel &i) const;

    int number() const { return number_; }

    void setNumber(int v) { number_ = v; }

    const muse::String &transposition() const { return transposition_; }

    void setTransposition(const muse::String &s) { transposition_ = s; }

    bool showNumberLong() const { return showNumberLong_; }

    void setShowNumberLong(bool v) { showNumberLong_ = v; }

    bool showNumberShort() const { return showNumberShort_; }

    void setShowNumberShort(bool v) { showNumberShort_ = v; }

    bool showTranspositionLong() const { return showTranspositionLong_; }

    void setShowTranspositionLong(bool v) { showTranspositionLong_ = v; }

    bool showTranspositionShort() const { return showTranspositionShort_; }

    void setShowTranspositionShort(bool v) { showTranspositionShort_ = v; }

    bool useCustomName() const { return useCustomName_; }

    void setUseCustomName(bool v) { useCustomName_ = v; }

    muse::String customNameLong() const { return customNameLong_; }

    void setCustomNameLong(const muse::String &v) { customNameLong_ = v; }

    muse::String customNameShort() const { return customNameShort_; }

    void setCustomNameShort(const muse::String &v) { customNameShort_ = v; }

    bool allowGroupName() const { return allowGroupName_; }

    void setAllowGroupName(bool v) { allowGroupName_ = v; }

    muse::String customNameLongGroup() const { return customNameLongGroup_; }

    void setCustomNameLongGroup(const muse::String &v) { customNameLongGroup_ = v; }

    muse::String customNameShortGroup() const { return customNameShortGroup_; }

    void setCustomNameShortGroup(const muse::String &v) { customNameShortGroup_ = v; }

    bool useCustomGroupName() const { return useCustomGroupName_; }

    void setUseCustomGroupName(bool v) { useCustomGroupName_ = v; }

    muse::String customNameLongIndividual() const { return customNameLongIndividual_; }

    void setCustomNameLongIndividual(const muse::String &v) { customNameLongIndividual_ = v; }

    muse::String customNameShortIndividual() const { return customNameShortIndividual_; }

    void setCustomNameShortIndividual(const muse::String &v) { customNameShortIndividual_ = v; }

    bool useCustomIndividualName() const { return useCustomIndividualName_; }

    void setUseCustomIndividualName(bool v) { useCustomIndividualName_ = v; }

    bool empty() const;

private:
    int number_ = 0;
    bool showNumberLong_ = true;
    bool showNumberShort_ = true;

    muse::String transposition_;
    bool showTranspositionLong_ = true;
    bool showTranspositionShort_ = true;

    bool useCustomName_ = false;
    muse::String customNameLong_;
    muse::String customNameShort_;

    bool allowGroupName_ = true;

    muse::String customNameLongGroup_;
    muse::String customNameShortGroup_;
    bool useCustomGroupName_ = false;

    muse::String customNameLongIndividual_;
    muse::String customNameShortIndividual_;
    bool useCustomIndividualName_ = false;
};

inline bool InstrumentLabel::operator==(const InstrumentLabel &i) const {
    return StaffLabel::operator==(i) && number_ == i.number_ && transposition_ == i.transposition_ &&
           showNumberLong_ == i.showNumberLong_ && showNumberShort_ == i.showNumberShort_ && useCustomName_ == i.useCustomName_ &&
           customNameLong_ == i.customNameLong_ && customNameShort_ == i.customNameShort_ &&
           showTranspositionLong_ == i.showTranspositionLong_ && showTranspositionShort_ == i.showTranspositionShort_ &&
           allowGroupName_ == i.allowGroupName_ && customNameLongGroup_ == i.customNameLongGroup_ &&
           customNameShortGroup_ == i.customNameShortGroup_ && useCustomGroupName_ == i.useCustomGroupName_ &&
           customNameLongIndividual_ == i.customNameLongIndividual_ && customNameShortIndividual_ == i.customNameShortIndividual_ &&
           useCustomIndividualName_ == i.useCustomIndividualName_;
}

inline bool InstrumentLabel::empty() const {
    static const InstrumentLabel EMPTY_LABEL = InstrumentLabel();

    return *this == EMPTY_LABEL;
}
} // namespace mu::engraving
