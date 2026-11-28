// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core_test_env.h"
#include "engrave/src/api/state.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/playback/flatplaybackscore.h"
#include "global/io/buffer.h"
#include "importexport/midi/internal/midishared/midifile.h"
#include <engrave/engrave.h>

namespace {

using namespace mu::engraving;
using namespace mu::iex::midi;

engrave::Engine &engine() { return *engrave::test::core_env().engine; }

std::unique_ptr<engrave::Document> open_mscx(std::string_view xml) {
    const std::span<const std::byte> bytes(reinterpret_cast<const std::byte *>(xml.data()), xml.size());
    auto opened = engine().open(bytes, {.format = engrave::Format::MSCX});
    EXPECT_TRUE(opened) << (opened ? std::string{} : opened.error().message);
    return opened ? std::move(*opened) : nullptr;
}

MidiFile parse_smf(const std::vector<std::byte> &bytes) {
    muse::ByteArray raw(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    muse::io::Buffer buf(&raw);
    EXPECT_TRUE(buf.open(muse::io::IODevice::READ_ONLY));
    MidiFile mf;
    EXPECT_TRUE(mf.read(&buf));
    return mf;
}

int track_port(const MidiTrack &track) {
    for (const auto &[tick, ev] : track.events()) {
        if (ev.type() == ME_META && ev.meta_type() == META_PORT_CHANGE && ev.len() >= 1) {
            return ev.edata()[0];
        }
    }
    return 0;
}

int pitch_bend_value(const MidiEvent &ev) { return static_cast<int>(ev.data_a()) | (static_cast<int>(ev.data_b()) << 7); }

struct NoteSpan {
    int on = 0;
    int off = 0;
    uint8_t channel = 0;
    uint8_t pitch = 0;
    uint8_t velocity = 0;
};

std::vector<NoteSpan> collect_note_spans(const MidiTrack &track) {
    std::vector<NoteSpan> spans;
    std::map<std::pair<uint8_t, uint8_t>, std::pair<int, uint8_t>> active;
    for (const auto &[tick, ev] : track.events()) {
        if (ev.type() == ME_NOTEON && ev.velo() > 0) {
            const auto key = std::pair<uint8_t, uint8_t>{ev.channel(), ev.pitch()};
            EXPECT_EQ(active.count(key), 0u) << "overlapping NoteOn without prior NoteOff at tick " << tick;
            active[key] = {tick, ev.velo()};
        } else if ((ev.type() == ME_NOTEON && ev.velo() == 0) || ev.type() == ME_NOTEOFF) {
            const auto key = std::pair<uint8_t, uint8_t>{ev.channel(), ev.pitch()};
            const auto it = active.find(key);
            EXPECT_NE(it, active.end()) << "unmatched NoteOff at tick " << tick;
            if (it != active.end()) {
                spans.push_back(NoteSpan{it->second.first, tick, ev.channel(), ev.pitch(), it->second.second});
                active.erase(it);
            }
        }
    }
    EXPECT_TRUE(active.empty()) << "unterminated NoteOn left active at end of track";
    return spans;
}

TEST(ExportMidi, OverlappingSamePitchUnisonsDoNotCutOffEarly) {
    // Measure 1: staggered overlap C4 [0..960] + C4 [480..1440]
    // Measure 2: nested overlap D4 [1920..3840] + D4 [2400..2880]
    // Measure 3: simultaneous unison E4 [3840..4320] + E4 [3840..4800]
    constexpr std::string_view mscx = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Division>480</Division>
    <Part id="1">
      <Staff id="1"><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <trackName>Piano</trackName>
      <Instrument id="piano">
        <trackName>Piano</trackName>
        <Channel><program value="0"/></Channel>
      </Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Chord><durationType>half</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
          <Rest><durationType>half</durationType></Rest>
        </voice>
        <voice>
          <Rest><durationType>quarter</durationType></Rest>
          <Chord><durationType>half</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
          <Rest><durationType>quarter</durationType></Rest>
        </voice>
      </Measure>
      <Measure>
        <voice>
          <Chord><durationType>whole</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
        </voice>
        <voice>
          <Rest><durationType>quarter</durationType></Rest>
          <Chord><durationType>quarter</durationType><Note><pitch>62</pitch><tpc>16</tpc></Note></Chord>
          <Rest><durationType>half</durationType></Rest>
        </voice>
      </Measure>
      <Measure>
        <voice>
          <Chord><durationType>quarter</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
          <Rest><durationType>half</durationType></Rest>
          <Rest><durationType>quarter</durationType></Rest>
        </voice>
        <voice>
          <Chord><durationType>half</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
          <Rest><durationType>half</durationType></Rest>
        </voice>
      </Measure>
    </Staff>
  </Score>
</museScore>)";

    auto doc = open_mscx(mscx);
    ASSERT_NE(doc, nullptr);
    const auto exported = doc->export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(exported) << exported.error().message;

    const MidiFile mf = parse_smf(*exported);
    ASSERT_EQ(mf.tracks().size(), 1u);
    const std::vector<NoteSpan> spans = collect_note_spans(mf.tracks()[0]);

    std::vector<NoteSpan> c4, d4, e4;
    for (const NoteSpan &s : spans) {
        if (s.pitch == 60) c4.push_back(s);
        if (s.pitch == 62) d4.push_back(s);
        if (s.pitch == 64) e4.push_back(s);
    }

    // Staggered C4: [0..480] then restrike [480..1392] (95% piano gate on 960 ticks) without premature cutoff at 912/960.
    ASSERT_EQ(c4.size(), 2u);
    EXPECT_EQ(c4[0].on, 0);
    EXPECT_EQ(c4[0].off, 480);
    EXPECT_EQ(c4[1].on, 480);
    EXPECT_EQ(c4[1].off, 1392);

    // Nested D4: [1920..2400] then restrike [2400..3744] (95% gate on whole note) without premature cutoff at 2856/2880.
    ASSERT_EQ(d4.size(), 2u);
    EXPECT_EQ(d4[0].on, 1920);
    EXPECT_EQ(d4[0].off, 2400);
    EXPECT_EQ(d4[1].on, 2400);
    EXPECT_EQ(d4[1].off, 3744);

    // Simultaneous E4: merged attack at 3840 held through the longer half note at 4752 (not cut off at 4296/4320).
    ASSERT_EQ(e4.size(), 1u);
    EXPECT_EQ(e4[0].on, 3840);
    EXPECT_EQ(e4[0].off, 4752);
}

TEST(ExportMidi, ConcurrentGuitarBendsIsolateOnDynamicMelodicChannelsAndScaleWithRpns) {
    // Measure 1: Voice 1 holds unbent E4 (64) across the measure while Voice 2 bends G3 (55) up +2 semitones.
    // Measure 2: Voice 1 bends G3 (55) up +2 semitones while Voice 2 simultaneously bends B3 (59) up +1 semitone.
    constexpr std::string_view mscx = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Division>480</Division>
    <Part id="1">
      <Staff id="1"><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <trackName>Guitar</trackName>
      <Instrument id="guitar-steel">
        <trackName>Guitar</trackName>
        <StringData>
          <frets>19</frets>
          <string>40</string><string>45</string><string>50</string>
          <string>55</string><string>59</string><string>64</string>
        </StringData>
        <Channel><program value="25"/><volume value="100"/><pan value="64"/></Channel>
      </Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Chord>
            <durationType>whole</durationType>
            <Note><pitch>64</pitch><tpc>18</tpc><string>0</string><fret>0</fret></Note>
          </Chord>
        </voice>
        <voice>
          <Chord>
            <durationType>half</durationType>
            <Note><pitch>55</pitch><tpc>15</tpc><string>2</string><fret>0</fret></Note>
          </Chord>
          <Chord>
            <durationType>half</durationType>
            <Note><pitch>57</pitch><tpc>17</tpc><string>2</string><fret>2</fret></Note>
          </Chord>
        </voice>
      </Measure>
      <Measure>
        <voice>
          <Chord>
            <durationType>half</durationType>
            <Note><pitch>55</pitch><tpc>15</tpc><string>2</string><fret>0</fret></Note>
          </Chord>
          <Chord>
            <durationType>half</durationType>
            <Note><pitch>57</pitch><tpc>17</tpc><string>2</string><fret>2</fret></Note>
          </Chord>
        </voice>
        <voice>
          <Chord>
            <durationType>half</durationType>
            <Note><pitch>59</pitch><tpc>19</tpc><string>1</string><fret>0</fret></Note>
          </Chord>
          <Chord>
            <durationType>half</durationType>
            <Note><pitch>60</pitch><tpc>14</tpc><string>1</string><fret>1</fret></Note>
          </Chord>
        </voice>
      </Measure>
    </Staff>
  </Score>
</museScore>)";

    auto doc = open_mscx(mscx);
    ASSERT_NE(doc, nullptr);
    MasterScore *master = engrave::DocumentAccess::master_score(*doc);
    ASSERT_NE(master, nullptr);
    auto note_at = [&](int tick, track_idx_t track) -> Note * {
        Segment *seg = master->tick2segment(Fraction::from_ticks(tick), true, SegmentType::CHORD_REST);
        if (!seg) return nullptr;
        Chord *ch = to_chord(seg->element(track));
        return (ch && !ch->notes().empty()) ? ch->notes()[0] : nullptr;
    };
    ASSERT_NE(master->add_guitar_bend(GuitarBendType::BEND, note_at(0, 1), note_at(960, 1)), nullptr);
    ASSERT_NE(master->add_guitar_bend(GuitarBendType::BEND, note_at(1920, 0), note_at(2880, 0)), nullptr);
    ASSERT_NE(master->add_guitar_bend(GuitarBendType::BEND, note_at(1920, 1), note_at(2880, 1)), nullptr);

    const auto rpn_on = doc->export_score({.format = engrave::Format::MIDI, .midi_rpns = true});
    const auto rpn_off = doc->export_score({.format = engrave::Format::MIDI, .midi_rpns = false});
    ASSERT_TRUE(rpn_on) << rpn_on.error().message;
    ASSERT_TRUE(rpn_off) << rpn_off.error().message;

    const MidiFile mf_on = parse_smf(*rpn_on);
    const MidiFile mf_off = parse_smf(*rpn_off);
    ASSERT_EQ(mf_on.tracks().size(), 1u);
    ASSERT_EQ(mf_off.tracks().size(), 1u);

    const std::vector<NoteSpan> spans_on = collect_note_spans(mf_on.tracks()[0]);

    // Find Measure 1 unbent E4 (64) and bent G3 (55).
    const NoteSpan *m1_unbent = nullptr;
    const NoteSpan *m1_bent = nullptr;
    const NoteSpan *m2_bend_a = nullptr;
    const NoteSpan *m2_bend_b = nullptr;
    for (const NoteSpan &s : spans_on) {
        if (s.on < 1920 && s.pitch == 64) m1_unbent = &s;
        if (s.on < 1920 && s.pitch == 55) m1_bent = &s;
        if (s.on >= 1920 && s.pitch == 55) m2_bend_a = &s;
        if (s.on >= 1920 && s.pitch == 59) m2_bend_b = &s;
    }
    ASSERT_NE(m1_unbent, nullptr);
    ASSERT_NE(m1_bent, nullptr);
    ASSERT_NE(m2_bend_a, nullptr);
    ASSERT_NE(m2_bend_b, nullptr);

    // Bent note overlapping unbent string must isolate onto a free melodic channel (never ch 9).
    EXPECT_NE(m1_bent->channel, m1_unbent->channel);
    EXPECT_NE(m1_bent->channel, 9u);

    // Two concurrent bends in Measure 2 must run on distinct melodic channels.
    EXPECT_NE(m2_bend_a->channel, m2_bend_b->channel);
    EXPECT_NE(m2_bend_a->channel, 9u);
    EXPECT_NE(m2_bend_b->channel, 9u);

    // Unbent channel must see zero non-center pitch bends during Measure 1, while dynamic channel receives
    // replicated program/RPN setup, non-center bend curve, and center reset.
    bool unbent_ch_bent_in_m1 = false;
    bool dyn_ch_got_program = false;
    bool dyn_ch_got_rpn = false;
    int dyn_m1_peak_on = 8192;
    bool dyn_m1_reset = false;
    int m2_b_peak_on = 8192;
    for (const auto &[tick, ev] : mf_on.tracks()[0].events()) {
        if (ev.type() == ME_PITCHBEND) {
            const int v = pitch_bend_value(ev);
            if (ev.channel() == m1_unbent->channel && tick < 1920 && v != 8192) {
                unbent_ch_bent_in_m1 = true;
            }
            if (ev.channel() == m1_bent->channel && tick <= m1_bent->off) {
                dyn_m1_peak_on = std::max(dyn_m1_peak_on, v);
                if (tick >= m1_bent->on && v == 8192) dyn_m1_reset = true;
            }
            if (ev.channel() == m2_bend_b->channel && tick >= 1920) {
                m2_b_peak_on = std::max(m2_b_peak_on, v);
            }
        } else if (ev.channel() == m1_bent->channel) {
            if (ev.type() == ME_PROGRAM && ev.data_b() == 25) dyn_ch_got_program = true;
            if (ev.type() == ME_CONTROLLER && ev.controller() == CTRL_HDATA && ev.value() == 12) dyn_ch_got_rpn = true;
        }
    }
    EXPECT_FALSE(unbent_ch_bent_in_m1) << "unbent string channel must stay centered during concurrent bend";
    EXPECT_TRUE(dyn_ch_got_program);
    EXPECT_TRUE(dyn_ch_got_rpn);
    EXPECT_TRUE(dyn_m1_reset);
    // With midi_rpns = true (±12 semitones): +2 semitones peaks around 8192 + 1365 = 9557, +1 around 8875.
    EXPECT_NEAR(dyn_m1_peak_on, 9557, 120);
    EXPECT_NEAR(m2_b_peak_on, 8875, 120);

    // With midi_rpns = false (GM default ±2 semitones): no RPN CCs, and +1 semitone scales to ~12288 (+2 to ~16383).
    int rpn_ccs_off = 0;
    int m2_b_peak_off = 8192;
    int dyn_m1_peak_off = 8192;
    const std::vector<NoteSpan> spans_off = collect_note_spans(mf_off.tracks()[0]);
    uint8_t m1_bent_ch_off = 255;
    uint8_t m2_b_ch_off = 255;
    for (const NoteSpan &s : spans_off) {
        if (s.on < 1920 && s.pitch == 55) m1_bent_ch_off = s.channel;
        if (s.on >= 1920 && s.pitch == 59) m2_b_ch_off = s.channel;
    }
    ASSERT_NE(m1_bent_ch_off, 255);
    ASSERT_NE(m2_b_ch_off, 255);
    for (const auto &[tick, ev] : mf_off.tracks()[0].events()) {
        if (ev.type() == ME_CONTROLLER && (ev.controller() == CTRL_LRPN || ev.controller() == CTRL_HRPN || ev.controller() == CTRL_HDATA)) {
            ++rpn_ccs_off;
        } else if (ev.type() == ME_PITCHBEND) {
            const int v = pitch_bend_value(ev);
            if (ev.channel() == m1_bent_ch_off && tick < 1920) dyn_m1_peak_off = std::max(dyn_m1_peak_off, v);
            if (ev.channel() == m2_b_ch_off && tick >= 1920) m2_b_peak_off = std::max(m2_b_peak_off, v);
        }
    }
    EXPECT_EQ(rpn_ccs_off, 0);
    EXPECT_NEAR(m2_b_peak_off, 12288, 250);
    EXPECT_GT(dyn_m1_peak_off, 15500);
}

TEST(ExportMidi, WriteBudgetRejectsDuringPlaybackScoreConstruction) {
    auto opened = engine().open(std::filesystem::path(ENGRAVE_TEST_SOURCE_ROOT) / "tests/data/inputs/golden/bend.gp");
    ASSERT_TRUE(opened) << opened.error().message;
    auto &doc = **opened;

    // Tiny working budget must reject during PlaybackScoreBuilder::build before materializing the full store.
    const auto rejected = doc.export_score({.format = engrave::Format::MIDI, .capture = {.max_working_bytes = 1}});
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().code, engrave::ErrorCode::RESOURCE_LIMIT);

    // Direct builder check: aborting on the first growth callback stops before walking notes.
    MasterScore *master = engrave::DocumentAccess::master_score(doc);
    ASSERT_NE(master, nullptr);

    struct BudgetHit {};

    std::uint64_t first_charge = 0;
    int charge_calls = 0;
    PlaybackScoreBuilder builder;
    EXPECT_THROW(std::ignore = builder.build(master, {},
                                             [&](std::uint64_t bytes) {
                                                 ++charge_calls;
                                                 first_charge = bytes;
                                                 throw BudgetHit{};
                                             }),
                 BudgetHit);
    EXPECT_EQ(charge_calls, 1);
    EXPECT_GT(first_charge, 0u);
}

TEST(ExportMidi, ExpressionCC11TracksArticulationChannelSwitches) {
    // Violin (supports single-note dynamics) switching from arco (ch 0) to pizzicato (ch 1) across a crescendo.
    constexpr std::string_view mscx = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Division>480</Division>
    <Part id="1">
      <Staff id="1"><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <trackName>Violin</trackName>
      <Instrument id="violin">
        <trackName>Violin</trackName>
        <Channel><program value="40"/></Channel>
        <Channel name="pizzicato"><program value="45"/></Channel>
      </Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Dynamic><subtype>p</subtype></Dynamic>
          <Spanner type="HairPin">
            <HairPin><subtype>0</subtype></HairPin>
            <next><location><measures>1</measures><fractions>1/2</fractions></location></next>
          </Spanner>
          <Chord><durationType>half</durationType><Note><pitch>69</pitch><tpc>17</tpc></Note></Chord>
          <Chord><durationType>half</durationType><Note><pitch>71</pitch><tpc>19</tpc></Note></Chord>
        </voice>
      </Measure>
      <Measure>
        <voice>
          <PlayTechAnnotation>
            <playTechType>pizzicato</playTechType>
            <text>pizz.</text>
          </PlayTechAnnotation>
          <Chord><durationType>half</durationType><Note><pitch>72</pitch><tpc>14</tpc></Note></Chord>
          <Spanner type="HairPin">
            <prev><location><measures>-1</measures><fractions>-1/2</fractions></location></prev>
          </Spanner>
          <Dynamic><subtype>ff</subtype></Dynamic>
          <Chord><durationType>half</durationType><Note><pitch>74</pitch><tpc>16</tpc></Note></Chord>
        </voice>
      </Measure>
    </Staff>
  </Score>
</museScore>)";

    auto doc = open_mscx(mscx);
    ASSERT_NE(doc, nullptr);
    const auto exported = doc->export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(exported) << exported.error().message;

    const MidiFile mf = parse_smf(*exported);
    ASSERT_EQ(mf.tracks().size(), 1u);

    std::set<uint8_t> note_channels;
    std::map<uint8_t, std::vector<int>> cc11_values;
    for (const auto &[tick, ev] : mf.tracks()[0].events()) {
        if (ev.type() == ME_NOTEON && ev.velo() > 0) {
            note_channels.insert(ev.channel());
        } else if (ev.type() == ME_CONTROLLER && ev.controller() == CTRL_EXPRESSION) {
            cc11_values[ev.channel()].push_back(ev.value());
        }
    }

    ASSERT_EQ(note_channels.size(), 2u) << "expected distinct arco and pizzicato channels";
    for (uint8_t ch : note_channels) {
        ASSERT_GE(cc11_values[ch].size(), 2u) << "channel " << int(ch) << " must receive CC11 crescendo updates";
        EXPECT_LT(cc11_values[ch].front(), cc11_values[ch].back());
    }
}

TEST(ExportMidi, NestedSostenutoSpansReleaseAtOuterEnd) {
    // Voice 1 whole note at tick 0 with LaissezVib spans [192..4032];
    // Voice 2 quarter note at tick 480 with LaissezVib spans [528..3888] (strictly nested inside Voice 1).
    constexpr std::string_view mscx = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Division>480</Division>
    <Part id="1">
      <Staff id="1"><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <trackName>Piano</trackName>
      <Instrument id="piano">
        <trackName>Piano</trackName>
        <Channel><program value="0"/></Channel>
      </Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Chord>
            <durationType>whole</durationType>
            <Note><pitch>60</pitch><tpc>14</tpc><LaissezVib/></Note>
          </Chord>
        </voice>
        <voice>
          <Rest><durationType>quarter</durationType></Rest>
          <Chord>
            <durationType>quarter</durationType>
            <Note><pitch>64</pitch><tpc>18</tpc><LaissezVib/></Note>
          </Chord>
          <Rest><durationType>half</durationType></Rest>
        </voice>
      </Measure>
      <Measure>
        <voice><Rest><durationType>measure</durationType><duration>4/4</duration></Rest></voice>
      </Measure>
    </Staff>
  </Score>
</museScore>)";

    auto doc = open_mscx(mscx);
    ASSERT_NE(doc, nullptr);
    const auto exported = doc->export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(exported) << exported.error().message;

    const MidiFile mf = parse_smf(*exported);
    ASSERT_EQ(mf.tracks().size(), 1u);

    std::vector<std::pair<int, int>> cc66;
    for (const auto &[tick, ev] : mf.tracks()[0].events()) {
        if (ev.type() == ME_CONTROLLER && ev.controller() == CTRL_SOSTENUTO) {
            cc66.emplace_back(tick, ev.value());
        }
    }

    ASSERT_EQ(cc66.size(), 2u) << "nested sostenuto spans must merge into one on/off pair";
    EXPECT_EQ(cc66[0].second, 127);
    EXPECT_EQ(cc66[1].second, 0);
    // Outer span ends at 0.1 * 1920 + 3840 = 4032 ticks (inner would have released early at 3888).
    EXPECT_GE(cc66[1].first, 4000);
}

TEST(ExportMidi, TempoMultiplierMatchesTempoTimelineElapsedTime) {
    constexpr std::string_view mscx = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Division>480</Division>
    <Part id="1">
      <Staff id="1"><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <trackName>Piano</trackName>
      <Instrument id="piano">
        <trackName>Piano</trackName>
        <Channel><program value="0"/></Channel>
      </Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Tempo><tempo>2</tempo><text>120</text></Tempo>
          <Chord><durationType>half</durationType><Note><pitch>60</pitch><tpc>14</tpc></Note></Chord>
          <Breath><symbol>caesura</symbol><pause>2</pause></Breath>
          <Chord><durationType>half</durationType><Note><pitch>64</pitch><tpc>18</tpc></Note></Chord>
        </voice>
      </Measure>
    </Staff>
  </Score>
</museScore>)";

    auto doc = open_mscx(mscx);
    ASSERT_NE(doc, nullptr);
    MasterScore *master = engrave::DocumentAccess::master_score(*doc);
    ASSERT_NE(master, nullptr);
    ASSERT_TRUE(master->set_tempo_multiplier(BeatsPerSecond(2.0)));

    const double expected_second_note_secs = master->tempo_timeline(true).utick2utime(960);
    // Nominal time before caesura (2 beats at 2 bps = 1.0s) + 2.0s pause = 3.0s; at 2x multiplier = 1.5s.
    EXPECT_NEAR(expected_second_note_secs, 1.5, 1e-3);

    const auto exported = doc->export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(exported) << exported.error().message;
    const MidiFile mf = parse_smf(*exported);
    ASSERT_EQ(mf.tracks().size(), 1u);

    std::vector<std::pair<int, int>> tempo_us_events;
    int second_note_tick = -1;
    for (const auto &[tick, ev] : mf.tracks()[0].events()) {
        if (ev.type() == ME_META && ev.meta_type() == META_TEMPO && ev.len() == 3) {
            const int us_per_quarter = (ev.edata()[0] << 16) | (ev.edata()[1] << 8) | ev.edata()[2];
            tempo_us_events.emplace_back(tick, us_per_quarter);
        } else if (ev.type() == ME_NOTEON && ev.velo() > 0 && ev.pitch() == 64) {
            second_note_tick = tick;
        }
    }
    ASSERT_GT(second_note_tick, 960) << "caesura pause must expand into extra SMF ticks";
    ASSERT_GE(tempo_us_events.size(), 2u);

    // Integrate SMF elapsed seconds from tick 0 to second_note_tick using exported META_TEMPO events.
    double smf_secs = 0.0;
    int cur_tick = 0;
    int cur_us_per_q = 500000;
    for (const auto &[t, us_per_q] : tempo_us_events) {
        if (t >= second_note_tick) break;
        if (t > cur_tick) {
            smf_secs += (static_cast<double>(t - cur_tick) / mf.division()) * (static_cast<double>(cur_us_per_q) / 1'000'000.0);
            cur_tick = t;
        }
        cur_us_per_q = us_per_q;
    }
    if (second_note_tick > cur_tick) {
        smf_secs += (static_cast<double>(second_note_tick - cur_tick) / mf.division()) * (static_cast<double>(cur_us_per_q) / 1'000'000.0);
    }

    EXPECT_NEAR(smf_secs, expected_second_note_secs, 5e-3);
}

TEST(ExportMidi, MultiPortArticulationChannelRoutesToMatchingPortTrack) {
    constexpr std::string_view mscx = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="4.60">
  <Score>
    <Division>480</Division>
    <Part id="1">
      <Staff id="1"><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
      <trackName>Violin</trackName>
      <Instrument id="violin">
        <trackName>Violin</trackName>
        <Channel><program value="40"/></Channel>
        <Channel name="pizzicato"><program value="45"/></Channel>
      </Instrument>
    </Part>
    <Staff id="1">
      <Measure>
        <voice>
          <TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
          <Chord><durationType>whole</durationType><Note><pitch>69</pitch><tpc>17</tpc></Note></Chord>
        </voice>
      </Measure>
      <Measure>
        <voice>
          <PlayTechAnnotation>
            <playTechType>pizzicato</playTechType>
            <text>pizz.</text>
          </PlayTechAnnotation>
          <Chord><durationType>whole</durationType><Note><pitch>72</pitch><tpc>14</tpc></Note></Chord>
        </voice>
      </Measure>
    </Staff>
  </Score>
</museScore>)";

    auto doc = open_mscx(mscx);
    ASSERT_NE(doc, nullptr);
    MasterScore *master = engrave::DocumentAccess::master_score(*doc);
    ASSERT_NE(master, nullptr);
    ASSERT_EQ(master->parts().size(), 1u);
    Part *part = master->parts()[0];
    InstrChannel *pizz = nullptr;
    for (InstrChannel *ch : part->instrument()->channel()) {
        if (ch->name() == u"pizzicato") pizz = ch;
    }
    ASSERT_NE(pizz, nullptr);
    master->update_midi_mapping(pizz, part, /*midi_port=*/1, /*midi_channel=*/2);

    const auto exported = doc->export_score({.format = engrave::Format::MIDI});
    ASSERT_TRUE(exported) << exported.error().message;

    const MidiFile mf = parse_smf(*exported);
    ASSERT_EQ(mf.tracks().size(), 2u) << "pizzicato channel on port 1 must route to a track with META_PORT_CHANGE(1)";
    EXPECT_EQ(track_port(mf.tracks()[0]), 0);
    EXPECT_EQ(track_port(mf.tracks()[1]), 1);

    const std::vector<NoteSpan> port0_notes = collect_note_spans(mf.tracks()[0]);
    const std::vector<NoteSpan> port1_notes = collect_note_spans(mf.tracks()[1]);
    ASSERT_EQ(port0_notes.size(), 1u);
    EXPECT_EQ(port0_notes[0].pitch, 69);
    ASSERT_EQ(port1_notes.size(), 1u);
    EXPECT_EQ(port1_notes[0].pitch, 72);
    EXPECT_EQ(port1_notes[0].channel, 2);

    bool port1_has_pizz_program = false;
    bool port0_has_ch2_setup = false;
    for (const auto &[tick, ev] : mf.tracks()[0].events()) {
        if (ev.type() == ME_PROGRAM && ev.channel() == 2) port0_has_ch2_setup = true;
    }
    for (const auto &[tick, ev] : mf.tracks()[1].events()) {
        if (ev.type() == ME_PROGRAM && ev.channel() == 2 && ev.data_b() == 45) port1_has_pizz_program = true;
    }
    EXPECT_FALSE(port0_has_ch2_setup) << "port 1 channel setup must not leak onto port 0 track";
    EXPECT_TRUE(port1_has_pizz_program);
}

} // namespace
