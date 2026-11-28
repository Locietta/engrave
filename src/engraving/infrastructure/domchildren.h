/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "dom/accidental.h"
#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/beam.h"
#include "dom/box.h"
#include "dom/chord.h"
#include "dom/chordrest.h"
#include "dom/deadslapped.h"
#include "dom/engravingitem.h"
#include "dom/fret.h"
#include "dom/guitarbend.h"
#include "dom/hammeronpulloff.h"
#include "dom/harmony.h"
#include "dom/hook.h"
#include "dom/jianpudurationline.h"
#include "dom/jianpuoctavedot.h"
#include "dom/ledgerline.h"
#include "dom/lyrics.h"
#include "dom/measure.h"
#include "dom/measurebase.h"
#include "dom/measurenumber.h"
#include "dom/mmrest.h"
#include "dom/mmrestrange.h"
#include "dom/note.h"
#include "dom/notedot.h"
#include "dom/parenthesis.h"
#include "dom/part.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/sharedpart.h"
#include "dom/spacer.h"
#include "dom/spanner.h"
#include "dom/staff.h"
#include "dom/stafflines.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/tabdurationsymbol.h"
#include "dom/tapping.h"
#include "dom/text.h"
#include "dom/textlinebase.h"
#include "dom/tie.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/tuplet.h"

namespace mu::engraving {

template <typename Fn>
void for_each_layout_payload_child(const EngravingItem &owner, const EngravingItem::LayoutData &data, Fn &&visit) {
    if (owner.is_chord_rest()) {
        const auto *cr_data = static_cast<const ChordRest::LayoutData *>(&data);
        visit(cr_data->tab_dur);
        for (auto *line : cr_data->jianpu_duration_lines) visit(line);
    }
    switch (owner.type()) {
        case ElementType::NOTE: {
            const auto *note_data = static_cast<const Note::LayoutData *>(&data);
            for (auto *dot : note_data->dots) visit(dot);
            for (auto *dot : note_data->jianpu_octave_dots) visit(dot);
            break;
        }
        case ElementType::REST:
        case ElementType::MMREST:
        case ElementType::MEASURE_REPEAT: {
            const auto *rest_data = static_cast<const Rest::LayoutData *>(&data);
            for (auto *dot : rest_data->dots) visit(dot);
            break;
        }
        case ElementType::CHORD: {
            const auto *chord_data = static_cast<const Chord::LayoutData *>(&data);
            visit(chord_data->stem);
            visit(chord_data->hook);
            visit(chord_data->stem_slash);
            for (const auto &group : chord_data->parentheses) {
                visit(group.left);
                visit(group.right);
            }
            for (auto *line : chord_data->ledger_lines) visit(line);
            break;
        }
        case ElementType::TUPLET: {
            const auto *tuplet_data = static_cast<const Tuplet::LayoutData *>(&data);
            visit(tuplet_data->number);
            break;
        }
        case ElementType::GUITAR_BEND: {
            const auto *bend_data = static_cast<const GuitarBend::LayoutData *>(&data);
            visit(bend_data->hold_line);
            for (auto *seg : bend_data->segments) visit(seg);
            break;
        }
        case ElementType::TAPPING: {
            const auto *tapping_data = static_cast<const Tapping::LayoutData *>(&data);
            visit(tapping_data->half_slur_above);
            visit(tapping_data->half_slur_below);
            break;
        }
        case ElementType::SEGMENT: {
            const auto *seg_data = static_cast<const Segment::LayoutData *>(&data);
            for (auto *copy : seg_data->display_copies) visit(copy);
            for (auto *ann : seg_data->display_annotations) visit(ann);
            for (auto *gen : seg_data->generated) visit(gen);
            break;
        }
        case ElementType::MEASURE: {
            const auto *meas_data = static_cast<const Measure::LayoutData *>(&data);
            visit(meas_data->mm_rest);
            for (auto *seg : meas_data->segments) visit(seg);
            break;
        }
        default:
            if (owner.is_spanner() || owner.type() == ElementType::TEXTLINE_BASE) {
                const auto *span_data = static_cast<const Spanner::LayoutData *>(&data);
                for (auto *seg : span_data->segments) visit(seg);
            }
            break;
    }
}

template <typename Fn>
void for_each_owned_item_child(EngravingItem &item, Fn &&visit) {
    visit(item.left_paren());
    visit(item.right_paren());

    if (item.is_chord_rest()) {
        auto *cr = to_chord_rest(&item);
        for (auto *lyric : cr->lyrics()) visit(lyric);
        for (auto *el : cr->el()) visit(el);
    }
    if (item.is_text_line_base_segment()) {
        auto *seg = to_text_line_base_segment(&item);
        visit(seg->text());
        visit(seg->end_text());
    }
    if (item.is_measure_base()) {
        auto *mb = to_measure_base(&item);
        for (auto *el : mb->el()) visit(el);
        if (item.is_t_box()) visit(to_t_box(&item)->text());
    }

    switch (item.type()) {
        case ElementType::NOTE: {
            auto *note = to_note(&item);
            visit(note->accidental());
            visit(note->tie_for());
            for (auto *el : note->el()) visit(el);
            break;
        }
        case ElementType::CHORD: {
            auto *chord = to_chord(&item);
            for (auto *note : chord->notes()) visit(note);
            for (auto *grace : chord->grace_notes()) visit(grace);
            for (auto *art : chord->articulations()) visit(art);
            visit(chord->arpeggio());
            visit(chord->tremolo_single_chord());
            if (auto *tremolo = chord->tremolo_two_chord(); tremolo && tremolo->chord1() == chord) visit(tremolo);
            break;
        }
        case ElementType::REST:
        case ElementType::MMREST:
        case ElementType::MEASURE_REPEAT: visit(to_rest(&item)->dead_slapped()); break;
        case ElementType::FRET_DIAGRAM: visit(to_fret_diagram(&item)->harmony()); break;
        case ElementType::TAPPING: visit(to_tapping(&item)->text()); break;
        case ElementType::GUITAR_BEND_SEGMENT: visit(to_guitar_bend_segment(&item)->bend_text()); break;
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT:
            for (auto *txt : to_hammer_on_pull_off_segment(&item)->hopo_text()) visit(txt);
            break;
        case ElementType::SEGMENT: {
            auto *seg = to_segment(&item);
            for (auto *el : seg->elist()) visit(el);
            for (auto *ann : seg->annotations()) visit(ann);
            break;
        }
        case ElementType::MEASURE: {
            auto *m = to_measure(&item);
            for (auto *row : m->mstaves()) {
                if (!row) continue;
                visit(row->measure_number());
                visit(row->mm_range_text());
                visit(row->lines());
                visit(row->vspacer_up());
                visit(row->vspacer_down());
            }
            for (auto *s = m->first(); s; s = s->next()) visit(s);
            break;
        }
        default: break;
    }
}

template <typename Fn>
void for_each_owned_item_child(const EngravingItem &item, Fn &&visit) {
    for_each_owned_item_child(const_cast<EngravingItem &>(item),
                              [&](EngravingItem *child) { visit(static_cast<const EngravingItem *>(child)); });
}

} // namespace mu::engraving
