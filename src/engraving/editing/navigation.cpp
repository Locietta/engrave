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

#include "navigation.h"

#include "../dom/box.h"
#include "../dom/chord.h"
#include "../dom/engravingitem.h"
#include "../dom/fret.h"
#include "../dom/guitarbend.h"
#include "../dom/hammeronpulloff.h"
#include "../dom/harmony.h"
#include "../dom/lyrics.h"
#include "../dom/measure.h"
#include "../dom/measurerepeat.h"
#include "../dom/mscore.h"
#include "../dom/note.h"
#include "../dom/pagelockindicator.h"
#include "../dom/rest.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/soundflag.h"
#include "../dom/spanner.h"
#include "../dom/staff.h"
#include "../dom/system.h"
#include "../dom/systemlockindicator.h"
#include "../dom/tapping.h"
#include "../style/style.h"
#include "global/btree.h"

using namespace mu;

namespace mu::engraving {
using ElementPair = std::pair<EngravingItem *, int>; // element and its index as a child

//    compares if two children are order correctly
//    from top-left to bottom-right

static bool is_child1before_child2(const ElementPair &child1, const ElementPair &child2) {
    assert(child1.first->parent() == child2.first->parent());

    PointF p1 = child1.first->pos() + child1.first->ldata()->bbox().center();
    PointF p2 = child2.first->pos() + child2.first->ldata()->bbox().center();

    // If one child is *clearly* above the other then visit the higher one first
    double vertical_separation = p2.y() - p1.y();
    if (vertical_separation > 10) {
        return true;
    } else if (vertical_separation < -10) {
        return false;
    }

    // Children are roughly aligned vertically. Visit the one on the left first
    double horizontal_separation = p2.x() - p1.x();
    if (horizontal_separation > 0.0) {
        return true;
    } else if (horizontal_separation < 0.0) {
        return false;
    }

    // Children are aligned horizontally so check exact vertical position.
    if (vertical_separation > 0.0) {
        return true;
    } else if (vertical_separation < 0.0) {
        return false;
    }

    // Elements are at exact same position so fall back to using child index to determine order.
    if (child1.second < child2.second) {
        return true;
    }

    return false;
}

//    return a set of children ordered
//    on the basis of position

static muse::BTreeSet<ElementPair, decltype(&is_child1before_child2)> to_child_pairs_set(const EngravingItem *element) {
    muse::BTreeSet<ElementPair, decltype(&is_child1before_child2)> children(&is_child1before_child2);
    int index = 0;

    for (EngravingObject *object : element->children()) {
        EngravingItem *engraving_item = to_engraving_item(object);

        if (!engraving_item) {
            continue;
        }

        children.insert(std::pair<EngravingItem *, int>(engraving_item, index));
        index++;
    }

    return children;
}

//    return next Chord or Rest

ChordRest *Navigation::next_chord_rest(const ChordRest *cr, const ChordRestNavigateOptions &options) {
    if (!cr) {
        return nullptr;
    }

    if (cr->is_grace()) {
        const Chord *c = to_chord(cr);
        Chord *pc = to_chord(cr->ownership_parent());

        if (options.skip_grace) {
            cr = to_chord_rest(cr->ownership_parent());
        } else if (cr->is_grace_before()) {
            const auto group = pc->grace_notes_before();
            auto i = std::find(group.begin(), group.end(), c);
            if (i == group.end()) {
                return nullptr; // unable to find self?
            }
            ++i;
            if (i != group.end()) {
                return *i;
            }
            // if this was last grace note before, return parent
            return pc;
        } else {
            const auto group = pc->grace_notes_after();
            auto i = std::find(group.begin(), group.end(), c);
            if (i == group.end()) {
                return nullptr; // unable to find self?
            }
            ++i;
            if (i != group.end()) {
                return *i;
            }
            // if this was last grace note after, fall through to find next main note
            cr = pc;
        }
    } else { // cr is not a grace note
        if (cr->is_chord() && !options.skip_grace) {
            const Chord *c = to_chord(cr);
            if (!c->grace_notes().empty()) {
                const auto group = c->grace_notes_after();
                if (!group.empty()) {
                    return group.front();
                }
            }
        }
    }

    track_idx_t track = cr->track();
    SegmentType st = SegmentType::CHORD_REST;
    Segment *cur_seg = cr->segment();
    for (Segment *seg = cur_seg->next1_mm(st); seg; seg = seg->next1_mm(st)) {
        if (options.disable_over_repeats && !segments_are_adjacent(cur_seg, seg)) {
            return nullptr;
        }
        ChordRest *e = to_chord_rest(seg->element(track));
        if (e) {
            if (options.skip_measure_repeat_rests && e->is_rest() && e->measure()->is_measure_repeat_group(track2staff(track))) {
                continue; // these rests are not shown, skip them
            }
            if (e->is_chord() && !options.skip_grace) {
                Chord *c = to_chord(e);
                if (!c->grace_notes().empty()) {
                    const auto group = c->grace_notes_before();
                    if (!group.empty()) {
                        return group.front();
                    }
                }
            }
            return e;
        }
    }

    return nullptr;
}

//    return previous Chord or Rest
//    if grace is true, include grace notes

ChordRest *Navigation::prev_chord_rest(const ChordRest *cr, const ChordRestNavigateOptions &options) {
    if (!cr) {
        return nullptr;
    }

    if (cr->is_grace()) {
        const Chord *c = to_chord(cr);
        Chord *pc = to_chord(cr->ownership_parent());

        if (options.skip_grace) {
            cr = to_chord_rest(cr->ownership_parent());
        } else if (cr->is_grace_before()) {
            const auto group = pc->grace_notes_before();
            auto i = std::find(group.begin(), group.end(), c);
            if (i == group.end()) {
                return nullptr; // unable to find self?
            }
            if (i != group.begin()) {
                return group.at(static_cast<size_t>(std::distance(group.begin(), i) - 1));
            }
            // if this was first grace note before, fall through to find previous main note
            cr = pc;
        } else {
            const auto group = pc->grace_notes_after();
            auto i = std::find(group.begin(), group.end(), c);
            if (i == group.end()) {
                return nullptr; // unable to find self?
            }
            if (i != group.begin()) {
                return group.at(static_cast<size_t>(std::distance(group.begin(), i) - 1));
            }
            // if this was first grace note after, return parent
            return pc;
        }
    } else {
        //
        // cr is not a grace note
        if (cr->is_chord() && !options.skip_grace) {
            const Chord *c = to_chord(cr);
            const auto group = c->grace_notes_before();
            if (!group.empty()) {
                return group.back();
            }
        }
    }

    track_idx_t track = cr->track();
    SegmentType st = SegmentType::CHORD_REST;
    Segment *cur_seg = cr->segment();
    for (Segment *seg = cr->segment()->prev1_mm(st); seg; seg = seg->prev1_mm(st)) {
        if (options.disable_over_repeats && !segments_are_adjacent(cur_seg, seg)) {
            return nullptr;
        }

        ChordRest *e = to_chord_rest(seg->element(track));
        if (e) {
            if (options.skip_measure_repeat_rests && e->is_rest() && e->measure()->is_measure_repeat_group(track2staff(track))) {
                continue; // these rests are not shown, skip them
            }
            if (e->is_chord() && !options.skip_grace) {
                const auto group = to_chord(e)->grace_notes_after();
                if (!group.empty()) {
                    return group.back();
                }
            }
            return e;
        }
    }

    return nullptr;
}

EngravingItem *Navigation::first_element(Score *score, bool frame) {
    if (frame) {
        MeasureBase *mb = score->measures()->first();
        if (mb && mb->is_box()) {
            return mb;
        }
    }
    Segment *s = score->first_segment_mm(SegmentType::ALL);
    return s ? s->element(0) : nullptr;
}

EngravingItem *Navigation::last_element(Score *score, bool frame) {
    if (frame) {
        MeasureBase *mb = score->measures()->last();
        if (mb && mb->is_box()) {
            auto box_children = to_child_pairs_set(mb);
            if (!box_children.empty()) {
                return box_children.rbegin()->first;
            }
            return mb;
        }
    }
    EngravingItem *re = 0;
    Segment *seg = score->last_segment_mm();
    if (!seg) {
        return nullptr;
    }
    while (true) {
        for (size_t i = (score->staves().size() - 1) * ::mu::engraving::voices; i < score->staves().size() * ::mu::engraving::voices; i++) {
            if (seg->element(i)) {
                re = seg->element(i);
            }
        }
        if (re) {
            if (re->is_chord()) {
                return to_chord(re)->notes().front();
            }
            return re;
        }
        seg = seg->prev1_mm(SegmentType::ALL);
    }
}

//    - find the lyric (if any) before this one (not including lines)
//    - currently used to determine the first lyric of a melisma

Lyrics *Navigation::last_lyrics_in_measure(const Segment *seg, const staff_idx_t staff_idx, const int no, const PlacementV &placement) {
    while (seg) {
        const track_idx_t strack = staff_idx * ::mu::engraving::voices;
        const track_idx_t etrack = strack + ::mu::engraving::voices;
        for (track_idx_t track = strack; track < etrack; ++track) {
            EngravingItem *el = seg->element(track);
            Lyrics *prev_lyrics = el && el->is_chord() ? to_chord_rest(el)->lyrics(no, placement) : nullptr;
            if (prev_lyrics) {
                return prev_lyrics;
            }
        }
        seg = seg->prev1(mu::engraving::SegmentType::CHORD_REST);
    }
    return nullptr;
}

Lyrics *Navigation::prev_lyrics(const Lyrics *lyrics) {
    Segment *seg = lyrics->ownership_parent() ? lyrics->segment() : nullptr;
    if (!seg) {
        return nullptr;
    }
    while ((seg = seg->prev1(mu::engraving::SegmentType::CHORD_REST))) {
        const track_idx_t strack = lyrics->staff_idx() * ::mu::engraving::voices;
        const track_idx_t etrack = strack + ::mu::engraving::voices;
        for (track_idx_t track = strack; track < etrack; ++track) {
            EngravingItem *el = seg->element(track);
            Lyrics *prev_lyrics = el && el->is_chord() ? to_chord_rest(el)->lyrics(lyrics->verse(), lyrics->placement()) : nullptr;
            if (prev_lyrics) {
                return prev_lyrics;
            }
        }
    }
    return nullptr;
}

Lyrics *Navigation::next_lyrics(const Lyrics *lyrics) {
    Segment *seg = lyrics->ownership_parent() ? lyrics->segment() : nullptr;
    if (!seg) {
        return nullptr;
    }
    Segment *next_segment = seg;
    while ((next_segment = next_segment->next1(mu::engraving::SegmentType::CHORD_REST))) {
        const track_idx_t strack = lyrics->staff_idx() * ::mu::engraving::voices;
        const track_idx_t etrack = strack + ::mu::engraving::voices;
        for (track_idx_t track = strack; track < etrack; ++track) {
            EngravingItem *el = next_segment->element(track);
            Lyrics *next_lyrics = el && el->is_chord() ? to_chord_rest(el)->lyrics(lyrics->verse(), lyrics->placement()) : nullptr;
            if (next_lyrics) {
                return next_lyrics;
            }
        }
    }
    return nullptr;
}

//    move current selection

} // namespace mu::engraving
