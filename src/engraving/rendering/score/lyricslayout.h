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
#ifndef MU_ENGRAVING_LYRICSLAYOUT_DEV_H
#define MU_ENGRAVING_LYRICSLAYOUT_DEV_H

#include <optional>

#include "layoutcontext.h"

namespace mu::engraving {
struct Score;
struct System;
struct Lyrics;
struct LyricsLine;
struct LyricsLineSegment;
struct SkylineLine;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct LyricsLayout {
private:
    struct LyricsVerse {
    private:
        std::vector<Lyrics *> lyrics_;
        std::vector<LyricsLineSegment *> lines_;
    public:
        void add_lyrics(Lyrics *l) { lyrics_.push_back(l); }

        void add_line(LyricsLineSegment *lls) { lines_.push_back(lls); }

        const std::vector<Lyrics *> &lyrics() const { return lyrics_; }

        const std::vector<LyricsLineSegment *> &lines() const { return lines_; }
    };

    using LyricsVersesMap = std::map<int, LyricsVerse>;

public:
    LyricsLayout() = default;

    static void layout(Lyrics *item, LayoutContext &ctx);
    static void layout(LyricsLine *item);
    static void layout(LyricsLineSegment *item, LayoutContext &ctx);

    static void compute_vertical_positions(System *system, LayoutContext &ctx);

private:
    static void create_or_remove_lyrics_line(Lyrics *item, LayoutContext &ctx);

    static void layout_melisma_line(LyricsLineSegment *item);
    static void layout_dashes(LyricsLineSegment *item);

    static Lyrics *find_next_lyrics(const ChordRest *end_chord_rest, int verse_number, std::optional<PlacementV> placement = std::nullopt);

    static void compute_vertical_positions(staff_idx_t staff_idx, System *system, LayoutContext &ctx);
    static void collect_lyrics_verses(staff_idx_t staff_idx, System *system, LyricsVersesMap &lyrics_verses_above,
                                      LyricsVersesMap &lyrics_verses_below);

    static void set_default_positions(staff_idx_t staff_idx, const LyricsVersesMap &lyrics_verses_above,
                                      const LyricsVersesMap &lyrics_verses_below, LayoutContext &ctx);

    static void check_collisions_with_staff_elements(System *system, staff_idx_t staff_idx, LayoutContext &ctx,
                                                     const LyricsVersesMap &lyrics_verses_above,
                                                     const LyricsVersesMap &lyrics_verses_below);
    static SkylineLine create_skyline_for_verse(int verse, bool north, const LyricsVersesMap &lyrics_verses, System *system);
    static void move_this_verse_and_outer_ones(int verse, int last_verse, bool above, double diff, const LyricsVersesMap &lyrics_verses);

    static void add_to_skyline(System *system, staff_idx_t staff_idx, LayoutContext &ctx, const LyricsVersesMap &lyrics_verses_above,
                               const LyricsVersesMap &lyrics_verses_below);

    static double lyrics_line_start_x(const LyricsLineSegment *item);
    static double lyrics_line_end_x(const LyricsLineSegment *item, const Lyrics *end_lyrics = nullptr);
    static void adjust_lyrics_line_y_offset(LyricsLineSegment *item, const Lyrics *end_lyrics = nullptr);
};
} // namespace mu::engraving::rendering::score
#endif // MU_ENGRAVING_LYRICSLAYOUT_DEV_H
