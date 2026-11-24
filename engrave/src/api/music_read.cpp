#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/state.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/dummyparent.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/trill.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/engravingproject.h"
#include "engraving/types/symnames.h"
#include "engraving/types/typesconv.h"
#include "global/hashmap.h"
#include <engrave/music.h>

namespace engrave {
namespace {

struct ElementIdHash {
    using is_avalanching = void;

    std::uint64_t operator()(const ElementId &id) const noexcept {
        const auto h1 = muse::Hash<std::uint64_t>{}(id.hi);
        const auto h2 = muse::Hash<std::uint64_t>{}(id.lo);
        return muse::Hash<std::uint64_t>{}(h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)));
    }
};

using ElementIdSet = muse::HashSet<ElementId, ElementIdHash>;
using MeasureIndices = muse::HashMap<const mu::engraving::Measure *, std::int32_t>;

using Tick = mu::engraving::Fraction;
using mu::engraving::Chord;
using mu::engraving::ChordRest;
using mu::engraving::EngravingItem;
using mu::engraving::EngravingObject;
using mu::engraving::Measure;
using mu::engraving::Note;
using mu::engraving::NoteType;
using mu::engraving::Score;
using mu::engraving::Segment;
using mu::engraving::Spanner;
using mu::engraving::Staff;
using mu::engraving::TConv;
using mu::engraving::TextBase;
using mu::engraving::Tie;
using mu::engraving::VoiceAssignment;

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

std::optional<ElementId> persisted_id(const EngravingObject &object) {
    if (object.eid().is_valid() && takes_id(object)) return music_id(object);
    return std::nullopt;
}

std::string token(muse::AsciiStringView value) { return std::string(std::string_view(value)); }

Position span_position(const Measure &measure, Tick tick, mu::engraving::track_idx_t track, const MeasureIndices &indices) {
    const auto found = indices.find(&measure);
    const int staff = track == muse::nidx ? -1 : static_cast<int>(mu::engraving::track2staff(track));
    const int voice = track == muse::nidx ? -1 : static_cast<int>(mu::engraving::track2voice(track));
    return {music_id(measure), reduced(tick - measure.tick()), staff, voice, found == indices.end() ? -1 : found->second};
}

std::array<std::string_view, 2> mark_text(const MarkPayload &payload) {
    return std::visit(
        [](const auto &mark) -> std::array<std::string_view, 2> {
            using Mark = std::remove_cvref_t<decltype(mark)>;
            if constexpr (std::is_same_v<Mark, DynamicMark>) {
                return {mark.value, mark.custom_text ? std::string_view(*mark.custom_text) : std::string_view{}};
            } else if constexpr (std::is_same_v<Mark, LyricMark>) {
                return {mark.text, mark.syllabic};
            } else if constexpr (std::is_same_v<Mark, HarmonyMark> || std::is_same_v<Mark, TextMark>) {
                return {mark.text};
            } else if constexpr (std::is_same_v<Mark, FermataMark> || std::is_same_v<Mark, ArticulationMark>) {
                return {mark.symbol};
            } else if constexpr (std::is_same_v<Mark, ArpeggioMark> || std::is_same_v<Mark, TremoloMark> ||
                                 std::is_same_v<Mark, BarlineMark>) {
                return {mark.subtype};
            } else if constexpr (std::is_same_v<Mark, ClefMark>) {
                return {mark.clef};
            } else {
                static_assert(false, "handle text for every MarkPayload alternative");
            }
        },
        payload);
}

bool mark_before(const MarkMusic &a, const MarkMusic &b) {
    if (a.offset != b.offset) return a.offset < b.offset;
    const auto key = [](const MarkMusic &mark) {
        const auto *text = std::get_if<TextMark>(&mark.payload);
        return std::tuple{mark.scope, mark.staff, mark.voice, mark.payload.index(), text ? text->kind : TextMark::Kind::EXPRESSION,
                          mark.id};
    };
    if (key(a) != key(b)) return key(a) < key(b);
    return mark_text(a.payload) < mark_text(b.payload);
}

using LinkedMarkKey =
    std::tuple<const mu::engraving::LinkedObjects *, mu::engraving::ElementType, const mu::engraving::Part *, std::int64_t, std::int64_t>;
using LinkedMarkSet = muse::HashSet<LinkedMarkKey>;

struct MarkRead {
    MeasureMusic &bar;
    const std::vector<int> &staves;
    const std::vector<int> &voices;
    const MeasureIndices &measure_indices;
    LinkedMarkSet &seen_links;
    ReadBudget &budget;
    std::size_t &work;
};

Result<void> append_mark(const EngravingItem &item, MarkMusic mark, MarkRead &read) {
    mark.id = persisted_id(item);
    if (!charge(read.budget.rows, 1)) return music_limit();
    for (auto text : mark_text(mark.payload)) {
        if (!charge_text(read.budget, text)) return music_limit();
    }
    read.bar.markings.push_back(std::move(mark));
    return {};
}

std::string symbol_token(mu::engraving::SymId symbol) { return token(mu::engraving::SymNames::name_for_sym_id(symbol)); }

bool applies_to_selection(const EngravingItem &item, const std::vector<int> &staves, const std::vector<int> &voices) {
    return std::ranges::any_of(staves, [&](int staff) {
        return std::ranges::any_of(voices,
                                   [&](int voice) { return item.element_applies_to_track(mu::engraving::staff2track(staff, voice)); });
    });
}

Result<bool> first_linked_annotation(const EngravingItem &item, MarkScope scope, MarkRead &read) {
    // Staff and voice rows describe the individual source objects and their own anchors.
    if (scope != MarkScope::SYSTEM && scope != MarkScope::INSTRUMENT) return true;
    const auto *links = item.links();
    if (!links) return true;
    // Count source text once per examined copy, even when only one copy can emit text.
    if (item.is_text_base() && !charge(read.work, mu::engraving::to_text_base(&item)->xml_text().size())) return music_limit();
    const auto tick = reduced(item.tick());
    const auto *part = scope == MarkScope::INSTRUMENT ? item.part() : nullptr;
    return read.seen_links.emplace(links, item.type(), part, tick.num, tick.den).second;
}

Result<void> read_annotation(const EngravingItem &item, Fraction offset, MarkRead &read) {
    using mu::engraving::ElementType;
    const auto type = item.type();
    if (type == ElementType::FRET_DIAGRAM) {
        const auto *harmony = mu::engraving::to_fret_diagram(&item)->harmony();
        if (!harmony) return {};
        if (!charge(read.work, 1)) return music_limit();
        if (harmony->generated()) return {};
        return read_annotation(*harmony, offset, read);
    }
    const bool system = item.system_flag() || type == ElementType::SYSTEM_TEXT || type == ElementType::REHEARSAL_MARK;
    MarkMusic mark{.offset = offset, .scope = system ? MarkScope::SYSTEM : MarkScope::STAFF};
    if (system) {
        if (read.staves.empty()) return {};
    } else {
        if (item.track() == muse::nidx) return {};
        mark.staff = static_cast<int>(item.staff_idx());
        if (type == ElementType::DYNAMIC || type == ElementType::EXPRESSION) {
            if (!charge(read.work, read.staves.size() * read.voices.size())) return music_limit();
            if (!applies_to_selection(item, read.staves, read.voices)) return {};
            switch (static_cast<const TextBase &>(item).voice_assignment()) {
                case VoiceAssignment::ALL_VOICE_IN_INSTRUMENT: mark.scope = MarkScope::INSTRUMENT; break;
                case VoiceAssignment::ALL_VOICE_IN_STAFF: break;
                case VoiceAssignment::CURRENT_VOICE_ONLY:
                    mark.scope = MarkScope::VOICE;
                    mark.voice = static_cast<int>(item.voice());
                    break;
            }
        } else if (!std::ranges::binary_search(read.staves, mark.staff)) {
            return {};
        }
    }
    const auto first = first_linked_annotation(item, mark.scope, read);
    if (!first) return std::unexpected(first.error());
    if (!*first) return {};
    switch (type) {
        case ElementType::DYNAMIC: {
            const auto &dynamic = static_cast<const mu::engraving::Dynamic &>(item);
            if (dynamic.dynamic_type() == mu::engraving::DynamicType::OTHER) {
                mark.payload = DynamicMark{dynamic.plain_text().to_std_string()};
            } else {
                DynamicMark value{token(TConv::to_xml(dynamic.dynamic_type()))};
                if (dynamic.has_custom_text()) {
                    auto text = dynamic.plain_text().to_std_string();
                    if (text != value.value) value.custom_text = std::move(text);
                }
                mark.payload = std::move(value);
            }
            break;
        }
        case ElementType::FERMATA: mark.payload = FermataMark{symbol_token(mu::engraving::to_fermata(&item)->sym_id())}; break;
        case ElementType::HARMONY: mark.payload = HarmonyMark{mu::engraving::to_harmony(&item)->harmony_name().to_std_string()}; break;
        case ElementType::EXPRESSION:
        case ElementType::STAFF_TEXT:
        case ElementType::SYSTEM_TEXT:
        case ElementType::REHEARSAL_MARK: {
            auto kind = TextMark::Kind::EXPRESSION;
            if (type == ElementType::STAFF_TEXT) kind = TextMark::Kind::STAFF;
            if (type == ElementType::SYSTEM_TEXT) kind = TextMark::Kind::SYSTEM;
            if (type == ElementType::REHEARSAL_MARK) kind = TextMark::Kind::REHEARSAL;
            mark.payload = TextMark{kind, static_cast<const TextBase &>(item).plain_text().to_std_string()};
            break;
        }
        default: return {};
    }
    return append_mark(item, std::move(mark), read);
}

Result<void> read_structural_mark(const EngravingItem &item, const Segment &segment, MarkRead &read) {
    const auto staff = static_cast<int>(item.staff_idx());
    if (item.generated() || !std::ranges::binary_search(read.staves, staff)) return {};
    MarkMusic mark{.offset = reduced(segment.rtick()), .staff = staff};
    if (item.is_clef()) {
        const auto *clef = mu::engraving::to_clef(&item);
        if (clef->is_courtesy() || (!segment.is_clef_type() && !segment.is_header_clef_type())) return {};
        if (clef->is_header() && segment.tick().is_zero()) return {};
        mark.payload = ClefMark{token(TConv::to_xml(clef->clef_type()))};
    } else if (item.is_bar_line()) {
        using mu::engraving::BarLineType;
        const auto subtype = mu::engraving::to_bar_line(&item)->bar_line_type();
        if (subtype == BarLineType::NORMAL || subtype == BarLineType::START_REPEAT || subtype == BarLineType::END_REPEAT ||
            subtype == BarLineType::END_START_REPEAT)
            return {};
        auto name = token(TConv::to_xml(subtype));
        std::ranges::replace(name, '-', '_');
        mark.payload = BarlineMark{std::move(name)};
    } else {
        return {};
    }
    return append_mark(item, std::move(mark), read);
}

std::string_view arpeggio_token(mu::engraving::ArpeggioType type) {
    using mu::engraving::ArpeggioType;
    switch (type) {
        case ArpeggioType::NORMAL: return "normal";
        case ArpeggioType::UP: return "up";
        case ArpeggioType::DOWN: return "down";
        case ArpeggioType::BRACKET: return "bracket";
        case ArpeggioType::UP_STRAIGHT: return "up_straight";
        case ArpeggioType::DOWN_STRAIGHT: return "down_straight";
    }
    return {};
}

Result<void> read_event_marks(const ChordRest &source, MarkMusic mark, MarkRead &read) {
    for (const auto *lyric : source.lyrics()) {
        if (!charge(read.work, 1)) return music_limit();
        if (lyric->generated()) continue;
        LyricMark value{lyric->verse(), lyric->plain_text().to_std_string(), token(TConv::to_xml(lyric->syllabic()))};
        if (lyric->ticks() > Tick(0, 1)) value.melisma_ticks = reduced(lyric->ticks());
        mark.payload = std::move(value);
        if (auto result = append_mark(*lyric, mark, read); !result) return result;
    }
    if (!source.is_chord()) return {};
    const auto &chord = static_cast<const Chord &>(source);
    for (const auto *articulation : chord.articulations()) {
        if (!charge(read.work, 1)) return music_limit();
        if (articulation->generated()) continue;
        auto symbol = articulation->text_type() == mu::engraving::ArticulationTextType::NO_TEXT ?
                          symbol_token(articulation->sym_id()) :
                          token(TConv::to_xml(articulation->text_type()));
        mark.payload = ArticulationMark{std::move(symbol), articulation->is_ornament()};
        if (auto result = append_mark(*articulation, mark, read); !result) return result;
    }
    if (const auto *arpeggio = chord.arpeggio()) {
        if (!charge(read.work, 1)) return music_limit();
        if (!arpeggio->generated()) {
            mark.payload = ArpeggioMark{std::string(arpeggio_token(arpeggio->arpeggio_type()))};
            if (auto result = append_mark(*arpeggio, mark, read); !result) return result;
        }
    }
    if (const auto *tremolo = chord.tremolo_single_chord()) {
        if (!charge(read.work, 1)) return music_limit();
        if (!tremolo->generated()) {
            mark.payload = TremoloMark{token(TConv::to_xml(tremolo->tremolo_type()))};
            if (auto result = append_mark(*tremolo, mark, read); !result) return result;
        }
    }
    if (const auto *tremolo = chord.tremolo_two_chord()) {
        if (!charge(read.work, 1)) return music_limit();
        if (!tremolo->generated() && tremolo->chord1() == &chord) {
            TremoloMark value{token(TConv::to_xml(tremolo->tremolo_type())), true};
            if (const auto *end = tremolo->chord2()) {
                value.end_event = persisted_id(*end);
                if (const auto *measure = end->measure())
                    value.end = span_position(*measure, end->tick(), end->track(), read.measure_indices);
            }
            mark.payload = std::move(value);
            if (auto result = append_mark(*tremolo, mark, read); !result) return result;
        }
    }
    return {};
}

Result<void> read_attached_marks(const ChordRest &event, Fraction offset, int staff, int voice, MarkRead &read) {
    MarkMusic mark{
        .offset = offset, .scope = MarkScope::VOICE, .staff = staff, .voice = voice, .attachment = MarkAttachment{persisted_id(event)}};
    if (auto result = read_event_marks(event, mark, read); !result) return result;
    if (!event.is_chord()) return {};
    const auto &chord = static_cast<const Chord &>(event);
    for (bool after : {false, true}) {
        int ordinal = 0;
        const auto graces = after ? chord.grace_notes_after() : chord.grace_notes_before();
        for (const auto *grace : graces) {
            if (!charge(read.work, 1)) return music_limit();
            mark.attachment->grace = GraceAttachment{after, ordinal++, persisted_id(*grace)};
            if (auto result = read_event_marks(*grace, mark, read); !result) return result;
        }
    }
    return {};
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

Result<void> read_segment_marks(const Segment &segment, MarkRead &read) {
    using mu::engraving::SegmentType;
    const auto tick = reduced(segment.tick());
    const auto offset = reduced(segment.rtick());
    if (segment.is_time_sig_type() || segment.is_key_sig_type()) {
        for (int staff : read.staves) {
            if (!charge(read.work, 1)) return music_limit();
            const auto *item = segment.element(mu::engraving::staff2track(staff));
            if (!item || item->generated()) continue;
            SignatureInfo signature{.tick = tick, .staff = staff};
            if (item->is_time_sig()) {
                const auto *time = mu::engraving::to_time_sig(item);
                if (time->is_courtesy()) continue;
                signature.type = "TimeSig";
                signature.timesig = written(time->sig());
            } else if (item->is_key_sig()) {
                const auto *key = mu::engraving::to_key_sig(item);
                if (key->is_courtesy()) continue;
                signature.type = "KeySig";
                signature.key = static_cast<int>(key->concert_key());
            } else {
                continue;
            }
            if (!charge(read.budget.rows, 1)) return music_limit();
            signature.id = persisted_id(*item);
            read.bar.signature_changes.push_back(std::move(signature));
        }
    }
    if (segment.is_type(SegmentType::CLEF_TYPES | SegmentType::BAR_LINE_TYPES)) {
        for (const auto *item : segment.elist()) {
            if (!charge(read.work, 1)) return music_limit();
            if (item) {
                if (auto result = read_structural_mark(*item, segment, read); !result) return result;
            }
        }
    }
    for (const auto *item : segment.annotations()) {
        if (!charge(read.work, 1)) return music_limit();
        if (!item || item->generated()) continue;
        if (item->is_tempo_text()) {
            if (!charge(read.budget.rows, 1)) return music_limit();
            const auto *tempo = mu::engraving::to_tempo_text(item);
            std::string text = tempo->plain_text().to_std_string();
            if (!charge_text(read.budget, text)) return music_limit();
            read.bar.signature_changes.push_back(
                {"Tempo", tick, -1, {}, 0, std::round(tempo->tempo().val * 60.0 * 1e4) / 1e4, std::move(text), persisted_id(*tempo)});
        } else if (auto result = read_annotation(*item, offset, read); !result) {
            return result;
        }
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
    if (source.has_voice_assignment_properties()) return applies_to_selection(source, staves, voices);
    const bool staff_wide = source.anchor() == Spanner::Anchor::SEGMENT && !source.is_voice_specific();
    auto matches = [&](mu::engraving::track_idx_t track) {
        return track == muse::nidx ||
               (std::ranges::binary_search(staves, static_cast<int>(mu::engraving::track2staff(track))) &&
                (staff_wide || std::ranges::binary_search(voices, static_cast<int>(mu::engraving::track2voice(track)))));
    };
    return matches(source.track()) || matches(source.effective_track2());
}

std::optional<std::string> hook_token(mu::engraving::HookType hook) {
    using mu::engraving::HookType;
    switch (hook) {
        case HookType::NONE: return std::nullopt;
        case HookType::HOOK_90: return "hook_90";
        case HookType::HOOK_45: return "hook_45";
        case HookType::HOOK_90T: return "hook_90t";
        case HookType::ARROW: return "arrow";
        case HookType::ARROW_FILLED: return "arrow_filled";
        case HookType::ROSETTE: return "rosette";
    }
    return std::nullopt;
}

std::optional<std::string> line_text(const Spanner &source, muse::String text) {
    if (text.empty()) return std::nullopt;
    if (!text.contains(u'<')) return TextBase::un_escape(std::move(text)).to_std_string();
    // The engine's text parser handles formatting and escapes. Spell out the
    // pedal glyphs so they also read correctly without the engraving font.
    text.replace(u"<sym>keyboardPedalPed</sym>", u"Ped.");
    text.replace(u"<sym>keyboardPedalUp</sym>", u"*");
    mu::engraving::Text formatted(source.score()->dummy());
    formatted.set_xml_text(text);
    auto plain = formatted.plain_text().to_std_string();
    if (plain.empty()) return std::nullopt;
    return plain;
}

std::optional<SpanDetail> read_span_detail(const Spanner &source, std::optional<std::string> &subtype) {
    using mu::engraving::DirectionV;
    using mu::engraving::Pid;
    using mu::engraving::PropertyFlags;
    using mu::engraving::TextLineBase;
    SpanDetail detail;
    if (source.is_slur()) {
        const auto direction = mu::engraving::to_slur(&source)->slur_direction();
        if (direction == DirectionV::UP) detail.direction = "up";
        if (direction == DirectionV::DOWN) detail.direction = "down";
    } else if (source.is_hairpin()) {
        const auto &hairpin = *mu::engraving::to_hairpin(&source);
        using mu::engraving::HairpinType;
        switch (hairpin.hairpin_type()) {
            case HairpinType::CRESC_HAIRPIN: subtype = "crescendo"; break;
            case HairpinType::DIM_HAIRPIN: subtype = "diminuendo"; break;
            case HairpinType::CRESC_LINE: subtype = "crescendo_line"; break;
            case HairpinType::DIM_LINE: subtype = "diminuendo_line"; break;
            case HairpinType::INVALID: break;
        }
        detail.niente = hairpin.hairpin_circled_tip();
        if (hairpin.property_flags(Pid::BEGIN_TEXT) != PropertyFlags::STYLED) detail.begin_text = line_text(source, hairpin.begin_text());
        if (hairpin.property_flags(Pid::CONTINUE_TEXT) != PropertyFlags::STYLED)
            detail.continue_text = line_text(source, hairpin.continue_text());
    } else if (source.is_glissando()) {
        const auto &gliss = *mu::engraving::to_glissando(&source);
        subtype = gliss.glissando_type() == mu::engraving::GlissandoType::WAVY ? "wavy" : "straight";
        if (gliss.property_flags(Pid::GLISS_TEXT) != PropertyFlags::STYLED) detail.text = line_text(source, gliss.text());
    } else if (source.is_trill()) {
        subtype = token(TConv::to_xml(mu::engraving::to_trill(&source)->trill_type()));
    } else if (source.is_ottava()) {
        const auto &ottava = *mu::engraving::to_ottava(&source);
        subtype = mu::engraving::Ottava::ottava_type_name(ottava.ottava_type());
        if (ottava.property_flags(Pid::BEGIN_TEXT) != PropertyFlags::STYLED) detail.begin_text = line_text(source, ottava.begin_text());
        if (ottava.property_flags(Pid::CONTINUE_TEXT) != PropertyFlags::STYLED)
            detail.continue_text = line_text(source, ottava.continue_text());
        if (ottava.property_flags(Pid::END_TEXT) != PropertyFlags::STYLED) detail.end_text = line_text(source, ottava.end_text());
    } else if (source.is_pedal() || source.is_text_line()) {
        const auto &line = static_cast<const TextLineBase &>(source);
        detail.begin_text = line_text(source, line.begin_text());
        detail.continue_text = line_text(source, line.continue_text());
        detail.end_text = line_text(source, line.end_text());
        detail.begin_hook = hook_token(line.begin_hook_type());
        detail.end_hook = hook_token(line.end_hook_type());
    }
    if (detail.direction || detail.niente || detail.text || detail.begin_text || detail.continue_text || detail.end_text ||
        detail.begin_hook || detail.end_hook)
        return detail;
    return std::nullopt;
}

Result<void> read_spans(const Score &score, Tick begin, Tick end, const std::vector<int> &staves, const std::vector<int> &voices,
                        const MeasureIndices &indices, MusicWindow &window, ReadBudget &budget, std::size_t &work) {
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
        auto detail = read_span_detail(source, subtype);
        if (subtype && !charge_text(budget, *subtype)) return music_limit();
        if (detail) {
            for (const auto *text : {&detail->direction, &detail->text, &detail->begin_text, &detail->continue_text, &detail->end_text,
                                     &detail->begin_hook, &detail->end_hook}) {
                if (*text && !charge_text(budget, **text)) return music_limit();
            }
        }
        seen.insert(*id);
        SpanMusic span{*id,
                       std::move(type_name),
                       std::move(subtype),
                       span_position(*start, start_tick, source.track(), indices),
                       span_position(*finish, end_tick, source.effective_track2(), indices),
                       source_anchor(source, *start, start_tick, false),
                       source_anchor(source, *finish, end_tick, true),
                       std::move(detail)};
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
    const auto staff_selected = [&](int staff) { return std::ranges::binary_search(staves, staff); };
    for (auto *measure = score.first_measure(); measure && measure->tick() < end; measure = measure->next_measure()) {
        if (!charge(work, 1)) return music_limit();
        for (auto *segment = measure->first(mu::engraving::SegmentType::CHORD_REST); segment && segment->tick() < end;
             segment = segment->next(mu::engraving::SegmentType::CHORD_REST)) {
            if (!charge(work, 1)) return music_limit();
            // Scan every track so cross-staff note lines starting on an excluded staff
            // and ending on a selected staff are still discovered via spanner_for().
            const std::size_t ntracks = score.ntracks();
            for (std::size_t track = 0; track < ntracks; ++track) {
                const auto *item = segment->element(track);
                if (!item || !item->is_chord()) continue;
                const auto &chord = *mu::engraving::to_chord(item);
                const bool in_window = chord.tick() >= begin && staff_selected(static_cast<int>(mu::engraving::track2staff(track)));
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

    auto work = max_scan_work;
    MeasureIndices indices;
    const Measure *measure = nullptr;
    std::int32_t measure_count = 0;
    for (const auto *bar = score.first_measure(); bar; bar = bar->next_measure()) {
        if (!charge(work, 1)) return music_limit();
        if (measure_count == range.first_measure) measure = bar;
        indices.emplace(bar, measure_count++);
    }
    if (!measure) return std::unexpected(Error{ErrorCode::INVALID_ARGUMENT, "measure: outside score; expected an existing bar"});
    const auto remaining = measure_count - range.first_measure;
    if (range.measure_count > remaining && !range.clamp_to_score_end)
        return std::unexpected(
            Error{ErrorCode::INVALID_ARGUMENT, "count: window extends past score; expected a count within the remaining bars"});
    const auto count = std::min(range.measure_count, remaining);

    MusicWindow window{.score = range.score, .revision = impl_->revision};
    const auto begin = measure->tick();
    auto end = begin;
    ReadBudget budget;
    if (!charge(budget.rows, staves->size())) return music_limit();
    for (int staff_index : *staves) {
        const auto &staff = *score.staff(staff_index);
        const auto *meter = staff.time_sig(begin);
        const auto transpose = staff.transpose(begin);
        const auto key = staff.concert_key(begin);
        std::string part_name = staff.part()->part_name().to_std_string();
        std::string instrument_id = staff.origin_part(begin)->instrument_id(begin).to_std_string();
        std::string clef = token(TConv::to_xml(staff.clef(begin)));
        if (!charge_text(budget, part_name) || !charge_text(budget, instrument_id) || !charge_text(budget, clef)) return music_limit();
        window.staves.push_back({staff_index,
                                 std::move(part_name),
                                 std::move(instrument_id),
                                 written(meter ? meter->sig() : measure->timesig()),
                                 key == mu::engraving::Key::INVALID ? 0 : static_cast<int>(key),
                                 {transpose.diatonic, transpose.chromatic},
                                 staff.is_drum_staff(begin),
                                 std::move(clef)});
    }

    ElementIdSet seen_tuplets;
    LinkedMarkSet seen_links;
    for (int index = 0; index < count; ++index, measure = measure->next_measure()) {
        MeasureMusic bar{{music_id(*measure), range.first_measure + index, reduced(measure->tick()), reduced(measure->ticks())},
                         measure->repeat_start(),
                         measure->repeat_end(),
                         measure->repeat_end() ? measure->repeat_count() : 2};
        MarkRead marks{bar, *staves, *voices, indices, seen_links, budget, work};
        for (const auto *segment = measure->first(); segment; segment = segment->next()) {
            if (!charge(work, 1)) return music_limit();
            if (auto result = read_segment_marks(*segment, marks); !result) return std::unexpected(std::move(result.error()));
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
                    if (auto result = read_attached_marks(*mu::engraving::to_chord_rest(item), event->offset, staff, voice, marks); !result)
                        return std::unexpected(std::move(result.error()));
                    lane.events.push_back(std::move(*event));
                }
                if (!lane.events.empty()) bar.voices.push_back(std::move(lane));
            }
        }
        std::stable_sort(bar.markings.begin(), bar.markings.end(), mark_before);
        window.measures.push_back(std::move(bar));
        end = measure->end_tick();
    }
    if (auto result = read_spans(score, begin, end, *staves, *voices, indices, window, budget, work); !result)
        return std::unexpected(std::move(result.error()));
    return window;
}

} // namespace engrave
