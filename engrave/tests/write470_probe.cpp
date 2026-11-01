// Focused export probe for the preserved independent 4.7.6 reader.
#include <engrave/engrave.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 6) {
        std::fprintf(stderr, "usage: write470_probe ASSETS INPUT OUTPUT mscx|mscz default|4.70|5.00\n");
        return 1;
    }
    engrave::EngineOptions engine_options;
    engine_options.resources = std::filesystem::u8path(argv[1]);
    auto engine = engrave::Engine::create(engine_options);
    if (!engine) { std::fprintf(stderr, "%s\n", engine.error().message.c_str()); return 2; }
    auto document = (*engine)->open(std::filesystem::u8path(argv[2]));
    if (!document) { std::fprintf(stderr, "%s\n", document.error().message.c_str()); return 2; }
    engrave::ExportOptions options;
    const std::string format(argv[4]);
    if (format == "mscx") options.format = engrave::Format::Mscx;
    else if (format == "mscz") options.format = engrave::Format::Mscz;
    else return 1;
    if (std::string(argv[5]) != "default") options.native_version = argv[5];
    auto result = (*document)->export_score_with_warnings(options);
    if (!result) {
        std::fprintf(stderr, "%s: %s\n", std::string(engrave::to_string(result.error().code)).c_str(),
                     result.error().message.c_str());
        for (const auto& warning : result.error().export_warnings)
            std::fprintf(stderr, "%s %s\n", warning.code.c_str(), warning.message.c_str());
        return 2;
    }
    for (const auto& warning : result->warnings)
        std::fprintf(stdout, "%s %s\n", warning.code.c_str(),
                     warning.object ? engrave::to_string(*warning.object).c_str() : "-");
    std::ofstream output(std::filesystem::u8path(argv[3]), std::ios::binary);
    output.write(reinterpret_cast<const char*>(result->bytes.data()), static_cast<std::streamsize>(result->bytes.size()));
    return output ? 0 : 3;
}
