#pragma once

#include <utility>

#include "global/fn.h"
#include "global/unique_fn.h"

namespace music {

using muse::FnRef;
using muse::UniqueFn;

template <class Sig>
UniqueFn<Sig> make_unique_fn(auto &&fn) {
    return UniqueFn<Sig>(std::forward<decltype(fn)>(fn));
}

} // namespace music
