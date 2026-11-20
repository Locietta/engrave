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
