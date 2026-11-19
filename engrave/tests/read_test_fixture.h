#pragma once

// The read API: outline(), query() and get(), against the score code, which walks
// the same score independently.
//
//   engrave_read_test --assets=<resources> [--corpus=<file or dir>]...
//
// The corpus test compares a query with the semantic dump (engrave/src/score/
// score_semantic.cpp).  The comparison is exact and covers the element kinds both
// walks report: for every score of a document, master and excerpts,
//   - each chord, rest and measure repeat of every staff and voice, with its tick and
//     length in ticks;
//   - each note of those chords, with its pitch;
//   - each time signature, key signature and clef, with its tick, staff and value.
// A query over the same types must give one row per dump row with the same fields and
// no row of its own.  What the dump reports and a query does not is out of scope:
// annotations, spanners, frame elements, and everything layout builds.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "draw/svgrenderer.h"
#include "engrave/src/api/handles.h"
#include "engrave/src/api/pid_support.h"
#include "engrave/src/api/state.h"
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/score/json.h"
#include "engrave/src/score/score_metadata.h"
#include "engrave/src/score/score_positions.h"
#include "engrave/src/score/score_semantic.h"
#include "engrave/src/score/write470.h"
#include "engraving/automation/automationdata.h"
#include "engraving/automation/tempovalues.h"
#include "engraving/dom/accidental.h"
#include "engraving/dom/anchors.h"
#include "engraving/dom/bagpembell.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/capo.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordbracket.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/groups.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/hook.h"
#include "engraving/dom/image.h"
#include "engraving/dom/imageStore.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/ledgerline.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurenumber.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/mmrest.h"
#include "engraving/dom/mmrestrange.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/notedot.h"
#include "engraving/dom/noteline.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/page.h"
#include "engraving/dom/parenthesis.h"
#include "engraving/dom/part.h"
#include "engraving/dom/property.h"
#include "engraving/dom/rangelock.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spannermap.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stem.h"
#include "engraving/dom/stemslash.h"
#include "engraving/dom/system.h"
#include "engraving/dom/tapping.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tiejumppointlist.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/trill.h"
#include "engraving/dom/tuplet.h"
#include "engraving/editing/addremoveelement.h"
#include "engraving/editing/editpart.h"
#include "engraving/editing/reset.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/infrastructure/eidregister.h"
#include "engraving/normalization/normalize.h"
#include "engraving/normalization/notes.h"
#include "engraving/playback/flatplaybackscore.h"
#include "engraving/playback/metaparsers/notearticulationsparser.h"
#include "engraving/rendering/score/chordlayout.h"
#include "engraving/rendering/score/horizontalspacing.h"
#include "engraving/rendering/score/layoutcontext.h"
#include "engraving/rendering/score/layoutdatastore.h"
#include "engraving/rendering/score/measurelayout.h"
#include "engraving/rendering/score/modifydom.h"
#include "engraving/rendering/score/restlayout.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "engraving/rendering/score/systemlayout.h"
#include "engraving/rendering/score/tlayout.h"
#include "engraving/rendering/score/tupletlayout.h"
#include "engraving/rw/inoutdata.h"
#include "engraving/rw/rwregister.h"
#include "engraving/rw/write/twrite.h"
#include "engraving/rw/write/writecontext.h"
#include "engraving/rw/xmlwriter.h"
#include "engraving/types/constants.h"
#include "global/io/buffer.h"
#include "importexport/musicxml/internal/musicxmlconfiguration.h"
#include "serialization/xmlstreamreader.h"
#include "serialization/zipreader.h"
#include <engrave/engrave.h>
#include <engrave/testing/write470.h>

namespace read_test {

using engrave::ElementId;

using engrave::ErrorCode;

using engrave::PropertyRead;

using mu::engraving::MasterScore;

// A whole note in the engine's ticks, which is what a Fraction in the API counts.
constexpr std::int64_t whole_note_ticks = 4 * mu::engraving::Constants::division;
extern std::filesystem::path g_assets;
extern std::vector<std::filesystem::path> g_corpus;
extern engrave::Engine *g_engine;

std::string read_text(const std::filesystem::path &path);
std::vector<std::byte> as_bytes(std::string_view text);
std::string_view as_text(const std::vector<std::byte> &bytes);
std::string normalize_lf(std::string_view text);
std::unique_ptr<engrave::Document> open_path(const std::filesystem::path &path);
MasterScore &master_of(engrave::Document &document);
std::int64_t ticks_of(const engrave::Fraction &value);
std::filesystem::path write470_fixture();

struct ItemLayoutSpy {
    static inline std::size_t count = 0;

    ItemLayoutSpy() {
        count = 0;
        mu::engraving::rendering::score::ScoreRenderer::layout_hook = [](const auto *) { ++count; };
    }

    ~ItemLayoutSpy() { mu::engraving::rendering::score::ScoreRenderer::layout_hook = nullptr; }
};

std::string formatted_text_fixture();
mu::engraving::TextBase *staff_text_of(MasterScore &master);
std::string mmr_source(bool prefix_note = false);
// Inspect ownership rather than scan_elements(), which omits layout-only groups.
size_t grace_group_count(const mu::engraving::EngravingObject *object);
mu::engraving::Chord *add_test_grace(mu::engraving::Chord *chord, mu::engraving::NoteType type);

/// Checks the error a call must fail with, without reading .error() of a success.
template <typename T>
void expect_code(const engrave::Result<T> &result, ErrorCode expected, std::string_view what) {
    if (result) {
        ADD_FAILURE() << what << ": the call succeeded";
        return;
    }
    EXPECT_EQ(result.error().code, expected) << what << ": " << result.error().message;
}

/// One transaction on the score, so that an engine command may run (the batch API is
/// S6's; a read test drives the primitive the same way the handle tests do).
struct Command {
    explicit Command(MasterScore &score) : score_(score) {
        mu::engraving::MScore::set_error(mu::engraving::MsError::MS_NO_ERROR);
        score_.start_cmd(muse::TranslatableString::untranslatable("engrave read test"));
    }

    ~Command() {
        if (open_) score_.end_cmd(false);
    }

    Command(const Command &) = delete;
    Command &operator=(const Command &) = delete;

    void commit() {
        score_.end_cmd(false);
        open_ = false;
        engrave::assign_missing_ids(score_);
    }

private:
    MasterScore &score_;
    bool open_ = true;
};

// One element of the comparison, as a line so that a difference prints readably.
using Row = std::string;
const char *type_name(const music::Json &kind);
Row event_row(std::string_view type, int tick, std::size_t staff, std::size_t voice, int ticks);
Row note_row(int tick, std::size_t staff, std::size_t voice, int pitch);
std::string value_text(const music::Json &value);
// The rows the semantic dump reports for one score body.
void oracle_rows(const music::Json &body, std::multiset<Row> &rows);
const PropertyRead *property(const engrave::ElementInfo &element, std::string_view name);
std::int64_t int_property(const engrave::ElementInfo &element, std::string_view name);
// The rows a query reports, in the same form as oracle_rows.
void query_rows(const std::vector<engrave::ElementInfo> &elements, std::multiset<Row> &rows);
/// Compares two row sets, reporting the rows whose counts differ rather than the whole
/// set.
void expect_same_rows(const std::multiset<Row> &expected, const std::multiset<Row> &actual, std::string_view what);

constexpr std::string_view compared_types[] = {"Chord", "Rest", "MeasureRepeat", "Note", "Clef", "KeySig", "TimeSig"};

constexpr std::string_view compared_props[] = {"pitch", "timesig", "concertKey"};
std::vector<std::string> names(std::span<const std::string_view> values);
/// Every element of the comparison's types in one score, asked for measure window by
/// measure window so that a large score stays under the result cap, and page by page
/// so that paging is part of every corpus run.
std::vector<engrave::ElementInfo> read_all(engrave::Document &document, const engrave::ScoreSel &score, std::size_t measures);

std::filesystem::path corpus_file(std::initializer_list<std::string_view> extensions);
/// The first corpus score that has an element of a type.
std::unique_ptr<engrave::Document> open_with(std::string_view type);

std::uint64_t layout_count(const mu::engraving::Score &score);

constexpr std::string_view repeat_edit_xml = R"(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time></attributes>
<barline location="left"><repeat direction="forward"/></barline>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note></measure>
<measure number="2"><note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
<barline location="right"><repeat direction="backward"/></barline></measure>
<measure number="3"><direction><sound tempo="60"/></direction>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note></measure>
</part></score-partwise>)";
std::string dynamics_musicxml(std::string_view direction, std::string_view notations, std::string_view note_attributes = "");
std::vector<mu::engraving::Dynamic *> imported_dynamics(engrave::Document &document);
// Parse the exported document before reopening it: successful import alone does
// not establish that literal text was serialized as well-formed XML.
std::unique_ptr<engrave::Document> dynamics_native_roundtrip(engrave::Document &document, std::string_view expected_text = "",
                                                             engrave::Format format = engrave::Format::MSCX);

bool looks_like_score(const std::filesystem::path &path);
void collect(const std::filesystem::path &path);
std::string chordline_offset_fixture();
std::vector<mu::engraving::ChordLine *> offset_test_lines(mu::engraving::Score &score);
std::string offset_test_write(const mu::engraving::ChordLine *line);
std::string legacy_override_write(const mu::engraving::EngravingItem *item, bool standalone = false);
std::string legacy_segment_fixture(std::string_view spanners);
std::string split_spanner_fixture(std::string_view overrides = "");
void break_first_measure(MasterScore &master);
std::string cross_staff_grace_beam_fixture(bool after, int anchor_index);
std::string s18_break_fixture(const std::string &breaks);
std::string s18_break_xml(const std::string &type);
void expect_s18_breaks(mu::engraving::Measure *measure, const std::vector<mu::engraving::LayoutBreakType> &expected);
void render_s20_scores(engrave::Document &document);

template <typename Check>
void check_s20_roundtrips(engrave::Document &document, const Check &check) {
    ASSERT_NO_FATAL_FAILURE(check(document));
    ASSERT_NO_FATAL_FAILURE(render_s20_scores(document));
    ASSERT_NO_FATAL_FAILURE(check(document));
    ASSERT_TRUE(document.checkpoint("section propagation"));
    ASSERT_TRUE(document.restore_checkpoint("section propagation"));
    ASSERT_NO_FATAL_FAILURE(check(document));
    ASSERT_NO_FATAL_FAILURE(render_s20_scores(document));
    ASSERT_NO_FATAL_FAILURE(check(document));
    for (const auto format : {engrave::Format::MSCX, engrave::Format::MSCZ}) {
        SCOPED_TRACE(static_cast<int>(format));
        const auto saved = document.export_score({.format = format});
        ASSERT_TRUE(saved);
        auto reopened = g_engine->open(*saved, {.format = format});
        ASSERT_TRUE(reopened) << reopened.error().message;
        ASSERT_NO_FATAL_FAILURE(check(**reopened));
        ASSERT_NO_FATAL_FAILURE(render_s20_scores(**reopened));
        ASSERT_NO_FATAL_FAILURE(check(**reopened));
    }
}

std::string s17_beam_fixture(const std::string &voice);
// Compare the output across an explicit ownership boundary, not only a reflow
// which can reuse surviving payloads. Source shells remain transitional L3-L6.
void expect_discard_rebuild_parity(engrave::Document &document, mu::engraving::Score &score, const engrave::ScoreSel &selection = {},
                                   bool require_chord_overrides = false);
std::string parenthesis_link_fixture();
std::string s11_triplet_fixture(bool stem_offset = false);

} // namespace read_test
