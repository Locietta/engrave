// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <functional>

#include <amc/flatset.hpp>

namespace muse {

template <class Key, class Compare = std::less<Key>>
using FlatSet = amc::FlatSet<Key, Compare>;

} // namespace muse
