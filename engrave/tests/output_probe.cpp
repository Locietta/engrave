// Renders one score through the public API and writes the results, so the
// superproject's scripts/test_a4_output.py can compare them with the render goldens
// exactly as scripts/golden.py does.
//
//   engrave_output_probe --assets=DIR --soundfont=FILE... --render=SCORE --out=DIR
//   engrave_output_probe --assets=DIR --positions=SCORE --out=DIR
//   engrave_output_probe --assets=DIR --soundfont=FILE... --audio=SCORE --format=wav|flac|ogg|mp3|aac
//                        --out=FILE [--repeats=0] [--sample-rate=HZ]
//
// --render writes page-<n>.svg for every page, page-<n>.png for every page and
// score.pdf, all with the A2 defaults (144 dpi, transparent PNG, no page
// background). It prints one JSON object on stdout. The exit status is 0 on
// success, 2 for a returned error (printed as "error: Code: message") and 3 for
// an exception.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <engrave/engrave.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

std::string_view extension_of(engrave::AudioFormat format) {
    switch (format) {
        case engrave::AudioFormat::WAV: return "wav";
        case engrave::AudioFormat::FLAC: return "flac";
        case engrave::AudioFormat::OPUS: return "ogg";
        case engrave::AudioFormat::MP3: return "mp3";
        case engrave::AudioFormat::AAC: return "aac";
    }
    return "bin";
}

void write(const std::filesystem::path &path, const std::vector<std::byte> &data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) throw std::runtime_error("cannot write '" + path.string() + "'");
}

void write_audio(const std::filesystem::path &path, const std::vector<std::byte> &data) { write(path, data); }

std::vector<std::string> notes(engrave::Document &doc) {
    auto outline = doc.outline();
    if (!outline) throw std::runtime_error("outline failed");
    std::vector<std::string> result;
    for (int first = 0; first < outline->measure_count; first += 128) {
        engrave::Query q;
        q.types = {"Note"};
        q.props = {"pitch"};
        q.first_measure = first;
        q.measure_count = std::min(128, outline->measure_count - first);
        auto found = doc.query(q);
        if (!found || found->next_cursor) throw std::runtime_error("note query incomplete");
        for (const auto &n : found->elements) {
            std::ostringstream row;
            row << n.staff << ':' << n.voice << ':' << n.tick.num << '/' << n.tick.den;
            for (const auto &p : n.props) {
                if (auto v = std::get_if<std::int64_t>(&p.value)) row << ':' << *v;
            }
            result.push_back(row.str());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

int usage() {
    std::fprintf(stderr, "usage: --assets=DIR [--soundfont=FILE]... --render=SCORE --out=DIR\n"
                         "       --assets=DIR --positions=SCORE --out=DIR\n"
                         "       --assets=DIR [--soundfont=FILE]... --audio=SCORE --format=NAME --out=FILE\n");
    return 1;
}

} // namespace

int main(int argc, char **argv) {
    std::filesystem::path assets, out, score, test_startup_font;
    std::vector<std::filesystem::path> soundfonts;
    std::string mode, format = "wav";
    bool repeats = true;
    bool test_output_open_failure = false;
    int sample_rate = 44100;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        const auto value = [&argument](std::size_t prefix) { return std::string(argument.substr(prefix)); };
        if (argument.starts_with("--assets="))
            assets = std::filesystem::u8path(value(9));
        else if (argument.starts_with("--soundfont="))
            soundfonts.push_back(std::filesystem::u8path(value(12)));
        else if (argument.starts_with("--out="))
            out = std::filesystem::u8path(value(6));
        else if (argument.starts_with("--render=")) {
            mode = "render";
            score = std::filesystem::u8path(value(9));
        } else if (argument.starts_with("--positions=")) {
            mode = "positions";
            score = std::filesystem::u8path(value(12));
        } else if (argument.starts_with("--export=")) {
            mode = "export";
            score = std::filesystem::u8path(value(9));
        } else if (argument.starts_with("--audio=")) {
            mode = "audio";
            score = std::filesystem::u8path(value(8));
        } else if (argument.starts_with("--format="))
            format = value(9);
        else if (argument.starts_with("--sample-rate="))
            sample_rate = std::atoi(value(14).c_str());
        else if (argument == "--no-repeats")
            repeats = false;
        else if (argument == "--test-output-open-failure")
            test_output_open_failure = true;
        else if (argument.starts_with("--test-startup-font="))
            test_startup_font = std::filesystem::u8path(value(20));
        else
            return usage();
    }
    if (assets.empty() || score.empty() || out.empty() || mode.empty()) return usage();

    try {
        // This test-only mode runs in a fresh process, so the first render's
        // temporary path is predictable without exposing engine internals.
        struct CacheCleanup {
            std::filesystem::path path;
            std::filesystem::path blocked;

            ~CacheCleanup() {
                std::error_code ec;
                if (!blocked.empty()) std::filesystem::remove(blocked, ec);
                if (!path.empty()) std::filesystem::remove(path, ec);
            }
        } cache_cleanup;

        engrave::EngineOptions options;
        options.resources = assets;
        options.soundfonts = soundfonts;
        options.audio_sample_rate = sample_rate;
        if (!test_startup_font.empty()) options.fonts.push_back(test_startup_font);
        if (test_output_open_failure) {
            if (mode != "audio" || format != "wav" || !std::filesystem::create_directory(out))
                throw std::runtime_error("test needs a new cache directory and WAV audio");
            cache_cleanup.path = out;
            options.cache_dir = out;
#ifdef _WIN32
            const int process = _getpid();
#else
            const int process = getpid();
#endif
            const auto blocked = out / ("engrave-audio-" + std::to_string(process) + "-1.wav");
            if (!std::filesystem::create_directory(blocked)) throw std::runtime_error("cannot block test output path");
            cache_cleanup.blocked = blocked;
        }
        auto engine = engrave::Engine::create(options);
        if (!engine) {
            std::fprintf(stderr, "error: %s: %s\n", std::string(engrave::to_string(engine.error().code)).c_str(),
                         engine.error().message.c_str());
            return 2;
        }
        if (!test_startup_font.empty()) {
            const auto fonts = (*engine)->runtime_fonts();
            if (fonts.size() != 1 || fonts[0].id <= 0 || fonts[0].family.empty() || fonts[0].content_fingerprint.empty() ||
                !fonts[0].path.is_absolute() || fonts[0].path == test_startup_font)
                throw std::runtime_error("startup font metadata is incomplete");
            const auto read = [](const std::filesystem::path &path) {
                std::ifstream file(path, std::ios::binary);
                return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            };
            const auto retained = fonts[0].path;
            if (read(retained).empty() || read(retained) != read(test_startup_font))
                throw std::runtime_error("startup font metadata did not identify the retained copy");
            (*engine).reset();
            if (std::filesystem::exists(retained)) throw std::runtime_error("startup font survived Engine destruction");
            return 0;
        }
        auto document = (*engine)->open(score);
        if (!document) {
            std::fprintf(stderr, "error: %s: %s\n", std::string(engrave::to_string(document.error().code)).c_str(),
                         document.error().message.c_str());
            return 2;
        }

        if (mode == "positions") {
            std::filesystem::create_directories(out);
            for (const auto [kind, filename] : {std::pair{engrave::PositionKind::MEASURE, "measurePositions.json"},
                                                std::pair{engrave::PositionKind::SEGMENT, "segmentPositions.json"}}) {
                const auto positions = (*document)->positions(kind);
                if (!positions) throw std::runtime_error(positions.error().message);
                nlohmann::json result;
                result["page_size"] = {{"width", positions->page_width}, {"height", positions->page_height}};
                result["elements"] = nlohmann::json::array();
                result["events"] = nlohmann::json::array();
                for (const auto &element : positions->elements)
                    result["elements"].push_back({{"id", element.id},
                                                  {"x", element.x},
                                                  {"y", element.y},
                                                  {"sx", element.sx},
                                                  {"sy", element.sy},
                                                  {"page", element.page}});
                for (const auto &event : positions->events)
                    result["events"].push_back({{"elid", event.elid}, {"position", event.position}});
                std::ofstream file(out / filename, std::ios::binary | std::ios::trunc);
                file << result.dump();
                if (!file) throw std::runtime_error("cannot write positions");
            }
            return 0;
        }

        if (mode == "export") {
            std::filesystem::create_directories(out);
            const auto before = notes(**document);
            const std::pair<engrave::Format, const char *> formats[] = {{engrave::Format::MUSIC_XML, "score.musicxml"},
                                                                        {engrave::Format::MSCZ, "score.mscz"},
                                                                        {engrave::Format::MXL, "score.mxl"},
                                                                        {engrave::Format::MIDI, "score.mid"}};
            for (auto [kind, name] : formats) {
                engrave::ExportOptions ex;
                ex.format = kind;
                auto bytes = (*document)->export_score(ex);
                if (!bytes) throw std::runtime_error(bytes.error().message);
                write(out / name, *bytes);
                auto reopened = (*engine)->open(out / name);
                if (!reopened) throw std::runtime_error(reopened.error().message);
                // Native serialization must preserve note placement and pitch exactly.
                // Interchange import normalizes voices/staves; the captured oracle owns it.
                if ((kind == engrave::Format::MSCZ || kind == engrave::Format::MSCX) && notes(**reopened) != before)
                    throw std::runtime_error("native semantic reopen changed notes");
            }
            return 0;
        }

        if (mode == "render") {
            engrave::RenderOptions render;
            render.format = engrave::RenderFormat::SVG;
            const auto svg = (*document)->render(render);
            if (!svg) {
                std::fprintf(stderr, "error: %s: %s\n", std::string(engrave::to_string(svg.error().code)).c_str(),
                             svg.error().message.c_str());
                return 2;
            }
            std::filesystem::create_directories(out);
            for (std::size_t page = 0; page < svg->pages.size(); ++page)
                write(out / ("page-" + std::to_string(page) + ".svg"), svg->pages[page]);

            render.format = engrave::RenderFormat::PNG;
            const auto png = (*document)->render(render);
            if (!png) {
                std::fprintf(stderr, "error: %s: %s\n", std::string(engrave::to_string(png.error().code)).c_str(),
                             png.error().message.c_str());
                return 2;
            }
            for (std::size_t page = 0; page < png->pages.size(); ++page)
                write(out / ("page-" + std::to_string(page) + ".png"), png->pages[page]);

            render.format = engrave::RenderFormat::PDF;
            const auto pdf = (*document)->render(render);
            if (!pdf) {
                std::fprintf(stderr, "error: %s: %s\n", std::string(engrave::to_string(pdf.error().code)).c_str(),
                             pdf.error().message.c_str());
                return 2;
            }
            write(out / "score.pdf", pdf->document);
            std::printf("{\"pages\": %zu}\n", svg->pages.size());
            return 0;
        }

        engrave::AudioOptions audio;
        audio.use_repeats = repeats;
        if (format == "wav")
            audio.format = engrave::AudioFormat::WAV;
        else if (format == "flac")
            audio.format = engrave::AudioFormat::FLAC;
        else if (format == "ogg" || format == "opus")
            audio.format = engrave::AudioFormat::OPUS;
        else if (format == "mp3")
            audio.format = engrave::AudioFormat::MP3;
        else if (format == "aac")
            audio.format = engrave::AudioFormat::AAC;
        else
            return usage();
        audio.sample_rate = sample_rate;

        std::vector<std::byte> bytes;
        const auto rendered = (*document)->render_audio(
            audio, [&bytes](std::span<const std::byte> chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); });
        if (test_output_open_failure) {
            if (rendered || rendered.error().code != engrave::ErrorCode::IO || !bytes.empty())
                throw std::runtime_error("output open failure did not return Io without calling the sink");
            const auto recovered = (*document)->render_audio(
                audio, [&bytes](std::span<const std::byte> chunk) { bytes.insert(bytes.end(), chunk.begin(), chunk.end()); });
            if (!recovered || bytes.empty()) throw std::runtime_error("render after output failure did not recover");
            return 0;
        }
        if (!rendered) {
            std::fprintf(stderr, "error: %s: %s\n", std::string(engrave::to_string(rendered.error().code)).c_str(),
                         rendered.error().message.c_str());
            return 2;
        }
        write_audio(out, bytes);
        std::printf("{\"bytes\": %llu, \"frames\": %llu, \"seconds\": %.3f, \"extension\": \"%s\"}\n",
                    static_cast<unsigned long long>(rendered->bytes), static_cast<unsigned long long>(rendered->frames), rendered->seconds,
                    std::string(extension_of(audio.format)).c_str());
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "exception: %s\n", error.what());
        return 3;
    }
}
