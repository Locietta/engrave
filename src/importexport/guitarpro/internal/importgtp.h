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

#include <map>
#include <vector>

#include "io/iodevice.h"
#include "types/bytearray.h"
#include "types/string.h"

#include "engraving/dom/measurebase.h"
#include "engraving/dom/stringdata.h"
#include "engraving/engravingerrors.h"
#include "engraving/iengravingconfiguration.h"
#include "engraving/types/types.h"

#include "continiouselementsbuilder.h"
#include "gtp/igpdombuilder.h"
#include "guitarbendimport/guitarbendimporter.h"

namespace mu::engraving {
class Chord;
class ChordRest;
class Drumset;
class Instrument;
class LetRing;
class Measure;
class Note;
class PalmMute;
class Score;
class Segment;
class Tuplet;
class Vibrato;
class Volta;
class MasterScore;
class Ottava;
class Hairpin;
class Bend;
class Slur;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
static constexpr int gp_max_lyric_lines = 5;
static constexpr int gp_max_track_number = 32;
static constexpr int gp_max_string_number = 7;
static constexpr int gp_default_percussion_channel = 9;
static constexpr int gp_invalid_keysig = 127;
static constexpr int gp_volta_binary = 1;
static constexpr int gp_volta_flags = 2;

mu::engraving::Err import_gtp(mu::engraving::Score *score, const muse::String &filename, const char *data, unsigned int data_len);

enum class Repeat : char;

struct GpTrack {
    int patch;
    uint8_t volume, pan, chorus, reverb, phase, tremolo;
};

struct GPVolta {
    int volta_type;
    std::vector<int> volta_info;
};

/* How the fermatas are represented in Guitar Pro is two integers, the
 * first is an index value and the second is the time division that
 * index value refers to, and they are givin with respect to a
 * measure. Time division 0 means a minim, 1 is a crotchet, 2 is a
 * quaver and so on, with the index (counting from 0) referring to how
 * many time divisions occur before the fermata. These numbers are
 * separated in GP6 with a '/' character. For example, a note
 * occurring on the third beat of a measure in a 4/4 bar would be
 * represented as 2/1.
 */

struct GPFermata {
    int index;
    int time_division;
    muse::String type;
};

struct GPLyrics {
    muse::StringList lyrics;
    std::vector<mu::engraving::Segment *> segments;
    std::vector<size_t> lyric_pos;
    size_t from_beat = 0;
    size_t beat_counter_value = 0;
    size_t lyric_track = 0;
};

struct GpBar {
    mu::engraving::Fraction timesig = mu::engraving::Fraction(4, 4);
    bool free_time = false;
    int keysig = ::mu::iex::guitarpro::gp_invalid_keysig;
    muse::String marker;
    mu::engraving::BarLineType bar_line = mu::engraving::BarLineType::NORMAL;
    mu::engraving::Repeat repeat_flags = mu::engraving::Repeat::NONE;
    int repeats = 2;
    GPVolta volta;
    muse::String direction;
    muse::String direction_style;
    muse::String section[2];
    std::vector<muse::String> directions;
};

//---------------------------------------------------------
//   GuitarPro
//---------------------------------------------------------

class GuitarPro {
public:
    const std::shared_ptr<mu::engraving::IEngravingConfiguration> &engraving_configuration() const;

protected:

    using staff_idx_t = mu::engraving::staff_idx_t;
    using track_idx_t = mu::engraving::track_idx_t;
    using Fraction = mu::engraving::Fraction;
    using Measure = mu::engraving::Measure;
    using ChordRest = mu::engraving::ChordRest;
    using Chord = mu::engraving::Chord;
    using Note = mu::engraving::Note;

    enum class TabImportOption { STANDARD = 1, TAB = 2, BOTH = 3 };

    struct GPProperties {
        std::vector<TabImportOption> parts_import_options;
    };

    struct ReadNoteResult {
        bool slur = false;
        bool hammer_on_pull_off = false;
        bool let_ring = false;
        bool palm_mute = false;
        bool trill = false;
        bool vibrato = false;

        /// harmonic marks
        bool harmonic_artificial = false;
        bool harmonic_pinch = false;
        bool harmonic_tap = false;
        bool harmonic_semi = false;
    };

    GPProperties properties_;

    std::vector<Note *> slide_list; //list of start slide notes

    // note effect bit masks
    static const uint8_t effect_bend = 0x1;
    static const uint8_t effect_staccato = 0x1;
    static const uint8_t effect_hammer = 0x2;
    static const uint8_t effect_palm_mute = 0x2;
    static const uint8_t effect_tremolo = 0x4;
    static const uint8_t effect_let_ring = 0x8;
    static const uint8_t effect_slide_old = 0x4;
    static const uint8_t effect_slide = 0x8;
    static const uint8_t effect_grace = 0x10;
    static const uint8_t effect_artificial_harmonic = 0x10;
    static const uint8_t effect_trill = 0x20;
    static const uint8_t effect_ghost = 0x01;
    static const uint8_t effect_vibrato = 0x40;

    // arpeggio direction masks
    static const uint8_t arpeggio_up = 0xa;
    static const uint8_t arpeggio_down = 0x2;

    // note bit masks
    static const uint8_t note_ghost = 0x04;       // 2
    static const uint8_t note_dead = 0x20;        //5
    static const uint8_t note_dynamic = 0x10;     // 4
    static const uint8_t note_fret = 0x20;        //5
    static const uint8_t note_fingering = 0x80;   //7
    static const uint8_t note_marcato = 0x02;     //1
    static const uint8_t note_sforzato = 0x40;    //6
    static const uint8_t note_slur = 0x8;         //3
    static const uint8_t note_appogiatura = 0x02; //1

    // beat bit masks
    static const uint8_t beat_rasgueado = 0x01;
    static const uint8_t beat_vibrato_tremolo = 0x02;
    static const uint8_t beat_fade = 0x10;
    static const uint8_t beat_effect = 0x20;
    static const uint8_t beat_tremolo = 0x04;
    static const uint8_t beat_arpeggio = 0x40;
    static const uint8_t beat_stroke_dir = 0x02;
    static const uint8_t beat_dotted = 0x01;
    static const uint8_t beat_pause = 0x40;
    static const uint8_t beat_tuplet = 0x20;
    static const uint8_t beat_lyrics = 0x4;
    static const uint8_t beat_effects_value = 0x8;
    static const uint8_t beat_mix_change = 0x10;
    static const uint8_t beat_chord = 0x2;

    // score bit masks
    static const uint8_t score_timesig_numerator = 0x1;
    static const uint8_t score_timesig_denominator = 0x2;
    static const uint8_t score_repeat_start = 0x4;
    static const uint8_t score_repeat_end = 0x8;
    static const uint8_t score_marker = 0x20;
    static const uint8_t score_volta = 0x10;
    static const uint8_t score_keysig = 0x40;
    static const uint8_t score_double_bar = 0x80;

    // slide kinds
    static const int shift_slide = 1;
    static const int legato_slide = 2;
    static const int slide_out_down = 4;
    static const int slide_out_up = 8;
    static const int slide_in_above = 16;
    static const int slide_in_below = 32;

    // harmonic mark types
    static const int harmonic_mark_natural = 1;
    static const int harmonic_mark_artificial = 2;
    static const int harmonic_mark_tap = 3;
    static const int harmonic_mark_pinch = 4;
    static const int harmonic_mark_semi = 5;

    static const int max_pitch = 127;
    static const char *const errmsg[];
    int version = 0;
    int key = 0;

    mu::engraving::Segment *last_segment = nullptr;
    Measure *last_measure = nullptr;
    int last_tempo = -1;

    std::vector<mu::engraving::Ottava *> ottava; /// will be removed
    mu::engraving::Hairpin **hairpins = nullptr; /// will be removed
    mu::engraving::MasterScore *score = nullptr;
    muse::io::IODevice *f = nullptr;
    int cur_pos = 0;
    int previous_tempo = -1;
    std::vector<int> previous_dynamic_by_track;
    constexpr static int invalid_dynamic = -1;
    constexpr static int default_dynamic = 0;
    int tempo = -1;
    std::map<int, int> slides;

    GPLyrics gp_lyrics;
    int slide = 0;
    int volta_sequence = 0;
    std::vector<mu::engraving::Slur *> slurs;

    void skip(int64_t len);
    void read(void *p, int64_t len);
    uint8_t read_u_int8();
    int read_char();
    muse::String read_pascal_string(int);

    muse::String read_word_pascal_string();
    muse::String read_byte_pascal_string();
    int read_int();
    muse::String read_delphi_string();
    void read_volta(GPVolta *, Measure *);
    void read_bend(Note *);
    std::vector<mu::engraving::PitchValue> read_bend_data_from_file();
    void create_bend(Note *note, std::vector<mu::engraving::PitchValue> &bend_data);
    virtual bool read_mix_change(Measure *measure);
    virtual int read_beat_effects(int track, mu::engraving::Segment *) = 0;
    void read_lyrics();
    void read_channels();
    void set_tuplet(mu::engraving::Tuplet *tuplet, int tuple);
    void setup_tuplet_style(mu::engraving::Tuplet *tuplet);
    Fraction len2fraction(int len);
    void add_dynamic(Note *, int d);
    void create_measures();
    void apply_beat_effects(mu::engraving::Chord *, int input_beat_effects, bool &has_vibrato_left_hand, bool &has_vibrato_w_trem_bar);
    void read_tremolo_bar(int track, mu::engraving::Segment *);
    void read_chord(mu::engraving::Segment *seg, int track, int num_strings, muse::String name, bool gp_header);
    void rests_for_empty_beats(mu::engraving::Segment *seg, Measure *measure, ChordRest *cr, Fraction &l, int track, const Fraction &tick);
    void create_slur(bool has_slur, staff_idx_t staff_idx, ChordRest *cr);
    void create_slide(int slide, ChordRest *cr, int staff_idx, Note *note = nullptr);
    void add_text_to_note(muse::String text, Note *note);
    void add_text_articulation(Note *note, mu::engraving::ArticulationTextType type);
    void add_palm_mute(ChordRest *cr, bool has_palm_mute);
    void add_let_ring(ChordRest *cr, bool has_palm_mute);
    void add_trill(ChordRest *cr, bool has_trill);
    void add_hammer_on_pull_off(ChordRest *cr, bool has_hammer_on_pull_off);
    void add_rasgueado(ChordRest *cr, bool has_rasgueado);
    void add_vibrato_left_hand(ChordRest *cr, bool has_vibrato_left_hand);
    void add_vibrato_w_trem_bar(ChordRest *cr, bool has_vibrato_w_trem_bar);
    void add_harmonic_marks(ChordRest *cr, bool has_harmonic_artificial, bool has_harmonic_pinch, bool has_harmonic_tap,
                            bool has_harmonic_semi);
    void add_tap(Chord *);
    void add_slap(Note *);
    void add_pop(Note *);
    bool create_tuning_string(int strings, int tuning[]); // returns useFlats

    virtual std::unique_ptr<IGPDomBuilder> create_gp_dom_builder() const { return nullptr; }

    void init_dynamics(size_t staves_num);
    void add_tunings();

public:
    std::vector<std::string> tunings;

    void set_tempo(int n, Measure *measure);
    muse::String title, subtitle, artist, album, composer, poet;
    muse::StringList comments;
    std::array<GpTrack, ::mu::iex::guitarpro::gp_max_track_number * 2> channel_defaults;
    size_t staves = 0;
    size_t measures = 0;
    std::vector<GpBar> bars;
    std::unique_ptr<ContiniousElementsBuilder> continious_elements_builder_;
    std::unique_ptr<GuitarBendImporter> guitar_bend_importer_;

    enum class GuitarProError : char { GP_NO_ERROR, GP_UNKNOWN_FORMAT, GP_EOF, GP_BAD_NUMBER_OF_STRINGS };

    GuitarPro(mu::engraving::MasterScore *, int v);

    virtual ~GuitarPro() {}

    virtual bool read(muse::io::IODevice *) = 0;

    muse::String error(GuitarProError n) const { return muse::String::from_utf8(errmsg[int(n)]); }
};

//---------------------------------------------------------
//   GuitarPro1
//---------------------------------------------------------

class GuitarPro1 : public GuitarPro {
protected:
    ReadNoteResult read_note(int string, mu::engraving::Note *note);
    int read_beat_effects(int track, mu::engraving::Segment *) override;

public:
    GuitarPro1(mu::engraving::MasterScore *s, int v) : GuitarPro(s, v) {}

    bool read(muse::io::IODevice *) override;
};

//---------------------------------------------------------
//   GuitarPro2
//---------------------------------------------------------

class GuitarPro2 : public GuitarPro1 {
public:
    GuitarPro2(mu::engraving::MasterScore *s, int v) : GuitarPro1(s, v) {}

    bool read(muse::io::IODevice *) override;
};

//---------------------------------------------------------
//   GuitarPro3
//---------------------------------------------------------

class GuitarPro3 : public GuitarPro1 {
    int read_beat_effects(int track, mu::engraving::Segment *segment) override;

public:
    GuitarPro3(mu::engraving::MasterScore *s, int v) : GuitarPro1(s, v) {}

    bool read(muse::io::IODevice *) override;
};

//---------------------------------------------------------
//   GuitarPro4
//---------------------------------------------------------

class GuitarPro4 : public GuitarPro {
    std::vector<int> cur_dynam;
    std::vector<int> tuple_kind;
    void read_info();
    ReadNoteResult read_note(int string, int staff_idx, Note *note);
    int read_beat_effects(int track, mu::engraving::Segment *segment) override;
    bool read_mix_change(mu::engraving::Measure *measure) override;
    int convert_g_p4_slide_num(int slide);

public:
    GuitarPro4(mu::engraving::MasterScore *s, int v) : GuitarPro(s, v) {}

    bool read(muse::io::IODevice *) override;
};

//---------------------------------------------------------
//   GuitarPro5
//---------------------------------------------------------

class GuitarPro5 : public GuitarPro {
    std::map<std::pair<int, int>, bool> dead_end;
    int beat_counter_value{0};
    std::unordered_map<mu::engraving::Chord *, mu::engraving::TremoloType> tremolos_in_chords_; // for adding tremolo for tied notes
    std::unordered_map<Note *, Note *> harmonic_notes_;                                         // for adding ties for harmonic notes
    bool current_beat_has_rasgueado_ = false;
    std::unordered_set<ChordRest *> let_ring_for_chords_; // fixing gp5 bug with no storing let ring for tied notes
    std::vector<std::pair<Note *, Note *>> glissando_note_pairs_;

    void read_info();
    void read_page_setup();
    int read_beat_effects(int track, mu::engraving::Segment *segment) override;
    ReadNoteResult read_note(int string, Note *note);
    bool read_mix_change(Measure *measure) override;
    void read_measure(Measure *measure, int staff_idx, mu::engraving::Tuplet *[], bool mix_change);
    bool read_tracks();
    void read_measures(int starting_tempo);
    Fraction read_beat(const Fraction &tick, int voice, Measure *measure, int staff_idx, mu::engraving::Tuplet **tuplets, bool mix_change);
    ReadNoteResult read_note_effects(Note *);
    float natural_harmonic_from_fret(int fret);
    void add_glissandos();

public:
    GuitarPro5(mu::engraving::MasterScore *s, int v) : GuitarPro(s, v) {}

    bool read(muse::io::IODevice *) override;
};

//---------------------------------------------------------
//   GuitarPro6
//---------------------------------------------------------

class GuitarPro6 : public GuitarPro {
    // an integer stored in the header indicating that the file is not compressed (BCFS).
    const int gpx_header_uncompressed = 1397113666;
    // an integer stored in the header indicating that the file is not compressed (BCFZ).
    const int gpx_header_compressed = 1514554178;
    int position = 0;
    // a constant storing the amount of bits per byte
    const int bits_in_byte = 8;

    // contains all the information about notes that will go in the parts
    struct GPPartInfo {
        muse::XmlDomNode master_bars;
        muse::XmlDomNode bars;
        muse::XmlDomNode voices;
        muse::XmlDomNode beats;
        muse::XmlDomNode notes;
        muse::XmlDomNode rhythms;
    };

    void parse_file(const char *filename, muse::ByteArray *data);

    int read_bit(muse::ByteArray *buffer);
    muse::ByteArray get_bytes(muse::ByteArray *buffer, int offset, int length);
    void read_gpx(muse::ByteArray *buffer);
    void read_gpx(muse::ByteArray *buffer, int depth);
    int read_integer(muse::ByteArray *buffer, int offset);
    muse::ByteArray read_string(muse::ByteArray *buffer, int offset, int length);
    int read_bits(muse::ByteArray *buffer, int bits_to_read);
    int read_bits_reversed(muse::ByteArray *buffer, int bits_to_read);
    int find_num_measures(GPPartInfo *part_info);
    void read_master_tracks(muse::XmlDomNode *master_track);
    void read_drum_note(Note *note, int element, int variation);
    muse::XmlDomNode get_node(const muse::String &id, muse::XmlDomNode current_dom_node);
    void unhandled_node(muse::String node_name);
    void make_tie(Note *note);

    int read_beat_effects(int /*track*/, mu::engraving::Segment *) override { return 0; }

    std::map<std::pair<int, int>, Note *> slide_map;

protected:
    bool gpif_read_ = false;
    const static std::map<std::string, std::string> instrument_mapping;
    void read_gpif(muse::ByteArray *data);

    virtual std::unique_ptr<IGPDomBuilder> create_gp_dom_builder() const override;

public:
    GuitarPro6(mu::engraving::MasterScore *s) : GuitarPro(s, 6) {}

    GuitarPro6(mu::engraving::MasterScore *s, int v) : GuitarPro(s, v) {}

    bool read(muse::io::IODevice *) override;
};

class GuitarPro7 : public GuitarPro6 {
    virtual std::unique_ptr<IGPDomBuilder> create_gp_dom_builder() const override;

public:
    GuitarPro7(mu::engraving::MasterScore *s) : GuitarPro6(s, 7) {}

    bool read(muse::io::IODevice *) override;
    GPProperties read_properties(muse::ByteArray *data);
};
} // namespace mu::iex::guitarpro
