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

#include "engravingcompat.h"

#include <set>

#include "dom/beam.h"
#include "dom/box.h"
#include "dom/chord.h"
#include "dom/hammeronpulloff.h"
#include "dom/harmony.h"
#include "dom/instrument.h"
#include "dom/lyrics.h"
#include "dom/marker.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/part.h"
#include "dom/pedal.h"
#include "dom/spanner.h"
#include "dom/staff.h"
#include "dom/system.h"
#include "dom/tapping.h"
#include "editing/editchord.h"
#include "editing/reset.h"
#include "editing/transaction/transaction.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "rw/compat/compatutils.h"
#include "types/types.h"

using namespace mu::engraving;

namespace mu::engraving::compat {
void EngravingCompat::do_pre_layout_compat_if_needed(MasterScore *score) {
    int msc_version = score->msc_version();

    if (msc_version < 400) {
        apply_pre400_style_compat(score);
    }

    if (msc_version < 440) {
        correct_pedal_end_points(score);
        migrate_dynamic_pos_on_vocal_staves(score);
        if (msc_version >= 420) {
            undo_staff_text_exclude_from_part(score);
        }
    }

    if (msc_version < 460) {
        reset_marker_left_font_size(score);
        reset_rest_vertical_offsets(score);
        adjust_v_box_distances(score);
    }

    if (msc_version < 470) {
        pre470_text_compat(score);
        migrate_note_parens(score);
    }

    if (msc_version < 500) {
        reset_ineffective_center_between_staves(score);
    }
}

void EngravingCompat::correct_pedal_end_points(MasterScore *score) {
    // Pedal lines ending with 45° hook used to be hacked to end before their actual end duration.
    // Hack is now removed, so we need to correct them to preserve engraving result. (M.S.)
    for (auto pair : score->spanner()) {
        Spanner *spanner = pair.second;
        if (spanner->is_pedal() && to_pedal(spanner)->end_hook_type() == HookType::HOOK_45) {
            ChordRest *end_cr = score->find_chord_rest_ending_before_tick_in_staff(spanner->tick2(), track2staff(spanner->track()));
            if (end_cr) {
                for (EngravingObject *item : spanner->link_list()) {
                    to_spanner(item)->set_tick2(end_cr->tick());
                }
            }
        }
    }
}

void EngravingCompat::undo_staff_text_exclude_from_part(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            if (!mb->is_measure()) {
                continue;
            }
            for (Segment &segment : to_measure(mb)->segments()) {
                if (!segment.is_chord_rest_type()) {
                    continue;
                }
                for (EngravingItem *item : segment.annotations()) {
                    if (!item || !item->is_staff_text()) {
                        continue;
                    }
                    if (item->exclude_from_other_parts()) {
                        item->undo_change_property(Pid::EXCLUDE_FROM_OTHER_PARTS, false);
                        for (EngravingObject *linked_item : item->link_list()) {
                            if (linked_item == item && !linked_item->score()->is_master()) {
                                to_engraving_item(item)->set_appearance_linked_to_master(false);
                            } else if (linked_item != item) {
                                linked_item->undo_change_property(Pid::VISIBLE, false);
                            }
                        }
                    }
                }
            }
        }
    }
}

void EngravingCompat::migrate_dynamic_pos_on_vocal_staves(MasterScore *master_score) {
    auto migrate_voice_assignment_and_position = [master_score](EngravingItem *item) {
        if (item->voice() != 0) {
            item->set_property(Pid::VOICE_ASSIGNMENT, VoiceAssignment::CURRENT_VOICE_ONLY);
        }
        // Migrate position on vocal staves (to match old default, which used to be below)
        Staff *staff = item->staff();
        Part *part = staff ? staff->part() : nullptr;
        Instrument *instrument = part ? part->instrument() : nullptr;
        const bool is_vocal_instrument = instrument && instrument->is_vocal_instrument();
        const bool direction_is_default = item->get_property(Pid::DIRECTION) == item->property_default(Pid::DIRECTION);
        const PlacementV default_placement = master_score->style().style_v(item->get_property_style(Pid::PLACEMENT)).value<PlacementV>();
        const bool default_is_below = default_placement == PlacementV::BELOW;

        if (is_vocal_instrument && direction_is_default && default_is_below) {
            item->set_property(Pid::DIRECTION, DirectionV::DOWN);
            item->set_property_flags(Pid::DIRECTION, PropertyFlags::UNSTYLED);
        }
    };

    for (Score *score : master_score->score_list()) {
        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            if (!mb->is_measure()) {
                continue;
            }
            for (Segment &segment : to_measure(mb)->segments()) {
                if (!segment.is_chord_rest_type()) {
                    continue;
                }
                for (EngravingItem *item : segment.annotations()) {
                    if (item && item->has_voice_assignment_properties()) {
                        migrate_voice_assignment_and_position(item);
                    }
                }
            }
        }

        for (auto pair : score->spanner()) {
            Spanner *spanner = pair.second;
            if (spanner->is_hairpin()) {
                migrate_voice_assignment_and_position(spanner);
            }
        }
    }
}

void EngravingCompat::reset_ineffective_center_between_staves(MasterScore *master_score) {
    /* Before 5.0, dynamics and hairpins could only be centered between staves of their own part, but
     * they could carry the property, e.g. if the item was copied from a piano part into a vocal part,
     * even if it visually had no effect. Now it can also center between vocal staves, so we need to
     * reset it to keep the old layout. */
    auto reset_if_ineffective = [](EngravingItem *item) {
        const Part *part = item->part();
        if (part && part->nstaves() <= 1 && item->get_property(Pid::CENTER_BETWEEN_STAVES).value<AutoOnOff>() == AutoOnOff::ON) {
            item->reset_property(Pid::CENTER_BETWEEN_STAVES);
        }
    };

    for (Score *score : master_score->score_list()) {
        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            if (!mb->is_measure()) {
                continue;
            }
            for (Segment &segment : to_measure(mb)->segments()) {
                if (!segment.is_chord_rest_type() && !segment.is_time_tick_type()) {
                    continue;
                }
                for (EngravingItem *item : segment.annotations()) {
                    if (item && (item->is_dynamic() || item->is_expression())) {
                        reset_if_ineffective(item);
                    }
                }
            }
        }

        for (auto pair : score->spanner()) {
            Spanner *spanner = pair.second;
            if (spanner->is_hairpin()) {
                reset_if_ineffective(spanner);
            }
        }
    }
}

void EngravingCompat::reset_marker_left_font_size(MasterScore *master_score) {
    // Reset the new incorrect 4.4.0 - 4.4.2 default size of 11 to the previous correct size of 18
    const double incorrect_default_size = 11.0;
    const double correct_default_size = 18.0;
    bool needs_adjust_marker_size = master_score->mscore_version().contains(u"4.4") && master_score->mscore_version() != u"4.4.3";
    if (!needs_adjust_marker_size || master_score->style().style_d(Sid::REPEAT_LEFT_FONT_SIZE) != incorrect_default_size) {
        return;
    }
    master_score->style().set(Sid::REPEAT_LEFT_FONT_SIZE, correct_default_size);

    for (Score *score : master_score->score_list()) {
        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            if (!mb->is_measure()) {
                continue;
            }
            Measure *meas = to_measure(mb);
            for (EngravingItem *item : meas->el()) {
                if (!item->is_marker()) {
                    continue;
                }
                Marker *marker = to_marker(item);
                if (marker->text_style_type() != TextStyleType::REPEAT_LEFT || marker->size() != incorrect_default_size) {
                    continue;
                }
                marker->set_size(correct_default_size);
            }
        }
    }
}

void EngravingCompat::reset_rest_vertical_offsets(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            if (!mb->is_measure()) {
                continue;
            }
            Measure *measure = to_measure(mb);
            for (staff_idx_t staff = 0; staff < score->nstaves(); ++staff) {
                if (!measure->has_voices(staff)) {
                    continue;
                }
                track_idx_t start_track = staff2track(staff);
                track_idx_t end_track = start_track + ::mu::engraving::voices;
                for (Segment &seg : measure->segments()) {
                    if (!seg.is_chord_rest_type()) {
                        continue;
                    }
                    for (track_idx_t track = start_track; track < end_track; ++track) {
                        EngravingItem *item = seg.element(track);
                        if (item && item->is_rest()) {
                            item->reset_property(Pid::OFFSET);
                        }
                    }
                }
            }
        }
    }
}

void EngravingCompat::adjust_v_box_distances(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            MeasureBase *nextmb = mb->next();
            if (mb->is_v_box_base()) {
                VBox *vbox = static_cast<VBox *>(mb);
                vbox->set_property(Pid::PADDING_TO_NOTATION_ABOVE, Spatium()); // Because pre-4.6 these didn't exist
                vbox->set_property_flags(Pid::PADDING_TO_NOTATION_ABOVE, PropertyFlags::UNSTYLED);
                vbox->set_property(Pid::PADDING_TO_NOTATION_BELOW, Spatium());
                vbox->set_property_flags(Pid::PADDING_TO_NOTATION_BELOW, PropertyFlags::UNSTYLED);
                if (nextmb && nextmb->is_v_box_base()) {
                    VBox *first = static_cast<VBox *>(mb);
                    VBox *second = static_cast<VBox *>(nextmb);
                    if (first->bottom_gap() > 0_sp && second->top_gap() > 0_sp) {
                        first->set_property(Pid::BOTTOM_GAP,
                                            first->bottom_gap() + second->top_gap()); // Because pre-4.6 these used to be added
                        first->set_property_flags(Pid::BOTTOM_GAP, PropertyFlags::UNSTYLED);
                    }
                }
            }
        }
    }
}

static constexpr std::array<ElementType, 6> offset_unit_convert_types = {ElementType::FINGERING, ElementType::HAMMER_ON_PULL_OFF_TEXT,
                                                                         ElementType::LYRICS, ElementType::TAPPING};

void EngravingCompat::pre470_text_compat(MasterScore *master_score) {
    auto do_compat = [](EngravingItem *item) {
        if (!item->is_text_base()) {
            return;
        }

        TextBase *text = to_text_base(item);

        if (!text->is_styled(Pid::FRAME_ROUND)) {
            text->set_frame_round(compat::CompatUtils::convert_pre470_frame_radius(text->frame_round().val()));
        }

        // Text offset could have been in mm for these types prior to 4.7
        // Convert actual distance to spatium
        if (muse::contains(::mu::engraving::compat::offset_unit_convert_types, item->type()) && !item->size_is_spatium_dependent()) {
            PointF offset = item->offset();
            double spatium = item->style().spatium();
            offset /= spatium;
            offset *= ::mu::engraving::dpmm;

            item->set_property(Pid::OFFSET, offset);
        }

        // Staff text, system text, and harp pedal diagrams are the only types which are attached to notes and weren't already
        // placed with their left edges / centres / right edges aligned to the left / centre / right of the notehead
        if (text->position_relative_to_notehead_rest() &&
            (text->is_staff_text() || text->is_system_text() || text->is_harp_pedal_diagram())) {
            double mag = item->staff() ? item->staff()->staff_mag(item) : 1.0;
            double x_adj = item->sym_width(SymId::NOTEHEAD_BLACK) * mag;

            switch (text->position()) {
                case AlignH::HCENTER: text->set_property(Pid::OFFSET, PointF(text->offset().x() - x_adj / 2, text->offset().y())); break;
                case AlignH::RIGHT: text->set_property(Pid::OFFSET, PointF(text->offset().x() - x_adj, text->offset().y())); break;
                default: break;
            }
        }
    };

    for (Score *score : master_score->score_list()) {
        score->scan_elements(do_compat);
    }
}

void EngravingCompat::migrate_note_parens(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        score->scan_elements(CompatUtils::do_migrate_note_parens);
    }
}

static void do_migrate_offset500(EngravingItem *item) {
    if (item->offset().is_null() || (!item->is_text_base() && !item->is_spanner() && !item->is_spanner_segment()) ||
        !item->has_voice_assignment_properties()) {
        return;
    }

    item->set_property(Pid::OFFSET, CompatUtils::get_adjusted_offset(item, item->offset()));

    if (!muse::real_is_null(item->offset().y())) {
        // In versions <5 any adjustment to y offset meant we couldn't centre items between staves
        item->set_property(Pid::CENTER_BETWEEN_STAVES, AutoOnOff::OFF);
        if (item->is_styled(Pid::CENTER_BETWEEN_STAVES)) {
            item->set_property_flags(Pid::CENTER_BETWEEN_STAVES, PropertyFlags::UNSTYLED);
        }
    }
}

void EngravingCompat::migrate_offset500(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        for (Spanner *sp : score->spanner_list()) {
            if (!sp->has_voice_assignment_properties()) {
                continue;
            }
            sp->set_placement_based_on_voice_assignment(
                sp->style().style_v(Sid::DYNAMICS_HAIRPIN_VOICE_BASED_PLACEMENT).value<DirectionV>());
            do_migrate_offset500(sp);
            for (SpannerSegment *seg : sp->spanner_segments()) {
                do_migrate_offset500(seg);
            }
        }

        score->scan_elements(do_migrate_offset500);
    }
}

void EngravingCompat::do_post_layout_compat_if_needed(MasterScore *score) {
    bool need_relayout = false;

    int msc_version = score->msc_version();

    if (msc_version < 300) {
        reset_all_elements_positions(score);
        need_relayout = true;
    }

    if (msc_version <= 206) {
        reset_all_cross_beams(score);
        need_relayout = true;
    }

    if (msc_version < 440) {
        need_relayout |= relayout_user_modified_cross_staff_beams(score);
    }

    if (msc_version < 470) {
        need_relayout |= set_lyric_line_visibility(score);
    }

    if (msc_version < 500) {
        migrate_offset500(score);
        AlignmentMigration500::migrate_snapped_and_same_item_type_alignment(score);
        AlignmentMigration500::migrate_hopo_letter_alignment(score);
        migrate_offset_after_autoplace(score);
        need_relayout = true;
    }

    if (need_relayout) {
        score->update();
    }
}

static void do_migrate_offset_after_autoplace(EngravingItem *item) {
    // Before 5.0, item's were autoplaced from their offset position
    // After 5.0, offset is applied after autoplace

    // We can't fix the offset for some types.
    // These are types where layout is spread out in multiple steps and cannot be accurately calculated with one TLayout::layoutItem call.
    if (item->generated() || !autoplace_applies_to_type(item->type()) || !item->autoplace() || item->offset().is_null() ||
        item->is_rest() || item->is_lyrics() || item->is_lyrics_line_segment() || item->is_articulation_family() || item->is_fingering() ||
        item->is_slur_segment() || item->is_tie_segment()) {
        return;
    }

    // Initial position
    PointF pushed_pos = item->ldata()->pos();

    // Calculate item's position without autoplace
    PropertyFlags autoplace_pf = item->property_flags(Pid::AUTOPLACE);
    item->set_property(Pid::AUTOPLACE, false);

    item->mutldata()->set_pos(PointF());
    item->renderer()->layout_item(item);
    PointF origin_pos = item->ldata()->pos();

    item->set_property(Pid::AUTOPLACE, true);
    item->set_property_flags(Pid::AUTOPLACE, autoplace_pf);

    // The difference is the amount autoplace moves the item
    const double difference = pushed_pos.y() - origin_pos.y();
    if (muse::real_is_null(difference)) {
        // No difference, no need to change the offset
        return;
    }

    PointF new_offset = item->offset();
    new_offset.ry() -= difference;

    new_offset.ry() = item->place_above() ? std::min(new_offset.y(), 0.0) : std::max(new_offset.y(), 0.0);

    item->set_property(Pid::OFFSET, new_offset);
    if (item->property_flags(Pid::OFFSET) == PropertyFlags::STYLED) {
        item->set_property_flags(Pid::OFFSET, PropertyFlags::UNSTYLED);
    }
}

void EngravingCompat::migrate_offset_after_autoplace(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        score->scan_elements(do_migrate_offset_after_autoplace);
    }
}

bool EngravingCompat::relayout_user_modified_cross_staff_beams(MasterScore *score) {
    if (score->msc_version() >= 440) {
        return false;
    }
    bool found = false;

    auto find_beam = [&found](ChordRest *cr) {
        Beam *beam = cr->beam();
        if (beam && beam->user_modified() && beam->cross() && beam->elements().front() == cr) {
            found = true;
            beam->trigger_layout();
        }
    };

    for (MeasureBase *mb = score->first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &seg : to_measure(mb)->segments()) {
            if (!seg.is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *item : seg.elist()) {
                if (!item) {
                    continue;
                }
                find_beam(to_chord_rest(item));
                if (item->is_chord()) {
                    for (Chord *grace : to_chord(item)->grace_notes()) {
                        find_beam(grace);
                    }
                }
            }
        }
    }

    return found;
}

bool EngravingCompat::set_lyric_line_visibility(MasterScore *master_score) {
    bool need_relayout = false;
    for (Score *score : master_score->score_list()) {
        for (Spanner *sp : score->unmanaged_spanners()) {
            if (!sp->is_lyrics_line()) {
                continue;
            }

            LyricsLine *ll = to_lyrics_line(sp);
            if (!ll->lyrics() || ll->visible() == ll->lyrics()->visible()) {
                continue;
            }

            ll->set_visible(ll->lyrics()->visible());
            need_relayout = true;
        }
    }

    return need_relayout;
}

void EngravingCompat::reset_all_elements_positions(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();
        Reset::reset_all_positions(tx, score);
    }
}

static void reset_beam_offset(EngravingItem *e) {
    if (e->is_beam() && to_beam(e)->full_cross()) {
        e->reset();
    }
}

void EngravingCompat::reset_all_cross_beams(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        score->scan_elements(reset_beam_offset);
    }
}

void EngravingCompat::apply_pre400_style_compat(MasterScore *master_score) {
    if (MScore::test_mode) {
        return;
    }

    for (Score *score : master_score->score_list()) {
        MStyle &style = score->style();
        const double sp = style.spatium();

        style.set(Sid::DYNAMICS_FONT_SIZE, 10.0);
        double double_bar_distance = style.style_absolute(Sid::DOUBLE_BAR_DISTANCE);
        double_bar_distance -= style.style_absolute(Sid::DOUBLE_BAR_WIDTH);
        style.set(Sid::DOUBLE_BAR_DISTANCE, double_bar_distance / sp);
        double end_bar_distance = style.style_absolute(Sid::END_BAR_DISTANCE);
        end_bar_distance -= (style.style_absolute(Sid::BAR_WIDTH) + style.style_absolute(Sid::END_BAR_WIDTH)) / 2;
        style.set(Sid::END_BAR_DISTANCE, end_bar_distance / sp);
        double repeat_barline_dot_separation = style.style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION);
        double dot_width = score->engraving_font()->width(SymId::REPEAT_DOT, 1.0);
        repeat_barline_dot_separation -= (style.style_absolute(Sid::BAR_WIDTH) + dot_width) / 2;
        style.set(Sid::REPEAT_BARLINE_DOT_SEPARATION, repeat_barline_dot_separation / sp);
        score->reset_style_value(Sid::MEASURE_SPACING);
    }
}

// Migrate item centring & alignment. Before 5.0 offsets to one item would move the whole aligned group
// From 5.0, offsets are applied after centring & alignment which means items can be individually offset from their group
// Replicate the appearance of <5.0 scores by giving all items in a centred or aligned group the same offset
// These functions collect the same groups of items as are collected in AlignmentLayout & SystemLayout, then set their offset to the highest in the group

bool AlignmentMigration500::row_item_is_above(const EngravingItem *item) {
    return item->is_articulation_family() ? to_articulation(item)->ldata()->up() : item->place_above();
}

void AlignmentMigration500::align_item_offset_group(const std::vector<EngravingItem *> &group) {
    if (group.size() < 2) {
        return;
    }

    bool above = row_item_is_above(group.front());
    bool outermost_y_offset_found = false;
    double outermost_y_offset = 0.0;
    for (const EngravingItem *item : group) {
        double y = item->offset().y();
        if (muse::real_is_null(y)) {
            continue;
        }
        if (!outermost_y_offset_found || (above ? y < outermost_y_offset : y > outermost_y_offset)) {
            outermost_y_offset = y;
            outermost_y_offset_found = true;
        }
    }

    if (!outermost_y_offset_found) {
        return;
    }

    for (EngravingItem *item : group) {
        if (!muse::real_is_equal(item->offset().y(), outermost_y_offset)) {
            item->set_property(Pid::OFFSET, PointF(item->offset().x(), outermost_y_offset));
        }
    }
}

void AlignmentMigration500::migrate_snapped_and_same_item_type_alignment(MasterScore *master_score) {
    // For items aligned by AlignmentLayout::alignItemsWithTheirSnappingChain
    // Used for dynamics, expressions, hairpins, tempo text and gradual tempo change lines
    //
    // For items aligned by AlignmentLayout::alignItemsForSystem
    // Used for harmony, fret diagrams and sticking
    using RowKey = std::tuple<const System *, staff_idx_t, bool>;

    for (Score *score : master_score->score_list()) {
        std::vector<EngravingItem *> snapped_candidates;

        std::map<RowKey, std::vector<EngravingItem *>> harmony_groups;
        std::map<RowKey, std::vector<EngravingItem *>> harmony_on_fret_diagram_groups;
        std::map<RowKey, std::vector<EngravingItem *>> fret_groups;
        std::map<RowKey, std::vector<EngravingItem *>> sticking_groups;

        bool align_chord_symbols = score->style().style_b(Sid::VERTICALLY_ALIGN_CHORD_SYMBOLS);

        score->scan_elements([&](EngravingItem *item) {
            if (!item) {
                return;
            }

            if (((item->is_dynamic() && item->autoplace()) || (item->is_expression() && item->add_to_skyline()) ||
                 item->is_hairpin_segment() || item->is_tempo_text() || item->is_gradual_tempo_change_segment())) {
                snapped_candidates.push_back(item);
            }

            if (!item->add_to_skyline() || item->exclude_vertical_align()) {
                return;
            }
            bool is_harmony = align_chord_symbols && item->is_harmony();
            bool is_fret_diagram = align_chord_symbols && item->is_fret_diagram();
            bool is_sticking = item->is_sticking();
            if (!is_harmony && !is_fret_diagram && !is_sticking) {
                return;
            }

            const System *system = to_system(item->find_ancestor(ElementType::SYSTEM));
            if (!system) {
                return;
            }
            RowKey key(system, item->staff_idx(), item->place_above());
            if (is_harmony && !to_harmony(item)->get_parent_fret_diagram()) {
                harmony_groups[key].push_back(item);
            } else if (is_harmony && to_harmony(item)->get_parent_fret_diagram()) {
                harmony_on_fret_diagram_groups[key].push_back(item);
            } else if (is_fret_diagram) {
                fret_groups[key].push_back(item);
            } else {
                sticking_groups[key].push_back(item);
            }
        });

        std::set<EngravingItem *> visited;
        for (EngravingItem *item : snapped_candidates) {
            if (muse::contains(visited, item)) {
                continue;
            }
            const System *system = to_system(item->find_ancestor(ElementType::SYSTEM));
            std::vector<EngravingItem *> group;
            item->ldata()->scan_connected_items_in_snapping_chain(system, [&visited, &group](EngravingItem *chain_item) {
                if (visited.insert(chain_item).second) {
                    group.push_back(chain_item);
                }
            });
            align_item_offset_group(group);
        }

        for (auto &pair : harmony_groups) {
            align_item_offset_group(pair.second);
        }
        for (auto &pair : harmony_on_fret_diagram_groups) {
            align_item_offset_group(pair.second);
        }
        for (auto &pair : fret_groups) {
            align_item_offset_group(pair.second);
        }
        for (auto &pair : sticking_groups) {
            align_item_offset_group(pair.second);
        }
    }
}

void AlignmentMigration500::migrate_hopo_letter_alignment(MasterScore *master_score) {
    // For items aligned by AlignmentLayout::alignHopoLetters
    for (Score *score : master_score->score_list()) {
        const bool align_letters_tab = score->style().style_b(Sid::HOPO_ALIGN_LETTERS_TAB_STAVES);
        const bool align_letters_std = score->style().style_b(Sid::HOPO_ALIGN_LETTERS_STANDARD_STAVES);

        for (Spanner *spanner : score->spanner_list()) {
            if (!spanner->is_hammer_on_pull_off()) {
                continue;
            }
            HammerOnPullOff *hopo = to_hammer_on_pull_off(spanner);
            const Staff *staff = hopo->staff();
            const StaffType *staff_type = staff ? staff->staff_type(hopo->tick()) : nullptr;
            if (!staff_type) {
                continue;
            }
            bool align_letters = staff_type->is_tab_staff() ? align_letters_tab : align_letters_std;
            if (!align_letters) {
                continue;
            }

            for (SpannerSegment *seg : hopo->spanner_segments()) {
                if (!seg->system()) {
                    continue;
                }

                std::vector<EngravingItem *> group;
                for (HammerOnPullOffText *text : to_hammer_on_pull_off_segment(seg)->hopo_text()) {
                    group.push_back(text);
                }
                if (group.empty()) {
                    continue;
                }

                bool above = row_item_is_above(group.front());
                bool items_same_placement = true;
                for (EngravingItem *item : group) {
                    if (row_item_is_above(item) != above) {
                        items_same_placement = false; // Don't try to align items split above/below
                    }
                }

                if (!items_same_placement) {
                    continue;
                }

                if (hopo->start_element() && hopo->start_element()->is_chord_rest()) {
                    ChordRest *scr = to_chord_rest(hopo->start_element());
                    if (scr->is_chord()) {
                        for (Articulation *art : to_chord(scr)->articulations()) {
                            if (art->is_tapping() && to_tapping(art)->text() && row_item_is_above(art) == above) {
                                group.push_back(art);
                            }
                        }
                    }
                }

                align_item_offset_group(group);
            }
        }
    }
}
} // namespace mu::engraving::compat
