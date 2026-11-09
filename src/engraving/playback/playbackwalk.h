// SPDX-License-Identifier: GPL-3.0-only
#pragma once

//! NOTE: the score walk shared by the legacy PlaybackModel (test oracle, design exception E1) and the
//! listener-free PlaybackScoreBuilder (production export). The walk is the former PlaybackModel load
//! code, moved unchanged; only its writes go through a Store, selected at compile time:
//!
//!   bool contains(const InstrumentTrackId &) const;           // the track already exists
//!   void touch(const InstrumentTrackId &);                    // creates the track
//!   void resolve_setup(const InstrumentTrackId &, Resolve);   // creates the track, resolve(PlaybackSetupData &)
//!   const PlaybackSetupData *find_setup(const InstrumentTrackId &) const;
//!   void apply_context(const InstrumentTrackId &, const PlaybackPartData &, Walk &); // dynamics and context events
//!   void emit(const InstrumentTrackId &, Render);             // creates the track, render(sink)
//!
//! A track is created exactly where the legacy model created its map entry, so every store sees the same tracks

#include <functional>

#include <amc/flatset.hpp>

#include "../dom/fret.h"
#include "../dom/harmony.h"
#include "../dom/masterscore.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/part.h"
#include "../dom/repeatlist.h"
#include "../dom/segment.h"
#include "../dom/staff.h"
#include "../types/types.h"
#include "global/flatmap.h"
#include "log.h"
#include "mpe/events.h"
#include "mpe/iarticulationprofilesrepository.h"
#include "playbackcontext.h"
#include "playbackeventsrenderer.h"
#include "playbacksetupdataresolver.h"

namespace mu::engraving {
//! The metronome track, present in every walk
inline const InstrumentTrackId &playback_metronome_track_id() {
    static const InstrumentTrackId id{999, String(u"metronome")};
    return id;
}

inline InstrumentTrackId playback_chord_symbols_track_id(const ID &part_id) { return {part_id, String(u"chord_symbols")}; }

inline bool is_playback_chord_symbols_track(const InstrumentTrackId &track_id) {
    return track_id == playback_chord_symbols_track_id(track_id.part_id);
}

//! One part's context, applied to each of the part's tracks
struct PlaybackPartData {
    muse::mpe::DynamicAutomationLayers dynamics;
    muse::FlatMap<muse::mpe::timestamp_t, muse::mpe::SoundPresetChangeEventList> sound_presets;
    muse::FlatMap<muse::mpe::timestamp_t, muse::mpe::TextArticulationEventList> text_articulations;
    muse::FlatMap<muse::mpe::timestamp_t, muse::mpe::SyllableEventList> syllables;
};

template <class Store>
struct PlaybackWalk {
    using ChangedTrackIdSet = InstrumentTrackIdSet;

    Score *score = nullptr;
    const PlaybackContextPtr &playback_ctx;
    const PlaybackEventsRenderer &renderer;
    const PlaybackSetupDataResolver &setup_resolver;
    const muse::mpe::IArticulationProfilesRepository *profiles_repository = nullptr;
    const std::function<void()> &checkpoint_callback;
    bool expand_repeats = true;
    bool metronome_enabled = true;
    Store &store;

    void checkpoint() const {
        if (checkpoint_callback) checkpoint_callback();
    }

    void update(const int tick_from, const int tick_to, const track_idx_t track_from, const track_idx_t track_to,
                ChangedTrackIdSet *track_changes = nullptr) {
        score->master_score()->set_expand_repeats(expand_repeats);
        update_setup_data();
        update_context(track_from, track_to, tick_from, tick_to);
        update_events(tick_from, tick_to, track_from, track_to, track_changes);
    }

    const RepeatList &repeat_list() const {
        score->master_score()->set_expand_repeats(expand_repeats);

        return score->repeat_list();
    }

    muse::mpe::ArticulationsProfilePtr default_articulation_profile(const InstrumentTrackId &track_id) const {
        const muse::mpe::PlaybackSetupData *setup_data = store.find_setup(track_id);
        if (!setup_data) {
            return nullptr;
        }

        if (!profiles_repository) {
            return nullptr;
        }

        return profiles_repository->default_profile(setup_data->category);
    }

    void update_setup_data() {
        EID score_eid = score->eid();
        if (!score_eid.is_valid()) {
            score_eid = score->assign_new_eid();
        }

        std::string score_id = score_eid.to_std_string();

        for (const Part *part : score->parts()) {
            checkpoint();
            for (const auto &pair : part->instruments()) {
                checkpoint();
                InstrumentTrackId track_id{part->id(), pair.second->id()};
                if (!track_id.is_valid() || store.contains(track_id)) {
                    continue;
                }

                store.resolve_setup(track_id, [&](muse::mpe::PlaybackSetupData &setup_data) {
                    setup_resolver.resolve_setup_data(pair.second, setup_data);
                    setup_data.score_id = score_id;
                });
            }

            if (part->has_chord_symbol()) {
                InstrumentTrackId track_id = playback_chord_symbols_track_id(part->id());
                store.resolve_setup(track_id, [&](muse::mpe::PlaybackSetupData &setup_data) {
                    setup_resolver.resolve_chord_symbols_setup_data(part->instrument(), setup_data);
                    setup_data.score_id = score_id;
                });
            }
        }

        store.resolve_setup(playback_metronome_track_id(), [&](muse::mpe::PlaybackSetupData &setup_data) {
            setup_resolver.resolve_metronome_setup_data(setup_data);
            setup_data.score_id = score_id;
        });
    }

    void update_context(const track_idx_t track_from, const track_idx_t track_to, const int tick_from, const int tick_to) {
        playback_ctx->update(track_from, track_to, tick_from, tick_to, expand_repeats, checkpoint_callback);

        for (const Part *part : score->parts()) {
            checkpoint();
            const TrackRange track_range = part->track_range();
            if (track_to <= track_range.start_track || track_from >= track_range.end_track) {
                continue;
            }

            InstrumentTrackIdSet track_ids = part->instrument_track_id_set();
            if (part->has_chord_symbol()) {
                track_ids.insert(playback_chord_symbols_track_id(part->id()));
            }

            if (track_ids.empty()) {
                continue;
            }

            const PlaybackPartData data{playback_ctx->dynamic_level_layers(track_range.start_track, track_range.end_track),
                                        playback_ctx->sound_presets(track_range.start_track, track_range.end_track),
                                        playback_ctx->text_articulations(track_range.start_track, track_range.end_track),
                                        playback_ctx->syllables(track_range.start_track, track_range.end_track)};

            for (const InstrumentTrackId &track_id : track_ids) {
                checkpoint();
                store.apply_context(track_id, data, *this);
            }
        }
    }

    void process_segment(const int tick_position_offset, const Segment *segment, const amc::FlatSet<staff_idx_t> &staff_idx_set,
                         bool is_first_chord_rest_segment_of_measure, ChangedTrackIdSet *track_changes) {
        for (const EngravingItem *item : segment->annotations()) {
            checkpoint();
            if (!item || !item->part()) {
                continue;
            }

            const Harmony *chord_symbol = find_chord_symbol(item);
            if (!chord_symbol) {
                continue;
            }

            staff_idx_t staff_idx = item->staff_idx();
            if (staff_idx_set.find(staff_idx) == staff_idx_set.cend()) {
                continue;
            }

            InstrumentTrackId track_id = playback_chord_symbols_track_id(item->part()->id());

            muse::mpe::ArticulationsProfilePtr profile = default_articulation_profile(track_id);
            if (!profile) {
                LOGE() << "unsupported instrument family: " << item->part()->id();
                continue;
            }

            if (chord_symbol->play()) {
                store.emit(track_id, [&](auto &sink) {
                    renderer.render_chord_symbol(chord_symbol, tick_position_offset, profile, playback_ctx, sink);
                });
            }

            collect_changes_tracks(track_id, track_changes);
        }

        if (segment->is_time_tick_type()) {
            return; // optimization: search only for annotations
        }

        for (const EngravingItem *item : segment->elist()) {
            checkpoint();
            if (!item || !item->is_chord_rest() || !item->part()) {
                continue;
            }

            staff_idx_t staff_idx = item->staff_idx();
            if (staff_idx_set.find(staff_idx) == staff_idx_set.cend()) {
                continue;
            }

            InstrumentTrackId track_id = id_key(item);

            if (!track_id.is_valid()) {
                continue;
            }

            if (is_first_chord_rest_segment_of_measure) {
                if (item->is_measure_repeat()) {
                    const MeasureRepeat *measure_repeat = to_measure_repeat(item);
                    const Measure *current_measure = measure_repeat->measure();

                    process_measure_repeat(tick_position_offset, measure_repeat, current_measure, staff_idx, track_changes);

                    continue;
                } else if (item->voice() == 0) {
                    const Measure *current_measure = segment->measure();

                    if (current_measure->measure_repeat_count(staff_idx) > 0) {
                        const MeasureRepeat *measure_repeat = current_measure->measure_repeat_element(staff_idx);

                        process_measure_repeat(tick_position_offset, measure_repeat, current_measure, staff_idx, track_changes);
                        continue;
                    }
                }
            }

            if (item->is_rest_family()) {
                continue;
            }

            muse::mpe::ArticulationsProfilePtr profile = default_articulation_profile(track_id);
            if (!profile) {
                LOGE() << "unsupported instrument family: " << item->part()->id();
                continue;
            }

            store.emit(track_id, [&](auto &sink) { renderer.render(item, tick_position_offset, profile, playback_ctx, sink); });

            collect_changes_tracks(track_id, track_changes);
        }
    }

    void process_measure_repeat(const int tick_position_offset, const MeasureRepeat *measure_repeat, const Measure *current_measure,
                                const staff_idx_t staff_idx, ChangedTrackIdSet *track_changes) {
        if (!measure_repeat || !current_measure) {
            return;
        }

        const Measure *referring_measure = measure_repeat->referring_measure(current_measure);
        if (!referring_measure) {
            return;
        }

        IF_ASSERT_FAILED(referring_measure != current_measure) { return; }

        int current_measure_tick = current_measure->tick().ticks();
        int referring_measure_tick = referring_measure->tick().ticks();
        int repeat_position_tick_offset = current_measure_tick - referring_measure_tick;
        int tick_from = tick_position_offset + repeat_position_tick_offset;

        amc::FlatSet<staff_idx_t> staff_to_process_idx_set{staff_idx};
        int chord_rest_segment_num = -1;

        for (const Segment *seg = referring_measure->first(); seg; seg = seg->next()) {
            checkpoint();
            if (!seg->is_chord_rest_type() && !seg->is_time_tick_type()) {
                continue;
            }

            if (seg->is_chord_rest_type()) {
                chord_rest_segment_num++;
            }

            process_segment(tick_from, seg, staff_to_process_idx_set, chord_rest_segment_num == 0, track_changes);
        }
    }

    void update_events(const int tick_from, const int tick_to, const track_idx_t track_from, const track_idx_t track_to,
                       ChangedTrackIdSet *track_changes = nullptr) {
        TRACEFUNC;

        const auto dom_staves = score->staff_idx_set_from_range(track_from, track_to, [](const Staff &staff) {
            return staff.is_primary_staff(); // skip linked staves
        });
        std::ignore = muse::checked_grow<staff_idx_t>(0, dom_staves.size());
        amc::FlatSet<staff_idx_t> staff_to_process_idx_set(dom_staves.begin(), dom_staves.end());

        const muse::mpe::ArticulationsProfilePtr metronome_profile = default_articulation_profile(playback_metronome_track_id());
        // The metronome entry exists before the walk, as the legacy sink construction here always created it
        store.touch(playback_metronome_track_id());

        for (const RepeatSegment *repeat_segment : repeat_list()) {
            checkpoint();
            int tick_position_offset = repeat_segment->utick - repeat_segment->tick;
            int repeat_start_tick = repeat_segment->tick;
            int repeat_end_tick = repeat_segment->end_tick();

            if (repeat_start_tick > tick_to || repeat_end_tick <= tick_from) {
                continue;
            }

            for (const Measure *measure : repeat_segment->measure_list()) {
                checkpoint();
                int measure_start_tick = measure->tick().ticks();
                int measure_end_tick = measure->end_tick().ticks();

                if (measure_start_tick > tick_to || measure_end_tick <= tick_from) {
                    continue;
                }

                int chord_rest_segment_num = -1;

                for (const Segment *segment = measure->first(); segment; segment = segment->next()) {
                    checkpoint();
                    if (!segment->is_chord_rest_type() && !segment->is_time_tick_type()) {
                        continue;
                    }

                    int segment_start_tick = segment->tick().ticks();
                    int segment_end_tick = segment_start_tick + segment->ticks().ticks();

                    if (segment_start_tick > tick_to || segment_end_tick <= tick_from) {
                        continue;
                    }

                    if (segment->is_chord_rest_type()) {
                        chord_rest_segment_num++;
                    }

                    process_segment(tick_position_offset, segment, staff_to_process_idx_set, chord_rest_segment_num == 0, track_changes);
                }

                if (metronome_enabled) {
                    store.emit(playback_metronome_track_id(), [&](auto &sink) {
                        renderer.render_metronome(score, measure, tick_position_offset, metronome_profile, sink);
                    });
                    collect_changes_tracks(playback_metronome_track_id(), track_changes);
                }
            }
        }
    }

    InstrumentTrackId id_key(const EngravingItem *item) const {
        if (item->is_harmony()) {
            return playback_chord_symbols_track_id(item->part()->id());
        }

        return make_instrument_track_id(item);
    }

    static const Harmony *find_chord_symbol(const EngravingItem *item) {
        if (item->is_harmony()) {
            return to_harmony(item);
        } else if (item->is_fret_diagram()) {
            return to_fret_diagram(item)->harmony();
        }

        return nullptr;
    }

    static void collect_changes_tracks(const InstrumentTrackId &track_id, ChangedTrackIdSet *result) {
        if (!result) {
            return;
        }

        result->insert(track_id);
    }
};
} // namespace mu::engraving
