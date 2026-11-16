/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include "engrave/src/api/batch.h"
#include "engrave/src/api/handles.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/image.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stem.h"
#include "engraving/editing/editmeasurerepeat.h"
#include "engraving/editing/transaction/transaction.h"
#include "io/fileinfo.h"
#include "utils/scorecomp.h"
#include "utils/scorerw.h"
#include "utils/testutils.h"

using namespace mu::engraving;

static const String parts_data_dir(u"parts_data/");

static void remove_public_item(EngravingItem *item) {
    ASSERT_NE(item, nullptr);
    auto &master = *item->master_score();
    engrave::assign_missing_ids(master);
    engrave::api::EditContext context(master);
    auto prepared = engrave::api::prepare(context, engrave::Remove{engrave::from_engine(item->eid())});
    ASSERT_TRUE(prepared) << prepared.error().message;
    const auto result = (*prepared)(context);
    ASSERT_TRUE(result) << result.error().message;
}

struct EngravingPartsTests : ::testing::Test {
    void test_part_creation(const String &test);
    void create_linked_staff(MasterScore *score);

    MasterScore *do_add_breath();
    MasterScore *do_remove_breath();
    MasterScore *do_add_fingering();
    MasterScore *do_remove_fingering();
    MasterScore *do_add_symbol();
    MasterScore *do_remove_symbol();
    MasterScore *do_add_chordline();
    MasterScore *do_remove_chordline();
    MasterScore *do_remove_measure_repeat();
    MasterScore *do_add_image();
    MasterScore *do_remove_image();
};

void EngravingPartsTests::create_linked_staff(MasterScore *master_score) {
    master_score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    Staff *source_staff = master_score->staff(0);
    ASSERT_TRUE(source_staff);
    Staff *linked_staff = Factory::create_staff(source_staff->part());
    linked_staff->set_part(source_staff->part());
    master_score->undo_insert_staff(linked_staff, 1, false);
    Excerpt::clone_staff(source_staff, linked_staff);
    master_score->end_cmd();
    ASSERT_TRUE(master_score->staff(1));
}

TEST_F(EngravingPartsTests, voicesExcerpt) {
    MasterScore *master_score = ScoreRW::read_score(::parts_data_dir + u"voices.mscx");
    ASSERT_TRUE(master_score);

    //
    // create first part
    //
    std::vector<Part *> parts;

    parts.push_back(master_score->parts().at(0));
    Score *nscore = master_score->create_score();
    ASSERT_TRUE(nscore);

    Excerpt *ex = new Excerpt(master_score);
    ex->set_excerpt_score(nscore);
    nscore->set_excerpt(ex);
    master_score->excerpts().push_back(ex);
    ex->set_name(parts.front()->long_name());
    ex->set_parts(parts);
    Excerpt::create_excerpt(ex);
    ex->set_voice_visible(nscore->staff(0), 0, false);

    //nscore->setName(parts.front()->partName());

    //
    // create second part
    //
    parts.clear();
    parts.push_back(master_score->parts().at(1));
    nscore = master_score->create_score();
    ASSERT_TRUE(nscore);

    ex = new Excerpt(master_score);
    ex->set_excerpt_score(nscore);
    nscore->set_excerpt(ex);
    master_score->excerpts().push_back(ex);
    ex->set_name(parts.front()->long_name());
    ex->set_parts(parts);
    Excerpt::create_excerpt(ex);
    ex->set_voice_visible(nscore->staff(0), 0, false);
    ex->set_voice_visible(nscore->staff(0), 1, false);
    ex->set_voice_visible(nscore->staff(0), 2, false);

    //
    // create second part
    //
    parts.clear();
    parts.push_back(master_score->parts().at(1));
    nscore = master_score->create_score();
    ASSERT_TRUE(nscore);

    ex = new Excerpt(master_score);
    ex->set_excerpt_score(nscore);
    nscore->set_excerpt(ex);
    master_score->excerpts().push_back(ex);
    ex->set_name(parts.front()->long_name());
    ex->set_parts(parts);
    Excerpt::create_excerpt(ex);
    ex->set_voice_visible(nscore->staff(0), 1, false);
    ex->set_voice_visible(nscore->staff(0), 2, false);
    ex->set_voice_visible(nscore->staff(0), 3, false);

    //nscore->setName(parts.front()->partName());

    master_score->set_excerpts_changed(true);

    // Compare the cloned source before layout resolves the automatic articulation
    // glyph for its remapped voice (Below becomes Above when rendered).
    ASSERT_TRUE(ScoreRW::save_source_score(master_score, u"voices.mscx"));
    EXPECT_TRUE(ScoreComp::compare_files(ScoreRW::root_path() + u"/" + ::parts_data_dir + u"voices-ref.mscx",
                                         ScoreRW::scratch_path(u"voices.mscx")));

    delete master_score;
}

void EngravingPartsTests::test_part_creation(const String &test) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + test + u".mscx");
    ASSERT_TRUE(score);

    if (muse::io::FileInfo(ScoreRW::root_path() + u"/" + ::parts_data_dir + test + u"-ref.mscx").exists()) {
        EXPECT_TRUE(ScoreComp::save_compare_score(score, test + u"-1.mscx", ::parts_data_dir + test + u"-ref.mscx"));
    } else {
        EXPECT_TRUE(ScoreComp::save_compare_score(score, test + u"-1.mscx", ::parts_data_dir + test + u".mscx"));
    }

    TestUtils::create_parts(score, 2);

    if (muse::io::FileInfo(ScoreRW::root_path() + u"/" + ::parts_data_dir + test + u"-parts-ref.mscx").exists()) {
        EXPECT_TRUE(ScoreComp::save_compare_score(score, test + u"-parts.mscx", ::parts_data_dir + test + u"-parts-ref.mscx"));
    } else {
        EXPECT_TRUE(ScoreComp::save_compare_score(score, test + u"-parts.mscx", ::parts_data_dir + test + u"-parts.mscx"));
    }

    delete score;
}

TEST_F(EngravingPartsTests, append_measure) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-all.mscx");
    ASSERT_TRUE(score);

    TestUtils::create_parts(score, 2);

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    score->insert_measure(0);
    score->end_cmd();

    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-all-appendmeasures.mscx", ::parts_data_dir + u"part-all-appendmeasures.mscx"));

    delete score;
}

TEST_F(EngravingPartsTests, insert_measure) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-all.mscx");
    ASSERT_TRUE(score);

    TestUtils::create_parts(score, 2);

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    Measure *m = score->first_measure();
    score->insert_measure(m);
    score->end_cmd();

    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-all-insertmeasures.mscx", ::parts_data_dir + u"part-all-insertmeasures.mscx"));

    delete score;
}

TEST_F(EngravingPartsTests, styleScore) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"partStyle.mscx");
    ASSERT_TRUE(score);

    TestUtils::create_parts(score, 2);
    score->style().set(Sid::CLEF_LEFT_MARGIN, 4.0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"partStyle-score-test.mscx", ::parts_data_dir + u"partStyle-score-ref.mscx"));
    delete score;
}

TEST_F(EngravingPartsTests, styleScoreReload) {
    MasterScore *part_score = ScoreRW::read_score(::parts_data_dir + u"partStyle-score-reload.mscx");
    ASSERT_TRUE(part_score);

    EXPECT_TRUE(ScoreComp::save_compare_score(part_score, u"partStyle-score-reload-test.mscx",
                                              ::parts_data_dir + u"partStyle-score-reload-ref.mscx"));
    delete part_score;
}

//   test part creation

TEST_F(EngravingPartsTests, createPart1) { test_part_creation(u"part-empty"); }

TEST_F(EngravingPartsTests, create_empty_part) {
    String source_file_name = u"part-empty";
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + source_file_name + u".mscx");
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, source_file_name + u"-1.mscx", ::parts_data_dir + source_file_name + u".mscx"));

    TestUtils::create_empty_part(score);

    // Check that measures have correct ticks set
    for (Excerpt *excerpt : score->excerpts()) {
        Score *excerpt_score = excerpt->excerpt_score();
        ASSERT_TRUE(excerpt_score);
        for (MeasureBase *mb = excerpt_score->first(); mb; mb = mb->next()) {
            if (mb->is_measure()) {
                EXPECT_GT(mb->ticks(), Fraction(0, 1));
            }
            if (mb->prev()) {
                EXPECT_EQ(mb->prev()->end_tick(), mb->tick());
            }
        }
    }
}

TEST_F(EngravingPartsTests, createPart2) { test_part_creation(u"part-all"); }

TEST_F(EngravingPartsTests, createPart3) {
    // second part has system text on empty chordrest segment
    test_part_creation(u"part-54346");
}

TEST_F(EngravingPartsTests, createPartBreath) { test_part_creation(u"part-breath"); }

MasterScore *EngravingPartsTests::do_add_breath() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-empty-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure();
    Segment *s = m->tick2segment(Fraction(1, 4));
    Chord *chord = to_chord(s->element(0));
    Note *note = chord->up_note();
    Breath *b = Factory::create_breath(score->dummy());
    b->set_sym_id(SymId::BREATH_MARK_COMMA);

    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"), [&](Transaction &) {
        b->set_track(note->track());
        b->set_pos(PointF());
        b->set_placement(b->track() & 1 ? PlacementV::BELOW : PlacementV::ABOVE);
        b->set_ownership_parent(m->undo_get_segment(SegmentType::BREATH, chord->end_tick()));
        score->undo_add_element(b);
    }); // does layout

    return score;
}

TEST_F(EngravingPartsTests, add_breath) {
    MasterScore *score = do_add_breath();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-breath-add.mscx", ::parts_data_dir + u"part-breath-add.mscx"));
    delete score;
}

MasterScore *EngravingPartsTests::do_remove_breath() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-breath-add.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure();
    Segment *s = m->first()->next(SegmentType::BREATH);
    Breath *b = to_breath(s->element(0));

    score->select(b);
    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    remove_public_item(score->selection().element());
    score->set_layout_all();
    score->end_cmd();
    return score;
}

TEST_F(EngravingPartsTests, removeBreath) {
    MasterScore *score = do_remove_breath();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-breath-del.mscx", ::parts_data_dir + u"part-breath-del.mscx"));
    delete score;
}

TEST_F(EngravingPartsTests, createPartFingering) { test_part_creation(u"part-fingering"); }

MasterScore *EngravingPartsTests::do_add_fingering() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-empty-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure();
    Segment *s = m->tick2segment(Fraction(1, 4));
    Chord *chord = to_chord(s->element(0));
    Note *note = chord->up_note();
    Fingering *b = Factory::create_fingering(note);
    b->set_xml_text("3");

    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"), [&](Transaction &) {
        b->set_track(note->track());
        b->set_ownership_parent(note);
        score->undo_add_element(b);
    }); // does layout
    return score;
}

TEST_F(EngravingPartsTests, add_fingering) {
    MasterScore *score = do_add_fingering();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-fingering-add.mscx", ::parts_data_dir + u"part-fingering-add.mscx"));
    delete score;
}

MasterScore *EngravingPartsTests::do_remove_fingering() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-fingering-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure();
    Segment *s = m->first()->next(SegmentType::CHORD_REST);
    Chord *chord = to_chord(s->element(0));
    Note *note = chord->up_note();
    EngravingItem *fingering = 0;
    for (EngravingItem *e : note->el()) {
        if (e->is_fingering()) {
            fingering = e;
            break;
        }
    }
    score->select(fingering);

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    remove_public_item(score->selection().element());
    score->set_layout_all();
    score->end_cmd();
    return score;
}

TEST_F(EngravingPartsTests, removeFingering) {
    MasterScore *score = do_remove_fingering();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-fingering-del.mscx", ::parts_data_dir + u"part-fingering-del.mscx"));
    delete score;
}

TEST_F(EngravingPartsTests, createPartSymbol) { test_part_creation(u"part-symbol"); }

MasterScore *EngravingPartsTests::do_add_symbol() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-empty-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure();
    Segment *s = m->tick2segment(Fraction(1, 4));
    Chord *chord = to_chord(s->element(0));
    Note *note = chord->up_note();
    Symbol *b = new Symbol(note);
    b->set_sym(SymId::G_CLEF);

    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"), [&](Transaction &) {
        b->set_track(note->track());
        b->set_ownership_parent(note);
        score->undo_add_element(b);
    }); // does layout
    return score;
}

TEST_F(EngravingPartsTests, addSymbol) {
    MasterScore *score = do_add_symbol();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-symbol-add.mscx", ::parts_data_dir + u"part-symbol-add.mscx"));
    delete score;
}

MasterScore *EngravingPartsTests::do_remove_symbol() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-symbol-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure();
    Segment *s = m->first()->next(SegmentType::CHORD_REST);
    Chord *chord = to_chord(s->element(0));
    Note *note = chord->up_note();
    EngravingItem *se = 0;
    for (EngravingItem *e : note->el()) {
        if (e->is_symbol()) {
            se = e;
            break;
        }
    }
    score->select(se);

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    remove_public_item(score->selection().element());
    score->set_layout_all();
    score->end_cmd();
    return score;
}

TEST_F(EngravingPartsTests, removeSymbol) {
    MasterScore *score = do_remove_symbol();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-symbol-del.mscx", ::parts_data_dir + u"part-symbol-del.mscx"));
    delete score;
}

TEST_F(EngravingPartsTests, createPartChordline) { test_part_creation(u"part-chordline"); }

MasterScore *EngravingPartsTests::do_add_chordline() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-empty-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure();
    Segment *s = m->tick2segment(Fraction(1, 4));
    Chord *chord = to_chord(s->element(0));
    Note *note = chord->up_note();
    ChordLine *b = Factory::create_chord_line(chord);
    b->set_chord_line_type(ChordLineType::FALL);

    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"), [&](Transaction &) {
        b->set_track(note->track());
        b->set_note(note);
        b->set_ownership_parent(chord);
        score->undo_add_element(b);
    }); // does layout
    return score;
}

TEST_F(EngravingPartsTests, addChordline) {
    MasterScore *score = do_add_chordline();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-chordline-add.mscx", ::parts_data_dir + u"part-chordline-add.mscx"));
    delete score;
}

MasterScore *EngravingPartsTests::do_remove_chordline() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-chordline-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure();
    Segment *s = m->first()->next(SegmentType::CHORD_REST);
    Chord *chord = to_chord(s->element(0));

    EngravingItem *se = 0;
    for (EngravingItem *e : chord->el()) {
        if (e->is_chord_line()) {
            se = e;
            break;
        }
    }
    score->select(se);

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    remove_public_item(score->selection().element());
    score->set_layout_all();
    score->end_cmd();
    return score;
}

TEST_F(EngravingPartsTests, removeChordline) {
    MasterScore *score = do_remove_chordline();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-chordline-del.mscx", ::parts_data_dir + u"part-chordline-del.mscx"));
    delete score;
}

TEST_F(EngravingPartsTests, createPartMeasureRepeat) { test_part_creation(u"part-measure-repeat"); }

MasterScore *EngravingPartsTests::do_remove_measure_repeat() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-measure-repeat-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure()->next_measure()->next_measure();
    MeasureRepeat *mr = m->measure_repeat_element(0);
    score->select(mr);

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    remove_public_item(score->selection().element());
    score->set_layout_all();
    score->end_cmd();
    return score;
}

TEST_F(EngravingPartsTests, removeMeasureRepeat) {
    MasterScore *score = do_remove_measure_repeat();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-measure-repeat-del.mscx", ::parts_data_dir + u"part-measure-repeat-del.mscx"));
    delete score;
}

TEST_F(EngravingPartsTests, createPartStemless) { test_part_creation(u"part-stemless"); }

TEST_F(EngravingPartsTests, partExclusion) {
    MasterScore *master_score = ScoreRW::read_score(::parts_data_dir + u"partExclusion.mscx");

    ASSERT_TRUE(master_score);

    Score *part_score = TestUtils::create_part(master_score);
    ASSERT_TRUE(part_score);

    ScoreRW::save_score(master_score, u"partExclusion.mscx");
    EXPECT_TRUE(ScoreComp::save_compare_score(part_score, u"partExclusion-part-0.mscx", ::parts_data_dir + u"partExclusion-part-0.mscx"));

    // Collect the relevant items
    std::vector<EngravingItem *> items_to_exclude;
    for (MeasureBase *mb = master_score->first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) {
            items_to_exclude.push_back(mb);
            continue;
        }
        for (Segment &segment : to_measure(mb)->segments()) {
            EngravingItem *item = segment.element(0);
            if (item && item->is_clef() && !item->generated()) {
                items_to_exclude.push_back(item);
            }
            for (EngravingItem *annotation : segment.annotations()) {
                if (annotation->is_text_base()) {
                    items_to_exclude.push_back(annotation);
                }
            }
        }
    }
    for (auto pair : master_score->spanner()) {
        Spanner *spanner = pair.second;
        if (spanner && spanner->is_ottava()) {
            items_to_exclude.push_back(spanner);
        }
    }

    // Invert exclusion property
    for (EngravingItem *item : items_to_exclude) {
        master_score->select(item);
        bool exclude = item->get_property(Pid::EXCLUDE_FROM_OTHER_PARTS).to_bool();
        item->undo_change_property(Pid::EXCLUDE_FROM_OTHER_PARTS, !exclude);
    }

    // Excluded items are not in the part anymore and viceversa
    EXPECT_TRUE(ScoreComp::save_compare_score(part_score, u"partExclusion-part-1.mscx", ::parts_data_dir + u"partExclusion-part-1.mscx"));

    // Revert again and check it is equal to the initial file again
    for (EngravingItem *item : items_to_exclude) {
        master_score->select(item);
        bool exclude = item->get_property(Pid::EXCLUDE_FROM_OTHER_PARTS).to_bool();
        item->undo_change_property(Pid::EXCLUDE_FROM_OTHER_PARTS, !exclude);
    }

    // Not applicable anymore because creating new elements creates new EIDs too
    //EXPECT_TRUE(ScoreComp::saveCompareScore(partScore, u"partExclusion-part-0.mscx", PARTS_DATA_DIR + u"partExclusion-part-0.mscx"));
}

TEST_F(EngravingPartsTests, partPropertyLinking) {
    MasterScore *master_score = ScoreRW::read_score(::parts_data_dir + u"partPropertyLinking.mscx");

    ASSERT_TRUE(master_score);

    Score *part_score = TestUtils::create_part(master_score);
    ASSERT_TRUE(part_score);

    ScoreRW::save_score(master_score, u"partPropertyLinking.mscx");
    EXPECT_TRUE(ScoreComp::save_compare_score(part_score, u"partPropertyLinking-part-0.mscx",
                                              ::parts_data_dir + u"partPropertyLinking-part-0.mscx"));

    Dynamic *dynamic = nullptr;
    Measure *measure = master_score->first_measure();
    for (Segment &segment : measure->segments()) {
        for (EngravingItem *annotation : segment.annotations()) {
            if (annotation->is_dynamic()) {
                dynamic = to_dynamic(annotation);
                break;
            }
        }
    }
    Dynamic *test_item = to_dynamic(dynamic->find_linked_in_score(part_score));
    ASSERT_TRUE(test_item);

    test_item->undo_change_property(Pid::PLACEMENT, PropertyValue::from_value(PlacementV::ABOVE), PropertyFlags::NOSTYLE);
    test_item->undo_change_property(Pid::MUSICAL_SYMBOLS_SCALE, PropertyValue::from_value(1.2), PropertyFlags::NOSTYLE);
    EXPECT_FALSE(test_item->is_position_linked_to_master());
    EXPECT_FALSE(test_item->is_appearance_linked_to_master());

    EXPECT_TRUE(ScoreComp::save_compare_score(part_score, u"partPropertyLinking-part-1.mscx",
                                              ::parts_data_dir + u"partPropertyLinking-part-1.mscx"));

    test_item->undo_change_property(Pid::POSITION_LINKED_TO_MASTER, true, PropertyFlags::NOSTYLE);
    test_item->undo_change_property(Pid::APPEARANCE_LINKED_TO_MASTER, true, PropertyFlags::NOSTYLE);
    EXPECT_TRUE(ScoreComp::save_compare_score(part_score, u"partPropertyLinking-part-0.mscx",
                                              ::parts_data_dir + u"partPropertyLinking-part-0.mscx"));
}

TEST_F(EngravingPartsTests, partSpanners) { test_part_creation(u"part-spanners"); }

TEST_F(EngravingPartsTests, partTies) {
    const String test = u"linked-ties";
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + test + u".mscx");
    ASSERT_TRUE(score);
    create_linked_staff(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, test + u"-1.mscx", ::parts_data_dir + test + u"-1.mscx"));
    TestUtils::create_part(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, test + u"-parts.mscx", ::parts_data_dir + test + u"-parts.mscx"));
    delete score;
}

TEST_F(EngravingPartsTests, inputFromParts) {
    // Enter notes *in parts* and check that they are correctly cloned to the score.

    Score *score = ScoreRW::read_score(::parts_data_dir + u"input-from-parts.mscz");
    ASSERT_TRUE(score);
    staff_idx_t flute_staff = 0;
    staff_idx_t oboe_staff = 1;
    staff_idx_t clarinet_staff = 2;
    staff_idx_t bassoon_staff = 3;

    Score *flute_part = nullptr;
    Score *oboe_part = nullptr;
    Score *clarinet_part = nullptr;
    Score *bassoon_part = nullptr;
    for (Score *part : score->score_list()) {
        String part_name = part->name();
        if (part_name == u"Flute") {
            flute_part = part;
        } else if (part_name == u"Oboe") {
            oboe_part = part;
        } else if (part_name == u"Clarinet in B♭") {
            clarinet_part = part;
        } else if (part_name == u"Bassoon") {
            bassoon_part = part;
        }
    }
    ASSERT_TRUE(flute_part);
    ASSERT_TRUE(oboe_part);
    ASSERT_TRUE(clarinet_part);
    ASSERT_TRUE(bassoon_part);

    track_idx_t voice = 3;
    Segment *part_segment = flute_part->first_measure()->find_first_r(SegmentType::CHORD_REST, Fraction(0, 1));
    ASSERT_TRUE(part_segment);
    flute_part->set_note_rest(part_segment, voice, NoteVal(60), Fraction(1, 1));
    Segment *score_segment = score->tick2segment(part_segment->tick(), true, SegmentType::CHORD_REST);
    ASSERT_TRUE(score_segment);
    Chord *chord = to_chord(score_segment->element(staff2track(flute_staff) + voice));
    ASSERT_TRUE(chord);

    voice = 2;
    part_segment = oboe_part->first_measure()->next_measure()->find_first_r(SegmentType::CHORD_REST, Fraction(0, 1));
    ASSERT_TRUE(part_segment);
    oboe_part->set_note_rest(part_segment, voice, NoteVal(60), Fraction(1, 1));
    score_segment = score->tick2segment(part_segment->tick(), true, SegmentType::CHORD_REST);
    ASSERT_TRUE(score_segment);
    chord = to_chord(score_segment->element(staff2track(oboe_staff) + voice));
    ASSERT_TRUE(chord);

    voice = 1;
    part_segment = clarinet_part->first_measure()->next_measure()->next_measure()->find_first_r(SegmentType::CHORD_REST, Fraction(0, 1));
    ASSERT_TRUE(part_segment);
    clarinet_part->set_note_rest(part_segment, voice, NoteVal(60), Fraction(1, 1));
    score_segment = score->tick2segment(part_segment->tick(), true, SegmentType::CHORD_REST);
    ASSERT_TRUE(score_segment);
    chord = to_chord(score_segment->element(staff2track(clarinet_staff) + voice));
    ASSERT_TRUE(chord);

    voice = 0;
    part_segment = bassoon_part->first_measure()->next_measure()->next_measure()->next_measure()->find_first_r(SegmentType::CHORD_REST,
                                                                                                               Fraction(0, 1));
    ASSERT_TRUE(part_segment);
    bassoon_part->set_note_rest(part_segment, voice, NoteVal(60), Fraction(1, 1));
    score_segment = score->tick2segment(part_segment->tick(), true, SegmentType::CHORD_REST);
    ASSERT_TRUE(score_segment);
    chord = to_chord(score_segment->element(staff2track(bassoon_staff) + voice));
    ASSERT_TRUE(chord);
}

TEST_F(EngravingPartsTests, crossMeasureUpwardGlissExcerpt) {
    MasterScore *master = ScoreRW::read_score(::parts_data_dir + u"cross-measure-gliss.mscx");
    ASSERT_TRUE(master);
    const Note *original = to_chord(master->first_measure()->first_chord_rest(4))->up_note();
    ASSERT_EQ(original->pitch(), 48);
    ASSERT_EQ(original->spanner_for().size(), 1u);
    ASSERT_EQ(to_note(original->spanner_for().front()->end_element())->pitch(), 84);
    ASSERT_EQ(original->spanner_for().front()->end_element()->find_measure(), master->first_measure()->next_measure());
    ASSERT_EQ(original->spanner_for().front()->end_element()->track(), 0u);
    Score *part = TestUtils::create_part(master, 0);
    ASSERT_TRUE(part);
    part->do_layout();
    const Note *cloned = to_chord(part->first_measure()->first_chord_rest(4))->up_note();
    ASSERT_EQ(cloned->spanner_for().size(), 1u);
    EXPECT_EQ(cloned->spanner_for().front()->end_element()->track(), 0u);
    EXPECT_EQ(cloned->pitch(), 48);
    EXPECT_EQ(cloned->spanner_for().front()->start_element(), cloned);
    EXPECT_EQ(cloned->spanner_for().front()->end_element()->score(), part);
    EXPECT_EQ(cloned->spanner_for().front()->end_element()->find_measure(), part->first_measure()->next_measure());
    EXPECT_EQ(to_note(cloned->spanner_for().front()->end_element())->pitch(), 84);
    delete master;
}

TEST_F(EngravingPartsTests, rejects_wrong_external_link) {
    const auto root = ScoreRW::root_path() + u"/../../../tests/data/";
    EXPECT_FALSE(ScoreComp::compare_files(root + u"external-link-expected.xml", root + u"external-link-wrong.xml"));
}
