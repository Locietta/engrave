/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "soundfontrepository.h"

#include "fluidsynth/fluidsoundfontparser.h"
#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::synth;

bool SoundFontRepository::is_sound_font_loaded(const std::string &name) const {
    for (const auto &p : sound_fonts_) {
        if (p.second.name == name) {
            return true;
        }
    }
    return false;
}

bool SoundFontRepository::is_loading_sound_fonts() const { return loading_sound_font_count_ > 0; }

const SoundFontsMap &SoundFontRepository::sound_fonts() const { return sound_fonts_; }

void SoundFontRepository::set_on_sound_fonts_changed(std::function<void()> on_sound_fonts_changed) {
    on_sound_fonts_changed_ = std::move(on_sound_fonts_changed);
}

void SoundFontRepository::notify_sound_fonts_changed() {
    if (on_sound_fonts_changed_) {
        on_sound_fonts_changed_();
    }
}

void SoundFontRepository::add_sound_font(const SoundFontUri &uri) {

    loading_sound_font_count_++;
    do_add_sound_font(uri, nullptr, [this]() {
        --loading_sound_font_count_;
        notify_sound_fonts_changed();
    });
}

static io::PathT file_name_from_uri(const SoundFontUri &uri) {
    io::PathT path = uri.to_local_file();
    return io::filename(path);
}

void SoundFontRepository::add_sound_font_data(const SoundFontUri &uri, const ByteArray &data) {

    LOGI() << "try add sound font uri: " << uri << ", size: " << data.size();

    const io::PathT file_name = file_name_from_uri(uri);

    {
        FILE *file = fopen(file_name.c_str(), "wb");
        IF_ASSERT_FAILED(file) { return; }
        size_t wsize = fwrite(data.const_char(), sizeof(char), data.size(), file);
        IF_ASSERT_FAILED(wsize == data.size()) { return; }
        fclose(file);
    }

    RetVal<SoundFontMeta> meta = FluidSoundFontParser::parse_sound_font(file_name);

    if (meta.ret) {
        sound_fonts_.insert_or_assign(uri, std::move(meta.val));
        LOGI() << "added sound font, uri: " << uri;
    } else {
        LOGE() << "Failed parse SoundFont presets for " << file_name << ": " << meta.ret.to_string();
    }

    notify_sound_fonts_changed();
}

void SoundFontRepository::do_add_sound_font(const SoundFontUri &uri, const SoundFontsMap *cache, std::function<void()> on_finished) {
    if (cache) {
        auto it = cache->find(uri);
        if (it != cache->cend()) {
            sound_fonts_.insert(*it);
            if (on_finished) {
                on_finished();
            }
            return;
        }
    }

    auto parse_and_add = [this, on_finished](const SoundFontUri &uri, const SoundFontPath &path) {
        RetVal<SoundFontMeta> meta = FluidSoundFontParser::parse_sound_font(path);

        if (meta.ret) {
            sound_fonts_.insert_or_assign(uri, std::move(meta.val));
            LOGI() << "added sound font, uri: " << uri;
        } else {
            LOGE() << "Failed parse SoundFont presets for " << path << ": " << meta.ret.to_string();
        }

        if (on_finished) {
            on_finished();
        }
    };

    if (uri.scheme() == "file") {
        SoundFontPath path = uri.to_local_file();
        parse_and_add(uri, path);
    } else {
        MUSE_NOT_SUPPORTED;
        if (on_finished) {
            on_finished();
        }
    }
}

void SoundFontRepository::load_sound_fonts(const std::vector<SoundFontUri> &uris) {

    SoundFontsMap cache = std::move(sound_fonts_);
    sound_fonts_ = {};
    // Native loading is synchronous. Notify once after replacing the repository,
    // including an empty selection; no callback needs to retain the old cache.
    for (const SoundFontUri &uri : uris) {
        do_add_sound_font(uri, &cache);
    }
    notify_sound_fonts_changed();
}
