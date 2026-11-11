/*
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <memory>

namespace mu::engraving {
struct EngravingConfiguration;
struct EngravingContextConfiguration;
struct EngravingFontsProvider;

namespace rendering::score {
struct ScoreRenderer;
} // namespace rendering::score

// The engine services a score uses. The engine context owns the only instance
// and outlives every score built with it; scores keep a reference.
struct EngravingServices {
    // Borrowed from the context; null only during initialization or teardown.
    EngravingConfiguration *configuration = nullptr;
    EngravingFontsProvider *engraving_fonts = nullptr;
    EngravingContextConfiguration *context_configuration = nullptr;
    rendering::score::ScoreRenderer *renderer = nullptr;
};
} // namespace mu::engraving
