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

#include "repeatlist.h"

#include <list>
#include <set>
#include <utility> // std::pair

#include "jump.h"
#include "layoutbreak.h"
#include "marker.h"
#include "measure.h"
#include "score.h"
#include "volta.h"

#include "global/realfn.h"
#include "global/log.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   RepeatSegment
//---------------------------------------------------------

RepeatSegment::RepeatSegment(int playback_count) : tick(0), utick(0), pause(0.0), playback_count(playback_count) {}

void RepeatSegment::add_measure(Measure const *const m) {
    if (m != nullptr) {
        if (measure_list_.empty()) {
            tick = m->tick().ticks();
        }
        if ((measure_list_.empty()) || (measure_list_.back() != m)) {
            measure_list_.push_back(m);
        }
    }
}

/// \brief Expands or clips my measureList up to m (including m)
void RepeatSegment::add_measures(Measure const *const m) {
    if (!measure_list_.empty()) {
        // Add up to the current measure, final measure is added outside of this condition
        Measure const *last_measure = measure_list_.back()->next_measure_mm();
        if (last_measure && (last_measure->tick() < m->tick())) { // Ensure provided reference is later than current last
            while (last_measure && last_measure != m) {
                measure_list_.push_back(last_measure);
                last_measure = last_measure->next_measure_mm();
            }
        }
        //else { // Possibly clip compared to current last measure }
        while (!measure_list_.empty() && (measure_list_.back()->tick() >= m->tick())) {
            measure_list_.pop_back();
        }
    }
    add_measure(m);
}

bool RepeatSegment::contains_measure(Measure const *const m) const {
    for (Measure const *const measure : measure_list_) {
        if (measure == m) {
            return true;
        }
    }
    return false;
}

bool RepeatSegment::ends_with_measure(Measure const *const m) const {
    if (measure_list_.empty()) {
        return false;
    }

    return measure_list_.back() == m;
}

bool RepeatSegment::starts_with_measure(const Measure *const m) const {
    if (measure_list_.empty()) {
        return false;
    }

    return measure_list_.front() == m;
}

bool RepeatSegment::is_empty() const { return measure_list_.empty(); }

int RepeatSegment::len() const { return (measure_list_.empty()) ? 0 : (measure_list_.back()->end_tick().ticks() - tick); }

int RepeatSegment::end_tick() const { return tick + len(); }

void RepeatSegment::pop_measure() {
    if (!measure_list_.empty()) {
        measure_list_.pop_back();
    }
}

const std::vector<const Measure *> &RepeatSegment::measure_list() const { return measure_list_; }

bool operator==(const RepeatSegment &lhs, const RepeatSegment &rhs) {
    if (lhs.measure_list_.size() != rhs.measure_list_.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.measure_list_.size(); i++) {
        if (lhs.measure_list_.at(i) != rhs.measure_list_.at(i)) {
            return false;
        }
    }
    return lhs.tick == rhs.tick && lhs.utick == rhs.utick && muse::real_is_equal(lhs.pause, rhs.pause) &&
           lhs.playback_count == rhs.playback_count;
}

//---------------------------------------------------------
//   RepeatList
//---------------------------------------------------------

RepeatList::RepeatList(Score *s) {
    score_ = s;
    utick2tick_hint_ = 0;
}

//---------------------------------------------------------
//   ~RepeatList
//---------------------------------------------------------

RepeatList::~RepeatList() { muse::delete_all(*this); }

//---------------------------------------------------------
//   ticks
//---------------------------------------------------------

int RepeatList::ticks() const {
    if (size() > 0) {
        const RepeatSegment *s = back();
        return s->utick + s->len();
    }
    return 0;
}

//---------------------------------------------------------
//   update
//---------------------------------------------------------

void RepeatList::update(bool expand, bool update_ties) {
    if (!score_changed_ && expand == expanded_) {
        return;
    }

    std::vector<RepeatSegment> old_segments;
    old_segments.reserve(size());
    for (RepeatSegment *rs : *this) {
        old_segments.push_back(*rs);
    }

    if (expand) {
        unwind();
    } else {
        flatten();
    }

    bool structure_changed = (old_segments.size() != size());
    if (!structure_changed) {
        for (size_t i = 0; i < size(); ++i) {
            if (!(old_segments[i] == *at(i))) {
                structure_changed = true;
                break;
            }
        }
    }

    score_changed_ = false;

    if (update_ties && structure_changed) {
        score_->undo_remove_stale_tie_jump_points();
    }
}

void RepeatList::update_uticks() {
    int utick = 0;

    for (RepeatSegment *s : *this) {
        s->utick = utick;
        utick += s->len();
    }
}

//---------------------------------------------------------
//   utick2tick
//---------------------------------------------------------

int RepeatList::utick2tick(int tick) const {
    size_t n = size();
    if (n == 0) {
        return tick;
    }
    if (tick < 0) {
        return 0;
    }
    unsigned ii = (utick2tick_hint_ < n) && (tick >= at(utick2tick_hint_)->utick) ? utick2tick_hint_ : 0;
    for (unsigned i = ii; i < n; ++i) {
        if ((tick >= at(i)->utick) && ((i + 1 == n) || (tick < at(i + 1)->utick))) {
            utick2tick_hint_ = i;
            return tick - (at(i)->utick - at(i)->tick);
        }
    }

    ASSERT_X(String(u"tick %1 not found in RepeatList").arg(tick));
    return 0;
}

//---------------------------------------------------------
//   tick2utick
//---------------------------------------------------------

int RepeatList::tick2utick(int tick) const {
    if (empty()) {
        return 0;
    }
    for (const RepeatSegment *s : *this) {
        if (tick >= s->tick && tick < s->end_tick()) {
            return s->utick + (tick - s->tick);
        }
    }
    return back()->utick + (tick - back()->tick);
}

///
/// \brief Lookup the RepeatSegment containing the given utick
///
std::vector<RepeatSegment *>::const_iterator RepeatList::find_repeat_segment_from_u_tick(int utick) const {
    for (auto it = cbegin(); it != cend(); ++it) {
        const RepeatSegment *seg = *it;
        if (utick >= seg->utick && utick < seg->utick + seg->len()) {
            return it;
        }
    }

    return cend();
}

std::vector<RepeatSegmentInfo> RepeatList::segment_info_list() const {
    std::vector<RepeatSegmentInfo> result;
    result.reserve(size());

    for (const RepeatSegment *seg : *this) {
        result.push_back({seg->tick, seg->end_tick(), seg->utick});
    }

    return result;
}

//---------------------------------------------------------
//   flatten
///   Make this repeat list flat (don't expand repeats)
//---------------------------------------------------------

void RepeatList::flatten() {
    muse::delete_all(*this);
    clear();

    Measure *m = score_->first_measure_mm();
    if (!m) {
        return;
    }

    RepeatSegment *s = new RepeatSegment(1);
    do {
        s->add_measure(m);
        m = m->next_measure_mm();
    } while (m);
    push_back(s);

    expanded_ = false;
}

//---------------------------------------------------------
//   unwind
//    implements:
//          - repeats
//          - volta
//          - d.c. al fine
//          - d.s. al fine
//          - d.s. al coda
//---------------------------------------------------------
enum class RepeatListElementType : unsigned char { SECTION_BREAK, VOLTA_START, VOLTA_END, REPEAT_START, REPEAT_END, JUMP, MARKER };

class RepeatListElement {
public:
    RepeatListElementType const repeat_list_element_type;
    EngravingItem const *const element;
    Measure const *const measure; // convenience, should be measure containing element
private:
    int repeat_count;

public:
    RepeatListElement(RepeatListElementType type, EngravingItem const *const el, Measure const *const m)
        : repeat_list_element_type(type), element(el), measure(m) {
        repeat_count = (type == RepeatListElementType::REPEAT_START) ? 1 : 0;
    }

    ~RepeatListElement() {
        // Voltas are cloned elements, we need to cleanup ourselves
        if (repeat_list_element_type == RepeatListElementType::VOLTA_START) {
            delete element;
        }
    }

    int get_repeat_count() const { return repeat_count; }

    void add_to_repeat_count(int add) { repeat_count += add; }
};

///---------------------------------------------------------
/// \brief Collects all RepeatListElementType elements in the score
/// \details Those are the only type of elements that actually influence playback order
///---------------------------------------------------------
void RepeatList::collect_repeat_list_elements() {
    RepeatListElementList section_rl_elements;

    // Clear out previous listing
    for (const RepeatListElementList &srle : rl_elements_) {
        muse::delete_all(srle);
    }
    rl_elements_.clear();

    RepeatListElement *start_from_repeat_measure = nullptr;

    // Section breaks may occur on non-Measure frames, so must search list of all MeasureBases
    // unwinding itself will only use actual Measures
    MeasureBase *mb = score_->first_measure_mm();
    // First measure of a section/score is always used as a reference REPEAT_START point
    // even if it doesn't have a start repeat
    start_from_repeat_measure = new RepeatListElement(RepeatListElementType::REPEAT_START, mb, to_measure(mb));
    section_rl_elements.push_back(start_from_repeat_measure);
    // Also trace down the final actual measure of this section (in case a frame follows)
    MeasureBase *section_end_measure_base = nullptr;
    // Used to track voltas; overlappings and real endings
    Volta *volta;
    std::list<Volta *> pre_processed_voltas;

    // Voltas might overlap (duplicate entries on multiple staves or "real" overlaps)
    // so we will pre-process them into cloned versions that handle those overlaps.
    // This assumes that spanners are ordered from first to last tick-wise
    for (const auto &spanner_entry : score_->spanner()) {
        if (!spanner_entry.second->is_volta() || !spanner_entry.second->play_spanner()) {
            continue;
        }

        if (!spanner_entry.second->start_measure() || !spanner_entry.second->end_measure()) {
            continue;
        }

        volta = to_volta(spanner_entry.second)->clone();

        if (pre_processed_voltas.empty()) { // First entry
            pre_processed_voltas.push_back(volta);
        } else { // Compare
            std::list<Volta *> voltas_to_merge;
            Fraction start_measure_tick = volta->start_measure()->tick();

            // List all overlapping voltas
            while ((!pre_processed_voltas.empty()) && (start_measure_tick <= pre_processed_voltas.back()->end_measure()->tick())) {
                voltas_to_merge.push_back(pre_processed_voltas.back());
                pre_processed_voltas.pop_back();
            }

            while (!voltas_to_merge.empty()) {
                // We'll have to shorten the already stored volta and split its remainder for merging
                Volta *remainder = voltas_to_merge.back()->clone();
                if (volta->start_measure() != remainder->start_measure()) {
                    // First part is not empty
                    voltas_to_merge.back()->set_end_element(volta->start_measure()->prev_measure_mm());
                    remainder->set_start_element(volta->start_measure());
                    // Store it
                    pre_processed_voltas.push_back(voltas_to_merge.back());
                }
                //else { New volta and existing one start at the same moment, there is no first part, only a remainder }
                voltas_to_merge.pop_back();

                // remainder and volta now have the same start point
                // Compare the end points and make remainder end first
                if (volta->end_measure()->tick() < remainder->end_measure()->tick()) {
                    Volta *swap = volta;
                    volta = remainder;
                    remainder = swap;
                }
                // Cross-section of the repeatList
                std::vector<int> endings = remainder->endings();
                muse::remove_if(endings, [&volta](const int &ending) { return !(volta->has_ending(ending)); });

                remainder->set_endings(endings);
                // Split and merge done
                pre_processed_voltas.push_back(remainder);
                if (volta->end_measure() != remainder->end_measure()) {
                    // volta extends past the end of remainder -> move its startpoint after remainder
                    volta->set_start_element(remainder->end_measure()->next_measure_mm());
                } else { // volta matched remainder endpoint, nothing left to merge from
                    pre_processed_voltas.splice(pre_processed_voltas.cend(), voltas_to_merge);
                    delete volta;
                    volta = nullptr;
                }
            } // !voltasToMerge.empty()

            if (volta != nullptr) {
                pre_processed_voltas.push_back(volta);
            }
        }
    }

    volta = nullptr;
    for (; mb; mb = mb->next_mm()) {
        if (mb->is_measure()) {
            Measure *m = to_measure(mb);
            section_end_measure_base = mb; // ending measure of section is the most recently encountered actual Measure
            Measure *underlying_measure = m->is_mm_rest() ? m->mm_rest_first() : m;

            // Volta ?
            if ((!pre_processed_voltas.empty()) && (pre_processed_voltas.front()->start_measure() == underlying_measure)) {
                if (volta != nullptr) {
                    //if (volta->endMeasure()->tick() < m->tick()) {
                    // The previous volta was supposed to end before us (open volta case) -> insert the end
                    section_rl_elements.push_back(new RepeatListElement(RepeatListElementType::VOLTA_END, volta, m->prev_measure_mm()));
                    //volta = nullptr; // No need, replaced immediately further down
                    //      }
                    //else { // Overlapping voltas; this should not happen as preProcessedVoltas should've dealt with this already }
                }
                // Now insert the start of the current volta
                volta = pre_processed_voltas.front();
                section_rl_elements.push_back(new RepeatListElement(RepeatListElementType::VOLTA_START, volta, m));
                // Look for start of next volta
                pre_processed_voltas.pop_front();
            }
            // Start
            if (m->repeat_start()) {
                if (volta != nullptr) {
                    if (volta->start_measure() != underlying_measure) {
                        // Volta and Start repeat are not on the same measure
                        // assume the previous volta was supposed to end before us (open volta case) -> insert the end
                        // Warning: This might "break" a volta prematurely if its explicit notated end is later than this point
                        //          Consider splitting the volta or ignoring this repeat all together
                        section_rl_elements.push_back(new RepeatListElement(RepeatListElementType::VOLTA_END, volta, m->prev_measure_mm()));
                        volta = nullptr;
                    }
                    //else { // Volta and Start Repeat coincide on the same measure, see test::repeat56.mscx }
                }
                start_from_repeat_measure = new RepeatListElement(RepeatListElementType::REPEAT_START, m, m);
                section_rl_elements.push_back(start_from_repeat_measure);
            }
            // Jumps and Markers
            for (EngravingItem *e : m->el()) {
                if (e->system_flag() && !e->is_top_system_object()) {
                    continue;
                }

                if (e->is_jump()) {
                    section_rl_elements.push_back(new RepeatListElement(RepeatListElementType::JUMP, e, m));
                    if (volta != nullptr) {
                        if ((volta->end_measure()->tick() < m->tick()) ||
                            ((volta->end_measure()->tick() == m->tick()) &&
                             (volta->get_property(Pid::END_HOOK_TYPE).value<HookType>() == HookType::NONE))) {
                            // The previous volta was supposed to end before us
                            // or open volta ends together with us -> insert the end
                            if (!m->repeat_end()) {
                                // But only do so if this measure doesn't also have an end repeat: see #327681
                                section_rl_elements.push_back(new RepeatListElement(RepeatListElementType::VOLTA_END, volta, m));
                                volta = nullptr;
                            }
                            //else {
                            //    The measure also has an end repeat which should be included in the volta
                            //    We can't abort the volta here, and leave it to the end repeat to do so
                            //}
                        }
                        //else { // Volta is spanning past this jump instruction }
                    }
                } else if (e->is_marker()) {
                    RepeatListElement *marker_rle = new RepeatListElement(RepeatListElementType::MARKER, e, m);
                    // There may be multiple markers in the same measure. Make sure we place right markers before left
                    // At the same time, we should ensure Markers are evaluated before Jumps
                    bool marker_rl_eis_right = to_marker(e)->is_right_marker();
                    auto insertion_it = section_rl_elements.end() - 1;
                    while ((*insertion_it)->measure == marker_rle->measure) {
                        bool marker_should_go_before = false;
                        if (((*insertion_it)->repeat_list_element_type == RepeatListElementType::MARKER) &&
                            (!marker_rl_eis_right) // We can be the end when right aligned
                        ) {
                            bool stored_marker_is_right = to_marker((*insertion_it)->element)->is_right_marker();
                            marker_should_go_before = stored_marker_is_right;
                        }
                        if (marker_should_go_before || ((*insertion_it)->repeat_list_element_type == RepeatListElementType::JUMP)) {
                            // Decrease position
                            // This should always be possible as the list always starts with a REPEAT_START element
                            assert(insertion_it != section_rl_elements.begin());
                            --insertion_it;
                        } else {
                            // Found location after which we should go
                            break;
                        }
                    }
                    // We should be inserted right after insertionIt
                    section_rl_elements.insert(insertion_it + 1, marker_rle);
                }
            }
            // End
            if (m->repeat_end()) {
                section_rl_elements.push_back(new RepeatListElement(RepeatListElementType::REPEAT_END, m, m));
                if (start_from_repeat_measure != nullptr) {
                    start_from_repeat_measure->add_to_repeat_count(m->repeat_count() - 1);
                }
                if (volta != nullptr) {
                    //if (volta->endMeasure()->tick() < m->tick()) {
                    // The previous volta was supposed to end before us (open volta case) -> insert the end
                    section_rl_elements.push_back(new RepeatListElement(RepeatListElementType::VOLTA_END, volta, m));
                    volta = nullptr;
                    //} else {
                    //    // Volta is spanning over this end repeat, consider splitting the volta
                    //    // or ignoring this repeat all together
                    //}
                }
            }
            // Volta end
            if ((volta != nullptr) && (volta->end_measure()->tick() == m->tick()) &&
                (volta->get_property(Pid::END_HOOK_TYPE).value<HookType>() != HookType::NONE)) {
                // end of closed volta
                section_rl_elements.push_back(new RepeatListElement(RepeatListElementType::VOLTA_END, volta, m));
                volta = nullptr;
            }
        }
        // Section break (or end of score)
        if (mb->section_break() || !mb->next_measure_mm()) {
            if (section_end_measure_base != nullptr) {
                if (volta != nullptr) {
                    //if (volta->endMeasure()->tick() < mb->tick()) {
                    // The previous volta was supposed to end before us (open volta case) -> insert the end
                    section_rl_elements.push_back(
                        new RepeatListElement(RepeatListElementType::VOLTA_END, volta, to_measure(section_end_measure_base)));
                    volta = nullptr;
                    //} else {
                    //    // Volta is spanning over this section break, consider splitting the volta
                    //    // and adding it again at the start of the next section
                    //}
                }
                section_rl_elements.push_back(
                    new RepeatListElement(RepeatListElementType::SECTION_BREAK, mb, to_measure(section_end_measure_base)));
                section_end_measure_base = nullptr; // reset to indicate not having found the end for the next section
                // store section
                rl_elements_.push_back(section_rl_elements);
            }
            // prepare for new section
            if (Measure *next_meas = mb->next_measure_mm()) {
                section_rl_elements = RepeatListElementList();
                // First measure of a section/score is always used as a reference REPEAT_START point
                // even if it doesn't have a start repeat
                start_from_repeat_measure = new RepeatListElement(RepeatListElementType::REPEAT_START, next_meas, to_measure(next_meas));
                section_rl_elements.push_back(start_from_repeat_measure);
            } else {
                // no more measures -> done
                break;
            }
        }
    }
}

///
/// \brief Finds a matching marker in the score (section first)
/// \remark Special case labels:
///         "start" will result in start of current section
///         "end" will result in end of current section
///
std::pair<std::vector<RepeatListElementList>::const_iterator, RepeatListElementList::const_iterator>
RepeatList::find_marker(muse::String label, std::vector<RepeatListElementList>::const_iterator reference_section_it,
                        RepeatListElementList::const_iterator reference_repeat_list_element_it) const {
    bool found = false;
    std::vector<RepeatListElementList>::const_iterator found_section_it;
    RepeatListElementList::const_iterator found_repeat_list_element_it;

    // Start in the current section
    found_section_it = reference_section_it;
    // Handle special label casing first
    if (label == "start") {
        found_repeat_list_element_it = reference_section_it->cbegin();
        found = true;
    } else if (label == "end") {
        found_repeat_list_element_it = reference_section_it->cend() - 1;
        found = true;
    } else {
        found_repeat_list_element_it = reference_repeat_list_element_it;
    }

    // Search backwards in this section
    while (!found && (found_repeat_list_element_it != reference_section_it->cbegin())) {
        --found_repeat_list_element_it;
        if (((*found_repeat_list_element_it)->repeat_list_element_type == RepeatListElementType::MARKER) &&
            ((to_marker((*found_repeat_list_element_it)->element))->label() == label)) {
            found = true;
        }
    }

    // Search forwards in this section
    if (!found) {
        found_repeat_list_element_it = reference_repeat_list_element_it + 1;
        while (!found && (found_repeat_list_element_it != reference_section_it->cend())) {
            if (((*found_repeat_list_element_it)->repeat_list_element_type == RepeatListElementType::MARKER) &&
                ((to_marker((*found_repeat_list_element_it)->element))->label() == label)) {
                found = true;
                break;
            }
            ++found_repeat_list_element_it;
        }
    }

    // Search backwards through all previous sections
    if (!found) {
        while (!found && (found_section_it != rl_elements_.cbegin())) {
            --found_section_it;
            found_repeat_list_element_it = found_section_it->cend();
            while (!found && (found_repeat_list_element_it != found_section_it->cbegin())) {
                --found_repeat_list_element_it;
                if (((*found_repeat_list_element_it)->repeat_list_element_type == RepeatListElementType::MARKER) &&
                    ((to_marker((*found_repeat_list_element_it)->element))->label() == label)) {
                    found = true;
                }
            }
        }
    }

    // Search forwards through all following sections
    if (!found) {
        found_section_it = reference_section_it + 1;
        while (found_section_it != rl_elements_.cend()) {
            found_repeat_list_element_it = found_section_it->cbegin();
            while (!found && (found_repeat_list_element_it != found_section_it->cend())) {
                if (((*found_repeat_list_element_it)->repeat_list_element_type == RepeatListElementType::MARKER) &&
                    ((to_marker((*found_repeat_list_element_it)->element))->label() == label)) {
                    found = true;
                    break;
                }
                ++found_repeat_list_element_it;
            }
            if (found) {
                break;
            }
            ++found_section_it;
        }
    }

    return std::make_pair(found_section_it, found_repeat_list_element_it);
}

///
/// \brief RepeatList::performJump
/// \param sectionIt                 [in]   Section of the jump target
/// \param repeatListElementTargetIt [in]   RepeatListElement of the jump target within the section
/// \param withRepeats               [in]   Whether first or last playthrough of the target is the actual target, influences playbackCount
/// \param playbackCount             [out]  Will contain the resulting playbackCount value
/// \param activeVolta               [out]  Contains a reference to the active Volta for jump target
/// \param startRepeatReference      [out]  Reference point to return to and compare against for jump target
///
void RepeatList::perform_jump(std::vector<RepeatListElementList>::const_iterator section_it,
                              RepeatListElementList::const_iterator repeat_list_element_target_it, bool with_repeats,
                              int *const playback_count, Volta const **const active_volta,
                              RepeatListElement const **const start_repeat_reference) const {
    RepeatListElementList::const_iterator repeat_list_element_it;
    // Fast forward processing up to our desired marker
    *active_volta = nullptr;
    *start_repeat_reference = *(section_it->cbegin());
    for (repeat_list_element_it = section_it->cbegin(); repeat_list_element_it != repeat_list_element_target_it; ++repeat_list_element_it) {
        switch ((*repeat_list_element_it)->repeat_list_element_type) {
            case RepeatListElementType::VOLTA_START: {
                *active_volta = to_volta((*repeat_list_element_it)->element);
            } break;
            case RepeatListElementType::VOLTA_END: {
                *active_volta = nullptr;
            } break;
            case RepeatListElementType::REPEAT_START: {
                *start_repeat_reference = *repeat_list_element_it;
            } break;
            default: {
                // nothing to do
            } break;
        }
    }
    // Set the correct playbackCount
    if (with_repeats) {
        if (*active_volta != nullptr) {
            *playback_count = (*active_volta)->first_ending();
            if (*playback_count == 0) {
                *playback_count = 1;
            }
        } else {
            *playback_count = 1;
        }
    } else { // Final repeat after jump
        if (*active_volta != nullptr) {
            *playback_count = (*active_volta)->last_ending();
            if (*playback_count == 0) {
                *playback_count = (*start_repeat_reference)->get_repeat_count();
            }
        } else {
            *playback_count = (*start_repeat_reference)->get_repeat_count();
        }
    }
}

///
/// \brief RepeatList::unwind
///
void RepeatList::unwind() {
    TRACEFUNC;

    muse::delete_all(*this);
    clear();
    jumps_taken_.clear();

    if (!score_->first_measure_mm()) {
        return;
    }

    collect_repeat_list_elements();

    // Following variables are used during unwinding, but may be altered when following jumps
    // Therefor they are declared outside of the loop
    RepeatSegment *rs = nullptr;
    int playback_count;
    Volta const *active_volta = nullptr;
    std::pair<std::vector<RepeatListElementList>::const_iterator, RepeatListElementList::const_iterator> play_until =
        std::make_pair(rl_elements_.cend(), rl_elements_[0].cend());
    std::pair<std::vector<RepeatListElementList>::const_iterator, RepeatListElementList::const_iterator> continue_at =
        std::make_pair(rl_elements_.cend(), rl_elements_[0].cend());
    Jump const *active_jump = nullptr;
    bool force_final_repeat = false; // Used during jump processing
    RepeatListElementList::const_iterator repeat_list_element_it;

    for (std::vector<RepeatListElementList>::const_iterator section_it = rl_elements_.cbegin(); section_it != rl_elements_.cend();
         ++section_it) {
        // Unwind this section
        RepeatListElement const *start_repeat_reference;
        playback_count = 1;
        repeat_list_element_it = section_it->cbegin(); // Should always be a REPEAT_START indicator
        start_repeat_reference = *repeat_list_element_it;
        active_volta = nullptr;
        play_until.first = rl_elements_.cend();
        continue_at.first = rl_elements_.cend();
        force_final_repeat = false;

        rs = new RepeatSegment(playback_count);
        rs->add_measure((*repeat_list_element_it)->measure);
        ++repeat_list_element_it;

        while (repeat_list_element_it != section_it->cend()) {
            if (rs) {
                rs->add_measures((*repeat_list_element_it)->measure);
            }
            switch ((*repeat_list_element_it)->repeat_list_element_type) {
                case RepeatListElementType::SECTION_BREAK: {
                    if ((rs != nullptr) && (!rs->is_empty())) {
                        push_back(rs);
                    }
                } break;
                case RepeatListElementType::VOLTA_START: {
                    active_volta = to_volta((*repeat_list_element_it)->element);
                    // A volta whose endings all lie beyond the exhausted repeats is played after them.
                    const bool play_after_repeats = (playback_count > 1) &&
                                                    (playback_count >= start_repeat_reference->get_repeat_count()) &&
                                                    (active_volta->first_ending() > playback_count);
                    if (!active_volta->has_ending(playback_count) && !play_after_repeats) {
                        // Should be skipped, remove our measure from rs
                        rs->pop_measure();
                        if (!rs->is_empty()) {
                            push_back(rs);
                        }
                        // Skip the volta, but only up to the last repeat structure actually passed:
                        // an open volta's VOLTA_END is deferred to the section end, and skipping that
                        // would discard the remainder of the score.
                        Measure const *last_skipped_measure = active_volta->end_measure();
                        do {
                            ++repeat_list_element_it;
                            const RepeatListElementType skipped_type = (*repeat_list_element_it)->repeat_list_element_type;
                            const bool is_repeat_structure = (skipped_type == RepeatListElementType::REPEAT_START) ||
                                                             (skipped_type == RepeatListElementType::REPEAT_END) ||
                                                             (skipped_type == RepeatListElementType::JUMP);
                            if (is_repeat_structure && ((last_skipped_measure == nullptr) ||
                                                        ((*repeat_list_element_it)->measure->tick() > last_skipped_measure->tick()))) {
                                last_skipped_measure = (*repeat_list_element_it)->measure;
                            }
                        } while ((*repeat_list_element_it)->repeat_list_element_type != RepeatListElementType::VOLTA_END);

                        RepeatListElementList::const_iterator after_volta_end_it = repeat_list_element_it + 1;
                        const bool volta_ended_by_section_end =
                            (after_volta_end_it == section_it->cend()) ||
                            ((*after_volta_end_it)->repeat_list_element_type == RepeatListElementType::SECTION_BREAK);
                        if (!volta_ended_by_section_end || (last_skipped_measure == nullptr)) {
                            last_skipped_measure = (*repeat_list_element_it)->measure;
                        }
                        active_volta = nullptr;
                        // Start next rs on the following measure
                        Measure const *const possible_next_measure = last_skipped_measure->next_measure_mm();
                        if (possible_next_measure == nullptr) {
                            rs = nullptr; // end of score, but will still encounter section break, notify it
                        } else {
                            rs = new RepeatSegment(playback_count);
                            rs->add_measure(possible_next_measure);
                        }
                    }
                    // else { take the volta, it's already set as activeVolta, so just continue }
                } break;
                case RepeatListElementType::VOLTA_END: {
                    active_volta = nullptr;
                } break;
                case RepeatListElementType::REPEAT_START: {
                    if (rs == nullptr) { // Sent here by an end-repeat
                        rs = new RepeatSegment(playback_count);
                        rs->add_measure((*repeat_list_element_it)->measure);
                    } else {
                        int desired_playback_count = (force_final_repeat) ? (*repeat_list_element_it)->get_repeat_count() : 1;
                        if (rs->playback_count != desired_playback_count) {
                            // Restart a segment as we have a new playbackCount reference
                            rs->pop_measure();
                            if (!rs->is_empty()) {
                                push_back(rs);
                            }
                            playback_count = desired_playback_count;
                            rs = new RepeatSegment(playback_count);
                            rs->add_measure((*repeat_list_element_it)->measure);
                        }
                        start_repeat_reference = *repeat_list_element_it;
                    }
                } break;
                case RepeatListElementType::REPEAT_END: {
                    (*repeat_list_element_it)->add_to_repeat_count(1);
                    if ((playback_count < start_repeat_reference->get_repeat_count()) &&
                        ((*repeat_list_element_it)->get_repeat_count() < (*repeat_list_element_it)->measure->repeat_count())) {
                        // Honor the repeat
                        if ((rs != nullptr) && (!rs->is_empty())) {
                            push_back(rs);
                        }
                        rs = nullptr;
                        do { // rewind
                            --repeat_list_element_it;
                            if ((*repeat_list_element_it)->repeat_list_element_type == RepeatListElementType::VOLTA_START) {
                                active_volta = nullptr;
                            } else if ((*repeat_list_element_it)->repeat_list_element_type == RepeatListElementType::VOLTA_END) {
                                active_volta = to_volta((*repeat_list_element_it)->element);
                            }
                        } while ((*repeat_list_element_it) != start_repeat_reference);
                        ++playback_count;
                        continue; // Force evaluation of the start repeat
                    }
                    //else { // Exhausted repeats, just continue }
                } break;
                case RepeatListElementType::JUMP: {
                    if (active_jump == to_jump((*repeat_list_element_it)->element)) {
                        // We've reached this jump again after just having taken it
                        // If this jump forced final repeat playthrough of measures
                        // it only forces so up to this point - see test::repeat23
                        force_final_repeat = false;
                    }
                    // TODO future improvement, use the repeatList property from the Jump to verify if it should be honored
                    // For now (or in case of an empty repeatList property) a Jump is only honored on final playthrough
                    // of the measure, and only taken once
                    if ((playback_count >= start_repeat_reference->get_repeat_count()) ||
                        ((active_volta != nullptr) && (playback_count == active_volta->last_ending()))) {
                        std::pair<Jump const *const, int> jump_occurrence =
                            std::make_pair(to_jump((*repeat_list_element_it)->element), playback_count);
                        if (jumps_taken_.find(jump_occurrence) == jumps_taken_.end()) { // Not yet processed
                            // Processing it now
                            jumps_taken_.insert(jump_occurrence);
                            // Find the jump targets
                            std::pair<std::vector<RepeatListElementList>::const_iterator, RepeatListElementList::const_iterator> jump_to =
                                find_marker(jump_occurrence.first->jump_to(), section_it, repeat_list_element_it);
                            play_until = find_marker(jump_occurrence.first->play_until(), section_it, repeat_list_element_it);
                            continue_at = find_marker(jump_occurrence.first->continue_at(), section_it, repeat_list_element_it);

                            // Execute
                            if (jump_to.first != rl_elements_.cend()) {
                                if (rs && !rs->is_empty()) {
                                    push_back(rs);
                                }
                                rs = nullptr;

                                active_jump = jump_occurrence.first;
                                perform_jump(jump_to.first, jump_to.second, active_jump->play_repeats(), &playback_count, &active_volta,
                                             &start_repeat_reference);
                                section_it = jump_to.first;
                                repeat_list_element_it = jump_to.second;
                                force_final_repeat = !(active_jump->play_repeats());

                                // Re-evaluate our repeat count for end repeat measures
                                if (play_until.first != rl_elements_.cend()) { // Only required if we have an end target
                                    for (auto rle_it = repeat_list_element_it + 1; rle_it != section_it->cend(); ++rle_it) {
                                        if (((*rle_it)->repeat_list_element_type == RepeatListElementType::REPEAT_END) &&
                                            ((*rle_it)->get_repeat_count() !=
                                             0)) { // We've been played before - see test::repeat22 for why only then
                                            // Clear out our current repeat count
                                            (*rle_it)->add_to_repeat_count(-((*rle_it)->get_repeat_count()));
                                            if (force_final_repeat) {
                                                // Set repeat count to the maximum number for this repeat
                                                (*rle_it)->add_to_repeat_count((*rle_it)->measure->repeat_count());
                                            }
                                        }
                                    }
                                }
                                if (active_volta != nullptr) {
                                    // We jumped into a volta, which might not be the only one of this startRepeatReference
                                    // If our jump didn't take us to the last playthrough, we must re-evaluate
                                    // all voltas related to this startRepeatReference
                                    if (playback_count < start_repeat_reference->get_repeat_count()) {
                                        // test::repeat52 and test::repeat53
                                        RepeatListElementList::const_iterator find_repeat_it = repeat_list_element_it;
                                        do {
                                            --find_repeat_it;
                                        } while ((*find_repeat_it) != start_repeat_reference);
                                        ++find_repeat_it; // Start volta analysis past this start repeat
                                        Volta const *volta_reference = nullptr;
                                        int processed_repeat_count = 1;
                                        while ((find_repeat_it != section_it->cend()) &&
                                               ((*find_repeat_it)->repeat_list_element_type != RepeatListElementType::REPEAT_START) &&
                                               (processed_repeat_count < start_repeat_reference->get_repeat_count())) {
                                            if ((*find_repeat_it)->repeat_list_element_type == RepeatListElementType::VOLTA_START) {
                                                volta_reference = to_volta((*find_repeat_it)->element);
                                                if (volta_reference->last_ending() < playback_count) {
                                                    // We won't pass here anymore - no need to adjust end repeat to this volta
                                                    volta_reference = nullptr;
                                                }
                                            } else if ((*find_repeat_it)->repeat_list_element_type == RepeatListElementType::REPEAT_END) {
                                                if (volta_reference != nullptr) {
                                                    int remaining_repeats_count = 0;
                                                    for (int ending : volta_reference->endings()) {
                                                        if (ending >= playback_count) {
                                                            ++remaining_repeats_count;
                                                        }
                                                    }
                                                    // Apply new remainingRepeatsCount
                                                    (*find_repeat_it)->add_to_repeat_count(-((*find_repeat_it)->get_repeat_count()));
                                                    (*find_repeat_it)
                                                        ->add_to_repeat_count((*find_repeat_it)->measure->repeat_count() -
                                                                              remaining_repeats_count - 1);
                                                }
                                                processed_repeat_count += (*find_repeat_it)->measure->repeat_count() - 1;
                                            }
                                            ++find_repeat_it;
                                        }
                                    }
                                }

                                // We've arrived at the target marker
                                rs = new RepeatSegment(playback_count);
                                rs->add_measure((*repeat_list_element_it)->measure);
                            } // End of jump execution
                        }
                    }
                } break;
                case RepeatListElementType::MARKER: {
                    if (((section_it == play_until.first) && (repeat_list_element_it == play_until.second)) &&
                        ((playback_count == start_repeat_reference->get_repeat_count()) ||
                         ((active_volta != nullptr) && (playback_count == active_volta->last_ending())))) {
                        // Found final playThrough of this Marker
                        if (rs && !rs->is_empty()) {
                            push_back(rs);
                        }
                        rs = nullptr;
                        play_until.first = rl_elements_.cend(); // Clear this reference - processed
                        force_final_repeat = false;

                        if (continue_at.first != rl_elements_.cend()) {
                            perform_jump(continue_at.first, continue_at.second, true, &playback_count, &active_volta,
                                         &start_repeat_reference);
                            section_it = continue_at.first;
                            repeat_list_element_it = continue_at.second;
                            if (start_repeat_reference->measure != (*(continue_at.second))->measure) {
                                // Use the continueAt target as repeat reference if it wasn't this measure already
                                // Check for measure is required as REPEAT_START is processed before a marker
                                // see test::repeat23 m12
                                start_repeat_reference = *(continue_at.second);
                            }

                            // We've arrived at the target marker
                            rs = new RepeatSegment(playback_count);
                            rs->add_measure((*repeat_list_element_it)->measure);

                            continue_at.first = rl_elements_.cend(); // Clear this reference - processed
                        } else { // Nowhere to go to, break out of this section loop and onto the next section
                            repeat_list_element_it = section_it->cend();
                            continue;
                        }
                    }
                } break;
            }
            ++repeat_list_element_it;
        }

        // Reached the end of this section
        if (!this->empty()) {
            // Inform the last RepeatSegment that the Section Break pause property should be honored now
            rs = this->back();
            repeat_list_element_it = section_it->cend() - 1;
            assert((*repeat_list_element_it)->repeat_list_element_type == RepeatListElementType::SECTION_BREAK);

            LayoutBreak const *const layout_break = to_measure_base((*repeat_list_element_it)->element)->section_break_element();
            if (layout_break != nullptr) {
                rs->pause = layout_break->pause();
            }
        }
    }

    update_uticks();
    expanded_ = true;
}
} // namespace mu::engraving
