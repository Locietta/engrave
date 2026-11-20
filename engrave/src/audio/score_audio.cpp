// Native score audio helpers shared by the host and the worker.
//
// Track identity is the score's InstrumentTrackId; the actual rendering lives
// in MuseScore 4's audio engine (engine_audio.cpp) and only runs in a worker
// process. This file keeps the deterministic track description and the MSCZ
// audiosettings.json default mix.

#include "score_audio.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/playback/playbackwalk.h"
#include "global/io/byteview.h"
#include "global/serialization/json_fields.h"
#include "global/serialization/zipreader.h"

namespace music {
namespace {

using mu::engraving::Instrument;
using mu::engraving::InstrumentTrackId;
using mu::engraving::Part;
using mu::engraving::Score;

// audiosettings.json is tiny; reject pathological archives before parsing.
constexpr std::size_t max_audio_settings_bytes = 1024 * 1024;
constexpr std::size_t max_audio_settings_tracks = 4096;

namespace json = muse::json;

void apply_settings_output(AudioOutputParams &output, const glz::generic &settings) {
    if (const auto *volume = json::member(settings, "volumeDb").get_if<double>(); volume && *volume >= -120.0 && *volume <= 24.0) {
        output.volume_db = *volume;
    }
    if (const auto *balance = json::member(settings, "balance").get_if<double>(); balance && *balance >= -1.0 && *balance <= 1.0) {
        output.balance = *balance;
    }
}

template <class Output>
void apply_solo_mute(Output &output, const glz::generic &state) {
    if (const auto *solo = json::member(state, "solo").get_if<bool>()) output.solo = *solo;
    if (const auto *mute = json::member(state, "mute").get_if<bool>()) output.muted = *mute;
}

template <class Fn>
void for_each_track(const glz::generic &settings, Fn apply) {
    const auto &tracks = json::array(json::member(settings, "tracks"));
    if (tracks.size() > max_audio_settings_tracks) return;
    for (const auto &track : tracks) {
        const auto *part = json::member(track, "partId").get_if<std::string>();
        const auto *instrument = json::member(track, "instrumentId").get_if<std::string>();
        if (part && instrument) apply(std::make_pair(*part, *instrument), track);
    }
}

} // namespace

std::vector<ScoreAudioTrackInfo> score_audio_track_info(Score *score) {
    if (!score) {
        throw std::invalid_argument("score_audio: score must not be null");
    }

    std::vector<ScoreAudioTrackInfo> result;
    std::size_t part_index = 0;
    for (const Part *part : score->parts()) {
        if (!part) {
            ++part_index;
            continue;
        }

        // Part::instrumentTrackIdList() dereferences every instrument, so
        // dedupe here after skipping malformed entries. The order matches the
        // engine: instruments are stored by their first tick in a std::map.
        std::set<muse::String> seen_instruments;
        for ([[maybe_unused]] const auto &[tick, instrument] : part->instruments()) {
            if (!instrument) {
                continue;
            }
            const InstrumentTrackId id{part->id(), instrument->id()};
            if (!id.is_valid() || !seen_instruments.insert(id.instrument_id).second) {
                continue;
            }

            ScoreAudioTrackInfo info;
            info.id = id;
            info.part_id = part->id().to_std_string();
            info.instrument_id = id.instrument_id.to_std_string();
            info.part_name = part->part_name().to_std_string();
            info.instrument_name = instrument->track_name().to_std_string();
            info.chord_symbols = false;
            info.part_index = part_index;
            result.push_back(std::move(info));
        }

        if (part->has_chord_symbol()) {
            ScoreAudioTrackInfo info;
            info.id = mu::engraving::playback_chord_symbols_track_id(part->id());
            info.part_id = part->id().to_std_string();
            info.instrument_id = info.id.instrument_id.to_std_string();
            info.part_name = part->part_name().to_std_string();
            // Mirror PlaybackController's mixer title for the pseudo track.
            info.instrument_name = "Chords." + info.part_name;
            info.chord_symbols = true;
            info.part_index = part_index;
            result.push_back(std::move(info));
        }

        ++part_index;
    }

    return result;
}

bool json_nesting_ok(std::string_view text, std::size_t max_depth = 64) {
    std::size_t depth = 0;
    bool in_string = false;
    bool escape = false;
    for (char c : text) {
        if (escape) {
            escape = false;
            continue;
        }
        if (in_string) {
            if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            ++depth;
            if (depth > max_depth) return false;
        } else if (c == '}' || c == ']') {
            if (depth > 0) {
                --depth;
            }
        }
    }
    return true;
}

namespace {

const muse::ZipReader::FileInfo *find_zip_entry(const std::vector<muse::ZipReader::FileInfo> &entries, std::string_view name) {
    for (const auto &entry : entries) {
        if (entry.file_path.to_std_string() == name) {
            return &entry;
        }
    }
    return nullptr;
}

std::optional<glz::generic> read_audio_settings(muse::ZipReader &zip, const std::vector<muse::ZipReader::FileInfo> &entries,
                                                std::string_view file_path) {
    const auto *entry = find_zip_entry(entries, file_path);
    if (!entry || entry->size > max_audio_settings_bytes) return std::nullopt;

    const muse::ByteArray bytes = zip.file_data(std::string(file_path));
    if (zip.has_error() || bytes.empty() || bytes.size() > max_audio_settings_bytes) return std::nullopt;
    const auto text = json::text(bytes);
    if (!json_nesting_ok(text)) return std::nullopt;

    auto settings = glz::read_json<glz::generic>(text);
    if (!settings || !settings->is_object()) return std::nullopt;
    return std::move(*settings);
}

} // namespace

ScoreAudioMixDefaults score_audio_mix_defaults(const std::string &mscz_bytes) {
    ScoreAudioMixDefaults result;
    if (mscz_bytes.size() < 4) return result;
    try {
        muse::io::ByteView buffer(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(mscz_bytes.data()), mscz_bytes.size()));
        if (!buffer.open(muse::io::IODevice::READ_ONLY)) return result;
        muse::ZipReader zip(&buffer);
        if (zip.has_error()) return result;

        const auto file_infos = zip.file_info_list();
        if (file_infos.empty() || file_infos.size() > 4096) return result;

        if (const auto settings = read_audio_settings(zip, file_infos, "audiosettings.json")) {
            apply_settings_output(result.master, json::member(*settings, "master"));
            for_each_track(*settings, [&](auto key, const glz::generic &track) {
                AudioOutputParams output;
                apply_settings_output(output, json::member(track, "out"));
                apply_solo_mute(output, json::member(track, "soloMuteState"));
                result.tracks.emplace(std::move(key), output);
            });
        }

        std::vector<std::string> excerpt_names;
        for (const auto &entry : file_infos) {
            const std::string path = entry.file_path.to_std_string();
            if (path.starts_with("Excerpts/") && (path.ends_with(".mscx") || path.ends_with(".MSCX"))) {
                const std::size_t slash = path.find('/', 9);
                if (slash != std::string::npos) {
                    std::string name = path.substr(9, slash - 9);
                    if (!name.empty() && std::find(excerpt_names.begin(), excerpt_names.end(), name) == excerpt_names.end()) {
                        excerpt_names.push_back(std::move(name));
                    }
                }
            }
        }
        for (const auto &entry : file_infos) {
            const std::string path = entry.file_path.to_std_string();
            if (path.starts_with("Excerpts/") && path.ends_with("/audiosettings.json")) {
                std::string name = path.substr(9, path.size() - 9 - 18);
                if (!name.empty() && std::find(excerpt_names.begin(), excerpt_names.end(), name) == excerpt_names.end()) {
                    excerpt_names.push_back(std::move(name));
                }
            }
        }

        result.excerpt_tracks_by_index.resize(excerpt_names.size());
        for (std::size_t i = 0; i < excerpt_names.size(); ++i) {
            const std::string &name = excerpt_names[i];
            const std::string file_path = "Excerpts/" + name + "/audiosettings.json";
            const auto settings = read_audio_settings(zip, file_infos, file_path);
            if (!settings) continue;
            const auto *tracks = json::member(*settings, "tracks").get_if<glz::generic::array_t>();
            if (!tracks || tracks->size() > max_audio_settings_tracks) continue;

            std::map<std::pair<std::string, std::string>, TrackSoloMute> solo_mute_map;
            for_each_track(*settings, [&](auto key, const glz::generic &track) {
                TrackSoloMute state;
                apply_solo_mute(state, json::member(track, "soloMuteState"));
                solo_mute_map.emplace(std::move(key), state);
            });
            result.excerpt_tracks_by_index[i] = solo_mute_map;
            result.excerpt_tracks_by_name[name] = std::move(solo_mute_map);
        }
    } catch (...) {
        // A damaged archive or unexpected JSON shape is not a score-open error.
        return {};
    }
    return result;
}

AudioMix score_audio_default_mix(const ScoreAudioMixDefaults &defaults, const std::vector<ScoreAudioTrackInfo> &tracks, int excerpt_id,
                                 const std::string &excerpt_name) {
    const std::map<std::pair<std::string, std::string>, TrackSoloMute> *excerpt_solo_mute =
        excerpt_id >= 0 ? defaults.get_excerpt_map(excerpt_id, excerpt_name) : nullptr;

    AudioMix mix;
    mix.master = defaults.master;
    mix.tracks.reserve(tracks.size());
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        const ScoreAudioTrackInfo &track = tracks[index];
        AudioOutputParams output;
        const auto key = std::make_pair(track.part_id, track.instrument_id);
        if (const auto found = defaults.tracks.find(key); found != defaults.tracks.end()) {
            output.volume_db = found->second.volume_db;
            output.balance = found->second.balance;
            if (excerpt_id < 0) {
                output.solo = found->second.solo;
                output.muted = found->second.muted;
            }
        }
        if (excerpt_id >= 0) {
            // An excerpt file owns its solo/mute state; without one the track
            // is neither soloed nor muted.
            if (excerpt_solo_mute) {
                const auto found = excerpt_solo_mute->find(key);
                if (found != excerpt_solo_mute->end()) {
                    output.solo = found->second.solo;
                    output.muted = found->second.muted;
                }
            }
        }
        mix.tracks.push_back(AudioTrackParams{static_cast<std::int64_t>(index), output});
    }

    derive_force_mute(mix);
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        if (tracks[index].chord_symbols) {
            mix.tracks[index].output.muted = true;
            mix.tracks[index].output.force_mute = true;
        }
    }
    return mix;
}

} // namespace music
