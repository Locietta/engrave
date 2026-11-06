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

#ifndef MUSE_AUDIO_SFCACHEDLOADER_H
#define MUSE_AUDIO_SFCACHEDLOADER_H

#include <cstdio>

#include <sfloader/fluid_sfont.h>
#include <sfloader/fluid_defsfont.h>

namespace muse::audio::synth {
struct SoundFontData {
    fluid_sfont_t *sound_font_ptr = nullptr;
    std::FILE *file_stream = nullptr;
};

struct SoundFontCache : public std::map<std::string, SoundFontData> {
    static SoundFontCache *instance() {
        static SoundFontCache s;
        return &s;
    }

    // Called only after all synthesizers owned by the engine have gone away.
    // A font still in use stays cached and can be released on a later attempt.
    void clear_unused() {
        for (auto it = begin(); it != end();) {
            SoundFontData &data = it->second;
            if (data.sound_font_ptr) {
                auto *defs_font = static_cast<fluid_defsfont_t *>(fluid_sfont_get_data(data.sound_font_ptr));
                if (delete_fluid_defsfont(defs_font) != FLUID_OK) {
                    ++it;
                    continue;
                }
                delete_fluid_sfont(data.sound_font_ptr);
            }
            if (data.file_stream) std::fclose(data.file_stream);
            it = erase(it);
        }
    }

private:
    SoundFontCache() = default;

    ~SoundFontCache() { clear_unused(); }
};

void *open_sound_font(const char *filename) {
    auto search = SoundFontCache::instance()->find(filename);

    if (search != SoundFontCache::instance()->cend() && search->second.file_stream) {
        return search->second.file_stream;
    }

    std::FILE *stream = std::fopen(filename, "rb");

    SoundFontData sf_data;
    sf_data.file_stream = stream;

    SoundFontCache::instance()->emplace(filename, std::move(sf_data));

    return stream;
}

int read_sound_font(void *buf, fluid_long_long_t count, void *handle) {
    return static_cast<int>(std::fread(buf, count, 1, static_cast<std::FILE *>(handle)));
}

int seek_sound_font(void *handle, fluid_long_long_t offset, int origin) {
    return std::fseek(static_cast<std::FILE *>(handle), offset, origin);
}

int close_sound_font(void * /*handle*/) {
    //!Note Prevent closing of sound-font files by Fluid instances,
    //!     instead the actual closing of cached sound-font files will happen in SoundFontCache.
    //!     However, we still need to provide "some" callback for Fluid's API

    return FLUID_OK;
}

fluid_long_long_t tell_sound_font(void *handle) { return std::ftell(static_cast<std::FILE *>(handle)); }

int delete_sound_font(fluid_sfont_t * /*sfont*/) {
    //!Note Prevent removal of sound-fonts by Fluid instances,
    //!     instead the actual removal of cached sound-fonts will happen in SoundFontCache.
    //!     However, we still need to provide "some" callback for Fluid's API

    return FLUID_OK;
}

static fluid_file_callbacks_t file_callbacks{open_sound_font, read_sound_font, seek_sound_font, close_sound_font, tell_sound_font};

fluid_sfont_t *load_sound_font(fluid_sfloader_t *loader, const char *filename) {
    auto search = SoundFontCache::instance()->find(filename);
    if (search != SoundFontCache::instance()->cend()) {
        return search->second.sound_font_ptr;
    }

    fluid_defsfont_t *defsfont = nullptr;
    fluid_sfont_t *result = nullptr;

    defsfont = new_fluid_defsfont(static_cast<fluid_settings_t *>(fluid_sfloader_get_data(loader)));

    if (!defsfont) {
        return nullptr;
    }

    result = new_fluid_sfont(fluid_defsfont_sfont_get_name, fluid_defsfont_sfont_get_preset, fluid_defsfont_sfont_iteration_start,
                             fluid_defsfont_sfont_iteration_next, delete_sound_font);

    if (!result) {
        return result;
    }

    fluid_sfont_set_data(result, defsfont);
    defsfont->sfont = result;
    defsfont->fcbs = &file_callbacks;

    if (fluid_defsfont_load(defsfont, &file_callbacks, filename) == FLUID_FAILED) {
        fluid_defsfont_sfont_delete(result);
        return nullptr;
    }

    SoundFontData &sf_data = SoundFontCache::instance()->operator[](filename);
    sf_data.sound_font_ptr = result;

    return result;
}
} // namespace muse::audio::synth

#endif // MUSE_AUDIO_SFCACHEDLOADER_H
