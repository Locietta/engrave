// SPDX-License-Identifier: GPL-3.0-only
//
// CLI probe for the headless MuseScore audio engine driver (roadmap step 5).
//
//   engine_audio_probe ASSETS SOUNDFONT SCORE OUT.EXT [--no-repeats] [--mute-part N] [--excerpt N]
//
// ASSETS is the native runtime asset root (.runtime/assets), SOUNDFONT an
// .sf2/.sf3 file, SCORE a MusicXML/MSCZ/MSCX/MIDI/GuitarPro file and OUT.EXT
// the target audio file. --mute-part N mutes every track of the zero-based
// part index N (repeatable; mix check); --excerpt N selects the zero-based
// excerpt to render (one excerpt per visible part is generated first when the
// score has none). --midi-events records the events FluidSynth handles together
// with their render-frame (sample) offset; at most the first 1,000,000 are
// printed and midiEventsTruncated marks a longer stream. The probe prints one
// JSON object on stdout; engine and engraving logs go to stderr.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "engrave/src/audio/engine_audio.h"
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/runtime/noqt_runtime.h"
#include "engrave/src/score/score_excerpts.h"
#include "engraving/dom/masterscore.h"
#include "engraving/engravingproject.h"
#include "global/types/uri.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::size_t k_max_score_bytes = 8 * 1024 * 1024;

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string to_utf8(const fs::path &path) {
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t c : utf8) {
        result.push_back(static_cast<char>(c));
    }
    return result;
}

std::string score_format_from_path(const fs::path &path) {
    const std::string ext = lowercase(to_utf8(path.extension()));
    if (ext == ".musicxml" || ext == ".xml") {
        return "musicxml";
    }
    if (ext == ".mxl") {
        return "mxl";
    }
    if (ext == ".mscx") {
        return "mscx";
    }
    if (ext == ".mscz") {
        return "mscz";
    }
    if (ext == ".mid" || ext == ".midi" || ext == ".kar") {
        return "mid";
    }
    if (ext == ".gp" || ext == ".gp3" || ext == ".gp4" || ext == ".gp5" || ext == ".gp6" || ext == ".gp7" || ext == ".gpx" ||
        ext == ".gtp" || ext == ".ptb") {
        return ext.substr(1);
    }
    throw std::invalid_argument("unsupported score extension: " + ext);
}

std::string audio_format_from_path(const fs::path &path) {
    const std::string ext = lowercase(to_utf8(path.extension()));
    if (ext == ".wav") {
        return "wav";
    }
    if (ext == ".flac") {
        return "flac";
    }
    if (ext == ".ogg") {
        return "ogg";
    }
    if (ext == ".mp3") {
        return "mp3";
    }
    if (ext == ".aac") {
        return "aac";
    }
    throw std::invalid_argument("unsupported audio extension: " + ext);
}

music::EngineAudioFormat parse_audio_format(const std::string &name) {
    if (name == "wav") {
        return music::EngineAudioFormat::WAV;
    }
    if (name == "flac") {
        return music::EngineAudioFormat::FLAC;
    }
    if (name == "ogg") {
        return music::EngineAudioFormat::OGG;
    }
    if (name == "mp3") {
        return music::EngineAudioFormat::MP3;
    }
    return music::EngineAudioFormat::AAC;
}

std::string read_file(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("cannot open score file: " + to_utf8(path));
    }

    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        throw std::runtime_error("cannot stat score file: " + to_utf8(path));
    }
    if (size == 0 || size > ::k_max_score_bytes) {
        throw std::runtime_error("score file must contain 1..8388608 bytes");
    }

    std::string data(static_cast<std::size_t>(size), '\0');
    stream.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (!stream || static_cast<std::size_t>(stream.gcount()) != data.size()) {
        throw std::runtime_error("short read from score file: " + to_utf8(path));
    }
    return data;
}

std::uint64_t peak_working_set_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof(counters))) {
        return 0;
    }
    return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
#else
    // VmHWM is the peak resident set size in KiB on Linux.
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmHWM:", 0) == 0) {
            return std::strtoull(line.c_str() + 6, nullptr, 10) * 1024ULL;
        }
    }
    return 0;
#endif
}

double seconds_since(const std::chrono::steady_clock::time_point &start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

Json track_json(const music::EngineAudioTrackInfo &track) {
    return Json{
        {"partId", track.part_id},
        {"instrumentId", track.instrument_id},
        {"partName", track.part_name},
        {"instrumentName", track.instrument_name},
        {"chordSymbols", track.chord_symbols},
        {"partIndex", track.part_index},
    };
}

Json midi_event_json(const muse::midi::Event &event) {
    Json json = {{"opcode", event.opcode_string()}};
    if (!event.is_channel_voice()) {
        return json;
    }

    json["channel"] = static_cast<int>(event.channel());
    switch (event.opcode()) {
        case muse::midi::Event::Opcode::NOTE_ON:
        case muse::midi::Event::Opcode::NOTE_OFF:
            json["note"] = static_cast<int>(event.note());
            json["velocity"] = static_cast<int>(event.velocity7());
            break;
        case muse::midi::Event::Opcode::CONTROL_CHANGE:
            json["index"] = static_cast<int>(event.index());
            json["data"] = static_cast<int>(event.data7());
            break;
        case muse::midi::Event::Opcode::PITCH_BEND: json["pitchBend"] = static_cast<int>(event.pitch_bend14()); break;
        case muse::midi::Event::Opcode::PROGRAM_CHANGE: json["program"] = static_cast<int>(event.program()); break;
        default: break;
    }

    return json;
}
} // namespace

int main(int argc, char **argv) {
    Json out = Json::object();
    out["ok"] = false;

    if (argc < 5) {
        std::cerr << "Usage: engine_audio_probe ASSETS SOUNDFONT SCORE OUT.EXT"
                     " [--no-repeats] [--mute-part N]\n";
        return 2;
    }

    const std::string assets = argv[1];
    const std::string soundfont = argv[2];
    const fs::path score_path = fs::u8path(argv[3]);
    const fs::path output_path_value = fs::u8path(argv[4]);

    bool use_repeats_value = true;
    bool capture_midi = false;
    std::vector<int> mute_parts;
    int excerpt_id_value = -1;
    try {
        for (int i = 5; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--no-repeats") {
                use_repeats_value = false;
            } else if (arg == "--midi-events") {
                capture_midi = true;
            } else if (arg == "--mute-part" && i + 1 < argc) {
                mute_parts.push_back(std::stoi(argv[++i]));
            } else if (arg == "--excerpt" && i + 1 < argc) {
                excerpt_id_value = std::stoi(argv[++i]);
            } else {
                std::cerr << "Unknown argument: " << arg << '\n';
                return 2;
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Invalid argument: " << e.what() << '\n';
        return 2;
    }

    try {
        out["assets"] = assets;
        out["score"] = to_utf8(score_path);
        out["output"] = to_utf8(output_path_value);
        out["repeats"] = use_repeats_value;
        out["excerptId"] = excerpt_id_value;
        out["muteParts"] = mute_parts;
        out["format"] = audio_format_from_path(output_path_value);

        const std::string score_format = score_format_from_path(score_path);
        const std::string score_bytes = read_file(score_path);
        out["scoreFormat"] = score_format;

        auto runtime_start = std::chrono::steady_clock::now();
        music::EngineContext context(assets);
        auto score = music::load_native_score(context, score_bytes, score_format);
        mu::engraving::MasterScore *master = score->master_score();
        out["loadSeconds"] = seconds_since(runtime_start);

        auto engine_start = std::chrono::steady_clock::now();
        std::string bootstrap_error;
        const bool engine_ready = music::initialize_engine_audio(context.audio(), soundfont, &bootstrap_error);
        out["engineSeconds"] = seconds_since(engine_start);
        out["soundfont"] = {
            {"path", soundfont},
            {"resourceId", music::engine_soundfont_resource_id(context.audio())},
            {"uri", music::engine_soundfont_uri(context.audio())},
            {"uriRoundTrip", false},
        };
        if (!engine_ready) {
            throw std::runtime_error(bootstrap_error.empty() ? "engine initialisation failed" : bootstrap_error);
        }

        {
            // Windows drive letters and non-ASCII names must survive
            // Uri::fromLocalFile()/toLocalFile() (fromLocalFile percent-encodes
            // ':' and non-safe bytes, uri.cpp:117-154).
            const muse::Uri uri(music::engine_soundfont_uri(context.audio()));
            const std::string round_tripped = uri.to_local_file().to_std_string();
            std::error_code ec;
            const fs::path expected = fs::absolute(fs::u8path(soundfont), ec).lexically_normal();
            out["soundfont"]["uriRoundTrip"] = !ec && fs::u8path(round_tripped).lexically_normal() == expected;
        }

        if (excerpt_id_value >= 0) {
            // One excerpt per visible part when the file has none; existing
            // excerpts are left untouched.
            music::generate_score_excerpts(master);
        }
        out["excerptCount"] = master->excerpts().size();
        const std::vector<music::EngineAudioTrackInfo> tracks = music::engine_audio_tracks(master, excerpt_id_value);
        out["trackCount"] = tracks.size();

        music::AudioRenderRequest request;
        request.format = parse_audio_format(out["format"].get<std::string>());
        request.output_path = to_utf8(output_path_value);
        request.excerpt_id = excerpt_id_value;
        request.use_repeats = use_repeats_value;
        request.tracks.assign(tracks.size(), music::EngineAudioTrackParams{});
        for (const int mute_part : mute_parts) {
            if (mute_part < 0 || static_cast<std::size_t>(mute_part) >= master->parts().size()) {
                throw std::invalid_argument("--mute-part index is out of range");
            }
            for (std::size_t index = 0; index < tracks.size(); ++index) {
                if (tracks[index].part_index == static_cast<std::size_t>(mute_part)) {
                    request.tracks[index].muted = true;
                }
            }
        }

        auto export_start = std::chrono::steady_clock::now();
        std::size_t midi_event_count = 0;
        std::vector<Json> midi_events;
        if (capture_midi) {
            constexpr std::size_t k_max_midi_events = 1000000;
            midi_events.reserve(4096);
            music::set_engine_audio_midi_event_hook(context.audio(),
                                                    [&midi_event_count, &midi_events](const muse::midi::Event &event, std::uint64_t frame) {
                                                        ++midi_event_count;
                                                        if (midi_events.size() < k_max_midi_events) {
                                                            Json json = midi_event_json(event);
                                                            json["frame"] = frame;
                                                            midi_events.push_back(std::move(json));
                                                        }
                                                    });
        }
        const music::AudioRenderResult result = music::export_score_audio(context.audio(), master, request);
        if (capture_midi) {
            music::set_engine_audio_midi_event_hook(context.audio(), nullptr);
        }
        out["exportSeconds"] = seconds_since(export_start);

        out["playTime"] = result.play_time_secs;
        out["duration"] = result.duration_secs;
        out["frames"] = result.frames;
        out["bytes"] = result.bytes;
        out["ok"] = result.ok;
        out["error"] = result.error;
        out["tracks"] = Json::array();
        for (const music::EngineAudioTrackInfo &track : tracks) {
            out["tracks"].push_back(track_json(track));
        }
        out["midiEventTotal"] = midi_event_count;
        out["midiEventsTruncated"] = midi_event_count > midi_events.size();
        if (capture_midi) {
            out["midiEvents"] = std::move(midi_events);
        }
        out["peakWorkingSetBytes"] = peak_working_set_bytes();
        out["peakWorkingSetMiB"] = peak_working_set_bytes() / (1024.0 * 1024.0);
    } catch (const std::exception &e) {
        out["ok"] = false;
        out["error"] = e.what();
        out["peakWorkingSetBytes"] = peak_working_set_bytes();
        out["peakWorkingSetMiB"] = peak_working_set_bytes() / (1024.0 * 1024.0);
    }

    std::cout << out.dump(2) << '\n';
    return out["ok"].get<bool>() ? 0 : 1;
}
