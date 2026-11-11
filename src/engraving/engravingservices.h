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
    std::shared_ptr<EngravingConfiguration> configuration;
    std::shared_ptr<EngravingFontsProvider> engraving_fonts;
    std::shared_ptr<EngravingContextConfiguration> context_configuration;
    std::shared_ptr<rendering::score::ScoreRenderer> renderer;
    // Null until the palette score exists (it is built with these services).
};
} // namespace mu::engraving
