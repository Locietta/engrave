/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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
#include <gmock/gmock.h>

#include <cmath>

#include "utils/scorerw.h"

#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/staff.h"

#include "engraving/playback/playbackcontext.h"
#include "engraving/playback/utils/arrangementutils.h"

using namespace mu::engraving;
using namespace muse::mpe;
using namespace muse;

static const muse::String playback_context_test_files_dir(u"playback/playbackcontext_data/");

static constexpr int ticks_step = 480;

static void expect_levels_match(const DynamicAutomationLayers &actual_layers,
                                const std::map<timestamp_t, dynamic_level_t> &expected_dynamics) {
    EXPECT_FALSE(actual_layers.empty());
    for (const auto &layer : actual_layers) {
        for (const auto &[timestamp, expected_level] : expected_dynamics) {
            const dynamic_level_t actual_level = dynamic_level_from_normalized(evaluate_curve_at(layer.second, timestamp));
            EXPECT_EQ(actual_level, expected_level) << "at timestamp " << timestamp;
        }
    }
}

static void expect_level_near(const DynamicAutomationLayers &actual_layers, timestamp_t timestamp, dynamic_level_t expected_level) {
    for (const auto &layer : actual_layers) {
        const dynamic_level_t actual_level = dynamic_level_from_normalized(evaluate_curve_at(layer.second, timestamp));
        EXPECT_LE(std::abs(actual_level - expected_level), 1) << "at timestamp " << timestamp;
    }
}

struct EngravingPlaybackContextTests : ::testing::Test {};

//! Checks that hairpins outside/inside repeats don't overlap. See:
//! https://github.com/musescore/MuseScore/issues/16167
//! https://github.com/musescore/MuseScore/issues/23940
TEST_F(EngravingPlaybackContextTests, Hairpins_Repeats) {
    // [GIVEN] Score with hairpins and repeats
    MasterScore *score = ScoreRW::read_score(::playback_context_test_files_dir + u"dynamics/hairpins_and_repeats.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    const RepeatList &repeats = score->repeat_list();
    ASSERT_EQ(repeats.size(), 2);

    // [WHEN] Init automation
    score->init_automation();

    // [GIVEN] Context for parsing dynamics
    PlaybackContext ctx(score);

    // [WHEN] Parse dynamics
    const TrackRange track_range = parts.front()->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [GIVEN] 1st hairpin (before the repeat): p -> f
    constexpr mpe::dynamic_level_t p = dynamic_level_from_type(mpe::DynamicType::P);
    constexpr mpe::dynamic_level_t f = dynamic_level_from_type(mpe::DynamicType::F);

    std::map<timestamp_t, dynamic_level_t> expected_dynamics{
        {timestamp_from_ticks(score, 0), p},
        {timestamp_from_ticks(score, 1920), f},
    };

    // [GIVEN] 2nd hairpin (inside the repeat): f -> fff
    constexpr mpe::dynamic_level_t fff = dynamic_level_from_type(mpe::DynamicType::FFF);
    constexpr int f_to_fff_start_tick = 1920;
    constexpr int f_to_fff_end_tick = f_to_fff_start_tick + 1440;

    for (const mu::engraving::RepeatSegment *repeat_segment : repeats) {
        int tick_position_offset = repeat_segment->utick - repeat_segment->tick;
        expected_dynamics.emplace(timestamp_from_ticks(score, f_to_fff_start_tick + tick_position_offset), f);
        expected_dynamics.emplace(timestamp_from_ticks(score, f_to_fff_end_tick + tick_position_offset), fff);
    }

    // [GIVEN] 3rd hairpin (right after the repeat): ppp -> p
    constexpr mpe::dynamic_level_t ppp = dynamic_level_from_type(mpe::DynamicType::PPP);
    constexpr int ppp_to_p_start_tick = 3840 + 1920; // real tick + repeat tick

    expected_dynamics.emplace(timestamp_from_ticks(score, ppp_to_p_start_tick), ppp);
    expected_dynamics.emplace(timestamp_from_ticks(score, ppp_to_p_start_tick + 1440), p);

    // [WHEN] Get the actual dynamics
    DynamicAutomationLayers layers = ctx.dynamic_level_layers(track_range.start_track, track_range.end_track);

    // [THEN] The dynamics match the expectation
    expect_levels_match(layers, expected_dynamics);

    delete score;
}

TEST_F(EngravingPlaybackContextTests, Dynamics_MeasureRepeats) {
    // [GIVEN] Score with 5 measures and 2 instruments. There is a measure repeat on the last 2 measures of the 1st instrument
    // (so the previous 2 measures will be repeated)
    MasterScore *score = ScoreRW::read_score(::playback_context_test_files_dir + u"dynamics/dynamics_and_measure_repeats.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_EQ(parts.size(), 2);

    // [WHEN] Init automation
    score->init_automation();

    // [GIVEN] Context for parsing dynamics
    PlaybackContext ctx(score);

    // [WHEN] Parse dynamics for the 1st instrument (with measure repeats)
    const TrackRange track0_range = parts.at(0)->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track0_range.start_track, track0_range.end_track, 0, last_tick);

    std::map<timestamp_t, dynamic_level_t> expected_dynamics{
        // 1st measure
        {timestamp_from_ticks(score, 0), dynamic_level_from_type(mpe::DynamicType::NATURAL)},

        // 2nd measure
        {timestamp_from_ticks(score, 1920), dynamic_level_from_type(mpe::DynamicType::PPP)}, // 1st quarter note
        {timestamp_from_ticks(score, 3360), dynamic_level_from_type(mpe::DynamicType::P)},   // 4th quarter note

        // 3rd measure
        {timestamp_from_ticks(score, 4320), dynamic_level_from_type(mpe::DynamicType::MF)},  // 2nd quarter note
        {timestamp_from_ticks(score, 5280), dynamic_level_from_type(mpe::DynamicType::FFF)}, // 4th quarter note

        // copy of 2nd measure
        {timestamp_from_ticks(score, 5760), dynamic_level_from_type(mpe::DynamicType::PPP)},
        {timestamp_from_ticks(score, 7200), dynamic_level_from_type(mpe::DynamicType::P)},

        // copy of 3rd measure
        {timestamp_from_ticks(score, 8160), dynamic_level_from_type(mpe::DynamicType::MF)},
        {timestamp_from_ticks(score, 9120), dynamic_level_from_type(mpe::DynamicType::FFF)},
    };

    // [WHEN] Get the actual dynamics
    DynamicAutomationLayers layers = ctx.dynamic_level_layers(track0_range.start_track, track0_range.end_track);

    // [THEN] The dynamics match the expectation
    expect_levels_match(layers, expected_dynamics);

    // [WHEN] Parse dynamics for the 2nd instrument (without measure repeats)
    const TrackRange track1_range = parts.at(1)->track_range();
    ctx.clear(track1_range.start_track, track1_range.end_track, 0, last_tick);
    ctx.update(track1_range.start_track, track1_range.end_track, 0, last_tick);

    // [WHEN] Get the actual dynamics
    layers = ctx.dynamic_level_layers(track1_range.start_track, track1_range.end_track);

    // [THEN] Measure repeat on the 1st instrument doesn't affect other instruments
    expected_dynamics = {
        // 1st measure
        {timestamp_from_ticks(score, 0), dynamic_level_from_type(mpe::DynamicType::F)},

        // 3rd measure
        {timestamp_from_ticks(score, 3840), dynamic_level_from_type(mpe::DynamicType::FF)},
    };

    // [THEN] The dynamics match the expectation
    expect_levels_match(layers, expected_dynamics);

    delete score;
}

TEST_F(EngravingPlaybackContextTests, Dynamics_OnDifferentVoices) {
    // [GIVEN]
    MasterScore *score = ScoreRW::read_score(::playback_context_test_files_dir + u"dynamics/dynamics_on_voices.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    // [WHEN] Init automation
    score->init_automation();

    // [GIVEN] Context for parsing dynamics
    PlaybackContext ctx(score);

    // [WHEN] Parse dynamics
    const Part *part = parts.front();
    const TrackRange track_range = part->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [WHEN] Get the actual dynamics
    DynamicAutomationLayers actual_layers = ctx.dynamic_level_layers(track_range.start_track, track_range.end_track);

    // [THEN] The dynamics match the expectation
    std::map<layer_idx_t, std::map<timestamp_t, dynamic_level_t>> expected_layers;

    auto add_dyn_to_all_staves_and_voices = [score, part, &expected_layers](mpe::DynamicType dyn, int tick) {
        const TrackRange track_range = part->track_range();
        for (track_idx_t track_idx = track_range.start_track; track_idx < track_range.end_track; ++track_idx) {
            expected_layers[static_cast<layer_idx_t>(track_idx)][timestamp_from_ticks(score, tick)] = dynamic_level_from_type(dyn);
        }
    };

    auto add_dyn_to_staff_and_voice = [score, &expected_layers](mpe::DynamicType dyn, staff_idx_t staff_idx, voice_idx_t voice_idx,
                                                                int tick) {
        layer_idx_t layer_idx = static_cast<layer_idx_t>(staff2track(staff_idx, voice_idx));
        expected_layers[layer_idx][timestamp_from_ticks(score, tick)] = dynamic_level_from_type(dyn);
    };

    // 1st measure
    add_dyn_to_all_staves_and_voices(mpe::DynamicType::PPP, 0); // "Apply to all voices"

    // 2nd measure
    add_dyn_to_staff_and_voice(mpe::DynamicType::PP, 0, 1, 1920); // 1st staff & 2nd voice ("Apply to 2nd voice")

    // 3rd measure
    add_dyn_to_staff_and_voice(mpe::DynamicType::MF, 0, 0, 3840); // 1st staff & 1st voice ("Apply to 1st voice")

    for (voice_idx_t voice_idx = 0; voice_idx < ::mu::engraving::voices; ++voice_idx) {
        // "Apply to all voices" is on but there is the mf on the 1st staff & 1st at the same tick
        if (voice_idx != 0) {
            add_dyn_to_staff_and_voice(mpe::DynamicType::P, 0, voice_idx, 3840);
        }

        add_dyn_to_staff_and_voice(mpe::DynamicType::P, 1, voice_idx, 3840);
    }

    // 4th measure
    for (voice_idx_t voice_idx = 0; voice_idx < ::mu::engraving::voices; ++voice_idx) {
        add_dyn_to_staff_and_voice(mpe::DynamicType::F, 0, voice_idx, 5760); // 1st staff only ("All voices on this staff")
    }

    // 5th measure
    add_dyn_to_all_staves_and_voices(mpe::DynamicType::FF, 7680); // "Apply to all voices"

    ASSERT_EQ(actual_layers.size(), expected_layers.size());
    for (const auto &[layer_idx, expected_levels] : expected_layers) {
        auto actual_it = actual_layers.find(layer_idx);
        ASSERT_TRUE(actual_it != actual_layers.end());

        for (const auto &[timestamp, expected_level] : expected_levels) {
            const dynamic_level_t actual_level = dynamic_level_from_normalized(evaluate_curve_at(actual_it->second, timestamp));
            EXPECT_EQ(actual_level, expected_level) << "layer " << layer_idx << " at timestamp " << timestamp;
        }
    }

    delete score;
}

//! See: https://github.com/musescore/MuseScore/issues/23596
TEST_F(EngravingPlaybackContextTests, Dynamics_Overlap) {
    // [GIVEN]
    MasterScore *score = ScoreRW::read_score(::playback_context_test_files_dir + u"dynamics/dynamics_overlap.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    // [WHEN] Init automation
    score->init_automation();

    // [GIVEN] Context for parsing dynamics
    PlaybackContext ctx(score);

    // [WHEN] Parse dynamics
    const Part *part = parts.front();
    const TrackRange track_range = part->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [WHEN] Get the actual dynamics
    DynamicAutomationLayers actual_layers = ctx.dynamic_level_layers(track_range.start_track, track_range.end_track);

    // [THEN] The dynamics match the expectation
    constexpr mpe::dynamic_level_t ff = dynamic_level_from_type(mpe::DynamicType::FF);
    constexpr mpe::dynamic_level_t pp = dynamic_level_from_type(mpe::DynamicType::PP);

    std::map<timestamp_t, dynamic_level_t> expected_dynamics{
        // 1st measure: Diminuendo ff -> pp
        {timestamp_from_ticks(score, 0), ff},

        // 2nd measure: ff (starts at the 1920 tick)
        {timestamp_from_ticks(score, 1920), ff},
    };

    expect_levels_match(actual_layers, expected_dynamics);

    // The curve holds { inValue=pp, outValue=ff } at tick 1920, so the ramp approaches pp but never reaches it
    expect_level_near(actual_layers, timestamp_from_ticks(score, 1919), pp);

    delete score;
}

TEST_F(EngravingPlaybackContextTests, Dynamics_Niente) {
    // [GIVEN]
    MasterScore *score = ScoreRW::read_score(::playback_context_test_files_dir + u"dynamics/dynamics_niente.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    // [WHEN] Init automation
    score->init_automation();

    // [GIVEN] Context for parsing dynamics
    PlaybackContext ctx(score);

    // [WHEN] Parse dynamics
    const Part *part = parts.front();
    const TrackRange track_range = part->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [WHEN] Get the actual dynamics
    DynamicAutomationLayers actual_layers = ctx.dynamic_level_layers(track_range.start_track, track_range.end_track);

    // [THEN] The dynamics match the expectation
    constexpr mpe::dynamic_level_t f = dynamic_level_from_type(mpe::DynamicType::F);
    constexpr mpe::dynamic_level_t n = dynamic_level_from_type(mpe::DynamicType::PPPPPPPPP);

    std::map<timestamp_t, dynamic_level_t> expected_dynamics{
        // 1st measure: Dim. al niente with 'n' dynamic
        {timestamp_from_ticks(score, 0), f},
        {timestamp_from_ticks(score, 1920), n},

        // 3rd measure: same, now with niente circle
        {timestamp_from_ticks(score, 3840), f},
        {timestamp_from_ticks(score, 3840 + 1920), n},

        // 5th measure: Cresc. dal niente with 'n' dynamic
        {timestamp_from_ticks(score, 7680), n},
        {timestamp_from_ticks(score, 7680 + 1920), f},

        // 7th measure: same, now with niente circle
        {timestamp_from_ticks(score, 11520), n},
        {timestamp_from_ticks(score, 11520 + 1920), f},
    };

    expect_levels_match(actual_layers, expected_dynamics);

    delete score;
}

TEST_F(EngravingPlaybackContextTests, Dynamics_HairpinWithCompound) {
    // [GIVEN]
    MasterScore *score = ScoreRW::read_score(::playback_context_test_files_dir + u"dynamics/dynamics_compound.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    // [WHEN] Init automation
    score->init_automation();

    // [GIVEN] Context for parsing dynamics
    PlaybackContext ctx(score);

    // [WHEN] Parse dynamics
    const Part *part = parts.front();
    const TrackRange track_range = part->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [WHEN] Get the actual dynamics
    DynamicAutomationLayers actual_layers = ctx.dynamic_level_layers(track_range.start_track, track_range.end_track);

    // [THEN] The dynamics match the expectation
    constexpr mpe::dynamic_level_t f = dynamic_level_from_type(mpe::DynamicType::F);
    constexpr mpe::dynamic_level_t p = dynamic_level_from_type(mpe::DynamicType::P);
    constexpr mpe::dynamic_level_t pp = dynamic_level_from_type(mpe::DynamicType::PP);

    constexpr int measure_ticks = 1920;
    constexpr int compound_dynamic_ticks = 384;

    std::map<timestamp_t, dynamic_level_t> expected_dynamics{
        // 1st and 2nd measures: fp -> f
        {timestamp_from_ticks(score, 0 * measure_ticks), f},
        {timestamp_from_ticks(score, 0 * measure_ticks + compound_dynamic_ticks), p},
        {timestamp_from_ticks(score, 1 * measure_ticks), f},
        {timestamp_from_ticks(score, 1 * measure_ticks + compound_dynamic_ticks), p},
        {timestamp_from_ticks(score, 2 * measure_ticks), f},

        // 4th and 5th measures: fp -> pp, then jump back to f. The hairpin's end tick is claimed by the next
        // dynamic marking (fp/f), which contradicts the pp hairpin's direction; that marking's own value wins
        // there, so the ramp never actually reaches pp (see the expectLevelNear checks below).
        {timestamp_from_ticks(score, 3 * measure_ticks), f},
        {timestamp_from_ticks(score, 3 * measure_ticks + compound_dynamic_ticks), p},
        {timestamp_from_ticks(score, 4 * measure_ticks), f},
        {timestamp_from_ticks(score, 4 * measure_ticks + compound_dynamic_ticks), p},
        {timestamp_from_ticks(score, 5 * measure_ticks), f},

        // 7th measure: fp -> pp, now don't jump back to f
        {timestamp_from_ticks(score, 6 * measure_ticks), f},
        {timestamp_from_ticks(score, 6 * measure_ticks + compound_dynamic_ticks), p},
        {timestamp_from_ticks(score, 7 * measure_ticks), pp},
    };

    expect_levels_match(actual_layers, expected_dynamics);

    // The ramps into the 4th and 5th measures get arbitrarily close to pp without ever reaching it exactly
    expect_level_near(actual_layers, timestamp_from_ticks(score, 4 * measure_ticks - 1), pp);
    expect_level_near(actual_layers, timestamp_from_ticks(score, 5 * measure_ticks - 1), pp);

    delete score;
}

TEST_F(EngravingPlaybackContextTests, PlayTechniques) {
    // [GIVEN] Score with playing technique annotations
    Score *score = ScoreRW::read_score(::playback_context_test_files_dir + u"play_techniques/play_tech_annotations.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    const TrackRange track_range = parts.front()->track_range();
    const track_idx_t track_idx = track_range.start_track;

    // [GIVEN] Context for parsing techniques
    PlaybackContext ctx(score);

    // [THEN] No technique parsed, returns the "Natural" type
    int max_tick = score->end_tick().ticks();

    for (int tick = 0; tick <= max_tick; tick += ::ticks_step) {
        std::pair<timestamp_t, PlayingTechniqueType> actual_type = ctx.playing_technique(track_idx, tick);
        EXPECT_EQ(actual_type.first, 0);
        EXPECT_EQ(actual_type.second, PlayingTechniqueType::NATURAL);
    }

    // [WHEN] Parse techniques
    ctx.update(track_range.start_track, track_range.end_track, 0, max_tick);

    // [THEN] The techniques successfully parsed
    constexpr int ticks_per_measure = 1920;

    const std::map<int /*tick*/, PlayingTechniqueType> expected_types{
        {0, PlayingTechniqueType::PIZZICATO},
        {1440, PlayingTechniqueType::OPEN},
        {ticks_per_measure * 1, PlayingTechniqueType::MUTE},
        {ticks_per_measure * 2, PlayingTechniqueType::TREMOLO},
        {ticks_per_measure * 3, PlayingTechniqueType::DETACHE},
        {ticks_per_measure * 4, PlayingTechniqueType::MARTELE},
        {ticks_per_measure * 5, PlayingTechniqueType::COL_LEGNO},
        {ticks_per_measure * 6, PlayingTechniqueType::SUL_PONTICELLO},
        {ticks_per_measure * 7, PlayingTechniqueType::SUL_TASTO},
        {ticks_per_measure * 8, PlayingTechniqueType::VIBRATO},
        {ticks_per_measure * 9, PlayingTechniqueType::LEGATO},
        {ticks_per_measure * 10, PlayingTechniqueType::DISTORTION},
        {ticks_per_measure * 11, PlayingTechniqueType::OVERDRIVE},
        {ticks_per_measure * 12, PlayingTechniqueType::HARMONICS},
        {ticks_per_measure * 13, PlayingTechniqueType::JAZZ_TONE},
        {ticks_per_measure * 14, PlayingTechniqueType::HANDBELLS_SWING},
        {ticks_per_measure * 15, PlayingTechniqueType::HANDBELLS_SWING_UP},
        {ticks_per_measure * 16, PlayingTechniqueType::HANDBELLS_SWING_DOWN},
        {ticks_per_measure * 17, PlayingTechniqueType::HANDBELLS_ECHO1},
        {ticks_per_measure * 18, PlayingTechniqueType::HANDBELLS_ECHO2},
        {ticks_per_measure * 19, PlayingTechniqueType::HANDBELLS_R},
        {ticks_per_measure * 20, PlayingTechniqueType::HANDBELLS_LV},
        {ticks_per_measure * 21, PlayingTechniqueType::HANDBELLS_DAMP},
    };

    auto find_expected_type = [&expected_types, score](int tick) -> std::pair<timestamp_t, PlayingTechniqueType> {
        auto it = muse::find_less_or_equal(expected_types, tick);
        if (it == expected_types.end()) {
            return std::make_pair(0, PlayingTechniqueType::NATURAL);
        }

        return std::make_pair(timestamp_from_ticks(score, it->first), it->second);
    };

    for (int tick = 0; tick <= max_tick; tick += ::ticks_step) {
        std::pair<timestamp_t, PlayingTechniqueType> actual_type = ctx.playing_technique(track_idx, tick);
        std::pair<timestamp_t, PlayingTechniqueType> expected_type = find_expected_type(tick);
        EXPECT_EQ(actual_type, expected_type);
    }

    // [WHEN] Find position of Damp
    const timestamp_t actual_damp_position =
        ctx.find_playing_technique_timestamp(track_idx, PlayingTechniqueType::HANDBELLS_DAMP, ticks_per_measure * 20);

    // [THEN] Position is correct
    EXPECT_EQ(actual_damp_position, timestamp_from_ticks(score, ticks_per_measure * 21));

    // [WHEN] Clear the context
    ctx.clear(0, muse::nidx, 0, max_tick);

    // [THEN] No technique parsed, returns the "Natural" type
    for (int tick = 0; tick <= max_tick; tick += ::ticks_step) {
        std::pair<timestamp_t, PlayingTechniqueType> actual_type = ctx.playing_technique(track_idx, tick);
        EXPECT_EQ(actual_type.first, 0);
        EXPECT_EQ(actual_type.second, PlayingTechniqueType::NATURAL);
    }

    delete score;
}

TEST_F(EngravingPlaybackContextTests, PlayTechniques_MeasureRepeats) {
    // [GIVEN] Score with 5 measures. The 1st measure is repeated. There also is a measure repeat on the last 2 measures
    // (so the previous 2 measures will be repeated)
    Score *score = ScoreRW::read_score(::playback_context_test_files_dir + u"play_techniques/play_techniques_measure_repeats.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    const TrackRange track_range = parts.front()->track_range();
    const track_idx_t track_idx = track_range.start_track;

    // [GIVEN] the 1st measure is repeated
    constexpr int repeat_offset_tick = 1920;

    // [GIVEN] Context for parsing playing techniques
    PlaybackContext ctx(score);

    // [WHEN] Parse playing techniques
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [THEN] The playing technique map matches the expectation
    std::map<int, PlayingTechniqueType> expected_types{
        // 1st measure
        {0, PlayingTechniqueType::NATURAL},

        // 2nd measure
        {1920 + repeat_offset_tick, PlayingTechniqueType::MUTE}, // 1st quarter note

        // 3rd measure
        {4320 + repeat_offset_tick, PlayingTechniqueType::DISTORTION}, // 2nd quarter note

        // copy of 2nd measure
        {5760 + repeat_offset_tick, PlayingTechniqueType::MUTE},

        // copy of 3rd measure
        {8160 + repeat_offset_tick, PlayingTechniqueType::DISTORTION},
    };

    for (const auto &pair : expected_types) {
        std::pair<timestamp_t, PlayingTechniqueType> actual_type = ctx.playing_technique(track_idx, pair.first);
        EXPECT_EQ(actual_type.first, timestamp_from_ticks(score, pair.first));
        EXPECT_EQ(actual_type.second, pair.second);
    }

    delete score;
}

TEST_F(EngravingPlaybackContextTests, SoundFlags_TextArticulations) {
    // [GIVEN] Score (piano with 2 staves) with sound flags
    Score *score = ScoreRW::read_score(::playback_context_test_files_dir + u"sound_flags/sound_flags.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    // [GIVEN] Context for parsing sound flags
    PlaybackContext ctx(score);

    // [WHEN] Parse the sound flags
    const Part *part = parts.front();
    const TrackRange track_range = part->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [WHEN] Get the actual articulations
    std::map<timestamp_t, TextArticulationEventList> actual_articulations =
        ctx.text_articulations(track_range.start_track, track_range.end_track);

    // [THEN] Expected articulations
    const layer_idx_t second_staff_layer = static_cast<layer_idx_t>(staff2track(1));

    TextArticulationEvent sul_tasto1_staff, sul_tasto2_staff;
    sul_tasto1_staff.text = u"Sul Tasto";
    sul_tasto1_staff.layer_idx = 0;
    sul_tasto2_staff.text = sul_tasto1_staff.text;
    sul_tasto2_staff.layer_idx = second_staff_layer;

    TextArticulationEvent bartok; // "apply to all staves" is OFF (apply to 1st staff)
    bartok.text = u"bartok";
    bartok.layer_idx = 0;

    TextArticulationEvent pizz; // "apply to all staves" is ON (apply to 2nd staff)
    pizz.text = u"pizzicato";
    pizz.layer_idx = second_staff_layer;

    TextArticulationEvent espressivo; // "apply to all staves" is OFF (apply to 1st staff)
    espressivo.text = u"Espressivo";
    espressivo.layer_idx = second_staff_layer;

    std::map<timestamp_t, TextArticulationEventList> expected_articulations{
        {timestamp_from_ticks(score, 1920), {sul_tasto1_staff, sul_tasto2_staff}},
        {timestamp_from_ticks(score, 3840), {bartok, pizz}},
        {timestamp_from_ticks(score, 7680), {espressivo}},
    };

    EXPECT_EQ(actual_articulations, expected_articulations);

    // [THEN] We can get articulations for a specific track & tick
    for (track_idx_t track_idx = track_range.start_track; track_idx < track_range.end_track; ++track_idx) {
        TextArticulationEvent event = ctx.text_articulation(track_idx, 0);
        EXPECT_TRUE(event.text.empty());

        event = ctx.text_articulation(track_idx, 2000);
        staff_idx_t staff_idx = track2staff(track_idx);

        if (staff_idx == 0) {
            sul_tasto1_staff.flags.set_flag(TextArticulationEvent::STARTS_AT_PLAYBACK_POSITION, false);
            sul_tasto1_staff.layer_idx = static_cast<layer_idx_t>(track_idx);
            EXPECT_EQ(event, sul_tasto1_staff);
        } else {
            sul_tasto2_staff.flags.set_flag(TextArticulationEvent::STARTS_AT_PLAYBACK_POSITION, false);
            sul_tasto2_staff.layer_idx = static_cast<layer_idx_t>(track_idx);
            EXPECT_EQ(event, sul_tasto2_staff);
        }

        event = ctx.text_articulation(track_idx, 4500);

        if (staff_idx == 0) {
            bartok.flags.set_flag(TextArticulationEvent::STARTS_AT_PLAYBACK_POSITION, false);
            bartok.layer_idx = static_cast<layer_idx_t>(track_idx);
            EXPECT_EQ(event, bartok);
        } else {
            pizz.flags.set_flag(TextArticulationEvent::STARTS_AT_PLAYBACK_POSITION, false);
            pizz.layer_idx = static_cast<layer_idx_t>(track_idx);
            EXPECT_EQ(event, pizz);
        }

        event = ctx.text_articulation(track_idx, 7680);

        if (staff_idx == 0) {
            bartok.flags.set_flag(TextArticulationEvent::STARTS_AT_PLAYBACK_POSITION, false);
            bartok.layer_idx = static_cast<layer_idx_t>(track_idx);
            EXPECT_EQ(event, bartok);
        } else {
            espressivo.flags.set_flag(TextArticulationEvent::STARTS_AT_PLAYBACK_POSITION, true);
            espressivo.layer_idx = static_cast<layer_idx_t>(track_idx);
            EXPECT_EQ(event, espressivo);
        }
    }

    delete score;
}

TEST_F(EngravingPlaybackContextTests, SoundFlags_MeasureRepeats) {
    // [GIVEN] Score with 5 measures. There is a measure repeat on the last 2 measures
    // (so the previous 2 measures will be repeated)
    Score *score = ScoreRW::read_score(::playback_context_test_files_dir + u"sound_flags/sound_flags_measure_repeats.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_FALSE(parts.empty());

    // [GIVEN] Context for parsing sound flags
    PlaybackContext ctx(score);

    // [WHEN] Parse sound flags
    const TrackRange track_range = parts.front()->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [THEN] The actual text articulations match the expectation
    TextArticulationEvent espressivo, bartok;
    espressivo.text = u"Espressivo";
    bartok.text = u"bartok";

    std::map<timestamp_t, TextArticulationEventList> expected_articulations{
        {timestamp_from_ticks(score, 1920), {espressivo}}, // 2nd measure
        {timestamp_from_ticks(score, 3840), {bartok}},     // 3rd measure
        {timestamp_from_ticks(score, 5760), {espressivo}}, // measure repeat
        {timestamp_from_ticks(score, 7680), {bartok}},     // measure repeat
    };

    std::map<timestamp_t, TextArticulationEventList> actual_articulations =
        ctx.text_articulations(track_range.start_track, track_range.end_track);
    EXPECT_EQ(expected_articulations, actual_articulations);

    delete score;
}

/**
 * @brief Engraving_PlaybackContextTests_SoundFlags_CancelPlayingTechniques
 *  @details Checks whether Arco & Open & "Ord." correctly cancel playing techniques. See:
 *  https://github.com/musescore/MuseScore/issues/22403
 */
TEST_F(EngravingPlaybackContextTests, SoundFlags_CancelPlayingTechniques) {
    // [GIVEN] Score (violin + brass) with sound flags & playing techniques
    Score *score = ScoreRW::read_score(::playback_context_test_files_dir + u"sound_flags/cancel_playing_technique.mscx");

    const std::vector<Part *> &parts = score->parts();
    ASSERT_EQ(parts.size(), 2);

    // [GIVEN] Context for parsing sound flags & playing techniques
    PlaybackContext ctx(score);

    // [WHEN] Parse the violin part
    const Part *violin_part = parts.at(0);
    const TrackRange violin_track_range = violin_part->track_range();
    const track_idx_t violin_track_idx = violin_track_range.start_track;
    const int score_end_tick = score->end_tick().ticks();
    ctx.update(violin_track_range.start_track, violin_track_range.end_track, 0, score_end_tick);

    // [THEN] 1st measure: Pizz.
    for (int tick = 0; tick < 1920; tick += ::ticks_step) {
        std::pair<timestamp_t, PlayingTechniqueType> actual_type = ctx.playing_technique(violin_track_idx, tick);
        EXPECT_EQ(actual_type.first, 0);
        EXPECT_EQ(actual_type.second, PlayingTechniqueType::PIZZICATO);
    }

    // [THEN] "Natural" for all the other measures, as Pizz. was canceled with "Ord." in the 2nd measure
    const timestamp_t expected_natural_timestamp = timestamp_from_ticks(score, 1920);
    int last_tick = score->last_measure()->tick().ticks();
    for (int tick = 1920; tick < last_tick; tick += ::ticks_step) {
        std::pair<timestamp_t, PlayingTechniqueType> actual_type = ctx.playing_technique(violin_track_idx, tick);
        EXPECT_EQ(actual_type.first, expected_natural_timestamp);
        EXPECT_EQ(actual_type.second, PlayingTechniqueType::NATURAL);
    }

    // [THEN] The actual text articulations match the expectation
    TextArticulationEvent ordinary;
    ordinary.text = mpe::ordinary_playing_technique_code;
    ordinary.layer_idx = 0;

    TextArticulationEvent bartok;
    bartok.text = u"bartok";
    bartok.layer_idx = 0;

    std::map<timestamp_t, TextArticulationEventList> expected_articulations{
        {timestamp_from_ticks(score, 1920), {ordinary}}, // 2nd measure (cancels Pizz.)
        {timestamp_from_ticks(score, 3840), {bartok}},   // 3rd measure
        {timestamp_from_ticks(score, 5760), {ordinary}}, // 4th (canceled by Arco)
    };

    std::map<timestamp_t, TextArticulationEventList> actual_articulations =
        ctx.text_articulations(violin_track_range.start_track, violin_track_range.end_track);
    EXPECT_EQ(expected_articulations, actual_articulations);

    // [WHEN] Parse the brass part
    const Part *brass_part = parts.at(1);
    const TrackRange brass_track_range = brass_part->track_range();
    const track_idx_t brass_track_idx = brass_track_range.start_track;
    ctx.clear(brass_track_range.start_track, brass_track_range.end_track, 0, score_end_tick);
    ctx.update(brass_track_range.start_track, brass_track_range.end_track, 0, score_end_tick);

    // [THEN] 1st measure: Standard
    for (int tick = 0; tick < 1920; tick += ::ticks_step) {
        std::pair<timestamp_t, PlayingTechniqueType> actual_type = ctx.playing_technique(brass_track_idx, tick);
        EXPECT_EQ(actual_type.first, 0);
        EXPECT_EQ(actual_type.second, PlayingTechniqueType::NATURAL);
    }

    // [THEN] "Open" starting from the 2nd measure
    const timestamp_t expected_open_timestamp = timestamp_from_ticks(score, 1920);
    for (int tick = 1920; tick < last_tick; tick += ::ticks_step) {
        std::pair<timestamp_t, PlayingTechniqueType> actual_type = ctx.playing_technique(brass_track_idx, tick);
        EXPECT_EQ(actual_type.first, expected_open_timestamp);
        EXPECT_EQ(actual_type.second, PlayingTechniqueType::OPEN);
    }

    // [THEN] The actual text articulations match the expectation
    const layer_idx_t second_staff_layer = static_cast<layer_idx_t>(staff2track(1));
    ordinary.layer_idx = second_staff_layer;

    TextArticulationEvent mute;
    mute.text = u"mute";
    mute.layer_idx = second_staff_layer;

    expected_articulations = {
        {timestamp_from_ticks(score, 0), {mute}},        // 1st measure
        {timestamp_from_ticks(score, 1920), {ordinary}}, // 2nd measure (canceled by Open)
    };

    actual_articulations = ctx.text_articulations(brass_track_range.start_track, brass_track_range.end_track);
    EXPECT_EQ(expected_articulations, actual_articulations);

    delete score;
}

TEST_F(EngravingPlaybackContextTests, Lyrics_Multiverses) {
    // Score with two verses
    Score *score = ScoreRW::read_score(::playback_context_test_files_dir + u"lyrics_multiverses.mscx");
    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    // [GIVEN] Context for parsing lyrics
    PlaybackContext ctx(score);

    // [WHEN] Parse lyrics
    const Part *part = score->parts().front();
    const TrackRange track_range = part->track_range();
    const int last_tick = score->end_tick().ticks();
    ctx.update(track_range.start_track, track_range.end_track, 0, last_tick);

    // [THEN] Lyrics have been parsed correctly
    auto make_syllable = [](const String &text, bool hyphened_to_next = false) {
        SyllableEvent e;
        e.text = text;
        e.flags.set_flag(SyllableEvent::HYPHENED_TO_NEXT, hyphened_to_next);
        return e;
    };

    const std::map<timestamp_t, SyllableEventList> expected_events{
        // 1st verse
        {0, {make_syllable(u"Wal", true)}},
        {250000, {make_syllable(u"king")}},
        {500000, {make_syllable(u"a", true)}},
        {750000, {make_syllable(u"round")}},
        {1000000, {make_syllable(u"in")}},
        {1250000, {make_syllable(u"the")}},
        {1500000, {make_syllable(u"park")}},

        // 2nd verse
        {2000000, {make_syllable(u"Should")}},
        {2500000, {make_syllable(u"feel")}},
        {3000000, {make_syllable(u"like")}},
        {3500000, {make_syllable(u"work")}},

        // 1st verse
        {4000000, {make_syllable(u"Hello")}},
        {4500000, {make_syllable(u"world")}},

        // 1st verse (repeated)
        {6000000, {make_syllable(u"Hello")}},
        {6500000, {make_syllable(u"world")}},
    };

    const std::map<timestamp_t, SyllableEventList> actual_events = ctx.syllables(track_range.start_track, track_range.end_track);
    EXPECT_EQ(actual_events.size(), expected_events.size());

    for (const auto &pair : actual_events) {
        auto expected_it = expected_events.find(pair.first);
        ASSERT_TRUE(expected_it != expected_events.end());
        ASSERT_EQ(expected_it->second.size(), pair.second.size());

        for (size_t i = 0; i < expected_it->second.size(); ++i) {
            const SyllableEvent &expected_event = expected_it->second.at(i);
            const SyllableEvent &actual_event = pair.second.at(i);

            EXPECT_EQ(actual_event.text, expected_event.text);
            EXPECT_EQ(actual_event.layer_idx, expected_event.layer_idx);
            EXPECT_EQ(actual_event.flags, expected_event.flags);
        }
    }

    delete score;
}
