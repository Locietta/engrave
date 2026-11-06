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
#ifndef MU_ENGRAVING_LD_ACCESS_DEV_H
#define MU_ENGRAVING_LD_ACCESS_DEV_H

#include <optional>

#ifdef MUE_ENABLE_ENGRAVING_LD_ACCESS
#include "log.h"
#endif

namespace mu::engraving {
//! NOTE Enum for detect layoutdata bad access errors
enum class LdAccess : unsigned char {
    CHECK = 0,       // should be correct; assert if still not
    BAD,             // known to be bad; don’t assert, we’ll fix it later
    MAYBE_NOTINITED, // in this case it’s okay if we access it before it’s been inited
    PASS             // for debug
};

#ifdef MUE_ENABLE_ENGRAVING_LD_ACCESS

//! TODO Using this macro, we can collect and output debugging information to show the dependency tree
#define LD_CONDITION(val)                                                                                       \
    if (!val) {                                                                                                 \
        LOGE_T("LD_ACCESS")() << "BAD ACCESS to: " << #val << ", file: " << __FILE__ << ", line: " << __LINE__; \
    }

#else
#define LD_CONDITION(val) (void) val;
#endif

// mark as independent
#define LD_INDEPENDENT

template <typename T>
struct LdFieldDebug {

    LdFieldDebug(const char *name, T def = T()) : name_(name), def_(def) {}

    inline void reset() { val_.reset(); }

    inline bool has_value() const { return val_.has_value(); }

    inline const T &value(LdAccess mode = LdAccess::CHECK) const {
        if (!val_.has_value()) {
#ifdef MUE_ENABLE_ENGRAVING_LD_ACCESS
            if (mode == LdAccess::CHECK) {
                LOGE_T("LD_ACCESS")() << "BAD ACCESS to: " << name_;
            }
#else
            (void) mode;
#endif
            return def_;
        }
        return val_.value();
    }

    inline const T &operator()() const { return value(); }

    operator T() const { return value(); }

    inline T &mut_value() {
        if (!val_.has_value()) {
            val_ = std::make_optional<T>(def_);
        }
        return val_.value();
    }

    inline void set_value(const T &v) { val_ = std::make_optional<T>(v); }

    LdFieldDebug &operator=(const T &v) {
        val_ = v;
        return *this;
    }

private:
    const char *name_ = nullptr;
    T def_;
    std::optional<T> val_;
};

template <typename T>
struct LdFieldProd {

    LdFieldProd(const char *, T def = T()) : val_(def) {}

    inline void reset() { val_ = T(); }

    inline bool has_value() const { return true; }

    inline const T &value(LdAccess = LdAccess::CHECK) const { return val_; }

    inline const T &operator()() const { return value(); }

    operator T() const { return value(); }

    inline T &mut_value() { return val_; }

    inline void set_value(const T &v) { val_ = v; }

    LdFieldProd &operator=(const T &v) {
        val_ = v;
        return *this;
    }

private:
    T val_;
};

#ifdef NDEBUG
template <typename T>
using ld_field = LdFieldProd<T>;
#else
template <typename T>
using ld_field = LdFieldDebug<T>;
#endif
} // namespace mu::engraving

#endif // MU_ENGRAVING_LD_ACCESS_DEV_H
