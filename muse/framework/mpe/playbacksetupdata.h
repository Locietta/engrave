/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#ifndef MUSE_MPE_PLAYBACKSETUPDATA_H
#define MUSE_MPE_PLAYBACKSETUPDATA_H

#include <optional>

#include "soundid.h"

namespace muse::mpe {
struct PlaybackSetupData {
    String id;
    StringList sub_categories;

    std::optional<std::string> score_id;
    std::optional<std::string> music_xml_sound_id;

    SoundCategory category = SoundCategory::UNDEFINED;

    bool supports_single_note_dynamics = false;

    PlaybackSetupData() = default;

    PlaybackSetupData(SoundId id, SoundCategory category, SoundSubCategories &&sound_sub_categories = {}, bool supports_snd = false)
        : id(sound_id_to_string(id)), category(category), supports_single_note_dynamics(supports_snd) {
        for (SoundSubCategory sub_category : sound_sub_categories) {
            sub_categories.push_back(sound_sub_category_to_string(sub_category));
        }
    }

    PlaybackSetupData(String id, SoundCategory category, StringList &&sub_categories = {})
        : id(std::move(id)), sub_categories(std::move(sub_categories)), category(category) {}

    SoundId sound_id() const { return sound_id_from_string(id); }

    SoundSubCategories sound_sub_categories() const {
        SoundSubCategories result;
        for (const String &sub_category : sub_categories) {
            result.insert(sound_sub_category_from_string(sub_category));
        }

        return result;
    }

    bool is_known_sound() const {
        if (sound_id_from_string(id) == SoundId::UNKNOWN) {
            return false;
        }

        for (const String &sub_category : sub_categories) {
            if (sound_sub_category_from_string(sub_category) == SoundSubCategory::UNKNOWN) {
                return false;
            }
        }

        return true;
    }

    bool contains(const SoundSubCategory subcategory) const {
        return muse::contains(sub_categories, sound_sub_category_to_string(subcategory));
    }

    void add(const SoundSubCategory subcategory) {
        auto insert_pos = std::find_if(sub_categories.cbegin(), sub_categories.cend(), [subcategory](const String &str) {
            SoundSubCategory existing_sub_category = sound_sub_category_from_string(str);
            return subcategory < existing_sub_category;
        });

        sub_categories.insert(insert_pos, sound_sub_category_to_string(subcategory));
    }

    bool operator==(const PlaybackSetupData &other) const {
        return id == other.id && category == other.category && sub_categories == other.sub_categories &&
               supports_single_note_dynamics == other.supports_single_note_dynamics;
    }

    bool operator<(const PlaybackSetupData &other) const {
        if (other.id > id) {
            return true;
        } else if (other.id == id) {
            if (other.category > category) {
                return true;
            } else if (other.category == category) {
                return other.sub_categories > sub_categories;
            }
        }

        return false;
    }

    bool is_valid() const {
        if (category == SoundCategory::UNDEFINED) {
            return false;
        }

        if (id.empty()) {
            return false;
        }

        if (id == ::muse::mpe::id_strings.at(SoundId::UNDEFINED)) {
            return false;
        }

        return true;
    }

    String to_string() const {
        String result;

        if (!sub_categories.empty()) {
            result = String(u"%1.%2.%3").arg(sound_category_to_string(category)).arg(id).arg(sub_categories.join(u":"));
        } else {
            result = String(u"%1.%2").arg(sound_category_to_string(category)).arg(id);
        }

        return result;
    }

    static PlaybackSetupData from_string(const String &str) {
        if (str.empty()) {
            return PlaybackSetupData();
        }

        StringList sub_str_list = str.split(u".");

        if (sub_str_list.size() < 2) {
            return PlaybackSetupData();
        }

        StringList sub_categories;
        if (sub_str_list.size() == 3) {
            sub_categories = sub_str_list.at(2).split(u":");
        }

        PlaybackSetupData result = {sub_str_list.at(1), sound_category_from_string(sub_str_list.at(0)), std::move(sub_categories)};

        return result;
    }
};

static const PlaybackSetupData generic_setup_data = {SoundId::LAST, SoundCategory::LAST, {SoundSubCategory::LAST}};

static const String generic_setup_data_string = ::muse::mpe::generic_setup_data.to_string();
} // namespace muse::mpe

#endif // MUSE_MPE_PLAYBACKSETUPDATA_H
