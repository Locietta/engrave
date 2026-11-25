// SPDX-License-Identifier: GPL-3.0-only
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "audio/engine/internal/synthesizers/fluidsynth/fluidsequencer.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/playback/flatplaybackscore.h"
#include "engraving/playback/playbackeventsrenderer.h"
#include "engraving/playback/playbackwalk.h"
#include "global/hashmap.h"
#include "mpe/tests/utils/articulationutils.h"
#include "playback_projection_flat.h"
#include "utils/scorerw.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

namespace {
ArticulationAppliedData applied(const ArticulationType type, const timestamp_t timestamp, const duration_t duration) {
    return ArticulationAppliedData(ArticulationMeta(type, {}, timestamp, duration), 0, hundred_percent);
}

void emit_fixture_note(TrackEvents &track, const timestamp_t at, const pitch_level_t pitch, const bool with_override,
                       FreezeProgress &progress) {
    ArticulationPatternSegment staccato;
    staccato.arrangement_pattern = ArrangementPattern(8000, 1);
    staccato.expression_pattern = ExpressionPattern(ArticulationMap::expected_size, ten_percent, 7000);
    staccato.expression_pattern.dynamic_offset_map.at(hundred_percent) = 6500;
    ArticulationPatternSegment tenuto;
    tenuto.arrangement_pattern = ArrangementPattern(hundred_percent, 0);

    ArticulationMap articulations;
    // Insert out of order so the test exercises the ascending articulation order stored on the note.
    articulations.emplace(
        ArticulationType::TENUTO,
        ArticulationAppliedData(
            {ArticulationType::TENUTO, std::make_shared<const ArticulationPattern>(ArticulationPattern{{0, tenuto}}), at, 400000}, 0,
            hundred_percent));
    articulations.emplace(
        ArticulationType::STACCATO,
        ArticulationAppliedData(
            {ArticulationType::STACCATO, std::make_shared<const ArticulationPattern>(ArticulationPattern{{0, staccato}}), at + 1, 300000},
            0, hundred_percent));
    articulations.pre_calculate_average_data();
    constexpr CurvePoint curve[]{{0, 0}, {5000, -1200}};
    emit_note(track, at,
              {at, 500000, 2, 3, pitch, dynamic_level_from_type(mpe::DynamicType::NATURAL), 2.125, false, with_override ? 0.375f : 0.f},
              articulations, curve, progress);
}

TextArticulationEvent make_text(const char16_t *text, const layer_idx_t layer) {
    TextArticulationEvent event;
    event.text = String(text);
    event.layer_idx = layer;
    event.flags.set_flag(TextArticulationEvent::STARTS_AT_PLAYBACK_POSITION);
    return event;
}

SoundPresetChangeEvent make_preset(const char16_t *code, const layer_idx_t layer) {
    SoundPresetChangeEvent event;
    event.code = String(code);
    event.layer_idx = layer;
    return event;
}

SyllableEvent make_syllable(const char16_t *text, const layer_idx_t layer) {
    SyllableEvent event;
    event.text = String(text);
    event.layer_idx = layer;
    event.flags.set_flag(SyllableEvent::HYPHENED_TO_NEXT);
    return event;
}

DynamicAutomationLayers make_dynamics() {
    DynamicAutomationMap curve;
    mpe::AutomationPoint first;
    first.out_value = 0.25;
    curve.emplace(0, first);
    mpe::AutomationPoint second;
    second.in_value = mpe::AutomationPoint::ExplicitArrival{0.75, mpe::AutomationPoint::Ease{0.25, 0.625}};
    second.out_value = 0.5;
    curve.emplace(1000000, second);

    DynamicAutomationLayers layers;
    const auto shared_curve = std::make_shared<const DynamicAutomationMap>(std::move(curve));
    layers.emplace(4, shared_curve);
    layers.emplace(1, shared_curve);
    return layers;
}

struct FlatFixture {
    FreezeProgress progress;
    PlaybackScore flat;
    InstrumentTrackId id{ID(7), String(u"violin")};

    template <class Event>
    void emit_context(const timestamp_t key, const std::vector<Event> &events) {
        freeze_cold_events(flat.track(id), key, std::span{events}, progress);
    }

    void build() {
        auto &track = flat.track(id);
        track.setup = PlaybackSetupData(String(u"violin"), SoundCategory::STRINGS, StringList{String(u"orchestral")});
        track.setup.score_id = "score-1";
        freeze_dynamics(track, make_dynamics(), progress);

        // Context events first, as apply_context_to_track_data inserts them before the walk
        emit_context(0, std::vector<SoundPresetChangeEvent>{make_preset(u"Engrave.Preset.Δ", 3)});
        emit_context(0, std::vector<TextArticulationEvent>{make_text(u"mute", 3)});
        emit_context(0, std::vector<SyllableEvent>{make_syllable(u"la", 3)});
        emit_context(250000, std::vector<SoundPresetChangeEvent>{});

        emit_fixture_note(track, 0, 6500, false, progress);
        emit_fixture_note(track, 0, 6900, true, progress);
        emit_empty_group(track, 125000, progress);
        emit_fixture_note(track, 500000, 6600, false, progress);
        emit_fixture_note(track, 0, 7300, false, progress);
    }

    static std::string flat_projection(const PlaybackScore &score) {
        music::test::FlatProjection projection;
        projection.score(score);
        return projection.str();
    }
};
} // namespace

TEST(FlatPlaybackStoreTests, FreezeDynamicsPreservesAutomationArrivalAndClearsOldLayers) {
    TrackEvents track;
    FreezeProgress progress;
    freeze_dynamics(track, make_dynamics(), progress);
    ASSERT_EQ(track.dynamic_layers.size(), 2u);
    const auto points = track.layer_points(track.dynamic_layers.back());
    ASSERT_EQ(points.size(), 2u);
    EXPECT_EQ(points[1].at, 1000000);
    const auto *arrival = std::get_if<mpe::AutomationPoint::ExplicitArrival>(&points[1].point.in_value);
    ASSERT_NE(arrival, nullptr);
    EXPECT_EQ(arrival->value.raw(), 0.75);
    EXPECT_EQ(arrival->ease.t.raw(), 0.25);
    EXPECT_EQ(arrival->ease.value.raw(), 0.625);
    EXPECT_EQ(points[1].point.out_value.raw(), 0.5);

    freeze_dynamics(track, {}, progress);
    EXPECT_TRUE(track.dynamic_layers.empty());
    EXPECT_TRUE(track.dynamic_points.empty());
}

TEST(FlatPlaybackStoreTests, EmissionPreservesOrderAndFields) {
    FlatFixture fixture;
    fixture.build();
    const TrackEvents &track = fixture.flat.tracks.front().second;

    ASSERT_EQ(track.notes.size(), 4u);
    ASSERT_EQ(track.cold.size(), 3u);
    ASSERT_EQ(track.empty_groups.size(), 2u);

    // One emission ordinal across cold events, empty groups and notes
    EXPECT_EQ(track.cold[0].seq, 0u);
    EXPECT_EQ(track.cold[2].seq, 2u);
    EXPECT_EQ(track.empty_groups[0].group_timestamp, 250000);
    EXPECT_EQ(track.empty_groups[0].seq, 3u);
    EXPECT_EQ(track.notes[0].seq, 4u);
    EXPECT_EQ(track.empty_groups[1].group_timestamp, 125000);
    EXPECT_EQ(track.notes[3].group_timestamp, 0);
    EXPECT_EQ(track.notes[3].seq, 8u);

    const NoteRecord &note = track.notes[1];
    EXPECT_EQ(note.nominal_timestamp, 0);
    EXPECT_EQ(note.actual_timestamp, 50);
    EXPECT_EQ(note.nominal_duration, 500000);
    EXPECT_EQ(note.actual_duration, 450000);
    EXPECT_EQ(note.bps, 2.125);
    EXPECT_EQ(note.nominal_pitch_level, 6900);
    EXPECT_EQ(note.staff_layer, 3u);
    EXPECT_EQ(note.voice_layer, 2u);
    ASSERT_TRUE(note.velocity_override.has_value());
    EXPECT_EQ(*note.velocity_override, 0.375f);
    EXPECT_FALSE(track.notes[0].velocity_override.has_value());

    const auto articulations = track.note_articulations(note);
    ASSERT_EQ(articulations.size(), 2u);
    EXPECT_EQ(articulations[0].type, ArticulationType::STACCATO);
    EXPECT_EQ(articulations[0].timestamp, 1);
    EXPECT_EQ(articulations[0].overall_duration, 300000);
    EXPECT_EQ(articulations[1].type, ArticulationType::TENUTO);

    const auto pitch = track.pitch_curve(note);
    ASSERT_EQ(pitch.size(), 2u);
    EXPECT_EQ(pitch[1].at, 5000);
    EXPECT_EQ(pitch[1].value, -1200);
    const auto expression = track.expression_curve(note);
    ASSERT_EQ(expression.size(), 11u);
    EXPECT_EQ(expression.front().value, 7000);
    EXPECT_EQ(expression.back().at, hundred_percent);
    EXPECT_EQ(expression.back().value, 6500);

    const auto &preset = std::get<SoundPresetChangeEvent>(track.cold[0].event);
    EXPECT_EQ(preset.code, String(u"Engrave.Preset.Δ"));
    EXPECT_EQ(preset.layer_idx, 3u);
    EXPECT_TRUE(std::holds_alternative<SyllableEvent>(track.cold[2].event));

    ASSERT_EQ(track.dynamic_layers.size(), 2u);
    EXPECT_EQ(track.dynamic_layers[0].layer, 1u);
    EXPECT_EQ(track.layer_points(track.dynamic_layers[1]).size(), 2u);
    EXPECT_EQ(track.setup.id, String(u"violin"));

    // Dynamics are assigned, not appended
    freeze_dynamics(fixture.flat.track(fixture.id), make_dynamics(), fixture.progress);
    EXPECT_EQ(track.dynamic_points.size(), 4u);
}

TEST(FlatPlaybackStoreTests, EmissionRejectsMismatchedArticulationKey) {
    TrackEvents track;
    FreezeProgress progress;
    ArticulationMap articulations;
    articulations.emplace(ArticulationType::ACCENT, applied(ArticulationType::MARCATO, 0, 1));
    EXPECT_THROW(emit_note(track, 0, {}, articulations, {}, progress), std::logic_error);
    EXPECT_TRUE(track.notes.empty());
}

TEST(FlatPlaybackStoreTests, CurvePointsRetainSignedValuesAndAmplitude) {
    static_assert(std::is_same_v<decltype(CurvePoint::value), std::int32_t>);
    static_assert(std::is_same_v<decltype(CurvePoint::at), duration_percentage_t>);

    constexpr auto min = std::numeric_limits<std::int32_t>::min();
    constexpr auto max = std::numeric_limits<std::int32_t>::max();
    const std::vector<std::vector<CurvePoint>> cases{{{0, 0}, {500, 500}, {1000, -340}}, {{0, max}, {500, -5}}, {{0, 7}, {500, min}}};
    constexpr CurvePoint peaks[]{{500, 500}, {0, max}, {500, min}};

    for (std::size_t i = 0; i < cases.size(); ++i) {
        ArticulationPatternSegment segment;
        segment.arrangement_pattern.duration_factor_value = hundred_percent;
        for (const auto &[at, value] : cases[i]) {
            segment.expression_pattern.dynamic_offset_map.emplace(at, value);
        }
        auto pattern = std::make_shared<const ArticulationPattern>(ArticulationPattern{{0, segment}});
        ArticulationMap articulations;
        articulations.emplace(ArticulationType::STANDARD,
                              ArticulationAppliedData({ArticulationType::STANDARD, pattern, 0, 1000}, 0, hundred_percent));
        articulations.pre_calculate_average_data();

        TrackEvents track;
        FreezeProgress progress;
        emit_note(track, 0, {0, 1000, 0, 0, 6500, dynamic_level_from_type(mpe::DynamicType::NATURAL)}, articulations, cases[i], progress);
        ASSERT_EQ(track.notes.size(), 1u);
        const auto pitch = track.pitch_curve(track.notes.front());
        const auto expression = track.expression_curve(track.notes.front());
        ASSERT_EQ(pitch.size(), cases[i].size());
        ASSERT_EQ(expression.size(), cases[i].size());
        for (std::size_t j = 0; j < cases[i].size(); ++j) {
            EXPECT_EQ(pitch[j].at, cases[i][j].at);
            EXPECT_EQ(pitch[j].value, cases[i][j].value);
            EXPECT_EQ(expression[j].at, cases[i][j].at);
            EXPECT_EQ(expression[j].value, cases[i][j].value);
        }
        EXPECT_EQ(max_amplitude_level(pitch), peaks[i].value);
        EXPECT_EQ(amplitude_value_point(expression), std::make_pair(peaks[i].at, peaks[i].value));
        if (i == 0) {
            EXPECT_FLOAT_EQ(velocity_fraction(expression), 0.5f);
        }
    }
}

// Design 4.4: the freeze loops poll every FreezeProgress::k_stride records, across calls
TEST(FlatPlaybackStoreTests, FreezeLoopsPollWithinTheStride) {
    constexpr std::uint32_t stride = FreezeProgress::k_stride;

    struct Cancelled {};

    {
        // One large context list: a cancellation is observed after exactly one stride
        std::vector<TextArticulationEvent> texts(stride * 2 + 10, make_text(u"t", 1));
        TrackEvents track;
        auto cancel = [] { throw Cancelled{}; };
        FreezeProgress progress{.checkpoint = cancel};
        EXPECT_THROW(freeze_cold_events(track, 0, std::span{texts}, progress), Cancelled);
        EXPECT_EQ(track.cold.size(), stride);
    }
    {
        // Two articulations, two pitch points, eleven expression points and the note count toward the same stride.
        TrackEvents track;
        std::uint32_t polls = 0;
        auto bump = [&polls] { ++polls; };
        FreezeProgress progress{.checkpoint = bump};
        for (std::uint32_t i = 0; i < stride + 1; ++i) {
            emit_fixture_note(track, 0, 6500, false, progress);
        }
        EXPECT_EQ(polls, 16u);
        EXPECT_EQ(progress.pending, 16u);
    }
    {
        // One layer with many automation points
        DynamicAutomationMap curve;
        for (std::uint32_t i = 0; i < stride + 1; ++i) curve.emplace(i, mpe::AutomationPoint{});
        DynamicAutomationLayers layers;
        layers.emplace(0, std::make_shared<const DynamicAutomationMap>(std::move(curve)));
        TrackEvents track;
        auto cancel = [] { throw Cancelled{}; };
        FreezeProgress progress{.checkpoint = cancel};
        EXPECT_THROW(freeze_dynamics(track, layers, progress), Cancelled);
        EXPECT_EQ(track.dynamic_points.size(), stride);
    }
    {
        // The stride continues across calls: small lists still reach a poll
        TrackEvents track;
        std::uint32_t polls = 0;
        auto bump = [&polls] { ++polls; };
        FreezeProgress progress{.checkpoint = bump};
        const std::vector<SyllableEvent> one{make_syllable(u"a", 0)};
        for (std::uint32_t i = 0; i < stride * 3; ++i) freeze_cold_events(track, i, std::span{one}, progress);
        EXPECT_EQ(polls, 3u);
    }
}

TEST(FlatPlaybackStoreTests, RecordSpansAreChecked) {
    FlatFixture fixture;
    fixture.build();
    TrackEvents &track = fixture.flat.track(fixture.id);
    NoteRecord note = track.notes[0];
    note.pitch_curve.count = static_cast<std::uint32_t>(track.curve_points.size()) + 1;
    EXPECT_THROW(std::ignore = track.pitch_curve(note), StoreLimit);
    note.pitch_curve = {std::numeric_limits<std::uint32_t>::max(), 1};
    EXPECT_THROW(std::ignore = track.pitch_curve(note), StoreLimit);
}

TEST(FlatPlaybackStoreTests, ProjectionPreservesGroupOrderAndFloatBits) {
    FlatFixture fixture;
    fixture.build();
    const auto projection = FlatFixture::flat_projection(fixture.flat);
    EXPECT_NE(projection.find("group 0 6\npreset 3 16:"), std::string::npos);
    EXPECT_NE(projection.find("group 125000 0\ngroup 250000 0\ngroup 500000 1\n"), std::string::npos);
    EXPECT_NE(projection.find("note 0 50 500000 450000 4001000000000000 6900 "), std::string::npos);
}

TEST(FlatPlaybackStoreTests, ProjectionDetectsCorruptedRecords) {
    FlatFixture fixture;
    fixture.build();
    const auto baseline = FlatFixture::flat_projection(fixture.flat);

    const std::vector<std::pair<const char *, void (*)(TrackEvents &)>> corruptions{
        {"note duration", [](TrackEvents &t) { t.notes[0].actual_duration += 1; }},
        {"note bps", [](TrackEvents &t) { t.notes[2].bps = 2.0; }},
        {"velocity", [](TrackEvents &t) { t.notes[1].velocity_override.reset(); }},
        {"layer", [](TrackEvents &t) { t.notes[0].voice_layer = 0; }},
        {"articulation", [](TrackEvents &t) { t.articulations[0].type = ArticulationType::ACCENT; }},
        {"curve value", [](TrackEvents &t) { t.curve_points[1].value += 1; }},
        {"curve span", [](TrackEvents &t) { t.notes[0].pitch_curve.count -= 1; }},
        {"group", [](TrackEvents &t) { t.notes[3].group_timestamp = 1; }},
        {"order", [](TrackEvents &t) { std::swap(t.notes[0].seq, t.notes[1].seq); }},
        {"cold text", [](TrackEvents &t) { std::get<TextArticulationEvent>(t.cold[1].event).text = String(u"mutf"); }},
        {"cold layer", [](TrackEvents &t) { std::get<SoundPresetChangeEvent>(t.cold[0].event).layer_idx = 4; }},
        {"empty group", [](TrackEvents &t) { t.empty_groups.pop_back(); }},
        {"dynamics", [](TrackEvents &t) { t.dynamic_points[1].point.out_value = 0.875; }},
        {"setup", [](TrackEvents &t) { t.setup.score_id.reset(); }},
    };
    for (const auto &[label, corrupt] : corruptions) {
        PlaybackScore copy = fixture.flat;
        corrupt(copy.tracks.front().second);
        EXPECT_NE(FlatFixture::flat_projection(copy), baseline) << label;
    }
}

// Step 4c: the sequencer lowers the records in (group, seq) order, whatever order the groups were emitted in
TEST(FlatPlaybackStoreTests, SequencerLowersRecordsInGroupOrder) {
    struct TimelineSequencer : muse::audio::synth::FluidSequencer {
        using FluidSequencer::main_stream_events_;
    };

    const auto lower = [](const TrackEvents &events) {
        TimelineSequencer sequencer;
        sequencer.init(events.setup, std::nullopt, false, muse::audio::synth::k_fluid_channel_count);
        sequencer.load(events);
        std::vector<std::pair<timestamp_t, std::uint32_t>> timeline;
        for (const auto &entry : sequencer.main_stream_events_.events) timeline.emplace_back(entry.timestamp, entry.event.note());
        return timeline;
    };
    for (const std::uint32_t count : {1u, 6000u}) {
        TrackEvents emitted;
        emitted.setup = PlaybackSetupData(String(u"piano"), SoundCategory::KEYBOARDS, StringList{});
        FreezeProgress progress;
        // Emit higher group keys first. Every note sounds at the same timestamp.
        for (std::uint32_t i = 0; i < count; ++i) {
            const timestamp_t group = static_cast<timestamp_t>(count - i) * 10;
            const pitch_level_t pitch = pitch_level(PitchClass::C, 4) + static_cast<pitch_level_t>(i % 24) * pitch_level_step;
            const auto level = dynamic_level_from_type(mpe::DynamicType::NATURAL);
            emit_note(emitted, group, {100, 1000, 0, 0, pitch, level}, {}, {}, progress);
            emit_note(emitted, group, {100, 1000, 0, 0, pitch + pitch_level_step, level}, {}, {}, progress);
        }
        const auto actual = lower(emitted);
        ASSERT_EQ(actual.size(), 4u * count);
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint32_t first = 60 + (count - 1 - i) % 24;
            EXPECT_EQ(actual[2 * i], std::make_pair(timestamp_t{100}, first));
            EXPECT_EQ(actual[2 * i + 1], std::make_pair(timestamp_t{100}, first + 1));
            EXPECT_EQ(actual[2 * (count + i)], std::make_pair(timestamp_t{1100}, first));
            EXPECT_EQ(actual[2 * (count + i) + 1], std::make_pair(timestamp_t{1100}, first + 1));
        }
    }
}

struct PlaybackScoreBuilderTests : ::testing::Test {
protected:
    void SetUp() override {
        ArticulationPatternSegment segment(ArrangementPattern(hundred_percent, 0),
                                           PitchPattern(ArticulationMap::expected_size, ten_percent, 0),
                                           ExpressionPattern(ArticulationMap::expected_size, ten_percent, 0));
        ArticulationPattern pattern;
        pattern.emplace(0, std::move(segment));
        profile_ = std::make_shared<ArticulationsProfile>();
        for (const ArticulationType type :
             {ArticulationType::STANDARD, ArticulationType::STACCATO, ArticulationType::TENUTO, ArticulationType::ACCENT,
              ArticulationType::TREMOLO32ND, ArticulationType::PIZZICATO, ArticulationType::STACCATISSIMO, ArticulationType::TREMOLO8TH,
              ArticulationType::PEDAL, ArticulationType::TRILL, ArticulationType::LEGATO}) {
            profile_->set_pattern(type, pattern);
        }
        profiles_.profiles.fill(profile_);
    }

    muse::mpe::ArticulationProfileBundle profiles_;
    ArticulationsProfilePtr profile_;
};

namespace {
constexpr duration_t quarter_at_120 = 500000;

std::unique_ptr<MasterScore> read_model_fixture(const String &file) {
    std::unique_ptr<MasterScore> score(ScoreRW::read_score(String(u"playback/playbackmodel_data/") + file));
    if (score) score->init_automation();
    return score;
}

const TrackEvents *first_part_track(const PlaybackScore &playback, const MasterScore &score) {
    const Part *part = score.parts().front();
    return playback.find({part->id(), part->instrument_id()});
}

const std::vector<std::pair<String, bool>> &builder_fixtures() {
    static const std::vector<std::pair<String, bool>> fixtures{
        {u"playback/playbackmodel_data/dynamics/dynamics.mscx", false},
        {u"playback/playbackmodel_data/spanners/spanners.mscx", false},
        {u"playback/playbackmodel_data/repeat_and_tremolo/repeat_and_tremolo.mscx", false},
        {u"playback/playbackmodel_data/multi_measure_repeat/multi_measure_repeat.mscx", false},
        {u"playback/playbackmodel_data/playback_setup_instruments/playback_setup_instruments.mscx", false},
        {u"playback/playbackmodel_data/metronome_6_4_with_repeat/metronome_6_4_with_repeat.mscx", true},
        {u"playback/playbackcontext_data/sound_flags/sound_flags.mscx", false},
        {u"playback/playbackcontext_data/lyrics_multiverses.mscx", false},
        {u"playback/playbackcontext_data/dynamics/hairpins_and_repeats.mscx", false},
    };
    return fixtures;
}

} // namespace

TEST_F(PlaybackScoreBuilderTests, UnrollsRepeatsAndJumps) {
    const std::pair<String, size_t> fixtures[]{
        {u"repeat_range/repeat_range.mscx", 6 * 4},
        {u"repeat_with_2_voltas/repeat_with_2_voltas.mscx", 7 * 4},
        {u"da_capo_al_fine/da_capo_al_fine.mscx", 8 * 4},
        {u"dal_segno_al_coda/dal_segno_al_coda.mscx", 8 * 4},
        {u"dal_segno_al_fine/dal_segno_al_fine.mscx", 9 * 4},
        {u"da_capo_al_coda/da_capo_al_coda.mscx", 11 * 4},
        {u"single_measure_repeat/single_measure_repeat.mscx", 2 * 4 + 4 * 8 + 1},
        {u"multi_measure_repeat/multi_measure_repeat.mscx", 8 * 4 + 1},
    };
    for (const auto &[file, count] : fixtures) {
        SCOPED_TRACE(file.to_std_string());
        const auto score = read_model_fixture(file);
        ASSERT_TRUE(score);
        const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
        const auto *track = first_part_track(playback, *score);
        ASSERT_TRUE(track);
        EXPECT_EQ(track->notes.size(), count);
    }
}

TEST_F(PlaybackScoreBuilderTests, TremoloStaysOnTimeAfterRepeats) {
    const auto score = read_model_fixture(u"repeat_and_tremolo/repeat_and_tremolo.mscx");
    ASSERT_TRUE(score);
    const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
    const auto *track = first_part_track(playback, *score);
    ASSERT_TRUE(track);
    // Half-note offsets include the repeat and the rest before the final three tremolos.
    constexpr std::array offsets{0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12};
    constexpr duration_t half_note = 2 * quarter_at_120;
    ASSERT_EQ(track->notes.size(), offsets.size() * 16);
    for (size_t group = 0; group < offsets.size(); ++group) {
        const timestamp_t start = offsets[group] * half_note;
        for (int stroke = 0; stroke < 16; ++stroke) {
            const auto &note = track->notes[group * 16 + stroke];
            EXPECT_EQ(note.group_timestamp, start);
            EXPECT_EQ(note.actual_timestamp, start + stroke * (half_note / 16));
        }
    }
}

TEST_F(PlaybackScoreBuilderTests, PreservesTiesThroughMeasureRepeats) {
    const auto score = read_model_fixture(u"measure_repeat_tied_notes.mscx");
    ASSERT_TRUE(score);
    const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
    const auto *track = first_part_track(playback, *score);
    ASSERT_TRUE(track);
    constexpr std::array measure_offsets{1, 2, 4, 5};
    const duration_t whole_note = 4 * quarter_at_120;
    ASSERT_EQ(track->notes.size(), measure_offsets.size());
    for (size_t i = 0; i < measure_offsets.size(); ++i) {
        EXPECT_EQ(track->notes[i].actual_timestamp, measure_offsets[i] * whole_note);
        EXPECT_EQ(track->notes[i].actual_duration, whole_note);
    }
}

TEST_F(PlaybackScoreBuilderTests, PreservesTiedDurationAfterTempoChangesAndRepeats) {
    const auto score = read_model_fixture(u"repeat_tempo_changes_and_tie/repeat_tempo_changes_and_tie.mscx");
    ASSERT_TRUE(score);
    const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
    const auto *track = first_part_track(playback, *score);
    ASSERT_TRUE(track);
    ASSERT_EQ(track->notes.size(), 1u);
    EXPECT_EQ(track->notes.front().nominal_duration, 8 * quarter_at_120);
}

TEST_F(PlaybackScoreBuilderTests, IntegratesTempoChangesWithinNotes) {
    const auto score = read_model_fixture(u"tempo_changes_during_notes/tempo_changes_during_notes.mscx");
    ASSERT_TRUE(score);
    const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
    const auto *track = first_part_track(playback, *score);
    ASSERT_TRUE(track);

    const auto quarter = [](int bpm) { return duration_t{60000000} / bpm; };
    constexpr std::array stepped_tempo{100, 100, 10, 10, 10, 10, 100, 100};
    constexpr std::array ritenuto{100, 95, 90, 85, 80, 75, 70, 65};
    const auto tied_duration = [&](const auto &tempos) {
        duration_t duration = 0;
        for (int bpm : tempos) duration += quarter(bpm);
        return duration;
    };
    std::vector<duration_t> expected{tied_duration(stepped_tempo), tied_duration(ritenuto)};
    for (const auto &tempos : {stepped_tempo, ritenuto}) {
        for (int bpm : tempos) expected.insert(expected.end(), 2, quarter(bpm) / 2);
    }
    expected.push_back(tied_duration(stepped_tempo));
    expected.push_back(tied_duration(ritenuto));

    ASSERT_EQ(track->notes.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        SCOPED_TRACE(i);
        const auto &note = track->notes[i];
        EXPECT_NEAR(note.nominal_duration, expected[i], 2000);
        if (i >= expected.size() - 2) {
            const auto articulations = track->note_articulations(note);
            ASSERT_EQ(articulations.size(), 1u);
            EXPECT_EQ(articulations.front().type, ArticulationType::PEDAL);
            EXPECT_NEAR(articulations.front().timestamp, note.nominal_timestamp, 2000);
            EXPECT_NEAR(articulations.front().overall_duration, expected[i], 2000);
        }
    }
}

TEST_F(PlaybackScoreBuilderTests, AppliesSpannersToTheirCoveredNotes) {
    const auto score = read_model_fixture(u"spanners/spanners.mscx");
    ASSERT_TRUE(score);
    const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
    const auto *track = first_part_track(playback, *score);
    ASSERT_TRUE(track);
    constexpr std::array types{ArticulationType::PEDAL, ArticulationType::TRILL, ArticulationType::LEGATO};
    size_t next = 0;
    for (size_t beat = 0; beat < types.size() * 4; ++beat) {
        SCOPED_TRACE(beat);
        const auto start = static_cast<timestamp_t>(beat) * quarter_at_120;
        ASSERT_LT(next, track->notes.size());
        ASSERT_EQ(track->notes[next].group_timestamp, start);
        const bool covered = beat % 4 == 1 || beat % 4 == 2;
        const auto expected = covered ? types[beat / 4] : ArticulationType::STANDARD;
        // A trill expands a beat into multiple records with the same group timestamp.
        do {
            const auto articulations = track->note_articulations(track->notes[next++]);
            ASSERT_EQ(articulations.size(), 1u);
            EXPECT_EQ(articulations.front().type, expected);
            if (covered) {
                EXPECT_EQ(articulations.front().timestamp, (beat / 4 * 4 + 1) * quarter_at_120);
                EXPECT_EQ(articulations.front().overall_duration, 2 * quarter_at_120);
            }
        } while (next < track->notes.size() && track->notes[next].group_timestamp == start);
    }
    EXPECT_EQ(next, track->notes.size());
}

TEST_F(PlaybackScoreBuilderTests, ResetsPizzicatoAtArco) {
    const auto score = read_model_fixture(u"pizz_to_arco/pizz_to_arco.mscx");
    ASSERT_TRUE(score);
    const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
    const auto *track = first_part_track(playback, *score);
    ASSERT_TRUE(track);
    ASSERT_EQ(track->notes.size(), 4u);
    for (size_t i = 0; i < track->notes.size(); ++i) {
        SCOPED_TRACE(i);
        const auto &note = track->notes[i];
        EXPECT_EQ(note.group_timestamp, i * quarter_at_120);
        const auto articulations = track->note_articulations(note);
        ASSERT_EQ(articulations.size(), i == 0 ? 2u : 1u);
        EXPECT_TRUE(contains_articulation(articulations, i < 2 ? ArticulationType::PIZZICATO : ArticulationType::STANDARD));
        if (i == 0) EXPECT_TRUE(contains_articulation(articulations, ArticulationType::STACCATISSIMO));
    }
}

TEST_F(PlaybackScoreBuilderTests, ResolvesSparseDynamicsAcrossHairpins) {
    const auto score = read_model_fixture(u"dynamics/dynamics.mscx");
    ASSERT_TRUE(score);
    const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
    const auto *track = first_part_track(playback, *score);
    ASSERT_TRUE(track);
    ASSERT_FALSE(track->dynamic_layers.empty());
    const auto curve = track->layer_points(track->dynamic_layers.front());
    ASSERT_FALSE(curve.empty());
    EXPECT_LT(curve.size(), 10u);
    const auto level_at = [&](timestamp_t time) { return dynamic_level_from_normalized(evaluate_curve_at(curve, time)); };
    const auto piano = dynamic_level_from_type(mpe::DynamicType::P);
    const auto forte = dynamic_level_from_type(mpe::DynamicType::F);
    const auto pianissimo = dynamic_level_from_type(mpe::DynamicType::PP);
    EXPECT_EQ(level_at(0), piano);
    EXPECT_EQ(level_at(4 * quarter_at_120), piano);
    EXPECT_EQ(level_at(8 * quarter_at_120), forte);
    EXPECT_EQ(level_at(12 * quarter_at_120), forte);
    // The second hairpin approaches its target one tick before the sudden pp.
    EXPECT_NEAR(level_at(16 * quarter_at_120 - quarter_at_120 / 480), forte + dynamic_level_step, 1);
    EXPECT_EQ(level_at(16 * quarter_at_120), pianissimo);
    EXPECT_EQ(curve.back().at, 16 * quarter_at_120);
}

TEST_F(PlaybackScoreBuilderTests, ResolvesSetupDataForMultiInstrumentScore) {
    const auto score = read_model_fixture(u"playback_setup_instruments/playback_setup_instruments.mscx");
    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 12u);

    constexpr bool supports_snd = true;
    const muse::HashMap<String, PlaybackSetupData> expected_setup_data{
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

    const auto playback = PlaybackScoreBuilder{profiles_}.build(score.get());
    for (const Part *part : score->parts()) {
        for (const auto &pair : part->instruments()) {
            const String &instrument_id = pair.second->id();
            const TrackEvents *track = playback.find({part->id(), instrument_id});
            ASSERT_TRUE(track) << instrument_id.to_std_string();
            EXPECT_EQ(track->setup, expected_setup_data.at(instrument_id)) << instrument_id.to_std_string();
        }
    }
}

TEST_F(PlaybackScoreBuilderTests, FallsBackToStandardArticulationWhenUnsupported) {
    const auto score = read_model_fixture(u"wrong_articulations/wrong_articulations.mscx");
    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 2u);

    const Part *winds_part = score->parts().at(0);
    const Part *voice_part = score->parts().at(1);
    ASSERT_EQ(winds_part->instrument_id(), "winds");
    ASSERT_EQ(voice_part->instrument_id(), "voice");

    auto standard_only_profile = std::make_shared<ArticulationsProfile>();
    standard_only_profile->set_pattern(ArticulationType::STANDARD, profile_->pattern(ArticulationType::STANDARD));
    muse::mpe::ArticulationProfileBundle standard_only_bundle;
    standard_only_bundle.profiles.fill(standard_only_profile);

    const auto playback = PlaybackScoreBuilder{standard_only_bundle}.build(score.get());
    for (const Part *part : {winds_part, voice_part}) {
        const std::string instrument_id = part->instrument_id().to_std_string();
        const TrackEvents *track = playback.find({part->id(), part->instrument_id()});
        ASSERT_TRUE(track) << instrument_id;
        ASSERT_EQ(track->notes.size(), 8u) << instrument_id;
        for (const NoteRecord &note : track->notes) {
            const auto articulations = track->note_articulations(note);
            ASSERT_EQ(articulations.size(), 1u) << instrument_id;
            EXPECT_EQ(articulations.front().type, ArticulationType::STANDARD) << instrument_id;
            EXPECT_GT(note.actual_duration, 0) << instrument_id;
        }
    }
}

TEST_F(PlaybackScoreBuilderTests, RendersFixtureNotesAndOptionalMetronome) {
    for (const auto &[path, metronome] : builder_fixtures()) {
        const std::unique_ptr<MasterScore> score(ScoreRW::read_score(path));
        ASSERT_TRUE(score) << path.to_std_string();
        score->init_automation();
        PlaybackScoreBuilder builder{profiles_};
        builder.metronome_enabled = metronome;
        const auto flat = builder.build(score.get());

        const auto *clicks = flat.find(playback_metronome_track_id());
        ASSERT_TRUE(clicks) << path.to_std_string();
        EXPECT_EQ(clicks->notes.size(), metronome ? 6u * 5u : 0u) << path.to_std_string();
        std::size_t notes = 0;
        for (const auto &[id, track] : flat.tracks)
            if (id != playback_metronome_track_id()) notes += track.notes.size();
        EXPECT_GT(notes, 0u) << path.to_std_string();
    }
}

TEST_F(PlaybackScoreBuilderTests, BuilderCarriesDistinctPresets) {
    const String fixture = String::from_utf8(ENGRAVING_TESTS_DATA_ROOT) + u"/../../../tests/data/playback_dod/cold_preset.mscx";
    const std::unique_ptr<MasterScore> score(ScoreRW::read_score(fixture, true));
    ASSERT_TRUE(score);
    score->init_automation();
    const auto flat = PlaybackScoreBuilder{profiles_}.build(score.get());

    const Part *part = score->parts().front();
    const TrackEvents *track = flat.find(InstrumentTrackId{part->id(), part->instrument_id()});
    ASSERT_TRUE(track);
    std::vector<String> presets;
    bool text = false, syllable = false;
    for (const ColdEvent &cold : track->cold) {
        if (const auto *preset = std::get_if<SoundPresetChangeEvent>(&cold.event)) {
            presets.push_back(preset->code);
        }
        text = text || std::holds_alternative<TextArticulationEvent>(cold.event);
        syllable = syllable || (std::holds_alternative<SyllableEvent>(cold.event) && cold.group_timestamp == 0);
    }
    // Both presets of the first flag (in order) at tick 0, then the beat-3 flag's
    ASSERT_GE(presets.size(), 3u);
    EXPECT_EQ(presets[0], String(u"Engrave.Preset.Δ1"));
    EXPECT_EQ(presets[1], String(u"Second.Preset"));
    EXPECT_EQ(presets.back(), String(u"Third.Preset"));
    EXPECT_TRUE(text);
    EXPECT_TRUE(syllable);
    // At tick 0 the cold events precede the chord's notes, even though they use a separate store
    ASSERT_FALSE(track->notes.empty());
    EXPECT_GT(track->notes.front().seq, track->cold.front().seq);
}

TEST_F(PlaybackScoreBuilderTests, BuilderPollsAndClearsTheCheckpoint) {
    const std::unique_ptr<MasterScore> score(ScoreRW::read_score(u"playback/playbackmodel_data/spanners/spanners.mscx"));
    ASSERT_TRUE(score);
    score->init_automation();
    const PlaybackScoreBuilder builder{profiles_};

    std::uint64_t built_polls = 0;
    const auto count_poll = [&] { ++built_polls; };
    auto flat = builder.build(score.get(), count_poll);
    const auto polls_before_release = built_polls;
    flat.release();
    EXPECT_EQ(built_polls, polls_before_release);
    EXPECT_GT(built_polls, 50u);

    struct Cancelled {};

    std::uint64_t polls = 0;
    EXPECT_THROW(std::ignore = builder.build(score.get(),
                                             [&] {
                                                 if (++polls == 50) throw Cancelled{};
                                             }),
                 Cancelled);
}

TEST_F(PlaybackScoreBuilderTests, BuilderReturnsNoTracksForNullScore) {
    const PlaybackScoreBuilder builder{profiles_};
    EXPECT_TRUE(builder.build(nullptr).tracks.empty());
}

TEST_F(PlaybackScoreBuilderTests, RendererEmitsEmptyGroupsAndAppendsToExistingKeys) {
    std::unique_ptr<MasterScore> score(ScoreRW::read_score(u"playback/playbackmodel_data/repeat_range/repeat_range.mscx"));
    ASSERT_TRUE(score);
    Segment *segment = score->first_measure()->first(SegmentType::CHORD_REST);
    ASSERT_TRUE(segment);
    const Chord *chord = nullptr;
    for (EngravingItem *item : segment->elist()) {
        if (item && item->is_chord()) {
            chord = to_chord(item);
            break;
        }
    }
    ASSERT_TRUE(chord);
    // A chord without notes takes the fixed-event path and renders an empty list
    std::unique_ptr<Chord> empty_chord(Factory::create_chord(segment));
    empty_chord->set_track(chord->track());
    ASSERT_TRUE(empty_chord->notes().empty());

    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score.get());
    const PlaybackEventsRenderer renderer;
    const dynamic_level_t level = dynamic_level_from_type(mpe::DynamicType::MF);

    TrackEvents events;
    FreezeProgress progress;
    renderer.render(empty_chord.get(), 1000, 2000, level, ctx, profile_, events, progress);
    ASSERT_EQ(events.empty_groups.size(), 1u);
    EXPECT_EQ(events.empty_groups.front().group_timestamp, 1000);
    EXPECT_EQ(events.empty_groups.front().seq, 0u);
    EXPECT_TRUE(events.notes.empty());

    renderer.render(chord, 1000, 2000, level, ctx, profile_, events, progress);
    renderer.render(chord, 1000, 3000, level, ctx, profile_, events, progress);
    const std::size_t chord_size = chord->notes().size();
    ASSERT_EQ(events.notes.size(), 2 * chord_size);
    for (std::size_t i = 0; i < events.notes.size(); ++i) {
        EXPECT_EQ(events.notes[i].group_timestamp, 1000);
        EXPECT_EQ(events.notes[i].seq, i + 1);
        EXPECT_EQ(events.notes[i].nominal_duration, i < chord_size ? 2000 : 3000);
    }

    Harmony harmony(segment);
    harmony.set_track(chord->track());
    harmony.set_harmony(String(u"C"));
    ASSERT_TRUE(harmony.is_realizable());
    // Use a valid chord symbol with an empty realized voicing.
    harmony.realized_harmony().set_dirty(false);
    ASSERT_TRUE(harmony.get_realized_harmony().notes().empty());
    const std::size_t previous_notes = events.notes.size();
    renderer.render_chord_symbol(&harmony, 0, profile_, ctx, events, progress);
    ASSERT_EQ(events.empty_groups.size(), 2u);
    EXPECT_EQ(events.empty_groups.back().group_timestamp, 0);
    EXPECT_EQ(events.empty_groups.back().seq, previous_notes + 1);
    EXPECT_EQ(events.notes.size(), previous_notes);
}

namespace mu::engraving {
struct PlaybackContextReleaseTestAccess {
    static void materialization(PlaybackContext &context, const std::array<mu::engraving::AutomationCurve, 4> &curves) {
        for (int track = 0; track < 4; ++track) {
            context.used_tracks_.insert(track);
            context.dynamics_curve_by_track_.emplace(track, &curves[track]);
            for (int i = 0; i < 50000; ++i) {
                const int tick = i * 4 + track;
                context.syllables_by_track_[track].emplace(tick, SyllableEvent{String(u"lyric")});
                context.text_articulations_by_track_[track].emplace(tick, TextArticulationEvent{});
                // A large group requires per-event polls beyond the cursor scans.
                if (i == 0) context.sound_presets_by_track_[track].emplace(tick, SoundPresetChangeEventList(50000));
            }
        }
    }

    static void populate(PlaybackContext &context) {
        for (int i = 0; i < 100000; ++i) {
            context.dynamics_curve_by_track_.emplace(i, nullptr);
            // Opaque, distinct keys: this test only hashes and releases them, never dereferences them.
            context.current_verse_num_by_chord_rest_.emplace(reinterpret_cast<const ChordRest *>(std::uintptr_t(i + 1) * 16), i);
        }
        auto &syllables = context.syllables_by_track_[0];
        for (int i = 0; i < 200000; ++i) {
            syllables.emplace(i, SyllableEvent{String::from_utf8(std::to_string(i))});
        }
    }

    static std::size_t size(const PlaybackContext &context) {
        std::size_t result = context.dynamics_curve_by_track_.size() + context.current_verse_num_by_chord_rest_.size();
        for (const auto &[track, syllables] : context.syllables_by_track_) result += syllables.size();
        return result;
    }
};
} // namespace mu::engraving

TEST(FlatPlaybackStoreTests, ReleaseBoundsColdOwnersAndFinishesAfterCancellation) {
    for (int cancel_at : {0, 3, 60}) {
        PlaybackScore score;
        score.context = std::make_shared<PlaybackContext>(nullptr);
        PlaybackContextReleaseTestAccess::populate(*score.context);
        const std::weak_ptr<PlaybackContext> owner = score.context;
        for (int t = 0; t < 4; ++t) {
            auto &events = score.track({static_cast<std::uint64_t>(t + 1), String(u"test")});
            for (int i = 0; i < 50000; ++i) {
                events.cold.push_back({i, static_cast<std::uint32_t>(i), SyllableEvent{String::from_utf8(std::to_string(i))}});
            }
        }
        std::size_t previous = 600000, polls = 0;

        struct Cancelled {};

        const auto release = [&] {
            score.release([&] {
                std::size_t current = score.context ? PlaybackContextReleaseTestAccess::size(*score.context) : 0;
                for (const auto &[id, events] : score.tracks) current += events.cold.size();
                EXPECT_LE(previous - current, 4096u);
                previous = current;
                ++polls;
                if (cancel_at && polls == cancel_at) throw Cancelled{};
            });
        };
        const auto start = std::chrono::steady_clock::now();
        if (cancel_at)
            EXPECT_THROW(release(), Cancelled);
        else {
            release();
            EXPECT_GE(polls, 146u);
        }
        EXPECT_TRUE(score.tracks.empty());
        EXPECT_FALSE(score.context);
        EXPECT_TRUE(owner.expired());
        EXPECT_LT((std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()), 100.0);
    }
}

TEST(FlatPlaybackStoreTests, PartColdCopiesReleaseInPolledStridesEvenDuringUnwind) {
    struct Cancelled {};

    for (int cancel_at : {0, 3}) {
        for (bool unwind : {false, true}) {
            std::size_t previous = 0, polls = 0;
            PlaybackPartData *live = nullptr;
            const auto run = [&] {
                const auto checkpoint = [&] {
                    const std::size_t current = live->sound_presets.size() + live->text_articulations.size() + live->syllables.size();
                    EXPECT_LE(previous - current, 4096u);
                    previous = current;
                    if (++polls == cancel_at) throw Cancelled{};
                };
                PlaybackPartData data(checkpoint);
                live = &data;
                for (int i = 0; i < 50000; ++i) {
                    data.sound_presets[i].push_back(SoundPresetChangeEvent{});
                    data.text_articulations[i].push_back(TextArticulationEvent{});
                    data.syllables[i].push_back(SyllableEvent{String(u"lyric")});
                }
                previous = 150000;
                if (unwind) throw Cancelled{};
                data.release();
                EXPECT_TRUE(data.sound_presets.empty());
                EXPECT_TRUE(data.text_articulations.empty());
                EXPECT_TRUE(data.syllables.empty());
            };
            if (cancel_at || unwind)
                EXPECT_THROW(run(), Cancelled);
            else
                run();
            if (!cancel_at) {
                EXPECT_EQ(previous, 0u);
                EXPECT_GE(polls, 37u);
            } else
                EXPECT_EQ(polls, 3u);
        }
    }
}

TEST(FlatPlaybackStoreTests, CancelsInsideContextMaterialization) {
    auto *score = ScoreRW::read_score(u"playback/playbackcontext_data/lyrics_multiverses.mscx");
    ASSERT_TRUE(score);
    score->init_automation();
    std::array<mu::engraving::AutomationCurve, 4> curves;
    for (int track = 0; track < 4; ++track) {
        auto hint = curves[track].end();
        for (int i = 0; i < 200000; ++i) {
            mu::engraving::AutomationPoint point;
            point.value.out_value = 0.1 * (track + 1);
            hint = curves[track].insert(hint, {i, point});
        }
    }
    PlaybackContext context(score);
    PlaybackContextReleaseTestAccess::materialization(context, curves);

    struct Cancelled {};

    using Clock = std::chrono::steady_clock;
    const auto milliseconds = [](auto duration) { return std::chrono::duration<double, std::milli>(duration).count(); };
    // ASan tracks allocations during materialization and unwind, so the latency budget belongs to the release lane.
#if defined(__clang__)
    constexpr bool enforce_latency = !__has_feature(address_sanitizer);
#elif defined(__SANITIZE_ADDRESS__)
    constexpr bool enforce_latency = false;
#else
    constexpr bool enforce_latency = true;
#endif
    for (int pass = 0; pass < 4; ++pass) {
        const auto run = [&](const auto &checkpoint) {
            if (pass == 0) std::ignore = context.syllables(0, 4, checkpoint);
            if (pass == 1) std::ignore = context.text_articulations(0, 4, checkpoint);
            if (pass == 2) std::ignore = context.sound_presets(0, 4, checkpoint);
            if (pass == 3) std::ignore = context.dynamic_level_layers(0, 4, checkpoint);
        };
        std::size_t polls = 0, largest_at = 0;
        double maximum = 0;
        auto previous = Clock::now();
        // Complete the populated pass, including every distinct curve transition.
        run([&] {
            const auto now = Clock::now();
            const double interval = milliseconds(now - previous);
            if (polls && interval > maximum) {
                maximum = interval;
                largest_at = polls - 1;
            }
            previous = now;
            ++polls;
        });
        EXPECT_GT(polls, 200000u); // Presets require more than cursor-scan polls.
        if constexpr (enforce_latency) {
            EXPECT_LT(maximum, 2.5);
        }
        double return_maximum = 0;
        for (std::size_t cancel_at : {largest_at, polls - 2}) {
            std::size_t current = 0;
            Clock::time_point requested;
            bool pending = false;
            EXPECT_THROW(run([&] {
                             if (pending) throw Cancelled{};
                             if (current++ == cancel_at) {
                                 requested = Clock::now();
                                 pending = true;
                             }
                         }),
                         Cancelled);
            ASSERT_TRUE(pending);
            return_maximum = std::max(return_maximum, milliseconds(Clock::now() - requested));
        }
        std::cout << "materialization pass " << pass << " max_checkpoint_interval_ms=" << maximum
                  << " request_to_return_including_unwind_ms=" << return_maximum << '\n';
        if constexpr (enforce_latency) {
            EXPECT_LT(return_maximum, 100.0);
        }
    }
    delete score;
}
