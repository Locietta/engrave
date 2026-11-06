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

#include "slur.h"
#include "textbase.h"

namespace mu::engraving {
class HammerOnPullOffText final : public TextBase {
    OBJECT_ALLOCATOR(engraving, HammerOnPullOffText)
    DECLARE_CLASSOF(ElementType::HAMMER_ON_PULL_OFF_TEXT)

public:
    HammerOnPullOffText(HammerOnPullOffSegment *parent = nullptr);
    HammerOnPullOffText(const HammerOnPullOffText &h);

    HammerOnPullOffText *clone() const override { return new HammerOnPullOffText(*this); }

    HammerOnPullOffSegment *hopo_segment() const { return to_hammer_on_pull_off_segment(ownership_parent()); }

    Chord *start_chord() const { return start_chord_; }

    Chord *end_chord() const { return end_chord_; }

    void set_start_chord(Chord *c) { start_chord_ = c; }

    void set_end_chord(Chord *c) { end_chord_ = c; }

    bool is_user_modified() const override;

    bool is_valid() const { return is_valid_; }

    void set_is_valid(bool v) { is_valid_ = v; }

    bool is_hammer_on() const { return is_hammer_on_; }

    void set_is_hammer_on(bool v) { is_hammer_on_ = v; }

    Color cur_color(const rendering::PaintOptions &opt) const override;

    PropertyValue property_default(Pid id) const override;

    bool position_relative_to_notehead_rest() const override { return false; }

private:
    Chord *start_chord_ = nullptr;
    Chord *end_chord_ = nullptr;
    bool is_valid_ = true;
    bool is_hammer_on_ = true;
};

class HammerOnPullOffSegment final : public SlurSegment {
    OBJECT_ALLOCATOR(engraving, HammerOnPullOffSegment)
    DECLARE_CLASSOF(ElementType::HAMMER_ON_PULL_OFF_SEGMENT)

public:
    HammerOnPullOffSegment(HammerOnPullOff *sp);
    HammerOnPullOffSegment(const HammerOnPullOffSegment &other);

    HammerOnPullOffSegment *clone() const override { return new HammerOnPullOffSegment(*this); }

    HammerOnPullOff *hammer_on_pull_off() const { return to_hammer_on_pull_off(spanner()); }

    Color cur_color(const rendering::PaintOptions &opt) const override;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void set_track(track_idx_t idx) override;
    void set_selected(bool f) override;

    void update_hopo_text();

    void add_hopo_text(HammerOnPullOffText *t) { hopo_text_.push_back(t); }

    const std::vector<HammerOnPullOffText *> &hopo_text() const { return hopo_text_; }

    bool is_user_modified() const override;
    bool is_valid() const;

    void reset() override;

private:
    struct HopoTextRegion {
        Chord *start_chord = nullptr;
        Chord *end_chord = nullptr;
        bool is_hammer_on = true;
        bool is_valid = true;
    };

    std::vector<HopoTextRegion> compute_hopo_text_regions(Chord *start_chord, Chord *end_chord);
    void resolve_start_end_notes(Note **start_note, Note **end_note, Chord *start_chord, Chord *end_chord, bool is_tab_staff);

    std::vector<HammerOnPullOffText *> hopo_text_;
};

class HammerOnPullOff final : public Slur {
    OBJECT_ALLOCATOR(engraving, HammerOnPullOff)
    DECLARE_CLASSOF(ElementType::HAMMER_ON_PULL_OFF)

public:
    HammerOnPullOff *clone() const override { return new HammerOnPullOff(*this); }

    SlurTieSegment *new_slur_tie_segment() override { return new HammerOnPullOffSegment(this); }

    friend class Factory;

    HammerOnPullOff(DummyParentOr<EngravingItem> parent);
    HammerOnPullOff(const HammerOnPullOff &);
};
} // namespace mu::engraving
