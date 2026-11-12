#include "sniff.h"

#include <algorithm>
#include <cctype>
#include <string_view>

#include "engrave/src/score/json.h"
#include "global/io/byteview.h"
#include "global/serialization/xmlstreamreader.h"
#include "global/serialization/zipreader.h"

namespace engrave {
namespace {
bool ends_with(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && std::equal(suffix.begin(), suffix.end(), text.end() - suffix.size(),
                                                      [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
}

std::optional<Format> sniff_zip(const std::string &data) {
    muse::io::ByteView buffer(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(data.data()), data.size()));
    buffer.open(muse::io::IODevice::READ_ONLY);
    muse::ZipReader zip(&buffer);
    if (zip.has_error()) return std::nullopt;
    if (zip.file_exists("META-INF/container.xml")) {
        muse::XmlStreamReader xml(zip.file_data("META-INF/container.xml"));
        bool musicxml = false;
        while (!xml.at_end()) {
            xml.read_next();
            if (!xml.is_start_element() || xml.name() != "rootfile") continue;
            const std::string path = xml.attribute("full-path").to_std_string();
            if (ends_with(path, ".mscx")) return Format::MSCZ;
            musicxml = musicxml || ends_with(path, ".musicxml") || (ends_with(path, ".xml") && !path.starts_with("META-INF/"));
        }
        if (musicxml) return Format::MXL;
    }
    if (zip.file_exists("Content/score.gpif")) return Format::GUITAR_PRO;
    return std::nullopt;
}

std::optional<Format> sniff_xml(const std::string &data) {
    std::string_view text = data;
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos || text[first] != '<') return std::nullopt;
    muse::XmlStreamReader xml{std::string_view(data)};
    while (!xml.at_end()) {
        xml.read_next();
        if (xml.is_start_element()) {
            if (xml.name() == "museScore") return Format::MSCX;
            if (xml.name() == "score-partwise" || xml.name() == "score-timewise") return Format::MUSIC_XML;
            return std::nullopt;
        }
    }
    return std::nullopt;
}
} // namespace

std::optional<Format> sniff_format(const std::string &data) {
    const std::string_view head(data.data(), std::min<std::size_t>(data.size(), 64));
    if (head.starts_with("PK\x03\x04")) {
        if (auto format = sniff_zip(data)) return format;
    }
    std::string_view text = data;
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first != std::string_view::npos && text[first] == '{') {
        const auto parsed = music::parse_json<music::Json>(text);
        if (parsed && parsed->is_object() && parsed->contains("mnx") && parsed->at("mnx").is_object()) return Format::MNX;
    }
    if (head.starts_with("MThd")) return Format::MIDI;
    if (auto format = sniff_xml(data)) return format;
    // importGTP's magics: Power Tab, GP6 (BCFZ), and GP1-5, whose header is a
    // length byte followed by "FICHIER GUITAR PRO" or "FICHIER GUITARE PRO".
    if (head.starts_with("ptab") || head.starts_with("BCFZ") || (!head.empty() && head.substr(1).starts_with("FICHIER GUITAR")))
        return Format::GUITAR_PRO;
    return std::nullopt;
}

std::optional<Format> format_from_extension(const std::filesystem::path &name) {
    const auto u8 = name.extension().u8string();
    std::string ext(u8.begin(), u8.end());
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".mnx") return Format::MNX;
    if (ext == ".mscz") return Format::MSCZ;
    if (ext == ".mscx") return Format::MSCX;
    if (ext == ".musicxml" || ext == ".xml") return Format::MUSIC_XML;
    if (ext == ".mxl") return Format::MXL;
    if (ext == ".mid" || ext == ".midi" || ext == ".kar") return Format::MIDI;
    if (ext == ".gp" || ext == ".gp3" || ext == ".gp4" || ext == ".gp5" || ext == ".gp6" || ext == ".gp7" || ext == ".gpx" ||
        ext == ".gtp" || ext == ".ptb")
        return Format::GUITAR_PRO;
    return std::nullopt;
}

} // namespace engrave
