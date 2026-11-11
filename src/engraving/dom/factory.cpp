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

#include "factory.h"

#include "accidental.h"
#include "actionicon.h"
#include "ambitus.h"
#include "anchors.h"
#include "arpeggio.h"
#include "articulation.h"
#include "bagpembell.h"
#include "barline.h"
#include "beam.h"
#include "bend.h"
#include "box.h"
#include "bracket.h"
#include "breath.h"
#include "capo.h"
#include "chord.h"
#include "chordbracket.h"
#include "chordline.h"
#include "deadslapped.h"
#include "dynamic.h"
#include "expression.h"
#include "fermata.h"
#include "figuredbass.h"
#include "fingering.h"
#include "fret.h"
#include "glissando.h"
#include "gradualtempochange.h"
#include "guitarbend.h"
#include "hairpin.h"
#include "hammeronpulloff.h"
#include "harmonicmark.h"
#include "harmony.h"
#include "harppedaldiagram.h"
#include "image.h"
#include "instrchange.h"
#include "instrumentname.h"
#include "jump.h"
#include "keysig.h"
#include "laissezvib.h"
#include "layoutbreak.h"
#include "letring.h"
#include "log.h"
#include "lyrics.h"
#include "marker.h"
#include "measure.h"
#include "measurenumber.h"
#include "measurerepeat.h"
#include "mmrest.h"
#include "mmrestrange.h"
#include "note.h"
#include "noteline.h"
#include "ornament.h"
#include "ottava.h"
#include "page.h"
#include "pagelockindicator.h"
#include "palmmute.h"
#include "parenthesis.h"
#include "partialtie.h"
#include "pedal.h"
#include "pickscrape.h"
#include "playcounttext.h"
#include "playtechannotation.h"
#include "rasgueado.h"
#include "rehearsalmark.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "slur.h"
#include "soundflag.h"
#include "spacer.h"
#include "staff.h"
#include "stafflines.h"
#include "staffstate.h"
#include "stafftext.h"
#include "stafftypechange.h"
#include "staffvisibilityindicator.h"
#include "stavesharinglabel.h"
#include "stem.h"
#include "stemslash.h"
#include "sticking.h"
#include "stringtunings.h"
#include "system.h"
#include "systemdivider.h"
#include "systemlockindicator.h"
#include "systemtext.h"
#include "tabdurationsymbol.h"
#include "tapping.h"
#include "tempotext.h"
#include "text.h"
#include "textline.h"
#include "tie.h"
#include "timesig.h"
#include "tremolobar.h"
#include "tremolosinglechord.h"
#include "tremolotwochord.h"
#include "trill.h"
#include "tripletfeel.h"
#include "tuplet.h"
#include "types/typesconv.h"
#include "vibrato.h"
#include "volta.h"
#include "whammybar.h"

using namespace mu;
using namespace mu::engraving;

EngravingItem *Factory::create_item(ElementType type, DummyParentOr<EngravingItem> parent, bool is_accessible_enabled) {
    EngravingItem *item = do_create_item(type, parent);

    if (item) {
        item->set_accessible_enabled(is_accessible_enabled);
    }

    return item;
}

//! The parent if it is of the given type, the dummy otherwise: a parent of the wrong
//! type is no parent at all.
template <typename T>
static DummyParentOr<T> parent_of_type_or_dummy(EngravingItem *parent, DummyParent *dummy) {
    return parent_or_dummy(dynamic_cast<T *>(parent), dummy);
}

EngravingItem *Factory::do_create_item(ElementType type, DummyParentOr<EngravingItem> parent) {
    EngravingObject *parent_object = parent;
    Score *score = parent_object->score();
    DummyParent *dummy = score->dummy();
    // null when the item is to be created unattached; the cases below that need a
    // parent of a particular type fall back to the dummy either way
    EngravingItem *parent_item = parent.as<EngravingItem>();

    switch (type) {
        case ElementType::VOLTA: return new Volta(parent);
        case ElementType::OTTAVA: return new Ottava(parent);
        case ElementType::TEXTLINE: return new TextLine(parent);
        case ElementType::NOTELINE: return new NoteLine(parent);
        case ElementType::TRILL: return new Trill(parent);
        case ElementType::LET_RING: return new LetRing(parent);
        case ElementType::GRADUAL_TEMPO_CHANGE: return new GradualTempoChange(parent);
        case ElementType::VIBRATO: return new Vibrato(parent);
        case ElementType::PALM_MUTE: return new PalmMute(parent);
        case ElementType::WHAMMY_BAR: return new WhammyBar(parent);
        case ElementType::RASGUEADO: return new Rasgueado(parent);
        case ElementType::HARMONIC_MARK: return new HarmonicMark(parent);
        case ElementType::PICK_SCRAPE: return new PickScrape(parent);
        case ElementType::PEDAL: return new Pedal(parent);
        case ElementType::HAIRPIN: return new Hairpin(parent);
        case ElementType::CLEF: return new Clef(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::KEYSIG: return new KeySig(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::TIMESIG: return new TimeSig(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::BAR_LINE: return new BarLine(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::SYSTEM_DIVIDER: return new SystemDivider(parent_of_type_or_dummy<System>(parent_item, dummy));
        case ElementType::ARPEGGIO: return new Arpeggio(parent_of_type_or_dummy<Chord>(parent_item, dummy));
        case ElementType::CHORD_BRACKET: return new ChordBracket(parent_of_type_or_dummy<Chord>(parent_item, dummy));
        case ElementType::BREATH: return new Breath(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::GLISSANDO: return new Glissando(parent);
        case ElementType::BRACKET: return new Bracket(parent);
        case ElementType::ARTICULATION: return new Articulation(parent_of_type_or_dummy<ChordRest>(parent_item, dummy));
        case ElementType::TAPPING: return new Tapping(parent_of_type_or_dummy<ChordRest>(parent_item, dummy));
        case ElementType::ORNAMENT: return new Ornament(parent_of_type_or_dummy<ChordRest>(parent_item, dummy));
        case ElementType::FERMATA: return new Fermata(parent);
        case ElementType::CHORDLINE: return new ChordLine(parent_of_type_or_dummy<Chord>(parent_item, dummy));
        case ElementType::ACCIDENTAL: return new Accidental(parent);
        case ElementType::DYNAMIC: return new Dynamic(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::EXPRESSION: return new Expression(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::TEXT: return new Text(parent);
        case ElementType::MEASURE_NUMBER: return new MeasureNumber(parent_of_type_or_dummy<Measure>(parent_item, dummy));
        case ElementType::MMREST_RANGE: return new MMRestRange(parent_of_type_or_dummy<Measure>(parent_item, dummy));
        case ElementType::INSTRUMENT_NAME: return new InstrumentName(parent_of_type_or_dummy<System>(parent_item, dummy));
        case ElementType::STAFF_TEXT: return new StaffText(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::STAVE_SHARING_LABEL: return new StaveSharingLabel(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::PLAY_COUNT_TEXT: return new PlayCountText(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::PLAYTECH_ANNOTATION: return new PlayTechAnnotation(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::CAPO: return new Capo(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::SYSTEM_TEXT: return new SystemText(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::REHEARSAL_MARK: return new RehearsalMark(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::INSTRUMENT_CHANGE: return new InstrumentChange(parent);
        case ElementType::SOUND_FLAG: return new SoundFlag(parent);
        case ElementType::STAFFTYPE_CHANGE: return new StaffTypeChange(parent_of_type_or_dummy<MeasureBase>(parent_item, dummy));
        case ElementType::STAFF_VISIBILITY_INDICATOR:
            return new StaffVisibilityIndicator(parent_of_type_or_dummy<System>(parent_item, dummy));
        case ElementType::NOTEHEAD: return new NoteHead(parent_of_type_or_dummy<Note>(parent_item, dummy));
        case ElementType::NOTEDOT: {
            if (parent_item && parent_item->is_note()) {
                return new NoteDot(to_note(parent_item));
            } else if (parent_item && parent_item->is_rest()) {
                return new NoteDot(to_rest(parent_item));
            } else {
                return new NoteDot(dummy);
            }
        }
        case ElementType::TREMOLO_SINGLECHORD: return new TremoloSingleChord(parent_of_type_or_dummy<Chord>(parent_item, dummy));
        case ElementType::TREMOLO_TWOCHORD: return new TremoloTwoChord(parent_of_type_or_dummy<Chord>(parent_item, dummy));
        case ElementType::LAYOUT_BREAK: return new LayoutBreak(parent_of_type_or_dummy<MeasureBase>(parent_item, dummy));
        case ElementType::MARKER: return new Marker(parent);
        case ElementType::JUMP: return new Jump(parent_of_type_or_dummy<Measure>(parent_item, dummy));
        case ElementType::MEASURE_REPEAT: return new MeasureRepeat(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::ACTION_ICON: return new ActionIcon(parent);
        case ElementType::NOTE: return new Note(parent_of_type_or_dummy<Chord>(parent_item, dummy));
        case ElementType::SYMBOL: return new Symbol(parent);
        case ElementType::FSYMBOL: return new FSymbol(parent);
        case ElementType::CHORD: return new Chord(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::REST: return new Rest(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::MMREST: return new MMRest(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::DEAD_SLAPPED: return new DeadSlapped(to_rest(parent_item));
        case ElementType::SPACER: return new Spacer(parent_of_type_or_dummy<Measure>(parent_item, dummy));
        case ElementType::STAFF_STATE: return new StaffState(parent);
        case ElementType::TEMPO_TEXT: return new TempoText(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::HARMONY: return new Harmony(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::FRET_DIAGRAM: return new FretDiagram(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::HARP_DIAGRAM: return new HarpPedalDiagram(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::BEND: return new Bend(parent_of_type_or_dummy<Note>(parent_item, dummy));
        case ElementType::GUITAR_BEND: return new GuitarBend(parent_of_type_or_dummy<Note>(parent_item, dummy));
        case ElementType::TREMOLOBAR: return new TremoloBar(parent);
        case ElementType::LYRICS: return new Lyrics(parent_of_type_or_dummy<ChordRest>(parent_item, dummy));
        case ElementType::LYRICSLINE: return new LyricsLine(parent);
        case ElementType::FIGURED_BASS: return new FiguredBass(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::STEM: return new Stem(parent_of_type_or_dummy<Chord>(parent_item, dummy));
        case ElementType::SLUR: return new Slur(parent);
        case ElementType::HAMMER_ON_PULL_OFF: return new HammerOnPullOff(parent);
        case ElementType::TIE: return new Tie(parent);
        case ElementType::TUPLET: return new Tuplet(parent_of_type_or_dummy<Measure>(parent_item, dummy));
        case ElementType::FINGERING: return new Fingering(parent_of_type_or_dummy<Note>(parent_item, dummy));
        case ElementType::HBOX: return new HBox(score);
        case ElementType::VBOX: return new VBox(score);
        case ElementType::TBOX: return new TBox(score);
        case ElementType::FBOX: return new FBox(score);
        case ElementType::MEASURE: return new Measure(score);
        case ElementType::TAB_DURATION_SYMBOL: return new TabDurationSymbol(parent_of_type_or_dummy<ChordRest>(parent_item, dummy));
        case ElementType::IMAGE: return new Image(parent);
        case ElementType::BAGPIPE_EMBELLISHMENT: return new BagpipeEmbellishment(parent);
        case ElementType::AMBITUS: return new Ambitus(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::STICKING: return new Sticking(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::TRIPLET_FEEL: return new TripletFeel(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::STRING_TUNINGS: return new StringTunings(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::TIME_TICK_ANCHOR: return new TimeTickAnchor(parent_of_type_or_dummy<Segment>(parent_item, dummy));
        case ElementType::LAISSEZ_VIB: return new LaissezVib(parent_of_type_or_dummy<Note>(parent_item, dummy));
        case ElementType::PARTIAL_TIE: return new PartialTie(parent_of_type_or_dummy<Note>(parent_item, dummy));
        case ElementType::PARTIAL_LYRICSLINE: return new PartialLyricsLine(parent);
        case ElementType::PARENTHESIS: return new Parenthesis(parent);

        case ElementType::TEXTLINE_BASE:
        case ElementType::TEXTLINE_SEGMENT:
        case ElementType::GLISSANDO_SEGMENT:
        case ElementType::NOTELINE_SEGMENT:
        case ElementType::GUITAR_BEND_SEGMENT:
        case ElementType::GUITAR_BEND_HOLD:
        case ElementType::GUITAR_BEND_HOLD_SEGMENT:
        case ElementType::GUITAR_BEND_TEXT:
        case ElementType::SLUR_SEGMENT:
        case ElementType::TIE_SEGMENT:
        case ElementType::LAISSEZ_VIB_SEGMENT:
        case ElementType::PARTIAL_TIE_SEGMENT:
        case ElementType::STEM_SLASH:
        case ElementType::PAGE:
        case ElementType::BEAM:
        case ElementType::HOOK:
        case ElementType::HAIRPIN_SEGMENT:
        case ElementType::OTTAVA_SEGMENT:
        case ElementType::TRILL_SEGMENT:
        case ElementType::LET_RING_SEGMENT:
        case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
        case ElementType::VIBRATO_SEGMENT:
        case ElementType::PALM_MUTE_SEGMENT:
        case ElementType::WHAMMY_BAR_SEGMENT:
        case ElementType::RASGUEADO_SEGMENT:
        case ElementType::HARMONIC_MARK_SEGMENT:
        case ElementType::PICK_SCRAPE_SEGMENT:
        case ElementType::VOLTA_SEGMENT:
        case ElementType::PEDAL_SEGMENT:
        case ElementType::LYRICSLINE_SEGMENT:
        case ElementType::PARTIAL_LYRICSLINE_SEGMENT:
        case ElementType::LEDGER_LINE:
        case ElementType::STAFF_LINES:
        case ElementType::SELECTION:
        case ElementType::SHADOW_NOTE:
        case ElementType::SEGMENT:
        case ElementType::SYSTEM:
        case ElementType::MAXTYPE:
        case ElementType::INVALID:
        case ElementType::PART:
        case ElementType::SHARED_PART:
        case ElementType::STAFF:
        case ElementType::SCORE:
        case ElementType::BRACKET_ITEM:
        case ElementType::GRACE_NOTES_GROUP:
        case ElementType::ROOT_ITEM:
        case ElementType::FIGURED_BASS_ITEM:
        case ElementType::DUMMY:
        case ElementType::SYSTEM_LOCK_INDICATOR:
        case ElementType::PAGE_LOCK_INDICATOR:
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT:
        case ElementType::HAMMER_ON_PULL_OFF_TEXT:
        case ElementType::TAPPING_HALF_SLUR:
        case ElementType::TAPPING_HALF_SLUR_SEGMENT:
        case ElementType::TAPPING_TEXT: break;
    }

    LOGD() << "Cannot create element of type " << static_cast<int>(type) << " (" << TConv::to_xml(type) << ")";

    return nullptr;
}

EngravingItem *Factory::create_item_by_name(const AsciiStringView &name, DummyParentOr<EngravingItem> parent, bool is_accessible_enabled) {
    ElementType type = TConv::from_xml(name, ElementType::INVALID, is_accessible_enabled);
    if (type == ElementType::INVALID) {
        LOGE() << "Invalid type: " << name;
        return nullptr;
    }
    return create_item(type, parent, is_accessible_enabled);
}

#define CREATE_ITEM_IMPL(T, P, is_accessible_enabled, create_name)  \
    T *Factory::create_name(P parent, bool is_accessible_enabled) { \
        T *e = new T(parent);                                       \
        e->set_accessible_enabled(is_accessible_enabled);           \
        return e;                                                   \
    }

#define MAKE_ITEM_IMPL(T, P, make_name, create_name) \
    std::shared_ptr<T> Factory::make_name(P parent) { return std::shared_ptr<T>(create_name(parent)); }

#define COPY_ITEM_IMPL(T, copy_name)      \
    T *Factory::copy_name(const T &src) { \
        T *copy = new T(src);             \
        return copy;                      \
    }

CREATE_ITEM_IMPL(Accidental, DummyParentOr<EngravingItem>, is_accessible_enabled, create_accidental)
MAKE_ITEM_IMPL(Accidental, DummyParentOr<EngravingItem>, make_accidental, create_accidental)

CREATE_ITEM_IMPL(Ambitus, DummyParentOr<Segment>, is_accessible_enabled, create_ambitus)
MAKE_ITEM_IMPL(Ambitus, DummyParentOr<Segment>, make_ambitus, create_ambitus)

CREATE_ITEM_IMPL(Arpeggio, DummyParentOr<Chord>, is_accessible_enabled, create_arpeggio)
MAKE_ITEM_IMPL(Arpeggio, DummyParentOr<Chord>, make_arpeggio, create_arpeggio)

CREATE_ITEM_IMPL(ChordBracket, DummyParentOr<Chord>, is_accessible_enabled, create_chord_bracket)
MAKE_ITEM_IMPL(ChordBracket, DummyParentOr<Chord>, make_chord_bracket, create_chord_bracket)

CREATE_ITEM_IMPL(Articulation, DummyParentOr<ChordRest>, is_accessible_enabled, create_articulation)
MAKE_ITEM_IMPL(Articulation, DummyParentOr<ChordRest>, make_articulation, create_articulation)

CREATE_ITEM_IMPL(Tapping, DummyParentOr<ChordRest>, is_accessible_enabled, create_tapping)
MAKE_ITEM_IMPL(Tapping, DummyParentOr<ChordRest>, make_tapping, create_tapping)

CREATE_ITEM_IMPL(Ornament, DummyParentOr<ChordRest>, is_accessible_enabled, create_ornament)
MAKE_ITEM_IMPL(Ornament, DummyParentOr<ChordRest>, make_ornament, create_ornament)

CREATE_ITEM_IMPL(BarLine, DummyParentOr<Segment>, is_accessible_enabled, create_bar_line)
COPY_ITEM_IMPL(BarLine, copy_bar_line)
MAKE_ITEM_IMPL(BarLine, DummyParentOr<Segment>, make_bar_line, create_bar_line)

CREATE_ITEM_IMPL(Beam, Score *, is_accessible_enabled, create_beam)
MAKE_ITEM_IMPL(Beam, Score *, make_beam, create_beam)

CREATE_ITEM_IMPL(Bend, DummyParentOr<Note>, is_accessible_enabled, create_bend)
MAKE_ITEM_IMPL(Bend, DummyParentOr<Note>, make_bend, create_bend)

CREATE_ITEM_IMPL(Bracket, DummyParentOr<EngravingItem>, is_accessible_enabled, create_bracket)
MAKE_ITEM_IMPL(Bracket, DummyParentOr<EngravingItem>, make_bracket, create_bracket)

BracketItem *Factory::create_bracket_item(DummyParentOr<EngravingItem> parent) {
    BracketItem *bi = new BracketItem(parent);
    return bi;
}

BracketItem *Factory::create_bracket_item(DummyParentOr<EngravingItem> parent, BracketType bracket_type, size_t span) {
    BracketItem *bi = new BracketItem(parent, bracket_type, span);
    return bi;
}

CREATE_ITEM_IMPL(Breath, DummyParentOr<Segment>, is_accessible_enabled, create_breath)
MAKE_ITEM_IMPL(Breath, DummyParentOr<Segment>, make_breath, create_breath)

CREATE_ITEM_IMPL(Chord, DummyParentOr<Segment>, is_accessible_enabled, create_chord)

Chord *Factory::copy_chord(const Chord &src, bool link) {
    Chord *copy = new Chord(src, link);
    copy->set_accessible_enabled(src.accessible_enabled());

    return copy;
}
MAKE_ITEM_IMPL(Chord, DummyParentOr<Segment>, make_chord, create_chord)

CREATE_ITEM_IMPL(ChordLine, DummyParentOr<Chord>, is_accessible_enabled, create_chord_line)
COPY_ITEM_IMPL(ChordLine, copy_chord_line)
MAKE_ITEM_IMPL(ChordLine, DummyParentOr<Chord>, make_chord_line, create_chord_line)

CREATE_ITEM_IMPL(Clef, DummyParentOr<Segment>, is_accessible_enabled, create_clef)
COPY_ITEM_IMPL(Clef, copy_clef)
MAKE_ITEM_IMPL(Clef, DummyParentOr<Segment>, make_clef, create_clef)

CREATE_ITEM_IMPL(DeadSlapped, Rest *, is_accessible_enabled, create_dead_slapped)
COPY_ITEM_IMPL(DeadSlapped, copy_dead_slapped)

CREATE_ITEM_IMPL(Fermata, DummyParentOr<Segment>, is_accessible_enabled, create_fermata)
MAKE_ITEM_IMPL(Fermata, DummyParentOr<Segment>, make_fermata, create_fermata)

CREATE_ITEM_IMPL(FiguredBass, DummyParentOr<Segment>, is_accessible_enabled, create_figured_bass)
MAKE_ITEM_IMPL(FiguredBass, DummyParentOr<Segment>, make_figured_bass, create_figured_bass)

CREATE_ITEM_IMPL(FretDiagram, DummyParentOr<Segment>, is_accessible_enabled, create_fret_diagram)
COPY_ITEM_IMPL(FretDiagram, copy_fret_diagram)
MAKE_ITEM_IMPL(FretDiagram, DummyParentOr<Segment>, make_fret_diagram, create_fret_diagram)

CREATE_ITEM_IMPL(HarpPedalDiagram, DummyParentOr<Segment>, is_accessible_enabled, create_harp_pedal_diagram)
COPY_ITEM_IMPL(HarpPedalDiagram, copy_harp_pedal_diagram)
MAKE_ITEM_IMPL(HarpPedalDiagram, DummyParentOr<Segment>, make_harp_pedal_diagram, create_harp_pedal_diagram);

CREATE_ITEM_IMPL(KeySig, DummyParentOr<Segment>, is_accessible_enabled, create_key_sig)
COPY_ITEM_IMPL(KeySig, copy_key_sig)
MAKE_ITEM_IMPL(KeySig, DummyParentOr<Segment>, make_key_sig, create_key_sig)

CREATE_ITEM_IMPL(LaissezVib, DummyParentOr<Note>, is_accessible_enabled, create_laissez_vib)
COPY_ITEM_IMPL(LaissezVib, copy_laissez_vib);

CREATE_ITEM_IMPL(LayoutBreak, DummyParentOr<MeasureBase>, is_accessible_enabled, create_layout_break)
COPY_ITEM_IMPL(LayoutBreak, copy_layout_break)
MAKE_ITEM_IMPL(LayoutBreak, DummyParentOr<MeasureBase>, make_layout_break, create_layout_break)

CREATE_ITEM_IMPL(Lyrics, DummyParentOr<ChordRest>, is_accessible_enabled, create_lyrics)
COPY_ITEM_IMPL(Lyrics, copy_lyrics)

CREATE_ITEM_IMPL(LyricsLine, DummyParentOr<EngravingItem>, is_accessible_enabled, create_lyrics_line)
COPY_ITEM_IMPL(LyricsLine, copy_lyrics_line)

CREATE_ITEM_IMPL(Measure, Score *, is_accessible_enabled, create_measure)
COPY_ITEM_IMPL(Measure, copy_measure)

CREATE_ITEM_IMPL(MeasureRepeat, DummyParentOr<Segment>, is_accessible_enabled, create_measure_repeat)
COPY_ITEM_IMPL(MeasureRepeat, copy_measure_repeat)

CREATE_ITEM_IMPL(StringTunings, DummyParentOr<Segment>, is_accessible_enabled, create_string_tunings)
COPY_ITEM_IMPL(StringTunings, copy_string_tunings)
MAKE_ITEM_IMPL(StringTunings, DummyParentOr<Segment>, make_string_tunings, create_string_tunings);

CREATE_ITEM_IMPL(Note, DummyParentOr<Chord>, is_accessible_enabled, create_note)

Note *Factory::copy_note(const Note &src, bool link) {
    Note *copy = new Note(src, link);
    copy->set_accessible_enabled(src.accessible_enabled());

    return copy;
}
MAKE_ITEM_IMPL(Note, DummyParentOr<Chord>, make_note, create_note)

CREATE_ITEM_IMPL(NoteDot, DummyParentOr<Note>, is_accessible_enabled, create_note_dot)
CREATE_ITEM_IMPL(NoteDot, Rest *, is_accessible_enabled, create_note_dot)
COPY_ITEM_IMPL(NoteDot, copy_note_dot)

CREATE_ITEM_IMPL(NoteLine, DummyParentOr<Note>, is_accessible_enabled, create_note_line)
MAKE_ITEM_IMPL(NoteLine, DummyParentOr<Note>, make_note_line, create_note_line);

CREATE_ITEM_IMPL(Page, Score *, is_accessible_enabled, create_page)

PageLockIndicator *Factory::create_page_lock_indicator(DummyParentOr<System> parent, const RangeLock *lock, bool is_accessible_enabled) {
    PageLockIndicator *pli = new PageLockIndicator(parent, lock);
    pli->set_accessible_enabled(is_accessible_enabled);
    return pli;
}

COPY_ITEM_IMPL(PageLockIndicator, copy_page_lock_indicator)

CREATE_ITEM_IMPL(PartialTie, DummyParentOr<Note>, is_accessible_enabled, create_partial_tie)
COPY_ITEM_IMPL(PartialTie, copy_partial_tie)

CREATE_ITEM_IMPL(PartialLyricsLine, DummyParentOr<EngravingItem>, is_accessible_enabled, create_partial_lyrics_line)
COPY_ITEM_IMPL(PartialLyricsLine, copy_partial_lyrics_line)

CREATE_ITEM_IMPL(Rest, DummyParentOr<Segment>, is_accessible_enabled, create_rest)

Rest *Factory::create_rest(DummyParentOr<Segment> parent, const TDuration &t, bool is_accessible_enabled) {
    Rest *r = new Rest(parent, t);
    r->set_accessible_enabled(is_accessible_enabled);

    return r;
}

Rest *Factory::copy_rest(const Rest &src, bool link) {
    Rest *copy = new Rest(src, link);
    copy->set_accessible_enabled(src.accessible_enabled());

    return copy;
}

CREATE_ITEM_IMPL(Segment, DummyParentOr<Measure>, is_accessible_enabled, create_segment)

Segment *Factory::create_segment(DummyParentOr<Measure> parent, SegmentType type, const Fraction &t, bool is_accessible_enabled) {
    Segment *s = new Segment(parent, type, t);
    s->set_accessible_enabled(is_accessible_enabled);

    return s;
}

CREATE_ITEM_IMPL(Slur, DummyParentOr<EngravingItem>, is_accessible_enabled, create_slur)
MAKE_ITEM_IMPL(Slur, DummyParentOr<EngravingItem>, make_slur, create_slur)

CREATE_ITEM_IMPL(Spacer, DummyParentOr<Measure>, is_accessible_enabled, create_spacer)
MAKE_ITEM_IMPL(Spacer, DummyParentOr<Measure>, make_spacer, create_spacer)

Staff *Factory::create_staff(Part *parent) {
    Staff *staff = new Staff(parent);
    staff->set_part(parent);
    return staff;
}

CREATE_ITEM_IMPL(StaffLines, DummyParentOr<Measure>, is_accessible_enabled, create_staff_lines)
COPY_ITEM_IMPL(StaffLines, copy_staff_lines)

CREATE_ITEM_IMPL(StaffState, DummyParentOr<EngravingItem>, is_accessible_enabled, create_staff_state)

CREATE_ITEM_IMPL(StaffTypeChange, DummyParentOr<MeasureBase>, is_accessible_enabled, create_staff_type_change)
MAKE_ITEM_IMPL(StaffTypeChange, DummyParentOr<MeasureBase>, make_staff_type_change, create_staff_type_change)

StaffText *Factory::create_staff_text(DummyParentOr<Segment> parent, TextStyleType text_style_type, bool is_accessible_enabled) {
    StaffText *staff_text = new StaffText(parent, text_style_type);
    staff_text->set_accessible_enabled(is_accessible_enabled);

    return staff_text;
}

StaveSharingLabel *Factory::create_stave_sharing_label(DummyParentOr<Segment> parent, TextStyleType text_style_type,
                                                       bool is_accessible_enabled) {
    StaveSharingLabel *stave_sharing_label = new StaveSharingLabel(parent, text_style_type);
    stave_sharing_label->set_accessible_enabled(is_accessible_enabled);
    return stave_sharing_label;
}

CREATE_ITEM_IMPL(SoundFlag, DummyParentOr<EngravingItem>, is_accessible_enabled, create_sound_flag)

CREATE_ITEM_IMPL(Expression, DummyParentOr<Segment>, is_accessible_enabled, create_expression)

CREATE_ITEM_IMPL(RehearsalMark, DummyParentOr<Segment>, is_accessible_enabled, create_rehearsal_mark)

CREATE_ITEM_IMPL(Stem, DummyParentOr<Chord>, is_accessible_enabled, create_stem)
COPY_ITEM_IMPL(Stem, copy_stem)

CREATE_ITEM_IMPL(StemSlash, DummyParentOr<Chord>, is_accessible_enabled, create_stem_slash)
COPY_ITEM_IMPL(StemSlash, copy_stem_slash)

CREATE_ITEM_IMPL(System, Score *, is_accessible_enabled, create_system)

SystemText *Factory::create_system_text(DummyParentOr<Segment> parent, TextStyleType text_style_type, ElementType type,
                                        bool is_accessible_enabled) {
    SystemText *system_text = new SystemText(parent, text_style_type, type);
    system_text->set_accessible_enabled(is_accessible_enabled);

    return system_text;
}

CREATE_ITEM_IMPL(InstrumentChange, DummyParentOr<Segment>, is_accessible_enabled, create_instrument_change)

InstrumentChange *Factory::create_instrument_change(DummyParentOr<Segment> parent, const Instrument &instrument,
                                                    bool is_accessible_enabled) {
    InstrumentChange *instrument_change = new InstrumentChange(instrument, parent);
    instrument_change->set_accessible_enabled(is_accessible_enabled);

    return instrument_change;
}

CREATE_ITEM_IMPL(Sticking, DummyParentOr<Segment>, is_accessible_enabled, create_sticking)

CREATE_ITEM_IMPL(Fingering, DummyParentOr<Note>, is_accessible_enabled, create_fingering)

Fingering *Factory::create_fingering(DummyParentOr<Note> parent, TextStyleType text_style_type, bool is_accessible_enabled) {
    Fingering *fingering = new Fingering(parent, text_style_type);
    fingering->set_accessible_enabled(is_accessible_enabled);

    return fingering;
}

CREATE_ITEM_IMPL(Harmony, DummyParentOr<EngravingItem>, is_accessible_enabled, create_harmony)

CREATE_ITEM_IMPL(TempoText, DummyParentOr<Segment>, is_accessible_enabled, create_tempo_text)

Text *Factory::create_text(DummyParentOr<EngravingItem> parent, TextStyleType tid, bool is_accessible_enabled) {
    Text *t = new Text(parent, tid);
    t->set_accessible_enabled(is_accessible_enabled);

    return t;
}

COPY_ITEM_IMPL(Text, copy_text)

CREATE_ITEM_IMPL(Tie, DummyParentOr<EngravingItem>, is_accessible_enabled, create_tie)

Tie *Factory::copy_tie(const Tie &src) {
    Tie *copy = src.is_laissez_vib() ? new LaissezVib(*to_laissez_vib(&src)) : new Tie(src);
    copy->set_accessible_enabled(src.accessible_enabled());

    return copy;
}

CREATE_ITEM_IMPL(TimeSig, DummyParentOr<Segment>, is_accessible_enabled, create_time_sig)
COPY_ITEM_IMPL(TimeSig, copy_time_sig)
MAKE_ITEM_IMPL(TimeSig, DummyParentOr<Segment>, make_time_sig, create_time_sig)

CREATE_ITEM_IMPL(TremoloTwoChord, DummyParentOr<Chord>, is_accessible_enabled, create_tremolo_two_chord)
COPY_ITEM_IMPL(TremoloTwoChord, copy_tremolo_two_chord)
MAKE_ITEM_IMPL(TremoloTwoChord, DummyParentOr<Chord>, make_tremolo_two_chord, create_tremolo_two_chord)

CREATE_ITEM_IMPL(TremoloSingleChord, DummyParentOr<Chord>, is_accessible_enabled, create_tremolo_single_chord)
COPY_ITEM_IMPL(TremoloSingleChord, copy_tremolo_single_chord)
MAKE_ITEM_IMPL(TremoloSingleChord, DummyParentOr<Chord>, make_tremolo_single_chord, create_tremolo_single_chord)

CREATE_ITEM_IMPL(TremoloBar, DummyParentOr<EngravingItem>, is_accessible_enabled, create_tremolo_bar)
MAKE_ITEM_IMPL(TremoloBar, DummyParentOr<EngravingItem>, make_tremolo_bar, create_tremolo_bar)

CREATE_ITEM_IMPL(Tuplet, DummyParentOr<Measure>, is_accessible_enabled, create_tuplet)
COPY_ITEM_IMPL(Tuplet, copy_tuplet)

CREATE_ITEM_IMPL(Hairpin, DummyParentOr<EngravingItem>, is_accessible_enabled, create_hairpin)
MAKE_ITEM_IMPL(Hairpin, DummyParentOr<EngravingItem>, make_hairpin, create_hairpin)

CREATE_ITEM_IMPL(HammerOnPullOff, DummyParentOr<EngravingItem>, is_accessible_enabled, create_hammer_on_pull_off)
MAKE_ITEM_IMPL(HammerOnPullOff, DummyParentOr<EngravingItem>, make_hammer_on_pull_off, create_hammer_on_pull_off)

CREATE_ITEM_IMPL(Glissando, DummyParentOr<EngravingItem>, is_accessible_enabled, create_glissando)
MAKE_ITEM_IMPL(Glissando, DummyParentOr<EngravingItem>, make_glissando, create_glissando)

CREATE_ITEM_IMPL(GuitarBend, DummyParentOr<Note>, is_accessible_enabled, create_guitar_bend)
MAKE_ITEM_IMPL(GuitarBend, DummyParentOr<Note>, make_guitar_bend, create_guitar_bend)

CREATE_ITEM_IMPL(Jump, DummyParentOr<Measure>, is_accessible_enabled, create_jump)

CREATE_ITEM_IMPL(Trill, DummyParentOr<EngravingItem>, is_accessible_enabled, create_trill)

TripletFeel *Factory::create_triplet_feel(DummyParentOr<Segment> parent, TripletFeelType type, bool is_accessible_enabled) {
    TripletFeel *t = new TripletFeel(parent, type);
    t->set_accessible_enabled(is_accessible_enabled);

    return t;
}

CREATE_ITEM_IMPL(Vibrato, DummyParentOr<EngravingItem>, is_accessible_enabled, create_vibrato)

CREATE_ITEM_IMPL(TextLine, DummyParentOr<EngravingItem>, is_accessible_enabled, create_text_line)
MAKE_ITEM_IMPL(TextLine, DummyParentOr<EngravingItem>, make_text_line, create_text_line);

CREATE_ITEM_IMPL(Ottava, DummyParentOr<EngravingItem>, is_accessible_enabled, create_ottava)

CREATE_ITEM_IMPL(LetRing, DummyParentOr<EngravingItem>, is_accessible_enabled, create_let_ring)

CREATE_ITEM_IMPL(Marker, DummyParentOr<EngravingItem>, is_accessible_enabled, create_marker)

Marker *Factory::create_marker(DummyParentOr<EngravingItem> parent, TextStyleType tid, bool is_accessible_enabled) {
    Marker *m = new Marker(parent, tid);
    m->set_accessible_enabled(is_accessible_enabled);

    return m;
}

MAKE_ITEM_IMPL(Marker, DummyParentOr<EngravingItem>, make_marker, create_marker)

CREATE_ITEM_IMPL(GradualTempoChange, DummyParentOr<EngravingItem>, is_accessible_enabled, create_gradual_tempo_change)

CREATE_ITEM_IMPL(PalmMute, DummyParentOr<EngravingItem>, is_accessible_enabled, create_palm_mute)

CREATE_ITEM_IMPL(WhammyBar, DummyParentOr<EngravingItem>, is_accessible_enabled, create_whammy_bar)
MAKE_ITEM_IMPL(WhammyBar, DummyParentOr<EngravingItem>, make_whammy_bar, create_whammy_bar)

CREATE_ITEM_IMPL(Rasgueado, DummyParentOr<EngravingItem>, is_accessible_enabled, create_rasgueado)

CREATE_ITEM_IMPL(HarmonicMark, DummyParentOr<EngravingItem>, is_accessible_enabled, create_harmonic_mark)

CREATE_ITEM_IMPL(PickScrape, DummyParentOr<EngravingItem>, is_accessible_enabled, create_pick_scrape)

CREATE_ITEM_IMPL(Volta, DummyParentOr<EngravingItem>, is_accessible_enabled, create_volta)

CREATE_ITEM_IMPL(Pedal, DummyParentOr<EngravingItem>, is_accessible_enabled, create_pedal)

CREATE_ITEM_IMPL(Dynamic, DummyParentOr<Segment>, is_accessible_enabled, create_dynamic)

CREATE_ITEM_IMPL(MMRest, DummyParentOr<Segment>, is_accessible_enabled, create_mm_rest)

CREATE_ITEM_IMPL(VBox, Score *, is_accessible_enabled, create_v_box)

VBox *Factory::create_title_v_box(Score *parent, bool is_accessible_enabled) {
    VBox *b = new VBox(parent);
    b->set_accessible_enabled(is_accessible_enabled);
    b->set_size_is_spatium_dependent(false);
    b->set_tick(Fraction(0, 1));

    return b;
}

CREATE_ITEM_IMPL(HBox, Score *, is_accessible_enabled, create_h_box)

CREATE_ITEM_IMPL(TBox, Score *, is_accessible_enabled, create_t_box)

CREATE_ITEM_IMPL(FBox, Score *, is_accessible_enabled, create_f_box)

Image *Factory::create_image(DummyParentOr<EngravingItem> parent) {
    Image *image = new Image(parent);

    return image;
}

CREATE_ITEM_IMPL(Symbol, DummyParentOr<EngravingItem>, is_accessible_enabled, create_symbol)
CREATE_ITEM_IMPL(FSymbol, DummyParentOr<EngravingItem>, is_accessible_enabled, create_f_symbol)

CREATE_ITEM_IMPL(PlayCountText, DummyParentOr<Segment>, is_accessible_enabled, create_play_count_text)

PlayTechAnnotation *Factory::create_play_tech_annotation(DummyParentOr<Segment> parent, PlayingTechniqueType technique_type,
                                                         TextStyleType style_type, bool is_accessible_enabled) {
    PlayTechAnnotation *annotation = new PlayTechAnnotation(parent, technique_type, style_type);
    annotation->set_accessible_enabled(is_accessible_enabled);

    return annotation;
}

CREATE_ITEM_IMPL(Capo, DummyParentOr<Segment>, is_accessible_enabled, create_capo)
MAKE_ITEM_IMPL(Capo, DummyParentOr<Segment>, make_capo, create_capo);

CREATE_ITEM_IMPL(TimeTickAnchor, DummyParentOr<Segment>, is_accessible_enabled, create_time_tick_anchor)

CREATE_ITEM_IMPL(StaffVisibilityIndicator, DummyParentOr<System>, is_accessible_enabled, create_staff_visibility_indicator)

SystemLockIndicator *Factory::create_system_lock_indicator(DummyParentOr<System> parent, const RangeLock *lock,
                                                           bool is_accessible_enabled) {
    SystemLockIndicator *sli = new SystemLockIndicator(parent, lock);
    sli->set_accessible_enabled(is_accessible_enabled);
    return sli;
}

COPY_ITEM_IMPL(SystemLockIndicator, copy_system_lock_indicator)

CREATE_ITEM_IMPL(Parenthesis, DummyParentOr<EngravingItem>, is_accessible_enabled, create_parenthesis);
COPY_ITEM_IMPL(Parenthesis, copy_parenthesis)
