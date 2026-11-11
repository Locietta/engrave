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

#include <functional>

#include "../types/types.h"
#include "durationelement.h"
#include "fermata.h"

namespace mu::engraving {
enum class CrossMeasure : signed char { UNKNOWN = -1, NONE = 0, FIRST, SECOND };

struct Articulation;
struct BeamBase;
struct BeamSegment;
struct Lyrics;
struct Measure;
struct Score;
struct Segment;
struct Slur;
struct TabDurationSymbol;
struct Transaction;
enum class SegmentType;

//-------------------------------------------------------------------
//   ChordRest
//    Virtual base class. Chords and rests can be part of a beam
//-------------------------------------------------------------------

struct ChordRest : DurationElement {
private:
    DECLARE_CLASSOF(ElementType::INVALID) // dummy

public:
    ChordRest(const ElementType &type, DummyParentOr<Segment> parent);
    ChordRest(const ChordRest &, bool link = false);
    ChordRest &operator=(const ChordRest &) = delete;
    ~ChordRest();

    virtual void scan_elements(std::function<void(EngravingItem *)> func) override;

    bool accept_drop(EditData &) const override;
    virtual EngravingItem *drop(Transaction &tx, EditData &) override;
    virtual void undo_unlink() override;

    virtual Segment *segment() const { return (Segment *) ownership_parent(); }

    void set_beam_mode(BeamMode m) { beam_mode_ = m; }

    BeamMode beam_mode() const { return beam_mode_; }

    void set_beam(Beam *b);
    void set_beamlet(BeamSegment *b);

    BeamSegment *beamlet() const { return beamlet_; }

    virtual Beam *beam() const final;

    int beams() const { return duration_type_.hooks(); }

    virtual double up_pos() const = 0;
    virtual double down_pos() const = 0;

    virtual double right_edge() const = 0;

    bool is_small() const { return is_small_; }

    void set_small(bool val) { is_small_ = val; }

    int staff_move() const { return staff_move_; }

    void set_staff_move(int val) { staff_move_ = val; }

    int stored_staff_move() const { return stored_staff_move_; }

    staff_idx_t v_staff_idx() const override { return staff_idx() + staff_move_; }

    void check_staff_move_validity();

    const TDuration duration_type() const { return cross_measure_ == CrossMeasure::FIRST ? cross_measure_t_dur_ : duration_type_; }

    const TDuration actual_duration_type() const { return duration_type_; }

    void set_duration_type(DurationType t);
    void set_duration_type(const Fraction &ticks);
    void set_duration_type(TDuration v);

    void set_dots(int n) { duration_type_.set_dots(n); }

    int dots() const {
        return cross_measure_ == CrossMeasure::FIRST ? cross_measure_t_dur_.dots() :
                                                       (cross_measure_ == CrossMeasure::SECOND ? 0 : duration_type_.dots());
    }

    int actual_dots() const { return duration_type_.dots(); }

    Fraction duration_type_ticks() const {
        return cross_measure_ == CrossMeasure::FIRST ? cross_measure_t_dur_.ticks() : duration_type_.ticks();
    }

    String duration_user_name() const;

    void set_track(track_idx_t val) override;

    const std::vector<Lyrics *> &lyrics() const { return lyrics_; }

    std::vector<Lyrics *> &lyrics() { return lyrics_; }

    Lyrics *lyrics(int verse) const;
    Lyrics *lyrics(int verse, PlacementV) const;
    int last_verse(PlacementV) const;
    bool is_melisma_end() const;
    void set_melisma_end(bool v);

    virtual void add(EngravingItem *) override;
    virtual void remove(EngravingItem *) override;
    void remove_delete_beam(bool beamed);
    void replace_beam(Beam *new_beam);

    const ElementList &el() const { return el_; }

    //! TODO Look like a hack, see using
    void add_fermata(Fermata *f) { add_el(f); }

    void remove_fermata(Fermata *f) { remove_el(f); }

    //! --------------------------------

    Slur *slur(const ChordRest *second_chord_rest = nullptr) const;

    CrossMeasure cross_measure() const { return cross_measure_; }

    void set_cross_measure(CrossMeasure val) { cross_measure_ = val; }

    // the following two functions should not be used, unless absolutely necessary;
    // the cross-measure duration is best managed through setDuration() and crossMeasureSetup()
    TDuration cross_measure_duration_type() const { return cross_measure_t_dur_; }

    void set_cross_measure_duration_type(TDuration v) { cross_measure_t_dur_ = v; }

    void undo_change_property(Pid id, const PropertyValue &new_value, PropertyFlags ps = PropertyFlags::NOSTYLE) override;
    void local_spatium_changed(double old_value, double new_value) override;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;
    bool is_grace() const;
    bool is_grace_before() const;
    bool is_grace_after() const;
    Breath *has_breath_mark() const;

    Segment *next_segment_after_cr(SegmentType types) const;

    void set_score(Score *s) override;
    EngravingItem *next_articulation_or_lyric(EngravingItem *e);
    EngravingItem *prev_articulation_or_lyric(EngravingItem *e);
    virtual EngravingItem *next_element() override;
    virtual EngravingItem *prev_element() override;
    EngravingItem *last_element_before_segment();
    virtual EngravingItem *next_segment_element() override;
    virtual EngravingItem *prev_segment_element() override;

    virtual void compute_up();

    bool is_full_measure_rest() const { return duration_type_ == DurationType::V_MEASURE; }

    virtual void remove_markings(bool keep_tremolo = false);

    bool is_before(const EngravingItem *) const override;

    virtual double intrinsic_mag() const = 0;

    TabDurationSymbol *tab_dur() const { return tab_dur_; }

    void set_tab_dur(TabDurationSymbol *s) { tab_dur_ = s; }

    bool is_below_cross_beam(const BeamBase *beam_base) const;

    bool has_following_jump_item() const;
    bool has_preceding_jump_item() const;

    struct LayoutData : public DurationElement::LayoutData {
        ld_field<bool> up = {"[ChordRest] up", true}; // actual stem direction
    };
    DECLARE_LAYOUTDATA_METHODS(ChordRest)

    //! DEPRECATED ------
    void set_up(bool val) { mutldata()->up = val; }

    bool up() const { return ldata()->up; }

    //! -----------------

protected:

    void add_el(EngravingItem *e) { el_.push_back(e); }

    bool remove_el(EngravingItem *e) { return el_.remove(e); }

    void clear_els() { el_.clear(); }

    std::vector<Lyrics *> lyrics_;
    TabDurationSymbol *tab_dur_ = nullptr; // stores a duration symbol in tablature staves

    Beam *beam_ = nullptr;
    BeamSegment *beamlet_ = nullptr;
    BeamMode beam_mode_ = BeamMode::INVALID;
    bool is_small_ = false;
    bool melisma_end_ = false;

    // CrossMeasure: combine 2 tied notes if across a bar line and can be combined in a single duration
    CrossMeasure cross_measure_ = CrossMeasure::UNKNOWN; ///< 0: no cross-measure modification; 1: 1st note of a mod.; -1: 2nd note
    TDuration cross_measure_t_dur_;                      ///< the total Duration type of the combined notes

private:

    void process_siblings(std::function<void(EngravingItem *)> func);

    ElementList el_;
    TDuration duration_type_;
    int staff_move_ = 0;        // -1, 0, +1, used for crossbeaming
    int stored_staff_move_ = 0; // used to remember and re-apply staff move if needed
};
} // namespace mu::engraving
