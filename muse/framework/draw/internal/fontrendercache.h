/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "global/io/path.h"
#include "types/fontstypes.h"

namespace muse::draw {
struct FontRenderCache {
    void init();
    void set_cache_dir_path(const io::PathT &path, const std::string &revision = std::string());

    void store(const FaceKey &face, glyph_idx_t glyph_idx, const GlyphImage &image);
    GlyphImage load(const FaceKey &face, glyph_idx_t glyph_idx) const;

private:
    io::PathT revision_file_path() const;
    io::PathT make_file_path(const FaceKey &face, glyph_idx_t glyph_idx, const GlyphImage &image) const;
    void update_cache_revision();

    using GlyphImages = std::unordered_map<glyph_idx_t, GlyphImage>;

    mutable std::map<FaceKey, GlyphImages> cache_;
    io::PathT cache_dir_path_;
    std::string cache_revision_;

    struct CacheInfo {
        io::PathT file_path;
        uint32_t width = 0;
        uint32_t height = 0;
        RectF rect;
        float range = 0.0f;
    };

    using CacheInfoMap = std::map<std::string, CacheInfo>;

    void load_cached_info(CacheInfoMap &map, const io::PathT &dir) const;

    mutable bool cache_info_loaded_ = false;
    mutable CacheInfoMap cache_info_map_;
};
} // namespace muse::draw
