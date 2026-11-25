// SPDX-License-Identifier: GPL-3.0-only
#include "playback_dump.h"

#include <array>
#include <sstream>
#include <stdexcept>

int main() {
    using namespace muse::mpe;
    TrackEvents track;
    auto dump_note = [](const TrackEvents &events, const NoteRecord &note, const ArticulationMap &articulations) {
        std::ostringstream out;
        music::test::LegacyDump{out}.note(events, note, articulations);
        return out.str();
    };
    FreezeProgress progress{.dump_note = dump_note};
    TextArticulationEvent text;
    text.text = muse::String(u"a\nb");
    freeze_cold_events(track, 18, std::span{&text, 1}, progress);
    freeze_cold_events(track, 18, std::array{SoundPresetChangeEvent{}}, progress);
    freeze_cold_events(track, 18, std::array{SyllableEvent{}}, progress);
    emit_note(track, 18, {20, 1000, 0, 0, 6000, 5000, 2.0}, {}, {}, progress);
    emit_empty_group(track, 17, progress);

    std::ostringstream out;
    music::test::FlatDump{out}.track(1, muse::String(u"test"), track);
    const auto value = out.str();
    if (!value.contains("groups 2\ngroup 17 0\ngroup 18 4\nevent 2\ntext 0 0 3:0061000a0062\nevent 3\npreset 0 0:\n"
                        "event 4\ntext 0 0 0:\nevent 1\narrangement 20 20 1000 1000 0 0 f64:4000000000000000\n"
                        "pitch 6000\n curve 0\nexpression 5000 0\n curve 0\narticulations 0\n"
                        "averages 10000 0 0 0 0\n curve 0\n curve 0\n")) {
        throw std::runtime_error(value);
    }
}
