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

#include "../dom/score.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct ChangeMetaTags : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, ChangeMetaTags)

    Score *score = nullptr;
    std::map<String, String> meta_tags;

    void flip() override;

public:
    ChangeMetaTags(Score *s, const std::map<String, String> &m) : score(s), meta_tags(m) {}

    UNDO_TYPE(CommandType::CHANGE_META_INFO)
    UNDO_NAME("ChangeMetaTags")
    UNDO_CHANGED_OBJECTS({score})
};

struct ChangeMetaText : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, ChangeMetaText)

    Score *score = nullptr;
    String id;
    String text;

    void flip() override;

public:
    ChangeMetaText(Score *s, const String &i, const String &t) : score(s), id(i), text(t) {}

    UNDO_TYPE(CommandType::CHANGE_META_INFO)
    UNDO_NAME("ChangeMetaText")
    UNDO_CHANGED_OBJECTS({score})
};

struct ChangeScoreOrder : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, ChangeScoreOrder)

    Score *score = nullptr;
    ScoreOrder order;
    void flip() override;

public:
    ChangeScoreOrder(Score *sc, ScoreOrder so) : score(sc), order(so) {}

    UNDO_TYPE(CommandType::CHANGE_SCORE_ORDER)
    UNDO_NAME("ChangeScoreOrder")
    UNDO_CHANGED_OBJECTS({score})
};

struct ChangePageNumberOffset : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, ChangePageNumberOffset)

    Score *score = nullptr;
    int page_offset = 0;

    void flip() override;

public:
    ChangePageNumberOffset(Score *s, int po) : score(s), page_offset(po) {}

    UNDO_NAME("ChangePageNumberOffset")
    UNDO_CHANGED_OBJECTS({score})
};
} // namespace mu::engraving
