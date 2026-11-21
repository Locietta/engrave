// Jianpu (numbered notation): pitch spelling, key numbering, the layout output on
// a Jianpu staff, and that rendering it leaves the authored score untouched.
//
// Ported from the tests of MuseScore 82ba7e5736 (durationtype, note, keysig, beam,
// jianpudurationline and jianpuoctavedot) onto tests/data/inputs/jianpu-beam.mscx,
// upstream's beam_data/jianpuBeam.mscx.

#include "draw/fontmetrics.h"
#include "engrave/src/score/score_semantic.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/jianpudurationline.h"
#include "engraving/dom/jianpuoctavedot.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/notedot.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/utils.h"
#include "engraving/infrastructure/sourcehandle.h"
#include "engraving/rendering/score/beamtremololayout.h"
#include "engraving/types/typesconv.h"
#include "handles_test_fixture.h"

using namespace handles_test;
using namespace mu::engraving;

namespace {

std::filesystem::path jianpu_fixture() { return g_source_root / "tests/data/inputs/jianpu-beam.mscx"; }

// The fixture with extra markup before its first chord of the first measure.
std::string jianpu_source_with(std::string_view before_first_chord) {
    auto xml = read_text(jianpu_fixture());
    const auto at = xml.find("<Chord>");
    if (at == std::string::npos) throw std::runtime_error("jianpu fixture has no chord");
    xml.insert(at, before_first_chord);
    return xml;
}

// The fixture's header and Jianpu staff around other measures.
std::string jianpu_score(std::string_view measures) {
    auto xml = read_text(jianpu_fixture());
    const auto first = xml.find("<Measure>");
    const auto end = xml.find("</Staff>", first);
    if (first == std::string::npos || end == std::string::npos) throw std::runtime_error("jianpu fixture has no measures");
    return xml.replace(first, end - first, measures);
}

// Starts a measure with a staff type change to the given <StaffType> markup.
std::string staff_type_change(std::string_view type) { return std::format("<StaffTypeChange>{}</StaffTypeChange>", type); }

constexpr std::string_view jianpu_type = "<StaffType group=\"jianpu\"><name>jianpu</name><lines>0</lines></StaffType>";
constexpr std::string_view pitched_type = "<StaffType group=\"pitched\"><name>stdNormal</name></StaffType>";
constexpr std::string_view four_four = "<TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>";

// Chord and rest tags such as <dots> go before <durationType>, as the writer puts them.
std::string chord(std::string_view duration, int pitch, int tpc, std::string_view chord_tags = "", std::string_view note_tags = "") {
    return std::format("<Chord>{}<durationType>{}</durationType><Note>{}<pitch>{}</pitch><tpc>{}</tpc></Note></Chord>", chord_tags,
                       duration, note_tags, pitch, tpc);
}

std::string rest(std::string_view duration, std::string_view tags = "") {
    return std::format("<Rest>{}<durationType>{}</durationType></Rest>", tags, duration);
}

ChordRest *chord_rest_at(Score &score, Fraction tick, track_idx_t track = 0) {
    Segment *segment = score.tick2segment(tick, true, SegmentType::CHORD_REST);
    EngravingItem *item = segment ? segment->element(track) : nullptr;
    if (!item || !item->is_chord_rest()) throw std::runtime_error(std::format("no chord rest at {}", tick.ticks()));
    return to_chord_rest(item);
}

std::vector<ChordRest *> chord_rests(Score &score, track_idx_t track) {
    std::vector<ChordRest *> result;
    for (Segment *s = score.first_segment(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
        if (auto *item = s->element(track); item && item->is_chord_rest()) result.push_back(to_chord_rest(item));
    }
    return result;
}

void render(engrave::Document &document) {
    const auto svg = document.render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(svg) << svg.error().message;
    ASSERT_FALSE(svg->pages.empty());
}

// Natural notes in C: the digit is the scale step counted from C.
std::string c_major_digit(int pitch) {
    static constexpr std::array<char, 12> digits = {'1', 0, '2', 0, '3', '4', 0, '5', 0, '6', 0, '7'};
    return std::string(1, digits[static_cast<size_t>(pitch % 12)]);
}

// Undotted digits in C start at C3 (pitch 48); each further octave is one dot.
int c_major_octave(int pitch) { return pitch >= 48 ? (pitch - 48) / 12 : -((47 - pitch) / 12 + 1); }

} // namespace

TEST(Jianpu, AugmentationAndDiminutionLinesFollowTheDuration) {
    const auto augmentation = [](DurationType type, int dots) {
        TDuration duration(type);
        duration.set_dots(dots);
        return jianpu_augmentation_lines_for_duration(duration);
    };
    // A half is two quarters: one line after the digit; each dot adds half the value.
    EXPECT_EQ(augmentation(DurationType::V_QUARTER, 0), 0);
    EXPECT_EQ(augmentation(DurationType::V_HALF, 0), 1);
    EXPECT_EQ(augmentation(DurationType::V_HALF, 1), 2);
    EXPECT_EQ(augmentation(DurationType::V_HALF, 2), 2);
    EXPECT_EQ(augmentation(DurationType::V_WHOLE, 0), 3);
    EXPECT_EQ(augmentation(DurationType::V_WHOLE, 1), 5);
    EXPECT_EQ(augmentation(DurationType::V_BREVE, 0), 7);
    EXPECT_EQ(augmentation(DurationType::V_LONG, 0), 15);

    const std::pair<DurationType, int> diminution[] = {
        {DurationType::V_LONG, 0},    {DurationType::V_BREVE, 0},  {DurationType::V_WHOLE, 0}, {DurationType::V_HALF, 0},
        {DurationType::V_QUARTER, 0}, {DurationType::V_EIGHTH, 1}, {DurationType::V_16TH, 2},  {DurationType::V_32ND, 3},
        {DurationType::V_64TH, 4},    {DurationType::V_128TH, 5},  {DurationType::V_256TH, 6}, {DurationType::V_512TH, 7},
        {DurationType::V_1024TH, 8},
    };
    for (const auto &[type, lines] : diminution) EXPECT_EQ(jianpu_diminution_lines_for_duration(TDuration(type)), lines) << int(type);

    // A dot of a quarter or more is a line; a smaller one stays a dot. A dotted half is
    // "- -", a double-dotted half "- -" and a dot for the remaining eighth.
    const std::tuple<DurationType, int, int> residual[] = {
        {DurationType::V_EIGHTH, 1, 1}, {DurationType::V_QUARTER, 1, 1}, {DurationType::V_QUARTER, 2, 2}, {DurationType::V_HALF, 1, 0},
        {DurationType::V_HALF, 2, 1},   {DurationType::V_WHOLE, 2, 0},   {DurationType::V_WHOLE, 3, 1},   {DurationType::V_BREVE, 0, 0},
    };
    for (const auto &[type, dots, left] : residual) EXPECT_EQ(jianpu_residual_dots(type, dots), left) << int(type) << "/" << dots;
}

TEST(Jianpu, DigitsAndOctavesFollowTheTonicAndMode) {
    EXPECT_EQ(pitch2_jianpu_octave(36, Tpc::TPC_C, Key::C, KeyMode::MAJOR), -1);
    EXPECT_EQ(pitch2_jianpu_octave(48, Tpc::TPC_C, Key::C, KeyMode::MAJOR), 0);
    EXPECT_EQ(pitch2_jianpu_octave(60, Tpc::TPC_C, Key::C, KeyMode::MAJOR), 1);
    EXPECT_EQ(pitch2_jianpu_octave(72, Tpc::TPC_C, Key::C, KeyMode::MAJOR), 2);
    EXPECT_EQ(pitch2_jianpu_octave(43, Tpc::TPC_G, Key::G, KeyMode::MAJOR), -1);
    EXPECT_EQ(pitch2_jianpu_octave(48, Tpc::TPC_C, Key::G, KeyMode::MAJOR), -1);
    EXPECT_EQ(pitch2_jianpu_octave(55, Tpc::TPC_G, Key::G, KeyMode::MAJOR), 0);
    EXPECT_EQ(pitch2_jianpu_octave(60, Tpc::TPC_C, Key::G, KeyMode::MAJOR), 0);
    EXPECT_EQ(pitch2_jianpu_octave(66, Tpc::TPC_F_S, Key::G, KeyMode::MAJOR), 0);
    EXPECT_EQ(pitch2_jianpu_octave(67, Tpc::TPC_G, Key::G, KeyMode::MAJOR), 1);
    EXPECT_EQ(pitch2_jianpu_octave(79, Tpc::TPC_G, Key::G, KeyMode::MAJOR), 2);

    // Each mode of the C tonic, from its relative major.
    const std::pair<Key, KeyMode> c_tonics[] = {
        {Key::B_B, KeyMode::DORIAN},  {Key::A_B, KeyMode::PHRYGIAN}, {Key::G, KeyMode::LYDIAN},    {Key::F, KeyMode::MIXOLYDIAN},
        {Key::E_B, KeyMode::AEOLIAN}, {Key::C, KeyMode::IONIAN},     {Key::D_B, KeyMode::LOCRIAN}, {Key::E_B, KeyMode::MINOR},
        {Key::C, KeyMode::MAJOR},     {Key::C, KeyMode::UNKNOWN},    {Key::C, KeyMode::NONE},
    };
    for (const auto &[key, mode] : c_tonics) EXPECT_EQ(jianpu_tonic_tpc(key, mode), Tpc::TPC_C) << int(key) << "/" << int(mode);
    // An unmarked mode is the major key of the signature.
    EXPECT_EQ(jianpu_tonic_tpc(Key::G, KeyMode::UNKNOWN), Tpc::TPC_G);
    EXPECT_EQ(jianpu_tonic_tpc(Key::E_B, KeyMode::UNKNOWN), Tpc::TPC_E_B);

    const auto spelled = [](int tpc, int tonic, KeyMode mode) {
        const auto spelling = tpc2_jianpu(tpc, tonic, mode);
        return spelling.accidental.to_std_string() + spelling.step_name.to_std_string();
    };
    // C Dorian numbers from its second degree: C is 2, E flat is 4, E natural is #4.
    const int c_dorian = jianpu_tonic_tpc(Key::B_B, KeyMode::DORIAN);
    EXPECT_EQ(spelled(Tpc::TPC_C, c_dorian, KeyMode::DORIAN), "2");
    EXPECT_EQ(spelled(Tpc::TPC_E_B, c_dorian, KeyMode::DORIAN), "4");
    EXPECT_EQ(spelled(Tpc::TPC_E, c_dorian, KeyMode::DORIAN), "#4");
    EXPECT_EQ(spelled(Tpc::TPC_F_S, Tpc::TPC_C, KeyMode::MAJOR), "#4");
    EXPECT_EQ(spelled(Tpc::TPC_G_B, Tpc::TPC_C, KeyMode::MAJOR), "b5");
    const int a_minor = jianpu_tonic_tpc(Key::C, KeyMode::MINOR);
    EXPECT_EQ(a_minor, Tpc::TPC_A);
    EXPECT_EQ(spelled(Tpc::TPC_A, a_minor, KeyMode::MINOR), "6");
    EXPECT_EQ(spelled(Tpc::TPC_C, a_minor, KeyMode::MINOR), "1");
    EXPECT_EQ(jianpu_accidental_mark(u"#b"), u"♯♭");

    EXPECT_EQ(pitch2_jianpu_octave(58, Tpc::TPC_B_B, c_dorian, KeyMode::DORIAN), 0);
    EXPECT_EQ(pitch2_jianpu_octave(60, Tpc::TPC_C, c_dorian, KeyMode::DORIAN), 0);
    EXPECT_EQ(pitch2_jianpu_octave(57, Tpc::TPC_A, a_minor, KeyMode::MINOR), -1);
    EXPECT_EQ(pitch2_jianpu_octave(59, Tpc::TPC_B, a_minor, KeyMode::MINOR), -1);
    EXPECT_EQ(pitch2_jianpu_octave(60, Tpc::TPC_C, a_minor, KeyMode::MINOR), 0);
    EXPECT_EQ(pitch2_jianpu_octave(69, Tpc::TPC_A, a_minor, KeyMode::MINOR), 0);
    EXPECT_EQ(pitch2_jianpu_octave(72, Tpc::TPC_C, a_minor, KeyMode::MINOR), 1);
}

TEST(Jianpu, KeySignatureNumberingMapsTheTonicAndLabel) {
    KeySigEvent g_major;
    g_major.set_concert_key(Key::G);
    for (int field = 0; field < 3; ++field) {
        KeySigEvent changed = g_major;
        if (field == 0) changed.set_jianpu_numbering(JianpuNumbering::CUSTOM);
        if (field == 1) changed.set_jianpu_tonic_key(Key::D);
        if (field == 2) changed.set_jianpu_tonic_mode(KeyMode::DORIAN);
        EXPECT_FALSE(changed == g_major) << field;
        EXPECT_TRUE(changed != g_major) << field;
    }

    KeySigEvent d_dorian;
    d_dorian.set_concert_key(Key::D);
    d_dorian.set_mode(KeyMode::DORIAN);
    // D major's signature in Dorian is E Dorian.
    EXPECT_EQ(jianpu_key_mapping(d_dorian).mode, KeyMode::DORIAN);
    EXPECT_EQ(jianpu_key_mapping(d_dorian).tonic_tpc, Tpc::TPC_E);
    d_dorian.set_jianpu_numbering(JianpuNumbering::CUSTOM);
    d_dorian.set_jianpu_tonic_key(Key::A_B);
    d_dorian.set_jianpu_tonic_mode(KeyMode::LYDIAN);
    EXPECT_EQ(jianpu_key_mapping(d_dorian).mode, KeyMode::LYDIAN);
    EXPECT_EQ(jianpu_key_mapping(d_dorian).tonic_tpc, Tpc::TPC_A_B);

    const auto label = [](Key key, std::optional<KeyMode> mode) {
        KeySigEvent ks;
        ks.set_concert_key(key);
        if (mode) ks.set_mode(*mode);
        return jianpu_key_label(ks).to_std_string();
    };
    EXPECT_EQ(label(Key::C, std::nullopt), "1=C");
    EXPECT_EQ(label(Key::G, std::nullopt), "1=G");
    EXPECT_EQ(label(Key::C, KeyMode::NONE), "1=C");
    EXPECT_EQ(label(Key::C, KeyMode::MAJOR), "1=C");
    EXPECT_EQ(label(Key::C, KeyMode::MINOR), "6=A");
    EXPECT_EQ(label(Key::B_B, KeyMode::DORIAN), "2=C");
    EXPECT_EQ(label(Key::E_B, std::nullopt), "1=E♭");
    KeySigEvent custom;
    custom.set_concert_key(Key::G);
    custom.set_jianpu_numbering(JianpuNumbering::CUSTOM);
    custom.set_jianpu_tonic_key(Key::C);
    custom.set_jianpu_tonic_mode(KeyMode::MAJOR);
    EXPECT_EQ(jianpu_key_label(custom).to_std_string(), "1=C");

    EXPECT_EQ(TConv::to_xml(JianpuNumbering::CUSTOM), "custom");
    EXPECT_EQ(TConv::to_xml(JianpuNumbering::FOLLOW_SCORE_KEY), "followScoreKey");
    EXPECT_EQ(TConv::to_xml(Key::E_B), "Eb");
    EXPECT_EQ(TConv::to_xml(Key::F_S), "F#");
    for (int key = int(Key::MIN); key <= int(Key::MAX); ++key) EXPECT_EQ(TConv::from_xml(TConv::to_xml(Key(key)), Key::INVALID), Key(key));
    EXPECT_EQ(TConv::from_xml("unrecognized", JianpuNumbering::FOLLOW_SCORE_KEY), JianpuNumbering::FOLLOW_SCORE_KEY);
    EXPECT_EQ(TConv::from_xml("unrecognized", Key::C), Key::C);
    EXPECT_EQ(TConv::from_xml("jianpu", StaffGroup::STANDARD), StaffGroup::JIANPU);
    EXPECT_EQ(TConv::from_xml("jianpu", StaffTypes::STANDARD), StaffTypes::JIANPU);
    EXPECT_EQ(TConv::from_xml("jianpu_number", TextStyleType::DEFAULT), TextStyleType::JIANPU_NUMBER);
    EXPECT_EQ(StaffType::get_default_preset(StaffGroup::JIANPU)->type(), StaffTypes::JIANPU);
    // The Jianpu group leaves the older defaults alone.
    EXPECT_EQ(StaffType::get_default_preset(StaffGroup::TAB)->xml_name(), u"tab6StrSimple");
    EXPECT_EQ(StaffType::get_default_preset(StaffGroup::PERCUSSION)->xml_name(), u"perc5Line");
    EXPECT_EQ(StaffType::get_default_preset(StaffGroup::STANDARD)->xml_name(), u"stdNormal");

    // A linked staff keeps its own numbering, as it may be a staff of another type.
    for (Pid pid : {Pid::JIANPU_NUMBERING, Pid::JIANPU_TONIC_KEY, Pid::JIANPU_TONIC_MODE}) EXPECT_FALSE(property_link_same_score(pid));
    EXPECT_EQ(property_type(Pid::JIANPU_NUMBERING), PType::JIANPU_NUMBERING);
    EXPECT_EQ(property_type(Pid::JIANPU_TONIC_KEY), PType::KEY);
    EXPECT_EQ(property_type(Pid::JIANPU_TONIC_MODE), PType::KEY_MODE);
}

TEST(Jianpu, KeySignatureNumberingRoundTripsAndFollowsEdits) {
    auto document = open_text(jianpu_source_with("<KeySig><eid>Z_Z</eid><concertKey>2</concertKey><mode>major</mode>"
                                                 "<jianpuNumbering>custom</jianpuNumbering><jianpuTonicKey>Eb</jianpuTonicKey>"
                                                 "<jianpuTonicMode>phrygian</jianpuTonicMode></KeySig>"));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *segment = master.first_measure()->find_segment(SegmentType::KEY_SIG, Fraction(0, 1));
    ASSERT_NE(segment, nullptr);
    auto *keysig = to_key_sig(segment->element(0));
    ASSERT_NE(keysig, nullptr);
    ASSERT_FALSE(keysig->generated());
    EXPECT_EQ(keysig->get_property(Pid::JIANPU_NUMBERING).type(), PType::JIANPU_NUMBERING);
    EXPECT_EQ(keysig->get_property(Pid::JIANPU_NUMBERING).value<JianpuNumbering>(), JianpuNumbering::CUSTOM);
    EXPECT_EQ(keysig->get_property(Pid::JIANPU_TONIC_KEY).type(), PType::KEY);
    EXPECT_EQ(keysig->get_property(Pid::JIANPU_TONIC_KEY).value<Key>(), Key::E_B);
    EXPECT_EQ(keysig->get_property(Pid::JIANPU_TONIC_MODE).value<KeyMode>(), KeyMode::PHRYGIAN);
    EXPECT_EQ(keysig->property_default(Pid::JIANPU_NUMBERING).value<JianpuNumbering>(), JianpuNumbering::FOLLOW_SCORE_KEY);
    EXPECT_EQ(keysig->property_default(Pid::JIANPU_TONIC_KEY).value<Key>(), Key::C);
    EXPECT_EQ(keysig->property_default(Pid::JIANPU_TONIC_MODE).value<KeyMode>(), KeyMode::MAJOR);
    EXPECT_EQ(master.staff(0)->key_sig_event(Fraction(0, 1)), keysig->key_sig_event());

    const auto written = export_mscx(*document);
    for (const auto tag :
         {"<jianpuNumbering>custom</jianpuNumbering>", "<jianpuTonicKey>Eb</jianpuTonicKey>", "<jianpuTonicMode>phrygian</jianpuTonicMode>",
          "<StaffType group=\"jianpu\">", "<jianpuTextStyle>jianpu_number</jianpuTextStyle>"})
        EXPECT_NE(written.find(tag), std::string::npos) << tag;
    auto reopened = reopen(*document);
    ASSERT_TRUE(reopened);
    EXPECT_EQ(master_of(*reopened).staff(0)->key_sig_event(Fraction(0, 1)), keysig->key_sig_event());

    // Eb Phrygian numbers its tonic 3.
    render(*document);
    EXPECT_EQ(keysig->ldata()->jianpu_label.to_std_string(), "3=E♭");
    EXPECT_TRUE(keysig->ldata()->key_symbols.empty());

    const auto id = engine_id(*keysig);
    auto edited = document->edit({{engrave::SetProperty{id, "jianpuTonicKey", engrave::Enum{"D"}},
                                   engrave::SetProperty{id, "jianpuTonicMode", engrave::Enum{"major"}}}});
    ASSERT_TRUE(edited) << edited.error().message;
    render(*document);
    EXPECT_EQ(keysig->ldata()->jianpu_label.to_std_string(), "1=D");
    EXPECT_EQ(master.staff(0)->key_sig_event(Fraction(0, 1)).jianpu_tonic_key(), Key::D);
    edited = document->edit({{engrave::SetProperty{id, "jianpuNumbering", engrave::Enum{"followScoreKey"}}}});
    ASSERT_TRUE(edited) << edited.error().message;
    render(*document);
    // Follow the signature: two sharps, D major.
    EXPECT_EQ(keysig->ldata()->jianpu_label.to_std_string(), "1=D");
    EXPECT_EQ(export_mscx(*document).find("<jianpuNumbering>"), std::string::npos);

    const auto rejected = document->edit({{engrave::SetProperty{id, "jianpuTonicKey", engrave::Enum{"invalid"}}}});
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().code, ErrorCode::INVALID_ARGUMENT);
}

TEST(Jianpu, LayoutDrawsDigitsDotsLinesAndRestsInsteadOfNoteheads) {
    auto document = open_path(jianpu_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    ASSERT_TRUE(master.staff(0)->is_jianpu_staff(Fraction(0, 1)));
    render(*document);

    const StaffType *type = master.staff(0)->staff_type(Fraction(0, 1));
    EXPECT_EQ(type->group(), StaffGroup::JIANPU);
    EXPECT_EQ(type->jianpu_font().family(), muse::draw::Font::FontFamily(u"FreeSans"));
    EXPECT_GT(type->jianpu_box_h(), 0.0);

    int chords = 0, rests = 0, lined = 0, dotted = 0, beamed = 0;
    for (track_idx_t track = 0; track < 2; ++track) {
        for (ChordRest *cr : chord_rests(master, track)) {
            if (cr->is_rest()) {
                ++rests;
                const auto *rest = to_rest(cr);
                EXPECT_EQ(rest->ldata()->sym, SymId::NO_SYM);
                EXPECT_TRUE(rest->dot_list().empty());
                EXPECT_DOUBLE_EQ(rest->ldata()->bbox().height(), type->jianpu_box_h() * rest->mag_s());
                // The fixture's half rest is "0 -": one augmentation line after the zero.
                const size_t lines = rest->duration_type().type() == DurationType::V_HALF ? 1 : 0;
                ASSERT_EQ(rest->jianpu_duration_lines().size(), lines);
                const double zero_width = muse::draw::FontMetrics(type->jianpu_font()).width(muse::String(u"0"));
                EXPECT_DOUBLE_EQ(rest->ldata()->jianpu_digit_box.width(), zero_width);
                for (const JianpuDurationLine *line : rest->jianpu_duration_lines()) {
                    ++lined;
                    EXPECT_DOUBLE_EQ(line->ldata()->len, zero_width);
                    EXPECT_DOUBLE_EQ(line->ldata()->pos().x(), zero_width * 1.2);
                    EXPECT_EQ(line->chord_rest(), rest);
                    // The line takes space like the rest: its staff y is the rest's own.
                    EXPECT_DOUBLE_EQ(line->pos().y(), rest->pos().y());
                    EXPECT_GE(rest->shape().right(), line->pos().x() + line->ldata()->bbox().right());
                }
                continue;
            }
            ++chords;
            Chord *chord = to_chord(cr);
            EXPECT_EQ(chord->stem(), nullptr);
            EXPECT_EQ(chord->hook(), nullptr);
            EXPECT_TRUE(chord->ledger_lines().empty());
            const int lines = chord->duration_type().type() == DurationType::V_HALF ? 1 : 0;
            ASSERT_EQ(chord->jianpu_duration_lines().size(), size_t(lines));
            for (const JianpuDurationLine *line : chord->jianpu_duration_lines()) {
                ++lined;
                const Note *up = chord->up_note();
                // One head width long and 1.2 head widths after the digit.
                EXPECT_DOUBLE_EQ(line->ldata()->len, up->head_width());
                EXPECT_DOUBLE_EQ(line->ldata()->pos().x(), up->pos().x() + up->bbox_x_shift() + up->head_width() * 1.2);
                EXPECT_DOUBLE_EQ(line->ldata()->line_width, master.style().style_absolute(Sid::JIANPU_DURATION_LINE_THICKNESS));
                EXPECT_EQ(line->chord_rest(), chord);
                EXPECT_EQ(line->track(), chord->track());
                const Shape shape = chord->shape();
                EXPECT_GE(shape.right(), line->pos().x() + line->ldata()->bbox().right());
            }
            if (chord->beam()) {
                ++beamed;
                EXPECT_DOUBLE_EQ(chord->beam()->slope(), 0.0);
            }
            for (const Note *note : chord->notes()) {
                // The fixture's notes are naturals in C major.
                EXPECT_EQ(note->jianpu_digit().to_std_string(), c_major_digit(note->pitch())) << note->pitch();
                EXPECT_TRUE(note->jianpu_accidental().is_empty());
                EXPECT_TRUE(!note->accidental() || note->accidental()->ldata()->is_skip_draw());
                const int octave = c_major_octave(note->pitch());
                ASSERT_EQ(note->jianpu_octave_dots().size(), size_t(std::abs(octave))) << note->pitch();
                for (const JianpuOctaveDot *dot : note->jianpu_octave_dots()) {
                    ++dotted;
                    EXPECT_EQ(dot->ldata()->above, octave > 0);
                    EXPECT_EQ(dot->note(), note);
                    EXPECT_EQ(dot->track(), note->track());
                    EXPECT_DOUBLE_EQ(dot->ldata()->radius, master.style().style_absolute(Sid::JIANPU_OCTAVE_DOT_RADIUS) * note->mag());
                    EXPECT_GT(dot->ldata()->len, 0.0);
                }
            }
        }
    }
    EXPECT_EQ(chords, 37);
    EXPECT_EQ(rests, 6);
    EXPECT_EQ(lined, 1);
    EXPECT_GT(dotted, 0);
    EXPECT_GT(beamed, 0);

    // The generated header key signature shows the tonic of the C major default.
    auto *keysig_segment = master.first_measure()->find_rendered_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
    ASSERT_NE(keysig_segment, nullptr);
    const auto *keysig = to_key_sig(keysig_segment->effective_element(0));
    ASSERT_NE(keysig, nullptr);
    EXPECT_EQ(keysig->ldata()->jianpu_label.to_std_string(), "1=C");
}

TEST(Jianpu, ChordDigitsStackHighestOnTopInOneColumn) {
    auto document = open_text(jianpu_source_with("<Chord><eid>Y_Y</eid><durationType>quarter</durationType>"
                                                 "<Note><eid>X_X</eid><pitch>60</pitch><tpc>14</tpc></Note>"
                                                 "<Note><eid>W_W</eid><pitch>64</pitch><tpc>18</tpc></Note>"
                                                 "<Note><eid>V_V</eid><pitch>67</pitch><tpc>15</tpc></Note></Chord>"));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    render(*document);
    auto *chord = to_chord(chord_rests(master, 0).front());
    ASSERT_EQ(chord->notes().size(), 3u);
    const double x = chord->notes().front()->x();
    double above = std::numeric_limits<double>::max();
    for (const Note *note : chord->notes()) {
        EXPECT_DOUBLE_EQ(note->x(), x);
        // notes() runs from low to high pitch; each digit sits above the previous one.
        EXPECT_LT(note->y(), above);
        above = note->y();
    }
    EXPECT_EQ(chord->up_note()->jianpu_digit().to_std_string(), "5");
}

TEST(Jianpu, SingleGraceNoteGetsItsOwnDiminutionBeam) {
    auto document = open_text(jianpu_source_with("<Chord><eid>U_U</eid><acciaccatura/><durationType>eighth</durationType>"
                                                 "<Note><eid>T_T</eid><pitch>62</pitch><tpc>16</tpc></Note></Chord>"));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    render(*document);
    auto *main = to_chord(chord_rests(master, 0).front());
    ASSERT_EQ(main->grace_notes().size(), 1u);
    Chord *grace = main->grace_notes().front();
    Beam *beam = grace->beam();
    ASSERT_NE(beam, nullptr);
    EXPECT_TRUE(beam->is_grace());
    ASSERT_EQ(beam->rendered_elements().size(), 1u);
    EXPECT_EQ(beam->rendered_elements().front(), grace);
    ASSERT_EQ(beam->beam_segments().size(), 1u);
    EXPECT_NEAR(beam->beam_segments().front()->line.x2() - beam->beam_segments().front()->line.x1(), grace->ldata()->bbox().width(), 0.001);
}

TEST(Jianpu, BeamsAreHorizontalWithoutRewritingTheAuthoredBeam) {
    auto document = open_path(jianpu_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    render(*document);
    int beams = 0;
    for (ChordRest *cr : chord_rests(master, 0)) {
        Beam *beam = cr->beam();
        if (!beam || beam->rendered_elements().front() != cr) continue;
        ++beams;
        EXPECT_TRUE(rendering::score::BeamTremoloLayout::no_slope(beam));
        EXPECT_FALSE(beam->get_property(Pid::BEAM_NO_SLOPE).to_bool()) << "the style default stays the authored value";
        EXPECT_DOUBLE_EQ(beam->slope(), 0.0);
        EXPECT_DOUBLE_EQ(beam->beam_width(), master.style().style_absolute(Sid::JIANPU_DIMINUTION_BEAM_THICKNESS) * beam->mag() *
                                                 (beam->is_grace() ? master.style().style_d(Sid::GRACE_NOTE_MAG) : 1.0));
        for (const BeamSegment *segment : beam->beam_segments()) EXPECT_DOUBLE_EQ(segment->line.y1(), segment->line.y2());
    }
    EXPECT_GT(beams, 0);
}

TEST(Jianpu, SecondVoiceStacksBelowTheFirst) {
    auto xml = read_text(jianpu_fixture());
    const auto end = xml.find("</Measure>");
    ASSERT_NE(end, std::string::npos);
    xml.insert(end, "<voice><Chord><eid>S_S</eid><durationType>whole</durationType>"
                    "<Note><eid>R_R</eid><pitch>48</pitch><tpc>14</tpc></Note></Chord></voice>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    render(*document);
    int shared = 0;
    for (Segment *s = master.first_segment(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
        auto *upper = s->element(0);
        auto *lower = s->element(1);
        if (!upper || !lower) continue;
        ++shared;
        const auto baseline = [](EngravingItem *item) {
            return item->is_chord() ? to_chord(item)->up_note()->ldata()->pos().y() : item->ldata()->pos().y();
        };
        EXPECT_GT(baseline(lower) - baseline(upper), master.style().style_absolute(Sid::JIANPU_VOICE_VERTICAL_DISTANCE))
            << s->tick().ticks();
    }
    EXPECT_GT(shared, 0);
}

TEST(Jianpu, RenderingLeavesTheSourceHandlesAndQueriesUntouched) {
    auto document = open_path(jianpu_fixture());
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    expect_no_layout(master);
    const auto cold_native = native_captures(*document);
    const auto cold_source = source_native(master);
    const auto cold_semantic = music::semantic_dump(&master).dump();
    const auto cold_objects = engrave::document_objects(master);
    const auto cold_handles = master.eid_register()->size();
    const auto cold_outline = document->outline();
    ASSERT_TRUE(cold_outline);
    ASSERT_EQ(cold_outline->parts.size(), 1u);
    ASSERT_EQ(cold_outline->parts.front().staves.size(), 1u);
    EXPECT_EQ(cold_outline->parts.front().staves.front().group, "jianpu");

    for (int cycle = 0; cycle < 2; ++cycle) {
        SCOPED_TRACE(cycle);
        render(*document);
        size_t dots = 0, lines = 0;
        for (EngravingObject *object : cold_objects) {
            EXPECT_FALSE(object->is_jianpu_octave_dot() || object->is_jianpu_duration_line());
        }
        master.scan_elements([&](EngravingItem *item) {
            if (!item->is_jianpu_octave_dot() && !item->is_jianpu_duration_line()) return;
            item->is_jianpu_octave_dot() ? ++dots : ++lines;
            EXPECT_FALSE(source_object(*item));
            EXPECT_TRUE(item->generated());
            // Layout output: drawn, but not a child of its authored owner.
            const auto &siblings = item->ownership_parent()->children();
            EXPECT_EQ(std::find(siblings.begin(), siblings.end(), item), siblings.end());
        });
        EXPECT_GT(dots, 0u);
        EXPECT_EQ(lines, 1u);
        EXPECT_EQ(engrave::document_objects(master), cold_objects);
        EXPECT_EQ(master.eid_register()->size(), cold_handles);
        EXPECT_EQ(source_native(master), cold_source);
        EXPECT_EQ(music::semantic_dump(&master).dump(), cold_semantic);
        EXPECT_EQ(native_captures(*document), cold_native);

        master.layout_data_store()->clear();
        for (ChordRest *cr : chord_rests(master, 0)) {
            EXPECT_TRUE(cr->jianpu_duration_lines().empty());
            if (cr->is_chord())
                for (Note *note : to_chord(cr)->notes()) {
                    EXPECT_TRUE(note->jianpu_octave_dots().empty());
                    EXPECT_TRUE(note->jianpu_digit().is_empty());
                }
        }
        EXPECT_EQ(source_native(master), cold_source);
    }
}

TEST(Jianpu, SvgDrawsTheFreeSansDigitsAndRests) {
    auto document = open_path(jianpu_fixture());
    ASSERT_TRUE(document);
    // The default Jianpu face is a bundled font, not the tablature fallback (FreeSerif).
    const auto &fonts = engrave::DocumentAccess::context(*document).fonts_database();
    EXPECT_EQ(fonts.actual_font(muse::draw::FontDataKey(u"FreeSans"), muse::draw::Font::Type::TABLATURE).family(),
              muse::draw::Font::FontFamily(u"FreeSans"));
    const auto *type = master_of(*document).staff(0)->staff_type(Fraction(0, 1));
    muse::draw::Font serif(type->jianpu_font());
    serif.set_family(u"FreeSerif", muse::draw::Font::Type::TABLATURE);
    EXPECT_NE(muse::draw::FontMetrics(serif).tight_bounding_rect(u"01234567").height(), type->jianpu_box_h());

    const auto svg = document->render({.format = engrave::RenderFormat::SVG});
    ASSERT_TRUE(svg) << svg.error().message;
    ASSERT_EQ(svg->pages.size(), 1u);
    EXPECT_GT(text_of(svg->pages.front()).size(), 1000u);

    const auto png = document->render({.format = engrave::RenderFormat::PNG, .dpi = 72});
    ASSERT_TRUE(png) << png.error().message;
    ASSERT_EQ(png->pages.size(), 1u);
    EXPECT_GT(png->pages.front().size(), 1000u);
}

TEST(Jianpu, RestsKeepTheDotsTheirLinesDoNotShow) {
    auto document =
        open_text(jianpu_score(std::format("<Measure><voice>{}{}{}{}{}{}</voice></Measure><Measure><voice>{}{}</voice></Measure>",
                                           four_four, rest("quarter", "<dots>1</dots>"), rest("eighth", "<dots>1</dots>"), rest("16th"),
                                           rest("quarter"), rest("eighth"), rest("half", "<dots>2</dots>"), rest("eighth"))));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    render(*document);

    struct Expected {
        Fraction tick;
        size_t lines;
        size_t dots;
    };

    // "0." for the dotted quarter and the dotted eighth; "0 - - ." for the 3.5
    // quarters of the double-dotted half.
    for (const auto &[tick, lines, dots] : {Expected{{0, 1}, 0, 1}, Expected{{3, 8}, 0, 1}, Expected{{1, 1}, 2, 1}}) {
        SCOPED_TRACE(tick.ticks());
        const auto *r = to_rest(chord_rest_at(master, tick));
        ASSERT_EQ(r->jianpu_duration_lines().size(), lines);
        ASSERT_EQ(r->dot_list().size(), dots);
        double marks_right = r->ldata()->jianpu_digit_box.right();
        for (const JianpuDurationLine *line : r->jianpu_duration_lines())
            marks_right = std::max(marks_right, line->pos().x() + line->ldata()->bbox().right());
        for (const NoteDot *dot : r->dot_list()) {
            EXPECT_TRUE(dot->visible());
            EXPECT_GT(dot->pos().x(), marks_right);
            EXPECT_DOUBLE_EQ(dot->pos().y(), 0.0) << "on the centre line of the digit";
            EXPECT_GT(r->shape().bbox().right(), dot->pos().x());
        }
    }
}

TEST(Jianpu, RestsInABeamGroupKeepTheirOwnUnderlines) {
    constexpr std::string_view begin = "<BeamMode>begin</BeamMode>";
    constexpr std::string_view mid = "<BeamMode>mid</BeamMode>";
    // A sixteenth rest first, then one between a sixteenth and an eighth.
    auto document = open_text(jianpu_score(std::format(
        "<Measure><voice>{}{}{}{}{}{}</voice></Measure><Measure><voice>{}{}{}{}{}</voice></Measure>", four_four, rest("16th", begin),
        chord("eighth", 62, 16, mid), chord("16th", 64, 18, mid), chord("quarter", 65, 13), chord("half", 67, 15),
        chord("16th", 62, 16, begin), rest("16th", mid), chord("eighth", 64, 18, mid), chord("quarter", 65, 13), chord("half", 67, 15))));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    render(*document);

    using Segments = std::vector<std::tuple<int, int, int>>;
    const auto segments = [&](Fraction tick) {
        Segments result;
        const Beam *beam = chord_rest_at(master, tick)->beam();
        if (!beam) {
            ADD_FAILURE() << "no beam at " << tick.ticks();
            return result;
        }
        for (const BeamSegment *segment : beam->beam_segments())
            result.emplace_back(segment->level, segment->start_tick.ticks(), segment->end_tick.ticks());
        std::ranges::sort(result);
        return result;
    };
    // Ticks of 480 per quarter. Each underline level spans every chord or rest at
    // least that short, and one alone gets a full-width underline.
    EXPECT_EQ(segments({0, 1}), (Segments{{0, 0, 360}, {1, 0, 0}, {1, 360, 360}}));
    EXPECT_EQ(segments({1, 1}), (Segments{{0, 1920, 2160}, {1, 1920, 2040}}));
}

TEST(Jianpu, VoiceBandsFollowTheStaffTypeOfEachMeasure) {
    const auto voices = std::format("<voice>{}</voice><voice>{}</voice>", chord("whole", 67, 15), chord("whole", 60, 14));
    const auto voices_after_time =
        std::format("<voice>{}{}</voice><voice>{}</voice>", four_four, chord("whole", 67, 15), chord("whole", 60, 14));
    const auto source = [&](bool jianpu_first, bool jianpu_second) {
        const auto change = jianpu_first == jianpu_second ? std::string() : staff_type_change(jianpu_second ? jianpu_type : pitched_type);
        auto xml = jianpu_score(std::format("<Measure>{}</Measure><Measure>{}{}</Measure>", voices_after_time, change, voices));
        if (!jianpu_first) {
            const auto start = xml.find("<StaffType group=\"jianpu\">");
            const auto end = xml.find("</StaffType>", start) + std::string_view("</StaffType>").size();
            xml.replace(start, end - start, pitched_type);
        }
        return xml;
    };
    const auto baseline = [](ChordRest *cr) { return to_chord(cr)->up_note()->ldata()->pos().y(); };

    auto pitched = open_text(source(false, false));
    ASSERT_TRUE(pitched);
    render(*pitched);
    for (const bool jianpu_first : {false, true}) {
        SCOPED_TRACE(jianpu_first ? "Jianpu to pitched" : "pitched to Jianpu");
        auto mixed = open_text(source(jianpu_first, !jianpu_first));
        ASSERT_TRUE(mixed);
        auto &master = master_of(*mixed);
        render(*mixed);
        ASSERT_EQ(master.first_measure()->system(), master.first_measure()->next_measure()->system());
        const Fraction jianpu_tick = jianpu_first ? Fraction(0, 1) : Fraction(1, 1);
        const Fraction pitched_tick = jianpu_first ? Fraction(1, 1) : Fraction(0, 1);
        ASSERT_TRUE(master.staff(0)->is_jianpu_staff(jianpu_tick));
        ASSERT_FALSE(master.staff(0)->is_jianpu_staff(pitched_tick));

        EXPECT_GT(baseline(chord_rest_at(master, jianpu_tick, 1)) - baseline(chord_rest_at(master, jianpu_tick, 0)),
                  master.style().style_absolute(Sid::JIANPU_VOICE_VERTICAL_DISTANCE));
        // The pitched measure keeps its notes on their staff lines, as on a pitched staff.
        for (const track_idx_t track : {track_idx_t(0), track_idx_t(1)})
            EXPECT_DOUBLE_EQ(baseline(chord_rest_at(master, pitched_tick, track)),
                             baseline(chord_rest_at(master_of(*pitched), pitched_tick, track)))
                << track;
    }
}

TEST(Jianpu, GeneratedMarksFollowTheVisibilityOfTheirOwners) {
    // Pitch 72 in C is "1" with two dots above, and a half has one line.
    auto document = open_text(jianpu_score(std::format("<Measure><voice>{}{}{}</voice></Measure><Measure><voice>{}{}</voice></Measure>",
                                                       four_four, chord("half", 72, 14, "", "<visible>0</visible>"),
                                                       rest("half", "<visible>0</visible>"), chord("half", 72, 14), rest("half"))));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto marks_of = [](ChordRest *cr) {
        std::vector<const EngravingItem *> marks(cr->jianpu_duration_lines().begin(), cr->jianpu_duration_lines().end());
        if (cr->is_chord())
            for (const Note *note : to_chord(cr)->notes())
                marks.insert(marks.end(), note->jianpu_octave_dots().begin(), note->jianpu_octave_dots().end());
        return marks;
    };
    render(*document);
    ChordRest *hidden_chord = chord_rest_at(master, {0, 1});
    ChordRest *hidden_rest = chord_rest_at(master, {1, 2});
    ChordRest *shown_chord = chord_rest_at(master, {1, 1});
    ChordRest *gap = chord_rest_at(master, {3, 2});
    for (ChordRest *cr : {hidden_chord, shown_chord}) ASSERT_EQ(marks_of(cr).size(), 3u);
    for (ChordRest *cr : {hidden_rest, gap}) ASSERT_EQ(marks_of(cr).size(), 1u);
    for (ChordRest *cr : {hidden_chord, hidden_rest})
        for (const EngravingItem *mark : marks_of(cr)) EXPECT_FALSE(mark->visible()) << mark->type_name();
    for (ChordRest *cr : {shown_chord, gap})
        for (const EngravingItem *mark : marks_of(cr)) EXPECT_TRUE(mark->visible()) << mark->type_name();

    // A gap rest is not drawn; the marks of its layout as a rest go with it.
    to_rest(gap)->set_gap(true);
    master.invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
    render(*document);
    EXPECT_TRUE(marks_of(gap).empty());
    master.scan_elements([&](EngravingItem *item) {
        if (item->is_jianpu_duration_line()) EXPECT_NE(to_jianpu_duration_line(item)->chord_rest(), gap);
    });
}

TEST(Jianpu, AccidentalsTakeTheirSpaceBeforeTheDigit) {
    std::string run;
    for (int i = 0; i < 4; ++i) run += chord("32nd", 60, 14) + chord("32nd", 66, 20); // C, then F sharp: "♯4"
    auto document = open_text(jianpu_score(
        std::format("<Measure><voice>{}{}{}{}</voice></Measure>", four_four, run, chord("quarter", 64, 18), chord("half", 67, 15))));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    render(*document);

    int sharps = 0;
    const Note *previous = nullptr;
    for (ChordRest *cr : chord_rests(master, 0)) {
        const Note *note = to_chord(cr)->up_note();
        const Note::LayoutData *ldata = note->ldata();
        if (!ldata->jianpu_accidental.is_empty()) {
            ++sharps;
            EXPECT_EQ(ldata->jianpu_accidental, u"♯");
            const RectF accidental = ldata->jianpu_accidental_box;
            ASSERT_TRUE(accidental.is_valid());
            EXPECT_LE(accidental.right(), ldata->jianpu_digit_baseline.x());
            EXPECT_LE(note->shape().bbox().left(), accidental.left());
            EXPECT_LE(cr->shape().bbox().left(), accidental.left() + note->pos().x());
            // Spacing keeps the previous digit clear of the accidental.
            ASSERT_NE(previous, nullptr);
            EXPECT_LE(previous->page_pos().x() + previous->ldata()->jianpu_digit_box.right(), note->page_pos().x() + accidental.left());
        }
        // The digit draws in its own box, wherever the shape reaches.
        EXPECT_DOUBLE_EQ(ldata->jianpu_digit_baseline.y(), ldata->jianpu_digit_box.bottom());
        previous = note;
    }
    EXPECT_EQ(sharps, 4);
}

TEST(Jianpu, TablatureRelayoutDropsTheJianpuMarks) {
    constexpr std::string_view guitar = R"(<?xml version="1.0" encoding="UTF-8"?>
<museScore version="5.00"><Score><Division>480</Division>
<Part id="1"><Staff><StaffType group="jianpu"><name>jianpu</name><lines>0</lines></StaffType></Staff>
<Instrument id="classical-guitar"><instrumentId>strings.guitar.nylon</instrumentId>
<StringData><frets>19</frets><string>40</string><string>45</string><string>50</string><string>55</string><string>59</string><string>64</string></StringData>
</Instrument></Part>
<Staff id="1"><Measure><voice><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>
<Chord><durationType>half</durationType><Note><pitch>76</pitch><tpc>18</tpc><string>0</string><fret>12</fret></Note></Chord>
<Rest><durationType>half</durationType></Rest>
</voice></Measure></Staff></Score></museScore>)";
    auto document = open_text(guitar);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    render(*document);
    auto *chord = to_chord(chord_rest_at(master, {0, 1}));
    ChordRest *r = chord_rest_at(master, {1, 2});
    ASSERT_FALSE(chord->up_note()->jianpu_octave_dots().empty());
    ASSERT_EQ(chord->jianpu_duration_lines().size(), 1u);
    ASSERT_EQ(r->jianpu_duration_lines().size(), 1u);

    // The relayout reuses the layout data of the Jianpu pass.
    master.staff(0)->set_staff_type(Fraction(0, 1), *StaffType::preset(StaffTypes::TAB_6COMMON));
    master.invalidate_layout(Fraction(0, 1), Fraction(-1, 1));
    render(*document);
    ASSERT_TRUE(master.staff(0)->is_tab_staff(Fraction(0, 1)));
    EXPECT_TRUE(chord->up_note()->jianpu_octave_dots().empty());
    EXPECT_TRUE(chord->jianpu_duration_lines().empty());
    EXPECT_TRUE(r->jianpu_duration_lines().empty());
    master.scan_elements([](EngravingItem *item) { EXPECT_FALSE(item->is_jianpu_octave_dot() || item->is_jianpu_duration_line()); });
}

TEST(Jianpu, DraggedBeamDrawsFlatAndKeepsItsAuthoredEnds) {
    auto xml = read_text(jianpu_fixture());
    constexpr std::string_view beam_id = "<eid>A_E</eid>";
    xml.insert(xml.find(beam_id) + beam_id.size(), "<Fragment><y1>1.25</y1><y2>2.5</y2></Fragment>");
    auto document = open_text(xml);
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    const auto cold_native = native_captures(*document);
    render(*document);

    Beam *beam = chord_rests(master, 0).front()->beam();
    ASSERT_NE(beam, nullptr);
    const auto *dragged = beam->authored_overrides.find({AuthoredChildRole::BEAM_FRAGMENT}, Pid::BEAM_POS);
    ASSERT_NE(dragged, nullptr);
    EXPECT_DOUBLE_EQ(beam->slope(), 0.0);
    ASSERT_FALSE(beam->beam_segments().empty());
    for (const BeamSegment *segment : beam->beam_segments()) EXPECT_DOUBLE_EQ(segment->line.y1(), segment->line.y2());
    const auto ends = dragged->value.value<PairF>();
    EXPECT_DOUBLE_EQ(ends.first, 1.25);
    EXPECT_DOUBLE_EQ(ends.second, 2.5);
    EXPECT_EQ(native_captures(*document), cold_native);
}

TEST(Jianpu, KeyEditsKeepTheCustomNumbering) {
    auto document = open_text(jianpu_source_with("<KeySig><eid>Z_Z</eid><concertKey>2</concertKey>"
                                                 "<jianpuNumbering>custom</jianpuNumbering><jianpuTonicKey>Eb</jianpuTonicKey>"
                                                 "<jianpuTonicMode>phrygian</jianpuTonicMode></KeySig>"));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *keysig = to_key_sig(master.first_measure()->find_segment(SegmentType::KEY_SIG, Fraction(0, 1))->element(0));
    const auto id = engine_id(*keysig);
    for (const auto &[property, key] : {std::pair{"concertKey", 3}, std::pair{"actualKey", 4}}) {
        SCOPED_TRACE(property);
        const auto edited = document->edit({{engrave::SetProperty{id, property, std::int64_t(key)}}});
        ASSERT_TRUE(edited) << edited.error().message;
        EXPECT_EQ(keysig->jianpu_numbering(), JianpuNumbering::CUSTOM);
        EXPECT_EQ(keysig->jianpu_tonic_key(), Key::E_B);
        EXPECT_EQ(keysig->jianpu_tonic_mode(), KeyMode::PHRYGIAN);
        EXPECT_EQ(master.staff(0)->key_sig_event(Fraction(0, 1)), keysig->key_sig_event());
        render(*document);
        EXPECT_EQ(keysig->ldata()->jianpu_label.to_std_string(), "3=E♭");

        auto reopened = reopen(*document);
        ASSERT_TRUE(reopened);
        const KeySigEvent saved = master_of(*reopened).staff(0)->key_sig_event(Fraction(0, 1));
        EXPECT_EQ(saved, keysig->key_sig_event());
        EXPECT_EQ(saved.jianpu_tonic_key(), Key::E_B);
    }
    EXPECT_EQ(keysig->concert_key(), Key(3));
    EXPECT_EQ(keysig->key(), Key(4));
}

TEST(Jianpu, InactiveTonicIsSavedAndDefaultsWriteNothing) {
    const auto keysig_xml = [](const std::string &written) {
        const auto at = written.find("<KeySig");
        return at == std::string::npos ? std::string() : written.substr(at, written.find("</KeySig>", at) - at);
    };
    auto plain = open_text(jianpu_source_with("<KeySig><eid>Z_Z</eid><concertKey>2</concertKey></KeySig>"));
    ASSERT_TRUE(plain);
    const auto plain_keysig = keysig_xml(export_mscx(*plain));
    ASSERT_FALSE(plain_keysig.empty());
    EXPECT_EQ(plain_keysig.find("jianpu"), std::string::npos) << plain_keysig;

    auto document =
        open_text(jianpu_source_with("<KeySig><eid>Z_Z</eid><concertKey>2</concertKey>"
                                     "<jianpuTonicKey>Eb</jianpuTonicKey><jianpuTonicMode>phrygian</jianpuTonicMode></KeySig>"));
    ASSERT_TRUE(document);
    auto &master = master_of(*document);
    auto *keysig = to_key_sig(master.first_measure()->find_segment(SegmentType::KEY_SIG, Fraction(0, 1))->element(0));
    EXPECT_EQ(keysig->jianpu_numbering(), JianpuNumbering::FOLLOW_SCORE_KEY);
    render(*document);
    EXPECT_EQ(keysig->ldata()->jianpu_label.to_std_string(), "1=D") << "the inactive tonic leaves the score key in charge";

    const auto written = keysig_xml(export_mscx(*document));
    EXPECT_EQ(written.find("<jianpuNumbering>"), std::string::npos) << written;
    EXPECT_NE(written.find("<jianpuTonicKey>Eb</jianpuTonicKey>"), std::string::npos) << written;
    EXPECT_NE(written.find("<jianpuTonicMode>phrygian</jianpuTonicMode>"), std::string::npos) << written;
    auto reopened = reopen(*document);
    ASSERT_TRUE(reopened);
    EXPECT_EQ(master_of(*reopened).staff(0)->key_sig_event(Fraction(0, 1)), keysig->key_sig_event());

    // Switching to a custom numbering picks up the kept tonic: Eb Phrygian numbers its tonic 3.
    const auto edited = document->edit({{engrave::SetProperty{engine_id(*keysig), "jianpuNumbering", engrave::Enum{"custom"}}}});
    ASSERT_TRUE(edited) << edited.error().message;
    render(*document);
    EXPECT_EQ(keysig->ldata()->jianpu_label.to_std_string(), "3=E♭");
}

TEST(Jianpu, DescribeListsTheJianpuKeySignatureProperties) {
    const auto keysig = g_engine->describe("KeySig");
    ASSERT_TRUE(keysig) << keysig.error().message;
    const auto find = [&](std::string_view name) -> const engrave::PropInfo * {
        const auto it = std::ranges::find(keysig->props, name, &engrave::PropInfo::name);
        return it == keysig->props.end() ? nullptr : &*it;
    };
    const auto *numbering = find("jianpuNumbering");
    ASSERT_NE(numbering, nullptr);
    EXPECT_TRUE(numbering->settable);
    EXPECT_EQ(numbering->enum_tokens, (std::vector<std::string>{"followScoreKey", "custom"}));
    const auto *tonic = find("jianpuTonicKey");
    ASSERT_NE(tonic, nullptr);
    EXPECT_EQ(tonic->enum_tokens.size(), 15u);
    EXPECT_NE(std::ranges::find(tonic->enum_tokens, "F#"), tonic->enum_tokens.end());
    EXPECT_NE(find("jianpuTonicMode"), nullptr);
    for (const auto type : {"JianpuOctaveDot", "JianpuDurationLine"}) EXPECT_TRUE(g_engine->describe(type)) << type;
}
