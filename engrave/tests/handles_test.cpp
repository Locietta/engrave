#include "engraving/dom/dummyparent.h"
#include "engraving/dom/guitarbend.h"
#include "handles_test_fixture.h"

using namespace handles_test;

TEST(Handles, PageQueriesFollowRangeLayoutAndOutputDiscard) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    ASSERT_EQ(note->pitch(), 71);
    master.ensure_layout();
    const muse::RectF area(0, 0, 10000, 10000);
    ASSERT_FALSE(master.pages().empty());
    EXPECT_TRUE(std::ranges::contains(master.pages().front()->items(area), note));
    const auto ranges = master.layout_state.range_count;
    master.do_layout_range(mu::engraving::Fraction(0, 1), mu::engraving::Fraction(1, 4));
    EXPECT_EQ(master.layout_state.range_count, ranges + 1);
    EXPECT_TRUE(std::ranges::contains(master.pages().front()->items(area), note));
    master.layout_data_store()->clear();
    const auto layouts = master.layout_state.whole_count + master.layout_state.range_count;
    EXPECT_TRUE(master.pages().empty());
    EXPECT_TRUE(master.systems().empty());
    EXPECT_EQ(master.first_measure()->system(), nullptr);
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, layouts);
    master.ensure_layout();
    EXPECT_TRUE(std::ranges::contains(master.pages().front()->items(area), note));
}

TEST(Handles, H1Inventory) {
    ASSERT_FALSE(g_corpus.empty());
    std::map<std::string, Evidence> evidence;

    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        inventory_evidence(*document, path.filename().string(), evidence);
    }

    // Not only what the corpus shows: a type an operation can create is measured too,
    // so adding an Add pair or a creating operation fails here until the inventory says
    // what class the new type has.
    std::set<std::string> created;
    if (auto document = created_types_document(created)) {
        SCOPED_TRACE("types created through the edit API");
        inventory_evidence(*document, "created", evidence);
    }
    for (const char *type :
         {"Articulation", "Chord", "Clef", "Dynamic", "Fermata",   "HairPin",    "LayoutBreak", "Lyrics", "Measure", "Note", "Part",
          "Rest",         "Score", "Slur", "Staff",   "StaffText", "SystemText", "Tempo",       "Tie",    "Tuplet",  "VBox"}) {
        EXPECT_TRUE(created.contains(type)) << "the edit API did not create a " << type << "; does it still create one?";
    }
    ASSERT_FALSE(evidence.empty()) << "no corpus score opens";

    std::map<std::string, std::string> observed;
    for (const auto &[type, row] : evidence) {
        if (type == "Segment") {
            observed[type] = class_name(IdClass::ADDRESS);
        } else if (row.assigned > 0) {
            // The walk hands this type out, so every id it gave must survive a save.
            if (row.kept != row.assigned) {
                ADD_FAILURE() << type << ": only " << row.kept << " of " << row.assigned << " ids survived a save";
                continue;
            }
            observed[type] = class_name(IdClass::PERSISTED);
        } else {
            // The walk hands nothing out for this type, so only its declared class is left
            // to pin: an element layout builds, one an address reaches, or one whose id
            // only the file carries.
            observed[type] = class_name(row.declared);
        }
        EXPECT_EQ(class_name(row.declared), observed[type])
            << type << " is declared " << class_name(row.declared) << ", the corpus shows " << observed[type];
    }

    if (g_update_inventory) {
        music::SortedJson types = music::make_object<music::SortedJson>();
        // A row this run did not see is kept: the corpus a regeneration is run with is no
        // reason to forget what the walk was measured to do with a type.
        if (std::filesystem::is_regular_file(g_inventory)) {
            const auto old = music::parse_json<music::SortedJson>(read_text(g_inventory));
            if (old && old->contains("types")) {
                int kept = 0;
                for (const auto &[type, value] : old->at("types").get_object()) {
                    types[type] = value;
                    if (!observed.contains(type)) ++kept;
                }
                if (kept) std::printf("kept %d row(s) this corpus did not show\n", kept);
            }
        }
        for (const auto &[type, value] : observed) types[type] = value;
        music::SortedJson document = {
            {"note", "The id class of every element type the corpus shows or an operation can create. persisted: the "
                     "writer stores its id and a reload restores it; layout: created by layout, so the next pass hands "
                     "out a new id; address: reached through an Address instead; session: alive in memory but not "
                     "written. This test fails when a type is missing here or its class changed."},
            {"types", types},
        };
        std::ofstream out(g_inventory, std::ios::binary);
        out << music::dump_json(document, 2) << '\n';
        std::printf("wrote %zu types to %s\n", observed.size(), g_inventory.string().c_str());
        return;
    }

    ASSERT_TRUE(std::filesystem::is_regular_file(g_inventory)) << "no " << g_inventory.string() << "; run --update-inventory";
    const auto inventory = music::parse_json<music::SortedJson>(read_text(g_inventory));
    ASSERT_TRUE(inventory) << g_inventory.string() << " is not JSON";
    const auto &declared = inventory->at("types");
    for (const auto &[type, value] : observed) {
        ASSERT_TRUE(declared.contains(type)) << type << " is unclassified (" << value << "); add it to " << g_inventory.string();
        EXPECT_EQ(declared.at(type).get<std::string>(), value) << "the class of " << type << " changed";
    }
    for (const auto &[type, value] : declared.get_object()) {
        if (!observed.contains(type)) std::printf("note: %s is no longer in the corpus\n", type.c_str());
    }
}

TEST(Handles, H2IdsBeforeTheFirstSave) {
    ASSERT_FALSE(g_corpus.empty());
    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        expect_first_ids(*document);
    }

    const auto templates = g_engine->templates();
    ASSERT_GE(templates.size(), 40u);
    for (const auto &info : templates) {
        auto document = g_engine->create_document({.template_path = info.path});
        ASSERT_TRUE(document) << info.id << ": " << document.error().message;
        SCOPED_TRACE("template " + info.id);
        expect_first_ids(**document);
    }
}

// --- H3 -------------------------------------------------------------------

TEST(Handles, H3IdsSurviveSaveAndRestore) {
    ASSERT_FALSE(g_corpus.empty());
    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        const auto before = element_map(*document);
        EXPECT_FALSE(before.empty());

        auto reloaded = reopen(*document);
        if (!reloaded) continue;
        expect_same_ids(before, element_map(*reloaded), "a save and reload");

        // A snapshot restore is the same write and read again.
        auto restored = reopen(*reloaded);
        if (!restored) continue;
        expect_same_ids(before, element_map(*restored), "the second round trip");
        expect_inv(*reloaded);
        expect_inv(*restored);
    }
}

// --- H4 -------------------------------------------------------------------

TEST(Handles, H4ExportLeavesIdsAndRevisionAlone) {
    ASSERT_FALSE(g_corpus.empty());
    for (const auto &path : g_corpus) {
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        const auto before = element_map(*document);
        const std::string first = export_mscz(*document);
        const std::string second = export_mscz(*document);
        EXPECT_FALSE(first.empty());
        EXPECT_EQ(unzip_entries(first), unzip_entries(second)) << "two exports of one score differ";
        EXPECT_EQ(element_map(*document), before) << "an export changed an id";
        EXPECT_EQ(document->revision(), 1u);
        expect_inv(*document);
    }
}

// --- H5 -------------------------------------------------------------------

TEST(Handles, H5aLinksAcrossExcerpts) {
    // Excerpt generation links a part's elements as clones in the excerpt score.  It needs
    // notes to link, which a template-built score has none of, so the corpus is the source.
    int checked = 0;
    for (const auto &path : g_corpus) {
        if (path.extension() != ".mscx") continue;
        auto document = open_path(path);
        if (!document) continue;
        SCOPED_TRACE(path.string());
        MasterScore &master = master_of(*document);
        if (master.excerpts().empty()) music::generate_score_excerpts(&master);
        if (master.excerpts().empty()) continue;
        engrave::assign_missing_ids(master);

        // A note of the master and its clone in an excerpt: any note whose link group holds
        // a note of another score.
        Note *master_note = nullptr;
        Note *excerpt_note = nullptr;
        for (const engrave::LiveId &entry : engrave::live_ids(master)) {
            if (master_note || entry.object->type() != ElementType::NOTE) continue;
            if (entry.object->score() != &master) continue; // the master's own note, not a clone
            for (EngravingObject *linked : entry.object->link_list()) {
                if (!linked->is_note() || linked->score() == &master) continue;
                master_note = mu::engraving::to_note(entry.object);
                excerpt_note = mu::engraving::to_note(linked);
                break;
            }
        }
        if (!master_note || !excerpt_note) continue;
        ++checked;

        const ElementId master_id = engine_id(*master_note);
        const ElementId excerpt_id = engine_id(*excerpt_note);
        EXPECT_NE(master_note->eid(), excerpt_note->eid()) << "the excerpt clone shares the master's id";
        EXPECT_TRUE(live(master, master_id));
        EXPECT_TRUE(live(master, excerpt_id));

        // A property set on the master propagates to the excerpt clone.
        const muse::draw::Color red(255, 0, 0);
        commit(master, [&](MasterScore &) {
            master_note->undo_change_property(mu::engraving::Pid::COLOR, red, mu::engraving::PropertyFlags::NOSTYLE);
        });
        EXPECT_EQ(excerpt_note->color(), red) << "the property did not reach the linked element";
        EXPECT_TRUE(live(master, master_id)) << "a property edit lost the element's id";

        ASSERT_TRUE(document->edit({{engrave::Remove{master_id}}}));
        EXPECT_FALSE(live(master, master_id)) << "the removed element still resolves";
        EXPECT_FALSE(live(master, excerpt_id)) << "the linked clone survived the removal";
        expect_inv(*document);
        break; // one linked score is what this test needs; the first with notes is enough
    }
    EXPECT_GT(checked, 0) << "no corpus score has a linked note";
}

TEST(Handles, H5bRemovingAPartLeavesNoId) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    MasterScore &master = master_of(*document);
    // A save gives the parts and their staves their ids.
    EXPECT_FALSE(export_mscz(*document).empty());
    expect_inv(*document);

    std::set<std::string> seen;
    for (const engrave::LiveId &entry : engrave::live_ids(master)) seen.insert(engrave::to_string(entry.id));

    const mu::engraving::InstrumentIndex index = mu::engraving::search_template_index_for_id(muse::String(u"piano"));
    ASSERT_NE(index.instr_template, nullptr);
    for (int round = 0; round < 3; ++round) {
        master.append_part(index.instr_template);
        ASSERT_GT(master.parts().size(), 1u);
        Part *part = master.parts().back();
        engrave::assign_missing_ids(master);
        const ElementId part_id = engine_id(*part);
        EXPECT_TRUE(seen.insert(engrave::to_string(part_id)).second) << "a new part inherited an id";
        for (Staff *staff : part->staves()) {
            if (!staff->eid().is_valid()) continue;
            EXPECT_TRUE(seen.insert(id_text(*staff)).second) << "a new staff inherited an id";
        }
        expect_inv(*document);

        master.cmd_remove_part(part);
        engrave::assign_missing_ids(master);
        expect_inv(*document);
        EXPECT_FALSE(live(master, part_id)) << "the removed part still resolves";
    }

    // An excerpt is a score, not an element, and leaves no entry either.
    if (master.excerpts().empty()) music::generate_score_excerpts(&master);
    ASSERT_FALSE(master.excerpts().empty());
    engrave::assign_missing_ids(master);
    mu::engraving::Excerpt *excerpt = master.excerpts().front();
    const ElementId excerpt_id = engine_id(*excerpt->excerpt_score());
    expect_inv(*document);
    master.remove_excerpt(excerpt);
    delete excerpt;
    engrave::assign_missing_ids(master);
    expect_inv(*document);
    EXPECT_FALSE(live(master, excerpt_id)) << "the removed excerpt still resolves";

    // The same round through the edit API: a part and its staves get fresh ids and
    // leave none behind.
    for (int round = 0; round < 3; ++round) {
        auto added = document->edit(engrave::Batch{{engrave::AddPart{"piano", std::nullopt}}});
        ASSERT_TRUE(added) << added.error().message;
        ASSERT_FALSE(added->ops.empty());
        ASSERT_FALSE(added->ops.front().created.empty());
        const ElementId part_id = *added->ops.front().created.front();
        for (const auto &id : added->ops.front().created) {
            ASSERT_TRUE(id) << "a created part or staff has no id";
            EXPECT_TRUE(seen.insert(engrave::to_string(*id)).second) << "a new part or staff inherited an id";
        }
        expect_inv(*document);

        auto removed = document->edit(engrave::Batch{{engrave::RemovePart{part_id}}});
        ASSERT_TRUE(removed) << removed.error().message;
        expect_inv(*document);
        EXPECT_FALSE(live(master, part_id)) << "the removed part still resolves";
    }
}

// --- H7 -------------------------------------------------------------------

// --- H8 -------------------------------------------------------------------

TEST(Handles, H8ReplacedElementsLoseTheirId) {
    auto document = open_with(ElementType::CLEF);
    ASSERT_TRUE(document) << "no corpus score has a clef with an id";
    MasterScore &master = master_of(*document);

    // A ChangeElement swaps the element for a new one: the old id goes, and a new one
    // takes its place at the same position.
    EngravingItem *clef = mu::engraving::to_engraving_item(first_live(master, ElementType::CLEF));
    ASSERT_NE(clef, nullptr);
    Measure *measure = master.tick2measure(clef->tick());
    ASSERT_NE(measure, nullptr);
    Segment *clef_segment = nullptr;
    for (Segment *segment = measure->first(); segment && !clef_segment; segment = segment->next()) {
        if (segment->element(clef->track()) == clef) clef_segment = segment;
    }
    ASSERT_NE(clef_segment, nullptr) << "no segment holds the clef";
    const ElementId clef_id = engine_id(*clef);
    const auto before = engrave::live_ids(master);
    commit(master, [&](MasterScore &) {
        auto *replacement = mu::engraving::Factory::create_clef(clef_segment);
        replacement->set_clef_type(mu::engraving::ClefType::F);
        replacement->set_track(clef->track());
        master.apply_element_replace(clef, replacement);
    });
    const engrave::IdDiff diff = engrave::diff_ids(before, engrave::live_ids(master));
    EXPECT_FALSE(diff.replaced.empty()) << "the replaced clef kept its id";
    EXPECT_FALSE(diff.created.empty());
    for (const ElementId &id : diff.replaced) {
        EXPECT_FALSE(live(master, id)) << "a replaced id still resolves";
    }
    EXPECT_FALSE(live(master, clef_id)) << "the replaced clef still resolves";
    expect_inv(*document);

    // A duration change keeps a chord whose new length fits where it is (the length is
    // a property then) and rebuilds it otherwise; either way the diff and the resolver
    // agree on whether the old id still names the chord.
    mu::engraving::ChordRest *chord_rest = mu::engraving::to_chord_rest(first_live(master, ElementType::CHORD));
    ASSERT_NE(chord_rest, nullptr) << "the walk hands out no chord";
    const ElementId chord_id = engine_id(*chord_rest);
    const auto before_change = engrave::live_ids(master);
    commit(master, [&](MasterScore &) { master.change_c_rlen(chord_rest, mu::engraving::Fraction(1, 2)); });
    const engrave::IdDiff change = engrave::diff_ids(before_change, engrave::live_ids(master));
    const bool dropped = std::ranges::contains(change.replaced, chord_id) || std::ranges::contains(change.removed, chord_id);
    EXPECT_NE(live(master, chord_id), dropped) << "the diff and the resolver disagree about the chord";
    expect_inv(*document);
}

TEST(Handles, H9DuplicateIdsInTheInput) {
    const CraftedInput input = crafted_input();
    if (input.master.empty()) GTEST_SKIP() << "no imported corpus score to write a current MSCX from";
    const auto types = id_types(input.master);
    ASSERT_GE(types.size(), 4u);

    // (a) two elements of the same type claim one id, (b) two types share one id.  Only
    // ids the file itself carries can be duplicated.
    const auto spans = eid_spans(input.master);
    std::set<std::string> in_file;
    for (const auto &[start, end] : spans) in_file.insert(input.master.substr(start, end - start));
    std::map<std::string, std::vector<std::string>> by_type;
    for (const auto &[id, type] : types) {
        if (in_file.contains(id)) by_type[type].push_back(id);
    }
    std::string same_a;
    std::string same_b;
    std::string other;
    std::string same_type;
    for (const auto &[type, ids] : by_type) {
        if (!same_a.empty() || ids.size() < 2) continue;
        same_type = type;
        same_a = ids[0];
        same_b = ids[1];
    }
    for (const auto &[type, ids] : by_type) {
        if (ids.empty() || type == same_type) continue;
        other = ids.front();
        break;
    }
    ASSERT_FALSE(same_b.empty()) << "no two elements share a type";
    ASSERT_FALSE(other.empty()) << "the score has only one element type";

    // (a) two elements of the same type claim one id, (b) two types share one id.  Only
    // ids the file itself carries can be duplicated, and only where the reader keeps both
    // elements: it replaces some of what it read (a measure it rebuilt, for one).
    std::string duplicate;
    for (const auto &[type, ids] : by_type) {
        if (ids.size() < 2) continue;
        std::string candidate = with_copied_id(input.master, ids[1], ids[0]);
        if (candidate == input.master || !reader_sees_duplicate(candidate)) continue;
        duplicate = std::move(candidate);
        break;
    }
    ASSERT_FALSE(duplicate.empty()) << "no two elements of one type that the reader keeps share an id";
    expect_repaired(duplicate);

    bool crossed = false;
    for (const auto &[from_type, from_ids] : by_type) {
        for (const auto &[to_type, to_ids] : by_type) {
            if (from_type == to_type || from_ids.empty() || to_ids.empty()) continue;
            std::string candidate = with_copied_id(input.master, from_ids.front(), to_ids.front());
            if (candidate == input.master || !reader_sees_duplicate(candidate)) continue;
            expect_repaired(candidate);
            crossed = true;
            break;
        }
        if (crossed) break;
    }
    EXPECT_TRUE(crossed) << "two types cannot be given one id";

    // (c) an element a <linkedTo> names appears twice: the link still has one target.
    if (!input.excerpt.empty()) {
        const std::size_t at = input.excerpt.find("<linkedTo>");
        if (at != std::string::npos) {
            const std::size_t start = at + std::string("<linkedTo>").size();
            const std::string target = input.excerpt.substr(start, input.excerpt.find("</linkedTo>", start) - start);
            const auto excerpt_types = id_types(input.excerpt);
            const auto candidate = std::ranges::find_if(excerpt_types, [&target](const auto &entry) { return entry.first != target; });
            ASSERT_TRUE(candidate != excerpt_types.end());
            expect_repaired(with_copied_id(input.excerpt, candidate->first, target));
        }
    }
}

// --- H10 ------------------------------------------------------------------

TEST(Handles, H10MalformedIdsInTheInput) {
    const CraftedInput input = crafted_input();
    if (input.master.empty()) GTEST_SKIP() << "no imported corpus score to write a current MSCX from";
    const std::string &source = input.master;
    const auto spans = eid_spans(source);
    ASSERT_GE(spans.size(), 8u);
    const std::size_t victim = 5;

    const std::vector<std::string> malformed = {
        "",                          // empty
        "A",                         // no separator
        "A_B_C",                     // three halves
        "!_A",                       // a character outside the alphabet
        "AAAAAAAAAAAA_AAAAAAAAAAAA", // halves longer than 11 characters
        "_",                         // two empty halves
    };
    for (const std::string &value : malformed) {
        SCOPED_TRACE("eid '" + value + "'");
        const std::string text = with_eid(source, victim, value);
        ASSERT_NE(text, source);
        auto document = open_text(text);
        ASSERT_TRUE(document);
        MasterScore &master = master_of(*document);
        EXPECT_EQ(master.eid_register()->repairs().malformed, 1u);
        EXPECT_TRUE(warned_about(*document, "malformed"));
        EXPECT_FALSE(engrave::live_ids(master).empty());
        expect_inv(*document);
        // The element got a fresh id, so a caller can still name it.
        EXPECT_TRUE(live(master, engrave::live_ids(master).front().id));

        auto strict = g_engine->open(as_bytes(text), {.strict_ids = true});
        ASSERT_FALSE(strict) << "strict ids accepted '" << value << "'";
        EXPECT_EQ(strict.error().code, ErrorCode::INVALID_INPUT);
    }

    // Two texts that differ but decode to one id: a half keeps its low 64 bits, so
    // the top two bits of an 11-character half do not count.
    const std::string colliding = with_eid(with_eid(source, victim, "AAAAAAAAAAP_AAAAAAAAAAP"), victim + 1, "AAAAAAAAAAf_AAAAAAAAAAf");
    auto document = open_text(colliding);
    ASSERT_TRUE(document);
    EXPECT_EQ(master_of(*document).eid_register()->repairs().duplicate, 1u);
    expect_inv(*document);
}

TEST(Handles, LegacyMalformedEidAndMissingSpannerPayload) {
    const std::string bad_eid_460 = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Part id="1">
      <Staff><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <Instrument id="piano"><trackName>Piano</trackName></Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Chord>
            <eid>!_A</eid>
            <durationType>whole</durationType>
            <Note><pitch>60</pitch><tpc>14</tpc></Note>
          </Chord>
        </voice>
      </Measure>
    </Staff>
  </Score>
</museScore>
)";
    auto repaired = open_text(bad_eid_460);
    ASSERT_TRUE(repaired);
    EXPECT_EQ(master_of(*repaired).eid_register()->repairs().malformed, 1u);
    auto strict = g_engine->open(as_bytes(bad_eid_460), {.strict_ids = true});
    ASSERT_FALSE(strict);
    EXPECT_EQ(strict.error().code, ErrorCode::INVALID_INPUT);

    // Matching start/end legacy Slur connectors without a <Slur> payload must not crash.
    const std::string missing_payload_460 = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Part id="1">
      <Staff><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <Instrument id="piano"><trackName>Piano</trackName></Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Chord>
            <durationType>half</durationType>
            <Spanner type="Slur">
              <next><location><fractions>1/2</fractions></location></next>
            </Spanner>
            <Note><pitch>60</pitch><tpc>14</tpc></Note>
          </Chord>
          <Chord>
            <durationType>half</durationType>
            <Spanner type="Slur">
              <prev><location><fractions>-1/2</fractions></location></prev>
            </Spanner>
            <Note><pitch>62</pitch><tpc>16</tpc></Note>
          </Chord>
        </voice>
      </Measure>
    </Staff>
  </Score>
</museScore>
)";
    auto safe = open_text(missing_payload_460);
    ASSERT_TRUE(safe);
    expect_inv(*safe);
}

// --- H11 ------------------------------------------------------------------

TEST(Handles, H11StaleHandles) {
    ASSERT_GE(g_corpus.size(), 2u);
    const auto path = corpus_file({".mscx"});
    ASSERT_FALSE(path.empty());
    ElementId id;
    {
        auto document = open_path(path);
        ASSERT_TRUE(document);
        Note *note = first_note(master_of(*document));
        ASSERT_NE(note, nullptr);
        id = engine_id(*note);
        EXPECT_TRUE(live(master_of(*document), id));
        expect_inv(*document);
    }

    // Ids are document-scoped: the same file opened twice hands out the same ids, so
    // an id of the closed document is a documented false match in the new one.
    auto same = open_path(path);
    ASSERT_TRUE(same);
    MasterScore &master = master_of(*same);
    const auto same_ids = engrave::live_ids(master);
    const bool present = std::ranges::any_of(same_ids, [&id](const engrave::LiveId &entry) { return entry.id == id; });
    EXPECT_EQ(present, live(master, id)) << "the same file gave a different id set, or the id resolves differently";

    // An id of another document is not found.
    auto other = open_path(corpus_file({".musicxml"}));
    ASSERT_TRUE(other);
    MasterScore &other_master = master_of(*other);
    std::set<std::string> other_texts;
    for (const engrave::LiveId &entry : engrave::live_ids(other_master)) other_texts.insert(engrave::to_string(entry.id));
    int checked = 0;
    for (const engrave::LiveId &entry : same_ids) {
        if (other_texts.contains(engrave::to_string(entry.id))) continue;
        const auto resolved = engrave::resolve(other_master, entry.id);
        ASSERT_FALSE(resolved) << "another document's id resolves";
        EXPECT_EQ(resolved.error().code, ErrorCode::NOT_FOUND);
        ++checked;
        break;
    }
    EXPECT_EQ(checked, 1);

    // An id the snapshot path preserved still works.
    auto restored = reopen(*same);
    ASSERT_TRUE(restored);
    EXPECT_TRUE(live(master_of(*restored), id));

    // Generated and layout elements never carry an id, so no caller can hold one.
    for (EngravingObject *object : engrave::document_objects(master)) {
        if (engrave::id_class(*object) == IdClass::PERSISTED) continue;
        if (engrave::id_class(*object) == IdClass::SESSION) continue; // the file stores one
        EXPECT_FALSE(object->eid().is_valid()) << object_name(*object) << " has an id";
    }
    for (EngravingObject *object : engrave::document_objects(master)) {
        if (!object->is_engraving_item() || !static_cast<EngravingItem &>(*object).generated()) continue;
        EXPECT_FALSE(engrave::takes_id(*object)) << object_name(*object) << " is handed out";
    }
    expect_inv(*same);
}

// --- H12 ------------------------------------------------------------------

TEST(Handles, H12Addresses) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    MasterScore &master = master_of(*document);

    Segment *segment = first_segment(master, SegmentType::CHORD_REST);
    ASSERT_NE(segment, nullptr);
    const auto address = engrave::address_of(*segment);
    ASSERT_TRUE(address) << "a chord-rest segment has no address";
    const auto first = engrave::resolve(master, *address);
    ASSERT_TRUE(first) << first.error().message;
    EXPECT_EQ(first->object, segment);
    EXPECT_TRUE(first->writable);

    // Unrelated edits leave the address alone.
    Note *note = first_note(master);
    ASSERT_NE(note, nullptr);
    commit(master, [&](MasterScore &) {
        note->undo_change_property(mu::engraving::Pid::COLOR, muse::draw::Color(0, 0, 255), mu::engraving::PropertyFlags::NOSTYLE);
    });
    const auto second = engrave::resolve(master, *address);
    ASSERT_TRUE(second) << second.error().message;
    EXPECT_EQ(second->object, segment);

    // The address fails once its measure is gone.
    Measure *measure = segment->measure()->next_measure();
    if (!measure) measure = segment->measure();
    Segment *doomed = measure->first(SegmentType::CHORD_REST);
    ASSERT_NE(doomed, nullptr);
    const auto doomed_address = engrave::address_of(*doomed);
    ASSERT_TRUE(doomed_address);
    commit(master, [&](MasterScore &) { master.delete_measures(measure, measure); });
    const auto gone = engrave::resolve(master, *doomed_address);
    EXPECT_FALSE(gone) << "an address of a removed measure still resolves";
    expect_inv(*document);

    // Layout addresses are read-only and name the layout object. Edits no longer
    // lay out, so this direct resolve requests geometry the way get() does.
    master.ensure_layout();
    const auto page = engrave::resolve(master, engrave::PageAddress{.score = {}, .index = 0});
    ASSERT_TRUE(page) << page.error().message;
    EXPECT_FALSE(page->writable);
    ASSERT_EQ(page->object->type(), ElementType::PAGE);
    const auto page_address = engrave::address_of(*page->object);
    ASSERT_TRUE(page_address);
    const auto page_again = engrave::resolve(master, *page_address);
    ASSERT_TRUE(page_again);
    EXPECT_EQ(page_again->object, page->object);

    const auto system = engrave::resolve(master, engrave::SystemAddress{.score = {}, .page = 0, .index = 0});
    ASSERT_TRUE(system) << system.error().message;
    EXPECT_FALSE(system->writable);
    ASSERT_EQ(system->object->type(), ElementType::SYSTEM);
    const auto system_address = engrave::address_of(*system->object);
    ASSERT_TRUE(system_address);
    const auto system_again = engrave::resolve(master, *system_address);
    ASSERT_TRUE(system_again);
    EXPECT_EQ(system_again->object, system->object);

    // A missing position is a stale handle, an unknown kind a caller error.
    const auto missing = engrave::resolve(master, engrave::PageAddress{.score = {}, .index = 9999});
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ErrorCode::STALE_HANDLE);
    auto bad_kind = std::get<engrave::SegmentAddress>(*address);
    bad_kind.kind = "NoSuchSegment";
    const auto bad = engrave::resolve(master, engrave::Address{bad_kind});
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code, ErrorCode::INVALID_ARGUMENT);
}

TEST(Handles, SegmentTicksValidateBeforeResolvingOrEditing) {
    const auto fixture = std::ranges::find_if(g_corpus, [](const auto &path) { return path.filename() == "linked_parts.mscx"; });
    ASSERT_NE(fixture, g_corpus.end());
    auto document = open_path(*fixture);
    ASSERT_TRUE(document);
    MasterScore &master = master_of(*document);
    Segment *segment = first_segment(master, SegmentType::CHORD_REST);
    ASSERT_NE(segment, nullptr);
    const auto resolved_address = engrave::address_of(*segment);
    ASSERT_TRUE(resolved_address);
    auto address = std::get<engrave::SegmentAddress>(*resolved_address);
    ASSERT_EQ(address.tick.num, 0);
    const std::int64_t low = std::numeric_limits<int>::min();
    const std::int64_t high = std::numeric_limits<int>::max();

    // The entire component range is accepted, including unreduced zero.
    for (const auto tick : {engrave::Fraction{0, 1}, {0, high - 1}, {0, high}}) {
        address.tick = tick;
        const auto found = engrave::resolve(master, engrave::Address{address});
        ASSERT_TRUE(found) << tick.num << '/' << tick.den << ": " << found.error().message;
        EXPECT_EQ(found->object, segment);
        EXPECT_TRUE(document->get(engrave::Ref{engrave::Address{address}}));
    }
    for (const auto tick : {engrave::Fraction{low, 1}, {low + 1, 1}, {high - 1, 1}, {high, 1}}) {
        address.tick = tick;
        const auto found = document->get(engrave::Ref{engrave::Address{address}});
        ASSERT_FALSE(found);
        EXPECT_EQ(found.error().code, ErrorCode::STALE_HANDLE);
    }
    // Equivalent large fractions must still resolve a nonzero position in a
    // later measure, without overflowing the measure-relative lookup.
    Measure *later_measure = segment->measure()->next_measure();
    ASSERT_NE(later_measure, nullptr);
    Segment *later_segment = later_measure->first(SegmentType::CHORD_REST);
    ASSERT_NE(later_segment, nullptr);
    const auto later_address = engrave::address_of(*later_segment);
    ASSERT_TRUE(later_address);
    auto equivalent = std::get<engrave::SegmentAddress>(*later_address);
    ASSERT_GT(equivalent.tick.num, 0);
    const std::int64_t scale = std::min(high / equivalent.tick.num, high / equivalent.tick.den);
    ASSERT_GT(scale, 1);
    equivalent.tick.num *= scale;
    equivalent.tick.den *= scale;
    const auto same_position = engrave::resolve(master, engrave::Address{equivalent});
    ASSERT_TRUE(same_position) << same_position.error().message;
    EXPECT_EQ(same_position->object, later_segment);

    // Adding two int fractions can leave 64-bit engine terms. The exact
    // absolute position is 2/1, not the wrapped numerator -4.
    const auto old_measure_tick = later_measure->tick();
    const auto old_relative_tick = later_segment->rtick();
    later_measure->set_tick(mu::engraving::Fraction(static_cast<int>(high), static_cast<int>(high - 1)));
    later_segment->set_rtick(mu::engraving::Fraction(static_cast<int>(high - 2), static_cast<int>(high - 1)));
    const auto wide_address = engrave::address_of(*later_segment);
    equivalent.tick = {2, 1};
    const auto wide_position = engrave::resolve(master, engrave::Address{equivalent});
    equivalent.tick = {-4, high - 1};
    const auto wrapped_position = engrave::resolve(master, engrave::Address{equivalent});
    later_measure->set_tick(mu::engraving::Fraction(1, static_cast<int>(high - 1)));
    later_segment->set_rtick(mu::engraving::Fraction(1, static_cast<int>(high)));
    const auto unrepresentable_address = engrave::address_of(*later_segment);
    later_measure->set_tick(old_measure_tick);
    later_segment->set_rtick(old_relative_tick);
    ASSERT_TRUE(wide_address);
    const auto written_tick = std::get<engrave::SegmentAddress>(*wide_address).tick;
    EXPECT_EQ(written_tick.num, 2);
    EXPECT_EQ(written_tick.den, 1);
    ASSERT_TRUE(wide_position) << wide_position.error().message;
    EXPECT_EQ(wide_position->object, later_segment);
    ASSERT_FALSE(wrapped_position);
    EXPECT_EQ(wrapped_position.error().code, ErrorCode::STALE_HANDLE);
    EXPECT_FALSE(unrepresentable_address);

    for (const auto tick : {engrave::Fraction{low - 1, 1},
                            {high + 1, 1},
                            {4294967296LL, 1},
                            {std::numeric_limits<std::int64_t>::min(), 1},
                            {std::numeric_limits<std::int64_t>::max(), 1},
                            {0, high + 1},
                            {0, std::numeric_limits<std::int64_t>::max()},
                            {0, 0},
                            {0, -1},
                            {0, std::numeric_limits<std::int64_t>::min()}}) {
        address.tick = tick;
        const auto found = document->get(engrave::Ref{engrave::Address{address}});
        ASSERT_FALSE(found) << tick.num << '/' << tick.den;
        EXPECT_EQ(found.error().code, ErrorCode::INVALID_ARGUMENT);
    }

    // This used to wrap to tick zero and add text at the first segment.
    address.tick = {4294967296LL, 1};
    const auto before = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(before);
    const auto revision = document->revision();
    const auto edit = document->edit({{engrave::AddText{engrave::Address{address}, "staff", "wrong position"}}});
    ASSERT_FALSE(edit);
    EXPECT_EQ(edit.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(document->revision(), revision);
    const auto after = document->export_score({.format = engrave::Format::MSCX});
    ASSERT_TRUE(after);
    EXPECT_EQ(*after, *before);

    // A foreign or missing measure keeps the usual handle error even if the
    // rest of the address is malformed.
    address.measure = {0, 0};
    const auto missing = document->get(engrave::Ref{engrave::Address{address}});
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code, ErrorCode::NOT_FOUND);
}

// --- H13 ------------------------------------------------------------------

TEST(Handles, H13DeterministicIds) {
    // The engine is one per process and deterministic ids are a process-global
    // switch, so the check runs in a child process of this same binary.
    ASSERT_FALSE(g_deterministic) << "already the deterministic child";
    std::string command = "\"" + g_program + "\" --deterministic --gtest_filter=Handles.H13IdsWithDeterministicEngine";
#ifdef _WIN32
    command = "call " + command; // cmd strips the first quote of a command that starts with one
#endif
    for (const auto &argument : g_engine_args) command += " \"" + argument + "\"";
    const std::filesystem::path log = std::filesystem::temp_directory_path() / "engrave-handles-h13.log";
    command += " > \"" + log.string() + "\" 2>&1";
    const int status = std::system(command.c_str());
    if (status != 0) {
        std::fprintf(stderr, "the deterministic child failed: %s\n%s\n", command.c_str(), read_text(log).c_str());
    }
    EXPECT_EQ(status, 0) << "the deterministic child failed";
}

TEST(Handles, H13IdsWithDeterministicEngine) {
    if (!g_deterministic) GTEST_SKIP() << "only meaningful in the --deterministic child";
    auto first = open_path(g_corpus.front());
    ASSERT_TRUE(first);
    auto second = open_path(g_corpus.front());
    ASSERT_TRUE(second);
    EXPECT_EQ(element_map(*second), element_map(*first)) << "two opens of one file disagree";
    expect_inv(*first);
    expect_inv(*second);
    auto carrier_document = open_text(R"(<museScore version="5.00"><Score><eid>B_B</eid>
      <Part><Staff id="1"/><Instrument><Channel/></Instrument></Part>
      <Staff id="1"><Measure><voice><Chord><durationType>whole</durationType>
      <Stem><userLen>1</userLen></Stem><Note><eid>C_C</eid><pitch>60</pitch><tpc>14</tpc></Note>
      </Chord></voice></Measure></Staff></Score></museScore>)");
    ASSERT_TRUE(carrier_document);
    auto &carrier_master = master_of(*carrier_document);
    auto *carrier_note = first_note(carrier_master);
    ASSERT_NE(carrier_note, nullptr);
    EXPECT_EQ(carrier_note->eid().to_std_string(), "C_C");
    const auto *carrier = carrier_note->chord()->authored_overrides.carrier({mu::engraving::AuthoredChildRole::STEM});
    ASSERT_NE(carrier, nullptr);
    EXPECT_NE(carrier->legacy_eid.to_std_string(), "C_C");
    auto restored = reopen(*carrier_document);
    ASSERT_TRUE(restored);
    EXPECT_EQ(master_of(*restored).eid_register()->repairs().duplicate, 0u);
}

TEST(Handles, RegistryIdentityUpdatesKeepTheRuntimeHandle) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto &registry = *master.eid_register();
    const auto old_eid = note->eid();
    const auto handle = registry.handle_of(note);
    const auto replacement = registry.new_eid_for_item(note);
    ASSERT_NE(replacement, old_eid);
    EXPECT_EQ(registry.handle_of(note), handle);
    EXPECT_EQ(registry.item_from_eid(old_eid), nullptr);
    EXPECT_EQ(registry.item_from_eid(replacement), note);
    const auto repairs = registry.repairs().duplicate;
    EXPECT_FALSE(registry.register_item_eid(replacement, &master));
    EXPECT_EQ(registry.repairs().duplicate, repairs + 1);
    EXPECT_EQ(registry.item_from_handle(handle), note);
    EXPECT_EQ(note->eid(), replacement);
    expect_inv(*document);
}

TEST(Handles, LoadedMeasureSpacersKeepMembershipThroughRowMembership) {
    using namespace mu::engraving;
    auto xml = read_text(g_source_root / "tests/data/inputs/golden/chordline_path.mscx");
    constexpr std::string_view tag = "<Measure>";
    const auto at = xml.find(tag);
    ASSERT_NE(at, std::string::npos);
    xml.insert(at + tag.size(), "<vspacerUp>2.5</vspacerUp><vspacerDown>3.5</vspacerDown>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto &registry = *master.eid_register();
    auto *measure = master.first_measure();
    ASSERT_NE(measure, nullptr);
    const std::array spacers{measure->vspacer_up(0), measure->vspacer_down(0)};
    std::array<SourceHandle, 2> handles;
    std::array<ElementId, 2> ids;
    std::array<Spatium, 2> gaps;
    for (size_t i = 0; i < spacers.size(); ++i) {
        ASSERT_NE(spacers[i], nullptr);
        ASSERT_TRUE(spacers[i]->eid().is_valid());
        handles[i] = registry.handle_of(spacers[i]);
        ids[i] = engrave::from_engine(spacers[i]->eid());
        gaps[i] = spacers[i]->gap();
        ASSERT_EQ(registry.live_item(handles[i]), spacers[i]);
        const auto resolved = engrave::resolve(master, ids[i]);
        ASSERT_TRUE(resolved);
        EXPECT_EQ(resolved->object, spacers[i]);
    }
    EXPECT_EQ(master.layout_state.whole_count + master.layout_state.range_count, 0u);
    expect_inv(*document);
}

TEST(Handles, RegistryHandlesAreDocumentScopedAndReadOnly) {
    const auto path = corpus_file({".mscx"});
    auto first = open_path(path);
    auto second = open_path(path);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    auto &a = *master_of(*first).eid_register();
    auto &b = *master_of(*second).eid_register();
    auto *note = first_note(master_of(*first));
    ASSERT_NE(note, nullptr);
    const auto handle = a.handle_of(note);
    ASSERT_TRUE(handle.is_valid());
    EXPECT_EQ(b.item_from_handle(handle), nullptr);
    const auto before = a.entries();
    for (int i = 0; i != 100; ++i) {
        EXPECT_EQ(a.item_from_handle(handle), note);
        EXPECT_EQ(a.item_from_eid(note->eid()), note);
        EXPECT_EQ(a.handle_of(note), handle);
    }
    EXPECT_EQ(a.entries(), before);
    EXPECT_EQ(a.item_from_handle({}), nullptr);
    auto invalid = handle;
    invalid.slot = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(a.item_from_handle(invalid), nullptr);
    invalid = handle;
    ++invalid.generation;
    EXPECT_EQ(a.item_from_handle(invalid), nullptr);
    expect_inv(*first);
    expect_inv(*second);
}

TEST(Handles, RegistryDestructionRetiresHandleBeforeAddressReuse) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto &registry = *master.eid_register();
    auto *copy = note->clone();
    copy->assign_new_eid();
    const auto handle = registry.handle_of(copy);
    ASSERT_TRUE(handle.is_valid());
    delete copy;
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    auto *next = note->clone();
    next->assign_new_eid();
    const auto next_handle = registry.handle_of(next);
    EXPECT_NE(next_handle, handle);
    if (next_handle.slot == handle.slot) EXPECT_GT(next_handle.generation, handle.generation);
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    delete next;
    expect_inv(*document);
}

TEST(Handles, DetachedButParentedSourceRejectsLookup) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *chord = note->chord();
    auto &registry = *master.eid_register();
    auto *copy = note->clone();
    copy->assign_new_eid();
    const auto handle = registry.handle_of(copy);
    EXPECT_EQ(registry.live_item(handle), nullptr) << "construction is not insertion";
    chord->add(copy);
    engrave::assign_missing_ids(master);
    const auto id = engrave::from_engine(copy->eid());
    EXPECT_EQ(registry.live_item(handle), copy);
    chord->remove(copy);
    EXPECT_EQ(copy->ownership_parent(), chord);
    EXPECT_EQ(registry.item_from_handle(handle), copy) << "detached roots remain alive until batch cleanup";
    const auto result = engrave::resolve(master, id);
    EXPECT_FALSE(result);
    if (!result) EXPECT_EQ(result.error().code, ErrorCode::STALE_HANDLE);
    chord->add(copy);
    EXPECT_EQ(registry.handle_of(copy), handle);
    EXPECT_TRUE(engrave::resolve(master, id));
    chord->remove(copy);
    delete copy;
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    expect_inv(*document);
}

TEST(Handles, SourceSubtreeRemovalAndReinsertionPreserveHandles) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *measure = note->chord()->measure();
    auto &registry = *master.eid_register();
    const std::array<EngravingObject *, 4> objects{measure, note->chord()->segment(), note->chord(), note};
    std::array<mu::engraving::SourceHandle, 4> handles;
    for (std::size_t i = 0; i != objects.size(); ++i) {
        handles[i] = registry.handle_of(objects[i]);
        ASSERT_TRUE(handles[i].is_valid());
        ASSERT_EQ(registry.live_item(handles[i]), objects[i]);
    }
    master.measures()->remove(measure);
    for (auto handle : handles) EXPECT_EQ(registry.live_item(handle), nullptr);
    EXPECT_EQ(measure->ownership_parent(), &master);
    master.measures()->add(measure);
    for (std::size_t i = 0; i != objects.size(); ++i) {
        EXPECT_EQ(registry.handle_of(objects[i]), handles[i]);
        EXPECT_EQ(registry.live_item(handles[i]), objects[i]);
    }
    expect_inv(*document);
}

TEST(Handles, HandlesRejectForcedObjectAddressReuse) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_FALSE(master.parts().empty());
    auto &registry = *master.eid_register();
    alignas(Part) std::array<std::byte, sizeof(Part)> storage;
    auto *first = std::construct_at(reinterpret_cast<Part *>(storage.data()), &master);
    first->assign_new_eid();
    const auto old = registry.handle_of(first);
    ASSERT_TRUE(old.is_valid());
    std::destroy_at(first);
    auto *second = std::construct_at(reinterpret_cast<Part *>(storage.data()), &master);
    second->assign_new_eid();
    const auto current = registry.handle_of(second);
    EXPECT_EQ(first, second);
    EXPECT_EQ(current.slot, old.slot);
    EXPECT_GT(current.generation, old.generation);
    EXPECT_EQ(registry.item_from_handle(old), nullptr);
    EXPECT_EQ(registry.item_from_handle(current), second);
    std::destroy_at(second);
    expect_inv(*document);
}

TEST(Handles, ClosedDocumentHandlesCannotResolveAfterReopen) {
    const auto path = corpus_file({".mscx"});
    auto first = open_path(path);
    ASSERT_TRUE(first);
    auto *note = first_note(master_of(*first));
    ASSERT_NE(note, nullptr);
    const auto handle = master_of(*first).eid_register()->handle_of(note);
    const auto eid = note->eid();
    first.reset();
    auto reopened = open_path(path);
    ASSERT_TRUE(reopened);
    auto &registry = *master_of(*reopened).eid_register();
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    ASSERT_NE(registry.item_from_eid(eid), nullptr);
    EXPECT_NE(registry.handle_of(registry.item_from_eid(eid)).document, handle.document);
    expect_inv(*reopened);
}

TEST(Handles, ExcerptTeardownRetiresDummyOwnedSpanners) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    if (master.excerpts().empty()) music::generate_score_excerpts(&master);
    ASSERT_FALSE(master.excerpts().empty());
    auto *excerpt = master.excerpts().front();
    auto *score = excerpt->excerpt_score();
    auto *spanner = mu::engraving::Factory::create_text_line(score->dummy());
    spanner->set_track(0);
    score->add_spanner(spanner, false);
    engrave::assign_missing_ids(master);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(spanner);
    const auto eid = spanner->eid();
    ASSERT_EQ(registry.live_item(handle), spanner);
    master.remove_excerpt(excerpt);
    delete excerpt;
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    EXPECT_EQ(registry.item_from_eid(eid), nullptr);
    EXPECT_FALSE(engrave::resolve(master, engrave::from_engine(eid)));
    expect_inv(*document);
}

TEST(Handles, ImportedTieIsAddressableBeforeSaving) {
    auto xml = read_text(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    const auto at = xml.find("<eid>J_J</eid>");
    ASSERT_NE(at, std::string::npos);
    xml.erase(at, std::string_view("<eid>J_J</eid>").size());
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *tie = note->tie_for();
    ASSERT_NE(tie, nullptr);
    ASSERT_NE(tie->end_note(), nullptr);
    EXPECT_EQ(tie->tick(), mu::engraving::Fraction(0, 1));
    EXPECT_EQ(tie->end_note()->tick(), mu::engraving::Fraction(1, 4));
    EXPECT_EQ(engrave::id_class(*tie), IdClass::PERSISTED);
    ASSERT_TRUE(tie->eid().is_valid());
    const auto id = engine_id(*tie);
    const auto resolved = engrave::resolve(master, id);
    ASSERT_TRUE(resolved);
    EXPECT_EQ(resolved->object, tie);
    EXPECT_TRUE(master.eid_register()->published(master.eid_register()->handle_of(tie)));
    const auto ids = engrave::live_ids(master);
    EXPECT_TRUE(std::ranges::any_of(ids, [id](const auto &entry) { return entry.id == id; }));
    auto restored = reopen(*document);
    ASSERT_TRUE(restored);
    const auto reopened = engrave::resolve(master_of(*restored), id);
    ASSERT_TRUE(reopened);
    ASSERT_TRUE(reopened->object->is_tie());
    ASSERT_NE(mu::engraving::to_tie(reopened->object)->end_note(), nullptr);
    EXPECT_EQ(mu::engraving::to_tie(reopened->object)->end_note()->tick(), mu::engraving::Fraction(1, 4));
    expect_inv(*document);
    expect_inv(*restored);
}

TEST(Handles, ExcludingTieEndFromPartDoesNotPublishAnOrphan) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto excerpts = master.excerpts();
    for (auto *excerpt : excerpts) {
        master.remove_excerpt(excerpt);
        delete excerpt;
    }
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    ASSERT_NE(note->tie_for(), nullptr);
    note->tie_for()->end_note()->chord()->set_exclude_from_other_parts(true);
    ASSERT_TRUE(music::generate_score_excerpts(&master));
    engrave::assign_missing_ids(master);
    auto *part_note = mu::engraving::to_note(note->find_linked_in_staff(master.excerpts().front()->excerpt_score()->staff(0)));
    ASSERT_NE(part_note, nullptr);
    EXPECT_EQ(part_note->tie_for(), nullptr);
    EXPECT_EQ(note->tie_for()->end_note()->pitch(), 71);
    auto restored = reopen(*document);
    ASSERT_TRUE(restored);
    expect_inv(*document);
    expect_inv(*restored);
}

TEST(Handles, RewrittenPartKeepsSlightBendIdentity) {
    using namespace mu::engraving;
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *bend = Factory::create_guitar_bend(note);
    bend->set_bend_type(GuitarBendType::SLIGHT_BEND);
    bend->set_note_span(note, note);
    {
        Command command(master);
        master.undo_add_element(bend, true);
        command.commit();
    }
    ASSERT_FALSE(master.excerpts().empty());
    ASSERT_NE(bend->find_linked_in_score(master.excerpts().front()->excerpt_score()), nullptr);
    engrave::assign_missing_ids(master);
    const auto original_id = engine_id(*bend);
    const auto original_handle = master.eid_register()->handle_of(bend);
    ASSERT_TRUE(document->edit({{engrave::SetTimeSig{engine_id(*master.first_measure()), {3, 4}}}}));
    EXPECT_EQ(master.eid_register()->live_item(original_handle), nullptr);
    EXPECT_FALSE(engrave::resolve(master, original_id));
    GuitarBend *part_bend = nullptr;
    for (auto *object : engrave::document_objects(master))
        if (object->is_guitar_bend() && object->score() != &master) part_bend = to_guitar_bend(object);
    ASSERT_NE(part_bend, nullptr);
    EXPECT_EQ(part_bend->bend_type(), GuitarBendType::SLIGHT_BEND);
    EXPECT_EQ(part_bend->start_note()->pitch(), 71);
    const auto id = engine_id(*part_bend);
    ASSERT_TRUE(engrave::resolve(master, id));
    auto restored = reopen(*document);
    ASSERT_TRUE(restored);
    const auto found = engrave::resolve(master_of(*restored), id);
    ASSERT_TRUE(found);
    ASSERT_TRUE(found->object->is_guitar_bend());
    EXPECT_EQ(to_guitar_bend(found->object)->bend_type(), GuitarBendType::SLIGHT_BEND);
    expect_inv(*document);
    expect_inv(*restored);
}

TEST(Handles, ConnectingUnmatchedTieRetiresItsHandle) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    Note *last = nullptr;
    for (auto *object : engrave::document_objects(master)) {
        if (!object->is_note()) continue;
        auto *note = mu::engraving::to_note(object);
        if (!last || note->tick() > last->tick()) last = note;
    }
    ASSERT_NE(last, nullptr);
    ASSERT_EQ(last->tie_for(), nullptr);
    auto *tie = mu::engraving::Factory::create_tie(last);
    tie->set_track(last->track());
    tie->set_start_note(last);
    last->set_tie_for(tie);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(tie);
    ASSERT_EQ(registry.live_item(handle), tie);
    master.connect_ties(false);
    EXPECT_EQ(last->tie_for(), nullptr);
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    expect_inv(*document);
}

TEST(Handles, RemovedBeamStaysAliveUntilBatchEnd) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *chord = note->chord();
    ASSERT_EQ(chord->beam(), nullptr);
    auto *beam = mu::engraving::Factory::create_beam(&master);
    beam->set_generated(false);
    beam->add(chord);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(beam);
    ASSERT_EQ(registry.live_item(handle), beam);
    Command command(master);
    master.apply_element_remove(beam);
    EXPECT_EQ(registry.item_from_handle(handle), beam);
    EXPECT_EQ(registry.live_item(handle), nullptr);
    command.commit();
    EXPECT_EQ(registry.item_from_handle(handle), nullptr);
    EXPECT_EQ(registry.live_item(handle), nullptr);
    expect_inv(*document);
}

TEST(Handles, MovingTremoloRebindsMembership) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    std::vector<mu::engraving::Chord *> chords;
    for (auto *segment = measure->first(); segment; segment = segment->next()) {
        auto *element = segment->element(0);
        if (element && element->is_chord()) chords.push_back(mu::engraving::to_chord(element));
    }
    ASSERT_GE(chords.size(), 4u);
    auto *tremolo = mu::engraving::Factory::create_tremolo_two_chord(chords[0]);
    tremolo->set_tremolo_type(mu::engraving::TremoloType::C8);
    tremolo->set_duration_type(chords[0]->duration_type());
    tremolo->set_chords(chords[0], chords[1]);
    chords[0]->set_tremolo_two_chord(tremolo, false);
    chords[1]->set_tremolo_two_chord(tremolo, false);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(tremolo);
    ASSERT_EQ(registry.live_item(handle), tremolo);
    Command command(master);
    mu::engraving::apply_move_tremolo(&master, chords[2]->tick(), chords[3]->tick(), tremolo, 0);
    auto *old_segment = chords[0]->segment();
    old_segment->remove(chords[0]);
    EXPECT_EQ(registry.live_item(handle), tremolo);
    old_segment->add(chords[0]);
    command.commit();
    EXPECT_EQ(registry.live_item(handle), tremolo);
    delete tremolo;
    expect_inv(*document);
}

TEST(Handles, StaffSortingAndReparentingPreserveIds) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/chordline_path.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *chord = note->chord();
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(note);
    const auto id = engine_id(*note);
    auto *staff = master.staff(0);
    const auto staff_id = engine_id(*staff);
    const auto staff_handle = registry.handle_of(staff);
    std::vector<mu::engraving::staff_idx_t> order;
    for (size_t i = 0; i < master.staves().size(); ++i) order.push_back(master.staves().size() - i - 1);
    ASSERT_EQ(master.staves().size(), 2u);
    master.sort_staves(order);
    EXPECT_EQ(master.staff(1), staff);
    EXPECT_EQ(engrave::resolve(master, staff_id)->object, staff);
    EXPECT_EQ(registry.handle_of(staff), staff_handle);
    auto *destination = mu::engraving::to_chord(chord->segment()->next_cr(chord->track())->element(chord->track()));
    ASSERT_NE(destination, nullptr);
    mu::engraving::apply_element_reparent(*note, *destination, destination->track());
    EXPECT_EQ(engrave::resolve(master, id)->object, note);
    EXPECT_EQ(registry.handle_of(note), handle);
    EXPECT_EQ(note->chord(), destination);
    mu::engraving::apply_element_reparent(*note, *chord, chord->track());
    auto *segment = chord->segment();
    auto *measure = segment->measure();
    const auto tick = segment->rtick();
    const auto segment_handle = registry.handle_of(segment);
    ASSERT_NE(measure->next_measure(), nullptr);
    mu::engraving::apply_segment_reparent(*segment, *measure->next_measure(), mu::engraving::Fraction(1, 4));
    EXPECT_EQ(registry.live_item(segment_handle), segment);
    EXPECT_EQ(engrave::resolve(master, id)->object, note);
    mu::engraving::apply_segment_reparent(*segment, *measure, tick);
    expect_inv(*document);
}

TEST(Handles, BatchDiffRetainsDestroyedIdentityAcrossSlotReuse) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *chord = note->chord();
    const auto old_id = engine_id(*note);
    ElementId new_id;
    auto result = engrave::api::BatchGuard(*document).run({[&](engrave::api::EditContext &) -> engrave::Result<void> {
        auto *replacement = note->clone();
        master.apply_element_remove(note);
        delete note;
        replacement->assign_new_eid();
        chord->add(replacement);
        new_id = engine_id(*replacement);
        return {};
    }});
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::ranges::contains(result->created, new_id));
    EXPECT_TRUE(std::ranges::contains(result->replaced, old_id));
    EXPECT_FALSE(engrave::resolve(master, old_id));
    EXPECT_TRUE(engrave::resolve(master, new_id));
    expect_inv(*document);
}

TEST(Handles, InsertBeforeRemovalUsesTheOriginalReplacementPosition) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *note = first_note(master);
    ASSERT_NE(note, nullptr);
    auto *other = note->clone();
    other->set_pitch(64);
    note->chord()->add(other);
    engrave::assign_missing_ids(master);
    const auto id = engine_id(*note);
    const auto measure = engine_id(*note->chord()->measure());
    const auto result = document->edit({{engrave::InsertMeasures{measure, 1}, engrave::Remove{id}}});
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::ranges::contains(result->replaced, id));
    EXPECT_FALSE(std::ranges::contains(result->removed, id));
    EXPECT_FALSE(engrave::resolve(master, id));
    expect_inv(*document);
}

TEST(Handles, RemovedExcerptDetachesBeamsBeforeTheNextOperation) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    if (master.excerpts().empty()) music::generate_score_excerpts(&master);
    ASSERT_FALSE(master.excerpts().empty());
    auto *excerpt = master.excerpts().front();
    auto *score = excerpt->excerpt_score();
    Note *note = nullptr;
    for (auto *object : engrave::document_objects(master))
        if (object->score() == score && object->is_note()) {
            note = mu::engraving::to_note(object);
            break;
        }
    ASSERT_NE(note, nullptr);
    auto *beam = mu::engraving::Factory::create_beam(score);
    beam->set_generated(false);
    beam->add(note->chord());
    engrave::assign_missing_ids(master);
    const auto id = engine_id(*beam);
    ASSERT_TRUE(engrave::resolve(master, id));
    auto result = engrave::api::BatchGuard(*document).run({[&](engrave::api::EditContext &) -> engrave::Result<void> {
                                                               master.apply_excerpt(excerpt, false);
                                                               return {};
                                                           },
                                                           [&](engrave::api::EditContext &) -> engrave::Result<void> {
                                                               EXPECT_FALSE(engrave::resolve(master, id));
                                                               return {};
                                                           }});
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::ranges::contains(result->removed, id));
    EXPECT_FALSE(engrave::resolve(master, id));
    expect_inv(*document);
}

TEST(Handles, RegeneratedExcerptReplacesIdsAtTheOriginalScoreIndex) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/hairpins_and_repeats.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    if (master.excerpts().empty()) music::generate_score_excerpts(&master);
    ASSERT_EQ(master.excerpts().size(), 1u);
    engrave::assign_missing_ids(master);
    auto *excerpt = master.excerpts().front();
    ElementId old_id;
    for (auto *object : engrave::document_objects(master))
        if (object->score() == excerpt->excerpt_score() && object->is_note()) {
            old_id = engine_id(*object);
            break;
        }
    ASSERT_TRUE(engrave::resolve(master, old_id));
    auto result = engrave::api::BatchGuard(*document).run({[&](engrave::api::EditContext &) -> engrave::Result<void> {
                                                               master.apply_excerpt(excerpt, false);
                                                               return {};
                                                           },
                                                           [&](engrave::api::EditContext &) -> engrave::Result<void> {
                                                               music::generate_score_excerpts(&master);
                                                               return {};
                                                           }});
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::ranges::contains(result->replaced, old_id));
    EXPECT_FALSE(std::ranges::contains(result->removed, old_id));
    expect_inv(*document);
}

TEST(Handles, EmptyingPublishedTextRemovesItsDiffIdButKeepsItsHandle) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/chordline_path.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *text = first_live(master, ElementType::TEXT);
    ASSERT_NE(text, nullptr);
    const auto id = engine_id(*text);
    const auto cleared = document->edit({{engrave::SetProperty{id, "text", std::string{}}}});
    ASSERT_TRUE(cleared);
    EXPECT_TRUE(std::ranges::contains(cleared->removed, id));
    EXPECT_TRUE(engrave::resolve(master, id));
    const auto filled = document->edit({{engrave::SetProperty{id, "text", std::string{"Restored title"}}}});
    ASSERT_TRUE(filled);
    EXPECT_TRUE(std::ranges::contains(filled->created, id));
    expect_inv(*document);
}

TEST(Handles, HiddenStaffSpacersLeaveTheDiffWhileTheirHandlesStillResolve) {
    auto xml = read_text(g_source_root / "tests/data/inputs/golden/chordline_path.mscx");
    const auto at = xml.find("<Measure>");
    ASSERT_NE(at, std::string::npos);
    xml.insert(at + std::string_view("<Measure>").size(), "<vspacerUp>2.5</vspacerUp><vspacerDown>3.5</vspacerDown>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    const std::array ids{engine_id(*measure->vspacer_up(0)), engine_id(*measure->vspacer_down(0))};
    for (auto target : {engine_id(*master.staff(0)), engine_id(*master.staff(0)->part())}) {
        const auto hidden = document->edit({{engrave::SetProperty{target, "visible", false}}});
        ASSERT_TRUE(hidden);
        for (auto id : ids) {
            EXPECT_TRUE(std::ranges::contains(hidden->removed, id));
            EXPECT_TRUE(engrave::resolve(master, id));
        }
        const auto shown = document->edit({{engrave::SetProperty{target, "visible", true}}});
        ASSERT_TRUE(shown);
        for (auto id : ids) EXPECT_TRUE(std::ranges::contains(shown->created, id));
    }
    expect_inv(*document);
}

TEST(Handles, RejectedAnnotationDoesNotBecomeSourceMember) {
    auto document = open_path(corpus_file({".mscx"}));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *measure = master.first_measure();
    auto *segment = measure->get_segment(SegmentType::END_BAR_LINE, measure->end_tick());
    auto *first = mu::engraving::Factory::create_play_count_text(segment);
    first->set_track(0);
    segment->add(first);
    auto *rejected = mu::engraving::Factory::create_play_count_text(segment);
    rejected->set_track(0);
    rejected->assign_new_eid();
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(rejected);
    segment->add(rejected);
    EXPECT_EQ(rejected->ownership_parent(), segment);
    EXPECT_EQ(registry.live_item(handle), nullptr);
    EXPECT_EQ(segment->find_annotation(ElementType::PLAY_COUNT_TEXT, 0, 0), first);
    delete rejected;
    segment->remove(first);
    delete first;
    expect_inv(*document);
}

TEST(Handles, TextFrameReplacementMaintainsSourceMembership) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/repeat70.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *object = first_live(master, ElementType::TBOX);
    ASSERT_NE(object, nullptr);
    auto *frame = mu::engraving::to_t_box(object);
    auto *text = frame->text();
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_of(text);
    ASSERT_EQ(registry.live_item(handle), text);
    frame->remove(text);
    EXPECT_EQ(registry.live_item(handle), nullptr);
    EXPECT_EQ(registry.live_item(registry.handle_of(frame->text())), frame->text());
    frame->add(text);
    EXPECT_EQ(registry.handle_of(text), handle);
    EXPECT_EQ(registry.live_item(handle), text);
    expect_inv(*document);
}

TEST(Handles, NativeArchiveAndCheckpointPreserveExcerptFileName) {
    auto document = open_path(g_source_root / "tests/data/inputs/golden/linked_parts.mscx");
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_FALSE(master.excerpts().empty());
    auto *excerpt = master.excerpts().front();
    excerpt->set_file_name(u"imported_custom_name");
    ASSERT_TRUE(document->checkpoint("filename"));
    EXPECT_EQ(excerpt->file_name(), muse::String(u"imported_custom_name"));
    ASSERT_TRUE(document->export_score({.format = engrave::Format::MSCZ}));
    EXPECT_EQ(excerpt->file_name(), muse::String(u"imported_custom_name"));
    expect_inv(*document);
}
