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

#include "containers.h"
#include "engraving/dom/score.h"

namespace mu::engraving::write {
struct WriteRange {
    MeasureBase *start_measure = nullptr;
    MeasureBase *end_measure = nullptr;
    staff_idx_t start_staff_idx = muse::nidx;
    staff_idx_t end_staff_idx = muse::nidx;

    bool operator==(const WriteRange &r) const {
        return start_measure == r.start_measure && end_measure == r.end_measure && start_staff_idx == r.start_staff_idx &&
               end_staff_idx == r.end_staff_idx;
    }
};

struct WriteContext {

    WriteContext(const Score *s, bool clipboard_mode = false) : score_(s), clipboardmode_value(clipboard_mode) {}

    EngravingConfiguration *configuration() const;

    Fraction cur_tick() const { return cur_tick_value; }

    void set_cur_tick(const Fraction &v) { cur_tick_value = v; }

    void inc_cur_tick(const Fraction &v) { cur_tick_value += v; }

    Fraction tick_diff() const { return tick_diff_value; }

    void set_tick_diff(const Fraction &v) { tick_diff_value = v; }

    track_idx_t cur_track() const { return cur_track_value; }

    void set_cur_track(track_idx_t v) { cur_track_value = v; }

    int track_diff() const { return track_diff_value; }

    void set_track_diff(int v) { track_diff_value = v; }

    bool clipboardmode() const { return clipboardmode_value; }

    // For a standalone excerpt, resolve link groups to the first member that
    // this writer actually emits, never to an absent master-score element.
    void set_standalone(bool value) { standalone_value = value; }

    bool standalone() const { return standalone_value; }

    EID standalone_link(const void *group, EID current) { return standalone_links.try_emplace(group, current).first->second; }

    void set_inline_style(bool value) { inline_style_value = value; }

    bool inline_style() const { return inline_style_value; }

    void set_inline_excerpts(bool value) { inline_excerpts_value = value; }

    bool inline_excerpts() const { return inline_excerpts_value; }

    const std::optional<Fraction> &first_clipboard_tick() const { return first_clipboard_tick_value; }

    void set_first_clipboard_tick(const Fraction &v) { first_clipboard_tick_value = v; }

    bool can_write(const EngravingItem *) const;
    bool can_write_note_idx(size_t note_idx, size_t total_notes_in_chord) const;
    bool can_write_voice(track_idx_t track) const;

    bool should_write_range() const { return range_value.has_value(); }

    const std::optional<WriteRange> &range() const { return range_value; }

    void set_range(const WriteRange &v) { range_value = v; }

    inline bool operator==(const WriteContext &c) const {
        return cur_tick_value == c.cur_tick_value && tick_diff_value == c.tick_diff_value && cur_track_value == c.cur_track_value &&
               track_diff_value == c.track_diff_value && clipboardmode_value == c.clipboardmode_value &&
               standalone_value == c.standalone_value && inline_style_value == c.inline_style_value &&
               inline_excerpts_value == c.inline_excerpts_value && standalone_links == c.standalone_links && range_value == c.range_value &&
               first_clipboard_tick_value == c.first_clipboard_tick_value;
    }

    inline bool operator!=(const WriteContext &c) const { return !this->operator==(c); }

private:

    const Score *score_ = nullptr;

    Fraction cur_tick_value{0, 1}; // used to optimize output
    Fraction tick_diff_value{0, 1};
    track_idx_t cur_track_value = muse::nidx;
    int track_diff_value{0}; // saved track is curTrack-trackDiff

    bool clipboardmode_value{false}; // used to modify write() behaviour
    bool standalone_value{false};
    bool inline_style_value{false};
    bool inline_excerpts_value{false};
    std::map<const void *, EID> standalone_links;

    std::optional<WriteRange> range_value;
    std::optional<Fraction> first_clipboard_tick_value;
};
} // namespace mu::engraving::write
