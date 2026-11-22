#pragma once

#include <memory>

namespace music {
struct RuntimeFontsDatabase;
} // namespace music

namespace muse::draw {

// Used by painters on the runtime's owner thread.
struct CairoFontCache {
    explicit CairoFontCache(const music::RuntimeFontsDatabase &fonts);
    ~CairoFontCache();

    CairoFontCache(const CairoFontCache &) = delete;
    CairoFontCache &operator=(const CairoFontCache &) = delete;

private:
    friend struct CairoPainter;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace muse::draw
