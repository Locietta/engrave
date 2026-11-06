#include "engrave/src/api/value_codec.h"

#include <format>
#include <limits>
#include <string>

#include "engraving/types/bps.h"
#include "engraving/types/fraction.h"
#include "engraving/types/propertyvalue.h"
#include "engraving/types/spatium.h"
#include "engraving/types/types.h"
#include "engraving/types/typesconv.h"

using namespace mu::engraving;

namespace engrave::api {
namespace {

std::string token_of(const muse::AsciiStringView &text) { return text.ascii(); }

std::string token_of(const muse::String &text) { return text.to_std_string(); }

// The name of a property value type whose value is opaque to the API.
std::string_view opaque_name(PType type) {
    switch (type) {
        case PType::DRAW_PATH: return "draw_path";
        case PType::GROUPS: return "groups";
        case PType::PITCH_VALUES: return "pitch_values";
        case PType::DURATION_TYPE_WITH_DOTS: return "duration_type_with_dots";
        default: return "undefined";
    }
}

std::string_view kind_name(ValueKind kind) {
    switch (kind) {
        case ValueKind::BOOL: return "bool";
        case ValueKind::INT: return "int";
        case ValueKind::REAL: return "real";
        case ValueKind::STRING: return "string";
        case ValueKind::POINT: return "point";
        case ValueKind::SIZE: return "size";
        case ValueKind::COLOR: return "color";
        case ValueKind::FRACTION: return "fraction";
        case ValueKind::ENUM: return "enum token";
        case ValueKind::INT_LIST: return "int list";
        case ValueKind::OPAQUE: return "opaque";
    }
    return "?";
}

Result<PropertyValue> fail(ValueKind expected) {
    return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::format("expected a {} value", kind_name(expected))});
}

// A caller's integer has to fit what the engine stores: a narrowing conversion that
// wraps or truncates is a value the caller did not ask for.
bool fits_int(std::int64_t value) { return value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max(); }

Result<PropertyValue> out_of_range(std::string_view what) {
    return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::format("{} is out of range", what)});
}

// A stored value of the type the pid declares, including the engine's own
// int-to-enum and enum-to-int tolerances (PropertyValue::value).
template <typename T>
Result<PropertyValue> enum_value(std::string_view token, T fallback) {
    const T parsed = TConv::from_xml(muse::AsciiStringView(token.data(), token.size()), fallback);
    if (token_of(TConv::to_xml(parsed)) != token) {
        return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::format("unknown token '{}'", token)});
    }
    return PropertyValue(parsed);
}

// Align and OrnamentInterval take their token as a String.
template <typename T>
Result<PropertyValue> enum_value_text(std::string_view token, T fallback) {
    const T parsed = TConv::from_xml(muse::String::from_utf8(token), fallback);
    if (token_of(TConv::to_xml(parsed)) != token) {
        return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::format("unknown token '{}'", token)});
    }
    return PropertyValue(parsed);
}

double real_of(const PropertyValue &value) {
    // A tempo the engine read from a score is a double; one it built is a
    // BeatsPerSecond.
    if (value.type() == PType::TEMPO) return value.value<BeatsPerSecond>().val;
    return value.to_double();
}

std::vector<std::int64_t> int_list(const std::vector<int> &values) { return {values.begin(), values.end()}; }

} // namespace

ValueKind kind_of(PType type) noexcept {
    switch (type) {
        case PType::BOOL: return ValueKind::BOOL;
        case PType::INT: return ValueKind::INT;
        case PType::SIZE_T: return ValueKind::INT;
        case PType::INT_VEC: return ValueKind::INT_LIST;
        case PType::REAL: return ValueKind::REAL;
        case PType::SPATIUM: return ValueKind::REAL;
        case PType::ABSOLUTE: return ValueKind::REAL;
        case PType::TEMPO: return ValueKind::REAL;
        case PType::STRING: return ValueKind::STRING;
        case PType::POINT: return ValueKind::POINT;
        case PType::PAIR_REAL: return ValueKind::POINT;
        case PType::SIZE: return ValueKind::SIZE;
        case PType::SCALE: return ValueKind::SIZE;
        case PType::COLOR: return ValueKind::COLOR;
        case PType::FRACTION: return ValueKind::FRACTION;
        case PType::SYMID: return ValueKind::ENUM;
        case PType::ORNAMENT_STYLE: return ValueKind::ENUM;
        case PType::ORNAMENT_INTERVAL: return ValueKind::ENUM;
        case PType::GLISS_STYLE: return ValueKind::ENUM;
        case PType::GLISS_TYPE: return ValueKind::ENUM;
        case PType::ALIGN: return ValueKind::ENUM;
        case PType::ALIGN_H: return ValueKind::ENUM;
        case PType::PLACEMENT_V: return ValueKind::ENUM;
        case PType::PLACEMENT_H: return ValueKind::ENUM;
        case PType::TEXT_PLACE: return ValueKind::ENUM;
        case PType::DIRECTION_V: return ValueKind::ENUM;
        case PType::DIRECTION_H: return ValueKind::ENUM;
        case PType::ORIENTATION: return ValueKind::ENUM;
        case PType::SHARED_LABEL_ORIENTATION: return ValueKind::ENUM;
        case PType::BEAM_MODE: return ValueKind::ENUM;
        case PType::ACCIDENTAL_ROLE: return ValueKind::ENUM;
        case PType::TIE_PLACEMENT: return ValueKind::ENUM;
        case PType::TIE_DOTS_PLACEMENT: return ValueKind::ENUM;
        case PType::TIMESIG_PLACEMENT: return ValueKind::ENUM;
        case PType::TIMESIG_STYLE: return ValueKind::ENUM;
        case PType::TIMESIG_MARGIN: return ValueKind::ENUM;
        case PType::NOTE_SPELLING_TYPE: return ValueKind::ENUM;
        case PType::CHORD_PRESET_TYPE: return ValueKind::ENUM;
        case PType::PARENTHESES_MODE: return ValueKind::ENUM;
        case PType::PLAY_COUNT_PRESET: return ValueKind::ENUM;
        case PType::CHANGE_METHOD: return ValueKind::ENUM;
        case PType::LAYOUTBREAK_TYPE: return ValueKind::ENUM;
        case PType::VELO_TYPE: return ValueKind::ENUM;
        case PType::BARLINE_TYPE: return ValueKind::ENUM;
        case PType::NOTEHEAD_TYPE: return ValueKind::ENUM;
        case PType::NOTEHEAD_SCHEME: return ValueKind::ENUM;
        case PType::NOTEHEAD_GROUP: return ValueKind::ENUM;
        case PType::CLEF_TYPE: return ValueKind::ENUM;
        case PType::DYNAMIC_TYPE: return ValueKind::ENUM;
        case PType::DYNAMIC_SPEED: return ValueKind::ENUM;
        case PType::LINE_TYPE: return ValueKind::ENUM;
        case PType::HOOK_TYPE: return ValueKind::ENUM;
        case PType::KEY_MODE: return ValueKind::ENUM;
        case PType::TEXT_STYLE: return ValueKind::ENUM;
        case PType::PLAYTECH_TYPE: return ValueKind::ENUM;
        case PType::TEMPOCHANGE_TYPE: return ValueKind::ENUM;
        case PType::LYRICS_DASH_SYSTEM_START_TYPE: return ValueKind::ENUM;
        case PType::PARTIAL_SPANNER_DIRECTION: return ValueKind::ENUM;
        case PType::MARKER_TYPE: return ValueKind::ENUM;
        case PType::ACTION_TYPE: return ValueKind::ENUM;
        case PType::LH_TAPPING_SYMBOL: return ValueKind::ENUM;
        case PType::RH_TAPPING_SYMBOL: return ValueKind::ENUM;
        case PType::VIBRATO_LINE_TYPE: return ValueKind::ENUM;
        case PType::VOICE_ASSIGNMENT: return ValueKind::ENUM;
        case PType::AUTO_ON_OFF: return ValueKind::ENUM;
        case PType::AUTO_CUSTOM_HIDE: return ValueKind::ENUM;
        case PType::MEASURE_NUMBER_PLACEMENT: return ValueKind::ENUM;
        case PType::CAPO_TRANSPOSE_MODE: return ValueKind::ENUM;
        case PType::INSTRUMENT_NAMES_ALIGN: return ValueKind::ENUM;
        case PType::INSTRUMENT_NAMES_FORMAT: return ValueKind::ENUM;
        case PType::INSTRUMENT_LABEL_VISIBILITY: return ValueKind::ENUM;
        // The enum types the engine stores and writes as plain ints.
        case PType::ORNAMENT_SHOW_ACCIDENTAL: return ValueKind::INT;
        case PType::CLEF_TO_BARLINE_POS: return ValueKind::INT;
        case PType::SLUR_STYLE_TYPE: return ValueKind::INT;
        case PType::NOTELINE_PLACEMENT_TYPE: return ValueKind::INT;
        case PType::UNDEFINED: return ValueKind::OPAQUE;
        case PType::DRAW_PATH: return ValueKind::OPAQUE;
        case PType::GROUPS: return ValueKind::OPAQUE;
        case PType::PITCH_VALUES: return ValueKind::OPAQUE;
        case PType::DURATION_TYPE_WITH_DOTS: return ValueKind::OPAQUE;
    }
    return ValueKind::OPAQUE;
}

Unit unit_of(PType type) noexcept {
    switch (type) {
        case PType::SPATIUM: return Unit::SPATIUM;
        case PType::ABSOLUTE: return Unit::ABSOLUTE;
        default: return Unit::NONE;
    }
}

std::span<const std::string_view> enum_tokens(PType type) noexcept {
    switch (type) {
        case PType::ALIGN_H: {
            static constexpr std::string_view tokens[] = {"left", "right", "center", "justify"};
            return tokens;
        }
        case PType::AUTO_CUSTOM_HIDE: {
            static constexpr std::string_view tokens[] = {"auto", "custom", "hide"};
            return tokens;
        }
        case PType::AUTO_ON_OFF: {
            static constexpr std::string_view tokens[] = {"auto", "on", "off"};
            return tokens;
        }
        case PType::BARLINE_TYPE: {
            static constexpr std::string_view tokens[] = {"normal",           "double", "start-repeat", "end-repeat", "dashed",      "end",
                                                          "end-start-repeat", "dotted", "reverse-end",  "heavy",      "double-heavy"};
            return tokens;
        }
        case PType::BEAM_MODE: {
            static constexpr std::string_view tokens[] = {"auto", "begin", "mid", "end", "no", "begin16", "begin32", "invalid"};
            return tokens;
        }
        case PType::CHANGE_METHOD: {
            static constexpr std::string_view tokens[] = {"normal", "ease-in", "ease-out", "ease-in-out", "exponential"};
            return tokens;
        }
        case PType::CHORD_PRESET_TYPE: {
            static constexpr std::string_view tokens[] = {"std", "jazz", "legacy", "custom"};
            return tokens;
        }
        case PType::CLEF_TYPE: {
            static constexpr std::string_view tokens[] = {
                "G",     "G15mb", "G8vb", "G8va",  "G15ma",      "G8vbo",   "G8vbp",   "G1",      "C1",      "C2",
                "C3",    "C4",    "C5",   "C_19C", "C1_F18C",    "C3_F18C", "C4_F18C", "C1_F20C", "C3_F20C", "C4_F20C",
                "F",     "F15mb", "F8vb", "F8va",  "F15ma",      "F3",      "F5",      "F_F18C",  "F_19C",   "PERC",
                "PERC2", "TAB",   "TAB4", "TAB2",  "TAB4_SERIF", "C4_8VB",  "G8vbc"};
            return tokens;
        }
        case PType::DIRECTION_H: {
            static constexpr std::string_view tokens[] = {"auto", "right", "left"};
            return tokens;
        }
        case PType::DIRECTION_V: {
            static constexpr std::string_view tokens[] = {"auto", "up", "down"};
            return tokens;
        }
        case PType::DYNAMIC_SPEED: {
            static constexpr std::string_view tokens[] = {"normal", "slow", "fast"};
            return tokens;
        }
        case PType::DYNAMIC_TYPE: {
            static constexpr std::string_view tokens[] = {
                "other-dynamics", "pppppp", "ppppp",  "pppp", "ppp", "pp", "p",   "mp",  "mf",   "f",    "ff",    "fff",
                "ffff",           "fffff",  "ffffff", "fp",   "pf",  "sf", "sfz", "sff", "sffz", "sfff", "sfffz", "sfp",
                "sfpp",           "rfz",    "rf",     "fz",   "m",   "r",  "s",   "z",   "n"};
            return tokens;
        }
        case PType::GLISS_STYLE: {
            static constexpr std::string_view tokens[] = {"blackkeys", "whitekeys", "diatonic", "portamento", "chromatic"};
            return tokens;
        }
        case PType::GLISS_TYPE: {
            static constexpr std::string_view tokens[] = {"0", "1"};
            return tokens;
        }
        case PType::INSTRUMENT_LABEL_VISIBILITY: {
            static constexpr std::string_view tokens[] = {"auto", "long", "short", "hide"};
            return tokens;
        }
        case PType::INSTRUMENT_NAMES_ALIGN: {
            static constexpr std::string_view tokens[] = {"right-right", "center-right", "center-center", "left-right"};
            return tokens;
        }
        case PType::INSTRUMENT_NAMES_FORMAT: {
            static constexpr std::string_view tokens[] = {"name-in-transp-num", "name-num-in-transp", "transp-name-num", "custom"};
            return tokens;
        }
        case PType::KEY_MODE: {
            static constexpr std::string_view tokens[] = {"unknown", "none",       "major",   "minor",  "dorian", "phrygian",
                                                          "lydian",  "mixolydian", "aeolian", "ionian", "locrian"};
            return tokens;
        }
        case PType::LAYOUTBREAK_TYPE: {
            static constexpr std::string_view tokens[] = {"line", "page", "section", "nobreak"};
            return tokens;
        }
        case PType::LINE_TYPE: {
            static constexpr std::string_view tokens[] = {"solid", "dashed", "dotted"};
            return tokens;
        }
        case PType::LH_TAPPING_SYMBOL: {
            static constexpr std::string_view tokens[] = {"dot", "circledT"};
            return tokens;
        }
        case PType::LYRICS_DASH_SYSTEM_START_TYPE: {
            static constexpr std::string_view tokens[] = {"standard", "underHeader", "underFirstNote"};
            return tokens;
        }
        case PType::MARKER_TYPE: {
            static constexpr std::string_view tokens[] = {"segno", "varsegno", "codab",  "varcoda",   "codetta", "fine",
                                                          "coda",  "codasym",  "dacoda", "dadblcoda", "user"};
            return tokens;
        }
        case PType::MEASURE_NUMBER_PLACEMENT: {
            static constexpr std::string_view tokens[] = {"above-system", "below-system", "on-so-staves", "on-all-staves"};
            return tokens;
        }
        case PType::NOTEHEAD_GROUP: {
            static constexpr std::string_view tokens[] = {"normal",
                                                          "cross",
                                                          "plus",
                                                          "xcircle",
                                                          "withx",
                                                          "triangle-up",
                                                          "triangle-down",
                                                          "slashed1",
                                                          "slashed2",
                                                          "diamond",
                                                          "diamond-old",
                                                          "circled",
                                                          "circled-large",
                                                          "large-arrow",
                                                          "altbrevis",
                                                          "slash",
                                                          "large-diamond",
                                                          "heavy-cross",
                                                          "heavy-cross-hat",
                                                          "sol",
                                                          "la",
                                                          "fa",
                                                          "mi",
                                                          "do",
                                                          "re",
                                                          "ti",
                                                          "do-walker",
                                                          "re-walker",
                                                          "ti-walker",
                                                          "do-funk",
                                                          "re-funk",
                                                          "ti-funk",
                                                          "do-name",
                                                          "di-name",
                                                          "ra-name",
                                                          "re-name",
                                                          "ri-name",
                                                          "me-name",
                                                          "mi-name",
                                                          "fa-name",
                                                          "fi-name",
                                                          "se-name",
                                                          "sol-name",
                                                          "le-name",
                                                          "la-name",
                                                          "li-name",
                                                          "te-name",
                                                          "ti-name",
                                                          "si-name",
                                                          "a-sharp-name",
                                                          "a-name",
                                                          "a-flat-name",
                                                          "b-sharp-name",
                                                          "b-name",
                                                          "b-flat-name",
                                                          "c-sharp-name",
                                                          "c-name",
                                                          "c-flat-name",
                                                          "d-sharp-name",
                                                          "d-name",
                                                          "d-flat-name",
                                                          "e-sharp-name",
                                                          "e-name",
                                                          "e-flat-name",
                                                          "f-sharp-name",
                                                          "f-name",
                                                          "f-flat-name",
                                                          "g-sharp-name",
                                                          "g-name",
                                                          "g-flat-name",
                                                          "h-name",
                                                          "h-sharp-name",
                                                          "swiss-rudiments-flam",
                                                          "swiss-rudiments-double",
                                                          "custom"};
            return tokens;
        }
        case PType::NOTEHEAD_SCHEME: {
            static constexpr std::string_view tokens[] = {"auto",
                                                          "normal",
                                                          "name-pitch",
                                                          "name-pitch-no-acc",
                                                          "name-pitch-german",
                                                          "name-pitch-german-no-acc",
                                                          "solfege-movable",
                                                          "solfege-fixed",
                                                          "shape-4",
                                                          "shape-7-aikin",
                                                          "shape-7-funk",
                                                          "shape-7-walker"};
            return tokens;
        }
        case PType::NOTEHEAD_TYPE: {
            static constexpr std::string_view tokens[] = {"auto", "whole", "half", "quarter", "breve"};
            return tokens;
        }
        case PType::NOTE_SPELLING_TYPE: {
            static constexpr std::string_view tokens[] = {"standard", "german", "germanPure", "solfeggio", "french"};
            return tokens;
        }
        case PType::ORIENTATION: {
            static constexpr std::string_view tokens[] = {"vertical", "horizontal"};
            return tokens;
        }
        case PType::ORNAMENT_STYLE: {
            static constexpr std::string_view tokens[] = {"baroque", "default"};
            return tokens;
        }
        case PType::PARENTHESES_MODE: {
            static constexpr std::string_view tokens[] = {"none", "both", "left", "right"};
            return tokens;
        }
        case PType::PARTIAL_SPANNER_DIRECTION: {
            static constexpr std::string_view tokens[] = {"none", "outgoing", "incoming", "both"};
            return tokens;
        }
        case PType::PLACEMENT_H: {
            static constexpr std::string_view tokens[] = {"left", "center", "right"};
            return tokens;
        }
        case PType::PLACEMENT_V: {
            static constexpr std::string_view tokens[] = {"above", "below"};
            return tokens;
        }
        case PType::PLAYTECH_TYPE: {
            static constexpr std::string_view tokens[] = {"undefined",
                                                          "natural",
                                                          "pizzicato",
                                                          "open",
                                                          "mute",
                                                          "tremolo",
                                                          "detache",
                                                          "martele",
                                                          "col_legno",
                                                          "sul_ponticello",
                                                          "sul_tasto",
                                                          "vibrato",
                                                          "legato",
                                                          "distortion",
                                                          "overdrive",
                                                          "harmonics",
                                                          "jazz_tone",
                                                          "handbells_swing",
                                                          "handbells_swing_up",
                                                          "handbells_swing_down",
                                                          "handbells_echo_1",
                                                          "handbells_echo_2",
                                                          "handbells_damp",
                                                          "handbells_lv",
                                                          "handbells_r"};
            return tokens;
        }
        case PType::PLAY_COUNT_PRESET: {
            static constexpr std::string_view tokens[] = {"xn", "nx", "playntimes", "nrepeats"};
            return tokens;
        }
        case PType::RH_TAPPING_SYMBOL: {
            static constexpr std::string_view tokens[] = {"T", "plus"};
            return tokens;
        }
        case PType::SHARED_LABEL_ORIENTATION: {
            static constexpr std::string_view tokens[] = {"vertical", "horizontal", "voice"};
            return tokens;
        }
        case PType::TEMPOCHANGE_TYPE: {
            static constexpr std::string_view tokens[] = {"undefined",  "accelerando", "allargando",   "calando",
                                                          "lentando",   "morendo",     "precipitando", "rallentando",
                                                          "ritardando", "smorzando",   "sostenuto",    "stringendo"};
            return tokens;
        }
        case PType::TEXT_PLACE: {
            static constexpr std::string_view tokens[] = {"auto", "above", "below", "left"};
            return tokens;
        }
        case PType::TEXT_STYLE: {
            static constexpr std::string_view tokens[] = {"default",
                                                          "title",
                                                          "subtitle",
                                                          "composer",
                                                          "poet",
                                                          "translator",
                                                          "frame",
                                                          "instrument_excerpt",
                                                          "instrument_long",
                                                          "instrument_short",
                                                          "instrument_change",
                                                          "group_bracket",
                                                          "header",
                                                          "footer",
                                                          "copyright",
                                                          "page_number",
                                                          "measure_number",
                                                          "measure_number_alternate",
                                                          "mmrest_range",
                                                          "tempo",
                                                          "tempo change",
                                                          "metronome",
                                                          "repeat_play_count",
                                                          "repeat_left",
                                                          "repeat_right",
                                                          "rehearsal_mark",
                                                          "system",
                                                          "staff",
                                                          "stave_sharing",
                                                          "expression",
                                                          "dynamics",
                                                          "hairpin",
                                                          "lyrics_odd",
                                                          "lyrics_even",
                                                          "harmony_a",
                                                          "harmony_b",
                                                          "harmony_roman",
                                                          "harmony_nashville",
                                                          "tuplet",
                                                          "sticking",
                                                          "fingering",
                                                          "tab_fret_number",
                                                          "guitar_fingering_lh",
                                                          "guitar_fingering_rh",
                                                          "hammer_on_pull_off",
                                                          "string_number",
                                                          "string_tunings",
                                                          "fret_diagram_fingering",
                                                          "fret_diagram_fret_number",
                                                          "harp_pedal_diagram",
                                                          "harp_pedal_text_diagram",
                                                          "articulation",
                                                          "textline",
                                                          "system_textline",
                                                          "noteline",
                                                          "volta",
                                                          "ottava",
                                                          "glissando",
                                                          "pedal",
                                                          "bend",
                                                          "let_ring",
                                                          "whammy_bar",
                                                          "palm_mute",
                                                          "user_1",
                                                          "user_2",
                                                          "user_3",
                                                          "user_4",
                                                          "user_5",
                                                          "user_6",
                                                          "user_7",
                                                          "user_8",
                                                          "user_9",
                                                          "user_10",
                                                          "user_11",
                                                          "user_12"};
            return tokens;
        }
        case PType::TIE_DOTS_PLACEMENT: {
            static constexpr std::string_view tokens[] = {"auto", "before", "after"};
            return tokens;
        }
        case PType::TIE_PLACEMENT: {
            static constexpr std::string_view tokens[] = {"auto", "inside", "outside"};
            return tokens;
        }
        case PType::TIMESIG_MARGIN: {
            static constexpr std::string_view tokens[] = {"hangIntoMargin", "rightAlignToBarline", "createSpace"};
            return tokens;
        }
        case PType::TIMESIG_PLACEMENT: {
            static constexpr std::string_view tokens[] = {"normal", "aboveStaves", "acrossStaves"};
            return tokens;
        }
        case PType::TIMESIG_STYLE: {
            static constexpr std::string_view tokens[] = {"normal", "narrow", "large"};
            return tokens;
        }
        case PType::VELO_TYPE: {
            static constexpr std::string_view tokens[] = {"offset", "user"};
            return tokens;
        }
        case PType::VIBRATO_LINE_TYPE: {
            static constexpr std::string_view tokens[] = {"none", "guitarVibrato", "guitarVibratoWide", "vibratoSawtooth",
                                                          "vibratoSawtoothWide"};
            return tokens;
        }
        case PType::VOICE_ASSIGNMENT: {
            static constexpr std::string_view tokens[] = {"allInInstrument", "allInStaff", "currentVoiceOnly"};
            return tokens;
        }
        case PType::ACTION_TYPE: {
            static constexpr std::string_view tokens[] = {"undefined",
                                                          "acciaccatura",
                                                          "appoggiatura",
                                                          "grace4",
                                                          "grace16",
                                                          "grace32",
                                                          "grace8after",
                                                          "grace16after",
                                                          "grace32after",
                                                          "beam-auto",
                                                          "beam-none",
                                                          "beam-break-left",
                                                          "beam-join",
                                                          "beam-break-inner-8th",
                                                          "beam-break-inner-16th",
                                                          "beam-feathered-accelerate",
                                                          "beam-feathered-decelerate",
                                                          "add-parentheses",
                                                          "add-brackets",
                                                          "insert-vbox",
                                                          "insert-hbox",
                                                          "insert-textframe",
                                                          "insert-fretframe",
                                                          "insert-staff-type-change",
                                                          "insert-measure",
                                                          "standard-bend",
                                                          "pre-bend",
                                                          "grace-note-bend",
                                                          "slight-bend",
                                                          "dive",
                                                          "pre-dive",
                                                          "dip",
                                                          "scoop",
                                                          "add-noteline",
                                                          "toggle-system-lock",
                                                          "toggle-page-lock"};
            return tokens;
        }
        case PType::CAPO_TRANSPOSE_MODE: {
            static constexpr std::string_view tokens[] = {"playback", "standard", "tab"};
            return tokens;
        }

        default: return {};
    }
}

Value to_value(PType type, const PropertyValue &value) {
    switch (type) {
        case PType::UNDEFINED: return {};
        case PType::BOOL: return value.to_bool();
        case PType::INT: return std::int64_t(value.to_int());
        case PType::SIZE_T: return std::int64_t(value.value<size_t>());
        case PType::INT_VEC: return int_list(value.value<std::vector<int>>());
        case PType::REAL: return value.to_double();
        case PType::SPATIUM: return value.value<Spatium>().val();
        case PType::ABSOLUTE: return value.to_double();
        case PType::TEMPO: return real_of(value);
        case PType::STRING: return value.value<String>().to_std_string();
        case PType::POINT: {
            const PointF point = value.value<PointF>();
            return Point{point.x(), point.y()};
        }
        case PType::PAIR_REAL: {
            const PairF pair = value.value<PairF>();
            return Point{pair.first, pair.second};
        }
        case PType::SIZE: {
            const SizeF size = value.value<SizeF>();
            return Size{size.width(), size.height()};
        }
        case PType::SCALE: {
            const ScaleF scale = value.value<ScaleF>();
            return Size{scale.width(), scale.height()};
        }
        case PType::COLOR: {
            const mu::engraving::Color color = value.value<mu::engraving::Color>();
            return Color{std::uint8_t(color.red()), std::uint8_t(color.green()), std::uint8_t(color.blue()), std::uint8_t(color.alpha())};
        }
        case PType::FRACTION: {
            const mu::engraving::Fraction fraction = value.value<mu::engraving::Fraction>();
            return engrave::Fraction{fraction.numerator(), fraction.denominator()};
        }
        case PType::ALIGN_H: return Enum{token_of(TConv::to_xml(value.value<AlignH>()))};
        case PType::AUTO_CUSTOM_HIDE: return Enum{token_of(TConv::to_xml(value.value<AutoCustomHide>()))};
        case PType::AUTO_ON_OFF: return Enum{token_of(TConv::to_xml(value.value<AutoOnOff>()))};
        case PType::BARLINE_TYPE: return Enum{token_of(TConv::to_xml(value.value<BarLineType>()))};
        case PType::BEAM_MODE: return Enum{token_of(TConv::to_xml(value.value<BeamMode>()))};
        case PType::CHANGE_METHOD: return Enum{token_of(TConv::to_xml(value.value<ChangeMethod>()))};
        case PType::CHORD_PRESET_TYPE: return Enum{token_of(TConv::to_xml(value.value<ChordStylePreset>()))};
        case PType::CLEF_TYPE: return Enum{token_of(TConv::to_xml(value.value<ClefType>()))};
        case PType::DIRECTION_H: return Enum{token_of(TConv::to_xml(value.value<DirectionH>()))};
        case PType::DIRECTION_V: return Enum{token_of(TConv::to_xml(value.value<DirectionV>()))};
        case PType::DYNAMIC_SPEED: return Enum{token_of(TConv::to_xml(value.value<DynamicSpeed>()))};
        case PType::DYNAMIC_TYPE: return Enum{token_of(TConv::to_xml(value.value<DynamicType>()))};
        case PType::GLISS_STYLE: return Enum{token_of(TConv::to_xml(value.value<GlissandoStyle>()))};
        case PType::GLISS_TYPE: return Enum{token_of(TConv::to_xml(value.value<GlissandoType>()))};
        case PType::INSTRUMENT_LABEL_VISIBILITY: return Enum{token_of(TConv::to_xml(value.value<InstrumentLabelVisibility>()))};
        case PType::INSTRUMENT_NAMES_ALIGN: return Enum{token_of(TConv::to_xml(value.value<InstrumentNamesAlign>()))};
        case PType::INSTRUMENT_NAMES_FORMAT: return Enum{token_of(TConv::to_xml(value.value<InstrumentNamesFormat>()))};
        case PType::KEY_MODE: return Enum{token_of(TConv::to_xml(value.value<KeyMode>()))};
        case PType::LAYOUTBREAK_TYPE: return Enum{token_of(TConv::to_xml(value.value<LayoutBreakType>()))};
        case PType::LINE_TYPE: return Enum{token_of(TConv::to_xml(value.value<LineType>()))};
        case PType::LH_TAPPING_SYMBOL: return Enum{token_of(TConv::to_xml(value.value<LHTappingSymbol>()))};
        case PType::LYRICS_DASH_SYSTEM_START_TYPE: return Enum{token_of(TConv::to_xml(value.value<LyricsDashSystemStart>()))};
        case PType::MARKER_TYPE: return Enum{token_of(TConv::to_xml(value.value<MarkerType>()))};
        case PType::MEASURE_NUMBER_PLACEMENT: return Enum{token_of(TConv::to_xml(value.value<MeasureNumberPlacement>()))};
        case PType::NOTEHEAD_GROUP: return Enum{token_of(TConv::to_xml(value.value<NoteHeadGroup>()))};
        case PType::NOTEHEAD_SCHEME: return Enum{token_of(TConv::to_xml(value.value<NoteHeadScheme>()))};
        case PType::NOTEHEAD_TYPE: return Enum{token_of(TConv::to_xml(value.value<NoteHeadType>()))};
        case PType::NOTE_SPELLING_TYPE: return Enum{token_of(TConv::to_xml(value.value<NoteSpellingType>()))};
        case PType::ORIENTATION: return Enum{token_of(TConv::to_xml(value.value<Orientation>()))};
        case PType::ORNAMENT_STYLE: return Enum{token_of(TConv::to_xml(value.value<OrnamentStyle>()))};
        case PType::PARENTHESES_MODE: return Enum{token_of(TConv::to_xml(value.value<ParenthesesMode>()))};
        case PType::PARTIAL_SPANNER_DIRECTION: return Enum{token_of(TConv::to_xml(value.value<PartialSpannerDirection>()))};
        case PType::PLACEMENT_H: return Enum{token_of(TConv::to_xml(value.value<PlacementH>()))};
        case PType::PLACEMENT_V: return Enum{token_of(TConv::to_xml(value.value<PlacementV>()))};
        case PType::PLAYTECH_TYPE: return Enum{token_of(TConv::to_xml(value.value<PlayingTechniqueType>()))};
        case PType::PLAY_COUNT_PRESET: return Enum{token_of(TConv::to_xml(value.value<RepeatPlayCountPreset>()))};
        case PType::RH_TAPPING_SYMBOL: return Enum{token_of(TConv::to_xml(value.value<RHTappingSymbol>()))};
        case PType::SHARED_LABEL_ORIENTATION: return Enum{token_of(TConv::to_xml(value.value<SharedLabelOrientation>()))};
        case PType::TEMPOCHANGE_TYPE: return Enum{token_of(TConv::to_xml(value.value<GradualTempoChangeType>()))};
        case PType::TEXT_PLACE: return Enum{token_of(TConv::to_xml(value.value<TextPlace>()))};
        case PType::TEXT_STYLE: return Enum{token_of(TConv::to_xml(value.value<TextStyleType>()))};
        case PType::TIE_DOTS_PLACEMENT: return Enum{token_of(TConv::to_xml(value.value<TieDotsPlacement>()))};
        case PType::TIE_PLACEMENT: return Enum{token_of(TConv::to_xml(value.value<TiePlacement>()))};
        case PType::TIMESIG_MARGIN: return Enum{token_of(TConv::to_xml(value.value<TimeSigVSMargin>()))};
        case PType::TIMESIG_PLACEMENT: return Enum{token_of(TConv::to_xml(value.value<TimeSigPlacement>()))};
        case PType::TIMESIG_STYLE: return Enum{token_of(TConv::to_xml(value.value<TimeSigStyle>()))};
        case PType::VELO_TYPE: return Enum{token_of(TConv::to_xml(value.value<VeloType>()))};
        case PType::VIBRATO_LINE_TYPE: return Enum{token_of(TConv::to_xml(value.value<VibratoType>()))};
        case PType::VOICE_ASSIGNMENT: return Enum{token_of(TConv::to_xml(value.value<VoiceAssignment>()))};
        case PType::ACTION_TYPE: return Enum{token_of(TConv::to_xml(value.value<ActionIconType>()))};
        case PType::CAPO_TRANSPOSE_MODE: return Enum{token_of(TConv::to_xml(value.value<CapoParams::TransposeMode>()))};

        case PType::SYMID: return Enum{token_of(TConv::to_xml(value.value<SymId>()))};
        case PType::ORNAMENT_INTERVAL: return Enum{token_of(TConv::to_xml(value.value<OrnamentInterval>()))};
        case PType::ALIGN: return Enum{token_of(TConv::to_xml(value.value<Align>()))};
        case PType::ACCIDENTAL_ROLE: return Enum{token_of(TConv::to_xml(value.value<AccidentalRole>()))};
        case PType::HOOK_TYPE: return Enum{token_of(TConv::to_xml(value.value<HookType>()))};
        case PType::ORNAMENT_SHOW_ACCIDENTAL: return std::int64_t(value.value<int>());
        case PType::CLEF_TO_BARLINE_POS: return std::int64_t(value.value<int>());
        case PType::SLUR_STYLE_TYPE: return std::int64_t(value.value<int>());
        case PType::NOTELINE_PLACEMENT_TYPE: return std::int64_t(value.value<int>());
        case PType::DRAW_PATH:
        case PType::GROUPS:
        case PType::PITCH_VALUES:
        case PType::DURATION_TYPE_WITH_DOTS: return Opaque{std::string(opaque_name(type))};
    }
    return {};
}

Result<PropertyValue> from_value(PType type, const Value &value) {
    switch (type) {
        case PType::BOOL:
            if (const auto *v = std::get_if<bool>(&value)) return PropertyValue(*v);
            return fail(ValueKind::BOOL);
        case PType::INT:
            if (const auto *v = std::get_if<std::int64_t>(&value)) {
                if (!fits_int(*v)) return out_of_range(std::format("the int {}", *v));
                return PropertyValue(int(*v));
            }
            return fail(ValueKind::INT);
        case PType::SIZE_T:
            if (const auto *v = std::get_if<std::int64_t>(&value)) {
                if (*v < 0 || static_cast<std::uint64_t>(*v) > std::numeric_limits<std::size_t>::max()) {
                    return out_of_range(std::format("the size {}", *v));
                }
                return PropertyValue(size_t(*v));
            }
            return fail(ValueKind::INT);
        case PType::INT_VEC:
            if (const auto *v = std::get_if<std::vector<std::int64_t>>(&value)) {
                std::vector<int> ints;
                ints.reserve(v->size());
                for (const std::int64_t entry : *v) {
                    if (!fits_int(entry)) return out_of_range(std::format("the int {} in the list", entry));
                    ints.push_back(int(entry));
                }
                return PropertyValue(std::move(ints));
            }
            return fail(ValueKind::INT_LIST);
        case PType::REAL:
            if (const auto *v = std::get_if<double>(&value)) return PropertyValue(*v);
            return fail(ValueKind::REAL);
        case PType::SPATIUM:
            if (const auto *v = std::get_if<double>(&value)) return PropertyValue(Spatium(*v));
            return fail(ValueKind::REAL);
        case PType::ABSOLUTE:
            if (const auto *v = std::get_if<double>(&value)) return PropertyValue(*v);
            return fail(ValueKind::REAL);
        case PType::TEMPO:
            if (const auto *v = std::get_if<double>(&value)) return PropertyValue(BeatsPerSecond(*v));
            return fail(ValueKind::REAL);
        case PType::STRING:
            if (const auto *v = std::get_if<std::string>(&value)) return PropertyValue(muse::String::from_utf8(*v));
            return fail(ValueKind::STRING);
        case PType::POINT:
            if (const auto *v = std::get_if<Point>(&value)) return PropertyValue(PointF(v->x, v->y));
            return fail(ValueKind::POINT);
        case PType::PAIR_REAL:
            if (const auto *v = std::get_if<Point>(&value)) return PropertyValue(PairF(v->x, v->y));
            return fail(ValueKind::POINT);
        case PType::SIZE:
            if (const auto *v = std::get_if<Size>(&value)) return PropertyValue(SizeF(v->w, v->h));
            return fail(ValueKind::SIZE);
        case PType::SCALE:
            if (const auto *v = std::get_if<Size>(&value)) return PropertyValue(ScaleF(v->w, v->h));
            return fail(ValueKind::SIZE);
        case PType::COLOR:
            if (const auto *v = std::get_if<engrave::Color>(&value)) return PropertyValue(mu::engraving::Color(v->r, v->g, v->b, v->a));
            return fail(ValueKind::COLOR);
        case PType::FRACTION:
            if (const auto *v = std::get_if<engrave::Fraction>(&value)) {
                if (!fits_int(v->num) || !fits_int(v->den)) return out_of_range("a fraction term");
                return PropertyValue(mu::engraving::Fraction(int(v->num), int(v->den)));
            }
            return fail(ValueKind::FRACTION);
        case PType::ALIGN_H:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<AlignH>(token->token, AlignH::HCENTER);
            return fail(ValueKind::ENUM);
        case PType::AUTO_CUSTOM_HIDE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<AutoCustomHide>(token->token, AutoCustomHide::AUTO);
            return fail(ValueKind::ENUM);
        case PType::AUTO_ON_OFF:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<AutoOnOff>(token->token, AutoOnOff::AUTO);
            return fail(ValueKind::ENUM);
        case PType::BARLINE_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<BarLineType>(token->token, BarLineType::NORMAL);
            return fail(ValueKind::ENUM);
        case PType::BEAM_MODE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<BeamMode>(token->token, BeamMode::AUTO);
            return fail(ValueKind::ENUM);
        case PType::CHANGE_METHOD:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<ChangeMethod>(token->token, ChangeMethod::NORMAL);
            return fail(ValueKind::ENUM);
        case PType::CHORD_PRESET_TYPE:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<ChordStylePreset>(token->token, ChordStylePreset::STANDARD);
            return fail(ValueKind::ENUM);
        case PType::CLEF_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<ClefType>(token->token, ClefType::G);
            return fail(ValueKind::ENUM);
        case PType::DIRECTION_H:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<DirectionH>(token->token, DirectionH::AUTO);
            return fail(ValueKind::ENUM);
        case PType::DIRECTION_V:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<DirectionV>(token->token, DirectionV::AUTO);
            return fail(ValueKind::ENUM);
        case PType::DYNAMIC_SPEED:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<DynamicSpeed>(token->token, DynamicSpeed::NORMAL);
            return fail(ValueKind::ENUM);
        case PType::DYNAMIC_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<DynamicType>(token->token, DynamicType::OTHER);
            return fail(ValueKind::ENUM);
        case PType::GLISS_STYLE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<GlissandoStyle>(token->token, GlissandoStyle::CHROMATIC);
            return fail(ValueKind::ENUM);
        case PType::GLISS_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<GlissandoType>(token->token, GlissandoType::STRAIGHT);
            return fail(ValueKind::ENUM);
        case PType::INSTRUMENT_LABEL_VISIBILITY:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<InstrumentLabelVisibility>(token->token, InstrumentLabelVisibility::AUTO);
            return fail(ValueKind::ENUM);
        case PType::INSTRUMENT_NAMES_ALIGN:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<InstrumentNamesAlign>(token->token, InstrumentNamesAlign::RIGHT_RIGHT);
            return fail(ValueKind::ENUM);
        case PType::INSTRUMENT_NAMES_FORMAT:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<InstrumentNamesFormat>(token->token, InstrumentNamesFormat::NAME_IN_TRANSP_NUM);
            return fail(ValueKind::ENUM);
        case PType::KEY_MODE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<KeyMode>(token->token, KeyMode::NONE);
            return fail(ValueKind::ENUM);
        case PType::LAYOUTBREAK_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<LayoutBreakType>(token->token, LayoutBreakType::NOBREAK);
            return fail(ValueKind::ENUM);
        case PType::LINE_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<LineType>(token->token, LineType::SOLID);
            return fail(ValueKind::ENUM);
        case PType::LH_TAPPING_SYMBOL:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<LHTappingSymbol>(token->token, LHTappingSymbol::DOT);
            return fail(ValueKind::ENUM);
        case PType::LYRICS_DASH_SYSTEM_START_TYPE:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<LyricsDashSystemStart>(token->token, LyricsDashSystemStart::STANDARD);
            return fail(ValueKind::ENUM);
        case PType::MARKER_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<MarkerType>(token->token, MarkerType::USER);
            return fail(ValueKind::ENUM);
        case PType::MEASURE_NUMBER_PLACEMENT:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<MeasureNumberPlacement>(token->token, MeasureNumberPlacement::ABOVE_SYSTEM);
            return fail(ValueKind::ENUM);
        case PType::NOTEHEAD_GROUP:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<NoteHeadGroup>(token->token, NoteHeadGroup::HEAD_NORMAL);
            return fail(ValueKind::ENUM);
        case PType::NOTEHEAD_SCHEME:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<NoteHeadScheme>(token->token, NoteHeadScheme::HEAD_AUTO);
            return fail(ValueKind::ENUM);
        case PType::NOTEHEAD_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<NoteHeadType>(token->token, NoteHeadType::HEAD_AUTO);
            return fail(ValueKind::ENUM);
        case PType::NOTE_SPELLING_TYPE:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<NoteSpellingType>(token->token, NoteSpellingType::STANDARD);
            return fail(ValueKind::ENUM);
        case PType::ORIENTATION:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<Orientation>(token->token, Orientation::VERTICAL);
            return fail(ValueKind::ENUM);
        case PType::ORNAMENT_STYLE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<OrnamentStyle>(token->token, OrnamentStyle::DEFAULT);
            return fail(ValueKind::ENUM);
        case PType::PARENTHESES_MODE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<ParenthesesMode>(token->token, ParenthesesMode::NONE);
            return fail(ValueKind::ENUM);
        case PType::PARTIAL_SPANNER_DIRECTION:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<PartialSpannerDirection>(token->token, PartialSpannerDirection::OUTGOING);
            return fail(ValueKind::ENUM);
        case PType::PLACEMENT_H:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<PlacementH>(token->token, PlacementH::LEFT);
            return fail(ValueKind::ENUM);
        case PType::PLACEMENT_V:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<PlacementV>(token->token, PlacementV::ABOVE);
            return fail(ValueKind::ENUM);
        case PType::PLAYTECH_TYPE:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<PlayingTechniqueType>(token->token, PlayingTechniqueType::NATURAL);
            return fail(ValueKind::ENUM);
        case PType::PLAY_COUNT_PRESET:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<RepeatPlayCountPreset>(token->token, RepeatPlayCountPreset::X_N);
            return fail(ValueKind::ENUM);
        case PType::RH_TAPPING_SYMBOL:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<RHTappingSymbol>(token->token, RHTappingSymbol::T);
            return fail(ValueKind::ENUM);
        case PType::SHARED_LABEL_ORIENTATION:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<SharedLabelOrientation>(token->token, SharedLabelOrientation::HORIZONTAL);
            return fail(ValueKind::ENUM);
        case PType::TEMPOCHANGE_TYPE:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<GradualTempoChangeType>(token->token, GradualTempoChangeType::UNDEFINED);
            return fail(ValueKind::ENUM);
        case PType::TEXT_PLACE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<TextPlace>(token->token, TextPlace::AUTO);
            return fail(ValueKind::ENUM);
        case PType::TEXT_STYLE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<TextStyleType>(token->token, TextStyleType::DEFAULT);
            return fail(ValueKind::ENUM);
        case PType::TIE_DOTS_PLACEMENT:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<TieDotsPlacement>(token->token, TieDotsPlacement::AUTO);
            return fail(ValueKind::ENUM);
        case PType::TIE_PLACEMENT:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<TiePlacement>(token->token, TiePlacement::AUTO);
            return fail(ValueKind::ENUM);
        case PType::TIMESIG_MARGIN:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<TimeSigVSMargin>(token->token, TimeSigVSMargin::HANG_INTO_MARGIN);
            return fail(ValueKind::ENUM);
        case PType::TIMESIG_PLACEMENT:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<TimeSigPlacement>(token->token, TimeSigPlacement::NORMAL);
            return fail(ValueKind::ENUM);
        case PType::TIMESIG_STYLE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<TimeSigStyle>(token->token, TimeSigStyle::NORMAL);
            return fail(ValueKind::ENUM);
        case PType::VELO_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<VeloType>(token->token, VeloType::OFFSET_VAL);
            return fail(ValueKind::ENUM);
        case PType::VIBRATO_LINE_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<VibratoType>(token->token, VibratoType::NONE);
            return fail(ValueKind::ENUM);
        case PType::VOICE_ASSIGNMENT:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<VoiceAssignment>(token->token, VoiceAssignment::ALL_VOICE_IN_INSTRUMENT);
            return fail(ValueKind::ENUM);
        case PType::ACTION_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<ActionIconType>(token->token, ActionIconType::UNDEFINED);
            return fail(ValueKind::ENUM);
        case PType::CAPO_TRANSPOSE_MODE:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value<CapoParams::TransposeMode>(token->token, CapoParams::TransposeMode::PLAYBACK_ONLY);
            return fail(ValueKind::ENUM);
        case PType::SYMID:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<SymId>(token->token, SymId::NO_SYM);
            return fail(ValueKind::ENUM);
        case PType::ORNAMENT_INTERVAL:
            if (const auto *token = std::get_if<Enum>(&value))
                return enum_value_text<OrnamentInterval>(token->token, ::mu::engraving::default_ornament_interval);
            return fail(ValueKind::ENUM);
        case PType::ALIGN:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value_text<Align>(token->token, Align());
            return fail(ValueKind::ENUM);
        case PType::ACCIDENTAL_ROLE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<AccidentalRole>(token->token, AccidentalRole::AUTO);
            return fail(ValueKind::ENUM);
        case PType::HOOK_TYPE:
            if (const auto *token = std::get_if<Enum>(&value)) return enum_value<HookType>(token->token, HookType::NONE);
            return fail(ValueKind::ENUM);
        case PType::ORNAMENT_SHOW_ACCIDENTAL:
            if (const auto *v = std::get_if<std::int64_t>(&value)) return PropertyValue(OrnamentShowAccidental(*v));
            return fail(ValueKind::INT);
        case PType::CLEF_TO_BARLINE_POS:
            if (const auto *v = std::get_if<std::int64_t>(&value)) return PropertyValue(ClefToBarlinePosition(*v));
            return fail(ValueKind::INT);
        case PType::SLUR_STYLE_TYPE:
            if (const auto *v = std::get_if<std::int64_t>(&value)) return PropertyValue(SlurStyleType(*v));
            return fail(ValueKind::INT);
        case PType::NOTELINE_PLACEMENT_TYPE:
            if (const auto *v = std::get_if<std::int64_t>(&value)) return PropertyValue(NoteLineEndPlacement(*v));
            return fail(ValueKind::INT);
        case PType::DRAW_PATH:
        case PType::GROUPS:
        case PType::PITCH_VALUES:
        case PType::DURATION_TYPE_WITH_DOTS:
        case PType::UNDEFINED:
            return std::unexpected(Error{ErrorCode::UNSUPPORTED, std::format("a {} value is read-only", opaque_name(type))});
    }
    return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, "unknown property value type"});
}

} // namespace engrave::api
