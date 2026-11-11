// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include "draw/internal/fontsengine.h"

namespace music {
struct RuntimeFontsOwner {
    std::thread::id owner = std::this_thread::get_id();
    bool installed = true;

    void check() const {
        if (!installed) {
            throw std::runtime_error("Runtime font services are not installed");
        }
        if (std::this_thread::get_id() != owner) {
            throw std::runtime_error("Runtime font services called from a non-owner thread");
        }
    }
};

struct RuntimeFontsEngine {
    RuntimeFontsEngine(const RuntimeFontsOwner &owner, std::shared_ptr<void> cache_store, std::unique_ptr<muse::draw::FontsEngine> engine)
        : owner_(owner), cache_store_(std::move(cache_store)), engine_(std::move(engine)) {}

    ~RuntimeFontsEngine() {
        engine_.reset();
        cache_store_.reset();
    }

    // The caller has already checked ownership and prepared the new engine.
    // Replacing the engine drops every upstream cached face, so replacements
    // and reactivations cannot keep using the old face.
    void set_engine(std::unique_ptr<muse::draw::FontsEngine> engine) noexcept { engine_ = std::move(engine); }

    // The render cache is optional (FontRenderCache is a no-op without a
    // directory); forward it so a caller can opt in.
    void set_render_cache_dir_path(const muse::io::PathT &path, const std::string &revision = std::string()) {
        owner_.check();
        if (!engine_) {
            throw std::runtime_error("Runtime font engine is not initialized");
        }
        engine_->set_render_cache_dir_path(path, revision);
    }

    void clear_loaded_faces() {
        owner_.check();
        if (engine_) {
            engine_->clear_loaded_faces();
        }
    }

    double line_spacing(const muse::draw::Font &f) const { return engine()->line_spacing(f); }

    double x_height(const muse::draw::Font &f) const { return engine()->x_height(f); }

    double height(const muse::draw::Font &f) const { return engine()->height(f); }

    double cap_height(const muse::draw::Font &f) const { return engine()->cap_height(f); }

    double ascent(const muse::draw::Font &f) const { return engine()->ascent(f); }

    double descent(const muse::draw::Font &f) const { return engine()->descent(f); }

    double underline_pos(const muse::draw::Font &f) const { return engine()->underline_pos(f); }

    double line_width(const muse::draw::Font &f) const { return engine()->line_width(f); }

    double strike_out_pos(const muse::draw::Font &f) const { return engine()->strike_out_pos(f); }

    bool in_font(const muse::draw::Font &f, char32_t ucs4) const { return engine()->in_font(f, ucs4); }

    double horizontal_advance(const muse::draw::Font &f, const char32_t &ch) const { return engine()->horizontal_advance(f, ch); }

    double horizontal_advance(const muse::draw::Font &f, const std::u32string &text) const { return engine()->horizontal_advance(f, text); }

    muse::RectF bounding_rect(const muse::draw::Font &f, const char32_t &ch) const { return engine()->bounding_rect(f, ch); }

    muse::RectF bounding_rect(const muse::draw::Font &f, const std::u32string &text) const { return engine()->bounding_rect(f, text); }

    muse::RectF tight_bounding_rect(const muse::draw::Font &f, const std::u32string &text) const {
        return engine()->tight_bounding_rect(f, text);
    }

    std::vector<muse::draw::GlyphImage> render(const muse::draw::Font &f, const std::u32string &text) const {
        return engine()->render(f, text);
    }

private:
    const muse::draw::FontsEngine *engine() const {
        owner_.check();
        if (!engine_) {
            throw std::runtime_error("Runtime font engine is not initialized");
        }
        return engine_.get();
    }

    const RuntimeFontsOwner &owner_;
    std::shared_ptr<void> cache_store_;
    std::unique_ptr<muse::draw::FontsEngine> engine_;
};
} // namespace music
