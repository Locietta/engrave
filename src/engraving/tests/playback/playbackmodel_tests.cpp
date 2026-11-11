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

#include <cmath>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engraving/dom/chord.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/editing/transaction/undoablecommand.h"
#include "engraving/playback/playbackmodel.h"
#include "mpe/internal/articulationprofilesrepository.h"
#include "mpe/tests/utils/articulationutils.h"
#include "utils/scorerw.h"

using ::testing::Return;
using ::testing::_;

using namespace mu::engraving;
using namespace muse::mpe;
using namespace muse;

static const String playback_model_test_files_dir(u"playback/playbackmodel_data/");
static constexpr duration_t quarter_note_duration = 500000;                    // duration in microseconds for 4/4 120BPM
static constexpr duration_t half_note_duration = ::quarter_note_duration * 2;  // duration in microseconds for 1/2 120BPM
static constexpr duration_t whole_note_duration = ::quarter_note_duration * 4; // duration in microseconds for 4/4 120BPM

struct EngravingPlaybackModelTests : ::testing::Test {
protected:
    void SetUp() override {
        dummy_pattern_segment_.arrangement_pattern =
            tests::create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
        dummy_pattern_segment_.pitch_pattern = tests::create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
        dummy_pattern_segment_.expression_pattern =
            tests::create_simple_expression_pattern(dynamic_level_from_type(mpe::DynamicType::NATURAL));
        dummy_pattern_.emplace(0, dummy_pattern_segment_);

        default_profile_ = std::make_shared<ArticulationsProfile>();

        repository_ = std::make_shared<ArticulationProfilesRepository>(*muse::io::file_system());
        repository_->default_profile_override = ArticulationsProfilePtr();
    }

    ArticulationPattern build_test_articulation_pattern() const {
        ArticulationPatternSegment blank_segment(ArrangementPattern(::muse::mpe::hundred_percent /*durationFactor*/, 0 /*timestampOffset*/),
                                                 PitchPattern(ArticulationMap::expected_size, ::muse::mpe::ten_percent, 0),
                                                 ExpressionPattern(ArticulationMap::expected_size, ::muse::mpe::ten_percent, 0));

        ArticulationPattern pattern;
        pattern.emplace(0, std::move(blank_segment));

        return pattern;
    }

    ArticulationsProfilePtr default_profile_ = nullptr;

    ArticulationPattern dummy_pattern_;
    ArticulationPatternSegment dummy_pattern_segment_;

    std::shared_ptr<ArticulationProfilesRepository> repository_ = nullptr;
};

/**
 * @brief PlaybackModelTests_SimpleRepeat
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 4 measures
 *          Additionally, there is a simple repeat from measure 2 up to measure 3. In total, we'll be playing 6 measures overall
 */
TEST_F(EngravingPlaybackModelTests, SimpleRepeat) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"repeat_range/repeat_range.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events - 4 quarter notes on every measure * 6 overall measures which should be played
    int expected_size = 24;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);
}

/**
 * @brief PlaybackModelTests_Two_Ending_Repeat
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 6 measures
 *          Additionally, there is a repeat at the end of the second measure. Measure 2 is the first ending of the repeat.
 *          Measure 3 is the second ending of the repeat. In total, we'll be playing 7 measures overall
 */
TEST_F(EngravingPlaybackModelTests, Two_Ending_Repeat) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"repeat_with_2_voltas/repeat_with_2_voltas.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events - 4 quarter notes on every measure * 7 measures which should be played
    int expected_size = 28;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);
}

/**
 * @brief PlaybackModelTests_Repeat_And_Tremolo
 * @details Checks that tremolos after a repeated section are rendered at the correct timestamp
 */
TEST_F(EngravingPlaybackModelTests, Repeat_And_Tremolo) {
    // [GIVEN] Simple piece of score (Flute, 4/4, 120 bpm)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"repeat_and_tremolo/repeat_and_tremolo.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [WHEN] The articulation profiles repository will be returning profiles
    default_profile_->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());
    default_profile_->set_pattern(ArticulationType::TREMOLO32ND, build_test_articulation_pattern());

    repository_->default_profile_override = default_profile_;

    // [GIVEN] Expected amount of events per timestamp
    const std::map<timestamp_t, size_t> expected_size_per_timestamp{
        // The first four half notes; repeated
        {0 * 2 * ::quarter_note_duration, 16},
        {1 * 2 * ::quarter_note_duration, 16},
        {2 * 2 * ::quarter_note_duration, 16},
        {3 * 2 * ::quarter_note_duration, 16},
        {4 * 2 * ::quarter_note_duration, 16},
        {5 * 2 * ::quarter_note_duration, 16},
        {6 * 2 * ::quarter_note_duration, 16},
        {7 * 2 * ::quarter_note_duration, 16},

        // After the repeat
        {8 * 2 * ::quarter_note_duration, 16},

        // Final three half notes
        {10 * 2 * ::quarter_note_duration, 16},
        {11 * 2 * ::quarter_note_duration, 16},
        {12 * 2 * ::quarter_note_duration, 16},
    };

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events per timestamp matches expectations
    auto is_expected_timestamp = [&expected_size_per_timestamp](timestamp_t timestamp) {
        if (muse::contains(expected_size_per_timestamp, timestamp)) {
            return testing::AssertionSuccess();
        } else {
            return testing::AssertionFailure() << "timestamp " << timestamp << " is not contained in expectedSizePerTimestamp";
        }
    };

    size_t timestamp_count = 0;
    for (const auto &pair : result) {
        ++timestamp_count;

        ASSERT_TRUE(is_expected_timestamp(pair.first));

        size_t notes = 0;
        for (const PlaybackEvent &event : pair.second) {
            if (std::holds_alternative<mpe::NoteEvent>(event)) {
                // Check actual timestamp
                const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
                EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, pair.first + notes * (2 * ::quarter_note_duration / 16));

                ++notes;
            }
        }

        EXPECT_EQ(expected_size_per_timestamp.at(pair.first), notes);
    }

    EXPECT_EQ(timestamp_count, expected_size_per_timestamp.size());
}

/**
 * @brief PlaybackModelTests_Repeat_Tempo_Changes_And_Tie
 * @details Checks that the length of tied notes is correct even after tempo changes and repeats
 */
TEST_F(EngravingPlaybackModelTests, Repeat_Tempo_Changes_And_Tie) {
    // [GIVEN] Score containing some repeated measures, some tempo changes and a tied note
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"repeat_tempo_changes_and_tie/repeat_tempo_changes_and_tie.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [WHEN] The articulation profiles repository will be returning profiles
    default_profile_->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());

    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] The duration of the tied note matches expectations
    size_t note_event_count = 0;
    for (const auto &pair : result) {
        for (const PlaybackEvent &event : pair.second) {
            if (std::holds_alternative<mpe::NoteEvent>(event)) {
                const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
                EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, 8 * ::quarter_note_duration);

                ++note_event_count;
            }
        }
    }

    // [THEN] The amount of note events matches expectations
    EXPECT_EQ(note_event_count, 1);
}

/**
 * @brief PlaybackModelTests_Da_Capo_Al_Fine
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 6 measures
 *          Additionally, there is a "D.C. Al Fine" marking at the end of the 6-th measure. Measure 2 is marked by "Fine"
 *          In total, we'll be playing 8 measures overall
 */
TEST_F(EngravingPlaybackModelTests, Da_Capo_Al_Fine) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"da_capo_al_fine/da_capo_al_fine.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events - 4 quarter notes on every measure * 8 measures which should be played
    int expected_size = 32;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);
}

/**
 * @brief PlaybackModelTests_Dal_Segno_Al_Coda
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 6 measures
 *          Additionally, there is a "D.S. Al Coda" marking at the end of the 4-th measure. Measure 2 is marked by "Segno" marking.
 *          The end of the 3-rd measure is marked by "To Coda" marking. The beginning of the 5-th measure is marked by "Coda" sign
 *          In total, we'll be playing 8 measures overall
 */
TEST_F(EngravingPlaybackModelTests, Dal_Segno_Al_Coda) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"dal_segno_al_coda/dal_segno_al_coda.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events - 4 quarter notes on every measure * 8 measures which should be played
    int expected_size = 32;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);
}

/**
 * @brief PlaybackModelTests_Dal_Segno_Al_Fine
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 6 measures
 *          Additionally, there is a "D.S. Al Fine" marking at the end of the 6-th measure. Measure 2 is marked by "Segno" marking.
 *          The end of the 4-th measure is marked by "Fine" marking. In total, we'll be playing 9 measures overall
 */
TEST_F(EngravingPlaybackModelTests, Dal_Segno_Al_Fine) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"dal_segno_al_fine/dal_segno_al_fine.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events - 4 quarter notes on every measure * 9 measures which should be played
    int expected_size = 36;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);
}

/**
 * @brief PlaybackModelTests_Da_Capo_Al_Coda
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 6 measures
 *          Additionally, there is a "D.C. Al Coda" marking at the end of the 6-th measure. The end of the measure 2 is marked by "To Coda".
 *          The beginning of the 4-th measure is marked by "Coda" sign. In total, we'll be playing 11 measures overall
 */
TEST_F(EngravingPlaybackModelTests, Da_Capo_Al_Coda) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"da_capo_al_coda/da_capo_al_coda.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events - 4 quarter notes on every measure * 11 measures which should be played
    int expected_size = 44;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);
}

/**
 * @brief PlaybackModelTests_Spanners
 * @details Given a score where in each measure there is a spanner over the second and third note,
 *          we check that the spanner affects indeed the expected notes.
 */
TEST_F(EngravingPlaybackModelTests, Spanners) {
    // [GIVEN] Simple piece of score, where in each measure there is a spanner over the second and third note
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"spanners/spanners.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events
    static constexpr int expected_number_of_events = 3 * 4;

    // [WHEN] The articulation profiles repository will be returning profiles
    default_profile_->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());
    default_profile_->set_pattern(ArticulationType::PEDAL, build_test_articulation_pattern());
    default_profile_->set_pattern(ArticulationType::TRILL, build_test_articulation_pattern());
    default_profile_->set_pattern(ArticulationType::LEGATO, build_test_articulation_pattern());

    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events matches expectations
    EXPECT_EQ(result.size(), expected_number_of_events);

    // [THEN] Details of applied articulations match expectations
    struct ExpectedArticulation {
        ArticulationType articulation_type = ArticulationType::STANDARD;
        timestamp_t from = 0;
        timestamp_t to = 0;
    };

    // [THEN] Amount of applied articulations matches expectations
    static const std::vector<ExpectedArticulation> expected_articulations = {
        {},
        {ArticulationType::PEDAL, 1 * ::quarter_note_duration, 3 * ::quarter_note_duration},
        {ArticulationType::PEDAL, 1 * ::quarter_note_duration, 3 * ::quarter_note_duration},
        {},
        {},
        {ArticulationType::TRILL, 5 * ::quarter_note_duration, 7 * ::quarter_note_duration},
        {ArticulationType::TRILL, 5 * ::quarter_note_duration, 7 * ::quarter_note_duration},
        {},
        {},
        {ArticulationType::LEGATO, 9 * ::quarter_note_duration, 11 * ::quarter_note_duration},
        {ArticulationType::LEGATO, 9 * ::quarter_note_duration, 11 * ::quarter_note_duration},
        {}};

    for (size_t i = 0; i < expected_number_of_events; ++i) {
        const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(result.at(i * ::quarter_note_duration).at(0));

        EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);

        const ArticulationMap::PairType &articulation = *note_event.expression_ctx().articulations.cbegin();

        const ExpectedArticulation &expected_articulation = expected_articulations[i];

        if (expected_articulation.articulation_type == ArticulationType::STANDARD) {
            EXPECT_EQ(articulation.first, ArticulationType::STANDARD);
        } else {
            EXPECT_EQ(articulation.first, expected_articulation.articulation_type);
            EXPECT_EQ(articulation.second.meta.timestamp, expected_articulation.from);
            EXPECT_EQ(articulation.second.meta.timestamp + articulation.second.meta.overall_duration, expected_articulation.to);
        }
    }
}

/**
 * @brief PlaybackModelTests_Dynamics
 * @details Test simple dynamic markings and hairpins
 */

TEST_F(EngravingPlaybackModelTests, Dynamics) {
    // [GIVEN] Score with piano marking at the start, then crescendo to forte,
    //         then again crescendo, followed by sudden pianissimo
    MasterScore *score = ScoreRW::read_score(::playback_model_test_files_dir + u"dynamics/dynamics.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [WHEN] The articulation profiles repository will be returning profiles
    default_profile_->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());

    repository_->default_profile_override = default_profile_;

    // [WHEN] Init automation
    score->init_automation();

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const DynamicAutomationLayers &dynamics = model.resolve_track_playback_data(part->id(), part->instrument_id()).dynamics;
    ASSERT_FALSE(dynamics.empty());
    const DynamicAutomationMap &dynamic_curve = dynamics.begin()->second;

    // dynamics is now a sparse curve resolved on read (mirrors Volume/Pan automation), not a pre-baked dense
    // map -- resolve levels via evaluateCurveAt instead of exact-key lookup.
    auto level_at = [&dynamic_curve](timestamp_t t) { return dynamic_level_from_normalized(evaluate_curve_at(dynamic_curve, t)); };

    // [THEN] Resolved dynamic levels match expectations
    EXPECT_LT(dynamic_curve.size(), 10u); // sparse: one point per marking/hairpin endpoint, not a resampled ramp

    static constexpr dynamic_level_t piano = dynamic_level_from_type(mpe::DynamicType::P);
    static constexpr dynamic_level_t forte = dynamic_level_from_type(mpe::DynamicType::F);
    static constexpr dynamic_level_t forte_plus_something = dynamic_level_from_type(mpe::DynamicType::F) + ::muse::mpe::dynamic_level_step;
    static constexpr dynamic_level_t pianissimo = dynamic_level_from_type(mpe::DynamicType::PP);

    // Start piano
    EXPECT_EQ(level_at(0 * ::quarter_note_duration), piano);

    // Still piano at the start of the crescendo
    EXPECT_EQ(level_at(4 * ::quarter_note_duration), piano);

    // Reach forte
    EXPECT_EQ(level_at(8 * ::quarter_note_duration), forte);

    // Still forte at the start of next crescendo
    EXPECT_EQ(level_at(12 * ::quarter_note_duration), forte);

    // The 2nd crescendo's end tick coincides with the pianissimo marking below, so its true target
    // (fortePlusSomething) is approached but never exactly reached -- check it's gotten very close by
    // the tick before
    const dynamic_level_t just_before_pianissimo = level_at(12 * ::quarter_note_duration + 1919 / 480.0 * ::quarter_note_duration);
    EXPECT_LE(std::abs(just_before_pianissimo - forte_plus_something), 1);

    // Finally, jump to pianissimo
    EXPECT_EQ(level_at(16 * ::quarter_note_duration), pianissimo);

    // That should be the last real curve point
    EXPECT_EQ(std::prev(dynamic_curve.cend())->first, 16 * ::quarter_note_duration);

    delete score;
}

/**
 * @brief PlaybackModelTests_Pizz_To_Arco_Technique
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 1 measure
 *          Additionally, the first note is marked by "pizzicato" + "stacattissimo". The 3-rd note is marked by "arco"
 *          We'll be playing 4 events overall
 */
TEST_F(EngravingPlaybackModelTests, Pizz_To_Arco_Technique) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"pizz_to_arco/pizz_to_arco.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events
    int expected_size = 4;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    default_profile_->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());
    default_profile_->set_pattern(ArticulationType::PIZZICATO, build_test_articulation_pattern());
    default_profile_->set_pattern(ArticulationType::STACCATISSIMO, build_test_articulation_pattern());

    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);

    // [THEN] The first note has Pizzicato and Staccatissimo articulations applied
    const mpe::NoteEvent &first_note_event = std::get<mpe::NoteEvent>(result.at(0).at(0));
    EXPECT_EQ(first_note_event.expression_ctx().articulations.size(), 2);
    EXPECT_TRUE(first_note_event.expression_ctx().articulations.contains(ArticulationType::PIZZICATO));
    EXPECT_TRUE(first_note_event.expression_ctx().articulations.contains(ArticulationType::STACCATISSIMO));

    // [THEN] The second note has only Pizzicato articulation applied
    const mpe::NoteEvent &second_note_event = std::get<mpe::NoteEvent>(result.at(500000).at(0));
    EXPECT_EQ(second_note_event.expression_ctx().articulations.size(), 1);
    EXPECT_TRUE(second_note_event.expression_ctx().articulations.contains(ArticulationType::PIZZICATO));

    // [THEN] The third note has only Standard articulation applied
    const mpe::NoteEvent &third_note_event = std::get<mpe::NoteEvent>(result.at(1000000).at(0));
    EXPECT_EQ(third_note_event.expression_ctx().articulations.size(), 1);
    EXPECT_TRUE(third_note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));

    // [THEN] The fourth note has only Standard articulation applied
    const mpe::NoteEvent &fourth_note_event = std::get<mpe::NoteEvent>(result.at(1500000).at(0));
    EXPECT_EQ(fourth_note_event.expression_ctx().articulations.size(), 1);
    EXPECT_TRUE(fourth_note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));
}

/**
 * @brief PlaybackModelTests_FallbackToStandardArticulation
 * @details In this case we're building up a playback model of a simple score - Winds + Voice, 4/4, 120bpm, Treble Cleff, 2 measures
 *          The user added a bunch of articulations that are not supported by these instruments. These include:
 *          * pizz, detache for Winds
 *          * Fall/Doit/Plop/Scoop and notes with crosshead for Voice
 *          Make sure that we will fallback to the Standard articulation
 */
TEST_F(EngravingPlaybackModelTests, FallbackToStandardArticulation) {
    // [GIVEN] Simple piece of score (Winds + Voice, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"wrong_articulations/wrong_articulations.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 2);

    const Part *winds_part = score->parts().at(0);
    ASSERT_EQ(winds_part->instrument_id(), "winds");

    const Part *voice_part = score->parts().at(1);
    ASSERT_EQ(voice_part->instrument_id(), "voice");

    constexpr int note_count = 8;

    // [WHEN] The articulation profiles repository will be returning profiles for Winds/Voices families
    default_profile_->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());

    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    // [WHEN] Request events for Winds
    const PlaybackEventsMap &winds_result = model.resolve_track_playback_data(winds_part->id(), winds_part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(winds_result.size(), note_count);

    for (auto pair : winds_result) {
        const mpe::PlaybackEventList &events = pair.second;

        for (const PlaybackEvent &event : events) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            // [THEN] Use the Standard articulation instead of the one added by the user
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));
            EXPECT_GT(note_event.arrangement_ctx().actual_duration, 0);
        }
    }

    // [WHEN] Request events for Voice
    const PlaybackEventsMap &voice_result = model.resolve_track_playback_data(voice_part->id(), voice_part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(voice_result.size(), note_count);

    for (auto pair : voice_result) {
        const mpe::PlaybackEventList &events = pair.second;

        for (const PlaybackEvent &event : events) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            // [THEN] Use the Standard articulation instead of the one added by the user
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));
            EXPECT_GT(note_event.arrangement_ctx().actual_duration, 0);
        }
    }
}

/**
 * @brief PlaybackModelTests_Single_Measure_Repeat
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 7 measures
 *          The first measure contains 4 quarter notes, and the second measure contains a "repeat last measure" sign to repeat these.
 *          The third measure contains 8 eighth notes, and the fourth through sixth measures contain "repeat last measure" signs to
 *          repeat these three times.
 *          The final measure contains a final whole note.
 */
TEST_F(EngravingPlaybackModelTests, Single_Measure_Repeat) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"single_measure_repeat/single_measure_repeat.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events - 2 measures of 4 quarter notes, 4 measures of 8 eighth notes, 1 final note
    int expected_size = 2 * 4 + 4 * 8 + 1;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);
}

/**
 * @brief PlaybackModelTests_Multi_Measure_Repeat
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 9 measures
 *          The first measure contains a simple pattern of 4 quarter notes.
 *          The second measure contains a "repeat last measure" sign; the third and fourth measures contain a "repeat last
 *          two measures" sign; the fifth through eighth measures contain a "repeat last four measures" sign.
 *          This means that we will repeat the same pattern eight times. The final measure contains a whole note.
 */
TEST_F(EngravingPlaybackModelTests, Multi_Measure_Repeat) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"multi_measure_repeat/multi_measure_repeat.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of events - 8 measures of 4 quarter notes, 1 final note
    int expected_size = 8 * 4 + 1;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);
}

//! See: https://github.com/musescore/MuseScore/issues/29051
TEST_F(EngravingPlaybackModelTests, MeasureRepeat_TiedNotes) {
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"measure_repeat_tied_notes.mscx");
    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    default_profile_->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.set_play_repeats(true);
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [THEN] Expected timestamps & durations
    constexpr duration_t expected_duration = ::half_note_duration + ::half_note_duration; // 2 tied half notes

    const std::vector<TimestampAndDuration> expected_tn_d_list{
        // 2nd measure
        {::whole_note_duration, expected_duration},

        // 3nd measure: measure repeat
        {::whole_note_duration * 2, expected_duration},

        // 2nd measure (repeated)
        {::whole_note_duration * 4, expected_duration},

        // 3nd measure (repeated): measure repeat
        {::whole_note_duration * 5, expected_duration},
    };

    size_t note_event_num = 0;

    for (const auto &pair : result) {
        for (const PlaybackEvent &event : pair.second) {
            if (!std::holds_alternative<mpe::NoteEvent>(event)) {
                continue;
            }

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
            const mpe::ArrangementContext &arrangement_ctx = note_event.arrangement_ctx();
            const TimestampAndDuration &expected_tn_d = expected_tn_d_list.at(note_event_num);

            EXPECT_EQ(arrangement_ctx.actual_timestamp, expected_tn_d.timestamp);
            EXPECT_EQ(arrangement_ctx.actual_duration, expected_tn_d.duration);

            ++note_event_num;
        }
    }

    EXPECT_EQ(expected_tn_d_list.size(), note_event_num);

    delete score;
}

/**
 * @brief PlaybackModelTests_SimpleRepeat_Changes_Notification
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 4 measures
 *          Additionally, there is a simple repeat from measure 2 up to measure 3. In total, we'll be playing 6 measures overall
 *
 *          When the model will be loaded we'll emulate a change notification on the 2-nd measure, so that there will be updated events
 *          on the main stream channel
 */
TEST_F(EngravingPlaybackModelTests, SimpleRepeat_Changes_Notification) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"repeat_range/repeat_range.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [GIVEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    InstrumentTrackId track_id{part->id(), part->instrument_id()};

    PlaybackData result = model.resolve_track_playback_data(track_id);
    size_t expected_event_count = 24;
    EXPECT_EQ(result.origin_events.size(), expected_event_count);

    ScoreChanges changes;
    changes.tick_from = 480;
    changes.tick_to = 3840;
    changes.staff_idx_from = 0;
    changes.staff_idx_to = 0;
    changes.changed_types = {ElementType::NOTE};
    score->master_score()->notify_changes(changes);
    EXPECT_EQ(model.resolve_track_playback_data(track_id).origin_events.size(), expected_event_count);
    changes.tick_from = 4800;
    changes.tick_to = 5760;
    changes.changed_types = {ElementType::PEDAL};
    score->master_score()->notify_changes(changes);
    EXPECT_EQ(model.resolve_track_playback_data(track_id).origin_events.size(), expected_event_count);
}

/**
 * @brief PlaybackModelTests_TempoChangesDuringNotes
 * @details Test that notes and other elements have the correct length when tempo changes occur during them
 */
TEST_F(EngravingPlaybackModelTests, TempoChangesDuringNotes) {
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"tempo_changes_during_notes/tempo_changes_during_notes.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [WHEN] The articulation profiles repository will be returning profiles
    default_profile_->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());
    default_profile_->set_pattern(ArticulationType::TREMOLO8TH, build_test_articulation_pattern());
    default_profile_->set_pattern(ArticulationType::PEDAL, build_test_articulation_pattern());

    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(part->id(), part->instrument_id()).origin_events;

    // [GIVEN] Expected events durations
    auto quarter_at_tempo = [](double tempo) { return static_cast<duration_t>(::quarter_note_duration / (tempo / 120.0)); };

    const std::vector<duration_t> expected_durations{
        // Tied note of two measures long, with tempo changes in the middle of it
        2 * quarter_at_tempo(100) + 4 * quarter_at_tempo(10) + 2 * quarter_at_tempo(100),

        // Tied note of two measures long, with ritenuto over it
        quarter_at_tempo(100) + quarter_at_tempo(95) + quarter_at_tempo(90) + quarter_at_tempo(85) + quarter_at_tempo(80) +
            quarter_at_tempo(75) + quarter_at_tempo(70) + quarter_at_tempo(65),

        // Tied note of two measures long, with tempo changes in the middle of it, with eighth notes tremolo
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(10) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(10) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(10) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(10) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(10) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(10) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(10) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(10) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),

        // Tied note of two measures long, with ritenuto over it, with eighth notes tremolo
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(100) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(95) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(95) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(90) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(90) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(85) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(85) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(80) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(80) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(75) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(75) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(70) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(70) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(65) * 0.5),
        static_cast<duration_t>(quarter_at_tempo(65) * 0.5),

        // Same as beginning, but now with pedal
        2 * quarter_at_tempo(100) + 4 * quarter_at_tempo(10) + 2 * quarter_at_tempo(100),
        quarter_at_tempo(100) + quarter_at_tempo(95) + quarter_at_tempo(90) + quarter_at_tempo(85) + quarter_at_tempo(80) +
            quarter_at_tempo(75) + quarter_at_tempo(70) + quarter_at_tempo(65),
    };

    // Allow slight deviations because of rounding errors
    auto approx_equal = [](duration_t lhs, duration_t rhs) {
        if (std::abs(lhs - rhs) < 2000) {
            return testing::AssertionSuccess();
        } else {
            return testing::AssertionFailure() << lhs << " vs " << rhs;
        }
    };

    // [THEN] Events match expectations
    size_t index = 0;
    for (const auto &pair : result) {
        for (const auto &event : pair.second) {
            SCOPED_TRACE("event " + std::to_string(index) + " at timestamp " + std::to_string(pair.first));

            ASSERT_TRUE(index < expected_durations.size());

            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            EXPECT_TRUE(approx_equal(note_event.arrangement_ctx().nominal_duration, expected_durations.at(index)));

            if (index >= expected_durations.size() - 2) { // For the last two notes
                // [THEN] Expect Pedal to be present
                EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);

                const ArticulationMap::PairType &articulation = *note_event.expression_ctx().articulations.cbegin();

                // and have same length as note
                EXPECT_EQ(articulation.first, ArticulationType::PEDAL);
                EXPECT_TRUE(approx_equal(articulation.second.meta.timestamp, note_event.arrangement_ctx().nominal_timestamp));
                EXPECT_TRUE(approx_equal(articulation.second.meta.overall_duration, note_event.arrangement_ctx().nominal_duration));
            }

            ++index;
        }
    }

    // [THEN] Number of events matches expectations
    ASSERT_EQ(index, expected_durations.size());
}

/**
 * @brief PlaybackModelTests_Metronome_4_4
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 4 measures
 *          Measure 1: 4 quarter notes, Measure 2: full measure rest, Measure 3: 8 eighth-notes, Measure 4: 16 sixteen-notes
 *          So that we'll be playing 16 beats in total
 */
TEST_F(EngravingPlaybackModelTests, Metronome_4_4) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"metronome_4_4/metronome_4_4.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of metronome events - 4 beats on every measure * 4 measures which should be played
    int expected_size = 16;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded with Metronome enabled
    PlaybackModel model(repository_.get());
    model.set_is_metronome_enabled(true);
    model.load(score);

    const PlaybackEventsMap &events_when_metronome_enabled = model.resolve_track_playback_data(model.metronome_track_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(events_when_metronome_enabled.size(), expected_size);
    for (const auto &[timestamp, list] : events_when_metronome_enabled) {
        EXPECT_EQ(list.size(), 1);
    }

    // [WHEN] Score has been changed
    ScoreChanges changes;
    changes.tick_from = 480;
    changes.tick_to = 960;
    changes.staff_idx_from = 0;
    changes.staff_idx_to = 0;
    changes.changed_types = {ElementType::NOTE};

    score->master_score()->notify_changes(changes);

    // [THEN] Amount of events does match expectations
    const PlaybackEventsMap &events_after_score_change = model.resolve_track_playback_data(model.metronome_track_id()).origin_events;
    EXPECT_EQ(events_after_score_change.size(), expected_size);
    for (const auto &[timestamp, list] : events_after_score_change) {
        EXPECT_EQ(list.size(), 1);
    }

    // [WHEN] The playback model requested to be loaded with Metronome disabled
    model.set_is_metronome_enabled(false);
    model.reload();

    const PlaybackEventsMap &events_when_metronome_disabled = model.resolve_track_playback_data(model.metronome_track_id()).origin_events;

    // [THEN] No Metronome events
    EXPECT_TRUE(events_when_metronome_disabled.empty());

    delete score;
}

/**
 * @brief PlaybackModelTests_Metronome_6_4_Repeat
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 4 measures
 *          Additionally, there is a simple repeat on measure 2. In total, we'll be playing 30 beats, including repeated measure
 */
TEST_F(EngravingPlaybackModelTests, Metronome_6_4_Repeat) {
    // [GIVEN] Simple piece of score (Violin, 6/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"metronome_6_4_with_repeat/metronome_6_4_with_repeat.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] Expected amount of metronome events - 6 beats on every measure * 5 measures which should be played
    int expected_size = 30;

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.set_is_metronome_enabled(true);
    model.load(score);

    const PlaybackEventsMap &result = model.resolve_track_playback_data(model.metronome_track_id()).origin_events;

    // [THEN] Amount of events does match expectations
    EXPECT_EQ(result.size(), expected_size);

    delete score;
}

/**
 * @brief PlaybackModelTests_Note_Entry_Playback_Note
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 1 measure
 *          Additionally, we'll emulate the situation where user clicks on the first note, so that we should playback it
 *
 */
TEST_F(EngravingPlaybackModelTests, Note_Entry_Playback_Note) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    MasterScore *score = ScoreRW::read_score(::playback_model_test_files_dir + u"note_entry_playback/note_entry_playback_note.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] The articulation profiles repository will be returning required profiles
    repository_->default_profile_override = default_profile_;

    // [GIVEN] The very first note of the score
    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    const Chord *chord = to_chord(first_segment->next_chord_rest(0));
    ASSERT_TRUE(chord);
    ASSERT_TRUE(chord->notes().size() == 1);

    const Note *first_note = chord->notes().front();
    mpe::timestamp_t first_note_timestamp = 0;

    // [WHEN] Init automation
    score->init_automation();

    // [GIVEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    PlaybackData result = model.resolve_track_playback_data(part->id(), part->instrument_id());

    // [GIVEN] Expected note event
    const mpe::NoteEvent &expected_event = std::get<mpe::NoteEvent>(result.origin_events.at(first_note_timestamp).front());

    // [THEN] Triggered events map will match our expectations
    auto check_events = [first_note_timestamp, expected_event](const PlaybackEventsMap &triggered_events) {
        ASSERT_EQ(triggered_events.size(), 1);
        const PlaybackEventList &event_list = triggered_events.at(first_note_timestamp);

        ASSERT_EQ(event_list.size(), 1);
        const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event_list.front());

        EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_event.arrangement_ctx().actual_timestamp);
        // Use the score dynamics for offstream playback by default -- nominalDynamicLevel is baked into
        // the note event itself (see PlaybackModel::triggerEventsForItems), covered by this equality check
        EXPECT_EQ(note_event.expression_ctx(), expected_event.expression_ctx());
        EXPECT_EQ(note_event.pitch_ctx(), expected_event.pitch_ctx());
    };

    // [WHEN] User has clicked on the first note
    check_events(model.trigger_events_for_items({first_note}, ::quarter_note_duration));

    delete score;
}

/**
 * @brief PlaybackModelTests_Note_Entry_Playback_Chord
 * @details In this case we're building up a playback model of a simple score - Violin, 4/4, 120bpm, Treble Cleff, 1 measure
 *          Additionally, we'll emulate the situation where user wants to playback the third chord and make sure that we'll sent
 *          all the necessary events into audio-engine
 *
 */
TEST_F(EngravingPlaybackModelTests, Note_Entry_Playback_Chord) {
    // [GIVEN] Simple piece of score (Violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"note_entry_playback/note_entry_playback_chord.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    const Part *part = score->parts().at(0);
    ASSERT_TRUE(part);
    ASSERT_EQ(part->instruments().size(), 1);

    // [GIVEN] The articulation profiles repository will be returning required profiles
    repository_->default_profile_override = default_profile_;

    // [GIVEN] The third chord of the score
    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    size_t expected_note_count = 3;
    int third_chord_position_tick = 480 * 2;
    timestamp_t third_chord_timestamp = 500000 * 2;

    const Chord *third_chord = first_measure->find_chord(Fraction::from_ticks(third_chord_position_tick), 0);
    ASSERT_TRUE(third_chord);
    ASSERT_TRUE(third_chord->notes().size() == expected_note_count);

    // [GIVEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    PlaybackData result = model.resolve_track_playback_data(part->id(), part->instrument_id());

    // [GIVEN] Expected note event
    const PlaybackEventList &expected_events = result.origin_events.at(third_chord_timestamp);

    // [THEN] Triggered events map will match our expectations
    auto check_events = [expected_events](const PlaybackEventsMap &triggered_events) {
        ASSERT_EQ(triggered_events.size(), 1);
        const PlaybackEventList &actual_events = triggered_events.at(0);
        ASSERT_EQ(actual_events.size(), expected_events.size());

        for (size_t i = 0; i < expected_events.size(); ++i) {
            const mpe::NoteEvent expected_note_event = std::get<mpe::NoteEvent>(expected_events.at(i));
            const mpe::NoteEvent actual_note_event = std::get<mpe::NoteEvent>(actual_events.at(i));

            EXPECT_TRUE(actual_note_event.arrangement_ctx().actual_timestamp == 0);
            EXPECT_FALSE(actual_note_event.expression_ctx() == expected_note_event.expression_ctx());
            EXPECT_TRUE(actual_note_event.pitch_ctx() == expected_note_event.pitch_ctx());
        }
    };

    // [WHEN] Don't use the score dynamics
    model.set_use_score_dynamics_for_offstream_playback(false);

    // [WHEN] User has clicked on the first note
    check_events(model.trigger_events_for_items({third_chord}, ::quarter_note_duration));
}

/**
 * @brief PlaybackModelTests_Playback_Setup_Data_MultiInstrument
 * @details In this case we're building up a playback model of a score with 12 instruments:
 *          - sopranissimo-saxophone
 *          - marching-tenor-drums
 *          - hand-clap
 *          - guitar-steel
 *          - bass-steel-drums
 *          - alto-viol
 *          - f-wagner-tuba
 *          - bass-harmonica-hohner
 *          - chinese-tom-toms
 *          - electric-piano
 *          - boy-soprano
 *
 *          We need to make sure that playback setup data will be properly prepared, so that the score will be played with appropriate sounds
 */
TEST_F(EngravingPlaybackModelTests, Playback_Setup_Data_MultiInstrument) {
    // [GIVEN] Score with 12 instruments
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"playback_setup_instruments/playback_setup_instruments.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 12);

    constexpr bool supports_snd = true; // supports single note dynamics

    // [GIVEN] Expected setup data for each instrument
    std::unordered_map<String, mpe::PlaybackSetupData> expected_setup_data = {
        {u"sopranissimo-saxophone", {SoundId::SAXOPHONE, SoundCategory::WINDS, {SoundSubCategory::SOPRANISSIMO}, supports_snd}},
        {u"marching-tenor-drums",
         {SoundId::DRUM, SoundCategory::PERCUSSIONS, {SoundSubCategory::MARCHING, SoundSubCategory::SNARE, SoundSubCategory::TENOR}}},
        {u"hand-clap", {SoundId::CLAP, SoundCategory::PERCUSSIONS, {SoundSubCategory::HAND}}},
        {u"guitar-steel",
         {SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC, SoundSubCategory::STEEL, SoundSubCategory::PLUCKED}}},
        {u"bass-steel-drums",
         {SoundId::STEEL_DRUMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::METAL, SoundSubCategory::STEEL, SoundSubCategory::BASS}}},
        {u"alto-viol", {SoundId::VIOL, SoundCategory::STRINGS, {SoundSubCategory::ALTO}, supports_snd}},
        {u"f-wagner-tuba", {SoundId::TUBA, SoundCategory::WINDS, {SoundSubCategory::WAGNER}, supports_snd}},
        {u"bass-harmonica-hohner",
         {SoundId::HARMONICA, SoundCategory::WINDS, {SoundSubCategory::BASS, SoundSubCategory::HOHNER}, supports_snd}},
        {u"chinese-tom-toms", {SoundId::TOM_TOMS, SoundCategory::PERCUSSIONS, {SoundSubCategory::CHINESE}}},
        {u"electric-piano", {SoundId::PIANO, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC}}},
        {u"crystal-synth",
         {SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS, {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_CRYSTAL}, supports_snd}},
        {u"boy-soprano", {SoundId::CHOIR, SoundCategory::VOICES, {SoundSubCategory::SOPRANO, SoundSubCategory::BOY}, supports_snd}},
    };

    // [WHEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [WHEN] The playback model requested to be loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    // [THEN] Result matches with our expectations
    for (const Part *part : score->parts()) {
        for (const auto &pair : part->instruments()) {
            const String &instrument_id = pair.second->id();
            const PlaybackData &result = model.resolve_track_playback_data(part->id(), instrument_id);

            EXPECT_EQ(result.setup_data, expected_setup_data.at(instrument_id));
        }
    }
}

/**
 * @brief PlaybackModelTests_Score_Changes_Track_Range
 * @details A score-level change with a narrow range (e.g. inserting measures) must re-render all tracks,
 *          while an automation edit on one staff must only re-render the tracks of that staff
 *          See: https://github.com/musescore/MuseScore/issues/35246
 */
TEST_F(EngravingPlaybackModelTests, Score_Changes_Track_Range) {
    // [GIVEN] Score with multiple instruments
    Score *score = ScoreRW::read_score(::playback_model_test_files_dir + u"playback_setup_instruments/playback_setup_instruments.mscx");

    ASSERT_TRUE(score);
    ASSERT_GT(score->parts().size(), 1);

    const Part *first_part = score->parts().front();
    const Part *last_part = score->parts().back();
    const InstrumentTrackId first_track_id{first_part->id(), first_part->instrument_id()};
    const InstrumentTrackId last_track_id{last_part->id(), last_part->instrument_id()};

    // [GIVEN] The articulation profiles repository will be returning profiles for StringsArticulation family
    repository_->default_profile_override = default_profile_;

    // [GIVEN] The playback model is loaded
    PlaybackModel model(repository_.get());
    model.load(score);

    // The tracks that the model re-rendered for the last change: the changed set,
    // which a send of the events empties for the next change.
    InstrumentTrackIdSet changed_tracks;
    const auto take_changed_tracks = [&model, &changed_tracks]() {
        changed_tracks = model.changed_track_id_set();
        model.send_events_for_changed_tracks();
    };

    // [WHEN] A score-level change (e.g. inserting a measure) with a narrow range on the 1st staff
    ScoreChanges changes;
    changes.tick_from = 0;
    changes.tick_to = 1920;
    changes.staff_idx_from = 0;
    changes.staff_idx_to = 0;
    changes.changed_types = {ElementType::SCORE, ElementType::MEASURE};
    changes.changed_objects = {{score, {CommandType::INSERT_TIME}}};

    score->master_score()->notify_changes(changes);
    take_changed_tracks();

    // [THEN] All tracks are re-rendered
    EXPECT_TRUE(muse::contains(changed_tracks, first_track_id));
    EXPECT_TRUE(muse::contains(changed_tracks, last_track_id));

    // [WHEN] An automation edit on the 1st staff
    changed_tracks.clear();
    changes.changed_types = {ElementType::SCORE};
    changes.changed_objects = {{score, {CommandType::EDIT_AUTOMATION_POINTS}}};

    score->master_score()->notify_changes(changes);
    take_changed_tracks();

    // [THEN] Only the tracks of the 1st staff are re-rendered
    EXPECT_TRUE(muse::contains(changed_tracks, first_track_id));
    EXPECT_FALSE(muse::contains(changed_tracks, last_track_id));

    // [WHEN] An automation edit together with another score-level change on the 1st staff
    changed_tracks.clear();
    changes.changed_objects = {{score, {CommandType::EDIT_AUTOMATION_POINTS, CommandType::INSERT_TIME}}};

    score->master_score()->notify_changes(changes);
    take_changed_tracks();

    // [THEN] All tracks are re-rendered
    EXPECT_TRUE(muse::contains(changed_tracks, first_track_id));
    EXPECT_TRUE(muse::contains(changed_tracks, last_track_id));

    delete score;
}
