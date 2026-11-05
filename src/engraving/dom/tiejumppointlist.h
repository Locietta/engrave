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
class Note;
class Tie;
class TieJumpPointList;

// Items of TieJumpPointList
// Stores information about an endpoint
class TieJumpPoint {
public:
    TieJumpPoint(Note *note, bool active, int idx, bool followingNote);

    TieJumpPoint() {}

    Note *note() const { return note_; }

    Tie *endTie() const;

    bool followingNote() const { return followingNote_; }

    const muse::String &id() const { return id_; }

    bool active() const { return active_; }

    void setActive(bool v) { active_ = v; }

    void undoSetActive(bool v);

    // Stores the list that this end point belongs to. This is needed to find the start tie from an endpoint
    void setJumpPointList(TieJumpPointList *jumpPointList) { jumpPointList_ = jumpPointList; }

    TieJumpPointList *jumpPointList() { return jumpPointList_; }

    muse::TranslatableString menuTitle() const;

private:
    muse::String precedingJumpItemName() const;
    Note *note_ = nullptr;
    bool active_ = false;
    muse::String id_;
    TieJumpPointList *jumpPointList_ = nullptr;
    bool followingNote_ = false;
};

// Created & deleted by Note
// Manages adding and removing ties from endpoints
class TieJumpPointList {
public:
    TieJumpPointList(Note *note) : note_(note) {}

    ~TieJumpPointList();

    void add(TieJumpPoint *item);
    void clear();

    size_t size() const { return jumpPoints_.size(); }

    bool empty() const { return jumpPoints_.empty(); }

    Tie *startTie() const;

    TieJumpPoint *findJumpPoint(const muse::String &id);
    void toggleJumpPoint(const muse::String &id);

    void undoAddTieToScore(TieJumpPoint *jumpPoint);
    void undoRemoveTieFromScore(TieJumpPoint *jumpPoint);

    std::vector<TieJumpPoint *>::iterator begin() { return jumpPoints_.begin(); }

    std::vector<TieJumpPoint *>::const_iterator begin() const { return jumpPoints_.begin(); }

    std::vector<TieJumpPoint *>::iterator end() { return jumpPoints_.end(); }

    std::vector<TieJumpPoint *>::const_iterator end() const { return jumpPoints_.end(); }

private:
    std::vector<TieJumpPoint *> jumpPoints_;
    Note *note_ = nullptr;
};
} // namespace mu::engraving
