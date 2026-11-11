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

#include "figuredbass.h"

#include "chord.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "factory.h"
#include "io/file.h"
#include "linkedobjects.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "rest.h"
#include "rw/xmlreader.h"
#include "score.h"
#include "segment.h"
#include "style/textstyle.h"

using namespace mu;
using namespace muse::io;
using namespace mu::engraving;

namespace mu::engraving {
static std::vector<FiguredBassFont> g_fb_fonts;

const std::vector<FiguredBassFont> &FiguredBass::fb_fonts() { return g_fb_fonts; }

//---------------------------------------------------------
//   figuredBassStyle
//---------------------------------------------------------

static const ElementStyle figured_bass_style{
    {Sid::FIGURED_BASS_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

//---------------------------------------------------------
//   figuredBassTextStyle
//---------------------------------------------------------

static const ElementStyle figured_bass_text_style{
    {Sid::FIGURED_BASS_FONT_FACE, Pid::FONT_FACE},
    {Sid::FIGURED_BASS_FONT_SIZE, Pid::FONT_SIZE},
    {Sid::FIGURED_BASS_FONT_STYLE, Pid::FONT_STYLE},
};

//---------------------------------------------------------
//   F I G U R E D   B A S S   I T E M
//---------------------------------------------------------

// used for indexed access to parenthesis chars
// (these is no normAccidToChar[], as accidentals may use mult. chars in normalized display):
const Char FiguredBassItem::norm_parenth_to_char[int(FiguredBassItem::Parenthesis::NUMOF)] = {0, '(', ')', '[', ']'};

FiguredBassItem::FiguredBassItem(FiguredBass *parent, int l) : EngravingItem(ElementType::FIGURED_BASS_ITEM, parent), ord_(l) {
    prefix_ = suffix_ = Modifier::NONE;
    digit_ = ::mu::engraving::fbi_digit_none;
    parenth_[0] = parenth_[1] = parenth_[2] = parenth_[3] = parenth_[4] = Parenthesis::NONE;
    cont_line_ = ContLine::NONE;
}

FiguredBassItem::FiguredBassItem(const FiguredBassItem &item) : EngravingItem(item) {
    ord_ = item.ord_;
    prefix_ = item.prefix_;
    digit_ = item.digit_;
    suffix_ = item.suffix_;
    parenth_[0] = item.parenth_[0];
    parenth_[1] = item.parenth_[1];
    parenth_[2] = item.parenth_[2];
    parenth_[3] = item.parenth_[3];
    parenth_[4] = item.parenth_[4];
    cont_line_ = item.cont_line_;
}

FiguredBassItem::~FiguredBassItem() {}

//---------------------------------------------------------
//   FiguredBassItem parse()
//
// converts a string into a property-based representation, if possible;
// return true on success | false if the string is non-conformant
//---------------------------------------------------------

bool FiguredBassItem::parse(String &str) {
    int ret_val;

    parse_parenthesis(str, 0);
    ret_val = parse_prefix_suffix(str, true); // prefix
    if (ret_val == -1) {
        return false;
    }
    parse_parenthesis(str, 1);
    ret_val = parse_digit(str); // digit
    if (ret_val == -1) {
        return false;
    }
    parse_parenthesis(str, 2);
    ret_val = parse_prefix_suffix(str, false); // suffix
    if (ret_val == -1) {
        return false;
    }
    parse_parenthesis(str, 3);
    // check for a possible cont. line symbol(s)
    cont_line_ = ContLine::NONE;                                    // contLine
    if (!str.empty() && (str.at(0) == u'-' || str.at(0) == u'_')) { // 1 symbol: simple continuation
        cont_line_ = ContLine::SIMPLE;
        str.remove(0, 1);
    }
    while (!str.empty() && (str.at(0) == u'-' || str.at(0) == u'_')) { // more than 1 symbol: extended continuation
        cont_line_ = ContLine::EXTENDED;
        str.remove(0, 1);
    }
    parse_parenthesis(str, 4);

    // remove useless parentheses, moving external parentheses toward central digit element
    if (prefix_ == Modifier::NONE && parenth_[1] == Parenthesis::NONE) {
        parenth_[1] = parenth_[0];
        parenth_[0] = Parenthesis::NONE;
    }
    if (digit_ == ::mu::engraving::fbi_digit_none && parenth_[2] == Parenthesis::NONE) {
        parenth_[2] = parenth_[1];
        parenth_[1] = Parenthesis::NONE;
    }
    if (cont_line_ == ContLine::NONE && parenth_[3] == Parenthesis::NONE) {
        parenth_[3] = parenth_[4];
        parenth_[4] = Parenthesis::NONE;
    }
    if (suffix_ == Modifier::NONE && parenth_[2] == Parenthesis::NONE) {
        parenth_[2] = parenth_[3];
        parenth_[3] = Parenthesis::NONE;
    }

    // some checks:
    // if some extra input, str is not conformant
    if (!str.empty()) {
        return false;
    }
    // can't have BOTH prefix and suffix
    // prefix, digit, suffix and cont.line cannot be ALL empty
    // suffix cannot combine with empty digit
    if ((prefix_ != Modifier::NONE && suffix_ != Modifier::NONE) ||
        (prefix_ == Modifier::NONE && digit_ == ::mu::engraving::fbi_digit_none && suffix_ == Modifier::NONE &&
         cont_line_ == ContLine::NONE) ||
        ((suffix_ == Modifier::CROSS || suffix_ == Modifier::BACKSLASH || suffix_ == Modifier::SLASH) &&
         digit_ == ::mu::engraving::fbi_digit_none)) {
        return false;
    }
    return true;
}

//---------------------------------------------------------
//   FiguredBassItem parsePrefixSuffix()
//
//    scans str to extract prefix or suffix properties. Stops at the first char which cannot fit.
//    Fitting chars are removed from str. DOES NOT generate any display text
//
// returns the number of Chars read from str or -1 if prefix / suffix has an illegal format
// (no prefix / suffix at all IS legal)
//---------------------------------------------------------

int FiguredBassItem::parse_prefix_suffix(String &str, bool b_prefix) {
    Modifier *dest = b_prefix ? &prefix_ : &suffix_;
    bool done = false;
    size_t size = str.size();
    str = str.trimmed();

    *dest = Modifier::NONE;

    while (str.size()) {
        switch (str.at(0).unicode()) {
            case 'b':
                if (*dest != Modifier::NONE) {
                    if (*dest == Modifier::FLAT) { // FLAT may double a previous FLAT
                        *dest = Modifier::DOUBLEFLAT;
                    } else {
                        return -1; // but no other combination is acceptable
                    }
                } else {
                    *dest = Modifier::FLAT;
                }
                break;
            case 'h':
                if (*dest != Modifier::NONE) { // cannot combine with any other accidental
                    return -1;
                }
                *dest = Modifier::NATURAL;
                break;
            case '#':
                if (*dest != Modifier::NONE) {
                    if (*dest == Modifier::SHARP) { // SHARP may double a previous SHARP
                        *dest = Modifier::DOUBLESHARP;
                    } else {
                        return -1; // but no other combination is acceptable
                    }
                } else {
                    *dest = Modifier::SHARP;
                }
                break;
            case '+':
                // accept '+' as both a prefix and a suffix for harmony notation
                if (*dest != Modifier::NONE) { // cannot combine with any other accidental
                    return -1;
                }
                *dest = Modifier::CROSS;
                break;
            // '\\' and '/' go into the suffix
            case '\\':
                if (suffix_ != Modifier::NONE) { // cannot combine with any other accidental
                    return -1;
                }
                suffix_ = Modifier::BACKSLASH;
                break;
            case '/':
                if (suffix_ != Modifier::NONE) { // cannot combine with any other accidental
                    return -1;
                }
                suffix_ = Modifier::SLASH;
                break;
            default: // any other char: no longer in prefix/suffix
                done = true;
                break;
        }
        if (done) {
            break;
        }
        str.remove(0, 1); // 'eat' the char and continue
    }

    return static_cast<int>(size - str.size()); // return how many chars we had read into prefix/suffix
}

//---------------------------------------------------------
//   FiguredBassItem parseDigit()
//
//    scans str to extract digit properties. Stops at the first char which cannot belong to digit part.
//    Fitting chars are removed from str. DOES NOT generate any display text
//
// returns the number of Chars read from str or -1 if no legal digit can be constructed
// (no digit at all IS legal)
//---------------------------------------------------------

int FiguredBassItem::parse_digit(String &str) {
    size_t size = str.size();
    str = str.trimmed();

    digit_ = ::mu::engraving::fbi_digit_none;

    while (str.size()) {
        // any digit acceptable
        if (str.at(0) >= u'0' && str.at(0) <= u'9') {
            if (digit_ == ::mu::engraving::fbi_digit_none) {
                digit_ = 0;
            }
            digit_ = digit_ * 10 + (str.at(0).unicode() - '0');
            str.remove(0, 1);
        }
        // anything else: no longer in digit part
        else {
            break;
        }
    }

    return static_cast<int>(size - str.size());
}

//---------------------------------------------------------
//   FiguredBassItem parseParenthesis()
//
//    scans str to extract a (possible) parenthesis, stores its code into parenth[parenthIdx]
//    and removes it from str. Only looks at first str char.
//
// returns the number of Chars read from str (actually 0 or 1).
//---------------------------------------------------------

int FiguredBassItem::parse_parenthesis(String &str, int parenth_idx) {
    if (str.empty()) {
        return 0;
    }

    char16_t c = str.at(0).unicode();
    Parenthesis code = Parenthesis::NONE;
    switch (c) {
        case '(': code = Parenthesis::ROUNDOPEN; break;
        case ')': code = Parenthesis::ROUNDCLOSED; break;
        case '[': code = Parenthesis::SQUAREDOPEN; break;
        case ']': code = Parenthesis::SQUAREDCLOSED; break;
        default: break;
    }
    parenth_[parenth_idx] = code;
    if (code != Parenthesis::NONE) {
        str.remove(0, 1);
        return 1;
    }
    return 0;
}

//---------------------------------------------------------
//   FiguredBassItem normalizedText()
//
// returns a string with the normalized text, i.e. the text displayed while editing;
// this is a standard textual representation of the item properties
//---------------------------------------------------------

String FiguredBassItem::normalized_text() const {
    String str;
    if (parenth_[0] != Parenthesis::NONE) {
        str.append(::mu::engraving::FiguredBassItem::norm_parenth_to_char[int(parenth_[0])]);
    }

    if (prefix_ != Modifier::NONE) {
        switch (prefix_) {
            case Modifier::FLAT: str.append(u'b'); break;
            case Modifier::NATURAL: str.append(u'h'); break;
            case Modifier::SHARP: str.append(u'#'); break;
            case Modifier::CROSS: str.append(u'+'); break;
            case Modifier::DOUBLEFLAT: str.append(u"bb"); break;
            case Modifier::DOUBLESHARP: str.append(u"##"); break;
            default: break;
        }
    }

    if (parenth_[1] != Parenthesis::NONE) {
        str.append(::mu::engraving::FiguredBassItem::norm_parenth_to_char[int(parenth_[1])]);
    }

    // digit
    if (digit_ != ::mu::engraving::fbi_digit_none) {
        str.append(String::number(digit_));
    }

    if (parenth_[2] != Parenthesis::NONE) {
        str.append(::mu::engraving::FiguredBassItem::norm_parenth_to_char[int(parenth_[2])]);
    }

    // suffix
    if (suffix_ != Modifier::NONE) {
        switch (suffix_) {
            case Modifier::FLAT: str.append(u'b'); break;
            case Modifier::NATURAL: str.append(u'h'); break;
            case Modifier::SHARP: str.append(u'#'); break;
            case Modifier::CROSS: str.append(u'+'); break;
            case Modifier::BACKSLASH: str.append(u'\\'); break;
            case Modifier::SLASH: str.append(u'/'); break;
            case Modifier::DOUBLEFLAT: str.append(u"bb"); break;
            case Modifier::DOUBLESHARP: str.append(u"##"); break;
            default: break;
        }
    }

    if (parenth_[3] != Parenthesis::NONE) {
        str.append(::mu::engraving::FiguredBassItem::norm_parenth_to_char[int(parenth_[3])]);
    }
    if (cont_line_ > ContLine::NONE) {
        str.append('_');
        if (cont_line_ > ContLine::SIMPLE) {
            str.append('_');
        }
    }
    if (parenth_[4] != Parenthesis::NONE) {
        str.append(::mu::engraving::FiguredBassItem::norm_parenth_to_char[int(parenth_[4])]);
    }

    return str;
}

//---------------------------------------------------------
//   PROPERTY METHODS
//---------------------------------------------------------

PropertyValue FiguredBassItem::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::FBPREFIX: return int(prefix_);
        case Pid::FBDIGIT: return digit_;
        case Pid::FBSUFFIX: return int(suffix_);
        case Pid::FBCONTINUATIONLINE: return int(cont_line_);
        case Pid::FBPARENTHESIS1: return int(parenth_[0]);
        case Pid::FBPARENTHESIS2: return int(parenth_[1]);
        case Pid::FBPARENTHESIS3: return int(parenth_[2]);
        case Pid::FBPARENTHESIS4: return int(parenth_[3]);
        case Pid::FBPARENTHESIS5: return int(parenth_[4]);
        default: return EngravingItem::get_property(property_id);
    }
}

bool FiguredBassItem::set_property(Pid property_id, const PropertyValue &v) {
    score()->add_refresh(canvas_bounding_rect());
    int val = v.to_int();
    switch (property_id) {
        case Pid::FBPREFIX:
            if (val < int(Modifier::NONE) || val >= int(Modifier::NUMOF)) {
                return false;
            }
            prefix_ = (Modifier) val;
            break;
        case Pid::FBDIGIT:
            if (val < 1 || val > 9) {
                return false;
            }
            digit_ = val;
            break;
        case Pid::FBSUFFIX:
            if (val < int(Modifier::NONE) || val >= int(Modifier::NUMOF)) {
                return false;
            }
            suffix_ = (Modifier) val;
            break;
        case Pid::FBCONTINUATIONLINE: cont_line_ = (ContLine) val; break;
        case Pid::FBPARENTHESIS1:
            if (val < int(Parenthesis::NONE) || val >= int(Parenthesis::NUMOF)) {
                return false;
            }
            parenth_[0] = (Parenthesis) val;
            break;
        case Pid::FBPARENTHESIS2:
            if (val < int(Parenthesis::NONE) || val >= int(Parenthesis::NUMOF)) {
                return false;
            }
            parenth_[1] = (Parenthesis) val;
            break;
        case Pid::FBPARENTHESIS3:
            if (val < int(Parenthesis::NONE) || val >= int(Parenthesis::NUMOF)) {
                return false;
            }
            parenth_[2] = (Parenthesis) val;
            break;
        case Pid::FBPARENTHESIS4:
            if (val < int(Parenthesis::NONE) || val >= int(Parenthesis::NUMOF)) {
                return false;
            }
            parenth_[3] = (Parenthesis) val;
            break;
        case Pid::FBPARENTHESIS5:
            if (val < int(Parenthesis::NONE) || val >= int(Parenthesis::NUMOF)) {
                return false;
            }
            parenth_[4] = (Parenthesis) val;
            break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

PropertyValue FiguredBassItem::property_default(Pid id) const {
    switch (id) {
        case Pid::FBPREFIX:
        case Pid::FBSUFFIX: return int(Modifier::NONE);
        case Pid::FBDIGIT: return ::mu::engraving::fbi_digit_none;
        case Pid::FBCONTINUATIONLINE: return static_cast<int>(ContLine::NONE);
        default: return EngravingItem::property_default(id);
    }
}

void FiguredBassItem::regenerate_display_text() {
    // re-generate displayText
    renderer()->layout_item(this);
}

//---------------------------------------------------------
//   UNDOABLE PROPERTY SETTERS
//---------------------------------------------------------

void FiguredBassItem::undo_set_prefix(Modifier pref) {
    if (pref <= Modifier::CROSS) {
        undo_change_property(Pid::FBPREFIX, (int) pref);
        // if setting some prefix and there is a suffix already, clear suffix
        if (pref != Modifier::NONE && suffix_ != Modifier::NONE) {
            undo_change_property(Pid::FBSUFFIX, int(Modifier::NONE));
        }
        regenerate_display_text();
    }
}

void FiguredBassItem::undo_set_digit(int digit) {
    if (digit >= 0 && digit <= 9) {
        undo_change_property(Pid::FBDIGIT, digit);
        regenerate_display_text();
    }
}

void FiguredBassItem::undo_set_suffix(Modifier suff) {
    undo_change_property(Pid::FBSUFFIX, int(suff));
    // if setting some suffix and there is a prefix already, clear prefix
    if (suff != Modifier::NONE && prefix_ != Modifier::NONE) {
        undo_change_property(Pid::FBPREFIX, int(Modifier::NONE));
    }
    regenerate_display_text();
}

void FiguredBassItem::undo_set_cont_line(ContLine val) {
    undo_change_property(Pid::FBCONTINUATIONLINE, int(val));
    regenerate_display_text();
}

void FiguredBassItem::undo_set_parenth1(Parenthesis par) {
    undo_change_property(Pid::FBPARENTHESIS1, int(par));
    regenerate_display_text();
}

void FiguredBassItem::undo_set_parenth2(Parenthesis par) {
    undo_change_property(Pid::FBPARENTHESIS2, int(par));
    regenerate_display_text();
}

void FiguredBassItem::undo_set_parenth3(Parenthesis par) {
    undo_change_property(Pid::FBPARENTHESIS3, int(par));
    regenerate_display_text();
}

void FiguredBassItem::undo_set_parenth4(Parenthesis par) {
    undo_change_property(Pid::FBPARENTHESIS4, int(par));
    regenerate_display_text();
}

void FiguredBassItem::undo_set_parenth5(Parenthesis par) {
    undo_change_property(Pid::FBPARENTHESIS5, int(par));
    regenerate_display_text();
}

//---------------------------------------------------------
//   startsWithParenthesis
//---------------------------------------------------------

bool FiguredBassItem::starts_with_parenthesis() const {
    if (prefix_ != Modifier::NONE) {
        return parenth_[0] != Parenthesis::NONE;
    }
    if (digit_ != ::mu::engraving::fbi_digit_none) {
        return parenth_[1] != Parenthesis::NONE;
    }
    if (suffix_ != Modifier::NONE) {
        return parenth_[2] != Parenthesis::NONE;
    }
    return false;
}

//---------------------------------------------------------
//   F I G U R E D   B A S S
//---------------------------------------------------------

FiguredBass::FiguredBass(DummyParentOr<Segment> parent)
    : TextBase(ElementType::FIGURED_BASS, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    init_element_style(&::mu::engraving::figured_bass_style);
    // figured bass inherits from TextBase for layout purposes
    // but there is no specific text style to use as a basis for styled properties
    // (this is true for historical reasons due to the special layout of figured bass)
    // override the styled property definitions
    for (const auto &p : *text_style(text_style_type())) {
        set_property_flags(p.pid, PropertyFlags::NOSTYLE);
    }
    for (const StyledProperty &p : ::mu::engraving::figured_bass_text_style) {
        set_property_flags(p.pid, PropertyFlags::STYLED);
        set_property(p.pid, style_value(p.pid, p.sid));
    }
    set_on_note(true);
    set_ticks(Fraction(0, 1));
    muse::delete_all(items_);
    items_.clear();
}

FiguredBass::FiguredBass(const FiguredBass &fb) : TextBase(fb) {
    set_on_note(fb.on_note());
    set_ticks(fb.ticks());
    for (auto i : fb.items_) { // deep copy is needed
        FiguredBassItem *fbi = new FiguredBassItem(*i);
        fbi->set_ownership_parent(this);
        items_.push_back(fbi);
    }
    //      items = fb.items;
}

FiguredBass::~FiguredBass() {
    for (FiguredBassItem *item : items_) {
        delete item;
    }
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid FiguredBass::get_property_style(Pid id) const {
    // do not use TextBase::getPropertyStyle
    // as most text style properties do not apply
    for (const StyledProperty &p : ::mu::engraving::figured_bass_text_style) {
        if (p.pid == id) {
            return p.sid;
        }
    }
    return EngravingItem::get_property_style(id);
}

void FiguredBass::regenerate_text() {
    // as the standard text editor keeps inserting spurious HTML formatting and styles
    // retrieve and work only on the plain text
    const String txt = plain_text();
    if (txt.is_empty()) { // if no text, nothing to do
        return;
    }

    // split text into lines and create an item for each line
    StringList list = txt.split(u'\n', muse::SKIP_EMPTY_PARTS);
    clear_items();
    String normalized_text;
    int idx = 0;
    for (String str : list) {
        FiguredBassItem *p_item = new FiguredBassItem(this, idx++);
        if (!p_item->parse(str)) { // if any item fails parsing
            clear_items();
            // TODO: this `startCmd` call is possibly invalid
            score()->start_cmd(TranslatableString("undoableAction", "Regenerate figured bass text"));
            trigger_layout();
            score()->end_cmd();
            delete p_item;
            return;
        }
        add_item_to_linked(p_item);

        // add item normalized text
        if (!normalized_text.is_empty()) {
            normalized_text.append('\n');
        }
        normalized_text.append(p_item->normalized_text());
    }
    // if all items parsed and text is styled, replaced entered text with normalized text
    if (items_.size()) {
        undo_change_property(Pid::TEXT, normalized_text);
    }

    // TODO: this `startCmd` call is possibly invalid
    score()->start_cmd(TranslatableString("undoableAction", "Regenerate figured bass text"));
    trigger_layout();
    score()->end_cmd();
}

//---------------------------------------------------------
//   setSelected /setVisible
//
//    forward flags to items
//---------------------------------------------------------

void FiguredBass::set_selected(bool flag) {
    EngravingItem::set_selected(flag);
    for (FiguredBassItem *item : items_) {
        item->set_selected(flag);
    }
}

void FiguredBass::set_visible(bool flag) {
    EngravingItem::set_visible(flag);
    for (FiguredBassItem *item : items_) {
        item->set_visible(flag);
    }
}

//---------------------------------------------------------
//   nextFiguredBass
//
//    returns the next *contiguous* FiguredBass element if it exists,
//    i.e. the FiguredBass element which starts where 'this' ends
//    returns 0 if none
//---------------------------------------------------------

FiguredBass *FiguredBass::next_figured_bass() const {
    if (ticks_ <= Fraction(0, 1)) { // if _ticks unset, no clear idea of when 'this' ends
        return 0;
    }
    Segment *next_segm;                              // the Segment beyond this' segment
    Fraction next_tick = segment()->tick() + ticks_; // the tick beyond this' duration

    // locate the ChordRest segment right after this' end
    next_segm = score()->tick2segment(next_tick, true, SegmentType::CHORD_REST);
    if (next_segm == 0) {
        return 0;
    }

    // scan segment annotations for an existing FB element in the this' staff
    for (EngravingItem *e : next_segm->annotations()) {
        if (e->is_figured_bass() && e->track() == track()) {
            return to_figured_bass(e);
        }
    }

    return 0;
}

//---------------------------------------------------------
//   additionalContLineX
//
//    if there is a continuation line, without other text elements, at pagePosY, returns its X coord (in page coords)
//    returns 0 if no cont.line there or if there are text elements before the cont.line
//
//    In practice, returns the X coord of a cont. line which can be the continuation of a previous cont. line
//
//    Note: pagePosY is the Y coord of the FiguredBassItem containing the line, not of the line itself,
//    as line position might depend on styles.
//---------------------------------------------------------

double FiguredBass::additional_cont_line_x(double page_pos_y) const {
    PointF pg_pos = page_pos();
    for (FiguredBassItem *fbi : items_) {
        // if item has cont.line but nothing before it
        // and item Y coord near enough to pagePosY
        if (fbi->cont_line() != FiguredBassItem::ContLine::NONE && fbi->digit() == ::mu::engraving::fbi_digit_none &&
            fbi->prefix() == FiguredBassItem::Modifier::NONE && fbi->suffix() == FiguredBassItem::Modifier::NONE &&
            fbi->parenth4() == FiguredBassItem::Parenthesis::NONE && std::abs(pg_pos.y() + fbi->ldata()->pos().y() - page_pos_y) < 0.05) {
            return pg_pos.x() + fbi->ldata()->pos().x();
        }
    }

    return 0.0; // no suitable line
}

//---------------------------------------------------------
//   PROPERTY METHODS
//---------------------------------------------------------

PropertyValue FiguredBass::get_property(Pid property_id) const { return TextBase::get_property(property_id); }

bool FiguredBass::set_property(Pid property_id, const PropertyValue &v) {
    score()->add_refresh(canvas_bounding_rect());
    if (property_id == Pid::TEXT_LINKED_TO_MASTER) {
        if (TextBase::set_property(property_id, v)) {
            regenerate_text();
            return true;
        }
        return false;
    }
    return TextBase::set_property(property_id, v);
}

PropertyValue FiguredBass::property_default(Pid id) const { return TextBase::property_default(id); }

void FiguredBass::clear_items() {
    const std::list<EngravingObject *> links = link_list();
    for (EngravingObject *linked_object : links) {
        if (!linked_object || !linked_object->is_figured_bass()) {
            continue;
        }
        if (linked_object == this) {
            muse::delete_all(items_);
            items_.clear();
        } else {
            bool is_this_text_linked = get_property(Pid::TEXT_LINKED_TO_MASTER).to_bool();
            bool is_other_text_linked = linked_object->get_property(Pid::TEXT_LINKED_TO_MASTER).to_bool();
            if ((score()->is_master() && !is_other_text_linked) || (!score()->is_master() && !is_this_text_linked)) {
                continue;
            }
            FiguredBass *linked_fb = to_figured_bass(linked_object);
            muse::delete_all(linked_fb->items_);
            linked_fb->items_.clear();
        }
    }
}

void FiguredBass::add_item_to_linked(FiguredBassItem *item) {
    const std::list<EngravingObject *> links = link_list();
    for (EngravingObject *linked_object : links) {
        if (!linked_object || !linked_object->is_figured_bass()) {
            continue;
        }
        Score *linked_score = linked_object->score();
        if (linked_object == this) {
            item->set_track(track());
            item->set_ownership_parent(this);
            items_.push_back(item);
            score()->do_undo_add_element(item);
        } else {
            bool is_this_text_linked = get_property(Pid::TEXT_LINKED_TO_MASTER).to_bool();
            bool is_other_text_linked = linked_object->get_property(Pid::TEXT_LINKED_TO_MASTER).to_bool();
            if ((score()->is_master() && !is_other_text_linked) || (!score()->is_master() && !is_this_text_linked)) {
                continue;
            }
            FiguredBass *linked_fb = to_figured_bass(linked_object);
            FiguredBassItem *item_clone = item->clone();

            item_clone->link_to(item);
            item_clone->set_track(linked_fb->track());
            item_clone->set_ownership_parent(linked_fb);
            item_clone->set_score(linked_fb->score());
            linked_fb->append_item(item_clone);
            linked_score->do_undo_add_element(item_clone);
        }
    }
}

//---------------------------------------------------------
//   STATIC FUNCTION
//    adding a new FiguredBass to a Segment;
//    the main purpose of this function is to ensure that ONLY ONE F.b. element exists for each Segment/staff;
//    it either re-uses an existing FiguredBass or creates a new one if none is found;
//    returns the FiguredBass and sets pNew to true if it has been newly created.
//
//    Sets an initial duration of the element up to the next ChordRest of the same staff.
//
//    As the F.b. very concept requires the base chord to have ONLY ONE note,
//    FiguredBass elements are created and looked for only in the first track of the staff.
//---------------------------------------------------------

FiguredBass *FiguredBass::add_figured_bass_to_segment(Segment *seg, track_idx_t track, const Fraction &ext_ticks, bool *p_new) {
    Fraction end_tick;                                   // where this FB is initially assumed to end
    staff_idx_t staff = track / ::mu::engraving::voices; // convert track to staff
    track = staff * ::mu::engraving::voices;             // first track for this staff

    // scan segment annotations for an existing FB element in the same staff
    FiguredBass *fb = 0;
    for (EngravingItem *e : seg->annotations()) {
        if (e->is_figured_bass() && (e->track() / ::mu::engraving::voices) == staff) {
            // an FB already exists in segment: re-use it
            fb = to_figured_bass(e);
            *p_new = false;
            end_tick = seg->tick() + fb->ticks();
            break;
        }
    }
    if (fb == 0) { // no FB at segment: create new
        fb = Factory::create_figured_bass(seg);
        fb->set_track(track);

        // locate next SegChordRest in the same staff to estimate presumed duration of element
        end_tick = Fraction::max();
        Segment *next_segm;
        for (voice_idx_t i_voice = 0; i_voice < ::mu::engraving::voices; i_voice++) {
            next_segm = seg->next_cr(track + i_voice);
            if (next_segm && next_segm->tick() < end_tick) {
                end_tick = next_segm->tick();
            }
        }
        if (end_tick == Fraction::max()) { // no next segment: set up to score end
            Measure *meas = seg->score()->last_measure();
            end_tick = meas->end_tick();
        }
        fb->set_ticks(end_tick - seg->tick());

        // set onNote status
        fb->set_on_note(false);                                                 // assume not onNote
        for (track_idx_t i = track; i < track + ::mu::engraving::voices; i++) { // if segment has chord in staff, set onNote
            if (seg->element(i) && seg->element(i)->is_chord()) {
                fb->set_on_note(true);
                break;
            }
        }
        *p_new = true;
    }

    // if we are extending a previous FB
    if (ext_ticks > Fraction(0, 1)) {
        // locate previous FB for same staff
        Segment *prev_segm;
        FiguredBass *prev_fb = 0;
        for (prev_segm = seg->prev1(SegmentType::DURATION); prev_segm; prev_segm = prev_segm->prev1(SegmentType::DURATION)) {
            for (EngravingItem *e : prev_segm->annotations()) {
                if (e->is_figured_bass() && (e->track()) == track) {
                    prev_fb = to_figured_bass(e); // previous FB found
                    break;
                }
            }
            if (prev_fb) {
                // if previous FB did not stop more than extTicks before this FB...
                Fraction delta = seg->tick() - prev_fb->segment()->tick();
                if (prev_fb->ticks() + ext_ticks >= delta) {
                    prev_fb->set_ticks(delta); // update prev FB ticks to last up to this FB
                }
                break;
            }
        }
    }
    return fb;
}

//---------------------------------------------------------
//   STATIC FUNCTIONS FOR FONT CONFIGURATION MANAGEMENT
//---------------------------------------------------------

bool FiguredBassFont::read(XmlReader &e) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "family") {
            family = e.read_text();
        } else if (tag == "displayName") {
            display_name = e.read_text();
        } else if (tag == "defaultPitch") {
            def_pitch = e.read_double();
        } else if (tag == "defaultLineHeight") {
            def_line_height = e.read_double();
        } else if (tag == "parenthesisRoundOpen") {
            display_parenthesis[1] = e.read_text().at(0);
        } else if (tag == "parenthesisRoundClosed") {
            display_parenthesis[2] = e.read_text().at(0);
        } else if (tag == "parenthesisSquareOpen") {
            display_parenthesis[3] = e.read_text().at(0);
        } else if (tag == "parenthesisSquareClosed") {
            display_parenthesis[4] = e.read_text().at(0);
        } else if (tag == "doubleflat") {
            display_accidental[int(FiguredBassItem::Modifier::DOUBLEFLAT)] = e.read_text().at(0);
        } else if (tag == "flat") {
            display_accidental[int(FiguredBassItem::Modifier::FLAT)] = e.read_text().at(0);
        } else if (tag == "natural") {
            display_accidental[int(FiguredBassItem::Modifier::NATURAL)] = e.read_text().at(0);
        } else if (tag == "sharp") {
            display_accidental[int(FiguredBassItem::Modifier::SHARP)] = e.read_text().at(0);
        } else if (tag == "doublesharp") {
            display_accidental[int(FiguredBassItem::Modifier::DOUBLESHARP)] = e.read_text().at(0);
        } else if (tag == "cross") {
            display_accidental[int(FiguredBassItem::Modifier::CROSS)] = e.read_text().at(0);
        } else if (tag == "backslash") {
            display_accidental[int(FiguredBassItem::Modifier::BACKSLASH)] = e.read_text().at(0);
        } else if (tag == "slash") {
            display_accidental[int(FiguredBassItem::Modifier::SLASH)] = e.read_text().at(0);
        } else if (tag == "digit") {
            int digit = e.int_attribute("value");
            if (digit < 0 || digit > 9) {
                return false;
            }
            while (e.read_next_start_element()) {
                const AsciiStringView t(e.name());
                if (t == "simple") {
                    display_digit[int(FiguredBassItem::Style::MODERN)][digit][int(FiguredBassItem::Combination::SIMPLE)] =
                        e.read_text().at(0);
                } else if (t == "crossed") {
                    display_digit[int(FiguredBassItem::Style::MODERN)][digit][int(FiguredBassItem::Combination::CROSSED)] =
                        e.read_text().at(0);
                } else if (t == "backslashed") {
                    display_digit[int(FiguredBassItem::Style::MODERN)][digit][int(FiguredBassItem::Combination::BACKSLASHED)] =
                        e.read_text().at(0);
                } else if (t == "slashed") {
                    display_digit[int(FiguredBassItem::Style::MODERN)][digit][int(FiguredBassItem::Combination::SLASHED)] =
                        e.read_text().at(0);
                } else if (t == "simpleHistoric") {
                    display_digit[int(FiguredBassItem::Style::HISTORIC)][digit][int(FiguredBassItem::Combination::SIMPLE)] =
                        e.read_text().at(0);
                } else if (t == "crossedHistoric") {
                    display_digit[int(FiguredBassItem::Style::HISTORIC)][digit][int(FiguredBassItem::Combination::CROSSED)] =
                        e.read_text().at(0);
                } else if (t == "backslashedHistoric") {
                    display_digit[int(FiguredBassItem::Style::HISTORIC)][digit][int(FiguredBassItem::Combination::BACKSLASHED)] =
                        e.read_text().at(0);
                } else if (t == "slashedHistoric") {
                    display_digit[int(FiguredBassItem::Style::HISTORIC)][digit][int(FiguredBassItem::Combination::SLASHED)] =
                        e.read_text().at(0);
                } else {
                    e.unknown();
                    return false;
                }
            }
        } else {
            e.unknown();
            return false;
        }
    }
    display_parenthesis[0] = display_accidental[int(FiguredBassItem::Modifier::NONE)] = ' ';
    return true;
}

//---------------------------------------------------------
//   Read Configuration File
//
//    reads a configuration and appends read data to g_FBFonts
//    resets everything and reads the built-in config file if fileName is null or empty
//---------------------------------------------------------

bool FiguredBass::read_config_file(const String &file_name) {
    String path;

    if (file_name.is_empty()) { // defaults to built-in xml
        path = u":/fonts/fonts_figuredbass.xml";
        g_fb_fonts.clear();
    } else {
        path = file_name;
    }

    File fi(path);
    if (!fi.open(IODevice::READ_ONLY)) {
        LOGE() << "Cannot open figured bass description: " << fi.file_path();
        return false;
    }
    XmlReader e(&fi);
    while (e.read_next_start_element()) {
        if (e.name() == "museScore") {
            // String version = e.attribute(String("version"));
            // StringList sl = version.split('.');
            // int _mscVersion = sl[0].toInt() * 100 + sl[1].toInt();

            while (e.read_next_start_element()) {
                if (e.name() == "font") {
                    FiguredBassFont f;
                    if (f.read(e)) {
                        g_fb_fonts.push_back(f);
                    } else {
                        return false;
                    }
                } else {
                    e.unknown();
                }
            }
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   Get Font Names
//
//    returns a list of display names for the fonts  configured to work with Figured Bass;
//    the index of a name in the list can be used to retrieve the font data with fontData()
//---------------------------------------------------------

std::vector<String> FiguredBass::font_names() {
    std::vector<String> names;
    names.reserve(g_fb_fonts.size());
    for (const FiguredBassFont &f : g_fb_fonts) {
        names.push_back(f.display_name);
    }
    return names;
}

//---------------------------------------------------------
//   Get Font Data
//
//    retrieves data about a Figured Bass font.
//    returns: true if idx is valid | false if it is not
// any of the pointer parameter can be null, if that datum is not needed
//---------------------------------------------------------

bool FiguredBass::font_data(int n_idx, String *p_family, String *p_display_name, double *p_size, double *p_line_height) {
    if (n_idx >= 0 && n_idx < static_cast<int>(g_fb_fonts.size())) {
        FiguredBassFont f = g_fb_fonts.at(n_idx);
        if (p_family) {
            *p_family = f.family;
        }
        if (p_display_name) {
            *p_display_name = f.display_name;
        }
        if (p_size) {
            *p_size = f.def_pitch;
        }
        if (p_line_height) {
            *p_line_height = f.def_line_height;
        }
        return true;
    }
    return false;
}

//---------------------------------------------------------
//   hasParentheses
//
//   return true if any FiguredBassItem starts with a parenthesis
//---------------------------------------------------------

bool FiguredBass::parentheses_mode() const {
    for (FiguredBassItem *item : items_) {
        if (item->starts_with_parenthesis()) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   Score::addFiguredBass
//    called from Keyboard Accelerator & menus
//---------------------------------------------------------

FiguredBass *Score::add_figured_bass() {
    EngravingItem *el = selection().element();
    if (!el || (!(el->is_note()) && !(el->is_rest()) && !(el->is_figured_bass()))) {
        MScore::set_error(MsError::NO_NOTE_FIGUREDBASS_SELECTED);
        return 0;
    }

    FiguredBass *fb;
    bool b_new = true;
    if (el->is_note()) {
        ChordRest *cr = to_note(el)->chord();
        fb = FiguredBass::add_figured_bass_to_segment(cr->segment(), cr->staff_idx() * ::mu::engraving::voices, Fraction(0, 1), &b_new);
    } else if (el->is_rest()) {
        ChordRest *cr = to_rest(el);
        fb = FiguredBass::add_figured_bass_to_segment(cr->segment(), cr->staff_idx() * ::mu::engraving::voices, Fraction(0, 1), &b_new);
    } else if (el->is_figured_bass()) {
        fb = to_figured_bass(el);
        b_new = false;
    } else {
        return 0;
    }

    if (fb == 0) {
        return 0;
    }

    if (b_new) {
        undo_add_element(fb);
    }
    select(fb, SelectType::SINGLE, 0);
    return fb;
}
} // namespace mu::engraving
