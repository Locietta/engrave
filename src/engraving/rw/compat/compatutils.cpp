/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "compatutils.h"

#include "dom/chord.h"
#include "dom/image.h"
#include "dom/instrchange.h"
#include "dom/instrument.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/mscore.h"
#include "dom/note.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staffstate.h"
#include "dom/tempotext.h"
#include "dom/textlinebase.h"
#include "draw/fontmetrics.h"
#include "draw/types/font.h"
#include "editing/editchord.h"
#include "global/containers.h"
#include "style/defaultstyle.h"
#include "style/styledef.h"
#include "style/textstyle.h"
#include "types/types.h"

using namespace mu::engraving;
using namespace mu::engraving::compat;

const muse::FlatMap<Sid, Sid> CompatUtils::align_vals_to_convert{
    {Sid::DEFAULT_ALIGN, Sid::DEFAULT_POSITION},
    {Sid::TITLE_ALIGN, Sid::TITLE_POSITION},
    {Sid::SUB_TITLE_ALIGN, Sid::SUB_TITLE_POSITION},
    {Sid::COMPOSER_ALIGN, Sid::COMPOSER_POSITION},
    {Sid::LYRICIST_ALIGN, Sid::LYRICIST_POSITION},
    {Sid::LYRICS_EVEN_ALIGN, Sid::LYRICS_EVEN_POSITION},
    {Sid::LYRICS_ODD_ALIGN, Sid::LYRICS_ODD_POSITION},
    {Sid::FINGERING_ALIGN, Sid::FINGERING_POSITION},
    {Sid::LH_GUITAR_FINGERING_ALIGN, Sid::LH_GUITAR_FINGERING_POSITION},
    {Sid::RH_GUITAR_FINGERING_ALIGN, Sid::RH_GUITAR_FINGERING_POSITION},
    {Sid::HAMMER_ON_PULL_OFF_TAPPING_ALIGN, Sid::HAMMER_ON_PULL_OFF_TAPPING_POSITION},
    {Sid::STRING_NUMBER_ALIGN, Sid::STRING_NUMBER_POSITION},
    {Sid::STAFF_TEXT_ALIGN, Sid::STAFF_TEXT_POSITION},
    {Sid::FRET_DIAGRAM_FINGERING_ALIGN, Sid::FRET_DIAGRAM_FINGERING_POSITION},
    {Sid::FRET_DIAGRAM_FRET_NUMBER_ALIGN, Sid::FRET_DIAGRAM_FRET_NUMBER_POSITION},
    {Sid::HARP_PEDAL_DIAGRAM_ALIGN, Sid::HARP_PEDAL_DIAGRAM_POSITION},
    {Sid::HARP_PEDAL_TEXT_DIAGRAM_ALIGN, Sid::HARP_PEDAL_TEXT_DIAGRAM_POSITION},
    {Sid::LONG_INSTRUMENT_ALIGN, Sid::LONG_INSTRUMENT_POSITION},
    {Sid::SHORT_INSTRUMENT_ALIGN, Sid::SHORT_INSTRUMENT_POSITION},
    {Sid::PART_INSTRUMENT_ALIGN, Sid::PART_INSTRUMENT_POSITION},
    {Sid::DYNAMICS_ALIGN, Sid::DYNAMICS_POSITION},
    {Sid::EXPRESSION_ALIGN, Sid::EXPRESSION_POSITION},
    {Sid::TEMPO_ALIGN, Sid::TEMPO_POSITION},
    {Sid::TEMPO_CHANGE_ALIGN, Sid::TEMPO_CHANGE_POSITION},
    {Sid::METRONOME_ALIGN, Sid::METRONOME_POSITION},
    {Sid::MEASURE_NUMBER_ALIGN, Sid::MEASURE_NUMBER_POSITION},
    {Sid::MM_REST_RANGE_ALIGN, Sid::MM_REST_RANGE_POSITION},
    {Sid::TRANSLATOR_ALIGN, Sid::TRANSLATOR_POSITION},
    {Sid::TUPLET_ALIGN, Sid::TUPLET_POSITION},
    {Sid::SYSTEM_TEXT_ALIGN, Sid::SYSTEM_TEXT_POSITION},
    {Sid::CHORD_SYMBOL_A_ALIGN, Sid::CHORD_SYM_POSITION},
    {Sid::REPEAT_LEFT_ALIGN, Sid::REPEAT_LEFT_POSITION},
    {Sid::REPEAT_RIGHT_ALIGN, Sid::REPEAT_RIGHT_POSITION},
    {Sid::FRAME_ALIGN, Sid::FRAME_POSITION},
    {Sid::TEXT_LINE_TEXT_ALIGN, Sid::TEXT_LINE_POSITION},
    {Sid::NOTE_LINE_ALIGN, Sid::NOTE_LINE_POSITION},
    {Sid::GLISSANDO_ALIGN, Sid::GLISSANDO_POSITION},
    {Sid::OTTAVA_TEXT_ALIGN_ABOVE, Sid::OTTAVA_POSITION},
    {Sid::VOLTA_ALIGN, Sid::VOLTA_POSITION},
    {Sid::PEDAL_TEXT_ALIGN, Sid::PEDAL_POSITION},
    {Sid::LET_RING_TEXT_ALIGN, Sid::LET_RING_POSITION},
    {Sid::PALM_MUTE_TEXT_ALIGN, Sid::PALM_MUTE_POSITION},
    {Sid::HAIRPIN_TEXT_ALIGN, Sid::HAIRPIN_POSITION},
    {Sid::BEND_ALIGN, Sid::BEND_POSITION},
    {Sid::HEADER_ALIGN, Sid::HEADER_POSITION},
    {Sid::FOOTER_ALIGN, Sid::FOOTER_POSITION},
    {Sid::COPYRIGHT_ALIGN, Sid::COPYRIGHT_POSITION},
    {Sid::PAGE_NUMBER_ALIGN, Sid::PAGE_NUMBER_POSITION},
    {Sid::INSTRUMENT_CHANGE_ALIGN, Sid::INSTRUMENT_CHANGE_POSITION},
    {Sid::STICKING_ALIGN, Sid::STICKING_POSITION},
    {Sid::USER1_ALIGN, Sid::USER1_POSITION},
    {Sid::USER2_ALIGN, Sid::USER2_POSITION},
    {Sid::USER3_ALIGN, Sid::USER3_POSITION},
    {Sid::USER4_ALIGN, Sid::USER4_POSITION},
    {Sid::USER5_ALIGN, Sid::USER5_POSITION},
    {Sid::USER6_ALIGN, Sid::USER6_POSITION},
    {Sid::USER7_ALIGN, Sid::USER7_POSITION},
    {Sid::USER8_ALIGN, Sid::USER8_POSITION},
    {Sid::USER9_ALIGN, Sid::USER9_POSITION},
    {Sid::USER10_ALIGN, Sid::USER10_POSITION},
    {Sid::USER11_ALIGN, Sid::USER11_POSITION},
    {Sid::USER12_ALIGN, Sid::USER12_POSITION},
};

void CompatUtils::do_compatibility_conversions(MasterScore *master_score) {
    if (!master_score) {
        return;
    }

    if (master_score->msc_version() < 500) {
        remove_mm_rest_elements(master_score);
    }
}

ArticulationAnchor CompatUtils::translate_to_new_articulation_anchor(int anchor) {
    switch (anchor) {
        case 0: // ArticulationAnchor::TOP_STAFF
        case 3: // ArticulationAnchor::TOP_CHORD
            return ArticulationAnchor::TOP;
        case 1: // ArticulationAnchor::BOTTOM_STAFF
        case 4: // ArticulationAnchor::BOTTOM_CHORD
            return ArticulationAnchor::BOTTOM;
        case 2: // ArticulationAnchor::CHORD
        default: return ArticulationAnchor::AUTO;
    }
}

double CompatUtils::convert_chord_ext_mod_units(double val) {
    // Pre 4.6 this value was in 1/5 of a spatium
    // After 4.6 this is in % of root cap height
    // The best we can do for conversion of old files is to assume a default spatium of 1.75mm and a default font size of 10pt
    // The height value is calculated from Edwin at 10pt using FontMetrics::capHeight
    const double default_spatium = StyleDef::style_values[static_cast<size_t>(Sid::SPATIUM)].default_value.to_double();
    muse::draw::Font f(u"Edwin", muse::draw::Font::Type::TEXT);
    f.set_point_size_f(10);
    const double default_font_cap_height = muse::draw::FontMetrics::cap_height(f); // 121

    return ((val / 5) * default_spatium) / default_font_cap_height;
}

Sid CompatUtils::position_style_from_align(Sid align) {
    return muse::value(::mu::engraving::compat::CompatUtils::align_vals_to_convert, align, Sid::NOSTYLE);
}

void CompatUtils::set_position_styles_from_align(MStyle *style, std::vector<Sid> ignore_sids) {
    // Make sure new position styles are initially the same as align values
    for (const StyleDef::StyleValue &st : StyleDef::style_values) {
        if (muse::contains(ignore_sids, st.sid)) {
            continue;
        }
        Sid position_sid = CompatUtils::position_style_from_align(st.sid);
        if (position_sid == Sid::NOSTYLE) {
            continue;
        }
        AlignH val = style->value(st.sid).value<Align>().horizontal;
        style->set(position_sid, val);
    }
}

void CompatUtils::set_text_line_text_position_from_align(TextLineBase *tl) {
    tl->set_begin_text_position(tl->begin_text_align().horizontal);
    if (tl->begin_text_position() != tl->property_default(Pid::BEGIN_TEXT_POSITION).value<AlignH>()) {
        tl->set_property_flags(Pid::BEGIN_TEXT_POSITION, PropertyFlags::UNSTYLED);
    }
    tl->set_continue_text_position(tl->continue_text_align().horizontal);
    if (tl->continue_text_position() != tl->property_default(Pid::CONTINUE_TEXT_POSITION).value<AlignH>()) {
        tl->set_property_flags(Pid::CONTINUE_TEXT_POSITION, PropertyFlags::UNSTYLED);
    }
    tl->set_end_text_position(tl->end_text_align().horizontal);
    if (tl->end_text_position() != tl->property_default(Pid::END_TEXT_POSITION).value<AlignH>()) {
        tl->set_property_flags(Pid::END_TEXT_POSITION, PropertyFlags::UNSTYLED);
    }
}

void CompatUtils::set_music_symbol_size470(MStyle &style) {
    // Music symbols have their own point size in 4.7
    // Initialize this to the text type's default font size
    for (TextStyleType text_style_type : all_text_styles()) {
        if (text_style_type == TextStyleType::REPEAT_LEFT || text_style_type == TextStyleType::REPEAT_RIGHT) {
            continue;
        }

        const TextStyle *ts = text_style(text_style_type);
        Sid music_symbol_size_sid = Sid::NOSTYLE;
        Sid font_size_sid = Sid::NOSTYLE;

        for (size_t i = 0; i < ::mu::engraving::text_style_size; ++i) {
            if (ts->at(i).pid == Pid::MUSIC_SYMBOL_SIZE) {
                music_symbol_size_sid = ts->at(i).sid;
            }

            if (ts->at(i).pid == Pid::FONT_SIZE) {
                font_size_sid = ts->at(i).sid;
            }
        }

        if (music_symbol_size_sid == Sid::NOSTYLE || font_size_sid == Sid::NOSTYLE) {
            continue;
        }

        double size = style.value(font_size_sid).to_double();
        if (text_style_type == TextStyleType::TEMPO) {
            size *= TempoText::default_sym_size_ratio;
        }

        style.set(music_symbol_size_sid, size);
    }
}

void CompatUtils::do_migrate_note_parens(EngravingItem *item) {
    if (!item->is_note()) {
        return;
    }
    Note *note = to_note(item);
    if (!note->left_paren() && !note->right_paren()) {
        return;
    }

    if (note->left_paren()) {
        note->remove(note->left_paren());
    }

    if (note->right_paren()) {
        note->remove(note->right_paren());
    }

    Chord *chord = note->chord();

    EditChord::add_chord_parentheses(chord, {note});
}

static constexpr double pre_470_dpi = 360;

Spatium CompatUtils::convert_pre470_frame_radius(double frame_radius) {
    // The frame radius used to be expressed in raster units and divided by 2 at drawing. Since 4.7 it is expressed in spatium.
    return Spatium(frame_radius * (::mu::engraving::dpi / ::pre_470_dpi) / DefaultStyle::base_style().value(Sid::SPATIUM).to_double()) / 2;
}

void CompatUtils::convert_pre470_image_size(Image *image) {
    if (image->size().is_null() && image->score()->msc_version() < 470) {
        image->init();
        image->set_size(image->size() * (::mu::engraving::dpi / ::pre_470_dpi));
    }
}

PointF CompatUtils::get_adjusted_offset(EngravingItem *item, PointF offset) {
    PointF default_offset = item->default_pos();
    return offset - default_offset;
}

void CompatUtils::migrate_offset500(EngravingItem *item, PropertyValue &offset) {
    if (!item->is_text_base() && !item->is_spanner() && !item->is_spanner_segment()) {
        return;
    }

    // Items with voice assignment properties depend on post-layout placement context.
    if (item->has_voice_assignment_properties()) {
        return;
    }

    offset = get_adjusted_offset(item, offset.value<PointF>());
}

static void reregister_instrument_changes(Score *score) {
    /* When reading the <5.0 InstrumentChange MMRest copy, it has overwritten the Part's InstrumentList
     * entry for this segment's tick. Deleting the copies unregistered their Instruments from the
     * InstrumentList. So we need to restore the InstrumentList entries with the originals. */
    for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &seg : to_measure(mb)->segments()) {
            for (EngravingItem *annotation : seg.annotations()) {
                Instrument *instr = nullptr;
                if (annotation->is_instrument_change()) {
                    instr = to_instrument_change(annotation)->instrument();
                } else if (annotation->is_staff_state() && to_staff_state(annotation)->staff_state_type() == StaffStateType::INSTRUMENT) {
                    instr = to_staff_state(annotation)->instrument();
                }
                if (instr) {
                    annotation->part()->set_instrument_at(*instr, seg.tick());
                }
            }
        }
    }
}

void CompatUtils::remove_mm_rest_elements(MasterScore *master_score) {
    // <5.0 MMRests had copies of underlying elements. We now move the element when toggling the rests.
    // Remove redundant copies which are written to the file

    for (Score *score : master_score->score_list()) {
        bool removed_instrument_change = false;

        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            if (!mb->is_measure()) {
                continue;
            }
            Measure *m = to_measure(mb);
            Measure *mmrest = m->mm_rest();

            if (!mmrest) {
                continue;
            }

            // Remove jumps/markers/layout breaks
            std::vector<EngravingItem *> measure_els = mmrest->el();
            for (EngravingItem *el : measure_els) {
                el->unlink();
                score->remove_element(el);
                delete el;
            }

            // Remove annotations
            for (Segment &seg : mmrest->segments()) {
                std::vector<EngravingItem *> annotations = seg.annotations();
                for (EngravingItem *annotation : annotations) {
                    removed_instrument_change |=
                        annotation->is_instrument_change() ||
                        (annotation->is_staff_state() && to_staff_state(annotation)->staff_state_type() == StaffStateType::INSTRUMENT);
                    annotation->unlink();
                    score->remove_element(annotation);
                    delete annotation;
                }
                if (seg.is_chord_rest_type()) {
                    continue;
                }
                // Remove time sigs, key sigs, clefs, ambitus, breaths, barlines
                for (staff_idx_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
                    EngravingItem *el = seg.element(staff2track(staff_idx));
                    if (!el) {
                        continue;
                    }
                    el->unlink();
                    score->remove_element(el);
                    delete el;
                }
            }
        }
        if (removed_instrument_change) {
            reregister_instrument_changes(score);
        }
    }
}
