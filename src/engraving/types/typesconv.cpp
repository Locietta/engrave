/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "typesconv.h"

#include "draw/types/drawtypes.h"
#include "global/types/translatablestring.h"
#include "log.h"
#include "symnames.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

template <typename T>
struct Item {
    T type;
    AsciiStringView xml;
    TranslatableString user_name;
    TranslatableString capitalized_user_name;

    // NOTE Ideally we would write `TranslatableString userName = {};` and omit these constructors
    // But that causes internal compiler errors with certain versions of GCC/MinGW
    // See discussion at https://github.com/musescore/MuseScore/pull/12612

    Item() = default;

    Item(T type, AsciiStringView xml) : type(type), xml(xml) {}

    Item(T type, AsciiStringView xml, const TranslatableString &capitalized_user_name)
        : type(type), xml(xml), capitalized_user_name(capitalized_user_name) {}

    Item(T type, AsciiStringView xml, const TranslatableString &user_name, const TranslatableString &capitalized_user_name)
        : type(type), xml(xml), user_name(user_name), capitalized_user_name(capitalized_user_name) {}
};

template <typename T, typename C>
static const Item<T> &find_item_by_type(const C &cont, const T &type) {
    const auto it = std::find_if(cont.cbegin(), cont.cend(), [&](const Item<T> &item) { return item.type == type; });

    IF_ASSERT_FAILED(it != cont.cend()) {
        static Item<T> dummy;
        return dummy;
    }

    return *it;
}

template <typename T, typename C>
static const TranslatableString &find_user_name_by_type(const C &cont, const T &type) {
    const Item<T> &item = find_item_by_type(cont, type);

    return item.user_name;
}

template <typename T, typename C>
static const TranslatableString &find_capitalized_user_name_by_type(const C &cont, const T &type) {
    const Item<T> &item = find_item_by_type(cont, type);

    return item.capitalized_user_name;
}

template <typename T, typename C>
static AsciiStringView find_xml_tag_by_type(const C &cont, const T &v) {
    const Item<T> &item = find_item_by_type(cont, v);

    return item.xml;
}

template <typename T, typename C>
static T find_type_by_xml_tag(const C &cont, const String &tag, T def) {
    muse::ByteArray ba = tag.to_ascii();
    auto it = std::find_if(cont.cbegin(), cont.cend(), [ba](const Item<T> &i) { return i.xml == ba.const_char(); });

    IF_ASSERT_FAILED(it != cont.cend()) { return def; }
    return it->type;
}

template <typename T, typename C>
static T find_type_by_xml_tag(const C &cont, const AsciiStringView &tag, T def, bool silent = false) {
    auto it = std::find_if(cont.cbegin(), cont.cend(), [tag](const Item<T> &i) { return i.xml == tag; });

    if (it == cont.cend()) {
        if (!silent) {
            LOGE() << "not found type for tag: " << tag;
            //assert(it != cont.cend());
        }
        return def;
    }

    return it->type;
}

// ==========================================================
String TConv::to_xml(const std::vector<int> &v) {
    StringList sl;
    for (int i : v) {
        sl << String::number(i);
    }
    return sl.join(u",");
}

std::vector<int> TConv::from_xml(const String &tag, const std::vector<int> &def) {
    std::vector<int> list;
    StringList sl = tag.split(u',', muse::SKIP_EMPTY_PARTS);
    for (const String &s : sl) {
        bool ok = false;
        int i = s.simplified().to_int(&ok);
        if (!ok) {
            return def;
        }
        list.push_back(i);
    }
    return list;
}

String TConv::to_xml(const std::vector<string_idx_t> &v) {
    std::vector<int> v_value;
    v_value.reserve(v.size());
    for (string_idx_t string : v) {
        v_value.push_back(static_cast<int>(string));
    }

    return to_xml(v_value);
}

std::vector<string_idx_t> TConv::from_xml(const String &tag, const std::vector<string_idx_t> &def) {
    std::vector<int> def_value;
    def_value.reserve(def.size());
    for (string_idx_t string : def) {
        def_value.push_back(static_cast<int>(string));
    }

    std::vector<string_idx_t> v;
    std::vector<int> v_value = from_xml(tag, def_value);
    v.reserve(v_value.size());

    for (int string : v_value) {
        v.push_back(static_cast<string_idx_t>(string));
    }

    return v;
}

static const std::array element_types{
    Item{ElementType::INVALID, "invalid", TranslatableString("engraving", "invalid", nullptr, 1),
         TranslatableString("engraving", "Invalid", nullptr, 1)},
    Item{ElementType::BRACKET_ITEM, "BracketItem", TranslatableString("engraving", "bracket(s)", nullptr, 1),
         TranslatableString("engraving", "Bracket(s)", nullptr, 1)},
    Item{ElementType::PART, "Part", TranslatableString("engraving", "part(s)", nullptr, 1),
         TranslatableString("engraving", "Part(s)", nullptr, 1)},
    Item{ElementType::SHARED_PART, "SharedPart", TranslatableString("engraving", "shared part(s)", nullptr, 1),
         TranslatableString("engraving", "Shared part(s)", nullptr, 1)},
    Item{ElementType::STAFF, "Staff", TranslatableString("engraving", "staff/staves", nullptr, 1),
         TranslatableString("engraving", "Staff/Staves", nullptr, 1)},
    Item{ElementType::SCORE, "Score", TranslatableString("engraving", "score(s)", nullptr, 1),
         TranslatableString("engraving", "Score(s)", nullptr, 1)},
    Item{ElementType::TEXT, "Text", TranslatableString("engraving", "text", nullptr, 1),
         TranslatableString("engraving", "Text", nullptr, 1)},
    Item{ElementType::MEASURE_NUMBER, "MeasureNumber", TranslatableString("engraving", "measure number(s)", nullptr, 1),
         TranslatableString("engraving", "Measure number(s)", nullptr, 1)},
    Item{ElementType::MMREST_RANGE, "MMRestRange", TranslatableString("engraving", "multimeasure rest range(s)", nullptr, 1),
         TranslatableString("engraving", "Multimeasure rest range(s)", nullptr, 1)},
    Item{ElementType::INSTRUMENT_NAME, "InstrumentName", TranslatableString("engraving", "instrument name(s)", nullptr, 1),
         TranslatableString("engraving", "Instrument name(s)", nullptr, 1)},
    Item{ElementType::SLUR_SEGMENT, "SlurSegment", TranslatableString("engraving", "slur segment(s)", nullptr, 1),
         TranslatableString("engraving", "Slur segment(s)", nullptr, 1)},
    Item{ElementType::TIE_SEGMENT, "TieSegment", TranslatableString("engraving", "tie segment(s)", nullptr, 1),
         TranslatableString("engraving", "Tie segment(s)", nullptr, 1)},
    Item{ElementType::LAISSEZ_VIB_SEGMENT, "LaissezVibSegment", TranslatableString("engraving", "laissez vibrer segment(s)", nullptr, 1),
         TranslatableString("engraving", "Laissez vibrer segment(s)", nullptr, 1)},
    Item{ElementType::PARTIAL_TIE_SEGMENT, "PartialTieSegment", TranslatableString("engraving", "partial tie segment(s)", nullptr, 1),
         TranslatableString("engraving", "Partial tie segment(s)", nullptr, 1)},
    Item{ElementType::BAR_LINE, "BarLine", TranslatableString("engraving", "barline(s)", nullptr, 1),
         TranslatableString("engraving", "Barline(s)", nullptr, 1)},
    Item{ElementType::STAFF_LINES, "StaffLines", TranslatableString("engraving", "staff lines", nullptr, 1),
         TranslatableString("engraving", "Staff lines", nullptr, 1)},
    Item{ElementType::SYSTEM_DIVIDER, "SystemDivider", TranslatableString("engraving", "system divider(s)", nullptr, 1),
         TranslatableString("engraving", "System divider(s)", nullptr, 1)},
    Item{ElementType::STEM_SLASH, "StemSlash", TranslatableString("engraving", "stem slash(es)", nullptr, 1),
         TranslatableString("engraving", "Stem slash(es)", nullptr, 1)},
    Item{ElementType::ARPEGGIO, "Arpeggio", TranslatableString("engraving", "arpeggio(s)", nullptr, 1),
         TranslatableString("engraving", "Arpeggio(s)", nullptr, 1)},
    Item{ElementType::CHORD_BRACKET, "ChordBracket", TranslatableString("engraving", "chord bracket(s)", nullptr, 1),
         TranslatableString("engraving", "Chord bracket(s)", nullptr, 1)},
    Item{ElementType::ACCIDENTAL, "Accidental", TranslatableString("engraving", "accidental(s)", nullptr, 1),
         TranslatableString("engraving", "Accidental(s)", nullptr, 1)},
    Item{ElementType::LEDGER_LINE, "LedgerLine", TranslatableString("engraving", "ledger line(s)", nullptr, 1),
         TranslatableString("engraving", "Ledger line(s)", nullptr, 1)},
    Item{ElementType::STEM, "Stem", TranslatableString("engraving", "stem(s)", nullptr, 1),
         TranslatableString("engraving", "Stem(s)", nullptr, 1)},
    Item{ElementType::HOOK, "Hook", // internally called "Hook", but "Flag" in SMuFL, so here externally too
         TranslatableString("engraving", "flag(s)", nullptr, 1), TranslatableString("engraving", "Flag(s)", nullptr, 1)},
    Item{ElementType::NOTE, "Note", TranslatableString("engraving", "note(s)", nullptr, 1),
         TranslatableString("engraving", "Note(s)", nullptr, 1)},
    Item{ElementType::CLEF, "Clef", TranslatableString("engraving", "clef(s)", nullptr, 1),
         TranslatableString("engraving", "Clef(s)", nullptr, 1)},
    Item{ElementType::KEYSIG, "KeySig", TranslatableString("engraving", "key signature(s)", nullptr, 1),
         TranslatableString("engraving", "Key signature(s)", nullptr, 1)},
    Item{ElementType::AMBITUS, "Ambitus", TranslatableString("engraving", "ambitus", nullptr, 1),
         TranslatableString("engraving", "Ambitus", nullptr, 1)},
    Item{ElementType::TIMESIG, "TimeSig", TranslatableString("engraving", "time signature(s)", nullptr, 1),
         TranslatableString("engraving", "Time signature(s)", nullptr, 1)},
    Item{ElementType::REST, "Rest", TranslatableString("engraving", "rest(s)", nullptr, 1),
         TranslatableString("engraving", "Rest(s)", nullptr, 1)},
    Item{ElementType::MMREST, "MMRest", TranslatableString("engraving", "multimeasure rest(s)", nullptr, 1),
         TranslatableString("engraving", "Multimeasure rest(s)", nullptr, 1)},
    Item{ElementType::DEAD_SLAPPED, "DeadSlapped", TranslatableString("engraving", "dead slapped", nullptr, 1),
         TranslatableString("engraving", "Dead slapped", nullptr, 1)},
    Item{ElementType::SYMBOL, "Symbol", TranslatableString("engraving", "symbol(s)", nullptr, 1),
         TranslatableString("engraving", "Symbol(s)", nullptr, 1)},
    Item{ElementType::BREATH, "Breath", TranslatableString("engraving", "breath(s)", nullptr, 1),
         TranslatableString("engraving", "Breath(s)", nullptr, 1)},
    Item{ElementType::MEASURE_REPEAT, "MeasureRepeat", TranslatableString("engraving", "measure repeat(s)", nullptr, 1),
         TranslatableString("engraving", "Measure repeat(s)", nullptr, 1)},
    Item{ElementType::TIE, "Tie", TranslatableString("engraving", "tie(s)", nullptr, 1),
         TranslatableString("engraving", "Tie(s)", nullptr, 1)},
    Item{ElementType::LAISSEZ_VIB, "LaissezVib", TranslatableString("engraving", "laissez vibrer(s)", nullptr, 1),
         TranslatableString("engraving", "Laissez vibrer(s)", nullptr, 1)},
    Item{ElementType::PARTIAL_TIE, "PartialTie", TranslatableString("engraving", "partial tie(s)", nullptr, 1),
         TranslatableString("engraving", "Partial tie(s)", nullptr, 1)},
    Item{ElementType::ARTICULATION, "Articulation", TranslatableString("engraving", "articulation(s)", nullptr, 1),
         TranslatableString("engraving", "Articulation(s)", nullptr, 1)},
    Item{ElementType::ORNAMENT, "Ornament", TranslatableString("engraving", "ornament(s)", nullptr, 1),
         TranslatableString("engraving", "Ornament(s)", nullptr, 1)},
    Item{ElementType::FERMATA, "Fermata", TranslatableString("engraving", "fermata(s)", nullptr, 1),
         TranslatableString("engraving", "Fermata(s)", nullptr, 1)},
    Item{ElementType::CHORDLINE, "ChordLine", TranslatableString("engraving", "chord line(s)", nullptr, 1),
         TranslatableString("engraving", "Chord line(s)", nullptr, 1)},
    Item{ElementType::DYNAMIC, "Dynamic", TranslatableString("engraving", "dynamic(s)", nullptr, 1),
         TranslatableString("engraving", "Dynamic(s)", nullptr, 1)},
    Item{ElementType::EXPRESSION, "Expression", TranslatableString("engraving", "expression(s)", nullptr, 1),
         TranslatableString("engraving", "Expression(s)", nullptr, 1)},
    Item{ElementType::BEAM, "Beam", TranslatableString("engraving", "beam(s)", nullptr, 1),
         TranslatableString("engraving", "Beam(s)", nullptr, 1)},
    Item{ElementType::LYRICS, "Lyrics", TranslatableString("engraving", "lyrics", nullptr, 1),
         TranslatableString("engraving", "Lyrics", nullptr, 1)},
    Item{ElementType::FIGURED_BASS, "FiguredBass", TranslatableString("engraving", "figured bass", nullptr, 1),
         TranslatableString("engraving", "Figured bass", nullptr, 1)},
    Item{ElementType::FIGURED_BASS_ITEM, "FiguredBassItem", TranslatableString("engraving", "figured bass item(s)", nullptr, 1),
         TranslatableString("engraving", "Figured bass item(s)", nullptr, 1)},
    Item{ElementType::MARKER, "Marker", TranslatableString("engraving", "marker(s)", nullptr, 1),
         TranslatableString("engraving", "Marker(s)", nullptr, 1)},
    Item{ElementType::JUMP, "Jump", TranslatableString("engraving", "jump(s)", nullptr, 1),
         TranslatableString("engraving", "Jump(s)", nullptr, 1)},
    Item{ElementType::FINGERING, "Fingering", TranslatableString("engraving", "fingering(s)", nullptr, 1),
         TranslatableString("engraving", "Fingering(s)", nullptr, 1)},
    Item{ElementType::TUPLET, "Tuplet", TranslatableString("engraving", "tuplet(s)", nullptr, 1),
         TranslatableString("engraving", "Tuplet(s)", nullptr, 1)},
    Item{ElementType::TEMPO_TEXT, "Tempo", TranslatableString("engraving", "tempo(s)", nullptr, 1),
         TranslatableString("engraving", "Tempo(s)", nullptr, 1)},
    Item{ElementType::STAFF_TEXT, "StaffText", TranslatableString("engraving", "staff text(s)", nullptr, 1),
         TranslatableString("engraving", "Staff text(s)", nullptr, 1)},
    Item{ElementType::STAVE_SHARING_LABEL, "StaveSharingLabel", TranslatableString("engraving", "stave sharing label(s)", nullptr, 1),
         TranslatableString("engraving", "Stave sharing label(s)", nullptr, 1)},
    Item{ElementType::SYSTEM_TEXT, "SystemText", TranslatableString("engraving", "system text(s)", nullptr, 1),
         TranslatableString("engraving", "System text(s)", nullptr, 1)},
    Item{ElementType::SOUND_FLAG, "SoundFlag", TranslatableString("engraving", "sound flag(s)", nullptr, 1),
         TranslatableString("engraving", "Sound flag(s)", nullptr, 1)},
    Item{ElementType::PLAY_COUNT_TEXT, "PlayCountText", TranslatableString("engraving", "play count text(s)", nullptr, 1),
         TranslatableString("engraving", "Play count text(s)", nullptr, 1)},
    Item{ElementType::PLAYTECH_ANNOTATION, "PlayTechAnnotation",
         TranslatableString("engraving", "playing technique annotation(s)", nullptr, 1),
         TranslatableString("engraving", "Playing technique annotation(s)", nullptr, 1)},
    Item{ElementType::CAPO, "Capo", TranslatableString("engraving", "capo(s)", nullptr, 1),
         TranslatableString("engraving", "Capo(s)", nullptr, 1)},
    Item{ElementType::STRING_TUNINGS, "StringTunings", TranslatableString("engraving", "string tunings", nullptr, 1),
         TranslatableString("engraving", "String tunings", nullptr, 1)},
    Item{ElementType::TRIPLET_FEEL, "TripletFeel", TranslatableString("engraving", "triplet feel", nullptr, 1),
         TranslatableString("engraving", "Triplet feel", nullptr, 1)},
    Item{ElementType::REHEARSAL_MARK, "RehearsalMark", TranslatableString("engraving", "rehearsal mark(s)", nullptr, 1),
         TranslatableString("engraving", "Rehearsal mark(s)", nullptr, 1)},
    Item{ElementType::INSTRUMENT_CHANGE, "InstrumentChange", TranslatableString("engraving", "instrument change(s)", nullptr, 1),
         TranslatableString("engraving", "Instrument change(s)", nullptr, 1)},
    Item{ElementType::STAFFTYPE_CHANGE, "StaffTypeChange", TranslatableString("engraving", "staff type change(s)", nullptr, 1),
         TranslatableString("engraving", "Staff type change(s)", nullptr, 1)},
    Item{ElementType::HARMONY, "Harmony", TranslatableString("engraving", "chord symbol(s)", nullptr, 1),
         TranslatableString("engraving", "Chord symbol(s)", nullptr, 1)},
    Item{ElementType::FRET_DIAGRAM, "FretDiagram", TranslatableString("engraving", "fretboard diagram(s)", nullptr, 1),
         TranslatableString("engraving", "Fretboard diagram(s)", nullptr, 1)},
    Item{ElementType::HARP_DIAGRAM, "HarpPedalDiagram", TranslatableString("engraving", "harp pedal diagram(s)", nullptr, 1),
         TranslatableString("engraving", "Harp pedal diagram(s)", nullptr, 1)},
    Item{ElementType::BEND, "Bend", TranslatableString("engraving", "bend(s)", nullptr, 1),
         TranslatableString("engraving", "Bend(s)", nullptr, 1)},
    Item{ElementType::TREMOLOBAR, "TremoloBar", TranslatableString("engraving", "tremolo bar(s)", nullptr, 1),
         TranslatableString("engraving", "Tremolo bar(s)", nullptr, 1)},
    Item{ElementType::VOLTA, "Volta", TranslatableString("engraving", "volta(s)", nullptr, 1),
         TranslatableString("engraving", "Volta(s)", nullptr, 1)},
    Item{ElementType::HAIRPIN_SEGMENT, "HairpinSegment", TranslatableString("engraving", "hairpin segment(s)", nullptr, 1),
         TranslatableString("engraving", "Hairpin segment(s)", nullptr, 1)},
    Item{ElementType::OTTAVA_SEGMENT, "OttavaSegment", TranslatableString("engraving", "ottava segment(s)", nullptr, 1),
         TranslatableString("engraving", "Ottava segment(s)", nullptr, 1)},
    Item{ElementType::TRILL_SEGMENT, "TrillSegment", TranslatableString("engraving", "trill segment(s)", nullptr, 1),
         TranslatableString("engraving", "Trill segment(s)", nullptr, 1)},
    Item{ElementType::LET_RING_SEGMENT, "LetRingSegment", TranslatableString("engraving", "let ring segment(s)", nullptr, 1),
         TranslatableString("engraving", "Let ring segment(s)", nullptr, 1)},
    Item{ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT, "GradualTempoChangeSegment",
         TranslatableString("engraving", "gradual tempo change segment(s)", nullptr, 1),
         TranslatableString("engraving", "Gradual tempo change segment(s)", nullptr, 1)},
    Item{ElementType::VIBRATO_SEGMENT, "VibratoSegment", TranslatableString("engraving", "vibrato segment(s)", nullptr, 1),
         TranslatableString("engraving", "Vibrato segment(s)", nullptr, 1)},
    Item{ElementType::PALM_MUTE_SEGMENT, "PalmMuteSegment", TranslatableString("engraving", "palm mute segment(s)", nullptr, 1),
         TranslatableString("engraving", "Palm mute segment(s)", nullptr, 1)},
    Item{ElementType::WHAMMY_BAR_SEGMENT, "WhammyBarSegment", TranslatableString("engraving", "whammy bar segment(s)", nullptr, 1),
         TranslatableString("engraving", "Whammy bar segment(s)", nullptr, 1)},
    Item{ElementType::RASGUEADO_SEGMENT, "RasgueadoSegment", TranslatableString("engraving", "rasgueado segment(s)", nullptr, 1),
         TranslatableString("engraving", "Rasgueado segment(s)", nullptr, 1)},
    Item{ElementType::HARMONIC_MARK_SEGMENT, "HarmonicMarkSegment", TranslatableString("engraving", "harmonic mark segment(s)", nullptr, 1),
         TranslatableString("engraving", "Harmonic mark segment(s)", nullptr, 1)},
    Item{ElementType::PICK_SCRAPE_SEGMENT, "PickScrapeSegment", TranslatableString("engraving", "pick scrape segment(s)", nullptr, 1),
         TranslatableString("engraving", "Pick scrape segment(s)", nullptr, 1)},
    Item{ElementType::TEXTLINE_SEGMENT, "TextLineSegment", TranslatableString("engraving", "text line segment(s)", nullptr, 1),
         TranslatableString("engraving", "Text line segment(s)", nullptr, 1)},
    Item{ElementType::VOLTA_SEGMENT, "VoltaSegment", TranslatableString("engraving", "volta segment(s)", nullptr, 1),
         TranslatableString("engraving", "Volta segment(s)", nullptr, 1)},
    Item{ElementType::PEDAL_SEGMENT, "PedalSegment", TranslatableString("engraving", "pedal segment(s)", nullptr, 1),
         TranslatableString("engraving", "Pedal segment(s)", nullptr, 1)},
    Item{ElementType::LYRICSLINE_SEGMENT, "LyricsLineSegment", TranslatableString("engraving", "extension line segment(s)", nullptr, 1),
         TranslatableString("engraving", "Extension line segment(s)", nullptr, 1)},
    Item{ElementType::PARTIAL_LYRICSLINE_SEGMENT, "PartialLyricsLineSegment",
         TranslatableString("engraving", "partial extension line segment(s)", nullptr, 1),
         TranslatableString("engraving", "Partial extension line segment(s)", nullptr, 1)},
    Item{ElementType::GLISSANDO_SEGMENT, "GlissandoSegment", TranslatableString("engraving", "glissando segment(s)", nullptr, 1),
         TranslatableString("engraving", "Glissando segment(s)", nullptr, 1)},
    Item{ElementType::NOTELINE_SEGMENT, "NoteLineSegment", TranslatableString("engraving", "note-anchored line segment(s)", nullptr, 1),
         TranslatableString("engraving", "Note-anchored line segment(s)", nullptr, 1)},
    Item{ElementType::LAYOUT_BREAK, "LayoutBreak", TranslatableString("engraving", "layout break(s)", nullptr, 1),
         TranslatableString("engraving", "Layout break(s)", nullptr, 1)},
    Item{ElementType::STAFF_VISIBILITY_INDICATOR, "staffVisibilityIndicator",
         TranslatableString("engraving", "staff visibility icon(s)", nullptr, 1),
         TranslatableString("engraving", "Staff visibility icon(s)", nullptr, 1)},
    Item{ElementType::SYSTEM_LOCK_INDICATOR, "systemLockIndicator", TranslatableString("engraving", "system lock(s)", nullptr, 1),
         TranslatableString("engraving", "System lock(s)", nullptr, 1)},
    Item{ElementType::SPACER, "Spacer", TranslatableString("engraving", "spacer(s)", nullptr, 1),
         TranslatableString("engraving", "Spacer(s)", nullptr, 1)},
    Item{ElementType::STAFF_STATE, "StaffState", TranslatableString("engraving", "staff state(s)", nullptr, 1),
         TranslatableString("engraving", "Staff state(s)", nullptr, 1)},
    Item{ElementType::NOTEHEAD, "NoteHead", TranslatableString("engraving", "notehead(s)", nullptr, 1),
         TranslatableString("engraving", "Notehead(s)", nullptr, 1)},
    Item{ElementType::NOTEDOT, "NoteDot", TranslatableString("engraving", "note dot(s)", nullptr, 1),
         TranslatableString("engraving", "Note dot(s)", nullptr, 1)},
    Item{ElementType::IMAGE, "Image", TranslatableString("engraving", "image(s)", nullptr, 1),
         TranslatableString("engraving", "Image(s)", nullptr, 1)},
    Item{ElementType::MEASURE, "Measure", TranslatableString("engraving", "measure(s)", nullptr, 1),
         TranslatableString("engraving", "Measure(s)", nullptr, 1)},
    Item{ElementType::SELECTION, "Selection", TranslatableString("engraving", "selection(s)", nullptr, 1),
         TranslatableString("engraving", "Selection(s)", nullptr, 1)},
    Item{ElementType::LASSO, "Lasso", TranslatableString("engraving", "lasso(s)", nullptr, 1),
         TranslatableString("engraving", "Lasso(s)", nullptr, 1)},
    Item{ElementType::SHADOW_NOTE, "ShadowNote", TranslatableString("engraving", "shadow note(s)", nullptr, 1),
         TranslatableString("engraving", "Shadow note(s)", nullptr, 1)},
    Item{ElementType::TAB_DURATION_SYMBOL, "TabDurationSymbol", TranslatableString("engraving", "tab duration symbol(s)", nullptr, 1),
         TranslatableString("engraving", "Tab duration symbol(s)", nullptr, 1)},
    Item{ElementType::FSYMBOL, "FSymbol", TranslatableString("engraving", "font symbol(s)", nullptr, 1),
         TranslatableString("engraving", "Font symbol(s)", nullptr, 1)},
    Item{ElementType::PAGE, "Page", TranslatableString("engraving", "page(s)", nullptr, 1),
         TranslatableString("engraving", "Page(s)", nullptr, 1)},
    Item{ElementType::PAGE_LOCK_INDICATOR, "pageLockIndicator", TranslatableString("engraving", "page lock(s)", nullptr, 1),
         TranslatableString("engraving", "Page lock(s)", nullptr, 1)},
    Item{ElementType::PARENTHESIS, "Parenthesis", TranslatableString("engraving", "parenthesis", nullptr, 1),
         TranslatableString("engraving", "Parenthesis", nullptr, 1)},
    Item{ElementType::HAIRPIN, "HairPin", TranslatableString("engraving", "hairpin(s)", nullptr, 1),
         TranslatableString("engraving", "Hairpin(s)", nullptr, 1)},
    Item{ElementType::OTTAVA, "Ottava", TranslatableString("engraving", "ottava(s)", nullptr, 1),
         TranslatableString("engraving", "Ottava(s)", nullptr, 1)},
    Item{ElementType::PEDAL, "Pedal", TranslatableString("engraving", "pedal(s)", nullptr, 1),
         TranslatableString("engraving", "Pedal(s)", nullptr, 1)},
    Item{ElementType::TRILL, "Trill", TranslatableString("engraving", "trill(s)", nullptr, 1),
         TranslatableString("engraving", "Trill(s)", nullptr, 1)},
    Item{ElementType::LET_RING, "LetRing", TranslatableString("engraving", "let ring", nullptr, 1),
         TranslatableString("engraving", "Let ring", nullptr, 1)},
    Item{ElementType::GRADUAL_TEMPO_CHANGE, "GradualTempoChange", TranslatableString("engraving", "gradual tempo change(s)", nullptr, 1),
         TranslatableString("engraving", "Gradual tempo change(s)", nullptr, 1)},
    Item{ElementType::VIBRATO, "Vibrato", TranslatableString("engraving", "vibrato(s)", nullptr, 1),
         TranslatableString("engraving", "Vibrato(s)", nullptr, 1)},
    Item{ElementType::PALM_MUTE, "PalmMute", TranslatableString("engraving", "palm mute(s)", nullptr, 1),
         TranslatableString("engraving", "Palm mute(s)", nullptr, 1)},
    Item{ElementType::WHAMMY_BAR, "WhammyBar", TranslatableString("engraving", "whammy bar(s)", nullptr, 1),
         TranslatableString("engraving", "Whammy bar(s)", nullptr, 1)},
    Item{ElementType::RASGUEADO, "Rasgueado", TranslatableString("engraving", "rasgueado(s)", nullptr, 1),
         TranslatableString("engraving", "Rasgueado(s)", nullptr, 1)},
    Item{ElementType::HARMONIC_MARK, "HarmonicMark", TranslatableString("engraving", "harmonic mark(s)", nullptr, 1),
         TranslatableString("engraving", "Harmonic mark(s)", nullptr, 1)},
    Item{ElementType::PICK_SCRAPE, "PickScrape", TranslatableString("engraving", "pick scrape out(s)", nullptr, 1),
         TranslatableString("engraving", "Pick scrape out(s)", nullptr, 1)},
    Item{ElementType::TEXTLINE, "TextLine", TranslatableString("engraving", "text line(s)", nullptr, 1),
         TranslatableString("engraving", "Text line(s)", nullptr, 1)},
    Item{ElementType::TEXTLINE_BASE, "TextLineBase", TranslatableString("engraving", "text line base(s)", nullptr, 1),
         TranslatableString("engraving", "Text line base(s)", nullptr, 1)}, // remove
    Item{ElementType::NOTELINE, "NoteLine", TranslatableString("engraving", "note-anchored line(s)", nullptr, 1),
         TranslatableString("engraving", "Note-anchored line(s)", nullptr, 1)},
    Item{ElementType::LYRICSLINE, "LyricsLine", TranslatableString("engraving", "extension line(s)", nullptr, 1),
         TranslatableString("engraving", "Extension line(s)", nullptr, 1)},
    Item{ElementType::PARTIAL_LYRICSLINE, "PartialLyricsLine", TranslatableString("engraving", "partial extension line(s)", nullptr, 1),
         TranslatableString("engraving", "Partial extension line(s)", nullptr, 1)},
    Item{ElementType::GLISSANDO, "Glissando", TranslatableString("engraving", "glissando(s)", nullptr, 1),
         TranslatableString("engraving", "Glissando(s)", nullptr, 1)},
    Item{ElementType::BRACKET, "Bracket", TranslatableString("engraving", "bracket(s)", nullptr, 1),
         TranslatableString("engraving", "Bracket(s)", nullptr, 1)},
    Item{ElementType::SEGMENT, "Segment", TranslatableString("engraving", "segment(s)", nullptr, 1),
         TranslatableString("engraving", "Segment(s)", nullptr, 1)},
    Item{ElementType::SYSTEM, "System", TranslatableString("engraving", "system(s)", nullptr, 1),
         TranslatableString("engraving", "System(s)", nullptr, 1)},
    Item{ElementType::CHORD, "Chord", TranslatableString("engraving", "chord(s)", nullptr, 1),
         TranslatableString("engraving", "Chord(s)", nullptr, 1)},
    Item{ElementType::SLUR, "Slur", TranslatableString("engraving", "slur(s)", nullptr, 1),
         TranslatableString("engraving", "Slur(s)", nullptr, 1)},
    Item{ElementType::HBOX, "HBox", TranslatableString("engraving", "horizontal frame(s)", nullptr, 1),
         TranslatableString("engraving", "Horizontal frame(s)", nullptr, 1)},
    Item{ElementType::VBOX, "VBox", TranslatableString("engraving", "vertical frame(s)", nullptr, 1),
         TranslatableString("engraving", "Vertical frame(s)", nullptr, 1)},
    Item{ElementType::TBOX, "TBox", TranslatableString("engraving", "text frame(s)", nullptr, 1),
         TranslatableString("engraving", "Text frame(s)", nullptr, 1)},
    Item{ElementType::FBOX, "FBox", TranslatableString("engraving", "fretboard diagram frame(s)", nullptr, 1),
         TranslatableString("engraving", "Fretboard diagram frame(s)", nullptr, 1)},
    Item{ElementType::ACTION_ICON, "ActionIcon", TranslatableString::untranslatable("action icon"),
         TranslatableString::untranslatable("Action icon")},
    Item{ElementType::BAGPIPE_EMBELLISHMENT, "BagpipeEmbellishment",
         TranslatableString("engraving", "bagpipe embellishment(s)", nullptr, 1),
         TranslatableString("engraving", "Bagpipe embellishment(s)", nullptr, 1)},
    Item{ElementType::STICKING, "Sticking", TranslatableString("engraving", "sticking(s)", nullptr, 1),
         TranslatableString("engraving", "Sticking(s)", nullptr, 1)},
    Item{ElementType::GRACE_NOTES_GROUP, "GraceNotesGroup", TranslatableString::untranslatable("grace notes group"),
         TranslatableString::untranslatable("Grace notes group")},
    Item{ElementType::GUITAR_BEND, "GuitarBend", TranslatableString("engraving", "guitar bend(s)", nullptr, 1),
         TranslatableString("engraving", "Guitar bend(s)", nullptr, 1)},
    Item{ElementType::GUITAR_BEND_SEGMENT, "GuitarBendSegment", TranslatableString("engraving", "guitar bend segment(s)", nullptr, 1),
         TranslatableString("engraving", "Guitar bend segment(s)", nullptr, 1)},
    Item{ElementType::GUITAR_BEND_HOLD, "GuitarBendHold", TranslatableString("engraving", "guitar bend hold(s)", nullptr, 1),
         TranslatableString("engraving", "Guitar bend hold(s)", nullptr, 1)},
    Item{ElementType::GUITAR_BEND_HOLD_SEGMENT, "GuitarBendHoldSegment",
         TranslatableString("engraving", "guitar bend hold segment(s)", nullptr, 1),
         TranslatableString("engraving", "Guitar bend hold segment(s)", nullptr, 1)},
    Item{ElementType::GUITAR_BEND_TEXT, "GuitarBendText", TranslatableString("engraving", "guitar bend text(s)", nullptr, 1),
         TranslatableString("engraving", "Guitar bend text(s)", nullptr, 1)},
    Item{ElementType::TREMOLO_SINGLECHORD, "TremoloSingleChord", TranslatableString("engraving", "tremolo(s)", nullptr, 1),
         TranslatableString("engraving", "Tremolo(s)", nullptr, 1)},
    Item{ElementType::TREMOLO_TWOCHORD, "TremoloTwoChord", TranslatableString("engraving", "tremolo(s)", nullptr, 1),
         TranslatableString("engraving", "Tremolo(s)", nullptr, 1)},
    Item{ElementType::TIME_TICK_ANCHOR, "TimeTickAnchor", TranslatableString("engraving", "time tick anchor(s)", nullptr, 1),
         TranslatableString("engraving", "Time tick anchor(s)", nullptr, 1)},
    Item{ElementType::HAMMER_ON_PULL_OFF, "HammerOnPullOff", TranslatableString("engraving", "hammer-on / pull-off(s)", nullptr, 1),
         TranslatableString("engraving", "Hammer-on / pull-off(s)", nullptr, 1)},
    Item{ElementType::HAMMER_ON_PULL_OFF_SEGMENT, "HammerOnPullOffSegment",
         TranslatableString("engraving", "hammer-on / pull-off segment(s)", nullptr, 1),
         TranslatableString("engraving", "Hammer-on / pull-off segment(s)", nullptr, 1)},
    Item{ElementType::HAMMER_ON_PULL_OFF_TEXT, "HammerOnPullOffText",
         TranslatableString("engraving", "hammer-on / pull-off text(s)", nullptr, 1),
         TranslatableString("engraving", "Hammer-on / pull-off text(s)", nullptr, 1)},
    Item{ElementType::TAPPING, "Tapping", TranslatableString("engraving", "tapping", nullptr, 1),
         TranslatableString("engraving", "Tapping", nullptr, 1)},
    Item{ElementType::TAPPING_HALF_SLUR, "TappingHalfSlur", TranslatableString("engraving", "tapping half slur(s)", nullptr, 1),
         TranslatableString("engraving", "Tapping half slur(s)", nullptr, 1)},
    Item{ElementType::TAPPING_HALF_SLUR_SEGMENT, "TappingHalfSlurSegment",
         TranslatableString("engraving", "tapping half slur segment(s)", nullptr, 1),
         TranslatableString("engraving", "Tapping half slur segment(s)", nullptr, 1)},
    Item{ElementType::TAPPING_TEXT, "TappingText", TranslatableString("engraving", "tapping text(s)", nullptr, 1),
         TranslatableString("engraving", "Tapping text(s)", nullptr, 1)},
    Item{ElementType::ROOT_ITEM, "RootItem", TranslatableString::untranslatable("root item"),
         TranslatableString::untranslatable("Root item")},
    Item{ElementType::DUMMY, "Dummy", TranslatableString::untranslatable("dummy"), TranslatableString::untranslatable("Dummy")},
};

static_assert(::element_types.size() == ::mu::engraving::tot_element_types, "please update ELEMENT_TYPES");

const TranslatableString &TConv::user_name(ElementType v) { return find_user_name_by_type(::element_types, v); }

const TranslatableString &TConv::capitalized_user_name(ElementType v) { return find_capitalized_user_name_by_type(::element_types, v); }

AsciiStringView TConv::to_xml(ElementType v) { return find_xml_tag_by_type<ElementType>(::element_types, v); }

ElementType TConv::from_xml(const AsciiStringView &tag, ElementType def, bool silent) {
    return find_type_by_xml_tag<ElementType>(::element_types, tag, def, silent);
}

static const std::vector<Item<AlignH>> align_h = {
    {AlignH::LEFT, "left"},
    {AlignH::RIGHT, "right"},
    {AlignH::HCENTER, "center"},
    {AlignH::JUSTIFY, "justify"},
};

static const std::vector<Item<AlignV>> align_v = {
    {AlignV::TOP, "top"},
    {AlignV::VCENTER, "center"},
    {AlignV::BOTTOM, "bottom"},
    {AlignV::BASELINE, "baseline"},
};

AlignH TConv::from_xml(const AsciiStringView &tag, AlignH def) { return find_type_by_xml_tag<AlignH>(::align_h, tag, def); }

AlignV TConv::from_xml(const AsciiStringView &tag, AlignV def) { return find_type_by_xml_tag<AlignV>(::align_v, tag, def); }

String TConv::to_xml(AlignH v) { return String::from_ascii(find_xml_tag_by_type<AlignH>(::align_h, v).ascii()); }

String TConv::to_xml(AlignV v) { return String::from_ascii(find_xml_tag_by_type<AlignV>(::align_v, v).ascii()); }

String TConv::to_xml(Align v) {
    StringList sl;
    sl << to_xml(v.horizontal);
    sl << to_xml(v.vertical);
    return sl.join(u",");
}

Align TConv::from_xml(const String &str, Align def) {
    StringList sl = str.split(',');
    if (sl.size() != 2) {
        LOGD() << "bad align value: " << str;
        return def;
    }

    Align a;
    a.horizontal = find_type_by_xml_tag<AlignH>(::align_h, sl.at(0), def.horizontal);
    a.vertical = find_type_by_xml_tag<AlignV>(::align_v, sl.at(1), def.vertical);
    return a;
}

static const std::vector<Item<IntervalStep>> interval_step = {{IntervalStep::UNISON, "unison"},   {IntervalStep::SECOND, "second"},
                                                              {IntervalStep::THIRD, "third"},     {IntervalStep::FOURTH, "fourth"},
                                                              {IntervalStep::FIFTH, "fifth"},     {IntervalStep::SIXTH, "sixth"},
                                                              {IntervalStep::SEVENTH, "seventh"}, {IntervalStep::OCTAVE, "octave"}};

static const std::vector<Item<IntervalType>> interval_type = {
    {IntervalType::AUTO, "auto"},       {IntervalType::MINOR, "minor"},           {IntervalType::MAJOR, "major"},
    {IntervalType::PERFECT, "perfect"}, {IntervalType::DIMINISHED, "diminished"}, {IntervalType::AUGMENTED, "augmented"},
};

String TConv::to_xml(OrnamentInterval interval) {
    StringList sl;
    sl << String::from_ascii(find_xml_tag_by_type<IntervalStep>(::interval_step, interval.step).ascii());
    sl << String::from_ascii(find_xml_tag_by_type<IntervalType>(::interval_type, interval.type).ascii());
    return sl.join(u",");
}

OrnamentInterval TConv::from_xml(const String &str, OrnamentInterval def) {
    StringList sl = str.split(',');
    if (sl.size() != 2) {
        LOGD() << "bad ornament interval value: " << str;
        return def;
    }

    OrnamentInterval interval;
    interval.step = find_type_by_xml_tag<IntervalStep>(::interval_step, sl.at(0), def.step);
    interval.type = find_type_by_xml_tag<IntervalType>(::interval_type, sl.at(1), def.type);
    return interval;
}

IntervalStep TConv::from_xml(const AsciiStringView &tag, IntervalStep def) {
    return find_type_by_xml_tag<IntervalStep>(::interval_step, tag, def);
}

IntervalType TConv::from_xml(const AsciiStringView &tag, IntervalType def) {
    return find_type_by_xml_tag<IntervalType>(::interval_type, tag, def);
}

static const std::vector<Item<TiePlacement>> tie_placement_constant = {
    {TiePlacement::AUTO, "auto"},
    {TiePlacement::INSIDE, "inside"},
    {TiePlacement::OUTSIDE, "outside"},
};

AsciiStringView TConv::to_xml(TiePlacement input_tie_placement) {
    return find_xml_tag_by_type<TiePlacement>(::tie_placement_constant, input_tie_placement);
}

TiePlacement TConv::from_xml(const AsciiStringView &str, TiePlacement def) {
    return find_type_by_xml_tag<TiePlacement>(::tie_placement_constant, str, def);
}

static const std::vector<Item<TieDotsPlacement>> tie_dots_placement = {
    {TieDotsPlacement::AUTO, "auto"},
    {TieDotsPlacement::BEFORE_DOTS, "before"},
    {TieDotsPlacement::AFTER_DOTS, "after"},
};

AsciiStringView TConv::to_xml(TieDotsPlacement placement) {
    return find_xml_tag_by_type<TieDotsPlacement>(::tie_dots_placement, placement);
}

TieDotsPlacement TConv::from_xml(const AsciiStringView &str, TieDotsPlacement def) {
    return find_type_by_xml_tag<TieDotsPlacement>(::tie_dots_placement, str, def);
}

static const std::vector<Item<TimeSigPlacement>> timesig_placement = {{TimeSigPlacement::NORMAL, "normal"},
                                                                      {TimeSigPlacement::ABOVE_STAVES, "aboveStaves"},
                                                                      {TimeSigPlacement::ACROSS_STAVES, "acrossStaves"}};
static const std::vector<Item<TimeSigStyle>> timesig_style = {
    {TimeSigStyle::NORMAL, "normal"}, {TimeSigStyle::NARROW, "narrow"}, {TimeSigStyle::LARGE, "large"}};
static const std::vector<Item<TimeSigVSMargin>> timesig_margin = {
    {TimeSigVSMargin::HANG_INTO_MARGIN, "hangIntoMargin"},
    {TimeSigVSMargin::RIGHT_ALIGN_TO_BARLINE, "rightAlignToBarline"},
    {TimeSigVSMargin::CREATE_SPACE, "createSpace"},
};

AsciiStringView TConv::to_xml(TimeSigPlacement time_sig_pos) {
    return find_xml_tag_by_type<TimeSigPlacement>(::timesig_placement, time_sig_pos);
}

TimeSigPlacement TConv::from_xml(const AsciiStringView &str, TimeSigPlacement def) {
    return find_type_by_xml_tag<TimeSigPlacement>(::timesig_placement, str, def);
}

AsciiStringView TConv::to_xml(TimeSigStyle time_sig_style) { return find_xml_tag_by_type<TimeSigStyle>(::timesig_style, time_sig_style); }

TimeSigStyle TConv::from_xml(const AsciiStringView &str, TimeSigStyle def) {
    return find_type_by_xml_tag<TimeSigStyle>(::timesig_style, str, def);
}

AsciiStringView TConv::to_xml(TimeSigVSMargin time_sig_vs_margin) {
    return find_xml_tag_by_type<TimeSigVSMargin>(::timesig_margin, time_sig_vs_margin);
}

TimeSigVSMargin TConv::from_xml(const AsciiStringView &str, TimeSigVSMargin def) {
    return find_type_by_xml_tag<TimeSigVSMargin>(::timesig_margin, str, def);
}

static const std::vector<Item<NoteSpellingType>> note_spelling_type_constant = {
    {NoteSpellingType::STANDARD, "standard"},   {NoteSpellingType::GERMAN, "german"}, {NoteSpellingType::GERMAN_PURE, "germanPure"},
    {NoteSpellingType::SOLFEGGIO, "solfeggio"}, {NoteSpellingType::FRENCH, "french"},
};

AsciiStringView TConv::to_xml(NoteSpellingType input_note_spelling_type) {
    return find_xml_tag_by_type<NoteSpellingType>(::note_spelling_type_constant, input_note_spelling_type);
}

NoteSpellingType TConv::from_xml(const AsciiStringView &str, NoteSpellingType def) {
    return find_type_by_xml_tag<NoteSpellingType>(::note_spelling_type_constant, str, def);
}

static const std::vector<Item<ChordStylePreset>> chord_style_preset_constant = {
    {ChordStylePreset::STANDARD, "std"},
    {ChordStylePreset::JAZZ, "jazz"},
    {ChordStylePreset::LEGACY, "legacy"},
    {ChordStylePreset::CUSTOM, "custom"},
};

AsciiStringView TConv::to_xml(ChordStylePreset input_chord_style_preset) {
    return find_xml_tag_by_type<ChordStylePreset>(::chord_style_preset_constant, input_chord_style_preset);
}

ChordStylePreset TConv::from_xml(const AsciiStringView &str, ChordStylePreset def) {
    return find_type_by_xml_tag<ChordStylePreset>(::chord_style_preset_constant, str, def);
}

std::vector<Item<TappingHand>> tapping_hand_value{
    {TappingHand::INVALID, "invalid"},
    {TappingHand::LEFT, "left"},
    {TappingHand::RIGHT, "right"},
};

AsciiStringView TConv::to_xml(TappingHand input_tapping_hand) {
    return find_xml_tag_by_type<TappingHand>(tapping_hand_value, input_tapping_hand);
}

TappingHand TConv::from_xml(const AsciiStringView &str, TappingHand def) {
    return find_type_by_xml_tag<TappingHand>(tapping_hand_value, str, def);
}

std::vector<Item<LHTappingSymbol>> lh_tapping_symbol{
    {LHTappingSymbol::DOT, "dot"},
    {LHTappingSymbol::CIRCLED_T, "circledT"},
};

AsciiStringView TConv::to_xml(LHTappingSymbol lh) { return find_xml_tag_by_type<LHTappingSymbol>(lh_tapping_symbol, lh); }

LHTappingSymbol TConv::from_xml(const AsciiStringView &str, LHTappingSymbol def) {
    return find_type_by_xml_tag<LHTappingSymbol>(lh_tapping_symbol, str, def);
}

std::vector<Item<RHTappingSymbol>> rh_tapping_symbol{
    {RHTappingSymbol::T, "T"},
    {RHTappingSymbol::PLUS, "plus"},
};

AsciiStringView TConv::to_xml(RHTappingSymbol rh) { return find_xml_tag_by_type<RHTappingSymbol>(rh_tapping_symbol, rh); }

RHTappingSymbol TConv::from_xml(const AsciiStringView &str, RHTappingSymbol def) {
    return find_type_by_xml_tag<RHTappingSymbol>(rh_tapping_symbol, str, def);
}

std::vector<Item<ParenthesesMode>> parentheses_mode{
    {ParenthesesMode::NONE, "none"}, {ParenthesesMode::BOTH, "both"}, {ParenthesesMode::LEFT, "left"}, {ParenthesesMode::RIGHT, "right"}};

AsciiStringView TConv::to_xml(ParenthesesMode pm) { return find_xml_tag_by_type<ParenthesesMode>(parentheses_mode, pm); }

ParenthesesMode TConv::from_xml(const AsciiStringView &str, ParenthesesMode def) {
    return find_type_by_xml_tag<ParenthesesMode>(parentheses_mode, str, def);
}

static const std::vector<Item<RepeatPlayCountPreset>> repeat_count_preset = {
    {RepeatPlayCountPreset::X_N, "xn", muse::TranslatableString("engraving", "x%1")},
    {RepeatPlayCountPreset::N_X, "nx", muse::TranslatableString("engraving", "%1x")},
    {RepeatPlayCountPreset::PLAY_N_TIMES, "playntimes", muse::TranslatableString("engraving", "Play %1 times")},
    {RepeatPlayCountPreset::N_REPEATS, "nrepeats", muse::TranslatableString("engraving", "%1 repeats")},
};

const muse::TranslatableString &TConv::user_name(RepeatPlayCountPreset v) {
    return find_capitalized_user_name_by_type(::repeat_count_preset, v);
}

String TConv::translated_user_name(RepeatPlayCountPreset v) {
    return find_capitalized_user_name_by_type(::repeat_count_preset, v).translated();
}

AsciiStringView TConv::to_xml(RepeatPlayCountPreset repeat_preset) {
    return find_xml_tag_by_type<RepeatPlayCountPreset>(::repeat_count_preset, repeat_preset);
}

RepeatPlayCountPreset TConv::from_xml(const AsciiStringView &str, RepeatPlayCountPreset def) {
    return find_type_by_xml_tag<RepeatPlayCountPreset>(::repeat_count_preset, str, def);
}

static const std::vector<Item<AutoCustomHide>> auto_custom_hide_constant = {
    {AutoCustomHide::AUTO, "auto"},
    {AutoCustomHide::CUSTOM, "custom"},
    {AutoCustomHide::HIDE, "hide"},
};

AsciiStringView TConv::to_xml(AutoCustomHide input_auto_custom_hide) {
    return find_xml_tag_by_type<AutoCustomHide>(::auto_custom_hide_constant, input_auto_custom_hide);
}

AutoCustomHide TConv::from_xml(const AsciiStringView &str, AutoCustomHide def) {
    return find_type_by_xml_tag<AutoCustomHide>(::auto_custom_hide_constant, str, def);
}

static const std::vector<Item<VoiceAssignment>> voice_assignment = {{VoiceAssignment::ALL_VOICE_IN_INSTRUMENT, "allInInstrument"},
                                                                    {VoiceAssignment::ALL_VOICE_IN_STAFF, "allInStaff"},
                                                                    {VoiceAssignment::CURRENT_VOICE_ONLY, "currentVoiceOnly"}};

AsciiStringView TConv::to_xml(VoiceAssignment voice_appl) { return find_xml_tag_by_type<VoiceAssignment>(::voice_assignment, voice_appl); }

VoiceAssignment TConv::from_xml(const AsciiStringView &str, VoiceAssignment def) {
    return find_type_by_xml_tag<VoiceAssignment>(::voice_assignment, str, def);
}

static const std::vector<Item<AutoOnOff>> auto_on_off_constant = {
    {AutoOnOff::AUTO, "auto"},
    {AutoOnOff::ON, "on"},
    {AutoOnOff::OFF, "off"},
};

AsciiStringView TConv::to_xml(AutoOnOff input_auto_on_off) {
    return find_xml_tag_by_type<AutoOnOff>(::auto_on_off_constant, input_auto_on_off);
}

AutoOnOff TConv::from_xml(const AsciiStringView &str, AutoOnOff def) {
    return find_type_by_xml_tag<AutoOnOff>(::auto_on_off_constant, str, def);
}

static const std::vector<Item<CapoParams::TransposeMode>> capo_transpose_mode = {
    {CapoParams::TransposeMode::PLAYBACK_ONLY, "playback"},
    {CapoParams::TransposeMode::STANDARD_ONLY, "standard"},
    {CapoParams::TransposeMode::TAB_ONLY, "tab"},
};

AsciiStringView TConv::to_xml(CapoParams::TransposeMode mode) {
    return find_xml_tag_by_type<CapoParams::TransposeMode>(::capo_transpose_mode, mode);
}

CapoParams::TransposeMode TConv::from_xml(const AsciiStringView &str, CapoParams::TransposeMode def) {
    return find_type_by_xml_tag<CapoParams::TransposeMode>(::capo_transpose_mode, str, def);
}

static const std::vector<Item<PartialSpannerDirection>> partial_spanner_direction = {{PartialSpannerDirection::NONE, "none"},
                                                                                     {PartialSpannerDirection::OUTGOING, "outgoing"},
                                                                                     {PartialSpannerDirection::INCOMING, "incoming"},
                                                                                     {PartialSpannerDirection::BOTH, "both"}};

AsciiStringView TConv::to_xml(PartialSpannerDirection v) {
    return find_xml_tag_by_type<PartialSpannerDirection>(::partial_spanner_direction, v);
}

PartialSpannerDirection TConv::from_xml(const AsciiStringView &str, PartialSpannerDirection def) {
    return find_type_by_xml_tag<PartialSpannerDirection>(::partial_spanner_direction, str, def);
}

String TConv::translated_user_name(SymId v) { return SymNames::translated_user_name_for_sym_id(v); }

AsciiStringView TConv::to_xml(SymId v) { return SymNames::name_for_sym_id(v); }

SymId TConv::from_xml(const AsciiStringView &tag, SymId def) { return SymNames::sym_id_by_name(tag, def); }

static const std::array<Item<Orientation>, 2> orientation = {{
    {Orientation::VERTICAL, "vertical", muse::TranslatableString("engraving", "Vertical")},
    {Orientation::HORIZONTAL, "horizontal", muse::TranslatableString("engraving", "Horizontal")},
}};

String TConv::translated_user_name(Orientation v) { return find_capitalized_user_name_by_type(::orientation, v).translated(); }

AsciiStringView TConv::to_xml(Orientation v) { return find_xml_tag_by_type<Orientation>(::orientation, v); }

Orientation TConv::from_xml(const AsciiStringView &tag, Orientation def) {
    return find_type_by_xml_tag<Orientation>(::orientation, tag, def);
}

static const std::array<Item<SharedLabelOrientation>, 3> shared_label_orientation = {{
    {SharedLabelOrientation::VERTICAL, "vertical", muse::TranslatableString("engraving", "Vertical")},
    {SharedLabelOrientation::HORIZONTAL, "horizontal", muse::TranslatableString("engraving", "Horizontal")},
    {SharedLabelOrientation::VOICE, "voice", muse::TranslatableString("engraving", "Voice")},
}};

String TConv::translated_user_name(SharedLabelOrientation v) {
    return find_capitalized_user_name_by_type(::shared_label_orientation, v).translated();
}

AsciiStringView TConv::to_xml(SharedLabelOrientation v) {
    return find_xml_tag_by_type<SharedLabelOrientation>(::shared_label_orientation, v);
}

SharedLabelOrientation TConv::from_xml(const AsciiStringView &tag, SharedLabelOrientation def) {
    return find_type_by_xml_tag<SharedLabelOrientation>(::shared_label_orientation, tag, def);
}

static const std::array<Item<NoteHeadType>, 5> notehead_types = {{
    {NoteHeadType::HEAD_AUTO, "auto", muse::TranslatableString("engraving", "Auto")},
    {NoteHeadType::HEAD_WHOLE, "whole", muse::TranslatableString("engraving/noteheadtype", "Whole")},
    {NoteHeadType::HEAD_HALF, "half", muse::TranslatableString("engraving/noteheadtype", "Half")},
    {NoteHeadType::HEAD_QUARTER, "quarter", muse::TranslatableString("engraving/noteheadtype", "Quarter")},
    {NoteHeadType::HEAD_BREVIS, "breve", muse::TranslatableString("engraving/noteheadtype", "Breve")},
}};

String TConv::translated_user_name(NoteHeadType v) { return find_capitalized_user_name_by_type(::notehead_types, v).translated(); }

AsciiStringView TConv::to_xml(NoteHeadType v) { return find_xml_tag_by_type<NoteHeadType>(::notehead_types, v); }

NoteHeadType TConv::from_xml(const AsciiStringView &tag, NoteHeadType def) {
    return find_type_by_xml_tag<NoteHeadType>(::notehead_types, tag, def);
}

/* *INDENT-OFF* */
static const std::vector<Item<NoteHeadScheme>> notehead_schemes = {
    {NoteHeadScheme::HEAD_AUTO, "auto", muse::TranslatableString("engraving", "Auto")},
    {NoteHeadScheme::HEAD_NORMAL, "normal", muse::TranslatableString("engraving/noteheadscheme", "Normal")},
    {NoteHeadScheme::HEAD_PITCHNAME, "name-pitch", muse::TranslatableString("engraving/noteheadscheme", "Pitch names")},
    {NoteHeadScheme::HEAD_PITCHNAME_NO_ACCIDENTALS, "name-pitch-no-acc",
     muse::TranslatableString("engraving/noteheadscheme", "Pitch names, no accidentals")},
    {NoteHeadScheme::HEAD_PITCHNAME_GERMAN, "name-pitch-german",
     muse::TranslatableString("engraving/noteheadscheme", "German pitch names")},
    {NoteHeadScheme::HEAD_PITCHNAME_GERMAN_NO_ACCIDENTALS, "name-pitch-german-no-acc",
     muse::TranslatableString("engraving/noteheadscheme", "German pitch names, no accidentals")},
    {NoteHeadScheme::HEAD_SOLFEGE, "solfege-movable",
     muse::TranslatableString("engraving/noteheadscheme", "Solf\u00e8ge movable Do")}, // &egrave;
    {NoteHeadScheme::HEAD_SOLFEGE_FIXED, "solfege-fixed",
     muse::TranslatableString("engraving/noteheadscheme", "Solf\u00e8ge fixed Do")}, // &egrave;
    {NoteHeadScheme::HEAD_SHAPE_NOTE_4, "shape-4", muse::TranslatableString("engraving/noteheadscheme", "4-shape (Walker)")},
    {NoteHeadScheme::HEAD_SHAPE_NOTE_7_AIKIN, "shape-7-aikin", muse::TranslatableString("engraving/noteheadscheme", "7-shape (Aikin)")},
    {NoteHeadScheme::HEAD_SHAPE_NOTE_7_FUNK, "shape-7-funk", muse::TranslatableString("engraving/noteheadscheme", "7-shape (Funk)")},
    {NoteHeadScheme::HEAD_SHAPE_NOTE_7_WALKER, "shape-7-walker", muse::TranslatableString("engraving/noteheadscheme", "7-shape (Walker)")}};
/* *INDENT-ON* */

String TConv::translated_user_name(NoteHeadScheme v) { return find_capitalized_user_name_by_type(::notehead_schemes, v).translated(); }

AsciiStringView TConv::to_xml(NoteHeadScheme v) { return find_xml_tag_by_type<NoteHeadScheme>(::notehead_schemes, v); }

NoteHeadScheme TConv::from_xml(const AsciiStringView &tag, NoteHeadScheme def) {
    return find_type_by_xml_tag<NoteHeadScheme>(::notehead_schemes, tag, def);
}

static const std::vector<Item<InstrumentLabelVisibility>> instrument_label_visibilities = {
    {InstrumentLabelVisibility::AUTO, "auto", muse::TranslatableString("engraving", "Auto")},
    {InstrumentLabelVisibility::LONG, "long", muse::TranslatableString("engraving", "Long")},
    {InstrumentLabelVisibility::SHORT, "short", muse::TranslatableString("engraving", "Short")},
    {InstrumentLabelVisibility::HIDE, "hide", muse::TranslatableString("engraving", "Hide")}};

String TConv::translated_user_name(InstrumentLabelVisibility v) {
    return find_capitalized_user_name_by_type(::instrument_label_visibilities, v).translated();
}

AsciiStringView TConv::to_xml(InstrumentLabelVisibility v) {
    return find_xml_tag_by_type<InstrumentLabelVisibility>(::instrument_label_visibilities, v);
}

InstrumentLabelVisibility TConv::from_xml(const AsciiStringView &tag, InstrumentLabelVisibility def) {
    return find_type_by_xml_tag<InstrumentLabelVisibility>(::instrument_label_visibilities, tag, def);
}

static const std::vector<Item<NoteHeadGroup>> notehead_groups = {
    {NoteHeadGroup::HEAD_NORMAL, "normal", muse::TranslatableString("engraving/noteheadgroup", "Normal")},
    {NoteHeadGroup::HEAD_CROSS, "cross", muse::TranslatableString("engraving/noteheadgroup", "Cross")},
    {NoteHeadGroup::HEAD_PLUS, "plus", muse::TranslatableString("engraving/noteheadgroup", "Plus")},
    {NoteHeadGroup::HEAD_XCIRCLE, "xcircle", muse::TranslatableString("engraving/noteheadgroup", "XCircle")},
    {NoteHeadGroup::HEAD_WITHX, "withx", muse::TranslatableString("engraving/noteheadgroup", "With X")},
    {NoteHeadGroup::HEAD_TRIANGLE_UP, "triangle-up", muse::TranslatableString("engraving/noteheadgroup", "Triangle up")},
    {NoteHeadGroup::HEAD_TRIANGLE_DOWN, "triangle-down", muse::TranslatableString("engraving/noteheadgroup", "Triangle down")},
    {NoteHeadGroup::HEAD_SLASHED1, "slashed1", muse::TranslatableString("engraving/noteheadgroup", "Slashed (forwards)")},
    {NoteHeadGroup::HEAD_SLASHED2, "slashed2", muse::TranslatableString("engraving/noteheadgroup", "Slashed (backwards)")},
    {NoteHeadGroup::HEAD_DIAMOND, "diamond", muse::TranslatableString("engraving/noteheadgroup", "Diamond")},
    {NoteHeadGroup::HEAD_DIAMOND_OLD, "diamond-old", muse::TranslatableString("engraving/noteheadgroup", "Diamond (old)")},
    {NoteHeadGroup::HEAD_CIRCLED, "circled", muse::TranslatableString("engraving/noteheadgroup", "Circled")},
    {NoteHeadGroup::HEAD_CIRCLED_LARGE, "circled-large", muse::TranslatableString("engraving/noteheadgroup", "Circled large")},
    {NoteHeadGroup::HEAD_LARGE_ARROW, "large-arrow", muse::TranslatableString("engraving/noteheadgroup", "Large arrow")},
    {NoteHeadGroup::HEAD_BREVIS_ALT, "altbrevis", muse::TranslatableString("engraving/noteheadgroup", "Alt. brevis")},

    {NoteHeadGroup::HEAD_SLASH, "slash", muse::TranslatableString("engraving/noteheadgroup", "Slash")},
    {NoteHeadGroup::HEAD_LARGE_DIAMOND, "large-diamond", muse::TranslatableString("engraving/noteheadgroup", "Large diamond")},

    {NoteHeadGroup::HEAD_HEAVY_CROSS, "heavy-cross", muse::TranslatableString("engraving/noteheadgroup", "Heavy cross")},
    {NoteHeadGroup::HEAD_HEAVY_CROSS_HAT, "heavy-cross-hat", muse::TranslatableString("engraving/noteheadgroup", "Heavy cross hat")},

    // shape notes
    {NoteHeadGroup::HEAD_SOL, "sol", muse::TranslatableString("engraving/noteheadgroup", "Sol")},
    {NoteHeadGroup::HEAD_LA, "la", muse::TranslatableString("engraving/noteheadgroup", "La")},
    {NoteHeadGroup::HEAD_FA, "fa", muse::TranslatableString("engraving/noteheadgroup", "Fa")},
    {NoteHeadGroup::HEAD_MI, "mi", muse::TranslatableString("engraving/noteheadgroup", "Mi")},
    {NoteHeadGroup::HEAD_DO, "do", muse::TranslatableString("engraving/noteheadgroup", "Do")},
    {NoteHeadGroup::HEAD_RE, "re", muse::TranslatableString("engraving/noteheadgroup", "Re")},
    {NoteHeadGroup::HEAD_TI, "ti", muse::TranslatableString("engraving/noteheadgroup", "Ti")},

    // not exposed
    {NoteHeadGroup::HEAD_DO_WALKER, "do-walker", muse::TranslatableString("engraving/noteheadgroup", "Do (Walker)")},
    {NoteHeadGroup::HEAD_RE_WALKER, "re-walker", muse::TranslatableString("engraving/noteheadgroup", "Re (Walker)")},
    {NoteHeadGroup::HEAD_TI_WALKER, "ti-walker", muse::TranslatableString("engraving/noteheadgroup", "Ti (Walker)")},
    {NoteHeadGroup::HEAD_DO_FUNK, "do-funk", muse::TranslatableString("engraving/noteheadgroup", "Do (Funk)")},
    {NoteHeadGroup::HEAD_RE_FUNK, "re-funk", muse::TranslatableString("engraving/noteheadgroup", "Re (Funk)")},
    {NoteHeadGroup::HEAD_TI_FUNK, "ti-funk", muse::TranslatableString("engraving/noteheadgroup", "Ti (Funk)")},

    // note name
    {NoteHeadGroup::HEAD_DO_NAME, "do-name", muse::TranslatableString("engraving/noteheadgroup", "Do (Name)")},
    {NoteHeadGroup::HEAD_DI_NAME, "di-name", muse::TranslatableString("engraving/noteheadgroup", "Di (Name)")},
    {NoteHeadGroup::HEAD_RA_NAME, "ra-name", muse::TranslatableString("engraving/noteheadgroup", "Ra (Name)")},
    {NoteHeadGroup::HEAD_RE_NAME, "re-name", muse::TranslatableString("engraving/noteheadgroup", "Re (Name)")},
    {NoteHeadGroup::HEAD_RI_NAME, "ri-name", muse::TranslatableString("engraving/noteheadgroup", "Ri (Name)")},
    {NoteHeadGroup::HEAD_ME_NAME, "me-name", muse::TranslatableString("engraving/noteheadgroup", "Me (Name)")},
    {NoteHeadGroup::HEAD_MI_NAME, "mi-name", muse::TranslatableString("engraving/noteheadgroup", "Mi (Name)")},
    {NoteHeadGroup::HEAD_FA_NAME, "fa-name", muse::TranslatableString("engraving/noteheadgroup", "Fa (Name)")},
    {NoteHeadGroup::HEAD_FI_NAME, "fi-name", muse::TranslatableString("engraving/noteheadgroup", "Fi (Name)")},
    {NoteHeadGroup::HEAD_SE_NAME, "se-name", muse::TranslatableString("engraving/noteheadgroup", "Se (Name)")},
    {NoteHeadGroup::HEAD_SOL_NAME, "sol-name", muse::TranslatableString("engraving/noteheadgroup", "Sol (Name)")},
    {NoteHeadGroup::HEAD_LE_NAME, "le-name", muse::TranslatableString("engraving/noteheadgroup", "Le (Name)")},
    {NoteHeadGroup::HEAD_LA_NAME, "la-name", muse::TranslatableString("engraving/noteheadgroup", "La (Name)")},
    {NoteHeadGroup::HEAD_LI_NAME, "li-name", muse::TranslatableString("engraving/noteheadgroup", "Li (Name)")},
    {NoteHeadGroup::HEAD_TE_NAME, "te-name", muse::TranslatableString("engraving/noteheadgroup", "Te (Name)")},
    {NoteHeadGroup::HEAD_TI_NAME, "ti-name", muse::TranslatableString("engraving/noteheadgroup", "Ti (Name)")},
    {NoteHeadGroup::HEAD_SI_NAME, "si-name", muse::TranslatableString("engraving/noteheadgroup", "Si (Name)")},

    {NoteHeadGroup::HEAD_A_SHARP, "a-sharp-name", muse::TranslatableString("engraving/noteheadgroup", "A♯ (Name)")},
    {NoteHeadGroup::HEAD_A, "a-name", muse::TranslatableString("engraving/noteheadgroup", "A (Name)")},
    {NoteHeadGroup::HEAD_A_FLAT, "a-flat-name", muse::TranslatableString("engraving/noteheadgroup", "A♭ (Name)")},
    {NoteHeadGroup::HEAD_B_SHARP, "b-sharp-name", muse::TranslatableString("engraving/noteheadgroup", "B♯ (Name)")},
    {NoteHeadGroup::HEAD_B, "b-name", muse::TranslatableString("engraving/noteheadgroup", "B (Name)")},
    {NoteHeadGroup::HEAD_B_FLAT, "b-flat-name", muse::TranslatableString("engraving/noteheadgroup", "B♭ (Name)")},
    {NoteHeadGroup::HEAD_C_SHARP, "c-sharp-name", muse::TranslatableString("engraving/noteheadgroup", "C♯ (Name)")},
    {NoteHeadGroup::HEAD_C, "c-name", muse::TranslatableString("engraving/noteheadgroup", "C (Name)")},
    {NoteHeadGroup::HEAD_C_FLAT, "c-flat-name", muse::TranslatableString("engraving/noteheadgroup", "C♭ (Name)")},
    {NoteHeadGroup::HEAD_D_SHARP, "d-sharp-name", muse::TranslatableString("engraving/noteheadgroup", "D♯ (Name)")},
    {NoteHeadGroup::HEAD_D, "d-name", muse::TranslatableString("engraving/noteheadgroup", "D (Name)")},
    {NoteHeadGroup::HEAD_D_FLAT, "d-flat-name", muse::TranslatableString("engraving/noteheadgroup", "D♭ (Name)")},
    {NoteHeadGroup::HEAD_E_SHARP, "e-sharp-name", muse::TranslatableString("engraving/noteheadgroup", "E♯ (Name)")},
    {NoteHeadGroup::HEAD_E, "e-name", muse::TranslatableString("engraving/noteheadgroup", "E (Name)")},
    {NoteHeadGroup::HEAD_E_FLAT, "e-flat-name", muse::TranslatableString("engraving/noteheadgroup", "E♭ (Name)")},
    {NoteHeadGroup::HEAD_F_SHARP, "f-sharp-name", muse::TranslatableString("engraving/noteheadgroup", "F♯ (Name)")},
    {NoteHeadGroup::HEAD_F, "f-name", muse::TranslatableString("engraving/noteheadgroup", "F (Name)")},
    {NoteHeadGroup::HEAD_F_FLAT, "f-flat-name", muse::TranslatableString("engraving/noteheadgroup", "F♭ (Name)")},
    {NoteHeadGroup::HEAD_G_SHARP, "g-sharp-name", muse::TranslatableString("engraving/noteheadgroup", "G♯ (Name)")},
    {NoteHeadGroup::HEAD_G, "g-name", muse::TranslatableString("engraving/noteheadgroup", "G (Name)")},
    {NoteHeadGroup::HEAD_G_FLAT, "g-flat-name", muse::TranslatableString("engraving/noteheadgroup", "G♭ (Name)")},
    {NoteHeadGroup::HEAD_H, "h-name", muse::TranslatableString("engraving/noteheadgroup", "H (Name)")},
    {NoteHeadGroup::HEAD_H_SHARP, "h-sharp-name", muse::TranslatableString("engraving/noteheadgroup", "H♯ (Name)")},

    // Swiss rudiments
    {NoteHeadGroup::HEAD_SWISS_RUDIMENTS_FLAM, "swiss-rudiments-flam",
     muse::TranslatableString("engraving/noteheadgroup", "Swiss Rudiments Flam")},
    {NoteHeadGroup::HEAD_SWISS_RUDIMENTS_DOUBLE, "swiss-rudiments-double",
     muse::TranslatableString("engraving/noteheadgroup", "Swiss Rudiments Doublé")},

    {NoteHeadGroup::HEAD_CUSTOM, "custom", muse::TranslatableString("engraving", "Custom")}};

const muse::TranslatableString &TConv::user_name(NoteHeadGroup v) { return find_capitalized_user_name_by_type(::notehead_groups, v); }

String TConv::translated_user_name(NoteHeadGroup v) { return find_capitalized_user_name_by_type(::notehead_groups, v).translated(); }

AsciiStringView TConv::to_xml(NoteHeadGroup v) { return find_xml_tag_by_type<NoteHeadGroup>(::notehead_groups, v); }

NoteHeadGroup TConv::from_xml(const AsciiStringView &tag, NoteHeadGroup def) {
    auto it =
        std::find_if(::notehead_groups.cbegin(), ::notehead_groups.cend(), [tag](const Item<NoteHeadGroup> &i) { return i.xml == tag; });

    if (it != ::notehead_groups.cend()) {
        return it->type;
    }

    // compatibility
    bool ok = false;
    int v = tag.to_int(&ok);
    if (ok) {
        return static_cast<NoteHeadGroup>(v);
    }

    return def;
}

// table must be in sync with enum ClefType in types.h
static const std::vector<Item<ClefType>> clef_types = {
    {ClefType::G, "G", muse::TranslatableString("engraving/cleftype", "Treble clef")},
    {ClefType::G15_MB, "G15mb", muse::TranslatableString("engraving/cleftype", "Treble clef 15ma bassa")},
    {ClefType::G8_VB, "G8vb", muse::TranslatableString("engraving/cleftype", "Treble clef 8va bassa")},
    {ClefType::G8_VA, "G8va", muse::TranslatableString("engraving/cleftype", "Treble clef 8va alta")},
    {ClefType::G15_MA, "G15ma", muse::TranslatableString("engraving/cleftype", "Treble clef 15ma alta")},
    {ClefType::G8_VB_O, "G8vbo", muse::TranslatableString("engraving/cleftype", "Double treble clef 8va bassa on 2nd line")},
    {ClefType::G8_VB_P, "G8vbp", muse::TranslatableString("engraving/cleftype", "Treble clef optional 8va bassa")},
    {ClefType::G_1, "G1", muse::TranslatableString("engraving/cleftype", "French violin clef")},
    {ClefType::C1, "C1", muse::TranslatableString("engraving/cleftype", "Soprano clef")},
    {ClefType::C2, "C2", muse::TranslatableString("engraving/cleftype", "Mezzo-soprano clef")},
    {ClefType::C3, "C3", muse::TranslatableString("engraving/cleftype", "Alto clef")},
    {ClefType::C4, "C4", muse::TranslatableString("engraving/cleftype", "Tenor clef")},
    {ClefType::C5, "C5", muse::TranslatableString("engraving/cleftype", "Baritone clef (C clef)")},
    {ClefType::C_19C, "C_19C", muse::TranslatableString("engraving/cleftype", "C clef, H shape (19th century)")},
    {ClefType::C1_F18C, "C1_F18C", muse::TranslatableString("engraving/cleftype", "Soprano clef (French, 18th century)")},
    {ClefType::C3_F18C, "C3_F18C", muse::TranslatableString("engraving/cleftype", "Alto clef (French, 18th century)")},
    {ClefType::C4_F18C, "C4_F18C", muse::TranslatableString("engraving/cleftype", "Tenor clef (French, 18th century)")},
    {ClefType::C1_F20C, "C1_F20C", muse::TranslatableString("engraving/cleftype", "Soprano clef (French, 20th century)")},
    {ClefType::C3_F20C, "C3_F20C", muse::TranslatableString("engraving/cleftype", "Alto clef (French, 20th century)")},
    {ClefType::C4_F20C, "C4_F20C", muse::TranslatableString("engraving/cleftype", "Tenor clef (French, 20th century)")},
    {ClefType::F, "F", muse::TranslatableString("engraving/cleftype", "Bass clef")},
    {ClefType::F15_MB, "F15mb", muse::TranslatableString("engraving/cleftype", "Bass clef 15ma bassa")},
    {ClefType::F8_VB, "F8vb", muse::TranslatableString("engraving/cleftype", "Bass clef 8va bassa")},
    {ClefType::F_8VA, "F8va", muse::TranslatableString("engraving/cleftype", "Bass clef 8va alta")},
    {ClefType::F_15MA, "F15ma", muse::TranslatableString("engraving/cleftype", "Bass clef 15ma alta")},
    {ClefType::F_B, "F3", muse::TranslatableString("engraving/cleftype", "Baritone clef (F clef)")},
    {ClefType::F_C, "F5", muse::TranslatableString("engraving/cleftype", "Subbass clef")},
    {ClefType::F_F18C, "F_F18C", muse::TranslatableString("engraving/cleftype", "F clef (French, 18th century)")},
    {ClefType::F_19C, "F_19C", muse::TranslatableString("engraving/cleftype", "F clef (19th century)")},

    {ClefType::PERC, "PERC", muse::TranslatableString("engraving/cleftype", "Percussion")},
    {ClefType::PERC2, "PERC2", muse::TranslatableString("engraving/cleftype", "Percussion 2")},

    {ClefType::TAB, "TAB", muse::TranslatableString("engraving/cleftype", "Tablature")},
    {ClefType::TAB4, "TAB4", muse::TranslatableString("engraving/cleftype", "Tablature 4 lines")},
    {ClefType::TAB_SERIF, "TAB2", muse::TranslatableString("engraving/cleftype", "Tablature Serif")},
    {ClefType::TAB4_SERIF, "TAB4_SERIF", muse::TranslatableString("engraving/cleftype", "Tablature Serif 4 lines")},

    {ClefType::C4_8VB, "C4_8VB", muse::TranslatableString("engraving/cleftype", "Tenor clef 8va bassa")},
    {ClefType::G8_VB_C, "G8vbc", muse::TranslatableString("engraving/cleftype", "G clef ottava bassa with C clef")},
};

const muse::TranslatableString &TConv::user_name(ClefType v) { return find_capitalized_user_name_by_type(::clef_types, v); }

String TConv::translated_user_name(ClefType v) { return find_capitalized_user_name_by_type(::clef_types, v).translated(); }

AsciiStringView TConv::to_xml(ClefType v) { return find_xml_tag_by_type<ClefType>(::clef_types, v); }

ClefType TConv::from_xml(const AsciiStringView &tag, ClefType def) {
    auto it = std::find_if(::clef_types.cbegin(), ::clef_types.cend(), [tag](const Item<ClefType> &i) { return i.xml == tag; });

    if (it != ::clef_types.cend()) {
        return it->type;
    }

    // compatibility
    bool ok = false;
    int v = tag.to_int(&ok);
    if (ok) {
        return static_cast<ClefType>(v);
    }

    return def;
}

struct DynamicItem {
    DynamicType type;
    AsciiStringView xml;
    SymId sym_id;
    muse::TranslatableString user_name;
};

static const std::vector<DynamicItem> dynamic_types = {
    {DynamicType::OTHER, "other-dynamics", SymId::NO_SYM, muse::TranslatableString("engraving/dynamictype", "Other dynamic")},
    {DynamicType::PPPPPP, "pppppp", SymId::DYNAMIC_PPPPPP, muse::TranslatableString("engraving/dynamictype", "pppppp")},
    {DynamicType::PPPPP, "ppppp", SymId::DYNAMIC_PPPPP, muse::TranslatableString("engraving/dynamictype", "ppppp")},
    {DynamicType::PPPP, "pppp", SymId::DYNAMIC_PPPP, muse::TranslatableString("engraving/dynamictype", "pppp")},
    {DynamicType::PPP, "ppp", SymId::DYNAMIC_PPP, muse::TranslatableString("engraving/dynamictype", "ppp (piano pianissimo)")},
    {DynamicType::PP, "pp", SymId::DYNAMIC_PP, muse::TranslatableString("engraving/dynamictype", "pp (pianissimo)")},
    {DynamicType::P, "p", SymId::DYNAMIC_PIANO, muse::TranslatableString("engraving/dynamictype", "p (piano)")},

    {DynamicType::MP, "mp", SymId::DYNAMIC_MP, muse::TranslatableString("engraving/dynamictype", "mp (mezzo piano)")},
    {DynamicType::MF, "mf", SymId::DYNAMIC_MF, muse::TranslatableString("engraving/dynamictype", "mf (mezzo forte)")},

    {DynamicType::F, "f", SymId::DYNAMIC_FORTE, muse::TranslatableString("engraving/dynamictype", "f (forte)")},
    {DynamicType::FF, "ff", SymId::DYNAMIC_FF, muse::TranslatableString("engraving/dynamictype", "ff (fortissimo)")},
    {DynamicType::FFF, "fff", SymId::DYNAMIC_FFF, muse::TranslatableString("engraving/dynamictype", "fff (forte fortissimo)")},
    {DynamicType::FFFF, "ffff", SymId::DYNAMIC_FFFF, muse::TranslatableString("engraving/dynamictype", "ffff")},
    {DynamicType::FFFFF, "fffff", SymId::DYNAMIC_FFFFF, muse::TranslatableString("engraving/dynamictype", "fffff")},
    {DynamicType::FFFFFF, "ffffff", SymId::DYNAMIC_FFFFFF, muse::TranslatableString("engraving/dynamictype", "ffffff")},

    {DynamicType::FP, "fp", SymId::DYNAMIC_FORTE_PIANO, muse::TranslatableString("engraving/dynamictype", "fp (forte piano)")},
    {DynamicType::PF, "pf", SymId::NO_SYM, muse::TranslatableString("engraving/dynamictype", "pf (poco forte)")},

    {DynamicType::SF, "sf", SymId::DYNAMIC_SFORZANDO1, muse::TranslatableString("engraving/dynamictype", "sf (sforzando)")},
    {DynamicType::SFZ, "sfz", SymId::DYNAMIC_SFORZATO, muse::TranslatableString("engraving/dynamictype", "sfz (sforzato)")},
    {DynamicType::SFF, "sff", SymId::NO_SYM, muse::TranslatableString("engraving/dynamictype", "sff (sforzando)")},
    {DynamicType::SFFZ, "sffz", SymId::DYNAMIC_SFORZATO_FF, muse::TranslatableString("engraving/dynamictype", "sffz (sforzato)")},
    {DynamicType::SFFF, "sfff", SymId::NO_SYM, muse::TranslatableString("engraving/dynamictype", "sfff (sforzando)")},
    {DynamicType::SFFFZ, "sfffz", SymId::NO_SYM, muse::TranslatableString("engraving/dynamictype", "sfffz (sforzato)")},
    {DynamicType::SFP, "sfp", SymId::DYNAMIC_SFORZANDO_PIANO, muse::TranslatableString("engraving/dynamictype", "sfp (sforzando piano)")},
    {DynamicType::SFPP, "sfpp", SymId::DYNAMIC_SFORZANDO_PIANISSIMO,
     muse::TranslatableString("engraving/dynamictype", "sfpp (sforzando pianissimo)")},

    {DynamicType::RFZ, "rfz", SymId::DYNAMIC_RINFORZANDO2, muse::TranslatableString("engraving/dynamictype", "rfz (rinforzando)")},
    {DynamicType::RF, "rf", SymId::DYNAMIC_RINFORZANDO1, muse::TranslatableString("engraving/dynamictype", "rf (rinforzando)")},
    {DynamicType::FZ, "fz", SymId::DYNAMIC_FORZANDO, muse::TranslatableString("engraving/dynamictype", "fz (forzando)")},
    {DynamicType::M, "m", SymId::DYNAMIC_MEZZO, muse::TranslatableString("engraving/dynamictype", "m (mezzo)")},
    {DynamicType::R, "r", SymId::DYNAMIC_RINFORZANDO, muse::TranslatableString("engraving/dynamictype", "r (rinforzando)")},
    {DynamicType::S, "s", SymId::DYNAMIC_SFORZANDO, muse::TranslatableString("engraving/dynamictype", "s (sforzando)")},
    {DynamicType::Z, "z", SymId::DYNAMIC_Z, muse::TranslatableString("engraving/dynamictype", "z (forzando)")},
    {DynamicType::N, "n", SymId::DYNAMIC_NIENTE, muse::TranslatableString("engraving/dynamictype", "n (niente)")},
};

SymId TConv::sym_id(DynamicType v) {
    auto it = std::find_if(::dynamic_types.cbegin(), ::dynamic_types.cend(), [v](const DynamicItem &i) { return i.type == v; });

    IF_ASSERT_FAILED(it != ::dynamic_types.cend()) { return SymId::NO_SYM; }
    return it->sym_id;
}

DynamicType TConv::dynamic_type(SymId v) {
    auto it = std::find_if(::dynamic_types.cbegin(), ::dynamic_types.cend(), [v](const DynamicItem &i) { return i.sym_id == v; });

    IF_ASSERT_FAILED(it != ::dynamic_types.cend()) { return DynamicType::OTHER; }
    return it->type;
}

DynamicType TConv::dynamic_type(const AsciiStringView &tag) {
    static const std::map<AsciiStringView, DynamicType> dynamic_str_map = {
        {"<sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</"
         "sym><sym>dynamicPiano</sym>",
         DynamicType::PPPPPP},
        {
            "<sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>",
            DynamicType::PPPPP,
        },
        {"<sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>", DynamicType::PPPP},
        {"<sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>", DynamicType::PPP},
        {"<sym>dynamicPiano</sym><sym>dynamicPiano</sym>", DynamicType::PP},
        {"<sym>dynamicPiano</sym>", DynamicType::P},

        {"<sym>dynamicMezzo</sym><sym>dynamicPiano</sym>", DynamicType::MP},
        {"<sym>dynamicMezzo</sym><sym>dynamicForte</sym>", DynamicType::MF},

        {"<sym>dynamicForte</sym>", DynamicType::F},
        {"<sym>dynamicForte</sym><sym>dynamicForte</sym>", DynamicType::FF},
        {"<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>", DynamicType::FFF},
        {"<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>", DynamicType::FFFF},
        {"<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>",
         DynamicType::FFFFF},
        {"<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</"
         "sym><sym>dynamicForte</sym>",
         DynamicType::FFFFFF},

        {"<sym>dynamicForte</sym><sym>dynamicPiano</sym>", DynamicType::FP},
        {"<sym>dynamicPiano</sym><sym>dynamicForte</sym>", DynamicType::PF},

        {"<sym>dynamicSforzando</sym><sym>dynamicForte</sym>", DynamicType::SF},
        {"<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>", DynamicType::SFZ},
        {"<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>", DynamicType::SFF},
        {"<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>", DynamicType::SFFZ},
        {"<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicPiano</sym>", DynamicType::SFP},
        {"<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>", DynamicType::SFPP},

        {"<sym>dynamicRinforzando</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>", DynamicType::RFZ},
        {"<sym>dynamicRinforzando</sym><sym>dynamicForte</sym>", DynamicType::RF},
        {"<sym>dynamicForte</sym><sym>dynamicZ</sym>", DynamicType::FZ},

        {"<sym>dynamicMezzo</sym>", DynamicType::M},
        {"<sym>dynamicRinforzando</sym>", DynamicType::R},
        {"<sym>dynamicSforzando</sym>", DynamicType::S},
        {"<sym>dynamicZ</sym>", DynamicType::Z},
        {"<sym>dynamicNiente</sym>", DynamicType::N}};

    auto search = dynamic_str_map.find(tag);
    if (search != dynamic_str_map.cend()) {
        return search->second;
    }

    return DynamicType::OTHER;
}

bool TConv::dynamic_valid(const AsciiStringView &tag) {
    auto it = std::find_if(::dynamic_types.cbegin(), ::dynamic_types.cend(), [tag](const DynamicItem &i) { return i.xml == tag; });

    if (it != ::dynamic_types.cend()) {
        return true;
    }
    return false;
}

const muse::TranslatableString &TConv::user_name(DynamicType v) {
    auto it = std::find_if(::dynamic_types.cbegin(), ::dynamic_types.cend(), [v](const DynamicItem &i) { return i.type == v; });

    IF_ASSERT_FAILED(it != ::dynamic_types.cend()) {
        static TranslatableString dummy;
        return dummy;
    }
    return it->user_name;
}

String TConv::translated_user_name(DynamicType v) { return user_name(v).translated(); }

AsciiStringView TConv::to_xml(DynamicType v) {
    auto it = std::find_if(::dynamic_types.cbegin(), ::dynamic_types.cend(), [v](const DynamicItem &i) { return i.type == v; });

    IF_ASSERT_FAILED(it != ::dynamic_types.cend()) {
        static AsciiStringView dummy;
        return dummy;
    }
    return it->xml;
}

DynamicType TConv::from_xml(const AsciiStringView &tag, DynamicType def) {
    auto it = std::find_if(::dynamic_types.cbegin(), ::dynamic_types.cend(), [tag](const DynamicItem &i) { return i.xml == tag; });

    IF_ASSERT_FAILED(it != ::dynamic_types.cend()) { return def; }
    return it->type;
}

static const std::vector<Item<DynamicSpeed>> dynamic_speeds = {
    {DynamicSpeed::NORMAL, "normal"},
    {DynamicSpeed::SLOW, "slow"},
    {DynamicSpeed::FAST, "fast"},
};

String TConv::translated_user_name(DynamicSpeed v) { return find_capitalized_user_name_by_type(::dynamic_speeds, v).translated(); }

AsciiStringView TConv::to_xml(DynamicSpeed v) { return find_xml_tag_by_type<DynamicSpeed>(::dynamic_speeds, v); }

DynamicSpeed TConv::from_xml(const AsciiStringView &tag, DynamicSpeed def) {
    return find_type_by_xml_tag<DynamicSpeed>(::dynamic_speeds, tag, def);
}

static const std::vector<Item<HookType>> hook_types = {
    {HookType::NONE, "hook_none"},
    {HookType::HOOK_90, "hook_90"},
    {HookType::HOOK_45, "hook_45"},
    {HookType::HOOK_90T, "hook_90t"},
};

String TConv::translated_user_name(HookType v) { return find_capitalized_user_name_by_type(::hook_types, v).translated(); }

String TConv::to_xml(HookType v) { return String::number(static_cast<int>(v)); }

HookType TConv::from_xml(const AsciiStringView &tag, HookType def) {
    bool ok = false;
    int v = tag.to_int(&ok);
    return ok ? HookType(v) : def;
}

static const std::vector<Item<LineType>> line_types = {
    {LineType::SOLID, "solid"}, {LineType::DASHED, "dashed"}, {LineType::DOTTED, "dotted"}};

AsciiStringView TConv::to_xml(LineType v) { return find_xml_tag_by_type(::line_types, v); }

LineType TConv::from_xml(const AsciiStringView &tag, LineType def) {
    // Pre-4.0 files
    bool ok = false;
    if (int v = tag.to_int(&ok); ok) {
        PenStyle pen_style = static_cast<PenStyle>(v);
        switch (pen_style) {
            case PenStyle::NO_PEN: return def;
            case PenStyle::SOLID_LINE: return LineType::SOLID;
            case PenStyle::DASH_LINE:
            case PenStyle::DASH_DOT_LINE:
            case PenStyle::CUSTOM_DASH_LINE: return LineType::DASHED;
            case PenStyle::DOT_LINE:
            case PenStyle::DASH_DOT_DOT_LINE: return LineType::DOTTED;
        }
    }

    return find_type_by_xml_tag(::line_types, tag, def);
}

static const std::vector<Item<KeyMode>> key_modes = {
    {KeyMode::UNKNOWN, "unknown"}, {KeyMode::NONE, "none"},         {KeyMode::MAJOR, "major"},     {KeyMode::MINOR, "minor"},
    {KeyMode::DORIAN, "dorian"},   {KeyMode::PHRYGIAN, "phrygian"}, {KeyMode::LYDIAN, "lydian"},   {KeyMode::MIXOLYDIAN, "mixolydian"},
    {KeyMode::AEOLIAN, "aeolian"}, {KeyMode::IONIAN, "ionian"},     {KeyMode::LOCRIAN, "locrian"},
};

String TConv::translated_user_name(KeyMode v) { return find_capitalized_user_name_by_type(::key_modes, v).translated(); }

AsciiStringView TConv::to_xml(KeyMode v) { return find_xml_tag_by_type<KeyMode>(::key_modes, v); }

KeyMode TConv::from_xml(const AsciiStringView &tag, KeyMode def) { return find_type_by_xml_tag<KeyMode>(::key_modes, tag, def); }

static const std::vector<Item<TextStyleType>> textstyle_types = {
    {TextStyleType::DEFAULT, "default", muse::TranslatableString("engraving", "Default")},
    {TextStyleType::TITLE, "title", muse::TranslatableString("engraving", "Title")},
    {TextStyleType::SUBTITLE, "subtitle", muse::TranslatableString("engraving", "Subtitle")},
    {TextStyleType::COMPOSER, "composer", muse::TranslatableString("engraving", "Composer")},
    {TextStyleType::LYRICIST, "poet", muse::TranslatableString("engraving", "Lyricist")},
    {TextStyleType::TRANSLATOR, "translator", muse::TranslatableString("engraving", "Translator")},
    {TextStyleType::FRAME, "frame", muse::TranslatableString("engraving", "Frame")},
    {TextStyleType::INSTRUMENT_EXCERPT, "instrument_excerpt", muse::TranslatableString("engraving", "Instrument name (Part)")},
    {TextStyleType::INSTRUMENT_LONG, "instrument_long", muse::TranslatableString("engraving", "Instrument name (Long)")},
    {TextStyleType::INSTRUMENT_SHORT, "instrument_short", muse::TranslatableString("engraving", "Instrument name (Short)")},
    {TextStyleType::INSTRUMENT_CHANGE, "instrument_change", muse::TranslatableString("engraving", "Instrument change")},
    {TextStyleType::GROUP_BRACKET, "group_bracket", muse::TranslatableString("engraving", "Group bracket")},
    {TextStyleType::HEADER, "header", muse::TranslatableString("engraving", "Header")},
    {TextStyleType::FOOTER, "footer", muse::TranslatableString("engraving", "Footer")},
    {TextStyleType::COPYRIGHT, "copyright", muse::TranslatableString("engraving", "Copyright")},
    {TextStyleType::PAGE_NUMBER, "page_number", muse::TranslatableString("engraving", "Page number")},

    {TextStyleType::MEASURE_NUMBER, "measure_number", muse::TranslatableString("engraving", "Measure number")},
    {TextStyleType::MEASURE_NUMBER_ALTERNATE, "measure_number_alternate",
     muse::TranslatableString("engraving", "Measure number (alternate)")},
    {TextStyleType::MMREST_RANGE, "mmrest_range", muse::TranslatableString("engraving", "Multimeasure rest range")},

    {TextStyleType::TEMPO, "tempo", muse::TranslatableString("engraving", "Tempo")},
    {TextStyleType::TEMPO_CHANGE, "tempo change", muse::TranslatableString("engraving", "Gradual tempo change")},
    {TextStyleType::METRONOME, "metronome", muse::TranslatableString("engraving", "Metronome")},
    {TextStyleType::REPEAT_PLAY_COUNT, "repeat_play_count", muse::TranslatableString("engraving", "Repeat play count")},
    {TextStyleType::REPEAT_LEFT, "repeat_left", muse::TranslatableString("engraving", "Repeat text left")},
    {TextStyleType::REPEAT_RIGHT, "repeat_right", muse::TranslatableString("engraving", "Repeat text right")},
    {TextStyleType::REHEARSAL_MARK, "rehearsal_mark", muse::TranslatableString("engraving", "Rehearsal mark")},
    {TextStyleType::SYSTEM, "system", muse::TranslatableString("engraving", "System")},

    {TextStyleType::STAFF, "staff", muse::TranslatableString("engraving", "Staff")},
    {TextStyleType::STAVE_SHARING, "stave_sharing", muse::TranslatableString("engraving", "Stave sharing label")},
    {TextStyleType::EXPRESSION, "expression", muse::TranslatableString("engraving", "Expression")},
    {TextStyleType::DYNAMICS, "dynamics", muse::TranslatableString("engraving", "Dynamics")},
    {TextStyleType::HAIRPIN, "hairpin", muse::TranslatableString("engraving", "Hairpin")},
    {TextStyleType::LYRICS_ODD, "lyrics_odd", muse::TranslatableString("engraving", "Lyrics odd lines")},
    {TextStyleType::LYRICS_EVEN, "lyrics_even", muse::TranslatableString("engraving", "Lyrics even lines")},
    {TextStyleType::HARMONY_A, "harmony_a", muse::TranslatableString("engraving", "Chord symbol")},
    {TextStyleType::HARMONY_B, "harmony_b", muse::TranslatableString("engraving", "Chord symbol (alternate)")},
    {TextStyleType::HARMONY_ROMAN, "harmony_roman", muse::TranslatableString("engraving", "Roman numeral analysis")},
    {TextStyleType::HARMONY_NASHVILLE, "harmony_nashville", muse::TranslatableString("engraving", "Nashville number")},

    {TextStyleType::TUPLET, "tuplet", muse::TranslatableString("engraving", "Tuplet")},
    {TextStyleType::STICKING, "sticking", muse::TranslatableString("engraving", "Sticking")},
    {TextStyleType::FINGERING, "fingering", muse::TranslatableString("engraving", "Fingering")},
    {TextStyleType::TAB_FRET_NUMBER, "tab_fret_number", muse::TranslatableString("engraving", "Tablature fret number")},
    {TextStyleType::LH_GUITAR_FINGERING, "guitar_fingering_lh", muse::TranslatableString("engraving", "LH guitar fingering")},
    {TextStyleType::RH_GUITAR_FINGERING, "guitar_fingering_rh", muse::TranslatableString("engraving", "RH guitar fingering")},
    {TextStyleType::HAMMER_ON_PULL_OFF, "hammer_on_pull_off", muse::TranslatableString("engraving", "Hammer-ons, pull-offs, and tapping")},
    {TextStyleType::STRING_NUMBER, "string_number", muse::TranslatableString("engraving", "String number")},
    {TextStyleType::STRING_TUNINGS, "string_tunings", muse::TranslatableString("engraving", "String tunings")},
    {TextStyleType::FRET_DIAGRAM_FINGERING, "fret_diagram_fingering", muse::TranslatableString("engraving", "Fretboard diagram fingering")},
    {TextStyleType::FRET_DIAGRAM_FRET_NUMBER, "fret_diagram_fret_number",
     muse::TranslatableString("engraving", "Fretboard diagram fret number")},
    {TextStyleType::HARP_PEDAL_DIAGRAM, "harp_pedal_diagram", muse::TranslatableString("engraving", "Harp pedal diagram")},
    {TextStyleType::HARP_PEDAL_TEXT_DIAGRAM, "harp_pedal_text_diagram", muse::TranslatableString("engraving", "Harp pedal text diagram")},
    {TextStyleType::ARTICULATION, "articulation", muse::TranslatableString("engraving", "Articulation")},

    {TextStyleType::TEXTLINE, "textline", muse::TranslatableString("engraving", "Text line")},
    {TextStyleType::SYSTEM_TEXTLINE, "system_textline", muse::TranslatableString("engraving", "System text line")},
    {TextStyleType::NOTELINE, "noteline", muse::TranslatableString("engraving", "Note-anchored line")},
    {TextStyleType::VOLTA, "volta", muse::TranslatableString("engraving", "Volta")},
    {TextStyleType::OTTAVA, "ottava", muse::TranslatableString("engraving", "Ottava")},
    {TextStyleType::GLISSANDO, "glissando", muse::TranslatableString("engraving", "Glissando")},
    {TextStyleType::PEDAL, "pedal", muse::TranslatableString("engraving", "Pedal")},
    {TextStyleType::BEND, "bend", muse::TranslatableString("engraving", "Bends & Dives")},
    {TextStyleType::LET_RING, "let_ring", muse::TranslatableString("engraving", "Let ring")},
    {TextStyleType::WHAMMY_BAR, "whammy_bar", muse::TranslatableString("engraving", "Whammy bar")},
    {TextStyleType::PALM_MUTE, "palm_mute", muse::TranslatableString("engraving", "Palm mute")},

    {TextStyleType::USER1, "user_1", muse::TranslatableString("engraving", "User-1")},
    {TextStyleType::USER2, "user_2", muse::TranslatableString("engraving", "User-2")},
    {TextStyleType::USER3, "user_3", muse::TranslatableString("engraving", "User-3")},
    {TextStyleType::USER4, "user_4", muse::TranslatableString("engraving", "User-4")},
    {TextStyleType::USER5, "user_5", muse::TranslatableString("engraving", "User-5")},
    {TextStyleType::USER6, "user_6", muse::TranslatableString("engraving", "User-6")},
    {TextStyleType::USER7, "user_7", muse::TranslatableString("engraving", "User-7")},
    {TextStyleType::USER8, "user_8", muse::TranslatableString("engraving", "User-8")},
    {TextStyleType::USER9, "user_9", muse::TranslatableString("engraving", "User-9")},
    {TextStyleType::USER10, "user_10", muse::TranslatableString("engraving", "User-10")},
    {TextStyleType::USER11, "user_11", muse::TranslatableString("engraving", "User-11")},
    {TextStyleType::USER12, "user_12", muse::TranslatableString("engraving", "User-12")},
};

const muse::TranslatableString &TConv::user_name(TextStyleType v) { return find_capitalized_user_name_by_type(::textstyle_types, v); }

String TConv::translated_user_name(TextStyleType v) { return find_capitalized_user_name_by_type(::textstyle_types, v).translated(); }

AsciiStringView TConv::to_xml(TextStyleType v) { return find_xml_tag_by_type<TextStyleType>(::textstyle_types, v); }

TextStyleType TConv::from_xml(const AsciiStringView &tag, TextStyleType def) {
    auto it =
        std::find_if(::textstyle_types.cbegin(), ::textstyle_types.cend(), [tag](const Item<TextStyleType> &i) { return i.xml == tag; });

    if (it != ::textstyle_types.cend()) {
        return it->type;
    }

    // compatibility

    static const std::map<AsciiStringView, TextStyleType> old_tst_tags = {{"Default", TextStyleType::DEFAULT},
                                                                          {"Title", TextStyleType::TITLE},
                                                                          {"Subtitle", TextStyleType::SUBTITLE},
                                                                          {"Composer", TextStyleType::COMPOSER},
                                                                          {"Lyricist", TextStyleType::LYRICIST},
                                                                          {"Translator", TextStyleType::TRANSLATOR},
                                                                          {"Frame", TextStyleType::FRAME},
                                                                          {"Instrument Name (Part)", TextStyleType::INSTRUMENT_EXCERPT},
                                                                          {"Instrument Name (Long)", TextStyleType::INSTRUMENT_LONG},
                                                                          {"Instrument Name (Short)", TextStyleType::INSTRUMENT_SHORT},
                                                                          {"Instrument Change", TextStyleType::INSTRUMENT_CHANGE},
                                                                          {"Header", TextStyleType::HEADER},
                                                                          {"Footer", TextStyleType::FOOTER},
                                                                          {"Copyright", TextStyleType::COPYRIGHT},
                                                                          {"Page Number", TextStyleType::PAGE_NUMBER},

                                                                          {"Measure Number", TextStyleType::MEASURE_NUMBER},
                                                                          {"Multimeasure Rest Range", TextStyleType::MMREST_RANGE},

                                                                          {"Tempo", TextStyleType::TEMPO},
                                                                          {"Metronome", TextStyleType::METRONOME},
                                                                          {"Repeat Text Left", TextStyleType::REPEAT_LEFT},
                                                                          {"Repeat Text Right", TextStyleType::REPEAT_RIGHT},
                                                                          {"Rehearsal Mark", TextStyleType::REHEARSAL_MARK},
                                                                          {"System", TextStyleType::SYSTEM},

                                                                          {"Staff", TextStyleType::STAFF},
                                                                          {"Expression", TextStyleType::EXPRESSION},
                                                                          {"Dynamics", TextStyleType::DYNAMICS},
                                                                          {"Hairpin", TextStyleType::HAIRPIN},
                                                                          {"Lyrics Odd Lines", TextStyleType::LYRICS_ODD},
                                                                          {"Lyrics Even Lines", TextStyleType::LYRICS_EVEN},
                                                                          {"Chord Symbol", TextStyleType::HARMONY_A},
                                                                          {"Chord Symbol (Alternate)", TextStyleType::HARMONY_B},
                                                                          {"Roman Numeral Analysis", TextStyleType::HARMONY_ROMAN},
                                                                          {"Nashville Number", TextStyleType::HARMONY_NASHVILLE},

                                                                          {"Tuplet", TextStyleType::TUPLET},
                                                                          {"Sticking", TextStyleType::STICKING},
                                                                          {"Fingering", TextStyleType::FINGERING},
                                                                          {"LH Guitar Fingering", TextStyleType::LH_GUITAR_FINGERING},
                                                                          {"RH Guitar Fingering", TextStyleType::RH_GUITAR_FINGERING},
                                                                          {"String Number", TextStyleType::STRING_NUMBER},

                                                                          {"Text Line", TextStyleType::TEXTLINE},
                                                                          {"Note-anchored Line", TextStyleType::NOTELINE},
                                                                          {"Volta", TextStyleType::VOLTA},
                                                                          {"Ottava", TextStyleType::OTTAVA},
                                                                          {"Glissando", TextStyleType::GLISSANDO},
                                                                          {"Pedal", TextStyleType::PEDAL},
                                                                          {"Bend", TextStyleType::BEND},
                                                                          {"Let Ring", TextStyleType::LET_RING},
                                                                          {"Palm Mute", TextStyleType::PALM_MUTE},

                                                                          {"User-1", TextStyleType::USER1},
                                                                          {"User-2", TextStyleType::USER2},
                                                                          {"User-3", TextStyleType::USER3},
                                                                          {"User-4", TextStyleType::USER4},
                                                                          {"User-5", TextStyleType::USER5},
                                                                          {"User-6", TextStyleType::USER6},
                                                                          {"User-7", TextStyleType::USER7},
                                                                          {"User-8", TextStyleType::USER8},
                                                                          {"User-9", TextStyleType::USER9},
                                                                          {"User-10", TextStyleType::USER10},
                                                                          {"User-11", TextStyleType::USER11},
                                                                          {"User-12", TextStyleType::USER12},

                                                                          {"Technique", TextStyleType::EXPRESSION},

                                                                          {"12", TextStyleType::DYNAMICS},
                                                                          {"26", TextStyleType::STAFF}};

    auto old = old_tst_tags.find(tag);
    if (old != old_tst_tags.cend()) {
        return old->second;
    }

    LOGE() << "not found type for tag: " << tag;
    UNREACHABLE;
    return def;
}

static const std::vector<Item<ChangeMethod>> change_methods = {
    {ChangeMethod::NORMAL, "normal"},           {ChangeMethod::EASE_IN, "ease-in"},         {ChangeMethod::EASE_OUT, "ease-out"},
    {ChangeMethod::EASE_IN_OUT, "ease-in-out"}, {ChangeMethod::EXPONENTIAL, "exponential"},
};

static double easing_factor(const double x, const ChangeMethod method) {
    switch (method) {
        case ChangeMethod::NORMAL: return x;
        case ChangeMethod::EASE_IN: return 1 - std::sqrt(1 - std::pow(x, 2));
        case ChangeMethod::EASE_OUT: return std::sqrt(1 - std::pow(x - 1, 2));
        case ChangeMethod::EASE_IN_OUT:
            if (x < 0.5) {
                return (1.0 - std::sqrt(1 - std::pow(2 * x, 2))) / 2;
            } else {
                return (std::sqrt(1.0 - std::pow(-2 * x + 2, 2)) + 1) / 2;
            }
        case ChangeMethod::EXPONENTIAL:
            if (muse::real_is_equal(x, 1.0)) {
                return x;
            } else {
                return 1.0 - std::pow(2, -10 * x);
            }
    }

    return 1.0;
}

template <typename T>
static std::map<int /*tickPosition*/, T> build_eased_value_curve(const int ticks_duration, const int steps_count, const T amplitude,
                                                                 const ChangeMethod method) {
    if (steps_count <= 0) {
        return {};
    }

    std::map<int, T> result;

    double duration_step = static_cast<double>(ticks_duration) / static_cast<double>(steps_count);

    for (int i = 0; i <= steps_count; ++i) {
        result.emplace(i * duration_step, easing_factor(static_cast<double>(i) / static_cast<double>(steps_count), method) * amplitude);
    }

    return result;
}

std::map<int, int> TConv::easing_value_curve(const int ticks_duration, const int steps_count, const int amplitude,
                                             const ChangeMethod method) {
    return build_eased_value_curve(ticks_duration, steps_count, amplitude, method);
}

std::map<int, double> TConv::easing_value_curve(const int ticks_duration, const int steps_count, const double amplitude,
                                                const ChangeMethod method) {
    return build_eased_value_curve(ticks_duration, steps_count, amplitude, method);
}

AsciiStringView TConv::to_xml(ChangeMethod v) { return find_xml_tag_by_type<ChangeMethod>(::change_methods, v); }

ChangeMethod TConv::from_xml(const AsciiStringView &tag, ChangeMethod def) {
    return find_type_by_xml_tag<ChangeMethod>(::change_methods, tag, def);
}

String TConv::to_xml(const PitchValue &v) {
    return String(u"point time=\"%1\" pitch=\"%2\" vibrato=\"%3\"").arg(v.time).arg(v.pitch).arg(v.vibrato);
}

struct AccidentalUnicodeItem {
    AccidentalVal accidental;
    const char *name = nullptr;
    const char *full_name = nullptr;
};

static const std::vector<AccidentalUnicodeItem> accidentals_names = {
    {AccidentalVal::SHARP3,
     //: Visible text in the UI. Please preserve the accidental symbol in the translation
     MUSE_TRANSLATE_NOOP("engraving", "triple ♯"),
     //: Accessible text for screen readers. Please avoid using accidental symbols in the translation
     MUSE_TRANSLATE_NOOP("engraving", "triple sharp")},
    {AccidentalVal::SHARP2,
     //: Visible text in the UI. Please preserve the accidental symbol in the translation
     MUSE_TRANSLATE_NOOP("engraving", "double ♯"),
     //: Accessible text for screen readers. Please avoid using accidental symbols in the translation
     MUSE_TRANSLATE_NOOP("engraving", "double sharp")},
    {AccidentalVal::SHARP,
     //: Visible text in the UI. Please preserve the accidental symbol in the translation
     MUSE_TRANSLATE_NOOP("engraving", "♯"),
     //: Accessible text for screen readers. Please avoid using accidental symbols in the translation
     MUSE_TRANSLATE_NOOP("engraving", "sharp")},
    {AccidentalVal::NATURAL,
     //: Visible text in the UI. Please preserve the accidental symbol in the translation
     MUSE_TRANSLATE_NOOP("engraving", "♮"),
     //: Accessible text for screen readers. Please avoid using accidental symbols in the translation
     MUSE_TRANSLATE_NOOP("engraving", "natural")},
    {AccidentalVal::FLAT,
     //: Visible text in the UI. Please preserve the accidental symbol in the translation
     MUSE_TRANSLATE_NOOP("engraving", "♭"),
     //: Accessible text for screen readers. Please avoid using accidental symbols in the translation
     MUSE_TRANSLATE_NOOP("engraving", "flat")},
    {AccidentalVal::FLAT2,
     //: Visible text in the UI. Please preserve the accidental symbol in the translation
     MUSE_TRANSLATE_NOOP("engraving", "double ♭"),
     //: Accessible text for screen readers. Please avoid using accidental symbols in the translation
     MUSE_TRANSLATE_NOOP("engraving", "double flat")},
    {AccidentalVal::FLAT3,
     //: Visible text in the UI. Please preserve the accidental symbol in the translation
     MUSE_TRANSLATE_NOOP("engraving", "triple ♭"),
     //: Accessible text for screen readers. Please avoid using accidental symbols in the translation
     MUSE_TRANSLATE_NOOP("engraving", "triple flat")}};

const char *TConv::user_name(AccidentalVal accidental, bool full) {
    auto it = std::find_if(::accidentals_names.cbegin(), ::accidentals_names.cend(),
                           [accidental](const AccidentalUnicodeItem &i) { return i.accidental == accidental; });

    if (it != ::accidentals_names.cend()) {
        return full ? it->full_name : it->name;
    }

    return "";
}

String TConv::to_xml(AccidentalRole v) { return String::number(static_cast<int>(v)); }

AccidentalRole TConv::from_xml(const AsciiStringView &tag, AccidentalRole def) {
    bool ok = false;
    int r = tag.to_int(&ok);
    return ok ? static_cast<AccidentalRole>(r) : def;
}

static const std::vector<Item<GuitarBendType>> guitar_bend_types = {
    {GuitarBendType::BEND, "bend"},
    {GuitarBendType::PRE_BEND, "pre-bend"},
    {GuitarBendType::GRACE_NOTE_BEND, "grace-note-bend"},
    {GuitarBendType::SLIGHT_BEND, "slight-bend"},
    {GuitarBendType::DIVE, "dive"},
    {GuitarBendType::PRE_DIVE, "pre-dive"},
    {GuitarBendType::DIP, "dip"},
    {GuitarBendType::SCOOP, "scoop"},
};

AsciiStringView TConv::to_xml(GuitarBendType v) { return find_xml_tag_by_type(::guitar_bend_types, v); }

GuitarBendType TConv::from_xml(const AsciiStringView &tag, GuitarBendType def) {
    return find_type_by_xml_tag(::guitar_bend_types, tag, def);
}

static const std::vector<Item<NoteCaseType>> note_case_types = {
    {NoteCaseType::AUTO, "auto"},
    {NoteCaseType::CAPITAL, "capital"},
    {NoteCaseType::LOWER, "lower"},
    {NoteCaseType::UPPER, "upper"},
};

AsciiStringView TConv::to_xml(NoteCaseType v) { return find_xml_tag_by_type(::note_case_types, v); }

NoteCaseType TConv::from_xml(const AsciiStringView &tag, NoteCaseType def) { return find_type_by_xml_tag(::note_case_types, tag, def); }

String TConv::to_xml(BeatsPerSecond v, int precision) { return String::number(v.val, precision); }

BeatsPerSecond TConv::from_xml(const AsciiStringView &tag, BeatsPerSecond def) {
    bool ok = false;
    double v = tag.to_double(&ok);
    return ok ? BeatsPerSecond(v) : def;
}

static const std::vector<Item<DurationType>> duration_types = {
    {DurationType::V_QUARTER, "quarter", muse::TranslatableString("engraving", "Quarter")},
    {DurationType::V_EIGHTH, "eighth", muse::TranslatableString("engraving", "Eighth")},
    {DurationType::V_1024TH, "1024th", muse::TranslatableString("engraving", "1024th")},
    {DurationType::V_512TH, "512th", muse::TranslatableString("engraving", "512th")},
    {DurationType::V_256TH, "256th", muse::TranslatableString("engraving", "256th")},
    {DurationType::V_128TH, "128th", muse::TranslatableString("engraving", "128th")},
    {DurationType::V_64TH, "64th", muse::TranslatableString("engraving", "64th")},
    {DurationType::V_32ND, "32nd", muse::TranslatableString("engraving", "32nd")},
    {DurationType::V_16TH, "16th", muse::TranslatableString("engraving", "16th")},
    {DurationType::V_HALF, "half", muse::TranslatableString("engraving", "Half")},
    {DurationType::V_WHOLE, "whole", muse::TranslatableString("engraving", "Whole")},
    {DurationType::V_MEASURE, "measure", muse::TranslatableString("engraving", "Measure")},
    {DurationType::V_BREVE, "breve", muse::TranslatableString("engraving", "Breve")},
    {DurationType::V_LONG, "long", muse::TranslatableString("engraving", "Longa")},
    {DurationType::V_ZERO, "", muse::TranslatableString("engraving", "Zero")},
    {DurationType::V_INVALID, "", muse::TranslatableString("engraving", "Invalid")},
};

String TConv::translated_user_name(DurationType v) { return find_capitalized_user_name_by_type(::duration_types, v).translated(); }

AsciiStringView TConv::to_xml(DurationType v) { return find_xml_tag_by_type<DurationType>(::duration_types, v); }

DurationType TConv::from_xml(const AsciiStringView &tag, DurationType def) {
    return find_type_by_xml_tag<DurationType>(::duration_types, tag, def);
}

static const std::vector<Item<PlayingTechniqueType>> play_tech_types = {
    {PlayingTechniqueType::UNDEFINED, "undefined", muse::TranslatableString::untranslatable("Undefined")},
    {PlayingTechniqueType::NATURAL, "natural", muse::TranslatableString("engraving/playtechtype", "Normal")},
    {PlayingTechniqueType::PIZZICATO, "pizzicato", muse::TranslatableString("engraving/playtechtype", "Pizzicato")},
    //: For brass and plucked string instruments: staff text that prescribes to play without mute, see https://en.wikipedia.org/wiki/Mute_(music)
    {PlayingTechniqueType::OPEN, "open", muse::TranslatableString("engraving/playtechtype", "Open")},
    //: For brass and plucked string instruments: staff text that prescribes to use mute while playing, see https://en.wikipedia.org/wiki/Mute_(music)
    {PlayingTechniqueType::MUTE, "mute", muse::TranslatableString("engraving/playtechtype", "Mute")},
    {PlayingTechniqueType::TREMOLO, "tremolo", muse::TranslatableString("engraving/playtechtype", "Tremolo")},
    {PlayingTechniqueType::DETACHE, "detache", muse::TranslatableString("engraving/playtechtype", "Détaché")},
    {PlayingTechniqueType::MARTELE, "martele", muse::TranslatableString("engraving/playtechtype", "Martelé")},
    {PlayingTechniqueType::COL_LEGNO, "col_legno", muse::TranslatableString("engraving/playtechtype", "Col legno")},
    {PlayingTechniqueType::SUL_PONTICELLO, "sul_ponticello", muse::TranslatableString("engraving/playtechtype", "Sul ponticello")},
    {PlayingTechniqueType::SUL_TASTO, "sul_tasto", muse::TranslatableString("engraving/playtechtype", "Sul tasto")},
    {PlayingTechniqueType::VIBRATO, "vibrato", muse::TranslatableString("engraving/playtechtype", "Vibrato")},
    {PlayingTechniqueType::LEGATO, "legato", muse::TranslatableString("engraving/playtechtype", "Legato")},
    {PlayingTechniqueType::DISTORTION, "distortion", muse::TranslatableString("engraving/playtechtype", "Distortion")},
    {PlayingTechniqueType::OVERDRIVE, "overdrive", muse::TranslatableString("engraving/playtechtype", "Overdrive")},
    {PlayingTechniqueType::HARMONICS, "harmonics", muse::TranslatableString("engraving/playtechtype", "Harmonics")},
    {PlayingTechniqueType::JAZZ_TONE, "jazz_tone", muse::TranslatableString("engraving/playtechtype", "Jazz tone")},
    // Handbells
    {PlayingTechniqueType::HANDBELLS_SWING, "handbells_swing", muse::TranslatableString("engraving/playtechtype", "Swing")},
    {PlayingTechniqueType::HANDBELLS_SWING_UP, "handbells_swing_up", muse::TranslatableString("engraving/playtechtype", "Swing up")},
    {PlayingTechniqueType::HANDBELLS_SWING_DOWN, "handbells_swing_down", muse::TranslatableString("engraving/playtechtype", "Swing down")},
    {PlayingTechniqueType::HANDBELLS_ECHO1, "handbells_echo_1", muse::TranslatableString("engraving/playtechtype", "Echo")},
    {PlayingTechniqueType::HANDBELLS_ECHO2, "handbells_echo_2", muse::TranslatableString("engraving/playtechtype", "Echo")},
    {PlayingTechniqueType::HANDBELLS_DAMP, "handbells_damp", muse::TranslatableString("engraving/playtechtype", "Damp")},
    {PlayingTechniqueType::HANDBELLS_LV, "handbells_lv", muse::TranslatableString("engraving/playtechtype", "Let vibrate")},
    {PlayingTechniqueType::HANDBELLS_R, "handbells_r", muse::TranslatableString("engraving/playtechtype", "Ring")},
};

const muse::TranslatableString &TConv::user_name(PlayingTechniqueType v) {
    return find_capitalized_user_name_by_type(::play_tech_types, v);
}

AsciiStringView TConv::to_xml(PlayingTechniqueType v) { return find_xml_tag_by_type<PlayingTechniqueType>(::play_tech_types, v); }

PlayingTechniqueType TConv::from_xml(const AsciiStringView &tag, PlayingTechniqueType def) {
    return find_type_by_xml_tag<PlayingTechniqueType>(::play_tech_types, tag, def);
}

static const std::vector<Item<GradualTempoChangeType>> tempo_change_types = {
    {GradualTempoChangeType::UNDEFINED, "undefined"},
    {GradualTempoChangeType::ACCELERANDO, "accelerando", muse::TranslatableString("engraving/gradualtempochangetype", "accel.")},
    {GradualTempoChangeType::ALLARGANDO, "allargando", muse::TranslatableString("engraving/gradualtempochangetype", "allarg.")},
    {GradualTempoChangeType::CALANDO, "calando", muse::TranslatableString("engraving/gradualtempochangetype", "calando")},
    {GradualTempoChangeType::LENTANDO, "lentando", muse::TranslatableString("engraving/gradualtempochangetype", "lentando")},
    {GradualTempoChangeType::MORENDO, "morendo", muse::TranslatableString("engraving/gradualtempochangetype", "morendo")},
    {GradualTempoChangeType::PRECIPITANDO, "precipitando", muse::TranslatableString("engraving/gradualtempochangetype", "precipitando")},
    {GradualTempoChangeType::RALLENTANDO, "rallentando", muse::TranslatableString("engraving/gradualtempochangetype", "rall.")},
    {GradualTempoChangeType::RITARDANDO, "ritardando", muse::TranslatableString("engraving/gradualtempochangetype", "rit.")},
    {GradualTempoChangeType::SMORZANDO, "smorzando", muse::TranslatableString("engraving/gradualtempochangetype", "smorz.")},
    {GradualTempoChangeType::SOSTENUTO, "sostenuto", muse::TranslatableString("engraving/gradualtempochangetype", "sost.")},
    {GradualTempoChangeType::STRINGENDO, "stringendo", muse::TranslatableString("engraving/gradualtempochangetype", "string.")}};

const muse::TranslatableString &TConv::user_name(GradualTempoChangeType v) {
    return find_capitalized_user_name_by_type(::tempo_change_types, v);
}

AsciiStringView TConv::to_xml(GradualTempoChangeType v) { return find_xml_tag_by_type<GradualTempoChangeType>(::tempo_change_types, v); }

GradualTempoChangeType TConv::from_xml(const AsciiStringView &tag, GradualTempoChangeType def) {
    return find_type_by_xml_tag<GradualTempoChangeType>(::tempo_change_types, tag, def);
}

static const std::vector<Item<OrnamentStyle>> ornamentstyle_types = {{OrnamentStyle::BAROQUE, "baroque"},
                                                                     {OrnamentStyle::DEFAULT, "default"}};

AsciiStringView TConv::to_xml(OrnamentStyle v) { return find_xml_tag_by_type<OrnamentStyle>(::ornamentstyle_types, v); }

OrnamentStyle TConv::from_xml(const AsciiStringView &tag, OrnamentStyle def) {
    return find_type_by_xml_tag<OrnamentStyle>(::ornamentstyle_types, tag, def);
}

static const std::vector<Item<PlacementV>> placementv_types = {{PlacementV::ABOVE, "above"}, {PlacementV::BELOW, "below"}};

AsciiStringView TConv::to_xml(PlacementV v) { return find_xml_tag_by_type<PlacementV>(::placementv_types, v); }

PlacementV TConv::from_xml(const AsciiStringView &tag, PlacementV def) {
    return find_type_by_xml_tag<PlacementV>(::placementv_types, tag, def);
}

static const std::vector<Item<PlacementH>> placementh_types = {
    {PlacementH::LEFT, "left"}, {PlacementH::RIGHT, "center"}, {PlacementH::CENTER, "right"}};

AsciiStringView TConv::to_xml(PlacementH v) { return find_xml_tag_by_type<PlacementH>(::placementh_types, v); }

PlacementH TConv::from_xml(const AsciiStringView &tag, PlacementH def) {
    return find_type_by_xml_tag<PlacementH>(::placementh_types, tag, def);
}

static const std::vector<Item<TextPlace>> textplace_types = {
    {TextPlace::AUTO, "auto"}, {TextPlace::ABOVE, "above"}, {TextPlace::BELOW, "below"}, {TextPlace::LEFT, "left"}};

AsciiStringView TConv::to_xml(TextPlace v) { return find_xml_tag_by_type<TextPlace>(::textplace_types, v); }

TextPlace TConv::from_xml(const AsciiStringView &tag, TextPlace def) {
    auto it = std::find_if(::textplace_types.cbegin(), ::textplace_types.cend(), [tag](const Item<TextPlace> &i) { return i.xml == tag; });

    if (it != ::textplace_types.cend()) {
        return it->type;
    }

    // compatibility
    static const std::vector<Item<TextPlace>> old_textplace_types = {
        {TextPlace::AUTO, "0"}, {TextPlace::ABOVE, "1"}, {TextPlace::BELOW, "2"}, {TextPlace::LEFT, "3"}};

    auto oldit =
        std::find_if(old_textplace_types.cbegin(), old_textplace_types.cend(), [tag](const Item<TextPlace> &i) { return i.xml == tag; });

    IF_ASSERT_FAILED(oldit != old_textplace_types.cend()) { return def; }
    return oldit->type;
}

static const std::array<Item<DirectionV>, 3> directionv_types = {{
    {DirectionV::AUTO, "auto", muse::TranslatableString("engraving", "Auto")},
    {DirectionV::UP, "up", muse::TranslatableString("engraving", "Up")},
    {DirectionV::DOWN, "down", muse::TranslatableString("engraving", "Down")},
}};

String TConv::translated_user_name(DirectionV v) { return find_capitalized_user_name_by_type(::directionv_types, v).translated(); }

AsciiStringView TConv::to_xml(DirectionV v) { return find_xml_tag_by_type<DirectionV>(::directionv_types, v); }

DirectionV TConv::from_xml(const AsciiStringView &tag, DirectionV def) {
    auto it =
        std::find_if(::directionv_types.cbegin(), ::directionv_types.cend(), [tag](const Item<DirectionV> &i) { return i.xml == tag; });

    if (it != ::directionv_types.cend()) {
        return it->type;
    }

    // compatibility
    //    bool ok = false;
    //    int v = tag.toInt(&ok);
    //    if (ok) {
    //        return static_cast<DirectionV>(v);
    //    }

    return def;
}

static const std::vector<Item<DirectionH>> directionh_types = {
    {DirectionH::AUTO, "auto", muse::TranslatableString("engraving", "Auto")},
    {DirectionH::RIGHT, "right", muse::TranslatableString("engraving", "Right")},
    {DirectionH::LEFT, "left", muse::TranslatableString("engraving", "Left")},
};

String TConv::translated_user_name(DirectionH v) { return find_capitalized_user_name_by_type(::directionh_types, v).translated(); }

AsciiStringView TConv::to_xml(DirectionH v) { return find_xml_tag_by_type<DirectionH>(::directionh_types, v); }

DirectionH TConv::from_xml(const AsciiStringView &tag, DirectionH def) {
    auto it =
        std::find_if(::directionh_types.cbegin(), ::directionh_types.cend(), [tag](const Item<DirectionH> &i) { return i.xml == tag; });

    if (it != ::directionh_types.cend()) {
        return it->type;
    }

    // compatibility
    bool ok = false;
    int v = tag.to_int(&ok);
    if (ok) {
        return static_cast<DirectionH>(v);
    }

    return def;
}

static const std::vector<Item<LayoutBreakType>> layoutbreak_types = {
    {LayoutBreakType::LINE, "line", muse::TranslatableString("engraving/layoutbreaktype", "System break")},
    {LayoutBreakType::PAGE, "page", muse::TranslatableString("engraving/layoutbreaktype", "Page break")},
    {LayoutBreakType::SECTION, "section", muse::TranslatableString("engraving/layoutbreaktype", "Section break")},
    {LayoutBreakType::NOBREAK, "nobreak", muse::TranslatableString("engraving/layoutbreaktype", "Keep measures on the same system")}};

const muse::TranslatableString &TConv::user_name(LayoutBreakType v) { return find_capitalized_user_name_by_type(::layoutbreak_types, v); }

AsciiStringView TConv::to_xml(LayoutBreakType v) { return find_xml_tag_by_type<LayoutBreakType>(::layoutbreak_types, v); }

LayoutBreakType TConv::from_xml(const AsciiStringView &tag, LayoutBreakType def) {
    return find_type_by_xml_tag<LayoutBreakType>(::layoutbreak_types, tag, def);
}

static const std::vector<Item<VeloType>> velo_types = {{VeloType::OFFSET_VAL, "offset"}, {VeloType::USER_VAL, "user"}};

AsciiStringView TConv::to_xml(VeloType v) { return find_xml_tag_by_type<VeloType>(::velo_types, v); }

VeloType TConv::from_xml(const AsciiStringView &tag, VeloType def) { return find_type_by_xml_tag<VeloType>(::velo_types, tag, def); }

static const std::vector<Item<BeamMode>> beammode_types = {
    {BeamMode::AUTO, "auto"}, {BeamMode::BEGIN, "begin"},     {BeamMode::MID, "mid"},         {BeamMode::END, "end"},
    {BeamMode::NONE, "no"},   {BeamMode::BEGIN16, "begin16"}, {BeamMode::BEGIN32, "begin32"}, {BeamMode::INVALID, "invalid"}};

AsciiStringView TConv::to_xml(BeamMode v) { return find_xml_tag_by_type<BeamMode>(::beammode_types, v); }

BeamMode TConv::from_xml(const AsciiStringView &tag, BeamMode def) {
    auto it = std::find_if(::beammode_types.cbegin(), ::beammode_types.cend(), [tag](const Item<BeamMode> &i) { return i.xml == tag; });

    if (it != ::beammode_types.cend()) {
        return it->type;
    }

    // compatibility
    bool ok = false;
    int v = tag.to_int(&ok);
    if (ok) {
        return static_cast<BeamMode>(v);
    }

    return def;
}

static const std::vector<Item<GlissandoStyle>> glissandostyle_types = {{GlissandoStyle::BLACK_KEYS, "blackkeys"},
                                                                       {GlissandoStyle::WHITE_KEYS, "whitekeys"},
                                                                       {GlissandoStyle::DIATONIC, "diatonic"},
                                                                       {GlissandoStyle::PORTAMENTO, "portamento"},
                                                                       {GlissandoStyle::CHROMATIC, "chromatic"}};

AsciiStringView TConv::to_xml(GlissandoStyle v) { return find_xml_tag_by_type<GlissandoStyle>(::glissandostyle_types, v); }

GlissandoStyle TConv::from_xml(const AsciiStringView &tag, GlissandoStyle def) {
    auto it = std::find_if(::glissandostyle_types.cbegin(), ::glissandostyle_types.cend(),
                           [tag](const Item<GlissandoStyle> &i) { return i.xml == tag; });

    if (it != ::glissandostyle_types.cend()) {
        return it->type;
    }

    // compatibility
    if (tag == "Chromatic") {
        return GlissandoStyle::CHROMATIC;
    }

    return def;
}

static const std::vector<Item<BarLineType>> barline_types = {{BarLineType::NORMAL, "normal"},
                                                             {BarLineType::DOUBLE, "double"},
                                                             {BarLineType::START_REPEAT, "start-repeat"},
                                                             {BarLineType::END_REPEAT, "end-repeat"},
                                                             {BarLineType::BROKEN, "dashed"},
                                                             {BarLineType::END, "end"},
                                                             {BarLineType::END_START_REPEAT, "end-start-repeat"},
                                                             {BarLineType::DOTTED, "dotted"},
                                                             {BarLineType::REVERSE_END, "reverse-end"},
                                                             {BarLineType::HEAVY, "heavy"},
                                                             {BarLineType::DOUBLE_HEAVY, "double-heavy"}};

AsciiStringView TConv::to_xml(BarLineType v) { return find_xml_tag_by_type<BarLineType>(::barline_types, v); }

BarLineType TConv::from_xml(const AsciiStringView &tag, BarLineType def) {
    auto it = std::find_if(::barline_types.cbegin(), ::barline_types.cend(), [tag](const Item<BarLineType> &i) { return i.xml == tag; });

    if (it != ::barline_types.cend()) {
        return it->type;
    }

    // compatibility
    bool ok = false;
    int v = tag.to_int(&ok);
    if (ok) {
        return static_cast<BarLineType>(v);
    }

    return def;
}

static const std::array<Item<TremoloType>, 14> tremolo_types = {
    {{TremoloType::INVALID_TREMOLO, ""},
     {TremoloType::R8, "r8", muse::TranslatableString("engraving/tremolotype", "Eighth through stem")},
     {TremoloType::R16, "r16", muse::TranslatableString("engraving/tremolotype", "16th through stem")},
     {TremoloType::R32, "r32", muse::TranslatableString("engraving/tremolotype", "32nd through stem")},
     {TremoloType::R64, "r64", muse::TranslatableString("engraving/tremolotype", "64th through stem")},
     {TremoloType::R128, "r128", muse::TranslatableString("engraving/tremolotype", "128th through stem")},
     {TremoloType::R256, "r256", muse::TranslatableString("engraving/tremolotype", "256th through stem")},
     {TremoloType::BUZZ_ROLL, "buzzroll", muse::TranslatableString("engraving/tremolotype", "Buzz roll")},
     {TremoloType::C8, "c8", muse::TranslatableString("engraving/tremolotype", "Eighth between notes")},
     {TremoloType::C16, "c16", muse::TranslatableString("engraving/tremolotype", "16th between notes")},
     {TremoloType::C32, "c32", muse::TranslatableString("engraving/tremolotype", "32nd between notes")},
     {TremoloType::C64, "c64", muse::TranslatableString("engraving/tremolotype", "64th between notes")},
     {TremoloType::C128, "c128", muse::TranslatableString("engraving/tremolotype", "128th between notes")},
     {TremoloType::C256, "c256", muse::TranslatableString("engraving/tremolotype", "256th between notes")}}};

const muse::TranslatableString &TConv::user_name(TremoloType v) { return find_capitalized_user_name_by_type(::tremolo_types, v); }

AsciiStringView TConv::to_xml(TremoloType v) { return find_xml_tag_by_type<TremoloType>(::tremolo_types, v); }

TremoloType TConv::from_xml(const AsciiStringView &tag, TremoloType def) {
    return find_type_by_xml_tag<TremoloType>(::tremolo_types, tag, def);
}

static const std::vector<Item<TremoloBarType>> tremolobar_types = {{{TremoloBarType::DIP, "dip"},
                                                                    {TremoloBarType::DIVE, "dive"},
                                                                    {TremoloBarType::RELEASE_UP, "release (up)"},
                                                                    {TremoloBarType::INVERTED_DIP, "inverted dip"},
                                                                    {TremoloBarType::RETURN, "return"},
                                                                    {TremoloBarType::RELEASE_DOWN, "release (down)"},
                                                                    {TremoloBarType::CUSTOM, "custom"}}};

AsciiStringView TConv::to_xml(TremoloBarType v) { return find_xml_tag_by_type<TremoloBarType>(::tremolobar_types, v); }

TremoloBarType TConv::from_xml(const AsciiStringView &tag, TremoloBarType def) {
    return find_type_by_xml_tag<TremoloBarType>(::tremolobar_types, tag, def);
}

static const std::vector<Item<BracketType>> bracket_types = {
    {BracketType::NORMAL, "Normal", muse::TranslatableString("engraving/brackettype", "Normal")},
    {BracketType::BRACE, "Brace", muse::TranslatableString("engraving/brackettype", "Brace")},
    {BracketType::SQUARE, "Square", muse::TranslatableString("engraving/brackettype", "Square")},
    {BracketType::LINE, "Line", muse::TranslatableString("engraving/brackettype", "Line")},
    {BracketType::GROUP, "Group", muse::TranslatableString("engraving/brackettype", "Group")},
    {BracketType::NO_BRACKET, "NoBracket", muse::TranslatableString("engraving/brackettype", "No bracket")}};

const muse::TranslatableString &TConv::user_name(BracketType v) { return find_capitalized_user_name_by_type(::bracket_types, v); }

String TConv::translated_user_name(BracketType v) { return find_capitalized_user_name_by_type(::bracket_types, v).translated(); }

AsciiStringView TConv::to_xml(BracketType v) { return find_xml_tag_by_type<BracketType>(::bracket_types, v); }

BracketType TConv::from_xml(const AsciiStringView &tag, BracketType def) {
    auto it = std::find_if(::bracket_types.cbegin(), ::bracket_types.cend(), [tag](const Item<BracketType> &i) { return i.xml == tag; });

    if (it != ::bracket_types.cend()) {
        return it->type;
    }

    // compatibility
    if (tag == "Akkolade") {
        return BracketType::BRACE;
    }

    return def;
}

//! TODO Add xml names
static const std::array<Item<ArpeggioType>, 6> arpeggio_types = {
    {{ArpeggioType::NORMAL, "0", muse::TranslatableString("engraving", "Arpeggio")},
     {ArpeggioType::UP, "1", muse::TranslatableString("engraving", "Up arpeggio")},
     {ArpeggioType::DOWN, "2", muse::TranslatableString("engraving", "Down arpeggio")},
     {ArpeggioType::BRACKET, "3", muse::TranslatableString("engraving", "Bracket arpeggio")},
     {ArpeggioType::UP_STRAIGHT, "4", muse::TranslatableString("engraving", "Up arpeggio straight")},
     {ArpeggioType::DOWN_STRAIGHT, "5", muse::TranslatableString("engraving", "Down arpeggio straight")}}};

const muse::TranslatableString &TConv::user_name(ArpeggioType v) { return find_capitalized_user_name_by_type(::arpeggio_types, v); }

AsciiStringView TConv::to_xml(ArpeggioType v) { return find_xml_tag_by_type<ArpeggioType>(::arpeggio_types, v); }

ArpeggioType TConv::from_xml(const AsciiStringView &tag, ArpeggioType def) {
    return find_type_by_xml_tag<ArpeggioType>(::arpeggio_types, tag, def);
}

struct EmbelItem {
    muse::TranslatableString name;
    AsciiStringView notes;
};

// TODO: Can't use .arg, because Palettes use these strings and doesn't support TranslatableString
static const std::vector<EmbelItem> embellishment_types = {
    // Single Grace notes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace low G"), "LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace low A"), "LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace B"), "B"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace C"), "C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace D"), "D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace E"), "E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace F"), "F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace high G"), "HG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Single grace high A"), "HA"},

    // Double Grace notes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "D LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "D B"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "E LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "E B"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "E C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "E D"},

    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "F LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "F LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "F B"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "F C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "F D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "F E"},

    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HG LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HG LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HG B"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HG C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HG D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HG E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HG F"},

    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HA LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HA B"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HA C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HA D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HA E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HA F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double grace"), "HA HG"},

    // Half Doublings
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on low G"), "LG D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on low A"), "LA D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on B"), "B D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on C"), "C D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on D"), "D E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on E"), "E F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on F"), "F HG"},
    // ? { muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on high G"), "HG F" },
    // ? { muse::TranslatableString("engraving/bagpipeembellishment", "Half doubling on high A"), "HA HG" },

    // Regular Doublings
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on high G"), "HG F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on high A"), "HA HG"},

    // Half Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half strike on low A"), "LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half strike on B"), "B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half strike on C"), "C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half strike on D"), "D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half strike on D"), "D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half strike on E"), "E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half strike on F"), "F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half strike on high G"), "HG F"},

    // Regular Grip
    {muse::TranslatableString("engraving/bagpipeembellishment", "Grip"), "D LG"},

    // D Throw
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half D throw"), "D C"},

    // Regular Doublings (continued)
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on low G"), "HG LG D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on low A"), "HG LA D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on B"), "HG B D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on C"), "HG C D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on D"), "HG D E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on E"), "HG E F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Doubling on F"), "HG F HG"},

    // Thumb Doublings
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb doubling on low G"), "HA LG D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb doubling on low A"), "HA LA D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb doubling on B"), "HA B D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb doubling on C"), "HA C D"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb doubling on D"), "HA D E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb doubling on E"), "HA E F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb doubling on F"), "HA F HG"},
    // ? { muse::TranslatableString("engraving/bagpipeembellishment", "Thumb doubling on high G"), "HA HG F" },

    // G Grace note Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note on low A"), "HG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note on B"), "HG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note on C"), "HG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note on D"), "HG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note on D"), "HG D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note on E"), "HG E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note on F"), "HG F E"},

    // Regular Double Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on low A"), "LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on B"), "LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on C"), "LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on D"), "LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on D"), "C D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on E"), "LA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on F"), "E F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on high G"), "F HG F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Double strike on high A"), "HG HA HG"},

    // Thumb Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb strike on low A"), "HA LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb strike on B"), "HA B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb strike on C"), "HA C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb strike on D"), "HA D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb strike on D"), "HA D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb strike on E"), "HA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb strike on F"), "HA F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb strike on high G"), "HA HG F"},

    // Regular Grips (continued)
    {muse::TranslatableString("engraving/bagpipeembellishment", "Grip"), "LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Grip"), "LG B LG"},

    // Taorluath and Birl
    {muse::TranslatableString("engraving/bagpipeembellishment", "Birl"), "LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "D throw"), "LG D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half heavy D throw"), "D LG C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Taorluath"), "D LG E"},

    // Birl, Bubbly, D Throws (cont/bagpipeembellishmentinued) and Taorluaths (continued)
    {muse::TranslatableString("engraving/bagpipeembellishment", "Birl"), "LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Bubbly"), "D LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Heavy D throw"), "LG D LG C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Taorluath"), "LG D LG E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Taorluath"), "LG B LG E"},

    // Half Double Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on low A"), "LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on B"), "B LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on C"), "C LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on D"), "D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on D"), "D C D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on E"), "E LA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on F"), "F E F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on high G"), "HG F HG F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half double strike on high A"), "HA HG HA HG"},

    // Half Grips
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on low A"), "LA LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on B"), "B LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on C"), "C LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on D"), "D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on D"), "D LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on E"), "E LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on F"), "F LG F LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on high G"), "HG LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half grip on high A"), "HA LG D LG"},

    // Half Peles
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half pele on low A"), "LA E LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half pele on B"), "B E B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half pele on C"), "C E C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half pele on D"), "D E D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half pele on D"), "D E D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half pele on E"), "E F E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half pele on F"), "F HG F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half pele on high G"), "HG HA HG F"},

    // G Grace note Grips
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note grip on low A"), "HG LA LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note grip on B"), "HG B LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note grip on C"), "HG C LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note grip on D"), "HG D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note grip on D"), "HG D LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note grip on E"), "HG E LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note grip on F"), "HG F LG F LG"},

    // Thumb Grips
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grip on low A"), "HA LA LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grip on B"), "HA B LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grip on C"), "HA C LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grip on D"), "HA D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grip on D"), "HA D LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grip on E"), "HA E LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grip on F"), "HA F LG F LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grip on high G"), "HA HG LG F LG"},

    // Bubbly
    {muse::TranslatableString("engraving/bagpipeembellishment", "Bubbly"), "LG D LG C LG"},

    //  Birls
    {muse::TranslatableString("engraving/bagpipeembellishment", "Birl"), "HG LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Birl"), "HA LA LG LA LG"},

    // Regular Peles
    {muse::TranslatableString("engraving/bagpipeembellishment", "Pele on low A"), "HG LA E LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Pele on B"), "HG B E B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Pele on C"), "HG C E C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Pele on D"), "HG D E D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Pele on D"), "HG D E D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Pele on E"), "HG E F E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Pele on F"), "HG F HG F E"},

    // Thumb Grace Note Peles
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grace note pele on low A"), "HA LA E LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grace note pele on B"), "HA B E B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grace note pele on C"), "HA C E C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grace note pele on D"), "HA D E D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grace note pele on D"), "HA D E D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grace note pele on E"), "HA E F E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grace note pele on F"), "HA F HG F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb grace note pele on high G"), "HA HG HA HG F"},

    // G Grace note Double Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note double strike on low A"), "HG LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note double strike on B"), "HG B LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note double strike on C"), "HG C LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note double strike on D"), "HG D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note double strike on D"), "HG D C D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note double strike on E"), "HG E LA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note double strike on F"), "HG F E F E"},

    // Thumb Double Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb double strike on low A"), "HA LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb double strike on B"), "HA B LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb double strike on C"), "HA C LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb double strike on D"), "HA D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb double strike on D"), "HA D C D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb double strike on E"), "HA E LA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb double strike on F"), "HA F E F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb double strike on high G"), "HA HG F HG F"},

    // Regular Triple Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on low A"), "LG LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on B"), "LG B LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on C"), "LG C LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on D"), "LG D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on D"), "C D C D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on E"), "LA E LA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on F"), "E F E F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on high G"), "F HG F HG F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Triple strike on high A"), "HG HA HG HA HG"},

    // Half Triple Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on low A"), "LA LG LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on B"), "B LG B LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on C"), "C LG C LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on D"), "D LG D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on D"), "D C D C D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on E"), "E LA E LA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on F"), "F E F E F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on high G"), "HG F HG F HG F"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Half triple strike on high A"), "HA HG HA HG HA HG"},

    // G Grace note Triple Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note triple strike on low A"), "HG LA LG LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note triple strike on B"), "HG B LG B LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note triple strike on C"), "HG C LG C LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note triple strike on D"), "HG D LG D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note triple strike on D"), "HG D C D C D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note triple strike on E"), "HG E LA E LA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "G grace note triple strike on F"), "HG F E F E F E"},

    // Thumb Triple Strikes
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb triple strike on low A"), "HA LA LG LA LG LA LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb triple strike on B"), "HA B LG B LG B LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb triple strike on C"), "HA C LG C LG C LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb triple strike on D"), "HA D LG D LG D LG"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb triple strike on D"), "HA D C D C D C"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb triple strike on E"), "HA E LA E LA E LA"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb triple strike on F"), "HA F E F E F E"},
    {muse::TranslatableString("engraving/bagpipeembellishment", "Thumb triple strike on high G"), "HA HG F HG F HG F"},
};

const muse::TranslatableString &TConv::user_name(EmbellishmentType v) { return ::embellishment_types.at(static_cast<size_t>(v)).name; }

String TConv::translated_user_name(EmbellishmentType v) { return ::embellishment_types.at(static_cast<size_t>(v)).name.translated(); }

String TConv::to_xml(EmbellishmentType v) { return String::number(static_cast<size_t>(v)); }

EmbellishmentType TConv::from_xml(const AsciiStringView &tag, EmbellishmentType def) {
    bool ok = false;
    int v = tag.to_int(&ok);
    if (ok) {
        return static_cast<EmbellishmentType>(v);
    }
    return def;
}

StringList TConv::embellishment_notes(EmbellishmentType v) {
    return String::from_ascii(::embellishment_types.at(v).notes.ascii()).split(u' ');
}

size_t TConv::embellishments_count() { return ::embellishment_types.size(); }

struct ChordLineNameType {
    ChordLineType type;
    bool straight;
    bool wavy;

    bool operator==(const ChordLineNameType &other) const { return type == other.type && straight == other.straight && wavy == other.wavy; }
};

//! TODO Add xml names
static const std::array<Item<ChordLineNameType>, 15> chordline_types = {
    {{{ChordLineType::NOTYPE, false, false}, "0"},
     {{ChordLineType::FALL, false, false}, "1", muse::TranslatableString("engraving", "Fall")},
     {{ChordLineType::DOIT, false, false}, "2", muse::TranslatableString("engraving", "Doit")},
     {{ChordLineType::PLOP, false, false}, "3", muse::TranslatableString("engraving", "Plop")},
     {{ChordLineType::SCOOP, false, false}, "4", muse::TranslatableString("engraving", "Scoop")},
     {{ChordLineType::NOTYPE, true, false}, "0"},
     {{ChordLineType::FALL, true, false}, "1", muse::TranslatableString("engraving", "Slide out down")},
     {{ChordLineType::DOIT, true, false}, "2", muse::TranslatableString("engraving", "Slide out up")},
     {{ChordLineType::PLOP, true, false}, "3", muse::TranslatableString("engraving", "Slide in above")},
     {{ChordLineType::SCOOP, true, false}, "4", muse::TranslatableString("engraving", "Slide in below")},
     {{ChordLineType::NOTYPE, true, true}, "0"},
     {{ChordLineType::FALL, true, true}, "1", muse::TranslatableString("engraving", "Slide out down (rough)")},
     {{ChordLineType::DOIT, true, true}, "2", muse::TranslatableString("engraving", "Slide out up (rough)")},
     {{ChordLineType::PLOP, true, true}, "3", muse::TranslatableString("engraving", "Slide in above (rough)")},
     {{ChordLineType::SCOOP, true, true}, "4", muse::TranslatableString("engraving", "Slide in below (rough)")}}};

const muse::TranslatableString &TConv::user_name(ChordLineType v, bool straight, bool wavy) {
    return find_capitalized_user_name_by_type(::chordline_types, ChordLineNameType{v, straight, wavy});
}

AsciiStringView TConv::to_xml(ChordLineType v) { return find_xml_tag_by_type<ChordLineNameType>(::chordline_types, {v, false, false}); }

ChordLineType TConv::from_xml(const AsciiStringView &tag, ChordLineType def) {
    return find_type_by_xml_tag<ChordLineNameType>(::chordline_types, tag, {def, false, false}).type;
}

struct DrumPitchItem {
    DrumNum num = DrumNum(0);
    String user_name;
};

// TODO: Can't use TranslatableString, because Drumset uses these strings and doesn't support TranslatableString
static const std::vector<DrumPitchItem> drumpitchs = {
    {DrumNum(27), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "High Q")},
    {DrumNum(28), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Slap")},
    {DrumNum(29), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Scratch Push")},

    {DrumNum(30), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Scratch Pull")},
    {DrumNum(31), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Sticks")},
    {DrumNum(32), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Square Click")},
    {DrumNum(33), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Metronome Click")},
    {DrumNum(34), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Metronome Bell")},
    {DrumNum(35), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Acoustic Bass Drum")},
    {DrumNum(36), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Bass Drum 1")},
    {DrumNum(37), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Side Stick")},
    {DrumNum(38), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Acoustic Snare")},
    {DrumNum(39), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Hand Clap")},

    {DrumNum(40), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Electric Snare")},
    {DrumNum(41), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Low Floor Tom")},
    {DrumNum(42), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Closed Hi-Hat")},
    {DrumNum(43), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "High Floor Tom")},
    {DrumNum(44), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Pedal Hi-Hat")},
    {DrumNum(45), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Low Tom")},
    {DrumNum(46), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Open Hi-Hat")},
    {DrumNum(47), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Low-Mid Tom")},
    {DrumNum(48), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Hi-Mid Tom")},
    {DrumNum(49), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Crash Cymbal 1")},

    {DrumNum(50), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "High Tom")},
    {DrumNum(51), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Ride Cymbal 1")},
    {DrumNum(52), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Chinese Cymbal")},
    {DrumNum(53), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Ride Bell")},
    {DrumNum(54), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Tambourine")},
    {DrumNum(55), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Splash Cymbal")},
    {DrumNum(56), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Cowbell")},
    {DrumNum(57), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Crash Cymbal 2")},
    {DrumNum(58), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Vibraslap")},
    {DrumNum(59), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Ride Cymbal 2")},

    {DrumNum(60), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Hi Bongo")},
    {DrumNum(61), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Low Bongo")},
    {DrumNum(62), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Mute Hi Conga")},
    {DrumNum(63), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Open Hi Conga")},
    {DrumNum(64), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Low Conga")},
    {DrumNum(65), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "High Timbale")},
    {DrumNum(66), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Low Timbale")},
    {DrumNum(67), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "High Agogo")},
    {DrumNum(68), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Low Agogo")},
    {DrumNum(69), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Cabasa")},

    {DrumNum(70), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Maracas")},
    {DrumNum(71), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Short Whistle")},
    {DrumNum(72), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Long Whistle")},
    {DrumNum(73), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Short Güiro")},
    {DrumNum(74), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Long Güiro")},
    {DrumNum(75), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Claves")},
    {DrumNum(76), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Hi Wood Block")},
    {DrumNum(77), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Low Wood Block")},
    {DrumNum(78), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Mute Cuica")},
    {DrumNum(79), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Open Cuica")},

    {DrumNum(80), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Mute Triangle")},
    {DrumNum(81), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Open Triangle")},
    {DrumNum(82), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Shaker")},
    {DrumNum(83), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Sleigh Bell")},
    {DrumNum(84), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Mark Tree")},
    {DrumNum(85), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Castanets")},
    {DrumNum(86), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Mute Surdo")},
    {DrumNum(87), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Open Surdo")},

    {DrumNum(91), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Snare (Rim shot)")},

    {DrumNum(93), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Ride (Edge)")},

    {DrumNum(99), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Cowbell Low")},

    {DrumNum(102), MUSE_TRANSLATE_NOOP_U16("engraving/drumset", "Cowbell High")},
};

const String &TConv::user_name(DrumNum v) {
    auto it = std::find_if(::drumpitchs.cbegin(), ::drumpitchs.cend(), [v](const DrumPitchItem &i) { return i.num == v; });

    IF_ASSERT_FAILED(it != ::drumpitchs.cend()) {
        static const String dummy;
        return dummy;
    }
    return it->user_name;
}

//! TODO Add xml names
static const std::array<Item<GlissandoType>, 2> glissando_types = {
    {{GlissandoType::STRAIGHT, "0", muse::TranslatableString("engraving", "Straight glissando")},
     {GlissandoType::WAVY, "1", muse::TranslatableString("engraving", "Wavy glissando")}}};

const muse::TranslatableString &TConv::user_name(GlissandoType v) { return find_capitalized_user_name_by_type(::glissando_types, v); }

AsciiStringView TConv::to_xml(GlissandoType v) { return find_xml_tag_by_type<GlissandoType>(::glissando_types, v); }

GlissandoType TConv::from_xml(const AsciiStringView &tag, GlissandoType def) {
    return find_type_by_xml_tag<GlissandoType>(::glissando_types, tag, def);
}

static const std::vector<Item<JumpType>> jump_types = {
    {JumpType::DC, "dc", muse::TranslatableString("engraving", "Da Capo")},
    {JumpType::DC_AL_FINE, "dcalfine", muse::TranslatableString("engraving", "Da Capo al Fine")},
    {JumpType::DC_AL_CODA, "dcalcoda", muse::TranslatableString("engraving", "Da Capo al Coda")},
    {JumpType::DS_AL_CODA, "dsalcoda", muse::TranslatableString("engraving", "D.S. al Coda")},
    {JumpType::DS_AL_FINE, "dsalfine", muse::TranslatableString("engraving", "D.S. al Fine")},
    {JumpType::DS, "ds", muse::TranslatableString("engraving", "D.S.")},

    {JumpType::DC_AL_DBLCODA, "dcaldblcoda", muse::TranslatableString("engraving", "Da Capo al Doppia Coda")},
    {JumpType::DS_AL_DBLCODA, "dsaldblcoda", muse::TranslatableString("engraving", "Dal Segno al Doppia Coda")},
    {JumpType::DSS, "dss", muse::TranslatableString("engraving", "Dal Doppio Segno")},
    {JumpType::DSS_AL_CODA, "dssalcoda", muse::TranslatableString("engraving", "Dal Doppio Segno al Coda")},
    {JumpType::DSS_AL_DBLCODA, "dssaldblcoda", muse::TranslatableString("engraving", "Dal Doppio Segno al Doppia Coda")},
    {JumpType::DSS_AL_FINE, "dssalfine", muse::TranslatableString("engraving", "Dal Doppio Segno al Fine")},

    {JumpType::USER, "user", muse::TranslatableString("engraving", "Custom")}};

const muse::TranslatableString &TConv::user_name(JumpType v) { return find_capitalized_user_name_by_type(::jump_types, v); }

String TConv::translated_user_name(JumpType v) { return find_capitalized_user_name_by_type(::jump_types, v).translated(); }

static const std::array<Item<MarkerType>, 11> marker_types = {
    {{MarkerType::SEGNO, "segno", muse::TranslatableString("engraving", "Segno")},
     {MarkerType::VARSEGNO, "varsegno", muse::TranslatableString("engraving", "Segno variation")},
     {MarkerType::CODA, "codab", muse::TranslatableString("engraving", "Coda")},
     {MarkerType::VARCODA, "varcoda", muse::TranslatableString("engraving", "Varied coda")},
     {MarkerType::CODETTA, "codetta", muse::TranslatableString("engraving", "Doppia Coda")},
     {MarkerType::FINE, "fine", muse::TranslatableString("engraving", "Fine")},
     {MarkerType::TOCODA, "coda", muse::TranslatableString("engraving", "To coda")},
     {MarkerType::TOCODASYM, "codasym", muse::TranslatableString("engraving", "To coda (symbol)")},
     {MarkerType::DA_CODA, "dacoda", muse::TranslatableString("engraving", "Da Coda")},
     {MarkerType::DA_DBLCODA, "dadblcoda", muse::TranslatableString("engraving", "Da Doppia Coda")},
     {MarkerType::USER, "user", muse::TranslatableString("engraving", "Custom")}}};

const muse::TranslatableString &TConv::user_name(MarkerType v) { return find_capitalized_user_name_by_type(::marker_types, v); }

String TConv::translated_user_name(MarkerType v) { return find_capitalized_user_name_by_type(::marker_types, v).translated(); }

AsciiStringView TConv::to_xml(MarkerType v) { return find_xml_tag_by_type<MarkerType>(::marker_types, v); }

MarkerType TConv::from_xml(const AsciiStringView &tag, MarkerType def) {
    auto it = std::find_if(::marker_types.cbegin(), ::marker_types.cend(), [tag](const Item<MarkerType> &i) { return i.xml == tag; });

    if (it != ::marker_types.cend()) {
        return it->type;
    }

    // compatibility

    if (tag == "Repeat") {
        return MarkerType::TOCODA;
    }

    return def;
}

static const std::array<Item<StaffGroup>, 3> staffgroup_types = {{
    {StaffGroup::STANDARD, "pitched", muse::TranslatableString("engraving/staffgroup", "Standard")},
    {StaffGroup::PERCUSSION, "percussion", muse::TranslatableString("engraving/staffgroup", "Percussion")},
    {StaffGroup::TAB, "tablature", muse::TranslatableString("engraving/staffgroup", "Tablature")},
}};

String TConv::translated_user_name(StaffGroup v) { return find_capitalized_user_name_by_type(::staffgroup_types, v).translated(); }

AsciiStringView TConv::to_xml(StaffGroup v) { return find_xml_tag_by_type<StaffGroup>(::staffgroup_types, v); }

StaffGroup TConv::from_xml(const AsciiStringView &tag, StaffGroup def) {
    return find_type_by_xml_tag<StaffGroup>(::staffgroup_types, tag, def);
}

static const std::array<Item<StaffTypes>, 27> stafftypes_items = {{
    {StaffTypes::STANDARD, "stdNormal", muse::TranslatableString("engraving/stafftypes", "Standard")},
    {StaffTypes::PERC_1LINE, "perc1Line", muse::TranslatableString("engraving/stafftypes", "Perc. 1 line")},
    {StaffTypes::PERC_2LINE, "perc2Line", muse::TranslatableString("engraving/stafftypes", "Perc. 2 lines")},
    {StaffTypes::PERC_3LINE, "perc3Line", muse::TranslatableString("engraving/stafftypes", "Perc. 3 lines")},
    {StaffTypes::PERC_5LINE, "perc5Line", muse::TranslatableString("engraving/stafftypes", "Perc. 5 lines")},
    {StaffTypes::TAB_6SIMPLE, "tab6StrSimple", muse::TranslatableString("engraving/stafftypes", "Tab. 6-str. simple")},
    {StaffTypes::TAB_6COMMON, "tab6StrCommon", muse::TranslatableString("engraving/stafftypes", "Tab. 6-str. common")},
    {StaffTypes::TAB_6FULL, "tab6StrFull", muse::TranslatableString("engraving/stafftypes", "Tab. 6-str. full")},
    {StaffTypes::TAB_4SIMPLE, "tab4StrSimple", muse::TranslatableString("engraving/stafftypes", "Tab. 4-str. simple")},
    {StaffTypes::TAB_4COMMON, "tab4StrCommon", muse::TranslatableString("engraving/stafftypes", "Tab. 4-str. common")},
    {StaffTypes::TAB_4FULL, "tab4StrFull", muse::TranslatableString("engraving/stafftypes", "Tab. 4-str. full")},
    {StaffTypes::TAB_5SIMPLE, "tab5StrSimple", muse::TranslatableString("engraving/stafftypes", "Tab. 5-str. simple")},
    {StaffTypes::TAB_5COMMON, "tab5StrCommon", muse::TranslatableString("engraving/stafftypes", "Tab. 5-str. common")},
    {StaffTypes::TAB_5FULL, "tab5StrFull", muse::TranslatableString("engraving/stafftypes", "Tab. 5-str. full")},
    {StaffTypes::TAB_UKULELE, "tabUkulele", muse::TranslatableString("engraving/stafftypes", "Tab. ukulele")},
    {StaffTypes::TAB_BALALAJKA, "tabBalajka", muse::TranslatableString("engraving/stafftypes", "Tab. balalaika")},
    {StaffTypes::TAB_DULCIMER, "tabDulcimer", muse::TranslatableString("engraving/stafftypes", "Tab. dulcimer")},
    {StaffTypes::TAB_ITALIAN, "tab6StrItalian", muse::TranslatableString("engraving/stafftypes", "Tab. 6-str. Italian")},
    {StaffTypes::TAB_FRENCH, "tab6StrFrench", muse::TranslatableString("engraving/stafftypes", "Tab. 6-str. French")},
    {StaffTypes::TAB_7COMMON, "tab7StrCommon", muse::TranslatableString("engraving/stafftypes", "Tab. 7-str. common")},
    {StaffTypes::TAB_8COMMON, "tab8StrCommon", muse::TranslatableString("engraving/stafftypes", "Tab. 8-str. common")},
    {StaffTypes::TAB_9COMMON, "tab9StrCommon", muse::TranslatableString("engraving/stafftypes", "Tab. 9-str. common")},
    {StaffTypes::TAB_10COMMON, "tab10StrCommon", muse::TranslatableString("engraving/stafftypes", "Tab. 10-str. common")},
    {StaffTypes::TAB_7SIMPLE, "tab7StrSimple", muse::TranslatableString("engraving/stafftypes", "Tab. 7-str. simple")},
    {StaffTypes::TAB_8SIMPLE, "tab8StrSimple", muse::TranslatableString("engraving/stafftypes", "Tab. 8-str. simple")},
    {StaffTypes::TAB_9SIMPLE, "tab9StrSimple", muse::TranslatableString("engraving/stafftypes", "Tab. 9-str. simple")},
    {StaffTypes::TAB_10SIMPLE, "tab10StrSimple", muse::TranslatableString("engraving/stafftypes", "Tab. 10-str. simple")},
}};

const TranslatableString &TConv::user_name(StaffTypes v) { return find_capitalized_user_name_by_type(::stafftypes_items, v); }

String TConv::translated_user_name(StaffTypes v) { return find_capitalized_user_name_by_type(::stafftypes_items, v).translated(); }

AsciiStringView TConv::to_xml(StaffTypes v) { return find_xml_tag_by_type<StaffTypes>(::stafftypes_items, v); }

StaffTypes TConv::from_xml(const AsciiStringView &tag, StaffTypes def) {
    return find_type_by_xml_tag<StaffTypes>(::stafftypes_items, tag, def);
}

const std::array<Item<TrillType>, 4> trill_types = {
    {{TrillType::TRILL_LINE, "trill", muse::TranslatableString("engraving/trilltype", "Trill line")},
     {TrillType::UPPRALL_LINE, "upprall", muse::TranslatableString("engraving/trilltype", "Upprall line")},
     {TrillType::DOWNPRALL_LINE, "downprall", muse::TranslatableString("engraving/trilltype", "Downprall line")},
     {TrillType::PRALLPRALL_LINE, "prallprall", muse::TranslatableString("engraving/trilltype", "Prallprall line")}}};

const muse::TranslatableString &TConv::user_name(TrillType v) { return find_capitalized_user_name_by_type(::trill_types, v); }

String TConv::translated_user_name(TrillType v) { return find_capitalized_user_name_by_type(::trill_types, v).translated(); }

AsciiStringView TConv::to_xml(TrillType v) { return find_xml_tag_by_type<TrillType>(::trill_types, v); }

TrillType TConv::from_xml(const AsciiStringView &tag, TrillType def) {
    auto it = std::find_if(::trill_types.cbegin(), ::trill_types.cend(), [tag](const Item<TrillType> &i) { return i.xml == tag; });

    if (it != ::trill_types.cend()) {
        return it->type;
    }

    // compatibility

    if (tag == "0") {
        return TrillType::TRILL_LINE;
    } else if (tag == "pure") {
        return TrillType::PRALLPRALL_LINE; // obsolete, compatibility only
    }

    return def;
}

const std::array<Item<VibratoType>, 5> vibrato_types = {
    {{VibratoType::NONE, "none", muse::TranslatableString("engraving/vibratotype", "None")},
     {VibratoType::GUITAR_VIBRATO, "guitarVibrato", muse::TranslatableString("engraving/vibratotype", "Guitar vibrato")},
     {VibratoType::GUITAR_VIBRATO_WIDE, "guitarVibratoWide", muse::TranslatableString("engraving/vibratotype", "Guitar vibrato wide")},
     {VibratoType::VIBRATO_SAWTOOTH, "vibratoSawtooth", muse::TranslatableString("engraving/vibratotype", "Vibrato sawtooth")},
     {VibratoType::VIBRATO_SAWTOOTH_WIDE, "vibratoSawtoothWide",
      muse::TranslatableString("engraving/vibratotype", "Tremolo sawtooth wide")}}};

const muse::TranslatableString &TConv::user_name(VibratoType v) { return find_capitalized_user_name_by_type(::vibrato_types, v); }

String TConv::translated_user_name(VibratoType v) { return find_capitalized_user_name_by_type(::vibrato_types, v).translated(); }

AsciiStringView TConv::to_xml(VibratoType v) { return find_xml_tag_by_type<VibratoType>(::vibrato_types, v); }

VibratoType TConv::from_xml(const AsciiStringView &tag, VibratoType def) {
    return find_type_by_xml_tag<VibratoType>(::vibrato_types, tag, def);
}

// Note about "engraving/sym": they need to be in this context because PaletteCell::translationContext expects them there
struct ArticulationTextTypeItem {
    ArticulationTextType type;
    AsciiStringView xml;
    String text;
    muse::TranslatableString name;
};

const std::array<ArticulationTextTypeItem, 9> articulationtext_types = {{
    // Guitar
    {ArticulationTextType::SLAP, "Slap", String(u"S"), muse::TranslatableString("engraving/sym", "Slap")},
    {ArticulationTextType::POP, "Pop", String(u"P"), muse::TranslatableString("engraving/sym", "Pop")},
    // Handbells
    {ArticulationTextType::TD, "TD", String(u"TD"), muse::TranslatableString("engraving/sym", "Thumb damp")},
    {ArticulationTextType::BD, "BD", String(u"BD"), muse::TranslatableString("engraving/sym", "Brush damp")},
    {ArticulationTextType::RT, "RT", String(u"RT"), muse::TranslatableString("engraving/sym", "Ring touch")},
    {ArticulationTextType::PL, "PL", String(u"Pl"), muse::TranslatableString("engraving/sym", "Pluck")},
    {ArticulationTextType::SB, "SB", String(u"SB"), muse::TranslatableString("engraving/sym", "Singing bell")},
    {ArticulationTextType::VIB, "VIB", String(u"vib."), muse::TranslatableString("engraving/sym", "Vibrato")},
}};

const muse::TranslatableString &TConv::user_name(ArticulationTextType v) {
    auto it = std::find_if(::articulationtext_types.cbegin(), ::articulationtext_types.cend(),
                           [v](const ArticulationTextTypeItem &i) { return i.type == v; });

    IF_ASSERT_FAILED(it != ::articulationtext_types.cend()) {
        static muse::TranslatableString dummy;
        return dummy;
    }
    return it->name;
}

String TConv::text(ArticulationTextType v) {
    auto it = std::find_if(::articulationtext_types.cbegin(), ::articulationtext_types.cend(),
                           [v](const ArticulationTextTypeItem &i) { return i.type == v; });

    IF_ASSERT_FAILED(it != ::articulationtext_types.cend()) {
        static String dummy;
        return dummy;
    }
    return it->text;
}

AsciiStringView TConv::to_xml(ArticulationTextType v) {
    auto it = std::find_if(::articulationtext_types.cbegin(), ::articulationtext_types.cend(),
                           [v](const ArticulationTextTypeItem &i) { return i.type == v; });

    IF_ASSERT_FAILED(it != ::articulationtext_types.cend()) {
        static AsciiStringView dummy;
        return dummy;
    }
    return it->xml;
}

ArticulationTextType TConv::from_xml(const AsciiStringView &tag, ArticulationTextType def) {
    auto it = std::find_if(::articulationtext_types.cbegin(), ::articulationtext_types.cend(),
                           [tag](const ArticulationTextTypeItem &i) { return i.xml == tag; });

    if (it != ::articulationtext_types.cend()) {
        return it->type;
    }

    // compatibility
    return def;
}

const std::array<Item<LyricsSyllabic>, 4> lyricssyllabic_types = {{{LyricsSyllabic::SINGLE, "single"},
                                                                   {LyricsSyllabic::BEGIN, "begin"},
                                                                   {LyricsSyllabic::END, "end"},
                                                                   {LyricsSyllabic::MIDDLE, "middle"}}};

AsciiStringView TConv::to_xml(LyricsSyllabic v) { return find_xml_tag_by_type<LyricsSyllabic>(::lyricssyllabic_types, v); }

LyricsSyllabic TConv::from_xml(const AsciiStringView &tag, LyricsSyllabic def) {
    return find_type_by_xml_tag<LyricsSyllabic>(::lyricssyllabic_types, tag, def);
}

const std::array<Item<LyricsDashSystemStart>, 3> lyrics_dash_system_start_types = {{
    {LyricsDashSystemStart::STANDARD, "standard"},
    {LyricsDashSystemStart::UNDER_HEADER, "underHeader"},
    {LyricsDashSystemStart::UNDER_FIRST_NOTE, "underFirstNote"},
}};

AsciiStringView TConv::to_xml(LyricsDashSystemStart v) {
    return find_xml_tag_by_type<LyricsDashSystemStart>(::lyrics_dash_system_start_types, v);
}

LyricsDashSystemStart TConv::from_xml(const AsciiStringView &tag, LyricsDashSystemStart def) {
    return find_type_by_xml_tag<LyricsDashSystemStart>(::lyrics_dash_system_start_types, tag, def);
}

const std::array<const muse::TranslatableString, 17> key_names = {
    {muse::TranslatableString("engraving", "C♭ major / A♭ minor"), muse::TranslatableString("engraving", "G♭ major / E♭ minor"),
     muse::TranslatableString("engraving", "D♭ major / B♭ minor"), muse::TranslatableString("engraving", "A♭ major / F minor"),
     muse::TranslatableString("engraving", "E♭ major / C minor"), muse::TranslatableString("engraving", "B♭ major / G minor"),
     muse::TranslatableString("engraving", "F major / D minor"), muse::TranslatableString("engraving", "C major / A minor"),
     muse::TranslatableString("engraving", "G major / E minor"), muse::TranslatableString("engraving", "D major / B minor"),
     muse::TranslatableString("engraving", "A major / F♯ minor"), muse::TranslatableString("engraving", "E major / C♯ minor"),
     muse::TranslatableString("engraving", "B major / G♯ minor"), muse::TranslatableString("engraving", "F♯ major / D♯ minor"),
     muse::TranslatableString("engraving", "C♯ major / A♯ minor"), muse::TranslatableString("engraving", "Open / Atonal"),
     muse::TranslatableString("engraving", "Custom")}};

const muse::TranslatableString &TConv::user_name(Key v, bool is_atonal, bool is_custom) {
    if (is_atonal) {
        return ::key_names[15];
    } else if (is_custom) {
        return ::key_names[16];
    }

    int key_int = static_cast<int>(v);
    return ::key_names[key_int + 7];
}

String TConv::translated_user_name(Key v, bool is_atonal, bool is_custom) { return user_name(v, is_atonal, is_custom).translated(); }

const std::array<Item<MeasureNumberPlacement>, 4> measure_number_modes = {{
    {MeasureNumberPlacement::ABOVE_SYSTEM, "above-system"},
    {MeasureNumberPlacement::BELOW_SYSTEM, "below-system"},
    {MeasureNumberPlacement::ON_SYSTEM_OBJECT_STAVES, "on-so-staves"},
    {MeasureNumberPlacement::ON_ALL_STAVES, "on-all-staves"},
}};

AsciiStringView TConv::to_xml(MeasureNumberPlacement v) { return find_xml_tag_by_type<MeasureNumberPlacement>(::measure_number_modes, v); }

MeasureNumberPlacement TConv::from_xml(const AsciiStringView &tag, MeasureNumberPlacement def) {
    return find_type_by_xml_tag<MeasureNumberPlacement>(::measure_number_modes, tag, def);
}

const std::array<Item<InstrumentNamesAlign>, 4> instr_labels_align = {{{InstrumentNamesAlign::RIGHT_RIGHT, "right-right"},
                                                                       {InstrumentNamesAlign::CENTER_RIGHT, "center-right"},
                                                                       {InstrumentNamesAlign::CENTER_CENTER, "center-center"},
                                                                       {InstrumentNamesAlign::LEFT_RIGHT, "left-right"}}};

mu::engraving::AsciiStringView mu::engraving::TConv::to_xml(InstrumentNamesAlign v) {
    return find_xml_tag_by_type<InstrumentNamesAlign>(::instr_labels_align, v);
}

InstrumentNamesAlign TConv::from_xml(const AsciiStringView &str, InstrumentNamesAlign def) {
    return find_type_by_xml_tag<InstrumentNamesAlign>(::instr_labels_align, str, def);
}

const std::array<Item<InstrumentNamesFormat>, 4> instr_names_format = {{{InstrumentNamesFormat::NAME_IN_TRANSP_NUM, "name-in-transp-num"},
                                                                        {InstrumentNamesFormat::NAME_NUM_IN_TRANSP, "name-num-in-transp"},
                                                                        {InstrumentNamesFormat::TRANSP_NAME_NUM, "transp-name-num"},
                                                                        {InstrumentNamesFormat::CUSTOM, "custom"}}};

mu::engraving::AsciiStringView mu::engraving::TConv::to_xml(InstrumentNamesFormat v) {
    return find_xml_tag_by_type<InstrumentNamesFormat>(::instr_names_format, v);
}

InstrumentNamesFormat TConv::from_xml(const AsciiStringView &str, InstrumentNamesFormat def) {
    return find_type_by_xml_tag<InstrumentNamesFormat>(::instr_names_format, str, def);
}

struct ActionIconTypeItem {
    ActionIconType type;
    AsciiStringView xml;
    muse::TranslatableString name;
    char16_t icon = 0;
};

//! NOTE The icon code is a UTF-16-like address of the icon in the icon font.
// See also /muse/framework/ui/view/iconcodes.h
// If explicit synchronization with command names and icons is required, then we need to:
// * Add an interface something like "IActionIconDecoration".
// * Use it in ActionIcon(Item) to get titles and icons.
// * Implement and register it outside the engraving module.

const std::vector<ActionIconTypeItem> actionicon_types = {{
    {ActionIconType::UNDEFINED, "undefined", TranslatableString("action", "Undefined"), 0x0000},

    {ActionIconType::ACCIACCATURA, "acciaccatura", TranslatableString("action", "Add grace note: acciaccatura"), 0xF427},
    {ActionIconType::APPOGGIATURA, "appoggiatura", TranslatableString("action", "Add grace note: appoggiatura"), 0xF428},
    {ActionIconType::GRACE4, "grace4", TranslatableString("action", "Add grace note: quarter"), 0xF429},
    {ActionIconType::GRACE16, "grace16", TranslatableString("action", "Add grace note: 16th"), 0xF42A},
    {ActionIconType::GRACE32, "grace32", TranslatableString("action", "Add grace note: 32nd"), 0xF42B},
    {ActionIconType::GRACE8_AFTER, "grace8after", TranslatableString("action", "Add grace note: eighth after"), 0xF42C},
    {ActionIconType::GRACE16_AFTER, "grace16after", TranslatableString("action", "Add grace note: 16th after"), 0xF42D},
    {ActionIconType::GRACE32_AFTER, "grace32after", TranslatableString("action", "Add grace note: 32nd after"), 0xF42E},

    {ActionIconType::BEAM_AUTO, "beam-auto", TranslatableString("action", "Auto beam"), 0xF329},
    {ActionIconType::BEAM_NONE, "beam-none", TranslatableString("action", "No beam"), 0xF33A},
    {ActionIconType::BEAM_BREAK_LEFT, "beam-break-left", TranslatableString("action", "Break beam left"), 0xF33B},
    {ActionIconType::BEAM_JOIN, "beam-join", TranslatableString("action", "Join beams"), 0xF33D},
    {ActionIconType::BEAM_BREAK_INNER_8TH, "beam-break-inner-8th", TranslatableString("action", "Break inner beams (eighth)"), 0xF33E},
    {ActionIconType::BEAM_BREAK_INNER_16TH, "beam-break-inner-16th", TranslatableString("action", "Break inner beams (16th)"), 0xF33F},
    {ActionIconType::BEAM_FEATHERED_ACCELERATE, "beam-feathered-accelerate", TranslatableString("action", "Feathered beam, accelerate"),
     0xEF88},
    {ActionIconType::BEAM_FEATHERED_DECELERATE, "beam-feathered-decelerate", TranslatableString("action", "Feathered beam, decelerate"),
     0xEF89},

    {ActionIconType::PARENTHESES, "add-parentheses", TranslatableString("action", "Add parentheses to element"), 0xEF7C},
    {ActionIconType::BRACKETS, "add-brackets", TranslatableString("action", "Add brackets to element"), 0xEF7D},

    {ActionIconType::VFRAME, "insert-vbox", TranslatableString("action", "Insert vertical frame"), 0xEF6C},
    {ActionIconType::HFRAME, "insert-hbox", TranslatableString("action", "Insert horizontal frame"), 0xEF6D},
    {ActionIconType::TFRAME, "insert-textframe", TranslatableString("action", "Insert text frame"), 0xEF6E},
    {ActionIconType::FFRAME, "insert-fretframe", TranslatableString("action", "Insert fretboard diagram legend"), 0xF491},
    {ActionIconType::STAFF_TYPE_CHANGE, "insert-staff-type-change", TranslatableString("action", "Insert staff type change"), 0xF433},
    {ActionIconType::MEASURE, "insert-measure", TranslatableString("action", "Insert one measure before selection"), 0xF432},

    {ActionIconType::STANDARD_BEND, "standard-bend", TranslatableString("action", "Standard bend"), 0xF45F},
    {ActionIconType::PRE_BEND, "pre-bend", TranslatableString("action", "Pre-bend"), 0xF460},
    {ActionIconType::GRACE_NOTE_BEND, "grace-note-bend", TranslatableString("action", "Grace note bend"), 0xF461},
    {ActionIconType::SLIGHT_BEND, "slight-bend", TranslatableString("action", "Slight bend"), 0xF462},

    {ActionIconType::DIVE, "dive", TranslatableString("action", "Dive"), 0xF4B3},
    {ActionIconType::PRE_DIVE, "pre-dive", TranslatableString("action", "Pre-dive"), 0xF4B4},
    {ActionIconType::DIP, "dip", TranslatableString("action", "Dip"), 0xF4B5},
    {ActionIconType::SCOOP, "scoop", TranslatableString("action", "Scoop"), 0xF4B6},

    {ActionIconType::NOTE_ANCHORED_LINE, "add-noteline", TranslatableString("action", "Add note-anchored line"), 0xF47B},

    {ActionIconType::SYSTEM_LOCK, "toggle-system-lock", TranslatableString("action", "Lock/unlock selected system(s)"), 0xF487},
    {ActionIconType::PAGE_LOCK, "toggle-page-lock", TranslatableString("action", "Lock/unlock selected page(s)"), 0xF4C3},
}};

static const ActionIconTypeItem &find_item_by_type(const std::vector<ActionIconTypeItem> &cont, const ActionIconType &type) {
    const auto it = std::find_if(cont.cbegin(), cont.cend(), [&](const ActionIconTypeItem &item) { return item.type == type; });

    IF_ASSERT_FAILED(it != cont.cend()) {
        static ActionIconTypeItem dummy;
        return dummy;
    }

    return *it;
}

const TranslatableString &TConv::user_name(ActionIconType v) { return find_item_by_type(::actionicon_types, v).name; }

char16_t TConv::icon_code(ActionIconType v) { return find_item_by_type(::actionicon_types, v).icon; }

AsciiStringView TConv::to_xml(ActionIconType v) { return find_item_by_type(::actionicon_types, v).xml; }

ActionIconType TConv::from_xml(const AsciiStringView &tag, ActionIconType def) {
    auto it =
        std::find_if(::actionicon_types.cbegin(), ::actionicon_types.cend(), [tag](const ActionIconTypeItem &i) { return i.xml == tag; });

    if (it == ::actionicon_types.cend()) {
        LOGE() << "not found type for tag: " << tag;
        return def;
    }

    return it->type;
}
