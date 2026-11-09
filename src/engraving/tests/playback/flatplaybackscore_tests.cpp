// SPDX-License-Identifier: GPL-3.0-only
// Step 4b of the playback DoD refactor: the flat MPE store, its freeze rules, dual execution through
// the playback model and the dump B extractors, plus the renderer's sink contract.

#include <algorithm>
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
        DualEventsSink sink{{legacy.origin_events}, {flat.track(id)}};
        sink(key, std::move(list));
    }

    template <class Event>
    void emit_context(const timestamp_t key, const std::vector<Event> &events) {
        PlaybackEventList &list = legacy.origin_events[key];
        list.insert(list.end(), events.begin(), events.end());
        freeze_cold_events(flat.track(id), key, std::span{events});
    }

    void build() {
        legacy.setup_data = PlaybackSetupData(String(u"violin"), SoundCategory::STRINGS, StringList{String(u"orchestral")});
        legacy.setup_data.score_id = "score-1";
        flat.track(id).setup = legacy.setup_data;
        legacy.dynamics = make_dynamics();
        freeze_dynamics(flat.track(id), legacy.dynamics);

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
    EXPECT_EQ(pitch[1].value(), -1200);
    const auto expression = track.expression_curve(note);
    ASSERT_EQ(expression.size(), 2u);
    EXPECT_EQ(expression[0].value(), 7000);

    const auto &preset = std::get<SoundPresetChangeEvent>(track.cold[0].event);
    EXPECT_EQ(preset.code, String(u"Engrave.Preset.Δ"));
    EXPECT_EQ(preset.layer_idx, 3u);
    EXPECT_TRUE(std::holds_alternative<SyllableEvent>(track.cold[2].event));

    ASSERT_EQ(track.dynamic_layers.size(), 2u);
    EXPECT_EQ(track.dynamic_layers[0].layer, 1u);
    EXPECT_EQ(track.layer_points(track.dynamic_layers[1]).size(), 2u);
    EXPECT_EQ(track.setup.id, String(u"violin"));

    // Dynamics are assigned, not appended
    freeze_dynamics(fixture.flat.track(fixture.id), make_dynamics());
    EXPECT_EQ(track.dynamic_points.size(), 4u);
}

TEST(FlatPlaybackStoreTests, FreezeRejectsEventsWithoutProducerAndKeyMismatch) {
    TrackEvents track;
    const PlaybackEventList monostate{PlaybackEvent{}};
    EXPECT_THROW(freeze_events(track, 0, monostate), std::logic_error);
    const PlaybackEventList controller{ControllerChangeEvent{}};
    EXPECT_THROW(freeze_events(track, 0, controller), std::logic_error);

    mpe::NoteEvent note = make_note(0, 6500, false);
    ExpressionContext expression = note.expression_ctx();
    expression.articulations.emplace(ArticulationType::ACCENT, applied(ArticulationType::MARCATO, 0, 1));
    ArrangementContext arrangement = note.arrangement_ctx();
    PitchContext pitch = note.pitch_ctx();
    const PlaybackEventList mismatched{mpe::NoteEvent(std::move(arrangement), std::move(pitch), std::move(expression))};
    EXPECT_THROW(freeze_events(track, 0, mismatched), std::logic_error);
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
        {"curve value", [](TrackEvents &t) { t.curve_points[1].value32 += 1; }},
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

    // Loads with dual execution and returns the two dump B projections
    std::pair<std::string, std::string> project(MasterScore *score, bool metronome, PlaybackScore &flat) {
        score->init_automation();
        PlaybackModel model(repository_.get());
        model.set_is_metronome_enabled(metronome);
        model.load(score, {}, &flat);

        const InstrumentTrackIdSet ids = model.existing_track_id_set();
        std::vector<InstrumentTrackId> ordered(ids.cbegin(), ids.cend());
        std::sort(ordered.begin(), ordered.end());
        std::vector<music::test::LegacyProjectionTrack> tracks;
        for (const InstrumentTrackId &id : ordered) {
            tracks.push_back({id.part_id.to_uint64(), id.instrument_id, &model.resolve_track_playback_data(id)});
        }
        music::test::LegacyProjection legacy;
        legacy.score(tracks);
        return {legacy.out, DualFixture::flat_projection(flat)};
    }

    ArticulationsProfilePtr profile_;
    std::shared_ptr<NiceMock<ArticulationProfilesRepositoryMock>> repository_;
};

TEST_F(FlatPlaybackModelTests, DualExecutionMatchesTheLegacyModel) {
    const std::vector<std::pair<String, bool>> fixtures{
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
    for (const auto &[path, metronome] : fixtures) {
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

TEST_F(FlatPlaybackModelTests, DualExecutionNeedsAFreshModel) {
    MasterScore *score = ScoreRW::read_score(u"playback/playbackmodel_data/repeat_range/repeat_range.mscx");
    ASSERT_TRUE(score);
    PlaybackModel model(repository_.get());
    model.load(score);
    PlaybackScore flat;
    EXPECT_THROW(model.load(score, {}, &flat), std::logic_error);
    delete score;
}

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
    DualEventsSink dual{{data.origin_events}, {flat.track(id)}};
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
