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

#include "textedit.h"

#include "navigation.h"
#include "transaction/transaction.h"
#include "transaction/undostack.h"

#include "iengravingfont.h"
#include "types/symnames.h"

#include "../dom/fret.h"
#include "../dom/lyrics.h"
#include "../dom/masterscore.h"
#include "../dom/score.h"
#include "../dom/symbol.h"
#include "../dom/utils.h"

#include "../rendering/iscorerenderer.h"

#include "log.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   ~TextEditData
//---------------------------------------------------------

TextEditData::~TextEditData() {
    if (delete_text) {
        for (EngravingObject *se : text_base_->link_list()) {
            to_text_base(se)->delete_later();
        }
    }
}

//---------------------------------------------------------
//   editInsertText
//---------------------------------------------------------

void TextBase::edit_insert_text(TextCursor *cursor, const String &s) {
    const LayoutData *ldata = this->ldata();
    IF_ASSERT_FAILED(ldata && !ldata->layout_invalid) { return; }

    text_invalid_ = true;

    int col = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!s.at(i).is_high_surrogate()) {
            ++col;
        }
    }

    const TextBlock &block = ldata->blocks.at(cursor->row());
    const CharFormat *previous_format = block.format_at(std::max(int(cursor->column()) - 1, 0));
    if (previous_format && previous_format->font_family() == "ScoreText" && s == " ") {
        // This space would be ignored by the xml parser (see #15629)
        // We must use the nonBreaking space character instead
        String non_breaking_space = String(Char(0xa0));
        cursor->cur_line().insert(cursor, non_breaking_space);
    } else {
        cursor->cur_line().insert(cursor, s);
    }

    cursor->set_column(cursor->column() + col);
    cursor->clear_selection();

    trigger_layout();
}

//---------------------------------------------------------
//   endEdit
//---------------------------------------------------------

void TextBase::commit_text() { score()->end_cmd(); }

//---------------------------------------------------------
//   insertSym
//---------------------------------------------------------

void TextBase::insert_sym(SymId id) {
    delete_selected_text();
    String s = score()->engraving_font()->to_string(id);
    cursor()->format()->set_font_family(u"ScoreText");
    score()->undo(new InsertText(cursor(), s));
}

//---------------------------------------------------------
//   insertText
//---------------------------------------------------------

void TextBase::insert_text(const String &s) { score()->undo(new InsertText(cursor(), s)); }

//---------------------------------------------------------
//   movePosition
//---------------------------------------------------------

void TextBase::move_position(TextCursor::MoveOperation op) {
    cursor()->move_position(op);
    score()->add_refresh(canvas_bounding_rect());
    score()->update();
}

//---------------------------------------------------------
//  ChangeText::insertText
//---------------------------------------------------------

void ChangeText::insert_text() {
    TextCursor tc = cursor_;
    tc.set_format(format_); // To undo TextCursor::updateCursorFormat()
    tc.text()->edit_insert_text(&tc, s_);
    *tc.text()->cursor() = tc;
}

//---------------------------------------------------------
//  ChangeText::removeText
//---------------------------------------------------------

void ChangeText::remove_text() {
    TextCursor tc = cursor_;
    TextBlock &l = cursor_.cur_line();
    size_t column = cursor_.column();
    format_ = *l.format_at(static_cast<int>(column + s_.size()) - 1);

    for (size_t n = 0; n < s_.size(); ++n) {
        l.remove(static_cast<int>(column), &cursor_);
    }
    cursor_.text()->trigger_layout();
    *cursor_.text()->cursor() = tc;
    cursor_.text()->set_text_invalid();
}

//---------------------------------------------------------
//   SplitJoinText
//---------------------------------------------------------

void SplitJoinText::join() {
    TextBase *t = cursor_.text();
    size_t line = cursor_.row();
    t->set_text_invalid();
    t->trigger_layout();

    TextBase::LayoutData *ldata = t->mutldata();
    CharFormat *char_fmt = cursor_.format(); // take current format
    size_t col = ldata->text_block(static_cast<int>(line) - 1).columns();
    int eol = ldata->text_block(static_cast<int>(line)).eol();
    auto fragments_list = ldata->text_block(static_cast<int>(line)).fragments_without_empty();

    if (fragments_list.size() > 0) {
        ldata->text_block(static_cast<int>(line) - 1).remove_empty_fragment();
    }
    muse::join(ldata->text_block(static_cast<int>(line) - 1).fragments(), fragments_list);

    ldata->blocks.erase(ldata->blocks.begin() + line);

    cursor_.set_row(line - 1);
    cursor_.cur_line().set_eol(eol);
    cursor_.set_column(col);
    cursor_.set_format(*char_fmt); // restore orig. format at new line
    cursor_.clear_selection();

    *t->cursor() = cursor_;
    t->set_text_invalid();
}

void SplitJoinText::split() {
    TextBase *t = cursor_.text();
    size_t line = cursor_.row();
    bool eol = cursor_.cur_line().eol();
    t->set_text_invalid();
    t->trigger_layout();

    TextBase::LayoutData *ldata = t->mutldata();
    CharFormat *char_fmt = cursor_.format(); // take current format
    ldata->blocks.insert(ldata->blocks.begin() + line + 1, cursor_.cur_line().split(static_cast<int>(cursor_.column()), t->cursor()));
    cursor_.cur_line().set_eol(true);

    cursor_.set_row(line + 1);
    cursor_.cur_line().set_eol(eol);
    cursor_.set_column(0);
    cursor_.set_format(*char_fmt); // restore orig. format at new line
    cursor_.clear_selection();

    *t->cursor() = cursor_;
    t->set_text_invalid();
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *TextBase::drop(Transaction &, EditData &ed) {
    TextCursor *cursor = this->cursor();

    EngravingItem *e = ed.drop_element;

    switch (e->type()) {
        case ElementType::SYMBOL: {
            SymId id = to_symbol(e)->sym();
            delete e;

            insert_sym(id);
        } break;

        case ElementType::FSYMBOL: {
            String s = to_f_symbol(e)->to_string();
            delete e;

            CharFormat *current_format = cursor->format();
            if (current_format->font_family() == u"ScoreText") {
                current_format->set_font_family(property_default(Pid::FONT_FACE).value<String>());
            }

            delete_selected_text();
            score()->undo(new InsertText(cursor, s));
        } break;

        default: LOGD("drop <%s> not handled", e->type_name()); break;
    }
    return 0;
}

//---------------------------------------------------------
//   paste
//---------------------------------------------------------

void TextBase::paste(const String &txt) {
    if (MScore::debug_mode) {
        LOGD("<%s>", muPrintable(txt));
    }

    int state = 0;
    String token;
    String sym;
    bool sym_state = false;
    CharFormat format = *cursor()->format();

    score()->start_cmd(TranslatableString("undoableAction", "Paste text"));
    for (size_t i = 0; i < txt.size(); i++) {
        Char c = txt.at(i);
        if (state == 0) {
            if (c == '<') {
                state = 1;
                token.clear();
            } else if (c == '&') {
                state = 2;
                token.clear();
            } else {
                if (sym_state) {
                    sym += c;
                } else {
                    delete_selected_text();
                    cursor()->set_format(format);
                    if (c.is_high_surrogate()) {
                        Char high_surrogate = c;
                        assert(i + 1 < txt.size());
                        i++;
                        Char low_surrogate = txt.at(i);
                        insert_text(String::from_ucs4(Char::surrogate_to_ucs4(high_surrogate, low_surrogate)));
                    } else if (c == '\r') {
                        continue;
                    } else if (c == '\n') {
                        score()->undo(new SplitText(cursor()));
                    } else {
                        insert_text(String(c));
                    }
                }
            }
        } else if (state == 1) {
            if (c == '>') {
                state = 0;
                if (token == "sym") {
                    sym_state = true;
                    sym.clear();
                } else if (token == "/sym") {
                    sym_state = false;
                    cursor()->set_format(format);
                    insert_sym(SymNames::sym_id_by_name(sym));
                } else {
                    prepare_format(token, format);
                }
            } else {
                token += c;
            }
        } else if (state == 2) {
            if (c == ';') {
                state = 0;
                if (token == "lt") {
                    insert_text(u"<");
                } else if (token == "gt") {
                    insert_text(u">");
                } else if (token == "amp") {
                    insert_text(u"&");
                } else if (token == "quot") {
                    insert_text(u"\"");
                } else {
                    insert_sym(SymNames::sym_id_by_name(token));
                }
            } else if (!c.is_letter()) {
                state = 0;
                insert_text(u"&");
                insert_text(token);
                insert_text(c);
            } else {
                token += c;
            }
        }
    }
    if (state == 2) {
        insert_text(u"&");
        insert_text(token);
    }
    score()->end_cmd();
}

//---------------------------------------------------------
//   endHexState
//---------------------------------------------------------

void TextBase::end_hex_state() {
    TextCursor *cursor = this->cursor();

    if (hex_state_ >= 0) {
        if (hex_state_ > 0) {
            size_t c2 = cursor->column();
            size_t c1 = c2 - (hex_state_ + 1);

            TextBlock &t = mutldata()->blocks[cursor->row()];
            String ss = t.remove(static_cast<int>(c1), hex_state_ + 1, cursor);
            bool ok;
            char32_t code = ss.mid(1).to_int(&ok, 16);
            cursor->set_column(c1);
            cursor->clear_selection();
            if (ok) {
                edit_insert_text(cursor, String::from_ucs4(code));
            } else {
                LOGD("cannot convert hex string <%s>, state %d (%zu-%zu)", muPrintable(ss.mid(1)), hex_state_, c1, c2);
            }
        }
        hex_state_ = -1;
    }
}

//---------------------------------------------------------
//   deleteSelectedText
//---------------------------------------------------------

bool TextBase::delete_selected_text() {
    TextCursor *cursor = this->cursor();

    if (!cursor->has_selection()) {
        return false;
    }

    size_t r1 = cursor->select_line();
    size_t c1 = cursor->select_column();

    if (r1 > cursor->row() || (r1 == cursor->row() && c1 > cursor->column())) {
        // swap start end of selection
        r1 = cursor->row();
        c1 = cursor->column();
        cursor->set_row(cursor_->select_line());
        cursor->set_column(cursor->select_column());
    }

    cursor->clear_selection();
    for (;;) {
        if (r1 == cursor->row() && c1 == cursor->column()) {
            break;
        }
        if (cursor->column() == 0 && cursor->row() != 0) {
            score()->undo(new JoinText(cursor));
        } else {
            // move cursor left:
            if (!cursor_->move_position(TextCursor::MoveOperation::LEFT)) {
                break;
            }
            TextCursor undo_cursor(*cursor_);
            score()->undo(new RemoveText(&undo_cursor, String(cursor_->current_character())));
        }
    }
    return true;
}

//---------------------------------------------------------
//   ChangeTextProperties
//---------------------------------------------------------

void ChangeTextProperties::restore_selection() {
    TextCursor &tc = cursor();
    tc.text()->cursor()->set_select_line(tc.select_line());
    tc.text()->cursor()->set_select_column(tc.select_column());
    tc.text()->cursor()->set_row(tc.row());
    tc.text()->cursor()->set_column(tc.column());
}

ChangeTextProperties::ChangeTextProperties(const TextCursor *tc, Pid prop_id, const PropertyValue &prop_val, PropertyFlags flags)
    : TextEditUndoableCommand(*tc) {
    property_id_ = prop_id;
    property_val_ = prop_val;
    flags_ = flags_;
    if (property_id_ == Pid::FONT_STYLE) {
        existing_style_ = static_cast<FontStyle>(cursor().text()->get_property(prop_id).to_int());
    }
}

void ChangeTextProperties::undo() {
    cursor().text()->reset_formatting();
    cursor().text()->set_xml_text(xml_text_);
    restore_selection();
    cursor().text()->renderer()->layout_text1(cursor().text());
}

void ChangeTextProperties::redo() {
    xml_text_ = cursor().text()->xml_text();
    restore_selection();
    cursor().text()->set_property_flags(property_id_, flags_);

    if (property_id_ == Pid::FONT_STYLE) {
        FontStyle set_style = static_cast<FontStyle>(property_val_.to_int());
        TextCursor *tc = cursor().text()->cursor();
        // user turned on bold/italic/underline/strike for text where it's not set, or turned it off for text where it is set,
        // note this logic only works because the user can only click one at a time
        if ((set_style & FontStyle::BOLD) != (existing_style_ & FontStyle::BOLD)) {
            tc->set_format(FormatId::BOLD, set_style & FontStyle::BOLD);
        }
        if ((set_style & FontStyle::ITALIC) != (existing_style_ & FontStyle::ITALIC)) {
            tc->set_format(FormatId::ITALIC, set_style & FontStyle::ITALIC);
        }
        if ((set_style & FontStyle::UNDERLINE) != (existing_style_ & FontStyle::UNDERLINE)) {
            tc->set_format(FormatId::UNDERLINE, set_style & FontStyle::UNDERLINE);
        }
        if ((set_style & FontStyle::STRIKE) != (existing_style_ & FontStyle::STRIKE)) {
            tc->set_format(FormatId::STRIKE, set_style & FontStyle::STRIKE);
        }
    } else {
        cursor().text()->set_property(property_id_, property_val_);
    }
}
