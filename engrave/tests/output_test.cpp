// The render half of the output API: page_count() and render(), compared with
// the render goldens.
//
//   engrave_output_test --assets=<resources> --corpus=<file or dir>... --soundfont=<file>... [--native-fonts=<dir>]
//
// The comparison and its limits (what each layer proves) are in
// render_goldens.json and in the slice report:
//   - SVG is the stored golden's exact text, so its sha256 is compared;
//   - PNG is compared the way scripts/golden.py's describe_png does: the file is
//     decoded to the RGBA bytes PIL would see and hashed, and the size and mode
//     must match;
//   - a PDF cannot be normalized without PyMuPDF, so here it is checked to be a
//     PDF whose page count and size behave; the superproject's
//     scripts/test_a4_output.py compares the same files with describe_pdf.
// The audio tests are in audio_test.cpp; the Engine is shared.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "digest.h"
#include "draw/types/pen.h"
#include "draw/types/pixmap.h"
#include "engrave/src/score/json.h"
#include "output_fixture.h"
#include <engrave/engrave.h>

#ifdef _WIN32
#include <process.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace music::test {

std::unique_ptr<engrave::Engine> g_engine;
std::vector<std::filesystem::path> g_corpus;
std::vector<std::filesystem::path> g_soundfonts;
std::vector<std::filesystem::path> g_retained_for_cleanup;
std::filesystem::path g_assets;
std::filesystem::path g_self;
// The fonts directory of the Pixi environment that provides Cairo (--native-fonts=);
// by default the engine checkout's own environment.
std::filesystem::path g_native_fonts = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / ".pixi/envs/default/fonts";

std::filesystem::path corpus_file(std::string_view name) {
    for (const auto &path : g_corpus) {
        if (path.filename() == name) return path;
    }
    return {};
}

std::filesystem::path probe_binary() {
    const auto directory = g_self.parent_path();
    std::filesystem::path candidate = directory / "engrave_output_probe";
#ifdef _WIN32
    candidate += ".exe";
#endif
    return candidate;
}

int run_probe(const std::vector<std::string> &arguments) {
    std::vector<std::string> command;
    command.push_back(probe_binary().string());
    command.insert(command.end(), arguments.begin(), arguments.end());
#ifdef _WIN32
    std::vector<std::wstring> wide;
    wide.reserve(command.size());
    for (const std::string &argument : command) wide.push_back(std::filesystem::path(argument).wstring());
    std::vector<const wchar_t *> pointers;
    for (const std::wstring &argument : wide) pointers.push_back(argument.c_str());
    pointers.push_back(nullptr);
    return _wspawnv(_P_WAIT, std::filesystem::path(command.front()).wstring().c_str(), pointers.data());
#else
    std::vector<char *> pointers;
    for (std::string &argument : command) pointers.push_back(argument.data());
    pointers.push_back(nullptr);
    pid_t pid = 0;
    if (posix_spawn(&pid, command.front().c_str(), nullptr, nullptr, pointers.data(), ::environ) != 0) return -1;
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

namespace {

constexpr const char *golden_file_name = "render_goldens.json";

std::string platform_name() {
#ifdef _WIN32
    return "windows";
#else
    return "linux";
#endif
}

std::span<const std::byte> bytes_of(const std::vector<std::byte> &data) { return std::span<const std::byte>(data.data(), data.size()); }

std::string as_text(const std::vector<std::byte> &data) { return std::string(reinterpret_cast<const char *>(data.data()), data.size()); }

// scripts/golden.py reads and writes the SVG text as UTF-8 with universal
// newlines, so a CRLF from the canvas becomes LF in the golden.
std::vector<std::byte> with_lf_newlines(const std::vector<std::byte> &data) {
    std::vector<std::byte> result;
    result.reserve(data.size());
    for (const std::byte value : data) {
        if (value == std::byte{'\r'}) continue;
        result.push_back(value);
    }
    return result;
}

music::SortedJson load_goldens() {
    std::filesystem::path path = std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "engrave/tests" / golden_file_name;
    const char *env = std::getenv("ENGRAVE_RENDER_GOLDENS");
    if (env) path = std::filesystem::u8path(env);
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot read the render goldens in '" + path.string() + "'");
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    auto goldens = music::parse_json<music::SortedJson>(text);
    if (!goldens) throw std::runtime_error("the render goldens are not JSON");
    return std::move(*goldens);
}

struct OpenedScore {
    std::unique_ptr<engrave::Document> document;
};

OpenedScore open(const std::string &name) {
    const std::filesystem::path path = corpus_file(name);
    if (path.empty()) return {};
    auto result = g_engine->open(path);
    if (!result) {
        std::fprintf(stderr, "cannot open %s: %s\n", path.string().c_str(), result.error().message.c_str());
        return {};
    }
    return OpenedScore{std::move(*result)};
}

void expect_code(const engrave::Result<engrave::RenderOutput> &result, engrave::ErrorCode code, std::string_view what) {
    ASSERT_FALSE(result) << what << ": expected " << engrave::to_string(code) << ", got a value";
    EXPECT_EQ(result.error().code, code) << what << ": " << result.error().message;
}

} // namespace
} // namespace music::test

namespace {

using music::test::g_engine;

void collect(const std::filesystem::path &path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::directory_iterator(path, ec)) files.push_back(entry.path());
        std::ranges::sort(files);
        for (const auto &file : files) collect(file);
        return;
    }
    if (std::filesystem::is_regular_file(path, ec)) music::test::g_corpus.push_back(path);
}

TEST(Output, CapabilitiesListTheRenderAndAudioFormats) {
    const engrave::Capabilities capabilities = g_engine->capabilities();
    EXPECT_EQ(capabilities.render_formats,
              (std::vector<engrave::RenderFormat>{engrave::RenderFormat::PDF, engrave::RenderFormat::PNG, engrave::RenderFormat::SVG}));
    EXPECT_EQ(capabilities.audio_formats,
              (std::vector<engrave::AudioFormat>{engrave::AudioFormat::WAV, engrave::AudioFormat::FLAC, engrave::AudioFormat::OPUS,
                                                 engrave::AudioFormat::MP3, engrave::AudioFormat::AAC}));
    EXPECT_EQ(capabilities.soundfonts, music::test::g_soundfonts);
    EXPECT_EQ(capabilities.audio_sample_rate, 44100);
}

// The render golden comparison: every fixture of render_goldens.json, rendered with
// the options (144 dpi, transparent PNG, no page background, all pages).
TEST(Output, RenderMatchesTheA2Goldens) {
    const music::SortedJson goldens = music::test::load_goldens().at("fixtures");
    const std::string platform = music::test::platform_name();
    int compared = 0;
    for (const auto &[name, platforms] : goldens.get_object()) {
        if (!platforms.contains(platform)) continue;
        music::test::OpenedScore opened = music::test::open(name);
        ASSERT_TRUE(opened.document) << "the corpus has no " << name << " (pass --corpus=<directory>)";
        engrave::Document &document = *opened.document;
        const auto &golden = platforms.at(platform);

        const auto count = document.page_count();
        ASSERT_TRUE(count) << name << ": " << count.error().message;
        EXPECT_EQ(*count, golden.at("pdf").as<std::int32_t>()) << name << ": page count";

        engrave::RenderOptions options;
        options.format = engrave::RenderFormat::SVG;
        const auto svg = document.render(options);
        ASSERT_TRUE(svg) << name << ": " << svg.error().message;
        const auto &pages = golden.at("svg");
        ASSERT_EQ(svg->pages.size(), pages.size()) << name << ": rendered pages";
        for (std::size_t page = 0; page < svg->pages.size(); ++page) {
            EXPECT_EQ(music::test::sha256_hex(music::test::bytes_of(music::test::with_lf_newlines(svg->pages[page]))),
                      pages[page].get<std::string>())
                << name << ": page " << page << " SVG differs from the render golden";
        }

        options.format = engrave::RenderFormat::PNG;
        const auto png = document.render(options);
        ASSERT_TRUE(png) << name << ": " << png.error().message;
        ASSERT_EQ(png->pages.size(), svg->pages.size());
        std::string error;
        const auto image = music::test::decode_png(music::test::bytes_of(png->pages.front()), error);
        ASSERT_TRUE(image) << name << ": " << error;
        const auto &expected = golden.at("png");
        EXPECT_EQ(image->width, expected.at("width").as<std::uint32_t>()) << name << ": PNG width";
        EXPECT_EQ(image->height, expected.at("height").as<std::uint32_t>()) << name << ": PNG height";
        EXPECT_EQ(music::test::sha256_hex(image->rgba), expected.at("sha256").get<std::string>())
            << name << ": the RGBA pixels differ from the render golden";

        options.format = engrave::RenderFormat::PDF;
        const auto pdf = document.render(options);
        ASSERT_TRUE(pdf) << name << ": " << pdf.error().message;
        ASSERT_FALSE(pdf->document.empty());
        EXPECT_EQ(music::test::as_text(pdf->document).substr(0, 5), "%PDF-") << name << ": not a PDF";
        ++compared;
    }
    EXPECT_GE(compared, 3) << "no fixture was compared";
}

// The page range and the renderer's limits, on a two-page score and on a page
// that is too large at the top dpi.
TEST(Output, PageRangesAndLimits) {
    music::test::OpenedScore opened = music::test::open("slurs.mscz");
    ASSERT_TRUE(opened.document);
    engrave::Document &document = *opened.document;
    ASSERT_EQ(*document.page_count(), 2);

    engrave::RenderOptions all;
    all.format = engrave::RenderFormat::SVG;
    const auto everything = document.render(all);
    ASSERT_TRUE(everything);
    ASSERT_EQ(everything->pages.size(), 2u);

    all.first_page = 1;
    const auto second = document.render(all);
    ASSERT_TRUE(second);
    ASSERT_EQ(second->pages.size(), 1u);
    EXPECT_EQ(second->pages.front(), everything->pages.back()) << "the ranged render is not page 1";

    all.first_page = 0;
    all.last_page = 0;
    const auto first = document.render(all);
    ASSERT_TRUE(first);
    ASSERT_EQ(first->pages.size(), 1u);
    EXPECT_EQ(first->pages.front(), everything->pages.front());

    engrave::RenderOptions pdf;
    pdf.format = engrave::RenderFormat::PDF;
    const auto full = document.render(pdf);
    ASSERT_TRUE(full);
    pdf.first_page = 1;
    pdf.last_page = 1;
    const auto one = document.render(pdf);
    ASSERT_TRUE(one);
    EXPECT_EQ(music::test::as_text(one->document).substr(0, 5), "%PDF-");
    EXPECT_LT(one->document.size(), full->document.size()) << "a one-page PDF is not smaller than the two-page one";

    engrave::RenderOptions bad = all;
    bad.first_page = -1;
    music::test::expect_code(document.render(bad), engrave::ErrorCode::INVALID_ARGUMENT, "a negative first page");
    bad = all;
    bad.first_page = 1;
    bad.last_page = 0;
    music::test::expect_code(document.render(bad), engrave::ErrorCode::INVALID_ARGUMENT, "a reversed range");
    bad = all;
    bad.last_page = 5;
    music::test::expect_code(document.render(bad), engrave::ErrorCode::INVALID_ARGUMENT, "a range past the end");
    bad = all;
    bad.format = engrave::RenderFormat::PNG;
    bad.dpi = engrave::min_render_dpi - 1;
    music::test::expect_code(document.render(bad), engrave::ErrorCode::INVALID_ARGUMENT, "a dpi below the minimum");
    bad.dpi = engrave::max_render_dpi + 1;
    music::test::expect_code(document.render(bad), engrave::ErrorCode::INVALID_ARGUMENT, "a dpi above the maximum");
    bad = all;
    bad.dpi = 96;
    music::test::expect_code(document.render(bad), engrave::ErrorCode::INVALID_ARGUMENT, "a dpi on a format that takes none");

    // 8.27 x 11.69 inch at 1200 dpi is 139e6 pixels, over the renderer's 64e6.
    music::test::OpenedScore page = music::test::open("ghost_note.mscx");
    ASSERT_TRUE(page.document);
    engrave::RenderOptions huge;
    huge.format = engrave::RenderFormat::PNG;
    huge.dpi = engrave::max_render_dpi;
    music::test::expect_code(page.document->render(huge), engrave::ErrorCode::RESOURCE_LIMIT, "an oversized page");
}

TEST(Output, CancellationPollsPageBoundariesAndDiscardsAllPages) {
    // This fixture has exactly two pages on both supported platforms.
    auto opened = music::test::open("slurs.mscz");
    ASSERT_TRUE(opened.document);
    auto &document = *opened.document;
    ASSERT_EQ(*document.page_count(), 2);
    for (auto format : {engrave::RenderFormat::PDF, engrave::RenderFormat::SVG, engrave::RenderFormat::PNG}) {
        engrave::RenderOptions options;
        options.format = format;
        // PDF: 1 before page 1, 2 after drawing page 1, 3 before page 2,
        // 4 after drawing page 2, 5 after final encoding.
        // SVG/PNG: each page polls before draw, before encode, after encode.
        const int total = format == engrave::RenderFormat::PDF ? 5 : 6;
        for (int stop = 1; stop <= total; ++stop) {
            int polls = 0;
            options.cancelled = [&] { return ++polls == stop; };
            const auto result = document.render(options);
            ASSERT_FALSE(result) << stop;
            EXPECT_EQ(result.error().code, engrave::ErrorCode::CANCELLED);
            EXPECT_EQ(polls, stop); // no later page/encode boundary was crossed
            options.cancelled = {};
            ASSERT_TRUE(document.render(options));
        }
        int polls = 0;
        options.cancelled = [&] {
            ++polls;
            return false;
        };
        ASSERT_TRUE(document.render(options));
        EXPECT_EQ(polls, total);
    }
}

TEST(Output, SelectionsNameTheScoreOrOneOfItsExcerpts) {
    music::test::OpenedScore opened = music::test::open("linked_parts.mscx");
    ASSERT_TRUE(opened.document);
    engrave::Document &document = *opened.document;
    const auto outline = document.outline();
    ASSERT_TRUE(outline);
    ASSERT_FALSE(outline->excerpts.empty()) << "the fixture has no linked part";

    const engrave::ScoreSel excerpt{outline->excerpts.front().id};
    // An excerpt is not laid out until something asks for its pages, so the
    // outline that has never done so reports none; page_count lays it out.
    const auto excerpt_pages = document.page_count(excerpt);
    ASSERT_TRUE(excerpt_pages) << excerpt_pages.error().message;
    EXPECT_GE(*excerpt_pages, 1);
    const auto laid_out = document.outline();
    ASSERT_TRUE(laid_out);
    EXPECT_EQ(laid_out->excerpts.front().pages, *excerpt_pages) << "page_count did not lay the excerpt out";

    engrave::RenderOptions options;
    options.format = engrave::RenderFormat::SVG;
    options.score = excerpt;
    const auto rendered = document.render(options);
    ASSERT_TRUE(rendered) << rendered.error().message;
    EXPECT_EQ(rendered->pages.size(), static_cast<std::size_t>(*excerpt_pages));

    const auto master = document.render({});
    ASSERT_TRUE(master);
    if (!master->pages.empty() && !rendered->pages.empty()) {
        EXPECT_NE(rendered->pages.front(), master->pages.front()) << "the excerpt rendered as the full score";
    }

    engrave::ElementId absent;
    absent.hi = 1;
    absent.lo = 2;
    music::test::expect_code(document.render(engrave::RenderOptions{.score = engrave::ScoreSel{absent}}), engrave::ErrorCode::NOT_FOUND,
                             "an id no element has");

    const auto score = document.page_count(engrave::ScoreSel{outline->excerpts.front().id});
    ASSERT_TRUE(score);

    const auto note = document.query(engrave::Query{.types = {"Note"}, .limit = 1});
    ASSERT_TRUE(note);
    ASSERT_FALSE(note->elements.empty());
    music::test::expect_code(document.render(engrave::RenderOptions{.score = engrave::ScoreSel{note->elements.front().id}}),
                             engrave::ErrorCode::INVALID_ARGUMENT, "a selection that names a note");
}

} // namespace

TEST(RuntimeAssets, FontRegistrationRelayoutsExistingScoresWithoutAnEdit) {
    using namespace music::test;
    std::ifstream input(corpus_file("linked_parts.mscx"), std::ios::binary);
    ASSERT_TRUE(input);
    std::string xml((std::istreambuf_iterator<char>(input)), {});
    const auto frame = xml.find("<height>10</height>");
    ASSERT_NE(frame, std::string::npos);
    xml.insert(frame, "<Text><style>title</style><text><font face=\"Petaluma Script\"/>MMMWWWiii012345</text></Text>");
    const auto excerpt_text = xml.find("<text>Flute</text>");
    ASSERT_NE(excerpt_text, std::string::npos);
    xml.replace(excerpt_text, std::string("<text>Flute</text>").size(), "<text><font face=\"Petaluma Script\"/>MMMWWWiii012345</text>");
    const auto *bytes = reinterpret_cast<const std::byte *>(xml.data());
    auto opened = g_engine->open(std::span<const std::byte>(bytes, xml.size()), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto outline = document.outline();
    ASSERT_TRUE(outline);
    ASSERT_FALSE(outline->excerpts.empty());
    const engrave::ScoreSel excerpt{outline->excerpts.front().id};
    ASSERT_TRUE(document.page_count(excerpt));
    const auto text_width = [&](engrave::ScoreSel score) {
        const auto result = document.query({.score = score, .types = {"Text"}, .layout_boxes = true});
        if (!result || result->elements.empty() || !result->elements.front().box) return 0.0;
        return result->elements.front().box->width;
    };
    const double master_before = text_width({});
    const double excerpt_before = text_width(excerpt);
    ASSERT_GT(master_before, 0.0);
    ASSERT_GT(excerpt_before, 0.0);
    engrave::RenderOptions pdf;
    pdf.format = engrave::RenderFormat::PDF;
    const auto before_render = document.render(pdf);
    ASSERT_TRUE(before_render);
    EXPECT_EQ(as_text(before_render->document).find("PetalumaScript"), std::string::npos);
    const auto revision = document.revision();
    const auto missing = g_engine->add_font(g_assets / "missing-font.ttf");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, engrave::ErrorCode::NOT_FOUND);
    const auto invalid = g_engine->add_font(g_assets / "engraving/instruments/instruments.xml");
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, engrave::ErrorCode::INVALID_INPUT);

    const auto source = g_assets / "fonts/petaluma/PetalumaScript.otf";
    const auto copy = std::filesystem::temp_directory_path() / "engrave-runtime-test-petaluma.otf";
    std::filesystem::copy_file(source, copy, std::filesystem::copy_options::overwrite_existing);
    ASSERT_TRUE(g_engine->add_font(copy));
    std::filesystem::remove(copy);
    // The first layout-dependent call refreshes master and excerpt together.
    ASSERT_TRUE(document.page_count());
    EXPECT_NE(text_width({}), master_before);
    EXPECT_NE(text_width(excerpt), excerpt_before);
    EXPECT_EQ(document.revision(), revision);
    const auto after_render = document.render(pdf);
    ASSERT_TRUE(after_render);
    // Cairo embeds the selected face's PostScript name in the PDF. Checking
    // that name tests drawing independently of layout metrics and PDF bytes.
    EXPECT_NE(as_text(after_render->document).find("PetalumaScript"), std::string::npos);
}

TEST(RuntimeAssets, RuntimeFamilyDrawingMatchesLayoutAcrossReplacements) {
    using namespace music::test;
    // Ubuntu is a pinned transitive dependency of Cairo in pixi.lock.
    const auto &fonts = g_native_fonts;
    ASSERT_TRUE(std::filesystem::is_regular_file(fonts / "Ubuntu-M.ttf")) << "pass --native-fonts=<pixi env>/fonts";
    std::ifstream input(corpus_file("linked_parts.mscx"), std::ios::binary);
    ASSERT_TRUE(input);
    std::string xml((std::istreambuf_iterator<char>(input)), {});
    const auto frame = xml.find("<height>10</height>");
    ASSERT_NE(frame, std::string::npos);
    xml.insert(frame, "<Text><style>title</style><text><font face=\"Ubuntu\"/>MMMWWWiii012345</text></Text>");
    auto opened = g_engine->open(std::as_bytes(std::span(xml.data(), xml.size())), {.format = engrave::Format::MSCX});
    ASSERT_TRUE(opened);
    auto &document = **opened;
    const auto text_width = [&] {
        const auto result = document.query({.types = {"Text"}, .layout_boxes = true});
        if (!result || result->elements.empty() || !result->elements.front().box) return 0.0;
        return result->elements.front().box->width;
    };
    const double before_width = text_width();
    ASSERT_GT(before_width, 0.0);
    engrave::RenderOptions pdf;
    pdf.format = engrave::RenderFormat::PDF;
    const auto before = document.render(pdf);
    ASSERT_TRUE(before);
    EXPECT_EQ(as_text(before->document).find("Ubuntu-Medium"), std::string::npos);
    ASSERT_TRUE(g_engine->add_font(fonts / "Ubuntu-M.ttf"));
    const auto registered = g_engine->runtime_fonts();
    const auto medium = std::find_if(registered.begin(), registered.end(), [](const auto &font) { return font.family == "Ubuntu"; });
    ASSERT_NE(medium, registered.end());
    ASSERT_TRUE(medium->bold);
    ASSERT_FALSE(medium->italic);
    EXPECT_NE(text_width(), before_width);
    const auto after = document.render(pdf);
    ASSERT_TRUE(after);
    // Ordinary score text has UNKNOWN type; drawing must use the same
    // registered Medium face as layout's regular-family fallback.
    EXPECT_NE(as_text(after->document).find("Ubuntu-Medium"), std::string::npos);

    // Regular and Light share the same family/bold/italic key but have distinct
    // bytes and PostScript names. A request-only invalidation cannot fix this:
    // the resolved-face cache also has to distinguish their retained identity.
    int regular_id = 0;
    double regular_width = 0;
    for (const bool light : {false, true, false}) {
        SCOPED_TRACE(light ? "Ubuntu Light" : "Ubuntu Regular");
        ASSERT_TRUE(g_engine->add_font(fonts / (light ? "Ubuntu-L.ttf" : "Ubuntu-R.ttf")));
        const auto snapshot = g_engine->runtime_fonts();
        const auto selected = std::find_if(snapshot.begin(), snapshot.end(),
                                           [](const auto &font) { return font.family == "Ubuntu" && !font.bold && !font.italic; });
        ASSERT_NE(selected, snapshot.end());
        const double width = text_width();
        if (light) {
            EXPECT_NE(selected->id, regular_id);
            EXPECT_NE(width, regular_width);
        } else if (regular_id == 0) {
            regular_id = selected->id;
            regular_width = width;
        } else {
            EXPECT_EQ(selected->id, regular_id);
            EXPECT_EQ(width, regular_width);
        }
        const auto rendered = document.render(pdf);
        ASSERT_TRUE(rendered);
        const auto text = as_text(rendered->document);
        EXPECT_NE(text.find("Ubuntu"), std::string::npos);
        EXPECT_EQ(text.find("Ubuntu-Medium"), std::string::npos);
        if (light) {
            EXPECT_NE(text.find("Ubuntu-Light"), std::string::npos);
        } else {
            EXPECT_EQ(text.find("Ubuntu-Light"), std::string::npos);
        }
    }
}

TEST(RuntimeAssets, FontMetadataTracksRetainedCopiesAndReplacements) {
    const auto source = music::test::g_assets / "fonts/edwin/Edwin-Roman.otf";
#ifdef _WIN32
    const int process = _getpid();
#else
    const int process = getpid();
#endif
    const auto copy = std::filesystem::temp_directory_path() / ("engrave-font-metadata-" + std::to_string(process) + ".otf");

    struct Cleanup {
        std::filesystem::path path;

        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{copy};

    ASSERT_TRUE(std::filesystem::copy_file(source, copy));
    const auto read = [](const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const auto original_bytes = read(source);
    ASSERT_FALSE(original_bytes.empty());
    ASSERT_TRUE(g_engine->add_font(copy));
    const auto first_snapshot = g_engine->runtime_fonts();
    const auto first =
        std::find_if(first_snapshot.begin(), first_snapshot.end(), [&](const auto &font) { return read(font.path) == original_bytes; });
    ASSERT_NE(first, first_snapshot.end());
    const auto original = *first;
    EXPECT_GT(original.id, 0);
    EXPECT_FALSE(original.family.empty());
    EXPECT_TRUE(original.path.is_absolute());
    EXPECT_NE(original.path, copy);
    EXPECT_FALSE(original.content_fingerprint.empty());

    // An appended byte leaves this font usable but creates a distinct copy of
    // the same family/style, exercising replacement without another fixture.
    {
        std::ofstream file(copy, std::ios::binary | std::ios::app);
        file.put('\0');
        ASSERT_TRUE(file);
    }
    ASSERT_TRUE(g_engine->add_font(copy));
    const auto replaced = g_engine->runtime_fonts();
    ASSERT_EQ(replaced.size(), first_snapshot.size());
    const auto entry = std::find_if(replaced.begin(), replaced.end(), [&](const auto &font) {
        return font.family == original.family && font.bold == original.bold && font.italic == original.italic;
    });
    ASSERT_NE(entry, replaced.end());
    EXPECT_NE(entry->id, original.id);
    EXPECT_NE(entry->path, original.path);
    EXPECT_NE(entry->content_fingerprint, original.content_fingerprint);
    EXPECT_EQ(read(original.path), original_bytes);
    EXPECT_EQ(read(entry->path), read(copy));

    ASSERT_TRUE(g_engine->add_font(source));
    const auto restored = g_engine->runtime_fonts();
    const auto again = std::find_if(restored.begin(), restored.end(), [&](const auto &font) { return font.id == original.id; });
    ASSERT_NE(again, restored.end());
    EXPECT_EQ(again->path, original.path);
    {
        std::ofstream file(copy, std::ios::binary | std::ios::trunc);
        file << "invalid-font";
    }
    const auto invalid = g_engine->add_font(copy);
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.error().code, engrave::ErrorCode::INVALID_INPUT);
    const auto unchanged = g_engine->runtime_fonts();
    EXPECT_EQ(unchanged.size(), restored.size());
    EXPECT_NE(std::find_if(unchanged.begin(), unchanged.end(), [&](const auto &font) { return font.id == original.id; }), unchanged.end());
}

TEST(RuntimeAssets, InitialEngineFontIsListedInMetadata) {
    const auto font = music::test::g_assets / "fonts/edwin/Edwin-Roman.otf";
    const auto score = music::test::corpus_file("ghost_note.mscx");
    ASSERT_FALSE(score.empty());
    const std::vector<std::string> arguments = {
        "--assets=" + music::test::g_assets.string(),
        "--render=" + score.string(),
        "--out=" + std::filesystem::temp_directory_path().string(),
        "--test-startup-font=" + font.string(),
    };
    EXPECT_EQ(music::test::run_probe(arguments), 0);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    std::filesystem::path assets;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument.starts_with("--assets=")) {
            assets = std::filesystem::path(argument.substr(9));
        } else if (argument.starts_with("--corpus=")) {
            collect(std::filesystem::path(argument.substr(9)));
        } else if (argument.starts_with("--soundfont=")) {
            music::test::g_soundfonts.push_back(std::filesystem::path(argument.substr(12)));
        } else if (argument.starts_with("--native-fonts=")) {
            music::test::g_native_fonts = std::filesystem::path(argument.substr(15));
        }
    }
    if (music::test::g_soundfonts.empty()) {
        if (const char *env = std::getenv("MUSIC_MCP_SOUNDFONT")) music::test::g_soundfonts.push_back(std::filesystem::u8path(env));
    }
    music::test::g_self = std::filesystem::absolute(argv[0]);
    music::test::g_assets = assets;
    if (music::test::g_corpus.empty()) {
        for (const auto &candidate : {std::filesystem::path("fixtures/golden"), std::filesystem::path("../fixtures/golden"),
                                      std::filesystem::path("../../fixtures/golden")}) {
            if (std::filesystem::is_directory(candidate)) {
                collect(candidate);
                break;
            }
        }
    }
    engrave::EngineOptions options;
    options.resources = assets;
    options.soundfonts = music::test::g_soundfonts;
    auto engine = engrave::Engine::create(options);
    if (!engine) {
        std::fprintf(stderr, "Engine::create: %s (pass --assets=<resources>)\n", engine.error().message.c_str());
        return 1;
    }
    music::test::g_engine = std::move(*engine);
    int result = RUN_ALL_TESTS();
    music::test::g_engine.reset();
    for (const auto &path : music::test::g_retained_for_cleanup) {
        if (std::filesystem::exists(path)) {
            std::fprintf(stderr, "retained SoundFont survived Engine teardown: %s\n", path.string().c_str());
            result = 1;
        }
    }
    return result;
}

TEST(Pen, ReadingDashPatternDoesNotChangeValueEquality) {
    muse::draw::Pen pen(muse::draw::PenStyle::DASH_DOT_LINE);
    const auto copy = pen;
    EXPECT_EQ(pen.dash_pattern(), (std::vector<double>{4, 2, 1, 2}));
    EXPECT_EQ(pen, copy);
    pen.set_dash_pattern({3, 2, 1});
    EXPECT_EQ(pen.dash_pattern(), (std::vector<double>{3, 2, 1, 1}));
    pen.set_style(muse::draw::PenStyle::DOT_LINE);
    EXPECT_EQ(pen.dash_pattern(), (std::vector<double>{1, 2}));
}

TEST(Pixmap, OwnsInputAndCopiesIndependentBytes) {
    muse::ByteArray bytes("png-bytes");
    muse::draw::Pixmap pixmap(bytes, muse::Size(2, 3));
    const auto copy = pixmap;
    bytes[0] = 'x';
    EXPECT_EQ(pixmap.data(), muse::ByteArray("png-bytes"));
    EXPECT_EQ(pixmap, copy);
    EXPECT_NE(pixmap.data().const_data(), copy.data().const_data());
    muse::draw::Pixmap moved(std::move(bytes), muse::Size(2, 3));
    EXPECT_EQ(moved.data(), muse::ByteArray("xng-bytes"));
}
