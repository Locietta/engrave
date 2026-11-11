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
#include "twrite.h"

#include <charconv>
#include <limits>
#include <set>
#include <stdexcept>

#include "../../infrastructure/rtti.h"
#include "../../style/textstyle.h"
#include "../../types/symnames.h"
#include "../../types/typesconv.h"
#include "../xmlwriter.h"
#include "connectorinfowriter.h"
#include "dom/accidental.h"
#include "dom/ambitus.h"
#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/bagpembell.h"
#include "dom/barline.h"
#include "dom/beam.h"
#include "dom/bend.h"
#include "dom/box.h"
#include "dom/bracket.h"
#include "dom/breath.h"
#include "dom/bsymbol.h"
#include "dom/capo.h"
#include "dom/chord.h"
#include "dom/chordbracket.h"
#include "dom/chordline.h"
#include "dom/chordrest.h"
#include "dom/clef.h"
#include "dom/drumset.h"
#include "dom/dynamic.h"
#include "dom/expression.h"
#include "dom/factory.h"
#include "dom/fermata.h"
#include "dom/figuredbass.h"
#include "dom/fingering.h"
#include "dom/fret.h"
#include "dom/glissando.h"
#include "dom/gradualtempochange.h"
#include "dom/groups.h"
#include "dom/guitarbend.h"
#include "dom/hairpin.h"
#include "dom/hammeronpulloff.h"
#include "dom/harmonicmark.h"
#include "dom/harmony.h"
#include "dom/harppedaldiagram.h"
#include "dom/hook.h"
#include "dom/image.h"
#include "dom/imageStore.h"
#include "dom/instrchange.h"
#include "dom/instrument.h"
#include "dom/jump.h"
#include "dom/keysig.h"
#include "dom/laissezvib.h"
#include "dom/layoutbreak.h"
#include "dom/ledgerline.h"
#include "dom/letring.h"
#include "dom/linkedobjects.h"
#include "dom/location.h"
#include "dom/lyrics.h"
#include "dom/marker.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurenumber.h"
#include "dom/measurerepeat.h"
#include "dom/mmrest.h"
#include "dom/mmrestrange.h"
#include "dom/mscore.h"
#include "dom/note.h"
#include "dom/notedot.h"
#include "dom/noteline.h"
#include "dom/ornament.h"
#include "dom/ottava.h"
#include "dom/page.h"
#include "dom/palmmute.h"
#include "dom/parenthesis.h"
#include "dom/part.h"
#include "dom/partialtie.h"
#include "dom/pedal.h"
#include "dom/pickscrape.h"
#include "dom/playcounttext.h"
#include "dom/playtechannotation.h"
#include "dom/rasgueado.h"
#include "dom/rehearsalmark.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/sharedpart.h"
#include "dom/sig.h"
#include "dom/slur.h"
#include "dom/soundflag.h"
#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/staffstate.h"
#include "dom/stafftext.h"
#include "dom/stafftype.h"
#include "dom/stafftypechange.h"
#include "dom/stavesharinglabel.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/sticking.h"
#include "dom/stringdata.h"
#include "dom/stringtunings.h"
#include "dom/symbol.h"
#include "dom/system.h"
#include "dom/systemdivider.h"
#include "dom/systemtext.h"
#include "dom/tapping.h"
#include "dom/tempotext.h"
#include "dom/text.h"
#include "dom/textbase.h"
#include "dom/textline.h"
#include "dom/textlinebase.h"
#include "dom/tie.h"
#include "dom/timesig.h"
#include "dom/tremolobar.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/trill.h"
#include "dom/tuplet.h"
#include "dom/utils.h"
#include "dom/vibrato.h"
#include "dom/volta.h"
#include "dom/whammybar.h"
#include "editing/transpose.h"
#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "engraving/internal/engravingfont.h"
#include "log.h"
#include "types/types.h"
#include "writecontext.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::engraving::write;

void TWrite::write_item(const EngravingItem *item, XmlWriter &xml, WriteContext &ctx) {
    switch (item->type()) {
        case ElementType::ACCIDENTAL: write(item_cast<const Accidental *>(item), xml, ctx); break;
        case ElementType::AMBITUS: write(item_cast<const Ambitus *>(item), xml, ctx); break;
        case ElementType::ARPEGGIO: write(item_cast<const Arpeggio *>(item), xml, ctx); break;
        case ElementType::ARTICULATION: write(item_cast<const Articulation *>(item), xml, ctx); break;
        case ElementType::BAGPIPE_EMBELLISHMENT: write(item_cast<const BagpipeEmbellishment *>(item), xml, ctx); break;
        case ElementType::BAR_LINE: write(item_cast<const BarLine *>(item), xml, ctx); break;
        case ElementType::BEAM: write(item_cast<const Beam *>(item), xml, ctx); break;
        case ElementType::BEND: write(item_cast<const Bend *>(item), xml, ctx); break;
        case ElementType::HBOX: write(item_cast<const HBox *>(item), xml, ctx); break;
        case ElementType::VBOX: write(item_cast<const VBox *>(item), xml, ctx); break;
        case ElementType::FBOX: write(item_cast<const FBox *>(item), xml, ctx); break;
        case ElementType::TBOX: write(item_cast<const TBox *>(item), xml, ctx); break;
        case ElementType::BRACKET: write(item_cast<const Bracket *>(item), xml, ctx); break;
        case ElementType::BREATH: write(item_cast<const Breath *>(item), xml, ctx); break;
        case ElementType::CHORD: write(item_cast<const Chord *>(item), xml, ctx); break;
        case ElementType::CHORD_BRACKET: write(item_cast<const ChordBracket *>(item), xml, ctx); break;
        case ElementType::CHORDLINE: write(item_cast<const ChordLine *>(item), xml, ctx); break;
        case ElementType::CLEF: write(item_cast<const Clef *>(item), xml, ctx); break;
        case ElementType::CAPO: write(item_cast<const Capo *>(item), xml, ctx); break;
        case ElementType::DYNAMIC: write(item_cast<const Dynamic *>(item), xml, ctx); break;
        case ElementType::EXPRESSION: write(item_cast<const Expression *>(item), xml, ctx); break;
        case ElementType::FERMATA: write(item_cast<const Fermata *>(item), xml, ctx); break;
        case ElementType::FIGURED_BASS: write(item_cast<const FiguredBass *>(item), xml, ctx); break;
        case ElementType::FINGERING: write(item_cast<const Fingering *>(item), xml, ctx); break;
        case ElementType::FRET_DIAGRAM: write(item_cast<const FretDiagram *>(item), xml, ctx); break;
        case ElementType::GLISSANDO: write(item_cast<const Glissando *>(item), xml, ctx); break;
        case ElementType::GRADUAL_TEMPO_CHANGE: write(item_cast<const GradualTempoChange *>(item), xml, ctx); break;
        case ElementType::GUITAR_BEND: write(item_cast<const GuitarBend *>(item), xml, ctx); break;
        case ElementType::HAIRPIN: write(item_cast<const Hairpin *>(item), xml, ctx); break;
        case ElementType::HAMMER_ON_PULL_OFF: write(item_cast<const HammerOnPullOff *>(item), xml, ctx); break;
        case ElementType::HARMONY: write(item_cast<const Harmony *>(item), xml, ctx); break;
        case ElementType::HARMONIC_MARK: write(item_cast<const HarmonicMark *>(item), xml, ctx); break;
        case ElementType::HARP_DIAGRAM: write(item_cast<const HarpPedalDiagram *>(item), xml, ctx); break;
        case ElementType::HOOK: write(item_cast<const Hook *>(item), xml, ctx); break;
        case ElementType::IMAGE: write(item_cast<const Image *>(item), xml, ctx); break;
        case ElementType::INSTRUMENT_CHANGE: write(item_cast<const InstrumentChange *>(item), xml, ctx); break;
        case ElementType::JUMP: write(item_cast<const Jump *>(item), xml, ctx); break;
        case ElementType::KEYSIG: write(item_cast<const KeySig *>(item), xml, ctx); break;
        case ElementType::LAISSEZ_VIB: write(item_cast<const LaissezVib *>(item), xml, ctx); break;
        case ElementType::LAYOUT_BREAK: write(item_cast<const LayoutBreak *>(item), xml, ctx); break;
        case ElementType::LEDGER_LINE: write(item_cast<const LedgerLine *>(item), xml, ctx); break;
        case ElementType::LET_RING: write(item_cast<const LetRing *>(item), xml, ctx); break;
        case ElementType::LYRICS: write(item_cast<const Lyrics *>(item), xml, ctx); break;
        case ElementType::LYRICSLINE: write(item_cast<const LyricsLine *>(item), xml, ctx); break;
        case ElementType::MARKER: write(item_cast<const Marker *>(item), xml, ctx); break;
        case ElementType::MEASURE_NUMBER: write(item_cast<const MeasureNumber *>(item), xml, ctx); break;
        case ElementType::MEASURE_REPEAT: write(item_cast<const MeasureRepeat *>(item), xml, ctx); break;
        case ElementType::MMREST: write(item_cast<const MMRest *>(item), xml, ctx); break;
        case ElementType::MMREST_RANGE: write(item_cast<const MMRestRange *>(item), xml, ctx); break;
        case ElementType::NOTE: write(item_cast<const Note *>(item), xml, ctx); break;
        case ElementType::NOTEDOT: write(item_cast<const NoteDot *>(item), xml, ctx); break;
        case ElementType::NOTEHEAD: write(item_cast<const NoteHead *>(item), xml, ctx); break;
        case ElementType::NOTELINE: write(item_cast<const NoteLine *>(item), xml, ctx); break;
        case ElementType::ORNAMENT: write(item_cast<const Ornament *>(item), xml, ctx); break;
        case ElementType::OTTAVA: write(item_cast<const Ottava *>(item), xml, ctx); break;
        case ElementType::PAGE: write(item_cast<const Page *>(item), xml, ctx); break;
        case ElementType::PALM_MUTE: write(item_cast<const PalmMute *>(item), xml, ctx); break;
        case ElementType::PARENTHESIS: write(item_cast<const Parenthesis *>(item), xml, ctx); break;
        case ElementType::PARTIAL_LYRICSLINE: write(item_cast<const PartialLyricsLine *>(item), xml, ctx); break;
        case ElementType::PARTIAL_TIE: write(item_cast<const PartialTie *>(item), xml, ctx); break;
        case ElementType::PEDAL: write(item_cast<const Pedal *>(item), xml, ctx); break;
        case ElementType::PICK_SCRAPE: write(item_cast<const PickScrape *>(item), xml, ctx); break;
        case ElementType::PLAY_COUNT_TEXT: write(item_cast<const PlayCountText *>(item), xml, ctx); break;
        case ElementType::PLAYTECH_ANNOTATION: write(item_cast<const PlayTechAnnotation *>(item), xml, ctx); break;
        case ElementType::RASGUEADO: write(item_cast<const Rasgueado *>(item), xml, ctx); break;
        case ElementType::REHEARSAL_MARK: write(item_cast<const RehearsalMark *>(item), xml, ctx); break;
        case ElementType::REST: write(item_cast<const Rest *>(item), xml, ctx); break;
        case ElementType::SEGMENT: write(item_cast<const Segment *>(item), xml, ctx); break;
        case ElementType::SLUR: write(item_cast<const Slur *>(item), xml, ctx); break;
        case ElementType::SPACER: write(item_cast<const Spacer *>(item), xml, ctx); break;
        case ElementType::STAFF_STATE: write(item_cast<const StaffState *>(item), xml, ctx); break;
        case ElementType::STAFF_TEXT: write(item_cast<const StaffText *>(item), xml, ctx); break;
        case ElementType::STAVE_SHARING_LABEL: write(item_cast<const StaveSharingLabel *>(item), xml, ctx); break;
        case ElementType::STAFFTYPE_CHANGE: write(item_cast<const StaffTypeChange *>(item), xml, ctx); break;
        case ElementType::STEM: write(item_cast<const Stem *>(item), xml, ctx); break;
        case ElementType::STEM_SLASH: write(item_cast<const StemSlash *>(item), xml, ctx); break;
        case ElementType::STICKING: write(item_cast<const Sticking *>(item), xml, ctx); break;
        case ElementType::STRING_TUNINGS: write(item_cast<const StringTunings *>(item), xml, ctx); break;
        case ElementType::SYMBOL: write(item_cast<const Symbol *>(item), xml, ctx); break;
        case ElementType::FSYMBOL: write(item_cast<const FSymbol *>(item), xml, ctx); break;
        case ElementType::SYSTEM: write(item_cast<const System *>(item), xml, ctx); break;
        case ElementType::SYSTEM_DIVIDER: write(item_cast<const SystemDivider *>(item), xml, ctx); break;
        case ElementType::SYSTEM_TEXT: write(item_cast<const SystemText *>(item), xml, ctx); break;
        case ElementType::SOUND_FLAG: write(item_cast<const SoundFlag *>(item), xml, ctx); break;
        case ElementType::TAPPING: write(item_cast<const Tapping *>(item), xml, ctx); break;
        case ElementType::TEMPO_TEXT: write(item_cast<const TempoText *>(item), xml, ctx); break;
        case ElementType::TEXT: write(item_cast<const Text *>(item), xml, ctx); break;
        case ElementType::TEXTLINE: write(item_cast<const TextLine *>(item), xml, ctx); break;
        case ElementType::TIE: write(item_cast<const Tie *>(item), xml, ctx); break;
        case ElementType::TIMESIG: write(item_cast<const TimeSig *>(item), xml, ctx); break;
        case ElementType::TREMOLO_SINGLECHORD: write(item_cast<const TremoloSingleChord *>(item), xml, ctx); break;
        case ElementType::TREMOLO_TWOCHORD: write(item_cast<const TremoloTwoChord *>(item), xml, ctx); break;
        case ElementType::TREMOLOBAR: write(item_cast<const TremoloBar *>(item), xml, ctx); break;
        case ElementType::TRILL: write(item_cast<const Trill *>(item), xml, ctx); break;
        case ElementType::TUPLET: write(item_cast<const Tuplet *>(item), xml, ctx); break;
        case ElementType::VIBRATO: write(item_cast<const Vibrato *>(item), xml, ctx); break;
        case ElementType::VOLTA: write(item_cast<const Volta *>(item), xml, ctx); break;
        case ElementType::WHAMMY_BAR: write(item_cast<const WhammyBar *>(item), xml, ctx); break;
        default: {
            UNREACHABLE;
            LOGE() << "not implemented write for type: " << item->type_name();
        }
    }
}

void TWrite::write_items(const ElementList &items, XmlWriter &xml, WriteContext &ctx) {
    for (const EngravingItem *e : items) {
        write_item(e, xml, ctx);
    }
}

//-----------------------------------------------------------------------------
//   writeProperty
//
//    - styled properties are never written
//    - other properties are written if forced or if different from default value
//-----------------------------------------------------------------------------

void TWrite::write_property(const EngravingItem *item, XmlWriter &xml, Pid pid, bool force) {
    if (item->is_styled(pid)) {
        return;
    }
    PropertyValue p = item->get_property(pid);
    if (!p.is_valid()) {
        LOGD("%s invalid property %d <%s>", item->type_name(), int(pid), property_name(pid));
        return;
    }
    PropertyValue d = force ? PropertyValue() : item->property_default(pid);

    if (pid == Pid::FONT_STYLE) {
        FontStyle ds = FontStyle(d.is_valid() ? d.to_int() : 0);
        FontStyle fs = FontStyle(p.to_int());
        if ((fs & FontStyle::BOLD) != (ds & FontStyle::BOLD)) {
            xml.tag("bold", fs & FontStyle::BOLD);
        }
        if ((fs & FontStyle::ITALIC) != (ds & FontStyle::ITALIC)) {
            xml.tag("italic", fs & FontStyle::ITALIC);
        }
        if ((fs & FontStyle::UNDERLINE) != (ds & FontStyle::UNDERLINE)) {
            xml.tag("underline", fs & FontStyle::UNDERLINE);
        }
        if ((fs & FontStyle::STRIKE) != (ds & FontStyle::STRIKE)) {
            xml.tag("strike", fs & FontStyle::STRIKE);
        }
        return;
    }

    PType type = property_type(pid);
    if (PType::ABSOLUTE == type) {
        double f1 = p.to_real();
        if (d.is_valid() && std::abs(f1 - d.to_real()) < 0.0001) { // fuzzy compare
            return;
        }
        p = PropertyValue(Spatium::from_absolute(f1, item->spatium()));
        d = PropertyValue();
    } else if (PType::POINT == type) {
        PointF p1 = p.value<PointF>();
        if (d.is_valid()) {
            PointF p2 = d.value<PointF>();
            if ((std::abs(p1.x() - p2.x()) < 0.0001) && (std::abs(p1.y() - p2.y()) < 0.0001)) {
                return;
            }
        }
        double q = item->offset_is_spatium_dependent() ? item->style().spatium() : ::mu::engraving::dpmm;
        p = PropertyValue(p1 / q);
        d = PropertyValue();
    }
    xml.tag_property(pid, p, d);
}

void TWrite::write_page_locks(const Score *score, XmlWriter &xml) {
    std::vector<const RangeLock *> locks = score->page_locks()->all_locks();
    if (locks.empty()) {
        return;
    }

    xml.start_element("PageLocks");
    for (const RangeLock *sl : locks) {
        write_page_lock(sl, xml);
    }
    xml.end_element();
}

void TWrite::write_system_locks(const Score *score, XmlWriter &xml) {
    std::vector<const RangeLock *> locks = score->system_locks()->all_locks();
    if (locks.empty()) {
        return;
    }

    xml.start_element("SystemLocks");
    for (const RangeLock *sl : locks) {
        write_system_lock(sl, xml);
    }
    xml.end_element();
}

void TWrite::write_system_dividers(const Score *score, XmlWriter &xml, WriteContext &ctx) {
    std::vector<const System *> systems_to_write;
    for (const System *system : score->systems()) {
        bool write_system = (system->system_divider_left() && !system->system_divider_left()->generated()) ||
                            (system->system_divider_right() && !system->system_divider_right()->generated());
        if (write_system) {
            systems_to_write.push_back(system);
        }
    }

    if (systems_to_write.empty()) {
        return;
    }

    xml.start_element("SystemDividers");
    for (const System *system : systems_to_write) {
        xml.start_element("system", {{"idx", muse::index_of(score->systems(), system)}});
        SystemDivider *divider_left = system->system_divider_left();
        if (divider_left && !divider_left->generated()) {
            write(divider_left, xml, ctx);
        }
        SystemDivider *divider_right = system->system_divider_right();
        if (divider_right && !divider_right->generated()) {
            write(divider_right, xml, ctx);
        }
        xml.end_element();
    }
    xml.end_element();
}

void TWrite::write_score_spanners(const Score *score, XmlWriter &xml, WriteContext &ctx) {
    if (score->spanner_map().empty()) {
        return;
    }
    std::set<Spanner *> pending;
    for (const auto &i : score->spanner_map().map()) {
        Spanner *s = i.second;
        if (!s->generated() && ctx.can_write(s)) {
            pending.insert(s);
        }
    }
    xml.start_element("SpannerMap");
    for (const auto &i : score->spanner_map().map()) {
        Spanner *s = i.second;
        if (!pending.count(s)) {
            continue;
        }
        if (s->links()) {
            // The reader resolves linkedTo immediately, so write the eligible main first.
            Spanner *main = to_spanner(s->links()->main_element());
            if (pending.erase(main)) {
                TWrite::write_item(main, xml, ctx);
            }
        }
        if (pending.erase(s)) {
            TWrite::write_item(s, xml, ctx);
        }
    }
    xml.end_element();
}

void TWrite::write_score_spanners(const Score *score, track_idx_t start_track, track_idx_t end_track, Segment *start_segment,
                                  Segment *end_segment, XmlWriter &xml, WriteContext &ctx) {
    IF_ASSERT_FAILED(ctx.clipboardmode()) { LOGD() << "Should only be used in clipboard mode"; }

    std::vector<Spanner *> spanners_to_write;
    Fraction start_tick = start_segment->tick();
    Fraction end_tick = end_segment ? end_segment->tick() : score->end_tick();
    const SpannerMap::IntervalList &spanners = score->spanner_map().find_contained(start_tick.ticks(), end_tick.ticks());
    for (auto &i : spanners) {
        Spanner *s = i.value;
        if (s->generated() || s->track() < start_track || s->effective_track2() >= end_track || !ctx.can_write(s)) {
            continue;
        }
        // Do not write chordrest anchored spanners ending on the final tick of the region to the clipboard
        // The chordrest they are anchored to is outside of the selected region
        if (s->tick2() == end_tick && s->anchor() == Spanner::Anchor::CHORDREST) {
            continue;
        }
        // Don't write voltas to clipboard
        if (s->is_volta() && s->system_flag()) {
            continue;
        }
        spanners_to_write.push_back(s);
    }

    if (spanners_to_write.empty()) {
        return;
    }

    xml.start_element("SpannerMap");
    for (Spanner *s : spanners_to_write) {
        TWrite::write_item(s, xml, ctx);
    }
    xml.end_element();
}

void TWrite::write_item_eid(const EngravingObject *item, XmlWriter &xml) {
    EID eid = item->eid();
    if (!eid.is_valid()) {
        eid = item->assign_new_eid();
    }

    std::array<char, EID::max_str_size> buf{};
    const char *last = eid.to_chars(buf.data(), buf.data() + buf.size());
    const auto size = static_cast<size_t>(last - buf.data());

    xml.tag("eid", AsciiStringView{buf.data(), size});
}

void TWrite::write_item_link(const EngravingObject *item, XmlWriter &xml, WriteContext &ctx) {
    if (!item->links() || item->links()->size() <= 1 || ctx.clipboardmode()) {
        return;
    }

    if (ctx.standalone()) {
        const EID first = ctx.standalone_link(item->links(), item->eid());
        if (first != item->eid()) xml.tag("linkedTo", first.to_std_string());
        return;
    }

    EngravingItem *main_element = static_cast<EngravingItem *>(item->links()->main_element());
    if (main_element != item) {
        EID eid_of_main_element = main_element->eid();
        DO_ASSERT(eid_of_main_element.is_valid());
        xml.tag("linkedTo", eid_of_main_element.to_std_string());
    }
}

void TWrite::write_page_lock(const RangeLock *page_lock, XmlWriter &xml) {
    xml.start_element("pageLock");

    xml.tag("startMeasure", page_lock->start_mb()->eid().to_std_string());
    xml.tag("endMeasure", page_lock->end_mb()->eid().to_std_string());

    xml.end_element();
}

void TWrite::write_system_lock(const RangeLock *system_lock, XmlWriter &xml) {
    xml.start_element("systemLock");

    xml.tag("startMeasure", system_lock->start_mb()->eid().to_std_string());
    xml.tag("endMeasure", system_lock->end_mb()->eid().to_std_string());

    xml.end_element();
}

String TWrite::line_break_to_tag(const String &str) {
    // Raw newlines appearing next to tags (<font size="10> or <sym>...) get eaten by XML readers.
    String s = str;
    return s.replace(u"\n", u"<br/>");
}

void TWrite::write_styled_properties(const EngravingItem *item, XmlWriter &xml) {
    for (const StyledProperty &spp : *item->styled_properties()) {
        write_property(item, xml, spp.pid);
    }
}

void TWrite::write_item_properties(const EngravingItem *item, XmlWriter &xml, WriteContext &ctx) {
    write_item_eid(item, xml);

    bool autoplace_enabled = item->score()->style().style_b(Sid::AUTOPLACE_ENABLED);
    if (!autoplace_enabled) {
        item->score()->style().set(Sid::AUTOPLACE_ENABLED, true);
        write_property(item, xml, Pid::AUTOPLACE);
        item->score()->style().set(Sid::AUTOPLACE_ENABLED, autoplace_enabled);
    } else {
        write_property(item, xml, Pid::AUTOPLACE);
    }

    write_item_link(item, xml, ctx);

    bool write_spanner_track = item->is_spanner() && (to_spanner(item)->anchor() == Spanner::Anchor::NOTE ||
                                                      (to_spanner(item)->anchor() == Spanner::Anchor::SEGMENT && ctx.clipboardmode()));
    if ((item->track() != ctx.cur_track() && item->track() != muse::nidx && !item->is_beam() && !item->is_tuplet()) ||
        write_spanner_track) {
        // Writing track number for beams and tuplets is redundant as it is calculated
        // during layout.
        int t = static_cast<int>(item->track()) + ctx.track_diff();
        xml.tag("track", t);
    }

    for (Pid pid : {Pid::COLOR, Pid::VISIBLE, Pid::Z}) {
        if (item->property_flags(pid) == PropertyFlags::NOSTYLE) {
            write_property(item, xml, pid);
        }
    }

    if (!item->offset().is_null()) {
        write_property(item, xml, Pid::OFFSET);
    }

    if (!item->has_voice_assignment_properties() && item->property_flags(Pid::PLACEMENT) == PropertyFlags::NOSTYLE) {
        write_property(item, xml, Pid::PLACEMENT);
    }

    write_property(item, xml, Pid::POSITION_LINKED_TO_MASTER);
    write_property(item, xml, Pid::APPEARANCE_LINKED_TO_MASTER);
    write_property(item, xml, Pid::EXCLUDE_FROM_OTHER_PARTS);

    bool left_generated = item->left_paren() && item->left_paren()->generated();
    bool right_generated = item->right_paren() && item->right_paren()->generated();
    if (!left_generated || !right_generated) {
        write_property(item, xml, Pid::HAS_PARENTHESES);
    }
    if (item->left_paren() && !item->left_paren()->generated()) {
        write_item(item->left_paren(), xml, ctx);
    }
    if (item->right_paren() && !item->right_paren()->generated()) {
        write_item(item->right_paren(), xml, ctx);
    }
}

void TWrite::write(const Accidental *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::ACCIDENTAL_BRACKET);
    write_property(item, xml, Pid::ACCIDENTAL_ROLE);
    write_property(item, xml, Pid::SMALL);
    write_property(item, xml, Pid::ACCIDENTAL_TYPE);
    write_property(item, xml, Pid::ACCIDENTAL_STACKING_ORDER_OFFSET);
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Ambitus *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    xml.tag_property(Pid::HEAD_GROUP, item->note_head_group(), Ambitus::noteheadgroup_default);
    xml.tag_property(Pid::HEAD_TYPE, item->note_head_type(), Ambitus::noteheadtype_default);
    xml.tag_property(Pid::MIRROR_HEAD, item->direction(), Ambitus::direction_default);
    xml.tag("hasLine", item->has_line(), true);
    xml.tag_property(Pid::LINE_WIDTH, item->line_width(), Ambitus::linewidth_default);
    xml.tag("topPitch", item->top_pitch());
    xml.tag("topTpc", item->top_tpc());
    xml.tag("bottomPitch", item->bottom_pitch());
    xml.tag("bottomTpc", item->bottom_tpc());
    if (item->top_accidental()->accidental_type() != AccidentalType::NONE) {
        xml.start_element("topAccidental");
        write(item->top_accidental(), xml, ctx);
        xml.end_element();
    }
    if (item->bottom_accidental()->accidental_type() != AccidentalType::NONE) {
        xml.start_element("bottomAccidental");
        write(item->bottom_accidental(), xml, ctx);
        xml.end_element();
    }
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Arpeggio *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_property(item, xml, Pid::ARPEGGIO_TYPE);
    write_properties(item, xml, ctx);

    xml.end_element();
}

void TWrite::write(const ChordBracket *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);

    write_property(item, xml, Pid::BRACKET_HOOK_LEN);
    write_property(item, xml, Pid::BRACKET_HOOK_POS);
    write_property(item, xml, Pid::BRACKET_RIGHT_SIDE);

    write_properties(item, xml, ctx);

    xml.end_element();
}

void TWrite::write_properties(const Arpeggio *item, XmlWriter &xml, WriteContext &ctx) {
    write_item_properties(item, xml, ctx);
    if (!real_is_null(item->user_len1())) {
        xml.tag("userLen1", item->user_len1() / item->spatium());
    }
    if (!real_is_null(item->user_len2())) {
        xml.tag("userLen2", item->user_len2() / item->spatium());
    }
    if (item->span() != 1) {
        xml.tag("span", item->span());
    }
    write_property(item, xml, Pid::PLAY);
    write_property(item, xml, Pid::TIME_STRETCH);
}

void TWrite::write(const Articulation *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    if (item->is_ornament()) {
        write(to_ornament(item), xml, ctx);
        return;
    }
    xml.start_element(item);
    write_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write_properties(const Articulation *item, XmlWriter &xml, WriteContext &ctx) {
    if (!item->channel_name().is_empty()) {
        xml.tag("channe", {{"name", item->channel_name()}});
    }

    write_property(item, xml, Pid::DIRECTION);
    if (item->text_type() != ArticulationTextType::NO_TEXT) {
        xml.tag("subtype", TConv::to_xml(item->text_type()));
    } else if (!item->is_tapping()) {
        xml.tag("subtype", SymNames::name_for_sym_id(item->sym_id()));
    }

    write_property(item, xml, Pid::PLAY);
    write_property(item, xml, Pid::ORNAMENT_STYLE);
    write_styled_properties(item, xml);
    write_item_properties(item, xml, ctx);
}

void TWrite::write(const Ornament *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);

    if (item->cue_note_chord()) {
        write(item->cue_note_chord(), xml, ctx);
    } else {
        if (item->accidental_above()) {
            write(item->accidental_above(), xml, ctx);
        }
        if (item->accidental_below()) {
            write(item->accidental_below(), xml, ctx);
        }
    }

    write_property(item, xml, Pid::INTERVAL_ABOVE);
    write_property(item, xml, Pid::INTERVAL_BELOW);
    write_property(item, xml, Pid::ORNAMENT_SHOW_ACCIDENTAL);
    write_property(item, xml, Pid::ORNAMENT_SHOW_CUE_NOTE);
    write_property(item, xml, Pid::START_ON_UPPER_NOTE);
    write_properties(static_cast<const Articulation *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const BagpipeEmbellishment *item, XmlWriter &xml, WriteContext &) {
    xml.start_element(item);
    xml.tag("subtype", TConv::to_xml(item->embel_type()));
    xml.end_element();
}

void TWrite::write(const BarLine *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);

    write_property(item, xml, Pid::BARLINE_TYPE);
    write_property(item, xml, Pid::BARLINE_SPAN);
    write_property(item, xml, Pid::BARLINE_SPAN_FROM);
    write_property(item, xml, Pid::BARLINE_SPAN_TO);

    for (const EngravingItem *e : *item->el()) {
        write_item(e, xml, ctx);
    }

    if (ctx.clipboardmode() && item->measure()) {
        xml.tag("playCount", item->measure()->repeat_count());
    }

    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Beam *item, XmlWriter &xml, WriteContext &ctx) {
    if (item->elements().empty()) {
        return;
    }
    xml.start_element(item);
    write_item_properties(item, xml, ctx);

    write_property(item, xml, Pid::STEM_DIRECTION);
    write_property(item, xml, Pid::BEAM_NO_SLOPE);
    write_property(item, xml, Pid::GROW_LEFT);
    write_property(item, xml, Pid::GROW_RIGHT);
    write_property(item, xml, Pid::BEAM_CROSS_STAFF_MOVE);

    int idx = item->direction_idx();
    if (item->user_modified()) {
        double spatium_value = item->spatium();
        for (BeamFragment *f : item->beam_fragments()) {
            xml.start_element("Fragment");
            xml.tag("y1", f->py1[idx] / spatium_value);
            xml.tag("y2", f->py2[idx] / spatium_value);
            xml.end_element();
        }
    }

    // this info is used for regression testing
    // l1/l2 is the beam position of the layout engine
    if (MScore::test_mode) {
        double spatium8 = item->spatium() * .125;
        for (BeamFragment *f : item->beam_fragments()) {
            xml.tag("l1", int(lrint(f->py1[idx] / spatium8)));
            xml.tag("l2", int(lrint(f->py2[idx] / spatium8)));
        }
    }

    xml.end_element();
}

void TWrite::write(const Bend *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    for (const PitchValue &v : item->points()) {
        xml.tag("point", {{"time", v.time}, {"pitch", v.pitch}, {"vibrato", v.vibrato}});
    }
    write_styled_properties(item, xml);
    write_property(item, xml, Pid::PLAY);
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Box *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_box_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write_box_properties(const Box *item, XmlWriter &xml, WriteContext &ctx) {
    if (item->is_h_box()) {
        return write_properties(dynamic_cast<const HBox *>(item), xml, ctx);
    }
    return write_properties(item, xml, ctx);
}

void TWrite::write_properties(const Box *item, XmlWriter &xml, WriteContext &ctx) {
    for (Pid id : {Pid::BOX_HEIGHT, Pid::BOX_WIDTH, Pid::TOP_GAP, Pid::BOTTOM_GAP, Pid::LEFT_MARGIN, Pid::RIGHT_MARGIN, Pid::TOP_MARGIN,
                   Pid::BOTTOM_MARGIN, Pid::BOX_AUTOSIZE, Pid::SIZE_SPATIUM_DEPENDENT}) {
        bool force = ((item->is_v_box() || item->is_f_box()) && id == Pid::BOX_HEIGHT) || (item->is_h_box() && id == Pid::BOX_WIDTH);
        write_property(item, xml, id, force);
    }
    if (item->is_v_box_base()) {
        write_property(item, xml, Pid::PADDING_TO_NOTATION_ABOVE);
        write_property(item, xml, Pid::PADDING_TO_NOTATION_BELOW);
    }
    write_item_properties(item, xml, ctx);
    for (const EngravingItem *e : item->el()) {
        write_item(e, xml, ctx);
    }
}

void TWrite::write(const HBox *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write_properties(const HBox *item, XmlWriter &xml, WriteContext &ctx) {
    write_property(item, xml, Pid::CREATE_SYSTEM_HEADER);
    write_properties(static_cast<const Box *>(item), xml, ctx);
}

void TWrite::write(const VBox *item, XmlWriter &xml, WriteContext &ctx) { write(static_cast<const Box *>(item), xml, ctx); }

void TWrite::write(const FBox *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);

    write_property(item, xml, Pid::FRET_FRAME_TEXT_SCALE);
    write_property(item, xml, Pid::FRET_FRAME_DIAGRAM_SCALE);
    write_property(item, xml, Pid::FRET_FRAME_COLUMN_GAP);
    write_property(item, xml, Pid::FRET_FRAME_ROW_GAP);
    write_property(item, xml, Pid::FRET_FRAME_CHORDS_PER_ROW);
    write_property(item, xml, Pid::FRET_FRAME_H_ALIGN);

    write_properties(static_cast<const Box *>(item), xml, ctx);

    xml.end_element();
}

void TWrite::write(const TBox *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_properties(static_cast<const Box *>(item), xml, ctx);
    write(item->text(), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Bracket *item, XmlWriter &xml, WriteContext &ctx) {
    bool is_start_tag = false;
    switch (item->bracket_item()->bracket_type()) {
        case BracketType::BRACE:
        case BracketType::SQUARE:
        case BracketType::LINE:
        case BracketType::GROUP: {
            xml.start_element(item, {{"type", TConv::to_xml(item->bracket_item()->bracket_type())}});
            is_start_tag = true;
        } break;
        case BracketType::NORMAL:
            xml.start_element(item);
            is_start_tag = true;
            break;
        case BracketType::NO_BRACKET: break;
    }

    if (is_start_tag) {
        if (item->bracket_item()->column()) {
            xml.tag("level", static_cast<int>(item->bracket_item()->column()));
        }

        write_item_properties(item, xml, ctx);

        xml.end_element();
    }
}

void TWrite::write(const Breath *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_property(item, xml, Pid::SYMBOL);
    write_property(item, xml, Pid::PAUSE);
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Chord *item, XmlWriter &xml, WriteContext &ctx) {
    bool found_notes = false;
    const size_t note_count = item->notes().size();
    for (size_t note_idx = 0; note_idx < note_count; ++note_idx) {
        if (ctx.can_write_note_idx(note_idx, note_count)) {
            found_notes = true;
            break;
        }
    }
    if (!found_notes) {
        if (item->tuplet()) {
            // HACK: See PR #30178 - deselected chords still not fully handled in tuplets...
            Rest *dummy_rest = Factory::create_rest(item->segment());
            dummy_rest->set_duration_type(item->duration_type());
            dummy_rest->set_tuplet(item->tuplet());
            dummy_rest->set_ticks(item->ticks());
            dummy_rest->set_track(item->track());
            write(dummy_rest, xml, ctx);
            dummy_rest->delete_later();
        } else {
            // No writable notes found, write a Location instead (skip this chord)...

            const Fraction tick = item->tick();
            const track_idx_t track = item->track();

            Location curr = Location::absolute();
            curr.set_frac(ctx.cur_tick());
            curr.set_track(static_cast<int>(ctx.cur_track()));

            Location dest = Location::absolute();
            dest.set_frac(tick);
            dest.set_track(static_cast<int>(track));

            dest.to_relative(curr);
            TWrite::write(&dest, xml, ctx);

            ctx.set_cur_tick(tick);
            ctx.set_cur_track(track);
        }
        return;
    }

    for (Chord *ch : item->grace_notes()) {
        write(ch, xml, ctx);
    }
    write_chord_rest_beam(item, xml, ctx);
    xml.start_element(item);
    write_properties(to_chord_rest(item), xml, ctx);
    for (const Articulation *a : item->articulations()) {
        write_item(a, xml, ctx);
    }
    switch (item->note_type()) {
        case NoteType::NORMAL: break;
        case NoteType::ACCIACCATURA: xml.tag("acciaccatura"); break;
        case NoteType::APPOGGIATURA: xml.tag("appoggiatura"); break;
        case NoteType::GRACE4: xml.tag("grace4"); break;
        case NoteType::GRACE16: xml.tag("grace16"); break;
        case NoteType::GRACE32: xml.tag("grace32"); break;
        case NoteType::GRACE8_AFTER: xml.tag("grace8after"); break;
        case NoteType::GRACE16_AFTER: xml.tag("grace16after"); break;
        case NoteType::GRACE32_AFTER: xml.tag("grace32after"); break;
        default: break;
    }

    if (item->no_stem()) {
        xml.tag("noStem", item->no_stem());
    } else if (item->stem() && (item->stem()->is_user_modified() || !item->stem()->user_length().is_zero())) {
        write(item->stem(), xml, ctx);
    }
    if (item->hook() && item->hook()->is_user_modified()) {
        write(item->hook(), xml, ctx);
    }
    if (item->show_stem_slash() != item->property_default(Pid::SHOW_STEM_SLASH).to_bool()) {
        xml.tag("showStemSlash", item->show_stem_slash());
    }
    if (item->stem_slash() && item->stem_slash()->is_user_modified()) {
        write(item->stem_slash(), xml, ctx);
    }
    write_property(item, xml, Pid::STEM_DIRECTION);

    for (size_t note_idx = 0; note_idx < note_count; ++note_idx) {
        if (!ctx.can_write_note_idx(note_idx, note_count)) {
            continue;
        }
        const Note *note = item->notes().at(note_idx);
        write(note, xml, ctx);
    }

    // Write parens
    for (const NoteParenthesisInfo *paren_pair : item->note_parentheses()) {
        xml.start_element("NoteParenGroup");

        write(paren_pair->left_paren(), xml, ctx);
        write(paren_pair->right_paren(), xml, ctx);

        xml.start_element("Notes");
        for (const Note *note : paren_pair->notes()) {
            EID note_eid = note->eid();
            if (!note_eid.is_valid()) {
                note_eid = note->assign_new_eid();
            }
            xml.tag("NoteEID", note_eid.to_std_string());
        }
        xml.end_element();
        xml.end_element();
    }

    if (item->arpeggio()) {
        write_item(item->arpeggio(), xml, ctx);
    }

    if (item->tremolo_single_chord()) {
        write(item->tremolo_single_chord(), xml, ctx);
    } else if (item->tremolo_two_chord() && item->tremolo_chord_type() != TremoloChordType::TREMOLO_SECOND_CHORD) {
        write(item->tremolo_two_chord(), xml, ctx);
    }

    write_property(item, xml, Pid::COMBINE_VOICE);

    for (EngravingItem *e : item->el()) {
        if (e->is_chord_line() && to_chord_line(e)->note()) { // this is now written by Note
            continue;
        }
        write_item(e, xml, ctx);
    }
    xml.end_element();
}

void TWrite::write_chord_rest_beam(const ChordRest *item, XmlWriter &xml, WriteContext &ctx) {
    Beam *b = item->beam();
    if (b && b->elements().front() == item && (MScore::test_mode || !b->generated())) {
        write(b, xml, ctx);
    }
}

void TWrite::write_properties(const ChordRest *item, XmlWriter &xml, WriteContext &ctx) {
    write_item_properties(item, xml, ctx);

    //
    // BeamMode default:
    //    REST  - BeamMode::NONE
    //    CHORD - BeamMode::AUTO
    //
    if ((item->is_rest() && item->beam_mode() != BeamMode::NONE) || (item->is_chord() && item->beam_mode() != BeamMode::AUTO)) {
        xml.tag("BeamMode", TConv::to_xml(item->beam_mode()));
    }
    write_property(item, xml, Pid::SMALL);
    if (item->actual_duration_type().dots()) {
        xml.tag("dots", item->actual_duration_type().dots());
    }
    write_property(item, xml, Pid::STAFF_MOVE);

    if (item->actual_duration_type().is_valid()) {
        xml.tag("durationType", TConv::to_xml(item->actual_duration_type().type()));
    }

    if (!item->ticks().is_zero() &&
        (!item->actual_duration_type().fraction().is_valid() || (item->actual_duration_type().fraction() != item->ticks()))) {
        xml.tag_fraction("duration", item->ticks());
        //xml.tagE("duration z=\"%d\" n=\"%d\"", ticks().numerator(), ticks().denominator());
    }

    for (Lyrics *lyrics : item->lyrics()) {
        write(lyrics, xml, ctx);
    }

    if (!item->is_grace()) {
        Fraction t(item->global_ticks());
        if (item->staff()) {
            t /= item->staff()->time_stretch(ctx.cur_tick());
        }
        ctx.inc_cur_tick(t);
    }
}

void TWrite::write_tuplet_start(DurationElement *item, XmlWriter &xml, WriteContext &ctx) {
    if (item->tuplet() && item->tuplet()->elements().front() == item) {
        write_tuplet_start(item->tuplet(), xml, ctx); // recursion
        TWrite::write(item->tuplet(), xml, ctx);
    }
}

void TWrite::write_tuplet_end(DurationElement *item, XmlWriter &xml, WriteContext &ctx) {
    if (item->tuplet() && item->tuplet()->elements().back() == item) {
        xml.tag("endTuplet");
        write_tuplet_end(item->tuplet(), xml, ctx); // recursion
    }
}

static muse::String chord_line_number_text(double value) {
    char buffer[64];
    const auto result =
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (result.ec != std::errc()) {
        throw std::runtime_error("Native coordinate serialization failed");
    }
    return muse::String::from_std_string(std::string(buffer, result.ptr));
}

void TWrite::write(const ChordLine *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::CHORD_LINE_TYPE);
    write_property(item, xml, Pid::CHORD_LINE_STRAIGHT);
    write_property(item, xml, Pid::CHORD_LINE_WAVY);
    write_property(item, xml, Pid::PLAY);
    xml.tag("lengthX", item->length_x(), 0.0);
    xml.tag("lengthY", item->length_y(), 0.0);
    // Policy precedes generic point properties, including the user position.
    if (!item->size_is_spatium_dependent()) {
        xml.tag("sizeIsSpatiumDependent", false);
    }
    write_item_properties(item, xml, ctx);
    if (item->has_source_offsets()) {
        const auto offsets = item->xml_source_offsets();
        // Emit all four, including zeros and dormant straight/wavy controls.
        // Serialization never depends on whether a layout has run.
        xml.tag("o1", {{"x", chord_line_number_text(offsets[0].x())}, {"y", chord_line_number_text(offsets[0].y())}});
        xml.tag("o2", {{"x", chord_line_number_text(offsets[1].x())}, {"y", chord_line_number_text(offsets[1].y())}});
        xml.tag("o3", {{"x", chord_line_number_text(offsets[2].x())}, {"y", chord_line_number_text(offsets[2].y())}});
        xml.tag("o4", {{"x", chord_line_number_text(offsets[3].x())}, {"y", chord_line_number_text(offsets[3].y())}});
    } else if (item->modified()) {
        //! NOTE Need separated "given" data and layout data
        const ChordLine::LayoutData *ldata = item->ldata();
        const PainterPath &path = ldata->path;
        size_t n = path.element_count();
        xml.start_element("Path");
        for (size_t i = 0; i < n; ++i) {
            const PainterPath::Element &e = path.element_at(i);
            double spatium = item->style().spatium();
            double x = e.x / spatium;
            double y = e.y / spatium;
            xml.tag("Element", {{"type", int(e.type)}, {"x", x}, {"y", y}});
        }
        xml.end_element();
    }
    xml.end_element();
}

void TWrite::write(const Clef *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::CLEF_TYPE_CONCERT);
    write_property(item, xml, Pid::CLEF_TYPE_TRANSPOSING);
    write_property(item, xml, Pid::CLEF_TO_BARLINE_POS);
    if (item->is_header()) {
        xml.tag("isHeader", true);
    }
    write_property(item, xml, Pid::IS_COURTESY);
    if (!item->show_courtesy()) {
        xml.tag("showCourtesyClef", item->show_courtesy());
    }
    if (item->for_instrument_change()) {
        xml.tag("forInstrumentChange", item->for_instrument_change());
    }
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Capo *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::ACTIVE);
    write_property(item, xml, Pid::CAPO_FRET_POSITION);
    write_property(item, xml, Pid::CAPO_GENERATE_TEXT);
    write_property(item, xml, Pid::CAPO_TRANSPOSE_MODE);

    std::set<string_idx_t> ordered_strings;
    for (string_idx_t idx : item->params().ignored_strings) {
        ordered_strings.insert(idx);
    }

    for (string_idx_t idx : ordered_strings) {
        xml.start_element("string", {{"no", idx}});
        xml.tag("apply", false);
        xml.end_element();
    }

    write_properties(static_cast<const StaffTextBase *>(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const Dynamic *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_property(item, xml, Pid::DYNAMIC_TYPE);
    write_property(item, xml, Pid::VELOCITY);
    write_property(item, xml, Pid::AVOID_BARLINES);
    write_property(item, xml, Pid::CENTER_ON_NOTEHEAD);
    write_property(item, xml, Pid::PLAY);
    write_property(item, xml, Pid::ANCHOR_TO_END_OF_PREVIOUS);

    if (item->is_velocity_change_available()) {
        write_property(item, xml, Pid::VELO_CHANGE);
        write_property(item, xml, Pid::VELO_CHANGE_SPEED);
    }

    write_properties(to_text_base(item), xml, ctx, to_dynamic(item)->has_custom_text());
    xml.end_element();
}

void TWrite::write(const Expression *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write_properties(const TextBase *item, XmlWriter &xml, WriteContext &ctx, bool write_text) {
    if (item->has_voice_assignment_properties()) {
        write_property(item, xml, Pid::VOICE_ASSIGNMENT);
        write_property(item, xml, Pid::DIRECTION);
        write_property(item, xml, Pid::CENTER_BETWEEN_STAVES);
    }

    write_property(item, xml, Pid::TEXT_STYLE);
    write_item_properties(item, xml, ctx);

    write_styled_properties(item, xml);

    for (const auto &spp : *text_style(item->text_style_type())) {
        if (item->is_styled(spp.pid) ||
            (spp.pid == Pid::FONT_SIZE && item->get_property(spp.pid).to_double() == TextBase::undefined_font_size) ||
            (spp.pid == Pid::FONT_FACE && item->get_property(spp.pid).value<String>() == TextBase::undefined_font_family)) {
            continue;
        }
        write_property(item, xml, spp.pid);
    }
    if (write_text) {
        xml.write_xml(u"text", line_break_to_tag(item->xml_text()));
    }

    write_property(item, xml, Pid::TEXT_LINKED_TO_MASTER);
}

void TWrite::write(const Fermata *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);
    xml.tag("subtype", SymNames::name_for_sym_id(item->sym_id()));
    write_property(item, xml, Pid::TIME_STRETCH);
    write_property(item, xml, Pid::PLAY);
    write_property(item, xml, Pid::MIN_DISTANCE);
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const FiguredBass *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);
    if (!item->on_note()) {
        xml.tag("onNote", item->on_note());
    }
    if (item->ticks().is_not_zero()) {
        xml.tag_fraction("ticks", item->ticks());
    }
    // if unparseable items, write full text data
    if (item->items().size() < 1) {
        write_properties(to_text_base(item), xml, ctx, true);
    } else {
        //            if (textStyleType() != StyledPropertyListIdx::FIGURED_BASS)
        //                  // if all items parsed and not unstiled, we simply have a special style: write it
        //                  xml.tag("style", textStyle().name());
        for (FiguredBassItem *f_b_item : item->items()) {
            write(f_b_item, xml, ctx);
        }
        write_styled_properties(item, xml);
        write_item_properties(item, xml, ctx);
    }
    xml.end_element();
}

void TWrite::write(const FiguredBassItem *item, XmlWriter &xml, WriteContext &) {
    xml.start_element("FiguredBassItem", item);
    xml.tag("brackets", {{"b0", int(item->parenth1())},
                         {"b1", int(item->parenth2())},
                         {"b2", int(item->parenth3())},
                         {"b3", int(item->parenth4())},
                         {"b4", int(item->parenth5())}});

    if (item->prefix() != FiguredBassItem::Modifier::NONE) {
        xml.tag("prefix", int(item->prefix()));
    }
    if (item->digit() != ::mu::engraving::fbi_digit_none) {
        xml.tag("digit", item->digit());
    }
    if (item->suffix() != FiguredBassItem::Modifier::NONE) {
        xml.tag("suffix", int(item->suffix()));
    }
    if (item->cont_line() != FiguredBassItem::ContLine::NONE) {
        xml.tag("continuationLine", int(item->cont_line()));
    }
    xml.end_element();
}

void TWrite::write(const Fingering *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const FretDiagram *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);

    static const std::array<Pid, 10> pids{{Pid::MIN_DISTANCE, Pid::FRET_OFFSET, Pid::FRET_FRETS, Pid::FRET_STRINGS, Pid::FRET_NUT, Pid::MAG,
                                           Pid::ORIENTATION, Pid::FRET_SHOW_FINGERINGS, Pid::FRET_FINGERING, Pid::EXCLUDE_VERTICAL_ALIGN}};

    // Write properties first and only once
    for (Pid p : pids) {
        write_property(item, xml, p);
    }
    write_item_properties(item, xml, ctx);

    if (item->harmony()) {
        write(item->harmony(), xml, ctx);
    }

    xml.start_element("fretDiagram");
    {
        for (int i = 0; i < item->strings(); ++i) {
            FretItem::Marker m = item->marker(i);
            std::vector<FretItem::Dot> all_dots = item->dot(i);

            bool dot_exists = false;
            for (auto const &d : all_dots) {
                if (d.exists() && !d.is_part_of_slur_barre) { // Don't write dot if part of slur barré (will be generate during layout)
                    dot_exists = true;
                    break;
                }
            }

            // Only write a string if we have anything to write
            if (!dot_exists && !m.exists()) {
                continue;
            }

            // Start the string writing
            xml.start_element("string", {{"no", i}});

            // Write marker
            if (m.exists()) {
                xml.tag("marker", FretItem::marker_type_to_name(m.mtype));
            }

            // Write any dots
            for (auto const &d : all_dots) {
                if (d.exists()) {
                    // TODO: write fingering
                    xml.tag("dot", {{"fret", d.fret}}, FretItem::dot_type_to_name(d.dtype));
                }
            }

            xml.end_element();
        }

        for (int fi = 1; fi <= item->frets(); ++fi) {
            FretItem::Barre b = item->barre(fi);
            if (!b.exists()) {
                continue;
            }

            xml.tag("barre", {{"start", b.start_string}, {"end", b.end_string}}, fi);
        }
    }
    xml.end_element();
    xml.end_element();
}

void TWrite::write(const Glissando *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);

    xml.tag_property("text", item->text(), item->property_default(Pid::GLISS_TEXT));

    if (ctx.clipboardmode() && item->is_harp_gliss().has_value()) {
        xml.tag_property("isHarpGliss", PropertyValue(item->is_harp_gliss().value()));
    }

    for (auto id : {Pid::GLISS_SHIFT, Pid::GLISS_EASEIN, Pid::GLISS_EASEOUT}) {
        write_property(item, xml, id);
    }

    write_properties(static_cast<const SLine *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const GuitarBend *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    xml.tag("guitarBendType", TConv::to_xml(item->bend_type()));
    xml.tag("bendStartTimeFactor", item->start_time_factor());
    xml.tag("bendEndTimeFactor", item->end_time_factor());

    if (item->target_time_factor().has_value()) {
        xml.tag("bendTargetTimeFactor", item->target_time_factor().value());
    }

    write_property(item, xml, Pid::DIRECTION);
    write_property(item, xml, Pid::BEND_SHOW_HOLD_LINE);
    if (item->is_dive()) {
        write_property(item, xml, Pid::GUITAR_DIVE_TAB_POS);
        write_property(item, xml, Pid::VIBRATO_LINE_TYPE);
        write_property(item, xml, Pid::GUITAR_DIVE_IS_SLACK);
        if (item->bend_type() == GuitarBendType::DIP || item->overlapping_bend_or_dive()) {
            write_property(item, xml, Pid::GUITAR_BEND_AMOUNT);
        }
    }

    write_properties(static_cast<const SLine *>(item), xml, ctx);

    GuitarBendHold *hold = item->hold_line();
    if (hold) {
        xml.start_element(hold);
        write_properties(static_cast<const SLine *>(hold), xml, ctx);
        xml.end_element();
    }

    xml.end_element();
}

void TWrite::write_properties(const GuitarBendSegment *item, XmlWriter &xml, WriteContext &ctx) {
    write_property(item, xml, Pid::BEND_VERTEX_OFF);

    GuitarBendText *text = item->bend_text();
    if (text && text->is_user_modified()) {
        xml.start_element(text);
        write_properties(to_text_base(text), xml, ctx, false);
        xml.end_element();
    }
}

void TWrite::write_properties(const SLine *item, XmlWriter &xml, WriteContext &ctx) {
    write_properties(static_cast<const Spanner *>(item), xml, ctx);
    if (item->diagonal()) {
        xml.tag("diagonal", item->diagonal());
    }

    for (Pid pid : {Pid::LINE_WIDTH, Pid::LINE_STYLE, Pid::DASH_LINE_LEN, Pid::DASH_GAP_LEN}) {
        if (item->property_flags(pid) == PropertyFlags::NOSTYLE) {
            write_property(item, xml, pid);
        }
    }

    write_styled_properties(item, xml);

    if (!item->is_user_modified()) {
        return;
    }

    double spatium_value = item->style().spatium();
    for (const SpannerSegment *seg : item->spanner_segments()) {
        xml.start_element("Segment", seg);
        xml.tag("subtype", int(seg->spanner_segment_type()));
        xml.tag_point("offset", seg->offset() / spatium_value);
        xml.tag_point("off2", seg->user_off2() / spatium_value);
        write_property(seg, xml, Pid::MIN_DISTANCE);
        if (seg->is_guitar_bend_segment()) {
            write_properties(static_cast<const GuitarBendSegment *>(seg), xml, ctx);
        }
        write_item_properties(seg, xml, ctx);
        xml.end_element();
    }
}

void TWrite::write_properties(const Spanner *item, XmlWriter &xml, WriteContext &ctx) {
    write_property(item, xml, Pid::PLAY);
    if (item->anchor() == Spanner::Anchor::SEGMENT) {
        int t2 = static_cast<int>(item->track2()) + ctx.track_diff();
        xml.tag("track2", t2);
        xml.tag_fraction("startTick", item->tick(),
                         /* default = */ Fraction(-1, 0)); // Need to be able to write start of score, so a different default is needed
        xml.tag_fraction("ticks", item->ticks());
    } else if (!item->is_guitar_bend_hold()) {
        /* GuitarBendHold lines are regenerated during layout by GuitarBend::updateHoldLine(),
         * which recomputes their endpoints, so there is no point in serializing them */

        const bool is_partial_tie_or_lv = item->is_partial_tie() || item->is_laissez_vib();
        const bool has_start_end_elements = item->start_element() && item->end_element();
        IF_ASSERT_FAILED(is_partial_tie_or_lv || has_start_end_elements) {
            write_item_properties(item, xml, ctx);
            return;
        }
        if (EngravingItem *start_el = item->start_element()) {
            EID start_el_eid = start_el->eid();
            if (!start_el_eid.is_valid()) {
                assert(item->anchor() == Spanner::Anchor::NOTE);
                start_el_eid = start_el->assign_new_eid();
            }
            xml.tag("startElement", start_el_eid.to_std_string());
        }
        if (EngravingItem *end_el = item->end_element()) {
            EID end_el_eid = end_el->eid();
            IF_ASSERT_FAILED(end_el_eid.is_valid()) { end_el_eid = end_el->assign_new_eid(); }
            xml.tag("endElement", end_el_eid.to_std_string());
        }
    }
    write_item_properties(item, xml, ctx);
}

void TWrite::write(const GradualTempoChange *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    for (Pid pid : {Pid::TEMPO_CHANGE_TYPE, Pid::TEMPO_EASING_METHOD, Pid::TEMPO_CHANGE_FACTOR, Pid::SNAP_AFTER}) {
        if (item->property_flags(pid) == PropertyFlags::NOSTYLE) {
            write_property(item, xml, pid);
        }
    }
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write_properties(const TextLineBase *item, XmlWriter &xml, WriteContext &ctx) {
    for (Pid pid : TextLineBase::text_line_base_property_ids()) {
        if (item->property_flags(pid) == PropertyFlags::NOSTYLE) {
            write_property(item, xml, pid);
        }
    }

    write_properties(static_cast<const SLine *>(item), xml, ctx);
}

void TWrite::write(const Groups *item, XmlWriter &xml, WriteContext &) {
    xml.start_element("Groups");
    for (const GroupNode &n : item->nodes()) {
        xml.tag("Node", {{"pos", n.pos}, {"action", n.action}});
    }
    xml.end_element();
}

void TWrite::write(const Hairpin *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    xml.tag("subtype", int(item->hairpin_type()));

    for (Pid pid :
         {Pid::VELO_CHANGE, Pid::HAIRPIN_CIRCLEDTIP, Pid::SINGLE_NOTE_DYNAMICS, Pid::VELO_CHANGE_METHOD, Pid::VOICE_ASSIGNMENT,
          Pid::DIRECTION, Pid::CENTER_BETWEEN_STAVES, Pid::SNAP_BEFORE, Pid::SNAP_AFTER, Pid::HAIRPIN_HEIGHT, Pid::HAIRPIN_CONT_HEIGHT}) {
        if (item->property_flags(pid) == PropertyFlags::NOSTYLE) {
            write_property(item, xml, pid);
        }
    }

    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const HammerOnPullOff *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);

    write_property(item, xml, Pid::PARTIAL_SPANNER_DIRECTION);

    write_properties(to_slur(item), xml, ctx);

    xml.end_element();
}

void TWrite::write_properties(const HammerOnPullOffSegment *seg, XmlWriter &xml, WriteContext &ctx) {
    for (size_t i = 0; i < seg->hopo_text().size(); ++i) {
        HammerOnPullOffText *hopo_text = seg->hopo_text()[i];
        if (!hopo_text->is_user_modified()) {
            continue;
        }
        write(hopo_text, xml, ctx, i);
    }
}

void TWrite::write(const HammerOnPullOffText *item, XmlWriter &xml, WriteContext &ctx, size_t idx) {
    xml.start_element(item, {{"idx", idx}});

    write_properties(to_text_base(item), xml, ctx, /*writeText*/ false);
    xml.end_element();
}

static void write_harmony_info(const HarmonyInfo *item, const Harmony *h, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element("harmonyInfo");
    if (item->root_tpc() != Tpc::TPC_INVALID || item->bass_tpc() != Tpc::TPC_INVALID) {
        int r_root_tpc = item->root_tpc();
        int r_bass_tpc = item->bass_tpc();
        if (h->staff()) {
            // parent can be a fret diagram
            const Segment *segment = h->get_parent_seg();
            Fraction tick = segment ? segment->tick() : Fraction(-1, 1);
            const Interval &interval = h->staff()->transpose(tick);
            if (ctx.clipboardmode() && !h->score()->style().style_b(Sid::CONCERT_PITCH) && interval.chromatic) {
                r_root_tpc = Transpose::transpose_tpc(item->root_tpc(), interval, true);
                r_bass_tpc = Transpose::transpose_tpc(item->bass_tpc(), interval, true);
            }
        }

        if (item->id() > 0) {
            xml.tag("extension", item->id());
        }
        // parser uses leading "=" as a hidden specifier for minor
        // this may or may not currently be incorporated into _textName
        String write_name = item->text_name();
        if (item->parsed_chord() && item->parsed_chord()->name().starts_with(u'=') && !write_name.starts_with(u'=')) {
            write_name = u"=" + write_name;
        }
        if (!write_name.is_empty()) {
            xml.tag("name", write_name);
        }
        if (r_root_tpc != Tpc::TPC_INVALID) {
            xml.tag("root", r_root_tpc);
        }
        if (r_bass_tpc != Tpc::TPC_INVALID) {
            xml.tag("bass", r_bass_tpc);
        }
    } else {
        xml.tag("name", item->text_name());
    }

    xml.end_element();
}

void TWrite::write(const Harmony *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item) || item->chords().empty()) {
        return;
    }
    xml.start_element(item);
    write_property(item, xml, Pid::HARMONY_TYPE);
    write_property(item, xml, Pid::PLAY);

    //! needed to genarate harmony_to_diagram.xml
    // xml.tag("name", item->harmonyName());

    // check tpcs valid?
    if (item->root_case() != NoteCaseType::CAPITAL) {
        xml.tag("rootCase", TConv::to_xml(item->root_case()));
    }

    if (item->bass_case() != NoteCaseType::CAPITAL) {
        xml.tag("bassCase", TConv::to_xml(item->bass_case()));
    }

    for (const HarmonyInfo *info : item->chords()) {
        write_harmony_info(info, item, xml, ctx);
    }

    for (const HDegree &hd : item->degree_list()) { // Do we really still need this?
        HDegreeType tp = hd.type();
        if (tp == HDegreeType::ADD || tp == HDegreeType::ALTER || tp == HDegreeType::SUBTRACT) {
            xml.start_element("degree");
            xml.tag("degree-value", hd.value());
            xml.tag("degree-alter", hd.alter());
            switch (tp) {
                case HDegreeType::ADD: xml.tag("degree-type", "add"); break;
                case HDegreeType::ALTER: xml.tag("degree-type", "alter"); break;
                case HDegreeType::SUBTRACT: xml.tag("degree-type", "subtract"); break;
                default: break;
            }
            xml.end_element();
        }
    }

    write_property(item, xml, Pid::HARMONY_DO_NOT_STACK_MODIFIERS);
    write_property(item, xml, Pid::EXCLUDE_VERTICAL_ALIGN);
    write_properties(to_text_base(item), xml, ctx, false);
    //Pid::HARMONY_VOICE_LITERAL, Pid::HARMONY_VOICING, Pid::HARMONY_DURATION
    //written by the above function call because they are part of element style
    xml.end_element();
}

void TWrite::write(const HarmonicMark *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const HarpPedalDiagram *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_property(item, xml, Pid::HARP_IS_DIAGRAM);

    // Write vector of harp strings.  Order is always D, C, B, E, F, G, A
    xml.start_element("pedalState");
    for (size_t idx = 0; idx < item->get_pedal_state().size(); idx++) {
        xml.tag("string", {{"name", idx}}, static_cast<int>(item->get_pedal_state().at(idx)));
    }
    xml.end_element();

    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const Hook *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    xml.tag("name", SymNames::name_for_sym_id(item->sym()));
    if (item->score_font()) {
        xml.tag("font", item->score_font()->name());
    }
    write_properties(static_cast<const BSymbol *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write_properties(const BSymbol *item, XmlWriter &xml, WriteContext &ctx) {
    for (const EngravingItem *e : item->leafs()) {
        write_item(e, xml, ctx);
    }
    write_item_properties(item, xml, ctx);
}

void TWrite::write(const Image *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_properties(static_cast<const BSymbol *>(item), xml, ctx);

    xml.tag("path", item->store_item() ? item->store_item()->hash_name() : std::string());

    write_property(item, xml, Pid::AUTOSCALE);
    write_property(item, xml, Pid::SIZE);
    write_property(item, xml, Pid::LOCK_ASPECT_RATIO);
    write_property(item, xml, Pid::SIZE_IS_SPATIUM);

    xml.end_element();
}

void TWrite::write(const Instrument *item, XmlWriter &xml, WriteContext &, const Part *part) {
    if (item->id().is_empty()) {
        xml.start_element("Instrument");
    } else {
        xml.start_element("Instrument", {{"id", item->id()}});
    }

    if (!item->sound_id().empty()) {
        xml.tag("soundId", item->sound_id());
    }

    if (!item->instrument_label().empty()) {
        write(item->instrument_label(), xml);
    }

    //      if (!_trackName.empty())
    xml.tag("trackName", item->track_name());
    if (item->min_pitch_p() > ::mu::engraving::min_pitch) {
        xml.tag("minPitchP", item->min_pitch_p());
    }
    if (item->max_pitch_p() < ::mu::engraving::max_pitch) {
        xml.tag("maxPitchP", item->max_pitch_p());
    }
    if (item->min_pitch_a() > ::mu::engraving::min_pitch) {
        xml.tag("minPitchA", item->min_pitch_a());
    }
    if (item->max_pitch_a() < ::mu::engraving::max_pitch) {
        xml.tag("maxPitchA", item->max_pitch_a());
    }
    if (item->transpose().diatonic) {
        xml.tag("transposeDiatonic", item->transpose().diatonic);
    }
    if (item->transpose().chromatic) {
        xml.tag("transposeChromatic", item->transpose().chromatic);
    }
    if (!item->music_xml_id().is_empty()) {
        xml.tag("instrumentId", item->music_xml_id());
    }
    if (item->use_drumset()) {
        xml.tag("useDrumset", item->use_drumset());
        item->drumset()->save(xml);
    }
    for (size_t i = 0; i < item->cleff_type_count(); ++i) {
        ClefTypeList ct = item->clef_type(i);
        if (ct.concert_clef == ct.transposing_clef) {
            if (ct.concert_clef != ClefType::G) {
                if (i) {
                    xml.tag("clef", {{"staff", i + 1}}, TConv::to_xml(ct.concert_clef));
                } else {
                    xml.tag("clef", TConv::to_xml(ct.concert_clef));
                }
            }
        } else {
            if (i) {
                xml.tag("concertClef", {{"staff", i + 1}}, TConv::to_xml(ct.concert_clef));
                xml.tag("transposingClef", {{"staff", i + 1}}, TConv::to_xml(ct.transposing_clef));
            } else {
                xml.tag("concertClef", TConv::to_xml(ct.concert_clef));
                xml.tag("transposingClef", TConv::to_xml(ct.transposing_clef));
            }
        }
    }

    if (item->single_note_dynamics() != item->get_single_note_dynamics_from_template()) {
        xml.tag("singleNoteDynamics", item->single_note_dynamics());
    }

    if (item->glissando_style() != GlissandoStyle::CHROMATIC) {
        xml.tag("glissandoStyle", TConv::to_xml(item->glissando_style()));
    }

    if (!item->string_data()->is_null()) {
        write(item->string_data(), xml);
    }

    for (const NamedEventList &a : item->midi_actions()) {
        write(&a, xml, "MidiAction");
    }
    for (const MidiArticulation &a : item->articulation()) {
        write(&a, xml);
    }
    for (const InstrChannel *a : item->channel()) {
        write(a, xml, part);
    }
    xml.end_element();
}

static void midi_event_write(const MidiCoreEvent &e, XmlWriter &xml) {
    switch (e.type()) {
        case ME_NOTEON: xml.tag("note-on", {{"channel", e.channel()}, {"pitch", e.pitch()}, {"velo", e.velo()}}); break;

        case ME_NOTEOFF: xml.tag("note-off", {{"channel", e.channel()}, {"pitch", e.pitch()}, {"velo", e.velo()}}); break;

        case ME_CONTROLLER:
            if (e.controller() == CTRL_PROGRAM) {
                if (e.channel() == 0) {
                    xml.tag("program", {{"value", e.value()}});
                } else {
                    xml.tag("program", {{"channel", e.channel()}, {"value", e.value()}});
                }
            } else {
                if (e.channel() == 0) {
                    xml.tag("controller", {{"ctrl", e.controller()}, {"value", e.value()}});
                } else {
                    xml.tag("controller", {{"channel", e.channel()}, {"ctrl", e.controller()}, {"value", e.value()}});
                }
            }
            break;
        default: LOGD("MidiCoreEvent::write: unknown type"); break;
    }
}

void TWrite::write(const InstrChannel *item, XmlWriter &xml, const Part *part) {
    if (item->name().is_empty() || item->name() == InstrChannel::default_name) {
        xml.start_element("Channel");
    } else {
        xml.start_element("Channel", {{"name", item->name()}});
    }
    if (item->color() != InstrChannel::default_color) {
        xml.tag("color", item->color());
    }

    for (const MidiCoreEvent &e : item->init_list()) {
        if (e.type() == ME_INVALID) {
            continue;
        }
        if (e.type() == ME_CONTROLLER) {
            // Don't write bank if automatically switched, but always write if switched by the user
            if (e.data_a() == CTRL_HBANK && e.data_b() == 0 && !item->user_bank_controller()) {
                continue;
            }
            if (e.data_a() == CTRL_LBANK && e.data_b() == 0 && !item->user_bank_controller()) {
                continue;
            }
            if (e.data_a() == CTRL_VOLUME && e.data_b() == 100) {
                continue;
            }
            if (e.data_a() == CTRL_PANPOT && e.data_b() == 64) {
                continue;
            }
            if (e.data_a() == CTRL_REVERB_SEND && e.data_b() == 0) {
                continue;
            }
            if (e.data_a() == CTRL_CHORUS_SEND && e.data_b() == 0) {
                continue;
            }
        }

        midi_event_write(e, xml);
    }
    if (!MScore::test_mode) {
        // xml.tag("synti", ::synti->name(synti));
        xml.tag("synti", item->synti());
    }

    if (part && part->master_score()->export_midi_mapping() && part->score() == part->master_score()) {
        xml.tag("midiPort", part->master_score()->midi_mapping(item->channel())->port());
        xml.tag("midiChannel", part->master_score()->midi_mapping(item->channel())->channel());
    }
    for (const NamedEventList &a : item->midi_actions) {
        write(&a, xml, "MidiAction");
    }
    for (const MidiArticulation &a : item->articulation) {
        write(&a, xml);
    }
    xml.end_element();
}

void TWrite::write(const MidiArticulation *item, XmlWriter &xml) {
    if (item->name.is_empty()) {
        xml.start_element("Articulation");
    } else {
        xml.start_element("Articulation", {{"name", item->name}});
    }
    if (!item->descr.is_empty()) {
        xml.tag("descr", item->descr);
    }
    xml.tag("velocity", item->velocity);
    xml.tag("gateTime", item->gate_time);
    xml.end_element();
}

void TWrite::write(const StaffLabel &item, XmlWriter &xml) {
    xml.start_element("StaffLabel");
    write_properties(item, xml);
    xml.end_element();
}

void TWrite::write_properties(const StaffLabel &item, XmlWriter &xml) {
    String long_name = item.long_name();
    if (!long_name.empty()) {
        xml.write_xml(u"longName", line_break_to_tag(long_name));
    }

    String short_name = item.short_name();
    if (!short_name.empty()) {
        xml.write_xml(u"shortName", line_break_to_tag(short_name));
    }
}

void TWrite::write(const InstrumentLabel &item, XmlWriter &xml) {
    xml.start_element("InstrumentLabel");
    write_properties(item, xml);
    xml.end_element();
}

void TWrite::write_properties(const InstrumentLabel &item, XmlWriter &xml) {
    write_properties(static_cast<StaffLabel>(item), xml);

    String transposition = item.transposition();
    if (!transposition.empty()) {
        xml.write_xml(u"transposition", line_break_to_tag(transposition));
    }

    if (!item.show_transposition_long()) {
        xml.tag("showTranspositionLong", item.show_transposition_long());
    }

    if (!item.show_transposition_short()) {
        xml.tag("showTranspositionShort", item.show_transposition_short());
    }

    if (item.number() != 0) {
        xml.tag("number", item.number());
    }

    if (!item.show_number_long()) {
        xml.tag("showNumberLong", item.show_number_long());
    }

    if (!item.show_number_short()) {
        xml.tag("showNumberShort", item.show_number_short());
    }

    if (item.use_custom_name()) {
        xml.tag("useCustomName", item.use_custom_name());
    }

    if (!item.custom_name_long().empty()) {
        xml.write_xml(u"customNameLong", item.custom_name_long());
    }

    if (!item.custom_name_short().empty()) {
        xml.write_xml(u"customNameShort", item.custom_name_short());
    }

    if (!item.allow_group_name()) {
        xml.tag("allowGroupName", item.allow_group_name());
    }

    if (!item.custom_name_long_group().empty()) {
        xml.write_xml(u"customNameLongGroup", item.custom_name_long_group());
    }

    if (!item.custom_name_short_group().empty()) {
        xml.write_xml(u"customNameShortGroup", item.custom_name_short_group());
    }

    if (item.use_custom_group_name()) {
        xml.tag("useCustomGroupName", item.use_custom_group_name());
    }

    if (!item.custom_name_long_individual().empty()) {
        xml.write_xml(u"customNameLongIndividual", item.custom_name_long_individual());
    }

    if (!item.custom_name_short_individual().empty()) {
        xml.write_xml(u"customNameShortIndividual", item.custom_name_short_individual());
    }

    if (item.use_custom_individual_name()) {
        xml.tag("useCustomIndividualName", item.use_custom_individual_name());
    }
}

void TWrite::write(const NamedEventList *item, XmlWriter &xml, const AsciiStringView &n) {
    xml.start_element(n, {{"name", item->name}});
    if (!item->descr.empty()) {
        xml.tag("descr", item->descr);
    }
    for (const MidiCoreEvent &e : item->events) {
        midi_event_write(e, xml);
    }
    xml.end_element();
}

void TWrite::write(const InstrumentChange *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write(item->instrument(), xml, ctx, item->part());
    if (item->init()) {
        xml.tag("init", item->init());
    }
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const Jump *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.tag("jumpTo", item->jump_to());
    xml.tag("playUntil", item->play_until());
    xml.tag("continueAt", item->continue_at());
    write_property(item, xml, Pid::PLAY_REPEATS);
    xml.end_element();
}

void TWrite::write(const KeySig *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_item_properties(item, xml, ctx);
    if (item->is_atonal()) {
        xml.tag("custom", 1);
    } else {
        xml.tag("concertKey", int(item->concert_key()));
        if (item->concert_key() != item->key()) {
            xml.tag("actualKey", int(item->key()));
        }
        if (item->is_custom()) {
            xml.tag("custom", 1);
            for (const CustDef &cd : item->custom_key_defs()) {
                xml.start_element("CustDef");
                xml.tag("sym", SymNames::name_for_sym_id(cd.sym));
                xml.tag("def", {{"degree", cd.degree}, {"xAlt", cd.x_alt.val()}, {"octAlt", cd.oct_alt}});
                xml.end_element();
            }
        }
    }

    if (item->mode() != KeyMode::UNKNOWN) {
        xml.tag("mode", TConv::to_xml(item->mode()));
    }

    write_property(item, xml, Pid::SHOW_COURTESY);
    write_property(item, xml, Pid::IS_COURTESY);

    if (item->for_instrument_change()) {
        xml.tag("forInstrumentChange", true);
    }
    xml.end_element();
}

void TWrite::write(const LaissezVib *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::MIN_LENGTH);
    write_property(item, xml, Pid::TIE_PLACEMENT);
    write_properties(static_cast<const SlurTie *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const LayoutBreak *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_item_properties(item, xml, ctx);

    for (auto id : {Pid::LAYOUT_BREAK, Pid::PAUSE, Pid::START_WITH_LONG_NAMES, Pid::START_WITH_MEASURE_ONE, Pid::FIRST_SYSTEM_INDENTATION,
                    Pid::SHOW_COURTESY}) {
        write_property(item, xml, id);
    }

    xml.end_element();
}

void TWrite::write(const LedgerLine *item, XmlWriter &xml, WriteContext &) {
    xml.start_element(item);
    xml.tag("lineWidth", item->width() / item->spatium());
    xml.tag("lineLen", item->len() / item->spatium());
    if (!item->vertical()) {
        xml.tag("vertical", item->vertical());
    }
    xml.end_element();
}

void TWrite::write(const LetRing *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Location *item, XmlWriter &xml, WriteContext &) {
    static constexpr Location rel_defaults = Location::relative();

    DO_ASSERT(item->is_relative());
    xml.start_element("location");
    xml.tag("staves", item->staff(), rel_defaults.staff());
    xml.tag("voices", item->voice(), rel_defaults.voice());
    xml.tag("measures", item->measure(), rel_defaults.measure());
    xml.tag_fraction("fractions", item->frac().reduced(), rel_defaults.frac());
    xml.tag("grace", item->grace_index(), rel_defaults.grace_index());
    xml.tag("notes", item->note(), rel_defaults.note());
    xml.tag("timeTick", item->is_time_tick(), false);
    xml.end_element();
}

void TWrite::write(const Lyrics *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_property(item, xml, Pid::VERSE);
    if (item->syllabic() != LyricsSyllabic::SINGLE) {
        xml.tag("syllabic", TConv::to_xml(item->syllabic()));
    }
    write_property(item, xml, Pid::LYRIC_TICKS);
    write_property(item, xml, Pid::CENTER_BETWEEN_STAVES);

    write_properties(to_text_base(item), xml, ctx, true);
    if (item->separator() && !item->separator()->generated()) {
        write(item->separator(), xml, ctx);
    }
    xml.end_element();
}

void TWrite::write(const LyricsLine *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const SLine *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Marker *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.tag("label", item->label());
    write_property(item, xml, Pid::MARKER_TYPE);
    write_property(item, xml, Pid::MARKER_CENTER_ON_SYMBOL);
    xml.end_element();
}

void TWrite::write(const MeasureNumber *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const MeasureRepeat *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::SUBTYPE);
    write_properties(to_rest(item), xml, ctx);
    write_property(item, xml, Pid::MEASURE_REPEAT_NUMBER_OFFSET);
    write_items(item->el(), xml, ctx);
    xml.end_element();
}

void TWrite::write_properties(const Rest *item, XmlWriter &xml, WriteContext &ctx) { write_properties(to_chord_rest(item), xml, ctx); }

void TWrite::write(const MMRest *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element("Rest"); // for compatibility, see also Measure::readVoice()
    write_properties(to_chord_rest(item), xml, ctx);
    write_property(item, xml, Pid::MMREST_NUMBER_OFFSET);
    write_property(item, xml, Pid::MMREST_NUMBER_VISIBLE);
    write_items(item->el(), xml, ctx);
    xml.end_element();
}

void TWrite::write(const MMRestRange *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const Note *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_item_properties(item, xml, ctx);

    if (item->accidental()) {
        write(item->accidental(), xml, ctx);
    }
    write_items(item->el(), xml, ctx);
    bool write_dots = false;
    for (NoteDot *dot : item->dots()) {
        if (!dot->offset().is_null() || !dot->visible() || dot->color() != ctx.configuration()->default_color() ||
            dot->visible() != item->visible()) {
            write_dots = true;
            break;
        }
    }
    if (write_dots) {
        for (NoteDot *dot : item->dots()) {
            write(dot, xml, ctx);
        }
    }

    if (item->laissez_vib()) {
        write(item->laissez_vib(), xml, ctx);
    }

    if (item->incoming_partial_tie()) {
        write(item->incoming_partial_tie(), xml, ctx);
    }

    if (item->outgoing_partial_tie()) {
        write(item->outgoing_partial_tie(), xml, ctx);
    }

    if (Tie *tie_back = item->tie_back_non_partial()) {
        Note *tie_start_note = tie_back->start_note();
        const std::optional<Fraction> &first_clipboard_tick = ctx.first_clipboard_tick();
        bool tie_start_out_of_range =
            first_clipboard_tick.has_value() && tie_start_note && tie_start_note->tick() < first_clipboard_tick.value();
        if (!tie_start_out_of_range) {
            TWrite::write_item(tie_back, xml, ctx);
        }
    }

    if ((item->chord() == 0 || item->chord()->play_event_type() != PlayEventType::AUTO) && !item->play_events().empty()) {
        xml.start_element("Events");
        for (const NoteEvent &e : item->play_events()) {
            write(&e, xml, ctx);
        }
        xml.end_element();
    }
    for (Pid id : {Pid::PITCH, Pid::CENT_OFFSET, Pid::TPC1, Pid::TPC2, Pid::SMALL, Pid::MIRROR_HEAD, Pid::DOT_POSITION, Pid::HEAD_SCHEME,
                   Pid::HEAD_GROUP, Pid::USER_VELOCITY, Pid::PLAY, Pid::TUNING, Pid::FRET, Pid::STRING, Pid::GHOST, Pid::DEAD,
                   Pid::HEAD_TYPE, Pid::FIXED, Pid::FIXED_LINE}) {
        write_property(item, xml, id);
    }

    for (Spanner *e : item->spanner_back()) {
        TWrite::write_item(e, xml, ctx);
    }

    for (EngravingItem *e : item->chord()->el()) {
        if (e->is_chord_line() && to_chord_line(e)->note() && to_chord_line(e)->note() == item) {
            write(to_chord_line(e), xml, ctx);
        }
    }

    if (item->override_bend_visibility_rules()) {
        xml.tag("overrideBendVisibilityRules", true);
    }

    write_property(item, xml, Pid::HIDE_GENERATED_PARENTHESES);

    xml.end_element();
}

void TWrite::write(const NoteEvent *item, XmlWriter &xml, WriteContext &) {
    xml.start_element("Event");
    xml.tag("pitch", item->pitch(), 0);
    xml.tag("ontime", item->ontime(), 0);
    xml.tag("len", item->len(), NoteEvent::note_length);
    xml.end_element();
}

void TWrite::write(const NoteDot *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const NoteHead *item, XmlWriter &xml, WriteContext &ctx) { write(static_cast<const Symbol *>(item), xml, ctx); }

void TWrite::write(const NoteLine *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Ottava *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    for (Pid pid : {Pid::OTTAVA_TYPE, Pid::NUMBERS_ONLY}) {
        if (item->property_flags(pid) == PropertyFlags::NOSTYLE) {
            write_property(item, xml, pid);
        }
    }
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Page *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    for (System *system : item->systems()) {
        write(system, xml, ctx);
    }
    xml.end_element();
}

void TWrite::write(const PalmMute *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Parenthesis *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);
    write_property(item, xml, Pid::HORIZONTAL_DIRECTION);
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Part *item, XmlWriter &xml, WriteContext &ctx) {
    if (item->is_shared_part()) {
        write(to_shared_part(item), xml, ctx);
        return;
    }

    xml.start_element(item, {{"id", item->id().to_uint64()}});

    write_item_eid(item, xml);

    write_properties(item, xml, ctx);

    if (SharedPart *shared_part = item->shared_part()) {
        DO_ASSERT(shared_part->eid().is_valid());
        xml.tag("sharedPart", shared_part->eid().to_std_string());
    }

    xml.end_element();
}

void TWrite::write(const SharedPart *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item, {{"id", item->id().to_uint64()}});

    write_item_eid(item, xml);

    xml.tag(property_name(Pid::SHARED_PART_ENABLED), item->get_property(Pid::SHARED_PART_ENABLED).to_bool());

    write_properties(item, xml, ctx);

    xml.end_element();
}

void TWrite::write_properties(const Part *item, XmlWriter &xml, WriteContext &ctx) {
    auto should_write_staff = [&ctx](const Staff *staff) {
        if (!ctx.should_write_range()) {
            return true;
        }

        const WriteRange &range = ctx.range().value();
        const staff_idx_t idx = staff->idx();

        return idx >= range.start_staff_idx && idx < range.end_staff_idx;
    };

    for (const Staff *staff : item->staves()) {
        if (should_write_staff(staff)) {
            write(staff, xml, ctx);
        }
    }

    if (bool show = item->get_property(Pid::VISIBLE).to_bool(); !show) {
        xml.tag("show", show);
    }

    if (item->soloist()) {
        xml.tag("soloist", item->soloist());
    }

    if (item->hide_when_empty() != AutoOnOff::AUTO) {
        xml.tag("hideWhenEmpty", TConv::to_xml(item->hide_when_empty()));
    }

    if (item->hide_staves_when_individually_empty()) {
        xml.tag("hideStavesWhenIndividuallyEmpty", item->hide_staves_when_individually_empty());
    }

    if (item->prefer_sharp_flat() != PreferSharpFlat::AUTO) {
        switch (item->prefer_sharp_flat()) {
            case PreferSharpFlat::AUTO: break;
            case PreferSharpFlat::FLATS: xml.tag("preferSharpFlat", "flats"); break;
            case PreferSharpFlat::SHARPS: xml.tag("preferSharpFlat", "sharps"); break;
            case PreferSharpFlat::NONE: xml.tag("preferSharpFlat", "none"); break;
            default: break;
        }
    }

    write(item->instrument(), xml, ctx, item);
}

void TWrite::write(const PartialTie *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::TIE_PLACEMENT);
    write_property(item, xml, Pid::PARTIAL_SPANNER_DIRECTION);
    write_properties(static_cast<const SlurTie *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const PartialLyricsLine *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    for (Pid pid : {Pid::VERSE, Pid::CENTER_BETWEEN_STAVES}) {
        if (item->property_flags(pid) == PropertyFlags::NOSTYLE) {
            write_property(item, xml, pid);
        }
    }
    xml.tag("isEndMelisma", item->is_end_melisma());
    write_properties(static_cast<const SLine *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Pedal *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const PickScrape *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const PlayCountText *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::PLAY_COUNT_TEXT_SETTING);
    write_property(item, xml, Pid::PLAY_COUNT_TEXT);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const PlayTechAnnotation *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::PLAY_TECH_TYPE);
    write_property(item, xml, Pid::PLAY);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const Rasgueado *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const RehearsalMark *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const Rest *item, XmlWriter &xml, WriteContext &ctx) {
    if (item->is_gap()) {
        return;
    }
    write_chord_rest_beam(item, xml, ctx);
    xml.start_element(item);
    write_styled_properties(item, xml);
    write_property(item, xml, Pid::ALIGN_WITH_OTHER_RESTS);
    write_properties(to_chord_rest(item), xml, ctx);
    write_items(item->el(), xml, ctx);
    bool write_dots = false;
    for (NoteDot *dot : item->dot_list()) {
        if (!dot->offset().is_null() || !dot->visible() || dot->color() != ctx.configuration()->default_color() ||
            dot->visible() != item->visible()) {
            write_dots = true;
            break;
        }
    }
    if (write_dots) {
        for (NoteDot *dot : item->dot_list()) {
            write(dot, xml, ctx);
        }
    }
    xml.end_element();
}

void TWrite::write(const Segment *item, XmlWriter &xml, WriteContext &) {
    if (item->written()) {
        return;
    }
    item->set_written(true);
    if (item->extra_leading_space().is_zero()) {
        return;
    }
    xml.start_element(item);
    xml.tag("leadingSpace", item->extra_leading_space().val());
    xml.end_element();
}

void TWrite::write(const Slur *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);

    write_property(item, xml, Pid::PARTIAL_SPANNER_DIRECTION);

    write_properties(static_cast<const SlurTie *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write_properties(const SlurTie *item, XmlWriter &xml, WriteContext &ctx) {
    write_properties(static_cast<const Spanner *>(item), xml, ctx);
    int idx = 0;
    for (const SpannerSegment *ss : item->spanner_segments()) {
        write_slur(((SlurTieSegment *) ss), xml, ctx, idx++);
    }
    write_property(item, xml, Pid::SLUR_DIRECTION);
    write_property(item, xml, Pid::SLUR_STYLE_TYPE);
    write_property(item, xml, Pid::MASK_SLURTIE);
}

void TWrite::write_slur(const SlurTieSegment *seg, XmlWriter &xml, WriteContext &ctx, int no) {
    if (!seg->is_user_modified()) {
        return;
    }

    xml.start_element(seg, {{"no", no}});

    double spatium_value = seg->style().spatium();
    if (!seg->ups(Grip::START).off.is_null()) {
        xml.tag_point("o1", seg->ups(Grip::START).off / spatium_value);
    }
    if (!seg->ups(Grip::BEZIER1).off.is_null()) {
        xml.tag_point("o2", seg->ups(Grip::BEZIER1).off / spatium_value);
    }
    if (!seg->ups(Grip::BEZIER2).off.is_null()) {
        xml.tag_point("o3", seg->ups(Grip::BEZIER2).off / spatium_value);
    }
    if (!seg->ups(Grip::END).off.is_null()) {
        xml.tag_point("o4", seg->ups(Grip::END).off / spatium_value);
    }

    if (seg->is_hammer_on_pull_off_segment()) {
        write_properties(to_hammer_on_pull_off_segment(seg), xml, ctx);
    }

    write_item_properties(seg, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Spacer *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    xml.tag("subtype", int(item->spacer_type()));
    write_item_properties(item, xml, ctx);
    xml.tag("space", item->gap().val());
    xml.end_element();
}

void TWrite::write(const Staff *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);

    write_item_eid(item, xml);
    write_item_link(item, xml, ctx);

    // for copy/paste we need to know the actual transposition
    if (ctx.clipboardmode()) {
        Interval v = item->part()->instrument()->transpose(); // TODO: tick?
        if (v.diatonic) {
            xml.tag("transposeDiatonic", v.diatonic);
        }
        if (v.chromatic) {
            xml.tag("transposeChromatic", v.chromatic);
        }
    }

    write(item->staff_type(Fraction(0, 1)), xml, ctx);
    ClefTypeList ct = item->default_clef_type();
    if (ct.concert_clef == ct.transposing_clef) {
        if (ct.concert_clef != ClefType::G) {
            xml.tag("defaultClef", TConv::to_xml(ct.concert_clef));
        }
    } else {
        xml.tag("defaultConcertClef", TConv::to_xml(ct.concert_clef));
        xml.tag("defaultTransposingClef", TConv::to_xml(ct.transposing_clef));
    }

    if (item->hide_when_empty() != AutoOnOff::AUTO) {
        xml.tag("hideWhenEmpty", TConv::to_xml(item->hide_when_empty()));
    }
    if (item->cutaway()) {
        xml.tag("cutaway", item->cutaway());
    }
    if (item->show_if_entire_system_empty()) {
        xml.tag("showIfSystemEmpty", item->show_if_entire_system_empty());
    }
    if (item->hide_system_bar_line()) {
        xml.tag("hideSystemBarLine", item->hide_system_bar_line());
    }
    if (item->merge_matching_rests() != AutoOnOff::AUTO) {
        xml.tag("mergeMatchingRests", TConv::to_xml(item->merge_matching_rests()));
    }
    if (!item->visible()) {
        xml.tag("isStaffVisible", item->visible());
    }

    for (const BracketItem *i : item->score()->brackets(item->idx())) {
        write(i, xml);
    }

    write_property(item, xml, Pid::STAFF_BARLINE_SPAN);
    write_property(item, xml, Pid::STAFF_BARLINE_SPAN_FROM);
    write_property(item, xml, Pid::STAFF_BARLINE_SPAN_TO);
    write_property(item, xml, Pid::STAFF_USERDIST);
    write_property(item, xml, Pid::PLAYBACK_VOICE1);
    write_property(item, xml, Pid::PLAYBACK_VOICE2);
    write_property(item, xml, Pid::PLAYBACK_VOICE3);
    write_property(item, xml, Pid::PLAYBACK_VOICE4);
    write_property(item, xml, Pid::SHOW_MEASURE_NUMBERS);

    xml.end_element();
}

void TWrite::write(const BracketItem *item, XmlWriter &xml) {
    if (item->bracket_type() == BracketType::NO_BRACKET) {
        return;
    }

    xml.start_element(item);

    xml.tag("type", TConv::to_xml(item->bracket_type()));
    write_property(item, xml, Pid::BRACKET_SPAN, /*force*/ true);
    write_property(item, xml, Pid::BRACKET_COLUMN, /*force*/ true);
    write_property(item, xml, Pid::VISIBLE);

    if (item->bracket_type() == BracketType::GROUP) {
        write_property(item, xml, Pid::GROUP_BRACKET_SHOW_TEXT);
        write_property(item, xml, Pid::GROUP_BRACKET_SHOW_BRACKET);
        write(item->label(), xml);
    }

    xml.end_element();
}

void TWrite::write(const StaffState *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    xml.tag("subtype", int(item->staff_state_type()));
    if (item->staff_state_type() == StaffStateType::INSTRUMENT) {
        write(item->instrument(), xml, ctx, nullptr);
    }
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const StaffText *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);

    write_properties(static_cast<const StaffTextBase *>(item), xml, ctx);

    if (const SoundFlag *flag = item->sound_flag()) {
        write_item(flag, xml, ctx);
    }

    xml.end_element();
}

void TWrite::write(const StaveSharingLabel *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);

    write_properties(static_cast<const StaffTextBase *>(item), xml, ctx);

    xml.end_element();
}

void TWrite::write(const StaffTextBase *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);
    write_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write_properties(const StaffTextBase *item, XmlWriter &xml, WriteContext &ctx) {
    for (const ChannelActions &s : item->channel_actions()) {
        int channel = s.channel;
        for (const String &name : s.midi_action_names) {
            xml.tag("MidiAction", {{"channel", channel}, {"name", name}});
        }
    }
    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        if (!item->channel_name(voice).is_empty()) {
            xml.tag("channelSwitch", {{"voice", voice}, {"name", item->channel_name(voice)}});
        }
    }
    if (item->set_aeolus_stops()) {
        for (int i = 0; i < 4; ++i) {
            xml.tag("aeolus", {{"group", i}}, item->aeolus_stop(i));
        }
    }
    if (item->swing()) {
        DurationType swing_unit;
        if (item->swing_parameters().swing_unit == Constants::division / 2) {
            swing_unit = DurationType::V_EIGHTH;
        } else if (item->swing_parameters().swing_unit == Constants::division / 4) {
            swing_unit = DurationType::V_16TH;
        } else {
            swing_unit = DurationType::V_ZERO;
        }
        int swing_ratio = item->swing_parameters().swing_ratio;
        xml.tag("swing", {{"unit", TConv::to_xml(swing_unit)}, {"ratio", swing_ratio}});
    }

    write_properties(to_text_base(item), xml, ctx, true);
}

void TWrite::write(const StaffType *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element("StaffType", {{"group", TConv::to_xml(item->group())}});
    if (!item->xml_name().is_empty()) {
        xml.tag("name", item->xml_name());
    }
    if (!item->staff_label().empty()) {
        write(item->staff_label(), xml);
    }
    if (item->lines() != 5) {
        xml.tag("lines", item->lines());
    }
    if (!real_is_equal(item->line_distance().val(), 1.0)) {
        xml.tag("lineDistance", item->line_distance().val());
    }
    if (!real_is_null(item->yoffset().val())) {
        xml.tag("yoffset", item->yoffset().val());
    }
    if (!real_is_equal(item->user_mag(), 1.0)) {
        xml.tag("mag", item->user_mag());
    }
    if (item->is_small()) {
        xml.tag("small", item->is_small());
    }
    if (item->step_offset()) {
        xml.tag("stepOffset", item->step_offset());
    }
    if (!item->gen_clef()) {
        xml.tag("clef", item->gen_clef());
    }
    if (item->stemless()) {
        xml.tag("stemless", item->stemless());
    }
    if (!item->show_barlines()) {
        xml.tag("barlines", item->show_barlines());
    }
    if (!item->gen_timesig()) {
        xml.tag("timesig", item->gen_timesig());
    }
    if (item->invisible()) {
        xml.tag("invisible", item->invisible());
    }
    if (item->color() != ctx.configuration()->default_color()) {
        xml.tag_property(Pid::COLOR, item->color());
    }
    if (item->group() == StaffGroup::STANDARD) {
        xml.tag("noteheadScheme", TConv::to_xml(item->note_head_scheme()), TConv::to_xml(NoteHeadScheme::HEAD_NORMAL));
    }
    xml.tag("instrumentLabelVisibility", TConv::to_xml(item->instrument_label_visibility()),
            TConv::to_xml(InstrumentLabelVisibility::AUTO));
    if (item->group() == StaffGroup::STANDARD || item->group() == StaffGroup::PERCUSSION) {
        if (!item->gen_keysig()) {
            xml.tag("keysig", item->gen_keysig());
        }
        if (!item->show_ledger_lines()) {
            xml.tag("ledgerlines", item->show_ledger_lines());
        }
    } else {
        xml.tag("durations", item->gen_durations());
        xml.tag("durationFontName", item->duration_font_name()); // write font names anyway for backward compatibility
        xml.tag("durationFontSize", item->duration_font_size());
        xml.tag("durationFontY", item->duration_font_user_y());
        if (item->sym_repeat() != TablatureSymbolRepeat::NEVER) {
            xml.tag("symbolRepeat", int(item->sym_repeat()));
        }
        xml.tag("fretUseTextStyle", item->fret_use_text_style());
        if (item->fret_use_text_style()) {
            xml.tag("fretTextStyle", TConv::to_xml(item->fret_text_style()));
        } else {
            xml.tag("fretPresetIdx", item->fret_preset_idx());
            xml.tag("fretFontSize", item->fret_font_size());
            xml.tag("fretFontY", item->fret_font_user_y());
        }
        xml.tag("linesThrough", item->lines_through());
        xml.tag("minimStyle", int(item->minim_style()));
        xml.tag("onLines", item->on_lines());
        xml.tag("showRests", item->show_rests());
        xml.tag("stemsDown", item->stems_down());
        xml.tag("stemsThrough", item->stem_through());
        xml.tag("upsideDown", item->upside_down());
        xml.tag("showTabFingering", item->show_tab_fingering(), false);
        xml.tag("useNumbers", item->use_numbers());
        // only output "showBackTied" if different from !"stemless"
        // to match the behaviour in 2.0.2 scores (or older)
        if (item->show_back_tied() != !item->stemless()) {
            xml.tag("showBackTied", item->show_back_tied());
        }
    }
    xml.end_element();
}

void TWrite::write(const StaffTypeChange *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    if (item->staff_type()) {
        write(item->staff_type(), xml, ctx);
    }
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Stem *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_item_properties(item, xml, ctx);
    write_property(item, xml, Pid::USER_LEN);
    write_property(item, xml, Pid::LINE_WIDTH);
    xml.end_element();
}

void TWrite::write(const StemSlash *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Sticking *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const StringData *item, XmlWriter &xml) {
    xml.start_element("StringData");
    xml.tag("frets", item->frets());
    for (const InstrString &strg : item->string_list()) {
        XmlWriter::Attributes attrs;

        if (strg.open) {
            attrs.push_back({"open", "1"});
        }

        if (strg.use_flat) {
            attrs.push_back({"useFlat", "1"});
        }

        xml.tag("string", attrs, strg.pitch);
    }
    xml.end_element();
}

void TWrite::write(const StringTunings *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);

    write_property(item, xml, Pid::STRINGTUNINGS_PRESET);

    xml.tag("visibleStrings", TConv::to_xml(item->visible_strings()));

    if (!item->string_data()->is_null()) {
        write(item->string_data(), xml);
    }

    write_properties(static_cast<const StaffTextBase *>(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const Symbol *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    xml.tag("name", SymNames::name_for_sym_id(item->sym()));
    if (item->score_font()) {
        xml.tag("font", item->score_font()->name());
        write_property(item, xml, Pid::SYMBOLS_SIZE);
        write_property(item, xml, Pid::SYMBOL_ANGLE);
    }
    write_properties(static_cast<const BSymbol *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const FSymbol *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    xml.tag("font", item->font().family().id());
    xml.tag("fontsize", item->font().point_size_f());
    xml.tag("code", item->code());
    write_properties(static_cast<const BSymbol *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const System *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    if (item->system_divider_left() && item->system_divider_left()->is_user_modified()) {
        write(item->system_divider_left(), xml, ctx);
    }
    if (item->system_divider_right() && item->system_divider_right()->is_user_modified()) {
        write(item->system_divider_right(), xml, ctx);
    }
    xml.end_element();
}

void TWrite::write(const SystemDivider *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item, {{"type", (item->divider_type() == SystemDividerType::LEFT ? "left" : "right")}});

    write_property(item, xml, Pid::SCORE_FONT);
    write_property(item, xml, Pid::SYMBOLS_SIZE);
    write_property(item, xml, Pid::SYMBOL_ANGLE);

    write_properties(static_cast<const BSymbol *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const SystemText *item, XmlWriter &xml, WriteContext &ctx) { write(static_cast<const StaffTextBase *>(item), xml, ctx); }

void TWrite::write(const SoundFlag *item, XmlWriter &xml, WriteContext &) {
    if (item->sound_presets().empty() && item->playing_technique().empty()) {
        return;
    }

    xml.start_element(item);

    write_property(item, xml, Pid::PLAY);

    if (!item->sound_presets().empty()) {
        xml.tag("presets", item->sound_presets().join(u","));
    }

    if (!item->playing_technique().empty()) {
        xml.tag("playingTechnique", item->playing_technique());
    }

    write_property(item, xml, Pid::APPLY_TO_ALL_STAVES);

    xml.end_element();
}

void TWrite::write(const Tapping *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);

    xml.tag("hand", TConv::to_xml(item->hand()));

    if (item->half_slur_above() && item->half_slur_above()->is_user_modified()) {
        write(item->half_slur_above(), xml, ctx);
    }
    if (item->half_slur_below() && item->half_slur_below()->is_user_modified()) {
        write(item->half_slur_below(), xml, ctx);
    }

    write_properties(to_articulation(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const TappingHalfSlur *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);

    xml.tag("isHalfSlurAbove", item->is_half_slur_above());
    write_properties(static_cast<const SlurTie *>(item), xml, ctx);

    xml.end_element();
}

void TWrite::write(const TempoText *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::PLAY);
    xml.tag("tempo", TConv::to_xml(item->tempo(), Constants::tempo_precision));
    if (item->follow_text()) {
        xml.tag("followText", item->follow_text());
    }
    switch (item->tempo_text_type()) {
        case TempoTextType::NORMAL: break;
        case TempoTextType::A_TEMPO: xml.tag("type", "aTempo"); break;
        case TempoTextType::TEMPO_PRIMO: xml.tag("type", "tempoPrimo"); break;
    }
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const Text *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(to_text_base(item), xml, ctx, true);
    xml.end_element();
}

void TWrite::write(const TextLine *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    if (item->system_flag()) {
        xml.start_element(item, {{"system", "1"}});
    } else {
        xml.start_element(item);
    }
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Tie *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::TIE_PLACEMENT);
    write_properties(static_cast<const SlurTie *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const TimeSig *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::TIMESIG_TYPE);
    write_item_properties(item, xml, ctx);

    xml.tag("sigN", item->numerator());
    xml.tag("sigD", item->denominator());
    if (item->stretch() != Fraction(1, 1)) {
        xml.tag("stretchN", item->stretch().numerator());
        xml.tag("stretchD", item->stretch().denominator());
    }
    write_property(item, xml, Pid::NUMERATOR_STRING);
    write_property(item, xml, Pid::DENOMINATOR_STRING);
    if (!item->groups().empty()) {
        write(&item->groups(), xml, ctx);
    }
    write_property(item, xml, Pid::SHOW_COURTESY);
    write_property(item, xml, Pid::IS_COURTESY);
    write_property(item, xml, Pid::SCALE);

    xml.end_element();
}

void TWrite::write(const TremoloSingleChord *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);

    write_property(item, xml, Pid::TREMOLO_TYPE);
    write_property(item, xml, Pid::PLAY);
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const TremoloTwoChord *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }

    xml.start_element(item);

    write_property(item, xml, Pid::TREMOLO_TYPE);
    write_property(item, xml, Pid::TREMOLO_STYLE);
    write_property(item, xml, Pid::PLAY);
    write_item_properties(item, xml, ctx);

    // write manual adjustments to file
    int idx = item->direction_idx();
    if (item->user_modified()) {
        double spatium_value = item->spatium();

        xml.start_element("Fragment");
        xml.tag("y1", item->beam_fragment().py1[idx] / spatium_value);
        xml.tag("y2", item->beam_fragment().py2[idx] / spatium_value);
        xml.end_element();
    }

    // this info is used for regression testing
    // l1/l2 is the beam position of the layout engine
    if (MScore::test_mode) {
        double spatium8 = item->spatium() * .125;
        xml.tag("l1", int(lrint(item->beam_fragment().py1[idx] / spatium8)));
        xml.tag("l2", int(lrint(item->beam_fragment().py2[idx] / spatium8)));
    }
    xml.end_element();
}

void TWrite::write(const TremoloBar *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_property(item, xml, Pid::MAG);
    write_property(item, xml, Pid::LINE_WIDTH);
    write_property(item, xml, Pid::PLAY);
    for (const PitchValue &v : item->points()) {
        xml.tag("point", {{"time", v.time}, {"pitch", v.pitch}, {"vibrato", v.vibrato}});
    }
    write_item_properties(item, xml, ctx);
    xml.end_element();
}

void TWrite::write(const Trill *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    xml.tag("subtype", TConv::to_xml(item->trill_type()));

    if (item->property_flags(Pid::ORNAMENT_STYLE) == PropertyFlags::NOSTYLE) {
        write_property(item, xml, Pid::ORNAMENT_STYLE);
    }
    write_properties(static_cast<const SLine *>(item), xml, ctx);

    if (item->ornament()) {
        write(item->ornament(), xml, ctx);
    }
    xml.end_element();
}

void TWrite::write(const Tuplet *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_item_properties(item, xml, ctx);

    write_property(item, xml, Pid::NORMAL_NOTES);
    write_property(item, xml, Pid::ACTUAL_NOTES);
    write_property(item, xml, Pid::P1);
    write_property(item, xml, Pid::P2);

    xml.tag("baseNote", TConv::to_xml(item->base_len().type()));
    if (int dots = item->base_len().dots()) {
        xml.tag("baseDots", dots);
    }

    if (item->number()) {
        xml.start_element("Number", item->number());
        write_property(item->number(), xml, Pid::TEXT_STYLE);
        write_property(item->number(), xml, Pid::TEXT);
        xml.end_element();
    }

    write_styled_properties(item, xml);

    xml.end_element();
}

void TWrite::write(const Vibrato *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    xml.tag("subtype", TConv::to_xml(item->vibrato_type()));
    write_properties(static_cast<const SLine *>(item), xml, ctx);
    xml.end_element();
}

void TWrite::write(const Volta *item, XmlWriter &xml, WriteContext &ctx) {
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.tag("endings", TConv::to_xml(item->endings()));
    xml.end_element();
}

void TWrite::write(const WhammyBar *item, XmlWriter &xml, WriteContext &ctx) {
    if (!ctx.can_write(item)) {
        return;
    }
    xml.start_element(item);
    write_properties(static_cast<const TextLineBase *>(item), xml, ctx);
    xml.end_element();
}

//---------------------------------------------------------
//   writeVoiceMove
//    write <move> and starting <voice> tags to denote
//    change in position.
//    Returns true if <voice> tag was written.
//---------------------------------------------------------

static bool write_voice_move(XmlWriter &xml, WriteContext &ctx, Segment *seg, const Fraction &start_tick, track_idx_t track,
                             int *last_track_written_ptr) {
    bool voice_tag_written = false;
    int &last_track_written = *last_track_written_ptr;
    if ((last_track_written < static_cast<int>(track)) && !ctx.clipboardmode()) {
        while (last_track_written < (static_cast<int>(track) - 1)) {
            xml.tag("voice");
            ++last_track_written;
        }
        xml.start_element("voice");
        ctx.set_cur_tick(start_tick);
        ctx.set_cur_track(track);
        ++last_track_written;
        voice_tag_written = true;
    }

    if ((ctx.cur_tick() != seg->tick()) || (track != ctx.cur_track())) {
        Location curr = Location::absolute();
        Location dest = Location::absolute();
        curr.set_frac(ctx.cur_tick());
        dest.set_frac(seg->tick());
        curr.set_track(static_cast<int>(ctx.cur_track()));
        dest.set_track(static_cast<int>(track));

        dest.to_relative(curr);
        dest.set_is_time_tick(seg->is_time_tick_type());
        TWrite::write(&dest, xml, ctx);

        ctx.set_cur_tick(seg->tick());
        ctx.set_cur_track(track);
    }

    return voice_tag_written;
}

static void write_time_sig(Score *score, const Fraction &tick, XmlWriter &xml, WriteContext &ctx) {
    Fraction tsf = score->sigmap()->timesig(tick).nominal();
    TimeSig *ts = Factory::create_time_sig(score->dummy());
    ts->set_sig(tsf);
    TWrite::write(ts, xml, ctx);
    ts->master_score()->eid_register()->remove_item(ts);
    delete ts;
}

//---------------------------------------------------------
//   writeSegments
//    ls  - write upto this segment (excluding)
//          can be zero
//---------------------------------------------------------
void TWrite::write_segments(XmlWriter &xml, WriteContext &ctx, track_idx_t strack, track_idx_t etrack, Segment *sseg, Segment *eseg,
                            bool write_system_elements, bool force_time_sig) {
    bool clip = ctx.clipboardmode();

    // in clipboard mode, ls might be in an mmrest
    // since we are traversing regular measures,
    // force them out of mmRest
    if (clip) {
        Measure *lm = eseg ? eseg->measure() : 0;
        Measure *fm = sseg ? sseg->measure() : 0;
        if (lm && lm->is_mm_rest()) {
            lm = lm->mm_rest_last();
            if (lm) {
                eseg = lm->next_measure() ? lm->next_measure()->first() : nullptr;
            } else {
                LOGD("writeSegments: no measure for end segment in mmrest");
            }
        }
        if (fm && fm->is_mm_rest()) {
            fm = fm->mm_rest_first();
            if (fm) {
                sseg = fm->first(SegmentType::CHORD_REST);
            }
        }
    }

    std::vector<Segment *> segments;
    for (Segment *segment = sseg; segment && segment != eseg; segment = segment->next1()) {
        segments.push_back(segment);
    }

    write_segments(xml, ctx, strack, etrack, segments, write_system_elements, force_time_sig);
}

void TWrite::write_segments(XmlWriter &xml, WriteContext &ctx, track_idx_t strack, track_idx_t etrack,
                            const std::vector<Segment *> &segments, bool write_system_elements, bool force_time_sig) {
    if (segments.empty()) {
        return;
    }

    Segment *sseg = segments.front();
    Score *score = sseg->score();
    Fraction start_tick = ctx.cur_tick();

    int last_track_written = static_cast<int>(strack - 1); // for counting necessary <voice> tags
    for (track_idx_t track = strack; track < etrack; ++track) {
        if (!ctx.can_write_voice(track)) {
            if (force_time_sig && track2voice(track) == 0) {
                bool voice_tag_written = write_voice_move(xml, ctx, sseg, start_tick, track, &last_track_written);
                write_time_sig(score, start_tick, xml, ctx);
                if (voice_tag_written) {
                    xml.end_element(); // </voice>
                }
            }

            continue;
        }

        bool voice_tag_written = false;
        bool time_sig_written = false; // for forceTimeSig
        bool cr_written = false;       // for forceTimeSig
        bool key_sig_written = false;  // for forceTimeSig

        for (Segment *segment : segments) {
            if (!segment->enabled()) {
                continue;
            }
            if (track == 0) {
                segment->set_written(false);
            }
            EngravingItem *e = segment->element(track);

            //
            // special case: - barline span > 1
            //               - part (excerpt) staff starts after
            //                 barline element
            bool need_move = (segment->tick() != ctx.cur_tick() || (static_cast<int>(track) > last_track_written));
            if ((segment->is_end_bar_line_type()) && !e && write_system_elements && ((track % ::mu::engraving::voices) == 0)) {
                // search barline:
                for (int idx = static_cast<int>(track - ::mu::engraving::voices); idx >= 0;
                     idx -= static_cast<int>(::mu::engraving::voices)) {
                    if (segment->element(idx)) {
                        int o_diff = ctx.track_diff();
                        ctx.set_track_diff(idx); // staffIdx should be zero
                        TWrite::write_item(segment->element(idx), xml, ctx);
                        ctx.set_track_diff(o_diff);
                        break;
                    }
                }
            }
            for (EngravingItem *e1 : segment->annotations()) {
                if (e1->generated()) {
                    continue;
                }
                bool write_system = write_system_elements;
                if (!write_system) {
                    ElementType et = e1->type();
                    if ((et == ElementType::REHEARSAL_MARK) || (et == ElementType::SYSTEM_TEXT) || (et == ElementType::TRIPLET_FEEL) ||
                        (et == ElementType::PLAYTECH_ANNOTATION) || (et == ElementType::CAPO) || (et == ElementType::STRING_TUNINGS) ||
                        (et == ElementType::JUMP) || (et == ElementType::MARKER) || (et == ElementType::TEMPO_TEXT) ||
                        (et == ElementType::VOLTA) || (et == ElementType::GRADUAL_TEMPO_CHANGE) || (et == ElementType::PLAY_COUNT_TEXT)) {
                        write_system = (e1->track() == track); // always show these on appropriate staves
                    }
                }
                if (e1->track() != track || (e1->system_flag() && !write_system)) {
                    continue;
                }
                if (need_move) {
                    voice_tag_written |= write_voice_move(xml, ctx, segment, start_tick, track, &last_track_written);
                    need_move = false;
                }
                TWrite::write_item(e1, xml, ctx);
            }

            if (!e || !ctx.can_write(e)) {
                continue;
            }
            if (e->generated()) {
                continue;
            }
            if (force_time_sig && track2voice(track) == 0 && segment->segment_type() == SegmentType::CHORD_REST && !time_sig_written &&
                !cr_written) {
                // Ensure that <voice> tag is open
                voice_tag_written |= write_voice_move(xml, ctx, segment, start_tick, track, &last_track_written);
                // we will miss a key sig!
                if (!key_sig_written) {
                    Key ck = score->staff(track2staff(track))->concert_key(segment->tick());
                    Key tk = score->staff(track2staff(track))->key(segment->tick());
                    KeySig *ks = Factory::create_key_sig(score->dummy());
                    ks->set_key(ck, tk);
                    TWrite::write(ks, xml, ctx);
                    ks->master_score()->eid_register()->remove_item(ks);
                    delete ks;
                    key_sig_written = true;
                }
                // we will miss a time sig!
                write_time_sig(score, segment->tick(), xml, ctx);
                time_sig_written = true;
            }
            if (need_move) {
                voice_tag_written |= write_voice_move(xml, ctx, segment, start_tick, track, &last_track_written);
                // needMove = false; //! NOTE Not necessary, because needMove is currently never read again.
            }
            if (e->is_chord_rest()) {
                ChordRest *cr = to_chord_rest(e);
                write_tuplet_start(cr, xml, ctx);
            }
            TWrite::write_item(e, xml, ctx);

            if (e->is_chord_rest()) {
                ChordRest *cr = to_chord_rest(e);
                write_tuplet_end(cr, xml, ctx);
            }

            if (!(e->is_rest() && to_rest(e)->is_gap())) {
                TWrite::write(segment, xml, ctx); // write only once
            }
            if (force_time_sig) {
                if (segment->segment_type() == SegmentType::KEY_SIG) {
                    key_sig_written = true;
                }
                if (segment->segment_type() == SegmentType::TIME_SIG) {
                    time_sig_written = true;
                }
                if (segment->segment_type() == SegmentType::CHORD_REST) {
                    cr_written = true;
                }
            }
        }

        if (voice_tag_written) {
            xml.end_element(); // </voice>
        }
    }
}
