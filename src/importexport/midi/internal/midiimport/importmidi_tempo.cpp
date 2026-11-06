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
#include "importmidi_tempo.h"

#include <cmath>
#include <map>

#include "importmidi_inner.h"
#include "importmidi_beat.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/factory.h"
#include "engraving/types/constants.h"
#include "importmidi_operations.h"

#include "global/realfn.h"
#include "global/log.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace MidiTempo {
ReducedFraction time2_tick(double time, double ticks_per_sec) { return ReducedFraction::from_ticks(int(ticks_per_sec * time)); }

// tempo in beats per second

double find_basic_tempo(const std::multimap<int, MTrack> &tracks, bool is_human_performance) {
    for (const auto &track : tracks) {
        // don't read tempo from tempo track for human performed files
        // because very often the tempo in such track is completely erroneous
        if (is_human_performance && track.second.chords.empty()) {
            continue;
        }
        for (const auto &ie : track.second.mtrack->events()) {
            const MidiEvent &e = ie.second;
            if (e.type() == ME_META && e.meta_type() == META_TEMPO) {
                const uchar *data = (uchar *) e.edata();
                const unsigned tempo = data[2] + (data[1] << 8) + (data[0] << 16);
                return 1000000.0 / double(tempo);
            }
        }
    }

    return 2; // default beats per second = 120 beats per minute
}

static void set_tempo_to_score(Score *score, int tick, double beats_per_second, const bool round_tempo, BeatsPerSecond &last_tempo) {
    Measure *measure = score->tick2measure(Fraction::from_ticks(tick));
    if (!measure) {
        LOGD("MidiTempo::setTempoToScore: no measure for tick %d", tick);
        return;
    }

    const Fraction f = Fraction::from_ticks(tick);
    Segment *existing_segment = measure->find_segment(SegmentType::CHORD_REST, f);
    if (!existing_segment) {
        existing_segment = measure->find_segment(SegmentType::TIME_TICK, f);
    }

    if (existing_segment) {
        for (EngravingItem *e : existing_segment->annotations()) {
            if (e->is_tempo_text()) {
                last_tempo = to_tempo_text(e)->tempo();
                return; // already have a tempo marking here
            }
        }
    }

    // don't repeat tempo, always set only tempo for tick 0
    if (tick > 0 && last_tempo == beats_per_second) {
        return;
    }

    auto *data = midi_import_operations.data();
    const bool show_tempo_text = data->track_opers.show_tempo_text.value();

    if (!show_tempo_text && tick == 0 && muse::real_is_equal(beats_per_second, Constants::default_tempo.val)) {
        return;
    }

    const double tempo_in_bpm = round_tempo ? static_cast<int>(std::lround(beats_per_second * 60.0)) : (beats_per_second * 60.0);

    Segment *segment = measure->get_chord_rest_or_time_tick_segment(f);
    TempoText *tempo_text = mu::engraving::Factory::create_tempo_text(segment);
    tempo_text->set_tempo(beats_per_second);
    tempo_text->set_xml_text(String(u"<sym>metNoteQuarterUp</sym> = %1").arg(tempo_in_bpm));
    tempo_text->set_track(0);
    tempo_text->set_visible(show_tempo_text);
    segment->add(tempo_text);

    last_tempo = beats_per_second;

    if (show_tempo_text) {
        data->has_tempo_text = true; // to show tempo text column in the MIDI import panel
    }
}

static inline double round_to_bpm(double beats_per_second) { return static_cast<int>(std::lround(beats_per_second * 60.0)) / 60.0; }

static void apply_all_tempo_events(const std::multimap<int, MTrack> &tracks, Score *score, const bool round_tempo) {
    // Tempo-only tracks all share the same key in `tracks`, so iterating it directly would visit
    // their events track-by-track rather than in score-tick order. Collect first, then apply in tick order.
    std::multimap<int, double> tempo_events; // score tick -> beats per second

    for (const auto &track : tracks) {
        if (track.second.is_division_in_tps) { // ticks per second
            const double ticks_per_beat = Constants::division;
            const double beats_per_second =
                round_tempo ? round_to_bpm(track.second.division / ticks_per_beat) : (track.second.division / ticks_per_beat);
            tempo_events.emplace(0, beats_per_second);
        } else { // beats per second
            for (const auto &ie : track.second.mtrack->events()) {
                const MidiEvent &e = ie.second;
                if (e.type() == ME_META && e.meta_type() == META_TEMPO) {
                    const auto tick = to_muse_score_ticks(ie.first, track.second.division, false);
                    const uchar *data = (uchar *) e.edata();
                    const unsigned tempo = data[2] + (data[1] << 8) + (data[0] << 16);
                    const double beats_per_second = round_tempo ? round_to_bpm(1000000.0 / tempo) : (1000000.0 / tempo);
                    tempo_events.emplace(tick.ticks(), beats_per_second);
                }
            }
        }
    }

    BeatsPerSecond last_tempo = Constants::default_tempo;
    for (const auto &[tick, beats_per_second] : tempo_events) {
        set_tempo_to_score(score, tick, beats_per_second, round_tempo, last_tempo);
    }
}

void set_tempo(const std::multimap<int, MTrack> &tracks, Score *score) {
    // The engine has no MIDI import preferences: the upstream default rounds tempi.
    const bool round_tempo = true;

    auto *midi_data = midi_import_operations.data();
    std::set<ReducedFraction> beats = midi_data->human_beat_data.beat_set; // copy

    if (beats.empty()) {
        // it's most likely not a human performance;
        // we find all tempo events and set tempo changes to score
        apply_all_tempo_events(tracks, score, round_tempo);
    } else { // calculate and set tempo from adjusted beat locations
        if (midi_data->track_opers.measure_count2x_less.value()) {
            MidiBeat::remove_every2nd_beat(beats);
        }

        Q_ASSERT_X(beats.size() > 1, "MidiBeat::setTempo", "Human beat count < 2");

        double average_tempo_factor = 0.0;
        int counter = 0;
        auto it = beats.begin();
        auto beat_start = *it;
        const auto new_beat_len = ReducedFraction::from_ticks(Constants::division);

        for (++it; it != beats.end(); ++it) {
            const auto &beat_end = *it;

            Q_ASSERT_X(beat_end > beat_start, "MidiBeat::detectTempoChanges", "Beat end <= beat start that is incorrect");

            average_tempo_factor += (new_beat_len / (beat_end - beat_start)).to_double();
            ++counter;
            beat_start = beat_end;
        }
        average_tempo_factor /= counter;

        const double basic_tempo = MidiTempo::find_basic_tempo(tracks, true);
        const double tempo = round_tempo ? round_to_bpm(basic_tempo * average_tempo_factor) : (basic_tempo * average_tempo_factor);

        BeatsPerSecond last_tempo = Constants::default_tempo;
        set_tempo_to_score(score, 0, tempo, round_tempo, last_tempo);
    }
}
}} // namespace mu::iex::midi::MidiTempo
