// SPDX-License-Identifier: GPL-3.0-only

// Witnesses for the normalized articulation precedence (playback refactor step 1). Every case first
// asserts that its input really contains the conflicting articulations, then checks the selected outcome.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "audio/engine/internal/synthesizers/fluidsynth/fluidsequencer.h"
#include "audio/engine/internal/synthesizers/fluidsynth/soundmapping.h"
#include "engrave/src/runtime/resource_filesystem.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/part.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/playback/metaparsers/chordarticulationsparser.h"
#include "engraving/playback/playbackeventsrenderer.h"
#include "engraving/playback/renderers/bendsrenderer.h"
#include "engraving/playback/renderers/gracechordcontext.h"
#include "engraving/playback/renderingcontext.h"
#include "engraving/playback/utils/expressionutils.h"
#include "mpe/internal/articulationprofilesrepository.h"
#include "mpe/tests/utils/articulationutils.h"
#include "utils/scorerw.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;
using namespace muse::audio::synth;

namespace {
constexpr duration_t quarter_note_duration = 500000; // 4/4 at 120 BPM

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
    sequencer.init(setup, std::nullopt, use_dynamic_events, k_fluid_channel_count);

    PlaybackData data;
    data.setup_data = setup;
    data.origin_events.emplace(event.arrangement_ctx().actual_timestamp, PlaybackEventList{event});
    sequencer.load(freeze_playback_data(data));
}

std::vector<const midi::Event *> midi_events_at(const TimelineSequencer &sequencer, const timestamp_t timestamp,
                                                const midi::Event::Opcode opcode) {
    std::vector<const midi::Event *> result;
    for (const auto &entry : sequencer.main_stream_events_.events) {
        if (entry.timestamp == timestamp && entry.event.opcode() == opcode) {
            result.push_back(&entry.event);
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

void add_grace(Score *score, const Chord *chord, const int pitch, const NoteType type, const int ticks) {
    score->start_cmd(TranslatableString::untranslatable("Articulation precedence tests"));
    score->set_grace_note(const_cast<Chord *>(chord), pitch, type, ticks);
    score->end_cmd();
}

// The common articulations the renderers see for this chord
ArticulationMap chord_articulations(const Chord *chord, const ArticulationsProfilePtr &profile, const PlaybackContextPtr &playback_ctx) {
    RenderingContext ctx = build_rendering_ctx(chord, 0, profile, playback_ctx);
    ChordArticulationsParser::build_chord_articulation_map(chord, ctx, ctx.common_articulations);
    return ctx.common_articulations;
}

std::vector<const mpe::NoteEvent *> note_events(const PlaybackEventsMap &result) {
    std::vector<const mpe::NoteEvent *> notes;
    for (const auto &pair : result) {
        for (const PlaybackEvent &event : pair.second) {
            if (std::holds_alternative<mpe::NoteEvent>(event)) {
                notes.push_back(&std::get<mpe::NoteEvent>(event));
            }
        }
    }
    return notes;
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

// (a) Ordinary selector: a principal with graces before and after renders both groups. Each group takes its
//     own rule's time; together they take at most half the principal, scaled proportionally
TEST(EngravingArticulationPrecedenceTests, GraceBeforeAndAfterOneChord) {
    struct Case {
        const char16_t *score;
        NoteType before_type;
        int before_ticks;
        ArticulationType expected;
        pitch_level_t after_pitch;
        duration_t before, after; // the principal keeps the rest
    };

    // 4/4, 120 BPM, quarter principal, eighth after-grace (its own rule: half = 250 ms):
    // - appoggiatura eighth (half, 250 ms): 250 + 250 > 250, scaled to 125/125, principal 250
    // - acciaccatura (31.25 ms): 31.25 + 250 > 250, scaled to 27.778/222.222, principal 250
    // 6/4 (compound), quarter principal, so both rules allow 2/3 = 333.333 ms:
    // - quarter appoggiatura (333.333) + eighth after (250): scaled to 142.857/107.143, principal 250
    const char16_t *simple = u"playback/playbackeventsrenderer_data/single_note_appoggiatura_post/single_note_appoggiatura_post.mscx";
    const char16_t *compound = u"playback/playbackmodel_data/metronome_6_4_with_repeat/metronome_6_4_with_repeat.mscx";
    const Case cases[] = {
        {simple, NoteType::APPOGGIATURA, Constants::division / 2, ArticulationType::PRE_APPOGGIATURA, pitch_level(PitchClass::G, 4), 125000,
         125000},
        {simple, NoteType::ACCIACCATURA, Constants::division / 2, ArticulationType::ACCIACCATURA, pitch_level(PitchClass::G, 4), 27778,
         222222},
        {compound, NoteType::GRACE4, Constants::division, ArticulationType::PRE_APPOGGIATURA, pitch_level(PitchClass::B, 4), 142857,
         107143},
    };

    for (const Case &c : cases) {
        // F4 quarter; the simple score already has a G4 eighth grace after. Add an A4 grace before (and a B4
        // eighth after in the compound score)
        Score *score = ScoreRW::read_score(c.score);
        ASSERT_TRUE(score);
        const Chord *chord = find_chord(score, 0, 0);
        ASSERT_TRUE(chord);
        ASSERT_EQ(chord->notes().front()->pitch(), 65);
        add_grace(score, chord, 69, c.before_type, c.before_ticks);
        if (chord->grace_notes_after().empty()) {
            add_grace(score, chord, 71, NoteType::GRACE8_AFTER, Constants::division / 2);
        }
        ASSERT_EQ(chord->grace_notes_before().size(), 1);
        ASSERT_EQ(chord->grace_notes_after().size(), 1);

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
        ASSERT_EQ(build_rendering_ctx(chord, 0, profile, playback_ctx).nominal_duration, quarter_note_duration);

        // [WHEN] Render the chord
        PlaybackEventsMap result;
        PlaybackEventsRenderer renderer;
        renderer.render(chord, 0, profile, playback_ctx, result);

        // [THEN] Before grace, delayed principal, after grace; the graces fill exactly half
        std::vector<const mpe::NoteEvent *> notes;
        for (const auto &pair : result) {
            for (const PlaybackEvent &event : pair.second) {
                notes.push_back(&std::get<mpe::NoteEvent>(event));
            }
        }

        ASSERT_EQ(notes.size(), 3);
        EXPECT_EQ(c.before + c.after, quarter_note_duration / 2);
        EXPECT_EQ(notes[0]->pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::A, 4));
        EXPECT_EQ(notes[0]->arrangement_ctx().nominal_timestamp, 0);
        EXPECT_EQ(notes[0]->arrangement_ctx().nominal_duration, c.before);
        EXPECT_EQ(notes[1]->pitch_ctx().nominal_pitch_level, pitch_level(PitchClass::F, 4));
        EXPECT_EQ(notes[1]->arrangement_ctx().nominal_timestamp, c.before);
        EXPECT_EQ(notes[1]->arrangement_ctx().nominal_duration, quarter_note_duration - c.before - c.after);
        EXPECT_EQ(notes[2]->pitch_ctx().nominal_pitch_level, c.after_pitch);
        EXPECT_EQ(notes[2]->arrangement_ctx().nominal_timestamp, quarter_note_duration - c.after);
        EXPECT_EQ(notes[2]->arrangement_ctx().nominal_duration, c.after);

        delete score;
    }
}

// (a) Multibend path: a bent principal with a grace before and bent graces after. The after-graces' bend
//     segments get their scaled durations
TEST(EngravingArticulationPrecedenceTests, GraceBeforeAndAfterInMultibend) {
    Score *score = ScoreRW::read_score(u"playback/playbackeventsrenderer_data/guitar_bends/guitar_bends.mscx");
    ASSERT_TRUE(score);

    // G3 quarter at 2880: an A3 bends into it, and it bends on into its two eighth graces after (A3, G3).
    // Give it an unbent B3 eighth appoggiatura before
    const Chord *principal = find_chord(score, 2880, 0);
    ASSERT_TRUE(principal);
    ASSERT_EQ(principal->notes().front()->pitch(), 55);
    ASSERT_EQ(principal->grace_notes_after().size(), 2);
    ASSERT_TRUE(principal->notes().front()->bend_for());
    ASSERT_TRUE(principal->grace_notes_before().empty());
    add_grace(score, principal, 59, NoteType::APPOGGIATURA, Constants::division / 2);
    ASSERT_EQ(principal->grace_notes_before().size(), 1);

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
    RenderingContext principal_ctx = build_rendering_ctx(principal, 0, profile, playback_ctx);
    ChordArticulationsParser::build_chord_articulation_map(principal, principal_ctx, principal_ctx.common_articulations);
    ASSERT_TRUE(principal_ctx.common_articulations.contains(ArticulationType::PRE_APPOGGIATURA));
    ASSERT_TRUE(principal_ctx.common_articulations.contains(ArticulationType::POST_APPOGGIATURA));
    ASSERT_EQ(select_grace_type(principal_ctx.common_articulations), ArticulationType::PRE_APPOGGIATURA);

    // [THEN] Before 250 and after 250 ms scale to 125 + 125: B3 125 ms, G3 from +125 for 250 ms, the two
    //        bent after-graces 62.5 ms each at the end
    const timestamp_t start = principal_ctx.nominal_timestamp;
    const GraceChordCtx grace_ctx = GraceChordCtx::build_ctx(principal, ArticulationType::PRE_APPOGGIATURA, principal_ctx);
    EXPECT_EQ(grace_ctx.principal_chord_ctx.nominal_timestamp, start + 125000);
    EXPECT_EQ(grace_ctx.principal_chord_ctx.nominal_duration, 250000);
    ASSERT_EQ(grace_ctx.grace_chord_ctx_list.size(), 1);
    EXPECT_EQ(grace_ctx.grace_chord_ctx_list[0].second.nominal_timestamp, start);
    EXPECT_EQ(grace_ctx.grace_chord_ctx_list[0].second.nominal_duration, 125000);
    ASSERT_EQ(grace_ctx.after_grace_chord_ctx_list.size(), 2);
    EXPECT_EQ(grace_ctx.after_grace_chord_ctx_list[0].second.nominal_timestamp, start + 375000);
    EXPECT_EQ(grace_ctx.after_grace_chord_ctx_list[0].second.nominal_duration, 62500);
    EXPECT_EQ(grace_ctx.after_grace_chord_ctx_list[1].second.nominal_timestamp, start + 437500);
    EXPECT_EQ(grace_ctx.after_grace_chord_ctx_list[1].second.nominal_duration, 62500);

    // [WHEN] Render the multibend from its first note
    const Chord *start_chord = find_chord(score, 480, 0);
    ASSERT_TRUE(start_chord);
    PlaybackEventList events;
    BendsRenderer::render(start_chord->notes().front(), build_rendering_ctx(start_chord, 0, profile, playback_ctx), events);

    // [THEN] Relative to EngravingBendsRendererTests.Multibend (G3 at 83%, after-graces at 91% and 95% of the
    //        3 s bend from 0.5 s), G3 moves to 3.125 s (87%) and the after-graces to 3.375 s (95%) and 3.4375 s (97%)
    ASSERT_EQ(events.size(), 1);
    PitchCurve expected_pitch_curve;
    expected_pitch_curve.emplace(0, 0);
    expected_pitch_curve.emplace(1600, 100);
    expected_pitch_curve.emplace(3300, 0);
    expected_pitch_curve.emplace(5000, 100);
    expected_pitch_curve.emplace(5800, 200);
    expected_pitch_curve.emplace(8700, 100); // A3 -> G3
    expected_pitch_curve.emplace(9500, 200); // G3 -> A3 (grace after)
    expected_pitch_curve.emplace(9700, 100); // A3 -> G3 (grace after)
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

// Chord-articulation precedence: a rolled chord stays rolled (arpeggios first, then enum order). The tremolo case is
// the one the legacy selector got wrong: the tremolo is parsed (and was inserted) before the arpeggio, so the
// hash-ordered loop rendered the tremolo. The trill is parsed after the arpeggio
TEST(EngravingArticulationPrecedenceTests, ArpeggioOverOrnamentAndTremolo) {
    for (const bool tremolo : {false, true}) {
        // F4+A4+C5 sixteenth with an arpeggio; add a trill or a 16th single-chord tremolo
        Score *score = ScoreRW::read_score(u"playback/playbackeventsrenderer_data/chord_arpeggio/chord_arpeggio.mscx");
        ASSERT_TRUE(score);
        Chord *chord = const_cast<Chord *>(find_chord(score, 0, 0));
        ASSERT_TRUE(chord);
        ASSERT_TRUE(chord->arpeggio());
        score->start_cmd(TranslatableString::untranslatable("Articulation precedence tests"));
        if (tremolo) {
            TremoloSingleChord *t = Factory::create_tremolo_single_chord(chord);
            t->set_tremolo_type(TremoloType::R16);
            t->set_track(chord->track());
            t->set_ownership_parent(chord);
            score->undo_add_element(t);
        } else {
            Ornament *trill = Factory::create_ornament(chord);
            trill->set_sym_id(SymId::ORNAMENT_TRILL);
            trill->set_track(chord->track());
            trill->set_ownership_parent(chord);
            score->undo_add_element(trill);
        }
        score->end_cmd();
        const ArticulationType other = tremolo ? ArticulationType::TREMOLO16TH : ArticulationType::TRILL;
        ASSERT_EQ(tremolo, chord->tremolo_single_chord() != nullptr);

        ArticulationsProfilePtr profile = std::make_shared<ArticulationsProfile>();
        for (const ArticulationType type : {ArticulationType::ARPEGGIO, ArticulationType::TRILL, ArticulationType::TREMOLO16TH}) {
            profile->set_pattern(type, blank_pattern());
        }
        PlaybackContextPtr playback_ctx = std::make_shared<PlaybackContext>(score);

        // [GIVEN] The chord carries both, and the other type is lower in enum order
        const ArticulationMap articulations = chord_articulations(chord, profile, playback_ctx);
        ASSERT_TRUE(articulations.contains(ArticulationType::ARPEGGIO));
        ASSERT_TRUE(articulations.contains(other));
        ASSERT_LT(other, ArticulationType::ARPEGGIO);

        // [WHEN] Render the chord
        PlaybackEventsMap result;
        PlaybackEventsRenderer renderer;
        renderer.render(chord, 0, profile, playback_ctx, result);

        // [THEN] Three rolled notes, one per pitch, staggered by a third of the arpeggio span
        const std::vector<const mpe::NoteEvent *> notes = note_events(result);
        constexpr duration_t sixteenth = quarter_note_duration / 4;
        const pitch_level_t pitches[] = {pitch_level(PitchClass::F, 4), pitch_level(PitchClass::A, 4), pitch_level(PitchClass::C, 5)};
        ASSERT_EQ(notes.size(), 3) << (tremolo ? "tremolo" : "trill");
        for (size_t i = 0; i < 3; ++i) {
            EXPECT_EQ(notes[i]->pitch_ctx().nominal_pitch_level, pitches[i]);
            EXPECT_EQ(notes[i]->arrangement_ctx().nominal_timestamp, static_cast<timestamp_t>(i) * (sixteenth / 3));
        }

        delete score;
    }
}

// Blocking 1 (Terra): a mixed before-group (appoggiatura + acciaccatura) with after-graces, on the real keyboard
// profile. Each grace plays with its own type only; the principal, which carries none of them, starts after the
// before-graces and ends before the after-graces
TEST(EngravingArticulationPrecedenceTests, MixedBeforeGroupWithDefaultProfile) {
    Score *score =
        ScoreRW::read_score(u"playback/playbackeventsrenderer_data/single_note_appoggiatura_post/single_note_appoggiatura_post.mscx");
    ASSERT_TRUE(score);
    const Chord *chord = find_chord(score, 0, 0);
    ASSERT_TRUE(chord);
    add_grace(score, chord, 69, NoteType::APPOGGIATURA, Constants::division / 2);
    add_grace(score, chord, 71, NoteType::ACCIACCATURA, Constants::division / 2);
    add_grace(score, chord, 64, NoteType::GRACE8_AFTER, Constants::division / 2);
    add_grace(score, chord, 62, NoteType::GRACE8_AFTER, Constants::division / 2);
    ASSERT_EQ(chord->grace_notes_before().size(), 2);
    ASSERT_EQ(chord->grace_notes_after().size(), 3);

    mpe::ArticulationProfilesRepository repository(*muse::io::file_system());
    const ArticulationsProfilePtr profile = repository.default_profile(ArticulationFamily::KEYBOARDS);
    ASSERT_TRUE(profile);
    ASSERT_TRUE(profile->contains(ArticulationType::PRE_APPOGGIATURA));
    ASSERT_TRUE(profile->contains(ArticulationType::POST_APPOGGIATURA));
    const ArrangementPattern &acciaccatura = profile->pattern(ArticulationType::ACCIACCATURA).cbegin()->second.arrangement_pattern;
    ASSERT_EQ(acciaccatura.timestamp_offset_value, -2500);
    ASSERT_EQ(acciaccatura.duration_factor_value, 12000);
    PlaybackContextPtr playback_ctx = std::make_shared<PlaybackContext>(score);

    // [GIVEN] The principal carries all three grace types; the before-group is one PRE budget (2 graces: 2 x 31.25 ms),
    //         the after-group half (250 ms); together 312.5 > 250, scaled to 50 + 200 ms
    const ArticulationMap articulations = chord_articulations(chord, profile, playback_ctx);
    ASSERT_TRUE(articulations.contains(ArticulationType::PRE_APPOGGIATURA));
    ASSERT_TRUE(articulations.contains(ArticulationType::ACCIACCATURA));
    ASSERT_TRUE(articulations.contains(ArticulationType::POST_APPOGGIATURA));
    ASSERT_EQ(select_grace_type(articulations), ArticulationType::PRE_APPOGGIATURA);

    PlaybackEventsMap result;
    PlaybackEventsRenderer renderer;
    renderer.render(chord, 0, profile, playback_ctx, result);
    const std::vector<const mpe::NoteEvent *> notes = note_events(result);
    ASSERT_EQ(notes.size(), 6);

    const mpe::NoteEvent *principal = nullptr;
    std::vector<const mpe::NoteEvent *> before, after;
    for (const mpe::NoteEvent *note : notes) {
        const pitch_level_t pitch = note->pitch_ctx().nominal_pitch_level;
        if (pitch == pitch_level(PitchClass::F, 4)) {
            principal = note;
        } else if (pitch == pitch_level(PitchClass::A, 4) || pitch == pitch_level(PitchClass::B, 4)) {
            before.push_back(note);
        } else {
            after.push_back(note);
        }
    }
    ASSERT_TRUE(principal);
    ASSERT_EQ(before.size(), 2);
    ASSERT_EQ(after.size(), 3);

    // [THEN] The principal plays from its nominal start, 50 ms, and none of the grace types moves it
    const mpe::ArrangementContext &p = principal->arrangement_ctx();
    EXPECT_EQ(p.nominal_timestamp, 50000);
    EXPECT_EQ(p.nominal_duration, 250000);
    EXPECT_EQ(p.actual_timestamp, p.nominal_timestamp);
    for (const ArticulationType type :
         {ArticulationType::PRE_APPOGGIATURA, ArticulationType::ACCIACCATURA, ArticulationType::POST_APPOGGIATURA}) {
        EXPECT_FALSE(principal->expression_ctx().articulations.contains(type));
    }

    // [THEN] Each grace carries only its own type; every before-grace has sounded and ended before the principal,
    //        and every after-grace starts after the principal has ended, within the note
    for (const mpe::NoteEvent *note : before) {
        const bool is_acciaccatura = note->pitch_ctx().nominal_pitch_level == pitch_level(PitchClass::B, 4);
        const ArticulationType own = is_acciaccatura ? ArticulationType::ACCIACCATURA : ArticulationType::PRE_APPOGGIATURA;
        EXPECT_TRUE(note->expression_ctx().articulations.contains(own));
        EXPECT_FALSE(note->expression_ctx().articulations.contains(is_acciaccatura ? ArticulationType::PRE_APPOGGIATURA :
                                                                                     ArticulationType::ACCIACCATURA));
        EXPECT_FALSE(note->expression_ctx().articulations.contains(ArticulationType::POST_APPOGGIATURA));
        EXPECT_LT(note->arrangement_ctx().actual_timestamp, p.actual_timestamp);
        EXPECT_LE(note->arrangement_ctx().actual_timestamp + note->arrangement_ctx().actual_duration, p.actual_timestamp);
    }
    for (const mpe::NoteEvent *note : after) {
        EXPECT_TRUE(note->expression_ctx().articulations.contains(ArticulationType::POST_APPOGGIATURA));
        EXPECT_FALSE(note->expression_ctx().articulations.contains(ArticulationType::PRE_APPOGGIATURA));
        EXPECT_FALSE(note->expression_ctx().articulations.contains(ArticulationType::ACCIACCATURA));
        EXPECT_GE(note->arrangement_ctx().actual_timestamp, p.actual_timestamp + p.actual_duration);
        EXPECT_LE(note->arrangement_ctx().actual_timestamp + note->arrangement_ctx().actual_duration, quarter_note_duration);
    }

    delete score;
}

// Should-fix 3 (Terra): the rounded grace durations of each group fill its budget exactly, with no zero member, and
// the last after-grace ends exactly where the next note starts
TEST(EngravingArticulationPrecedenceTests, GraceGroupBudgetRounding) {
    struct Case {
        NoteType before_type;
        std::vector<int> after_ticks; // added after the existing G4 eighth
        duration_t before, after;
    };

    // equal split: 2 x 31.25 + 250 > 250 -> 50 + 200 ms over three eighths (66,667 + 66,666 + 66,667)
    // uneven split with a 32nd: acciaccatura 31.25 + 250 > 250 -> 27,778 + 222,222 over 8th/16th/32nd
    const Case cases[] = {
        {NoteType::APPOGGIATURA, {Constants::division / 2, Constants::division / 2}, 50000, 200000},
        {NoteType::ACCIACCATURA, {Constants::division / 4, Constants::division / 8}, 27778, 222222},
    };

    for (const Case &c : cases) {
        Score *score =
            ScoreRW::read_score(u"playback/playbackeventsrenderer_data/single_note_appoggiatura_post/single_note_appoggiatura_post.mscx");
        ASSERT_TRUE(score);
        const Chord *chord = find_chord(score, 0, 0);
        ASSERT_TRUE(chord);
        add_grace(score, chord, 69, c.before_type, Constants::division / 2);
        if (c.before_type == NoteType::APPOGGIATURA) {
            add_grace(score, chord, 71, NoteType::ACCIACCATURA, Constants::division / 2);
        }
        for (const int ticks : c.after_ticks) {
            add_grace(score, chord, 64,
                      ticks == Constants::division / 2 ? NoteType::GRACE8_AFTER :
                      ticks == Constants::division / 4 ? NoteType::GRACE16_AFTER :
                                                         NoteType::GRACE32_AFTER,
                      ticks);
        }
        ASSERT_EQ(chord->grace_notes_after().size(), 1 + c.after_ticks.size());

        ArticulationsProfilePtr profile = std::make_shared<ArticulationsProfile>();
        for (const ArticulationType type :
             {ArticulationType::PRE_APPOGGIATURA, ArticulationType::ACCIACCATURA, ArticulationType::POST_APPOGGIATURA}) {
            profile->set_pattern(type, blank_pattern());
        }
        PlaybackContextPtr playback_ctx = std::make_shared<PlaybackContext>(score);
        RenderingContext ctx = build_rendering_ctx(chord, 0, profile, playback_ctx);
        ChordArticulationsParser::build_chord_articulation_map(chord, ctx, ctx.common_articulations);
        const GraceChordCtx grace_ctx = GraceChordCtx::build_ctx(chord, select_grace_type(ctx.common_articulations), ctx);

        duration_t before_sum = 0;
        timestamp_t expected_start = 0;
        for (const auto &pair : grace_ctx.grace_chord_ctx_list) {
            EXPECT_EQ(pair.second.nominal_timestamp, expected_start);
            EXPECT_GT(pair.second.nominal_duration, 0);
            expected_start += pair.second.nominal_duration;
            before_sum += pair.second.nominal_duration;
        }
        EXPECT_EQ(before_sum, c.before);
        EXPECT_EQ(grace_ctx.principal_chord_ctx.nominal_timestamp, c.before);

        duration_t after_sum = 0;
        expected_start = quarter_note_duration - c.after;
        ASSERT_EQ(grace_ctx.after_grace_chord_ctx_list.size(), 1 + c.after_ticks.size());
        for (const auto &pair : grace_ctx.after_grace_chord_ctx_list) {
            EXPECT_EQ(pair.second.nominal_timestamp, expected_start);
            EXPECT_GT(pair.second.nominal_duration, 0);
            expected_start += pair.second.nominal_duration;
            after_sum += pair.second.nominal_duration;
        }
        EXPECT_EQ(after_sum, c.after);
        EXPECT_EQ(expected_start, quarter_note_duration); // the boundary with the next note
        EXPECT_EQ(grace_ctx.principal_chord_ctx.nominal_timestamp + grace_ctx.principal_chord_ctx.nominal_duration,
                  quarter_note_duration - c.after);

        delete score;
    }
}

// Blocking 2 (Terra): a multibend that starts on a grace bend must not pass that grace's PRE marker on to later
// chords of the chain. 6/4 (compound), quarters at 120 BPM: E4 grace bends into F4, which bends into G4
TEST(EngravingArticulationPrecedenceTests, MultibendInheritedGraceMarker) {
    for (const bool after_only : {true, false}) {
        Score *score = ScoreRW::read_score(u"playback/playbackmodel_data/metronome_6_4_with_repeat/metronome_6_4_with_repeat.mscx");
        ASSERT_TRUE(score);
        Chord *first = const_cast<Chord *>(find_chord(score, 0, 0));
        Chord *second = const_cast<Chord *>(find_chord(score, Constants::division, 0));
        ASSERT_TRUE(first && second);
        Note *second_note = second->notes().front();
        second_note->set_pitch(67);
        second_note->set_tpc_from_pitch();

        score->start_cmd(TranslatableString::untranslatable("Articulation precedence tests"));
        ASSERT_TRUE(score->add_guitar_bend(GuitarBendType::GRACE_NOTE_BEND, first->notes().front(), nullptr));
        score->end_cmd();
        score->start_cmd(TranslatableString::untranslatable("Articulation precedence tests"));
        ASSERT_TRUE(score->add_guitar_bend(GuitarBendType::BEND, first->notes().front(), second_note));
        score->end_cmd();

        // the later principal has only after-graces (2 eighths), or only an acciaccatura before
        if (after_only) {
            add_grace(score, second, 69, NoteType::GRACE8_AFTER, Constants::division / 2);
            add_grace(score, second, 67, NoteType::GRACE8_AFTER, Constants::division / 2);
        } else {
            add_grace(score, second, 69, NoteType::ACCIACCATURA, Constants::division / 2);
        }

        ArticulationsProfilePtr profile = std::make_shared<ArticulationsProfile>();
        const ArticulationPattern zero_pattern = pattern_with(PitchPattern(ArticulationMap::expected_size, ten_percent, 0),
                                                              ExpressionPattern(ArticulationMap::expected_size, ten_percent, 0));
        for (const ArticulationType type : {ArticulationType::STANDARD, ArticulationType::MULTIBEND, ArticulationType::PRE_APPOGGIATURA,
                                            ArticulationType::ACCIACCATURA, ArticulationType::POST_APPOGGIATURA}) {
            profile->set_pattern(type, zero_pattern);
        }
        PlaybackContextPtr playback_ctx = std::make_shared<PlaybackContext>(score);
        const Part *part = score->parts().front();
        playback_ctx->update(part->track_range().start_track, part->track_range().end_track, 0, score->end_tick().ticks());

        // [GIVEN] The chain starts on a grace bend (PRE on its principal); the later principal has no PRE of its own
        ASSERT_EQ(first->grace_notes_before().size(), 1);
        ASSERT_TRUE(first->grace_notes_before().front()->notes().front()->is_grace_bend_start());
        ASSERT_TRUE(chord_articulations(first, profile, playback_ctx).contains(ArticulationType::PRE_APPOGGIATURA));
        const ArticulationMap later = chord_articulations(second, profile, playback_ctx);
        ASSERT_FALSE(later.contains(ArticulationType::PRE_APPOGGIATURA));
        ASSERT_EQ(later.contains(ArticulationType::POST_APPOGGIATURA), after_only);
        ASSERT_EQ(later.contains(ArticulationType::ACCIACCATURA), !after_only);
        ASSERT_TRUE(build_rendering_ctx(second, 0, profile, playback_ctx).time_signature_fraction.is_compound());

        // [WHEN] Render the first chord, which renders the whole multibend from its grace
        PlaybackEventsMap result;
        PlaybackEventsRenderer renderer;
        renderer.render(first, 0, profile, playback_ctx, result);
        const mpe::NoteEvent *bend = nullptr;
        for (const mpe::NoteEvent *note : note_events(result)) {
            if (note->expression_ctx().articulations.contains(ArticulationType::MULTIBEND)) {
                bend = note;
            }
        }
        ASSERT_TRUE(bend);
        const mpe::ArrangementContext &arrangement = bend->arrangement_ctx();

        if (after_only) {
            // [THEN] The after-graces keep the compound after-only rule, 2/3 of the quarter (333,333 us, not half),
            //        so G4 ends at 1,000,000 - 333,333
            EXPECT_EQ(arrangement.actual_timestamp + arrangement.actual_duration, 666667);
        } else {
            // [THEN] G4 is delayed by its own acciaccatura (31.25 ms), not by an appoggiatura rule: it arrives at
            //        531,250 us of the 0..1,000,000 bend (53%)
            EXPECT_EQ(arrangement.actual_timestamp + arrangement.actual_duration, 1000000);
            const PitchCurve &curve = bend->pitch_ctx().pitch_curve;
            ASSERT_TRUE(curve.contains(5300));
            EXPECT_EQ(curve.at(5300), 150);
        }

        delete score;
    }
}

// One-group exception (owner): a before-group that mixes an appoggiatura and an acciaccatura, with no after-graces.
// The principal must not keep the acciaccatura marker (keyboard profile: -25% offset, 120% duration), which made it
// sound before its graces
TEST(EngravingArticulationPrecedenceTests, MixedBeforeGroupWithoutAfterGraces) {
    // F4 quarter with a G4 eighth acciaccatura; add an A4 eighth appoggiatura
    Score *score = ScoreRW::read_score(u"playback/playbackeventsrenderer_data/single_note_acciaccatura/single_note_acciaccatura.mscx");
    ASSERT_TRUE(score);
    const Chord *chord = find_chord(score, 0, 0);
    ASSERT_TRUE(chord);
    ASSERT_EQ(chord->grace_notes_before().size(), 1);
    add_grace(score, chord, 69, NoteType::APPOGGIATURA, Constants::division / 2);
    ASSERT_EQ(chord->grace_notes_before().size(), 2);
    ASSERT_TRUE(chord->grace_notes_after().empty());

    mpe::ArticulationProfilesRepository repository(*muse::io::file_system());
    const ArticulationsProfilePtr profile = repository.default_profile(ArticulationFamily::KEYBOARDS);
    ASSERT_TRUE(profile);
    ASSERT_EQ(profile->pattern(ArticulationType::ACCIACCATURA).cbegin()->second.arrangement_pattern.timestamp_offset_value, -2500);
    PlaybackContextPtr playback_ctx = std::make_shared<PlaybackContext>(score);

    // [GIVEN] Both before types and no after type; one PRE budget for two graces: 2 x 31.25 ms
    const ArticulationMap articulations = chord_articulations(chord, profile, playback_ctx);
    ASSERT_TRUE(articulations.contains(ArticulationType::PRE_APPOGGIATURA));
    ASSERT_TRUE(articulations.contains(ArticulationType::ACCIACCATURA));
    ASSERT_FALSE(articulations.contains(ArticulationType::POST_APPOGGIATURA));
    ASSERT_EQ(select_grace_type(articulations), ArticulationType::PRE_APPOGGIATURA);

    PlaybackEventsMap result;
    PlaybackEventsRenderer renderer;
    renderer.render(chord, 0, profile, playback_ctx, result);
    const std::vector<const mpe::NoteEvent *> notes = note_events(result);
    ASSERT_EQ(notes.size(), 3);

    const mpe::NoteEvent *principal = nullptr;
    std::vector<const mpe::NoteEvent *> graces;
    for (const mpe::NoteEvent *note : notes) {
        if (note->pitch_ctx().nominal_pitch_level == pitch_level(PitchClass::F, 4)) {
            principal = note;
        } else {
            graces.push_back(note);
        }
    }
    ASSERT_TRUE(principal);
    ASSERT_EQ(graces.size(), 2);

    // [THEN] The principal plays from its nominal start, 62.5 ms, without a grace type
    const mpe::ArrangementContext &p = principal->arrangement_ctx();
    EXPECT_EQ(p.nominal_timestamp, 62500);
    EXPECT_EQ(p.nominal_duration, quarter_note_duration - 62500);
    EXPECT_EQ(p.actual_timestamp, p.nominal_timestamp);
    EXPECT_FALSE(principal->expression_ctx().articulations.contains(ArticulationType::PRE_APPOGGIATURA));
    EXPECT_FALSE(principal->expression_ctx().articulations.contains(ArticulationType::ACCIACCATURA));

    // [THEN] Both graces carry only their own type, fill exactly the budget and have ended before the principal
    duration_t nominal_sum = 0;
    for (const mpe::NoteEvent *note : graces) {
        const bool is_acciaccatura = note->pitch_ctx().nominal_pitch_level == pitch_level(PitchClass::G, 4);
        EXPECT_TRUE(note->expression_ctx().articulations.contains(is_acciaccatura ? ArticulationType::ACCIACCATURA :
                                                                                    ArticulationType::PRE_APPOGGIATURA));
        EXPECT_FALSE(note->expression_ctx().articulations.contains(is_acciaccatura ? ArticulationType::PRE_APPOGGIATURA :
                                                                                     ArticulationType::ACCIACCATURA));
        EXPECT_LE(note->arrangement_ctx().actual_timestamp + note->arrangement_ctx().actual_duration, p.actual_timestamp);
        nominal_sum += note->arrangement_ctx().nominal_duration;
    }
    EXPECT_EQ(nominal_sum, 62500);

    delete score;
}