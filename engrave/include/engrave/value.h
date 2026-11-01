#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace engrave {

// The unit a numeric value is expressed in.  The value codec derives None, Spatium
// and Absolute from the property value type; Millimetre and Degree are vocabulary
// for the per-property units a later slice can add.
enum struct Unit {
    None,
    Spatium,   // staff spaces
    Absolute,  // 1/1200 inch: the engine's DPI (a millimetre is DPMM of these)
    Millimetre,
    Degree,
};

// The shape of a property or style value.  One per Value alternative.
enum struct ValueKind { Bool, Int, Real, String, Point, Size, Color, Fraction, Enum, IntList, Opaque };

namespace detail {

// Compare positive-denominator fractions without multiplying their terms. Each
// reciprocal reverses the order; unsigned magnitudes also cover INT64_MIN.
constexpr std::strong_ordering compare_fraction(std::int64_t a, std::int64_t b,
                                                std::int64_t c, std::int64_t d) {
    if (a < 0 && c >= 0) return std::strong_ordering::less;
    if (a >= 0 && c < 0) return std::strong_ordering::greater;
    const auto magnitude = [](std::int64_t n) {
        const auto bits = static_cast<std::uint64_t>(n);
        return n < 0 ? std::uint64_t{0} - bits : bits;
    };
    std::uint64_t an = magnitude(a), ad = static_cast<std::uint64_t>(b);
    std::uint64_t cn = magnitude(c), cd = static_cast<std::uint64_t>(d);
    bool reversed = a < 0;
    for (;;) {
        const auto aq = an / ad, cq = cn / cd;
        if (aq != cq) {
            const bool less = (aq < cq) != reversed;
            return less ? std::strong_ordering::less : std::strong_ordering::greater;
        }
        const auto ar = an % ad, cr = cn % cd;
        if (ar == 0 || cr == 0) {
            if (ar == cr) return std::strong_ordering::equal;
            const bool less = (ar == 0) != reversed;
            return less ? std::strong_ordering::less : std::strong_ordering::greater;
        }
        an = ad;
        ad = ar;
        cn = cd;
        cd = cr;
        reversed = !reversed;
    }
}

} // namespace detail

// A fraction of a whole note, also used for written time signatures. The
// denominator is positive and the terms are not necessarily reduced. Ordering
// and equality compare values; inspect num and den to distinguish notation.
struct Fraction {
    std::int64_t num = 0;
    std::int64_t den = 1;

    constexpr std::strong_ordering operator<=>(const Fraction& other) const {
        // Invalid denominators are outside the API contract. Keep them safely
        // ordered by their stored terms, before valid fractions.
        if (den <= 0 || other.den <= 0) {
            if (den > 0) return std::strong_ordering::greater;
            if (other.den > 0) return std::strong_ordering::less;
            if (const auto by_num = num <=> other.num; by_num != 0) return by_num;
            return den <=> other.den;
        }
        return detail::compare_fraction(num, den, other.num, other.den);
    }

    constexpr bool operator==(const Fraction& other) const { return (*this <=> other) == 0; }
};

struct Point {
    double x = 0;
    double y = 0;

    bool operator==(const Point&) const = default;
};

struct Size {
    double w = 0;
    double h = 0;

    bool operator==(const Size&) const = default;
};

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    bool operator==(const Color&) const = default;
};

// An enum value as its MSCX token: the text the engine's TConv writes for it, so
// the vocabulary is the one of a saved score and survives a sync.  A few enums
// have no token list (SYMID's set is open, Align and OrnamentInterval build theirs
// from two tokens, AccidentalRole and HookType use numbers); their tokens are still
// accepted and produced.
struct Enum {
    std::string token;

    bool operator==(const Enum&) const = default;
};

// A value the engine keeps as an object and the API does not map: read-only.
// type is the property value type's name, e.g. "draw_path".
struct Opaque {
    std::string type;

    bool operator==(const Opaque&) const = default;
};

// The value of one property or style.  A default-constructed Value is
// std::monostate: no value at all.
using Value = std::variant<std::monostate, bool, std::int64_t, double, std::string, Fraction, Point, Size, Color,
                           Enum, std::vector<std::int64_t>, Opaque>;

} // namespace engrave
