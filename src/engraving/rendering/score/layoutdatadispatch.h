/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <array>
#include <cstddef>

#include "dom/accidental.h"
#include "dom/ambitus.h"
#include "dom/anchors.h"
#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/bagpembell.h"
#include "dom/barline.h"
#include "dom/beam.h"
#include "dom/bend.h"
#include "dom/bracket.h"
#include "dom/chord.h"
#include "dom/chordline.h"
#include "dom/clef.h"
#include "dom/deadslapped.h"
#include "dom/dynamic.h"
#include "dom/expression.h"
#include "dom/fermata.h"
#include "dom/figuredbass.h"
#include "dom/fingering.h"
#include "dom/fret.h"
#include "dom/guitarbend.h"
#include "dom/hairpin.h"
#include "dom/harmony.h"
#include "dom/indicatoricon.h"
#include "dom/instrumentname.h"
#include "dom/jianpudurationline.h"
#include "dom/jianpuoctavedot.h"
#include "dom/keysig.h"
#include "dom/laissezvib.h"
#include "dom/ledgerline.h"
#include "dom/lyrics.h"
#include "dom/measure.h"
#include "dom/measurerepeat.h"
#include "dom/mmrest.h"
#include "dom/note.h"
#include "dom/pagelockindicator.h"
#include "dom/parenthesis.h"
#include "dom/rest.h"
#include "dom/segment.h"
#include "dom/slurtie.h"
#include "dom/spacer.h"
#include "dom/staffstate.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/system.h"
#include "dom/tabdurationsymbol.h"
#include "dom/tapping.h"
#include "dom/textbase.h"
#include "dom/textlinebase.h"
#include "dom/tie.h"
#include "dom/timesig.h"
#include "dom/tremolobar.h"
#include "dom/tremolotwochord.h"
#include "dom/tuplet.h"

namespace mu::engraving::rendering::score {
template <typename Item, std::size_t ChunkSize, ElementType... Types>
struct LayoutDataBinding {
    using Data = typename Item::LayoutData;
    static constexpr std::size_t chunk_size = ChunkSize;

    template <typename Action>
    static constexpr void bind(auto &table) {
        const auto invoke = +[](Action &action) -> decltype(auto) { return action.template operator()<Data>(); };
        if constexpr (sizeof...(Types) == 0) {
            table.fill(invoke);
        } else {
            ((table[static_cast<unsigned char>(Types)] = invoke), ...);
        }
    }
};

template <typename... Bindings>
struct LayoutDataDispatch {
    template <typename Action>
    static decltype(auto) dispatch(ElementType type, Action action) {
        using Result = decltype(action.template operator()<EngravingItem::LayoutData>());
        static constexpr auto table = [] {
            std::array<Result (*)(Action &), 256> result{};
            (Bindings::template bind<Action>(result), ...);
            return result;
        }();
        return table[static_cast<unsigned char>(type)](action);
    }
};

// The unqualified binding supplies the default before the explicit entries override it.
using LayoutDataTypes = LayoutDataDispatch<
    LayoutDataBinding<EngravingItem, 64>, LayoutDataBinding<Accidental, 64, ElementType::ACCIDENTAL>,
    LayoutDataBinding<Ambitus, 16, ElementType::AMBITUS>,
    LayoutDataBinding<Arpeggio, 16, ElementType::ARPEGGIO, ElementType::CHORD_BRACKET>,
    LayoutDataBinding<Articulation, 16, ElementType::ARTICULATION, ElementType::ORNAMENT>,
    LayoutDataBinding<BagpipeEmbellishment, 16, ElementType::BAGPIPE_EMBELLISHMENT>, LayoutDataBinding<BarLine, 16, ElementType::BAR_LINE>,
    LayoutDataBinding<Beam, 64, ElementType::BEAM>, LayoutDataBinding<TremoloTwoChord, 16, ElementType::TREMOLO_TWOCHORD>,
    LayoutDataBinding<Bend, 16, ElementType::BEND>, LayoutDataBinding<Bracket, 16, ElementType::BRACKET>,
    LayoutDataBinding<Dynamic, 16, ElementType::DYNAMIC>, LayoutDataBinding<Expression, 16, ElementType::EXPRESSION>,
    LayoutDataBinding<TextBase, 64, ElementType::CAPO, ElementType::GUITAR_BEND_TEXT, ElementType::HAMMER_ON_PULL_OFF_TEXT,
                      ElementType::HARP_DIAGRAM, ElementType::INSTRUMENT_CHANGE, ElementType::JUMP, ElementType::LYRICS,
                      ElementType::MARKER, ElementType::MEASURE_NUMBER, ElementType::MMREST_RANGE, ElementType::PLAY_COUNT_TEXT,
                      ElementType::PLAYTECH_ANNOTATION, ElementType::REHEARSAL_MARK, ElementType::STAFF_TEXT,
                      ElementType::STAVE_SHARING_LABEL, ElementType::STICKING, ElementType::STRING_TUNINGS, ElementType::SYSTEM_TEXT,
                      ElementType::TAPPING_TEXT, ElementType::TEMPO_TEXT, ElementType::TEXT, ElementType::TRIPLET_FEEL>,
    LayoutDataBinding<Chord, 64, ElementType::CHORD>, LayoutDataBinding<ChordLine, 16, ElementType::CHORDLINE>,
    LayoutDataBinding<Clef, 16, ElementType::CLEF>, LayoutDataBinding<DeadSlapped, 16, ElementType::DEAD_SLAPPED>,
    LayoutDataBinding<FiguredBassItem, 16, ElementType::FIGURED_BASS_ITEM>, LayoutDataBinding<FiguredBass, 16, ElementType::FIGURED_BASS>,
    LayoutDataBinding<Fingering, 16, ElementType::FINGERING>,
    LayoutDataBinding<SlurTie, 16, ElementType::SLUR, ElementType::TIE, ElementType::PARTIAL_TIE, ElementType::LAISSEZ_VIB,
                      ElementType::HAMMER_ON_PULL_OFF, ElementType::TAPPING_HALF_SLUR>,
    LayoutDataBinding<Fermata, 16, ElementType::FERMATA>,
    LayoutDataBinding<Spanner, 16, ElementType::VOLTA, ElementType::OTTAVA, ElementType::TRILL, ElementType::LET_RING,
                      ElementType::GRADUAL_TEMPO_CHANGE, ElementType::VIBRATO, ElementType::PALM_MUTE, ElementType::WHAMMY_BAR,
                      ElementType::RASGUEADO, ElementType::HARMONIC_MARK, ElementType::PICK_SCRAPE, ElementType::TEXTLINE,
                      ElementType::PEDAL, ElementType::LYRICSLINE, ElementType::PARTIAL_LYRICSLINE, ElementType::GLISSANDO,
                      ElementType::NOTELINE, ElementType::GUITAR_BEND_HOLD, ElementType::TEXTLINE_BASE>,
    LayoutDataBinding<Hairpin, 16, ElementType::HAIRPIN>, LayoutDataBinding<FretDiagram, 16, ElementType::FRET_DIAGRAM>,
    LayoutDataBinding<TextLineBaseSegment, 16, ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT, ElementType::HAIRPIN_SEGMENT,
                      ElementType::HARMONIC_MARK_SEGMENT, ElementType::LET_RING_SEGMENT, ElementType::NOTELINE_SEGMENT,
                      ElementType::OTTAVA_SEGMENT, ElementType::PALM_MUTE_SEGMENT, ElementType::PEDAL_SEGMENT,
                      ElementType::PICK_SCRAPE_SEGMENT, ElementType::RASGUEADO_SEGMENT, ElementType::TEXTLINE_SEGMENT,
                      ElementType::VOLTA_SEGMENT, ElementType::WHAMMY_BAR_SEGMENT>,
    LayoutDataBinding<GuitarBend, 16, ElementType::GUITAR_BEND>, LayoutDataBinding<GuitarBendSegment, 16, ElementType::GUITAR_BEND_SEGMENT>,
    LayoutDataBinding<GuitarBendHoldSegment, 16, ElementType::GUITAR_BEND_HOLD_SEGMENT>,
    LayoutDataBinding<SlurTieSegment, 16, ElementType::HAMMER_ON_PULL_OFF_SEGMENT, ElementType::SLUR_SEGMENT,
                      ElementType::TAPPING_HALF_SLUR_SEGMENT>,
    LayoutDataBinding<Harmony, 16, ElementType::HARMONY>, LayoutDataBinding<InstrumentName, 16, ElementType::INSTRUMENT_NAME>,
    LayoutDataBinding<JianpuDurationLine, 16, ElementType::JIANPU_DURATION_LINE>,
    LayoutDataBinding<JianpuOctaveDot, 16, ElementType::JIANPU_OCTAVE_DOT>, LayoutDataBinding<KeySig, 16, ElementType::KEYSIG>,
    LayoutDataBinding<LaissezVibSegment, 16, ElementType::LAISSEZ_VIB_SEGMENT>, LayoutDataBinding<LedgerLine, 64, ElementType::LEDGER_LINE>,
    LayoutDataBinding<LyricsLineSegment, 16, ElementType::LYRICSLINE_SEGMENT, ElementType::PARTIAL_LYRICSLINE_SEGMENT>,
    LayoutDataBinding<MeasureBase, 16, ElementType::HBOX, ElementType::VBOX, ElementType::TBOX, ElementType::FBOX>,
    LayoutDataBinding<Segment, 64, ElementType::SEGMENT>, LayoutDataBinding<Measure, 64, ElementType::MEASURE>,
    LayoutDataBinding<MeasureRepeat, 16, ElementType::MEASURE_REPEAT>, LayoutDataBinding<MMRest, 16, ElementType::MMREST>,
    LayoutDataBinding<Note, 64, ElementType::NOTE>, LayoutDataBinding<PageLockIndicator, 16, ElementType::PAGE_LOCK_INDICATOR>,
    LayoutDataBinding<TieSegment, 16, ElementType::PARTIAL_TIE_SEGMENT, ElementType::TIE_SEGMENT>,
    LayoutDataBinding<Rest, 64, ElementType::REST>, LayoutDataBinding<Tuplet, 16, ElementType::TUPLET>,
    LayoutDataBinding<Spacer, 16, ElementType::SPACER>, LayoutDataBinding<StaffState, 16, ElementType::STAFF_STATE>,
    LayoutDataBinding<IndicatorIcon, 16, ElementType::STAFF_VISIBILITY_INDICATOR, ElementType::STAVE_SHARING_CHANGE,
                      ElementType::SYSTEM_LOCK_INDICATOR>,
    LayoutDataBinding<Stem, 64, ElementType::STEM>, LayoutDataBinding<StemSlash, 16, ElementType::STEM_SLASH>,
    LayoutDataBinding<System, 16, ElementType::SYSTEM>, LayoutDataBinding<TabDurationSymbol, 16, ElementType::TAB_DURATION_SYMBOL>,
    LayoutDataBinding<Tapping, 16, ElementType::TAPPING>, LayoutDataBinding<TimeSig, 16, ElementType::TIMESIG>,
    LayoutDataBinding<TremoloBar, 16, ElementType::TREMOLOBAR>, LayoutDataBinding<TimeTickAnchor, 16, ElementType::TIME_TICK_ANCHOR>,
    LayoutDataBinding<Parenthesis, 16, ElementType::PARENTHESIS>
>;
} // namespace mu::engraving::rendering::score
