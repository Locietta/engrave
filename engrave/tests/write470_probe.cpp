// Focused export probe for the preserved independent 4.7.6 reader.
#include <engrave/engrave.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
    if (argc != 6 && argc != 7) {
        std::fprintf(stderr, "usage: write470_probe ASSETS INPUT OUTPUT mscx|mscz default|4.70|5.00 [paired|one-end]\n");
        return 1;
    }
    engrave::EngineOptions engine_options;
    engine_options.resources = std::filesystem::u8path(argv[1]);
    auto engine = engrave::Engine::create(engine_options);
    if (!engine) { std::fprintf(stderr, "%s\n", engine.error().message.c_str()); return 2; }
    auto document = (*engine)->open(std::filesystem::u8path(argv[2]));
    if (!document) { std::fprintf(stderr, "%s\n", document.error().message.c_str()); return 2; }
    if (argc == 7) {
        auto notes = (*document)->query({.types = {"Note"}});
        if (!notes || notes->elements.size() != 2) return 4;
        engrave::Batch batch;
        batch.ops.push_back(engrave::SetPitch{notes->elements[0].id, 62, 16});
        if (std::string(argv[6]) == "paired") batch.ops.push_back(engrave::SetPitch{notes->elements[1].id, 62, 16});
        else if (std::string(argv[6]) != "one-end") return 1;
        if (!(*document)->edit(batch)) return 4;
    }
    engrave::ExportOptions options;
    const std::string format(argv[4]);
    if (format == "mscx") options.format = engrave::Format::Mscx;
    else if (format == "mscz") options.format = engrave::Format::Mscz;
    else return 1;
    if (std::string(argv[5]) != "default") options.native_version = argv[5];
    auto result = (*document)->export_score_with_warnings(options);
    const auto& warnings = result ? result->warnings : result.error().export_warnings;
    nlohmann::json json = nlohmann::json::array();
    for (const auto& warning : warnings) {
        nlohmann::json value{{"code", warning.code}, {"message", warning.message}};
        if (warning.object) value["object"] = {{"kind", warning.object->kind}, {"id", warning.object->id}};
        json.push_back(std::move(value));
    }
    std::fprintf(stdout, "%s\n", json.dump().c_str());
    if (!result) {
        std::fprintf(stderr, "%s: %s\n", std::string(engrave::to_string(result.error().code)).c_str(), result.error().message.c_str());
        return 2;
    }
    std::ofstream output(std::filesystem::u8path(argv[3]), std::ios::binary);
    output.write(reinterpret_cast<const char*>(result->bytes.data()), static_cast<std::streamsize>(result->bytes.size()));
    return output ? 0 : 3;
}
