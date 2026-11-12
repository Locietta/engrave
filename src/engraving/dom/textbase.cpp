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

#include <cmath>
#include <stack>

#include "../editing/textedit.h"
#include "anchors.h"
#include "box.h"
#include "draw/fontmetrics.h"
#include "engraving/internal/engravingfont.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "harppedaldiagram.h"
#include "instrumentname.h"
#include "log.h"
#include "measure.h"
#include "mscore.h"
#include "page.h"
#include "rendering/score/textlayout.h"
#include "rw/xmlreader.h"
#include "rw/xmlwriter.h"
#include "score.h"
#include "style/textstyle.h"
#include "types/symnames.h"
#include "types/typesconv.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

namespace mu::engraving {
static const char *fallback_symbol_font = "Bravura";
static const char *fallback_symboltext_font = "Bravura Text";

//---------------------------------------------------------
//   isSorted
/// return true if (r1,c1) is at or before (r2,c2)
//---------------------------------------------------------

bool TextBase::is_sorted(size_t r1, size_t c1, size_t r2, size_t c2) {
    if (r1 < r2) {
        return true;
    }

    if ((r1 == r2) && (c1 <= c2)) {
        return true;
    }

    return false;
}

//---------------------------------------------------------
//   swap
/// swap (r1,c1) with (r2,c2)
//---------------------------------------------------------

void TextBase::swap(size_t &r1, size_t &c1, size_t &r2, size_t &c2) {
    std::swap(r1, r2);
    std::swap(c1, c2);
}

//---------------------------------------------------------
//   sort
/// swap (r1,c1) with (r2,c2) if they are not sorted
//---------------------------------------------------------

void TextBase::sort(size_t &r1, size_t &c1, size_t &r2, size_t &c2) {
    if (!is_sorted(r1, c1, r2, c2)) {
        swap(r1, c1, r2, c2);
    }
}

const String TextBase::undefined_font_family = String(u"Undefined");
const double TextBase::undefined_font_size = -1.0;

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool CharFormat::operator==(const CharFormat &cf) const {
    return cf.style() == style() && cf.valign() == valign() && cf.font_size() == font_size() && cf.font_family() == font_family();
}

//---------------------------------------------------------
//   operator=
//---------------------------------------------------------

CharFormat &CharFormat::operator=(const CharFormat &cf) {
    set_style(cf.style());
    set_valign(cf.valign());
    set_font_size(cf.font_size());
    set_font_family(cf.font_family());

    return *this;
}

//---------------------------------------------------------
//   clearSelection
//---------------------------------------------------------

void TextCursor::clear_selection() {
    select_line_ = row_;
    select_column_ = column_;
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void TextCursor::init() {
    PropertyValue family = text_->property_default(Pid::FONT_FACE);
    format_.set_font_family(family.value<String>());

    PropertyValue size = text_->property_default(Pid::FONT_SIZE);
    format_.set_font_size(size.to_real());

    PropertyValue style = text_->property_default(Pid::FONT_STYLE);
    format_.set_style(static_cast<FontStyle>(style.to_int()));

    PropertyValue vertical_align = text_->property_default(Pid::TEXT_SCRIPT_ALIGN);
    format_.set_valign(static_cast<VerticalAlignment>(vertical_align.to_int()));
}

std::pair<size_t, size_t> TextCursor::position_to_local_coord(int position) const {
    const TextBase::LayoutData *ldata = text_->ldata();
    IF_ASSERT_FAILED(ldata) { return {muse::nidx, muse::nidx}; }

    int current_position = 0;
    for (size_t i = 0; i < ldata->blocks.size(); ++i) {
        const TextBlock &t = ldata->blocks.at(i);
        for (size_t j = 0; j < t.columns(); ++j) {
            if (current_position == position) {
                return {i, j};
            }

            current_position++;
        }
    }

    return {muse::nidx, muse::nidx};
}

int TextCursor::current_position() const { return position(static_cast<int>(row()), static_cast<int>(column())); }

TextCursor::Range TextCursor::selection_range() const {
    int cursor_position = current_position();
    int selection_position = position(static_cast<int>(select_line()), static_cast<int>(select_column()));

    if (cursor_position > selection_position) {
        return range(selection_position, cursor_position);
    } else {
        return range(cursor_position, selection_position);
    }
}

//---------------------------------------------------------
//   columns
//---------------------------------------------------------

size_t TextCursor::columns() const { return text_->ldata()->text_block(static_cast<int>(row_)).columns(); }

//---------------------------------------------------------
//   currentCharacter
//---------------------------------------------------------

Char TextCursor::current_character() const {
    const TextBase::LayoutData *ldata = text_->ldata();
    IF_ASSERT_FAILED(ldata) { return Char(); }

    const TextBlock &t = ldata->blocks.at(row());
    String s = t.text(static_cast<int>(column()), 1, text_);
    s = TextBase::un_escape(s);
    if (s.is_empty()) {
        return Char();
    }
    return s.at(0);
}

//---------------------------------------------------------
//   updateCursorFormat
//---------------------------------------------------------

void TextCursor::update_cursor_format() {
    const TextBase::LayoutData *ldata = text_->ldata();
    IF_ASSERT_FAILED(ldata) { return; }
    const TextBlock &block = ldata->blocks.at(row_);

    size_t col = has_selection() ? select_column() : column();
    // Get format at the LEFT of the cursor position
    const CharFormat *format = block.format_at(std::max(static_cast<int>(col) - 1, 0));
    if (!format) {
        init();
    } else {
        set_format(*format);
    }
}

//---------------------------------------------------------
//   cursorRect
//---------------------------------------------------------

RectF TextCursor::cursor_rect() const {
    const TextBlock &tline = cur_line();
    const TextFragment *fragment = tline.fragment(static_cast<int>(column()));

    Font font_value = fragment ? fragment->font(text_) : text_->font();
    if (fragment) {
        // Ensure the cursor height matches that of the associated text font
        TextLayout::substitute_music_font(font_value, fragment->calculated_font_size(text_));
    }
    const FontMetrics fm(font_value);

    double font_cap_height = fm.cap_height();
    double font_ascent = fm.ascent();

    // Bravura Text returns small values for its cap heights
    double cursor_cap_height =
        font_cap_height > 0 && font_value.family().id() != u"Bravura Text" ? std::min(font_cap_height, font_ascent) : font_ascent;
    double cursor_descent = cursor_cap_height * .3;

    double h = cursor_cap_height + 2 * cursor_descent; // symmetrical cursor
    double w = std::max(2.0, cursor_cap_height / 16.0);
    double x = tline.xpos(column(), text_);
    double y = tline.y() + cursor_descent - h;
    return RectF(x - w / 2, y, w, h);
}

RectF TextCursor::cursor_canvas_rect() const { return cursor_rect().translated(text_->canvas_pos()); }

//---------------------------------------------------------
//   curLine
//    return the current text line in edit mode
//---------------------------------------------------------

const TextBlock &TextCursor::cur_line() const {
    const TextBase::LayoutData *ldata = text_->ldata();
    IF_ASSERT_FAILED(ldata) {
        static TextBlock dummy;
        return dummy;
    }

    return ldata->blocks.at(row_);
}

TextBlock &TextCursor::cur_line() {
    TextBase::LayoutData *ldata = text_->mutldata();
    return ldata->blocks[row_];
}

//---------------------------------------------------------
//   changeSelectionFormat
//---------------------------------------------------------

void TextCursor::change_selection_format(FormatId id, const FormatValue &val) {
    TextBase::LayoutData *ldata = text_->mutldata();

    size_t r1 = select_line();
    size_t r2 = row();
    size_t c1 = select_column();
    size_t c2 = column();

    TextBase::sort(r1, c1, r2, c2);

    for (size_t row = 0; row < ldata->blocks.size(); ++row) {
        TextBlock &t = ldata->blocks[row];
        if (row < r1) {
            continue;
        }
        if (row > r2) {
            break;
        }
        if (row == r1 && r1 == r2) {
            t.change_format(id, val, static_cast<int>(c1), static_cast<int>(c2 - c1));
        } else if (row == r1) {
            t.change_format(id, val, static_cast<int>(c1), static_cast<int>(t.columns() - c1));
        } else if (row == r2) {
            t.change_format(id, val, 0, static_cast<int>(c2));
        } else {
            t.change_format(id, val, 0, static_cast<int>(t.columns()));
        }
    }

    text_->renderer()->layout_text1(text_);
}

const CharFormat TextCursor::selected_fragments_format() const {
    if (!text_ || (!has_selection() && editing())) {
        return format_;
    }

    // Formatting belongs to logical source runs, not measured/wrapped lines.
    TextBase::LayoutData source;
    const TextBase::LayoutData *ldata = text_->ldata();
    if (!editing()) {
        text_->create_blocks(&source, true);
        ldata = &source;
    }
    const bool selected = editing() && has_selection();
    IF_ASSERT_FAILED(ldata) { return CharFormat(); }

    if (ldata->blocks.empty()) {
        return format_;
    }

    size_t start_row = selected ? select_line_ : 0;
    size_t end_row = selected ? row_ : ldata->blocks.size() - 1;
    size_t selection_start_col = selected ? select_column_ : 0;
    size_t selection_end_col = selected ? column_ : 1; // corrected below at `endColumn

    TextBase::sort(start_row, selection_start_col, end_row, selection_end_col);

    const TextFragment *tf = ldata->text_block(static_cast<int>(start_row)).fragment(static_cast<int>(selection_start_col));
    CharFormat result_format = tf ? tf->format : CharFormat();

    bool all_blocks_empty = true;
    for (size_t row = start_row; row <= end_row; ++row) {
        const TextBlock &block = ldata->blocks.at(row);

        if (block.fragments().empty()) {
            continue;
        }
        all_blocks_empty = false;

        const size_t start_column = (row == start_row) ? selection_start_col : 0;
        const size_t end_column = (row == end_row && selected) ? selection_end_col : block.columns();

        size_t column = 0;

        bool is_single_fragment = block.fragments().size() == 1;

        for (const TextFragment &fragment : block.fragments()) {
            const size_t frag_cols = fragment.columns();

            if (!is_single_fragment) {
                if (column + frag_cols <= start_column) {
                    column += frag_cols;
                    continue;
                }
                if (column > 0 && column >= end_column) {
                    break;
                }
            }

            // proper bitwise 'and' to ensure Bold/Italic/Underline/Strike only true if true for all fragments
            result_format.set_style(
                static_cast<FontStyle>(static_cast<int>(result_format.style()) & static_cast<int>(fragment.format.style())));

            if (result_format.font_family() == "ScoreText") {
                result_format.set_font_family(fragment.format.font_family());
            }
            if (fragment.format.font_family() != "ScoreText" && result_format.font_family() != fragment.format.font_family()) {
                result_format.set_font_family(TextBase::undefined_font_family);
            }

            if (result_format.font_size() != fragment.format.font_size()) {
                result_format.set_font_size(TextBase::undefined_font_size);
            }

            if (result_format.valign() != fragment.format.valign()) {
                result_format.set_valign(VerticalAlignment::ALIGN_UNDEFINED);
            }

            column += frag_cols;
        }
    }

    if (!all_blocks_empty) {
        return result_format;
    }

    CharFormat default_format;
    default_format.set_style(FontStyle(text_->property_default(Pid::FONT_STYLE).value<int>()));
    default_format.set_font_family(text_->property_default(Pid::FONT_FACE).value<String>());
    default_format.set_font_size(text_->property_default(Pid::FONT_SIZE).to_double());
    default_format.set_valign(VerticalAlignment(text_->property_default(Pid::TEXT_SCRIPT_ALIGN).to_int()));

    return default_format;
}

//---------------------------------------------------------
//   PointF
//---------------------------------------------------------

void TextCursor::set_format(FormatId id, FormatValue val) {
    if (!has_selection()) {
        if (!editing()) {
            text_->select_all();
        } else if (format()->format_value(id) == val) {
            return;
        }
    }
    format()->set_format_value(id, val);
    if (has_selection()) {
        change_selection_format(id, val);
        text()->set_text_invalid();
    }
    if (!editing()) {
        clear_selection();
    }
}

//---------------------------------------------------------
//   movePosition
//---------------------------------------------------------

bool TextCursor::move_position(TextCursor::MoveOperation op, TextCursor::MoveMode mode, int count) {
    for (int i = 0; i < count; i++) {
        switch (op) {
            case TextCursor::MoveOperation::LEFT:
                if (has_selection() && mode == TextCursor::MoveMode::MOVE_ANCHOR) {
                    size_t r1 = select_line_;
                    size_t r2 = row_;
                    size_t c1 = select_column_;
                    size_t c2 = column_;

                    TextBase::sort(r1, c1, r2, c2);
                    clear_selection();
                    row_ = r1;
                    column_ = c1;
                } else if (column_ == 0) {
                    if (row_ == 0) {
                        return false;
                    }
                    --row_;
                    column_ = cur_line().columns();
                } else {
                    --column_;
                }
                break;

            case TextCursor::MoveOperation::RIGHT:
                if (has_selection() && mode == TextCursor::MoveMode::MOVE_ANCHOR) {
                    size_t r1 = select_line_;
                    size_t r2 = row_;
                    size_t c1 = select_column_;
                    size_t c2 = column_;

                    TextBase::sort(r1, c1, r2, c2);
                    clear_selection();
                    row_ = r2;
                    column_ = c2;
                } else if (column() >= cur_line().columns()) {
                    if (row_ >= text_->ldata()->rows() - 1) {
                        return false;
                    }
                    ++row_;
                    column_ = 0;
                } else {
                    ++column_;
                }
                break;

            case TextCursor::MoveOperation::UP:
                if (row_ == 0) {
                    return false;
                }
                --row_;
                if (column_ > cur_line().columns()) {
                    column_ = cur_line().columns();
                }

                break;

            case TextCursor::MoveOperation::DOWN:
                if (row_ >= text_->ldata()->rows() - 1) {
                    return false;
                }
                ++row_;
                if (column_ > cur_line().columns()) {
                    column_ = cur_line().columns();
                }

                break;

            case TextCursor::MoveOperation::START:
                row_ = 0;
                column_ = 0;

                break;

            case TextCursor::MoveOperation::END:
                row_ = text_->ldata()->rows() - 1;
                column_ = cur_line().columns();

                break;

            case TextCursor::MoveOperation::START_OF_LINE: column_ = 0; break;

            case TextCursor::MoveOperation::END_OF_LINE: column_ = cur_line().columns(); break;

            case TextCursor::MoveOperation::WORD_LEFT:
                if (column_ > 0) {
                    --column_;
                    while (column_ > 0 && current_character().is_space()) {
                        --column_;
                    }
                    while (column_ > 0 && !current_character().is_space()) {
                        --column_;
                    }
                    if (current_character().is_space()) {
                        ++column_;
                    }
                }
                break;

            case TextCursor::MoveOperation::NEXT_WORD: {
                size_t cols = columns();
                if (column_ < cols) {
                    ++column_;
                    while (column_ < cols && !current_character().is_space()) {
                        ++column_;
                    }
                    while (column_ < cols && current_character().is_space()) {
                        ++column_;
                    }
                }
            } break;

            default: LOGD("Text::movePosition: not implemented"); return false;
        }
        if (mode == TextCursor::MoveMode::MOVE_ANCHOR) {
            clear_selection();
        }
    }

    update_cursor_format();
    text_->score()->add_refresh(text_->canvas_bounding_rect());

    return true;
}

//---------------------------------------------------------
//   doubleClickSelect
//---------------------------------------------------------

void TextCursor::select_word() {
    clear_selection();

    // if clicked on a space, select surrounding spaces
    // otherwise select surround non-spaces
    const bool select_spaces = current_character().is_space();

    //handle double-clicking inside a word
    size_t start_position = column_;

    while (column_ > 0 && current_character().is_space() == select_spaces) {
        --column_;
    }

    if (current_character().is_space() != select_spaces) {
        ++column_;
    }

    select_column_ = column_;

    column_ = start_position;
    while (column_ < cur_line().columns() && current_character().is_space() == select_spaces) {
        ++column_;
    }

    update_cursor_format();
    text_->score()->add_refresh(text_->canvas_bounding_rect());
}

//---------------------------------------------------------
//   set
//---------------------------------------------------------

bool TextCursor::set(const PointF &p, TextCursor::MoveMode mode) {
    PointF pt = p - text_->canvas_pos();
    if (!text_->ldata()->bbox().contains(pt)) {
        return false;
    }

    //      if (_text->_layout.empty())
    //            _text->_layout.append(TextBlock());
    row_ = 0;

    const TextBase::LayoutData *ldata = text_->ldata();
    IF_ASSERT_FAILED(ldata) { return false; }

    for (size_t row = 0; row < ldata->blocks.size(); ++row) {
        const TextBlock &l = ldata->blocks.at(row);
        if (l.y() > pt.y()) {
            row_ = row;
            break;
        }
    }
    column_ = cur_line().column(pt.x(), text_);

    text_->score()->set_update_all();
    if (mode == TextCursor::MoveMode::MOVE_ANCHOR) {
        clear_selection();
    }
    update_cursor_format();

    return true;
}

//---------------------------------------------------------
//   selectedText
//    return current selection
//---------------------------------------------------------

String TextCursor::selected_text(bool with_format) const {
    size_t r1 = select_line();
    size_t r2 = row_;
    size_t c1 = select_column();
    size_t c2 = column();
    TextBase::sort(r1, c1, r2, c2);
    return extract_text(static_cast<int>(r1), static_cast<int>(c1), static_cast<int>(r2), static_cast<int>(c2), with_format);
}

//---------------------------------------------------------
//   extractText
//    return text between (r1,c1) and (r2,c2).
//---------------------------------------------------------

String TextCursor::extract_text(int r1, int c1, int r2, int c2, bool with_format) const {
    const TextBase::LayoutData *ldata = text_->ldata();
    IF_ASSERT_FAILED(ldata) { return String(); }

    assert(TextBase::is_sorted(r1, c1, r2, c2));
    const std::vector<TextBlock> &tb = ldata->blocks;

    if (r1 == r2) {
        return tb.at(r1).text(c1, c2 - c1, text_, with_format);
    }

    String str = tb.at(r1).text(c1, -1, text_, with_format) + u"\n";

    for (int r = r1 + 1; r < r2; ++r) {
        str += tb.at(r).text(0, -1, text_, with_format) + u"\n";
    }

    str += tb.at(r2).text(0, c2, text_, with_format);
    return str;
}

TextCursor::Range TextCursor::range(int start, int end) const {
    const TextBase::LayoutData *ldata = text_->ldata();
    IF_ASSERT_FAILED(ldata) { return Range(); }

    String result;
    int pos = 0;
    for (size_t i = 0; i < ldata->blocks.size(); ++i) {
        const TextBlock &t = ldata->blocks.at(i);

        for (size_t j = 0; j < t.columns(); ++j) {
            if (pos > end) {
                return {start, end, result};
            }

            if (start < pos) {
                result += t.text(static_cast<int>(j), 1, text_);
            }

            pos++;
        }
    }

    return {start, end, result};
}

int TextCursor::position(int row, int column) const {
    const TextBase::LayoutData *ldata = text_->ldata();
    IF_ASSERT_FAILED(ldata) { return 0; }

    int result = 0;

    for (int i = 0; i < row; ++i) {
        const TextBlock &t = ldata->blocks.at(i);
        result += static_cast<int>(t.columns());
    }

    result += column;

    return result;
}

//---------------------------------------------------------
//   TextFragment
//---------------------------------------------------------
TextFragment::TextFragment(const String &s) { text = s; }

TextFragment::TextFragment(TextCursor *cursor, const String &s) : TextFragment(s) { format = *cursor->format(); }

TextFragment::TextFragment(const TextFragment &f) {
    text = f.text;
    format = f.format;
    pos = f.pos;
}

TextFragment &TextFragment::operator=(const TextFragment &f) {
    text = f.text;
    format = f.format;
    pos = f.pos;
    return *this;
}

//---------------------------------------------------------
//   split
//---------------------------------------------------------

TextFragment TextFragment::split(int column) {
    size_t idx = 0;
    int col = 0;
    TextFragment f;
    f.format = format;

    for (size_t i = 0; i < text.size(); ++i) {
        const Char &c = text.at(i);
        if (col == column) {
            if (idx) {
                if (idx < text.size()) {
                    f.text = text.mid(idx);
                    text = text.left(idx);
                }
            }
            return f;
        }
        ++idx;
        if (c.is_high_surrogate()) {
            continue;
        }
        ++col;
    }
    return f;
}

//---------------------------------------------------------
//   columns
//---------------------------------------------------------

int TextFragment::columns() const {
    int col = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text.at(i).is_high_surrogate()) {
            continue;
        }
        ++col;
    }
    return col;
}

//---------------------------------------------------------
//   operator ==
//---------------------------------------------------------

bool TextFragment::operator==(const TextFragment &f) const { return format == f.format && text == f.text; }

//---------------------------------------------------------
//   font
//---------------------------------------------------------

Font TextFragment::font(const TextBase *t) const {
    Font font;

    double m = calculated_font_size(t);

    String family;
    Font::Type font_type = Font::Type::UNKNOWN;
    if (format.font_family() == "ScoreText") {
        if (t->has_symbol_scale()) {
            std::string font_name =
                t->score()->engraving_fonts()->font_by_name(t->style().style_st(Sid::MUSICAL_SYMBOL_FONT).to_std_string())->family();
            family = String::from_std_string(font_name);
            font_type = Font::Type::MUSIC_SYMBOL;

            m = StyleDef::default_smufl_point_size();
            m *= t->get_property(Pid::MUSICAL_SYMBOLS_SCALE).to_double();
            if (t->size_is_spatium_dependent()) {
                m *= t->spatium_scaling();
            }

            if (t->style().style_b(Sid::DYNAMICS_OVERRIDE_FONT)) {
                std::string font_name2 =
                    t->score()->engraving_fonts()->font_by_name(t->style().style_st(Sid::DYNAMICS_FONT).to_std_string())->family();
                family = String::from_std_string(font_name2);
            }

            m *= t->mag();
        } else if (t->has_symbol_size()) {
            family = t->style().style_st(Sid::MUSICAL_TEXT_FONT);
            font_type = Font::Type::MUSIC_SYMBOL_TEXT;
            m = t->get_property(Pid::MUSIC_SYMBOL_SIZE).to_double();
            if (t->size_is_spatium_dependent()) {
                m *= t->spatium_scaling();
            }
            m *= t->mag();
        }
        // check if all symbols are available
        font.set_family(family, font_type);
        font.set_no_font_merging(true);
        FontMetrics fm(font);

        resolve_fallback(font_type, fm, family);
    } else {
        family = format.font_family();
        font_type = Font::Type::UNKNOWN;
        font.set_bold(format.bold());
        font.set_italic(format.italic());
        font.set_underline(format.underline());
        font.set_strike(format.strike());
    }

    font.set_family(family, font_type);
    assert(m > 0.0);

    font.set_point_size_f(m);
    return font;
}

//---------------------------------------------------------
//   calculatedFontSize
//---------------------------------------------------------

double TextFragment::calculated_font_size(const TextBase *t) const {
    double size = format.font_size();

    if (t->size_is_spatium_dependent()) {
        size *= t->spatium_scaling();
    }
    if (format.valign() != VerticalAlignment::ALIGN_NORMAL) {
        size *= ::mu::engraving::subscript_size;
    }

    assert(size > 0.0);
    return size * t->mag();
}

void TextFragment::resolve_fallback(muse::draw::Font::Type font_type, const muse::draw::FontMetrics &fm, String &family) const {
    std::vector<char32_t> missing_chars;
    for (size_t i = 0; i < text.size(); ++i) {
        const Char &c = text.at(i);
        if (c.is_high_surrogate()) {
            if (i + 1 == text.size()) {
                ASSERT_X("bad string");
            }
            const Char &c2 = text.at(i + 1);
            ++i;
            char32_t v = Char::surrogate_to_ucs4(c, c2);
            if (!fm.in_font(v)) {
                missing_chars.push_back(v);
            }
        } else {
            if (!fm.in_font(c.unicode())) {
                missing_chars.push_back(c.unicode());
            }
        }
    }

    static String fallback_symbol_font_family = String::from_utf8(fallback_symbol_font);
    static String fallback_symbol_text_font_family = String::from_utf8(fallback_symboltext_font);
    static FontMetrics fallback_symbol_fm(Font(fallback_symbol_font_family, Font::Type::MUSIC_SYMBOL));
    static FontMetrics fallback_symbol_text_fm(Font(fallback_symbol_text_font_family, Font::Type::MUSIC_SYMBOL_TEXT));

    if (font_type == Font::Type::MUSIC_SYMBOL) {
        for (char32_t missing_char : missing_chars) {
            if (fallback_symbol_fm.in_font(missing_char)) {
                family = fallback_symbol_font_family;
                return;
            }
        }
    } else {
        for (char32_t missing_char : missing_chars) {
            if (fallback_symbol_text_fm.in_font(missing_char)) {
                family = fallback_symbol_text_font_family;
                return;
            }
        }
    }
}

//---------------------------------------------------------

//   fragmentsWithoutEmpty
//---------------------------------------------------------

std::list<TextFragment> TextBlock::fragments_without_empty() {
    std::list<TextFragment> list;
    for (const auto &x : fragments_) {
        if (!x.text.is_empty()) {
            list.push_back(x);
        }
    }

    return list;
}

//---------------------------------------------------------
//   xpos
//---------------------------------------------------------

double TextBlock::xpos(size_t column, const TextBase *t) const {
    size_t col = 0;
    for (const TextFragment &f : fragments_) {
        if (column == col) {
            return f.pos.x();
        }
        FontMetrics fm(f.font(t));
        size_t idx = 0;
        for (size_t i = 0; i < f.text.size(); ++i) {
            ++idx;
            if (f.text.at(i).is_high_surrogate()) {
                continue;
            }
            ++col;
            if (column == col) {
                return f.pos.x() + fm.width(f.text.left(idx));
            }
        }
    }
    return shape_.bbox().x();
}

//---------------------------------------------------------
//   fragment
//---------------------------------------------------------

const TextFragment *TextBlock::fragment(int column) const {
    if (fragments_.empty()) {
        return nullptr;
    }
    int col = 0;
    auto f = fragments_.begin();
    for (; f != fragments_.end(); ++f) {
        for (size_t i = 0; i < f->text.size(); ++i) {
            if (f->text.at(i).is_high_surrogate()) {
                continue;
            }
            if (column == col) {
                return &*f;
            }
            ++col;
        }
    }
    if (column == col) {
        return &*(std::prev(f));
    }
    return 0;
}

//---------------------------------------------------------
//   formatAt
//---------------------------------------------------------

const CharFormat *TextBlock::format_at(int column) const {
    const TextFragment *f = fragment(column);
    if (f) {
        return &(f->format);
    }
    return 0;
}

//---------------------------------------------------------
//   boundingRect
//---------------------------------------------------------

RectF TextBlock::bounding_rect(int col1, int col2, const TextBase *t) const {
    double x1 = xpos(col1, t);
    double x2 = xpos(col2, t);
    const RectF &bbox = shape_.bbox();
    return RectF(x1, bbox.y(), x2 - x1, bbox.height());
}

//---------------------------------------------------------
//   columns
//---------------------------------------------------------

size_t TextBlock::columns() const {
    size_t col = 0;
    for (const TextFragment &f : fragments_) {
        for (size_t i = 0; i < f.text.size(); ++i) {
            if (!f.text.at(i).is_high_surrogate()) {
                ++col;
            }
        }
    }
    return col;
}

//---------------------------------------------------------
//   column
//    Return nearest column for position x. X is in
//    Text coordinate system
//---------------------------------------------------------

int TextBlock::column(double x, TextBase *t) const {
    int col = 0;
    for (const TextFragment &f : fragments_) {
        int idx = 0;
        if (x <= f.pos.x()) {
            return col;
        }
        double px = 0.0;
        for (size_t i = 0; i < f.text.size(); ++i) {
            ++idx;
            if (f.text.at(i).is_high_surrogate()) {
                continue;
            }
            FontMetrics fm(f.font(t));
            double xo = fm.width(f.text.left(idx));
            if (x <= f.pos.x() + px + (xo - px) * .5) {
                return col;
            }
            ++col;
            px = xo;
        }
    }
    return static_cast<int>(this->columns());
}

//---------------------------------------------------------
//   insert
//---------------------------------------------------------

void TextBlock::insert(TextCursor *cursor, const String &s) {
    int rcol, ridx;
    remove_empty_fragment(); // since we are going to write text, we don't need an empty fragment to hold format info. if such exists, delete it
    auto i = fragment(static_cast<int>(cursor->column()), &rcol, &ridx);
    if (i != fragments_.end()) {
        if (!(i->format == *cursor->format())) {
            if (rcol == 0) {
                fragments_.insert(i, TextFragment(cursor, s));
            } else {
                TextFragment f2 = i->split(rcol);
                i = fragments_.insert(std::next(i), TextFragment(cursor, s));
                fragments_.insert(std::next(i), f2);
            }
        } else {
            i->text.insert(ridx, s);
        }
    } else {
        if (!fragments_.empty() && fragments_.back().format == *cursor->format()) {
            fragments_.back().text.append(s);
        } else {
            fragments_.push_back(TextFragment(cursor, s));
        }
    }
}

//---------------------------------------------------------
//
//   insertEmptyFragmentIfNeeded
//   used to insert an empty TextFragment in TextBlocks that have none
//   that way, the formatting information (most importantly the font size) of the line is preserved
//
//---------------------------------------------------------

void TextBlock::insert_empty_fragment_if_needed(TextCursor *cursor) {
    if (fragments_.size() == 0 || fragments_.front().text.is_empty()) {
        fragments_.insert(fragments_.begin(), TextFragment(cursor, u""));
    }
}

//---------------------------------------------------------
//   removeEmptyFragment
//---------------------------------------------------------

void TextBlock::remove_empty_fragment() {
    if (fragments_.size() > 0 && fragments_.front().text.is_empty()) {
        fragments_.pop_back();
    }
}

//---------------------------------------------------------
//   fragment
//    inputs:
//      column is the column relative to the start of the TextBlock.
//    outputs:
//      rcol will be the column relative to the start of the TextFragment that the input column is in.
//      ridx will be the QChar index into TextFragment's text String relative to the start of that TextFragment.
//
//---------------------------------------------------------

std::list<TextFragment>::iterator TextBlock::fragment(int column, int *rcol, int *ridx) {
    int col = 0;
    for (auto it = fragments_.begin(); it != fragments_.end(); ++it) {
        *rcol = 0;
        *ridx = 0;
        for (size_t i = 0; i < it->text.size(); ++i) {
            if (col == column) {
                return it;
            }
            ++*ridx;
            if (it->text.at(i).is_high_surrogate()) {
                continue;
            }
            ++col;
            ++*rcol;
        }
    }
    return fragments_.end();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

String TextBlock::remove(int column, TextCursor *cursor) {
    int col = 0;
    String s;
    for (auto it = fragments_.begin(); it != fragments_.end(); ++it) {
        size_t idx = 0;

        for (size_t i = 0; i < it->text.size(); ++i) {
            if (col == column) {
                if (it->text.at(i).is_surrogate()) {
                    s = it->text.mid(idx, 2);
                    it->text.remove(idx, 2);
                } else {
                    s = it->text.mid(idx, 1);
                    it->text.remove(idx, 1);
                }
                if (it->text.is_empty()) {
                    fragments_.erase(it);
                }
                simplify();
                insert_empty_fragment_if_needed(cursor); // without this, cursorRect can't calculate the y position of the cursor correctly
                return s;
            }
            ++idx;
            if (it->text.at(i).is_high_surrogate()) {
                continue;
            }
            ++col;
        }
    }
    insert_empty_fragment_if_needed(cursor); // without this, cursorRect can't calculate the y position of the cursor correctly
    return s;
    //      LOGD("TextBlock::remove: column %d not found", column);
}

//---------------------------------------------------------
//   simplify
//---------------------------------------------------------

void TextBlock::simplify() {
    if (fragments_.size() < 2) {
        return;
    }
    auto i = fragments_.begin();
    TextFragment *f = &*i;
    ++i;
    for (; i != fragments_.end(); ++i) {
        while (i != fragments_.end() && (i->format == f->format)) {
            f->text.append(i->text);
            i = fragments_.erase(i);
        }
        if (i == fragments_.end()) {
            break;
        }
        f = &*i;
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

String TextBlock::remove(int start, int n, TextCursor *cursor) {
    if (n == 0) {
        return String();
    }
    int col = 0;
    String s;
    for (auto i = fragments_.begin(); i != fragments_.end();) {
        bool inc = true;
        for (size_t idx = 0; idx < i->text.size();) {
            Char c = i->text.at(idx);
            if (col == start) {
                if (c.is_high_surrogate()) {
                    s += c;
                    i->text.remove(idx, 1);
                    c = i->text.at(idx);
                }
                s += c;
                i->text.remove(idx, 1);
                if (i->text.is_empty() && (fragments_.size() > 1)) {
                    i = fragments_.erase(i);
                    inc = false;
                }
                --n;
                if (n == 0) {
                    insert_empty_fragment_if_needed(
                        cursor); // without this, cursorRect can't calculate the y position of the cursor correctly
                    return s;
                }
                continue;
            }
            ++idx;
            if (c.is_high_surrogate()) {
                continue;
            }
            ++col;
        }
        if (inc) {
            ++i;
        }
    }
    insert_empty_fragment_if_needed(cursor); // without this, cursorRect can't calculate the y position of the cursor correctly
    return s;
}

//---------------------------------------------------------
//   changeFormat
//---------------------------------------------------------

void TextBlock::change_format(FormatId id, const FormatValue &data, int start, int n) {
    int col = 0;
    for (auto i = fragments_.begin(); i != fragments_.end(); ++i) {
        int columns = i->columns();
        if (start + n <= col) {
            if (columns == 0) {
                // still apply the format change. Otherwise we have deviating formats for e. g. empty lines
                // otherwise we have Issue #19571
                i->change_format(id, data);
            }
            break;
        }
        if (start >= col + columns) {
            col += i->columns();
            continue;
        }
        int end_col = col + columns;

        if ((start <= col) && (start < end_col) && ((start + n) < end_col)) {
            // left
            TextFragment f = i->split(start + n - col);
            i->change_format(id, data);
            i = fragments_.insert(std::next(i), f);
        } else if (start > col && ((start + n) < end_col)) {
            // middle
            TextFragment lf = i->split(start + n - col);
            TextFragment mf = i->split(start - col);
            mf.change_format(id, data);
            i = fragments_.insert(std::next(i), mf);
            i = fragments_.insert(std::next(i), lf);
        } else if (start > col) {
            // right
            TextFragment f = i->split(start - col);
            f.change_format(id, data);
            i = fragments_.insert(std::next(i), f);
        } else {
            if (id == FormatId::FONT_FAMILY && i->format.font_family() == "ScoreText") {
                void(0); // do nothing, we need to leave that as is
            } else {
                // complete fragment
                i->change_format(id, data);
            }
        }
        col = end_col;
    }
}

//---------------------------------------------------------
//   formatValue
//---------------------------------------------------------
FormatValue CharFormat::format_value(FormatId id) const {
    switch (id) {
        case FormatId::BOLD: return bold();
        case FormatId::ITALIC: return italic();
        case FormatId::UNDERLINE: return underline();
        case FormatId::STRIKE: return strike();
        case FormatId::VALIGN: return static_cast<int>(valign());
        case FormatId::FONT_SIZE: return font_size();
        case FormatId::FONT_FAMILY: return font_family();
    }

    return FormatValue();
}

//---------------------------------------------------------
//   setFormatValue
//---------------------------------------------------------

void CharFormat::set_format_value(FormatId id, const FormatValue &val) {
    switch (id) {
        case FormatId::BOLD: set_bold(std::get<bool>(val)); break;
        case FormatId::ITALIC: set_italic(std::get<bool>(val)); break;
        case FormatId::UNDERLINE: set_underline(std::get<bool>(val)); break;
        case FormatId::STRIKE: set_strike(std::get<bool>(val)); break;
        case FormatId::VALIGN: valign_ = static_cast<VerticalAlignment>(std::get<int>(val)); break;
        case FormatId::FONT_SIZE: font_size_ = std::get<double>(val); break;
        case FormatId::FONT_FAMILY: font_family_ = std::get<String>(val); break;
    }
}

//---------------------------------------------------------
//   changeFormat
//---------------------------------------------------------

void TextFragment::change_format(FormatId id, const FormatValue &data) { format.set_format_value(id, data); }

//---------------------------------------------------------
//   split
//---------------------------------------------------------

TextBlock TextBlock::split(int column, TextCursor *cursor) {
    TextBlock tl;

    int col = 0;
    for (auto it = fragments_.begin(); it != fragments_.end(); ++it) {
        size_t idx = 0;
        for (size_t i = 0; i < it->text.size(); ++i) {
            if (col == column) {
                if (idx) {
                    if (idx < it->text.size()) {
                        TextFragment tf(it->text.mid(idx));
                        tf.format = it->format;
                        tl.fragments_.push_back(tf);
                        it->text = it->text.left(idx);
                        ++it;
                    }
                }
                for (; it != fragments_.end(); it = fragments_.erase(it)) {
                    tl.fragments_.push_back(*it);
                }

                if (fragments_.size() == 0) {
                    insert_empty_fragment_if_needed(cursor);
                }
                return tl;
            }
            ++idx;
            if (it->text.at(i).is_high_surrogate()) {
                continue;
            }
            ++col;
        }
    }

    TextFragment tf(u"");
    if (fragments_.size() > 0) {
        tf.format = fragments_.back().format;
    } else if (fragments_.size() == 0) {
        insert_empty_fragment_if_needed(cursor);
    }

    tl.fragments_.push_back(tf);
    return tl;
}

// Temporary source runs encode symbol IDs in supplementary private-use scalars.
// They are serialized back to <sym> names, never measured or published as glyphs.
static constexpr char32_t source_symbol_base = 0xF0000;
static_assert(static_cast<char32_t>(SymId::LAST_SYM) < 0xFFFE);

static String to_symbol_xml(Char c, const TextBase *t) {
    SymId sym_id = t->score()->engraving_font()->from_code(c.unicode());
    if (sym_id == SymId::NO_SYM) {
        sym_id = t->score()->engraving_fonts()->fallback_font()->from_code(c.unicode());
    }
    return u"<sym>" + String::from_ascii(SymNames::name_for_sym_id(sym_id).ascii()) + u"</sym>";
}

static String symbol_fragment_xml(const TextFragment &fragment, const TextBase *text, bool source_only) {
    String result;
    if (source_only) {
        for (size_t i = 0; i < fragment.text.size(); ++i) {
            const Char character = fragment.text.at(i);
            const char32_t code = character.is_high_surrogate() && i + 1 < fragment.text.size() ?
                                      Char::surrogate_to_ucs4(character, fragment.text.at(++i)) :
                                      character.unicode();
            if (code >= source_symbol_base && code <= source_symbol_base + static_cast<char32_t>(SymId::LAST_SYM)) {
                result += u"<sym>" + String::from_ascii(SymNames::name_for_sym_id(static_cast<SymId>(code - source_symbol_base)).ascii()) +
                          u"</sym>";
            } else {
                result += XmlWriter::xml_string(String::from_ucs4(code));
            }
        }
    } else {
        for (size_t i = 0; i < fragment.text.size(); ++i) result += to_symbol_xml(fragment.text.at(i), text);
    }
    return result;
}

//---------------------------------------------------------
//   text
//    extract text, symbols are marked with <sym>xxx</sym>
//---------------------------------------------------------

String TextBlock::text(int col1, int len, const TextBase *t, bool with_format) const {
    String s;
    int col = 0;
    double size;
    String family;
    for (const auto &f : fragments_) {
        if (f.text.is_empty()) {
            continue;
        }
        if (with_format) {
            s += TextBase::get_html_start_tag(f.format.font_size(), size, f.format.font_family(), family, f.format.style(),
                                              f.format.valign());
        }

        for (size_t i = 0; i < f.text.size(); ++i) {
            Char c = f.text.at(i);
            if (col >= col1 && (len < 0 || ((col - col1) < len))) {
                if (f.format.font_family() == "ScoreText" && with_format) {
                    s += to_symbol_xml(c, t);
                } else {
                    s += String::to_xml_escaped(c.unicode());
                }
            }
            if (!c.is_high_surrogate()) {
                ++col;
            }
        }
        if (with_format) {
            s += TextBase::get_html_end_tag(f.format.style(), f.format.valign());
        }
    }
    return s;
}

//---------------------------------------------------------
//   Text
//---------------------------------------------------------

TextBase::TextBase(const ElementType &type, EngravingObject *parent, TextStyleType tid, ElementFlags f)
    : EngravingItem(type, parent, f | ElementFlag::MOVABLE) {
    text_line_spacing_ = 1.0;
    text_style_type_ = tid;
    bg_color_ = Color::transparent;
    frame_color_ = Color::black;
    align_ = {AlignH::LEFT, AlignV::TOP};
    frame_type_ = FrameType::NO_FRAME;
    frame_width_ = 0.1_sp;
    padding_width_ = 0.2_sp;
    frame_round_ = 0_sp;

    cursor_ = new TextCursor(this);
    cursor_->init();
}

TextBase::TextBase(const ElementType &type, EngravingObject *parent, ElementFlags f) : TextBase(type, parent, TextStyleType::DEFAULT, f) {}

TextBase::TextBase(const TextBase &st) : EngravingItem(st) {
    cursor_ = new TextCursor(this);
    cursor_->set_format(*(st.cursor()->format()));
    text_ = st.text_;
    text_invalid_ = st.text_invalid_;
    layout_to_parent_width_ = st.layout_to_parent_width_;
    hex_state_ = -1;

    text_style_type_ = st.text_style_type_;
    text_line_spacing_ = st.text_line_spacing_;
    bg_color_ = st.bg_color_;
    frame_color_ = st.frame_color_;
    align_ = st.align_;
    frame_type_ = st.frame_type_;
    frame_width_ = st.frame_width_;
    padding_width_ = st.padding_width_;
    frame_round_ = st.frame_round_;
    position_ = st.position_;
    symbol_size_ = st.symbol_size_;
    symbol_scale_ = st.symbol_scale_;

    voice_assignment_ = st.voice_assignment_;
    direction_ = st.direction_;
    center_between_staves_ = st.center_between_staves_;
    anchor_to_end_of_previous_ = st.anchor_to_end_of_previous_;

    size_t n = element_style_->size() + ::mu::engraving::text_style_size;
    delete[] property_flags_list_;
    property_flags_list_ = new PropertyFlags[n];
    for (size_t i = 0; i < n; ++i) {
        property_flags_list_[i] = st.property_flags_list_[i];
    }
    links_ = 0;
}

TextBase::~TextBase() { delete cursor_; }

//---------------------------------------------------------
//   textColor
//---------------------------------------------------------

Color TextBase::text_color(const rendering::PaintOptions &opt) const { return cur_color(opt); }

//---------------------------------------------------------
//   insert
//    insert character
//---------------------------------------------------------

void TextBase::insert(TextCursor *cursor, char32_t code, LayoutData *ldata) const {
    if (cursor->row() >= ldata->blocks.size()) {
        ldata->blocks.push_back(TextBlock());
    }
    if (code == '\t') {
        code = ' ';
    }

    if (cursor->row() < ldata->blocks.size()) {
        ldata->blocks[cursor->row()].insert(cursor, String::from_ucs4(code));
    }

    cursor->set_column(cursor->column() + 1);
    cursor->clear_selection();
}

//---------------------------------------------------------
//   parseStringProperty
//---------------------------------------------------------

static String parse_string_property(const String &s) {
    String rs;
    for (size_t i = 0; i < s.size(); ++i) {
        Char c = s.at(i);
        if (c == '"') {
            break;
        }
        rs += c;
    }
    return rs;
}

//---------------------------------------------------------
//   parseNumProperty
//---------------------------------------------------------

static double parse_num_property(const String &s) { return parse_string_property(s).to_double(); }

//---------------------------------------------------------
//   createLayout
//    create layout from text
//---------------------------------------------------------
void TextBase::create_blocks() { create_blocks(mutldata()); }

void TextBase::create_blocks(LayoutData *ldata, bool source_only) const {
    // Capture markup before clearing blocks: an active editor may own the latest text.
    const String source = xml_text();
    // reset all previous formatting information
    ldata->blocks.clear();
    TextCursor cursor = *cursor_;
    cursor.set_row(0);
    cursor.set_column(0);

    int state = 0;
    String token;
    String sym;
    bool sym_state = false;
    for (size_t i = 0; i < source.size(); i++) {
        const Char &c = source.at(i);
        if (state == 0) {
            if (c == '<') {
                state = 1;
                token.clear();
            } else if (c == '&') {
                state = 2;
                token.clear();
            } else if (c == '\n') {
                if (ldata->rows() <= cursor.row()) {
                    ldata->blocks.push_back(TextBlock());
                }

                if (cursor.row() < ldata->rows()) {
                    if (ldata->blocks.at(cursor.row()).fragments().size() == 0) {
                        ldata->blocks[cursor.row()].insert_empty_fragment_if_needed(
                            &cursor); // used to preserve the Font size of the line (font info is held in TextFragments, see PR #5881)
                    }

                    ldata->blocks[cursor.row()].set_eol(true);
                }

                cursor.set_row(cursor.row() + 1);
                cursor.set_column(0);
                if (ldata->rows() <= cursor.row()) {
                    ldata->blocks.push_back(TextBlock());
                }

                if (cursor.row() < ldata->rows()) {
                    if (ldata->blocks.at(cursor.row()).fragments().size() == 0) {
                        ldata->blocks[cursor.row()].insert_empty_fragment_if_needed(
                            &cursor); // an empty fragment may be needed on either side of the newline
                    }
                }
            } else {
                if (sym_state) {
                    sym += c;
                } else {
                    if (c.is_high_surrogate()) {
                        i++;
                        assert(i < source.size());
                        insert(&cursor, Char::surrogate_to_ucs4(c, source.at(i)), ldata);
                    } else {
                        insert(&cursor, c.unicode(), ldata);
                    }
                }
            }
        } else if (state == 1) {
            if (c == '>') {
                state = 0;
                prepare_format(token, *cursor.format());
                if (token == "sym") {
                    sym_state = true;
                    sym.clear();
                } else if (token == "/sym") {
                    sym_state = false;
                    SymId id = SymNames::sym_id_by_name(sym);
                    if (id != SymId::NO_SYM) {
                        CharFormat fmt = *cursor.format(); // save format

                        char32_t code = source_symbol_base + static_cast<char32_t>(id);
                        if (!source_only) {
                            const auto score_font = score()->engraving_font();
                            code = U' ';
                            if (id != SymId::SPACE) {
                                code = score_font->is_valid(id) ? score_font->sym_code(id) :
                                                                  score()->engraving_fonts()->fallback_font()->sym_code(id);
                            }
                        }
                        cursor.format()->set_font_family(u"ScoreText");
                        insert(&cursor, code, ldata);
                        cursor.set_format(fmt); // restore format
                    } else {
                        LOGD("unknown symbol <%s>", muPrintable(sym));
                    }
                }
            } else {
                token += c;
            }
        } else if (state == 2) {
            if (c == ';') {
                state = 0;
                if (token == "lt") {
                    insert(&cursor, '<', ldata);
                } else if (token == "gt") {
                    insert(&cursor, '>', ldata);
                } else if (token == "amp") {
                    insert(&cursor, '&', ldata);
                } else if (token == "quot") {
                    insert(&cursor, '"', ldata);
                } else {
                    // TODO insert(&cursor, SymNames::symIdByName(token));
                }
            } else {
                token += c;
            }
        }
    }
    if (ldata->blocks.empty()) {
        ldata->blocks.push_back(TextBlock());
    }
    ldata->layout_invalid = false;
}

//---------------------------------------------------------
//   prepareFormat - used when reading from XML and when pasting from clipboard
//---------------------------------------------------------
bool TextBase::prepare_format(const String &token, CharFormat &format) const {
    if (token == "b") {
        format.set_bold(true);
        return true;
    } else if (token == "/b") {
        format.set_bold(false);
    } else if (token == "i") {
        format.set_italic(true);
        return true;
    } else if (token == "/i") {
        format.set_italic(false);
    } else if (token == "u") {
        format.set_underline(true);
        return true;
    } else if (token == "/u") {
        format.set_underline(false);
    } else if (token == "s") {
        format.set_strike(true);
        return true;
    } else if (token == "/s") {
        format.set_strike(false);
    } else if (token == "sub") {
        format.set_valign(VerticalAlignment::ALIGN_SUB_SCRIPT);
    } else if (token == "/sub") {
        format.set_valign(VerticalAlignment::ALIGN_NORMAL);
    } else if (token == "sup") {
        format.set_valign(VerticalAlignment::ALIGN_SUPER_SCRIPT);
    } else if (token == "/sup") {
        format.set_valign(VerticalAlignment::ALIGN_NORMAL);
    } else if (token.starts_with(u"font ")) {
        String remainder = token.mid(5);
        if (remainder.starts_with(u"size=\"")) {
            format.set_font_size(parse_num_property(remainder.mid(6)));
            return true;
        } else if (remainder.starts_with(u"face=\"")) {
            String face = parse_string_property(remainder.mid(6));
            face = un_escape(face);
            format.set_font_family(face);
            return true;
        } else {
            LOGD("cannot parse html property <%s> in text <%s>", muPrintable(token), muPrintable(text_));
        }
    }
    return false;
}

//---------------------------------------------------------
//   layoutFrame
//---------------------------------------------------------

void TextBase::layout_frame() { layout_frame(mutldata()); }

void TextBase::layout_frame(LayoutData *ldata) const {
    //      if (empty()) {    // or bbox.width() <= 1.0
    if (ldata->bbox().width() <= 1.0 || ldata->bbox().height() < 1.0) { // or bbox.width() <= 1.0
        // this does not work for Harmony:
        FontMetrics fm(font());
        double ch = fm.ascent();
        double cw = fm.width('n');
        ldata->frame = RectF(0.0, -ch, cw, ch);
    } else {
        ldata->frame = ldata->bbox();
    }

    if (rectangle()) {
        // make sure width >= height, if only one row (basically: make square for single characters)
        if (ldata->frame.height() > ldata->frame.width() && ldata->rows() == 1) {
            double w = ldata->frame.height() - ldata->frame.width();
            ldata->frame.adjust(-w * .5, 0.0, w * .5, 0.0);
        }
    } else if (circle()) {
        if (ldata->frame.width() > ldata->frame.height()) {
            ldata->frame.set_top(ldata->frame.y() + (ldata->frame.width() - ldata->frame.height()) * -.5);
            ldata->frame.set_height(ldata->frame.width());
        } else {
            ldata->frame.set_left(ldata->frame.x() + (ldata->frame.height() - ldata->frame.width()) * -.5);
            ldata->frame.set_width(ldata->frame.height());
        }
    }
    double spatium_value = spatium();
    double w = (padding_width() + frame_width() * .5f).val() * spatium_value;
    ldata->frame.adjust(-w, -w, w, w);
    w = 0.5 * frame_width().val() * spatium_value;
    ldata->set_bbox(ldata->frame.adjusted(-w, -w, w, w));
}

//---------------------------------------------------------
//   lineSpacing
//---------------------------------------------------------

double TextBase::line_spacing() const { return font_metrics().line_spacing(); }

//---------------------------------------------------------
//   lineHeight
//---------------------------------------------------------

double TextBase::line_height() const { return font_metrics().height(); }

//---------------------------------------------------------
//   baseLine
//---------------------------------------------------------

double TextBase::base_line() const { return font_metrics().ascent(); }

FontStyle TextBase::font_style() const { return cursor_->format()->style(); }

String TextBase::family() const { return cursor_->format()->font_family(); }

double TextBase::size() const { return cursor_->format()->font_size(); }

void TextBase::set_font_style(const FontStyle &val) {
    cursor_->set_format(FormatId::BOLD, val & FontStyle::BOLD);
    cursor_->set_format(FormatId::ITALIC, val & FontStyle::ITALIC);
    cursor_->set_format(FormatId::UNDERLINE, val & FontStyle::UNDERLINE);
    cursor_->set_format(FormatId::STRIKE, val & FontStyle::STRIKE);
}

void TextBase::set_family(const String &val) { cursor_->set_format(FormatId::FONT_FAMILY, val); }

void TextBase::set_size(const double &val) { cursor_->set_format(FormatId::FONT_SIZE, val); }

//---------------------------------------------------------
//   XmlNesting
//---------------------------------------------------------

struct XmlNesting : std::stack<String> {
private:

    String *s_value;

public:
    XmlNesting(String *s) { s_value = s; }

    void push_token(const String &t) {
        *s_value += u"<";
        *s_value += t;
        *s_value += u">";
        push(t);
    }

    void push_b() { push_token(u"b"); }

    void push_i() { push_token(u"i"); }

    void push_u() { push_token(u"u"); }

    void push_s() { push_token(u"s"); }

    String pop_token() {
        String s = top();
        pop();
        *s_value += u"</";
        *s_value += s;
        *s_value += u">";
        return s;
    }

    void pop_token(const char *t) {
        StringList ps;
        for (;;) {
            String s = pop_token();
            if (s == t) {
                break;
            }
            ps << s;
        }
        for (const String &s : ps) {
            push_token(s);
        }
    }

    void pop_b() { pop_token("b"); }

    void pop_i() { pop_token("i"); }

    void pop_u() { pop_token("u"); }

    void pop_s() { pop_token("s"); }
};

//---------------------------------------------------------
//   genText
//---------------------------------------------------------
String TextBase::gen_text(const LayoutData *ldata, bool source_only) const {
    if (!ldata) {
        return String();
    }

    String text;
    XmlNesting xml_nesting(&text);

    CharFormat fmt;
    fmt.set_font_family(property_default(Pid::FONT_FACE).value<String>());
    fmt.set_font_size(property_default(Pid::FONT_SIZE).to_real());
    fmt.set_style(static_cast<FontStyle>(property_default(Pid::FONT_STYLE).to_int()));

    // Prepare the initial style tags (if any).
    // We only need those if there is any fragment in the blocks with a different style than the default one.
    bool bold_value = false;
    bool italic_value = false;
    bool underline_value = false;
    bool strike_value = false;
    for (const TextBlock &block : ldata->blocks) {
        for (const TextFragment &f : block.fragments()) {
            if (!f.format.bold() && fmt.bold()) {
                bold_value = true;
            }
            if (!f.format.italic() && fmt.italic()) {
                italic_value = true;
            }
            if (!f.format.underline() && fmt.underline()) {
                underline_value = true;
            }
            if (!f.format.strike() && fmt.strike()) {
                strike_value = true;
            }
        }
    }
    if (bold_value) {
        xml_nesting.push_b();
    }
    if (italic_value) {
        xml_nesting.push_i();
    }
    if (underline_value) {
        xml_nesting.push_u();
    }
    if (strike_value) {
        xml_nesting.push_s();
    }

    // And here for the actual formatting of the text.
    for (const TextBlock &block : ldata->blocks) {
        for (const TextFragment &f : block.fragments()) {
            // don't skip, empty text fragments hold information for empty lines
            //    if (f.text.isEmpty())                   // skip empty fragments, not to
            //        continue;                           // insert extra HTML formatting

            // Push or Pop XML tags according to the current format changes
            const CharFormat &format = f.format;
            if (fmt.bold() != format.bold()) {
                if (format.bold()) {
                    xml_nesting.push_b();
                } else {
                    xml_nesting.pop_b();
                }
            }
            if (fmt.italic() != format.italic()) {
                if (format.italic()) {
                    xml_nesting.push_i();
                } else {
                    xml_nesting.pop_i();
                }
            }
            if (fmt.underline() != format.underline()) {
                if (format.underline()) {
                    xml_nesting.push_u();
                } else {
                    xml_nesting.pop_u();
                }
            }
            if (fmt.strike() != format.strike()) {
                if (format.strike()) {
                    xml_nesting.push_s();
                } else {
                    xml_nesting.pop_s();
                }
            }

            if (format.font_size() != fmt.font_size()) {
                text += String(u"<font size=\"%1\"/>").arg(format.font_size());
            }
            if (format.font_family() != "ScoreText" && format.font_family() != fmt.font_family()) {
                text += String(u"<font face=\"%1\"/>").arg(TextBase::escape(format.font_family()));
            }

            VerticalAlignment va = format.valign();
            VerticalAlignment cva = fmt.valign();
            if (cva != va) {
                switch (va) {
                    case VerticalAlignment::ALIGN_NORMAL:
                        xml_nesting.pop_token(cva == VerticalAlignment::ALIGN_SUPER_SCRIPT ? "sup" : "sub");
                        break;
                    case VerticalAlignment::ALIGN_SUPER_SCRIPT: xml_nesting.push_token(u"sup"); break;
                    case VerticalAlignment::ALIGN_SUB_SCRIPT: xml_nesting.push_token(u"sub"); break;
                    case VerticalAlignment::ALIGN_UNDEFINED: break;
                }
            }
            if (format.font_family() == u"ScoreText") {
                text += symbol_fragment_xml(f, this, source_only);
            } else {
                text += XmlWriter::xml_string(f.text);
            }
            fmt = format;
        }
        if (block.eol()) {
            text += Char::LINE_FEED;
        }
    }
    while (!xml_nesting.empty()) {
        xml_nesting.pop_token();
    }

    return text;
}

void TextBase::gen_text() {
    text_ = gen_text(ldata());
    text_invalid_ = false;
}

//---------------------------------------------------------
//   selectAll
//---------------------------------------------------------

void TextBase::select_all() {
    const LayoutData *ldata = this->ldata();
    if (!ldata || ldata->blocks.empty()) {
        return;
    }

    cursor()->set_select_column(0);
    cursor()->set_select_line(0);
    cursor()->set_row(ldata->rows() - 1);
    cursor()->set_column(cursor()->cur_line().columns());
}

void TextBase::select(SelectTextType type) {
    switch (type) {
        case SelectTextType::WORD: cursor()->select_word(); break;
        case SelectTextType::ALL: select_all(); break;
    }
}

//---------------------------------------------------------
//   pageRectangle
//---------------------------------------------------------

RectF TextBase::page_rectangle() const {
    if (ownership_parent() && (ownership_parent()->is_h_box() || ownership_parent()->is_v_box() || ownership_parent()->is_t_box())) {
        Box *box = to_box(ownership_parent());
        RectF r = box->page_bounding_rect();
        double x = r.x() + box->left_margin() * ::mu::engraving::dpmm;
        double y = r.y() + box->top_margin() * ::mu::engraving::dpmm;
        double h = r.height() - (box->top_margin() + box->bottom_margin()) * ::mu::engraving::dpmm;
        double w = r.width() - (box->left_margin() + box->right_margin()) * ::mu::engraving::dpmm;

        // SizeF ps = _doc->pageSize();
        // return RectF(x, y, ps.width(), ps.height());

        return RectF(x, y, w, h);
    }
    if (ownership_parent() && ownership_parent()->is_page()) {
        Page *box = to_page(ownership_parent());
        RectF r = box->page_bounding_rect();
        double x = r.x() + box->lm();
        double y = r.y() + box->tm();
        double h = r.height() - box->tm() - box->bm();
        double w = r.width() - box->lm() - box->rm();
        return RectF(x, y, w, h);
    }
    return page_bounding_rect();
}

//--------------------------------------------------------
//   setXmlText
//---------------------------------------------------------

void TextBase::set_xml_text(const String &s) {
    text_ = s;
    text_invalid_ = false;
    mutldata()->blocks.clear();
    mutldata()->layout_invalid = true;
}

void TextBase::check_custom_formatting(const String &s) {
    // Import records source formatting flags; parsing/reading never changes them.
    const String face_tag = u"<font face=\"";
    for (size_t pos = s.index_of(face_tag); pos != muse::nidx; pos = s.index_of(face_tag, pos + face_tag.size())) {
        if (un_escape(parse_string_property(s.mid(pos + face_tag.size()))) != property_default(Pid::FONT_FACE).value<String>()) {
            set_property_flags(Pid::FONT_FACE, PropertyFlags::UNSTYLED);
            break;
        }
    }
    if (s.contains(u"<font size")) {
        set_property_flags(Pid::FONT_SIZE, PropertyFlags::UNSTYLED);
    }
    if (s.contains(u"<b>") || s.contains(u"<i>") || s.contains(u"<u>") || s.contains(u"<s>")) {
        set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
    }
}

void TextBase::reset_formatting() {
    // reset any formatting properties that can be changed per-character (doesn't change existing text)
    cursor()->format()->set_font_family(property_default(Pid::FONT_FACE).value<String>());
    cursor()->format()->set_font_size(property_default(Pid::FONT_SIZE).to_real());
    cursor()->format()->set_style(static_cast<FontStyle>(property_default(Pid::FONT_STYLE).to_int()));
    cursor()->format()->set_valign(VerticalAlignment::ALIGN_NORMAL);
}

//---------------------------------------------------------
//   fragmentList
//---------------------------------------------------------

/*
 Return the text as a single list of TextFragment
 Used by the MusicXML formatted export to avoid parsing the xml text format
 */

std::list<TextFragment> TextBase::fragment_list() const {
    std::list<TextFragment> res;

    const TextBase *text = this;
    std::unique_ptr<TextBase> tmp_text;
    const LayoutData *ldata = this->ldata();
    if (!ldata || ldata->layout_invalid) {
        // Create temporary text object to avoid side effects
        // of createLayout() call.
        tmp_text.reset(to_text_base(this->clone()));
        tmp_text->create_blocks();
        text = tmp_text.get();
    }

    const LayoutData *tmlldata = text->ldata();
    for (const TextBlock &block : tmlldata->blocks) {
        for (const TextFragment &f : block.fragments()) {
            /* TODO TBD
            if (f.text.empty())                     // skip empty fragments, not to
                  continue;                           // insert extra HTML formatting
             */
            res.push_back(f);
            if (block.eol()) {
                // simply append a newline
                res.back().text += u"\n";
            }
        }
    }
    return res;
}

//---------------------------------------------------------
//   plainText
//    return plain text with symbols
//---------------------------------------------------------

String TextBase::plain_text() const {
    String s;

    const TextBase *text = this;
    std::unique_ptr<TextBase> tmp_text;
    const LayoutData *ldata = this->ldata();
    if (!ldata || ldata->layout_invalid) {
        // Create temporary text object to avoid side effects
        // of createLayout() call.
        tmp_text.reset(to_text_base(this->clone()));
        tmp_text->create_blocks();
        text = tmp_text.get();
    }

    const LayoutData *tmlldata = text->ldata();
    for (const TextBlock &block : tmlldata->blocks) {
        for (const TextFragment &f : block.fragments()) {
            s += f.text;
        }
        if (block.eol()) {
            s += Char::LINE_FEED;
        }
    }
    return s;
}

//---------------------------------------------------------
//   xmlText
//---------------------------------------------------------

String TextBase::xml_text() const {
    if (!text_invalid_) {
        return text_;
    }

    String text = gen_text(ldata());
    return text;
}

//---------------------------------------------------------
//   unEscape
//---------------------------------------------------------

String TextBase::un_escape(String s) {
    s.replace(u"&lt;", u"<");
    s.replace(u"&gt;", u">");
    s.replace(u"&amp;", u"&");
    s.replace(u"&apos;", u"'");
    s.replace(u"&quot;", u"\"");
    return s;
}

//---------------------------------------------------------
//   escape
//---------------------------------------------------------

String TextBase::escape(String s) {
    s.replace(u"<", u"&lt;");
    s.replace(u">", u"&gt;");
    s.replace(u"&", u"&amp;");
    s.replace(u"'", u"&apos;");
    s.replace(u"\"", u"&quot;");
    return s;
}

//---------------------------------------------------------
//   subtype
//---------------------------------------------------------

int TextBase::subtype() const { return int(text_style_type()); }

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

TranslatableString TextBase::subtype_user_name() const {
    return score() ? score()->get_text_style_user_name(text_style_type()) : TConv::user_name(text_style_type());
}

//---------------------------------------------------------
//   validateText
//    check if s is a valid musescore xml text string
//    - simple bugs are automatically adjusted
//   return true if text is valid or could be fixed
//  (this is incomplete/experimental)
//---------------------------------------------------------

bool TextBase::validate_text(String &s) {
    String d;
    for (size_t i = 0; i < s.size(); ++i) {
        Char c = s.at(i);
        if (c == u'&') {
            const char16_t *ok[]{u"amp;", u"lt;", u"gt;", u"quot;"};
            String t = s.mid(i + 1);
            bool found = false;
            for (auto k : ok) {
                if (t.starts_with(k)) {
                    d.append(c);
                    d.append(k);
                    i += int(std::u16string_view(k).size());
                    found = true;
                    break;
                }
            }
            if (!found) {
                d.append(u"&amp;");
            }
        } else if (c == u'<') {
            const char16_t *ok[]{u"b>",    u"/b>",    u"i>",   u"/i>",   u"u>",   u"/u",    u"s>",   u"/s>",
                                 u"font ", u"/font>", u"sym>", u"/sym>", u"sub>", u"/sub>", u"sup>", u"/sup>"};
            String t = s.mid(i + 1);
            bool found = false;
            for (auto k : ok) {
                if (t.starts_with(k)) {
                    d.append(c);
                    d.append(k);
                    i += int(std::u16string_view(k).size());
                    found = true;
                    break;
                }
            }
            if (!found) {
                d.append(u"&lt;");
            }
        } else {
            d.append(c);
        }
    }

    String ss = u"<data>" + d + u"</data>\n";
    muse::ByteArray ba = ss.to_utf8();
    XmlReader xml(ba);
    while (xml.read_next_start_element()) {
        // LOGD("  token %d <%s>", int(xml.tokenType()), muPrintable(xml.name().toString()));
    }
    if (xml.error() == XmlReader::NO_ERROR) {
        s = d;
        return true;
    }
    LOGD() << "xml error at byte offset " << xml.byte_offset() << ": " << xml.error_string();
    LOGD() << "text: |" << ss << "|";
    return false;
}

//---------------------------------------------------------
//   font
//---------------------------------------------------------

Font TextBase::font() const {
    double m = size();
    if (size_is_spatium_dependent()) {
        m *= spatium() / default_spatium();
    }
    Font f(family(), Font::Type::UNKNOWN);
    f.set_point_size_f(m);
    f.set_bold(bold());
    f.set_italic(italic());
    if (underline()) {
        f.set_underline(underline());
    }
    if (strike()) {
        f.set_strike(strike());
    }

    return f;
}

//---------------------------------------------------------
//   fontMetrics
//---------------------------------------------------------

FontMetrics TextBase::font_metrics() const { return FontMetrics(font()); }

bool TextBase::is_property_linked_to_master(Pid id) const {
    if (property_group(id) == PropertyGroup::TEXT) {
        return is_text_linked_to_master();
    }

    return EngravingItem::is_property_linked_to_master(id);
}

bool TextBase::is_unlinked_from_master() const {
    EngravingItem *parent = ownership_parent_item();
    if (parent && parent->is_unlinked_from_master()) {
        return true;
    }

    return !get_property(Pid::TEXT_LINKED_TO_MASTER).to_bool() || EngravingItem::is_unlinked_from_master();
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TextBase::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::TEXT_STYLE: return text_style_type();
        case Pid::FONT_FACE: return cursor_->selected_fragments_format().font_family();
        case Pid::FONT_SIZE: return cursor_->selected_fragments_format().font_size();
        case Pid::FONT_STYLE: return static_cast<int>(cursor_->selected_fragments_format().style());
        case Pid::TEXT_LINE_SPACING: return text_line_spacing();
        case Pid::FRAME_TYPE: return static_cast<int>(frame_type());
        case Pid::FRAME_WIDTH: return frame_width();
        case Pid::FRAME_PADDING: return padding_width();
        case Pid::FRAME_ROUND: return frame_round();
        case Pid::FRAME_FG_COLOR: return PropertyValue::from_value(frame_color());
        case Pid::FRAME_BG_COLOR: return PropertyValue::from_value(bg_color());
        case Pid::ALIGN: return PropertyValue::from_value(align());
        case Pid::POSITION: return PropertyValue::from_value(position());
        case Pid::TEXT_SCRIPT_ALIGN: return static_cast<int>(cursor_->selected_fragments_format().valign());
        case Pid::TEXT: return xml_text();
        case Pid::TEXT_LINKED_TO_MASTER: return is_text_linked_to_master();
        case Pid::DIRECTION: return direction();
        case Pid::CENTER_BETWEEN_STAVES: return center_between_staves();
        case Pid::VOICE_ASSIGNMENT: return voice_assignment();
        case Pid::MUSIC_SYMBOL_SIZE: return symbol_size();
        case Pid::MUSICAL_SYMBOLS_SCALE: return symbol_scale();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TextBase::set_property(Pid pid, const PropertyValue &v) {
    if (text_invalid_ && ldata() && ldata()->is_valid()) {
        gen_text();
    }

    bool rv = true;
    switch (pid) {
        case Pid::COLOR:
            if (color() == frame_color()) {
                set_frame_color(v.value<Color>());
            }
            EngravingItem::set_property(pid, v);
            break;
        case Pid::TEXT_STYLE: init_text_style_type(v.value<TextStyleType>()); break;
        case Pid::FONT_FACE: set_family(v.value<String>()); break;
        case Pid::FONT_SIZE: set_size(v.to_real()); break;
        case Pid::FONT_STYLE: set_font_style(FontStyle(v.to_int())); break;
        case Pid::TEXT_LINE_SPACING: set_text_line_spacing(v.to_real()); break;
        case Pid::FRAME_TYPE: set_frame_type(FrameType(v.to_int())); break;
        case Pid::FRAME_WIDTH: set_frame_width(v.value<Spatium>()); break;
        case Pid::FRAME_PADDING: set_padding_width(v.value<Spatium>()); break;
        case Pid::FRAME_ROUND: set_frame_round(v.value<Spatium>()); break;
        case Pid::FRAME_FG_COLOR: set_frame_color(v.value<Color>()); break;
        case Pid::FRAME_BG_COLOR: set_bg_color(v.value<Color>()); break;
        case Pid::TEXT: set_xml_text(v.value<String>()); break;
        case Pid::ALIGN: set_align(v.value<Align>()); break;
        case Pid::POSITION: set_position(v.value<AlignH>()); break;
        case Pid::TEXT_SCRIPT_ALIGN: cursor_->set_format(FormatId::VALIGN, v.to_int()); break;
        case Pid::TEXT_LINKED_TO_MASTER:
            if (is_text_linked_to_master() == v.to_bool()) {
                break;
            }
            if (!is_text_linked_to_master()) {
                relink_properties_to_master(PropertyGroup::TEXT);
            }
            set_text_linked_to_master(v.to_bool());
            break;
        case Pid::DIRECTION: set_direction(v.value<DirectionV>()); break;
        case Pid::CENTER_BETWEEN_STAVES: set_center_between_staves(v.value<AutoOnOff>()); break;
        case Pid::VOICE_ASSIGNMENT: set_voice_assignment(v.value<VoiceAssignment>()); break;
        case Pid::MUSIC_SYMBOL_SIZE: set_symbol_size(v.to_double()); break;
        case Pid::MUSICAL_SYMBOLS_SCALE: set_symbol_scale(v.to_double()); break;
        default: rv = EngravingItem::set_property(pid, v); break;
    }

    trigger_layout();

    return rv;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TextBase::property_default(Pid id) const {
    if (id == Pid::Z) {
        return EngravingItem::property_default(id);
    }

    if (composition()) {
        PropertyValue v = ownership_parent()->property_default(id);
        if (v.is_valid()) {
            return v;
        }
    }

    Sid sid = get_property_style(id);
    if (sid != Sid::NOSTYLE) {
        return style_value(id, sid);
    }

    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::DEFAULT;
        case Pid::TEXT: return String();
        case Pid::TEXT_SCRIPT_ALIGN: return static_cast<int>(VerticalAlignment::ALIGN_NORMAL);
        case Pid::TEXT_LINKED_TO_MASTER: return true;
        case Pid::DIRECTION: return DirectionV::AUTO;
        case Pid::CENTER_BETWEEN_STAVES: return AutoOnOff::AUTO;
        case Pid::VOICE_ASSIGNMENT: return VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
        default:
            for (const auto &p : *text_style(TextStyleType::DEFAULT)) {
                if (p.pid == id) {
                    return style_value(id, p.sid);
                }
            }
    }

    return EngravingItem::property_default(id);
}

//---------------------------------------------------------
//   getPropertyFlagsIdx
//---------------------------------------------------------

int TextBase::get_property_flags_idx(Pid id) const {
    int i = 0;
    for (const StyledProperty &p : *element_style_) {
        if (p.pid == id) {
            return i;
        }
        ++i;
    }
    for (const auto &p : *text_style(text_style_type())) {
        if (p.pid == id) {
            return i;
        }
        ++i;
    }
    return -1;
}

//---------------------------------------------------------
//   offsetSid
//---------------------------------------------------------

Sid TextBase::default_pos_sid() const {
    const OffsetSids offsets = text_style(text_style_type())->offset_sids;
    return place_above() ? offsets.above : offsets.below;
}

PointF TextBase::default_pos() const {
    if (parent()->is_text_line_base_segment()) {
        return PointF();
    }

    return EngravingItem::default_pos();
}

//---------------------------------------------------------
//   getHtmlStartTag - helper function for extractText with withFormat = true
//---------------------------------------------------------
String TextBase::get_html_start_tag(double new_size, double &cur_size, const String &new_family, String &cur_family, FontStyle style,
                                    VerticalAlignment v_align) {
    String s;
    if (std::fabs(new_size - cur_size) > 0.1) {
        cur_size = new_size;
        s += String(u"<font size=\"%1\"/>").arg(new_size);
    }
    if (new_family != cur_family && new_family != "ScoreText") {
        cur_family = new_family;
        s += String(u"<font face=\"%1\"/>").arg(new_family);
    }
    if (style & FontStyle::BOLD) {
        s += u"<b>";
    }
    if (style & FontStyle::ITALIC) {
        s += u"<i>";
    }
    if (style & FontStyle::UNDERLINE) {
        s += u"<u>";
    }
    if (style & mu::engraving::FontStyle::STRIKE) {
        s += u"<s>";
    }
    if (v_align == VerticalAlignment::ALIGN_SUB_SCRIPT) {
        s += u"<sub>";
    } else if (v_align == VerticalAlignment::ALIGN_SUPER_SCRIPT) {
        s += u"<sup>";
    }
    return s;
}

//---------------------------------------------------------
//   getHtmlEndTag - helper function for extractText with withFormat = true
//---------------------------------------------------------
String TextBase::get_html_end_tag(FontStyle style, VerticalAlignment v_align) {
    String s;
    if (v_align == VerticalAlignment::ALIGN_SUB_SCRIPT) {
        s += u"</sub>";
    } else if (v_align == VerticalAlignment::ALIGN_SUPER_SCRIPT) {
        s += u"</sup>";
    }
    if (style & FontStyle::STRIKE) {
        s += u"</s>";
    }
    if (style & FontStyle::UNDERLINE) {
        s += u"</u>";
    }
    if (style & FontStyle::ITALIC) {
        s += u"</i>";
    }
    if (style & FontStyle::BOLD) {
        s += u"</b>";
    }
    return s;
}

void TextBase::notify_about_text_cursor_changed() {}

void TextBase::notify_about_text_inserted([[maybe_unused]] int start_position, [[maybe_unused]] int end_position,
                                          [[maybe_unused]] const String &text) {}

void TextBase::notify_about_text_removed([[maybe_unused]] int start_position, [[maybe_unused]] int end_position,
                                         [[maybe_unused]] const String &text) {}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid TextBase::get_property_style(Pid id) const {
    for (const StyledProperty &p : *element_style_) {
        if (p.pid == id) {
            return p.sid;
        }
    }
    for (const auto &p : *text_style(text_style_type())) {
        if (p.pid == id) {
            return p.sid;
        }
    }
    return Sid::NOSTYLE;
}

//---------------------------------------------------------
//   styleChanged
//---------------------------------------------------------

void TextBase::style_changed() {
    if (!styled_properties()) {
        LOGD("no styled properties");
        return;
    }
    int i = 0;
    for (const StyledProperty &spp : *element_style_) {
        PropertyFlags f = property_flags_list_[i];
        if (f == PropertyFlags::STYLED) {
            set_property(spp.pid, style_value(spp.pid, get_property_style(spp.pid)));
        }
        ++i;
    }
    for (const auto &spp : *text_style(text_style_type())) {
        PropertyFlags f = property_flags_list_[i];
        if (f == PropertyFlags::STYLED) {
            set_property(spp.pid, style_value(spp.pid, get_property_style(spp.pid)));
        }
        ++i;
    }
}

//---------------------------------------------------------
//   initElementStyle
//---------------------------------------------------------

void TextBase::init_element_style(const ElementStyle *ss) {
    element_style_ = ss;
    size_t n = ss->size() + ::mu::engraving::text_style_size;

    delete[] property_flags_list_;
    property_flags_list_ = new PropertyFlags[n];
    for (size_t i = 0; i < n; ++i) {
        property_flags_list_[i] = PropertyFlags::STYLED;
    }
    for (const StyledProperty &p : *element_style_) {
        set_property(p.pid, style_value(p.pid, p.sid));
    }
    for (const auto &p : *text_style(text_style_type())) {
        set_property(p.pid, style_value(p.pid, p.sid));
    }
}

//---------------------------------------------------------
//   initTid
//---------------------------------------------------------

void TextBase::init_text_style_type(TextStyleType tid, bool preserve_different) {
    if (!preserve_different) {
        init_text_style_type(tid);
        return;
    }

    const LayoutData *ldata = this->ldata();
    if (!ldata || ldata->layout_invalid) {
        create_blocks();
    }

    // Before setting the new style - check if any fragments contain custom formatting. If they do, preserve
    // the old values for face, size, and style...
    const bool had_custom_fragments = has_custom_formatting();

    set_text_style_type(tid);

    for (const auto &p : *text_style(tid)) {
        const bool is_fragment_style = p.pid == Pid::FONT_FACE || p.pid == Pid::FONT_SIZE || p.pid == Pid::FONT_STYLE;
        if (is_fragment_style && had_custom_fragments) {
            continue;
        }
        if (get_property(p.pid) == property_default(p.pid)) {
            set_property(p.pid, style_value(p.pid, p.sid));
        }
    }
}

void TextBase::init_text_style_type(TextStyleType tid) {
    auto get_text_pid = [&](Pid p) -> Pid {
        static const std::vector<std::pair<Pid, Pid>> text_line_pid_map = {
            {Pid::FONT_FACE, Pid::BEGIN_FONT_FACE}, {Pid::FONT_SIZE, Pid::BEGIN_FONT_SIZE},    {Pid::FONT_STYLE, Pid::BEGIN_FONT_STYLE},
            {Pid::ALIGN, Pid::BEGIN_TEXT_ALIGN},    {Pid::POSITION, Pid::BEGIN_TEXT_POSITION},
        };

        const bool is_text_line = parent()->is_text_line_base_segment();
        for (const auto &pid_pair : text_line_pid_map) {
            const Pid text_pid = pid_pair.first;
            const Pid text_line_pid = pid_pair.second;

            if (p == text_line_pid || p == text_pid) {
                return is_text_line ? text_line_pid : text_pid;
            }
        }

        return p;
    };

    set_text_style_type(tid);
    for (const auto &p : *text_style(tid)) {
        set_property(get_text_pid(p.pid), style_value(p.pid, p.sid));
    }
}

//---------------------------------------------------------
//   hasCustomFormatting
//---------------------------------------------------------

bool TextBase::has_custom_formatting() const {
    const LayoutData *ldata = this->ldata();
    IF_ASSERT_FAILED(ldata) { return false; }

    CharFormat fmt;
    fmt.set_font_family(family());
    fmt.set_font_size(size());
    fmt.set_style(font_style());
    fmt.set_valign(VerticalAlignment::ALIGN_NORMAL);

    for (const TextBlock &block : ldata->blocks) {
        for (const TextFragment &f : block.fragments()) {
            if (f.text.is_empty()) { // skip empty fragments, not to
                continue;            // insert extra HTML formatting
            }
            const CharFormat &format = f.format;
            if (fmt.style() != format.style()) {
                return true;
            }
            if (format.font_size() != fmt.font_size()) {
                return true;
            }
            if (format.font_family() != fmt.font_family()) {
                return true;
            }

            VerticalAlignment va = format.valign();
            VerticalAlignment cva = fmt.valign();
            if (cva != va) {
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------------------
//   stripText
//    remove some custom text formatting and return
//    result as xml string
//---------------------------------------------------------

String TextBase::strip_text(bool remove_style, bool remove_size, bool remove_face) const {
    LayoutData source;
    create_blocks(&source, true);
    const LayoutData *ldata = &source;

    String txt;
    bool bold_value = false;
    bool italic_value = false;
    bool underline_value = false;
    bool strike_value = false;

    for (const TextBlock &block : ldata->blocks) {
        for (const TextFragment &f : block.fragments()) {
            if (!f.format.bold() && bold()) {
                bold_value = true;
            }
            if (!f.format.italic() && italic()) {
                italic_value = true;
            }
            if (!f.format.underline() && underline()) {
                underline_value = true;
            }
            if (!f.format.strike() && strike()) {
                strike_value = true;
            }
        }
    }
    CharFormat fmt;
    fmt.set_font_family(family());
    fmt.set_font_size(size());
    fmt.set_style(font_style());
    fmt.set_valign(VerticalAlignment::ALIGN_NORMAL);

    XmlNesting xml_nesting(&txt);
    if (!remove_style) {
        if (bold_value) {
            xml_nesting.push_b();
        }
        if (italic_value) {
            xml_nesting.push_i();
        }
        if (underline_value) {
            xml_nesting.push_u();
        }
        if (strike_value) {
            xml_nesting.push_s();
        }
    }

    for (const TextBlock &block : ldata->blocks) {
        for (const TextFragment &f : block.fragments()) {
            if (f.text.is_empty()) { // skip empty fragments, not to
                continue;            // insert extra HTML formatting
            }
            const CharFormat &format = f.format;
            if (!remove_style) {
                if (fmt.bold() != format.bold()) {
                    if (format.bold()) {
                        xml_nesting.push_b();
                    } else {
                        xml_nesting.pop_b();
                    }
                }
                if (fmt.italic() != format.italic()) {
                    if (format.italic()) {
                        xml_nesting.push_i();
                    } else {
                        xml_nesting.pop_i();
                    }
                }
                if (fmt.underline() != format.underline()) {
                    if (format.underline()) {
                        xml_nesting.push_u();
                    } else {
                        xml_nesting.pop_u();
                    }
                }
                if (fmt.strike() != format.strike()) {
                    if (format.strike()) {
                        xml_nesting.push_s();
                    } else {
                        xml_nesting.pop_s();
                    }
                }
            }

            if (!remove_size && (format.font_size() != fmt.font_size())) {
                txt += String(u"<font size=\"%1\"/>").arg(format.font_size());
            }
            if (!remove_face && (format.font_family() != fmt.font_family())) {
                txt += String(u"<font face=\"%1\"/>").arg(TextBase::escape(format.font_family()));
            }

            VerticalAlignment va = format.valign();
            VerticalAlignment cva = fmt.valign();
            if (cva != va) {
                switch (va) {
                    case VerticalAlignment::ALIGN_NORMAL:
                        xml_nesting.pop_token(cva == VerticalAlignment::ALIGN_SUPER_SCRIPT ? "sup" : "sub");
                        break;
                    case VerticalAlignment::ALIGN_SUPER_SCRIPT: xml_nesting.push_token(u"sup"); break;
                    case VerticalAlignment::ALIGN_SUB_SCRIPT: xml_nesting.push_token(u"sub"); break;
                    case VerticalAlignment::ALIGN_UNDEFINED: break;
                }
            }
            if (format.font_family() == u"ScoreText") {
                txt += symbol_fragment_xml(f, this, true);
            } else {
                txt += XmlWriter::xml_string(f.text);
            }
            fmt = format;
        }
        if (block.eol()) {
            txt += Char::LINE_FEED;
        }
    }
    while (!xml_nesting.empty()) {
        xml_nesting.pop_token();
    }
    return txt;
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void TextBase::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (ps == PropertyFlags::STYLED && v == property_default(id)) {
        // this is a reset
        // remove some custom formatting
        if (id == Pid::FONT_STYLE) {
            undo_change_property(Pid::TEXT, strip_text(true, false, false), property_flags(id));
        } else if (id == Pid::FONT_SIZE) {
            undo_change_property(Pid::TEXT, strip_text(false, true, false), property_flags(id));
        } else if (id == Pid::FONT_FACE) {
            undo_change_property(Pid::TEXT, strip_text(false, false, true), property_flags(id));
        }
    }

    static const PropertyIdSet character_specific_properties{Pid::FONT_STYLE, Pid::FONT_FACE, Pid::FONT_SIZE, Pid::TEXT_SCRIPT_ALIGN};

    if (!muse::contains(character_specific_properties, id)) {
        EngravingItem::undo_change_property(id, v, ps);
        return;
    }

    score()->undo(new ChangeTextProperties(cursor_, id, v, ps));

    if (cursor_->editing()) {
        // If we're in edit mode, changes will be propagated later when
        // propagating the Pid::TEXT property, so don't propagate now.
        return;
    }

    const std::list<EngravingObject *> linked_objects = link_list_for_property_propagation();
    for (EngravingObject *linked_object : linked_objects) {
        TextBase *linked_text = to_text_base(linked_object);
        if (linked_text == this) {
            continue;
        }

        Score *linked_score = linked_text->score();
        TextCursor *linked_cursor = linked_text->cursor();
        PropertyPropagation property_propagate = property_propagation(linked_text, id);

        switch (property_propagate) {
            case PropertyPropagation::PROPAGATE: linked_score->undo(new ChangeTextProperties(linked_cursor, id, v, ps)); break;
            case PropertyPropagation::UNLINK: unlink_property_from_master(id); break;
            default: break;
        }
    }

    for (EngravingItem *origin_item : origin_items()) {
        // This is a shared item: propagate to all origin items
        score()->undo(new ChangeTextProperties(to_text_base(origin_item)->cursor(), id, v, ps));
    }
    if (EngravingItem *shared_el = shared_item(); shared_el && shared_el->origin_items().front() == this) {
        // This is the first origin item of the shared item: propagate to shared item
        score()->undo(new ChangeTextProperties(to_text_base(shared_el)->cursor(), id, v, ps));
    }
}

bool mu::engraving::TextBase::has_symbol_scale() const {
    bool has_symbol_scale = is_dynamic() || is_string_tunings() || is_play_tech_annotation() ||
                            (is_harp_pedal_diagram() && to_harp_pedal_diagram(this)->is_diagram()) ||
                            (parent() && parent()->is_ottava_segment()) || (parent() && parent()->is_tuplet()) ||
                            (parent() && parent()->is_pedal_segment());

    return has_symbol_scale;
}

double TextBase::spatium_scaling() const {
    double spatium_scaling;

    if (is_instrument_name()) {
        spatium_scaling = to_instrument_name(this)->largest_staff_spatium() / default_spatium();
    } else {
        spatium_scaling = spatium() / default_spatium();
    }

    return spatium_scaling;
}
} // namespace mu::engraving
