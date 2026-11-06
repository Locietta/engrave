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

#include "dom/articulation.h"
#include "dom/capo.h"
#include "dom/chord.h"
#include "dom/dynamic.h"
#include "dom/excerpt.h"
#include "dom/expression.h"
#include "dom/factory.h"
#include "dom/harmony.h"
#include "dom/image.h"
#include "dom/instrchange.h"
#include "dom/key.h"
#include "dom/keylist.h"
#include "dom/laissezvib.h"
#include "dom/linkedobjects.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/noteline.h"
#include "dom/ornament.h"
#include "dom/part.h"
#include "dom/playtechannotation.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/staff.h"
#include "dom/staffstate.h"
#include "dom/stafftext.h"
#include "dom/stafftextbase.h"
#include "dom/stem.h"
#include "dom/tempotext.h"
#include "dom/textline.h"
#include "editing/editchord.h"
#include "editing/editkeysig.h"
#include "editing/transaction/transaction.h"
#include "editing/transpose.h"
#include "log.h"
#include "style/defaultstyle.h"
#include "style/styledef.h"
#include "style/textstyle.h"
#include "types/string.h"

using namespace mu::engraving;
using namespace mu::engraving::compat;

const std::set<SymId> CompatUtils::ornament_ids{SymId::ORNAMENT_TURN,
                                                SymId::ORNAMENT_TURN_INVERTED,
                                                SymId::ORNAMENT_TURN_SLASH,
                                                SymId::ORNAMENT_TRILL,
                                                SymId::BRASS_MUTE_CLOSED,
                                                SymId::ORNAMENT_MORDENT,
                                                SymId::ORNAMENT_SHORT_TRILL,
                                                SymId::ORNAMENT_TREMBLEMENT,
                                                SymId::ORNAMENT_PRALL_MORDENT,
                                                SymId::ORNAMENT_LINE_PRALL,
                                                SymId::ORNAMENT_UP_PRALL,
                                                SymId::ORNAMENT_UP_MORDENT,
                                                SymId::ORNAMENT_PRECOMP_MORDENT_UPPER_PREFIX,
                                                SymId::ORNAMENT_DOWN_MORDENT,
                                                SymId::ORNAMENT_PRALL_UP,
                                                SymId::ORNAMENT_PRALL_DOWN,
                                                SymId::ORNAMENT_PRECOMP_SLIDE,
                                                SymId::ORNAMENT_SHAKE3,
                                                SymId::ORNAMENT_SHAKE_MUFFAT1,
                                                SymId::ORNAMENT_TREMBLEMENT_COUPERIN,
                                                SymId::ORNAMENT_PINCE_COUPERIN};

const std::map<Sid, Sid> CompatUtils::align_vals_to_convert{
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
    TRACEFUNC;

    if (!master_score) {
        return;
    }

    // TODO: collect all compatibility conversions here
    if (master_score->msc_version() < 400) {
        replace_staff_text_with_play_technique_annotation(master_score);
    }
    if (master_score->msc_version() < 410) {
        reconstruct_type_of_custom_dynamics(master_score);
        replace_old_with_new_expressions(master_score);
        replace_old_with_new_ornaments(master_score);
        reset_rest_vertical_offset(master_score);
        split_articulations(master_score);
        reset_articulation_offsets(master_score);
        reset_stem_lengths_for_two_note_trems(master_score);
        replace_staff_text_with_capo(master_score);
    }
    if (master_score->msc_version() < 420) {
        add_missing_init_key_for_transposing_instrument(master_score);
        reset_frames_exclusion_from_parts(master_score);
    }
    if (master_score->msc_version() < 440) {
        map_header_footer_styles(master_score);
    }

    if (master_score->msc_version() < 450) {
        convert_text_line_to_note_anchored_line(master_score);
        convert_laissez_vib_artic_to_tie(master_score);
    }

    if (master_score->msc_version() < 500) {
        remove_mm_rest_elements(master_score);
    }
}

void CompatUtils::replace_staff_text_with_play_technique_annotation(MasterScore *score) {
    TRACEFUNC;

    //! NOTE: Before MU4, they were available in the Staff Text Properties dialog (Change Channel page)
    static const std::unordered_map<muse::String, PlayingTechniqueType> text_to_play_technique_type{
        {u"natural", PlayingTechniqueType::NATURAL},
        {u"normal", PlayingTechniqueType::NATURAL},

        {u"open", PlayingTechniqueType::OPEN},
        {u"mute", PlayingTechniqueType::MUTE},

        {u"distortion", PlayingTechniqueType::DISTORTION},
        {u"overdriven", PlayingTechniqueType::OVERDRIVE},
        {u"harmonics", PlayingTechniqueType::HARMONICS},
        {u"jazz", PlayingTechniqueType::JAZZ_TONE},

        {u"arco", PlayingTechniqueType::NATURAL},
        {u"pizzicato", PlayingTechniqueType::PIZZICATO},
        {u"tremolo", PlayingTechniqueType::TREMOLO},
    };

    std::map<StaffTextBase *, PlayingTechniqueType> old_play_techniques_and_new_types;

    for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (Segment *segment = measure->first(); segment; segment = segment->next()) {
            std::vector<EngravingItem *> annotations = segment->annotations();

            for (EngravingItem *annotation : annotations) {
                if (!annotation || !annotation->is_staff_text_base()) {
                    continue;
                }

                StaffTextBase *text = to_staff_text_base(annotation);
                PlayingTechniqueType type =
                    muse::value(text_to_play_technique_type, text->plain_text().to_lower(), PlayingTechniqueType::UNDEFINED);

                if (type == PlayingTechniqueType::UNDEFINED) {
                    muse::String channel_name = text->channel_name(0).to_lower();

                    if (!channel_name.is_empty()) {
                        type = muse::value(text_to_play_technique_type, channel_name, PlayingTechniqueType::UNDEFINED);
                    }
                }

                if (type == PlayingTechniqueType::UNDEFINED) {
                    continue;
                }

                old_play_techniques_and_new_types.emplace(text, type);
                LinkedObjects *links = text->links();
                if (!links || links->empty()) {
                    continue;
                }
                for (EngravingObject *linked : *links) {
                    if (linked != text) {
                        old_play_techniques_and_new_types.emplace(to_staff_text_base(linked), type);
                    }
                }
            }
        }
    }

    for (auto &pair : old_play_techniques_and_new_types) {
        StaffTextBase *old_play_tech = pair.first;
        PlayingTechniqueType type = pair.second;
        Segment *parent_segment = old_play_tech->segment();

        PlayTechAnnotation *new_play_tech = Factory::create_play_tech_annotation(parent_segment, type, old_play_tech->text_style_type());
        new_play_tech->set_xml_text(old_play_tech->xml_text());
        new_play_tech->set_track(old_play_tech->track());

        LinkedObjects *links = old_play_tech->links();
        new_play_tech->set_links(links);
        if (links) {
            links->push_back(new_play_tech);
        }
        parent_segment->add(new_play_tech);
        parent_segment->remove_annotation(old_play_tech);

        delete old_play_tech;
    }
}

void CompatUtils::assign_initial_part_to_excerpts(const std::vector<Excerpt *> &excerpts) {
    TRACEFUNC;

    std::set<ID> assigned_part_id_set;

    auto assign_initial_part_id = [&assigned_part_id_set](Excerpt *excerpt, const ID &initial_part_id) {
        excerpt->set_initial_part_id(initial_part_id);
        assigned_part_id_set.insert(initial_part_id);
    };

    for (Excerpt *excerpt : excerpts) {
        for (const Part *part : excerpt->excerpt_score()->parts()) {
            if (excerpt->name() == part->part_name()) {
                assign_initial_part_id(excerpt, part->id());
                break;
            }
        }
    }

    for (Excerpt *excerpt : excerpts) {
        if (excerpt->initial_part_id().is_valid()) {
            continue;
        }

        for (Part *part : excerpt->excerpt_score()->parts()) {
            if (muse::contains(assigned_part_id_set, part->id())) {
                continue;
            }

            if (excerpt->name().contains(part->part_name())) {
                assign_initial_part_id(excerpt, part->id());
                break;
            }
        }
    }
}

void CompatUtils::replace_old_with_new_ornaments(MasterScore *score) {
    std::set<Articulation *> old_ornaments; // ornaments used to be articulations

    for (Measure *meas = score->first_measure(); meas; meas = meas->next_measure()) {
        for (Segment &seg : meas->segments()) {
            if (!seg.is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *item : seg.elist()) {
                if (!item || !item->is_chord()) {
                    continue;
                }
                for (Articulation *articulation : to_chord(item)->articulations()) {
                    if (articulation->is_ornament()) {
                        continue;
                    }
                    if (::mu::engraving::compat::CompatUtils::ornament_ids.find(articulation->sym_id()) !=
                        ::mu::engraving::compat::CompatUtils::ornament_ids.end()) {
                        old_ornaments.insert(articulation);
                        LinkedObjects *links = articulation->links();
                        if (!links || links->empty()) {
                            continue;
                        }
                        for (EngravingObject *linked : *links) {
                            if (linked != articulation) {
                                old_ornaments.insert(to_articulation(linked));
                            }
                        }
                    }
                }
            }
        }
    }

    for (Articulation *old_ornament : old_ornaments) {
        Chord *parent_chord = to_chord(old_ornament->chord_rest());

        Ornament *new_ornament = Factory::create_ornament(parent_chord);
        new_ornament->set_track(old_ornament->track());
        new_ornament->set_sym_id(old_ornament->sym_id());
        new_ornament->set_pos(old_ornament->pos());
        new_ornament->set_ornament_style(old_ornament->ornament_style());
        new_ornament->set_direction(old_ornament->direction());
        new_ornament->set_autoplace(old_ornament->autoplace());
        new_ornament->set_play_articulation(old_ornament->play_articulation());

        LinkedObjects *links = old_ornament->links();
        new_ornament->set_links(links);
        if (links) {
            links->push_back(new_ornament);
        }
        parent_chord->add(new_ornament);
        parent_chord->remove(old_ornament);
        delete old_ornament;
    }
}

void CompatUtils::replace_old_with_new_expressions(MasterScore *score) {
    std::set<StaffText *> old_expressions; // Expressions used to be staff text

    for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (Segment &seg : measure->segments()) {
            for (EngravingItem *item : seg.annotations()) {
                if (item && item->is_staff_text() && to_staff_text(item)->text_style_type() == TextStyleType::EXPRESSION) {
                    old_expressions.insert(to_staff_text(item));
                    LinkedObjects *links = item->links();
                    if (!links || links->empty()) {
                        continue;
                    }
                    for (EngravingObject *linked_item : *links) {
                        if (linked_item != item) {
                            old_expressions.insert(to_staff_text(linked_item));
                        }
                    }
                }
            }
        }
    }

    for (StaffText *old_expression : old_expressions) {
        Segment *parent_segment = old_expression->segment();

        Expression *new_expression = Factory::create_expression(parent_segment);
        new_expression->set_track(old_expression->track());
        new_expression->set_xml_text(old_expression->xml_text());
        new_expression->map_properties_from_old_expressions(old_expression);

        LinkedObjects *links = old_expression->links();
        new_expression->set_links(links);
        if (links) {
            links->push_back(new_expression);
        }

        parent_segment->add(new_expression);
        parent_segment->remove_annotation(old_expression);
        delete old_expression;
    }
}

void CompatUtils::reconstruct_type_of_custom_dynamics(MasterScore *score) {
    // Before version 4.1, Dynamics containing custom text were saved as type "other"
    // We check the string to see if we can reconstruct their true type.
    std::set<Dynamic *> other_type_dynamic;

    for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (Segment &seg : measure->segments()) {
            for (EngravingItem *item : seg.annotations()) {
                if (item && item->is_dynamic() && to_dynamic(item)->dynamic_type() == DynamicType::OTHER) {
                    other_type_dynamic.insert(to_dynamic(item));
                    LinkedObjects *links = item->links();
                    if (!links || links->empty()) {
                        continue;
                    }
                    for (EngravingObject *linked_item : *links) {
                        if (linked_item != item) {
                            other_type_dynamic.insert(to_dynamic(linked_item));
                        }
                    }
                }
            }
        }
    }

    for (Dynamic *dynamic : other_type_dynamic) {
        DynamicType dyn_type = reconstruct_dynamic_type_from_string(dynamic);
        dynamic->set_dynamic_type(dyn_type);
    }
}

void CompatUtils::split_articulations(MasterScore *master_score) {
    std::set<Articulation *> to_remove;
    for (Measure *meas = master_score->first_measure(); meas; meas = meas->next_measure()) {
        for (Segment &seg : meas->segments()) {
            if (!seg.is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *item : seg.elist()) {
                if (!item || !item->is_chord()) {
                    continue;
                }
                Chord *chord = to_chord(item);
                for (Articulation *a : chord->articulations()) {
                    if (a->is_linked()) {
                        continue; // only worry about main artics, links will be done later
                    }
                    std::set<SymId> ids = mu::engraving::split_articulations({a->sym_id()});
                    if (ids.size() <= 1) {
                        continue;
                    }
                    to_remove.insert(a);
                }
            }
        }
    }
    // separate into individual articulations
    for (Articulation *combined_artic : to_remove) {
        auto components = mu::engraving::split_articulations({combined_artic->sym_id()});
        Chord *parent_chord = to_chord(combined_artic->chord_rest());
        for (SymId id : components) {
            Articulation *new_artic = Factory::create_articulation(parent_chord);
            new_artic->set_sym_id(id);
            if (parent_chord->has_articulation(new_artic)) {
                delete new_artic;
                continue;
            }
            new_artic->set_track(combined_artic->track());
            new_artic->set_pos(combined_artic->pos());
            new_artic->set_direction(combined_artic->direction());
            new_artic->set_anchor(combined_artic->anchor());
            new_artic->set_color(combined_artic->color());
            new_artic->set_play_articulation(combined_artic->play_articulation());
            new_artic->set_visible(combined_artic->visible());
            new_artic->set_ornament_style(combined_artic->ornament_style());
            LinkedObjects *links = new LinkedObjects();
            links->push_back(new_artic);
            new_artic->set_links(links);
            parent_chord->add(new_artic);

            // newArtic is the main articulation
            LinkedObjects *old_links = combined_artic->links();
            if (!old_links || old_links->empty()) {
                continue;
            }
            for (EngravingObject *linked_item : *old_links) {
                IF_ASSERT_FAILED(linked_item && linked_item->is_articulation()) { continue; }
                if (linked_item == combined_artic) {
                    continue;
                }
                Articulation *old_artic = to_articulation(linked_item);
                Chord *old_parent = to_chord(old_artic->chord_rest());
                old_parent->add(new_artic->linked_clone());
            }
        }
    }
    // finally, remove the combined articulations
    for (Articulation *combined_artic : to_remove) {
        LinkedObjects *links = combined_artic->links();
        if (!links || links->empty()) {
            Chord *parent_chord = to_chord(combined_artic->chord_rest());
            parent_chord->remove(combined_artic);
            delete combined_artic;
            continue;
        }
        std::set<Articulation *> remove_links;
        for (auto linked : *links) {
            IF_ASSERT_FAILED(linked && linked->is_articulation()) { continue; }
            remove_links.insert(to_articulation(linked));
        }
        for (Articulation *linked_artic : remove_links) {
            if (linked_artic != combined_artic) {
                Chord *linked_parent = to_chord(linked_artic->chord_rest());
                linked_parent->remove(linked_artic);
                delete linked_artic;
            }
        }
        Chord *parent_chord = to_chord(combined_artic->chord_rest());
        parent_chord->remove(combined_artic);
        delete combined_artic;
    }
}

DynamicType CompatUtils::reconstruct_dynamic_type_from_string(Dynamic *dynamic) {
    static std::vector<Dyn> sorted_dyn_list; // copy of dynList sorted by string length

    if (sorted_dyn_list.empty()) {
        sorted_dyn_list = Dynamic::dynamic_list();
        std::sort(sorted_dyn_list.begin(), sorted_dyn_list.end(), [](const Dyn &a, const Dyn &b) {
            String string_a = String::from_utf8(a.text);
            String string_b = String::from_utf8(b.text);
            return string_a.size() > string_b.size();
        });
    }

    for (Dyn dyn : sorted_dyn_list) {
        String dyn_text = String::from_utf8(dyn.text);
        if (dyn_text.size() == 0) {
            return DynamicType::OTHER;
        }
        if (dynamic->xml_text().contains(dyn_text)) {
            return dyn.type;
        }
    }
    return DynamicType::OTHER;
}

ArticulationAnchor CompatUtils::translate_to_new_articulation_anchor(int anchor) {
    switch (anchor) {
        case 0: // ArticulationAnchor::TOP_STAFF
        case 3: // ArticulationAnchor::TOP_CHORD
            return ArticulationAnchor::TOP;
            break;
        case 1: // ArticulationAnchor::BOTTOM_STAFF
        case 4: // ArticulationAnchor::BOTTOM_CHORD
            return ArticulationAnchor::BOTTOM;
            break;
        case 2: // ArticulationAnchor::CHORD
        default: return ArticulationAnchor::AUTO; break;
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

void CompatUtils::reset_rest_vertical_offset(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
            for (Segment &segment : measure->segments()) {
                if (!segment.is_chord_rest_type()) {
                    continue;
                }
                for (EngravingItem *item : segment.elist()) {
                    if (!item || !item->is_rest()) {
                        continue;
                    }
                    Rest *rest = to_rest(item);
                    if (rest->offset().y() != 0) {
                        PointF new_offset = PointF(rest->offset().x(), 0.0);
                        rest->set_property(Pid::OFFSET, new_offset);
                    }
                }
            }
        }
    }
}

void CompatUtils::reset_articulation_offsets(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
            for (Segment &segment : measure->segments()) {
                if (!segment.is_chord_rest_type()) {
                    continue;
                }
                for (EngravingItem *item : segment.elist()) {
                    if (!item || !item->is_chord()) {
                        continue;
                    }
                    Chord *chord = to_chord(item);
                    for (Articulation *artic : chord->articulations()) {
                        if (!artic) {
                            continue;
                        }
                        artic->set_property(Pid::OFFSET, PointF());
                    }
                }
            }
        }
    }
}

void CompatUtils::reset_stem_lengths_for_two_note_trems(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
            for (Segment &segment : measure->segments()) {
                if (!segment.is_chord_rest_type()) {
                    continue;
                }
                for (EngravingItem *item : segment.elist()) {
                    if (!item || !item->is_chord()) {
                        continue;
                    }
                    Chord *chord = to_chord(item);
                    TremoloTwoChord *trem = chord->tremolo_two_chord();
                    Stem *stem = chord->stem();
                    if (stem && trem) {
                        if (!stem->user_length().is_zero()) {
                            stem->set_user_length(0_sp);
                        }
                    }
                }
            }
        }
    }
}

void CompatUtils::replace_staff_text_with_capo(MasterScore *score) {
    TRACEFUNC;

    std::set<StaffTextBase *> old_capo_set;

    for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (Segment *segment = measure->first(); segment; segment = segment->next()) {
            for (EngravingItem *annotation : segment->annotations()) {
                if (!annotation || !annotation->is_staff_text_base()) {
                    continue;
                }

                StaffTextBase *text = to_staff_text_base(annotation);

                if (text->capo() > 0) {
                    old_capo_set.insert(text);
                } else {
                    continue;
                }

                LinkedObjects *links = text->links();
                if (!links || links->empty()) {
                    continue;
                }

                for (EngravingObject *linked : *links) {
                    if (linked != text && linked && linked->is_staff_text_base()) {
                        old_capo_set.insert(to_staff_text_base(linked));
                    }
                }
            }
        }
    }

    for (StaffTextBase *old_capo : old_capo_set) {
        Segment *parent_segment = old_capo->segment();
        Capo *new_capo = Factory::create_capo(parent_segment);

        int capo_fret_position = old_capo->capo() - 1;

        CapoParams params;
        params.active = capo_fret_position > 0;
        params.fret_position = capo_fret_position;

        new_capo->set_track(old_capo->track());
        new_capo->set_params(params);
        new_capo->set_property(Pid::PLACEMENT, old_capo->placement());

        LinkedObjects *links = old_capo->links();
        new_capo->set_links(links);
        if (links) {
            links->push_back(new_capo);
        }

        parent_segment->add(new_capo);
        parent_segment->remove_annotation(old_capo);

        delete old_capo;
    }
}

void CompatUtils::add_missing_init_key_for_transposing_instrument(MasterScore *score) {
    TRACEFUNC;

    for (Part *part : score->parts()) {
        Instrument *instrument = part->instrument();
        Interval v = instrument->transpose();
        if (v.chromatic % 12) {
            for (Staff *staff : part->staves()) {
                KeyList *keys = staff->key_list();
                if (keys->find(0) == keys->end()) {
                    KeySigEvent kse;
                    Key key = Key::C;
                    Key c_key = key;
                    if (!score->style().style_b(Sid::CONCERT_PITCH)) {
                        c_key = Transpose::transpose_key(key, v);
                    }
                    kse.set_concert_key(c_key);
                    kse.set_key(key);
                    EditKeySig::undo_change_key_sig(score->transaction_manager()->current_or_dummy_transaction(), score, staff,
                                                    Fraction(0, 1), kse);
                }
            }
        }
    }
}

void CompatUtils::reset_frames_exclusion_from_parts(MasterScore *master_score) {
    for (Score *score : master_score->score_list()) {
        for (MeasureBase *measure_base = score->first(); measure_base; measure_base = measure_base->next()) {
            if (!measure_base->is_measure()) {
                measure_base->set_exclude_from_other_parts(false);
            }
        }
    }
}

void CompatUtils::map_header_footer_styles(MasterScore *score) {
    // Copyright and page numbers used header/footer styling before 4.4 - after 4.4 these have their own styles. To ensure nothing
    // changes visually when loading a pre-4.4 score for the first time, we must search the header/footer strings for copyright/page
    // number macros and set the "defaults" for copyright/page number styles based on where the macros were inserted...
    const auto do_map = [score](const TextStyleType type, const std::vector<Sid> &header_footer_string_sids) {
        const TextStyle *header_footer_text_style = text_style(type);
        const TextStyle *copyright_text_style = text_style(TextStyleType::COPYRIGHT);
        const TextStyle *page_number_text_style = text_style(TextStyleType::PAGE_NUMBER);

        //! NOTE: Keep in sync with Page::replaceTextMacros
        const std::wregex copyright_search(LR"(\$[cC])");
        const std::wregex page_number_search(LR"(\$[pPnN])");

        bool have_mapped_copyright = false;
        bool have_mapped_page_number = false;
        for (const Sid sid : header_footer_string_sids) {
            const String s = score->style().style_st(sid);
            if (!have_mapped_copyright && s.contains(copyright_search)) {
                for (size_t i = 0; i < ::mu::engraving::text_style_size; ++i) {
                    const Sid header_footer_sid = header_footer_text_style->at(i).sid;
                    const PropertyValue pv = score->style().style_v(header_footer_sid);
                    const Sid copyright_sid = copyright_text_style->at(i).sid;
                    score->style().set(copyright_sid, pv);
                }
                have_mapped_copyright = true;
            }
            if (!have_mapped_page_number && s.contains(page_number_search)) {
                for (size_t i = 0; i < ::mu::engraving::text_style_size; ++i) {
                    const Sid header_footer_sid = header_footer_text_style->at(i).sid;
                    const Sid page_number_sid = page_number_text_style->at(i).sid;
                    const PropertyValue pv = score->style().style_v(header_footer_sid);
                    score->style().set(page_number_sid, pv);
                }
                have_mapped_page_number = true;
            }
        }
    };

    do_map(TextStyleType::HEADER,
           {Sid::ODD_HEADER_L, Sid::ODD_HEADER_C, Sid::ODD_HEADER_R, Sid::EVEN_HEADER_L, Sid::EVEN_HEADER_C, Sid::EVEN_HEADER_R});
    do_map(TextStyleType::FOOTER,
           {Sid::ODD_FOOTER_L, Sid::ODD_FOOTER_C, Sid::ODD_FOOTER_R, Sid::EVEN_FOOTER_L, Sid::EVEN_FOOTER_C, Sid::EVEN_FOOTER_R});
}

NoteLine *CompatUtils::create_note_line_from_text_line(TextLine *text_line) {
    IF_ASSERT_FAILED((text_line->start_element() && text_line->start_element()->is_note()) &&
                     (text_line->end_element() && text_line->end_element()->is_note())) {
        LOGW("Skipping textline with non note endpoints");
        return nullptr;
    }
    Note *start_note = to_note(text_line->start_element());
    Note *end_note = to_note(text_line->end_element());

    NoteLine *note_line = Factory::create_note_line(start_note);
    note_line->set_start_element(start_note);
    note_line->set_track(text_line->track());
    note_line->set_tick(text_line->tick());
    note_line->set_end_element(end_note);
    note_line->set_tick2(text_line->tick2());
    note_line->set_visible(text_line->visible());

    // Preserve old layout style
    note_line->set_line_end_placement(NoteLineEndPlacement::LEFT_EDGE);

    for (Pid pid : text_line->text_line_base_property_ids()) {
        note_line->set_property(pid, text_line->get_property(pid));
    }

    for (const SpannerSegment *old_seg : text_line->spanner_segments()) {
        LineSegment *new_seg = note_line->create_line_segment();
        new_seg->set_offset(old_seg->offset());
        new_seg->set_user_off2(old_seg->user_off2());

        note_line->add(new_seg);
    }

    LinkedObjects *links = text_line->links();
    note_line->set_links(links);
    if (links) {
        links->push_back(note_line);
    }

    return note_line;
}

void CompatUtils::convert_text_line_to_note_anchored_line(MasterScore *master_score) {
    std::set<TextLine *> old_lines; // NoteLines used to be TextLines

    for (Measure *measure = master_score->first_measure(); measure; measure = measure->next_measure()) {
        for (Segment &segment : measure->segments()) {
            if (!segment.is_chord_rest_type()) {
                continue;
            }
            for (track_idx_t track = 0; track <= master_score->ntracks(); track++) {
                EngravingItem *el = segment.element(track);
                if (!el || !el->is_chord()) {
                    continue;
                }
                Chord *chord = to_chord(el);
                for (Note *note : chord->notes()) {
                    for (Spanner *spanner : note->spanner_for()) {
                        if (!spanner->is_text_line()) {
                            continue;
                        }

                        old_lines.insert(to_text_line(spanner));

                        LinkedObjects *links = spanner->links();
                        if (!links || links->empty()) {
                            continue;
                        }

                        for (EngravingObject *linked : *links) {
                            if (linked != spanner && linked && linked->is_text_line()) {
                                old_lines.insert(to_text_line(linked));
                            }
                        }
                    }
                }
            }
        }
    }

    for (TextLine *old_line : old_lines) {
        NoteLine *new_line = create_note_line_from_text_line(old_line);
        if (!new_line) {
            old_line->ownership_parent_item()->remove(old_line);
            delete old_line;
            continue;
        }
        EngravingItem *parent = new_line->ownership_parent_item();

        parent->remove(old_line);
        parent->add(new_line);

        delete old_line;
    }
}

void CompatUtils::convert_laissez_vib_artic_to_tie(MasterScore *master_score) {
    std::set<Articulation *> old_artics; // NoteLines used to be TextLines

    for (Measure *meas = master_score->first_measure(); meas; meas = meas->next_measure()) {
        for (Segment &seg : meas->segments()) {
            if (!seg.is_chord_rest_type()) {
                continue;
            }
            for (EngravingItem *item : seg.elist()) {
                if (!item || !item->is_chord()) {
                    continue;
                }
                Chord *chord = to_chord(item);
                for (Articulation *a : chord->articulations()) {
                    if (!a->is_laissez_vib()) {
                        continue;
                    }
                    old_artics.insert(a);

                    LinkedObjects *links = a->links();
                    if (!links || links->empty()) {
                        continue;
                    }

                    for (EngravingObject *linked : *links) {
                        if (linked != a && linked && linked->is_laissez_vib()) {
                            old_artics.insert(to_articulation(linked));
                        }
                    }
                }
            }
        }
    }

    for (Articulation *old_artic : old_artics) {
        Chord *parent_chord = to_chord(old_artic->chord_rest());
        Note *parent_note = old_artic->up() ? parent_chord->up_note() : parent_chord->down_note();

        parent_chord->remove(old_artic);

        LaissezVib *lv = Factory::create_laissez_vib(parent_note);
        parent_note->add(lv);

        delete old_artic;
    }
}

void CompatUtils::set_harmony_root_tpc_from_function(HarmonyInfo *info, const Harmony *h, const muse::String &s) {
    Key key = Key::INVALID;
    const Staff *st = h->staff();
    key = st ? st->key(h->tick()) : Key::INVALID;
    info->set_root_tpc(function2_tpc(s, key));
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

void CompatUtils::reset_hook_height_sign(TextLineBase *tl) {
    if (!tl->place_below()) {
        return;
    }

    if (!tl->is_styled(Pid::BEGIN_HOOK_HEIGHT)) {
        Spatium begin_hook_height = tl->get_property(Pid::BEGIN_HOOK_HEIGHT).value<Spatium>();
        tl->set_property(Pid::BEGIN_HOOK_HEIGHT, -begin_hook_height);
    }
    if (!tl->is_styled(Pid::END_HOOK_HEIGHT)) {
        Spatium end_hook_height = tl->get_property(Pid::END_HOOK_HEIGHT).value<Spatium>();
        tl->set_property(Pid::END_HOOK_HEIGHT, -end_hook_height);
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

    // We need additional context for items with voice assignment properties
    // Migrate in EngravingCompat after layout
    if (item->has_voice_assignment_properties()) {
        return;
    }

    offset = get_adjusted_offset(item, offset.value<PointF>());
}

void CompatUtils::migrate_offset_pre302(EngravingItem *item, int msc_version) {
    if (msc_version > 301 || item->offset().is_null()) {
        return;
    }

    PropertyValue offset = item->get_property(Pid::OFFSET);
    CompatUtils::migrate_offset500(item, offset);
    item->set_property(Pid::OFFSET, offset);
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
                    annotation->part()->set_instrument(instr, seg.tick());
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
