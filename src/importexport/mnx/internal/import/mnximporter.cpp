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
#include "mnximporter.h"

#include <algorithm>
#include <array>
#include <exception>
#include <string_view>
#include <vector>

#include "engraving/dom/barline.h"
#include "engraving/dom/bracketitem.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/score.h"
#include "engraving/dom/sig.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/volta.h"
#include "engraving/editing/editstaffbrackets.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/types/symnames.h"
#include "internal/shared/mnxtypesconv.h"
#include "log.h"
#include "model/layout.h"
#include "model/music.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
namespace {
int tick_span_from(int lines) {
    return ::mu::engraving::barline_span_tic_k1_from + (lines == 0 ? ::mu::engraving::barline_span_1_linestaff_from : 0);
}

int tick_span_to(int lines) { return (lines == 0 ? ::mu::engraving::barline_span_1_linestaff_from : (2 * -lines)) + 1; }

int mensur_striche_span_from(int lines) { return lines == 0 ? ::mu::engraving::barline_span_1_linestaff_to : 2 * lines; }

int mensur_striche_final_span_to(int lines) {
    // For 1-line staves, spanTo=0 triggers special full-height rendering.
    // Use matching from/to to collapse the final mensurstrich barline to zero height.
    return lines == 0 ? ::mu::engraving::barline_span_1_linestaff_to : 0;
}
} // namespace

//---------------------------------------------------------
//   createDrumset
//   Build a MuseScore Drumset from an MNX part kit definition.
//---------------------------------------------------------

static const model::Sound *find_sound(const model::Root &root, std::string_view sound_id) {
    if (!root.global.sounds) {
        return nullptr;
    }
    for (const auto &[id, sound] : *root.global.sounds) {
        if (id == sound_id) {
            return &sound;
        }
    }
    return nullptr;
}

static Drumset *create_drumset(const model::Root &root, size_t part_idx,
                               std::map<std::pair<size_t, std::string>, int> &kit_component_to_midi) {
    const model::Part &mnx_part = root.parts[part_idx];
    if (!mnx_part.kit) {
        return nullptr;
    }

    Drumset *drumset = new Drumset();
    Drumset *default_drumset = mu::engraving::sm_drumset;
    for (int i = 0; i < ::mu::engraving::drum_instruments; ++i) {
        drumset->drum(i) = DrumInstrument(String(), NoteHeadGroup::HEAD_INVALID, 0, DirectionV::AUTO);
    }

    const StaffType *perc_staff_type = StaffType::preset(StaffTypes::PERC_DEFAULT);
    /// @todo If MNX gains explicit staff type info, use it here instead of PERC_DEFAULT.
    const int middle_line = perc_staff_type ? perc_staff_type->middle_line() : 4;

    struct KitEntry {
        std::string id;
        const model::KitComponent *component = nullptr;
        int midi_pitch = -1;
    };

    std::vector<KitEntry> kit_entries;
    kit_entries.reserve(mnx_part.kit->size());
    for (const auto &[kit_id, kit_component] : *mnx_part.kit) {
        kit_entries.push_back({kit_id, &kit_component, -1});
    }
    // Fallback pitches are handed out in kit id order, as the JSON object order used to give.
    std::ranges::stable_sort(kit_entries, {}, &KitEntry::id);

    std::array<bool, ::mu::engraving::drum_instruments> used_pitches{};
    const auto find_fallback_pitch = [&used_pitches]() -> int {
        for (int pitch = 0; pitch < ::mu::engraving::drum_instruments; ++pitch) {
            if (!used_pitches[pitch]) {
                return pitch;
            }
        }
        return -1;
    };
    for (auto &entry : kit_entries) {
        if (entry.component->sound) {
            if (const model::Sound *sound = find_sound(root, *entry.component->sound); sound && sound->midi_number) {
                entry.midi_pitch = *sound->midi_number;
            }
        }

        if (pitch_is_valid(entry.midi_pitch)) {
            used_pitches[entry.midi_pitch] = true;
        }
    }

    for (auto &entry : kit_entries) {
        if (pitch_is_valid(entry.midi_pitch)) {
            continue;
        }
        int fallback_pitch = default_drumset ? default_drumset->default_pitch_for_line(middle_line - entry.component->staff_position) : -1;
        if (!pitch_is_valid(fallback_pitch) || used_pitches[fallback_pitch]) {
            fallback_pitch = find_fallback_pitch();
        }
        if (!pitch_is_valid(fallback_pitch)) {
            LOGW() << "Kit component \"" << entry.id << "\" lacks a valid MIDI pitch and no fallback is available.";
            continue;
        }
        entry.midi_pitch = fallback_pitch;
        used_pitches[fallback_pitch] = true;
        LOGW() << "Kit component \"" << entry.id << "\" lacks a valid MIDI pitch. Using fallback pitch " << fallback_pitch << ".";
    }

    for (const auto &entry : kit_entries) {
        if (!pitch_is_valid(entry.midi_pitch)) {
            continue;
        }
        kit_component_to_midi[{part_idx, entry.id}] = entry.midi_pitch;

        const bool has_default = default_drumset && default_drumset->is_valid(entry.midi_pitch);
        NoteHeadGroup notehead = has_default ? default_drumset->note_head(entry.midi_pitch) : NoteHeadGroup::HEAD_NORMAL;
        DirectionV stem_direction = has_default ? default_drumset->stem_direction(entry.midi_pitch) : DirectionV::AUTO;
        int voice = has_default ? default_drumset->voice(entry.midi_pitch) : 0;
        String shortcut = has_default ? default_drumset->shortcut(entry.midi_pitch) : String();

        String name;
        if (entry.component->name) {
            name = String::from_std_string(*entry.component->name);
        }
        if (name.is_empty() && entry.component->sound) {
            if (const model::Sound *sound = find_sound(root, *entry.component->sound); sound && sound->name) {
                name = String::from_std_string(*sound->name);
            }
        }
        if (name.is_empty()) {
            // MuseScore requires a note name
            name = String(u"Percussion note");
        }

        int line = middle_line - entry.component->staff_position;
        drumset->drum(entry.midi_pitch) = DrumInstrument(name, notehead, line, stem_direction, -1, -1, voice, shortcut);
        if (notehead == NoteHeadGroup::HEAD_CUSTOM && has_default) {
            for (int type = 0; type < static_cast<int>(NoteHeadType::HEAD_TYPES); ++type) {
                drumset->drum(entry.midi_pitch).noteheads[type] = default_drumset->note_heads(entry.midi_pitch, NoteHeadType(type));
            }
        }
    }

    return drumset;
}

//---------------------------------------------------------
//   loadInstrument
//   Populate a MuseScore Instrument from an MNX part definition. Returns the part's
//   transposition with the keyFifthsFlipAt value MuseScore can represent.
//---------------------------------------------------------

static std::optional<model::PartTransposition> load_instrument(const model::Root &root, size_t part_idx, Part *part, Instrument *instrument,
                                                               std::map<std::pair<size_t, std::string>, int> &kit_component_to_midi) {
    const model::Part &mnx_part = root.parts[part_idx];
    // Initialize drumset
    if (mnx_part.kit) {
        instrument->set_use_drumset(true);
        Drumset *drumset = create_drumset(root, part_idx, kit_component_to_midi);
        if (drumset) {
            instrument->set_drumset(drumset);
            instrument->channel(0)->set_bank(128);
        }
    } else {
        instrument->set_use_drumset(false);
    }

    // Names
    instrument->set_track_name(part->part_name());
    instrument->set_long_name(part->long_name());
    instrument->set_short_name(part->short_name());

    // Transposition
    // MNX transposition has opposite signs.
    if (!mnx_part.transposition) {
        return std::nullopt;
    }
    model::PartTransposition transposition = *mnx_part.transposition;
    const model::Interval &interval = transposition.interval;
    instrument->set_transpose(Interval(-interval.staff_distance, -interval.half_steps));
    const auto key_fifths_flip = transposition.key_fifths_flip_at;
    part->set_prefer_sharp_flat(to_muse_score_prefer_sharp_flat(key_fifths_flip.value_or(0)));
    const Interval mnx_transpose(interval.staff_distance, interval.half_steps);
    const int updated_flip_at = to_mnx_key_fifths_flip_value(part->prefer_sharp_flat(), mnx_transpose);
    if (key_fifths_flip && key_fifths_flip.value() != updated_flip_at) {
        LOGW() << "MNX keyFifthsFlipAt value (" << key_fifths_flip.value() << ") cannot be imported for part (" << part->part_name()
               << "). Using (" << updated_flip_at << ") instead.";
    }
    transposition.key_fifths_flip_at = updated_flip_at;
    return transposition;
}

//---------------------------------------------------------
//   mnxPartStaffToStaffIdx
//   Map MNX part/staff numbers to MuseScore staff indices.
//---------------------------------------------------------

staff_idx_t MnxImporter::mnx_part_staff_to_staff_idx(size_t part_index, int staff_num) {
    staff_idx_t idx = muse::value(mnx_part_staff_to_staff_, std::make_pair(part_index, staff_num), muse::nidx);
    IF_ASSERT_FAILED(idx != muse::nidx) { throw std::logic_error("Unmapped staff encountered"); }
    return idx;
}

//---------------------------------------------------------
//   mnxLayoutStaffToStaffIdx
//   Resolve MNX layout staff sources to MuseScore staff indices.
//---------------------------------------------------------

staff_idx_t MnxImporter::mnx_layout_staff_to_staff_idx(const model::Staff &mnx_staff) {
    for (const auto &source : mnx_staff.sources) {
        if (const auto part = index().find_part(source.part)) {
            return mnx_part_staff_to_staff_idx(*part, source.staff_or_default());
        }
    }
    return muse::nidx;
}

//---------------------------------------------------------
//   mnxMeasureToMeasure
//   Convert an MNX measure index to a MuseScore Measure pointer.
//---------------------------------------------------------

Measure *MnxImporter::try_mnx_measure_to_measure(const size_t mnx_meas_idx) {
    const Fraction meas_tick = muse::value(mnx_meas_to_tick_, mnx_meas_idx, {-1, 1});
    if (meas_tick < Fraction(0, 1)) {
        return nullptr; // not mapped, e.g. an index past the last measure of the score
    }
    return score_->tick2measure(meas_tick);
}

Measure *MnxImporter::mnx_measure_to_measure(const size_t mnx_meas_idx) {
    Fraction meas_tick = muse::value(mnx_meas_to_tick_, mnx_meas_idx, {-1, 1});
    IF_ASSERT_FAILED(meas_tick >= Fraction(0, 1)) {
        throw std::logic_error("MNX measure index " + std::to_string(mnx_meas_idx) + " is not mapped.");
    }
    Measure *measure = score_->tick2measure(meas_tick);
    IF_ASSERT_FAILED(measure) {
        throw std::logic_error("MNX measure index " + std::to_string(mnx_meas_idx) + " has invalid tick " +
                               meas_tick.to_string().to_std_string());
    }
    return measure;
}

//---------------------------------------------------------
//   mnxEventIdToCR
//   Look up a MuseScore ChordRest by MNX event id.
//---------------------------------------------------------

engraving::ChordRest *MnxImporter::mnx_event_id_to_cr(std::string_view event_id) {
    const model::EventEntry *entry = index().find_event(event_id);
    if (!entry) {
        return nullptr;
    }
    return muse::value(mnx_event_to_cr_, entry->event);
}

//---------------------------------------------------------
//   mnxNoteIdToNote
//   Look up a MuseScore Note by MNX note id.
//---------------------------------------------------------

engraving::Note *MnxImporter::mnx_note_id_to_note(std::string_view note_id) {
    if (const auto *note = index().find_note(note_id)) {
        return muse::value(mnx_note_to_note_, note->note);
    }
    if (const auto *kit_note = index().find_kit_note(note_id)) {
        return muse::value(mnx_kit_note_to_note_, kit_note->note);
    }
    return nullptr;
}

//---------------------------------------------------------
//   setAndStyleProperty
//   Set a property and apply style flags respecting inheritance.
//---------------------------------------------------------

void MnxImporter::set_and_style_property(EngravingObject *e, Pid id, PropertyValue v) {
    if (v.is_valid()) {
        e->set_property(id, v);
    }
    if (e->property_flags(id) == PropertyFlags::NOSTYLE) {
        return;
    }
    const bool can_leave_styled = (e->get_property(id) == e->property_default(id));
    e->set_property_flags(id, can_leave_styled ? PropertyFlags::STYLED : PropertyFlags::UNSTYLED);
}

//---------------------------------------------------------
//   mnxMeasurePosToTick
//   Convert an MNX measure-relative position to absolute tick.
//---------------------------------------------------------

Fraction MnxImporter::mnx_measure_pos_to_tick(const model::MeasureRhythmicPosition &meas_pos) {
    const size_t meas_idx = index().measure_ids.at(meas_pos.measure);
    const Fraction meas_tick = muse::value(mnx_meas_to_tick_, meas_idx, Fraction(-1, 1));
    IF_ASSERT_FAILED(meas_tick.positive()) {
        throw std::logic_error("MNX global measure at " + std::to_string(meas_idx) + " was not mapped.");
    }
    return meas_tick + to_muse_score_r_tick(meas_pos.position);
}

//---------------------------------------------------------
//   importSettings
//   Import MNX score-level settings.
//---------------------------------------------------------

void MnxImporter::import_settings() {
    /// @todo add settings as MNX adds them

    // MNX specifies that the barline of the last bar is a finale barline by default.
    // This appears always to be the case for MuseScore as well, so nothing needs to be done for this.
}

//---------------------------------------------------------
//   createStaff
//   Create a MuseScore staff from an MNX part/staff entry.
//---------------------------------------------------------

void MnxImporter::create_staff(Part *part, size_t part_index, int staff_num) {
    Staff *staff = Factory::create_staff(part);
    if (part->instrument()->use_drumset() && !staff->is_drum_staff(Fraction(0, 1))) {
        /// @todo If MNX gains explicit staff type/line-count info, use it here.
        staff->set_staff_type(Fraction(0, 1), *StaffType::preset(StaffTypes::PERC_DEFAULT));
        staff->set_default_clef_type(ClefTypeList(ClefType::PERC2, ClefType::PERC2));
        staff->clef_list().set_clef(0, ClefTypeList(ClefType::PERC2, ClefType::PERC2));
    }
    score_->append_staff(staff);
    mnx_part_staff_to_staff_.emplace(std::make_pair(part_index, staff_num), staff->idx());
    staff_to_mnx_part_.emplace(staff->idx(), part_index);
}

//---------------------------------------------------------
//   importParts
//   Create MuseScore parts, instruments, and staves from MNX parts.
//---------------------------------------------------------

void MnxImporter::import_parts() {
    part_transpositions_.clear();
    for (size_t part_index = 0; part_index < root().parts.size(); ++part_index) {
        const model::Part &mnx_part = root().parts[part_index];
        Part *part = new Part(score_);
        /// @todo a better way to find the instrument, perhaps by part name or else some future mnx enhancement
        const InstrumentTemplate *it = [&]() {
            if (mnx_part.kit) {
                return search_template(u"drumset");
            }
            return search_template(u"piano");
        }();
        if (it) {
            part->init_from_instr_template(it);
        }
        part->set_long_name(String::from_std_string(mnx_part.name.value_or("")));
        part->set_short_name(String::from_std_string(mnx_part.short_name.value_or("")));
        part_transpositions_.push_back(load_instrument(root(), part_index, part, part->instrument(), mnx_kit_component_to_midi_));
        for (int staff_num = 1; staff_num <= mnx_part.staves_or_default(); staff_num++) {
            create_staff(part, part_index, staff_num);
        }
        score_->append_part(part);
    }
}

//---------------------------------------------------------
//   importBrackets
//   Import bracket and barline span information from MNX layouts.
//---------------------------------------------------------

void MnxImporter::import_brackets() {
    group_barline_overrides_.clear();

    const model::SystemLayout *full_score_layout = model::full_score_layout(root());
    if (!full_score_layout) {
        LOGI() << "Unable to find full score layout. Using default layout from parts.";
    }
    const auto layout_spans = full_score_layout ? model::layout_spans(*full_score_layout) : model::default_layout_spans(root());
    std::vector<const model::Staff *> layout_staves;
    if (full_score_layout) {
        layout_staves = model::flatten_staves(*full_score_layout);
    }
    for (const auto &span : layout_spans) {
        BracketType brt = to_muse_score_bracket_type(span.symbol.value_or(model::StaffSymbol::NO_SYMBOL));
        if (brt == BracketType::NO_BRACKET && span.start >= span.end) {
            continue;
        }
        const staff_idx_t staff_idx =
            full_score_layout ? mnx_layout_staff_to_staff_idx(*layout_staves.at(span.start)) : static_cast<staff_idx_t>(span.start);
        if (staff_idx == muse::nidx) {
            LOGE() << "Staff not found for span starting at " << span.start << " and ending at " << span.end << ".";
            continue;
        }
        BracketItem *bi = Factory::create_bracket_item(score_->dummy());
        bi->set_bracket_type(brt);
        const int group_span = static_cast<int>(span.end - span.start + 1);
        bi->set_bracket_span(group_span);
        bi->set_column(size_t(span.depth));
        EditStaffBrackets::add_bracket(score_, staff_idx, bi);
        if (group_span <= 1 || span.kind != model::LayoutSpan::Kind::GROUP) {
            continue;
        }
        const model::BarlineOverride group_override = span.barline;
        if (group_override == model::BarlineOverride::NONE) {
            continue;
        }
        const staff_idx_t end_staff = staff_idx + static_cast<staff_idx_t>(group_span - 1);
        if (end_staff >= score_->nstaves()) {
            LOGE() << "Group span starting at staff " << staff_idx << " exceeds score staff count.";
            continue;
        }
        group_barline_overrides_.push_back({
            staff_idx,
            end_staff,
            group_override,
        });
    }

    // Apply default staff barline connections using the same precedence as explicit barline import:
    // any Unified on the connection wins; otherwise the closest Mensurstrich applies.
    const staff_idx_t staff_count = score_->nstaves();
    for (staff_idx_t idx = 0; idx + 1 < staff_count; ++idx) {
        bool local_span = false;
        bool mensur_striche = false;
        size_t priority = static_cast<size_t>(staff_count) + 1;
        for (const auto &override_span : group_barline_overrides_) {
            if (idx < override_span.start_staff || idx >= override_span.end_staff) {
                continue;
            }
            if (override_span.barline_override == model::BarlineOverride::UNIFIED) {
                local_span = true;
            }
            const size_t group_size = static_cast<size_t>(override_span.end_staff - override_span.start_staff + 1);
            if (priority > group_size) {
                mensur_striche = override_span.barline_override == model::BarlineOverride::MENSURSTRICH;
                priority = group_size;
            }
        }
        mensur_striche = mensur_striche && !local_span;

        Staff *staff = score_->staff(idx);
        if (local_span || mensur_striche) {
            staff->set_bar_line_span(true);
            staff->set_bar_line_to(0);
            if (mensur_striche) {
                const int lines = staff->lines(Fraction(0, 1)) - 1;
                staff->set_bar_line_from(mensur_striche_span_from(lines));
            } else {
                staff->set_bar_line_from(0);
            }
        } else {
            staff->set_bar_line_span(false);
            staff->set_bar_line_from(0);
            staff->set_bar_line_to(0);
        }
    }

    // Hide trailing barline segment on final staff of standalone mensurstrich groups.
    for (const auto &override_span : group_barline_overrides_) {
        if (override_span.barline_override != model::BarlineOverride::MENSURSTRICH) {
            continue;
        }
        Staff *final_staff = score_->staff(override_span.end_staff);
        if (!final_staff->bar_line_span()) {
            const int lines = final_staff->lines(Fraction(0, 1)) - 1;
            final_staff->set_bar_line_from(mensur_striche_span_from(lines));
            final_staff->set_bar_line_to(mensur_striche_final_span_to(lines));
        }
    }
}

//---------------------------------------------------------
//   createKeySig
//   Create MuseScore key signatures for all staves at a measure.
//---------------------------------------------------------

void MnxImporter::create_key_sig(engraving::Measure *measure, int key_fifths) {
    const Key concert_key = to_muse_score_key(key_fifths);
    if (concert_key == Key::INVALID) {
        LOGE() << "invalid mnx key fifths " << key_fifths << " for measure " << measure->measure_index();
        return;
    }
    for (staff_idx_t idx = 0; idx < score_->nstaves(); idx++) {
        Staff *staff = score_->staff(idx);
        KeySigEvent key_sig_event;
        key_sig_event.set_concert_key(concert_key);
        key_sig_event.set_key(concert_key);
        if (!score()->style().style_b(Sid::CONCERT_PITCH)) {
            const size_t mnx_part_index = muse::value(staff_to_mnx_part_, idx, muse::nidx);
            IF_ASSERT_FAILED(mnx_part_index != muse::nidx) { throw std::logic_error("Staff " + std::to_string(idx) + " is not mapped."); }
            if (const auto &part_transposition = part_transpositions_.at(mnx_part_index)) {
                int transp_fifths = model::transposed_key(*part_transposition, key_fifths);
                const Key transp_key = to_muse_score_key(transp_fifths);
                if (transp_key != Key::INVALID) {
                    key_sig_event.set_key(transp_key);
                } else {
                    // measure has not been added to score yet, so use nmeasures to calculate the measure.
                    LOGW() << "invalid mnx transposed key fifths " << transp_fifths << " for measure at index " << score_->nmeasures();
                    // set the document to concert pitch and let MuseScore deal with it.
                    score_->style().set(Sid::CONCERT_PITCH, true);
                }
            }
        }
        Segment *seg = measure->get_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
        KeySig *ks = Factory::create_key_sig(seg);
        ks->set_key_sig_event(key_sig_event);
        ks->set_track(staff2track(idx));
        seg->add(ks);
        staff->set_key(measure->tick(), ks->key_sig_event());
    }
}

//---------------------------------------------------------
//   createTimeSig
//   Create MuseScore time signatures from MNX data.
//---------------------------------------------------------

// The time signature as written: 6/8 stays 6/8 rather than reducing to 3/4.
static Fraction time_sig_fraction(const model::Time &time_sig) { return Fraction(time_sig.count, static_cast<int>(time_sig.unit)); }

void MnxImporter::create_time_sig(engraving::Measure *measure, const model::Time &time_sig) {
    const Fraction sig_fraction = time_sig_fraction(time_sig);
    // setSig is the only way in; TimeSig has no public setTimeSigType.
    const TimeSigType sig_type = to_muse_score_time_sig_type(time_sig.display);
    for (track_idx_t track_idx = 0; track_idx < score_->ntracks(); track_idx += ::mu::engraving::voices) {
        Segment *seg = measure->get_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
        TimeSig *ts = Factory::create_time_sig(seg);
        ts->set_sig(sig_fraction, sig_type);
        ts->set_track(track_idx);
        seg->add(ts);
    }
}

//---------------------------------------------------------
//   setBarline
//   Create MuseScore barlines matching the MNX barline type.
//---------------------------------------------------------

void MnxImporter::set_barline(engraving::Measure *measure, const model::Barline &barline) {
    const model::BarlineType mnx_blt = barline.type;
    BarLineType blt = to_muse_score_bar_line_type(mnx_blt);
    Segment *bls = measure->get_segment_r(SegmentType::END_BAR_LINE, measure->ticks());

    for (staff_idx_t idx = 0; idx < score_->staves().size(); idx++) {
        bool local_span = false;
        bool mensur_striche = false;
        bool mensur_last = false;
        size_t priority = score_->nstaves() + 1;
        for (const auto &override_span : group_barline_overrides_) {
            if (idx < override_span.start_staff || idx > override_span.end_staff) {
                continue;
            }
            if (override_span.barline_override == model::BarlineOverride::UNIFIED) {
                local_span = true;
            }
            const size_t group_size = static_cast<size_t>(override_span.end_staff - override_span.start_staff + 1);
            if (priority > group_size) {
                mensur_striche = override_span.barline_override == model::BarlineOverride::MENSURSTRICH;
                mensur_last = mensur_striche && idx == override_span.end_staff;
                priority = group_size;
            }
        }
        const bool local_tick = mnx_blt == model::BarlineType::TICK;
        const bool local_short = mnx_blt == model::BarlineType::SHORT;
        mensur_striche = mensur_striche && !local_span && !local_tick && !local_short;

        BarLine *bl = Factory::create_bar_line(bls);
        bl->set_track(staff2track(idx));
        bl->set_visible(mnx_blt != model::BarlineType::NO_BARLINE);
        bl->set_generated(false);
        bl->set_bar_line_type(blt);
        bl->set_span_staff(!local_tick && !local_short && (local_span || mensur_striche));
        if (local_tick) {
            int lines = bl->staff()->lines(bls->tick() - Fraction::eps()) - 1;
            bl->set_span_from(tick_span_from(lines));
            bl->set_span_to(tick_span_to(lines));
        } else if (local_short) {
            bl->set_span_from(::mu::engraving::barline_span_shor_t1_from);
            bl->set_span_to(::mu::engraving::barline_span_shor_t1_to);
        } else if (mensur_striche) {
            int lines = bl->staff()->lines(bls->tick() - Fraction::eps()) - 1;
            bl->set_visible(bl->visible() && !mensur_last);
            bl->set_span_from(mensur_striche_span_from(lines));
            bl->set_span_to(0);
        } else {
            bl->set_span_from(0);
            bl->set_span_to(0);
        }
        bls->add(bl);
    }
}

//---------------------------------------------------------
//   createVolta
//   Create MuseScore volta (ending) spanning measures.
//---------------------------------------------------------

void MnxImporter::create_volta(engraving::Measure *measure, const model::Ending &ending) {
    constexpr track_idx_t volta_track_idx = 0; /// @todo more options as indicated by mnx spec.

    // build_index checks that the ending fits in the score.
    Measure *end_measure = measure;
    for (int countdown = ending.duration - 1; countdown > 0 && end_measure->next_measure(); countdown--) {
        end_measure = end_measure->next_measure();
    }

    Volta *volta = Factory::create_volta(score_->dummy());
    volta->set_track(volta_track_idx);
    volta->set_tick(measure->tick());
    volta->set_tick2(end_measure->end_tick());
    volta->set_visible(true);
    if (const auto &numbers = ending.numbers) {
        volta->set_endings(*numbers);
        // use default MuseScore ending text format, based on observed defaults in 4.6.x
        String text;
        for (int number : *numbers) {
            if (!text.empty()) {
                text += u", ";
            }
            text += String(u"%1").arg(number);
        }
        text += u".";
        volta->set_text(text);
    }
    volta->set_volta_type(ending.open_or_default() ? Volta::Type::OPEN : Volta::Type::CLOSED);
    score_->add_element(volta);
}

//---------------------------------------------------------
//   createJumpOrMarker
//---------------------------------------------------------

void MnxImporter::create_jump_or_marker(engraving::Measure *measure, std::variant<JumpType, MarkerType> type,
                                        const std::optional<std::string> &glyph_name) {
    constexpr track_idx_t cur_track_idx = 0; /// @todo more options as offered by new versions of mnx spec.

    const ElementType element_type = std::holds_alternative<JumpType>(type) ? ElementType::JUMP : ElementType::MARKER;

    TextBase *item = to_text_base(Factory::create_item(element_type, measure));
    item->set_track(cur_track_idx);

    std::visit(
        [&](const auto &v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, JumpType>) {
                IF_ASSERT_FAILED(item->is_jump()) { throw std::logic_error("Variant is JumpType but created item is not a Jump."); }
                IF_ASSERT_FAILED(v != JumpType::USER) { throw std::logic_error("JumpType USER not supported."); }
                to_jump(item)->set_jump_type(v);
            } else if constexpr (std::is_same_v<T, MarkerType>) {
                IF_ASSERT_FAILED(item->is_marker()) { throw std::logic_error("Variant is MarkerType but created item is not a Marker."); }
                to_marker(item)->set_marker_type(v);
                to_marker(item)->reset_property(engraving::Pid::LABEL);
            } else {
                static_assert(!sizeof(T), "Unhandled std::variant alternative in createJumpOrMarker");
            }
        },
        type);

    if (item->text_style_type() == TextStyleType::REPEAT_RIGHT) {
        // MuseScore should do this but doesn't, at least for Fine.
        item->set_position(score_->style().value(Sid::REPEAT_RIGHT_POSITION).value<AlignH>());
    }

    if (glyph_name.has_value()) {
        if (SymNames::sym_id_by_name(glyph_name.value()) != SymId::NO_SYM) {
            item->set_xml_text(String(u"<sym>%1</sym>").arg(String::from_std_string(glyph_name.value())));
        }
    }

    measure->add(item);
}

//---------------------------------------------------------
//   createTempoMark
//   Create MuseScore tempo text from an MNX tempo element.
//---------------------------------------------------------

void MnxImporter::create_tempo_mark(engraving::Measure *measure, const model::Tempo &tempo) {
    /// @todo more options as offered by new versions of mnx spec.
    constexpr track_idx_t cur_track_idx = 0;

    Fraction r_tick(0, 1);
    if (const auto &location = tempo.location) {
        r_tick = to_muse_score_r_tick(*location);
    }
    Segment *s = measure->get_chord_rest_or_time_tick_segment(measure->tick() + r_tick);

    TempoText *item = Factory::create_tempo_text(s);
    item->set_track(cur_track_idx);

    Fraction note_value_in_quarters = to_muse_score_fraction(model::duration_of(tempo.value)) / Fraction(1, 4);
    const double bps = (note_value_in_quarters.to_double() * tempo.bpm) / 60.0;
    item->set_tempo(bps);

    String tempo_text = TempoText::duration2tempo_text_string(to_muse_score_duration(tempo.value));
    tempo_text += String(u" = %1").arg(tempo.bpm);
    item->set_xml_text(tempo_text);

    s->add(item);
}

//---------------------------------------------------------
//   importGlobalMeasures
//   Build MuseScore measures and global items from MNX global section.
//---------------------------------------------------------

void MnxImporter::import_global_measures() {
    Fraction curr_time_sig(4, 4);
    score_->sigmap()->clear();
    score_->sigmap()->add(0, curr_time_sig);

    // pass 1 creates the measures as it goes
    int last_display_num = 0;
    const auto &global_measures = root().global.measures;
    for (size_t measure_index = 0; measure_index < global_measures.size(); ++measure_index) {
        const model::MeasureGlobal &mnx_measure = global_measures[measure_index];
        const bool is_first = measure_index == 0;
        Measure *measure = Factory::create_measure(score_);
        Fraction tick(score_->last() ? score_->last()->end_tick() : Fraction(0, 1));
        measure->set_tick(tick);
        if (const auto &mnx_time_sig = mnx_measure.time) {
            Fraction this_time_sig = time_sig_fraction(*mnx_time_sig);
            if (!this_time_sig.identical(curr_time_sig)) {
                score_->sigmap()->add(tick.ticks(), this_time_sig);
                curr_time_sig = this_time_sig;
            }
            create_time_sig(measure, *mnx_time_sig);
        }
        measure->set_timesig(curr_time_sig);
        measure->set_ticks(curr_time_sig);

        if (const auto &key_sig = mnx_measure.key; key_sig || is_first) {
            const int key_fifths = key_sig ? key_sig->fifths : 0;
            create_key_sig(measure, key_fifths);
        }
        if (const auto &barline = mnx_measure.barline) {
            set_barline(measure, *barline);
        }
        if (mnx_measure.repeat_start) {
            measure->set_repeat_start(true);
        }
        if (const auto &rpt = mnx_measure.repeat_end) {
            measure->set_repeat_end(true);
            if (const auto n_times = rpt->times) {
                measure->set_repeat_count(*n_times);
            }
        }
        if (mnx_measure.fine) {
            create_jump_or_marker(measure, MarkerType::FINE);
        }
        if (const auto &jump = mnx_measure.jump) {
            create_jump_or_marker(measure, to_muse_score_jump_type(jump->type));
        }
        if (const auto &segno = mnx_measure.segno) {
            create_jump_or_marker(measure, MarkerType::SEGNO, segno->glyph);
        }
        if (const auto &tempos = mnx_measure.tempos) {
            for (const auto &tempo : *tempos) {
                create_tempo_mark(measure, tempo);
            }
        }
        if (const auto &mnx_fermata = mnx_measure.fermata) {
            Segment *const segment = measure->get_segment_r(SegmentType::END_BAR_LINE, measure->ticks());
            add_fermata(segment, *mnx_fermata, 0);
        }

        /// @todo MNX currently offers no way to exclude a measure from having
        /// a measure number.
        int curr_display_num = mnx_measure.number.value_or(static_cast<int>(measure_index) + 1);
        if (curr_display_num != last_display_num + 1) {
            measure->set_measure_number_offset(curr_display_num - last_display_num - 1);
        }
        last_display_num = curr_display_num;

        score_->measures()->append(measure);
        mnx_meas_to_tick_.emplace(measure_index, tick);
    }

    // pass 2 for items that require all measures to exist already
    for (size_t measure_index = 0; measure_index < global_measures.size(); ++measure_index) {
        if (const auto &ending = global_measures[measure_index].ending) {
            create_volta(mnx_measure_to_measure(measure_index), *ending);
        }
    }
}

//---------------------------------------------------------
//   createClefs
//   Import clefs for a part's staves within a measure.
//---------------------------------------------------------

void MnxImporter::create_clefs(size_t part_index, const std::vector<model::PositionedClef> &mnx_clefs, engraving::Measure *measure) {
    /// @todo honor the MNX clef glyph if MuseScore ever allows it.
    for (const model::PositionedClef &mnx_clef : mnx_clefs) {
        staff_idx_t staff_idx = mnx_part_staff_to_staff_idx(part_index, mnx_clef.staff_or_default());
        Fraction r_tick{};
        if (const auto &position = mnx_clef.position) {
            r_tick = to_muse_score_r_tick(*position).reduced();
        }
        ClefType clef_type = to_muse_score_clef_type(mnx_clef.clef);
        if (clef_type != ClefType::INVALID) {
            const bool is_header = !measure->prev_measure() && r_tick.is_zero();
            Segment *clef_seg = measure->get_segment_r(is_header ? SegmentType::HEADER_CLEF : SegmentType::CLEF, r_tick);
            Clef *clef = Factory::create_clef(clef_seg);
            clef->set_track(staff2track(staff_idx));
            clef->set_concert_clef(clef_type);
            clef->set_transposing_clef(clef_type);
            clef->set_generated(false);
            clef_seg->add(clef);
        } else {
            LOGE() << "Unsupported clef in part " << part_index << " at measure " << measure->measure_index();
        }
    }
}

//---------------------------------------------------------
//   importMnx
//   Main entry to import an MNX document into a MuseScore score.
//---------------------------------------------------------

void MnxImporter::import_mnx() {
    // The document was validated when it was loaded, so importing it reports no input errors.
    // TransactionManager::transaction() runs endTransaction() only on the normal path, and
    // Transaction is a non-owning handle with no destructor, so an unexpected exception (an
    // engine bug or an allocation failure) escaping this callback would leave the score holding
    // an open transaction. Keep it inside, let the transaction close, then rethrow.
    std::exception_ptr failure;
    score_->transaction_manager()->transaction(muse::TranslatableString::untranslatable("MNX import"), [&](Transaction &) {
        try {
            if (const auto &support = root().mnx.support) {
                use_beams_ = support->use_beams_or_default();
                use_accidental_display_ = support->use_accidental_display_or_default();
            }
            import_settings();
            import_parts();
            import_brackets();
            import_global_measures();
            import_part_measures();
        } catch (...) {
            failure = std::current_exception();
        }
    });

    if (failure) {
        std::rethrow_exception(failure);
    }
}
} // namespace mu::iex::mnxio
