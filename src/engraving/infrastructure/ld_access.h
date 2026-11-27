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

#include <tuple>
#include <type_traits>
#include <utility>

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
#define LD_CONDITION(val) std::ignore = (val);
#endif

// mark as independent
#define LD_INDEPENDENT

namespace detail {
template <typename T, bool Scalar = std::is_scalar_v<T>>
struct LdFieldDefault {
    constexpr LdFieldDefault(const T &def) noexcept : def_(def) {}

    constexpr T default_value() const noexcept { return def_; }

private:
    T def_;
};

// Non-scalar layout fields always default-construct; EBO avoids storing a second Shape/vector.
template <typename T>
struct LdFieldDefault<T, false> {
    constexpr LdFieldDefault(const T &) noexcept {}

    T default_value() const { return T{}; }
};
} // namespace detail

template <typename T>
struct LdField : private detail::LdFieldDefault<T> {
    LdField(const char *name = nullptr, T def = T())
        : detail::LdFieldDefault<T>(def),
#ifdef MUE_ENABLE_ENGRAVING_LD_ACCESS
          name_(name),
#endif
          val_(std::move(def)) {
#ifndef MUE_ENABLE_ENGRAVING_LD_ACCESS
        std::ignore = name;
#endif
    }

    LdField(const LdField &) = default;
    LdField(LdField &&) = default;
    LdField &operator=(const LdField &) = default;
    LdField &operator=(LdField &&) = default;

    inline void reset() {
        val_ = this->default_value();
        has_value_ = false;
    }

    inline bool has_value() const noexcept { return has_value_; }

    inline const T &value([[maybe_unused]] LdAccess mode = LdAccess::CHECK) const {
#ifdef MUE_ENABLE_ENGRAVING_LD_ACCESS
        if (!has_value_ && mode == LdAccess::CHECK) {
            LOGE_T("LD_ACCESS")() << "BAD ACCESS to: " << name_;
        }
#endif
        return val_;
    }

    inline const T &operator()() const { return value(); }

    operator T() const { return value(); }

    inline T &mut_value() noexcept {
        has_value_ = true;
        return val_;
    }

    inline void set_value(const T &v) {
        val_ = v;
        has_value_ = true;
    }

    inline void set_value(T &&v) {
        val_ = std::move(v);
        has_value_ = true;
    }

    LdField &operator=(const T &v) {
        val_ = v;
        has_value_ = true;
        return *this;
    }

    LdField &operator=(T &&v) {
        val_ = std::move(v);
        has_value_ = true;
        return *this;
    }

private:
#ifdef MUE_ENABLE_ENGRAVING_LD_ACCESS
    const char *name_ = nullptr;
#endif
    T val_;
    bool has_value_ = false;
};

template <typename T>
using ld_field = LdField<T>;
} // namespace mu::engraving

#endif // MU_ENGRAVING_LD_ACCESS_DEV_H
