/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include <memory>

#include <gtest/gtest.h>

#include "engraving/compat/scoreaccess.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mmrest.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/editing/editpart.h"
#include "engraving/editing/transaction/undostack.h"
#include "engraving/infrastructure/localfileinfoprovider.h"
#include "engraving/infrastructure/mscreader.h"
#include "engraving/infrastructure/mscwriter.h"
#include "engraving/rw/mscloader.h"
#include "engraving/rw/mscsaver.h"
#include "engraving/tests/utils/scorerw.h"
#include "global/io/buffer.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::io;

struct EngravingSaveHiddenPartsTests : ::testing::Test {
    static constexpr int num_measures = 10;

    /* Two single-staff instruments:
     * - The upper one only has notes in the first and last measures, so measures 2-9 form
     *   a multimeasure rest while the lower one is hidden.
     * - The lower one has a single note in `hiddenNoteMeasure`.
     * - The lower instrument is hidden at instrument level only (as e.g. the Timeline does),
     *   so its staff is still visible on its own (unlike hiding it from the Layout panel,
     *   which also hides its staves).
    */
    std::unique_ptr<MasterScore> create_score_with_hidden_instrument(int hidden_note_measure) {
        MasterScore *score = compat::ScoreAccess::create_master_score(ScoreRW::services());
        for (int i = 0; i < 2; ++i) {
            Part *part = new Part(score);
            score->append_part(part);
            score->append_staff(Factory::create_staff(part));
        }
        score->append_measures(::EngravingSaveHiddenPartsTests::num_measures);

        score->start_cmd(TranslatableString::untranslatable("Save hidden parts tests"));
        auto add_note = [score](int measure_number, staff_idx_t staff_idx) {
            Segment *segment = score->cr_measure(measure_number - 1)->first(SegmentType::CHORD_REST);
            score->set_note_rest(segment, staff_idx * ::mu::engraving::voices, NoteVal(60), Fraction(1, 4));
        };
        add_note(1, 0);
        add_note(::EngravingSaveHiddenPartsTests::num_measures, 0);
        add_note(hidden_note_measure, 1);
        score->undo_change_style_val(Sid::CREATE_MULTI_MEASURE_RESTS, true);
        EditPart::set_part_visible(score, score->parts().back(), false);
        score->end_cmd();

        return std::unique_ptr<MasterScore>(score);
    }

    ByteArray save_score(MasterScore *score) {
        ByteArray data;
        Buffer buffer(&data);

        MscWriter::Params params;
        params.device = &buffer;

        MscWriter writer(params);
        EXPECT_TRUE(writer.open());
        EXPECT_TRUE(MscSaver().write_mscz(score, writer));
        writer.close();

        return data;
    }

    std::unique_ptr<MasterScore> load_score(ByteArray data) {
        Buffer buffer(&data);

        MscReader::Params params;
        params.device = &buffer;

        MscReader reader(params);
        EXPECT_TRUE(reader.open());

        MasterScore *score = compat::ScoreAccess::create_master_score(ScoreRW::services());
        score->set_file_info_provider(std::make_shared<LocalFileInfoProvider>(PathT()));
        EXPECT_TRUE(MscLoader().load_mscz(score, reader, nullptr, /*ignoreVersionError*/ true));
        score->do_layout();

        return std::unique_ptr<MasterScore>(score);
    }

    // Pairs of (first measure number, number of measures) for every multimeasure rest
    std::vector<std::pair<int, int>> mm_rests(const Score *score) {
        std::vector<std::pair<int, int>> result;
        for (const Measure *m = score->first_measure(); m; m = m->next_measure()) {
            if (const Measure *mm_rest = m->mm_rest()) {
                result.emplace_back(static_cast<int>(m->measure_index()) + 1, mm_rest->mm_rest_count());
            }
        }
        return result;
    }

    bool has_note(const Score *score, int measure_number, staff_idx_t staff_idx) {
        const Segment *segment = score->cr_measure(measure_number - 1)->first(SegmentType::CHORD_REST);
        const EngravingItem *item = segment->element(staff_idx * ::mu::engraving::voices);
        return item && item->is_chord();
    }

    void show_all_parts(MasterScore *score) {
        score->start_cmd(TranslatableString::untranslatable("Save hidden parts tests"));
        for (Part *part : score->parts()) {
            EditPart::set_part_visible(score, part, true);
        }
        score->end_cmd();
    }
};

TEST_F(EngravingSaveHiddenPartsTests, SavingDoesNotChangeOrRelayoutTheScore) {
    // [GIVEN] A laid out score with MM rests and a hidden instrument
    std::unique_ptr<MasterScore> score = create_score_with_hidden_instrument(5);
    ASSERT_EQ(mm_rests(score.get()), (std::vector<std::pair<int, int>>{{2, 8}}));

    const std::vector<System *> systems_before = score->systems();
    const Measure *mm_rest_before = score->cr_measure(1)->mm_rest();
    const size_t undo_stack_size_before = score->undo_stack()->size();
    const size_t undo_stack_index_before = score->undo_stack()->current_index();

    // [WHEN] The score is saved
    save_score(score.get());

    // [THEN] Nothing was re-laid out: re-layout frees systems and MM rests that others (e.g. the playback
    // cursor) may still point to, and must not happen while parts are serialised on other threads
    EXPECT_EQ(score->systems(), systems_before);
    EXPECT_EQ(score->cr_measure(1)->mm_rest(), mm_rest_before);

    // [THEN] The score itself is unchanged
    EXPECT_FALSE(score->parts().back()->show());
    EXPECT_EQ(score->undo_stack()->size(), undo_stack_size_before);
    EXPECT_EQ(score->undo_stack()->current_index(), undo_stack_index_before);
}

// see musescore.org/en/node/27861
TEST_F(EngravingSaveHiddenPartsTests, HiddenInstrumentSurvivesSaveAndReload) {
    // [GIVEN] A score with MM rests and a hidden instrument that has a note inside the MM rest of the visible one
    std::unique_ptr<MasterScore> score = create_score_with_hidden_instrument(5);
    ASSERT_EQ(mm_rests(score.get()), (std::vector<std::pair<int, int>>{{2, 8}}));

    // [WHEN] The score is saved and reloaded
    std::unique_ptr<MasterScore> reloaded = load_score(save_score(score.get()));

    // [THEN] The instrument is still hidden, its note is kept, and the MM rests are the same
    ASSERT_EQ(reloaded->parts().size(), 2);
    EXPECT_FALSE(reloaded->parts().back()->show());
    ASSERT_EQ(reloaded->nmeasures(), ::EngravingSaveHiddenPartsTests::num_measures);
    EXPECT_TRUE(has_note(reloaded.get(), 5, 1));
    EXPECT_EQ(mm_rests(reloaded.get()), mm_rests(score.get()));

    // [WHEN] The instrument is shown, in both the reloaded and the original score
    show_all_parts(score.get());
    show_all_parts(reloaded.get());

    // [THEN] Its note breaks the MM rest in the same way as if it had never been saved
    EXPECT_EQ(mm_rests(score.get()), (std::vector<std::pair<int, int>>{{2, 3}, {6, 4}}));
    EXPECT_EQ(mm_rests(reloaded.get()), mm_rests(score.get()));
}

TEST_F(EngravingSaveHiddenPartsTests, EditOnMMRestOfHiddenLayoutSurvivesSaveAndReload) {
    // [GIVEN] A score where the hidden instrument has a note in the first measure of the MM rest,
    // so that this MM rest only exists while the instrument is hidden
    std::unique_ptr<MasterScore> score = create_score_with_hidden_instrument(2);
    ASSERT_EQ(mm_rests(score.get()), (std::vector<std::pair<int, int>>{{2, 8}}));

    // [GIVEN] Its number has been moved by the user
    MMRest *mm_rest = to_mm_rest(score->cr_measure(1)->mm_rest()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_TRUE(mm_rest);
    score->start_cmd(TranslatableString::untranslatable("Save hidden parts tests"));
    mm_rest->undo_change_property(Pid::MMREST_NUMBER_OFFSET, Spatium(2.5));
    score->end_cmd();

    // [WHEN] The score is saved and reloaded
    std::unique_ptr<MasterScore> reloaded = load_score(save_score(score.get()));

    // [THEN] The edit is kept
    ASSERT_EQ(mm_rests(reloaded.get()), (std::vector<std::pair<int, int>>{{2, 8}}));
    const MMRest *reloaded_mm_rest = to_mm_rest(reloaded->cr_measure(1)->mm_rest()->first(SegmentType::CHORD_REST)->element(0));
    ASSERT_TRUE(reloaded_mm_rest);
    EXPECT_EQ(reloaded_mm_rest->get_property(Pid::MMREST_NUMBER_OFFSET).value<Spatium>(), Spatium(2.5));
}
