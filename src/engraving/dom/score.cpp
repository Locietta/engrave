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
 Implementation of class Score (partial).
*/

#include "score.h"

#include <cmath>
#include <map>

#include "anchors.h"
#include "beam.h"
#include "box.h"
#include "bracket.h"
#include "capo.h"
#include "chord.h"
#include "clef.h"
#include "containers.h"
#include "dummyparent.h"
#include "editing/addremoveelement.h"
#include "editing/editclef.h"
#include "editing/editkeysig.h"
#include "editing/editstaffbrackets.h"
#include "editing/editstavesharing.h"
#include "editing/splitjoinmeasure.h"
#include "editing/transaction/transaction.h"
#include "editing/transaction/undostack.h"
#include "editing/transpose.h"
#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "engraving/internal/engravingfont.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "excerpt.h"
#include "factory.h"
#include "fret.h"
#include "glissando.h"
#include "guitarbend.h"
#include "harmony.h"
#include "imageStore.h"
#include "instrchange.h"
#include "instrtemplate.h"
#include "internal/engravingcontextconfiguration.h" // IWYU pragma: keep
#include "key.h"
#include "keysig.h"
#include "linkedobjects.h"
#include "log.h"
#include "lyrics.h"
#include "masterscore.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "ottava.h"
#include "page.h"
#include "palmmute.h"
#include "part.h"
#include "partialtie.h"
#include "pitchspelling.h"
#include "rehearsalmark.h"
#include "repeatlist.h"
#include "rest.h"
#include "scoreorder.h"
#include "segment.h"
#include "select.h"
#include "sig.h"
#include "slur.h"
#include "staff.h"
#include "stafftextbase.h"
#include "stafftype.h"
#include "style/defaultstyle.h"
#include "style/style.h"
#include "synthesizerstate.h"
#include "system.h"
#include "systemdivider.h"
#include "tempotext.h"
#include "tempotimeline.h"
#include "text.h"
#include "tie.h"
#include "tiemap.h"
#include "timesig.h"
#include "tuplet.h"
#include "types/translatablestring.h"
#include "types/typesconv.h"
#include "utils.h"
#include "volta.h"

using namespace mu;
using namespace muse;
using namespace mu::engraving;

namespace mu::engraving {
std::set<Score *> Score::valid_scores;

bool Score::is_alive(const Score *score) { return valid_scores.find(const_cast<Score *>(score)) != valid_scores.end(); }

bool no_seq = false;
bool no_midi = false;
bool midi_input_trace = false;
bool midi_output_trace = false;

static void mark_instruments_as_primary(std::vector<Part *> &parts) {

    std::unordered_map<String /*instrumentId*/, int /*count*/> instrument_count;

    for (Part *part : parts) {
        Instrument *instrument = part->instrument();
        if (!instrument) {
            continue;
        }

        auto it = instrument_count.find(instrument->id());
        if (it == instrument_count.cend()) {
            it = instrument_count.insert(instrument_count.begin(), {instrument->id(), 0});
        }

        it->second++;

        bool is_primary = (it->second % 2 != 0);
        instrument->set_is_primary(is_primary);
    }
}

//---------------------------------------------------------
//   Score
//---------------------------------------------------------

Score::Score(const EngravingServices &services) : EngravingObject(ElementType::SCORE, nullptr), services_(services), selection_(this) {
    Score::valid_scores.insert(this);
    master_score_ = nullptr;

    engraving_font_ = engraving_fonts()->font_by_name("Leland");

    file_division_ = Constants::division;
    style_ = DefaultStyle::default_style();

    dummy_ = new DummyParent(this);

    update_padding_tables();
}

Score::Score(MasterScore *parent, bool force_part_style /* = true */) : Score{parent->services()} {
    Score::valid_scores.insert(this);
    master_score_ = parent;

    if (DefaultStyle::default_style_for_parts()) {
        style_ = *DefaultStyle::default_style_for_parts();
    } else {
        // inherit most style settings from parent
        style_ = parent->style();

        static const Sid styles[] = {Sid::PAGE_WIDTH,
                                     Sid::PAGE_HEIGHT,
                                     Sid::PAGE_PRINTABLE_WIDTH,
                                     Sid::PAGE_EVEN_LEFT_MARGIN,
                                     Sid::PAGE_ODD_LEFT_MARGIN,
                                     Sid::PAGE_EVEN_TOP_MARGIN,
                                     Sid::PAGE_EVEN_BOTTOM_MARGIN,
                                     Sid::PAGE_ODD_TOP_MARGIN,
                                     Sid::PAGE_ODD_BOTTOM_MARGIN,
                                     Sid::PAGE_TWOSIDED,
                                     Sid::SPATIUM};
        // but borrow defaultStyle page layout settings
        for (auto i : styles) {
            style_.set(i, DefaultStyle::default_style().value(i));
        }
        // and force some style settings that just make sense for parts
        if (force_part_style) {
            style().set(Sid::CONCERT_PITCH, false);
            style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
            style().set(Sid::DIVIDER_LEFT, false);
            style().set(Sid::DIVIDER_RIGHT, false);
        }
    }
    // update style values
    style_.precompute_values();
    check_chord_list();
    synthesizer_state_ = parent->synthesizer_state_;
    msc_version_ = parent->msc_version_;
    update_padding_tables();
}

Score::Score(MasterScore *parent, const MStyle &s) : Score{parent} {
    Score::valid_scores.insert(this);
    style_ = s;
    update_padding_tables();
}

//---------------------------------------------------------
//   ~Score
//---------------------------------------------------------

Score::~Score() {
    // The register lives on the master score, which outlives an excerpt unless the
    // whole document is going away. An excerpt removed from the document must not
    // leave its id behind: a later allocation at the same address would inherit it.
    if (master_score_ && master_score_ != this && Score::is_alive(master_score_)) {
        master_score_->eid_register()->on_item_destroyed(this);
    }

    Score::valid_scores.erase(this);

    // deselectAll();
    muse::delete_all(systems_); // systems are layout-only objects so we delete
    // them prior to measures.
    for (MeasureBase *m = measures_.first(); m;) {
        MeasureBase *nm = m->next();
        if (m->is_measure() && to_measure(m)->mm_rest()) {
            delete to_measure(m)->mm_rest();
        }
        delete m;
        m = nm;
    }

    spanner_.clear();

    muse::delete_all(system_locks_.all_locks());
    system_locks_.clear();

    muse::delete_all(page_locks_.all_locks());
    page_locks_.clear();

    muse::delete_all(parts_);
    parts_.clear();

    muse::delete_all(staves_);
    staves_.clear();

    muse::delete_all(pages_);
    pages_.clear();

    for (auto &bracket_list : brackets_) {
        muse::delete_all(bracket_list);
    }

    master_score_ = nullptr;

    image_store.clear_unused();

    // last: everything that is deleted above may park children on it on its way out
    delete dummy_;
    dummy_ = nullptr;
}

//---------------------------------------------------------
//   Score::clone
//         To create excerpt clone to show when changing PageSettings
//         Use MasterScore::clone() instead
//---------------------------------------------------------

Score *Score::clone() {
    // TODO: see comments regarding setting version in corresponding code in 3.x branch
    // and also compare to MasterScore::clone()
    Excerpt *excerpt = new Excerpt(master_score());
    excerpt->set_name(name());

    TracksMap tracks;

    for (Part *part : parts_) {
        excerpt->parts().push_back(part);

        const TrackRange range = part->track_range();
        for (track_idx_t track = range.start_track; track < range.end_track; ++track) {
            tracks.insert({track, track});
        }
    }

    excerpt->set_tracks_mapping(tracks);

    master_score()->init_and_add_excerpt(excerpt, true);
    master_score()->remove_excerpt(excerpt);

    return excerpt->excerpt_score();
}

//! A bracket is a rendering of its bracket item, rebuilt from it on every layout, so
//! one whose item is gone has nothing left to represent.
static void on_bracket_item_destruction(Score *score, const BracketItem *item) {
    for (System *system : score->systems()) {
        std::vector<Bracket *> &brackets = system->brackets();
        for (auto it = brackets.begin(); it != brackets.end();) {
            Bracket *bracket = *it;
            if (bracket && bracket->bracket_item() == item) {
                it = brackets.erase(it);
                delete bracket;
            } else {
                ++it;
            }
        }
    }
}

//---------------------------------------------------------
//   Score::onElementDestruction
//    Ensure correct state of the score after destruction
//    of the element (e.g. remove invalid pointers etc.).
//---------------------------------------------------------

void Score::register_layout_copy(const EngravingItem &source, EngravingItem &display) {
    display.set_layout_copy(true);
    std::erase_if(layout_copies_, [&](const auto &entry) { return entry.first == &source; });
    layout_copies_.emplace_back(&source, &display);
    // Composite annotations (e.g. a fret diagram's harmony) keep their own IDs.
    // Pair the clone's source children in the order maintained by clone().
    const auto &source_children = source.children();
    const auto &display_children = display.children();
    auto next = display_children.begin();
    for (const auto *child : source_children) {
        if (!child->is_engraving_item()) continue;
        while (next != display_children.end() && (*next)->type() != child->type()) ++next;
        if (next == display_children.end()) break;
        register_layout_copy(*to_engraving_item(child), *to_engraving_item(*next++));
    }
}

const EngravingItem *Score::layout_copy(const EngravingItem &source) const {
    const auto found = std::ranges::find_if(layout_copies_, [&](const auto &entry) { return entry.first == &source; });
    return found != layout_copies_.end() ? found->second : nullptr;
}

void Score::on_element_destruction(EngravingItem *e) {
    Score *score = e->EngravingObject::score();

    if (!score) {
        return;
    }

    // The EID register lives on the master score, so unregister the item even when
    // its own score is being deleted (e.g. when an excerpt is removed), as long as
    // the master score, and with it the register, is still alive
    MasterScore *master_score = score->master_score();
    if (master_score && Score::valid_scores.find(master_score) != Score::valid_scores.end()) {
        master_score->eid_register()->on_item_destroyed(e);
    }

    if (Score::valid_scores.find(score) == Score::valid_scores.end()) {
        // The score is already deleted
        return;
    }

    // Keys are only compared, never dereferenced. A removed source can outlive
    // its display copy until the next layout; neither may leave a stale mapping.
    std::erase_if(score->layout_copies_, [e](const auto &entry) { return entry.first == e || entry.second == e; });

    if (e->is_bracket_item()) {
        on_bracket_item_destruction(score, to_bracket_item(e));
    }

    score->selection().remove(e);
    score->cmd_state().unset_element(e);
}

//---------------------------------------------------------
//   addMeasure
//---------------------------------------------------------

void Score::add_measure(MeasureBase *m, MeasureBase *pos) {
    m->set_next(pos);
    measures_.add(m);
}

//---------------------------------------------------------
//    updateTicksAndTimeSigMap
//    update:
//      - measure ticks
//      - time signature map
//---------------------------------------------------------

/**
 This is needed after
      - inserting or removing a measure
      - changing the sigmap
      - after inserting/deleting time (changes the sigmap)
*/

void Score::update_ticks_and_time_sig_map(bool sync_mm_rests) {

    Fraction tick = Fraction(0, 1);
    Measure *fm = first_measure();
    if (!fm) {
        for (Staff *staff : staves_) staff->clear_time_sig();
        if (is_master()) sigmap()->clear();
        measures_.update_tick_index();
        need_update_ticks_and_time_sig_map_ = false;
        return;
    }

    for (Staff *staff : staves_) {
        staff->clear_time_sig();
    }

    if (is_master()) {
        sigmap()->clear();
        sigmap()->add(0, SigEvent(fm->ticks(), fm->timesig(), 0));
    }

    for (MeasureBase *mb = first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) {
            mb->set_tick(tick);
            continue;
        }
        Measure *m = to_measure(mb);
        Fraction mtick = m->tick();
        Fraction diff = tick - mtick;
        Fraction measure_ticks = m->ticks();
        m->move_ticks(diff);
        if (sync_mm_rests && m->mm_rest()) {
            m->mm_rest()->move_ticks(diff);
        }

        rebuild_time_sig_map(m);

        tick += measure_ticks;
    }

    measures_.update_tick_index();
    need_update_ticks_and_time_sig_map_ = false;
}

//---------------------------------------------------------
//    rebuildTimeSigMap
///    updates the time signature map for a measure
//---------------------------------------------------------

void Score::rebuild_time_sig_map(Measure *measure) {
    for (Segment &segment : measure->segments()) {
        if (!segment.is_time_sig_type()) {
            continue;
        }
        for (size_t staff_idx = 0; staff_idx < staves_.size(); ++staff_idx) {
            TimeSig *ts = to_time_sig(segment.element(staff_idx * ::mu::engraving::voices));
            if (ts) {
                staff(staff_idx)->add_time_sig(ts);
            }
        }
    }

    if (!is_master()) {
        return;
    }

    // create event if measure len and time signature are different
    // even if they are equivalent 4/4 vs 2/2
    // also check if nominal time signature has changed

    const Fraction m_ticks = measure->is_mm_rest() ?
                                 measure->mm_rest_first()->ticks() :
                                 measure->ticks(); // for time signature the underlying measure length matters for MM rests

    const Measure *pm = measure->prev_measure();
    // prevMeasure() doesn't return MM rest so we don't handle it here

    if (pm && (!m_ticks.identical(pm->ticks()) || !measure->timesig().identical(pm->timesig()))) {
        sigmap()->add(measure->tick().ticks(), SigEvent(m_ticks, measure->timesig(), measure->measure_number()));
    }
}

//---------------------------------------------------------
//   pos2measureBase
//     Return measure base for canvas relative position \a p.
//---------------------------------------------------------

MeasureBase *Score::pos2measure_base(const PointF &p, bool scan_measures_only, staff_idx_t *staff_idx, int *pitch, Segment **seg,
                                     PointF *offset) const {
    MeasureBase *mb = search_measure_base(p, scan_measures_only);
    if (!mb) {
        return nullptr;
    }
    if (!mb->is_measure()) {
        //! NOTE: staffIdx, pitch, and seg are only relevant for Measure types...
        if (staff_idx) {
            *staff_idx = muse::nidx;
        }
        if (pitch) {
            *pitch = -1;
        }
        if (seg) {
            *seg = nullptr;
        }
        if (offset) {
            // TODO: Relevant?
        }
        return mb;
    }

    Measure *m = to_measure(mb);
    if (!m) {
        return nullptr;
    }

    const System *system = m->system();
    const double y = p.y() - system->canvas_pos().y();

    const staff_idx_t staff_idx_result = system->search_staff(y);

    // search for segment + offset
    const PointF offset_result = p - m->canvas_pos();
    const staff_idx_t strack = staff_idx_result * ::mu::engraving::voices;
    if (!staff(staff_idx_result)) {
        return nullptr;
    }

    // int etrack = staff(i)->part()->nstaves() * VOICES + strack;
    track_idx_t etrack = ::mu::engraving::voices + strack;

    constexpr SegmentType st = SegmentType::CHORD_REST;
    Segment *segment = m->search_segment(offset_result.x(), st, strack, etrack);
    if (!segment) {
        return nullptr;
    }

    const SysStaff *sys_staff = m->system()->staff(staff_idx_result);
    if (staff_idx) {
        *staff_idx = staff_idx_result;
    }
    if (pitch) {
        const Staff *s1 = staves_[staff_idx_result];
        const Fraction tick = segment->tick();
        const ClefType clef = s1->clef(tick);
        *pitch = y2pitch(offset_result.y() - sys_staff->bbox().y(), clef, s1->spatium(tick));
    }
    if (offset) {
        *offset = offset_result - PointF(segment->x(), sys_staff->bbox().y());
    }
    if (seg) {
        *seg = segment;
    }

    return m;
}

Measure *Score::pos2measure(const PointF &p, staff_idx_t *staff_idx, int *pitch, Segment **seg, PointF *offset) const {
    MeasureBase *mb = pos2measure_base(p, /*scanMeasuresOnly*/ true, staff_idx, pitch, seg, offset);
    return mb && mb->is_measure() ? to_measure(mb) : nullptr;
}

//---------------------------------------------------------
//   dragPosition
///   \param p   drag position in canvas coordinates
///   \param rst \b input: current staff index \n
///              \b output: new staff index for drag position
///   \param seg \b input: current segment \n
///              \b output: new segment for drag position
//---------------------------------------------------------

void Score::drag_position(const PointF &pos, staff_idx_t *rst, Segment **seg, double spacing_factor, bool allow_time_anchor) const {
    Measure *m = nullptr;

    if (!drag_position_to_measure(pos, this, &m, rst, spacing_factor)) {
        return;
    }

    drag_position_to_segment(pos, m, *rst, seg, spacing_factor, allow_time_anchor);
}

//---------------------------------------------------------
//   setShowInvisible
//---------------------------------------------------------

void Score::set_show_invisible(bool v) {
    show_invisible_ = v;
    // BSP tree does not include elements which are not
    // displayed, so we need to refresh it to get
    // invisible elements displayed or properly hidden.
    rebuild_bsp_tree();
}

//---------------------------------------------------------
//   setShowUnprintable
//---------------------------------------------------------

void Score::set_show_unprintable(bool v) { show_unprintable_ = v; }

//---------------------------------------------------------
//   setShowFrames
//---------------------------------------------------------

void Score::set_show_frames(bool v) { show_frames_ = v; }

//---------------------------------------------------------
//   setShowPageborders
//---------------------------------------------------------

void Score::set_show_pageborders(bool v) { show_pageborders_ = v; }

void Score::set_show_sound_flags(bool v) {
    if (show_sound_flags_ == v) {
        return;
    }

    show_sound_flags_ = v;
    set_layout_all();
}

//---------------------------------------------------------
//   setMarkIrregularMeasures
//---------------------------------------------------------

void Score::set_mark_irregular_measures(bool v) { mark_irregular_measures_ = v; }

void Score::set_show_anchors(const ShowAnchors &show_anchors) { show_anchors_ = show_anchors; }

//---------------------------------------------------------
//   readOnly
//---------------------------------------------------------

bool Score::read_only() const { return master_score_->read_only(); }

//---------------------------------------------------------
//   dirty
//---------------------------------------------------------

bool Score::dirty() const { return !undo_stack()->is_clean(); }

void Score::invalidate_repeat_list() { master_score()->invalidate_repeat_list(); }

bool Score::is_open() const { return is_open_; }

void Score::set_is_open(bool open) { is_open_ = open; }

//---------------------------------------------------------
//   searchPage
//    p is in canvas coordinates
//---------------------------------------------------------

Page *Score::search_page(const PointF &p) const {

    for (Page *page : pages_) {
        RectF r = page->ldata()->bbox().translated(page->pos());
        if (r.contains(p)) {
            return page;
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   searchSystem
///   Returns list of systems as there may be more than
///   one system in a row
///   \param pos Position in canvas coordinates
///   \param preferredSystem If not nullptr, will give more
///   space to the given system when searching it by its
///   coordinate.
///   \returns List of found systems.
//---------------------------------------------------------

std::vector<System *> Score::search_system(const PointF &pos, bool include_measureless_systems, const System *preferred_system,
                                           double spacing_factor, double preferred_spacing_factor) const {
    std::vector<System *> systems;
    Page *page = search_page(pos);
    if (!page) {
        return systems;
    }
    double y = pos.y() - page->pos().y(); // transform to page relative
    const std::vector<System *> &sl = page->systems();
    double y2;
    size_t n = sl.size();
    for (size_t i = 0; i < n; ++i) {
        System *s = sl.at(i);
        if (!s->first() || (!include_measureless_systems && !s->first_measure())) {
            continue;
        }
        System *ns = 0; // next system row
        size_t ii = i + 1;
        for (; ii < n; ++ii) {
            ns = sl.at(ii);
            if (ns->y() != s->y()) {
                break;
            }
        }
        if ((ii == n) || (ns == 0)) {
            y2 = page->height();
        } else {
            double current_spacing_factor;
            double sy2 = s->y() + s->ldata()->bbox().height();
            if (s == preferred_system) {
                current_spacing_factor = preferred_spacing_factor; //y2 = ns->y();
            } else if (ns == preferred_system) {
                current_spacing_factor = 1.0 - preferred_spacing_factor; //y2 = sy2;
            } else {
                current_spacing_factor = spacing_factor;
            }
            y2 = sy2 + (ns->y() - sy2) * current_spacing_factor;
        }
        if (y < y2) {
            systems.push_back(s);
            for (size_t iii = i + 1; iii < n; ++iii) {
                if (sl.at(iii)->y() != s->y()) {
                    break;
                }
                systems.push_back(sl.at(iii));
            }
            return systems;
        }
    }
    return systems;
}

//---------------------------------------------------------
//   searchMeasure
///   \param p Position in canvas coordinates
///   \param preferredSystem If not nullptr, will give more
///   space to measures in this system when searching.
//---------------------------------------------------------

MeasureBase *Score::search_measure_base(const PointF &p, bool scan_measures_only, const System *preferred_system, double spacing_factor,
                                        double preferred_spacing_factor) const {
    std::vector<System *> systems =
        search_system(p, /*includeMeasurelessSystems*/ !scan_measures_only, preferred_system, spacing_factor, preferred_spacing_factor);
    MeasureBase *last_mb = nullptr;
    for (System *system : systems) {
        const double x = p.x() - system->canvas_pos().x();
        for (MeasureBase *mb : system->measures()) {
            if (scan_measures_only && !mb->is_measure()) {
                continue;
            }
            if (x < (mb->x() + mb->ldata()->bbox().width())) {
                return mb;
            }
            last_mb = mb;
        }
    }
    return last_mb;
}

//---------------------------------------------------------
//    getNextValidInputSegment
//    - segment is of type SegmentType::ChordRest
//---------------------------------------------------------

static Segment *get_next_valid_input_segment(Segment *segment, track_idx_t track, voice_idx_t voice) {
    if (!segment) {
        return nullptr;
    }

    assert(segment->segment_type() == SegmentType::CHORD_REST);

    Fraction next_tick = segment->measure()->tick();
    for (Segment *s1 = segment; s1; s1 = s1->prev(SegmentType::CHORD_REST)) {
        if (EngravingItem *element = s1->element(track + voice)) {
            next_tick = to_chord_rest(element)->end_tick();
            break;
        }
    }

    for (; segment; segment = segment->next(SegmentType::CHORD_REST)) {
        if (segment->element(track + voice) || (voice && segment->tick() == next_tick)) {
            break;
        }
    }

    return segment;
}

//---------------------------------------------------------
//   getPosition
//    return true if valid position found
//---------------------------------------------------------

bool Score::get_position(Position *pos, const PointF &p, voice_idx_t voice) const {
    System *preferred_system = nullptr;
    staff_idx_t preferred_staff_idx = muse::nidx;
    const double spacing_factor = 0.5;
    const double preferred_spacing_factor = 0.75;
    if (note_entry_mode() && input_state().staff_group() != StaffGroup::TAB) {
        // for non-tab staves, prefer the current system & staff
        // this makes it easier to add notes far above or below the staff
        // not helpful for tab since notes are not entered above or below
        Segment *seg = input_state().segment();
        if (seg) {
            preferred_system = seg->system();
        }
        track_idx_t track = input_state().track();
        if (track != muse::nidx) {
            preferred_staff_idx = track >> 2;
        }
    }

    //! NOTE: For Positions we only care about Measure types, we're not interested in all MeasureBases...
    MeasureBase *mb = search_measure_base(p, /*scanMeasuresOnly*/ true, preferred_system, spacing_factor, preferred_spacing_factor);
    Measure *measure = mb && mb->is_measure() ? to_measure(mb) : nullptr;
    if (!measure) {
        return false;
    }

    pos->fret = ::mu::engraving::invalid_fret_index;
    //
    //    search staff
    //
    pos->staff_idx = 0;
    SysStaff *sstaff = 0;
    System *system = measure->system();
    double y = p.y() - system->page_pos().y();
    for (; pos->staff_idx < nstaves(); ++pos->staff_idx) {
        Staff *st = staff(pos->staff_idx);
        if (!st->part()->show()) {
            continue;
        }
        double sy2;
        SysStaff *ss = system->staff(pos->staff_idx);
        if (!ss->show()) {
            continue;
        }
        staff_idx_t idx = muse::nidx;
        SysStaff *nstaff = 0;

        // find next visible staff
        for (staff_idx_t i = pos->staff_idx + 1; i < nstaves(); ++i) {
            Staff *sti = staff(i);
            if (!sti->part()->show()) {
                continue;
            }
            nstaff = system->staff(i);
            if (!nstaff->show()) {
                nstaff = 0;
                continue;
            }
            if (i == preferred_staff_idx) {
                idx = i;
            }
            break;
        }

        if (nstaff) {
            double current_spacing_factor;
            if (pos->staff_idx == preferred_staff_idx) {
                current_spacing_factor = preferred_spacing_factor;
            } else if (idx == preferred_staff_idx) {
                current_spacing_factor = 1.0 - preferred_spacing_factor;
            } else {
                current_spacing_factor = spacing_factor;
            }
            double s1y2 = ss->bbox().bottom();
            sy2 = system->page()->canvas_pos().y() + s1y2 + (nstaff->bbox().y() - s1y2) * current_spacing_factor;
        } else {
            sy2 = system->page()->canvas_pos().y() + system->page()->height() - system->page_pos().y(); // system->height();
        }
        if (y < sy2) {
            sstaff = ss;
            break;
        }
    }
    if (sstaff == 0) {
        return false;
    }

    //
    //    search segment
    //
    PointF pppp(p - measure->canvas_pos());
    double x = pppp.x();
    Segment *segment = 0;
    pos->segment = 0;
    pos->beyond_score = false;

    // int track = pos->staffIdx * VOICES + voice;
    track_idx_t track = pos->staff_idx * ::mu::engraving::voices;

    for (segment = measure->first(SegmentType::CHORD_REST); segment;) {
        segment = get_next_valid_input_segment(segment, track, voice);
        if (segment == 0) {
            break;
        }
        Segment *ns = get_next_valid_input_segment(segment->next(SegmentType::CHORD_REST), track, voice);

        double x1 = segment->x();
        if (!ns) {
            if (measure == score()->last_measure() && x > measure->ldata()->bbox().width()) {
                x = measure->ldata()->bbox().width() + score()->style().spatium();
                pos->beyond_score = true;
            } else {
                x = x1;
            }
            pos->segment = segment;
            break;
        }

        double x2 = ns->x();
        double d = x2 - x1;
        if (x < (x1 + d * .5)) {
            x = x1;
            pos->segment = segment;
            break;
        }
        segment = ns;
    }
    if (segment == 0) {
        return false;
    }
    //
    // TODO: restrict to reasonable values (pitch 0-127)
    //
    const Staff *s = staff(pos->staff_idx);
    const Fraction tick = segment->tick();
    const double mag = s->staff_mag(tick);
    // in TABs, step from one string to another; in other staves, step on and between lines
    double line_dist = s->staff_type(tick)->line_distance().val() * (s->is_tab_staff(measure->tick()) ? 1 : .5) * mag * style().spatium();

    const double y_off = sstaff->y_offset(); // Get system staff vertical offset (usually for 1-line staves)
    pos->line = lrint((pppp.y() - sstaff->bbox().y() - y_off) / line_dist);
    if (s->is_tab_staff(measure->tick())) {
        if (pos->line < -1 || pos->line > s->lines(tick) + 1) {
            return false;
        }
        if (pos->line < 0) {
            pos->line = 0;
        } else if (pos->line >= s->lines(tick)) {
            pos->line = s->lines(tick) - 1;
        }
    } else {
        int min_line = abs_step(::mu::engraving::min_pitch);
        ClefType clef = s->clef(segment->tick());
        min_line = rel_step(min_line, clef);
        int max_line = abs_step(::mu::engraving::max_pitch);
        max_line = rel_step(max_line, clef);

        if (pos->line > min_line || pos->line < max_line) {
            return false;
        }
    }

    y = sstaff->y() + pos->line * line_dist;
    pos->pos = PointF(x, y) + measure->canvas_pos();
    return true;
}

//---------------------------------------------------------
//   checkHasMeasures
//---------------------------------------------------------

bool Score::check_has_measures() const {
    Page *page = pages().empty() ? 0 : pages().front();
    const std::vector<System *> *sl = page ? &page->systems() : 0;
    if (sl == 0 || sl->empty() || sl->front()->measures().empty()) {
        LOGD("first create measure, then repeat operation");
        return false;
    }
    return true;
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void Score::spatium_changed(double old_value, double new_value) {
    scan_elements([&](EngravingItem *e) { e->spatium_changed(old_value, new_value); });
    for (Staff *staff : staves_) {
        staff->spatium_changed(old_value, new_value);
    }
    layout_options_.note_head_width = engraving_font_->width(SymId::NOTEHEAD_BLACK, new_value / style().default_spatium());
    update_padding_tables();
}

//---------------------------------------------------------
//   updateStyle
//---------------------------------------------------------

static void update_style(EngravingItem *e) {
    bool v = e->generated();
    e->style_changed();
    e->set_generated(v);
}

//---------------------------------------------------------
//   styleChanged
//    must be called after every style change
//---------------------------------------------------------

void Score::style_changed() {
    scan_elements(update_style);
    for (Staff *staff : staves()) {
        for (int tick = 0; tick != -1; tick = staff->staff_type_range(Fraction::from_ticks(tick + 1)).second) {
            StaffType *st = staff->staff_type(Fraction::from_ticks(tick));
            if (!st) {
                continue;
            }
            st->style_changed();
        }
    }
    update_padding_tables();
    set_layout_all();
}

//---------------------------------------------------------
//   addElement
//---------------------------------------------------------

/**
 Add \a element to its parent.

 Several elements (clef, keysig, timesig) need special handling, as they may cause
 changes throughout the score.
*/

void Score::add_element(EngravingItem *element) {
    EngravingItem *parent = element->ownership_parent_item();
    element->trigger_layout();

    //      LOGD("Score(%p) EngravingItem(%p)(%s) parent %p(%s)",
    //         this, element, element->typeName(), parent, parent ? parent->typeName() : "");

    ElementType et = element->type();
    if (et == ElementType::MEASURE || (et == ElementType::HBOX && !(parent && parent->is_v_box())) || et == ElementType::VBOX ||
        et == ElementType::TBOX || et == ElementType::FBOX) {
        measures()->add(to_measure_base(element));
        element->trigger_layout();
        return;
    }

    if (et == ElementType::BEAM) { // a beam is not attached to its system
        parent = nullptr;
    }

    if (parent) {
        parent->add(element);
    }

    switch (et) {
        case ElementType::BEAM: {
            Beam *b = to_beam(element);
            size_t n = b->elements().size();
            for (size_t i = 0; i < n; ++i) {
                b->elements().at(i)->set_beam(b);
            }
        } break;

        case ElementType::SLUR:
        case ElementType::HAMMER_ON_PULL_OFF:
            add_layout_flags(LayoutFlag::PLAY_EVENTS);
            // fall through

        case ElementType::VOLTA:
        case ElementType::TRILL:
        case ElementType::VIBRATO:
        case ElementType::PEDAL:
        case ElementType::TEXTLINE:
        case ElementType::HAIRPIN:
        case ElementType::LET_RING:
        case ElementType::GRADUAL_TEMPO_CHANGE:
        case ElementType::PALM_MUTE:
        case ElementType::WHAMMY_BAR:
        case ElementType::RASGUEADO:
        case ElementType::HARMONIC_MARK:
        case ElementType::PICK_SCRAPE:
        case ElementType::PARTIAL_LYRICSLINE: {
            Spanner *spanner = to_spanner(element);
            add_spanner(spanner);
            for (SpannerSegment *ss : spanner->spanner_segments()) {
                if (ss->system()) {
                    ss->system()->add(ss);
                }
            }
        } break;

        case ElementType::OTTAVA: {
            Ottava *o = to_ottava(element);
            add_spanner(o);
            for (SpannerSegment *ss : o->spanner_segments()) {
                if (ss->system()) {
                    ss->system()->add(ss);
                }
            }
            o->staff()->update_ottava();
        } break;

        case ElementType::INSTRUMENT_CHANGE: {
            add_layout_flags(LayoutFlag::REBUILD_MIDI_MAPPING);
            cmd_state().instruments_changed = true;
        } break;

        case ElementType::CHORD: {
            // May need to reconnect slur when inserting new chord
            SpannerMap &smap = spanner_map();
            Fraction tick = element->tick();
            auto spanners = smap.find_overlapping(tick.ticks(), tick.ticks());
            for (auto interval : spanners) {
                Spanner *s = interval.value;
                if (s->track() != element->track() || !s->is_slur()) {
                    continue;
                }
                if (!s->start_element() && s->tick() == tick) {
                    s->set_start_element(element);
                }
                if (!s->end_element() && s->tick2() == tick) {
                    s->set_end_element(element);
                }
            }
            break;
        }
        case ElementType::HARMONY:
        case ElementType::FRET_DIAGRAM:
            if (element->part()) {
                element->part()->update_harmony_channels(true);
            }
            break;
        case ElementType::GUITAR_BEND: {
            GuitarBend *bend = to_guitar_bend(element);
            if (bend->bend_type() == GuitarBendType::GRACE_NOTE_BEND || bend->bend_type() == GuitarBendType::PRE_BEND ||
                bend->bend_type() == GuitarBendType::PRE_DIVE) {
                Note *start_note = bend->start_note();
                Chord *start_chord = start_note ? start_note->chord() : nullptr;
                if (start_chord) {
                    start_chord->set_no_stem(true);
                    start_chord->set_beam_mode(BeamMode::NONE);
                }
            }
        }

        default: break;
    }

    if (element->is_text_base() && to_text_base(element)->has_parent_segment() &&
        to_segment(element->parent())->is_type(SegmentType::DURATION)) {
        MoveElementAnchors::check_measure_boundaries_and_move_if_need(element);
    }

    element->trigger_layout();
}

void Score::do_undo_add_element(EngravingItem *element) {
    if (element->generated()) {
        add_element(element);
    } else {
        undo(new AddElement(element));
    }
}

//---------------------------------------------------------
//   removeElement
///   Remove \a element from its parent.
///   Several elements (clef, keysig, timesig) need special handling, as they may cause
///   changes throughout the score.
//---------------------------------------------------------

void Score::remove_element(EngravingItem *element) {
    EngravingItem *parent = element->ownership_parent_item();
    element->trigger_layout();

    // special for MEASURE, HBOX, VBOX
    // their parent is not static

    ElementType et = element->type();
    bool parent_is_v_box = parent && parent->is_v_box();

    if (et == ElementType::MEASURE || (et == ElementType::HBOX && !parent_is_v_box) || et == ElementType::VBOX || et == ElementType::TBOX ||
        et == ElementType::FBOX) {
        MeasureBase *mb = to_measure_base(element);
        measures()->remove(mb);

        System *system = mb->system();
        if (!system) {
#ifndef NDEBUG
            // vertical boxes are not shown in continuous view so no system
            const bool no_system_mode = line_mode() && element->is_v_box_base();
            assert(no_system_mode || !is_open());
#endif
            return;
        }

        system->remove_measure(mb);

        // See also InsertRemoveMeasures::removeMeasures()
        if (element->is_box() && system->measures().empty()) {
            Page *page = system->page();
            if (page) {
                muse::remove(page->systems(), system);
                system->set_page(nullptr);
            }

            muse::remove(systems_, system);
            system->delete_later();

            if (page && page->systems().empty()) {
                // Remove this page, since it is now empty.
                // This involves renumbering and repositioning all subsequent pages.
                PointF pos = page->pos();
                auto ii = std::find(pages().begin(), pages().end(), page);
                pages().erase(ii);
                page->delete_later();

                while (ii != pages().end()) {
                    page = *ii;
                    page->set_page_number(page->page_number() - 1);
                    PointF p = page->pos();
                    page->set_pos(pos);
                    pos = p;
                    ii++;
                }
            }
        }
        return;
    }

    if (et == ElementType::BEAM) { // a beam is not attached to its system
        parent = nullptr;
    }

    if (parent) {
        parent->remove(element);
    }

    switch (et) {
        case ElementType::BEAM:
            for (ChordRest *cr : to_beam(element)->elements()) {
                cr->set_beam(0);
            }
            break;

        case ElementType::SLUR:
        case ElementType::HAMMER_ON_PULL_OFF:
            add_layout_flags(LayoutFlag::PLAY_EVENTS);
            // fall through

        case ElementType::VOLTA:
        case ElementType::TRILL:
        case ElementType::VIBRATO:
        case ElementType::PEDAL:
        case ElementType::LET_RING:
        case ElementType::GRADUAL_TEMPO_CHANGE:
        case ElementType::PALM_MUTE:
        case ElementType::PARTIAL_LYRICSLINE:
        case ElementType::WHAMMY_BAR:
        case ElementType::RASGUEADO:
        case ElementType::HARMONIC_MARK:
        case ElementType::PICK_SCRAPE:
        case ElementType::TEXTLINE:
        case ElementType::HAIRPIN: {
            Spanner *spanner = to_spanner(element);
            spanner->trigger_layout();
            remove_spanner(spanner);
        } break;

        case ElementType::GUITAR_BEND: {
            Note *start_note = to_guitar_bend(element)->start_note();
            if (start_note) {
                start_note->set_parentheses_mode(ParenthesesMode::NONE);
                start_note->chord()->set_no_stem(false);
                start_note->chord()->set_beam_mode(BeamMode::AUTO);
            }
            Note *end_note = to_guitar_bend(element)->end_note();
            if (end_note) {
                end_note->set_ghost(false);
                end_note->set_visible(true);
                const StringData *string_data = end_note->origin_part()->string_data(end_note->tick(), end_note->staff_idx());
                int end_fret = string_data->fret(end_note->pitch(), end_note->string(), end_note->staff());
                end_note->set_fret(end_fret);
            }
            break;
        }

        case ElementType::OTTAVA: {
            Ottava *o = to_ottava(element);
            o->trigger_layout();
            remove_spanner(o);
            o->staff()->update_ottava();
        } break;

        case ElementType::CHORD:
        case ElementType::REST:
        case ElementType::MMREST: {
            ChordRest *cr = to_chord_rest(element);
            if (cr->beam()) {
                cr->beam()->remove(cr);
            }
            for (Lyrics *lyr : cr->lyrics()) {
                lyr->remove_from_score();
            }
            // TODO: check for tuplet?
        } break;
        case ElementType::INSTRUMENT_CHANGE: {
            add_layout_flags(LayoutFlag::REBUILD_MIDI_MAPPING);
            cmd_state().instruments_changed = true;
        } break;

        case ElementType::HARMONY:
        case ElementType::FRET_DIAGRAM:
            if (element->part()) {
                element->part()->update_harmony_channels(true, true);
            }
            break;

        default: break;
    }
}

void Score::do_undo_remove_element(EngravingItem *element) {
    if (element->generated()) {
        remove_element(element);
        //! HACK: don't delete as it may still be used in PropertiesPanel
        // element->deleteLater();
    } else {
        undo(new RemoveElement(element));
    }
}

bool Score::can_reselect_item(const EngravingItem *item) const {
    if (!item || item->selected()) {
        return false;
    }

    EngravingItem *seg = const_cast<EngravingItem *>(item->find_ancestor(ElementType::SEGMENT));
    if (seg) {
        std::vector<EngravingItem *> elements = seg->get_children(false);
        return muse::contains(elements, const_cast<EngravingItem *>(item));
    }

    return true;
}

//---------------------------------------------------------
//   firstMeasure
//---------------------------------------------------------

Measure *Score::first_measure() const {
    MeasureBase *mb = measures_.first();
    while (mb && !mb->is_measure()) {
        mb = mb->next();
    }

    return to_measure(mb);
}

//---------------------------------------------------------
//   firstMeasureMM
//---------------------------------------------------------

Measure *Score::first_measure_mm() const {
    Measure *m = first_measure();
    if (m && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && m->has_mm_rest()) {
        return m->mm_rest();
    }
    return m;
}

//---------------------------------------------------------
//   firstMM
//---------------------------------------------------------

MeasureBase *Score::first_mm() const {
    MeasureBase *m = measures_.first();
    if (m && m->is_measure() && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && to_measure(m)->has_mm_rest()) {
        return to_measure(m)->mm_rest();
    }
    return m;
}

//---------------------------------------------------------
//   measure
//---------------------------------------------------------

MeasureBase *Score::measure(int idx) const {
    MeasureBase *mb = measures_.first();
    for (int i = 0; i < idx; ++i) {
        mb = mb->next();
        if (mb == 0) {
            return 0;
        }
    }
    return mb;
}

//---------------------------------------------------------
//   crMeasure
//    Returns a measure containing chords and/or rests
//    by its index, skipping other MeasureBase descendants
//---------------------------------------------------------

Measure *Score::cr_measure(int idx) const {
    int i = -1;
    for (MeasureBase *mb = measures_.first(); mb; mb = mb->next()) {
        if (mb->is_measure()) {
            ++i;
            if (i == idx) {
                return to_measure(mb);
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   lastMeasure
//---------------------------------------------------------

Measure *Score::last_measure() const {
    MeasureBase *mb = measures_.last();

    if (!mb) {
        return nullptr;
    }

    while (mb && !mb->is_measure()) {
        mb = mb->prev();
    }
    return to_measure(mb);
}

//---------------------------------------------------------
//   lastMeasureMM
//---------------------------------------------------------

Measure *Score::last_measure_mm() const {
    Measure *m = last_measure();
    return m ? const_cast<Measure *>(m->covering_mm_rest_or_this()) : nullptr;
}

//---------------------------------------------------------
//   endTick
//---------------------------------------------------------

Fraction Score::end_tick() const {
    Measure *m = last_measure();
    return m ? m->end_tick() : Fraction(0, 1);
}

//---------------------------------------------------------
//   firstSegment
//---------------------------------------------------------

Segment *Score::first_segment(SegmentType seg_type) const {
    Segment *seg;
    Measure *m = first_measure();
    if (!m) {
        seg = 0;
    } else {
        seg = m->first();
        if (seg && !(seg->segment_type() & seg_type)) {
            seg = seg->next1(seg_type);
        }
    }
    return seg;
}

//---------------------------------------------------------
//   firstSegmentMM
//---------------------------------------------------------

Segment *Score::first_segment_mm(SegmentType seg_type) const {
    Measure *m = first_measure_mm();
    return m ? m->first(seg_type) : 0;
}

//---------------------------------------------------------
//   lastSegment
//---------------------------------------------------------

Segment *Score::last_segment() const {
    Measure *m = last_measure();
    return m ? m->last() : 0;
}

//---------------------------------------------------------
//   lastSegmentMM
//---------------------------------------------------------

Segment *Score::last_segment_mm() const {
    Measure *m = last_measure_mm();
    return m ? m->last() : 0;
}

double Score::utick2utime(int tick) const { return tempo_timeline(/*expandRepeats*/ true).utick2utime(tick); }

int Score::utime2utick(double utime) const { return tempo_timeline(/*expandRepeats*/ true).utime2utick(utime); }

const TempoTimeline &Score::tempo_timeline() const { return master_score()->tempo_timeline(); }

const TempoTimeline &Score::tempo_timeline(bool expand_repeats) const { return master_score()->tempo_timeline(expand_repeats); }

void Score::set_tempo_timeline_override(std::optional<TempoTimeline> timeline) {
    master_score()->set_tempo_timeline_override(std::move(timeline));
}

//---------------------------------------------------------
//   scanElementsInRange
//---------------------------------------------------------

void Score::scan_elements_in_range(std::function<void(EngravingItem *)> func) {
    const Fraction selection_start_tick = selection_.start_segment() ? selection_.start_segment()->tick() : Fraction(0, 1);
    const Fraction selection_end_tick = selection_.end_segment() ? selection_.end_segment()->tick() : end_tick();
    auto do_scan_element = [&](EngravingItem *e) {
        if (e->tick() >= selection_start_tick && e->tick() < selection_end_tick) {
            func(e);
        }
    };

    System *last_system = nullptr;
    // Inculde previous measure/system so we scan items at their end tick
    // Filter anything else out in doScanElement
    MeasureBase *start_mb = selection_.start_measure_base();
    if (start_mb->prev()) {
        start_mb = start_mb->prev();
    }
    for (MeasureBase *mb = start_mb; mb && mb->tick() < selection_end_tick; mb = mb->next()) {
        mb->scan_elements(do_scan_element);

        if (mb->is_measure() && to_measure(mb)->mm_rest()) {
            Measure *mmr = to_measure(mb)->mm_rest();
            mmr->scan_elements(do_scan_element);
        }
        System *system = mb ? mb->system() : nullptr;
        if (system && system != last_system) {
            system->scan_elements(do_scan_element);
            last_system = system;
        }
    }
}

//---------------------------------------------------------
//   setSelection
//---------------------------------------------------------

void Score::set_selection(const Selection &s) {
    deselect_all();
    selection_ = s;

    for (EngravingItem *e : selection_.elements()) {
        e->set_selected(true);
    }
}

//---------------------------------------------------------
//   getText
//---------------------------------------------------------

Text *Score::get_text(TextStyleType tid) const {
    MeasureBase *m = first();
    if (m && m->is_v_box()) {
        for (EngravingItem *e : m->el()) {
            if (e->is_text() && to_text(e)->text_style_type() == tid) {
                return to_text(e);
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   metaTag
//---------------------------------------------------------

String Score::meta_tag(const String &s) const {
    if (muse::contains(meta_tags_, s)) {
        return meta_tags_.at(s);
    }

    return muse::value(master_score_->meta_tags_, s);
}

//---------------------------------------------------------
//   setMetaTag
//---------------------------------------------------------

void Score::set_meta_tag(const String &tag, const String &val) { meta_tags_.insert_or_assign(tag, val); }

//---------------------------------------------------------
//   setSynthesizerState
//---------------------------------------------------------

void Score::set_synthesizer_state(const SynthesizerState &s) {
    // TODO: make undoable
    synthesizer_state_ = s;
}

//---------------------------------------------------------
//   appendScore
//---------------------------------------------------------

bool Score::append_score(Score *score, bool add_page_break, bool add_section_break) {
    if (parts().size() < score->parts().size() || staves().size() < score->staves().size()) {
        LOGD("Score to append has %zu parts and %zu staves, but this score only has %zu parts and %zu staves.", score->parts().size(),
             score->staves().size(), parts().size(), staves().size());
        return false;
    }

    if (!last()) {
        LOGD("This score doesn't have any MeasureBase objects.");
        return false;
    }

    // apply Page/Section Breaks if desired
    if (add_page_break) {
        if (!last()->page_break()) {
            last()->undo_set_break(false, LayoutBreakType::LINE); // remove line break if exists
            last()->undo_set_break(true, LayoutBreakType::PAGE);  // apply page break
        }
    } else if (!last()->line_break() && !last()->page_break()) {
        last()->undo_set_break(true, LayoutBreakType::LINE);
    }

    if (add_section_break && !last()->section_break()) {
        last()->undo_set_break(true, LayoutBreakType::SECTION);
    }

    // match concert pitch states
    if (style().style_b(Sid::CONCERT_PITCH) != score->style().style_b(Sid::CONCERT_PITCH)) {
        score->cmd_concert_pitch_changed(style().style_b(Sid::CONCERT_PITCH));
    }

    // clone the measures
    append_measures_from_score(score, Fraction(0, 1), score->last()->end_tick());

    set_layout_all();
    return true;
}

//---------------------------------------------------------
//   appendMeasuresFromScore
//     clone measures from another score to the end of this
//---------------------------------------------------------

bool Score::append_measures_from_score(Score *score, const Fraction &start_tick, const Fraction &end_tick) {
    Transaction &tx = transaction_manager()->current_or_dummy_transaction();

    Fraction tick_of_append = last()->end_tick();
    TieMap tie_map;

    MeasureBase *fmb = score->tick2measure(start_tick);
    MeasureBase *emb = score->tick2measure(end_tick);
    Fraction cur_tick = tick_of_append;
    for (MeasureBase *cmb = fmb; cmb != emb; cmb = cmb->next()) {
        MeasureBase *nmb;
        if (cmb->is_measure()) {
            Measure *nm = to_measure(cmb)->clone_measure(this, cur_tick, &tie_map);
            cur_tick += nm->ticks();
            nmb = to_measure_base(nm);
        } else {
            nmb = static_cast<MeasureBase *>(cmb->clone());
        }

        nmb->set_score(this);
        measures()->append(nmb);
    }

    Measure *first_appended_measure = tick2measure(tick_of_append);

    // if the appended score has less staves,
    // make sure the measures have full measure rest
    for (Measure *m = first_appended_measure; m; m = m->next_measure()) {
        for (size_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
            Fraction f;
            for (Segment *s = m->first(SegmentType::CHORD_REST); s; s = s->next(SegmentType::CHORD_REST)) {
                for (voice_idx_t v = 0; v < ::mu::engraving::voices; ++v) {
                    ChordRest *cr = to_chord_rest(s->element(staff_idx * ::mu::engraving::voices + v));
                    if (cr == 0) {
                        continue;
                    }
                    f += cr->actual_ticks();
                }
            }
            if (f.is_zero()) {
                add_rest(m->tick(), staff_idx * ::mu::engraving::voices, TDuration(DurationType::V_MEASURE), 0);
            }
        }
    }

    // at first added measure, check if we need to add Clef/Key/TimeSig
    //  this is needed if it was changed and needs to be changed back
    size_t n = nstaves();
    Fraction otick = fmb->tick(), ctick = tick_of_append;
    for (staff_idx_t staff_idx = 0; staff_idx < n; ++staff_idx) { // iterate over all staves
        track_idx_t track_idx = staff2track(staff_idx);           // idx of irst track on the staff
        Staff *staff = this->staff(staff_idx);
        Staff *ostaff = score->staff(staff_idx);

        // check if key signature needs to be changed
        if (ostaff->key(otick) != staff->key(ctick)) {
            Segment *ns = first_appended_measure->undo_get_segment(SegmentType::KEY_SIG, ctick);
            KeySigEvent nkse = KeySigEvent(ostaff->key_sig_event(otick));
            KeySig *nks = Factory::create_key_sig(ns);
            nks->set_score(this);
            nks->set_track(track_idx);

            nks->set_key_sig_event(nkse);
            staff->set_key(ctick, nkse);
            ns->add(nks);
        }
        // check if a key signature is present but is spurious (i.e. no actual change)
        else if (staff->current_key_tick(ctick) == ctick && staff->key(ctick - Fraction::eps()) == ostaff->key(otick)) {
            Segment *ns = first_appended_measure->first(SegmentType::KEY_SIG);
            if (ns) {
                ns->remove(ns->element(track_idx));
            }
        }

        // check if time signature needs to be changed
        TimeSig *ots = ostaff->time_sig(otick), *cts = staff->time_sig(ctick);
        TimeSig *pts = staff->time_sig(ctick - Fraction::eps());
        if (ots && cts && *ots != *cts) {
            Segment *ns = first_appended_measure->undo_get_segment(SegmentType::TIME_SIG, ctick);
            TimeSig *nsig = new TimeSig(*ots);

            nsig->set_score(this);
            nsig->set_track(track_idx);
            ns->add(nsig);
        }
        // check if a time signature is present but is spurious (i.e. no actual change)
        else if (staff->current_time_sig_tick(ctick) == ctick && ots && pts && *pts == *ots) {
            Segment *ns = first_appended_measure->first(SegmentType::TIME_SIG);
            if (ns) {
                ns->remove(ns->element(track_idx));
            }
        }

        // check if clef signature needs to be changed
        if (ostaff->clef(otick) != staff->clef(ctick)) {
            EditClef::undo_change_clef(tx, this, staff, first_appended_measure, ostaff->clef(otick));
        }
        // check if a clef change is present but is spurious (i.e. no actual change)
        else if (staff->current_clef_tick(ctick) == ctick && staff->clef(ctick - Fraction::eps()) == ostaff->clef(otick)) {
            Segment *ns = first_appended_measure->first(SegmentType::CLEF);
            if (!ns) {
                ns = first_appended_measure->first(SegmentType::HEADER_CLEF);
            }
            if (ns) {
                ns->remove(ns->element(track_idx));
            }
        }
    }

    // check if section starts with a pick-up measure to be merged with end of previous section
    Measure *cm = first_appended_measure, *pm = cm->prev_measure();
    if (pm->timesig() == cm->timesig() && pm->ticks() + cm->ticks() == cm->timesig()) {
        SplitJoinMeasure::join_measures(tx, master_score_, pm->tick(), cm->tick());
    }

    // clone the spanners (only in the range currently copied)
    auto ospans = score->spanner();
    auto lb = ospans.lower_bound(start_tick.ticks()), ub = ospans.upper_bound(end_tick.ticks());
    for (auto sp = lb; sp != ub; sp++) {
        Spanner *spanner = sp->second;

        if (spanner->tick2() > end_tick) {
            continue; // map is by tick() so this can still happen in theory...
        }
        Spanner *ns = to_spanner(spanner->clone());
        ns->set_score(this);
        ns->move_to_dummy();
        ns->set_tick(spanner->tick() - start_tick + tick_of_append);
        ns->set_tick2(spanner->tick2() - start_tick + tick_of_append);
        ns->compute_start_element();
        ns->compute_end_element();
        add_element(ns);
    }

    return true;
}

//---------------------------------------------------------
//   splitStaff
//---------------------------------------------------------

void Score::split_staff(staff_idx_t staff_idx, int split_point) {
    //      LOGD("split staff %d point %d", staffIdx, splitPoint);

    //
    // create second staff
    //
    Staff *st = staff(staff_idx);
    Part *p = st->part();
    Staff *ns = Factory::create_staff(p);
    ns->init(st);

    // convert staffIdx from score-relative to part-relative
    staff_idx_t staff_idx_part = staff_idx - p->staff(0)->idx();
    undo_insert_staff(ns, staff_idx_part + 1, false);

    Segment *seg = first_measure()->get_segment(SegmentType::HEADER_CLEF, Fraction(0, 1));
    Clef *clef = Factory::create_clef(seg);
    clef->set_clef_type(ClefType::F);
    clef->set_track((staff_idx + 1) * ::mu::engraving::voices);
    undo_add_element(clef);

    Transaction &tx = transaction_manager()->current_or_dummy_transaction();
    EditKeySig::undo_change_key_sig(tx, this, ns, Fraction(0, 1), st->key_sig_event(Fraction(0, 1)));

    master_score()->rebuild_midi_mapping();
    cmd_state().instruments_changed = true;
    do_layout();

    //
    // move notes
    //
    select(0, SelectType::SINGLE, 0);
    track_idx_t strack = staff_idx * ::mu::engraving::voices;
    track_idx_t dtrack = (staff_idx + 1) * ::mu::engraving::voices;

    // Keep track of ties to be reconnected.
    struct OldTie {
        Tie *tie;
        Note *nnote;
    };

    std::map<Note *, OldTie> old_ties;

    // Notes under the split point can be part of a tuplet, so keep track
    // of the tuplet mapping too!
    std::map<Tuplet *, Tuplet *> tuplet_mapping;
    Tuplet *tuplet_src[::mu::engraving::voices] = {};
    Tuplet *tuplet_dst[::mu::engraving::voices] = {};

    for (Segment *s = first_segment(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
        for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
            EngravingItem *e = s->element(strack + voice);

            if (!e) {
                continue;
            }
            if (to_duration_element(e)->tuplet()) {
                tuplet_src[voice] = to_duration_element(e)->tuplet();
                if (muse::contains(tuplet_mapping, tuplet_src[voice])) {
                    tuplet_dst[voice] = tuplet_mapping[tuplet_src[voice]];
                } else {
                    tuplet_dst[voice] = Factory::copy_tuplet(*tuplet_src[voice]);
                    tuplet_dst[voice]->set_track(dtrack);
                    tuplet_mapping.insert({tuplet_src[voice], tuplet_dst[voice]});
                }
            } else {
                tuplet_src[voice] = nullptr;
                tuplet_dst[voice] = nullptr;
            }

            bool create_rest_dst = true;
            bool create_rest_src = false;
            TDuration length_dst = to_chord_rest(e)->actual_duration_type();
            TDuration length_src = TDuration();

            if (e->is_chord()) {
                Chord *c = to_chord(e);
                std::list<Note *> remove_notes;
                for (Note *note : c->notes()) {
                    if (note->pitch() >= split_point) {
                        continue;
                    } else {
                        Chord *chord = to_chord(s->element(dtrack + voice));
                        assert(!chord || (chord->is_chord()));
                        if (!chord) {
                            chord = Factory::copy_chord(*c);
                            muse::delete_all(chord->notes());
                            chord->notes().clear();
                            chord->set_tuplet(tuplet_dst[voice]);
                            chord->set_track(dtrack + voice);
                            undo_add_element(chord);
                        }
                        Note *nnote = Factory::copy_note(*note);
                        if (note->tie_for()) {
                            // Save the note and the tie for processing later.
                            // Use the end note as index in the map so, when this is found
                            // we know the tie has to be recreated.
                            old_ties.insert({note->tie_for()->end_note(), OldTie{note->tie_for(), nnote}});
                        }
                        nnote->set_track(dtrack + voice);
                        chord->add(nnote);
                        nnote->update_line();
                        remove_notes.push_back(note);
                        create_rest_dst = false;
                        length_dst = chord->actual_duration_type();

                        // Is the note the last note of a tie?
                        if (muse::contains(old_ties, note)) {
                            // Yes! Create a tie between the new notes and remove the
                            // old tie.
                            Tie *tie = old_ties[note].tie->clone();
                            tie->set_start_note(old_ties[note].nnote);
                            tie->set_end_note(nnote);
                            tie->set_track(nnote->track());
                            undo_add_element(tie);
                            undo_remove_element(old_ties[note].tie);
                            muse::remove(old_ties, note);
                        }
                    }
                }
                create_rest_src = false;
                for (Note *note : remove_notes) {
                    undo_remove_element(note);
                    Chord *chord = note->chord();
                    if (chord->notes().empty()) {
                        for (auto sp : spanner()) {
                            Slur *slur = to_slur(sp.second);
                            if (!slur->is_slur()) {
                                continue;
                            }
                            if (slur->start_cr() == chord) {
                                slur->undo_change_property(Pid::TRACK, slur->track() + ::mu::engraving::voices);
                                for (EngravingObject *ee : slur->link_list()) {
                                    Slur *lslur = to_slur(ee);
                                    lslur->set_start_element(0);
                                }
                            }
                            if (slur->end_cr() == chord) {
                                slur->undo_change_property(Pid::SPANNER_TRACK2, slur->track2() + ::mu::engraving::voices);
                                for (EngravingObject *ee : slur->link_list()) {
                                    Slur *lslur = to_slur(ee);
                                    lslur->set_end_element(0);
                                }
                            }
                        }
                        create_rest_src = true;
                        length_src = chord->actual_duration_type();
                        undo_remove_element(chord);
                    }
                }
            }

            if (create_rest_src) {
                add_rest(s, strack + voice, length_src, tuplet_src[voice]);
            }
            if (create_rest_dst) {
                add_rest(s, dtrack + voice, length_dst, tuplet_dst[voice]);
            }
        }
    }
}

//---------------------------------------------------------
//   cmdRemovePart
//---------------------------------------------------------

void Score::cmd_remove_part(Part *part) {
    if (!part) {
        return;
    }

    Transaction &tx = transaction_manager()->current_or_dummy_transaction();
    EditStaveSharing::handle_remove_part(tx, part);

    staff_idx_t sidx = staff_idx(part);
    size_t n = part->nstaves();

    for (staff_idx_t i = 0; i < n; ++i) {
        cmd_remove_staff(sidx);
    }

    undo_remove_part(part, muse::index_of(parts_, part));

    if (!has_shared_parts()) {
        undo_change_style_val(Sid::ENABLE_STAVE_SHARING, false);
    }
}

//---------------------------------------------------------
//   insertPart
//---------------------------------------------------------

void Score::insert_part(Part *part, size_t target_part_idx) {
    if (!part) {
        return;
    }

    part->set_score(this);
    assign_id_if_need(*part);

    if (target_part_idx < parts_.size()) {
        parts_.insert(parts_.begin() + target_part_idx, part);
    } else {
        parts_.push_back(part);
    }

    master_score()->rebuild_midi_mapping();
    set_instruments_changed(true);
    mark_instruments_as_primary(parts_);
}

void Score::append_part(Part *part) {
    if (!part) {
        return;
    }

    part->set_score(this);
    assign_id_if_need(*part);
    parts_.push_back(part);
    mark_instruments_as_primary(parts_);
}

//---------------------------------------------------------
//   removePart
//---------------------------------------------------------

void Score::remove_part(Part *part) {
    part_idx_t index = muse::index_of(parts_, part);

    if (index == muse::nidx) {
        for (size_t i = 0; i < parts_.size(); ++i) {
            if (parts_[i]->id() == part->id()) {
                index = i;
                break;
            }
        }
    }

    parts_.erase(parts_.begin() + index);
    mark_instruments_as_primary(parts_);

    if (excerpt_) {
        for (Part *excerpt_part : excerpt_->parts()) {
            if (excerpt_part->id() != part->id()) {
                continue;
            }

            muse::remove(excerpt_->parts(), excerpt_part);
            mark_instruments_as_primary(excerpt_->parts());
            break;
        }
    }

    master_score()->rebuild_midi_mapping();
    set_instruments_changed(true);
}

//---------------------------------------------------------
//   insertStaff
//---------------------------------------------------------

void Score::insert_staff(Staff *staff, staff_idx_t ridx) {
    if (!staff || !staff->part()) {
        return;
    }

    assign_id_if_need(*staff);
    staff->part()->insert_staff(staff, ridx);

    staff_idx_t idx = staff_idx(staff->part()) + ridx;
    staves_.insert(staves_.begin() + idx, staff);
    brackets_.insert(brackets_.begin() + idx, std::vector<BracketItem *>());
    for (staff_idx_t i = idx + 1; i < static_cast<staff_idx_t>(brackets_.size()); ++i) {
        for (BracketItem *bi : brackets_[i]) {
            bi->set_start_staff_idx(i);
        }
    }

    for (auto i = staff->score()->spanner().cbegin(); i != staff->score()->spanner().cend(); ++i) {
        Spanner *s = i->second;
        if (!move_down_when_adding_staves(s, idx)) {
            continue;
        }
        if (s->staff_idx() >= idx) {
            track_idx_t t = s->track() + ::mu::engraving::voices;
            if (t >= ntracks()) {
                t = ntracks() - 1;
            }
            s->set_track(t);
            for (SpannerSegment *ss : s->spanner_segments()) {
                ss->set_track(t);
            }
            if (s->track2() != muse::nidx) {
                t = s->track2() + ::mu::engraving::voices;
                s->set_track2(t < ntracks() ? t : s->track());
            }
        }
    }

    update_staves_number_for_systems();
}

void Score::append_staff(Staff *staff) {
    if (!staff || !staff->part()) {
        return;
    }

    assign_id_if_need(*staff);
    staff->part()->append_staff(staff);
    staves_.push_back(staff);
    brackets_.emplace_back();

    update_staves_number_for_systems();
}

void Score::assign_id_if_need(Staff &staff) const {
    if (staff.id() == ::mu::engraving::invalid_id) {
        staff.set_id(new_staff_id());
    }
}

void Score::assign_id_if_need(Part &part) const {
    if (part.id() == ::mu::engraving::invalid_id) {
        part.set_id(new_part_id());
    }
}

void Score::update_staves_number_for_systems() {
    for (System *system : systems_) {
        if (!system->first_measure()) {
            continue;
        }

        system->adjust_staves_number(nstaves());
    }
}

ID Score::new_staff_id() const {
    ID max_id = 0;

    for (const Staff *staff : score()->staves()) {
        max_id = std::max(max_id, staff->id());
    }

    return max_id + 1;
}

ID Score::new_part_id() const {
    ID max_id = 0;

    for (const Part *part : score()->parts()) {
        max_id = std::max(max_id, part->id());
    }

    return max_id + 1;
}

//---------------------------------------------------------
//   removeStaff
//---------------------------------------------------------

void Score::remove_staff(Staff *staff) {
    staff_idx_t idx = staff->idx();
    for (auto i = staff->score()->spanner().cbegin(); i != staff->score()->spanner().cend(); ++i) {
        Spanner *s = i->second;
        if (s->staff_idx() > idx) {
            track_idx_t t = s->track() >= ::mu::engraving::voices ? s->track() - ::mu::engraving::voices : 0;
            s->set_track(t);
            for (SpannerSegment *ss : s->spanner_segments()) {
                ss->set_track(t);
            }
            if (s->track2() != muse::nidx) {
                t = s->track2() - ::mu::engraving::voices;
                s->set_track2((t != muse::nidx) ? t : s->track());
            }
        }
    }

    muse::remove(staves_, staff);

    muse::delete_all(brackets_[idx]);
    brackets_.erase(brackets_.begin() + idx);
    for (staff_idx_t i = idx; i < static_cast<staff_idx_t>(brackets_.size()); ++i) {
        for (BracketItem *bi : brackets_[i]) {
            bi->set_start_staff_idx(i);
        }
    }
    staff->part()->remove_staff(staff);

    if (staff->is_system_object_staff()) {
        muse::remove(system_object_staves_, staff);
    }

    update_staves_number_for_systems();
}

//---------------------------------------------------------
//   adjustKeySigs
//---------------------------------------------------------

void Score::adjust_key_sigs(track_idx_t sidx, track_idx_t eidx, KeyList km) {
    for (track_idx_t staff_idx = sidx; staff_idx < eidx; ++staff_idx) {
        Staff *staff = staves_[staff_idx];
        for (auto i = km.begin(); i != km.end(); ++i) {
            Fraction tick = Fraction::from_ticks(i->first);
            Measure *measure = tick2measure(tick);
            if (!measure) {
                continue;
            }
            if (staff->is_drum_staff(tick)) {
                continue;
            }
            KeySigEvent key = i->second;
            Interval v = staff->part()->instrument(tick)->transpose();
            if (!v.is_zero() && !style().style_b(Sid::CONCERT_PITCH) && !key.is_atonal()) {
                v.flip();
                key.set_key(Transpose::transpose_key(key.concert_key(), v, staff->part()->prefer_sharp_flat()));
            }
            staff->set_key(tick, key);

            Segment *s = measure->get_segment(SegmentType::KEY_SIG, tick);
            KeySig *keysig = Factory::create_key_sig(s);
            keysig->set_track(staff_idx * ::mu::engraving::voices);
            keysig->set_key_sig_event(key);
            do_undo_add_element(keysig);
        }
    }
}

//---------------------------------------------------------
//   getKeyList
//      This is taken from MuseScore::editInstrList()
//---------------------------------------------------------

KeyList Score::key_list() const {
    // find the keylist of the first pitched staff
    KeyList tmp_keymap;
    Staff *first_staff = nullptr;
    bool is_first_staff = true;
    for (Staff *s : master_score()->staves()) {
        if (!s->is_drum_staff(Fraction(0, 1))) {
            KeyList *km = s->key_list();
            if (is_first_staff) {
                is_first_staff = false;
                tmp_keymap.insert(km->begin(), km->end());
                first_staff = s;
                continue;
            }

            // in terms of ticks, tmpKeymap = intersection(tmpKeymap, km)
            // note that if every single staff has a custom local keysig on this tick,
            // we will just copy the one from the top staff.
            auto curr_key = km->begin();
            KeyList kl;
            for (auto key : tmp_keymap) {
                if (key.first < (*curr_key).first) {
                    continue;
                }
                while ((*curr_key).first < key.first && curr_key != km->end()) {
                    curr_key++;
                }
                if (curr_key == km->end()) {
                    break;
                }
                if (key.first == (*curr_key).first) {
                    // there is a matching key sig on this staff
                    ++curr_key;
                    kl.insert(key);
                    if (curr_key == km->end()) {
                        break;
                    }
                }
            }
            tmp_keymap.clear();
            tmp_keymap.insert(kl.begin(), kl.end());
        }
    }
    Key normalized_c = Key::C;
    bool need_normalize = first_staff && !master_score()->style().style_b(Sid::CONCERT_PITCH) &&
                          (first_staff->part()->instrument()->transpose().chromatic || first_staff->part()->instruments().size() > 1);
    // normalize the keyevents to concert pitch if necessary
    KeyList nkl;
    for (auto key : tmp_keymap) {
        if (first_staff && !key.second.for_instrument_change()) {
            if (need_normalize) {
                Key c_key = key.second.concert_key();
                key.second.set_key(c_key);
            }
            nkl.insert(key);
        }
    }
    if (first_staff && !master_score()->style().style_b(Sid::CONCERT_PITCH) &&
        (first_staff->part()->instrument()->transpose().chromatic || first_staff->part()->instruments().size() > 1)) {
        int interval = first_staff->part()->instrument()->transpose().chromatic;
        normalized_c = Transpose::transpose_key(normalized_c, interval);
    }

    // create initial keyevent for transposing instrument if necessary
    auto i = nkl.begin();
    if (i == nkl.end() || i->first != 0) {
        nkl[0].set_concert_key(normalized_c);
    }

    return nkl;
}

//---------------------------------------------------------
//   cmdRemoveStaff
//---------------------------------------------------------

void Score::cmd_remove_staff(staff_idx_t staff_idx) {
    Staff *s = staff(staff_idx);
    EditStaffBrackets::adjust_brackets_del(this, staff_idx, staff_idx + 1);

    undo_remove_staff(s);
}

void Score::sort_system_objects(std::vector<staff_idx_t> &dst) {
    std::vector<staff_idx_t> move_to;
    for (const Staff *staff : system_object_staves_) {
        staff_idx_t old_staff_idx = staff->idx();
        staff_idx_t new_staff_ifx = old_staff_idx < dst.size() ? dst[old_staff_idx] : muse::nidx;

        if (muse::contains(move_to, new_staff_ifx)) {
            new_staff_ifx = muse::nidx;
        }

        move_to.push_back(new_staff_ifx);
    }

    for (staff_idx_t i = 0; i < system_object_staves_.size(); i++) {
        if (move_to[i] == system_object_staves_[i]->idx()) {
            // this sysobj staff doesn't move
            continue;
        } else {
            // move all system objects from systemObjectStaves[i] to _staves[moveTo[i]]
            for (MeasureBase *mb = measures()->first(); mb; mb = mb->next()) {
                if (!mb->is_measure()) {
                    continue;
                }

                Measure *m = to_measure(mb);
                for (EngravingItem *e : m->el()) {
                    if ((e->is_jump() || e->is_marker()) && e->is_linked() && e->track() == staff2track(system_object_staves_[i]->idx())) {
                        if (move_to[i] == muse::nidx) {
                            // delete this clone
                            m->remove(e);
                            e->unlink();
                            delete e;
                        } else {
                            e->set_track(staff2track(staves_[move_to[i]]->idx()));
                        }
                    }
                }

                for (Segment *s = m->first(); s; s = s->next()) {
                    if (s->is_chord_rest() || !s->annotations().empty()) {
                        const auto annotations = s->annotations(); // make a copy since we alter the list
                        for (EngravingItem *e : annotations) {
                            if (e->is_rehearsal_mark() || e->is_system_text() || e->is_triplet_feel() || e->is_tempo_text() ||
                                is_system_text_line(e)) {
                                if (e->track() == staff2track(system_object_staves_[i]->idx()) && e->is_linked()) {
                                    if (move_to[i] == muse::nidx) {
                                        s->remove_annotation(e);
                                        e->unlink();
                                        delete e;
                                    } else {
                                        e->set_track(staff2track(staves_[move_to[i]]->idx()));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // update systemObjectStaves with the correct staff
            if (move_to[i] == muse::nidx) {
                system_object_staves_.erase(system_object_staves_.begin() + i);
                move_to.erase(move_to.begin() + i);
            } else {
                system_object_staves_[i] = staves_[move_to[i]];
            }
        }
    }
}

//---------------------------------------------------------
//   sortStaves
//---------------------------------------------------------

void Score::sort_staves(std::vector<staff_idx_t> &dst) {
    sort_system_objects(dst);
    muse::delete_all(systems());
    systems().clear(); //??
    parts_.clear();
    Part *cur_part = nullptr;
    std::vector<Staff *> dl;
    std::map<size_t, size_t> track_map;
    track_idx_t track = 0;
    for (staff_idx_t idx : dst) {
        Staff *staff = staves_[idx];
        if (staff->part() != cur_part) {
            cur_part = staff->part();
            cur_part->clear_staves();
            parts_.push_back(cur_part);
        }
        cur_part->append_staff(staff);
        dl.push_back(staff);
        for (size_t itrack = 0; itrack < ::mu::engraving::voices; ++itrack) {
            track_map.insert({idx * ::mu::engraving::voices + itrack, track++});
        }
    }
    staves_ = dl;

    for (Measure *m = first_measure(); m; m = m->next_measure()) {
        m->sort_staves(dst);
        if (m->has_mm_rest()) {
            m->mm_rest()->sort_staves(dst);
        }
    }
    for (auto i : spanner_.map()) {
        Spanner *sp = i.second;
        voice_idx_t voice = sp->voice();
        staff_idx_t staff_idx = sp->staff_idx();
        staff_idx_t idx = muse::index_of(dst, staff_idx);
        if (idx != muse::nidx && !sp->is_top_system_object()) {
            sp->set_track(idx * ::mu::engraving::voices + voice);
            if (sp->track2() != muse::nidx) {
                sp->set_track2(idx * ::mu::engraving::voices + (sp->track2() % ::mu::engraving::voices)); // at least keep the voice...
            }
        }
    }
    set_layout_all();
    mark_instruments_as_primary(parts_);
}

//---------------------------------------------------------
//   cmdConcertPitchChanged
//---------------------------------------------------------

void Score::cmd_concert_pitch_changed(bool flag) {
    if (flag == style().style_b(Sid::CONCERT_PITCH)) {
        return;
    }

    Transaction &tx = transaction_manager()->current_or_dummy_transaction();

    undo_change_style_val(Sid::CONCERT_PITCH, flag); // change style flag

    for (Staff *staff : staves_) {
        if (staff->staff_type(Fraction(0, 1))->group() == StaffGroup::PERCUSSION) { // TODO
            continue;
        }
        // if this staff has no transposition, and no instrument changes, we can skip it
        Interval interval = staff->part()->instrument()->transpose(); //tick?
        if (interval.is_zero() && staff->part()->instruments().size() == 1) {
            continue;
        }
        if (!flag) {
            interval.flip();
        }

        staff_idx_t staff_idx = staff->idx();
        track_idx_t start_track = staff_idx * ::mu::engraving::voices;
        track_idx_t end_track = start_track + ::mu::engraving::voices;

        Transpose::transpose_keys(tx, this, staff_idx, staff_idx + 1, Fraction(0, 1), last_segment()->tick(), !flag);

        for (Segment *segment = first_segment(SegmentType::CHORD_REST); segment; segment = segment->next1(SegmentType::CHORD_REST)) {
            interval = staff->transpose(segment->tick());
            if (!flag) {
                interval.flip();
            }
            for (EngravingItem *e : segment->annotations()) {
                if (!e->is_harmony() || (e->track() < start_track) || (e->track() >= end_track)) {
                    continue;
                }
                Harmony *h = to_harmony(e);
                for (EngravingObject *se : h->link_list()) {
                    // don't transpose all links
                    // just ones resulting from mmrests
                    Harmony *he = to_harmony(se); // toHarmony() does not work as e is an ScoreElement
                    if (he->staff() == h->staff()) {
                        Transpose::undo_transpose_harmony(tx, he, interval);
                    }
                }
                //realized harmony should be invalid after a transpose command
                assert(!h->realized_harmony().valid());
            }
        }
    }
}

static void on_focused_item_changed([[maybe_unused]] EngravingItem *item) {}

//---------------------------------------------------------
//   deselect
//---------------------------------------------------------

void Score::deselect(EngravingItem *el) {
    add_refresh(el->page_bounding_rect());
    selection_.remove(el);
    set_selection_changed(true);
    selection_.update();
}

//---------------------------------------------------------
//   select
//    staffIdx is valid, if element is of type MEASURE
//---------------------------------------------------------

void Score::select(EngravingItem *item, SelectType type, staff_idx_t staff_idx) {
    select(std::vector<EngravingItem *>{item}, type, staff_idx);
}

void Score::select(const std::vector<EngravingItem *> &items, SelectType type, staff_idx_t staff_idx) {
    for (EngravingItem *item : items) {
        do_select(item, type, staff_idx);
    }

    if (!selection_.elements().empty()) {
        on_focused_item_changed(selection_.elements().back());
    }
}

void Score::do_select(EngravingItem *e, SelectType type, staff_idx_t staff_idx) {
    if (MScore::debug_mode) {
        LOGD("select element <%s> type %d(state %d) staff %zu", e ? e->type_name() : "", int(type), int(selection().state()),
             e ? e->staff_idx() : -1);
    }

    switch (type) {
        case SelectType::SINGLE: select_single(e, staff_idx); break;
        case SelectType::ADD: select_add(e); break;
        case SelectType::RANGE: select_range(e, staff_idx); break;
        case SelectType::REPLACE: break;
    }
    set_selection_changed(true);
}

//---------------------------------------------------------
//   selectSingle
//    staffIdx is valid, if element is of type MEASURE
//---------------------------------------------------------

void Score::select_single(EngravingItem *e, staff_idx_t staff_idx) {
    SelState sel_state = selection_.state();
    deselect_all();
    if (e == 0) {
        sel_state = SelState::NONE;
        set_update_all();
    } else {
        if (e->is_measure()) {
            do_select(to_measure(e)->covering_mm_rest_or_this(), SelectType::RANGE, staff_idx);
            return;
        }
        add_refresh(e->page_bounding_rect());
        selection_.add(e);
        is_.set_track(e->track());
        sel_state = SelState::LIST;
        if (e->is_note()) {
            e = to_note(e)->chord();
        }
        if (e->is_chord_rest()) {
            is_.set_last_segment(is_.segment());
            is_.set_segment(to_chord_rest(e)->segment());
        }
    }
    selection_.set_active_segment(0);
    selection_.set_active_track(0);

    selection_.set_state(sel_state);
}

//---------------------------------------------------------
//   switchToPageMode
//---------------------------------------------------------

void Score::switch_to_page_mode() {
    if (layout_mode() != LayoutMode::PAGE) {
        set_layout_mode(LayoutMode::PAGE);
        do_layout();
    }
}

//---------------------------------------------------------
//   selectAdd
//---------------------------------------------------------

void Score::select_add(EngravingItem *e) {
    SelState sel_state = selection_.state();

    if (selection_.is_range()) {
        do_select(e, SelectType::SINGLE, 0);
        return;
    }

    if (e->is_measure()) {
        Measure *m = to_measure(e)->covering_mm_rest_or_this();
        Fraction tick = m->tick();
        if (selection_.is_none()) {
            selection_.set_range(m->tick2segment(tick), m == last_measure_mm() ? 0 : m->last(), 0, nstaves());
            set_update_all();
            sel_state = SelState::RANGE;
            selection_.update_selected_elements();
        }
    } else if (!muse::contains(selection_.elements(), e)) {
        add_refresh(e->page_bounding_rect());
        sel_state = SelState::LIST;
        selection_.add(e);
    }

    selection_.set_state(sel_state);
}

//---------------------------------------------------------
//   selectRange
//    staffIdx is valid, if element is of type MEASURE
//---------------------------------------------------------

static Segment *find_element_start_segment(Score *score, EngravingItem *e) {
    if (Segment *ancestor = to_segment(e->find_ancestor(ElementType::SEGMENT))) {
        if (ancestor->is_type(SegmentType::DURATION)) {
            return ancestor;
        }
    }

    return score->tick2segment_mm(e->tick(), true, SegmentType::DURATION);
}

/// Returns `nullptr` when the end segment is the end of the score;
/// returns `def` if no end segment is found.
static Segment *find_element_end_segment(Score *score, EngravingItem *e, Segment *def) {
    ChordRest *cr = nullptr;

    if (e->is_note()) {
        cr = to_note(e)->chord();
    } else if (e->is_chord_rest()) {
        cr = to_chord_rest(e);
    } else if (EngravingItem *c = e->find_ancestor(ElementType::CHORD)) {
        cr = to_chord_rest(c);
    } else if (EngravingItem *r = e->find_ancestor(ElementType::REST)) {
        cr = to_chord_rest(r);
    }

    if (cr) {
        return cr->next_segment_after_cr(SegmentType::CHORD_REST | SegmentType::END_BAR_LINE | SegmentType::CLEF);
    }

    if (e->is_spanner() || e->is_spanner_segment()) {
        Spanner *sp = e->is_spanner() ? to_spanner(e) : to_spanner_segment(e)->spanner();
        return score->tick2segment_mm(sp->tick2(), true, SegmentType::DURATION);
    }

    if (Segment *seg = to_segment(e->find_ancestor(ElementType::SEGMENT))) {
        if (seg->is_type(SegmentType::DURATION)) {
            // https://github.com/musescore/MuseScore/pull/25821#issuecomment-2617369881
            return seg->next_cr(e->track(), true);
        }
        // Strictly speaking redundant, but more efficient than `tick2segmentMM`
        else if (Segment *cr_seg_at_same_tick = seg->measure()->find_segment_r(SegmentType::DURATION, seg->rtick())) {
            return cr_seg_at_same_tick;
        }
    }

    const Fraction &tick = e->tick();
    if (tick == score->end_tick()) {
        return nullptr;
    }

    if (Segment *seg = score->tick2segment_mm(tick, true, SegmentType::DURATION)) {
        return seg;
    }

    return def;
}

static std::pair</*startStaff*/ staff_idx_t, /*endStaff*/ staff_idx_t> box_start_end_staves(const Box *box, size_t score_nstaves) {
    IF_ASSERT_FAILED(box) { return {muse::nidx, muse::nidx}; }

    if (box->is_v_box_base()) {
        if (score_nstaves == 0) {
            return {muse::nidx, muse::nidx};
        }
        return {0, score_nstaves};
    }

    const System *system = box->system();
    if (!system) {
        return {muse::nidx, muse::nidx};
    }

    const staff_idx_t first_visible = system->first_visible_staff();
    const staff_idx_t last_visible = system->last_visible_staff();
    if (first_visible == muse::nidx || last_visible == muse::nidx) {
        return {muse::nidx, muse::nidx};
    }
    return {first_visible, last_visible + 1};
}

void Score::select_range(EngravingItem *e, staff_idx_t staff_idx) {
    if (selection_.is_single()) {
        if (!e->is_measure_base() && !e->is_note() && !e->is_chord_rest()) {
            // Try to select similar in range
            bool success = try_select_similar_in_range(e);
            if (success) {
                return;
            }
        }

        // Try to extend single selection into range selection
        bool success = try_extend_single_selection_to_range(e, staff_idx);
        if (success) {
            return;
        }
    }

    if (e->is_measure()) {
        Measure *m = to_measure(e)->covering_mm_rest_or_this();
        Segment *start_segment = m->first(SegmentType::CHORD_REST);
        Segment *end_segment = m->last();
        Fraction tick = m->tick();
        Fraction etick = tick + m->ticks();

        if (selection_.is_range()) {
            // Extend existing range selection
            selection_.extend_range_selection(start_segment, end_segment, staff_idx, tick, etick);
        } else {
            // Create new range selection
            if (!selection_.is_none()) {
                deselect_all();
            }
            selection_.set_range(start_segment, end_segment, staff_idx, staff_idx + 1);
        }

        selection_.update_selected_elements();
        selection_.set_active_track(staff_idx * ::mu::engraving::voices);
        return;
    }

    if (selection_.is_range()) {
        // Try to extend existing range selection to e
        bool success = try_extend_range_selection_to_elem(e);
        if (success) {
            return;
        }
    }

    if (e->is_note() || e->is_chord_rest()) {
        // Create new range selection
        if (!selection_.is_none()) {
            deselect_all();
        }

        ChordRest *cr = e->is_note() ? static_cast<ChordRest *>(to_note(e)->chord()) : to_chord_rest(e);

        SegmentType st = SegmentType::CHORD_REST | SegmentType::END_BAR_LINE | SegmentType::CLEF;
        selection_.set_range(cr->segment(), cr->next_segment_after_cr(st), cr->staff_idx(), cr->staff_idx() + 1);
        selection_.update_selected_elements();

        selection_.set_active_track(cr->track());
        return;
    }

    // Nothing worked; just select the new element
    do_select(e, SelectType::SINGLE, staff_idx);
}

bool Score::try_select_similar_in_range(EngravingItem *e) {
    EngravingItem *selected_element = selection_.element();
    if (!selected_element || e->type() != selected_element->type()) {
        return false;
    }

    staff_idx_t idx1 = selected_element->staff_idx();
    staff_idx_t idx2 = e->staff_idx();

    if (idx1 == muse::nidx || idx2 == muse::nidx) {
        return false;
    }

    if (idx2 < idx1) {
        std::swap(idx1, idx2);
    }

    Fraction t1 = selected_element->tick();
    Fraction t2 = e->tick();
    if (t1 > t2) {
        std::swap(t1, t2);
    }

    Segment *s1 = tick2segment_mm(t1, true, SegmentType::CHORD_REST);
    Segment *s2 = tick2segment_mm(t2, true, SegmentType::CHORD_REST);
    if (s2) {
        s2 = s2->next1_mm(SegmentType::CHORD_REST);
    }

    if (!s1) {
        return false;
    }

    selection_.set_range(s1, s2, idx1, idx2 + 1);
    select_similar_in_range(e);
    if (selected_element->track() == e->track()) {
        // limit to this voice only
        const std::vector<EngravingItem *> &list = selection_.elements();
        for (EngravingItem *el : list) {
            if (el->track() != e->track()) {
                selection_.remove(el);
            }
        }
    }

    return true;
}

bool Score::try_extend_single_selection_to_range(EngravingItem *new_element, staff_idx_t staff_idx) {
    EngravingItem *selected_element = selection_.element();
    if (!selected_element) {
        return false;
    }

    Segment *start_segment = nullptr;
    Segment *end_segment = nullptr;

    Box *selected_box = selected_element->is_box() ? to_box(selected_element) : nullptr;
    if (selected_box) {
        if (selected_box->tick() == end_tick()) {
            start_segment = last_segment_mm();
        } else {
            start_segment = tick2segment_mm(selected_box->tick(), true, SegmentType::DURATION);
            end_segment = start_segment;
        }
    } else {
        start_segment = find_element_start_segment(this, selected_element);
        end_segment = find_element_end_segment(this, selected_element, start_segment);
    }
    if (!start_segment) { //! NOTE: endSegment is allowed to be null (meaning "extend to end of score")...
        return false;
    }

    Box *new_box = new_element->is_box() ? to_box(new_element) : nullptr;

    staff_idx_t start_staff_idx = muse::nidx;
    staff_idx_t end_staff_idx = muse::nidx;

    if (selected_box) {
        const std::pair<staff_idx_t, staff_idx_t> box_staves = box_start_end_staves(selected_box, nstaves());
        start_staff_idx = box_staves.first;
        end_staff_idx = box_staves.second;
    } else {
        start_staff_idx = selected_element->staff_idx();
        end_staff_idx = start_staff_idx + 1;
    }

    if (start_staff_idx == muse::nidx) {
        return false;
    }

    track_idx_t active_track = new_element->track();
    bool active_segment_is_start = false;
    bool extended_backwards = false;
    bool extended_forwards = false;

    // TODO: Quite a lot of nesting - should clean this up...
    if (new_element->is_measure()) {
        Measure *m = to_measure(new_element)->covering_mm_rest_or_this();
        const Fraction tick = m->tick();

        if (tick < start_segment->tick()) {
            start_segment = m->first(SegmentType::CHORD_REST);
            active_segment_is_start = true;
            extended_backwards = true;
        }
        if (m == last_measure_mm()) {
            if (end_segment) {
                extended_forwards = true;
            }
            end_segment = nullptr;
        } else if (end_segment && tick + m->ticks() > end_segment->tick()) {
            end_segment = m->last();
            extended_forwards = true;
        }

        start_staff_idx = std::min(start_staff_idx, staff_idx);
        end_staff_idx = std::max(end_staff_idx, staff_idx + 1);

        active_track = staff_idx * ::mu::engraving::voices;
    } else {
        Segment *new_start_segment = nullptr;
        Segment *new_end_segment = nullptr;
        if (new_box) {
            if (new_box->tick() == end_tick()) {
                new_start_segment = last_segment_mm();
            } else {
                new_start_segment = tick2segment_mm(new_box->tick(), true, SegmentType::DURATION);
                new_end_segment = new_start_segment;
            }
        } else {
            new_start_segment = find_element_start_segment(this, new_element);
            new_end_segment = find_element_end_segment(this, new_element, new_start_segment);
        }

        if (new_start_segment &&
            (new_start_segment->tick() != start_segment->tick() ? new_start_segment->tick() < start_segment->tick() :
                                                                  new_box && (!selected_box || new_box->is_before(selected_box)))) {
            start_segment = new_start_segment;
            active_segment_is_start = true;
            extended_backwards = true;
        }

        const Fraction new_end_tick = new_end_segment ? new_end_segment->tick() : end_tick();
        const Fraction range_end_tick = end_segment ? end_segment->tick() : end_tick();
        if (new_end_tick != range_end_tick ? new_end_tick > range_end_tick :
                                             new_box && (!selected_box || selected_box->is_before(new_box))) {
            end_segment = new_end_segment;
            extended_forwards = true;
        }

        staff_idx_t new_staff_idx = new_element->staff_idx();
        if (new_box) {
            const std::pair<staff_idx_t, staff_idx_t> box_staves = box_start_end_staves(new_box, nstaves());
            if (box_staves.first != muse::nidx) {
                start_staff_idx = std::min(start_staff_idx, box_staves.first);
                end_staff_idx = std::max(end_staff_idx, box_staves.second);
            }
            active_track = staff2track(start_staff_idx);
        } else if (new_staff_idx != muse::nidx) {
            start_staff_idx = std::min(start_staff_idx, new_staff_idx);
            end_staff_idx = std::max(end_staff_idx, new_staff_idx + 1);
        }
    }

    Box *start_box = extended_backwards ? new_box : selected_box;
    Box *end_box = extended_forwards ? new_box : selected_box;

    selection_.set_range(start_segment, end_segment, start_staff_idx, end_staff_idx, start_box, end_box);
    selection_.update_selected_elements();

    selection_.set_active_track(active_track);
    selection_.set_active_segment(active_segment_is_start ? start_segment : end_segment);

    return true;
}

bool Score::try_extend_range_selection_to_elem(EngravingItem *e) {
    Box *box = e->is_box() ? to_box(e) : nullptr;

    Segment *elem_start_seg = nullptr;
    Segment *elem_end_seg = nullptr;
    if (box) {
        if (box->tick() == end_tick()) {
            elem_start_seg = last_segment_mm();
        } else {
            elem_start_seg = tick2segment_mm(box->tick(), true, SegmentType::DURATION);
            elem_end_seg = elem_start_seg;
        }
    } else {
        elem_start_seg = find_element_start_segment(this, e);
        elem_end_seg = find_element_end_segment(this, e, selection_.end_segment());
    }

    if (!elem_start_seg) { //! NOTE: elemEndSeg allowed to be null (meaning "extend to end of score")...
        return false;
    }

    staff_idx_t element_staff_idx = muse::nidx;

    if (box) {
        const std::pair<staff_idx_t, staff_idx_t> box_staves = box_start_end_staves(box, nstaves());
        if (box_staves.first == muse::nidx) {
            return false;
        }
        selection_.set_staff_start(static_cast<int>(std::min(selection_.staff_start(), box_staves.first)));
        selection_.set_staff_end(static_cast<int>(std::max(selection_.staff_end(), box_staves.second)));
        element_staff_idx = box_staves.first;
    } else {
        element_staff_idx = e->staff_idx();
    }

    if (element_staff_idx == muse::nidx) {
        return false;
    }

    const Fraction elem_start_tick = elem_start_seg->tick();
    const Fraction elem_end_tick = elem_end_seg ? elem_end_seg->tick() : end_tick();

    selection_.extend_range_selection(elem_start_seg, elem_end_seg, element_staff_idx, elem_start_tick, elem_end_tick, box);

    selection_.update_selected_elements();
    selection_.set_active_track(box ? staff2track(selection_.staff_start()) : e->track());
    return true;
}

//---------------------------------------------------------
//   collectMatch
//---------------------------------------------------------

void Score::collect_match(ElementPattern *p, EngravingItem *e) {
    if (p->type != int(ElementType::INVALID) && p->type != int(e->type())) {
        return;
    }

    if (p->type == int(ElementType::NOTE)) {
        if (p->subtype < 0) {
            if (!(to_note(e)->chord()->is_grace())) {
                return;
            }
        } else if ((to_note(e)->chord()->is_grace()) || (p->subtype != e->subtype())) {
            return;
        }
    } else if (p->subtype_valid && p->subtype != e->subtype()) {
        return;
    }

    if ((p->staff_start != muse::nidx) && ((p->staff_start > e->staff_idx()) || (p->staff_end <= e->staff_idx()))) {
        return;
    }

    if (p->voice != muse::nidx && p->voice != e->voice()) {
        return;
    }

    if (p->system) {
        EngravingItem *ee = e;
        do {
            if (ee->is_system()) {
                if (p->system != ee) {
                    return;
                }
                break;
            }
            ee = ee->layout_parent();
        } while (ee);
    }

    if (e->is_rest() && p->duration_ticks != Fraction(-1, 1)) {
        const Rest *r = to_rest(e);
        if (p->duration_ticks != r->actual_ticks()) {
            return;
        }
    }

    if (p->measure) {
        auto e_measure = e->find_measure();
        if (!e_measure && e->is_spanner_segment()) {
            if (auto s = to_spanner_segment(e)->spanner()) {
                if (auto se = s->start_element()) {
                    if (auto mse = se->find_measure()) {
                        e_measure = mse;
                    }
                }
            }
        }
        if (p->measure != e_measure) {
            return;
        }
    }

    if ((p->beat.is_valid()) && (p->beat != e->beat())) {
        return;
    }

    p->el.push_back(e);
}

//---------------------------------------------------------
//   collectNoteMatch
//---------------------------------------------------------

void Score::collect_note_match(NotePattern *p, EngravingItem *e) {
    if (!e->is_note()) {
        return;
    }
    Note *n = to_note(e);
    if (p->type != NoteType::INVALID && p->type != n->note_type()) {
        return;
    }
    if (p->pitch != -1 && p->pitch != n->pitch()) {
        return;
    }
    if (p->string != ::mu::engraving::invalid_string_index && p->string != n->string()) {
        return;
    }
    if (p->tpc != Tpc::TPC_INVALID && p->tpc != n->tpc()) {
        return;
    }
    if (p->notehead != NoteHeadGroup::HEAD_INVALID && p->notehead != n->head_group()) {
        return;
    }
    if (p->duration_type.type() != DurationType::V_INVALID && p->duration_type != n->chord()->actual_duration_type()) {
        return;
    }
    if (p->duration_ticks != Fraction(-1, 1) && p->duration_ticks != n->chord()->actual_ticks()) {
        return;
    }
    if ((p->staff_start != muse::nidx) && ((p->staff_start > e->staff_idx()) || (p->staff_end <= e->staff_idx()))) {
        return;
    }
    if (p->voice != muse::nidx && p->voice != e->voice()) {
        return;
    }
    if (p->system && (p->system != n->chord()->segment()->system())) {
        return;
    }
    if (p->measure && (p->measure != n->find_measure())) {
        return;
    }
    if ((p->beat.is_valid()) && (p->beat != n->beat())) {
        return;
    }
    p->el.push_back(n);
}

//---------------------------------------------------------
//   selectSimilar
//---------------------------------------------------------

void Score::select_similar(EngravingItem *e, bool same_staff) {
    Score *score = e->score();

    ElementPattern pattern;
    pattern.type = int(e->type());
    pattern.subtype = 0;
    pattern.subtype_valid = false;
    if (e->is_note()) {
        if (to_note(e)->chord()->is_grace()) {
            pattern.subtype = -1; // hack
        } else {
            pattern.subtype = e->subtype();
        }
    } else if (e->is_hairpin_segment() || e->is_harmony()) {
        pattern.subtype = e->subtype();
        pattern.subtype_valid = true;
    }
    pattern.staff_start = same_staff ? e->staff_idx() : muse::nidx;
    pattern.staff_end = same_staff ? e->staff_idx() + 1 : muse::nidx;
    pattern.voice = muse::nidx;

    score->scan_elements([&](EngravingItem *item) { collect_match(&pattern, item); });

    score->select(0, SelectType::SINGLE, 0);
    score->select(pattern.el, SelectType::ADD, 0);
}

//---------------------------------------------------------
//   selectSimilarInRange
//---------------------------------------------------------

void Score::select_similar_in_range(EngravingItem *e) {
    Score *score = e->score();

    ElementPattern pattern;
    pattern.type = int(e->type());
    pattern.subtype = 0;
    pattern.subtype_valid = false;
    if (e->is_note()) {
        if (to_note(e)->chord()->is_grace()) {
            pattern.subtype = -1; //hack
        } else {
            pattern.subtype = e->subtype();
        }
        pattern.subtype_valid = true;
    } else if (e->is_hairpin_segment() || e->is_harmony()) {
        pattern.subtype = e->subtype();
        pattern.subtype_valid = true;
    }
    pattern.staff_start = selection().staff_start();
    pattern.staff_end = selection().staff_end();
    pattern.voice = muse::nidx;

    score->scan_elements_in_range([&](EngravingItem *item) { collect_match(&pattern, item); });

    score->select(0, SelectType::SINGLE, 0);
    score->select(pattern.el, SelectType::ADD, 0);
}

//---------------------------------------------------------
//   scoreOrder
//---------------------------------------------------------

ScoreOrder Score::score_order() const {
    ScoreOrder order = score_order_;
    order.customized = !order.is_score_order(this);
    return order;
}

//---------------------------------------------------------
//   setScoreOrder
//---------------------------------------------------------

void Score::set_score_order(ScoreOrder order) { score_order_ = order; }

//---------------------------------------------------------
//   updateBracesAndBarlines
//---------------------------------------------------------

void Score::update_braces_and_barlines(Part *part, size_t new_index) {
    bool no_braces_found = true;
    for (size_t index_in_part = 0; index_in_part < part->nstaves(); ++index_in_part) {
        size_t index_in_score = part->staves()[index_in_part]->idx();
        const std::vector<BracketItem *> &staff_brackets = brackets(index_in_score);
        for (BracketItem *bi : staff_brackets) {
            no_braces_found = false;
            if (bi->bracket_type() == BracketType::BRACE) {
                if ((index_in_part <= new_index) && (new_index <= (bi->bracket_span()))) {
                    bi->undo_change_property(Pid::BRACKET_SPAN, bi->bracket_span() + 1);
                }
            }
        }
    }

    auto update_bracket_span = [this, part](size_t mod_index, size_t ref_index) {
        Staff *mod_staff = staff(part->staves()[mod_index]->idx());
        Staff *ref_staff = staff(part->staves()[ref_index]->idx());
        mod_staff->undo_change_property(Pid::STAFF_BARLINE_SPAN, ref_staff->get_property(Pid::STAFF_BARLINE_SPAN));
    };

    if (new_index >= 2) {
        update_bracket_span(new_index - 1, new_index - 2);
    }

    if (part->nstaves() == 2) {
        const InstrumentTemplate *tp = search_template(part->instrument_id());
        if (tp) {
            const bool first_staff_bar_line_span = tp->barline_span[0];
            if (first_staff_bar_line_span) {
                staff(part->staff(0)->idx())->undo_change_property(Pid::STAFF_BARLINE_SPAN, first_staff_bar_line_span);
            }
            if (no_braces_found && (tp->bracket[0] != BracketType::NO_BRACKET)) {
                EditStaffBrackets::undo_add_bracket(this, part->staves()[0], 0, tp->bracket[0], part->nstaves());
            }
        }
    } else {
        update_bracket_span(new_index, new_index - 1);
    }
}

//---------------------------------------------------------
//   setBracketsAndBarlines
//---------------------------------------------------------

void Score::set_brackets_and_barlines() { score_order().set_brackets_and_barlines(this); }

//---------------------------------------------------------
//   remapBracketsAndBarlines
///  Reconstructs brackets and barlines based on how they
///  are in the masterScore. Must be called after creating
///  an excerpt or adding new parts to it.
//---------------------------------------------------------

void Score::remap_brackets_and_barlines() {
    if (is_master()) {
        return;
    }

    // Remove all brackets
    for (Staff *staff : staves()) {
        std::vector<BracketItem *> &staff_brackets = brackets(staff->idx());
        for (BracketItem *bracket : staff_brackets) {
            bracket->set_bracket_type(BracketType::NO_BRACKET);
        }
    }
    // Remap all brackets from masterScore
    Score *master = master_score();
    for (staff_idx_t master_staff_idx = 0; master_staff_idx < master->nstaves(); ++master_staff_idx) {
        Staff *master_staff = master->staff(master_staff_idx);
        std::vector<BracketItem *> &staff_brackets = master->brackets(master_staff_idx);

        for (size_t bracket_idx = 0; bracket_idx < staff_brackets.size(); ++bracket_idx) {
            BracketItem *bracket = staff_brackets.at(bracket_idx);
            Staff *first_bracketed = nullptr;
            int span = 0;

            // There is no guarantee that bracket->bracketSpan() is correctly bounded,
            // so masterStaffIdx + bracket->bracketSpan() might be > master->nstaves(),
            // in which case masterStaff below will become null.
            size_t end_spanned_master_staff_idx = std::min(master_staff_idx + bracket->bracket_span(), master->nstaves());

            for (staff_idx_t spanned_master_st_idx = master_staff_idx; spanned_master_st_idx < end_spanned_master_staff_idx;
                 ++spanned_master_st_idx) {
                master_staff = master->staff(spanned_master_st_idx);
                IF_ASSERT_FAILED(master_staff) { continue; }

                Staff *linked_staff = to_staff(master_staff->find_linked_in_score(this));
                if (!linked_staff) {
                    continue;
                }

                if (!first_bracketed) {
                    first_bracketed = linked_staff;
                }

                ++span;
            }

            if (first_bracketed && span > 1) {
                EditStaffBrackets::set_bracket_type(this, first_bracketed->idx(), bracket_idx, bracket->bracket_type());
                EditStaffBrackets::set_bracket_span(this, first_bracketed->idx(), bracket_idx, span);
            }
        }
    }

    // Remap all barline spans from masterScore
    for (Staff *staff : staves()) {
        Staff *master_staff = to_staff(staff->find_linked_in_score(master_score()));
        if (!master_staff || !master_staff->bar_line_span()) {
            continue;
        }
        // Look in the masterScore for all the staves spanned by a common barline.
        // If at least one of them is also in this score, then connect it through.
        bool extend_barline = false;
        bool span = master_staff->bar_line_span();
        while (!extend_barline && span && master_staff->idx() + 1 < master->nstaves()) {
            master_staff = master_score()->staff(master_staff->idx() + 1);
            span = master_staff->bar_line_span();
            if (master_staff->find_linked_in_score(this)) {
                extend_barline = true;
                break;
            }
        }
        if (extend_barline) {
            staff->set_bar_line_span(true);
        }
    }
}

//---------------------------------------------------------
//   addLyrics
//---------------------------------------------------------

void Score::add_lyrics(const Fraction &tick, staff_idx_t staff_idx, const String &txt) {
    if (txt.trimmed().is_empty()) {
        return;
    }
    Measure *measure = tick2measure(tick);
    Segment *seg = measure->find_segment(SegmentType::CHORD_REST, tick);
    if (seg == 0) {
        LOGD("no segment found for lyrics<%s> at tick %d", muPrintable(txt), tick.ticks());
        return;
    }

    bool lyrics_added = false;
    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        track_idx_t track = staff_idx * ::mu::engraving::voices + voice;
        ChordRest *cr = to_chord_rest(seg->element(track));
        if (cr) {
            Lyrics *l = Factory::create_lyrics(cr);
            l->set_xml_text(txt);
            l->set_track(track);
            cr->add(l);
            lyrics_added = true;
            break;
        }
    }
    if (!lyrics_added) {
        LOGD("no chord/rest for lyrics<%s> at tick %d, staff %zu", muPrintable(txt), tick.ticks(), staff_idx);
    }
}

BeatsPerSecond Score::tempo(const Fraction &tick) const { return tempo_timeline(/*expandRepeats*/ false).tempo(tick.ticks()); }

BeatsPerSecond Score::multiplied_tempo(const Fraction &tick) const {
    const TempoTimeline &timeline = tempo_timeline(/*expandRepeats*/ false);
    return timeline.tempo(tick.ticks()) * timeline.tempo_multiplier();
}

BeatsPerSecond Score::multiplied_tempo_at_utick(int utick) const {
    // utick is only truly expanded when repeats are expanded; otherwise it's a plain tick
    const TempoTimeline &timeline = tempo_timeline();
    return timeline.tempo(utick) * timeline.tempo_multiplier();
}

//---------------------------------------------------------
//   cmdSelectAll
//---------------------------------------------------------

void Score::cmd_select_all() {

    if (measures_.size() == 0) {
        return;
    }
    deselect_all();
    select_range(first_mm(), 0);
    select_range(!last()->is_measure() ? last() : last_measure_mm(), nstaves() - 1);
    set_update_all();
    update();
}

//---------------------------------------------------------
//   cmdSelectSection
//---------------------------------------------------------

void Score::cmd_select_section() {

    Segment *s = selection_.start_segment();
    if (s == 0) {
        return;
    }
    MeasureBase *sm = s->measure();
    MeasureBase *em = sm;
    while (sm->prev()) {
        if (sm->prev()->section_break()) {
            break;
        }
        sm = sm->prev();
    }
    while (em->next()) {
        if (em->section_break()) {
            break;
        }
        em = em->next();
    }
    while (sm && !sm->is_measure()) {
        sm = sm->next();
    }
    while (em && !em->is_measure()) {
        em = em->next();
    }
    if (sm == 0 || em == 0) {
        return;
    }

    selection_.set_range(to_measure(sm)->first(), to_measure(em)->last(), 0, nstaves());

    set_update_all();
    update();
}

//---------------------------------------------------------
//   scoreList
//    return a list of scores containing the root score
//    and all part scores (if there are any)
//---------------------------------------------------------

std::list<Score *> Score::score_list() {
    std::list<Score *> scores;
    MasterScore *root = master_score();
    scores.push_back(root);
    for (const Excerpt *ex : root->excerpts()) {
        if (ex->excerpt_score()) {
            scores.push_back(ex->excerpt_score());
        }
    }
    return scores;
}

//---------------------------------------------------------
//   appendPart
//---------------------------------------------------------

void Score::append_part(const InstrumentTemplate *t) {
    Part *part = new Part(this);
    part->init_from_instr_template(t);
    for (staff_idx_t i = 0; i < t->staff_count; ++i) {
        Staff *staff = Factory::create_staff(part);
        StaffType *stt = staff->staff_type(Fraction(0, 1));
        undo_insert_staff(staff, i);
        staff->init(t, stt, int(i));
    }
    undo_insert_part(part, parts_.size());
    master_score()->rebuild_midi_mapping();
}

//---------------------------------------------------------
//   appendMeasures
//---------------------------------------------------------

void Score::append_measures(int n) {
    InsertMeasureOptions options;
    options.create_measure_rests = false;

    for (int i = 0; i < n; ++i) {
        insert_measure(ElementType::MEASURE, nullptr, options);
    }
}

//---------------------------------------------------------
//   addSpanner
//---------------------------------------------------------

void Score::add_spanner(Spanner *s, bool compute_start_end) {
    spanner_.add_spanner(s);
    s->added();
    if (compute_start_end) {
        s->compute_start_element();
        s->compute_end_element();
    }
}

//---------------------------------------------------------
//   spannerList
//---------------------------------------------------------

std::vector<Spanner *> Score::spanner_list() const {
    std::vector<Spanner *> result;
    const std::multimap<int, Spanner *> &spanner_map = spanner_.map();
    result.reserve(spanner_map.size());
    for (auto it = spanner_map.begin(); it != spanner_map.end(); ++it) {
        result.push_back(it->second);
    }
    return result;
}

//---------------------------------------------------------
//   removeSpanner
//---------------------------------------------------------

void Score::remove_spanner(Spanner *s) {
    spanner_.remove_spanner(s);
    s->removed();

    EngravingItem *start_element = s->start_element();
    Chord *start_chord = start_element && start_element->is_chord() ? to_chord(start_element) : nullptr;
    if (start_chord) {
        start_chord->remove_starting_spanner(s);
    }

    EngravingItem *end_element = s->end_element();
    Chord *end_chord = end_element && end_element->is_chord() ? to_chord(end_element) : nullptr;
    if (end_chord) {
        end_chord->remove_ending_spanner(s);
    }
}

//---------------------------------------------------------
//   isSpannerStartEnd
//    does is spanner start or end at tick position tick
//    for track ?
//---------------------------------------------------------

bool Score::is_spanner_start_end(const Fraction &tick, track_idx_t track) const {
    for (auto i : spanner_.map()) {
        if (i.second->track() != track) {
            continue;
        }
        if (i.second->tick() == tick || i.second->tick2() == tick) {
            return true;
        }
    }
    return false;
}

void Score::insert_time(const Fraction &tick, const Fraction &len, const std::vector<RepeatSegmentInfo> &old_segments) {
    for (Staff *staff : staves()) {
        staff->insert_time(tick, len);
    }
    for (Part *part : parts()) {
        part->insert_time(tick, len);
    }

    on_time_inserted(tick, len, old_segments);
}

void Score::on_time_inserted(const Fraction &, const Fraction &, const std::vector<RepeatSegmentInfo> &) {}

//---------------------------------------------------------
//   addUnmanagedSpanner
//---------------------------------------------------------

void Score::add_unmanaged_spanner(Spanner *s) { unmanaged_spanner_.insert(s); }

//---------------------------------------------------------
//   removeSpanner
//---------------------------------------------------------

void Score::remove_unmanaged_spanner(Spanner *s) { unmanaged_spanner_.erase(s); }

//---------------------------------------------------------
//   uniqueStaves
//---------------------------------------------------------

std::vector<staff_idx_t> Score::unique_staves() const {
    std::vector<staff_idx_t> sl;

    for (size_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
        Staff *s = staff(staff_idx);
        if (s->links()) {
            bool already_in_list = false;
            for (staff_idx_t idx : sl) {
                if (s->links()->contains(staff(idx))) {
                    already_in_list = true;
                    break;
                }
            }
            if (already_in_list) {
                continue;
            }
        }
        sl.push_back(staff_idx);
    }
    return sl;
}

//---------------------------------------------------------
//   findCR
//    find chord/rest <= tick in track
//---------------------------------------------------------

ChordRest *Score::find_cr(Fraction tick, track_idx_t track) const {
    Measure *m = tick2measure_mm(tick);
    if (!m) {
        //LOGD("findCR: no measure for tick %d", tick.ticks());
        return nullptr;
    }
    // attach to first rest all spanner when mmRest
    if (m->is_mm_rest()) {
        tick = m->tick();
    }
    Segment *s = m->first(SegmentType::CHORD_REST);
    for (Segment *ns = s;; ns = ns->next(SegmentType::CHORD_REST)) {
        if (!ns || ns->tick() > tick) {
            break;
        }
        EngravingItem *el = ns->element(track);
        if (el && el->is_rest() && to_rest(el)->is_gap()) {
            continue;
        } else if (el) {
            s = ns;
        }
    }

    if (!s) {
        return nullptr;
    }
    EngravingItem *el = s->element(track);
    if (el && el->is_rest() && to_rest(el)->is_gap()) {
        return nullptr;
    }

    return to_chord_rest(el);
}

//---------------------------------------------------------
//   findChordRestEndingBeforeTickInStaff
//    find last chord/rest on staff that ends before tick
//---------------------------------------------------------

ChordRest *Score::find_chord_rest_ending_before_tick_in_staff(const Fraction &tick, staff_idx_t staff_idx) const {
    return find_chord_rest_ending_before_tick_in_staff_and_voice(tick, staff_idx, false, 0);
}

//-----------------------------------------------------------------
//   findChordRestEndingBeforeTickInStaffAndVoice
//    find last chord/rest on staff and voice that ends before tick
//-----------------------------------------------------------------
ChordRest *Score::find_chord_rest_ending_before_tick_in_staff_and_voice(const Fraction &tick, staff_idx_t staff_idx,
                                                                        voice_idx_t voice) const {
    return find_chord_rest_ending_before_tick_in_staff_and_voice(tick, staff_idx, true, voice);
}

//-----------------------------------------------------------------
//   findChordRestEndingBeforeTickInStaffAndVoice
//    find last chord/rest on staff and voice that ends before tick
//    allow no specific voice
//-----------------------------------------------------------------
ChordRest *Score::find_chord_rest_ending_before_tick_in_staff_and_voice(const Fraction &tick, staff_idx_t staff_idx, bool force_voice,
                                                                        voice_idx_t voice) const {
    Fraction ptick = tick - Fraction::eps();
    Measure *m = normalize_state.running ? tick2measure(ptick) : tick2measure_mm(ptick);
    if (!m) {
        LOGD("findCRinStaff: no measure for tick %d", ptick.ticks());
        return nullptr;
    }
    // attach to first rest all spanner when mmRest
    if (m->is_mm_rest()) {
        ptick = m->tick();
    }

    Segment *s = m->first(SegmentType::CHORD_REST);
    track_idx_t strack = force_voice ? staff_idx * ::mu::engraving::voices + voice : staff_idx * ::mu::engraving::voices;
    track_idx_t etrack = force_voice ? strack + 1 : strack + ::mu::engraving::voices;
    track_idx_t actual_track = strack;

    Fraction last_tick = Fraction(-1, 1);
    for (Segment *ns = s;; ns = ns->next(SegmentType::CHORD_REST)) {
        if (!ns || ns->tick() > ptick) {
            break;
        }
        // found a segment; now find longest cr on this staff that does not overlap tick
        for (track_idx_t t = strack; t < etrack; ++t) {
            ChordRest *cr = to_chord_rest(ns->element(t));
            if ((cr) && ((!force_voice) || voice == cr->voice())) {
                Fraction end_tick = cr->end_tick();
                if (end_tick >= last_tick && end_tick <= tick) {
                    s = ns;
                    actual_track = t;
                    last_tick = end_tick;
                }
            }
        }
    }
    if (s) {
        return to_chord_rest(s->element(actual_track));
    }
    return nullptr;
}

ChordRest *Score::find_chord_rest_ending_before_tick_in_track(const Fraction &tick, track_idx_t track_idx) const {
    Measure *m = normalize_state.running ? tick2measure(tick - Fraction::eps()) : tick2measure_mm(tick - Fraction::eps());
    if (!m) {
        LOGD("findCRinStaff: no measure for tick %d", tick.ticks());
        return nullptr;
    }

    for (const Segment *s = m->last(SegmentType::CHORD_REST); s; s = s->prev(SegmentType::CHORD_REST)) {
        if (ChordRest *cr = to_chord_rest(s->element(track_idx))) {
            if (cr->end_tick() <= tick) {
                return cr;
            }
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   cmdNextPrevSystem
//---------------------------------------------------------

ChordRest *Score::cmd_next_prev_system(ChordRest *cr, bool next) {
    IF_ASSERT_FAILED(cr) { return nullptr; }

    auto new_cr = cr;
    auto current_measure = cr->measure();
    auto current_system = current_measure->system() ? current_measure->system() : current_measure->covering_mm_rest_or_this()->system();
    if (!current_system) {
        return cr;
    }
    auto destination_measure = current_system->first_measure();
    if (!destination_measure) {
        return cr;
    }

    auto first_segment = destination_measure->first(SegmentType::CHORD_REST);

    // Case: Go to next system
    if (next) {
        if ((destination_measure = current_system->last_measure()->next_measure())) {
            // There is a next system present: get it and accommodate for MMRest
            current_system =
                destination_measure->system() ? destination_measure->system() : destination_measure->covering_mm_rest_or_this()->system();
            if (!current_system) {
                return cr;
            }
            if ((destination_measure = current_system->first_measure())) {
                if ((new_cr = destination_measure->first()->next_chord_rest(track_zero_voice(cr->track()), false))) {
                    cr = new_cr;
                }
            }
        } else if (current_measure != last_measure()) {
            // There is no next system present: go to last measure of current system
            if ((destination_measure = last_measure())) {
                if ((new_cr = destination_measure->first()->next_chord_rest(track_zero_voice(cr->track()), false))) {
                    if (!destination_measure->is_mm_rest()) {
                        cr = new_cr;
                    }
                    // Last visual measure is a MMRest: go to very last measure within that MMRest
                    else if ((destination_measure = last_measure_mm()) &&
                             (new_cr = destination_measure->first()->next_chord_rest(track_zero_voice(cr->track()), false))) {
                        cr = new_cr;
                    }
                }
            }
        }
    }
    // Case: Go to previous system
    else {
        auto current_segment = cr->segment();
        // Only go to previous system's beginning if user is already at the absolute beginning of current system
        // and not in first measure of entire score
        if ((destination_measure != first_measure() && destination_measure != first_measure_mm()) &&
            (current_segment == first_segment || (current_measure->mm_rest() && current_measure->mm_rest()->is_first_in_system()))) {
            if (!(destination_measure = destination_measure->prev_measure_mm())) {
                return cr;
            }
            if (!(current_system = destination_measure->system() ? destination_measure->system() :
                                                                   destination_measure->covering_mm_rest_or_this()->system())) {
                return cr;
            }
            destination_measure = current_system->first_measure();
        }
        if (destination_measure) {
            if ((new_cr = destination_measure->first()->next_chord_rest(track_zero_voice(cr->track()), false))) {
                cr = new_cr;
            }
        }
    }
    return cr;
}

//---------------------------------------------------------
//   cmdNextPrevFrame
//   Return next/previous [Vertical/Horizontal/Text] frame
//   to be used as a navigation command
//---------------------------------------------------------

Box *Score::cmd_next_prev_frame(MeasureBase *current_measure_base, bool next) const {
    Box *selected_box{nullptr};
    while (!selected_box && (current_measure_base = (next ? current_measure_base->next() : current_measure_base->prev()))) {
        if (current_measure_base->is_box()) {
            selected_box = to_box(current_measure_base);
        }
    }
    return selected_box;
}

//---------------------------------------------------------
//   cmdNextPrevSection
//    Return [Box* or ChordRest*] of next/previous section
//---------------------------------------------------------

EngravingItem *Score::cmd_next_prev_section(EngravingItem *el, bool dir) const {
    auto current_measure_base = el->find_measure_base();
    auto destination = current_measure_base;
    if (current_measure_base) {
        // -----------------------
        // Next Section of Score
        // -----------------------
        if (dir) {
            if ((destination = get_next_prev_section_break(current_measure_base, true))) {
                el = get_score_element_of_measure_base(destination->next());
            }
        }
        // -------------------------
        // Previous Section of Score
        // -------------------------
        else {
            auto current_segment = el->is_chord_rest() ? to_chord_rest(el)->segment() : nullptr;
            if ((destination = get_next_prev_section_break(current_measure_base, false))) {
                if (current_segment) {
                    if ((el = get_score_element_of_measure_base((score()->first() == destination) ? destination : destination->next()))) {
                        if (el->is_chord_rest() && (to_chord_rest(el)->segment() == current_segment)) {
                            if ((destination = get_next_prev_section_break(destination, false))) {
                                el = !(destination->section_break()) ? destination : get_score_element_of_measure_base(destination->next());
                            }
                        }
                    }
                } else if ((score()->first() != current_measure_base) && (el = get_score_element_of_measure_base(destination->next()))) {
                    if (el->find_measure_base() == current_measure_base) {
                        if ((destination = get_next_prev_section_break(destination, false))) {
                            el = !(destination->section_break()) ? el : get_score_element_of_measure_base(destination->next());
                        }
                    }
                }
            }
        }
    }
    return el;
}

//---------------------------------------------------------
//   getNextPrevSectionBreak
//    Condition: MeasureBase* must be valid before call
//    If no previous section break exists selects first
//    MeasureBase within score
//---------------------------------------------------------

MeasureBase *Score::get_next_prev_section_break(MeasureBase *mb, bool dir) const {
    auto destination = mb;
    if (destination) {
        if (dir) {
            // Find next section break
            auto end_of_section{false};
            while (!end_of_section) {
                if ((destination = destination->next())) {
                    end_of_section = destination->section_break();
                } else {
                    break;
                }
            }
        } else {
            // Find previous section break
            auto in_current_section{true};
            while (in_current_section && destination) {
                if (destination->index()) {
                    // Safety: SegFaults if invoking prev() when index=0
                    //         even when MeasureBase* is valid!
                    destination = destination->prev();
                    in_current_section = !(destination->section_break());
                } else {
                    destination = nullptr;
                }
            }
            if (in_current_section || !destination) {
                destination = score()->first();
            }
        }
    }
    return destination;
}

//---------------------------------------------------------
//   getScoreElementOfMeasureBase
//    Helper function
//    Get an EngravingItem* as Box or ChordRest depending on
//    MeasureBase
//---------------------------------------------------------

EngravingItem *Score::get_score_element_of_measure_base(MeasureBase *mb) const {
    EngravingItem *el{nullptr};
    ChordRest *cr{nullptr};
    const Measure *current_measure{nullptr};
    if (mb) {
        if (mb->is_box()) {
            el = to_box(mb);
        } else if ((current_measure = mb->find_measure())) {
            // Accommodate for MMRest
            if (style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && current_measure->has_mm_rest()) {
                current_measure = current_measure->covering_mm_rest_or_this();
            }
            if ((cr = current_measure->first()->next_chord_rest(0, false))) {
                el = cr;
            }
        }
    }
    return el;
}

//---------------------------------------------------------
//   nmeasure
//---------------------------------------------------------

size_t Score::nmeasures() const {
    size_t n = 0;
    for (const Measure *m = first_measure(); m; m = m->next_measure()) {
        n++;
    }
    return n;
}

//---------------------------------------------------------
//   firstTrailingMeasure
//---------------------------------------------------------

Measure *Score::first_trailing_measure(ChordRest **cr) {
    Measure *first_measure = nullptr;
    auto m = last_measure();

    if (!cr) {
        // No active selection: prepare first empty trailing measure of entire score
        while (m && m->is_empty(muse::nidx)) {
            first_measure = m;
            m = m->prev_measure();
        }
    } else {
        // Active selection: select full measure rest of active staff's empty trailing measure
        ChordRest *temp_cr = *cr;
        while (m && (temp_cr = m->first()->next_chord_rest(track_zero_voice((*cr)->track()), false))->is_full_measure_rest()) {
            *cr = temp_cr;
            first_measure = m;
            m = m->prev_measure();
        }
    }

    return first_measure;
}

//---------------------------------------------------------
//   hasLyrics
//---------------------------------------------------------

bool Score::has_lyrics() const {
    if (!first_measure()) {
        return false;
    }

    SegmentType st = SegmentType::CHORD_REST;
    for (Segment *seg = first_measure()->first(st); seg; seg = seg->next1(st)) {
        for (size_t i = 0; i < ntracks(); ++i) {
            ChordRest *cr = to_chord_rest(seg->element(static_cast<int>(i)));
            if (cr && !cr->lyrics().empty()) {
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------------------
//   hasHarmonies
//---------------------------------------------------------

bool Score::has_harmonies() const {
    if (!first_measure()) {
        return false;
    }

    SegmentType st = SegmentType::CHORD_REST;
    for (Segment *seg = first_measure()->first(st); seg; seg = seg->next1(st)) {
        for (EngravingItem *e : seg->annotations()) {
            if (e->is_harmony()) {
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------------------
//   lyricCount
//---------------------------------------------------------

int Score::lyric_count() const { return int(lyrics().size()); }

std::vector<Lyrics *> Score::lyrics() const {
    std::vector<Lyrics *> result;
    SegmentType st = SegmentType::CHORD_REST;
    for (size_t track = 0; track < ntracks(); track++) {
        size_t max_lyrics = 1;
        for (Measure *m = first_measure(); m; m = m->next_measure()) {
            m->set_playback_count(0);
        }
        // follow the repeat segments
        const RepeatList &rlist = expanded_repeat_list();
        for (const RepeatSegment *rs : rlist) {
            Fraction start_tick = Fraction::from_ticks(rs->tick);
            Fraction end_tick = Fraction::from_ticks(rs->end_tick());
            for (Measure *m = tick2measure(start_tick); m; m = m->next_measure()) {
                size_t play_count = m->playback_count();
                for (Segment *seg = m->first(st); seg; seg = seg->next(st)) {
                    ChordRest *cr = to_chord_rest(seg->element(track));
                    if (!cr || cr->lyrics().empty()) {
                        continue;
                    }
                    if (cr->lyrics().size() > max_lyrics) {
                        max_lyrics = cr->lyrics().size();
                    }
                    if (play_count > cr->lyrics().size()) {
                        continue;
                    }
                    Lyrics *l = cr->lyrics(static_cast<int>(play_count));
                    if (!l) {
                        continue;
                    }
                    result.push_back(l);
                }
                m->set_playback_count(m->playback_count() + 1);
                if (m->end_tick() >= end_tick) {
                    break;
                }
            }
        }
        // consider remaining lyrics
        for (size_t lyrics_number = 0; lyrics_number < max_lyrics; lyrics_number++) {
            for (Measure *m = first_measure(); m; m = m->next_measure()) {
                size_t play_count = m->playback_count();
                if (lyrics_number < play_count) {
                    continue;
                }
                for (Segment *seg = m->first(st); seg; seg = seg->next(st)) {
                    ChordRest *cr = to_chord_rest(seg->element(track));
                    if (!cr || cr->lyrics().empty()) {
                        continue;
                    }
                    if (cr->lyrics().size() > max_lyrics) {
                        max_lyrics = cr->lyrics().size();
                    }
                    if (lyrics_number > cr->lyrics().size()) {
                        continue;
                    }
                    Lyrics *l = cr->lyrics(static_cast<int>(lyrics_number));
                    if (!l) {
                        continue;
                    }
                    result.push_back(l);
                }
            }
        }
    }
    return result;
}

//---------------------------------------------------------
//   extractLyrics
//---------------------------------------------------------

String Score::extract_lyrics() const {
    String result;
    std::vector<Lyrics *> list = lyrics();
    for (const Lyrics *l : list) {
        String lyric = l->plain_text().trimmed();
        LyricsSyllabic ls = l->syllabic();
        if (ls == LyricsSyllabic::SINGLE || ls == LyricsSyllabic::END) {
            result += lyric + u" ";
        } else if (ls == LyricsSyllabic::BEGIN || ls == LyricsSyllabic::MIDDLE) {
            result += lyric;
        }
    }
    return result.trimmed();
}

//---------------------------------------------------------
//   harmonyCount
//---------------------------------------------------------

int Score::harmony_count() const {
    int count = 0;
    SegmentType st = SegmentType::CHORD_REST;
    for (Segment *seg = first_measure()->first(st); seg; seg = seg->next1(st)) {
        for (EngravingItem *e : seg->annotations()) {
            if (e->is_harmony()) {
                count++;
            }
        }
    }
    return count;
}

//---------------------------------------------------------
//   keysig
//---------------------------------------------------------

int Score::keysig() const {
    Key result = Key::C;
    for (size_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
        Staff *st = staff(staff_idx);
        constexpr Fraction t(0, 1);
        Key key = st->concert_key(t);
        if (st->staff_type(t)->group() == StaffGroup::PERCUSSION ||
            st->key_sig_event(t).is_atonal()) { // ignore percussion and custom / atonal key
            continue;
        }
        result = key;
        //TODO keySigs and pitched to unpitched instr changes (Igor Korsukov 2021-02-05) ??? why here?
        break;
    }
    return int(result);
}

//---------------------------------------------------------
//   duration
//---------------------------------------------------------

int Score::duration() const {
    const RepeatList &rl = master_score()->repeat_list(true);
    if (rl.empty()) {
        return 0;
    }
    const RepeatSegment *rs = rl.back();
    return lrint(utick2utime(rs->utick + rs->len()));
}

//---------------------------------------------------------
//   durationWithoutRepeats
//---------------------------------------------------------

int Score::duration_without_repeats() const {
    const RepeatList &rl = master_score()->repeat_list(false);
    if (rl.empty()) {
        return 0;
    }
    const RepeatSegment *rs = rl.back();
    return lrint(utick2utime(rs->utick + rs->len()));
}

std::set<staff_idx_t> Score::staff_idx_set_from_range(const track_idx_t track_from, const track_idx_t track_to,
                                                      StaffAccepted staff_accepted) const {
    std::set<staff_idx_t> result;

    for (const Part *part : score_->parts()) {
        const TrackRange range = part->track_range();
        if (track_to < range.start_track || track_from >= range.end_track) {
            continue;
        }

        std::set<staff_idx_t> staff_idx_list = part->stave_idx_list();
        if (!staff_accepted) {
            result.insert(staff_idx_list.cbegin(), staff_idx_list.cend());
            continue;
        }

        for (staff_idx_t idx : staff_idx_list) {
            const Staff *staff = score_->staff(idx);
            if (!staff) {
                continue;
            }

            if (staff_accepted(*staff)) {
                result.insert(idx);
            }
        }
    }

    return result;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Score::get_property(Pid /*id*/) const {
    LOGD("Score::getProperty: unhandled id");
    return PropertyValue();
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Score::set_property(Pid /*id*/, const PropertyValue & /*v*/) {
    LOGD("Score::setProperty: unhandled id");
    set_layout_all();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Score::property_default(Pid /*id*/) const { return PropertyValue(); }

void Score::reset_style_value(Sid style_to_reset) {
    const MStyle &def_style = DefaultStyle::default_style();
    undo_change_style_val(style_to_reset, def_style.value(style_to_reset));
}

void Score::reset_style_values(const StyleIdSet &style_id_set) {
    if (style_id_set.empty()) {
        return;
    }

    std::unordered_map<Sid, PropertyValue> values;
    const MStyle &def_style = DefaultStyle::default_style();

    for (Sid sid : style_id_set) {
        values.emplace(sid, def_style.value(sid));
    }

    undo_change_style_values(std::move(values));
}

//---------------------------------------------------------
//   setStyle
//---------------------------------------------------------

void Score::set_style(const MStyle &s, const bool overlap) {
    if (!overlap) {
        style() = s;
        return;
    }

    MStyle style_copy = s;

    for (int i = static_cast<int>(Sid::NOSTYLE) + 1; i < static_cast<int>(Sid::STYLES); i++) {
        Sid sid = static_cast<Sid>(i);

        if (!style_copy.is_default(sid)) {
            style().set(sid, style_copy.value(sid));
        }
    }
}

//---------------------------------------------------------
//   getTextStyleUserName
//---------------------------------------------------------

TranslatableString Score::get_text_style_user_name(TextStyleType tid) const {
    if (int(tid) >= int(TextStyleType::USER1) && int(tid) <= int(TextStyleType::USER12)) {
        int idx = int(tid) - int(TextStyleType::USER1);
        Sid sid[] = {Sid::USER1_NAME, Sid::USER2_NAME, Sid::USER3_NAME, Sid::USER4_NAME,  Sid::USER5_NAME,  Sid::USER6_NAME,
                     Sid::USER7_NAME, Sid::USER8_NAME, Sid::USER9_NAME, Sid::USER10_NAME, Sid::USER11_NAME, Sid::USER12_NAME};
        String user_name = style().style_st(sid[idx]);
        if (!user_name.empty()) {
            return TranslatableString::untranslatable(user_name);
        }
    }
    return TConv::user_name(tid);
}

String Score::name() const { return excerpt_ ? excerpt_->name() : String(); }

//---------------------------------------------------------
//   addRefresh
//---------------------------------------------------------

void Score::add_refresh(const RectF &r) {
    update_state_.refresh.unite(r);
    cmd_state().set_update_mode(UpdateMode::UPDATE);
}

//---------------------------------------------------------
//   staffIdx
//
//  Return index for the staff in the score.
//---------------------------------------------------------

staff_idx_t Score::staff_idx(const Staff *staff) const {
    staff_idx_t idx = 0;
    for (Staff *s : staves_) {
        if (s == staff) {
            break;
        }
        ++idx;
    }
    return idx;
}

//---------------------------------------------------------
//   staffIdx
//
///  Return index for the first staff of \a part.
//---------------------------------------------------------

staff_idx_t Score::staff_idx(const Part *part) const {
    staff_idx_t idx = 0;
    for (Part *p : parts_) {
        if (p == part) {
            break;
        }
        idx += p->nstaves();
    }
    return idx;
}

Staff *Score::staff_by_id(const ID &staff_id) const {
    for (Staff *staff : staves_) {
        if (staff->id() == staff_id) {
            return staff;
        }
    }

    return nullptr;
}

Part *Score::part_by_id(const ID &part_id) const {
    for (Part *part : parts_) {
        if (part->id() == part_id) {
            return part;
        }
    }

    return nullptr;
}

void Score::clear_system_object_staves() { system_object_staves_.clear(); }

void Score::add_system_object_staff(Staff *staff) { system_object_staves_.push_back(staff); }

void Score::remove_system_object_staff(Staff *staff) { muse::remove(system_object_staves_, staff); }

const std::vector<Staff *> Score::system_object_staves_with_top_staff() const {
    std::vector<Staff *> result;
    if (Staff *top_staff = staff(0)) {
        result.push_back(top_staff);
    }

    muse::join(result, system_object_staves());

    return result;
}

const std::vector<Part *> &Score::parts() const { return parts_; }

size_t Score::visible_part_count() const {
    int count = 0;
    for (const Part *part : parts_) {
        if (part->show()) {
            ++count;
        }
    }
    return count;
}

std::vector<Part *> Score::visible_parts() const {
    std::vector<Part *> result;
    result.reserve(parts_.size());

    for (Part *p : parts_) {
        if (p->show()) {
            result.push_back(p);
        }
    }

    return result;
}

std::vector<SharedPart *> Score::shared_parts() const {
    std::vector<SharedPart *> shared_parts;
    for (Part *part : parts_) {
        if (part->is_shared_part()) {
            shared_parts.push_back(to_shared_part(part));
        }
    }

    return shared_parts;
}

bool Score::has_shared_parts() const {
    for (Part *part : parts_) {
        if (part->is_shared_part()) {
            return true;
        }
    }

    return false;
}

size_t Score::visible_staves_count() const {
    int count = 0;
    for (const Staff *staff : staves_) {
        if (staff->show()) {
            ++count;
        }
    }
    return count;
}

bool Score::all_staves_invisible() const {
    for (const Staff *staff : staves_) {
        if (staff->show()) {
            return false;
        }
    }

    return true;
}

void Score::rebuild_bsp_tree() {
    for (Page *page : pages()) {
        page->invalidate_bsp_tree();
    }
}

//---------------------------------------------------------
//   scanElements
//    scan all elements
//---------------------------------------------------------

void Score::scan_elements(std::function<void(EngravingItem *)> func) {
    for (MeasureBase *mb = first(); mb; mb = mb->next()) {
        mb->scan_elements(func);
        if (mb->is_measure()) {
            Measure *m = to_measure(mb);
            Measure *mmr = m->mm_rest();
            if (mmr) {
                mmr->scan_elements(func);
            }
        }
    }
    for (Page *page : pages()) {
        for (System *s : page->systems()) {
            s->scan_elements(func);
        }
        func(page);
    }
}

//---------------------------------------------------------
//   connectTies
///   Rebuild tie connections.
//---------------------------------------------------------

void Score::connect_ties(bool silent) {
    size_t tracks = nstaves() * ::mu::engraving::voices;
    Measure *m = first_measure();
    if (!m) {
        return;
    }

    auto connect_ties_for_chord = [silent](Chord *c, Segment *s, track_idx_t track) -> void {
        for (Note *n : c->notes()) {
            if (n->laissez_vib()) {
                continue;
            }
            // connect a tie without end note
            Tie *tie = n->tie_for();
            if (tie) {
                tie->update_possible_jump_points();
            }
            if (tie && !tie->is_partial_tie() && !tie->end_note()) {
                Note *nnote;
                nnote = search_tie_note(n);
                if (nnote == 0) {
                    if (!silent) {
                        LOGD("next note at %d track %zu for tie not found", s->tick().ticks(), track);
                        delete tie;
                        n->set_tie_for(0);
                    }
                } else {
                    tie->set_end_note(nnote);
                    nnote->set_tie_back(tie);
                }
            }
            // connect a glissando without initial note (old glissando format)
            for (Spanner *spanner : n->spanner_back()) {
                if (spanner->is_glissando() && !spanner->start_element()) {
                    Note *initial_note = Glissando::guess_initial_note(n->chord());
                    n->remove_spanner_back(spanner);
                    if (initial_note) {
                        spanner->set_start_element(initial_note);
                        spanner->set_end_element(n);
                        spanner->set_tick(initial_note->chord()->tick());
                        spanner->set_tick2(n->chord()->tick());
                        spanner->set_track(n->track());
                        spanner->set_track2(n->track());
                        spanner->set_ownership_parent(initial_note);
                        initial_note->add(spanner);
                    } else {
                        delete spanner;
                    }
                }
            }
            // spanner with no end element can happen during copy/paste
            for (Spanner *spanner : n->spanner_for()) {
                if (spanner->end_element() == nullptr) {
                    n->remove_spanner_for(spanner);
                    delete spanner;
                }
            }
        }
    };

    SegmentType st = SegmentType::CHORD_REST;
    for (Segment *s = m->first(st); s; s = s->next1(st)) {
        for (track_idx_t track = 0; track < tracks; ++track) {
            EngravingItem *e = s->element(track);
            if (e == 0 || !e->is_chord()) {
                continue;
            }
            Chord *c = to_chord(e);
            connect_ties_for_chord(c, s, track);
            for (Chord *gc : c->grace_notes()) {
                connect_ties_for_chord(gc, s, track);

                for (Note *n : gc->notes()) {
                    // spanner with no end element apparently happens when reading some 206 files
                    // (and possibly in other situations too)
                    for (Spanner *spanner : n->spanner_for()) {
                        if (spanner->end_element() == nullptr) {
                            n->remove_spanner_for(spanner);
                            delete spanner;
                        }
                    }
                }
            }
        }
    }
}

bool Score::auto_layout_enabled() const { return is_open(); }

//---------------------------------------------------------
//   doLayout
//    do a complete (re-) layout
//---------------------------------------------------------

void Score::invalidate_layout(const Fraction &start, const Fraction &end) {
    // Layout locks CmdState while it builds generated elements. Those writes
    // must not invalidate scores that an earlier pass already laid out.
    if (master_score_ && master_score_->cmd_state().locked()) return;
    if (!layout_state.dirty) {
        layout_state.start = start;
        layout_state.end = end;
    } else {
        layout_state.start = std::min(layout_state.start, start);
        layout_state.end = (layout_state.end < Fraction(0, 1) || end < Fraction(0, 1)) ? Fraction(-1, 1) : std::max(layout_state.end, end);
    }
    layout_state.dirty = true;
}

void Score::invalidate_layouts(const Fraction &start, const Fraction &end) {
    if (master_score()->cmd_state().locked()) return;
    master_score()->invalidate_layout(start, end);
    // score_list() builds a linked list. Invalidation runs for every layout
    // trigger, so walk the existing excerpt storage without allocating.
    for (auto *excerpt : master_score()->excerpts()) {
        if (excerpt->excerpt_score()) excerpt->excerpt_score()->invalidate_layout(start, end);
    }
}

void Score::ensure_layout() {
    if (master_score()->layout_boundary)
        master_score()->layout_boundary(*this);
    else
        layout_if_dirty();
}

void Score::layout_if_dirty() {
    if (layout_state.dirty) do_layout_range(layout_state.start, layout_state.end);
}

void Score::do_layout() {
    // Explicit full layout also supports legacy callers that use direct DOM setters.
    invalidate_normalization(*this, Fraction(0, 1), Fraction(-1, 1));
    do_layout_range(Fraction(0, 1), Fraction(-1, 1));
}

void Score::do_layout_range(const Fraction &st, const Fraction &et) {
    normalize_score(*this);

    Fraction start = st;
    Fraction end = et;
    if (layout_state.dirty) {
        start = std::min(start, layout_state.start);
        end = (end < Fraction(0, 1) || layout_state.end < Fraction(0, 1)) ? Fraction(-1, 1) : std::max(end, layout_state.end);
    }

    auto spanners = score()->spanner_map().find_overlapping(start.ticks(), end.ticks());
    for (auto interval : spanners) {
        Spanner *spanner = interval.value;
        if (!spanner->staff()->visible()) {
            continue;
        }
        start = std::min(start, spanner->tick());
        end = std::max(end, spanner->tick2());
    }

    engraving_font_ = engraving_fonts()->font_by_name(style().value(Sid::MUSICAL_SYMBOL_FONT).value<String>().to_std_string());
    layout_options_.note_head_width = engraving_font_->width(SymId::NOTEHEAD_BLACK, style().spatium() / style().default_spatium());

    if (start <= Fraction(0, 1) && (end < Fraction(0, 1) || end == Fraction::max()))
        ++layout_state.whole_count;
    else
        ++layout_state.range_count;
    // If rendering throws, preserve the requested range for a later retry.
    layout_state.dirty = true;
    layout_state.start = start;
    layout_state.end = end;
    renderer()->layout_score(this, start, end);
    layout_state.last_start = start;
    layout_state.last_end = end;
    layout_state.dirty = false;
}

void Score::update_padding_tables() { padding_tables_.update(style()); }

void Score::add_system_lock(const RangeLock *lock) {
    system_locks_.add(lock);

    lock->start_mb()->trigger_layout();
    lock->end_mb()->trigger_layout();
}

void Score::remove_system_lock(const RangeLock *lock) {
    system_locks_.remove(lock);

    lock->start_mb()->trigger_layout();
    lock->end_mb()->trigger_layout();
}

void Score::add_page_lock(const RangeLock *lock) {
    page_locks_.add(lock);

    lock->start_mb()->trigger_layout();
    lock->end_mb()->trigger_layout();
}

void Score::remove_page_lock(const RangeLock *lock) {
    page_locks_.remove(lock);

    lock->start_mb()->trigger_layout();
    lock->end_mb()->trigger_layout();
}

//---------------------------------------------------------
//   updateSwing
//---------------------------------------------------------

void Score::update_swing() {
    for (Staff *s : staves_) {
        s->clear_swing_map();
    }
    Measure *fm = first_measure();
    if (!fm) {
        return;
    }
    for (Segment *s = fm->first(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
        for (const EngravingItem *e : s->annotations()) {
            if (!e->is_staff_text_base()) {
                continue;
            }
            const StaffTextBase *st = to_staff_text_base(e);
            if (st->xml_text().is_empty()) {
                continue;
            }
            Staff *staff = st->staff();
            if (!st->swing()) {
                continue;
            }
            SwingParameters sp;
            sp.swing_ratio = st->swing_parameters().swing_ratio;
            sp.swing_unit = st->swing_parameters().swing_unit;
            if (st->system_flag()) {
                for (Staff *sta : staves_) {
                    sta->insert_into_swing_map(s->tick(), sp);
                }
            } else {
                staff->insert_into_swing_map(s->tick(), sp);
            }
        }
    }
}

//---------------------------------------------------------
//   updateCapo
//---------------------------------------------------------

void Score::update_capo(bool ignore_notation_update /* = false */) {
    Measure *fm = first_measure();
    if (!fm) {
        return;
    }

    for (Segment *s = fm->first(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
        Fraction segment_tick = s->tick();

        for (EngravingItem *e : s->annotations()) {
            if (e->is_harmony()) {
                to_harmony(e)->realized_harmony().set_dirty(true);
            } else if (e->is_fret_diagram()) {
                Harmony *harmony = to_fret_diagram(e)->harmony();
                if (harmony) {
                    harmony->realized_harmony().set_dirty(true);
                }
            }

            if (!e->is_capo()) {
                continue;
            }

            for (Staff *staff : e->staff()->staff_list()) {
                staff->insert_capo_params(segment_tick, to_capo(e)->params(), ignore_notation_update);
            }
        }
    }
}

//---------------------------------------------------------
//   updateChannel
//---------------------------------------------------------

void Score::update_channel() {
    for (Staff *s : staves()) {
        for (voice_idx_t i = 0; i < ::mu::engraving::voices; ++i) {
            s->clear_channel_list(i);
        }
    }
    Measure *fm = first_measure();
    if (!fm) {
        return;
    }
    for (Segment *s = fm->first(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
        for (const EngravingItem *e : s->annotations()) {
            if (e->is_instrument_change()) {
                for (Staff *staff : e->part()->staves()) {
                    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
                        staff->insert_into_channel_list(voice, s->tick(), 0);
                    }
                }
                continue;
            }
            if (!e->is_staff_text_base()) {
                continue;
            }
            const StaffTextBase *st = to_staff_text_base(e);
            for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
                String an(st->channel_name(voice));
                if (an.is_empty()) {
                    continue;
                }
                Staff *staff = Score::staff(st->staff_idx());
                int a = staff->part()->instrument(s->tick())->channel_idx(an);
                if (a != -1) {
                    staff->insert_into_channel_list(voice, s->tick(), a);
                }
            }
        }
    }

    for (auto it = spanner().cbegin(); it != spanner().cend(); ++it) {
        Spanner *spanner = (*it).second;
        if (spanner->is_palm_mute()) {
            to_palm_mute(spanner)->set_channel();
        }
        if (spanner->is_volta()) {
            to_volta(spanner)->set_channel();
        }
    }

    for (Segment *s = fm->first(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
        for (Staff *st : staves()) {
            track_idx_t strack = st->idx() * ::mu::engraving::voices;
            track_idx_t etrack = strack + ::mu::engraving::voices;
            for (track_idx_t track = strack; track < etrack; ++track) {
                if (!s->element(track)) {
                    continue;
                }
                EngravingItem *e = s->element(track);
                if (!e->is_chord()) {
                    continue;
                }
                Chord *c = to_chord(e);
                size_t channel = st->channel(c->tick(), c->voice());
                Instrument *instr = c->part()->instrument(c->tick());
                if (channel >= instr->channel().size()) {
                    LOGD() << "Channel " << channel << " too high. Max " << instr->channel().size();
                    channel = 0;
                }
                for (Note *note : c->notes()) {
                    if (note->hidden()) {
                        continue;
                    }
                    note->set_subchannel(static_cast<int>(channel));
                }
            }
        }
    }
}

SystemDivider *Score::system_divider(size_t system_idx, SystemDividerType type) const {
    if (muse::contains(system_dividers_, system_idx)) {
        return system_dividers_.at(system_idx).at(static_cast<size_t>(type));
    }

    return nullptr;
}

void Score::add_system_divider(size_t system_idx, SystemDivider *divider) {
    if (!muse::contains(system_dividers_, system_idx)) {
        system_dividers_.emplace(system_idx, std::array<SystemDivider *, 2>{nullptr, nullptr});
    }

    system_dividers_.at(system_idx)[static_cast<size_t>(divider->divider_type())] = divider;
}

BracketType Score::bracket_type(staff_idx_t staff_idx, size_t idx) const {
    const std::vector<BracketItem *> &staff_brackets = brackets(staff_idx);
    if (idx < staff_brackets.size()) {
        return staff_brackets[idx]->bracket_type();
    }
    return BracketType::NO_BRACKET;
}

size_t Score::bracket_span(staff_idx_t staff_idx, size_t idx) const {
    const std::vector<BracketItem *> &staff_brackets = brackets(staff_idx);
    if (idx < staff_brackets.size()) {
        return staff_brackets[idx]->bracket_span();
    }
    return 0;
}

const std::vector<BracketItem *> &Score::brackets(staff_idx_t staff_idx) const { return brackets_.at(staff_idx); }

std::vector<BracketItem *> &Score::brackets(staff_idx_t staff_idx) {
    const size_t new_size = std::max(static_cast<size_t>(staff_idx) + 1, staves_.size());
    if (brackets_.size() < new_size) {
        brackets_.resize(new_size);
    }
    return brackets_[staff_idx];
}

//---------------------------------------------------------
//   bracketLevels
//---------------------------------------------------------

size_t Score::bracket_levels(staff_idx_t staff_idx) const {
    const std::vector<BracketItem *> &staff_brackets = brackets(staff_idx);
    size_t columns = 0;
    for (auto bi : staff_brackets) {
        columns = std::max(columns, bi->column());
    }
    return columns;
}

AutomationDataConstPtr Score::automation_data() const { return master_score_->automation_data(); }

void Score::edit_automation_points(const AutomationCurveKey &key, AutomationPointEdits &edits, bool undoable) {
    master_score_->edit_automation_points(key, edits, undoable);
}

TransactionManager *Score::transaction_manager() const { return master_score_->transaction_manager(); }

UndoStack *Score::undo_stack() const { return master_score_->undo_stack(); }

const RepeatList &Score::repeat_list() const { return master_score_->repeat_list(); }

const RepeatList &Score::repeat_list(bool expand_repeats) const { return master_score_->repeat_list(expand_repeats); }

std::vector<RepeatSegmentInfo> Score::repeat_segment_info_list(bool expand_repeats) const {
    // Read-only: don't trigger tie/jump-point cleanup, safe to call mid-edit
    return repeat_list(expand_repeats).segment_info_list();
}

const RepeatList &Score::expanded_repeat_list() const { return repeat_list(/*expandRepeats*/ true); }

TimeSigMap *Score::sigmap() const { return master_score_->sigmap(); }

void Score::set_update_all() { master_score_->set_update_all(); }

void Score::set_layout_all(staff_idx_t staff, const EngravingItem *e) { master_score_->set_layout_all(staff, e); }

void Score::set_layout(const Fraction &tick, staff_idx_t staff, const EngravingItem *e) { master_score_->set_layout(tick, staff, e); }

void Score::set_layout(const Fraction &tick1, const Fraction &tick2, staff_idx_t staff1, staff_idx_t staff2, const EngravingItem *e) {
    master_score_->set_layout(tick1, tick2, staff1, staff2, e);
}

CmdState &Score::cmd_state() { return master_score_->cmd_state(); }

const CmdState &Score::cmd_state() const { return master_score_->cmd_state(); }

void Score::add_layout_flags(LayoutFlags f) { master_score_->add_layout_flags(f); }

void Score::set_instruments_changed(bool v) { master_score_->set_instruments_changed(v); }

Fraction Score::loop_boundary_tick(LoopBoundaryType type) const { return master_score_->loop_boundary_tick(type); }

void Score::set_loop_boundary_tick(LoopBoundaryType type, Fraction tick) { master_score_->set_loop_boundary_tick(type, tick); }

//---------------------------------------------------------
//   ScoreLoad::_loading
//    If the _loading > 0 then pushes and pops to
//    the undo stack do not emit a warning.
//    Usually pushes and pops to the undo stack are only
//    valid inside a startCmd() - endCmd(). Exceptions
//    occurred during score loading.
//---------------------------------------------------------

int ScoreLoad::loading_ = 0;
} // namespace mu::engraving
