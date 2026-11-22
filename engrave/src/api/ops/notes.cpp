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
#include "engraving/dom/durationtype.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/interval.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/noteval.h"
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
using mu::engraving::NoteVal;
using mu::engraving::Score;
using mu::engraving::Segment;
using mu::engraving::Staff;
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

mu::engraving::Rest *rest_covering(mu::engraving::Measure &measure, const mu::engraving::Fraction &tick, mu::engraving::track_idx_t track) {
    EngravingItem *previous = nullptr;
    for (auto *segment = measure.first(mu::engraving::SegmentType::CHORD_REST); segment && segment->tick() < tick;
         segment = segment->next(mu::engraving::SegmentType::CHORD_REST)) {
        if (auto *item = segment->element(track)) previous = item;
    }
    if (!previous || !previous->is_rest()) return nullptr;
    auto *rest = mu::engraving::to_rest(previous);
    return tick < rest->end_tick() ? rest : nullptr;
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

} // namespace

Result<Step> prepare(EditContext &context, const EnterNotes &op) {
    const auto *at = std::get_if<SegmentAddress>(&op.at);
    if (!at) return invalid("EnterNotes enters notes at a segment address");
    if (at->kind != "ChordRest") return invalid(std::format("EnterNotes enters notes in a ChordRest segment, not in a '{}' one", at->kind));
    const auto duration = to_reduced_engine_fraction(op.duration);
    if (!duration || op.duration.num <= 0) return invalid("the duration must be a positive fraction of a whole note");
    for (const Pitch &pitch : op.pitches) {
        if (!mu::engraving::pitch_is_valid(pitch.pitch)) return invalid(std::format("{} is not a pitch", pitch.pitch));
        if (pitch.tpc && !mu::engraving::tpc_is_valid(*pitch.tpc)) return invalid(std::format("{} is not a tonal pitch class", *pitch.tpc));
    }
    std::vector<int> pitches;
    pitches.reserve(op.pitches.size());
    for (const Pitch &pitch : op.pitches) pitches.push_back(pitch.pitch);
    std::ranges::sort(pitches);
    if (std::ranges::adjacent_find(pitches) != pitches.end()) return invalid("the pitches of one chord must differ");
    auto target = context.target(op.at);
    if (!target) return std::unexpected(target.error());
    const auto tick = *to_reduced_engine_fraction(at->tick);

    return [target = *target, tick, duration = *duration, pitches = op.pitches](EditContext &context) -> Result<void> {
        const auto &at = std::get<SegmentAddress>(target.ref);
        const auto location = context.writable_measure(at);
        if (!location) return std::unexpected(location.error());
        auto *measure = *location;
        Score *score = measure->score();
        Segment *start = find_segment(*measure, at);
        if (start && id_class(*start) == IdClass::LAYOUT) return invalid("layout builds this segment; it cannot be edited");
        const auto track = mu::engraving::staff2track(at.staff, at.voice);
        const Staff *staff = score->staff(at.staff);
        if (staff->is_tab_staff(tick) || staff->is_drum_staff(tick))
            return invalid("EnterNotes enters notes on a pitched staff, not on a tablature or a drum set");

        const bool interior = measure->tick() < tick && tick < measure->end_tick();
        auto *covering = interior && (!start || !start->element(track)) ? rest_covering(*measure, tick, track) : nullptr;
        const bool open_voice = interior && at.voice != 0 && !measure->has_voice(track);
        if (!start && !covering && !open_voice) return std::unexpected(missing_segment(at));
        auto *tuplet = covering ? covering->tuplet() : nullptr;
        if (const auto *item = start ? start->element(track) : nullptr; item && item->is_chord_rest())
            tuplet = mu::engraving::to_chord_rest(item)->tuplet();
        const auto shortest = mu::engraving::TDuration(mu::engraving::DurationType::V_1024TH).fraction();
        if (shortest.denominator() % duration.denominator() != 0)
            return invalid("the duration cannot be written with ordinary note values");
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
            auto left = score->set_rests(begin, track, *leading, true, tuplet);
            if (left.empty()) return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine could not enter the leading rest"});
            // Empty secondary voices normally start with implicit gaps. This split
            // authors the leading silence so normalization retains it as a rest.
            for (auto *rest : left) {
                rest->set_gap(false);
                if (visible.is_valid()) {
                    rest->undo_change_property(mu::engraving::Pid::VISIBLE, visible);
                    rest->undo_change_property(mu::engraving::Pid::COLOR, color);
                }
            }
            auto *right = score->set_rest(tick, track, *trailing, true, tuplet);
            if (!right) return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine could not enter the trailing rest"});
            start = right->segment();
        }
        const NoteVal first = pitches.empty() ? NoteVal() : note_value(pitches.front());
        EngravingItem *last = nullptr;
        score->set_note_rest_at(start, track, first, duration, DirectionV::AUTO, false, {}, false, &last);
        const auto piece_at = [&](const mu::engraving::Fraction &offset) -> ChordRest * {
            auto *segment = score->tick2segment(offset, true, mu::engraving::SegmentType::CHORD_REST, false);
            auto *item = segment ? segment->element(track) : nullptr;
            return item && item->is_chord_rest() ? mu::engraving::to_chord_rest(item) : nullptr;
        };
        auto *piece = piece_at(tick);
        if (!piece || !last) return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine left no chord or rest at this position"});

        // Stop at the last piece produced by note entry, even if there is a pre-existing
        // chord at the following tick.
        std::vector<Note *> previous(pitches.size());
        mu::engraving::Fraction remaining = duration;
        for (;;) {
            context.created(piece);
            remaining -= piece->ticks();
            if (!pitches.empty()) {
                auto *chord = piece->is_chord() ? mu::engraving::to_chord(piece) : nullptr;
                Note *lead = chord ? chord->find_note(first.pitch) : nullptr;
                if (!lead || (previous.front() && (!previous.front()->tie_for() || previous.front()->tie_for()->end_note() != lead)))
                    return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine did not connect the entered chord pieces"});
                context.created(lead);
                previous.front() = lead;
                for (std::size_t i = 1; i < pitches.size(); ++i) {
                    Note *note = mu::engraving::NoteInput::add_note_to_chord(score, chord, note_value(pitches[i]));
                    if (previous[i]) {
                        auto *tie = mu::engraving::Factory::create_tie(previous[i]);
                        tie->set_note_span(previous[i], note);
                        score->undo_add_element(tie);
                    }
                    context.created(note);
                    previous[i] = note;
                }
            }
            if (last == piece || (!previous.empty() && last == previous.front())) break;
            piece = piece_at(piece->end_tick());
            if (!piece || remaining <= mu::engraving::Fraction(0, 1)) break;
        }
        if (!remaining.is_zero() || !piece)
            return std::unexpected(Error{ErrorCode::EDIT_REJECTED, "the engine did not fill the requested duration"});
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
