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
#include <memory>

#include "utils/scorerw.h"

#include "engraving/dom/guitarbend.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"

#include "engraving/playback/renderingcontext.h"
#include "engraving/playback/renderers/bendsrenderer.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static constexpr duration_t quarter_note_duration = 500000; // duration in microseconds for 4/4 120BPM

static constexpr track_idx_t bends_track = staff2track(0);
static constexpr track_idx_t dives_track = staff2track(2);

struct EngravingBendsRendererTests : ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_score = ScoreRW::read_score(u"playback/playbackeventsrenderer_data/guitar_bends/guitar_bends.mscx");

        ASSERT_TRUE(s_score);
        ASSERT_EQ(s_score->parts().size(), 2);
        ASSERT_EQ(s_score->nstaves(), 3);

        s_playback_ctx = std::make_shared<PlaybackContext>(s_score);
        const Part *part = s_score->parts().front();
        const TrackRange track_range = part->track_range();
        const int last_tick = s_score->end_tick().ticks();
        s_playback_ctx->update(track_range.start_track, track_range.end_track, 0, last_tick);

        s_profile = std::make_shared<ArticulationsProfile>();
        s_profile->set_pattern(ArticulationType::STANDARD, build_test_articulation_pattern());
        s_profile->set_pattern(ArticulationType::MULTIBEND, build_test_articulation_pattern());
        s_profile->set_pattern(ArticulationType::PRE_APPOGGIATURA, build_test_articulation_pattern());
        s_profile->set_pattern(ArticulationType::POST_APPOGGIATURA, build_test_articulation_pattern());
        s_profile->set_pattern(ArticulationType::DISTORTION, build_test_articulation_pattern());
    }

    static void TearDownTestSuite() {
        delete s_score;
        s_score = nullptr;
        s_playback_ctx.reset();
        s_profile.reset();
    }

    static ArticulationPattern build_test_articulation_pattern() {
        ArticulationPatternSegment blank_segment(ArrangementPattern(::muse::mpe::hundred_percent /*durationFactor*/, 0 /*timestampOffset*/),
                                                 PitchPattern(ArticulationMap::expected_size, ::muse::mpe::ten_percent, 0),
                                                 ExpressionPattern(ArticulationMap::expected_size, ::muse::mpe::ten_percent, 0));

        ArticulationPattern pattern;
        pattern.emplace(0, std::move(blank_segment));

        return pattern;
    }

    static RenderingContext build_context(const Chord *chord, int tick_offset = 0) {
        return build_rendering_ctx(chord, tick_offset, s_profile, s_playback_ctx);
    }

    static const Chord *find_chord(int tick, track_idx_t track) {
        for (const MeasureBase *mb = s_score->first(); mb; mb = mb->next()) {
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

    static Score *s_score;
    static PlaybackContextPtr s_playback_ctx;
    static ArticulationsProfilePtr s_profile;
};

Score *EngravingBendsRendererTests::s_score = nullptr;
PlaybackContextPtr EngravingBendsRendererTests::s_playback_ctx = nullptr;
ArticulationsProfilePtr EngravingBendsRendererTests::s_profile = nullptr;

/*!
 * @details Render a multibend with the following structure:
 * F3 -> bend -> G3 -> bend -> F3 -> bend -> G3 (appoggiatura) -> grace note bend -> A3 -> hold -> A3 -> bend -> G3 -> A3 (grace after) -> G3 (grace after)
 */
TEST_F(EngravingBendsRendererTests, Multibend) {
    // [GIVEN] First chord of the multibend
    const Chord *start_chord = find_chord(480, ::bends_track);
    ASSERT_TRUE(start_chord);
    ASSERT_EQ(start_chord->notes().size(), 1);

    // [GIVEN] First note of the multibend
    const Note *start_note = start_chord->notes().front();

    // [THEN] Check that the note is recognised as part of the multibend
    EXPECT_TRUE(BendsRenderer::is_multibend_part(start_note));

    // [GIVEN] Context of the chord
    RenderingContext start_chord_ctx = build_context(start_chord);

    // [WHEN] Render the note
    PlaybackEventList events;
    BendsRenderer::render(start_note, start_chord_ctx, events);

    // [THEN] Render the multibend as a single NoteEvent
    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(events.front()));

    // [THEN] The note event has the correct timestamp and duration
    const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(events.front());
    EXPECT_EQ(note_event.arrangement_ctx().actual_timestamp, ::quarter_note_duration);    // starts after a quarter rest
    EXPECT_EQ(note_event.arrangement_ctx().actual_duration, ::quarter_note_duration * 6); // F3 + G3 + F3 + A3 + A3 + G3
    EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 3));

    // [THEN] The note event contains the multibend articulation with the correct timestamp and duration
    auto multibend_it = note_event.expression_ctx().articulations.find(ArticulationType::MULTIBEND);
    ASSERT_TRUE(multibend_it != note_event.expression_ctx().articulations.end());
    const mpe::ArticulationMeta &multibend_meta = multibend_it->second.meta;
    EXPECT_EQ(multibend_meta.timestamp, note_event.arrangement_ctx().actual_timestamp);
    EXPECT_EQ(multibend_meta.overall_duration, note_event.arrangement_ctx().actual_duration);

    // [THEN] The note event contains the distortion articulation (persistent)
    EXPECT_TRUE(note_event.expression_ctx().articulations.contains(ArticulationType::DISTORTION));

    // [THEN] The pitch curve is correct
    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, 0);      // F3
    expected_pitch_curve.emplace(1600, 100); // F3 -> G3
    expected_pitch_curve.emplace(3300, 0);   //  G3 -> F3
    expected_pitch_curve.emplace(5000, 100); // F3 -> G3 (appoggiatura)
    expected_pitch_curve.emplace(5800, 200); // G3 (appoggiatura) -> A3
    expected_pitch_curve.emplace(8300, 100); // A3 -> hold -> G3
    // See: https://github.com/musescore/MuseScore/issues/28645
    expected_pitch_curve.emplace(9100, 200); // G3 -> A3 (grace 8th after)
    expected_pitch_curve.emplace(9500, 100); // A3 -> G3 (grace 8th after)

    EXPECT_EQ(note_event.pitch_ctx().pitch_curve, expected_pitch_curve);

    // [THEN] Check we don't render other notes of the multibend (only the first one)
    const GuitarBend *bend = start_chord->notes().front()->bend_for();
    ASSERT_TRUE(bend);

    while (bend) {
        const Note *note = bend->end_note();
        ASSERT_TRUE(note);
        EXPECT_TRUE(BendsRenderer::is_multibend_part(note));

        RenderingContext ctx = build_context(note->chord());

        events.clear();
        BendsRenderer::render(note, ctx, events);
        EXPECT_TRUE(events.empty());

        bend = note->bend_for();
    }
}

/*!
 * @details Render a chord with 2 bends and 1 note:
 * 1st bend: D3 -> E3
 * 2nd bend: G3 -> A3
 * note event: C4
 */
TEST_F(EngravingBendsRendererTests, MultipleBendsOnOneChord) {
    // [GIVEN] Chord with multiple notes
    const Chord *chord = find_chord(3840, ::bends_track);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 3);

    // [GIVEN] Context of the chord
    RenderingContext ctx = build_context(chord);

    // [WHEN] Render the chord
    PlaybackEventList events;
    for (const Note *note : chord->notes()) {
        BendsRenderer::render(note, ctx, events);
    }

    // [THEN] 3 events: 2 bends + 1 note
    ASSERT_EQ(events.size(), 3);
    for (const mpe::PlaybackEvent &event : events) {
        ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(event));
    }

    PitchCurve expected_bend_curve;
    expected_bend_curve.emplace(0, 0);
    expected_bend_curve.emplace(5000, 100);

    const mpe::NoteEvent &bend_event1 = std::get<mpe::NoteEvent>(events.at(0));
    EXPECT_EQ(bend_event1.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::D, 3));
    EXPECT_EQ(bend_event1.pitch_ctx().pitch_curve, expected_bend_curve);

    const mpe::NoteEvent &bend_event2 = std::get<mpe::NoteEvent>(events.at(1));
    EXPECT_EQ(bend_event2.pitch_ctx().pitch_curve, expected_bend_curve);
    EXPECT_EQ(bend_event2.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::G, 3));

    const mpe::NoteEvent &note_event = std::get<mpe::NoteEvent>(events.at(2));
    EXPECT_NE(note_event.pitch_ctx().pitch_curve, expected_bend_curve);
    EXPECT_EQ(note_event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::C, 4));
}

/*!
 * @details Render a pre-bend with the following structure:
 * Pre-bend G3 up to A3 -> hold for 1 quarter -> release down to G3
 * Check that we ignore the grace note (A3) and render only the principal note (G3)
 */
TEST_F(EngravingBendsRendererTests, PreBend) {
    // [GIVEN] Chord with a pre-bend
    const Chord *chord = find_chord(5760, ::bends_track);
    ASSERT_TRUE(chord);

    // [GIVEN] Pre-bend grace note (unplayable)
    ASSERT_EQ(chord->grace_notes_before().size(), 1);
    const Chord *grace_chord = chord->grace_notes_before().front();
    ASSERT_EQ(grace_chord->notes().size(), 1);
    const Note *unplayable_grace_pre_bend_note = grace_chord->notes().front();
    ASSERT_TRUE(unplayable_grace_pre_bend_note->is_pre_bend_or_dive_start());

    // [GIVEN] Playable principal note
    ASSERT_EQ(chord->notes().size(), 1);
    const Note *playble_principal_note = chord->notes().front();
    ASSERT_TRUE(playble_principal_note->bend_back());
    ASSERT_EQ(playble_principal_note->bend_back()->start_note(), unplayable_grace_pre_bend_note);

    // [GIVEN] Context of the chord
    RenderingContext ctx = build_context(chord);

    // [WHEN] Render the unplayable pre-bend grace note
    PlaybackEventList events;
    BendsRenderer::render(unplayable_grace_pre_bend_note, ctx, events);

    // [THEN] No events
    EXPECT_TRUE(events.empty());

    // [WHEN] Render the first playable (principal) note
    BendsRenderer::render(playble_principal_note, ctx, events);

    // [THEN] Note successfully rendered
    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(events.front()));

    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, 0);       // A3
    expected_pitch_curve.emplace(3300, 0);    // tied A3
    expected_pitch_curve.emplace(6600, -100); // Release down to G3

    const mpe::NoteEvent &event = std::get<mpe::NoteEvent>(events.front());
    EXPECT_EQ(event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 3)); // A3
    EXPECT_EQ(event.arrangement_ctx().actual_duration, ::quarter_note_duration * 3); // A3 + A3 + G3
    EXPECT_EQ(event.pitch_ctx().pitch_curve, expected_pitch_curve);
}

/*!
 * @details Render a quarter note (A3) with a slight bend
 */
TEST_F(EngravingBendsRendererTests, SlightBend) {
    // [GIVEN] Quarter note with a slight bend
    const Chord *chord = find_chord(7680, ::bends_track);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);
    const Note *note = chord->notes().front();

    // [GIVEN] Context of the chord
    RenderingContext ctx = build_context(chord);

    // [WHEN] Render the note
    PlaybackEventList events;
    BendsRenderer::render(note, ctx, events);

    // [THEN] Note successfully rendered
    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(events.front()));

    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, 0);      // A3
    expected_pitch_curve.emplace(10000, 25); // 1/4

    const mpe::NoteEvent &event = std::get<mpe::NoteEvent>(events.front());
    EXPECT_EQ(event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 3));
    EXPECT_EQ(event.pitch_ctx().pitch_curve, expected_pitch_curve);

    auto art_it = event.expression_ctx().articulations.find(mpe::ArticulationType::MULTIBEND);
    EXPECT_TRUE(art_it != event.expression_ctx().articulations.end());

    const ArticulationMeta &meta = art_it->second.meta;
    EXPECT_EQ(meta.timestamp, timestamp_from_ticks(s_score, note->tick().ticks()));
    EXPECT_EQ(meta.overall_duration, ::quarter_note_duration);
}

/*!
 * @details Render a multibend with custom time offsets
 * E3 (pre-appoggiatura) -> F3, start offset: 25%, end offset: 75%
 * F3 -> G3, start offset: 25%, end offset: 100%
 * G3 -> F3, start offset: 25%, end offset: 50%
 */
TEST_F(EngravingBendsRendererTests, Multibend_CustomTimeOffsets) {
    // [GIVEN] First chord of the multibend
    const Chord *start_chord = find_chord(9600, ::bends_track);
    ASSERT_TRUE(start_chord);

    // [GIVEN] First (grace) note of the multibend
    ASSERT_EQ(start_chord->grace_notes_before().size(), 1);
    const Chord *grace_chord = start_chord->grace_notes_before().front();
    ASSERT_EQ(grace_chord->notes().size(), 1);
    const Note *start_grace_note = grace_chord->notes().front();

    // [GIVEN] Context of the chord
    RenderingContext ctx = build_context(start_chord);

    // [WHEN] Render the note
    PlaybackEventList events;
    BendsRenderer::render(start_grace_note, ctx, events);

    // [THEN] Note successfully rendered
    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(events.front()));

    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, 0);
    expected_pitch_curve.emplace(400, 0);    // start offset: 25%
    expected_pitch_curve.emplace(1200, 50);  // end offset: 75%
    expected_pitch_curve.emplace(2450, 50);  // start offset: 25%
    expected_pitch_curve.emplace(3300, 250); // end offset: 100%
    expected_pitch_curve.emplace(4125, 250); // start offset: 25%
    expected_pitch_curve.emplace(4950, 150); // end offset: 50%

    const mpe::NoteEvent &event = std::get<mpe::NoteEvent>(events.front());
    EXPECT_EQ(event.arrangement_ctx().actual_duration, ::quarter_note_duration * 3);
    EXPECT_EQ(event.pitch_ctx().pitch_curve, expected_pitch_curve);
}

/*!
 * @details Checks we can render a bend starting from a tied note
 * See: https://github.com/musescore/MuseScore/issues/21345
 */
TEST_F(EngravingBendsRendererTests, BendOnTiedNotes) {
    // [GIVEN] First chord of the multibend
    const Chord *chord = find_chord(11520, ::bends_track);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);

    // [GIVEN] Context of the chord
    RenderingContext ctx = build_context(chord);

    // [GIVEN] Quarter A3 tied to another quarter A3
    const Note *note = chord->notes().front();
    ASSERT_TRUE(note->tie_for());
    ASSERT_FALSE(note->tie_back());

    // [THEN] Check that the note is recognised as part of the multibend
    EXPECT_TRUE(BendsRenderer::is_multibend_part(note));

    // [WHEN] Render the tied note
    PlaybackEventList events;
    BendsRenderer::render(note, ctx, events);

    // [THEN] Note successfully rendered
    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(events.front()));

    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, 0);
    expected_pitch_curve.emplace(3300, 0);   // tied A3
    expected_pitch_curve.emplace(6600, 100); // B3

    const mpe::NoteEvent &event = std::get<mpe::NoteEvent>(events.front());
    EXPECT_EQ(event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 3));
    EXPECT_EQ(event.pitch_ctx().pitch_curve, expected_pitch_curve);
    EXPECT_EQ(event.arrangement_ctx().actual_duration, ::quarter_note_duration * 3); // 2 tied A3 + B3

    // [THEN] The note event contains the multibend articulation with the correct timestamp and duration
    auto multibend_it = event.expression_ctx().articulations.find(ArticulationType::MULTIBEND);
    ASSERT_TRUE(multibend_it != event.expression_ctx().articulations.end());

    const mpe::ArticulationAppliedData &articulation_data = multibend_it->second;
    EXPECT_NE(articulation_data.occupied_from_value, 0);
    EXPECT_EQ(articulation_data.occupied_to_value, ::muse::mpe::hundred_percent);

    const mpe::ArticulationMeta &multibend_meta = articulation_data.meta;
    EXPECT_EQ(multibend_meta.timestamp, event.arrangement_ctx().actual_timestamp);
    EXPECT_EQ(multibend_meta.overall_duration, event.arrangement_ctx().actual_duration);
}

/*!
 * @details Render 3 tied quarter note (A3) with 2 dips on them (on the 1st and 2nd notes)
 */
TEST_F(EngravingBendsRendererTests, Dip) {
    // [GIVEN] Quarter note with a slight bend
    const Chord *chord = find_chord(480, ::dives_track);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);
    const Note *note = chord->notes().front();

    // [GIVEN] Context of the chord
    RenderingContext ctx = build_context(chord);

    // [WHEN] Render the note
    PlaybackEventList events;
    BendsRenderer::render(note, ctx, events);

    // [THEN] Note successfully rendered
    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(events.front()));

    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, 0);      // A3
    expected_pitch_curve.emplace(800, -50);  // Go down (-1/2)
    expected_pitch_curve.emplace(1600, 0);   // Go to the origin pitch
    expected_pitch_curve.emplace(3300, 0);   // Hold A3 (tied notes)
    expected_pitch_curve.emplace(4100, -50); // Go down (-1/2)
    expected_pitch_curve.emplace(5000, 0);   // Go to the origin pitch

    const mpe::NoteEvent &event = std::get<mpe::NoteEvent>(events.front());
    EXPECT_EQ(event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 3));
    EXPECT_EQ(event.pitch_ctx().pitch_curve, expected_pitch_curve);

    auto art_it = event.expression_ctx().articulations.find(mpe::ArticulationType::MULTIBEND);
    EXPECT_TRUE(art_it != event.expression_ctx().articulations.end());

    const ArticulationMeta &meta = art_it->second.meta;
    EXPECT_EQ(meta.timestamp, timestamp_from_ticks(s_score, note->tick().ticks()));
    EXPECT_EQ(meta.overall_duration, ::quarter_note_duration * 3); // 3 tied quarter notes
}

/*!
 * @details Render a quarter note (B3) with a scoop
 */
TEST_F(EngravingBendsRendererTests, Scoop) {
    // [GIVEN] Quarter note with a slight bend
    const Chord *chord = find_chord(1920, ::dives_track);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->notes().size(), 1);
    const Note *note = chord->notes().front();

    // [GIVEN] Context of the chord
    RenderingContext ctx = build_context(chord);

    // [WHEN] Render the note
    PlaybackEventList events;
    BendsRenderer::render(note, ctx, events);

    // [THEN] Note successfully rendered
    ASSERT_EQ(events.size(), 1);
    ASSERT_TRUE(std::holds_alternative<mpe::NoteEvent>(events.front()));

    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, -50);  // Go down (-1/2)
    expected_pitch_curve.emplace(2500, 0); // Go to the origin pitch (B3)

    const mpe::NoteEvent &event = std::get<mpe::NoteEvent>(events.front());
    EXPECT_EQ(event.pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::B, 3));
    EXPECT_EQ(event.pitch_ctx().pitch_curve, expected_pitch_curve);

    auto art_it = event.expression_ctx().articulations.find(mpe::ArticulationType::MULTIBEND);
    EXPECT_TRUE(art_it != event.expression_ctx().articulations.end());

    const ArticulationMeta &meta = art_it->second.meta;
    EXPECT_EQ(meta.timestamp, timestamp_from_ticks(s_score, note->tick().ticks()));
    EXPECT_EQ(meta.overall_duration, ::quarter_note_duration);
}
