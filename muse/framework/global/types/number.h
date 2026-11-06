#ifndef MUSE_GLOBAL_NUMBER_T_H
#define MUSE_GLOBAL_NUMBER_T_H

#include <limits>
#include <cassert>
#include <cmath>

#include "global/logstream.h"

namespace muse {
// default
inline constexpr int compare_real_prec(7);

inline constexpr double pow_minus10(int prec) {
    double result = 1.0;
    for (int i = 0; i < prec; ++i) {
        result /= 10;
    }
    return result;
}

// use
inline constexpr double compare_real_epsilon = pow_minus10(::muse::compare_real_prec);

template <typename T>
inline constexpr bool is_zero(T v) {
    if constexpr (std::numeric_limits<T>::is_integer) {
        return v == 0;
    } else {
        return std::abs(v) <= ::muse::compare_real_epsilon;
    }
}

template <typename T>
inline constexpr bool is_equal(T v1, T v2) {
    if constexpr (std::numeric_limits<T>::is_integer) {
        return v1 == v2;
    } else {
        return std::abs(v1 - v2) <= std::max(std::abs(v1), std::abs(v2)) * ::muse::compare_real_epsilon;
    }
}

template <typename T>
inline T check_valid(T v, T def = T()) {
    if constexpr (std::numeric_limits<T>::is_integer) {
        return v;
    } else {
        bool valid = !std::isnan(v) && !std::isinf(v);
        if (!valid) {
            assert(!std::isnan(v) && !std::isinf(v));
            return def;
        }
        return v;
    }
}

template <typename T>
class NumberT {
public:

    using value_type = T;

    constexpr NumberT() = default;

    NumberT(T v) : val_(muse::check_valid(v)) {}

    //! NOTE At the moment, not all compilers have std::is_nan is constexpr,
    //! so we can't make check_valid is constexpr,
    //! so we can't make a copy constructor is constexpr.
    //! This function is a helper for creating constexpr variables.
    static constexpr NumberT<T> make(T v) {
        NumberT<T> n;
        n.val_ = v;
        return n;
    }

    NumberT<T> &operator=(T v) {
        val_ = muse::check_valid(v);
        return *this;
    }

    inline operator T() const { return val_; }

    template <typename V>
    static T cast(V v) {
        return static_cast<T>(v);
    }

    inline constexpr T raw() const { return val_; }

    inline T &raw() { return val_; }

    inline double to_double() const { return static_cast<double>(val_); }

    inline NumberT<T> operator-() const { return NumberT<T>(-val_); }

    inline bool is_zero() const { return muse::is_zero(val_); }

    inline bool is_negative() const { return val_ < static_cast<T>(0); }

    inline bool is_positive() const { return val_ > static_cast<T>(0); }

    inline bool operator==(const NumberT<T> &n) const { return muse::is_equal(val_, n.val_); }

    inline bool operator!=(const NumberT<T> &n) const { return !muse::is_equal(val_, n.val_); }

    inline bool operator==(T n) const { return muse::is_equal(val_, n); }

    inline bool operator!=(T n) const { return !muse::is_equal(val_, n); }

    inline bool operator>(const NumberT<T> &n) const { return val_ > n.val_; }

    inline bool operator<(const NumberT<T> &n) const { return val_ < n.val_; }

    inline bool operator>(T n) const { return val_ > n; }

    inline bool operator<(T n) const { return val_ < n; }

    inline bool operator>=(const NumberT<T> &n) const { return val_ > n.val_ || muse::is_equal(val_, n.val_); }

    inline bool operator<=(const NumberT<T> &n) const { return val_ < n.val_ || muse::is_equal(val_, n.val_); }

    inline bool operator>=(T n) const { return val_ > n || muse::is_equal(val_, n); }

    inline bool operator<=(T n) const { return val_ < n || muse::is_equal(val_, n); }

    inline NumberT<T> operator+(const NumberT<T> &n) const { return NumberT<T>(val_ + n.val_); }

    inline NumberT<T> operator-(const NumberT<T> &n) const { return NumberT<T>(val_ - n.val_); }

    inline NumberT<T> operator+(T n) const { return NumberT<T>(val_ + n); }

    inline NumberT<T> operator-(T n) const { return NumberT<T>(val_ - n); }

    inline NumberT<T> &operator+=(const NumberT<T> &n) {
        val_ += muse::check_valid(n.val_);
        return *this;
    }

    inline NumberT<T> &operator-=(const NumberT<T> &n) {
        val_ -= muse::check_valid(n.val_);
        return *this;
    }

    inline NumberT<T> &operator+=(T n) {
        val_ += muse::check_valid(n);
        return *this;
    }

    inline NumberT<T> &operator-=(T n) {
        val_ -= muse::check_valid(n);
        return *this;
    }

    inline NumberT<T> operator/(const NumberT<T> &n) const { return NumberT<T>(val_ / n.val_); }

    inline NumberT<T> operator*(const NumberT<T> &n) const { return NumberT<T>(val_ * n.val_); }

    inline NumberT<T> operator/(T n) const { return NumberT<T>(val_ / n); }

    inline NumberT<T> operator*(T n) const { return NumberT<T>(val_ * n); }

    inline NumberT<T> &operator/=(const NumberT<T> &n) {
        val_ /= muse::check_valid(n.val_, T(1));
        return *this;
    }

    inline NumberT<T> &operator*=(const NumberT<T> &n) {
        val_ *= muse::check_valid(n.val_);
        return *this;
    }

    inline NumberT<T> &operator/=(T n) {
        val_ /= muse::check_valid(n, T(1));
        return *this;
    }

    inline NumberT<T> &operator*=(T n) {
        val_ *= muse::check_valid(n);
        return *this;
    }

protected:

    T val_ = T();
};

template <typename T>
inline NumberT<T> operator*(T a, const NumberT<T> &b) {
    return NumberT<T>(a * b.raw());
}

using integer_t = NumberT<int>;
using real_t = NumberT<double>;

template <typename T>
const T &min(const T &a, const T &b) {
    return std::min(a, b);
}

template <typename T>
const T &min(const NumberT<T> &a, const T &b) {
    return std::min(a.raw(), b);
}

template <typename T>
const T &min(const T &a, const NumberT<T> &b) {
    return std::min(a, b.raw());
}

template <typename T>
const T &max(const T &a, const T &b) {
    return std::max(a, b);
}

template <typename T>
const T &max(const NumberT<T> &a, const T &b) {
    return std::max(a.raw(), b);
}

template <typename T>
const T &max(const T &a, const NumberT<T> &b) {
    return std::max(a, b.raw());
}

template <typename T>
inline T divide(const T &dividend, const T &divisor) {
    if (is_zero(divisor)) {
        assert(!is_zero(divisor));
        return T();
    }
    return dividend / divisor;
}

#if (defined(_MSCVER) || defined(_MSC_VER))
#pragma warning(push)
#pragma warning(disable : 4723) // potential divide by 0
#endif
template <typename T>
inline T divide(const T &dividend, const T &divisor, const T &def) {
    if (is_zero(divisor)) {
        return def;
    }
    return dividend / divisor;
}

#if (defined(_MSCVER) || defined(_MSC_VER))
#pragma warning(pop)
#endif

template <typename T>
inline NumberT<T> divide(const NumberT<T> &dividend, const NumberT<T> &divisor, const NumberT<T> &def) {
    if (divisor.is_zero()) {
        return def;
    }
    return dividend / divisor;
}
} // namespace muse

template <typename T>
inline muse::logger::Stream &operator<<(muse::logger::Stream &s, const muse::NumberT<T> &n) {
    s << n.raw();
    return s;
}

#endif // MUSE_GLOBAL_NUMBER_T_H
