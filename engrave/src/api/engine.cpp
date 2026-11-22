#include <engrave/engine.h>

#include <algorithm>
#include <cstdio>
#include <format>
#include <fstream>
#include <iterator>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_set>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/mnx.h"
#include "engrave/src/api/sniff.h"
#include "engrave/src/api/state.h"
#include "engrave/src/runtime/runtime_assets.h"
#include "engrave/src/runtime/version.h"
#include "engrave/src/score/json.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/text.h"
#include "engraving/engravingproject.h"

namespace engrave {
namespace {
std::mutex s_engine_slot_mutex;
std::weak_ptr<EngineState> s_active_engine;

std::unexpected<Error> fail(ErrorCode code, std::string message) { return std::unexpected(Error{code, std::move(message)}); }

const char *backend_name(Format format) {
    switch (format) {
        case Format::MSCZ: return "mscz";
        case Format::MSCX: return "mscx";
        case Format::MUSIC_XML: return "musicxml";
        case Format::MXL: return "mxl";
        case Format::MIDI: return "midi";
        case Format::GUITAR_PRO: return "gp";
        case Format::MNX: return "mnx";
    }
    return "";
}

// The upstream template names carry an ordering prefix: "01-Treble_Clef".
std::string template_id(const std::filesystem::path &mscx) {
    std::string stem = utf8(mscx.stem());
    const auto dash = stem.find('-');
    if (dash != std::string::npos && dash > 0 &&
        std::all_of(stem.begin(), stem.begin() + dash, [](unsigned char c) { return c >= '0' && c <= '9'; }))
        stem.erase(0, dash + 1);
    return stem;
}

// The metadata half of upstream's MasterNotation::applyOptions for a score
// created from a template: the meta tags and the title frame's texts.
void apply_metadata(mu::engraving::MasterScore *score, const CreateOptions &options) {
    using mu::engraving::TextStyleType;

    struct Field {
        const char16_t *tag;
        TextStyleType style;
        const std::string &value;
    };

    const Field fields[] = {
        {u"workTitle", TextStyleType::TITLE, options.title},
        {u"subtitle", TextStyleType::SUBTITLE, options.subtitle},
        {u"composer", TextStyleType::COMPOSER, options.composer},
        {u"lyricist", TextStyleType::LYRICIST, options.lyricist},
    };
    mu::engraving::ScoreLoad load;
    for (const Field &field : fields) {
        if (field.value.empty()) continue;
        const muse::String text = muse::String::from_std_string(field.value);
        score->set_meta_tag(field.tag, text);
        mu::engraving::TextBase *item = score->get_text(field.style);
        if (!item) item = score->add_text(field.style);
        if (item) item->set_plain_text(text);
    }
    score->set_layout_all();
    mu::engraving::normalize_document(*score, {.mode = mu::engraving::NormalizeMode::LOAD});
    score->update();
}
} // namespace

Engine::Engine(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

void EngineState::release(std::shared_ptr<EngineState> state) noexcept {
    // The weak slot expires before teardown starts. Keep startup excluded until
    // the final owner's reset finishes cleaning up process globals.
    const std::scoped_lock lock(s_engine_slot_mutex);
    state.reset();
}

Result<std::unique_ptr<Engine>> Engine::create(const EngineOptions &options) {
    std::error_code ec;
    const auto resources = std::filesystem::absolute(options.resources, ec);
    if (options.resources.empty() || !std::filesystem::is_regular_file(resources / "engraving/instruments/instruments.xml", ec))
        return fail(ErrorCode::NOT_FOUND, "no engrave resources in '" + utf8(options.resources) + "'");
    for (const auto &font : options.fonts) {
        if (!std::filesystem::is_regular_file(font, ec)) return fail(ErrorCode::NOT_FOUND, "no font file '" + utf8(font) + "'");
    }
    if (options.audio_sample_rate < 8000 || options.audio_sample_rate > 192000)
        return fail(ErrorCode::INVALID_ARGUMENT,
                    std::format("the audio sample rate must be between 8000 and 192000 Hz, not {}", options.audio_sample_rate));
    for (const auto &soundfont : options.soundfonts) {
        if (!std::filesystem::is_regular_file(soundfont, ec))
            return fail(ErrorCode::NOT_FOUND, "no SoundFont file '" + utf8(soundfont) + "'");
        const auto extension = soundfont.extension().u8string();
        if (extension != u8".sf2" && extension != u8".sf3")
            return fail(ErrorCode::INVALID_ARGUMENT, "a SoundFont is an .sf2 or .sf3 file, not '" + utf8(soundfont) + "'");
    }
    const std::scoped_lock lock(s_engine_slot_mutex);
    if (!s_active_engine.expired()) return fail(ErrorCode::UNSUPPORTED, "an Engine or one of its Documents is still active");

    auto state = std::make_shared<EngineState>();
    try {
        state->resources = resources;
        state->cache_dir =
            options.cache_dir.empty() ? std::filesystem::temp_directory_path(ec) : std::filesystem::absolute(options.cache_dir);
        for (const auto &soundfont : options.soundfonts) state->soundfonts.push_back(std::filesystem::absolute(soundfont));
        state->audio_sample_rate = options.audio_sample_rate;
        state->context =
            std::make_unique<music::EngineContext>(utf8(resources), options.cache_dir.empty() ? std::string() : utf8(state->cache_dir));
        mu::engraving::MScore::test_mode = options.deterministic_ids;
        for (const auto &font : options.fonts) state->context->assets().add_font(utf8(std::filesystem::absolute(font)));
    } catch (const std::bad_alloc &) {
        throw;
    } catch (const std::exception &error) {
        return fail(ErrorCode::INVALID_INPUT, error.what());
    }
    std::unique_ptr<Engine> engine(new Engine(std::make_unique<Impl>(state)));
    s_active_engine = state;
    return engine;
}

Result<void> Engine::add_font(const std::filesystem::path &file) {
    impl_->state->check_thread();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) return fail(ErrorCode::NOT_FOUND, "no font file '" + utf8(file) + "'");
    try {
        // addFont publishes a new registry and replaces the font engine, discarding
        // its cached FreeType faces and glyph metrics before documents relayout.
        impl_->state->context->assets().add_font(utf8(std::filesystem::absolute(file)));
    } catch (const std::runtime_error &error) {
        return fail(ErrorCode::INVALID_INPUT, error.what());
    }
    ++impl_->state->font_generation;
    return {};
}

std::vector<RuntimeFontInfo> Engine::runtime_fonts() const {
    impl_->state->check_thread();
    const auto active = impl_->state->context->assets().fonts();
    std::vector<RuntimeFontInfo> result;
    result.reserve(active.size());
    for (const auto &font : active) {
        result.push_back({font.id, font.family, font.bold, font.italic, path_from_utf8(font.path), font.content_hash});
    }
    return result;
}

Engine::~Engine() {
    impl_->state->check_thread();
    EngineState::release(std::move(impl_->state));
}

Capabilities Engine::capabilities() const {
    impl_->state->check_thread();
    return {
        .engine_version = music::engine_version(),
        .import_formats = {Format::MSCZ, Format::MSCX, Format::MUSIC_XML, Format::MXL, Format::MIDI, Format::GUITAR_PRO, Format::MNX},
        .export_formats = {Format::MSCX, Format::MSCZ, Format::MUSIC_XML, Format::MXL, Format::MIDI},
        .render_formats = {RenderFormat::PDF, RenderFormat::PNG, RenderFormat::SVG},
        .audio_formats = {AudioFormat::WAV, AudioFormat::FLAC, AudioFormat::OPUS, AudioFormat::MP3},
        .soundfonts = impl_->state->soundfonts,
        .audio_sample_rate = impl_->state->audio_sample_rate,
    };
}

std::vector<TemplateInfo> Engine::templates() const {
    impl_->state->check_thread();
    const auto root = impl_->state->resources / "templates";
    std::ifstream file(root / "categories.json", std::ios::binary);
    if (!file) return {};
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const auto parsed = music::parse_json_expected<music::Json>(text);
    if (!parsed) throw std::runtime_error("templates/categories.json: " + glz::format_error(parsed.error(), text));
    if (!parsed->is_array()) throw std::runtime_error("templates/categories.json: expected a JSON array");
    const auto &categories = *parsed;
    std::vector<TemplateInfo> result;
    for (const auto &category : categories.get_array()) {
        for (const auto &entry : category.at("files").get_array()) {
            const auto mscx = root / path_from_utf8(entry.get<std::string>());
            std::string id = template_id(mscx);
            std::string title = id;
            std::ranges::replace(title, '_', ' ');
            result.push_back({std::move(id), std::move(title), category.at("title").get<std::string>(), mscx});
        }
    }
    return result;
}

std::vector<InstrumentInfo> Engine::instruments() const {
    impl_->state->check_thread();
    std::vector<InstrumentInfo> result;
    std::unordered_set<std::string> seen;
    for (const auto *group : mu::engraving::instrument_catalog().groups) {
        for (const auto *entry : group->instrument_templates) {
            auto id = entry->id.to_std_string();
            if (!seen.insert(id).second) continue;

            InstrumentInfo info{
                .id = std::move(id),
                .name = entry->track_name.to_std_string(),
                .group_id = group->id.to_std_string(),
                .staff_count = static_cast<int>(entry->staff_count),
                .transpose = {entry->transpose.diatonic, entry->transpose.chromatic},
                .uses_drumset = entry->use_drumset,
            };
            if (info.uses_drumset) {
                const auto instrument = mu::engraving::Instrument::from_template(entry);
                const auto &kit = *instrument.drumset();
                for (int key = 0; key < mu::engraving::drum_instruments; ++key) {
                    if (kit.is_valid(key)) info.drums.push_back({key, kit.line(key), kit.voice(key), kit.name(key).to_std_string()});
                }
            }
            result.push_back(std::move(info));
        }
    }
    return result;
}

Result<std::unique_ptr<Document>> Engine::open(std::span<const std::byte> data, const OpenOptions &options) {
    impl_->state->check_thread();
    if (data.empty()) return fail(ErrorCode::INVALID_INPUT, "the input is empty");
    if (data.size() > max_input_bytes) return fail(ErrorCode::RESOURCE_LIMIT, std::format("the input exceeds {} bytes", max_input_bytes));
    const std::string bytes(reinterpret_cast<const char *>(data.data()), data.size());

    std::vector<Diagnostic> diagnostics;
    std::optional<Format> format = options.format ? options.format : sniff_format(bytes);
    if (!format) {
        format = format_from_extension(options.filename_hint);
        if (!format) return fail(ErrorCode::UNSUPPORTED_FORMAT, "the data matches no score format the engine reads");
        diagnostics.push_back(
            {Severity::INFO, std::format("the content was not recognised; reading it as {} because of its file name", to_string(*format))});
    }
    music::ScoreProject project;
    try {
        if (*format == Format::MNX) {
            auto opened = open_mnx(*impl_->state->context, bytes);
            if (!opened) return fail(ErrorCode::INVALID_INPUT, std::format("cannot read the data as MNX: {}", opened.error()));
            project = *std::move(opened);
        } else {
            project = music::load_native_score(*impl_->state->context, bytes, backend_name(*format));
        }
    } catch (const music::UnsupportedScoreVersion &error) {
        return fail(ErrorCode::UNSUPPORTED, error.what());
    } catch (const std::bad_alloc &) {
        throw;
    } catch (const std::exception &error) {
        return fail(ErrorCode::INVALID_INPUT, std::format("cannot read the data as {}: {}", to_string(*format), error.what()));
    }
    // An MSCZ can carry the project's default mixer settings. Read them from the
    // input, before the DOM exists and before anything can rewrite the archive.
    music::ScoreAudioMixDefaults audio_defaults;
    if (*format == Format::MSCZ) audio_defaults = music::score_audio_mix_defaults(bytes);
    if (project->is_corrupted_upon_loading())
        diagnostics.push_back({Severity::WARNING, "the score failed the engine's sanity check after loading"});
    auto repairs = id_repairs(*project->master_score());
    if (options.strict_ids && !repairs.empty()) {
        return fail(ErrorCode::INVALID_INPUT, "the score has malformed or duplicated element ids: " + repairs.front().message);
    }
    diagnostics.insert(diagnostics.end(), repairs.begin(), repairs.end());
    return Document::Impl::make(impl_->state, std::move(project), std::move(audio_defaults), std::move(diagnostics));
}

Result<std::unique_ptr<Document>> Engine::open(const std::filesystem::path &file, const OpenOptions &options) {
    impl_->state->check_thread();
    std::error_code ec;
    const auto status = std::filesystem::status(file, ec);
    if (!std::filesystem::exists(status)) return fail(ErrorCode::NOT_FOUND, "no file '" + utf8(file) + "'");
    if (!std::filesystem::is_regular_file(status)) return fail(ErrorCode::INVALID_ARGUMENT, "'" + utf8(file) + "' is not a file");
    const auto size = std::filesystem::file_size(file, ec);
    if (ec) return fail(ErrorCode::IO, "cannot read '" + utf8(file) + "': " + ec.message());
    if (size > max_input_bytes) return fail(ErrorCode::RESOURCE_LIMIT, std::format("'{}' exceeds {} bytes", utf8(file), max_input_bytes));

    std::vector<std::byte> data(size);
    std::ifstream in(file, std::ios::binary);
    if (!in.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size)))
        return fail(ErrorCode::IO, "cannot read '" + utf8(file) + "'");
    OpenOptions effective = options;
    if (effective.filename_hint.empty()) effective.filename_hint = file;
    return open(data, effective);
}

Result<std::unique_ptr<Document>> Engine::create_document(const CreateOptions &options) {
    impl_->state->check_thread();
    if (options.template_id.empty() == options.template_path.empty())
        return fail(ErrorCode::INVALID_ARGUMENT, "set exactly one of template_id and template_path");
    auto mscx = options.template_path;
    if (!options.template_id.empty()) {
        const auto all = templates();
        const auto it = std::ranges::find(all, options.template_id, &TemplateInfo::id);
        if (it == all.end()) return fail(ErrorCode::NOT_FOUND, "no template '" + options.template_id + "'");
        mscx = it->path;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(mscx, ec)) return fail(ErrorCode::NOT_FOUND, "no template file '" + utf8(mscx) + "'");
    if (mscx.extension() != ".mscx") return fail(ErrorCode::INVALID_ARGUMENT, "a template is an .mscx file, not '" + utf8(mscx) + "'");

    music::ScoreProject project;
    try {
        project = music::load_native_score_dir(*impl_->state->context, utf8(std::filesystem::absolute(mscx)));
    } catch (const music::UnsupportedScoreVersion &error) {
        return fail(ErrorCode::UNSUPPORTED, error.what());
    } catch (const std::bad_alloc &) {
        throw;
    } catch (const std::exception &error) {
        return fail(ErrorCode::INVALID_INPUT, std::format("cannot read the template '{}': {}", utf8(mscx), error.what()));
    }
    apply_metadata(project->master_score(), options);
    return Document::Impl::make(impl_->state, std::move(project), {}, {});
}

} // namespace engrave
