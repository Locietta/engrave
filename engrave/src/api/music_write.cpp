#include <algorithm>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <type_traits>
#include <utility>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/durationtype.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/sharedpart.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/editing/transpose.h"
#include "engraving/engravingproject.h"
#include <engrave/music.h>

namespace engrave {
namespace {

using Tick = mu::engraving::Fraction;
using mu::engraving::Measure;
using mu::engraving::Staff;
using mu::engraving::TDuration;
using mu::engraving::Transpose;

constexpr int shortest_denominator = 1024;

struct PhraseLocation {
    std::size_t lane;
    std::optional<std::size_t> item;
    std::optional<std::size_t> member;

    std::string prefix() const {
        auto text = std::format("lane {}", lane);
        if (item) text += std::format(", item {}", *item);
        if (member) text += std::format(", member {}", *member);
        return text + ": ";
    }
};

std::unexpected<Error> invalid(std::string message) { return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::move(message)}); }

Fraction public_fraction(Tick tick) {
    tick.reduce();
    return {tick.numerator64(), tick.denominator64()};
}

std::optional<Tick> as_tick(Fraction value) {
    if (value.den <= 0 || value.num < 0) return std::nullopt;
    const auto divisor = std::gcd(value.num, value.den);
    value.num /= divisor;
    value.den /= divisor;
    if (value.num > std::numeric_limits<int>::max() || value.den > std::numeric_limits<int>::max()) return std::nullopt;
    return Tick(static_cast<int>(value.num), static_cast<int>(value.den));
}

// Engine comparisons multiply the terms, so reduce and fit each intermediate
// into int before using it as an operand again.
std::optional<Tick> checked_tick(Tick tick) { return as_tick(public_fraction(tick)); }

std::unexpected<Error> invalid_tick() { return invalid("the position or duration exceeds the engine's fraction range"); }

std::optional<Tick> written_duration(Fraction value) {
    const auto duration = as_tick(value);
    if (!duration || duration->is_zero() || shortest_denominator % duration->denominator() != 0) return std::nullopt;
    return duration;
}

Result<Pitch> entry_pitch(const EntryPitch &entry, const Staff &staff, Tick tick, PitchDomain domain) {
    const auto *spelled = std::get_if<SpelledPitch>(&entry);
    if (!spelled) return invalid("a pitched staff requires a spelled pitch");
    const auto step = static_cast<int>(spelled->step);
    if (step < 0 || step >= mu::engraving::step_delta_octave || spelled->alter < -3 || spelled->alter > 3)
        return invalid("a pitch needs a step from C through B and an alteration from -3 through 3");
    const auto tpc = mu::engraving::step2tpc(step, static_cast<mu::engraving::AccidentalVal>(spelled->alter));
    const auto interval = staff.transpose(tick);
    const auto midi = (static_cast<std::int64_t>(spelled->octave) + 1) * mu::engraving::pitch_delta_octave + mu::engraving::tpc2pitch(tpc) +
                      (domain == PitchDomain::Written ? interval.chromatic : 0);
    if (midi < mu::engraving::min_pitch || midi > mu::engraving::max_pitch) return invalid("the concert pitch is outside MIDI range");

    int concert_tpc = tpc;
    if (domain == PitchDomain::Written) {
        concert_tpc = Transpose::transpose_tpc(tpc, interval, true);
        auto inverse = interval;
        inverse.flip();
        if (Transpose::transpose_tpc(concert_tpc, inverse, true) != tpc)
            return invalid("the written pitch cannot retain its spelling under this transposition");
    }
    return Pitch{static_cast<int>(midi), concert_tpc};
}

Result<DrumKey> entry_drum(const EntryPitch &entry, const mu::engraving::Drumset &kit) {
    const auto *drum = std::get_if<DrumKey>(&entry);
    if (!drum) return invalid("a drumset staff requires a drum key");
    if (!mu::engraving::pitch_is_valid(drum->key) || !kit.is_valid(drum->key))
        return invalid(std::format("key {} is absent from the staff's drumset", drum->key));
    return *drum;
}

bool is_after_grace(GraceKind kind) {
    return kind == GraceKind::Grace8After || kind == GraceKind::Grace16After || kind == GraceKind::Grace32After;
}

struct Cursor {
    const Measure *measure;
    Tick tick;
};

struct EventSpan {
    Cursor end;
    Tick last_measure_start;
};

struct ChordRefs {
    Tick end;
    std::vector<int> keys;
    std::vector<OpRef> ties;
    std::size_t entry_op;
    PhraseLocation location;
};

struct PhraseBatch {
    Batch batch;
    std::vector<PhraseLocation> locations;
    std::map<mu::engraving::track_idx_t, std::map<Tick, ChordRefs>> rhythms;

    std::size_t append(Op op, const PhraseLocation &location) {
        const auto index = batch.ops.size();
        batch.ops.push_back(std::move(op));
        locations.push_back(location);
        return index;
    }

    Result<void> add_ties() {
        for (const auto &[track, events] : rhythms) {
            for (const auto &[tick, chord] : events) {
                if (chord.ties.empty()) continue;
                const auto next = events.find(chord.end);
                if (next == events.end() || next->second.keys.empty())
                    return invalid(chord.location.prefix() + "a tie needs a next contiguous chord on the same track");
                for (const auto &source : chord.ties) {
                    const auto key = chord.keys[source.item - 1];
                    const auto &keys = next->second.keys;
                    const auto match = std::ranges::find(keys, key);
                    if (match == keys.end()) return invalid(chord.location.prefix() + "the next chord has no note of the same pitch");
                    append(AddTie{source, OpRef{next->second.entry_op, static_cast<std::size_t>(match - keys.begin()) + 1}},
                           chord.location);
                }
            }
        }
        return {};
    }

    void label(Error &error) const {
        if (error.op_index && *error.op_index < locations.size()) error.message.insert(0, locations[*error.op_index].prefix());
    }
};

struct LaneBuilder {
    const Staff &staff;
    int staff_index;
    int voice;
    PitchDomain domain;
    Cursor cursor;
    PhraseBatch &batch;
    PhraseLocation location;

    SegmentAddress address() const {
        return {from_engine(cursor.measure->eid()), staff_index, voice, public_fraction(cursor.tick), "ChordRest"};
    }

    Result<void> check_context_change(Tick from, Tick to) const {
        const auto *origin = staff.origin_part(from);
        const auto &instruments = origin->instruments();
        const auto change = instruments.upper_bound(from.ticks());
        if (change != instruments.end() && public_fraction(Tick::from_ticks(change->first)) < public_fraction(to))
            return invalid("an event cannot sustain across an instrument change inside a measure");
        if (staff.part()->is_shared_part()) {
            for (const auto &[tick, state] : mu::engraving::to_shared_part(staff.part())->authored_states()) {
                const auto position = public_fraction(tick);
                if (public_fraction(from) < position && position < public_fraction(to) && staff.origin_part(tick) != origin)
                    return invalid("an event cannot sustain across a staff-sharing change inside a measure");
            }
        }
        const auto *meter = staff.next_time_sig(from + Tick::eps());
        if (meter && public_fraction(meter->tick()) < public_fraction(to))
            return invalid("an event cannot sustain across a meter change inside a measure");
        return {};
    }

    Result<EventSpan> advance(Tick length, bool drums, const Tick &ratio) const {
        auto end = cursor;
        while (end.measure) {
            if (staff.is_tab_staff(end.tick) || staff.is_drum_staff(end.tick) != drums)
                return invalid("the event crosses a change in staff type");
            const auto start = end.tick;
            const auto stretch = checked_tick(staff.time_stretch(start));
            const auto scale = stretch ? checked_tick(*stretch * ratio) : std::nullopt;
            const auto boundary = checked_tick(end.measure->end_tick());
            const auto room = boundary ? checked_tick(*boundary - start) : std::nullopt;
            const auto available = scale && room ? checked_tick(*room * *scale) : std::nullopt;
            if (!available || available->is_zero()) return invalid_tick();
            if (length < *available) {
                const auto elapsed = checked_tick(length / *scale);
                const auto stop = elapsed ? checked_tick(start + *elapsed) : std::nullopt;
                if (!stop) return invalid_tick();
                if (auto changed = check_context_change(start, *stop); !changed) return std::unexpected(std::move(changed.error()));
                return EventSpan{{end.measure, *stop}, start};
            }
            const auto remaining = checked_tick(length - *available);
            if (!remaining) return invalid_tick();
            if (auto changed = check_context_change(start, *boundary); !changed) return std::unexpected(std::move(changed.error()));
            end = {end.measure->next_measure(), *boundary};
            if (remaining->is_zero()) return EventSpan{end, start};
            length = *remaining;
        }
        return invalid("the event extends beyond the end of the score");
    }

    Result<void> check_tuplet_overlap(Tick end) const {
        const auto track = mu::engraving::staff2track(staff_index, voice);
        for (auto *measure = cursor.measure; measure && measure->tick() < end; measure = measure->next_measure()) {
            for (auto *segment = measure->first(mu::engraving::SegmentType::CHORD_REST); segment && segment->tick() < end;
                 segment = segment->next(mu::engraving::SegmentType::CHORD_REST)) {
                const auto *item = segment->element(track);
                if (!item || !item->is_chord_rest()) continue;
                const auto *rhythm = mu::engraving::to_chord_rest(item);
                if (rhythm->tuplet() && rhythm->end_tick() > cursor.tick) return invalid("the event overlaps an existing tuplet");
            }
        }
        return {};
    }

    Result<void> add_graces(const std::vector<PhraseGrace> &graces, OpRef principal, Tick pitch_tick,
                            std::optional<Fraction> event_end = {}) {
        for (const auto &grace : graces) {
            if (is_after_grace(grace.kind) != event_end.has_value())
                return invalid("the grace kind does not match its side of the principal chord");
            std::vector<Pitch> pitches;
            for (const auto &note : grace.notes) {
                auto pitch = entry_pitch(note, staff, pitch_tick, domain);
                if (!pitch) return std::unexpected(std::move(pitch.error()));
                pitches.push_back(*pitch);
            }
            batch.append(AddGraceChord{principal, {grace.kind, grace.written_duration, std::move(pitches)}, event_end}, location);
        }
        return {};
    }

    Result<std::size_t> enter(const PhraseEvent &event, Tick duration, Tick end, bool drums, bool continue_ties) {
        ChordRefs result{.end = end, .location = location};
        std::set<int> seen;
        std::vector<Pitch> pitches;
        std::vector<DrumKey> keys;
        const auto *kit = drums ? staff.origin_part(cursor.tick)->instrument(cursor.tick)->drumset() : nullptr;
        if (drums && !kit) return invalid("the staff has no effective drumset");
        for (const auto &note : event.notes) {
            int key;
            if (kit) {
                auto drum = entry_drum(note.pitch, *kit);
                if (!drum) return std::unexpected(std::move(drum.error()));
                key = drum->key;
                keys.push_back(*drum);
            } else {
                auto pitch = entry_pitch(note.pitch, staff, cursor.tick, domain);
                if (!pitch) return std::unexpected(std::move(pitch.error()));
                key = pitch->pitch;
                pitches.push_back(*pitch);
            }
            if (!seen.insert(key).second) return invalid("the pitches or drum keys of one chord must differ");
            result.keys.push_back(key);
        }
        const auto at = address();
        const auto length = public_fraction(duration);
        result.entry_op = drums ? batch.append(EnterDrums{at, length, std::move(keys)}, location) :
                                  batch.append(EnterNotes{at, length, std::move(pitches)}, location);
        for (std::size_t i = 0; i < event.notes.size(); ++i) {
            if (continue_ties || event.notes[i].tie_to_next) result.ties.push_back({result.entry_op, i + 1});
        }
        const auto entry = result.entry_op;
        batch.rhythms[mu::engraving::staff2track(staff_index, voice)].emplace(cursor.tick, std::move(result));
        return entry;
    }

    Result<void> add_event(const PhraseEvent &event, const Tick &ratio = {1, 1}) {
        if (!cursor.measure) return invalid("the event starts past the end of the score");
        const auto duration = written_duration(event.written_duration);
        if (!duration) return invalid("the event needs a positive duration in ordinary note values");
        const bool drums = staff.is_drum_staff(cursor.tick);
        const bool has_grace = !event.before.empty() || !event.after.empty();
        if (has_grace && event.notes.empty()) return invalid("a rest cannot carry grace chords");
        if (has_grace && drums) return invalid("drumset grace entry is unavailable");
        auto span = advance(*duration, drums, ratio);
        if (!span) return std::unexpected(std::move(span.error()));
        const auto end = span->end.tick;
        if (auto checked = check_tuplet_overlap(end); !checked) return checked;

        auto remaining = *duration;
        // The drum operation uses the kit active at its entry and is bounded by a barline.
        while (drums && public_fraction(cursor.measure->end_tick()) < public_fraction(end)) {
            // advance checked the terms and the unscaled length for this same measure.
            const auto boundary = *checked_tick(cursor.measure->end_tick());
            const auto piece = *checked_tick(*checked_tick(boundary - cursor.tick) * *checked_tick(staff.time_stretch(cursor.tick)));
            auto source = enter(event, piece, boundary, true, true);
            if (!source) return std::unexpected(std::move(source.error()));
            remaining = *checked_tick(remaining - piece);
            cursor = {cursor.measure->next_measure(), boundary};
        }
        auto source = enter(event, remaining, end, drums, false);
        if (!source) return std::unexpected(std::move(source.error()));
        const OpRef principal{*source, 0};
        if (auto graces = add_graces(event.before, principal, cursor.tick); !graces) return graces;
        if (auto graces = add_graces(event.after, principal, span->last_measure_start, public_fraction(end)); !graces) return graces;
        cursor = span->end;
        return {};
    }

    Result<void> add_tuplet(const TupletGroup &group) {
        if (!cursor.measure) return invalid("the tuplet starts past the end of the score");
        if (group.actual <= 0 || group.normal <= 0 || group.actual > shortest_denominator || group.normal > shortest_denominator)
            return invalid("the tuplet counts must be between 1 and 1024");
        if (group.members.empty()) return invalid("a tuplet needs at least one member");
        Tick sum;
        for (std::size_t index = 0; index < group.members.size(); ++index) {
            location.member = index;
            const auto duration = written_duration(group.members[index].written_duration);
            if (!duration) return invalid("the tuplet member needs a positive duration in ordinary note values");
            const auto total = checked_tick(sum + *duration);
            if (!total) return invalid_tick();
            sum = *total;
        }
        location.member.reset();
        const auto base = checked_tick(sum / group.actual);
        const auto placeholder = base ? checked_tick(*base * group.normal) : std::nullopt;
        if (!placeholder) return invalid_tick();
        if (!TDuration::is_valid(*base) || *placeholder < Tick(1, 512) || mu::engraving::to_duration_list(*placeholder, true).size() != 1)
            return invalid("the ratio and member durations do not give one notated span for a tuplet");
        const bool drums = staff.is_drum_staff(cursor.tick);
        auto span = advance(*placeholder, drums, {1, 1});
        if (!span) return std::unexpected(std::move(span.error()));
        if (public_fraction(span->end.tick) > public_fraction(cursor.measure->end_tick()))
            return invalid("a tuplet must fit within one measure");
        if (auto checked = check_tuplet_overlap(span->end.tick); !checked) return checked;
        const auto at = address();
        const auto length = public_fraction(*placeholder);
        const auto rest = drums ? batch.append(EnterDrums{at, length, {}}, location) : batch.append(EnterNotes{at, length, {}}, location);
        const OpRef target{rest, 0};
        batch.append(AddTuplet{target, target, group.actual, group.normal}, location);

        const Tick ratio(group.actual, group.normal);
        for (std::size_t index = 0; index < group.members.size(); ++index) {
            location.member = index;
            if (auto result = add_event(group.members[index], ratio); !result) return result;
        }
        location.member.reset();
        return {};
    }
};

using LaneIntervals = std::map<mu::engraving::track_idx_t, std::map<Tick, Tick>>;

Result<void> check_overlap(LaneIntervals &intervals, mu::engraving::track_idx_t track, Tick start, Tick end) {
    auto &same_track = intervals[track];
    const auto next = same_track.lower_bound(start);
    if ((next != same_track.end() && next->first < end) || (next != same_track.begin() && std::prev(next)->second > start))
        return invalid("the lane overlaps another lane on the same track");
    same_track.emplace_hint(next, start, end);
    return {};
}

Result<void> lower_lane(const PhraseLane &lane, const mu::engraving::Score &score, mu::engraving::MasterScore &master, std::size_t index,
                        PhraseBatch &batch, LaneIntervals &intervals) {
    const auto &start = lane.start;
    const PhraseLocation location{index};
    auto prefix_error = [](Error error, const PhraseLocation &at) {
        return std::unexpected(Error{error.code, at.prefix() + error.message});
    };
    if (lane.items.empty()) return invalid(location.prefix() + "a lane needs at least one item");
    if (lane.domain != PitchDomain::Concert && lane.domain != PitchDomain::Written)
        return invalid(location.prefix() + "unknown pitch domain");
    if (start.staff < 0 || static_cast<std::size_t>(start.staff) >= score.nstaves() || start.voice < 0 ||
        start.voice >= mu::engraving::voices)
        return invalid(location.prefix() + "the staff or voice is outside the score");
    auto resolved = resolve(master, start.measure);
    if (!resolved) return prefix_error(std::move(resolved.error()), location);
    if (!resolved->object->is_measure() || resolved->object->score() != &score)
        return invalid(location.prefix() + "the starting measure must belong to the selected score");
    const auto *measure = mu::engraving::to_measure(resolved->object);
    const auto offset = as_tick(start.offset);
    if (!offset || public_fraction(*offset) >= public_fraction(measure->ticks()))
        return invalid(location.prefix() + "the offset is outside the starting measure");

    const auto begin = checked_tick(measure->tick());
    const auto tick = begin ? checked_tick(*begin + *offset) : std::nullopt;
    if (!tick) return prefix_error(invalid_tick().error(), location);
    LaneBuilder builder{*score.staff(start.staff), start.staff, start.voice, lane.domain, {measure, *tick}, batch, location};
    for (std::size_t item = 0; item < lane.items.size(); ++item) {
        builder.location.item = item;
        auto result = std::visit(
            [&](const auto &entry) -> Result<void> {
                if constexpr (std::is_same_v<std::decay_t<decltype(entry)>, PhraseEvent>)
                    return builder.add_event(entry);
                else
                    return builder.add_tuplet(entry);
            },
            lane.items[item]);
        if (!result) return prefix_error(std::move(result.error()), builder.location);
    }
    auto overlap = check_overlap(intervals, mu::engraving::staff2track(start.staff, start.voice), *tick, builder.cursor.tick);
    if (!overlap) return prefix_error(std::move(overlap.error()), location);
    return {};
}

} // namespace

Result<EditResult> Document::write_phrase(const Phrase &phrase, const EditOptions &options) {
    impl_->engine->check_thread();
    auto &master = *impl_->project->master_score();
    auto score = select_score(master, phrase.score);
    if (!score) return std::unexpected(std::move(score.error()));
    if (phrase.lanes.empty()) return invalid("a phrase needs at least one lane");
    PhraseBatch batch;
    LaneIntervals intervals;
    for (std::size_t index = 0; index < phrase.lanes.size(); ++index) {
        if (auto lane = lower_lane(phrase.lanes[index], **score, master, index, batch, intervals); !lane)
            return std::unexpected(std::move(lane.error()));
    }
    if (auto ties = batch.add_ties(); !ties) return std::unexpected(std::move(ties.error()));
    auto result = edit(batch.batch, options);
    if (!result)
        batch.label(result.error());
    else if (result->rejection)
        batch.label(result->rejection->error);
    return result;
}

} // namespace engrave
