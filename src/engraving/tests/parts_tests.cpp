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

#include "engraving/dom/beam.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
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
#include "engraving/editing/editvisibility.h"
#include "engraving/editing/editvoice.h"
#include "engraving/editing/transaction/transaction.h"
#include "io/fileinfo.h"
#include "utils/scorecomp.h"
#include "utils/scorerw.h"
#include "utils/testutils.h"

using namespace mu::engraving;

static const String parts_data_dir(u"parts_data/");

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
    MasterScore *do_add_measure_repeat();
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

//---------------------------------------------------------
//   voicesExcerpt
//---------------------------------------------------------

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

    EXPECT_TRUE(ScoreComp::save_compare_score(master_score, u"voices.mscx", ::parts_data_dir + u"voices-ref.mscx"));

    delete master_score;
}

//---------------------------------------------------------
//   testPartCreation
//---------------------------------------------------------

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

//---------------------------------------------------------
//   testAppendMeasure
//---------------------------------------------------------

TEST_F(EngravingPartsTests, append_measure) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-all.mscx");
    ASSERT_TRUE(score);

    TestUtils::create_parts(score, 2);

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    score->insert_measure(0);
    score->end_cmd();

    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-all-appendmeasures.mscx", ::parts_data_dir + u"part-all-appendmeasures.mscx"));

    score->undo_redo(true, 0);

    EXPECT_TRUE(
        ScoreComp::save_compare_score(score, u"part-all-uappendmeasures.mscx", ::parts_data_dir + u"part-all-uappendmeasures.mscx"));
    delete score;
}

//---------------------------------------------------------
//   testInsertMeasure
//---------------------------------------------------------

TEST_F(EngravingPartsTests, insert_measure) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-all.mscx");
    ASSERT_TRUE(score);

    TestUtils::create_parts(score, 2);

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    Measure *m = score->first_measure();
    score->insert_measure(m);
    score->end_cmd();

    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-all-insertmeasures.mscx", ::parts_data_dir + u"part-all-insertmeasures.mscx"));

    score->undo_redo(true, 0);

    EXPECT_TRUE(
        ScoreComp::save_compare_score(score, u"part-all-uinsertmeasures.mscx", ::parts_data_dir + u"part-all-uinsertmeasures.mscx"));
    delete score;
}

//---------------------------------------------------------
//   styleScore
//---------------------------------------------------------

TEST_F(EngravingPartsTests, styleScore) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"partStyle.mscx");
    ASSERT_TRUE(score);

    TestUtils::create_parts(score, 2);
    score->style().set(Sid::CLEF_LEFT_MARGIN, 4.0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"partStyle-score-test.mscx", ::parts_data_dir + u"partStyle-score-ref.mscx"));
    delete score;
}

//---------------------------------------------------------
//   styleScoreReload
//---------------------------------------------------------

TEST_F(EngravingPartsTests, styleScoreReload) {
    MasterScore *part_score = ScoreRW::read_score(::parts_data_dir + u"partStyle-score-reload.mscx");
    ASSERT_TRUE(part_score);

    EXPECT_TRUE(ScoreComp::save_compare_score(part_score, u"partStyle-score-reload-test.mscx",
                                              ::parts_data_dir + u"partStyle-score-reload-ref.mscx"));
    delete part_score;
}

//---------------------------------------------------------
//   test part creation
//---------------------------------------------------------

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

//---------------------------------------------------------
//    Breath
//---------------------------------------------------------

TEST_F(EngravingPartsTests, createPartBreath) { test_part_creation(u"part-breath"); }

//---------------------------------------------------------
//    doAddBreath
//---------------------------------------------------------

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
    EditData dd;
    Breath *b = Factory::create_breath(score->dummy());
    b->set_sym_id(SymId::BREATH_MARK_COMMA);
    dd.drop_element = b;

    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"),
                                              [&](Transaction &tx) { note->drop(tx, dd); }); // does layout

    return score;
}

//---------------------------------------------------------
//   addBreath
//---------------------------------------------------------

TEST_F(EngravingPartsTests, add_breath) {
    MasterScore *score = do_add_breath();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-breath-add.mscx", ::parts_data_dir + u"part-breath-add.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoAddBreath
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoAddBreath) {
    MasterScore *score = do_add_breath();
    ASSERT_TRUE(score);

    score->undo_redo(true, 0);

    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-breath-uadd.mscx", ::parts_data_dir + u"part-breath-uadd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoAddBreath
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoAddBreath) {
    MasterScore *score = do_add_breath();
    ASSERT_TRUE(score);

    score->undo_redo(true, 0);
    score->undo_redo(false, 0);

    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-breath-uradd.mscx", ::parts_data_dir + u"part-breath-uradd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   doRemoveBreath
//---------------------------------------------------------

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
    score->cmd_delete_selection();
    score->set_layout_all();
    score->end_cmd();
    return score;
}

//---------------------------------------------------------
//   removeBreath
//---------------------------------------------------------

TEST_F(EngravingPartsTests, removeBreath) {
    MasterScore *score = do_remove_breath();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-breath-del.mscx", ::parts_data_dir + u"part-breath-del.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRemoveBreath
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRemoveBreath) {
    MasterScore *score = do_remove_breath();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-breath-udel.mscx", ::parts_data_dir + u"part-breath-udel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoRemoveBreath
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoRemoveBreath) {
    MasterScore *score = do_remove_breath();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    score->undo_redo(false, 0);

    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-breath-urdel.mscx", ::parts_data_dir + u"part-breath-urdel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   Fingering
//---------------------------------------------------------

TEST_F(EngravingPartsTests, createPartFingering) { test_part_creation(u"part-fingering"); }

//---------------------------------------------------------
//   doAddFingering
//---------------------------------------------------------

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
    EditData dd;
    Fingering *b = Factory::create_fingering(note);
    b->set_xml_text("3");
    dd.drop_element = b;

    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"),
                                              [&](Transaction &tx) { note->drop(tx, dd); }); // does layout
    return score;
}

//---------------------------------------------------------
//   addFingering
//---------------------------------------------------------

TEST_F(EngravingPartsTests, add_fingering) {
    MasterScore *score = do_add_fingering();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-fingering-add.mscx", ::parts_data_dir + u"part-fingering-add.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoAddFingering
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoAddFingering) {
    MasterScore *score = do_add_fingering();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-fingering-uadd.mscx", ::parts_data_dir + u"part-fingering-uadd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoAddFingering
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoAddFingering) {
    MasterScore *score = do_add_fingering();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    score->undo_redo(false, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-fingering-uradd.mscx", ::parts_data_dir + u"part-fingering-uradd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   doRemoveFingering
//---------------------------------------------------------

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
    score->cmd_delete_selection();
    score->set_layout_all();
    score->end_cmd();
    return score;
}

//---------------------------------------------------------
//   removeFingering
//---------------------------------------------------------

TEST_F(EngravingPartsTests, removeFingering) {
    MasterScore *score = do_remove_fingering();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-fingering-del.mscx", ::parts_data_dir + u"part-fingering-del.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRemoveFingering
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRemoveFingering) {
    MasterScore *score = do_remove_fingering();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-fingering-udel.mscx", ::parts_data_dir + u"part-fingering-udel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoRemoveFingering
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoRemoveFingering) {
    MasterScore *score = do_remove_fingering();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    score->undo_redo(false, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-fingering-urdel.mscx", ::parts_data_dir + u"part-fingering-urdel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   Symbol
//---------------------------------------------------------

TEST_F(EngravingPartsTests, createPartSymbol) { test_part_creation(u"part-symbol"); }

//---------------------------------------------------------
//   doAddSymbol
//---------------------------------------------------------

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
    EditData dd;
    Symbol *b = new Symbol(note);
    b->set_sym(SymId::G_CLEF);
    dd.drop_element = b;

    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"),
                                              [&](Transaction &tx) { note->drop(tx, dd); }); // does layout
    return score;
}

//---------------------------------------------------------
//   addSymbol
//---------------------------------------------------------

TEST_F(EngravingPartsTests, addSymbol) {
    MasterScore *score = do_add_symbol();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-symbol-add.mscx", ::parts_data_dir + u"part-symbol-add.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoAddSymbol
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoAddSymbol) {
    MasterScore *score = do_add_symbol();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-symbol-uadd.mscx", ::parts_data_dir + u"part-symbol-uadd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoAddSymbol
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoAddSymbol) {
    MasterScore *score = do_add_symbol();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    score->undo_redo(false, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-symbol-uradd.mscx", ::parts_data_dir + u"part-symbol-uradd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   doRemoveSymbol
//---------------------------------------------------------

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
    score->cmd_delete_selection();
    score->set_layout_all();
    score->end_cmd();
    return score;
}

//---------------------------------------------------------
//   removeSymbol
//---------------------------------------------------------

TEST_F(EngravingPartsTests, removeSymbol) {
    MasterScore *score = do_remove_symbol();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-symbol-del.mscx", ::parts_data_dir + u"part-symbol-del.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRemoveSymbol
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRemoveSymbol) {
    MasterScore *score = do_remove_symbol();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-symbol-udel.mscx", ::parts_data_dir + u"part-symbol-udel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoRemoveSymbol
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoRemoveSymbol) {
    MasterScore *score = do_remove_symbol();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    score->undo_redo(false, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-symbol-urdel.mscx", ::parts_data_dir + u"part-symbol-urdel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   Chordline
//---------------------------------------------------------

TEST_F(EngravingPartsTests, createPartChordline) { test_part_creation(u"part-chordline"); }

//---------------------------------------------------------
//   doAddChordline
//---------------------------------------------------------

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
    EditData dd;
    ChordLine *b = Factory::create_chord_line(chord);
    b->set_chord_line_type(ChordLineType::FALL);
    dd.drop_element = b;

    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"),
                                              [&](Transaction &tx) { note->drop(tx, dd); }); // does layout
    return score;
}

//---------------------------------------------------------
//   addChordline
//---------------------------------------------------------

TEST_F(EngravingPartsTests, addChordline) {
    MasterScore *score = do_add_chordline();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-chordline-add.mscx", ::parts_data_dir + u"part-chordline-add.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoAddChordline
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoAddChordline) {
    MasterScore *score = do_add_chordline();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-chordline-uadd.mscx", ::parts_data_dir + u"part-chordline-uadd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoAddChordline
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoAddChordline) {
    MasterScore *score = do_add_chordline();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    score->undo_redo(false, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-chordline-uradd.mscx", ::parts_data_dir + u"part-chordline-uradd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   doRemoveChordline
//---------------------------------------------------------

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
    score->cmd_delete_selection();
    score->set_layout_all();
    score->end_cmd();
    return score;
}

//---------------------------------------------------------
//   removeChordline
//---------------------------------------------------------

TEST_F(EngravingPartsTests, removeChordline) {
    MasterScore *score = do_remove_chordline();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-chordline-del.mscx", ::parts_data_dir + u"part-chordline-del.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRemoveChordline
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRemoveChordline) {
    MasterScore *score = do_remove_chordline();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-chordline-udel.mscx", ::parts_data_dir + u"part-chordline-udel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoRemoveChordline
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoRemoveChordline) {
    MasterScore *score = do_remove_chordline();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    score->undo_redo(false, 0);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-chordline-urdel.mscx", ::parts_data_dir + u"part-chordline-urdel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   MeasureRepeat
//---------------------------------------------------------

TEST_F(EngravingPartsTests, createPartMeasureRepeat) { test_part_creation(u"part-measure-repeat"); }

//---------------------------------------------------------
//   doAddMeasureRepeat
//---------------------------------------------------------

MasterScore *EngravingPartsTests::do_add_measure_repeat() {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-empty-parts.mscx");
    if (!score) {
        ADD_FAILURE() << "score is null";
        return nullptr;
    }

    Measure *m = score->first_measure()->next_measure();

    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    EditMeasureRepeat::add_measure_repeat(score->transaction_manager()->current_or_dummy_transaction(), score, m, 4,
                                          0); // test with 4-measure repeat in first staff
    score->set_layout_all();
    score->end_cmd();

    return score;
}

//---------------------------------------------------------
//   addMeasureRepeat
//---------------------------------------------------------

TEST_F(EngravingPartsTests, add_measure_repeat) {
    MasterScore *score = do_add_measure_repeat();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-measure-repeat-add.mscx", ::parts_data_dir + u"part-measure-repeat-add.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoAddMeasureRepeat
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoAddMeasureRepeat) {
    MasterScore *score = do_add_measure_repeat();
    ASSERT_TRUE(score);

    score->undo_redo(true, 0);

    EXPECT_TRUE(
        ScoreComp::save_compare_score(score, u"part-measure-repeat-uadd.mscx", ::parts_data_dir + u"part-measure-repeat-uadd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoAddMeasureRepeat
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoAddMeasureRepeat) {
    MasterScore *score = do_add_measure_repeat();
    ASSERT_TRUE(score);

    score->undo_redo(true, 0);
    score->undo_redo(false, 0);

    EXPECT_TRUE(
        ScoreComp::save_compare_score(score, u"part-measure-repeat-uradd.mscx", ::parts_data_dir + u"part-measure-repeat-uradd.mscx"));
    delete score;
}

//---------------------------------------------------------
//   doRemoveMeasureRepeat
//---------------------------------------------------------

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
    score->cmd_delete_selection();
    score->set_layout_all();
    score->end_cmd();
    return score;
}

//---------------------------------------------------------
//   removeMeasureRepeat
//---------------------------------------------------------

TEST_F(EngravingPartsTests, removeMeasureRepeat) {
    MasterScore *score = do_remove_measure_repeat();
    ASSERT_TRUE(score);
    EXPECT_TRUE(ScoreComp::save_compare_score(score, u"part-measure-repeat-del.mscx", ::parts_data_dir + u"part-measure-repeat-del.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRemoveMeasureRepeat
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRemoveMeasureRepeat) {
    MasterScore *score = do_remove_measure_repeat();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    EXPECT_TRUE(
        ScoreComp::save_compare_score(score, u"part-measure-repeat-udel.mscx", ::parts_data_dir + u"part-measure-repeat-udel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   undoRedoRemoveMeasureRepeat
//---------------------------------------------------------

TEST_F(EngravingPartsTests, undoRedoRemoveMeasureRepeat) {
    MasterScore *score = do_remove_measure_repeat();
    ASSERT_TRUE(score);
    score->undo_redo(true, 0);
    score->undo_redo(false, 0);

    EXPECT_TRUE(
        ScoreComp::save_compare_score(score, u"part-measure-repeat-urdel.mscx", ::parts_data_dir + u"part-measure-repeat-urdel.mscx"));
    delete score;
}

//---------------------------------------------------------
//   Stemless
//---------------------------------------------------------

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

TEST_F(EngravingPartsTests, partVisibleTracks) {
    Score *score = ScoreRW::read_score(::parts_data_dir + u"part-visible-tracks.mscz");
    ASSERT_TRUE(score);

    Score *part = nullptr;
    for (Score *s : score->score_list()) {
        if (!s->is_master()) {
            part = s;
            break;
        }
    }
    ASSERT_TRUE(part);
    Measure *m = part->first_measure();
    ASSERT_TRUE(m);
    Chord *c = m->find_chord(Fraction(0, 1), 0);
    ASSERT_TRUE(c);
    Note *n = c->down_note();
    ASSERT_TRUE(n);

    part->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    part->select(n);
    EditVoice::change_selected_elements_voice(part->transaction_manager()->current_or_dummy_transaction(), part, 1);
    part->end_cmd();

    EXPECT_TRUE(
        ScoreComp::save_compare_score(score, u"part-visible-tracks-score.mscx", ::parts_data_dir + u"part-visible-tracks-score-ref.mscx"));

    EXPECT_TRUE(
        ScoreComp::save_compare_score(part, u"part-visible-tracks-part.mscx", ::parts_data_dir + u"part-visible-tracks-part-ref.mscx"));
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

//---------------------------------------------------------
//   noteVisibilityBeams
//   stems and beams follow each score's own note visibility (#31928)
//---------------------------------------------------------

static std::vector<Chord *> first_measure_chords(Score *score, track_idx_t track) {
    std::vector<Chord *> chords;
    for (Segment *s = score->first_measure()->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
        if (s->element(track) && s->element(track)->is_chord()) {
            chords.push_back(to_chord(s->element(track)));
        }
    }
    return chords;
}

static void set_note_visibility(Score *score, track_idx_t track, bool visible) {
    score->transaction_manager()->transaction(TranslatableString::untranslatable("Engraving parts tests"), [&](Transaction &tx) {
        for (Chord *chord : first_measure_chords(score, track)) {
            for (Note *note : chord->notes()) {
                EditVisibility::undo_change_visible(tx, note, visible);
            }
        }
    });
}

static void expect_beamed_chords_visible(Score *score, track_idx_t track, bool visible) {
    const std::vector<Chord *> chords = first_measure_chords(score, track);
    ASSERT_EQ(chords.size(), 4u);
    for (const Chord *chord : chords) {
        EXPECT_EQ(chord->notes().front()->visible(), visible);
        ASSERT_TRUE(chord->stem());
        EXPECT_EQ(chord->stem()->visible(), visible);
        ASSERT_TRUE(chord->beam());
        EXPECT_EQ(chord->beam()->visible(), visible);
    }
}

TEST_F(EngravingPartsTests, noteVisibilityBeams) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-visibility-beams.mscx");
    ASSERT_TRUE(score);
    Score *part = TestUtils::create_part(score);
    ASSERT_TRUE(part);

    // hide the notes in the score, then show them in the part
    set_note_visibility(score, 0, false);
    set_note_visibility(part, 0, true);
    expect_beamed_chords_visible(score, 0, false);
    expect_beamed_chords_visible(part, 0, true);

    // hiding them in the score again leaves the part's stems and beams visible
    set_note_visibility(score, 0, false);
    expect_beamed_chords_visible(score, 0, false);
    expect_beamed_chords_visible(part, 0, true);

    delete score;
}

TEST_F(EngravingPartsTests, noteVisibilityBeamsTwoParts) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-visibility-beams.mscx");
    ASSERT_TRUE(score);
    Score *part1 = TestUtils::create_part(score);
    Score *part2 = TestUtils::create_part(score);
    ASSERT_TRUE(part1 && part2);

    // showing the notes in one part leaves the other part alone
    set_note_visibility(score, 0, false);
    set_note_visibility(part1, 0, true);
    expect_beamed_chords_visible(score, 0, false);
    expect_beamed_chords_visible(part1, 0, true);
    expect_beamed_chords_visible(part2, 0, false);

    // and so does hiding them again
    set_note_visibility(part2, 0, true);
    set_note_visibility(part1, 0, false);
    expect_beamed_chords_visible(score, 0, false);
    expect_beamed_chords_visible(part1, 0, false);
    expect_beamed_chords_visible(part2, 0, true);

    delete score;
}

TEST_F(EngravingPartsTests, noteVisibilityBeamsVoiceParts) {
    MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"part-visibility-beams.mscx");
    ASSERT_TRUE(score);
    Score *voice1_part = TestUtils::create_part(score);
    Score *voice2_part = TestUtils::create_part(score);
    ASSERT_TRUE(voice1_part && voice2_part);

    // one part per voice
    score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
    score->excerpts().at(0)->set_voice_visible(voice1_part->staff(0), 1, false);
    score->excerpts().at(1)->set_voice_visible(voice2_part->staff(0), 0, false);
    score->end_cmd();
    voice1_part->do_layout();
    voice2_part->do_layout();
    ASSERT_TRUE(first_measure_chords(voice1_part, 1).empty());
    ASSERT_EQ(first_measure_chords(voice2_part, 0).at(0)->notes().front()->pitch(), 67);

    // hide voice 2 in the score, then show it in its own part
    set_note_visibility(score, 1, false);
    set_note_visibility(voice2_part, 0, true);
    expect_beamed_chords_visible(score, 0, true);
    expect_beamed_chords_visible(score, 1, false);
    expect_beamed_chords_visible(voice1_part, 0, true);
    expect_beamed_chords_visible(voice2_part, 0, true);

    // hide voice 2 in the score again
    set_note_visibility(score, 1, false);
    expect_beamed_chords_visible(score, 1, false);
    expect_beamed_chords_visible(voice2_part, 0, true);

    delete score;
}

//---------------------------------------------------------
//   staffStyles
//---------------------------------------------------------

TEST_F(EngravingPartsTests, deleteLinkedRestsRemovesEmptySegments) {
    for (bool from_part : {false, true}) {
        SCOPED_TRACE(from_part);
        MasterScore *score = ScoreRW::read_score(::parts_data_dir + u"delete-linked-rests.mscx");
        ASSERT_TRUE(score);
        Score *part = TestUtils::create_part(score);
        ASSERT_TRUE(part);
        auto check = [&](size_t expected) {
            for (Score *s : {static_cast<Score *>(score), part}) {
                size_t count = 0;
                for (Segment *segment = s->first_measure()->first(SegmentType::CHORD_REST); segment;
                     segment = segment->next(SegmentType::CHORD_REST)) {
                    EXPECT_FALSE(segment->empty());
                    EngravingItem *item = segment->element(0);
                    EXPECT_TRUE(item && item->is_rest());
                    if (item && item->is_rest()) {
                        EXPECT_EQ(to_rest(item)->ticks(), expected == 1 ? Fraction(1, 1) : Fraction(1, 4));
                    }
                    ++count;
                }
                EXPECT_EQ(count, expected);
            }
        };
        check(4);
        Score *owner = from_part ? part : score;
        score->start_cmd(TranslatableString::untranslatable("Engraving parts tests"));
        owner->select(owner->first_measure(), SelectType::SINGLE, 0);
        owner->cmd_delete_selection();
        score->end_cmd();
        check(1);
        score->undo_redo(true, nullptr);
        check(4);
        score->undo_redo(false, nullptr);
        check(1);
        ASSERT_TRUE(ScoreRW::save_score(score, u"delete-linked-rests-test.mscx"));
        delete score;
        score = ScoreRW::read_score(u"delete-linked-rests-test.mscx", true);
        ASSERT_TRUE(score);
        ASSERT_EQ(score->excerpts().size(), 1u);
        part = score->excerpts().front()->excerpt_score();
        check(1);
        delete score;
    }
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
