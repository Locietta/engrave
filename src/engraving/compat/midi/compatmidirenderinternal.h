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

#ifndef __RENDERMIDI_H__
#define __RENDERMIDI_H__

#include <memory>

#include "../../dom/measure.h"
#include "../../dom/mscore.h"
#include "../../types/types.h"

#include "pitchwheelrenderer.h"
#include "pausemap.h"
#include "velocitymap.h"

namespace mu::engraving {
struct EventsHolder;
struct MasterScore;
struct Staff;
struct Instrument;
struct Chord;

// This struct specifies how to render an articulation.
//   atype - the articulation type to implement, such as SymId::ornamentTurn
//   ostyles - the actual ornament has a property called ornamentStyle whose value is
//             a value of type OrnamentStyle.  This ostyles field indicates the
//             the set of ornamentStyles which apply to this rendition.
//   duration - the default duration for each note in the rendition, the final duration
//            rendered might be less than this if an articulation is attached to a note of
//            short duration.
//   prefix - vector of integers. indicating which notes to play at the beginning of rendering the
//            articulation.  0 represents the principle note, 1==> the note diatonically 1 above
//            -1 ==> the note diatonically 1 below.  E.g., in the key of G, if a turn articulation
//            occurs above the note F#, then 0==>F#, 1==>G, -1==>E.
//            These integers indicate which notes actual notes to play when rendering the ornamented
//            note.   However, if the same integer appears several times adjacently such as {0,0,0,1}
//            That means play the notes tied.  e.g., F# followed by G, but the duration of F# is 3x the
//            duration of the G.
//    body   - notes to play comprising the body of the rendered ornament.
//            The body differs from the prefix and suffix in several ways.
//            * body does not support tied notes: {0,0,0,1} means play 4 distinct notes (not tied).
//            * if there is sufficient duration in the principle note, AND repeatp is true, then body
//               will be rendered multiple times, as the duration allows.
//            * to avoid a time gap (or rest) in rendering the articulation, if sustainp is true,
//               then the final note of the body will be sustained to fill the left-over time.
//    suffix - similar to prefix but played once at the end of the rendered ornament.
//    repeatp  - whether the body is repeatable in its entirety.
//    sustainp - whether the final note of the body should be sustained to fill the remaining duration.
struct OrnamentExcursion {
    SymId atype;
    std::set<OrnamentStyle> ostyles;
    int duration;
    std::vector<int> prefix;
    std::vector<int> body;
    bool repeatp;
    bool sustainp;
    std::vector<int> suffix;
};

static const std::set<OrnamentStyle> baroque = {OrnamentStyle::BAROQUE};
static const std::set<OrnamentStyle> defstyle = {OrnamentStyle::DEFAULT};
static const std::set<OrnamentStyle> any; // empty set has the special meaning of any-style, rather than no-styles.
static constexpr int sixteenth_ticks = Constants::division / 4;
static constexpr int thirty_second_ticks = sixteenth_ticks / 2;
constexpr int slide_duration = thirty_second_ticks;
constexpr int grace_bend_duration_ticks = sixteenth_ticks;
static const std::vector<OrnamentExcursion> excursions = {
    //  articulation type            set of  duration       body         repeatp      suffix
    //                               styles          prefix                    sustainp
    {SymId::ORNAMENT_TURN, any, thirty_second_ticks, {}, {1, 0, -1, 0}, false, true, {}},
    {SymId::ORNAMENT_TURN_INVERTED, any, thirty_second_ticks, {}, {-1, 0, 1, 0}, false, true, {}},
    {SymId::ORNAMENT_TURN_SLASH, any, thirty_second_ticks, {}, {-1, 0, 1, 0}, false, true, {}},
    {SymId::ORNAMENT_TRILL, baroque, thirty_second_ticks, {1, 0}, {1, 0}, true, true, {}},
    {SymId::ORNAMENT_TRILL, defstyle, thirty_second_ticks, {0, 1}, {0, 1}, true, true, {}},
    {SymId::BRASS_MUTE_CLOSED, baroque, thirty_second_ticks, {0, -1}, {0, -1}, true, true, {}},
    {SymId::ORNAMENT_MORDENT, any, thirty_second_ticks, {}, {0, -1, 0}, false, true, {}},
    {SymId::ORNAMENT_SHORT_TRILL, defstyle, thirty_second_ticks, {}, {0, 1, 0}, false, true, {}}, // inverted mordent
    {SymId::ORNAMENT_SHORT_TRILL, baroque, thirty_second_ticks, {1, 0, 1}, {0}, false, true, {}}, // short trill
    {SymId::ORNAMENT_TREMBLEMENT, any, thirty_second_ticks, {1, 0}, {1, 0}, false, true, {}},
    {SymId::BRASS_MUTE_CLOSED, defstyle, thirty_second_ticks, {}, {0}, false, true, {}}, // regular hand-stopped brass
    {SymId::ORNAMENT_PRALL_MORDENT, any, thirty_second_ticks, {}, {1, 0, -1, 0}, false, true, {}},
    {SymId::ORNAMENT_LINE_PRALL, any, thirty_second_ticks, {2, 2, 2}, {1, 0}, true, true, {}},
    {SymId::ORNAMENT_UP_PRALL, any, sixteenth_ticks, {-1, 0}, {1, 0}, true, true, {1, 0}},    // p 144 Ex 152 [1]
    {SymId::ORNAMENT_UP_MORDENT, any, sixteenth_ticks, {-1, 0}, {1, 0}, true, true, {-1, 0}}, // p 144 Ex 152 [1]
    {SymId::ORNAMENT_PRECOMP_MORDENT_UPPER_PREFIX,
     any,
     sixteenth_ticks,
     {1, 1, 1, 0},
     {1, 0},
     true,
     true,
     {}}, // p136 Cadence Appuyee [1] [2]
    {SymId::ORNAMENT_DOWN_MORDENT,
     any,
     sixteenth_ticks,
     {1, 1, 1, 0},
     {1, 0},
     true,
     true,
     {-1, 0}},                                                                                     // p136 Cadence Appuyee + mordent [1] [2]
    {SymId::ORNAMENT_PRALL_UP, any, sixteenth_ticks, {1, 0}, {1, 0}, true, true, {-1, 0}},         // p136 Double Cadence [1]
    {SymId::ORNAMENT_PRALL_DOWN, any, sixteenth_ticks, {1, 0}, {1, 0}, true, true, {-1, 0, 0, 0}}, // p144 ex 153 [1]
    {SymId::ORNAMENT_PRECOMP_SLIDE, any, thirty_second_ticks, {}, {0}, false, true, {}}

    // [1] Some of the articulations/ornaments in the excursions table above come from
    // Baroque Music, Style and Performance A Handbook, by Robert Donington,(c) 1982
    // ISBN 0-393-30052-8, W. W. Norton & Company, Inc.

    // [2] In some cases, the example from [1] does not preserve the timing.
    // For example, illustrates 2+1/4 counts per half note.
};

//---------------------------------------------------------
//   CompatMidiRendererInternal
///   MIDI renderer for a score
//---------------------------------------------------------

struct CompatMidiRendererInternal {

    //! @brief helper structure to find channel
    struct ChannelLookup {
        struct LookupData {
            constexpr static int invalid_string = -1;

            int32_t string = ::mu::engraving::CompatMidiRendererInternal::ChannelLookup::LookupData::invalid_string;
            staff_idx_t staff_idx = 0;
            MidiInstrumentEffect effect = MidiInstrumentEffect::NONE;
            bool harmony = false;

            bool operator<(const LookupData &other) const;
        };

        std::map<int, std::map<LookupData, int>> channels_map;
        uint32_t max_channel = 0;

        uint32_t get_channel(uint32_t instrument_channel, const LookupData &lookup_data);
    };

    enum HarmonyChannelSetting {
        DISABLED = -1, // harmony chords should not be played
        DEFAULT = 0,   // harmony chords' channels are setup in default way
        LOOKUP         // harmony chords' channels are setup in specific way
    };

    struct Context {
        struct BuiltInArticulation {
            double velocity_multiplier = 1.0;
            int gate_time = 100;
        };

        static std::unordered_map<String, BuiltInArticulation> s_built_in_articulations_values;

        bool each_string_has_channel = false;   //!to better display the guitar instrument, each string has its own channel
        bool instruments_have_effects = false;  //!when effect is applied, new channel should be used
        bool use_default_articulations = false; //!using default articulations means ignoring the ones stored for each instrument
        bool apply_caesuras = false;            //! to add pauses (caesura) between midi events
        HarmonyChannelSetting harmony_channel_setting = HarmonyChannelSetting::DEFAULT;
        std::unordered_set<std::string> parts_with_muted_harmony;

        int snd_controller = CTRL_BREATH;

        std::unordered_map<track_idx_t, VelocityMap> velocities_by_track;
        std::unordered_map<track_idx_t, VelocityMap> velocity_multiplications_by_track;
        std::unordered_map<String, std::unordered_set<String>> articulations_without_values_by_instrument;

        std::shared_ptr<ChannelLookup> channels = std::make_shared<ChannelLookup>();
        std::shared_ptr<PauseMap> pause_map = std::make_shared<PauseMap>();
    };

    explicit CompatMidiRendererInternal(Score *s);

    void render_score(EventsHolder &events, const Context &ctx, bool expand_repeats);

    static const int articulation_conv_factor{100000};
    static bool grace_notes_merged(Chord *chord);

private:

    void render_staff(EventsHolder &events, const Staff *sctx, PitchWheelRenderer &pitch_wheel_renderer);

    void render_spanners(EventsHolder &events, PitchWheelRenderer &pitch_wheel_renderer);
    void do_render_spanners(EventsHolder &events, Spanner *s, uint32_t channel, PitchWheelRenderer &pitch_wheel_renderer,
                            MidiInstrumentEffect effect);

    void collect_measure_events(EventsHolder &events, Measure const *m, const Staff *sctx, int tick_offset,
                                PitchWheelRenderer &pitch_wheel_renderer, std::array<Chord *, ::mu::engraving::voices> &prev_chords);
    void do_collect_measure_events(EventsHolder &events, Measure const *m, const Staff *sctx, int tick_offset,
                                   PitchWheelRenderer &pitch_wheel_renderer, std::array<Chord *, ::mu::engraving::voices> &prev_chords);

    struct ChordParams {
        bool let_ring = false;
        bool palm_mute = false;
        bool hammer_on_pull_off = false;
        int end_let_ring_tick = 0;
    };

    ChordParams collect_chord_params(const Chord *chord, int tick_offset) const;
    void collect_grace_before_chord_events(Chord *chord, Chord *prev_chord, EventsHolder &events, double velo_multiplier, Staff *st,
                                           int tick_offset, PitchWheelRenderer &pitch_wheel_renderer, MidiInstrumentEffect effect);
    void fill_articulations_info();

    Score *score = nullptr;
    Context context_;
};
} // namespace mu::engraving

#endif
