#include <engrave/document.h>

#include <format>
#include <iterator>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/score/export_options.h"
#include "engrave/src/score/write470.h"
#include "global/io/writebudget.h"

#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/textbase.h"
#include "engraving/engravingproject.h"

namespace engrave {

Result<int> excerpt_id_of(mu::engraving::MasterScore &master, const ScoreSel &selection) {
    const Result<mu::engraving::Score *> selected = select_score(master, selection);
    if (!selected) return std::unexpected(selected.error());
    if ((*selected)->isMaster()) return -1;
    const std::vector<mu::engraving::Excerpt *> &excerpts = master.excerpts();
    for (std::size_t index = 0; index < excerpts.size(); ++index) {
        if (excerpts[index] && excerpts[index]->excerptScore() == *selected) return static_cast<int>(index);
    }
    return std::unexpected(Error{ErrorCode::StaleHandle, "the selected excerpt is not part of the document"});
}

std::unique_ptr<Document> Document::Impl::make(EngineState &engine, music::ScoreProject project,
                                               music::ScoreAudioMixDefaults audio_defaults, std::vector<Diagnostic> diagnostics) {
    auto impl = std::make_unique<Impl>();
    impl->engine = &engine;
    impl->font_generation = engine.font_generation;
    impl->project = std::move(project);
    impl->audio_defaults = std::move(audio_defaults);
    impl->diagnostics = std::move(diagnostics);
    // Every path that produces a document (open, create, and later a restore) hands
    // out ids here, so that an element always has one before a caller can name it.
    assign_missing_ids(*impl->project->masterScore());
    ++engine.live_documents;
    return std::unique_ptr<Document>(new Document(std::move(impl)));
}

void Document::Impl::refresh_fonts() {
    if (font_generation == engine->font_generation) return;
    for (auto *score : project->masterScore()->scoreList()) {
        // Rebuild text blocks as well as geometry: justified text can retain
        // font-dependent fragment splits from an earlier layout.
        score->scanElements([](mu::engraving::EngravingItem *item) {
            if (item->isTextBase()) mu::engraving::toTextBase(item)->mutldata()->layoutInvalid = true;
        });
        score->setLayoutAll();
        score->doLayout();
        score->update();
    }
    font_generation = engine->font_generation;
}

Document::Document(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

mu::engraving::MasterScore *DocumentAccess::master_score(Document &document) { return document.impl_->project->masterScore(); }

music::ScoreProject &DocumentAccess::project(Document &document) { return document.impl_->project; }

music::EngineContext &DocumentAccess::context(Document &document) { return *document.impl_->engine->context; }

api::EditHooks &DocumentAccess::edit_hooks(Document &document) { return document.impl_->edit_hooks; }

Document::~Document() {
    impl_->engine->check_thread();
    --impl_->engine->live_documents;
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
        Error error{ErrorCode::Unsupported, "this export has warnings; use export_score_with_warnings"};
        error.export_warnings = std::move(result->warnings);
        return std::unexpected(std::move(error));
    }
    return std::move(result->bytes);
}

Result<ExportOutput> Document::export_score_with_warnings(const ExportOptions &options) {
    impl_->engine->check_thread();
    if (impl_->poisoned) return std::unexpected(Error{ErrorCode::Poisoned, "an earlier edit left the document unusable"});
    if (options.capture.cancelled && options.capture.cancelled())
        return std::unexpected(Error{ErrorCode::Cancelled, "score capture cancelled"});
    impl_->refresh_fonts();
    const char *name = nullptr;
    switch (options.format) {
        case Format::Mscz: name = "mscz"; break;
        case Format::Mscx: name = "mscx"; break;
        case Format::MusicXml: name = "musicxml"; break;
        case Format::Mxl: name = "mxl"; break;
        case Format::Midi: name = "midi"; break;
        case Format::Mnx:
        case Format::GuitarPro:
        default:
            return std::unexpected(Error{ErrorCode::Unsupported, std::format("the engine does not write {}", to_string(options.format))});
    }
    const bool native = options.format == Format::Mscz || options.format == Format::Mscx;
    if (options.native_version != "5.00" && (!native || options.native_version != "4.70")) {
        Error error{ErrorCode::Unsupported,
                    std::format("native score version '{}' is not supported for {}", options.native_version, to_string(options.format))};
        error.export_warnings.push_back({"B470_UNSUPPORTED_TARGET", "Cannot write the requested score version.",
                                         ExportWarningObject{"request", "version:" + options.native_version}});
        return std::unexpected(std::move(error));
    }
    if (options.format == Format::Midi && (options.capture.max_bytes || options.capture.max_working_bytes || options.capture.cancelled)) {
        return std::unexpected(Error{ErrorCode::Unsupported, "bounded MIDI capture is not supported by the playback-event builder"});
    }
    const auto excerpt = excerpt_id_of(*impl_->project->masterScore(), options.score);
    if (!excerpt) return std::unexpected(excerpt.error());
    const auto selected = select_score(*impl_->project->masterScore(), options.score);
    if (!selected) return std::unexpected(selected.error());
    if ((*selected)->npages() == 0 && (*selected)->nmeasures() > 0) {
        (*selected)->doLayout();
        (*selected)->update();
    }
    music::ExportOptions backend;
    backend.excerpt_id = *excerpt;
    backend.standalone_score = true;
    backend.midi_expand_repeats = options.midi_expand_repeats;
    backend.midi_export_rpns = options.midi_rpns;
    if (!options.capture.max_bytes && !options.capture.max_working_bytes && !options.capture.cancelled) {
        if (options.native_version == "4.70")
            return write470::export_score(*impl_->engine->context, impl_->project, **selected, name, backend);
        const std::string data = music::export_native_score(*impl_->engine->context, impl_->project, name, backend, {});
        const auto *bytes = reinterpret_cast<const std::byte *>(data.data());
        return ExportOutput{std::vector<std::byte>(bytes, bytes + data.size()), {}};
    }
    muse::io::WriteBudget budget;
    budget.output = options.capture.max_bytes;
    budget.working = options.capture.max_working_bytes;
    budget.cancelled = &options.capture.cancelled;
    if (!budget.working && budget.output) {
        budget.working = default_score_capture_working_bytes(*budget.output);
    }
    muse::io::WriteBudgetScope scope(budget);
    const auto stopped = [&]() -> std::unexpected<Error> {
        if (budget.callback_error) std::rethrow_exception(budget.callback_error);
        if (budget.failure == muse::io::WriteBudget::Failure::Cancelled)
            return std::unexpected(Error{ErrorCode::Cancelled, "score capture cancelled"});
        return std::unexpected(Error{ErrorCode::ResourceLimit, "score capture byte limit exceeded"});
    };
    try {
        if (options.native_version == "4.70") {
            auto result = write470::export_score(*impl_->engine->context, impl_->project, **selected, name, backend);
            if (!budget.poll()) return stopped();
            if (result && !budget.outputSize(result->bytes.size())) return stopped();
            return result;
        }
        const std::string data = music::export_native_score(*impl_->engine->context, impl_->project, name, backend, {});
        if (!budget.poll() || !budget.outputSize(data.size())) return stopped();
        const auto *bytes = reinterpret_cast<const std::byte *>(data.data());
        return ExportOutput{std::vector<std::byte>(bytes, bytes + data.size()), {}};
    } catch (...) {
        if (budget.failure != muse::io::WriteBudget::Failure::None) return stopped();
        throw;
    }
}

} // namespace engrave
