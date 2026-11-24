#include <algorithm>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engrave/src/api/batch.h"
#include "engrave/src/api/pid_support.h"
#include "engrave/src/api/value_codec.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/durationtype.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/property.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/utils.h"
#include "engraving/dom/volta.h"
#include "engraving/style/style.h"
#include "engraving/style/styledef.h"
#include "engraving/types/fraction.h"
#include "engraving/types/typesconv.h"

namespace engrave::api {
namespace {

using mu::engraving::ChordRest;
using mu::engraving::ElementType;
using mu::engraving::EngravingItem;
using mu::engraving::EngravingObject;
using mu::engraving::Note;
using mu::engraving::Pid;
using mu::engraving::PropertyFlags;
using mu::engraving::PropertyValue;
using mu::engraving::Score;
using mu::engraving::Segment;
using mu::engraving::SegmentType;
using mu::engraving::Spanner;
using mu::engraving::Tie;
using mu::engraving::Tuplet;
using mu::engraving::TupletBracketType;
using mu::engraving::TupletNumberType;

std::unexpected<Error> invalid(std::string message) { return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::move(message)}); }

std::unexpected<Error> rejected(std::string message) { return std::unexpected(Error{ErrorCode::EDIT_REJECTED, std::move(message)}); }

Result<mu::engraving::Measure *> measure_endpoint(const EditContext &context, const Target &target) {
    auto object = context.object(target);
    if (!object) return std::unexpected(object.error());
    if (!(*object)->is_measure()) return invalid(std::format("{}: a measure is required", target.field));
    return mu::engraving::to_measure(*object);
}

Result<SegmentTrack> line_endpoint(const EditContext &context, const Target &target) {
    if (target.is_position()) return context.segment_track(target);
    const auto rest = context.chord_rest_at(target);
    if (!rest) return std::unexpected(rest.error());
    auto *segment = (*rest)->segment();
    if (!segment) return invalid(std::format("{}: the chord or rest must occupy a segment", target.field));
    return SegmentTrack{segment, (*rest)->track()};
}

bool segment_line(ElementType type) {
    switch (type) {
        case ElementType::OTTAVA:
        case ElementType::TEXTLINE:
        case ElementType::TRILL:
        case ElementType::LET_RING:
        case ElementType::GRADUAL_TEMPO_CHANGE:
        case ElementType::VIBRATO:
        case ElementType::PALM_MUTE:
        case ElementType::WHAMMY_BAR:
        case ElementType::RASGUEADO:
        case ElementType::HARMONIC_MARK:
        case ElementType::PICK_SCRAPE:
        case ElementType::PEDAL:
        case ElementType::HAIRPIN:
        case ElementType::PARTIAL_LYRICSLINE: return true;
        default: return false;
    }
}

void add_at_ticks(Spanner *spanner, mu::engraving::track_idx_t track, const mu::engraving::Fraction &start,
                  const mu::engraving::Fraction &end) {
    spanner->set_track(track);
    spanner->set_track2(track);
    spanner->set_tick(start);
    spanner->set_tick2(end);
    spanner->score()->undo_add_element(spanner, true);
}

// The properties of an operation, checked against the catalogue's kinds.
Result<std::vector<std::pair<Pid, PropertyValue>>> properties(const std::vector<std::pair<std::string, Value>> &props) {
    std::vector<std::pair<Pid, PropertyValue>> resolved;
    resolved.reserve(props.size());
    for (const auto &[name, value] : props) {
        const Pid pid = pid_of(name);
        if (pid == Pid::END) return invalid(std::format("no property '{}'", name));
        auto checked = from_value(mu::engraving::property_type(pid), value);
        if (!checked) return std::unexpected(checked.error());
        resolved.emplace_back(pid, *checked);
    }
    return resolved;
}

Result<void> validate_properties(std::string_view type, const std::vector<std::pair<Pid, PropertyValue>> &props) {
    for (const auto &property : props) {
        const Pid pid = property.first;
        const std::optional<bool> listed = settable(type, pid);
        if (!listed) return invalid(std::format("a {} has no property '{}'", type, name_of(pid)));
        if (!*listed) {
            return invalid(std::format("'{}' of a {} is changed by structural operations only", name_of(pid), type));
        }
    }
    return {};
}

// Apply through the attached item so changes also reach its linked copies.
void set_properties(EngravingItem &object, const std::vector<std::pair<Pid, PropertyValue>> &props) {
    for (const auto &[pid, value] : props) {
        const PropertyFlags flags = object.property_flags(pid);
        object.undo_change_property(pid, value, flags == PropertyFlags::STYLED ? PropertyFlags::UNSTYLED : flags);
    }
}

} // namespace

Result<Step> prepare(EditContext &context, const AddSpanner &op) {
    auto start = context.target(op.start, "start", Selector::POSITION);
    if (!start) return std::unexpected(start.error());
    auto end = context.target(op.end, "end", Selector::POSITION);
    if (!end) return std::unexpected(end.error());
    const ElementType type = mu::engraving::TConv::from_xml(muse::AsciiStringView(op.type), ElementType::INVALID);
    if (type != ElementType::SLUR && !segment_line(type)) return invalid(std::format("'{}' is not an addable spanner type", op.type));
    auto props = properties(op.props);
    if (!props) return std::unexpected(props.error());
    if (auto valid = validate_properties(mu::engraving::TConv::to_xml(type).ascii(), *props); !valid) return std::unexpected(valid.error());
    return [start = *start, end = *end, type, props = std::move(*props)](EditContext &context) -> Result<void> {
        Spanner *spanner = nullptr;
        if (type == ElementType::SLUR) {
            auto first = context.chord_rest_at(start);
            if (!first) return std::unexpected(first.error());
            auto last = context.chord_rest_at(end);
            if (!last) return std::unexpected(last.error());
            Score *score = (*first)->score();
            if ((*last)->score() != score) return invalid("a spanner starts and ends in one score");
            if ((*last)->is_before(*first)) return rejected("end: the slur end precedes the start");
            spanner = score->add_slur(*first, *last, nullptr);
            if (!spanner) return rejected("the engine did not make the slur");
        } else {
            auto first = line_endpoint(context, start);
            if (!first) return std::unexpected(first.error());
            auto last = line_endpoint(context, end);
            if (!last) return std::unexpected(last.error());
            Score *score = first->segment->score();
            if (last->segment->score() != score) return invalid("a spanner starts and ends in one score");
            const auto from = first->segment->tick();
            const auto to = last->segment->tick();
            if (to <= from) return rejected("end: the end onset must follow the start");
            std::unique_ptr<EngravingItem> created(mu::engraving::Factory::create_item(type, score->dummy()));
            if (!created || !created->is_spanner() || mu::engraving::to_spanner(created.get())->anchor() != Spanner::Anchor::SEGMENT)
                return invalid("the spanner must be anchored to segments");
            spanner = mu::engraving::to_spanner(created.release());
            // The end operand supplies only a tick. Linked copies use the track delta in their own score.
            add_at_ticks(spanner, first->track, from, to);
        }
        set_properties(*spanner, props);
        context.created(spanner);
        return {};
    };
}

Result<Step> prepare(EditContext &context, const AddSpannerToBarEnd &op) {
    auto start = context.target(op.start, "start", Selector::POSITION);
    if (!start) return std::unexpected(start.error());
    auto end = context.target(op.end.measure, "end", Selector::MEASURE);
    if (!end) return std::unexpected(end.error());
    if (end->object && !end->object->is_measure()) return invalid("end: a measure is required");
    const ElementType type = mu::engraving::TConv::from_xml(muse::AsciiStringView(op.type), ElementType::INVALID);
    if (!segment_line(type)) return invalid(std::format("'{}' is not an addable segment spanner type", op.type));
    auto props = properties(op.props);
    if (!props) return std::unexpected(props.error());
    if (auto valid = validate_properties(mu::engraving::TConv::to_xml(type).ascii(), *props); !valid) return std::unexpected(valid.error());
    return [start = *start, end = *end, type, props = std::move(*props)](EditContext &context) -> Result<void> {
        auto first = line_endpoint(context, start);
        if (!first) return std::unexpected(first.error());
        auto measure = measure_endpoint(context, end);
        if (!measure) return std::unexpected(measure.error());
        Score *score = first->segment->score();
        if ((*measure)->score() != score) return invalid("a spanner starts and ends in one score");
        const auto from = first->segment->tick();
        const auto to = (*measure)->end_tick();
        if (to <= from) return invalid("end: the barline must follow the start of the spanner");

        std::unique_ptr<EngravingItem> item(mu::engraving::Factory::create_item(type, score->dummy()));
        if (!item || !item->is_spanner() || mu::engraving::to_spanner(item.get())->anchor() != Spanner::Anchor::SEGMENT) {
            return invalid("a bar-end spanner must be anchored to segments");
        }
        auto *spanner = mu::engraving::to_spanner(item.release());
        add_at_ticks(spanner, first->track, from, to);
        set_properties(*spanner, props);
        context.created(spanner);
        return {};
    };
}

Result<Step> prepare(EditContext &context, const AddVolta &op) {
    auto first = context.target(op.first_measure, "first_measure", Selector::MEASURE);
    if (!first) return std::unexpected(first.error());
    auto last = context.target(op.last_measure, "last_measure", Selector::MEASURE);
    if (!last) return std::unexpected(last.error());
    if ((first->object && !first->object->is_measure()) || (last->object && !last->object->is_measure()))
        return invalid("a volta spans measures");
    auto endings = op.endings;
    std::ranges::sort(endings);
    if (endings.empty() || endings.front() < 1 || std::ranges::adjacent_find(endings) != endings.end())
        return invalid("volta endings must be distinct positive pass numbers");
    std::string text;
    for (int ending : endings) {
        if (!text.empty()) text += ", ";
        text += std::format("{}.", ending);
    }
    return [first = *first, last = *last, endings = std::move(endings), text = std::move(text),
            closed = op.closed](EditContext &context) -> Result<void> {
        auto start = measure_endpoint(context, first);
        if (!start) return std::unexpected(start.error());
        auto end = measure_endpoint(context, last);
        if (!end) return std::unexpected(end.error());
        Score *score = (*start)->score();
        if ((*end)->score() != score) return invalid("a volta begins and ends in one score");
        if ((*end)->tick() < (*start)->tick()) return invalid("the last measure precedes the first");
        // Playback closes a volta at either repeat barline within its span.
        for (auto *measure = *start;; measure = measure->next_measure()) {
            if ((measure != *start && measure->repeat_start()) || (measure != *end && measure->repeat_end()))
                return invalid("a volta cannot cross a repeat barline");
            if (measure == *end) break;
        }
        auto *volta = mu::engraving::Factory::create_volta(score->dummy());
        volta->set_endings(endings);
        volta->set_text(muse::String::from_std_string(text));
        volta->set_volta_type(closed ? mu::engraving::Volta::Type::CLOSED : mu::engraving::Volta::Type::OPEN);
        add_at_ticks(volta, 0, (*start)->tick(), (*end)->end_tick());
        score->invalidate_repeat_list();
        context.created(volta);
        return {};
    };
}

Result<Step> prepare(EditContext &context, const AddTie &op) {
    auto target = context.target(op.note, "note");
    if (!target) return std::unexpected(target.error());
    std::optional<Target> end_target;
    if (op.end_note) {
        auto end = context.target(*op.end_note, "end_note");
        if (!end) return std::unexpected(end.error());
        end_target = *end;
    }
    return [target = *target, end_target](EditContext &context) -> Result<void> {
        auto object = context.object(target);
        if (!object) return std::unexpected(object.error());
        if (!(*object)->is_note()) return invalid(std::format("{} is not a note", (*object)->type_name()));
        Note *note = mu::engraving::to_note(*object);
        Note *end = nullptr;
        if (end_target) {
            auto destination = context.object(*end_target);
            if (!destination) return std::unexpected(destination.error());
            if (!(*destination)->is_note()) return invalid("the tie destination must be a note");
            end = mu::engraving::to_note(*destination);
            const auto matches = [note](const Note *other) {
                return other && other->score() == note->score() && other->track() == note->track() && other->pitch() == note->pitch() &&
                       other->chord() && !other->chord()->is_grace();
            };
            if (!matches(end) || !note->chord() || note->chord()->is_grace())
                return rejected("the tie requires principal notes of the same pitch and track");
            // Rhythmic entry may have written a held event in several tied pieces.
            while (note->tie_for() && note->chord()->end_tick() < end->tick()) {
                auto *continuation = note->tie_for()->end_note();
                if (!matches(continuation) || continuation->tick() != note->chord()->end_tick()) break;
                note = continuation;
            }
            if (note->tick() >= end->tick() || note->chord()->end_tick() != end->tick())
                return rejected("the tie destination must be contiguous with the source");
        }
        if (note->tie_for()) return rejected("the note already has a tie");
        Score *score = note->score();
        score->master_score()->update_repeat_list(false);
        if (!end_target) end = mu::engraving::search_tie_note(note);
        if (!end) return rejected("no note of the same pitch follows this one");
        if (end->tie_back()) return rejected("the note to tie to already has a tie");

        // What EditTie's createAndAddTie makes of a tie to a note that is free: the
        // engine sets both notes' tie pointers when Score::addElement adds the tie.
        Tie *tie = mu::engraving::Factory::create_tie(note);
        tie->set_start_note(note);
        tie->set_track(note->track());
        tie->set_tick(note->chord()->segment()->tick());
        tie->set_end_note(end);
        tie->set_ticks(end->chord()->segment()->tick() - note->chord()->segment()->tick());
        score->undo_add_element(tie);
        tie->add_ties_to_jump_points();
        context.created(tie);
        return {};
    };
}

Result<Step> prepare(EditContext &context, const AddTuplet &op) {
    if (op.actual <= 0 || op.normal <= 0) return invalid(std::format("{}:{} is not a tuplet ratio", op.actual, op.normal));
    auto first = context.target(op.first, "first");
    if (!first) return std::unexpected(first.error());
    auto last = context.target(op.last, "last");
    if (!last) return std::unexpected(last.error());
    return [first = *first, last = *last, actual = op.actual, normal = op.normal](EditContext &context) -> Result<void> {
        auto first_rest = context.chord_rest_at(first);
        if (!first_rest) return std::unexpected(first_rest.error());
        auto last_rest = context.chord_rest_at(last);
        if (!last_rest) return std::unexpected(last_rest.error());
        Score *score = (*first_rest)->score();
        const mu::engraving::track_idx_t track = (*first_rest)->track();
        if ((*last_rest)->score() != score || (*last_rest)->track() != track) {
            return invalid("a tuplet covers the chords and rests of one voice");
        }
        const mu::engraving::Fraction from = (*first_rest)->tick();
        const mu::engraving::Fraction to = (*last_rest)->tick();
        if (to < from) return invalid("the range ends before it starts");

        // The ticks are collected first: making a tuplet replaces the chord rest it
        // starts with, and its rests are not to be tupleted again.
        std::vector<mu::engraving::Fraction> ticks;
        for (Segment *segment = (*first_rest)->segment(); segment && segment->tick() <= to;
             segment = segment->next1(SegmentType::CHORD_REST)) {
            const EngravingItem *element = segment->element(track);
            if (element && element->is_chord_rest()) ticks.push_back(segment->tick());
        }
        if (ticks.empty()) return invalid("there is no chord or rest to divide");

        // The tuplet is what the score's style makes of one; Score::addTuplet keeps
        // the number and bracket it is given, and marks them styled when they are the
        // style's.
        const mu::engraving::Fraction ratio(actual, normal);
        const TupletNumberType number_type = static_cast<TupletNumberType>(score->style().style_i(mu::engraving::Sid::TUPLET_NUMBER_TYPE));
        const TupletBracketType bracket_type =
            static_cast<TupletBracketType>(score->style().style_i(mu::engraving::Sid::TUPLET_BRACKET_TYPE));
        for (const mu::engraving::Fraction &tick : ticks) {
            ChordRest *rest = score->find_cr(tick, track);
            if (!rest) return rejected(std::format("the chord rest at tick {} went away", tick.to_string().to_std_string()));
            // Score::addTuplet's own ratio check: the base length is the chord rest's
            // reduced duration divided by the normal count.
            const mu::engraving::Fraction base = rest->ticks().reduced() * mu::engraving::Fraction(1, normal);
            if ((rest->duration_type() < mu::engraving::TDuration(mu::engraving::DurationType::V_512TH) &&
                 rest->duration_type() != mu::engraving::TDuration(mu::engraving::DurationType::V_MEASURE)) ||
                !mu::engraving::TDuration::is_valid(base) || !score->can_add_tuplet(rest, ratio, mu::engraving::TDuration(base))) {
                return rejected(std::format("a {}:{} tuplet does not fit the {} at tick {}", actual, normal, rest->type_name(),
                                            tick.to_string().to_std_string()));
            }
        }
        for (const mu::engraving::Fraction &tick : ticks) {
            ChordRest *rest = score->find_cr(tick, track);
            if (!rest) return rejected("a checked tuplet target disappeared");
            Tuplet *tuplet = score->add_tuplet(rest, ratio, number_type, bracket_type);
            if (!tuplet) return rejected("the engine did not make the tuplet");
            context.created(tuplet);
        }
        return {};
    };
}

} // namespace engrave::api
