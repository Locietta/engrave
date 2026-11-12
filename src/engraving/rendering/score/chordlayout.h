/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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
#ifndef MU_ENGRAVING_LAYOUTCHORDS_DEV_H
#define MU_ENGRAVING_LAYOUTCHORDS_DEV_H

#include <vector>

#include "dom/chord.h"
#include "dom/measurerepeat.h"
#include "dom/mmrest.h"
#include "dom/rest.h"
#include "layoutcontext.h"

namespace mu::engraving {
struct MStyle;
struct Measure;
struct Note;
struct Rest;
struct Score;
struct Segment;
struct Staff;
struct Slur;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct ChordPosInfo {
    std::vector<Chord *> chords;
    std::vector<Note *> up_stem_notes;
    std::vector<Note *> down_stem_notes;
    int up_voices = 0;
    int down_voices = 0;
    double max_up_width = 0.0;
    double max_down_width = 0.0;
    double max_up_mag = 0.0;
    double max_down_mag = 0.0;

    // dots and hooks can affect layout of notes as well as vice versa
    int up_dots = 0;
    int down_dots = 0;
    bool up_hooks = false;
    bool down_hooks = false;

    // also check for grace notes
    bool up_grace = false;
    bool down_grace = false;
};

struct OffsetInfo {
    double up_offset = 0.0;            // offset to apply to upstem chords
    double down_offset = 0.0;          // offset to apply to downstem chords
    double dot_adjust = 0.0;           // additional chord offset to account for dots
    double dot_adjust_threshold = 0.0; // if it exceeds this amount

    // centering adjustments for whole note, breve, and small chords
    double center_up = 0.0;          // offset to apply in order to center upstem chords
    double oversize_up = 0.0;        // adjustment to oversized upstem chord needed if laid out to the right
    double center_down = 0.0;        // offset to apply in order to center downstem chords
    double center_adjust_up = 0.0;   // adjustment to upstem chord needed after centering donwstem chord
    double center_adjust_down = 0.0; // adjustment to downstem chord needed after centering upstem chord

    std::set<track_idx_t> tracks_to_adjust;
};

struct ChordLayout {

    static void layout(Chord *item, LayoutContext &ctx);
    static void layout_note_lines(const Chord *chord);

    static void layout_spanners(Chord *item, LayoutContext &ctx);

    static void layout_articulations(Chord *item, LayoutContext &ctx);
    static void layout_articulations2(Chord *item, LayoutContext &ctx, bool layout_on_cross_beam_side = false);
    static void layout_articulations3(Chord *item, Slur *s, LayoutContext &ctx);

    static void layout_stem(Chord *item, const LayoutContext &ctx);

    static void compute_up(const Chord *item, ChordRest::LayoutData *ldata, const LayoutContext &ctx);
    static void compute_up(ChordRest *item, const LayoutContext &ctx);
    static int compute_auto_stem_direction(const std::vector<int> &note_distances);
    static bool is_chord_pos_below_beam(const Chord *item, Beam *beam);
    static bool is_chord_pos_below_trem(const Chord *item, TremoloTwoChord *trem);

    static void layout_chords1(LayoutContext &ctx, Segment *segment, staff_idx_t staff_idx);
    static double layout_chords2(std::vector<Note *> &notes, bool up, LayoutContext &ctx);
    static void layout_chords3(const std::vector<Chord *> &, const std::vector<Note *> &, const Staff *, LayoutContext &ctx);
    static void layout_ledger_lines(const std::vector<Chord *> &chords, LayoutContext &ctx);
    static void get_note_list_for_dots(Chord *c, std::vector<Note *> &, std::vector<Note *> &, std::vector<int> &);
    static void reposition_grace_notes_after(Segment *segment, size_t tracks);
    static void append_grace_notes(Chord *chord, LayoutContext &ctx);
    static Segment *grace_notes_after_segment(const Chord *chord);
    static void clear_line_attach_points(Measure *measure);
    static void update_line_attach_points(Chord *chord, bool is_first_in_measure, LayoutContext &ctx);

    static void layout_chord_base_fingering(Chord *chord, System *system, LayoutContext &ctx);

    static void cross_measure_setup(Chord *chord, bool on, LayoutContext &ctx);

    static void check_start_end_slurs(Chord *chord, LayoutContext &ctx);

    static void check_and_fill_shape(const ChordRest *item, ChordRest::LayoutData *ldata, const LayoutConfiguration &conf);
    static void fill_shape(const ChordRest *item, Chord::LayoutData *ldata, const LayoutConfiguration &conf);
    static void fill_shape(const Chord *item, Chord::LayoutData *ldata);
    static void fill_shape(const MeasureRepeat *item, MeasureRepeat::LayoutData *ldata, const LayoutConfiguration &conf);

    static void add_line_attach_points(Spanner *spanner);

    static bool chord_has_dots_all_invisible(Chord *chord);

    static double center_x(const Chord *chord);

private:
    friend struct RestLayout;

    static void layout_pitched(Chord *item, LayoutContext &ctx);
    static void layout_tablature(Chord *item, LayoutContext &ctx);

    static void layout_lv_articulation(Chord *item, LayoutContext &ctx);

    static void layout_note2(Note *note, LayoutContext &ctx);

    static void create_paren_groups(Chord *chord);

    static void place_dots(const std::vector<Chord *> &chords, const std::vector<Note *> &notes, LayoutContext &ctx);

    static void set_dot_relative_line(Note *note, int dot_move, LayoutContext &ctx);
    static void set_dot_x(const std::vector<Chord *> &chords, const std::array<double, 3 * ::mu::engraving::voices> &dot_pos,
                          const Staff *staff, const double up_dot_pos_x, const double down_dot_pos_x);

    static void skip_accidentals(Segment *segment, track_idx_t start_track, track_idx_t end_track);

    static Shape chord_rest_shape(const ChordRest *item);

    static bool leave_space_for_tie(const Articulation *item);

    static bool compute_up_beam_case(const Chord *item, Beam *beam);
    static bool compute_up_tremolo_case(const Chord *item, TremoloTwoChord *tremolo, const LayoutContext &ctx);

    static void update_ledger_lines(Chord *item, LayoutContext &ctx);

    static ChordPosInfo calculate_chord_pos_info(Segment *segment, staff_idx_t staff_idx, track_idx_t part_start_track,
                                                 track_idx_t part_end_track, LayoutContext &ctx);
    static void calculate_max_note_widths(ChordPosInfo &pos_info, const Fraction &tick, const Staff *staff, LayoutContext &ctx);
    static OffsetInfo centre_chords(const Segment *segment, ChordPosInfo &pos_info, staff_idx_t staff_idx, const Fraction &tick,
                                    LayoutContext &ctx);
    static void calculate_chord_offsets(Segment *segment, staff_idx_t staff_idx, const Fraction &tick, OffsetInfo &offset_info,
                                        ChordPosInfo &pos_info, LayoutContext &ctx);
    static void apply_chord_offsets(Segment *segment, staff_idx_t staff_idx, track_idx_t part_start_track, track_idx_t part_end_track,
                                    OffsetInfo &offset_info, const ChordPosInfo &pos_info, LayoutContext &ctx);
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_LAYOUTCHORDS_DEV_H
