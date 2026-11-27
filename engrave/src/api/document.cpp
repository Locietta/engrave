#include <engrave/document.h>

#include <exception>
#include <format>
#include <iterator>
#include <limits>
#include <span>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/score/export_options.h"
#include "engrave/src/score/write470.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/textbase.h"
#include "engraving/engravingproject.h"
#include "engraving/rendering/score/layoutdatastore.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "global/io/byteview.h"
#include "global/io/writebudget.h"
#include "global/serialization/zipreader.h"

namespace engrave {

Document::Impl::~Impl() = default;

mu::engraving::rendering::score::LayoutDataStore &Document::Impl::adopt_layout_output(mu::engraving::Score &score, LayoutOutputs &outputs) {
    using mu::engraving::rendering::score::LayoutDataStore;
    auto *existing = score.layout_data_store();
    if (std::ranges::any_of(outputs, [existing](const auto &output) { return output.get() == existing; })) return *existing;
    std::erase_if(outputs, [](const auto &output) { return output->score() == nullptr; });
    // reserve can fail; ownership and all live borrowers must be unchanged then.
    outputs.reserve(outputs.size() + 1);
    if (auto output = score.renderer()->take_layout_data_store(score)) {
        outputs.push_back(std::move(output));
        return *existing;
    }
    auto output = std::make_unique<LayoutDataStore>(score);
    auto *result = output.get();
    outputs.push_back(std::move(output));
    score.bind_layout_data_store(result);
    return *result;
}

Result<int> excerpt_id_of(mu::engraving::MasterScore &master, const ScoreSel &selection) {
    const Result<mu::engraving::Score *> selected = select_score(master, selection);
    if (!selected) return std::unexpected(selected.error());
    if ((*selected)->is_master()) return -1;
    const std::vector<mu::engraving::Excerpt *> &excerpts = master.excerpts();
    for (std::size_t index = 0; index < excerpts.size(); ++index) {
        if (excerpts[index] && excerpts[index]->excerpt_score() == *selected) return static_cast<int>(index);
    }
    return std::unexpected(Error{ErrorCode::STALE_HANDLE, "the selected excerpt is not part of the document"});
}

std::unique_ptr<Document> Document::Impl::make(const std::shared_ptr<EngineState> &engine, music::ScoreProject project,
                                               music::ScoreAudioMixDefaults audio_defaults, std::vector<Diagnostic> diagnostics) {
    auto impl = std::make_unique<Impl>();
    impl->engine = engine;
    impl->project = std::move(project);
    impl->project->master_score()->invalidate_repeat_list();
    impl->project->master_score()->init_automation();
    impl->bind_layout_boundary(*impl->project->master_score(), impl->layout_outputs);
    for (auto *score : impl->project->master_score()->score_list()) {
        score->layout_state.font_generation = engine->font_generation;
        score->layout_state.revision = impl->revision;
    }
    impl->audio_defaults = std::move(audio_defaults);
    impl->diagnostics = std::move(diagnostics);
    // Every path that produces a document (open, create, and later a restore) hands
    // out ids here, so that an element always has one before a caller can name it.
    assign_missing_ids(*impl->project->master_score());
    return std::unique_ptr<Document>(new Document(std::move(impl)));
}

void Document::Impl::bind_layout_boundary(mu::engraving::MasterScore &master, LayoutOutputs &outputs) {
    for (auto *score : master.score_list())
        if (score->layout_data_store()) adopt_layout_output(*score, outputs);
    master.layout_boundaries = std::make_unique<mu::engraving::LayoutBoundaries>();
    master.layout_boundaries->layout = [this](mu::engraving::Score &score) { ensure_layout(score); };
    master.layout_boundaries->store = [this](mu::engraving::Score &score) -> auto & { return adopt_layout_output(score); };
}

void Document::Impl::ensure_layout(mu::engraving::Score &score) {
    adopt_layout_output(score);
    std::exception_ptr error;
    try {
        if (score.layout_state.revision != revision && !score.layout_state.dirty)
            score.invalidate_layout(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(-1, 1));
        if (score.layout_state.font_generation != engine->font_generation) {
            // Justified fragments retain font-dependent splits, not only geometry.
            score.scan_elements([](mu::engraving::EngravingItem *item) {
                if (item->is_text_base()) mu::engraving::to_text_base(item)->mutldata()->layout_invalid = true;
            });
            score.invalidate_layout(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(-1, 1));
            score.layout_if_dirty();
            score.layout_state.font_generation = engine->font_generation;
        } else {
            score.layout_if_dirty();
        }
        score.layout_state.revision = revision;
    } catch (...) {
        error = std::current_exception();
    }
    if (error) {
        // LayoutContext and its borrowed payloads have unwound. A later geometry
        // request must rebuild a complete view, including after a range failure.
        discard_layout_output(score);
        std::rethrow_exception(error);
    }
}

void Document::Impl::discard_layout_output(mu::engraving::Score &score) {
    if (auto *output = score.layout_data_store())
        output->clear();
    else
        score.invalidate_layout(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(-1, 1));
}

Document::Document(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

mu::engraving::MasterScore *DocumentAccess::master_score(Document &document) { return document.impl_->project->master_score(); }

music::ScoreProject &DocumentAccess::project(Document &document) { return document.impl_->project; }

music::EngineContext &DocumentAccess::context(Document &document) { return *document.impl_->engine->context; }

Document::~Document() {
    impl_->engine->check_thread();
    auto engine = impl_->engine;
    impl_.reset();
    EngineState::release(std::move(engine));
}

Result<Document::Impl::Snapshot> Document::Impl::capture_snapshot(const std::function<bool()> &cancelled, std::size_t byte_limit,
                                                                  std::optional<std::size_t> checkpoint_name_size) const {
    muse::io::WriteBudget budget;
    const auto poll = [&]() -> bool { return cancelled(); };
    if (cancelled) budget.cancelled = muse::FnRef<bool() const>(poll);
    muse::io::WriteBudgetScope scope(budget);
    const auto stopped = [&]() -> std::unexpected<Error> {
        if (budget.failure == muse::io::WriteBudget::Failure::CANCELLED)
            return std::unexpected(Error{ErrorCode::CANCELLED, "snapshot capture cancelled"});
        return std::unexpected(Error{ErrorCode::RESOURCE_LIMIT, "snapshot capture byte limit exceeded"});
    };
    try {
        if (!budget.poll()) return stopped();
        auto &master = *project->master_score();
        auto remaining = byte_limit;
        const auto reserve = [&](std::size_t count, std::size_t width = 1) {
            if (count > remaining / width) return false;
            remaining -= count * width;
            return true;
        };
        if (!reserve(checkpoint_name_size ? sizeof(Checkpoint) : sizeof(Snapshot)) || !reserve(1) ||
            (checkpoint_name_size && (!reserve(*checkpoint_name_size) || !reserve(1))))
            return stopped();
        if (const auto *timeline = master.tempo_timeline_override()) {
            constexpr auto pause_bytes = sizeof(mu::engraving::PausesMap::value_type) + 4 * sizeof(void *);
            if (!reserve(timeline->points().size(), sizeof(mu::engraving::TempoTimePoint)) ||
                !reserve(timeline->pauses().size(), pause_bytes))
                return stopped();
        }
        const auto scores = master.score_list();
        if (!reserve(scores.size(), sizeof(Snapshot::Versions))) return stopped();
        for (const auto *score : scores) {
            if (!budget.poll() || !reserve(score->mscore_version().size(), sizeof(char16_t)) || !reserve(1, sizeof(char16_t)))
                return stopped();
        }
        budget.output = remaining;
        budget.archive_entry = std::min<std::size_t>(remaining, max_uncompressed_file_bytes);
        // XML escaping and temporary entry buffers also consume working memory.
        budget.working = 16 * static_cast<std::uint64_t>(remaining);
        Snapshot snapshot;
        auto exported = music::export_native_score(*engine->context, project, "mscz",
                                                   music::ExportOptions{.transaction_snapshot = true, .compress = false});
        if (!budget.poll()) return stopped();
        if (!exported)
            return std::unexpected(Error{ErrorCode::IO, std::format("cannot capture the snapshot: {}", exported.error().message)});
        snapshot.bytes = *std::move(exported);
        // Restore reads each snapshot entry at most once, so the central directory
        // bounds its decompression budget without inflating any entry.
        muse::io::ByteView buffer(std::span(reinterpret_cast<const std::uint8_t *>(snapshot.bytes.data()), snapshot.bytes.size()));
        buffer.open(muse::io::IODevice::READ_ONLY);
        muse::ZipReader zip(&buffer);
        if (zip.file_info_list().empty() || zip.has_error())
            return std::unexpected(Error{ErrorCode::RESOURCE_LIMIT, "the snapshot exceeds the native ZIP reader limits"});
        if (!budget.poll()) return stopped();
        snapshot.runtime_tempo = master.runtime_tempo_state();
        snapshot.source_revision = master.source_revision();
        snapshot.saved_revision = master.saved_revision();
        snapshot.revision = revision;
        snapshot.versions.reserve(scores.size());
        for (const auto *score : scores) {
            if (!budget.poll()) return stopped();
            snapshot.versions.push_back({score->mscore_version().to_std_u16_string(), score->mscore_revision(), score->msc_version()});
        }
        snapshot.byte_count = byte_limit - remaining + snapshot.bytes.size();
        if (!budget.poll()) return stopped();
        return snapshot;
    } catch (const muse::io::WriteBudgetStop &) {
        return stopped();
    } catch (const std::exception &error) {
        if (budget.failure != muse::io::WriteBudget::Failure::NONE) return stopped();
        return std::unexpected(Error{ErrorCode::IO, std::format("cannot capture the snapshot: {}", error.what())});
    }
}

Result<void> Document::checkpoint(std::string_view name, std::function<bool()> cancelled) {
    impl_->engine->check_thread();
    if (impl_->poisoned)
        return std::unexpected(Error{.code = ErrorCode::POISONED,
                                     .message = "restore_checkpoint or reopen the document before taking a checkpoint",
                                     .poisoned = true});
    auto available = impl_->checkpoint_byte_limit;
    for (const auto &entry : impl_->checkpoints) {
        if (entry.name == name) continue;
        if (entry.byte_count > available) return std::unexpected(Error{ErrorCode::RESOURCE_LIMIT, "checkpoint byte limit exceeded"});
        available -= entry.byte_count;
    }
    auto captured = impl_->capture_snapshot(cancelled, available, name.size());
    if (!captured) return std::unexpected(captured.error());
    const auto byte_count = captured->byte_count;
    Impl::Checkpoint saved{.name = std::string(name), .snapshot = std::move(*captured), .byte_count = byte_count};
    auto found = std::ranges::find(impl_->checkpoints, name, &Impl::Checkpoint::name);
    if (found == impl_->checkpoints.end())
        impl_->checkpoints.push_back(std::move(saved));
    else
        *found = std::move(saved);
    return {};
}

Result<void> Document::restore_checkpoint(std::string_view name) {
    impl_->engine->check_thread();
    const auto found = std::ranges::find(impl_->checkpoints, name, &Impl::Checkpoint::name);
    if (found == impl_->checkpoints.end()) return std::unexpected(Error{ErrorCode::NOT_FOUND, "checkpoint not found"});
    return impl_->restore_snapshot(found->snapshot, true);
}

Result<void> Document::Impl::restore_snapshot(const Snapshot &snapshot, bool advance_revision) try {
    if (advance_revision && revision == std::numeric_limits<std::uint64_t>::max())
        return std::unexpected(Error{ErrorCode::RESOURCE_LIMIT, "document revision exhausted"});
    LayoutOutputs restored_outputs;
    auto restored = music::load_native_score(*engine->context, snapshot.bytes, "mscz", true);
    if (!restored) return std::unexpected(Error{ErrorCode::IO, std::format("cannot restore the snapshot: {}", restored.error().message)});
    mu::engraving::ScoreLoad load;
    auto &master = *(*restored)->master_score();
    const auto scores = master.score_list();
    if (scores.size() != snapshot.versions.size())
        return std::unexpected(Error{ErrorCode::IO, "the restored snapshot has a different number of linked scores"});
    auto versions = snapshot.versions.begin();
    for (auto *score : scores) {
        score->set_mscore_version(muse::String(std::u16string_view(versions->version)));
        score->set_mscore_revision(versions->revision);
        score->set_msc_version(versions->msc_version);
        mu::engraving::invalidate_normalization(*score);
        ++versions;
    }
    mu::engraving::normalize_document(master, {.mode = mu::engraving::NormalizeMode::LOAD});
    master.invalidate_repeat_list();
    master.init_automation();
    master.restore_runtime_tempo_state(snapshot.runtime_tempo);
    master.restore_source_revisions(snapshot.source_revision, snapshot.saved_revision);
    assign_missing_ids(master);
    bind_layout_boundary(master, restored_outputs);
    const auto restored_revision = advance_revision ? revision + 1 : snapshot.revision;
    for (auto *score : scores) {
        score->layout_state.revision = restored_revision;
        score->layout_state.font_generation = engine->font_generation;
        score->invalidate_layout(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(-1, 1));
    }
    // The old project needs its layout stores throughout DOM teardown.
    project = *std::move(restored);
    layout_outputs.swap(restored_outputs);
    revision = restored_revision;
    poisoned = false;
    return {};
} catch (const std::exception &error) {
    return std::unexpected(Error{ErrorCode::IO, std::format("cannot restore the snapshot: {}", error.what())});
} catch (...) {
    return std::unexpected(Error{ErrorCode::IO, "cannot restore the snapshot: the engine threw an unknown exception"});
}

Result<void> Document::drop_checkpoint(std::string_view name) {
    impl_->engine->check_thread();
    const auto found = std::ranges::find(impl_->checkpoints, name, &Impl::Checkpoint::name);
    if (found == impl_->checkpoints.end()) return std::unexpected(Error{ErrorCode::NOT_FOUND, "checkpoint not found"});
    impl_->checkpoints.erase(found);
    return {};
}

std::uint64_t Document::revision() const {
    impl_->engine->check_thread();
    return impl_->revision;
}

const std::vector<Diagnostic> &Document::diagnostics() const {
    impl_->engine->check_thread();
    return impl_->diagnostics;
}

Result<std::vector<std::byte>> Document::export_score(const ExportOptions &options) {
    auto result = export_score_with_warnings(options);
    if (!result) return std::unexpected(std::move(result.error()));
    if (!result->warnings.empty()) {
        Error error{ErrorCode::UNSUPPORTED, "this export has warnings; use export_score_with_warnings"};
        error.export_warnings = std::move(result->warnings);
        return std::unexpected(std::move(error));
    }
    return std::move(result->bytes);
}

Result<ExportOutput> Document::export_score_with_warnings(const ExportOptions &options) {
    impl_->engine->check_thread();
    const bool native = options.format == Format::MSCZ || options.format == Format::MSCX;
    if (native && impl_->poisoned)
        return std::unexpected(
            Error{.code = ErrorCode::POISONED, .message = "restore_checkpoint or reopen the document before saving", .poisoned = true});

    if (options.capture.cancelled && options.capture.cancelled())
        return std::unexpected(Error{ErrorCode::CANCELLED, "score capture cancelled"});
    const char *name = nullptr;
    switch (options.format) {
        case Format::MSCZ: name = "mscz"; break;
        case Format::MSCX: name = "mscx"; break;
        case Format::MUSIC_XML: name = "musicxml"; break;
        case Format::MXL: name = "mxl"; break;
        case Format::MIDI: name = "midi"; break;
        case Format::MNX:
        case Format::GUITAR_PRO:
        default:
            return std::unexpected(Error{ErrorCode::UNSUPPORTED, std::format("the engine does not write {}", to_string(options.format))});
    }
    if (options.native_version != "5.00" && (!native || options.native_version != "4.70")) {
        Error error{ErrorCode::UNSUPPORTED,
                    std::format("native score version '{}' is not supported for {}", options.native_version, to_string(options.format))};
        error.export_warnings.push_back({"B470_UNSUPPORTED_TARGET", "Cannot write the requested score version.",
                                         ExportWarningObject{"request", "version:" + options.native_version}});
        return std::unexpected(std::move(error));
    }
    const auto excerpt = excerpt_id_of(*impl_->project->master_score(), options.score);
    if (!excerpt) return std::unexpected(excerpt.error());
    const auto selected = select_score(*impl_->project->master_score(), options.score);
    if (!selected) return std::unexpected(selected.error());
    if (!native && options.format != Format::MIDI) impl_->ensure_layout(**selected);
    music::ExportOptions backend;
    backend.excerpt_id = *excerpt;
    backend.standalone_score = true;
    backend.midi_expand_repeats = options.midi_expand_repeats;
    backend.midi_export_rpns = options.midi_rpns;
    if (!options.capture.max_bytes && !options.capture.max_working_bytes && !options.capture.cancelled) {
        if (options.native_version == "4.70")
            return write470::export_score(*impl_->engine->context, impl_->project, **selected, name, backend);
        auto data = music::export_native_score(*impl_->engine->context, impl_->project, name, backend);
        if (!data) return std::unexpected(std::move(data.error()));
        const auto *bytes = reinterpret_cast<const std::byte *>(data->data());
        return ExportOutput{std::vector<std::byte>(bytes, bytes + data->size()), {}};
    }
    const auto cancelled_poll = [&]() -> bool { return options.capture.cancelled && options.capture.cancelled(); };
    muse::io::WriteBudget budget;
    budget.output = options.capture.max_bytes;
    budget.working = options.capture.max_working_bytes;
    if (options.capture.cancelled) {
        budget.cancelled = muse::FnRef<bool() const>(cancelled_poll);
    }
    if (!budget.working && budget.output) {
        budget.working = default_score_capture_working_bytes(*budget.output);
    }
    muse::io::WriteBudgetScope scope(budget);
    const auto stopped = [&]() -> std::unexpected<Error> {
        if (budget.callback_error) std::rethrow_exception(budget.callback_error);
        if (budget.failure == muse::io::WriteBudget::Failure::CANCELLED)
            return std::unexpected(Error{ErrorCode::CANCELLED, "score capture cancelled"});
        return std::unexpected(Error{ErrorCode::RESOURCE_LIMIT, "score capture byte limit exceeded"});
    };
    std::exception_ptr caught;
    try {
        if (options.native_version == "4.70") {
            auto result = write470::export_score(*impl_->engine->context, impl_->project, **selected, name, backend);
            if (!budget.poll()) return stopped();
            if (result && !budget.output_size(result->bytes.size())) return stopped();
            return result;
        }
        auto data = music::export_native_score(*impl_->engine->context, impl_->project, name, backend);
        if (!data) return std::unexpected(std::move(data.error()));
        if (!budget.poll() || !budget.output_size(data->size())) return stopped();
        const auto *bytes = reinterpret_cast<const std::byte *>(data->data());
        return ExportOutput{std::vector<std::byte>(bytes, bytes + data->size()), {}};
    } catch (...) {
        caught = std::current_exception();
    }
    if (budget.failure != muse::io::WriteBudget::Failure::NONE) return stopped();
    std::rethrow_exception(caught);
}

} // namespace engrave
