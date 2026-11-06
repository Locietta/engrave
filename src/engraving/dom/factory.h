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

#include <memory>

#include "engravingitem.h"

namespace mu::engraving {
class Instrument;
class RangeLock;
class TremoloSingleChord;
class TremoloTwoChord;

enum class SegmentType;
enum class TripletFeelType : unsigned char;

class Factory {
public:

    static EngravingItem *create_item(ElementType type, DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static EngravingItem *create_item_by_name(const AsciiStringView &name, DummyParentOr<EngravingItem> parent,
                                              bool is_accessible_enabled = true);

    static Accidental *create_accidental(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Accidental> make_accidental(DummyParentOr<EngravingItem> parent);

    static Ambitus *create_ambitus(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Ambitus> make_ambitus(DummyParentOr<Segment> parent);

    static Arpeggio *create_arpeggio(DummyParentOr<Chord> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Arpeggio> make_arpeggio(DummyParentOr<Chord> parent);

    static ChordBracket *create_chord_bracket(DummyParentOr<Chord> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<ChordBracket> make_chord_bracket(DummyParentOr<Chord> parent);

    static Articulation *create_articulation(DummyParentOr<ChordRest> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Articulation> make_articulation(DummyParentOr<ChordRest> parent);

    static Tapping *create_tapping(DummyParentOr<ChordRest> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Tapping> make_tapping(DummyParentOr<ChordRest> parent);

    static Ornament *create_ornament(DummyParentOr<ChordRest> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Ornament> make_ornament(DummyParentOr<ChordRest> parent);

    static BarLine *create_bar_line(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static BarLine *copy_bar_line(const BarLine &src);
    static std::shared_ptr<BarLine> make_bar_line(DummyParentOr<Segment> parent);

    static Beam *create_beam(Score *parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Beam> make_beam(Score *parent);

    static Bend *create_bend(DummyParentOr<Note> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Bend> make_bend(DummyParentOr<Note> parent);

    static Bracket *create_bracket(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Bracket> make_bracket(DummyParentOr<EngravingItem> parent);
    static BracketItem *create_bracket_item(DummyParentOr<EngravingItem> parent);
    static BracketItem *create_bracket_item(DummyParentOr<EngravingItem> parent, BracketType bracket_type, size_t span);

    static Breath *create_breath(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Breath> make_breath(DummyParentOr<Segment> parent);

    static Chord *create_chord(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static Chord *copy_chord(const Chord &src, bool link = false);
    static std::shared_ptr<Chord> make_chord(DummyParentOr<Segment> parent);

    static ChordLine *create_chord_line(DummyParentOr<Chord> parent, bool is_accessible_enabled = true);
    static ChordLine *copy_chord_line(const ChordLine &src);
    static std::shared_ptr<ChordLine> make_chord_line(DummyParentOr<Chord> parent);

    static Clef *create_clef(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static Clef *copy_clef(const Clef &src);
    static std::shared_ptr<Clef> make_clef(DummyParentOr<Segment> parent);

    static Fermata *create_fermata(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Fermata> make_fermata(DummyParentOr<Segment> parent);

    static FiguredBass *create_figured_bass(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<FiguredBass> make_figured_bass(DummyParentOr<Segment> parent);

    static FretDiagram *create_fret_diagram(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static FretDiagram *copy_fret_diagram(const FretDiagram &src);
    static std::shared_ptr<FretDiagram> make_fret_diagram(DummyParentOr<Segment> parent);

    static HarpPedalDiagram *create_harp_pedal_diagram(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static HarpPedalDiagram *copy_harp_pedal_diagram(const HarpPedalDiagram &src);
    static std::shared_ptr<HarpPedalDiagram> make_harp_pedal_diagram(DummyParentOr<Segment> parent);

    static KeySig *create_key_sig(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static KeySig *copy_key_sig(const KeySig &src);
    static std::shared_ptr<KeySig> make_key_sig(DummyParentOr<Segment> parent);

    static LaissezVib *create_laissez_vib(DummyParentOr<Note> parent, bool is_accessible_enabled = true);
    static LaissezVib *copy_laissez_vib(const LaissezVib &src);

    static LayoutBreak *create_layout_break(DummyParentOr<MeasureBase> parent, bool is_accessible_enabled = true);
    static LayoutBreak *copy_layout_break(const LayoutBreak &src);
    static std::shared_ptr<LayoutBreak> make_layout_break(DummyParentOr<MeasureBase> parent);

    static StaffVisibilityIndicator *create_staff_visibility_indicator(DummyParentOr<System> parent, bool is_accessible_enabled = true);

    static SystemLockIndicator *create_system_lock_indicator(DummyParentOr<System> parent, const RangeLock *lock,
                                                             bool is_accessible_enabled = true);
    static SystemLockIndicator *copy_system_lock_indicator(const SystemLockIndicator &src);

    static Lyrics *create_lyrics(DummyParentOr<ChordRest> parent, bool is_accessible_enabled = true);
    static Lyrics *copy_lyrics(const Lyrics &src);

    static LyricsLine *create_lyrics_line(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static LyricsLine *copy_lyrics_line(const LyricsLine &src);

    static Measure *create_measure(Score *parent, bool is_accessible_enabled = true);
    static Measure *copy_measure(const Measure &src);

    static MeasureRepeat *create_measure_repeat(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static MeasureRepeat *copy_measure_repeat(const MeasureRepeat &src);

    static Note *create_note(DummyParentOr<Chord> parent, bool is_accessible_enabled = true);
    static Note *copy_note(const Note &src, bool link = false);
    static std::shared_ptr<Note> make_note(DummyParentOr<Chord> parent);

    static NoteDot *create_note_dot(DummyParentOr<Note> parent, bool is_accessible_enabled = true);
    static NoteDot *create_note_dot(Rest *parent, bool is_accessible_enabled = true);
    static NoteDot *copy_note_dot(const NoteDot &src);

    static NoteLine *create_note_line(DummyParentOr<Note> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<NoteLine> make_note_line(DummyParentOr<Note> parent);

    static Page *create_page(Score *parent, bool is_accessible_enabled = true);

    static PageLockIndicator *create_page_lock_indicator(DummyParentOr<System> parent, const RangeLock *lock,
                                                         bool is_accessible_enabled = true);
    static PageLockIndicator *copy_page_lock_indicator(const PageLockIndicator &src);

    static Parenthesis *create_parenthesis(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static Parenthesis *copy_parenthesis(const Parenthesis &src);

    static PartialTie *create_partial_tie(DummyParentOr<Note> parent, bool is_accessible_enabled = true);
    static PartialTie *copy_partial_tie(const PartialTie &src);

    static PartialLyricsLine *create_partial_lyrics_line(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static PartialLyricsLine *copy_partial_lyrics_line(const PartialLyricsLine &src);

    static Rest *create_rest(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static Rest *create_rest(DummyParentOr<Segment> parent, const TDuration &t, bool is_accessible_enabled = true);
    static Rest *copy_rest(const Rest &src, bool link = false);
    static MMRest *create_mm_rest(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);

    static DeadSlapped *create_dead_slapped(Rest *parent, bool is_accessible_enabled = true);
    static DeadSlapped *copy_dead_slapped(const DeadSlapped &src);

    static Segment *create_segment(DummyParentOr<Measure> parent, bool is_accessible_enabled = true);
    static Segment *create_segment(DummyParentOr<Measure> parent, SegmentType type, const Fraction &t, bool is_accessible_enabled = true);

    static Slur *create_slur(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Slur> make_slur(DummyParentOr<EngravingItem> parent);

    static Spacer *create_spacer(DummyParentOr<Measure> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Spacer> make_spacer(DummyParentOr<Measure> parent);

    static Staff *create_staff(Part *parent);

    static StaffLines *create_staff_lines(DummyParentOr<Measure> parent, bool is_accessible_enabled = true);
    static StaffLines *copy_staff_lines(const StaffLines &src);

    static StaffState *create_staff_state(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static StaffTypeChange *create_staff_type_change(DummyParentOr<MeasureBase> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<StaffTypeChange> make_staff_type_change(DummyParentOr<MeasureBase> parent);

    static StaffText *create_staff_text(DummyParentOr<Segment> parent, TextStyleType text_style_type = TextStyleType::STAFF,
                                        bool is_accessible_enabled = true);
    static StaveSharingLabel *create_stave_sharing_label(DummyParentOr<Segment> parent,
                                                         TextStyleType text_style_type = TextStyleType::STAVE_SHARING,
                                                         bool is_accessible_enabled = true);

    static SoundFlag *create_sound_flag(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static Expression *create_expression(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);

    static RehearsalMark *create_rehearsal_mark(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);

    static Stem *create_stem(DummyParentOr<Chord> parent, bool is_accessible_enabled = true);
    static Stem *copy_stem(const Stem &src);

    static StemSlash *create_stem_slash(DummyParentOr<Chord> parent, bool is_accessible_enabled = true);
    static StemSlash *copy_stem_slash(const StemSlash &src);

    static System *create_system(Score *parent, bool is_accessible_enabled = true);
    static SystemText *create_system_text(DummyParentOr<Segment> parent, TextStyleType text_style_type = TextStyleType::SYSTEM,
                                          ElementType type = ElementType::SYSTEM_TEXT, bool is_accessible_enabled = true);

    static InstrumentChange *create_instrument_change(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static InstrumentChange *create_instrument_change(DummyParentOr<Segment> parent, const Instrument &instrument,
                                                      bool is_accessible_enabled = true);

    static Sticking *create_sticking(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);

    static Fingering *create_fingering(DummyParentOr<Note> parent, bool is_accessible_enabled = true);
    static Fingering *create_fingering(DummyParentOr<Note> parent, TextStyleType text_style_type, bool is_accessible_enabled = true);

    static Harmony *create_harmony(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static TempoText *create_tempo_text(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);

    static Text *create_text(DummyParentOr<EngravingItem> parent, TextStyleType tid = TextStyleType::DEFAULT,
                             bool is_accessible_enabled = true);
    static Text *copy_text(const Text &src);

    static Tie *create_tie(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static Tie *copy_tie(const Tie &src);

    static TimeSig *create_time_sig(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static TimeSig *copy_time_sig(const TimeSig &src);
    static std::shared_ptr<TimeSig> make_time_sig(DummyParentOr<Segment> parent);

    static TremoloTwoChord *create_tremolo_two_chord(DummyParentOr<Chord> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<TremoloTwoChord> make_tremolo_two_chord(DummyParentOr<Chord> parent);
    static TremoloTwoChord *copy_tremolo_two_chord(const TremoloTwoChord &src);

    static TremoloSingleChord *create_tremolo_single_chord(DummyParentOr<Chord> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<TremoloSingleChord> make_tremolo_single_chord(DummyParentOr<Chord> parent);
    static TremoloSingleChord *copy_tremolo_single_chord(const TremoloSingleChord &src);

    static TremoloBar *create_tremolo_bar(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<TremoloBar> make_tremolo_bar(DummyParentOr<EngravingItem> parent);

    static TripletFeel *create_triplet_feel(DummyParentOr<Segment> parent, TripletFeelType type, bool is_accessible_enabled = true);

    static Tuplet *create_tuplet(DummyParentOr<Measure> parent, bool is_accessible_enabled = true);
    static Tuplet *copy_tuplet(const Tuplet &src);

    static Hairpin *create_hairpin(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Hairpin> make_hairpin(DummyParentOr<EngravingItem> parent);

    static HammerOnPullOff *create_hammer_on_pull_off(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<HammerOnPullOff> make_hammer_on_pull_off(DummyParentOr<EngravingItem> parent);

    static Glissando *create_glissando(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Glissando> make_glissando(DummyParentOr<EngravingItem> parent);

    static GuitarBend *create_guitar_bend(DummyParentOr<Note> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<GuitarBend> make_guitar_bend(DummyParentOr<Note> parent);

    static Jump *create_jump(DummyParentOr<Measure> parent, bool is_accessible_enabled = true);

    static Trill *create_trill(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static Vibrato *create_vibrato(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static TextLine *create_text_line(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<TextLine> make_text_line(DummyParentOr<EngravingItem> parent);

    static Ottava *create_ottava(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static LetRing *create_let_ring(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static Marker *create_marker(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static Marker *create_marker(DummyParentOr<EngravingItem> parent, TextStyleType tid, bool is_accessible_enabled = true);

    static std::shared_ptr<Marker> make_marker(DummyParentOr<EngravingItem> parent);

    static GradualTempoChange *create_gradual_tempo_change(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static PalmMute *create_palm_mute(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static WhammyBar *create_whammy_bar(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<WhammyBar> make_whammy_bar(DummyParentOr<EngravingItem> parent);

    static Rasgueado *create_rasgueado(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static HarmonicMark *create_harmonic_mark(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static PickScrape *create_pick_scrape(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static Volta *create_volta(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static Pedal *create_pedal(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static Dynamic *create_dynamic(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);

    static VBox *create_v_box(Score *parent, bool is_accessible_enabled = true);

    static VBox *create_v_box(const ElementType &type, Score *parent, bool is_accessible_enabled = true);

    static VBox *create_title_v_box(Score *parent, bool is_accessible_enabled = true);

    static HBox *create_h_box(Score *parent, bool is_accessible_enabled = true);

    static TBox *create_t_box(Score *parent, bool is_accessible_enabled = true);

    static FBox *create_f_box(Score *parent, bool is_accessible_enabled = true);

    static Image *create_image(DummyParentOr<EngravingItem> parent);

    static Symbol *create_symbol(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);
    static FSymbol *create_f_symbol(DummyParentOr<EngravingItem> parent, bool is_accessible_enabled = true);

    static PlayCountText *create_play_count_text(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);

    static PlayTechAnnotation *create_play_tech_annotation(DummyParentOr<Segment> parent, PlayingTechniqueType technique_type,
                                                           TextStyleType style_type, bool is_accessible_enabled = true);

    static Capo *create_capo(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static std::shared_ptr<Capo> make_capo(DummyParentOr<Segment> parent);

    static TimeTickAnchor *create_time_tick_anchor(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);

    static StringTunings *create_string_tunings(DummyParentOr<Segment> parent, bool is_accessible_enabled = true);
    static StringTunings *copy_string_tunings(const StringTunings &src);
    static std::shared_ptr<StringTunings> make_string_tunings(DummyParentOr<Segment> parent);

private:
    static EngravingItem *do_create_item(ElementType type, DummyParentOr<EngravingItem> parent);
};
} // namespace mu::engraving
