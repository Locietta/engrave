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

#include <type_traits>
#include <vector>

#include "../infrastructure/eid.h"
#include "../infrastructure/rtti.h" // IWYU pragma: export
#include "../style/styledef.h"
#include "../types/propertyvalue.h"
#include "../types/types.h"

namespace mu::engraving {
struct Accidental;
struct Ambitus;
struct Arpeggio;
struct Articulation;
struct BSymbol;
struct BagpipeEmbellishment;
struct BarLine;
struct Beam;
struct BeamSegment;
struct Bend;
struct Box;
struct Bracket;
struct BracketItem;
struct Breath;
struct Chord;
struct ChordBracket;
struct ChordLine;
struct ChordRest;
struct Clef;
struct Capo;
struct DeadSlapped;
struct DummyParent;
struct DurationElement;
struct Dynamic;
struct Expression;
struct EngravingItem;
struct EngravingObject;
struct FBox;
struct FSymbol;
struct Fermata;
struct FiguredBass;
struct FiguredBassItem;
struct Fingering;
struct FretDiagram;
struct Glissando;
struct GlissandoSegment;
struct GraceNotesGroup;
struct GradualTempoChange;
struct GradualTempoChangeSegment;
struct GuitarBend;
struct GuitarBendSegment;
struct GuitarBendHold;
struct GuitarBendHoldSegment;
struct GuitarBendText;
struct HBox;
struct Hairpin;
struct HairpinSegment;
struct HammerOnPullOff;
struct HammerOnPullOffSegment;
struct HammerOnPullOffText;
struct HarmonicMark;
struct HarmonicMarkSegment;
struct Harmony;
struct HarpPedalDiagram;
struct Hook;
struct Image;
struct IndicatorIcon;
struct InstrumentChange;
struct InstrumentName;
struct Jump;
struct KeySig;
struct LayoutBreak;
struct LaissezVib;
struct LaissezVibSegment;
struct LedgerLine;
struct LetRing;
struct LetRingSegment;
struct LineSegment;
struct Lyrics;
struct LyricsLine;
struct LyricsLineSegment;
struct MMRest;
struct MMRestRange;
struct Marker;
struct MasterScore;
struct Measure;
struct MeasureBase;
struct MeasureNumber;
struct MeasureRepeat;
struct MStyle;
struct Note;
struct NoteDot;
struct NoteHead;
struct NoteLine;
struct NoteLineSegment;
struct Ornament;
struct Ottava;
struct OttavaSegment;
struct Page;
struct PageLockIndicator;
struct PalmMute;
struct PalmMuteSegment;
struct Parenthesis;
struct Part;
struct PartialLyricsLine;
struct PartialLyricsLineSegment;
struct PartialTie;
struct PartialTieSegment;
struct Pedal;
struct PedalSegment;
struct PickScrape;
struct PickScrapeSegment;
struct PlayCountText;
struct PlayTechAnnotation;
struct Rasgueado;
struct RasgueadoSegment;
struct RehearsalMark;
struct Rest;
struct Score;
struct Segment;
struct SharedPart;
struct SLine;
struct Slur;
struct SlurSegment;
struct SlurTie;
struct SlurTieSegment;
struct Spacer;
struct Spanner;
struct SpannerSegment;
struct Staff;
struct StaffLines;
struct StaffState;
struct StaffText;
struct StaveSharingLabel;
struct StaffTextBase;
struct StaffTypeChange;
struct StaffVisibilityIndicator;
struct Stem;
struct StemSlash;
struct Sticking;
struct StringTunings;
struct Symbol;
struct System;
struct SystemDivider;
struct SystemLockIndicator;
struct SystemText;
struct SoundFlag;
struct Tapping;
struct TappingHalfSlur;
struct TappingHalfSlurSegment;
struct TBox;
struct TempoText;
struct Text;
struct TextBase;
struct TextLine;
struct TextLineBase;
struct TextLineBaseSegment;
struct TextLineSegment;
struct Tie;
struct TieSegment;
struct TimeSig;
struct TimeTickAnchor;
struct TremoloBar;
struct Trill;
struct TrillSegment;
struct TripletFeel;
struct Tuplet;
struct VBox;
struct Vibrato;
struct VibratoSegment;
struct Volta;
struct VoltaSegment;
struct WhammyBar;
struct WhammyBarSegment;

struct LinkedObjects;

enum class Pid : short;
enum class PropertyFlags : char;

using EngravingObjectList = std::vector<EngravingObject *>;
using EngravingItemList = std::vector<EngravingItem *>;

//! Parameter type for a parent that is either a real parent of one of the given types,
//! or the dummy, i.e. no parent at all. It holds nothing but the pointer and makes no
//! difference at runtime; the point is that "not attached to anything" has to be said
//! rather than slipped in as a pointer of the wrong kind.
//!
//! Best-effort only: it narrows what a call site can pass, it does not prove anything.
template <typename... ParentTypes>
struct DummyParentOr {
    template <typename T, std::enable_if_t<std::is_same_v<T, DummyParent> || (std::is_base_of_v<ParentTypes, T> || ...), int> = 0>
    DummyParentOr(T *parent) : parent_(parent) {}

    operator EngravingObject *() const { return parent_; }

    //! The parent as one of the accepted types, or null when it is the dummy - i.e.
    //! when there is no parent to speak to yet.
    template <typename T>
    T *as() const;

private:
    EngravingObject *parent_ = nullptr;
};

//! The given parent if there is one, the dummy otherwise.
template <typename T>
DummyParentOr<T> parent_or_dummy(T *parent, DummyParent *dummy) {
    return parent ? DummyParentOr<T>(parent) : DummyParentOr<T>(dummy);
}

struct EngravingObject {
    EngravingObject(const ElementType &type, EngravingObject *parent);
    EngravingObject(const EngravingObject &se);

    virtual ~EngravingObject();

    inline ElementType type() const { return type_; }

    inline bool is_type(ElementType t) const { return t == type_; }

    const char *type_name() const;
    virtual TranslatableString type_user_name() const;
    virtual String translated_type_user_name() const;

    EID eid() const;
    bool set_eid(EID id) const;
    EID assign_new_eid() const;

    //! The raw tree link, i.e. the inverse of children(): the ownership parent
    //! if there is one, otherwise the dummy the object is parked on. Prefer
    //! ownershipParent(), which reports "not attached to anything" as null.
    EngravingObject *parent() const;

    void set_ownership_parent(EngravingObject *p);
    //! The parent this object is attached to. Null while it is parked on the dummy,
    //! i.e. not attached to anything.
    EngravingObject *ownership_parent() const;
    void move_to_dummy();

    const EngravingObjectList &children() const { return children_; }

    EngravingItemList children_items(bool all = false) const;
    std::vector<EngravingItem *> get_children(bool include_invisible = true) const;

    virtual void scan_elements(std::function<void(EngravingItem *)>) {}

    // context
    virtual void set_score(Score *s);
    Score *score() const;
    MasterScore *master_score() const;
    bool on_same_score(const EngravingObject *other) const;
    const MStyle &style() const;

    virtual EngravingObject *property_delegate(Pid) const { return nullptr; }

    virtual PropertyValue get_property(Pid) const = 0;
    virtual bool set_property(Pid, const PropertyValue &) = 0;
    virtual PropertyValue property_default(Pid) const;
    virtual void reset_property(Pid id);

    virtual bool size_is_spatium_dependent() const { return true; }

    virtual bool offset_is_spatium_dependent() const { return true; }

    virtual void reset(); // reset all properties & position to default

    virtual void init_element_style(const ElementStyle *);

    virtual const ElementStyle *styled_properties() const { return element_style_; }

    virtual PropertyFlags *property_flags_list() const { return property_flags_list_; }

    virtual PropertyFlags property_flags(Pid) const;
    bool is_styled(Pid pid) const;
    PropertyValue style_value(Pid, Sid) const;

    virtual void set_property_flags(Pid, PropertyFlags);

    virtual Sid get_property_style(Pid) const;

    virtual void style_changed();

    virtual void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps);
    void undo_change_property(Pid id, const PropertyValue &);
    void undo_reset_property(Pid id);

    void undo_push_property(Pid);

    std::list<EngravingObject *> link_list() const;

    void link_to(EngravingObject *);
    void unlink();
    bool is_linked(EngravingObject *se = nullptr) const;

    virtual void undo_unlink();

    LinkedObjects *links() const { return links_; }

    void set_links(LinkedObjects *le);

protected:
    virtual int get_property_flags_idx(Pid id) const;

    //! NOTE For compatibility reasons, hope, we will remove the need for this method.
    void hack_set_type(const ElementType &t) { type_ = t; }

    void add_child(EngravingObject *o);
    void remove_child(EngravingObject *o);

    const ElementStyle *element_style_{&::mu::engraving::EngravingObject::empty_style};
    PropertyFlags *property_flags_list_ = nullptr;
    LinkedObjects *links_ = nullptr;
    Score *score_ = nullptr;

private:
    static ElementStyle const empty_style;

    void do_set_parent(EngravingObject *p);
    void do_set_score(Score *sc);

    ElementType type_ = ElementType::INVALID;

    EngravingObject *parent_ = nullptr;
    EngravingObjectList children_;

public:

    virtual std::list<EngravingObject *> link_list_for_property_propagation() const { return link_list(); }

    //---------------------------------------------------
    // check type
    //
    // Example for ChordRest:
    //
    //    bool             isChordRest()
    //---------------------------------------------------

#define CONVERT(predicate, kind) \
    bool predicate() const { return type() == ElementType::kind; }

    CONVERT(is_note, NOTE)
    CONVERT(is_rest, REST)
    CONVERT(is_mm_rest, MMREST)
    CONVERT(is_dead_slapped, DEAD_SLAPPED)
    CONVERT(is_chord, CHORD)
    CONVERT(is_bar_line, BAR_LINE)
    CONVERT(is_articulation, ARTICULATION)
    CONVERT(is_ornament, ORNAMENT)
    CONVERT(is_fermata, FERMATA)
    CONVERT(is_marker, MARKER)
    CONVERT(is_clef, CLEF)
    CONVERT(is_key_sig, KEYSIG)
    CONVERT(is_time_sig, TIMESIG)
    CONVERT(is_measure, MEASURE)
    CONVERT(is_tempo_text, TEMPO_TEXT)
    CONVERT(is_breath, BREATH)
    CONVERT(is_h_box, HBOX)
    CONVERT(is_v_box, VBOX)
    CONVERT(is_t_box, TBOX)
    CONVERT(is_f_box, FBOX)
    CONVERT(is_glissando, GLISSANDO)
    CONVERT(is_glissando_segment, GLISSANDO_SEGMENT)
    CONVERT(is_guitar_bend, GUITAR_BEND)
    CONVERT(is_guitar_bend_segment, GUITAR_BEND_SEGMENT)
    CONVERT(is_guitar_bend_hold, GUITAR_BEND_HOLD)
    CONVERT(is_guitar_bend_hold_segment, GUITAR_BEND_HOLD_SEGMENT)
    CONVERT(is_guitar_bend_text, GUITAR_BEND_TEXT)
    CONVERT(is_system_divider, SYSTEM_DIVIDER)
    CONVERT(is_rehearsal_mark, REHEARSAL_MARK)
    CONVERT(is_triplet_feel, TRIPLET_FEEL)
    CONVERT(is_harmony, HARMONY)
    CONVERT(is_volta, VOLTA)
    CONVERT(is_jump, JUMP)
    CONVERT(is_ottava, OTTAVA)
    CONVERT(is_layout_break, LAYOUT_BREAK)
    CONVERT(is_staff_visibility_indicator, STAFF_VISIBILITY_INDICATOR)
    CONVERT(is_system_lock_indicator, SYSTEM_LOCK_INDICATOR)
    CONVERT(is_segment, SEGMENT)
    CONVERT(is_system, SYSTEM)
    CONVERT(is_lyrics, LYRICS)
    CONVERT(is_stem, STEM)
    CONVERT(is_beam, BEAM)
    CONVERT(is_hook, HOOK)
    CONVERT(is_stem_slash, STEM_SLASH)
    CONVERT(is_laissez_vib_segment, LAISSEZ_VIB_SEGMENT)
    CONVERT(is_laissez_vib, LAISSEZ_VIB)
    CONVERT(is_partial_tie_segment, PARTIAL_TIE_SEGMENT)
    CONVERT(is_partial_tie, PARTIAL_TIE)
    CONVERT(is_spacer, SPACER)
    CONVERT(is_staff_lines, STAFF_LINES)
    CONVERT(is_ambitus, AMBITUS)
    CONVERT(is_bracket, BRACKET)
    CONVERT(is_instrument_change, INSTRUMENT_CHANGE)
    CONVERT(is_staff_type_change, STAFFTYPE_CHANGE)
    CONVERT(is_hairpin, HAIRPIN)
    CONVERT(is_hairpin_segment, HAIRPIN_SEGMENT)
    CONVERT(is_bend, BEND)
    CONVERT(is_tremolo_bar, TREMOLOBAR)
    CONVERT(is_measure_repeat, MEASURE_REPEAT)
    CONVERT(is_tuplet, TUPLET)
    CONVERT(is_note_dot, NOTEDOT)
    CONVERT(is_dynamic, DYNAMIC)
    CONVERT(is_expression, EXPRESSION)
    CONVERT(is_instrument_name, INSTRUMENT_NAME)
    CONVERT(is_accidental, ACCIDENTAL)
    CONVERT(is_text_line, TEXTLINE)
    CONVERT(is_text_line_segment, TEXTLINE_SEGMENT)
    CONVERT(is_pedal, PEDAL)
    CONVERT(is_pedal_segment, PEDAL_SEGMENT)
    CONVERT(is_ottava_segment, OTTAVA_SEGMENT)
    CONVERT(is_ledger_line, LEDGER_LINE)
    CONVERT(is_volta_segment, VOLTA_SEGMENT)
    CONVERT(is_note_line, NOTELINE)
    CONVERT(is_note_line_segment, NOTELINE_SEGMENT)
    CONVERT(is_trill, TRILL)
    CONVERT(is_trill_segment, TRILL_SEGMENT)
    CONVERT(is_let_ring, LET_RING)
    CONVERT(is_let_ring_segment, LET_RING_SEGMENT)
    CONVERT(is_gradual_tempo_change, GRADUAL_TEMPO_CHANGE)
    CONVERT(is_gradual_tempo_change_segment, GRADUAL_TEMPO_CHANGE_SEGMENT)
    CONVERT(is_vibrato, VIBRATO)
    CONVERT(is_palm_mute, PALM_MUTE)
    CONVERT(is_palm_mute_segment, PALM_MUTE_SEGMENT)
    CONVERT(is_whammy_bar, WHAMMY_BAR)
    CONVERT(is_whammy_bar_segment, WHAMMY_BAR_SEGMENT)
    CONVERT(is_rasgueado, RASGUEADO)
    CONVERT(is_rasgueado_segment, RASGUEADO_SEGMENT)
    CONVERT(is_harmonic_mark, HARMONIC_MARK)
    CONVERT(is_harmonic_mark_segment, HARMONIC_MARK_SEGMENT)
    CONVERT(is_pick_scrape, PICK_SCRAPE)
    CONVERT(is_pick_scrape_segment, PICK_SCRAPE_SEGMENT)
    CONVERT(is_vibrato_segment, VIBRATO_SEGMENT)
    CONVERT(is_symbol, SYMBOL)
    CONVERT(is_f_symbol, FSYMBOL)
    CONVERT(is_fingering, FINGERING)
    CONVERT(is_note_head, NOTEHEAD)
    CONVERT(is_partial_lyrics_line, PARTIAL_LYRICSLINE)
    CONVERT(is_partial_lyrics_line_segment, PARTIAL_LYRICSLINE_SEGMENT)
    CONVERT(is_figured_bass, FIGURED_BASS)
    CONVERT(is_figured_bass_item, FIGURED_BASS_ITEM)
    CONVERT(is_staff_state, STAFF_STATE)
    CONVERT(is_arpeggio, ARPEGGIO)
    CONVERT(is_chord_bracket, CHORD_BRACKET)
    CONVERT(is_image, IMAGE)
    CONVERT(is_chord_line, CHORDLINE)
    CONVERT(is_fret_diagram, FRET_DIAGRAM)
    CONVERT(is_harp_pedal_diagram, HARP_DIAGRAM)
    CONVERT(is_page, PAGE)
    CONVERT(is_page_lock_indicator, PAGE_LOCK_INDICATOR)
    CONVERT(is_text, TEXT)
    CONVERT(is_measure_number, MEASURE_NUMBER)
    CONVERT(is_mm_rest_range, MMREST_RANGE)
    CONVERT(is_staff_text, STAFF_TEXT)
    CONVERT(is_stave_sharing_label, STAVE_SHARING_LABEL)
    CONVERT(is_system_text, SYSTEM_TEXT)
    CONVERT(is_sound_flag, SOUND_FLAG)
    CONVERT(is_play_count_text, PLAY_COUNT_TEXT)
    CONVERT(is_play_tech_annotation, PLAYTECH_ANNOTATION)
    CONVERT(is_capo, CAPO)
    CONVERT(is_bracket_item, BRACKET_ITEM)
    CONVERT(is_score, SCORE)
    CONVERT(is_staff, STAFF)
    CONVERT(is_part, PART)
    CONVERT(is_shared_part, SHARED_PART)
    CONVERT(is_bagpipe_embellishment, BAGPIPE_EMBELLISHMENT)
    CONVERT(is_sticking, STICKING)
    CONVERT(is_grace_notes_group, GRACE_NOTES_GROUP)
    CONVERT(is_string_tunings, STRING_TUNINGS)
    CONVERT(is_time_tick_anchor, TIME_TICK_ANCHOR)
    CONVERT(is_parenthesis, PARENTHESIS)
    CONVERT(is_hammer_on_pull_off, HAMMER_ON_PULL_OFF)
    CONVERT(is_hammer_on_pull_off_segment, HAMMER_ON_PULL_OFF_SEGMENT)
    CONVERT(is_hammer_on_pull_off_text, HAMMER_ON_PULL_OFF_TEXT)
    CONVERT(is_tapping, TAPPING)
    CONVERT(is_tapping_half_slur, TAPPING_HALF_SLUR)
    CONVERT(is_tapping_half_slur_segment, TAPPING_HALF_SLUR_SEGMENT)
#undef CONVERT

    virtual bool is_engraving_item() const { return false; } // overridden in element.h

    bool is_rest_family() const { return is_rest() || is_mm_rest() || is_measure_repeat(); }

    bool is_chord_rest() const { return is_rest_family() || is_chord(); }

    bool is_duration_element() const { return is_chord_rest() || is_tuplet(); }

    bool is_slur_tie() const { return is_slur() || is_tie(); }

    bool is_slur_tie_segment() const { return is_slur_segment() || is_tie_segment(); }

    bool is_s_line_segment() const;

    bool is_box() const { return is_v_box() || is_h_box() || is_t_box() || is_f_box(); }

    bool is_v_box_base() const { return is_v_box() || is_t_box() || is_f_box(); }

    bool is_measure_base() const { return is_measure() || is_box(); }

    bool is_text_base() const;

    bool is_text_line_base_segment() const {
        return is_hairpin_segment() || is_let_ring_segment() || is_gradual_tempo_change_segment() || is_text_line_segment() ||
               is_ottava_segment() || is_palm_mute_segment() || is_pick_scrape_segment() || is_whammy_bar_segment() ||
               is_rasgueado_segment() || is_harmonic_mark_segment() || is_pedal_segment() || is_volta_segment() || is_note_line_segment();
    }

    bool is_slur() const {
        return type() == ElementType::SLUR || type() == ElementType::HAMMER_ON_PULL_OFF || type() == ElementType::TAPPING_HALF_SLUR;
    }

    bool is_slur_segment() const {
        return type() == ElementType::SLUR_SEGMENT || type() == ElementType::HAMMER_ON_PULL_OFF_SEGMENT ||
               type() == ElementType::TAPPING_HALF_SLUR_SEGMENT;
    }

    bool is_line_segment() const {
        return is_glissando_segment() || is_lyrics_line_segment() || is_text_line_base_segment() || is_trill_segment() ||
               is_vibrato_segment() || is_guitar_bend_segment() || is_guitar_bend_hold_segment();
    }

    bool is_tie_segment() const {
        return type() == ElementType::TIE_SEGMENT || type() == ElementType::LAISSEZ_VIB_SEGMENT ||
               type() == ElementType::PARTIAL_TIE_SEGMENT;
    }

    bool is_tie() const { return type() == ElementType::TIE || type() == ElementType::LAISSEZ_VIB || type() == ElementType::PARTIAL_TIE; }

    bool is_spanner_segment() const { return is_line_segment() || is_text_line_base_segment() || is_slur_segment() || is_tie_segment(); }

    bool is_b_symbol() const { return is_image() || is_symbol(); }

    bool is_text_line_base() const {
        return is_hairpin() || is_let_ring() || is_gradual_tempo_change() || is_text_line() || is_ottava() || is_palm_mute() ||
               is_pick_scrape() || is_whammy_bar() || is_rasgueado() || is_harmonic_mark() || is_pedal() || is_volta() || is_note_line();
    }

    bool is_lyrics_line() const { return type() == ElementType::LYRICSLINE || type() == ElementType::PARTIAL_LYRICSLINE; }

    bool is_lyrics_line_segment() const {
        return type() == ElementType::LYRICSLINE_SEGMENT || type() == ElementType::PARTIAL_LYRICSLINE_SEGMENT;
    }

    bool is_s_line() const {
        return is_text_line_base() || is_trill() || is_glissando() || is_vibrato() || is_guitar_bend() || is_guitar_bend_hold();
    }

    bool is_spanner() const { return is_slur() || is_tie() || is_lyrics_line() || is_s_line(); }

    bool is_staff_text_base() const {
        return is_staff_text() || is_stave_sharing_label() || is_system_text() || is_triplet_feel() || is_play_tech_annotation() ||
               is_capo() || is_string_tunings();
    }

    bool is_articulation_family() const { return is_articulation() || is_ornament() || is_tapping(); }

    bool is_articulation_or_fermata() const { return is_articulation_family() || is_fermata(); }

    bool is_indicator_icon() const { return is_system_lock_indicator() || is_page_lock_indicator() || is_staff_visibility_indicator(); }
};

template <typename... ParentTypes>
template <typename T>
T *DummyParentOr<ParentTypes...>::as() const {
    return parent_ && !parent_->is_type(ElementType::DUMMY) ? static_cast<T *>(parent_) : nullptr;
}

//---------------------------------------------------
// safe casting of EngravingObject
//
// Example for ChordRest:
//
//    ChordRest* toChordRest(EngravingItem* e)
//---------------------------------------------------

static inline Rest *to_rest(EngravingObject *e) {
    assert(!e || e->is_rest_family());
    return (Rest *) e;
}

static inline const Rest *to_rest(const EngravingObject *e) {
    assert(!e || e->is_rest_family());
    return (const Rest *) e;
}

static inline Articulation *to_articulation(EngravingObject *e) {
    assert(!e || e->is_articulation_family());
    return (Articulation *) e;
}

static inline const Articulation *to_articulation(const EngravingObject *e) {
    assert(!e || e->is_articulation_family());
    return (const Articulation *) e;
}

static inline Arpeggio *to_arpeggio(EngravingObject *e) {
    assert(!e || e->is_arpeggio() || e->is_chord_bracket());
    return (Arpeggio *) e;
}

static inline const Arpeggio *to_arpeggio(const EngravingObject *e) {
    assert(!e || e->is_arpeggio() || e->is_chord_bracket());
    return (const Arpeggio *) e;
}

#define CONVERT(a, cast_name, predicate)                         \
    static inline a *cast_name(EngravingObject *e) {             \
        assert(!e || e->predicate());                            \
        return (a *) e;                                          \
    }                                                            \
    static inline const a *cast_name(const EngravingObject *e) { \
        assert(!e || e->predicate());                            \
        return (const a *) e;                                    \
    }

CONVERT(EngravingItem, to_engraving_item, is_engraving_item)
CONVERT(Note, to_note, is_note)
CONVERT(Chord, to_chord, is_chord)
CONVERT(BarLine, to_bar_line, is_bar_line)
CONVERT(Ornament, to_ornament, is_ornament)
CONVERT(Fermata, to_fermata, is_fermata)
CONVERT(Marker, to_marker, is_marker)
CONVERT(Clef, to_clef, is_clef)
CONVERT(KeySig, to_key_sig, is_key_sig)
CONVERT(TimeSig, to_time_sig, is_time_sig)
CONVERT(Measure, to_measure, is_measure)
CONVERT(MeasureBase, to_measure_base, is_measure_base)
CONVERT(DurationElement, to_duration_element, is_duration_element)
CONVERT(ChordRest, to_chord_rest, is_chord_rest)
CONVERT(TempoText, to_tempo_text, is_tempo_text)
CONVERT(Breath, to_breath, is_breath)
CONVERT(Box, to_box, is_box)
CONVERT(HBox, to_h_box, is_h_box)
CONVERT(VBox, to_v_box, is_v_box)
CONVERT(TBox, to_t_box, is_t_box)
CONVERT(FBox, to_f_box, is_f_box)
CONVERT(Spanner, to_spanner, is_spanner)
CONVERT(SpannerSegment, to_spanner_segment, is_spanner_segment)
CONVERT(Glissando, to_glissando, is_glissando)
CONVERT(GlissandoSegment, to_glissando_segment, is_glissando_segment)
CONVERT(GuitarBend, to_guitar_bend, is_guitar_bend)
CONVERT(GuitarBendSegment, to_guitar_bend_segment, is_guitar_bend_segment)
CONVERT(GuitarBendHold, to_guitar_bend_hold, is_guitar_bend_hold)
CONVERT(GuitarBendHoldSegment, to_guitar_bend_hold_segment, is_guitar_bend_hold_segment)
CONVERT(GuitarBendText, to_guitar_bend_text, is_guitar_bend_text)
CONVERT(SystemDivider, to_system_divider, is_system_divider)
CONVERT(RehearsalMark, to_rehearsal_mark, is_rehearsal_mark)
CONVERT(Bend, to_bend, is_bend)
CONVERT(TripletFeel, to_triplet_feel, is_triplet_feel)
CONVERT(Harmony, to_harmony, is_harmony)
CONVERT(Jump, to_jump, is_jump)
CONVERT(StaffText, to_staff_text, is_staff_text)
CONVERT(StaveSharingLabel, to_stave_sharing_label, is_stave_sharing_label);
CONVERT(StaffTextBase, to_staff_text_base, is_staff_text_base)
CONVERT(TextBase, to_text_base, is_text_base)
CONVERT(TextLineBase, to_text_line_base, is_text_line_base)
CONVERT(TextLineBaseSegment, to_text_line_base_segment, is_text_line_base_segment)
CONVERT(PlayCountText, to_play_count_text, is_play_count_text)
CONVERT(PlayTechAnnotation, to_play_tech_annotation, is_play_tech_annotation)
CONVERT(Capo, to_capo, is_capo)
CONVERT(Ottava, to_ottava, is_ottava)
CONVERT(LayoutBreak, to_layout_break, is_layout_break)
CONVERT(IndicatorIcon, to_indicator_icon, is_indicator_icon)
CONVERT(StaffVisibilityIndicator, to_staff_visibility_indicator, is_staff_visibility_indicator)
CONVERT(SystemLockIndicator, to_system_lock_indicator, is_system_lock_indicator)
CONVERT(Segment, to_segment, is_segment)
CONVERT(System, to_system, is_system)
CONVERT(Lyrics, to_lyrics, is_lyrics)
CONVERT(Stem, to_stem, is_stem)
CONVERT(Beam, to_beam, is_beam)
CONVERT(Hook, to_hook, is_hook)
CONVERT(StemSlash, to_stem_slash, is_stem_slash)
CONVERT(SLine, to_s_line, is_s_line)
CONVERT(LineSegment, to_line_segment, is_line_segment)
CONVERT(Slur, to_slur, is_slur)
CONVERT(SlurSegment, to_slur_segment, is_slur_segment)
CONVERT(Tie, to_tie, is_tie)
CONVERT(TieSegment, to_tie_segment, is_tie_segment)
CONVERT(SlurTie, to_slur_tie, is_slur_tie)
CONVERT(SlurTieSegment, to_slur_tie_segment, is_slur_tie_segment)
CONVERT(LaissezVibSegment, to_laissez_vib_segment, is_laissez_vib_segment)
CONVERT(PartialTieSegment, to_partial_tie_segment, is_partial_tie_segment)
CONVERT(Spacer, to_spacer, is_spacer)
CONVERT(StaffLines, to_staff_lines, is_staff_lines)
CONVERT(Ambitus, to_ambitus, is_ambitus)
CONVERT(Bracket, to_bracket, is_bracket)
CONVERT(InstrumentChange, to_instrument_change, is_instrument_change)
CONVERT(StaffTypeChange, to_staff_type_change, is_staff_type_change)
CONVERT(Text, to_text, is_text)
CONVERT(MeasureNumber, to_measure_number, is_measure_number)
CONVERT(MMRestRange, to_mm_rest_range, is_mm_rest_range)
CONVERT(Hairpin, to_hairpin, is_hairpin)
CONVERT(HairpinSegment, to_hairpin_segment, is_hairpin_segment)
CONVERT(TremoloBar, to_tremolo_bar, is_tremolo_bar)
CONVERT(MeasureRepeat, to_measure_repeat, is_measure_repeat)
CONVERT(MMRest, to_mm_rest, is_mm_rest)
CONVERT(Tuplet, to_tuplet, is_tuplet)
CONVERT(NoteDot, to_note_dot, is_note_dot)
CONVERT(Dynamic, to_dynamic, is_dynamic)
CONVERT(Expression, to_expression, is_expression)
CONVERT(InstrumentName, to_instrument_name, is_instrument_name)
CONVERT(Accidental, to_accidental, is_accidental)
CONVERT(TextLine, to_text_line, is_text_line)
CONVERT(TextLineSegment, to_text_line_segment, is_text_line_segment)
CONVERT(Pedal, to_pedal, is_pedal)
CONVERT(PedalSegment, to_pedal_segment, is_pedal_segment)
CONVERT(OttavaSegment, to_ottava_segment, is_ottava_segment)
CONVERT(LedgerLine, to_ledger_line, is_ledger_line)
CONVERT(Volta, to_volta, is_volta)
CONVERT(VoltaSegment, to_volta_segment, is_volta_segment)
CONVERT(NoteLine, to_note_line, is_note_line)
CONVERT(NoteLineSegment, to_note_line_segment, is_note_line_segment)
CONVERT(Trill, to_trill, is_trill)
CONVERT(TrillSegment, to_trill_segment, is_trill_segment)
CONVERT(LetRing, to_let_ring, is_let_ring)
CONVERT(LetRingSegment, to_let_ring_segment, is_let_ring_segment)
CONVERT(GradualTempoChange, to_gradual_tempo_change, is_gradual_tempo_change)
CONVERT(GradualTempoChangeSegment, to_gradual_tempo_change_segment, is_gradual_tempo_change_segment)
CONVERT(Vibrato, to_vibrato, is_vibrato)
CONVERT(VibratoSegment, to_vibrato_segment, is_vibrato_segment)
CONVERT(PalmMute, to_palm_mute, is_palm_mute)
CONVERT(PalmMuteSegment, to_palm_mute_segment, is_palm_mute_segment)
CONVERT(WhammyBar, to_whammy_bar, is_whammy_bar)
CONVERT(WhammyBarSegment, to_whammy_bar_segment, is_whammy_bar_segment)
CONVERT(Rasgueado, to_rasgueado, is_rasgueado)
CONVERT(RasgueadoSegment, to_rasgueado_segment, is_rasgueado_segment)
CONVERT(HarmonicMark, to_harmonic_mark, is_harmonic_mark)
CONVERT(HarmonicMarkSegment, to_harmonic_mark_segment, is_harmonic_mark_segment)
CONVERT(PickScrape, to_pick_scrape, is_pick_scrape)
CONVERT(PickScrapeSegment, to_pick_scrape_segment, is_pick_scrape_segment)
CONVERT(Symbol, to_symbol, is_symbol)
CONVERT(FSymbol, to_f_symbol, is_f_symbol)
CONVERT(BSymbol, to_b_symbol, is_b_symbol)
CONVERT(Fingering, to_fingering, is_fingering)
CONVERT(NoteHead, to_note_head, is_note_head)
CONVERT(LyricsLine, to_lyrics_line, is_lyrics_line)
CONVERT(LyricsLineSegment, to_lyrics_line_segment, is_lyrics_line_segment)
CONVERT(FiguredBass, to_figured_bass, is_figured_bass)
CONVERT(FiguredBassItem, to_figured_bass_item, is_figured_bass_item)
CONVERT(StaffState, to_staff_state, is_staff_state)
CONVERT(ChordBracket, to_chord_bracket, is_chord_bracket)
CONVERT(Image, to_image, is_image)
CONVERT(ChordLine, to_chord_line, is_chord_line)
CONVERT(FretDiagram, to_fret_diagram, is_fret_diagram)
CONVERT(HarpPedalDiagram, to_harp_pedal_diagram, is_harp_pedal_diagram)
CONVERT(Page, to_page, is_page)
CONVERT(PageLockIndicator, to_page_lock_indicator, is_page_lock_indicator)
CONVERT(SystemText, to_system_text, is_system_text)
CONVERT(BracketItem, to_bracket_item, is_bracket_item)
CONVERT(Staff, to_staff, is_staff)
CONVERT(Part, to_part, is_part)
CONVERT(SharedPart, to_shared_part, is_shared_part)
CONVERT(BagpipeEmbellishment, to_bagpipe_embellishment, is_bagpipe_embellishment)
CONVERT(Sticking, to_sticking, is_sticking)
CONVERT(GraceNotesGroup, to_grace_notes_group, is_grace_notes_group)
CONVERT(DeadSlapped, to_dead_slapped, is_dead_slapped)
CONVERT(StringTunings, to_string_tunings, is_string_tunings)
CONVERT(SoundFlag, to_sound_flag, is_sound_flag)
CONVERT(TimeTickAnchor, to_time_tick_anchor, is_time_tick_anchor)
CONVERT(LaissezVib, to_laissez_vib, is_laissez_vib)
CONVERT(PartialTie, to_partial_tie, is_partial_tie)
CONVERT(PartialLyricsLine, to_partial_lyrics_line, is_partial_lyrics_line)
CONVERT(PartialLyricsLineSegment, to_partial_lyrics_line_segment, is_partial_lyrics_line_segment)
CONVERT(Parenthesis, to_parenthesis, is_parenthesis)
CONVERT(HammerOnPullOff, to_hammer_on_pull_off, is_hammer_on_pull_off)
CONVERT(HammerOnPullOffSegment, to_hammer_on_pull_off_segment, is_hammer_on_pull_off_segment)
CONVERT(HammerOnPullOffText, to_hammer_on_pull_off_text, is_hammer_on_pull_off_text)
CONVERT(Tapping, to_tapping, is_tapping)
CONVERT(TappingHalfSlur, to_tapping_half_slur, is_tapping_half_slur)
CONVERT(TappingHalfSlurSegment, to_tapping_half_slur_segment, is_tapping_half_slur_segment)
#undef CONVERT
} // namespace mu::engraving
