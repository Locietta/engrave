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
    if (delete_excerpt) {
        delete excerpt;
        excerpt = nullptr;
    }
}

void AddExcerpt::undo() {
    delete_excerpt = true;
    excerpt->master_score()->remove_excerpt(excerpt);
}

void AddExcerpt::redo() {
    delete_excerpt = false;
    excerpt->master_score()->add_excerpt(excerpt);
}

std::vector<EngravingObject *> AddExcerpt::object_items() const {
    if (excerpt) {
        if (MasterScore *score = excerpt->master_score()) {
            return {score};
        }
    }

    return {};
}

//---------------------------------------------------------
//   RemoveExcerpt
//---------------------------------------------------------

RemoveExcerpt::RemoveExcerpt(Excerpt *ex) : excerpt(ex) { index = muse::index_of(excerpt->master_score()->excerpts(), excerpt); }

RemoveExcerpt::~RemoveExcerpt() {
    if (delete_excerpt) {
        delete excerpt;
        excerpt = nullptr;
    }
}

void RemoveExcerpt::undo() {
    delete_excerpt = false;
    excerpt->master_score()->add_excerpt(excerpt, index);
}

void RemoveExcerpt::redo() {
    delete_excerpt = true;
    excerpt->master_score()->remove_excerpt(excerpt);
}

std::vector<EngravingObject *> RemoveExcerpt::object_items() const {
    if (excerpt) {
        if (MasterScore *score = excerpt->master_score()) {
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
    score->set_excerpts_changed(true);
}

//---------------------------------------------------------
//   ChangeExcerptTitle
//---------------------------------------------------------

void ChangeExcerptTitle::flip() {
    String s = title;
    title = excerpt->name();
    excerpt->set_name(s);
    excerpt->master_score()->set_excerpts_changed(true);
}

//---------------------------------------------------------
//   AddPartToExcerpt
//---------------------------------------------------------

AddPartToExcerpt::AddPartToExcerpt(Excerpt *e, Part *p, size_t target_part_idx) : excerpt_(e), part_(p), target_part_idx_(target_part_idx) {
    assert(excerpt_);
    assert(part_);
}

void AddPartToExcerpt::undo() {
    muse::remove(excerpt_->parts(), part_);

    if (Score *score = excerpt_->excerpt_score()) {
        score->remove_part(part_);
    }
}

void AddPartToExcerpt::redo() {
    std::vector<Part *> &excerpt_parts = excerpt_->parts();
    if (target_part_idx_ < excerpt_parts.size()) {
        excerpt_parts.insert(excerpt_parts.begin() + target_part_idx_, part_);
    } else {
        excerpt_parts.push_back(part_);
    }

    if (Score *score = excerpt_->excerpt_score()) {
        score->insert_part(part_, target_part_idx_);
    }
}

void AddPartToExcerpt::cleanup(bool was_done) {
    if (!was_done) {
        delete part_;
        part_ = nullptr;
    }
}
