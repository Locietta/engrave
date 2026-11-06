#include "engrave/src/runtime/engine_context.h"
#include "engrave/src/runtime/noqt_runtime.h"
#include "engrave/src/score/score_positions.h"
#include "engraving/engravingproject.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include <iostream>
#include <stdexcept>

namespace {
void expect(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

void check_on_page(const nlohmann::json &data) {
    const double width = data.at("pageSize").at("width").get<double>();
    const double height = data.at("pageSize").at("height").get<double>();
    for (const auto &element : data.at("elements")) {
        const double x = element.at("x").get<double>();
        const double y = element.at("y").get<double>();
        expect(element.at("sx").get<double>() > 0 && element.at("sy").get<double>() > 0, "Position element has zero width or height");
        expect(x >= 0 && x < width && y >= 0 && y < height, "Position element lies outside the page");
    }
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc != 2) throw std::invalid_argument("Usage: score_positions_test ASSETS");
        music::EngineContext context(argv[1]);
        const auto project = music::load_native_score(context, R"(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1"><measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef>
</attributes><barline location="left"><repeat direction="forward"/></barline>
<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
<barline location="right"><repeat direction="backward"/></barline></measure></part>
</score-partwise>)",
                                                      "musicxml");
        auto *score = project->master_score();
        for (bool expanded : {false, true}) {
            for (bool segments : {false, true}) {
                score->set_expand_repeats(expanded);
                const auto data = music::score_positions(score, segments);
                expect(score->expand_repeats() == expanded, "Position query changed repeat setting");
                expect(data.at("elements").size() == 1 && data.at("events").size() == 2, "Position query failed to expand the repeat");
            }
        }
        // MusicXML multiple-rest turns on the multi-measure-rest style and sets
        // the break flags, matching upstream layout and PositionJsonWriter.
        const auto mmrest = music::load_native_score(context, R"(<score-partwise version="4.0">
<part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
<part id="P1">
<measure number="1"><attributes><divisions>1</divisions>
<time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef>
<measure-style><multiple-rest>4</multiple-rest></measure-style>
</attributes><note><rest/><duration>4</duration><voice>1</voice></note></measure>
<measure number="2"><note><rest/><duration>4</duration><voice>1</voice></note></measure>
<measure number="3"><note><rest/><duration>4</duration><voice>1</voice></note></measure>
<measure number="4"><note><rest/><duration>4</duration><voice>1</voice></note></measure>
<measure number="5"><note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note></measure>
</part></score-partwise>)",
                                                     "musicxml");
        auto *mm_score = mmrest->master_score();
        auto *mm_first = mm_score->first_measure_mm();
        expect(mm_first && mm_first->is_mm_rest() && mm_first->mm_rest_count() == 4,
               "MusicXML multiple-rest was not imported as a multi-measure rest");
        auto *mm_next = mm_first->next_measure_mm();
        expect(mm_next && !mm_next->is_mm_rest() && !mm_next->next_measure_mm(),
               "Multi-measure rest did not cover the four underlying measures");
        // Upstream PositionJsonWriter iterates firstMeasureMM/nextMeasureMM, so a
        // four-measure rest contributes one element at its start, not four.
        const auto mm_measures = music::score_positions(mm_score, false);
        expect(mm_measures.at("elements").size() == 2 && mm_measures.at("events").size() == 2,
               "Measure positions did not follow upstream multi-measure rest iteration");
        expect(mm_measures.at("events")[0].at("position") == 0 && mm_measures.at("events")[1].at("position") == 8000,
               "Measure positions do not span the multi-measure rest in milliseconds");
        check_on_page(mm_measures);
        const auto mm_segments = music::score_positions(mm_score, true);
        expect(mm_segments.at("elements").size() == 2 && mm_segments.at("events").size() == 2,
               "Segment positions did not follow upstream multi-measure rest iteration");
        expect(mm_segments.at("events")[0].at("position") == 0 && mm_segments.at("events")[1].at("position") == 8000,
               "Segment positions do not span the multi-measure rest in milliseconds");
        check_on_page(mm_segments);
        bool rejected = false;
        try {
            music::score_positions(nullptr, false);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        expect(rejected, "Null score was accepted");
        std::cout << "Position queries leave both initial repeat settings unchanged; multi-measure rests"
                     " follow upstream measure iteration and span their playback time\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
