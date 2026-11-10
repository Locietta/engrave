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

Articulation *MnxImporter::add_articulation(ChordRest *cr, model::Orientation orient, SymId sym_id, const char *name) {
    if (!cr->is_chord()) {
        LOGW() << "Skipping MNX articulation \"" << name << "\" on rest at tick " << cr->tick().to_string();
        return nullptr;
    }
    Articulation *articulation = Factory::create_articulation(cr);
    articulation->set_sym_id(sym_id);
    set_and_style_property(articulation, Pid::ARTICULATION_ANCHOR, int(to_muse_score_articulation_anchor(orient)));
    cr->add(articulation);
    return articulation;
}

//---------------------------------------------------------
//   addFermata
//   Helper to create and add a fermata to a segment.
//---------------------------------------------------------

void MnxImporter::add_fermata(Segment *seg, const model::Fermata &mnx_fermata, track_idx_t track) {
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
    fermata->set_sym_id_and_time_stretch(to_muse_score_fermata_sym_id(mnx_fermata.symbol_or_default()));
    set_and_style_property(fermata, Pid::PLACEMENT, to_muse_score_placement_v(mnx_fermata.orient_or_default(), fermata));
    seg->add(fermata);
}

//---------------------------------------------------------
//   importMarkings
//   Import all articulations/markings for an MNX event.
//---------------------------------------------------------

void MnxImporter::import_markings(const model::Event &mnx_event, ChordRest *cr, const Fraction &event_end_tick, Measure *measure) {
    if (!mnx_event.markings) {
        return;
    }
    const auto &markings = *mnx_event.markings;
    if (markings.accent) {
        add_articulation(cr, markings.accent->orient_or_default(), SymId::ARTIC_ACCENT_ABOVE, "accent");
    }
    if (markings.bow_direction) {
        import_bow_direction(*markings.bow_direction, cr);
    }
    if (markings.breath) {
        import_breath(*markings.breath, cr, event_end_tick, measure);
    }
    if (markings.soft_accent) {
        add_articulation(cr, markings.soft_accent->orient_or_default(), SymId::ARTIC_SOFT_ACCENT_ABOVE, "softAccent");
    }
    if (markings.spiccato) {
        add_articulation(cr, markings.spiccato->orient_or_default(), SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE, "spiccato");
    }
    if (markings.staccatissimo) {
        add_articulation(cr, markings.staccatissimo->orient_or_default(), SymId::ARTIC_STACCATISSIMO_ABOVE, "staccatissimo");
    }
    if (markings.staccato) {
        add_articulation(cr, markings.staccato->orient_or_default(), SymId::ARTIC_STACCATO_ABOVE, "staccato");
    }
    if (markings.stress) {
        add_articulation(cr, markings.stress->orient_or_default(), SymId::ARTIC_STRESS_ABOVE, "stress");
    }
    if (markings.strong_accent) {
        import_strong_accent(*markings.strong_accent, cr);
    }
    if (markings.tenuto) {
        add_articulation(cr, markings.tenuto->orient_or_default(), SymId::ARTIC_TENUTO_ABOVE, "tenuto");
    }
    if (markings.tremolo) {
        import_tremolo(*markings.tremolo, cr);
    }
    if (markings.unstress) {
        add_articulation(cr, markings.unstress->orient_or_default(), SymId::ARTIC_UNSTRESS_ABOVE, "unstress");
    }
}

//---------------------------------------------------------
//   importBowDirection
//   Import MNX bow direction marking.
//---------------------------------------------------------

void MnxImporter::import_bow_direction(const model::BowDirection &bow_direction, ChordRest *cr) {
    switch (bow_direction.direction) {
        case model::UpDown::DOWN: add_articulation(cr, bow_direction.orient_or_default(), SymId::STRINGS_DOWN_BOW, "downBow"); break;
        case model::UpDown::UP: add_articulation(cr, bow_direction.orient_or_default(), SymId::STRINGS_UP_BOW, "upBow"); break;
    }
}

//---------------------------------------------------------
//   importBreath
//   Import MNX breath mark.
//---------------------------------------------------------

void MnxImporter::import_breath(const model::BreathMark &breath, ChordRest *cr, const Fraction &event_end_tick, Measure *measure) {
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
    breath_mark->set_sym_id(to_muse_score_breath_mark_sym(breath.symbol));
    set_and_style_property(breath_mark, Pid::PLACEMENT, to_muse_score_placement_v(breath.orient_or_default(), breath_mark));
    segment->add(breath_mark);
}

//---------------------------------------------------------
//   importStrongAccent
//   Import MNX strong accent marking.
//---------------------------------------------------------

void MnxImporter::import_strong_accent(const model::StrongAccent &strong_accent, ChordRest *cr) {
    SymId sym_id = SymId::ARTIC_MARCATO_ABOVE;
    if (strong_accent.pointing_or_default() == model::UpDownAuto::DOWN) {
        sym_id = SymId::ARTIC_MARCATO_BELOW;
    }
    add_articulation(cr, strong_accent.orient_or_default(), sym_id, "strongAccent");
}

//---------------------------------------------------------
//   importTremolo
//   Import MNX single-note tremolo marking.
//---------------------------------------------------------

void MnxImporter::import_tremolo(const model::TremoloSingle &tremolo, ChordRest *cr) {
    if (!cr->is_chord()) {
        LOGW() << "Skipping MNX tremolo on rest at tick " << cr->tick().to_string();
        return;
    }
    const int marks = tremolo.marks;
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
//   importFermata
//   Import MNX fermata
//---------------------------------------------------------

void MnxImporter::import_fermata(const model::Fermata &mnx_fermata, engraving::ChordRest *cr) {
    Segment *seg = cr->segment();
    IF_ASSERT_FAILED(seg) {
        LOGE() << "cr has no segment when importing fermata";
        return;
    }
    add_fermata(seg, mnx_fermata, cr->track());
}
} // namespace mu::iex::mnxio