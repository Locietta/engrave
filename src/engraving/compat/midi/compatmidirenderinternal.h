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
#include <string>

#include "../../dom/measure.h"
#include "../../dom/mscore.h"
#include "../../types/types.h"
#include "global/btree.h"
#include "global/flatset.h"
#include "global/hashmap.h"
#include "pausemap.h"
#include "pitchwheelrenderer.h"
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
    muse::FlatSet<OrnamentStyle> ostyles;
    int duration;
    std::vector<int> prefix;
    std::vector<int> body;
    bool repeatp;
    bool sustainp;
    std::vector<int> suffix;
};

static constexpr int sixteenth_ticks = Constants::division / 4;
static constexpr int thirty_second_ticks = sixteenth_ticks / 2;
constexpr int slide_duration = thirty_second_ticks;
constexpr int grace_bend_duration_ticks = sixteenth_ticks;
extern const std::vector<OrnamentExcursion> excursions;

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

        muse::BTreeMap<int, muse::BTreeMap<LookupData, int>> channels_map;
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

        static muse::HashMap<String, BuiltInArticulation> s_built_in_articulations_values;

        bool each_string_has_channel = false;   //!to better display the guitar instrument, each string has its own channel
        bool instruments_have_effects = false;  //!when effect is applied, new channel should be used
        bool use_default_articulations = false; //!using default articulations means ignoring the ones stored for each instrument
        bool apply_caesuras = false;            //! to add pauses (caesura) between midi events
        HarmonyChannelSetting harmony_channel_setting = HarmonyChannelSetting::DEFAULT;
        muse::HashSet<std::string> parts_with_muted_harmony;

        int snd_controller = CTRL_BREATH;

        muse::HashMap<track_idx_t, VelocityMap> velocities_by_track;
        muse::HashMap<track_idx_t, VelocityMap> velocity_multiplications_by_track;
        muse::HashMap<String, muse::HashSet<String>> articulations_without_values_by_instrument;

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
