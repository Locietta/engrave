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

#pragma once

#include "../types/types.h"
#include "draw/types/geometry.h"
#include "interval.h"

namespace mu::engraving {
struct Chord;
struct ChordRest;
struct Drumset;
struct EngravingItem;
struct KeySig;
struct Lyrics;
struct Measure;
struct Note;
struct PartialLyricsLine;
struct Rest;
struct Score;
struct Score;
struct Segment;
struct Selection;
struct Spanner;
struct Staff;
struct System;
struct Tuplet;
struct Volta;
struct NoteVal;

enum class Key : signed char;

extern RectF handle_rect(const PointF &pos);

extern int pitch_key_adjust(int note, Key);
extern int line2pitch(int line, ClefType clef, Key);
extern int y2pitch(double y, ClefType clef, double spatium);

extern String pitch2string(int v, bool use_flats = false);
extern int string2pitch(const String &s);
extern String convert_pitch_string_flats_and_sharps_to_unicode(const String &str);

extern Note *next_chord_note(Note *note);
extern Note *prev_chord_note(Note *note);

extern Note *search_tie_note(const Note *note, const Segment *next_segment = nullptr, const bool disable_over_repeats = true);

extern int abs_step(int pitch);
extern int abs_step(int tpc, int pitch);

extern int abs_step(int line, ClefType clef);
extern int rel_step(int line, ClefType clef);
extern int rel_step(int pitch, int tpc, ClefType clef);
extern int pitch2step(int pitch);
extern int step2pitch(int step);
int chromatic_pitch_steps(const Note *note_l, const Note *note_r, const int nominal_diatonic_steps);
extern int note_val_to_line(const NoteVal &nval, const Staff *staff, const Fraction &tick);
extern AccidentalVal note_val_to_accidental_val(const NoteVal &nval, const Staff *staff, const Fraction &tick);
extern int compare_notes_pos(const Note *n1, const Note *n2);

extern Segment *skip_tuplet(Tuplet *tuplet);
extern SymIdList time_sig_sym_ids_from_string(const String &, TimeSigStyle time_sig_style = TimeSigStyle::NORMAL);
extern Fraction actual_ticks(Fraction duration, Tuplet *tuplet, Fraction time_stretch);

extern bool drag_position_to_measure(const PointF &pos, const Score *score, Measure **measure, staff_idx_t *staff_idx,
                                     const double spacing_factor = 0.5);
extern bool drag_position_to_segment(const PointF &pos, const Measure *measure, const staff_idx_t staff_idx, Segment **segment,
                                     const double spacing_factor = 0.5, const bool allow_time_anchor = false);
extern Segment *segment_or_chord_rest_segment_at_same_tick(Segment *segment);

extern double y_staff_difference(const System *system1, const System *system2, staff_idx_t staff_idx1);

extern bool allow_remove_when_removing_staves(EngravingItem *item, staff_idx_t start_staff, staff_idx_t end_staff = 0);
extern bool move_down_when_adding_staves(EngravingItem *item, staff_idx_t start_staff, staff_idx_t end_staff = 0);

extern void collect_chords_and_rest(Segment *segment, staff_idx_t staff_idx, std::vector<Chord *> &chords, std::vector<Rest *> &rests);
extern void collect_chords_overlapping_rests(Segment *segment, staff_idx_t staff_idx, std::vector<Chord *> &chords);
extern std::vector<EngravingItem *> collect_system_objects(const Score *score, const std::vector<Staff *> &staves = {});
extern std::unordered_set<EngravingItem *> collect_elements_anchored_to_chord_rest(const ChordRest *cr);
extern std::unordered_set<EngravingItem *> collect_elements_anchored_to_note(const Note *cr, bool include_forward_ties_spanners,
                                                                             bool include_backward_ties_spanners);

extern MeasureBeat find_beat(const Score *score, int tick);

extern bool note_anchored_spanner_is_in_range(const Spanner *, const Fraction &range_start, const Fraction &range_end);

extern Interval ornament_interval_to_general_interval(OrnamentInterval interval);

extern String format_unique_excerpt_name(const String &base_name, const StringList &all_excerpt_lower_names);

extern bool is_first_system_key_sig(const KeySig *ks);

extern String bend_amount_to_string(int fulls, int quarts, bool use_fractions = true);

extern InstrumentTrackId make_instrument_track_id(const EngravingItem *item);

extern std::vector<Measure *> find_following_repeat_measures(const Measure *measure);
extern std::vector<Measure *> find_previous_repeat_measures(const Measure *measure);
extern bool repeat_has_partial_lyric_line(const Measure *end_repeat_measure);
extern bool segments_are_adjacent(const Segment *first_seg, const Segment *second_seg);
extern bool segments_are_in_different_repeat_segments(const Segment *first_seg, const Segment *second_seg);
extern bool is_valid_bar_line_for_repeat_section(const Segment *first_seg, const Segment *second_seg);

extern PartialLyricsLine *find_prev_partial_lyrics_line_dash(Lyrics *lyrics);

extern bool is_element_in_fret_box(const EngravingItem *item);

extern std::vector<EngravingItem *> filter_target_elements(const Selection &sel, EngravingItem *drop_element, bool &unique);

extern Lyrics *search_next_lyrics(Segment *s, staff_idx_t staff_idx, int verse, PlacementV p);
extern bool note_is_before(const Note *n1, const Note *n2);
extern void update_percussion_notes(Chord *c, const Drumset *drumset);
} // namespace mu::engraving
