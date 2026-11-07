/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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
#include "importmidi_lyrics.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "../midishared/midifile.h"
#include "engraving/dom/box.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/text.h"
#include "global/types/string.h"
#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "importmidi_inner.h"
#include "importmidi_operations.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace MidiLyrics {
const std::string meta_prefix = "@";
const std::string text_prefix = "@T";

bool is_meta_text(const std::string &text) {
    return text.substr(0, ::mu::iex::midi::MidiLyrics::meta_prefix.size()) == ::mu::iex::midi::MidiLyrics::meta_prefix;
}

bool is_lyric_text(const std::string &text) {
    return !is_meta_text(text) ||
           text.substr(0, ::mu::iex::midi::MidiLyrics::text_prefix.size()) == ::mu::iex::midi::MidiLyrics::text_prefix;
}

bool is_lyric_event(const MidiEvent &e) { return e.type() == ME_META && (e.meta_type() == META_TEXT || e.meta_type() == META_LYRIC); }

std::multimap<ReducedFraction, std::string> extract_lyrics_from_track(const MidiTrack &track, int division, bool is_division_in_tps) {
    std::multimap<ReducedFraction, std::string> lyrics;

    for (const auto &i : track.events()) {
        const auto &e = i.second;
        if (is_lyric_event(e)) {
            const uchar *data = (uchar *) e.edata();
            std::string text = MidiCharset::from_uchar(data);
            text.erase(text.find_last_not_of(' ') + 1);
            if (is_lyric_text(text)) {
                const auto tick = to_muse_score_ticks(i.first, division, is_division_in_tps);
                // no charset handling here
                lyrics.insert({tick, text});
            }
        }
    }
    return lyrics;
}

#ifndef NDEBUG

bool are_equal_indexes_successive(const std::vector<MTrack> &tracks) {
    std::set<int> used_indexes;

    int prev_index = -1;
    for (const MTrack &track : tracks) {
        if (track.index_of_operation < 0) {
            return false;
        }
        if (track.index_of_operation != prev_index) {
            const auto it = used_indexes.find(track.index_of_operation);
            if (it == used_indexes.end()) {
                used_indexes.insert(track.index_of_operation);
            } else {
                return false;
            }
            prev_index = track.index_of_operation;
        }
    }
    return true;
}

#endif

struct BestTrack {
    int index = -1;
    // <orig time, current time - after quantization>
    std::vector<std::pair<ReducedFraction, ReducedFraction>> matched_lyric_times;
};

// find track to insert lyrics
// it will be the track with max onTime match cases, except drum tracks

BestTrack find_best_track(const std::vector<MTrack> &tracks, const std::multimap<ReducedFraction, std::string> &lyric_track,
                          const std::set<int> &used_tracks) {
    BestTrack best_track;
    int max_matches = 0;
#ifndef NDEBUG
    assert(are_equal_indexes_successive(tracks));
#endif
    for (size_t i = 0; i != tracks.size(); ++i) {
        if (tracks[i].mtrack->drum_track() || used_tracks.find(static_cast<int>(i)) != used_tracks.end()) {
            continue;
        }
        // due to staff split there can be equal indexes of operation
        if (i > 0 && tracks[i].index_of_operation == tracks[i - 1].index_of_operation) {
            continue;
        }

        int matches = 0;
        for (const auto &chord : tracks[i].chords) {
            for (const auto &note : chord.second.notes) {
                if (lyric_track.find(note.orig_on_time) != lyric_track.end()) {
                    ++matches;
                    best_track.matched_lyric_times.push_back({chord.first, note.orig_on_time});
                    break;
                }
            }
        }
        if (matches > max_matches) {
            max_matches = matches;
            best_track.index = tracks[i].index_of_operation;
        }
    }

    return best_track;
}

bool is_title_prefix(const muse::String &text) {
    return text.left(::mu::iex::midi::MidiLyrics::text_prefix.size()) == muse::String::from_utf8(::mu::iex::midi::MidiLyrics::text_prefix);
}

void add_title_to_score(Score *score, const muse::String &string, int text_counter) {
    TextStyleType ssid = TextStyleType::DEFAULT;
    if (text_counter == 1) {
        ssid = TextStyleType::TITLE;
    } else if (text_counter == 2) {
        ssid = TextStyleType::COMPOSER;
    }

    MeasureBase *measure = score->first();
    Text *text = mu::engraving::Factory::create_text(measure, ssid);
    text->set_plain_text(string.right(string.size() - ::mu::iex::midi::MidiLyrics::text_prefix.size()));

    if (!measure->is_v_box()) {
        measure = mu::engraving::Factory::create_v_box(score);
        measure->set_tick(Fraction(0, 1));
        measure->set_next(score->first());
        score->measures()->add(measure);
    }
    measure->add(text);
}

// remove slashes in kar format

muse::String remove_slashes(const muse::String &text) {
    muse::String new_text = text;
    new_text = new_text.replace(u"/", u"");
    new_text = new_text.replace(u"\\", u"");
    return new_text;
}

std::string remove_slashes(const std::string &text) {
    std::string str = text;
    char chars[] = "/\\";
    for (unsigned int i = 0; i != strlen(chars); ++i) {
        str.erase(std::remove(str.begin(), str.end(), chars[i]), str.end());
    }
    return str;
}

void add_title_if_any(const std::multimap<ReducedFraction, std::string> &lyric_track, Score *score) {
    int text_counter = 0;
    for (const auto &lyric : lyric_track) {
        if (lyric.first == ReducedFraction(0, 1)) {
            muse::String text = MidiCharset::convert_to_charset(lyric.second);
            if (is_title_prefix(text)) {
                ++text_counter;
                add_title_to_score(score, text, text_counter);
            }
        } else if (lyric.first > ReducedFraction(0, 1)) {
            break;
        }
    }
}

void add_lyrics_to_score(const std::multimap<ReducedFraction, std::string> &lyric_track,
                         const std::vector<std::pair<ReducedFraction, ReducedFraction>> &matched_lyric_times, const Staff *staff_add_to) {
    Score *score = staff_add_to->score();

    add_title_if_any(lyric_track, score);

    for (const auto &time_pair : matched_lyric_times) {
        const ReducedFraction quantized_time = time_pair.first;
        const ReducedFraction original_time = time_pair.second;
        const auto it = lyric_track.find(original_time);

        assert(it != lyric_track.end());

        muse::String text = MidiCharset::convert_to_charset(it->second);
        if (original_time != ReducedFraction(0, 1) || !is_title_prefix(text)) { // not title
            score->add_lyrics(quantized_time.fraction(), staff_add_to->idx(), remove_slashes(text).to_xml_escaped());
        }
    }
}

void extract_lyrics_to_midi_data(const MidiFile *mf) {
    for (const auto &t : mf->tracks()) {
        const auto lyrics = extract_lyrics_from_track(t, mf->division(), mf->is_division_in_tps());
        if (!lyrics.empty()) {
            midi_import_operations.data()->lyric_tracks.push_back(lyrics);
        }
    }
}

void set_initial_lyrics_from_midi_data(const std::vector<MTrack> &tracks) {
    std::set<int> used_tracks;
    auto &data = *midi_import_operations.data();
    const auto &lyric_tracks = data.lyric_tracks;
    if (lyric_tracks.empty()) {
        return;
    }

    for (size_t i = 0; i != lyric_tracks.size(); ++i) {
        const BestTrack best_track = find_best_track(tracks, lyric_tracks[i], used_tracks);
        if (best_track.index >= 0) {
            used_tracks.insert(best_track.index);
            add_lyrics_to_score(lyric_tracks[i], best_track.matched_lyric_times, tracks[best_track.index].staff);
            data.track_opers.lyric_track_index.set_value(best_track.index, static_cast<int>(i));
        }
    }
}

std::vector<std::pair<ReducedFraction, ReducedFraction>>
find_matched_lyric_times(const std::multimap<ReducedFraction, MidiChord> &chords,
                         const std::multimap<ReducedFraction, std::string> &lyric_track) {
    // <chord quantized on time, chord original on time>
    std::vector<std::pair<ReducedFraction, ReducedFraction>> matched_lyric_times;

    for (const auto &chord : chords) {
        for (const auto &note : chord.second.notes) {
            if (lyric_track.find(note.orig_on_time) != lyric_track.end()) {
                matched_lyric_times.push_back({chord.first, note.orig_on_time});
                break;
            }
        }
    }
    return matched_lyric_times;
}

void set_lyrics_from_operations(const std::vector<MTrack> &tracks) {
    const auto &lyric_tracks = midi_import_operations.data()->lyric_tracks;
    if (lyric_tracks.empty()) {
        return;
    }
    for (const auto &track : tracks) {
        const auto &opers = midi_import_operations.data()->track_opers;
        const int lyric_track_index = opers.lyric_track_index.value(track.index_of_operation);
        if (lyric_track_index >= 0 && static_cast<size_t>(lyric_track_index) < lyric_tracks.size()) {
            const auto &lyric_track = lyric_tracks[lyric_track_index];
            const auto matched_lyric_times = find_matched_lyric_times(track.chords, lyric_track);

            add_lyrics_to_score(lyric_track, matched_lyric_times, track.staff);
        }
    }
}

void set_lyrics_to_score(std::vector<MTrack> &tracks) {
    const auto *data = midi_import_operations.data();
    if (data->processings_of_opened_file == 0) {
        set_initial_lyrics_from_midi_data(tracks);
    } else {
        set_lyrics_from_operations(tracks);
    }
}

std::vector<std::string> make_lyrics_list_for_ui() {
    std::vector<std::string> list;
    const auto &lyrics = midi_import_operations.data()->lyric_tracks;
    if (lyrics.empty()) {
        return list;
    }

    const unsigned int symbol_limit = 16;

    for (const auto &track_lyric : lyrics) {
        std::string lyric_text;

        for (const auto &lyric : track_lyric) {
            const auto &text = remove_slashes(lyric.second);
            if (is_meta_text(text)) {
                continue;
            }
            if (!lyric_text.empty()) {
                lyric_text += " "; // visual text delimiter
            }
            if (lyric_text.size() + text.size() > symbol_limit) {
                lyric_text += text.substr(0, symbol_limit - lyric_text.size());
            } else {
                lyric_text += text;
            }
            if (lyric_text.size() >= symbol_limit - 1) {
                lyric_text += "…";
                break;
            }
        }
        list.push_back(lyric_text);
    }
    return list;
}
}} // namespace mu::iex::midi::MidiLyrics
