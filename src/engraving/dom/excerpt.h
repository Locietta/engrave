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

#ifndef MU_ENGRAVING_EXCERPT_H
#define MU_ENGRAVING_EXCERPT_H

#include "../types/fraction.h"
#include "../types/types.h"
#include "types/string.h"

namespace mu::engraving {
struct MasterScore;
struct Measure;
struct Part;
struct Score;
struct Staff;
struct Spanner;
struct TieMap;

struct Excerpt {
    Excerpt(MasterScore *master_score = nullptr) { master_score_ = master_score; }

    Excerpt(const Excerpt &ex, bool copy_contents = true);

    ~Excerpt();

    bool inited() const;

    bool custom() const;
    void mark_as_custom();

    const ID &initial_part_id() const;
    void set_initial_part_id(const ID &id);

    MasterScore *master_score() const { return master_score_; }

    Score *excerpt_score() const { return excerpt_score_; }

    void set_excerpt_score(Score *s);

    const String &name() const;
    void set_name(const String &name, bool save_and_notify = true);

    // The name used to store this excerpt in the msc file/folder.
    // When reading/writing, the engraving module sets this value, so that other
    // modules can also read/write data about this excerpt using the correct name.
    bool has_file_name() const;
    const String &file_name() const;
    void set_file_name(const String &file_name);
    void update_file_name(size_t index = muse::nidx);

    std::vector<Part *> &parts() { return parts_; }

    const std::vector<Part *> &parts() const { return parts_; }

    void set_parts(const std::vector<Part *> &parts) { parts_ = parts; }

    bool contains_part(const Part *part) const;

    void remove_part(const ID &id);

    size_t nstaves() const;
    bool is_empty() const;

    const TracksMap &tracks_mapping();
    void set_tracks_mapping(const TracksMap &tracks_mapping);

    void set_voice_visible(Staff *staff, voice_idx_t voice_index, bool visible);

    static std::vector<Excerpt *> create_excerpts_from_parts(const std::vector<Part *> &parts, MasterScore *score);

    static void create_excerpt(Excerpt *);
    static void clone_staves(Score *source_score, Score *dst_score, const std::vector<staff_idx_t> &source_staves_indexes,
                             const TracksMap &all_tracks);
    static void clone_measures(Score *oscore, Score *score);
    static void link_measures(Score *excerpt_score, Score *master_score);
    static void clone_staff(Staff *ostaff, Staff *nstaff, bool clone_spanners = true);
    static void clone_staff2(Staff *ostaff, Staff *nstaff, const Fraction &start_tick, const Fraction &end_tick);
    static void clone_spanner(Spanner *s, Score *score, track_idx_t dst_track, track_idx_t dst_track2);
    static void create_linked_tabs(MasterScore *score);

private:
    friend struct MasterScore;

    static void promote_gap_rests_to_real_rests(const Measure *measure, staff_idx_t staff_idx);
    static void clone_mm_rests(Score *source_score, Score *dst_score, const std::vector<staff_idx_t> &source_staves_indexes,
                               const TracksMap &track_list, TieMap &tie_map);

    void set_inited(bool inited);
    void write_name_to_meta_tags();

    void update_tracks_mapping();

    MasterScore *master_score_ = nullptr;
    Score *excerpt_score_ = nullptr;
    String name_;
    String file_name_;
    std::vector<Part *> parts_;
    TracksMap tracks_mapping_;
    bool inited_ = false;
    ID initial_part_id_;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_EXCERPT_H
