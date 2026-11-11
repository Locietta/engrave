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
 Implementation of classes SysStaff and System.
*/

#include "system.h"

#include "../editing/navigation.h"
#include "beam.h"
#include "box.h"
#include "bracket.h"
#include "bracketitem.h"
#include "chordrest.h"
#include "factory.h"
#include "iengravingconfiguration.h" // IWYU pragma: keep
#include "log.h"
#include "measure.h"
#include "mscore.h"
#include "page.h"
#include "pagelockindicator.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "spacer.h"
#include "spanner.h"
#include "staff.h"
#include "staffvisibilityindicator.h"
#include "style/style.h"
#include "system.h"
#include "systemdivider.h"
#include "systemlockindicator.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   ~SysStaff
//---------------------------------------------------------

SysStaff::~SysStaff() {
    for (auto &pair : instrument_names_) {
        if (InstrumentName *n = pair.second) {
            delete n;
        }
    }
}

//---------------------------------------------------------
//   yBottom
//---------------------------------------------------------

double SysStaff::y_bottom() const { return skyline().south().valid() ? skyline().south().max() : height_; }

//---------------------------------------------------------
//   saveLayout
//---------------------------------------------------------

void SysStaff::save_layout() {
    height_ = bbox().height();
    y_pos_ = bbox().y();
}

//---------------------------------------------------------
//   saveLayout
//---------------------------------------------------------

void SysStaff::restore_layout() {
    bbox().set_top(y_pos_);
    bbox().set_height(height_);
}

InstrumentName *SysStaff::name(InstrumentNameRole role) const {
    if (muse::contains(instrument_names_, role)) {
        return instrument_names_.at(role);
    }

    return nullptr;
}

void SysStaff::add_instrument_name(InstrumentName *n) {
    InstrumentNameRole role = n->instrument_name_role();
    if (InstrumentName *cur_name = name(role)) {
        delete cur_name;
    }

    instrument_names_[role] = n;
}

void SysStaff::remove_instrument_name(InstrumentNameRole role) {
    if (InstrumentName *n = name(role)) {
        delete n;
    }

    instrument_names_[role] = nullptr;
}

//---------------------------------------------------------
//   System
//---------------------------------------------------------

System::System(Score *parent) : EngravingItem(ElementType::SYSTEM, parent) {}

void System::set_ownership_parent(Score *score) { EngravingItem::set_ownership_parent(score); }

EngravingItem *System::layout_parent() const { return page_; }

//---------------------------------------------------------
//   ~System
//---------------------------------------------------------

System::~System() {
    if (page_) {
        muse::remove(page_->systems(), this);
    }
    for (SpannerSegment *ss : spanner_segments()) {
        if (ss->system() == this) {
            ss->set_system(nullptr);
        }
    }
    for (MeasureBase *mb : measures()) {
        if (mb->system() == this) {
            mb->set_system(nullptr);
        }
    }
    muse::delete_all(staves_);
    muse::delete_all(brackets_);
    muse::delete_all(system_lock_indicators_);
    if (staff_visibility_indicator_) {
        delete staff_visibility_indicator_;
    }
    if (page_lock_indicator_) {
        delete page_lock_indicator_;
    }
}

//---------------------------------------------------------
///   clear
///   Clear layout of System
//---------------------------------------------------------

void System::clear() {
    for (MeasureBase *mb : measures()) {
        if (mb->system() == this) {
            mb->set_system(nullptr);
        }
    }
    ml_.clear();
    for (SpannerSegment *ss : spanner_segments_) {
        if (ss->system() == this) {
            ss->set_system(nullptr);
        }
    }
    spanner_segments_.clear();
    // _systemDividers are reused
}

//---------------------------------------------------------
//   appendMeasure
//---------------------------------------------------------

void System::append_measure(MeasureBase *mb) {
    assert(!mb->is_measure() || !(style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && to_measure(mb)->has_mm_rest()));
    mb->set_system(this);
    ml_.push_back(mb);
}

//---------------------------------------------------------
//   removeMeasure
//---------------------------------------------------------

void System::remove_measure(MeasureBase *mb) {
    ml_.erase(std::remove(ml_.begin(), ml_.end(), mb), ml_.end());
    if (mb->system() == this) {
        mb->set_system(nullptr);
    }
}

//---------------------------------------------------------
//   removeLastMeasure
//---------------------------------------------------------

void System::remove_last_measure() {
    if (ml_.empty()) {
        return;
    }
    MeasureBase *mb = ml_.back();
    ml_.pop_back();
    if (mb->system() == this) {
        mb->set_system(nullptr);
    }
}

//---------------------------------------------------------
//   vbox
//    a system can only contain one vertical frame
//---------------------------------------------------------

Box *System::vbox() const {
    if (!ml_.empty() && ml_.front()->is_v_box_base()) {
        return to_box(ml_.front());
    }
    return nullptr;
}

//---------------------------------------------------------
//   insertStaff
//---------------------------------------------------------

SysStaff *System::insert_staff(int idx) {
    SysStaff *staff = new SysStaff;
    if (idx) {
        // HACK: guess position
        staff->bbox().set_top(staves_[idx - 1]->y() + 6 * spatium());
    }
    staves_.insert(staves_.begin() + idx, staff);
    return staff;
}

//---------------------------------------------------------
//   removeStaff
//---------------------------------------------------------

void System::remove_staff(int idx) { staves_.erase(staves_.begin() + idx); }

//---------------------------------------------------------
//   adjustStavesNumber
//---------------------------------------------------------

void System::adjust_staves_number(size_t nstaves) {
    for (size_t i = staves_.size(); i < nstaves; ++i) {
        insert_staff(static_cast<int>(i));
    }
    const size_t dn = staves_.size() - nstaves;
    for (size_t i = 0; i < dn; ++i) {
        remove_staff(static_cast<int>(staves_.size()) - 1);
    }
}

size_t System::get_brackets_columns_count() const {
    size_t columns = 0;
    for (const Staff *staff : score()->staves()) {
        for (auto bi : score()->brackets(staff->idx())) {
            columns = std::max(columns, bi->column() + 1);
        }
    }
    return columns;
}

void System::set_has_staff_visibility_indicator(bool has) {
    if (has && !staff_visibility_indicator_) {
        staff_visibility_indicator_ = Factory::create_staff_visibility_indicator(this);
    } else if (!has && staff_visibility_indicator_) {
        delete staff_visibility_indicator_;
        staff_visibility_indicator_ = nullptr;
    }
}

bool System::is_locked() const { return ml_.front()->is_start_of_system_lock(); }

const RangeLock *System::system_lock() const { return ml_.front()->system_lock(); }

void System::add_system_lock_indicator(SystemLockIndicator *sli) {
    assert(sli);
    system_lock_indicators_.push_back(sli);
}

void System::delete_system_lock_indicators() {
    muse::delete_all(system_lock_indicators_);
    system_lock_indicators_.clear();
}

void System::set_page_lock_indicator(PageLockIndicator *pli) { page_lock_indicator_ = pli; }

void System::delete_page_lock_indicator() {
    delete page_lock_indicator_;
    page_lock_indicator_ = nullptr;
}

//---------------------------------------------------------
//   nextVisibleStaff
//---------------------------------------------------------

staff_idx_t System::first_visible_staff_from(staff_idx_t start_staff_idx) const {
    for (staff_idx_t i = start_staff_idx; i < staves_.size(); ++i) {
        Staff *s = score()->staff(i);
        SysStaff *ss = staves_[i];

        if (s->show() && ss->show()) {
            return i;
        }
    }

    return muse::nidx;
}

staff_idx_t System::next_visible_staff(staff_idx_t staff_idx) const { return first_visible_staff_from(staff_idx + 1); }

staff_idx_t System::prev_visible_staff(staff_idx_t start_staff_idx) const {
    if (start_staff_idx == 0 || staves_.empty()) {
        return muse::nidx;
    }
    start_staff_idx = std::min(start_staff_idx, static_cast<staff_idx_t>(staves_.size()));

    for (staff_idx_t i = start_staff_idx - 1;; --i) {
        Staff *s = score()->staff(i);
        SysStaff *ss = staves_[i];

        if (s->show() && ss->show()) {
            return i;
        }

        if (i == 0) {
            break;
        }
    }

    return muse::nidx;
}

staff_idx_t System::last_visible_staff() const {
    const size_t nstaves = std::min(score()->nstaves(), staves_.size());
    if (nstaves < 1 || staves_.empty()) {
        return muse::nidx;
    }

    for (staff_idx_t i = nstaves - 1;; --i) {
        Staff *staff = score()->staff(i);
        SysStaff *sys_staff = staves_[i];
        if (staff->show() && sys_staff->show()) {
            return i;
        }
        if (i == 0) {
            break;
        }
    }

    return muse::nidx;
}

//---------------------------------------------------------
//   firstVisibleStaff
//---------------------------------------------------------

staff_idx_t System::first_visible_staff() const { return first_visible_staff_from(0); }

//---------------------------------------------------------
//   searchStaff
///   Finds a staff which y position is most close to the
///   given \p y.
///   \param y The y coordinate in system coordinates.
///   \param preferredStaff If not -1, will give more space
///   to a staff with the given number when searching it by
///   coordinate.
///   \returns Number of the found staff.
//---------------------------------------------------------

staff_idx_t System::search_staff(double y, staff_idx_t preferred_staff /* = invalid */, double spacing_factor) const {
    staff_idx_t i = 0;
    const size_t nstaves = score()->nstaves();
    for (; i < nstaves;) {
        SysStaff *stff = staff(i);
        if (!stff->show() || !score()->staff(i)->show()) {
            ++i;
            continue;
        }
        staff_idx_t ni = i;
        for (;;) {
            ++ni;
            if (ni == nstaves || (staff(ni)->show() && score()->staff(ni)->show())) {
                break;
            }
        }

        double sy2;
        if (ni != nstaves) {
            SysStaff *nstaff = staff(ni);
            double s1y2 = stff->bbox().y() + stff->bbox().height();
            if (i == preferred_staff) {
                sy2 = s1y2 + (nstaff->bbox().y() - s1y2);
            } else if (ni == preferred_staff) {
                sy2 = s1y2;
            } else {
                sy2 = s1y2 + (nstaff->bbox().y() - s1y2) * spacing_factor;
            }
        } else {
            sy2 = page()->height() - pos().y();
        }
        if (y > sy2) {
            i = ni;
            continue;
        }
        break;
    }
    return i;
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void System::add(EngravingItem *el) {
    if (!el) {
        return;
    }
    // LOGD("%p System::add: %p %s", this, el, el->typeName());

    if (el->is_measure_base()) {
        to_measure_base(el)->set_system(this);
    } else if (el->is_spanner_segment()) {
        to_spanner_segment(el)->set_system(this);
    } else if (!el->is_beam()) { // a beam's placement is derived from its elements
        el->set_ownership_parent(this);
    }

    switch (el->type()) {
        case ElementType::INSTRUMENT_NAME: {
            InstrumentName *n = to_instrument_name(el);
            SysStaff *sys_staff = staves_[n->staff_idx()];
            sys_staff->add_instrument_name(n);
            n->set_sys_staff(sys_staff);
            break;
        }
        case ElementType::BEAM: score()->add_element(el); break;

        case ElementType::BRACKET: {
            Bracket *b = to_bracket(el);
            brackets_.push_back(b);
        } break;

        case ElementType::MEASURE:
        case ElementType::HBOX:
        case ElementType::VBOX:
        case ElementType::TBOX:
        case ElementType::FBOX: score()->add_element(el); break;
        case ElementType::TEXTLINE_SEGMENT:
        case ElementType::HAIRPIN_SEGMENT:
        case ElementType::OTTAVA_SEGMENT:
        case ElementType::TRILL_SEGMENT:
        case ElementType::VIBRATO_SEGMENT:
        case ElementType::VOLTA_SEGMENT:
        case ElementType::SLUR_SEGMENT:
        case ElementType::TIE_SEGMENT:
        case ElementType::LAISSEZ_VIB_SEGMENT:
        case ElementType::PARTIAL_TIE_SEGMENT:
        case ElementType::PEDAL_SEGMENT:
        case ElementType::LYRICSLINE_SEGMENT:
        case ElementType::PARTIAL_LYRICSLINE_SEGMENT:
        case ElementType::GLISSANDO_SEGMENT:
        case ElementType::NOTELINE_SEGMENT:
        case ElementType::LET_RING_SEGMENT:
        case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
        case ElementType::PALM_MUTE_SEGMENT:
        case ElementType::WHAMMY_BAR_SEGMENT:
        case ElementType::RASGUEADO_SEGMENT:
        case ElementType::HARMONIC_MARK_SEGMENT:
        case ElementType::PICK_SCRAPE_SEGMENT:
        case ElementType::GUITAR_BEND_SEGMENT:
        case ElementType::GUITAR_BEND_HOLD_SEGMENT:
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT:
        case ElementType::TAPPING_HALF_SLUR_SEGMENT: {
            SpannerSegment *ss = to_spanner_segment(el);
#ifndef NDEBUG
            if (muse::contains(spanner_segments_, ss)) {
                LOGD("System::add() %s %p already there", ss->type_name(), ss);
            } else
#endif
                spanner_segments_.push_back(ss);
        } break;

        case ElementType::SYSTEM_DIVIDER: {
            SystemDivider *sd = to_system_divider(el);
            if (sd->divider_type() == SystemDividerType::LEFT) {
                system_divider_left_ = sd;
            } else {
                system_divider_right_ = sd;
            }
        } break;

        default: LOGD("System::add(%s) not implemented", el->type_name()); return;
    }

    el->added();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void System::remove(EngravingItem *el) {
    switch (el->type()) {
        case ElementType::INSTRUMENT_NAME: {
            // NOTE: el gets deleted here
            InstrumentName *n = to_instrument_name(el);
            SysStaff *sys_staff = staves_[n->staff_idx()];
            sys_staff->remove_instrument_name(n->instrument_name_role());
            return;
        }
        case ElementType::BEAM: score()->remove_element(el); break;
        case ElementType::BRACKET: {
            Bracket *b = to_bracket(el);
            if (!muse::remove(brackets_, b)) {
                LOGD("System::remove: bracket not found");
            }
        } break;
        case ElementType::MEASURE:
        case ElementType::HBOX:
        case ElementType::VBOX:
        case ElementType::TBOX:
        case ElementType::FBOX: score()->remove_element(el); break;
        case ElementType::TEXTLINE_SEGMENT:
        case ElementType::HAIRPIN_SEGMENT:
        case ElementType::OTTAVA_SEGMENT:
        case ElementType::TRILL_SEGMENT:
        case ElementType::VIBRATO_SEGMENT:
        case ElementType::VOLTA_SEGMENT:
        case ElementType::SLUR_SEGMENT:
        case ElementType::TIE_SEGMENT:
        case ElementType::LAISSEZ_VIB_SEGMENT:
        case ElementType::PARTIAL_TIE_SEGMENT:
        case ElementType::PEDAL_SEGMENT:
        case ElementType::LYRICSLINE_SEGMENT:
        case ElementType::PARTIAL_LYRICSLINE_SEGMENT:
        case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
        case ElementType::GLISSANDO_SEGMENT:
        case ElementType::NOTELINE_SEGMENT:
        case ElementType::GUITAR_BEND_SEGMENT:
        case ElementType::GUITAR_BEND_HOLD_SEGMENT: {
            SpannerSegment *ss = to_spanner_segment(el);
            if (!muse::remove(spanner_segments_, ss)) {
                LOGD("System::remove: %p(%s) not found, score %p", el, el->type_name(), score());
                assert(score() == el->score());
            }
            if (ss->system() == this) {
                ss->set_system(nullptr);
            }
        } break;
        case ElementType::SYSTEM_DIVIDER:
            if (el == system_divider_left_) {
                system_divider_left_ = 0;
            } else {
                assert(system_divider_right_ == el);
                system_divider_right_ = 0;
            }
            break;

        default: LOGD("System::remove(%s) not implemented", el->type_name()); return;
    }

    el->removed();
}

//---------------------------------------------------------
//   change
//---------------------------------------------------------

void System::change(EngravingItem *o, EngravingItem *n) {
    remove(o);
    add(n);
}

//---------------------------------------------------------
//   snap
//---------------------------------------------------------

Fraction System::snap(const Fraction &tick, const PointF p) const {
    for (const MeasureBase *m : ml_) {
        if (p.x() < m->x() + m->width()) {
            return to_measure(m)->snap(tick, p - m->pos()); //TODO: MeasureBase
        }
    }
    return to_measure(ml_.back())->snap(tick, p - pos()); //TODO: MeasureBase
}

//---------------------------------------------------------
//   snap
//---------------------------------------------------------

Fraction System::snap_note(const Fraction &tick, const PointF p, int staff) const {
    for (const MeasureBase *m : ml_) {
        if (p.x() < m->x() + m->width()) {
            return to_measure(m)->snap_note(tick, p - m->pos(), staff); //TODO: MeasureBase
        }
    }
    return to_measure(ml_.back())->snap(tick, p - pos()); // TODO: MeasureBase
}

//---------------------------------------------------------
//   firstMeasure
//---------------------------------------------------------

Measure *System::first_measure() const {
    auto i = std::find_if(ml_.begin(), ml_.end(), [](MeasureBase *mb) { return mb->is_measure(); });
    return i != ml_.end() ? to_measure(*i) : 0;
}

//---------------------------------------------------------
//   lastMeasure
//---------------------------------------------------------

Measure *System::last_measure() const {
    auto i = std::find_if(ml_.rbegin(), ml_.rend(), [](MeasureBase *mb) { return mb->is_measure(); });
    return i != ml_.rend() ? to_measure(*i) : 0;
}

//---------------------------------------------------------
//   nextMeasure
//---------------------------------------------------------

MeasureBase *System::next_measure(const MeasureBase *m) const {
    if (m == ml_.back()) {
        return 0;
    }
    MeasureBase *nm = m->next();
    if (nm->is_measure() && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && to_measure(nm)->has_mm_rest()) {
        nm = to_measure(nm)->mm_rest();
    }
    return nm;
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void System::scan_elements(std::function<void(EngravingItem *)> func) {
    if (page_lock_indicator_) {
        func(page_lock_indicator_);
    }

    if (vbox()) {
        return;
    }
    for (Bracket *b : brackets_) {
        b->scan_elements(func);
    }

    if (system_divider_left_) {
        func(system_divider_left_);
    }
    if (system_divider_right_) {
        func(system_divider_right_);
    }

    if (staff_visibility_indicator_) {
        func(staff_visibility_indicator_);
    }

    for (auto i : system_lock_indicators_) {
        func(i);
    }

    for (const SysStaff *st : staves_) {
        if (st->show()) {
            for (auto &pair : st->instrument_names()) {
                if (InstrumentName *n = pair.second) {
                    func(n);
                }
            }
        } else {
            if (InstrumentName *n = st->name(InstrumentNameRole::GROUP); n && n->effective_staff_idx() != muse::nidx) {
                func(n);
            }
            if (InstrumentName *n = st->name(InstrumentNameRole::PART); n && n->effective_staff_idx() != muse::nidx) {
                func(n);
            }
        }
    }

    for (SpannerSegment *ss : spanner_segments_) {
        staff_idx_t staff_idx = ss->spanner()->staff_idx();
        if (staff_idx == muse::nidx) {
            LOGD("System::scanElements: staffIDx == -1: %s %p", ss->spanner()->type_name(), ss->spanner());
            staff_idx = 0;
        }
        bool v = true;
        Spanner *spanner = ss->spanner();
        if (spanner->anchor() == Spanner::Anchor::SEGMENT || spanner->anchor() == Spanner::Anchor::CHORDREST) {
            EngravingItem *se = spanner->start_element();
            EngravingItem *ee = spanner->end_element();
            bool v1 = true;
            if (se && se->is_chord_rest()) {
                ChordRest *cr = to_chord_rest(se);
                Measure *m = cr->measure();
                v1 = m->visible(cr->staff_idx());
            }
            bool v2 = true;
            if (!v1 && ee && ee->is_chord_rest()) {
                ChordRest *cr = to_chord_rest(ee);
                Measure *m = cr->measure();
                v2 = m->visible(cr->staff_idx());
            }
            v = v1 || v2; // hide spanner if both chords are hidden
        }
        if ((score()->staff(staff_idx)->show() && staves_[staff_idx]->show() && v) || spanner->is_volta() || spanner->system_flag()) {
            ss->scan_elements(func);
        }
    }
}

//---------------------------------------------------------
//   staffYpage
//    return page coordinates
//---------------------------------------------------------

double System::staff_ypage(staff_idx_t staff_idx) const {
    if (staff_idx >= staves_.size()) {
        return page_pos().y();
    }

    return staves_[staff_idx]->y() + y();
}

//---------------------------------------------------------
//   staffCanvasYpage
//    return canvas coordinates
//---------------------------------------------------------

double System::staff_canvas_ypage(staff_idx_t staff_idx) const { return staves_[staff_idx]->y() + y() + page()->canvas_pos().y(); }

SysStaff *System::staff(size_t staff_idx) const {
    if (staff_idx < staves_.size()) {
        return staves_[staff_idx];
    }

    return nullptr;
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *System::next_segment_element() {
    Measure *m = first_measure();
    if (m) {
        Segment *first_seg = m->segments().first();
        if (first_seg) {
            return first_seg->element(0);
        }
    }
    return Navigation::last_element(score());
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *System::prev_segment_element() {
    EngravingItem *re = 0;
    Measure *m = first_measure();
    if (m) {
        Segment *seg = m->first();
        while (!re) {
            seg = seg->prev1_mm();
            if (!seg) {
                return Navigation::first_element(score());
            }

            if (seg->segment_type() == SegmentType::END_BAR_LINE) {
                score()->input_state().set_track((score()->staves().size() - 1) * ::mu::engraving::voices); //correction
            }
            re = seg->last_element_for_navigation(score()->staves().size() - 1);
        }
    }
    return re;
}

//---------------------------------------------------------
//   topDistance
//    return minimum distance to the above south skyline
//---------------------------------------------------------

double System::top_distance(staff_idx_t staff_idx, const SkylineLine &s) const {
    assert(!vbox());
    assert(!s.is_north());
    // in continuous view, we only build a partial skyline for performance reasons
    // this means we cannot expect the minDistance calculation to produce meaningful results
    // so just give up on autoplace for spanners in continuous view
    // (or any other calculations that rely on this value)
    if (score()->line_mode() && !configuration()->min_distance_for_partial_skyline_calculated()) {
        return 0.0;
    }
    return s.min_distance(staff(staff_idx)->skyline().north());
}

//---------------------------------------------------------
//   bottomDistance
//---------------------------------------------------------

double System::bottom_distance(staff_idx_t staff_idx, const SkylineLine &s) const {
    assert(!vbox());
    assert(s.is_north());
    // see note on topDistance() above
    if (score()->line_mode() && !configuration()->min_distance_for_partial_skyline_calculated()) {
        return 0.0;
    }
    return staff(staff_idx)->skyline().south().min_distance(s);
}

//---------------------------------------------------------
//   firstVisibleSysStaff
//---------------------------------------------------------

staff_idx_t System::first_visible_sys_staff() const {
    size_t nstaves = staves_.size();
    for (staff_idx_t i = 0; i < nstaves; ++i) {
        if (staves_[i]->show()) {
            return i;
        }
    }
    return muse::nidx;
}

//---------------------------------------------------------
//   lastVisibleSysStaff
//---------------------------------------------------------

staff_idx_t System::last_visible_sys_staff() const {
    int nstaves = static_cast<int>(staves_.size());
    for (int i = nstaves - 1; i >= 0; --i) {
        if (staves_[i]->show()) {
            return static_cast<staff_idx_t>(i);
        }
    }
    return muse::nidx;
}

//---------------------------------------------------------
//   minTop
//    Return the minimum top margin.
//---------------------------------------------------------

double System::min_top() const {
    staff_idx_t si = first_visible_sys_staff();
    SysStaff *s = si == muse::nidx ? nullptr : staff(si);
    if (s) {
        return -s->skyline().north().max();
    }
    return 0.0;
}

//---------------------------------------------------------
//   minBottom
//    Return the minimum bottom margin.
//---------------------------------------------------------

double System::min_bottom() const {
    if (const Box *vb = vbox()) {
        return vb->absolute_from_spatium(vb->bottom_gap());
    }
    staff_idx_t si = last_visible_sys_staff();
    SysStaff *s = si == muse::nidx ? nullptr : staff(si);
    if (s) {
        return s->skyline().south().max() - s->bbox().height();
    }
    return 0.0;
}

//---------------------------------------------------------
//   spacerDistance
//    Return the distance needed due to spacers
//---------------------------------------------------------

double System::spacer_distance(bool up) const {
    staff_idx_t staff = up ? first_visible_sys_staff() : last_visible_sys_staff();
    if (staff == muse::nidx) {
        return 0.0;
    }
    double dist = 0.0;
    for (MeasureBase *mb : measures()) {
        if (mb->is_measure()) {
            Measure *m = to_measure(mb);
            Spacer *sp = up ? m->vspacer_up(staff) : m->vspacer_down(staff);
            if (sp) {
                if (sp->spacer_type() == SpacerType::FIXED) {
                    dist = sp->absolute_gap();
                    break;
                } else {
                    dist = std::max(dist, sp->absolute_gap());
                }
            }
        }
    }
    return dist;
}

//---------------------------------------------------------
//   upSpacer
//    Return largest upSpacer for this system. This can
//    be a downSpacer of the previous system.
//---------------------------------------------------------

Spacer *System::up_spacer(staff_idx_t staff_idx, Spacer *prev_down_spacer) const {
    if (staff_idx == muse::nidx) {
        return nullptr;
    }

    if (prev_down_spacer && (prev_down_spacer->spacer_type() == SpacerType::FIXED)) {
        return prev_down_spacer;
    }

    Spacer *spacer{prev_down_spacer};
    for (MeasureBase *mb : measures()) {
        if (!(mb && mb->is_measure())) {
            continue;
        }
        Spacer *sp{to_measure(mb)->vspacer_up(staff_idx)};
        if (sp) {
            if (!spacer || ((spacer->spacer_type() == SpacerType::UP) && (sp->gap() > spacer->gap()))) {
                spacer = sp;
            }
            continue;
        }
    }
    return spacer;
}

//---------------------------------------------------------
//   downSpacer
//    Return the largest downSpacer for this system.
//---------------------------------------------------------

Spacer *System::down_spacer(staff_idx_t staff_idx) const {
    if (staff_idx == muse::nidx) {
        return nullptr;
    }

    Spacer *spacer{nullptr};
    for (MeasureBase *mb : measures()) {
        if (!(mb && mb->is_measure())) {
            continue;
        }
        Spacer *sp{to_measure(mb)->vspacer_down(staff_idx)};
        if (sp) {
            if (sp->spacer_type() == SpacerType::FIXED) {
                return sp;
            } else {
                if (!spacer || (sp->gap() > spacer->gap())) {
                    spacer = sp;
                }
            }
        }
    }
    return spacer;
}

//---------------------------------------------------------
//   firstNoteRestSegmentX
//    in System() coordinates
//    returns the position of the first note or rest,
//    or the position just after the last non-chordrest segment
//---------------------------------------------------------

double System::first_note_rest_segment_x(bool leading) const {
    double margin = style().style_absolute(Sid::HEADER_TO_LINE_START_DISTANCE);
    for (const MeasureBase *mb : measures()) {
        if (mb->is_measure()) {
            const Measure *measure = to_measure(mb);
            margin = measure->first_note_rest_segment_x(leading);
            break;
        }
    }

    return margin;
}

//---------------------------------------------------------
//   endingXForOpenEndedLines
//    in System() coordinates returns the end point for
//    open ended (i.e. cross-system) lines
//---------------------------------------------------------

double System::ending_x_for_open_ended_lines() const {
    double margin = style().style_absolute(Sid::LINE_END_TO_BARLINE_DISTANCE);
    double system_end_x = ldata()->bbox().width();

    Measure *last_meas = last_measure();
    if (!last_meas) {
        return system_end_x - margin;
    }

    return last_meas->ending_x_for_open_ended_lines();
}

//---------------------------------------------------------
//   lastChordRest
//    returns the last chordrest of a system for a particular track
//---------------------------------------------------------

ChordRest *System::last_chord_rest(track_idx_t track) const {
    for (auto measure_base_iter = measures().rbegin(); measure_base_iter != measures().rend(); measure_base_iter++) {
        if ((*measure_base_iter)->is_measure()) {
            const Measure *measure = to_measure(*measure_base_iter);
            return measure->last_chord_rest(track);
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   firstChordRest
//    returns the last chordrest of a system for a particular track
//---------------------------------------------------------

ChordRest *System::first_chord_rest(track_idx_t track) const {
    for (const MeasureBase *mb : measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        const Measure *measure = to_measure(mb);
        return measure->first_chord_rest(track);
    }
    return nullptr;
}

//---------------------------------------------------------
//   pageBreak
//---------------------------------------------------------

bool System::page_break() const { return ml_.empty() ? false : ml_.back()->page_break(); }

//---------------------------------------------------------
//   endTick
//---------------------------------------------------------

Fraction System::end_tick() const { return measures().back()->end_tick(); }

//---------------------------------------------------------
//   firstSysStaffOfPart
//---------------------------------------------------------

staff_idx_t System::first_sys_staff_of_part(const Part *part) const {
    staff_idx_t staff_idx = 0;
    for (const Part *p : score()->parts()) {
        if (p == part) {
            return staff_idx;
        }
        staff_idx += p->nstaves();
    }
    return muse::nidx; // Part not found.
}

//---------------------------------------------------------
//   firstVisibleSysStaffOfPart
//---------------------------------------------------------

staff_idx_t System::first_visible_sys_staff_of_part(const Part *part) const {
    staff_idx_t first_idx = first_sys_staff_of_part(part);
    if (first_idx == muse::nidx) {
        return muse::nidx;
    }

    for (staff_idx_t idx = first_idx; idx < first_idx + part->nstaves(); ++idx) {
        const SysStaff *s = staff(idx);
        if (s && s->show()) {
            return idx;
        }
    }

    return muse::nidx; // No visible staves on this part.
}

staff_idx_t System::first_visible_sys_staff_with_instrument(const String &instrument_id, staff_idx_t start_from) {
    Fraction tick = first()->tick();
    for (staff_idx_t idx = start_from; idx < staves_.size(); ++idx) {
        Part *part = score()->staff(idx)->part();
        if (part->instrument(tick)->id() == instrument_id) {
            staff_idx_t first_vis_of_part = first_visible_sys_staff_of_part(part);
            if (first_vis_of_part != muse::nidx) {
                return first_vis_of_part;
            }
        }
    }

    return muse::nidx;
}

//---------------------------------------------------------
//   lastSysStaffOfPart
//---------------------------------------------------------

staff_idx_t System::last_sys_staff_of_part(const Part *part) const {
    staff_idx_t first_idx = first_sys_staff_of_part(part);
    if (first_idx == muse::nidx) {
        return muse::nidx; // Part not found.
    }
    return first_idx + part->nstaves() - 1;
}

//---------------------------------------------------------
//   lastVisibleSysStaffOfPart
//---------------------------------------------------------

staff_idx_t System::last_visible_sys_staff_of_part(const Part *part) const {
    staff_idx_t first_staff_idx = first_sys_staff_of_part(part);
    if (first_staff_idx == muse::nidx) {
        return muse::nidx;
    }
    for (int idx = static_cast<int>(last_sys_staff_of_part(part)); idx >= static_cast<int>(first_staff_idx); --idx) {
        if (staff(idx)->show()) {
            return idx;
        }
    }
    return muse::nidx; // No visible staves on this part.
}

std::vector<staff_idx_t> System::visible_staves_of_part(const Part *part) const {
    std::vector<staff_idx_t> result;
    result.reserve(part->nstaves());

    staff_idx_t start_idx = first_sys_staff_of_part(part);
    staff_idx_t end_idx = start_idx + part->nstaves();
    for (staff_idx_t staff_idx = start_idx; staff_idx < end_idx; ++staff_idx) {
        if (staff(staff_idx)->show()) {
            result.push_back(staff_idx);
        }
    }

    return result;
}

std::vector<Part *> System::visible_parts_of_group(staff_idx_t start, staff_idx_t end) const {
    std::vector<Part *> result;

    for (staff_idx_t idx = start; idx < end;) {
        Part *part = score()->staff(idx)->part();
        if (visible_staves_of_part(part).size() > 0) {
            result.push_back(part);
        }
        idx += part->nstaves();
    }

    return result;
}
} // namespace mu::engraving
