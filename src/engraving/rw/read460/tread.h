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

#include "global/types/string.h"

#include "../xmlreader.h"
#include "readcontext.h"

#include "../../dom/property.h"

namespace mu::engraving {
struct XmlReader;
struct EngravingItem;
struct TextBase;
struct TempoText;
struct Dynamic;
struct Expression;
struct FretDiagram;
struct Sticking;
struct SystemText;
struct RehearsalMark;
struct Fermata;
struct Image;
struct Ambitus;
struct Accidental;
struct MMRestRange;
struct ActionIcon;
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
struct BracketItem;
struct Breath;

struct Chord;
struct ChordBracket;
struct ChordRest;
struct ChordLine;
struct Clef;
struct Capo;

struct Excerpt;

struct FiguredBass;
struct FiguredBassItem;
struct Fingering;

struct Glissando;
struct GradualTempoChange;
struct Groups;
struct GuitarBend;
struct GuitarBendSegment;
struct GuitarBendHold;

struct Hairpin;
struct HammerOnPullOff;
struct HammerOnPullOffSegment;
struct Harmony;
struct HarmonicMark;
struct HarpPedalDiagram;
struct Hook;

struct Instrument;
struct InstrChannel;
struct InstrumentChange;
struct InstrumentLabel;

struct Jump;

struct KeyList;
struct KeySig;

struct LayoutBreak;
struct LaissezVib;
struct LedgerLine;
struct LetRing;
struct LineSegment;
struct Location;
struct Lyrics;
struct LyricsLine;

struct Marker;
struct MeasureBase;
struct MeasureNumber;
struct MeasureNumberBase;
struct MeasureRepeat;
struct MidiArticulation;
struct MMRest;

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
struct Parenthesis;
struct Part;
struct PartialLyricsLine;
struct PartialTie;
struct Pedal;
struct PlayCountText;
struct PlayTechAnnotation;

struct Rasgueado;
struct Rest;

struct Segment;
struct SLine;
struct Slur;
struct SlurTie;
struct SlurTieSegment;
struct Spanner;
struct Spacer;
struct Staff;
struct StaffLabel;
struct StaffState;
struct StaffText;
struct StaffTextBase;
struct StaffType;
struct StaffTypeChange;
struct Stem;
struct StemSlash;
struct StringData;
struct StringTunings;
struct System;
struct SystemDivider;
struct Symbol;
struct SoundFlag;
struct BSymbol;
struct FSymbol;

struct Tapping;
struct TappingHalfSlur;
struct Text;
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
} // namespace mu::engraving

namespace mu::engraving::compat {
struct TremoloCompat;
} // namespace mu::engraving::compat

namespace mu::engraving::read460 {
struct TRead {
    TRead() = default;

    // factory
    static void read_item(EngravingItem *el, XmlReader &xml, ReadContext &ctx);

    // types

    static void read(TextBase *t, XmlReader &xml, ReadContext &ctx);
    static void read(TempoText *t, XmlReader &xml, ReadContext &ctx);
    static void read(Dynamic *d, XmlReader &xml, ReadContext &ctx);
    static void read(SystemText *t, XmlReader &xml, ReadContext &ctx);
    static void read(RehearsalMark *m, XmlReader &xml, ReadContext &ctx);
    static void read(Image *i, XmlReader &xml, ReadContext &ctx);
    static void read(Ambitus *a, XmlReader &xml, ReadContext &ctx);
    static void read(Accidental *a, XmlReader &xml, ReadContext &ctx);
    static void read(Jump *j, XmlReader &xml, ReadContext &ctx);
    static void read(MMRestRange *r, XmlReader &xml, ReadContext &ctx);
    static void read(ActionIcon *i, XmlReader &xml, ReadContext &ctx);
    static void read(Arpeggio *a, XmlReader &xml, ReadContext &ctx);
    static void read(ChordBracket *b, XmlReader &e, ReadContext &ctx);
    static void read(Articulation *a, XmlReader &xml, ReadContext &ctx);
    static void read(Audio *a, XmlReader &xml, ReadContext &ctx);

    static void read(BagpipeEmbellishment *b, XmlReader &xml, ReadContext &ctx);
    static void read(BarLine *l, XmlReader &xml, ReadContext &ctx);
    static void read(Beam *b, XmlReader &xml, ReadContext &ctx);
    static void read(Bend *b, XmlReader &xml, ReadContext &ctx);
    static void read(Box *b, XmlReader &xml, ReadContext &ctx);
    static void read(HBox *b, XmlReader &xml, ReadContext &ctx);
    static void read(VBox *b, XmlReader &xml, ReadContext &ctx);
    static void read(FBox *b, XmlReader &xml, ReadContext &ctx);
    static void read(TBox *b, XmlReader &xml, ReadContext &ctx);
    static void read(Bracket *b, XmlReader &xml, ReadContext &ctx);
    static void read(Breath *b, XmlReader &xml, ReadContext &ctx);

    static void read(Chord *ch, XmlReader &xml, ReadContext &ctx);
    static void read(ChordLine *l, XmlReader &xml, ReadContext &ctx);
    /// @param isHeader receives whether the clef belongs in a header clef segment
    static void read(Clef *c, XmlReader &xml, ReadContext &ctx, bool *is_header = nullptr);
    static void read(Capo *c, XmlReader &xml, ReadContext &ctx);

    static void read(Excerpt *item, XmlReader &xml, ReadContext &ctx);
    static void read(Expression *item, XmlReader &xml, ReadContext &ctx);

    static void read(Fermata *f, XmlReader &xml, ReadContext &ctx);
    static void read(FiguredBass *b, XmlReader &xml, ReadContext &ctx);
    static void read(FiguredBassItem *i, XmlReader &xml, ReadContext &ctx);
    static void read(Fingering *f, XmlReader &xml, ReadContext &ctx);
    static void read(FretDiagram *d, XmlReader &xml, ReadContext &ctx);

    static void read(Glissando *g, XmlReader &xml, ReadContext &ctx);
    static void read(GradualTempoChange *c, XmlReader &xml, ReadContext &ctx);
    static void read(Groups *g, XmlReader &xml, ReadContext &ctx);
    static void read(GuitarBend *g, XmlReader &xml, ReadContext &ctx);
    static void read(GuitarBendHold *h, XmlReader &xml, ReadContext &ctx);

    static void read(Hairpin *h, XmlReader &xml, ReadContext &ctx);
    static void read(HammerOnPullOff *h, XmlReader &xml, ReadContext &ctx);
    static void read(Harmony *h, XmlReader &xml, ReadContext &ctx);
    static void read(HarpPedalDiagram *h, XmlReader &xml, ReadContext &ctx);
    static void read(HarmonicMark *h, XmlReader &xml, ReadContext &ctx);
    static void read(Hook *h, XmlReader &xml, ReadContext &ctx);

    static void read(Instrument *item, XmlReader &xml, ReadContext &ctx, Part *part);
    static void read(InstrChannel *item, XmlReader &e, ReadContext &ctx, Part *part, const InstrumentTrackId &instr_id);
    static void read(InstrumentChange *c, XmlReader &xml, ReadContext &ctx);

    static void read(KeyList *item, XmlReader &xml, ReadContext &ctx);
    static void read(KeySig *s, XmlReader &xml, ReadContext &ctx);

    static void read(LaissezVib *lv, XmlReader &xml, ReadContext &ctx);
    static void read(LayoutBreak *b, XmlReader &xml, ReadContext &ctx);
    static void read(LedgerLine *l, XmlReader &xml, ReadContext &ctx);
    static void read(LetRing *r, XmlReader &xml, ReadContext &ctx);
    static void read(LineSegment *l, XmlReader &xml, ReadContext &ctx);
    static void read(Location *l, XmlReader &xml, ReadContext &ctx);
    static void read(Lyrics *l, XmlReader &xml, ReadContext &ctx);
    static void read(LyricsLine *l, XmlReader &xml, ReadContext &ctx);

    static void read(Marker *m, XmlReader &xml, ReadContext &ctx);
    static void read(MeasureNumber *n, XmlReader &xml, ReadContext &ctx);
    static void read(MeasureNumberBase *b, XmlReader &xml, ReadContext &ctx);
    static void read(MeasureRepeat *r, XmlReader &xml, ReadContext &ctx);
    static void read(MidiArticulation *item, XmlReader &xml);
    static void read(MMRest *r, XmlReader &xml, ReadContext &ctx);

    static void read(Note *n, XmlReader &xml, ReadContext &ctx);
    static void read(NoteEvent *item, XmlReader &xml, ReadContext &ctx);
    static void read(NoteDot *d, XmlReader &xml, ReadContext &ctx);
    static void read(NoteHead *h, XmlReader &xml, ReadContext &ctx);
    static void read(NoteLine *nl, XmlReader &xml, ReadContext &ctx);
    static void read(Ornament *o, XmlReader &xml, ReadContext &ctx);
    static void read(Ottava *o, XmlReader &xml, ReadContext &ctx);

    static void read(Page *p, XmlReader &xml, ReadContext &ctx);
    static void read(PalmMute *p, XmlReader &xml, ReadContext &ctx);
    static void read(Parenthesis *p, XmlReader &xml, ReadContext &ctx);
    static void read(Part *p, XmlReader &xml, ReadContext &ctx);
    static void read(PartialLyricsLine *p, XmlReader &xml, ReadContext &ctx);
    static void read(PartialTie *p, XmlReader &xml, ReadContext &ctx);
    static void read(Pedal *p, XmlReader &xml, ReadContext &ctx);
    static void read(PlayCountText *t, XmlReader &xml, ReadContext &ctx);
    static void read(PlayTechAnnotation *a, XmlReader &xml, ReadContext &ctx);

    static void read(Rasgueado *r, XmlReader &xml, ReadContext &ctx);
    static void read(Rest *r, XmlReader &xml, ReadContext &ctx);

    static void read(Segment *s, XmlReader &xml, ReadContext &ctx);
    static void read(SLine *l, XmlReader &xml, ReadContext &ctx);
    static void read(Slur *s, XmlReader &xml, ReadContext &ctx);
    static void read(SlurTie *s, XmlReader &xml, ReadContext &ctx);
    static void read(SlurTieSegment *s, XmlReader &xml, ReadContext &ctx);
    static void read(Spacer *s, XmlReader &xml, ReadContext &ctx);
    static void read(Staff *s, XmlReader &xml, ReadContext &ctx);
    static void read(StaffState *s, XmlReader &xml, ReadContext &ctx);
    static void read(StaffText *t, XmlReader &xml, ReadContext &ctx);
    static void read(StaffTextBase *t, XmlReader &xml, ReadContext &ctx);
    static void read(StaffType *t, XmlReader &xml, ReadContext &ctx);
    static void read(StaffTypeChange *c, XmlReader &xml, ReadContext &ctx);
    static void read(Sticking *s, XmlReader &xml, ReadContext &ctx);
    static void read(Stem *s, XmlReader &xml, ReadContext &ctx);
    static void read(StemSlash *s, XmlReader &xml, ReadContext &ctx);
    static void read(StringData *item, XmlReader &xml);
    static void read(StringTunings *s, XmlReader &xml, ReadContext &ctx);
    static void read(System *s, XmlReader &xml, ReadContext &ctx);
    static void read(SystemDivider *d, XmlReader &xml, ReadContext &ctx);
    static void read(Symbol *sym, XmlReader &xml, ReadContext &ctx);
    static void read(SoundFlag *sym, XmlReader &xml, ReadContext &ctx);
    static void read(FSymbol *sym, XmlReader &xml, ReadContext &ctx);

    static void read(Tapping *t, XmlReader &xml, ReadContext &ctx);
    static void read(TappingHalfSlur *t, XmlReader &xml, ReadContext &ctx);
    static void read(Text *t, XmlReader &xml, ReadContext &ctx);
    static void read(TextLine *l, XmlReader &xml, ReadContext &ctx);
    static void read(TextLineBase *b, XmlReader &xml, ReadContext &ctx);
    static void read(Tie *t, XmlReader &xml, ReadContext &ctx);
    static void read(TimeSig *s, XmlReader &xml, ReadContext &ctx);
    static void read(TremoloTwoChord *t, XmlReader &xml, ReadContext &ctx);
    static void read(TremoloSingleChord *t, XmlReader &xml, ReadContext &ctx);

    static void read(TremoloBar *b, XmlReader &xml, ReadContext &ctx);
    static void read(Trill *t, XmlReader &xml, ReadContext &ctx);
    static void read(Tuplet *t, XmlReader &xml, ReadContext &ctx);
    static void read(Vibrato *v, XmlReader &xml, ReadContext &ctx);
    static void read(Volta *v, XmlReader &xml, ReadContext &ctx);

    // compat
    static void read(compat::TremoloCompat *tc, XmlReader &xml, ReadContext &ctx);

    // temp compat

    static PropertyValue read_property_value(Pid type, XmlReader &e, ReadContext &ctx);
    static bool read_property(EngravingItem *item, const AsciiStringView &, XmlReader &, ReadContext &, Pid);
    static void read_property(EngravingItem *item, XmlReader &, ReadContext &, Pid);
    static bool read_styled_property(EngravingItem *item, const AsciiStringView &tag, XmlReader &xml, ReadContext &ctx);

    static bool read_item_properties(EngravingItem *item, XmlReader &xml, ReadContext &ctx);
    static bool read_box_properties(Box *b, XmlReader &xml, ReadContext &ctx);
    static bool read_text_properties(TextBase *t, XmlReader &xml, ReadContext &ctx);

    static bool read_properties(Ambitus *a, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Arpeggio *a, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Articulation *a, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(BSymbol *sym, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Chord *ch, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(ChordRest *ch, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Clef *c, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Fermata *f, XmlReader &xml, ReadContext &ctx);

    static bool read_properties(GuitarBendSegment *g, const AsciiStringView &tag, XmlReader &xml, ReadContext &);

    static bool read_properties(Instrument *item, XmlReader &xml, ReadContext &ctx, Part *part, bool *custom_drumset);

    static bool read_properties(LedgerLine *l, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(LineSegment *l, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Lyrics *l, XmlReader &xml, ReadContext &ctx);

    static bool read_properties(MMRest *r, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(MeasureNumberBase *r, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(MeasureBase *b, XmlReader &xml, ReadContext &ctx);

    static void read(NamedEventList *item, XmlReader &xml);
    static bool read_properties(Note *n, XmlReader &xml, ReadContext &ctx);

    static bool read_properties(Ornament *o, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Ottava *o, XmlReader &xml, ReadContext &ctx);

    static bool read_properties(Part *p, XmlReader &xml, ReadContext &ctx);

    static bool read_properties(SLine *l, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Slur *s, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(SlurTie *s, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Spanner *s, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(Staff *s, XmlReader &e, ReadContext &ctx);
    static bool read_properties(Stem *s, XmlReader &e, ReadContext &ctx);

    static bool read_properties(TextLineBase *b, XmlReader &e, ReadContext &ctx);
    static bool read_properties(Volta *v, XmlReader &e, ReadContext &ctx);

    static void read_spanner(XmlReader &e, ReadContext &ctx, EngravingItem *current, track_idx_t track);
    static void read_spanner(XmlReader &e, ReadContext &ctx, Score *current, track_idx_t track);

    static void read_system_locks(Score *score, XmlReader &e);
    static void read_system_dividers(Score *score, XmlReader &e, ReadContext &ctx);

    static void read_item_eid(EngravingObject *item, XmlReader &xml);
    static void read_item_link(EngravingItem *item, XmlReader &xml, ReadContext &ctx);

private:
    static bool read_properties(Box *b, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(HBox *b, XmlReader &xml, ReadContext &ctx);

    static bool read_properties(TextBase *t, XmlReader &xml, ReadContext &ctx);
    static bool read_properties(StaffTextBase *t, XmlReader &xml, ReadContext &ctx);

    static void read_system_lock(Score *score, XmlReader &e);

    static void read_hopo_text(HammerOnPullOffSegment *hopo_seg, XmlReader &xml, ReadContext &ctx, int idx);

    static String line_break_from_tag(const String &str);

    static void read_note_paren_group(Chord *ch, XmlReader &e, ReadContext &ctx);
};
} // namespace mu::engraving::read460
