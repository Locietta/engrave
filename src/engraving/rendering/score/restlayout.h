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

#include "dom/mmrest.h"
#include "dom/rest.h"
#include "layoutcontext.h"

namespace mu::engraving {
struct MMRest;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
using RestGroup = std::vector<Rest *>;
using RestGroups = std::vector<RestGroup>;
using InterruptionPoints = std::array<std::list<Fraction>, ::mu::engraving::voices>;

struct RestLayout {
    static void layout_rest(const Rest *item, Rest::LayoutData *ldata, const LayoutContext &ctx);
    static void fill_shape(const Rest *item, Rest::LayoutData *ldata, const LayoutConfiguration &conf);
    static Shape compute_shape(const Rest *item, const Rest::LayoutData *ldata, const LayoutConfiguration &conf);

    static void resolve_vertical_rest_conflicts(LayoutContext &ctx, Segment *segment, staff_idx_t staff_idx);
    static void resolve_rest_vs_chord(std::vector<Rest *> &rests, std::vector<Chord *> &chords, const Staff *staff, Segment *segment);
    static void resolve_rest_vs_rest(std::vector<Rest *> &rests, const Staff *staff, Segment *segment, LayoutContext &ctx,
                                     bool consider_beams = false);

    static void align_rests(const System *system, LayoutContext &ctx);
    static void check_full_measure_rest_collisions(const System *system, LayoutContext &ctx);

private:
    static void fill_shape(const Rest *item, Rest::LayoutData *ldata);
    static Shape compute_shape(const Rest *item, const Rest::LayoutData *ldata);
    static void fill_shape(const MMRest *item, MMRest::LayoutData *ldata, const LayoutConfiguration &conf);
    static Shape compute_shape(const MMRest *item, const MMRest::LayoutData *ldata, const LayoutConfiguration &conf);

    static int compute_natural_line(int lines);
    static int compute_voice_offset(const Rest *item, Rest::LayoutData *ldata); // Vertical displacement in multi-voice cases
    static int compute_whole_or_breve_rest_offset(const Rest *item, int voice_offset, int lines);

    static void update_symbol(const Rest *item, Rest::LayoutData *ldata);

    static RestGroups compute_rest_groups(const System *system, LayoutContext &ctx);
    static InterruptionPoints compute_interruption_points(const Measure *measure, staff_idx_t staff_idx);
};
} // namespace mu::engraving::rendering::score
