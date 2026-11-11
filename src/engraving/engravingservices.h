/*
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <memory>

namespace muse::draw {
struct IImageProvider;
} // namespace muse::draw

namespace mu::engraving {
struct IEngravingConfiguration;
struct EngravingContextConfiguration;
struct IEngravingFontsProvider;

namespace rendering {
struct IScoreRenderer;
} // namespace rendering

// The engine services a score uses. The engine context owns the only instance
// and outlives every score built with it; scores keep a reference.
struct EngravingServices {
    std::shared_ptr<IEngravingConfiguration> configuration;
    std::shared_ptr<IEngravingFontsProvider> engraving_fonts;
    std::shared_ptr<EngravingContextConfiguration> context_configuration;
    std::shared_ptr<rendering::IScoreRenderer> renderer;
    // Raster image codec and thumbnails. The engine has none: it rejects scores
    // with raster images when they are opened, so the paths that need it are
    // not reached.
    std::shared_ptr<muse::draw::IImageProvider> image_provider;
    // Null until the palette score exists (it is built with these services).
};
} // namespace mu::engraving
