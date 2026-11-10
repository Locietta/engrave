// SPDX-License-Identifier: GPL-3.0-only
// Semantic rules ported from mnxdom's SemanticValidate.cpp and EntityMap (MIT, Robert
// Patterson), restated over the generated model; see docs/mnx.md.
#include "index.h"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <utility>

#include "music.h"

namespace mu::iex::mnxio::model {

const EventEntry *Index::find_event(std::string_view id) const {
    const auto it = event_ids.find(std::string(id));
    return it == event_ids.end() ? nullptr : &events[it->second];
}

const NoteEntry<Note> *Index::find_note(std::string_view id) const {
    const auto it = note_ids.find(std::string(id));
    return it == note_ids.end() ? nullptr : &notes[it->second];
}

const NoteEntry<KitNote> *Index::find_kit_note(std::string_view id) const {
    const auto it = kit_note_ids.find(std::string(id));
    return it == kit_note_ids.end() ? nullptr : &kit_notes[it->second];
}

std::optional<std::uint32_t> Index::find_measure(std::string_view id) const {
    const auto it = measure_ids.find(std::string(id));
    return it == measure_ids.end() ? std::nullopt : std::optional(it->second);
}

std::optional<std::uint32_t> Index::find_part(std::string_view id) const {
    const auto it = part_ids.find(std::string(id));
    return it == part_ids.end() ? std::nullopt : std::optional(it->second);
}

std::optional<std::uint32_t> Index::find_layout(std::string_view id) const {
    const auto it = layout_ids.find(std::string(id));
    return it == layout_ids.end() ? std::nullopt : std::optional(it->second);
}

const BeamEntry *Index::find_beam(std::string_view event_id) const {
    const auto it = beams.find(std::string(event_id));
    return it == beams.end() ? nullptr : &it->second;
}

int compare(const Position &lhs, const Position &rhs, bool rhs_includes_trailing_grace) {
    if (lhs.measure != rhs.measure) return lhs.measure < rhs.measure ? -1 : 1;
    if (lhs.fraction != rhs.fraction) return lhs.fraction < rhs.fraction ? -1 : 1;
    const auto &l = lhs.grace_index;
    const auto &r = rhs.grace_index;
    if (rhs_includes_trailing_grace && r && *r == 0 && l && *l > 0) return 0;
    if (!l && !r) return 0;
    if (!l) return -1;
    if (!r) return 1;
    if (*l == *r) return 0;
    return *l < *r ? -1 : 1;
}

const Time *current_time(const Root &root, const Index &index, std::uint32_t measure) {
    const auto from = index.time_from[measure];
    return from ? &*root.global.measures[*from].time : nullptr;
}

const Key *current_key(const Root &root, const Index &index, std::uint32_t measure) {
    const auto from = index.key_from[measure];
    return from ? &*root.global.measures[*from].key : nullptr;
}

Rational measure_length(const Root &root, const Index &index, std::uint32_t measure) {
    const Time *time = current_time(root, index, measure);
    return time ? duration_of(*time) : Rational(1);
}

namespace {

constexpr int max_dots = 16; // keeps duration arithmetic far from overflow
constexpr int max_staves = 64;
// Bounds that keep pitch and transposition arithmetic (music_theory.hpp loops over them) small.
constexpr int max_octave = 20;
constexpr int max_diatonic_delta = 7;
constexpr int max_interval_steps = 70;
constexpr int max_fifths = 30;

// |value| > limit, without std::abs (which overflows for INT_MIN).
constexpr bool beyond(int value, int limit) { return value < -limit || value > limit; }

// A JSON pointer token for an object key (RFC 6901 escaping).
std::string key_token(std::string_view key) {
    std::string out;
    for (const char c : key) {
        if (c == '~')
            out += "~0";
        else if (c == '/')
            out += "~1";
        else
            out += c;
    }
    return out;
}

std::string at(std::string_view base, std::string_view key) { return std::format("{}/{}", base, key); }

std::string at(std::string_view base, std::string_view key, std::size_t i) { return std::format("{}/{}/{}", base, key, i); }

template <class T>
const std::vector<T> &items(const std::optional<std::vector<T>> &list) {
    static const std::vector<T> empty;
    return list ? *list : empty;
}

template <class T>
std::string id_or(const T &object, std::string_view fallback = "<no-id>") {
    return object.id ? std::string(*object.id) : std::string(fallback);
}

struct OttavaSpan {
    int staff = 1;
    std::optional<std::string> voice;
    Position start;
    Position end;
    int value = 0;
    bool ends_at_measure_end = false;
};

struct PendingGrace {
    std::uint32_t event;
};

class Builder {
public:
    Builder(const Root &root, const IndexOptions &options) : root_(root), options_(options) {}

    std::expected<Indexed, std::vector<Diagnostic>> run() {
        index_measures();
        index_lyric_lines();
        for (std::uint32_t p = 0; p < root_.parts.size(); ++p) index_part(p);
        index_layouts();
        if (index_.lyric_line_order.empty()) {
            std::ranges::sort(found_lyric_lines_);
            const auto duplicates = std::ranges::unique(found_lyric_lines_);
            found_lyric_lines_.erase(duplicates.begin(), duplicates.end());
            index_.lyric_line_order = std::move(found_lyric_lines_);
        }
        for (std::uint32_t p = 0; p < root_.parts.size(); ++p) index_ottavas(p);

        validate_global();
        for (std::uint32_t p = 0; p < root_.parts.size(); ++p) validate_part(p);
        validate_events();
        validate_layouts();
        validate_scores();

        const bool failed = std::ranges::any_of(diagnostics_, [](const Diagnostic &d) { return d.severity == Severity::ERROR; });
        if (failed) return std::unexpected(std::move(diagnostics_));
        return Indexed{std::move(index_), std::move(diagnostics_)};
    }

private:
    const Root &root_;
    const IndexOptions &options_;
    Index index_;
    std::vector<Diagnostic> diagnostics_;
    IdMap<std::string> all_ids_; // every id, to the pointer of its first definition
    std::vector<std::string> found_lyric_lines_;
    std::vector<std::uint32_t> part_measure_count_; // part measures that have a global measure

    void error(std::string pointer, std::string message) {
        diagnostics_.push_back({Severity::ERROR, std::move(pointer), std::move(message)});
    }

    void warning(std::string pointer, std::string message) {
        diagnostics_.push_back({Severity::WARNING, std::move(pointer), std::move(message)});
    }

    // Records an id; a second definition is an error. Returns whether it was new.
    bool add_id(const std::optional<Id> &id, const std::string &pointer) {
        if (!id) return false;
        const auto [it, added] = all_ids_.try_emplace(*id, pointer);
        if (!added) error(pointer, std::format("ID \"{}\" already exists at {}", std::string(*id), it->second));
        return added;
    }

    // A JSON object read as key/value pairs; MNX gives each key one meaning, so a repeat is an error.
    template <class T>
    void check_unique_keys(const std::optional<std::vector<std::pair<std::string, T>>> &map, const std::string &pointer) {
        if (!map) return;
        ankerl::unordered_dense::set<std::string_view> keys;
        for (const auto &[key, _] : *map)
            if (!keys.insert(key).second) error(at(pointer, key_token(key)), std::format("Key \"{}\" appears more than once.", key));
    }

    void check_fraction(const Fraction &fraction, const std::string &pointer) {
        if (fraction.denominator <= 0 || fraction.numerator < 0)
            error(pointer, std::format("Fraction [{}, {}] must have a non-negative numerator and a positive denominator.",
                                       fraction.numerator, fraction.denominator));
    }

    // Position fractions are checked here, so a valid position converts to a Rational safely.
    bool check_position(const RhythmicPosition &position, const std::string &pointer) {
        check_fraction(position.fraction, at(pointer, "fraction"));
        if (position.grace_index && *position.grace_index < 0) error(at(pointer, "graceIndex"), "Grace index must not be negative.");
        return position.fraction.denominator > 0 && position.fraction.numerator >= 0;
    }

    bool check_note_value(const NoteValue &value, const std::string &pointer) {
        const int dots = value.dots_or_default();
        if (dots < 0 || dots > max_dots) {
            error(at(pointer, "dots"), std::format("A note value may have 0 to {} dots, not {}.", max_dots, dots));
            return false;
        }
        return true;
    }

    bool check_quantity(const NoteValueQuantity &quantity, const std::string &pointer) {
        bool ok = check_note_value(quantity.duration, at(pointer, "duration"));
        if (quantity.multiple < 1) {
            error(at(pointer, "multiple"), std::format("A note value quantity needs a positive multiple, not {}.", quantity.multiple));
            ok = false;
        }
        return ok;
    }

    std::optional<Position> position_in(std::uint32_t measure, const RhythmicPosition &position, bool respect_grace) {
        if (position.fraction.denominator <= 0) return std::nullopt;
        Position out{measure, Rational(position.fraction), std::nullopt};
        if (respect_grace && position.grace_index) out.grace_index = static_cast<std::uint32_t>(*position.grace_index);
        return out;
    }

    void index_measures() {
        const auto &measures = root_.global.measures;
        index_.time_from.resize(measures.size());
        index_.key_from.resize(measures.size());
        std::optional<std::uint32_t> time;
        std::optional<std::uint32_t> key;
        for (std::uint32_t m = 0; m < measures.size(); ++m) {
            const auto &measure = measures[m];
            const std::string pointer = at("/global", "measures", m);
            if (add_id(measure.id, pointer)) index_.measure_ids.emplace(*measure.id, m);
            if (measure.time) {
                if (measure.time->count < 1)
                    error(at(pointer, "time/count"), std::format("A time signature needs a positive count, not {}.", measure.time->count));
                else
                    time = m;
            }
            if (measure.key) {
                key = m;
                if (beyond(measure.key->fifths, max_fifths))
                    error(at(pointer, "key/fifths"), std::format("A key has at most {} fifths, not {}.", max_fifths, measure.key->fifths));
            }
            if (measure.repeat_end && measure.repeat_end->times && *measure.repeat_end->times < 1)
                error(at(pointer, "repeatEnd/times"), "A repeat is played at least once.");
            index_.time_from[m] = time;
            index_.key_from[m] = key;
            if (measure.number && *measure.number < 0) error(at(pointer, "number"), "A measure number must not be negative.");
            for (std::size_t i = 0; i < items(measure.tempos).size(); ++i) {
                const auto &tempo = (*measure.tempos)[i];
                const std::string tempo_pointer = at(pointer, "tempos", i);
                if (tempo.location) check_position(*tempo.location, at(tempo_pointer, "location"));
                if (tempo.bpm < 1) error(at(tempo_pointer, "bpm"), std::format("A tempo needs a positive bpm, not {}.", tempo.bpm));
                check_note_value(tempo.value, at(tempo_pointer, "value"));
            }
            if (measure.fine) check_position(measure.fine->location, at(pointer, "fine/location"));
            if (measure.jump) check_position(measure.jump->location, at(pointer, "jump/location"));
            if (measure.segno) check_position(measure.segno->location, at(pointer, "segno/location"));
            if (measure.ending &&
                (measure.ending->duration < 1 || m + static_cast<std::size_t>(measure.ending->duration) > measures.size()))
                error(at(pointer, "ending/duration"), std::format("An ending of {} measures does not fit in the remaining {} measures.",
                                                                  measure.ending->duration, measures.size() - m));
        }
    }

    void index_lyric_lines() {
        if (!root_.global.lyrics) return;
        if (const auto &order = root_.global.lyrics->line_order) index_.lyric_line_order = *order;
    }

    // Walks one sequence's content, indexing its events and notes with their timing (mnxdom's
    // walkSequenceContent semantics) and checking the durations of tuplets and tremolos.
    struct Walk {
        std::uint32_t part;
        std::uint32_t measure;
        std::uint32_t sequence;
        int staff;
        std::optional<std::string> voice;
        std::vector<std::uint32_t> pending_grace;
        bool out_of_range = false;
    };

    void flush_grace(Walk &walk) {
        std::uint32_t grace_index = 1;
        for (auto it = walk.pending_grace.rbegin(); it != walk.pending_grace.rend(); ++it) index_.events[*it].grace_index = grace_index++;
        walk.pending_grace.clear();
    }

    // A computed duration or position, if it is bounded (see Rational). Otherwise the sequence is
    // reported once and the value replaced by zero, so later arithmetic stays defined.
    Rational bounded(Walk &walk, const Rational &value, const std::string &pointer) {
        if (value.is_bounded()) return value;
        if (!walk.out_of_range) error(pointer, "Durations or positions in this sequence are too large to represent.");
        walk.out_of_range = true;
        return Rational();
    }

    // The length of an event's note value, if it is valid (an invalid one is reported).
    std::optional<Rational> note_length(const Event &event, const std::string &pointer) {
        if (!check_note_value(event.duration, at(pointer, "duration"))) return std::nullopt;
        return duration_of(event.duration);
    }

    void index_event(Walk &walk, const Event &event, const std::string &pointer, EventContainer container, Rational start,
                     Rational duration) {
        const auto entry_index = static_cast<std::uint32_t>(index_.events.size());
        EventEntry entry;
        entry.event = &event;
        entry.part = walk.part;
        entry.measure = walk.measure;
        entry.sequence = walk.sequence;
        entry.container = container;
        entry.start = start;
        entry.duration = duration;
        entry.pointer = pointer;
        index_.events.push_back(std::move(entry));
        index_.event_of.emplace(&event, entry_index);
        if (add_id(event.id, pointer)) index_.event_ids.emplace(*event.id, entry_index);
        const auto &notes = items(event.notes);
        for (std::uint32_t n = 0; n < notes.size(); ++n) {
            const auto note_index = static_cast<std::uint32_t>(index_.notes.size());
            index_.notes.push_back({&notes[n], entry_index, n});
            if (add_id(notes[n].id, at(pointer, "notes", n))) index_.note_ids.emplace(*notes[n].id, note_index);
        }
        const auto &kit_notes = items(event.kit_notes);
        for (std::uint32_t n = 0; n < kit_notes.size(); ++n) {
            const auto note_index = static_cast<std::uint32_t>(index_.kit_notes.size());
            index_.kit_notes.push_back({&kit_notes[n], entry_index, n});
            if (add_id(kit_notes[n].id, at(pointer, "kitNotes", n))) index_.kit_note_ids.emplace(*kit_notes[n].id, note_index);
        }
        if (index_.lyric_line_order.empty() && event.lyrics) {
            for (const auto &[line, _] : items(event.lyrics->lines)) found_lyric_lines_.push_back(line);
        }
        if (container == EventContainer::GRACE) {
            walk.pending_grace.push_back(entry_index);
        } else {
            flush_grace(walk);
        }
    }

    // Returns the nominal (unscaled) length of the content, for duration checks. Every value it
    // computes is bounded (see bounded()).
    Rational walk_content(Walk &walk, const std::vector<SequenceItem> &content, const std::string &pointer, EventContainer container,
                          Rational &elapsed, Rational ratio) {
        Rational nominal;
        for (std::size_t i = 0; i < content.size(); ++i) {
            const std::string item_pointer = at(pointer, "content", i);
            std::visit(
                [&]<class T>(const T &item) {
                    if constexpr (std::is_same_v<T, Event>) {
                        const auto length = note_length(item, item_pointer);
                        const Rational duration = length ? bounded(walk, *length * ratio, item_pointer) : Rational();
                        index_event(walk, item, item_pointer, container, elapsed, duration);
                        elapsed = bounded(walk, elapsed + duration, item_pointer);
                        if (length) nominal = bounded(walk, nominal + *length, item_pointer);
                    } else if constexpr (std::is_same_v<T, Grace>) {
                        for (std::size_t e = 0; e < item.content.size(); ++e) {
                            const std::string event_pointer = at(item_pointer, "content", e);
                            std::ignore = note_length(item.content[e], event_pointer);
                            index_event(walk, item.content[e], event_pointer, EventContainer::GRACE, elapsed, Rational());
                        }
                    } else if constexpr (std::is_same_v<T, Tuplet>) {
                        const bool inner_ok = check_quantity(item.inner, at(item_pointer, "inner"));
                        const bool outer_ok = check_quantity(item.outer, at(item_pointer, "outer"));
                        if (item.staff) check_staff(walk.part, *item.staff, at(item_pointer, "staff"));
                        if (!inner_ok || !outer_ok) return;
                        const Rational inner_length = bounded(walk, duration_of(item.inner), item_pointer);
                        const Rational outer_length = bounded(walk, duration_of(item.outer), item_pointer);
                        if (inner_length == Rational() || outer_length == Rational()) return;
                        const Rational tuplet = bounded(walk, outer_length / inner_length, item_pointer);
                        const Rational child_ratio = bounded(walk, ratio * tuplet, item_pointer);
                        const Rational inner = walk_content(walk, item.content, item_pointer, EventContainer::TUPLET, elapsed, child_ratio);
                        if (inner > inner_length)
                            error(item_pointer, "Entries in content array add up to more than the expected value.");
                        else if (inner < inner_length)
                            error(item_pointer, "Entries in content array add up to less than the expected value.");
                        nominal = bounded(walk, nominal + outer_length, item_pointer);
                    } else if constexpr (std::is_same_v<T, MultiNoteTremolo>) {
                        if (!check_quantity(item.outer, at(item_pointer, "outer"))) return;
                        const int multiple = item.outer.multiple;
                        if (multiple < 2) error(item_pointer, std::format("Multi-note tremolo has {} events.", multiple));
                        if (item.marks < 1) error(at(item_pointer, "marks"), "A tremolo needs at least one mark.");
                        if (item.individual_duration) check_note_value(*item.individual_duration, at(item_pointer, "individualDuration"));
                        const Rational start = elapsed;
                        const Rational outer = bounded(walk, duration_of(item.outer), item_pointer);
                        const Rational expected = bounded(walk, outer * Rational(multiple), item_pointer);
                        const Rational child_ratio = bounded(walk, ratio / Rational(multiple), item_pointer);
                        Rational inner;
                        for (std::size_t e = 0; e < item.content.size(); ++e) {
                            const auto &event = item.content[e];
                            const std::string event_pointer = at(item_pointer, "content", e);
                            const auto length = note_length(event, event_pointer);
                            const Rational duration = length ? bounded(walk, *length * child_ratio, event_pointer) : Rational();
                            index_event(walk, event, event_pointer, EventContainer::TREMOLO, elapsed, duration);
                            elapsed = bounded(walk, elapsed + duration, event_pointer);
                            if (length) inner = bounded(walk, inner + *length, event_pointer);
                        }
                        if (inner > expected)
                            error(item_pointer, "Entries in content array add up to more than the expected value.");
                        else if (inner < expected)
                            error(item_pointer, "Entries in content array add up to less than the expected value.");
                        // The tremolo always spans its outer value, whatever its events add up to.
                        elapsed = bounded(walk, start + bounded(walk, outer * ratio, item_pointer), item_pointer);
                        nominal = bounded(walk, nominal + outer, item_pointer);
                    } else if constexpr (std::is_same_v<T, Space>) {
                        check_fraction(item.duration, at(item_pointer, "duration"));
                        if (item.duration.denominator <= 0 || item.duration.numerator < 0) return;
                        elapsed = bounded(walk, elapsed + bounded(walk, Rational(item.duration) * ratio, item_pointer), item_pointer);
                        nominal = bounded(walk, nominal + Rational(item.duration), item_pointer);
                    }
                },
                content[i]);
        }
        return nominal;
    }

    bool check_staff(std::uint32_t part, int staff, const std::string &pointer) {
        const int staves = root_.parts[part].staves_or_default();
        if (staff < 1 || staff > staves) {
            error(pointer, std::format("Staff {} does not exist in part {} ({} staves).", staff, id_or(root_.parts[part]), staves));
            return false;
        }
        return true;
    }

    void index_part(std::uint32_t p) {
        const auto &part = root_.parts[p];
        const std::string pointer = at("", "parts", p);
        if (add_id(part.id, pointer)) index_.part_ids.emplace(*part.id, p);
        // Part measures beyond the global measures have no time or position; validate_part
        // reports them, and nothing below them is indexed.
        const auto measures = static_cast<std::uint32_t>(std::min(part.measures.size(), root_.global.measures.size()));
        part_measure_count_.push_back(measures);
        for (std::uint32_t m = 0; m < measures; ++m) {
            const auto &measure = part.measures[m];
            const std::string measure_pointer = at(pointer, "measures", m);
            for (std::uint32_t s = 0; s < measure.sequences.size(); ++s) {
                const auto &sequence = measure.sequences[s];
                const std::string sequence_pointer = at(measure_pointer, "sequences", s);
                Walk walk{p, m, s, sequence.staff_or_default(), sequence.voice, {}, false};
                Rational elapsed;
                if (sequence.full_measure) elapsed = measure_length(root_, index_, m);
                const Rational nominal =
                    walk_content(walk, sequence.content, sequence_pointer, EventContainer::SEQUENCE, elapsed, Rational(1));
                flush_grace(walk);
                if (nominal > measure_length(root_, index_, m))
                    error(sequence_pointer, "Entries in content array add up to more than the expected value.");
            }
            index_beams(measure, measure_pointer);
        }
    }

    void index_beams(const PartMeasure &measure, const std::string &pointer) {
        const auto &beams = items(measure.beams);
        for (std::size_t b = 0; b < beams.size(); ++b) {
            const auto &beam = beams[b];
            for (const auto &event : beam.events) {
                const auto [it, added] = index_.beams.try_emplace(event, BeamEntry{&beam, 0});
                if (!added) error(at(pointer, "beams", b), std::format("ID \"{}\" already exists in another beam.", std::string(event)));
            }
            // validate_beams reports a sub-beam event that is not in its parent beam.
            const auto walk_levels = [&](const auto &self, const Beam &level_beam, int level) -> void {
                if (!level_beam.events.empty()) {
                    if (const auto it = index_.beams.find(level_beam.events.front()); it != index_.beams.end())
                        it->second.start_level = it->second.start_level == 0 ? level : std::min(it->second.start_level, level);
                }
                for (const auto &child : items(level_beam.beams)) self(self, child, level + 1);
            };
            walk_levels(walk_levels, beam, 1);
        }
    }

    void index_layouts() {
        const auto &layouts = items(root_.layouts);
        for (std::uint32_t l = 0; l < layouts.size(); ++l)
            if (add_id(layouts[l].id, at("", "layouts", l))) index_.layout_ids.emplace(*layouts[l].id, l);
    }

    void index_ottavas(std::uint32_t p) {
        const auto &part = root_.parts[p];
        std::vector<OttavaSpan> spans;
        for (std::uint32_t m = 0; m < part_measure_count_[p]; ++m) {
            const auto &ottavas = items(part.measures[m].ottavas);
            for (std::size_t o = 0; o < ottavas.size(); ++o) {
                const auto &ottava = ottavas[o];
                const std::string pointer = at(at("", "parts", p), "measures", m) + std::format("/ottavas/{}", o);
                if (ottava.staff) check_staff(p, *ottava.staff, at(pointer, "staff"));
                const bool start_ok = check_position(ottava.position, at(pointer, "position"));
                const bool end_ok = check_position(ottava.end.position, at(pointer, "end/position"));
                const auto end_measure = index_.find_measure(ottava.end.measure);
                if (!end_measure) {
                    error(at(pointer, "end/measure"), std::format("ID \"{}\" is not a global measure.", std::string(ottava.end.measure)));
                    continue;
                }
                if (!start_ok || !end_ok) continue;
                if (m > *end_measure)
                    error(pointer, "Ottava ends before it begins");
                else if (m == *end_measure && Rational(ottava.position.fraction) > Rational(ottava.end.position.fraction))
                    error(pointer, "Ottava ends before it begins (in the same measure)");
                OttavaSpan span;
                span.staff = ottava.staff_or_default();
                if (options_.ottavas_respect_voice_targets) span.voice = ottava.voice;
                span.start = *position_in(m, ottava.position, options_.ottavas_respect_grace_targets);
                span.end = *position_in(*end_measure, ottava.end.position, options_.ottavas_respect_grace_targets);
                span.value = static_cast<int>(ottava.value);
                span.ends_at_measure_end = span.end.fraction == measure_length(root_, index_, *end_measure);
                spans.push_back(std::move(span));
            }
        }
        if (spans.empty()) return;
        for (auto &entry : index_.events) {
            if (entry.part != p) continue;
            const auto &sequence = part.measures[entry.measure].sequences[entry.sequence];
            const int staff = entry.event->staff.value_or(sequence.staff_or_default());
            Position position{entry.measure, entry.start, std::nullopt};
            if (options_.ottavas_respect_grace_targets) position.grace_index = entry.grace_index;
            int total = 0;
            for (const auto &span : spans) {
                if (staff != span.staff) continue;
                if (span.voice && (!sequence.voice || *sequence.voice != *span.voice)) continue;
                if (compare(position, span.start) < 0) continue;
                if (compare(position, span.end, span.ends_at_measure_end) > 0) continue;
                total += span.value;
            }
            entry.ottava_shift = -total;
        }
    }

    void validate_global() {
        if (const auto &lyrics = root_.global.lyrics) {
            ankerl::unordered_dense::map<std::string, std::string> lines;
            const auto &order = items(lyrics->line_order);
            for (std::size_t i = 0; i < order.size(); ++i) {
                const std::string pointer = at("/global/lyrics", "lineOrder", i);
                if (const auto [it, added] = lines.try_emplace(order[i], pointer); !added)
                    error(pointer, std::format("ID \"{}\" already exists at {}", order[i], it->second));
            }
            const auto &metadata = items(lyrics->line_metadata);
            if (lyrics->line_order) {
                if (lyrics->line_metadata && lines.size() != metadata.size())
                    error("/global/lyrics/lineMetadata", "Size of line metadata does not match size of line order.");
                for (const auto &[line, _] : metadata)
                    if (!lines.contains(line))
                        error(at("/global/lyrics/lineMetadata", key_token(line)), std::format("ID \"{}\" not found in line order", line));
            }
        }
        check_unique_keys(root_.global.sounds, "/global/sounds");
        if (root_.global.lyrics) check_unique_keys(root_.global.lyrics->line_metadata, "/global/lyrics/lineMetadata");
        for (const auto &[sound_id, sound] : items(root_.global.sounds)) {
            if (sound.midi_number && (*sound.midi_number < 0 || *sound.midi_number > 127))
                error(at("/global/sounds", key_token(sound_id)), std::format("Invalid midi number: {}", *sound.midi_number));
        }
    }

    void validate_measure_repeats(std::uint32_t p) {
        // A repeat of N measures covers the N measures starting with the one declaring it.
        const auto &measures = root_.parts[p].measures;
        std::size_t block_start = 0;
        std::size_t block_end = 0;
        for (std::size_t m = 0; m < measures.size(); ++m) {
            const auto &repeat = measures[m].measure_repeat;
            if (!repeat) continue;
            const std::string pointer = at(at("", "parts", p), "measures", m) + "/measureRepeat";
            if (m < block_end) {
                error(pointer, std::format("Measure repeat at measure index {} is already covered by the measure repeat at index {}.", m,
                                           block_start));
                continue;
            }
            if (repeat->number < 1) {
                error(at(pointer, "number"), std::format("A measure repeat needs a positive count, not {}.", repeat->number));
                continue;
            }
            const auto count = static_cast<std::size_t>(repeat->number);
            block_start = m;
            block_end = m + count;
            if (count > m) error(pointer, std::format("Measure repeat of {} measures reaches back before the start of the part.", count));
            if (block_end > measures.size())
                error(pointer, std::format("Measure repeat of {} measures extends past the end of the part.", count));
        }
    }

    void validate_part(std::uint32_t p) {
        const auto &part = root_.parts[p];
        const std::string pointer = at("", "parts", p);
        const std::size_t global_count = root_.global.measures.size();
        if (part.measures.size() != global_count)
            error(pointer, std::format("Part contains a different number of measures ({}) than are defined globally ({}).",
                                       part.measures.size(), global_count));
        const int staves = part.staves_or_default();
        if (staves < 1 || staves > max_staves)
            error(at(pointer, "staves"), std::format("A part has 1 to {} staves, not {}.", max_staves, staves));
        check_unique_keys(part.kit, at(pointer, "kit"));
        for (const auto &[component_id, component] : items(part.kit)) {
            if (!component.sound) continue;
            const auto &sounds = items(root_.global.sounds);
            const bool defined = std::ranges::any_of(sounds, [&](const auto &entry) { return entry.first == *component.sound; });
            if (!defined)
                error(at(at(pointer, "kit"), key_token(component_id)),
                      std::format("Sound ID {} is not defined in global.sounds.", std::string(*component.sound)));
        }
        if (const auto &transposition = part.transposition) {
            const auto &interval = transposition->interval;
            if (beyond(interval.staff_distance, max_interval_steps) || beyond(interval.half_steps, 2 * max_interval_steps))
                error(at(pointer, "transposition/interval"), "The transposition interval is too large.");
            if (transposition->key_fifths_flip_at && beyond(*transposition->key_fifths_flip_at, max_fifths))
                error(at(pointer, "transposition/keyFifthsFlipAt"), std::format("keyFifthsFlipAt is at most {} fifths.", max_fifths));
        }
        if (!part.measures.empty() && !part.kit && staves >= 1 && staves <= max_staves) {
            std::vector<bool> has_clef(static_cast<std::size_t>(staves) + 1, false);
            for (const auto &clef : items(part.measures[0].clefs)) {
                const int staff = clef.staff_or_default();
                const bool at_start = !clef.position || (clef.position->fraction.numerator == 0 && clef.position->fraction.denominator > 0);
                if (staff >= 1 && staff <= staves && at_start) has_clef[static_cast<std::size_t>(staff)] = true;
            }
            // The importer gives such a staff a default clef, so this does not block the import.
            std::string missing;
            for (int staff = 1; staff <= staves; ++staff)
                if (!has_clef[static_cast<std::size_t>(staff)]) missing += std::format("{}{}", missing.empty() ? "" : ", ", staff);
            if (!missing.empty())
                warning(at(pointer, "measures", 0),
                        std::format("Missing clef at the beginning of staff {} in part {} (first measure).", missing, id_or(part)));
        }
        validate_measure_repeats(p);
        for (std::uint32_t m = 0; m < part_measure_count_[p]; ++m) {
            const auto &measure = part.measures[m];
            const std::string measure_pointer = at(pointer, "measures", m);
            const auto &clefs = items(measure.clefs);
            for (std::size_t c = 0; c < clefs.size(); ++c) {
                const std::string clef_pointer = at(measure_pointer, "clefs", c);
                check_staff(p, clefs[c].staff_or_default(), clef_pointer);
                if (clefs[c].position) check_position(*clefs[c].position, at(clef_pointer, "position"));
            }
            ankerl::unordered_dense::map<std::string, std::string> voices;
            for (std::size_t s = 0; s < measure.sequences.size(); ++s) {
                const auto &sequence = measure.sequences[s];
                const std::string sequence_pointer = at(measure_pointer, "sequences", s);
                check_staff(p, sequence.staff_or_default(), at(sequence_pointer, "staff"));
                if (sequence.full_measure && sequence.full_measure->visual_duration)
                    check_note_value(*sequence.full_measure->visual_duration, at(sequence_pointer, "fullMeasure/visualDuration"));
                if (sequence.full_measure && !sequence.content.empty())
                    error(sequence_pointer,
                          std::format("Sequence \"{}\" has full measure rest but content is not empty.", id_or(sequence)));
                if (sequence.voice) {
                    if (sequence.voice->empty())
                        error(at(sequence_pointer, "voice"), "Sequence has an empty voice identifier.");
                    else if (const auto [it, added] = voices.try_emplace(*sequence.voice, sequence_pointer); !added)
                        error(at(sequence_pointer, "voice"),
                              std::format("Voice \"{}\" already exists at {}.", *sequence.voice, it->second));
                }
            }
            validate_beams(items(measure.beams), at(measure_pointer, "beams"), p, m, 1, nullptr);
            validate_dynamics(p, items(measure.dynamics), at(measure_pointer, "dynamics"));
            const auto &arpeggios = items(measure.arpeggios);
            for (std::size_t a = 0; a < arpeggios.size(); ++a)
                validate_arpeggio(p, m, arpeggios[a].position, arpeggios[a].span, arpeggios[a].direction_or_default(),
                                  at(measure_pointer, "arpeggios", a), "Arpeggio");
            const auto &non_arpeggios = items(measure.non_arpeggios);
            for (std::size_t a = 0; a < non_arpeggios.size(); ++a)
                validate_arpeggio(p, m, non_arpeggios[a].position, non_arpeggios[a].span, std::nullopt,
                                  at(measure_pointer, "nonArpeggios", a), "Non-arpeggio");
        }
    }

    void validate_ties(const std::vector<Tie> &ties, const std::string &pointer, std::uint32_t part, const Note *note,
                       const KitNote *kit_note) {
        for (std::size_t t = 0; t < ties.size(); ++t) {
            const auto &tie = ties[t];
            const std::string tie_pointer = at(pointer, "ties", t);
            if (!tie.target) {
                if (!tie.lv_or_default())
                    error(tie_pointer, "Tie has neither a target nor is it an lv tie.");
                else if (tie.target_type)
                    error(tie_pointer, "Tie is an lv tie but also has targetType.");
                continue;
            }
            if (tie.lv_or_default()) error(tie_pointer, "Tie has both a target and is an lv tie.");
            if (note) {
                const auto *target = index_.find_note(*tie.target);
                if (!target) {
                    error(at(tie_pointer, "target"), std::format("Tie points to non-existent target {}", std::string(*tie.target)));
                    continue;
                }
                if (index_.events[target->event].part != part) error(tie_pointer, "Tie points to a note in a different part.");
                if (!same_pitch(note->pitch, target->note->pitch)) error(tie_pointer, "Tie points to a note with a different pitch.");
            } else {
                const auto *target = index_.find_kit_note(*tie.target);
                if (!target) {
                    error(at(tie_pointer, "target"), std::format("Tie points to non-existent target {}", std::string(*tie.target)));
                    continue;
                }
                if (index_.events[target->event].part != part) error(tie_pointer, "Tie points to a note in a different part.");
                if (target->note->kit_component != kit_note->kit_component) error(tie_pointer, "Tie points to a different kit component.");
            }
        }
    }

    void validate_events() {
        ankerl::unordered_dense::set<std::string> lyric_lines;
        if (root_.global.lyrics) {
            for (const auto &line : items(root_.global.lyrics->line_order)) lyric_lines.insert(line);
            if (!root_.global.lyrics->line_order)
                for (const auto &[line, _] : items(root_.global.lyrics->line_metadata)) lyric_lines.insert(line);
        }
        // Lyric line ids are only checked when the global lyrics declare them.
        const bool check_lyric_lines = !lyric_lines.empty();
        for (const auto &entry : index_.events) {
            const Event &event = *entry.event;
            const auto &part = root_.parts[entry.part];
            const std::string &pointer = entry.pointer;
            const bool has_notes = !items(event.notes).empty();
            const bool has_kit_notes = !items(event.kit_notes).empty();
            if (event.rest) {
                if (has_notes) error(pointer, std::format("Event \"{}\" is a rest but also has notes.", id_or(event)));
            } else if (!has_notes && !has_kit_notes) {
                error(pointer, std::format("Event \"{}\" is neither a rest nor has notes.", id_or(event)));
            }
            if (event.staff) check_staff(entry.part, *event.staff, at(pointer, "staff"));
            const auto &notes = items(event.notes);
            for (std::size_t n = 0; n < notes.size(); ++n) {
                const auto &note = notes[n];
                const std::string note_pointer = at(pointer, "notes", n);
                const int alter = note.pitch.alter_or_default();
                if (beyond(note.pitch.octave, max_octave))
                    error(at(note_pointer, "pitch/octave"), std::format("Octave {} is out of range.", note.pitch.octave));
                if (note.written && beyond(note.written->diatonic_delta_or_default(), max_diatonic_delta))
                    error(at(note_pointer, "written/diatonicDelta"), std::format("diatonicDelta is at most {}.", max_diatonic_delta));
                if (alter < -3 || alter > 3)
                    error(at(note_pointer, "pitch/alter"),
                          std::format("Note \"{}\" has alteration value {}. MNX files are limited to +/-3.", id_or(note), alter));
                if (note.staff) check_staff(entry.part, *note.staff, at(note_pointer, "staff"));
                validate_ties(items(note.ties), note_pointer, entry.part, &note, nullptr);
            }
            const auto &kit_notes = items(event.kit_notes);
            for (std::size_t n = 0; n < kit_notes.size(); ++n) {
                const auto &kit_note = kit_notes[n];
                const std::string note_pointer = at(pointer, "kitNotes", n);
                const auto &kit = items(part.kit);
                if (!std::ranges::any_of(kit, [&](const auto &component) { return component.first == kit_note.kit_component; }))
                    error(note_pointer, std::format("Kit note has kit element {} that is not defined in the part's kit.",
                                                    std::string(kit_note.kit_component)));
                if (kit_note.staff) check_staff(entry.part, *kit_note.staff, at(note_pointer, "staff"));
                validate_ties(items(kit_note.ties), note_pointer, entry.part, nullptr, &kit_note);
            }
            if (event.lyrics) check_unique_keys(event.lyrics->lines, at(pointer, "lyrics/lines"));
            if (check_lyric_lines && event.lyrics) {
                for (const auto &[line, _] : items(event.lyrics->lines))
                    if (!lyric_lines.contains(line))
                        error(at(at(pointer, "lyrics/lines"), key_token(line)),
                              std::format("ID \"{}\" not found in the lyric lines", line));
            }
            const auto &slurs = items(event.slurs);
            for (std::size_t s = 0; s < slurs.size(); ++s) {
                const auto &slur = slurs[s];
                const std::string slur_pointer = at(pointer, "slurs", s);
                const auto *target = index_.find_event(slur.target);
                if (!target)
                    error(at(slur_pointer, "target"), std::format("Slur target \"{}\" is not an event.", std::string(slur.target)));
                if (slur.end_note) {
                    const auto *end = index_.find_note(*slur.end_note);
                    if (!target || !end || index_.events[end->event].event != target->event)
                        error(slur_pointer,
                              std::format("Slur contains end note \"{}\" that does not exist in target.", std::string(*slur.end_note)));
                }
                if (slur.start_note) {
                    const auto *start = index_.find_note(*slur.start_note);
                    if (!start || index_.events[start->event].event != &event)
                        error(slur_pointer, std::format("Slur contains start note \"{}\" that does not exist in the containing event.",
                                                        std::string(*slur.start_note)));
                }
            }
        }
    }

    // `parent` is the beam a level of sub-beams belongs to; its events must contain theirs.
    void validate_beams(const std::vector<Beam> &beams, const std::string &pointer, std::uint32_t part, std::uint32_t measure, int depth,
                        const Beam *parent) {
        ankerl::unordered_dense::set<std::string> seen;
        for (std::size_t b = 0; b < beams.size(); ++b) {
            const auto &beam = beams[b];
            const std::string beam_pointer = std::format("{}/{}", pointer, b);
            if (beam.events.empty()) {
                error(beam_pointer, "Beam contains no events.");
                continue;
            }
            std::uint32_t current_measure = measure;
            bool first = depth == 1;
            std::optional<bool> grace_beam;
            std::optional<std::string> voice;
            Rational current_time;
            for (const auto &id : beam.events) {
                if (!seen.insert(id).second) {
                    error(beam_pointer, std::format("Event \"{}\" is duplicated in beam at depth {}.", std::string(id), depth));
                    continue;
                }
                const auto *entry = index_.find_event(id);
                if (!entry) {
                    error(beam_pointer, std::format("Beam event \"{}\" is not an event.", std::string(id)));
                    continue;
                }
                if (entry->part != part) error(beam_pointer, std::format("Beam event \"{}\" is in another part.", std::string(id)));
                if (parent && !std::ranges::contains(parent->events, id))
                    error(beam_pointer, std::format("Sub-beam event \"{}\" is not in its parent beam.", std::string(id)));
                if (first && entry->measure != current_measure)
                    error(beam_pointer, "First event in beam is not in the same measure as the beam.");
                else if (entry->measure < current_measure)
                    error(beam_pointer, "Beam measures are out of sequence");
                else if (entry->measure > current_measure)
                    current_time = entry->start;
                first = false;
                current_measure = entry->measure;
                if (entry->start < current_time) error(beam_pointer, "Beam events are out of sequence.");
                current_time = entry->start;
                if (entry->container == EventContainer::TREMOLO) {
                    error(beam_pointer,
                          std::format("Beam containing event \"{}\" is actually a multi-note tremolo and should not be a beam.",
                                      std::string(id)));
                    continue;
                }
                const bool is_grace = entry->container == EventContainer::GRACE;
                if (grace_beam && *grace_beam != is_grace)
                    error(beam_pointer, std::format("Event \"{}\" attempts to beam a grace note to a non grace note.", std::string(id)));
                grace_beam = is_grace;
                const auto &sequence = root_.parts[entry->part].measures[entry->measure].sequences[entry->sequence];
                const std::string event_voice = sequence.voice.value_or("");
                if (voice && *voice != event_voice)
                    error(beam_pointer,
                          std::format("Event \"{}\" attempts to beam events from different voices together.", std::string(id)));
                voice = event_voice;
                if (depth > flag_count(entry->event->duration.base))
                    error(beam_pointer, std::format("Event \"{}\" cannot have {} beams", std::string(id), depth));
            }
            if (beam.direction && beam.events.size() != 1)
                error(beam_pointer, "Beam specifies a hook direction but contains more than one event.");
            validate_beams(items(beam.beams), at(beam_pointer, "beams"), part, measure, depth + 1, &beam);
        }
    }

    void validate_dynamics(std::uint32_t part, const std::vector<DynamicGroup> &dynamics, const std::string &pointer) {
        for (std::size_t d = 0; d < dynamics.size(); ++d) {
            const auto &dynamic = dynamics[d];
            const std::string dynamic_pointer = std::format("{}/{}", pointer, d);
            const auto require = [&](bool present, std::string_view type, std::string_view field) {
                if (!present) error(dynamic_pointer, std::format("Dynamic of type \"{}\" requires field \"{}\".", type, field));
            };
            const bool has_text = dynamic.value || !dynamic.prefix.value_or("").empty() || !dynamic.suffix.value_or("").empty() ||
                                  !items(dynamic.glyphs).empty();
            switch (dynamic.type) {
                case DynamicGroupType::IMMEDIATE: require(dynamic.value.has_value(), "immediate", "value"); break;
                case DynamicGroupType::GRADUAL:
                    require(dynamic.end.has_value(), "gradual", "end");
                    require(dynamic.wedge_type.has_value(), "gradual", "wedgeType");
                    break;
                case DynamicGroupType::RELATIVE: require(dynamic.relative_value.has_value(), "relative", "relativeValue"); break;
                case DynamicGroupType::ACCENT: require(dynamic.value.has_value(), "accent", "value"); break;
            }
            const bool accent_text = dynamic.type == DynamicGroupType::ACCENT &&
                                     (dynamic.accent_prefix_or_default() != DynamicPrefix::NONE ||
                                      dynamic.accent_suffix_or_default() != DynamicSuffix::NONE || dynamic.residual_value);
            if (!has_text && !accent_text && dynamic.type != DynamicGroupType::GRADUAL)
                error(dynamic_pointer, "Dynamic has none of value, attackValue, prefix, or suffix.");
            check_position(dynamic.position, at(dynamic_pointer, "position"));
            if (dynamic.end) {
                if (!index_.find_measure(dynamic.end->measure))
                    error(at(dynamic_pointer, "end/measure"),
                          std::format("ID \"{}\" is not a global measure.", std::string(dynamic.end->measure)));
                check_position(dynamic.end->position, at(dynamic_pointer, "end/position"));
            }
            if (dynamic.staff) check_staff(part, *dynamic.staff, at(dynamic_pointer, "staff"));
            if (dynamic.staff_end) check_staff(part, *dynamic.staff_end, at(dynamic_pointer, "staffEnd"));
        }
    }

    void validate_arpeggio(std::uint32_t part, std::uint32_t measure, const RhythmicPosition &position, const IdPair &span,
                           std::optional<UpDownAuto> direction, const std::string &pointer, std::string_view name) {
        check_position(position, at(pointer, "position"));
        const auto *start = index_.find_note(span.start);
        const auto *end = index_.find_note(span.end);
        if (!start) error(at(pointer, "span/start"), std::format("ID \"{}\" is not a note.", std::string(span.start)));
        if (!end) error(at(pointer, "span/end"), std::format("ID \"{}\" is not a note.", std::string(span.end)));
        if (!start || !end) return;
        const auto &start_event = index_.events[start->event];
        const auto &end_event = index_.events[end->event];
        if (start_event.part != end_event.part) error(pointer, std::format("{} span notes must be in the same part.", name));
        if (start_event.measure != end_event.measure) error(pointer, std::format("{} span notes must be in the same bar.", name));
        if (start_event.part != part || start_event.measure != measure || end_event.measure != measure)
            error(pointer, std::format("{} span notes must be in the same bar as the {}.", name, name));
        const bool same_sequence =
            start_event.sequence == end_event.sequence && start_event.measure == end_event.measure && start_event.part == end_event.part;
        const bool same_position =
            start_event.start == end_event.start && (!same_sequence || start_event.grace_index == end_event.grace_index);
        if (!same_position) {
            error(pointer, std::format("{} span notes must be in the same rhythmic position.", name));
            return;
        }
        if (position.fraction.denominator > 0) {
            const bool matches =
                Rational(position.fraction) == start_event.start &&
                (!same_sequence || !position.grace_index || static_cast<std::uint32_t>(*position.grace_index) == start_event.grace_index);
            if (!matches) error(pointer, std::format("{} position must match the span notes' rhythmic position.", name));
        }
        const int start_pitch = semitones(start->note->pitch);
        const int end_pitch = semitones(end->note->pitch);
        if (!direction) {
            if (start_pitch >= end_pitch) error(pointer, "Non-arpeggio span start note must be lower in sounded pitch than the end note.");
        } else if (*direction == UpDownAuto::UP && start_pitch > end_pitch) {
            error(pointer, "Arpeggio with direction \"up\" must start on a lower sounded pitch than it ends on.");
        } else if (*direction == UpDownAuto::DOWN && start_pitch < end_pitch) {
            error(pointer, "Arpeggio with direction \"down\" must start on a higher sounded pitch than it ends on.");
        }
    }

    void validate_layout_content(const std::vector<SystemLayoutItem> &content, const std::string &pointer, std::string_view layout_id) {
        for (std::size_t i = 0; i < content.size(); ++i) {
            const std::string item_pointer = at(pointer, "content", i);
            if (const auto *group = std::get_if<StaffGroup>(&content[i])) {
                if (group->content.empty()) error(item_pointer, "Layout group contains no content.");
                validate_layout_content(group->content, item_pointer, layout_id);
                continue;
            }
            const auto &staff = std::get<Staff>(content[i]);
            if (staff.sources.empty()) error(at(item_pointer, "sources"), "Layout staff has no sources.");

            // Each (part, staff) appears once, or every occurrence names a distinct voice.
            struct KeyState {
                std::size_t count = 0;
                bool missing_voice = false;
                std::vector<std::string> voices;
            };

            std::vector<std::pair<std::pair<std::string, int>, KeyState>> keys;
            bool voices_ok = true;
            for (std::size_t s = 0; s < staff.sources.size(); ++s) {
                const auto &source = staff.sources[s];
                const std::string source_pointer = at(item_pointer, "sources", s);
                const auto part = index_.find_part(source.part);
                if (!part) {
                    error(at(source_pointer, "part"), std::format("ID \"{}\" is not a part.", std::string(source.part)));
                } else {
                    const int staves = root_.parts[*part].staves_or_default();
                    const int staff_number = source.staff_or_default();
                    if (staff_number < 1 || staff_number > staves)
                        error(source_pointer, std::format("Layout \"{}\" has invalid staff number ({}) for part {}", layout_id,
                                                          staff_number, std::string(source.part)));
                }
                const std::pair key{std::string(source.part), source.staff_or_default()};
                auto it = std::ranges::find(keys, key, [](const auto &entry) { return entry.first; });
                if (it == keys.end()) {
                    keys.push_back({key, {}});
                    it = keys.end() - 1;
                }
                auto &state = it->second;
                ++state.count;
                if (!source.voice) {
                    state.missing_voice = true;
                } else if (source.voice->empty() || std::ranges::contains(state.voices, *source.voice)) {
                    voices_ok = false;
                } else {
                    state.voices.push_back(*source.voice);
                }
            }
            for (const auto &[_, state] : keys)
                if (state.count > 1 && (state.missing_voice || state.voices.size() != state.count)) voices_ok = false;
            if (!voices_ok)
                error(item_pointer, std::format("Layout staff \"{}\" has one or more part voices specified multiple times.", id_or(staff)));
        }
    }

    void validate_layouts() {
        const auto &layouts = items(root_.layouts);
        for (std::size_t l = 0; l < layouts.size(); ++l)
            validate_layout_content(layouts[l].content, at("", "layouts", l), layouts[l].id ? std::string_view(*layouts[l].id) : "<no-id>");
    }

    void require_layout(const Id &id, const std::string &pointer) {
        if (!index_.find_layout(id)) error(pointer, std::format("ID \"{}\" is not a layout.", std::string(id)));
    }

    void validate_scores() {
        const auto &scores = items(root_.scores);
        for (std::size_t s = 0; s < scores.size(); ++s) {
            const auto &score = scores[s];
            const std::string pointer = at("", "scores", s);
            if (score.layout) require_layout(*score.layout, at(pointer, "layout"));
            const auto &rests = items(score.multimeasure_rests);
            for (std::size_t r = 0; r < rests.size(); ++r) {
                const std::string rest_pointer = at(pointer, "multimeasureRests", r);
                const auto start = index_.find_measure(rests[r].start);
                if (!start) {
                    error(at(rest_pointer, "start"), std::format("ID \"{}\" is not a global measure.", std::string(rests[r].start)));
                } else if (rests[r].duration < 1 || *start + static_cast<std::size_t>(rests[r].duration) > root_.global.measures.size()) {
                    error(rest_pointer, std::format("Multimeasure rest at measure \"{}\" in score \"{}\" spans non-existent measures",
                                                    std::string(rests[r].start), score.name));
                }
            }
            if (!score.pages) continue;
            std::optional<std::uint32_t> last_system;
            bool first_system = true;
            bool has_system = false;
            for (std::size_t g = 0; g < score.pages->size(); ++g) {
                const auto &page = (*score.pages)[g];
                const std::string page_pointer = at(pointer, "pages", g);
                if (page.layout) require_layout(*page.layout, at(page_pointer, "layout"));
                for (std::size_t y = 0; y < page.systems.size(); ++y) {
                    const auto &system = page.systems[y];
                    const std::string system_pointer = at(page_pointer, "systems", y);
                    has_system = true;
                    if (system.layout) require_layout(*system.layout, at(system_pointer, "layout"));
                    const auto measure = index_.find_measure(system.measure);
                    if (!measure) {
                        error(at(system_pointer, "measure"),
                              std::format("Score \"{}\" references missing measure \"{}\"", score.name, std::string(system.measure)));
                        continue;
                    }
                    if (first_system && *measure > 0)
                        error(system_pointer, std::format("The first system in score \"{}\" starts after the first measure", score.name));
                    first_system = false;
                    if (last_system && *measure <= *last_system)
                        error(system_pointer, std::format("Score \"{}\" contains system that {} previous system", score.name,
                                                          *measure < *last_system ? "starts before" : "starts on the same measure as"));
                    last_system = measure;
                    const auto &changes = items(system.layout_changes);
                    for (std::size_t c = 0; c < changes.size(); ++c) {
                        const auto &change = changes[c];
                        const std::string change_pointer = at(system_pointer, "layoutChanges", c);
                        require_layout(change.layout, at(change_pointer, "layout"));
                        const auto change_measure = index_.find_measure(change.location.measure);
                        if (!change_measure) {
                            error(at(change_pointer, "location/measure"),
                                  std::format("ID \"{}\" is not a global measure.", std::string(change.location.measure)));
                            continue;
                        }
                        if (check_position(change.location.position, at(change_pointer, "location/position")) &&
                            Rational(change.location.position.fraction) >= measure_length(root_, index_, *change_measure))
                            error(change_pointer, std::format("Layout \"{}\" starts at or past the end of the measure.", id_or(change)));
                    }
                }
            }
            if (!has_system) error(pointer, std::format("Score \"{}\" has pages but no systems", score.name));
        }
    }
};

} // namespace

std::expected<Indexed, std::vector<Diagnostic>> build_index(const Root &root, const IndexOptions &options) {
    return Builder(root, options).run();
}

} // namespace mu::iex::mnxio::model
