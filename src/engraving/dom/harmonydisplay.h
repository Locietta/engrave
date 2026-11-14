// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "harmony.h"

namespace mu::engraving {
// Callers resolve the source key, including a mapped origin for display copies.
String harmony_display_name(const Harmony &harmony, Key source_key, std::vector<std::vector<Harmony::DisplayAction>> *output = nullptr);
} // namespace mu::engraving
