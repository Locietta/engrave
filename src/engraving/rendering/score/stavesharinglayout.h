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
#pragma once

#include "dom/sharedpart.h"
#include "dom/stavesharinglabel.h"
#include "layoutcontext.h"

namespace mu::engraving::rendering::score {
struct StaveSharingLayout {
    static void update_stave_sharing_for_full_system(MeasureBase *first_mb, MeasureBase *last_mb, LayoutContext &ctx);
    static bool update_stave_sharing_for_last_added_measure(System *system, LayoutContext &ctx);
    static void update_notation_without_recomputing_track_map(Measure *measure, LayoutContext &ctx);

private:
    using TrackGroup = std::vector<track_idx_t>;

    struct StaveSharingContext {
        bool update_for_last_added = true;
        bool track_map_changed = false;
        Fraction s_tick = Fraction(0, 1);
        Fraction e_tick = Fraction(0, 1);
        std::vector<Segment *> all_segments;
        std::vector<Segment *> cr_segments;
        std::vector<Segment *> segments_to_update;
        std::vector<Segment *> cr_segments_to_update;
        std::vector<Spanner *> overlapping_spanners;

        SharedPart *cur_shared_part = nullptr;
        SharedTrackMap cur_track_map;
        std::unordered_set<Note *> shared_unison_notes;
        std::vector<StaveSharingLabel *> old_stave_sharing_labels;
        std::vector<StaveSharingLabel *> updated_stave_sharing_labels;

        Score *score = nullptr;
        LayoutContext &layout_ctx;
        const MStyle &style;

        StaveSharingContext(MeasureBase *first, MeasureBase *last, LayoutContext &ctx);
    };

    static void update_stave_sharing(StaveSharingContext &ctx);

    static void update_track_maps(StaveSharingContext &ctx);
    static SharedTrackMap compute_track_map(StaveSharingContext &ctx);

    static bool is_empty(track_idx_t track, StaveSharingContext &ctx);

    static bool same_instrument(track_idx_t prev_track, track_idx_t next_track, const Fraction &tick, StaveSharingContext &ctx);

    static bool is_unison(track_idx_t prev_track, track_idx_t next_track, StaveSharingContext &ctx);
    static bool can_go_to_same_voice(track_idx_t prev_track, track_idx_t next_track, StaveSharingContext &ctx,
                                     const TrackGroup &cur_track_group, std::unordered_set<Note *> &local_unison_notes);

    static bool check_segment_for_same_voice(Segment *segment, track_idx_t prev_track, track_idx_t next_track);
    static bool check_clef_key_time_sig_for_same_voice(Segment *segment, track_idx_t prev_track, track_idx_t next_track);
    static bool seg_has_local_key_sig(const Segment *segment, const SharedPart *shared_part);
    static bool check_segment_annotations_for_same_voice(Segment *segment, track_idx_t prev_track, track_idx_t next_track);
    static bool check_items_for_same_voice(Segment *segment, track_idx_t prev_track, track_idx_t next_track,
                                           const std::function<bool(const EngravingItem *, const EngravingItem *)> &is_equal);
    static bool check_note_spanners_for_unison(const Note *note1, const Note *note2);
    static bool check_spanners_for_same_voice(track_idx_t prev_track, track_idx_t next_track, StaveSharingContext &ctx);
    static bool check_articulations_for_same_voice(Chord *chord1, Chord *chord2);

    static bool can_go_to_same_stave(track_idx_t prev_track, track_idx_t next_track, StaveSharingContext &ctx);

    static void update_notation(StaveSharingContext &ctx);
    static void compute_segments_to_update(StaveSharingContext &ctx);
    static void disconnect_all(StaveSharingContext &ctx);

    static EngravingItem *make_shared_item(EngravingItem *origin_item, EngravingItem *possible_shared_item, track_idx_t shared_track,
                                           EngravingObject *shared_parent,
                                           const std::function<bool(const EngravingItem *, const EngravingItem *)> &is_equal);

    static void make_shared_notation(StaveSharingContext &ctx);
    static void make_shared_clef_key_time_sigs(StaveSharingContext &ctx);
    static void make_shared_chord_rests(StaveSharingContext &ctx);
    static void make_shared_breaths(StaveSharingContext &ctx);
    static void make_shared_articulations(Chord *origin_chord, Chord *shared_chord);
    static void make_shared_ties_and_note_spanners(Note *origin_note, Note *shared_note);
    static void make_shared_annotations(StaveSharingContext &ctx);
    static void make_shared_spanners(StaveSharingContext &ctx);

    static void make_stave_sharing_labels(StaveSharingContext &ctx);
    static bool unison_note_needs_label(Note *unison_note, bool &is_for_new_system, StaveSharingContext &ctx);
    static String format_unison_label(Note *unison_note, const SharedTrackMap &track_map, bool is_for_new_system,
                                      const StaveSharingContext &ctx);
    static String format_instrument_change_lable(const InstrumentChange *origin_change, const InstrumentChange *shared_change,
                                                 const StaveSharingContext &ctx);

    static void manage_voice_property_and_track_for_shared_items(const std::vector<EngravingItem *> &shared_items,
                                                                 track_idx_t start_origin_track, track_idx_t end_origin_track,
                                                                 const SharedTrackMap &track_map);

    static void cleanup(StaveSharingContext &ctx);
    static void add_measure_rests_if_need(StaveSharingContext &ctx);
};
} // namespace mu::engraving::rendering::score
