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

#include "../paintoptions.h"

namespace muse::draw {
struct Painter;
} // namespace muse::draw

namespace mu::engraving {
struct MStyle;
struct IEngravingFont;
struct Score;
struct EngravingItem;

struct Accidental;
struct ActionIcon;
struct Ambitus;
struct Arpeggio;
struct Articulation;

struct BagpipeEmbellishment;
struct BarLine;
struct Beam;
struct Bend;
struct Bracket;
struct Breath;

struct Capo;
struct Chord;
struct ChordBracket;
struct ChordLine;
struct Clef;

struct DeadSlapped;
struct Dynamic;

struct Expression;

struct Fermata;
struct Fingering;
struct FiguredBass;
struct FiguredBassItem;
struct FretDiagram;
struct FSymbol;

struct Glissando;
struct GlissandoSegment;
struct GradualTempoChange;
struct GradualTempoChangeSegment;
struct GuitarBend;
struct GuitarBendSegment;

struct Hairpin;
struct HairpinSegment;
struct HammerOnPullOffSegment;
struct HammerOnPullOffText;
struct HarpPedalDiagram;
struct HarmonicMarkSegment;
struct Harmony;
struct Hook;

struct Image;

struct InstrumentChange;
struct InstrumentName;

struct Jump;

struct KeySig;

struct LayoutBreak;
struct LedgerLine;
struct LetRing;
struct LetRingSegment;
struct SLine;
struct LineSegment;
struct Lyrics;

struct Marker;
struct MeasureNumber;
struct MeasureRepeat;

struct Note;
struct NoteHead;
struct NoteLineSegment;

struct Ornament;
struct Ottava;
struct OttavaSegment;

struct PalmMute;
struct PalmMuteSegment;
struct Pedal;
struct PedalSegment;
struct PickScrapeSegment;
struct PlayCountText;
struct PlayTechAnnotation;

struct RasgueadoSegment;
struct RehearsalMark;
struct Rest;

struct ShadowNote;
struct Slur;
struct SlurSegment;
struct SoundFlag;
struct Spacer;
struct StaffLines;
struct StaffState;
struct StaffText;
struct StaveSharingLabel;
struct StaffTypeChange;
struct Stem;
struct StemSlash;
struct Sticking;
struct StringTunings;
struct Symbol;
struct SystemDivider;
struct SystemText;

struct TabDurationSymbol;
struct Tapping;
struct TempoText;
struct Text;
struct TextBase;
struct TextBlock;
struct TextFragment;
struct TextLine;
struct TextLineSegment;
struct TextLineBaseSegment;
struct TieSegment;
struct TimeSig;
struct TremoloSingleChord;
struct TremoloTwoChord;
struct TremoloBar;
struct Trill;
struct TrillSegment;
struct TripletFeel;
struct Tuplet;

struct Vibrato;
struct VibratoSegment;
struct Volta;
struct VoltaSegment;

struct WhammyBarSegment;
} // namespace mu::engraving

namespace mu::engraving::rendering::single {
struct SingleDraw {

    static void draw_item(const EngravingItem *item, muse::draw::Painter *painter, const PaintOptions &opt); // factory

private:
    static void draw(const Accidental *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const ActionIcon *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Ambitus *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Arpeggio *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const ChordBracket *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Articulation *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const BagpipeEmbellishment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const BarLine *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Beam *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Bend *item, muse::draw::Painter *painter, const PaintOptions &opt);
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

    static void draw(const LayoutBreak *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const LedgerLine *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const LetRingSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Lyrics *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Marker *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const MeasureNumber *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const MeasureRepeat *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Note *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const NoteHead *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const NoteLineSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Ornament *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const OttavaSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const PalmMuteSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PedalSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PickScrapeSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PlayCountText *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const PlayTechAnnotation *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const RasgueadoSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const RehearsalMark *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Rest *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const ShadowNote *item, muse::draw::Painter *painter, const PaintOptions &opt);
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
    static void draw(const SoundFlag *item, muse::draw::Painter *painter, const PaintOptions &opt);

    static void draw(const Tapping *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TempoText *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const Text *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TextLineSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TieSegment *item, muse::draw::Painter *painter, const PaintOptions &opt);
    static void draw(const TimeSig *item, muse::draw::Painter *painter, const PaintOptions &opt);
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
} // namespace mu::engraving::rendering::single
