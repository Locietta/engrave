import std;
import engrave;

namespace {

void check(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

template <typename T>
T take(engrave::Result<T> result) {
    if (!result) throw std::runtime_error(std::string(engrave::to_string(result.error().code)) + ": " + result.error().message);
    return std::move(*result);
}

void check_contracts() {
    using namespace engrave;
    check(version_major == 0 && version_minor == 1 && version_patch == 0 && version_string == "0.1.0", "API version");
    constexpr Fraction third{1, 3};
    constexpr Fraction sixth{1, 6};
    constexpr Fraction sum{third.num * sixth.den + sixth.num * third.den, third.den * sixth.den};
    static_assert(sum == Fraction{1, 2});
    static_assert(Fraction{-2, 6} == Fraction{-1, 3});
    static_assert(Fraction{-1, 3} < Fraction{1, 6});
    static_assert(Fraction{std::numeric_limits<std::int64_t>::max(), 2} > Fraction{1, 3});
    check(sum == Fraction{3, 6} && sixth < third && third > sixth, "fraction comparison");
    check(ElementId{0, 1} < ElementId{1, 0} && ElementId{0, 0} == ElementId{}, "element id ADL operators");
    check(element_id_from_string("A_A") == ElementId{0, 0}, "MSCX zero id decoding");
    check(to_string(ElementId{0, 0}) == "A_A", "MSCX zero id encoding");
    Result<int> failure = std::unexpected(Error{ErrorCode::NOT_FOUND, "missing"});
    check(!failure && to_string(failure.error().code) == "NotFound", "error propagation");
    check(default_score_capture_working_bytes(2) == 1048832, "capture allowance");
    check(std::string_view(export_warning_code::invalid_tie) == "B470_INVALID_TIE", "warning constant");
    // An empty asset path fails before consuming the process's single runtime.
    auto invalid = Engine::create({});
    check(!invalid && invalid.error().code == ErrorCode::NOT_FOUND, "invalid asset error");
}

void check_score(const std::filesystem::path &assets) {
    using namespace engrave;
    EngineOptions options;
    options.resources = assets;
    auto engine = take(Engine::create(options));
    CreateOptions create;
    create.template_id = "Treble_Clef";
    auto document = take(engine->create_document(create));
    auto before = take(document->outline());
    check(!before.measures.empty(), "template has measures");
    const auto first = before.measures.front();
    SegmentAddress at{first.id, 0, 0, first.tick, "ChordRest"};
    Batch batch;
    batch.ops = {InsertMeasures{std::nullopt, 1}, EnterNotes{at, {1, 4}, {{60, std::nullopt}}}};
    auto edited = take(document->edit(batch));
    check(!edited.rejection, "batch accepted");
    auto outline = take(document->outline());
    // Appending one 4/4 bar extends the score by one whole note.
    const auto &last = outline.measures.back();
    const auto &old_last = before.measures.back();
    check(last.tick == Fraction{old_last.tick.num * old_last.duration.den + old_last.duration.num * old_last.tick.den,
                                old_last.tick.den * old_last.duration.den},
          "appended measure starts at previous bar end");
    Query query;
    query.types = {"Note"};
    query.first_measure = 0;
    query.measure_count = 1;
    query.props = {"pitch"};
    auto notes = take(document->query(query));
    check(!notes.elements.empty(), "note query");
    const auto &note = notes.elements.front();
    check(note.tick == Fraction{0, 1}, "note onset");
    const auto pitch = std::ranges::find(note.props, "pitch", &PropertyRead::name);
    check(pitch != note.props.end() && std::get<std::int64_t>(pitch->value) == 60, "middle C pitch");
    query.types = {"Chord"};
    query.props.clear();
    auto chords = take(document->query(query));
    check(!chords.elements.empty() && chords.elements.front().duration == Fraction{1, 4}, "quarter-note duration");
    RenderOptions render;
    render.format = RenderFormat::SVG;
    render.last_page = 0;
    auto output = take(document->render(render));
    check(!output.pages.empty() && !output.pages.front().empty(), "SVG page bytes");
    const auto &bytes = output.pages.front();
    const std::string_view svg(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    check(svg.find("<svg") != std::string_view::npos && svg.find("</svg>") != std::string_view::npos &&
              svg.find("http://www.w3.org/2000/svg") != std::string_view::npos,
          "SVG root and namespace");
}

} // namespace

int main(int argc, char **argv) {
    try {
        std::filesystem::path assets;
        if (argc == 3 && std::string_view(argv[1]) == "--assets")
            assets = argv[2];
        else if (argc != 1)
            throw std::runtime_error("usage: engrave_module_test [--assets <dir>]");
        check_contracts();
        if (!assets.empty()) check_score(assets);
        std::cout << "engrave module: contracts passed" << (assets.empty() ? "" : "; score edit/query/SVG passed") << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "engrave module: " << error.what() << '\n';
        return 1;
    }
}
