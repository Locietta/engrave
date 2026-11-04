// SPDX-License-Identifier: GPL-3.0-only
#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/runtime/noqt_runtime.h"
#include "engrave/src/score/score_excerpts.h"
#include "engrave/src/score/score_metadata.h"
#include "engrave/src/render/score_render.h"
#include "engraving/engravingproject.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/measure.h"
#include "engraving/playback/metaparsers/chordarticulationsparser.h"
#include "engraving/playback/playbackcontext.h"
#include "engraving/playback/renderingcontext.h"
#include "engraving/dom/part.h"
#include "engraving/dom/text.h"
#include "global/io/buffer.h"
#include "importexport/musicxml/internal/export/exportmusicxml.h"
#include "importexport/midi/internal/midiexport/exportmidi.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void expect(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

void save(const std::filesystem::path &path, const std::string &bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), bytes.size());
    stream.close();
    expect(bool(stream), "Cannot write test output");
}

std::string export_selection(const music::EngineContext &context, mu::engraving::Score *score, bool midi) {
    muse::ByteArray bytes;
    muse::io::Buffer buffer(&bytes);
    buffer.open(muse::io::IODevice::WriteOnly);
    if (midi) {
        mu::iex::midi::ExportMidi writer(score);
        expect(writer.write(&buffer, true, true), "Excerpt MIDI export failed");
    } else
        expect(mu::iex::musicxml::saveXml(score, &buffer, context.musicXml()), "Excerpt XML export failed");
    return {reinterpret_cast<const char *>(bytes.constData()), bytes.size()};
}

const char *fixture = R"(<score-partwise version="4.0"><part-list>
<score-part id="P1"><part-name>Violin</part-name><midi-instrument id="I1"><midi-channel>1</midi-channel><midi-program>41</midi-program></midi-instrument></score-part>
<score-part id="P2"><part-name>Cello</part-name><midi-instrument id="I2"><midi-channel>2</midi-channel><midi-program>43</midi-program></midi-instrument></score-part>
</part-list><part id="P1"><measure number="1"><attributes><divisions>1</divisions><time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef></attributes><note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note></measure></part>
<part id="P2"><measure number="1"><attributes><divisions>1</divisions><time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>F</sign><line>4</line></clef></attributes><note><pitch><step>E</step><octave>3</octave></pitch><duration>4</duration><type>whole</type></note></measure></part></score-partwise>)";

// Native 5.00 input, deliberately independent of the MusicXML fixture above and
// of external files. Each case loads a fresh score with an explicit play value.
const char *chordline_fixture = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff><StaffType group="pitched"><name>stdNormal</name></StaffType></Staff>
<trackName>Flute</trackName><Instrument id="flute"><longName>Flute</longName>
<trackName>Flute</trackName><instrumentId>wind.flutes.flute</instrumentId>
<Channel><program value="73"/></Channel></Instrument></Part>
<Staff id="1"><Measure><voice>
<Clef><concertClefType>G</concertClefType><transposingClefType>G</transposingClefType></Clef>
<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><durationType>whole</durationType><Note><pitch>72</pitch><tpc>14</tpc></Note>
<ChordLine><play>0</play><subtype>1</subtype></ChordLine></Chord>
</voice></Measure></Staff></Score></museScore>)";

mu::engraving::ChordLine *check_chordline_playback(mu::engraving::Score *score, bool enabled, const char *stage) {
    using namespace mu::engraving;
    using namespace muse::mpe;
    const auto check = [=](bool value, const char *message) {
        if (!value) throw std::runtime_error(std::string(enabled ? "Enabled " : "Disabled ") + stage + ": " + message);
    };
    check(score && score->firstMeasure(), "Missing measure");
    auto *chord = score->firstMeasure()->findChord(Fraction(0, 1), 0);
    check(chord && chord->notes().size() == 1, "Missing fixture chord");
    auto *line = chord->chordLine();
    check(line && line->chordLineType() == ChordLineType::FALL, "Missing fall");
    check(line->playChordLine() == enabled, "Playback flag changed");
    check(line->getProperty(Pid::PLAY).toBool() == enabled, "PLAY property changed");

    // Exercise the playback consumer as well as the stored property. A nonempty
    // Fall profile ensures the enabled control cannot pass by emitting nothing.
    auto profile = std::make_shared<ArticulationsProfile>();
    ArticulationPattern pattern;
    pattern.emplace(
        0, ArticulationPatternSegment(ArrangementPattern(HUNDRED_PERCENT, 0), PitchPattern(2, HUNDRED_PERCENT, 0),
                                      ExpressionPattern(2, HUNDRED_PERCENT, dynamicLevelFromType(muse::mpe::DynamicType::Natural))));
    profile->setPattern(ArticulationType::Fall, pattern);
    auto playback = std::make_shared<PlaybackContext>(score);
    const auto rendering = buildRenderingCtx(chord, 0, profile, playback);
    check(rendering.isValid(), "Invalid playback rendering context");
    ArticulationMap articulations;
    ChordArticulationsParser::buildChordArticulationMap(chord, rendering, articulations);
    check(articulations.contains(ArticulationType::Fall) == enabled, "Fall playback articulation changed");
    check(articulations.size() == (enabled ? 1u : 0u), "Unexpected playback articulations");
    return line;
}

void test_chordline_playback(const music::EngineContext &context, const std::filesystem::path &out, bool enabled) {
    std::string input = chordline_fixture;
    if (enabled) {
        const std::string disabled_play = "<play>0</play>";
        const auto play = input.find(disabled_play);
        expect(play != std::string::npos, "Fixture missing explicit playback flag");
        input.replace(play, disabled_play.size(), "<play>1</play>");
    }
    auto project = music::load_native_score(context, input, "mscx");
    auto *master = project->masterScore();
    expect(music::list_score_excerpts(master).empty(), "Chordline fixture already has excerpts");
    auto *original = check_chordline_playback(master, enabled, "master before generation");
    expect(music::generate_score_excerpts(master), "Chordline excerpt was not generated");
    expect(music::list_score_excerpts(master).size() == 1, "Expected one chordline excerpt");
    expect(check_chordline_playback(master, enabled, "master after generation") == original, "Generation replaced the master chordline");
    auto *excerpt = music::select_score(master, 0);
    expect(excerpt != master, "Expected a separate excerpt score");
    expect(check_chordline_playback(excerpt, enabled, "generated excerpt") != original, "Expected a cloned chordline in excerpt");

    const auto archive = music::export_native_score(context, project, "mscz", 0, "");
    save(out / (enabled ? "chordline-enabled.mscz" : "chordline-disabled.mscz"), archive);
    auto reopened = music::load_native_score(context, archive, "mscz");
    auto *reopened_master = reopened->masterScore();
    expect(music::list_score_excerpts(reopened_master).size() == 1, "Saved chordline excerpt lost");
    expect(!music::generate_score_excerpts(reopened_master), "Saved chordline excerpt regenerated");
    check_chordline_playback(reopened_master, enabled, "reopened master");
    check_chordline_playback(music::select_score(reopened_master, 0), enabled, "reopened excerpt");
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc != 3) throw std::invalid_argument("Usage: excerpts_test ASSETS OUTPUT_DIRECTORY");
        const auto out = std::filesystem::absolute(std::filesystem::u8path(argv[2]));
        std::filesystem::create_directories(out);
        music::EngineContext context(argv[1]);
        test_chordline_playback(context, out, false);
        test_chordline_playback(context, out, true);
        auto project = music::load_native_score(context, fixture, "musicxml");
        auto *master = project->masterScore();
        expect(music::list_score_excerpts(master).empty(), "Unexpected initial excerpts");
        expect(music::generate_score_excerpts(master), "No excerpts generated");
        auto records = music::list_score_excerpts(master);
        expect(records.size() == 2, "Expected one excerpt per part");
        auto *first = music::select_score(master, 0);
        expect(!music::generate_score_excerpts(master), "Repeated generation changed score");
        expect(music::select_score(master, 0) == first && music::list_score_excerpts(master).size() == 2,
               "Repeated generation changed excerpt identity");
        for (int i = 0; i < 2; ++i) {
            auto *selected = music::select_score(master, i);
            expect(selected->parts().size() == 1 && selected->nstaves() == 1 && selected->npages() > 0,
                   "Excerpt staff/part/layout filter failed");
            expect(records[i].part_ids.at(0) == master->parts()[i]->id().toStdString(), "Excerpt part ID mismatch");
            save(out / ("part" + std::to_string(i) + ".musicxml"), export_selection(context, selected, false));
            save(out / ("part" + std::to_string(i) + ".mid"), export_selection(context, selected, true));
            save(out / ("part" + std::to_string(i) + ".svg"), music::render_score(context, selected, "svg", 0));
        }
        for (int invalid : {-2, 2}) {
            bool rejected = false;
            try {
                music::select_score(master, invalid);
            } catch (const std::out_of_range &) {
                rejected = true;
            }
            expect(rejected, "Invalid excerpt ID accepted");
        }
        const auto archive = music::export_native_score(context, project, "mscz", 0, "");
        save(out / "with-excerpts.mscz", archive);
        auto reopened = music::load_native_score(context, archive, "mscz");
        expect(music::list_score_excerpts(reopened->masterScore()).size() == 2, "Saved excerpts lost on reopen");
        expect(!music::generate_score_excerpts(reopened->masterScore()), "Loaded excerpts were regenerated");
        for (int i = 0; i < 2; ++i) {
            auto *selected = music::select_score(reopened->masterScore(), i);
            expect(selected->parts().size() == 1 && selected->npages() > 0, "Reopened excerpt invalid");
            save(out / ("reopened-part" + std::to_string(i) + ".musicxml"), export_selection(context, selected, false));
        }
        auto hidden = music::load_native_score(context, fixture, "musicxml");
        hidden->masterScore()->parts()[1]->setShow(false);
        expect(music::generate_score_excerpts(hidden->masterScore()), "Visible excerpt missing");
        expect(music::list_score_excerpts(hidden->masterScore()).size() == 1, "Hidden part generated excerpt");
        // No title Text exists in this fixture, so workTitle is the selected title source.
        master->setMetaTag(u"workTitle", muse::String::fromUtf8("\xE4\xB8\xAD\xE6\x96\x87 /\xC2\xA0\xE3\x80\x80"
                                                                "A.B\xEF\xBB\xBF"
                                                                "C"));
        expect(music::score_title_filename_safe(master) == "\xE4\xB8\xAD\xE6\x96\x87"
                                                           "_A_B_C",
               "Unicode filename-safe title mismatch");
        music::MetadataTrack binding;
        binding.part_id = master->parts()[0]->id().toStdString();
        binding.instrument_id = master->parts()[0]->instrumentId().toStdString();
        binding.type = "fluid_soundfont";
        binding.name = "Test preset";
        binding.sound_id = "test-resource-id";
        binding.vendor = "must be omitted for Fluid";
        auto metadata = music::score_metadata(master, {binding});
        expect(metadata["tracks"].size() == 1 && !metadata["tracks"][0].contains("vendor"), "Track metadata incorrect");
        bool duplicate_rejected = false;
        try {
            music::score_metadata(master, {binding, binding});
        } catch (const std::invalid_argument &) {
            duplicate_rejected = true;
        }
        expect(duplicate_rejected, "Duplicate metadata track accepted");
        save(out / "metadata.json", metadata.dump(2));
        auto track_project = music::load_native_score(context, fixture, "musicxml");
        auto *tracks_score = track_project->masterScore();
        tracks_score->parts()[0]->setId(muse::ID(uint64_t(10)));
        tracks_score->parts()[1]->setId(muse::ID(uint64_t(2)));
        music::MetadataTrack upper = binding, lower = binding;
        upper.part_id = "10";
        upper.instrument_id = tracks_score->parts()[0]->instrumentId().toStdString();
        lower.part_id = "2";
        lower.instrument_id = tracks_score->parts()[1]->instrumentId().toStdString();
        lower.type = "vst_plugin";
        lower.vendor = "Actual test vendor";
        lower.sound_id = lower.name;
        lower.category = lower.name;
        const auto sorted = music::score_metadata(tracks_score, {upper, lower})["tracks"];
        expect(sorted[0]["partId"] == "2" && sorted[1]["partId"] == "10", "Metadata track order is not numeric");
        expect(sorted[0]["vendor"] == "Actual test vendor" && !sorted[0].contains("soundId") && !sorted[0].contains("category"),
               "Metadata optional resource fields mismatch");
        for (const auto &bad_part : {"99999", "18446744073709551616", "not-an-id"}) {
            auto bad = lower;
            bad.part_id = bad_part;
            bool rejected = false;
            try {
                music::score_metadata(tracks_score, {bad});
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            expect(rejected, "Invalid metadata part accepted");
        }
        lower.instrument_id = "not-an-instrument";
        bool instrument_rejected = false;
        try {
            music::score_metadata(tracks_score, {lower});
        } catch (const std::invalid_argument &) {
            instrument_rejected = true;
        }
        expect(instrument_rejected, "Invalid metadata instrument accepted");
        tracks_score->setMetaTag(u"workTitle", muse::String::fromUtf8("A\xC2\x85\xE1\xA0\x8E"
                                                                      "B\xE2\x80\xA8"
                                                                      "C"));
        expect(music::score_title_filename_safe(tracks_score) == "A\xC2\x85\xE1\xA0\x8E"
                                                                 "B_C",
               "Non-JS whitespace was replaced");
        std::cout << "Excerpt generation/select/idempotence/visibility/save-reopen and Unicode title/track metadata passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
