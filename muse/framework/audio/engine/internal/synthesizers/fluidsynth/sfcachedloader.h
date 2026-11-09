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

#include <fluidsynth.h>
#include <sfloader/fluid_defsfont.h>
#include <sfloader/fluid_sfont.h>

#include "global/flatmap.h"

namespace muse::audio::synth {
struct SoundFontData {
    fluid_sfont_t *sound_font_ptr = nullptr;
    std::FILE *file_stream = nullptr;
};

struct SoundFontCache : FlatMap<std::string, SoundFontData> {
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

    ~SoundFontCache() { clear_unused(); }
};

struct FluidLoaderData {
    fluid_settings_t *settings = nullptr;
    SoundFontCache *cache = nullptr;
};

// Fluid's file-open callback has no user data and is also called by lazy
// sample loads after the loader returns. Borrow the owner for its full lifetime.
inline thread_local SoundFontCache *file_cache_bridge = nullptr;

inline void *open_sound_font(const char *filename) {
    auto search = file_cache_bridge->find(filename);

    if (search != file_cache_bridge->cend() && search->second.file_stream) {
        return search->second.file_stream;
    }

    std::FILE *stream = std::fopen(filename, "rb");

    SoundFontData sf_data;
    sf_data.file_stream = stream;

    file_cache_bridge->emplace(filename, std::move(sf_data));

    return stream;
}

inline int read_sound_font(void *buf, fluid_long_long_t count, void *handle) {
    return static_cast<int>(std::fread(buf, count, 1, static_cast<std::FILE *>(handle)));
}

inline int seek_sound_font(void *handle, fluid_long_long_t offset, int origin) {
    return std::fseek(static_cast<std::FILE *>(handle), offset, origin);
}

inline int close_sound_font(void * /*handle*/) {
    //!Note Prevent closing of sound-font files by Fluid instances,
    //!     instead the actual closing of cached sound-font files will happen in SoundFontCache.
    //!     However, we still need to provide "some" callback for Fluid's API

    return FLUID_OK;
}

inline fluid_long_long_t tell_sound_font(void *handle) { return std::ftell(static_cast<std::FILE *>(handle)); }

inline int delete_sound_font(fluid_sfont_t * /*sfont*/) {
    //!Note Prevent removal of sound-fonts by Fluid instances,
    //!     instead the actual removal of cached sound-fonts will happen in SoundFontCache.
    //!     However, we still need to provide "some" callback for Fluid's API

    return FLUID_OK;
}

inline fluid_file_callbacks_t file_callbacks{open_sound_font, read_sound_font, seek_sound_font, close_sound_font, tell_sound_font};

inline fluid_sfont_t *load_sound_font(fluid_sfloader_t *loader, const char *filename) {
    auto &data = *static_cast<FluidLoaderData *>(fluid_sfloader_get_data(loader));

    auto search = data.cache->find(filename);
    if (search != data.cache->cend()) {
        return search->second.sound_font_ptr;
    }

    fluid_defsfont_t *defsfont = nullptr;
    fluid_sfont_t *result = nullptr;

    defsfont = new_fluid_defsfont(data.settings);

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

    SoundFontData &sf_data = data.cache->operator[](filename);
    sf_data.sound_font_ptr = result;

    return result;
}
} // namespace muse::audio::synth

#endif // MUSE_AUDIO_SFCACHEDLOADER_H
