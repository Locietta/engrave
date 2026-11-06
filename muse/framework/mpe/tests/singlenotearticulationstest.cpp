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

#include "mpe/events.h"
#include "mpe/tests/utils/articulationutils.h"

using namespace muse;
using namespace muse::mpe;
using namespace muse::mpe::tests;

class MpeSingleNoteArticulationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // [GIVEN] Nominal arrangement data of the very first quarter note on the score, with the 120BPM tempo and 4/4 time signature
        nominal_timestamp_ = 0;  // msecs
        nominal_duration_ = 500; // msecs
        voice_idx_ = 0;          // first voice
        staff_idx_ = 0;          // first staff

        // [GIVEN] Pitch data of the note
        pitch_class_ = PitchClass::A;
        octave_ = 4;

        // [GIVEN] Expression data of the note - no dynamic/articulations modifiers
        nominal_dynamic_ = dynamic_level_from_type(DynamicType::NATURAL);

        // [GIVEN] Articulation pattern "None", which means that note should be played without any modifications
        standard_pattern_.arrangement_pattern =
            create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
        standard_pattern_.pitch_pattern = create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
        standard_pattern_.expression_pattern = create_simple_expression_pattern(dynamic_level_from_type(DynamicType::NATURAL));
    }

    timestamp_t nominal_timestamp_;
    duration_t nominal_duration_;
    voice_layer_idx_t voice_idx_;
    staff_layer_idx_t staff_idx_;

    PitchClass pitch_class_;
    octave_t octave_;

    dynamic_level_t nominal_dynamic_;

    ArticulationPatternSegment standard_pattern_;
};

/**
 * @brief MPE_SingleNoteArticulationsTest_StandardPattern
 * @details In this case we're gonna build a simple note event without any articulation applied on the top of it
 *          So the actual PitchCurve and ExpressionCurve would be equal to the "StandardPattern" from the articulation Profile
 */
TEST_F(MpeSingleNoteArticulationsTest, StandardPattern) {
    // [GIVEN] No articulations applied on the top of the note
    ArticulationPattern scope;
    scope.emplace(0, standard_pattern_);

    ArticulationMeta standard_meta;
    standard_meta.type = ArticulationType::STACCATO;
    standard_meta.pattern = scope;
    standard_meta.timestamp = nominal_timestamp_;
    standard_meta.overall_duration = nominal_duration_;

    ArticulationAppliedData standard_articulation_applied(std::move(standard_meta), 0, ::muse::mpe::hundred_percent);

    ArticulationMap applied_articulations;
    applied_articulations.emplace(ArticulationType::STANDARD, std::move(standard_articulation_applied));
    applied_articulations.pre_calculate_average_data();

    // [WHEN] Note event with given parameters being built
    NoteEvent event(nominal_timestamp_, nominal_duration_, voice_idx_, staff_idx_, pitch_level(pitch_class_, octave_), nominal_dynamic_,
                    std::move(applied_articulations), 0);

    // [THEN] We expect that expression curve will be using default curve from standard patterns
    EXPECT_EQ(event.expression_ctx().expression_curve, standard_pattern_.expression_pattern.dynamic_offset_map);

    // [THEN] We expect that there is no pitch offset at all on this note
    for (const auto &pair : event.pitch_ctx().pitch_curve) {
        EXPECT_EQ(pair.second, 0);
    }
}

/**
 * @brief MPE_SingleNoteArticulationsTest_StaccatoPattern
 * @details In this case we're gonna build a simple note event with the staccato articulation applied on the top of it
 *          So the actual arrangement context of the note would reflect the parameters from staccato articulation pattern
 */
TEST_F(MpeSingleNoteArticulationsTest, StaccatoPattern) {
    // [GIVEN] Articulation pattern "Staccato", which instructs a performer to shorten duration of a note
    ArticulationPatternSegment staccato_articulation;
    staccato_articulation.arrangement_pattern =
        create_arrangement_pattern(5 * ::muse::mpe::ten_percent /*duration_factor*/, 0 /*timestamp_offset*/);
    staccato_articulation.pitch_pattern = create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
    staccato_articulation.expression_pattern =
        create_simple_expression_pattern(nominal_dynamic_ /* no dynamic changes comparing to the standard one*/);

    ArticulationPattern scope;
    scope.emplace(0, staccato_articulation);

    // [GIVEN] Staccato articulation applied on the note, since staccato is a single-note articulation
    //         occupied range is from 0% to 100%
    ArticulationMeta staccato_meta;
    staccato_meta.type = ArticulationType::STACCATO;
    staccato_meta.pattern = scope;
    staccato_meta.timestamp = nominal_timestamp_;
    staccato_meta.overall_duration = nominal_duration_;

    ArticulationAppliedData staccato_applied(std::move(staccato_meta), 0, ::muse::mpe::hundred_percent);

    ArticulationMap applied_articulations = {};
    applied_articulations.emplace(ArticulationType::STACCATO, std::move(staccato_applied));
    applied_articulations.pre_calculate_average_data();

    // [WHEN] Note event with given parameters being built
    NoteEvent event(nominal_timestamp_, nominal_duration_, voice_idx_, staff_idx_, pitch_level(pitch_class_, octave_), nominal_dynamic_,
                    std::move(applied_articulations), 0);

    // [THEN] We expect the nominal duration of a note to be unchanged
    EXPECT_EQ(event.arrangement_ctx().nominal_duration, nominal_duration_);

    // [THEN] We expect the actual duration of the staccato note to be halved.
    EXPECT_EQ(event.arrangement_ctx().actual_duration, nominal_duration_ * 0.5);
}

/**
 * @brief MPE_SingleNoteArticulationsTest_AccentPattern
 * @details In this case we're gonna build a simple note event with the accent articulation applied on the top of it
 *          So the actual expression context of the note would reflect the parameters from articulation pattern
 */
TEST_F(MpeSingleNoteArticulationsTest, AccentPattern) {
    // [GIVEN] Articulation pattern "Accent", which instructs a performer to play a note louder (usually on 1 dynamic level)
    ArticulationPatternSegment accent_articulation;
    accent_articulation.arrangement_pattern =
        create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
    accent_articulation.pitch_pattern = create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
    accent_articulation.expression_pattern = create_simple_expression_pattern(
        dynamic_level_from_type(DynamicType::MF) /* increasing a note's dynamic on a single level from Natural dynamic*/);

    ArticulationPattern scope;
    scope.emplace(0, accent_articulation);

    // [GIVEN] Staccato articulation applied on the note, since staccato is a single-note articulation
    //         occupied range is from 0% to 100%
    ArticulationMeta accent_meta;
    accent_meta.type = ArticulationType::ACCENT;
    accent_meta.pattern = scope;
    accent_meta.timestamp = nominal_timestamp_;
    accent_meta.overall_duration = nominal_duration_;

    ArticulationAppliedData accent_applied(std::move(accent_meta), 0, ::muse::mpe::hundred_percent);

    ArticulationMap applied_articulations = {};
    applied_articulations.emplace(ArticulationType::ACCENT, std::move(accent_applied));
    applied_articulations.pre_calculate_average_data();

    // [WHEN] Note event with given parameters being built
    NoteEvent event(nominal_timestamp_, nominal_duration_, voice_idx_, staff_idx_, pitch_level(pitch_class_, octave_), nominal_dynamic_,
                    std::move(applied_articulations), 0);

    // [THEN] We expect the nominal duration of a note to be unchanged,
    //        since accent doesn't affect any arrangement data
    EXPECT_EQ(event.arrangement_ctx().nominal_duration, nominal_duration_);

    // [THEN] We expect the nominal dynamic type is still unchanged
    EXPECT_EQ(event.expression_ctx().nominal_dynamic_level, dynamic_level_from_type(DynamicType::NATURAL));

    // [THEN] However, an amplitude dynamic value in ExpressionCurve has been increased on single level due to Accent Pattern
    EXPECT_EQ(event.expression_ctx().expression_curve.max_amplitude_level(), dynamic_level_from_type(DynamicType::MF));
}

/**
 * @brief MPE_SingleNoteArticulationsTest_AccentPattern
 * @details In this case we're gonna build a simple note event with the accent articulation applied on the top of it
 *          So the actual expression context of the note would reflect the parameters from articulation pattern.
 *
 *          The note is marked by "mezzo forte" dynamic already, so accent articulation should increase this value up to one level - "fortissimo"
 */
TEST_F(MpeSingleNoteArticulationsTest, AccentPattern_Nominal_MezzoForte) {
    // [GIVEN] The note marked by mezzo forte dynamic
    nominal_dynamic_ = dynamic_level_from_type(DynamicType::MF);

    // [GIVEN] Articulation pattern "Accent", which instructs a performer to play a note louder (usually on 1 dynamic level)
    ArticulationPatternSegment accent_articulation;
    accent_articulation.arrangement_pattern =
        create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
    accent_articulation.pitch_pattern = create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
    accent_articulation.expression_pattern = create_simple_expression_pattern(
        dynamic_level_from_type(DynamicType::NATURAL) +
        ::muse::mpe::dynamic_level_step /* increasing a note's dynamic on a single level from Natural dynamic*/);

    ArticulationPattern scope;
    scope.emplace(0, accent_articulation);

    // [GIVEN] Staccato articulation applied on the note, since staccato is a single-note articulation
    //         occupied range is from 0% to 100%
    ArticulationMeta accent_meta;
    accent_meta.type = ArticulationType::ACCENT;
    accent_meta.pattern = scope;
    accent_meta.timestamp = nominal_timestamp_;
    accent_meta.overall_duration = nominal_duration_;

    ArticulationAppliedData accent_applied(std::move(accent_meta), 0, ::muse::mpe::hundred_percent);

    ArticulationMap applied_articulations = {};
    applied_articulations.emplace(ArticulationType::ACCENT, std::move(accent_applied));
    applied_articulations.pre_calculate_average_data();

    // [WHEN] Note event with given parameters being built
    NoteEvent event(nominal_timestamp_, nominal_duration_, voice_idx_, staff_idx_, pitch_level(pitch_class_, octave_), nominal_dynamic_,
                    std::move(applied_articulations), 0);

    // [THEN] We expect the nominal duration of a note to be unchanged,
    //        since accent doesn't affect any arrangement data
    EXPECT_EQ(event.arrangement_ctx().nominal_duration, nominal_duration_);

    // [THEN] We expect the nominal dynamic type is still unchanged
    EXPECT_EQ(event.expression_ctx().nominal_dynamic_level, dynamic_level_from_type(DynamicType::MF));

    // [THEN] However, an amplitude dynamic value in ExpressionCurve has been increased on single level due to Accent Pattern
    EXPECT_EQ(event.expression_ctx().expression_curve.max_amplitude_level(), dynamic_level_from_type(DynamicType::F));
}

/**
 * @brief MPE_SingleNoteArticulationsTest_PocoTenuto
 * @details In this case we're gonna build a simple note event with the combination of staccato and tenuto articulations
 *          applied on the top of it. So the actual expression context of the note would reflect the average parameters
 *          from articulation patterns
 */
TEST_F(MpeSingleNoteArticulationsTest, PocoTenuto) {
    // [GIVEN] Articulation pattern "Staccato", which instructs a performer to shorten duration of a note
    ArticulationPatternSegment staccato_pattern;
    staccato_pattern.arrangement_pattern =
        create_arrangement_pattern(5 * ::muse::mpe::ten_percent /*duration_factor*/, 0 /*timestamp_offset*/);
    staccato_pattern.pitch_pattern = create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
    staccato_pattern.expression_pattern =
        create_simple_expression_pattern(nominal_dynamic_ /* no dynamic changes comparing to the standard one*/);

    ArticulationPattern staccato_scope;
    staccato_scope.emplace(0, staccato_pattern);

    // [GIVEN] Articulation pattern "Tenuto", which instructs a performer to play a note for the whole nominal duration
    ArticulationPatternSegment tenuto_pattern;
    tenuto_pattern.arrangement_pattern =
        create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
    tenuto_pattern.pitch_pattern = create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
    tenuto_pattern.expression_pattern =
        create_simple_expression_pattern(nominal_dynamic_ /* no dynamic changes comparing to the standard one*/);

    ArticulationPattern tenuto_scope;
    tenuto_scope.emplace(0, tenuto_pattern);

    ArticulationMap applied_articulations = {};

    // [GIVEN] Staccato articulation applied on the note, since staccato is a single-note articulation
    //         occupied range is from 0% to 100%
    ArticulationMeta staccato_meta;
    staccato_meta.type = ArticulationType::STACCATO;
    staccato_meta.pattern = staccato_scope;
    staccato_meta.timestamp = nominal_timestamp_;
    staccato_meta.overall_duration = nominal_duration_;

    ArticulationAppliedData staccato_applied(std::move(staccato_meta), 0, ::muse::mpe::hundred_percent);
    applied_articulations.emplace(ArticulationType::STACCATO, std::move(staccato_applied));

    ArticulationMeta tenuto_meta;
    tenuto_meta.type = ArticulationType::TENUTO;
    tenuto_meta.pattern = tenuto_scope;
    tenuto_meta.timestamp = nominal_timestamp_;
    tenuto_meta.overall_duration = nominal_duration_;

    // [GIVEN] Tenuto articulation applied on the note
    ArticulationAppliedData tenuto_applied(std::move(tenuto_meta), 0, ::muse::mpe::hundred_percent);
    applied_articulations.emplace(ArticulationType::TENUTO, std::move(tenuto_applied));
    applied_articulations.pre_calculate_average_data();

    // [WHEN] Note event with given parameters being built
    NoteEvent event(nominal_timestamp_, nominal_duration_, voice_idx_, staff_idx_, pitch_level(pitch_class_, octave_), nominal_dynamic_,
                    std::move(applied_articulations), 0);

    // [THEN] We expect the nominal duration of a note to be unchanged
    EXPECT_EQ(event.arrangement_ctx().nominal_duration, nominal_duration_);

    // [THEN] We expect the actual duration of the note marked by staccato + tenuto to be equal to 0.75 of nominal duration
    EXPECT_EQ(event.arrangement_ctx().actual_duration, nominal_duration_ * 0.75);
}

/**
 * @brief MPE_SingleNoteArticulationsTest_QuickFall
 * @details In this case we're gonna build a simple note event with the quick fall articulation
 *          applied on the top of it. So the actual pitch context of the note would reflect the
 *          parameters from articulation pattern
 */
TEST_F(MpeSingleNoteArticulationsTest, QuickFall) {
    // [GIVEN] Articulation pattern "Tenuto", which instructs a performer to play a note for the whole nominal duration
    ArticulationPatternSegment quick_fall_pattern;
    quick_fall_pattern.arrangement_pattern =
        create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);

    // Linear decreasing pitch
    quick_fall_pattern.pitch_pattern = create_simple_pitch_pattern(
        -::muse::mpe::pitch_level_step / (::muse::mpe::max_pitch_level / ::muse::mpe::ten_percent) /*increment_pitch_diff*/);
    quick_fall_pattern.expression_pattern =
        create_simple_expression_pattern(nominal_dynamic_ /* no dynamic changes comparing to the standard one*/);

    ArticulationPattern quick_fall_scope;
    quick_fall_scope.emplace(0, quick_fall_pattern);

    // [GIVEN] Staccato articulation applied on the note, since staccato is a single-note articulation
    //         occupied range is from 0% to 100%
    ArticulationMeta quick_fall_meta;
    quick_fall_meta.type = ArticulationType::QUICK_FALL;
    quick_fall_meta.pattern = quick_fall_scope;
    quick_fall_meta.timestamp = nominal_timestamp_;
    quick_fall_meta.overall_duration = nominal_duration_;

    ArticulationAppliedData quick_fall_applied(std::move(quick_fall_meta), 0, ::muse::mpe::hundred_percent);

    ArticulationMap applied_articulations = {};
    applied_articulations.emplace(ArticulationType::QUICK_FALL, std::move(quick_fall_applied));
    applied_articulations.pre_calculate_average_data();

    // [WHEN] Note event with given parameters being built
    NoteEvent event(nominal_timestamp_, nominal_duration_, voice_idx_, staff_idx_, pitch_level(pitch_class_, octave_), nominal_dynamic_,
                    std::move(applied_articulations), 0);

    // [THEN] We expect the pitch curve of the note marked by quick fall articulation to be equal to the pitch curve from corresponding pattern
    EXPECT_EQ(event.pitch_ctx().pitch_curve, quick_fall_pattern.pitch_pattern.pitch_offset_map);
}

/**
 * @brief MPE_SingleNoteArticulationsTest_Scoop
 * @details In this case we're gonna build a simple note event with the scoop articulation
 *          applied on the top of it. So the actual pitch context of the note would reflect the
 *          parameters from articulation pattern
 */
TEST_F(MpeSingleNoteArticulationsTest, Scoop) {
    // [GIVEN] Articulation pattern "Scoop", which instructs a performer to play ahead of a nominal duration,
    //         starting at a lower pitch, and then placing it on the note being played.
    duration_percentage_t timestamp_offset = static_cast<duration_percentage_t>(-2.5 * ::muse::mpe::ten_percent);
    nominal_timestamp_ = 1000; //msecs

    ArticulationPatternSegment scoop_pattern;
    scoop_pattern.arrangement_pattern =
        create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, timestamp_offset /*timestamp_offset*/);

    // Linear increasing pitch
    scoop_pattern.pitch_pattern = create_simple_pitch_pattern(ArticulationMap::expected_size /*increment_pitch_diff*/);
    scoop_pattern.expression_pattern =
        create_simple_expression_pattern(nominal_dynamic_ /* no dynamic changes comparing to the standard one*/);

    ArticulationPattern scope;
    scope.emplace(0, scoop_pattern);

    // [GIVEN] Staccato articulation applied on the note, since staccato is a single-note articulation
    //         occupied range is from 0% to 100%
    ArticulationMeta scoop_meta;
    scoop_meta.type = ArticulationType::SCOOP;
    scoop_meta.pattern = scope;
    scoop_meta.timestamp = nominal_timestamp_;
    scoop_meta.overall_duration = nominal_duration_;

    ArticulationAppliedData scoop_applied(std::move(scoop_meta), 0, ::muse::mpe::hundred_percent);

    ArticulationMap applied_articulations = {};
    applied_articulations.emplace(ArticulationType::SCOOP, std::move(scoop_applied));
    applied_articulations.pre_calculate_average_data();

    // [WHEN] Note event with given parameters being built
    NoteEvent event(nominal_timestamp_, nominal_duration_, voice_idx_, staff_idx_, pitch_level(pitch_class_, octave_), nominal_dynamic_,
                    std::move(applied_articulations), 0);

    // [THEN] We expect the pitch curve of the note marked by scoop articulation to be equal to the pitch curve from corresponding pattern
    EXPECT_EQ(event.pitch_ctx().pitch_curve, scoop_pattern.pitch_pattern.pitch_offset_map);

    // [THEN] We expect that actual timestamp of the note will consider timestamp offset from the articulation pattern
    //        In other words, we'll start to playback a note with pitch offset and then finally land on the note being played
    EXPECT_EQ(event.arrangement_ctx().actual_timestamp, nominal_timestamp_ + nominal_duration_ * percentage_to_factor(timestamp_offset));
}
