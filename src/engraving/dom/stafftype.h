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

#include <array>

#include "draw/types/font.h"

#include "stafflabel.h"
#include "../style/styledef.h"
#include "../types/types.h"

namespace mu::engraving {
class Chord;
class ChordRest;
class MStyle;
class Score;
class Staff;

//---------------------------------------------------------
//   TablatureFont
//---------------------------------------------------------

constexpr int num_of_digitfrets = 100;       // the max fret number which can be rendered with numbers
constexpr int num_of_letterfrets = 17;       // the max fret number which can be rendered with letters
constexpr int num_of_bassstring_slashes = 5; // the max number of slashes supported for French bass strings notation
                                             // (currently, only 3 slashes are used at most; another two are
                                             // foreseen for future customizability)

// default values for 'grid'-like beaming to use with value symbols in stemless TAB
constexpr Spatium grid_beam_def_width = 0.25_sp; // all values in sp
constexpr Spatium grid_stem_def_height = 1.75_sp;
constexpr Spatium grid_stem_def_width = 0.125_sp;

struct TablatureFretFont {
    TablatureFretFont();

    String family;                                                             // the family of the physical font to use
    String display_name;                                                       // the name to display to the user
    double def_size = 9.0;                                                     // the default size of the font
    double def_y_offset = 0.0;                                                 // the default Y displacement
    Char x_char = u'X';                                                        // the char to use for 'x'
    std::array<String, ::mu::engraving::num_of_bassstring_slashes> slash_char; // the char used to draw one or more '/' symbols
    std::array<String, ::mu::engraving::num_of_digitfrets> display_digit;      // the string to draw for digit frets
    std::array<Char, ::mu::engraving::num_of_letterfrets> display_letter;      // the char to use for letter frets
};

enum class TabVal : char {
    VAL_LONGA = 0,
    VAL_BREVIS,
    VAL_SEMIBREVIS,
    VAL_MINIMA,
    VAL_SEMIMINIMA,
    VAL_FUSA,
    VAL_SEMIFUSA,
    VAL_32,
    VAL_64,
    VAL_128,
    VAL_256,
    VAL_512,
    VAL_1024,
    NUM_OF
};

enum class TablatureMinimStyle : char {
    NONE = 0, // do not draw half notes at all
    SHORTER,  // draw half notes with a shorter stem
    SLASHED   // draw half notes with stem with two slashes
};

enum class TablatureSymbolRepeat : char {
    NEVER = 0, // never repeat the same duration symbol
    SYSTEM,    // repeat at the beginning of a new system
    MEASURE,   // repeat at the beginning of a new measure
    ALWAYS     // always repeat
};

struct TablatureDurationFont {
    TablatureDurationFont();

    String family;                                                    // the family of the physical font to use
    String display_name;                                              // the name to display to the user
    double def_size = 15.0;                                           // the default size of the font
    double def_y_offset = 0.0;                                        // the default Y displacement
    Spatium grid_beam_width = ::mu::engraving::grid_beam_def_width;   // the width of the 'grid'-style beam (in sp)
    Spatium grid_stem_height = ::mu::engraving::grid_stem_def_height; // the height of the 'grid'-style stem (in sp)
    Spatium grid_stem_width = ::mu::engraving::grid_stem_def_width;   // the width of the 'grid'-style stem (in sp)
    // the note value with no beaming in 'grid'-style beaming
    DurationType zero_beam_level = DurationType::V_QUARTER;
    Char display_dot = u'l';                 // the char to use to draw a dot
    Char display_value[int(TabVal::NUM_OF)]; // the char to use to draw a duration value
};

enum class ShowTiedFret : unsigned char {
    TIE_AND_FRET,
    TIE,
    NONE,
};

enum class ParenthesizeTiedFret : unsigned char {
    START_OF_SYSTEM,
    START_OF_MEASURE,
    NEVER,
};

//---------------------------------------------------------
//   StaffType
//---------------------------------------------------------

class StaffType {
    // The default color of a new staff type, set by initStaffTypes().
    static Color s_default_color;

public:
    StaffType();

    StaffType(StaffGroup sg, StaffTypes staff_type, int lines, int stp_off, double line_dist, bool gen_clef, bool show_bar_lines,
              bool stemless, bool gen_time_sig, bool gen_key_sig, bool show_ledger_liness, bool invisible, const Color &color);

    StaffType(StaffGroup sg, StaffTypes staff_type, int lines, int stp_off, double line_dist, bool gen_clef, bool show_bar_lines,
              bool stemless, bool gen_timesig, bool invisible, const Color &color, const String &dur_font_name, double gen_dur,
              bool fret_font_use_text_style, const String &fret_font_name, TablatureSymbolRepeat sym_repeat, bool lines_through,
              TablatureMinimStyle minim_style, bool on_lines, bool show_rests, bool stems_down, bool stem_through, bool upside_down,
              bool show_tab_fingering, bool use_numbers, bool show_back_tied);

    virtual ~StaffType() = default;

    bool operator==(const StaffType &) const;

    const MStyle &style() const;

    const Score *score() const { return score_; }

    StaffGroup group() const { return group_; }

    void set_group(StaffGroup g) { group_ = g; }

    StaffTypes type() const;

    void set_type(StaffTypes type) { staff_type_ = type; }

    String staff_type_name() const;
    String xml_name() const;
    String translated_group_name() const;

    const StaffLabel &staff_label() const { return staff_label_; }

    StaffLabel &staff_label() { return staff_label_; }

    const String &long_name() const { return staff_label_.long_name(); }

    const String &short_name() const { return staff_label_.short_name(); }

    void set_long_name(const String &s) { staff_label_.set_long_name(s); }

    void set_short_name(const String &s) { staff_label_.set_short_name(s); }

    void set_lines(int val) { lines_ = val; }

    int lines() const { return lines_; }

    int middle_line() const;
    int bottom_line() const;

    void set_step_offset(int v) { step_offset_ = v; }

    int step_offset() const { return step_offset_; }

    void set_line_distance(const Spatium &val) { line_distance_ = val; }

    Spatium line_distance() const { return line_distance_; }

    void set_gen_clef(bool val) { gen_clef_ = val; }

    bool gen_clef() const { return gen_clef_; }

    void set_show_barlines(bool val) { show_barlines_ = val; }

    bool show_barlines() const { return show_barlines_; }

    double user_mag() const { return user_mag_; }

    bool is_small() const { return small_; }

    bool invisible() const { return invisible_; }

    const Color &color() const { return color_; }

    void set_user_mag(double val) { user_mag_ = val; }

    void set_small(bool val) { small_ = val; }

    void set_invisible(bool val) { invisible_ = val; }

    void set_color(const Color &val) { color_ = val; }

    Spatium yoffset() const { return yoffset_; }

    void set_yoffset(Spatium val) { yoffset_ = val; }

    double spatium() const;

    void set_stemless(bool val) { stemless_ = val; }

    bool stemless() const { return stemless_; }

    bool gen_timesig() const { return gen_timesig_; }

    void set_gen_timesig(bool val) { gen_timesig_ = val; }

    // static function to deal with presets
    static const StaffType *get_default_preset(StaffGroup grp);
    static const StaffType *preset(StaffTypes idx);
    static const StaffType *preset_from_xml_name(const String &xml_name);

    void set_gen_keysig(bool val) { gen_keysig_ = val; }

    bool gen_keysig() const { return gen_keysig_; }

    void set_show_ledger_lines(bool val) { show_ledger_lines_ = val; }

    bool show_ledger_lines() const { return show_ledger_lines_; }

    void set_note_head_scheme(NoteHeadScheme s) { note_head_scheme_ = s; }

    NoteHeadScheme note_head_scheme() const { return note_head_scheme_; }

    void set_instrument_label_visibility(InstrumentLabelVisibility v) { instrument_label_visibility_ = v; }

    InstrumentLabelVisibility instrument_label_visibility() const { return instrument_label_visibility_; }

    String fret_string(int fret, int string, bool dead_note) const; // returns a string with the text for fret
    String duration_string(DurationType type, int dots) const;

    // functions to cope with historic TAB's peculiarities, like upside-down, bass string notations
    int phys_string_to_visual(int strg) const;                     // return the string in visual order from physical string
    int visual_string_to_phys(int line) const;                     // return the string in physical order from visual string
    Spatium phys_string_to_y_offset(int strg) const;               // return the string Y offset (in sp, chord-relative)
    String tab_bass_string_prefix(int strg, bool *has_fret) const; // return a string with the prefix, if any, identifying a bass string
    int num_of_tab_ledger_lines(int string) const;

    // properties getters (some getters require updated metrics)
    double duration_box_h() const;
    double duration_box_y() const;

    const muse::draw::Font &duration_font() const { return duration_font_; }

    const TablatureDurationFont &tab_duration_font() const { return duration_fonts_[duration_font_idx_]; }

    const String &duration_font_name() const { return duration_fonts_[duration_font_idx_].display_name; }

    double duration_font_size() const { return duration_font_size_; }

    double duration_font_user_y() const { return duration_font_user_y_; }

    double duration_font_y_offset() const;

    double duration_grid_y_offset() const { return duration_grid_y_offset_; }

    double fret_box_h() const { return fret_box_h_; }

    double dead_fret_box_h() const { return dead_fret_box_h_; }

    double fret_box_y() const;
    double dead_fret_box_y() const;

    // 2 methods to return the size of a box masking lines under a fret mark
    double fret_mask_h() const;
    double fret_mask_y() const;

    const muse::draw::Font &fret_font() const { return fret_font_; }

    const String fret_font_name() const { return fret_font_info_.display_name; }

    double fret_font_size() const { return fret_font_size_; }

    double fret_font_user_y() const { return fret_font_user_y_; }

    double fret_font_y_offset() const;

    bool gen_durations() const { return gen_durations_; }

    bool lines_through() const { return lines_through_; }

    TablatureMinimStyle minim_style() const { return minim_style_; }

    TablatureSymbolRepeat sym_repeat() const { return sym_repeat_; }

    bool on_lines() const { return on_lines_; }

    bool show_rests() const { return show_rests_; }

    bool stems_down() const { return stems_down_; }

    bool stem_through() const { return stems_through_; }

    bool upside_down() const { return upside_down_; }

    bool show_tab_fingering() const { return show_tab_fingering_; }

    bool use_numbers() const { return use_numbers_; }

    bool show_back_tied() const { return show_back_tied_; }

    bool fret_use_text_style() const { return fret_use_text_style_; }

    TextStyleType fret_text_style() const { return fret_text_style_; }

    size_t fret_preset_idx() const { return fret_preset_idx_; }

    // properties setters (setting some props invalidates metrics)
    void set_duration_font_name(const String &);
    void set_duration_font_size(double);

    void set_duration_font_user_y(double val) { duration_font_user_y_ = val; }

    void set_fret_font_size(double);

    void set_fret_font_user_y(double val) { fret_font_user_y_ = val; }

    void set_gen_durations(bool val) { gen_durations_ = val; }

    void set_lines_through(bool val) { lines_through_ = val; }

    void set_minim_style(TablatureMinimStyle val) { minim_style_ = val; }

    void set_symbol_repeat(TablatureSymbolRepeat val) { sym_repeat_ = val; }

    void set_on_lines(bool);

    void set_show_rests(bool val) { show_rests_ = val; }

    void set_stems_down(bool val) { stems_down_ = val; }

    void set_stems_through(bool val) { stems_through_ = val; }

    void set_upside_down(bool val) { upside_down_ = val; }

    void set_show_tab_fingering(bool val) { show_tab_fingering_ = val; }

    void set_use_numbers(bool val);

    void set_show_back_tied(bool val) { show_back_tied_ = val; }

    void set_score(Score *score);

    void set_fret_use_text_style(bool val) { fret_use_text_style_ = val; }

    void set_fret_text_style(const TextStyleType &val);
    void set_fret_preset_idx(size_t idx);
    void set_fret_preset(const String &str);

    bool is_tab_staff() const { return group_ == StaffGroup::TAB; }

    bool is_drum_staff() const { return group_ == StaffGroup::PERCUSSION; }

    bool is_simple_tab_staff() const;
    bool is_common_tab_staff() const;
    bool is_hidden_element_on_tab(Sid common_tab_style, Sid simple_tab_style) const;

    void style_changed();

    // static functions for font config files
    static std::vector<String> tab_font_names(bool b_duration);
    static bool tab_font_data(bool b_duration, size_t n_idx, double &p_size, double &p_y_off);

    static void init_staff_types(const Color &default_color);

    static const std::vector<StaffType> &presets() { return presets_; }

private:

    friend class TabDurationSymbol;

    Score *score_ = nullptr;

    double default_spatium() const;
    void set_duration_metrics();
    void set_fret_metrics();

    static void init_tab_fonts();

    StaffGroup group_ = StaffGroup::STANDARD;

    StaffTypes staff_type_ = StaffTypes::STANDARD;

    StaffLabel staff_label_;

    double user_mag_ = 1.0; // allowed 0.1 - 10.0
    Spatium yoffset_;
    bool small_ = false;
    bool invisible_ = false;
    Color color_;

    int lines_ = 5;
    int step_offset_ = 0;
    Spatium line_distance_ = 1_sp;

    bool show_barlines_ = true;
    bool show_ledger_lines_ = true;
    bool stemless_ = false; // do not show stems

    bool gen_clef_ = true;    // create clef at beginning of system
    bool gen_timesig_ = true; // whether time signature is shown or not
    bool gen_keysig_ = true;  // create key signature at beginning of system

    // Standard: configurable properties
    NoteHeadScheme note_head_scheme_ = NoteHeadScheme::HEAD_NORMAL;
    InstrumentLabelVisibility instrument_label_visibility_ = InstrumentLabelVisibility::AUTO;

    // TAB: configurable propertiesm
    double duration_font_size_ = 15.0;  // the size (in points) for the duration symbol font
    double duration_font_user_y_ = 0.0; // the vertical offset (spatium units) for the duration symb. font
    // user configurable
    double fret_font_size_ = 10.0;  // the size (in points) for the fret marks font
    double fret_font_user_y_ = 0.0; // additional vert. offset of fret marks with respect to
    // the string line (spatium unit); user configurable
    bool gen_durations_ = false;                                  // whether duration symbols are drawn or not
    bool lines_through_ = false;                                  // whether lines for strings and stems may pass through fret marks or not
    TablatureMinimStyle minim_style_ = TablatureMinimStyle::NONE; // how to draw minim stems (stem-and-beam durations only)
    TablatureSymbolRepeat sym_repeat_ = TablatureSymbolRepeat::NEVER; // if and when to repeat the same duration symbol
    bool on_lines_ = true;                                            // whether fret marks are drawn on the string lines or between them
    bool show_rests_ = false;                                         // whether to draw rests or not
    bool stems_down_ = true;                                          // stems are drawn downward (stem-and-beam durations only)
    bool stems_through_ = true;       // stems are drawn through the staff rather than beside it (stem-and-beam durations only)
    bool upside_down_ = false;        // whether lines are drawn with highest string at top (false) or at bottom (true)
    bool show_tab_fingering_ = false; // Allow fingering in tablature staff (true) or not (false)
    bool use_numbers_ = true;         // true: use numbers ('0' - ...) for frets | false: use letters ('a' - ...)
    bool show_back_tied_ = true;      // whether back-tied notes are shown or not

    // TAB: internally managed variables
    // Note: values in RASTER UNITS are independent from score scaling and
    //    must be multiplied by magS() to be used in contexts using sp units
    double duration_box_h_ = 0.0;
    double duration_box_y_ = 0.0;    // the height and the y rect.coord. (relative to staff top line)
                                     // of a box bounding all duration symbols (raster units) internally computed:
                                     // depends upon _onString and the metrics of the duration font
    muse::draw::Font duration_font_; // font used to draw dur. symbols; cached for efficiency
    size_t duration_font_idx_ = 0;   // the index of current dur. font in dur. font array
    double duration_y_offset_ = 0.0; // the vertical offset to draw duration symbols with respect to the
    // string lines (raster units); internally computed: depends upon _onString and duration font
    double duration_grid_y_offset_ = 0.0; // the vertical offset to draw the bottom of duration grid with respect to the
    // string lines (raster units); internally computed: depends upon _onstring and duration font
    double fret_box_h_ = 0.0;
    double fret_box_y_ = 0.0; // the height and the y rect.coord. (relative to staff line)
    double dead_fret_box_h_ = 0.0;
    double dead_fret_box_y_ = 0.0;
    // of a box bounding all fret characters (raster units) internally computed:
    // depends upon _onString, _useNumbers and the metrics of the fret font
    bool fret_use_text_style_ = false;
    TextStyleType fret_text_style_ = TextStyleType::TAB_FRET_NUMBER;
    muse::draw::Font fret_font_; // font used to draw fret marks; cached for efficiency
    TablatureFretFont fret_font_info_;
    size_t fret_preset_idx_ = 0; // the index of current fret font in fret font array
    double fret_y_offset_ = 0.0; // the vertical offset to draw fret marks with respect to the string lines;
    double dead_fret_y_offset_ = 0.0;
    // (raster units); internally computed: depends upon _onString, _useNumbers
    // and the metrics of the fret font

    // the array of configured fonts
    static std::vector<TablatureFretFont> fret_fonts_;
    static std::vector<TablatureDurationFont> duration_fonts_;
    static std::vector<StaffType> presets_;
};
} // namespace mu::engraving
