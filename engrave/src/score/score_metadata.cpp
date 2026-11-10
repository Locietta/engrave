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

// Adapted for the native host from
// webmscore-4.7.5-1 src/converter/internal/compat/notationmeta.cpp
// (mu::converter::NotationMeta, Score* overload and helpers).
//
// The JSON field schema and the title/composer recognition heuristics are kept
// identical to the upstream implementation. QJsonArray/QJsonObject are replaced
// by the engine's JSON arrays/objects and QString by muse::String/std::string. The
// notation/audio-settings dependency: track descriptions come from
// score_audio_track_info(), which lists every InstrumentTrackId and the
// chord-symbol pseudo track.

#include "score_metadata.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "engrave/src/audio/score_audio.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/timesig.h"
#include "engraving/style/style.h"
#include "global/realfn.h"
#include "global/types/string.h"

namespace music {
namespace {
using namespace mu::engraving;

std::string bool_to_string(bool b) { return b ? "true" : "false"; }

bool should_try_recognize_text(const Text *text) {
    const TextStyleType type = text->text_style_type();
    if (type == TextStyleType::DEFAULT || type == TextStyleType::FRAME) {
        return true;
    }

    return static_cast<int>(type) >= static_cast<int>(TextStyleType::USER1) &&
           static_cast<int>(type) <= static_cast<int>(TextStyleType::USER12);
}

muse::String recognize_title(const Score *score) {
    const MeasureBase *mb = score->first();
    if (!mb || !mb->is_v_box()) {
        return muse::String();
    }

    const Text *title_text = nullptr;
    double max_font_size = DBL_MIN;
    double min_y = DBL_MAX;

    for (const EngravingItem *item : mb->el()) {
        if (!item || !item->is_text()) {
            continue;
        }

        const Text *text = to_text(item);
        if (!should_try_recognize_text(text)) {
            continue;
        }

        if (text->size() < max_font_size) {
            continue;
        }

        if (muse::real_is_equal(text->size(), max_font_size) && text->y() > min_y) {
            continue;
        }

        title_text = text;
        max_font_size = text->size();
        min_y = text->y();
    }

    return title_text ? title_text->plain_text() : muse::String();
}

muse::String recognize_composer(const Score *score) {
    const MeasureBase *mb = score->first();
    if (!mb || !mb->is_v_box()) {
        return muse::String();
    }

    const Text *rightmost_text = nullptr;
    double rightmost_text_x = mb->ldata()->bbox().center().x();

    for (const EngravingItem *item : mb->el()) {
        if (!item || !item->is_text()) {
            continue;
        }

        const Text *text = to_text(item);
        if (!should_try_recognize_text(text)) {
            continue;
        }

        if (text->x() > rightmost_text_x) {
            rightmost_text = text;
            rightmost_text_x = text->x();
        }
    }

    return rightmost_text ? rightmost_text->plain_text() : muse::String();
}

muse::String title_string(const Score *score) {
    muse::String title;
    const Text *text = score->get_text(TextStyleType::TITLE);
    if (text) {
        title = text->plain_text();
    }

    if (title.is_empty()) {
        title = score->meta_tag(u"workTitle");
    }

    if (title.is_empty()) {
        title = recognize_title(score);
    }

    if (title.is_empty()) {
        title = score->name();
    }

    return title;
}

std::string title(const Score *score) { return title_string(score).to_std_string(); }

// Exactly the character set of the JS regex
// /[\s<>:{}"/\\|?*~.\0\cA-\cZ]+/g. The JS class includes NUL, the control range
// U+0001..U+001A (which also covers TAB/LF/VT/FF/CR), ASCII space, and the
// Unicode WhiteSpace/LineTerminator additions below. U+0085 and U+180E are not
// JS whitespace and are deliberately not matched.
bool is_filename_unsafe(char32_t c) {
    if (c <= 0x1A) {
        return true; // U+0000, U+0001..U+001A
    }

    switch (c) {
        case U' ':
        case U'<':
        case U'>':
        case U':':
        case U'{':
        case U'}':
        case U'"':
        case U'/':
        case U'\\':
        case U'|':
        case U'?':
        case U'*':
        case U'~':
        case U'.': return true;
        default: break;
    }

    if (c < 0x7F) {
        return false;
    }

    return c == 0x00A0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F ||
           c == 0x3000 || c == 0xFEFF;
}

std::string subtitle(const Score *score) {
    muse::String subtitle;
    const Text *text = score->get_text(TextStyleType::SUBTITLE);
    if (text) {
        subtitle = text->plain_text();
    }

    return subtitle.to_std_string();
}

std::string composer(const Score *score) {
    muse::String composer;
    const Text *text = score->get_text(TextStyleType::COMPOSER);
    if (text) {
        composer = text->plain_text();
    }

    if (composer.is_empty()) {
        composer = score->meta_tag(u"composer");
    }

    if (composer.is_empty()) {
        composer = recognize_composer(score);
    }

    return composer.to_std_string();
}

std::string poet(const Score *score) {
    muse::String poet;
    const Text *text = score->get_text(TextStyleType::LYRICIST);
    if (text) {
        poet = text->plain_text();
    }

    if (poet.is_empty()) {
        poet = score->meta_tag(u"lyricist");
    }

    return poet.to_std_string();
}

std::string timesig(const Score *score) {
    size_t staves = score->nstaves();
    size_t tracks = staves * ::mu::engraving::voices;
    const Segment *time_sig_segment = score->first_segment_mm(SegmentType::TIME_SIG);
    if (!time_sig_segment) {
        return std::string();
    }

    std::string time_sig;
    const EngravingItem *element = nullptr;
    for (size_t track = 0; track < tracks; ++track) {
        element = time_sig_segment->element(track);
        if (element) {
            break;
        }
    }

    if (element && element->is_time_sig()) {
        const TimeSig *ts = to_time_sig(element);
        time_sig = std::to_string(ts->numerator()) + "/" + std::to_string(ts->denominator());
    }

    return time_sig;
}

std::pair<int, std::string> tempo(const Score *score) {
    int tempo = 0;
    muse::String tempo_text;
    for (const Segment *segment = score->first_segment_mm(SegmentType::ALL); segment; segment = segment->next1_mm()) {
        auto annotations = segment->annotations();
        for (const EngravingItem *annotation : annotations) {
            if (annotation && annotation->is_tempo_text()) {
                const TempoText *tt = to_tempo_text(annotation);
                tempo = static_cast<int>(std::round(tt->tempo().to_bpm().val));
                tempo_text = tt->xml_text();
            }
        }
    }

    return {tempo, tempo_text.to_std_string()};
}

bool parse_part_id(const std::string &text, std::uint64_t &value) {
    if (text.empty()) {
        return false;
    }

    std::uint64_t parsed = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }

        const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
        if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return false;
        }
        parsed = parsed * 10 + digit;
    }

    value = parsed;
    return true;
}

SortedJson track_json(const InstrumentTrackId &id, const MetadataTrack &track) {
    SortedJson json_track = make_object<SortedJson>();
    json_track["instrumentId"] = id.instrument_id.to_std_string();
    json_track["partId"] = id.part_id.to_std_string();
    json_track["type"] = track.type;

    // Upstream inserts vendor only for non-Fluid sources. The descriptor
    // carries the actual vendor value; the type check preserves the upstream
    // omission without inventing a vendor for Fluid.
    if (track.vendor && track.type != "fluid_soundfont") {
        json_track["vendor"] = *track.vendor;
    }

    json_track["name"] = track.name;

    // Upstream only adds these keys when they differ from the display name.
    if (track.sound_id && *track.sound_id != track.name) {
        json_track["soundId"] = *track.sound_id;
    }
    if (track.category && *track.category != track.name) {
        json_track["category"] = *track.category;
    }

    return json_track;
}

SortedJson tracks_json_array(const Score *score, const std::vector<MetadataTrack> &tracks) {
    SortedJson json_tracks_array = make_array<SortedJson>();
    std::map<InstrumentTrackId, SortedJson> sorted_json_tracks;

    for (const MetadataTrack &track : tracks) {
        std::uint64_t part_id_value = 0;
        if (!parse_part_id(track.part_id, part_id_value) || part_id_value == 0) {
            throw std::invalid_argument("score_metadata: invalid part id '" + track.part_id + "'");
        }

        const Part *part = nullptr;
        for (const Part *candidate : score->parts()) {
            if (candidate && candidate->id().to_uint64() == part_id_value) {
                part = candidate;
                break;
            }
        }
        if (!part) {
            throw std::invalid_argument("score_metadata: unknown part id '" + track.part_id + "'");
        }

        if (track.instrument_id.empty()) {
            throw std::invalid_argument("score_metadata: empty instrument id for part '" + track.part_id + "'");
        }

        const InstrumentTrackIdSet ids = part->instrument_track_id_set();
        const auto match = std::find_if(ids.begin(), ids.end(), [&track](const InstrumentTrackId &id) {
            return id.instrument_id.to_std_string() == track.instrument_id;
        });
        if (match == ids.end()) {
            throw std::invalid_argument("score_metadata: unknown instrument id '" + track.instrument_id + "' for part '" + track.part_id +
                                        "'");
        }

        const auto inserted = sorted_json_tracks.emplace(*match, make_object<SortedJson>());
        if (!inserted.second) {
            throw std::invalid_argument("score_metadata: duplicate track for part '" + track.part_id + "' and instrument '" +
                                        track.instrument_id + "'");
        }
        inserted.first->second = track_json(*match, track);
    }

    for (const auto &entry : sorted_json_tracks) {
        array_push(json_tracks_array, entry.second);
    }

    return json_tracks_array;
}

SortedJson parts_json_array(const Score *score) {
    SortedJson json_parts_array = make_array<SortedJson>();
    for (const Part *part : score->parts()) {
        SortedJson json_part = make_object<SortedJson>();
        json_part["id"] = part->id().to_std_string();
        muse::String name = part->long_name();
        json_part["name"] = name.replace(u"\n", u"").to_std_string();
        int midi_program = part->midi_program();
        json_part["program"] = midi_program;
        json_part["instrumentId"] = part->instrument_id().to_std_string();
        json_part["instrumentName"] = part->instrument_name().to_std_string();
        json_part["lyricCount"] = part->lyric_count();
        json_part["harmonyCount"] = part->harmony_count();
        json_part["hasPitchedStaff"] = bool_to_string(part->has_pitched_staff());
        json_part["hasTabStaff"] = bool_to_string(part->has_tab_staff());
        json_part["hasDrumStaff"] = bool_to_string(part->has_drum_staff());
        json_part["isVisible"] = bool_to_string(part->show());
        array_push(json_parts_array, json_part);
    }

    return json_parts_array;
}

SortedJson page_format_json(const MStyle &style) {
    SortedJson format = make_object<SortedJson>();
    // Upstream stores round(...) as a QJson double; QJsonDocument serializes a
    // whole value without a fractional part, so int keeps the existing output.
    format["height"] = static_cast<int>(std::round(style.style_d(Sid::PAGE_HEIGHT) * ::mu::engraving::inch));
    format["width"] = static_cast<int>(std::round(style.style_d(Sid::PAGE_WIDTH) * ::mu::engraving::inch));
    format["twosided"] = bool_to_string(style.style_b(Sid::PAGE_TWOSIDED));

    return format;
}

void find_text_by_type(TextStyleType text_style_type, std::vector<muse::String> &strings, EngravingItem *element) {
    if (!element || !element->is_text_base()) {
        return;
    }

    const TextBase *text = to_text_base(element);
    if (text->text_style_type() == text_style_type) {
        strings.push_back(text->plain_text());
    }
}

SortedJson type_data_json(Score *score) {
    SortedJson types_data = make_object<SortedJson>();
    static const std::vector<std::pair<std::string, TextStyleType>> names_types_list{{"titles", TextStyleType::TITLE},
                                                                                     {"subtitles", TextStyleType::SUBTITLE},
                                                                                     {"composers", TextStyleType::COMPOSER},
                                                                                     {"poets", TextStyleType::LYRICIST}};

    for (const auto &name_type : names_types_list) {
        SortedJson type_data = make_array<SortedJson>();
        std::vector<muse::String> type_text_strings;
        score->scan_elements([&](EngravingItem *item) { find_text_by_type(name_type.second, type_text_strings, item); });
        for (const auto &type_str : type_text_strings) {
            array_push(type_data, type_str.to_std_string());
        }
        types_data[name_type.first] = type_data;
    }

    return types_data;
}

SortedJson excerpts_json_array(const Score *score) {
    SortedJson json_excerpts_array = make_array<SortedJson>();

    const MasterScore *master = score->master_score();
    if (!master) {
        return json_excerpts_array;
    }

    const std::vector<Excerpt *> &excerpts = master->excerpts();
    for (size_t i = 0; i < excerpts.size(); ++i) {
        const Excerpt *e = excerpts[i];
        if (!e) {
            continue;
        }

        // Loaders can leave an excerpt entry without an excerpt score. Skip it
        // instead of dereferencing null; the remaining entries keep their
        // original master-list index as "id".
        Score *part = e->excerpt_score();
        if (!part) {
            continue;
        }

        SortedJson json_excerpt = make_object<SortedJson>();
        json_excerpt["id"] = static_cast<int>(i);
        json_excerpt["title"] = e->name().to_std_string();
        json_excerpt["parts"] = parts_json_array(part);

        array_push(json_excerpts_array, json_excerpt);
    }

    return json_excerpts_array;
}

SortedJson metadata_tracks_json_array(const std::vector<ScoreMetadataTrack> &tracks) {
    SortedJson json_tracks_array = make_array<SortedJson>();
    for (const ScoreMetadataTrack &track : tracks) {
        SortedJson json_track = make_object<SortedJson>();
        json_track["trackId"] = track.track_id;
        json_track["partId"] = track.part_id;
        json_track["instrumentId"] = track.instrument_id;
        json_track["partName"] = track.part_name;
        json_track["name"] = track.name;
        json_track["chordSymbols"] = track.chord_symbols;
        json_track["volumeDb"] = track.volume_db;
        json_track["volume"] = track.volume_db;
        json_track["balance"] = track.balance;
        json_track["solo"] = track.solo;
        json_track["muted"] = track.muted;
        json_track["forceMute"] = track.force_mute;
        array_push(json_tracks_array, std::move(json_track));
    }
    return json_tracks_array;
}

SortedJson make_score_metadata_json(Score *score, SortedJson json_tracks) {
    SortedJson json = make_object<SortedJson>();

    json["title"] = title(score);
    json["subtitle"] = subtitle(score);
    json["composer"] = composer(score);
    json["poet"] = poet(score);
    json["mscoreVersion"] = score->mscore_version().to_std_string();
    json["fileVersion"] = score->msc_version();
    json["pages"] = static_cast<int>(score->npages());       // no = operator for size_t
    json["measures"] = static_cast<int>(score->nmeasures()); // no = operator for size_t
    json["hasLyrics"] = bool_to_string(score->has_lyrics());
    json["hasHarmonies"] = bool_to_string(score->has_harmonies());
    json["keysig"] = score->keysig();
    json["previousSource"] = score->meta_tag(u"source").to_std_string();
    json["timesig"] = timesig(score);
    json["duration"] = score->duration();
    json["lyrics"] = score->extract_lyrics().to_std_string();

    auto tempo_value = tempo(score);
    json["tempo"] = tempo_value.first;
    json["tempoText"] = tempo_value.second;

    json["parts"] = parts_json_array(score);
    json["pageFormat"] = page_format_json(score->style());
    json["textFramesData"] = type_data_json(score);
    json["tracks"] = std::move(json_tracks);
    json["excerpts"] = excerpts_json_array(score);

    return json;
}
} // namespace

SortedJson score_metadata(mu::engraving::Score *score, const std::vector<ScoreMetadataTrack> &tracks) {
    if (!score) {
        return make_object<SortedJson>();
    }
    return make_score_metadata_json(score, metadata_tracks_json_array(tracks));
}

SortedJson score_metadata(mu::engraving::Score *score, const std::vector<MetadataTrack> &tracks) {
    if (!score) {
        if (!tracks.empty()) {
            throw std::invalid_argument("score_metadata: cannot validate tracks without a score");
        }
        return make_object<SortedJson>();
    }

    // Validate and build the tracks before doing any other work, so an invalid
    // descriptor cannot produce a partially populated result.
    SortedJson json_tracks = tracks_json_array(score, tracks);
    return make_score_metadata_json(score, std::move(json_tracks));
}

SortedJson score_metadata(mu::engraving::Score *score) {
    if (!score) {
        return make_object<SortedJson>();
    }
    std::vector<ScoreMetadataTrack> tracks;
    const auto infos = score_audio_track_info(score);
    tracks.reserve(infos.size());
    for (std::size_t index = 0; index < infos.size(); ++index) {
        const auto &info = infos[index];
        ScoreMetadataTrack t;
        t.track_id = static_cast<std::int64_t>(index);
        t.part_id = info.part_id;
        t.instrument_id = info.instrument_id;
        t.part_name = info.part_name;
        t.name = info.instrument_name;
        t.chord_symbols = info.chord_symbols;
        // No export mix is available here; chord playback is still off.
        t.muted = info.chord_symbols;
        t.force_mute = info.chord_symbols;
        tracks.push_back(std::move(t));
    }
    return score_metadata(score, tracks);
}

std::string score_title_filename_safe(const mu::engraving::Score *score) {
    if (!score) {
        return std::string();
    }

    // Work on Unicode code points. UTF-8 is not a fixed-width encoding, so a
    // byte-wise character class would corrupt multibyte text (and would also
    // be unable to match U+FEFF, U+2028, etc. correctly).
    const std::u32string input = title_string(score).to_std_u32_string();
    std::u32string output;
    output.reserve(input.size());

    bool in_replacement_run = false;
    for (const char32_t ch : input) {
        if (is_filename_unsafe(ch)) {
            if (!in_replacement_run) {
                output.push_back(U'_');
                in_replacement_run = true;
            }
        } else {
            output.push_back(ch);
            in_replacement_run = false;
        }
    }

    if (output.empty()) {
        return std::string();
    }

    return muse::String::from_ucs4(output.data(), output.size()).to_std_string();
}

std::string score_title(const mu::engraving::Score *score) {
    if (!score) {
        return std::string();
    }

    return title(score);
}
} // namespace music
