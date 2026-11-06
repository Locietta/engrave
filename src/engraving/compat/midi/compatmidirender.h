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

#include <set>
#include <vector>

#include "engraving/types/types.h"

#include "event.h"
#include "compatmidirenderinternal.h"

namespace mu::engraving {
class Chord;
class Drumset;
class Glissando;
class Instrument;
class Measure;
class Note;
class NoteEventList;
class Score;
class Segment;
class Trill;

class CompatMidiRender {
public:
    static void render_score(Score *score, EventsHolder &events, const CompatMidiRendererInternal::Context &ctx, bool expand_repeats);
    static int get_controller_for_snd(Score *score, int global_snd_controller);
    static void create_play_events(const Score *score, Measure const *start = nullptr, Measure const *end = nullptr,
                                   const CompatMidiRendererInternal::Context &context = CompatMidiRendererInternal::Context());

    static int tick(const CompatMidiRendererInternal::Context &ctx, int tick);
private:
    static void create_play_events(const Score *score, const CompatMidiRendererInternal::Context &context, Chord *chord,
                                   Chord *prev_chord = nullptr, Chord *next_chord = nullptr);
    static void create_grace_notes_play_events(const Score *score, const Fraction &tick, Chord *chord, int &ontime, int &trailtime);
    static std::vector<NoteEventList> render_chord(const CompatMidiRendererInternal::Context &context, Chord *chord, Chord *prev_chord,
                                                   int gate_time, int ontime, int trailtime);
    static void render_arpeggio(Chord *chord, std::vector<NoteEventList> &ell, int ontime);
    static void render_tremolo(Chord *chord, std::vector<NoteEventList> &ell, int &ontime, double tremolo_part_of_chord = 1.0);
    static void render_chord_articulation(const CompatMidiRendererInternal::Context &context, Chord *chord, std::vector<NoteEventList> &ell,
                                          int &gate_time, double grace_on_beat_proportion, bool tremolo_before = false);
    static void update_gate_time(const Instrument *instr, int &gate_time, const String &articulation_name,
                                 const CompatMidiRendererInternal::Context &context);
    static void render_glissando(NoteEventList *events, Note *notestart, double grace_on_beat_proportion, bool tremolo_before = false);
    static bool render_note_articulation(NoteEventList *events, Note *note, bool chromatic, int requested_ticks_per_note,
                                         const std::vector<int> &prefix, const std::vector<int> &body, bool repeatp, bool sustainp,
                                         const std::vector<int> &suffix, int fastest_freq = 64, int slowest_freq = 8, // 64 Hz and 8 Hz
                                         double grace_on_beat_proportion = 0, bool tremolo_before = false);
    static bool render_note_articulation(NoteEventList *events, Note *note, bool chromatic, SymId articulation_type,
                                         OrnamentStyle ornament_style);
    static bool render_note_articulation(NoteEventList *events, Note *note, bool chromatic, TrillType trill_type,
                                         OrnamentStyle ornament_style);
    static void create_slide_in_note_play_events(Note *note, Chord *prev_chord, NoteEventList *el);
    static void create_slide_out_note_play_events(Note *note, NoteEventList *el, int on_time, bool has_tremolo);

    // Helpers
    static Chord *get_chord_from_segment(Segment *segment, track_idx_t track);
    static Trill *find_first_trill(Chord *chord);
    static int adjust_trailtime(int trailtime, Chord *current_chord, Chord *next_chord);
    static bool note_is_glissando_start(Note *note);
    static int slide_length_chord_dependent(Chord *chord);
    static std::set<size_t> get_notes_indexes_to_render(Chord *chord);
    static Glissando *back_glissando(Note *note);
    static bool is_glissando_valid(Glissando *glissando);
    static const Drumset *get_drumset(const Chord *chord);
    static int total_tied_note_ticks(Note *note);
};
} // namespace mu::engraving
