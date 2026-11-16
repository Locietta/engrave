#pragma once

// Element handles: the inventory (H1), the ids a score has after open and create
// (H2), what survives a save and a replay (H3, H4), linked parts (H5), deletion and
// replacement (H7, H8), hostile ids in the input (H9, H10), stale handles (H11),
// addresses (H12) and determinism (H13).  Every test ends with INV: the register is
// a bijection onto the elements that are part of the score, and every element that
// should have an id has one.
//
//   engrave_handles_test --assets=<resources> [--corpus=<file or dir>]... [options]
//   --deterministic       create the engine with sequential ids (H13's child)
//   --update-inventory    rewrite handle_inventory.json from the corpus
//
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "engrave/src/api/batch.h"
#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engrave/src/runtime/score_io.h"
#include "engrave/src/score/export_options.h"
#include "engrave/src/score/json.h"
#include "engrave/src/score/score_excerpts.h"
#include "engraving/dom/accidental.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordbracket.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/image.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/page.h"
#include "engraving/dom/part.h"
#include "engraving/dom/partialtie.h"
#include "engraving/dom/playcounttext.h"
#include "engraving/dom/property.h"
#include "engraving/dom/rangelock.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/sharedpart.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spacer.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/system.h"
#include "engraving/dom/systemdivider.h"
#include "engraving/dom/tapping.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tiejumppointlist.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/editing/addremoveelement.h"
#include "engraving/editing/editkeysig.h"
#include "engraving/editing/editmeasures.h"
#include "engraving/editing/editpagelocks.h"
#include "engraving/editing/edittremolo.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/infrastructure/eidregister.h"
#include "engraving/rendering/score/layoutdatastore.h"
#include "engraving/rw/inoutdata.h"
#include "engraving/rw/rwregister.h"
#include "engraving/rw/write/twrite.h"
#include "engraving/rw/write/writecontext.h"
#include "engraving/rw/xmlwriter.h"
#include "global/io/buffer.h"
#include "global/serialization/zipreader.h"
#include <engrave/engrave.h>

namespace handles_test {

using engrave::ElementId;

using engrave::ErrorCode;

using engrave::IdClass;

using mu::engraving::ElementType;

using mu::engraving::EngravingItem;

using mu::engraving::EngravingObject;

using mu::engraving::MasterScore;

using mu::engraving::Measure;

using mu::engraving::MeasureBase;

using mu::engraving::Note;

using mu::engraving::Part;

using mu::engraving::Score;

using mu::engraving::Segment;

using mu::engraving::SegmentType;

using mu::engraving::Staff;
extern std::filesystem::path g_assets;
extern std::filesystem::path g_inventory;
extern std::filesystem::path g_source_root;
extern std::vector<std::filesystem::path> g_corpus;
extern std::vector<std::string> g_engine_args;
extern std::string g_program;
extern bool g_deterministic;
extern bool g_update_inventory;
extern std::unique_ptr<engrave::Engine> g_engine;

std::string read_text(const std::filesystem::path &path);
std::vector<std::byte> as_bytes(std::string_view text);
std::string text_of(const std::vector<std::byte> &bytes);

const char *class_name(IdClass value);
MasterScore &master_of(engrave::Document &document);
/// Whether an id still names an element that is part of the score.
bool live(MasterScore &master, const ElementId &id);
ElementId engine_id(const EngravingObject &object);
std::string id_text(const EngravingObject &object);
std::string object_name(const EngravingObject &object);
/// Where an element is, for the inventory's evidence.
std::string detail(const EngravingObject &object);

Note *first_note(Score &score);
Segment *first_segment(Score &score, SegmentType kind);
mu::engraving::ChordRest *first_chord_rest(Score &score);
/// The first element of a type that the walk hands an id out for.
EngravingObject *first_live(MasterScore &master, ElementType type);

/// One transaction on the score, so that the engine's undo commands may run.
struct Command {
    explicit Command(MasterScore &score) : score_(score) {
        mu::engraving::MScore::set_error(mu::engraving::MsError::MS_NO_ERROR);
        score_.start_cmd(muse::TranslatableString::untranslatable("engrave handles test"));
    }

    ~Command() {
        if (open_) score_.end_cmd(true);
    }

    Command(const Command &) = delete;
    Command &operator=(const Command &) = delete;

    // Publish identities after source finalization.
    void commit() {
        score_.end_cmd(false);
        open_ = false;
        engrave::assign_missing_ids(score_);
    }

private:
    MasterScore &score_;
    bool open_ = true;
};

template <typename Function>
void commit(MasterScore &master, const Function &function) {
    Command command(master);
    function(master);
    command.commit();
}

std::unique_ptr<engrave::Document> open_path(const std::filesystem::path &path);
std::unique_ptr<engrave::Document> open_text(std::string_view text, engrave::OpenOptions options = {});
std::string export_mscz(engrave::Document &document);
std::map<std::string, std::string> unzip_entries(std::string_view zip_bytes);
/// The engine's own MSCX of the score (or of one of its excerpts). The version it
/// carries is the current one, so the reader that repairs ids is the one under test.
std::string export_mscx(engrave::Document &document, int excerpt_id = -1);
/// The snapshot path: write the score and read it back into a new document.
std::unique_ptr<engrave::Document> reopen(engrave::Document &document);

/// A corpus file with one of these extensions (any, when none is given).
std::filesystem::path corpus_file(std::initializer_list<std::string_view> extensions);
/// Whether the corpus file is one the engine writes itself, rather than an import that
/// brings its own layout quirks.
bool written_by_engine(const std::filesystem::path &path);
// Translate the historical composition input, including its part/instrument metadata.
std::string composition_input();
std::vector<Score *> native_scores(MasterScore &master);
void expect_no_layout(MasterScore &master);
std::map<std::string, std::string> native_captures(engrave::Document &document, bool legacy = false);
std::string source_native(Score &score);

/// What an element is beyond its id: its type, where it sits, and a payload that a
/// save and reload has to preserve.
std::string payload(const EngravingObject &object);
std::string describe(const engrave::LiveId &entry);
/// Every id of the document and what it names, keyed by the id's text so that a
/// failure prints ids rather than bytes.
std::map<std::string, std::string> element_map(engrave::Document &document);
/// Compares two element maps, reporting the ids that differ rather than the whole map.
void expect_same_ids(const std::map<std::string, std::string> &before, const std::map<std::string, std::string> &after,
                     std::string_view what);
/// The first corpus score that has an element of this type with an id.
std::unique_ptr<engrave::Document> open_with(ElementType type);

void expect_inv(engrave::Document &document);

struct Evidence {
    IdClass declared = IdClass::PERSISTED;
    int assigned = 0;   // instances the walk gives an id
    int kept = 0;       // of those, how many kept that id through a reload
    int others = 0;     // other instances (not generated)
    int other_kept = 0; // of those, how many kept the id the probe gave them
    // The class of the strongest instance seen: a multi-measure rest and everything it
    // holds is layout's, but the measure and barline types are not, so one instance's
    // class does not decide the type's.
    int strength = -1;
};

int class_strength(IdClass value);
/// Feeds one open document to the inventory: what class each type declares, and whether
/// the ids the walk hands out survive the score's own write and read.
void inventory_evidence(engrave::Document &document, const std::string &source, std::map<std::string, Evidence> &evidence);
/// A score with one instance of every type an operation can create, made through the
/// public edit API on a template. The corpus cannot show a type it has no instance of,
/// so the Add pair table and the other creating operations are measured here too.
std::unique_ptr<engrave::Document> created_types_document(std::set<std::string> &types);

void expect_first_ids(engrave::Document &document);

/// A current-version MSCX of an imported corpus score, plus one of its excerpts:
/// the files the 5.00 reader reads, which is the reader that repairs ids.
struct CraftedInput {
    std::string master;
    std::string excerpt;
};

CraftedInput crafted_input();
/// The spans of the text inside every <eid> element.
std::vector<std::pair<std::size_t, std::size_t>> eid_spans(const std::string &text);
/// The text with the n-th id replaced.
std::string with_eid(std::string text, std::size_t index, std::string_view value);
/// The text with every id `from` renamed to `to`, which gives two elements one id.
std::string with_copied_id(std::string text, std::string_view from, std::string_view to);
/// A corpus MSCX file that has ids of its own and, if asked, a tag.
std::filesystem::path corpus_mscx(std::string_view tag);
/// The id texts of a corpus file, with the type of the element each names.
std::map<std::string, std::string> id_types(const std::string &text);
bool warned_about(engrave::Document &document, std::string_view text);

/// Whether the crafted text gives the reader a duplicate to repair.
bool reader_sees_duplicate(const std::string &text);
void expect_repaired(const std::string &text);

bool looks_like_score(const std::filesystem::path &path);
void collect(const std::filesystem::path &path);

} // namespace handles_test
