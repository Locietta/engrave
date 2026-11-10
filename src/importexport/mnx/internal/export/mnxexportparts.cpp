/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/interval.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/spannermap.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/volta.h"
#include "engraving/editing/transpose.h"
#include "engraving/types/symnames.h"
#include "internal/shared/mnxtypesconv.h"
#include "log.h"
#include "mnxexporter.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
//---------------------------------------------------------
//   exportDrumsetKit
//---------------------------------------------------------

void MnxExporter::export_drumset_kit(const Part *part, const Instrument *instrument, model::Part &mnx_part) {
    IF_ASSERT_FAILED(part && instrument) { return; }
    IF_ASSERT_FAILED(instrument->use_drumset()) { return; }

    const Drumset *drumset = instrument->drumset();
    IF_ASSERT_FAILED(drumset) { return; }

    const Staff *staff_for_kit = part->staff(0);
    const Measure *first_measure = score_ ? score_->first_measure() : nullptr;
    const StaffType *staff_type = nullptr;
    if (staff_for_kit && first_measure) {
        staff_type = staff_for_kit->staff_type(first_measure->tick());
    }
    if (!staff_type) {
        staff_type = StaffType::preset(StaffTypes::PERC_DEFAULT);
    }
    const int middle_line = staff_type ? staff_type->middle_line() : 4;

    auto &mnx_kit = ensure(mnx_part.kit);
    auto &mnx_sounds = ensure(root_.global.sounds);

    for (int pitch = 0; pitch < ::mu::engraving::drum_instruments; ++pitch) {
        if (!drumset->is_valid(pitch)) {
            continue;
        }

        const std::string kit_id = "drum-midi-" + std::to_string(pitch);
        const int line = drumset->line(pitch);
        const int staff_position = middle_line - line;

        model::KitComponent kit_component;
        kit_component.staff_position = staff_position;
        String name = drumset->name(pitch);
        if (name.is_empty()) {
            name = String(u"Percussion note");
        }
        kit_component.name = name.to_std_string();

        const std::string sound_id = "drum-midi-" + std::to_string(pitch);
        kit_component.sound = model::Id(sound_id);
        mnx_kit.emplace_back(kit_id, std::move(kit_component));
        if (std::ranges::none_of(mnx_sounds, [&](const auto &entry) { return entry.first == sound_id; })) {
            model::Sound sound;
            sound.name = name.to_std_string();
            sound.midi_number = pitch;
            mnx_sounds.emplace_back(sound_id, std::move(sound));
        }
    }
}

//---------------------------------------------------------
//   appendClefsForMeasure
//   export clefs for a single measure
//---------------------------------------------------------

static void append_clefs_for_measure(const Part *part, const Measure *measure, model::PartMeasure &mnx_measure) {
    if (part && part->instrument() && part->instrument()->use_drumset()) {
        /// @todo Export percussion and tab staves/clefs when MNX supports them.
        return;
    }

    const bool is_first_measure = (measure->prev_measure() == nullptr);
    const size_t staves = part->nstaves();
    const TrackRange track_range = part->track_range();

    constexpr SegmentType cleft_types = SegmentType::CLEF | SegmentType::HEADER_CLEF;
    for (Segment *segment = measure->first(cleft_types); segment; segment = segment->next(cleft_types)) {
        const SegmentType segment_type = segment->segment_type();
        if ((segment_type == SegmentType::HEADER_CLEF) && !is_first_measure) {
            continue;
        }

        const Fraction r_tick = segment->rtick();
        for (track_idx_t track = track_range.start_track; track < track_range.end_track; track += ::mu::engraving::voices) {
            Clef *clef = to_clef(segment->element(track));
            if (!clef || clef->is_courtesy()) {
                continue;
            }
            if (clef->generated() && !(r_tick.is_zero() && is_first_measure)) {
                continue;
            }

            const auto mnx_clef_value = to_mnx_clef(clef->clef_type());
            if (!mnx_clef_value) {
                LOGW() << "Skipping nsupported clef type in MNX export: " << int(clef->clef_type());
                continue;
            }

            model::PositionedClef mnx_clef;
            mnx_clef.clef = *mnx_clef_value;
            if (staves > 1) {
                const size_t staff_idx = (track - track_range.start_track) / ::mu::engraving::voices;
                mnx_clef.staff = static_cast<int>(staff_idx + 1);
            }
            if (r_tick.is_not_zero()) {
                mnx_clef.position = model::RhythmicPosition{.fraction = to_mnx_fraction(r_tick)};
            }
            ensure(mnx_measure.clefs).push_back(std::move(mnx_clef));
        }
    }
}

//---------------------------------------------------------
//   findFirstChordRest
//---------------------------------------------------------

static const ChordRest *find_first_chord_rest(const Slur *s) {
    /// @todo this function is copied from same function in musicxml and should probably be a method on Slur

    const EngravingItem *e1 = s->start_element();
    if (!e1 || !(e1->is_chord_rest())) {
        LOGD("no valid start element for slur %p", s);
        return nullptr;
    }

    const EngravingItem *e2 = s->end_element();
    if (!e2 || !(e2->is_chord_rest())) {
        LOGD("no valid end element for slur %p", s);
        return nullptr;
    }

    if (e1->tick() < e2->tick()) {
        return to_chord_rest(e1);
    } else if (e1->tick() > e2->tick()) {
        return to_chord_rest(e2);
    }

    if (e1->is_rest() || e2->is_rest()) {
        return nullptr;
    }

    const Chord *c1 = to_chord(e1);
    const Chord *c2 = to_chord(e2);

    // c1->tick() == c2->tick()
    if (!c1->is_grace() && !c2->is_grace()) {
        // slur between two regular notes at the same tick
        // probably shouldn't happen but handle just in case
        LOGD("invalid slur between chords %p and %p at tick %d", c1, c2, c1->tick().ticks());
        return 0;
    } else if (c1->is_grace_before() && !c2->is_grace_before()) {
        return c1; // easy case: c1 first
    } else if (c1->is_grace_after() && !c2->is_grace_after()) {
        return c2; // easy case: c2 first
    } else if (c2->is_grace_before() && !c1->is_grace_before()) {
        return c2; // easy case: c2 first
    } else if (c2->is_grace_after() && !c1->is_grace_after()) {
        return c1; // easy case: c1 first
    } else {
        // both are grace before or both are grace after -> compare grace indexes
        // (note: higher means closer to the non-grace chord it is attached to)
        if ((c1->is_grace_before() && c1->grace_index() < c2->grace_index()) ||
            (c1->is_grace_after() && c1->grace_index() > c2->grace_index())) {
            return c1;
        } else {
            return c2;
        }
    }
}

static model::MultiStaffOrientation mnx_multi_staff_orient_from_direction(const EngravingItem *item) {
    switch (item->get_property(Pid::DIRECTION).value<DirectionV>()) {
        case DirectionV::UP: return model::MultiStaffOrientation::ABOVE;
        case DirectionV::DOWN: return model::MultiStaffOrientation::BELOW;
        case DirectionV::AUTO:
            if (item->part()->staves().size() > 1) {
                if (item->get_property(Pid::CENTER_BETWEEN_STAVES) == AutoOnOff::ON) {
                    return model::MultiStaffOrientation::BETWEEN;
                }
            }
    }

    return model::MultiStaffOrientation::AUTO;
}

static void export_mnx_voice_assignment(model::DynamicGroup &mnx_dynamic, int mnx_staff_num, VoiceAssignment voice_assignment,
                                        track_idx_t cur_track_idx) {
    switch (voice_assignment) {
        case VoiceAssignment::CURRENT_VOICE_ONLY:
            mnx_dynamic.staff = mnx_staff_num;
            mnx_dynamic.voice = make_mnx_voice_id_from_track(mnx_staff_num, cur_track_idx);
            break;
        case VoiceAssignment::ALL_VOICE_IN_STAFF: mnx_dynamic.staff = mnx_staff_num; break;
        case VoiceAssignment::ALL_VOICE_IN_INSTRUMENT: break;
    }
}

//---------------------------------------------------------
//   createVolta
//   export volta as an MNX ending
//---------------------------------------------------------

void MnxExporter::create_volta(const Volta *volta) {
    IF_ASSERT_FAILED(volta) { return; }

    Measure *start_measure = to_measure(volta->start_element());
    IF_ASSERT_FAILED(start_measure) {
        LOGW() << "Skipping volta with missing start measure.";
        return;
    }

    const size_t mnx_measure_index = mnx_measure_index_from_measure(start_measure);
    auto &mnx_measure = root_.global.measures.at(mnx_measure_index);

    const Fraction end_tick = volta->tick2() - Fraction::eps();
    Measure *end_measure = start_measure->score()->tick2measure(end_tick);
    IF_ASSERT_FAILED(end_measure) {
        LOGW() << "Skipping volta with missing end measure.";
        return;
    }
    IF_ASSERT_FAILED(end_measure->tick() >= start_measure->tick()) {
        LOGW() << "Skipping volta with end measure before start measure.";
        return;
    }

    int duration = 0;
    bool found_end = false;
    for (const Measure *cursor = start_measure; cursor; cursor = cursor->next_measure()) {
        ++duration;
        if (cursor == end_measure) {
            found_end = true;
            break;
        }
    }
    IF_ASSERT_FAILED(found_end) {
        LOGW() << "Skipping volta with unsupported end measure.";
        return;
    }

    auto &mnx_ending = ensure(mnx_measure.ending);
    mnx_ending.duration = duration;
    mnx_ending.open = volta->volta_type() == Volta::Type::OPEN;
    const std::vector<int> endings = volta->endings();
    if (!endings.empty()) {
        mnx_ending.numbers = endings;
    }
}

//---------------------------------------------------------
//   createSlur
//   export a single slur spanner
//---------------------------------------------------------

void MnxExporter::create_slur(const Slur *s) {
    IF_ASSERT_FAILED(s) { return; }
    IF_ASSERT_FAILED(s->start_element() && s->end_element()) {
        LOGW() << "Skipping slur with missing endpoints.";
        return;
    }
    if (!s->start_element()->is_chord_rest() || !s->end_element()->is_chord_rest()) {
        LOGW() << "Skipping slur not attached to chord-rests.";
        return;
    }

    if (const ChordRest *start_cr = find_first_chord_rest(s)) {
        const ChordRest *end_cr = start_cr == s->start_element() ? to_chord_rest(s->end_element()) : to_chord_rest(s->start_element());
        model::Event *mnx_event = mnx_event_from_cr(start_cr);
        model::Event *end_event = end_cr ? mnx_event_from_cr(end_cr) : nullptr;
        if (mnx_event && end_event) {
            model::Slur mnx_slur;
            mnx_slur.target = model::Id(end_cr->eid().to_std_string());
            mnx_slur.line_type = to_mnx_slur_line_type(s->style_type());
            if (s->slur_direction() != DirectionV::AUTO) {
                mnx_slur.side = s->up() ? model::SlurSide::UP : model::SlurSide::DOWN;
            }
            ensure(mnx_event->slurs).push_back(std::move(mnx_slur));
            /// @todo export side and sideEnd in opposite directions, if/when MuseScore supports it.
            /// @todo endNote and startNote are not supported by MuseScore (yet?)
        }
    }
}

//---------------------------------------------------------
//   createHairpin
//   export a single hairpin spanner
//---------------------------------------------------------

void MnxExporter::create_hairpin(const Hairpin *hairpin) {
    IF_ASSERT_FAILED(hairpin) { return; }

    if (hairpin->is_line_type()) {
        LOGI() << "Line-type hairpins not yet supported by MNX export.";
        return;
    }

    const Staff *start_staff = score_->staff(track2staff(hairpin->track()));
    const Staff *end_staff = score_->staff(track2staff(hairpin->effective_track2()));
    IF_ASSERT_FAILED(start_staff && start_staff->part()) {
        LOGW() << "Skipping hairpin with missing start staff or part context.";
        return;
    }

    if (!end_staff) {
        LOGW() << "Hairpin has no end staff and will be exported using the start staff/voice only.";
    }

    const Measure *start_measure = score_->tick2measure(hairpin->tick());
    IF_ASSERT_FAILED(start_measure) {
        LOGW() << "Skipping hairpin with missing start measure for tick " << hairpin->tick().ticks() << ".";
        return;
    }

    const Fraction adjusted_end_tick = hairpin->tick2() - Fraction::eps();
    Measure *end_measure = score_->tick2measure(adjusted_end_tick);
    IF_ASSERT_FAILED(end_measure) {
        LOGW() << "Skipping hairpin with missing end measure.";
        return;
    }
    IF_ASSERT_FAILED(end_measure->tick() >= start_measure->tick()) {
        LOGW() << "Skipping hairpin with end measure before start measure.";
        return;
    }

    const size_t mnx_measure_index = mnx_measure_index_from_measure(start_measure);
    const auto [mnx_part_idx, mnx_staff_num] = mnx_part_staff_from_staff_idx(start_staff->idx());
    auto &mnx_measure = root_.parts.at(mnx_part_idx).measures.at(mnx_measure_index);

    const Fraction start_offset = hairpin->tick() - start_measure->tick();
    const Fraction end_offset = hairpin->tick2() - end_measure->tick();
    const std::string end_measure_id = get_or_assign_eid(const_cast<Measure *>(end_measure)).to_std_string();

    model::DynamicGroup mnx_hairpin;
    mnx_hairpin.type = model::DynamicGroupType::GRADUAL;
    mnx_hairpin.wedge_type =
        hairpin->hairpin_type() == HairpinType::CRESC_HAIRPIN ? model::WedgeType::INCREASING : model::WedgeType::DECREASING;
    mnx_hairpin.position = model::RhythmicPosition{.fraction = to_mnx_fraction(start_offset.reduced())};
    mnx_hairpin.end = model::MeasureRhythmicPosition{.measure = model::Id(end_measure_id),
                                                     .position = {.fraction = to_mnx_fraction(end_offset.reduced())}};

    if (end_staff && end_staff->idx() != start_staff->idx()) {
        const auto [mnx_end_part_idx, mnx_end_staff_num] = mnx_part_staff_from_staff_idx(end_staff->idx());
        if (mnx_end_part_idx == mnx_part_idx) {
            mnx_hairpin.staff_end = mnx_end_staff_num;
        } else {
            LOGW() << "Hairpin ends in a different part and will be exported using the start staff/voice only.";
        }
    }

    set_unless_default(mnx_hairpin.orient, mnx_multi_staff_orient_from_direction(hairpin), model::MultiStaffOrientation::AUTO);
    export_mnx_voice_assignment(mnx_hairpin, mnx_staff_num, hairpin->voice_assignment(), hairpin->track());
    ensure(mnx_measure.dynamics).push_back(std::move(mnx_hairpin));
}

//---------------------------------------------------------
//   createOttava
//   export a single ottava spanner
//---------------------------------------------------------

void MnxExporter::create_ottava(const Ottava *ottava) {
    IF_ASSERT_FAILED(ottava) { return; }

    if (!ottava->start_element() || !ottava->end_element()) {
        LOGW() << "Skipping ottava with missing endpoints.";
        return;
    }

    const std::optional<model::OttavaAmount> amount = to_mnx_ottava_amount(ottava->ottava_type());
    if (!amount) {
        LOGW() << "Skipping unsupported ottava type in MNX export: " << int(ottava->ottava_type());
        return;
    }

    const EngravingItem *start_element = ottava->start_element();
    const EngravingItem *end_element = ottava->end_element();

    const Measure *start_measure = start_element->find_measure();
    const Measure *end_measure = end_element->find_measure();

    if (!start_measure) {
        LOGW() << "Skipping ottava with missing start measure.";
        return;
    }

    if (!end_measure) {
        // Adjust end measure by tick if end element is generated outside normal measure lookup.
        const Fraction adjusted_end_tick = ottava->tick2() - Fraction::eps();
        end_measure = start_measure->score()->tick2measure(adjusted_end_tick);
        if (!end_measure) {
            LOGW() << "Skipping ottava with missing end measure.";
            return;
        }
    }

    const Fraction start_offset = ottava->tick() - start_measure->tick();
    const Fraction adjusted_end_tick =
        ottava->end_element()->is_chord_rest() ? to_chord_rest(ottava->end_element())->tick() : ottava->tick2();
    const Fraction end_offset = adjusted_end_tick - end_measure->tick();

    // Resolve part and staff so we can attach the ottava to the correct part measure.
    const Staff *staff = start_element->staff();
    IF_ASSERT_FAILED(staff && staff->part()) {
        LOGW() << "Skipping ottava with missing staff or part context.";
        return;
    }

    const Part *part = staff->part();
    const auto [mnx_part_idx, mnx_staff_num] = mnx_part_staff_from_staff_idx(staff->idx());
    const size_t mnx_measure_index = mnx_measure_index_from_measure(start_measure);
    auto &mnx_measure = root_.parts.at(mnx_part_idx).measures.at(mnx_measure_index);
    const std::string end_measure_id = get_or_assign_eid(const_cast<Measure *>(end_measure)).to_std_string();

    model::Ottava mnx_ottava;
    mnx_ottava.value = amount.value();
    mnx_ottava.position = model::RhythmicPosition{.fraction = to_mnx_fraction(start_offset.reduced())};
    mnx_ottava.end = model::MeasureRhythmicPosition{.measure = model::Id(end_measure_id),
                                                    .position = {.fraction = to_mnx_fraction(end_offset.reduced())}};

    // toChord only asserts in debug builds, so the type has to be checked first: an ottava
    // ending on a rest would otherwise yield a bogus Chord* and read past it here.
    if (ottava->end_element()->is_chord()) {
        const Chord *end_c = to_chord(ottava->end_element());
        if (!end_c->grace_notes_before().empty()) {
            // Ensure grace notes before the end of the ottava are included.
            mnx_ottava.end.position.grace_index = 0;
        }
    }

    if (part && part->nstaves() > 1) {
        mnx_ottava.staff = mnx_staff_num;
    }
    ensure(mnx_measure.ottavas).push_back(std::move(mnx_ottava));

    /// @todo map track-specific ottava to ottava.voice() if MuseScore decides to implements it.
}

//---------------------------------------------------------
//   exportSpanners
//---------------------------------------------------------

void MnxExporter::export_spanners() {
    // All content exists now, so its events have their final addresses.
    events_by_id_.clear();
    const auto index_events = [this](this const auto &self, auto &events) -> void {
        for (auto &item : events) {
            if constexpr (std::is_same_v<std::decay_t<decltype(item)>, model::Event>) {
                if (item.id) events_by_id_.emplace(std::string(*item.id), &item);
            } else {
                std::visit(
                    [&]<class T>(T &value) {
                        if constexpr (std::is_same_v<T, model::Event>) {
                            if (value.id) events_by_id_.emplace(std::string(*value.id), &value);
                        } else if constexpr (std::is_same_v<T, model::Space>) {
                        } else {
                            self(value.content);
                        }
                    },
                    item);
            }
        }
    };
    for (auto &part : root_.parts) {
        for (auto &measure : part.measures) {
            for (auto &sequence : measure.sequences) {
                index_events(sequence.content);
            }
        }
    }
    const SpannerMap::IntervalList &spanners =
        score_->spanner_map().find_overlapping(score_->first_measure()->tick().ticks(), score_->last_measure()->end_tick().ticks());

    for (const auto &entry : spanners) {
        Spanner *sp = entry.value;
        if (sp->generated()) {
            continue;
        }
        switch (sp->type()) {
            case ElementType::HAIRPIN: create_hairpin(to_hairpin(sp)); break;
            case ElementType::OTTAVA: create_ottava(to_ottava(sp)); break;
            case ElementType::SLUR: create_slur(to_slur(sp)); break;
            case ElementType::VOLTA: create_volta(to_volta(sp)); break;
            default: break;
        }
    }
}

//---------------------------------------------------------
//   createArpeggios
//   emit MNX measure-level arpeggios and non-arpeggios
//---------------------------------------------------------

void MnxExporter::create_arpeggios(const Part *part, const Measure *measure, model::PartMeasure &mnx_measure) {
    IF_ASSERT_FAILED(part && measure) { return; }

    const TrackRange track_range = part->track_range();
    std::unordered_set<const Arpeggio *> exported_arpeggios;

    auto is_exported_chord = [this](const Chord *chord) { return chord && cr_to_mnx_event_.find(chord) != cr_to_mnx_event_.end(); };

    auto is_exportable_endpoint = [&](const Note *note) {
        if (!note || !is_exported_chord(note->chord())) {
            return false;
        }
        const Staff *staff = note->staff();
        if (staff && staff->is_drum_staff(note->tick())) {
            return pitch_is_valid(note->pitch());
        }
        return to_mnx_pitch(note).has_value();
    };

    auto find_bottom_chord = [](const Arpeggio *arpeggio) -> Chord * {
        Chord *chord = arpeggio ? arpeggio->chord() : nullptr;
        Segment *segment = chord ? chord->segment() : nullptr;
        if (!chord || !segment) {
            return nullptr;
        }

        Chord *bottom_chord = chord;
        for (track_idx_t track = arpeggio->track(); track <= arpeggio->end_track(); ++track) {
            EngravingItem *item = segment->element(track);
            if (!item || !item->is_chord()) {
                continue;
            }
            Chord *span_chord = to_chord(item);
            if (span_chord == chord || span_chord->span_arpeggio() == arpeggio) {
                bottom_chord = span_chord;
            }
        }
        return bottom_chord;
    };

    auto mnx_position = [](const MnxChordTargetPosition &position) {
        model::RhythmicPosition result{.fraction = position.fraction};
        if (position.grace_index) {
            result.grace_index = static_cast<int>(position.grace_index.value());
        }
        return result;
    };

    auto process_arpeggio = [&](Chord *chord) {
        IF_ASSERT_FAILED(chord) { return; }

        Arpeggio *arpeggio = chord->arpeggio();
        if (!arpeggio || !exported_arpeggios.insert(arpeggio).second) {
            return;
        }
        if (!is_exported_chord(chord)) {
            LOGW() << "Skipping arpeggio whose chord was not exported.";
            return;
        }

        Chord *bottom_chord = chord;
        if (chord->is_grace()) {
            if (arpeggio->span() != 1) {
                LOGW() << "Skipping multi-track grace-note arpeggio.";
                return;
            }
        } else {
            bottom_chord = find_bottom_chord(arpeggio);
            if (!bottom_chord || bottom_chord->track() < chord->track()) {
                LOGW() << "Skipping arpeggio with invalid span.";
                return;
            }
        }

        if (!is_exported_chord(bottom_chord)) {
            LOGW() << "Skipping arpeggio whose endpoint chord was not exported.";
            return;
        }

        Note *top_note = chord->up_note();
        Note *bottom_note = bottom_chord->down_note();
        if (!is_exportable_endpoint(top_note) || !is_exportable_endpoint(bottom_note)) {
            LOGW() << "Skipping arpeggio with non-exportable endpoint note.";
            return;
        }

        const std::optional<MnxChordTargetPosition> position = mnx_chord_target_position(chord, measure);
        if (!position) {
            return;
        }
        const model::IdPair span{.end = model::Id(get_or_assign_eid(bottom_note).to_std_string()),
                                 .start = model::Id(get_or_assign_eid(top_note).to_std_string())};

        switch (arpeggio->arpeggio_type()) {
            case ArpeggioType::NORMAL: {
                ensure(mnx_measure.arpeggios).push_back(model::Arpeggio{.position = mnx_position(*position), .span = span});
                break;
            }
            case ArpeggioType::UP:
            case ArpeggioType::UP_STRAIGHT: {
                ensure(mnx_measure.arpeggios)
                    .push_back(model::Arpeggio{
                        .arrow = true, .direction = model::UpDownAuto::UP, .position = mnx_position(*position), .span = span});
                break;
            }
            case ArpeggioType::DOWN:
            case ArpeggioType::DOWN_STRAIGHT: {
                ensure(mnx_measure.arpeggios)
                    .push_back(model::Arpeggio{
                        .arrow = true, .direction = model::UpDownAuto::DOWN, .position = mnx_position(*position), .span = span});
                break;
            }
            case ArpeggioType::BRACKET: {
                ensure(mnx_measure.non_arpeggios).push_back(model::NonArpeggio{.position = mnx_position(*position), .span = span});
                break;
            }
            default: LOGW() << "Skipping unsupported arpeggio type: " << static_cast<int>(arpeggio->arpeggio_type()); break;
        }
    };

    for (Segment *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
        for (track_idx_t track = track_range.start_track; track < track_range.end_track; ++track) {
            EngravingItem *item = segment->element(track);
            if (!item || !item->is_chord()) {
                continue;
            }
            Chord *chord = to_chord(item);
            process_arpeggio(chord);
            for (Chord *grace_chord : chord->grace_notes()) {
                process_arpeggio(grace_chord);
            }
        }
    }
}

/// Splits a dynamic's text into leading text, SMuFL glyphs, and trailing text. Returns false,
/// leaving all three empty, when the text interleaves glyphs and words in a way MNX cannot
/// describe -- the caller must not export a partial reading of it.
static bool split_dynamic_text(const Dynamic *dynamic, String &prefix, std::vector<SymId> &sym_ids, String &suffix) {
    const auto engraving_fonts = dynamic->score()->services().engraving_fonts;

    bool got_glyph{};
    bool got_suffix{};
    for (const TextFragment &fragment : dynamic->fragment_list()) {
        const CharFormat &format = fragment.format;
        if (format.font_family() == u"ScoreText") {
            if (got_suffix) {
                // Can't encode a dynamic with multiple glyph sequences separated by text.
                // Clear everything so a partial reading cannot be mistaken for a whole one.
                prefix.clear();
                suffix.clear();
                sym_ids.clear();
                return false;
            }
            for (size_t i = 0; i < fragment.text.size(); ++i) {
                const Char ch = fragment.text.at(i);
                const SymId sym_id = engraving_fonts->fallback_font()->from_code(ch.unicode());
                if (sym_id != SymId::NO_SYM) {
                    got_glyph = true;
                    sym_ids.push_back(sym_id);
                }
            }
        } else {
            if (got_glyph) {
                suffix += fragment.text;
                got_suffix = true;
            } else {
                prefix += fragment.text;
            }
        }
    }
    prefix = prefix.trimmed();
    suffix = suffix.trimmed();
    return true;
}

//---------------------------------------------------------
//   createDynamic
//   export a dynamic text annotation
//---------------------------------------------------------

static void create_dynamic(const Dynamic *dynamic, const Fraction &r_tick, int mnx_staff_num, model::PartMeasure &mnx_measure) {
    IF_ASSERT_FAILED(dynamic) { return; }

    String prefix{};
    String suffix{};
    std::vector<SymId> sym_ids{};
    if (!split_dynamic_text(dynamic, prefix, sym_ids, suffix)) {
        LOGI() << "Dynamic \"" << dynamic->plain_text() << "\" mixes glyphs and text in a way MNX cannot describe; not exported.";
        return;
    }

    bool saw_increase{};
    bool saw_decrease{};
    auto check_relative = [&](const String &value) {
        if (value == u"piu" || value == u"più") {
            saw_increase = true;
        }
        if (value == u"menos" || value == u"meno") {
            saw_decrease = true;
        }
    };
    check_relative(prefix);
    check_relative(suffix);

    MnxDynamicMapping mapping = to_mnx_dynamic_type(dynamic->dynamic_type());
    if (!mapping.value) {
        // MuseScore renders dynamics it has no type for, such as "sfzp". MNX can describe
        // them even so, because it builds a dynamic from parts rather than from a fixed
        // vocabulary, so recover those parts from the glyphs or letters that spell it.
        if (!sym_ids.empty()) {
            if (const auto from_glyphs = to_mnx_dynamic_from_sym_ids(sym_ids)) {
                mapping = from_glyphs.value();
            }
        } else if (const auto from_letters = to_mnx_dynamic_from_letters(dynamic->plain_text().trimmed().to_std_string())) {
            // Ask the whole text rather than `prefix`, even though the two hold the same letters
            // here. splitDynamicText drops music-font characters that map to no SymId, so a
            // dynamic carrying one would reach `prefix` looking like a clean spelling and export
            // with the glyph silently gone. plainText keeps those characters, the parse rejects
            // them, and we skip the dynamic instead of exporting a lie.
            mapping = from_letters.value();
            // The parse consumed the entire text, so there is nothing left over to say.
            prefix.clear();
        }
    }

    // MNX requires a value on an immediate and on an accent, so a dynamic we cannot map is
    // exportable only as a relative one, which may carry its text alone.
    const bool is_relative = saw_increase || saw_decrease;
    if (!mapping.value && (!is_relative || (prefix.empty() && sym_ids.empty() && suffix.empty()))) {
        /// @todo Export dynamics whose text is not a dynamic spelling at all once MNX can
        /// describe one. Today every dynamic group needs a value or a relative value, so
        /// there is nothing to hang free text on.
        if (!is_relative) {
            LOGI() << "Dynamic \"" << dynamic->plain_text() << "\" has no MNX representation and is not exported.";
        }
        return;
    }

    model::DynamicGroup mnx_dynamic;
    mnx_dynamic.position = model::RhythmicPosition{.fraction = to_mnx_fraction(r_tick)};
    if (is_relative) {
        mnx_dynamic.type = model::DynamicGroupType::RELATIVE;
        mnx_dynamic.relative_value = saw_increase ? model::RelativeDynamicValue::LOUDER : model::RelativeDynamicValue::SOFTER;
        mnx_dynamic.value = mapping.value;
    } else if (mapping.is_accent) {
        mnx_dynamic.type = model::DynamicGroupType::ACCENT;
        mnx_dynamic.value = mapping.value;
        // accentPrefix and accentSuffix default to "s" and "z", so anything else must be
        // written out; they are omitted when they match, as the MNX examples do.
        set_unless_default(mnx_dynamic.accent_prefix, mapping.accent_prefix, model::DynamicPrefix::S);
        set_unless_default(mnx_dynamic.accent_suffix, mapping.accent_suffix, model::DynamicSuffix::Z);
        mnx_dynamic.residual_value = mapping.residual_value;
    } else {
        mnx_dynamic.type = model::DynamicGroupType::IMMEDIATE;
        mnx_dynamic.value = mapping.value;
    }

    if (!prefix.empty()) {
        mnx_dynamic.prefix = prefix.to_std_string();
    }
    if (!suffix.empty()) {
        mnx_dynamic.suffix = suffix.to_std_string();
    }
    if (!sym_ids.empty()) {
        auto &mnx_glyphs = ensure(mnx_dynamic.glyphs);
        for (const SymId sym : sym_ids) {
            mnx_glyphs.push_back(SymNames::name_for_sym_id(sym).ascii());
        }
    }

    set_unless_default(mnx_dynamic.orient, mnx_multi_staff_orient_from_direction(dynamic), model::MultiStaffOrientation::AUTO);
    export_mnx_voice_assignment(mnx_dynamic, mnx_staff_num, dynamic->voice_assignment(), dynamic->track());
    ensure(mnx_measure.dynamics).push_back(std::move(mnx_dynamic));
}

//---------------------------------------------------------
//   exportTextAnnotations
//   export dynamics, staff text, etc. for a single measure
//---------------------------------------------------------

static void export_text_annotations(const Part *part, const Measure *measure, model::PartMeasure &mnx_measure) {
    const size_t staves = part->nstaves();
    const track_idx_t part_start_track = part->track_range().start_track;
    constexpr SegmentType time_segments = SegmentType::DURATION;
    for (Segment *segment = measure->first(time_segments); segment; segment = segment->next(time_segments)) {
        const Fraction r_tick = segment->rtick();
        for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
            const track_idx_t track = part_start_track + ::mu::engraving::voices * staff_idx;
            for (EngravingItem *annotation : segment->annotations()) {
                if (annotation->track() < track || annotation->track() >= track + ::mu::engraving::voices || !annotation->is_text_base()) {
                    continue;
                }
                switch (annotation->type()) {
                    case ElementType::DYNAMIC: create_dynamic(to_dynamic(annotation), r_tick, int(staff_idx) + 1, mnx_measure); break;
                    /// @todo other kinds of staff text when defined by MNX
                    default: break;
                }
            }
        }
    }
}

//---------------------------------------------------------
//   exportMeasureRepeat
//   export a measure repeat, if this measure belongs to one
//---------------------------------------------------------

// Returns true if the measure belongs to a measure repeat group, in which case its content
// is represented by the repeat rather than by sequences.
static bool export_measure_repeat(const Part *part, const Measure *measure, model::PartMeasure &mnx_measure) {
    /// @todo Export MeasureRepeat counter, displayNumber, and staffPosition. MuseScore keeps
    /// none of them per element: the counter comes from the repeat's place in the run,
    /// whether `number` is drawn is fixed for 2- and 4-bar repeats and score-wide styled for
    /// 1-bar ones, and the glyph's staff position is computed from the staff's middle line.
    const Staff *first_staff = part->staff(0);
    if (!first_staff || !measure->is_measure_repeat_group(first_staff->idx())) {
        return false;
    }

    const staff_idx_t staff_idx = first_staff->idx();
    const int num_measures = measure->measure_repeat_num_measures(staff_idx);

    // MNX attaches a measure repeat to the part, not to one of its staves.
    for (staff_idx_t s = 1; s < part->nstaves(); ++s) {
        const Staff *staff = part->staff(s);
        if (staff && measure->measure_repeat_num_measures(staff->idx()) != num_measures) {
            LOGW() << "Measure repeat differs between staves of a part; exporting the first staff's.";
            break;
        }
    }

    // The group is declared on its first measure; the rest are continuations.
    if (measure->measure_repeat_count(staff_idx) == 1) {
        mnx_measure.measure_repeat = model::MeasureRepeat{.number = num_measures};
    }
    return true;
}

//---------------------------------------------------------
//   createParts
//---------------------------------------------------------

bool MnxExporter::create_parts() {
    if (!score_) {
        return false;
    }

    auto &mnx_parts = root_.parts;
    staff_to_part_staff_.clear();
    exported_staves_.clear();
    bool has_part = false;

    for (Part *part : score_->parts()) {
        bool has_tab_staff = false;
        const Measure *first_measure = score_->first_measure();
        const Fraction tick = first_measure ? first_measure->tick() : Fraction(0, 1);
        for (staff_idx_t s = 0; s < part->nstaves(); ++s) {
            if (const Staff *staff = part->staff(s)) {
                if (staff->is_tab_staff(tick)) {
                    has_tab_staff = true;
                    break;
                }
            }
        }
        if (has_tab_staff) {
            LOGI() << "Skipping tab part \"" << part->part_name() << "\" in MNX export.";
            continue;
        }

        const size_t mnx_part_index = mnx_parts.size();
        auto &mnx_part = mnx_parts.emplace_back();
        mnx_part.id = model::Id(get_or_assign_eid(part).to_std_string());
        has_part = true;

        const String long_name = part->long_name();
        if (!long_name.is_empty()) {
            mnx_part.name = long_name.to_std_string();
        }

        const String short_name = part->short_name();
        if (!short_name.is_empty()) {
            mnx_part.short_name = short_name.to_std_string();
        }

        set_unless_default(mnx_part.staves, static_cast<int>(part->nstaves()), 1);

        const Instrument *instrument = part->instrument();
        if (instrument) {
            const Interval transpose = instrument->transpose();
            if (!transpose.is_zero()) {
                const Interval mnx_transpose(-transpose.diatonic, -transpose.chromatic);
                auto &mnx_transposition = ensure(mnx_part.transposition);
                mnx_transposition.interval =
                    model::Interval{.half_steps = mnx_transpose.chromatic, .staff_distance = mnx_transpose.diatonic};
                const PreferSharpFlat prefer = part->prefer_sharp_flat();
                mnx_transposition.key_fifths_flip_at = to_mnx_key_fifths_flip_value(prefer, mnx_transpose);
            }
            if (instrument->use_drumset()) {
                export_drumset_kit(part, instrument, mnx_part);
            }
        }

        for (staff_idx_t s = 0; s < part->nstaves(); ++s) {
            if (const Staff *staff = part->staff(s)) {
                staff_to_part_staff_.emplace(staff->idx(), std::make_pair(mnx_part_index, static_cast<int>(s + 1)));
                exported_staves_.push_back(score_->staff(staff->idx()));
            }
        }

        auto &mnx_measures = mnx_part.measures;
        for (const Measure *measure = score_->first_measure(); measure; measure = measure->next_measure()) {
            auto &mnx_measure = mnx_measures.emplace_back();

            append_clefs_for_measure(part, measure, mnx_measure);
            // A measure repeat stands in for the measure's content, so the sequences stay
            // empty. Measure's constructor has already created them.
            if (export_measure_repeat(part, measure, mnx_measure)) {
                continue;
            }
            create_sequences(part, measure, mnx_measure);
            export_text_annotations(part, measure, mnx_measure);
            /// the items below must come after createSequences.
            create_arpeggios(part, measure, mnx_measure);
        }
    }

    export_spanners();

    if (!lyric_line_ids_.empty()) {
        /// @todo line metadata if MNX spec expands to include anything we can export.
        auto &mnx_line_order = ensure(ensure(root_.global.lyrics).line_order);
        for (const auto &line_id : lyric_line_ids_) {
            mnx_line_order.push_back(line_id);
        }
    }

    std::sort(exported_staves_.begin(), exported_staves_.end(), [](const Staff *lhs, const Staff *rhs) {
        if (!lhs || !rhs) {
            return lhs != nullptr;
        }
        return lhs->idx() < rhs->idx();
    });

    return has_part;
}
} // namespace mu::iex::mnxio
