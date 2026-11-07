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

#include "stafftype.h"

#include "draw/fontmetrics.h"
#include "iengravingconfiguration.h"
#include "mscore.h"
#include "score.h"
#include "staff.h"
#include "style/defaultstyle.h"
#include "style/style.h"
#include "style/textstyle.h"
#include "translation.h"
#include "types/typesconv.h"

using namespace muse::draw;
using namespace muse::io;
using namespace mu::engraving;

namespace mu::engraving {
// HISTORIC TAB BASS STRING NOTATION
// The following constants refer to the specifics of bass string notation in historic
//    (Renaiss./Baroque French and Italian) tablatures.

// how much to lower a bass string note with slashes with respect to line distance (in fraction of line distance)
constexpr double stafftype_tab_bassslash_yoffset = 0.33;

// The following constants could ideally be customizable values;
//    they are currently constants to simplify implementation;
// Note that these constants do not constrain which strings of an instrument are
//    physically frettable (which is defined in the instrument itself) but fix the
//    number of bass strings for which the notation is able to express a fret number
//    rather than simply a string ordinal.
constexpr int num_of_bassstrings_with_letter = 4; // the max number of bass strings frettable with letter notation (French)
constexpr int num_of_bassstrings_with_number = 2; // the max number of bass strings frettable with number notation (Italian)

//---------------------------------------------------------
//   StaffTypeTablature
//---------------------------------------------------------

constexpr double tab_default_dur_yoffs = -1.0;

Color StaffType::s_default_color = Color::black;

std::vector<TablatureFretFont> StaffType::fret_fonts_ = {};
std::vector<TablatureDurationFont> StaffType::duration_fonts_ = {};

//---------------------------------------------------------
//   StaffType
//---------------------------------------------------------

StaffType::StaffType() {
    color_ = s_default_color;
    // set reasonable defaults for type-specific members */
    sym_repeat_ = TablatureSymbolRepeat::NEVER;
    set_duration_font_name(duration_fonts_[0].display_name);
    set_fret_preset_idx(0);
}

StaffType::StaffType(StaffGroup sg, StaffTypes staff_type, int lines, int stp_off, double line_dist, bool gen_clef, bool show_bar_lines,
                     bool stemless, bool gen_time_sig, bool gen_key_sig, bool show_ledger_lines, bool invisible, const Color &color)
    : group_(sg), staff_type_(staff_type), invisible_(invisible), color_(color), lines_(lines), step_offset_(stp_off),
      line_distance_(Spatium(line_dist)), show_barlines_(show_bar_lines), show_ledger_lines_(show_ledger_lines), stemless_(stemless),
      gen_clef_(gen_clef), gen_timesig_(gen_time_sig), gen_keysig_(gen_key_sig) {}

StaffType::StaffType(StaffGroup sg, StaffTypes staff_type, int lines, int stp_off, double line_dist, bool gen_clef, bool show_bar_lines,
                     bool stemless, bool gen_timesig, [[maybe_unused]] bool invisible, const Color &color, const String &dur_font_name,
                     double gen_dur, bool fret_font_use_text_style, const String &fret_font_name, TablatureSymbolRepeat sym_repeat,
                     bool lines_through, TablatureMinimStyle minim_style, bool on_lines, bool show_rests, bool stems_down,
                     bool stem_through, bool upside_down, bool show_tab_fingering, bool use_numbers, bool show_back_tied) {
    color_ = color;
    group_ = sg;
    staff_type_ = staff_type;
    set_lines(lines);
    set_step_offset(stp_off);
    set_line_distance(Spatium(line_dist));
    set_gen_clef(gen_clef);
    set_show_barlines(show_bar_lines);
    set_stemless(stemless);
    set_gen_timesig(gen_timesig);
    set_gen_keysig(sg != StaffGroup::TAB);
    set_duration_font_name(dur_font_name);
    set_gen_durations(gen_dur);
    set_fret_use_text_style(fret_font_use_text_style);
    if (fret_font_use_text_style) {
        set_fret_text_style(TextStyleType::TAB_FRET_NUMBER);
    } else {
        set_fret_preset(fret_font_name);
    }
    set_symbol_repeat(sym_repeat);
    set_lines_through(lines_through);
    set_minim_style(minim_style);
    set_on_lines(on_lines);
    set_show_rests(show_rests);
    set_stems_down(stems_down);
    set_stems_through(stem_through);
    set_upside_down(upside_down);
    set_show_tab_fingering(show_tab_fingering);
    set_use_numbers(use_numbers);
    set_show_back_tied(show_back_tied);
}

//---------------------------------------------------------
//   translatedGroupName
//---------------------------------------------------------

String StaffType::translated_group_name() const { return TConv::translated_user_name(group_); }

int StaffType::middle_line() const { return lines_ - 1 - step_offset_; }

int StaffType::bottom_line() const { return (lines_ - 1) * 2; }

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool StaffType::operator==(const StaffType &st) const {
    bool equal = true;

    equal &= (group_ == st.group_);
    equal &= (staff_type_ == st.staff_type_);
    equal &= (staff_label_ == st.staff_label_);
    equal &= (user_mag_ == st.user_mag_);
    equal &= (yoffset_ == st.yoffset_);
    equal &= (small_ == st.small_);
    equal &= (invisible_ == st.invisible_);
    equal &= (color_ == st.color_);
    equal &= (lines_ == st.lines_);
    equal &= (step_offset_ == st.step_offset_);
    equal &= (line_distance_ == st.line_distance_);
    equal &= (show_barlines_ == st.show_barlines_);
    equal &= (show_ledger_lines_ == st.show_ledger_lines_);
    equal &= (stemless_ == st.stemless_);
    equal &= (gen_clef_ == st.gen_clef_);
    equal &= (gen_timesig_ == st.gen_timesig_);
    equal &= (gen_keysig_ == st.gen_keysig_);
    equal &= (note_head_scheme_ == st.note_head_scheme_);
    equal &= (instrument_label_visibility_ == st.instrument_label_visibility_);
    equal &= (duration_font_size_ == st.duration_font_size_);
    equal &= (duration_font_user_y_ == st.duration_font_user_y_);
    equal &= (fret_font_size_ == st.fret_font_size_);
    equal &= (fret_font_user_y_ == st.fret_font_user_y_);
    equal &= (gen_durations_ == st.gen_durations_);
    equal &= (lines_through_ == st.lines_through_);
    equal &= (minim_style_ == st.minim_style_);
    equal &= (sym_repeat_ == st.sym_repeat_);
    equal &= (on_lines_ == st.on_lines_);
    equal &= (show_rests_ == st.show_rests_);
    equal &= (stems_down_ == st.stems_down_);
    equal &= (stems_through_ == st.stems_through_);
    equal &= (upside_down_ == st.upside_down_);
    equal &= (show_tab_fingering_ == st.show_tab_fingering_);
    equal &= (use_numbers_ == st.use_numbers_);
    equal &= (show_back_tied_ == st.show_back_tied_);
    equal &= (duration_box_h_ == st.duration_box_h_);
    equal &= (duration_box_y_ == st.duration_box_y_);
    equal &= (duration_font_ == st.duration_font_);
    equal &= (duration_font_idx_ == st.duration_font_idx_);
    equal &= (duration_y_offset_ == st.duration_y_offset_);
    equal &= (duration_grid_y_offset_ == st.duration_grid_y_offset_);
    equal &= (fret_box_h_ == st.fret_box_h_);
    equal &= (dead_fret_box_h_ == st.dead_fret_box_h_);
    equal &= (fret_box_y_ == st.fret_box_y_);
    equal &= (dead_fret_box_y_ == st.dead_fret_box_y_);
    equal &= (fret_font_ == st.fret_font_);
    equal &= (fret_font_info_.family == st.fret_font_info_.family);
    equal &= (fret_y_offset_ == st.fret_y_offset_);
    equal &= (fret_use_text_style_ == st.fret_use_text_style_);
    equal &= (fret_text_style_ == st.fret_text_style_);
    equal &= (fret_preset_idx_ == st.fret_preset_idx_);

    return equal;
}

StaffTypes StaffType::type() const { return staff_type_; }

String StaffType::staff_type_name() const { return TConv::translated_user_name(staff_type_); }

String StaffType::xml_name() const { return String::from_ascii(TConv::to_xml(staff_type_).ascii()); }

//---------------------------------------------------------
//   isSimpleTabStaff
//---------------------------------------------------------

bool StaffType::is_simple_tab_staff() const {
    if (!is_tab_staff()) {
        return false;
    }

    StaffTypes st_type = type();

    switch (st_type) {
        case StaffTypes::TAB_4SIMPLE:
        case StaffTypes::TAB_5SIMPLE:
        case StaffTypes::TAB_6SIMPLE:
        case StaffTypes::TAB_7SIMPLE:
        case StaffTypes::TAB_8SIMPLE:
        case StaffTypes::TAB_9SIMPLE:
        case StaffTypes::TAB_10SIMPLE:
        case StaffTypes::TAB_ITALIAN:
        case StaffTypes::TAB_FRENCH: return true;

        default: break;
    }

    return false;
}

//---------------------------------------------------------
//   isCommonTabStaff
//---------------------------------------------------------

bool StaffType::is_common_tab_staff() const { return !is_tab_staff() ? false : !is_simple_tab_staff(); }

//---------------------------------------------------------
//   isHiddenElementOnTab
//---------------------------------------------------------

bool StaffType::is_hidden_element_on_tab(Sid common_tab_style, Sid simple_tab_style) const {
    return (is_common_tab_staff() && !style().style_b(common_tab_style)) || (is_simple_tab_staff() && !style().style_b(simple_tab_style));
}

void StaffType::style_changed() {
    if (!fret_use_text_style_) {
        return;
    }
    set_fret_text_style(fret_text_style_);
}

//---------------------------------------------------------
//   setOnLines
//---------------------------------------------------------

void StaffType::set_on_lines(bool val) {
    on_lines_ = val;
    set_duration_metrics();
    set_fret_metrics();
}

void StaffType::set_use_numbers(bool val) {
    use_numbers_ = val;
    set_fret_metrics();
}

void StaffType::set_fret_text_style(const TextStyleType &val) {
    fret_text_style_ = val;
    fret_font_info_ = TablatureFretFont();
    fret_font_ = Font();

    const TextStyle *ts = text_style(fret_text_style_);

    for (const TextStyleProperty property : *ts) {
        switch (property.type) {
            case TextStylePropertyType::FONT_FACE: {
                String font_name = style().style_st(property.sid);
                fret_font_info_.family = font_name;
                fret_font_.set_family(font_name, Font::Type::TABLATURE);
            } break;
            case TextStylePropertyType::FONT_SIZE: {
                double font_size = style().style_d(property.sid);
                set_fret_font_size(font_size);
            } break;
            case TextStylePropertyType::FONT_STYLE: {
                FontStyle f_style = style().style_v(property.sid).value<FontStyle>();
                fret_font_.set_bold(f_style & FontStyle::BOLD);
                fret_font_.set_italic(f_style & FontStyle::ITALIC);
                fret_font_.set_underline(f_style & FontStyle::UNDERLINE);
                fret_font_.set_strike(f_style & FontStyle::STRIKE);
            } break;
            default: continue;
        }
    }

    PointF offset = style().style_v(ts->offset_sids.above).value<PointF>();
    set_fret_font_user_y(offset.y());

    set_fret_metrics();
}

void StaffType::set_fret_preset_idx(size_t idx) {
    // Clear all previous formatting
    fret_font_ = Font();
    if (idx >= fret_fonts_.size()) {
        fret_preset_idx_ = 0;
        fret_font_info_ = fret_fonts_[0];
    } else {
        fret_preset_idx_ = idx;
        fret_font_info_ = fret_fonts_[idx];
    }

    fret_font_.set_family(fret_font_info_.family, Font::Type::TABLATURE);
    set_fret_font_size(fret_font_info_.def_size);
    set_fret_font_user_y(fret_font_info_.def_y_offset);
    set_fret_metrics();
}

void StaffType::set_fret_preset(const String &name) {
    String loc_name = name;
    // convert old names for two built-in fonts which have changed of name
    if (name == "MuseScore Tab Late Renaiss") {
        loc_name = u"MuseScore Phalèse";
    }
    size_t idx = 0;
    for (idx = 0; idx < fret_fonts_.size(); idx++) {
        if (fret_fonts_[idx].display_name == loc_name) {
            break;
        }
    }

    set_fret_preset_idx(idx);
}

//---------------------------------------------------------
//   setDurationMetrics
//    checks whether the internally computed metrics are is still valid and re-computes them, if not
//---------------------------------------------------------

void StaffType::set_score(Score *score) {
    score_ = score;
    if (score_ && group_ == StaffGroup::TAB) {
        set_duration_metrics();
        set_fret_metrics();
    }
}

void StaffType::set_duration_metrics() {
    if (!score_) {
        return;
    }

    // FontMetrics returns results unreliably rounded to integral pixels;
    // use a scaled up font and then scale computed values down
    Font font(duration_font());
    font.set_point_size_f(duration_font_size_);
    FontMetrics fm(font);
    String txt(duration_fonts_[duration_font_idx_].display_value, size_t(TabVal::NUM_OF));
    RectF bb(fm.tight_bounding_rect(txt));
    // raise symbols by a default margin and, if marks are above lines, by half the line distance
    // (converted from spatium units to raster units)
    duration_grid_y_offset_ =
        (::mu::engraving::tab_default_dur_yoffs - (on_lines_ ? 0.0 : line_distance().val() * 0.5)) * default_spatium();
    // this is the bottomest point of any duration sign
    duration_y_offset_ = duration_grid_y_offset_;
    // move symbols so that the lowest margin 'sits' on the base line:
    // move down by the whole part above (negative) the base line
    // ( -bb.y() ) then up by the whole height ( -bb.height() )
    duration_y_offset_ -= (bb.height() + bb.y()) / 100.0;
    duration_box_h_ = bb.height() / 100.0;
    duration_box_y_ = duration_grid_y_offset_ - bb.height() / 100.0;
}

void StaffType::set_fret_metrics() {
    if (!score_) {
        return;
    }

    FontMetrics fm(fret_font());
    RectF bb;
    // compute vertical displacement
    if (use_numbers_) {
        // compute total height of used characters
        String txt;
        for (int idx = 0; idx < 10; idx++) { // use only first 10 digits
            txt.append(fret_font_info_.display_digit[idx]);
        }
        bb = fm.tight_bounding_rect(txt);
        // for numbers: centre on '0': move down by the whole part above (negative)
        // the base line ( -bb.y() ) then up by half the whole height ( -bb.height()/2 )
        RectF bx(fm.tight_bounding_rect(fret_font_info_.display_digit[0]));
        fret_y_offset_ = -(bx.y() + bx.height() / 2.0);
        // _fretYOffset = -(bb.y() + bb.height()/2.0);  // <- using bbox of all chars
    } else {
        // compute total height of used characters
        const String txt(fret_font_info_.display_letter.data(), ::mu::engraving::num_of_letterfrets);
        bb = fm.tight_bounding_rect(txt);
        // for letters: centre on the 'a' ascender, by moving down half of the part above the base line in bx
        RectF bx(fm.tight_bounding_rect(fret_font_info_.display_letter[0]));
        fret_y_offset_ = -bx.y() / 2.0;
    }

    // Calculate position for dead fret marks - these must be centred separately based on their glyph
    RectF dead_bb = fm.tight_bounding_rect(fret_font_info_.x_char);
    double line_thickness = style().style_s(Sid::STAFF_LINE_WIDTH).val() * default_spatium() * 0.5;
    dead_fret_y_offset_ = -dead_bb.y() / 2.0 + line_thickness;

    // if on string, we are done; if between strings, raise by half line distance
    if (!on_lines_) {
        double line_adj = line_distance().val() * default_spatium() * 0.5;
        fret_y_offset_ -= line_adj;
        dead_fret_y_offset_ -= line_adj;
    }

    // from _fretYOffset, compute _fretBoxH and _fretBoxY
    fret_box_h_ = bb.height();
    fret_box_y_ = bb.y() + fret_y_offset_;

    dead_fret_box_h_ = dead_bb.height();
    dead_fret_box_y_ = dead_bb.y() + dead_fret_y_offset_;
}

//---------------------------------------------------------
//   setDurationFontName
//---------------------------------------------------------

void StaffType::set_duration_font_name(const String &name) {
    size_t idx;
    for (idx = 0; idx < duration_fonts_.size(); idx++) {
        if (duration_fonts_[idx].display_name == name) {
            break;
        }
    }
    if (idx >= duration_fonts_.size()) {
        idx = 0; // if name not found, use first font
    }
    duration_font_idx_ = idx;
    duration_font_.set_family(duration_fonts_[idx].family, Font::Type::TABLATURE);
    set_duration_font_size(duration_fonts_[idx].def_size);
    set_duration_font_user_y(duration_fonts_[idx].def_y_offset);
    set_duration_metrics();
}

//---------------------------------------------------------
//   durationBoxH / durationBoxY
//---------------------------------------------------------

double StaffType::default_spatium() const { return StyleDef::style_values[static_cast<size_t>(Sid::SPATIUM)].default_value.to_double(); }

double StaffType::duration_box_h() const {
    if (!gen_durations_ && !stemless_) {
        return 0.0;
    }
    return duration_box_h_;
}

double StaffType::duration_box_y() const {
    if (!gen_durations_ && !stemless_) {
        return 0.0;
    }
    return duration_box_y_ + duration_font_user_y_ * default_spatium();
}

double StaffType::duration_font_y_offset() const { return duration_y_offset_ + duration_font_user_y_ * default_spatium(); }

double StaffType::fret_box_y() const { return fret_box_y_ + fret_font_user_y_ * default_spatium(); }

double StaffType::dead_fret_box_y() const { return dead_fret_box_y_ + fret_font_user_y_ * default_spatium(); }

double StaffType::fret_mask_h() const { return line_distance_.val() * default_spatium(); }

double StaffType::fret_mask_y() const { return (on_lines_ ? -0.5 : -1.0) * line_distance_.val() * default_spatium(); }

double StaffType::fret_font_y_offset() const { return fret_y_offset_ + fret_font_user_y_ * default_spatium(); }

//---------------------------------------------------------
//   setDurationFontSize / setFretFontSize
//---------------------------------------------------------

void StaffType::set_duration_font_size(double val) {
    duration_font_size_ = val;
    duration_font_.set_point_size_f(val);
    set_duration_metrics();
}

void StaffType::set_fret_font_size(double val) {
    fret_font_size_ = val;
    fret_font_.set_point_size_f(val);
    set_fret_metrics();
}

//---------------------------------------------------------
//   fretString / durationString
//
//    construct the text string for a given fret / duration
//---------------------------------------------------------

static const String unknown_fret = String(u"?");

String StaffType::fret_string(int fret, int string, bool dead_note) const {
    if (fret == ::mu::engraving::invalid_fret_index) {
        return ::mu::engraving::unknown_fret;
    }
    if (dead_note) {
        return String(fret_font_info_.x_char);
    } else {
        bool has_fret;
        String text = tab_bass_string_prefix(string, &has_fret);
        if (!has_fret) { // if the notation does not allow to fret this string,
            return text; // return the prefix only
        }
        // otherwise, add to prefix the relevant digit/letter string
        return text +
               (use_numbers_ ?
                    (fret >= ::mu::engraving::num_of_digitfrets ? ::mu::engraving::unknown_fret : fret_font_info_.display_digit[fret]) :
                    (fret >= ::mu::engraving::num_of_letterfrets ? ::mu::engraving::unknown_fret : fret_font_info_.display_letter[fret]));
    }
}

String StaffType::duration_string(DurationType type, int dots) const {
    String s = duration_fonts_[duration_font_idx_].display_value[size_t(type)];
    for (int count = 0; count < dots; count++) {
        s.append(duration_fonts_[duration_font_idx_].display_dot);
    }
    return s;
}

//---------------------------------------------------------
//    tabBassStringPrefix
//
//    returns a String (possibly empty) with the prefix identifying a bass string in TAB's;
//    can deal with non-bass strings (i.e. regular TAB lines).
//
//    Implements the specifics of historic notations for bass lines (i.e. strings outside
//    the lines of the tab), both Italian and French.
//
//    strg   the instrument physical string ordinal (0 = topmost string, may exceed the number
//                of lines actually present in the TAB to reference a bass string)
//    bool   pntr to a bool receiving the info if notation allows to express a fret number or not
//                (this is potentially different from the fact that the instrument string itself can be fretted or not)
//---------------------------------------------------------

String StaffType::tab_bass_string_prefix(int strg, bool *has_fret) const {
    *has_fret = true; // assume notation allows to fret this string
    int bass_strg_idx = (strg >= lines_ ? strg - lines_ + 1 : 0);
    if (use_numbers_) {
        // if above the max bass string which can be fretted with number notation
        // return a number with the string index
        if (bass_strg_idx > ::mu::engraving::num_of_bassstrings_with_number) {
            *has_fret = false;
            return fret_font_info_.display_digit[strg + 1];
        }
        // if a frettable bass string, return an empty string
        return String();
    } else {
        // bass string notation
        // if above the max bass string which can be fretted with letter notation
        // return a number with the bass string index itself
        if (bass_strg_idx > ::mu::engraving::num_of_bassstrings_with_letter) {
            *has_fret = false;
            return fret_font_info_.display_digit[bass_strg_idx - 1];
        }
        // if a frettable bass string, return a character with the relevant num. of slashes;
        // note that the number of slashes is bassStrgIdx-1 (1st bass has no slash)
        // and slashChar[] is 0-based (slashChar[0] => 1 slash, ...), whence the -2
        String prefix = bass_strg_idx > 1 ? String(fret_font_info_.slash_char[bass_strg_idx - 2]) : String();
        return prefix;
    }
}

//---------------------------------------------------------
//   numOfLedgerLines
//
//    in TAB's, returns the number of ledgerlines needed by bass lines in some TAB styles.
//
//    Returns 0 if staff is not a TAB, if a TAB but style does not use ledger lines
//    or ledger lines do not apply to the given string.
//---------------------------------------------------------

int StaffType::num_of_tab_ledger_lines(int string) const {
    if (group_ != StaffGroup::TAB || !use_numbers_) {
        return 0;
    }

    int num_of_ledgers = string < 0 ? -string : string - lines_ + 1;
    return num_of_ledgers >= 1 && num_of_ledgers <= ::mu::engraving::num_of_bassstrings_with_number ? num_of_ledgers : 0;
}

//---------------------------------------------------------
//   physStringToVisual / visualStringToPhys
//
//    returns the string ordinal in visual order (top to down) from a string ordinal in physical order
//    or vice-versa: manages upsideDown
//---------------------------------------------------------

int StaffType::phys_string_to_visual(int strg) const {
    if (strg < 0) { // if above top string, return top string
        strg = 0;
    }
    //      // NO: bass strings may exist, which are in addition to tab string lines
    //      if (strg >= _lines)                 // if physical string has no visual representation,
    //            strg = _lines - 1;            // reduce to nearest visual line
    // if TAB upside down, flip around top line
    return upside_down_ ? lines_ - 1 - strg : strg;
}

int StaffType::visual_string_to_phys(int line) const {
    // if TAB upside down, reverse string number
    line = (upside_down_ ? lines_ - 1 - line : line);

    if (line < 0) { // if above top string, reduce to top string
        line = 0;
    }
    // NO: bass strings may exist, which are in addition to tab string lines
    //      if (line >= _lines)
    //            line = _lines - 1;
    return line;
}

//---------------------------------------------------------
//   physStringToYOffset
//
//    returns the string Y offset from a string ordinal in physical order:
//    manages upsideDown and extra bass strings.
//
//    The returned values is in sp. and is relative to the staff top line.
//
//    Note: the difference with physStringToVisual() is that this function takes into account
//          peculiarities of bass string notations.
//---------------------------------------------------------

Spatium StaffType::phys_string_to_y_offset(int strg) const {
    double y_offset = strg; // the y offset of the visual string, as a multiple of line distance
    if (y_offset < 0) {     // if above top physical string, limit to top string
        y_offset = 0;
    }
    if (y_offset >= lines_) { // if physical string 'below' tab lines,
        y_offset = lines_;    // reduce to first string 'below' tab body
        if (!use_numbers_) {  // with letters, add some space for the slashes ascender
            y_offset = on_lines_ ? lines_ : lines_ + ::mu::engraving::stafftype_tab_bassslash_yoffset;
        }
    }
    // if TAB upside down, flip around top line
    y_offset = upside_down_ ? (double) (lines_ - 1) - y_offset : y_offset;
    return y_offset * line_distance_;
}

//---------------------------------------------------------
//   STATIC FUNCTIONS FOR FONT CONFIGURATION MANAGEMENT
//---------------------------------------------------------

TablatureFretFont::TablatureFretFont() {
    // Set up defaults
    for (size_t i = 0; i < ::mu::engraving::num_of_digitfrets; i++) {
        display_digit.at(i) = String::number(i);
    }

    for (size_t i = 0; i < ::mu::engraving::num_of_letterfrets; i++) {
        display_letter[i] = Char(u'a' + static_cast<char16_t>(i));
    }

    for (size_t i = 0; i < ::mu::engraving::num_of_bassstring_slashes; i++) {
        for (size_t j = 0; j < i; j++) {
            slash_char.at(i).append(u"/");
        }
    }
}

TablatureDurationFont::TablatureDurationFont() {
    for (size_t i = 0; i < size_t(TabVal::NUM_OF); i++) {
        display_value[i] = Char(u'a' + static_cast<char16_t>(i));
    }
}

//---------------------------------------------------------
//   Tablature font presets
//---------------------------------------------------------

static TablatureFretFont make_fret_font_preset(const String &family, const String &display_name) {
    // Create fret font from a regular font
    TablatureFretFont f;
    f.family = family;
    f.display_name = display_name;
    return f;
}

static TablatureFretFont make_muse_score_fret_font_preset(const String &family, const String &display_name) {
    // Create fret font from a specialist MuseScore tablature font
    TablatureFretFont f;
    f.family = family;
    f.display_name = display_name;
    f.def_size = 10.0;
    f.display_digit[10] = String(u"X");
    for (size_t i = 0; i < ::mu::engraving::num_of_bassstring_slashes; ++i) {
        f.slash_char[i] = String(Char(u'A' + static_cast<char16_t>(i)));
    }
    return f;
}

static TablatureDurationFont make_duration_font_preset(const String &family, const String &display_name,
                                                       Spatium beam_width = ::mu::engraving::grid_beam_def_width,
                                                       Spatium stem_height = ::mu::engraving::grid_stem_def_height,
                                                       Spatium stem_width = ::mu::engraving::grid_stem_def_width,
                                                       DurationType zero_beam_level = DurationType::V_QUARTER) {
    TablatureDurationFont f;
    f.family = family;
    f.display_name = display_name;
    f.grid_beam_width = beam_width;
    f.grid_stem_height = stem_height;
    f.grid_stem_width = stem_width;
    f.zero_beam_level = zero_beam_level;
    return f;
}

void StaffType::init_tab_fonts() {
    // keep displayName strings and the order/count of these presets unchanged:
    // saved scores reference a preset by displayName (<460) or by
    // positional index (>=460)
    fret_fonts_ = {
        make_fret_font_preset(u"FreeSans", u"MuseScore Tab Sans"),
        make_fret_font_preset(u"FreeSerif", u"MuseScore Tab Serif"),
        make_muse_score_fret_font_preset(u"MuseScoreTabRenaiss", u"MuseScore Tab Renaiss"),
        make_muse_score_fret_font_preset(u"MuseScoreTabPhalese", u"MuseScore Phalèse"),
        make_muse_score_fret_font_preset(u"MuseScoreTabBonneuilDeVisee", u"MuseScore Bonneuil-de Visée"),
        make_muse_score_fret_font_preset(u"MuseScoreTabBonneuilGaultier", u"MuseScore Bonneuil-Gaultier"),
        make_muse_score_fret_font_preset(u"MuseScoreTabDowland", u"MuseScore Dowland"),
        make_muse_score_fret_font_preset(u"MuseScoreTabLuteDidactic", u"MuseScore Lute Didactic"),
    };

    duration_fonts_ = {
        make_duration_font_preset(u"MuseScoreTabModern", u"MuseScore Tab Modern", 0.5_sp, 3.0_sp, 0.1_sp, DurationType::V_QUARTER),
        make_duration_font_preset(u"MuseScoreTabItalian", u"MuseScore Tab Italian", 0.15_sp, 1.75_sp, 0.15_sp, DurationType::V_WHOLE),
        make_duration_font_preset(u"MuseScoreTabFrench", u"MuseScore Tab French", 0.30_sp, 3.125_sp, 0.21_sp, DurationType::V_QUARTER),
        make_duration_font_preset(u"MuseScoreTabFrenchBaroqueHeadless", u"MuseScore French Baroque (headless)"),
        make_duration_font_preset(u"MuseScoreTabFrenchBaroque", u"MuseScore French Baroque"),
    };
}

//---------------------------------------------------------
//   fontNames
//
//    returns a list of display names for the fonts  configured to work with Tablatures;
//    the index of a name in the list can be used to retrieve the font data with fontData()
//---------------------------------------------------------

std::vector<String> StaffType::tab_font_names(bool b_duration) {
    std::vector<String> names;
    if (b_duration) {
        names.reserve(duration_fonts_.size());
        for (const TablatureDurationFont &f : duration_fonts_) {
            names.push_back(f.display_name);
        }
    } else {
        names.reserve(fret_fonts_.size());
        for (const TablatureFretFont &f : fret_fonts_) {
            names.push_back(f.display_name);
        }
    }
    return names;
}

//---------------------------------------------------------
//   fontData
//
//    retrieves data about a Tablature font.
//    returns: true if idx is valid | false if it is not
//---------------------------------------------------------

bool StaffType::tab_font_data(bool b_duration, size_t n_idx, double &p_size, double &p_y_off) {
    if (b_duration) {
        if (n_idx < duration_fonts_.size()) {
            TablatureDurationFont f = duration_fonts_.at(n_idx);
            p_size = f.def_size;
            p_y_off = f.def_y_offset;
            return true;
        }
    } else {
        TablatureFretFont f = n_idx < fret_fonts_.size() ? fret_fonts_.at(n_idx) : TablatureFretFont();
        p_size = f.def_size;
        p_y_off = f.def_y_offset;
        return true;
    }
    return false;
}

const MStyle &StaffType::style() const {
    if (!score_) {
        return DefaultStyle::default_style();
    }

    return score_->style();
}

//---------------------------------------------------------
//   spatium
//---------------------------------------------------------

double StaffType::spatium() const { return style().spatium() * (is_small() ? style().style_d(Sid::SMALL_STAFF_MAG) : 1.0) * user_mag(); }

//=========================================================
//
//   BUILT-IN STAFF TYPES and STAFF TYPE PRESETS
//
//=========================================================

static const int default_preset[::mu::engraving::staff_group_max] = {
    0, // default pitched preset is "stdNormal"
    4, // default percussion preset is "perc5lines"
    5  // default tab preset is "tab6StrCommon"
};

//---------------------------------------------------------
//   Static functions for StaffType presets
//---------------------------------------------------------

const StaffType *StaffType::preset(StaffTypes idx) {
    if (int(idx) < 0 || int(idx) >= int(presets_.size())) {
        return &presets_[0];
    }

    return &presets_[int(idx)];
}

const StaffType *StaffType::preset_from_xml_name(const String &xml_name) {
    for (size_t i = 0; i < presets_.size(); ++i) {
        if (presets_[i].xml_name() == xml_name) {
            return &presets_[i];
        }
    }

    return nullptr;
}

const StaffType *StaffType::get_default_preset(StaffGroup grp) {
    int idx_value = ::mu::engraving::default_preset[int(grp)];
    return &presets_[idx_value];
}

//---------------------------------------------------------
//   initStaffTypes
//---------------------------------------------------------

std::vector<StaffType> StaffType::presets_;

/* *INDENT-OFF* */
void StaffType::init_staff_types(const Color &default_color) {
    s_default_color = default_color;
    init_tab_fonts(); // set up TAB font presets, before initStaffTypes()

    // keep in sync with enum class StaffTypes
    presets_ = {
        //                       group,              staff type,                    lin stpOff  dist clef   bars stmless time  key    ledger invis     color
        StaffType(StaffGroup::STANDARD, StaffTypes::STANDARD, 5, 0, 1, true, true, false, true, true, true, false, default_color),
        StaffType(StaffGroup::PERCUSSION, StaffTypes::PERC_1LINE, 1, 0, 1, true, true, false, true, false, true, false, default_color),
        StaffType(StaffGroup::PERCUSSION, StaffTypes::PERC_2LINE, 2, 0, 1, true, true, false, true, false, true, false, default_color),
        StaffType(StaffGroup::PERCUSSION, StaffTypes::PERC_3LINE, 3, 0, 1, true, true, false, true, false, true, false, default_color),
        StaffType(StaffGroup::PERCUSSION, StaffTypes::PERC_5LINE, 5, 0, 1, true, true, false, true, false, true, false, default_color),

        //                 group            staff type,               lin stpOff dist clef   bars stemless time  invis     color       duration font       genDur textStyle fret font                               duration symbol repeat       thru    minim style                  onLin  rests  stmDn  stmThr upsDn  sTFing nums  bkTied
        StaffType(StaffGroup::TAB, StaffTypes::TAB_6SIMPLE, 6, 0, 1.5, true, true, true, false, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_6COMMON, 6, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_6FULL, 6, 0, 1.5, true, true, false, true, false, default_color, u"MuseScore Tab Modern",
                  false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false, TablatureMinimStyle::SLASHED, true, true, true,
                  true, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_4SIMPLE, 4, 0, 1.5, true, true, true, false, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_4COMMON, 4, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_4FULL, 4, 0, 1.5, true, true, false, true, false, default_color, u"MuseScore Tab Modern",
                  false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false, TablatureMinimStyle::SLASHED, true, true, true,
                  true, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_5SIMPLE, 5, 0, 1.5, true, true, true, false, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_5COMMON, 5, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_5FULL, 5, 0, 1.5, true, true, false, true, false, default_color, u"MuseScore Tab Modern",
                  false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false, TablatureMinimStyle::SLASHED, true, true, true,
                  true, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_UKULELE, 4, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_BALALAJKA, 3, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_DULCIMER, 3, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, true, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_ITALIAN, 6, 0, 1.5, false, true, true, true, false, default_color,
                  u"MuseScore Tab Italian", true, false, u"MuseScore Tab Renaiss", TablatureSymbolRepeat::NEVER, true,
                  TablatureMinimStyle::NONE, true, true, false, false, true, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_FRENCH, 6, 0, 1.5, false, true, true, true, false, default_color,
                  u"MuseScore Tab French", true, false, u"MuseScore Tab Renaiss", TablatureSymbolRepeat::NEVER, true,
                  TablatureMinimStyle::NONE, false, false, false, false, false, false, false, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_7COMMON, 7, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_8COMMON, 8, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_9COMMON, 9, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_10COMMON, 10, 0, 1.5, true, true, false, true, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::SHORTER, true, true, true, false, false, true, true, true),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_7SIMPLE, 7, 0, 1.5, true, true, true, false, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_8SIMPLE, 8, 0, 1.5, true, true, true, false, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_9SIMPLE, 9, 0, 1.5, true, true, true, false, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
        StaffType(StaffGroup::TAB, StaffTypes::TAB_10SIMPLE, 10, 0, 1.5, true, true, true, false, false, default_color,
                  u"MuseScore Tab Modern", false, true, u"MuseScore Tab Sans", TablatureSymbolRepeat::NEVER, false,
                  TablatureMinimStyle::NONE, true, false, true, false, false, false, true, false),
    };
}

/* *INDENT-ON* */
} // namespace mu::engraving
