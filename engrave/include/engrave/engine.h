#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <engrave/describe.h>
#include <engrave/document.h>
#include <engrave/error.h>
#include <engrave/output.h>

namespace engrave {

struct EngineOptions {
    // Runtime assets: engraving/, fonts/, mpe/ and templates/.
    std::filesystem::path resources;
    // Parent of the engine's cache directory; empty means the system
    // temporary directory.
    std::filesystem::path cache_dir;
    // Extra text fonts, added in order.
    std::vector<std::filesystem::path> fonts;
    // SoundFonts the audio renderer may use, in order; the first is the default
    // one every track plays unless its AudioOptions name another. The audio
    // engine itself starts on the first render_audio, because it is
    // process-global and costs a synthesizer per font.
    std::vector<std::filesystem::path> soundfonts;
    // The audio sample rate, in Hz. Fixed for the Engine: its audio context is
    // created with it, so AudioOptions::sample_rate must agree.
    std::int32_t audio_sample_rate = 44100;
    // Sequential element ids instead of random ones, so that two opens of the same
    // file agree.  Only for tests that never round-trip a score: the sequence
    // collides with the ids a read-back score already has, and the reader then
    // renumbers them.  It also changes what the writer stores, so leave it off
    // otherwise.
    bool deterministic_ids = false;
    // Threads that synthesize a render's active tracks concurrently; the mix is
    // bit-identical for every value. 0 picks hardware_concurrency - 1, clamped
    // to 1..8; 1 synthesizes on the calling thread; larger values are used as
    // given, capped at 64. Negative values are INVALID_ARGUMENT. A positive
    // integer in the ENGRAVE_AUDIO_THREADS environment variable overrides it.
    // Callbacks still run on the Engine's thread, and a render with one active
    // track never fans out.
    std::int32_t audio_threads = 0;
};

struct Capabilities {
    std::string engine_version; // written into saved scores, e.g. "5.0.0"
    std::vector<Format> import_formats;
    std::vector<Format> export_formats;
    // Render formats and audio encoders this build has.
    std::vector<RenderFormat> render_formats;
    std::vector<AudioFormat> audio_formats;
    std::vector<std::filesystem::path> soundfonts; // current ordered selection
    std::int32_t audio_sample_rate = 0;
};

// An active runtime text font, as registered by EngineOptions::fonts or add_font.
// id identifies the retained copy within this Engine; it is reused when the
// same bytes are registered again and must not be compared across Engines.
struct RuntimeFontInfo {
    int id = 0;
    std::string family;
    bool bold = false;
    bool italic = false;
    // Absolute path to the engine-owned copy, not the caller's source file.
    // Treat it as read-only. It remains available until the Engine and its
    // Documents are destroyed, even if another font replaces this entry.
    // A snapshot does not extend that lifetime.
    std::filesystem::path path;
    // crc32:<8 lowercase hex>:<byte size>; non-cryptographic and may collide.
    std::string content_fingerprint;
};

struct TemplateInfo {
    std::string id;             // e.g. "Treble_Clef"
    std::string title;          // e.g. "Treble Clef"
    std::string category;       // e.g. "General"
    std::filesystem::path path; // the template's .mscx
};

struct DrumInfo {
    int key = 0;
    int line = 0;
    int default_voice = 0;
    std::string name;
};

struct InstrumentInfo {
    std::string id;
    std::string name;
    std::string group_id;
    int staff_count = 0;
    Transposition transpose;
    bool uses_drumset = false;
    std::vector<DrumInfo> drums;
};

// Byte inputs and file paths other than uncompressed score XML use this limit.
inline constexpr std::size_t max_input_bytes = 8 * 1024 * 1024;
inline constexpr std::size_t max_uncompressed_file_bytes = 64 * 1024 * 1024;

struct OpenOptions {
    // Unset: detect the format from the content, then from the extension of
    // filename_hint.
    std::optional<Format> format;
    // Only its extension is used. open(path) defaults it to the path.
    std::filesystem::path filename_hint;
    // Reject a score whose element ids are malformed or duplicated. By default
    // such a score opens, the affected elements get fresh ids, and a warning
    // diagnostic says how many were repaired.
    bool strict_ids = false;
};

// Exactly one of template_id and template_path must be set. Parts, measures,
// key and tempo come from the template.
struct CreateOptions {
    std::string template_id;             // a TemplateInfo::id
    std::filesystem::path template_path; // an .mscx in an MSCX directory
    std::string title;
    std::string subtitle;
    std::string composer;
    std::string lyricist;
};

// A process can use one engine runtime at a time. Every call on an Engine
// or its Documents, including destruction, must come from the thread
// that created the Engine. Calling from another thread aborts.
struct Engine {
    // Validates options before checking whether an Engine or its Documents
    // still hold the runtime. In that case create returns Unsupported.
    // A failed initialization releases the runtime for retry.
    static Result<std::unique_ptr<Engine>> create(const EngineOptions &options);

    ~Engine();
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    Capabilities capabilities() const;

    // Register an immutable copy of a text font. NotFound for a missing file,
    // InvalidInput for an unusable font. Existing documents keep their revision;
    // their next layout-dependent call relays out all scores.
    Result<void> add_font(const std::filesystem::path &file);

    // Snapshot of active runtime text fonts, excluding bundled fonts. Its size
    // is the registration count. A replacement for the same family and style
    // occupies the existing slot; the returned values are independent copies.
    std::vector<RuntimeFontInfo> runtime_fonts() const;

    // Replace the ordered SoundFont list, validating and retaining file copies.
    // Empty disables audio. Validation does not start audio or refresh the live
    // SoundFont handles; existing documents use the list on their next render.
    // Layout, revision and mix defaults are unchanged. Indices refer to this list.
    // capabilities().soundfonts returns the retained paths usable in AudioOptions.
    // NotFound for missing files, InvalidInput for malformed files. Failure keeps
    // the previous selection (validated copies may remain cached until shutdown).
    Result<void> set_soundfonts(const std::vector<std::filesystem::path> &files);

    // The templates listed in resources/templates/categories.json, in that
    // file's order.
    std::vector<TemplateInfo> templates() const;

    // Catalog order, with the first definition of each ID used by AddPart.
    std::vector<InstrumentInfo> instruments() const;

    // Unsupported for MSCX/MSCZ written before MuseScore 4.6 (mscVersion < 460).
    Result<std::unique_ptr<Document>> open(std::span<const std::byte> data, const OpenOptions &options = {});
    // Unsupported for MSCX/MSCZ written before MuseScore 4.6 (mscVersion < 460).
    Result<std::unique_ptr<Document>> open(const std::filesystem::path &file, const OpenOptions &options = {});

    // A new score from a template, at revision 1.
    Result<std::unique_ptr<Document>> create_document(const CreateOptions &options);

    // The catalogue: every element type the engine recognises, sorted by name,
    // with the properties its class handles, and every style in style id order.
    // The tables are static; no score is read.  NotFound for a type name the
    // engine does not know.
    std::vector<TypeInfo> describe() const;
    Result<TypeInfo> describe(std::string_view type) const;
    std::vector<StyleInfo> describe_styles() const;

private:
    struct Impl;
    explicit Engine(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace engrave
