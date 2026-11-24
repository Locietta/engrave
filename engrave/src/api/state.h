#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "engrave/src/api/batch.h"
#include "engrave/src/audio/score_audio.h"
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/runtime/score_io.h"
#include "engraving/dom/tempotimeline.h"
#include <engrave/document.h>
#include <engrave/engine.h>

namespace mu::engraving {
struct MasterScore;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct LayoutDataStore;
} // namespace mu::engraving::rendering::score

namespace engrave {

// Contract checks stay on in release builds: they guard process-global engine
// state that a violation would corrupt.
void check(bool condition, std::string_view message) noexcept;

struct EngineState {
    std::thread::id owner = std::this_thread::get_id();
    std::filesystem::path resources;
    // Where a render_audio writes its temporary file. Set by Engine::create.
    std::filesystem::path cache_dir;
    std::vector<std::filesystem::path> soundfonts;
    std::int32_t audio_sample_rate = 44100;
    std::uint64_t font_generation = 0;
    unsigned audio_render_index = 0;
    std::unique_ptr<music::EngineContext> context;

    static void release(std::shared_ptr<EngineState> state) noexcept;

    void check_thread() const noexcept {
        check(std::this_thread::get_id() == owner, "engrave called from a thread other than the Engine's");
    }
};

struct Engine::Impl {
    std::shared_ptr<EngineState> state;
    Result<std::unique_ptr<Document>> open_input(const std::string &bytes, const OpenOptions &options);
};

struct Document::Impl {
    struct Snapshot {
        struct Versions {
            std::u16string version;
            int revision = 0;
            int msc_version = 0;
        };

        std::string bytes;
        std::vector<Versions> versions;
        mu::engraving::RuntimeTempoState runtime_tempo;
        std::uint64_t source_revision = 0;
        std::uint64_t saved_revision = 0;
        std::uint64_t revision = 0;
        std::size_t byte_count = 0;
    };

    struct Checkpoint {
        std::string name;
        Snapshot snapshot;
        std::size_t byte_count = 0;
    };

    Result<Snapshot> capture_snapshot(const std::function<bool()> &cancelled = {}, std::size_t byte_limit = 128 * 1024 * 1024,
                                      std::optional<std::size_t> checkpoint_name_size = {}) const;
    Result<void> restore_snapshot(const Snapshot &snapshot, bool advance_revision);

    std::vector<Checkpoint> checkpoints;
    std::size_t checkpoint_byte_limit = 64 * 1024 * 1024;
    ~Impl();
    std::shared_ptr<EngineState> engine;
    // Declared before project: payloads remain available throughout DOM teardown.
    using LayoutOutputs = std::vector<std::unique_ptr<mu::engraving::rendering::score::LayoutDataStore>>;
    LayoutOutputs layout_outputs;
    music::ScoreProject project;
    // The source MSCZ's audiosettings.json, read before the DOM exists; a
    // document from any other format, or from a template, has neutral values.
    music::ScoreAudioMixDefaults audio_defaults;
    std::uint64_t revision = 1;
    std::vector<Diagnostic> diagnostics;
    // Source mutation failed after writing; reads remain available.
    bool poisoned = false;
    void bind_layout_boundary(mu::engraving::MasterScore &master, LayoutOutputs &outputs);

    mu::engraving::rendering::score::LayoutDataStore &adopt_layout_output(mu::engraving::Score &score) {
        return adopt_layout_output(score, layout_outputs);
    }

    static mu::engraving::rendering::score::LayoutDataStore &adopt_layout_output(mu::engraving::Score &score, LayoutOutputs &outputs);
    void ensure_layout(mu::engraving::Score &score);
    void discard_layout_output(mu::engraving::Score &score);

    static std::unique_ptr<Document> make(const std::shared_ptr<EngineState> &engine, music::ScoreProject project,
                                          music::ScoreAudioMixDefaults audio_defaults, std::vector<Diagnostic> diagnostics);
};

// Internal access for engine integration and tests.
struct DocumentAccess {
    static Document::Impl &impl(Document &document) { return *document.impl_; }

    static mu::engraving::MasterScore *master_score(Document &document);
    static music::ScoreProject &project(Document &document);
    static music::EngineContext &context(Document &document);
};

// The path as UTF-8 with '/' separators: the engine's path type reads
// "E:\a" as a relative path.
std::string utf8(const std::filesystem::path &path);

// The index of the excerpt the selection names, or -1 for the master score, as
// the audio driver names it. NotFound, StaleHandle and InvalidArgument as
// select_score reports them.
Result<int> excerpt_id_of(mu::engraving::MasterScore &master, const ScoreSel &selection);

} // namespace engrave

namespace engrave {
std::filesystem::path path_from_utf8(std::string_view text);
} // namespace engrave
