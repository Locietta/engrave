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

#include "../dom/textbase.h"
#include "elementeditdata.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
//---------------------------------------------------------
//   TextEditData
//---------------------------------------------------------

struct TextEditData : public ElementEditData {
public:

    String old_xml_text;
    size_t start_undo_idx = 0;

    bool delete_text = false;

    String selected_text;
    String selected_plain_text;
    static constexpr const char *mime_rich_text_format = "application/musescore/richtext";

    TextEditData(TextBase *t) : text_base_(t) {}

    TextEditData(const TextEditData &) = delete;
    TextEditData &operator=(const TextEditData &) = delete;
    ~TextEditData();

    virtual EditDataType type() override { return EditDataType::TEXT_EDIT_DATA; }

    void set_delete_text(bool val) { delete_text = val; }

private:
    TextBase *text_base_ = nullptr;
};

//---------------------------------------------------------
//   TextEditUndoableCommand
//---------------------------------------------------------

struct TextEditUndoableCommand : UndoableCommand {

public:
    UNDO_TYPE(CommandType::TEXT_EDIT)
    UNDO_NAME("TextEdit")
    UNDO_CHANGED_OBJECTS({cursor_.text()})

    TextEditUndoableCommand(const TextCursor &tc) : cursor_(tc) {}

    bool matches_filter(UndoableCommandFilter f, const EngravingItem *target) const override {
        return f == UndoableCommandFilter::TEXT_EDIT && cursor_.text() == target;
    }

    TextCursor &cursor() { return cursor_; }

    const TextCursor &cursor() const { return cursor_; }

protected:
    TextCursor cursor_;
};

//---------------------------------------------------------
//   ChangeText
//---------------------------------------------------------

struct ChangeTextProperties : TextEditUndoableCommand {

public:
    ChangeTextProperties(const TextCursor *tc, Pid prop_id, const PropertyValue &prop_val, PropertyFlags flags);
    void undo() override;
    void redo() override;

private:

    void restore_selection();

    String xml_text_;
    Pid property_id_;
    PropertyValue property_val_;
    FontStyle existing_style_;
    PropertyFlags flags_;
};

//---------------------------------------------------------
//   ChangeText
//---------------------------------------------------------

struct ChangeText : TextEditUndoableCommand {

public:
    ChangeText(const TextCursor *tc, const String &t) : TextEditUndoableCommand(*tc), s_(t), format_(*tc->format()) {}

    virtual void undo() override = 0;
    virtual void redo() override = 0;

    const String &string() const { return s_; }

protected:
    void insert_text();
    void remove_text();

private:

    String s_;
    CharFormat format_;
};

//---------------------------------------------------------
//   InsertText
//---------------------------------------------------------

struct InsertText : ChangeText {
public:
    InsertText(const TextCursor *tc, const String &t) : ChangeText(tc, t) {}

    virtual void redo() override { insert_text(); }

    virtual void undo() override { remove_text(); }
    UNDO_NAME("InsertText")
};

//---------------------------------------------------------
//   RemoveText
//---------------------------------------------------------

struct RemoveText : ChangeText {
public:
    RemoveText(const TextCursor *tc, const String &t) : ChangeText(tc, t) {}

    virtual void redo() override { remove_text(); }

    virtual void undo() override { insert_text(); }
    UNDO_NAME("RemoveText")
};

//---------------------------------------------------------
//   SplitJoinText
//---------------------------------------------------------

struct SplitJoinText : TextEditUndoableCommand {
protected:
    virtual void split();
    virtual void join();

public:
    SplitJoinText(const TextCursor *tc) : TextEditUndoableCommand(*tc) {}
};

//---------------------------------------------------------
//   SplitText
//---------------------------------------------------------

struct SplitText : SplitJoinText {
private:

    virtual void undo() override { join(); }

    virtual void redo() override { split(); }

public:
    SplitText(const TextCursor *tc) : SplitJoinText(tc) {}
    UNDO_NAME("SplitText")
};

//---------------------------------------------------------
//   JoinText
//---------------------------------------------------------

struct JoinText : SplitJoinText {
private:

    virtual void undo() override { split(); }

    virtual void redo() override { join(); }

public:
    JoinText(const TextCursor *tc) : SplitJoinText(tc) {}
    UNDO_NAME("JoinText")
};
} // namespace mu::engraving
