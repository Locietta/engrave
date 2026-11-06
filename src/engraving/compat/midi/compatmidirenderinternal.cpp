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

/**
 \file
 render score into event list
*/

#include "compatmidirenderinternal.h"

#include <cmath>

#include "compat/midi/event.h"
#include "compatmidirender.h"
#include "dom/accidental.h"
#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/bend.h"
#include "dom/chord.h"
#include "dom/durationtype.h"
#include "dom/dynamic.h"
#include "dom/fret.h"
#include "dom/glissando.h"
#include "dom/guitarbend.h"
#include "dom/hairpin.h"
#include "dom/harmony.h"
#include "dom/instrument.h"
#include "dom/letring.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurerepeat.h"
#include "dom/note.h"
#include "dom/noteevent.h"
#include "dom/part.h"
#include "dom/repeatlist.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "dom/stafftextbase.h"
#include "dom/swing.h"
#include "dom/tie.h"
#include "dom/trill.h"
#include "dom/utils.h"
#include "dom/vibrato.h"
#include "dom/volta.h"
#include "log.h"
#include "types/constants.h"

namespace mu::engraving {
static PitchWheelSpecs g_wheel_spec;
static constexpr int let_ring_max_ticks = Constants::division * 16;
// TODO this should be a (configurable?) constant somewhere
static constexpr Fraction articulation_change_time_max = Fraction(1, 16);
std::unordered_map<String, CompatMidiRendererInternal::Context::BuiltInArticulation>
    CompatMidiRendererInternal::Context::s_built_in_articulations_values = {
        {u"staccatissimo", {1.0, 30}}, {u"staccato", {1.0, 50}},  {u"portato", {1.0, 67}},    {u"tenuto", {1.0, 100}},
        {u"accent", {1.2, 100}},       {u"marcato", {1.44, 100}}, {u"sforzato", {1.69, 100}},
};

struct CollectNoteParams {
    double velocity_multiplier = 1.;
    int tick_offset = 0;
    int grace_offset_on = 0;
    int grace_offset_off = 0;
    int end_let_ring_tick = 0;
    int previous_chord_ticks = -1;
    bool let_ring_note = false;
    MidiInstrumentEffect effect = MidiInstrumentEffect::NONE;
    bool call_all_sound_off = false; //NoteOn silence channel
};

struct PlayNoteParams {
    int channel = 0;
    int pitch = 0;
    int velo = 0;
    int on_time = 0;
    int off_time = 0;
    int offset = 0;
    int staff_idx = 0;
    MidiInstrumentEffect effect = MidiInstrumentEffect::NONE;
    bool call_all_sound_off = false; //NoteOn silence channel
};

struct VibratoParams {
    int low_pitch = 0;
    int high_pitch = 0;
    int period = 0;
};

struct BendPlaybackInfo {
    int start_tick = 0;
    int end_tick = 0;
    float start_time_factor = 0.f;
    float end_time_factor = 1.f;
};

static uint32_t get_channel(const Instrument *instr, const Note *note, MidiInstrumentEffect effect,
                            const CompatMidiRendererInternal::Context &context);

static void fill_score_velocities(const Score *score, CompatMidiRendererInternal::Context &context);
static void fill_hairpin_velocities(const Hairpin *h, std::unordered_map<staff_idx_t, VelocityMap> &velocities_by_track);
static void fill_volta_velocities(const Volta *volta, VelocityMap &velo_map);

static double chord_velocity_multiplier(const Chord *chord, const CompatMidiRendererInternal::Context &context);
static double velocity_multiplier_by_instrument(const Instrument *instrument, const String &articulation_name,
                                                const CompatMidiRendererInternal::Context &context);
static int grace_bend_duration(const Chord *chord);

//---------------------------------------------------------
//   Converts midi time (noteoff - noteon) to milliseconds
//---------------------------------------------------------
int to_milliseconds(float tempo, float midi_time) {
    float ticks_per_second = (float) Constants::division * tempo;
    int time = (int) ((midi_time / ticks_per_second) * 1000.0f);
    if (time > 0x7fff) { //maximum possible value
        time = 0x7fff;
    }
    return time;
}

//---------------------------------------------------------
//   Detects if a note is a start of a glissando
//---------------------------------------------------------
bool is_glissando_for(const Note *note) {
    for (Spanner *spanner : note->spanner_for()) {
        if (spanner->is_glissando()) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   Detects if a note is an end of a glissando
//---------------------------------------------------------
bool is_glissando_back(const Note *note) {
    for (Spanner *spanner : note->spanner_back()) {
        if (spanner->is_glissando()) {
            return true;
        }
    }
    return false;
}

static void collect_glissando(int channel, MidiInstrumentEffect effect, int on_time, int off_time, int pitch_delta,
                              PitchWheelRenderer &pitch_wheel_renderer, staff_idx_t staff_idx) {
    const float scale = (float) g_wheel_spec.m_limit / g_wheel_spec.m_amplitude;

    PitchWheelRenderer::PitchWheelFunction func;
    func.m_start_tick = on_time;
    func.m_end_tick = off_time;

    auto linear_func = [start_tick = on_time, end_tick = off_time, pitch_delta, scale](uint32_t tick) {
        float x = (float) (tick - start_tick) / (end_tick - start_tick);
        return pitch_delta * x * scale;
    };
    func.func = linear_func;

    pitch_wheel_renderer.add_pitch_wheel_function(func, channel, staff_idx, effect);
}

static Fraction get_play_ticks_for_bend(const Note *note) {
    Tie *tie = note->tie_for();
    if (!tie || !tie->end_note()) {
        return note->chord()->actual_ticks();
    }

    Fraction stick = note->chord()->tick();
    Note *next_note = tie->end_note();
    while (tie && tie->end_note()) {
        next_note = tie->end_note();
        for (EngravingItem *e : next_note->el()) {
            if (e && (e->is_bend())) {
                return next_note->chord()->tick() - stick;
            }
        }

        tie = next_note->tie_for();
    }

    return next_note->chord()->end_tick() - stick;
}

//---------------------------------------------------------
//   playNote
//---------------------------------------------------------
static void play_note(EventsHolder &events, const Note *note, PlayNoteParams params, PitchWheelRenderer &pitch_wheel_renderer) {
    if (!note->play()) {
        return;
    }

    if (note->user_velocity() != 0) {
        params.velo = note->customize_velocity(params.velo);
    }

    if (params.call_all_sound_off && params.on_time != 0) {
        NPlayEvent ev1(ME_CONTROLLER, params.channel, CTRL_ALL_NOTES_OFF, 0);
        ev1.set_effect(params.effect);
        events[params.channel].emplace(params.on_time - 1, ev1);
    }

    NPlayEvent ev(ME_NOTEON, params.channel, params.pitch, params.velo);
    ev.set_originating_staff(params.staff_idx);
    ev.set_tuning(note->tuning());
    ev.set_note(note);
    ev.set_effect(params.effect);
    if (params.off_time > 0 && params.off_time < params.on_time) {
        return;
    }

    events[params.channel].emplace(std::max(0, params.on_time - params.offset), ev);
    Accidental *acc = note->accidental();
    if (acc) {
        AccidentalType type = acc->accidental_type();
        double cents = Accidental::subtype2cent_offset(type);
        if (!muse::real_is_null(cents)) {
            double pw_value = cents / 100.0 * (double) g_wheel_spec.m_limit / (double) g_wheel_spec.m_amplitude;
            PitchWheelRenderer::PitchWheelFunction func;
            func.m_start_tick = params.on_time - params.offset;
            func.m_end_tick = params.off_time - params.offset;
            auto microtonal_pw = [pw_value](uint32_t tick) {
                UNUSED(tick);
                return static_cast<int>(std::round(pw_value));
            };
            func.func = microtonal_pw;
            pitch_wheel_renderer.add_pitch_wheel_function(func, params.channel, params.staff_idx, MidiInstrumentEffect::NONE);
        }
    }
    // adds portamento for continuous glissando
    for (Spanner *spanner : note->spanner_for()) {
        if (spanner->is_glissando()) {
            Glissando *glissando = to_glissando(spanner);
            if (glissando->glissando_style() == GlissandoStyle::PORTAMENTO) {
                Note *next_note = to_note(spanner->end_element());
                double pitch_delta = next_note->ppitch() - params.pitch;
                int time_delta = params.off_time - params.on_time;
                if (pitch_delta != 0 && time_delta != 0) {
                    collect_glissando(params.channel, params.effect, params.on_time, params.off_time, pitch_delta, pitch_wheel_renderer,
                                      glissando->staff_idx());
                }
            }
        }
    }

    ev.set_velo(0);
    if (params.off_time != -1) {
        events[params.channel].emplace(std::max(0, params.off_time - params.offset), ev);
    }
}

static void collect_vibrato(int channel, int on_time, int off_time, const VibratoParams &vibrato_params,
                            PitchWheelRenderer &pitch_wheel_renderer, MidiInstrumentEffect effect, staff_idx_t staff_idx) {
    const uint16_t vibrato_period = vibrato_params.period;
    const uint32_t duration = off_time - on_time;
    const float scale = 2 * (float) g_wheel_spec.m_limit / g_wheel_spec.m_amplitude / 100;

    if (duration < vibrato_period) {
        return;
    }

    const int pillar_amplitude = (vibrato_params.high_pitch - vibrato_params.low_pitch);

    PitchWheelRenderer::PitchWheelFunction func;
    func.m_start_tick = on_time;
    func.m_end_tick = off_time - duration % vibrato_period; //removed last points to make more smooth of the end

    int low_pitch = vibrato_params.low_pitch;
    auto vibrato_func = [start_tick = on_time, pillar_amplitude, vibrato_period, low_pitch, scale](uint32_t tick) {
        float x = (float) (tick - start_tick) / vibrato_period;
        return (pillar_amplitude * 2 / M_PI * asin(sin(2 * M_PI * x)) + low_pitch) * scale;
    };
    func.func = vibrato_func;

    pitch_wheel_renderer.add_pitch_wheel_function(func, channel, staff_idx, effect);
}

static void add_const_pitch_wheel(int start_tick, int end_tick, float value, PitchWheelRenderer &pitch_wheel_renderer, int channel,
                                  staff_idx_t staff_idx, MidiInstrumentEffect effect) {
    const float scale = (float) g_wheel_spec.m_limit / g_wheel_spec.m_amplitude;

    PitchWheelRenderer::PitchWheelFunction pitch_wheel_const_func;
    auto const_func = [value, scale](uint32_t tick) {
        UNUSED(tick)
        return value * scale;
    };

    pitch_wheel_const_func.func = const_func;
    pitch_wheel_const_func.m_start_tick = start_tick;
    pitch_wheel_const_func.m_end_tick = end_tick;
    pitch_wheel_renderer.add_pitch_wheel_function(pitch_wheel_const_func, channel, staff_idx, effect);
}

static bool should_proceed_bend(const Note *note) {
    const GuitarBend *bend_for = note->bend_for();

    if (bend_for->bend_type() == GuitarBendType::PRE_DIVE) {
        return true;
    }

    const Note *base_note = bend_for->start_note_of_chain();

    const GuitarBend *first_bend = base_note->bend_for();
    if (first_bend && first_bend->bend_type() == GuitarBendType::PRE_BEND) {
        const Note *next_note = first_bend->end_note();
        if (next_note) {
            base_note = next_note;
        }
    }

    return base_note->last_tied_note(false) == note;
}

static BendPlaybackInfo get_bend_playback_info(const GuitarBend *bend, int bend_start, int bend_duration, bool grace_before_bend) {
    BendPlaybackInfo bend_info;

    // currently ignoring diagram for "grace before" bends
    if (!grace_before_bend) {
        bend_info.start_time_factor = bend->start_time_factor();
        bend_info.end_time_factor = bend->end_time_factor();
    }

    bend_info.start_tick = bend_start + bend_duration * bend_info.start_time_factor;
    bend_info.end_tick = bend_start + bend_duration * bend_info.end_time_factor;

    return bend_info;
}

static void fill_bend_durations(const Note *bend_start_note, const std::unordered_set<const Note *> &current_notes,
                                std::unordered_map<const Note *, int> &durations) {
    if (!bend_start_note || current_notes.empty()) {
        return;
    }

    int each_bend_duration = bend_start_note->chord()->actual_ticks().ticks() / static_cast<int>(current_notes.size());

    for (const Note *note : current_notes) {
        durations.insert({note, each_bend_duration});
    }
}

static std::unordered_map<const Note *, int> get_grace_note_bend_durations(const Note *note) {
    std::unordered_map<const Note *, int> durations;
    const Note *bend_start_note = nullptr;
    std::unordered_set<const Note *> current_notes;

    if (note->bend_for() && note->bend_for()->bend_type() == GuitarBendType::SLIGHT_BEND) {
        return {};
    }

    while (note->tie_for()) {
        const Tie *tie_for = note->tie_for();
        IF_ASSERT_FAILED(tie_for->end_note()) {
            LOGE() << "cannot find tied note for note on track " << note->track() << ", tick " << note->tick().ticks();
            return {};
        }
        note = tie_for->end_note();
    }

    while (note->bend_for() || note->tie_for_non_partial()) {
        const GuitarBend *bend_for = note->bend_for();
        if (!bend_for) {
            const Tie *tie_for = note->tie_for_non_partial();
            if (tie_for && tie_for->end_note() && tie_for->end_note() != note) {
                note = tie_for->end_note();
                continue;
            }
            break;
        }

        const Note *end_note = bend_for->end_note();
        if (!end_note || note == end_note) {
            LOGE() << "cannot find end bend note for note on track " << note->track() << ", tick " << note->tick().ticks();
            return {};
        }

        if (end_note->chord()->is_grace_after()) {
            if (current_notes.empty()) {
                IF_ASSERT_FAILED(note->chord() == end_note->chord()->ownership_parent()) {
                    LOGE() << "error in filling bends midi data for note on track " << note->track() << ", tick " << note->tick().ticks();
                    return {};
                }
                bend_start_note = note;
                current_notes.insert(bend_start_note);
            }

            if (end_note->bend_for()) {
                current_notes.insert(end_note);
            }
        } else {
            fill_bend_durations(bend_start_note, current_notes, durations);
            bend_start_note = nullptr;
            current_notes.clear();
        }

        note = bend_for->end_note();
    }

    fill_bend_durations(bend_start_note, current_notes, durations);

    return durations;
}

/*
 * All consecutive tie and bend combinations are processed in a single pass, adding pitch bends where needed to ensure continuity between notes.
 *
 * When processing the first bend in a series, the duration of any preceding ties is also included,
 * allowing for an accurate total duration calculation.
 *
 * Additional calls (for notes that have already been processed) are filtered out by the function shouldProceedBend(Note*),
 * preventing redundant processing.
*/
static void collect_guitar_bend(const Note *note, int channel, int on_time, int grace_offset, int previous_chord_ticks,
                                PitchWheelRenderer &pitch_wheel_renderer, MidiInstrumentEffect effect) {
    if (!should_proceed_bend(note)) {
        return;
    }

    const auto &grace_note_bend_durations = get_grace_note_bend_durations(note);

    int cur_pitch_bend_segment_start = on_time;

    int quarter_offset_from_start_note = 0;
    int current_quarter_tones = 0;

    if (note->bend_for()->bend_type() == GuitarBendType::GRACE_NOTE_BEND) {
        cur_pitch_bend_segment_start -= grace_offset;
    }

    const float scale = (float) g_wheel_spec.m_limit / g_wheel_spec.m_amplitude;

    while (note->bend_for() || note->tie_for()) {
        GuitarBend *bend_for = note->bend_for();
        int duration = note->chord()->actual_ticks().ticks();
        if (bend_for) {
            const Note *end_note = bend_for->end_note();
            if (!end_note) {
                break;
            }

            bool grace_before_bend = false;
            if (note->chord()->is_grace_before() && bend_for) {
                if (end_note->note_type() == NoteType::NORMAL) {
                    duration = (previous_chord_ticks == -1) ?
                                   ::mu::engraving::grace_bend_duration_ticks :
                                   std::min(previous_chord_ticks / 2, ::mu::engraving::grace_bend_duration_ticks);
                    grace_before_bend = true;
                }
            } else if (muse::contains(grace_note_bend_durations, note)) {
                duration = grace_note_bend_durations.at(note);
            }

            bend_for->compute_bend_amount();
            current_quarter_tones = bend_for->bend_amount_in_quarter_tones();

            if (bend_for->bend_type() == GuitarBendType::PRE_DIVE) {
                quarter_offset_from_start_note += current_quarter_tones;
                duration = 0;
            } else {
                BendPlaybackInfo bend_playback_info =
                    get_bend_playback_info(bend_for, cur_pitch_bend_segment_start, duration, grace_before_bend);
                double initial_pitch_bend_value = quarter_offset_from_start_note / 2.0;

                if (bend_playback_info.start_tick > cur_pitch_bend_segment_start && initial_pitch_bend_value != 0) {
                    add_const_pitch_wheel(cur_pitch_bend_segment_start, bend_playback_info.start_tick, initial_pitch_bend_value,
                                          pitch_wheel_renderer, channel, note->staff_idx(), effect);
                }

                double tick_delta = duration * (bend_playback_info.end_time_factor - bend_playback_info.start_time_factor);
                if (muse::is_zero(tick_delta)) {
                    quarter_offset_from_start_note += current_quarter_tones;
                    cur_pitch_bend_segment_start += duration;
                    note = end_note;
                    continue;
                }
                double a = current_quarter_tones / 2.0 / (tick_delta * tick_delta);
                double b = initial_pitch_bend_value;
                auto bend_func = [start_tick = bend_playback_info.start_tick, scale, a, b](uint32_t tick) {
                    float x = (float) (tick - start_tick);
                    float y = a * x * x + b;
                    return y * scale;
                };

                PitchWheelRenderer::PitchWheelFunction pitch_wheel_square_func;

                pitch_wheel_square_func.func = bend_func;

                pitch_wheel_square_func.m_start_tick = bend_playback_info.start_tick;
                pitch_wheel_square_func.m_end_tick = bend_playback_info.end_tick;

                pitch_wheel_renderer.add_pitch_wheel_function(pitch_wheel_square_func, channel, note->staff_idx(), effect);
                quarter_offset_from_start_note += current_quarter_tones;

                const int cur_pitch_bend_segment_end = cur_pitch_bend_segment_start + duration;
                if (bend_playback_info.end_tick < cur_pitch_bend_segment_end) {
                    int const_pitch_wheelduration =
                        (quarter_offset_from_start_note == 0 ? g_wheel_spec.m_step :
                                                               cur_pitch_bend_segment_end - bend_playback_info.end_tick);
                    add_const_pitch_wheel(bend_playback_info.end_tick, bend_playback_info.end_tick + const_pitch_wheelduration,
                                          quarter_offset_from_start_note / 2.0, pitch_wheel_renderer, channel, note->staff_idx(), effect);
                }
            }

            if (note == end_note) {
                break;
            }

            note = end_note;
        } else {
            if (!note->is_grace() && note->bend_back()) {
                int const_pitch_wheelduration = 0;
                int note_tick = note->tick().ticks();
                if (quarter_offset_from_start_note == 0) {
                    // reset pitchwheel once, no need to keep in for each tick
                    const_pitch_wheelduration = g_wheel_spec.m_step;
                } else {
                    Note *last_tied = note->last_tied_note(false);
                    IF_ASSERT_FAILED(last_tied) {
                        LOGE() << "couldn't find tied note for note on track " << note->track() << ", tick " << note->tick().ticks()
                               << ", guitar bend midi may be incorrect";
                        const_pitch_wheelduration = note->chord()->actual_ticks().ticks();
                    }
                    else {
                        Chord *last_chord = last_tied->chord();
                        // keep the last pitchwheel value for the total duration of tied notes
                        const_pitch_wheelduration =
                            last_chord->tick().ticks() - note_tick + (last_tied->bend_for() ? 0 : last_chord->actual_ticks().ticks());
                    }
                }

                add_const_pitch_wheel(note_tick, note_tick + const_pitch_wheelduration, quarter_offset_from_start_note / 2.0,
                                      pitch_wheel_renderer, channel, note->staff_idx(), effect);
            }

            const Tie *tie = note->tie_for();
            note = tie->end_note();
            if (!note) {
                break;
            }
        }

        cur_pitch_bend_segment_start += duration;
    }

    // adding pitch wheel to last note of bend/tie chain, if it's end of bend
    if (!note->is_grace() && note->bend_back()) {
        int const_pitch_wheelduration = (quarter_offset_from_start_note == 0) ? g_wheel_spec.m_step : note->chord()->actual_ticks().ticks();
        add_const_pitch_wheel(note->tick().ticks(), note->tick().ticks() + const_pitch_wheelduration, quarter_offset_from_start_note / 2.0,
                              pitch_wheel_renderer, channel, note->staff_idx(), effect);
    }
}

static void collect_bend(const PitchValues &play_data, staff_idx_t staff_idx, int channel, int on_time, int off_time,
                         PitchWheelRenderer &pitch_wheel_renderer, MidiInstrumentEffect effect) {
    size_t pitch_size = play_data.size();

    const float scale = 2 * (float) g_wheel_spec.m_limit / g_wheel_spec.m_amplitude / PitchValue::pitch_for_semitone;
    uint32_t duration = off_time - on_time;

    for (size_t i = 0; i < pitch_size - 1; i++) {
        PitchValue cur_value = play_data.at(i);
        PitchValue next_value = play_data.at(i + 1);

        //! y = a x^2 + b - curve
        float cur_tick = (float) cur_value.time * duration / PitchValue::max_time;
        float next_tick = (float) next_value.time * duration / PitchValue::max_time;

        float a = (float) (next_value.pitch - cur_value.pitch) / ((cur_tick - next_tick) * (cur_tick - next_tick));
        float b = cur_value.pitch;

        uint32_t x0 = cur_value.time * duration / PitchValue::max_time;

        PitchWheelRenderer::PitchWheelFunction func;
        func.m_start_tick = on_time + x0;
        uint32_t start_time_next_point = next_value.time * duration / PitchValue::max_time;
        func.m_end_tick = on_time + start_time_next_point;

        auto bend_func = [start_tick = func.m_start_tick, scale, a, b](uint32_t tick) {
            float x = (float) (tick - start_tick);

            float y = a * x * x + b;

            return y * scale;
        };
        func.func = bend_func;
        pitch_wheel_renderer.add_pitch_wheel_function(func, channel, staff_idx, effect);
    }
    PitchWheelRenderer::PitchWheelFunction func;
    func.m_start_tick = on_time + play_data.at(pitch_size - 1).time * duration / PitchValue::max_time;
    func.m_end_tick = off_time;

    if (func.m_end_tick == func.m_start_tick) {
        return;
    }

    //! y = releaseValue linear curve
    uint32_t release_value = play_data.at(pitch_size - 1).pitch * scale;
    auto bend_func = [release_value](uint32_t tick) {
        UNUSED(tick)
        return release_value;
    };
    func.func = bend_func;
    pitch_wheel_renderer.add_pitch_wheel_function(func, channel, staff_idx, effect);
}

static bool let_ring_should_apply(const NoteEvent &event, const Note *note) {
    if (note->has_slide_from_note()) {
        return false;
    }

    if (is_glissando_back(note)) {
        return true;
    }

    if (event.slide() || is_glissando_for(note)) {
        return false;
    }

    return true;
}

static void render_snd(EventsHolder &events, const Chord *chord, int note_channel, int tick_offset,
                       const CompatMidiRendererInternal::Context &context) {
    Fraction stick = chord->tick();
    Fraction etick = stick + chord->ticks();
    const VelocityMap &velo_events = context.velocities_by_track.at(chord->track());
    const VelocityMap &mult_events = context.velocity_multiplications_by_track.at(chord->track());
    auto changes = velo_events.changes_in_range(stick, etick);
    auto mult_changes = mult_events.changes_in_range(stick, etick);

    std::map<int, int> velocity_map;
    for (auto &change : changes) {
        int last_val = -1;
        int end_point = change.second.ticks();
        for (int t = change.first.ticks(); t <= end_point; t++) {
            int velo = velo_events.val(Fraction::from_ticks(t));
            if (velo == last_val) {
                continue;
            }
            last_val = velo;

            velocity_map[t] = velo;
        }
    }

    double conversion_factor = CompatMidiRendererInternal::articulation_conv_factor;
    for (auto &change : mult_changes) {
        // Ignore fix events: they are available as cached ramp starts
        // and considering them ends up with multiplying twice effectively
        if (change.first == change.second) {
            continue;
        }

        int last_val = CompatMidiRendererInternal::articulation_conv_factor;
        int end_point = change.second.ticks();
        int last_velocity = 0;
        auto last_valocity_it = velocity_map.upper_bound(change.first.ticks());
        if (last_valocity_it != velocity_map.end()) {
            last_velocity = last_valocity_it->second;
        } else if (!velocity_map.empty()) {
            last_velocity = velocity_map.cbegin()->second;
        }

        for (int t = change.first.ticks(); t <= end_point; t++) {
            int mult = mult_events.val(Fraction::from_ticks(t));
            if (mult == last_val || mult == conversion_factor) {
                continue;
            }
            last_val = mult;

            double real_mult = mult / conversion_factor;
            if (velocity_map.find(t) != velocity_map.end()) {
                last_velocity = velocity_map[t];
                velocity_map[t] *= real_mult;
            } else {
                velocity_map[t] = last_velocity * real_mult;
            }
        }
    }

    for (auto point = velocity_map.cbegin(); point != velocity_map.cend(); ++point) {
        // NOTE:JT if we ever want to use poly aftertouch instead of CC, this is where we want to
        // be using it. Instead of ME_CONTROLLER, use ME_POLYAFTER (but duplicate for each note in chord)
        NPlayEvent event = NPlayEvent(ME_CONTROLLER, note_channel, context.snd_controller, std::clamp(point->second, 0, 127));
        event.set_originating_staff(chord->staff_idx());
        events[note_channel].insert(std::make_pair(point->first + tick_offset, event));
    }
}

int grace_bend_duration(const Chord *chord) {
    int grace_duration = ::mu::engraving::grace_bend_duration_ticks;
    if (chord) {
        grace_duration = std::min(chord->ticks().ticks() / 2, grace_duration);
    }

    return grace_duration;
}

static int calculate_tie_length(const Note *note) {
    int tie_len = 0;

    const Note *n = note;
    while (n) {
        // Process ties or bends
        const Tie *tie_for = n->tie_for_non_partial();
        const GuitarBend *bend_for = n->bend_for();

        if (tie_for && tie_for->end_note() != n) {
            n = tie_for->end_note();
        } else if (bend_for && bend_for->end_note() != n) {
            bool is_pre_dive = bend_for->bend_type() == GuitarBendType::PRE_DIVE;
            n = bend_for->end_note();
            if (is_pre_dive) {
                continue; // PRE_DIVE ghost already has parent chord's ticks
            }
        } else {
            break;
        }

        IF_ASSERT_FAILED(n) { break; }

        const NoteEventList &nel = n->play_events();

        if (!nel.empty() && (!n->chord()->is_grace())) {
            tie_len += nel[0].len() * n->chord()->actual_ticks().ticks() / NoteEvent::note_length;
        }
    }

    return tie_len;
}

//---------------------------------------------------------
//   collectNote
//---------------------------------------------------------

static void collect_note(EventsHolder &events, const Note *note, const CollectNoteParams &note_params, Staff *staff,
                         PitchWheelRenderer &pitch_wheel_renderer, const CompatMidiRendererInternal::Context &context) {
    if (!note->play() || note->hidden()) { // do not play overlapping notes
        return;
    }

    Chord *chord = note->chord();
    const Instrument *instr = chord->part()->instrument(chord->tick());
    MidiInstrumentEffect note_effect = note_params.effect;

    int note_channel = get_channel(instr, note, note_effect, context);
    auto midi_effect_from_event = [](const NoteEvent &event) {
        if (event.slide()) {
            return MidiInstrumentEffect::SLIDE;
        }

        return MidiInstrumentEffect::NONE;
    };

    int tie_len = calculate_tie_length(note);
    if (chord->is_grace()) {
        assert(!CompatMidiRendererInternal::grace_notes_merged(
            chord)); // this function should not be called on a grace note if grace notes are merged
        chord = to_chord(chord->ownership_parent());
    }

    int ticks = chord->actual_ticks().ticks(); // ticks of the actual note
    // calculate additional length due to ties forward
    // taking NoteEvent length adjustments into account

    int tick1 = note->tick().ticks() + note_params.tick_offset;
    const GuitarBend *bend_for = note->bend_for();
    const GuitarBend *bend_back = note->bend_back();

    NoteEventList nel = note->play_events();
    size_t nels = nel.size();
    for (size_t i = 0; i < nels; ++i) {
        const NoteEvent &e =
            nel[i]; // we make an explicit const ref, not a const copy.  no need to copy as we won't change the original object.

        // skip if note has a tie into it and only one NoteEvent
        // its length was already added to previous note
        // if we wish to suppress first note of ornament
        // then change "nels == 1" to "i == 0", and change "break" to "continue"
        if (note->tie_back() && nels == 1 && !is_glissando_for(note)) {
            Note *tied_back = note->tie_back()->start_note();
            if (!tied_back) {
                break;
            }

            const auto &events_list = tied_back->play_events();
            IF_ASSERT_FAILED(!events_list.empty()) {
                LOGE() << "play events are empty for note on track " << tied_back->track() << ", tick " << tied_back->tick().ticks();
                break;
            }

            if (note_effect == MidiInstrumentEffect::NONE) {
                note_effect = midi_effect_from_event(events_list.front());
                note_channel = get_channel(instr, note, note_effect, context);
            }

            break;
        }

        // skipping the notes which are connected by bends
        if (bend_back && bend_back->bend_type() != GuitarBendType::PRE_BEND && i == 0) {
            continue;
        }

        int p = std::clamp(note->ppitch() + e.pitch() + note->harmonic_pitch_offset(), 0, 127);
        int on = tick1 + (ticks * e.ontime()) / 1000;
        int off = on + (ticks * e.len()) / 1000 - 1;

        if (note->dead_note()) {
            const double ticks_per_second = chord->score()->multiplied_tempo(chord->tick()).val * Constants::division;
            constexpr double dead_note_duration_in_sec = 0.05;
            const double dead_note_duration_in_ticks = ticks_per_second * dead_note_duration_in_sec;
            if (off - on > dead_note_duration_in_ticks) {
                off = on + dead_note_duration_in_ticks;
            }
        } else {
            if ((note->tie_for() || bend_for) && i == nels - 1) {
                off += tie_len;
            }

            if (note_params.let_ring_note && let_ring_should_apply(e, note)) {
                off = std::max(off, note_params.end_let_ring_tick);
                if (off - on > ::mu::engraving::let_ring_max_ticks) {
                    off = on + ::mu::engraving::let_ring_max_ticks;
                }
            }
        }

        // Get the velocity used for this note from the staff
        // This allows correct playback of tremolos even without SND enabled.
        Fraction non_unwound_tick = Fraction::from_ticks(on - note_params.tick_offset);
        int velo =
            context.velocities_by_track.at(note->track()).val(non_unwound_tick) * note_params.velocity_multiplier * e.velocity_multiplier();
        if (e.play()) {
            PlayNoteParams play_params;
            MidiInstrumentEffect event_effect = note_effect;
            int event_channel = note_channel;
            play_params.pitch = p;
            play_params.velo = std::clamp(velo, 1, 127);
            play_params.on_time = std::max(0, on - note_params.grace_offset_on);
            play_params.off_time = std::max(0, off - note_params.grace_offset_off);

            if (event_effect == MidiInstrumentEffect::NONE) {
                event_effect = midi_effect_from_event(e);
                event_channel = get_channel(instr, note, event_effect, context);
            }

            play_params.effect = event_effect;
            play_params.channel = event_channel;

            if (note_params.grace_offset_on == 0) {
                play_params.offset = ticks * e.offset() / 1000;
            }

            play_params.staff_idx = static_cast<int>(staff->idx());
            play_params.call_all_sound_off = note_params.call_all_sound_off;
            play_note(events, note, play_params, pitch_wheel_renderer);

            if (instr->single_note_dynamics()) {
                render_snd(events, chord, note_channel, note_params.tick_offset, context);
            }
        }
    }

    // Bends
    if (bend_for) {
        collect_guitar_bend(note, note_channel, tick1, note_params.grace_offset_on, note_params.previous_chord_ticks, pitch_wheel_renderer,
                            note_effect);
    } else {
        // old bends implementation
        for (const EngravingItem *e : note->el()) {
            if (!e || (!e->is_bend())) {
                continue;
            }

            const Bend *bend = to_bend(e);
            if (!bend->play_bend()) {
                break;
            }

            collect_bend(bend->points(), bend->staff_idx(), note_channel, tick1, tick1 + get_play_ticks_for_bend(note).ticks(),
                         pitch_wheel_renderer, note_effect);
        }
    }
}

//---------------------------------------------------------
//   aeolusSetStop
//---------------------------------------------------------

static void aeolus_set_stop(int tick, int channel, int i, int k, bool val, EventsHolder &events) {
    NPlayEvent event;
    event.set_type(ME_CONTROLLER);
    event.set_controller(98);
    if (val) {
        event.set_value(0x40 + 0x20 + i);
    } else {
        event.set_value(0x40 + 0x10 + i);
    }

    event.set_channel(static_cast<uint8_t>(channel));
    events[channel].emplace(tick, event);

    event.set_value(k);
    events[channel].emplace(tick, event);
}

//---------------------------------------------------------
//   collectProgramChanges
//---------------------------------------------------------

static void collect_program_changes(EventsHolder &events, Measure const *m, const Staff *staff, int tick_offset) {
    int first_staff_idx = static_cast<int>(staff->idx());
    int next_staff_idx = first_staff_idx + 1;

    //
    // collect program changes and controller
    //
    for (Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
        for (EngravingItem *e : s->annotations()) {
            if (!e->is_staff_text_base() || static_cast<int>(e->staff_idx()) < first_staff_idx ||
                static_cast<int>(e->staff_idx()) >= next_staff_idx) {
                continue;
            }
            const StaffTextBase *st1 = to_staff_text_base(e);
            Fraction tick = s->tick() + Fraction::from_ticks(tick_offset);

            Instrument *instr = e->part()->instrument(tick);
            for (const ChannelActions &ca : st1->channel_actions()) {
                int channel = instr->channel().at(ca.channel)->channel();
                for (const String &ma : ca.midi_action_names) {
                    NamedEventList *nel = instr->midi_action(ma, ca.channel);
                    if (!nel) {
                        continue;
                    }
                    for (MidiCoreEvent event : nel->events) {
                        event.set_channel(channel);
                        NPlayEvent e1(event);
                        e1.set_originating_staff(first_staff_idx);
                        if (e1.data_a() == CTRL_PROGRAM) {
                            events[channel].emplace(tick.ticks() - 1, e1);
                        } else {
                            events[channel].emplace(tick.ticks(), e1);
                        }
                    }
                }
            }
            if (st1->set_aeolus_stops()) {
                Staff *s1 = st1->staff();
                int voice = 0;
                int channel = s1->channel(tick, voice);

                for (int i = 0; i < 4; ++i) {
                    static int num[4] = {12, 13, 16, 16};
                    for (int k = 0; k < num[i]; ++k) {
                        aeolus_set_stop(tick.ticks(), channel, i, k, st1->get_aeolus_stop(i, k), events);
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------
//    renderHarmony
///    renders chord symbols
//---------------------------------------------------------
static void render_harmony(EventsHolder &events, Measure const *m, Harmony *h, int tick_offset,
                           const CompatMidiRendererInternal::Context &context) {
    if (!h->is_realizable() || context.harmony_channel_setting == CompatMidiRendererInternal::HarmonyChannelSetting::DISABLED) {
        return;
    }

    if (context.parts_with_muted_harmony.find(h->part()->id().to_std_string()) != context.parts_with_muted_harmony.end()) {
        return;
    }

    Staff *staff = m->score()->staff(h->track() / ::mu::engraving::voices);
    const InstrChannel *instr_channel = staff->part()->harmony_channel();
    IF_ASSERT_FAILED(instr_channel) {
        LOGE() << "channel for harmony isn't found for part " << staff->part()->part_name();
        return;
    }

    if (!staff->is_primary_staff()) {
        return;
    }

    int channel = instr_channel->channel();
    int staff_idx = static_cast<int>(staff->idx());

    if (context.harmony_channel_setting == CompatMidiRendererInternal::HarmonyChannelSetting::LOOKUP) {
        CompatMidiRendererInternal::ChannelLookup::LookupData lookup_data;
        lookup_data.harmony = true;
        channel = context.channels->get_channel(channel, lookup_data);
    }

    int velocity = context.velocities_by_track.at(h->track()).val(h->tick());

    const RealizedHarmony &r = h->get_realized_harmony();

    NPlayEvent ev(ME_NOTEON, static_cast<uint8_t>(channel), 0, velocity);
    ev.set_harmony(h);
    Fraction duration = r.get_actual_duration(h->tick().ticks() + tick_offset);

    int on_time = h->tick().ticks() + tick_offset;
    int off_time = on_time + duration.ticks();

    ev.set_originating_staff(staff_idx);
    ev.set_tuning(0.0);

    //add play events
    const RealizedHarmony::PitchMap &notes = r.notes();
    for (const auto &[pitch, _] : notes) {
        ev.set_pitch(pitch);
        ev.set_velo(velocity);
        events[channel].emplace(on_time, ev);
        ev.set_velo(0);
        events[channel].emplace(off_time, ev);
    }
}

void CompatMidiRendererInternal::collect_grace_before_chord_events(Chord *chord, Chord *prev_chord, EventsHolder &events,
                                                                   double velo_multiplier, Staff *st, int tick_offset,
                                                                   PitchWheelRenderer &pitch_wheel_renderer, MidiInstrumentEffect effect) {
    // calculate offset for grace notes here
    const std::vector<Chord *> gr_chords = chord->grace_notes_before(); // create copy
    std::vector<Chord *> grace_notes_before_bar;
    std::copy_if(gr_chords.begin(), gr_chords.end(), std::back_inserter(grace_notes_before_bar),
                 [](Chord *ch) { return ch->note_type() == NoteType::ACCIACCATURA; });

    int grace_tick_sum = 0;
    int grace_tick_offset = 0;

    size_t acciacatura_grace_size = grace_notes_before_bar.size();
    if (acciacatura_grace_size > 0) {
        grace_tick_sum = grace_notes_before_bar[0]->ticks().ticks();
        if (prev_chord) {
            grace_tick_sum = std::min(prev_chord->ticks().ticks() / 2, grace_tick_sum);
        }

        grace_tick_offset = grace_tick_sum / static_cast<int>(acciacatura_grace_size);
    } else {
        bool has_grace_bend = std::any_of(gr_chords.begin(), gr_chords.end(), [](Chord *ch) {
            return std::any_of(ch->notes().begin(), ch->notes().end(), [](Note *n) { return n->is_grace_bend_start(); });
        });

        if (has_grace_bend) {
            grace_tick_sum = grace_bend_duration(prev_chord);
        }
    }

    if (!grace_notes_merged(chord)) {
        int current_beafore_beat_note = 0;
        for (Chord *c : gr_chords) {
            for (const Note *note : c->notes()) {
                GuitarBend *bend_for = note->bend_for();
                if (bend_for && bend_for->bend_type() == GuitarBendType::PRE_BEND) {
                    continue;
                }

                CollectNoteParams params;
                params.effect = effect;
                params.velocity_multiplier = velo_multiplier;
                params.tick_offset = tick_offset;

                bool is_grace_bend = (note->bend_for() && note->bend_for()->bend_type() == GuitarBendType::GRACE_NOTE_BEND);
                if (prev_chord) {
                    params.previous_chord_ticks = prev_chord->actual_ticks().ticks();
                }

                if (note->note_type() == NoteType::ACCIACCATURA) {
                    params.grace_offset_on = grace_tick_sum - grace_tick_offset * current_beafore_beat_note;
                    params.grace_offset_off = grace_tick_sum - grace_tick_offset * (current_beafore_beat_note + 1);

                    collect_note(events, note, params, st, pitch_wheel_renderer, context_);
                } else if (is_grace_bend) {
                    params.grace_offset_on = grace_tick_sum;
                    params.grace_offset_off = 0;

                    collect_note(events, note, params, st, pitch_wheel_renderer, context_);
                } else {
                    collect_note(events, note, params, st, pitch_wheel_renderer, context_);
                }
            }

            current_beafore_beat_note++;
        }
    }
}

bool should_play_hammer_on(const Chord *chord) {
    int current_tick = chord->tick().ticks();
    Chord *first_tied_chord = chord->next_tied_chord(true, false);
    const Score *score = chord->score();
    while (first_tied_chord) {
        current_tick = first_tied_chord->tick().ticks();
        first_tied_chord = first_tied_chord->next_tied_chord(true, false);
    }

    for (auto it : score->spanner_map().find_overlapping(current_tick, current_tick + chord->ticks().ticks())) {
        Spanner *spanner = it.value;
        if (spanner->track() != chord->track()) {
            continue;
        }

        if (spanner->is_hammer_on_pull_off() && (spanner->start_chord() != chord)) {
            return true;
        }
    }

    return false;
}

CompatMidiRendererInternal::ChordParams CompatMidiRendererInternal::collect_chord_params(const Chord *chord, int tick_offset) const {
    ChordParams chord_params;

    int current_tick = chord->tick().ticks();
    for (auto it : score->spanner_map().find_overlapping(current_tick + 1, current_tick + 2)) {
        Spanner *spanner = it.value;
        if (spanner->track() != chord->track()) {
            continue;
        }

        if (spanner->is_let_ring()) {
            LetRing *let_ring = to_let_ring(spanner);
            chord_params.let_ring = true;
            ChordRest *end_cr = let_ring->end_cr();
            chord_params.end_let_ring_tick = (end_cr ? end_cr->end_tick().ticks() : let_ring->tick2().ticks()) + tick_offset;
        } else if (spanner->is_palm_mute()) {
            chord_params.palm_mute = true;
        }
    }

    chord_params.hammer_on_pull_off = should_play_hammer_on(chord);
    return chord_params;
}

//---------------------------------------------------------
//   doCollectMeasureEvents
//---------------------------------------------------------

void CompatMidiRendererInternal::do_collect_measure_events(EventsHolder &events, Measure const *m, const Staff *staff, int tick_offset,
                                                           PitchWheelRenderer &pitch_wheel_renderer,
                                                           std::array<Chord *, ::mu::engraving::voices> &prev_chords) {
    staff_idx_t first_staff_idx = staff->idx();
    for (Staff *st : staff->master_score()->staves()) {
        if (staff->id() == st->id()) {
            first_staff_idx = st->idx();
        }
    }
    staff_idx_t next_staff_idx = first_staff_idx + 1;

    SegmentType st = SegmentType::CHORD_REST;
    track_idx_t strack = first_staff_idx * ::mu::engraving::voices;
    track_idx_t etrack = next_staff_idx * ::mu::engraving::voices;
    for (Segment *seg = m->first(st); seg; seg = seg->next(st)) {
        Fraction tick = seg->tick();

        //render harmony
        for (EngravingItem *e : seg->annotations()) {
            if (!e || (e->track() < strack) || (e->track() >= etrack)) {
                continue;
            }
            Harmony *h = nullptr;
            if (e->is_harmony()) {
                h = to_harmony(e);
            } else if (e->is_fret_diagram()) {
                h = to_fret_diagram(e)->harmony();
            }
            if (!h || !h->play()) {
                continue;
            }
            render_harmony(events, m, h, tick_offset, context_);
        }

        for (track_idx_t track = strack; track < etrack; ++track) {
            // Skip linked staves, except primary
            Staff *st1 = m->score()->staff(track / ::mu::engraving::voices);
            if (!st1->is_primary_staff()) {
                track += ::mu::engraving::voices - 1;
                continue;
            }

            size_t voice = track % ::mu::engraving::voices;
            EngravingItem *cr = seg->element(track);
            if (!cr || !cr->is_chord()) {
                prev_chords[voice] = nullptr;
                continue;
            }

            Chord *chord = to_chord(cr);
            double velo_multiplier = NoteEvent::default_velocity_multiplier * chord_velocity_multiplier(chord, context_);

            //
            // Add normal note events
            //
            ChordParams chord_params = collect_chord_params(chord, tick_offset);

            MidiInstrumentEffect effect = MidiInstrumentEffect::NONE;
            if (chord_params.palm_mute) {
                effect = MidiInstrumentEffect::PALM_MUTE;
            } else if (chord_params.hammer_on_pull_off) {
                effect = MidiInstrumentEffect::HAMMER_PULL;
            }

            collect_grace_before_chord_events(chord, prev_chords[voice], events, velo_multiplier, st1, tick_offset, pitch_wheel_renderer,
                                              effect);

            Instrument *instr = st1->part()->instrument(tick);
            for (const Note *note : chord->notes()) {
                CollectNoteParams params;
                params.velocity_multiplier = velo_multiplier;
                params.tick_offset = tick_offset;
                params.let_ring_note = chord_params.let_ring;
                params.end_let_ring_tick = chord_params.end_let_ring_tick;
                if (context_.instruments_have_effects) {
                    params.effect = effect;
                }

                if (context_.each_string_has_channel && instr->has_strings()) {
                    params.call_all_sound_off = true;
                }

                collect_note(events, note, params, st1, pitch_wheel_renderer, context_);
            }

            if (!grace_notes_merged(chord)) {
                const std::vector<Chord *> grace_after = chord->grace_notes_after(); // create copy
                for (Chord *c : grace_after) {
                    for (const Note *note : c->notes()) {
                        CollectNoteParams params;
                        params.velocity_multiplier = velo_multiplier;
                        params.tick_offset = tick_offset;
                        params.effect = effect;
                        collect_note(events, note, params, st1, pitch_wheel_renderer, context_);
                    }
                }
            }

            prev_chords[voice] = chord;
        }
    }
}

CompatMidiRendererInternal::CompatMidiRendererInternal(Score *s) : score(s) {}

//---------------------------------------------------------
//   collectMeasureEvents
//    redirects to the correct function based on the passed method
//---------------------------------------------------------

void CompatMidiRendererInternal::collect_measure_events(EventsHolder &events, Measure const *m, const Staff *staff, int tick_offset,
                                                        PitchWheelRenderer &pitch_wheel_renderer,
                                                        std::array<Chord *, ::mu::engraving::voices> &prev_chords) {
    do_collect_measure_events(events, m, staff, tick_offset, pitch_wheel_renderer, prev_chords);

    collect_program_changes(events, m, staff, tick_offset);
}

//---------------------------------------------------------
//   renderStaff
//---------------------------------------------------------

void CompatMidiRendererInternal::render_staff(EventsHolder &events, const Staff *staff, PitchWheelRenderer &pitch_wheel_renderer) {
    Measure const *last_measure = nullptr;

    const RepeatList &repeat_list = score->repeat_list();
    std::array<Chord *, ::mu::engraving::voices> prev_chords = {nullptr};

    for (const RepeatSegment *rs : repeat_list) {
        const int tick_offset = rs->utick - rs->tick;

        Measure const *const start = rs->first_measure();

        for (Measure const *m = start; m; m = m->next_measure()) {
            staff_idx_t staff_idx = staff->idx();
            if (m->is_measure_repeat_group(staff_idx)) {
                MeasureRepeat *mr = m->measure_repeat_element(staff_idx);
                Measure const *play_measure = last_measure;
                if (!play_measure || !mr) {
                    continue;
                }

                for (int i = m->measure_repeat_count(staff_idx); i < mr->num_measures() && play_measure->prev_measure(); ++i) {
                    play_measure = play_measure->prev_measure();
                }

                int offset = (m->tick() - play_measure->tick()).ticks();
                collect_measure_events(events, play_measure, staff, tick_offset + offset, pitch_wheel_renderer, prev_chords);
            } else {
                last_measure = m;
                collect_measure_events(events, last_measure, staff, tick_offset, pitch_wheel_renderer, prev_chords);
            }

            if (m == rs->last_measure()) {
                break;
            }
        }
    }
}

//---------------------------------------------------------
//   renderSpanners
//---------------------------------------------------------

void CompatMidiRendererInternal::render_spanners(EventsHolder &events, PitchWheelRenderer &pitch_wheel_renderer) {
    for (const auto &sp : score->spanner_map().map()) {
        Spanner *s = sp.second;

        if (!s->staff()->is_primary_staff()) {
            continue;
        }

        int idx = s->staff()->channel(s->tick(), 0);
        int channel = s->part()->instrument(s->tick())->channel(idx)->channel();
        const auto &channels = context_.channels->channels_map[channel];
        if (channels.empty()) {
            do_render_spanners(events, s, channel, pitch_wheel_renderer, MidiInstrumentEffect::NONE);
        } else {
            for (const auto &channel2 : channels) {
                do_render_spanners(events, s, channel2.second, pitch_wheel_renderer, channel2.first.effect);
            }
        }
    }
}

static std::vector<std::pair<int, int>> collect_ticks_for_effect(const Score *const score, track_idx_t track, int stick, int etick,
                                                                 MidiInstrumentEffect effect) {
    std::vector<std::pair<int, int>> ticks_for_effect;
    int cur_tick = stick;

    for (auto it : score->spanner_map().find_overlapping(stick, etick)) {
        Spanner *spanner = it.value;

        if (spanner->track() != track) {
            continue;
        }

        if (spanner->is_palm_mute()) {
            int palm_mute_start_tick = spanner->tick().ticks();
            int palm_mute_end_tick = spanner->tick2().ticks();

            if (cur_tick < palm_mute_start_tick) {
                int next_tick = std::min(palm_mute_start_tick, etick);
                if (effect == MidiInstrumentEffect::NONE) {
                    ticks_for_effect.push_back({cur_tick, next_tick - 1});
                }

                cur_tick = next_tick;
            }

            if (palm_mute_start_tick <= cur_tick) {
                int next_tick = std::min(palm_mute_end_tick, etick);
                if (effect == MidiInstrumentEffect::PALM_MUTE) {
                    ticks_for_effect.push_back({cur_tick, next_tick - 1});
                }

                cur_tick = next_tick;
            }
        }
    }

    if (cur_tick < etick && effect == MidiInstrumentEffect::NONE) {
        ticks_for_effect.push_back({cur_tick, etick - 1});
    }

    return ticks_for_effect;
}

static VibratoParams get_vibrato_params(VibratoType type) {
    VibratoParams params;

    switch (type) {
        case VibratoType::GUITAR_VIBRATO:
            // guitar vibrato, up only
            params.low_pitch = 0;
            params.high_pitch = 10;
            params.period = Constants::division / 3;
            break;

        case VibratoType::GUITAR_VIBRATO_WIDE:
            params.low_pitch = 0; // 100 is a semitone
            params.high_pitch = 20;
            params.period = Constants::division / 2.5;
            break;

        case VibratoType::VIBRATO_SAWTOOTH_WIDE:
            // vibrato with whammy bar up and down
            params.low_pitch = -25; // 1/16
            params.high_pitch = 25;
            params.period = Constants::division / 2;
            break;

        case VibratoType::VIBRATO_SAWTOOTH:
            params.low_pitch = -12;
            params.high_pitch = 12;
            params.period = Constants::division / 2;
            break;

        default: LOGE() << "vibrato type is not handled in midi renderer"; break;
    }

    return params;
}

void CompatMidiRendererInternal::do_render_spanners(EventsHolder &events, Spanner *s, uint32_t channel,
                                                    PitchWheelRenderer &pitch_wheel_renderer, MidiInstrumentEffect effect) {
    struct PedalEvent {
        int tick = 0;
        bool on = true;
        int staff_idx = 0;

        PedalEvent() = default;

        PedalEvent(int tick, bool on, int staff_idx) : tick(tick), on(on), staff_idx(staff_idx) {}
    };

    std::vector<PedalEvent> pedal_event_list;

    int staff_idx = static_cast<int>(s->staff_idx());

    if (s->is_pedal()) {
        PedalEvent last_event;

        if (!pedal_event_list.empty()) {
            last_event = pedal_event_list.back();
        } else {
            last_event = {0, true, staff_idx};
        }

        int st = s->tick().ticks();

        if (!last_event.on && last_event.tick >= (st + 2)) {
            pedal_event_list.emplace(pedal_event_list.cend() - 1, st + (2 - MScore::pedal_events_min_ticks), false, staff_idx);
        }
        int a = st + 2;
        pedal_event_list.emplace_back(a, true, staff_idx);

        int t = s->tick2().ticks() + (2 - MScore::pedal_events_min_ticks);
        if (!score->repeat_list().empty()) {
            const RepeatSegment &last_repeat = *score->repeat_list().back();
            if (t > last_repeat.utick + last_repeat.len()) {
                t = last_repeat.utick + last_repeat.len();
            }
        }
        pedal_event_list.emplace_back(t, false, staff_idx);
    } else if (s->is_vibrato()) {
        int stick = s->tick().ticks();
        int etick = s->tick2().ticks();

        // from start to end of trill, send bend events at regular interval
        Vibrato *t = to_vibrato(s);
        VibratoParams vibrato_params = get_vibrato_params(t->vibrato_type());

        std::vector<std::pair<int, int>> vibrato_ticks_for_effect = collect_ticks_for_effect(score, s->track(), stick, etick, effect);

        for (const auto &[tick_start, tick_end] : vibrato_ticks_for_effect) {
            collect_vibrato(channel, tick_start, tick_end, vibrato_params, pitch_wheel_renderer, effect, s->staff_idx());
        }
    }

    for (const auto &pe : pedal_event_list) {
        NPlayEvent event;
        if (pe.on) {
            event = NPlayEvent(ME_CONTROLLER, static_cast<uint8_t>(channel), CTRL_SUSTAIN, 127);
        } else {
            event = NPlayEvent(ME_CONTROLLER, static_cast<uint8_t>(channel), CTRL_SUSTAIN, 0);
        }
        event.set_originating_staff(pe.staff_idx);
        event.set_effect(effect);
        events[channel].emplace(pe.tick, event);
    }
}

//---------------------------------------------------------
// findFirstTrill
//  search the spanners in the score, finding the first one
//  which overlaps this chord and is of type ElementType::TRILL
//---------------------------------------------------------

static Trill *find_first_trill(Chord *chord) {
    auto spanners = chord->score()->spanner_map().find_overlapping(1 + chord->tick().ticks(), chord->end_tick().ticks() - 1);
    for (auto i : spanners) {
        if (!i.value->is_trill()) {
            continue;
        }
        if (i.value->track() != chord->track()) {
            continue;
        }
        Trill *trill = to_trill(i.value);
        if (!trill->play_spanner()) {
            continue;
        }
        return trill;
    }
    return nullptr;
}

void CompatMidiRendererInternal::render_score(EventsHolder &events, const Context &context, bool expand_repeats) {
    UNUSED(expand_repeats);

    context_ = context;
    PitchWheelRenderer pitch_wheel_render(g_wheel_spec);

    score->update_swing();
    score->update_capo();

    if (!context_.use_default_articulations) {
        fill_articulations_info();
    }

    CompatMidiRender::create_play_events(score, score->first_measure(), nullptr, context_);

    score->update_channel();
    fill_score_velocities(score, context_);

    // create note & other events
    for (const Staff *st : score->staves()) {
        render_staff(events, st, pitch_wheel_render);
    }
    events.fixup_midi();

    // create sustain pedal events
    render_spanners(events, pitch_wheel_render);

    EventsHolder pitch_wheel_events = pitch_wheel_render.render_pitch_wheel();
    events.merge_pitch_wheel_events(pitch_wheel_events);
    if (context_.apply_caesuras) {
        context_.pause_map->calculate(score);
    }
}

void CompatMidiRendererInternal::fill_articulations_info() {
    for (const Part *part : score->parts()) {
        for (const auto &[tick, instr] : part->instruments()) {
            String instr_id = instr->id();
            for (auto it = Context::s_built_in_articulations_values.cbegin(); it != Context::s_built_in_articulations_values.cend(); it++) {
                const String &articulation_name = it->first;
                const std::vector<MidiArticulation> &instr_articulations = instr->articulation();
                bool instr_has_articulation = std::any_of(instr_articulations.begin(), instr_articulations.end(),
                                                          [articulation_name](const MidiArticulation &instr_articulation) {
                                                              return instr_articulation.name == articulation_name;
                                                          });

                if (!instr_has_articulation) {
                    context_.articulations_without_values_by_instrument[instr_id].insert(articulation_name);
                }
            }
        }
    }
}

double chord_velocity_multiplier(const Chord *chord, const CompatMidiRendererInternal::Context &context) {
    double velo_multiplier = 1.0;
    Instrument *instr = chord->part()->instrument();
    for (Articulation *a : chord->articulations()) {
        if (a->play_articulation()) {
            velo_multiplier *= velocity_multiplier_by_instrument(instr, a->articulation_name(), context);
        }
    }

    return velo_multiplier;
}

double velocity_multiplier_by_instrument(const Instrument *instrument, const String &articulation_name,
                                         const CompatMidiRendererInternal::Context &context) {
    using Ctx = CompatMidiRendererInternal::Context;
    if (context.use_default_articulations) {
        auto it = Ctx::s_built_in_articulations_values.find(articulation_name);
        if (it != Ctx::s_built_in_articulations_values.end()) {
            return it->second.velocity_multiplier;
        }
    } else {
        auto articulations_for_instrument_it = context.articulations_without_values_by_instrument.find(instrument->id());
        if (articulations_for_instrument_it != context.articulations_without_values_by_instrument.end()) {
            const auto &articulations_for_instrument = articulations_for_instrument_it->second;
            if (articulations_for_instrument.find(articulation_name) != articulations_for_instrument.end()) {
                return Ctx::s_built_in_articulations_values[articulation_name].velocity_multiplier;
            }
        }
    }

    return instrument->get_velocity_multiplier(articulation_name);
}

/* static */
uint32_t get_channel(const Instrument *instr, const Note *note, MidiInstrumentEffect effect,
                     const CompatMidiRendererInternal::Context &context) {
    int subchannel = note->subchannel();
    int channel = instr->channel(subchannel)->channel();

    if (!context.instruments_have_effects && !context.each_string_has_channel) {
        return channel;
    }

    CompatMidiRendererInternal::ChannelLookup::LookupData lookup_data;

    if (context.instruments_have_effects) {
        lookup_data.effect = effect;
    }

    if (context.each_string_has_channel && instr->has_strings()) {
        if (note->string() >= 0) {
            lookup_data.string = note->string();
        } else {
            int string = 0;
            int fret = 0;
            const StringData *string_data = instr->string_data();
            IF_ASSERT_FAILED(string_data && string_data->convert_pitch(note->pitch(), note->staff(), &string, &fret)) {
                LOGE() << "channel isn't calculated for instrument " << instr->name_as_plain_text();
                return channel;
            }

            lookup_data.string = string;
        }

        lookup_data.staff_idx = note->staff_idx();
    }

    return context.channels->get_channel(channel, lookup_data);
}

uint32_t CompatMidiRendererInternal::ChannelLookup::get_channel(uint32_t instrument_channel, const LookupData &lookup_data) {
    auto &channels_for_instrument = channels_map[instrument_channel];

    auto channel_it = channels_for_instrument.find(lookup_data);
    if (channel_it != channels_for_instrument.end()) {
        return channel_it->second;
    }

    channels_for_instrument.insert({lookup_data, max_channel});
    return max_channel++;
}

bool CompatMidiRendererInternal::ChannelLookup::LookupData::operator<(
    const CompatMidiRendererInternal::ChannelLookup::LookupData &other) const {
    if (harmony && !other.harmony) {
        return true;
    }

    if (!harmony && other.harmony) {
        return false;
    }

    return std::tie(string, staff_idx, effect) < std::tie(other.string, other.staff_idx, other.effect);
}

// In the case that graceNotesBefore or graceNotesAfter are attached to a note
// with an articulation such as a trill, then the grace notes are/will-be/have-been
// already merged into the articulation.
// So this predicate, graceNotesMerged, checks for this condition to avoid calling
// functions which would re-emit the grace notes by a different algorithm.

bool CompatMidiRendererInternal::grace_notes_merged(Chord *chord) {
    if (find_first_trill(chord)) {
        return true;
    }
    for (Articulation *a : chord->articulations()) {
        for (auto &oe : excursions) {
            if (oe.atype == a->sym_id()) {
                return true;
            }
        }
    }
    return false;
}

/* static */
void fill_hairpin_velocities(const Hairpin *h, std::unordered_map<track_idx_t, VelocityMap> &velocities_by_track) {
    Staff *st = h->staff();
    Fraction tick = h->tick();
    Fraction tick2 = h->tick2();
    int velo_change = h->velo_change();
    ChangeMethod method = h->velo_change_method();

    // Make the change negative when the hairpin is a diminuendo
    HairpinType htype = h->hairpin_type();
    ChangeDirection direction = ChangeDirection::INCREASING;
    if (htype == HairpinType::DIM_HAIRPIN || htype == HairpinType::DIM_LINE) {
        velo_change *= -1;
        direction = ChangeDirection::DECREASING;
    }

    switch (h->voice_assignment()) {
        case VoiceAssignment::ALL_VOICE_IN_STAFF:
            if (st->is_primary_staff()) {
                for (track_idx_t track = st->idx() * ::mu::engraving::voices; track < (st->idx() + 1) * ::mu::engraving::voices; ++track) {
                    velocities_by_track[track].add_hairpin(tick, tick2, velo_change, method, direction);
                }
            }
            break;
        case VoiceAssignment::ALL_VOICE_IN_INSTRUMENT:
            for (Staff *s : st->part()->staves()) {
                if (!s->is_primary_staff()) {
                    continue;
                }
                for (track_idx_t track = s->idx() * ::mu::engraving::voices; track < (s->idx() + 1) * ::mu::engraving::voices; ++track) {
                    velocities_by_track[track].add_hairpin(tick, tick2, velo_change, method, direction);
                }
            }
            break;
        case VoiceAssignment::CURRENT_VOICE_ONLY:
            if (st->is_primary_staff()) {
                velocities_by_track[h->track()].add_hairpin(tick, tick2, velo_change, method, direction);
            }
            break;
    }
}

void fill_score_velocities(const Score *score, CompatMidiRendererInternal::Context &context) {
    Score *main_score = score->master_score();

    if (!main_score->first_measure()) {
        return;
    }

    for (Segment *s = main_score->first_measure()->first(); s; s = s->next1()) {
        Fraction tick = s->tick();
        for (const EngravingItem *e : s->annotations()) {
            if (!e->is_dynamic() || !e->staff()->is_primary_staff()) {
                continue;
            }

            const Dynamic *d = to_dynamic(e);
            int v = d->velocity();

            // treat an invalid dynamic as no change, i.e. a dynamic set to 0
            if (v < 1) {
                continue;
            }

            // make sure value is legal
            v = std::clamp(v, 1, 127);

            // If a dynamic has 'velocity change' update its ending
            int change = d->change_in_velocity();
            ChangeDirection direction = ChangeDirection::INCREASING;
            if (change < 0) {
                direction = ChangeDirection::DECREASING;
            }

            const Fraction etick = change != 0 ? tick + d->velocity_change_length(main_score->multiplied_tempo(tick)) : tick;

            switch (d->voice_assignment()) {
                case VoiceAssignment::ALL_VOICE_IN_STAFF: {
                    for (track_idx_t track = d->staff_idx() * ::mu::engraving::voices;
                         track < (d->staff_idx() + 1) * ::mu::engraving::voices; ++track) {
                        context.velocities_by_track[track].add_dynamic(tick, v);
                    }
                    if (change != 0) {
                        ChangeMethod method = ChangeMethod::NORMAL;
                        for (track_idx_t track = d->staff_idx() * ::mu::engraving::voices;
                             track < (d->staff_idx() + 1) * ::mu::engraving::voices; ++track) {
                            context.velocities_by_track[track].add_hairpin(tick, etick, change, method, direction);
                        }
                    }
                } break;
                case VoiceAssignment::ALL_VOICE_IN_INSTRUMENT: {
                    Part *part = d->staff()->part();
                    staff_idx_t p_start_staff = part->staves().front()->idx();
                    staff_idx_t p_end_staff = part->staves().back()->idx();
                    if (d->staff_idx() < p_start_staff || d->staff_idx() > p_end_staff) {
                        break;
                    }
                    for (staff_idx_t staff_idx = p_start_staff; staff_idx <= p_end_staff; ++staff_idx) {
                        Staff *stp = main_score->staff(staff_idx);
                        if (!stp->is_primary_staff()) {
                            continue;
                        }
                        for (track_idx_t track = stp->idx() * ::mu::engraving::voices; track < (stp->idx() + 1) * ::mu::engraving::voices;
                             ++track) {
                            context.velocities_by_track[track].add_dynamic(tick, v);
                        }
                        if (change != 0) {
                            ChangeMethod method = ChangeMethod::NORMAL;
                            for (track_idx_t track = stp->idx() * ::mu::engraving::voices;
                                 track < (stp->idx() + 1) * ::mu::engraving::voices; ++track) {
                                context.velocities_by_track[track].add_hairpin(tick, etick, change, method, direction);
                            }
                        }
                    }
                } break;
                case VoiceAssignment::CURRENT_VOICE_ONLY: {
                    context.velocities_by_track[d->track()].add_dynamic(tick, v);
                    if (change != 0) {
                        ChangeMethod method = ChangeMethod::NORMAL;
                        context.velocities_by_track[d->track()].add_hairpin(tick, etick, change, method, direction);
                    }
                } break;
            }
        }

        if (s->is_chord_rest_type()) {
            for (track_idx_t track = 0; track < main_score->ntracks(); ++track) {
                EngravingItem *el = s->element(track);
                if (!el || !el->is_chord() || !el->staff()->is_primary_staff()) {
                    continue;
                }

                Chord *chord = to_chord(el);

                double velo_multiplier = chord_velocity_multiplier(chord, context);

                if (muse::real_is_equal(velo_multiplier, 1.0)) {
                    continue;
                }

                Fraction articulation_change_time = std::min(s->ticks(), ::mu::engraving::articulation_change_time_max);
                int start = velo_multiplier * CompatMidiRendererInternal::articulation_conv_factor;
                int change = (velo_multiplier - 1) * CompatMidiRendererInternal::articulation_conv_factor;
                context.velocity_multiplications_by_track[track].add_dynamic(chord->tick(), start);
                context.velocity_multiplications_by_track[track].add_hairpin(chord->tick(), chord->tick() + articulation_change_time,
                                                                             change, ChangeMethod::NORMAL, ChangeDirection::DECREASING);
            }
        }
    }

    for (const auto &sp : main_score->spanner_map().map()) {
        Spanner *s = sp.second;
        if (!s->is_hairpin() || !s->staff()->is_primary_staff()) {
            continue;
        }

        fill_hairpin_velocities(to_hairpin(s), context.velocities_by_track);
    }

    for (Staff *st : main_score->staves()) {
        if (!st->is_primary_staff()) {
            continue;
        }
        for (track_idx_t track = st->idx() * ::mu::engraving::voices; track < (st->idx() + 1) * ::mu::engraving::voices; ++track) {
            context.velocities_by_track[track].setup();
            context.velocity_multiplications_by_track[track].setup();
        }
    }

    for (auto it = main_score->spanner().cbegin(); it != main_score->spanner().cend(); ++it) {
        Spanner *spanner = (*it).second;
        if (!spanner->is_volta()) {
            continue;
        }

        Volta *volta = to_volta(spanner);
        Staff *st = volta->staff();
        if (!st->is_primary_staff()) {
            continue;
        }
        for (track_idx_t track = st->idx() * ::mu::engraving::voices; track < (st->idx() + 1) * ::mu::engraving::voices; ++track) {
            fill_volta_velocities(volta, context.velocities_by_track[track]);
        }
    }
}

/* static */
void fill_volta_velocities(const Volta *volta, VelocityMap &velo_map) {
    Measure *start_measure = volta->start_measure();
    Measure *end_measure = volta->end_measure();

    if (start_measure && end_measure) {
        if (!end_measure->repeat_end()) {
            return;
        }

        Fraction start_tick = start_measure->tick() - Fraction::eps();
        Fraction end_tick = end_measure->end_tick() - Fraction::eps();
        int prev_velo = velo_map.val(start_tick);
        velo_map.add_dynamic(end_tick, prev_velo);
    }
}
} // namespace mu::engraving
