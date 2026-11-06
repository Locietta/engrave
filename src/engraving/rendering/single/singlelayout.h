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
#ifndef MU_ENGRAVING_SINGLELAYOUT_H
#define MU_ENGRAVING_SINGLELAYOUT_H

#include <memory>

#include "dom/textbase.h"

namespace mu::engraving {
struct MStyle;
struct IEngravingFont;
struct EngravingItem;
struct Score;

struct Accidental;
struct ActionIcon;
struct Ambitus;
struct Arpeggio;
struct Articulation;

struct BagpipeEmbellishment;
struct BarLine;
struct Bend;
struct Bracket;
struct Breath;

struct Capo;
struct Chord;
struct ChordLine;
struct Clef;

struct DummyParent;
struct Dynamic;

struct Expression;

struct Fermata;
struct Fingering;
struct FretDiagram;
struct FSymbol;

struct Glissando;
struct GlissandoSegment;
struct GradualTempoChange;
struct GradualTempoChangeSegment;

struct Hairpin;
struct HairpinSegment;
struct HarpPedalDiagram;

struct Image;
struct InstrumentChange;

struct Jump;

struct KeySig;

struct LayoutBreak;
struct LetRing;
struct LetRingSegment;
struct SLine;
struct LineSegment;
struct Lyrics;

struct Marker;
struct MeasureNumber;
struct MeasureRepeat;

struct NoteHead;
struct NoteLine;

struct Ornament;
struct Ottava;
struct OttavaSegment;

struct PalmMute;
struct PalmMuteSegment;
struct Pedal;
struct PedalSegment;
struct PlayCountText;
struct PlayTechAnnotation;

struct RehearsalMark;

struct Slur;
struct SoundFlag;
struct Spacer;
struct StaffText;
struct StaveSharingLabel;
struct StaffTypeChange;
struct Sticking;
struct StringTunings;
struct Symbol;
struct SystemText;

struct TempoText;
struct Text;
struct TextLine;
struct TextLineSegment;
struct TextLineBaseSegment;
struct TimeSig;
struct TremoloSingleChord;
struct TremoloTwoChord;
struct TremoloBar;
struct Trill;
struct TrillSegment;

struct Vibrato;
struct VibratoSegment;
struct Volta;
struct VoltaSegment;
} // namespace mu::engraving

namespace mu::engraving::rendering::single {
struct SingleLayout {
    SingleLayout() = default;

    static void layout_item(engraving::EngravingItem *item);

    struct Context {
        Context(engraving::Score *s) : score_(s) {}

        const engraving::MStyle &style() const;
        std::shared_ptr<engraving::IEngravingFont> engraving_font() const;
        engraving::DummyParent *dummy_parent() const;

        //! NOTE Temporarily, do not use
        engraving::Score *dont_use_score() const { return score_; }

    private:
        engraving::Score *score_ = nullptr;
    };

    static void layout(Accidental *item, const Context &ctx);
    static void layout(ActionIcon *item, const Context &ctx);
    static void layout(Ambitus *item, const Context &ctx);
    static void layout(Arpeggio *item, const Context &ctx);
    static void layout(ChordBracket *item, const Context &ctx);
    static void layout(Articulation *item, const Context &ctx);

    static void layout(BagpipeEmbellishment *item, const Context &ctx);
    static void layout(BarLine *item, const Context &ctx);
    static void layout(Bend *item, const Context &ctx);
    static void layout(Bracket *item, const Context &ctx);
    static void layout(Breath *item, const Context &);

    static void layout(Capo *item, const Context &ctx);
    static void layout(Chord *item, const Context &ctx);
    static void layout(ChordLine *item, const Context &ctx);
    static void layout(Clef *item, const Context &ctx);

    static void layout(Dynamic *item, const Context &ctx);

    static void layout(Expression *item, const Context &ctx);

    static void layout(Fermata *item, const Context &ctx);
    static void layout(Fingering *item, const Context &ctx);
    static void layout(FretDiagram *item, const Context &ctx);
    static void layout(FSymbol *item, const Context &ctx);

    static void layout(Glissando *item, const Context &ctx);
    static void layout(GradualTempoChange *item, const Context &ctx);
    static void layout(GuitarBend *item, const Context &ctx);

    static void layout(Hairpin *item, const Context &ctx);
    static void layout(HammerOnPullOff *item, const Context &ctx);
    static void layout(HammerOnPullOffSegment *item, const Context &ctx);
    static void layout(HarpPedalDiagram *item, const Context &ctx);

    static void layout(Image *item, const Context &ctx);
    static void layout(InstrumentChange *item, const Context &ctx);

    static void layout(Jump *item, const Context &ctx);

    static void layout(KeySig *item, const Context &ctx);

    static void layout(LayoutBreak *item, const Context &ctx);
    static void layout(LetRing *item, const Context &ctx);
    static void layout(Lyrics *item, const Context &ctx);

    static void layout(NoteHead *item, const Context &ctx);
    static void layout(NoteLine *item, const Context &ctx);

    static void layout(Marker *item, const Context &ctx);
    static void layout(MeasureNumber *item, const Context &ctx);
    static void layout(MeasureRepeat *item, const Context &ctx);

    static void layout(Ornament *item, const Context &ctx);
    static void layout(Ottava *item, const Context &ctx);

    static void layout(PalmMute *item, const Context &ctx);
    static void layout(Pedal *item, const Context &ctx);
    static void layout(PlayCountText *item, const Context &ctx);
    static void layout(PlayTechAnnotation *item, const Context &ctx);

    static void layout(RehearsalMark *item, const Context &ctx);

    static void layout(Slur *item, const Context &ctx);
    static void layout(SoundFlag *item, const Context &ctx);
    static void layout(Spacer *item, const Context &);
    static void layout(StaffText *item, const Context &ctx);
    static void layout(StaveSharingLabel *item, const Context &ctx);
    static void layout(StaffTypeChange *item, const Context &ctx);
    static void layout(Stem *item, const Context &ctx);
    static void layout(Sticking *item, const Context &ctx);
    static void layout(StringTunings *item, const Context &ctx);
    static void layout(Symbol *item, const Context &ctx);
    static void layout(SystemText *item, const Context &ctx);

    static void layout(Tapping *item, const Context &ctx);
    static void layout(TempoText *item, const Context &ctx);
    static void layout(TextLine *item, const Context &ctx);
    static void layout(TimeSig *item, const Context &ctx);
    static void layout(TremoloSingleChord *item, const Context &ctx);
    static void layout(TremoloTwoChord *item, const Context &ctx);
    static void layout(TremoloBar *item, const Context &ctx);
    static void layout(Trill *item, const Context &ctx);

    static void layout(Vibrato *item, const Context &ctx);
    static void layout(Volta *item, const Context &ctx);
    static void layout(WhammyBar *item, const Context &ctx);

private:
    static void layout(GlissandoSegment *item, const Context &ctx);
    static void layout(GradualTempoChangeSegment *item, const Context &ctx);
    static void layout(GuitarBendSegment *item, const Context &ctx);
    static void layout(HairpinSegment *item, const Context &ctx);
    static void layout(LetRingSegment *item, const Context &ctx);
    static void layout(OttavaSegment *item, const Context &ctx);
    static void layout(PalmMuteSegment *item, const Context &ctx);
    static void layout(PedalSegment *item, const Context &ctx);
    static void layout(TextLineSegment *item, const Context &ctx);
    static void layout(TrillSegment *item, const Context &ctx);
    static void layout(VibratoSegment *item, const Context &ctx);
    static void layout(VoltaSegment *item, const Context &ctx);
    static void layout(WhammyBarSegment *item, const Context &ctx);

    static void layout(Text *item, const Context &ctx);
    static void layout_text_base(const TextBase *item, const Context &ctx, TextBase::LayoutData *ldata);
    static void layout1_text_base(const TextBase *item, const Context &ctx, TextBase::LayoutData *ldata);

    static void layout_line(SLine *item, const Context &ctx);
    static void layout_line_segment(LineSegment *item, const Context &ctx);
    static void layout_text_line_base_segment(TextLineBaseSegment *item, const Context &ctx);

    static void layout_group_bracket(Bracket *item, const Context &ctx);
};
} // namespace mu::engraving::rendering::single

#endif // MU_ENGRAVING_SINGLELAYOUT_H
