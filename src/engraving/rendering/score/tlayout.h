/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include "layoutcontext.h"

#include "../../dom/accidental.h"
#include "../../dom/actionicon.h"
#include "../../dom/ambitus.h"
#include "../../dom/arpeggio.h"
#include "../../dom/articulation.h"

#include "../../dom/barline.h"
#include "../../dom/bend.h"
#include "../../dom/box.h"
#include "../../dom/bracket.h"
#include "../../dom/breath.h"

#include "../../dom/chordbracket.h"
#include "../../dom/chordline.h"
#include "../../dom/clef.h"
#include "../../dom/capo.h"
#include "../../dom/chord.h"

#include "../../dom/deadslapped.h"
#include "../../dom/dynamic.h"

#include "../../dom/expression.h"

#include "../../dom/fermata.h"
#include "../../dom/figuredbass.h"
#include "../../dom/fingering.h"
#include "../../dom/fret.h"

#include "../../dom/guitarbend.h"

#include "../../dom/harppedaldiagram.h"
#include "../../dom/harmony.h"
#include "../../dom/hairpin.h"
#include "../../dom/hook.h"

#include "../../dom/image.h"
#include "../../dom/instrchange.h"
#include "../../dom/instrumentname.h"

#include "../../dom/jump.h"

#include "../../dom/keysig.h"

#include "../../dom/laissezvib.h"
#include "../../dom/layoutbreak.h"

#include "../../dom/marker.h"
#include "../../dom/measurebase.h"
#include "../../dom/measurenumber.h"
#include "../../dom/measurerepeat.h"
#include "../../dom/mmrest.h"
#include "../../dom/mmrestrange.h"

#include "../../dom/note.h"
#include "../../dom/notedot.h"

#include "../../dom/pagelockindicator.h"
#include "../../dom/parenthesis.h"
#include "../../dom/playcounttext.h"
#include "../../dom/playtechannotation.h"

#include "../../dom/rehearsalmark.h"
#include "../../dom/rest.h"

#include "../../dom/staffstate.h"
#include "../../dom/stafftext.h"
#include "../../dom/stafftype.h"
#include "../../dom/stafftypechange.h"
#include "../../dom/stavesharinglabel.h"
#include "../../dom/stem.h"
#include "../../dom/stemslash.h"
#include "../../dom/sticking.h"
#include "../../dom/systemdivider.h"
#include "../../dom/systemtext.h"
#include "../../dom/soundflag.h"

#include "../../dom/tabdurationsymbol.h"
#include "../../dom/tapping.h"
#include "../../dom/textbase.h"
#include "../../dom/tempotext.h"
#include "../../dom/text.h"
#include "../../dom/timesig.h"
#include "../../dom/anchors.h"
#include "../../dom/tremolobar.h"
#include "../../dom/tripletfeel.h"
#include "../../dom/trill.h"
#include "../../dom/tuplet.h"

#include "../../dom/ornament.h"

namespace mu::engraving {
struct EngravingItem;

struct BagpipeEmbellishment;
struct Beam;

struct Chord;

struct Glissando;
struct GlissandoSegment;
struct GradualTempoChangeSegment;
struct GradualTempoChange;

struct HairpinSegment;
struct Hairpin;
struct HammerOnPullOff;
struct HammerOnPullOffSegment;
struct HammerOnPullOffText;
struct HarmonicMarkSegment;

struct LedgerLine;
struct LetRing;
struct LetRingSegment;
struct LineSegment;
struct Lyrics;
struct LyricsLine;
struct LyricsLineSegment;

struct Ottava;
struct OttavaSegment;

struct PalmMute;
struct PalmMuteSegment;
struct Parenthesis;
struct Pedal;
struct PedalSegment;
struct PickScrapeSegment;

struct NoteLineSegment;

struct RasgueadoSegment;

struct ShadowNote;
struct SLine;
struct Slur;
struct Spacer;
struct SpannerSegment;
struct StaffLines;
struct StringTunings;

struct BSymbol;
struct Symbol;
struct FSymbol;

struct SystemDivider;
struct SystemText;

struct TabDurationSymbol;
struct Tapping;
struct TempoText;
struct Text;
struct TextLine;
struct TextLineSegment;
struct TextLineBase;
struct TextLineBaseSegment;
struct Tie;
struct TimeSig;
struct TimeTickAnchor;
struct TremoloSingleChord;
struct TremoloTwoChord;
struct TremoloBar;
struct TrillSegment;
struct TripletFeel;
struct Trill;
struct Tuplet;

struct VibratoSegment;
struct Vibrato;
struct Volta;
struct VoltaSegment;

struct WhammyBarSegment;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct TLayout {

    static void layout_item(EngravingItem *item, LayoutContext &ctx); // factory

    static void layout_accidental(const Accidental *item, Accidental::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_action_icon(const ActionIcon *item, ActionIcon::LayoutData *ldata);
    static void layout_ambitus(const Ambitus *item, Ambitus::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_arpeggio(const Arpeggio *item, Arpeggio::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_chord_bracket(const ChordBracket *item, ChordBracket::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_articulation(Articulation *item, Articulation::LayoutData *ldata);
    static void fill_articulation_shape(const Articulation *item, Articulation::LayoutData *ldata);

    static void layout_bar_line(const BarLine *item, BarLine::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_bar_line2(BarLine *item, LayoutContext &ctx);
    static void layout_beam(Beam *item, const LayoutContext &ctx);
    static void layout_beam1(Beam *item, LayoutContext &ctx);
    static void layout_bend(const Bend *item, Bend::LayoutData *ldata);

    static void layout_h_box(const HBox *item, HBox::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_v_box(const VBox *item, VBox::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_f_box(const FBox *item, FBox::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_t_box(const TBox *item, TBox::LayoutData *ldata, const LayoutContext &ctx);

    static void layout_bracket(const Bracket *item, Bracket::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_breath(const Breath *item, Breath::LayoutData *ldata, const LayoutConfiguration &conf);

    static void layout_chord(Chord *item, LayoutContext &ctx);
    static void layout_chord_line(const ChordLine *item, ChordLine::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_clef(const Clef *item, Clef::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_capo(const Capo *item, Capo::LayoutData *ldata, const LayoutContext &ctx);

    static void layout_dead_slapped(const DeadSlapped *item, DeadSlapped::LayoutData *ldata);
    static void layout_dynamic(Dynamic *item, Dynamic::LayoutData *ldata, const LayoutConfiguration &conf);

    static void layout_expression(const Expression *item, Expression::LayoutData *ldata);

    static void layout_fermata(const Fermata *item, Fermata::LayoutData *ldata);
    static void layout_figured_bass(const FiguredBass *item, FiguredBass::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_fingering(const Fingering *item, Fingering::LayoutData *ldata);
    static void layout_fret_diagram(const FretDiagram *item, FretDiagram::LayoutData *ldata, const LayoutContext &ctx);

    static void layout_glissando(Glissando *item, LayoutContext &ctx);
    static void layout_glissando_segment(GlissandoSegment *item, LayoutContext &ctx);
    static void layout_grace_notes_group(GraceNotesGroup *item, LayoutContext &ctx);
    static void layout_grace_notes_group2(const GraceNotesGroup *item, GraceNotesGroup::LayoutData *ldata);
    static void layout_gradual_tempo_change_segment(GradualTempoChangeSegment *item, LayoutContext &ctx);
    static void layout_gradual_tempo_change(GradualTempoChange *item, LayoutContext &ctx);
    static void layout_guitar_bend(GuitarBend *item, LayoutContext &ctx, System *system = nullptr);
    static void layout_guitar_bend_segment(GuitarBendSegment *item, LayoutContext &ctx);
    static void fill_guitar_bend_segment_shape(const GuitarBendSegment *item, GuitarBendSegment::LayoutData *ldata);

    static void layout_hairpin_segment(HairpinSegment *item, LayoutContext &ctx);
    static void layout_hairpin(Hairpin *item, LayoutContext &ctx);
    static void layout_hammer_on_pull_off(HammerOnPullOff *item, LayoutContext &ctx);
    static void layout_hammer_on_pull_off_segment(HammerOnPullOffSegment *item, LayoutContext &ctx);
    static void layout_hammer_on_pull_off_text(HammerOnPullOffText *item, LayoutContext &ctx);
    static void layout_harp_pedal_diagram(const HarpPedalDiagram *item, HarpPedalDiagram::LayoutData *ldata);
    static void layout_harmonic_mark_segment(HarmonicMarkSegment *item, LayoutContext &ctx);
    static void layout_harmony(Harmony *item, Harmony::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_hook(const Hook *item, Hook::LayoutData *ldata);

    static void layout_indicator_icon(const IndicatorIcon *item, IndicatorIcon::LayoutData *ldata);

    static void layout_image(const Image *item, Image::LayoutData *ldata);
    static void layout_instrument_change(const InstrumentChange *item, InstrumentChange::LayoutData *ldata);
    static void layout_instrument_name(const InstrumentName *item, InstrumentName::LayoutData *ldata);

    static void layout_jump(const Jump *item, Jump::LayoutData *ldata);

    static void layout_key_sig(const KeySig *item, KeySig::LayoutData *ldata, const LayoutConfiguration &conf);

    static void layout_laissez_vib(LaissezVib *item);
    static void layout_layout_break(const LayoutBreak *item, LayoutBreak::LayoutData *ldata);
    static void layout_ledger_line(LedgerLine *item, LayoutContext &ctx);
    static void layout_let_ring(LetRing *item, LayoutContext &ctx);
    static void layout_let_ring_segment(LetRingSegment *item, LayoutContext &ctx);
    static void layout_line_segment(LineSegment *item, LayoutContext &ctx); // factory
    static void layout_lyrics(Lyrics *item, LayoutContext &ctx);
    static void layout_lyrics_line(LyricsLine *item);
    static void layout_lyrics_line_segment(LyricsLineSegment *item, LayoutContext &ctx);

    static void layout_marker(Marker *item, Marker::LayoutData *ldata, LayoutContext &ctx);
    static void layout_measure_base(MeasureBase *item, LayoutContext &ctx);                                                  // factory
    static void layout_base_measure_base(const MeasureBase *item, MeasureBase::LayoutData *ldata, const LayoutContext &ctx); // base class
    static void layout_measure_number(MeasureNumber *item, MeasureNumber::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_measure_repeat(const MeasureRepeat *item, MeasureRepeat::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_measure_repeat_extender(const MeasureRepeat *item, MeasureRepeat::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_mm_rest(const MMRest *item, MMRest::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_mm_rest_range(MMRestRange *item, MMRestRange::LayoutData *ldata, const LayoutContext &ctx);

    static void layout_note(const Note *item, Note::LayoutData *ldata);
    static void fill_note_shape(const Note *item, Note::LayoutData *ldata);
    static void layout_note_dot(const NoteDot *item, NoteDot::LayoutData *ldata);
    static void layout_note_anchored_line(SLine *item, SLine::LayoutData *ldata, LayoutContext &ctx);
    static void layout_note_line(NoteLine *item, LayoutContext &ctx);
    static void layout_note_line_segment(NoteLineSegment *item, LayoutContext &ctx);

    static void layout_ornament(const Ornament *item, Ornament::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_ornament_cue_note(Ornament *item, LayoutContext &ctx);

    static void layout_ottava(Ottava *item, LayoutContext &ctx);
    static void layout_ottava_segment(OttavaSegment *item, LayoutContext &ctx);

    static void layout_page_lock_indicator(const PageLockIndicator *item, PageLockIndicator::LayoutData *ldata);
    static void layout_palm_mute(PalmMute *item, LayoutContext &ctx);
    static void layout_palm_mute_segment(PalmMuteSegment *item, LayoutContext &ctx);
    static void layout_parenthesis(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_pedal(Pedal *item, LayoutContext &ctx);
    static void layout_pedal_segment(PedalSegment *item, LayoutContext &ctx);
    static void layout_pick_scrape_segment(PickScrapeSegment *item, LayoutContext &ctx);
    static void layout_play_count_text(PlayCountText *item, PlayCountText::LayoutData *ldata);
    static void layout_play_tech_annotation(const PlayTechAnnotation *item, PlayTechAnnotation::LayoutData *ldata);

    static void layout_rasgueado_segment(RasgueadoSegment *item, LayoutContext &ctx);
    static void layout_rehearsal_mark(const RehearsalMark *item, RehearsalMark::LayoutData *ldata);
    static void layout_rest(const Rest *item, Rest::LayoutData *ldata, const LayoutContext &ctx);

    static void layout_shadow_note(ShadowNote *item, LayoutContext &ctx);
    static void layout_line(SLine *item, LayoutContext &ctx); // base class
    static void layout_slur(Slur *item, LayoutContext &ctx);
    static void layout_spacer(Spacer *item, LayoutContext &ctx);
    static void layout_spanner(Spanner *item, LayoutContext &ctx);
    static void layout_staff_lines(StaffLines *item, LayoutContext &ctx);
    static void layout_for_width(StaffLines *item, double w, LayoutContext &ctx);
    static void layout_staff_state(const StaffState *item, StaffState::LayoutData *ldata);
    static void layout_staff_text(const StaffText *item, StaffText::LayoutData *ldata);
    static void layout_stave_sharing_label(const StaveSharingLabel *item, StaveSharingLabel::LayoutData *ldata);
    static void layout_staff_type_change(const StaffTypeChange *item, StaffTypeChange::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_stem(const Stem *item, Stem::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_stem_slash(const StemSlash *item, StemSlash::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_sticking(const Sticking *item, Sticking::LayoutData *ldata);
    static void layout_string_tunings(StringTunings *item, LayoutContext &ctx);
    static void layout_sound_flag(const SoundFlag *item, SoundFlag::LayoutData *ldata);

    static void layout_symbol(const Symbol *item, Symbol::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_f_symbol(const FSymbol *item, FSymbol::LayoutData *ldata);

    static void layout_system_divider(const SystemDivider *item, SystemDivider::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_system_text(const SystemText *item, SystemText::LayoutData *ldata);

    static void layout_tab_duration_symbol(const TabDurationSymbol *item, TabDurationSymbol::LayoutData *ldata);
    static void layout_tab_duration_symbol2(const TabDurationSymbol *item, TabDurationSymbol::LayoutData *ldata);
    static void layout_tapping(Tapping *item, Tapping::LayoutData *ldata, LayoutContext &ctx);
    static void layout_tapping_half_slur(TappingHalfSlur *item);
    static void layout_tempo_text(const TempoText *item, TempoText::LayoutData *ldata);

    static void layout_text(const Text *item, Text::LayoutData *ldata);

    static void layout_text_line(TextLine *item, LayoutContext &ctx);
    static void layout_text_line_segment(TextLineSegment *item, LayoutContext &ctx);
    static void layout_text_line_base(TextLineBase *item, LayoutContext &ctx);
    static void layout_text_line_base_segment(TextLineBaseSegment *item, LayoutContext &ctx); // base class
    static Shape recalculate_text_line_base_segment_shape(const TextLineBaseSegment *item);

    static void layout_tie(Tie *item, LayoutContext &ctx);
    static void layout_time_sig(const TimeSig *item, TimeSig::LayoutData *ldata, const LayoutContext &ctx);
    static void layout_time_tick_anchor(TimeTickAnchor *item, LayoutContext &);
    static void layout_tremolo_single(TremoloSingleChord *item, LayoutContext &ctx);
    static void layout_tremolo_two(TremoloTwoChord *item, LayoutContext &ctx);
    static void layout_tremolo_bar(const TremoloBar *item, TremoloBar::LayoutData *ldata);
    static void layout_trill(Trill *item, LayoutContext &ctx);
    static void layout_trill_segment(TrillSegment *item, LayoutContext &ctx);
    static void fill_trill_segment_shape(const TrillSegment *item, TrillSegment::LayoutData *ldata, const LayoutConfiguration &conf);
    static void layout_triplet_feel(const TripletFeel *item, TripletFeel::LayoutData *ldata);
    static void layout_tuplet(Tuplet *item, LayoutContext &ctx);
    static void fill_tuplet_shape(const Tuplet *item, Tuplet::LayoutData *ldata);

    static void layout_vibrato(Vibrato *item, LayoutContext &ctx);
    static void layout_vibrato_segment(VibratoSegment *item, LayoutContext &ctx);
    static void layout_volta(Volta *item, LayoutContext &ctx);
    static void layout_volta_segment(VoltaSegment *item, LayoutContext &ctx);

    static void layout_whammy_bar_segment(WhammyBarSegment *item, LayoutContext &ctx);

    static void update_barline_shape(const BarLine *item, BarLine::LayoutData *ldata, const LayoutContext &ctx);

    // layoutSystem;
    static SpannerSegment *layout_system(Spanner *item, System *system, LayoutContext &ctx); // factory
    static SpannerSegment *layout_system(LyricsLine *line, System *system, LayoutContext &ctx);
    static SpannerSegment *layout_system(Volta *line, System *system, LayoutContext &ctx);
    static SpannerSegment *layout_system(Slur *line, System *system, LayoutContext &ctx);

private:

    friend struct SlurTieLayout;

    static void layout_figured_bass_item(const FiguredBassItem *item, FiguredBassItem::LayoutData *ldata, const LayoutContext &ctx);

    static SpannerSegment *layout_system_s_line(SLine *line, System *system, LayoutContext &ctx);
    static double volta_mid_end_segment_start_x(Volta *volta, System *system, LayoutContext &ctx);
    static SpannerSegment *get_next_layout_system_segment(Spanner *spanner, System *system,
                                                          std::function<SpannerSegment *()> create_segment);

    static void manage_hairpin_snapping(HairpinSegment *item, LayoutContext &ctx);

    static void check_rehearsal_mark_vs_big_time_sig(const RehearsalMark *item, RehearsalMark::LayoutData *ldata);

    static void manage_tempo_change_snapping(GradualTempoChangeSegment *item, LayoutContext &ctx);
    static void do_layout_gradual_tempo_change_segment(GradualTempoChangeSegment *item, LayoutContext &ctx);

    static void align_tab_grace_notes_to_main_stave(GraceNotesGroup *grace_notes, const Staff *notation_staff);

    static void layout_group_bracket(const Bracket *item, Bracket::LayoutData *ldata, const LayoutConfiguration &conf);
};
} // namespace mu::engraving::rendering::score
