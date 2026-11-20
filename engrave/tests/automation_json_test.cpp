// SPDX-License-Identifier: GPL-3.0-only

#include <cmath>
#include <limits>
#include <memory>
#include <string_view>

#include <gtest/gtest.h>

#include "engrave/tests/handles_test_fixture.h"
#include "engraving/automation/automationdata.h"
#include "engraving/automation/internal/automationrw.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/playback/playbackcontext.h"
#include "engraving/types/constants.h"

TEST(AutomationJson, ReadsLegacyFieldsAndWritesSixDecimalPlaces) {
    using namespace mu::engraving;
    constexpr std::string_view source = R"([{"type":"Tempo","points":[
        {"tick":2.5,"outValue":0.123456789,"inValueKind":"FromPrevious"},
        {"tick":480,"outValue":"wrong type","inValue":0.75,"generated":"yes"},
        {"tick":480,"outValue":1}
    ]}])";
    AutomationData data;
    AutomationRW::read(data, muse::ByteArray(source.data(), source.size()));
    const auto &curve = data.curve(tempo_key);
    ASSERT_EQ(curve.size(), 2u);
    ASSERT_TRUE(curve.contains(3));
    EXPECT_DOUBLE_EQ(curve.at(3).value.out_value.raw(), 0.123456789);
    EXPECT_TRUE(std::holds_alternative<AutomationPoint::ArrivalFromPrevious>(curve.at(3).value.in_value));
    ASSERT_TRUE(curve.contains(480));
    EXPECT_DOUBLE_EQ(curve.at(480).value.out_value.raw(), 0.0);
    EXPECT_TRUE(curve.at(480).generated);
    const auto *arrival = std::get_if<AutomationPoint::ExplicitArrival>(&curve.at(480).value.in_value);
    ASSERT_NE(arrival, nullptr);
    EXPECT_DOUBLE_EQ(arrival->value.raw(), 0.75);
    EXPECT_TRUE(arrival->ease.is_none());

    AutomationRW::read(data, muse::ByteArray("[{", 2));
    EXPECT_EQ(data.curve(tempo_key).size(), 2u);
    const auto original_curves = data.curves();
    AutomationRW::read(data, muse::ByteArray("[] garbage", 10));
    EXPECT_EQ(data.curves(), original_curves);
    const auto bytes = AutomationRW::write(data, false);
    EXPECT_EQ(std::string_view(bytes.const_char(), bytes.size()), R"([
  {
    "points": [
      {
        "inValueKind": "FromPrevious",
        "outValue": 0.123457,
        "tick": 3
      }
    ],
    "type": "Tempo"
  }
]
)");
}

TEST(AutomationJson, SkipsMalformedCurvesAndTicks) {
    using namespace mu::engraving;
    constexpr std::string_view source = R"([
        {"type":"Tempo"},
        {"type":"Tempo","points":null},
        {"type":"Tempo","points":{}},
        {"type":"Tempo","partId":"bad","instrumentId":"flute","points":[{"tick":0}]},
        {"type":"Tempo","staffId":"18446744073709551616","points":[{"tick":0}]},
        {"type":"Dynamics","staffId":"17","voiceId":null,"points":[{"tick":0,"outValue":0.9}]},
        {"type":"Dynamics","staffId":"17","voiceId":-1,"points":[{"tick":10,"outValue":0.8}]},
        {"type":"Dynamics","staffId":"17","voiceId":4,"points":[{"tick":20,"outValue":0.7}]},
        {"type":"Dynamics","staffId":"17","voiceId":1.5,"points":[{"tick":25,"outValue":0.65}]},
        {"type":"Dynamics","staffId":"17","voiceId":"bad","points":[{"tick":30,"outValue":0.6}]},
        {"type":"Dynamics","staffId":"17","voiceId":2147483648,"points":[{"tick":40,"outValue":0.5}]},
        {"type":"Dynamics","staffId":"17","points":[{"tick":100,"outValue":0.25}]},
        {"type":"Tempo","points":[null,{},{"tick":-0.25,"outValue":4},{"tick":-2.5,"outValue":5},{"tick":-2147483648,"outValue":6},
            {"tick":2147483648,"outValue":99},{"tick":12,"outValue":2,"inValue":1,"ease":null}]}
    ])";
    AutomationData data;
    EXPECT_NO_THROW(AutomationRW::read(data, muse::ByteArray(source.data(), source.size())));
    const auto &curve = data.curve(tempo_key);
    ASSERT_EQ(curve.size(), 1u);
    ASSERT_TRUE(curve.contains(12));
    const auto *arrival = std::get_if<AutomationPoint::ExplicitArrival>(&curve.at(12).value.in_value);
    ASSERT_NE(arrival, nullptr);
    EXPECT_DOUBLE_EQ(arrival->value.raw(), 1);
    EXPECT_TRUE(arrival->ease.is_none());

    const auto staff_wide = AutomationCurveKey::staff(AutomationType::DYNAMICS, muse::ID(17));
    const auto &staff_curve = data.curve(staff_wide);
    ASSERT_EQ(staff_curve.size(), 1u);
    ASSERT_TRUE(staff_curve.contains(100));
    EXPECT_DOUBLE_EQ(staff_curve.at(100).value.out_value.raw(), 0.25);
    EXPECT_EQ(data.curves().size(), 2u);
}

TEST(AutomationJson, WritesScopedCurvesAndPreservesWideIntegers) {
    using namespace mu::engraving;
    constexpr std::string_view source = R"([
        {"type":"Volume","partId":"4294967296","instrumentId":"flute/\"\n\u007f","points":[
            {"tick":0,"outValue":2147483648.0,"inValue":2147483647,"ease":[{"t":0.25,"value":-2147483648}]}
        ]},
        {"type":"Dynamics","staffId":"17","voiceId":2,"points":[
            {"tick":0,"outValue":-2147483648,"inValueKind":"FromPrevious"},
            {"tick":2147483647,"outValue":2147483647,"inValue":0}
        ]}
    ])";
    AutomationData data;
    AutomationRW::read(data, muse::ByteArray(source.data(), source.size()));
    const auto instrument =
        AutomationCurveKey::instrument(AutomationType::VOLUME, {muse::ID(4294967296ULL), muse::String(u"flute/\"\n\x7f")});
    const auto staff = AutomationCurveKey::staff(AutomationType::DYNAMICS, muse::ID(17), 2);
    const auto &volume = data.curve(instrument);
    ASSERT_TRUE(volume.contains(0));
    EXPECT_DOUBLE_EQ(volume.at(0).value.out_value.raw(), 2147483648.0);
    const auto *arrival = std::get_if<AutomationPoint::ExplicitArrival>(&volume.at(0).value.in_value);
    ASSERT_NE(arrival, nullptr);
    EXPECT_DOUBLE_EQ(arrival->value.raw(), 2147483647.0);
    EXPECT_DOUBLE_EQ(arrival->ease.t.raw(), 0.25);
    EXPECT_DOUBLE_EQ(arrival->ease.value.raw(), -2147483648.0);
    ASSERT_EQ(data.curve(staff).size(), 2u);
    EXPECT_TRUE(data.curve(staff).contains(0));
    EXPECT_TRUE(data.curve(staff).contains(2147483647));

    const auto bytes = AutomationRW::write(data, false);
    EXPECT_EQ(std::string_view(bytes.const_char(), bytes.size()), R"([
  {
    "points": [
      {
        "inValueKind": "FromPrevious",
        "outValue": -2147483648,
        "tick": 0
      },
      {
        "inValue": 0,
        "outValue": 2147483647,
        "tick": 2147483647
      }
    ],
    "staffId": "17",
    "type": "Dynamics",
    "voiceId": 2
  },
  {
    "instrumentId": "flute\/\"\n\u007f",
    "partId": "4294967296",
    "points": [
      {
        "ease": [
          {
            "t": 0.25,
            "value": -2147483648
          }
        ],
        "inValue": 2147483647,
        "outValue": 2147483648,
        "tick": 0
      }
    ],
    "type": "Volume"
  }
]
)");
    AutomationData reloaded;
    AutomationRW::read(reloaded, bytes);
    EXPECT_EQ(reloaded.curves(), data.curves());
}

TEST(AutomationJson, BoundaryTicksAreOverflowSafeInTimelineAndPlayback) {
    using namespace mu::engraving;
    constexpr int k_min = std::numeric_limits<int>::min();
    constexpr int k_max = std::numeric_limits<int>::max();

    AutomationCurve tempo_curve;
    tempo_curve.emplace(k_min, AutomationPoint{{.in_value = AutomationPoint::ArrivalFromPrevious{}, .out_value = muse::real_t(0.5)}});
    tempo_curve.emplace(k_max,
                        AutomationPoint{{.in_value = AutomationPoint::ExplicitArrival{muse::real_t(0.8)}, .out_value = muse::real_t(0.5)}});

    TempoTimeline timeline;
    timeline.rebuild(tempo_curve, {});
    const double t_start = timeline.utick2utime(k_min);
    const double t_mid = timeline.utick2utime(0);
    const double t_end = timeline.utick2utime(k_max);
    EXPECT_TRUE(std::isfinite(t_start));
    EXPECT_TRUE(std::isfinite(t_mid));
    EXPECT_TRUE(std::isfinite(t_end));
    EXPECT_LT(t_start, t_mid);
    EXPECT_LT(t_mid, t_end);
    EXPECT_EQ(timeline.utime2utick(t_start), k_min);
    EXPECT_EQ(timeline.utime2utick(t_mid), 0);
    EXPECT_EQ(timeline.utime2utick(t_end), k_max);

    // Exercise an inverse conversion spanning more than INT_MAX ticks from a single anchor.
    timeline.rebuild(TempoValues{{k_min, 2.0}, {k_max, 2.0}}, {});
    EXPECT_EQ(timeline.utime2utick(timeline.utick2utime(0)), 0);
    EXPECT_EQ(timeline.utime2utick(timeline.utick2utime(k_max - 1)), k_max - 1);
    EXPECT_EQ(timeline.utime2utick(-std::numeric_limits<double>::infinity()), k_min);
    EXPECT_EQ(timeline.utime2utick(std::numeric_limits<double>::infinity()), k_max);

    constexpr std::string_view xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time></attributes>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
</measure></part>
</score-partwise>)";
    auto opened = handles_test::g_engine->open(handles_test::as_bytes(xml), {.format = engrave::Format::MUSIC_XML});
    ASSERT_TRUE(opened);
    auto &master = handles_test::master_of(**opened);
    ASSERT_FALSE(master.staves().empty());

    const auto staff_key = AutomationCurveKey::staff(AutomationType::DYNAMICS, master.staff(0)->id());
    AutomationCurve dynamics_curve;
    dynamics_curve.emplace(k_min, AutomationPoint{{.in_value = AutomationPoint::ArrivalFromPrevious{}, .out_value = muse::real_t(0.2)}});
    dynamics_curve.emplace(
        k_max, AutomationPoint{{.in_value = AutomationPoint::ExplicitArrival{muse::real_t(0.8)}, .out_value = muse::real_t(0.8)}});
    AutomationCurveMap curves;
    curves.emplace(staff_key, std::move(dynamics_curve));
    auto automation = std::make_shared<AutomationData>();
    automation->set_curves(std::move(curves));
    master.set_automation_data(std::move(automation));

    PlaybackContext context(&master);
    const auto midpoint = context.appliable_dynamic_level(0, 0);
    EXPECT_NEAR(midpoint, static_cast<muse::mpe::dynamic_level_t>(0.5 * muse::mpe::max_dynamic_level), 1);
}
