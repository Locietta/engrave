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
};

struct MeasureMusic {
    MeasureInfo measure;
    bool repeat_start = false;
    bool repeat_end = false;
    int repeat_count = 2;
    std::vector<SignatureInfo> signature_changes;
    std::vector<VoiceEvents> voices;
};

struct TupletMusic {
    ElementId id;
    int actual = 0;
    int normal = 0;
    Fraction base_duration;
};

struct Position {
    ElementId measure;
    Fraction offset;
    int staff = 0;
    int voice = 0;
};

struct SpanMusic {
    ElementId id;
    std::string type;
    Position start;
    Position end;
    std::optional<ElementId> start_element;
    std::optional<ElementId> end_element;
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
    std::vector<int> staves; // Empty selects all staves.
    std::vector<int> voices; // Empty selects all voices.
};

} // namespace engrave
