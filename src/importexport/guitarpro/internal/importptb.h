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
#ifndef MU_IMPORTEXPORT_IMPORTPTB_H
#define MU_IMPORTEXPORT_IMPORTPTB_H

#include "io/file.h"

#include "engraving/types/fraction.h"
#include "engraving/dom/score.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/drumset.h"

#include "engraving/engravingerrors.h"

namespace mu::engraving {
class PalmMute;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
class PowerTab {
    muse::io::IODevice *file = nullptr;
    mu::engraving::MasterScore *score = nullptr;

    bool read_boolean();
    unsigned char read_u_int8();
    char read_char();
    unsigned short read_short();
    int read_int();
    void skip(int len = 1);
    std::string read_string(int length = -1);

    struct PtSongInfo {
        int classification{0};
        int release_type{0};
        int album_type{0};
        int day{0};
        int month{0};
        int year{0};
        int style{0};
        int level{0};
        bool liverecording{0};
        std::string name;
        std::string interpret;
        std::string album;
        std::string author;
        std::string lyricist;
        std::string arranger;
        std::string guitar_transcriber;
        std::string bass_transcriber;
        std::string lyrics;
        std::string guitar_instructions;
        std::string bass_instructions;
        std::string instructions;
        std::string copyright;
    };

    struct PtComponent {
        enum Type { EMPTY, GUITAR_IN, TEMPO, SYMBOL, BAR, NOTE, BEAT, T_DIRECTION };

        virtual Type type() { return EMPTY; }

        virtual ~PtComponent() {}
    };

    struct StRhytmSlash final : public PtComponent {
        int position{0};
        int duration{0};
        bool triplet{false};
        bool tripletend{false};
        bool dotted{false};
        bool double_dotted{false};
        bool is_rest{false};
    };

    struct PtGuitarIn final : public PtComponent {
        int rhythm_slash{true};
        int staff{0};
        int trackinfo{0};
        int section{0};
        int position{0};

        Type type() override { return GUITAR_IN; }
    };

    struct PtSymbol final : public PtComponent {
        int value{0};

        PtSymbol(int val) : value(val) {}

        Type type() override { return SYMBOL; }
    };

    struct PtBar final : public PtComponent {
        int measure_no{0};
        int repeat_close{0};
        bool repeat_start{false};

        int numerator{0};
        int denominator{0};

        Type type() override { return BAR; }
    };

    std::vector<PtBar *> bars;

    struct PtNote final : public PtComponent {
        int value{0};
        int str{0};
        int bend{0};
        bool tied{false};
        bool dead{false};
        bool hammer{false};
        int slide{0};

        Type type() override { return NOTE; }
    };

    struct PtChord final : public PtComponent {
        int key;
        int formula;
        int modification;
        int extra;
        int top_fret;
        std::vector<int> frets;
    };

    struct PtChordText final : public PtComponent {
        int position;
        int key;
        int formula;
        int formula_mod;
        int extra;
    };

    struct PtBeat final : public PtComponent {
        int position{0};
        int staff{0};
        int voice{0};
        int mmrest{0};
        bool is_rest_value{false};
        int duration{0};
        int enters{0};
        int times{0};
        bool dotted{false};
        bool tuplet{false};
        bool double_dotted{false};
        bool vibrato{false};
        bool grace{false};
        bool arpeggio_up{false};
        bool arpeggio_down{false};
        bool palm_mute{false};
        bool accent{false};
        bool staccato{false};
        std::vector<PtNote> notes;

        PtBeat(int input_staff, int input_voice) : staff(input_staff), voice(input_voice) {}

        PtBeat() = default;

        Type type() override { return BEAT; }
    };

    struct PtDirection final : public PtComponent {
        enum Direction {
            DIRECTION_CODA = 0,
            DIRECTION_DOUBLE_CODA = 1,
            DIRECTION_SEGNO = 2,
            DIRECTION_SEGNO_SEGNO = 3,
            DIRECTION_FINE = 4,
            DIRECTION_DA_CAPO = 5,
            DIRECTION_DAL_SEGNO = 6,
            DIRECTION_DAL_SEGNO_SEGNO = 7,
            DIRECTION_TO_CODA = 8,
            DIRECTION_TO_DOUBLE_CODA = 9,
            DIRECTION_DA_CAPO_AL_CODA = 10,
            DIRECTION_DA_CAPO_AL_DOUBLE_CODA = 11,
            DIRECTION_DAL_SEGNO_AL_CODA = 12,
            DIRECTION_DAL_SEGNO_AL_DOUBLE_CODA = 13,
            DIRECTION_DAL_SEGNO_SEGNO_AL_CODA = 14,
            DIRECTION_DAL_SEGNO_SEGNO_AL_DOUBLE_CODA = 15,
            DIRECTION_DA_CAPO_AL_FINE = 16,
            DIRECTION_DAL_SEGNO_AL_FINE = 17,
            DIRECTION_DAL_SEGNO_SEGNO_AL_FINE = 18
        };

        enum ActiveSym { ACTIVE_SYMBOL_NONE = 0, ACTIVE_SYMBOL_DC = 1, ACTIVE_SYMBOL_DS = 2, ACTIVE_SYMBOL_DSS = 3 };

        Direction direction{DIRECTION_CODA};
        ActiveSym active_symbol{ACTIVE_SYMBOL_NONE};
        int repeat{0};

        PtDirection(int dir, int sym, int rep) : direction((Direction) dir), active_symbol((ActiveSym) sym), repeat(rep) {}

        Type type() override { return T_DIRECTION; }
    };

    struct PtPosition {
        int position{0};
        std::vector<std::shared_ptr<PtComponent>> components;
        void add_component(PtComponent *c);
    };

    struct TrackInfo {
        int number{0};
        std::string name;
        int instrument{0};
        int volume{0};
        int balance{0};
        int reverb{0};
        int chorus{0};
        int tremolo{0};
        int phaser{0};
        int capo{0};
        std::string tuning_name;
        int offset{0};
        std::vector<int> strings;
        int notes_count{0};
    };

    typedef std::list<PtBeat> tBeatList;

    struct PtTrack;

    struct PtSection {
        int number{0};
        int staves{0};
        std::string part_name;
        char part_marker{0};

        std::vector<PtPosition> positions;

        std::vector<int> staff_map;

        int tempo{0};

        std::list<StRhytmSlash> rhythm;

        std::map<int, PtChordText> chord_text_map;
        std::vector<tBeatList> beats;
        std::list<std::shared_ptr<PtBar>> bars;
        bool readed{false};

        void copy_tracks(PtTrack *);

        PtSection(int num);
        PtPosition &get_position(int pos);
        int get_next_position_number();
    };

    struct ChordData {
        int a[3];

        bool operator>(const ChordData &other) const {
            if (a[0] == other.a[0]) {
                if (a[1] == other.a[1]) {
                    return a[2] > other.a[2];
                }
                return a[1] > other.a[1];
            }
            return a[0] > other.a[0];
        }

        bool operator<(const ChordData &other) const {
            if (a[0] == other.a[0]) {
                if (a[1] == other.a[1]) {
                    return a[2] < other.a[2];
                }
                return a[1] < other.a[1];
            }
            return a[0] < other.a[0];
        }

        bool operator==(const ChordData &other) const { return memcmp(a, other.a, 3 * sizeof(int)) == 0; }
    };

    struct PtTrack {
        std::vector<TrackInfo> infos;
        std::vector<PtSection> sections;
        PtSection &get_section(int ind);
        std::map<ChordData, PtChord> diagram_map;

        std::list<PtGuitarIn> guitar_ins;
    };

    struct PtSong {
        PtSongInfo info;
        PtTrack track1;
        PtTrack track2;
    };

    void read_song_info(PtSongInfo &info);
    void read_data_instruments(PtTrack &info);
    void read_track_info(PtTrack &info);
    void read_guitar_in(PtTrack &info);
    void read_tempo_marker(PtTrack &info);
    void read_section_symbol(PtTrack &info);
    void read_section(PtSection &sec);
    void read_bar_line(PtSection &sec);
    void read_direction(PtSection &sec);
    void read_time_signature(PtBar *bar);
    void read_staff(int staff, PtSection &sec);
    void read_position(int staff, int voice, PtSection &sec);
    void read_note(PtBeat *beat);
    void read_rehearsal_sign(PtSection &sec);
    void read_chord_text(PtSection &sec);
    void read_chord(PtTrack &si);
    void read_rhytm_slash(PtSection &sec);

    void read_floating_text();
    void read_font_settings();
    void read_dynamic();
    void read_key_signature();

    bool read_version();
    int read_header_items();

    std::vector<int> last_staff_map;
    std::vector<int> get_staff_map(PtSection &sec);
    int repeat_count{0};
    void add_to_score(PtSection &sec);

    mu::engraving::Measure *create_measure(PtBar *bar, const mu::engraving::Fraction &tick);
    void fill_measure(tBeatList &elist, mu::engraving::Measure *measure, int staff, std::vector<mu::engraving::Note *> &);

    int staves{0};

    PtTrack *cur_track;
    int staff_inc{0};
    char last_part{0};

    PtSection *cur_section;

    std::vector<mu::engraving::PalmMute *> palm_mutes;
    void add_palm_mute(mu::engraving::Chord *);

public:
    PowerTab(muse::io::IODevice *f, mu::engraving::MasterScore *s) : file(f), score(s) {}

    mu::engraving::Err read();
};
} // namespace mu::iex::guitarpro

#endif // MU_IMPORTEXPORT_IMPORTPTB_H
