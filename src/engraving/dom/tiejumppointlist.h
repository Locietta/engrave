/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "global/types/string.h"
#include "global/types/translatablestring.h"

namespace mu::engraving {
struct Note;
struct Tie;
struct TieJumpPointList;

// Items of TieJumpPointList
// Stores information about an endpoint
struct TieJumpPoint {
    TieJumpPoint(Note *note, bool active, int idx, bool following_note);

    TieJumpPoint() {}

    Note *note() const { return note_; }

    Tie *end_tie() const;

    bool following_note() const { return following_note_; }

    const muse::String &id() const { return id_; }

    bool active() const { return active_; }

    void set_active(bool v) { active_ = v; }

    void undo_set_active(bool v);

    // Stores the list that this end point belongs to. This is needed to find the start tie from an endpoint
    void set_jump_point_list(TieJumpPointList *jump_point_list) { jump_point_list_ = jump_point_list; }

    TieJumpPointList *jump_point_list() { return jump_point_list_; }

    muse::TranslatableString menu_title() const;

private:
    muse::String preceding_jump_item_name() const;
    Note *note_ = nullptr;
    bool active_ = false;
    muse::String id_;
    TieJumpPointList *jump_point_list_ = nullptr;
    bool following_note_ = false;
};

// Created & deleted by Note
// Manages adding and removing ties from endpoints
struct TieJumpPointList {
    TieJumpPointList(Note *note) : note_(note) {}

    ~TieJumpPointList();

    void add(TieJumpPoint *item);
    void clear();

    size_t size() const { return jump_points_.size(); }

    bool empty() const { return jump_points_.empty(); }

    Tie *start_tie() const;

    TieJumpPoint *find_jump_point(const muse::String &id);
    void toggle_jump_point(const muse::String &id);

    void undo_add_tie_to_score(TieJumpPoint *jump_point);
    void undo_remove_tie_from_score(TieJumpPoint *jump_point);

    std::vector<TieJumpPoint *>::iterator begin() { return jump_points_.begin(); }

    std::vector<TieJumpPoint *>::const_iterator begin() const { return jump_points_.begin(); }

    std::vector<TieJumpPoint *>::iterator end() { return jump_points_.end(); }

    std::vector<TieJumpPoint *>::const_iterator end() const { return jump_points_.end(); }

private:
    std::vector<TieJumpPoint *> jump_points_;
    Note *note_ = nullptr;
};
} // namespace mu::engraving
