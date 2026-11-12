// Probe before test-framework headers: they can also import floating-point abs.
#include <cstdlib>

using AbsBeforeLayoutHeaders = decltype(::abs(0.5));

#include "engraving/rendering/score/slurtielayout.h"

using AbsAfterLayoutHeaders = decltype(::abs(0.5));

#include <type_traits>

// A layout implementation include must not change the arithmetic overload set
// of clients. In particular, GCC's SIMD headers import std::abs into ::.
static_assert(std::is_same_v<AbsBeforeLayoutHeaders, AbsAfterLayoutHeaders>, "Layout headers changed global abs overload resolution");
