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

#pragma once

#include "layoutcontext.h"

namespace mu::engraving {
class Chord;
class Stem;
class StaffType;
class Rest;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class StemLayout {
public:
    // all in spatium units
    static constexpr double stafftype_tab_defaultstemlen_up = 3.0;
    static constexpr double stafftype_tab_defaultstemdist_up = 1.0;
    static constexpr double stafftype_tab_defaultstemposy_up =
        -::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemdist_up;
    static constexpr double stafftype_tab_defaultstemlen_dn = 3.0;
    static constexpr double stafftype_tab_defaultstemdist_dn = 1.0;
    static constexpr double stafftype_tab_defaultstemposy_dn =
        ::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemdist_dn;
    static constexpr double stafftype_tab_defaultstemlen_thru = 3.5;
    static constexpr double stafftype_tab_defaultstemposx = 0.75;
    // TAB STEM NOTATION
    // the ratio between the length of a full stem and the length of a short stem
    // (used for half note stems, in some TAB styles)
    static constexpr double stafftype_tab_shortstemratio = 0.5;
    // metrics of slashes through half note stems
    static constexpr double stafftype_tab_slash_width = 1.2;  /* X width of half note slash */
    static constexpr double stafftype_tab_slash_slanty = 0.8; /* the Y coord of the slash slant */
    static constexpr double stafftype_tab_slash_thick = 0.4;  /* slash thickness */
    static constexpr double stafftype_tab_slash_displ = 0.8;  /* the total displacement between one slash and the next:
                                                      includes slash thickness and empty space between slashes*/
    // the total height of a double slash
    static constexpr double stafftype_tab_slash_2_totheight = (::mu::engraving::rendering::score::StemLayout::stafftype_tab_slash_thick +
                                                               ::mu::engraving::rendering::score::StemLayout::stafftype_tab_slash_displ +
                                                               ::mu::engraving::rendering::score::StemLayout::stafftype_tab_slash_slanty);
    // the initial Y coord for a double shash on an UP stem = topmost corner of topmost slash
    static constexpr double stafftype_tab_slash_2_starty_up =
        ((::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemlen_up -
          ::mu::engraving::rendering::score::StemLayout::stafftype_tab_slash_2_totheight) *
         0.5);
    // the initial Y coord for a double shash on an DN stem = topmost corner of topmost slash
    static constexpr double stafftype_tab_slash_2_starty_dn =
        ((::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemlen_up +
          ::mu::engraving::rendering::score::StemLayout::stafftype_tab_slash_2_totheight) *
         0.5);

    static double calc_default_stem_length(Chord *item, const LayoutContext &ctx);
    static int min_staff_overlap(bool up, int staff_lines, int beam_count, bool has_hook, double beam_spacing, bool use_wide_beams,
                                 bool is_full_size);

    static double stem_pos_x(const ChordRest *item);
    static double stem_pos_x(const Chord *item);
    static double stem_pos_x(const Rest *item);

    static PointF stem_pos(const ChordRest *item);
    static PointF stem_pos(const Chord *item);
    static PointF stem_pos(const Rest *item);

    static double tab_stem_length(const Chord *item, const StaffType *st);

    static double tab_stem_pos_x() { return ::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemposx; }

    static double tab_rest_stem_pos_y(const ChordRest *item, const StaffType *st);
    static PointF tab_stem_pos(const Chord *item, const StaffType *st);

private:
    static int stem_length_beam_addition(const Chord *item, const LayoutContext &ctx);
    static int max_reduction(const Chord *item, const LayoutContext &ctx, int extension_outside_staff);
    static int stem_optical_adjustment(const Chord *item, int stem_end_position);
    static int calc_min_stem_length(Chord *item, const LayoutContext &ctx);
    static int calc4_beams_exception(const Chord *item, int stem_length);

    static inline int calc_beam_count(const Chord *item);
};
} // namespace mu::engraving::rendering::score
