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

#include "editexcerpt.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   AddExcerpt
//---------------------------------------------------------

AddExcerpt::AddExcerpt(Excerpt *ex) : excerpt(ex) {}

AddExcerpt::~AddExcerpt() {
    if (deleteExcerpt) {
        delete excerpt;
        excerpt = nullptr;
    }
}

void AddExcerpt::undo() {
    deleteExcerpt = true;
    excerpt->masterScore()->removeExcerpt(excerpt);
}

void AddExcerpt::redo() {
    deleteExcerpt = false;
    excerpt->masterScore()->addExcerpt(excerpt);
}

std::vector<EngravingObject *> AddExcerpt::objectItems() const {
    if (excerpt) {
        if (MasterScore *score = excerpt->masterScore()) {
            return {score};
        }
    }

    return {};
}

//---------------------------------------------------------
//   RemoveExcerpt
//---------------------------------------------------------

RemoveExcerpt::RemoveExcerpt(Excerpt *ex) : excerpt(ex) { index = muse::indexOf(excerpt->masterScore()->excerpts(), excerpt); }

RemoveExcerpt::~RemoveExcerpt() {
    if (deleteExcerpt) {
        delete excerpt;
        excerpt = nullptr;
    }
}

void RemoveExcerpt::undo() {
    deleteExcerpt = false;
    excerpt->masterScore()->addExcerpt(excerpt, index);
}

void RemoveExcerpt::redo() {
    deleteExcerpt = true;
    excerpt->masterScore()->removeExcerpt(excerpt);
}

std::vector<EngravingObject *> RemoveExcerpt::objectItems() const {
    if (excerpt) {
        if (MasterScore *score = excerpt->masterScore()) {
            return {score};
        }
    }

    return {};
}

//---------------------------------------------------------
//   SwapExcerpt
//---------------------------------------------------------

void SwapExcerpt::flip() {
    Excerpt *tmp = score->excerpts().at(pos1);
    score->excerpts()[pos1] = score->excerpts().at(pos2);
    score->excerpts()[pos2] = tmp;
    score->setExcerptsChanged(true);
}

//---------------------------------------------------------
//   ChangeExcerptTitle
//---------------------------------------------------------

void ChangeExcerptTitle::flip() {
    String s = title;
    title = excerpt->name();
    excerpt->setName(s);
    excerpt->masterScore()->setExcerptsChanged(true);
}

//---------------------------------------------------------
//   AddPartToExcerpt
//---------------------------------------------------------

AddPartToExcerpt::AddPartToExcerpt(Excerpt *e, Part *p, size_t targetPartIdx) : excerpt_(e), part_(p), targetPartIdx_(targetPartIdx) {
    assert(excerpt_);
    assert(part_);
}

void AddPartToExcerpt::undo() {
    muse::remove(excerpt_->parts(), part_);

    if (Score *score = excerpt_->excerptScore()) {
        score->removePart(part_);
    }
}

void AddPartToExcerpt::redo() {
    std::vector<Part *> &excerptParts = excerpt_->parts();
    if (targetPartIdx_ < excerptParts.size()) {
        excerptParts.insert(excerptParts.begin() + targetPartIdx_, part_);
    } else {
        excerptParts.push_back(part_);
    }

    if (Score *score = excerpt_->excerptScore()) {
        score->insertPart(part_, targetPartIdx_);
    }
}

void AddPartToExcerpt::cleanup(bool wasDone) {
    if (!wasDone) {
        delete part_;
        part_ = nullptr;
    }
}
