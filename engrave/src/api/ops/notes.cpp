#include <algorithm>
#include <format>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engrave/src/api/batch.h"
#include "engrave/src/api/handles.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/durationtype.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/interval.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/noteval.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/utils.h"
#include "engraving/editing/editnote.h"
#include "engraving/editing/noteinput.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/editing/transpose.h"

namespace engrave::api {
namespace {

using mu::engraving::Chord;
using mu::engraving::ChordRest;
using mu::engraving::DirectionV;
using mu::engraving::EngravingItem;
using mu::engraving::Note;
using mu::engraving::NoteType;
using mu::engraving::NoteVal;
using mu::engraving::Score;
using mu::engraving::Segment;
using mu::engraving::Staff;
using mu::engraving::TDuration;
using mu::engraving::Transpose;

std::unexpected<Error> invalid(std::string message) { return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::move(message)}); }

std::optional<mu::engraving::Fraction> to_reduced_engine_fraction(Fraction value) {
    if (value.den <= 0) return std::nullopt;
    // Taking the remainder first keeps INT64_MIN out of std::gcd.
    const auto divisor = std::gcd(value.num % value.den, value.den);
    value.num /= divisor;
    value.den /= divisor;
    if (value.num < std::numeric_limits<int>::min() || value.num > std::numeric_limits<int>::max() ||
        value.den > std::numeric_limits<int>::max())
        return std::nullopt;
    return mu::engraving::Fraction(static_cast<int>(value.num), static_cast<int>(value.den));
}

ChordRest *rhythm_covering(mu::engraving::Measure &measure, const mu::engraving::Fraction &tick, mu::engraving::track_idx_t track) {
    EngravingItem *previous = nullptr;
    for (auto *segment = measure.first(mu::engraving::SegmentType::CHORD_REST); segment && segment->tick() < tick;
         segment = segment->next(mu::engraving::SegmentType::CHORD_REST)) {
        if (auto *item = segment->element(track)) previous = item;
    }
    if (!previous || !previous->is_chord_rest()) return nullptr;
    auto *rhythm = mu::engraving::to_chord_rest(previous);
    return tick < rhythm->end_tick() ? rhythm : nullptr;
}

std::optional<mu::engraving::Fraction> split_duration(const mu::engraving::Fraction &length, const mu::engraving::Fraction &unit) {
    const auto offset = to_reduced_engine_fraction({length.numerator64(), length.denominator64()});
    if (!offset) return std::nullopt;
    const auto local = *offset / unit;
    const auto duration = to_reduced_engine_fraction({local.numerator64(), local.denominator64()});
    // A rest can only be split at multiples of the shortest notated duration.
    const auto shortest = mu::engraving::TDuration(mu::engraving::DurationType::V_1024TH).fraction();
    if (!duration || shortest.denominator() % duration->denominator() != 0) return std::nullopt;
    return duration;
}

// The element a running operation works on.
Result<EngravingItem *> edited_item(EditContext &context, const Target &target) {
    auto object = context.object(target);
    if (!object) return std::unexpected(object.error());
    if (!(*object)->is_engraving_item()) return invalid("this operation works on an element, not on a part or a score");
    return mu::engraving::to_engraving_item(*object);
}

// The concert and the written spelling of a pitch at this note's position: the pitch's
// tpc in the staff's concert key, then transposed into what the staff writes, as
// Note::setTpcFromPitch spells a note that the score enters.
std::pair<int, int> spell(const Note &note, int pitch, std::optional<int> tpc) {
    const int tpc1 = tpc.value_or(note.tpc1default(pitch));
    mu::engraving::Interval interval = note.staff()->transpose(note.chord()->tick());
    int tpc2 = tpc1;
    if (!interval.is_zero()) {
        interval.flip();
        tpc2 = Transpose::transpose_tpc(tpc1, interval, true);
    }
    return {tpc1, tpc2};
}

// What the engine's pitch change does not do: it sets the pitch of one note and of its
// linked copies, so a tablature note (whose fret and string go with the pitch) and a
// percussion note (whose drum the pitch names) need the engine's own edits for those.
// A tied note changes alone, as it does when the score's own pitch keys move it.
std::optional<std::string> unchangeable(const Note &note) {
    const Chord *chord = note.chord();
    const Staff *staff = note.staff();
    if (!chord || !staff) return "it is not in a chord of the score";
    const mu::engraving::Fraction tick = chord->tick();
    if (staff->is_tab_staff(tick)) return "a tablature note changes with its fret and string";
    if (staff->is_drum_staff(tick)) return "a percussion note is chosen from the drum set";
    return std::nullopt;
}

// The note value a Pitch asks for. Its tpc, when given, is the concert-pitch spelling:
// the engine derives the written one from the staff's transposition.
NoteVal note_value(const Pitch &pitch) {
    NoteVal value(pitch.pitch);
    if (pitch.tpc) value.tpc1 = *pitch.tpc;
    return value;
}

Result<std::vector<NoteVal>> note_values(const std::vector<Pitch> &pitches) {
    std::vector<NoteVal> values;
    values.reserve(pitches.size());
    for (const auto &pitch : pitches) {
        if (!mu::engraving::pitch_is_valid(pitch.pitch)) return invalid(std::format("{} is not a pitch", pitch.pitch));
        if (pitch.tpc && !mu::engraving::tpc_is_valid(*pitch.tpc)) return invalid(std::format("{} is not a tonal pitch class", *pitch.tpc));
        values.push_back(note_value(pitch));
    }
    return values;
}

struct GraceStyle {
    NoteType type;
    int denominator;
};

std::optional<GraceStyle> grace_style(GraceKind kind) {
    switch (kind) {
        case GraceKind::Appoggiatura: return GraceStyle{NoteType::APPOGGIATURA, 0};
        case GraceKind::Acciaccatura: return GraceStyle{NoteType::ACCIACCATURA, 0};
        case GraceKind::Grace4: return GraceStyle{NoteType::GRACE4, 4};
        case GraceKind::Grace8After: return GraceStyle{NoteType::GRACE8_AFTER, 8};
        case GraceKind::Grace16: return GraceStyle{NoteType::GRACE16, 16};
        case GraceKind::Grace16After: return GraceStyle{NoteType::GRACE16_AFTER, 16};
        case GraceKind::Grace32: return GraceStyle{NoteType::GRACE32, 32};
        case GraceKind::Grace32After: return GraceStyle{NoteType::GRACE32_AFTER, 32};
    }
    return std::nullopt;
}

Result<void> validate_chord(const std::vector<NoteVal> &values) {
    std::vector<int> keys;
    keys.reserve(values.size());
    for (const auto &value : values) keys.push_back(value.pitch);
    std::ranges::sort(keys);
    if (std::ranges::adjacent_find(keys) != keys.end()) return invalid("the pitches or drum keys of one chord must differ");
    return {};
}

Result<Step> prepare_entry(EditContext &context, const SegmentAddress &address, Fraction length, std::vector<NoteVal> values,
                           bool percussion) {
    if (address.kind != "ChordRest") return invalid("rhythmic entry requires a ChordRest segment address");
    const auto duration = to_reduced_engine_fraction(length);
    if (!duration || length.num <= 0) return invalid("the duration must be a positive fraction of a whole note");
    if (auto valid = validate_chord(values); !valid) return std::unexpected(valid.error());
    auto target = context.target(Ref{Address{address}});
    if (!target) return std::unexpected(target.error());
    const auto tick = *to_reduced_engine_fraction(address.tick);

    return [target = *target, tick, duration = *duration, values = std::move(values), percussion](EditContext &context) -> Result<void> {
        const auto &at = std::get<SegmentAddress>(target.ref);
        const auto location = context.writable_measure(at);
        if (!location) return std::unexpected(location.error());
        auto *measure = *location;
        Score *score = measure->score();
        Segment *start = find_segment(*measure, at);
        if (start && id_class(*start) == IdClass::LAYOUT) return invalid("layout builds this segment; it cannot be edited");
        const auto track = mu::engraving::staff2track(at.staff, at.voice);
        const Staff *staff = score->staff(at.staff);

        const mu::engraving::Drumset *kit = nullptr;
        if (percussion) {
            if (staff->is_drum_staff(tick)) kit = staff->origin_part(tick)->instrument(tick)->drumset();
            if (!kit) return invalid("EnterDrums requires a percussion staff with a drumset");
            for (const auto &value : values) {
                if (!kit->is_valid(value.pitch)) return invalid(std::format("key {} is absent from the staff's drumset", value.pitch));
            }
        } else if (staff->is_tab_staff(tick) || staff->is_drum_staff(tick)) {
            return invalid("EnterNotes requires a pitched staff");
        }

        const bool interior = measure->tick() < tick && tick < measure->end_tick();
        auto *covered = interior && (!start || !start->element(track)) ? rhythm_covering(*measure, tick, track) : nullptr;
        if (covered && !covered->is_rest()) return invalid("the entry position falls inside a sounding event");
        auto *covering = covered ? mu::engraving::to_rest(covered) : nullptr;
        const bool open_voice = interior && at.voice != 0 && !measure->has_voice(track);
        if (!start && !covering && !open_voice) return std::unexpected(missing_segment(at));
        auto *tuplet = covering ? covering->tuplet() : nullptr;
        if (const auto *item = start ? start->element(track) : nullptr; item && item->is_chord_rest())
            tuplet = mu::engraving::to_chord_rest(item)->tuplet();
        const auto shortest = mu::engraving::TDuration(mu::engraving::DurationType::V_1024TH).fraction();
        if (shortest.denominator() % duration.denominator() != 0)
            return invalid("the duration cannot be written with ordinary note values");
        if (kit) {
            const auto entry_end = tick + mu::engraving::actual_ticks(duration, tuplet, staff->time_stretch(tick));
            auto limit = measure->end_tick();
            if (tuplet) limit = std::min(limit, tuplet->tick() + tuplet->actual_ticks());
            if (entry_end > limit) return invalid("drum entry must fit within its measure and tuplet");
            // Gap creation can delete a later tuplet as a unit, even when only its first note overlaps.
            for (auto *segment = measure->first(mu::engraving::SegmentType::CHORD_REST); segment && segment->tick() < entry_end;
                 segment = segment->next(mu::engraving::SegmentType::CHORD_REST)) {
                const auto *item = segment->element(track);
                if (!item || !item->is_chord_rest()) continue;
                const auto *rhythm = mu::engraving::to_chord_rest(item);
                if (rhythm->end_tick() > tick && rhythm->tuplet() != tuplet) return invalid("drum entry cannot cross a tuplet boundary");
            }
        }
        if (covering || open_voice) {
            const auto begin = covering ? covering->tick() : measure->tick();
            const auto end = covering ? covering->end_tick() : measure->end_tick();
            const auto leading = split_duration(tick - begin, mu::engraving::actual_ticks({1, 1}, tuplet, staff->time_stretch(begin)));
            const auto trailing = split_duration(end - tick, mu::engraving::actual_ticks({1, 1}, tuplet, staff->time_stretch(tick)));
            if (!leading || !trailing) return invalid("the entry position does not split into notated rests");
            // make_gap visits only the chord/rests at and after its start.
            const auto visible = covering ? covering->get_property(mu::engraving::Pid::VISIBLE) : mu::engraving::PropertyValue{};
            const auto color = covering ? covering->get_property(mu::engraving::Pid::COLOR) : mu::engraving::PropertyValue{};
            if (covering) score->undo_remove_element(covering);
            const auto left = score->set_rests(begin, track, *leading, true, tuplet);
            if (left.empty()) return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine could not enter the leading rest"});
            const auto right = score->set_rests(tick, track, *trailing, true, tuplet);
            if (right.empty()) return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine could not enter the trailing rest"});
            // Empty secondary voices normally start with implicit gaps. This split
            // authors the leading silence so normalization retains it as a rest.
            for (auto *rest : left) rest->set_gap(false);
            if (covering) {
                for (const auto *side : {&left, &right}) {
                    for (auto *rest : *side) {
                        rest->undo_change_property(mu::engraving::Pid::VISIBLE, visible);
                        rest->undo_change_property(mu::engraving::Pid::COLOR, color);
                    }
                }
            }
            start = right.front()->segment();
        }
        auto mapped_value = [kit](NoteVal value) {
            if (kit) value.head_group = kit->note_head(value.pitch);
            return value;
        };
        const NoteVal first = values.empty() ? NoteVal() : mapped_value(values.front());
        const auto stem = kit && !values.empty() ? kit->stem_direction(first.pitch) : DirectionV::AUTO;
        EngravingItem *last = nullptr;
        score->set_note_rest_at(start, track, first, duration, stem, false, {}, false, &last);
        const auto piece_at = [&](const mu::engraving::Fraction &offset) -> ChordRest * {
            auto *segment = score->tick2segment(offset, true, mu::engraving::SegmentType::CHORD_REST, false);
            auto *item = segment ? segment->element(track) : nullptr;
            return item && item->is_chord_rest() ? mu::engraving::to_chord_rest(item) : nullptr;
        };
        auto *piece = piece_at(tick);
        if (!piece || !last) return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine left no chord or rest at this position"});

        // Stop at the last piece produced by note entry, even if there is a pre-existing
        // chord at the following tick.
        std::vector<Note *> previous(values.size());
        mu::engraving::Fraction remaining = duration;
        for (;;) {
            context.created(piece);
            remaining -= piece->ticks();
            if (!values.empty()) {
                auto *chord = piece->is_chord() ? mu::engraving::to_chord(piece) : nullptr;
                Note *lead = chord ? chord->find_note(first.pitch) : nullptr;
                if (!lead || (previous.front() && (!previous.front()->tie_for() || previous.front()->tie_for()->end_note() != lead)))
                    return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine did not connect the entered chord pieces"});
                context.created(lead);
                previous.front() = lead;
                for (std::size_t i = 1; i < values.size(); ++i) {
                    Note *note = mu::engraving::NoteInput::add_note_to_chord(score, chord, mapped_value(values[i]));
                    if (previous[i]) {
                        auto *tie = mu::engraving::Factory::create_tie(previous[i]);
                        tie->set_note_span(previous[i], note);
                        score->undo_add_element(tie);
                    }
                    context.created(note);
                    previous[i] = note;
                }
                if (kit) mu::engraving::update_percussion_notes(chord, kit);
            }
            if (last == piece || (!previous.empty() && last == previous.front())) {
                if (remaining.is_zero()) return {};
                break;
            }
            if (remaining <= mu::engraving::Fraction(0, 1)) break;
            piece = piece_at(piece->end_tick());
            if (!piece) break;
        }
        return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine did not fill the requested duration"});
    };
}

} // namespace

Result<Step> prepare(EditContext &context, const EnterNotes &op) {
    const auto *at = std::get_if<SegmentAddress>(&op.at);
    if (!at) return invalid("EnterNotes enters notes at a segment address");
    auto values = note_values(op.pitches);
    if (!values) return std::unexpected(values.error());
    return prepare_entry(context, *at, op.duration, std::move(*values), false);
}

Result<Step> prepare(EditContext &context, const EnterDrums &op) {
    std::vector<NoteVal> values;
    values.reserve(op.keys.size());
    for (const auto key : op.keys) {
        if (key.key < 0 || key.key >= mu::engraving::drum_instruments)
            return invalid(std::format("drum key {} must be between 0 and 127", key.key));
        values.emplace_back(key.key);
    }
    return prepare_entry(context, op.at, op.duration, std::move(values), true);
}

Result<Step> prepare(EditContext &context, const AddGraceChord &op) {
    const auto style = grace_style(op.grace.kind);
    const auto duration = to_reduced_engine_fraction(op.grace.written_duration);
    if (!style || !duration || duration->numerator() <= 0 || *duration > mu::engraving::Fraction(1, 4))
        return invalid("a grace chord needs a written duration of a quarter note or shorter");
    TDuration written;
    written.set_val(duration->ticks());
    if (written.fraction() != *duration || (style->denominator && *duration != mu::engraving::Fraction(1, style->denominator)))
        return invalid("the written duration does not fit the grace kind");
    if (op.grace.pitches.empty()) return invalid("a grace chord needs at least one pitch");
    auto values = note_values(op.grace.pitches);
    if (!values) return std::unexpected(values.error());
    if (auto valid = validate_chord(*values); !valid) return std::unexpected(valid.error());
    auto target = context.target(op.principal);
    if (!target) return std::unexpected(target.error());
    const bool after = bool(style->type & (NoteType::GRACE8_AFTER | NoteType::GRACE16_AFTER | NoteType::GRACE32_AFTER));
    const auto event_end = op.event_end ? to_reduced_engine_fraction(*op.event_end) : std::nullopt;
    if (op.event_end && (!after || !event_end || event_end->numerator() <= 0))
        return invalid("an event end requires an after-grace and a positive score tick");

    return [target = *target, type = style->type, after, event_end, length = duration->ticks(),
            values = std::move(*values)](EditContext &context) -> Result<void> {
        auto item = edited_item(context, target);
        if (!item) return std::unexpected(item.error());
        if (!(*item)->is_chord()) return invalid("a grace chord requires a principal chord");
        auto *principal = mu::engraving::to_chord(*item);
        if (principal->is_grace()) return invalid("a grace chord requires a principal chord");
        if (event_end) {
            while (principal->end_tick() < *event_end) {
                const auto *tie = principal->notes().empty() ? nullptr : principal->notes().front()->tie_for();
                const auto *next = tie ? tie->end_note() : nullptr;
                auto *continuation = next ? next->chord() : nullptr;
                if (!continuation || continuation->track() != principal->track() || continuation->tick() != principal->end_tick())
                    return invalid("the grace anchor has no tied continuation at the event end");
                principal = continuation;
            }
            if (principal->end_tick() != *event_end) return invalid("the grace anchor does not end at the requested tick");
        }
        const auto tick = principal->tick();
        if (principal->staff()->is_tab_staff(tick) || principal->staff()->is_drum_staff(tick))
            return invalid("AddGraceChord requires a pitched staff");
        auto *score = principal->score();

        const auto &first = values.front();
        // The native after-grace view plays the stored chords in reverse order.
        const auto index = after ? 0 : principal->grace_notes().size();
        Note *note = score->set_grace_note(principal, first.pitch, type, length, index);
        auto *chord = note->chord();
        const auto [tpc1, tpc2] =
            spell(*note, first.pitch, first.tpc1 == mu::engraving::TPC_INVALID ? std::nullopt : std::optional{first.tpc1});
        mu::engraving::EditNote::undo_change_pitch(score, note, first.pitch, tpc1, tpc2);
        context.created(chord);
        context.created(note);
        for (std::size_t i = 1; i < values.size(); ++i) {
            context.created(mu::engraving::NoteInput::add_note_to_chord(score, chord, values[i]));
        }
        return {};
    };
}

Result<Step> prepare(EditContext &context, const SetPitch &op) {
    if (!mu::engraving::pitch_is_valid(op.pitch)) return invalid(std::format("{} is not a pitch", op.pitch));
    if (op.tpc && !mu::engraving::tpc_is_valid(*op.tpc)) return invalid(std::format("{} is not a tonal pitch class", *op.tpc));
    auto target = context.target(op.note);
    if (!target) return std::unexpected(target.error());
    return [target = *target, pitch = op.pitch, tpc = op.tpc](EditContext &context) -> Result<void> {
        auto item = edited_item(context, target);
        if (!item) return std::unexpected(item.error());
        if (!(*item)->is_note()) return invalid("SetPitch sets the pitch of a note");
        Note *note = mu::engraving::to_note(*item);
        if (const auto why = unchangeable(*note)) return invalid(std::format("the note keeps its pitch: {}", *why));
        const auto [tpc1, tpc2] = spell(*note, pitch, tpc);
        if (!mu::engraving::tpc_is_valid(tpc1) || !mu::engraving::tpc_is_valid(tpc2))
            return invalid(std::format("the pitch cannot be spelled at this position as {}", pitch));
        std::vector<Note *> changed_notes;
        auto record_if_changed = [&](Note *candidate) {
            if (candidate && (candidate->pitch() != pitch || candidate->tpc1() != tpc1 || candidate->tpc2() != tpc2)) {
                changed_notes.push_back(candidate);
            }
        };
        record_if_changed(note);
        for (mu::engraving::EngravingObject *linked : note->link_list()) {
            if (!linked || !linked->is_note() || linked == note) continue;
            record_if_changed(mu::engraving::to_note(linked));
        }
        mu::engraving::EditNote::undo_change_pitch(note->score(), note, pitch, tpc1, tpc2);
        for (Note *changed : changed_notes) context.repitched(*changed);
        return {};
    };
}

Result<Step> prepare(EditContext &context, const SetDuration &op) {
    const auto duration = to_reduced_engine_fraction(op.duration);
    if (!duration || op.duration.num <= 0) return invalid("the duration must be a positive fraction of a whole note");
    auto target = context.target(op.chord_rest);
    if (!target) return std::unexpected(target.error());
    return [target = *target, duration = *duration](EditContext &context) -> Result<void> {
        auto item = edited_item(context, target);
        if (!item) return std::unexpected(item.error());
        if (!(*item)->is_chord_rest()) return invalid("SetDuration changes the length of a chord or of a rest");
        ChordRest *chord_rest = mu::engraving::to_chord_rest(*item);
        Score *score = chord_rest->score();
        score->change_c_rlen(chord_rest, duration);
        return {};
    };
}

Result<Step> prepare(EditContext &context, const Remove &op) {
    auto target = context.target(op.target);
    if (!target) return std::unexpected(target.error());
    return [target = *target](EditContext &context) -> Result<void> {
        auto item = edited_item(context, target);
        if (!item) return std::unexpected(item.error());
        EngravingItem *target_item = *item;
        Score *score = target_item->score();
        if (target_item->is_note()) {
            // deleteItem's note case: one note of a chord goes alone; the last one goes
            // with its chord, which deleteItem replaces with a rest of the same length.
            // Only the selection it makes of the remaining down note is left out.
            Note *note = mu::engraving::to_note(target_item);
            Chord *chord = note->chord();
            if (chord && chord->notes().size() > 1) {
                score->undo_remove_element(note);
                return {};
            }
            score->delete_item(chord);
            return {};
        }
        score->delete_item(target_item);
        return {};
    };
}

} // namespace engrave::api
