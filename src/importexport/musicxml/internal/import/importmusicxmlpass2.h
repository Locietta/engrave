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

#include "importmusicxmlpass1.h"
#include "../shared/musicxmlsupport.h"
#include "../shared/musicxmltypes.h"
#include "musicxmltupletstate.h"

namespace mu::engraving {
class Accidental;
class Beam;
class ChordRest;
class EngravingItem;
class FiguredBass;
class FiguredBassItem;
class Fraction;
class FretDiagram;
class Glissando;
class GradualTempoChange;
class Hairpin;
class Harmony;
class Instrument;
class Jump;
class Lyrics;
class Marker;
class Measure;
class Note;
class Pedal;
class Score;
class SLine;
class Slur;
class Spanner;
class Staff;
class Sticking;
class StringData;
class Text;
class Tie;
class Trill;
class Tuplet;
class Volta;
enum class TupletBracketType : unsigned char;
enum class TupletNumberType : unsigned char;
} // namespace mu::engraving

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   support enums / structs / classes
//---------------------------------------------------------

// One nonempty container, or one existing word-inferred marking. Text contains
// escaped literals and symbol-table-generated markup only; no private metadata.
struct MusicXmlDynamic {
    muse::String text;
    // Escaped literal text with markup-looking content must not be parsed as
    // dynamic symbols. Ranges exist only while importing, not in the score.
    std::vector<std::pair<size_t, size_t>> protected_literal_ranges;
    engraving::Color color;
    muse::String placement;
    muse::String enclosure;
    bool has_total_y = false;
    double total_y = 0.0;
};

using GraceChordList = std::vector<engraving::Chord *>;
using FiguredBassList = std::vector<engraving::FiguredBass *>;
using Tuplets = std::map<muse::String, engraving::Tuplet *>;
using Beams = std::map<muse::String, engraving::Beam *>;

//---------------------------------------------------------
//   MusicXmlSlash
//---------------------------------------------------------

enum class MusicXmlSlash : unsigned char { NONE, RHYTHM, SLASH };

//---------------------------------------------------------
//   MusicXmlTupletDesc
//---------------------------------------------------------

/**
 Describe the information extracted from
 a single note/notations/tuplet element.
 */

struct MusicXmlTupletDesc {
    MusicXmlTupletDesc();
    MusicXmlStartStop type;
    engraving::DirectionV direction;
    engraving::TupletBracketType bracket;
    engraving::TupletNumberType shownumber;
};

//---------------------------------------------------------
//   SlurDesc
//---------------------------------------------------------

/**
 The description of Slurs being handled
 */

class SlurDesc {
public:
    enum class State : unsigned char { NONE, START, STOP };

    SlurDesc() : slur_(0), state_(State::NONE) {}

    engraving::Slur *slur() const { return slur_; }

    void start(engraving::Slur *slur) {
        slur_ = slur;
        state_ = State::START;
    }

    void stop(engraving::Slur *slur) {
        slur_ = slur;
        state_ = State::STOP;
    }

    bool is_start() const { return state_ == State::START; }

    bool is_stop() const { return state_ == State::STOP; }
private:
    engraving::Slur *slur_ = nullptr;
    State state_;
};

typedef std::map<engraving::SLine *, std::pair<int, int>> MusicXmlSpannerMap;

//---------------------------------------------------------
//   MusicXmlSpannerDesc
//---------------------------------------------------------

struct MusicXmlSpannerDesc {
    engraving::SLine *sp = nullptr;
    engraving::ElementType tp = engraving::ElementType::INVALID;
    int nr = 0;

    MusicXmlSpannerDesc(engraving::SLine *sp, engraving::ElementType tp, int nr) : sp(sp), tp(tp), nr(nr) {}

    MusicXmlSpannerDesc(engraving::ElementType tp, int nr) : sp(0), tp(tp), nr(nr) {}
};

//---------------------------------------------------------
//   MusicXmlExtendedSpannerDesc
//---------------------------------------------------------

struct MusicXmlExtendedSpannerDesc {
    engraving::SLine *sp = nullptr;
    engraving::Fraction tick2{0, 0};
    engraving::track_idx_t track2 = 0;
    bool is_started = false;
    bool is_stopped = false;

    MusicXmlExtendedSpannerDesc() {}

    muse::String to_string() const;
};

//---------------------------------------------------------
//   HarmonyDesc
//---------------------------------------------------------

/**
 The description of a chord symbol with or without a fret diagram
 */

struct HarmonyDesc {
    engraving::track_idx_t track_;
    bool fret_diagram_visible() const;
    engraving::Harmony *harmony_;
    engraving::FretDiagram *fret_diagram_;

    const engraving::ChordDescription *descr() const;

    HarmonyDesc(engraving::track_idx_t input_track, engraving::Harmony *input_harmony, engraving::FretDiagram *fret_diagram)
        : track_(input_track), harmony_(input_harmony), fret_diagram_(fret_diagram) {}

    HarmonyDesc() : track_(muse::nidx), harmony_(nullptr), fret_diagram_(nullptr) {}
};

using HarmonyMap = std::multimap<int, HarmonyDesc>;

//---------------------------------------------------------
//   MusicXmlLyricsExtend
//---------------------------------------------------------

class MusicXmlLyricsExtend {
public:
    MusicXmlLyricsExtend() {}

    void init();
    void add_lyric(engraving::Lyrics *const lyric);
    void set_extend(const int verse, const engraving::track_idx_t track, const engraving::Fraction &tick,
                    const engraving::Lyrics *prev_added_lyrics);

private:
    std::set<engraving::Lyrics *> lyrics_;
};

struct GraceNoteLyrics {
    engraving::Lyrics *lyric = nullptr;
    bool extend = false;
    int no = 0;

    GraceNoteLyrics(engraving::Lyrics *lyric, bool extend, int no) : lyric(lyric), extend(extend), no(no) {}
};

struct InferredPercInstr {
    int pitch;
    engraving::track_idx_t track;
    engraving::String name;
    engraving::Fraction tick;

    InferredPercInstr(int pitch, engraving::track_idx_t track, engraving::String name, engraving::Fraction tick)
        : pitch(pitch), track(track), name(name), tick(tick) {}

    InferredPercInstr() : pitch(-1), track(muse::nidx), name(u""), tick(engraving::Fraction(0, -1)) {}
};

typedef std::vector<InferredPercInstr> InferredPercList;

typedef std::map<engraving::String, std::pair<engraving::String, engraving::DurationType>> MetronomeTextMap;

//---------------------------------------------------------
//   MusicXmlParserLyric
//---------------------------------------------------------

class MusicXmlParserLyric {
public:
    MusicXmlParserLyric(const LyricNumberHandler lyric_number_handler, muse::XmlStreamReader &e, engraving::Score *score,
                        const IMusicXmlConfiguration &configuration, MusicXmlLogger *logger, bool is_voice_staff);

    std::set<engraving::Lyrics *> extended_lyrics() const { return extended_lyrics_; }

    std::map<int, engraving::Lyrics *> numbered_lyrics() const { return numbered_lyrics_; }

    std::vector<engraving::Sticking *> inferred_stickings() const { return inferred_stickings_; }

    void parse(bool visibility = true);
private:
    void skip_log_curr_elem();
    void read_elision(muse::String &formatted_text);
    const LyricNumberHandler lyric_number_handler_;
    muse::XmlStreamReader &e_;
    const engraving::Score *score_ = nullptr; // the score
    const IMusicXmlConfiguration &configuration_;
    MusicXmlLogger *logger_ = nullptr;                      // Error logger
    std::map<int, engraving::Lyrics *> numbered_lyrics_;    // lyrics with valid number
    std::set<engraving::Lyrics *> extended_lyrics_;         // lyrics with the extend flag set
    std::vector<engraving::Sticking *> inferred_stickings_; // stickings with valid number
    double default_y_ = 0.0;
    double relative_y_ = 0.0;
    muse::String placement_;
    muse::String placement() const;

    double total_y() const { return default_y_ + relative_y_; }

    bool has_total_y() const { return !muse::real_is_null(default_y_) || !muse::real_is_null(relative_y_); }

    bool is_likely_sticking(const muse::String &text, const engraving::LyricsSyllabic syllabic, const bool has_extend);
    bool is_voice_staff_ = true;
};

//---------------------------------------------------------
//   Notation
//      Most with text base attributes (font-*, placement)
//---------------------------------------------------------

class Notation {
public:
    Notation(const muse::String &name, const muse::String &parent = {}, const engraving::SymId &sym_id = engraving::SymId::NO_SYM) {
        name_ = name;
        parent_ = parent;
        sym_id_ = sym_id;
    }

    void add_attribute(const muse::String &name, const muse::String &value);
    muse::String attribute(const muse::String &name) const;

    const std::map<muse::String, muse::String> &attributes() const { return attributes_; }

    muse::String name() const { return name_; }

    muse::String parent() const { return parent_; }

    void set_sym_id(const engraving::SymId &sym_id) { sym_id_ = sym_id; }

    engraving::SymId sym_id() const { return sym_id_; }

    void set_sub_type(const muse::String &sub_type) { sub_type_ = sub_type; }

    muse::String sub_type() const { return sub_type_; }

    muse::String print() const;

    void set_text(const muse::String &text) { text_ = text; }

    muse::String text() const { return text_; }

    void set_visible(const bool visible) { visible_ = visible; }

    bool visible() const { return visible_; }

    static Notation notation_with_attributes(const muse::String &name, const std::vector<muse::XmlStreamReader::Attribute> &attributes,
                                             const muse::String &parent = {}, const engraving::SymId &sym_id = engraving::SymId::NO_SYM);
private:
    muse::String name_;
    muse::String parent_;
    engraving::SymId sym_id_ = engraving::SymId::NO_SYM;
    muse::String sub_type_;
    muse::String text_;
    std::map<muse::String, muse::String> attributes_;
    bool visible_ = true;
};

//---------------------------------------------------------
//   forward references and defines
//---------------------------------------------------------

struct DelayedArpeggio {
    muse::String arpeggio_type_ = u"";
    int arpeggio_no_ = 0;

    DelayedArpeggio(muse::String arp_type, int no) : arpeggio_type_(arp_type), arpeggio_no_(no) {}

    DelayedArpeggio() : arpeggio_type_(muse::String(u"")), arpeggio_no_(0) {}

    void clear() {
        arpeggio_type_ = u"";
        arpeggio_no_ = 0;
    }
};

class MusicXmlLogger;
class MusicXmlDelayedDirectionElement;
class MusicXmlInferredFingering;
class MusicXmlParserPass2;

using DelayedDirectionsList = std::vector<MusicXmlDelayedDirectionElement *>;
using InferredFingeringsList = std::vector<MusicXmlInferredFingering *>;
using SlurStack = std::array<SlurDesc, ::mu::iex::musicxml::max_number_level>;
using TrillStack = std::array<engraving::Trill *, ::mu::iex::musicxml::max_number_level>;
using BracketsStack = std::array<MusicXmlExtendedSpannerDesc, ::mu::iex::musicxml::max_number_level>;
using OttavasStack = std::array<MusicXmlExtendedSpannerDesc, ::mu::iex::musicxml::max_number_level>;
using HairpinsStack = std::array<MusicXmlExtendedSpannerDesc, ::mu::iex::musicxml::max_number_level>;
using InferredHairpinsStack = std::vector<engraving::Hairpin *>;
using InferredTempoLineStack = std::vector<engraving::GradualTempoChange *>;
using SpannerStack = std::array<MusicXmlExtendedSpannerDesc, ::mu::iex::musicxml::max_number_level>;
using SpannerSet = std::set<engraving::Spanner *>;
using DelayedArpMap = std::map<int, DelayedArpeggio>;
using SegnoStack = std::map<int, engraving::Marker *>;
using SystemElements = std::multimap<int, engraving::EngravingItem *>;

// Ties are identified by the pitch and track of their first note
typedef std::pair<int, engraving::track_idx_t> TieLocation;
using MusicXmlTieMap = std::map<TieLocation, engraving::Tie *>;

//---------------------------------------------------------
//   MusicXmlParserNotations
//---------------------------------------------------------

class MusicXmlParserNotations {
public:
    MusicXmlParserNotations(muse::XmlStreamReader &e, engraving::Score *score, MusicXmlLogger *logger, MusicXmlParserPass2 &pass2);
    void parse();
    void add_to_score(engraving::ChordRest *const cr, engraving::Note *const note, const engraving::Fraction &tick, SlurStack &slurs,
                      engraving::Glissando *glissandi[::mu::iex::musicxml::max_number_level][2], MusicXmlSpannerMap &spanners,
                      TrillStack &trills, MusicXmlTieMap &ties, std::vector<engraving::Note *> &unstarted_tie_notes,
                      std::vector<engraving::Note *> &unended_tie_notes, ArpeggioMap &arp_map, DelayedArpMap &delayed_arps);

    muse::String errors() const { return errors_; }

    MusicXmlTupletDesc tuplet_desc() const { return tuplet_desc_; }

    bool has_tremolo() const { return has_tremolo_; }

    muse::String tremolo_type() const { return tremolo_type_; }

    muse::String tremolo_smufl() const { return tremolo_smufl_; }

    engraving::Color tremolo_color() const { return tremolo_color_; }

    int tremolo_nr() const { return tremolo_nr_; }

    bool must_stop_grace_after() const { return (slur_stop_ && !slur_start_) || wavy_line_stop_; }
private:
    void add_error(const muse::String &error); // Add an error to be shown in the GUI
    void add_notation(const Notation &notation, engraving::ChordRest *const cr, engraving::Note *const note);
    void add_technical(const Notation &notation, engraving::Note *note);
    void arpeggio();
    void harmonic();
    void harmon_mute();
    void hole();
    void articulations();
    void dynamics();
    void fermata();
    void glissando_slide();
    void mordent_normal_or_inverted();
    void ornaments();
    void slur();
    void skip_log_curr_elem();
    void technical();
    void other_technical();
    void tied();
    void tuplet();
    void other_notation();
    muse::XmlStreamReader &e_;
    MusicXmlParserPass2 &pass2_;
    engraving::Score *score_ = nullptr; // the score
    MusicXmlLogger *logger_ = nullptr;  // the error logger
    muse::String errors_;               // errors to present to the user
    MusicXmlTupletDesc tuplet_desc_;
    std::vector<MusicXmlDynamic> dynamics_list_;
    std::vector<Notation> notations_;
    bool has_tremolo_ = false;
    muse::String tremolo_type_;
    int tremolo_nr_ = 0;
    muse::String tremolo_smufl_;
    engraving::Color tremolo_color_;
    muse::String wavy_line_type_;
    int wavy_line_no_ = 0;
    muse::String arpeggio_type_;
    int arpeggio_no_ = 0;
    engraving::Color arpeggio_color_;
    bool slur_stop_ = false;
    bool slur_start_ = false;
    bool wavy_line_stop_ = false;
    bool visible_ = true;
};

//---------------------------------------------------------
//   MusicXmlParserPass2
//---------------------------------------------------------

class MusicXmlParserPass2 {
public:
    MusicXmlParserPass2(engraving::Score *score, MusicXmlParserPass1 &pass1, MusicXmlLogger *logger);

    const IMusicXmlConfiguration *configuration() const { return pass1_.configuration(); }

    engraving::Err parse(const muse::ByteArray &data);

    muse::String errors() const { return errors_; }

    // part specific data interface functions
    void add_spanner(const MusicXmlSpannerDesc &desc);
    MusicXmlExtendedSpannerDesc &get_spanner(const MusicXmlSpannerDesc &desc);
    void clear_spanner(const MusicXmlSpannerDesc &desc);
    void delete_handled_spanner(engraving::SLine *const &spanner);

    int divs() { return divs_; }

    engraving::SLine *delayed_ottava() { return delayed_ottava_; }

    void set_delayed_ottava(engraving::SLine *ottava) { delayed_ottava_ = ottava; }

    void add_inferred_hairpin(engraving::Hairpin *hp) { inferred_hairpins_.push_back(hp); }

    const InferredHairpinsStack &get_inferred_hairpins() { return inferred_hairpins_; }

    void add_inferred_tempo_line(engraving::GradualTempoChange *hp) { inferred_tempo_lines_.push_back(hp); }

    const InferredTempoLineStack &get_inferred_tempo_line() { return inferred_tempo_lines_; }

    void add_system_element(engraving::EngravingItem *el, const engraving::Fraction &tick) { sys_elements_.insert({tick.ticks(), el}); }

    const SystemElements system_elements() const { return sys_elements_; }

    InferredPercInstr inferred_perc_instr(const engraving::Fraction &tick, const engraving::track_idx_t track_idx);

    void add_inferred_perc_instr(InferredPercInstr instr) { inferred_perc_.push_back(instr); }

    void add_elem_offset(engraving::EngravingItem *el, engraving::track_idx_t track, const muse::String &placement,
                         engraving::Measure *measure, const engraving::Fraction &tick);

private:
    void add_error(const muse::String &error); // Add an error to be shown in the GUI
    void init_part_state(const muse::String &part_id);
    SpannerSet find_incomplete_spanners_at_part_end();
    engraving::Err parse();
    void score_partwise();
    void part_list();
    void score_part();
    void part();
    void measure(const muse::String &part_id, const engraving::Fraction time);
    void measure_layout(engraving::Measure *measure);
    void set_measure_repeats(const engraving::staff_idx_t score_rel_staff, engraving::Measure *measure);
    void attributes(const muse::String &part_id, engraving::Measure *measure, const engraving::Fraction &tick);
    void measure_style(engraving::Measure *measure);
    void barline(const muse::String &part_id, engraving::Measure *measure, const engraving::Fraction &tick);
    void key(const muse::String &part_id, engraving::Measure *measure, const engraving::Fraction &tick);
    void clef(const muse::String &part_id, engraving::Measure *measure, const engraving::Fraction &tick);
    void time(const muse::String &part_id, engraving::Measure *measure, const engraving::Fraction &tick);
    void divisions();
    engraving::Note *note(const muse::String &part_id, engraving::Measure *measure, const engraving::Fraction s_time,
                          const engraving::Fraction prev_time, engraving::Fraction &missing_prev, engraving::Fraction &dura,
                          engraving::Fraction &missing_curr, muse::String &current_voice, GraceChordList &gcl, size_t &gac,
                          Beams &curr_beams, FiguredBassList &fbl, int &alt, MusicXmlTupletStates &tuplet_states, Tuplets &tuplets,
                          ArpeggioMap &arp_map, DelayedArpMap &delayed_arps);
    void note_print_spacing_no(engraving::Fraction &dura);
    engraving::FiguredBassItem *figure(const int idx, const bool paren, engraving::FiguredBass *parent);
    engraving::FiguredBass *figured_bass();
    engraving::FretDiagram *frame();
    void harmony(const muse::String &part_id, engraving::Measure *measure, const engraving::Fraction &s_time, HarmonyMap &harmony_map);
    engraving::Accidental *accidental();
    void beam(std::map<int, muse::String> &beam_types);
    void duration(engraving::Fraction &dura);
    void forward(engraving::Fraction &dura);
    void backup(engraving::Fraction &dura);
    void time_modification(engraving::Fraction &time_mod, engraving::TDuration &normal_type);
    void stem(engraving::DirectionV &sd, bool &nost);
    void do_ending(const muse::String &part_id, engraving::Measure *measure, const muse::String &number, const muse::String &type,
                   const engraving::Color color, const muse::String &text, const bool print);
    void staff_details(const muse::String &part_id, engraving::Measure *measure = nullptr);
    void staff_tuning(engraving::StringData *t);
    void skip_log_curr_elem();

    // multi-measure rest state handling
    void set_multi_measure_rest_count(int count);
    int get_and_dec_multi_measure_rest_count();

    void xml_set_drumset_pitch(engraving::Note *note, const engraving::Chord *chord, const engraving::Staff *staff, int step, int octave,
                               engraving::NoteHeadGroup head_group, engraving::DirectionV &stem_dir, engraving::Instrument *instrument);

    // generic pass 2 data

    muse::XmlStreamReader e_;
    int divs_ = 0;                      // the current divisions value
    engraving::Score *score_ = nullptr; // the score
    MusicXmlParserPass1 &pass1_;        // the pass1 results
    MusicXmlLogger *logger_ = nullptr;  // Error logger
    muse::String errors_;               // Errors to present to the user

    // part specific data (TODO: move to part-specific class)

    // Measure duration according to last timesig read
    // TODO: store timesigs read in pass 1, use those instead
    // or use score->sigmap() ?
    engraving::Fraction time_sig_dura_;

    SlurStack slurs_{{}};
    TrillStack trills_{{}}; // Current trills
    BracketsStack brackets_;
    OttavasStack ottavas_;   // Current ottavas
    HairpinsStack hairpins_; // Current hairpins
    InferredHairpinsStack inferred_hairpins_;
    InferredTempoLineStack inferred_tempo_lines_;
    MusicXmlExtendedSpannerDesc dummy_new_music_xml_spanner_desc_;

    engraving::Glissando *glissandi_[::mu::iex::musicxml::max_number_level][2]{{}}; // Current slides ([0]) / glissandi ([1])

    MusicXmlTieMap ties_;
    std::vector<engraving::Note *> unstarted_tie_notes_;
    std::vector<engraving::Note *> unended_tie_notes_;
    engraving::Volta *last_volta_ = nullptr;
    bool has_drumset_ = false; // drumset defined TODO: move to pass 1

    MusicXmlSpannerMap spanners_;

    MusicXmlExtendedSpannerDesc pedal_;          // Current pedal
    engraving::Pedal *pedal_continue_ = nullptr; // Current pedal type="change" requiring fixup
    engraving::Harmony *harmony_ = nullptr;      // Current harmony
    engraving::Chord *trem_start_ = nullptr;     // Starting chord for current tremolo
    engraving::FiguredBass *fig_bass_ = nullptr; // Current figured bass element (to attach to next note)
    engraving::SLine *delayed_ottava_ = nullptr; // Current delayed ottava
    SegnoStack segnos_;                          // List of segno markings
    int multi_measure_rest_count_ = 0;
    int measure_number_ = 0;                         // Current measure number as written in the score
    MusicXmlLyricsExtend extended_lyrics_;           // Lyrics with "extend" requiring fixup
    std::vector<GraceNoteLyrics> grace_note_lyrics_; // Lyrics to be moved from grace note to main note

    MusicXmlSlash measure_style_slash_ = MusicXmlSlash::NONE; // Are we inside a measure to be displayed as slashes?

    size_t nstaves_ = 0; // Number of staves in current part
    std::vector<int> measure_repeat_num_measures_;
    std::vector<int> measure_repeat_count_;

    SystemElements sys_elements_;
    InferredPercList inferred_perc_;
};

//---------------------------------------------------------
//   MusicXmlParserDirection
//---------------------------------------------------------

class MusicXmlParserDirection {
public:
    MusicXmlParserDirection(muse::XmlStreamReader &e, engraving::Score *score, MusicXmlParserPass1 &pass1, MusicXmlParserPass2 &pass2,
                            MusicXmlLogger *logger);

    const IMusicXmlConfiguration *configuration() const { return pass1_.configuration(); }

    void direction(const muse::String &part_id, engraving::Measure *measure, const engraving::Fraction &tick, MusicXmlSpannerMap &spanners,
                   DelayedDirectionsList &delayed_directions, InferredFingeringsList &inferred_fingerings, HarmonyMap &harmony_map,
                   bool &measure_has_coda, SegnoStack &segnos);

    double total_y() const { return default_y_ + relative_y_; }

    muse::String placement() const;

    void set_bpm(const double bpm) { tpo_sound_ = bpm; }

private:
    void direction_type(std::vector<MusicXmlSpannerDesc> &starts, std::vector<MusicXmlSpannerDesc> &stops);
    void bracket(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                 std::vector<MusicXmlSpannerDesc> &stops);
    void octave_shift(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                      std::vector<MusicXmlSpannerDesc> &stops);
    void pedal(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
               std::vector<MusicXmlSpannerDesc> &stops);
    void dashes(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
                std::vector<MusicXmlSpannerDesc> &stops);
    void wedge(const muse::String &type, const int number, std::vector<MusicXmlSpannerDesc> &starts,
               std::vector<MusicXmlSpannerDesc> &stops);
    muse::String metronome(double &r);
    void sound();
    void play();
    void swing();
    void dynamics();
    void harp_pedal();
    void other_direction();
    void handle_repeats(engraving::Measure *measure, const engraving::Fraction tick, bool &measure_has_coda, SegnoStack &segnos,
                        DelayedDirectionsList &delayed_directions);
    engraving::Marker *find_marker(const muse::String &repeat) const;
    engraving::Jump *find_jump(const muse::String &repeat) const;
    void handle_nmi_cmi(engraving::Measure *measure, const engraving::Fraction &tick, DelayedDirectionsList &delayed_directions);
    void handle_chord_sym(const engraving::Fraction &tick, HarmonyMap &harmony_map);
    void handle_tempo(muse::String &words_string);
    muse::String match_repeat(const muse::String &plain_words) const;
    void skip_log_curr_elem();
    bool is_likely_credit(const engraving::Fraction &tick) const;
    void text_to_dynamic(muse::String &text);
    void text_to_cresc_line(muse::String &text);
    void add_inferred_hairpin(const engraving::Fraction &tick, const bool is_vocal_staff);
    void add_inferred_tempo_line(const engraving::Fraction &tick);
    bool is_lyric_bracket() const;
    bool is_likely_subtitle(const engraving::Fraction &tick) const;
    bool is_likely_legally_downloaded(const engraving::Fraction &tick) const;
    bool is_likely_tempo_text(const engraving::track_idx_t track) const;
    void handle_fraction();
    bool is_likely_tempo_line(const engraving::track_idx_t track) const;
    engraving::Text *add_text_to_header(const engraving::TextStyleType text_style_type);
    void hide_redundant_header_text(const engraving::Text *inferred_text, const std::vector<muse::String> meta_tags);
    bool is_likely_fingering(const muse::String &fingering_str) const;
    bool is_likely_sticking();
    bool is_likely_dynamic_range() const;
    engraving::PlayingTechniqueType get_playing_technique() const;
    void handle_drum_instrument(bool is_perc, engraving::Fraction tick) const;

    // void terminateInferredLine(const std::vector<TextLineBase*> lines, const Fraction& tick);

    bool has_total_y() const { return has_relative_y_ || has_default_y_; }

    muse::XmlStreamReader &e_;
    engraving::Score *score_ = nullptr; // the score
    MusicXmlParserPass1 &pass1_;        // the pass1 results
    MusicXmlParserPass2 &pass2_;        // the pass2 results
    MusicXmlLogger *logger_ = nullptr;  // Error logger

    engraving::Color color_;
    engraving::Hairpin *inferred_hairpin_start_ = nullptr;
    engraving::GradualTempoChange *inferred_tempo_line_start_ = nullptr;
    std::vector<MusicXmlDynamic> dynamics_list_;
    muse::String font_family_;
    muse::String enclosure_;
    muse::String words_text_;
    muse::String metro_text_;
    muse::String rehearsal_text_;
    muse::String justify_;
    muse::String dyna_velocity_;
    muse::String snd_coda_;
    muse::String snd_dacapo_;
    muse::String snd_dalsegno_;
    muse::String snd_fine_;
    muse::String snd_segno_;
    muse::String snd_to_coda_;
    muse::String coda_id_;
    muse::String segno_id_;
    muse::String placement_;
    muse::String play_;
    bool has_default_y_ = false;
    double default_y_ = 0.0;
    bool has_relative_y_ = false;
    double relative_y_ = 0.0;
    double relative_x_ = 0.0;
    double tpo_metro_ = 0.0; // tempo according to metronome
    double tpo_sound_ = 0.0; // tempo according to sound
    bool visible_ = true;
    bool system_direction_ = false;
    std::pair<int, int> swing_ = {0, 0};
    std::vector<engraving::EngravingItem *> elems_;
    engraving::Fraction offset_;
    engraving::track_idx_t track_ = muse::nidx;
};

//---------------------------------------------------------
//   MusicXmlDelayedDirectionElement
//---------------------------------------------------------
/**
 Helper class to allow Direction elements to be sorted by _totalY
 before being added to the score.
 */

class MusicXmlDelayedDirectionElement {
public:
    MusicXmlDelayedDirectionElement(double total_y, engraving::EngravingItem *element, engraving::track_idx_t track, muse::String placement,
                                    engraving::Measure *measure, engraving::Fraction tick)
        : total_y_(total_y), element_(element), track_(track), placement_(placement), measure_(measure), tick_(tick) {}

    double total_y() const { return total_y_; }

    engraving::EngravingItem *element() const { return element_; }

    engraving::track_idx_t track() const { return track_; }

    const engraving::Fraction &tick() const { return tick_; }

    const muse::String &placement() const { return placement_; }

    engraving::Measure *measure() const { return measure_; }

private:
    double total_y_ = 0.0;
    engraving::EngravingItem *element_ = nullptr;
    engraving::track_idx_t track_ = muse::nidx;
    muse::String placement_;
    engraving::Measure *measure_ = nullptr;
    engraving::Fraction tick_;
};

//---------------------------------------------------------
//   MusicXmlInferredFingering
//---------------------------------------------------------
/**
 Helper class to allow Direction elements to be reinterpreted as fingerings
 */

class MusicXmlInferredFingering {
public:
    MusicXmlInferredFingering(double total_y, engraving::EngravingItem *element, muse::String &text, engraving::track_idx_t track,
                              muse::String placement, engraving::Measure *measure, engraving::Fraction tick, bool need_use_default_font);

    double total_y() const { return total_y_; }

    engraving::Fraction tick() const { return tick_; }

    engraving::track_idx_t track() const { return track_; }

    std::vector<muse::String> fingerings() const { return fingerings_; }

    bool find_and_add_to_notes(engraving::Measure *measure);
    MusicXmlDelayedDirectionElement *to_delayed_direction();

private:
    double total_y_;
    engraving::EngravingItem *element_;
    muse::String text_;
    std::vector<muse::String> fingerings_;
    engraving::track_idx_t track_;
    muse::String placement_;
    engraving::Measure *measure_;
    engraving::Fraction tick_;
    bool need_use_default_font_ = false;

    void round_tick(engraving::Measure *measure);
    void add_to_notes(std::vector<engraving::Note *> &notes) const;
};
} // namespace mu::iex::musicxml
