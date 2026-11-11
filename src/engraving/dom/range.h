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

#include <list>
#include <vector>

#include "../types/fraction.h"
#include "../types/types.h"

namespace mu::engraving {
struct EngravingItem;
struct Measure;
struct Tuplet;
struct Segment;
struct Spanner;
struct ScoreRange;
struct ChordRest;
struct Chord;
struct Note;
struct Score;
struct Tie;
struct BarLine;
struct Spacer;
struct Marker;
struct Volta;

//---------------------------------------------------------
//   TrackList
//---------------------------------------------------------

struct TrackList : std::vector<EngravingItem *> {

public:
    typedef std::map<Chord *, Chord *> ClonedChordMap;

    TrackList(ScoreRange *r) { range_ = r; }

    ~TrackList();

    Fraction ticks() const { return duration_; }

    ScoreRange *range() const { return range_; }

    track_idx_t track() const { return track_; }

    void set_track(track_idx_t val) { track_ = val; }

    void read(const Segment *fs, const Segment *ls);
    bool write(Score *, const Fraction &) const;

    void append_gap(const Fraction &, Score *score);
    bool truncate(const Fraction &);
    void dump() const;

private:

    Tuplet *write_tuplet(Tuplet *parent, Tuplet *tuplet, Measure *&measure, Fraction &rest) const;
    void append(EngravingItem *);
    void append_tuplet(Tuplet *src_tuplet, Tuplet *dst_tuplet);
    void combine_tuplet(Tuplet *dst, Tuplet *src);

    Fraction duration_;
    ScoreRange *range_ = nullptr;
    track_idx_t track_ = 0;
    ClonedChordMap cloned_chord_;
};

//---------------------------------------------------------
//   Annotation
//---------------------------------------------------------

struct Annotation {
    Fraction tick;
    EngravingItem *e = nullptr;
};

//---------------------------------------------------------
//   ScoreRange
//---------------------------------------------------------

struct ScoreRange {
    ScoreRange() {}

    ~ScoreRange();
    void read(Segment *first, Segment *last, bool read_spanner = true);
    bool write(Score *, const Fraction &) const;
    Fraction ticks() const;

    Segment *first() const { return first_; }

    Segment *last() const { return last_; }

    void fill(const Fraction &);
    bool truncate(const Fraction &);

protected:
    std::vector<Spanner *> spanner_;
    std::vector<Annotation> annotations_;

private:
    struct BarLinesBackup {
        Fraction s_position;
        bool former_measure_start_or_end;
        BarLine *bl = nullptr;
    };

    std::vector<BarLinesBackup> bar_lines_;

    void backup_bar_lines(Segment *first, Segment *last);
    bool insert_bar_line(Measure *m, const BarLinesBackup &bar_line) const;
    void restore_bar_lines(Score *score, const Fraction &tick) const;
    void delete_bar_lines();

    struct BreaksBackup {
        Fraction s_position;
        LayoutBreakType l_break_type;
    };

    std::vector<BreaksBackup> breaks_;

    void backup_breaks(Segment *first, Segment *last);
    void restore_breaks(Score *score, const Fraction &tick) const;

    struct StartEndRepeatBackup {
        Fraction s_position;
        bool is_start_repeat;
    };

    std::vector<StartEndRepeatBackup> start_end_repeats_;

    void backup_repeats(Segment *first, Segment *last);
    void restore_repeats(Score *score, const Fraction &tick) const;

    struct SpacerBackup {
        Fraction s_position;
        staff_idx_t staff_idx;
        Spacer *s = nullptr;
    };

    void backup_spacers(Segment *first, Segment *last);
    void restore_spacers(Score *score, const Fraction &tick) const;
    void delete_spacers();

    struct JumpsMarkersBackup {
        Fraction s_position;
        EngravingItem *e = nullptr;
    };

    bool end_of_measure_element(EngravingItem *e) const;
    void backup_jumps_and_markers(Segment *first, Segment *last);
    void restore_jumps_and_markers(Score *score, const Fraction &tick) const;
    void delete_jumps_and_markers();

    void restore_volta(Score *score, const Fraction &tick, Volta *v) const;

    friend struct TrackList;

    std::vector<TrackList *> tracks_;
    std::vector<Tie *> start_ties_;
    std::vector<SpacerBackup> spacers_;
    std::vector<JumpsMarkersBackup> jumps_markers_;
    Segment *first_ = nullptr;
    Segment *last_ = nullptr;
};
} // namespace mu::engraving
