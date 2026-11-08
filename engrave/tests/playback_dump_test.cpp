// SPDX-License-Identifier: GPL-3.0-only
#include "playback_dump.h"

#include <bit>
#include <sstream>
#include <stdexcept>

int main() {
    using namespace muse::mpe;
    PlaybackData data;
    data.origin_events[17]; // Empty keys are part of full identity.
    data.origin_events[18].emplace_back(std::monostate{});
    ControllerChangeEvent controller;
    controller.type = ControllerChangeEvent::PITCH_BEND;
    controller.layer_idx = 37;
    controller.val = std::bit_cast<float>(std::uint32_t{0x80000000});
    data.origin_events[18].emplace_back(controller);
    TextArticulationEvent text;
    text.text = muse::String(u"a\nb");
    data.origin_events[18].emplace_back(text);
    data.origin_events[18].emplace_back(SoundPresetChangeEvent{});
    data.origin_events[18].emplace_back(SyllableEvent{});
    data.origin_events[18].emplace_back(NoteEvent{});
    std::ostringstream out;
    music::test::LegacyDump{out}.track(1, muse::String(u"test"), data);
    const auto value = out.str();
    for (const auto *expected : {"group 17 0", "monostate", "controller 2 37 f32:80000000", "0061000a0062", "preset", "averages"}) {
        if (value.find(expected) == std::string::npos) throw std::runtime_error(expected);
    }
}
