// SPDX-License-Identifier: GPL-3.0-only
#include "write470_spanners.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <climits>
#include <optional>
#include <stdexcept>
#include <utility>

#include <ankerl/unordered_dense.h>

#include "engraving/dom/authoredoverride.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/linkedobjects.h"
#include "engraving/dom/location.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/volta.h"
#include "global/io/writebudget.h"

namespace engrave::write470 {
namespace {
using namespace mu::engraving;
using namespace std::literals;

constexpr std::array note_types{"Tie"sv, "Glissando"sv, "NoteLine"sv, "GuitarBend"sv, "TextLine"sv};
constexpr std::array chord_rest_types{"Slur"sv, "HammerOnPullOff"sv};
constexpr std::array voice_types{"HairPin"sv,      "Pedal"sv,     "Ottava"sv,  "Trill"sv,     "TextLine"sv,   "GradualTempoChange"sv,
                                 "LetRing"sv,      "PalmMute"sv,  "Vibrato"sv, "WhammyBar"sv, "PickScrape"sv, "PartialLyricsLine"sv,
                                 "HarmonicMark"sv, "Rasgueado"sv, "Volta"sv};

using Poll = void (*)();

void charge(std::size_t bytes) {
    if (auto *budget = muse::io::active_write_budget; budget && !budget->charge(bytes)) throw muse::io::WriteBudgetStop{};
}

void require(bool valid, const char *message) {
    if (!valid) throw std::invalid_argument(message);
}

std::string_view tag(pugi::xml_node node) { return node.name(); }

const EngravingObject *source_object(pugi::xml_node node, const Score &score) {
    const auto eid = EID::from_std_string(std::string_view(node.child_value("eid")));
    return eid.is_valid() ? score.master_score()->eid_register()->item_from_eid(eid) : nullptr;
}

bool names_element(pugi::xml_node field, const EngravingItem *element) {
    return field && element && element->eid() == EID::from_std_string(std::string_view(field.child_value()));
}

struct Cursor {
    Fraction tick;
    int track;

    bool operator==(const Cursor &) const = default;
};

struct Slot {
    pugi::xml_node next;
    Cursor before, after;
    bool chord_rest = false;
    bool inside_grace_cluster = false;
};

struct VoiceEvent {
    std::size_t pair;
    std::size_t slot;
    bool start;
};

struct Voice {
    pugi::xml_node xml;
    std::vector<Slot> slots;
    std::vector<std::size_t> by_tick;
    std::vector<VoiceEvent> events;
};

struct Row {
    pugi::xml_node xml;
    const Measure *measure;
    std::array<Voice, voices> lanes;
};

struct Owner {
    pugi::xml_node xml;
    Location key;
};

struct Endpoint {
    Location key = Location::absolute();
    pugi::xml_node receiver;
    Voice *voice = nullptr;
};

struct Pair {
    pugi::xml_node body;
    const Spanner *spanner;
    Endpoint start, end;
    pugi::xml_node forward, backward;
};

struct EmittedBody {
    pugi::xml_node xml;
    const Spanner *spanner;
};

struct ScoreIndex {
    const Score &score;
    Poll poll;
    std::vector<const Measure *> measures;
    std::vector<std::vector<Row>> staves;
    ankerl::unordered_dense::map<const EngravingItem *, Owner> owners;
    std::vector<const Note *> notes;
    std::vector<pugi::xml_node> lyrics_lines;
};

Location position(Cursor cursor, const Row &row, int ordinal) {
    auto loc = Location::absolute();
    loc.set_track(cursor.track);
    loc.set_measure(ordinal);
    loc.set_frac(cursor.tick - row.measure->tick());
    return loc;
}

Location read_location(pugi::xml_node xml) {
    auto loc = Location::relative();
    loc.set_staff(xml.child("staves").text().as_int());
    loc.set_voice(xml.child("voices").text().as_int());
    loc.set_measure(xml.child("measures").text().as_int());
    if (auto frac = xml.child("fractions")) {
        bool ok = false;
        loc.set_frac(Fraction::from_string(muse::String::from_utf8(frac.child_value()), &ok));
        require(ok && loc.frac().is_valid(), "Invalid 5.00 location fraction");
    }
    loc.set_grace_index(xml.child("grace").text().as_int(INT_MIN));
    loc.set_note(xml.child("notes").text().as_int());
    loc.set_is_time_tick(xml.child("timeTick").text().as_bool());
    return loc;
}

void write_location(pugi::xml_node xml, const Location &loc) {
    const auto integer = [&](const char *name, int value, int neutral = 0) {
        if (value != neutral) xml.append_child(name).text().set(value);
    };
    integer("staves", loc.staff());
    integer("voices", loc.voice());
    integer("measures", loc.measure());
    if (!loc.frac().is_zero()) xml.append_child("fractions").text().set(loc.frac().reduced().to_string().to_std_string().c_str());
    integer("grace", loc.grace_index(), INT_MIN);
    integer("notes", loc.note());
    if (loc.is_time_tick()) xml.append_child("timeTick").text().set(1);
}

Cursor move_cursor(Cursor current, pugi::xml_node xml) {
    auto move = read_location(xml);
    require(move.measure() == 0, "Voice location changes measure");
    current.tick += move.frac();
    current.track += move.staff() * static_cast<int>(voices) + move.voice();
    return current;
}

bool duration_tag(std::string_view name) { return name == "Chord" || name == "Rest" || name == "MeasureRepeat"; }

void index_owner(ScoreIndex &index, const EngravingItem *item, pugi::xml_node xml, Location key) {
    charge(2 * sizeof(Owner) + 4 * sizeof(void *));
    require(index.owners.emplace(item, Owner{xml, key}).second, "Duplicate 5.00 endpoint owner");
}

} // namespace

std::vector<LegacyNote> legacy_notes(pugi::xml_node chord, const Chord &dom, void (*poll)()) {
    charge(dom.notes().size() * (sizeof(LegacyNote) + 2 * sizeof(void *)));
    std::vector<LegacyNote> notes;
    notes.reserve(dom.notes().size());
    // 4.7.6 adds the notes while the chord is still on the tick-zero dummy segment.
    const bool tab = dom.staff()->staff_type(Fraction(0, 1))->is_tab_staff();
    for (auto xml : chord.children("Note")) {
        poll();
        const auto *item = source_object(xml, *dom.score());
        require(item && item->is_note() && to_note(item)->chord() == &dom, "5.00 note does not match its chord");
        const auto *note = to_note(item);
        const LegacyNote entry{note, xml, note->pitch(), note->string(),
                               xml.child("fixed").text().as_bool() ? xml.child("fixedLine").text().as_int() :
                                                                     xml.child("line").text().as_int(invalid_line)};
        // The legacy reader inserts after the first matching pitch/line, even if more unisons follow it.
        auto insert = std::find_if(notes.begin(), notes.end(), [&](const LegacyNote &other) {
            poll();
            return tab ? entry.string > other.string : entry.pitch <= other.pitch;
        });
        if (!tab && insert != notes.end() && insert->pitch == entry.pitch && entry.line >= insert->line) ++insert;
        notes.insert(insert, entry);
    }
    return notes;
}

void sort_legacy_percussion_notes(std::vector<LegacyNote> &notes, const Chord &dom, void (*poll)()) {
    // 4.7.6 resolves the group while the chord is still on the tick-zero dummy segment.
    const Fraction initial_tick(0, 1);
    if (dom.staff()->staff_type(initial_tick)->group() != StaffGroup::PERCUSSION) return;
    const auto *drumset = dom.part()->instrument(initial_tick)->drumset();
    for (auto &entry : notes) {
        poll();
        if (!drumset || !drumset->is_valid(entry.pitch))
            entry.line = 0;
        else if (!entry.note->fixed())
            entry.line = drumset->line(entry.pitch);
    }
    std::sort(notes.begin(), notes.end(), [&](const LegacyNote &left, const LegacyNote &right) {
        poll();
        if (left.line != right.line) return left.line > right.line;
        if (left.pitch != right.pitch) return left.pitch < right.pitch;
        const auto *left_tie = left.note->tie_back();
        const auto *right_tie = right.note->tie_back();
        if (!left_tie || !right_tie) return left_tie && !right_tie;
        if (left_tie->is_partial_tie() || right_tie->is_partial_tie()) return !left_tie->is_partial_tie() && right_tie->is_partial_tie();
        const auto *start = left_tie->start_note();
        const auto *other_start = right_tie->start_note();
        if (!start || !other_start) return start && !other_start;
        if (start->chord() == other_start->chord()) return start->unison_index() < other_start->unison_index();
        return start->chord()->is_before(other_start->chord());
    });
}

namespace {

void index_notes(ScoreIndex &index, pugi::xml_node chord, const Chord &dom, Location key) {
    for (auto xml : chord.children("Note")) {
        index.poll();
        const auto *item = source_object(xml, index.score);
        require(item && item->is_note() && to_note(item)->chord() == &dom, "5.00 note does not match its chord");
        index.notes.push_back(to_note(item));
    }
    const auto notes = legacy_notes(chord, dom, index.poll);
    for (std::size_t ordinal = 0; ordinal < notes.size(); ++ordinal) {
        index.poll();
        key.set_note(static_cast<int>(ordinal));
        index_owner(index, notes[ordinal].note, notes[ordinal].xml, key);
    }
}

void index_voice(ScoreIndex &index, Row &row, int ordinal, int track, Voice &voice) {
    Cursor cursor{row.measure->tick(), track};
    int next_grace = 0;
    for (auto child : voice.xml.children()) {
        index.poll();
        Slot slot{child, cursor, cursor, false, next_grace != 0};
        if (tag(child) == "location") {
            cursor = move_cursor(cursor, child);
        } else if (tag(child) == "tick") {
            cursor.tick = Fraction::from_ticks(child.text().as_int());
        } else if (duration_tag(tag(child))) {
            const auto *item = source_object(child, index.score);
            require(item && item->is_chord_rest(), "Missing 5.00 duration owner");
            const auto *cr = to_chord_rest(item);
            require(cr->measure() == row.measure && cr->tick() == cursor.tick && cr->track() == static_cast<track_idx_t>(cursor.track),
                    "5.00 duration owner does not match the voice cursor");
            const bool grace = cr->is_chord() && to_chord(cr)->is_grace();
            auto key = position(cursor, row, ordinal);
            if (grace) key.set_grace_index(next_grace++);
            if (tag(child) != "MeasureRepeat") index_owner(index, cr, child, key);
            for (auto lyrics : child.children("Lyrics")) {
                index.poll();
                if (auto line = lyrics.child("LyricsLine")) {
                    charge(2 * sizeof(line));
                    index.lyrics_lines.push_back(line);
                }
            }
            if (tag(child) == "Chord") index_notes(index, child, *to_chord(cr), key);
            if (!grace) {
                cursor.tick += tag(child) == "MeasureRepeat" ? row.measure->ticks() : cr->actual_ticks_at(cursor.tick);
                next_grace = 0;
            }
            slot.chord_rest = true;
        }
        slot.after = cursor;
        charge(2 * sizeof(Slot));
        voice.slots.push_back(slot);
    }
    charge(sizeof(Slot));
    voice.slots.push_back({{}, cursor, cursor});
}

void index_score(ScoreIndex &index, pugi::xml_node xml) {
    for (auto *measure = index.score.first_measure(); measure; measure = measure->next_measure()) {
        index.poll();
        charge(2 * sizeof(void *));
        index.measures.push_back(measure);
    }
    charge(index.score.nstaves() * sizeof(std::vector<Row>));
    index.staves.resize(index.score.nstaves());
    for (auto staff : xml.children("Staff")) {
        index.poll();
        const auto id = staff.attribute("id").as_uint();
        require(id > 0 && id <= index.staves.size(), "Invalid 5.00 staff ordinal");
        auto &rows = index.staves[id - 1];
        charge(index.measures.size() * sizeof(Row));
        rows.reserve(index.measures.size());
        int ordinal = 0;
        for (auto measure : staff.children("Measure")) {
            index.poll();
            require(static_cast<std::size_t>(ordinal) < index.measures.size(), "Unexpected 5.00 measure");
            auto &row = rows.emplace_back(Row{measure, index.measures[ordinal], {}});
            int voice = 0;
            for (auto lane : measure.children("voice")) {
                require(voice < static_cast<int>(voices), "Invalid 5.00 voice ordinal");
                row.lanes[voice].xml = lane;
                index_voice(index, row, ordinal, static_cast<int>((id - 1) * voices) + voice, row.lanes[voice]);
                ++voice;
            }
            ++ordinal;
        }
        require(rows.size() == index.measures.size(), "Missing 5.00 measures");
    }
}

std::optional<Endpoint> voice_endpoint(ScoreIndex &index, Fraction tick, track_idx_t track) {
    if (track / voices >= index.staves.size() || index.measures.empty()) return std::nullopt;
    // An interior barline belongs to the following measure. The final barline has no following row.
    const auto next = std::upper_bound(index.measures.begin(), index.measures.end(), tick,
                                       [](const Fraction &value, const Measure *measure) { return value < measure->tick(); });
    if (next == index.measures.begin()) return std::nullopt;
    const auto ordinal = static_cast<std::size_t>(next - index.measures.begin() - 1);
    auto &rows = index.staves[track / voices];
    if (ordinal >= rows.size() || tick > rows[ordinal].measure->end_tick()) return std::nullopt;
    auto &row = rows[ordinal];
    const auto lane = track % voices;
    for (std::size_t i = 0; i <= lane; ++i) {
        if (row.lanes[i].xml) continue;
        charge(sizeof(Slot) + 128);
        auto &voice = row.lanes[i];
        voice.xml = row.xml.append_child("voice");
        const Cursor initial{row.measure->tick(), static_cast<int>(track - lane + i)};
        voice.slots.push_back({{}, initial, initial});
    }
    return Endpoint{position({tick, static_cast<int>(track)}, row, static_cast<int>(ordinal)), row.lanes[lane].xml, &row.lanes[lane]};
}

std::optional<Endpoint> item_endpoint(const ScoreIndex &index, const EngravingItem *item, std::string_view type) {
    const auto found = index.owners.find(item);
    if (found == index.owners.end() || !is_legacy_connector(type, tag(found->second.xml))) return std::nullopt;
    return Endpoint{found->second.key, found->second.xml};
}

bool clean_line_segments(pugi::xml_node body, const Spanner &dom, AuthoredChildRole role, const ScoreIndex &index) {
    std::size_t next = 0;
    const auto *table = authored_override_table(&dom);
    const Spanner *line = role == AuthoredChildRole::HOLD_LINE ? static_cast<const GuitarBend &>(dom).hold_line() : &dom;
    for (auto segment : body.children("Segment")) {
        index.poll();
        std::size_t ordinal = next;
        if (const auto no = segment.attribute("no")) {
            const std::string_view text = no.value();
            const auto result = std::from_chars(text.data(), text.data() + text.size(), ordinal);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || ordinal < next || !table) return false;
            const AuthoredChildKey key{role, static_cast<std::uint32_t>(ordinal), 0, role == AuthoredChildRole::HOLD_LINE ? 1u : 0u};
            if (ordinal > UINT32_MAX || !table->carrier(key)) return false;
            const auto segments = line ? std::span(line->spanner_segments()) : std::span<SpannerSegment *const>{};
            if (ordinal > next) {
                if (!segments.empty()) {
                    if (ordinal >= segments.size()) return false;
                } else {
                    const auto *subtype = table->find(key, Pid::SUBTYPE);
                    if (ordinal >= index.measures.size()) return false;
                    if (subtype) {
                        const auto kind = static_cast<SpannerSegmentType>(subtype->value.to_int());
                        if (kind != SpannerSegmentType::END && kind != SpannerSegmentType::MIDDLE) return false;
                    }
                }
            }
            for (; next < ordinal; ++next) {
                index.poll();
                charge(256);
                // The legacy reader appends segments in file order, so fill the missing prefix.
                const auto subtype = !segments.empty() ? segments[next]->spanner_segment_type() :
                                     next == 0         ? SpannerSegmentType::BEGIN :
                                                         SpannerSegmentType::MIDDLE;
                body.insert_child_before("Segment", segment).append_child("subtype").text().set(static_cast<int>(subtype));
            }
            segment.remove_attribute(no);
        }
        next = ordinal + 1;
    }
    return true;
}

std::string clean_body(pugi::xml_node body, const Spanner &spanner, int receiver_track, const ScoreIndex &index) {
    // 4.7.6 reads this property only through these spanners' styled-property readers.
    constexpr std::array styled_text_size_types{"HairPin"sv, "Ottava"sv, "Pedal"sv, "Volta"sv};
    if (body.child("textSizeIsSpatiumDependent") && !std::ranges::contains(styled_text_size_types, tag(body)))
        return std::string(body.name()) + "/textSizeIsSpatiumDependent";
    for (const char *field : {"startTick", "track2", "startElement", "endElement", "ticks", "ticks_f"}) body.remove_child(field);
    if (auto track = body.child("track"); track && track.text().as_int(-1) == receiver_track) body.remove_child(track);
    if (spanner.is_guitar_bend()) {
        constexpr std::array types{"bend"sv, "pre-bend"sv, "grace-note-bend"sv, "slight-bend"sv, "dive"sv, "pre-dive"sv,
                                   "dip"sv,  "scoop"sv};
        const auto type = body.child("guitarBendType");
        const auto found = std::ranges::find(types, type.child_value());
        if (found == types.end()) return "GuitarBend/guitarBendType";
        type.text().set(static_cast<int>(found - types.begin()));
        if (auto hold = body.child("GuitarBendHold"); hold && !clean_line_segments(hold, spanner, AuthoredChildRole::HOLD_LINE, index))
            return "GuitarBendHold/Segment ordinal";
    }
    if (!clean_line_segments(body, spanner, AuthoredChildRole::SPANNER_SEGMENT, index))
        return std::string(body.name()) + "/Segment ordinal";
    return {};
}

const Spanner *note_back_spanner(const Note &owner, pugi::xml_node body) {
    if (tag(body) == "Tie") return owner.tie_back_non_partial();
    const auto &back = owner.spanner_back();
    const auto found = std::ranges::find_if(back, [&](const Spanner *spanner) { return names_element(body.child("eid"), spanner); });
    return found == back.end() ? nullptr : *found;
}

std::optional<Pair> plan_pair(ScoreIndex &index, pugi::xml_node body, const Spanner &spanner, const Note *note) {
    const auto type = tag(body);
    const auto *start = spanner.start_element();
    const auto *end = spanner.end_element();
    if (const auto anchor = spanner.anchor(); anchor == Spanner::Anchor::NOTE || anchor == Spanner::Anchor::CHORDREST) {
        if (!names_element(body.child("startElement"), start) || !names_element(body.child("endElement"), end)) return std::nullopt;
        if (note) {
            if (end != note || !start->is_note()) return std::nullopt;
            const auto *first = to_note(start);
            if (spanner.is_tie() ? first->tie_for_non_partial() != &spanner : !std::ranges::contains(first->spanner_for(), &spanner))
                return std::nullopt;
        }
        auto from = item_endpoint(index, start, type);
        auto to = item_endpoint(index, end, type);
        if (from && to) return Pair{body, &spanner, *from, *to, {}, {}};
    } else if (is_legacy_connector(type, "voice")) {
        auto start_tick = spanner.tick();
        auto end_tick = spanner.tick2();
        if (spanner.is_volta() && end && end->is_measure()) end_tick = to_measure(end)->end_tick();
        auto from = voice_endpoint(index, start_tick, spanner.track());
        auto to = voice_endpoint(index, end_tick, spanner.effective_track2());
        if (from && to) return Pair{body, &spanner, *from, *to, {}, {}};
    }
    return std::nullopt;
}

bool discarded_slur(const Spanner &spanner) {
    if (!spanner.is_slur()) return false;
    return spanner.generated() || !spanner.start_element() || !spanner.end_element() ||
           (spanner.start_element() == spanner.end_element() &&
            static_cast<const Slur &>(spanner).partial_spanner_direction() == PartialSpannerDirection::NONE);
}

void plan_body(ScoreIndex &index, pugi::xml_node body, const Spanner *spanner, const Note *note, std::vector<Pair> &pairs,
               std::vector<ConnectorIssue> &issues) {
    index.poll();
    if (spanner && spanner->score() == &index.score && tag(body) == spanner->type_name() && names_element(body.child("eid"), spanner)) {
        if (discarded_slur(*spanner)) {
            body.parent().remove_child(body);
            return;
        }
        if (auto pair = plan_pair(index, body, *spanner, note)) {
            if (auto detail = clean_body(body, *spanner, pair->start.key.track(), index); detail.empty()) {
                charge(2 * sizeof(Pair) + 8 * 192);
                pairs.push_back(*pair);
            } else {
                issues.push_back({body, std::move(detail)});
            }
            return;
        }
    }
    issues.push_back({body, std::string(body.name()) + " endpoint mapping"});
}

void plan_score(ScoreIndex &index, pugi::xml_node xml, std::vector<Pair> &pairs, std::vector<ConnectorIssue> &issues) {
    for (auto map : xml.children("SpannerMap")) {
        for (auto body = map.first_child(); body;) {
            const auto next = body.next_sibling();
            const auto *item = source_object(body, index.score);
            plan_body(index, body, item && item->is_spanner() ? to_spanner(item) : nullptr, nullptr, pairs, issues);
            body = next;
        }
    }
    for (const auto *note : index.notes) {
        index.poll();
        const auto owner = index.owners.at(note).xml;
        for (auto body : owner.children()) {
            index.poll();
            if (tag(body) == "PartialTie" || tag(body) == "LaissezVib") {
                body.remove_child("startElement");
                body.remove_child("endElement");
                if (body.child("track").text().as_int(-1) == index.owners.at(note).key.track()) body.remove_child("track");
            } else if (is_legacy_connector(tag(body), "Note")) {
                plan_body(index, body, note_back_spanner(*note, body), note, pairs, issues);
            }
        }
    }
    for (auto line : index.lyrics_lines) {
        index.poll();
        const auto *item = source_object(line, index.score);
        if (!item || !item->is_lyrics_line()) {
            issues.push_back({line, "LyricsLine owner"});
            continue;
        }
        const auto *spanner = to_spanner(item);
        auto issue = clean_body(line, *spanner, static_cast<int>(spanner->track()), index);
        if (!issue.empty()) issues.push_back({line, std::move(issue)});
    }
}

void prepare_voice(Voice &voice, Poll poll) {
    charge(voice.slots.size() * sizeof(std::size_t));
    voice.by_tick.reserve(voice.slots.size());
    for (std::size_t i = 0; i < voice.slots.size(); ++i) {
        poll();
        if (!voice.slots[i].inside_grace_cluster) voice.by_tick.push_back(i);
    }
    std::ranges::sort(voice.by_tick, [&](std::size_t left, std::size_t right) {
        poll();
        const auto &a = voice.slots[left];
        const auto &b = voice.slots[right];
        if (a.before.tick != b.before.tick) return a.before.tick < b.before.tick;
        if (a.chord_rest != b.chord_rest) return a.chord_rest;
        return left > right;
    });
}

void schedule_endpoint(std::size_t pair, bool start, Endpoint &endpoint, const ScoreIndex &index) {
    if (!endpoint.voice) return;
    auto &voice = *endpoint.voice;
    if (voice.by_tick.empty()) prepare_voice(voice, index.poll);
    const auto tick = index.measures[endpoint.key.measure()]->tick() + endpoint.key.frac();
    const auto it = std::ranges::lower_bound(voice.by_tick, tick, {}, [&](std::size_t slot) { return voice.slots[slot].before.tick; });
    const auto slot = it == voice.by_tick.end() ? voice.slots.size() - 1 : *it;
    charge(2 * sizeof(VoiceEvent));
    voice.events.push_back({pair, slot, start});
}

pugi::xml_node insert_at(pugi::xml_node receiver, pugi::xml_node before, const char *name) {
    return before ? receiver.insert_child_before(name, before) : receiver.append_child(name);
}

void emit_endpoint(Pair &pair, bool start, pugi::xml_node before) {
    const auto &from = start ? pair.start : pair.end;
    const auto &to = start ? pair.end : pair.start;
    auto receiver = insert_at(from.receiver, before, "Spanner");
    receiver.append_attribute("type") = pair.body.name();
    if (start) {
        require(bool(receiver.append_move(pair.body)), "Could not relocate 4.70 spanner body");
    }
    auto relative = to.key;
    // Non-note locations use the sentinel in both directions. Their note delta is zero.
    relative.to_relative(from.key);
    const auto loc = receiver.append_child(start ? "next" : "prev").append_child("location");
    write_location(loc, relative);
    auto decoded = read_location(loc);
    decoded.to_absolute(from.key);
    require(decoded == to.key, "4.70 reciprocal connector location changed during serialization");
    (start ? pair.forward : pair.backward) = receiver;
}

void emit_move(Voice &voice, const Slot &slot, Cursor &cursor, const Cursor &target, bool time_tick) {
    if (cursor == target && !time_tick) return;
    charge(512);
    constexpr auto voices_per_staff = static_cast<int>(voices);
    auto move = Location::relative();
    move.set_staff(target.track / voices_per_staff - cursor.track / voices_per_staff);
    move.set_voice(target.track % voices_per_staff - cursor.track % voices_per_staff);
    move.set_frac(target.tick - cursor.tick);
    move.set_is_time_tick(time_tick);
    auto location = insert_at(voice.xml, slot.next, "location");
    write_location(location, move);
    cursor = move_cursor(cursor, location);
    require(cursor == target, "4.70 voice move changed during serialization");
}

void emit_voice(Voice &voice, const Row &row, std::vector<Pair> &pairs, Poll poll) {
    std::ranges::sort(voice.events, [&](const VoiceEvent &a, const VoiceEvent &b) {
        poll();
        if (a.slot != b.slot) return a.slot < b.slot;
        const auto &left = a.start ? pairs[a.pair].start : pairs[a.pair].end;
        const auto &right = b.start ? pairs[b.pair].start : pairs[b.pair].end;
        if (left.key.frac() != right.key.frac()) return left.key.frac() < right.key.frac();
        if (a.pair != b.pair) return a.pair < b.pair;
        return a.start && !b.start;
    });
    const auto properties = [&](const VoiceEvent &event) {
        const auto &pair = pairs[event.pair];
        const auto &end = event.start ? pair.start : pair.end;
        return Cursor{row.measure->tick() + end.key.frac(), end.key.track()};
    };
    std::size_t event = 0;
    auto cursor = voice.slots.front().before;
    for (std::size_t i = 0; i < voice.slots.size(); ++i) {
        poll();
        const auto &slot = voice.slots[i];
        require(cursor == slot.before, "4.70 connector insertion changed an original voice checkpoint");
        std::optional<Cursor> previous;
        while (event < voice.events.size() && voice.events[event].slot == i) {
            poll();
            const auto &entry = voice.events[event++];
            const auto target = properties(entry);
            const bool anchored = slot.chord_rest && target == slot.before;
            if (target != previous) emit_move(voice, slot, cursor, target, !anchored);
            emit_endpoint(pairs[entry.pair], entry.start, slot.next);
            previous = target;
        }
        if (slot.next) emit_move(voice, slot, cursor, slot.before, false);
        require(!slot.next || cursor == slot.before, "4.70 could not restore original voice checkpoint");
        if (tag(slot.next) == "location")
            cursor = move_cursor(cursor, slot.next);
        else if (slot.next) {
            cursor.tick += slot.after.tick - slot.before.tick;
            cursor.track += slot.after.track - slot.before.track;
        }
        require(!slot.next || cursor == slot.after, "4.70 connector insertion displaced an original voice event");
    }
}

void emit_pairs(ScoreIndex &index, std::vector<Pair> &pairs, std::vector<EmittedBody> &bodies) {
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        index.poll();
        auto &pair = pairs[i];
        if (pair.start.voice)
            schedule_endpoint(i, true, pair.start, index);
        else
            emit_endpoint(pair, true, {});
        if (pair.end.voice)
            schedule_endpoint(i, false, pair.end, index);
        else
            emit_endpoint(pair, false, {});
    }
    for (auto &staff : index.staves) {
        for (auto &row : staff) {
            index.poll();
            for (auto &voice : row.lanes) {
                if (!voice.events.empty()) emit_voice(voice, row, pairs, index.poll);
            }
        }
    }
    for (const auto &pair : pairs) {
        index.poll();
        require(pair.forward && pair.backward && pair.body.parent() == pair.forward, "Incomplete 4.70 spanner pair");
        charge(2 * sizeof(EmittedBody));
        bodies.push_back({pair.body, pair.spanner});
    }
}

void rebase_links(pugi::xml_node root, const std::vector<EmittedBody> &bodies, Poll poll) {
    const auto hash_node = [](pugi::xml_node node) { return node.hash_value(); };
    ankerl::unordered_dense::map<pugi::xml_node, const Spanner *, decltype(hash_node)> by_node;
    for (const auto &[body, spanner] : bodies) {
        poll();
        if (!spanner->links() && !body.child("linkedTo")) continue;
        charge(80);
        by_node.emplace(body, spanner);
    }
    if (by_node.empty()) return;

    const auto hash = [](const auto &key) {
        return ankerl::unordered_dense::hash<const LinkedObjects *>{}(key.first) ^ static_cast<std::size_t>(key.second);
    };
    ankerl::unordered_dense::map<std::pair<const LinkedObjects *, ElementType>, const Spanner *, decltype(hash)> earliest;
    const auto visit = [&](const auto &self, pugi::xml_node node) -> void {
        poll();
        if (auto found = by_node.find(node); found != by_node.end()) {
            node.remove_child("linkedTo");
            const auto *spanner = found->second;
            if (const auto *links = spanner->links()) {
                const auto [first, inserted] = earliest.try_emplace({links, spanner->type()}, spanner);
                if (!inserted) {
                    charge(192);
                    node.insert_child_after("linkedTo", node.child("eid")).text().set(first->second->eid().to_std_string().c_str());
                }
            }
            return;
        }
        for (auto child : node.children()) self(self, child);
    };
    visit(visit, root);
}
} // namespace

bool is_legacy_connector(std::string_view type, std::string_view receiver) {
    if (receiver == "Note") return std::ranges::contains(note_types, type);
    if (receiver == "Chord" || receiver == "Rest") return std::ranges::contains(chord_rest_types, type);
    return receiver == "voice" && std::ranges::contains(voice_types, type);
}

std::vector<ConnectorIssue> reconstruct_connectors(pugi::xml_node root, std::span<Score *const> scores, Poll poll) {
    std::vector<ConnectorIssue> issues;
    std::vector<EmittedBody> bodies;
    std::vector<pugi::xml_node> nodes{root.child("Score")};
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (auto score : nodes[i].children("Score")) nodes.push_back(score);
        poll();
        auto xml = nodes[i];
        const auto eid = EID::from_std_string(std::string_view(xml.child_value("eid")));
        const auto found = std::ranges::find_if(scores, [&](const Score *score) { return score->eid() == eid; });
        require(eid.is_valid() && found != scores.end(), "Missing 5.00 score identity");
        ScoreIndex index{**found, poll, {}, {}, {}, {}, {}};
        index_score(index, xml);
        std::vector<Pair> pairs;
        plan_score(index, xml, pairs, issues);
        if (!issues.empty()) return issues;
        emit_pairs(index, pairs, bodies);
        for (auto map = xml.child("SpannerMap"); map;) {
            const auto next = map.next_sibling("SpannerMap");
            require(!map.first_child(), "Unmapped 5.00 spanner body");
            xml.remove_child(map);
            map = next;
        }
    }
    rebase_links(root, bodies, poll);
    return issues;
}

} // namespace engrave::write470
