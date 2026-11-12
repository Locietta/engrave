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

#include "containers.h"
#include "engravingitem.h"
#include "interval.h"
#include "noteevent.h"
#include "noteval.h"
#include "pitchspelling.h"
#include "symbol.h"
#include "tie.h"
#include "tiejumppointlist.h"

namespace mu::engraving {
struct Accidental;
struct AccidentalState;
struct Chord;
struct Factory;
struct NoteDot;
struct Score;
struct Spanner;
struct StaffType;
struct Text;
struct Tie;
struct Transaction;
enum class AccidentalType : unsigned char;
enum class NoteType : unsigned char;
struct NoteParenthesisInfo;

static constexpr int max_dots = 4;

//--------------------------------------------------------------------------------
// LINE ATTACHMENT POINT
// Represents the attachment point of any line (tie, slur, glissando...)
// with respect to the note. Each note can hold a vector of line attach points, which
// it may use to make spacing decision with the surrounding items.
//--------------------------------------------------------------------------------
struct LineAttachPoint {
    LineAttachPoint(EngravingItem *l, double x, double y, bool start) : line_(l), pos_(PointF(x, y)), start_(start) {}

    const EngravingItem *line() const { return line_; }

    const PointF pos() const { return pos_; }

    bool start() const { return start_; }

private:
    EngravingItem *line_ = nullptr;
    PointF pos_ = PointF(0.0, 0.0);
    bool start_ = true;
};

//---------------------------------------------------------
//   @@ NoteHead
//---------------------------------------------------------

struct NoteHead final : Symbol {
private:
    DECLARE_CLASSOF(ElementType::NOTEHEAD)

public:
    NoteHead(DummyParentOr<Note> parent);
    NoteHead(const NoteHead &) = default;
    NoteHead &operator=(const NoteHead &) = delete;

    NoteHead *clone() const override { return new NoteHead(*this); }

    NoteHeadGroup head_group() const;
};

static const int invalid_line = -10000;

//---------------------------------------------------------------------------------------
//   @@ Note
///    Graphic representation of a note.
//
//   @P accidental       Accidental       note accidental (null if none)
//   @P accidentalType   int              note accidental type
//   @P dots             array[NoteDot]   list of note dots (some can be null, read only)
//   @P dotsCount        int              number of note dots (read only)
//   @P elements         array[EngravingItem]   list of elements attached to notehead
//   @P fret             int              fret number in tablature
//   @P ghost            bool             ghost note (guitar: death note)
//   @P headScheme       enum (NoteHeadScheme.HEAD_AUTO, .HEAD_NORMAL, .HEAD_PITCHNAME, .HEAD_PITCHNAME_NO_ACCIDENTALS, .HEAD_PITCHNAME_GERMAN, .HEAD_PITCHNAME_GERMAN_NO_ACCIDENTALS, .HEAD_SHAPE_NOTE_4, .HEAD_SHAPE_NOTE_7_AIKIN, .HEAD_SHAPE_NOTE_7_FUNK, .HEAD_SHAPE_NOTE_7_WALKER, .HEAD_SOLFEGE, .HEAD_SOLFEGE_FIXED)
//   @P headGroup        enum (NoteHeadGroup.HEAD_NORMAL, .HEAD_BREVIS_ALT, .HEAD_CROSS, .HEAD_DIAMOND, .HEAD_DO, .HEAD_FA, .HEAD_LA, .HEAD_MI, .HEAD_RE, .HEAD_SLASH, .HEAD_LARGE_DIAMOND, .HEAD_SOL, .HEAD_TI, .HEAD_XCIRCLE, .HEAD_TRIANGLE)
//   @P headType         enum (NoteHeadType.HEAD_AUTO, .HEAD_BREVIS, .HEAD_HALF, .HEAD_QUARTER, .HEAD_WHOLE)
//   @P hidden           bool             hidden, not played note (read only)
//   @P line             int              notehead position (read only)
//   @P mirror           bool             mirror notehead on x axis (read only)
//   @P pitch            int              midi pitch
//   @P play             bool             play note
//   @P ppitch           int              actual played midi pitch (honoring ottavas) (read only)
//   @P isSmall          bool             small notehead
//   @P string           int              string number in tablature
//   @P subchannel       int              midi subchannel (for midi articulation) (read only)
//   @P tieBack          Tie              note backward tie (null if none, read only)
//   @P tieFor           Tie              note forward tie (null if none, read only)
//   @P tpc              int              tonal pitch class, as per concert pitch setting
//   @P tpc1             int              tonal pitch class, non transposed
//   @P tpc2             int              tonal pitch class, transposed
//   @P tuning           float            tuning offset in cent
//   @P userDotPosition  enum (Direction.AUTO, Direction.DOWN, Direction.UP)
//   @P userMirror       enum (DirectionH.AUTO, DirectionH.LEFT, DirectionH.RIGHT)
//   @P veloOffset       int
//   @P veloType         enum (Note.OFFSET_VAL, Note.USER_VAL)
//---------------------------------------------------------------------------------------

struct Note final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::NOTE)

public:
    enum class SlideType : unsigned char { UNDEFINED = 0, UP_TO_NOTE, DOWN_TO_NOTE, UP_FROM_NOTE, DOWN_FROM_NOTE };

    enum DisplayFretOption : signed char { HIDE = -1, NO_HARMONIC, NATURAL_HARMONIC, ARTIFICIAL_HARMONIC };

    ~Note();

    std::vector<Note *> compound_notes() const;

    Note &operator=(const Note &) = delete;

    virtual Note *clone() const override { return new Note(*this, false); }

    Chord *chord() const { return (Chord *) ownership_parent(); }

    void set_ownership_parent(DummyParentOr<Chord> ch);

    void undo_unlink() override;

    double mag() const override;
    EngravingItem *element_base() const override;

    void scan_elements(std::function<void(EngravingItem *)> func) override;
    void set_track(track_idx_t val) override;

    staff_idx_t v_staff_idx() const override;

    int play_ticks() const;
    Fraction play_ticks_fraction() const;

    double head_width() const;
    double head_height() const;
    double tab_head_width(const StaffType *tab = 0) const;
    double tab_head_height(const StaffType *tab = 0) const;
    PointF stem_down_nw() const;
    PointF stem_up_se() const;
    double bbox_x_shift() const;
    double notehead_center_x() const;
    double bbox_right_pos() const;
    double head_body_width() const;
    double outside_tie_attach_x(bool up) const;

    NoteHeadScheme head_scheme() const { return head_scheme_; }

    void update_head_group(const NoteHeadGroup head_group);

    NoteHeadGroup head_group() const { return head_group_; }

    NoteHeadType head_type() const { return head_type_; }

    void set_head_scheme(NoteHeadScheme val);
    void set_head_group(NoteHeadGroup val);
    void set_head_type(NoteHeadType t);

    int subtype() const override { return int(head_group_); }

    TranslatableString subtype_user_name() const override;

    void set_pitch(int val, bool notify_about_changed = true);
    void set_pitch(int pitch, int tpc1, int tpc2);

    int pitch() const { return pitch_; }

    double cent_offset() const { return cent_offset_; }

    void set_cent_offset(double v) { cent_offset_ = v; }

    int quarter_tone_offset() const { return std::round(cent_offset_ / 50); }

    int ottave_capo_fret() const;
    int linked_ottava_pitch_offset() const;
    int ppitch() const; // playback pitch
    int epitch() const; // effective pitch
    int octave() const;
    int playing_octave() const;

    double tuning() const { return tuning_; }

    void set_tuning(double v) { tuning_ = v; }

    double playing_tuning() const;
    void undo_set_tpc(int v);
    int transposition() const;

    bool fixed() const { return fixed_; }

    void set_fixed(bool v) { fixed_ = v; }

    int fixed_line() const { return fixed_line_; }

    void set_fixed_line(int v) { fixed_line_ = v; }

    int tpc() const;

    int tpc1() const { return tpc_[0]; } // non transposed tpc

    int tpc2() const { return tpc_[1]; } // transposed tpc

    String tpc_user_name(bool explicit_accidental = false, bool full = false) const;

    void set_tpc(int v);

    void set_tpc1(int v) { tpc_[0] = v; }

    void set_tpc2(int v) { tpc_[1] = v; }

    void set_tpc_from_pitch(Prefer prefer = Prefer::NEAREST);
    int tpc1default(int pitch) const;
    int tpc2default(int pitch) const;
    int transpose_tpc(int tpc) const;

    int playing_tpc() const;

    Accidental *accidental() const { return accidental_; }

    void set_accidental(Accidental *a) { accidental_ = a; }

    AccidentalType accidental_type() const;
    void set_accidental_type(AccidentalType type);

    int line() const;

    void set_line(int n) { line_ = n; }

    int fret() const { return fret_; }

    void set_fret(int val) { fret_ = val; }

    float harmonic_fret() const { return harmonic_fret_; }

    void set_harmonic_fret(float val) { harmonic_fret_ = val; }

    int harmonic_pitch_offset() const { return harmonic_pitch_offset_; }

    void set_harmonic_pitch_offset(int val) { harmonic_pitch_offset_ = val; }

    DisplayFretOption display_fret() const { return display_fret_; }

    void set_display_fret(DisplayFretOption val) { display_fret_ = val; }

    String fret_string() const { return fret_string_; }

    void set_fret_string(const String &s) { fret_string_ = s; }

    bool negative_fret_used() const;

    int string() const { return string_; }

    void set_string(int val) { string_ = val; }

    int string_or_line() const;

    bool ghost() const { return ghost_; }

    void set_ghost(bool val) { ghost_ = val; }

    bool dead_note() const { return dead_note_; }

    void set_dead_note(bool dead_note) { dead_note_ = dead_note; }

    bool fret_conflict() const { return fret_conflict_; }

    void set_fret_conflict(bool val) { fret_conflict_ = val; }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    bool is_small() const { return is_small_; }

    void set_small(bool val);

    bool play() const { return play_; }

    void set_play(bool val) { play_ = val; }

    GuitarBend *bend_for() const;
    GuitarBend *bend_back() const;
    GuitarBend *dive_for() const;
    GuitarBend *dive_back() const;

    Tie *tie_for() const { return tie_for_; }

    Tie *tie_back() const { return tie_back_; }

    Tie *tie_for_non_partial() const;
    Tie *tie_back_non_partial() const;
    LaissezVib *laissez_vib() const;
    PartialTie *incoming_partial_tie() const;
    PartialTie *outgoing_partial_tie() const;
    void set_tie_for(Tie *t);
    void set_tie_back(Tie *t);
    Note *first_tied_note(bool ignore_playback = true) const;
    Note *last_tied_note(bool ignore_playback = true) const;

    int unison_index() const;
    void disconnect_tied_notes();
    void connect_tied_notes();

    void setup_after_read(const Fraction &tick, bool paste_mode);

    bool hidden() const { return hidden_; }

    void set_hidden(bool val) { hidden_ = val; }

    bool dots_hidden() const { return dots_hidden_; }

    void set_dots_hidden(bool val) { dots_hidden_ = val; }

    NoteType note_type() const;
    String note_type_user_name() const;

    ElementList &el() { return el_; }

    const ElementList &el() const { return el_; }

    int subchannel() const { return subchannel_; }

    void set_subchannel(int val) { subchannel_ = val; }

    DirectionH user_mirror() const { return user_mirror_; }

    void set_user_mirror(DirectionH d) { user_mirror_ = d; }

    DirectionV user_dot_position() const { return user_dot_position_; }

    void set_user_dot_position(DirectionV d) { user_dot_position_ = d; }

    DirectionV dot_position() const { return dot_position_; }

    void set_dot_position(DirectionV d) { dot_position_ = d; }

    bool dot_is_up() const; // actual dot position

    void reset() override;

    float user_velocity_fraction() const;

    int user_velocity() const { return user_velocity_; }

    void set_user_velocity(int v) { user_velocity_ = v; }

    void set_on_time_offset(int v);
    void set_off_time_offset(int v);

    int customize_velocity(int velo) const;

    NoteDot *dot(int n) { return dots_.at(n); }

    const std::vector<NoteDot *> &dots() const { return dots_; }

    std::vector<NoteDot *> &dots() { return dots_; }

    void update_accidental(AccidentalState *);
    void update_line();
    void set_nval(const NoteVal &, Fraction = {-1, 1});

    NoteEventList &play_events() { return play_events_; }

    const NoteEventList &play_events() const { return play_events_; }

    NoteEvent *note_event(int idx) { return &play_events_[idx]; }

    void set_play_events(const NoteEventList &l) { play_events_ = l; }

    const std::vector<Spanner *> &spanner_for() const { return spanner_for_; }

    const std::vector<Spanner *> &spanner_back() const { return spanner_back_; }

    void add_spanner_back(Spanner *e) {
        if (!muse::contains(spanner_back_, e)) {
            spanner_back_.push_back(e);
        }
    }

    bool remove_spanner_back(Spanner *e) { return muse::remove(spanner_back_, e); }

    void add_spanner_for(Spanner *e) {
        if (!muse::contains(spanner_for_, e)) {
            spanner_for_.push_back(e);
        }
    }

    bool remove_spanner_for(Spanner *e) { return muse::remove(spanner_for_, e); }

    bool transpose_diatonic(int interval, bool keep_alterations, bool use_double_accidentals);
    bool transpose(Interval interval, bool use_double_sharps_flats);

    void local_spatium_changed(double old_value, double new_value) override;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;
    void style_changed() override;

    bool mark() const { return mark_; }

    void set_mark(bool v) const { mark_ = v; }

    void set_score(Score *s) override;

    static SymId note_head(int direction, NoteHeadGroup, NoteHeadType, int tpc, Key key, NoteHeadScheme scheme);
    static SymId note_head(int direction, NoteHeadGroup, NoteHeadType);
    NoteVal note_val() const;

    EngravingItem *next_in_el(EngravingItem *e);
    EngravingItem *prev_in_el(EngravingItem *e);
    EngravingItem *next_element() override;
    EngravingItem *prev_element() override;
    EngravingItem *last_element_before_segment();
    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    std::vector<Note *> tied_notes() const;

    void set_off_time_type(int v) { off_time_type_ = v; }

    void set_on_time_type(int v) { on_time_type_ = v; }

    int off_time_type() const { return off_time_type_; }

    int on_time_type() const { return on_time_type_; }

    void attach_slide(SlideType slide_type);

    bool has_slide_to_note() const;
    bool has_slide_from_note() const;

    SlideType slide_to_type() const { return slide_to_type_; }

    SlideType slide_from_type() const { return slide_from_type_; }

    void set_parentheses_mode(const ParenthesesMode &v, bool add_to_linked = true, bool generated = false) override;

    const NoteParenthesisInfo *parenthesis_info() const;

    void set_harmonic(bool val) { harmonic_ = val; }

    bool harmonic() const { return harmonic_; }

    bool is_grace() const;

    bool is_pre_bend_or_dive_start() const;
    bool is_grace_bend_start() const;
    bool is_continuation_of_bend() const;

    bool has_another_straight_above_or_below(bool above) const;

    std::vector<LineAttachPoint> &line_attach_points() { return line_attach_points_; }

    const std::vector<LineAttachPoint> &line_attach_points() const { return line_attach_points_; }

    void add_start_line_attach_point(PointF point, EngravingItem *line) { add_line_attach_point(point, line, true); }

    void add_end_line_attach_point(PointF point, EngravingItem *line) { add_line_attach_point(point, line, false); }

    PointF pos_in_staff_coordinates();

    bool is_trill_cue_note() const { return is_trill_cue_note_; }

    void set_is_trill_cue_note(bool v);

    SymId note_head() const;
    // Source symbol for an explicit direction, without reading display caches.
    SymId note_head(int direction) const;
    bool is_note_name() const;

    void update_fretting_for_ties_and_bends();
    bool should_hide_fret() const;
    bool should_force_show_fret() const;

    void set_visible(bool v) override;

    bool override_bend_visibility_rules() const { return override_bend_visibility_rules_; }

    void set_override_bend_visibility_rules(bool v) { override_bend_visibility_rules_ = v; }

    bool hide_generated_parens() const { return hide_generated_parens_; }

    void set_hide_generated_parens(bool v) { hide_generated_parens_ = v; }

    TieJumpPointList *tie_jump_points() { return &jump_points_; }

    const TieJumpPointList *tie_jump_points() const { return &jump_points_; }

    bool is_exact_unison(Note *other);

    struct LayoutData : public EngravingItem::LayoutData {
        ld_field<bool> use_tablature = {"[Note] useTablature", false};
        ld_field<SymId> cached_notehead_sym = {"[Note] cachedNoteheadSym",
                                               SymId::NO_SYM};                      // use in draw to avoid recomputing at every update
        ld_field<SymId> cached_sym_null = {"[Note] cachedSymNull", SymId::NO_SYM};  // additional symbol for some transparent notehead
        ld_field<bool> mirror = {"[Note] mirror", false};                           // True if note is mirrored at stem.
        ld_field<bool> has_generated_parens = {"[Note] hasGeneratedParens", false}; // Should generated parens be created
    };
    DECLARE_LAYOUTDATA_METHODS(Note)

private:

    friend struct Factory;
    Note(DummyParentOr<Chord> ch);
    Note(const Note &, bool link = false);

    void add_spanner(Spanner *);
    void remove_spanner(Spanner *);
    int concert_pitch_idx() const;
    void update_rel_line(int abs_line, bool undoable);

    static std::vector<Note *> find_tied_notes(Note *start_note, bool follow_partial_ties = true);

    void get_note_list_for_dots(std::vector<Note *> &top_down_notes, std::vector<Note *> &bottom_up_notes, std::vector<int> &anchored_dots);

    void add_line_attach_point(PointF point, EngravingItem *line, bool start);

    bool ghost_ = false;     // ghost note
    bool dead_note_ = false; // dead note

    bool is_trill_cue_note_ = false;

    bool hidden_ = false;        // marks this note as the hidden one if there are
                                 // overlapping notes; hidden notes are not played
                                 // and heads + accidentals are not shown
    bool dots_hidden_ = false;   // dots of hidden notes are hidden too
                                 // except if only one note is dotted
    bool fret_conflict_ = false; // used by TAB staves to mark a fretting conflict:
                                 // two or more notes on the same string
    bool is_small_ = false;
    bool play_ = true;          // note is not played if false
    mutable bool mark_ = false; // for use in sequencer
    bool fixed_ = false;        // for slash notation

    bool override_bend_visibility_rules_ = false;

    bool hide_generated_parens_ = false;

    SlideType slide_to_type_ = SlideType::UNDEFINED;
    SlideType slide_from_type_ = SlideType::UNDEFINED;

    DirectionH user_mirror_ = DirectionH::AUTO;       ///< user override of mirror
    DirectionV user_dot_position_ = DirectionV::AUTO; ///< user override of dot position
    DirectionV dot_position_ = DirectionV::AUTO;      // used as an intermediate step when resolving dot conflicts

    NoteHeadScheme head_scheme_ = NoteHeadScheme::HEAD_AUTO;
    NoteHeadGroup head_group_ = NoteHeadGroup::HEAD_NORMAL;
    NoteHeadType head_type_ = NoteHeadType::HEAD_AUTO;

    VeloType velo_type_ = VeloType::USER_VAL;

    int off_time_type_ = 0; // compatibility only 1 - user(absolute), 2 - offset (%)
    int on_time_type_ = 0;  // compatibility only 1 - user, 2 - offset

    int subchannel_ = 0;                       // articulation
    int line_ = ::mu::engraving::invalid_line; // y-Position; 0 - top line.
    int fret_ = -1;                            // for tablature view
    float harmonic_fret_ = -1.0;
    int harmonic_pitch_offset_ = 0;
    DisplayFretOption display_fret_ = DisplayFretOption::NO_HARMONIC;
    int string_ = -1;
    mutable int tpc_[2] = {Tpc::TPC_INVALID, Tpc::TPC_INVALID}; // tonal pitch class  (concert/transposing)
    mutable int pitch_ = 0;                                     // Note pitch as midi value (0 - 127).
    mutable double cent_offset_ = 0.0;                          // Pitch offset in cents (100 cents = 1 semitone)

    int user_velocity_ = 0; // velocity user offset in percent, or absolute velocity for this note
    int fixed_line_ = 0;    // fixed line number if _fixed == true
    double tuning_ = 0.0;   // pitch offset in cent, playable only by internal synthesizer

    Accidental *accidental_ = nullptr;

    Tie *tie_for_ = nullptr;
    Tie *tie_back_ = nullptr;

    bool harmonic_ = false;

    bool has_parens_ = false;

    ElementList el_; // fingering, other text, symbols or images
    std::vector<NoteDot *> dots_;
    NoteEventList play_events_;
    std::vector<Spanner *> spanner_for_;
    std::vector<Spanner *> spanner_back_;

    String fret_string_;

    std::vector<LineAttachPoint> line_attach_points_;
    TieJumpPointList jump_points_{this};
};
} // namespace mu::engraving
