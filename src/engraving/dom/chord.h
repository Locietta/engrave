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

#include <iterator>

/**
 \file
 Definition of classes Chord, HelpLine and NoteList.
*/

#include <functional>
#include <set>
#include <vector>

#include "articulation.h"
#include "chordrest.h"
#include "draw/types/color.h"

namespace mu::engraving {
struct AccidentalState;
struct Arpeggio;
struct Chord;
struct Hook;
struct LedgerLine;
struct Note;
struct NoteEventList;
struct Stem;
struct StemSlash;
struct Transaction;
struct TremoloSingleChord;
struct TremoloTwoChord;

enum class NoteType : unsigned char {
    ///.\{
    NORMAL = 0,
    ACCIACCATURA = 0x1,
    APPOGGIATURA = 0x2, // grace notes
    GRACE4 = 0x4,
    GRACE16 = 0x8,
    GRACE32 = 0x10,
    GRACE8_AFTER = 0x20,
    GRACE16_AFTER = 0x40,
    GRACE32_AFTER = 0x80,
    INVALID = 0xFF
    ///\}
};

constexpr NoteType operator|(NoteType t1, NoteType t2) {
    return static_cast<NoteType>(static_cast<unsigned char>(t1) | static_cast<unsigned char>(t2));
}

constexpr bool operator&(NoteType t1, NoteType t2) { return static_cast<unsigned char>(t1) & static_cast<unsigned char>(t2); }

enum class PlayEventType : unsigned char {
    ///.\{
    AUTO, ///< Play events for all notes are calculated by MuseScore.
    USER, ///< Some play events are modified by user. Those events are written into the mscx file.
    ///.\}
};

// A filtered, non-owning query over a chord's grace notes. Iterators own their
// traversal state; neither the view nor its iterators write to the chord.
struct GraceNotesView {
    struct Iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Chord *;
        using difference_type = std::ptrdiff_t;
        using reference = Chord *;
        using pointer = Chord **;

        const std::vector<Chord *> *source = nullptr;
        size_t index = 0;
        bool after = false;
        bool filter_unplayable = false;

        Chord *operator*() const { return (*source)[after ? source->size() - 1 - index : index]; }

        Iterator &operator++();

        Iterator operator++(int) {
            auto previous = *this;
            ++*this;
            return previous;
        }

        bool operator==(const Iterator &) const = default;
        void skip_unmatched();
    };

    const std::vector<Chord *> *source;
    bool after;
    bool filter_unplayable;

    Iterator begin() const {
        Iterator it{source, 0, after, filter_unplayable};
        it.skip_unmatched();
        return it;
    }

    Iterator end() const { return {source, source->size(), after, filter_unplayable}; }

    bool empty() const { return begin() == end(); }

    size_t size() const { return static_cast<size_t>(std::distance(begin(), end())); }

    Chord *front() const { return *begin(); }

    Chord *back() const;
    Chord *at(size_t index) const;

    Chord *operator[](size_t index) const { return at(index); }

    operator std::vector<Chord *>() const { return {begin(), end()}; }
};

struct GraceNotesGroup final : std::vector<Chord *>, EngravingItem {
public:
    GraceNotesGroup *clone() const override { return new GraceNotesGroup(*this); }

    GraceNotesGroup(DummyParentOr<Chord> c);

    Chord *parent() const { return parent_value; }

    EngravingItem *layout_parent() const override;

    void set_pos(double x, double y) override;

    Segment *appended_segment() const { return appended_segment_value; }

    void set_appended_segment(Segment *s) { appended_segment_value = s; }

    void add_to_shape();

private:
    Chord *parent_value = nullptr;
    Segment *appended_segment_value = nullptr; // the graceNoteGroup is appended to this segment
};

//---------------------------------------------------------
//   @@ Chord
///    Graphic representation of a chord.
///    Single notes are handled as degenerated chords.
//
//   @P beam          Beam          the beam of the chord, if any (read only)
//   @P graceNotes    array[Chord]  the list of grace note chords (read only)
//   @P hook          Hook          the hook of the chord, if any (read only)
//   @P lyrics        array[Lyrics] the list of lyrics (read only)
//   @P notes         array[Note]   the list of notes (read only)
//   @P stem          Stem          the stem of the chord, if any (read only)
//   @P stemSlash     StemSlash     the stem slash of the chord (acciaccatura), if any (read only)
//   @P stemDirection Direction     the stem direction of the chord: AUTO, UP, DOWN (read only)
//---------------------------------------------------------

struct NoteParenthesisInfo {
    NoteParenthesisInfo(Parenthesis *l_paren, Parenthesis *r_paren, std::vector<Note *> n_list);
    ~NoteParenthesisInfo();

    Parenthesis *left_paren() const { return left_paren_; }

    Parenthesis *right_paren() const { return right_paren_; }

    const std::vector<Note *> &notes() const { return notes_; }

    void insert_note(Note *note);
    void remove_note(Note *note);

private:
    Parenthesis *left_paren_ = nullptr;
    Parenthesis *right_paren_ = nullptr;
    std::vector<Note *> notes_;
};

using NoteParenthesisInfoList = std::vector<NoteParenthesisInfo *>;

struct Chord final : ChordRest {
private:
    DECLARE_CLASSOF(ElementType::CHORD)

public:

    ~Chord();
    Chord &operator=(const Chord &) = delete;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    Chord *clone() const override { return new Chord(*this, false); }

    EngravingItem *linked_clone() override { return new Chord(*this, true); }

    void undo_unlink() override;

    void set_score(Score *s) override;
    double intrinsic_mag() const override;
    double mag() const override;
    double note_head_width() const;

    void set_color(const Color &c) override;
    void set_stem_direction(DirectionV d);

    DirectionV stem_direction() const { return stem_direction_; }

    Chord *prev() const;
    Chord *next() const;

    bool is_ui_item() const { return is_ui_item_; }

    void set_is_ui_item(bool val) { is_ui_item_ = val; }

    const std::vector<LedgerLine *> &ledger_lines() const { return ledger_lines_; }

    std::vector<LedgerLine *> &ledger_lines() { return ledger_lines_; }

    void resize_ledger_lines_to(size_t new_size);

    double default_stem_length() const { return default_stem_length_; }

    void set_default_stem_length(double l) { default_stem_length_ = l; }

    void set_beam_extension(double extension);

    std::vector<Note *> &notes() { return notes_; }

    const std::vector<Note *> &notes() const { return notes_; }

    const NoteParenthesisInfoList &note_parentheses() const { return note_parens_; }

    const NoteParenthesisInfo *find_note_parenthesis_info(const Note *note) const;
    NoteParenthesisInfo *find_note_parenthesis_info(const Parenthesis *paren);
    void add_note_parenthesis_info(NoteParenthesisInfo *note_paren_info);
    void remove_note_parenthesis_info(const NoteParenthesisInfo *note_paren_info);
    void add_note_to_parenthesis_info(Note *note, const Parenthesis *paren);
    void remove_note_from_parenthesis_info(Note *note, const Parenthesis *paren);

    bool is_chord_playable() const;
    void set_is_chord_playable(const bool is_playable);

    // Chord has at least one Note
    Note *up_note() const;
    Note *down_note() const;
    int up_string() const;
    int down_string() const;
    std::vector<int> note_distances() const;

    double max_head_width() const;

    Note *find_note(int pitch, int skip = 0) const;

    Stem *stem() const { return stem_; }

    Arpeggio *arpeggio() const { return arpeggio_; }

    void set_arpeggio(Arpeggio *a) { arpeggio_ = a; }

    Arpeggio *span_arpeggio() const { return span_arpeggio_; }

    void set_span_arpeggio(Arpeggio *a) { span_arpeggio_ = a; }

    void undo_change_span_arpeggio(Arpeggio *a);

    TremoloType tremolo_type() const;
    TremoloTwoChord *tremolo_two_chord() const;
    TremoloSingleChord *tremolo_single_chord() const;

    void set_tremolo_two_chord(TremoloTwoChord *tr, bool apply_logic = true);
    void set_tremolo_single_chord(TremoloSingleChord *tr);

    ChordLine *chord_line() const;

    bool ends_note_anchored_line() const { return ends_note_anchored_line_; }

    void set_ends_note_anchored_line(bool val) { ends_note_anchored_line_ = val; }

    void update_ends_note_anchored_line();

    StemSlash *stem_slash() const { return stem_slash_; }

    bool show_stem_slash() const { return show_stem_slash_; }

    void set_show_stem_slash_in_advance();
    void request_show_stem_slash(bool show);

    void set_show_stem_slash(bool show) { show_stem_slash_ = show; }

    bool slash();
    void set_slash(bool flag, bool stemless);
    void remove_markings(bool keep_tremolo = false) override;

    const std::vector<Chord *> &grace_notes() const { return grace_notes_; }

    std::vector<Chord *> all_grace_chords_of_main_chord();

    void remove_all_grace_notes();

    GraceNotesView grace_notes_before(bool filter_unplayable = false) const;
    GraceNotesView grace_notes_after(bool filter_unplayable = false) const;

    size_t grace_index() const { return grace_index_; }

    void set_grace_index(size_t val) { grace_index_ = val; }

    Chord *grace_note_at(size_t idx) const;

    int line(bool up) const { return up ? up_line() : down_line(); }

    int line() const { return ldata()->up ? up_line() : down_line(); }

    int up_line() const;
    int down_line() const;
    double right_edge() const override;

    bool under_beam() const;

    Hook *hook() const { return hook_; }

    void set_hook(Hook *h) { hook_ = h; }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    Note *selected_note() const;

    PointF page_pos() const override; ///< position in page coordinates
    void cmd_update_notes(AccidentalState *, staff_idx_t staff_idx);

    NoteType note_type() const { return note_type_; }

    void set_note_type(NoteType t);

    bool is_grace() const { return note_type_ != NoteType::NORMAL; }

    void to_grace_after();

    bool is_pre_bend_or_grace_bend_start() const;
    bool is_grace_bend_end() const;

    bool is_trill_cue_note() const { return is_trill_cue_note_; }

    void set_is_trill_cue_note(bool v);

    void set_track(track_idx_t val) override;

    double dot_pos_x() const { return dot_pos_x_; }

    void set_dot_pos_x(double x) { dot_pos_x_ = x; }

    bool no_stem() const { return no_stem_; }

    void set_no_stem(bool val) { no_stem_ = val; }

    double space_lw() { return space_lw_; }

    void set_space_lw(double lw) { space_lw_ = lw; }

    double space_rw() { return space_rw_; }

    void set_space_rw(double rw) { space_rw_ = rw; }

    AutoOnOff combine_voice() const { return combine_voice_; }

    void set_combine_voice(AutoOnOff v) { combine_voice_ = v; }

    bool should_combine_voice() const;
    static bool combine_voice(const Chord *chord1, const Chord *chord2);

    PlayEventType play_event_type() const { return play_event_type_; }

    void set_play_event_type(PlayEventType v) { play_event_type_ = v; }

    std::vector<NoteEventList> get_note_event_lists();
    void set_note_event_lists(std::vector<NoteEventList> &nel);

    TremoloChordType tremolo_chord_type() const;

    std::vector<Articulation *> &articulations() { return articulations_; }

    const std::vector<Articulation *> &articulations() const { return articulations_; }

    std::set<SymId> articulation_symbol_ids() const;
    Articulation *has_articulation(const Articulation *);
    Tapping *tapping() const;

    bool has_single_articulation() const { return articulations_.size() == 1; }

    void update_articulations(const std::set<SymId> &new_articulation_ids,
                              ArticulationsUpdateMode replace_mode = ArticulationsUpdateMode::INSERT);

    void local_spatium_changed(double old_value, double new_value) override;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;
    bool is_user_modified() const override;

    void reset() override;

    Segment *segment() const override;
    Measure *measure() const override;

    void sort_notes();

    Chord *next_tied_chord(bool backwards = false, bool same_size = true) const;

    Fraction end_tick_including_tied() const;

    EngravingItem *next_element() override;
    EngravingItem *prev_element() override;
    EngravingItem *next_segment_element() override;
    EngravingItem *last_element_before_segment();
    EngravingItem *prev_segment_element() override;

    Note *first_grace_or_note();

    void undo_change_property(Pid id, const PropertyValue &new_value);
    void undo_change_property(Pid id, const PropertyValue &new_value, PropertyFlags ps) override;

    void style_changed() override;

    bool allow_kerning_above() const { return allow_kerning_above_; }

    bool allow_kerning_below() const { return allow_kerning_below_; }

    void compute_kerning_exceptions();

    Ornament *find_ornament(bool for_playback = false) const;

    const std::set<Spanner *> &starting_spanners() const { return starting_spanners_; }

    const std::set<Spanner *> &ending_spanners() const { return ending_spanners_; }

    void add_starting_spanner(Spanner *spanner) { starting_spanners_.insert(spanner); }

    void remove_starting_spanner(Spanner *spanner) { starting_spanners_.erase(spanner); }

    void add_ending_spanner(Spanner *spanner) { ending_spanners_.insert(spanner); }

    void remove_ending_spanner(Spanner *spanner) { ending_spanners_.erase(spanner); }

    bool should_have_stem() const;
    bool should_have_hook() const;
    void create_stem();
    void remove_stem();
    void create_hook();

    double up_pos() const override;
    double down_pos() const override;

    struct StartEndSlurs {
        bool start_up = false;
        bool start_down = false;
        bool end_up = false;
        bool end_down = false;

        void reset() {
            start_up = false;
            start_down = false;
            end_up = false;
            end_down = false;
        }
    };

    StartEndSlurs &start_end_slurs() { return start_end_slurs_; }

    bool all_notes_tied_to_next() const;
    bool all_elements_invisible() const;

private:

    friend struct Factory;

    Chord(DummyParentOr<Segment> parent);
    Chord(const Chord &, bool link = false);

    // `includeTemporarySiblings`: whether items that are deleted & recreated during every layout should also be processed
    void process_siblings(std::function<void(EngravingItem *)> func, bool include_temporary_siblings) const;

    std::vector<Note *> notes_; // sorted to decreasing line step
    std::vector<LedgerLine *> ledger_lines_;
    NoteParenthesisInfoList note_parens_;

    Stem *stem_ = nullptr;
    Hook *hook_ = nullptr;
    StemSlash *stem_slash_ = nullptr; // for grace notes
    bool show_stem_slash_ = false;

    Arpeggio *arpeggio_ = nullptr;      // arpeggio which starts on the chord
    Arpeggio *span_arpeggio_ = nullptr; // arpeggio which spans over this chord

    TremoloTwoChord *tremolo_two_chord_ = nullptr;
    TremoloSingleChord *tremolo_single_chord_ = nullptr;

    bool ends_note_anchored_line_ = false; // true if this chord is the ending point of a glissando (needed for layout)
    std::vector<Chord *> grace_notes_;     // storage for all grace notes
    size_t grace_index_ = 0;               // if this is a grace note, index in parent list

    bool is_trill_cue_note_ = false;

    DirectionV stem_direction_ = DirectionV::AUTO;
    NoteType note_type_ = NoteType::NORMAL; // mark grace notes: acciaccatura and appoggiatura
    bool no_stem_ = false;
    PlayEventType play_event_type_ = PlayEventType::AUTO; // play events were modified by user

    double space_lw_ = 0.0;
    double space_rw_ = 0.0;

    double default_stem_length_ = 0.0;

    bool is_ui_item_ = false;

    double dot_pos_x_ = 0.0;

    StartEndSlurs start_end_slurs_;

    std::set<Spanner *> starting_spanners_; // spanners starting on this item
    std::set<Spanner *> ending_spanners_;   // spanners ending on this item

    bool allow_kerning_above_ = true;
    bool allow_kerning_below_ = true;

    AutoOnOff combine_voice_ = AutoOnOff::AUTO;

    std::vector<Articulation *> articulations_;
};
} // namespace mu::engraving
