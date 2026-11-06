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
#include <memory>

#include "mpe/tests/utils/articulationutils.h"

#include "engraving/dom/note.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tremolotwochord.h"

#include "engraving/playback/playbackeventsrenderer.h"

#include "utils/scorerw.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static const String playback_events_rendering_dir(u"playback/playbackeventsrenderer_data/");
static constexpr duration_t quarter_note_duration = 500000;                               // duration in microseconds for 4/4 120BPM
static constexpr duration_t quaver_note_duration = ::quarter_note_duration / 2;           // duration in microseconds for 4/4 120BPM
static constexpr duration_t semi_quaver_note_duration = ::quaver_note_duration / 2;       // duration in microseconds for 4/4 120BPM
static constexpr duration_t demi_semi_quaver_note_duration = ::quarter_note_duration / 8; // duration in microseconds for 4/4 120BPM
static constexpr duration_t half_note_duration = ::quarter_note_duration * 2;             // duration in microseconds for 1/2 120BPM
static constexpr duration_t whole_note_duration = ::quarter_note_duration * 4;            // duration in microseconds for 4/4 120BPM

class EngravingPlaybackEventsRendererTests : public ::testing::Test {
protected:
    void SetUp() override {
        dummy_pattern_segment_.arrangement_pattern =
            tests::create_arrangement_pattern(::muse::mpe::hundred_percent /*duration_factor*/, 0 /*timestamp_offset*/);
        dummy_pattern_segment_.pitch_pattern = tests::create_simple_pitch_pattern(0 /*increment_pitch_diff*/);
        dummy_pattern_segment_.expression_pattern =
            tests::create_simple_expression_pattern(dynamic_level_from_type(mpe::DynamicType::NATURAL));
        dummy_pattern_.emplace(0, dummy_pattern_segment_);

        default_profile_ = std::make_shared<ArticulationsProfile>();
    }

    const Chord *find_chord(const Score *score, int tick, track_idx_t track = 0) const {
        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            if (!mb->is_measure()) {
                continue;
            }

            const Chord *chord = to_measure(mb)->find_chord(Fraction::from_ticks(tick), track);
            if (chord) {
                return chord;
            }
        }

        return nullptr;
    }

    ArticulationsProfilePtr default_profile_ = nullptr;

    ArticulationPattern dummy_pattern_;
    ArticulationPatternSegment dummy_pattern_segment_;

    PlaybackEventsRenderer renderer_;
};

/**
 * @brief PlaybackEventsRendererTests_SingleNote_TenutoAccent
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note marked by TENUTO-ACCENT articulation.
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_TenutoAccent) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_tenuto_accent/tenuto_accent.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ACCENT, dummy_pattern_);
    default_profile_->set_pattern(ArticulationType::TENUTO, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] We expect that a single note event will be rendered from the chord
    EXPECT_EQ(result.size(), 1);

    mpe::NoteEvent event = std::get<mpe::NoteEvent>(result.begin()->second.front());

    // [THEN] We expect that the note event will match time expectations of the very first quarter note with 120BPM tempo
    EXPECT_EQ(event.arrangement_ctx().nominal_timestamp, 0);
    EXPECT_EQ(event.arrangement_ctx().nominal_duration, ::quarter_note_duration);

    // [THEN] We expect that the note event will match pitch expectations of F4 quarter note
    EXPECT_EQ(event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4));

    // [THEN] We expect that the note event will match expression expectations of our note
    EXPECT_EQ(event.expression_ctx().nominal_dynamic_level, dynamic_level_from_type(mpe::DynamicType::NATURAL));
    EXPECT_EQ(event.expression_ctx().articulations.size(), 2);
    EXPECT_TRUE(event.expression_ctx().articulations.contains(ArticulationType::TENUTO));
    EXPECT_TRUE(event.expression_ctx().articulations.contains(ArticulationType::ACCENT));
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_NoArticulations
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note without any articulations applied
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_NoArticulations) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_no_articulations/no_articulations.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] We expect that a single note event will be rendered from the chord
    EXPECT_EQ(result.size(), 1);

    mpe::NoteEvent event = std::get<mpe::NoteEvent>(result.begin()->second.front());

    // [THEN] We expect that the note event will match time expectations of the very first quarter note with 120BPM tempo
    EXPECT_EQ(event.arrangement_ctx().nominal_timestamp, 0);
    EXPECT_EQ(event.arrangement_ctx().nominal_duration, ::quarter_note_duration);

    // [THEN] We expect that the note event will match pitch expectations of F4 quarter note
    EXPECT_EQ(event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4));

    // [THEN] We expect that the note event will match expression expectations of our note
    EXPECT_EQ(event.expression_ctx().nominal_dynamic_level, dynamic_level_from_type(mpe::DynamicType::NATURAL));
    EXPECT_EQ(event.expression_ctx().articulations.size(), 1);
    EXPECT_TRUE(event.expression_ctx().articulations.contains(ArticulationType::STANDARD));
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Trill_Modern
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note marked by Trill articulation.
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Trill_Modern) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_trill_default_tempo/single_note_trill_default_tempo.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected trill disclosure
    int expected_trill_sub_notes_count = 8;

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TRILL, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_trill_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Trill
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TRILL));

            // [THEN] The amount of trill alterations depends on many things, such as a tempo
            //        However, there is a couple of general rules of this disclosure:
            //        - Trill should start from a principal note
            //        - Trill should end up on a principal note. For that reasons is highly common to put a triplet on the last notes
            if (i == 0 || i == result.size() - 1) {
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4));
            }

            // [THEN] In modern trills each even note should be higher than the principal one on a single diatonic step
            if ((i + 1) % 2 == 0) {
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4) + 2 * ::muse::mpe::pitch_level_step);
            }
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Unexpandable_Trill
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 32-nd note marked by Trill articulation.
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Unexpandable_Trill) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_unexpandable_trill/single_note_unexpandable_trill.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected trill disclosure
    int expected_trill_sub_notes_count = 1;

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TRILL, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] We expect that the only one note event will be rendered,
    //        since Trill consists of 32-nd note, we can't break down 32-nd principal note into smaller sub-notes
    EXPECT_EQ(result.size(), expected_trill_sub_notes_count);
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Trill_Baroque
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note marked by Trill(Baroque) articulation.
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Trill_Baroque) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_trill_baroque/single_note_trill_baroque.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected trill disclosure
    int expected_trill_sub_notes_count = 10;

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TRILL_BAROQUE, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_trill_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - TrillBaroque
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TRILL_BAROQUE));

            // [THEN] The amount of trill alterations depends on various things, such as a tempo
            //        However, there is a couple of general rules of this disclosure in Baroque style:
            //        - Trill should start from a principal note + 1 diatonic step
            //        - Trill should end up on a principal note. For that reasons is highly common to put a triplet on the last notes
            if (i == 0) {
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4) + 2 * ::muse::mpe::pitch_level_step);
            }

            if (i == pair.second.size() - 1) {
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4));
            }

            // [THEN] In baroque trills each odd note should be higher than the principal one on a single diatonic step
            if (i % 2 == 0 && i < pair.second.size() - 2) {
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4) + 2 * ::muse::mpe::pitch_level_step);
            }

            // [THEN] However, the note before the last should be lowe than the principal one on a single diatonic step
            if (i == pair.second.size() - 2) {
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4) - ::muse::mpe::pitch_level_step);
            }
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Turn_Regular
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note marked by Turn articulation.
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Turn_Regular) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_regular_turn/single_note_regular_turn.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 4;
    duration_t expected_sub_note_duration = ::quaver_note_duration / expected_sub_notes_count;
    duration_t expected_principal_note_duration = 312500;

    std::vector<duration_t> expected_durations = {expected_sub_note_duration, expected_sub_note_duration, expected_sub_note_duration,
                                                  expected_principal_note_duration};

    std::vector<timestamp_t> expected_timestamps = {0, expected_sub_note_duration, expected_sub_note_duration * 2,
                                                    expected_sub_note_duration * 3};

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::F, 4);
    pitch_level_t plus = nominal_pitch_level + 2 * ::muse::mpe::pitch_level_step;
    pitch_level_t minus = nominal_pitch_level - ::muse::mpe::pitch_level_step;
    std::vector<pitch_level_t> expected_pitches = {plus, nominal_pitch_level, minus, nominal_pitch_level};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TURN, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Turn
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TURN));

            // [THEN] We expect that each sub-note in Regular Turn articulation will be equal to 0.25 of the principal note
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations[i]);
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps[i]);

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Turn_Inverted
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note marked by Inverted Turn articulation.
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Turn_Inverted) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_inverted_turn/single_note_inverted_turn.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 4;
    duration_t expected_sub_note_duration = ::quaver_note_duration / expected_sub_notes_count;
    duration_t expected_principal_note_duration = 312500;

    std::vector<duration_t> expected_durations = {expected_sub_note_duration, expected_sub_note_duration, expected_sub_note_duration,
                                                  expected_principal_note_duration};

    std::vector<timestamp_t> expected_timestamps = {0, expected_sub_note_duration, expected_sub_note_duration * 2,
                                                    expected_sub_note_duration * 3};

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::F, 4);
    pitch_level_t plus = nominal_pitch_level + 2 * ::muse::mpe::pitch_level_step;
    pitch_level_t minus = nominal_pitch_level - ::muse::mpe::pitch_level_step;
    std::vector<pitch_level_t> expected_pitches = {minus, nominal_pitch_level, plus, nominal_pitch_level};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::INVERTED_TURN, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - InvertedTurn
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::INVERTED_TURN));

            // [THEN] We expect that each sub-note in Inverted Turn articulation will be equal to 0.25 of the principal note
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations[i]);
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps[i]);

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Turn_Inverted_Slash_Variation
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note marked by "Turn with slash" articulation,
 *          which is just a variation of Inverted Turn articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Turn_Inverted_Slash_Variation) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir +
                                       u"single_note_inverted_turn_slash_variation/single_note_inverted_turn_slash_variation.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 4;
    duration_t expected_sub_note_duration = ::quaver_note_duration / expected_sub_notes_count;
    duration_t expected_principal_note_duration = 312500;

    std::vector<duration_t> expected_durations = {expected_sub_note_duration, expected_sub_note_duration, expected_sub_note_duration,
                                                  expected_principal_note_duration};

    std::vector<timestamp_t> expected_timestamps = {0, expected_sub_note_duration, expected_sub_note_duration * 2,
                                                    expected_sub_note_duration * 3};

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::F, 4);
    pitch_level_t plus = nominal_pitch_level + 2 * ::muse::mpe::pitch_level_step;
    pitch_level_t minus = nominal_pitch_level - ::muse::mpe::pitch_level_step;
    std::vector<pitch_level_t> expected_pitches = {minus, nominal_pitch_level, plus, nominal_pitch_level};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::INVERTED_TURN, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - InvertedTurn
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::INVERTED_TURN));

            // [THEN] We expect that each sub-note in Inverted Turn articulation will be equal to 0.25 of the principal note
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations[i]);
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps[i]);

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Upper_Mordent
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note marked by "Upper Mordent" articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Upper_Mordent) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_upper_mordent/single_note_upper_mordent.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;

    std::vector<duration_t> expected_durations = {::demi_semi_quaver_note_duration, ::demi_semi_quaver_note_duration,
                                                  3 * ::quarter_note_duration / 4};

    std::vector<timestamp_t> expected_timestamps = {0, ::demi_semi_quaver_note_duration, 2 * ::demi_semi_quaver_note_duration};

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::F, 4);
    pitch_level_t plus = nominal_pitch_level + 2 * ::muse::mpe::pitch_level_step;

    std::vector<pitch_level_t> expected_pitches = {nominal_pitch_level, plus, nominal_pitch_level};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::UPPER_MORDENT, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Upper Mordent
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::UPPER_MORDENT));

            // [THEN] We expect that each sub-note in Upper Mordent articulation has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Lower_Mordent
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note marked by "Lower Mordent" articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Lower_Mordent) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_lower_mordent/single_note_lower_mordent.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;

    std::vector<duration_t> expected_durations = {::demi_semi_quaver_note_duration, ::demi_semi_quaver_note_duration,
                                                  3 * ::quarter_note_duration / 4};

    std::vector<timestamp_t> expected_timestamps = {0, ::demi_semi_quaver_note_duration, 2 * ::demi_semi_quaver_note_duration};

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::F, 4);
    pitch_level_t minus = nominal_pitch_level - ::muse::mpe::pitch_level_step;

    std::vector<pitch_level_t> expected_pitches = {nominal_pitch_level, minus, nominal_pitch_level};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::LOWER_MORDENT, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Lower Mordent
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::LOWER_MORDENT));

            // [THEN] We expect that each sub-note has an expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_TwoNotes_Discrete_Chromatic_Glissando
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with F4 and B4 quarter notes connected by Discrete Glissando articulation.
 */
TEST_F(EngravingPlaybackEventsRendererTests, TwoNotes_Discrete_Glissando) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"two_notes_discrete_glissando/two_notes_discrete_glissando.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected glissando disclosure
    size_t expected_sub_notes_count = pitch_steps_count(std::abs(pitch_level_diff(PitchClass::F, 4, PitchClass::B, 4)));

    float expected_duration = static_cast<float>(::quarter_note_duration) / expected_sub_notes_count;

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::F, 4);

    std::vector<pitch_level_t> expected_pitches;
    for (size_t i = 0; i < expected_sub_notes_count; ++i) {
        expected_pitches.push_back(nominal_pitch_level + static_cast<int>(i) * ::muse::mpe::pitch_level_step);
    }

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::DISCRETE_GLISSANDO, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Discrete Glissando
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::DISCRETE_GLISSANDO));

            // [THEN] We expect that each sub-note in Discrete Glissando articulation has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, static_cast<int>(expected_duration));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, static_cast<int>(i * expected_duration));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_TwoNotes_Continuous_Glissando
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with F4 and B4 quarter notes connected by ContinuousGlissando articulation.
 *          This measure is then repeated, see: https://github.com/musescore/MuseScore/issues/27287
 */
TEST_F(EngravingPlaybackEventsRendererTests, TwoNotes_Continuous_Glissando) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"two_notes_continuous_glissando/two_notes_continuous_glissando.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->repeat_list().size(), 2);

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected glissando disclosure
    int expected_sub_notes_count = 1;

    duration_t expected_duration = ::quarter_note_duration;

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::F, 4);

    std::vector<pitch_level_t> expected_pitches;
    for (int i = 0; i < expected_sub_notes_count; ++i) {
        expected_pitches.push_back(nominal_pitch_level + i * ::muse::mpe::pitch_level_step);
    }

    std::vector<timestamp_t> expected_timestamps{
        0,
        ::whole_note_duration, // 2nd repeat segment
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::CONTINUOUS_GLISSANDO, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    for (const RepeatSegment *repeat_segment : score->repeat_list()) {
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;
        renderer_.render(chord, tick_position_offset, default_profile_, ctx, result);
    }

    // [THEN] 2 events, as we have 2 repeat segments
    EXPECT_EQ(result.size(), 2);
    ASSERT_EQ(expected_timestamps.size(), result.size());

    size_t note_event_num = 0;

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each sub-note has an expected duration
            timestamp_t expected_timestamp = expected_timestamps.at(note_event_num);
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_duration);
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamp);

            // [THEN] We expect that each note event has only one articulation applied - Continuous Glissando
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            auto gliss_it = note_event.expression_ctx().articulations.find(ArticulationType::CONTINUOUS_GLISSANDO);
            ASSERT_TRUE(gliss_it != note_event.expression_ctx().articulations.end());

            // [THEN] The articulation has the correct timestamp & duration
            const ArticulationMeta &art_meta = gliss_it->second.meta;
            EXPECT_EQ(art_meta.timestamp, expected_timestamp);
            EXPECT_EQ(art_meta.overall_duration, expected_duration);

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));

            note_event_num++;
        }
    }

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_TwoNotes_Glissando_NoPlay
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with F4 and B4 quarter notes connected by ContinuousGlissando articulation.
 *          However, the glissando has been marked as "no play", so we'll render the Standard articulation instead
 */
TEST_F(EngravingPlaybackEventsRendererTests, TwoNotes_Glissando_NoPlay) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir +
                                       u"two_notes_continuous_glissando_no_play/two_notes_continuous_glissando_no_play.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected glissando disclosure
    int expected_sub_notes_count = 1;

    duration_t expected_duration = ::quarter_note_duration;

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::F, 4);

    std::vector<pitch_level_t> expected_pitches;
    for (int i = 0; i < expected_sub_notes_count; ++i) {
        expected_pitches.push_back(nominal_pitch_level + i * ::muse::mpe::pitch_level_step);
    }

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Standard articulation
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));

            // [THEN] We expect that each sub-note has an expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_duration);
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, static_cast<duration_t>(i) * expected_duration);

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_TwoNotes_Discrete_Harp_Glissando
 * @details Render a score with 3 measures containing whole notes (C4, C5, C4) and glissandos
 *          between them.  Add harp pedal diagrams to the first and second measures set to a
 *          whole tone scale, then back to Cmaj.  Check the glisses render as expected
 */
TEST_F(EngravingPlaybackEventsRendererTests, TwoNotes_Discrete_Harp_Glissando) {
    // [GIVEN]
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"TwoNotes_Discrete_Harp_Glissando/TwoNotes_Discrete_Harp_Glissando.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    Measure *second_measure = first_measure->next_measure();
    ASSERT_TRUE(second_measure);

    Segment *second_segment = second_measure->segments().first_cr_segment();
    ASSERT_TRUE(second_segment);

    ChordRest *second_chord = second_segment->next_chord_rest(0);
    ASSERT_TRUE(second_chord);

    // TEST FIRST GLISS

    // [GIVEN] Expected glissando disclosure
    size_t expected_sub_notes_count = 8;

    float expected_duration = static_cast<float>(::whole_note_duration) / expected_sub_notes_count;

    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::C, 4);
    std::vector<int> pitches_wt = {0, 0, 2, 4, 6, 8, 10, 12};

    std::vector<pitch_level_t> expected_pitches;
    for (size_t i = 0; i < expected_sub_notes_count; ++i) {
        expected_pitches.push_back(nominal_pitch_level + pitches_wt.at(i) * ::muse::mpe::pitch_level_step);
    }

    default_profile_->set_pattern(ArticulationType::DISCRETE_GLISSANDO, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Discrete Glissando
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::DISCRETE_GLISSANDO));

            // [THEN] We expect that each sub-note in Discrete Glissando articulation has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, static_cast<int>(expected_duration));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, static_cast<int>(i * expected_duration));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }

    // TEST SECOND GLISS

    // [GIVEN] Expected glissando disclosure
    expected_sub_notes_count = 4;
    expected_duration = static_cast<float>(::whole_note_duration) / expected_sub_notes_count;

    nominal_pitch_level = pitch_level(PitchClass::C, 5);
    std::vector<int> pitches2 = {0, 1, 3, 6};

    expected_pitches.clear();
    for (size_t i = 0; i < expected_sub_notes_count; ++i) {
        expected_pitches.push_back(nominal_pitch_level - pitches2.at(i) * ::muse::mpe::pitch_level_step);
    }

    default_profile_->set_pattern(ArticulationType::DISCRETE_GLISSANDO, dummy_pattern_);

    PlaybackEventsMap result2;
    renderer_.render(second_chord, 0, default_profile_, ctx, result2);

    for (const auto &pair : result2) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Discrete Glissando
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::DISCRETE_GLISSANDO));

            // [THEN] We expect that each sub-note in Discrete Glissando articulation has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, static_cast<int>(expected_duration));

            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, ::whole_note_duration + static_cast<int>(i * expected_duration));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Glissando_on_tied_notes
 * @details Make sure we render discrete/continuous glissando on tied notes
 */
TEST_F(EngravingPlaybackEventsRendererTests, Glissando_on_tied_notes) {
    // [GIVEN]
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"glissando_on_tied_notes/glissando_on_tied_notes.mscx");

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::DISCRETE_GLISSANDO, dummy_pattern_);
    default_profile_->set_pattern(ArticulationType::CONTINUOUS_GLISSANDO, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // ###################################################
    // 1st case: Discrete glissando on a tied note
    // ###################################################
    const Chord *chord = find_chord(score, 960, 0); // first tied A4
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);
    ASSERT_TRUE(chord->notes().front()->tie_for());

    // [WHEN] Request to render a chord with the A4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] Note event has the correct duration and standard articulation (no glissando)
    pitch_level_t nominal_pitch_level = pitch_level(PitchClass::A, 4);
    duration_t glissando_note_duration = ::quarter_note_duration / 4;

    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.begin()->second.size(), 1);
    mpe::NoteEvent resulting_note_event = std::get<mpe::NoteEvent>(result.begin()->second.at(0));
    EXPECT_EQ(resulting_note_event.expression_ctx().articulations.size(), 1);
    EXPECT_TRUE(resulting_note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));
    EXPECT_EQ(resulting_note_event.pitch_ctx().nominal_pitch_level, nominal_pitch_level);
    EXPECT_EQ(resulting_note_event.arrangement_ctx().nominal_duration,
              ::quarter_note_duration + glissando_note_duration); // A4 + 1st glissando note

    // [GIVEN] Tied A4 with discrete glissando
    chord = find_chord(score, 1440, 0);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);
    ASSERT_TRUE(chord->notes().front()->tie_back());

    // [WHEN] Request to render a chord with the A4 note on it
    result.clear();
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] Expected glissando disclosure
    size_t expected_sub_notes_count = 3;
    mpe::timestamp_t expected_timestamp = timestamp_from_ticks(score, chord->tick().ticks()) + glissando_note_duration;
    std::vector<int> pitches_wt = {-1, -2, -3};

    std::vector<pitch_level_t> expected_pitches;
    for (size_t i = 0; i < expected_sub_notes_count; ++i) {
        expected_pitches.push_back(nominal_pitch_level + pitches_wt.at(i) * ::muse::mpe::pitch_level_step);
    }

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Discrete Glissando
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::DISCRETE_GLISSANDO));

            // [THEN] We expect that each sub-note in Discrete Glissando articulation has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, glissando_note_duration);
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamp + static_cast<int>(i * glissando_note_duration));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }

    // ###################################################
    // 2nd case: Continuous glissando on a tied note
    // ###################################################
    chord = find_chord(score, 4800, 0); // tied A4 with continuous glissando
    ASSERT_TRUE(chord);
    ASSERT_FALSE(chord->notes().empty());
    ASSERT_TRUE(chord->notes().front()->tie_for());

    // [WHEN] Request to render a chord with the A4 note on it
    result.clear();
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] Expected glissando disclosure
    expected_sub_notes_count = 1;
    expected_timestamp = timestamp_from_ticks(score, chord->tick().ticks());
    glissando_note_duration = ::quarter_note_duration * 2;
    nominal_pitch_level = pitch_level(PitchClass::A, 4);

    expected_pitches.clear();
    for (size_t i = 0; i < expected_sub_notes_count; ++i) {
        expected_pitches.push_back(nominal_pitch_level + int(i) * ::muse::mpe::pitch_level_step);
    }

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied - Continuous Glissando
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::CONTINUOUS_GLISSANDO));

            // [THEN] We expect that each sub-note has an expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, glissando_note_duration);
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp,
                      expected_timestamp + static_cast<duration_t>(i) * glissando_note_duration);

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Acciaccatura
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note prepended with G4 8-th acciaccatura note
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Acciaccatura) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_acciaccatura/single_note_acciaccatura.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 2;

    std::vector<duration_t> expected_durations = {::demi_semi_quaver_note_duration / 2,
                                                  ::quarter_note_duration - (::demi_semi_quaver_note_duration / 2)};

    std::vector<timestamp_t> expected_timestamps = {0, ::demi_semi_quaver_note_duration / 2};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::G, 4),
        pitch_level(PitchClass::F, 4),
    };

    std::vector<ArticulationType> expected_articulations = {
        ArticulationType::ACCIACCATURA,
        ArticulationType::STANDARD,
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ACCIACCATURA, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that Acciaccatura is only applied to the grace note event
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(expected_articulations.at(i)));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_AcciaccaturaChord
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note prepended with G4 B4 D5 F5 8th acciaccatura chord
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_AcciaccaturaChord) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_acciaccatura_chord/single_note_acciaccatura_chord.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 5;

    std::vector<duration_t> expected_durations = {::demi_semi_quaver_note_duration / 2, ::demi_semi_quaver_note_duration / 2,
                                                  ::demi_semi_quaver_note_duration / 2, ::demi_semi_quaver_note_duration / 2,
                                                  ::quarter_note_duration - (::demi_semi_quaver_note_duration / 2)};

    std::vector<timestamp_t> expected_timestamps = {0, 0, 0, 0, ::demi_semi_quaver_note_duration / 2};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::G, 4), pitch_level(PitchClass::B, 4), pitch_level(PitchClass::D, 5),
        pitch_level(PitchClass::F, 5), pitch_level(PitchClass::F, 4),
    };

    std::vector<ArticulationType> expected_articulations = {
        ArticulationType::ACCIACCATURA, ArticulationType::ACCIACCATURA, ArticulationType::ACCIACCATURA,
        ArticulationType::ACCIACCATURA, ArticulationType::STANDARD,
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ACCIACCATURA, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that Acciaccatura is only applied to the grace note events
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(expected_articulations.at(i)));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_MultiAcciaccatura
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note prepended with A4+G4 8-th acciaccatura notes
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_MultiAcciaccatura) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_multi_acciaccatura/single_note_multi_acciaccatura.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;

    std::vector<duration_t> expected_durations = {::demi_semi_quaver_note_duration / 2, ::demi_semi_quaver_note_duration / 2,
                                                  ::quarter_note_duration - ::demi_semi_quaver_note_duration};

    std::vector<timestamp_t> expected_timestamps = {0, ::demi_semi_quaver_note_duration / 2, ::demi_semi_quaver_note_duration};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::A, 4),
        pitch_level(PitchClass::G, 4),
        pitch_level(PitchClass::F, 4),
    };

    std::vector<ArticulationType> expected_articulations = {
        ArticulationType::ACCIACCATURA,
        ArticulationType::ACCIACCATURA,
        ArticulationType::STANDARD,
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ACCIACCATURA, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that Acciaccatura is only applied to the grace note events
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(expected_articulations.at(i)));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_GraceNoteWithTiedNotes
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter grace note, followed by the G4 half note and tied to another half note
 */
TEST_F(EngravingPlaybackEventsRendererTests, GraceNoteWithTiedNotes) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"grace_note_with_tied_notes/grace_note_with_tied_notes.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 2;

    std::vector<duration_t> expected_durations = {
        ::half_note_duration / 2,                            // PreAppoggiatura, quarter note
        2 * ::half_note_duration - ::half_note_duration / 2, // 2 * half note duration - grace note duration
    };

    std::vector<timestamp_t> expected_timestamps = {0, ::half_note_duration / 2};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::G, 4),
        pitch_level(PitchClass::G, 4),
    };

    std::vector<ArticulationType> expected_articulations = {
        ArticulationType::PRE_APPOGGIATURA,
        ArticulationType::STANDARD,
        ArticulationType::STANDARD,
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::PRE_APPOGGIATURA, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord with the F4 note on it
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] The events map is not empty
    EXPECT_FALSE(result.empty());

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that PreAppoggiatura is only applied to the grace note event
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(expected_articulations.at(i)));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_Appoggiatura_Post
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note prepended with G4 8-th appoggiatura note
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_Appoggiatura_Post) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_appoggiatura_post/single_note_appoggiatura_post.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 2;

    std::vector<duration_t> expected_durations = {::quaver_note_duration, ::quaver_note_duration};

    std::vector<timestamp_t> expected_timestamps = {0, ::quaver_note_duration};

    std::vector<pitch_level_t> expected_pitches = {pitch_level(PitchClass::F, 4), pitch_level(PitchClass::G, 4)};

    std::vector<ArticulationType> expected_articulations = {
        ArticulationType::STANDARD,
        ArticulationType::POST_APPOGGIATURA,
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::POST_APPOGGIATURA, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that PostAppoggiatura is only applied to the grace note event
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(expected_articulations.at(i)));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_AppoggiaturaChord_Post
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note prepended with G4 B4 D5 F5 8th appoggiatura chord
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_AppoggiaturaChord_Post) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir +
                                       u"single_note_appoggiatura_chord_post/single_note_appoggiatura_chord_post.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 5;

    std::vector<duration_t> expected_durations = {::quaver_note_duration, ::quaver_note_duration, ::quaver_note_duration,
                                                  ::quaver_note_duration, ::quaver_note_duration};

    std::vector<timestamp_t> expected_timestamps = {0, ::quaver_note_duration, ::quaver_note_duration, ::quaver_note_duration,
                                                    ::quaver_note_duration};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::F, 4), pitch_level(PitchClass::G, 4), pitch_level(PitchClass::B, 4),
        pitch_level(PitchClass::D, 5), pitch_level(PitchClass::F, 5),
    };

    std::vector<ArticulationType> expected_articulations = {
        ArticulationType::STANDARD,          ArticulationType::POST_APPOGGIATURA, ArticulationType::POST_APPOGGIATURA,
        ArticulationType::POST_APPOGGIATURA, ArticulationType::POST_APPOGGIATURA,
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::POST_APPOGGIATURA, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that PostAppoggiatura is only applied to the grace note events
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(expected_articulations.at(i)));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_SingleNote_MultiAppoggiatura_Post
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 quarter note prepended with G4 16-th and A4 32-nd appoggiatura notes
 */
TEST_F(EngravingPlaybackEventsRendererTests, SingleNote_MultiAppoggiatura_Post) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir +
                                       u"single_note_multi_appoggiatura_post/single_note_multi_appoggiatura_post.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;

    std::vector<duration_t> expected_durations = {::quarter_note_duration - ::semi_quaver_note_duration - ::demi_semi_quaver_note_duration,
                                                  ::semi_quaver_note_duration, ::demi_semi_quaver_note_duration};

    std::vector<timestamp_t> expected_timestamps = {
        0, ::quarter_note_duration - ::semi_quaver_note_duration - ::demi_semi_quaver_note_duration,
        ::quarter_note_duration - ::demi_semi_quaver_note_duration};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::G, 4),
        pitch_level(PitchClass::A, 4),
    };

    std::vector<ArticulationType> expected_articulations = {
        ArticulationType::STANDARD,
        ArticulationType::POST_APPOGGIATURA,
        ArticulationType::POST_APPOGGIATURA,
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::POST_APPOGGIATURA, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that PostAppoggiatura is only applied to the grace note events
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(expected_articulations.at(i)));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamps.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Chord_Arpeggio
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4+A4+C4 16th chord marked by the Arpeggio articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Chord_Arpeggio) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"chord_arpeggio/chord_arpeggio.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;
    int expected_offset = ::semi_quaver_note_duration / 3;

    std::vector<timestamp_t> expected_timestamp = {
        0,
        expected_offset,
        expected_offset * 2,
    };

    std::vector<duration_t> expected_durations = {::semi_quaver_note_duration, ::semi_quaver_note_duration - expected_offset,
                                                  ::semi_quaver_note_duration - expected_offset * 2};

    std::vector<pitch_level_t> expected_pitches = {pitch_level(PitchClass::F, 4), pitch_level(PitchClass::A, 4),
                                                   pitch_level(PitchClass::C, 5)};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ARPEGGIO, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::ARPEGGIO));

            // [THEN] We expect that each sub-note has expected timestamp and duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamp.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Chord_Arpeggio_Grace
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the E4 acciaccatura followed by the F4+A4+C5 quarter chord
 *          marked by the Arpeggio articulation. The grace chord has no arpeggio of its own,
 *          so it must be rendered as a plain chord
 */
TEST_F(EngravingPlaybackEventsRendererTests, Chord_Arpeggio_Grace) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"chord_arpeggio_grace/chord_arpeggio_grace.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure: 1 grace note + 3 arpeggiated notes
    std::vector<pitch_level_t> expected_pitches = {pitch_level(PitchClass::E, 4), pitch_level(PitchClass::F, 4),
                                                   pitch_level(PitchClass::A, 4), pitch_level(PitchClass::C, 5)};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ARPEGGIO, dummy_pattern_);
    default_profile_->set_pattern(ArticulationType::ACCIACCATURA, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] The grace note is rendered along with the arpeggiated notes
        ASSERT_EQ(pair.second.size(), expected_pitches.size());

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Chord_Arpeggio_Up
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4+A4+C4 quarter chord marked by the Arpeggio Up articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Chord_Arpeggio_Up) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"chord_arpeggio_up/chord_arpeggio_up.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;
    int expected_offset = 60000;

    std::vector<timestamp_t> expected_timestamp = {
        0,
        expected_offset,
        expected_offset * 2,
    };

    std::vector<duration_t> expected_durations = {::quarter_note_duration, ::quarter_note_duration - expected_offset,
                                                  ::quarter_note_duration - expected_offset * 2};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::A, 4),
        pitch_level(PitchClass::C, 5),
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ARPEGGIO_UP, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::ARPEGGIO_UP));

            // [THEN] We expect that each sub-note has expected timestamp and duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamp.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief Chord_Arpeggio_TiedNotes
 * @details In this case we're gonna render a simple piece of score with 3 measures,
 *          which starts with the B4+D5 half chord marked by the Arpeggio Up articulation
 *          Check that Arpeggio gets extended by tied notes
 */
TEST_F(EngravingPlaybackEventsRendererTests, Chord_Arpeggio_Up_TiedNotes) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"chord_arpeggio_with_tied_notes/chord_arpeggio_with_tied_notes.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 2;
    int expected_offset = 60000;

    std::vector<timestamp_t> expected_timestamp = {
        0,
        expected_offset,
    };

    std::vector<duration_t> expected_durations = {
        ::half_note_duration * 3,                   // + 2 tied notes
        ::half_note_duration * 3 - expected_offset, // + 2 tied notes
    };

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::B, 4),
        pitch_level(PitchClass::D, 5),
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ARPEGGIO_UP, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::ARPEGGIO_UP));

            // [THEN] We expect that each sub-note has expected duration
            // [THEN] We expect that each sub-note has expected timestamp and duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamp.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Chord_Arpeggio_Down
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4+A4+C4 quarter chord marked by the Arpeggio Down articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Chord_Arpeggio_Down) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"chord_arpeggio_down/chord_arpeggio_down.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;
    int expected_offset = 60000;

    std::vector<timestamp_t> expected_timestamp = {
        0,
        expected_offset,
        expected_offset * 2,
    };

    std::vector<duration_t> expected_durations = {::quarter_note_duration, ::quarter_note_duration - expected_offset,
                                                  ::quarter_note_duration - expected_offset * 2};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::C, 5),
        pitch_level(PitchClass::A, 4),
        pitch_level(PitchClass::F, 4),
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ARPEGGIO_DOWN, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::ARPEGGIO_DOWN));

            // [THEN] We expect that each sub-note has expected timestamp and duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamp.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Chord_Arpeggio_Straight_Down
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4+A4+C4 quarter chord marked by the Arpeggio Straight Down articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Chord_Arpeggio_Straight_Down) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"chord_arpeggio_straight_down/chord_arpeggio_straight_down.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;
    int expected_offset = 60000;

    std::vector<timestamp_t> expected_timestamp = {
        0,
        expected_offset,
        expected_offset * 2,
    };

    std::vector<duration_t> expected_durations = {::quarter_note_duration, ::quarter_note_duration - expected_offset,
                                                  ::quarter_note_duration - expected_offset * 2};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::C, 5),
        pitch_level(PitchClass::A, 4),
        pitch_level(PitchClass::F, 4),
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ARPEGGIO_STRAIGHT_DOWN, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::ARPEGGIO_STRAIGHT_DOWN));

            // [THEN] We expect that each sub-note has expected timestamp and duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamp.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Chord_Arpeggio_Straight_Up
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4+A4+C4 quarter chord marked by the Arpeggio Straight Up articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Chord_Arpeggio_Straight_Up) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"chord_arpeggio_straight_up/chord_arpeggio_straight_up.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;
    int expected_offset = 60000;

    std::vector<timestamp_t> expected_timestamp = {
        0,
        expected_offset,
        expected_offset * 2,
    };

    std::vector<duration_t> expected_durations = {::quarter_note_duration, ::quarter_note_duration - expected_offset,
                                                  ::quarter_note_duration - expected_offset * 2};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::A, 4),
        pitch_level(PitchClass::C, 5),
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ARPEGGIO_STRAIGHT_UP, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::ARPEGGIO_STRAIGHT_UP));

            // [THEN] We expect that each sub-note has expected timestamp and duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_timestamp, expected_timestamp.at(i));
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Chord_Arpeggio_Straight_Up
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4+A4+C4 quarter chord marked by the Arpeggio Bracket, which means that chord should be played
 *          like it has not arpeggio
 */
TEST_F(EngravingPlaybackEventsRendererTests, Chord_Arpeggio_Bracket) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"chord_arpeggio_bracket/chord_arpeggio_bracket.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 3;

    std::vector<duration_t> expected_durations = {::quarter_note_duration, ::quarter_note_duration, ::quarter_note_duration};

    std::vector<pitch_level_t> expected_pitches = {
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::A, 4),
        pitch_level(PitchClass::C, 5),
    };

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_PartiallyTiedArpeggio
 */
TEST_F(EngravingPlaybackEventsRendererTests, PartiallyTiedArpeggio) {
    // [GIVEN] Simple piece of score (violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"partially_tied_arpeggio.mscx");
    ASSERT_TRUE(score);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::ARPEGGIO_UP, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Render the score
    PlaybackEventsMap result;
    for (const RepeatSegment *repeat : score->repeat_list()) {
        const int tick_position_offset = repeat->utick - repeat->tick;

        for (const Measure *m : repeat->measure_list()) {
            for (const Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                const EngravingItem *el = s->element(0);
                if (!el || !el->is_chord()) {
                    continue;
                }

                renderer_.render(to_chord(el), tick_position_offset, default_profile_, ctx, result);
            }
        }
    }

    const std::vector<timestamp_t> expected_arpeggio_timestamps{
        0,       // 1st measure
        3500000, // 2nd measure
        7500000, // 2nd measure (repeated)
    };

    const std::vector<duration_t> expected_arpeggio_durations{
        ::quarter_note_duration,     // 1st measure
        ::quarter_note_duration * 2, // 2nd measure
        ::quarter_note_duration,     // 2nd measure (repeated)
    };

    size_t arpeggio_num = 0;

    for (const auto &pair : result) {
        if (pair.second.empty()) {
            continue;
        }

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const PlaybackEvent &event = pair.second.at(i);
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            ASSERT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::ARPEGGIO_UP));

            // [THEN] Each note event has the correct timestamp & duration
            ASSERT_TRUE(arpeggio_num < expected_arpeggio_timestamps.size());
            ASSERT_TRUE(arpeggio_num < expected_arpeggio_durations.size());

            const duration_t expected_offset = 60000 * i;
            const timestamp_t expected_timestamp = expected_arpeggio_timestamps.at(arpeggio_num) + expected_offset;
            const duration_t expected_duration = expected_arpeggio_durations.at(arpeggio_num) - expected_offset;

            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_timestamp);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_duration);
        }

        arpeggio_num++;
    }

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_Single_Note_Tremolo
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4 half note marked by the 8-th Tremolo articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Single_Note_Tremolo) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_tremolo/single_note_tremolo.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_sub_notes_count = 4;

    std::vector<duration_t> expected_durations = {::quaver_note_duration, ::quaver_note_duration, ::quaver_note_duration,
                                                  ::quaver_note_duration};

    std::vector<pitch_level_t> expected_pitches = {pitch_level(PitchClass::F, 4), pitch_level(PitchClass::F, 4),
                                                   pitch_level(PitchClass::F, 4), pitch_level(PitchClass::F, 4)};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TREMOLO8TH, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TREMOLO8TH));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(i));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(i));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Single_Note_Tremolo_OnTiedNote
 * @details In this case we're gonna render a simple piece of score with 3 measures,
 *          which starts with an A4 half note tied to another A4 half note marked by the 32nd Tremolo articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Single_Note_Tremolo_OnTiedNote) {
    // [GIVEN] Simple piece of score (violin, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_tremolo_on_tied_note/single_note_tremolo_on_tied_note.mscx");
    ASSERT_TRUE(score);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);
    default_profile_->set_pattern(ArticulationType::TREMOLO32ND, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [GIVEN] Chord of the 1st tied note (without tremolo)
    const Chord *chord = find_chord(score, 1920);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);

    const Note *first_tied_note = chord->notes().front();
    ASSERT_TRUE(first_tied_note->tie_for());
    ASSERT_FALSE(first_tied_note->tie_back());

    // [WHEN] Request to render the 1st chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] Note event has normal duration and standard articulation (no tremolo)
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.begin()->second.size(), 1);
    mpe::NoteEvent resulting_note_event = std::get<mpe::NoteEvent>(result.begin()->second.at(0));
    EXPECT_EQ(resulting_note_event.expression_ctx().articulations.size(), 1);
    EXPECT_TRUE(resulting_note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));
    EXPECT_EQ(resulting_note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 4));
    EXPECT_EQ(resulting_note_event.arrangement_ctx().nominal_duration, ::half_note_duration);

    // [GIVEN] Chord of the 2nd tied note (with tremolo)
    chord = find_chord(score, 2880);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);

    const Note *last_tied_note = chord->notes().front();
    ASSERT_TRUE(last_tied_note->tie_back());
    ASSERT_FALSE(last_tied_note->tie_for());

    // [WHEN] Request to render the 2nd chord
    result.clear();
    renderer_.render(chord, 0, default_profile_, ctx, result);

    constexpr int expected_sub_notes_count = 16;
    constexpr mpe::duration_t expected_duration = ::half_note_duration / expected_sub_notes_count;

    for (const auto &pair : result) {
        // [THEN] We expect that rendered note events number will match expectations
        EXPECT_EQ(pair.second.size(), expected_sub_notes_count);

        for (size_t i = 0; i < pair.second.size(); ++i) {
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(pair.second.at(i));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TREMOLO32ND));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_duration);

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 4));
        }
    }

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_Single_Note_Tremolo_OnTiedNote_PlaybackDisabled
 * @details This test verifies that tied notes with tremolo markings are NOT rendered
 *          when the tremolo's playTremolo setting is disabled (false).
 *          This is the fix for issue #30858 - preventing double playback when tremolo is disabled.
 */
TEST_F(EngravingPlaybackEventsRendererTests, Single_Note_Tremolo_OnTiedNote_PlaybackDisabled) {
    // [GIVEN] Simple piece of score (violin, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"single_note_tremolo_on_tied_note/single_note_tremolo_on_tied_note.mscx");
    ASSERT_TRUE(score);

    // [GIVEN] Find the chord with tremolo and disable tremolo playback
    const Chord *chord = find_chord(score, 2880); // Chord with tremolo
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);

    const Note *last_tied_note = chord->notes().front();
    ASSERT_TRUE(last_tied_note->tie_back());
    ASSERT_FALSE(last_tied_note->tie_for());

    // [GIVEN] Disable tremolo playback - this simulates the user unchecking "Play tremolo"
    TremoloSingleChord *tremolo_single = chord->tremolo_single_chord();
    TremoloTwoChord *tremolo_two = chord->tremolo_two_chord();

    if (tremolo_single) {
        tremolo_single->set_play_tremolo(false);
    }
    if (tremolo_two) {
        tremolo_two->set_play_tremolo(false);
    }

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);
    default_profile_->set_pattern(ArticulationType::TREMOLO32ND, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [GIVEN] First tied note (without tremolo) - should render normally
    const Chord *first_chord = find_chord(score, 1920);
    ASSERT_TRUE(first_chord);
    ASSERT_EQ(first_chord->notes().size(), 1);

    const Note *first_tied_note = first_chord->notes().front();
    ASSERT_TRUE(first_tied_note->tie_for());
    ASSERT_FALSE(first_tied_note->tie_back());

    // [WHEN] Request to render the first chord (without tremolo)
    PlaybackEventsMap result;
    renderer_.render(first_chord, 0, default_profile_, ctx, result);

    // [THEN] First note renders normally (standard articulation)
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.begin()->second.size(), 1);
    mpe::NoteEvent first_note_event = std::get<mpe::NoteEvent>(result.begin()->second.at(0));
    EXPECT_EQ(first_note_event.expression_ctx().articulations.size(), 1);
    EXPECT_TRUE(first_note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));
    EXPECT_EQ(first_note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 4));
    EXPECT_EQ(first_note_event.arrangement_ctx().nominal_duration, ::whole_note_duration); // Full note, not half note

    // [WHEN] Request to render the second chord (with tremolo disabled)
    result.clear();
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] Since tremolo playback is disabled, this should NOT render the tied note
    // This verifies that our fix prevents double playback when playTremolo = false
    EXPECT_EQ(result.size(), 0); // No events should be rendered for disabled tremolo on tied note

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_Single_Chord_Tremolo
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4+A4+C5 quarter notes chord marked by the 16-th Tremolo articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Single_Chord_Tremolo) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"single_chord_tremolo/single_chord_tremolo.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_chords_count = 4;
    int expected_sub_notes_count = 3;

    std::vector<duration_t> expected_durations = {::semi_quaver_note_duration, ::semi_quaver_note_duration, ::semi_quaver_note_duration,
                                                  ::semi_quaver_note_duration};

    std::vector<pitch_level_t> expected_pitches = {pitch_level(PitchClass::F, 4), pitch_level(PitchClass::A, 4),
                                                   pitch_level(PitchClass::C, 5)};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TREMOLO16TH, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] We expect that rendered note events number will match expectations
    EXPECT_EQ(result.begin()->second.size(), expected_chords_count * expected_sub_notes_count);

    for (int chord_idx = 0; chord_idx < expected_chords_count; ++chord_idx) {
        for (int note_idx = 0; note_idx < expected_sub_notes_count; ++note_idx) {
            int event_idx = (chord_idx * expected_sub_notes_count) + note_idx;

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(result.begin()->second.at(event_idx));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TREMOLO16TH));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(chord_idx));

            // [THEN] We expect that each note event will match expected pitch disclosure
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitches.at(note_idx));
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Single_Chord_Tremolo_TiedNotes
 * @details In this case we're gonna render a simple piece of score with 3 single measures.
 *          The second measure has a whole note tied to a whole note in the third measure.
 *          Each note has a Tremolo32nd.
 *          This test checks that all tied notes have the correct tremolo time and duration
 *          See: https://github.com/musescore/MuseScore/issues/23168
 */
TEST_F(EngravingPlaybackEventsRendererTests, Single_Chord_Tremolo_TiedNotes) {
    // [GIVEN] Simple piece of score (violin, 4/4, 120 bpm, Treble Cleff)
    Score *score =
        ScoreRW::read_score(::playback_events_rendering_dir + u"single_chord_tremolo_tied_notes/single_chord_tremolo_tied_notes.mscx");

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TREMOLO32ND, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Render the score
    PlaybackEventsMap result;

    for (const Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (const Segment *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
            const mu::engraving::EngravingItem *element = segment->element(0);
            if (element && element->is_chord()) {
                renderer_.render(to_chord(element), 0, default_profile_, ctx, result);
            }
        }
    }

    // [THEN] We expect 2 lists of events (for 2 tied notes)
    EXPECT_EQ(result.size(), 2);

    constexpr timestamp_t expected_tremolo_timestamp = 2000000;                  // timestamp of the 1st tied note
    constexpr timestamp_t expected_tremolo_duration = ::whole_note_duration * 2; // total duration of all tied notes
    constexpr timestamp_t expected_tremolo_timestamp_to = expected_tremolo_timestamp + expected_tremolo_duration;

    constexpr pitch_level_t expected_pitch_level = pitch_level(PitchClass::F, 4);
    constexpr duration_t expected_sub_note_duration = 62500;

    size_t tremolo_start_range_event_count = 0;
    size_t tremolo_end_range_event_count = 0;

    for (const auto &pair : result) {
        // [THEN] Expected number of sub-notes
        EXPECT_EQ(pair.second.size(), 32);

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            ASSERT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TREMOLO32ND));

            // [THEN] Each note event has the correct articulation time and duration
            const ArticulationAppliedData &articulation_data = note_event.expression_ctx().articulations.at(ArticulationType::TREMOLO32ND);
            EXPECT_EQ(articulation_data.meta.timestamp, expected_tremolo_timestamp);
            EXPECT_EQ(articulation_data.meta.overall_duration, expected_tremolo_duration);

            const timestamp_t note_timestamp_to =
                note_event.arrangement_ctx().actual_timestamp + note_event.arrangement_ctx().actual_duration;

            // [THEN] Each note has the correct occupiedFrom/To
            if (note_event.arrangement_ctx().nominal_timestamp == expected_tremolo_timestamp) {
                EXPECT_EQ(articulation_data.occupied_from_value, 0);
                EXPECT_NE(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                tremolo_start_range_event_count++;
            } else if (note_timestamp_to == expected_tremolo_timestamp_to) {
                EXPECT_NE(articulation_data.occupied_from_value, 0);
                EXPECT_EQ(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                tremolo_end_range_event_count++;
            } else {
                EXPECT_NE(articulation_data.occupied_from_value, 0);
                EXPECT_NE(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
            }

            // [THEN] Each note event has the correct pitch level
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitch_level);

            // [THEN] Each note event has the correct duration
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_sub_note_duration);

            // [THEN] Each note event has the correct pitch level
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitch_level);

            // [THEN] Each note event has the correct duration
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_sub_note_duration);
        }
    }

    // [THEN] We expect only 1 start & end event for the tremolo
    EXPECT_EQ(tremolo_start_range_event_count, 1);
    EXPECT_EQ(tremolo_end_range_event_count, 1);

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_PartiallyTiedTremolo
 */
TEST_F(EngravingPlaybackEventsRendererTests, PartiallyTiedTremolo) {
    // [GIVEN] Simple piece of score (violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"partially_tied_tremolo.mscx");
    ASSERT_TRUE(score);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TREMOLO16TH, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Render the score
    PlaybackEventsMap result;
    for (const RepeatSegment *repeat : score->repeat_list()) {
        const int tick_position_offset = repeat->utick - repeat->tick;

        for (const Measure *m : repeat->measure_list()) {
            for (const Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                const EngravingItem *el = s->element(0);
                if (!el || !el->is_chord()) {
                    continue;
                }

                renderer_.render(to_chord(el), tick_position_offset, default_profile_, ctx, result);
            }
        }
    }

    // [THEN] We expect 4 lists of events (for each note with tremolo)
    EXPECT_EQ(result.size(), 4);

    const std::vector<timestamp_t> expected_tremolo_timestamps{
        0,        // 1st measure
        5000000,  // 3rd measure, outgoing partially tied A4
        5000000,  // 1st measure (repeated), incoming A4
        11000000, // 3rd measure (repeated), outgoing A4 note
    };

    const std::vector<duration_t> expected_tremolo_durations{
        ::half_note_duration,     // 1st measure
        ::half_note_duration * 2, // 3rd measure: total duration of all partially tied notes
        ::half_note_duration * 2, // 1st measure (repeated): total duration of all partially tied notes
        ::half_note_duration,     // 3rd measure (repeated)
    };

    constexpr pitch_level_t expected_pitch_level = pitch_level(PitchClass::A, 4);
    constexpr size_t expected_sub_note_count = 8;
    constexpr duration_t expected_sub_note_duration = ::half_note_duration / expected_sub_note_count;

    size_t tremolo_note_num = 0;
    size_t tremolo_start_range_event_count = 0;
    size_t tremolo_end_range_event_count = 0;

    for (const auto &pair : result) {
        // [THEN] Expected number of sub-notes
        EXPECT_EQ(pair.second.size(), expected_sub_note_count);

        const timestamp_t expected_tremolo_timestamp = expected_tremolo_timestamps.at(tremolo_note_num);
        const duration_t expected_tremolo_duration = expected_tremolo_durations.at(tremolo_note_num);
        const timestamp_t expected_tremolo_timestamp_to = expected_tremolo_timestamp + expected_tremolo_duration;

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            ASSERT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TREMOLO16TH));

            // [THEN] Each note event has the correct articulation time and duration
            const ArticulationAppliedData &articulation_data = note_event.expression_ctx().articulations.at(ArticulationType::TREMOLO16TH);
            EXPECT_EQ(articulation_data.meta.timestamp, expected_tremolo_timestamp);
            EXPECT_EQ(articulation_data.meta.overall_duration, expected_tremolo_duration);

            const timestamp_t note_timestamp_to =
                note_event.arrangement_ctx().actual_timestamp + note_event.arrangement_ctx().actual_duration;

            // [THEN] Each note has the correct occupiedFrom/To
            if (note_event.arrangement_ctx().nominal_timestamp == expected_tremolo_timestamp) {
                EXPECT_EQ(articulation_data.occupied_from_value, 0);
                EXPECT_NE(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                tremolo_start_range_event_count++;
            } else if (note_timestamp_to == expected_tremolo_timestamp_to) {
                EXPECT_NE(articulation_data.occupied_from_value, 0);
                EXPECT_EQ(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                tremolo_end_range_event_count++;
            } else {
                EXPECT_NE(articulation_data.occupied_from_value, 0);
                EXPECT_NE(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
            }

            // [THEN] Each note event has the correct pitch level
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitch_level);

            // [THEN] Each note event has the correct duration
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_sub_note_duration);
        }

        tremolo_note_num++;
    }

    // [THEN] We expect 3 start & end events for each tremolo
    EXPECT_EQ(tremolo_start_range_event_count, 3);
    EXPECT_EQ(tremolo_end_range_event_count, 3);

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_Two_Chords_Tremolo
 * @details In this case we're gonna render a simple piece of score with a single measure,
 *          which starts with the F4+A4+C5 chord and ends with C5+E5+G5 chord connected by the 16-th Tremolo articulation
 */
TEST_F(EngravingPlaybackEventsRendererTests, Two_Chords_Tremolo) {
    // [GIVEN] Simple piece of score (piano, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"two_chords_tremolo/two_chords_tremolo.mscx");

    Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    ChordRest *chord = first_segment->next_chord_rest(0);
    ASSERT_TRUE(chord);

    // [GIVEN] Expected disclosure
    int expected_chords_count = 16;
    int chord_notes_count = 3;

    std::vector<duration_t> expected_durations(expected_chords_count, ::semi_quaver_note_duration);

    std::vector<pitch_level_t> first_chord_pitches = {pitch_level(PitchClass::F, 4), pitch_level(PitchClass::A, 4),
                                                      pitch_level(PitchClass::C, 5)};

    std::vector<pitch_level_t> second_chord_pitches = {pitch_level(PitchClass::C, 5), pitch_level(PitchClass::E, 5),
                                                       pitch_level(PitchClass::G, 5)};

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TREMOLO16TH, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Request to render a chord
    PlaybackEventsMap result;
    renderer_.render(chord, 0, default_profile_, ctx, result);

    // [THEN] We expect that rendered note events number will match expectations
    EXPECT_EQ(result.begin()->second.size(), expected_chords_count * chord_notes_count);

    for (int chord_idx = 0; chord_idx < expected_chords_count; ++chord_idx) {
        for (int note_idx = 0; note_idx < chord_notes_count; ++note_idx) {
            int event_idx = (chord_idx * chord_notes_count) + note_idx;

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(result.begin()->second.at(event_idx));

            // [THEN] We expect that each note event has only one articulation applied
            EXPECT_EQ(note_event.expression_ctx().articulations.size(), 1);
            EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::TREMOLO16TH));

            // [THEN] We expect that each sub-note has expected duration
            EXPECT_EQ(note_event.arrangement_ctx().nominal_duration, expected_durations.at(chord_idx));

            // [THEN] We expect that each note event will match expected pitch disclosure
            if (chord_idx % 2 == 0) {
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, first_chord_pitches.at(note_idx));
            } else {
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, second_chord_pitches.at(note_idx));
            }
        }
    }
}

/**
 * @brief PlaybackEventsRendererTests_Pauses
 *  @details Checks whether we correctly calculate note durations when there are pauses in the score. See:
 * https://github.com/musescore/MuseScore/issues/20669
 * https://github.com/musescore/MuseScore/issues/20557
 * https://github.com/musescore/MuseScore/issues/21289
 */
TEST_F(EngravingPlaybackEventsRendererTests, Pauses) {
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"pauses/pauses.mscx");

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Render the score
    PlaybackEventsMap result;

    for (const RepeatSegment *repeat_segment : score->repeat_list()) {
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;

        for (const Measure *m : repeat_segment->measure_list()) {
            for (const Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                const EngravingItem *el = s->element(0);
                if (!el || !el->is_chord()) {
                    continue;
                }

                renderer_.render(to_chord(el), tick_position_offset, default_profile_, ctx, result);
            }
        }
    }

    // [THEN] Expected time and duration of each event
    timestamp_t second_measure_time = ::quarter_note_duration * 4;

    timestamp_t third_measure_time =
        second_measure_time + ::quarter_note_duration + 2000000 + ::half_note_duration + ::quarter_note_duration + 4000000;

    timestamp_t fourth_measure_time =
        third_measure_time + ::half_note_duration + ::quarter_note_duration + 3000000 + ::quarter_note_duration;

    timestamp_t fifth_measure_time =
        fourth_measure_time + ::whole_note_duration * 2 + 5000000; // repeat: only final iteration carries the section-break pause

    std::vector<TimestampAndDuration> expected_tn_d_list{
        // 1st measure (no notes)

        // 2nd measure
        {second_measure_time, ::quarter_note_duration}, // + 2s pause (caesura)
        {second_measure_time + ::quarter_note_duration + 2000000, ::half_note_duration},
        {second_measure_time + ::quarter_note_duration + 2000000 + ::half_note_duration,
         ::quarter_note_duration}, // + 4s pause (section break)

        // 3rd measure
        {third_measure_time, ::half_note_duration + ::quarter_note_duration}, // tied half + quarter notes + 3s pause (caesura)
        {third_measure_time + ::half_note_duration + ::quarter_note_duration + 3000000, ::quarter_note_duration},

        // 4th measure (whole note inside repeat + section break)
        {fourth_measure_time, ::whole_note_duration},                         // no extra pause (non-final segment)
        {fourth_measure_time + ::whole_note_duration, ::whole_note_duration}, // final iteration carries the 5s section-break pause

        // 5th measure
        {fifth_measure_time, ::half_note_duration},
    };

    EXPECT_FALSE(result.empty());

    int tnd_num = 0;
    for (const auto &pair : result) {
        for (const mpe::PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const TimestampAndDuration &expected_tn_d = expected_tn_d_list.at(tnd_num);
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            EXPECT_EQ(pair.first, expected_tn_d.timestamp);
            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_tn_d.timestamp);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_tn_d.duration);

            ++tnd_num;
        }
    }

    EXPECT_EQ(tnd_num, expected_tn_d_list.size());
}

/**
 * @brief PlaybackEventsRendererTests_TiedNotesAndRepeats
 *  @details Checks whether we correctly calculate tied note durations when they are inside/outside repeats. See:
 *  https://github.com/musescore/MuseScore/issues/22863
 *  https://github.com/musescore/MuseScore/issues/27661
 */
TEST_F(EngravingPlaybackEventsRendererTests, TiedNotesAndRepeats) {
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"tied_notes_and_repeats.mscx");
    ASSERT_TRUE(score);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Render the score
    PlaybackEventsMap result;

    for (const RepeatSegment *repeat_segment : score->repeat_list()) {
        const int tick_position_offset = repeat_segment->utick - repeat_segment->tick;

        for (const Measure *m : repeat_segment->measure_list()) {
            for (const Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                const EngravingItem *el = s->element(0);
                if (!el || !el->is_chord()) {
                    continue;
                }

                renderer_.render(to_chord(el), tick_position_offset, default_profile_, ctx, result);
            }
        }
    }

    // [THEN] Expected pitch, time and duration of each event
    const std::vector<pitch_level_t> expected_pitch_list{
        // 1st measure (no notes)

        // 2nd measure
        pitch_level(PitchClass::C, 5),
        pitch_level(PitchClass::A, 4),

        // 2nd measure (repeated)
        pitch_level(PitchClass::C, 5),
        pitch_level(PitchClass::A, 4),

        // 3rd measure
        pitch_level(PitchClass::B, 4),

        // 4th measure
        pitch_level(PitchClass::B, 4),

        // 4th measure (repeated)
        pitch_level(PitchClass::B, 4),
    };

    constexpr timestamp_t second_measure_time = ::whole_note_duration;
    constexpr timestamp_t second_mesaure_repeated_time = ::whole_note_duration * 2;
    constexpr timestamp_t third_measure = ::whole_note_duration * 3;
    constexpr timestamp_t fourth_measure = ::whole_note_duration * 4;
    constexpr timestamp_t fourth_measure_repeated = ::whole_note_duration * 5;

    const std::vector<TimestampAndDuration> expected_tn_d_list{
        // 1st measure (no notes)

        // 2nd measure
        {second_measure_time, ::quarter_note_duration * 2},                        // 2 tied C5
        {second_measure_time + ::quarter_note_duration * 2, ::half_note_duration}, // A4 tied to a quarter A4 outside of the repeat

        // 2nd measure (repeated)
        {second_mesaure_repeated_time, ::quarter_note_duration * 2}, // 2 tied C5
        {second_mesaure_repeated_time + ::half_note_duration,
         ::half_note_duration + ::quarter_note_duration}, // A4 tied to a quarter A4 outside of the repeat

        // 3rd measure
        {third_measure + ::quarter_note_duration * 3, ::quarter_note_duration * 2}, // B4 tied to a quarter B4 in the next measure

        // 4th measure
        {fourth_measure + ::quarter_note_duration * 3, ::quarter_note_duration * 2}, // B4 tied to a quarter B4 in the next repeat segment

        // 4th measure (repeated)
        {fourth_measure_repeated + ::quarter_note_duration * 3,
         ::quarter_note_duration + ::half_note_duration}, // B4 tied to a half B4 in the next measure
    };

    ASSERT_EQ(expected_pitch_list.size(), expected_tn_d_list.size());
    EXPECT_FALSE(result.empty());

    int event_num = 0;
    for (const auto &pair : result) {
        for (const mpe::PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const TimestampAndDuration &expected_tn_d = expected_tn_d_list.at(event_num);
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            EXPECT_EQ(pair.first, expected_tn_d.timestamp);
            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_tn_d.timestamp);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_tn_d.duration);
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitch_list.at(event_num));

            ++event_num;
        }
    }

    EXPECT_EQ(event_num, expected_tn_d_list.size());

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_PartialTie
 * @details Checks whether we correctly calculate the duration of partially tied notes
 */
TEST_F(EngravingPlaybackEventsRendererTests, PartialTie) {
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"partial_tie.mscx");

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Render the score
    PlaybackEventsMap result;

    for (const RepeatSegment *repeat_segment : score->repeat_list()) {
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;

        for (const Measure *m : repeat_segment->measure_list()) {
            for (const Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                const EngravingItem *el = s->element(0);
                if (!el || !el->is_chord()) {
                    continue;
                }

                renderer_.render(to_chord(el), tick_position_offset, default_profile_, ctx, result);
            }
        }
    }

    // [THEN] Expected pitch, time and duration of each event
    std::vector<pitch_level_t> expected_pitch_list{
        // 1st measure
        pitch_level(PitchClass::A, 4),

        // 2nd measure: no notes

        // 3rd measure
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::A, 4), // partial tie to a whole note, also tied to a quarter note in the 4th measure

        // 1st measure (repeated)
        // 2nd measure (repeated)

        // 3rd measure (repeated)
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::F, 4),
        pitch_level(PitchClass::A, 4),

        // 4th measure (2nd repeat segment): 1 tied quarter A4 note, skip
    };

    timestamp_t third_measure_time = ::whole_note_duration * 2;
    timestamp_t third_measure_repeated_time = ::whole_note_duration * 3 + ::whole_note_duration * 2;

    std::vector<TimestampAndDuration> expected_tn_d_list{
        // 1st measure
        {0, ::whole_note_duration},

        // 2nd measure: no notes

        // 3rd measure
        {third_measure_time, ::quarter_note_duration},
        {third_measure_time + ::quarter_note_duration, ::quarter_note_duration},
        {third_measure_time + ::quarter_note_duration * 2, ::quarter_note_duration},
        {third_measure_time + ::quarter_note_duration * 3,
         ::quarter_note_duration + ::whole_note_duration}, // partial tie to a whole A4 note

        // 1st measure (repeated): skip the whole A4 note
        // 2nd measure (repeated): no notes

        // 3rd measure (repeated)
        {third_measure_repeated_time, ::quarter_note_duration},
        {third_measure_repeated_time + ::quarter_note_duration, ::quarter_note_duration},
        {third_measure_repeated_time + ::quarter_note_duration * 2, ::quarter_note_duration},
        {third_measure_repeated_time + ::quarter_note_duration * 3,
         ::quarter_note_duration * 2}, // also tied to a quarter note in the 4th measure

        // 4th measure (2nd repeat segment): 1 tied quarter A4 note, skip
    };

    ASSERT_EQ(expected_pitch_list.size(), expected_tn_d_list.size());
    EXPECT_FALSE(result.empty());

    int event_num = 0;
    for (const auto &pair : result) {
        for (const mpe::PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const TimestampAndDuration &expected_tn_d = expected_tn_d_list.at(event_num);
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            EXPECT_EQ(pair.first, expected_tn_d.timestamp);
            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_tn_d.timestamp);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_tn_d.duration);
            EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, expected_pitch_list.at(event_num));

            ++event_num;
        }
    }

    EXPECT_EQ(event_num, expected_tn_d_list.size());

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_TrillLine_TiedNotes
 * Checks that we can render the trill line if it doesn't last the full duration of the tied notes
 * See: https://github.com/musescore/MuseScore/issues/18676
 */
TEST_F(EngravingPlaybackEventsRendererTests, TrillLine_TiedNotes) {
    // [GIVEN] Simple piece of score (violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"trill_line_on_tied_notes.mscx");

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);
    default_profile_->set_pattern(ArticulationType::TRILL, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Render the score
    PlaybackEventsMap result;

    for (const Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (const Segment *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
            const mu::engraving::EngravingItem *element = segment->element(0);
            if (element && element->is_chord()) {
                renderer_.render(to_chord(element), 0, default_profile_, ctx, result);
            }
        }
    }

    // [THEN] We expect 4 lists of events (for 4 tied notes)
    EXPECT_EQ(result.size(), 4);

    constexpr timestamp_t expected_trill_timestamp = ::whole_note_duration; // timestamp of the 2nd tied note (where the trill starts)
    constexpr duration_t expected_trill_duration = ::whole_note_duration * 2;
    constexpr timestamp_t expected_trill_timestamp_to = expected_trill_timestamp + expected_trill_duration;

    size_t note_num = 0;
    size_t trill_start_range_event_count = 0;
    size_t trill_end_range_event_count = 0;

    for (const auto &pair : result) {
        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            // [THEN] We expect that each note event has only one articulation applied
            ASSERT_EQ(note_event.expression_ctx().articulations.size(), 1);

            if (note_num == 0 || note_num == 3) {
                // [THEN] Normal notes (no trill)
                EXPECT_EQ(pair.second.size(), 1);
                EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::STANDARD));
                EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 4));

                // [THEN] Each note event has the correct duration
                EXPECT_EQ(note_event.arrangement_ctx().actual_duration, ::whole_note_duration);
            } else {
                // [THEN] Notes under the trill line
                constexpr size_t expected_sub_notes = 32;
                EXPECT_EQ(pair.second.size(), expected_sub_notes);

                // [THEN] Each note event has the correct articulation time and duration
                const ArticulationAppliedData &articulation_data = note_event.expression_ctx().articulations.at(ArticulationType::TRILL);
                EXPECT_EQ(articulation_data.meta.timestamp, expected_trill_timestamp);
                EXPECT_EQ(articulation_data.meta.overall_duration, expected_trill_duration);

                const timestamp_t note_timestamp_to =
                    note_event.arrangement_ctx().actual_timestamp + note_event.arrangement_ctx().actual_duration;

                // [THEN] Each note has the correct occupiedFrom/To
                if (note_event.arrangement_ctx().nominal_timestamp == expected_trill_timestamp) {
                    EXPECT_EQ(articulation_data.occupied_from_value, 0);
                    EXPECT_NE(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                    trill_start_range_event_count++;
                } else if (note_timestamp_to == expected_trill_timestamp_to) {
                    EXPECT_NE(articulation_data.occupied_from_value, 0);
                    EXPECT_EQ(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                    trill_end_range_event_count++;
                } else {
                    EXPECT_NE(articulation_data.occupied_from_value, 0);
                    EXPECT_NE(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                }

                // [THEN] Each note event has the correct duration
                constexpr duration_t expected_sub_note_duration = ::whole_note_duration / expected_sub_notes;
                EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_sub_note_duration);
            }
        }

        ++note_num;
    }

    // [THEN] We expect only 1 start & end event for the trill
    EXPECT_EQ(trill_start_range_event_count, 1);
    EXPECT_EQ(trill_end_range_event_count, 1);

    delete score;
}

/**
 * @brief PlaybackEventsRendererTests_Trill_TiedNotes
 * Checks that we can render the normal trill (ornament) if it starts from a tied note
 * See: https://github.com/musescore/MuseScore/issues/18676
 * See: https://github.com/musescore/MuseScore/issues/27472
 */
TEST_F(EngravingPlaybackEventsRendererTests, Trill_TiedNotes) {
    // [GIVEN] Simple piece of score (violin, 4/4, 120 bpm, Treble Cleff)
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"trill_on_tied_notes.mscx");

    ASSERT_TRUE(score);
    ASSERT_EQ(score->repeat_list().size(), 2);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::TRILL, dummy_pattern_);

    // [GIVEN] Dummy context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [GIVEN] The chord we want to render
    const Measure *first_measure = score->first_measure();
    ASSERT_TRUE(first_measure);

    const Segment *first_segment = first_measure->segments().first_cr_segment();
    ASSERT_TRUE(first_segment);

    const ChordRest *cr = first_segment->next_chord_rest(0);
    ASSERT_TRUE(cr);
    ASSERT_TRUE(cr->is_chord());

    const Chord *chord = to_chord(cr);
    ASSERT_EQ(chord->notes().size(), 1);
    ASSERT_EQ(chord->articulations().size(), 1);

    const Articulation *articulation = chord->articulations().front();
    ASSERT_TRUE(articulation->is_ornament());
    ASSERT_EQ(to_ornament(articulation)->sym_id(), SymId::ORNAMENT_TRILL);

    // [WHEN] Render the chord
    PlaybackEventsMap result;
    for (const RepeatSegment *repeat_segment : score->repeat_list()) {
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;
        renderer_.render(to_chord(chord), tick_position_offset, default_profile_, ctx, result);
    }

    // [THEN] We expect 2 lists of events (for each repeat segment)
    EXPECT_EQ(result.size(), score->repeat_list().size());

    constexpr duration_t expected_trill_duration = ::whole_note_duration * 2; // 2 tied notes
    const std::vector<timestamp_t> expected_trill_timestamps{
        0,
        expected_trill_duration, // 2nd repeat segment
    };

    size_t repeat_segment_num = 0;
    size_t trill_start_range_event_count = 0;
    size_t trill_end_range_event_count = 0;

    for (const auto &pair : result) {
        // [THEN] Sub-notes of the trill
        constexpr size_t expected_sub_notes = 64;
        EXPECT_EQ(pair.second.size(), expected_sub_notes);

        const timestamp_t expected_trill_timestamp = expected_trill_timestamps.at(repeat_segment_num);
        const timestamp_t expected_trill_timestamp_to = expected_trill_timestamp + expected_trill_duration;
        ++repeat_segment_num;

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);

            // [THEN] We expect that each note event has only one articulation applied
            ASSERT_EQ(note_event.expression_ctx().articulations.size(), 1);

            // [THEN] Each note event has the correct articulation time and duration
            const ArticulationAppliedData &articulation_data = note_event.expression_ctx().articulations.at(ArticulationType::TRILL);
            EXPECT_EQ(articulation_data.meta.timestamp, expected_trill_timestamp);
            EXPECT_EQ(articulation_data.meta.overall_duration, expected_trill_duration);

            const timestamp_t note_timestamp_to =
                note_event.arrangement_ctx().actual_timestamp + note_event.arrangement_ctx().actual_duration;

            // [THEN] Each note has the correct occupiedFrom/To
            if (note_event.arrangement_ctx().nominal_timestamp == expected_trill_timestamp) {
                EXPECT_EQ(articulation_data.occupied_from_value, 0);
                EXPECT_NE(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                trill_start_range_event_count++;
            } else if (note_timestamp_to == expected_trill_timestamp_to) {
                EXPECT_NE(articulation_data.occupied_from_value, 0);
                EXPECT_EQ(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
                trill_end_range_event_count++;
            } else {
                EXPECT_NE(articulation_data.occupied_from_value, 0);
                EXPECT_NE(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);
            }

            // [THEN] Each note event has the correct duration
            constexpr duration_t expected_sub_note_duration = expected_trill_duration / expected_sub_notes;
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, expected_sub_note_duration);
        }
    }

    // [THEN] We expect 2 start & end event for each trill
    EXPECT_EQ(trill_start_range_event_count, 2);
    EXPECT_EQ(trill_end_range_event_count, 2);

    delete score;
}

TEST_F(EngravingPlaybackEventsRendererTests, CountIn) {
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"count_in.mscx");
    ASSERT_TRUE(score);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::STANDARD, dummy_pattern_);

    // [GIVEN] Anacrusis measure (1/4)
    const Measure *anacrusis_measure_1_4 = score->first_measure();
    ASSERT_TRUE(anacrusis_measure_1_4);

    // [WHEN] Render the anacrusis measure
    PlaybackEventsMap result;
    duration_t count_in_duration = 0;
    int start_tick = anacrusis_measure_1_4->tick().ticks();
    renderer_.render_count_in(score, start_tick, 0, default_profile_, result, count_in_duration);

    // [THEN] 7 quarter note events
    EXPECT_EQ(result.size(), 7);
    EXPECT_EQ(count_in_duration, ::quarter_note_duration * 7);

    timestamp_t expected_timestamp = 0;

    for (const auto &pair : result) {
        EXPECT_EQ(pair.second.size(), 1);

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, ::quarter_note_duration);
            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_timestamp);

            expected_timestamp += ::quarter_note_duration;
        }
    }

    // [GIVEN] Second measure (4/4)
    const Measure *second_measure_4_4 = anacrusis_measure_1_4->next_measure();
    ASSERT_TRUE(second_measure_4_4);

    // [WHEN] Render Count In events starting from the beginning of the measure (4/4)
    result.clear();
    count_in_duration = 0;
    start_tick = second_measure_4_4->tick().ticks();
    renderer_.render_count_in(score, start_tick, 0, default_profile_, result, count_in_duration);

    // [THEN] 4 quarter note events
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(count_in_duration, ::quarter_note_duration * 4);

    expected_timestamp = 0;

    for (const auto &pair : result) {
        EXPECT_EQ(pair.second.size(), 1);

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, ::quarter_note_duration);
            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_timestamp);

            expected_timestamp += ::quarter_note_duration;
        }
    }

    // [WHEN] Render the same measure (4/4), but starting from the 3rd beat
    result.clear();
    count_in_duration = 0;
    start_tick = second_measure_4_4->tick().ticks() + 480 + 480;
    renderer_.render_count_in(score, start_tick, 0, default_profile_, result, count_in_duration);

    // [THEN] 6 quarter note events
    EXPECT_EQ(result.size(), 6);
    EXPECT_EQ(count_in_duration, ::quarter_note_duration * 6);

    expected_timestamp = 0;

    for (const auto &pair : result) {
        EXPECT_EQ(pair.second.size(), 1);

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, ::quarter_note_duration);
            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_timestamp);

            expected_timestamp += ::quarter_note_duration;
        }
    }

    // [GIVEN] The next measure we want to render (3/8)
    const Measure *third_measure_3_8 = second_measure_4_4->next_measure();
    ASSERT_TRUE(third_measure_3_8);

    // [WHEN] Render the 3rd measure (3/8)
    result.clear();
    count_in_duration = 0;
    start_tick = third_measure_3_8->tick().ticks();
    renderer_.render_count_in(score, start_tick, 0, default_profile_, result, count_in_duration);

    // [THEN] 3 quaver note events
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(count_in_duration, ::quaver_note_duration * 3);

    expected_timestamp = 0;

    for (const auto &pair : result) {
        EXPECT_EQ(pair.second.size(), 1);

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, ::quaver_note_duration);
            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_timestamp);

            expected_timestamp += ::quaver_note_duration;
        }
    }

    // [WHEN] Render the 3rd measure (3/8) from some random tick that is not equal to any beat position
    result.clear();
    count_in_duration = 0;
    start_tick = third_measure_3_8->tick().ticks() + 333;
    renderer_.render_count_in(score, start_tick, 0, default_profile_, result, count_in_duration); // tick: 240 + 93

    // [THEN] 5 quaver note events (3 + 2)
    constexpr int closest_main_beat = 2880;
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(count_in_duration, ::quaver_note_duration * 5 - duration_from_tempo_and_ticks(2.0, closest_main_beat - start_tick));

    expected_timestamp = 0;

    for (const auto &pair : result) {
        EXPECT_EQ(pair.second.size(), 1);

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
            EXPECT_EQ(note_event.arrangement_ctx().actual_duration, ::quaver_note_duration);
            EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, expected_timestamp);

            expected_timestamp += ::quaver_note_duration;
        }
    }

    delete score;
}

TEST_F(EngravingPlaybackEventsRendererTests, HandbellsLetVibrate) {
    // [GIVEN] Score with LV & Damp techniques
    Score *score = ScoreRW::read_score(::playback_events_rendering_dir + u"handbells_let_vibrate.mscx");
    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    // [GIVEN] Fulfill articulations profile with dummy patterns
    default_profile_->set_pattern(ArticulationType::PEDAL, dummy_pattern_);

    // [GIVEN] Playback context
    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);

    // [WHEN] Parse the score
    const Part *part = score->parts().front();
    const TrackRange track_range = part->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx->update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [WHEN] Render the score
    PlaybackEventsMap result;

    for (const Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (const Segment *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
            const mu::engraving::EngravingItem *element = segment->element(0);
            if (element && element->is_chord()) {
                renderer_.render(to_chord(element), 0, default_profile_, ctx, result);
            }
        }
    }

    // [THEN] Pedal is correctly applied to the note events
    constexpr mpe::timestamp_t first_lv_start_timestamp = 0;        // 1st measure: LV
    constexpr mpe::timestamp_t first_lv_end_timestamp = 2000000;    // 2nd measure: Damp
    constexpr mpe::timestamp_t second_lv_start_timestamp = 4000000; // 3rd measure: LV (no Damp)

    EXPECT_FALSE(result.empty());

    for (const auto &pair : result) {
        EXPECT_FALSE(pair.second.empty());

        for (const PlaybackEvent &event : pair.second) {
            ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));

            const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(event);
            const mpe::timestamp_t timestamp = note_event.arrangement_ctx().actual_timestamp;
            const mpe::ArticulationMap &articulations = note_event.expression_ctx().articulations;

            auto pedal_it = articulations.find(mpe::ArticulationType::PEDAL);

            if (timestamp < first_lv_end_timestamp) {
                ASSERT_NE(pedal_it, articulations.end());

                const mpe::ArticulationMeta &meta = pedal_it->second.meta;
                EXPECT_EQ(meta.timestamp, first_lv_start_timestamp);
                EXPECT_EQ(meta.overall_duration, first_lv_end_timestamp);
            } else if (timestamp >= first_lv_end_timestamp && timestamp < second_lv_start_timestamp) {
                ASSERT_EQ(pedal_it, articulations.end());
            } else {
                ASSERT_NE(pedal_it, articulations.end());

                const mpe::ArticulationMeta &meta = pedal_it->second.meta;
                EXPECT_EQ(meta.timestamp, second_lv_start_timestamp);
                EXPECT_EQ(meta.overall_duration, mpe::infinite_duration); // no Damp
            }
        }
    }

    delete score;
}
