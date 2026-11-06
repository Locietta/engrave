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
#include "importmidi_instrument.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "engraving/dom/drumset.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/staff.h"
#include "engraving/editing/editstaffbrackets.h"
#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_operations.h"
#include "internal/midishared/generalmidi.h"

using namespace std::literals;
using namespace mu::engraving;

namespace mu::engraving {
extern std::vector<const InstrumentGroup *> instrument_groups;
} // namespace mu::engraving

namespace mu::iex::midi { namespace MidiInstr {
namespace {
struct GM1ProgramData {
    GM1Program program = {};
    std::string_view preferred_instrument_id = {};
};

// General MIDI Level 1
constexpr std::array g_m1_sound_data{
    GM1ProgramData{GM1Program::ACOUSTIC_GRAND_PIANO, "grand-piano"sv},
    GM1ProgramData{GM1Program::BRIGHT_ACOUSTIC_PIANO, "piano"sv},
    GM1ProgramData{GM1Program::ELECTRIC_GRAND_PIANO, "electric-piano"sv},
    GM1ProgramData{GM1Program::HONKY_TONK_PIANO, "honky-tonk-piano"sv},
    GM1ProgramData{GM1Program::ELECTRIC_PIANO1, "electric-piano"sv},
    GM1ProgramData{GM1Program::ELECTRIC_PIANO2, "electric-piano"sv},
    GM1ProgramData{GM1Program::HARPSICHORD, "harpsichord"sv},
    GM1ProgramData{GM1Program::CLAVI, "clavichord"sv},
    GM1ProgramData{GM1Program::CELESTA, "celesta"sv},
    GM1ProgramData{GM1Program::GLOCKENSPIEL, "glockenspiel"sv},
    GM1ProgramData{GM1Program::MUSIC_BOX, "crotales"sv},
    GM1ProgramData{GM1Program::VIBRAPHONE, "vibraphone"sv},
    GM1ProgramData{GM1Program::MARIMBA, "marimba"sv},
    GM1ProgramData{GM1Program::XYLOPHONE, "xylophone"sv},
    GM1ProgramData{GM1Program::TUBULAR_BELLS, "tubular-bells"sv},
    GM1ProgramData{GM1Program::DULCIMER, "dulcimer"sv},
    GM1ProgramData{GM1Program::DRAWBAR_ORGAN, "hammond-organ"sv},
    GM1ProgramData{GM1Program::PERCUSSIVE_ORGAN, "percussive-organ"sv},
    GM1ProgramData{GM1Program::ROCK_ORGAN, "rotary-organ"sv},
    GM1ProgramData{GM1Program::CHURCH_ORGAN, "pipe-organ"sv},
    GM1ProgramData{GM1Program::REED_ORGAN, "reed-organ"sv},
    GM1ProgramData{GM1Program::ACCORDION, "accordion"sv},
    GM1ProgramData{GM1Program::HARMONICA, "harmonica"sv},
    GM1ProgramData{GM1Program::TANGO_ACCORDION, "bandoneon"sv},
    GM1ProgramData{GM1Program::ACOUSTIC_GUITAR_NYLON, "guitar-nylon"sv},
    GM1ProgramData{GM1Program::ACOUSTIC_GUITAR_STEEL, "guitar-steel"sv},
    GM1ProgramData{GM1Program::ELECTRIC_GUITAR_JAZZ, "electric-guitar"sv},
    GM1ProgramData{GM1Program::ELECTRIC_GUITAR_CLEAN, "electric-guitar"sv},
    GM1ProgramData{GM1Program::ELECTRIC_GUITAR_MUTED, "electric-guitar"sv},
    GM1ProgramData{GM1Program::OVERDRIVEN_GUITAR, "electric-guitar"sv},
    GM1ProgramData{GM1Program::DISTORTION_GUITAR, "electric-guitar"sv},
    GM1ProgramData{GM1Program::GUITAR_HARMONICS, "electric-guitar"sv},
    GM1ProgramData{GM1Program::ACOUSTIC_BASS, "acoustic-bass"sv},
    GM1ProgramData{GM1Program::ELECTRIC_BASS_FINGER, "electric-bass"sv},
    GM1ProgramData{GM1Program::ELECTRIC_BASS_PICK, "bass-guitar"sv},
    GM1ProgramData{GM1Program::FRETLESS_BASS, "fretless-electric-bass"sv},
    GM1ProgramData{GM1Program::SLAP_BASS1, "electric-bass"sv},
    GM1ProgramData{GM1Program::SLAP_BASS2, "electric-bass"sv},
    GM1ProgramData{GM1Program::SYNTH_BASS1, "bass-synthesizer"sv},
    GM1ProgramData{GM1Program::SYNTH_BASS2, "bass-synthesizer"sv},
    GM1ProgramData{GM1Program::VIOLIN, "violin"sv},
    GM1ProgramData{GM1Program::VIOLA, "viola"sv},
    GM1ProgramData{GM1Program::CELLO, "violoncello"sv},
    GM1ProgramData{GM1Program::CONTRABASS, "contrabass"sv},
    GM1ProgramData{GM1Program::TREMOLO_STRINGS, "strings"sv},
    GM1ProgramData{GM1Program::PIZZICATO_STRINGS, "strings"sv},
    GM1ProgramData{GM1Program::ORCHESTRAL_HARP, "harp"sv},
    GM1ProgramData{GM1Program::TIMPANI, "timpani"sv},
    GM1ProgramData{GM1Program::STRING_ENSEMBLE1, "strings"sv},
    GM1ProgramData{GM1Program::STRING_ENSEMBLE2, "strings"sv},
    GM1ProgramData{GM1Program::SYNTH_STRINGS1, "string-synthesizer"sv},
    GM1ProgramData{GM1Program::SYNTH_STRINGS2, "string-synthesizer"sv},
    GM1ProgramData{GM1Program::CHOIR_AAHS, "voice"sv},
    GM1ProgramData{GM1Program::VOICE_OOHS, "voice"sv},
    GM1ProgramData{GM1Program::SYNTH_VOICE, "voice"sv},
    GM1ProgramData{GM1Program::ORCHESTRA_HIT, "brass-synthesizer"sv},
    GM1ProgramData{GM1Program::TRUMPET, "trumpet"sv},
    GM1ProgramData{GM1Program::TROMBONE, "trombone"sv},
    GM1ProgramData{GM1Program::TUBA, "tuba"sv},
    GM1ProgramData{GM1Program::MUTED_TRUMPET, "trumpet"sv},
    GM1ProgramData{GM1Program::FRENCH_HORN, "horn"sv},
    GM1ProgramData{GM1Program::BRASS_SECTION, "brass"sv},
    GM1ProgramData{GM1Program::SYNTH_BRASS1, "brass-synthesizer"sv},
    GM1ProgramData{GM1Program::SYNTH_BRASS2, "brass-synthesizer"sv},
    GM1ProgramData{GM1Program::SOPRANO_SAX, "soprano-saxophone"sv},
    GM1ProgramData{GM1Program::ALTO_SAX, "alto-saxophone"sv},
    GM1ProgramData{GM1Program::TENOR_SAX, "tenor-saxophone"sv},
    GM1ProgramData{GM1Program::BARITONE_SAX, "baritone-saxophone"sv},
    GM1ProgramData{GM1Program::OBOE, "oboe"sv},
    GM1ProgramData{GM1Program::ENGLISH_HORN, "english-horn"sv},
    GM1ProgramData{GM1Program::BASSOON, "bassoon"sv},
    GM1ProgramData{GM1Program::CLARINET, "clarinet"sv},
    GM1ProgramData{GM1Program::PICCOLO, "piccolo"sv},
    GM1ProgramData{GM1Program::FLUTE, "flute"sv},
    GM1ProgramData{GM1Program::RECORDER, "recorder"sv},
    GM1ProgramData{GM1Program::PAN_FLUTE, "pan-flute"sv},
    GM1ProgramData{GM1Program::BLOWN_BOTTLE, "pan-flute"sv},
    GM1ProgramData{GM1Program::SHAKUHACHI, "shakuhachi"sv},
    GM1ProgramData{GM1Program::WHISTLE, "c-tin-whistle"sv},
    GM1ProgramData{GM1Program::OCARINA, "ocarina"sv},
    GM1ProgramData{GM1Program::LEAD1_SQUARE, "square-synth"sv},
    GM1ProgramData{GM1Program::LEAD2_SAWTOOTH, "saw-synth"sv},
    GM1ProgramData{GM1Program::LEAD3_CALLIOPE, ""sv},
    GM1ProgramData{GM1Program::LEAD4_CHIFF, ""sv},
    GM1ProgramData{GM1Program::LEAD5_CHARANG, "tuned-klaxon-horns"sv},
    GM1ProgramData{GM1Program::LEAD6_VOICE, "kazoo"sv},
    GM1ProgramData{GM1Program::LEAD7_FIFTHS, ""sv},
    GM1ProgramData{GM1Program::LEAD8_BASS_AND_LEAD, ""sv},
    GM1ProgramData{GM1Program::PAD1_NEW_AGE, "new-age-synth"sv},
    GM1ProgramData{GM1Program::PAD2_WARM, "warm-synth"sv},
    GM1ProgramData{GM1Program::PAD3_POLYSYNTH, "poly-synth"sv},
    GM1ProgramData{GM1Program::PAD4_CHOIR, "choir-synth"sv},
    GM1ProgramData{GM1Program::PAD5_BOWED, "bowed-synth"sv},
    GM1ProgramData{GM1Program::PAD6_METALLIC, "metallic-synth"sv},
    GM1ProgramData{GM1Program::PAD7_HALO, "halo-synth"sv},
    GM1ProgramData{GM1Program::PAD8_SWEEP, "sweep-synth"sv},
    GM1ProgramData{GM1Program::F_X1_RAIN, "rain-synth"sv},
    GM1ProgramData{GM1Program::F_X2_SOUNDTRACK, "soundtrack-synth"sv},
    GM1ProgramData{GM1Program::F_X3_CRYSTAL, "crystal-synth"sv},
    GM1ProgramData{GM1Program::F_X4_ATMOSPHERE, "atmosphere-synth"sv},
    GM1ProgramData{GM1Program::F_X5_BRIGHTNESS, "brightness-synth"sv},
    GM1ProgramData{GM1Program::F_X6_GOBLINS, "goblins-synth"sv},
    GM1ProgramData{GM1Program::F_X7_ECHOES, "echoes-synth"sv},
    GM1ProgramData{GM1Program::F_X8_SCI_FI, "sci-fi-synth"sv},
    GM1ProgramData{GM1Program::SITAR, "sitar"sv},
    GM1ProgramData{GM1Program::BANJO, "banjo"sv},
    GM1ProgramData{GM1Program::SHAMISEN, "shamisen"sv},
    GM1ProgramData{GM1Program::KOTO, "koto"sv},
    GM1ProgramData{GM1Program::KALIMBA, "kalimba"sv},
    GM1ProgramData{GM1Program::BAG_PIPE, "bagpipe"sv},
    GM1ProgramData{GM1Program::FIDDLE, "violin"sv},
    GM1ProgramData{GM1Program::SHANAI, "shenai"sv},
    GM1ProgramData{GM1Program::TINKLE_BELL, "hand-bells"sv},
    GM1ProgramData{GM1Program::AGOGO, "hand-bells"sv},
    GM1ProgramData{GM1Program::STEEL_DRUMS, "steel-drums"sv},
    GM1ProgramData{GM1Program::WOODBLOCK, "temple-blocks"sv},
    GM1ProgramData{GM1Program::TAIKO_DRUM, ""sv},
    GM1ProgramData{GM1Program::MELODIC_TOM, "roto-toms"sv},
    GM1ProgramData{GM1Program::SYNTH_DRUM, ""sv},
    GM1ProgramData{GM1Program::REVERSE_CYMBAL, ""sv},
    GM1ProgramData{GM1Program::GUITAR_FRET_NOISE, ""sv},
    GM1ProgramData{GM1Program::BREATH_NOISE, ""sv},
    GM1ProgramData{GM1Program::SEASHORE, ""sv},
    GM1ProgramData{GM1Program::BIRD_TWEET, ""sv},
    GM1ProgramData{GM1Program::TELEPHONE_RING, ""sv},
    GM1ProgramData{GM1Program::HELICOPTER, ""sv},
    GM1ProgramData{GM1Program::APPLAUSE, ""sv},
    GM1ProgramData{GM1Program::GUNSHOT, "cannon"sv},
};
static_assert(::mu::iex::midi::MidiInstr::g_m1_sound_data.size() == 128);

const InstrumentTemplate *get_preferred_instrument(const GM1Program sound) {
    const std::string_view id = ::mu::iex::midi::MidiInstr::g_m1_sound_data[static_cast<std::uint8_t>(sound)].preferred_instrument_id;
    InstrumentIndex idx = search_template_index_for_id(String::from_utf8(id));
    if (idx.group_index == -1) {
        return nullptr;
    }

    return idx.instr_template;
}
} // namespace

muse::String instrument_name(GM1Program program, bool is_drum_track) {
    if (is_drum_track) {
        return muse::String::from_utf8("Percussion");
    }

    return muse::String::from_utf8(get_midi_name(program));
}

bool is_grand_staff(const MTrack &t1, const MTrack &t2) {
    return t1.mtrack->out_channel() == t2.mtrack->out_channel() && MChord::is_grand_staff_program(t1.program) && !t1.mtrack->drum_track() &&
           !t2.mtrack->drum_track();
}

bool is_same_channel(const MTrack &t1, const MTrack &t2) { return t1.mtrack->out_channel() == t2.mtrack->out_channel(); }

bool is3_staff_organ(GM1Program program) { return program >= GM1Program::DRAWBAR_ORGAN && program <= GM1Program::REED_ORGAN; }

bool are_next2_grand_staff(int current_track, const std::vector<MTrack> &tracks) {
    if (current_track + 1 >= tracks.size()) {
        return false;
    }
    return is_grand_staff(tracks[current_track], tracks[current_track + 1]);
}

bool are_next3_organ_staff(int current_track, const std::vector<MTrack> &tracks) {
    if (current_track + 2 >= tracks.size()) {
        return false;
    }
    if (!is3_staff_organ(tracks[current_track].program)) {
        return false;
    }

    return is_grand_staff(tracks[current_track], tracks[current_track + 1]) &&
           is_same_channel(tracks[current_track + 1], tracks[current_track + 2]);
}

muse::String concatenate_with_comma(const muse::String &left, const muse::String &right) {
    if (left.is_empty()) {
        return right;
    }
    if (right.is_empty()) {
        return left;
    }
    return left + u", " + right;
}

// set program equal to all staves, as it should be
// often only first stave in Grand Staff have correct program, other - default (piano)
// also handle track names
void set_grand_staff_program(std::vector<MTrack> &tracks) {
    for (int i = 0; i < tracks.size(); ++i) {
        MTrack &mt = tracks[i];

        if (are_next3_organ_staff(i, tracks)) {
            tracks[i + 1].program = mt.program;
            tracks[i + 2].program = mt.program;

            mt.name = concatenate_with_comma(mt.name, muse::String::from_utf8("Manual"));
            tracks[i + 1].name = muse::String();
            tracks[i + 2].name = concatenate_with_comma(tracks[i + 2].name, muse::String::from_utf8("Pedal"));

            i += 2;
        } else if (are_next2_grand_staff(i, tracks)) {
            tracks[i + 1].program = mt.program;
            if (mt.name != tracks[i + 1].name) {
                mt.name = muse::String();            // only one name place near bracket is available
                tracks[i + 1].name = muse::String(); // so instrument name will be used instead
            }
            i += 1;
        }
    }
}

std::set<int> find_all_pitches(const MTrack &track) {
    std::set<int> pitches;
    for (const auto &chord : track.chords) {
        for (const auto &note : chord.second.notes) {
            pitches.insert(note.pitch);
        }
    }
    return pitches;
}

std::set<int> find_not_empty_drum_pitches(const Drumset *drumset) {
    std::set<int> drum_pitches = {};
    for (int i = 0; i != ::mu::engraving::drum_instruments; ++i) {
        if (!drumset->name(i).empty()) {
            drum_pitches.insert(i);
        }
    }

    return drum_pitches;
}

static bool has_not_defined_drum_pitch(const std::set<int> &track_pitches, const std::set<int> &drum_pitches) {
    for (const int pitch : track_pitches) {
        if (drum_pitches.find(pitch) == drum_pitches.end()) {
            return true;
        }
    }

    return false;
}

static const InstrumentTemplate *find_instrument(const String &group_id, const String &instr_id) {
    const InstrumentTemplate *instr = nullptr;

    for (const InstrumentGroup *group : instrument_groups) {
        if (group->id == group_id) {
            for (const InstrumentTemplate *templ : group->instrument_templates) {
                if (templ->id == instr_id) {
                    instr = templ;
                    break;
                }
            }
            break;
        }
    }
    return instr;
}

// returns number of pitches which are present in p1, but missing in p2
static int count_missing_pitches(const std::set<int> &p1, const std::set<int> &p2) {
    std::vector<int> missing_pitches = {};
    missing_pitches.reserve(p1.size());

    std::set_difference(p1.begin(), p1.end(), p2.begin(), p2.end(), std::back_inserter(missing_pitches));

    return static_cast<int>(missing_pitches.size());
}

// find instrument with maximum MIDI program
// that is less than the track MIDI program, i.e. suitable instrument
static const InstrumentTemplate *find_closest_instrument(const GM1Program program, const std::set<int> &pitches, const bool is_drum_track) {
    int max_less_program = -1;
    int min_missing_pitches = std::numeric_limits<int>::max();
    const InstrumentTemplate *closest_template = nullptr;

    for (const InstrumentGroup *group : instrument_groups) {
        for (const InstrumentTemplate *templ : group->instrument_templates) {
            if (templ->staff_group == StaffGroup::TAB) {
                continue;
            }
            if (is_drum_track != templ->use_drumset) {
                continue;
            }

            if (is_drum_track) {
                int missing_pitches = std::numeric_limits<int>::max();
                if (templ->drumset) {
                    const std::set<int> drum_pitches = find_not_empty_drum_pitches(templ->drumset);
                    missing_pitches = count_missing_pitches(pitches, drum_pitches);
                }

                for (const auto &channel : templ->channel) {
                    if (channel.program() < to_midi_data(program) && missing_pitches < min_missing_pitches) {
                        max_less_program = channel.program();
                        min_missing_pitches = missing_pitches;

                        closest_template = templ;
                        break;
                    }
                }

                continue;
            }

            for (const auto &channel : templ->channel) {
                if (channel.program() < to_midi_data(program) && channel.program() > max_less_program) {
                    max_less_program = channel.program();
                    closest_template = templ;
                    break;
                }
            }
        }
    }
    return closest_template;
}

static std::vector<const InstrumentTemplate *> find_instruments_for_program(const MTrack &track) {
    std::vector<const InstrumentTemplate *> suitable_templates;
    const GM1Program program = track.program;
    const bool is_drum_track = track.mtrack->drum_track();

    const InstrumentTemplate *pref_instr = get_preferred_instrument(program);
    if (pref_instr && !is_drum_track) {
        suitable_templates.push_back(pref_instr);
    }

    std::set<int> track_pitches;
    if (is_drum_track) {
        track_pitches = find_all_pitches(track);
    }

    for (const InstrumentGroup *group : instrument_groups) {
        for (const InstrumentTemplate *templ : group->instrument_templates) {
            if (pref_instr && templ->id == pref_instr->id) {
                continue;
            }
            if (templ->staff_group == StaffGroup::TAB) {
                continue;
            }
            if (is_drum_track != templ->use_drumset) {
                continue;
            }

            if (is_drum_track) {
                std::set<int> drum_pitches;
                if (templ->drumset) {
                    drum_pitches = find_not_empty_drum_pitches(templ->drumset);
                }

                for (const auto &channel : templ->channel) {
                    if (channel.program() == to_midi_data(program)) {
                        if (templ->drumset) {
                            if (has_not_defined_drum_pitch(track_pitches, drum_pitches)) {
                                break;
                            }
                        }
                        suitable_templates.push_back(templ);
                        break;
                    }
                }

                continue;
            }

            for (const auto &channel : templ->channel) {
                if (channel.program() == to_midi_data(program)) {
                    suitable_templates.push_back(templ);
                    break;
                }
            }
        }
    }

    if (suitable_templates.empty()) {
        // Ms instruments with the desired MIDI program were not found
        // so we will find the most matching instrument

        if (program >= GM1Program::LEAD1_SQUARE && program <= GM1Program::F_X8_SCI_FI) {
            const InstrumentTemplate *instr = nullptr;
            if (track.mtrack->drum_track()) {
                instr = find_instrument(u"electronic-instruments", u"percussion-synthesizer");
            } else {
                instr = find_instrument(u"electronic-instruments", u"effect-synth");
            }
            if (instr) {
                suitable_templates.push_back(instr); // generic synth
            }
        } else if (program >= GM1Program::TINKLE_BELL) {
            auto instr = find_instrument(u"unpitched-percussion", u"snare-drum");
            if (instr) {
                suitable_templates.push_back(instr); // 1-line percussion staff
            }
        } else { // find instrument with maximum MIDI program
                 // that is less than the track MIDI program, i.e. suitable instrument
            auto instr = find_closest_instrument(program, track_pitches, is_drum_track);
            if (instr) {
                suitable_templates.push_back(instr);
            }
        }
    }

    return suitable_templates;
}

static std::pair<int, int> find_min_max_pitch(const MTrack &track) {
    int min_pitch = std::numeric_limits<int>::max();
    int max_pitch = -1;

    for (const auto &chord : track.chords) {
        for (const auto &note : chord.second.notes) {
            min_pitch = std::min(min_pitch, note.pitch);
            max_pitch = std::max(max_pitch, note.pitch);
        }
    }
    return std::pair(min_pitch, max_pitch);
}

static int find_max_pitch_diff(const std::pair<int, int> &min_max_pitch, const InstrumentTemplate *templ) {
    int diff = 0;
    if (min_max_pitch.first < templ->min_pitch_p) {
        diff = templ->min_pitch_p - min_max_pitch.first;
    }
    if (min_max_pitch.second > templ->max_pitch_p) {
        diff = std::max(diff, min_max_pitch.second - templ->max_pitch_p);
    }

    return diff;
}

static bool has_common_genre(const std::vector<const InstrumentGenre *> &genres) {
    for (const InstrumentGenre *genre : genres) {
        if (genre->id == "common") {
            return true;
        }
    }
    return false;
}

void sort_instrument_templates(std::vector<const InstrumentTemplate *> &templates, const std::pair<int, int> &min_max_pitch) {
    std::stable_sort(templates.begin(), templates.end(),
                     [min_max_pitch](const InstrumentTemplate *templ1, const InstrumentTemplate *templ2) {
                         const int diff1 = find_max_pitch_diff(min_max_pitch, templ1);
                         const int diff2 = find_max_pitch_diff(min_max_pitch, templ2);

                         if (diff1 != diff2) {
                             return diff1 < diff2;
                         }
                         // if drumset is not null - it's a particular drum instrument
                         // if drum set is null - it's a common drumset
                         // so prefer particular drum instruments
                         if (templ1->drumset && !templ2->drumset) {
                             return true;
                         }
                         if (!templ1->drumset && templ2->drumset) {
                             return false;
                         }
                         // prefer instruments with the "common" genre
                         const bool has_common1 = has_common_genre(templ1->genres);
                         const bool has_common2 = has_common_genre(templ2->genres);
                         if (has_common1 && !has_common2) {
                             return true;
                         }
                         if (!has_common1 && has_common2) {
                             return false;
                         }
                         return templ1->genres.size() > templ2->genres.size();
                     });
}

std::vector<const InstrumentTemplate *> find_suitable_instruments(const MTrack &track) {
    std::vector<const InstrumentTemplate *> templates = find_instruments_for_program(track);
    if (templates.empty()) {
        return templates;
    }

    const std::pair<int, int> min_max_pitch = find_min_max_pitch(track);
    sort_instrument_templates(templates, min_max_pitch);

    // add empty instrument to show no-instrument option
    if (!templates.empty()) {
        templates.push_back(nullptr);
    }

    return templates;
}

void find_instruments_for_all_tracks(const std::vector<MTrack> &tracks, bool force_reload) {
    auto &opers = midi_import_operations;
    auto &instr_list_option = opers.data()->track_opers.ms_instr_list;

    if (force_reload) {
        instr_list_option.clear();
    }

    if (opers.data()->processings_of_opened_file == 0 || force_reload) {
        // create instrument list on MIDI file opening
        for (const auto &track : tracks) {
            instr_list_option.set_value(track.index_of_operation, find_suitable_instruments(track));
            if (!instr_list_option.value(track.index_of_operation).empty()) {
                const int default_instr_index = 0;
                opers.data()->track_opers.ms_instr_index.set_default_value(default_instr_index);
            }
        }
    }
}

void instrument_templates_changed() {
    const std::vector<std::string> files = midi_import_operations.all_midi_files();
    for (const std::string &file : files) {
        MidiOperations::CurrentMidiFileSetter s(midi_import_operations, file);
        MidiOperations::FileData *data = midi_import_operations.data();
        if (data) {
            find_instruments_for_all_tracks(data->tracks, /* forceReload */ true);
        }
    }
}

void create_instruments(Score *score, std::vector<MTrack> &tracks) {
    const auto &opers = midi_import_operations;
    const auto &instr_list_option = opers.data()->track_opers.ms_instr_list;

    const int ntracks = tracks.size();
    for (int idx = 0; idx < ntracks; ++idx) {
        MTrack &track = tracks[idx];
        Part *part = new Part(score);

        const auto &instr_list = instr_list_option.value(track.index_of_operation);
        const InstrumentTemplate *instr = nullptr;
        if (!instr_list.empty()) {
            const int instr_index = opers.data()->track_opers.ms_instr_index.value(track.index_of_operation);
            instr = instr_list[instr_index];
            if (instr) {
                part->init_from_instr_template(instr);
            }
        }

        if (are_next3_organ_staff(idx, tracks)) {
            part->set_staves(3);
        } else if (are_next2_grand_staff(idx, tracks)) {
            part->set_staves(2);
        } else {
            part->set_staves(1);
        }

        Staff *staff = part->staff(0);
        if (part->nstaves() == 1) {
            if (track.mtrack->drum_track()) {
                staff->set_staff_type(Fraction(0, 1), *StaffType::preset(StaffTypes::PERC_DEFAULT));
                if (!instr) {
                    part->instrument()->set_drumset(sm_drumset);
                }
            }
        } else {
            if (!instr) {
                staff->set_bar_line_span(true);
                EditStaffBrackets::set_bracket_type(score, staff->idx(), 0, BracketType::BRACE);
            } else {
                staff->set_bar_line_span(instr->barline_span[0]);
                EditStaffBrackets::set_bracket_type(score, staff->idx(), 0, instr->bracket[0]);
            }
            EditStaffBrackets::set_bracket_span(score, staff->idx(), 0, 2);
        }

        if (instr) {
            for (size_t i = 0; i != part->nstaves(); ++i) {
                if (instr->staff_type_preset) {
                    part->staff(i)->init(instr, nullptr, static_cast<int>(i));
                    part->staff(i)->set_staff_type(Fraction(0, 1), *(instr->staff_type_preset));
                }
                //                        part->staff(i)->setLines(0, instr->staffLines[i]);
                //                        part->staff(i)->setSmall(0, instr->smallStaff[i]);
                part->staff(i)->set_default_clef_type(instr->clef_types[i]);
            }
        }

        for (size_t i = 0; i != part->nstaves(); ++i) {
            if (i > 0) {
                ++idx;
            }
            tracks[idx].staff = part->staff(i);
        }

        // only importing a single volume per track here, skip when multiple volumes
        // are defined, or the single volume is not defined on tick 0.
        if (track.volumes.size() == 1) {
            for (auto &i : track.volumes) {
                if (i.first == ReducedFraction(0, 1)) {
                    part->instrument()->channel(0)->set_volume(i.second);
                }
            }
        }

        score->append_part(part);
    }
}

muse::String ms_instr_name(int track_index) {
    const auto &opers = midi_import_operations.data()->track_opers;

    const int instr_index = opers.ms_instr_index.value(track_index);
    const auto &track_instr_list = opers.ms_instr_list.value(track_index);
    const InstrumentTemplate *instr = (track_instr_list.empty()) ? nullptr : track_instr_list[instr_index];
    if (!instr) {
        return muse::String();
    }

    if (!instr->instrument_name.long_name().empty()) {
        return instr->instrument_name.long_name();
    }

    return instr->track_name;
}
}} // namespace mu::iex::midi::MidiInstr
