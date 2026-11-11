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
 Implementation of EngravingItem, ElementList
*/

#include "engravingitem.h"

#include <cmath>
#include <cstddef>
#include <iterator>

#include "../editing/addremoveelement.h"
#include "../editing/editdata.h"
#include "../editing/editproperty.h"
#include "../editing/elementeditdata.h"
#include "../editing/navigation.h"
#include "chord.h"
#include "containers.h"
#include "dummyparent.h"
#include "factory.h"
#include "iengravingconfiguration.h"
#include "iengravingcontextconfiguration.h"
#include "iengravingfont.h"
#include "io/buffer.h"
#include "linkedobjects.h"
#include "log.h"
#include "masterscore.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "page.h"
#include "parenthesis.h"
#include "part.h"
#include "rendering/iscorerenderer.h"
#include "rendering/score/autoplace.h"
#include "rendering/score/chordlayout.h"
#include "rendering/score/tlayout.h"
#include "rootitem.h"
#include "rw/rwregister.h"
#include "score.h"
#include "segment.h"
#include "shape.h"
#include "sig.h"
#include "staff.h"
#include "stafflines.h"
#include "stafftype.h"
#include "system.h"
#include "translation.h"
#include "types/typesconv.h"
#define LOG_PROP() \
    if (0) LOGD()

using namespace mu;
using namespace muse::io;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

namespace mu::engraving {
namespace {
// ElementType -> its ordinal at ea2b00ee48, in current enumerator order. A deleted
// enumerator loses its row; a new one needs a fresh value that no saved default uses.
struct LegacyZ {
    ElementType type;
    int ordinal;
};

constexpr LegacyZ legacy_z[] = {
    {ElementType::INVALID, 0},
    {ElementType::BRACKET_ITEM, 1},
    {ElementType::PART, 2},
    {ElementType::SHARED_PART, 3},
    {ElementType::STAFF, 4},
    {ElementType::SCORE, 5},
    {ElementType::TEXT, 6},
    {ElementType::LAYOUT_BREAK, 7},
    {ElementType::MEASURE_NUMBER, 8},
    {ElementType::MMREST_RANGE, 9},
    {ElementType::INSTRUMENT_NAME, 10},
    {ElementType::BAR_LINE, 11},
    {ElementType::STAFF_LINES, 12},
    {ElementType::SYSTEM_DIVIDER, 13},
    {ElementType::SLUR, 14},
    {ElementType::SLUR_SEGMENT, 15},
    {ElementType::TIE, 16},
    {ElementType::TIE_SEGMENT, 17},
    {ElementType::LAISSEZ_VIB, 18},
    {ElementType::LAISSEZ_VIB_SEGMENT, 19},
    {ElementType::PARTIAL_TIE, 20},
    {ElementType::PARTIAL_TIE_SEGMENT, 21},
    {ElementType::STEM_SLASH, 22},
    {ElementType::ARPEGGIO, 23},
    {ElementType::CHORD_BRACKET, 24},
    {ElementType::ACCIDENTAL, 25},
    {ElementType::LEDGER_LINE, 26},
    {ElementType::STEM, 27},
    {ElementType::HOOK, 28},
    {ElementType::NOTE, 29},
    {ElementType::CLEF, 30},
    {ElementType::KEYSIG, 31},
    {ElementType::AMBITUS, 32},
    {ElementType::TIMESIG, 33},
    {ElementType::REST, 34},
    {ElementType::MMREST, 35},
    {ElementType::DEAD_SLAPPED, 36},
    {ElementType::SYMBOL, 37},
    {ElementType::BREATH, 38},
    {ElementType::MEASURE_REPEAT, 39},
    {ElementType::ARTICULATION, 40},
    {ElementType::ORNAMENT, 41},
    {ElementType::FERMATA, 42},
    {ElementType::CHORDLINE, 43},
    {ElementType::DYNAMIC, 44},
    {ElementType::EXPRESSION, 45},
    {ElementType::BEAM, 46},
    {ElementType::LYRICS, 47},
    {ElementType::FIGURED_BASS, 48},
    {ElementType::FIGURED_BASS_ITEM, 49},
    {ElementType::MARKER, 50},
    {ElementType::JUMP, 51},
    {ElementType::FINGERING, 52},
    {ElementType::TUPLET, 53},
    {ElementType::TEMPO_TEXT, 54},
    {ElementType::STAFF_TEXT, 55},
    {ElementType::SYSTEM_TEXT, 56},
    {ElementType::STAVE_SHARING_LABEL, 57},
    {ElementType::SOUND_FLAG, 58},
    {ElementType::PLAY_COUNT_TEXT, 59},
    {ElementType::PLAYTECH_ANNOTATION, 60},
    {ElementType::CAPO, 61},
    {ElementType::STRING_TUNINGS, 62},
    {ElementType::TRIPLET_FEEL, 63},
    {ElementType::REHEARSAL_MARK, 64},
    {ElementType::INSTRUMENT_CHANGE, 65},
    {ElementType::STAFFTYPE_CHANGE, 66},
    {ElementType::FRET_DIAGRAM, 67},
    {ElementType::HARMONY, 68},
    {ElementType::HARP_DIAGRAM, 69},
    {ElementType::BEND, 70},
    {ElementType::TREMOLOBAR, 71},
    {ElementType::VOLTA, 72},
    {ElementType::VOLTA_SEGMENT, 73},
    {ElementType::HAIRPIN, 74},
    {ElementType::HAIRPIN_SEGMENT, 75},
    {ElementType::OTTAVA, 76},
    {ElementType::OTTAVA_SEGMENT, 77},
    {ElementType::TRILL, 78},
    {ElementType::TRILL_SEGMENT, 79},
    {ElementType::LET_RING, 80},
    {ElementType::LET_RING_SEGMENT, 81},
    {ElementType::GRADUAL_TEMPO_CHANGE, 82},
    {ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT, 83},
    {ElementType::VIBRATO, 84},
    {ElementType::VIBRATO_SEGMENT, 85},
    {ElementType::PALM_MUTE, 86},
    {ElementType::PALM_MUTE_SEGMENT, 87},
    {ElementType::WHAMMY_BAR, 88},
    {ElementType::WHAMMY_BAR_SEGMENT, 89},
    {ElementType::RASGUEADO, 90},
    {ElementType::RASGUEADO_SEGMENT, 91},
    {ElementType::HARMONIC_MARK, 92},
    {ElementType::HARMONIC_MARK_SEGMENT, 93},
    {ElementType::PICK_SCRAPE, 94},
    {ElementType::PICK_SCRAPE_SEGMENT, 95},
    {ElementType::TEXTLINE, 96},
    {ElementType::TEXTLINE_SEGMENT, 97},
    {ElementType::PEDAL, 98},
    {ElementType::PEDAL_SEGMENT, 99},
    {ElementType::LYRICSLINE, 100},
    {ElementType::LYRICSLINE_SEGMENT, 101},
    {ElementType::PARTIAL_LYRICSLINE, 102},
    {ElementType::PARTIAL_LYRICSLINE_SEGMENT, 103},
    {ElementType::GLISSANDO, 104},
    {ElementType::GLISSANDO_SEGMENT, 105},
    {ElementType::NOTELINE, 106},
    {ElementType::NOTELINE_SEGMENT, 107},
    {ElementType::STAFF_VISIBILITY_INDICATOR, 108},
    {ElementType::SYSTEM_LOCK_INDICATOR, 109},
    {ElementType::SPACER, 110},
    {ElementType::STAFF_STATE, 111},
    {ElementType::NOTEHEAD, 112},
    {ElementType::NOTEDOT, 113},
    {ElementType::IMAGE, 114},
    {ElementType::MEASURE, 115},
    {ElementType::SELECTION, 116},
    {ElementType::TAB_DURATION_SYMBOL, 119},
    {ElementType::FSYMBOL, 120},
    {ElementType::PAGE, 121},
    {ElementType::PAGE_LOCK_INDICATOR, 122},
    {ElementType::TEXTLINE_BASE, 123},
    {ElementType::BRACKET, 124},
    {ElementType::SEGMENT, 125},
    {ElementType::SYSTEM, 126},
    {ElementType::CHORD, 127},
    {ElementType::HBOX, 128},
    {ElementType::VBOX, 129},
    {ElementType::TBOX, 130},
    {ElementType::FBOX, 131},
    {ElementType::BAGPIPE_EMBELLISHMENT, 133},
    {ElementType::STICKING, 134},
    {ElementType::GRACE_NOTES_GROUP, 135},
    {ElementType::GUITAR_BEND, 136},
    {ElementType::GUITAR_BEND_SEGMENT, 137},
    {ElementType::GUITAR_BEND_HOLD, 138},
    {ElementType::GUITAR_BEND_HOLD_SEGMENT, 139},
    {ElementType::GUITAR_BEND_TEXT, 140},
    {ElementType::TREMOLO_TWOCHORD, 141},
    {ElementType::TREMOLO_SINGLECHORD, 142},
    {ElementType::TIME_TICK_ANCHOR, 143},
    {ElementType::PARENTHESIS, 144},
    {ElementType::HAMMER_ON_PULL_OFF, 145},
    {ElementType::HAMMER_ON_PULL_OFF_SEGMENT, 146},
    {ElementType::HAMMER_ON_PULL_OFF_TEXT, 147},
    {ElementType::TAPPING, 148},
    {ElementType::TAPPING_HALF_SLUR, 149},
    {ElementType::TAPPING_HALF_SLUR_SEGMENT, 150},
    {ElementType::TAPPING_TEXT, 151},
    {ElementType::ROOT_ITEM, 152},
    {ElementType::DUMMY, 153},
};

constexpr bool legacy_z_matches_enum() {
    for (std::size_t i = 0; i < std::size(legacy_z); ++i) {
        if (legacy_z[i].type != static_cast<ElementType>(i)) return false;
    }
    return true;
}

static_assert(std::size(legacy_z) == static_cast<std::size_t>(ElementType::MAXTYPE), "every ElementType needs a pinned z order");
static_assert(legacy_z_matches_enum(), "legacy_z rows must follow ElementType order");
} // namespace

int default_z(ElementType type) { return legacy_z[static_cast<std::size_t>(type)].ordinal * 100; }

EngravingItem::EngravingItem(const ElementType &type, EngravingObject *parent, ElementFlags f) : EngravingObject(type, parent) {
    flags_ = f;
    color_ = configuration()->default_color();
    z_ = -1;
    min_distance_ = 0.0_sp;
}

EngravingItem::EngravingItem(const EngravingItem &e, bool link) : EngravingObject(e) {
    offset_ = e.offset_;
    track_ = e.track_;
    flags_ = e.flags_;
    set_flag(ElementFlag::SELECTED, false);
    z_ = e.z_;
    color_ = e.color_;
    min_distance_ = e.min_distance_;
    exclude_vertical_align_ = e.exclude_vertical_align_;
    item_discovered = false;

    accessible_enabled_ = e.accessible_enabled_;

    if (e.left_parenthesis_) {
        left_parenthesis_ = e.left_parenthesis_->clone();
        left_parenthesis_->set_ownership_parent(this);
        left_parenthesis_->set_track(track());
        if (link) {
            score()->undo(new Link(left_parenthesis_, e.left_parenthesis_));
        }
    }
    if (e.right_parenthesis_) {
        right_parenthesis_ = e.right_parenthesis_->clone();
        right_parenthesis_->set_ownership_parent(this);
        right_parenthesis_->set_track(track());
        if (link) {
            score()->undo(new Link(right_parenthesis_, e.right_parenthesis_));
        }
    }
}

EngravingItem::~EngravingItem() {
    Score::on_element_destruction(this);

    delete layout_data_;
}

bool EngravingItem::accessible_enabled() const { return accessible_enabled_; }

void EngravingItem::set_accessible_enabled(bool enabled) { accessible_enabled_ = enabled; }

EngravingItem *EngravingItem::accessible_parent_item() const {
    if (EngravingItem *p = layout_parent()) {
        return p;
    }

    if (ownership_parent()) {
        // attached to something, but not placed anywhere: in neither tree
        return nullptr;
    }

    // not attached to anything: head of the tree those objects form
    return score() && score()->dummy() ? score()->dummy()->root_item() : nullptr;
}

EngravingItemList EngravingItem::accessible_children() const { return children_items(); }

EngravingItem *EngravingItem::ownership_parent_item() const {
    EngravingObject *p = ownership_parent();
    if (p && p->is_engraving_item()) {
        return to_engraving_item(p);
    }

    return nullptr;
}

EngravingItem *EngravingItem::layout_parent() const { return ownership_parent_item(); }

const std::shared_ptr<IEngravingConfiguration> &EngravingItem::configuration() const { return score()->configuration(); }

const std::shared_ptr<IEngravingContextConfiguration> &EngravingItem::context_configuration() const {
    return score()->context_configuration();
}

const std::shared_ptr<rendering::IScoreRenderer> &EngravingItem::renderer() const { return score()->renderer(); }

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void EngravingItem::spatium_changed(double old_value, double new_value) {
    if (offset_is_spatium_dependent()) {
        offset_ *= (new_value / old_value);
    }
}

//---------------------------------------------------------
//   localSpatiumChanged
//    the scale of a staff changed
//---------------------------------------------------------

void EngravingItem::local_spatium_changed(double old_value, double new_value) {
    if (offset_is_spatium_dependent()) {
        offset_ *= (new_value / old_value);
    }
}

//---------------------------------------------------------
//   spatium
//---------------------------------------------------------

double EngravingItem::spatium() const {
    EngravingItem *p = layout_parent();
    if (system_flag() || (p && p->system_flag())) {
        return style().spatium();
    }
    Staff *s = staff();
    return s ? s->spatium(this) : style().spatium();
}

double EngravingItem::default_spatium() const { return style().default_spatium(); }

bool EngravingItem::is_interaction_available() const {
    if (!get_property(Pid::VISIBLE).to_bool() && (score()->printing() || !score()->is_show_invisible())) {
        return false;
    }

    return true;
}

//---------------------------------------------------------
//   offsetIsSpatiumDependent
//---------------------------------------------------------

bool EngravingItem::offset_is_spatium_dependent() const { return size_is_spatium_dependent() || (flags_ & ElementFlag::ON_STAFF); }

Sid EngravingItem::default_pos_sid() const { return Sid::NOSTYLE; }

PointF EngravingItem::default_pos() const {
    Sid style_id = default_pos_sid();

    if (style_id == Sid::NOSTYLE) {
        return PointF();
    }

    PointF offset_pos = style().value(style_id).value<PointF>();
    if (offset_is_spatium_dependent()) {
        offset_pos *= spatium();
    } else {
        offset_pos *= ::mu::engraving::dpmm;
    }

    return offset_pos;
}

PlacementV EngravingItem::placement() const {
    return flag(ElementFlag::PLACE_ABOVE) && !is_system_object_below_bottom_staff() ? PlacementV::ABOVE : PlacementV::BELOW;
}

//---------------------------------------------------------
//   magS
//---------------------------------------------------------

double EngravingItem::mag_s() const { return mag() * (style().spatium() / default_spatium()); }

//---------------------------------------------------------
//   name
//---------------------------------------------------------

TranslatableString EngravingItem::subtype_user_name() const { return {}; }

String EngravingItem::translated_subtype_user_name() const { return subtype_user_name().translated(); }

//---------------------------------------------------------
//   linkedClone
//---------------------------------------------------------

EngravingItem *EngravingItem::linked_clone() {
    EngravingItem *e = clone();
    score()->undo(new Link(e, this));
    return e;
}

//---------------------------------------------------------
//   deleteLater
//---------------------------------------------------------

void EngravingItem::delete_later() {
    if (selected()) {
        score()->deselect(this);
    }
    master_score()->cmd_state().delete_later(this);
}

//---------------------------------------------------------
//   scanElements
/// If leaf node, apply `func` on this element (after checking if it is visible).
/// Otherwise, recurse over all children (see ScoreElement::scanElements).
/// Note: This function is overridden in some classes to skip certain children,
/// or to apply `func` even to non-leaf nodes.
//---------------------------------------------------------

void EngravingItem::scan_elements(std::function<void(EngravingItem *)> func) {
    if (left_parenthesis_) {
        left_parenthesis_->scan_elements(func);
    }

    if (right_parenthesis_) {
        right_parenthesis_->scan_elements(func);
    }

    func(this);
}

bool EngravingItem::collect_for_drawing() const {
    if (!visible() && !score()->is_show_invisible()) {
        return false;
    }

    return true;
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void EngravingItem::reset() {
    undo_reset_property(Pid::AUTOPLACE);
    undo_reset_property(Pid::PLACEMENT);
    undo_reset_property(Pid::MIN_DISTANCE);
    undo_reset_property(Pid::OFFSET);
    undo_reset_property(Pid::LEADING_SPACE);
    EngravingObject::reset();
}

//---------------------------------------------------------
//   change
//---------------------------------------------------------

void EngravingItem::change(EngravingItem *o, EngravingItem *n) {
    remove(o);
    add(n);
}

//---------------------------------------------------------
//   staff
//---------------------------------------------------------

Staff *EngravingItem::staff() const {
    if (!has_staff() || score()->staves().empty()) {
        return nullptr;
    }

    return score()->staff(staff_idx());
}

bool EngravingItem::has_staff() const { return track_ != muse::nidx; }

//---------------------------------------------------------
//   staffType
//---------------------------------------------------------

const StaffType *EngravingItem::staff_type() const {
    Staff *s = staff();
    return s ? s->staff_type_for_element(this) : nullptr;
}

//---------------------------------------------------------
//   onTabStaff
//---------------------------------------------------------

bool EngravingItem::on_tab_staff() const {
    const StaffType *stt = staff_type();
    return stt ? stt->is_tab_staff() : false;
}

track_idx_t EngravingItem::track() const { return track_; }

void EngravingItem::set_track(track_idx_t val) {
    IF_ASSERT_FAILED(val < score_->ntracks() || val == 0 || val == muse::nidx) {
        // Zero and muse::nidx have special use cases.
        // In all other cases the track can't be larger than the total track count.
        return;
    }

    track_ = val;

    if (left_parenthesis_) {
        left_parenthesis_->set_track(val);
    }
    if (right_parenthesis_) {
        right_parenthesis_->set_track(val);
    }
}

//---------------------------------------------------------
//   z
//---------------------------------------------------------

int EngravingItem::z() const {
    if (z_ == -1) {
        z_ = default_z(type());
    }
    return z_;
}

void EngravingItem::set_z(int val) { z_ = val; }

staff_idx_t EngravingItem::staff_idx() const { return track2staff(track_); }

void EngravingItem::set_staff_idx(staff_idx_t val) {
    voice_idx_t voice_idx = voice();
    set_track(staff2track(val, voice_idx == muse::nidx ? 0 : voice_idx));
}

staff_idx_t EngravingItem::effective_staff_idx() const {
    if (!system_flag()) {
        return v_staff_idx();
    }

    const System *system = to_system(find_ancestor(ElementType::SYSTEM));
    if (!system || system->vbox()) {
        return v_staff_idx();
    }

    if (is_system_object_below_bottom_staff()) {
        return system->last_visible_staff();
    }

    staff_idx_t original_staff_idx = staff_idx();
    if (original_staff_idx == muse::nidx) {
        return muse::nidx;
    }

    const Measure *m = find_measure();

    // TODO - make more system markings ignore empty cutaway measures
    auto cutaway_measure_no = [&](staff_idx_t staff_idx) {
        if (!m) {
            return false;
        }
        bool measure_no = is_measure_number();
        bool staff_visible = m->mstaves()[staff_idx]->visible();
        bool cutaway = score()->staff(staff_idx)->cutaway() && m->is_empty(staff_idx);
        return (cutaway || !staff_visible) && measure_no;
    };

    const std::vector<Staff *> &system_object_staves = score_->system_object_staves(); // CAUTION: may not be ordered
    if (original_staff_idx > 0) {
        staff_idx_t prev_sys_obj_staff_idx = 0;
        for (Staff *sys_obj_staff : system_object_staves) {
            staff_idx_t sys_obj_staff_idx = sys_obj_staff->idx();
            if (sys_obj_staff_idx > prev_sys_obj_staff_idx && sys_obj_staff_idx < original_staff_idx) {
                prev_sys_obj_staff_idx = sys_obj_staff_idx;
            }
        }

        bool omit_object = true;
        for (staff_idx_t stf_idx = prev_sys_obj_staff_idx; stf_idx < original_staff_idx; ++stf_idx) {
            if (score_->staff(stf_idx)->show() && system->staff(stf_idx)->show() && !cutaway_measure_no(stf_idx)) {
                omit_object = false;
                break;
            }
        }

        if (omit_object) {
            // If all staves between this and the previous system object are hidden
            // we omit this object because it will be replaced by the upper one
            return muse::nidx;
        }
    }

    if (score_->staff(original_staff_idx)->show() && system->staff(original_staff_idx)->show() && !cutaway_measure_no(original_staff_idx)) {
        return original_staff_idx;
    }

    return system->next_visible_staff(original_staff_idx);
}

bool EngravingItem::is_top_system_object() const { return system_flag() && track() == 0; }

staff_idx_t EngravingItem::v_staff_idx() const { return staff_idx(); }

voice_idx_t EngravingItem::voice() const { return track2voice(track_); }

void EngravingItem::set_voice(voice_idx_t v) { set_track((track_ / ::mu::engraving::voices) * ::mu::engraving::voices + v); }

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

Fraction EngravingItem::tick() const {
    // walks EngravingObject, because the chain leaves EngravingItem at the score
    for (const EngravingObject *e = ownership_parent(); e; e = e->ownership_parent()) {
        if (e->is_segment()) {
            return to_segment(e)->tick();
        } else if (e->is_measure_base()) {
            return to_measure_base(e)->tick();
        }
    }
    return Fraction(0, 1);
}

//---------------------------------------------------------
//   rtick
//---------------------------------------------------------

Fraction EngravingItem::rtick() const {
    for (const EngravingObject *e = ownership_parent(); e; e = e->ownership_parent()) {
        if (e->is_segment()) {
            return to_segment(e)->rtick();
        }
    }
    return Fraction(0, 1);
}

//---------------------------------------------------------
//   beat
//---------------------------------------------------------

Fraction EngravingItem::beat() const {
    // Returns an appropriate fraction of ticks for use as a "Beat" reference
    // in the Select All Similar filter.
    int bar, beat, ticks;
    TimeSigMap *tsm = score()->sigmap();
    tsm->tick_values(tick().ticks(), &bar, &beat, &ticks);
    int ticks_b = ticks_beat(tsm->timesig(tick().ticks()).timesig().denominator());

    Fraction complex_fraction((++beat * ticks_b) + ticks, ticks_b);
    return complex_fraction.reduced();
}

//---------------------------------------------------------
//   part
//---------------------------------------------------------

Part *EngravingItem::part() const {
    Staff *s = staff();
    return s ? s->part() : 0;
}

Part *EngravingItem::origin_part() const {
    Part *p = part();
    return p ? p->origin_part(track(), tick()) : nullptr;
}

void EngravingItem::set_color(const Color &c) { color_ = c; }

Color EngravingItem::color() const { return color_; }

//---------------------------------------------------------
//   curColor
//---------------------------------------------------------

Color EngravingItem::cur_color(const rendering::PaintOptions &opt) const { return cur_color(get_property(Pid::VISIBLE).to_bool(), opt); }

//---------------------------------------------------------
//   curColor
//---------------------------------------------------------

Color EngravingItem::cur_color(bool is_visible, const rendering::PaintOptions &opt) const { return cur_color(is_visible, color(), opt); }

Color EngravingItem::cur_color(bool is_visible, Color normal_color, const rendering::PaintOptions &opt) const {
    if (opt.is_printing) {
        // For printing, always use the actual color of the item (default black unless overridden by the user)
        return normal_color;
    }

    if (flag(ElementFlag::DROP_TARGET)) {
        return configuration()->highlight_selection_color(track() == muse::nidx ? 0 : voice());
    }

    bool marked = false;
    if (is_note()) {
        marked = to_note(this)->mark();
    }

    auto maybe_override_color = [&](Color c) -> Color {
        if (opt.override_item_color) {
            return opt.override_item_color(this, c);
        }
        return c;
    };

    if (selected() || marked) {
        voice_idx_t voice_for_color_choice = track() == muse::nidx ? 0 : voice();
        if (has_voice_assignment_properties()) {
            VoiceAssignment voice_assignment = get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>();
            if (voice_assignment != VoiceAssignment::CURRENT_VOICE_ONLY) {
                voice_for_color_choice = ::mu::engraving::voices;
            }
        }
        return maybe_override_color(configuration()->selection_color(voice_for_color_choice, is_visible, is_unlinked_from_master()));
    }

    if (!is_visible) {
        return configuration()->invisible_color();
    }

    if (!opt.ignore_displayed_default_color && normal_color == configuration()->default_color()) {
        return maybe_override_color(configuration()->displayed_default_color(opt.invert_colors));
    }

    if (opt.invert_colors) {
        return maybe_override_color(normal_color.inverted());
    }
    return maybe_override_color(normal_color);
}

PointF EngravingItem::system_pos() const {
    // Returns position in system coordinates. Only applicable to items
    // that have a System ancestor.

    IF_ASSERT_FAILED(find_ancestor(ElementType::SYSTEM)) { return PointF(); }

    PointF result = pos();
    EngravingItem *ancestor = layout_parent();
    while (ancestor && !ancestor->is_system()) {
        result += ancestor->pos();
        ancestor = ancestor->layout_parent();
    }

    return result;
}

//---------------------------------------------------------
//   pagePos
//    return position in canvas coordinates
//---------------------------------------------------------

PointF EngravingItem::page_pos() const {
    PointF p(pos());
    EngravingItem *parent = layout_parent();
    if (parent == nullptr) {
        return p;
    }

    staff_idx_t idx = effective_staff_idx();

    if (idx == muse::nidx) {
        idx = v_staff_idx();
    }

    auto measure_staff_y = [this](const Measure *measure, staff_idx_t idx) -> double {
        if (!measure) {
            return 0.0;
        }
        if (measure->mstaves().size() <= idx) {
            LOGD("staffIdx out of bounds: %s", type_name());
            return 0.0;
        }
        return measure->staff_lines(idx)->y();
    };

    auto system_staff_y = [this](const System *system, staff_idx_t idx) -> double {
        if (!system) {
            return 0.0;
        }
        if (system->staves().size() <= idx) {
            LOGD("staffIdx out of bounds: %s", type_name());
            return 0.0;
        }
        return system->staff_ypage(idx);
    };

    if (flags_ & ElementFlag::ON_STAFF) {
        // Segments, measures and systems all contain multiple staves
        // Find the position of the stave that this item is on
        if (parent->is_segment()) {
            Segment *segment = to_segment(parent);
            Measure *measure = segment ? segment->measure() : nullptr;
            System *system = measure ? measure->system() : nullptr;
            p.ry() += measure_staff_y(measure, idx) + system_staff_y(system, idx);
        } else if (parent->is_measure()) {
            Measure *measure = to_measure(parent);
            System *system = measure->system();
            p.ry() += measure_staff_y(measure, idx) + system_staff_y(system, idx);
        } else if (parent->is_system()) {
            System *system = to_system(parent);
            p.ry() += system_staff_y(system, idx);
        } else {
            return p + parent->page_pos();
        }
        p.rx() = page_x();
    } else {
        if (parent->layout_parent()) {
            p += parent->page_pos();
        }
    }
    return p;
}

//---------------------------------------------------------
//   canvasPos
//---------------------------------------------------------

PointF EngravingItem::canvas_pos() const {
    PointF p(pos());
    EngravingItem *parent = layout_parent();
    if (parent == nullptr) {
        return p;
    }

    staff_idx_t idx = effective_staff_idx();

    if (idx == muse::nidx) {
        idx = v_staff_idx();
    }

    auto measure_staff_y = [this](const Measure *measure, staff_idx_t idx) -> double {
        if (!measure) {
            return 0.0;
        }
        if (measure->mstaves().size() <= idx) {
            LOGD("staffIdx out of bounds: %s", type_name());
            return 0.0;
        }
        return measure->staff_lines(idx)->y();
    };

    auto system_staff_y = [this](const System *system, staff_idx_t idx) -> double {
        if (!system) {
            return 0.0;
        }
        if (system->staves().size() <= idx) {
            LOGD("staffIdx out of bounds: %s", type_name());
            return 0.0;
        }
        return system->staff_ypage(idx);
    };

    auto page_y = [](const Page *page) -> double {
        if (!page) {
            return 0.0;
        }
        return page->y();
    };

    if (flags_ & ElementFlag::ON_STAFF) {
        // Segments, measures and systems all contain multiple staves
        // Find the position of the stave that this item is on
        if (parent->is_segment()) {
            Segment *segment = to_segment(parent);
            Measure *measure = segment ? segment->measure() : nullptr;
            System *system = measure ? measure->system() : nullptr;
            Page *page = system ? system->page() : nullptr;
            p.ry() += measure_staff_y(measure, idx) + system_staff_y(system, idx) + page_y(page);
        } else if (parent->is_measure()) {
            Measure *measure = to_measure(parent)->covering_mm_rest_or_this();
            System *system = measure->system();
            Page *page = system->page();
            p.ry() += measure_staff_y(measure, idx) + system_staff_y(system, idx) + page_y(page);
        } else if (parent->is_system()) {
            System *system = to_system(parent);
            p.ry() += system_staff_y(system, idx);
        } else {
            return p + parent->canvas_pos();
        }
        p.rx() = canvas_x();
    } else {
        p += parent->canvas_pos();
    }
    return p;
}

//---------------------------------------------------------
//   pageX
//---------------------------------------------------------

double EngravingItem::page_x() const {
    double xp = x();
    for (EngravingItem *e = layout_parent(); e && e->layout_parent(); e = e->layout_parent()) {
        xp += e->x();
    }
    return xp;
}

//---------------------------------------------------------
//    canvasX
//---------------------------------------------------------

double EngravingItem::canvas_x() const {
    double xp = x();
    for (EngravingItem *e = layout_parent(); e; e = e->layout_parent()) {
        xp += e->x();
    }
    return xp;
}

//---------------------------------------------------------
//   contains
//    Return true if p is inside the shape of the object.
//    Note: p is in page coordinates
//---------------------------------------------------------

bool EngravingItem::contains(const PointF &p) const { return shape().contains(p - page_pos()); }

//---------------------------------------------------------
//  intersects
//    Return true if \a rr intersects bounding box of object.
//    Note: \a rr is in page coordinates
//---------------------------------------------------------

bool EngravingItem::intersects(const RectF &rr) const { return shape().intersects(rr.translated(-page_pos())); }

//---------------------------------------------------------
//   remove
///   Remove \a el from the list. Return true on success.
//---------------------------------------------------------

bool ElementList::remove(EngravingItem *el) {
    auto i = find(begin(), end(), el);
    if (i == end()) {
        return false;
    }
    erase(i);
    return true;
}

//---------------------------------------------------------
//   replace
//---------------------------------------------------------

void ElementList::replace(EngravingItem *o, EngravingItem *n) {
    auto i = find(begin(), end(), o);
    if (i == end()) {
        LOGD("ElementList::replace: element not found");
        return;
    }
    *i = n;
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void EngravingItem::dump() const {
    const LayoutData *ldata = this->ldata();
    LOGD("---EngravingItem: %s, pos(%4.2f,%4.2f)"
         "\n   bbox(%g,%g,%g,%g)"
         "\n   abox(%g,%g,%g,%g)"
         "\n  parent: %p",
         type_name(), ldata->pos().x(), ldata->pos().y(), ldata->bbox().x(), ldata->bbox().y(), ldata->bbox().width(),
         ldata->bbox().height(), page_bounding_rect().x(), page_bounding_rect().y(), page_bounding_rect().width(),
         page_bounding_rect().height(), ownership_parent());
}

//---------------------------------------------------------
//   mimeData
//---------------------------------------------------------

muse::ByteArray EngravingItem::mime_data(const PointF &drag_offset) const {
    auto buffer = Buffer::opened(IODevice::WRITE_ONLY);
    XmlWriter xml(&buffer);

    xml.start_element("EngravingItem");
    if (is_note()) {
        xml.tag_fraction("duration", to_note(this)->chord()->ticks());
    }
    if (!drag_offset.is_null()) {
        xml.tag_point("dragOffset", drag_offset);
    }

    rw::RWRegister::writer()->write_item(this, xml);

    xml.end_element();
    xml.flush();
    buffer.close();
    return buffer.data();
}

//---------------------------------------------------------
//   readType
//    return new position of QDomElement in e
//---------------------------------------------------------

ElementType EngravingItem::read_type(XmlReader &e, PointF *drag_offset, Fraction *duration) {
    while (e.read_next_start_element()) {
        if (e.name() == "EngravingItem") {
            while (e.read_next_start_element()) {
                const AsciiStringView tag = e.name();
                if (tag == "dragOffset") {
                    *drag_offset = e.read_point();
                } else if (tag == "duration") {
                    *duration = e.read_fraction();
                } else {
                    ElementType type = TConv::from_xml(tag, ElementType::INVALID);
                    if (type == ElementType::INVALID) {
                        break;
                    }
                    return type;
                }
            }
        } else {
            e.unknown();
        }
    }
    return ElementType::INVALID;
}

//---------------------------------------------------------
//   readMimeData
//---------------------------------------------------------

EngravingItem *EngravingItem::read_mime_data(Score *score, const muse::ByteArray &data, PointF *drag_offset, Fraction *duration) {
    XmlReader e(data);

    const ElementType type = EngravingItem::read_type(e, drag_offset, duration);
    if (type == ElementType::INVALID) {
        LOGD("cannot read type");
        return nullptr;
    }

    EngravingItem *el = Factory::create_item(type, score->dummy(), false);
    if (el) {
        rw::RWRegister::reader()->read_item(el, e);
    }

    return el;
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void EngravingItem::add(EngravingItem *e) {
    switch (e->type()) {
        case ElementType::PARENTHESIS: {
            Parenthesis *p = to_parenthesis(e);
            p->set_visible(visible());
            if (p->direction() == DirectionH::LEFT) {
                left_parenthesis_ = p;
            } else if (p->direction() == DirectionH::RIGHT) {
                right_parenthesis_ = p;
            }
            break;
        }
        default: LOGD("EngravingItem: cannot add %s to %s", e->type_name(), type_name());
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void EngravingItem::remove(EngravingItem *e) {
    switch (e->type()) {
        case ElementType::PARENTHESIS: {
            if (e == left_parenthesis_) {
                left_parenthesis_ = nullptr;
            }
            if (e == right_parenthesis_) {
                right_parenthesis_ = nullptr;
            }
            break;
        }
        default:
            ASSERT_X(String(u"EngravingItem: cannot remove %1 from %2")
                         .arg(String::from_ascii(e->type_name()), String::from_ascii(type_name())));
    }
}

//---------------------------------------------------------
//   elementLessThan
//---------------------------------------------------------

bool element_less_than(const EngravingItem *const e1, const EngravingItem *const e2) {
    if (e1->z() == e2->z()) {
        if (e1->selected() && !e2->selected()) {
            return false;
        }
        if (!e1->selected() && e2->selected()) {
            return true;
        }
        if (e1->visible() && !e2->visible()) {
            return false;
        }
        if (!e1->visible() && e2->visible()) {
            return true;
        }

        return e1->track() < e2->track();
    }

    return e1->z() < e2->z();
}

//---------------------------------------------------------
//   autoplace
//---------------------------------------------------------

bool EngravingItem::autoplace() const {
    if (!score() || !style().style_b(Sid::AUTOPLACE_ENABLED)) {
        return false;
    }
    return !flag(ElementFlag::NO_AUTOPLACE);
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue EngravingItem::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::TICK: return tick();
        case Pid::TRACK: return track();
        case Pid::VOICE: return voice();
        case Pid::GENERATED: return generated();
        case Pid::COLOR: return PropertyValue::from_value(color());
        case Pid::VISIBLE: return visible();
        case Pid::SELECTED: return selected();
        case Pid::OFFSET: return PropertyValue::from_value(offset_);
        case Pid::MIN_DISTANCE: return min_distance_;
        case Pid::PLACEMENT: return placement();
        case Pid::AUTOPLACE: return autoplace();
        case Pid::Z: return z();
        case Pid::SYSTEM_FLAG: return system_flag();
        case Pid::SIZE_SPATIUM_DEPENDENT: return size_is_spatium_dependent();
        case Pid::POSITION_LINKED_TO_MASTER: return is_position_linked_to_master_value;
        case Pid::APPEARANCE_LINKED_TO_MASTER: return is_appearance_linked_to_master_value;
        case Pid::EXCLUDE_FROM_OTHER_PARTS: return exclude_from_other_parts_value;
        case Pid::EXCLUDE_VERTICAL_ALIGN: return exclude_vertical_align_;
        case Pid::HAS_PARENTHESES: return parentheses_mode();
        default:
            if (ownership_parent()) {
                return ownership_parent()->get_property(property_id);
            }

            return PropertyValue();
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool EngravingItem::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::TRACK: set_track(v.value<track_idx_t>()); break;
        case Pid::VOICE: set_voice(v.value<voice_idx_t>()); break;
        case Pid::GENERATED: set_generated(v.to_bool()); break;
        case Pid::COLOR: set_color(v.value<Color>()); break;
        case Pid::VISIBLE: set_visible(v.to_bool()); break;
        case Pid::SELECTED: set_selected(v.to_bool()); break;
        case Pid::OFFSET: offset_ = v.value<PointF>(); break;
        case Pid::MIN_DISTANCE: set_min_distance(v.value<Spatium>()); break;
        case Pid::PLACEMENT: set_placement(v.value<PlacementV>()); break;
        case Pid::AUTOPLACE: set_autoplace(v.to_bool()); break;
        case Pid::Z: set_z(v.to_int()); break;
        case Pid::SYSTEM_FLAG: set_system_flag(v.to_bool()); break;
        case Pid::SIZE_SPATIUM_DEPENDENT: set_size_is_spatium_dependent(v.to_bool()); break;
        case Pid::POSITION_LINKED_TO_MASTER:
            if (is_position_linked_to_master_value == v.to_bool()) {
                break;
            }
            if (!is_position_linked_to_master_value) {
                relink_properties_to_master(PropertyGroup::POSITION);
            }
            set_position_linked_to_master(v.to_bool());
            break;
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            if (is_appearance_linked_to_master_value == v.to_bool()) {
                break;
            }
            if (!is_appearance_linked_to_master_value) {
                relink_properties_to_master(PropertyGroup::APPEARANCE);
            }
            set_appearance_linked_to_master(v.to_bool());
            break;
        case Pid::EXCLUDE_FROM_OTHER_PARTS: set_exclude_from_other_parts(v.to_bool()); break;
        case Pid::EXCLUDE_VERTICAL_ALIGN: set_exclude_vertical_align(v.to_bool()); break;
        case Pid::HAS_PARENTHESES: set_parentheses_mode(v.value<ParenthesesMode>()); break;
        default:
            if (ownership_parent()) {
                return ownership_parent()->set_property(property_id, v);
            }

            LOG_PROP() << type_name() << " unknown <" << property_name(property_id) << ">(" << int(property_id)
                       << "), data: " << v.value<String>();
            return false;
    }
    trigger_layout();
    return true;
}

void EngravingItem::manage_exclusion_from_parts(bool exclude) {
    if (exclude) {
        const std::list<EngravingObject *> links = link_list();
        for (EngravingObject *linked_object : links) {
            if (linked_object->score() == score()) {
                continue;
            }
            EngravingItem *linked_item = to_engraving_item(linked_object);
            if (linked_item->selected()) {
                linked_item->score()->deselect(linked_item);
            }
            linked_item->score()->undo_remove_element(linked_item, false);
            linked_item->undo_unlink();
        }
    } else {
        score()->undo_add_element(this, /*addToLinkedStaves*/ true, /*ctrlModifier*/ false, this);
    }
}

EngravingItem *EngravingItem::shared_item() const { return ldata()->shared_item_; }

const std::vector<EngravingItem *> &EngravingItem::origin_items() const { return ldata()->origin_items_; }

void EngravingItem::connect_shared_item(EngravingItem *shared_item, EngravingItem *origin_item) {
    if (origin_item->ldata()->shared_item_ == shared_item) {
        return;
    }

    IF_ASSERT_FAILED(shared_item->type() == origin_item->type()) { return; }

    IF_ASSERT_FAILED(!shared_item->is_chord() && !origin_item->is_chord()) {
        // Notes are connected, not chords
        return;
    }

    IF_ASSERT_FAILED(shared_item->ldata()->shared_item_ == nullptr && origin_item->ldata()->origin_items_.empty()) { return; }

    origin_item->mutldata()->shared_item_ = shared_item;

    std::vector<EngravingItem *> &cur_origin_items = shared_item->mutldata()->origin_items_;
    auto it = std::lower_bound(cur_origin_items.begin(), cur_origin_items.end(), origin_item,
                               [](EngravingItem *item1, EngravingItem *item2) { return item1->track() < item2->track(); });

    cur_origin_items.insert(it, origin_item);
}

void EngravingItem::disconnect_shared_item(EngravingItem *shared_item, EngravingItem *origin_item) {
    IF_ASSERT_FAILED(origin_item->layout_data_->shared_item_ == shared_item) { return; }

    origin_item->layout_data_->shared_item_ = nullptr;

    DO_ASSERT(muse::remove(shared_item->layout_data_->origin_items_, origin_item));
}

void EngravingItem::disconnect_all_origin_items(EngravingItem *shared_item) {
    for (EngravingItem *origin_item : shared_item->origin_items()) {
        origin_item->layout_data_->shared_item_ = nullptr;
    }

    shared_item->layout_data_->origin_items_.clear();
}

void EngravingItem::disconnect_shared_tree(EngravingItem *item) {
    auto disconnect = [](EngravingItem *item) {
        if (EngravingItem *shared_item = item->shared_item()) {
            disconnect_shared_item(shared_item, item);
        }

        disconnect_all_origin_items(item);
    };

    if (item->is_spanner()) {
        disconnect(item);
        for (SpannerSegment *ss : to_spanner(item)->spanner_segments()) {
            ss->scan_elements(disconnect);
        }
        return;
    }

    item->scan_elements(disconnect);
}

bool EngravingItem::is_before(const EngravingItem *item) const {
    if (!item) {
        return false;
    }
    if (tick() != item->tick()) {
        return tick() < item->tick();
    }

    const Measure *this_measure = find_measure();
    const Measure *other_measure = item->find_measure();
    if (this_measure != other_measure) {
        return this_measure->is_before(other_measure);
    }

    const EngravingItem *this_seg = find_ancestor(ElementType::SEGMENT);
    const EngravingItem *other_seg = item->find_ancestor(ElementType::SEGMENT);
    if (!this_seg || !other_seg || !this_seg->is_segment() || !other_seg->is_segment()) {
        return false;
    }

    return to_segment(this_seg)->goes_before(to_segment(other_seg));
}

const Staff *EngravingItem::staff_to_center_against(bool above, const System *system) const {
    const Part *this_part = part();
    const Staff *this_staff = staff();
    if (!this_part || !this_staff) {
        return nullptr;
    }

    const Fraction item_tick = tick();
    const Instrument *this_part_instrument = this_part->instrument(item_tick);
    const bool this_instrument_is_vocal = this_part_instrument && this_part_instrument->is_vocal_instrument();
    const bool participates_in_staff_centering = this_part->nstaves() > 1 || this_instrument_is_vocal;
    if (!participates_in_staff_centering) {
        return nullptr;
    }

    if (!system) {
        if (is_spanner()) {
            const Segment *start_seg = to_spanner(this)->start_segment();
            system = start_seg ? start_seg->measure()->system() : nullptr;
        } else {
            system = to_system(find_ancestor(ElementType::SYSTEM));
        }
    }

    if (!system) {
        return nullptr;
    }

    const staff_idx_t this_idx = this_staff->idx();
    const staff_idx_t other_idx = above ? system->prev_visible_staff(this_idx) : system->next_visible_staff(this_idx);
    if (other_idx == muse::nidx) {
        return nullptr;
    }

    const Staff *other_staff = score()->staff(other_idx);
    if (!other_staff) {
        return nullptr;
    }

    const Part *other_part = other_staff->part();
    if (other_part == this_part) {
        return other_staff;
    }

    const Instrument *other_part_instrument = other_part->instrument(item_tick);
    const bool other_instrument_is_vocal = other_part_instrument && other_part_instrument->is_vocal_instrument();

    // If the staves are not of the same part, only allow centering if they are both vocal parts:
    return this_instrument_is_vocal && other_instrument_is_vocal ? other_staff : nullptr;
}

bool EngravingItem::applies_to_all_voices_in_instrument() const {
    return has_voice_assignment_properties() &&
           get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>() == VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
}

void EngravingItem::set_initial_track_and_voice_assignment(track_idx_t track, bool cur_voice_only_override) {
    IF_ASSERT_FAILED(track != muse::nidx) { return; }

    if (configuration()->dynamics_apply_to_all_voices() && !cur_voice_only_override) {
        set_track(track_zero_voice(track));
        set_property(Pid::VOICE_ASSIGNMENT, VoiceAssignment::ALL_VOICE_IN_INSTRUMENT);
    } else {
        set_track(track);
        set_property(Pid::VOICE_ASSIGNMENT, VoiceAssignment::CURRENT_VOICE_ONLY);
    }
}

void EngravingItem::check_voice_assignment_compatible_with_track() {
    voice_idx_t current_voice = voice();
    VoiceAssignment voice_assignment = get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>();

    if (voice_assignment != VoiceAssignment::CURRENT_VOICE_ONLY && current_voice != 0) {
        set_property(Pid::TRACK, track_zero_voice(track()));
    }
}

bool EngravingItem::element_applies_to_track(const track_idx_t ref_track) const {
    if (!has_voice_assignment_properties()) {
        return ref_track == track();
    }
    const VoiceAssignment voice_assignment = get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>();

    return element_applies_to_track(track(), ref_track, voice_assignment, part());
}

void EngravingItem::set_placement_based_on_voice_assignment(DirectionV styled_direction) {
    PlacementV old_placement = placement();
    PlacementV new_placement = PlacementV::BELOW;

    DirectionV internal_direction_property = get_property(Pid::DIRECTION).value<DirectionV>();
    const bool direction_is_auto = internal_direction_property == DirectionV::AUTO && styled_direction == DirectionV::AUTO;
    const bool center_between_staves = get_property(Pid::CENTER_BETWEEN_STAVES).value<AutoOnOff>() == AutoOnOff::ON;
    const bool gap_above = direction_is_auto && center_between_staves && staff_to_center_against(true);
    const bool gap_below = direction_is_auto && center_between_staves && staff_to_center_against(false);

    if (internal_direction_property != DirectionV::AUTO) {
        new_placement = internal_direction_property == DirectionV::UP ? PlacementV::ABOVE : PlacementV::BELOW;
    } else if (styled_direction != DirectionV::AUTO) {
        new_placement = styled_direction == DirectionV::UP ? PlacementV::ABOVE : PlacementV::BELOW;
    } else if (center_between_staves && part()->nstaves() > 1 && (gap_above || gap_below)) {
        bool is_on_last_staff_of_instrument = staff_idx() == part()->staves().back()->idx();
        new_placement = is_on_last_staff_of_instrument ? PlacementV::ABOVE : PlacementV::BELOW;
    } else {
        VoiceAssignment voice_assignment = get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>();
        if (voice_assignment == VoiceAssignment::ALL_VOICE_IN_INSTRUMENT || voice_assignment == VoiceAssignment::ALL_VOICE_IN_STAFF) {
            if (style().style_b(Sid::DYNAMICS_HAIRPINS_ABOVE_FOR_VOCAL_STAVES) &&
                origin_part()->instrument(tick())->is_vocal_instrument()) {
                new_placement = PlacementV::ABOVE;
            } else {
                new_placement = PlacementV::BELOW;
            }
        } else if (voice() == 0) {
            // Put above the staff only in case of multiple voices at this tick (similar to stem directions)
            const Measure *measure = score()->tick2measure(tick());
            Fraction start_tick = Fraction(-1, 1);
            Fraction length = Fraction(-1, 1);
            if (is_spanner()) {
                start_tick = tick();
                length = to_spanner(this)->ticks();
            } else if (const Segment *segment = to_segment(find_ancestor(ElementType::SEGMENT))) {
                if (segment && segment->is_time_tick_type() && segment->measure() != measure) {
                    // Edge case: this is a TimeTick segment at the end of previous measure. Happens only
                    // when dynamic is anchorToEndOfPrevious. In this case look for preceding segment.
                    segment = segment->prev1(SegmentType::DURATION);
                    assert(segment);
                    measure = segment->measure();
                }
                start_tick = segment->tick();
                length = segment->ticks();
            } else if (measure) {
                start_tick = measure->tick();
                length = measure->ticks();
            }
            if (measure && measure->has_voices(staff_idx(), start_tick, length)) {
                new_placement = PlacementV::ABOVE;
            } else {
                new_placement = PlacementV::BELOW;
            }
        } else {
            new_placement = voice() % 2 ? PlacementV::BELOW : PlacementV::ABOVE;
        }
    }

    if (direction_is_auto && center_between_staves) {
        /* If the preferred side has no staff to center against but the other side has one, place
         * the item on that side instead. If neither side has one, leave the placement alone: */
        const bool prefer_above = new_placement == PlacementV::ABOVE;
        const bool preferred_gap = prefer_above ? gap_above : gap_below;
        const bool other_gap = prefer_above ? gap_below : gap_above;
        if (!preferred_gap && other_gap) {
            new_placement = prefer_above ? PlacementV::BELOW : PlacementV::ABOVE;
        }
    }

    if (new_placement != old_placement) {
        set_placement(new_placement);
    }
}

void EngravingItem::relink_properties_to_master(PropertyGroup prop_group) {
    assert(!score()->is_master());

    const std::list<EngravingObject *> linked_elements = link_list_for_property_propagation();
    EngravingObject *master_element = nullptr;
    for (EngravingObject *element : linked_elements) {
        if (element->score()->is_master()) {
            master_element = element;
            break;
        }
    }

    if (!master_element) {
        return;
    }

    for (int i = 0; i < static_cast<int>(Pid::END); ++i) {
        const Pid property_id = static_cast<Pid>(i);
        if (property_group(property_id) != prop_group) {
            continue;
        }
        const PropertyValue master_value = master_element->get_property(property_id);
        const PropertyFlags master_flags = master_element->property_flags(property_id);
        if (get_property(property_id) != master_value) {
            set_property(property_id, master_value);
        }
        if (property_flags(property_id) != master_flags) {
            set_property_flags(property_id, master_flags);
        }
    }
}

void EngravingItem::relink_property_to_master(Pid property_id) {
    assert(!score()->is_master());

    const std::list<EngravingObject *> linked_elements = link_list_for_property_propagation();
    EngravingObject *master_element = nullptr;
    for (EngravingObject *element : linked_elements) {
        if (element->score()->is_master()) {
            master_element = element;
            break;
        }
    }

    if (!master_element) {
        return;
    }

    set_property(property_id, master_element->get_property(property_id));
    set_property_flags(property_id, master_element->property_flags(property_id));
}

PropertyPropagation EngravingItem::property_propagation(const EngravingItem *destination_item, Pid property_id) const {
    assert(destination_item != this);

    if (property_link(property_id)) {
        return PropertyPropagation::PROPAGATE; // These properties are always linked, no matter what
    }

    if (property_group(property_id) == PropertyGroup::NONE) {
        return PropertyPropagation::NONE;
    }

    const Score *source_score = score();
    const Score *destination_score = destination_item->score();
    const Staff *source_staff = staff();
    const Staff *destination_staff = destination_item->staff();

    if (source_score == destination_score) {
        const bool diff_staff = source_staff != destination_staff;
        const bool visible_position_or_color =
            property_id == Pid::VISIBLE || property_id == Pid::COLOR || property_group(property_id) == PropertyGroup::POSITION;
        const bool has_parens =
            property_id == Pid::HAS_PARENTHESES && is_note() && to_note(this)->ghost() && !to_note(this)->hide_generated_parens();
        const bool link_same_score = property_link_same_score(property_id);
        if ((diff_staff && (visible_position_or_color || has_parens)) || !link_same_score) {
            // Allow visibility and position to stay independent
            return PropertyPropagation::NONE;
        }
        // Maintain every other property synced
        return PropertyPropagation::PROPAGATE;
    }

    const bool is_text_property = property_group(property_id) == PropertyGroup::TEXT;
    if (is_text_property) {
        if (source_score->is_master() && destination_item->is_property_linked_to_master(property_id)) {
            // From master score - check if destination part follows master
            return PropertyPropagation::PROPAGATE;
        }

        if (!source_score->is_master() && is_property_linked_to_master(property_id)) {
            // From part - check if source part follows master
            return PropertyPropagation::PROPAGATE;
        }
    }

    if (!source_score->is_master()) {
        // Properties are only propagated when being edited from master. If this is being edited
        // from a part score, we mark it as unlinked so it becomes independent in the part.
        return PropertyPropagation::UNLINK;
    }

    if (system_flag() && !is_top_system_object()) {
        // Let only the top system object propagate
        return PropertyPropagation::NONE;
    }

    if (destination_item->is_property_linked_to_master(property_id)) {
        return PropertyPropagation::PROPAGATE;
    }

    return PropertyPropagation::NONE;
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void EngravingItem::undo_change_property(Pid pid, const PropertyValue &val, PropertyFlags ps) {
    if (pid == Pid::AUTOPLACE && (val.to_bool() == true && !autoplace())) {
        // Switching autoplacement on. Save user-defined
        // placement properties to undo stack.
        undo_push_property(Pid::PLACEMENT);
        undo_push_property(Pid::OFFSET);
    }
    EngravingObject::undo_change_property(pid, val, ps);
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue EngravingItem::property_default(Pid pid) const {
    switch (pid) {
        case Pid::GENERATED: return false;
        case Pid::VISIBLE: return true;
        case Pid::COLOR: return PropertyValue::from_value(configuration()->default_color());
        case Pid::PLACEMENT: {
            PropertyValue v = EngravingObject::property_default(pid);
            if (v.is_valid()) { // if it's a styled property
                return v;
            }
            return PlacementV::BELOW;
        }
        case Pid::SELECTED: return false;
        case Pid::OFFSET: {
            return PropertyValue::from_value(PointF());
        }
        case Pid::MIN_DISTANCE: {
            PropertyValue v = EngravingObject::property_default(pid);
            if (v.is_valid()) {
                return v;
            }
            return 0.0_sp;
        }
        case Pid::AUTOPLACE: return true;
        case Pid::Z: return default_z(type());
        case Pid::POSITION_LINKED_TO_MASTER: return true;
        case Pid::APPEARANCE_LINKED_TO_MASTER: return true;
        case Pid::EXCLUDE_FROM_OTHER_PARTS: return false;
        case Pid::EXCLUDE_VERTICAL_ALIGN: return false;
        case Pid::HAS_PARENTHESES: return ParenthesesMode::NONE;
        default: {
            PropertyValue v = EngravingObject::property_default(pid);

            if (v.is_valid()) {
                return v;
            }

            if (ownership_parent()) {
                return ownership_parent()->property_default(pid);
            }

            return PropertyValue();
        }
    }
}

//---------------------------------------------------------
//   custom
//    check if property is != default
//---------------------------------------------------------

bool EngravingItem::custom(Pid id) const { return property_default(id) != get_property(id); }

//---------------------------------------------------------
//   isPrintable
//---------------------------------------------------------

bool EngravingItem::is_printable() const {
    switch (type()) {
        case ElementType::PAGE:
        case ElementType::SYSTEM:
        case ElementType::MEASURE:
        case ElementType::SEGMENT:
        case ElementType::VBOX:
        case ElementType::HBOX:
        case ElementType::TBOX:
        case ElementType::FBOX:
        case ElementType::SPACER: return false;
        default: return true;
    }
}

bool EngravingItem::is_playable() const {
    switch (type()) {
        case ElementType::NOTE:
        case ElementType::CHORD: return true;
        case ElementType::HARMONY: return ownership_parent() && ownership_parent()->is_segment();
        default: return false;
    }
}

bool EngravingItem::is_system_object_below_bottom_staff() const {
    return system_flag() && staff() && staff()->has_system_objects_below_bottom_staff();
}

//---------------------------------------------------------
//   findAncestor
//---------------------------------------------------------

EngravingItem *EngravingItem::find_ancestor(ElementType t) {
    EngravingItem *e = this;
    while (e && e->type() != t) {
        e = e->layout_parent();
    }
    return e;
}

const EngravingItem *EngravingItem::find_ancestor(ElementType t) const {
    const EngravingItem *e = this;
    while (e && e->type() != t) {
        e = e->layout_parent();
    }
    return e;
}

//---------------------------------------------------------
//   findMeasure
//---------------------------------------------------------

Measure *EngravingItem::find_measure() {
    if (is_measure()) {
        return to_measure(this);
    } else if (EngravingItem *p = layout_parent()) {
        return p->find_measure();
    } else {
        return 0;
    }
}

//---------------------------------------------------------
//   findMeasure
//---------------------------------------------------------

const Measure *EngravingItem::find_measure() const {
    EngravingItem *e = const_cast<EngravingItem *>(this);
    return e->find_measure();
}

//---------------------------------------------------------
//   findMeasureBase
//---------------------------------------------------------

MeasureBase *EngravingItem::find_measure_base() {
    if (is_measure_base()) {
        return to_measure_base(this);
    } else if (EngravingItem *p = layout_parent()) {
        return p->find_measure_base();
    } else {
        return 0;
    }
}

//---------------------------------------------------------
//   findMeasureBase
//---------------------------------------------------------

const MeasureBase *EngravingItem::find_measure_base() const {
    EngravingItem *e = const_cast<EngravingItem *>(this);
    return e->find_measure_base();
}

//---------------------------------------------------------
//   undoSetVisible
//---------------------------------------------------------

void EngravingItem::undo_set_visible(bool v) { undo_change_property(Pid::VISIBLE, v); }

void EngravingItem::undo_add_element(EngravingItem *element, bool add_to_linked_staves) {
    score()->undo_add_element(element, add_to_linked_staves);
}

//---------------------------------------------------------
//   drawSymbol
//---------------------------------------------------------

void EngravingItem::draw_symbol(SymId id, Painter *p, const PointF &o, double scale) const {
    score()->engraving_font()->draw(id, p, mag_s() * scale, o);
}

void EngravingItem::draw_symbols(const SymIdList &symbols, Painter *p, const PointF &o, double scale) const {
    score()->engraving_font()->draw(symbols, p, mag_s() * scale, o);
}

void EngravingItem::draw_symbols(const SymIdList &symbols, Painter *p, const PointF &o, const SizeF &scale) const {
    score()->engraving_font()->draw(symbols, p, SizeF(mag_s() * scale), o);
}

//---------------------------------------------------------
//   symHeight
//---------------------------------------------------------

double EngravingItem::sym_height(SymId id) const { return score()->engraving_font()->height(id, mag_s()); }

//---------------------------------------------------------
//   symWidth
//---------------------------------------------------------

double EngravingItem::sym_width(SymId id) const { return score()->engraving_font()->width(id, mag_s()); }

double EngravingItem::sym_width(const SymIdList &symbols) const { return score()->engraving_font()->width(symbols, mag_s()); }

//---------------------------------------------------------
//   symAdvance
//---------------------------------------------------------

double EngravingItem::sym_advance(SymId id) const { return score()->engraving_font()->advance(id, mag_s()); }

//---------------------------------------------------------
//   symBbox
//---------------------------------------------------------

RectF EngravingItem::sym_bbox(SymId id) const { return score()->engraving_font()->bbox(id, mag_s()); }

RectF EngravingItem::sym_bbox(const SymIdList &symbols) const { return score()->engraving_font()->bbox(symbols, mag_s()); }

Shape EngravingItem::sym_shape_with_cutouts(SymId id) const {
    Shape shape = score()->engraving_font()->shape_with_cutouts(id, mag_s());
    for (ShapeElement &element : shape.elements()) {
        element.set_item(this);
    }

    return shape;
}

//---------------------------------------------------------
//   symSmuflAnchor
//---------------------------------------------------------

PointF EngravingItem::sym_smufl_anchor(SymId sym_id, SmuflAnchorId anchor_id) const {
    return score()->engraving_font()->smufl_anchor(sym_id, anchor_id, mag_s());
}

//---------------------------------------------------------
//   symIsValid
//---------------------------------------------------------

bool EngravingItem::sym_is_valid(SymId id) const { return score()->engraving_font()->is_valid(id); }

//---------------------------------------------------------
//   concertPitch
//---------------------------------------------------------

bool EngravingItem::concert_pitch() const { return style().style_b(Sid::CONCERT_PITCH); }

//---------------------------------------------------------
//   nextElement
//   selects the next score element
//---------------------------------------------------------

EngravingItem *EngravingItem::next_element() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().front();
    }
    if (e) {
        switch (e->type()) {
            case ElementType::SEGMENT: {
                Segment *s = to_segment(e);
                return s->next_element(staff_idx());
            }
            case ElementType::MEASURE: {
                Measure *m = to_measure(e);
                return m->next_element_staff(staff_idx());
            }
            case ElementType::CLEF:
            case ElementType::KEYSIG:
            case ElementType::TIMESIG:
            case ElementType::BAR_LINE: return next_segment_element();
            default: {
                return e->layout_parent()->next_element();
            }
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   prevElement
//   selects the previous score element
//---------------------------------------------------------

EngravingItem *EngravingItem::prev_element() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().back();
    }
    if (e) {
        switch (e->type()) {
            case ElementType::SEGMENT: {
                Segment *s = to_segment(e);
                return s->prev_element(staff_idx());
            }
            case ElementType::MEASURE: {
                Measure *m = to_measure(e);
                return m->prev_element_staff(staff_idx());
            }
            case ElementType::CLEF:
            case ElementType::KEYSIG:
            case ElementType::TIMESIG:
            case ElementType::BAR_LINE: return prev_segment_element();
            default: {
                return e->layout_parent()->prev_element();
            }
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------------------
//   nextSegmentElement
//   This function is used in for the next-element command to navigate between main elements
//   of segments. (Note, Rest, Clef, Time Signature, Key Signature, Barline, Ambitus, Breath, etc.)
//   The default implementation is to look for the first such element. After it is found each
//   element knows how to find the next one and overrides this method
//------------------------------------------------------------------------------------------

EngravingItem *EngravingItem::next_segment_element() {
    EngravingItem *p = this;
    while (p) {
        switch (p->type()) {
            case ElementType::NOTE:
                if (to_note(p)->chord()->is_grace()) {
                    break;
                }
                return p;
            case ElementType::REST:
            case ElementType::MMREST: return p;
            case ElementType::CHORD: {
                Chord *c = to_chord(p);
                if (!c->is_grace()) {
                    return c->notes().back();
                }
            } break;
            case ElementType::SEGMENT: {
                Segment *s = to_segment(p);
                return s->first_element_for_navigation(staff_idx());
            }
            case ElementType::MEASURE: {
                Measure *m = to_measure(p);
                return m->next_element_staff(staff_idx());
            }
            case ElementType::SYSTEM: {
                System *sys = to_system(p);
                return sys->next_segment_element();
            }
            default: break;
        }
        p = p->layout_parent();
    }
    return Navigation::first_element(score());
}

//------------------------------------------------------------------------------------------
//   prevSegmentElement
//   This function is used in for the prev-element command to navigate between main elements
//   of segments. (Note, Rest, Clef, Time Signature, Key Signature, Barline, Ambitus, Breath, etc.)
//   The default implementation is to look for the first such element. After it is found each
//   element knows how to find the previous one and overrides this method
//------------------------------------------------------------------------------------------

EngravingItem *EngravingItem::prev_segment_element() {
    EngravingItem *p = this;
    while (p) {
        switch (p->type()) {
            case ElementType::NOTE:
                if (to_note(p)->chord()->is_grace()) {
                    break;
                }
                return p;
            case ElementType::REST:
            case ElementType::MMREST: return p;
            case ElementType::CHORD: {
                Chord *c = to_chord(p);
                if (!c->is_grace()) {
                    return c->notes().front();
                }
            } break;
            case ElementType::SEGMENT: {
                Segment *s = to_segment(p);
                return s->last_element_for_navigation(staff_idx());
            }
            case ElementType::MEASURE: {
                Measure *m = to_measure(p);
                return m->prev_element_staff(staff_idx());
            }
            case ElementType::SYSTEM: {
                System *sys = to_system(p);
                return sys->prev_segment_element();
            }
            default: break;
        }
        p = p->layout_parent();
    }
    return Navigation::first_element(score());
}

//---------------------------------------------------------
//   isUserModified
//    Check if this element was modified by user and
//    therefore must be saved.
//---------------------------------------------------------

bool EngravingItem::is_user_modified() const {
    for (const StyledProperty &spp : *styled_properties()) {
        Pid pid = spp.pid;
        PropertyValue val = get_property(pid);
        PropertyValue default_value = property_default(pid);

        if (property_type(pid) == PType::ABSOLUTE) {
            if (std::abs(val.value<double>() - default_value.value<double>()) > 0.0001) { // we don’t care spatium diffs that small
                return true;
            }
        } else {
            if (get_property(pid) != property_default(pid)) {
                return true;
            }
        }
    }
    for (Pid p : {Pid::VISIBLE, Pid::OFFSET, Pid::COLOR, Pid::Z, Pid::AUTOPLACE}) {
        if (get_property(p) != property_default(p)) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void EngravingItem::trigger_layout() const {
    if (ownership_parent()) {
        score()->set_layout(tick(), staff_idx(), this);
    }
}

//----------------------------------------------------------------------
//   triggerLayoutAll
//
//   *************************** CAUTION *******************************
//   This causes a layout of the entire score: extremely expensive and
//   likely unnecessary! Consider overriding triggerLayout() instead.
//----------------------------------------------------------------------

void EngravingItem::trigger_layout_all() const {
    if (ownership_parent()) {
        score()->set_layout_all(staff_idx(), this);
    }
}

void EngravingItem::trigger_layout_to_end() const {
    if (ownership_parent()) {
        score()->set_layout(tick(), score()->end_tick(), staff_idx(), staff_idx(), this);
    }
}

//---------------------------------------------------------
//   addData
//---------------------------------------------------------

void EditData::add_data(std::shared_ptr<ElementEditData> ed) { data_.push_back(ed); }

//---------------------------------------------------------
//   styleP
//---------------------------------------------------------

double EngravingItem::style_p(Sid idx) const { return style().style_absolute(idx); }

void EngravingItem::set_parentheses_mode(const ParenthesesMode &v, bool add_to_linked, bool generated) {
    set_has_left_parenthesis(v & ParenthesesMode::LEFT, add_to_linked, generated);
    set_has_right_parenthesis(v & ParenthesesMode::RIGHT, add_to_linked, generated);
}

ParenthesesMode EngravingItem::parentheses_mode() const {
    ParenthesesMode p = ParenthesesMode::NONE;
    if (left_parenthesis_) {
        p |= ParenthesesMode::LEFT;
    }
    if (right_parenthesis_) {
        p |= ParenthesesMode::RIGHT;
    }

    return p;
}

void EngravingItem::set_has_left_parenthesis(bool v, bool add_to_linked, bool generated) {
    const bool has_generated_paren = left_parenthesis_ && left_parenthesis_->generated();
    const bool has_user_paren = left_parenthesis_ && !left_parenthesis_->generated();

    if (generated && v == has_generated_paren) {
        return;
    }

    if (!generated && v == has_user_paren) {
        return;
    }

    if (v) {
        if (!left_parenthesis_) {
            Parenthesis *paren = Factory::create_parenthesis(this);
            paren->set_track(track());
            paren->set_direction(DirectionH::LEFT);
            paren->set_generated(generated);

            score()->undo_add_element(paren, add_to_linked);
        }
    } else {
        score()->undo_remove_element(left_parenthesis_, add_to_linked);
        assert(left_parenthesis_ == nullptr);
    }
}

void EngravingItem::set_has_right_parenthesis(bool v, bool add_to_linked, bool generated) {
    const bool has_generated_paren = right_parenthesis_ && right_parenthesis_->generated();
    const bool has_user_paren = right_parenthesis_ && !right_parenthesis_->generated();

    if (generated && v == has_generated_paren) {
        return;
    }

    if (!generated && v == has_user_paren) {
        return;
    }

    if (v) {
        if (!right_parenthesis_) {
            Parenthesis *paren = Factory::create_parenthesis(this);
            paren->set_track(track());
            paren->set_direction(DirectionH::RIGHT);
            paren->set_generated(generated);

            score()->undo_add_element(paren, add_to_linked);
        }
    } else {
        score()->undo_remove_element(right_parenthesis_, add_to_linked);
        assert(right_parenthesis_ == nullptr);
    }
}

EngravingItem::BarBeat EngravingItem::barbeat() const {
    EngravingItem::BarBeat bar_beat = {0, 0, 0.0F};
    const EngravingItem *parent = this;
    while (parent && !parent->is_segment() && !parent->is_measure()) {
        parent = parent->layout_parent();
    }

    if (!parent) {
        return bar_beat;
    }

    int bar = 0;
    int displayed_bar = 0;
    int beat = 0;
    int ticks = 0;
    const Measure *measure = nullptr;

    const TimeSigMap *time_sig_map = score()->sigmap();
    int ticks_b = ticks_beat(time_sig_map->timesig(0).timesig().denominator());

    if (parent->is_segment()) {
        const Segment *segment = to_segment(parent);
        time_sig_map->tick_values(segment->tick().ticks(), &bar, &beat, &ticks);
        ticks_b = ticks_beat(time_sig_map->timesig(segment->tick().ticks()).timesig().denominator());
        measure = segment->find_measure();
        if (measure) {
            displayed_bar = measure->measure_number();
        }
    } else if (parent->is_measure()) {
        measure = to_measure(parent);
        bar = measure->measure_number();
        displayed_bar = bar;
        beat = -1;
        ticks = 0;
    }

    bar_beat = {bar + 1, displayed_bar + 1, beat + 1 + ticks / static_cast<float>(ticks_b)};
    return bar_beat;
}

EngravingItem *EngravingItem::find_linked_in_score(const Score *score) const {
    if (!score || !staff() || !links() || links()->empty()) {
        return nullptr;
    }

    // If this is a system element it may not be on the same stave, so just check if linked elements are in the score
    if (system_flag() && track() == 0) {
        for (EngravingObject *linked : *links()) {
            if (linked != this && linked->score() == score) {
                return to_engraving_item(linked);
            }
        }
        return nullptr;
    }

    Staff *linked_staff_in_score = staff()->find_linked_in_score(score);

    if (!linked_staff_in_score) {
        return nullptr;
    }

    return find_linked_in_staff(linked_staff_in_score);
}

EngravingItem *EngravingItem::find_linked_in_staff(const Staff *staff) const {
    if (!links() || links()->empty()) {
        return nullptr;
    }

    for (EngravingObject *linked : *links()) {
        if (to_engraving_item(linked)->staff() == staff) {
            return to_engraving_item(linked);
        }
    }
    return nullptr;
}

bool EngravingItem::selected() const { return flag(ElementFlag::SELECTED); }

void EngravingItem::set_selected(bool f) { set_flag(ElementFlag::SELECTED, f); }

void EngravingItem::set_visible(bool f) {
    set_flag(ElementFlag::INVISIBLE, !f);
    if (left_parenthesis_) {
        left_parenthesis_->set_visible(f);
    }
    if (right_parenthesis_) {
        right_parenthesis_->set_visible(f);
    }
}

String EngravingItem::format_bars_and_beats() const {
    String result;
    EngravingItem::BarBeat barbeat = this->barbeat();

    if (barbeat.bar != 0) {
        result = muse::mtrc("engraving", "Measure: %1").arg(barbeat.bar);

        if (barbeat.displayed_bar != barbeat.bar) {
            result += u"; " + muse::mtrc("engraving", "Displayed measure: %1").arg(barbeat.displayed_bar);
        }

        if (!muse::real_is_null(barbeat.beat)) {
            result += u"; " + muse::mtrc("engraving", "Beat: %1").arg(barbeat.beat);
        }
    }

    return result;
}

bool EngravingItem::is_property_linked_to_master(Pid id) const {
    if (property_group(id) == PropertyGroup::POSITION) {
        return is_position_linked_to_master();
    }

    if (property_group(id) == PropertyGroup::APPEARANCE) {
        return is_appearance_linked_to_master();
    }

    return true;
}

bool EngravingItem::is_unlinked_from_master() const {
    return !(get_property(Pid::POSITION_LINKED_TO_MASTER).to_bool() && get_property(Pid::APPEARANCE_LINKED_TO_MASTER).to_bool());
}

void EngravingItem::unlink_property_from_master(Pid id) {
    if (property_group(id) == PropertyGroup::POSITION) {
        score()->undo(new ChangeProperty(this, Pid::POSITION_LINKED_TO_MASTER, false));
    } else if (property_group(id) == PropertyGroup::APPEARANCE) {
        score()->undo(new ChangeProperty(this, Pid::APPEARANCE_LINKED_TO_MASTER, false));
    }
}

EngravingItem::LayoutData *EngravingItem::create_layout_data() const { return new EngravingItem::LayoutData(); }

const EngravingItem::LayoutData *EngravingItem::ldata() const { return ldata_internal(); }

EngravingItem::LayoutData *EngravingItem::mutldata() { return mutldata_internal(); }

const EngravingItem::LayoutData *EngravingItem::ldata_internal() const {
    if (!layout_data_) {
        layout_data_ = create_layout_data();
        layout_data_->item_ = this;
    }
    return layout_data_;
}

EngravingItem::LayoutData *EngravingItem::mutldata_internal() {
    if (!layout_data_) {
        layout_data_ = create_layout_data();
        layout_data_->item_ = this;
    }
    return layout_data_;
}

bool EngravingItem::element_applies_to_track(const track_idx_t element_track, const track_idx_t ref_track,
                                             const VoiceAssignment voice_assignment, const Part *part) {
    if (voice_assignment == VoiceAssignment::CURRENT_VOICE_ONLY && element_track == ref_track) {
        return true;
    }

    if (voice_assignment == VoiceAssignment::ALL_VOICE_IN_STAFF && track2staff(element_track) == track2staff(ref_track)) {
        return true;
    }

    if (!part) {
        return false;
    }

    if (voice_assignment == VoiceAssignment::ALL_VOICE_IN_INSTRUMENT) {
        const TrackRange range = part->track_range();
        if (range.start_track <= ref_track && range.end_track - 1 >= ref_track) {
            return true;
        }
    }
    return false;
}

void EngravingItem::LayoutData::set_bbox(const RectF &r) {
    DO_ASSERT(!std::isnan(r.x()) && !std::isinf(r.x()));
    DO_ASSERT(!std::isnan(r.y()) && !std::isinf(r.y()));
    DO_ASSERT(!std::isnan(r.width()) && !std::isinf(r.width()));
    DO_ASSERT(!std::isnan(r.height()) && !std::isinf(r.height()));

    //DO_ASSERT(!isShapeComposite());
    shape_.set_value(Shape(r, item_, Shape::Type::FIXED));
}

void EngravingItem::LayoutData::connect_item_snapped_before(EngravingItem *item_before) {
    IF_ASSERT_FAILED(item_before && item_before != item_ && item_before != item_snapped_after_ &&
                     item_before->ldata()->item_snapped_before() != item_) {
        return;
    }
    item_snapped_before_ = item_before;
    item_before->mutldata()->item_snapped_after_ = const_cast<EngravingItem *>(item_);
}

void EngravingItem::LayoutData::disconnect_item_snapped_before() {
    if (!item_snapped_before_) {
        return;
    }
    item_snapped_before_->mutldata()->item_snapped_after_ = nullptr;
    item_snapped_before_ = nullptr;
}

void EngravingItem::LayoutData::connect_item_snapped_after(EngravingItem *item_after) {
    IF_ASSERT_FAILED(item_after && item_after != item_ && item_after != item_snapped_before_ &&
                     item_after->ldata()->item_snapped_after() != item_) {
        return;
    }
    item_snapped_after_ = item_after;
    item_after->mutldata()->item_snapped_before_ = const_cast<EngravingItem *>(item_);
}

void EngravingItem::LayoutData::disconnect_item_snapped_after() {
    if (!item_snapped_after_) {
        return;
    }
    item_snapped_after_->mutldata()->item_snapped_before_ = nullptr;
    item_snapped_after_ = nullptr;
}

void EngravingItem::LayoutData::scan_connected_items_in_snapping_chain(const System *system,
                                                                       const std::function<void(EngravingItem *)> &func) const {
    EngravingItem *item = const_cast<EngravingItem *>(item_);
    func(item);

    std::vector<EngravingItem *> visited = {item}; // To break out of circular snap chains

    EngravingItem *snapped_before = item_snapped_before_;
    while (snapped_before && snapped_before->find_ancestor(ElementType::SYSTEM) == system) {
        IF_ASSERT_FAILED(!muse::contains(visited, snapped_before)) { break; }
        visited.push_back(snapped_before);
        func(snapped_before);
        snapped_before = snapped_before->ldata()->item_snapped_before();
    }

    EngravingItem *snapped_after = item_snapped_after_;
    while (snapped_after && snapped_after->find_ancestor(ElementType::SYSTEM) == system) {
        IF_ASSERT_FAILED(!muse::contains(visited, snapped_after)) { break; }
        visited.push_back(snapped_after);
        func(snapped_after);
        snapped_after = snapped_after->ldata()->item_snapped_after();
    }
}

const RectF &EngravingItem::LayoutData::bbox([[maybe_unused]] LdAccess mode) const {
    //! NOTE Temporary disabled CHECK - a lot of messages
    const Shape &sh = shape_.value(LdAccess::MAYBE_NOTINITED);

    //! NOTE Temporary
    {
        static const RectF dummy_value;

        switch (item_->type()) {
            case ElementType::NOTE:
            case ElementType::ORNAMENT: return !sh.elements().empty() ? sh.elements().at(0) : dummy_value;
            default: break;
        }
    }

    return sh.bbox();
}

Shape EngravingItem::LayoutData::shape(LdAccess mode) const {
    //! NOTE Temporary disabled CHECK - a lot of messages
    Shape sh = shape_.value(LdAccess::MAYBE_NOTINITED);

    const Parenthesis *left_paren = item_->left_paren();
    if (left_paren && left_paren->add_to_skyline()) {
        sh.add(left_paren->ldata()->shape().translated(left_paren->pos()));
    }
    const Parenthesis *right_paren = item_->right_paren();
    if (right_paren && right_paren->add_to_skyline()) {
        sh.add(right_paren->ldata()->shape().translated(right_paren->pos()));
    }

    //! NOTE Temporary
    //! Reimplementation: done
    {
        switch (item_->type()) {
            case ElementType::BEAM:
            case ElementType::GRACE_NOTES_GROUP:
            case ElementType::ORNAMENT:
            case ElementType::OTTAVA_SEGMENT:
            case ElementType::VOLTA_SEGMENT:
            case ElementType::PEDAL_SEGMENT:
            case ElementType::TEXTLINE_SEGMENT:
            case ElementType::HARMONIC_MARK_SEGMENT:
            case ElementType::PALM_MUTE_SEGMENT:
            case ElementType::LET_RING_SEGMENT:
            case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
            case ElementType::RASGUEADO_SEGMENT:
            case ElementType::WHAMMY_BAR_SEGMENT:
            case ElementType::SLUR_SEGMENT:
            case ElementType::TIE_SEGMENT:
            case ElementType::LAISSEZ_VIB_SEGMENT:
            case ElementType::PARTIAL_TIE_SEGMENT:
            case ElementType::NOTE: return sh;
            case ElementType::CHORD:
            case ElementType::REST:
            case ElementType::MEASURE_REPEAT:
            case ElementType::MMREST: {
                if (mode == LdAccess::CHECK) {
                    //! NOTE Temporary fix
                    //! We can remove it the moment we figure out the layout order of the elements
                    LayoutContext ctx(item_->score());
                    ChordRest::LayoutData *ldata = static_cast<ChordRest::LayoutData *>(const_cast<LayoutData *>(this));
                    ChordLayout::check_and_fill_shape(to_chord_rest(item_), ldata, ctx.conf());
                }
                return shape_.value(LdAccess::CHECK);
            } break;
            case ElementType::GUITAR_BEND_SEGMENT: {
                //! NOTE Temporary fix
                //! We can remove it the moment we figure out the layout order of the elements
                TLayout::fill_guitar_bend_segment_shape(to_guitar_bend_segment(item_),
                                                        static_cast<GuitarBendSegment::LayoutData *>(const_cast<LayoutData *>(this)));
                return shape_.value(LdAccess::CHECK);
            } break;
            case ElementType::HAIRPIN_SEGMENT: {
                //! To be removed when we're confident enough...
                IF_ASSERT_FAILED(shape_.has_value()) {
                    const_cast<LayoutData *>(this)->set_shape(TLayout::recalculate_text_line_base_segment_shape(to_hairpin_segment(item_)));
                }
                return shape_.value(LdAccess::CHECK);
            } break;
            case ElementType::TRILL_SEGMENT: {
                //! NOTE Temporary fix
                //! We can remove it the moment we figure out the layout order of the elements
                LayoutContext ctx(item_->score());
                TLayout::fill_trill_segment_shape(to_trill_segment(item_),
                                                  static_cast<HairpinSegment::LayoutData *>(const_cast<LayoutData *>(this)), ctx.conf());
                return shape_.value(LdAccess::CHECK);
            } break;
            case ElementType::TUPLET: {
                //! NOTE Temporary fix
                //! We can remove it the moment we figure out the layout order of the elements
                TLayout::fill_tuplet_shape(to_tuplet(item_), static_cast<Tuplet::LayoutData *>(const_cast<LayoutData *>(this)));
                return shape_.value(LdAccess::CHECK);
            } break;
            default: break;
        }
    }

    return sh;
}

#ifndef NDEBUG
void EngravingItem::LayoutData::do_set_pos_debug_hook([[maybe_unused]] double x, [[maybe_unused]] double y) {}

void EngravingItem::LayoutData::set_width_debug_hook([[maybe_unused]] double w) {}

#endif

void EngravingItem::LayoutData::dump(std::stringstream &ss) const {
    ss << "\n";
    ss << item_->type_name() << " id: " << item_->eid().to_std_string() << "\n";

    ss << "skip: " << (is_skip_draw_ ? "yes" : "no") << "\n";
    ss << "mag: " << mag_ << "\n";

    ss << "pos: ";
    mu::engraving::dump(pos_, ss);
    ss << "\n";

    ss << "shape: ";
    mu::engraving::dump(shape_, ss);
    ss << "\n";

    sup_dump(ss);
}

double EngravingItem::mag() const {
    if (!ldata()) {
        //LOGD() << "no layout data, will be returned default (1.0)";
        return 1.0;
    }
    return ldata()->mag();
}

PointF EngravingItem::staff_offset() const {
    const StaffType *st = staff_type();
    const double y_offset = st ? st->yoffset().val() * spatium() : 0.0;
    return PointF(0.0, y_offset);
}
} // namespace mu::engraving
