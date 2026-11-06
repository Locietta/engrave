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

#include "global/serialization/xmlstreamreader.h"
#include "musicxmltupletstate.h"
#include "musicxmlpart.h"
#include "engraving/engravingerrors.h"

namespace mu::engraving {
struct Fraction;
struct Score;
struct Part;
struct VBox;
} // namespace mu::engraving

namespace mu::iex::musicxml {
struct VoiceOverlapDetector;

//---------------------------------------------------------
//   PageFormat
//---------------------------------------------------------

struct PageFormat {
    engraving::SizeF size;         // automatically initialized (to invalid)
    double printable_width = 5;    // _width - left margin - right margin
    double even_left_margin = 0.2; // values in inch
    double odd_left_margin = 0.2;
    double even_top_margin = 0.2;
    double even_bottom_margin = 0.2;
    double odd_top_margin = 0.2;
    double odd_bottom_margin = 0.2;
    bool twosided = false;
};

typedef std::pair<int, int> StartStop;
typedef std::vector<StartStop> StartStopList;

//---------------------------------------------------------
//   MusicXmlOctaveShiftDesc
//---------------------------------------------------------

struct MusicXmlOctaveShiftDesc {
    enum class Type : char { UP, DOWN, STOP, NONE };
    Type tp;
    short size;
    engraving::Fraction time;
    short num;

    MusicXmlOctaveShiftDesc() : tp(Type::NONE), size(0), num(-1) {}

    MusicXmlOctaveShiftDesc(Type input_tp, short input_size, engraving::Fraction tm) : tp(input_tp), size(input_size), time(tm), num(-1) {}
};

//---------------------------------------------------------
//   MusicXmlPartGroup
//---------------------------------------------------------

struct MusicXmlPartGroup {
    int span = 0;
    int start = 0;
    engraving::BracketType type = engraving::BracketType::NO_BRACKET;
    bool barline_span = false;
    muse::draw::Color color;
    size_t column = 0;
};

typedef std::vector<MusicXmlPartGroup *> MusicXmlPartGroupList;
typedef std::map<muse::String, engraving::Part *> PartMap;
typedef std::map<int, MusicXmlPartGroup *> MusicXmlPartGroupMap;

//---------------------------------------------------------
//   CreditWords
//    a single parsed MusicXML credit-words element
//---------------------------------------------------------

struct CreditWords {
    int page = 0;
    muse::String type;
    double default_x = 0.0;
    double default_y = 0.0;
    double font_size = 0.0;
    muse::String justify;
    muse::String h_align;
    muse::String v_align;
    muse::String words;

    CreditWords(int p, muse::String tp, double dx, double dy, double fs, muse::String j, muse::String ha, muse::String va, muse::String w) {
        page = p;
        type = tp;
        default_x = dx;
        default_y = dy;
        font_size = fs;
        justify = j;
        h_align = ha;
        v_align = va;
        words = w;
    }
};

typedef std::vector<CreditWords *> CreditWordsList;

//---------------------------------------------------------
//   declarations
//---------------------------------------------------------

bool is_likely_credit_text(const muse::String &text, const bool case_insensitive);
bool is_likely_subtitle_text(const muse::String &text, const bool case_insensitive);

//---------------------------------------------------------
//   MusicXmlParserPass1
//---------------------------------------------------------

struct MusicXmlLogger;
struct IMusicXmlConfiguration;

struct MusicXmlParserPass1 {
    MusicXmlParserPass1(engraving::Score *score, const IMusicXmlConfiguration &configuration, MusicXmlLogger *logger);

    const IMusicXmlConfiguration *configuration() const { return &configuration_; }

    void init_part_state(const muse::String &part_id);
    engraving::Err parse(const muse::ByteArray &data);
    engraving::Err parse();

    muse::String errors() const { return errors_; }

    void score_partwise();
    void identification();
    void credit(CreditWordsList &credits);
    void defaults();
    void page_layout(PageFormat &pf, const double conversion);
    void part_list(MusicXmlPartGroupList &part_group_list);
    void part_group(const int score_parts, MusicXmlPartGroupList &part_group_list, MusicXmlPartGroupMap &part_groups,
                    muse::String &cur_part_group_name);
    void score_part(const muse::String &cur_part_group_name);
    void score_instrument(const muse::String &part_id, const muse::String &cur_part_group_name);
    void set_style(const muse::String &type, const double val);
    void midi_instrument(const muse::String &part_id);
    void part();
    void measure(const muse::String &part_id, const engraving::Fraction c_time, engraving::Fraction &mdur, VoiceOverlapDetector &vod,
                 const int measure_nr);
    void print(const int measure_nr);
    void attributes(const muse::String &part_id, const engraving::Fraction c_time);
    void clef(const muse::String &part_id);
    void time(const engraving::Fraction c_time);
    void transpose(const muse::String &part_id, const engraving::Fraction &tick);
    void divisions();
    void direction(const muse::String &part_id, const engraving::Fraction &c_time);
    void direction_type(const engraving::Fraction c_time, std::vector<MusicXmlOctaveShiftDesc> &starts,
                        std::vector<MusicXmlOctaveShiftDesc> &stops);
    void handle_octave_shift(const engraving::Fraction &c_time, const muse::String &type, short size, MusicXmlOctaveShiftDesc &desc);
    void notations(MusicXmlStartStop &tuplet_start_stop);
    void note(const muse::String &part_id, const engraving::Fraction &c_time, engraving::Fraction &missing_prev, engraving::Fraction &dura,
              engraving::Fraction &missing_curr, VoiceOverlapDetector &vod, MusicXmlTupletStates &tuplet_states);
    void note_print_spacing_no(engraving::Fraction &dura);
    engraving::Fraction calc_ticks(const int &int_ticks, const int &input_divisions, const muse::XmlStreamReader *xml_reader);

    engraving::Fraction calc_ticks(const int &int_ticks) { return calc_ticks(int_ticks, divs_, &e_); }

    void duration(engraving::Fraction &dura, muse::XmlStreamReader &e);

    void duration(engraving::Fraction &dura) { duration(dura, e_); }

    void forward(engraving::Fraction &dura);
    void backup(engraving::Fraction &dura);
    void time_modification(engraving::Fraction &time_mod);
    void pitch(int &step, float &alter, int &oct);
    void rest();
    void skip_log_curr_elem();
    bool determine_measure_length(std::vector<engraving::Fraction> &ml) const;
    VoiceList get_voice_list(const muse::String &id) const;
    bool determine_staff_move_voice(const muse::String &id, const int mx_staff, const int &mx_voice, int &ms_move, int &ms_track,
                                    int &ms_voice) const;
    int voice_to_int(const muse::String &voice);
    engraving::track_idx_t track_for_part(const muse::String &id) const;
    bool has_part(const muse::String &id) const;

    engraving::Part *get_part(const muse::String &id) const { return muse::value(part_map_, id); }

    MusicXmlPart get_music_xml_part(const muse::String &id) const { return muse::value(parts_, id); }

    size_t nparts() const { return parts_.size(); }

    MusicXmlInstruments get_instruments(const muse::String &id) const { return muse::value(instruments_, id); }

    void set_drumset_default(const muse::String &id, const muse::String &instr_id, const engraving::NoteHeadGroup hg, const int line,
                             const engraving::DirectionV sd);
    MusicXmlInstrList get_instr_list(const muse::String &id) const;
    MusicXmlIntervalList get_intervals(const muse::String &id) const;
    engraving::Fraction get_measure_start(const size_t i) const;
    int octave_shift(const muse::String &id, const engraving::staff_idx_t staff, const engraving::Fraction &f) const;

    const CreditWordsList &credits() const { return credits_; }

    bool has_beaming_info() const { return has_beaming_info_; }

    bool is_vocal_staff(const muse::String &part_id) const { return parts_.at(part_id).is_vocal_staff(); }

    bool is_percussion_staff(const muse::String &part_id) const { return parts_.at(part_id).is_percussion_staff(); }

    static engraving::VBox *create_and_add_v_box_for_credit_words(engraving::Score *score, engraving::Fraction tick);
    void create_default_header(engraving::Score *const score);
    void create_measures_and_vboxes(engraving::Score *const score, const std::vector<engraving::Fraction> &ml,
                                    const std::vector<engraving::Fraction> &ms, const std::set<int> &system_start_measure_nrs,
                                    const std::set<int> &page_start_measure_nrs, const CreditWordsList &cr_words,
                                    const muse::Size &page_size);

    void set_has_inferred_header_text(bool b) { has_inferred_header_text_ = b; }

    bool has_inferred_header_text() const { return has_inferred_header_text_; }

    int max_diff() const { return max_diff_; }

    void insert_adjusted_duration(engraving::Fraction key, engraving::Fraction value) { adjusted_durations_.insert({key, value}); }

    std::map<engraving::Fraction, engraving::Fraction> &adjusted_durations() { return adjusted_durations_; }

    void insert_seen_denominator(int val) { seen_denominators_.emplace(val); }

    MusicXmlExporterSoftware exporter_software() const { return exporter_software_; }

    bool sib_or_dolet() const;
    bool dolet() const;

private:
    // functions
    void add_error(const muse::String &error); // Add an error to be shown in the GUI
    void set_exporter_software(muse::String &exporter);
    void set_exporter_styles();

    // generic pass 1 data
    muse::XmlStreamReader e_;
    MusicXmlExporterSoftware exporter_software_ = MusicXmlExporterSoftware::OTHER; // Software which exported the file
    int divs_ = 0;                                                                 // Current MusicXML divisions value
    std::map<muse::String, MusicXmlPart> parts_;                                   // Parts data, mapped on part id
    std::set<int> system_start_measure_nrs_;                                       // Measure numbers of measures starting a page
    std::set<int> page_start_measure_nrs_;                                         // Measure numbers of measures starting a page
    std::vector<engraving::Fraction> measure_length_;                              // Length of each measure
    std::vector<engraving::Fraction> measure_start_;                               // Start time of each measure
    CreditWordsList credits_;                                                      // All credits collected
    PartMap part_map_;                                                             // TODO merge into MusicXmlPart ??
    std::map<muse::String, MusicXmlInstruments> instruments_;                      // instruments for each part, mapped on part id
    engraving::Score *score_ = nullptr;                                            // MuseScore score
    const IMusicXmlConfiguration &configuration_;
    MusicXmlLogger *logger_ = nullptr; // Error logger
    muse::String errors_;              // Errors to present to the user
    bool has_beaming_info_ = false;    // Whether the score supports or contains beaming info
    bool has_inferred_header_text_ = false;

    // part specific data (TODO: move to part-specific class)
    engraving::Fraction time_sig_dura_;                    // Measure duration according to last timesig read
    std::map<int, MusicXmlOctaveShiftDesc> octave_shifts_; // Pending octave-shifts
    muse::Size page_size_;                                 // Page width read from defaults

    const int max_diff_ = 5;                                                // Duration rounding tick threshold;
    std::map<engraving::Fraction, engraving::Fraction> adjusted_durations_; // Rounded durations
    std::set<int> seen_denominators_;                                       // Denominators seen. Used for rounding errors.
};
} // namespace mu::iex::musicxml
