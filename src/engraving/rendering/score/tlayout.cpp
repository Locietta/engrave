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
#include "tlayout.h"

#include <cfloat>

#include "accidentalslayout.h"
#include "arpeggiolayout.h"
#include "autoplace.h"
#include "beamlayout.h"
#include "boxlayout.h"
#include "chordlayout.h"
#include "dom/accidental.h"
#include "dom/ambitus.h"
#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/barline.h"
#include "dom/beam.h"
#include "dom/bend.h"
#include "dom/box.h"
#include "dom/bracket.h"
#include "dom/breath.h"
#include "dom/capo.h"
#include "dom/chord.h"
#include "dom/chordline.h"
#include "dom/clef.h"
#include "dom/deadslapped.h"
#include "dom/dynamic.h"
#include "dom/expression.h"
#include "dom/factory.h"
#include "dom/fermata.h"
#include "dom/figuredbass.h"
#include "dom/fingering.h"
#include "dom/fret.h"
#include "dom/glissando.h"
#include "dom/gradualtempochange.h"
#include "dom/guitarbend.h"
#include "dom/hairpin.h"
#include "dom/hammeronpulloff.h"
#include "dom/harmonicmark.h"
#include "dom/harmony.h"
#include "dom/harppedaldiagram.h"
#include "dom/hook.h"
#include "dom/image.h"
#include "dom/instrchange.h"
#include "dom/instrumentname.h"
#include "dom/jump.h"
#include "dom/keysig.h"
#include "dom/layoutbreak.h"
#include "dom/ledgerline.h"
#include "dom/letring.h"
#include "dom/line.h"
#include "dom/linkedobjects.h"
#include "dom/lyrics.h"
#include "dom/marker.h"
#include "dom/measure.h"
#include "dom/measurebase.h"
#include "dom/measurenumber.h"
#include "dom/measurenumberbase.h"
#include "dom/measurerepeat.h"
#include "dom/mmrest.h"
#include "dom/mmrestrange.h"
#include "dom/note.h"
#include "dom/notedot.h"
#include "dom/noteline.h"
#include "dom/ornament.h"
#include "dom/ottava.h"
#include "dom/page.h"
#include "dom/palmmute.h"
#include "dom/parenthesis.h"
#include "dom/part.h"
#include "dom/pedal.h"
#include "dom/pickscrape.h"
#include "dom/playtechannotation.h"
#include "dom/rasgueado.h"
#include "dom/rehearsalmark.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/slur.h"
#include "dom/soundflag.h"
#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/stafflines.h"
#include "dom/staffstate.h"
#include "dom/stafftext.h"
#include "dom/stafftype.h"
#include "dom/stafftypechange.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/sticking.h"
#include "dom/stringtunings.h"
#include "dom/symbol.h"
#include "dom/system.h"
#include "dom/systemdivider.h"
#include "dom/systemlockindicator.h"
#include "dom/systemtext.h"
#include "dom/tapping.h"
#include "dom/tempotext.h"
#include "dom/text.h"
#include "dom/textline.h"
#include "dom/tie.h"
#include "dom/timesig.h"
#include "dom/tremolobar.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/trill.h"
#include "dom/tripletfeel.h"
#include "dom/tuplet.h"
#include "dom/utils.h"
#include "dom/vibrato.h"
#include "dom/volta.h"
#include "dom/whammybar.h"
#include "draw/fontmetrics.h"
#include "dynamicslayout.h"
#include "editing/editchord.h"
#include "editing/navigation.h"
#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "global/realfn.h"
#include "global/types/number.h"
#include "guitarbendlayout.h"
#include "harmonylayout.h"
#include "horizontalspacing.h"
#include "iengravingfont.h"
#include "infrastructure/ld_access.h"
#include "infrastructure/rtti.h"
#include "log.h"
#include "lyricslayout.h"
#include "markerlayout.h"
#include "masklayout.h"
#include "measurelayout.h"
#include "measurenumberlayout.h"
#include "parenthesislayout.h"
#include "restlayout.h"
#include "slurtielayout.h"
#include "tappinglayout.h"
#include "textlayout.h"
#include "tremololayout.h"
#include "tupletlayout.h"
#include "types/symnames.h"
#include "types/typesconv.h"

using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rtti;
using namespace mu::engraving::rendering::score;

#define LAYOUT_CALL_ITEM(item) LAYOUT_CALL() << LAYOUT_ITEM_INFO(item);

void TLayout::layout_item(EngravingItem *item, LayoutContext &ctx) {

    EngravingItem::LayoutData *ldata = item->mutldata();

    switch (item->type()) {
        case ElementType::ACCIDENTAL:
            layout_accidental(item_cast<const Accidental *>(item), static_cast<Accidental::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::AMBITUS: layout_ambitus(item_cast<const Ambitus *>(item), static_cast<Ambitus::LayoutData *>(ldata), ctx); break;
        case ElementType::ARPEGGIO:
            layout_arpeggio(item_cast<const Arpeggio *>(item), static_cast<Arpeggio::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::CHORD_BRACKET:
            layout_chord_bracket(item_cast<const ChordBracket *>(item), static_cast<ChordBracket::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::ARTICULATION:
            layout_articulation(item_cast<Articulation *>(item), static_cast<Articulation::LayoutData *>(ldata));
            break;
        case ElementType::BAR_LINE:
            layout_bar_line(item_cast<const BarLine *>(item), static_cast<BarLine::LayoutData *>(ldata), ctx);
            break;
        case ElementType::BEAM: layout_beam(item_cast<Beam *>(item), ctx); break;
        case ElementType::BEND: layout_bend(item_cast<const Bend *>(item), static_cast<Bend::LayoutData *>(ldata)); break;
        case ElementType::HBOX: layout_h_box(item_cast<const HBox *>(item), static_cast<HBox::LayoutData *>(ldata), ctx); break;
        case ElementType::VBOX: layout_v_box(item_cast<const VBox *>(item), static_cast<VBox::LayoutData *>(ldata), ctx); break;
        case ElementType::FBOX: layout_f_box(item_cast<const FBox *>(item), static_cast<FBox::LayoutData *>(ldata), ctx); break;
        case ElementType::TBOX: layout_t_box(item_cast<const TBox *>(item), static_cast<TBox::LayoutData *>(ldata), ctx); break;
        case ElementType::BRACKET:
            layout_bracket(item_cast<const Bracket *>(item), static_cast<Bracket::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::BREATH:
            layout_breath(item_cast<const Breath *>(item), static_cast<Breath::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::CHORD: layout_chord(item_cast<Chord *>(item), ctx); break;
        case ElementType::CHORDLINE:
            layout_chord_line(item_cast<const ChordLine *>(item), static_cast<ChordLine::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::CLEF: layout_clef(item_cast<const Clef *>(item), static_cast<Clef::LayoutData *>(ldata), ctx.conf()); break;
        case ElementType::CAPO: layout_capo(item_cast<const Capo *>(item), static_cast<Capo::LayoutData *>(ldata), ctx); break;
        case ElementType::DEAD_SLAPPED:
            layout_dead_slapped(item_cast<const DeadSlapped *>(item), static_cast<DeadSlapped::LayoutData *>(ldata));
            break;
        case ElementType::DYNAMIC: layout_dynamic(item_cast<Dynamic *>(item), static_cast<Dynamic::LayoutData *>(ldata), ctx.conf()); break;
        case ElementType::EXPRESSION:
            layout_expression(item_cast<const Expression *>(item), static_cast<Expression::LayoutData *>(ldata));
            break;
        case ElementType::FERMATA: layout_fermata(item_cast<const Fermata *>(item), static_cast<Fermata::LayoutData *>(ldata)); break;
        case ElementType::FIGURED_BASS:
            layout_figured_bass(item_cast<const FiguredBass *>(item), static_cast<FiguredBass::LayoutData *>(ldata), ctx);
            break;
        case ElementType::FINGERING:
            layout_fingering(item_cast<const Fingering *>(item), static_cast<Fingering::LayoutData *>(ldata));
            break;
        case ElementType::FRET_DIAGRAM:
            layout_fret_diagram(item_cast<const FretDiagram *>(item), static_cast<FretDiagram::LayoutData *>(ldata), ctx);
            break;
        case ElementType::GLISSANDO: layout_glissando(item_cast<Glissando *>(item), ctx); break;
        case ElementType::GLISSANDO_SEGMENT: layout_glissando_segment(item_cast<GlissandoSegment *>(item), ctx); break;
        case ElementType::GRADUAL_TEMPO_CHANGE: layout_gradual_tempo_change(item_cast<GradualTempoChange *>(item), ctx); break;
        case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
            layout_gradual_tempo_change_segment(item_cast<GradualTempoChangeSegment *>(item), ctx);
            break;
        case ElementType::GUITAR_BEND: layout_guitar_bend(item_cast<GuitarBend *>(item), ctx); break;
        case ElementType::GUITAR_BEND_SEGMENT: layout_guitar_bend_segment(item_cast<GuitarBendSegment *>(item), ctx); break;
        case ElementType::GUITAR_BEND_HOLD_SEGMENT: GuitarBendLayout::layout_hold_line(item_cast<GuitarBendHoldSegment *>(item)); break;
        case ElementType::HAIRPIN: layout_hairpin(item_cast<Hairpin *>(item), ctx); break;
        case ElementType::HAIRPIN_SEGMENT: layout_hairpin_segment(item_cast<HairpinSegment *>(item), ctx); break;
        case ElementType::HAMMER_ON_PULL_OFF: layout_hammer_on_pull_off(item_cast<HammerOnPullOff *>(item), ctx); break;
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT:
            layout_hammer_on_pull_off_segment(item_cast<HammerOnPullOffSegment *>(item), ctx);
            break;
        case ElementType::HAMMER_ON_PULL_OFF_TEXT: layout_hammer_on_pull_off_text(item_cast<HammerOnPullOffText *>(item), ctx); break;
        case ElementType::HARP_DIAGRAM:
            layout_harp_pedal_diagram(item_cast<const HarpPedalDiagram *>(item), static_cast<HarpPedalDiagram::LayoutData *>(ldata));
            break;
        case ElementType::HARMONY: layout_harmony(item_cast<Harmony *>(item), static_cast<Harmony::LayoutData *>(ldata), ctx); break;
        case ElementType::HARMONIC_MARK_SEGMENT: layout_harmonic_mark_segment(item_cast<HarmonicMarkSegment *>(item), ctx); break;
        case ElementType::HOOK: layout_hook(item_cast<const Hook *>(item), static_cast<Hook::LayoutData *>(ldata)); break;
        case ElementType::IMAGE: layout_image(item_cast<const Image *>(item), static_cast<Image::LayoutData *>(ldata)); break;
        case ElementType::INSTRUMENT_CHANGE:
            layout_instrument_change(item_cast<const InstrumentChange *>(item), static_cast<InstrumentChange::LayoutData *>(ldata));
            break;
        case ElementType::INSTRUMENT_NAME:
            layout_instrument_name(item_cast<const InstrumentName *>(item), static_cast<InstrumentName::LayoutData *>(ldata));
            break;
        case ElementType::JUMP: layout_jump(item_cast<const Jump *>(item), static_cast<Jump::LayoutData *>(ldata)); break;
        case ElementType::KEYSIG:
            layout_key_sig(item_cast<const KeySig *>(item), static_cast<KeySig::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::LAISSEZ_VIB: layout_laissez_vib(item_cast<LaissezVib *>(item)); break;
        case ElementType::LAYOUT_BREAK:
            layout_layout_break(item_cast<const LayoutBreak *>(item), static_cast<LayoutBreak::LayoutData *>(ldata));
            break;
        // TODO: case ElementType::STAFF_VISIBILTY_INDICATOR:
        case ElementType::SYSTEM_LOCK_INDICATOR:
            layout_indicator_icon(item_cast<const IndicatorIcon *>(item), static_cast<IndicatorIcon::LayoutData *>(ldata));
            break;
        case ElementType::LET_RING: layout_let_ring(item_cast<LetRing *>(item), ctx); break;
        case ElementType::LET_RING_SEGMENT: layout_let_ring_segment(item_cast<LetRingSegment *>(item), ctx); break;
        case ElementType::LEDGER_LINE: layout_ledger_line(item_cast<LedgerLine *>(item), ctx); break;
        case ElementType::LYRICS: layout_lyrics(item_cast<Lyrics *>(item), ctx); break;
        case ElementType::LYRICSLINE_SEGMENT: layout_lyrics_line_segment(item_cast<LyricsLineSegment *>(item), ctx); break;
        case ElementType::PARTIAL_LYRICSLINE_SEGMENT: layout_lyrics_line_segment(item_cast<LyricsLineSegment *>(item), ctx); break;
        case ElementType::MARKER: layout_marker(item_cast<Marker *>(item), static_cast<Marker::LayoutData *>(ldata), ctx); break;
        case ElementType::MEASURE_NUMBER:
            layout_measure_number(item_cast<MeasureNumber *>(item), static_cast<MeasureNumber::LayoutData *>(ldata), ctx);
            break;
        case ElementType::MEASURE_REPEAT:
            layout_measure_repeat(item_cast<const MeasureRepeat *>(item), static_cast<MeasureRepeat::LayoutData *>(ldata), ctx);
            break;
        case ElementType::MMREST: layout_mm_rest(item_cast<const MMRest *>(item), static_cast<MMRest::LayoutData *>(ldata), ctx); break;
        case ElementType::MMREST_RANGE:
            layout_mm_rest_range(item_cast<MMRestRange *>(item), static_cast<MMRestRange::LayoutData *>(ldata), ctx);
            break;
        case ElementType::NOTE: layout_note(item_cast<const Note *>(item), static_cast<Note::LayoutData *>(ldata)); break;
        case ElementType::NOTEDOT: layout_note_dot(item_cast<const NoteDot *>(item), static_cast<NoteDot::LayoutData *>(ldata)); break;
        case ElementType::NOTEHEAD:
            layout_symbol(item_cast<const NoteHead *>(item), static_cast<NoteHead::LayoutData *>(ldata), ctx);
            break;
        case ElementType::NOTELINE: layout_note_line(item_cast<NoteLine *>(item), ctx); break;
        case ElementType::NOTELINE_SEGMENT: layout_note_line_segment(item_cast<NoteLineSegment *>(item), ctx); break;
        case ElementType::ORNAMENT:
            layout_ornament(item_cast<const Ornament *>(item), static_cast<Ornament::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::OTTAVA: layout_ottava(item_cast<Ottava *>(item), ctx); break;
        case ElementType::OTTAVA_SEGMENT: layout_ottava_segment(item_cast<OttavaSegment *>(item), ctx); break;
        case ElementType::PALM_MUTE: layout_palm_mute(item_cast<PalmMute *>(item), ctx); break;
        case ElementType::PALM_MUTE_SEGMENT: layout_palm_mute_segment(item_cast<PalmMuteSegment *>(item), ctx); break;
        case ElementType::PAGE_LOCK_INDICATOR:
            layout_page_lock_indicator(item_cast<const PageLockIndicator *>(item), static_cast<PageLockIndicator::LayoutData *>(ldata));
            break;
        case ElementType::PARENTHESIS:
            layout_parenthesis(item_cast<Parenthesis *>(item), static_cast<Parenthesis::LayoutData *>(ldata), ctx);
            break;
        case ElementType::PEDAL: layout_pedal(item_cast<Pedal *>(item), ctx); break;
        case ElementType::PEDAL_SEGMENT: layout_pedal_segment(item_cast<PedalSegment *>(item), ctx); break;
        case ElementType::PICK_SCRAPE_SEGMENT: layout_pick_scrape_segment(item_cast<PickScrapeSegment *>(item), ctx); break;
        case ElementType::PLAY_COUNT_TEXT:
            layout_play_count_text(item_cast<PlayCountText *>(item), static_cast<PlayCountText::LayoutData *>(ldata));
            break;
        case ElementType::PLAYTECH_ANNOTATION:
            layout_play_tech_annotation(item_cast<const PlayTechAnnotation *>(item), static_cast<PlayTechAnnotation::LayoutData *>(ldata));
            break;
        case ElementType::RASGUEADO_SEGMENT: layout_rasgueado_segment(item_cast<RasgueadoSegment *>(item), ctx); break;
        case ElementType::REHEARSAL_MARK:
            layout_rehearsal_mark(item_cast<const RehearsalMark *>(item), static_cast<RehearsalMark::LayoutData *>(ldata));
            break;
        case ElementType::REST: layout_rest(item_cast<const Rest *>(item), static_cast<Rest::LayoutData *>(ldata), ctx); break;
        case ElementType::SLUR: layout_slur(item_cast<Slur *>(item), ctx); break;
        case ElementType::SPACER: layout_spacer(item_cast<Spacer *>(item), ctx); break;
        case ElementType::STAFF_STATE:
            layout_staff_state(item_cast<const StaffState *>(item), static_cast<StaffState::LayoutData *>(ldata));
            break;
        case ElementType::STAFF_TEXT:
            layout_staff_text(item_cast<const StaffText *>(item), static_cast<StaffText::LayoutData *>(ldata));
            break;
        case ElementType::STAVE_SHARING_LABEL:
            layout_stave_sharing_label(item_cast<const StaveSharingLabel *>(item), static_cast<StaffText::LayoutData *>(ldata));
            break;
        case ElementType::STAFFTYPE_CHANGE:
            layout_staff_type_change(item_cast<const StaffTypeChange *>(item), static_cast<StaffTypeChange::LayoutData *>(ldata),
                                     ctx.conf());
            break;
        case ElementType::SOUND_FLAG:
            layout_sound_flag(item_cast<const SoundFlag *>(item), static_cast<SoundFlag::LayoutData *>(ldata));
            break;
        case ElementType::STEM: layout_stem(item_cast<const Stem *>(item), static_cast<Stem::LayoutData *>(ldata), ctx.conf()); break;
        case ElementType::STEM_SLASH:
            layout_stem_slash(item_cast<const StemSlash *>(item), static_cast<StemSlash::LayoutData *>(ldata), ctx.conf());
            break;
        case ElementType::STICKING: layout_sticking(item_cast<const Sticking *>(item), static_cast<Sticking::LayoutData *>(ldata)); break;
        case ElementType::STRING_TUNINGS: layout_string_tunings(item_cast<StringTunings *>(item), ctx); break;
        case ElementType::SYMBOL: layout_symbol(item_cast<const Symbol *>(item), static_cast<Symbol::LayoutData *>(ldata), ctx); break;
        case ElementType::FSYMBOL: layout_f_symbol(item_cast<const FSymbol *>(item), static_cast<FSymbol::LayoutData *>(ldata)); break;
        case ElementType::SYSTEM_DIVIDER:
            layout_system_divider(item_cast<const SystemDivider *>(item), static_cast<SystemDivider::LayoutData *>(ldata), ctx);
            break;
        case ElementType::SYSTEM_TEXT:
            layout_system_text(item_cast<const SystemText *>(item), static_cast<SystemText::LayoutData *>(ldata));
            break;
        case ElementType::TAPPING: layout_tapping(to_tapping(item), static_cast<Tapping::LayoutData *>(ldata), ctx); break;
        case ElementType::TAPPING_HALF_SLUR: layout_tapping_half_slur(to_tapping_half_slur(item)); break;
        case ElementType::TEMPO_TEXT:
            layout_tempo_text(item_cast<const TempoText *>(item), static_cast<TempoText::LayoutData *>(ldata));
            break;
        case ElementType::TEXT: layout_text(item_cast<const Text *>(item), static_cast<Text::LayoutData *>(ldata)); break;
        case ElementType::TEXTLINE: layout_text_line(item_cast<TextLine *>(item), ctx); break;
        case ElementType::TEXTLINE_SEGMENT: layout_text_line_segment(item_cast<TextLineSegment *>(item), ctx); break;
        case ElementType::TIE: layout_tie(item_cast<Tie *>(item), ctx); break;
        case ElementType::TIMESIG: layout_time_sig(item_cast<const TimeSig *>(item), static_cast<TimeSig::LayoutData *>(ldata), ctx); break;
        case ElementType::TIME_TICK_ANCHOR: layout_time_tick_anchor(static_cast<TimeTickAnchor *>(item), ctx); break;
        case ElementType::TREMOLO_SINGLECHORD: layout_tremolo_single(item_cast<TremoloSingleChord *>(item), ctx); break;
        case ElementType::TREMOLO_TWOCHORD: layout_tremolo_two(item_cast<TremoloTwoChord *>(item), ctx); break;
        case ElementType::TREMOLOBAR:
            layout_tremolo_bar(item_cast<const TremoloBar *>(item), static_cast<TremoloBar::LayoutData *>(ldata));
            break;
        case ElementType::TRILL: layout_trill(item_cast<Trill *>(item), ctx); break;
        case ElementType::TRILL_SEGMENT: layout_trill_segment(item_cast<TrillSegment *>(item), ctx); break;
        case ElementType::TRIPLET_FEEL:
            layout_triplet_feel(item_cast<const TripletFeel *>(item), static_cast<TripletFeel::LayoutData *>(ldata));
            break;
        case ElementType::TUPLET: layout_tuplet(item_cast<Tuplet *>(item), ctx); break;
        case ElementType::VIBRATO: layout_vibrato(item_cast<Vibrato *>(item), ctx); break;
        case ElementType::VIBRATO_SEGMENT: layout_vibrato_segment(item_cast<VibratoSegment *>(item), ctx); break;
        case ElementType::VOLTA: layout_volta(item_cast<Volta *>(item), ctx); break;
        case ElementType::VOLTA_SEGMENT: layout_volta_segment(item_cast<VoltaSegment *>(item), ctx); break;
        case ElementType::WHAMMY_BAR: layout_line(item_cast<WhammyBar *>(item), ctx); break;
        case ElementType::WHAMMY_BAR_SEGMENT: layout_whammy_bar_segment(item_cast<WhammyBarSegment *>(item), ctx); break;
        default: LOGE() << "not found in layout types item: " << item->type_name(); DO_ASSERT(false);
    }
}

void TLayout::layout_accidental(const Accidental *item, Accidental::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    if (ldata->is_valid()) {
        return;
    }

    ldata->syms.clear();

    // TODO: remove Accidental in layout
    // don't show accidentals for tab or slash notation
    if (item->on_tab_staff() || (item->note() && item->note()->fixed())) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);
    ldata->set_pos(PointF());

    if (item->accidental_type() == AccidentalType::NONE) {
        return;
    }

    ldata->column = 0;

    auto accidental_single_sym = [](const Accidental *item) -> SymId {
        // if the accidental is standard (doubleflat, flat, natural, sharp or double sharp)
        // and it has either no bracket or parentheses, then we have glyphs straight from smufl.

        if (item->bracket() == AccidentalBracket::PARENTHESIS && !item->parent_note_has_parentheses()) {
            switch (item->accidental_type()) {
                case AccidentalType::FLAT: return SymId::ACCIDENTAL_FLAT_PARENS;
                case AccidentalType::FLAT2: return SymId::ACCIDENTAL_DOUBLE_FLAT_PARENS;
                case AccidentalType::NATURAL: return SymId::ACCIDENTAL_NATURAL_PARENS;
                case AccidentalType::SHARP: return SymId::ACCIDENTAL_SHARP_PARENS;
                case AccidentalType::SHARP2: return SymId::ACCIDENTAL_DOUBLE_SHARP_PARENS;
                default: break;
            }
        }
        return SymId::NO_SYM;
    };

    auto accidental_bracket_syms = [](AccidentalBracket type) -> std::pair<SymId, SymId> {
        switch (type) {
            case AccidentalBracket::PARENTHESIS: return {SymId::ACCIDENTAL_PARENS_LEFT, SymId::ACCIDENTAL_PARENS_RIGHT};
            case AccidentalBracket::BRACKET: return {SymId::ACCIDENTAL_BRACKET_LEFT, SymId::ACCIDENTAL_BRACKET_RIGHT};
            case AccidentalBracket::BRACE:
                return {SymId::ACCIDENTAL_COMBINING_OPEN_CURLY_BRACE, SymId::ACCIDENTAL_COMBINING_CLOSE_CURLY_BRACE};
            case AccidentalBracket::NONE: return {SymId::NO_SYM, SymId::NO_SYM};
        }
        return {SymId::NO_SYM, SymId::NO_SYM};
    };

    Shape shape;

    // Single?
    SymId single_sym = accidental_single_sym(item);
    if (single_sym != SymId::NO_SYM && conf.engraving_font()->is_valid(single_sym)) {
        Accidental::LayoutData::Sym s(single_sym, 0.0, 0.0);
        ldata->syms.push_back(s);

        shape.add(item->sym_shape_with_cutouts(single_sym));
    }
    // Multi
    else {
        double margin = conf.style_absolute(Sid::BRACKETED_ACCIDENTAL_PADDING);
        double x = 0.0;

        std::pair<SymId, SymId> bracket_syms;
        bool is_need_bracket = item->bracket() != AccidentalBracket::NONE && !item->parent_note_has_parentheses();
        if (is_need_bracket) {
            bracket_syms = accidental_bracket_syms(item->bracket());
        }

        // Left
        if (bracket_syms.first != SymId::NO_SYM) {
            Accidental::LayoutData::Sym ls(bracket_syms.first, 0.0,
                                           item->bracket() == AccidentalBracket::BRACE ? item->spatium() * 0.4 : 0.0);
            ldata->syms.push_back(ls);
            shape.add(item->sym_bbox(bracket_syms.first), item);

            x += item->sym_advance(bracket_syms.first) + margin;
        }

        // Main
        SymId main_sym = item->sym_id();
        Accidental::LayoutData::Sym ms(main_sym, x, 0.0);
        ldata->syms.push_back(ms);
        shape.add(item->sym_shape_with_cutouts(main_sym).translated(PointF(x, 0.0)));

        // Right
        if (bracket_syms.second != SymId::NO_SYM) {
            x += item->sym_advance(main_sym) + margin;

            Accidental::LayoutData::Sym rs(bracket_syms.second, x,
                                           item->bracket() == AccidentalBracket::BRACE ? item->spatium() * 0.4 : 0.0);
            ldata->syms.push_back(rs);
            shape.add(item->sym_bbox(bracket_syms.second).translated(x, 0.0), item);
        }
    }

    ldata->set_shape(shape);
}

void TLayout::layout_ambitus(const Ambitus *item, Ambitus::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    if (ldata->is_valid()) {
        return;
    }

    const double spatium = item->spatium();
    const double head_wdt = item->head_width();

    Accidental::LayoutData *top_acc_data = item->top_accidental()->mutldata();
    Accidental::LayoutData *bottom_acc_data = item->bottom_accidental()->mutldata();

    ldata->set_pos(PointF());

    //
    // NOTEHEADS Y POS
    //
    {
        const Fraction tick = item->segment()->tick();
        const Staff *stf = ctx.dom().staff(item->staff_idx());
        const ClefType clf = stf->clef(tick);
        const Key key = stf->key(tick);
        const double line_dist = stf->line_distance(tick) * spatium;

        // top notehead
        if (!pitch_is_valid(item->top_pitch()) || !tpc_is_valid(item->top_tpc())) {
            ldata->top_pos.set_y(0.0); // if uninitialized, set to top staff line
        } else {
            int top_line = Ambitus::staff_line(item->top_tpc(), item->top_pitch(), clf);
            ldata->top_pos.set_y(top_line * line_dist * 0.5);
            // compute accidental
            AccidentalType accid_type = Ambitus::accidental_type(item->top_tpc(), key);
            item->top_accidental()->set_accidental_type(accid_type);
            TLayout::layout_accidental(item->top_accidental(), top_acc_data, ctx.conf());
            top_acc_data->set_pos_y(ldata->top_pos.y());
        }

        // bottom notehead
        if (!pitch_is_valid(item->bottom_pitch()) || !tpc_is_valid(item->bottom_tpc())) {
            const int num_of_lines = stf->lines(tick);
            ldata->bottom_pos.set_y((num_of_lines - 1) * line_dist); // if uninitialized, set to last staff line
        } else {
            int bottom_line = Ambitus::staff_line(item->bottom_tpc(), item->bottom_pitch(), clf);
            ldata->bottom_pos.set_y(bottom_line * line_dist * 0.5);
            // compute accidental
            AccidentalType accid_type = Ambitus::accidental_type(item->bottom_tpc(), key);
            item->bottom_accidental()->set_accidental_type(accid_type);
            TLayout::layout_accidental(item->bottom_accidental(), bottom_acc_data, ctx.conf());
            bottom_acc_data->set_pos_y(ldata->bottom_pos.y());
        }
    }

    //
    // NOTEHEAD X POS
    //
    // Note: manages colliding accidentals
    //
    {
        double acc_note_dist = item->absolute_from_spatium(ctx.conf().style_s(Sid::ACCIDENTAL_NOTE_DISTANCE));
        double x_accid_off_top = top_acc_data->bbox().width() + acc_note_dist;
        double x_accid_off_bottom = bottom_acc_data->bbox().width() + acc_note_dist;

        // if top accidental extends down more than bottom accidental extends up,
        // AND ambitus is not leaning right, bottom accidental needs to be displaced
        bool collision = (item->direction() != DirectionH::RIGHT) &&
                         (top_acc_data->pos().y() + top_acc_data->bbox().y() + top_acc_data->bbox().height() >
                          bottom_acc_data->pos().y() + bottom_acc_data->bbox().y());
        if (collision) {
            // displace bottom accidental (also attempting to 'undercut' flats)
            AccidentalType bottom_acc_type = item->bottom_accidental()->accidental_type();
            x_accid_off_bottom = x_accid_off_top + ((bottom_acc_type == AccidentalType::FLAT || bottom_acc_type == AccidentalType::FLAT2 ||
                                                     bottom_acc_type == AccidentalType::NATURAL) ?
                                                        bottom_acc_data->bbox().width() * 0.5 :
                                                        bottom_acc_data->bbox().width());
        }

        switch (item->direction()) {
            case DirectionH::AUTO: // noteheads one above the other
                // left align noteheads and right align accidentals 'hanging' on the left
                ldata->top_pos.set_x(0.0);
                ldata->bottom_pos.set_x(0.0);
                top_acc_data->set_pos_x(-x_accid_off_top);
                bottom_acc_data->set_pos_x(-x_accid_off_bottom);
                break;
            case DirectionH::LEFT: // top notehead at the left of bottom notehead
                // place top notehead at left margin; bottom notehead at right of top head;
                // top accid. 'hanging' on left of top head and bottom accid. 'hanging' at left of bottom head
                ldata->top_pos.set_x(0.0);
                ldata->bottom_pos.set_x(head_wdt);
                top_acc_data->set_pos_x(-x_accid_off_top);
                bottom_acc_data->set_pos_x(collision ? -x_accid_off_bottom : head_wdt - x_accid_off_bottom);
                break;
            case DirectionH::RIGHT: // top notehead at the right of bottom notehead
                // bottom notehead at left margin; top notehead at right of bottomnotehead
                // top accid. 'hanging' on left of top head and bottom accid. 'hanging' at left of bottom head
                ldata->top_pos.set_x(head_wdt);
                ldata->bottom_pos.set_x(0.0);
                top_acc_data->set_pos_x(head_wdt - x_accid_off_top);
                bottom_acc_data->set_pos_x(-x_accid_off_bottom);
                break;
        }
    }

    // LINE
    // compute line from top note centre to bottom note centre
    {
        LineF full_line(ldata->top_pos.x() + head_wdt * 0.5, ldata->top_pos.y(), ldata->bottom_pos.x() + head_wdt * 0.5,
                        ldata->bottom_pos.y());
        // shorten line on each side by offsets
        double y_delta = ldata->bottom_pos.y() - ldata->top_pos.y();
        if (!muse::real_is_null(y_delta)) {
            double off = Ambitus::lineoffset_default.to_absolute(spatium);
            PointF p1 = full_line.point_at(off / y_delta);
            PointF p2 = full_line.point_at(1 - (off / y_delta));
            ldata->line = LineF(p1, p2);
        } else {
            ldata->line = full_line;
        }
    }

    // BBOX
    {
        RectF head_rect(0, -0.5 * spatium, head_wdt, 1 * spatium);
        ldata->set_bbox(head_rect.translated(ldata->top_pos)
                            .united(head_rect.translated(ldata->bottom_pos))
                            .united(top_acc_data->bbox().translated(top_acc_data->pos()))
                            .united(bottom_acc_data->bbox().translated(bottom_acc_data->pos())));
    }
}

void TLayout::layout_arpeggio(const Arpeggio *item, Arpeggio::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);

    //! NOTE Can be edited and relayout,
    //! in this case the reset layout data has not yet been done
    //    if (ldata->isValid()) {
    //        return;
    //    }

    if (conf.style_b(Sid::ARPEGGIO_HIDDEN_IN_STD_IF_TAB)) {
        if (item->staff() && item->staff()->is_pitched_staff(item->tick())) {
            for (Staff *s : item->staff()->staff_list()) {
                if (s->on_same_score(item) && s->is_tab_staff(item->tick()) && s->visible()) {
                    ldata->set_is_skip_draw(true);
                }
            }
        }
    }
    ldata->set_is_skip_draw(false);
    if (!ldata->is_set_pos()) {
        ldata->set_pos(PointF());
    }

    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    //! NOTE Must already be set previously
    Chord *parent_chord = item->chord();
    LD_CONDITION(parent_chord->up_note()->ldata()->is_set_pos());
    LD_CONDITION(parent_chord->down_note()->ldata()->is_set_pos());

    auto symbol_line = [](const std::shared_ptr<const IEngravingFont> &f, Arpeggio::LayoutData *data, SymId end, SymId fill) {
        data->symbols.clear();

        double w = data->bottom - data->top;
        double w1 = f->advance(end, data->mag_s);
        double w2 = f->advance(fill, data->mag_s);
        int n = lrint((w - w1) / w2);
        for (int i = 0; i < n; ++i) {
            data->symbols.push_back(fill);
        }
        data->symbols.push_back(end);
    };

    ldata->arpeggio_height = ArpeggioLayout::compute_height(item);
    ldata->top = ArpeggioLayout::calc_top(item, conf);
    ldata->bottom = ArpeggioLayout::calc_bottom(item, ldata->arpeggio_height, conf);

    ldata->set_mag(item->staff() ? item->staff()->staff_mag(item->tick()) : item->mag());
    ldata->mag_s = conf.mag_s(ldata->mag());

    std::shared_ptr<const IEngravingFont> font = conf.engraving_font();
    switch (item->arpeggio_type()) {
        case ArpeggioType::NORMAL: {
            symbol_line(font, ldata, SymId::WIGGLE_ARPEGGIATO_UP, SymId::WIGGLE_ARPEGGIATO_UP);
            // string is rotated -90 degrees
            ldata->syms_b_box = font->bbox(ldata->symbols, ldata->mag_s);
            ldata->set_bbox(RectF(0.0, -ldata->syms_b_box.x() + ldata->top, ldata->syms_b_box.height(), ldata->syms_b_box.width()));
        } break;

        case ArpeggioType::UP: {
            symbol_line(font, ldata, SymId::WIGGLE_ARPEGGIATO_UP_ARROW, SymId::WIGGLE_ARPEGGIATO_UP);
            // string is rotated -90 degrees
            ldata->syms_b_box = font->bbox(ldata->symbols, ldata->mag_s);
            ldata->set_bbox(RectF(0.0, -ldata->syms_b_box.x() + ldata->top, ldata->syms_b_box.height(), ldata->syms_b_box.width()));
        } break;

        case ArpeggioType::DOWN: {
            symbol_line(font, ldata, SymId::WIGGLE_ARPEGGIATO_UP_ARROW, SymId::WIGGLE_ARPEGGIATO_UP);
            // string is rotated +90 degrees (so that UpArrow turns into a DownArrow)
            ldata->syms_b_box = font->bbox(ldata->symbols, ldata->mag_s);
            ldata->set_bbox(RectF(0.0, ldata->syms_b_box.x() + ldata->top, ldata->syms_b_box.height(), ldata->syms_b_box.width()));
        } break;

        case ArpeggioType::UP_STRAIGHT: {
            double x1 = item->spatium() * 0.5;
            ldata->syms_b_box = font->bbox(SymId::ARROWHEAD_BLACK_UP, ldata->mag_s);
            double w = ldata->syms_b_box.width();
            ldata->set_bbox(RectF(x1 - w * 0.5, ldata->top, w, ldata->bottom));
        } break;

        case ArpeggioType::DOWN_STRAIGHT: {
            double x1 = item->spatium() * 0.5;
            ldata->syms_b_box = font->bbox(SymId::ARROWHEAD_BLACK_DOWN, ldata->mag_s);
            double w = ldata->syms_b_box.width();
            ldata->set_bbox(RectF(x1 - w * 0.5, ldata->top, w, ldata->bottom));
        } break;

        case ArpeggioType::BRACKET: {
            double w = conf.style_s(Sid::ARPEGGIO_HOOK_LEN).val() * item->spatium();
            ldata->set_bbox(RectF(0.0, ldata->top, w, ldata->bottom));
        } break;
    }

    // Loop through staves spanned & regenerate chord shape
    // This makes sure the arpeggio's shape is added to the shape of each chord it spans
    Chord *chord = item->chord();
    Segment *seg = chord->segment();
    staff_idx_t stave_span = (item->track() + item->span() - 1) / ::mu::engraving::voices;
    for (staff_idx_t staff_idx = item->staff_idx(); staff_idx <= stave_span; staff_idx++) {
        seg->create_shape(staff_idx);
    }
}

static Shape get_chords_shape(const ChordBracket *item) {
    Chord *owner = item->chord();
    Shape result = owner->shape();
    result.remove_types({ElementType::CHORD_BRACKET, ElementType::ARPEGGIO});
    if (!owner->segment() || !owner->staff()) {
        return result;
    }

    const track_idx_t start_track = staff2track(owner->staff_idx());
    const track_idx_t end_track = start_track + ::mu::engraving::voices;

    for (track_idx_t track = start_track; track < end_track; ++track) {
        EngravingItem *e = owner->segment()->element(track);
        if (track == owner->track()) {
            continue;
        }

        if (!e || !e->is_chord()) {
            continue;
        }

        Chord *chord = to_chord(e);
        if (chord->v_staff_idx() != owner->v_staff_idx()) {
            continue;
        }

        Shape chord_shape = chord->shape();
        chord_shape.remove_types({ElementType::CHORD_BRACKET, ElementType::ARPEGGIO});
        chord_shape.translate(chord->pos() - owner->pos());
        result.add(chord_shape);
    }

    return result;
}

void TLayout::layout_chord_bracket(const ChordBracket *item, Arpeggio::LayoutData *ldata, const LayoutConfiguration &conf) {
    double spatium = item->spatium();
    ldata->arpeggio_height = ArpeggioLayout::compute_height(item);
    ldata->top = ArpeggioLayout::calc_top(item, conf) - 0.25 * spatium;
    ldata->bottom = ArpeggioLayout::calc_bottom(item, ldata->arpeggio_height, conf) + 0.5 * spatium;

    const double min_len = 2 * item->spatium();
    double diff = min_len - std::abs(ldata->bottom);
    if (diff > 0) {
        ldata->bottom += diff;
        if (item->hook_pos() == DirectionV::DOWN) {
            ldata->top -= diff;
        } else if (item->hook_pos() == DirectionV::AUTO) {
            ldata->top -= 0.5 * diff;
        }
    }

    ldata->set_mag(item->staff() ? item->staff()->staff_mag(item->tick()) : item->mag());
    ldata->mag_s = conf.mag_s(ldata->mag());

    ldata->set_shape(Shape(RectF(0.0, ldata->top, item->absolute_from_spatium(item->hook_length()), ldata->bottom).normalized(), item));

    const Note *upnote = item->chord()->up_note();
    ldata->set_pos_y(upnote->y() + upnote->ldata()->bbox().top());

    Shape chord_shape = get_chords_shape(item);
    Shape item_shape = item->shape().translated(PointF(0.0, item->ldata()->pos().y()));
    if (item->right_side()) {
        double x = HorizontalSpacing::min_horizontal_distance(chord_shape, item_shape, spatium);
        x = std::max(x, 0.0);
        ldata->set_pos_x(x);
    } else {
        double x = HorizontalSpacing::min_horizontal_distance(item_shape, chord_shape, spatium);
        x = std::max(x, 0.0);
        ldata->set_pos_x(-x);
    }

    // Loop through staves spanned & regenerate chord shape
    // This makes sure the arpeggio's shape is added to the shape of each chord it spans
    Chord *chord = item->chord();
    Segment *seg = chord->segment();
    staff_idx_t stave_span = (item->track() + item->span() - 1) / ::mu::engraving::voices;
    for (staff_idx_t staff_idx = item->staff_idx(); staff_idx <= stave_span; staff_idx++) {
        seg->create_shape(staff_idx);
    }
}

void TLayout::layout_articulation(Articulation *item, Articulation::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    if (item->is_hidden_on_tab_staff()) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);
    ldata->set_pos(PointF());

    //! NOTE Must already be set previously
    LD_CONDITION(ldata->sym_id.has_value());

    if (item->text_type() != ArticulationTextType::NO_TEXT) {
        if (!item->text()) {
            Text *text = new Text(item, TextStyleType::ARTICULATION);
            static const ElementStyle element_style;
            text->init_element_style(&element_style);
            item->set_text(text);
        }

        Text *text = item->text();
        text->set_xml_text(TConv::text(item->text_type()));
        text->set_track(item->track());
        text->set_ownership_parent(item);
        text->set_selected(item->selected());

        TextLayout::layout_base_text_base(item->text(), item->text()->mutldata());
    }

    fill_articulation_shape(item, ldata);
}

void TLayout::fill_articulation_shape(const Articulation *item, Articulation::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);

    SymId sym = item->sym_id();
    if (sym == SymId::ARTIC_ACCENT_ABOVE || sym == SymId::ARTIC_ACCENT_BELOW) {
        RectF sym_bbox = item->sym_bbox(sym);
        double width = sym_bbox.width();
        double height = sym_bbox.height();
        double third_width = width / 3;
        double third_height = height / 3;
        RectF base = RectF(0.0, 0.0, third_width, height);
        RectF center = RectF(third_width, third_height, third_width, third_height);
        RectF tip = RectF(2 * third_width, 1.25 * third_height, third_width, 0.5 * third_height);
        Shape shape;
        shape.add(base, item);
        shape.add(center, item);
        shape.add(tip, item);
        PointF translate(0.0, sym == SymId::ARTIC_ACCENT_ABOVE ? -height : 0.0);
        ldata->set_shape(shape.translate(translate));
    } else if (item->text_type() == ArticulationTextType::NO_TEXT) {
        ldata->set_shape(Shape(item->sym_bbox(sym), item));
    } else {
        ldata->set_shape(Shape(item->text()->ldata()->bbox(), item));
    }
}

static double bar_line_width(const BarLine *item, const MStyle &style, double dot_width) {
    double w = 0.0;
    switch (item->bar_line_type()) {
        case BarLineType::DOUBLE:
            w = style.style_absolute(Sid::DOUBLE_BAR_WIDTH) * 2.0 + style.style_absolute(Sid::DOUBLE_BAR_DISTANCE);
            break;
        case BarLineType::DOUBLE_HEAVY:
            w = style.style_absolute(Sid::END_BAR_WIDTH) * 2.0 + style.style_absolute(Sid::END_BAR_DISTANCE);
            break;
        case BarLineType::END_START_REPEAT:
            w = style.style_absolute(Sid::END_BAR_WIDTH) + style.style_absolute(Sid::BAR_WIDTH) * 2.0 +
                style.style_absolute(Sid::END_BAR_DISTANCE) * 2.0 + style.style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION) * 2.0 +
                dot_width * 2;
            break;
        case BarLineType::START_REPEAT:
        case BarLineType::END_REPEAT:
            w = style.style_absolute(Sid::END_BAR_WIDTH) + style.style_absolute(Sid::BAR_WIDTH) +
                style.style_absolute(Sid::END_BAR_DISTANCE) + style.style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION) + dot_width;
            break;
        case BarLineType::END:
        case BarLineType::REVERSE_END:
            w = style.style_absolute(Sid::END_BAR_WIDTH) + style.style_absolute(Sid::BAR_WIDTH) +
                style.style_absolute(Sid::END_BAR_DISTANCE);
            break;
        case BarLineType::BROKEN: w = style.style_absolute(Sid::DASH_BAR_WIDTH); break;
        case BarLineType::NORMAL:
        case BarLineType::DOTTED: w = style.style_absolute(Sid::BAR_WIDTH); break;
        case BarLineType::HEAVY: w = style.style_absolute(Sid::END_BAR_WIDTH); break;
    }
    return w;
}

void TLayout::layout_bar_line(const BarLine *item, BarLine::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    // barlines hidden on this staff
    if (item->staff() && item->segment()) {
        if ((!item->staff()->staff_type_for_element(item)->show_barlines() &&
             item->segment()->segment_type() == SegmentType::END_BAR_LINE) ||
            (item->staff()->hide_system_bar_line() && item->segment()->segment_type() == SegmentType::BEGIN_BAR_LINE)) {
            ldata->set_pos(PointF());
            ldata->set_bbox(RectF());
            ldata->set_is_skip_draw(true);
            return;
        }
    }

    ldata->set_is_skip_draw(false);
    ldata->set_pos(PointF());

    // Check conditions

    //! NOTE Here the goal is not just to check whether the conditions are met,
    //! but the goal is to clearly show these conditions to the developer.
    for (const EngravingItem *e : *item->el()) {
        switch (e->type()) {
            case ElementType::ARTICULATION:
                // form Articulation layout
                LD_CONDITION(item_cast<const Articulation *>(e)->ldata()->sym_id.has_value());
                break;
            case ElementType::SYMBOL: LD_INDEPENDENT; break;
            case ElementType::IMAGE:
                // will be done
                // LD_CONDITION(parentLD->isSetBbox());
                LD_INDEPENDENT;
                break;
            default: UNREACHABLE;
        }
    }

    ldata->set_mag(ctx.conf().style_b(Sid::SCALE_BARLINES) && item->staff() ? item->staff()->staff_mag(item->tick()) : 1.0);
    // Note: the true values of y1 and y2 are computed in layout2() (can be done only
    // after staff distances are known). This is a temporary layout.
    const double spatium = item->spatium();

    ldata->y1 = spatium * .5 * item->span_from();
    ldata->y2 = spatium * .5 * (8.0 + item->span_to());

    const IEngravingFontPtr font = ctx.engraving_font();
    const double mag_s = ctx.conf().mag_s(ldata->mag());

    double w = bar_line_width(item, ctx.conf().style(), font->width(SymId::REPEAT_DOT, mag_s)) * ldata->mag();
    RectF r(0.0, ldata->y1, w, ldata->y2 - ldata->y1);

    if (ctx.conf().style_b(Sid::REPEAT_BAR_TIPS)) {
        switch (item->bar_line_type()) {
            case BarLineType::START_REPEAT: {
                r.unite(font->bbox(SymId::BRACKET_TOP, mag_s).translated(0, ldata->y1));
            } break;
            case BarLineType::END_REPEAT: {
                double w1 = font->width(SymId::REVERSED_BRACKET_TOP, mag_s) - w;
                r.unite(font->bbox(SymId::REVERSED_BRACKET_TOP, mag_s).translated(-w1, ldata->y1));
            } break;
            case BarLineType::END_START_REPEAT: {
                double w1 = font->width(SymId::REVERSED_BRACKET_TOP, mag_s) - w;
                r.unite(font->bbox(SymId::REVERSED_BRACKET_TOP, mag_s).translated(-w1, ldata->y1));
                r.unite(font->bbox(SymId::BRACKET_TOP, mag_s).translated(0, ldata->y1));
            } break;
            default: break;
        }
    }

    ldata->set_bbox(r);

    update_barline_shape(item, ldata, ctx);

    //! NOTE The types are listed here explicitly to show what types there are (see add method)
    //! and accordingly show what the barline layout depends on.
    for (EngravingItem *e : *item->el()) {
        switch (e->type()) {
            case ElementType::ARTICULATION: {
                Articulation *a = item_cast<Articulation *>(e);
                Articulation::LayoutData *aldata = a->mutldata();
                TLayout::layout_articulation(a, aldata);
                DirectionV dir = a->direction();
                double distance = 0.5 * spatium;
                double x = /*barline*/ ldata->bbox().width() * 0.5;
                if (dir == DirectionV::DOWN) {
                    double bot_y = ldata->y2 + distance;
                    aldata->set_pos(PointF(x, bot_y));
                } else {
                    double top_y = ldata->y1 - distance;
                    aldata->set_pos(PointF(x, top_y));
                }
            } break;
            case ElementType::SYMBOL: {
                Symbol *sb = item_cast<Symbol *>(e);
                TLayout::layout_symbol(sb, sb->mutldata(), ctx);
            } break;
            case ElementType::IMAGE: {
                Image *im = item_cast<Image *>(e);
                TLayout::layout_image(im, im->mutldata());
            } break;
            default: UNREACHABLE;
        }
    }

    if (!item->segment()) {
        return;
    }

    if (Fermata *fermata =
            to_fermata(item->segment()->find_annotation(ElementType::FERMATA, item->track(), item->track() + ::mu::engraving::voices))) {
        layout_fermata(fermata, fermata->mutldata());
    }
}

void TLayout::update_barline_shape(const BarLine *item, BarLine::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    RectF bb = ldata->bbox();
    if (item->staff()) {
        // actual height may include span to next staff
        // but this should not be included in shapes or skylines
        double sp = item->spatium();
        int span = item->staff()->lines(item->tick()) - 1;
        int s_from;
        int s_to;
        if (span == 0 && item->span_to() == 0) {
            s_from = ::mu::engraving::barline_span_1_linestaff_from;
            s_to = item->span_staff() ? 0 : ::mu::engraving::barline_span_1_linestaff_to;
        } else {
            s_from = item->span_from();
            s_to = item->span_staff() ? 0 : item->span_to();
        }
        double y = sp * s_from * 0.5;
        double h = sp * (span + (s_to - s_from) * 0.5);
        if (ctx.conf().style_b(Sid::REPEAT_BAR_TIPS)) {
            switch (item->bar_line_type()) {
                case BarLineType::START_REPEAT:
                case BarLineType::END_REPEAT:
                case BarLineType::END_START_REPEAT: {
                    if (item->is_top()) {
                        double top = item->sym_bbox(SymId::BRACKET_TOP).height();
                        y -= top;
                        h += top;
                    }
                    if (item->is_bottom()) {
                        double bottom = item->sym_bbox(SymId::BRACKET_BOTTOM).height();
                        h += bottom;
                    }
                }
                default: break;
            }
        }
        bb.set_top(y);
        bb.set_height(h);
    }

    ldata->set_shape(Shape(bb, item));
}

//---------------------------------------------------------
//    called after system layout; set vertical dimensions
//---------------------------------------------------------
void TLayout::layout_bar_line2(BarLine *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    BarLine::LayoutData *ldata = item->mutldata();

    if (ldata->is_skip_draw()) {
        return;
    }

    item->calc_y();
    RectF bbox = ldata->bbox();
    bbox.set_top(ldata->y1);
    bbox.set_bottom(ldata->y2);

    if (ctx.conf().style_b(Sid::REPEAT_BAR_TIPS)) {
        switch (item->bar_line_type()) {
            case BarLineType::START_REPEAT:
                bbox.unite(item->sym_bbox(SymId::BRACKET_TOP).translated(0, ldata->y1));
                bbox.unite(item->sym_bbox(SymId::BRACKET_BOTTOM).translated(0, ldata->y2));
                break;
            case BarLineType::END_REPEAT: {
                double w1 = item->sym_bbox(SymId::REVERSED_BRACKET_TOP).x() - bbox.x();
                bbox.unite(item->sym_bbox(SymId::REVERSED_BRACKET_TOP).translated(-w1, ldata->y1));
                bbox.unite(item->sym_bbox(SymId::REVERSED_BRACKET_BOTTOM).translated(-w1, ldata->y2));
                break;
            }
            case BarLineType::END_START_REPEAT: {
                double w1 = item->sym_bbox(SymId::REVERSED_BRACKET_TOP).x() - bbox.x();
                bbox.unite(item->sym_bbox(SymId::REVERSED_BRACKET_TOP).translated(-w1, ldata->y1));
                bbox.unite(item->sym_bbox(SymId::REVERSED_BRACKET_BOTTOM).translated(-w1, ldata->y2));
                bbox.unite(item->sym_bbox(SymId::BRACKET_TOP).translated(0, ldata->y1));
                bbox.unite(item->sym_bbox(SymId::BRACKET_BOTTOM).translated(0, ldata->y2));
                break;
            }
            default: break;
        }
    }

    ldata->set_bbox(bbox);
}

void TLayout::layout_beam(Beam *item, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    BeamLayout::layout(item, ctx);
}

void TLayout::layout_beam1(Beam *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    BeamLayout::layout1(item, ctx);
}

void TLayout::layout_bend(const Bend *item, Bend::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    LD_CONDITION(item->note()->ldata()->is_set_pos());
    LD_CONDITION(item->note()->ldata()->is_set_bbox());

    double spatium = item->spatium();
    double lw = item->absolute_from_spatium(item->line_width());

    const Note::LayoutData *note_ld = item->note()->ldata();
    PointF note_pos = note_ld->pos();
    note_pos.ry() = std::max(note_pos.y(), 0.0);

    ldata->note_width = note_ld->bbox().width();
    ldata->note_pos = note_pos;

    RectF bb;

    FontMetrics fm(item->font(spatium));

    size_t n = item->points().size();
    double x = ldata->note_width;
    double y = -spatium * .8;
    double x2 = 0.0, y2 = 0.0;

    double aw = spatium * .5;
    PolygonF arrow_up;
    arrow_up << PointF(0, 0) << PointF(aw * .5, aw) << PointF(-aw * .5, aw);
    PolygonF arrow_down;
    arrow_down << PointF(0, 0) << PointF(aw * .5, -aw) << PointF(-aw * .5, -aw);

    for (size_t pt = 0; pt < n; ++pt) {
        if (pt == (n - 1)) {
            break;
        }
        int pitch = item->points().at(pt).pitch;
        if (pt == 0 && pitch) {
            y2 = -ldata->note_pos.y() - spatium * 2;
            x2 = x;
            bb.unite(RectF(x, y, x2 - x, y2 - y));

            bb.unite(arrow_up.translated(x2, y2 + spatium * .2).bounding_rect());

            int idx = (pitch + 12) / 25;
            const char *l = Bend::label[idx];
            bb.unite(fm.bounding_rect(String::from_ascii(l)));
            y = y2;
        }
        if (pitch == item->points().at(pt + 1).pitch) {
            if (pt == (n - 2)) {
                break;
            }
            x2 = x + spatium;
            y2 = y;
            bb.unite(RectF(x, y, x2 - x, y2 - y));
        } else if (pitch < item->points().at(pt + 1).pitch) {
            // up
            x2 = x + spatium * .5;
            y2 = -ldata->note_pos.y() - spatium * 2;
            double dx = x2 - x;
            double dy = y2 - y;

            PainterPath path;
            path.move_to(x, y);
            path.cubic_to(x + dx / 2, y, x2, y + dy / 4, x2, y2);
            bb.unite(path.bounding_rect());
            bb.unite(arrow_up.translated(x2, y2 + spatium * .2).bounding_rect());

            int idx = (item->points().at(pt + 1).pitch + 12) / 25;
            const char *l = Bend::label[idx];
            bb.unite(fm.bounding_rect(String::from_ascii(l)));
        } else {
            // down
            x2 = x + spatium * .5;
            y2 = y + spatium * 3;
            double dx = x2 - x;
            double dy = y2 - y;

            PainterPath path;
            path.move_to(x, y);
            path.cubic_to(x + dx / 2, y, x2, y + dy / 4, x2, y2);
            bb.unite(path.bounding_rect());

            bb.unite(arrow_down.translated(x2, y2 - spatium * .2).bounding_rect());
        }
        x = x2;
        y = y2;
    }
    bb.adjust(-lw, -lw, lw, lw);

    ldata->set_bbox(bb);
    ldata->set_pos(PointF());
}

void TLayout::layout_h_box(const HBox *item, HBox::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    BoxLayout::layout_h_box(item, ldata, ctx);
}

void TLayout::layout_v_box(const VBox *item, VBox::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    BoxLayout::layout_v_box(item, ldata, ctx);
}

void TLayout::layout_f_box(const FBox *item, FBox::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    BoxLayout::layout_f_box(item, ldata, ctx);
}

void TLayout::layout_t_box(const TBox *item, TBox::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    BoxLayout::layout_t_box(item, ldata, ctx);
}

void TLayout::layout_bracket(const Bracket *item, Bracket::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    LD_CONDITION(ldata->bracket_height.has_value());
    if (!ldata->bracket_height.has_value()) {
        return;
    }

    const_cast<Bracket *>(item)->set_visible(item->bi()->visible());

    ldata->brace_symbol = item->brace_symbol();

    switch (item->bracket_type()) {
        case BracketType::BRACE: {
            String musical_symbol_font = conf.style_st(Sid::MUSICAL_SYMBOL_FONT);
            if (musical_symbol_font == "Emmentaler" || musical_symbol_font == "Gonville") {
                ldata->brace_symbol = SymId::NO_SYM;
                double w = conf.style_absolute(Sid::AKKOLADE_WIDTH);
                double h2 = ldata->bracket_height * 0.5;

#define XM(a) (a + 700) * w / 700
#define YM(a) (a + 7100) * h2 / 7100

                PainterPath path;
                path.move_to(XM(-8), YM(-2048));
                path.cubic_to(XM(-8), YM(-3192), XM(-360), YM(-4304), XM(-360), YM(-5400));   // c 0
                path.cubic_to(XM(-360), YM(-5952), XM(-264), YM(-6488), XM(32), YM(-6968));   // c 1
                path.cubic_to(XM(36), YM(-6974), XM(38), YM(-6984), XM(38), YM(-6990));       // c 0
                path.cubic_to(XM(38), YM(-7008), XM(16), YM(-7024), XM(0), YM(-7024));        // c 0
                path.cubic_to(XM(-8), YM(-7024), XM(-22), YM(-7022), XM(-32), YM(-7008));     // c 1
                path.cubic_to(XM(-416), YM(-6392), XM(-544), YM(-5680), XM(-544), YM(-4960)); // c 0
                path.cubic_to(XM(-544), YM(-3800), XM(-168), YM(-2680), XM(-168), YM(-1568)); // c 0
                path.cubic_to(XM(-168), YM(-1016), XM(-264), YM(-496), XM(-560), YM(-16));    // c 1
                path.line_to(XM(-560), YM(0));                                                //  l 1
                path.line_to(XM(-560), YM(16));                                               //  l 1
                path.cubic_to(XM(-264), YM(496), XM(-168), YM(1016), XM(-168), YM(1568));     // c 0
                path.cubic_to(XM(-168), YM(2680), XM(-544), YM(3800), XM(-544), YM(4960));    // c 0
                path.cubic_to(XM(-544), YM(5680), XM(-416), YM(6392), XM(-32), YM(7008));     // c 1
                path.cubic_to(XM(-22), YM(7022), XM(-8), YM(7024), XM(0), YM(7024));          // c 0
                path.cubic_to(XM(16), YM(7024), XM(38), YM(7008), XM(38), YM(6990));          // c 0
                path.cubic_to(XM(38), YM(6984), XM(36), YM(6974), XM(32), YM(6968));          // c 1
                path.cubic_to(XM(-264), YM(6488), XM(-360), YM(5952), XM(-360), YM(5400));    // c 0
                path.cubic_to(XM(-360), YM(4304), XM(-8), YM(3192), XM(-8), YM(2048));        // c 0
                path.cubic_to(XM(-8), YM(1320), XM(-136), YM(624), XM(-512), YM(0));          // c 1
                path.cubic_to(XM(-136), YM(-624), XM(-8), YM(-1320), XM(-8), YM(-2048));      // c 0*/
                ldata->path = path;
                ldata->set_bbox(path.bounding_rect());
                ldata->shape.add(ldata->bbox());
                ldata->bracket_width = w + conf.style_absolute(Sid::AKKOLADE_BAR_DISTANCE);
            } else {
                if (item->brace_symbol() == SymId::NO_SYM) {
                    ldata->brace_symbol = SymId::BRACE;
                }
                double h = ldata->bracket_height;
                double w = item->sym_width(ldata->brace_symbol) * item->magx();
                ldata->set_bbox(RectF(0, 0, w, h));
                ldata->shape.add(ldata->bbox());
                ldata->bracket_width = w + conf.style_absolute(Sid::AKKOLADE_BAR_DISTANCE);
            }
        } break;
        case BracketType::NORMAL: {
            double w = conf.style_absolute(Sid::BRACKET_WIDTH) * .5;
            double bd = item->spatium() * (conf.style_st(Sid::MUSICAL_SYMBOL_FONT) == "Leland" ? .5 : .25);

            Shape shape = RectF(-w, -bd, w * 2, 2 * (ldata->bracket_height * 0.5 + bd));
            shape.add(item->sym_bbox(SymId::BRACKET_TOP).translated(PointF(-w, -bd)));
            shape.add(item->sym_bbox(SymId::BRACKET_BOTTOM).translated(PointF(-w, bd + ldata->bracket_height)));
            ldata->set_shape(shape);

            ldata->bracket_width = conf.style_absolute(Sid::BRACKET_WIDTH) + conf.style_absolute(Sid::BRACKET_DISTANCE);
        } break;
        case BracketType::SQUARE: {
            double w = conf.style_absolute(Sid::STAFF_LINE_WIDTH) * .5;
            double x = -w;
            double y = -w;
            double h = (ldata->bracket_height * 0.5 + w) * 2;
            w += (.5 * item->spatium() + 2 * w);
            ldata->set_bbox(RectF(x, y, w, h));
            ldata->shape.add(ldata->bbox());

            ldata->bracket_width = conf.style_absolute(Sid::STAFF_LINE_WIDTH) / 2 + 0.5 * item->spatium();
        } break;
        case BracketType::LINE: {
            double spatium_value = item->spatium();
            double w = 0.67 * conf.style_absolute(Sid::BRACKET_WIDTH) * .5;
            double x = -w;
            double bd = spatium_value * .25;
            double y = -bd;
            double h = (-y + ldata->bracket_height * 0.5) * 2;
            ldata->set_bbox(RectF(x, y, w, h));
            ldata->shape.add(ldata->bbox());

            ldata->bracket_width = 0.67 * conf.style_absolute(Sid::BRACKET_WIDTH) + conf.style_absolute(Sid::BRACKET_DISTANCE);
        } break;
        case BracketType::GROUP: layout_group_bracket(item, ldata, conf); break;
        case BracketType::NO_BRACKET: break;
    }
}

void TLayout::layout_group_bracket(const Bracket *item, Bracket::LayoutData *ldata, const LayoutConfiguration &conf) {
    BracketItem *bracket_item = item->bracket_item();

    double w = conf.style_absolute(Sid::GROUP_BRACKET_LINE_WIDTH) * 0.5;
    double x = 0.0;
    double y = -w;
    double h = (ldata->bracket_height * 0.5 + w) * 2;
    double width = conf.style_absolute(Sid::GROUP_BRACKET_HOOK_LEN);

    ldata->bracket_width = std::max(width, 2 * w);

    Shape shape;
    shape.add(RectF(x, y, width, h), item);
    ldata->set_shape(shape);

    if (!item->text()) {
        Text *bracket_text = new Text(const_cast<Bracket *>(item), TextStyleType::GROUP_BRACKET);
        bracket_text->set_selected(item->selected());
        bracket_text->set_generated(true);
        const_cast<Bracket *>(item)->set_text(bracket_text);
    }

    Text *text = item->text();
    text->set_visible(bracket_item->visible() && bracket_item->show_text());
    text->set_align(Align(text->align().horizontal, AlignV::VCENTER));
    text->set_position(AlignH::HCENTER);

    Orientation text_orientation = conf.style_v(Sid::GROUP_BRACKET_TEXT_ORIENTATION).value<Orientation>();
    text->set_text_angle(text_orientation == Orientation::VERTICAL ? -90.0 : 0.0);

    double text_padding = 0.5 * item->spatium();
    double bracket_height = item->ldata()->bbox().height();
    double min_height = bracket_height - 6 * text_padding;

    if (text_orientation == Orientation::HORIZONTAL) {
        text->set_xml_text(item->label_type() == InstrumentNameType::LONG ? bracket_item->long_name() : bracket_item->short_name());
        layout_text(text, text->mutldata());
    } else {
        // Vertical labels ignore labelType(): use the long name if it fits, otherwise the short name
        text->set_xml_text(bracket_item->long_name());
        layout_text(text, text->mutldata());
        if (text->ldata()->bbox().height() > min_height) {
            text->set_xml_text(bracket_item->short_name());
            layout_text(text, text->mutldata());
        }
    }

    text->mutldata()->set_pos_y(bracket_height / 2);

    switch (conf.style_v(Sid::GROUP_BRACKET_TEXT_ALIGN).value<DirectionH>()) {
        case DirectionH::AUTO: {
            text->mutldata()->set_pos_x(0.0);
            if (text->visible()) {
                RectF mask = text->ldata()->bbox().translated(text->pos()).padded(text_padding);
                ldata->set_mask(mask);
            }
            break;
        }
        case DirectionH::LEFT: text->mutldata()->set_pos_x(-text->mutldata()->bbox().right() - text_padding); break;
        case DirectionH::RIGHT: text->mutldata()->set_pos_x(-text->mutldata()->bbox().left() + text_padding);
    }

    shape.add(text->ldata()->shape().translated(text->pos()));
    ldata->set_shape(shape);
}

void TLayout::layout_breath(const Breath *item, Breath::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    // TODO review with all other instances of this
    // if (ldata->isValid()) {
    // return;
    // }

    const double voice_offset = item->place_below() ? item->staff()->staff_height(item->tick()) : 0.0;
    if (item->is_caesura()) {
        if (item->sym_id() == SymId::CHANT_CAESURA) {
            // special handling for chant caesura, which is higher than the normal one
            ldata->set_pos_y(1.5 * item->spatium() + voice_offset);
        } else {
            ldata->set_pos_y(item->sym_height(item->sym_id()) / 2 + voice_offset);
        }
    } else if ((conf.style_st(Sid::MUSICAL_SYMBOL_FONT) == "Emmentaler") && (item->sym_id() == SymId::BREATH_MARK_COMMA)) {
        const double shift = item->place_below() ? -0.5 * item->spatium() + item->sym_height(item->sym_id()) : 0.5 * item->spatium();
        ldata->set_pos_y(shift + voice_offset);
    } else {
        const double shift = item->place_below() ? 0.5 * item->spatium() + item->sym_height(item->sym_id()) : -0.5 * item->spatium();
        ldata->set_pos_y(shift + voice_offset);
    }

    ldata->set_bbox(item->sym_bbox(item->sym_id()));

    ldata->set_pos_x(-ldata->bbox().right());
}

void TLayout::layout_chord(Chord *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    ChordLayout::layout(item, ctx);
}

void TLayout::layout_chord_line(const ChordLine *item, ChordLine::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    Note *note = nullptr;
    const double spatium = item->spatium();

    if (item->note()) {
        note = item->chord()->find_note(item->note()->pitch());
    }

    if (!note) {
        note = item->chord()->up_note();
    }

    const Note::LayoutData *note_ld = note->ldata();

    //! NOTE Temporary, instead of item->chord()->shape()
    LD_CONDITION(item->chord()->ldata()->is_set_bbox());

    LD_CONDITION(note_ld->is_set_pos());
    LD_CONDITION(note_ld->is_set_bbox());

    ldata->set_mag(item->chord()->mag());

    if (item->has_source_offsets()) {
        ldata->path = item->source_offset_path(spatium, item->chord()->intrinsic_mag());
    } else if (!item->modified()) {
        double x2 = 0;
        double y2 = 0;
        double base_length = spatium * item->chord()->intrinsic_mag();
        double hor_base_length = 1.2 * base_length; // let the symbols extend a bit more horizontally
        x2 += item->is_to_the_left() ? -hor_base_length : hor_base_length;
        y2 += item->is_below() ? base_length : -base_length;
        if (item->chord_line_type() != ChordLineType::NOTYPE && !item->is_wavy()) {
            PainterPath path;
            if (!item->is_to_the_left()) {
                if (item->is_straight()) {
                    path.line_to(x2, y2);
                } else {
                    path.cubic_to(x2 / 2, 0.0, x2, y2 / 2, x2, y2);
                }
            } else {
                if (item->is_straight()) {
                    path.line_to(x2, y2);
                } else {
                    path.cubic_to(0.0, y2 / 2, x2 / 2, y2, x2, y2);
                }
            }
            ldata->path = path;
        }
    }

    double x = 0.0;
    double y = note_ld->pos().y();
    double hor_offset = 0.33 * spatium;  // one third of a space away from the note
    double vert_offset = 0.25 * spatium; // one quarter of a space from the center line
    // Get chord shape
    Shape chord_shape = item->chord()->shape();
    // ...but remove from the shape items that the chordline shouldn't try to avoid
    // (especially the chordline itself)
    chord_shape.remove_if([](ShapeElement &shape_el) {
        if (!shape_el.item()) {
            return true;
        }
        const EngravingItem *item = shape_el.item();
        if (item->is_chord_line() || item->is_harmony() || item->is_lyrics()) {
            return true;
        }
        return false;
    });

    const RectF r = item->is_wavy() ? conf.engraving_font()->bbox(item->wave_sym(), item->mag_s()) : ldata->path.bounding_rect();

    y += item->is_below() ? vert_offset : -vert_offset;

    const double y_below = y + r.height();
    double chord_edge =
        item->is_to_the_left() ? chord_shape.left_most_edge_at_height(y, y_below) : chord_shape.right_most_edge_at_height(y, y_below);

    if (!item->is_to_the_left()) {
        // Always place lines to the right of dots
        double x_min = chord_shape.left();
        for (ShapeElement &shape_el : chord_shape.elements()) {
            if (!shape_el.item() || !shape_el.item()->is_note_dot()) {
                continue;
            }

            x_min = std::max(x_min, shape_el.right());
        }
        chord_edge = std::max(chord_edge, x_min);
    }

    x += item->is_to_the_left() ? chord_edge - hor_offset : chord_edge + hor_offset;
    ldata->set_pos(x, y);

    if (!item->is_wavy()) {
        int x1 = 0, y1 = 0, width = 0, height = 0;

        x1 = r.x();
        y1 = r.y();
        width = r.width();
        height = r.height();
        ldata->set_bbox(x1, y1, width, height);
    } else {
        // Align an eighth space above/below the centre of the note
        if (item->is_below()) {
            ldata->move_y(r.height() - spatium * 0.125);
        } else {
            ldata->move_y(spatium * 0.075);
        }

        if (item->is_to_the_left()) {
            ldata->move_x(-r.width());
        }

        ldata->set_bbox(r);
    }
}

void TLayout::layout_clef(const Clef *item, Clef::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    // determine current number of lines and line distance
    int lines = 5;
    Spatium line_dist = 1.0_sp;
    int step_offset = 0;
    Spatium staff_offset_y = 0_sp;

    Segment *clef_seg = item->segment();

    // check clef visibility and type compatibility
    if (clef_seg && item->staff()) {
        StaffType *st_visibility = item->staff()->staff_type(item->tick());
        bool show = st_visibility->gen_clef(); // check staff type allows clef display

        const bool end_of_measure_clef = clef_seg->rtick() == clef_seg->measure()->ticks();
        const Fraction tick = end_of_measure_clef && !item->is_trailer() ? item->tick() : clef_seg->measure()->tick();

        const Fraction tick_prev = tick - Fraction::eps();
        const StaffType *st = item->staff()->staff_type(tick);
        const StaffType *st_prev = !tick_prev.negative() ? item->staff()->staff_type(tick_prev) : nullptr;
        StaffGroup staff_group = st->group();
        const bool hide_clef = st->is_tab_staff() ? conf.style_b(Sid::HIDE_TAB_CLEF_AFTER_FIRST) : !conf.style_b(Sid::GEN_CLEF);

        // if not tab, use instrument->useDrumset to set staffGroup (to allow pitched to unpitched in same staff)
        if (staff_group != StaffGroup::TAB) {
            staff_group = item->origin_part()->instrument(item->tick())->use_drumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
        }

        // check clef is compatible with staff type group:
        if (ClefInfo::staff_group(item->clef_type()) != staff_group) {
            if (tick > Fraction(0, 1) && (!item->generated() || hide_clef)) { // if clef is not generated, hide it
                show = false;
            } else { // if generated, replace with initial clef type
                // TODO : instead of initial staff clef (which is assumed to be compatible)
                // use the last compatible clef previously found in staff
                const_cast<Clef *>(item)->set_clef_type(item->staff()->clef_type(Fraction(0, 1)));
            }
        }

        // if clef not to show or not compatible with staff group
        if (!show) {
            ldata->set_bbox(RectF());
            ldata->sym_id = SymId::NO_SYM;
            LOGD("invisible clef at tick %d(%d) staff %zu", item->segment()->tick().ticks(), item->segment()->tick().ticks() / 1920,
                 item->staff_idx());
            return;
        }
        lines = st->lines(); // init values from staff type
        line_dist = st->line_distance();
        step_offset = st->step_offset();

        const Spatium st_offset = st->yoffset();
        const Spatium st_prev_offset = st_prev && clef_seg->rtick() != Fraction(0, 1) ? st_prev->yoffset() : 0.0_sp;
        staff_offset_y = item->is_header() ? 0.0_sp : st_offset - st_prev_offset;
    }

    double spatium_value = item->spatium();
    Spatium yoff = 0.0_sp;
    if (item->clef_type() != ClefType::INVALID && item->clef_type() != ClefType::MAX) {
        ldata->sym_id = ClefInfo::sym_id(item->clef_type());
        yoff = line_dist * (5 - ClefInfo::line(item->clef_type()));
    } else {
        ldata->sym_id = SymId::NO_SYM;
    }

    switch (item->clef_type()) {
        case ClefType::C_19C: // 19th C clef is like a G clef
            yoff = line_dist * 1.5;
            break;
        case ClefType::TAB:        // TAB clef
        case ClefType::TAB4:       // TAB clef 4 strings
        case ClefType::TAB_SERIF:  // TAB clef alternate style
        case ClefType::TAB4_SERIF: // TAB clef alternate style
            // on tablature, position clef at half the number of spaces * line distance
            yoff = line_dist * (lines - 1) * .5;
            step_offset = 0; //  ignore stepOffset for TAB and percussion clefs
            break;
        case ClefType::PERC: // percussion clefs
        case ClefType::PERC2:
            yoff = line_dist * (lines - 1) * 0.5;
            step_offset = 0;
            break;
        case ClefType::INVALID:
        case ClefType::MAX: LOGD("Clef::layout: invalid type"); return;
        default: break;
    }
    // clefs on palette or at start of system/measure are left aligned
    // other clefs are right aligned
    Shape shape(item->sym_shape_with_cutouts(ldata->sym_id));
    bool is_mid_measure_clef = item->is_mid_measure_clef();
    double x = is_mid_measure_clef ? -shape.right() : 0.0;
    ldata->set_pos(PointF(x, (yoff + (Spatium(step_offset) * 0.5) + staff_offset_y).to_absolute(spatium_value)));
    if (item->is_mid_measure_clef()) {
        ldata->set_shape(shape);
    } else {
        ldata->set_bbox(item->sym_bbox(ldata->sym_id));
    }
}

void TLayout::layout_capo(const Capo *item, Capo::LayoutData *ldata, const LayoutContext &) {
    LAYOUT_CALL_ITEM(item);
    //! NOTE Looks like it doesn't belong here
    if (item->should_automatically_generate_text() || item->empty()) {
        if (const Part *part = item->origin_part()) {
            if (const StringData *string_data = part->string_data(item->tick(), item->staff()->idx())) {
                String text = item->generate_text(string_data->strings());
                const_cast<Capo *>(item)->set_xml_text(text);
            }
        }
    }

    TextLayout::layout_base_text_base(item, ldata);

    if (item->autoplace()) {
        const Segment *s = item->segment();
        const Measure *m = s->measure();
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(m->ldata()->is_set_pos());
        LD_CONDITION(s->ldata()->is_set_pos());
    }

    Autoplace::autoplace_segment_element(item, ldata);
}

void TLayout::layout_dead_slapped(const DeadSlapped *item, DeadSlapped::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    if (ldata->is_valid()) {
        return;
    }

    const double dead_slapped_width = item->spatium() * 2;
    RectF rect = RectF(0, 0, dead_slapped_width, item->staff()->staff_height());
    ldata->set_bbox(rect);
    ldata->set_pos(PointF());

    // fillPath
    {
        constexpr double cross_thinkness_percentage = 0.1;
        double height = rect.height();
        double width = rect.width();
        double cross_thickness = width * cross_thinkness_percentage;

        PointF top_left = PointF(rect.x(), rect.y());
        PointF bottom_right = top_left + PointF(width, height);
        PointF top_right = top_left + PointF(width, 0);
        PointF bottom_left = top_left + PointF(0, height);
        PointF offset_x = PointF(cross_thickness, 0);

        PainterPath path1;
        path1.move_to(top_left);
        path1.line_to(top_left + offset_x);
        path1.line_to(bottom_right);
        path1.line_to(bottom_right - offset_x);
        path1.line_to(top_left);

        PainterPath path2;
        path2.move_to(top_right);
        path2.line_to(top_right - offset_x);
        path2.line_to(bottom_left);
        path2.line_to(bottom_left + offset_x);
        path2.line_to(top_right);

        ldata->path1 = path1;
        ldata->path2 = path2;
    }
}

void TLayout::layout_dynamic(Dynamic *item, Dynamic::LayoutData *ldata, const LayoutConfiguration &conf) {
    DynamicsLayout::layout_dynamic(item, ldata, conf);
}

void TLayout::layout_expression(const Expression *item, Expression::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    const_cast<Expression *>(item)->set_placement_based_on_voice_assignment(
        item->style().style_v(Sid::DYNAMICS_HAIRPIN_VOICE_BASED_PLACEMENT).value<DirectionV>());

    TextLayout::layout_base_text_base(item, ldata);

    const Segment *segment = item->segment();

    if (item->align().horizontal != AlignH::LEFT) {
        Chord *chord_to_align = nullptr;
        // Look for chord in this staff
        track_idx_t start_track = track2staff(item->staff_idx());
        track_idx_t end_track = start_track + ::mu::engraving::voices;
        for (track_idx_t track = start_track; track < end_track; ++track) {
            EngravingItem *e = segment->element(track);
            if (e && e->is_chord()) {
                chord_to_align = to_chord(e);
                break;
            }
        }

        if (chord_to_align && !chord_to_align->notes().empty()) {
            const Note *note = chord_to_align->notes().at(0);
            double head_width = note->head_width();
            bool center = item->align().horizontal == AlignH::HCENTER;
            ldata->move_x(head_width * (center ? 0.5 : 1));
        }
    }

    ldata->disconnect_snapped_items();

    if (!item->autoplace()) {
        return;
    }

    const Segment *s = item->segment();
    const Measure *m = s->measure();
    LD_CONDITION(ldata->is_set_pos());
    LD_CONDITION(m->ldata()->is_set_pos());
    LD_CONDITION(s->ldata()->is_set_pos());

    Dynamic *dynamic = to_dynamic(segment->find_annotation(ElementType::DYNAMIC, item->track(), item->track()));
    if (!dynamic) {
        Segment *time_tick_seg = m->find_segment_r(SegmentType::TIME_TICK, s->rtick());
        dynamic = time_tick_seg ? to_dynamic(time_tick_seg->find_annotation(ElementType::DYNAMIC, item->track(), item->track())) : nullptr;
    }
    if (!dynamic || dynamic->place_above() != item->place_above() || dynamic->voice_assignment() != item->voice_assignment() ||
        !dynamic->visible()) {
        Autoplace::autoplace_segment_element(item, ldata);
        return;
    }

    LD_CONDITION(dynamic->ldata()->is_set_bbox()); // dynamic->shape()
    LD_CONDITION(dynamic->ldata()->is_set_pos());

    // If there is a dynamic on same segment and track make space for it horizontally
    double padding = item->compute_dynamic_expression_distance(dynamic);
    double dynamic_right = dynamic->shape().translate(dynamic->pos()).right();
    double expression_left = ldata->bbox().translated(item->pos()).left();
    double difference = expression_left - dynamic_right - padding;
    ldata->move_x(-difference);

    if (item->snap_to_dynamics()) {
        ldata->connect_item_snapped_before(dynamic);
    }

    Autoplace::autoplace_segment_element(item, ldata);
}

void TLayout::layout_fermata(const Fermata *item, Fermata::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    const StaffType *st_type = item->staff_type();
    if (st_type && st_type->is_hidden_element_on_tab(Sid::FERMATA_SHOW_TAB_COMMON, Sid::FERMATA_SHOW_TAB_SIMPLE)) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);
    ldata->set_pos(PointF());

    double x = 0.0;
    double y = item->place_above() ? 0.0 : item->staff()->staff_height(item->tick());
    const Segment *s = item->segment();
    const track_idx_t layout_parent_track =
        s->is_type(SegmentType::BAR_LINE_TYPES) ? item->staff_idx() * ::mu::engraving::voices : item->track();
    const EngravingItem *e = s->element(layout_parent_track);

    if (e) {
        LD_CONDITION(e->ldata()->is_set_bbox()); // e->shape()
        LD_CONDITION(e->ldata()->is_set_pos());

        if (e->is_chord()) {
            const Chord *chord = to_chord(e);
            x = chord->x() + ChordLayout::center_x(chord);
        } else if (e->is_rest()) {
            const Rest *rest = to_rest(e);
            x = rest->x() + rest->center_x();
        } else {
            x = e->x() + e->shape().left() + e->width() * item->staff()->staff_mag(Fraction(0, 1)) * .5;
        }
    }

    String name = String::from_ascii(SymNames::name_for_sym_id(item->sym_id()).ascii());
    if (item->place_above()) {
        if (name.ends_with(u"Below")) {
            //! NOTE It is not clear whether SymId is layout data or given data.
            const_cast<Fermata *>(item)->set_sym_id(SymNames::sym_id_by_name(name.left(name.size() - 5) + u"Above"));
        }
    } else {
        if (name.ends_with(u"Above")) {
            //! NOTE It is not clear whether SymId is layout data or given data.
            const_cast<Fermata *>(item)->set_sym_id(SymNames::sym_id_by_name(name.left(name.size() - 5) + u"Below"));
        }
    }

    ldata->set_shape(Shape(item->sym_bbox(item->sym_id()), item));
    x -= 0.5 * ldata->bbox().width();

    ldata->set_pos(x, y);

    PointF default_pos = item->default_pos();
    ldata->move(default_pos);

    if (item->autoplace()) {
        const Segment *s2 = item->segment();
        const Measure *m = s2->measure();
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(m->ldata()->is_set_pos());
        LD_CONDITION(s2->ldata()->is_set_pos());
    }

    Autoplace::autoplace_segment_element(item, ldata);
}

//---------------------------------------------------------
//   FiguredBassItem layout
//    creates the display text (set as element text) and computes
//    the horiz. offset needed to align the right part as well as the vert. offset
//---------------------------------------------------------
void TLayout::layout_figured_bass_item(const FiguredBassItem *item, FiguredBassItem::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    // construct font metrics
    int font_idx = 0;
    Font f(FiguredBass::fb_fonts().at(font_idx).family, Font::Type::TABLATURE);

    // font size in pixels, scaled according to spatium()
    // (use the same font selection as used in draw() below)
    double m = ctx.conf().style_d(Sid::FIGURED_BASS_FONT_SIZE) * item->spatium() / item->default_spatium();
    f.set_point_size_f(m);
    FontMetrics fm(f);

    String str;
    double x = item->sym_width(SymId::NOTEHEAD_BLACK) * .5;
    double x1 = 0.0;
    double x2 = 0.0;

    // create display text
    int font = 0;
    int style = ctx.conf().style_i(Sid::FIGURED_BASS_STYLE);

    if (item->parenth1() != FiguredBassItem::Parenthesis::NONE) {
        str.append(FiguredBass::fb_fonts().at(font).display_parenthesis[int(item->parenth1())]);
    }

    // prefix
    if (item->prefix() != FiguredBassItem::Modifier::NONE) {
        // if no digit, the string created so far 'hangs' to the left of the note
        if (item->digit() == ::mu::engraving::fbi_digit_none) {
            x1 = fm.width(str);
        }
        str.append(FiguredBass::fb_fonts().at(font).display_accidental[int(item->prefix())]);
        // if no digit, the string from here onward 'hangs' to the right of the note
        if (item->digit() == ::mu::engraving::fbi_digit_none) {
            x2 = fm.width(str);
        }
    }

    if (item->parenth2() != FiguredBassItem::Parenthesis::NONE) {
        str.append(FiguredBass::fb_fonts().at(font).display_parenthesis[int(item->parenth2())]);
    }

    // digit
    if (item->digit() != ::mu::engraving::fbi_digit_none) {
        // if some digit, the string created so far 'hangs' to the left of the note
        x1 = fm.width(str);
        // if suffix is a combining shape, combine it with digit (multi-digit numbers cannot be combined)
        // unless there is a parenthesis in between
        if ((item->digit() < 10) &&
            (item->suffix() == FiguredBassItem::Modifier::CROSS || item->suffix() == FiguredBassItem::Modifier::BACKSLASH ||
             item->suffix() == FiguredBassItem::Modifier::SLASH) &&
            item->parenth3() == FiguredBassItem::Parenthesis::NONE) {
            str.append(FiguredBass::fb_fonts().at(font).display_digit[style][item->digit()]
                                                                     [int(item->suffix()) - (int(FiguredBassItem::Modifier::CROSS) - 1)]);
        }
        // if several digits or no shape combination, convert _digit to font styled chars
        else {
            String digits;
            int digit = item->digit();
            while (true) {
                digits.prepend(FiguredBass::fb_fonts().at(font).display_digit[style][(digit % 10)][0]);
                digit /= 10;
                if (digit == 0) {
                    break;
                }
            }
            str.append(digits);
        }
        // if some digit, the string from here onward 'hangs' to the right of the note
        x2 = fm.width(str);
    }

    if (item->parenth3() != FiguredBassItem::Parenthesis::NONE) {
        str.append(FiguredBass::fb_fonts().at(font).display_parenthesis[int(item->parenth3())]);
    }

    // suffix
    // append only if non-combining shape or cannot combine (no digit or parenthesis in between)
    if (item->suffix() != FiguredBassItem::Modifier::NONE &&
        ((item->suffix() != FiguredBassItem::Modifier::CROSS && item->suffix() != FiguredBassItem::Modifier::BACKSLASH &&
          item->suffix() != FiguredBassItem::Modifier::SLASH) ||
         item->digit() == ::mu::engraving::fbi_digit_none || item->parenth3() != FiguredBassItem::Parenthesis::NONE)) {
        str.append(FiguredBass::fb_fonts().at(font).display_accidental[int(item->suffix())]);
    }

    if (item->parenth4() != FiguredBassItem::Parenthesis::NONE) {
        str.append(FiguredBass::fb_fonts().at(font).display_parenthesis[int(item->parenth4())]);
    }

    //! NOTE Look like, this text not layout data (not depends by position)
    ldata->display_text = str; // this text will be displayed

    if (str.size()) {            // if some text
        x = x - (x1 + x2) * 0.5; // position the text so that [x1<-->x2] is centered below the note
    } else {                     // if no text (but possibly a line)
        x = 0;                   // start at note left margin
    }
    // vertical position
    double h = fm.line_spacing();
    h *= ctx.conf().style_d(Sid::FIGURED_BASS_LINE_HEIGHT);
    double y = 0.0;
    if (ctx.conf().style_i(Sid::FIGURED_BASS_ALIGNMENT) == 0) { // top alignment: stack down from first item
        y = h * item->ord();
    } else { // bottom alignment: stack up from last item
        y = -h * (item->figured_bass()->items_count() - item->ord());
    }
    ldata->set_pos(x, y);
    // determine bbox from text width
    //      w = fm.width(str);
    double w = fm.width(str);
    ldata->text_width = w;
    // if there is a cont.line, extend width to cover the whole FB element duration line
    int line_len = 0;
    if (item->cont_line() != FiguredBassItem::ContLine::NONE && (line_len = item->figured_bass()->ldata()->line_length(0)) > w) {
        w = line_len;
    }
    ldata->set_bbox(0, 0, w, h);
}

//    lays out the duration indicator line(s), filling the _lineLengths array
//    and the length of printed lines (used by continuation lines)

static void layout_lines(const FiguredBass *item, FiguredBass::LayoutData *ldata, const LayoutContext &ctx) {
    std::vector<double> line_lengths = ldata->line_lengths;
    if (item->ticks() <= Fraction(0, 1) || !item->segment()) {
        line_lengths.resize(1); // be sure to always have
        line_lengths[0] = 0;    // at least 1 item in array
        ldata->line_lengths = line_lengths;
        return;
    }

    ChordRest *last_cr = nullptr;                                 // the last ChordRest of this
    Segment *next_segm = nullptr;                                 // the Segment beyond this' segment
    Fraction next_tick = item->segment()->tick() + item->ticks(); // the tick beyond this' duration

    // locate the measure containing the last tick of this; it is either:
    // the same measure containing nextTick, if nextTick is not the first tick of a measure
    //    (and line should stop right before it)
    // or the previous measure, if nextTick is the first tick of a measure
    //    (and line should stop before any measure terminal segment (bar, clef, ...) )

    const Measure *m = ctx.dom().tick2measure(next_tick - Fraction::eps());
    if (m) {
        // locate the first segment (of ANY type) right after this' last tick
        for (next_segm = m->first(SegmentType::ALL); next_segm; next_segm = next_segm->next()) {
            if (next_segm->tick() >= next_tick) {
                break;
            }
        }
        // locate the last ChordRest of this
        if (next_segm) {
            track_idx_t start_track = track_zero_voice(item->track());
            track_idx_t end_track = start_track + ::mu::engraving::voices;
            for (const Segment *seg = next_segm->prev1(); seg; seg = seg->prev1()) {
                for (track_idx_t t = start_track; t < end_track; ++t) {
                    EngravingItem *el = seg->element(t);
                    if (el && el->is_chord_rest()) {
                        last_cr = to_chord_rest(el);
                        break;
                    }
                }
                if (last_cr) {
                    break;
                }
            }
        }
    }
    if (!m || !next_segm) {
        LOGD("FiguredBass layout: no segment found for tick %d", next_tick.ticks());
        line_lengths.resize(1); // be sure to always have
        line_lengths[0] = 0;    // at least 1 item in array
        ldata->line_lengths = line_lengths;
        return;
    }

    // get length of printed lines from horiz. page position of lastCR
    // (enter a bit 'into' the ChordRest for clarity)
    double printed_line_length = last_cr ? last_cr->page_x() - item->page_x() + 1.5 * item->spatium() : 3 * item->spatium();
    ldata->printed_line_length = printed_line_length;

    // get duration indicator line(s) from page position of nextSegm
    const std::vector<System *> &systems = ctx.dom().systems();
    System *s1 = item->segment()->measure()->system();
    System *s2 = next_segm->measure()->system();
    system_idx_t sys_idx1 = muse::index_of(systems, s1);
    system_idx_t sys_idx2 = muse::index_of(systems, s2);

    if (sys_idx1 == muse::nidx) {
        // The measure's system is not in the score's system list: a range layout restores a
        // system it has already detached from that list, and then there is no page position
        // to measure a line against. Draw the line to the next segment, as for a single
        // system below; the loop must not run from muse::nidx, which is one step short of a
        // full wrap-around.
        sys_idx1 = 0;
        sys_idx2 = 0;
        next_segm = item->segment()->next1();
    } else if (sys_idx2 == muse::nidx || sys_idx2 < sys_idx1) {
        sys_idx2 = sys_idx1;
        next_segm = item->segment()->next1();
        // TODO
        // During layout of figured bass next systems' numbers may be still
        // undefined (then sysIdx2 == muse::nidx) or change in the future.
        // A layoutSystem() approach similar to that for spanners should
        // probably be implemented.
    }

    system_idx_t i;
    int len;
    size_t seg_idx = 0;
    for (i = sys_idx1, seg_idx = 0; i <= sys_idx2; ++i, ++seg_idx) {
        len = 0;
        if (sys_idx1 == sys_idx2 || i == sys_idx1) {
            // single line
            len = next_segm->page_x() - item->page_x() - 4; // stop 4 raster units before next segm
        } else if (i == sys_idx1) {
            // initial line
            double w = s1->staff(item->staff_idx())->bbox().right();
            double x = s1->page_x() + w;
            len = x - item->page_x();
        } else if (i > 0 && i != sys_idx2) {
            // middle line
            LOGD("FiguredBass: duration indicator middle line not implemented");
        } else if (i == sys_idx2) {
            // end line
            LOGD("FiguredBass: duration indicator end line not implemented");
        }
        // store length item, reusing array items if already present
        if (line_lengths.size() <= seg_idx) {
            line_lengths.push_back(len);
        } else {
            line_lengths[seg_idx] = len;
        }
    }
    // if more array items than needed, truncate array
    if (line_lengths.size() > seg_idx) {
        line_lengths.resize(seg_idx);
    }

    ldata->line_lengths = line_lengths;
}

void TLayout::layout_figured_bass(const FiguredBass *item, FiguredBass::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    // VERTICAL POSITION:
    const double y = ctx.conf().style_d(Sid::FIGURED_BASS_Y_OFFSET) * item->spatium();
    ldata->set_pos(PointF(0.0, y));

    // BOUNDING BOX and individual item layout (if required)
    TextLayout::layout_base_text_base1(item, ldata); // prepare structs and data expected by Text methods
    // if element could be parsed into items, layout each element
    // Items list will be empty in edit mode (see FiguredBass::startEdit).
    // TODO: consider disabling specific layout in case text style is changed (tid() != TextStyleName::FIGURED_BASS).
    if (item->items().size() > 0) {
        Shape shape;
        layout_lines(item, ldata, ctx);
        // layout each item and enlarge bbox to include items bboxes
        for (FiguredBassItem *fit : item->items()) {
            FiguredBassItem::LayoutData *fildata = fit->mutldata();
            layout_figured_bass_item(fit, fildata, ctx);
            shape.add(fildata->bbox().translated(fit->pos()), fit);
        }
        ldata->set_shape(shape);
    }
}

void TLayout::layout_fingering(const Fingering *item, Fingering::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    Fraction tick = item->ownership_parent_item()->tick();
    const Staff *st = item->staff();
    if (st && st->is_tab_staff(tick) &&
        (!st->staff_type(tick)->show_tab_fingering() || item->text_style_type() == TextStyleType::STRING_NUMBER)) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);

    TextLayout::layout_base_text_base(item, ldata);
    ldata->set_pos_y(0.0); // handle placement below

    if (item->autoplace()) {
        const Note *note = item->note();
        const Chord *chord = note->chord();

        bool voices = chord->measure()->has_voices(chord->staff_idx(), chord->tick(), chord->actual_ticks());
        bool tight = voices && chord->notes().size() == 1 && !chord->beam() && item->text_style_type() != TextStyleType::STRING_NUMBER;

        double head_width = note->bbox_right_pos();

        // temporarily exclude self from chord shape
        const_cast<Fingering *>(item)->set_autoplace(false);

        if (item->layout_type() == ElementType::CHORD) {
            bool above = item->place_above();
            const Stem *stem = chord->stem();
            const Segment *segment = chord->segment();
            const Measure *measure = segment->measure();
            double sp = item->spatium();
            double md = item->min_distance().val() * sp;
            const SysStaff *ss = measure->system()->staff(chord->v_staff_idx());
            const Staff *v_staff = chord->staff(); // TODO: use current height at tick

            LD_CONDITION(measure->ldata()->is_set_pos());
            LD_CONDITION(segment->ldata()->is_set_pos());
            LD_CONDITION(chord->ldata()->is_set_pos());
            LD_CONDITION(note->ldata()->is_set_pos());
            LD_CONDITION(note->ldata()->is_set_bbox());
            //! NOTE A lot of spam
            // LD_CONDITION(note->ldata()->mirror.has_value());
            if (stem) {
                LD_CONDITION(stem->ldata()->is_set_pos());
                LD_CONDITION(stem->ldata()->is_set_bbox());
            }
            LD_CONDITION(chord->up_note()->ldata()->is_set_bbox());
            LD_CONDITION(chord->up_note()->ldata()->is_set_pos());
            LD_CONDITION(chord->down_note()->ldata()->is_set_bbox());
            LD_CONDITION(chord->down_note()->ldata()->is_set_pos());

            if (note->ldata()->mirror.value(LdAccess::BAD)) {
                ldata->move_x(-note->ldata()->pos().x());
            }
            ldata->move_x(head_width * .5);
            if (above) {
                if (tight) {
                    if (chord->stem()) {
                        ldata->move_x(-0.8 * sp);
                    }
                    ldata->move_y(-1.5 * sp);
                } else {
                    RectF r = ldata->bbox().translated(measure->pos() + segment->pos() + chord->pos() + note->pos() + item->pos());
                    SkylineLine sk(false);
                    sk.add(r, const_cast<Fingering *>(item));
                    double d = sk.min_distance(ss->skyline().north());
                    double yd = 0.0;
                    if (d > 0.0 && item->is_styled(Pid::MIN_DISTANCE)) {
                        yd -= d + item->ldata()->bbox().height() * .25;
                    }
                    // force extra space above staff & chord (but not other fingerings)
                    double top = 0.0;
                    if (chord->up() && chord->beam() && stem) {
                        top = stem->y() + stem->ldata()->bbox().top();
                    } else {
                        const Note *un = chord->up_note();
                        top = std::min(0.0, un->y() + un->ldata()->bbox().top());
                    }
                    top -= md;
                    double diff = (ldata->bbox().bottom() + ldata->pos().y() + yd + note->y()) - top;
                    if (diff > 0.0) {
                        yd -= diff;
                    }
                    ldata->move_y(yd);
                }
            } else {
                if (tight) {
                    if (chord->stem()) {
                        ldata->move_x(0.8 * sp);
                    }
                    ldata->move_y(1.5 * sp);
                } else {
                    RectF r = ldata->bbox().translated(measure->pos() + segment->pos() + chord->pos() + note->pos() + item->pos());
                    SkylineLine sk(true);
                    sk.add(r, const_cast<Fingering *>(item));
                    double d = ss->skyline().south().min_distance(sk);
                    double yd = 0.0;
                    if (d > 0.0 && item->is_styled(Pid::MIN_DISTANCE)) {
                        yd += d + item->ldata()->bbox().height() * .25;
                    }
                    // force extra space below staff & chord (but not other fingerings)
                    double bottom;
                    if (!chord->up() && chord->beam() && stem) {
                        bottom = stem->y() + stem->ldata()->bbox().bottom();
                    } else {
                        const Note *dn = chord->down_note();
                        bottom = std::max(v_staff->staff_height(item->tick()), dn->y() + dn->ldata()->bbox().bottom());
                    }
                    bottom += md;
                    double diff = bottom - (ldata->bbox().top() + ldata->pos().y() + yd + note->y());
                    if (diff > 0.0) {
                        yd += diff;
                    }
                    ldata->move_y(yd);
                }
            }
        } else if (item->text_style_type() == TextStyleType::LH_GUITAR_FINGERING) {
            // place to left of note
            double left = note->shape().left();
            if (left + note->x() < 0.0) {
                ldata->move_x(left);
            } else {
                ldata->move_x(-note->x());
            }
        }
        // for other fingering styles, do not autoplace

        // restore autoplace
        const_cast<Fingering *>(item)->set_autoplace(true);
    }
}

void TLayout::layout_fret_diagram(const FretDiagram *item, FretDiagram::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    double spatium = item->spatium() * item->user_mag();
    ldata->string_line_width = spatium * 0.08;
    ldata->nut_line_width =
        ((item->fret_offset() || !item->show_nut()) ? ldata->string_line_width :
                                                      ctx.conf().style_absolute(Sid::FRET_NUT_THICKNESS) * item->user_mag());
    ldata->nut_y = -0.5 * (ldata->nut_line_width - ldata->string_line_width);
    ldata->string_dist = ctx.conf().style_absolute(Sid::FRET_STRING_SPACING) * item->user_mag();
    ldata->fret_dist = ctx.conf().style_absolute(Sid::FRET_FRET_SPACING) * item->user_mag();
    ldata->marker_size = ldata->string_dist * .8;
    ldata->marker_y = ldata->nut_y - 0.5 * ldata->nut_line_width - ldata->marker_size - 0.20 * spatium;
    bool extended_style = item->style().style_b(Sid::FRET_STYLE_EXTENDED);
    ldata->string_extend_top = item->fret_offset() && extended_style ? -spatium * .2 : 0.0;
    ldata->string_extend_bottom = extended_style ? 0.5 * ldata->fret_dist : 0.0;
    ldata->dot_diameter = ctx.conf().style_absolute(Sid::FRET_DOT_SPATIUM_SIZE) * item->user_mag();
    ldata->grid_height = 0.0;

    double shape_margin_above_diagram = ldata->fret_dist * 1.5;
    double w = ldata->string_dist * (item->strings() - 1) + ldata->marker_size;
    double h = item->frets() * ldata->fret_dist + ldata->string_extend_bottom + 0.5 * ldata->string_line_width + shape_margin_above_diagram;
    double y = -shape_margin_above_diagram;
    double x = -(ldata->marker_size * .5);

    bool is_vertical = item->orientation() == Orientation::VERTICAL;

    // Allocate space for fret offset number
    if (item->fret_offset() > 0) {
        double padding = 0.4 * ldata->string_dist;
        for (auto i : item->barres()) {
            FretItem::Barre barre = i.second;
            if (!barre.exists()) {
                continue;
            }
            if ((barre.start_string == 0 && item->num_pos() == 0) ||
                ((barre.end_string == -1 || barre.end_string == item->strings() - 1) && item->num_pos() == 1)) {
                padding += 0.20 * ldata->dot_diameter * ctx.conf().style_d(Sid::BARRE_LINE_WIDTH);
                break;
            }
        }
        Font scaled_font(item->fret_num_font());
        FontMetrics fm2(scaled_font);
        String fret_text = String::number(item->fret_offset() + 1);
        if (ctx.conf().style_b(Sid::FRET_USE_CUSTOM_SUFFIX)) {
            fret_text += ctx.conf().style_st(Sid::FRET_CUSTOM_SUFFIX);
        }
        ldata->fret_text = fret_text;
        double numw = is_vertical ? fm2.width(fret_text) : fm2.tight_bounding_rect(fret_text).height();
        double xdiff = numw + padding;
        w += xdiff;
        x += (item->num_pos() == 0) == is_vertical ? -xdiff : 0;
        ldata->fret_num_padding = padding;
    }

    if (!is_vertical) {
        double temp_w = w, temp_x = x;
        w = h;
        h = temp_w;
        x = y;
        y = temp_x;
    }

    // When changing how bbox is calculated, don't forget to update the centerX and rightX methods too.
    Shape shape;
    shape.add(RectF(x, y, w, h), item);

    ldata->grid_height = h;

    ldata->fingering_items.clear();
    if (item->show_fingering()) {
        const double padding = 0.2 * spatium;
        Font fingering_font = item->fingering_font();
        FontMetrics font_metrics(fingering_font);
        double font_height = font_metrics.cap_height();
        for (size_t i = 0; i < item->fingering().size(); ++i) {
            int finger = item->fingering()[i];
            if (finger == 0) {
                continue;
            }
            String finger_s = String::number(finger);
            double width = font_metrics.width(finger_s);
            double digit_height = font_metrics.tight_bounding_rect(finger_s).height();
            double x_off = -0.5 * width;
            double finger_x = is_vertical ? (item->strings() - i - 1) * ldata->string_dist + x_off : x + w + padding;
            double finger_y = is_vertical ? y + h + font_height + padding : i * ldata->string_dist + 0.5 * digit_height;
            ldata->fingering_items.push_back(FretDiagram::FingeringItem(finger_s, PointF(finger_x, finger_y)));
            shape.add(ShapeElement(font_metrics.tight_bounding_rect(finger_s).translated(PointF(finger_x, finger_y)), item));
        }
    }

    ldata->set_shape(shape);

    if (item->ownership_parent()->is_segment()) {
        // We need to get the width of the notehead/rest in order to position the fret diagram correctly
        Segment *p_seg = item->segment();
        double notehead_width = 0.0;
        if (p_seg->is_chord_rest_type()) {
            staff_idx_t idx = item->staff()->idx();
            for (EngravingItem *e = p_seg->first_element_of_segment(idx); e; e = p_seg->next_element_of_segment(e, idx)) {
                if (e->is_rest()) {
                    const Rest *r = to_rest(e);
                    LD_CONDITION(r->ldata()->sym.has_value());
                    notehead_width = item->sym_width(r->ldata()->sym());
                    break;
                } else if (e->is_note()) {
                    Note *n = to_note(e);
                    notehead_width = n->head_width();
                    break;
                }
            }
        }
        if (real_is_null(notehead_width)) {
            // If note or rest not found, use standard notehead width
            notehead_width = item->sym_width(SymId::NOTEHEAD_BLACK);
        }

        ldata->set_pos((notehead_width - item->main_width()) / 2, -(ldata->shape().bottom() + item->style_p(Sid::FRET_Y)));
    }

    Harmony *harmony = item->harmony();
    if (harmony) {
        TLayout::layout_harmony(harmony, harmony->mutldata(), ctx);
        if (item->visible() && harmony->autoplace()) {
            double vert_dist = ldata->bbox().top() - harmony->ldata()->bbox().translated(harmony->pos()).bottom();
            double diff = vert_dist - harmony->min_distance().val() * item->spatium();
            if (diff < 0) {
                harmony->mutldata()->move_y(diff);
            }
        }
    }

    ldata->slur_paths.clear();
    for (auto i : item->barres()) {
        FretItem::Barre barre = i.second;
        if (!barre.exists()) {
            continue;
        }
        int start_string = barre.start_string;
        int end_string = barre.end_string != -1 ? barre.end_string : item->strings() - 1;
        int fret = i.first;
        bool slur_style_barre = ctx.conf().style_b(Sid::BARRE_APPEARANCE_SLUR);
        for (int string = start_string; string <= end_string; ++string) {
            if (slur_style_barre) {
                const_cast<FretDiagram *>(item)->add_dot_for_dot_style_barre(string, fret);
            } else {
                const_cast<FretDiagram *>(item)->remove_dot_for_dot_style_barre(string, fret);
            }
        }
        int length = end_string - start_string;
        double inset_x = (length < 2 ? 1.25 : 2) * ldata->string_line_width;
        double inset_y = fret == 1 ? ldata->nut_line_width + ldata->string_line_width : length < 2 ? 2 * ldata->string_line_width : inset_x;
        double start_x = start_string * ldata->string_dist + inset_x;
        double end_x = (end_string) *ldata->string_dist - inset_x;
        double shoulder_xoffset = 0.2 * (end_x - start_x);
        double start_end_y = (fret - 1) * ldata->fret_dist - inset_y;
        double shoulder_y = start_end_y - (length < 2 ? 0.3 : length < 3 ? 0.4 : 0.5) * ldata->fret_dist;
        double slur_thickness = (length < 2 ? 0.08 : 0.1) * item->spatium() * item->user_mag();
        double shoulder_yfor = shoulder_y - slur_thickness;
        double shoulder_yback = shoulder_y + slur_thickness;
        PointF bezier1for = PointF(start_x + shoulder_xoffset, shoulder_yfor);
        PointF bezier2for = PointF(end_x - shoulder_xoffset, shoulder_yfor);
        PointF bezier1back = PointF(start_x + shoulder_xoffset, shoulder_yback);
        PointF bezier2back = PointF(end_x - shoulder_xoffset, shoulder_yback);
        PainterPath slur_path = PainterPath();
        slur_path.move_to(start_x, start_end_y);
        slur_path.cubic_to(bezier1for, bezier2for, PointF(end_x, start_end_y));
        slur_path.cubic_to(bezier2back, bezier1back, PointF(start_x, start_end_y));
        ldata->slur_paths.push_back(slur_path);
    }
}

void TLayout::layout_glissando(Glissando *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    if (item->start_element()) {
        item->set_tick(item->start_element()->tick());
    }

    if (item->end_element()) {
        item->set_tick2(item->end_element()->tick());
    }

    TLayout::layout_line(const_cast<Glissando *>(item), ctx);

    if (item->spanner_segments().empty()) {
        LOGD("no segments");
        return;
    }
    Glissando::LayoutData *ldata = item->mutldata();
    ldata->set_pos(0.0, 0.0);

    String instr_id = item->staff()->part()->instrument_id(item->tick());
    bool harp_staff = instr_id == "harp";
    if (!item->is_harp_gliss().has_value()) {
        item->set_is_harp_gliss(harp_staff);
    } else {
        if (harp_staff != item->is_harp_gliss().value()) {
            // Preserve whether this gliss has its default playback style
            bool default_style = false;
            if (item->is_styled(Pid::GLISS_STYLE)) {
                default_style = true;
            }
            item->set_is_harp_gliss(harp_staff);
            if (default_style) {
                item->reset_property(Pid::GLISS_STYLE);
            }

            // Make sure harp glisses can only be diatonic and chromatic
            GlissandoStyle gliss_style = item->glissando_style();
            if (item->is_harp_gliss().value() && (gliss_style != GlissandoStyle::DIATONIC && gliss_style != GlissandoStyle::CHROMATIC)) {
                item->set_glissando_style(GlissandoStyle::DIATONIC);
            }
        }
    }

    layout_note_anchored_line(item, ldata, ctx);

    ChordLayout::add_line_attach_points(item);
}

void TLayout::layout_glissando_segment(GlissandoSegment *item, LayoutContext &) {
    LAYOUT_CALL_ITEM(item);

    GlissandoSegment::LayoutData *ldata = item->mutldata();
    if (item->pos2().x() <= 0) {
        ldata->set_bbox(RectF());
        return;
    }

    if (item->staff()) {
        ldata->set_mag(item->staff()->staff_mag(item->tick()));
    }
    RectF r = RectF(0.0, 0.0, item->pos2().x(), item->pos2().y()).normalized();
    double lw = item->absolute_from_spatium(item->line_width()) * .5;
    item->setbbox(r.adjusted(-lw, -lw, lw, lw));
}

void TLayout::layout_grace_notes_group(GraceNotesGroup *item, LayoutContext &ctx) {
    const StaffType *staff_type = item->parent()->staff_type();
    if (staff_type->is_tab_staff() && !item->empty()) {
        const Staff *staff = item->parent()->staff();
        if (staff->links()) {
            for (EngravingObject *linked_st : *staff->links()) {
                Staff *linked_staff = to_staff(linked_st);
                if (linked_staff->score() == staff->score() && !linked_staff->staff_type()->is_tab_staff() &&
                    linked_staff->idx() < staff->idx()) {
                    // HACK: align grace notes in TAB to their linked notation staff.
                    // CAUTION: Works only if notation staff is above the TAB.
                    // TODO: refactor grace notes to allow doing this properly.
                    align_tab_grace_notes_to_main_stave(item, linked_staff);
                    return;
                }
            }
        }
    }

    LAYOUT_CALL_ITEM(item);
    Shape shape_value;
    for (size_t i = item->size() - 1; i != muse::nidx; --i) {
        Chord *grace = item->at(i);
        Shape grace_shape = grace->shape();
        Shape group_shape = shape_value;
        group_shape.remove_if([grace](ShapeElement &s) {
            if (!s.item() || (s.item()->is_stem() && s.item()->v_staff_idx() != grace->v_staff_idx())) {
                return true;
            }
            return false;
        });
        double offset;
        offset = -std::max(HorizontalSpacing::min_horizontal_distance(grace_shape, group_shape, grace->spatium()), 0.0);
        // Adjust spacing for cross-beam situations
        if (i < item->size() - 1 && grace->stem()) {
            Chord *prev_grace = item->at(i + 1);
            if (prev_grace->up() != grace->up()) {
                double cross_correction = grace->notes().front()->head_width() - grace->stem()->width();
                if (prev_grace->up() && !grace->up()) {
                    offset += cross_correction;
                } else {
                    offset -= cross_correction;
                }
            }
        }
        shape_value.add(grace_shape.translate(PointF(offset, 0.0)));
        double xpos = offset - item->parent()->rxoffset() - item->parent()->ldata()->pos().x();
        grace->set_pos(xpos, 0.0);
    }

    const Segment *appended_seg = item->appended_segment();
    Chord *parent_chord = to_chord(item->parent());
    Shape staff_shape = appended_seg->staff_shape(parent_chord->staff_idx());
    bool is_tab_staff = parent_chord->staff_type() && parent_chord->staff_type()->is_tab_staff();
    if (is_tab_staff) {
        staff_shape.remove_if([](ShapeElement &el) {
            // Ignore stems on tab staves
            return el.item() && el.item()->is_stem();
        });
    }
    double shape_spatium = HorizontalSpacing::shape_spatium(shape_value);
    double x_pos = -HorizontalSpacing::min_horizontal_distance(shape_value, staff_shape, shape_spatium);

    // If the parent chord is cross-staff, also check against shape in the other staff and take the minimum
    if (item->parent()->staff_move() != 0) {
        double x_pos_cross = -HorizontalSpacing::min_horizontal_distance(
            shape_value, appended_seg->staff_shape(item->parent()->v_staff_idx()), shape_spatium);
        x_pos = std::min(x_pos, x_pos_cross);
    }
    // Same if the grace note itself is cross-staff
    Chord *first_gn = item->back();
    if (first_gn->staff_move() != 0) {
        double x_pos_cross =
            -HorizontalSpacing::min_horizontal_distance(shape_value, appended_seg->staff_shape(first_gn->v_staff_idx()), shape_spatium);
        x_pos = std::min(x_pos, x_pos_cross);
    }
    // Safety net in case the shape checks don't succeed
    x_pos = std::min(x_pos, -double(ctx.conf().style_absolute(Sid::GRACE_TO_MAIN_NOTE_DIST) + first_gn->notes().front()->head_width() / 2));

    item->set_pos(x_pos, 0.0);

    if (is_tab_staff) {
        ChordLayout::layout_stem(parent_chord, ctx);
    }
}

void TLayout::layout_grace_notes_group2(const GraceNotesGroup *item, GraceNotesGroup::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    //! NOTE A lot of spam
    // LD_CONDITION(ldata->isSetPos());

    Shape shape(Shape::Type::COMPOSITE);
    for (const Chord *grace : *item) {
        LD_CONDITION(grace->ldata()->is_set_shape());
        LD_CONDITION(grace->ldata()->is_set_pos());

        shape.add(grace->shape(LdAccess::PASS).translate(grace->pos() - item->pos()));
    }
    ldata->set_shape(shape);
}

void TLayout::align_tab_grace_notes_to_main_stave(GraceNotesGroup *grace_notes, const Staff *notation_staff) {
    // HACK: align grace notes in TAB to their linked notation staff.
    // CAUTION: Works only if notation staff is above the TAB.
    // TODO: refactor grace notes to allow doing this properly.

    for (Chord *grace : *grace_notes) {
        Chord *linked_grace = to_chord(grace->find_linked_in_staff(notation_staff));
        if (!linked_grace) {
            continue;
        }
        PointF linked_grace_pos = linked_grace->ldata()->pos();
        grace->set_pos(linked_grace_pos);
    }
}

void TLayout::manage_tempo_change_snapping(GradualTempoChangeSegment *item, LayoutContext &ctx) {
    GradualTempoChangeSegment::LayoutData *ldata = item->mutldata();

    ldata->disconnect_snapped_items();
    GradualTempoChangeSegment *tempo_change_segment_snapped_before = item->find_element_to_snap_before();
    if (tempo_change_segment_snapped_before) {
        ldata->connect_item_snapped_before(tempo_change_segment_snapped_before);
        do_layout_gradual_tempo_change_segment(tempo_change_segment_snapped_before, ctx);
    }

    TempoText *tempo_text_snapped_after = item->find_element_to_snap_after();
    if (tempo_text_snapped_after) {
        ldata->connect_item_snapped_after(tempo_text_snapped_after);
    }
}

void TLayout::do_layout_gradual_tempo_change_segment(GradualTempoChangeSegment *item, LayoutContext &ctx) {
    GradualTempoChangeSegment::LayoutData *ldata = item->mutldata();
    ldata->set_pos_y(0.0);

    auto extend_line_to_snapped_item_after = [item](EngravingItem *item_after) {
        assert(item_after->is_gradual_tempo_change_segment() || item_after->is_tempo_text());
        if (item->tempo_change()->adjust_for_rehearsal_mark(false) || item_after->find_ancestor(ElementType::SYSTEM) != item->system()) {
            return;
        }

        double x_item_pos = item_after->page_x() - item->system()->page_x();
        double item_left_edge = x_item_pos + item_after->ldata()->bbox().left();

        double padding = item->spatium();
        if (item_after->is_tempo_text()) {
            const double font_size_scale_factor = to_tempo_text(item_after)->size() / 10.0;
            padding = item->spatium() * font_size_scale_factor;
        } else if (item_after->is_gradual_tempo_change_segment()) {
            Text *start_text = to_gradual_tempo_change_segment(item_after)->text();
            if (start_text) {
                const double font_size_scale_factor = start_text->size() / 10.0;
                padding = item->spatium() * font_size_scale_factor;
            }
        }

        double max_tempo_line_end = item_left_edge - padding;
        double x_end_diff = max_tempo_line_end - (item->pos().x() + item->pos2().x());
        item->rxpos2() += x_end_diff;
    };

    if (ldata->item_snapped_after() && ldata->item_snapped_after()->is_tempo_text()) {
        TempoText *tempo_text_snapped_after = to_tempo_text(ldata->item_snapped_after());
        extend_line_to_snapped_item_after(tempo_text_snapped_after);
    } else if (ldata->item_snapped_after() && ldata->item_snapped_after()->is_gradual_tempo_change_segment()) {
        GradualTempoChangeSegment *tempo_change_segment_snapped_after = to_gradual_tempo_change_segment(ldata->item_snapped_after());
        extend_line_to_snapped_item_after(tempo_change_segment_snapped_after);
    }

    layout_text_line_base_segment(item, ctx);

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_gradual_tempo_change_segment(GradualTempoChangeSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    manage_tempo_change_snapping(item, ctx);

    do_layout_gradual_tempo_change_segment(item, ctx);
}

void TLayout::layout_gradual_tempo_change(GradualTempoChange *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(item, ctx);
}

void TLayout::layout_guitar_bend(GuitarBend *item, LayoutContext &ctx, System *system) {
    LAYOUT_CALL_ITEM(item);
    item->compute_bend_amount();

    GuitarBendLayout::update_segments_and_layout(item, ctx, system);

    item->update_hold_line();
    if (item->hold_line()) {
        GuitarBendLayout::update_segments_and_layout(item->hold_line(), ctx, system);
    }
}

void TLayout::layout_guitar_bend_segment(GuitarBendSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    bool tab_staff = item->staff_type()->is_tab_staff();
    if (tab_staff) {
        GuitarBendLayout::layout_tab_staff(item, ctx);
    } else {
        GuitarBendLayout::layout_standard_staff(item, ctx);
    }

    SysStaff *staff = item->system()->staff(item->staff_idx());
    Skyline &skyline = staff->skyline();
    SkylineLine &skyline_line = tab_staff ? skyline.north() : (item->guitar_bend()->ldata()->up() ? skyline.north() : skyline.south());
    skyline_line.add(item->shape().translate(item->pos()));

    fill_guitar_bend_segment_shape(item, item->mutldata());
}

void TLayout::fill_guitar_bend_segment_shape(const GuitarBendSegment *item, GuitarBendSegment::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    Shape shape;
    shape.add(ldata->bbox(), item);
    if (!item->bend_text()->empty()) {
        shape.add(item->bend_text()->shape().translate(item->bend_text()->pos()));
    }
    ldata->set_shape(shape);
}

void TLayout::layout_hairpin_segment(HairpinSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    HairpinSegment::LayoutData *ldata = item->mutldata();

    const StaffType *st_type = item->staff_type();

    if (st_type && st_type->is_hidden_element_on_tab(Sid::HAIRPIN_SHOW_TAB_COMMON, Sid::HAIRPIN_SHOW_TAB_SIMPLE)) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);

    const double spatium_value = item->spatium();

    ldata->disconnect_snapped_items();

    manage_hairpin_snapping(item, ctx);

    HairpinType type = item->hairpin()->hairpin_type();
    if (item->hairpin()->is_line_type()) {
        item->set_two_lines(false);
        layout_text_line_base_segment(item, ctx);
        item->set_draw_circled_tip(false);
        item->set_circled_tip_radius(0.0);
    } else {
        item->set_two_lines(true);

        item->hairpin()->set_begin_text_align({AlignH::LEFT, AlignV::VCENTER});
        item->hairpin()->set_end_text_align({AlignH::RIGHT, AlignV::VCENTER});
        item->hairpin()->set_begin_text_position(AlignH::LEFT);
        item->hairpin()->set_end_text_position(AlignH::RIGHT);

        double x1 = 0.0;
        layout_text_line_base_segment(item, ctx);
        if (!item->text()->empty()) {
            x1 = item->text()->width() + spatium_value * .5;
        }

        Transform t;
        double h1 = item->hairpin()->hairpin_height().val() * spatium_value * .5;
        double h2 = item->hairpin()->hairpin_cont_height().val() * spatium_value * .5;

        double x = item->pos2().x();
        if (!item->end_text()->empty()) {
            x -= (item->end_text()->width() + spatium_value * .5); // 0.5 spatium distance
        }
        if (x < spatium_value) { // minimum size of hairpin
            x = spatium_value;
        }
        double y = item->pos2().y();
        double len = sqrt(x * x + y * y);
        t.rotate_radians(asin(y / len));

        item->set_draw_circled_tip(item->hairpin()->hairpin_circled_tip());
        item->set_circled_tip_radius(item->draw_circled_tip() ? 0.6 * spatium_value * .5 : 0.0);

        LineF l1, l2;

        switch (type) {
            case HairpinType::CRESC_HAIRPIN: {
                switch (item->spanner_segment_type()) {
                    case SpannerSegmentType::SINGLE:
                    case SpannerSegmentType::BEGIN: {
                        l1.set_line(x1 + item->circled_tip_radius() * 2.0, 0.0, len, h1);
                        l2.set_line(x1 + item->circled_tip_radius() * 2.0, 0.0, len, -h1);
                        PointF circled_tip;
                        circled_tip.set_x(x1 + item->circled_tip_radius());
                        circled_tip.set_y(0.0);
                        item->set_circled_tip(circled_tip);
                    } break;

                    case SpannerSegmentType::MIDDLE:
                    case SpannerSegmentType::END:
                        item->set_draw_circled_tip(false);
                        l1.set_line(x1, h2, len, h1);
                        l2.set_line(x1, -h2, len, -h1);
                        break;
                }
            } break;
            case HairpinType::DIM_HAIRPIN: {
                switch (item->spanner_segment_type()) {
                    case SpannerSegmentType::SINGLE:
                    case SpannerSegmentType::END: {
                        l1.set_line(x1, h1, len - item->circled_tip_radius() * 2, 0.0);
                        l2.set_line(x1, -h1, len - item->circled_tip_radius() * 2, 0.0);
                        PointF circled_tip;
                        circled_tip.set_x(len - item->circled_tip_radius());
                        circled_tip.set_y(0.0);
                        item->set_circled_tip(circled_tip);
                    } break;
                    case SpannerSegmentType::BEGIN:
                    case SpannerSegmentType::MIDDLE:
                        item->set_draw_circled_tip(false);
                        l1.set_line(x1, h1, len, +h2);
                        l2.set_line(x1, -h1, len, -h2);
                        break;
                }
            } break;
            default: break;
        }

        // Do Coord rotation
        l1 = t.map(l1);
        l2 = t.map(l2);
        if (item->draw_circled_tip()) {
            item->set_circled_tip(t.map(item->circled_tip()));
        }

        ldata->points[0] = l1.p1();
        ldata->points[1] = l1.p2();
        ldata->points[2] = l2.p1();
        ldata->points[3] = l2.p2();
        ldata->npoints = 4;

        ldata->joined_hairpin.clear();
        if (item->spanner_segment_type() != SpannerSegmentType::MIDDLE) {
            if (type == HairpinType::DIM_HAIRPIN && item->spanner_segment_type() != SpannerSegmentType::BEGIN) {
                ldata->joined_hairpin << ldata->points[0] << ldata->points[1] << ldata->points[2]; // [top-left, joint, bottom-left]
            } else if (type == HairpinType::CRESC_HAIRPIN && item->spanner_segment_type() != SpannerSegmentType::END) {
                ldata->joined_hairpin << ldata->points[1] << ldata->points[0] << ldata->points[3]; // [top-right, joint, bottom-right]
            }
        }

        RectF r = RectF(l1.p1(), l1.p2()).normalized().united(RectF(l2.p1(), l2.p2()).normalized());
        if (!item->text()->empty()) {
            r.unite(item->text()->ldata()->bbox());
        }
        if (!item->end_text()->empty()) {
            r.unite(item->end_text()->ldata()->bbox().translated(x + item->end_text()->ldata()->bbox().width(), 0.0));
        }
        double w = item->absolute_from_spatium(ctx.conf().style_s(Sid::HAIRPIN_LINE_WIDTH));
        item->setbbox(r.adjusted(-w * .5, -w * .5, w, w));
    }

    if (!item->ownership_parent()) {
        item->set_pos(PointF());
        item->roffset() = PointF();
        return;
    }

    if (item->autoplace()) {
        Autoplace::autoplace_spanner_segment(item, ldata);
    }
}

void TLayout::manage_hairpin_snapping(HairpinSegment *item, LayoutContext &ctx) {
    HairpinSegment::LayoutData *ldata = item->mutldata();

    EngravingItem *possible_snap_before_element = nullptr;
    EngravingItem *possible_snap_after_element = nullptr;
    if (item->is_single_begin_type()) {
        possible_snap_before_element = item->find_element_to_snap_before();
    }
    if (item->is_single_end_type() && item->hairpin()->ticks().is_not_zero()) {
        possible_snap_after_element = item->find_element_to_snap_after();
    }

    bool do_snap_before = possible_snap_before_element && item->hairpin()->snap_to_item_before();
    if (do_snap_before && possible_snap_before_element->is_hairpin_segment()) {
        do_snap_before = do_snap_before && to_hairpin_segment(possible_snap_before_element)->hairpin()->snap_to_item_after() &&
                         possible_snap_before_element->ldata()->item_snapped_before() != item;
    }

    if (do_snap_before) {
        ldata->connect_item_snapped_before(possible_snap_before_element);
    }

    bool do_snap_after = possible_snap_after_element && item->hairpin()->snap_to_item_after();
    if (do_snap_after && possible_snap_after_element->is_hairpin_segment()) {
        do_snap_after = do_snap_after && to_hairpin_segment(possible_snap_after_element)->hairpin()->snap_to_item_before() &&
                        possible_snap_after_element->ldata()->item_snapped_after() != item;
    }

    if (do_snap_after) {
        ldata->connect_item_snapped_after(possible_snap_after_element);
    }

    // In case of dynamics/expressions before or after, make space for them horizontally
    double mag = item->staff()->staff_mag(item);
    double hairpin_dist_to_dyn_or_expr = ctx.conf().style().style_absolute(Sid::AUTOPLACE_HAIRPIN_DYNAMICS_DISTANCE) * mag;

    bool make_space_before = (do_snap_before && possible_snap_before_element->is_text_base()) ||
                             (possible_snap_before_element && possible_snap_before_element->is_dynamic());

    if (make_space_before && possible_snap_before_element->find_ancestor(ElementType::SYSTEM) == item->system()) {
        double x_item_pos = possible_snap_before_element->page_x() - item->system()->page_x();
        double item_right_edge = x_item_pos + possible_snap_before_element->ldata()->bbox().right();
        double x_min_hairpin_start = item_right_edge + hairpin_dist_to_dyn_or_expr;
        double x_start_diff = ldata->pos().x() - x_min_hairpin_start;
        if (x_start_diff < 0) {
            ldata->set_pos_x(x_min_hairpin_start);
            item->rxpos2() += x_start_diff;
        }
    }

    bool make_space_after = (do_snap_after && possible_snap_after_element->is_text_base()) ||
                            (possible_snap_after_element && possible_snap_after_element->is_dynamic());

    if (make_space_after && possible_snap_after_element->find_ancestor(ElementType::SYSTEM) == item->system()) {
        double x_item_pos = possible_snap_after_element->page_x() - item->system()->page_x();
        double item_left_edge = x_item_pos + possible_snap_after_element->ldata()->bbox().left();
        double x_max_hairpin_end = item_left_edge - hairpin_dist_to_dyn_or_expr;
        double x_end_diff = x_max_hairpin_end - (item->pos().x() + item->pos2().x());
        const double extend_threshold = 3.0 * item->spatium();
        if (x_end_diff < 0) {
            item->rxpos2() += x_end_diff;
        } else if (item->hairpin()->snap_to_item_after() && x_end_diff > extend_threshold) {
            item->rxpos2() += x_end_diff;
        }
    }
}

void TLayout::layout_hairpin(Hairpin *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    item->set_pos(0.0, 0.0);
    layout_text_line_base(item, ctx);
}

void TLayout::layout_hammer_on_pull_off(HammerOnPullOff *item, LayoutContext &ctx) { layout_slur(to_slur(item), ctx); }

void TLayout::layout_hammer_on_pull_off_segment(HammerOnPullOffSegment *item, LayoutContext &ctx) {
    // The layout of the slur has already been done. Here we layout the H/P letters.
    item->update_hopo_text();

    System *system = item->system();
    Fraction system_end_tick = system->end_tick();
    Skyline &sk = system->staff(item->staff_idx())->skyline();

    for (HammerOnPullOffText *hopo_text : item->hopo_text()) {
        bool above = hopo_text->place_above();

        Align align;
        align.vertical = AlignV::BASELINE;
        align.horizontal = AlignH::HCENTER;
        hopo_text->set_position(AlignH::HCENTER);
        hopo_text->set_align(align);
        layout_item(hopo_text, ctx);

        const Chord *start_chord = hopo_text->start_chord();
        const Chord *end_chord = hopo_text->end_chord();
        double start_x = start_chord->system_pos().x() + start_chord->up_note()->head_width();
        double end_x = start_x;
        if (end_chord->tick() < system_end_tick) {
            end_x = end_chord->system_pos().x();
        } else {
            // The last endChord of this segment is in next system. Use end barline instead.
            Segment *end_seg = system->last_measure()->last(SegmentType::BAR_LINE_TYPES);
            end_x = end_seg ? end_seg->system_pos().x() : end_x;
        }
        if (start_chord->stem() && end_chord->stem() && start_chord->up() == above && end_chord->up() == above) {
            // Mid-way between centered on the notes and centered on the stems
            end_x += (above ? 0.5 : -0.5) * end_chord->up_note()->head_width();
        }
        double center_x = 0.5 * (start_x + end_x);

        double vert_padding = 0.5 * item->spatium();
        Shape hopo_text_shape = hopo_text->ldata()->shape().translated(PointF(center_x, 0.0));
        SkylineLine &skl = above ? sk.north() : sk.south();
        SkylineLine skyline =
            skl.get_filtered_copy([](const ShapeElement &el) { return el.item() && el.item()->is_articulation_family(); });

        double y = above ? -skyline.min_distance_to_shape_above(hopo_text_shape) : skyline.min_distance_to_shape_below(hopo_text_shape);
        y += above ? -vert_padding : vert_padding;
        y = above ? std::min(y, -vert_padding) : std::max(y, item->staff()->staff_height(item->tick()) + vert_padding);

        Note *start_note = above ? start_chord->up_note() : start_chord->down_note();
        Note *end_note = above ? end_chord->up_note() : end_chord->down_note();
        double y_note_limit = above ? std::min(start_note->y(), end_note->y()) - 2 * vert_padding :
                                      std::max(start_note->y(), end_note->y()) + 2 * vert_padding;
        y = above ? std::min(y, y_note_limit) : std::max(y, y_note_limit);

        hopo_text->mutldata()->set_pos(center_x, y);

        hopo_text_shape.translate_y(y);
        skl.add(hopo_text_shape);
    }

    Shape hopo_segment_shape = item->mutldata()->shape();
    for (HammerOnPullOffText *hopo_text : item->hopo_text()) {
        hopo_segment_shape.add(hopo_text->ldata()->shape().translated(hopo_text->pos()));
    }

    item->mutldata()->set_shape(hopo_segment_shape);
}

void TLayout::layout_hammer_on_pull_off_text(HammerOnPullOffText *item, LayoutContext &ctx) {
    TextLayout::layout_base_text_base(item, ctx);
}

void TLayout::layout_harp_pedal_diagram(const HarpPedalDiagram *item, HarpPedalDiagram::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    const_cast<HarpPedalDiagram *>(item)->update_diagram_text();

    TextLayout::layout_base_text_base(item, ldata);

    if (item->autoplace()) {
        const Segment *s = to_segment(item->ownership_parent());
        const Measure *m = s->measure();

        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(m->ldata()->is_set_pos());
        LD_CONDITION(s->ldata()->is_set_pos());
    }

    Autoplace::autoplace_segment_element(item, ldata);
}

void TLayout::layout_harmonic_mark_segment(HarmonicMarkSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    HarmonicMarkSegment::LayoutData *ldata = item->mutldata();
    const StaffType *st_type = item->staff_type();
    if (st_type && (!st_type->is_tab_staff() ||
                    st_type->is_hidden_element_on_tab(Sid::HARMONIC_MARK_SHOW_TAB_COMMON, Sid::HARMONIC_MARK_SHOW_TAB_SIMPLE))) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);

    layout_text_line_base_segment(item, ctx);

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_harmony(Harmony *item, Harmony::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    HarmonyLayout::layout_harmony(item, ldata, ctx);
}

void TLayout::layout_hook(const Hook *item, Hook::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    if (ldata->is_valid()) {
        return;
    }

    ldata->set_bbox(item->sym_bbox(item->sym()));
}

void TLayout::layout_image(const Image *item, Image::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    IF_ASSERT_FAILED(item->layout_parent()) { return; }

    ldata->set_pos(0.0, 0.0);
    const_cast<Image *>(item)->init();

    SizeF image_size = item->size();

    // if autoscale && inside a box, scale to box relevant size
    if (item->auto_scale() && ((item->layout_parent()->is_h_box() || item->layout_parent()->is_v_box()))) {
        const EngravingItem::LayoutData *parent_ld = item->layout_parent()->ldata();

        LD_CONDITION(parent_ld->is_set_bbox());

        if (item->lock_aspect_ratio()) {
            double f = item->size_is_spatium() ? item->spatium() : ::mu::engraving::dpmm;
            SizeF size(item->image_size());
            double ratio = size.width() / size.height();
            double w = parent_ld->bbox().width();
            double h = parent_ld->bbox().height();
            if ((w / h) < ratio) {
                image_size.set_width(w / f);
                image_size.set_height((w / ratio) / f);
            } else {
                image_size.set_height(h / f);
                image_size.set_width(h * ratio / f);
            }
        } else {
            image_size = item->pixel2size(parent_ld->bbox().size());
        }
    }

    const_cast<Image *>(item)->set_size(image_size);

    // in any case, adjust position relative to parent
    ldata->set_bbox(RectF(PointF(), item->size2pixel(image_size)));
}

void TLayout::layout_instrument_change(const InstrumentChange *item, InstrumentChange::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    TextLayout::layout_base_text_base(item, ldata);

    if (item->autoplace()) {
        const Segment *s = to_segment(item->ownership_parent());
        const Measure *m = s->measure();
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(m->ldata()->is_set_pos());
        LD_CONDITION(s->ldata()->is_set_pos());
    }

    Autoplace::autoplace_segment_element(item, ldata);
}

void TLayout::layout_instrument_name(const InstrumentName *item, InstrumentName::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;
    //! NOTE There are problems, an investigation is required
    //    if (ldata->isValid()) {
    //        return;
    //    }

    TextLayout::layout_base_text_base(item, ldata);
}

void TLayout::layout_jump(const Jump *item, Jump::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_CONDITION(item->layout_parent()->ldata()->is_set_bbox());

    TextLayout::layout_base_text_base(item, ldata);

    AlignH position = item->position();

    // ignore courtesy keysig, timesig, but fall back if needed
    Measure *measure = to_measure(item->parent());
    const BarLine *bl = measure->end_bar_line();
    double x_adj = bl ? bl->segment()->x() + bl->ldata()->bbox().width() : measure->width();

    bool avoid_barline = item->staff_idx() != 0 && position != AlignH::HCENTER;
    if (avoid_barline) {
        bool start_repeat = measure->repeat_start();
        bool end_repeat = measure->repeat_end();
        staff_idx_t bl_above_idx = item->staff_idx() - 1;

        const double font_size_scale_factor = item->size() / 10.0;
        double padding = (start_repeat || end_repeat) ? 0.0 : 0.5 * item->spatium() * font_size_scale_factor;

        if (position == AlignH::LEFT) {
            const BarLine *bl_above = start_repeat || !measure->prev_measure() ? measure->start_bar_line(bl_above_idx) :
                                                                                 measure->prev_measure()->end_bar_line(bl_above_idx);
            double bl_width = (start_repeat && bl_above) ? bl_above->width() : 0.0;
            x_adj += padding + bl_width;
        } else if (position == AlignH::RIGHT) {
            const BarLine *bl_above = measure->end_bar_line(bl_above_idx);
            double bl_width = bl_above ? bl_above->width() : 0.0;
            x_adj -= bl_width + padding;
        }
    }
    ldata->move_x(x_adj);

    if (item->autoplace()) {
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(ldata->is_set_bbox());
        LD_CONDITION(measure->ldata()->is_set_pos());
    }

    Autoplace::autoplace_measure_element(item, ldata);
}

static void key_sig_add_layout(const KeySig *item, const LayoutConfiguration &conf, SymId sym, int line, KeySig::LayoutData *ldata) {
    double spatium_value = item->spatium();
    double step = spatium_value * (item->staff() ? item->staff()->staff_type_for_element(item)->line_distance().val() * 0.5 : 0.5);
    KeySym ks;
    ks.sym = sym;
    Spatium x = 0.0_sp;
    if (!ldata->key_symbols.empty()) {
        const KeySym &previous = ldata->key_symbols.back();
        Spatium accidental_gap = conf.style_s(Sid::KEYSIG_ACCIDENTAL_DISTANCE);
        if (previous.sym != sym) {
            accidental_gap *= 2;
        } else if (previous.sym == SymId::ACCIDENTAL_NATURAL && sym == SymId::ACCIDENTAL_NATURAL) {
            accidental_gap = conf.style_s(Sid::KEYSIG_NATURAL_DISTANCE);
        }
        Spatium previous_width = Spatium::from_absolute(item->sym_width(previous.sym), spatium_value);
        x = previous.x_pos + previous_width + accidental_gap;
        bool is_ascending = line < previous.line;
        SmuflAnchorId current_cutout = is_ascending ? SmuflAnchorId::CUT_OUT_SW : SmuflAnchorId::CUT_OUT_NW;
        SmuflAnchorId previous_cutout = is_ascending ? SmuflAnchorId::CUT_OUT_NE : SmuflAnchorId::CUT_OUT_SE;
        PointF cutout = item->sym_smufl_anchor(sym, current_cutout);
        double current_cutout_y = line * step + cutout.y();
        double previous_cutout_y = previous.line * step + item->sym_smufl_anchor(previous.sym, previous_cutout).y();
        if ((is_ascending && current_cutout_y < previous_cutout_y) || (!is_ascending && current_cutout_y > previous_cutout_y)) {
            x -= Spatium::from_absolute(cutout.x(), spatium_value);
        }
    }
    ks.x_pos = x;
    ks.line = line;
    ldata->key_symbols.push_back(ks);
}

void TLayout::layout_key_sig(const KeySig *item, KeySig::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    //! NOTE There are problems, an investigation is required
    //    if (ldata->isValid()) {
    //        return;
    //    }

    ldata->set_bbox(RectF());
    ldata->key_symbols.clear();

    const Staff *staff = item->staff();
    const StaffType *st_visibility = staff ? staff->staff_type(item->tick()) : nullptr;
    if (st_visibility && !st_visibility->gen_keysig()) {
        return;
    }

    const StaffType *st = item->staff_type();
    const Segment *s = item->segment();
    track_idx_t track = item->track();
    double spatium = item->spatium();
    double step = spatium * (st ? st->line_distance().val() * 0.5 : 0.5);

    // determine current clef for this staff
    ClefType clef = ClefType::G;
    if (item->staff()) {
        // Look for a clef before the key signature at the same tick
        Clef *c = nullptr;
        if (s) {
            for (Segment *seg = s->prev1(); !c && seg && seg->tick() == item->tick(); seg = seg->prev1()) {
                const bool is_clef_seg = seg->is_clef_type() || seg->is_header_clef_type() ||
                                         (seg->is_clef_repeat_announce_type() && s->is_key_sig_repeat_announce_type());
                if (seg->enabled() && is_clef_seg) {
                    c = to_clef(seg->element(track));
                }
            }
        }
        // If no clef found, get the clef type from the clefs list (using the previous tick)
        clef = c ? c->clef_type() : item->staff()->clef(item->tick() - Fraction::eps());
    }

    int t1 = int(item->key());
    const signed char *lines = ClefInfo::lines(clef);

    if (item->is_custom() && !item->is_atonal()) {
        Spatium accidental_gap = conf.style_s(Sid::KEYSIG_ACCIDENTAL_DISTANCE);
        // add standard key accidentals first, if necessary
        for (int i = 1; i <= std::abs(t1) && std::abs(t1) <= 7; ++i) {
            bool drop = false;
            for (const CustDef &cd : item->custom_key_defs()) {
                int degree = item->deg_in_key(cd.degree);
                // if custom keysig accidental takes place, don't create tonal accidental
                if ((degree * 2 + 2) % 7 == (t1 < 0 ? 8 - i : i) % 7) {
                    drop = true;
                    break;
                }
            }
            if (!drop) {
                KeySym ks;
                int line_index_offset = t1 > 0 ? -1 : 6;
                ks.sym = t1 > 0 ? SymId::ACCIDENTAL_SHARP : SymId::ACCIDENTAL_FLAT;
                ks.line = lines[line_index_offset + i];
                if (!ldata->key_symbols.empty()) {
                    const KeySym &previous = ldata->key_symbols.back();
                    Spatium previous_width = Spatium::from_absolute(item->sym_width(previous.sym), spatium);
                    ks.x_pos = previous.x_pos + previous_width + accidental_gap;
                } else {
                    ks.x_pos = 0_sp;
                }
                // TODO octave metters?
                ldata->key_symbols.push_back(ks);
            }
        }
        for (const CustDef &cd : item->custom_key_defs()) {
            SymId sym = item->sym_in_key(cd.sym, cd.degree);
            int degree = item->deg_in_key(cd.degree);
            bool flat = std::string(SymNames::name_for_sym_id(sym).ascii()).find("Flat") != std::string::npos;
            int acc_idx = (degree * 2 + 1) % 7; // C D E F ... index to F C G D index
            acc_idx = flat ? 13 - acc_idx : acc_idx;
            int line = lines[acc_idx] + cd.oct_alt * 7;
            Spatium xpos = cd.x_alt;
            if (!ldata->key_symbols.empty()) {
                const KeySym &previous = ldata->key_symbols.back();
                Spatium previous_width = Spatium::from_absolute(item->sym_width(previous.sym), spatium);
                xpos += previous.x_pos + previous_width + accidental_gap;
            }
            // if translated symbol if out of range, add key accidental followed by untranslated symbol
            if (sym == SymId::NO_SYM) {
                KeySym ks;
                ks.line = line;
                ks.x_pos = xpos;
                // for quadruple sharp use two double sharps
                if (cd.sym == SymId::ACCIDENTAL_TRIPLE_SHARP) {
                    ks.sym = SymId::ACCIDENTAL_DOUBLE_SHARP;
                    sym = SymId::ACCIDENTAL_DOUBLE_SHARP;
                } else {
                    ks.sym = t1 > 0 ? SymId::ACCIDENTAL_SHARP : SymId::ACCIDENTAL_FLAT;
                    sym = cd.sym;
                }
                ldata->key_symbols.push_back(ks);
                xpos += Spatium(t1 < 0 ? 0.7 : 1); // flats closer
            }
            // create symbol; natural only if is user defined
            if (sym != SymId::ACCIDENTAL_NATURAL || sym == cd.sym) {
                KeySym ks;
                ks.sym = sym;
                ks.line = line;
                ks.x_pos = xpos;
                ldata->key_symbols.push_back(ks);
            }
        }
    } else {
        auto layout_sharps_flats = [&]() {
            if (std::abs(t1) > 7) {
                LOGD("illegal t1 key %d", t1);
                return;
            }
            SymId symbol = t1 > 0 ? SymId::ACCIDENTAL_SHARP : SymId::ACCIDENTAL_FLAT;
            int line_index_offset = t1 > 0 ? 0 : 7;
            for (int i = 0; i < std::abs(t1); ++i) {
                key_sig_add_layout(item, conf, symbol, lines[line_index_offset + i], ldata);
            }
        };

        // Naturals are shown if:
        // Key signature is courtesy, mid-measure or prev. measure has no section break and no courtesy keysig.
        // AND we're not force hiding naturals (continuous mode)
        // AND key sig is CMaj/Amin OR style says they are on
        const Measure *pm = item->measure() ? item->measure()->prev_measure_mm() : nullptr;
        const bool is_courtesy = s && (s->is_type(SegmentType::COURTESY_KEY_SIG_TYPES) || !s->rtick().is_zero());
        const bool prev_trailer_courtesy = pm && !pm->section_break() && (!pm->trailer() || !pm->has_courtesy_key_sig());
        if (!item->hide_naturals() && track != muse::nidx &&
            (conf.style_i(Sid::KEY_SIG_NATURALS) != int(KeySigNatural::NONE) || (t1 == 0)) && (is_courtesy || prev_trailer_courtesy)) {
            KeySigEvent prev_ks_event = item->staff() ? item->staff()->key_sig_event(item->tick() - Fraction::eps()) : KeySigEvent();
            int t2 = int(prev_ks_event.key());

            // Handle naturals in continuation courtesy
            if (pm && s && s->is_type(SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE)) {
                Segment *prev_courtesy_seg = pm->find_segment_r(SegmentType::KEY_SIG_REPEAT_ANNOUNCE, pm->ticks());
                if (prev_courtesy_seg && prev_courtesy_seg->element(track)) {
                    t2 = int(to_key_sig(prev_courtesy_seg->element(track))->key());
                }
            }
            // Don't show naturals when going from sharps to flats, if style says so
            const bool same_accidentals = t1 * t2 >= 0;
            if (t2 != 0 && (same_accidentals || conf.style_b(Sid::KEY_SIG_SHOW_NATURALS_CHANGING_SHARPS_FLATS))) {
                auto key2accidentals = [](int key) -> int {
                    switch (std::abs(key)) {
                        case 7: return 0x7f;
                        case 6: return 0x3f;
                        case 5: return 0x1f;
                        case 4: return 0xf;
                        case 3: return 0x7;
                        case 2: return 0x3;
                        case 1: return 0x1;
                        case 0: return 0;
                        default: LOGD("illegal key %d", key); return 0;
                    }
                };

                int naturals = key2accidentals(t2);
                // remove redundant naturals
                if (!((t1 > 0) ^ (t2 > 0))) {
                    naturals &= ~key2accidentals(t1);
                }
                auto layout_naturals = [&]() {
                    int line_index_offset = t2 > 0 ? 0 : 7;
                    for (int i = 0; i < 7; ++i) {
                        if (naturals & (1 << i)) {
                            key_sig_add_layout(item, conf, SymId::ACCIDENTAL_NATURAL, lines[i + line_index_offset], ldata);
                        }
                    }
                };
                // Naturals should go BEFORE accidentals if style says so
                // or going from sharps to flats or vice versa (i.e. t1 & t2 have opposite signs)
                if (conf.style_i(Sid::KEY_SIG_NATURALS) == int(KeySigNatural::BEFORE) || !same_accidentals) {
                    layout_naturals();
                    layout_sharps_flats();
                } else {
                    layout_sharps_flats();
                    layout_naturals();
                }
            } else if (prev_ks_event.custom()) {
                Fraction prev_key_tick =
                    Fraction::from_ticks(item->staff()->key_list()->current_key_tick((item->tick() - Fraction::eps()).ticks()));
                Segment *prev_ks_seg = item->score()->tick2segment(prev_key_tick, true, SegmentType::KEY_SIG);
                if (prev_ks_seg) {
                    KeySig *prev_custom_key_sig = to_key_sig(prev_ks_seg->element(item->track()));
                    if (prev_custom_key_sig) {
                        for (KeySym key_sym : prev_custom_key_sig->ldata()->key_symbols) {
                            if (key_sym.sym != SymId::ACCIDENTAL_NATURAL) {
                                key_sig_add_layout(item, conf, SymId::ACCIDENTAL_NATURAL, key_sym.line, ldata);
                            }
                        }
                    }
                }
            }
        }

        // No naturals were added, so just create a regular keysig
        if (ldata->key_symbols.empty()) {
            layout_sharps_flats();
        }
    }

    ldata->set_pos_y((st ? step * st->step_offset() : 0.0) + item->staff_offset_y());

    Shape key_sig_shape;
    for (const KeySym &ks : ldata->key_symbols) {
        double x = ks.x_pos.to_absolute(spatium);
        double y = ks.line * step;
        Shape ks_shape = item->sym_shape_with_cutouts(ks.sym);
        for (const ShapeElement &ks_elem : ks_shape.elements()) {
            key_sig_shape.add(ks_elem.translated(x, y), item);
        }
    }
    ldata->set_shape(key_sig_shape);
}

void TLayout::layout_laissez_vib([[maybe_unused]] LaissezVib *item) {}

void TLayout::layout_layout_break(const LayoutBreak *item, LayoutBreak::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    if (ldata->is_valid() || !item->configuration()->can_layout_icons()) {
        return;
    }

    FontMetrics metrics(item->font());
    RectF bbox = metrics.bounding_rect(item->icon_code());
    ldata->set_shape(Shape(bbox, item));
}

void TLayout::layout_indicator_icon(const IndicatorIcon *item, IndicatorIcon::LayoutData *ldata) {
    if (!item->configuration()->can_layout_icons()) {
        return;
    }

    const MeasureBase *end_mb = nullptr;

    Shape shape;

    const FontMetrics metrics(item->font());
    const RectF icon_box = metrics.bounding_rect(item->icon_code());
    shape.add(icon_box, item);

    if (item->is_system_lock_indicator()) {
        const SystemLockIndicator *sli = to_system_lock_indicator(item);
        end_mb = sli->system_lock()->end_mb();
        if (item->selected()) {
            // Draw the range rect...
            const RangeLock *lock = sli->system_lock();

            double x_start = lock->start_mb()->x();
            double x_end = lock->end_mb()->x() + lock->end_mb()->width();

            double width = x_end - x_start;
            double height = icon_box.height() + sli->spatium();

            double y = icon_box.top() - 0.5 * sli->spatium();

            ldata->range_rect = RectF(x_start, y, width, height).translated(-sli->x(), 0.0);
            shape.add(ldata->range_rect);
        }
    } else {
        end_mb = item->system()->last();
    }

    ldata->set_shape(shape);

    const double spatium = item->spatium();

    double x = end_mb->x() + end_mb->width();
    x -= icon_box.right() + 0.5 * spatium;

    double x_offset = end_mb->x() + end_mb->width();
    for (EngravingItem *el : end_mb->el()) {
        if (el->is_layout_break()) {
            x_offset = std::min(x_offset, end_mb->x() + el->x() + el->ldata()->bbox().left() - spatium);
        }
    }

    for (const SystemLockIndicator *sli : item->system()->system_lock_indicators()) {
        if (sli != item && sli->system_lock()->end_mb() == end_mb) {
            const double sli_width = FontMetrics(sli->font()).bounding_rect(sli->icon_code()).width();
            // TODO: Rough spacing here
            x_offset -= (sli_width * 2) - spatium;
        }
    }

    x = std::min(x, x_offset - icon_box.right());

    ldata->set_pos(PointF(x, -2.5 * spatium));

    // Ensure it goes behind notation and LayoutBreak
    const_cast<IndicatorIcon *>(item)->set_z(-100);
}

static void layout_ledger_line(const LedgerLine *item, const LayoutContext &ctx, LedgerLine::LayoutData *ldata) {
    double chord_mag = item->chord()->mag();
    ldata->set_mag(chord_mag);
    ldata->line_width = ctx.conf().style_absolute(Sid::LEDGER_LINE_WIDTH) * chord_mag;
    if (item->staff()) {
        const_cast<LedgerLine *>(item)->set_color(item->staff()->staff_type(item->tick())->color());
    }
    double w2 = ldata->line_width * .5;

    if (item->vertical()) {
        ldata->set_bbox(-w2, 0, w2, item->len());
    } else {
        ldata->set_bbox(0, -w2, item->len(), 2 * w2);
    }
}

void TLayout::layout_ledger_line(LedgerLine *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    ::layout_ledger_line(item, ctx, item->mutldata());
}

void TLayout::layout_let_ring(LetRing *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(item, ctx);
}

void TLayout::layout_let_ring_segment(LetRingSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    HarmonicMarkSegment::LayoutData *ldata = item->mutldata();

    const StaffType *st_type = item->staff_type();

    if (st_type && st_type->is_hidden_element_on_tab(Sid::LET_RING_SHOW_TAB_COMMON, Sid::LET_RING_SHOW_TAB_SIMPLE)) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);

    layout_text_line_base_segment(item, ctx);

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_line_segment(LineSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_item(item, ctx);
}

void TLayout::layout_lyrics(Lyrics *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    LyricsLayout::layout(item, ctx);
}

void TLayout::layout_lyrics_line(LyricsLine *item) {
    LAYOUT_CALL_ITEM(item);
    LyricsLayout::layout(item);
}

void TLayout::layout_lyrics_line_segment(LyricsLineSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    LyricsLayout::layout(item, ctx);
}

void TLayout::layout_marker(Marker *item, Marker::LayoutData *ldata, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    MarkerLayout::layout_marker(item, ldata, ctx);
}

void TLayout::layout_measure_base(MeasureBase *item, LayoutContext &ctx) { layout_item(item, ctx); }

void TLayout::layout_base_measure_base(const MeasureBase *item, MeasureBase::LayoutData *ldata, const LayoutContext &ctx) {
    LD_CONDITION(ldata->is_set_bbox());

    double x = ldata->bbox().width();
    for (EngravingItem *e : item->el()) {
        if (e->is_layout_break()) {
            TLayout::layout_item(e, const_cast<LayoutContext &>(ctx));
            EngravingItem::LayoutData *eldata = e->mutldata();
            double spatium = item->spatium();
            double y = 0.0;
            if (to_layout_break(e)->is_no_break()) {
                x -= eldata->bbox().right() * .5;
            } else {
                x -= eldata->bbox().right() + 0.5 * spatium;
            }
            y = -2.5 * spatium;
            eldata->set_pos(x, y);
            x += eldata->bbox().left() - 0.5 * spatium;
        } else if (e->is_marker() || e->is_jump()) {
        } else {
            layout_item(e, const_cast<LayoutContext &>(ctx));
        }
    }
}

void TLayout::layout_measure_number(MeasureNumber *item, MeasureNumber::LayoutData *ldata, const LayoutContext &ctx) {
    MeasureNumberLayout::layout_measure_number(item, ldata, ctx);
}

void TLayout::layout_measure_repeat(const MeasureRepeat *item, MeasureRepeat::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    //! NOTE The types are listed here explicitly to show what types there are (see Rest::add method)
    //! and accordingly show what depends on.
    for (EngravingItem *e : item->el()) {
        switch (e->type()) {
            case ElementType::DEAD_SLAPPED: {
                DeadSlapped *ds = item_cast<DeadSlapped *>(e);
                LD_INDEPENDENT;
                layout_dead_slapped(ds, ds->mutldata());
            } break;
            case ElementType::SYMBOL: {
                Symbol *s = item_cast<Symbol *>(e);
                // LD_X not clear yet
                layout_symbol(s, s->mutldata(), ctx);
            } break;
            case ElementType::IMAGE: {
                Image *im = item_cast<Image *>(e);
                LD_INDEPENDENT;
                layout_image(im, im->mutldata());
            } break;
            default: UNREACHABLE;
        }
    }

    switch (item->num_measures()) {
        case 1: {
            ldata->set_sym_id(SymId::REPEAT1_BAR);
            if (ctx.conf().style_b(Sid::MR_NUMBER_SERIES) && item->track() != muse::nidx) {
                int place_in_series = 2; // "1" would be the measure actually being repeated
                staff_idx_t staff_idx = item->staff_idx();
                const Measure *m = item->measure();
                while (m && m->is_one_measure_repeat(staff_idx) && m->prev_is_one_measure_repeat(staff_idx)) {
                    place_in_series++;
                    m = m->prev_measure();
                }
                if (place_in_series % ctx.conf().style_i(Sid::MR_NUMBER_EVERY_X_MEASURES) == 0) {
                    if (ctx.conf().style_b(Sid::MR_NUMBER_SERIES_WITH_PARENTHESES)) {
                        ldata->set_number_sym(String(u"(%1)").arg(place_in_series));
                    } else {
                        ldata->set_number_sym(place_in_series);
                    }
                } else {
                    ldata->clear_number_sym();
                }
            } else if (ctx.conf().style_b(Sid::ONE_MEASURE_REPEAT_SHOW1)) {
                ldata->set_number_sym(1);
            } else {
                ldata->clear_number_sym();
            }
            break;
        }
        case 2:
            ldata->set_sym_id(SymId::REPEAT2_BARS);
            ldata->set_number_sym(item->num_measures());
            break;
        case 4:
            ldata->set_sym_id(SymId::REPEAT4_BARS);
            ldata->set_number_sym(item->num_measures());
            break;
        default:
            ldata->set_sym_id(SymId::NO_SYM); // should never happen
            ldata->clear_number_sym();
            break;
    }

    RectF bbox = item->sym_bbox(ldata->sym_id);

    if (item->track() != muse::nidx) { // if this is in score rather than a palette cell
        // For unknown reasons, the symbol has some offset in almost all SMuFL fonts
        // We compensate for it, to make sure the symbol is visually centered around the staff line
        double offset = (-bbox.top() - bbox.bottom()) / 2.0;

        const StaffType *staff_type = item->staff_type();

        // Only need to set y position here; x position is handled in MeasureLayout::layoutMeasureElements()
        ldata->set_pos(0, std::floor(staff_type->middle_line() / 2.0) * staff_type->line_distance().val() * item->spatium() + offset);
    }

    ChordLayout::fill_shape(item, ldata, ctx.conf());
}

void TLayout::layout_measure_repeat_extender(const MeasureRepeat *item, MeasureRepeat::LayoutData *ldata, const LayoutContext &ctx) {
    ldata->extender_line_left = LineF();
    ldata->extender_line_right = LineF();

    Measure *parent_measure = item->measure();
    if (!parent_measure) {
        return;
    }

    Measure *start_measure = parent_measure->prev_measure();
    if (!start_measure || start_measure->system() != parent_measure->system()) {
        return;
    }

    Measure *end_measure = parent_measure->next_measure();
    if (end_measure) {
        end_measure = end_measure->next_measure();
    }
    if (!end_measure || end_measure->system() != parent_measure->system()) {
        return;
    }

    // Reuse the function that calculates start and end pos for mmRests,
    // except start and end point are on different measures
    staff_idx_t staff_idx = item->staff_idx();
    double x_start = MeasureLayout::get_measure_start_end_pos(start_measure, start_measure->first(SegmentType::CHORD_REST), staff_idx,
                                                              /*needHeaderException*/ start_measure->header(), /*modernMMrest*/ true, ctx)
                         .x1;
    double x_end = MeasureLayout::get_measure_start_end_pos(end_measure, end_measure->first(SegmentType::CHORD_REST), staff_idx,
                                                            /*needHeaderException*/ false, /*modernMRest*/ true, ctx)
                       .x2;

    double margin = ctx.conf().style_absolute(Sid::MULTI_MEASURE_REST_MARGIN);
    x_start += margin;
    x_end -= margin;

    double x_item_in_sys_coords = item->x() + item->segment()->x() + parent_measure->x();

    PointF start_point(x_start + start_measure->x() - x_item_in_sys_coords, 0.0);
    PointF mid_point_l(-0.5 * item->spatium(), 0.0);
    ldata->extender_line_left = LineF(start_point, mid_point_l);

    PointF mid_point_r(item->sym_width(ldata->sym_id) + 0.5 * item->spatium(), 0.0);
    PointF end_point(x_end + end_measure->x() - x_item_in_sys_coords, 0.0);
    ldata->extender_line_right = LineF(mid_point_r, end_point);
}

void TLayout::layout_mm_rest(const MMRest *item, MMRest::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    //! NOTE The types are listed here explicitly to show what types there are (see Rest::add method)
    //! and accordingly show what depends on.
    for (EngravingItem *e : item->el()) {
        switch (e->type()) {
            case ElementType::DEAD_SLAPPED: {
                DeadSlapped *ds = item_cast<DeadSlapped *>(e);
                LD_INDEPENDENT;
                layout_dead_slapped(ds, ds->mutldata());
            } break;
            case ElementType::SYMBOL: {
                Symbol *s = item_cast<Symbol *>(e);
                // LD_X not clear yet
                layout_symbol(s, s->mutldata(), ctx);
            } break;
            case ElementType::IMAGE: {
                Image *im = item_cast<Image *>(e);
                LD_INDEPENDENT;
                layout_image(im, im->mutldata());
            } break;
            default: UNREACHABLE;
        }
    }

    LD_CONDITION(ldata->rest_width.has_value());

    //! NOTE This is not look like layout data, perhaps this is should be set not here
    ldata->number = item->measure()->mm_rest_count();
    ldata->set_number_sym(ldata->number);
    ldata->y_number_pos = ctx.conf().style_absolute(Sid::MM_REST_NUMBER_POS);

    const Staff *staff = item->staff();
    if (staff->lines(item->tick()) == 1) {
        ldata->y_number_pos = std::min(ldata->y_number_pos, -item->spatium());
    }

    if (item->is_old_style()) {
        SymIdList rest_syms;
        double syms_width = 0.0;

        int remaining = ldata->number;
        double spacing = ctx.conf().style_absolute(Sid::MM_REST_OLD_STYLE_SPACING);
        SymId sym;

        while (remaining > 0) {
            if (remaining >= 4) {
                sym = SymId::REST_LONGA;
                remaining -= 4;
            } else if (remaining >= 2) {
                sym = SymId::REST_DOUBLE_WHOLE;
                remaining -= 2;
            } else {
                sym = SymId::REST_WHOLE;
                remaining -= 1;
            }

            rest_syms.push_back(sym);
            syms_width += item->sym_bbox(sym).width();

            if (remaining > 0) { // do not add spacing after last symbol
                syms_width += spacing;
            }
        }

        ldata->rest_syms = rest_syms;
        ldata->syms_width = syms_width;
    }

    // Only need to set y position here; x position is handled in MeasureLayout::layoutMeasureElements()
    const StaffType *staff_type = item->staff_type();
    double midline = staff_type->middle_line() / 2.0;
    if (item->is_old_style()) {
        midline = std::max(1.0, midline);
    }
    ldata->set_pos(0, midline * staff_type->line_distance().val() * item->spatium());

    ChordLayout::fill_shape(item, ldata, ctx.conf());
}

void TLayout::layout_mm_rest_range(MMRestRange *item, MMRestRange::LayoutData *ldata, const LayoutContext &ctx) {
    MeasureNumberLayout::layout_mm_rest_range(item, ldata, ctx);
}

void TLayout::layout_note(const Note *item, Note::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    if (!ldata->is_set_pos()) {
        ldata->set_pos(PointF());
    }

    bool use_tablature = item->staff() && item->staff()->is_tab_staff(item->chord()->tick());
    ldata->use_tablature.set_value(use_tablature);

    RectF note_b_box;
    if (use_tablature) {
        if (item->display_fret() == Note::DisplayFretOption::HIDE) {
            return;
        }

        const Staff *st = item->staff();
        const StaffType *tab = st->staff_type_for_element(item);
        // not complete but we need systems to be laid out to add parenthesis
        if (item->fixed()) {
            const_cast<Note *>(item)->set_fret_string(u"/");
        } else {
            const_cast<Note *>(item)->set_fret_string(tab->fret_string(std::fabs(item->fret()), item->string(), item->dead_note()));

            if (item->negative_fret_used() && !item->dead_note()) {
                const_cast<Note *>(item)->set_fret_string(u"-" + item->fret_string());
            }

            if (item->display_fret() == Note::DisplayFretOption::ARTIFICIAL_HARMONIC) {
                const_cast<Note *>(item)->set_fret_string(
                    String(u"%1 <%2>").arg(item->fret_string(), String::number(item->harmonic_fret())));
            } else if (item->display_fret() == Note::DisplayFretOption::NATURAL_HARMONIC) {
                const_cast<Note *>(item)->set_fret_string(String(u"<%1>").arg(String::number(item->harmonic_fret())));
            }
        }

        double w = item->tab_head_width(tab);
        double mags = item->mag_s();
        double y = item->dead_note() ? tab->dead_fret_box_y() : tab->fret_box_y();
        double height = item->dead_note() ? tab->dead_fret_box_h() : tab->fret_box_h();

        note_b_box = RectF(0, y * mags, w, height * mags);
    } else {
        if (item->dead_note()) {
            const_cast<Note *>(item)->set_head_group(NoteHeadGroup::HEAD_CROSS);
        } else if (item->harmonic()) {
            const_cast<Note *>(item)->set_head_group(NoteHeadGroup::HEAD_DIAMOND);
        }

        SymId nh = item->note_head();
        ldata->cached_notehead_sym.set_value(nh);

        if (item->is_note_name()) {
            ldata->cached_sym_null.set_value(SymId::NOTE_EMPTY_BLACK);
            NoteHeadType ht = item->head_type() == NoteHeadType::HEAD_AUTO ? item->chord()->duration_type().head_type() : item->head_type();
            if (ht == NoteHeadType::HEAD_WHOLE) {
                ldata->cached_sym_null.set_value(SymId::NOTE_EMPTY_WHOLE);
            } else if (ht == NoteHeadType::HEAD_HALF) {
                ldata->cached_sym_null.set_value(SymId::NOTE_EMPTY_HALF);
            }
        } else {
            ldata->cached_sym_null.set_value(SymId::NO_SYM);
        }
        note_b_box = item->sym_bbox(nh);
    }

    ldata->set_bbox(note_b_box);

    fill_note_shape(item, ldata);
}

void TLayout::fill_note_shape(const Note *item, Note::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    RectF note_b_box = ldata->bbox();

    Shape shape(Shape::Type::COMPOSITE);
    shape.add(note_b_box, item);

    for (const NoteDot *dot : item->dots()) {
        if (!dot->add_to_skyline()) {
            continue;
        }
        shape.add(item->sym_bbox(SymId::AUGMENTATION_DOT).translated(dot->pos()), dot);
    }

    Accidental *acc = item->accidental();
    if (acc && acc->add_to_skyline()) {
        shape.add(acc->ldata()->shape().translated(acc->pos()));
    }
    const LaissezVib *lv = item->laissez_vib();
    const LaissezVibSegment *lv_seg = lv && !lv->segments_empty() ? lv->front_segment() : nullptr;
    if (lv_seg && lv_seg->add_to_skyline()) {
        const PointF corner_closest_to_note = lv->up() ? lv_seg->shape().bbox().bottom_left() : lv_seg->shape().bbox().top_left();
        const PointF pos = lv_seg->ldata()->pos_relative_to_note.value() - corner_closest_to_note;
        shape.add(lv_seg->shape().translate(pos));
    }
    for (auto e : item->el()) {
        if (e->add_to_skyline()) {
            if (e->is_fingering() && to_fingering(e)->layout_type() != ElementType::NOTE) {
                continue;
            }
            shape.add(e->ldata()->bbox().translated(e->pos()), e);
        }
    }

    Part *part = item->part();
    if (part && part->instrument()->has_strings()) {
        GuitarBend *bend = item->bend_for();
        if (bend && bend->add_to_skyline() && bend->bend_type() == GuitarBendType::SLIGHT_BEND && !bend->segments_empty()) {
            GuitarBendSegment *bend_seg = to_guitar_bend_segment(bend->front_segment());
            // Semi-hack: the relative position of note and bend
            // isn't fully known yet, so we use an approximation
            double sp = item->spatium();
            PointF approx_rel_pos(note_b_box.width() + 0.25 * sp, -0.25 * sp);
            shape.add(bend_seg->shape().translate(approx_rel_pos));
        }
        GuitarBend *bend_back = item->bend_back();
        if (bend_back && bend_back->bend_type() == GuitarBendType::SCOOP && !bend_back->segments_empty() &&
            item->find_ancestor(ElementType::SYSTEM)) {
            GuitarBendSegment *seg = to_guitar_bend_segment(bend_back->front_segment());
            shape.add(seg->shape().translated(seg->pos() - item->system_pos()));
        }
    }

    ldata->set_shape(shape);
}

void TLayout::layout_note_dot(const NoteDot *item, NoteDot::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    if (ldata->is_valid()) {
        return;
    }

    ldata->set_bbox(item->sym_bbox(SymId::AUGMENTATION_DOT));
}

void TLayout::layout_ornament(const Ornament *item, Ornament::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    LD_CONDITION(ldata->sym_id.has_value());

    ldata->set_shape(Shape());

    layout_articulation(const_cast<Ornament *>(item), ldata);

    double spatium_value = item->spatium();
    double vert_margin = 0.35 * spatium_value;
    static constexpr double ornament_accidental_mag = 0.6; // TODO: style?

    for (size_t i = 0; i < item->accidentals_above_and_below().size(); ++i) {
        bool above = (i == 0);
        Accidental *accidental = item->accidentals_above_and_below()[i];
        if (!accidental) {
            continue;
        }
        Accidental::LayoutData *acc_l_data = accidental->mutldata();

        LD_CONDITION(acc_l_data->is_set_bbox());

        accidental->compute_mag();

        acc_l_data->set_mag(acc_l_data->mag() * ornament_accidental_mag);
        acc_l_data->syms.clear(); // Invalidate
        layout_accidental(accidental, acc_l_data, conf);
        Shape accidental_shape = accidental->shape();
        double min_vert_dist = above ? accidental_shape.min_vertical_distance(Shape(ldata->bbox())) :
                                       Shape(ldata->bbox()).min_vertical_distance(accidental_shape);

        acc_l_data->set_pos(-0.5 * acc_l_data->bbox().width() + 0.5 * ldata->bbox().width(),
                            above ? (-min_vert_dist - vert_margin) : (min_vert_dist + vert_margin));
    }

    Shape sh(Shape::Type::COMPOSITE);
    sh.add(ldata->bbox(), item); // from Articulation
    for (const Accidental *accidental : item->accidentals_above_and_below()) {
        if (accidental && accidental->visible()) {
            LD_CONDITION(accidental->ldata()->is_set_shape());
            LD_CONDITION(accidental->ldata()->is_set_pos());
            sh.add(accidental->shape().translate(accidental->pos()));
        }
    }

    ldata->set_shape(sh);
}

void TLayout::layout_ornament_cue_note(Ornament *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    if (!item->ownership_parent()) {
        return;
    }

    Chord *parent_chord = to_chord(item->chord_rest());
    Chord *cue_note_chord = item->cue_note_chord();

    if (!cue_note_chord) {
        return;
    }

    const std::vector<Note *> &notes = cue_note_chord->notes();
    Note *cue_note = notes.empty() ? nullptr : notes.front();

    if (!cue_note) {
        return;
    }

    ChordLayout::layout_chords3({cue_note_chord}, {cue_note}, item->staff(), ctx);
    ChordLayout::layout_ledger_lines({cue_note_chord}, ctx);
    AccidentalsLayout::layout_accidentals({cue_note_chord}, ctx);
    layout_chord(cue_note_chord, ctx);

    Shape note_shape = cue_note_chord->shape();
    Shape parent_chord_shape = parent_chord->shape();
    double min_dist = HorizontalSpacing::min_horizontal_distance(parent_chord_shape, note_shape, parent_chord->spatium());
    // Check for possible other chords in same segment
    staff_idx_t start_staff = staff2track(parent_chord->staff_idx());
    for (staff_idx_t staff = start_staff; staff < start_staff + ::mu::engraving::voices; ++staff) {
        Segment *segment = parent_chord->segment();
        ChordRest *cr = segment->element(staff) ? to_chord_rest(segment->element(staff)) : nullptr;
        if (cr) {
            min_dist = std::max(min_dist, HorizontalSpacing::min_horizontal_distance(cr->shape(), note_shape, cr->spatium()));
        }
    }
    cue_note_chord->mutldata()->set_pos_x(min_dist);
}

void TLayout::layout_ottava(Ottava *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(item, ctx);
}

void TLayout::layout_ottava_segment(OttavaSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    OttavaSegment::LayoutData *ldata = item->mutldata();

    layout_text_line_base_segment(item, ctx);

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_page_lock_indicator(const PageLockIndicator *item, PageLockIndicator::LayoutData *ldata) {
    if (!item->configuration()->can_layout_icons()) {
        return;
    }
    const Page *page = item->page();
    const System *sys = item->system();

    IF_ASSERT_FAILED(page && sys) { return; }

    Shape shape;

    const double spatium = item->spatium();

    const FontMetrics metrics(item->font());
    const RectF icon_box = metrics.bounding_rect(item->icon_code());
    shape.add(icon_box, item);

    const double border_width = icon_box.height() + spatium;
    const double border_padding = 2 * spatium;
    const double icon_inset = (border_width - icon_box.width()) / 2;
    PointF pos_on_page = PointF(page->width() - border_padding - icon_box.width() - icon_box.left() - icon_inset,
                                page->height() - border_padding - icon_inset);
    PointF pos_on_system = pos_on_page - sys->pos();
    ldata->set_pos(pos_on_system);

    if (item->selected()) {
        // Outer range rectangle
        RectF range_rect = page->shape().bbox();
        PointF rect_page_pos = range_rect.top_left() - ldata->pos() - sys->pos();
        range_rect.translate(rect_page_pos);
        range_rect.adjust(border_padding, border_padding, -border_padding, -border_padding);
        ldata->range_rect = range_rect;

        // Inner rectangle cutout
        // Leaves border with rounded corners when painted
        RectF inner_rect = range_rect.adjusted(border_width, border_width, -border_width, -border_width);
        ldata->inner_range_rect = inner_rect;
        shape.add(RectF(PointF(range_rect.left(), range_rect.top()), PointF(range_rect.right(), inner_rect.top())));       // top
        shape.add(RectF(PointF(range_rect.left(), inner_rect.bottom()), PointF(range_rect.right(), range_rect.bottom()))); // bottom
        shape.add(RectF(PointF(range_rect.left(), inner_rect.top()), PointF(inner_rect.left(), inner_rect.bottom())));     // left
        shape.add(RectF(PointF(inner_rect.right(), inner_rect.top()), PointF(range_rect.right(), inner_rect.bottom())));   // right
    }

    ldata->set_shape(shape);

    // Ensure it goes behind notation and LayoutBreak
    const_cast<PageLockIndicator *>(item)->set_z(-100);
}

void TLayout::layout_palm_mute(PalmMute *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(item, ctx);
}

void TLayout::layout_palm_mute_segment(PalmMuteSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    PalmMuteSegment::LayoutData *ldata = item->mutldata();

    const StaffType *st_type = item->staff_type();

    if (st_type && st_type->is_hidden_element_on_tab(Sid::PALM_MUTE_SHOW_TAB_COMMON, Sid::PALM_MUTE_SHOW_TAB_SIMPLE)) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);

    layout_text_line_base_segment(item, ctx);

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_parenthesis(Parenthesis *item, Parenthesis::LayoutData *ldata, const LayoutContext &ctx) {
    ParenthesisLayout::layout_parenthesis(item, ldata, ctx);
}

void TLayout::layout_pedal(Pedal *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(item, ctx);
}

void TLayout::layout_pedal_segment(PedalSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    PedalSegment::LayoutData *ldata = item->mutldata();

    layout_text_line_base_segment(item, ctx);

    Text *end_text = item->end_text();
    if (end_text && !end_text->empty() && ldata->npoints > 0) { // Rosette
        PointF end_point = item->pos2();
        double end_text_width = end_text->ldata()->bbox().width();
        double x_end_text = end_point.x() - end_text_width;
        if (const Text *start_text = item->text()) {
            x_end_text = std::max(x_end_text, start_text->ldata()->bbox().width());
        }
        end_text->mutldata()->set_pos_x(x_end_text);

        double line_text_gap = item->absolute_from_spatium(item->get_property(Pid::GAP_BETWEEN_TEXT_AND_LINE).value<Spatium>());
        PointF &end_of_line = ldata->points.at(ldata->npoints - 1);
        end_of_line.set_x(x_end_text - line_text_gap);

        // Recalculate shape after moving end text
        Shape sh = recalculate_text_line_base_segment_shape(item);
        ldata->set_shape(sh);
    }

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_pick_scrape_segment(PickScrapeSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    PickScrapeSegment::LayoutData *ldata = item->mutldata();
    layout_text_line_base_segment(item, ctx);
    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_play_count_text(PlayCountText *item, TextBase::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    BarLine *bl = item->barline();
    Segment *seg = item->segment();

    TextLayout::layout_base_text_base(item, ldata);

    // Avoid incoming barlines from above
    double x_adj = 0.0;
    BarLine *bl_above = item->staff_idx() != 0 ? to_bar_line(seg->element(staff2track(item->staff_idx() - 1))) : nullptr;
    if (bl_above && bl_above->span_staff()) {
        x_adj = bl_above->width();
    }

    const double barline_width = bl->width();
    const double diff = item->width() - barline_width + x_adj;
    ldata->move_x(-diff);
}

void TLayout::layout_play_tech_annotation(const PlayTechAnnotation *item, PlayTechAnnotation::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    TextLayout::layout_base_text_base(item, ldata);

    if (item->is_handbells_symbol()) {
        Chord *chord = nullptr;
        Segment *seg = item->segment();
        track_idx_t s_track = staff2track(item->staff_idx());
        track_idx_t e_track = s_track + ::mu::engraving::voices;
        for (track_idx_t track = s_track; track < e_track; ++track) {
            if (seg->element(track) && seg->element(track)->is_chord()) {
                chord = to_chord(seg->element(track));
                break;
            }
        }
        double center = 0.0;
        if (chord) {
            Note *ref_note = item->place_above() ? chord->up_note() : chord->down_note();
            center = 0.5 * ref_note->width() + ref_note->x() + chord->x();
        } else {
            center = 0.5 * item->score()->note_head_width();
        }
        ldata->set_pos_x(center - 0.5 * (ldata->bbox().right() - ldata->bbox().left()));
    }

    if (item->autoplace()) {
        const Segment *s = to_segment(item->ownership_parent());
        const Measure *m = s->measure();
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(m->ldata()->is_set_pos());
        LD_CONDITION(s->ldata()->is_set_pos());
    }

    Autoplace::autoplace_segment_element(item, ldata);
}

void TLayout::layout_rasgueado_segment(RasgueadoSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    RasgueadoSegment::LayoutData *ldata = item->mutldata();
    const StaffType *st_type = item->staff_type();

    if (st_type && st_type->is_hidden_element_on_tab(Sid::RASGUEADO_SHOW_TAB_COMMON, Sid::RASGUEADO_SHOW_TAB_SIMPLE)) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);

    layout_text_line_base_segment(item, ctx);

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_rehearsal_mark(const RehearsalMark *item, RehearsalMark::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    TextLayout::layout_base_text_base(item, ldata);

    const Segment *s = item->segment();

    LD_CONDITION(s->ldata()->is_set_pos());

    if (!s->rtick().is_zero()) {
        return;
    }

    // first CR of measure, alignment is hcenter or right (the usual cases)
    // align with barline, point just after header, or start of measure depending on context
    const Measure *m = s->measure();
    const Segment *header = s->prev(); // possibly just a start repeat
    while (header && header->is_type(SegmentType::DURATION)) {
        header = header->prev();
    }
    double measure_x = -s->x();
    const Segment *repeat = m->find_segment_r(SegmentType::START_REPEAT_BAR_LINE, Fraction(0, 1));

    if (repeat) {
        LD_CONDITION(repeat->ldata()->is_set_pos());
    }

    double barline_x = repeat ? repeat->x() - s->x() : measure_x;
    const System *sys = m->system();
    bool system_first = (sys && m->is_first_in_system());

    if (!header || repeat || !system_first) {
        // no header, or header with repeat, or header mid-system - align with barline
        ldata->set_pos_x(barline_x);
        check_rehearsal_mark_vs_big_time_sig(item, ldata);
        return;
    }

    // header at start of system
    // align to a point just after the header
    EngravingItem *e = header->element(item->track());

    if (e) {
        LD_CONDITION(e->ldata()->is_set_bbox());
    }

    LD_CONDITION(header->ldata()->is_set_bbox());

    double w = e ? e->ldata()->bbox().width() : header->ldata()->bbox().width();
    ldata->set_pos_x(header->x() + w - s->x());

    // special case for right aligned rehearsal marks at start of system
    // left align with start of measure if that is further left
    if (item->position() == AlignH::RIGHT) {
        ldata->set_pos_x(std::min(ldata->pos().x(), measure_x + ldata->bbox().width()));
    }

    check_rehearsal_mark_vs_big_time_sig(item, ldata);
}

void TLayout::check_rehearsal_mark_vs_big_time_sig(const RehearsalMark *item, TextBase::LayoutData *ldata) {
    if (item->score()->style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() != TimeSigPlacement::ABOVE_STAVES) {
        return;
    }

    const Segment *s = item->segment();
    TimeSig *big_time_sig = nullptr;
    for (const Segment *segment = s; segment && segment->tick() == s->tick(); segment = segment->prev_active()) {
        if (!segment->is_type(SegmentType::TIME_SIG_TYPES)) {
            continue;
        }
        TimeSig *time_sig = to_time_sig(segment->element(item->track()));
        if (time_sig && time_sig->visible()) {
            big_time_sig = time_sig;
            break;
        }
    }

    if (!big_time_sig) {
        return;
    }

    double time_sig_right_edge =
        big_time_sig->ldata()->bbox().translated(big_time_sig->ldata()->pos()).right() + big_time_sig->segment()->x() - s->x();
    double reh_mark_left_edge = ldata->bbox().translated(item->ldata()->pos()).left();
    double distance = reh_mark_left_edge - time_sig_right_edge;
    const double margin = 0.5 * item->font_metrics().x_height();
    if (distance < margin) {
        ldata->move_x(margin - distance);
    }
}

void TLayout::layout_rest(const Rest *item, Rest::LayoutData *ldata, const LayoutContext &ctx) {
    RestLayout::layout_rest(item, ldata, ctx);
}

void TLayout::layout_line(SLine *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    item->compute_start_element();
    item->compute_end_element();

    System *s1;
    System *s2;
    PointF p1(item->line_pos(Grip::START, &s1));
    PointF p2(item->line_pos(Grip::END, &s2));

    const std::vector<System *> &systems = ctx.dom().systems();
    system_idx_t sys_idx1 = muse::index_of(systems, s1);
    system_idx_t sys_idx2 = muse::index_of(systems, s2);
    int segments_needed = 0;

    if (sys_idx1 == muse::nidx || sys_idx2 == muse::nidx) {
        return;
    }

    for (system_idx_t i = sys_idx1; i <= sys_idx2; ++i) {
        if (systems.at(i)->vbox()) {
            continue;
        }
        ++segments_needed;
    }

    int seg_count = int(item->spanner_segments().size());

    if (segments_needed != seg_count) {
        item->fixup_segments(segments_needed, [item]() { return item->create_line_segment(); });
        if (segments_needed > seg_count) {
            for (int i = seg_count; i < segments_needed; ++i) {
                LineSegment *line_segm = item->segment_at(i);
                // set user offset to previous segment's offset
                if (seg_count > 0) {
                    line_segm->set_offset(PointF(0, item->segment_at(i - 1)->offset().y()));
                } else {
                    line_segm->set_offset(PointF(0, item->offset().y()));
                }
            }
        }
    }

    int seg_idx = 0;
    for (system_idx_t i = sys_idx1; i <= sys_idx2; ++i) {
        System *system = systems.at(i);
        if (system->vbox()) {
            continue;
        }
        LineSegment *line_segm = item->segment_at(seg_idx++);
        line_segm->set_track(item->track()); // DEBUG
        line_segm->move_to_system(system);

        if (sys_idx1 == sys_idx2) {
            // single segment
            line_segm->set_spanner_segment_type(SpannerSegmentType::SINGLE);
            double len = p2.x() - p1.x();
            // enforcing a minimum length would be possible but inadvisable
            // the line length calculations are tuned well enough that this should not be needed
            //if (anchor() == Anchor::SEGMENT && !isPedal())
            //      len = std::max(1.0 * spatium(), len);
            line_segm->set_pos(p1);
            line_segm->set_pos2(PointF(len, p2.y() - p1.y()));
        } else if (i == sys_idx1) {
            // start segment
            line_segm->set_spanner_segment_type(SpannerSegmentType::BEGIN);
            line_segm->set_pos(p1);
            double x2 = system->ending_x_for_open_ended_lines();
            line_segm->set_pos2(PointF(x2 - p1.x(), 0.0));
        } else if (i > 0 && i != sys_idx2) {
            // middle segment
            line_segm->set_spanner_segment_type(SpannerSegmentType::MIDDLE);
            double x1 = system->first_note_rest_segment_x(true);
            double x2 = system->ending_x_for_open_ended_lines();
            line_segm->set_pos(PointF(x1, p1.y()));
            line_segm->set_pos2(PointF(x2 - x1, 0.0));
        } else if (i == sys_idx2) {
            // end segment
            double min_len = 0.0;
            double x1 = system->first_note_rest_segment_x(true);
            double len = std::max(min_len, p2.x() - x1);
            line_segm->set_spanner_segment_type(SpannerSegmentType::END);
            line_segm->set_pos(PointF(p2.x() - len, p2.y()));
            line_segm->set_pos2(PointF(len, 0.0));
        }
        layout_line_segment(line_segm, ctx);
    }
}

void TLayout::layout_note_anchored_line(SLine *item, EngravingItem::LayoutData *ldata, LayoutContext &ctx) {
    double spatium_value = item->spatium();
    Note *start_anchor = to_note(item->start_element());
    Note *end_anchor = to_note(item->end_element());
    Chord *start_chord = start_anchor->chord();
    Chord *end_chord = end_anchor->chord();
    LineSegment *start_seg = to_line_segment(item->front_segment());
    LineSegment *end_seg = to_line_segment(item->back_segment());

    // Note: line segments are defined by
    // initial point: ipos() (relative to system origin)
    // ending point:  pos2() (relative to initial point)

    // LINE ENDING POINTS TO NOTEHEAD CENTRES

    // assume line goes from centre of initial note centre to centre of ending note:
    // move first segment origin and last segment ending point from notehead origin to notehead centre
    // For TAB: begin at the right-edge of initial note rather than centre
    PointF start_offset = (start_chord->staff()->is_tab_staff(start_chord->tick())) ? PointF(start_anchor->ldata()->bbox().right(), 0.0) :
                                                                                      PointF(start_anchor->head_width() * 0.5, 0.0);

    PointF end_offset = PointF(end_anchor->head_width() * 0.5, 0.0);

    // AVOID HORIZONTAL LINES

    // for microtonality read tuning, or check note accidental
    double start_tune = start_anchor->tuning();
    double end_tune = end_anchor->tuning();
    AccidentalType start_acc = start_anchor->accidental_type();
    AccidentalType end_acc = end_anchor->accidental_type();
    if (muse::real_is_null(start_tune) && Accidental::is_microtonal(start_acc)) {
        start_tune = Accidental::subtype2cent_offset(start_acc);
    }
    if (muse::real_is_null(end_tune) && Accidental::is_microtonal(end_acc)) {
        end_tune = Accidental::subtype2cent_offset(end_acc);
    }

    int up_down = (0 < (end_anchor->ppitch() - start_anchor->ppitch())) - ((end_anchor->ppitch() - start_anchor->ppitch()) < 0);
    // same note, so compare tunings
    if (up_down == 0) {
        up_down = (0 < (end_tune - start_tune)) - ((end_tune - start_tune) < 0);
    }

    // on TAB's, glissando are by necessity on the same string, this gives an horizontal glissando line;
    // make bottom end point lower and top ending point higher
    if (start_chord->staff()->is_tab_staff(start_chord->tick())) {
        double y_off = start_chord->staff()->line_distance(start_chord->tick()) * 0.4 * spatium_value;
        start_offset.ry() += y_off * up_down;
        end_offset.ry() -= y_off * up_down;
    }
    // if not TAB, angle glissando between notes on the same line
    else {
        if (start_anchor->line() == end_anchor->line()) {
            start_offset.ry() += spatium_value * 0.25 * up_down;
            end_offset.ry() -= spatium_value * 0.25 * up_down;
        }
    }

    // move initial point of first segment and adjust its length accordingly
    start_seg->set_pos(start_seg->ldata()->pos() + start_offset);
    start_seg->set_pos2(start_seg->ipos2() - start_offset);
    // adjust ending point of last segment
    end_seg->set_pos2(end_seg->ipos2() + end_offset);

    // INTERPOLATION OF INTERMEDIATE POINTS

    // get total x-width and total y-height of all segments
    double x_tot = 0.0;
    for (SpannerSegment *segm : item->spanner_segments()) {
        x_tot += segm->ipos2().x();
    }
    double start_y = start_seg->ldata()->pos().y();
    double y_tot = end_seg->ldata()->pos().y() + end_seg->ipos2().y() - start_y;
    y_tot -= y_staff_difference(end_seg->system(), start_seg->system(), track2staff(item->track2()));
    double ratio = muse::divide(y_tot, x_tot, 1.0);
    // interpolate y-coord of intermediate points across total width and height
    double x_curr = 0.0;
    double y_curr;
    for (unsigned i = 0; i + 1 < item->spanner_segments().size(); i++) {
        SpannerSegment *segm = item->segment_at(i);
        x_curr += segm->ipos2().x();
        y_curr = start_y + ratio * x_curr;
        segm->rypos2() = y_curr - segm->ldata()->pos().y(); // position segm. end point at yCurr
        // next segment shall start where this segment stopped
        SpannerSegment *next_seg = item->segment_at(i + 1);
        y_curr += y_staff_difference(next_seg->system(), segm->system(), track2staff(item->track2()));
        segm = next_seg;
        segm->rypos2() += segm->ldata()->pos().y() - y_curr; // adjust next segm. vertical length
        segm->mutldata()->set_pos_y(y_curr);                 // position next segm. start point at yCurr
    }

    // KEEP CLEAR OF ALL ELEMENTS OF THE CHORD
    // Remove offset already applied
    start_offset *= -1.0;
    end_offset *= -1.0;
    // Look at chord shapes (but don't consider lyrics)
    Shape start_cr_shape = start_chord->shape();
    start_cr_shape.remove_if([](ShapeElement &s) {
        if (!s.item() || s.item()->is_lyrics()) {
            return true;
        } else {
            return false;
        }
    });

    double start_y_above = start_anchor->ldata()->pos().y() + start_anchor->ldata()->bbox().top_right().y();
    double start_y_below = start_y_above + start_anchor->ldata()->bbox().height();
    start_offset.rx() += start_cr_shape.right_most_edge_at_height(start_y_above, start_y_below) - start_anchor->pos().x();
    if (!end_chord->staff()->is_tab_staff(end_chord->tick())) {
        double end_y_above = end_anchor->ldata()->pos().y() + end_anchor->ldata()->bbox().top_left().y();
        double end_y_below = end_y_above + end_anchor->ldata()->bbox().height();
        double note_middle = end_y_above + end_anchor->ldata()->bbox().height() / 2;
        if (up_down != 0) {
            int ll_width = ctx.conf().style_s(Sid::LEDGER_LINE_WIDTH).val() * spatium_value;
            // Only check top/bottom half of note depending on line approach direction
            // to avoid clearing acidentals the line won't collide with
            end_y_above = up_down == 1 ? note_middle - ll_width : end_y_above;
            end_y_below = up_down == 1 ? end_y_below : note_middle + ll_width;
        }

        end_offset.rx() -= end_anchor->pos().x() - end_chord->shape().left_most_edge_at_height(end_y_above, end_y_below);
    }
    // Add note distance
    const double line_note_dist = 0.25 * item->spatium(); // TODO: style
    start_offset.rx() += line_note_dist;
    end_offset.rx() -= line_note_dist;

    // apply offsets: shorten first segment by x1 (and proportionally y) and adjust its length accordingly
    start_offset.ry() = start_seg->ipos2().y() * muse::divide(start_offset.x(), start_seg->ipos2().x(), 1.0);
    start_seg->set_pos(start_seg->ldata()->pos() + start_offset);
    start_seg->set_pos2(start_seg->ipos2() - start_offset);
    // adjust last segment length by x2 (and proportionally y)
    end_offset.ry() = end_seg->ipos2().y() * muse::divide(end_offset.x(), end_seg->ipos2().x(), 1.0);
    end_seg->set_pos2(end_seg->ipos2() + end_offset);

    for (SpannerSegment *segm : item->spanner_segments()) {
        TLayout::layout_item(segm, ctx);
    }

    // compute line bbox as the bbox of the last segment, relative to the end anchor note
    PointF end_anchor_page_pos = end_anchor->page_pos();
    PointF end_system_page_pos;
    IF_ASSERT_FAILED(end_chord->segment()->system()) { end_system_page_pos = end_seg->pos(); }
    else {
        end_system_page_pos = end_chord->segment()->system()->page_pos();
    }

    PointF end_anchor_syst_pos = end_anchor_page_pos - end_system_page_pos;
    RectF r = RectF(end_anchor_syst_pos - end_seg->pos(), end_anchor_syst_pos - end_seg->pos() - end_seg->pos2()).normalized();
    double lw = item->absolute_from_spatium(item->line_width()) * .5;
    ldata->set_bbox(r.adjusted(-lw, -lw, lw, lw));
}

void TLayout::layout_note_line(NoteLine *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    if (item->start_element()) {
        item->set_tick(item->start_element()->tick());
    }

    if (item->end_element()) {
        item->set_tick2(item->end_element()->tick());
    }

    TLayout::layout_line(item, ctx);

    if (item->line_end_placement() == NoteLineEndPlacement::OFFSET_ENDS) {
        layout_note_anchored_line(item, item->mutldata(), ctx);
        ChordLayout::add_line_attach_points(item);
    }
}

void TLayout::layout_note_line_segment(NoteLineSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);

    NoteLineSegment::LayoutData *ldata = item->mutldata();
    layout_text_line_base_segment(item, ctx);

    if (item->staff()) {
        ldata->set_mag(item->staff()->staff_mag(item->tick()));
    }
}

void TLayout::layout_slur(Slur *item, LayoutContext &ctx) { SlurTieLayout::create_slur_segments(item, ctx); }

void TLayout::layout_spacer(Spacer *item, LayoutContext &) {
    LAYOUT_CALL_ITEM(item);
    Spacer::LayoutData *ldata = item->mutldata();

    double spatium = item->spatium();

    PainterPath path = PainterPath();
    double w = spatium;
    double b = w * .5;
    double h = item->ownership_parent() ? item->absolute_gap() :
                                          item->absolute_from_spatium(std::min(item->gap(), 4.0_sp)); // limit length for palette

    switch (item->spacer_type()) {
        case SpacerType::DOWN:
            path.line_to(w, 0.0);
            path.move_to(b, 0.0);
            path.line_to(b, h);
            path.line_to(0.0, h - b);
            path.move_to(b, h);
            path.line_to(w, h - b);
            break;
        case SpacerType::UP:
            path.move_to(b, 0.0);
            path.line_to(0.0, b);
            path.move_to(b, 0.0);
            path.line_to(w, b);
            path.move_to(b, 0.0);
            path.line_to(b, h);
            path.move_to(0.0, h);
            path.line_to(w, h);
            break;
        case SpacerType::FIXED:
            path.line_to(w, 0.0);
            path.move_to(b, 0.0);
            path.line_to(b, h);
            path.move_to(0.0, h);
            path.line_to(w, h);
            break;
    }
    ldata->path = path;
    double lw = spatium * 0.4;
    RectF bb(0, 0, w, h);
    bb.adjust(-lw, -lw, lw, lw);
    ldata->set_bbox(bb);

    item->set_z(0.0);
}

void TLayout::layout_spanner(Spanner *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_item(item, ctx);
}

void TLayout::layout_staff_lines(StaffLines *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_for_width(item, item->measure()->width(), ctx);
}

void TLayout::layout_for_width(StaffLines *item, double w, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    StaffLines::LayoutData *ldata = item->mutldata();
    const Staff *s = item->staff();
    double spatium_value = item->spatium();
    double dist = spatium_value;
    item->set_pos(PointF(0.0, 0.0));
    int lines_value;
    if (s) {
        ldata->set_mag(s->staff_mag(item->measure()->tick()));
        item->set_visible(!s->is_lines_invisible(item->measure()->tick()));
        item->set_color(s->color(item->measure()->tick()));
        const StaffType *st = s->staff_type(item->measure()->tick());
        dist *= st->line_distance().val();
        lines_value = st->lines();
        ldata->set_pos_y(st->yoffset().val() * spatium_value);
        //            if (_lines == 1)
        //                  rypos() = 2 * _spatium;
    } else {
        lines_value = 5;
        item->set_color(item->configuration()->default_color());
    }
    item->set_lw(ctx.conf().style_s(Sid::STAFF_LINE_WIDTH).val() * spatium_value);
    double x1 = item->pos().x();
    double x2 = x1 + w;
    double y = item->pos().y();
    ldata->set_bbox(x1, -item->lw() * .5 + y, w, (lines_value - 1) * dist + item->lw());

    std::vector<LineF> ll;
    for (int i = 0; i < lines_value; ++i) {
        ll.push_back(LineF(x1, y, x2, y));
        y += dist;
    }
    item->set_lines(ll);
}

void TLayout::layout_staff_state(const StaffState *item, StaffState::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    if (ldata->is_valid()) {
        return;
    }

    double spatium_value = item->spatium();
    ldata->lw = (spatium_value * 0.3);
    double h = spatium_value * 4;
    double w = spatium_value * 2.5;
    //      double w1 = w * .6;

    PainterPath path;
    switch (item->staff_state_type()) {
        case StaffStateType::INSTRUMENT:
            path.line_to(w, 0.0);
            path.line_to(w, h);
            path.line_to(0.0, h);
            path.line_to(0.0, 0.0);
            path.move_to(w * .5, h - spatium_value * .5);
            path.line_to(w * .5, spatium_value * 2);
            path.move_to(w * .5, spatium_value * .8);
            path.line_to(w * .5, spatium_value * 1.0);
            break;

        case StaffStateType::TYPE:
            path.line_to(w, 0.0);
            path.line_to(w, h);
            path.line_to(0.0, h);
            path.line_to(0.0, 0.0);
            break;

        case StaffStateType::VISIBLE:
            path.line_to(w, 0.0);
            path.line_to(w, h);
            path.line_to(0.0, h);
            path.line_to(0.0, 0.0);
            break;

        case StaffStateType::INVISIBLE:
            path.line_to(w, 0.0);
            path.line_to(w, h);
            path.line_to(0.0, h);
            path.line_to(0.0, 0.0);
            break;

        default: LOGD("unknown layout break symbol"); break;
    }

    ldata->path = path;

    RectF bb(0, 0, w, h);
    bb.adjust(-ldata->lw, -ldata->lw, ldata->lw, ldata->lw);
    ldata->set_bbox(bb);
    ldata->set_pos(0.0, spatium_value * -6.0);
}

void TLayout::layout_staff_text(const StaffText *item, StaffText::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    TextLayout::layout_base_text_base(item, ldata);

    if (item->autoplace()) {
        const Segment *s = to_segment(item->ownership_parent());
        const Measure *m = s->measure();
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(m->ldata()->is_set_pos());
        LD_CONDITION(s->ldata()->is_set_pos());
    }

    Autoplace::autoplace_segment_element(item, ldata);

    if (SoundFlag *flag = item->sound_flag()) {
        layout_sound_flag(flag, flag->mutldata());
    }
}

void TLayout::layout_stave_sharing_label(const StaveSharingLabel *item, TextBase::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    TextLayout::layout_base_text_base(item, ldata);
    Autoplace::autoplace_segment_element(item, ldata);
}

void TLayout::layout_staff_type_change(const StaffTypeChange *item, StaffTypeChange::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    double spatium = conf.spatium();
    ldata->set_bbox(RectF(-item->lw() * .5, -item->lw() * .5, spatium * 2.5 + item->lw(), spatium * 2.5 + item->lw()));
    if (item->measure()) {
        double y = -1.5 * spatium - ldata->bbox().height() + item->measure()->system()->staff(item->staff_idx())->y();
        ldata->set_pos(spatium * .8, y);
    } else {
        ldata->set_pos(0.0, 0.0);
    }
}

void TLayout::layout_stem(const Stem *item, Stem::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);

    const bool up = item->up();
    const double up_value = up ? -1.0 : 1.0;

    //Note* note = up ? item->chord()->downNote() : item->chord()->upNote();
    //! NOTE A lot of spam
    // LD_CONDITION(note->ldata()->mirror.has_value());

    double y1 = 0.0; // vertical displacement to match note attach point
    double y2 = up_value * (item->length());

    bool is_tab_staff = false;
    if (item->chord()) {
        ldata->set_mag(item->chord()->mag());

        const Staff *staff = item->staff();
        const StaffType *staff_type = staff ? staff->staff_type_for_element(item->chord()) : nullptr;
        is_tab_staff = staff_type && staff_type->is_tab_staff();

        if (is_tab_staff) {
            if (staff_type->stem_through()) {
                // if stems through staves, gets Y pos. of stem-side note relative to chord other side
                const double staff_lines_distance = staff_type->line_distance().val() * item->spatium();
                y1 = (item->chord()->down_string() - item->chord()->up_string()) * up_value * staff_lines_distance;

                // if fret marks above lines, raise stem beginning by 1/2 line distance
                if (!staff_type->on_lines()) {
                    y1 -= staff_lines_distance * 0.5;
                }

                // shorten stem by 1/2 lineDist to clear the note and a little more to keep 'air' between stem and note
                y1 += up_value * staff_lines_distance * 0.7;
            }
            // in other TAB types, no correction
        } else { // non-TAB
            // move stem start to note attach point
            Note *note = up ? item->chord()->down_note() : item->chord()->up_note();
            if ((up && !note->ldata()->mirror.value(LdAccess::BAD)) || (!up && note->ldata()->mirror.value(LdAccess::BAD))) {
                y1 = note->stem_up_se().y();
            } else {
                y1 = note->stem_down_nw().y();
            }

            ldata->set_pos_y(note->ldata()->pos().y());
        }

        const Hook *hook = item->chord()->hook();
        const Beam *beam = item->chord()->beam();
        if (hook && !beam) {
            y2 += hook->smufl_anchor().y();
        }

        if (beam) {
            y2 -= up_value * beam->absolute_from_spatium(conf.style_s(Sid::BEAM_WIDTH)) * .5 * beam->mag();
        }
    }

    double line_width_correction = item->line_width_mag() * 0.5;
    double line_x = is_tab_staff ? 0.0 : up_value * line_width_correction;

    LineF line = LineF(line_x, y1, line_x, y2);
    ldata->line = line;

    // compute line and bounding rectangle
    RectF rect(line.p1(), line.p2() + PointF(0.0, ldata->beam_correction));
    ldata->set_bbox(rect.normalized().adjusted(-line_width_correction, 0, line_width_correction, 0));
}

void TLayout::layout_stem_slash(const StemSlash *item, StemSlash::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    const Chord *c = item->chord();
    const Stem *stem = c->stem();
    const Hook *hook = c->hook();
    const Beam *beam = c->beam();

    LD_CONDITION(stem->ldata()->is_set_pos());
    LD_CONDITION(stem->ldata()->is_set_bbox());
    //  Notes without a stem (including whole notes) don't draw a slash.

    static constexpr double height_reduction = 0.66;
    static constexpr double angle_increase = 1.2;
    static constexpr double length_increase = 1.1;
    const double mag = c->mag();

    double up = c->up() ? -1 : 1;
    double stem_tip_y =
        c->up() ? stem->ldata()->bbox().translated(stem->pos()).top() : stem->ldata()->bbox().translated(stem->pos()).bottom();
    double left_hang = conf.note_head_width() * mag / 2;
    double angle = conf.style_d(Sid::STEM_SLASH_ANGLE) * M_PI / 180; // converting to radians
    bool straight = conf.style_b(Sid::USE_STRAIGHT_NOTE_FLAGS);
    double grace_note_mag = mag;

    double start_x = stem->ldata()->bbox().translated(stem->pos()).right() - left_hang;
    double start_y =
        stem_tip_y - up * grace_note_mag * conf.style_absolute(Sid::STEM_SLASH_POSITION) * (straight || !hook ? height_reduction : 1);

    double end_x = 0;
    double end_y = 0;

    if (hook) {
        auto music_font = conf.style_st(Sid::MUSICAL_SYMBOL_FONT);
        // HACK: adjust slash angle for fonts with "fat" hooks. In future, we must use smufl cutOut
        if (c->beams() >= 2 && !straight && (music_font == "Bravura" || music_font == "Finale Maestro" || music_font == "Gonville")) {
            angle *= angle_increase;
        }
        end_x = hook->ldata()->bbox().translated(hook->ldata()->pos()).right(); // always ends at the right bbox margin of the hook
        end_y = start_y + up * (end_x - start_x) * tan(angle);
    } else if (beam) {
        PointF p1 = beam->start_anchor();
        PointF p2 = beam->end_anchor();
        double beam_angle = p2.x() > p1.x() ? atan((p2.y() - p1.y()) / (p2.x() - p1.x())) : 0;
        angle += up * beam_angle / 2;
        double length = 2 * item->spatium();
        bool obtuse_angle = (c->up() && beam_angle < 0) || (!c->up() && beam_angle > 0);
        if (obtuse_angle) {
            length *= length_increase; // needs to be slightly longer
        }
        end_x = start_x + length * cos(angle);
        end_y = start_y + up * length * sin(angle);
    } else {
        double right_hang =
            (conf.note_head_width() * mag / 2) - stem->width(); //  subtract the stem width so the slash is optically centered on the stem
        end_x = stem->ldata()->bbox().translated(stem->pos()).right() + right_hang;
        end_y = start_y + up * (end_x - start_x) * tan(angle);
    }

    ldata->line = LineF(PointF(start_x, start_y), PointF(end_x, end_y));
    ldata->stem_width = conf.style_absolute(Sid::STEM_SLASH_THICKNESS) * grace_note_mag;

    RectF bbox = RectF(ldata->line.p1(), ldata->line.p2()).normalized();
    bbox = bbox.adjusted(-ldata->stem_width / 2, -ldata->stem_width / 2, ldata->stem_width, ldata->stem_width);
    ldata->set_bbox(bbox);
}

void TLayout::layout_sticking(const Sticking *item, Sticking::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    TextLayout::layout_base_text_base(item, ldata);

    if (item->autoplace() && item->ownership_parent()) {
        const Segment *s = to_segment(item->ownership_parent());
        const Measure *m = s->measure();
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(m->ldata()->is_set_pos());
        LD_CONDITION(s->ldata()->is_set_pos());
    }

    Autoplace::autoplace_segment_element(item, ldata);
}

void TLayout::layout_string_tunings(StringTunings *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    item->update_text();

    TextLayout::layout_base_text_base(item, ctx);

    double spatium = item->spatium();

    if (item->no_string_visible()) {
        Font font(item->font());

        RectF rect;
        rect.set_top_left({0, item->ldata()->bbox().y() - 2.5 * spatium});
        rect.set_size({spatium, 2.5 * spatium});

        item->setbbox(rect);
    }

    for (TextBlock &block : item->mutldata()->blocks) {
        for (TextFragment &fragment : block.fragments()) {
            Font font = fragment.font(item);
            if (font.type() == Font::Type::MUSIC_SYMBOL) {
                // HACK: the music symbol doesn't have a good baseline
                // to go with text so we correct it here
                const double baseline_adjustment = 0.35 * spatium * item->symbol_scale();
                fragment.pos.set_y(fragment.pos.y() + baseline_adjustment);
            }
        }
    }

    double second_string_x_align = 0.0;
    for (const TextFragment &fragment : item->fragment_list()) {
        if (fragment.font(item).type() == Font::Type::MUSIC_SYMBOL) {
            second_string_x_align = std::max(second_string_x_align, fragment.pos.x());
        }
    }

    for (TextBlock &block : item->mutldata()->blocks) {
        double x_move = 0.0;
        for (TextFragment &fragment : block.fragments()) {
            if (block.fragments().front() == fragment) { // skip first
                continue;
            }

            if (fragment.font(item).type() == Font::Type::MUSIC_SYMBOL) {
                x_move = second_string_x_align - fragment.pos.x();
            }
            fragment.pos.set_x(fragment.pos.x() + x_move);
        }
    }

    Segment *parent_segment = item->segment();
    item->move(PointF(-parent_segment->x() + item->spatium(), 0.0));

    Autoplace::autoplace_segment_element(item, item->mutldata());
}

void TLayout::layout_sound_flag(const SoundFlag *item, SoundFlag::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);

    const Score *score = item->score();
    if (score && !score->show_sound_flags()) {
        return;
    }

    const EngravingItem *parent = item->ownership_parent_item();
    if (!parent) {
        return;
    }

    double spatium = item->spatium();
    double icon_height = spatium * 3.0;

    RectF parent_bbox = parent->ldata()->bbox();
    RectF icon_b_box = RectF(parent_bbox.x(), parent_bbox.y(), icon_height, icon_height);

    icon_b_box.move_center(parent_bbox.center());

    // <icon><space><text>
    double space = spatium / 2.0;
    icon_b_box.set_x(parent_bbox.x() - icon_b_box.width() - space);

    ldata->set_bbox(icon_b_box);
}

void TLayout::layout_symbol(const Symbol *item, Symbol::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    LD_INDEPENDENT;

    double expected_mag = item->staff() ? item->staff()->staff_mag(item->tick()) : ldata->mag();

    if (ldata->is_valid() && real_is_equal(ldata->mag(), expected_mag)) {
        return;
    }

    if (item->staff()) {
        ldata->set_mag(expected_mag);
    }
    ldata->set_bbox(item->score_font() ? item->score_font()->bbox(item->sym(), item->mag_s() * item->symbols_size()) :
                                         item->sym_bbox(item->sym()));
    double w = ldata->bbox().width();
    PointF p;
    if (item->align() == AlignV::BOTTOM) {
        p.set_y(-ldata->bbox().height());
    } else if (item->align() == AlignV::VCENTER) {
        p.set_y((-ldata->bbox().height()) * .5);
    } else if (item->align() == AlignV::BASELINE) {
        p.set_y(-item->base_line());
    }
    if (item->align() == AlignH::RIGHT) {
        p.set_x(-w);
    } else if (item->align() == AlignH::HCENTER) {
        p.set_x(-(w * .5));
    }
    ldata->set_pos(p);

    // see BSymbol::add
    for (EngravingItem *e : item->leafs()) {
        switch (e->type()) {
            case ElementType::SYMBOL: {
                Symbol *s = item_cast<Symbol *>(e);
                layout_symbol(s, s->mutldata(), ctx);
            } break;
            case ElementType::IMAGE: {
                Image *im = item_cast<Image *>(e);
                layout_image(im, im->mutldata());
            } break;
            default: UNREACHABLE; break;
        }
    }
}

void TLayout::layout_f_symbol(const FSymbol *item, FSymbol::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;
    if (ldata->is_valid()) {
        return;
    }

    ldata->set_bbox(FontMetrics::bounding_rect(item->font(), item->to_string()));
}

void TLayout::layout_system_divider(const SystemDivider *item, SystemDivider::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;
    if (ldata->is_valid()) {
        return;
    }

    layout_symbol(item, ldata, ctx);
}

void TLayout::layout_system_text(const SystemText *item, SystemText::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    TextLayout::layout_base_text_base(item, ldata);

    if (item->autoplace() && item->ownership_parent()) {
        const Segment *s = to_segment(item->ownership_parent());
        const Measure *m = s->measure();
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(m->ldata()->is_set_pos());
        LD_CONDITION(s->ldata()->is_set_pos());
    }

    Autoplace::autoplace_segment_element(item, ldata);
}

void TLayout::layout_tab_duration_symbol(const TabDurationSymbol *item, TabDurationSymbol::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    static constexpr Spatium tab_restsymbdispl = 2.0_sp;

    if (!item->tab()) {
        ldata->set_bbox(RectF());
        return;
    }
    double spatium = item->spatium();
    double hbb, wbb, xbb, ybb; // bbox sizes
    double xpos, ypos;         // position coords

    ldata->beam_grid = TabBeamGrid::NONE;
    ldata->beam_length = 0.0;
    ldata->beam_level = 0;
    Chord *chord = item->ownership_parent() && item->ownership_parent()->is_chord() ? to_chord(item->ownership_parent()) : nullptr;
    // A grid replaces the individual duration signs. Only connect equal, undotted
    // values; otherwise retain the signs rather than lose their rhythm information.
    auto connects = [](const ChordRest *left, const ChordRest *right) {
        if (!left || !right || !left->is_chord() || !right->is_chord()) {
            return false;
        }
        const bool regular_chords = !left->is_grace() && !right->is_grace() && !to_chord(left)->no_stem() && !to_chord(right)->no_stem();
        const bool same_context =
            left->measure() == right->measure() && left->staff_type() == right->staff_type() && left->tuplet() == right->tuplet();
        const bool equal_undotted_durations = left->duration_type() == right->duration_type() && !left->dots() && !right->dots();
        const bool left_continues = left->beam_mode() == BeamMode::BEGIN || left->beam_mode() == BeamMode::MID;
        const bool right_connects = right->beam_mode() == BeamMode::MID || right->beam_mode() == BeamMode::END;
        return regular_chords && same_context && equal_undotted_durations && left_continues && right_connects;
    };
    const int beam_level =
        chord ? static_cast<int>(chord->duration_type().type()) - static_cast<int>(item->tab()->tab_duration_font().zero_beam_level) : 0;
    const bool joins_previous = beam_level > 0 && (chord->beam_mode() == BeamMode::MID || chord->beam_mode() == BeamMode::END) &&
                                connects(Navigation::prev_chord_rest(chord), chord);
    const bool joins_next = beam_level > 0 && (chord->beam_mode() == BeamMode::BEGIN || chord->beam_mode() == BeamMode::MID) &&
                            connects(chord, Navigation::next_chord_rest(chord));
    // if no chord (shouldn't happens...) or not a special beam mode, layout regular symbol
    if (beam_level < 1 || (!joins_previous && !joins_next)) {
        FontMetrics fm(item->tab()->duration_font());
        hbb = item->tab()->duration_box_h();
        wbb = fm.width(item->text());
        xbb = 0.0;
        xpos = 0.0;
        ypos = item->tab()->duration_font_y_offset();
        ybb = item->tab()->duration_box_y() - ypos;
        // with rests, move symbol down by half its displacement from staff
        if (item->ownership_parent() && item->ownership_parent()->is_rest()) {
            ybb += tab_restsymbdispl.to_absolute(spatium);
            ypos += tab_restsymbdispl.to_absolute(spatium);
        }
    }
    // if on a chord with special beam mode, layout an 'English'-style duration grid
    else {
        const TablatureDurationFont &font = item->tab()->tab_duration_font();
        hbb = font.grid_stem_height.to_absolute(spatium); // bbox height is stem height
        wbb = font.grid_stem_width.to_absolute(spatium);  // bbox width is stem width
        xbb = -wbb * 0.5;                                 // bbox is half at left and half at right of stem centre
        ybb = -hbb;                                       // bbox top is at top of stem height
        xpos = 0.75 * spatium;                            // conventional centring of stem on fret marks
        ypos = item->tab()->duration_grid_y_offset();     // stem start is at bottom
        ldata->beam_level = beam_level;
        ldata->beam_grid = joins_previous ? TabBeamGrid::MEDIALFINAL : TabBeamGrid::INITIAL;
    }
    // set this' mag from parent chord mag (include staff mag)
    double mag = chord != nullptr ? chord->mag() : 1.0;
    ldata->set_mag(mag);
    mag = item->mag_s(); // local mag * score mag
                         // set magnified bbox and position
    ldata->set_bbox(xbb * mag, ybb * mag, wbb * mag, hbb * mag);
    ldata->set_pos(xpos * mag, ypos * mag);
}

void TLayout::layout_tab_duration_symbol2(const TabDurationSymbol *item, TabDurationSymbol::LayoutData *ldata) {
    if (ldata->beam_grid != TabBeamGrid::MEDIALFINAL) {
        return;
    }
    const ChordRest *previous = Navigation::prev_chord_rest(to_chord(item->ownership_parent()));
    if (!previous || !previous->tab_dur()) {
        return;
    }
    // Horizontal spacing is now final. Drawing applies the symbol magnification again.
    const double length = previous->tab_dur()->page_pos().x() - item->page_pos().x();
    ldata->beam_length = length / item->mag_s();
    RectF bbox = ldata->bbox();
    bbox.set_left(std::min(bbox.left(), length));
    ldata->set_bbox(bbox);
}

void TLayout::layout_tapping(Tapping *item, Tapping::LayoutData *ldata, LayoutContext &ctx) {
    TappingLayout::layout_tapping(item, ldata, ctx);
}

void TLayout::layout_tapping_half_slur([[maybe_unused]] TappingHalfSlur *item) {}

void TLayout::layout_tempo_text(const TempoText *item, TempoText::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    ldata->disconnect_snapped_items();

    TextLayout::layout_base_text_base(item, ldata);

    if (!item->autoplace()) {
        return;
    }

    // tempo text on first chordrest of measure should align over time sig if present, unless time sig is above staff
    Segment *s = item->segment();

    RehearsalMark *rehearsal_mark = to_rehearsal_mark(s->find_annotation(ElementType::REHEARSAL_MARK, item->track(), item->track()));
    RectF rehearsal_mark_bbox = rehearsal_mark ? rehearsal_mark->ldata()->bbox().translated(rehearsal_mark->pos()) : RectF();
    RectF this_bbox = ldata->bbox().translated(item->pos());

    if (s->rtick().is_zero()) {
        Segment *p = item->segment()->prev(SegmentType::TIME_SIG);
        if (p && !p->all_elements_invisible()) {
            ldata->move_x(-(s->x() - p->x()));
            EngravingItem *e = p->element(item->staff_idx() * ::mu::engraving::voices);
            if (e) {
                ldata->move_x(p->has_time_sig_above_staves() ? e->x() + e->width() + e->spatium() : e->x());
            }
        }
    }

    if (rehearsal_mark) {
        const bool same_side = item->place_above() == rehearsal_mark->place_above();
        const bool collision = item->place_above() ? muse::real_is_equal_or_more(rehearsal_mark_bbox.bottom(), this_bbox.top()) :
                                                     muse::real_is_equal_or_less(rehearsal_mark_bbox.top(), this_bbox.bottom());

        if (same_side && collision && item->get_property(Pid::TEMPO_ALIGN_RIGHT_OF_REHEARSAL_MARK).to_bool()) {
            double right_edge = rehearsal_mark_bbox.right();
            const double font_size_scale_factor = item->size() / 10.0;
            const double padding = 0.5 * item->spatium() * font_size_scale_factor;
            double cur_x = ldata->pos().x();
            ldata->set_pos_x(std::max(cur_x, right_edge + padding));
        }
    }
    Autoplace::autoplace_segment_element(item, ldata);
}

Shape TLayout::recalculate_text_line_base_segment_shape(const TextLineBaseSegment *item) {
    LAYOUT_CALL_ITEM(item);
    const TextLineBaseSegment::LayoutData *ldata = item->ldata();
    Shape shape;
    if (!item->text()->empty()) {
        shape.add(item->text()->ldata()->bbox().translated(item->text()->pos()), item->text());
    }
    if (!item->end_text()->empty()) {
        shape.add(item->end_text()->ldata()->bbox().translated(item->end_text()->pos()), item->end_text());
    }
    double lw2 = 0.5 * item->absolute_from_spatium(item->line_width());
    bool is_dotted_line = item->text_line_base()->line_style() == LineType::DOTTED;
    if (item->two_lines()) { // hairpins
        shape.add(TextLineBaseSegment::bounding_box_of_line(ldata->points[0], ldata->points[1], lw2, is_dotted_line), item);
        shape.add(TextLineBaseSegment::bounding_box_of_line(ldata->points[2], ldata->points[3], lw2, is_dotted_line), item);
    } else {
        for (int i = 0; i < ldata->npoints - 1; ++i) {
            shape.add(TextLineBaseSegment::bounding_box_of_line(ldata->points[i], ldata->points[i + 1], lw2, is_dotted_line), item,
                      !item->text_line_base()->line_visible());
        }
    }
    return shape;
}

void TLayout::layout_text(const Text *item, Text::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    if (item->layout_parent() && item->layout_to_parent_width()) {
        LD_CONDITION(item->layout_parent()->ldata()->is_set_bbox());
    }

    TextLayout::layout_base_text_base(item, ldata);
}

void TLayout::layout_text_line(TextLine *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(item, ctx);
}

void TLayout::layout_text_line_segment(TextLineSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    TextLineSegment::LayoutData *ldata = item->mutldata();
    layout_text_line_base_segment(item, ctx);

    Autoplace::autoplace_spanner_segment(item, ldata);
}

// Extends lines to fill the corner between them.
// Assumes that l1p2 == l2p1 is the intersection between the lines.
// If checkAngle is false, assumes that the lines are perpendicular,
// and some calculations are saved.
static inline void extend_lines(const PointF &l1p1, PointF &l1p2, PointF &l2p1, const PointF &l2p2, double line_width, bool check_angle) {
    PointF l1_unit_vector = (l1p2 - l1p1).normalized();
    PointF l2_unit_vector = (l2p1 - l2p2).normalized();

    double added_length = line_width * 0.5;

    if (check_angle) {
        double angle = M_PI - acos(PointF::dot_product(l1_unit_vector, l2_unit_vector));

        if (angle <= M_PI_2) {
            added_length *= tan(0.5 * angle);
        }
    }

    l1p2 += l1_unit_vector * added_length;
    l2p1 += l2_unit_vector * added_length;
}

static PolygonF create_arrow(bool start, bool filled, PointF &start_point, PointF &end_point, const TextLineBase *tl) {
    double arrow_width = 0.0;
    double arrow_height = 0.0;
    if (filled) {
        arrow_width = tl->absolute_from_spatium(start ? tl->begin_filled_arrow_width() : tl->end_filled_arrow_width());
        arrow_height = tl->absolute_from_spatium(start ? tl->begin_filled_arrow_height() : tl->end_filled_arrow_height());
    } else {
        arrow_width = tl->absolute_from_spatium(start ? tl->begin_line_arrow_width() : tl->end_line_arrow_width());
        arrow_height = tl->absolute_from_spatium(start ? tl->begin_line_arrow_height() : tl->end_line_arrow_height());
    }

    PolygonF arrow;
    if (start) {
        arrow << PointF(0.0, -arrow_height / 2) << PointF(-arrow_width, 0.0) << PointF(0.0, arrow_height / 2); // left
    } else {
        arrow << PointF(0.0, -arrow_height / 2) << PointF(arrow_width, 0.0) << PointF(0.0, arrow_height / 2); // right
    }

    PointF arrow_adjust = PointF(arrow_width, 0.0);
    arrow_adjust = (start ? 1.0 : -1.0) * arrow_adjust;
    arrow.translate(arrow_adjust);

    const double y_diff = end_point.y() - start_point.y();
    const double x_diff = end_point.x() - start_point.x();
    const double rotate = atan(y_diff / x_diff) + (end_point.x() < start_point.x() ? M_PI : 0.0);

    Transform t;
    t.rotate_radians(rotate);

    for (PointF &p : arrow) {
        p = t.map(p);
    }

    if (start) {
        arrow.translate(start_point);
    } else {
        arrow.translate(end_point);
    }

    const PointF line_vector = end_point - start_point;
    const PointF unit_vector = line_vector.normalized();
    const double reduction = filled ? arrow_width : tl->absolute_from_spatium(tl->line_width()) / 2;
    if (start) {
        start_point += reduction * unit_vector;
    } else {
        end_point -= reduction * unit_vector;
    }

    return arrow;
}

void TLayout::layout_text_line_base(TextLineBase *item, LayoutContext &ctx) { layout_line(item, ctx); }

void TLayout::layout_text_line_base_segment(TextLineBaseSegment *item, LayoutContext &ctx) {
    TextLineBaseSegment::LayoutData *ldata = item->mutldata();
    ldata->npoints = 0;
    const TextLineBase *tl = item->text_line_base();
    const double spatium = tl->spatium();
    const bool is_single_or_begin = item->is_single_begin_type();
    const bool is_single_or_end = item->is_single_end_type();
    const bool is_dotted_line = tl->line_style() == LineType::DOTTED;
    const double line_width = tl->absolute_from_spatium(tl->line_width());

    if (item->spanner()->place_below()) {
        ldata->set_pos_y(item->staff() ? item->staff()->staff_height(item->tick()) : 0.0);
    }

    // adjust Y pos to staffType offset
    ldata->move_y(item->staff_offset_y());

    if (!tl->diagonal()) {
        item->set_user_yoffset2(0);
    }

    auto align_base_line = [line_width](Text *text, PointF &pp1, PointF &pp2) {
        PointF width_correction(0.0, line_width / 2);
        switch (text->align().vertical) {
            case AlignV::TOP:
                pp1 += width_correction;
                pp2 += width_correction;
                break;
            case AlignV::VCENTER: break;
            case AlignV::BOTTOM:
                pp1 -= width_correction;
                pp2 -= width_correction;
                break;
            case AlignV::BASELINE:
                pp1 -= width_correction;
                pp2 -= width_correction;
                break;
        }
    };

    if (is_single_or_begin) {
        item->text()->set_xml_text(tl->begin_text());
        item->text()->set_family(tl->begin_font_family());
        item->text()->set_size(tl->begin_font_size());
        item->text()->set_symbol_scale(tl->begin_text_musical_symbols_scale());
        item->text()->set_symbol_size(tl->begin_text_music_symbols_size());
        item->text()->set_offset(tl->begin_text_offset() * item->mag());
        item->text()->set_align(tl->begin_text_align());
        item->text()->set_position(tl->begin_text_position());
        item->text()->set_font_style(tl->begin_font_style());
    } else {
        item->text()->set_xml_text(tl->continue_text());
        item->text()->set_family(tl->continue_font_family());
        item->text()->set_size(tl->continue_font_size());
        item->text()->set_symbol_scale(tl->continue_text_musical_symbols_scale());
        item->text()->set_symbol_size(tl->continue_text_music_symbols_size());
        item->text()->set_offset(tl->continue_text_offset() * item->mag());
        item->text()->set_align(tl->continue_text_align());
        item->text()->set_position(tl->continue_text_position());
        item->text()->set_font_style(tl->continue_font_style());
    }
    item->text()->set_placement(PlacementV::ABOVE);
    item->text()->set_track(item->track());
    item->text()->set_color(tl->line_color());
    layout_text(item->text(), item->text()->mutldata());

    if (is_single_or_end) {
        item->end_text()->set_xml_text(tl->end_text());
        item->end_text()->set_family(tl->end_font_family());
        item->end_text()->set_size(tl->end_font_size());
        item->end_text()->set_symbol_scale(tl->end_text_musical_symbols_scale());
        item->end_text()->set_symbol_size(tl->end_text_music_symbols_size());
        item->end_text()->set_offset(tl->end_text_offset() * item->mag());
        item->end_text()->set_align(tl->end_text_align());
        item->end_text()->set_position(tl->end_text_position());
        item->end_text()->set_font_style(tl->end_font_style());
        item->end_text()->set_placement(PlacementV::ABOVE);
        item->end_text()->set_track(item->track());
        item->end_text()->set_color(tl->line_color());
        layout_text(item->end_text(), item->end_text()->mutldata());
    } else {
        item->end_text()->set_xml_text(u"");
    }

    if (!item->text_line_base()->text_size_spatium_dependent()) {
        item->text()->set_size(item->text()->size() * item->default_spatium() / item->spatium());
        item->end_text()->set_size(item->end_text()->size() * item->default_spatium() / item->spatium());
    }

    PointF default_pos = tl->default_pos();
    ldata->move(default_pos);

    PointF pp1;
    PointF pp2(item->pos2());

    // line with no text or hooks - just use the basic rectangle for line
    if (item->text()->empty() && item->end_text()->empty() && (!is_single_or_begin || tl->begin_hook_type() == HookType::NONE) &&
        (!is_single_or_end || tl->end_hook_type() == HookType::NONE)) {
        ldata->npoints = 2;
        ldata->points[0] = pp1;
        ldata->points[1] = pp2;
        ldata->line_length = sqrt(PointF::dot_product(pp2 - pp1, pp2 - pp1));

        item->setbbox(TextLineBaseSegment::bounding_box_of_line(pp1, pp2, line_width / 2, is_dotted_line));
        return;
    }

    double l1 = 0.0;
    double l2 = 0.0;
    const double gap_between_text_and_line = spatium * tl->gap_between_text_and_line().val();

    const bool align_begin_text = tl->begin_text_place() == TextPlace::LEFT || tl->begin_text_place() == TextPlace::AUTO;
    const bool align_continue_text = tl->continue_text_place() == TextPlace::LEFT || tl->continue_text_place() == TextPlace::AUTO;
    const bool align_end_text = tl->end_text_place() == TextPlace::LEFT || tl->end_text_place() == TextPlace::AUTO;
    const bool has_begin_text = !item->text()->empty() && is_single_or_begin;
    const bool has_continue_text = !item->text()->empty() && !is_single_or_begin;
    const bool has_end_text = !item->end_text()->empty() && is_single_or_end;

    Shape shape;

    if (!item->text()->empty()) {
        if ((is_single_or_begin && align_begin_text) || (!is_single_or_begin && align_continue_text)) {
            l1 = gap_between_text_and_line;
            switch (item->text()->position()) {
                case AlignH::LEFT: l1 += item->text()->ldata()->bbox().width(); break;
                case AlignH::HCENTER: l1 += item->text()->ldata()->bbox().width() / 2; break;
                default: break;
            }
        }
        shape.add(item->text()->ldata()->bbox().translated(item->text()->pos()), item);
    }
    // set end text position and extend bbox
    if (!item->end_text()->empty()) {
        if (align_end_text) {
            l2 = gap_between_text_and_line;
            switch (item->end_text()->position()) {
                case AlignH::RIGHT: l2 += item->end_text()->ldata()->bbox().width(); break;
                case AlignH::HCENTER: l2 += item->end_text()->ldata()->bbox().width() / 2; break;
                default: break;
            }
        }
        const double end_text_x = std::max(pp2.x(), shape.bbox().right() + l2); // prevent end text from overlapping begin text
        item->end_text()->mutldata()->set_pos_x(end_text_x);
        shape.add(item->end_text()->ldata()->bbox().translated(item->end_text()->pos()), item);
    }

    bool begin_hook_drawn_separately = false;
    bool end_hook_drawn_separately = false;

    if (tl->line_visible() || !ctx.conf().is_printing_mode()) {
        pp1.rx() += l1;
        pp2.rx() -= l2;

        // Make sure baseline of text and line are properly aligned (accounting for line thickness)
        if ((has_begin_text && align_begin_text) || (has_continue_text && align_continue_text)) {
            align_base_line(item->text(), pp1, pp2);
        } else if (has_end_text && align_end_text) {
            align_base_line(item->end_text(), pp1, pp2);
        }

        const double begin_hook_height = (tl->place_below() ? -1.0 : 1.0) * tl->begin_hook_height().val() * spatium;
        const double end_hook_height = (tl->place_below() ? -1.0 : 1.0) * tl->end_hook_height().val() * spatium;

        double begin_hook_width = 0.0;
        double end_hook_width = 0.0;

        if (tl->begin_hook_type() == HookType::HOOK_45) {
            begin_hook_width = std::fabs(begin_hook_height * .4);
            pp1.rx() += begin_hook_width;
        }

        if (tl->end_hook_type() == HookType::HOOK_45) {
            end_hook_width = std::fabs(end_hook_height * .4);
            pp2.rx() -= end_hook_width;
        }

        // don't draw backwards lines (or hooks) if text is longer than nominal line length
        if (!item->text()->empty() && pp1.x() > pp2.x() && !tl->diagonal()) {
            ldata->set_shape(shape);
            return;
        }

        const bool begin_arrow =
            is_single_or_begin && (tl->begin_hook_type() == HookType::ARROW_FILLED || tl->begin_hook_type() == HookType::ARROW);
        const bool end_arrow =
            is_single_or_end && (tl->end_hook_type() == HookType::ARROW_FILLED || tl->end_hook_type() == HookType::ARROW);

        if (begin_arrow) {
            const bool filled = tl->begin_hook_type() == HookType::ARROW_FILLED;
            ldata->begin_arrow = create_arrow(true, filled, pp1, pp2, tl);
            shape.add(ldata->begin_arrow.bounding_rect(), item);
        }
        if (end_arrow) {
            const bool filled = tl->end_hook_type() == HookType::ARROW_FILLED;
            ldata->end_arrow = create_arrow(false, filled, pp1, pp2, tl);
            shape.add(ldata->end_arrow.bounding_rect(), item);
        }

        auto has_hook = [](HookType type) -> bool {
            switch (type) {
                case HookType::HOOK_45:
                case HookType::HOOK_90:
                case HookType::HOOK_90T: return true;
                case HookType::NONE:
                case HookType::ARROW:
                case HookType::ARROW_FILLED:
                case HookType::ROSETTE: return false;
                default: break;
            }
            return false;
        };

        const bool begin_hook = is_single_or_begin && has_hook(tl->begin_hook_type());
        const bool end_hook = is_single_or_end && has_hook(tl->end_hook_type());

        if (begin_hook) {
            // We use the term "endpoint" for the point that does not touch the main line.
            const PointF &begin_hook_endpoint = ldata->points[ldata->npoints++] =
                PointF(pp1.x() - begin_hook_width, pp1.y() + begin_hook_height);
            if (tl->begin_hook_type() == HookType::HOOK_90T) {
                // A T-hook needs to be drawn separately, so we add an extra point
                begin_hook_drawn_separately = true;
                ldata->points[ldata->npoints++] = PointF(pp1.x() - begin_hook_width, pp1.y() - begin_hook_height);
            } else if (tl->line_style() != LineType::SOLID) {
                // For non-solid lines, we also draw the hook separately,
                // so that we can distribute the dashes/dots for each linepiece individually
                begin_hook_drawn_separately = true;
                PointF &begin_hook_startpoint = ldata->points[ldata->npoints++] = pp1;

                if (tl->line_style() == LineType::DASHED) {
                    // For dashes lines, we extend the lines somewhat,
                    // so that the corner between them gets filled
                    bool check_angle = tl->begin_hook_type() == HookType::HOOK_45 || tl->diagonal();
                    extend_lines(begin_hook_endpoint, begin_hook_startpoint, pp1, pp2, line_width, check_angle);
                }
            }
        }

        ldata->points[ldata->npoints++] = pp1;
        PointF &pp22 = ldata->points[ldata->npoints++] = pp2; // Keep a reference so that we can modify later

        if (end_hook) {
            const PointF end_hook_endpoint = PointF(pp2.x() + end_hook_width, pp2.y() + end_hook_height);
            if (tl->end_hook_type() == HookType::HOOK_90T) {
                // A T-hook needs to be drawn separately, so we add an extra point
                end_hook_drawn_separately = true;
                ldata->points[ldata->npoints++] = PointF(pp2.x() + end_hook_width, pp2.y() - end_hook_height);
            } else if (tl->line_style() != LineType::SOLID) {
                // For non-solid lines, we also draw the hook separately,
                // so that we can distribute the dashes/dots for each linepiece individually
                end_hook_drawn_separately = true;
                PointF &end_hook_startpoint = ldata->points[ldata->npoints++] = pp2;

                if (tl->line_style() == LineType::DASHED) {
                    // For dashes lines, we extend the lines somewhat,
                    // so that the corner between them gets filled
                    bool check_angle = tl->end_hook_type() == HookType::HOOK_45 || tl->diagonal();
                    extend_lines(pp1, pp22, end_hook_startpoint, end_hook_endpoint, line_width, check_angle);
                }
            }

            ldata->points[ldata->npoints++] = end_hook_endpoint;
        }

        ldata->line_length = sqrt(PointF::dot_product(pp22 - pp1, pp22 - pp1));
    }

    // Calculate shape of line
    {
        int start = 0;
        if (begin_hook_drawn_separately) {
            PointF &p1 = ldata->points[start++];
            PointF &p2 = ldata->points[start++];
            shape.add(TextLineBaseSegment::bounding_box_of_line(p1, p2, line_width / 2, is_dotted_line), item);
        }

        int end = ldata->npoints;
        if (end_hook_drawn_separately) {
            PointF &p1 = ldata->points[--end];
            PointF &p2 = ldata->points[--end];
            shape.add(TextLineBaseSegment::bounding_box_of_line(p1, p2, line_width / 2, is_dotted_line), item);
        }

        for (int i = start; i < end - 1; ++i) {
            shape.add(TextLineBaseSegment::bounding_box_of_line(ldata->points[i], ldata->points[i + 1], line_width / 2, is_dotted_line),
                      item, !tl->line_visible());
        }
    }

    ldata->set_shape(shape);
}

void TLayout::layout_tie([[maybe_unused]] Tie *item, LayoutContext &) {}

void TLayout::layout_time_sig(const TimeSig *item, TimeSig::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    ldata->set_pos(0.0, 0.0);
    double spatium = item->spatium();

    ldata->set_bbox(RectF()); // prepare for an empty time signature

    ldata->point_large_left_paren = PointF();
    ldata->pz = PointF();
    ldata->pn = PointF();
    ldata->point_large_right_paren = PointF();

    const MStyle &style = item->style();
    TimeSigPlacement time_sig_placement = style.style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>();
    if (time_sig_placement == TimeSigPlacement::NORMAL) {
        const_cast<TimeSig *>(item)->set_system_flag(false);
    } else {
        const_cast<TimeSig *>(item)->set_system_flag(true);
    }

    double line_dist = 0.0;
    int num_of_lines = 0;
    TimeSigType sig_type = item->time_sig_type();
    const Staff *staff = item->staff();
    const Segment *seg = item->segment();
    const Measure *meas = seg ? seg->measure() : nullptr;
    const Fraction tick = meas ? meas->tick() : item->tick();

    if (staff) {
        // if staff is without time sig, format as if no text at all
        const StaffType *st_visibility = staff->staff_type(item->tick());
        if (!st_visibility->gen_timesig()) {
            // reset position and box sizes to 0
            // LOGD("staff: no time sig");
            ldata->point_large_left_paren.rx() = 0.0;
            ldata->pn.rx() = 0.0;
            ldata->pz.rx() = 0.0;
            ldata->point_large_right_paren.rx() = 0.0;
            ldata->set_bbox(RectF());
            // leave everything else as it is:
            // draw() will anyway skip any drawing if staff type has no time sigs
            return;
        }
        num_of_lines = staff->lines(tick);
        line_dist = staff->line_distance(tick);
    } else {
        // assume dimensions of a standard staff
        line_dist = 1.0;
        num_of_lines = 5;
    }

    // if some symbol
    // compute vert. displacement to center in the staff height
    // determine middle staff position:

    double yoff = spatium * (num_of_lines - 1) * .5 * line_dist;

    // C and Ccut are placed at the middle of the staff: use yoff directly
    IEngravingFontPtr font = ctx.engraving_font();
    ScaleF scale = item->scale();
    SizeF mag(item->mag_s() * scale);
    TimeSigStyle time_sig_style = item->time_sig_style();

    double num_dist = item->num_dist() * scale.height();

    Shape shape;
    if (sig_type == TimeSigType::FOUR_FOUR) {
        ldata->pz = PointF(0.0, yoff);
        SymId sym = time_sig_style == TimeSigStyle::LARGE  ? SymId::TIME_SIG_COMMON_LARGE :
                    time_sig_style == TimeSigStyle::NARROW ? SymId::TIME_SIG_COMMON_NARROW :
                                                             SymId::TIME_SIG_COMMON;
        RectF bbox = font->bbox(sym, mag);
        shape.add(bbox.translated(ldata->pz), item);
        ldata->ns.clear();
        ldata->ns.push_back(sym);
        ldata->ds.clear();
    } else if (sig_type == TimeSigType::ALLA_BREVE) {
        ldata->pz = PointF(0.0, yoff);
        SymId sym = time_sig_style == TimeSigStyle::LARGE  ? SymId::TIME_SIG_CUT_COMMON_LARGE :
                    time_sig_style == TimeSigStyle::NARROW ? SymId::TIME_SIG_CUT_COMMON_NARROW :
                                                             SymId::TIME_SIG_CUT_COMMON;
        RectF bbox = font->bbox(sym, mag);
        shape.add(bbox.translated(ldata->pz), item);
        ldata->ns.clear();
        ldata->ns.push_back(sym);
        ldata->ds.clear();
    } else if (sig_type == TimeSigType::CUT_BACH) {
        ldata->pz = PointF(0.0, yoff);
        SymId sym = time_sig_style == TimeSigStyle::LARGE  ? SymId::TIME_SIG_CUT2_LARGE :
                    time_sig_style == TimeSigStyle::NARROW ? SymId::TIME_SIG_CUT2_NARROW :
                                                             SymId::TIME_SIG_CUT2;
        RectF bbox = font->bbox(sym, mag);
        shape.add(bbox.translated(ldata->pz), item);
        ldata->ns.clear();
        ldata->ns.push_back(sym);
        ldata->ds.clear();
    } else if (sig_type == TimeSigType::CUT_TRIPLE) {
        ldata->pz = PointF(0.0, yoff);
        SymId sym = time_sig_style == TimeSigStyle::LARGE  ? SymId::TIME_SIG_CUT3_LARGE :
                    time_sig_style == TimeSigStyle::NARROW ? SymId::TIME_SIG_CUT3_NARROW :
                                                             SymId::TIME_SIG_CUT3;
        RectF bbox = font->bbox(sym, mag);
        shape.add(bbox.translated(ldata->pz), item);
        ldata->ns.clear();
        ldata->ns.push_back(sym);
        ldata->ds.clear();
    } else {
        if (item->numerator_string().is_empty()) {
            ldata->ns = time_sig_sym_ids_from_string(
                item->numerator_string().is_empty() ? String::number(item->sig().numerator()) : item->numerator_string(), time_sig_style);

            ldata->ds = time_sig_sym_ids_from_string(item->denominator_string().is_empty() ? String::number(item->sig().denominator()) :
                                                                                             item->denominator_string(),
                                                     time_sig_style);
        } else {
            ldata->ns = time_sig_sym_ids_from_string(item->numerator_string(), time_sig_style);
            ldata->ds = time_sig_sym_ids_from_string(item->denominator_string(), time_sig_style);
        }

        RectF num_rect = font->bbox(ldata->ns, mag);
        RectF den_rect = font->bbox(ldata->ds, mag);

        double pz_y = yoff - (den_rect.width() < 0.01 ? 0.0 : 0.5 * (num_rect.height() + num_dist));
        double pn_y = yoff + 0.5 * (den_rect.height() + num_dist);

        if (num_rect.width() >= den_rect.width()) {
            // numerator: one space above centre line, unless denomin. is empty (if so, directly centre in the middle)
            ldata->pz = PointF(0.0, pz_y);
            // denominator: horiz: centred around centre of numerator | vert: one space below centre line
            ldata->pn = PointF((num_rect.width() - den_rect.width()) * .5, pn_y);
        } else {
            // numerator: one space above centre line, unless denomin. is empty (if so, directly centre in the middle)
            ldata->pz = PointF((den_rect.width() - num_rect.width()) * .5, pz_y);
            // denominator: horiz: centred around centre of numerator | vert: one space below centre line
            ldata->pn = PointF(0.0, pn_y);
        }

        // centering of parenthesis so the middle of the parenthesis is at the divisor marking level
        int center_y = yoff / 2 + spatium;
        int widest_portion = num_rect.width() > den_rect.width() ? num_rect.width() : den_rect.width();
        ldata->point_large_left_paren = PointF(-spatium, center_y);
        ldata->point_large_right_paren = PointF(widest_portion + spatium, center_y);

        shape.add(num_rect.translated(ldata->pz), item); // translate bounding boxes to actual string positions
        shape.add(den_rect.translated(ldata->pn), item);
        if (item->large_parentheses()) {
            shape.add(RectF(ldata->point_large_left_paren.x(), ldata->point_large_left_paren.y() - den_rect.height(), spatium / 2,
                            num_rect.height() + den_rect.height()),
                      item);
            shape.add(RectF(ldata->point_large_right_paren.x(), ldata->point_large_right_paren.y() - den_rect.height(), spatium / 2,
                            num_rect.height() + den_rect.height()),
                      item);
        }
    }

    ldata->set_shape(shape);

    ldata->set_pos_x(-shape.bbox().left());

    if (item->is_above_staves()) {
        if (staff->has_system_objects_below_bottom_staff()) {
            double staff_height = spatium * (num_of_lines - 1) * line_dist;
            ldata->set_pos_y(staff_height - ldata->bbox().top());
        } else {
            ldata->set_pos_y(-ldata->bbox().bottom());
        }
    } else if (item->is_across_staves()) {
        double top = ldata->bbox().top();
        ldata->set_pos_y(-top);
    }

    ldata->move_y(item->y_pos());
}

void TLayout::layout_time_tick_anchor(TimeTickAnchor *item, LayoutContext &) {
    TimeTickAnchor::LayoutData *ldata = item->mutldata();
    const StaffType *st = item->staff_type();
    ldata->set_pos(0.0, 0.0);
    double width = item->segment()->width();
    double height = item->staff()->staff_height();
    if (st && st->lines() == 1) {
        // Special case for single line staves
        const double line_dist = st->line_distance().val() * item->spatium();
        ldata->set_pos_y(-line_dist);
        height = line_dist * 2;
    }
    ldata->set_bbox(0.0, 0.0, width, height);
}

void TLayout::layout_tremolo_single(TremoloSingleChord *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    TremoloLayout::layout(item, ctx);
}

void TLayout::layout_tremolo_two(TremoloTwoChord *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    TremoloLayout::layout(item, ctx);
}

void TLayout::layout_tremolo_bar(const TremoloBar *item, TremoloBar::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    LD_INDEPENDENT;

    double spatium = item->spatium();

    ldata->set_pos(0.0, -spatium * 3.0);

    /* we place the tremolo bars starting slightly before the
     *  notehead, and end it slightly after, drawing above the
     *  note. The values specified in Guitar Pro are very large, too
     *  large for the scale used in Musescore. We used the
     *  timeFactor and pitchFactor below to reduce these values down
     *  consistently to values that make sense to draw with the
     *  Musescore scale. */

    double time_factor = item->user_mag() / 1.0;
    double pitch_factor = -spatium * .02;

    PolygonF polygon;
    for (const PitchValue &v : item->points()) {
        polygon << PointF(v.time * time_factor, v.pitch * pitch_factor);
    }
    ldata->polygon = polygon;

    const double w = item->absolute_from_spatium(item->line_width());
    ldata->set_bbox(ldata->polygon.bounding_rect().adjusted(-w, -w, w, w));
}

void TLayout::layout_trill_segment(TrillSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    TrillSegment::LayoutData *ldata = item->mutldata();
    Trill *trill = item->trill();
    EngravingItem *start_item = trill->start_element();
    Chord *start_chord = start_item && start_item->is_chord() ? to_chord(start_item) : nullptr;
    if (start_chord) {
        // Semi-hack: spanners don't have staffMove property, so we change
        // the staffIdx itself to follow cross-staff chords if needed
        item->set_staff_idx(start_chord->v_staff_idx());
    }

    if (item->staff()) {
        ldata->set_mag(item->staff()->staff_mag(item->tick()));
    }
    if (item->spanner()->place_below()) {
        ldata->set_pos_y(item->staff() ? item->staff()->staff_height(item->tick()) : 0.0);
    }
    const double y_off = item->staff_offset_y();
    ldata->move_y(y_off);

    PointF default_pos = item->default_pos();
    ldata->move(default_pos);

    bool accidental_goes_below = trill->trill_type() == TrillType::DOWNPRALL_LINE;
    Ornament *ornament = trill->ornament();
    if (ornament) {
        if (item->is_single_begin_type()) {
            TLayout::layout_ornament(ornament, ornament->mutldata(), ctx.conf());
        }
        trill->set_accidental(accidental_goes_below ? ornament->accidental_below() : ornament->accidental_above());
        trill->set_cue_note_chord(ornament->cue_note_chord());
        ArticulationAnchor anchor = ornament->anchor();
        if (anchor == ArticulationAnchor::AUTO) {
            trill->set_placement(trill->track() % 2 ? PlacementV::BELOW : PlacementV::ABOVE);
        } else {
            trill->set_placement(anchor == ArticulationAnchor::TOP ? PlacementV::ABOVE : PlacementV::BELOW);
        }
        trill->set_property_flags(Pid::PLACEMENT,
                                  PropertyFlags::STYLED); // Ensures that the property isn't written (it is written by the ornamnent)
    }

    if (item->is_single_type() || item->is_begin_type()) {
        switch (trill->trill_type()) {
            case TrillType::TRILL_LINE: item->symbol_line(SymId::ORNAMENT_TRILL, SymId::WIGGLE_TRILL); break;
            case TrillType::PRALLPRALL_LINE: item->symbol_line(SymId::WIGGLE_TRILL, SymId::WIGGLE_TRILL); break;
            case TrillType::UPPRALL_LINE:
                item->symbol_line(SymId::ORNAMENT_BOTTOM_LEFT_CONCAVE_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
                                  SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END);
                break;
            case TrillType::DOWNPRALL_LINE:
                item->symbol_line(SymId::ORNAMENT_LEFT_VERTICAL_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
                                  SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END);
                break;
        }
        Accidental *a = trill->accidental();
        if (a) {
            double vert_margin = 0.35 * item->spatium();
            RectF box = item->sym_bbox(item->symbols().front());
            double x = 0;
            double y = 0;
            x = 0.5 * (box.width() - a->width());
            double min_vert_dist =
                accidental_goes_below ? Shape(box).min_vertical_distance(a->shape()) : a->shape().min_vertical_distance(Shape(box));
            y = (accidental_goes_below ? min_vert_dist + vert_margin : -min_vert_dist - vert_margin) + y_off;
            a->set_pos(x, y);
            a->set_ownership_parent(item);
        }
    } else {
        switch (trill->trill_type()) {
            case TrillType::TRILL_LINE:
            case TrillType::PRALLPRALL_LINE: item->symbol_line(SymId::WIGGLE_TRILL, SymId::WIGGLE_TRILL); break;
            case TrillType::UPPRALL_LINE:
            case TrillType::DOWNPRALL_LINE:
                item->symbol_line(SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
                                  SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END);
                break;
        }
    }

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::fill_trill_segment_shape(const TrillSegment *item, TrillSegment::LayoutData *ldata, const LayoutConfiguration &conf) {
    LAYOUT_CALL_ITEM(item);
    std::shared_ptr<const IEngravingFont> font = conf.engraving_font();
    Shape s = font->shape(item->symbols(), item->mag_s());
    Accidental *accidental = item->trill()->accidental();
    if (accidental && accidental->visible() && item->is_single_begin_type()) {
        s.add(accidental->shape().translate(accidental->pos()));
    }

    ldata->set_shape(s);
}

void TLayout::layout_triplet_feel(const TripletFeel *item, TripletFeel::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    layout_system_text(item, ldata);
}

void TLayout::layout_trill(Trill *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(static_cast<SLine *>(item), ctx);
}

void TLayout::layout_tuplet(Tuplet *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    TupletLayout::layout(item, ctx);
}

void TLayout::fill_tuplet_shape(const Tuplet *item, Tuplet::LayoutData *ldata) {
    LAYOUT_CALL_ITEM(item);
    Shape s;
    if (item->has_bracket()) {
        auto tuplet_rect = [](const PointF &p1, const PointF &p2, double height, const Tuplet *item) {
            Shape shape;
            double x_length = p2.x() - p1.x();
            int n_steps = 2 * std::ceil(x_length / item->spatium());
            double x_step = x_length / n_steps;
            double y_step = (p2.y() - p1.y()) / n_steps;
            double rect_height = (item->is_up() ? height : -height) - y_step;
            double x_cur = p1.x();
            double y_cur = p1.y() + y_step;
            for (int i = 0; i < n_steps; ++i) {
                shape.add(RectF(x_cur, y_cur, x_step, rect_height).normalized(), item);
                x_cur += x_step;
                y_cur += y_step;
            }
            return shape;
        };

        double height = item->style().style_absolute(Sid::TUPLET_BRACKET_HOOK_HEIGHT);
        s.add(tuplet_rect(item->bracket_l[0], item->bracket_l[1], height, item));
        s.add(tuplet_rect(item->bracket_l[1], item->bracket_l[2], height, item));
        if (item->number()) {
            s.add(tuplet_rect(item->bracket_r[0], item->bracket_r[1], height, item));
            s.add(tuplet_rect(item->bracket_r[1], item->bracket_r[2], height, item));
        } else {
            s.add(tuplet_rect(item->bracket_l[2], item->bracket_l[3], height, item));
        }
    }
    if (item->number()) {
        s.add(item->number()->ldata()->bbox().translated(item->number()->pos()));
    }

    ldata->set_shape(s);
}

void TLayout::layout_vibrato_segment(VibratoSegment *item, LayoutContext &) {
    LAYOUT_CALL_ITEM(item);
    VibratoSegment::LayoutData *ldata = item->mutldata();
    if (item->staff()) {
        ldata->set_mag(item->staff()->staff_mag(item->tick()));
    }
    if (item->spanner()->place_below()) {
        ldata->set_pos_y(item->staff() ? item->staff()->staff_height(item->tick()) : 0.0);
    }

    PointF default_pos = item->default_pos();
    ldata->move(default_pos);

    ldata->move_y(item->staff_offset_y());

    switch (item->vibrato()->vibrato_type()) {
        case VibratoType::GUITAR_VIBRATO: item->symbol_line(SymId::GUITAR_VIBRATO_STROKE, SymId::GUITAR_VIBRATO_STROKE); break;
        case VibratoType::GUITAR_VIBRATO_WIDE:
            item->symbol_line(SymId::GUITAR_WIDE_VIBRATO_STROKE, SymId::GUITAR_WIDE_VIBRATO_STROKE);
            break;
        case VibratoType::VIBRATO_SAWTOOTH: item->symbol_line(SymId::WIGGLE_SAWTOOTH, SymId::WIGGLE_SAWTOOTH); break;
        case VibratoType::VIBRATO_SAWTOOTH_WIDE: item->symbol_line(SymId::WIGGLE_SAWTOOTH_WIDE, SymId::WIGGLE_SAWTOOTH_WIDE); break;
        default: break;
    }

    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_vibrato(Vibrato *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(static_cast<SLine *>(item), ctx);

    if (item->spanner_segments().empty()) {
        LOGD("Vibrato: no segments");
        return;
    }
}

void TLayout::layout_volta(Volta *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    layout_line(item, ctx);
}

void TLayout::layout_volta_segment(VoltaSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    VoltaSegment::LayoutData *ldata = item->mutldata();
    layout_text_line_base_segment(item, ctx);
    if (item->place_below()) {
        if (TextBase *begin_text = item->text()) {
            PointF offset = begin_text->offset();
            offset.set_y(-offset.y() + begin_text->ldata()->bbox().height());
            begin_text->set_offset(offset);
        }
        if (TextBase *end_text = item->end_text()) {
            PointF offset = end_text->offset();
            offset.set_y(-offset.y() + end_text->ldata()->bbox().height());
            end_text->set_offset(offset);
        }
        // recalculate shape after moving texts
        Shape sh = recalculate_text_line_base_segment_shape(item);
        ldata->set_shape(sh);
    }
    Autoplace::autoplace_spanner_segment(item, ldata);
}

void TLayout::layout_whammy_bar_segment(WhammyBarSegment *item, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    WhammyBarSegment::LayoutData *ldata = item->mutldata();
    layout_text_line_base_segment(item, ctx);

    Autoplace::autoplace_spanner_segment(item, ldata);
}

using LayoutSystemTypes = rtti::TypeList<LyricsLine, Slur, HammerOnPullOff, TappingHalfSlur, Volta>;

struct LayoutSystemVisitor : rtti::Visitor<LayoutSystemVisitor> {
    template <typename T>
    static bool do_visit(Spanner *item, System *system, LayoutContext &ctx, SpannerSegment **seg_out) {
        if (T::classof(item)) {
            *seg_out = TLayout::layout_system(static_cast<T *>(item), system, ctx);
            return true;
        }
        return false;
    }
};

SpannerSegment *TLayout::layout_system(Spanner *item, System *system, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(item);
    SpannerSegment *seg = nullptr;
    bool found = LayoutSystemVisitor::visit(LayoutSystemTypes{}, item, system, ctx, &seg);
    if (!found) {
        SLine *line = dynamic_cast<SLine *>(item);
        if (line) {
            seg = layout_system_s_line(line, system, ctx);
        } else {
            DO_ASSERT(found);
        }
    }
    return seg;
}

SpannerSegment *TLayout::get_next_layout_system_segment(Spanner *spanner, System *system,
                                                        std::function<SpannerSegment *()> create_segment) {
    // Prefer a segment which has already been added to the system
    SpannerSegment *seg = nullptr;
    SpannerSegment *detached = nullptr;
    for (SpannerSegment *ss : spanner->spanner_segments()) {
        if (ss->system() == system) {
            seg = ss;
            break;
        }
        if (!detached && !ss->system()) {
            detached = ss;
        }
    }
    if (!seg) {
        seg = detached;
    }

    if (!seg) {
        if ((seg = spanner->pop_unused_segment())) {
            spanner->reuse(seg);
        } else {
            seg = create_segment();
            assert(seg);
            spanner->add(seg);
        }
    }
    seg->move_to_system(system);
    seg->set_spanner(spanner);
    seg->set_track(spanner->track());
    seg->set_visible(spanner->visible());
    return seg;
}

// layout spannersegment for system
SpannerSegment *TLayout::layout_system_s_line(SLine *line, System *system, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(line);
    Fraction stick = system->first_measure()->tick();
    Fraction etick = system->last_measure()->end_tick();

    LineSegment *line_segm =
        to_line_segment(TLayout::get_next_layout_system_segment(line, system, [line]() { return line->create_line_segment(); }));

    SpannerSegmentType sst;
    if (line->tick() >= stick) {
        //
        // this is the first call to layoutSystem,
        // processing the first line segment
        //
        line->compute_start_element();
        line->compute_end_element();
        if (line->tick2() < etick) {
            sst = SpannerSegmentType::SINGLE;
        } else if (line->tick2() == etick) {
            if (line->is_pedal() && to_pedal(line)->connect45_hook_to_next()) {
                sst = SpannerSegmentType::BEGIN;
            } else {
                sst = SpannerSegmentType::SINGLE;
            }
        } else {
            sst = SpannerSegmentType::BEGIN;
        }
        if (line->has_voice_assignment_properties()) {
            line->set_placement_based_on_voice_assignment(
                ctx.conf().style_v(Sid::DYNAMICS_HAIRPIN_VOICE_BASED_PLACEMENT).value<DirectionV>());
        }
    } else if (line->tick() < stick &&
               (line->tick2() > etick || (line->tick2() == etick && line->is_pedal() && to_pedal(line)->connect45_hook_to_next()))) {
        sst = SpannerSegmentType::MIDDLE;
    } else {
        //
        // this is the last call to layoutSystem
        // processing the last line segment
        //
        sst = SpannerSegmentType::END;
    }
    line_segm->set_spanner_segment_type(sst);
    line_segm->set_placement(line->placement());

    switch (sst) {
        case SpannerSegmentType::SINGLE: {
            System *s;
            PointF p1 = line->line_pos(Grip::START, &s);
            PointF p2 = line->line_pos(Grip::END, &s);
            double len = p2.x() - p1.x();
            line_segm->set_pos(p1);
            line_segm->set_pos2(PointF(len, p2.y() - p1.y()));
        } break;
        case SpannerSegmentType::BEGIN: {
            System *s;
            PointF p1 = line->line_pos(Grip::START, &s);
            line_segm->set_pos(p1);
            double x2 = system->ending_x_for_open_ended_lines();
            line_segm->set_pos2(PointF(x2 - p1.x(), 0.0));
        } break;
        case SpannerSegmentType::MIDDLE: {
            double x1 =
                line->is_volta() ? volta_mid_end_segment_start_x(to_volta(line), system, ctx) : system->first_note_rest_segment_x(true);
            double x2 = system->ending_x_for_open_ended_lines();
            System *s;
            PointF p1 = line->line_pos(Grip::START, &s);
            line_segm->set_pos(PointF(x1, p1.y()));
            line_segm->set_pos2(PointF(x2 - x1, 0.0));
        } break;
        case SpannerSegmentType::END: {
            System *s;
            PointF p2 = line->line_pos(Grip::END, &s);
            double x1 =
                line->is_volta() ? volta_mid_end_segment_start_x(to_volta(line), system, ctx) : system->first_note_rest_segment_x(true);
            double len = p2.x() - x1;
            line_segm->set_pos(PointF(p2.x() - len, p2.y()));
            line_segm->set_pos2(PointF(len, 0.0));
        } break;
    }

    layout_line_segment(line_segm, ctx);

    return line_segm;
}

double TLayout::volta_mid_end_segment_start_x(Volta *volta, System *system, LayoutContext &ctx) {
    Measure *first_measure = system->first_measure();
    if (!first_measure) {
        return 0.0;
    }

    Segment *ref_seg = first_measure->repeat_start() ? first_measure->first(SegmentType::START_REPEAT_BAR_LINE) :
                       ctx.conf().style_b(Sid::VOLTA_ALIGN_START_BEFORE_KEY_SIG) ? first_measure->first(SegmentType::KEY_SIG) :
                                                                                   nullptr;

    if (!ref_seg) {
        return system->first_note_rest_segment_x(true);
    }

    if (ref_seg->is_start_repeat_bar_line_type()) {
        return ref_seg->x() + first_measure->x();
    }

    KeySig *key_sig = to_key_sig(ref_seg->element(volta->track()));
    if (!key_sig || key_sig->ldata()->key_symbols.empty()) {
        return 0.0;
    }

    KeySym key_sym = key_sig->ldata()->key_symbols.front();
    double x_cutout = ctx.engraving_font()->smufl_anchor(key_sym.sym, SmuflAnchorId::CUT_OUT_NW, 1.0).x();
    return key_sig->x() + key_sym.x_pos.val() + x_cutout + ref_seg->x() + first_measure->x();
}

SpannerSegment *TLayout::layout_system(LyricsLine *line, System *system, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(line);
    if (!line->is_partial_lyrics_line() && !line->lyrics()) {
        return nullptr;
    }

    Fraction stick = system->first_measure()->tick();
    Fraction etick = system->last_measure()->end_tick();

    LyricsLineSegment *line_segm =
        to_lyrics_line_segment(TLayout::get_next_layout_system_segment(line, system, [line]() { return line->create_line_segment(); }));

    SpannerSegmentType sst;
    if (line->tick() >= stick) {
        TLayout::layout_lyrics_line(line);
        if (line->ticks().is_zero() && line->is_end_melisma()) {
            return nullptr;
        }
        line->compute_start_element();
        line->compute_end_element();
        sst = line->tick2() <= etick ? SpannerSegmentType::SINGLE : SpannerSegmentType::BEGIN;
    } else if (line->tick() < stick && line->tick2() > etick) {
        sst = SpannerSegmentType::MIDDLE;
    } else {
        sst = SpannerSegmentType::END;
    }
    line_segm->set_spanner_segment_type(sst);

    TLayout::layout_lyrics_line_segment(line_segm, ctx);
    if (!line->is_partial_lyrics_line() && !line->lyrics()) {
        // this line could have been removed in the process of laying out surrounding lyrics
        return nullptr;
    }

    return line_segm;
}

SpannerSegment *TLayout::layout_system(Volta *line, System *system, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(line);
    return layout_system_s_line(line, system, ctx);
}

SpannerSegment *TLayout::layout_system(Slur *line, System *system, LayoutContext &ctx) {
    LAYOUT_CALL_ITEM(line);
    return SlurTieLayout::layout_system(line, system, ctx);
}
