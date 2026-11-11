// SPDX-License-Identifier: GPL-3.0-only
// Native runtime font services and retained asset storage.
#include "runtime_assets.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <zlib.h>

#include "draw/internal/fontprovider.h"
#include "draw/internal/fontsdatabase.h"
#include "draw/internal/fontsengine.h"
#include "draw/types/fontstypes.h"
#include "fonts_database.h"
#include "fonts_engine_proxy.h"
#include "global/io/path.h"

namespace music {
namespace {

namespace draw = muse::draw;

constexpr std::uintmax_t k_max_font_file_bytes = 64ull * 1024ull * 1024ull;
constexpr std::uintmax_t k_max_sound_font_file_bytes = 128ull * 1024ull * 1024ull;
constexpr std::size_t k_max_retained_fonts = 32;
constexpr std::size_t k_max_retained_sound_fonts = 32;
constexpr std::size_t k_max_retained_bytes = 256ull * 1024ull * 1024ull;

std::string path_to_utf8(const std::filesystem::path &path) {
    const auto utf8 = path.u8string();
    return std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size());
}

struct CacheStore {
    explicit CacheStore(std::filesystem::path root) : root_(std::move(root)) {}

    void ensure_directory() {
        if (owned_) return;
        std::error_code error;
        const std::filesystem::path base = !root_.empty() ? root_ : std::filesystem::temp_directory_path(error);
        if (error || base.empty()) {
            throw std::runtime_error("Failed to resolve the temporary directory for runtime fonts");
        }

        std::random_device random;
        for (int attempt = 0; attempt < 64; ++attempt) {
            static std::atomic<std::uint64_t> sequence{0};
            const std::string name = "musescore-runtime-fonts-" + std::to_string(random()) + "-" + std::to_string(random()) + "-" +
                                     std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
            const std::filesystem::path candidate = base / name;

            std::error_code create_error;
            if (!std::filesystem::create_directory(candidate, create_error)) {
                if (create_error) {
                    throw std::runtime_error("Failed to create runtime font cache directory '" + path_to_utf8(candidate) +
                                             "': " + create_error.message());
                }
                continue; // Never adopt a pre-existing path; try fresh randomness.
            }

            std::error_code absolute_error;
            std::filesystem::path absolute = std::filesystem::absolute(candidate, absolute_error);
            if (absolute_error || absolute.empty() || !absolute.is_absolute()) {
                std::error_code cleanup_error;
                std::filesystem::remove(candidate, cleanup_error);
                throw std::runtime_error("Failed to resolve the runtime font cache directory path");
            }

            dir_ = std::move(absolute);
            owned_ = true;
            return;
        }

        throw std::runtime_error("Failed to create a unique runtime font cache directory");
    }

    ~CacheStore() {
        if (!owned_ || dir_.empty()) {
            return;
        }
        std::error_code error;
        std::filesystem::remove_all(dir_, error);
    }

    void clear() {
        if (!owned_) return;
        std::error_code error;
        std::filesystem::remove_all(dir_, error);
        if (error) throw std::system_error(error, "Failed to remove the owned runtime resource cache");
        owned_ = false;
        dir_.clear();
    }

    void retain() { ++leases_; }

    void release() noexcept {
        if (--leases_ == 0 && shutdown_pending_) {
            try {
                clear();
            } catch (const std::exception &error) {
                std::fprintf(stderr, "Runtime resource cleanup: %s\n", error.what());
            }
        }
    }

    void shutdown() {
        shutdown_pending_ = true;
        if (leases_ == 0) clear();
    }

    CacheStore(const CacheStore &) = delete;
    CacheStore &operator=(const CacheStore &) = delete;
    CacheStore(CacheStore &&) = delete;
    CacheStore &operator=(CacheStore &&) = delete;

    const std::filesystem::path &dir() {
        ensure_directory();
        return dir_;
    }

private:
    std::filesystem::path root_;
    std::filesystem::path dir_;
    bool owned_ = false;
    std::size_t leases_ = 0;
    bool shutdown_pending_ = false;
};

struct ResourceLease {
    explicit ResourceLease(std::shared_ptr<CacheStore> cache) : cache_(std::move(cache)) { cache_->retain(); }

    ~ResourceLease() { cache_->release(); }

    ResourceLease(const ResourceLease &) = delete;
    ResourceLease &operator=(const ResourceLease &) = delete;
private:
    std::shared_ptr<CacheStore> cache_;
};

struct TempFileGuard {
    explicit TempFileGuard(std::filesystem::path path) : path_(std::move(path)) {}

    ~TempFileGuard() {
        if (committed_ || path_.empty()) {
            return;
        }
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TempFileGuard(const TempFileGuard &) = delete;
    TempFileGuard &operator=(const TempFileGuard &) = delete;

    void commit() noexcept { committed_ = true; }

private:
    std::filesystem::path path_;
    bool committed_ = false;
};

struct ParsedFont {
    std::string family;
    bool bold = false;
    bool italic = false;
};

struct FreeTypeLibraryGuard {
    FT_Library library = nullptr;

    ~FreeTypeLibraryGuard() {
        if (library) {
            FT_Done_FreeType(library);
        }
    }
};

struct FreeTypeFaceGuard {
    FT_Face face = nullptr;

    ~FreeTypeFaceGuard() {
        if (face) {
            FT_Done_Face(face);
        }
    }
};

ParsedFont validate_font(const std::vector<std::uint8_t> &bytes, const std::string &input_source_path) {
    FT_Library library = nullptr;
    const FT_Error init_error = FT_Init_FreeType(&library);
    FreeTypeLibraryGuard library_guard{library};
    if (init_error != 0) {
        throw std::runtime_error("Failed to initialize FreeType for font validation: " + input_source_path);
    }

    FT_Face face = nullptr;
    const FT_Error face_error =
        FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte *>(bytes.data()), static_cast<FT_Long>(bytes.size()), 0, &face);
    FreeTypeFaceGuard face_guard{face};
    if (face_error != 0 || !face) {
        throw std::runtime_error("FreeType could not parse the font: " + input_source_path);
    }

    if (face->num_faces != 1) {
        throw std::runtime_error("Multi-face font files are not supported: " + input_source_path);
    }
    if (!FT_IS_SCALABLE(face)) {
        throw std::runtime_error("Font is not scalable: " + input_source_path);
    }
    if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0) {
        throw std::runtime_error("Font has no Unicode character map: " + input_source_path);
    }
    if (!face->family_name || face->family_name[0] == '\0') {
        throw std::runtime_error("Font has no family name: " + input_source_path);
    }

    ParsedFont parsed;
    parsed.family = face->family_name;
    parsed.bold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
    parsed.italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
    return parsed;
}

std::vector<std::uint8_t> read_resource_file(const std::string &input_source_path, std::uintmax_t limit, const std::string &kind) {
    if (input_source_path.empty() || input_source_path.find('\0') != std::string::npos) {
        throw std::runtime_error(kind + " path is empty or contains NUL");
    }

    const std::filesystem::path path = std::filesystem::u8path(input_source_path);
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        throw std::runtime_error(kind + " path is not a regular file: " + input_source_path);
    }

    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::runtime_error("Failed to read " + kind + " file size: " + input_source_path);
    }
    if (size == 0 || size > limit) {
        throw std::runtime_error(kind + " file is empty or exceeds " + std::to_string(limit) + " bytes: " + input_source_path);
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open " + kind + " file: " + input_source_path);
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size()) || input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("Failed to read the complete " + kind + " file: " + input_source_path);
    }
    return bytes;
}

// SoundFont 2/3 files are RIFF containers with form type "sfbk". The host must
// not initialise the audio engine just to validate runtime_set_soundfont, so
// this is a structural check only; the worker's engine parses the font fully.
void validate_sound_font_header(const std::vector<std::uint8_t> &bytes, const std::string &input_source_path) {
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "sfbk", 4) != 0) {
        throw std::runtime_error("Invalid SoundFont file: " + input_source_path);
    }
}

struct SoundFontRecord {
    RuntimeSoundFontInfo info;
    std::size_t size = 0;
};

std::string content_fingerprint(const std::vector<std::uint8_t> &bytes) {
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef *>(bytes.data()), static_cast<uInt>(bytes.size()));
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "crc32:%08x:%zu", static_cast<unsigned int>(crc), bytes.size());
    return buffer;
}

// The owner thread and the installed/shut-down state, shared by the asset store
// and the fonts-engine proxy it hands to the draw layer.

std::unique_ptr<draw::FontsEngine> create_engine(music::RuntimeFontsDatabase &database) {
    auto engine = std::make_unique<draw::FontsEngine>(database);
    engine->init();
    return engine;
}

} // namespace

struct RuntimeAssets::Impl {
    RuntimeFontsOwner owner;
    std::shared_ptr<CacheStore> cache_store;
    std::unique_ptr<RuntimeFontsDatabase> database;
    std::unique_ptr<RuntimeFontsEngine> proxy_engine;
    std::unique_ptr<draw::FontProvider> provider;
    int next_font_id = 1;
    std::vector<FontRecord> retained_fonts;
    std::vector<SoundFontRecord> retained_sound_fonts;

    std::size_t retained_resource_bytes() const {
        std::size_t result = 0;
        for (const auto &font : retained_fonts) result += font.bytes ? font.bytes->size() : 0;
        for (const auto &font : retained_sound_fonts) result += font.size;
        return result;
    }

    RuntimeFontInfo reactivate_retained(const FontRecord &retained);
};

RuntimeFontInfo RuntimeAssets::Impl::reactivate_retained(const FontRecord &retained) {
    RuntimeFontInfo result = retained.info;

    const std::shared_ptr<const ActiveTable> current = database->active_table();
    std::size_t index = current->size();
    bool already_active = false;
    for (std::size_t i = 0; i < current->size(); ++i) {
        if ((*current)[i].key == retained.key) {
            index = i;
            already_active = (*current)[i].info.id == retained.info.id;
            break;
        }
    }

    if (already_active) {
        return result;
    }

    auto next = std::make_shared<ActiveTable>(*current);
    if (index < next->size()) {
        (*next)[index] = retained;
    } else {
        next->push_back(retained);
    }

    auto new_engine = create_engine(*database);

    std::shared_ptr<const ActiveTable> published = std::move(next);
    database->publish(published);
    proxy_engine->set_engine(std::move(new_engine));
    return result;
}

RuntimeAssets::RuntimeAssets(const std::filesystem::path &cache_root) : impl_(std::make_unique<Impl>()) {
    if (!cache_root.empty()) {
        std::error_code error;
        std::filesystem::create_directories(cache_root, error);
    }
    impl_->cache_store = std::make_shared<CacheStore>(cache_root);
    impl_->database = std::make_unique<RuntimeFontsDatabase>(std::make_shared<ActiveTable>());
    impl_->proxy_engine = std::make_unique<RuntimeFontsEngine>(impl_->owner, impl_->cache_store, create_engine(*impl_->database));
    impl_->provider = std::make_unique<draw::FontProvider>(*impl_->database, *impl_->proxy_engine);
}

RuntimeAssets::~RuntimeAssets() {
    if (!impl_->owner.installed) return;
    try {
        shutdown();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Runtime resource cleanup: %s\n", error.what());
    }
}

music::RuntimeFontsDatabase &RuntimeAssets::fonts_database() { return *impl_->database; }

RuntimeFontsEngine &RuntimeAssets::fonts_engine() { return *impl_->proxy_engine; }

draw::FontProvider &RuntimeAssets::font_provider() { return *impl_->provider; }

void RuntimeAssets::shutdown() {
    if (!impl_->owner.installed) return;
    impl_->owner.check();
    // Release the face cache and retained files explicitly; leases keep the
    // cache directory until the last one is released.
    impl_->proxy_engine->set_engine(nullptr);
    impl_->cache_store->shutdown();
    impl_->retained_fonts.clear();
    impl_->retained_sound_fonts.clear();
    impl_->owner.installed = false;
}

std::shared_ptr<const void> RuntimeAssets::retain() {
    impl_->owner.check();
    return std::make_shared<ResourceLease>(impl_->cache_store);
}

RuntimeFontInfo RuntimeAssets::add_font(const std::string &source_path) {
    Impl &d = *impl_;
    d.owner.check();

    std::vector<std::uint8_t> bytes = read_resource_file(source_path, ::music::k_max_font_file_bytes, "Font");

    for (const FontRecord &retained : d.retained_fonts) {
        if (retained.bytes && retained.bytes->size() == bytes.size() &&
            std::memcmp(retained.bytes->const_data(), bytes.data(), bytes.size()) == 0) {
            return d.reactivate_retained(retained);
        }
    }

    const ParsedFont parsed = validate_font(bytes, source_path);

    const std::size_t retained_bytes = d.retained_resource_bytes();
    if (d.retained_fonts.size() >= ::music::k_max_retained_fonts) {
        throw std::runtime_error("Maximum number of retained runtime fonts (32) exceeded");
    }
    if (bytes.size() > ::music::k_max_retained_bytes || retained_bytes > ::music::k_max_retained_bytes - bytes.size()) {
        throw std::runtime_error("Retained runtime resource bytes exceed the 256 MiB limit");
    }

    const muse::draw::FontDataKey key(muse::draw::Font::FontFamily(parsed.family), parsed.bold, parsed.italic);

    // Non-cryptographic content identity only; this is not a security hash.
    const std::string content_hash_value = content_fingerprint(bytes);

    const int font_id = d.next_font_id;
    static std::atomic<std::uint64_t> file_sequence{0};
    const std::string file_name =
        "font_" + std::to_string(font_id) + "_" + std::to_string(file_sequence.fetch_add(1, std::memory_order_relaxed)) + ".ttf";
    const std::filesystem::path destination = d.cache_store->dir() / file_name;
    TempFileGuard cache_file_guard(destination);

    {
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Failed to create cached runtime font: " + path_to_utf8(destination));
        }
        output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("Failed to write cached runtime font: " + path_to_utf8(destination));
        }
    }

    std::error_code absolute_error;
    const std::filesystem::path absolute_destination = std::filesystem::absolute(destination, absolute_error);
    if (absolute_error || absolute_destination.empty() || !absolute_destination.is_absolute()) {
        throw std::runtime_error("Failed to resolve the cached runtime font path");
    }
    const std::string retained_path = path_to_utf8(absolute_destination);

    std::shared_ptr<const muse::ByteArray> shared_bytes = std::make_shared<muse::ByteArray>(bytes.data(), bytes.size());

    FontRecord record;
    record.info.id = font_id;
    record.info.family = parsed.family;
    record.info.bold = parsed.bold;
    record.info.italic = parsed.italic;
    record.info.path = retained_path;
    record.info.content_hash = content_hash_value;
    record.key = key;
    record.bytes = std::move(shared_bytes);

    const std::shared_ptr<const ActiveTable> current = d.database->active_table();
    auto next_active = std::make_shared<ActiveTable>(*current);
    bool replaced = false;
    for (FontRecord &existing : *next_active) {
        if (existing.key == record.key) {
            existing = record;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        next_active->push_back(record);
    }

    std::vector<FontRecord> next_retained = d.retained_fonts;
    next_retained.push_back(record);

    RuntimeFontInfo result = record.info;
    auto new_engine = create_engine(*d.database);

    std::shared_ptr<const ActiveTable> published = std::move(next_active);
    d.database->publish(published);
    d.retained_fonts.swap(next_retained);
    d.proxy_engine->set_engine(std::move(new_engine));
    d.next_font_id = font_id + 1;
    cache_file_guard.commit();
    return result;
}

std::vector<RuntimeFontInfo> RuntimeAssets::fonts() const {
    const Impl &d = *impl_;
    d.owner.check();

    const std::shared_ptr<const ActiveTable> active = d.database->active_table();
    std::vector<RuntimeFontInfo> result;
    result.reserve(active->size());
    for (const FontRecord &record : *active) {
        result.push_back(record.info);
    }
    return result;
}

RuntimeSoundFontInfo RuntimeAssets::freeze_sound_font(const std::string &source_path) {
    Impl &d = *impl_;
    d.owner.check();
    const auto bytes = read_resource_file(source_path, ::music::k_max_sound_font_file_bytes, "SoundFont");
    validate_sound_font_header(bytes, source_path);
    const auto fingerprint = content_fingerprint(bytes);
    for (const auto &retained : d.retained_sound_fonts) {
        if (retained.size == bytes.size() && retained.info.content_hash == fingerprint &&
            read_resource_file(retained.info.path, ::music::k_max_sound_font_file_bytes, "Cached SoundFont") == bytes) {
            return retained.info; // Exact byte comparison; the fingerprint is only a prefilter.
        }
    }
    if (d.retained_sound_fonts.size() >= ::music::k_max_retained_sound_fonts) {
        throw std::runtime_error("Maximum number of retained SoundFonts (32) exceeded");
    }
    if (d.retained_resource_bytes() > ::music::k_max_retained_bytes - bytes.size()) {
        throw std::runtime_error("Retained runtime resource bytes exceed the 256 MiB limit");
    }
    const auto destination = d.cache_store->dir() / ("soundfont_" + std::to_string(d.retained_sound_fonts.size()) + ".sf2");
    TempFileGuard guard(destination);
    {
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output) throw std::runtime_error("Failed to write retained SoundFont");
    }
    RuntimeSoundFontInfo result{path_to_utf8(destination), fingerprint};
    auto next = d.retained_sound_fonts;
    next.push_back({result, bytes.size()});
    d.retained_sound_fonts.swap(next);
    guard.commit();
    return result;
}

} // namespace music
