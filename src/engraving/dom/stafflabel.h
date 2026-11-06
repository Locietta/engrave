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
struct StaffLabel {
    StaffLabel() = default;

    StaffLabel(const muse::String &long_name, const muse::String &short_name) : long_name_(long_name), short_name_(short_name) {}

    bool operator==(const StaffLabel &i) const { return long_name_ == i.long_name_ && short_name_ == i.short_name_; }

    const muse::String &long_name() const { return long_name_; }

    const muse::String &short_name() const { return short_name_; }

    void set_long_name(const muse::String &s) { long_name_ = s; }

    void set_short_name(const muse::String &s) { short_name_ = s; }

    bool empty() const { return long_name_.empty() && short_name_.empty(); }

private:
    muse::String long_name_;
    muse::String short_name_;
};

struct InstrumentLabel : StaffLabel {
    bool operator==(const InstrumentLabel &i) const;

    int number() const { return number_; }

    void set_number(int v) { number_ = v; }

    const muse::String &transposition() const { return transposition_; }

    void set_transposition(const muse::String &s) { transposition_ = s; }

    bool show_number_long() const { return show_number_long_; }

    void set_show_number_long(bool v) { show_number_long_ = v; }

    bool show_number_short() const { return show_number_short_; }

    void set_show_number_short(bool v) { show_number_short_ = v; }

    bool show_transposition_long() const { return show_transposition_long_; }

    void set_show_transposition_long(bool v) { show_transposition_long_ = v; }

    bool show_transposition_short() const { return show_transposition_short_; }

    void set_show_transposition_short(bool v) { show_transposition_short_ = v; }

    bool use_custom_name() const { return use_custom_name_; }

    void set_use_custom_name(bool v) { use_custom_name_ = v; }

    muse::String custom_name_long() const { return custom_name_long_; }

    void set_custom_name_long(const muse::String &v) { custom_name_long_ = v; }

    muse::String custom_name_short() const { return custom_name_short_; }

    void set_custom_name_short(const muse::String &v) { custom_name_short_ = v; }

    bool allow_group_name() const { return allow_group_name_; }

    void set_allow_group_name(bool v) { allow_group_name_ = v; }

    muse::String custom_name_long_group() const { return custom_name_long_group_; }

    void set_custom_name_long_group(const muse::String &v) { custom_name_long_group_ = v; }

    muse::String custom_name_short_group() const { return custom_name_short_group_; }

    void set_custom_name_short_group(const muse::String &v) { custom_name_short_group_ = v; }

    bool use_custom_group_name() const { return use_custom_group_name_; }

    void set_use_custom_group_name(bool v) { use_custom_group_name_ = v; }

    muse::String custom_name_long_individual() const { return custom_name_long_individual_; }

    void set_custom_name_long_individual(const muse::String &v) { custom_name_long_individual_ = v; }

    muse::String custom_name_short_individual() const { return custom_name_short_individual_; }

    void set_custom_name_short_individual(const muse::String &v) { custom_name_short_individual_ = v; }

    bool use_custom_individual_name() const { return use_custom_individual_name_; }

    void set_use_custom_individual_name(bool v) { use_custom_individual_name_ = v; }

    bool empty() const;

private:
    int number_ = 0;
    bool show_number_long_ = true;
    bool show_number_short_ = true;

    muse::String transposition_;
    bool show_transposition_long_ = true;
    bool show_transposition_short_ = true;

    bool use_custom_name_ = false;
    muse::String custom_name_long_;
    muse::String custom_name_short_;

    bool allow_group_name_ = true;

    muse::String custom_name_long_group_;
    muse::String custom_name_short_group_;
    bool use_custom_group_name_ = false;

    muse::String custom_name_long_individual_;
    muse::String custom_name_short_individual_;
    bool use_custom_individual_name_ = false;
};

inline bool InstrumentLabel::operator==(const InstrumentLabel &i) const {
    return StaffLabel::operator==(i) && number_ == i.number_ && transposition_ == i.transposition_ &&
           show_number_long_ == i.show_number_long_ && show_number_short_ == i.show_number_short_ &&
           use_custom_name_ == i.use_custom_name_ && custom_name_long_ == i.custom_name_long_ &&
           custom_name_short_ == i.custom_name_short_ && show_transposition_long_ == i.show_transposition_long_ &&
           show_transposition_short_ == i.show_transposition_short_ && allow_group_name_ == i.allow_group_name_ &&
           custom_name_long_group_ == i.custom_name_long_group_ && custom_name_short_group_ == i.custom_name_short_group_ &&
           use_custom_group_name_ == i.use_custom_group_name_ && custom_name_long_individual_ == i.custom_name_long_individual_ &&
           custom_name_short_individual_ == i.custom_name_short_individual_ && use_custom_individual_name_ == i.use_custom_individual_name_;
}

inline bool InstrumentLabel::empty() const {
    static const InstrumentLabel empty_label = InstrumentLabel();

    return *this == empty_label;
}
} // namespace mu::engraving
