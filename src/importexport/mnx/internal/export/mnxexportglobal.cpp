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
#include <cmath>
#include <optional>

#include "engraving/dom/barline.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/key.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurebase.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/timesig.h"
#include "internal/shared/mnxtypesconv.h"
#include "log.h"
#include "mnxexporter.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
namespace {
struct MeasureNumberState {
    MeasureNumberState() { init(); }

    int display_number(const Measure *measure) {
        update_for_measure(measure);
        return display_number_;
    }

private:
    void init() {
        measure_number_ = 1;
        measure_number_offset_ = 0;
        display_number_ = 1;
    }

    void update_for_measure(const Measure *measure) {
        const MeasureBase *previous_mb = measure->prev();
        if (previous_mb) {
            previous_mb = previous_mb->mb_with_preceding_section_break();
        }
        if (previous_mb) {
            const LayoutBreak *layout_break = previous_mb->section_break_element();
            if (layout_break && layout_break->start_with_measure_one()) {
                init();
            }
        }

        measure_number_offset_ = measure->measure_number_offset();
        measure_number_ += measure_number_offset_;

        if (measure->is_anacrusis()) {
            display_number_ = 0;
            return;
        }

        display_number_ = measure_number_++;
    }

    int measure_number_ = 1;
    int measure_number_offset_ = 0;
    int display_number_ = 1;
};
} // namespace

//---------------------------------------------------------
//   assignTimeSignature
//---------------------------------------------------------

/// Finds the time signature element that this measure introduces, if any. The
/// measure's own timesig() is only a fraction, so this is the only source for the glyph.
static const TimeSig *find_time_sig_element(const Measure *measure) {
    const Segment *segment = measure->find_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
    if (!segment) {
        return nullptr;
    }
    for (const EngravingItem *item : segment->elist()) {
        if (item && item->is_time_sig()) {
            return to_time_sig(item);
        }
    }
    return nullptr;
}

static void assign_time_signature(model::MeasureGlobal &mnx_measure, const Measure *measure, std::optional<Fraction> &prev_time_sig,
                                  std::optional<model::TimeSignatureDisplay> &prev_display) {
    const Fraction time_sig = measure->timesig();
    const TimeSig *time_sig_element = find_time_sig_element(measure);
    const auto display = to_mnx_time_signature_display(time_sig_element ? time_sig_element->time_sig_type() : TimeSigType::NORMAL);

    // A change of glyph alone (2/2 to cut time, say) still needs to be emitted.
    if (prev_time_sig && time_sig.identical(*prev_time_sig) && display == prev_display) {
        return;
    }

    const auto unit = to_mnx_time_signature_unit(time_sig.denominator());
    if (!unit) {
        LOGW() << "Skipping time signature with unsupported MNX time signature unit: " << time_sig.denominator();
        return;
    }

    model::Time mnx_time_sig;
    mnx_time_sig.count = time_sig.numerator();
    mnx_time_sig.unit = *unit;
    mnx_time_sig.display = display;
    mnx_measure.time = mnx_time_sig;
    prev_time_sig = time_sig;
    prev_display = display;
}

//---------------------------------------------------------
//   assignKeySignature
//---------------------------------------------------------

static void assign_key_signature(model::MeasureGlobal &mnx_measure, const Score *score, const Measure *measure,
                                 std::optional<int> &prev_key_fifths) {
    if (score->staves().empty()) {
        return;
    }

    const Staff *staff = score->staff(0);
    const KeySigEvent key_sig_event = staff->key_sig_event(measure->tick());
    if (!key_sig_event.is_valid()) {
        return;
    }

    const int key_fifths = static_cast<int>(key_sig_event.concert_key());
    if (key_fifths != prev_key_fifths) {
        mnx_measure.key = model::Key{.fifths = key_fifths};
        prev_key_fifths = key_fifths;
    }

    prev_key_fifths = key_fifths;
}

//---------------------------------------------------------
//   assignBarline
//---------------------------------------------------------

static void assign_barline(model::MeasureGlobal &mnx_measure, const Measure *measure) {
    if (!measure->end_bar_line_visible()) {
        mnx_measure.barline = model::Barline{.type = model::BarlineType::NO_BARLINE};
        return;
    }

    model::BarlineType barline_type = to_mnx_bar_line_type(measure->end_bar_line_type());
    if (barline_type == model::BarlineType::REGULAR) {
        if (const BarLine *barline = measure->end_bar_line()) {
            const int span_from = barline->span_from();
            const int span_to = barline->span_to();
            const bool is_short =
                (span_from == ::mu::engraving::barline_span_shor_t1_from && span_to == ::mu::engraving::barline_span_shor_t1_to) ||
                (span_from == ::mu::engraving::barline_span_shor_t2_from && span_to == ::mu::engraving::barline_span_shor_t2_to);
            if (is_short) {
                barline_type = model::BarlineType::SHORT;
            } else if (const Staff *staff = barline->staff()) {
                const Fraction tick = barline->segment() ? barline->segment()->tick() : measure->tick();
                const int lines = staff->lines(tick - Fraction::eps()) - 1;
                const bool is_one_line = (lines <= 0);
                if (is_one_line) {
                    const int tick_from = ::mu::engraving::barline_span_tic_k1_from + ::mu::engraving::barline_span_1_linestaff_from;
                    const int tick_to = ::mu::engraving::barline_span_1_linestaff_from + 1;
                    if (span_from == tick_from && span_to == tick_to) {
                        barline_type = model::BarlineType::TICK;
                    }
                } else {
                    const bool is_tick =
                        (span_from == ::mu::engraving::barline_span_tic_k1_from && span_to == ::mu::engraving::barline_span_tic_k1_to) ||
                        (span_from == ::mu::engraving::barline_span_tic_k2_from && span_to == ::mu::engraving::barline_span_tic_k2_to);
                    if (is_tick) {
                        barline_type = model::BarlineType::TICK;
                    }
                }
            }
        }
    }
    const bool is_last_measure = (measure->next_measure() == nullptr);

    if (is_last_measure && barline_type != model::BarlineType::FINAL) {
        mnx_measure.barline = model::Barline{.type = barline_type};
    } else if (!is_last_measure && barline_type != model::BarlineType::REGULAR) {
        mnx_measure.barline = model::Barline{.type = barline_type};
    }
}

//---------------------------------------------------------
//   assignRepeats
//---------------------------------------------------------

static void assign_repeats(model::MeasureGlobal &mnx_measure, const Measure *measure) {
    if (measure->repeat_start()) {
        mnx_measure.repeat_start.emplace();
    }

    if (measure->repeat_end()) {
        auto &repeat_end = ensure(mnx_measure.repeat_end);
        const int repeat_count = measure->repeat_count();
        if (repeat_count > 0 && repeat_count != 2) {
            repeat_end.times = repeat_count;
        }
    }
}

//---------------------------------------------------------
//   createTempo
//   emit a MNX tempo entry from a TempoText item
//---------------------------------------------------------

static void create_tempo(model::MeasureGlobal &mnx_measure, const TempoText *tempo, const Fraction &rel_tick) {
    IF_ASSERT_FAILED(tempo) { return; }

    const auto location = to_mnx_fraction(rel_tick.reduced());
    const double bpm = tempo->tempo_bpm();
    if (bpm <= 0.0) {
        LOGW() << "Skipping tempo with invalid beats-per-minute " << bpm;
        return;
    }

    const TDuration dur = tempo->duration();
    const auto note_value = to_mnx_note_value(dur);
    if (!note_value) {
        LOGW() << "Skipping tempo with invalid duration value";
        return;
    }

    model::Tempo mnx_tempo;
    mnx_tempo.bpm = static_cast<int>(std::lround(bpm));
    mnx_tempo.value = *note_value;
    if (rel_tick.is_not_zero()) {
        mnx_tempo.location = model::RhythmicPosition{.fraction = location};
    }
    ensure(mnx_measure.tempos).push_back(std::move(mnx_tempo));
}

//---------------------------------------------------------
//   exportMeasureElements
//   export measure-level elements (jumps, markers, tempos)
//---------------------------------------------------------

static void export_measure_elements(model::MeasureGlobal &mnx_measure, const Measure *measure) {
    for (EngravingItem *item : measure->el()) {
        IF_ASSERT_FAILED(item) { continue; }

        const Fraction rel_tick = item->tick() - measure->tick();
        const model::RhythmicPosition location{.fraction = to_mnx_fraction(rel_tick.reduced())};
        const model::RhythmicPosition location_end{.fraction = to_mnx_fraction(measure->ticks().reduced())}; // end of measure (relative)
        const auto location_or_end = rel_tick.is_zero() ? location_end : location;

        switch (item->type()) {
            case ElementType::JUMP: {
                const Jump *jump = to_jump(item);
                IF_ASSERT_FAILED(jump) { break; }
                if (const auto mnx_jt = to_mnx_jump_type(jump->jump_type())) {
                    /// @note Current jump types default to measure end when stored at tick 0.
                    /// When MNX adds new types, revisit placement of new types.
                    mnx_measure.jump = model::Jump{.location = location_or_end, .type = *mnx_jt};
                }
                break;
            }
            case ElementType::MARKER: {
                const Marker *marker = to_marker(item);
                IF_ASSERT_FAILED(marker) { break; }
                if (marker->marker_type() == MarkerType::FINE) {
                    mnx_measure.fine = model::Fine{.location = location_or_end};
                } else if (marker->is_segno()) {
                    mnx_measure.segno = model::Segno{.location = location};
                    /// @todo Export segno glyph when appropriate (defer pending MNX spec clarifications).
                }
                break;
            }
            default: break;
        }
    }

    for (const Segment *segment = measure->first(); segment; segment = segment->next()) {
        for (const EngravingItem *item : segment->annotations()) {
            IF_ASSERT_FAILED(item) { continue; }

            const Fraction rel_tick = item->tick() - measure->tick();
            switch (item->type()) {
                case ElementType::TEMPO_TEXT: create_tempo(mnx_measure, to_tempo_text(item), rel_tick); break;
                default: break;
            }
        }
    }

    if (const BarLine *bar_line = measure->end_bar_line()) {
        if (const Segment *seg = bar_line->segment()) {
            for (const EngravingItem *item : seg->annotations()) {
                IF_ASSERT_FAILED(item) { continue; }
                if (item->is_fermata()) {
                    const Fermata *fermata = to_fermata(item);
                    DO_ASSERT(fermata);
                    if (fermata) {
                        mnx_measure.fermata = MnxExporter::mnx_fermata_from_fermata(fermata);
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------
//   createGlobal
//---------------------------------------------------------

void MnxExporter::create_global() {
    /// @note Lyrics metadata is exported at the end of `createParts`.
    /// @todo Export global sounds dictionary (mnx::global::Sound).

    if (!score_) {
        return;
    }

    auto &mnx_measures = root_.global.measures;
    MeasureNumberState measure_number_state;
    std::optional<Fraction> prev_time_sig;
    std::optional<model::TimeSignatureDisplay> prev_time_sig_display;
    std::optional<int> prev_key_fifths;
    size_t measure_index = 0;

    for (const Measure *measure = score_->first_measure(); measure; measure = measure->next_measure()) {
        auto &mnx_measure = mnx_measures.emplace_back();
        mnx_measure.id = model::Id(get_or_assign_eid(const_cast<Measure *>(measure)).to_std_string());
        meas_to_mnx_meas_.emplace(measure, measure_index);

        assign_barline(mnx_measure, measure);
        assign_repeats(mnx_measure, measure);
        assign_time_signature(mnx_measure, measure, prev_time_sig, prev_time_sig_display);
        assign_key_signature(mnx_measure, score_, measure, prev_key_fifths);
        export_measure_elements(mnx_measure, measure);

        const int display_number = measure_number_state.display_number(measure);
        if (display_number == 0) {
            /// @todo MNX does not support pickup measures; export as measure 0.
            mnx_measure.number = display_number;
        } else if (display_number != static_cast<int>(measure_index + 1)) {
            mnx_measure.number = display_number;
        }

        ++measure_index;
    }
}
} // namespace mu::iex::mnxio
