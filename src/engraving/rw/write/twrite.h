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

#include "../xmlwriter.h"
#include "writecontext.h"

#include "../../dom/property.h"

namespace mu::engraving {
struct EngravingItem;
struct ElementList;

struct Accidental;
struct ActionIcon;
struct Ambitus;
struct Arpeggio;
struct Articulation;
struct Audio;

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
struct ChordRest;
struct ChordLine;
struct Clef;
struct Capo;

struct DurationElement;
struct Dynamic;
struct Expression;
struct Fermata;
struct FiguredBass;
struct FiguredBassItem;
struct Fingering;
struct FretDiagram;

struct Glissando;
struct GradualTempoChange;
struct Groups;
struct GuitarBend;
struct GuitarBendSegment;

struct Hairpin;
struct HammerOnPullOff;
struct HammerOnPullOffSegment;
struct HammerOnPullOffText;
struct Harmony;
struct HarmonicMark;
struct HarpPedalDiagram;
struct Hook;

struct Image;
struct Instrument;
struct InstrChannel;
struct InstrumentChange;
struct InstrumentLabel;

struct Jump;

struct KeySig;

struct LayoutBreak;
struct LedgerLine;
struct LetRing;
struct Location;
struct Lyrics;

struct Marker;
struct MeasureNumber;
struct MeasureRepeat;
struct MidiArticulation;
struct MMRest;
struct MMRestRange;

struct NamedEventList;
struct Note;
struct NoteEvent;
struct NoteDot;
struct NoteHead;
struct NoteLine;
struct Ornament;
struct Ottava;

struct Page;
struct PalmMute;
struct Part;
struct Pedal;
struct PickScrape;
struct PlayTechAnnotation;

struct Rasgueado;
struct RehearsalMark;
struct Rest;

struct Segment;
struct Slur;
struct SlurTie;
struct SlurTieSegment;
struct SLine;
struct Spanner;
struct Spacer;
struct Staff;
struct StaffLabel;
struct StaffState;
struct StaffText;
struct StaveSharingLabel;
struct StaffTextBase;
struct StaffType;
struct StaffTypeChange;
struct Stem;
struct StemSlash;
struct Sticking;
struct StringData;
struct Symbol;
struct BSymbol;
struct FSymbol;
struct StringTunings;
struct System;
struct SystemDivider;
struct SystemText;
struct SoundFlag;

struct Tapping;
struct TappingHalfSlur;
struct TempoText;
struct Text;
struct TextBase;
struct TextLine;
struct TextLineBase;
struct Tie;
struct TimeSig;
struct TremoloSingleChord;
struct TremoloTwoChord;
struct TremoloBar;
struct Trill;
struct Tuplet;

struct Vibrato;
struct Volta;

struct WhammyBar;
} // namespace mu::engraving

namespace mu::engraving::write {
struct TWrite {
    TWrite() = default;

    static void write_item(const EngravingItem *item, XmlWriter &xml, WriteContext &ctx);
    static void write_items(const ElementList &items, XmlWriter &xml, WriteContext &ctx);

    static void write(const Accidental *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const ActionIcon *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Ambitus *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Arpeggio *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const ChordBracket *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Articulation *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Audio *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const BagpipeEmbellishment *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const BarLine *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Beam *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Bend *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Box *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const HBox *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const VBox *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const FBox *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const TBox *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Bracket *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Breath *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Chord *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const ChordLine *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Clef *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Capo *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Dynamic *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Expression *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Fermata *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const FiguredBass *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const FiguredBassItem *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Fingering *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const FretDiagram *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Glissando *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const GradualTempoChange *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Groups *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const GuitarBend *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Hairpin *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const HammerOnPullOff *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Harmony *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const HarmonicMark *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const HarpPedalDiagram *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Hook *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Image *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Instrument *item, XmlWriter &xml, WriteContext &ctx, const Part *part);
    static void write(const InstrChannel *item, XmlWriter &xml, const Part *part);
    static void write(const InstrumentChange *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Jump *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const KeySig *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const LaissezVib *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const LayoutBreak *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const LedgerLine *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const LetRing *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Location *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Lyrics *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const LyricsLine *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Marker *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const MeasureNumber *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const MeasureRepeat *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const MidiArticulation *item, XmlWriter &xml);
    static void write(const MMRest *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const MMRestRange *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const NamedEventList *item, XmlWriter &xml, const AsciiStringView &n);
    static void write(const Note *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const NoteEvent *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const NoteDot *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const NoteHead *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const NoteLine *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Ornament *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Ottava *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Page *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const PalmMute *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Parenthesis *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Part *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const SharedPart *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const PartialTie *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const PartialLyricsLine *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Pedal *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const PickScrape *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const PlayCountText *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const PlayTechAnnotation *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Rasgueado *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const RehearsalMark *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Rest *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Segment *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Slur *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Spacer *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Staff *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const StaffState *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const StaffText *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const StaveSharingLabel *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const StaffType *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const StaffTypeChange *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Stem *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const StemSlash *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Sticking *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const StringData *item, XmlWriter &xml);
    static void write(const StringTunings *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Symbol *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const FSymbol *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const System *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const SystemDivider *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const SystemText *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const SoundFlag *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Tapping *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const TappingHalfSlur *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const TempoText *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Text *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const TextLine *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Tie *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const TimeSig *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const TremoloSingleChord *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const TremoloTwoChord *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const TremoloBar *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Trill *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Tuplet *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const Vibrato *item, XmlWriter &xml, WriteContext &ctx);
    static void write(const Volta *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const WhammyBar *item, XmlWriter &xml, WriteContext &ctx);

    static void write_segments(XmlWriter &xml, WriteContext &ctx, track_idx_t st, track_idx_t et, Segment *sseg, Segment *eseg, bool, bool);
    static void write_segments(XmlWriter &xml, WriteContext &ctx, track_idx_t st, track_idx_t et, const std::vector<Segment *> &segments,
                               bool, bool);

    static void write_property(const EngravingItem *item, XmlWriter &xml, Pid pid, bool force = false);

    static void write_page_locks(const Score *score, XmlWriter &xml);
    static void write_system_locks(const Score *score, XmlWriter &xml);
    static void write_system_dividers(const Score *score, XmlWriter &xml, WriteContext &ctx);

    static void write_score_spanners(const Score *score, XmlWriter &xml, WriteContext &ctx);
    static void write_score_spanners(const Score *score, track_idx_t start_track, track_idx_t end_track, Segment *start_segment,
                                     Segment *end_segment, XmlWriter &xml, WriteContext &ctx);

    static void write_item_eid(const EngravingObject *item, XmlWriter &xml);
    static void write_item_link(const EngravingObject *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const StaffLabel &item, XmlWriter &xml);
    static void write(const InstrumentLabel &item, XmlWriter &xml);

private:

    static void write_styled_properties(const EngravingItem *item, XmlWriter &xml);

    static void write_item_properties(const EngravingItem *item, XmlWriter &xml, WriteContext &ctx);
    static void write_box_properties(const Box *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const Articulation *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const Arpeggio *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const Box *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const HBox *item, XmlWriter &xml, WriteContext &ctx);

    static void write_properties(const ChordRest *item, XmlWriter &xml, WriteContext &ctx);
    static void write_chord_rest_beam(const ChordRest *item, XmlWriter &xml, WriteContext &ctx);

    static void write_properties(const Part *part, XmlWriter &xml, WriteContext &ctx);

    static void write_properties(const Rest *item, XmlWriter &xml, WriteContext &ctx);

    static void write(const StaffTextBase *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const StaffTextBase *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const SlurTie *item, XmlWriter &xml, WriteContext &ctx);
    static void write_slur(const SlurTieSegment *seg, XmlWriter &xml, WriteContext &ctx, int no);

    static void write_properties(const HammerOnPullOffSegment *seg, XmlWriter &xml, WriteContext &ctx);
    static void write(const HammerOnPullOffText *item, XmlWriter &xml, WriteContext &ctx, size_t idx);

    static void write_properties(const SLine *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const GuitarBendSegment *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const Spanner *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const BSymbol *item, XmlWriter &xml, WriteContext &ctx);

    static void write_properties(const TextLineBase *item, XmlWriter &xml, WriteContext &ctx);
    static void write_properties(const TextBase *item, XmlWriter &xml, WriteContext &ctx, bool write_text);

    static void write_tuplet_start(DurationElement *item, XmlWriter &xml, WriteContext &ctx);
    static void write_tuplet_end(DurationElement *item, XmlWriter &xml, WriteContext &ctx);

    static void write_page_lock(const RangeLock *page_lock, XmlWriter &xml);
    static void write_system_lock(const RangeLock *system_lock, XmlWriter &xml);

    static muse::String line_break_to_tag(const String &str);
    static void write_properties(const StaffLabel &item, XmlWriter &xml);
    static void write_properties(const InstrumentLabel &item, XmlWriter &xml);

    static void write(const BracketItem *item, XmlWriter &xml);
};
} // namespace mu::engraving::write
