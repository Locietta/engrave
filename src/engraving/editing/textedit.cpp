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
    if (deleteText) {
        for (EngravingObject *se : textBase_->linkList()) {
            toTextBase(se)->deleteLater();
        }
    }
}

//---------------------------------------------------------
//   editInsertText
//---------------------------------------------------------

void TextBase::editInsertText(TextCursor *cursor, const String &s) {
    const LayoutData *ldata = this->ldata();
    IF_ASSERT_FAILED(ldata && !ldata->layoutInvalid) { return; }

    textInvalid_ = true;

    int col = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!s.at(i).isHighSurrogate()) {
            ++col;
        }
    }

    const TextBlock &block = ldata->blocks.at(cursor->row());
    const CharFormat *previousFormat = block.formatAt(std::max(int(cursor->column()) - 1, 0));
    if (previousFormat && previousFormat->fontFamily() == "ScoreText" && s == " ") {
        // This space would be ignored by the xml parser (see #15629)
        // We must use the nonBreaking space character instead
        String nonBreakingSpace = String(Char(0xa0));
        cursor->curLine().insert(cursor, nonBreakingSpace);
    } else {
        cursor->curLine().insert(cursor, s);
    }

    cursor->setColumn(cursor->column() + col);
    cursor->clearSelection();

    triggerLayout();
}

//---------------------------------------------------------
//   endEdit
//---------------------------------------------------------

void TextBase::commitText() { score()->endCmd(); }

//---------------------------------------------------------
//   insertSym
//---------------------------------------------------------

void TextBase::insertSym(SymId id) {
    deleteSelectedText();
    String s = score()->engravingFont()->toString(id);
    cursor()->format()->setFontFamily(u"ScoreText");
    score()->undo(new InsertText(cursor(), s));
}

//---------------------------------------------------------
//   insertText
//---------------------------------------------------------

void TextBase::insertText(const String &s) { score()->undo(new InsertText(cursor(), s)); }

//---------------------------------------------------------
//   movePosition
//---------------------------------------------------------

void TextBase::movePosition(TextCursor::MoveOperation op) {
    cursor()->movePosition(op);
    score()->addRefresh(canvasBoundingRect());
    score()->update();
}

//---------------------------------------------------------
//  ChangeText::insertText
//---------------------------------------------------------

void ChangeText::insertText() {
    TextCursor tc = cursor_;
    tc.setFormat(format_); // To undo TextCursor::updateCursorFormat()
    tc.text()->editInsertText(&tc, s_);
    *tc.text()->cursor() = tc;
}

//---------------------------------------------------------
//  ChangeText::removeText
//---------------------------------------------------------

void ChangeText::removeText() {
    TextCursor tc = cursor_;
    TextBlock &l = cursor_.curLine();
    size_t column = cursor_.column();
    format_ = *l.formatAt(static_cast<int>(column + s_.size()) - 1);

    for (size_t n = 0; n < s_.size(); ++n) {
        l.remove(static_cast<int>(column), &cursor_);
    }
    cursor_.text()->triggerLayout();
    *cursor_.text()->cursor() = tc;
    cursor_.text()->setTextInvalid();
}

//---------------------------------------------------------
//   SplitJoinText
//---------------------------------------------------------

void SplitJoinText::join() {
    TextBase *t = cursor_.text();
    size_t line = cursor_.row();
    t->setTextInvalid();
    t->triggerLayout();

    TextBase::LayoutData *ldata = t->mutldata();
    CharFormat *charFmt = cursor_.format(); // take current format
    size_t col = ldata->textBlock(static_cast<int>(line) - 1).columns();
    int eol = ldata->textBlock(static_cast<int>(line)).eol();
    auto fragmentsList = ldata->textBlock(static_cast<int>(line)).fragmentsWithoutEmpty();

    if (fragmentsList.size() > 0) {
        ldata->textBlock(static_cast<int>(line) - 1).removeEmptyFragment();
    }
    muse::join(ldata->textBlock(static_cast<int>(line) - 1).fragments(), fragmentsList);

    ldata->blocks.erase(ldata->blocks.begin() + line);

    cursor_.setRow(line - 1);
    cursor_.curLine().setEol(eol);
    cursor_.setColumn(col);
    cursor_.setFormat(*charFmt); // restore orig. format at new line
    cursor_.clearSelection();

    *t->cursor() = cursor_;
    t->setTextInvalid();
}

void SplitJoinText::split() {
    TextBase *t = cursor_.text();
    size_t line = cursor_.row();
    bool eol = cursor_.curLine().eol();
    t->setTextInvalid();
    t->triggerLayout();

    TextBase::LayoutData *ldata = t->mutldata();
    CharFormat *charFmt = cursor_.format(); // take current format
    ldata->blocks.insert(ldata->blocks.begin() + line + 1, cursor_.curLine().split(static_cast<int>(cursor_.column()), t->cursor()));
    cursor_.curLine().setEol(true);

    cursor_.setRow(line + 1);
    cursor_.curLine().setEol(eol);
    cursor_.setColumn(0);
    cursor_.setFormat(*charFmt); // restore orig. format at new line
    cursor_.clearSelection();

    *t->cursor() = cursor_;
    t->setTextInvalid();
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *TextBase::drop(Transaction &, EditData &ed) {
    TextCursor *cursor = this->cursor();

    EngravingItem *e = ed.dropElement;

    switch (e->type()) {
        case ElementType::SYMBOL: {
            SymId id = toSymbol(e)->sym();
            delete e;

            insertSym(id);
        } break;

        case ElementType::FSYMBOL: {
            String s = toFSymbol(e)->toString();
            delete e;

            CharFormat *currentFormat = cursor->format();
            if (currentFormat->fontFamily() == u"ScoreText") {
                currentFormat->setFontFamily(propertyDefault(Pid::FONT_FACE).value<String>());
            }

            deleteSelectedText();
            score()->undo(new InsertText(cursor, s));
        } break;

        default: LOGD("drop <%s> not handled", e->typeName()); break;
    }
    return 0;
}

//---------------------------------------------------------
//   paste
//---------------------------------------------------------

void TextBase::paste(const String &txt) {
    if (MScore::debugMode) {
        LOGD("<%s>", muPrintable(txt));
    }

    int state = 0;
    String token;
    String sym;
    bool symState = false;
    CharFormat format = *cursor()->format();

    score()->startCmd(TranslatableString("undoableAction", "Paste text"));
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
                if (symState) {
                    sym += c;
                } else {
                    deleteSelectedText();
                    cursor()->setFormat(format);
                    if (c.isHighSurrogate()) {
                        Char highSurrogate = c;
                        assert(i + 1 < txt.size());
                        i++;
                        Char lowSurrogate = txt.at(i);
                        insertText(String::fromUcs4(Char::surrogateToUcs4(highSurrogate, lowSurrogate)));
                    } else if (c == '\r') {
                        continue;
                    } else if (c == '\n') {
                        score()->undo(new SplitText(cursor()));
                    } else {
                        insertText(String(c));
                    }
                }
            }
        } else if (state == 1) {
            if (c == '>') {
                state = 0;
                if (token == "sym") {
                    symState = true;
                    sym.clear();
                } else if (token == "/sym") {
                    symState = false;
                    cursor()->setFormat(format);
                    insertSym(SymNames::symIdByName(sym));
                } else {
                    prepareFormat(token, format);
                }
            } else {
                token += c;
            }
        } else if (state == 2) {
            if (c == ';') {
                state = 0;
                if (token == "lt") {
                    insertText(u"<");
                } else if (token == "gt") {
                    insertText(u">");
                } else if (token == "amp") {
                    insertText(u"&");
                } else if (token == "quot") {
                    insertText(u"\"");
                } else {
                    insertSym(SymNames::symIdByName(token));
                }
            } else if (!c.isLetter()) {
                state = 0;
                insertText(u"&");
                insertText(token);
                insertText(c);
            } else {
                token += c;
            }
        }
    }
    if (state == 2) {
        insertText(u"&");
        insertText(token);
    }
    score()->endCmd();
}

//---------------------------------------------------------
//   endHexState
//---------------------------------------------------------

void TextBase::endHexState() {
    TextCursor *cursor = this->cursor();

    if (hexState_ >= 0) {
        if (hexState_ > 0) {
            size_t c2 = cursor->column();
            size_t c1 = c2 - (hexState_ + 1);

            TextBlock &t = mutldata()->blocks[cursor->row()];
            String ss = t.remove(static_cast<int>(c1), hexState_ + 1, cursor);
            bool ok;
            char32_t code = ss.mid(1).toInt(&ok, 16);
            cursor->setColumn(c1);
            cursor->clearSelection();
            if (ok) {
                editInsertText(cursor, String::fromUcs4(code));
            } else {
                LOGD("cannot convert hex string <%s>, state %d (%zu-%zu)", muPrintable(ss.mid(1)), hexState_, c1, c2);
            }
        }
        hexState_ = -1;
    }
}

//---------------------------------------------------------
//   deleteSelectedText
//---------------------------------------------------------

bool TextBase::deleteSelectedText() {
    TextCursor *cursor = this->cursor();

    if (!cursor->hasSelection()) {
        return false;
    }

    size_t r1 = cursor->selectLine();
    size_t c1 = cursor->selectColumn();

    if (r1 > cursor->row() || (r1 == cursor->row() && c1 > cursor->column())) {
        // swap start end of selection
        r1 = cursor->row();
        c1 = cursor->column();
        cursor->setRow(cursor_->selectLine());
        cursor->setColumn(cursor->selectColumn());
    }

    cursor->clearSelection();
    for (;;) {
        if (r1 == cursor->row() && c1 == cursor->column()) {
            break;
        }
        if (cursor->column() == 0 && cursor->row() != 0) {
            score()->undo(new JoinText(cursor));
        } else {
            // move cursor left:
            if (!cursor_->movePosition(TextCursor::MoveOperation::Left)) {
                break;
            }
            TextCursor undoCursor(*cursor_);
            score()->undo(new RemoveText(&undoCursor, String(cursor_->currentCharacter())));
        }
    }
    return true;
}

//---------------------------------------------------------
//   ChangeTextProperties
//---------------------------------------------------------

void ChangeTextProperties::restoreSelection() {
    TextCursor &tc = cursor();
    tc.text()->cursor()->setSelectLine(tc.selectLine());
    tc.text()->cursor()->setSelectColumn(tc.selectColumn());
    tc.text()->cursor()->setRow(tc.row());
    tc.text()->cursor()->setColumn(tc.column());
}

ChangeTextProperties::ChangeTextProperties(const TextCursor *tc, Pid propId, const PropertyValue &propVal, PropertyFlags flags)
    : TextEditUndoableCommand(*tc) {
    propertyId_ = propId;
    propertyVal_ = propVal;
    flags_ = flags_;
    if (propertyId_ == Pid::FONT_STYLE) {
        existingStyle_ = static_cast<FontStyle>(cursor().text()->getProperty(propId).toInt());
    }
}

void ChangeTextProperties::undo() {
    cursor().text()->resetFormatting();
    cursor().text()->setXmlText(xmlText_);
    restoreSelection();
    cursor().text()->renderer()->layoutText1(cursor().text());
}

void ChangeTextProperties::redo() {
    xmlText_ = cursor().text()->xmlText();
    restoreSelection();
    cursor().text()->setPropertyFlags(propertyId_, flags_);

    if (propertyId_ == Pid::FONT_STYLE) {
        FontStyle setStyle = static_cast<FontStyle>(propertyVal_.toInt());
        TextCursor *tc = cursor().text()->cursor();
        // user turned on bold/italic/underline/strike for text where it's not set, or turned it off for text where it is set,
        // note this logic only works because the user can only click one at a time
        if ((setStyle & FontStyle::Bold) != (existingStyle_ & FontStyle::Bold)) {
            tc->setFormat(FormatId::Bold, setStyle & FontStyle::Bold);
        }
        if ((setStyle & FontStyle::Italic) != (existingStyle_ & FontStyle::Italic)) {
            tc->setFormat(FormatId::Italic, setStyle & FontStyle::Italic);
        }
        if ((setStyle & FontStyle::Underline) != (existingStyle_ & FontStyle::Underline)) {
            tc->setFormat(FormatId::Underline, setStyle & FontStyle::Underline);
        }
        if ((setStyle & FontStyle::Strike) != (existingStyle_ & FontStyle::Strike)) {
            tc->setFormat(FormatId::Strike, setStyle & FontStyle::Strike);
        }
    } else {
        cursor().text()->setProperty(propertyId_, propertyVal_);
    }
}
