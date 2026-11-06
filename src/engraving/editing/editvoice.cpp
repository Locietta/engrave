/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "editvoice.h"

#include <vector>

#include "../dom/articulation.h"
#include "../dom/chord.h"
#include "../dom/excerpt.h"
#include "../dom/factory.h"
#include "../dom/lyrics.h"
#include "../dom/masterscore.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/partialtie.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"
#include "../dom/slur.h"
#include "../dom/spanner.h"
#include "../dom/staff.h"
#include "../dom/tie.h"
#include "../dom/tuplet.h"
#include "editchord.h"
#include "global/containers.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   changeSelectedElementsVoice
//    moves selected notes into specified voice if possible
//---------------------------------------------------------

void EditVoice::change_selected_elements_voice(Transaction &, Score *score, voice_idx_t voice) {
    auto is_operation_valid = [](Score *score, track_idx_t orig_track, track_idx_t dst_track) {
        Excerpt *excerpt = score->excerpt();
        if (!excerpt) {
            return true;
        }

        const track_idx_t dst_track_mapped = muse::key(excerpt->tracks_mapping(), dst_track, muse::nidx);
        if (dst_track_mapped == muse::nidx) {
            return false;
        }

        const track_idx_t orig_track_mapped = muse::key(excerpt->tracks_mapping(), orig_track, muse::nidx);
        if (orig_track_mapped == dst_track) {
            return false;
        }

        return true;
    };

    // Track groups of parentheses to recreate in new chord
    struct MovedParenGroup {
        Chord *dst_chord = nullptr;
        bool generated = false;
        bool left_visible = true;
        bool right_visible = true;
        PointF left_offset = PointF();
        PointF right_offset = PointF();
        std::vector<Note *> notes;
    };

    std::map<const NoteParenthesisInfo *, MovedParenGroup> moved_paren_groups;

    std::vector<EngravingItem *> new_elements;
    std::vector<EngravingItem *> oel = score->selection().elements(); // make copy
    for (EngravingItem *e : oel) {
        if (e->is_note()) {
            Note *note = to_note(e);
            Chord *chord = note->chord();

            // move grace notes with main chord only
            if (chord->is_grace() || chord->is_trill_cue_note()) {
                continue;
            }

            if (chord->voice() == voice) {
                continue;
            }

            Score *element_score = score;
            if (score->excerpt() && !chord->staff()->is_voice_visible(voice)) {
                // We are on a linked stave with the desired voice not visible
                // Get note in main score to continue
                note = to_note(note->find_linked_in_score(score->master_score()));
                chord = note->chord();
                element_score = score->master_score();
            }
            Segment *s = chord->segment();
            Measure *m = s->measure();
            size_t notes = chord->notes().size();
            track_idx_t dst_track = chord->staff_idx() * ::mu::engraving::voices + voice;
            ChordRest *dst_cr = to_chord_rest(s->element(dst_track));
            Chord *dst_chord = nullptr;

            if (!is_operation_valid(element_score, chord->track(), dst_track)) {
                continue;
            }

            // set up destination chord

            if (dst_cr && dst_cr->global_ticks() == chord->global_ticks()) {
                // existing chord/rest in destination with correct duration
                //   this case allows for tuplets, unlike the more general case below
                if (dst_cr->is_chord()) {
                    // can simply move note in
                    dst_chord = to_chord(dst_cr);
                } else {
                    // replace rest with chord, then move note in
                    dst_chord = Factory::create_chord(s);
                    dst_chord->set_track(dst_track);
                    dst_chord->set_duration_type(chord->duration_type());
                    dst_chord->set_ticks(chord->ticks());
                    dst_chord->set_tuplet(dst_cr->tuplet());
                    element_score->undo_remove_element(dst_cr);
                }
            } else if (!chord->tuplet()) {
                // rests or gap in destination
                //   insert new chord if the rests / gap are long enough
                //   then move note in
                bool has_incompatible_tuplet = false;
                const Chord *c_before = nullptr;
                const Chord *c_after_start = nullptr;
                for (const Segment *s2 = m->first(SegmentType::CHORD_REST); s2; s2 = s2->next(SegmentType::CHORD_REST)) {
                    const ChordRest *cr2 = to_chord_rest(s2->element(dst_track));
                    if (!cr2) {
                        continue;
                    }
                    if (const Tuplet *top_tuplet = cr2->top_tuplet()) {
                        if (top_tuplet->tick() < s->tick() && top_tuplet->end_tick() > s->tick()) {
                            has_incompatible_tuplet = true;
                            break;
                        }
                        if (top_tuplet->tick() < chord->end_tick() && top_tuplet->end_tick() > chord->end_tick()) {
                            has_incompatible_tuplet = true;
                            break;
                        }
                    }
                    if (!cr2->is_chord()) {
                        continue;
                    }
                    if (s2->tick() < s->tick()) {
                        c_before = to_chord(cr2);
                    }
                    if (s2->tick() >= s->tick()) {
                        c_after_start = to_chord(cr2);
                    }
                    if (s2->tick() >= chord->end_tick()) {
                        break;
                    }
                }
                if (has_incompatible_tuplet) {
                    continue;
                }
                if (c_before && c_before->end_tick() > s->tick()) {
                    // previous chord overlaps
                    continue;
                }
                if (c_after_start && c_after_start->tick() < chord->end_tick()) {
                    // next chord overlaps
                    continue;
                }
                // big enough gap found
                dst_chord = Factory::create_chord(s);
                dst_chord->set_track(dst_track);
                dst_chord->set_duration_type(chord->duration_type());
                dst_chord->set_ticks(chord->ticks());
                // makeGapVoice will not back-fill an empty voice
                if (voice && !dst_cr) {
                    element_score->expand_voice(s, /*m->first(SegmentType::ChordRest,*/ dst_track);
                }
                element_score->make_gap_voice(s, dst_track, chord->ticks(), s->tick());
            }

            if (!dst_chord) {
                continue;
            }

            // move note to destination chord

            // create & add new note
            Note *new_note = Factory::copy_note(*note);
            new_note->set_selected(false);
            new_note->set_ownership_parent(dst_chord);
            element_score->undo_add_element(new_note);
            new_elements.push_back(new_note);
            // add new chord if one was created
            if (dst_chord != dst_cr) {
                element_score->undo_add_cr(dst_chord, m, s->tick());
            }
            for (EngravingObject *linked : note->link_list()) {
                Note *linked_note = to_note(linked);
                Note *linked_new_note = linked == note ? new_note : to_note(new_note->find_linked_in_staff(linked_note->staff()));
                // reconnect the tie to this note, if any
                Tie *tie = linked_note->tie_back();
                if (tie) {
                    Note *start_note = tie->is_partial_tie() && !to_partial_tie(tie)->is_outgoing() ? nullptr : tie->start_note();
                    element_score->undo_change_spanner_elements(tie, start_note, linked_new_note);
                }
                // reconnect the tie from this note, if any
                tie = linked_note->tie_for();
                if (tie) {
                    element_score->undo_change_spanner_elements(tie, linked_new_note, tie->end_note());
                }

                // Reconnect note anchored spanners
                for (EngravingItem *item : linked_note->spanner_back()) {
                    if (!item || !item->is_spanner()) {
                        continue;
                    }
                    Spanner *spanner = to_spanner(item);
                    element_score->undo_change_spanner_elements(spanner, spanner->start_element(), linked_new_note);
                }

                for (EngravingItem *item : linked_note->spanner_for()) {
                    if (!item || !item->is_spanner()) {
                        continue;
                    }
                    Spanner *spanner = to_spanner(item);
                    element_score->undo_change_spanner_elements(spanner, linked_new_note, spanner->end_element());
                }
            }

            // Move paren to corresponding group in new chord
            if (!chord->note_parentheses().empty()) {
                const NoteParenthesisInfo *note_paren_info = chord->find_note_parenthesis_info(note);
                if (note_paren_info) {
                    Parenthesis *left_paren = note_paren_info->left_paren();
                    Parenthesis *right_paren = note_paren_info->right_paren();
                    DO_ASSERT(left_paren && right_paren);
                    if (left_paren && right_paren) {
                        bool generated = left_paren->generated();
                        MovedParenGroup &moved_group = moved_paren_groups[note_paren_info];
                        moved_group.dst_chord = dst_chord;
                        moved_group.generated = generated;
                        moved_group.notes.push_back(new_note);
                        moved_group.left_offset = left_paren->offset();
                        moved_group.right_offset = right_paren->offset();
                        moved_group.left_visible = left_paren->visible();
                        moved_group.right_visible = right_paren->visible();
                        EditChord::remove_chord_parentheses(chord, {note}, /*addToLinked*/ true, generated);
                    }
                }
            }

            // remove original note
            if (notes > 1) {
                element_score->undo_remove_element(note);
            } else if (notes == 1) {
                // take care of slurs
                int current_tick = chord->tick().ticks();
                for (auto it : element_score->spanner_map().find_overlapping(current_tick, current_tick + 1)) {
                    Spanner *spanner = it.value;
                    if (!spanner->is_slur()) {
                        continue;
                    }
                    Slur *slur = to_slur(spanner);
                    if (slur->start_element() == chord && slur->end_element() == chord) {
                        element_score->undo_change_spanner_elements(slur, dst_chord, dst_chord);
                        slur->undo_change_property(Pid::VOICE, voice);
                    } else if (slur->start_element() == chord) {
                        element_score->undo_change_spanner_elements(slur, dst_chord, slur->end_element());
                    } else if (slur->end_element() == chord) {
                        element_score->undo_change_spanner_elements(slur, slur->start_element(), dst_chord);
                    }
                }
                // move articulations
                for (Articulation *artic : chord->articulations()) {
                    if (dst_chord->has_articulation(artic)) {
                        continue;
                    }
                    element_score->undo_change_parent(artic, dst_chord, dst_chord->staff_idx());
                }
                // create rest to leave behind
                Rest *r = Factory::create_rest(s);
                r->set_track(chord->track());
                r->set_duration_type(chord->duration_type());
                r->set_ticks(chord->ticks());
                r->set_tuplet(chord->tuplet());
                // if there were grace notes, move them
                while (!chord->grace_notes().empty()) {
                    Chord *gc = chord->grace_notes().front();
                    Chord *ngc = Factory::copy_chord(*gc);
                    element_score->undo_remove_element(gc);
                    ngc->set_ownership_parent(dst_chord);
                    ngc->set_track(dst_chord->track());
                    element_score->undo_add_element(ngc);
                }
                // remove chord, replace with rest
                element_score->undo_remove_element(chord);
                element_score->undo_add_cr(r, m, s->tick());
            }

            // Move lyrics
            for (Lyrics *lyric : chord->lyrics()) {
                if (!lyric || dst_chord->lyrics(lyric->verse(), lyric->placement())) {
                    continue;
                }

                Lyrics *new_lyric = Factory::copy_lyrics(*lyric);
                new_lyric->set_ownership_parent(dst_chord);
                new_lyric->set_selected(false);
                new_lyric->set_track(dst_track);
                element_score->undo_add_element(new_lyric);
                new_elements.push_back(new_lyric);

                element_score->undo_remove_element(lyric);
            }
        } else if (e->has_voice_assignment_properties()) {
            if (e->is_spanner_segment()) {
                e = to_spanner_segment(e)->spanner();
            }

            track_idx_t dst_track = e->staff_idx() * ::mu::engraving::voices + voice;
            if (!is_operation_valid(score, e->track(), dst_track)) {
                continue;
            }

            e->undo_change_property(Pid::VOICE, voice);
            e->undo_change_property(Pid::VOICE_ASSIGNMENT, VoiceAssignment::CURRENT_VOICE_ONLY);
            new_elements.push_back(e);
        }
    }

    // Recreate parenthesis groups
    for (auto &pair : moved_paren_groups) {
        MovedParenGroup &moved_group = pair.second;
        IF_ASSERT_FAILED(!moved_group.notes.empty()) { continue; }
        EditChord::add_chord_parentheses(moved_group.dst_chord, moved_group.notes, /*addToLinked*/ true, moved_group.generated);

        const NoteParenthesisInfo *info = moved_group.notes.front()->parenthesis_info();
        Parenthesis *left_paren = info ? info->left_paren() : nullptr;
        Parenthesis *right_paren = info ? info->right_paren() : nullptr;

        IF_ASSERT_FAILED(info && left_paren && right_paren) { continue; }

        left_paren->undo_set_visible(moved_group.left_visible);
        right_paren->undo_set_visible(moved_group.right_visible);
        left_paren->undo_change_property(Pid::OFFSET, moved_group.left_offset);
        right_paren->undo_change_property(Pid::OFFSET, moved_group.right_offset);
    }

    if (!new_elements.empty()) {
        score->selection().clear();
        score->select(new_elements, SelectType::ADD, muse::nidx);
    }

    score->set_layout_all();
}

void EditVoice::change_selected_elements_voice_assignment(Transaction &, Score *score, VoiceAssignment voice_assignment) {
    std::vector<EngravingItem *> new_elements;

    for (EngravingItem *e : score->selection().elements()) {
        if (e->has_voice_assignment_properties()) {
            e->undo_change_property(Pid::VOICE_ASSIGNMENT, voice_assignment);
            new_elements.push_back(e);
        }
    }

    if (!new_elements.empty()) {
        score->selection().clear();
        score->select(new_elements, SelectType::ADD, muse::nidx);
    }
}
