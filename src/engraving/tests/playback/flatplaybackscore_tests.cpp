// SPDX-License-Identifier: GPL-3.0-only
// Steps 4b-4c of the playback DoD refactor: the flat MPE store, its freeze rules, the export's builder and,
// in the harness build, dual execution through the playback model, against the legacy oracle through the
// dump B extractors; plus the renderer's sink contract.

#include <algorithm>
#include <bit>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "audio/engine/internal/synthesizers/fluidsynth/fluidsequencer.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/playback/flatplaybackscore.h"
#include "engraving/playback/playbackeventsrenderer_impl.h"
#include "engraving/playback/playbackmodel.h"
#include "mpe/tests/mocks/articulationprofilesrepositorymock.h"
#include "mpe/tests/utils/articulationutils.h"
#include "playback_projection_flat.h"
#include "playback_projection_legacy.h"
#include "utils/scorerw.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

namespace {
// A sink defined only here: it links against the renderer through playbackeventsrenderer_impl.h
// without any change to the renderer unit (step-4a review finding 1)
struct RecordingSink {
    std::vector<std::pair<timestamp_t, std::size_t>> calls;

    void operator()(const timestamp_t key, PlaybackEventList &&list) { calls.emplace_back(key, list.size()); }
};

static_assert(PlaybackEventSink<RecordingSink>);

// The same list through the legacy sink and the flat freeze, as dual execution does, without the
// test-only DualEventsSink (so these tests also build without the harness)
struct BothSink {
    EventsMapSink legacy;
    FlatEventsSink flat;

    void operator()(const timestamp_t key, PlaybackEventList &&list) const {
        flat(key, PlaybackEventList(list));
        legacy(key, std::move(list));
    }
};

static_assert(PlaybackEventSink<BothSink>);

ArticulationAppliedData applied(const ArticulationType type, const timestamp_t timestamp, const duration_t duration) {
    return ArticulationAppliedData(ArticulationMeta(type, ArticulationPattern{}, timestamp, duration), 0, hundred_percent);
}

mpe::NoteEvent make_note(const timestamp_t at, const pitch_level_t pitch, const bool with_override) {
    ArrangementContext arrangement;
    arrangement.nominal_timestamp = at;
    arrangement.actual_timestamp = at + 7;
    arrangement.nominal_duration = 500000;
    arrangement.actual_duration = 450001;
    arrangement.voice_layer_index = 2;
    arrangement.staff_layer_index = 3;
    arrangement.bps = 2.125;

    PitchContext pitch_ctx;
    pitch_ctx.nominal_pitch_level = pitch;
    pitch_ctx.pitch_curve.emplace(0, 0);
    pitch_ctx.pitch_curve.emplace(5000, -1200);

    ExpressionContext expression;
    // Inserted out of enum order: the map (and so the flat record) is in ascending ArticulationType
    expression.articulations.emplace(ArticulationType::TENUTO, applied(ArticulationType::TENUTO, at, 400000));
    expression.articulations.emplace(ArticulationType::STACCATO, applied(ArticulationType::STACCATO, at + 1, 300000));
    expression.nominal_dynamic_level = dynamic_level_from_type(mpe::DynamicType::MF);
    expression.expression_curve.emplace(0, 7000);
    expression.expression_curve.emplace(10000, 6500);
    if (with_override) {
        expression.velocity_override = 0.375f;
    }

    return mpe::NoteEvent(std::move(arrangement), std::move(pitch_ctx), std::move(expression));
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
    layers.emplace(4, curve);
    layers.emplace(1, curve);
    return layers;
}

// The same emissions through the legacy sink and the flat freeze, as dual execution does
struct DualFixture {
    PlaybackData legacy;
    PlaybackScore flat;
    InstrumentTrackId id{ID(7), String(u"violin")};

    void emit(const timestamp_t key, PlaybackEventList list) {
        BothSink sink{{legacy.origin_events}, {flat.track(id), flat.progress}};
        sink(key, std::move(list));
    }

    template <class Event>
    void emit_context(const timestamp_t key, const std::vector<Event> &events) {
        PlaybackEventList &list = legacy.origin_events[key];
        list.insert(list.end(), events.begin(), events.end());
        freeze_cold_events(flat.track(id), key, std::span{events}, flat.progress);
    }

    void build() {
        legacy.setup_data = PlaybackSetupData(String(u"violin"), SoundCategory::STRINGS, StringList{String(u"orchestral")});
        legacy.setup_data.score_id = "score-1";
        flat.track(id).setup = legacy.setup_data;
        legacy.dynamics = make_dynamics();
        freeze_dynamics(flat.track(id), legacy.dynamics, flat.progress);

        // Context events first, as apply_context_to_track_data inserts them before the walk
        emit_context(0, std::vector<SoundPresetChangeEvent>{make_preset(u"Engrave.Preset.Δ", 3)});
        emit_context(0, std::vector<TextArticulationEvent>{make_text(u"mute", 3)});
        emit_context(0, std::vector<SyllableEvent>{make_syllable(u"la", 3)});
        emit_context(250000, std::vector<SoundPresetChangeEvent>{});

        emit(0, PlaybackEventList{make_note(0, 6500, false), make_note(0, 6900, true)});
        emit(125000, PlaybackEventList{});
        emit(500000, PlaybackEventList{make_note(500000, 6600, false)});
        emit(0, PlaybackEventList{make_note(0, 7300, false)}); // appended to an existing group
    }

    std::string legacy_projection() const {
        const music::test::LegacyProjectionTrack track{id.part_id.to_uint64(), id.instrument_id, &legacy};
        music::test::LegacyProjection projection;
        projection.score(std::span{&track, 1});
        return projection.out;
    }

    static std::string flat_projection(const PlaybackScore &score) {
        music::test::FlatProjection projection;
        projection.score(score);
        return projection.str();
    }
};
} // namespace

TEST(FlatPlaybackStoreTests, FreezeKeepsLegacyOrderAndFields) {
    DualFixture fixture;
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
    EXPECT_EQ(note.actual_timestamp, 7);
    EXPECT_EQ(note.nominal_duration, 500000);
    EXPECT_EQ(note.actual_duration, 450001);
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
    ASSERT_EQ(expression.size(), 2u);
    EXPECT_EQ(expression[0].value, 7000);

    const auto &preset = std::get<SoundPresetChangeEvent>(track.cold[0].event);
    EXPECT_EQ(preset.code, String(u"Engrave.Preset.Δ"));
    EXPECT_EQ(preset.layer_idx, 3u);
    EXPECT_TRUE(std::holds_alternative<SyllableEvent>(track.cold[2].event));

    ASSERT_EQ(track.dynamic_layers.size(), 2u);
    EXPECT_EQ(track.dynamic_layers[0].layer, 1u);
    EXPECT_EQ(track.layer_points(track.dynamic_layers[1]).size(), 2u);
    EXPECT_EQ(track.setup.id, String(u"violin"));

    // Dynamics are assigned, not appended
    freeze_dynamics(fixture.flat.track(fixture.id), make_dynamics(), fixture.flat.progress);
    EXPECT_EQ(track.dynamic_points.size(), 4u);
}

TEST(FlatPlaybackStoreTests, FreezeRejectsEventsWithoutProducerAndKeyMismatch) {
    TrackEvents track;
    FreezeProgress progress;
    const PlaybackEventList monostate{PlaybackEvent{}};
    EXPECT_THROW(freeze_events(track, 0, monostate, progress), std::logic_error);
    const PlaybackEventList controller{ControllerChangeEvent{}};
    EXPECT_THROW(freeze_events(track, 0, controller, progress), std::logic_error);

    mpe::NoteEvent note = make_note(0, 6500, false);
    ExpressionContext expression = note.expression_ctx();
    expression.articulations.emplace(ArticulationType::ACCENT, applied(ArticulationType::MARCATO, 0, 1));
    ArrangementContext arrangement = note.arrangement_ctx();
    PitchContext pitch = note.pitch_ctx();
    const PlaybackEventList mismatched{mpe::NoteEvent(std::move(arrangement), std::move(pitch), std::move(expression))};
    EXPECT_THROW(freeze_events(track, 0, mismatched, progress), std::logic_error);
}

// Design B3: curve entries keep the legacy value type, so the shared helpers see the same arithmetic on
// every platform. percentage_t is 32-bit on MSVC and 64-bit with libstdc++; the wide cases only run there
TEST(FlatPlaybackStoreTests, CurveValuesKeepTheLegacyTypeAndHelperResults) {
    static_assert(std::is_same_v<decltype(max_amplitude_level(std::declval<std::span<const CurvePoint>>())), percentage_t>);
    static_assert(std::is_same_v<decltype(amplitude_value_point(std::declval<std::span<const CurvePoint>>()).second),
                                 decltype(std::declval<PitchCurve>().amplitude_value_point().second)>);

    using Limits = std::numeric_limits<percentage_t>;
    std::vector<std::vector<percentage_t>> cases{
        {0, 120, -340}, {std::numeric_limits<std::int32_t>::max(), -5}, {std::numeric_limits<std::int32_t>::min() + 1, 7}};
    if constexpr (sizeof(percentage_t) > sizeof(std::int32_t)) {
        // Representable by the legacy curve only when percentage_t is wider than int32 (Linux)
        const percentage_t above = static_cast<percentage_t>(std::numeric_limits<std::int32_t>::max()) + 1;
        const percentage_t at_min = std::numeric_limits<std::int32_t>::min();
        cases.push_back({above, 3});
        cases.push_back({at_min, -2});
        cases.push_back({static_cast<percentage_t>(at_min) - 1, 1});
        cases.push_back({Limits::max() / 2, Limits::min() / 2});
    }

    for (const auto &values : cases) {
        mpe::NoteEvent note = make_note(0, 6500, false);
        PitchContext pitch = note.pitch_ctx();
        ExpressionContext expression = note.expression_ctx();
        pitch.pitch_curve = PitchCurve{};
        expression.expression_curve = ExpressionCurve{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            pitch.pitch_curve.emplace(static_cast<duration_percentage_t>(i * 1000), values[i]);
            expression.expression_curve.emplace(static_cast<duration_percentage_t>(i * 1000 + 500), values[i]);
        }
        const PitchCurve pitch_curve = pitch.pitch_curve;
        const ExpressionCurve expression_curve = expression.expression_curve;
        ArrangementContext arrangement = note.arrangement_ctx();
        const PlaybackEventList list{mpe::NoteEvent(std::move(arrangement), std::move(pitch), std::move(expression))};

        TrackEvents track;
        FreezeProgress progress;
        freeze_events(track, 0, list, progress);
        ASSERT_EQ(track.notes.size(), 1u);
        const auto flat_pitch = track.pitch_curve(track.notes[0]);
        const auto flat_expression = track.expression_curve(track.notes[0]);
        ASSERT_EQ(flat_pitch.size(), values.size());
        for (std::size_t i = 0; i < values.size(); ++i) {
            EXPECT_EQ(flat_pitch[i].value, values[i]);
            EXPECT_EQ(flat_expression[i].value, values[i]);
        }
        // The helpers the sequencer uses, on the flat span and on the legacy curve
        EXPECT_EQ(max_amplitude_level(flat_pitch), pitch_curve.max_amplitude_level());
        EXPECT_EQ(amplitude_value_point(flat_expression), expression_curve.amplitude_value_point());
        EXPECT_EQ(max_amplitude_level(flat_expression), expression_curve.max_amplitude_level());
        const float span_fraction = velocity_fraction(flat_expression);
        const float legacy_fraction = expression_curve.velocity_fraction();
        EXPECT_EQ(std::bit_cast<std::uint32_t>(span_fraction), std::bit_cast<std::uint32_t>(legacy_fraction));
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
        FreezeProgress progress;
        progress.checkpoint = [] { throw Cancelled{}; };
        EXPECT_THROW(freeze_cold_events(track, 0, std::span{texts}, progress), Cancelled);
        EXPECT_EQ(track.cold.size(), stride);
    }
    {
        // One large chord list: notes, articulations and curve points all count
        PlaybackEventList list;
        for (std::uint32_t i = 0; i < stride; ++i) list.emplace_back(make_note(0, 6500, false));
        TrackEvents track;
        FreezeProgress progress;
        std::uint32_t polls = 0;
        progress.checkpoint = [&polls] { ++polls; };
        freeze_events(track, 0, list, progress);
        const std::uint64_t records = track.notes.size() + track.articulations.size() + track.curve_points.size();
        EXPECT_EQ(polls, records / stride);
        EXPECT_EQ(progress.pending, records % stride);
    }
    {
        // One layer with many automation points
        DynamicAutomationMap curve;
        for (std::uint32_t i = 0; i < stride + 1; ++i) curve.emplace(i, mpe::AutomationPoint{});
        DynamicAutomationLayers layers;
        layers.emplace(0, curve);
        TrackEvents track;
        FreezeProgress progress;
        progress.checkpoint = [] { throw Cancelled{}; };
        EXPECT_THROW(freeze_dynamics(track, layers, progress), Cancelled);
        EXPECT_EQ(track.dynamic_points.size(), stride);
    }
    {
        // The stride continues across calls: small lists still reach a poll
        TrackEvents track;
        FreezeProgress progress;
        std::uint32_t polls = 0;
        progress.checkpoint = [&polls] { ++polls; };
        const std::vector<SyllableEvent> one{make_syllable(u"a", 0)};
        for (std::uint32_t i = 0; i < stride * 3; ++i) freeze_cold_events(track, i, std::span{one}, progress);
        EXPECT_EQ(polls, 3u);
    }
}

TEST(FlatPlaybackStoreTests, RecordSpansAreChecked) {
    DualFixture fixture;
    fixture.build();
    TrackEvents &track = fixture.flat.track(fixture.id);
    NoteRecord note = track.notes[0];
    note.pitch_curve.count = static_cast<std::uint32_t>(track.curve_points.size()) + 1;
    EXPECT_THROW(std::ignore = track.pitch_curve(note), StoreLimit);
    note.pitch_curve = {std::numeric_limits<std::uint32_t>::max(), 1};
    EXPECT_THROW(std::ignore = track.pitch_curve(note), StoreLimit);
}

TEST(FlatPlaybackStoreTests, ProjectionsAgreeOnDualEmission) {
    DualFixture fixture;
    fixture.build();
    const std::string legacy = fixture.legacy_projection();
    EXPECT_EQ(DualFixture::flat_projection(fixture.flat), legacy);
    // The projection covers cold, empty and appended groups
    EXPECT_NE(legacy.find("preset 3 16:"), std::string::npos);
    EXPECT_NE(legacy.find("group 125000 0\n"), std::string::npos);
    EXPECT_NE(legacy.find("group 250000 0\n"), std::string::npos);
    EXPECT_NE(legacy.find("group 0 6\n"), std::string::npos);
}

// Negative control: each corrupted flat record must make dump B fail
TEST(FlatPlaybackStoreTests, CorruptedFlatRecordFailsProjection) {
    DualFixture fixture;
    fixture.build();
    const std::string legacy = fixture.legacy_projection();

    const std::vector<std::pair<const char *, std::function<void(TrackEvents &)>>> corruptions{
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
        EXPECT_NE(DualFixture::flat_projection(copy), legacy) << label;
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
    // Two groups whose notes start together: the legacy map order puts group 0's note first
    const auto note_at = [](const pitch_level_t pitch) { return make_note(100, pitch, false); };

    for (const std::uint32_t count : {1u, 6000u}) {
        PlaybackData legacy;
        legacy.setup_data = PlaybackSetupData(String(u"piano"), SoundCategory::KEYBOARDS, StringList{});
        TrackEvents emitted;
        emitted.setup = legacy.setup_data;
        FreezeProgress progress;
        // Emission in descending group order; each group holds one note, all at the same actual time
        for (std::uint32_t i = 0; i < count; ++i) {
            const timestamp_t group = static_cast<timestamp_t>(count - i) * 10;
            const pitch_level_t pitch = static_cast<pitch_level_t>(2000 + (i % 24) * pitch_level_step);
            PlaybackEventList list{note_at(pitch), note_at(pitch + pitch_level_step)};
            legacy.origin_events[group].insert(legacy.origin_events[group].end(), list.begin(), list.end());
            freeze_events(emitted, group, list, progress);
        }
        const auto expected = lower(freeze_playback_data(legacy));
        const auto actual = lower(emitted);
        ASSERT_FALSE(expected.empty());
        EXPECT_EQ(actual, expected) << count;
    }
}

struct FlatPlaybackModelTests : ::testing::Test {
protected:
    void SetUp() override {
        ArticulationPatternSegment segment(ArrangementPattern(hundred_percent, 0),
                                           PitchPattern(ArticulationMap::expected_size, ten_percent, 0),
                                           ExpressionPattern(ArticulationMap::expected_size, ten_percent, 0));
        ArticulationPattern pattern;
        pattern.emplace(0, std::move(segment));
        profile_ = std::make_shared<ArticulationsProfile>();
        for (const ArticulationType type : {ArticulationType::STANDARD, ArticulationType::STACCATO, ArticulationType::TENUTO,
                                            ArticulationType::ACCENT, ArticulationType::TREMOLO32ND, ArticulationType::PIZZICATO}) {
            profile_->set_pattern(type, pattern);
        }
        repository_ = std::make_shared<NiceMock<ArticulationProfilesRepositoryMock>>();
        ON_CALL(*repository_, default_profile(_)).WillByDefault(Return(profile_));
    }

    // The legacy oracle's dump B projection of a loaded model
    static std::string legacy_projection(PlaybackModel &model) {
        const InstrumentTrackIdSet ids = model.existing_track_id_set();
        std::vector<InstrumentTrackId> ordered(ids.cbegin(), ids.cend());
        std::sort(ordered.begin(), ordered.end());
        std::vector<music::test::LegacyProjectionTrack> tracks;
        for (const InstrumentTrackId &id : ordered) {
            tracks.push_back({id.part_id.to_uint64(), id.instrument_id, &model.resolve_track_playback_data(id)});
        }
        music::test::LegacyProjection legacy;
        legacy.score(tracks);
        return legacy.out;
    }

    // Builds the export's flat store and, separately, loads the legacy oracle; returns both dump B projections
    std::pair<std::string, std::string> project_built(MasterScore *score, bool metronome, PlaybackScore &flat) {
        score->init_automation();
        PlaybackScoreBuilder builder{repository_.get()};
        builder.metronome_enabled = metronome;
        flat = builder.build(score);
        PlaybackModel model(repository_.get());
        model.set_is_metronome_enabled(metronome);
        model.load(score);
        return {legacy_projection(model), DualFixture::flat_projection(flat)};
    }

#ifdef ENGRAVE_PLAYBACK_HARNESS
    // Loads with dual execution (test only) and returns the two dump B projections
    std::pair<std::string, std::string> project(MasterScore *score, bool metronome, PlaybackScore &flat) {
        score->init_automation();
        PlaybackModel model(repository_.get());
        model.set_is_metronome_enabled(metronome);
        model.load(score, {}, &flat);
        return {legacy_projection(model), DualFixture::flat_projection(flat)};
    }
#endif

    ArticulationsProfilePtr profile_;
    std::shared_ptr<NiceMock<ArticulationProfilesRepositoryMock>> repository_;
};

namespace {
const std::vector<std::pair<String, bool>> &model_fixtures() {
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

std::uint64_t record_count(const PlaybackScore &flat) {
    std::uint64_t records = 0;
    for (const auto &[id, track] : flat.tracks) {
        records += track.notes.size() + track.articulations.size() + track.curve_points.size() + track.cold.size() +
                   track.empty_groups.size() + track.dynamic_points.size();
    }
    return records;
}
} // namespace

// Step 4c: the export's builder walks the score once into the flat store, with no legacy model; the
// legacy model loaded separately is the oracle
TEST_F(FlatPlaybackModelTests, BuilderMatchesTheLegacyOracle) {
    for (const auto &[path, metronome] : model_fixtures()) {
        MasterScore *score = ScoreRW::read_score(path);
        ASSERT_TRUE(score) << path.to_std_string();
        PlaybackScore flat;
        const auto [legacy, projected] = project_built(score, metronome, flat);
        std::size_t notes = 0;
        for (const auto &[id, track] : flat.tracks) notes += track.notes.size();
        EXPECT_GT(notes, 0u) << path.to_std_string();
        EXPECT_NE(legacy.find("\nnote "), std::string::npos) << path.to_std_string();
        EXPECT_EQ(projected, legacy) << path.to_std_string();
        EXPECT_FALSE(flat.progress.checkpoint) << path.to_std_string();
        delete score;
    }
}

TEST_F(FlatPlaybackModelTests, BuilderCarriesDistinctPresets) {
    const String fixture = String::from_utf8(ENGRAVING_TESTS_DATA_ROOT) + u"/../../../tests/data/playback_dod/cold_preset.mscx";
    MasterScore *score = ScoreRW::read_score(fixture, true);
    ASSERT_TRUE(score);
    PlaybackScore flat;
    const auto [legacy, projected] = project_built(score, false, flat);
    EXPECT_EQ(projected, legacy);
    EXPECT_NE(legacy.find("preset "), std::string::npos);
    delete score;
}

// The builder polls the walk exactly as the legacy model does, plus one poll per FreezeProgress::k_stride records
TEST_F(FlatPlaybackModelTests, BuilderPollsTheWalkAndTheFreeze) {
    MasterScore *score = ScoreRW::read_score(u"playback/playbackmodel_data/spanners/spanners.mscx");
    ASSERT_TRUE(score);
    score->init_automation();
    std::uint64_t legacy = 0, built = 0;
    {
        PlaybackModel model(repository_.get());
        model.set_is_metronome_enabled(false);
        model.load(score, [&legacy] { ++legacy; });
    }
    PlaybackScoreBuilder builder{repository_.get()};
    const PlaybackScore flat = builder.build(score, [&built] { ++built; });
    EXPECT_FALSE(flat.progress.checkpoint);
    const std::uint64_t records = record_count(flat);
    ASSERT_GT(records, 0u);
    EXPECT_EQ(built - legacy, records / FreezeProgress::k_stride);

    // A throwing checkpoint aborts the build
    struct Cancelled {};

    std::uint64_t polls = 0;
    EXPECT_THROW(std::ignore = builder.build(score,
                                             [&polls] {
                                                 if (++polls == 50) throw Cancelled{};
                                             }),
                 Cancelled);
    delete score;
}

TEST_F(FlatPlaybackModelTests, BuilderOfAnEmptyOrNullScoreIsEmpty) {
    const PlaybackScoreBuilder builder{repository_.get()};
    EXPECT_TRUE(builder.build(nullptr).tracks.empty());
}

#ifdef ENGRAVE_PLAYBACK_HARNESS
TEST_F(FlatPlaybackModelTests, DualExecutionMatchesTheLegacyModel) {
    for (const auto &[path, metronome] : model_fixtures()) {
        MasterScore *score = ScoreRW::read_score(path);
        ASSERT_TRUE(score) << path.to_std_string();
        PlaybackScore flat;
        const auto [legacy, projected] = project(score, metronome, flat);
        std::size_t notes = 0;
        for (const auto &[id, track] : flat.tracks) notes += track.notes.size();
        EXPECT_GT(notes, 0u) << path.to_std_string();
        EXPECT_NE(legacy.find("\nnote "), std::string::npos) << path.to_std_string();
        EXPECT_EQ(projected, legacy) << path.to_std_string();
        delete score;
    }
}

TEST_F(FlatPlaybackModelTests, DualExecutionCarriesDistinctPresets) {
    const String fixture = String::from_utf8(ENGRAVING_TESTS_DATA_ROOT) + u"/../../../tests/data/playback_dod/cold_preset.mscx";
    MasterScore *score = ScoreRW::read_score(fixture, true);
    ASSERT_TRUE(score);
    PlaybackScore flat;
    const auto [legacy, projected] = project(score, false, flat);
    EXPECT_EQ(projected, legacy);

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
    // At tick 0 the cold events precede the chord's notes, as in the legacy group
    ASSERT_FALSE(track->notes.empty());
    EXPECT_GT(track->notes.front().seq, track->cold.front().seq);
    delete score;
}

// The freeze loops poll through the model's checkpoint, and the hook does not outlive load
TEST_F(FlatPlaybackModelTests, DualExecutionPollsInsideTheFreeze) {
    const String path = u"playback/playbackmodel_data/spanners/spanners.mscx";
    const auto polls = [&](PlaybackScore *flat) {
        MasterScore *score = ScoreRW::read_score(path);
        score->init_automation();
        std::uint64_t count = 0;
        {
            PlaybackModel model(repository_.get());
            model.set_is_metronome_enabled(false);
            model.load(score, [&count] { ++count; }, flat);
        }
        delete score;
        return count;
    };

    const std::uint64_t legacy = polls(nullptr);
    PlaybackScore flat;
    flat.progress.pending = FreezeProgress::k_stride - 1; // the first frozen record reaches the stride
    const std::uint64_t dual = polls(&flat);
    EXPECT_FALSE(flat.progress.checkpoint);

    const std::uint64_t records = record_count(flat);
    ASSERT_GT(records, 0u);
    EXPECT_EQ(dual - legacy, (FreezeProgress::k_stride - 1 + records) / FreezeProgress::k_stride);
}

TEST_F(FlatPlaybackModelTests, DualExecutionNeedsAFreshModel) {
    MasterScore *score = ScoreRW::read_score(u"playback/playbackmodel_data/repeat_range/repeat_range.mscx");
    ASSERT_TRUE(score);
    PlaybackModel model(repository_.get());
    model.load(score);
    PlaybackScore flat;
    EXPECT_THROW(model.load(score, {}, &flat), std::logic_error);
    delete score;
}
#endif

// The renderer emits an empty list's key (step-4a review finding 2)
TEST_F(FlatPlaybackModelTests, RendererEmitsEmptyGroupsAndAppendsToExistingKeys) {
    MasterScore *score = ScoreRW::read_score(u"playback/playbackmodel_data/repeat_range/repeat_range.mscx");
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
    Chord *empty_chord = Factory::create_chord(segment);
    empty_chord->set_track(chord->track());
    ASSERT_TRUE(empty_chord->notes().empty());

    PlaybackContextPtr ctx = std::make_shared<PlaybackContext>(score);
    const PlaybackEventsRenderer renderer;
    const dynamic_level_t level = dynamic_level_from_type(mpe::DynamicType::MF);

    // Through the legacy wrapper, starting from an empty map
    PlaybackEventsMap legacy;
    renderer.render(empty_chord, 1000, 2000, level, ctx, profile_, legacy);
    ASSERT_EQ(legacy.size(), 1u);
    ASSERT_TRUE(legacy.contains(1000));
    EXPECT_TRUE(legacy.at(1000).empty());

    // Appending to an existing timestamp keeps the earlier events first
    renderer.render(chord, 1000, 2000, level, ctx, profile_, legacy);
    renderer.render(chord, 1000, 3000, level, ctx, profile_, legacy);
    ASSERT_EQ(legacy.at(1000).size(), 2 * chord->notes().size());
    EXPECT_EQ(std::get<mpe::NoteEvent>(legacy.at(1000).front()).arrangement_ctx().nominal_duration, 2000);
    EXPECT_EQ(std::get<mpe::NoteEvent>(legacy.at(1000).back()).arrangement_ctx().nominal_duration, 3000);

    // The flat sink records the empty group, and the projections agree
    PlaybackData data;
    PlaybackScore flat;
    const InstrumentTrackId id{ID(1), String(u"test")};
    BothSink dual{{data.origin_events}, {flat.track(id), flat.progress}};
    renderer.render(empty_chord, 1000, 2000, level, ctx, profile_, dual);
    renderer.render(chord, 1000, 2000, level, ctx, profile_, dual);
    ASSERT_EQ(flat.tracks.front().second.empty_groups.size(), 1u);
    const music::test::LegacyProjectionTrack track{id.part_id.to_uint64(), id.instrument_id, &data};
    music::test::LegacyProjection projection;
    projection.score(std::span{&track, 1});
    EXPECT_EQ(DualFixture::flat_projection(flat), projection.out);

    // A sink defined in this unit only
    RecordingSink recording;
    renderer.render(empty_chord, 1000, 2000, level, ctx, profile_, recording);
    renderer.render(chord, 1500, 2000, level, ctx, profile_, recording);
    ASSERT_EQ(recording.calls.size(), 2u);
    EXPECT_EQ(recording.calls[0], std::make_pair(timestamp_t{1000}, std::size_t{0}));
    EXPECT_EQ(recording.calls[1], std::make_pair(timestamp_t{1500}, chord->notes().size()));

    delete empty_chord;
    delete score;
}

namespace mu::engraving {
struct PlaybackContextReleaseTestAccess {
    static void populate(PlaybackContext &context) {
        auto &syllables = context.syllables_by_track_[0];
        for (int i = 0; i < 200000; ++i) {
            syllables.emplace(i, SyllableEvent{String::from_utf8(std::to_string(i))});
        }
    }

    static std::size_t size(const PlaybackContext &context) {
        std::size_t result = 0;
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
        std::size_t previous = 400000, polls = 0;

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
            EXPECT_GE(polls, 98u);
        }
        EXPECT_TRUE(score.tracks.empty());
        EXPECT_FALSE(score.context);
        EXPECT_TRUE(owner.expired());
        EXPECT_LT((std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()), 100.0);
    }
}
