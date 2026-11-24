#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <engrave/handle.h>
#include <engrave/ops/notes.h>
#include <engrave/query.h>
#include <engrave/value.h>

namespace engrave {

enum class StepName { C, D, E, F, G, A, B };

struct SpelledPitch {
    StepName step = StepName::C;
    int alter = 0;
    int octave = 4;
};

struct PitchedNote {
    int midi = 0;
    SpelledPitch concert;
    SpelledPitch written;
};

struct DrumNote {
    int key = 0;
    std::optional<std::string> kit_name;
};

struct TieLink {
    ElementId tie;
    std::optional<ElementId> other_note;
};

struct NoteMusic {
    ElementId id;
    std::variant<PitchedNote, DrumNote> pitch;
    std::optional<TieLink> incoming;
    std::optional<TieLink> outgoing;
};

struct GraceMusic {
    ElementId chord;
    GraceKind kind;
    Fraction written_duration;
    std::vector<NoteMusic> notes;
};

struct RestMusic {
    bool measure_rest = false;
    // A measure-repeat symbol refers to this many preceding measures.
    std::optional<int> measure_repeat_count;
};

struct ChordMusic {
    std::vector<NoteMusic> notes;
    std::vector<GraceMusic> before;
    std::vector<GraceMusic> after;
};

struct MusicEvent {
    ElementId id;
    Fraction offset; // From the start of the measure.
    Fraction written_duration;
    Fraction elapsed_duration;
    std::vector<ElementId> tuplet_path; // Outermost group first.
    std::variant<RestMusic, ChordMusic> body;
};

struct VoiceEvents {
    int staff = 0;
    int voice = 0;
    std::vector<MusicEvent> events;
};

struct StaffContext {
    int staff = 0;
    std::string part_name;
    std::string instrument_id;
    Fraction time_sig;
    int key = 0;
    Transposition transpose;
    bool uses_drumset = false;
    std::string clef;
};

struct Position {
    ElementId measure;
    Fraction offset;
    int staff = 0;                   // Zero-based; -1 when the endpoint has no track.
    int voice = 0;                   // Zero-based; -1 when the endpoint has no track.
    std::int32_t measure_index = -1; // Zero-based in the selected score; -1 when unknown.
};

enum class MarkScope { SYSTEM, INSTRUMENT, STAFF, VOICE };

struct GraceAttachment {
    bool after = false;
    int ordinal = 0;
    std::optional<ElementId> chord;
};

struct MarkAttachment {
    std::optional<ElementId> event;
    std::optional<GraceAttachment> grace;
};

struct DynamicMark {
    std::string value;
    std::optional<std::string> custom_text;
};

struct FermataMark {
    std::string symbol;
};

struct HarmonyMark {
    std::string text;
};

struct TextMark {
    enum class Kind { EXPRESSION, STAFF, SYSTEM, REHEARSAL };
    Kind kind = Kind::EXPRESSION;
    std::string text;
};

struct ArticulationMark {
    std::string symbol;
    bool ornament = false;
};

struct LyricMark {
    int verse = 0;
    std::string text;
    std::string syllabic;
    std::optional<Fraction> melisma_ticks;
};

struct ArpeggioMark {
    std::string subtype;
};

struct TremoloMark {
    std::string subtype;
    bool two_chord = false;
    std::optional<ElementId> end_event;
    std::optional<Position> end;
};

struct ClefMark {
    std::string clef;
};

struct BarlineMark {
    std::string subtype;
};

using MarkPayload = std::variant<DynamicMark, FermataMark, HarmonyMark, TextMark, ArticulationMark, LyricMark, ArpeggioMark, TremoloMark,
                                 ClefMark, BarlineMark>;

struct MarkMusic {
    std::optional<ElementId> id;
    Fraction offset;
    MarkScope scope = MarkScope::STAFF;
    int staff = -1; // Anchor staff for instrument-wide markings.
    int voice = -1;
    std::optional<MarkAttachment> attachment;
    MarkPayload payload;
};

struct MeasureMusic {
    MeasureInfo measure;
    bool repeat_start = false;
    bool repeat_end = false;
    int repeat_count = 2;
    std::vector<SignatureInfo> signature_changes;
    std::vector<VoiceEvents> voices;
    std::vector<MarkMusic> markings;
};

struct TupletMusic {
    ElementId id;
    int actual = 0;
    int normal = 0;
    Fraction base_duration;
};

struct SpanDetail {
    std::optional<std::string> direction;
    bool niente = false;
    std::optional<std::string> text;
    std::optional<std::string> begin_text;
    std::optional<std::string> continue_text;
    std::optional<std::string> end_text;
    std::optional<std::string> begin_hook;
    std::optional<std::string> end_hook;
};

struct SpanMusic {
    ElementId id;
    std::string type;
    std::optional<std::string> subtype;
    Position start;
    Position end;
    std::optional<ElementId> start_element;
    std::optional<ElementId> end_element;
    std::optional<SpanDetail> detail;
};

struct VoltaMusic {
    SpanMusic span;
    std::vector<int> endings;
    bool closed = true;
};

struct MusicWindow {
    ScoreSel score;
    std::uint64_t revision = 0;
    std::vector<StaffContext> staves;
    std::vector<MeasureMusic> measures;
    std::vector<TupletMusic> tuplets;
    std::vector<SpanMusic> spanners;
    std::vector<VoltaMusic> voltas;
};

struct MusicRange {
    ScoreSel score;
    int first_measure = 0;
    int measure_count = 1;
    std::vector<int> staves;         // Empty selects all staves.
    std::vector<int> voices;         // Empty selects all voices.
    bool clamp_to_score_end = false; // Shorten the count if the starting bar exists but fewer bars remain.
};

// NOLINTNEXTLINE(readability-identifier-naming) -- Preserve the public PitchDomain spelling.
enum class PitchDomain { Concert, Written };

using EntryPitch = std::variant<SpelledPitch, DrumKey>;

struct PhraseGrace {
    GraceKind kind;
    Fraction written_duration;
    std::vector<EntryPitch> notes;
};

struct PhraseNote {
    EntryPitch pitch;
    bool tie_to_next = false;
};

struct PhraseEvent {
    Fraction written_duration;
    std::vector<PhraseNote> notes; // An empty chord enters a rest.
    std::vector<PhraseGrace> before;
    std::vector<PhraseGrace> after;
};

struct TupletGroup {
    int actual = 3;
    int normal = 2;
    std::vector<PhraseEvent> members;
};

struct BarCheck {};

using PhraseItem = std::variant<PhraseEvent, TupletGroup, BarCheck>;

struct PhraseLane {
    Position start;
    PitchDomain domain = PitchDomain::Concert;
    std::vector<PhraseItem> items;
};

struct Phrase {
    ScoreSel score;
    std::vector<PhraseLane> lanes;
};

} // namespace engrave
