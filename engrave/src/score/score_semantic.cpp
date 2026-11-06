// SPDX-License-Identifier: GPL-3.0-only
#include "score_semantic.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <tuple>

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/tempotimeline.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/textlinebase.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/types/typesconv.h"

using namespace mu::engraving;
using json = nlohmann::ordered_json;

namespace music {
namespace {

std::string str(const muse::String &value) { return value.to_std_string(); }

std::string str(muse::AsciiStringView value) { return std::string(value.ascii(), value.size()); }

// Instrument names are stored as rich text; formatting tags are appearance.
std::string plain(const muse::String &rich) {
    muse::String text;
    bool tag = false;
    for (size_t i = 0; i < rich.size(); ++i) {
        const char16_t c = rich.at(i).unicode();
        if (c == u'<')
            tag = true;
        else if (c == u'>')
            tag = false;
        else if (!tag)
            text.append(rich.at(i));
    }
    return TextBase::un_escape(text).to_std_string();
}

std::string fraction(const Fraction &f) { return std::to_string(f.numerator()) + "/" + std::to_string(f.denominator()); }

// Same names as the MSCX grace tags parsed by scripts/test_roundtrip.py.
const char *grace_kind(NoteType type) {
    switch (type) {
        case NoteType::NORMAL: return "none";
        case NoteType::ACCIACCATURA: return "acciaccatura";
        case NoteType::APPOGGIATURA: return "appoggiatura";
        case NoteType::GRACE4: return "grace4";
        case NoteType::GRACE16: return "grace16";
        case NoteType::GRACE32: return "grace32";
        case NoteType::GRACE8_AFTER: return "grace8after";
        case NoteType::GRACE16_AFTER: return "grace16after";
        case NoteType::GRACE32_AFTER: return "grace32after";
        default: return "invalid";
    }
}

const char *staff_group(StaffGroup group) {
    switch (group) {
        case StaffGroup::STANDARD: return "pitched";
        case StaffGroup::PERCUSSION: return "percussion";
        case StaffGroup::TAB: return "tablature";
    }
    return "unknown";
}

// Untranslated subtype label ("Line break", "D.C. al Fine", "8va alta", ...).
std::string subtype(const EngravingItem *item) { return str(item->subtype_user_name().str); }

json text_item(const EngravingItem *item) {
    json out;
    out["type"] = str(TConv::to_xml(item->type()));
    out["tick"] = item->tick().ticks();
    out["track"] = static_cast<int>(item->track());
    // Dynamics omit the UI tooltip subtype label ("f (Forte)" vs "f (forte)");
    // the dynamic field below is the stable identity.
    if (!item->is_dynamic()) {
        const std::string sub = subtype(item);
        if (!sub.empty()) {
            out["subtype"] = sub;
        }
    }
    // Dynamics render as SMuFL glyphs; their kind is recorded below instead.
    if (item->is_text_base() && !item->is_dynamic()) {
        out["text"] = str(to_text_base(item)->plain_text());
    }
    if (item->is_tempo_text()) {
        // Quarter-note BPM rounded like test_roundtrip.py rounds <tempo> (beats per second there).
        out["bpm"] = std::round(to_tempo_text(item)->tempo().val * 60.0 * 1e4) / 1e4;
    } else if (item->is_dynamic()) {
        out["dynamic"] = str(TConv::to_xml(to_dynamic(item)->dynamic_type()));
    } else if (item->is_harmony()) {
        out["harmony"] = str(to_harmony(item)->harmony_name());
    }
    return out;
}

// Measure-level elements (measure->el()): system-level items omit track when not
// meaningful (e.g. LayoutBreak track is unassigned -1 in 4.7.6 vs 0 in 5.0);
// staff-level items retain track to identify the staff.
json measure_element_json(const EngravingItem *item) {
    json out = text_item(item);
    switch (item->type()) {
        case ElementType::LAYOUT_BREAK:
            // LayoutBreak is a system-level break (line/page/section); track is not
            // meaningful (-1 in 4.7.6 vs 0 in 5.0).
            out.erase("track");
            break;
        case ElementType::STAFFTYPE_CHANGE:
            // StaffTypeChange is staff-level; track identifies the affected staff.
            break;
        case ElementType::SPACER:
            // Spacer is staff-level; track identifies the affected staff.
            break;
        case ElementType::JUMP:
        case ElementType::MARKER:
            // Repeat jumps and markers are anchored to a staff (typically track 0).
            break;
        default:
            // Other system-level elements without a meaningful staff omit track.
            if (item->system_flag() && !item->flag(ElementFlag::ON_STAFF)) {
                out.erase("track");
            }
            break;
    }
    return out;
}

json note_json(const Note *note) {
    json out;
    out["pitch"] = note->pitch();
    out["tpc1"] = note->tpc1();
    out["tpc2"] = note->tpc2();
    out["head"] = str(TConv::to_xml(note->head_group()));
    out["headType"] = str(TConv::to_xml(note->head_type()));
    if (note->fret() >= 0 || note->string() >= 0) {
        out["fret"] = note->fret();
        out["string"] = note->string();
    }
    if (note->ghost()) {
        out["ghost"] = true;
    }
    if (!note->play()) {
        out["play"] = false;
    }
    if (const Tie *tie = note->tie_for()) {
        if (tie->type() == ElementType::LAISSEZ_VIB) {
            out["laissezVib"] = true;
        } else if (const Note *end = tie->end_note()) {
            out["tieTo"] = {end->tick().ticks(), static_cast<int>(end->track()), end->pitch()};
        } else {
            out["tieTo"] = nullptr; // partial tie (e.g. into a volta)
        }
    }
    json fingerings = json::array();
    json elements = json::array(); // symbols, bends, note-attached text, ...
    for (const EngravingItem *e : const_cast<Note *>(note)->el()) {
        if (e->is_fingering()) {
            fingerings.push_back(str(to_text_base(e)->plain_text()));
        } else if (!e->generated()) {
            json item = text_item(e);
            item.erase("tick");
            item.erase("track");
            elements.push_back(item);
        }
    }
    if (!fingerings.empty()) {
        out["fingerings"] = fingerings;
    }
    if (!elements.empty()) {
        out["elements"] = elements;
    }
    return out;
}

json chord_rest_json(const ChordRest *cr) {
    json out;
    out["kind"] = cr->is_chord() ? "chord" : cr->is_measure_repeat() ? "measureRepeat" : "rest";
    out["tick"] = cr->tick().ticks();
    out["ticks"] = cr->is_chord() && to_chord(cr)->is_grace() ? 0 : cr->actual_ticks().ticks();
    out["duration"] = str(TConv::to_xml(cr->duration_type().type()));
    if (cr->dots()) {
        out["dots"] = cr->dots();
    }
    if (const Tuplet *tuplet = cr->tuplet()) {
        out["tuplet"] = fraction(tuplet->ratio());
    }
    if (cr->is_chord()) {
        const Chord *chord = to_chord(cr);
        out["grace"] = grace_kind(chord->note_type());
        json notes = json::array();
        for (const Note *note : chord->notes()) {
            notes.push_back(note_json(note));
        }
        out["notes"] = notes;
        json articulations = json::array();
        for (const Articulation *a : chord->articulations()) {
            articulations.push_back(str(TConv::to_xml(a->sym_id())));
        }
        if (!articulations.empty()) {
            out["articulations"] = articulations;
        }
        // Chord lines, arpeggios, tremolos, stems set by the user.
        json elements = json::array();
        for (const EngravingItem *e : const_cast<Chord *>(chord)->el()) {
            if (!e->generated()) {
                elements.push_back(str(TConv::to_xml(e->type())) + ":" + subtype(e));
            }
        }
        if (chord->arpeggio()) {
            elements.push_back("Arpeggio:" + subtype(chord->arpeggio()));
        }
        if (chord->tremolo_single_chord()) {
            elements.push_back("TremoloSingleChord:" + subtype(chord->tremolo_single_chord()));
        }
        if (chord->tremolo_two_chord()) {
            elements.push_back("TremoloTwoChord:" + subtype(chord->tremolo_two_chord()));
        }
        if (!elements.empty()) {
            out["elements"] = elements;
        }
        if (chord->stem_direction() != DirectionV::AUTO) {
            out["stemDirection"] = str(TConv::to_xml(chord->stem_direction()));
        }
    } else if (!to_rest(cr)->visible()) {
        out["visible"] = false;
    }
    json lyrics = json::array();
    for (const Lyrics *l : cr->lyrics()) {
        lyrics.push_back({l->verse(), str(l->plain_text())});
    }
    if (!lyrics.empty()) {
        out["lyrics"] = lyrics;
    }
    return out;
}

json spanner_json(const Spanner *sp) {
    json out;
    out["type"] = str(TConv::to_xml(sp->type()));
    out["tick"] = sp->tick().ticks();
    out["tick2"] = sp->tick2().ticks();
    out["track"] = static_cast<int>(sp->track());
    out["track2"] = static_cast<int>(sp->track2());
    const std::string sub = subtype(sp);
    if (!sub.empty()) {
        out["subtype"] = sub;
    }
    out["anchor"] = static_cast<int>(sp->anchor());
    if (sp->is_volta()) {
        out["endings"] = to_volta(sp)->endings();
    }
    if (sp->is_text_line_base()) {
        const std::string begin = str(to_text_line_base(sp)->begin_text());
        if (!begin.empty()) {
            out["beginText"] = begin;
        }
    }
    return out;
}

json part_json(const Part *part) {
    json out;
    out["id"] = part->id().to_std_string();
    out["partName"] = plain(part->part_name());
    out["longName"] = plain(part->long_name());
    out["shortName"] = plain(part->short_name());
    out["instrumentId"] = str(part->instrument_id());
    out["show"] = part->show();
    const Interval transpose = part->instrument()->transpose();
    out["transpose"] = {transpose.chromatic, transpose.diatonic};
    json staves = json::array();
    for (const Staff *staff : part->staves()) {
        const StaffType *type = staff->staff_type(Fraction(0, 1));
        staves.push_back({
            {"index", static_cast<int>(staff->idx())},
            {"group", staff_group(type->group())},
            {"lines", type->lines()},
            {"invisible", type->invisible()},
        });
    }
    out["staves"] = staves;
    return out;
}

// Annotation and spanner containers keep insertion order, which a save and
// reload does not preserve; order by position, then by content.
void sort_items(json &items) {
    std::sort(items.begin(), items.end(), [](const json &a, const json &b) {
        const auto key = [](const json &v) { return std::make_tuple(v.value("tick", 0), v.value("track", 0), v.dump()); };
        return key(a) < key(b);
    });
}

// Musical content of one score (master or excerpt).
json score_body(Score *score) {
    json out;
    json parts = json::array();
    for (const Part *part : score->parts()) {
        parts.push_back(part_json(part));
    }
    out["parts"] = parts;

    json measures = json::array();
    json signatures = json::array(); // clefs, key and time signatures per staff
    json events = json::array();     // [staff][voice] -> chord/rest list
    json annotations = json::array();
    json segment_elements = json::array();
    const size_t nstaves = score->nstaves();
    for (size_t s = 0; s < nstaves; ++s) {
        events.push_back(json::array({json::array(), json::array(), json::array(), json::array()}));
    }

    for (const Measure *m = score->first_measure(); m; m = m->next_measure()) {
        json measure;
        measure["tick"] = m->tick().ticks();
        measure["ticks"] = m->ticks().ticks();
        measure["timesig"] = fraction(m->timesig());
        measure["startRepeat"] = m->repeat_start();
        measure["endRepeat"] = m->repeat_end() ? m->repeat_count() : 0;
        if (m->flag(ElementFlag::EXCLUDE_FROM_NUMBERING)) {
            // 5.0 renamed the "irregular" measure flag (Pid::IRREGULAR) to this.
            measure["irregular"] = true;
        }
        json elements = json::array();
        for (const EngravingItem *e : const_cast<Measure *>(m)->el()) {
            if (!e->generated()) {
                elements.push_back(measure_element_json(e));
            }
        }
        if (!elements.empty()) {
            sort_items(elements);
            measure["elements"] = elements;
        }
        measures.push_back(measure);

        for (const Segment *seg = m->first(); seg; seg = seg->next()) {
            for (const EngravingItem *a : seg->annotations()) {
                if (!a->generated()) {
                    annotations.push_back(text_item(a));
                }
            }
            for (size_t track = 0; track < nstaves * ::mu::engraving::voices; ++track) {
                const EngravingItem *e = seg->element(static_cast<track_idx_t>(track));
                if (!e || e->generated()) {
                    continue;
                }
                const size_t staff = track / ::mu::engraving::voices;
                if (e->is_chord_rest()) {
                    const ChordRest *cr = to_chord_rest(e);
                    json &voice = events[staff][track % ::mu::engraving::voices];
                    if (cr->is_chord()) {
                        for (const Chord *g : to_chord(cr)->grace_notes_before()) {
                            voice.push_back(chord_rest_json(g));
                        }
                    }
                    voice.push_back(chord_rest_json(cr));
                    if (cr->is_chord()) {
                        for (const Chord *g : to_chord(cr)->grace_notes_after()) {
                            voice.push_back(chord_rest_json(g));
                        }
                    }
                } else if (e->is_clef()) {
                    signatures.push_back({{"type", "Clef"},
                                          {"tick", seg->tick().ticks()},
                                          {"staff", staff},
                                          {"value", str(TConv::to_xml(to_clef(e)->clef_type()))}});
                } else if (e->is_key_sig()) {
                    const KeySig *k = to_key_sig(e);
                    signatures.push_back({{"type", "KeySig"},
                                          {"tick", seg->tick().ticks()},
                                          {"staff", staff},
                                          {"value", static_cast<int>(k->concert_key())},
                                          {"key", static_cast<int>(k->key())}});
                } else if (e->is_time_sig()) {
                    signatures.push_back(
                        {{"type", "TimeSig"}, {"tick", seg->tick().ticks()}, {"staff", staff}, {"value", fraction(to_time_sig(e)->sig())}});
                } else {
                    // Breaths, user barlines, ambitus, ...
                    segment_elements.push_back(text_item(e));
                }
            }
        }
    }
    out["measures"] = measures;
    out["signatures"] = signatures;
    out["events"] = events;
    sort_items(annotations);
    out["annotations"] = annotations;
    out["segmentElements"] = segment_elements;

    json spanners = json::array();
    for (const auto &entry : score->spanner_map().map()) {
        const Spanner *sp = entry.second;
        if (!sp->generated()) {
            spanners.push_back(spanner_json(sp));
        }
    }
    sort_items(spanners);
    out["spanners"] = spanners;
    return out;
}

} // namespace

json semantic_dump(MasterScore *score) {
    json out;
    out["schema"] = 1;
    json meta = json::object();
    for (const auto &[name, value] : score->meta_tags()) {
        if (name != u"creationDate" && name != u"platform") {
            meta[str(name)] = str(value);
        }
    }
    out["metadata"] = meta;

    // 5.0 derives tempo from an automation curve; the flattened timeline is the
    // replacement for the 4.x TempoMap it replaced: one point per tempo marking,
    // per resampled gradual-change step and per pause, keyed by score tick.
    json tempo = json::array();
    for (const TempoTimePoint &point : score->tempo_timeline(false).points()) {
        tempo.push_back({point.utick, std::round(point.bps * 60.0 * 1e4) / 1e4});
    }
    out["tempoMap"] = tempo;

    score->set_expand_repeats(true);
    json playback = json::array();
    for (const RepeatSegment *rs : score->repeat_list()) {
        playback.push_back({rs->tick, rs->len(), rs->utick});
    }
    out["playbackOrder"] = playback;

    out.update(score_body(score));

    json excerpts = json::array();
    for (const Excerpt *excerpt : score->excerpts()) {
        json e;
        e["name"] = str(excerpt->name());
        json part_ids = json::array();
        for (const Part *p : excerpt->parts()) {
            part_ids.push_back(p->id().to_std_string());
        }
        e["partIds"] = part_ids;
        if (Score *es = excerpt->excerpt_score()) {
            e["score"] = score_body(es);
        }
        excerpts.push_back(e);
    }
    out["excerpts"] = excerpts;
    return out;
}

} // namespace music
