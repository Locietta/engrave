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
#include "propertyvalue.h"

#include "log.h"
#include "realfn.h"

using namespace mu::engraving;

bool PropertyValue::is_valid() const { return type_ != PType::UNDEFINED; }

PType PropertyValue::type() const { return type_; }

bool PropertyValue::operator==(const PropertyValue &v) const {
    if (v.type_ == PType::UNDEFINED || type_ == PType::UNDEFINED) {
        return v.type_ == type_;
    }

    //! HACK Temporary hack for bool comparisons (maybe one type is bool and another type is int)
    if (v.type_ == PType::BOOL || type_ == PType::BOOL) {
        return v.value<bool>() == value<bool>();
    }

    //! HACK Temporary hack for int comparisons (maybe one type is int and another type is enum)
    if (v.type_ == PType::INT || type_ == PType::INT) {
        return v.value<int>() == value<int>();
    }

    //! HACK Temporary hack for Spatium comparisons (maybe one type is Spatium and another type is real)
    if (v.type_ == PType::SPATIUM || type_ == PType::SPATIUM) {
        return muse::real_is_equal(v.value<double>(), value<double>());
    }

    //! HACK Temporary hack for Fraction comparisons
    if (v.type_ == PType::FRACTION) {
        assert(type_ == PType::FRACTION);
        return v.value<Fraction>().identical(value<Fraction>());
    }

    if (v.type_ == PType::REAL) {
        assert(type_ == PType::REAL);
        return muse::real_is_equal(v.value<double>(), value<double>());
    }

    return v.type_ == type_ && v.data_ == data_;
}
