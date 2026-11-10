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

#include <mnxdom/mnxdom.h>

#include "engraving/dom/articulation.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/property.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/types/symid.h"
#include "internal/shared/mnxtypesconv.h"
#include "log.h"
#include "mnximporter.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
//---------------------------------------------------------
//   addArticulation
//   Helper to add a specific articulation to a chord.
//---------------------------------------------------------

Articulation *MnxImporter::add_articulation(ChordRest *cr, const mnx::sequence::EventMarkingBase &marking, SymId sym_id, const char *name) {
    if (!cr->is_chord()) {
        LOGW() << "Skipping MNX articulation \"" << name << "\" on rest at " << marking.pointer().to_string();
        return nullptr;
    }
    Articulation *articulation = Factory::create_articulation(cr);
    articulation->set_sym_id(sym_id);
    set_and_style_property(articulation, Pid::ARTICULATION_ANCHOR, int(to_muse_score_articulation_anchor(marking.orient())));
    cr->add(articulation);
    return articulation;
}

//---------------------------------------------------------
//   addFermata
//   Helper to create and add a fermata to a segment.
//---------------------------------------------------------

void MnxImporter::add_fermata(Segment *seg, const mnx::Fermata &mnx_fermata, track_idx_t track) {
    IF_ASSERT_FAILED(seg) {
        LOGE() << "Null segment passed to addFermata";
        return;
    }
    Fermata *fermata = Factory::create_fermata(seg);
    fermata->set_track(track);
    /// @note MNX decouples fermata symbol from duration, but MuseScore does not currently model
    /// an independently round-trippable fermata duration separate from the fermata subtype/symbol.
    /// Therefore, we ignore the `duration` value in MNX in favor of the `symbol` to determine
    /// the Fermata's symbol.
    fermata->set_sym_id_and_time_stretch(to_muse_score_fermata_sym_id(mnx_fermata.symbol()));
    set_and_style_property(fermata, Pid::PLACEMENT, to_muse_score_placement_v(mnx_fermata.orient(), fermata));
    seg->add(fermata);
}

//---------------------------------------------------------
//   importMarkings
//   Import all articulations/markings for an MNX event.
//---------------------------------------------------------

void MnxImporter::import_markings(const mnx::sequence::Event &mnx_event, ChordRest *cr, const Fraction &event_end_tick, Measure *measure) {
    if (!mnx_event.markings()) {
        return;
    }
    const auto markings = mnx_event.markings().value();
    if (const auto accent = markings.accent()) {
        import_accent(accent.value(), cr);
    }
    if (const auto bow_direction = markings.bowDirection()) {
        import_bow_direction(bow_direction.value(), cr);
    }
    if (const auto breath = markings.breath()) {
        import_breath(breath.value(), cr, event_end_tick, measure);
    }
    if (const auto soft_accent = markings.softAccent()) {
        import_soft_accent(soft_accent.value(), cr);
    }
    if (const auto spiccato = markings.spiccato()) {
        import_spiccato(spiccato.value(), cr);
    }
    if (const auto staccatissimo = markings.staccatissimo()) {
        import_staccatissimo(staccatissimo.value(), cr);
    }
    if (const auto staccato = markings.staccato()) {
        import_staccato(staccato.value(), cr);
    }
    if (const auto stress = markings.stress()) {
        import_stress(stress.value(), cr);
    }
    if (const auto strong_accent = markings.strongAccent()) {
        import_strong_accent(strong_accent.value(), cr);
    }
    if (const auto tenuto = markings.tenuto()) {
        import_tenuto(tenuto.value(), cr);
    }
    if (const auto tremolo = markings.tremolo()) {
        import_tremolo(tremolo.value(), cr);
    }
    if (const auto unstress = markings.unstress()) {
        import_unstress(unstress.value(), cr);
    }
}

//---------------------------------------------------------
//   importAccent
//   Import MNX accent marking.
//---------------------------------------------------------

void MnxImporter::import_accent(const mnx::sequence::Accent &accent, ChordRest *cr) {
    add_articulation(cr, accent, SymId::ARTIC_ACCENT_ABOVE, "accent");
}

//---------------------------------------------------------
//   importBowDirection
//   Import MNX bow direction marking.
//---------------------------------------------------------

void MnxImporter::import_bow_direction(const mnx::sequence::BowDirection &bow_direction, ChordRest *cr) {
    switch (bow_direction.direction()) {
        case mnx::MarkingUpDown::Down: add_articulation(cr, bow_direction, SymId::STRINGS_DOWN_BOW, "downBow"); break;
        case mnx::MarkingUpDown::Up: add_articulation(cr, bow_direction, SymId::STRINGS_UP_BOW, "upBow"); break;
    }
}

//---------------------------------------------------------
//   importBreath
//   Import MNX breath mark.
//---------------------------------------------------------

void MnxImporter::import_breath(const mnx::sequence::BreathMark &breath, ChordRest *cr, const Fraction &event_end_tick, Measure *measure) {
    // cr->track() is read unconditionally below, so a null cr is not survivable here.
    IF_ASSERT_FAILED(cr) { return; }
    Measure *target_measure = measure;
    if (!target_measure) {
        Segment *chord_rest_segment = cr->segment();
        target_measure = chord_rest_segment ? chord_rest_segment->measure() : nullptr;
    }
    IF_ASSERT_FAILED(target_measure) {
        LOGE() << "cr has no measure when importing breath mark";
        return;
    }

    Segment *segment = target_measure->get_segment_r(SegmentType::BREATH, event_end_tick);
    Breath *breath_mark = Factory::create_breath(segment);
    breath_mark->set_track(cr->track());
    breath_mark->set_sym_id(to_muse_score_breath_mark_sym(breath.symbol()));
    set_and_style_property(breath_mark, Pid::PLACEMENT, to_muse_score_placement_v(breath.orient(), breath_mark));
    segment->add(breath_mark);
}

//---------------------------------------------------------
//   importSoftAccent
//   Import MNX soft accent marking.
//---------------------------------------------------------

void MnxImporter::import_soft_accent(const mnx::sequence::SoftAccent &soft_accent, ChordRest *cr) {
    add_articulation(cr, soft_accent, SymId::ARTIC_SOFT_ACCENT_ABOVE, "softAccent");
}

//---------------------------------------------------------
//   importSpiccato
//   Import MNX spiccato marking.
//---------------------------------------------------------

void MnxImporter::import_spiccato(const mnx::sequence::Spiccato &spiccato, ChordRest *cr) {
    add_articulation(cr, spiccato, SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE, "spiccato");
}

//---------------------------------------------------------
//   importStaccatissimo
//   Import MNX staccatissimo marking.
//---------------------------------------------------------

void MnxImporter::import_staccatissimo(const mnx::sequence::Staccatissimo &staccatissimo, ChordRest *cr) {
    add_articulation(cr, staccatissimo, SymId::ARTIC_STACCATISSIMO_ABOVE, "staccatissimo");
}

//---------------------------------------------------------
//   importStaccato
//   Import MNX staccato marking.
//---------------------------------------------------------

void MnxImporter::import_staccato(const mnx::sequence::Staccato &staccato, ChordRest *cr) {
    add_articulation(cr, staccato, SymId::ARTIC_STACCATO_ABOVE, "staccato");
}

//---------------------------------------------------------
//   importStress
//   Import MNX stress marking.
//---------------------------------------------------------

void MnxImporter::import_stress(const mnx::sequence::Stress &stress, ChordRest *cr) {
    add_articulation(cr, stress, SymId::ARTIC_STRESS_ABOVE, "stress");
}

//---------------------------------------------------------
//   importStrongAccent
//   Import MNX strong accent marking.
//---------------------------------------------------------

void MnxImporter::import_strong_accent(const mnx::sequence::StrongAccent &strong_accent, ChordRest *cr) {
    SymId sym_id = SymId::ARTIC_MARCATO_ABOVE;
    if (strong_accent.pointing() == mnx::MarkingUpDownAuto::Down) {
        sym_id = SymId::ARTIC_MARCATO_BELOW;
    }
    add_articulation(cr, strong_accent, sym_id, "strongAccent");
}

//---------------------------------------------------------
//   importTenuto
//   Import MNX tenuto marking.
//---------------------------------------------------------

void MnxImporter::import_tenuto(const mnx::sequence::Tenuto &tenuto, ChordRest *cr) {
    add_articulation(cr, tenuto, SymId::ARTIC_TENUTO_ABOVE, "tenuto");
}

//---------------------------------------------------------
//   importTremolo
//   Import MNX single-note tremolo marking.
//---------------------------------------------------------

void MnxImporter::import_tremolo(const mnx::sequence::SingleNoteTremolo &tremolo, ChordRest *cr) {
    if (!cr->is_chord()) {
        LOGW() << "Skipping MNX tremolo on rest at " << tremolo.pointer().to_string();
        return;
    }
    const int marks = static_cast<int>(tremolo.marks());
    if (marks <= 0) {
        return;
    }
    int tremolo_type_value = int(TremoloType::R8) - 1 + marks;
    tremolo_type_value = std::clamp(tremolo_type_value, int(TremoloType::R8), int(TremoloType::R256));
    TremoloType type = TremoloType(tremolo_type_value);
    if (type == TremoloType::INVALID_TREMOLO) {
        return;
    }
    TremoloSingleChord *tremolo_mark = Factory::create_tremolo_single_chord(to_chord(cr));
    tremolo_mark->set_tremolo_type(type);
    cr->add(tremolo_mark);
}

//---------------------------------------------------------
//   importUnstress
//   Import MNX unstress marking.
//---------------------------------------------------------

void MnxImporter::import_unstress(const mnx::sequence::Unstress &unstress, ChordRest *cr) {
    add_articulation(cr, unstress, SymId::ARTIC_UNSTRESS_ABOVE, "unstress");
}

//---------------------------------------------------------
//   importFermata
//   Import MNX fermata
//---------------------------------------------------------

void MnxImporter::import_fermata(const mnx::Fermata &mnx_fermata, engraving::ChordRest *cr) {
    Segment *seg = cr->segment();
    IF_ASSERT_FAILED(seg) {
        LOGE() << "cr has no segment when importing fermata";
        return;
    }
    add_fermata(seg, mnx_fermata, cr->track());
}
} // namespace mu::iex::mnxio
