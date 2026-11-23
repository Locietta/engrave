/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "stavesharinglayout.h"

#include <algorithm>
#include <climits>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <gtl/btree.hpp>

#include "dom/breath.h"
#include "dom/chord.h"
#include "dom/factory.h"
#include "dom/hairpin.h"
#include "dom/instrchange.h"
#include "dom/keysig.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/part.h"
#include "dom/partialtie.h"
#include "dom/pitchspelling.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/sig.h"
#include "dom/staff.h"
#include "dom/stavesharingchange.h"
#include "dom/system.h"
#include "dom/timesig.h"
#include "engraving/dom/dummyparent.h"
#include "layoutdatastore.h"
#include "measurelayout.h"
#include "scorerenderer.h"
#include "systemheaderlayout.h"

using namespace mu::engraving;

namespace mu::engraving::rendering::score {
namespace {
LayoutDataStore &output(EngravingItem *item) { return item->renderer()->layout_data_store(*item->score()); }

void add_shared_output(EngravingItem *item) {
    item->set_layout_copy(true);
    // Note::clone copies an outgoing tie with no endpoint. Sharing reconnects
    // ties from their origins after all display notes are available.
    const auto clear_copied_ties = [&](auto &&self, EngravingItem *copy) -> void {
        if (copy->is_note()) {
            auto *note = to_note(copy);
            if (auto *tie = note->tie_for()) {
                note->set_tie_for(nullptr);
                delete tie;
            }
            if (auto *tie = note->incoming_partial_tie()) {
                note->set_tie_back(nullptr);
                delete tie;
            }
        }
        if (copy->is_chord()) {
            for (auto *note : to_chord(copy)->notes()) self(self, note);
            for (auto *grace : to_chord(copy)->grace_notes()) self(self, grace);
        }
    };
    if (item->is_chord() || item->is_note()) clear_copied_ties(clear_copied_ties, item);
    auto *parent = item->ownership_parent_item();
    if (item->is_spanner()) {
        auto *spanner = to_spanner(item);
        output(item).score_output().shared_spanners.push_back(spanner);
        if (parent && parent->is_note()) parent->add(item);
    } else if (item->is_tuplet()) {
        output(item).score_output().shared_tuplets.push_back(to_tuplet(item));
    } else if (parent->is_segment()) {
        to_segment(parent)->add_display_item(item);
    } else {
        assert(parent->is_layout_copy());
        parent->add(item);
    }
}

void remove_shared_output(EngravingItem *item) {
    assert(item->is_layout_copy());
    auto &store = output(item);
    if (auto *measure = item->find_measure()) store.retire_measure_borrowers(*measure);
    if (!item->is_spanner()) {
        auto spans = store.score_output().shared_spanners;
        for (auto *span : spans) {
            const auto belongs = [&](const EngravingItem *anchor) {
                for (const EngravingObject *owner = anchor; owner; owner = owner->ownership_parent())
                    if (owner == item) return true;
                return false;
            };
            const bool loses_start = belongs(span->start_element());
            const bool loses_end = belongs(span->end_element());
            if (span->anchor() == Spanner::Anchor::NOTE) {
                if (loses_start || loses_end) remove_shared_output(span);
            } else {
                // Track-based copies can reacquire replaced anchors while
                // keeping the segments already laid out in other systems.
                if (loses_start) span->set_start_element(nullptr);
                if (loses_end) span->set_end_element(nullptr);
            }
        }
    }
    if (item->is_spanner()) {
        auto *span = to_spanner(item);
        if (auto *parent = span->ownership_parent_item(); parent && parent->is_note()) parent->remove(span);

        std::erase(store.score_output().shared_spanners, span);
    } else if (item->is_tuplet()) {
        std::erase(store.score_output().shared_tuplets, to_tuplet(item));
    } else if (auto *parent = item->ownership_parent_item(); parent->is_segment()) {
        to_segment(parent)->remove_display_item(item);
    } else {
        parent->remove(item);
    }
    delete item;
}

EngravingItem *shared_slot(Segment *segment, track_idx_t track) {
    const auto &copies = segment->ldata()->display_copies;
    return track < copies.size() ? copies[track] : nullptr;
}

void connect_output(EngravingItem *shared, EngravingItem *origin) {
    EngravingItem::connect_shared_item(shared, origin);
    origin->score()->register_layout_copy(*origin, *shared);
}
} // namespace

bool StaveSharingLayout::update_stave_sharing_for_full_system(MeasureBase *first_mb, MeasureBase *last_mb, LayoutContext &ctx,
                                                              bool display_measures) {
    StaveSharingContext ssctx(first_mb, last_mb, ctx, display_measures);
    if (ssctx.cr_segments.empty()) return false;

    ssctx.update_for_last_added = false;
    update_stave_sharing(ssctx);
    return ssctx.track_map_changed;
}

bool StaveSharingLayout::update_stave_sharing_for_last_added_measure(System *system, LayoutContext &ctx) {
    if (!system->last()->is_measure()) {
        return false;
    }

    StaveSharingContext ssctx(system->first(), system->last(), ctx, true);
    if (ssctx.cr_segments.empty()) {
        return false;
    }

    ssctx.update_for_last_added = true;

    update_stave_sharing(ssctx);

    if (ssctx.track_map_changed) relayout_measures_on_track_map_changed(system, ctx);
    return ssctx.track_map_changed;
}

void StaveSharingLayout::relayout_measures_on_track_map_changed(System *system, LayoutContext &ctx) {
    SystemHeaderLayout::update_system_header_width(system, ctx);
    for (MeasureBase *mb : system->measures()) {
        if (mb->is_measure()) {
            to_measure(mb)->mutldata()->set_need_layout(true);
            MeasureLayout::layout_measure(mb, ctx);
        }
    }
}

void StaveSharingLayout::update_notation_without_recomputing_track_map(Measure *measure, LayoutContext &ctx) {
    StaveSharingContext ssctx(measure, measure, ctx);
    if (ssctx.cr_segments.empty()) {
        return;
    }

    for (Part *p : ssctx.layout_ctx.dom().parts()) {
        if (p->is_shared_part() && to_shared_part(p)->layout_show()) {
            SharedPart *shared_part = to_shared_part(p);
            ssctx.cur_shared_part = shared_part;
            update_notation(ssctx);
        }
    }

    measure->mutldata()->set_need_layout(true);
    MeasureLayout::layout_measure(measure, ctx);
}

void StaveSharingLayout::update_stave_sharing(StaveSharingContext &ctx) {
    for (Part *p : ctx.layout_ctx.dom().parts()) {
        if (p->is_shared_part() && to_shared_part(p)->layout_show()) {
            SharedPart *shared_part = to_shared_part(p);
            ctx.cur_shared_part = shared_part;
            ctx.collect_overlapping_spanners();
            update_track_maps(ctx);
            update_notation(ctx);
        }
    }
}

StaveSharingLayout::TrackGroup StaveSharingLayout::participating_tracks(const StaveSharingContext &ctx) {
    TrackGroup tracks;
    const auto segments = ctx.segments_in_range(ctx.cr_segments);
    for (const Part *part : ctx.cur_shared_part->origin_parts()) {
        if (part->staves().empty()) continue;
        const auto range = part->track_range();
        for (auto track = range.start_track; track < range.end_track; ++track) {
            if (track == range.start_track || std::ranges::any_of(segments, [track](const Segment *segment) {
                    const auto *cr = to_chord_rest(segment->effective_element(track));
                    return cr && !cr->is_full_measure_rest();
                })) {
                tracks.push_back(track);
            }
        }
    }
    return tracks;
}

bool StaveSharingLayout::is_valid(const SharedTrackMap &map, const TrackGroup &origin_tracks, StaveSharingContext &ctx) {
    if (map.empty() || !std::ranges::all_of(origin_tracks, [&](auto track) { return map.contains(track); })) return false;
    const SharedPart &part = *ctx.cur_shared_part;
    const auto shared_range = part.track_range();
    std::vector<std::pair<track_idx_t, track_idx_t>> by_shared_track;
    for (const auto &[origin, shared] : map) {
        if (shared < shared_range.start_track || shared >= shared_range.end_track) return false;
        if (!std::ranges::any_of(part.origin_parts(), [origin](const Part *p) {
                if (p->staves().empty()) return false;
                const auto range = p->track_range();
                return origin >= range.start_track && origin < range.end_track;
            }))
            return false;
        by_shared_track.emplace_back(shared, origin);
    }
    std::ranges::sort(by_shared_track);

    TrackGroup group;
    ankerl::unordered_dense::set<Note *> unison_notes;
    track_idx_t prev_shared = muse::nidx;
    for (const auto &[shared, origin] : by_shared_track) {
        if (shared != prev_shared) {
            if (prev_shared != muse::nidx && track2staff(shared) == track2staff(prev_shared)) {
                if (!can_go_to_same_stave(group.back(), origin, true, ctx)) return false;
            } else if (track2voice(shared) != 0) {
                return false;
            }
            group.clear();
            unison_notes.clear();
        } else if (!can_go_to_same_voice(group.back(), origin, ctx, group, unison_notes, ctx.layout_ctx.dom().staff(track2staff(shared)))) {
            return false;
        }
        group.push_back(origin);
        prev_shared = shared;
    }
    return true;
}

void StaveSharingLayout::update_track_maps(StaveSharingContext &ctx) {
    SharedPart &part = *ctx.cur_shared_part;
    const Fraction start = ctx.s_tick;
    const Fraction end = ctx.e_tick;
    const auto &authored = part.authored_states();
    const auto at_or_after = [&](const Fraction &tick) {
        return std::lower_bound(authored.begin(), authored.end(), tick,
                                [](const auto &entry, const Fraction &value) { return entry.first < value; });
    };
    const auto first = at_or_after(start);
    const auto last = at_or_after(end);
    const auto &old_states = part.rendered_states();
    const auto old_first = old_states.lower_bound(start);
    const auto old_last = old_states.lower_bound(end);
    const std::vector<RenderedStaveSharingStates::value_type> old_range(old_first, old_last);
    const SharedTrackMap old_start_map = part.track_map_at_tick(start);

    const auto resolve = [&](const Fraction &tick, const Fraction &until, const AuthoredStaveSharingState *state) {
        ctx.s_tick = tick;
        ctx.e_tick = until;
        const auto tracks = participating_tracks(ctx);
        const bool valid = state && !state->is_reset && is_valid(state->user_track_map, tracks, ctx);
        if (valid) return RenderedStaveSharingState{state->user_track_map, true};
        auto map = compute_track_map(tracks, ctx);
        return RenderedStaveSharingState{map ? std::move(*map) : SharedTrackMap{}, false, !map};
    };

    std::vector<std::pair<Fraction, RenderedStaveSharingState>> states;
    if (first == last || first->first > start) {
        const auto active = first == authored.begin() ? nullptr : &*std::prev(first);
        auto initial = resolve(start, first != last ? first->first : end, active ? &active->second : nullptr);
        states.emplace_back(start, std::move(initial));
    }
    for (auto it = first; it != last; ++it) {
        const auto next = std::next(it);
        states.emplace_back(it->first, resolve(it->first, next != last ? next->first : end, &it->second));
    }
    ctx.s_tick = start;
    ctx.e_tick = end;

    // A candidate system can gain or lose a measure while it is being fitted.
    // With no authored states the effective map is sufficient for this comparison.
    if (authored.empty()) {
        ctx.track_map_changed |= old_start_map != states.front().second.shared_track_map;
    } else {
        ctx.track_map_changed |=
            old_start_map != states.front().second.shared_track_map ||
            !std::ranges::equal(old_range, states, [](const auto &old_state, const auto &state) {
                return old_state.first == state.first && old_state.second.shared_track_map == state.second.shared_track_map;
            });
    }

    part.remove_rendered_states(start, end);
    for (auto &[tick, state] : states) part.set_rendered_state(tick, std::move(state));
}

std::optional<SharedTrackMap> StaveSharingLayout::compute_track_map(const TrackGroup &origin_tracks, StaveSharingContext &ctx) {
    ankerl::unordered_dense::set<track_idx_t> processed_tracks;

    ankerl::unordered_dense::map<track_idx_t, TrackGroup> unisons_map;
    for (size_t i = 0; i < origin_tracks.size(); ++i) {
        track_idx_t track1 = origin_tracks[i];
        if (muse::contains(processed_tracks, track1)) {
            continue;
        }

        for (size_t j = i + 1; j < origin_tracks.size(); ++j) {
            track_idx_t track2 = origin_tracks[j];
            if (muse::contains(processed_tracks, track2)) {
                continue;
            }
            if (is_unison(track1, track2, ctx)) {
                processed_tracks.insert(track1);
                processed_tracks.insert(track2);

                if (!muse::contains(unisons_map, track1)) {
                    unisons_map[track1] = {track1, track2};
                } else {
                    unisons_map.at(track1).push_back(track2);
                }
            }
        }
    }

    ankerl::unordered_dense::map<track_idx_t, TrackGroup> same_voice_map;
    for (size_t i = 0; i < origin_tracks.size(); ++i) {
        track_idx_t track1 = origin_tracks[i];
        if (muse::contains(processed_tracks, track1)) {
            continue;
        }
        processed_tracks.insert(track1);

        TrackGroup cur_track_group = {track1};
        ankerl::unordered_dense::set<Note *> local_unison_notes;

        track_idx_t last_in_this_voice = track1;
        for (size_t j = i + 1; j < origin_tracks.size(); ++j) {
            track_idx_t track2 = origin_tracks[j];
            if (muse::contains(processed_tracks, track2)) {
                continue;
            }
            if (can_go_to_same_voice(last_in_this_voice, track2, ctx, cur_track_group, local_unison_notes)) {
                processed_tracks.insert(track2);
                cur_track_group.push_back(track2);
                last_in_this_voice = track2;
            } else if (cur_track_group.size() > 1) {
                break;
            }
        }

        same_voice_map.emplace(track1, cur_track_group);
    }

    SharedTrackMap track_map;

    ankerl::unordered_dense::set<track_idx_t> assigned_tracks;
    const auto range = ctx.cur_shared_part->track_range();
    track_idx_t cur_shared_track = range.start_track;
    std::vector<bool> used_slots(range.end_track - range.start_track);
    const bool allow_crossing = ctx.style.style_b(Sid::ALLOW_VOICE_CROSSING);
    const auto compatible = [&](track_idx_t slot, const TrackGroup &group) {
        return std::ranges::all_of(track_map, [&](const auto &assignment) {
            const auto [origin, shared] = assignment;
            if (track2staff(shared) != track2staff(slot)) return true;
            return std::ranges::all_of(group, [&](track_idx_t track) {
                return shared < slot ? can_go_to_same_stave(origin, track, allow_crossing, ctx) :
                                       can_go_to_same_stave(track, origin, allow_crossing, ctx);
            });
        });
    };

    for (track_idx_t track : origin_tracks) {
        if (muse::contains(assigned_tracks, track)) {
            continue;
        }

        const auto unison = unisons_map.find(track);
        const auto &group = unison != unisons_map.end() ? unison->second : same_voice_map.at(track);
        while (cur_shared_track < range.end_track) {
            if (used_slots[cur_shared_track - range.start_track]) {
                ++cur_shared_track;
            } else if (!compatible(cur_shared_track, group)) {
                cur_shared_track = track_zero_voice(cur_shared_track + ::mu::engraving::voices);
            } else {
                break;
            }
        }
        if (cur_shared_track >= range.end_track) {
            cur_shared_track = range.start_track;
            while (cur_shared_track < range.end_track &&
                   (used_slots[cur_shared_track - range.start_track] || !compatible(cur_shared_track, group))) {
                ++cur_shared_track;
            }
            if (cur_shared_track == range.end_track) return std::nullopt;
        }
        used_slots[cur_shared_track - range.start_track] = true;

        for (track_idx_t origin_track : group) {
            track_map[origin_track] = cur_shared_track;
            assigned_tracks.insert(origin_track);
        }

        track2voice(cur_shared_track) == 0 ? ++cur_shared_track :
                                             cur_shared_track = track_zero_voice(cur_shared_track + ::mu::engraving::voices);
    }

    return track_map;
}

bool StaveSharingLayout::same_instrument(track_idx_t prev_track, track_idx_t next_track, const Fraction &tick, StaveSharingContext &ctx) {
    const Staff *next_staff = ctx.layout_ctx.dom().staff(track2staff(next_track));
    const Staff *prev_staff = ctx.layout_ctx.dom().staff(track2staff(prev_track));

    const Part *next_part = next_staff ? next_staff->part() : nullptr;
    const Part *prev_part = prev_staff ? prev_staff->part() : nullptr;

    const Instrument *next_instrument = next_part ? next_part->instrument(tick) : nullptr;
    const Instrument *prev_instrument = prev_part ? prev_part->instrument(tick) : nullptr;
    if (!next_instrument || !prev_instrument) {
        return false;
    }

    return next_instrument->id() == prev_instrument->id() && next_instrument->transpose() == prev_instrument->transpose();
}

// A shared Jianpu note counts from the key of its first origin (TLayout::jianpu_key_sig_event),
// and the key in effect can come from before the range, so origins share a voice only where
// their keys number the digits alike.
bool StaveSharingLayout::same_jianpu_numbering(track_idx_t prev_track, track_idx_t next_track, const Fraction &tick,
                                               const StaveSharingContext &ctx, const Staff *destination) {
    const std::vector<Staff *> &shared_staves = ctx.cur_shared_part->staves();
    const bool uses_jianpu = destination ?
                                 destination->is_jianpu_staff(tick) :
                                 std::ranges::any_of(shared_staves, [&tick](const Staff *staff) { return staff->is_jianpu_staff(tick); });
    if (!uses_jianpu) {
        return true;
    }
    const Staff *prev_staff = ctx.layout_ctx.dom().staff(track2staff(prev_track));
    const Staff *next_staff = ctx.layout_ctx.dom().staff(track2staff(next_track));
    if (!prev_staff || !next_staff) {
        return false;
    }
    return jianpu_key_mapping(prev_staff->key_sig_event(tick)) == jianpu_key_mapping(next_staff->key_sig_event(tick));
}

bool StaveSharingLayout::is_unison(track_idx_t prev_track, track_idx_t next_track, StaveSharingContext &ctx) {
    for (Segment *segment : ctx.segments_in_range(ctx.cr_segments)) {
        if (!same_instrument(prev_track, next_track, segment->tick(), ctx)) {
            return false;
        }

        ChordRest *cr1 = to_chord_rest(segment->effective_element(prev_track));
        ChordRest *cr2 = to_chord_rest(segment->effective_element(next_track));
        if (bool(cr1) != bool(cr2)) {
            return false;
        }

        if (!cr1) {
            continue;
        }

        if (cr1->is_chord() != cr2->is_chord()) {
            return false;
        }

        if (cr1->duration_type() != cr2->duration_type() || cr1->actual_ticks() != cr2->actual_ticks()) {
            return false;
        }

        if (!cr1->is_chord()) {
            continue;
        }

        if (!same_jianpu_numbering(prev_track, next_track, segment->tick(), ctx)) {
            return false;
        }

        Chord *c1 = to_chord(cr1);
        Chord *c2 = to_chord(cr2);

        auto check_chords_for_unison = [&](Chord *c1, Chord *c2) {
            if (c1->duration_type() != c2->duration_type() || c1->note_type() != c2->note_type()) {
                return false;
            }

            const std::vector<Note *> &notes1 = c1->notes();
            const std::vector<Note *> &notes2 = c2->notes();
            if (notes1.size() != notes2.size()) {
                return false;
            }

            for (size_t i = 0; i < notes1.size(); ++i) {
                Note *n1 = notes1[i];
                Note *n2 = notes2[i];

                if (!n1->is_exact_unison(n2)) {
                    return false;
                }

                if (!check_note_spanners_for_unison(n1, n2, ctx)) {
                    return false;
                }
            }

            if (!check_articulations_for_same_voice(c1, c2)) {
                return false;
            }

            return true;
        };

        if (!check_chords_for_unison(c1, c2)) {
            return false;
        }

        const std::vector<Chord *> &grace1 = c1->grace_notes();
        const std::vector<Chord *> &grace2 = c2->grace_notes();
        if (grace1.size() != grace2.size()) {
            return false;
        }
        for (size_t i = 0; i < grace1.size(); ++i) {
            if (!check_chords_for_unison(grace1[i], grace2[i])) {
                return false;
            }
        }
    }

    for (Segment *segment : ctx.segments_in_range(ctx.all_segments)) {
        if (!check_segment_for_same_voice(segment, prev_track, next_track)) {
            return false;
        }
    }

    if (!check_spanners_for_same_voice(prev_track, next_track, ctx)) {
        return false;
    }

    return true;
}

bool StaveSharingLayout::can_go_to_same_voice(track_idx_t prev_track, track_idx_t next_track, StaveSharingContext &ctx,
                                              const TrackGroup &cur_track_group, ankerl::unordered_dense::set<Note *> &local_unison_notes,
                                              const Staff *destination) {
    std::vector<Note *> potential_unison_notes;

    for (Segment *segment : ctx.segments_in_range(ctx.cr_segments)) {
        if (!same_instrument(prev_track, next_track, segment->tick(), ctx)) {
            return false;
        }
        ChordRest *cr1 = to_chord_rest(segment->effective_element(prev_track));
        ChordRest *cr2 = to_chord_rest(segment->effective_element(next_track));
        if (bool(cr1) != bool(cr2)) {
            return false;
        }

        if (!cr1) {
            continue;
        }

        if (cr1->is_chord() != cr2->is_chord()) {
            return false;
        }

        if (cr1->duration_type() != cr2->duration_type() || cr1->actual_ticks() != cr2->actual_ticks()) {
            return false;
        }

        if (!cr1->is_chord()) {
            continue;
        }

        if (!same_jianpu_numbering(prev_track, next_track, segment->tick(), ctx, destination)) {
            return false;
        }

        Chord *c1 = to_chord(cr1);
        Chord *c2 = to_chord(cr2);

        auto check_chords_for_same_voice = [&](Chord *c1, Chord *c2) {
            if (c1->duration_type() != c2->duration_type() || c1->note_type() != c2->note_type()) {
                return false;
            }

            const std::vector<Note *> &notes1 = c1->notes();
            const std::vector<Note *> &notes2 = c2->notes();
            if (notes1.size() != notes2.size()) {
                return false;
            }

            for (size_t i = 0; i < notes1.size(); ++i) {
                Note *n1 = notes1[i];
                Note *n2 = notes2[i];

                if (n2->pitch() > n1->pitch()) {
                    return false;
                }

                if (!n2->is_exact_unison(n1)) {
                    if (n2->pitch() == n1->pitch() || muse::contains(local_unison_notes, n1)) {
                        return false;
                    }

                    continue;
                }

                if (!check_note_spanners_for_unison(n1, n2, ctx)) {
                    return false;
                }

                for (track_idx_t track : cur_track_group) {
                    if (track == prev_track) {
                        continue;
                    }
                    if (ChordRest *cr = to_chord_rest(segment->effective_element(track)); cr && cr->is_chord()) {
                        for (Note *n : to_chord(cr)->notes()) {
                            if (!n2->is_exact_unison(n)) {
                                return false;
                            }
                        }
                    }
                }

                potential_unison_notes.push_back(n2);
            }

            return true;
        };

        if (!check_chords_for_same_voice(c1, c2)) {
            return false;
        }

        const std::vector<Chord *> &grace1 = c1->grace_notes();
        const std::vector<Chord *> &grace2 = c2->grace_notes();
        if (grace1.size() != grace2.size()) {
            return false;
        }
        for (size_t i = 0; i < grace1.size(); ++i) {
            if (!check_chords_for_same_voice(grace1[i], grace2[i])) {
                return false;
            }
        }

        if (!check_articulations_for_same_voice(c1, c2)) {
            return false;
        }
    }

    for (Segment *segment : ctx.segments_in_range(ctx.all_segments)) {
        if (!check_segment_for_same_voice(segment, prev_track, next_track)) {
            return false;
        }
    }

    if (!check_spanners_for_same_voice(prev_track, next_track, ctx)) {
        return false;
    }

    for (Note *unison_note : potential_unison_notes) {
        local_unison_notes.insert(unison_note);
    }

    return true;
}

bool StaveSharingLayout::check_segment_for_same_voice(Segment *segment, track_idx_t prev_track, track_idx_t next_track) {
    if (segment->header()) {
        return true;
    }

    if (!check_clef_key_time_sig_for_same_voice(segment, prev_track, next_track)) {
        return false;
    }

    if (segment->is_breath_type() &&
        !check_items_for_same_voice(segment, prev_track, next_track, [](const EngravingItem *item1, const EngravingItem *item2) {
            return to_breath(item1)->sym_id() == to_breath(item2)->sym_id();
        })) {
        return false;
    }

    if (!check_segment_annotations_for_same_voice(segment, prev_track, next_track)) {
        return false;
    }

    return true;
}

bool StaveSharingLayout::check_clef_key_time_sig_for_same_voice(Segment *segment, track_idx_t prev_track, track_idx_t next_track) {
    prev_track = track_zero_voice(prev_track);
    next_track = track_zero_voice(next_track);

    if (segment->is_clef_type() &&
        !check_items_for_same_voice(segment, prev_track, next_track, [](const EngravingItem *item1, const EngravingItem *item2) {
            return to_clef(item1)->clef_type() == to_clef(item2)->clef_type();
        })) {
        return false;
    }

    if (segment->is_time_sig_type() &&
        !check_items_for_same_voice(segment, prev_track, next_track, [](const EngravingItem *item1, const EngravingItem *item2) {
            IF_ASSERT_FAILED(item1->is_time_sig() && item2->is_time_sig()) { return false; }
            return *to_time_sig(item1) == *to_time_sig(item2);
        })) {
        return false;
    }

    if (segment->is_key_sig_type() &&
        !check_items_for_same_voice(segment, prev_track, next_track, [](const EngravingItem *item1, const EngravingItem *item2) {
            IF_ASSERT_FAILED(item1->is_key_sig() && item2->is_key_sig()) { return false; }
            return *to_key_sig(item1) == *to_key_sig(item2);
        })) {
        return false;
    }

    return true;
}

bool StaveSharingLayout::seg_has_local_key_sig(const Segment *segment, const SharedPart *shared_part) {
    // Key signatures applying to all staves are added to the shared staves at the same time as the origin staves,
    // so only key signatures which differ between origin staves need copying to the shared staves
    const KeySig *first_key_sig = nullptr;
    for (const Part *origin_part : shared_part->origin_parts()) {
        for (const Staff *staff : origin_part->staves()) {
            if (staff->is_drum_staff(segment->tick())) {
                continue;
            }

            const EngravingItem *item = segment->effective_element(staff->idx() * ::mu::engraving::voices);
            if (!item || !item->is_key_sig()) {
                return true;
            }

            const KeySig *key_sig = to_key_sig(item);
            if (key_sig->for_instrument_change()) {
                return true;
            }

            if (!first_key_sig) {
                first_key_sig = key_sig;
            } else if (!(*first_key_sig == *key_sig)) {
                return true;
            }
        }
    }

    return false;
}

bool StaveSharingLayout::check_segment_annotations_for_same_voice(Segment *segment, track_idx_t prev_track, track_idx_t next_track) {
    gtl::btree_multimap<ElementType, EngravingItem *> annotations_on_prev_track;
    gtl::btree_multimap<ElementType, EngravingItem *> annotations_on_next_track;

    for (EngravingItem *annotation : segment->rendered_annotations()) {
        if (annotation->is_instrument_change()) {
            // Instrument change labels should not prevent combining
            continue;
        }

        if (annotation->track() == prev_track) {
            annotations_on_prev_track.insert({annotation->type(), annotation});
        } else if (annotation->track() == next_track) {
            annotations_on_next_track.insert({annotation->type(), annotation});
        }
    }

    if (annotations_on_prev_track.size() != annotations_on_next_track.size()) {
        return false;
    }

    for (auto [type, item] : annotations_on_prev_track) {
        auto range = annotations_on_next_track.equal_range(type);
        if (range.first == range.second) {
            // No annotation of this type exists in nextTrack
            return false;
        }

        if (muse::contains(::mu::engraving::textbase_types, type)) {
            EngravingItem *matching_annotation = nullptr;
            for (auto i = range.first; i != range.second; ++i) {
                EngravingItem *next_item = i->second;
                if (next_item->type() == type && to_text_base(item)->xml_text() == to_text_base(next_item)->xml_text()) {
                    matching_annotation = next_item;
                    break;
                }
            }

            if (!matching_annotation) {
                return false;
            }
        }

        // TODO: other types will probably need other checks
    }

    return true;
}

bool StaveSharingLayout::check_items_for_same_voice(Segment *segment, track_idx_t prev_track, track_idx_t next_track,
                                                    const std::function<bool(const EngravingItem *, const EngravingItem *)> &is_equal) {
    EngravingItem *item1 = segment->effective_element(prev_track);
    EngravingItem *item2 = segment->effective_element(next_track);

    if (bool(item1) != bool(item2)) {
        return false;
    }

    if (item1 && !is_equal(item1, item2)) {
        return false;
    }

    return true;
}

bool StaveSharingLayout::check_note_spanners_for_unison(const Note *note1, const Note *note2, const StaveSharingContext &ctx) {
    const Tie *tie_back1 = note1->tie_back();
    const Tie *tie_back2 = note2->tie_back();
    if (bool(tie_back1) != bool(tie_back2)) {
        return false;
    }
    if (tie_back1 && bool(tie_back1->start_element()) != bool(tie_back2->start_element())) {
        return false;
    }

    const Tie *tie_for1 = note1->tie_for();
    const Tie *tie_for2 = note2->tie_for();
    if (bool(tie_for1) != bool(tie_for2)) {
        return false;
    }
    if (tie_for1 && bool(tie_for1->end_element()) != bool(tie_for2->end_element())) {
        return false;
    }

    // A note owns one link in each direction. Across independently fitted
    // intervals we cannot guarantee that the other endpoints will share it.
    const auto can_merge_endpoints = [&](Note *a, Note *b) {
        if (!a || !b) return a == b;
        return a->tick() == b->tick() && a->tick() >= ctx.s_tick && a->tick() < ctx.e_tick && a->track() == note1->track() &&
               b->track() == note2->track() && a->is_exact_unison(b);
    };
    if ((tie_back1 && !can_merge_endpoints(tie_back1->start_note(), tie_back2->start_note())) ||
        (tie_for1 && !can_merge_endpoints(tie_for1->end_note(), tie_for2->end_note()))) {
        return false;
    }

    const std::vector<Spanner *> &spanner_back1 = note1->spanner_back();
    const std::vector<Spanner *> &spanner_back2 = note2->spanner_back();
    if (spanner_back1.size() != spanner_back2.size()) {
        return false;
    }

    for (Spanner *sp1 : spanner_back1) {
        auto i = std::find_if(spanner_back2.begin(), spanner_back2.end(), [sp1](Spanner *sp2) { return sp2->type() == sp1->type(); });
        if (i == spanner_back2.end()) {
            return false;
        }

        Spanner *sp2 = *i;
        if (bool(sp1->start_element()) != bool(sp2->start_element())) {
            return false;
        }
    }

    const std::vector<Spanner *> &spanner_for1 = note1->spanner_for();
    const std::vector<Spanner *> &spanner_for2 = note2->spanner_for();
    if (spanner_for1.size() != spanner_for2.size()) {
        return false;
    }

    for (Spanner *sp1 : spanner_for1) {
        auto i = std::find_if(spanner_for2.begin(), spanner_for2.end(), [sp1](Spanner *sp2) { return sp2->type() == sp1->type(); });
        if (i == spanner_for2.end()) {
            return false;
        }

        Spanner *sp2 = *i;
        if (bool(sp1->end_element()) != bool(sp2->end_element())) {
            return false;
        }
    }

    return true;
}

bool StaveSharingLayout::check_spanners_for_same_voice(track_idx_t prev_track, track_idx_t next_track, StaveSharingContext &ctx) {
    gtl::btree_multimap<ElementType, Spanner *> spanners_on_prev_track;
    gtl::btree_multimap<ElementType, Spanner *> spanners_on_next_track;

    for (Spanner *spanner : ctx.overlapping_spanners) {
        if (!ctx.cur_shared_part->authored_states().empty() && (spanner->tick() >= ctx.e_tick || spanner->tick2() < ctx.s_tick)) continue;
        if (spanner->track() == prev_track) {
            spanners_on_prev_track.insert({spanner->type(), spanner});
        } else if (spanner->track() == next_track) {
            spanners_on_next_track.insert({spanner->type(), spanner});
        }
    }

    if (spanners_on_prev_track.size() != spanners_on_next_track.size()) {
        return false;
    }

    for (auto [type, spanner] : spanners_on_prev_track) {
        auto range = spanners_on_next_track.equal_range(type);
        if (range.first == range.second) {
            // No spanner of this type exists in nextTrack
            return false;
        }

        Spanner *matching_spanner = nullptr;
        for (auto i = range.first; i != range.second; ++i) {
            Spanner *next_spanner = i->second;
            if (next_spanner->tick() == spanner->tick() && next_spanner->ticks() == spanner->ticks()) {
                matching_spanner = next_spanner;
                break;
            }
        }

        if (!matching_spanner) {
            return false;
        }
    }

    return true;
}

bool StaveSharingLayout::check_articulations_for_same_voice(Chord *chord1, Chord *chord2) {
    const std::vector<Articulation *> &articulations1 = chord1->articulations();
    const std::vector<Articulation *> &articulations2 = chord2->articulations();

    if (articulations1.size() != articulations2.size()) {
        return false;
    }

    for (size_t i = 0; i < articulations1.size(); ++i) {
        Articulation *art1 = articulations1[i];
        Articulation *art2 = articulations2[i];
        if (art1->subtype() != art2->subtype()) {
            return false;
        }
    }

    return true;
}

bool StaveSharingLayout::can_go_to_same_stave(track_idx_t prev_track, track_idx_t next_track, bool allow_voice_crossing,
                                              StaveSharingContext &ctx) {

    for (Segment *segment : ctx.segments_in_range(ctx.all_segments)) {
        if (!same_instrument(prev_track, next_track, segment->tick(), ctx)) {
            return false;
        }

        if (segment->header()) {
            continue;
        }

        if (!check_clef_key_time_sig_for_same_voice(segment, prev_track, next_track)) {
            return false;
        }

        if (!segment->is_chord_rest_type() || allow_voice_crossing) {
            continue;
        }
        ChordRest *cr1 = to_chord_rest(segment->effective_element(prev_track));
        ChordRest *cr2 = to_chord_rest(segment->effective_element(next_track));
        if (cr1 && cr2 && cr1->is_chord() && cr2->is_chord()) {
            Chord *c1 = to_chord(cr1);
            Chord *c2 = to_chord(cr2);
            if (c2->up_note()->pitch() > c1->down_note()->pitch()) {
                return false;
            }
        }
    }

    return true;
}

void StaveSharingLayout::update_notation(StaveSharingContext &ctx) {
    compute_segments_to_update(ctx);
    ctx.collect_overlapping_spanners();

    disconnect_all(ctx);

    make_shared_notation(ctx);

    cleanup(ctx);
}

void StaveSharingLayout::compute_segments_to_update(StaveSharingContext &ctx) {
    if (!ctx.update_for_last_added || ctx.track_map_changed) {
        ctx.segments_to_update = ctx.all_segments;
        ctx.cr_segments_to_update = ctx.cr_segments;
        return;
    }

    ctx.segments_to_update.clear();
    ctx.cr_segments_to_update.clear();
    Measure *last_measure = ctx.cr_segments.back()->measure();
    for (Segment *rendered = last_measure->rendered_first(); rendered; rendered = rendered->rendered_next()) {
        Segment &seg = *rendered;
        ctx.segments_to_update.push_back(&seg);
        if (seg.is_chord_rest_type()) {
            ctx.cr_segments_to_update.push_back(&seg);
        }
    }
}

void StaveSharingLayout::disconnect_all(StaveSharingContext &ctx) {
    SharedPart *p = ctx.cur_shared_part;

    const TrackRange range = p->track_range();

    ctx.old_stave_sharing_labels.clear();
    ctx.updated_stave_sharing_labels.clear();
    ctx.obsolete_changes.clear();

    for (Segment *seg : ctx.segments_to_update) {
        for (auto *source : seg->annotations()) {
            if (source->system_flag() || source->track() < range.start_track || source->track() >= range.end_track) continue;
            if (muse::contains(seg->ldata()->suppressed_annotations, source)) continue;
            seg->mutldata()->suppressed_annotations.push_back(source);
            auto *copy = source->clone();
            add_shared_output(copy);
            ctx.score->register_layout_copy(*source, *copy);
        }
        if (!seg->header()) {
            for (track_idx_t track = range.start_track; track < range.end_track; ++track) {
                auto *source = seg->element(track);
                if (!source || shared_slot(seg, track) || (source->is_time_sig() && !to_time_sig(source)->is_local())) continue;
                auto *copy = source->clone();
                copy->set_ownership_parent(seg);
                add_shared_output(copy);
                ctx.score->register_layout_copy(*source, *copy);
            }
        }
        for (EngravingItem *item : seg->rendered_annotations()) {
            if (!item->system_flag() && item->track() >= range.start_track && item->track() < range.end_track) {
                EngravingItem::disconnect_all_origin_items(item);

                if (item->is_stave_sharing_label()) {
                    ctx.old_stave_sharing_labels.push_back(to_stave_sharing_label(item));
                }
                if (item->is_layout_copy() && item->is_stave_sharing_change()) {
                    ctx.obsolete_changes.push_back(to_stave_sharing_change(item));
                }
            }
        }

        if (seg->header()) {
            continue;
        }

        if (seg->is_type(SegmentType::BREATH | SegmentType::CLEF | SegmentType::TIME_SIG | SegmentType::KEY_SIG)) {
            for (track_idx_t track = range.start_track; track < range.end_track; ++track) {
                EngravingItem *item = seg->effective_element(track);
                if (!item || (item->is_time_sig() && !to_time_sig(item)->is_local())) {
                    continue;
                }
                EngravingItem::disconnect_all_origin_items(item);
            }
            continue;
        }

        if (!seg->is_chord_rest_type()) {
            continue;
        }

        for (track_idx_t track = range.start_track; track < range.end_track; ++track) {
            ChordRest *cr = to_chord_rest(seg->effective_element(track));
            if (!cr) {
                continue;
            }

            for (DurationElement *de = cr; de;) {
                Tuplet *tuplet = de->tuplet();
                if (!tuplet) {
                    break;
                }

                if (de == tuplet->elements().front()) {
                    EngravingItem::disconnect_all_origin_items(tuplet);
                }
                de = tuplet;
            }

            if (cr->is_rest()) {
                EngravingItem::disconnect_all_origin_items(cr);
            }

            if (cr->is_chord()) {
                auto disconnect_chord = [&](Chord *c) {
                    for (Note *note : c->notes()) {
                        EngravingItem::disconnect_all_origin_items(note);

                        if (Tie *tie_for = note->tie_for();
                            tie_for && !tie_for->end_note()) { // if it does have endNote it will be disconnected when we process that note
                            EngravingItem::disconnect_all_origin_items(tie_for);
                        }

                        for (Spanner *spanner_for : note->spanner_for()) {
                            if (!spanner_for->end_element()) {
                                EngravingItem::disconnect_all_origin_items(
                                    spanner_for); // if it does have endElement it will be disconnected when we process that note
                            }
                        }

                        if (Tie *tie_back = note->tie_back()) {
                            EngravingItem::disconnect_all_origin_items(tie_back);
                        }

                        for (Spanner *spanner_back : note->spanner_back()) {
                            EngravingItem::disconnect_all_origin_items(spanner_back);
                        }
                    }

                    for (Articulation *art : c->articulations()) {
                        EngravingItem::disconnect_all_origin_items(art);
                    }
                };

                Chord *c = to_chord(cr);
                disconnect_chord(c);

                for (Chord *grace : c->grace_notes()) {
                    disconnect_chord(grace);
                }
            }
        }
    }

    ctx.collect_overlapping_spanners();
    for (Spanner *spanner : ctx.overlapping_spanners) {
        if (!spanner->is_layout_copy() && !spanner->system_flag() && spanner->track() >= range.start_track &&
            spanner->track() < range.end_track) {
            auto &suppressed = output(spanner).score_output().suppressed_shared_spanners;
            if (std::find(suppressed.begin(), suppressed.end(), spanner) == suppressed.end()) suppressed.push_back(spanner);
            for (auto *segment : spanner->spanner_segments()) segment->move_to_system(nullptr);
        }
        if (spanner->is_layout_copy() && !spanner->system_flag() && spanner->track() >= range.start_track &&
            spanner->track() < range.end_track) {
            EngravingItem::disconnect_all_origin_items(spanner);
        }
    }
}

void StaveSharingLayout::make_shared_notation(StaveSharingContext &ctx) {
    make_shared_clef_key_time_sigs(ctx);
    make_shared_chord_rests(ctx);
    make_shared_breaths(ctx);
    make_shared_annotations(ctx);
    ctx.collect_overlapping_spanners();
    make_shared_spanners(ctx);
    make_stave_sharing_labels(ctx);
    make_stave_sharing_changes(ctx);
}

EngravingItem *StaveSharingLayout::make_shared_item(EngravingItem *origin_item, EngravingItem *possible_shared_item,
                                                    track_idx_t shared_track, EngravingObject *shared_parent,
                                                    const std::function<bool(const EngravingItem *, const EngravingItem *)> &is_equal) {

    EngravingItem *shared_item = nullptr;
    if (possible_shared_item) {
        if (is_equal(origin_item, possible_shared_item)) {
            shared_item = possible_shared_item;
        } else {
            remove_shared_output(possible_shared_item);
        }
    }

    if (!shared_item) {
        shared_item = origin_item->clone();
        shared_item->set_track(shared_track);
        shared_item->set_ownership_parent(shared_parent);
        add_shared_output(shared_item);
    }

    connect_output(shared_item, origin_item);

    return shared_item;
}

void StaveSharingLayout::make_shared_clef_key_time_sigs(StaveSharingContext &ctx) {
    // This function copies local time signatures, key signatures and clefs to shared staves
    // These items only apply to a single staff. Time signatures and key signatures applying to all staves
    // were added at the same time as the origin items

    for (Segment *seg : ctx.segments_to_update) {
        if (seg->header() || (!seg->is_clef_type() && !seg->is_key_sig_type() && !seg->is_time_sig_type())) {
            continue;
        }
        if (seg->is_key_sig_type() && !seg_has_local_key_sig(seg, ctx.cur_shared_part)) {
            continue;
        }
        for (const auto &[origin_track, shared_track] : ctx.map_at_tick(seg->tick())) {
            if (track2voice(shared_track) != 0) {
                // Clefs, key, and time signatures can only appear in the 1st voice
                // At this point, any items in subsequent voices should match what is in voice 1
                continue;
            }

            if (seg->is_clef_type()) {
                Clef *origin_clef = to_clef(seg->effective_element(track_zero_voice(origin_track)));
                if (origin_clef) {
                    make_shared_item(origin_clef, shared_slot(seg, shared_track), shared_track, seg,
                                     [](const EngravingItem *origin, const EngravingItem *shared) {
                                         return to_clef(shared)->clef_type() == to_clef(origin)->clef_type();
                                     });
                }
            }

            if (seg->is_time_sig_type()) {
                TimeSig *origin_time_sig = to_time_sig(seg->effective_element(track_zero_voice(origin_track)));
                if (origin_time_sig && origin_time_sig->is_local()) {
                    make_shared_item(origin_time_sig, shared_slot(seg, shared_track), shared_track, seg,
                                     [](const EngravingItem *origin, const EngravingItem *shared) {
                                         IF_ASSERT_FAILED(origin->is_time_sig() && shared->is_time_sig()) { return false; }
                                         return *to_time_sig(shared) == *to_time_sig(origin);
                                     });
                }
            }

            if (seg->is_key_sig_type()) {
                KeySig *origin_key_sig = to_key_sig(seg->effective_element(track_zero_voice(origin_track)));
                if (origin_key_sig) {
                    make_shared_item(origin_key_sig, shared_slot(seg, shared_track), shared_track, seg,
                                     [](const EngravingItem *origin, const EngravingItem *shared) {
                                         IF_ASSERT_FAILED(origin->is_key_sig() && shared->is_key_sig()) { return false; }
                                         return *to_key_sig(shared) == *to_key_sig(origin);
                                     });
                }
            }
        }
    }
}

void StaveSharingLayout::make_shared_chord_rests(StaveSharingContext &ctx) {
    Score *score = ctx.score;

    ctx.shared_unison_notes.clear();

    for (Segment *seg : ctx.cr_segments_to_update) {
        for (const auto &[origin_track, shared_track] : ctx.map_at_tick(seg->tick())) {
            ChordRest *origin_cr = to_chord_rest(seg->effective_element(origin_track));
            if (!origin_cr) {
                continue;
            }

            ChordRest *shared_cr = to_chord_rest(shared_slot(seg, shared_track));
            if (shared_cr) {
                bool same_type = shared_cr->type() == origin_cr->type();
                bool same_duration =
                    shared_cr->duration_type() == origin_cr->duration_type() && shared_cr->actual_ticks() == origin_cr->actual_ticks();
                if (!(same_type && same_duration)) {
                    remove_shared_output(shared_cr);
                    shared_cr = nullptr;
                }
            }

            bool bar_rest_in_sec_voice =
                origin_cr->is_rest() && to_rest(origin_cr)->is_full_measure_rest() && track2voice(shared_track) != 0;
            if (bar_rest_in_sec_voice) {
                continue;
            }

            if (!shared_cr) {
                shared_cr = to_chord_rest(origin_cr->clone());
                shared_cr->set_track(shared_track);
                shared_cr->set_ownership_parent(seg);
                add_shared_output(shared_cr);
            }
            score->register_layout_copy(*origin_cr, *shared_cr);

            DurationElement *shared_de = shared_cr;
            for (DurationElement *origin_de = origin_cr; origin_de;) {
                Tuplet *origin_tuplet = origin_de->tuplet();
                if (!origin_tuplet) {
                    break;
                }

                Tuplet *shared_tuplet = nullptr;
                if (origin_de == origin_tuplet->elements().front()) {
                    shared_tuplet = shared_de->tuplet();
                    if (!shared_tuplet) {
                        shared_tuplet = to_tuplet(origin_tuplet->clone());
                        shared_tuplet->set_track(shared_track);
                        shared_tuplet->set_ownership_parent(origin_tuplet->measure());
                        add_shared_output(shared_tuplet);
                    }
                } else {
                    shared_tuplet = to_tuplet(origin_tuplet->shared_item());
                }

                IF_ASSERT_FAILED(shared_tuplet) { break; }

                connect_output(shared_tuplet, origin_tuplet);

                // Layout reruns this for already shared elements; a second copy of a
                // nested tuplet would be autoplaced clear of the first.
                if (!shared_tuplet->contains(shared_de)) shared_tuplet->add(shared_de);

                origin_de = origin_tuplet;
                shared_de = shared_tuplet;
            }

            if (origin_cr->is_rest()) {
                connect_output(shared_cr, origin_cr);
            }

            if (!origin_cr->is_chord()) {
                continue;
            }

            auto make_shared_notes = [&](Chord *origin_chord, Chord *shared_chord) {
                Note *origin_note = origin_chord->up_note();
                Note *shared_note = nullptr;
                for (Note *n : shared_chord->notes()) {
                    if (n->is_exact_unison(origin_note)) {
                        shared_note = n;
                        break;
                    }
                }

                if (!shared_note) {
                    shared_note = origin_note->clone();
                    shared_note->set_track(shared_track);
                    shared_note->set_ownership_parent(shared_chord);
                    add_shared_output(shared_note);
                }

                connect_output(shared_note, origin_note);

                if (shared_note->origin_items().size() > 1) {
                    ctx.shared_unison_notes.insert(shared_note);
                }

                make_shared_ties_and_note_spanners(origin_note, shared_note);

                make_shared_articulations(origin_chord, shared_chord);
            };

            Chord *origin_chord = to_chord(origin_cr);
            Chord *shared_chord = to_chord(shared_cr);

            make_shared_notes(origin_chord, shared_chord);

            const std::vector<Chord *> &origin_graces = origin_chord->grace_notes();
            const std::vector<Chord *> &shared_graces = shared_chord->grace_notes();
            for (size_t i = 0; i < origin_graces.size(); ++i) {
                Chord *origin_grace = origin_graces[i];
                Chord *shared_grace = i < shared_graces.size() ? shared_graces[i] : nullptr;
                if (!shared_grace) {
                    shared_grace = to_chord(origin_grace->clone());
                    shared_grace->set_track(shared_track);
                    shared_grace->set_ownership_parent(shared_chord);
                    add_shared_output(shared_grace);
                }
                score->register_layout_copy(*origin_grace, *shared_grace);

                make_shared_notes(origin_grace, shared_grace);
            }
        }
    }
}

void StaveSharingLayout::make_shared_breaths(StaveSharingContext &ctx) {
    for (Segment *seg : ctx.segments_to_update) {
        if (!seg->is_breath_type()) continue;
        const SharedTrackMap &track_map = ctx.map_at_tick(seg->tick());
        if (track_map.empty()) continue;
        const auto start_origin_track = track_map.begin()->first;
        const auto end_origin_track = track_map.rbegin()->first;
        std::vector<EngravingItem *> shared_breaths;
        for (const auto &[origin_track, shared_track] : track_map) {
            Breath *origin_breath = to_breath(seg->effective_element(origin_track));
            if (!origin_breath) {
                continue;
            }

            EngravingItem *shared_breath = make_shared_item(origin_breath, shared_slot(seg, shared_track), shared_track, seg,
                                                            [](const EngravingItem *origin, const EngravingItem *shared) {
                                                                return to_breath(shared)->sym_id() == to_breath(origin)->sym_id();
                                                            });
            shared_breaths.push_back(shared_breath);
        }
        manage_voice_property_and_track_for_shared_items(shared_breaths, start_origin_track, end_origin_track, track_map);
    }
}

void StaveSharingLayout::make_shared_articulations(Chord *origin_chord, Chord *shared_chord) {

    for (Articulation *origin_art : origin_chord->articulations()) {
        Articulation *shared_art = nullptr;
        for (Articulation *possible_shared_art : shared_chord->articulations()) {
            if (possible_shared_art->subtype() == origin_art->subtype()) {
                shared_art = possible_shared_art;
                break;
            }
        }

        if (!shared_art) {
            shared_art = origin_art->clone();
            shared_art->set_track(shared_chord->track());
            shared_art->set_ownership_parent(shared_chord);

            add_shared_output(shared_art);
        }

        connect_output(shared_art, origin_art);
    }
}

void StaveSharingLayout::make_shared_ties_and_note_spanners(Note *origin_note, Note *shared_note) {

    Tie *tie_back = origin_note->tie_back();
    if (tie_back) {
        Note *shared_start_note = tie_back->start_note() ? to_note(tie_back->start_note()->shared_item()) : nullptr;
        Tie *shared_tie_back = shared_note->tie_back();
        if (shared_tie_back && shared_tie_back->start_note() != shared_start_note && shared_tie_back->origin_items().empty()) {
            remove_shared_output(shared_tie_back);
            shared_tie_back = nullptr;
        }
        if (!shared_tie_back) {
            auto *previous = shared_start_note ? shared_start_note->tie_for() : nullptr;
            if (previous && previous->origin_items().empty()) {
                remove_shared_output(previous);
                previous = nullptr;
            }
            // A live tie already owns the single forward link on this Note.
            if (!previous) {
                shared_tie_back = to_tie(tie_back->clone());
                shared_tie_back->set_note_span(shared_start_note, shared_note);
                add_shared_output(shared_tie_back);
            }
        }

        if (shared_tie_back && shared_tie_back->start_note() == shared_start_note) connect_output(shared_tie_back, tie_back);
    }

    Tie *tie_for = origin_note->tie_for();
    if (tie_for && !tie_for->end_note()) { // If it does have an end note we add it when we process the end note
        Tie *shared_tie_for = shared_note->tie_for();
        if (!shared_tie_for) {
            shared_tie_for = to_tie(tie_for->clone());
            shared_tie_for->set_note_span(shared_note, nullptr);

            add_shared_output(shared_tie_for);
        }

        connect_output(shared_tie_for, tie_for);
    }

    const std::vector<Spanner *> &origin_spanner_back = origin_note->spanner_back();
    const std::vector<Spanner *> &shared_spanner_back = shared_note->spanner_back();

    for (Spanner *spanner : origin_spanner_back) {
        Spanner *shared_spanner = nullptr;
        for (Spanner *sp : shared_spanner_back) {
            if (sp && sp->type() == spanner->type()) {
                shared_spanner = sp;
                break;
            }
        }

        if (!shared_spanner) {
            shared_spanner = to_spanner(spanner->clone());
            Note *start_note = spanner->start_element() ? to_note(spanner->start_element()->shared_item()) : nullptr;
            shared_spanner->set_note_span(start_note, shared_note);

            add_shared_output(shared_spanner);
        }

        connect_output(shared_spanner, spanner);
    }

    for (Spanner *spanner : origin_note->spanner_for()) {
        if (spanner->end_element()) { // If it does have an end note we add it when we process the end note
            continue;
        }

        Spanner *shared_spanner = nullptr;
        for (Spanner *sp : shared_note->spanner_for()) {
            if (sp->type() == spanner->type()) {
                shared_spanner = sp;
                break;
            }
        }

        if (!shared_spanner) {
            shared_spanner = to_spanner(spanner->clone());
            shared_spanner->set_track(shared_note->track());
            shared_spanner->set_start_element(shared_note);

            add_shared_output(shared_spanner);
        }

        connect_output(shared_spanner, spanner);
    }
}

void StaveSharingLayout::make_shared_annotations(StaveSharingContext &ctx) {
    for (Segment *seg : ctx.segments_to_update) {
        const SharedTrackMap &track_map = ctx.map_at_tick(seg->tick());
        if (track_map.empty()) continue;
        const auto start_origin_track = track_map.begin()->first;
        const auto end_origin_track = track_map.rbegin()->first;
        std::vector<EngravingItem *> shared_annotations;
        std::vector<EngravingItem *> annotations = seg->rendered_annotations(); // Copy because we are about to add
        for (EngravingItem *origin_item : annotations) {
            track_idx_t origin_track = origin_item->track();
            if (origin_track < start_origin_track || origin_track > end_origin_track) {
                continue;
            }

            if (!track_map.contains(origin_track) || origin_item->is_stave_sharing_label() || origin_item->is_stave_sharing_change())
                continue;

            if (origin_item->is_fermata()) {
                EngravingItem *fermata_origin_base_item = seg->effective_element(origin_track);
                if (fermata_origin_base_item && fermata_origin_base_item->is_chord()) {
                    fermata_origin_base_item = to_chord(fermata_origin_base_item)->up_note();
                }
                if (fermata_origin_base_item && !(fermata_origin_base_item->shared_item() || fermata_origin_base_item->is_bar_line())) {
                    // chordrests can't be linked, need to scan notes & check
                    continue;
                }
            }

            track_idx_t shared_track = track_map.at(origin_track);
            EngravingItem *shared_item = nullptr;
            for (EngravingItem *item : seg->rendered_annotations()) {
                if (!item->is_layout_copy() || item->track() != shared_track || item->type() != origin_item->type()) {
                    continue;
                }

                if (!item->is_instrument_change() && item->is_text_base() &&
                    to_text_base(item)->xml_text() != to_text_base(origin_item)->xml_text()) {
                    continue;
                }

                shared_item = item;
                break;
            }

            if (!shared_item) {
                shared_item = origin_item->clone();
                shared_item->set_track(shared_track);
                shared_item->set_ownership_parent(seg);
                add_shared_output(shared_item);
            }

            connect_output(shared_item, origin_item);

            shared_annotations.push_back(shared_item);

            if (shared_item->is_instrument_change()) {
                InstrumentChange *origin_change = to_instrument_change(origin_item);
                InstrumentChange *shared_change = to_instrument_change(shared_item);
                String text = format_instrument_change_lable(origin_change, shared_change, ctx);

                shared_change->set_xml_text(text);
            }
        }
        manage_voice_property_and_track_for_shared_items(shared_annotations, start_origin_track, end_origin_track, track_map);
    }
}

void StaveSharingLayout::make_shared_spanners(StaveSharingContext &ctx) {
    const auto overlapping_spanners = ctx.overlapping_spanners;
    for (Spanner *spanner : overlapping_spanners) {
        if (spanner->is_layout_copy() || spanner->system_flag() || spanner->tick() >= ctx.e_tick) continue;
        const SharedTrackMap &track_map = ctx.map_at_tick(spanner->tick());
        if (track_map.empty()) continue;
        const auto start_origin_track = track_map.begin()->first;
        const auto end_origin_track = track_map.rbegin()->first;
        if (spanner->track() < start_origin_track || spanner->track() > end_origin_track) {
            continue;
        }

        track_idx_t origin_track = spanner->track();
        if (!track_map.contains(origin_track)) continue;

        track_idx_t shared_track = track_map.at(origin_track);
        const auto &end_map = ctx.map_at_tick(spanner->tick2());
        const auto map_end_track = [&](const Spanner &span) {
            const auto end = end_map.find(span.effective_track2());
            return end != end_map.end() ? end->second : shared_track;
        };
        const auto shared_end_track = spanner->can_be_cross_staff() ? map_end_track(*spanner) : shared_track;

        Spanner *shared_spanner = nullptr;
        for (Spanner *possible_shared_spanner : ctx.overlapping_spanners) {
            if (possible_shared_spanner->is_layout_copy() && possible_shared_spanner->track() == shared_track &&
                possible_shared_spanner->type() == spanner->type() && possible_shared_spanner->tick() == spanner->tick() &&
                possible_shared_spanner->ticks() == spanner->ticks() &&
                (possible_shared_spanner->effective_track2() == shared_end_track || possible_shared_spanner->origin_items().empty())) {
                shared_spanner = possible_shared_spanner;
                break;
            }
        }

        if (!shared_spanner) {
            shared_spanner = to_spanner(spanner->clone());
            shared_spanner->set_track(shared_track);
            shared_spanner->set_track2(shared_end_track);
            shared_spanner->set_tick(spanner->tick());
            shared_spanner->set_ticks(spanner->ticks());
            shared_spanner->set_start_element(nullptr);
            shared_spanner->set_end_element(nullptr);
            shared_spanner->reset();

            add_shared_output(shared_spanner);
            ctx.overlapping_spanners.push_back(shared_spanner);
        } else if (shared_spanner->effective_track2() != shared_end_track) {
            shared_spanner->set_track2(shared_end_track);
            shared_spanner->set_end_element(nullptr);
        }

        connect_output(shared_spanner, spanner);

        const auto old_assignment =
            shared_spanner->is_hairpin() ? to_hairpin(shared_spanner)->voice_assignment() : VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
        EngravingItem *const shared_spanners[]{shared_spanner};
        manage_voice_property_and_track_for_shared_items(shared_spanners, start_origin_track, end_origin_track, track_map);
        // Staff-anchored lines have one track. Preserve the line at its start staff,
        // using staff-wide voice assignment where the line supports it.
        if (!spanner->can_be_cross_staff() && shared_spanner->has_voice_assignment_properties() &&
            std::ranges::any_of(shared_spanner->origin_items(),
                                [&](const auto *origin) { return map_end_track(*to_spanner(origin)) != shared_track; })) {
            shared_spanner->set_property(Pid::VOICE_ASSIGNMENT, VoiceAssignment::ALL_VOICE_IN_STAFF);
        }
        if (shared_spanner->is_hairpin() && to_hairpin(shared_spanner)->voice_assignment() != old_assignment) {
            for (auto *segment : shared_spanner->spanner_segments()) {
                if (!segment->system()) continue;
                output(shared_spanner).retire_spanner_borrowers(*segment);
                segment->move_to_system(nullptr);
            }
        }
    }
}

void StaveSharingLayout::make_stave_sharing_labels(StaveSharingContext &ctx) {
    for (Note *unison_note : ctx.shared_unison_notes) {
        const SharedTrackMap &track_map = ctx.map_at_tick(unison_note->tick());
        bool needs_restate_on_new_system = false;
        if (!unison_note_needs_label(unison_note, needs_restate_on_new_system, ctx)) {
            continue;
        }

        String text = format_unison_label(unison_note, track_map, needs_restate_on_new_system, ctx);
        Segment *segment = unison_note->chord()->segment();

        StaveSharingLabel *label = nullptr;
        for (EngravingItem *item : segment->rendered_annotations()) {
            if (item->is_layout_copy() && item->is_stave_sharing_label() && item->track() == unison_note->track() &&
                to_stave_sharing_label(item)->xml_text() == text) {
                label = to_stave_sharing_label(item);
                break;
            }
        }

        if (!label) {
            label = Factory::create_stave_sharing_label(segment);
            label->set_ownership_parent(segment);
            label->set_track(unison_note->track());
            label->set_xml_text(text);

            add_shared_output(label);
        }

        const TrackRange range = ctx.cur_shared_part->track_range();
        EngravingItem *const shared_labels[]{label};
        manage_voice_property_and_track_for_shared_items(shared_labels, range.start_track, range.end_track, track_map);
        ctx.updated_stave_sharing_labels.push_back(label);
    }
}

void StaveSharingLayout::make_stave_sharing_changes(StaveSharingContext &ctx) {
    if (ctx.cur_shared_part->authored_states().empty()) return;
    const track_idx_t track = ctx.cur_shared_part->track_range().start_track;
    for (auto *segment : ctx.cr_segments_to_update) {
        if (!ctx.cur_shared_part->exact_authored_state(segment->tick())) continue;

        const auto reuse = std::find_if(ctx.obsolete_changes.begin(), ctx.obsolete_changes.end(),
                                        [&](const StaveSharingChange *change) { return change->ownership_parent() == segment; });
        if (reuse != ctx.obsolete_changes.end()) {
            ctx.obsolete_changes.erase(reuse);
        } else {
            auto *change = Factory::create_stave_sharing_change(segment);
            change->set_track(track);
            add_shared_output(change);
        }
    }
}

bool StaveSharingLayout::unison_note_needs_label(Note *unison_note, bool &needs_restate_on_new_system, StaveSharingContext &ctx) {
    std::vector<track_idx_t> origin_tracks_of_this_note;
    for (EngravingItem *origin_note : unison_note->origin_items()) {
        origin_tracks_of_this_note.push_back(origin_note->track());
    }

    Note *prev_note = nullptr;
    track_idx_t cur_track = unison_note->track();
    Segment *cur_segment = unison_note->chord()->segment();
    for (Segment *seg = cur_segment->rendered_prev1(SegmentType::CHORD_REST); seg; seg = seg->rendered_prev1(SegmentType::CHORD_REST)) {
        if (EngravingItem *el = seg->effective_element(cur_track); el && el->is_chord()) {
            for (Note *note : to_chord(el)->notes()) {
                if (!note->origin_items().empty()) {
                    prev_note = note;
                    break;
                }
            }
        }
        if (prev_note) {
            break;
        }
    }

    if (!prev_note) {
        return true;
    }

    std::vector<track_idx_t> origin_tracks_of_prev_note;
    for (EngravingItem *origin_note : prev_note->origin_items()) {
        origin_tracks_of_prev_note.push_back(origin_note->track());
    }

    bool restate_on_new_system = ctx.style.style_v(Sid::UNISON_LABEL_RESTATE_ON_NEW_SYSTEM).value<AutoOnOff>() != AutoOnOff::OFF;
    needs_restate_on_new_system = restate_on_new_system &&
                                  unison_note->chord()->segment()->system() != prev_note->chord()->segment()->system() &&
                                  origin_tracks_of_prev_note == origin_tracks_of_this_note;

    return origin_tracks_of_prev_note != origin_tracks_of_this_note || needs_restate_on_new_system;
}

String StaveSharingLayout::format_unison_label(Note *unison_note, const SharedTrackMap &track_map, bool needs_restate_on_new_system,
                                               const StaveSharingContext &ctx) {
    const MStyle &style = ctx.style;

    String result;

    size_t origin_unisons_count = unison_note->origin_items().size();
    result += style.style_st(Sid::TEXT_FOR_UNISON_LABEL) + String::number(origin_unisons_count);
    bool trailing_dot = false; // TODO: style
    if (trailing_dot) {
        result += '.';
    }

    track_idx_t cur_track = unison_note->track();
    staff_idx_t cur_staff = track2staff(cur_track);

    std::vector<track_idx_t> origin_tracks;
    origin_tracks.reserve(origin_unisons_count);
    std::vector<const Instrument *> origin_instruments;
    origin_instruments.reserve(origin_unisons_count);
    std::vector<track_idx_t> tracks_mapped_to_this_stave;
    tracks_mapped_to_this_stave.reserve(origin_unisons_count);

    for (auto [origin_track, shared_track] : track_map) {
        if (track2staff(shared_track) == cur_staff) {
            tracks_mapped_to_this_stave.push_back(origin_track);
        }
        if (shared_track == cur_track) {
            origin_tracks.push_back(origin_track);
            origin_instruments.push_back(ctx.score->staff(track2staff(origin_track))->part()->instrument());
        }
    }

    bool add_parenthesis =
        needs_restate_on_new_system && ctx.style.style_v(Sid::UNISON_LABEL_RESTATE_ON_NEW_SYSTEM).value<AutoOnOff>() == AutoOnOff::AUTO;

    if (tracks_mapped_to_this_stave.size() <= origin_unisons_count) {
        if (add_parenthesis) {
            result.prepend('(');
            result.append(')');
        }
        return result;
    }

    bool trailing_dot_single;
    bool trailing_dot_multiple;
    int hyphen_limit;
    if (style.style_b(Sid::SHARED_ON_STAFF_NUMERALS_FOLLOW_INSTRUMENT_NUMERALS)) {
        trailing_dot_single = style.style_b(Sid::INSTRUMENT_NUMERALS_TRAILING_DOT_SINGLE);
        trailing_dot_multiple = style.style_b(Sid::INSTRUMENT_NUMERALS_TRAILING_DOT_MULTIPLE);
        hyphen_limit =
            style.style_b(Sid::INSTRUMENT_NUMERALS_HYPHEN_ENABLE) ? style.style_i(Sid::INSTRUMENT_NUMERALS_HYPHEN_THRESHOLD) : INT_MAX;
    } else {
        trailing_dot_single = style.style_b(Sid::SHARED_ON_STAFF_NUMERALS_TRAILING_DOT_SINGLE);
        trailing_dot_multiple = style.style_b(Sid::SHARED_ON_STAFF_NUMERALS_TRAILING_DOT_MULTIPLE);
        hyphen_limit = style.style_b(Sid::SHARED_ON_STAFF_NUMERALS_HYPHEN_ENABLE) ?
                           style.style_i(Sid::SHARED_ON_STAFF_NUMERALS_HYPHEN_THRESHOLD) :
                           INT_MAX;
    }

    String prefix =
        SystemHeaderLayout::format_shared_voice_label(origin_instruments, trailing_dot_single, trailing_dot_multiple, hyphen_limit);

    result.prepend(prefix + ' ');

    if (add_parenthesis) {
        result.prepend('(');
        result.append(')');
    }

    return result;
}

String StaveSharingLayout::format_instrument_change_lable(const InstrumentChange *origin_change, const InstrumentChange *shared_change,
                                                          const StaveSharingContext &ctx) {
    // Prefix the number to the string shown
    // TODO make these editable by the user
    const MStyle &style = ctx.style;

    if (shared_change->origin_items().empty() || !style.style_b(Sid::STAVE_SHARING_INSTR_CHANGE_PLAYER_NUM)) {
        return origin_change->xml_text();
    }

    bool trailing_dot_single;
    bool trailing_dot_multiple;
    int hyphen_limit;
    if (style.style_b(Sid::SHARED_ON_STAFF_NUMERALS_FOLLOW_INSTRUMENT_NUMERALS)) {
        trailing_dot_single = style.style_b(Sid::INSTRUMENT_NUMERALS_TRAILING_DOT_SINGLE);
        trailing_dot_multiple = style.style_b(Sid::INSTRUMENT_NUMERALS_TRAILING_DOT_MULTIPLE);
        hyphen_limit =
            style.style_b(Sid::INSTRUMENT_NUMERALS_HYPHEN_ENABLE) ? style.style_i(Sid::INSTRUMENT_NUMERALS_HYPHEN_THRESHOLD) : INT_MAX;
    } else {
        trailing_dot_single = style.style_b(Sid::SHARED_ON_STAFF_NUMERALS_TRAILING_DOT_SINGLE);
        trailing_dot_multiple = style.style_b(Sid::SHARED_ON_STAFF_NUMERALS_TRAILING_DOT_MULTIPLE);
        hyphen_limit = style.style_b(Sid::SHARED_ON_STAFF_NUMERALS_HYPHEN_ENABLE) ?
                           style.style_i(Sid::SHARED_ON_STAFF_NUMERALS_HYPHEN_THRESHOLD) :
                           INT_MAX;
    }

    String prefix;
    std::vector<const Instrument *> origin_instruments;
    for (const EngravingItem *shared_origin_change : shared_change->origin_items()) {
        const Part *origin_part = shared_origin_change->part();
        origin_instruments.push_back(origin_part->instrument());
    }
    prefix = SystemHeaderLayout::format_shared_voice_label(origin_instruments, trailing_dot_single, trailing_dot_multiple, hyphen_limit);

    return prefix + u" " + origin_change->xml_text();
}

void StaveSharingLayout::manage_voice_property_and_track_for_shared_items(std::span<EngravingItem *const> shared_items,
                                                                          track_idx_t start_origin_track, track_idx_t end_origin_track,
                                                                          const SharedTrackMap &track_map) {
    for (EngravingItem *shared_item : shared_items) {
        bool refers_to_voice2 = track2voice(shared_item->track()) != 0;
        bool voice2is_used = refers_to_voice2;
        if (!voice2is_used) {
            track_idx_t track_of_voice2 = track_zero_voice(shared_item->track()) + 1;
            for (track_idx_t origin_track = start_origin_track; origin_track <= end_origin_track; origin_track += ::mu::engraving::voices) {
                if (track_map.contains(origin_track) && track_map.at(origin_track) == track_of_voice2) {
                    voice2is_used = true;
                    break;
                }
            }
        }

        if (voice2is_used) {
            if (shared_item->has_voice_assignment_properties()) {
                shared_item->set_property(Pid::VOICE_ASSIGNMENT, VoiceAssignment::CURRENT_VOICE_ONLY);
                shared_item->set_placement_based_on_voice_assignment(shared_item->property_default(Pid::DIRECTION).value<DirectionV>());
            } else {
                if (refers_to_voice2) {
                    shared_item->set_placement(PlacementV::BELOW);
                } else {
                    shared_item->set_placement(PlacementV::ABOVE);
                }
            }
        } else {
            if (shared_item->has_voice_assignment_properties()) {
                shared_item->set_property(Pid::VOICE_ASSIGNMENT, VoiceAssignment::ALL_VOICE_IN_INSTRUMENT);
                shared_item->set_placement_based_on_voice_assignment(shared_item->property_default(Pid::DIRECTION).value<DirectionV>());
            } else {
                shared_item->reset_property(Pid::PLACEMENT);
            }
        }
    }
}

void StaveSharingLayout::cleanup(StaveSharingContext &ctx) {
    SharedPart *p = ctx.cur_shared_part;

    const TrackRange range = p->track_range();

    for (Segment *seg : ctx.segments_to_update) {
        std::vector<EngravingItem *> annotations = seg->rendered_annotations(); // Copy because we may remove elements
        for (EngravingItem *item : annotations) {
            if (item->is_layout_copy() && !item->system_flag() && !item->is_stave_sharing_label() && !item->is_stave_sharing_change() &&
                item->track() >= range.start_track && item->track() < range.end_track && item->origin_items().empty()) {
                remove_shared_output(item);
            }
        }

        if (seg->header()) {
            continue;
        }

        if (seg->is_type(SegmentType::BREATH | SegmentType::CLEF | SegmentType::TIME_SIG) ||
            (seg->is_key_sig_type() && seg_has_local_key_sig(seg, p))) {
            for (track_idx_t track = range.start_track; track < range.end_track; ++track) {
                EngravingItem *item = shared_slot(seg, track);
                if (!item || !item->origin_items().empty() || (item->is_time_sig() && !to_time_sig(item)->is_local())) {
                    continue;
                }
                remove_shared_output(item);
            }
            continue;
        }

        if (!seg->is_chord_rest_type()) {
            continue;
        }

        for (track_idx_t track = range.start_track; track < range.end_track; ++track) {
            ChordRest *cr = to_chord_rest(shared_slot(seg, track));
            if (!cr) {
                continue;
            }

            for (Tuplet *tuplet = cr->tuplet(); tuplet;) {
                auto *parent = tuplet->tuplet();
                if (tuplet->origin_items().empty()) remove_shared_output(tuplet);
                tuplet = parent;
            }

            if (cr->is_rest()) {
                bool bar_rest_on_first_voice = to_rest(cr)->is_full_measure_rest() && cr->voice() == 0;
                if (bar_rest_on_first_voice) {
                    continue;
                }
                if (cr->origin_items().empty()) remove_shared_output(cr);
                continue;
            }

            if (cr->is_chord()) {
                auto cleanup_chord = [&](Chord *c) {
                    std::vector<Articulation *> articulations = c->articulations(); // copy because may be removed
                    for (Articulation *art : articulations) {
                        if (art->origin_items().empty()) {
                            remove_shared_output(art);
                        }
                    }

                    std::vector<Note *> notes = c->notes(); // copy because may be removed
                    for (Note *note : notes) {
                        if (Tie *tie_back = note->tie_back(); tie_back && tie_back->origin_items().empty()) {
                            remove_shared_output(tie_back);
                        }

                        for (Spanner *sp : std::vector<Spanner *>(note->spanner_back())) {
                            if (sp->origin_items().empty()) {
                                remove_shared_output(sp);
                            }
                        }

                        if (Tie *tie_for = note->tie_for(); tie_for && !tie_for->end_element() && tie_for->origin_items().empty()) {
                            remove_shared_output(tie_for);
                        }

                        for (Spanner *sp : std::vector<Spanner *>(note->spanner_for())) {
                            if (!sp->end_element() && sp->origin_items().empty()) {
                                remove_shared_output(sp);
                            }
                        }

                        if (note->origin_items().empty()) {
                            remove_shared_output(note);
                        }
                    }
                    if (c->notes().empty()) {
                        remove_shared_output(c);
                    }
                };

                Chord *c = to_chord(cr);
                std::vector<Chord *> grace_notes = c->grace_notes(); // copy because we may remove elements
                for (Chord *grace : grace_notes) {
                    cleanup_chord(grace);
                }
                cleanup_chord(c);
            }
        }
    }

    if (ctx.cur_shared_part->authored_states().empty()) {
        add_measure_rests_if_need(ctx);
    } else {
        fill_rests_if_need(ctx);
    }

    for (Spanner *spanner : ctx.overlapping_spanners) {
        if (!spanner->is_layout_copy() && !spanner->system_flag() && spanner->track() >= range.start_track &&
            spanner->track() < range.end_track) {
            auto &suppressed = output(spanner).score_output().suppressed_shared_spanners;
            if (std::find(suppressed.begin(), suppressed.end(), spanner) == suppressed.end()) suppressed.push_back(spanner);
            for (auto *segment : spanner->spanner_segments()) segment->move_to_system(nullptr);
        }
        if (spanner->is_layout_copy() && !spanner->system_flag() && spanner->track() >= range.start_track &&
            spanner->track() < range.end_track && spanner->origin_items().empty()) {
            remove_shared_output(spanner);
        }
    }

    for (auto *change : ctx.obsolete_changes) remove_shared_output(change);

    for (StaveSharingLabel *old_label : ctx.old_stave_sharing_labels) {
        if (!muse::contains(ctx.updated_stave_sharing_labels, old_label)) {
            remove_shared_output(old_label);
        }
    }
}

void StaveSharingLayout::add_measure_rests_if_need(StaveSharingContext &ctx) {
    TrackRange track_range = ctx.cur_shared_part->track_range();
    staff_idx_t start_staff = track2staff(track_range.start_track);
    staff_idx_t end_staff = track2staff(track_range.end_track);

    Measure *prev = nullptr;
    for (Segment *segment : ctx.segments_to_update) {
        Measure *measure = segment->measure();
        if (measure == prev) continue;
        prev = measure;
        for (staff_idx_t staff_idx = start_staff; staff_idx < end_staff; ++staff_idx) {
            if (!measure->is_rendered_empty(staff_idx)) {
                continue;
            }
            if (ChordRest *first_cr = measure->first_chord_rest(staff_idx * ::mu::engraving::voices);
                first_cr && first_cr->is_full_measure_rest()) {
                continue;
            }

            auto *seg = measure->get_rendered_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
            if (seg->effective_element(staff_idx * ::mu::engraving::voices)) continue;
            auto *rest = Factory::create_rest(seg);
            rest->set_track(staff_idx * ::mu::engraving::voices);
            rest->set_duration_type(DurationType::V_MEASURE);
            rest->set_ticks(measure->ticks());
            add_shared_output(rest);
        }
    }
}

void StaveSharingLayout::fill_rests_if_need(StaveSharingContext &ctx) {
    const auto range = ctx.cur_shared_part->track_range();
    for (track_idx_t track = range.start_track; track < range.end_track; track += ::mu::engraving::voices) {
        const Staff *staff = ctx.score->staff(track2staff(track));
        const auto fill = [&](Measure *measure, Fraction tick, const Fraction &until) {
            const Fraction stretch = staff->rendered_time_stretch(tick);
            const auto durations = to_rhythmic_duration_list((until - tick) * stretch, true, (tick - measure->tick()) * stretch,
                                                             ctx.score->sigmap()->timesig(tick).nominal(), measure, 0, stretch);
            for (const TDuration &duration : durations) {
                auto *segment = measure->get_rendered_segment_r(SegmentType::CHORD_REST, tick - measure->tick());
                if (auto *copy = shared_slot(segment, track)) remove_shared_output(copy);
                auto *rest = Factory::create_rest(segment);
                rest->set_track(track);
                rest->set_duration_type(duration);
                rest->set_ticks(duration.is_measure() ? (until - tick) * stretch : duration.fraction());
                add_shared_output(rest);
                tick += rest->actual_ticks();
            }
        };

        Measure *prev = nullptr;
        for (Segment *updated : ctx.cr_segments_to_update) {
            Measure *measure = updated->measure();
            if (measure == prev) continue;
            prev = measure;
            Fraction filled_until = measure->tick();
            ChordRest *previous = nullptr;
            for (auto *segment = measure->rendered_first(); segment;) {
                auto *next = segment->rendered_next();
                if (segment->is_chord_rest_type()) {
                    if (auto *cr = to_chord_rest(segment->effective_element(track))) {
                        const Fraction tick = segment->tick();
                        if (tick > filled_until) {
                            fill(measure, filled_until, tick);
                        } else if (tick < filled_until && previous && previous->is_full_measure_rest()) {
                            fill(measure, previous->tick(), tick);
                            filled_until = tick;
                        }
                        filled_until = std::max(filled_until, tick + cr->actual_ticks());
                        previous = cr;
                    }
                }
                segment = next;
            }
            if (filled_until < measure->end_tick()) fill(measure, filled_until, measure->end_tick());
        }
    }
}

std::span<Segment *const> StaveSharingLayout::StaveSharingContext::segments_in_range(std::span<Segment *const> segments) const {
    if (cur_shared_part->authored_states().empty()) return segments;
    const auto by_tick = [](const Segment *segment, const Fraction &tick) { return segment->tick() < tick; };
    const auto first = std::lower_bound(segments.begin(), segments.end(), s_tick, by_tick);
    return {first, std::lower_bound(first, segments.end(), e_tick, by_tick)};
}

StaveSharingLayout::StaveSharingContext::StaveSharingContext(MeasureBase *first, MeasureBase *last, LayoutContext &ctx,
                                                             bool display_measures)
    : layout_ctx(ctx), style(ctx.conf().style()) {
    for (MeasureBase *mb = first; mb; mb = display_measures ? mb->next_mm() : mb->next()) {
        if (mb->is_measure()) {
            for (Segment *rendered = to_measure(mb)->rendered_first(); rendered; rendered = rendered->rendered_next()) {
                all_segments.push_back(rendered);
                if (rendered->is_chord_rest_type()) {
                    cr_segments.push_back(rendered);
                }
            }
        }

        if (mb == last) {
            break;
        }
    }

    if (cr_segments.empty()) {
        return;
    }

    s_tick = cr_segments.front()->tick();
    e_tick = cr_segments.back()->measure()->end_tick();
    score = cr_segments.front()->score();
}

const SharedTrackMap &StaveSharingLayout::StaveSharingContext::map_at_tick(const Fraction &tick) const {
    return cur_shared_part->track_map_at_tick(tick);
}

void StaveSharingLayout::StaveSharingContext::collect_overlapping_spanners() {
    overlapping_spanners.clear();
    for (auto *span : output(cr_segments.front()).score_output().shared_spanners) {
        if (!span->is_tie() && span->anchor() != Spanner::Anchor::NOTE && span->tick() <= e_tick && span->tick2() >= s_tick)
            overlapping_spanners.push_back(span);
    }
    auto spanners = score->spanner_map().find_overlapping(s_tick.ticks(), e_tick.ticks());
    for (auto &i : spanners) {
        overlapping_spanners.push_back(i.value);
    }
}
} // namespace mu::engraving::rendering::score
