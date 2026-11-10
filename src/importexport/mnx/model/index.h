// SPDX-License-Identifier: GPL-3.0-only
// The semantic index of an MNX document: the rules the schema cannot express, and the lookups
// the importer needs (see docs/mnx.md).
#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "mnx_model.h"
#include "rational.h"

namespace mu::iex::mnxio::model {

enum struct Severity : std::uint8_t {
    ERROR,   // the document cannot be imported
    WARNING, // the document imports, but something is missing or ignored
};

// A semantic problem at a JSON pointer into the document.
struct Diagnostic {
    Severity severity = Severity::ERROR;
    std::string pointer;
    std::string message;
};

// Where an event sits in its sequence.
enum struct EventContainer : std::uint8_t {
    SEQUENCE, // directly in the sequence content
    TUPLET,
    GRACE,
    TREMOLO, // a multi-note tremolo
};

// A rhythmic position in the global measure order. A grace index of 0 is the beat itself;
// grace notes before it count back from 1 (the one nearest the beat).
struct Position {
    std::uint32_t measure = 0; // global measure index
    Rational fraction;         // within the measure
    std::optional<std::uint32_t> grace_index;
};

struct EventEntry {
    const Event *event = nullptr;
    std::uint32_t part = 0;
    std::uint32_t measure = 0;
    std::uint32_t sequence = 0;
    EventContainer container = EventContainer::SEQUENCE;
    Rational start;    // metric time from the start of the measure
    Rational duration; // actual duration, with tuplet and tremolo ratios applied
    std::uint32_t grace_index = 0;
    int ottava_shift = 0; // octaves to add to the written pitch for the sounding pitch (negated ottava)
    std::string pointer;
};

template <class T>
struct NoteEntry {
    const T *note = nullptr;
    std::uint32_t event = 0; // index into Index::events
    std::uint32_t index = 0; // index within the event's notes or kit notes
};

struct BeamEntry {
    const Beam *beam = nullptr; // the top-level beam containing the event
    int start_level = 0;        // the deepest-first level of the beam that starts at the event, 0 if none
};

template <class Value>
using IdMap = ankerl::unordered_dense::map<std::string, Value>;

// Lookups into a document. It points into the document's vectors, which must stay unchanged
// (moving the Root is fine; copying or mutating it is not) while the index is in use.
struct Index {
    std::vector<EventEntry> events; // document order
    std::vector<NoteEntry<Note>> notes;
    std::vector<NoteEntry<KitNote>> kit_notes;
    ankerl::unordered_dense::map<const Event *, std::uint32_t> event_of;
    IdMap<std::uint32_t> event_ids;
    IdMap<std::uint32_t> note_ids;
    IdMap<std::uint32_t> kit_note_ids;
    IdMap<std::uint32_t> measure_ids; // global measures
    IdMap<std::uint32_t> part_ids;
    IdMap<std::uint32_t> layout_ids;
    IdMap<BeamEntry> beams; // by event id
    std::vector<std::string> lyric_line_order;
    std::vector<std::optional<std::uint32_t>> time_from; // per global measure: the measure whose time applies
    std::vector<std::optional<std::uint32_t>> key_from;  // per global measure: the measure whose key applies

    const EventEntry &entry(const Event &event) const { return events[event_of.at(&event)]; }

    const EventEntry *find_event(std::string_view id) const;
    const NoteEntry<Note> *find_note(std::string_view id) const;
    const NoteEntry<KitNote> *find_kit_note(std::string_view id) const;
    std::optional<std::uint32_t> find_measure(std::string_view id) const;
    std::optional<std::uint32_t> find_part(std::string_view id) const;
    std::optional<std::uint32_t> find_layout(std::string_view id) const;
    const BeamEntry *find_beam(std::string_view event_id) const;
};

// -1, 0 or 1 as `lhs` is before, at or after `rhs`. With `rhs_includes_trailing_grace`, a right
// position on a beat (grace index 0) also covers the grace notes after it at the same fraction.
int compare(const Position &lhs, const Position &rhs, bool rhs_includes_trailing_grace = false);

struct IndexOptions {
    // Whether ottava spans honour the grace index and voice they target. The importer turns both
    // off because MuseScore cannot target grace notes or voices with ottavas.
    bool ottavas_respect_grace_targets = true;
    bool ottavas_respect_voice_targets = true;
};

struct Indexed {
    Index index;
    std::vector<Diagnostic> warnings;
};

// Checks the document's semantic rules and indexes it. Fails with every diagnostic (errors
// and warnings) when any error is found.
std::expected<Indexed, std::vector<Diagnostic>> build_index(const Root &root, const IndexOptions &options = {});

// The time signature and key in effect at a global measure, if any is set at or before it.
const Time *current_time(const Root &root, const Index &index, std::uint32_t measure);
const Key *current_key(const Root &root, const Index &index, std::uint32_t measure);
// The bar length at a global measure: its current time signature, or 4/4 without one.
Rational measure_length(const Root &root, const Index &index, std::uint32_t measure);

} // namespace mu::iex::mnxio::model
