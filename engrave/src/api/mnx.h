// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <expected>
#include <string>

#include "engrave/src/runtime/score_io.h"

namespace engrave {
// Imports an MNX document. Malformed input (invalid JSON, a schema violation or a semantic
// error) comes back as the error message; it does not throw.
std::expected<music::ScoreProject, std::string> open_mnx(music::EngineContext &context, const std::string &bytes);
} // namespace engrave