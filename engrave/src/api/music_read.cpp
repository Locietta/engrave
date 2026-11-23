#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/engravingproject.h"
#include "global/hashmap.h"
#include <engrave/music.h>

namespace engrave {
namespace {

struct ElementIdHash {
    using is_avalanching = void;

    std::uint64_t operator()(const ElementId &id) const noexcept {
        const auto h1 = ankerl::unordered_dense::hash<std::uint64_t>{}(id.hi);
        const auto h2 = ankerl::unordered_dense::hash<std::uint64_t>{}(id.lo);
        return ankerl::unordered_dense::hash<std::uint64_t>{}(h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)));
    }
};

using ElementIdSet = muse::HashSet<ElementId, ElementIdHash>;

using Tick = mu::engraving::Fraction;
using mu::engraving::Chord;
using mu::engraving::ChordRest;
using mu::engraving::EngravingObject;
using mu::engraving::Measure;
using mu::engraving::Note;
using mu::engraving::NoteType;
using mu::engraving::Score;
using mu::engraving::Spanner;
using mu::engraving::Staff;
using mu::engraving::Tie;

constexpr int max_measures = 128;
constexpr std::size_t max_rows = 32768;
constexpr std::size_t max_text_bytes = 8 * 1024 * 1024;
constexpr std::size_t max_scan_work = 8 * 1024 * 1024;

struct ReadBudget {
    std::size_t rows = max_rows;
    std::size_t text_bytes = max_text_bytes;
};

std::unexpected<Error> music_limit() {
    return std::unexpected(Error{ErrorCode::RESOURCE_LIMIT, "too much music in one window; narrow the measure, staff or voice range"});
}

bool charge(std::size_t &remaining, std::size_t cost) {
    if (cost > remaining) return false;
    remaining -= cost;
    return true;
}

bool charge_text(ReadBudget &budget, std::string_view text) { return charge(budget.text_bytes, text.size()); }

Fraction written(Tick value) { return {value.numerator64(), value.denominator64()}; }

Fraction reduced(Tick value) {
    value.reduce();
    return written(value);
}

ElementId music_id(const EngravingObject &object) { return from_engine(object.eid()); }

std::optional<ElementId> optional_id(const EngravingObject *object) {
    if (object && object->eid().is_valid()) return music_id(*object);
    return std::nullopt;
}

SpelledPitch spelling(int pitch, int tpc) {
    if (!mu::engraving::tpc_is_valid(tpc)) {
        tpc = mu::engraving::pitch2tpc(pitch, mu::engraving::Key::C, mu::engraving::Prefer::NEAREST);
    }
    return {static_cast<StepName>(mu::engraving::tpc2step(tpc)), static_cast<int>(mu::engraving::tpc2alter(tpc)),
            (pitch - mu::engraving::tpc2pitch(tpc)) / mu::engraving::pitch_delta_octave - 1};
}

std::optional<TieLink> tie_link(const Tie *tie, bool incoming) {
    const auto id = optional_id(tie);
    if (!id) return std::nullopt;
    return TieLink{*id, optional_id(incoming ? tie->start_note() : tie->end_note())};
}

Result<std::vector<NoteMusic>> read_notes(const Chord &chord, const Staff &staff, Tick tick, ReadBudget &budget) {
    if (!charge(budget.rows, chord.notes().size())) return music_limit();
    const bool drums = staff.is_drum_staff(tick);
    const auto *kit = drums ? staff.origin_part(tick)->instrument(tick)->drumset() : nullptr;
    const auto transpose = staff.transpose(tick);
    std::vector<NoteMusic> notes;
    notes.reserve(chord.notes().size());
    for (const auto *note : chord.notes()) {
        NoteMusic row{.id = music_id(*note), .incoming = tie_link(note->tie_back(), true), .outgoing = tie_link(note->tie_for(), false)};
        const int pitch = note->pitch();
        if (drums) {
            DrumNote drum{pitch};
            if (kit && mu::engraving::pitch_is_valid(pitch) && kit->is_valid(pitch)) {
                std::string name = kit->name(pitch).to_std_string();
                if (!charge_text(budget, name)) return music_limit();
                drum.kit_name = std::move(name);
            }
            row.pitch = std::move(drum);
        } else {
            row.pitch = PitchedNote{pitch, spelling(pitch, note->tpc1()), spelling(pitch - transpose.chromatic, note->tpc2())};
        }
        notes.push_back(std::move(row));
    }
    return notes;
}

GraceKind grace_kind(NoteType kind) {
    switch (kind) {
        case NoteType::APPOGGIATURA: return GraceKind::Appoggiatura;
        case NoteType::ACCIACCATURA: return GraceKind::Acciaccatura;
        case NoteType::GRACE4: return GraceKind::Grace4;
        case NoteType::GRACE8_AFTER: return GraceKind::Grace8After;
        case NoteType::GRACE16: return GraceKind::Grace16;
        case NoteType::GRACE16_AFTER: return GraceKind::Grace16After;
        case NoteType::GRACE32: return GraceKind::Grace32;
        case NoteType::GRACE32_AFTER: return GraceKind::Grace32After;
        case NoteType::NORMAL:
        case NoteType::INVALID: break;
    }
    check(false, "grace chord has an invalid note type");
    return GraceKind::Appoggiatura;
}

Result<std::vector<GraceMusic>> read_graces(const mu::engraving::GraceNotesView &graces, const Staff &staff, Tick tick,
                                            ReadBudget &budget) {
    std::vector<GraceMusic> result;
    for (const auto *grace : graces) {
        if (!charge(budget.rows, 1)) return music_limit();
        auto notes = read_notes(*grace, staff, tick, budget);
        if (!notes) return std::unexpected(std::move(notes.error()));
        result.push_back({music_id(*grace), grace_kind(grace->note_type()), reduced(grace->ticks()), std::move(*notes)});
    }
    return result;
}

Result<MusicEvent> read_event(const ChordRest &source, const Staff &staff, const Measure &measure, MusicWindow &window,
                              ElementIdSet &seen_tuplets, ReadBudget &budget) {
    if (!charge(budget.rows, 1)) return music_limit();
    const auto tick = source.tick();
    MusicEvent event{music_id(source), reduced(tick - measure.tick()), reduced(source.ticks()), reduced(source.actual_ticks())};
    for (auto *tuplet = source.tuplet(); tuplet; tuplet = tuplet->tuplet()) {
        if (!charge(budget.rows, 1)) return music_limit();
        const auto id = music_id(*tuplet);
        event.tuplet_path.push_back(id);
        if (seen_tuplets.insert(id).second) {
            const auto ratio = tuplet->ratio();
            window.tuplets.push_back({id, ratio.numerator(), ratio.denominator(), reduced(tuplet->base_len().fraction())});
        }
    }
    std::ranges::reverse(event.tuplet_path);
    if (source.is_measure_repeat()) {
        event.body = RestMusic{false, static_cast<const mu::engraving::MeasureRepeat &>(source).num_measures()};
        return event;
    }
    if (source.is_rest()) {
        event.body = RestMusic{source.is_full_measure_rest()};
        return event;
    }
    const auto &chord = static_cast<const Chord &>(source);
    auto notes = read_notes(chord, staff, tick, budget);
    if (!notes) return std::unexpected(std::move(notes.error()));
    auto before = read_graces(chord.grace_notes_before(), staff, tick, budget);
    if (!before) return std::unexpected(std::move(before.error()));
    auto after = read_graces(chord.grace_notes_after(), staff, tick, budget);
    if (!after) return std::unexpected(std::move(after.error()));
    event.body = ChordMusic{std::move(*notes), std::move(*before), std::move(*after)};
    return event;
}

Result<void> read_signatures(const mu::engraving::Segment &segment, const std::vector<int> &staves, MeasureMusic &measure,
                             ReadBudget &budget) {
    const auto tick = reduced(segment.tick());
    for (int staff : staves) {
        const auto *item = segment.element(mu::engraving::staff2track(staff));
        if (!item || item->generated()) continue;
        if (!item->is_time_sig() && !item->is_key_sig()) continue;
        if (!charge(budget.rows, 1)) return music_limit();
        if (item->is_time_sig()) {
            measure.signature_changes.push_back({"TimeSig", tick, staff, written(mu::engraving::to_time_sig(item)->sig())});
        } else {
            measure.signature_changes.push_back(
                {"KeySig", tick, staff, {}, static_cast<int>(mu::engraving::to_key_sig(item)->concert_key())});
        }
    }
    for (const auto *item : segment.annotations()) {
        if (!item || item->generated() || !item->is_tempo_text()) continue;
        if (!charge(budget.rows, 1)) return music_limit();
        const auto *tempo = mu::engraving::to_tempo_text(item);
        std::string text = tempo->plain_text().to_std_string();
        if (!charge_text(budget, text)) return music_limit();
        measure.signature_changes.push_back({"Tempo", tick, -1, {}, 0, std::round(tempo->tempo().val * 60.0 * 1e4) / 1e4, std::move(text)});
    }
    return {};
}

Result<std::vector<int>> selection(std::vector<int> indices, int count, std::string_view label) {
    if (indices.empty()) {
        if (count > static_cast<int>(max_rows)) return music_limit();
        for (int index = 0; index < count; ++index) indices.push_back(index);
    } else {
        std::ranges::sort(indices);
        indices.erase(std::ranges::unique(indices).begin(), indices.end());
        if (indices.front() < 0 || indices.back() >= count)
            return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, std::string(label) + " selection is outside the score"});
    }
    return indices;
}

Position span_position(const Measure &measure, Tick tick, mu::engraving::track_idx_t track) {
    const int staff = track == muse::nidx ? -1 : static_cast<int>(mu::engraving::track2staff(track));
    const int voice = track == muse::nidx ? -1 : static_cast<int>(mu::engraving::track2voice(track));
    return {music_id(measure), reduced(tick - measure.tick()), staff, voice};
}

const Measure *source_measure(const Score &score, Tick tick) {
    auto *measure = score.tick2measure(tick);
    while (measure && tick < measure->tick()) measure = measure->prev_measure();
    if (measure && tick <= measure->end_tick()) return measure;
    return nullptr;
}

std::optional<ElementId> source_anchor(const Spanner &span, const Measure &measure, Tick tick, bool end) {
    const auto *attached = end ? span.end_element() : span.start_element();
    if (span.anchor() == Spanner::Anchor::NOTE) return optional_id(attached);
    if (span.anchor() == Spanner::Anchor::MEASURE) return music_id(measure);
    if (span.system_flag()) return std::nullopt;
    // The track and tick cannot distinguish a grace anchor from its principal.
    if (span.anchor() == Spanner::Anchor::CHORDREST && attached && attached->is_chord() && mu::engraving::to_chord(attached)->is_grace() &&
        attached->score() == measure.score() && attached->tick() == tick)
        return optional_id(attached);
    const auto track = end ? span.effective_track2() : span.track();
    if (track == muse::nidx) return std::nullopt;
    const auto *segment = measure.find_segment(mu::engraving::SegmentType::CHORD_REST, tick);
    if (!segment) return std::nullopt;
    if (const auto id = optional_id(segment->element(track))) return id;
    if (span.anchor() == Spanner::Anchor::SEGMENT && !span.is_voice_specific())
        return optional_id(segment->first_element(mu::engraving::track2staff(track)));
    return std::nullopt;
}

bool span_selected(const Spanner &source, const std::vector<int> &staves, const std::vector<int> &voices) {
    if (source.system_flag() || source.is_volta()) return true;
    if (source.has_voice_assignment_properties()) {
        for (int staff : staves) {
            if (std::ranges::any_of(voices,
                                    [&](int voice) { return source.element_applies_to_track(mu::engraving::staff2track(staff, voice)); }))
                return true;
        }
        return false;
    }
    const bool staff_wide = source.anchor() == Spanner::Anchor::SEGMENT && !source.is_voice_specific();
    auto matches = [&](mu::engraving::track_idx_t track) {
        return track == muse::nidx ||
               (std::ranges::binary_search(staves, static_cast<int>(mu::engraving::track2staff(track))) &&
                (staff_wide || std::ranges::binary_search(voices, static_cast<int>(mu::engraving::track2voice(track)))));
    };
    return matches(source.track()) || matches(source.effective_track2());
}

Result<void> read_spans(const Score &score, Tick begin, Tick end, const std::vector<int> &staves, const std::vector<int> &voices,
                        MusicWindow &window, ReadBudget &budget, std::size_t &work) {
    ElementIdSet seen;
    auto append = [&](const Spanner &source) -> Result<void> {
        if (!charge(work, 1)) return music_limit();
        const auto start_tick = source.tick();
        const auto end_tick = source.tick2();
        const auto anchor = source.anchor();
        const bool includes_end = anchor == Spanner::Anchor::CHORDREST || anchor == Spanner::Anchor::NOTE;
        if (start_tick >= end || end_tick < begin || (end_tick == begin && start_tick < begin && !includes_end)) return {};
        const auto id = optional_id(&source);
        if (!id || seen.contains(*id)) return {};
        if (source.has_voice_assignment_properties() && !charge(work, staves.size() * voices.size())) return music_limit();
        if (!span_selected(source, staves, voices)) return {};
        const auto *start = source_measure(score, start_tick);
        const auto *finish = source_measure(score, end_tick);
        if (finish && anchor == Spanner::Anchor::MEASURE && end_tick == finish->tick() && end_tick > start_tick)
            finish = finish->prev_measure();
        if (!finish && end_tick == score.last_measure()->end_tick()) finish = score.last_measure();
        if (!start || !finish) return {};
        if (!charge(budget.rows, 1)) return music_limit();
        std::string type_name = source.type_name();
        if (!charge_text(budget, type_name)) return music_limit();
        std::optional<std::string> subtype;
        if (source.is_hairpin()) {
            using mu::engraving::HairpinType;
            switch (mu::engraving::to_hairpin(&source)->hairpin_type()) {
                case HairpinType::CRESC_HAIRPIN: subtype = "crescendo"; break;
                case HairpinType::DIM_HAIRPIN: subtype = "diminuendo"; break;
                case HairpinType::CRESC_LINE: subtype = "crescendo_line"; break;
                case HairpinType::DIM_LINE: subtype = "diminuendo_line"; break;
                case HairpinType::INVALID: break;
            }
            if (subtype && !charge_text(budget, *subtype)) return music_limit();
        }
        seen.insert(*id);
        SpanMusic span{*id,
                       std::move(type_name),
                       std::move(subtype),
                       span_position(*start, start_tick, source.track()),
                       span_position(*finish, end_tick, source.effective_track2()),
                       source_anchor(source, *start, start_tick, false),
                       source_anchor(source, *finish, end_tick, true)};
        if (source.is_volta()) {
            const auto &volta = static_cast<const mu::engraving::Volta &>(source);
            if (!charge(budget.rows, volta.endings().size())) return music_limit();
            window.voltas.push_back({std::move(span), volta.endings(), volta.volta_type() == mu::engraving::Volta::Type::CLOSED});
        } else {
            window.spanners.push_back(std::move(span));
        }
        return {};
    };
    for (const auto &[tick, spanner] : score.spanner()) {
        if (auto result = append(*spanner); !result) return result;
    }
    for (const auto *spanner : score.unmanaged_spanners()) {
        if (auto result = append(*spanner); !result) return result;
    }
    // Note-owned lines can start before the window and end after it.
    auto note_spans = [&](const Chord &chord, bool in_window) -> Result<void> {
        if (!charge(work, 1 + chord.notes().size())) return music_limit();
        for (const auto *note : chord.notes()) {
            for (const auto *span : note->spanner_for()) {
                if (auto result = append(*span); !result) return result;
            }
            // A partial note line in an excerpt may only have its ending note.
            if (in_window) {
                for (const auto *span : note->spanner_back()) {
                    if (auto result = append(*span); !result) return result;
                }
            }
        }
        return {};
    };
    for (auto *measure = score.first_measure(); measure && measure->tick() < end; measure = measure->next_measure()) {
        if (!charge(work, 1)) return music_limit();
        for (auto *segment = measure->first(mu::engraving::SegmentType::CHORD_REST); segment && segment->tick() < end;
             segment = segment->next(mu::engraving::SegmentType::CHORD_REST)) {
            if (!charge(work, 1 + segment->elist().size())) return music_limit();
            for (const auto *item : segment->elist()) {
                if (!item || !item->is_chord()) continue;
                const auto &chord = *mu::engraving::to_chord(item);
                const bool in_window = chord.tick() >= begin;
                if (auto result = note_spans(chord, in_window); !result) return result;
                for (const auto *grace : chord.grace_notes()) {
                    if (auto result = note_spans(*grace, in_window); !result) return result;
                }
            }
        }
    }
    return {};
}

} // namespace

Result<MusicWindow> Document::read_music(const MusicRange &range) const {
    impl_->engine->check_thread();
    auto selected = select_score(*impl_->project->master_score(), range.score);
    if (!selected) return std::unexpected(std::move(selected.error()));
    const auto &score = **selected;
    if (range.first_measure < 0 || range.measure_count <= 0)
        return std::unexpected(
            Error{ErrorCode::INVALID_ARGUMENT, "the music window needs a nonnegative measure index and a positive count"});
    if (range.measure_count > max_measures) return music_limit();
    auto staves = selection(range.staves, static_cast<int>(score.nstaves()), "staff");
    if (!staves) return std::unexpected(std::move(staves.error()));
    auto voices = selection(range.voices, mu::engraving::voices, "voice");
    if (!voices) return std::unexpected(std::move(voices.error()));

    const auto *measure = score.first_measure();
    for (int index = 0; measure && index < range.first_measure; ++index) measure = measure->next_measure();
    if (!measure) return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, "the music window starts past the end of the score"});

    MusicWindow window{.score = range.score, .revision = impl_->revision};
    const auto begin = measure->tick();
    auto end = begin;
    ReadBudget budget;
    auto work = max_scan_work;
    if (!charge(budget.rows, staves->size())) return music_limit();
    for (int staff_index : *staves) {
        const auto &staff = *score.staff(staff_index);
        const auto *meter = staff.time_sig(begin);
        const auto transpose = staff.transpose(begin);
        const auto key = staff.concert_key(begin);
        std::string part_name = staff.part()->part_name().to_std_string();
        std::string instrument_id = staff.origin_part(begin)->instrument_id(begin).to_std_string();
        if (!charge_text(budget, part_name) || !charge_text(budget, instrument_id)) return music_limit();
        window.staves.push_back({staff_index,
                                 std::move(part_name),
                                 std::move(instrument_id),
                                 written(meter ? meter->sig() : measure->timesig()),
                                 key == mu::engraving::Key::INVALID ? 0 : static_cast<int>(key),
                                 {transpose.diatonic, transpose.chromatic},
                                 staff.is_drum_staff(begin)});
    }

    ElementIdSet seen_tuplets;
    for (int index = 0; index < range.measure_count; ++index, measure = measure->next_measure()) {
        if (!measure) return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, "the music window ends past the end of the score"});
        MeasureMusic bar{{music_id(*measure), range.first_measure + index, reduced(measure->tick()), reduced(measure->ticks())},
                         measure->repeat_start(),
                         measure->repeat_end(),
                         measure->repeat_end() ? measure->repeat_count() : 2};
        for (const auto *segment = measure->first(); segment; segment = segment->next()) {
            if (!charge(work, 1 + staves->size() + segment->annotations().size())) return music_limit();
            if (auto result = read_signatures(*segment, *staves, bar, budget); !result) return std::unexpected(std::move(result.error()));
        }
        for (int staff : *staves) {
            for (int voice : *voices) {
                VoiceEvents lane{staff, voice};
                const auto track = mu::engraving::staff2track(staff, voice);
                for (const auto *segment = measure->first(mu::engraving::SegmentType::CHORD_REST); segment;
                     segment = segment->next(mu::engraving::SegmentType::CHORD_REST)) {
                    if (!charge(work, 1)) return music_limit();
                    const auto *item = segment->element(track);
                    if (!item || (!item->is_chord() && !item->is_rest() && !item->is_measure_repeat())) continue;
                    if (item->is_rest() && mu::engraving::to_rest(item)->is_gap()) continue;
                    auto event =
                        read_event(*mu::engraving::to_chord_rest(item), *score.staff(staff), *measure, window, seen_tuplets, budget);
                    if (!event) return std::unexpected(std::move(event.error()));
                    lane.events.push_back(std::move(*event));
                }
                if (!lane.events.empty()) bar.voices.push_back(std::move(lane));
            }
        }
        window.measures.push_back(std::move(bar));
        end = measure->end_tick();
    }
    if (auto result = read_spans(score, begin, end, *staves, *voices, window, budget, work); !result)
        return std::unexpected(std::move(result.error()));
    return window;
}

} // namespace engrave
