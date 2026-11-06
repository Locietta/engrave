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

/**
 \file
 Definition of Score class.
*/

#include <memory>
#include <optional>
#include <set>

#include "../automation/automationtypes_fwd.h"
#include "../editing/cmd.h"
#include "../engravingservices.h"
#include "../rendering/layoutoptions.h"
#include "../rendering/paddingtable.h"
#include "../style/pagestyle.h"
#include "../style/style.h"
#include "../types/constants.h"
#include "chordlist.h"
#include "dummyparent.h"
#include "global/types/ret.h"
#include "guitarbend.h"
#include "input.h"
#include "mscore.h"
#include "property.h"
#include "rangelock.h"
#include "scoreorder.h"
#include "segment.h"
#include "select.h"
#include "spannermap.h"
#include "synthesizerstate.h"
#include "tuplet.h"

namespace mu::engraving {
struct IMimeData;
} // namespace mu::engraving

namespace mu::engraving::read114 {
struct Read114;
} // namespace mu::engraving::read114

namespace mu::engraving::read302 {
struct Read302;
} // namespace mu::engraving::read302

namespace mu::engraving::read400 {
struct Read400;
} // namespace mu::engraving::read400

namespace mu::engraving::read410 {
struct Read410;
} // namespace mu::engraving::read410

namespace mu::engraving::read460 {
struct Read460;
} // namespace mu::engraving::read460

namespace mu::engraving::read500 {
struct Read500;
} // namespace mu::engraving::read500

namespace mu::engraving::write {
struct Writer;
} // namespace mu::engraving::write

namespace mu::engraving::compat {
struct WriteScoreHook;
} // namespace mu::engraving::compat

namespace mu::engraving {
struct Articulation;
struct Box;
struct Bracket;
struct Chord;
struct ChordRest;
struct Element;
struct EventsHolder;
struct Excerpt;
struct FiguredBass;
struct Harmony;
struct InstrumentTemplate;
struct InputState;
struct KeyList;
struct KeySigEvent;
struct LinkedObjects;
struct Lyrics;
struct MasterScore;
struct Measure;
struct MeasureBase;
struct Note;
struct Page;
struct Part;
struct RehearsalMark;
struct RepeatList;
struct RepeatSegmentInfo;
struct Rest;
struct RootItem;
struct Score;
struct IEngravingFont;
struct Segment;
struct Slur;
struct Spanner;
struct Staff;
struct System;
struct TDuration;
struct TempoTimeline;
struct Text;
struct TimeSig;
struct TimeSigMap;
struct Transaction;
struct TransactionManager;
struct Tuplet;
struct UndoableCommand;
struct UndoStack;

struct ShadowNote;

struct Interval;
struct NoteVal;
struct ShowAnchors;

enum class BeatType : unsigned char;
enum class Key : signed char;
enum class SegmentType;
enum class OttavaType : unsigned char;
enum class Prefer : char;
enum class AccidentalType : unsigned char;
enum class LayoutBreakType : unsigned char;
enum class CommandType : signed char;
enum class SystemDividerType : unsigned char;

struct Position {
    Segment *segment = nullptr;
    staff_idx_t staff_idx = muse::nidx;
    int line = 0;
    int fret = ::mu::engraving::invalid_fret_index;
    PointF pos;
    bool beyond_score = false;
};

struct ShowAnchors {
    ShowAnchors() = default;

    ShowAnchors(voice_idx_t v_idx, staff_idx_t stf_idx, const Fraction &s_tick_main, const Fraction &e_tick_main,
                const Fraction &s_tick_ext, const Fraction &e_tick_ext)
        : voice_idx(v_idx), staff_idx(stf_idx), end_staff_idx(stf_idx + 1), start_tick_main_region(s_tick_main),
          end_tick_main_region(e_tick_main), start_tick_extended_region(s_tick_ext), end_tick_extended_region(e_tick_ext) {}

    ShowAnchors(voice_idx_t v_idx, staff_idx_t stf_idx, staff_idx_t end_stf_idx, const Fraction &s_tick_main, const Fraction &e_tick_main,
                const Fraction &s_tick_ext, const Fraction &e_tick_ext)
        : voice_idx(v_idx), staff_idx(stf_idx), end_staff_idx(end_stf_idx), start_tick_main_region(s_tick_main),
          end_tick_main_region(e_tick_main), start_tick_extended_region(s_tick_ext), end_tick_extended_region(e_tick_ext) {}

    void reset() {
        voice_idx = muse::nidx;
        staff_idx = muse::nidx;
        start_tick_main_region = Fraction(-1, 1);
        end_tick_main_region = Fraction(-1, 1);
        start_tick_extended_region = Fraction(-1, 1);
        end_tick_extended_region = Fraction(-1, 1);
    }

    bool operator==(const ShowAnchors &other) const {
        return voice_idx == other.voice_idx && staff_idx == other.staff_idx && end_staff_idx == other.end_staff_idx &&
               start_tick_main_region == other.start_tick_main_region && end_tick_main_region == other.end_tick_main_region &&
               start_tick_extended_region == other.start_tick_extended_region && end_tick_extended_region == other.end_tick_extended_region;
    }

    bool operator!=(const ShowAnchors &other) const { return !(*this == other); }

    voice_idx_t voice_idx = muse::nidx;
    staff_idx_t staff_idx = muse::nidx;
    staff_idx_t end_staff_idx = muse::nidx;
    Fraction start_tick_main_region = Fraction(-1, 1);
    Fraction end_tick_main_region = Fraction(-1, 1);
    Fraction start_tick_extended_region = Fraction(-1, 1);
    Fraction end_tick_extended_region = Fraction(-1, 1);
};

struct ScoreChanges {
    int tick_from = -1;
    int tick_to = -1;
    staff_idx_t staff_idx_from = muse::nidx;
    staff_idx_t staff_idx_to = muse::nidx;

    bool is_text_editing = false;

    std::map<EngravingObject *, std::unordered_set<CommandType>> changed_objects;
    ElementTypeSet changed_types;
    PropertyIdSet changed_property_id_set;
    StyleIdSet changed_style_id_set;

    bool is_valid_boundary() const {
        bool tick_range_valid = (tick_from != -1 && tick_to != -1);
        bool staff_range_valid = (staff_idx_from != muse::nidx && staff_idx_to != muse::nidx);

        return tick_range_valid && staff_range_valid;
    }

    bool is_valid() const { return is_valid_boundary() || !changed_types.empty(); }

    void clear() { *this = ScoreChanges(); }
};

// A cache that follows the committed changes of a master score (the playback
// model). MasterScore calls it synchronously, in registration order, when a
// transaction commits or an undo/redo completes.
struct IScoreChangesListener {
    virtual ~IScoreChangesListener() = default;
    virtual void on_score_changes(const ScoreChanges &changes) = 0;
};

//---------------------------------------------------------------------------------------
//   @@ Score
//   @P composer        string            composer of the score (read only)
//   @P duration        int               duration of score in seconds (read only)
//   @P excerpts        array[Excerpt]    the list of the excerpts (linked parts)
//   @P firstMeasure    Measure           the first measure of the score (read only)
//   @P firstMeasureMM  Measure           the first multi-measure rest measure of the score (read only)
//   @P harmonyCount    int               number of harmony items (read only)
//   @P hasHarmonies    bool              score has chord symbols (read only)
//   @P hasLyrics       bool              score has lyrics (read only)
//   @P keysig          int               key signature at the start of the score (read only)
//   @P lastMeasure     Measure           the last measure of the score (read only)
//   @P lastMeasureMM   Measure           the last multi-measure rest measure of the score (read only)
//   @P lastSegment     Segment           the last score segment (read-only)
//   @P lyricCount      int               number of lyric items (read only)
//   @P name            string            name of the score
//   @P nmeasures       int               number of measures (read only)
//   @P npages          int               number of pages (read only)
//   @P nstaves         int               number of staves (read only)
//   @P ntracks         int               number of tracks (staves * 4) (read only)
// not to be documented?
//   @P parts           array[Part]       the list of parts (read only)
//
//    a Score has always an associated MasterScore
//---------------------------------------------------------------------------------------

struct Score : EngravingObject {
private:
    OBJECT_ALLOCATOR(engraving, Score)
    DECLARE_CLASSOF(ElementType::SCORE)

public:
    const EngravingServices &services() const { return services_; }

    const std::shared_ptr<IEngravingConfiguration> &configuration() const { return services_.configuration; }

    const std::shared_ptr<IEngravingFontsProvider> &engraving_fonts() const { return services_.engraving_fonts; }

    const std::shared_ptr<IEngravingContextConfiguration> &context_configuration() const { return services_.context_configuration; }

    const std::shared_ptr<rendering::IScoreRenderer> &renderer() const { return services_.renderer; }

    Score(const Score &) = delete;
    Score &operator=(const Score &) = delete;
    virtual ~Score();
    Score *clone();

    virtual bool is_master() const { return false; }

    virtual bool read_only() const;

    static void on_element_destruction(EngravingItem *se);
    // Whether the score has not started destructing: code that reaches a score
    // through a pointer it does not own checks this before touching it.
    static bool is_alive(const Score *score);

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    RootItem *root_item() const { return root_item_; }

    DummyParent *dummy() const { return dummy_; }

    ShadowNote *shadow_note() const;

    void rebuild_bsp_tree();

    bool no_staves() const { return staves_.empty(); }

    void insert_part(Part *, size_t target_part_idx);
    void append_part(Part *);
    void remove_part(Part *);
    void insert_staff(Staff *, staff_idx_t);
    void append_staff(Staff *);
    void cmd_remove_staff(staff_idx_t staff_idx);
    void remove_staff(Staff *);
    void add_measure(MeasureBase *, MeasureBase *);

    Excerpt *excerpt() { return excerpt_; }

    void set_excerpt(Excerpt *e) { excerpt_ = e; }

    void cmd_remove_part(Part *);
    void cmd_add_ottava(OttavaType);
    void add_note_line();

    void cmd_add_grace(NoteType, int);
    void cmd_toggle_layout_break(LayoutBreakType);
    void remove_chord_rest(ChordRest *cr, bool clear_segment);
    void cmd_toggle_mmrest();
    void cmd_toggle_hide_empty();
    void cmd_set_hide_staff_if_empty_override(staff_idx_t staff_idx, System *system, engraving::AutoOnOff value);
    void cmd_move_rest(Rest *, DirectionV);
    void cmd_move_lyrics(Lyrics *, DirectionV);

    bool append_measures_from_score(Score *score, const Fraction &start_tick, const Fraction &end_tick);
    bool append_score(Score *, bool add_page_break = false, bool add_section_break = true);

    const std::vector<Staff *> &staves() const { return staves_; }

    size_t nstaves() const { return staves_.size(); }

    size_t visible_staves_count() const;
    bool all_staves_invisible() const;

    size_t ntracks() const { return staves_.size() * ::mu::engraving::voices; }

    staff_idx_t staff_idx(const Staff *) const;
    staff_idx_t staff_idx(const Part *) const;

    Staff *staff(size_t n) const { return (n < staves_.size()) ? staves_.at(n) : nullptr; }

    Staff *staff_by_id(const muse::ID &staff_id) const;
    Part *part_by_id(const muse::ID &part_id) const;

    void clear_system_object_staves();
    void add_system_object_staff(Staff *staff);
    void remove_system_object_staff(Staff *staff);

    const std::vector<Staff *> &system_object_staves() const { return system_object_staves_; }

    const std::vector<Staff *> system_object_staves_with_top_staff() const;

    MeasureBase *pos2measure_base(const PointF &, bool scan_measures_only, staff_idx_t *staff_idx, int *pitch, Segment **,
                                  PointF *offset) const;
    Measure *pos2measure(const PointF &p, staff_idx_t *staff_idx, int *pitch, Segment **seg, PointF *offset) const;
    void drag_position(const PointF &, staff_idx_t *staff_idx, Segment **, double spacing_factor = 0.5,
                       bool allow_time_anchor = false) const;

    void undo_add_element(EngravingItem *element, bool add_to_linked_staves = true, bool ctrl_modifier = false,
                          EngravingItem *element_to_relink = nullptr);
    void undo_add_cr(ChordRest *element, Measure *, const Fraction &tick);
    void undo_remove_element(EngravingItem *element, bool remove_linked = true);
    void undo_remove_hopo_text(HammerOnPullOffText *hopo_text);
    void undo_change_spanner_elements(Spanner *spanner, EngravingItem *start_element, EngravingItem *end_element);
    void undo_change_element(EngravingItem *old_element, EngravingItem *new_element);
    void undo_change_chord_rest_len(ChordRest *cr, const TDuration &);
    void undo_remove_part(Part *part, size_t part_idx = muse::nidx);
    void undo_insert_part(Part *part, size_t target_part_index);
    void undo_remove_staff(Staff *staff);
    void undo_insert_staff(Staff *staff, staff_idx_t idx, bool create_rests = true);
    bool undo_property_changed(EngravingItem *item, Pid prop_id, const PropertyValue &prop_value,
                               PropertyFlags prop_flags = PropertyFlags::NOSTYLE);
    void undo_property_changed(EngravingObject *, Pid, const PropertyValue &v, PropertyFlags ps = PropertyFlags::NOSTYLE);
    UndoStack *undo_stack() const;
    TransactionManager *transaction_manager() const;
    void undo(UndoableCommand *) const;
    void undo_remove_measures(Measure *, Measure *, bool preserve_ties = false, bool move_staff_type_changes = true);
    void undo_change_measure_repeat_count(Measure *m, int count, staff_idx_t staff_idx);
    void undo_insert_time(const Fraction &tick, const Fraction &len);
    void undo_change_style_val(Sid idx, const PropertyValue &v);
    void undo_change_style_values(std::unordered_map<Sid, PropertyValue> values);
    void undo_change_page_number_offset(int po);
    void undo_change_parent(EngravingItem *element, EngravingItem *parent, staff_idx_t input_staff, bool change_links_parents = true);
    void undo_reset_play_count_text_settings(BarLine *bl);
    void undo_update_play_count_text(Measure *m);
    void undo_change_bar_line_type(BarLine *bl, BarLineType bar_type, bool all_staves, bool replace = false);

    void update_instrument_change_transpositions(KeySigEvent &key, Staff *staff, const Fraction &tick);

    Measure *undo_get_measure(const Fraction &tick);

    Note *set_grace_note(Chord *, int pitch, NoteType type, int len);

    GuitarBend *add_guitar_bend(GuitarBendType type, Note *note, Note *end_note = nullptr);

    // Musical insertion with explicit targets; no selection or input cursor changes.
    Segment *set_note_rest_at(Segment *, track_idx_t track, NoteVal nval, Fraction, DirectionV stem_direction = DirectionV::AUTO,
                              bool force_accidental = false, const std::set<SymId> &articulation_ids = {}, bool rhythmic = false,
                              EngravingItem **last_entered = nullptr);
    Segment *set_note_rest(Segment *, track_idx_t track, NoteVal nval, Fraction, DirectionV stem_direction = DirectionV::AUTO,
                           bool force_accidental = false, const std::set<SymId> &articulation_ids = {}, bool rhythmic = false,
                           InputState *external_input_state = nullptr);
    void change_c_rlen(ChordRest *cr, const TDuration &);
    void change_c_rlen(ChordRest *cr, const Fraction &, bool fill_with_rest = true);
    void create_cr_sequence(const Fraction &f, ChordRest *cr, const Fraction &tick);

    Fraction make_gap(Segment *, track_idx_t track, const Fraction &, Tuplet *, bool keep_chord = false, bool delete_annotations = true);
    bool make_gap_voice(Segment *seg, track_idx_t track, Fraction len, const Fraction &tick, bool delete_annotations = true);

    Rest *add_rest(const Fraction &tick, track_idx_t track, TDuration, Tuplet *);
    Rest *add_rest(Segment *seg, track_idx_t track, TDuration d, Tuplet *);
    Chord *add_chord(const Fraction &tick, TDuration d, Chord *oc, bool gen_tie, Tuplet *tuplet);

    Tuplet *add_tuplet(ChordRest *destination_chord_rest, Fraction ratio, TupletNumberType number_type, TupletBracketType bracket_type);
    Tuplet *add_tuplet(ChordRest *destination_chord_rest, const Fraction &ratio, const TDuration &base_len, TupletNumberType number_type,
                       TupletBracketType bracket_type);
    bool can_add_tuplet(const ChordRest *chord_rest, const Fraction &ratio, const TDuration &base_len) const;

    ChordRest *add_clone(ChordRest *cr, const Fraction &tick, const TDuration &d);
    Rest *set_rest(const Fraction &tick, track_idx_t track, const Fraction &, bool use_dots, Tuplet *tuplet,
                   bool use_full_measure_rest = true);
    std::vector<Rest *> set_rests(const Fraction &tick, track_idx_t track, const Fraction &l, bool use_dots, Tuplet *tuplet,
                                  bool use_full_measure_rest = true);

    ChordRest *search_note(const Fraction &tick, track_idx_t track) const;

    // undo/redo ops
    void add_element(EngravingItem *);
    void do_undo_add_element(EngravingItem *);
    void remove_element(EngravingItem *);
    void do_undo_remove_element(EngravingItem *);

    Slur *add_slur(ChordRest *first_chord_rest, ChordRest *second_chord_rest, const Slur *slur_template);
    TextBase *add_text(TextStyleType type, EngravingItem *destination_element = nullptr);

    void delete_item(EngravingItem *);
    void delete_measures(MeasureBase *first_measure, MeasureBase *last_measure, bool preserve_ties = false);
    void restore_initial_key_sig_and_time_sig();
    void reconnect_slurs(MeasureBase *mb_start, MeasureBase *mb_last);
    void cmd_delete_selection();
    std::vector<ChordRest *> delete_range(Segment *seg_start, Segment *seg_end, track_idx_t track_start, track_idx_t track_end,
                                          const SelectionFilter &filter, bool selection_contains_multi_note_chords);
    void cmd_full_measure_rest();

    void start_cmd(const TranslatableString &action_name);                                                      // start undoable command
    void end_cmd(bool rollback = false, bool layout_all_parts = false, bool keep_rolled_back_elements = false); // end undoable command
    void update();
    void undo_redo(bool undo, EditData *);

    virtual void set_update_all();
    void set_layout_all(staff_idx_t staff = muse::nidx, const EngravingItem *e = nullptr);
    void set_layout(const Fraction &tick, staff_idx_t staff, const EngravingItem *e = nullptr);
    void set_layout(const Fraction &tick1, const Fraction &tick2, staff_idx_t staff1, staff_idx_t staff2, const EngravingItem *e = nullptr);
    virtual CmdState &cmd_state();
    virtual const CmdState &cmd_state() const;
    virtual void add_layout_flags(LayoutFlags);
    virtual void set_instruments_changed(bool);

    void cmd_toggle_autoplace(bool all);

    bool play_note() const { return update_state_.play_note; }

    void set_play_note(bool v) { update_state_.play_note = v; }

    bool play_chord() const { return update_state_.play_chord; }

    void set_play_chord(bool v) { update_state_.play_chord = v; }

    bool selection_empty() const { return selection_.staff_start() == selection_.staff_end(); }

    bool selection_changed() const { return update_state_.selection_changed; }

    void set_selection_changed(bool val) { update_state_.selection_changed = val; }

    const RectF &refresh_rect() const { return update_state_.refresh; }

    void add_refresh(const RectF &);

    void clear_refresh_rect() { update_state_.refresh = RectF(); }

    const std::vector<Part *> &parts() const;
    size_t visible_part_count() const;
    std::vector<Part *> visible_parts() const;
    std::vector<SharedPart *> shared_parts() const;
    bool has_shared_parts() const;

    using StaffAccepted = std::function<bool(const Staff &)>;
    std::set<staff_idx_t> staff_idx_set_from_range(const track_idx_t track_from, const track_idx_t track_to,
                                                   StaffAccepted staff_accepted = StaffAccepted()) const;

    void append_part(const InstrumentTemplate *);
    void sort_system_objects(std::vector<staff_idx_t> &dst);
    void sort_staves(std::vector<staff_idx_t> &dst);

    bool is_show_invisible() const { return show_invisible_; }

    bool show_unprintable() const { return show_unprintable_; }

    bool show_frames() const { return show_frames_; }

    bool show_pageborders() const { return show_pageborders_; }

    bool show_sound_flags() const { return show_sound_flags_; }

    bool mark_irregular_measures() const { return mark_irregular_measures_; }

    bool show_instrument_names() const { return show_instrument_names_; }

    void set_show_invisible(bool v);
    void set_show_unprintable(bool v);
    void set_show_frames(bool v);
    void set_show_pageborders(bool v);
    void set_show_sound_flags(bool v);
    void set_mark_irregular_measures(bool v);

    void set_show_instrument_names(bool v) { show_instrument_names_ = v; }

    void hide_anchors() { show_anchors_.reset(); }

    void set_show_anchors(const ShowAnchors &show_anchors);

    const ShowAnchors &show_anchors() const { return show_anchors_; }

    ChordRest *get_selected_chord_rest() const;
    std::set<ChordRest *> get_selected_chord_rests() const;
    void get_selected_start_end_chord_rests(ChordRest *&cr1, ChordRest *&cr2) const;

    void select(EngravingItem *item, SelectType = SelectType::SINGLE, staff_idx_t staff = 0);
    void select(const std::vector<EngravingItem *> &items, SelectType = SelectType::SINGLE, staff_idx_t staff = 0);
    void select_similar(EngravingItem *e, bool same_staff);
    void select_similar_in_range(EngravingItem *e);
    static void collect_match(ElementPattern *p, EngravingItem *e);
    static void collect_note_match(NotePattern *p, EngravingItem *e);
    void deselect(EngravingItem *obj);

    void deselect_all() { selection_.deselect_all(); }

    void update_selection() { selection_.update(); }

    EngravingItem *get_selected_element() const { return selection_.element(); }

    const Selection &selection() const { return selection_; }

    Selection &selection() { return selection_; }

    const SelectionFilter &selection_filter() const { return selection_filter_; }

    SelectionFilter &selection_filter() { return selection_filter_; }

    void set_selection(const Selection &s);

    bool can_reselect_item(const EngravingItem *item) const;

    Fraction pos();
    Measure *tick2measure(const Fraction &tick) const;
    Measure *tick2measure_mm(const Fraction &tick) const;
    MeasureBase *tick2measure_base(const Fraction &tick) const;
    Segment *tick2segment(const Fraction &tick, bool first, SegmentType st, bool use_m_mrest = false) const;
    Segment *tick2segment(const Fraction &tick) const;
    Segment *tick2segment(const Fraction &tick, bool first) const;
    Segment *tick2segment_mm(const Fraction &tick, bool first, SegmentType st) const;
    Segment *tick2segment_mm(const Fraction &tick) const;
    Segment *tick2segment_mm(const Fraction &tick, bool first) const;
    Segment *tick2left_segment(const Fraction &tick, bool use_m_mrest = false, SegmentType seg_type = SegmentType::CHORD_REST) const;
    Segment *tick2right_segment(const Fraction &tick, bool use_m_mrest = false, SegmentType seg_type = SegmentType::CHORD_REST) const;

    Segment *tick2left_segment_mm(const Fraction &tick) { return tick2left_segment(tick, /* useMMRest */ true); }

    void update_ticks_and_time_sig_map();

    void update_ticks_and_time_sig_map_later() { need_update_ticks_and_time_sig_map_ = true; }

    bool need_update_ticks_and_time_sig_map() const { return need_update_ticks_and_time_sig_map_; }

    EngravingItem *next_element();
    EngravingItem *prev_element();
    ChordRest *next_track(ChordRest *cr, bool skip_measure_repeat_rests = true);
    ChordRest *prev_track(ChordRest *cr, bool skip_measure_repeat_rests = true);
    ChordRest *cmd_next_prev_system(ChordRest *, bool);
    Box *cmd_next_prev_frame(MeasureBase *, bool) const;
    EngravingItem *cmd_next_prev_section(EngravingItem *, bool) const;
    MeasureBase *get_next_prev_section_break(MeasureBase *, bool) const;
    EngravingItem *get_score_element_of_measure_base(MeasureBase *) const;

    int file_division(int t) const { return static_cast<int>(((int64_t) t * Constants::division + file_division_ / 2) / file_division_); }

    void set_file_division(int t) { file_division_ = t; }

    bool dirty() const;

    bool saved_capture() const { return saved_capture_; }

    void set_saved_capture(bool v) { saved_capture_ = v; }

    bool printing() const { return printing_; }

    void set_printing(bool val) { printing_ = val; }

    bool has_corrupted_measures() const { return corrupted_; }

    void set_has_corrupted_measures(bool val) { corrupted_ = val; }

    bool is_open() const;
    void set_is_open(bool open);

    ChordList *chord_list() { return &chord_list_; }

    const ChordList *chord_list() const { return &chord_list_; }

    void check_chord_list();

    MStyle &style() { return style_; }

    const MStyle &style() const { return style_; }

    PageSizeGetAccessor page_size() const { return PageSizeGetAccessor(style_); }

    PageSizeSetAccessor page_size() { return PageSizeSetAccessor(style_); }

    void reset_style_value(Sid style_to_reset);
    void reset_style_values(const StyleIdSet &style_id_set);

    void set_style(const MStyle &s, const bool overlap = false);

    TranslatableString get_text_style_user_name(TextStyleType tid) const;

    // These position are in ticks and not uticks
    Fraction loop_in_tick() const { return loop_boundary_tick(LoopBoundaryType::LOOP_IN); }

    Fraction loop_out_tick() const { return loop_boundary_tick(LoopBoundaryType::LOOP_OUT); }

    void set_loop_in_tick(const Fraction &tick) { set_loop_boundary_tick(LoopBoundaryType::LOOP_IN, tick); }

    void set_loop_out_tick(const Fraction &tick) { set_loop_boundary_tick(LoopBoundaryType::LOOP_OUT, tick); }

    Fraction loop_boundary_tick(LoopBoundaryType type) const;
    void set_loop_boundary_tick(LoopBoundaryType type, Fraction tick);

    bool note_entry_mode() const { return input_state().note_entry_mode(); }

    bool using_note_entry_method(NoteEntryMethod m) { return input_state().using_note_entry_method(m); }

    const InputState &input_state() const { return is_; }

    InputState &input_state() { return is_; }

    void set_input_state(const InputState &st) { is_ = st; }

    void spatium_changed(double old_value, double new_value);
    void style_changed() override;

    BeatType tick2beat_type(const Fraction &tick) const;

    int msc_version() const { return msc_version_; }

    void set_msc_version(int v) { msc_version_ = v; }

    void add_lyrics(const Fraction &tick, staff_idx_t staff_idx, const String &);

    void update_swing();

    void update_capo(bool ignore_notation_update = false);
    void update_channel();

    void cmd_concert_pitch_changed(bool);

    virtual TimeSigMap *sigmap() const;

    BeatsPerSecond tempo(const Fraction &tick) const;
    BeatsPerSecond multiplied_tempo(const Fraction &tick) const;
    BeatsPerSecond multiplied_tempo_at_utick(int utick) const;

    Text *get_text(TextStyleType subtype) const;

    ScoreOrder score_order() const;
    void set_score_order(ScoreOrder order);
    void update_braces_and_barlines(Part *part, size_t index);
    void set_brackets_and_barlines();
    void remap_brackets_and_barlines();

    void lasso_select(const RectF &);
    void lasso_select_end();

    Page *search_page(const PointF &) const;
    std::vector<System *> search_system(const PointF &p, bool include_measureless_systems = true, const System *preferred_system = nullptr,
                                        double spacing_factor = 0.5, double preferred_spacing_factor = 1.0) const;
    MeasureBase *search_measure_base(const PointF &p, bool scan_measures_only = false, const System *preferred_system = nullptr,
                                     double spacing_factor = 0.5, double preferred_spacing_factor = 1.0) const;

    bool get_position(Position *pos, const PointF &, voice_idx_t voice) const;

    void cmd_delete_tuplet(Tuplet *, bool replace_with_rest);

    void adjust_key_sigs(track_idx_t sidx, track_idx_t eidx, KeyList km);
    KeyList key_list() const;

    /// To be used together with setExpandRepeats.
    /// For bigger operations, where suboperations might also use it,
    /// where those need to have the same value for expandRepeats.
    const RepeatList &repeat_list() const;
    /// For small, one-step operations, where you need to get the relevant repeatList just once
    const RepeatList &repeat_list(bool expand_repeats, bool update_ties = true) const;
    /// Read-only snapshot, safe to call mid-edit (e.g. before a structural change invalidates the current repeatList)
    std::vector<RepeatSegmentInfo> repeat_segment_info_list(bool expand_repeats) const;
    /// Always the repeat-expanded list, regardless of the current playback setting
    const RepeatList &expanded_repeat_list() const;

    void invalidate_repeat_list();

    double utick2utime(int tick) const;
    int utime2utick(double utime) const;

    const TempoTimeline &tempo_timeline() const;
    const TempoTimeline &tempo_timeline(bool expand_repeats) const;
    void set_tempo_timeline_override(std::optional<TempoTimeline> timeline);

    virtual size_t npages() const { return pages_.size(); }

    virtual page_idx_t page_idx(const Page *page) const { return muse::index_of(pages_, page); }

    virtual const std::vector<Page *> &pages() const { return pages_; }

    virtual std::vector<Page *> &pages() { return pages_; }

    const std::vector<System *> &systems() const { return systems_; }

    std::vector<System *> &systems() { return systems_; }

    MeasureBaseList *measures() { return &measures_; }

    bool check_has_measures() const;
    MeasureBase *first() const;
    MeasureBase *first_mm() const;
    MeasureBase *last() const;
    Measure *first_measure() const;
    Measure *first_measure_mm() const;
    Measure *last_measure() const;
    Measure *last_measure_mm() const;
    MeasureBase *measure(int idx) const;
    Measure *cr_measure(int idx) const;

    Fraction end_tick() const;

    Segment *first_segment(SegmentType s) const;
    Segment *first_segment_mm(SegmentType s) const;
    Segment *last_segment() const;
    Segment *last_segment_mm() const;

    void connect_ties(bool silent = false);
    void undo_remove_stale_tie_jump_points(bool undo = true);

    void scan_elements_in_range(std::function<void(EngravingItem *)> func);

    int file_division() const { return file_division_; } ///< division of current loading *.msc file

    void split_staff(staff_idx_t staff_idx, int split_point);
    FiguredBass *add_figured_bass();
    void expand_voice(Segment *s, track_idx_t track);
    void expand_voice();

    void cmd_enter_rest(const TDuration &d);
    void enter_rest(const TDuration &d, InputState *external_input_state = nullptr);
    void add_interval(int, const std::vector<Note *> &);
    void cmd_create_tuplet(ChordRest *, Tuplet *);

    bool auto_layout_enabled() const;

    void do_layout();
    void do_layout_range(const Fraction &st, const Fraction &et);

    SynthesizerState &synthesizer_state() { return synthesizer_state_; }

    void set_synthesizer_state(const SynthesizerState &s);

    MasterScore *master_score() const { return master_score_; }

    void set_master_score(MasterScore *s) { master_score_ = s; }

    const std::map<String, String> &meta_tags() const { return meta_tags_; }

    std::map<String, String> &meta_tags() { return meta_tags_; }

    void set_meta_tags(const std::map<String, String> &t) { meta_tags_ = t; }

    String meta_tag(const String &tag) const;
    void set_meta_tag(const String &tag, const String &val);

    int page_number_offset() const { return page_number_offset_; }

    void set_page_number_offset(int v) { page_number_offset_ = v; }

    String mscore_version() const { return mscore_version_; }

    int mscore_revision() const { return mscore_revision_; }

    void set_mscore_version(const String &val) { mscore_version_ = val; }

    void set_mscore_revision(int val) { mscore_revision_ = val; }

    //! NOTE Layout
    const LayoutOptions &layout_options() const { return layout_options_; }

    void set_layout_mode(LayoutMode lm) { layout_options_.mode = lm; }

    void set_show_v_box(bool v) { layout_options_.is_show_v_box = v; }

    double note_head_width() const { return layout_options_.note_head_width; }

    void set_note_head_width(double n) { layout_options_.note_head_width = n; }

    // temporary methods
    bool is_layout_mode(LayoutMode lm) const { return layout_options_.is_mode(lm); }

    LayoutMode layout_mode() const { return layout_options_.mode; }

    bool line_mode() const { return layout_options_.is_mode(LayoutMode::LINE); }

    bool linear_mode() const { return layout_options_.is_linear_mode(); }

    // ----

    void cmd_select_all();
    void cmd_select_section();

    struct InsertMeasureOptions {
        InsertMeasureOptions() {}

        bool create_measure_rests = false;
        bool move_signatures_clef = true;
        bool need_deselect_all = true;
        bool clone_box_to_all_parts = true;
        bool move_staff_type_changes = true;
        bool ignore_bar_lines = false;
    };

    MeasureBase *insert_measure(ElementType type, MeasureBase *before_measure = nullptr,
                                const InsertMeasureOptions &options = InsertMeasureOptions());
    MeasureBase *insert_box(ElementType type, MeasureBase *before_measure = nullptr,
                            const InsertMeasureOptions &options = InsertMeasureOptions());
    MeasureBase *insert_box(MeasureBase *box, MeasureBase *before_measure = nullptr,
                            const InsertMeasureOptions &options = InsertMeasureOptions());

    std::list<Score *> score_list();

    void append_measures(int num_measures);

    const std::multimap<int, Spanner *> &spanner() const { return spanner_.map(); }

    SpannerMap &spanner_map() { return spanner_; }

    const SpannerMap &spanner_map() const { return spanner_; }

    std::vector<Spanner *> spanner_list() const; // Return all spanners as a vector for Plugin API
    bool is_spanner_start_end(const Fraction &tick, track_idx_t track) const;
    void remove_spanner(Spanner *);
    void add_spanner(Spanner *, bool compute_start_end = true);
    void check_spanner(const Fraction &start_tick, const Fraction &last_tick, bool remove_orphans = true);

    const std::set<Spanner *> &unmanaged_spanners() const { return unmanaged_spanner_; }

    void add_unmanaged_spanner(Spanner *);
    void remove_unmanaged_spanner(Spanner *);

    ChordRest *find_cr(Fraction tick, track_idx_t track) const;
    ChordRest *find_chord_rest_ending_before_tick_in_staff(const Fraction &tick, staff_idx_t staff_idx) const;
    ChordRest *find_chord_rest_ending_before_tick_in_staff_and_voice(const Fraction &tick, staff_idx_t staff_idx, voice_idx_t voice) const;
    ChordRest *find_chord_rest_ending_before_tick_in_track(const Fraction &tick, track_idx_t track_idx) const;
    void insert_time(const Fraction &tick_pos, const Fraction &tick_len, const std::vector<RepeatSegmentInfo> &old_segments);

    std::shared_ptr<IEngravingFont> engraving_font() const { return engraving_font_; }

    void set_engraving_font(std::shared_ptr<IEngravingFont> f) { engraving_font_ = f; }

    std::vector<staff_idx_t> unique_staves() const;

    size_t nmeasures() const;

    bool has_harmonies() const;
    int harmony_count() const;

    bool has_lyrics() const;
    int lyric_count() const;
    std::vector<Lyrics *> lyrics() const;
    String extract_lyrics() const;

    int keysig() const;
    int duration() const;
    int duration_without_repeats() const;

    void cmd_remove_empty_trailing_measures();

    Measure *first_trailing_measure(ChordRest **cr = nullptr);

    muse::Ret sanity_check_local();

    bool check_keys();

    void switch_to_page_mode();

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    /// For MasterScores: returns the filename without extension
    /// For Scores: returns the excerpt name
    virtual String name() const;

    void cmd_time_delete();
    bool check_time_delete(Segment *start_segment, Segment *end_segment);
    void do_time_delete(Segment *start_segment, Segment *end_segment);
    void do_time_delete_for_measure(Measure *, Segment *, const Fraction &);

    void for_all_lyrics(std::function<void(Lyrics *)> f);

    void update_padding_tables();

    const PaddingTables &padding_tables() const { return padding_tables_; }

    const RangeLocks *system_locks() const { return &system_locks_; }

    void add_system_lock(const RangeLock *lock);
    void remove_system_lock(const RangeLock *lock);

    void clear_system_locks() { system_locks_.clear(); }

    const RangeLocks *page_locks() const { return &page_locks_; }

    void add_page_lock(const RangeLock *lock);
    void remove_page_lock(const RangeLock *lock);

    void rebuild_fret_box();

    const std::map<size_t, std::array<SystemDivider *, 2>> system_dividers() const { return system_dividers_; }

    SystemDivider *system_divider(size_t system_idx, SystemDividerType type) const;
    void add_system_divider(size_t system_idx, SystemDivider *divider);

    virtual AutomationDataConstPtr automation_data() const;
    virtual void edit_automation_points(const AutomationCurveKey &key, AutomationPointEdits &edits, bool undoable = true);

    friend struct Chord;

    BracketType bracket_type(staff_idx_t staff_idx, size_t idx) const;
    size_t bracket_span(staff_idx_t staff_idx, size_t idx) const;
    const std::vector<BracketItem *> &brackets(staff_idx_t staff_idx) const;
    std::vector<BracketItem *> &brackets(staff_idx_t staff_idx);
    size_t bracket_levels(staff_idx_t staff_idx) const;

protected:
    virtual void on_time_inserted(const Fraction &tick, const Fraction &len, const std::vector<RepeatSegmentInfo> &old_segments);

    friend struct MasterScore;
    explicit Score(const EngravingServices &services);
    Score(MasterScore *, bool force_part_style = true);
    Score(MasterScore *, const MStyle &);

    int file_division_ = 0; // division of current loading *.msc file
    SynthesizerState synthesizer_state_;

private:

    friend struct read302::Read302;
    friend struct read400::Read400;
    friend struct read410::Read410;
    friend struct read460::Read460;
    friend struct read500::Read500;
    friend struct write::Writer;

    static std::set<Score *> valid_scores;

    Note *get_selected_note();
    ChordRest *find_chord_rest_ending_before_tick_in_staff_and_voice(const Fraction &tick, staff_idx_t staff_idx, bool force_voice,
                                                                     voice_idx_t voice) const;

    void check_score();

    std::vector<Fraction> split_gap_to_measure_boundaries(ChordRest *, Fraction);

    void do_select(EngravingItem *e, SelectType type, staff_idx_t staff_idx);
    void select_single(EngravingItem *e, staff_idx_t staff_idx);
    void select_add(EngravingItem *e);
    void select_range(EngravingItem *e, staff_idx_t staff_idx);

    bool try_select_similar_in_range(EngravingItem *e);
    bool try_extend_single_selection_to_range(EngravingItem *e, staff_idx_t staff_idx);
    bool try_extend_range_selection_to_elem(EngravingItem *e);

    void rebuild_time_sig_map(Measure *m);

    void do_undo_remove_stale_tie_jump_points(Tie *tie, bool undo = true);
    void do_undo_reset_partial_slur(Slur *slur, bool undo);

    void delete_or_shorten_out_spanners_from_range(const Fraction &t1, const Fraction &t2, track_idx_t track_start, track_idx_t track_end,
                                                   const SelectionFilter &filter);
    void delete_slurs_from_range(const Fraction &t1, const Fraction &t2, track_idx_t track_start, track_idx_t track_end,
                                 const SelectionFilter &filter);
    void delete_annotations_from_range(Segment *seg_start, Segment *seg_end, track_idx_t track_start, track_idx_t track_end,
                                       const SelectionFilter &filter);

    void delete_range_at_track(std::vector<ChordRest *> &crs_to_select, const track_idx_t track, Segment *start_seg,
                               const Fraction &end_tick, Tuplet *current_tuplet, const SelectionFilter &filter,
                               bool selection_contains_multi_note_chords);

    muse::ID new_staff_id() const;
    muse::ID new_part_id() const;

    void assign_id_if_need(Staff &staff) const;
    void assign_id_if_need(Part &part) const;

    void update_staves_number_for_systems();

    FBox *find_fret_box() const;

    const EngravingServices &services_;
    MasterScore *master_score_ = nullptr;
    Excerpt *excerpt_ = nullptr;

    String mscore_version_;
    int mscore_revision_ = 0;

    std::shared_ptr<IEngravingFont> engraving_font_ = nullptr;

    int page_number_offset_ = 0; // Offset for page numbers.

    UpdateState update_state_;

    MeasureBaseList measures_; // here are the notes
    std::vector<Part *> parts_;
    std::vector<Staff *> staves_;
    std::vector<Staff *> system_object_staves_;
    RangeLocks system_locks_;

    RangeLocks page_locks_;

    SpannerMap spanner_;
    std::set<Spanner *> unmanaged_spanner_;

    std::vector<std::vector<BracketItem *>> brackets_;

    //
    // objects generated by layout:
    //
    std::vector<Page *> pages_;     // pages are build from systems
    std::vector<System *> systems_; // measures are accumulated to systems

    std::map<size_t, std::array<SystemDivider *, 2>> system_dividers_; // list of system dividers (left and right) indexed by system

    InputState is_;
    MStyle style_;
    ChordList chord_list_;

    bool show_invisible_ = true;
    bool show_unprintable_ = true;
    bool show_frames_ = true;
    bool show_pageborders_ = false;
    bool show_sound_flags_ = true;
    bool mark_irregular_measures_ = true;
    bool show_instrument_names_ = true;
    bool printing_ = false;      // True if we are drawing to a printer
    bool saved_capture_ = false; // True if we saved an image capture
    bool corrupted_ = false;
    bool need_update_ticks_and_time_sig_map_ = false;

    ShowAnchors show_anchors_;

    ScoreOrder score_order_;                   // used for score ordering
    int msc_version_ = Constants::msc_version; // version of current loading *.msc file

    bool is_open_ = false;

    std::map<String, String> meta_tags_;

    Selection selection_;
    SelectionFilter selection_filter_;

    RootItem *root_item_ = nullptr;
    DummyParent *dummy_ = nullptr;
    LayoutOptions layout_options_;

    ShadowNote *shadow_note_ = nullptr;

    PaddingTables padding_tables_;
    double minimum_padding_unit_ = 0.0;
};

static inline Score *to_score(EngravingObject *e) {
    assert(!e || e->is_score());
    return static_cast<Score *>(e);
}

static inline const Score *to_score(const EngravingObject *e) {
    assert(!e || e->is_score());
    return static_cast<const Score *>(e);
}

//---------------------------------------------------------
//   ScoreLoad
//---------------------------------------------------------

struct ScoreLoad {
private:
    static int loading_;

public:
    ScoreLoad() { ++loading_; }

    ~ScoreLoad() { --loading_; }

    static bool loading() { return loading_ > 0; }
};

DECLARE_OPERATORS_FOR_FLAGS(LayoutFlags)
} // namespace mu::engraving
