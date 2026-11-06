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
#include "log.h"
#include <array>
#include <exception>
#include <vector>

#include "mnximporter.h"
#include "internal/shared/mnxtypesconv.h"

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

static Drumset *create_drumset(const mnx::Part &mnx_part, const mnx::Document &doc,
                               std::map<std::pair<size_t, std::string>, int> &kit_component_to_midi) {
    if (!mnx_part.kit()) {
        return nullptr;
    }

    Drumset *drumset = new Drumset();
    Drumset *default_drumset = mu::engraving::sm_drumset;
    for (int i = 0; i < ::mu::engraving::drum_instruments; ++i) {
        drumset->drum(i) = DrumInstrument(String(), NoteHeadGroup::HEAD_INVALID, 0, DirectionV::AUTO);
    }

    const auto sounds = doc.global().sounds();
    const StaffType *perc_staff_type = StaffType::preset(StaffTypes::PERC_DEFAULT);
    /// @todo If MNX gains explicit staff type info, use it here instead of PERC_DEFAULT.
    const int middle_line = perc_staff_type ? perc_staff_type->middle_line() : 4;
    const size_t part_idx = mnx_part.calcArrayIndex();
    const auto kit = mnx_part.kit().value();

    struct KitEntry {
        std::string id;
        mnx::part::KitComponent component;
        int midi_pitch = -1;
    };

    std::vector<KitEntry> kit_entries;
    kit_entries.reserve(kit.size());
    for (const auto &[kit_id, kit_component] : kit) {
        kit_entries.push_back({kit_id, kit_component, -1});
    }

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
        if (entry.component.sound() && sounds && sounds->contains(entry.component.sound().value())) {
            const auto sound = sounds->at(entry.component.sound().value());
            if (const auto midi_number = sound.midiNumber()) {
                entry.midi_pitch = static_cast<int>(midi_number.value());
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
        int fallback_pitch = default_drumset ? default_drumset->default_pitch_for_line(middle_line - entry.component.staffPosition()) : -1;
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
        if (entry.component.name()) {
            name = String::from_std_string(entry.component.name().value());
        }
        if (name.is_empty() && entry.component.sound() && sounds && sounds->contains(entry.component.sound().value())) {
            const auto sound = sounds->at(entry.component.sound().value());
            if (const auto sound_name = sound.name()) {
                name = String::from_std_string(sound_name.value());
            }
        }
        if (name.is_empty()) {
            // MuseScore requires a note name
            name = String(u"Percussion note");
        }

        int line = middle_line - entry.component.staffPosition();
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
//   Populate a MuseScore Instrument from an MNX part definition.
//---------------------------------------------------------

static void load_instrument(mnx::Document &doc, Part *part, mnx::Part &mnx_part, Instrument *instrument,
                            std::map<std::pair<size_t, std::string>, int> &kit_component_to_midi) {
    // Initialize drumset
    if (mnx_part.kit().has_value()) {
        instrument->set_use_drumset(true);
        Drumset *drumset = create_drumset(mnx_part, doc, kit_component_to_midi);
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
    if (const std::optional<mnx::part::PartTransposition> mnx_transp = mnx_part.transposition()) {
        instrument->set_transpose(Interval(-mnx_transp->interval().staffDistance(), -mnx_transp->interval().halfSteps()));
        const auto key_fifths_flip = mnx_transp->keyFifthsFlipAt();
        part->set_prefer_sharp_flat(to_muse_score_prefer_sharp_flat(key_fifths_flip.value_or(0)));
        const Interval mnx_transpose(mnx_transp->interval().staffDistance(), mnx_transp->interval().halfSteps());
        const int updated_flip_at = to_mnx_key_fifths_flip_value(part->prefer_sharp_flat(), mnx_transpose);
        if (key_fifths_flip && key_fifths_flip.value() != updated_flip_at) {
            LOGW() << "MNX keyFifthsFlipAt value (" << key_fifths_flip.value() << ") cannot be imported for part (" << part->part_name()
                   << "). Using (" << updated_flip_at << ") instead.";
        }
        auto mnx_transposition = mnx_part.ensure_transposition(mnx_transp->interval());
        mnx_transposition.set_keyFifthsFlipAt(updated_flip_at);
    }
}

//---------------------------------------------------------
//   mnxPartStaffToStaffIdx
//   Map MNX part/staff numbers to MuseScore staff indices.
//---------------------------------------------------------

staff_idx_t MnxImporter::mnx_part_staff_to_staff_idx(const mnx::Part &mnx_part, int staff_num) {
    staff_idx_t idx = muse::value(mnx_part_staff_to_staff_, std::make_pair(mnx_part.calcArrayIndex(), staff_num), muse::nidx);
    IF_ASSERT_FAILED(idx != muse::nidx) { throw std::logic_error("Unmapped staff encountered"); }
    return idx;
}

//---------------------------------------------------------
//   mnxLayoutStaffToStaffIdx
//   Resolve MNX layout staff sources to MuseScore staff indices.
//---------------------------------------------------------

staff_idx_t MnxImporter::mnx_layout_staff_to_staff_idx(const mnx::layout::Staff &mnx_staff) {
    const auto sources = mnx_staff.sources();
    for (const auto &source : sources) {
        if (const auto part = mnx_document().getEntityMap().tryGet<mnx::Part>(source.part())) {
            return mnx_part_staff_to_staff_idx(part.value(), source.staff());
        } else {
            LOGE() << "Staff source points to invalid part\"" << source.part() << "\" " << source.pointer().to_string();
            LOGE() << source.dump(2);
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

engraving::ChordRest *MnxImporter::mnx_event_id_to_cr(const std::string &event_id) {
    const auto &doc_mapping = mnx_document().getEntityMap();
    const auto event = doc_mapping.tryGet<mnx::sequence::Event>(event_id);
    if (!event.has_value()) {
        return nullptr;
    }
    return muse::value(mnx_event_to_cr_, event->pointer().to_string());
}

//---------------------------------------------------------
//   mnxNoteIdToNote
//   Look up a MuseScore Note by MNX note id.
//---------------------------------------------------------

engraving::Note *MnxImporter::mnx_note_id_to_note(const std::string &note_id) {
    const auto &doc_mapping = mnx_document().getEntityMap();
    const auto note = doc_mapping.tryGet<mnx::sequence::NoteBase>(note_id);
    if (!note.has_value()) {
        return nullptr;
    }
    return muse::value(mnx_note_to_note_, note->pointer().to_string());
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

Fraction MnxImporter::mnx_measure_pos_to_tick(const mnx::MeasureRhythmicPosition &meas_pos) {
    const auto global_meas = mnx_document().getEntityMap().get<mnx::global::Measure>(meas_pos.measure());
    const size_t meas_idx = global_meas.calcArrayIndex();
    const Fraction meas_tick = muse::value(mnx_meas_to_tick_, meas_idx, Fraction(-1, 1));
    IF_ASSERT_FAILED(meas_tick.positive()) {
        throw std::logic_error("MNX global measure at " + std::to_string(meas_idx) + " was not mapped.");
    }
    return meas_tick + to_muse_score_r_tick(meas_pos.position());
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

void MnxImporter::create_staff(Part *part, const mnx::Part &mnx_part, int staff_num) {
    Staff *staff = Factory::create_staff(part);
    if (part->instrument()->use_drumset() && !staff->is_drum_staff(Fraction(0, 1))) {
        /// @todo If MNX gains explicit staff type/line-count info, use it here.
        staff->set_staff_type(Fraction(0, 1), *StaffType::preset(StaffTypes::PERC_DEFAULT));
        staff->set_default_clef_type(ClefTypeList(ClefType::PERC2, ClefType::PERC2));
        staff->clef_list().set_clef(0, ClefTypeList(ClefType::PERC2, ClefType::PERC2));
    }
    score_->append_staff(staff);
    mnx_part_staff_to_staff_.emplace(std::make_pair(mnx_part.calcArrayIndex(), staff_num), staff->idx());
    staff_to_mnx_part_.emplace(staff->idx(), mnx_part.calcArrayIndex());
}

//---------------------------------------------------------
//   importParts
//   Create MuseScore parts, instruments, and staves from MNX parts.
//---------------------------------------------------------

void MnxImporter::import_parts() {
    for (mnx::Part mnx_part : mnx_document().parts()) {
        Part *part = new Part(score_);
        /// @todo a better way to find the instrument, perhaps by part name or else some future mnx enhancement
        const InstrumentTemplate *it = [&]() {
            if (mnx_part.kit()) {
                return search_template(u"drumset");
            }
            return search_template(u"piano");
        }();
        if (it) {
            part->init_from_instr_template(it);
        }
        part->set_long_name(String::from_std_string(mnx_part.name_or("")));
        part->set_short_name(String::from_std_string(mnx_part.shortName_or("")));
        load_instrument(mnx_document(), part, mnx_part, part->instrument(), mnx_kit_component_to_midi_);
        for (int staff_num = 1; staff_num <= mnx_part.staves(); staff_num++) {
            create_staff(part, mnx_part, staff_num);
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

    auto full_score_layout = mnx_document().findFullScoreLayout();
    if (!full_score_layout) {
        LOGI() << "Unable to find full score layout. Using default layout from parts.";
    }
    const auto layout_spans = [&]() {
        if (full_score_layout) {
            if (const auto &layout_spans = mnx::util::buildLayoutSpans(full_score_layout.value())) {
                return layout_spans.value();
            }
            full_score_layout = std::nullopt;
        }
        return mnx::util::buildDefaultLayoutSpans(mnx_document().parts());
    }();

    std::optional<std::vector<mnx::layout::Staff>> layout_staves;
    if (full_score_layout) {
        layout_staves = mnx::util::flattenLayoutStaves(full_score_layout.value());
        IF_ASSERT_FAILED(layout_staves) {
            LOGE() << "Layout staves for full score layout were invalid.";
            return;
        }
    }
    for (const auto &span : layout_spans) {
        BracketType brt = to_muse_score_bracket_type(span.symbol.value_or(mnx::LayoutSymbol::NoSymbol));
        if (brt == BracketType::NO_BRACKET && span.startIndex >= span.endIndex) {
            continue;
        }
        const staff_idx_t staff_idx =
            layout_staves ? mnx_layout_staff_to_staff_idx(layout_staves->at(span.startIndex)) : static_cast<staff_idx_t>(span.startIndex);
        if (staff_idx == muse::nidx) {
            LOGE() << "Staff not found for span starting at " << span.startIndex << " and ending at " << span.endIndex << ".";
            continue;
        }
        BracketItem *bi = Factory::create_bracket_item(score_->dummy());
        bi->set_bracket_type(brt);
        const int group_span = static_cast<int>(span.endIndex - span.startIndex + 1);
        bi->set_bracket_span(group_span);
        bi->set_column(size_t(span.depth));
        EditStaffBrackets::add_bracket(score_, staff_idx, bi);
        if (group_span <= 1 || span.kind != mnx::util::LayoutSpan::Kind::Group) {
            continue;
        }
        const mnx::StaffGroupBarlineOverride group_override = span.barlineOverride;
        if (group_override == mnx::StaffGroupBarlineOverride::None) {
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
            if (override_span.barline_override == mnx::StaffGroupBarlineOverride::Unified) {
                local_span = true;
            }
            const size_t group_size = static_cast<size_t>(override_span.end_staff - override_span.start_staff + 1);
            if (priority > group_size) {
                mensur_striche = override_span.barline_override == mnx::StaffGroupBarlineOverride::Mensurstrich;
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
        if (override_span.barline_override != mnx::StaffGroupBarlineOverride::Mensurstrich) {
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
            const mnx::Part mnx_part = mnx_document().parts().at(mnx_part_index);
            if (const std::optional<mnx::part::PartTransposition> &part_transposition = mnx_part.transposition()) {
                int transp_fifths = part_transposition->calcTransposedKey(mnx::KeySignature::make(key_fifths)).fifths;
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

void MnxImporter::create_time_sig(engraving::Measure *measure, const mnx::TimeSignature &time_sig) {
    const Fraction sig_fraction = to_muse_score_fraction(time_sig);
    // setSig is the only way in; TimeSig has no public setTimeSigType.
    const TimeSigType sig_type = to_muse_score_time_sig_type(time_sig.display());
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

void MnxImporter::set_barline(engraving::Measure *measure, const mnx::global::Barline &barline) {
    const mnx::BarlineType mnx_blt = barline.type();
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
            if (override_span.barline_override == mnx::StaffGroupBarlineOverride::Unified) {
                local_span = true;
            }
            const size_t group_size = static_cast<size_t>(override_span.end_staff - override_span.start_staff + 1);
            if (priority > group_size) {
                mensur_striche = override_span.barline_override == mnx::StaffGroupBarlineOverride::Mensurstrich;
                mensur_last = mensur_striche && idx == override_span.end_staff;
                priority = group_size;
            }
        }
        const bool local_tick = mnx_blt == mnx::BarlineType::Tick;
        const bool local_short = mnx_blt == mnx::BarlineType::Short;
        mensur_striche = mensur_striche && !local_span && !local_tick && !local_short;

        BarLine *bl = Factory::create_bar_line(bls);
        bl->set_track(staff2track(idx));
        bl->set_visible(mnx_blt != mnx::BarlineType::NoBarline);
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

void MnxImporter::create_volta(engraving::Measure *measure, const mnx::global::Ending &ending) {
    constexpr track_idx_t volta_track_idx = 0; /// @todo more options as indicated by mnx spec.

    Measure *end_measure = measure;
    for (int countdown = ending.duration() - 1; countdown > 0; countdown--) {
        Measure *next = end_measure->next_measure();
        if (!next) {
            LOGW() << "Ending at " << ending.pointer().to_string() << " specifies non-existent end measure.";
            LOGW() << ending.dump(2);
        }
        end_measure = next;
    }

    Volta *volta = Factory::create_volta(score_->dummy());
    volta->set_track(volta_track_idx);
    volta->set_tick(measure->tick());
    volta->set_tick2(end_measure->end_tick());
    volta->set_visible(true);
    if (const auto &numbers = ending.numbers()) {
        volta->set_endings(numbers->toStdVector());
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
    volta->set_volta_type(ending.open() ? Volta::Type::OPEN : Volta::Type::CLOSED);
    score_->add_element(volta);
}

//---------------------------------------------------------
//   createJumpOrMarker
//   MuseScore does not allow jumps or markers to be assigned to a measure location.
//   Passed in for completeness to the MNX spec but currently unused by engraving.
//---------------------------------------------------------

void MnxImporter::create_jump_or_marker(engraving::Measure *measure, const mnx::FractionValue &, std::variant<JumpType, MarkerType> type,
                                        const std::optional<std::string> glyph_name) {
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

void MnxImporter::create_tempo_mark(engraving::Measure *measure, const mnx::global::Tempo &tempo) {
    /// @todo more options as offered by new versions of mnx spec.
    constexpr track_idx_t cur_track_idx = 0;

    Fraction r_tick(0, 1);
    if (const auto &location = tempo.location()) {
        r_tick = to_muse_score_fraction(location->fraction());
    }
    Segment *s = measure->get_chord_rest_or_time_tick_segment(measure->tick() + r_tick);

    TempoText *item = Factory::create_tempo_text(s);
    item->set_track(cur_track_idx);

    Fraction note_value_in_quarters = to_muse_score_fraction(tempo.value()) / Fraction(1, 4);
    const double bps = (note_value_in_quarters.to_double() * tempo.bpm()) / 60.0;
    item->set_tempo(bps);

    String tempo_text = TempoText::duration2tempo_text_string(to_muse_score_duration(tempo.value()));
    tempo_text += String(u" = %1").arg(tempo.bpm());
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
    for (const mnx::global::Measure &mnx_measure : mnx_document().global().measures()) {
        const bool is_first = mnx_measure.calcArrayIndex() == 0;
        Measure *measure = Factory::create_measure(score_);
        Fraction tick(score_->last() ? score_->last()->end_tick() : Fraction(0, 1));
        measure->set_tick(tick);
        if (const std::optional<mnx::TimeSignature> &mnx_time_sig = mnx_measure.time()) {
            Fraction this_time_sig = to_muse_score_fraction(mnx_time_sig.value());
            if (!this_time_sig.identical(curr_time_sig)) {
                score_->sigmap()->add(tick.ticks(), this_time_sig);
                curr_time_sig = this_time_sig;
            }
            create_time_sig(measure, mnx_time_sig.value());
        }
        measure->set_timesig(curr_time_sig);
        measure->set_ticks(curr_time_sig);

        if (const std::optional<mnx::KeySignature> &key_sig = mnx_measure.key(); key_sig || is_first) {
            const int key_fifths = key_sig ? key_sig->fifths() : 0;
            create_key_sig(measure, key_fifths);
        }
        if (const std::optional<mnx::global::Barline> &barline = mnx_measure.barline()) {
            set_barline(measure, barline.value());
        }
        if (mnx_measure.repeatStart()) {
            measure->set_repeat_start(true);
        }
        if (const std::optional<mnx::global::RepeatEnd> &rpt = mnx_measure.repeatEnd()) {
            measure->set_repeat_end(true);
            if (const std::optional<int> n_times = rpt->times()) {
                measure->set_repeat_count(n_times.value());
            }
        }
        if (const std::optional<mnx::global::Fine> &fine = mnx_measure.fine()) {
            create_jump_or_marker(measure, fine->location().fraction(), MarkerType::FINE);
        }
        if (const std::optional<mnx::global::Jump> &jump = mnx_measure.jump()) {
            create_jump_or_marker(measure, jump->location().fraction(), to_muse_score_jump_type(jump->type()));
        }
        if (const std::optional<mnx::global::Segno> &segno = mnx_measure.segno()) {
            create_jump_or_marker(measure, segno->location().fraction(), MarkerType::SEGNO, segno->glyph());
        }
        if (const std::optional<mnx::Array<mnx::global::Tempo>> &tempos = mnx_measure.tempos()) {
            for (const auto &tempo : tempos.value()) {
                create_tempo_mark(measure, tempo);
            }
        }
        if (const std::optional<mnx::Fermata> &mnx_fermata = mnx_measure.fermata()) {
            Segment *const segment = measure->get_segment_r(SegmentType::END_BAR_LINE, measure->ticks());
            add_fermata(segment, mnx_fermata.value(), 0);
        }

        /// @todo MNX currently offers no way to exclude a measure from having
        /// a measure number.
        int curr_display_num = mnx_measure.calcVisibleNumber();
        if (curr_display_num != last_display_num + 1) {
            measure->set_measure_number_offset(curr_display_num - last_display_num - 1);
        }
        last_display_num = curr_display_num;

        score_->measures()->append(measure);
        mnx_meas_to_tick_.emplace(mnx_measure.calcArrayIndex(), tick);
    }

    // pass 2 for items that require all measures to exist already
    for (const mnx::global::Measure &mnx_measure : mnx_document().global().measures()) {
        Measure *measure = mnx_measure_to_measure(mnx_measure.calcArrayIndex());
        if (const std::optional<mnx::global::Ending> &ending = mnx_measure.ending()) {
            create_volta(measure, ending.value());
        }
    }
}

//---------------------------------------------------------
//   createClefs
//   Import clefs for a part's staves within a measure.
//---------------------------------------------------------

void MnxImporter::create_clefs(const mnx::Part &mnx_part, const mnx::Array<mnx::part::PositionedClef> &mnx_clefs,
                               engraving::Measure *measure) {
    /// @todo honor the MNX clef glyph if MuseScore ever allows it.
    for (const mnx::part::PositionedClef &mnx_clef : mnx_clefs) {
        staff_idx_t staff_idx = mnx_part_staff_to_staff_idx(mnx_part, mnx_clef.staff());
        Fraction r_tick{};
        if (const std::optional<mnx::RhythmicPosition> &position = mnx_clef.position()) {
            r_tick = to_muse_score_fraction(position->fraction()).reduced();
        }
        ClefType clef_type = to_muse_score_clef_type(mnx_clef.clef());
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
            LOGE() << "Unsupported clef encountered at " << mnx_clef.pointer().to_string();
        }
    }
}

//---------------------------------------------------------
//   importMnx
//   Main entry to import an MNX document into a MuseScore score.
//---------------------------------------------------------

void MnxImporter::import_mnx() {
    // Malformed input makes the lookup helpers throw, and NotationMnxReader catches that to
    // report a failed import. TransactionManager::transaction() runs endTransaction() only on
    // the normal path, and Transaction is a non-owning handle with no destructor, so an
    // exception escaping this callback would leave the score holding an open transaction.
    // Keep it inside, let the transaction close, then rethrow for the reader to handle.
    std::exception_ptr failure;
    score_->transaction_manager()->transaction(muse::TranslatableString::untranslatable("MNX import"), [&](Transaction &) {
        try {
            if (!mnx_document_.hasEntityMap()) {
                auto policies = mnx::EntityMapPolicies();
                policies.ottavasRespectGraceTargets = false; // MuseScore can't target grace notes with ottavas
                policies.ottavasRespectVoiceTargets = false; // MuseScore can't target voices with ottavas
                mnx_document_.buildEntityMap(policies);
            }
            if (const auto &support = mnx_document_.mnx().support()) {
                use_beams_ = support->useBeams();
                use_accidental_display_ = support->useAccidentalDisplay();
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
