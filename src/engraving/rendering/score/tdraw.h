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

#include "dom/engravingitem.h"

namespace mu::engraving {
struct Accidental;
struct Ambitus;
struct Arpeggio;
struct Articulation;

struct BagpipeEmbellishment;
struct BarLine;
struct Beam;
struct Bend;

struct Box;
struct HBox;
struct VBox;
struct FBox;
struct TBox;

struct Bracket;
struct Breath;

struct Chord;
struct ChordBracket;
struct ChordLine;
struct Clef;
struct Capo;

struct DeadSlapped;
struct Dynamic;

struct Expression;

struct Fermata;
struct FiguredBassItem;
struct FiguredBass;
struct Fingering;
struct FretDiagram;

struct Glissando;
struct GlissandoSegment;
struct GraceNotesGroup;
struct GradualTempoChangeSegment;
struct GradualTempoChange;

struct HairpinSegment;
struct Hairpin;
struct HammerOnPullOffSegment;
struct HammerOnPullOffText;
struct HarpPedalDiagram;
struct HarmonicMarkSegment;
struct Harmony;
struct Hook;

struct IndicatorIcon;

struct Image;
struct InstrumentChange;
struct InstrumentName;
struct Jump;

struct KeySig;

struct LaissezVib;
struct LayoutBreak;
struct LedgerLine;
struct LetRing;
struct LetRingSegment;
struct LineSegment;
struct Lyrics;
struct LyricsLine;
struct LyricsLineSegment;

struct Marker;
struct MeasureBase;
struct MeasureNumber;
struct MeasureNumberBase;
struct MeasureRepeat;
struct MMRest;
struct MMRestRange;

struct Note;
struct NoteDot;
struct NoteLineSegment;

struct Ornament;

struct Ottava;
struct OttavaSegment;

struct PageLockIndicator;
struct PalmMute;
struct PalmMuteSegment;
struct Parenthesis;
struct Pedal;
struct PedalSegment;
struct PickScrapeSegment;
struct PlayCountText;
struct PlayTechAnnotation;

struct RasgueadoSegment;
struct RehearsalMark;
struct Rest;

struct SLine;
struct Slur;
struct Spacer;
struct SpannerSegment;
struct StaffLines;
struct StaffState;
struct StaffText;
struct StaffTypeChange;
struct Stem;
struct StemSlash;
struct Sticking;
struct StringTunings;

struct BSymbol;
struct Symbol;
struct FSymbol;

struct SystemDivider;
struct SystemText;
struct SoundFlag;

struct TabDurationSymbol;
struct Tapping;
struct TempoText;
struct TextBase;
struct TextBlock;
struct TextFragment;
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

// dev
struct System;
struct Measure;
struct Segment;
struct Chord;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct TDraw {

    static void draw_item(const EngravingItem *item, muse::draw::Painter *painter, const PaintOptions &opt); // factory

private:
    static void draw(const Accidental *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Ambitus *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Arpeggio *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const ChordBracket *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Articulation *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const BagpipeEmbellishment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const BarLine *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Beam *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Bend *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Box *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const HBox *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const VBox *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const FBox *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TBox *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Bracket *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Breath *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const ChordLine *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Clef *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Capo *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const DeadSlapped *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Dynamic *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Expression *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Fermata *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const FiguredBass *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const FiguredBassItem *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Fingering *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const FretDiagram *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const GlissandoSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const GradualTempoChangeSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const GuitarBendSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const GuitarBendHoldSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const HairpinSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const HammerOnPullOffSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const HammerOnPullOffText *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const HarpPedalDiagram *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const HarmonicMarkSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Harmony *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Hook *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Image *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const InstrumentChange *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const InstrumentName *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Jump *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const KeySig *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const LaissezVibSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const LayoutBreak *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const LedgerLine *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const LetRingSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Lyrics *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const LyricsLineSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Marker *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const MeasureNumber *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const MeasureRepeat *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const MMRest *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const MMRestRange *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Note *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const NoteDot *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const NoteHead *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const NoteLineSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Ornament *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const OttavaSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const PageLockIndicator *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Parenthesis *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PartialTieSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PalmMuteSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PedalSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PickScrapeSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PlayCountText *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PlayTechAnnotation *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const RasgueadoSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const RehearsalMark *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Rest *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw_debug_gap_rest(const Rest *item, muse::draw::Painter *painter);

    static void draw(const SlurSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Spacer *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const StaffLines *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const StaffState *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const StaffText *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const StaveSharingLabel *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const StaffTypeChange *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Stem *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const StemSlash *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Sticking *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const StringTunings *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Symbol *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const FSymbol *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const SystemDivider *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const SystemText *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const IndicatorIcon *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const SoundFlag *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const TabDurationSymbol *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Tapping *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TempoText *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Text *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TextLineSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TieSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TimeSig *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TimeTickAnchor *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TremoloSingleChord *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TremoloTwoChord *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TremoloBar *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TrillSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TripletFeel *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Tuplet *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const VibratoSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const VoltaSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const WhammyBarSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw_text_base(const TextBase *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TextBlock &text_block, const TextBase *item, muse::draw::Painter *painter);
    static void draw(const TextFragment &text_fragment, const TextBase *item, muse::draw::Painter *painter);
    static void draw_text_line_base_segment(const TextLineBaseSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void set_mask(const EngravingItem *item, muse::draw::Painter *painter);
};
} // namespace mu::engraving::rendering::score
