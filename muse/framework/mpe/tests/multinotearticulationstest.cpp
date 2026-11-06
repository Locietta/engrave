/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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
#include <map>

#include "mpe/events.h"
#include "mpe/tests/utils/articulationutils.h"

using namespace muse;
using namespace muse::mpe;
using namespace muse::mpe::tests;

class MpeMultiNoteArticulationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // [GIVEN] Rendering data of the very first quarter note on the score, with the 120BPM tempo and 4/4 time signature
        NoteMetaData &first_note_data = initial_data_[0];
        first_note_data.nominal_timestamp = 0;
        first_note_data.nominal_duration = 500;
        first_note_data.voice_idx = 0; // first voice
        first_note_data.staff_idx = 0; // first staff;
        first_note_data.nominal_pitch_level = pitch_level(PitchClass::A, 3);
        first_note_data.nominal_dynamic_level = dynamic_level_from_type(DynamicType::F);

        // [GIVEN] Rendering data of the second quarter note on the score, with the 120BPM tempo and 4/4 time signature
        NoteMetaData &second_note_data = initial_data_[1];
        second_note_data.nominal_timestamp = 500;
        second_note_data.nominal_duration = 500;
        second_note_data.voice_idx = 0; // first voice
        second_note_data.staff_idx = 0; // first staff;
        second_note_data.nominal_pitch_level = pitch_level(PitchClass::C, 4);
        second_note_data.nominal_dynamic_level = dynamic_level_from_type(DynamicType::F);

        // [GIVEN] Rendering data of the second quarter note on the score, with the 120BPM tempo and 4/4 time signature
        NoteMetaData &third_note_data = initial_data_[2];
        third_note_data.nominal_timestamp = 1000;
        third_note_data.nominal_duration = 500;
        third_note_data.voice_idx = 0; // first voice
        third_note_data.staff_idx = 0; // first staff;
        third_note_data.nominal_pitch_level = pitch_level(PitchClass::A, 3);
        third_note_data.nominal_dynamic_level = dynamic_level_from_type(DynamicType::F);

        // [GIVEN] Rendering data of the second quarter note on the score, with the 120BPM tempo and 4/4 time signature
        NoteMetaData &fourth_note_data = initial_data_[3];
        fourth_note_data.nominal_timestamp = 1500;
        fourth_note_data.nominal_duration = 500;
        fourth_note_data.voice_idx = 0; // first voice
        fourth_note_data.staff_idx = 0; // first staff;
        fourth_note_data.nominal_pitch_level = pitch_level(PitchClass::C, 4);
        fourth_note_data.nominal_dynamic_level = dynamic_level_from_type(DynamicType::F);

        // [GIVEN] Articulation pattern "Standard", which means that note should be played without any modifications
        standard_pattern_.arrangement_pattern =
            create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
        standard_pattern_.pitch_pattern = create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
        standard_pattern_.expression_pattern = create_simple_expression_pattern(dynamic_level_from_type(DynamicType::NATURAL));
    }

    struct NoteMetaData {
        timestamp_t nominal_timestamp = 0;
        duration_t nominal_duration = 0;
        voice_layer_idx_t voice_idx = 0;
        staff_layer_idx_t staff_idx = 0;

        pitch_level_t nominal_pitch_level = 0;

        dynamic_level_t nominal_dynamic_level = 0;
    };

    std::map<size_t, NoteMetaData> initial_data_;

    ArticulationPatternSegment standard_pattern_;
};

/**
 * @brief MPE_MultiNoteArticulationsTest_StandardPattern
 * @details In this case we're gonna build a very simple note sequence without any articulation applied on the top of them
 *          So the actual PitchCurve of every note would be equal to the data from the "StandardPattern" in Articulation Profile.
 *          However, the actual ExpressionCurve would be adapted by the current dynamic value ("forte")
 */
TEST_F(MpeMultiNoteArticulationsTest, StandardPattern) {
    // [GIVEN] No articulations applied on the top of the note
    ArticulationPattern pattern;
    pattern.emplace(0, standard_pattern_);

    ArticulationMeta meta;
    meta.type = ArticulationType::STANDARD;
    meta.pattern = pattern;
    meta.timestamp = initial_data_.at(0).nominal_timestamp;
    meta.overall_duration = initial_data_.at(0).nominal_duration;

    ArticulationAppliedData standard_articulation_applied(std::move(meta), 0, ::muse::mpe::hundred_percent);

    ArticulationMap applied_articulations;
    applied_articulations.emplace(ArticulationType::STANDARD, std::move(standard_articulation_applied));
    applied_articulations.pre_calculate_average_data();

    // [WHEN] Notes sequence with given parameters being built
    std::map<size_t, NoteEvent> note_events;

    for (const auto &pair : initial_data_) {
        NoteEvent note_event(pair.second.nominal_timestamp, pair.second.nominal_duration, pair.second.voice_idx, pair.second.staff_idx,
                             pair.second.nominal_pitch_level, pair.second.nominal_dynamic_level, applied_articulations, 0);

        note_events.emplace(pair.first, std::move(note_event));
    }

    for (const auto &pair : note_events) {
        // [THEN] We expect that nominal timestamp of every note will be equal to the actual timestamp, since there is no arrangement modifiers
        EXPECT_EQ(pair.second.arrangement_ctx().nominal_timestamp, pair.second.arrangement_ctx().actual_timestamp);

        // [THEN] We expect that nominal duration of every note will be equal to the actual duration, since there is no arrangement modifiers
        EXPECT_EQ(pair.second.arrangement_ctx().nominal_duration, pair.second.arrangement_ctx().actual_duration);

        // [THEN] We expect that amplitude dynamic value in ExpressionCurve adapted for the current dynamic value ("forte")
        EXPECT_EQ(pair.second.expression_ctx().expression_curve.max_amplitude_level(), dynamic_level_from_type(DynamicType::F));

        // [THEN] We expect that pitch curve of every note will be using default curve from the standard patterns
        EXPECT_EQ(pair.second.pitch_ctx().pitch_curve, standard_pattern_.pitch_pattern.pitch_offset_map);
    }
}

/**
 * @brief MPE_MultiNoteArticulationsTest_GlissandoPattern
 * @details In this case we're gonna build a very simple note sequence with glissando articulation applied on the top of the first two notes
 *          So the actual PitchCurve of the first two notes would be equal to the data from the "GlissandoPattern" in Articulation Profile
 */
TEST_F(MpeMultiNoteArticulationsTest, GlissandoPattern) {
    std::map<size_t, ArticulationMap> applied_articulations;

    // [GIVEN] Articulation pattern "Glissando", which instructs a performer to start on the pitch/rhythm
    //         and slide the pitch up/down to land on the next pitch/rhythm

    pitch_level_t pitch_diff = initial_data_[0].nominal_pitch_level - initial_data_[1].nominal_pitch_level;

    ArticulationPatternSegment glissando_pattern;
    glissando_pattern.arrangement_pattern =
        create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
    glissando_pattern.pitch_pattern =
        create_simple_pitch_pattern(pitch_diff / (::muse::mpe::max_pitch_level / ::muse::mpe::ten_percent) /*increment_pitch_diff*/);
    glissando_pattern.expression_pattern = create_simple_expression_pattern(
        dynamic_level_from_type(DynamicType::NATURAL) /* no dynamic changes comparing to the natural one*/);

    ArticulationPattern glissando_scope;
    glissando_scope.emplace(0, glissando_pattern);

    ArticulationMeta glissando_meta;
    glissando_meta.type = ArticulationType::DISCRETE_GLISSANDO;
    glissando_meta.pattern = glissando_scope;
    glissando_meta.timestamp = initial_data_.at(0).nominal_timestamp;
    glissando_meta.overall_duration = initial_data_.at(0).nominal_duration + initial_data_.at(1).nominal_duration;

    ArticulationPattern standard_scope;
    standard_scope.emplace(0, standard_pattern_);

    ArticulationMeta third_note_standard_meta;
    third_note_standard_meta.type = ArticulationType::STANDARD;
    third_note_standard_meta.pattern = standard_scope;
    third_note_standard_meta.timestamp = initial_data_.at(2).nominal_timestamp;
    third_note_standard_meta.overall_duration = initial_data_.at(0).nominal_duration;

    ArticulationMeta fourth_note_standard_meta;
    fourth_note_standard_meta.type = ArticulationType::STANDARD;
    fourth_note_standard_meta.pattern = standard_scope;
    fourth_note_standard_meta.timestamp = initial_data_.at(3).nominal_timestamp;
    fourth_note_standard_meta.overall_duration = initial_data_.at(0).nominal_duration;

    // [GIVEN] Glissando articulation applied on the first note, occupied range is from 0% to 50% of the entire articulation range
    ArticulationAppliedData glissando_applied_on_the_first_note(glissando_meta, 0, 5 * ::muse::mpe::ten_percent);
    applied_articulations[0].emplace(ArticulationType::DISCRETE_GLISSANDO, std::move(glissando_applied_on_the_first_note));
    applied_articulations[0].pre_calculate_average_data();

    // [GIVEN] Glissando articulation applied on the second note, occupied range is from 50% to 100% of the entire articulation range
    ArticulationAppliedData glissando_applied_on_the_second_note(glissando_meta, 50, ::muse::mpe::hundred_percent);
    applied_articulations[1].emplace(ArticulationType::DISCRETE_GLISSANDO, std::move(glissando_applied_on_the_second_note));
    applied_articulations[1].pre_calculate_average_data();

    // [GIVEN] No articulations applied on the third note, occupied range is from 0% to 100% of the entire articulation range
    ArticulationAppliedData third_note_standard_articulation(third_note_standard_meta, 0, ::muse::mpe::hundred_percent);
    applied_articulations[2].emplace(ArticulationType::STANDARD, std::move(third_note_standard_articulation));
    applied_articulations[2].pre_calculate_average_data();

    // [GIVEN] No articulations applied on the third note, occupied range is from 0% to 100% of the entire articulation range
    ArticulationAppliedData fourth_note_standard_articulation(fourth_note_standard_meta, 0, ::muse::mpe::hundred_percent);
    applied_articulations[3].emplace(ArticulationType::STANDARD, std::move(fourth_note_standard_articulation));
    applied_articulations[3].pre_calculate_average_data();

    // [WHEN] Notes sequence with given parameters being built
    std::map<size_t, NoteEvent> note_events;

    for (const auto &pair : initial_data_) {
        NoteEvent note_event(pair.second.nominal_timestamp, pair.second.nominal_duration, pair.second.voice_idx, pair.second.staff_idx,
                             pair.second.nominal_pitch_level, pair.second.nominal_dynamic_level, applied_articulations[pair.first], 0);

        note_events.emplace(pair.first, std::move(note_event));
    }

    for (const auto &pair : note_events) {
        // [THEN] We expect that ExpressionCurve of every note will be adapted for the current dynamic value ("forte")
        EXPECT_EQ(pair.second.expression_ctx().expression_curve.max_amplitude_level(), dynamic_level_from_type(DynamicType::F));
    }

    for (size_t i = 0; i < 2; ++i) {
        // [THEN] We expect that pitch curve of the notes 1 and 2 will be using a curve from the glissando pattern
        EXPECT_EQ(note_events.at(i).pitch_ctx().pitch_curve, glissando_pattern.pitch_pattern.pitch_offset_map);
    }

    for (size_t i = 2; i < 4; ++i) {
        // [THEN] We expect that pitch curve of the notes 3 and 4 will be using a curve from the standard pattern
        EXPECT_EQ(note_events.at(i).pitch_ctx().pitch_curve, standard_pattern_.pitch_pattern.pitch_offset_map);
    }
}

/**
 * @brief MPE_MultiNoteArticulationsTest_CrescendoPattern
 * @details In this case we're gonna build a very simple note sequence with crescendo articulation applied on
 *          the top of these notes (from forte to fortissimo). So the actual ExpressionCurve of the notes would be equal
 *          to the data from the "CrescendoPattern" in Articulation Profile
 */
TEST_F(MpeMultiNoteArticulationsTest, CrescendoPattern) {
    std::map<size_t, ArticulationMap> applied_articulations;

    // [GIVEN] Articulation pattern "Crescendo", which instructs a performer to gradually increase a volume of every note,
    //         using a certain range of dynamics. In our case there will be a crescendo from forte up to fortissimo

    int note_count = static_cast<int>(initial_data_.size());
    int dynamic_segments_count = note_count - 1;

    dynamic_level_t dynamic_level_diff = dynamic_level_from_type(DynamicType::FF) - dynamic_level_from_type(DynamicType::F);

    ArticulationPattern crescendo_scope;

    for (int i = 0; i <= dynamic_segments_count; ++i) {
        ArticulationPatternSegment crescendo_pattern;
        crescendo_pattern.arrangement_pattern =
            create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
        crescendo_pattern.pitch_pattern = create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
        crescendo_pattern.expression_pattern = create_simple_expression_pattern(
            dynamic_level_from_type(DynamicType::NATURAL) + i * ::muse::mpe::dynamic_level_step / dynamic_segments_count);

        crescendo_scope.emplace(25 * ::muse::mpe::one_percent * i, std::move(crescendo_pattern));
    }

    ArticulationMeta crescendo_meta;
    crescendo_meta.type = ArticulationType::DISCRETE_GLISSANDO;
    crescendo_meta.pattern = crescendo_scope;
    crescendo_meta.timestamp = initial_data_.at(0).nominal_timestamp;
    crescendo_meta.overall_duration = initial_data_.at(1).nominal_timestamp + initial_data_.at(1).nominal_duration;
    crescendo_meta.overall_dynamic_changes_range = dynamic_level_diff;

    for (const auto &pair : initial_data_) {
        crescendo_meta.overall_duration += pair.second.nominal_duration;
    }

    for (int i = 0; i < note_count; ++i) {
        // [GIVEN] Crescendo articulation applied on the note
        ArticulationAppliedData crescendo_applied(crescendo_meta, 25 * ::muse::mpe::one_percent * i,
                                                  25 * ::muse::mpe::one_percent * (i + 1));
        applied_articulations[i].emplace(ArticulationType::CRESCENDO, std::move(crescendo_applied));
        applied_articulations[i].pre_calculate_average_data();
    }

    // [WHEN] Notes sequence with given parameters being built
    std::map<size_t, NoteEvent> note_events;

    for (const auto &pair : initial_data_) {
        NoteEvent note_event(pair.second.nominal_timestamp, pair.second.nominal_duration, pair.second.voice_idx, pair.second.staff_idx,
                             pair.second.nominal_pitch_level, pair.second.nominal_dynamic_level, applied_articulations[pair.first], 0);

        note_events.emplace(pair.first, std::move(note_event));
    }

    for (int i = 0; i < note_count; ++i) {
        // [THEN] We expect that ExpressionCurve of every note will be adapted to the applied CrescendoPattern
        //        That means that every note will be played louder on 125% than the previous one

        dynamic_level_t actual_result =
            dynamic_level_from_type(DynamicType::F) + i * static_cast<dynamic_level_t>(dynamic_level_diff) / dynamic_segments_count;
        EXPECT_EQ(note_events.at(i).expression_ctx().expression_curve.max_amplitude_level(), actual_result);
    }
}

TEST_F(MpeMultiNoteArticulationsTest, IsMultiNoteArticulation) {
    const ArticulationTypeSet multi_types = {
        ArticulationType::TRILL,
        ArticulationType::CRESCENDO,
        ArticulationType::DIMINUENDO,
        ArticulationType::DISCRETE_GLISSANDO,
        ArticulationType::LEGATO,
        ArticulationType::PEDAL,
        ArticulationType::MULTIBEND,
        ArticulationType::ARPEGGIO,
        ArticulationType::ARPEGGIO_UP,
        ArticulationType::ARPEGGIO_DOWN,
        ArticulationType::ARPEGGIO_STRAIGHT_UP,
        ArticulationType::ARPEGGIO_STRAIGHT_DOWN,
    };

    const ArticulationTypeSet ranged_types{
        ArticulationType::LEGATO,
        ArticulationType::PEDAL,
        ArticulationType::MULTIBEND,
    };

    for (int i = int(ArticulationType::STANDARD); i < int(ArticulationType::LAST); ++i) {
        ArticulationType type = ArticulationType(i);
        bool is_multi = muse::contains(multi_types, type);
        bool is_ranged = muse::contains(ranged_types, type);

        EXPECT_EQ(mpe::is_multi_note_articulation(type), is_multi);
        EXPECT_EQ(mpe::is_single_note_articulation(type), !is_multi);
        EXPECT_EQ(mpe::is_ranged_articulation(type), is_ranged);
    }
}
