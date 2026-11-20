// SPDX-License-Identifier: GPL-3.0-only

#include <string_view>

#include <gtest/gtest.h>

#include "engraving/automation/automationdata.h"
#include "engraving/automation/internal/automationrw.h"

TEST(AutomationJson, ReadsLegacyFieldsAndWritesSixDecimalPlaces) {
    using namespace mu::engraving;
    constexpr std::string_view source = R"([{"type":"Tempo","points":[
        {"tick":-2.5,"outValue":0.123456789,"inValueKind":"FromPrevious"},
        {"tick":480,"outValue":"wrong type","inValue":0.75,"generated":"yes"},
        {"tick":480,"outValue":1}
    ]}])";
    AutomationData data;
    AutomationRW::read(data, muse::ByteArray(source.data(), source.size()));
    const auto &curve = data.curve(tempo_key);
    ASSERT_EQ(curve.size(), 2u);
    ASSERT_TRUE(curve.contains(-3));
    EXPECT_DOUBLE_EQ(curve.at(-3).value.out_value.raw(), 0.123456789);
    EXPECT_TRUE(std::holds_alternative<AutomationPoint::ArrivalFromPrevious>(curve.at(-3).value.in_value));
    ASSERT_TRUE(curve.contains(480));
    EXPECT_DOUBLE_EQ(curve.at(480).value.out_value.raw(), 0.0);
    EXPECT_TRUE(curve.at(480).generated);
    const auto *arrival = std::get_if<AutomationPoint::ExplicitArrival>(&curve.at(480).value.in_value);
    ASSERT_NE(arrival, nullptr);
    EXPECT_DOUBLE_EQ(arrival->value.raw(), 0.75);
    EXPECT_TRUE(arrival->ease.is_none());

    AutomationRW::read(data, muse::ByteArray("[{", 2));
    EXPECT_EQ(data.curve(tempo_key).size(), 2u);
    const auto bytes = AutomationRW::write(data, false);
    EXPECT_EQ(std::string_view(bytes.const_char(), bytes.size()), R"([
  {
    "points": [
      {
        "inValueKind": "FromPrevious",
        "outValue": 0.123457,
        "tick": -3
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
        {"type":"Tempo","points":[null,{},{"tick":2147483648,"outValue":99},
            {"tick":12,"outValue":2,"inValue":1,"ease":null}]}
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
}

TEST(AutomationJson, WritesScopedCurvesAndPreservesWideIntegers) {
    using namespace mu::engraving;
    constexpr std::string_view source = R"([
        {"type":"Volume","partId":"4294967296","instrumentId":"flute/\"\n\u007f","points":[
            {"tick":0,"outValue":2147483648.0,"inValue":2147483647,"ease":[{"t":0.25,"value":-2147483648}]}
        ]},
        {"type":"Dynamics","staffId":"17","voiceId":2,"points":[
            {"tick":-2147483648,"outValue":-2147483648,"inValueKind":"FromPrevious"},
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
    EXPECT_TRUE(data.curve(staff).contains(-2147483647 - 1));
    EXPECT_TRUE(data.curve(staff).contains(2147483647));

    const auto bytes = AutomationRW::write(data, false);
    EXPECT_EQ(std::string_view(bytes.const_char(), bytes.size()), R"([
  {
    "points": [
      {
        "inValueKind": "FromPrevious",
        "outValue": -2147483648,
        "tick": -2147483648
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
