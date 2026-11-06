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
#include "fontrendercache.h"

#include <array>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

#include "global/io/dir.h"
#include "global/io/file.h"
#include "global/io/fileinfo.h"
#include "global/stringutils.h"
#include "log.h"

using namespace muse::draw;

void FontRenderCache::init() {
    cache_.clear();
    cache_info_loaded_ = false;
    cache_info_map_.clear();

    if (!cache_dir_path_.empty()) {
        muse::io::Dir::mkpath(cache_dir_path_);
        update_cache_revision();
    }
}

void FontRenderCache::set_cache_dir_path(const muse::io::PathT &path, const std::string &revision) {
    cache_dir_path_ = path;
    cache_revision_ = revision;
    cache_.clear();
    cache_info_loaded_ = false;
    cache_info_map_.clear();

    if (!cache_dir_path_.empty()) {
        muse::io::Dir::mkpath(cache_dir_path_);
        update_cache_revision();
    }
}

muse::io::PathT FontRenderCache::revision_file_path() const { return cache_dir_path_.appending_component("revision"); }

void FontRenderCache::update_cache_revision() {
    if (cache_revision_.empty()) {
        return;
    }

    bool is_need_clear = true;
    const muse::io::PathT revision_path = revision_file_path();
    if (muse::io::File::exists(revision_path)) {
        muse::io::File revision_file(revision_path);
        if (revision_file.open(muse::io::IODevice::READ_ONLY)) {
            const muse::ByteArray data = revision_file.read_all();
            is_need_clear = std::string_view(data.const_char(), data.size()) != cache_revision_;
        }
    }

    if (is_need_clear) {
        muse::RetVal<muse::io::paths_t> files = muse::io::Dir::scan_files(cache_dir_path_, {}, muse::io::ScanMode::FILES_IN_CURRENT_DIR);
        if (!files.ret) {
            LOGE() << "failed scan font render cache dir: " << cache_dir_path_ << ", error: " << files.ret.to_string();
            return;
        }

        bool is_cleared = true;
        for (const muse::io::PathT &file_path : files.val) {
            if (!muse::io::File::remove(file_path)) {
                LOGE() << "failed remove font render cache file: " << file_path;
                is_cleared = false;
            }
        }

        cache_info_loaded_ = false;
        cache_info_map_.clear();

        if (!is_cleared) {
            return;
        }
    }

    if (!muse::io::File::write_file(revision_path, muse::ByteArray(cache_revision_.c_str())).success()) {
        LOGE() << "failed write cache revision file: " << revision_path;
    }
}

static std::string real_to_string(double n) {
    std::ostringstream stream;
    stream << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10) << n;
    return stream.str();
}

enum class CacheInfoParam : size_t { WIDTH = 0, HEIGHT, RECT_X, RECT_Y, RECT_WIDTH, RECT_HEIGHT, RANGE, COUNT };

static constexpr size_t cache_info_param_count = static_cast<size_t>(CacheInfoParam::COUNT);

static constexpr size_t param_index(CacheInfoParam param) { return static_cast<size_t>(param); }

static std::string key_to_string(const FaceKey &face, glyph_idx_t glyph_idx) {
    std::string str;
    str.reserve(50);
    str += face.data_key.family().id().to_std_string();
    str += "_" + std::to_string(glyph_idx);
    str += "_" + std::to_string(face.data_key.bold());
    str += "_" + std::to_string(face.data_key.italic());
    str += "_" + std::to_string(face.pixel_size);
    return str;
}

static std::array<std::string, cache_info_param_count> cache_info_params(const GlyphImage &image) {
    return {
        std::to_string(image.sdf.width), std::to_string(image.sdf.height),   real_to_string(image.rect.x()),
        real_to_string(image.rect.y()),  real_to_string(image.rect.width()), real_to_string(image.rect.height()),
        real_to_string(image.range),
    };
}

static std::string cache_info_to_string(const GlyphImage &image) {
    std::string str;
    const std::array<std::string, cache_info_param_count> params = cache_info_params(image);
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            str += "|";
        }
        str += params.at(i);
    }
    return str;
}

muse::io::PathT FontRenderCache::make_file_path(const FaceKey &face, glyph_idx_t glyph_idx, const GlyphImage &image) const {
    std::string str;
    str.reserve(100);
    str += key_to_string(face, glyph_idx);
    str += "_[" + cache_info_to_string(image);
    str += "].sdf";

    return muse::io::PathT(str);
}

void FontRenderCache::store(const FaceKey &face, glyph_idx_t glyph_idx, const GlyphImage &image) {
    cache_[face][glyph_idx] = image;

    if (cache_dir_path_.empty()) {
        return;
    }

    muse::io::PathT file_full_path = cache_dir_path_.appending_component(make_file_path(face, glyph_idx, image));
    muse::io::File file(file_full_path);
    if (!file.open(muse::io::IODevice::WRITE_ONLY)) {
        LOGE() << "failed open to write file: " << file_full_path;
        return;
    }

    file.write(image.sdf.bitmap);
}

GlyphImage FontRenderCache::load(const FaceKey &face, glyph_idx_t glyph_idx) const {
    auto face_it = cache_.find(face);
    if (face_it != cache_.end()) {
        const GlyphImages &images = face_it->second;
        auto image_it = images.find(glyph_idx);
        if (image_it != images.end()) {
            return image_it->second;
        }
    }

    if (!cache_info_loaded_) {
        if (!cache_dir_path_.empty()) {
            load_cached_info(cache_info_map_, cache_dir_path_);
        }
        cache_info_loaded_ = true;
    }

    auto cache_info_it = cache_info_map_.find(key_to_string(face, glyph_idx));
    if (cache_info_it == cache_info_map_.end()) {
        return GlyphImage();
    }

    const CacheInfo &info = cache_info_it->second;
    muse::io::File file(info.file_path);
    if (!file.open(muse::io::IODevice::READ_ONLY)) {
        LOGE() << "failed read file: " << file.file_path();
        return GlyphImage();
    }

    GlyphImage image;
    image.sdf.bitmap = file.read_all();
    image.sdf.width = info.width;
    image.sdf.hash = std::hash<std::string_view>{}({reinterpret_cast<const char *>(image.sdf.bitmap.data()), image.sdf.bitmap.size()});
    image.sdf.height = info.height;
    image.rect = info.rect;
    image.range = info.range;

    cache_[face][glyph_idx] = image;

    return image;
}

void FontRenderCache::load_cached_info(CacheInfoMap &map, const muse::io::PathT &dir) const {
    muse::RetVal<muse::io::paths_t> files = muse::io::Dir::scan_files(dir, {}, muse::io::ScanMode::FILES_IN_CURRENT_DIR);
    for (const muse::io::PathT &p : files.val) {
        std::string str = muse::io::FileInfo(p).complete_base_name().to_std_string();

        size_t start_info_idx = str.find('[');
        if (start_info_idx == std::string::npos) {
            continue;
        }

        size_t end_info_idx = str.find(']');
        if (end_info_idx == std::string::npos) {
            continue;
        }

        std::string key = str.substr(0, start_info_idx - 1);
        std::string data = str.substr(start_info_idx + 1, end_info_idx - start_info_idx - 1);
        std::vector<std::string> params;
        muse::strings::split(data, params, "|");
        if (params.size() != ::cache_info_param_count) {
            continue;
        }

        bool ok = false;

        int width = muse::AsciiStringView(params.at(param_index(CacheInfoParam::WIDTH))).to_int(&ok);
        if (!ok || width < 0) {
            LOGW() << "failed parse font render cache info from file: " << p;
            continue;
        }

        int height = muse::AsciiStringView(params.at(param_index(CacheInfoParam::HEIGHT))).to_int(&ok);
        if (!ok || height < 0) {
            LOGW() << "failed parse font render cache info from file: " << p;
            continue;
        }

        double rect_x = muse::AsciiStringView(params.at(param_index(CacheInfoParam::RECT_X))).to_double(&ok);
        bool is_parsed = ok;
        double rect_y = muse::AsciiStringView(params.at(param_index(CacheInfoParam::RECT_Y))).to_double(&ok);
        is_parsed = is_parsed && ok;
        double rect_width = muse::AsciiStringView(params.at(param_index(CacheInfoParam::RECT_WIDTH))).to_double(&ok);
        is_parsed = is_parsed && ok;
        double rect_height = muse::AsciiStringView(params.at(param_index(CacheInfoParam::RECT_HEIGHT))).to_double(&ok);
        is_parsed = is_parsed && ok;
        double range = muse::AsciiStringView(params.at(param_index(CacheInfoParam::RANGE))).to_double(&ok);
        is_parsed = is_parsed && ok;

        if (!is_parsed) {
            LOGW() << "failed parse font render cache info from file: " << p;
            continue;
        }

        CacheInfo info;
        info.file_path = p;
        info.width = static_cast<uint32_t>(width);
        info.height = static_cast<uint32_t>(height);
        info.rect = RectF(rect_x, rect_y, rect_width, rect_height);
        info.range = range;

        map[key] = info;
    }
}
