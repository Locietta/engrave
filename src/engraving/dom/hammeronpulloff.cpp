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

#include "hammeronpulloff.h"

#include "iengravingconfiguration.h" // IWYU pragma: keep

#include "mscore.h"
#include "note.h"
#include "score.h"
#include "stafftype.h"
#include "style/textstyle.h"
#include "system.h"

namespace mu::engraving {
HammerOnPullOff::HammerOnPullOff(DummyParentOr<EngravingItem> parent) : Slur(parent, ElementType::HAMMER_ON_PULL_OFF) {}

HammerOnPullOff::HammerOnPullOff(const HammerOnPullOff &other) : Slur(other) {}

HammerOnPullOffSegment::HammerOnPullOffSegment(HammerOnPullOff *sp) : SlurSegment(sp, ElementType::HAMMER_ON_PULL_OFF_SEGMENT) {}

HammerOnPullOffSegment::HammerOnPullOffSegment(const HammerOnPullOffSegment &other) : SlurSegment(other) {}

Color HammerOnPullOffSegment::cur_color(const rendering::PaintOptions &opt) const {
    if (!opt.is_printing && MScore::warn_guitar_bends && !is_valid()) {
        return selected() ? configuration()->critical_selected_color() : configuration()->critical_color();
    }

    return SlurSegment::cur_color(opt);
}

void HammerOnPullOffSegment::scan_elements(std::function<void(EngravingItem *)> func) {
    for (HammerOnPullOffText *hopo : hopo_text_) {
        hopo->scan_elements(func);
    }

    EngravingItem::scan_elements(func);
}

void HammerOnPullOffSegment::set_track(track_idx_t idx) {
    track_ = idx;
    for (HammerOnPullOffText *hopo : hopo_text_) {
        hopo->set_track(idx);
    }
}

void HammerOnPullOffSegment::set_selected(bool f) {
    for (HammerOnPullOffText *text : hopo_text_) {
        text->set_selected(f);
    }

    SlurTieSegment::set_selected(f);
}

void HammerOnPullOffSegment::update_hopo_text() {
    Chord *start_chord = nullptr;
    if (is_single_begin_type()) {
        EngravingItem *start_el = hammer_on_pull_off()->start_element();
        start_chord = start_el && start_el->is_chord() ? to_chord(start_el) : nullptr;
    } else {
        ChordRest *first_cr = system()->first_chord_rest(track());
        start_chord = first_cr && first_cr->is_chord() ? to_chord(first_cr) : nullptr;
    }

    Chord *end_chord = nullptr;
    if (is_single_end_type()) {
        EngravingItem *end_el = hammer_on_pull_off()->end_element();
        end_chord = end_el && end_el->is_chord() ? to_chord(end_el) : nullptr;
    } else {
        // If the segment doesn't end in this system, the endChord is the first chord of next system
        ChordRest *last_cr = system()->last_chord_rest(track());
        if (last_cr) {
            last_cr = to_chord_rest(last_cr->segment()->next1_with_elems_on_track(track())->element(track()));
        }
        end_chord = last_cr && last_cr->is_chord() ? to_chord(last_cr) : nullptr;
    }

    if (!start_chord || !end_chord) {
        muse::delete_all(hopo_text_);
        hopo_text_.clear();
        return;
    }

    std::vector<HopoTextRegion> hopo_text_regions = compute_hopo_text_regions(start_chord, end_chord);
    size_t region_count = hopo_text_regions.size();

    size_t cur_region_idx = 0;
    for (; cur_region_idx < region_count; ++cur_region_idx) {
        HopoTextRegion cur_region = hopo_text_regions[cur_region_idx];

        HammerOnPullOffText *cur_hopo_text;
        if (cur_region_idx < hopo_text_.size()) {
            // Reuse existing, if available
            cur_hopo_text = hopo_text_[cur_region_idx];
        } else {
            // Create new
            cur_hopo_text = new HammerOnPullOffText(this);
            hopo_text_.push_back(cur_hopo_text);
        }

        cur_hopo_text->set_track(track());
        cur_hopo_text->set_is_valid(cur_region.is_valid);
        cur_hopo_text->set_is_hammer_on(cur_region.is_hammer_on);
        cur_hopo_text->set_xml_text(style().style_b(Sid::HOPO_UPPER_CASE) ? (cur_region.is_hammer_on ? "H" : "P") :
                                                                            (cur_region.is_hammer_on ? "h" : "p"));
        cur_hopo_text->set_start_chord(cur_region.start_chord);
        cur_hopo_text->set_end_chord(cur_region.end_chord);
    }

    // Delete unused
    if (cur_region_idx < hopo_text_.size()) {
        size_t unused_hopo_count = hopo_text_.size() - cur_region_idx;
        for (size_t i = 0; i < unused_hopo_count; ++i) {
            delete hopo_text_.back();
            hopo_text_.pop_back();
        }
    }
}

std::vector<HammerOnPullOffSegment::HopoTextRegion> HammerOnPullOffSegment::compute_hopo_text_regions(Chord *start_chord,
                                                                                                      Chord *end_chord) {
    std::vector<HopoTextRegion> result;
    bool is_tab_staff = staff_type()->is_tab_staff();
    if ((is_tab_staff && !style().style_b(Sid::HOPO_SHOW_ON_TAB_STAVES)) ||
        (!is_tab_staff && !style().style_b(Sid::HOPO_SHOW_ON_STANDARD_STAVES))) {
        return result;
    }

    for (Chord *cur_chord = start_chord; cur_chord != end_chord;) {
        IF_ASSERT_FAILED(cur_chord->tick() <= end_chord->tick()) { break; }

        Chord *next_chord = cur_chord->next();
        if (!next_chord) {
            break;
        }

        Note *cur_note = nullptr;
        Note *next_note = nullptr;
        resolve_start_end_notes(&cur_note, &next_note, cur_chord, next_chord, is_tab_staff);

        bool is_valid = is_tab_staff ? next_note->string() == cur_note->string() : next_note->pitch() != cur_note->pitch();
        bool is_hammer_on = is_tab_staff ? next_note->fret() > cur_note->fret() : next_note->pitch() > cur_note->pitch();

        bool start_new_region = result.empty() || style().style_b(Sid::HOPO_SHOW_ALL) || result.back().is_hammer_on != is_hammer_on;

        if (start_new_region) {
            HopoTextRegion region;
            region.start_chord = cur_chord;
            region.end_chord = next_chord;
            region.is_valid = is_valid;
            region.is_hammer_on = is_hammer_on;
            result.push_back(region);
        } else {
            HopoTextRegion &cur_region = result.back();
            cur_region.end_chord = next_chord;
            cur_region.is_valid = cur_region.is_valid && is_valid;
        }

        cur_chord = next_chord;
    }

    return result;
}

void HammerOnPullOffSegment::resolve_start_end_notes(Note **start_note, Note **end_note, Chord *start_chord, Chord *end_chord,
                                                     bool is_tab_staff) {
    // In future we need the ability to draw slurs between individual notes. For now, slurs are
    // attached to chords so we just try to guess which notes to consider for this HOPO

    const std::vector<Note *> &start_notes = start_chord->notes();
    const std::vector<Note *> &end_notes = end_chord->notes();
    int start_notes_size = static_cast<int>(start_notes.size());
    int end_notes_size = static_cast<int>(end_notes.size());
    int larger_note_count = start_notes_size > end_notes_size ? start_notes_size : end_notes_size;

    for (int i = larger_note_count - 1; i >= 0; --i) {
        int start_note_idx = std::min(i, start_notes_size - 1);
        int end_note_idx = std::min(i, end_notes_size - 1);
        Note *s_note = start_notes[start_note_idx];
        Note *e_note = end_notes[end_note_idx];
        if (s_note->tie_for() && s_note->tie_for() == e_note->tie_back()) {
            continue;
        }

        bool usable_pair = s_note->pitch() != e_note->pitch();
        if (is_tab_staff) {
            usable_pair &= s_note->string() == e_note->string();
        }

        if (usable_pair) {
            *start_note = s_note;
            *end_note = e_note;
            return;
        }
    }

    // Couldn't find a usable pair
    *start_note = start_chord->up_note();
    *end_note = end_chord->up_note();
}

bool HammerOnPullOffSegment::is_user_modified() const {
    for (const HammerOnPullOffText *hopo_text : hopo_text_) {
        if (hopo_text->is_user_modified()) {
            return true;
        }
    }

    return SlurSegment::is_user_modified();
}

bool HammerOnPullOffSegment::is_valid() const {
    for (const HammerOnPullOffText *hopo_text : hopo_text_) {
        if (!hopo_text->is_valid()) {
            return false;
        }
    }

    return true;
}

void HammerOnPullOffSegment::reset() {
    for (HammerOnPullOffText *hopo_text : hopo_text_) {
        hopo_text->reset();
    }

    SlurTieSegment::reset();
}

static ElementStyle hopo_style;

HammerOnPullOffText::HammerOnPullOffText(HammerOnPullOffSegment *parent)
    : TextBase(ElementType::HAMMER_ON_PULL_OFF_TEXT, parent, TextStyleType::HAMMER_ON_PULL_OFF,
               ElementFlag::MOVABLE | ElementFlag::GENERATED | ElementFlag::ON_STAFF) {
    reset_property(Pid::PLACEMENT);
    init_element_style(&hopo_style);
}

HammerOnPullOffText::HammerOnPullOffText(const HammerOnPullOffText &h) : TextBase(h) {}

bool HammerOnPullOffText::is_user_modified() const {
    for (const TextStyleProperty &p : *text_style(text_style_type())) {
        if (get_property(p.pid) != property_default(p.pid)) {
            return true;
        }
    }

    return TextBase::is_user_modified();
}

Color HammerOnPullOffText::cur_color(const rendering::PaintOptions &opt) const {
    if (!is_valid() && MScore::warn_guitar_bends && !opt.is_printing) {
        return selected() ? configuration()->critical_selected_color() : configuration()->critical_color();
    }

    return TextBase::cur_color(opt);
}

PropertyValue HammerOnPullOffText::property_default(Pid id) const {
    switch (id) {
        case Pid::PLACEMENT: return PlacementV::ABOVE;
        default: return TextBase::property_default(id);
    }
}
} // namespace mu::engraving
