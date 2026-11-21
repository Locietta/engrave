/* SPDX-License-Identifier: GPL-3.0-only */
#include "layoutdatafactory.h"

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
namespace {
template <typename Action>
decltype(auto) dispatch_layout_data(ElementType type, Action &&action) {
    switch (type) {
        case ElementType::ACCIDENTAL: return action.template operator()<Accidental::LayoutData>();
        case ElementType::AMBITUS: return action.template operator()<Ambitus::LayoutData>();
        case ElementType::ARPEGGIO:
        case ElementType::CHORD_BRACKET: return action.template operator()<Arpeggio::LayoutData>();
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT: return action.template operator()<Articulation::LayoutData>();
        case ElementType::BAGPIPE_EMBELLISHMENT: return action.template operator()<BagpipeEmbellishment::LayoutData>();
        case ElementType::BAR_LINE: return action.template operator()<BarLine::LayoutData>();
        case ElementType::BEAM: return action.template operator()<Beam::LayoutData>();
        case ElementType::TREMOLO_TWOCHORD: return action.template operator()<TremoloTwoChord::LayoutData>();
        case ElementType::BEND: return action.template operator()<Bend::LayoutData>();
        case ElementType::BRACKET: return action.template operator()<Bracket::LayoutData>();
        case ElementType::DYNAMIC: return action.template operator()<Dynamic::LayoutData>();
        case ElementType::EXPRESSION: return action.template operator()<Expression::LayoutData>();
        case ElementType::CAPO:
        case ElementType::GUITAR_BEND_TEXT:
        case ElementType::HAMMER_ON_PULL_OFF_TEXT:
        case ElementType::HARP_DIAGRAM:
        case ElementType::INSTRUMENT_CHANGE:
        case ElementType::JUMP:
        case ElementType::LYRICS:
        case ElementType::MARKER:
        case ElementType::MEASURE_NUMBER:
        case ElementType::MMREST_RANGE:
        case ElementType::PLAY_COUNT_TEXT:
        case ElementType::PLAYTECH_ANNOTATION:
        case ElementType::REHEARSAL_MARK:
        case ElementType::STAFF_TEXT:
        case ElementType::STAVE_SHARING_LABEL:
        case ElementType::STICKING:
        case ElementType::STRING_TUNINGS:
        case ElementType::SYSTEM_TEXT:
        case ElementType::TAPPING_TEXT:
        case ElementType::TEMPO_TEXT:
        case ElementType::TEXT:
        case ElementType::TRIPLET_FEEL: return action.template operator()<TextBase::LayoutData>();
        case ElementType::CHORD: return action.template operator()<Chord::LayoutData>();
        case ElementType::CHORDLINE: return action.template operator()<ChordLine::LayoutData>();
        case ElementType::CLEF: return action.template operator()<Clef::LayoutData>();
        case ElementType::DEAD_SLAPPED: return action.template operator()<DeadSlapped::LayoutData>();
        case ElementType::FIGURED_BASS_ITEM: return action.template operator()<FiguredBassItem::LayoutData>();
        case ElementType::FIGURED_BASS: return action.template operator()<FiguredBass::LayoutData>();
        case ElementType::FINGERING: return action.template operator()<Fingering::LayoutData>();
        case ElementType::SLUR:
        case ElementType::TIE:
        case ElementType::PARTIAL_TIE:
        case ElementType::LAISSEZ_VIB:
        case ElementType::HAMMER_ON_PULL_OFF:
        case ElementType::TAPPING_HALF_SLUR: return action.template operator()<SlurTie::LayoutData>();
        case ElementType::FERMATA: return action.template operator()<Fermata::LayoutData>();
        case ElementType::VOLTA:
        case ElementType::OTTAVA:
        case ElementType::TRILL:
        case ElementType::LET_RING:
        case ElementType::GRADUAL_TEMPO_CHANGE:
        case ElementType::VIBRATO:
        case ElementType::PALM_MUTE:
        case ElementType::WHAMMY_BAR:
        case ElementType::RASGUEADO:
        case ElementType::HARMONIC_MARK:
        case ElementType::PICK_SCRAPE:
        case ElementType::TEXTLINE:
        case ElementType::PEDAL:
        case ElementType::LYRICSLINE:
        case ElementType::PARTIAL_LYRICSLINE:
        case ElementType::GLISSANDO:
        case ElementType::NOTELINE:
        case ElementType::GUITAR_BEND_HOLD:
        case ElementType::TEXTLINE_BASE: return action.template operator()<Spanner::LayoutData>();
        case ElementType::HAIRPIN: return action.template operator()<Hairpin::LayoutData>();
        case ElementType::FRET_DIAGRAM: return action.template operator()<FretDiagram::LayoutData>();
        case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT:
        case ElementType::HAIRPIN_SEGMENT:
        case ElementType::HARMONIC_MARK_SEGMENT:
        case ElementType::LET_RING_SEGMENT:
        case ElementType::NOTELINE_SEGMENT:
        case ElementType::OTTAVA_SEGMENT:
        case ElementType::PALM_MUTE_SEGMENT:
        case ElementType::PEDAL_SEGMENT:
        case ElementType::PICK_SCRAPE_SEGMENT:
        case ElementType::RASGUEADO_SEGMENT:
        case ElementType::TEXTLINE_SEGMENT:
        case ElementType::VOLTA_SEGMENT:
        case ElementType::WHAMMY_BAR_SEGMENT: return action.template operator()<TextLineBaseSegment::LayoutData>();
        case ElementType::GUITAR_BEND: return action.template operator()<GuitarBend::LayoutData>();
        case ElementType::GUITAR_BEND_SEGMENT: return action.template operator()<GuitarBendSegment::LayoutData>();
        case ElementType::GUITAR_BEND_HOLD_SEGMENT: return action.template operator()<GuitarBendHoldSegment::LayoutData>();
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT:
        case ElementType::SLUR_SEGMENT:
        case ElementType::TAPPING_HALF_SLUR_SEGMENT: return action.template operator()<SlurTieSegment::LayoutData>();
        case ElementType::HARMONY: return action.template operator()<Harmony::LayoutData>();
        case ElementType::INSTRUMENT_NAME: return action.template operator()<InstrumentName::LayoutData>();
        case ElementType::KEYSIG: return action.template operator()<KeySig::LayoutData>();
        case ElementType::LAISSEZ_VIB_SEGMENT: return action.template operator()<LaissezVibSegment::LayoutData>();
        case ElementType::LEDGER_LINE: return action.template operator()<LedgerLine::LayoutData>();
        case ElementType::LYRICSLINE_SEGMENT:
        case ElementType::PARTIAL_LYRICSLINE_SEGMENT: return action.template operator()<LyricsLineSegment::LayoutData>();
        case ElementType::HBOX:
        case ElementType::VBOX:
        case ElementType::TBOX:
        case ElementType::FBOX: return action.template operator()<MeasureBase::LayoutData>();
        case ElementType::SEGMENT: return action.template operator()<Segment::LayoutData>();
        case ElementType::MEASURE: return action.template operator()<Measure::LayoutData>();
        case ElementType::MEASURE_REPEAT: return action.template operator()<MeasureRepeat::LayoutData>();
        case ElementType::MMREST: return action.template operator()<MMRest::LayoutData>();
        case ElementType::NOTE: return action.template operator()<Note::LayoutData>();
        case ElementType::PAGE_LOCK_INDICATOR: return action.template operator()<PageLockIndicator::LayoutData>();
        case ElementType::PARTIAL_TIE_SEGMENT:
        case ElementType::TIE_SEGMENT: return action.template operator()<TieSegment::LayoutData>();
        case ElementType::REST: return action.template operator()<Rest::LayoutData>();
        case ElementType::TUPLET: return action.template operator()<Tuplet::LayoutData>();
        case ElementType::SPACER: return action.template operator()<Spacer::LayoutData>();
        case ElementType::STAFF_STATE: return action.template operator()<StaffState::LayoutData>();
        case ElementType::STAFF_VISIBILITY_INDICATOR:
        case ElementType::STAVE_SHARING_CHANGE:
        case ElementType::SYSTEM_LOCK_INDICATOR: return action.template operator()<IndicatorIcon::LayoutData>();
        case ElementType::STEM: return action.template operator()<Stem::LayoutData>();
        case ElementType::STEM_SLASH: return action.template operator()<StemSlash::LayoutData>();
        case ElementType::SYSTEM: return action.template operator()<System::LayoutData>();
        case ElementType::TAB_DURATION_SYMBOL: return action.template operator()<TabDurationSymbol::LayoutData>();
        case ElementType::TAPPING: return action.template operator()<Tapping::LayoutData>();
        case ElementType::TIMESIG: return action.template operator()<TimeSig::LayoutData>();
        case ElementType::TREMOLOBAR: return action.template operator()<TremoloBar::LayoutData>();
        case ElementType::TIME_TICK_ANCHOR: return action.template operator()<TimeTickAnchor::LayoutData>();
        case ElementType::PARENTHESIS: return action.template operator()<Parenthesis::LayoutData>();
        default: return action.template operator()<EngravingItem::LayoutData>();
    }
}
} // namespace

std::unique_ptr<EngravingItem::LayoutData> make_layout_data(ElementType type) {
    return dispatch_layout_data(type,
                                []<typename Data>() -> std::unique_ptr<EngravingItem::LayoutData> { return std::make_unique<Data>(); });
}

const EngravingItem::LayoutData &empty_layout_data(ElementType type) {
    return dispatch_layout_data(type, []<typename Data>() -> const EngravingItem::LayoutData & {
        static const Data empty;
        return empty;
    });
}
} // namespace mu::engraving::rendering::score
