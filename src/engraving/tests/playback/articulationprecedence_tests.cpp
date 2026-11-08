// SPDX-License-Identifier: GPL-3.0-only

// Witnesses for the normalized articulation precedence (playback refactor step 1). Every case first
// asserts that its input really contains the conflicting articulations, then checks the selected outcome.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "audio/engine/internal/synthesizers/fluidsynth/fluidsequencer.h"
#include "audio/engine/internal/synthesizers/fluidsynth/soundmapping.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/part.h"
#include "engraving/playback/metaparsers/chordarticulationsparser.h"
#include "engraving/playback/playbackeventsrenderer.h"
#include "engraving/playback/renderers/bendsrenderer.h"
#include "engraving/playback/renderingcontext.h"
#include "engraving/playback/utils/expressionutils.h"
#include "mpe/tests/utils/articulationutils.h"
#include "utils/scorerw.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;
using namespace muse::audio::synth;

namespace {
constexpr duration_t quarter_note_duration = 500000; // 4/4 at 120 BPM
constexpr duration_t eighth_note_duration = quarter_note_duration / 2;

ArticulationPattern pattern_with(PitchPattern pitch, ExpressionPattern expression) {
    ArticulationPattern pattern;
    pattern.emplace(
        0, ArticulationPatternSegment(tests::create_arrangement_pattern(hundred_percent, 0), std::move(pitch), std::move(expression)));
    return pattern;
}

ArticulationPattern blank_pattern() { return pattern_with(PitchPattern(), ExpressionPattern()); }

void add_articulation(ArticulationMap &map, const ArticulationType type, const ArticulationPattern &pattern, const timestamp_t timestamp,
                      const duration_t duration) {
    map.emplace(type, ArticulationAppliedData(ArticulationMeta(type, pattern, timestamp, duration), 0, hundred_percent));
}

mpe::NoteEvent make_note(ArticulationMap articulations, const timestamp_t timestamp = 0, const duration_t duration = quarter_note_duration,
                         const PitchCurve &pitch_curve = {}) {
    articulations.pre_calculate_average_data();
    return mpe::NoteEvent(timestamp, duration, 0, 0, pitch_level(PitchClass::C, 4), dynamic_level_from_type(mpe::DynamicType::NATURAL),
                          articulations, 2.0, 0.f, pitch_curve);
}

mpe::NoteEvent make_typed_note(const std::vector<ArticulationType> &types) {
    ArticulationMap articulations;
    for (const ArticulationType type : types) {
        add_articulation(articulations, type, blank_pattern(), 0, quarter_note_duration);
    }
    return make_note(std::move(articulations));
}

// Exposes the sequenced MIDI timeline that load() builds
struct TimelineSequencer : FluidSequencer {
    using FluidSequencer::main_stream_events_;
};

void load_sequencer(TimelineSequencer &sequencer, const PlaybackSetupData &setup, const mpe::NoteEvent &event,
                    const bool use_dynamic_events) {
    sequencer.init(setup, std::nullopt, use_dynamic_events, 16);

    PlaybackData data;
    data.setup_data = setup;
    data.origin_events.emplace(event.arrangement_ctx().actual_timestamp, PlaybackEventList{event});
    sequencer.load(data);
}

std::vector<const midi::Event *> midi_events_at(const TimelineSequencer &sequencer, const timestamp_t timestamp,
                                                const midi::Event::Opcode opcode) {
    std::vector<const midi::Event *> result;
    for (const auto &[at, events] : sequencer.main_stream_events_) {
        if (at.raw() != timestamp) {
            continue;
        }
        for (const auto &event : events) {
            const midi::Event &midi_event = std::get<midi::Event>(event);
            if (midi_event.opcode() == opcode) {
                result.push_back(&midi_event);
            }
        }
    }
    return result;
}

const Chord *find_chord(const Score *score, const int tick, const track_idx_t track) {
    for (const MeasureBase *mb = score->first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) {
            continue;
        }
        if (const Chord *chord = to_measure(mb)->find_chord(Fraction::from_ticks(tick), track)) {
            return chord;
        }
    }
    return nullptr;
}

void add_grace(Score *score, const Chord *chord, const int pitch, const NoteType type) {
    score->start_cmd(TranslatableString::untranslatable("Articulation precedence tests"));
    score->set_grace_note(const_cast<Chord *>(chord), pitch, type, Constants::division / 2);
    score->end_cmd();
}

// The common articulations the renderers see for this chord
ArticulationMap chord_articulations(const Chord *chord, const ArticulationsProfilePtr &profile, const PlaybackContextPtr &playback_ctx) {
    RenderingContext ctx = build_rendering_ctx(chord, 0, profile, playback_ctx);
    ChordArticulationsParser::build_chord_articulation_map(chord, ctx, ctx.common_articulations);
    return ctx.common_articulations;
}
} // namespace

TEST(EngravingArticulationPrecedenceTests, GraceSelectionTable) {
    const ArticulationType grace_types[] = {ArticulationType::PRE_APPOGGIATURA, ArticulationType::ACCIACCATURA,
                                            ArticulationType::POST_APPOGGIATURA};

    // every non-empty combination, with an unrelated articulation that must never be selected
    for (unsigned mask = 1; mask < 8; ++mask) {
        ArticulationMap articulations;
        add_articulation(articulations, ArticulationType::STANDARD, blank_pattern(), 0, quarter_note_duration);
        ArticulationType expected = ArticulationType::UNDEFINED;
        for (unsigned bit = 0; bit < 3; ++bit) {
            if (mask & (1u << bit)) {
                add_articulation(articulations, grace_types[bit], blank_pattern(), 0, quarter_note_duration);
                if (expected == ArticulationType::UNDEFINED) {
                    expected = grace_types[bit];
                }
            }
        }

        EXPECT_EQ(select_grace_type(articulations), expected) << "mask " << mask;
    }

    ArticulationMap no_graces;
    add_articulation(no_graces, ArticulationType::STANDARD, blank_pattern(), 0, quarter_note_duration);
    EXPECT_EQ(select_grace_type(no_graces), ArticulationType::UNDEFINED);
}

// (a) Ordinary selector: a principal with a grace before and a grace after renders only the before group
TEST(EngravingArticulationPrecedenceTests, GraceBeforeAndAfterOneChord) {
    struct Case {
        NoteType before_type;
        ArticulationType expected;
        duration_t grace_duration;
    };

    // a single appoggiatura takes half the principal; an acciaccatura a 64th
    const Case cases[] = {{NoteType::APPOGGIATURA, ArticulationType::PRE_APPOGGIATURA, eighth_note_duration},
                          {NoteType::ACCIACCATURA, ArticulationType::ACCIACCATURA, quarter_note_duration / 16}};

    for (const Case &c : cases) {
        // F4 quarter with a G4 eighth grace after; add an A4 grace before
        Score *score =
            ScoreRW::read_score(u"playback/playbackeventsrenderer_data/single_note_appoggiatura_post/single_note_appoggiatura_post.mscx");
        ASSERT_TRUE(score);
        const Chord *chord = find_chord(score, 0, 0);
        ASSERT_TRUE(chord);
        add_grace(score, chord, 69, c.before_type);

        ArticulationsProfilePtr profile = std::make_shared<ArticulationsProfile>();
        for (const ArticulationType type :
             {ArticulationType::PRE_APPOGGIATURA, ArticulationType::ACCIACCATURA, ArticulationType::POST_APPOGGIATURA}) {
            profile->set_pattern(type, blank_pattern());
        }
        PlaybackContextPtr playback_ctx = std::make_shared<PlaybackContext>(score);

        // [GIVEN] The input carries both grace groups
        const ArticulationMap articulations = chord_articulations(chord, profile, playback_ctx);
        ASSERT_TRUE(articulations.contains(c.expected));
        ASSERT_TRUE(articulations.contains(ArticulationType::POST_APPOGGIATURA));
        EXPECT_EQ(select_grace_type(articulations), c.expected);

        // [WHEN] Render the chord
        PlaybackEventsMap result;
        PlaybackEventsRenderer renderer;
        renderer.render(chord, 0, profile, playback_ctx, result);

        // [THEN] The before grace, then the delayed principal; the after grace is not rendered
        std::vector<const mpe::NoteEvent *> notes;
        for (const auto &pair : result) {
            for (const PlaybackEvent &event : pair.second) {
                notes.push_back(&std::get<mpe::NoteEvent>(event));
            }
        }

        ASSERT_EQ(notes.size(), 2);
        EXPECT_EQ(notes[0]->pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 4));
        EXPECT_EQ(notes[0]->arrangement_ctx().nominal_timestamp, 0);
        EXPECT_EQ(notes[0]->arrangement_ctx().nominal_duration, c.grace_duration);
        EXPECT_EQ(notes[1]->pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4));
        EXPECT_EQ(notes[1]->arrangement_ctx().nominal_timestamp, c.grace_duration);
        EXPECT_EQ(notes[1]->arrangement_ctx().nominal_duration, quarter_note_duration - c.grace_duration);

        delete score;
    }
}

// (a) Multibend selector: adding a grace after to a bent principal keeps its bent appoggiatura in the curve
TEST(EngravingArticulationPrecedenceTests, GraceBeforeAndAfterInMultibend) {
    Score *score = ScoreRW::read_score(u"playback/playbackeventsrenderer_data/guitar_bends/guitar_bends.mscx");
    ASSERT_TRUE(score);

    // A3 at the start of bar 2: a G3 appoggiatura bends into it; give it a B3 grace after too
    const Chord *principal = find_chord(score, 1920, 0);
    ASSERT_TRUE(principal);
    ASSERT_EQ(principal->notes().front()->pitch(), 57);
    ASSERT_EQ(principal->grace_notes_before().size(), 1);
    add_grace(score, principal, 59, NoteType::GRACE8_AFTER);
    ASSERT_EQ(principal->grace_notes_after().size(), 1);

    ArticulationsProfilePtr profile = std::make_shared<ArticulationsProfile>();
    const ArticulationPattern zero_pattern = pattern_with(PitchPattern(ArticulationMap::expected_size, ten_percent, 0),
                                                          ExpressionPattern(ArticulationMap::expected_size, ten_percent, 0));
    for (const ArticulationType type : {ArticulationType::STANDARD, ArticulationType::MULTIBEND, ArticulationType::PRE_APPOGGIATURA,
                                        ArticulationType::POST_APPOGGIATURA, ArticulationType::DISTORTION}) {
        profile->set_pattern(type, zero_pattern);
    }

    PlaybackContextPtr playback_ctx = std::make_shared<PlaybackContext>(score);
    const Part *part = score->parts().front();
    playback_ctx->update(part->track_range().start_track, part->track_range().end_track, 0, score->end_tick().ticks());

    // [GIVEN] The bent principal carries both grace groups
    const ArticulationMap articulations = chord_articulations(principal, profile, playback_ctx);
    ASSERT_TRUE(articulations.contains(ArticulationType::PRE_APPOGGIATURA));
    ASSERT_TRUE(articulations.contains(ArticulationType::POST_APPOGGIATURA));
    EXPECT_EQ(select_grace_type(articulations), ArticulationType::PRE_APPOGGIATURA);

    // [WHEN] Render the multibend from its first note
    const Chord *start_chord = find_chord(score, 480, 0);
    ASSERT_TRUE(start_chord);
    PlaybackEventList events;
    BendsRenderer::render(start_chord->notes().front(), build_rendering_ctx(start_chord, 0, profile, playback_ctx), events);

    // [THEN] The curve is the one without the grace after (see EngravingBendsRendererTests.Multibend): the
    //        bent G3 appoggiatura is still rendered
    ASSERT_EQ(events.size(), 1);
    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, 0);
    expected_pitch_curve.emplace(1600, 100);
    expected_pitch_curve.emplace(3300, 0);
    expected_pitch_curve.emplace(5000, 100); // F3 -> G3 (appoggiatura)
    expected_pitch_curve.emplace(5800, 200); // G3 (appoggiatura) -> A3
    expected_pitch_curve.emplace(8300, 100);
    expected_pitch_curve.emplace(9100, 200);
    expected_pitch_curve.emplace(9500, 100);
    EXPECT_EQ(std::get<mpe::NoteEvent>(events.front()).pitch_ctx().pitch_curve, expected_pitch_curve);

    delete score;
}

// (b) Program choice: technique over timbre, the next channel allocation and the bypasses
TEST(EngravingArticulationPrecedenceTests, ProgramChoice) {
    struct Case {
        PlaybackSetupData setup;
        ArticulationType technique;
        ArticulationType timbre;
    };

    const Case cases[] = {
        {PlaybackSetupData(SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC, SoundSubCategory::PLUCKED}),
         ArticulationType::HARMONIC, ArticulationType::DISTORTION},
        {PlaybackSetupData(SoundId::VIOLIN, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL}), ArticulationType::PIZZICATO,
         ArticulationType::MUTE},
    };

    for (const Case &c : cases) {
        const ArticulationMapping &mapping = articulation_sounds(c.setup);
        ASSERT_TRUE(mapping.contains(c.technique));
        ASSERT_TRUE(mapping.contains(c.timbre));
        ASSERT_NE(mapping.at(c.technique), mapping.at(c.timbre));

        std::vector<std::pair<midi::channel_t, midi::Program>> added;
        ChannelMap channels;
        channels.on_channel_added = [&added](const midi::channel_t channel, const midi::Program &program) {
            added.emplace_back(channel, program);
        };
        channels.init(c.setup, std::nullopt, 16);
        ASSERT_EQ(channels.channel_count(), 1);

        // [GIVEN] One note maps both articulations
        const mpe::NoteEvent both = make_typed_note({c.timbre, c.technique});
        ASSERT_EQ(both.expression_ctx().articulations.size(), 2);
        EXPECT_EQ(channels.select_program_articulation(both.expression_ctx().articulations), c.technique);

        // [THEN] The technique's program gets the next channel
        EXPECT_EQ(channels.resolve_channel_for_event(both), 1);
        ASSERT_EQ(added.size(), 2);
        EXPECT_EQ(added[1], std::make_pair(midi::channel_t(1), mapping.at(c.technique)));

        // [THEN] The timbre alone allocates the channel after it; the pair reuses its own
        EXPECT_EQ(channels.resolve_channel_for_event(make_typed_note({c.timbre})), 2);
        ASSERT_EQ(added.size(), 3);
        EXPECT_EQ(added[2], std::make_pair(midi::channel_t(2), mapping.at(c.timbre)));
        EXPECT_EQ(channels.resolve_channel_for_event(make_typed_note({c.technique, c.timbre})), 1);
        EXPECT_EQ(channels.channel_count(), 3);

        // [THEN] STANDARD bypasses the table
        EXPECT_EQ(channels.resolve_channel_for_event(make_typed_note({ArticulationType::STANDARD, c.technique, c.timbre})), 0);

        // [THEN] A program override bypasses it too
        ChannelMap overridden;
        overridden.init(c.setup, midi::Program(0, 5), 16);
        EXPECT_EQ(overridden.resolve_channel_for_event(both), 0);
        EXPECT_EQ(overridden.channel_count(), 1);
    }
}

// (c) Averaging fallback when no articulation has a meaningful dynamic: the lowest type's curve is used
TEST(EngravingArticulationPrecedenceTests, NaturalFallback) {
    const ArticulationPattern empty_curve = blank_pattern();
    const ArticulationPattern natural_curve = pattern_with(
        PitchPattern(), ExpressionPattern(ArticulationMap::expected_size, ten_percent, dynamic_level_from_type(mpe::DynamicType::NATURAL)));
    ASSERT_TRUE(empty_curve.cbegin()->second.expression_pattern.dynamic_offset_map.empty());
    ASSERT_EQ(natural_curve.cbegin()->second.expression_pattern.max_amplitude_level(), dynamic_level_from_type(mpe::DynamicType::NATURAL));

    struct Case {
        const ArticulationPattern &staccato; // the lower type
        const ArticulationPattern &tenuto;
        bool expect_empty;
    };

    const Case cases[] = {{empty_curve, natural_curve, true}, {natural_curve, empty_curve, false}};
    std::vector<midi::velocity_t> velocities_without_snd;

    for (const Case &c : cases) {
        // [GIVEN] Two articulations, neither with a meaningful dynamic offset, one curve empty
        ArticulationMap articulations;
        add_articulation(articulations, ArticulationType::STACCATO, c.staccato, 0, quarter_note_duration);
        add_articulation(articulations, ArticulationType::TENUTO, c.tenuto, 0, quarter_note_duration);
        const mpe::NoteEvent event = make_note(articulations);
        const ArticulationMap &applied = event.expression_ctx().articulations;
        ASSERT_EQ(applied.size(), 2);

        // [THEN] The cached averages come from STACCATO
        EXPECT_EQ(applied.average_dynamic_offset_map().empty(), c.expect_empty);
        EXPECT_EQ(applied.average_max_amplitude_level(), c.expect_empty ? 0 : dynamic_level_from_type(mpe::DynamicType::NATURAL));
        EXPECT_EQ(event.expression_ctx().expression_curve.empty(), c.expect_empty);

        for (const bool single_note_dynamics : {true, false}) {
            const PlaybackSetupData setup(SoundId::PIANO, SoundCategory::KEYBOARDS, {}, single_note_dynamics);
            TimelineSequencer sequencer;
            load_sequencer(sequencer, setup, event, single_note_dynamics);
            const auto note_on = midi_events_at(sequencer, 0, midi::Event::Opcode::NOTE_ON);
            ASSERT_EQ(note_on.size(), 1);

            if (single_note_dynamics) {
                // [THEN] An empty curve is velocity fraction 0.5, a flat curve from key 0 is 1.0
                EXPECT_EQ(note_on.front()->velocity16(), c.expect_empty ? 32768 : 65535);
            } else {
                velocities_without_snd.push_back(note_on.front()->velocity16());
            }
        }
    }

    // [THEN] Without single-note dynamics both paths end at the nominal level
    ASSERT_EQ(velocities_without_snd.size(), 2);
    EXPECT_EQ(velocities_without_snd[0], velocities_without_snd[1]);
}

// (d) Overlapping bends: one bend's centre reset lands on the other's sample. Both are emitted in ascending
//     type order, so the higher type's value is the one that holds
TEST(EngravingArticulationPrecedenceTests, OverlappingBendResetCollision) {
    constexpr timestamp_t collision = quarter_note_duration;
    PitchCurve curve; // shared by both bends, with a key at 50%
    curve.emplace(0, 0);
    curve.emplace(fifty_percent, 100);
    curve.emplace(hundred_percent, 100);

    // the short bend resets at `collision`, where the long one samples its 50% key
    for (const bool short_is_lower : {true, false}) {
        const ArticulationType short_bend = short_is_lower ? ArticulationType::FALL : ArticulationType::MULTIBEND;
        const ArticulationType long_bend = short_is_lower ? ArticulationType::MULTIBEND : ArticulationType::FALL;
        ASSERT_LT(ArticulationType::FALL, ArticulationType::MULTIBEND);

        // [GIVEN] Two bends on one note, the short one over the first half
        ArticulationMap articulations;
        add_articulation(articulations, short_bend, blank_pattern(), 0, collision);
        add_articulation(articulations, long_bend, blank_pattern(), 0, 2 * collision);
        const mpe::NoteEvent event = make_note(articulations, 0, 2 * collision, curve);

        const ArticulationMap &applied = event.expression_ctx().articulations;
        ASSERT_TRUE(bend_supported_types.contains(short_bend));
        ASSERT_TRUE(bend_supported_types.contains(long_bend));
        ASSERT_EQ(applied.at(short_bend).meta.timestamp + applied.at(short_bend).meta.overall_duration, collision);
        ASSERT_EQ(applied.at(long_bend).meta.overall_duration * fifty_percent / hundred_percent, collision);

        TimelineSequencer sequencer;
        load_sequencer(sequencer, PlaybackSetupData(SoundId::PIANO, SoundCategory::KEYBOARDS), event, false);

        // [THEN] FALL's event comes first and MULTIBEND's last: a short MULTIBEND's reset wins over FALL's sample
        const auto bends = midi_events_at(sequencer, collision, midi::Event::Opcode::PITCH_BEND);
        ASSERT_EQ(bends.size(), 2);
        if (short_is_lower) {
            EXPECT_EQ(bends[0]->data(), 8192);
            EXPECT_NE(bends[1]->data(), 8192);
        } else {
            EXPECT_NE(bends[0]->data(), 8192);
            EXPECT_EQ(bends[1]->data(), 8192);
        }
    }
}

// Chord-articulation precedence: a rolled chord with a trill stays rolled (arpeggios first, then enum order)
TEST(EngravingArticulationPrecedenceTests, ArpeggioOverOrnament) {
    // F4+A4+C5 sixteenth with an arpeggio; add a trill
    Score *score = ScoreRW::read_score(u"playback/playbackeventsrenderer_data/chord_arpeggio/chord_arpeggio.mscx");
    ASSERT_TRUE(score);
    Chord *chord = const_cast<Chord *>(find_chord(score, 0, 0));
    ASSERT_TRUE(chord);
    ASSERT_TRUE(chord->arpeggio());
    score->start_cmd(TranslatableString::untranslatable("Articulation precedence tests"));
    Ornament *trill = Factory::create_ornament(chord);
    trill->set_sym_id(SymId::ORNAMENT_TRILL);
    trill->set_track(chord->track());
    trill->set_ownership_parent(chord);
    score->undo_add_element(trill);
    score->end_cmd();

    ArticulationsProfilePtr profile = std::make_shared<ArticulationsProfile>();
    for (const ArticulationType type : {ArticulationType::ARPEGGIO, ArticulationType::TRILL}) {
        profile->set_pattern(type, blank_pattern());
    }
    PlaybackContextPtr playback_ctx = std::make_shared<PlaybackContext>(score);

    // [GIVEN] The chord carries both, and TRILL is lower in enum order
    const ArticulationMap articulations = chord_articulations(chord, profile, playback_ctx);
    ASSERT_TRUE(articulations.contains(ArticulationType::ARPEGGIO));
    ASSERT_TRUE(articulations.contains(ArticulationType::TRILL));
    ASSERT_LT(ArticulationType::TRILL, ArticulationType::ARPEGGIO);

    // [WHEN] Render the chord
    PlaybackEventsMap result;
    PlaybackEventsRenderer renderer;
    renderer.render(chord, 0, profile, playback_ctx, result);

    // [THEN] Three rolled notes, one per pitch, staggered by a third of the arpeggio span
    std::vector<const mpe::NoteEvent *> notes;
    for (const auto &pair : result) {
        for (const PlaybackEvent &event : pair.second) {
            notes.push_back(&std::get<mpe::NoteEvent>(event));
        }
    }
    constexpr duration_t sixteenth = quarter_note_duration / 4;
    const pitch_level_t pitches[] = {pitch_level(PitchClass::F, 4), pitch_level(PitchClass::A, 4), pitch_level(PitchClass::C, 5)};
    ASSERT_EQ(notes.size(), 3);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(notes[i]->pitch_ctx().nominal_pitch_level, pitches[i]);
        EXPECT_EQ(notes[i]->arrangement_ctx().nominal_timestamp, static_cast<timestamp_t>(i) * (sixteenth / 3));
    }

    delete score;
}