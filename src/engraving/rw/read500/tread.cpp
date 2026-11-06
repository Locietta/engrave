/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
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

#include "tread.h"

#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>

#include "iengravingconfiguration.h" // IWYU pragma: keep

#include "../../types/typesconv.h"
#include "../../types/symnames.h"
#include "../../infrastructure/rtti.h"
#include "../../infrastructure/htmlparser.h"

#include "../../compat/midi/event.h"

#include "../../dom/accidental.h"
#include "../../dom/actionicon.h"
#include "../../dom/ambitus.h"
#include "../../dom/arpeggio.h"
#include "../../dom/articulation.h"
#include "../../dom/bagpembell.h"
#include "../../dom/barline.h"
#include "../../dom/beam.h"
#include "../../dom/bend.h"
#include "../../dom/box.h"
#include "../../dom/bracket.h"
#include "../../dom/breath.h"
#include "../../dom/capo.h"
#include "../../dom/chord.h"
#include "../../dom/chordbracket.h"
#include "../../dom/chordline.h"
#include "../../dom/chordlist.h"
#include "../../dom/clef.h"
#include "../../dom/drumset.h"
#include "../../dom/dynamic.h"
#include "../../dom/excerpt.h"
#include "../../dom/expression.h"
#include "../../dom/factory.h"
#include "../../dom/fermata.h"
#include "../../dom/figuredbass.h"
#include "../../dom/fingering.h"
#include "../../dom/fret.h"
#include "../../dom/glissando.h"
#include "../../dom/gradualtempochange.h"
#include "../../dom/groups.h"
#include "../../dom/guitarbend.h"
#include "../../dom/hairpin.h"
#include "../../dom/hammeronpulloff.h"
#include "../../dom/harmonicmark.h"
#include "../../dom/harmony.h"
#include "../../dom/harppedaldiagram.h"
#include "../../dom/hook.h"
#include "../../dom/image.h"
#include "../../dom/instrchange.h"
#include "../../dom/instrument.h"
#include "../../dom/jump.h"
#include "../../dom/keysig.h"
#include "../../dom/laissezvib.h"
#include "../../dom/layoutbreak.h"
#include "../../dom/ledgerline.h"
#include "../../dom/letring.h"
#include "../../dom/line.h"
#include "../../dom/linkedobjects.h"
#include "../../dom/lyrics.h"
#include "../../dom/marker.h"
#include "../../dom/masterscore.h"
#include "../../dom/measure.h"
#include "../../dom/measurenumber.h"
#include "../../dom/measurerepeat.h"
#include "../../dom/mmrest.h"
#include "../../dom/mmrestrange.h"
#include "../../dom/mscore.h"
#include "../../dom/note.h"
#include "../../dom/notedot.h"
#include "../../dom/noteline.h"
#include "../../dom/ornament.h"
#include "../../dom/ottava.h"
#include "../../dom/page.h"
#include "../../dom/palmmute.h"
#include "../../dom/parenthesis.h"
#include "../../dom/part.h"
#include "../../dom/partialtie.h"
#include "../../dom/pedal.h"
#include "../../dom/playcounttext.h"
#include "../../dom/playtechannotation.h"
#include "../../dom/rasgueado.h"
#include "../../dom/rehearsalmark.h"
#include "../../dom/rest.h"
#include "../../dom/score.h"
#include "../../dom/segment.h"
#include "../../dom/sharedpart.h"
#include "../../dom/slur.h"
#include "../../dom/slurtie.h"
#include "../../dom/soundflag.h"
#include "../../dom/spacer.h"
#include "../../dom/spanner.h"
#include "../../dom/staff.h"
#include "../../dom/staffstate.h"
#include "../../dom/stafftext.h"
#include "../../dom/stafftextbase.h"
#include "../../dom/stafftype.h"
#include "../../dom/stafftypechange.h"
#include "../../dom/stavesharinglabel.h"
#include "../../dom/stem.h"
#include "../../dom/stemslash.h"
#include "../../dom/sticking.h"
#include "../../dom/stringtunings.h"
#include "../../dom/system.h"
#include "../../dom/systemdivider.h"
#include "../../dom/systemtext.h"
#include "../../dom/tapping.h"
#include "../../dom/tempotext.h"
#include "../../dom/text.h"
#include "../../dom/textline.h"
#include "../../dom/textlinebase.h"
#include "../../dom/tie.h"
#include "../../dom/timesig.h"
#include "../../dom/tremolobar.h"
#include "../../dom/tremolosinglechord.h"
#include "../../dom/tremolotwochord.h"
#include "../../dom/trill.h"
#include "../../dom/tuplet.h"
#include "../../dom/vibrato.h"
#include "../../dom/volta.h"
#include "../../dom/whammybar.h"

#include "../../editing/editstaffbrackets.h"

#include "../xmlreader.h"
#include "../compat/compatutils.h"
#include "../compat/tremolocompat.h"
#include "readcontext.h"

#include "log.h"

using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::read500;

void TRead::read_item(EngravingItem *item, XmlReader &xml, ReadContext &ctx) {
    switch (item->type()) {
        case ElementType::ACCIDENTAL: read(item_cast<Accidental *>(item), xml, ctx); break;
        case ElementType::ACTION_ICON: read(item_cast<ActionIcon *>(item), xml, ctx); break;
        case ElementType::AMBITUS: read(item_cast<Ambitus *>(item), xml, ctx); break;
        case ElementType::ARPEGGIO: read(item_cast<Arpeggio *>(item), xml, ctx); break;
        case ElementType::CHORD_BRACKET: read(item_cast<ChordBracket *>(item), xml, ctx); break;
        case ElementType::ARTICULATION: read(item_cast<Articulation *>(item), xml, ctx); break;
        case ElementType::TAPPING: read(item_cast<Tapping *>(item), xml, ctx); break;
        case ElementType::BAGPIPE_EMBELLISHMENT: read(item_cast<BagpipeEmbellishment *>(item), xml, ctx); break;
        case ElementType::BAR_LINE: read(item_cast<BarLine *>(item), xml, ctx); break;
        case ElementType::BEAM: read(item_cast<Beam *>(item), xml, ctx); break;
        case ElementType::BEND: read(item_cast<Bend *>(item), xml, ctx); break;
        case ElementType::HBOX: read(item_cast<HBox *>(item), xml, ctx); break;
        case ElementType::VBOX: read(item_cast<VBox *>(item), xml, ctx); break;
        case ElementType::FBOX: read(item_cast<FBox *>(item), xml, ctx); break;
        case ElementType::TBOX: read(item_cast<TBox *>(item), xml, ctx); break;
        case ElementType::BRACKET: read(item_cast<Bracket *>(item), xml, ctx); break;
        case ElementType::BREATH: read(item_cast<Breath *>(item), xml, ctx); break;
        case ElementType::CHORD: read(item_cast<Chord *>(item), xml, ctx); break;
        case ElementType::CHORDLINE: read(item_cast<ChordLine *>(item), xml, ctx); break;
        case ElementType::CLEF: read(item_cast<Clef *>(item), xml, ctx); break;
        case ElementType::CAPO: read(item_cast<Capo *>(item), xml, ctx); break;
        case ElementType::DYNAMIC: read(item_cast<Dynamic *>(item), xml, ctx); break;
        case ElementType::EXPRESSION: read(item_cast<Expression *>(item), xml, ctx); break;
        case ElementType::FERMATA: read(item_cast<Fermata *>(item), xml, ctx); break;
        case ElementType::FIGURED_BASS: read(item_cast<FiguredBass *>(item), xml, ctx); break;
        case ElementType::FINGERING: read(item_cast<Fingering *>(item), xml, ctx); break;
        case ElementType::FRET_DIAGRAM: read(item_cast<FretDiagram *>(item), xml, ctx); break;
        case ElementType::GLISSANDO: read(item_cast<Glissando *>(item), xml, ctx); break;
        case ElementType::GRADUAL_TEMPO_CHANGE: read(item_cast<GradualTempoChange *>(item), xml, ctx); break;
        case ElementType::GUITAR_BEND: read(item_cast<GuitarBend *>(item), xml, ctx); break;
        case ElementType::HAIRPIN: read(item_cast<Hairpin *>(item), xml, ctx); break;
        case ElementType::HAMMER_ON_PULL_OFF: read(item_cast<HammerOnPullOff *>(item), xml, ctx); break;
        case ElementType::HARMONY: read(item_cast<Harmony *>(item), xml, ctx); break;
        case ElementType::HARMONIC_MARK: read(item_cast<HarmonicMark *>(item), xml, ctx); break;
        case ElementType::HARP_DIAGRAM: read(item_cast<HarpPedalDiagram *>(item), xml, ctx); break;
        case ElementType::HOOK: read(item_cast<Hook *>(item), xml, ctx); break;
        case ElementType::IMAGE: read(item_cast<Image *>(item), xml, ctx); break;
        case ElementType::INSTRUMENT_CHANGE: read(item_cast<InstrumentChange *>(item), xml, ctx); break;
        case ElementType::JUMP: read(item_cast<Jump *>(item), xml, ctx); break;
        case ElementType::KEYSIG: read(item_cast<KeySig *>(item), xml, ctx); break;
        case ElementType::LAISSEZ_VIB: read(item_cast<LaissezVib *>(item), xml, ctx); break;
        case ElementType::LAYOUT_BREAK: read(item_cast<LayoutBreak *>(item), xml, ctx); break;
        case ElementType::LEDGER_LINE: read(item_cast<LedgerLine *>(item), xml, ctx); break;
        case ElementType::LET_RING: read(item_cast<LetRing *>(item), xml, ctx); break;
        case ElementType::LYRICS: read(item_cast<Lyrics *>(item), xml, ctx); break;
        case ElementType::LYRICSLINE: read(item_cast<LyricsLine *>(item), xml, ctx); break;
        case ElementType::MARKER: read(item_cast<Marker *>(item), xml, ctx); break;
        case ElementType::MEASURE_NUMBER: read(item_cast<MeasureNumber *>(item), xml, ctx); break;
        case ElementType::MEASURE_REPEAT: read(item_cast<MeasureRepeat *>(item), xml, ctx); break;
        case ElementType::MMREST: read(item_cast<MMRest *>(item), xml, ctx); break;
        case ElementType::MMREST_RANGE: read(item_cast<MMRestRange *>(item), xml, ctx); break;
        case ElementType::NOTE: read(item_cast<Note *>(item), xml, ctx); break;
        case ElementType::NOTEDOT: read(item_cast<NoteDot *>(item), xml, ctx); break;
        case ElementType::NOTEHEAD: read(item_cast<NoteHead *>(item), xml, ctx); break;
        case ElementType::NOTELINE: read(item_cast<NoteLine *>(item), xml, ctx); break;
        case ElementType::PAGE: read(item_cast<Page *>(item), xml, ctx); break;
        case ElementType::PALM_MUTE: read(item_cast<PalmMute *>(item), xml, ctx); break;
        case ElementType::PARTIAL_LYRICSLINE: read(item_cast<PartialLyricsLine *>(item), xml, ctx); break;
        case ElementType::PARTIAL_TIE: read(item_cast<PartialTie *>(item), xml, ctx); break;
        case ElementType::PEDAL: read(item_cast<Pedal *>(item), xml, ctx); break;
        case ElementType::PLAY_COUNT_TEXT: read(item_cast<PlayCountText *>(item), xml, ctx); break;
        case ElementType::PLAYTECH_ANNOTATION: read(item_cast<PlayTechAnnotation *>(item), xml, ctx); break;
        case ElementType::RASGUEADO: read(item_cast<Rasgueado *>(item), xml, ctx); break;
        case ElementType::REHEARSAL_MARK: read(item_cast<RehearsalMark *>(item), xml, ctx); break;
        case ElementType::REST: read(item_cast<Rest *>(item), xml, ctx); break;
        case ElementType::ORNAMENT: read(item_cast<Ornament *>(item), xml, ctx); break;
        case ElementType::OTTAVA: read(item_cast<Ottava *>(item), xml, ctx); break;
        case ElementType::SEGMENT: read(item_cast<Segment *>(item), xml, ctx); break;
        case ElementType::SLUR: read(item_cast<Slur *>(item), xml, ctx); break;
        case ElementType::SPACER: read(item_cast<Spacer *>(item), xml, ctx); break;
        case ElementType::STAFF_STATE: read(item_cast<StaffState *>(item), xml, ctx); break;
        case ElementType::STAFF_TEXT: read(item_cast<StaffText *>(item), xml, ctx); break;
        case ElementType::STAVE_SHARING_LABEL: read(item_cast<StaveSharingLabel *>(item), xml, ctx); break;
        case ElementType::STAFFTYPE_CHANGE: read(item_cast<StaffTypeChange *>(item), xml, ctx); break;
        case ElementType::STEM: read(item_cast<Stem *>(item), xml, ctx); break;
        case ElementType::STEM_SLASH: read(item_cast<StemSlash *>(item), xml, ctx); break;
        case ElementType::STICKING: read(item_cast<Sticking *>(item), xml, ctx); break;
        case ElementType::STRING_TUNINGS: read(item_cast<StringTunings *>(item), xml, ctx); break;
        case ElementType::SYMBOL: read(item_cast<Symbol *>(item), xml, ctx); break;
        case ElementType::SOUND_FLAG: read(item_cast<SoundFlag *>(item), xml, ctx); break;
        case ElementType::FSYMBOL: read(item_cast<FSymbol *>(item), xml, ctx); break;
        case ElementType::SYSTEM: read(item_cast<System *>(item), xml, ctx); break;
        case ElementType::SYSTEM_DIVIDER: read(item_cast<SystemDivider *>(item), xml, ctx); break;
        case ElementType::SYSTEM_TEXT: read(item_cast<SystemText *>(item), xml, ctx); break;
        case ElementType::TEMPO_TEXT: read(item_cast<TempoText *>(item), xml, ctx); break;
        case ElementType::TEXT: read(item_cast<Text *>(item), xml, ctx); break;
        case ElementType::TEXTLINE: read(item_cast<TextLine *>(item), xml, ctx); break;
        case ElementType::TIE: read(item_cast<Tie *>(item), xml, ctx); break;
        case ElementType::TIMESIG: read(item_cast<TimeSig *>(item), xml, ctx); break;
        case ElementType::TREMOLO_SINGLECHORD: read(item_cast<TremoloSingleChord *>(item), xml, ctx); break;
        case ElementType::TREMOLO_TWOCHORD: read(item_cast<TremoloTwoChord *>(item), xml, ctx); break;
        case ElementType::TREMOLOBAR: read(item_cast<TremoloBar *>(item), xml, ctx); break;
        case ElementType::TRILL: read(item_cast<Trill *>(item), xml, ctx); break;
        case ElementType::TUPLET: read(item_cast<Tuplet *>(item), xml, ctx); break;
        case ElementType::VIBRATO: read(item_cast<Vibrato *>(item), xml, ctx); break;
        case ElementType::VOLTA: read(item_cast<Volta *>(item), xml, ctx); break;
        case ElementType::WHAMMY_BAR: read(item_cast<WhammyBar *>(item), xml, ctx); break;
        default:
            UNREACHABLE;
            LOGE() << "Not implemented read for type: " << item->type_name();
            break;
    }
}

PropertyValue TRead::read_property_value(Pid id, XmlReader &e, ReadContext &ctx) {
    switch (property_type(id)) {
        case PType::BOOL: return PropertyValue(bool(e.read_int()));
        case PType::INT:
        case PType::SIZE_T: return PropertyValue(e.read_int());
        case PType::REAL: return PropertyValue(e.read_double());
        case PType::SPATIUM: return PropertyValue(Spatium(e.read_double()));
        case PType::ABSOLUTE: return PropertyValue(Spatium(e.read_double())); //! NOTE type mm, but stored in xml as spatium
        case PType::TEMPO: return PropertyValue(e.read_double());
        case PType::FRACTION: return PropertyValue::from_value(e.read_fraction());

        case PType::SYMID: return PropertyValue(TConv::from_xml(e.read_ascii_text(), SymId::NO_SYM));
        case PType::COLOR: return PropertyValue::from_value(e.read_color());
        case PType::ORNAMENT_STYLE: return PropertyValue::from_value(TConv::from_xml(e.read_ascii_text(), OrnamentStyle::DEFAULT));
        case PType::ORNAMENT_INTERVAL: return PropertyValue(TConv::from_xml(e.read_text(), ::mu::engraving::default_ornament_interval));
        case PType::ORNAMENT_SHOW_ACCIDENTAL: return OrnamentShowAccidental(e.read_int());
        case PType::POINT: return PropertyValue::from_value(e.read_point());
        case PType::SCALE: return PropertyValue::from_value(e.read_scale());
        case PType::SIZE: return PropertyValue::from_value(e.read_size());
        case PType::STRING: return PropertyValue(e.read_text());

        case PType::ALIGN: return PropertyValue(TConv::from_xml(e.read_text(), Align()));
        case PType::ALIGN_H: return PropertyValue(TConv::from_xml(e.read_ascii_text(), AlignH::HCENTER));
        case PType::PLACEMENT_V: return PropertyValue(TConv::from_xml(e.read_ascii_text(), PlacementV::ABOVE));
        case PType::PLACEMENT_H: return PropertyValue(TConv::from_xml(e.read_ascii_text(), PlacementH::LEFT));
        case PType::TEXT_PLACE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), TextPlace::AUTO));
        case PType::DIRECTION_V: return PropertyValue(TConv::from_xml(e.read_ascii_text(), DirectionV::AUTO));
        case PType::DIRECTION_H: return PropertyValue(TConv::from_xml(e.read_ascii_text(), DirectionH::AUTO));
        case PType::ORIENTATION: return PropertyValue(TConv::from_xml(e.read_ascii_text(), Orientation::VERTICAL));

        case PType::LAYOUTBREAK_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), LayoutBreakType::NOBREAK));
        case PType::VELO_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), VeloType::OFFSET_VAL));
        case PType::GLISS_STYLE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), GlissandoStyle::CHROMATIC));
        case PType::GLISS_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), GlissandoType::STRAIGHT));
        case PType::BARLINE_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), BarLineType::NORMAL));

        case PType::NOTEHEAD_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), NoteHeadType::HEAD_AUTO));
        case PType::NOTEHEAD_SCHEME: return PropertyValue(TConv::from_xml(e.read_ascii_text(), NoteHeadScheme::HEAD_AUTO));
        case PType::NOTEHEAD_GROUP: return PropertyValue(TConv::from_xml(e.read_ascii_text(), NoteHeadGroup::HEAD_NORMAL));

        case PType::CLEF_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), ClefType::G));
        case PType::CLEF_TO_BARLINE_POS: return ClefToBarlinePosition(e.read_int());

        case PType::DYNAMIC_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), DynamicType::OTHER));

        case PType::LINE_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), LineType::SOLID));
        case PType::HOOK_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), HookType::NONE));

        case PType::KEY_MODE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), KeyMode::NONE));

        case PType::TEXT_STYLE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), TextStyleType::DEFAULT));
        case PType::SLUR_STYLE_TYPE: return SlurStyleType(e.read_int());

        case PType::CHANGE_METHOD: return PropertyValue(TConv::from_xml(e.read_ascii_text(), ChangeMethod::NORMAL));

        case PType::BEAM_MODE: return PropertyValue(int(0));
        case PType::GROUPS: {
            Groups g;
            TRead::read(&g, e, ctx);
            return PropertyValue::from_value(g.nodes());
        }
        case PType::DURATION_TYPE_WITH_DOTS:
        case PType::INT_VEC: return PropertyValue(TConv::from_xml(String(e.read_text()), std::vector<int>()));

        case PType::PLAYTECH_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), PlayingTechniqueType::NATURAL));
        case PType::TEMPOCHANGE_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), GradualTempoChangeType::UNDEFINED));
        case PType::TIE_PLACEMENT: return PropertyValue(TConv::from_xml(e.read_ascii_text(), TiePlacement::AUTO));
        case PType::VOICE_ASSIGNMENT: return PropertyValue(TConv::from_xml(e.read_ascii_text(), VoiceAssignment::ALL_VOICE_IN_INSTRUMENT));
        case PType::AUTO_ON_OFF: return PropertyValue(TConv::from_xml(e.read_ascii_text(), AutoOnOff::AUTO));
        case PType::PARTIAL_SPANNER_DIRECTION:
            return PropertyValue(TConv::from_xml(e.read_ascii_text(), PartialSpannerDirection::OUTGOING));
        case PType::PARENTHESES_MODE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), ParenthesesMode::NONE));
        case PType::AUTO_CUSTOM_HIDE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), AutoCustomHide::AUTO));
        case PType::MARKER_TYPE: return PropertyValue(TConv::from_xml(e.read_ascii_text(), MarkerType::USER));
        default: ASSERT_X("unhandled PID type"); break;
    }
    return PropertyValue();
}

bool TRead::read_property(EngravingItem *item, const AsciiStringView &tag, XmlReader &xml, ReadContext &ctx, Pid pid) {
    if (tag == property_name(pid)) {
        read_property(item, xml, ctx, pid);
        return true;
    }
    return false;
}

void TRead::read_property(EngravingItem *item, XmlReader &xml, ReadContext &ctx, Pid pid) {
    double spatium = ctx.score() ? ctx.spatium() : item->spatium();
    PropertyValue v = read_property_value(pid, xml, ctx);
    switch (property_type(pid)) {
        case PType::ABSOLUTE: //! NOTE type mm, but stored in xml as spatium
            v = v.value<Spatium>().to_absolute(spatium);
            break;
        case PType::POINT:
            if (item->offset_is_spatium_dependent()) {
                v = v.value<PointF>() * spatium;
            } else {
                v = v.value<PointF>() * ::mu::engraving::dpmm;
            }
            break;
        default: break;
    }

    // Pre-4.4 compatibility: these items now use DIRECTION property
    if (pid == Pid::PLACEMENT && item->has_voice_assignment_properties()) {
        pid = Pid::DIRECTION;
        v = v.value<PlacementV>() == PlacementV::ABOVE ? PropertyValue(DirectionV::UP) : PropertyValue(DirectionV::DOWN);
    }

    if (pid == Pid::OFFSET && ctx.msc_version() < 500) {
        compat::CompatUtils::migrate_offset500(item, v);
    }

    if (!ctx.should_skip_property(pid)) {
        item->set_property(pid, v);
        if (item->is_styled(pid)) {
            item->set_property_flags(pid, PropertyFlags::UNSTYLED);
        }
    }
}

bool TRead::read_styled_property(EngravingItem *item, const AsciiStringView &tag, XmlReader &xml, ReadContext &ctx) {
    for (const StyledProperty &spp : *item->styled_properties()) {
        if (read_property(item, tag, xml, ctx, spp.pid)) {
            return true;
        }
    }
    return false;
}

bool TRead::read_item_properties(EngravingItem *item, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (tag == "eid") {
        read_item_eid(item, e, ctx);
    } else if (TRead::read_property(item, tag, e, ctx, Pid::SIZE_SPATIUM_DEPENDENT)) {
    } else if (TRead::read_property(item, tag, e, ctx, Pid::OFFSET)) {
    } else if (TRead::read_property(item, tag, e, ctx, Pid::MIN_DISTANCE)) {
    } else if (TRead::read_property(item, tag, e, ctx, Pid::AUTOPLACE)) {
    } else if (tag == "track") {
        item->set_track(e.read_int() + ctx.track_offset());
    } else if (tag == "color") {
        item->set_color(e.read_color());
    } else if (tag == "visible") {
        item->set_visible(e.read_int());
    } else if (tag == "linkedTo") {
        read_item_link(item, e, ctx);
    } else if (TRead::read_property(item, tag, e, ctx, Pid::POSITION_LINKED_TO_MASTER)) {
    } else if (TRead::read_property(item, tag, e, ctx, Pid::APPEARANCE_LINKED_TO_MASTER)) {
    } else if (TRead::read_property(item, tag, e, ctx, Pid::EXCLUDE_FROM_OTHER_PARTS)) {
    } else if (tag == "voice") {
        item->set_voice(e.read_int());
    } else if (tag == "tag") {
        e.skip_current_element();
    } else if (TRead::read_property(item, tag, e, ctx, Pid::PLACEMENT)) {
    } else if (tag == "z") {
        item->set_z(e.read_int());
    } else if (TRead::read_property(item, tag, e, ctx, Pid::HAS_PARENTHESES)) {
    } else if (tag == "Parenthesis") {
        Parenthesis *p = Factory::create_parenthesis(item);
        TRead::read(p, e, ctx);
        p->set_track(ctx.track());
        item->add(p);
    } else {
        return false;
    }
    return true;
}

static Spanner *try_create_spanner(const AsciiStringView &tag, ReadContext &ctx, bool require_note_anchor) {
    // Speculative probe: most callers offer every tag they do not understand, so a
    // miss (an ordinary property such as "eid" or "track") is expected and must
    // stay silent. Only a real element type whose spanner anchor mismatches is an
    // error worth reporting elsewhere.
    ElementType type = TConv::from_xml(tag, ElementType::INVALID, /*silent*/ true);
    if (type == ElementType::INVALID) {
        return nullptr;
    }

    EngravingItem *item = Factory::create_item(type, ctx.dummy());
    if (!item || !item->is_spanner()) {
        delete item;
        return nullptr;
    }

    Spanner *spanner = to_spanner(item);
    if ((spanner->anchor() == Spanner::Anchor::NOTE) != require_note_anchor) {
        delete item;
        return nullptr;
    }

    return spanner;
}

void TRead::read_score_spanners(Score *score, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (Spanner *spanner = try_create_spanner(tag, ctx, /*requireNoteAnchor*/ false)) {
            TRead::read_item(spanner, e, ctx);
            if (ctx.paste_mode()) {
                score->undo_add_element(spanner);
            } else {
                score->add_spanner(spanner);
            }
        } else {
            e.unknown();
        }
    }
}

void TRead::read_item_eid(EngravingObject *item, XmlReader &xml, ReadContext &ctx) {
    AsciiStringView s = xml.read_ascii_text();
    EID eid = EID::from_std_string(s);
    if (!eid.is_valid()) {
        // Malformed text: the element keeps no id and gets a fresh one from the
        // library's walk after loading (engrave: assign_missing_ids).
        ctx.score()->master_score()->eid_register()->note_malformed_eid();
        return;
    }

    if (ctx.paste_mode()) {
        // On pasting we must create new EIDs for the elements, as the serialized EID is the EID of the item which was copied
        // This could still be in use elsewhere in the score
        EID real_eid = item->assign_new_eid();
        ctx.register_pasted_eid(eid, real_eid);
        return;
    }

    if (!item->set_eid(eid)) {
        // Another element already has this id, so the file was written by a buggy or
        // hand-edited tool. Keep both elements addressable: this one gets a new id.
        item->assign_new_eid();
    }
}

void TRead::read_item_link(EngravingItem *item, XmlReader &xml, ReadContext &ctx) {
    AsciiStringView s = xml.read_ascii_text();
    EID eid = EID::from_std_string(s);
    DO_ASSERT(eid.is_valid());
    EIDRegister *eid_register = ctx.score()->master_score()->eid_register();
    EngravingObject *main_element = eid_register->item_from_eid(eid);
    IF_ASSERT_FAILED(main_element) {
        LOGE() << "Link failed: Main linked element not found for " << item->type_name() << " at " << ctx.tick().to_string();
        return;
    }
    IF_ASSERT_FAILED(main_element->type() == item->type()) {
        LOGE() << "Link failed: Main element type (" << main_element->type_name() << ") does not match linked item type ("
               << item->type_name() << ") at " << ctx.tick().to_string();
        return;
    }

    item->link_to(main_element);
}

void TRead::read(TextBase *t, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        if (!read_properties(to_text_base(t), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(TempoText *t, XmlReader &e, ReadContext &ctx) {
    /// This property was written in the wrong order from 4.6.0-4.6.2
    /// It was written before the text style and was reset on initialising the text style
    std::optional<double> symbol_size;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (read_property(t, tag, e, ctx, Pid::PLAY)) {
        } else if (tag == "tempo") {
            t->set_tempo(TConv::from_xml(e.read_ascii_text(), Constants::default_tempo));
        } else if (tag == "followText") {
            t->set_follow_text(e.read_int());
        } else if (tag == "type") {
            auto type = e.read_ascii_text();
            if (type == "aTempo") {
                t->set_tempo_text_type(TempoTextType::A_TEMPO);
            } else if (type == "tempoPrimo") {
                t->set_tempo_text_type(TempoTextType::TEMPO_PRIMO);
            } else if (type == "normal") {
                t->set_tempo_text_type(TempoTextType::NORMAL);
            } else {
                e.unknown();
            }
        } else if (tag == "musicSymbolSize") {
            symbol_size = e.read_double();
        } else if (!read_properties(to_text_base(t), e, ctx)) {
            e.unknown();
        }
    }
    // check sanity
    if (t->xml_text().is_empty()) {
        t->set_xml_text(String(u"<sym>metNoteQuarterUp</sym> = %1").arg(int(lrint(t->tempo().to_bpm().val))));
        t->set_visible(false);
    }

    if (symbol_size.has_value()) {
        t->set_property(Pid::MUSIC_SYMBOL_SIZE, symbol_size.value());
        if (t->is_styled(Pid::MUSIC_SYMBOL_SIZE)) {
            t->set_property_flags(Pid::MUSIC_SYMBOL_SIZE, PropertyFlags::UNSTYLED);
        }
    }
}

void TRead::read(StaffText *t, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());
        if (tag == "SoundFlag") {
            SoundFlag *flag = Factory::create_sound_flag(t);
            read(flag, xml, ctx);
            t->set_sound_flag(flag);
        } else if (!read_properties(static_cast<StaffTextBase *>(t), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(StaveSharingLabel *t, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        if (!read_properties(static_cast<StaffTextBase *>(t), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(StaffTextBase *t, XmlReader &xml, ReadContext &ctx) {
    t->clear();

    /// This property was written in the wrong order from 4.6.0-4.6.2
    /// It was written before the text style and was reset on initialising the text style
    std::optional<double> symbol_size;

    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());

        if (tag == "musicSymbolSize") {
            symbol_size = xml.read_double();
        } else if (!read_properties(t, xml, ctx)) {
            xml.unknown();
        }
    }

    if (symbol_size.has_value()) {
        t->set_property(Pid::MUSIC_SYMBOL_SIZE, symbol_size.value());
        if (t->is_styled(Pid::MUSIC_SYMBOL_SIZE)) {
            t->set_property_flags(Pid::MUSIC_SYMBOL_SIZE, PropertyFlags::UNSTYLED);
        }
    }
}

bool TRead::read_properties(StaffTextBase *t, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (tag == "MidiAction") {
        int channel = e.int_attribute("channel", 0);
        String name = e.attribute("name");
        bool found = false;
        size_t n = t->channel_actions().size();
        for (size_t i = 0; i < n; ++i) {
            ChannelActions *a = &t->channel_actions()[i];
            if (a->channel == channel) {
                a->midi_action_names.append(name);
                found = true;
                break;
            }
        }
        if (!found) {
            ChannelActions a;
            a.channel = channel;
            a.midi_action_names.append(name);
            t->channel_actions().push_back(a);
        }
        e.read_next();
    } else if (tag == "channelSwitch" || tag == "articulationChange") {
        voice_idx_t voice = static_cast<voice_idx_t>(e.int_attribute("voice", -1));
        if (voice < ::mu::engraving::voices) {
            t->set_channel_name(voice, e.attribute("name"));
        } else if (voice == muse::nidx) {
            // no voice applies channel to all voices for
            // compatibility
            for (voice_idx_t i = 0; i < ::mu::engraving::voices; ++i) {
                t->set_channel_name(i, e.attribute("name"));
            }
        }
        e.read_next();
    } else if (tag == "aeolus") {
        int group = e.int_attribute("group", -1);
        if (group >= 0 && group < 4) {
            t->set_aeolus_stop(group, e.read_int());
        } else {
            e.read_next();
        }
        t->set_set_aeolus_stops(true);
    } else if (tag == "swing") {
        DurationType swing_unit = TConv::from_xml(e.ascii_attribute("unit"), DurationType::V_INVALID);
        int unit = 0;
        if (swing_unit == DurationType::V_EIGHTH) {
            unit = Constants::division / 2;
        } else if (swing_unit == DurationType::V_16TH) {
            unit = Constants::division / 4;
        } else if (swing_unit == DurationType::V_ZERO) {
            unit = 0;
        }
        int ratio = e.int_attribute("ratio", 60);
        t->set_swing(true);
        t->set_swing_parameters(unit, ratio);
        e.read_next();
    } else if (tag == "capo") {
        int fret_id = e.int_attribute("fretId", 0);
        t->set_capo(fret_id);
        e.read_next();
    } else if (!read_properties(to_text_base(t), e, ctx)) {
        return false;
    }
    return true;
}

void TRead::read(Dynamic *d, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag = e.name();
        if (tag == "subtype") {
            d->set_dynamic_type(e.read_text());
        } else if (tag == "velocity") {
            d->set_velocity(e.read_int());
        } else if (tag == "veloChange") {
            d->set_change_in_velocity(e.read_int());
        } else if (tag == "veloChangeSpeed") {
            d->set_vel_change_speed(TConv::from_xml(e.read_ascii_text(), DynamicSpeed::NORMAL));
        } else if (tag == "play") {
            d->set_play_dynamic(e.read_bool());
        } else if (ctx.msc_version() < 470 && tag == "dynamicsSize") {
            read_property(d, e, ctx, Pid::MUSICAL_SYMBOLS_SCALE);
        } else if (read_property(d, tag, e, ctx, Pid::AVOID_BARLINES)) {
        } else if (read_property(d, tag, e, ctx, Pid::CENTER_ON_NOTEHEAD)) {
        } else if (read_property(d, tag, e, ctx, Pid::ANCHOR_TO_END_OF_PREVIOUS)) {
        } else if (!read_properties(to_text_base(d), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Expression *expr, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag = xml.name();
        if (tag == "snapToDynamics") {
            read_property(expr, xml, ctx, Pid::SNAP_TO_DYNAMICS);
        } else if (!read_properties(to_text_base(expr), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(FretDiagram *d, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "fretDiagram") {
            while (e.read_next_start_element()) {
                const AsciiStringView tag2(e.name());

                if (tag2 == "string") {
                    int no = e.int_attribute("no");
                    while (e.read_next_start_element()) {
                        const AsciiStringView t(e.name());
                        if (t == "dot") {
                            int fret = e.int_attribute("fret", 0);
                            FretDotType dtype = FretItem::name_to_dot_type(e.read_text());
                            d->set_dot(no, fret, true, dtype);
                        } else if (t == "marker") {
                            FretMarkerType mtype = FretItem::name_to_marker_type(e.read_text());
                            d->set_marker(no, mtype);
                        } else if (t == "fingering") {
                            e.read_text();
                            /*setFingering(no, e.readInt()); NOTE:JT todo */
                        } else {
                            e.unknown();
                        }
                    }
                } else if (tag2 == "barre") {
                    int start = e.int_attribute("start", -1);
                    int end = e.int_attribute("end", -1);
                    int fret = e.read_int();

                    d->set_barre(start, end, fret);
                } else if (!read_item_properties(d, e, ctx)) {
                    e.unknown();
                }
            }
        } else if (tag == "showNut") {
            TRead::read_property(d, e, ctx, Pid::FRET_NUT);
        } else if (tag == "orientation") {
            TRead::read_property(d, e, ctx, Pid::ORIENTATION);
        } else if (tag == "strings") {
            TRead::read_property(d, e, ctx, Pid::FRET_STRINGS);
        } else if (tag == "frets") {
            TRead::read_property(d, e, ctx, Pid::FRET_FRETS);
        } else if (tag == "fretOffset") {
            TRead::read_property(d, e, ctx, Pid::FRET_OFFSET);
        } else if (tag == "mag") {
            TRead::read_property(d, e, ctx, Pid::MAG);
        } else if (tag == "Harmony") {
            Harmony *h = new Harmony(d->score()->dummy());
            read(h, e, ctx);
            if (h->chords().empty()) {
                // Invalid harmony
                delete h;
            } else {
                d->add(h);
            }
        } else if (read_property(d, tag, e, ctx, Pid::FRET_SHOW_FINGERINGS)) {
        } else if (read_property(d, tag, e, ctx, Pid::FRET_FINGERING)) {
        } else if (TRead::read_property(d, tag, e, ctx, Pid::EXCLUDE_VERTICAL_ALIGN)) {
        } else if (!read_item_properties(d, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(TremoloBar *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        auto tag = e.name();
        if (tag == "point") {
            PitchValue pv;
            pv.time = e.int_attribute("time");
            pv.pitch = e.int_attribute("pitch");
            pv.vibrato = e.int_attribute("vibrato");
            b->points().push_back(pv);
            e.read_next();
        } else if (tag == "mag") {
            b->set_user_mag(e.read_double(0.1, 10.0));
        } else if (TRead::read_styled_property(b, tag, e, ctx)) {
        } else if (tag == "play") {
            b->set_play(e.read_int());
        } else if (TRead::read_property(b, tag, e, ctx, Pid::LINE_WIDTH)) {
        } else if (!read_item_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Sticking *s, XmlReader &xml, ReadContext &ctx) { read(to_text_base(s), xml, ctx); }

void TRead::read(SystemText *t, XmlReader &xml, ReadContext &ctx) { read(static_cast<StaffTextBase *>(t), xml, ctx); }

void TRead::read(PlayCountText *t, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());

        if (TRead::read_property(t, tag, xml, ctx, Pid::PLAY_COUNT_TEXT_SETTING)) {
        } else if (TRead::read_property(t, tag, xml, ctx, Pid::PLAY_COUNT_TEXT)) {
        } else if (!read_properties(to_text_base(t), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(PlayTechAnnotation *a, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());

        if (TRead::read_property(a, tag, xml, ctx, Pid::PLAY_TECH_TYPE)) {
        } else if (TRead::read_property(a, tag, xml, ctx, Pid::PLAY)) {
        } else if (!read_properties(to_staff_text_base(a), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(RehearsalMark *m, XmlReader &xml, ReadContext &ctx) { read(to_text_base(m), xml, ctx); }

void TRead::read(Instrument *item, XmlReader &e, ReadContext &ctx, Part *part) {
    bool custom_drumset = false;
    bool read_single_note_dynamics = false;

    item->clear_channels(); // remove default channel
    item->set_id(e.attribute("id"));
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "singleNoteDynamics") {
            item->set_single_note_dynamics(e.read_bool());
            read_single_note_dynamics = true;
        } else if (!read_properties(item, e, ctx, part, &custom_drumset)) {
            e.unknown();
        }
    }

    item->update_instrument_id();

    if (item->channel().empty()) { // for backward compatibility
        InstrChannel *fallback_channel = new InstrChannel;
        fallback_channel->set_name(String::from_utf8(InstrChannel::default_name));
        fallback_channel->set_program(item->recognize_midi_program());
        item->append_channel(fallback_channel);
    } else if (item->channel(0)->program() == -1) {
        item->channel(0)->set_program(item->recognize_midi_program());
    }

    if (!read_single_note_dynamics) {
        item->set_single_note_dynamics_from_template();
    }
}

bool TRead::read_properties(Instrument *item, XmlReader &e, ReadContext &ctx, Part *part, bool *custom_drumset) {
    PartAudioSettingsCompat part_audio_setting;
    InstrumentTrackId track_id;
    if (part && part->score()) {
        track_id = {part->score()->parts().size() + 1, item->id()}; //part is not assigned to score, _id field is not correct
    }
    part_audio_setting.instrument_id = track_id;

    const AsciiStringView tag(e.name());
    if (tag == "soundId") {
        item->set_sound_id(e.read_text());
    } else if (tag == "longName") {
        item->set_long_name(read_legacy_staff_name(e)); // Old implementation
    } else if (tag == "shortName") {
        item->set_short_name(read_legacy_staff_name(e)); // Old implementation
    } else if (tag == "InstrumentLabel") {
        read_instrument_label(item->instrument_label(), e); // New implementation
    } else if (tag == "trackName") {
        item->set_track_name(e.read_text());
    } else if (tag == "minPitchA") {
        item->set_min_pitch_a(e.read_int());
    } else if (tag == "minPitchP") {
        item->set_min_pitch_p(e.read_int());
    } else if (tag == "maxPitchA") {
        item->set_max_pitch_a(e.read_int());
    } else if (tag == "maxPitchP") {
        item->set_max_pitch_p(e.read_int());
    } else if (tag == "transposeChromatic") {
        Interval transpose = item->transpose();
        transpose.chromatic = e.read_int();
        item->set_transpose(transpose);
    } else if (tag == "transposeDiatonic") {
        Interval transpose = item->transpose();
        transpose.diatonic = e.read_int();
        item->set_transpose(transpose);
    } else if (tag == "instrumentId") {
        item->set_music_xml_id(e.read_text());
    } else if (tag == "useDrumset") {
        item->set_use_drumset(e.read_int());
        if (item->use_drumset()) {
            item->set_drumset(new Drumset(*sm_drumset));
        }
    } else if (tag == "Drum") {
        // if we see one of this tags, a custom drumset will be created
        if (!item->drumset()) {
            item->set_drumset(new Drumset(*sm_drumset));
        }
        if (!(*custom_drumset)) {
            const_cast<Drumset *>(item->drumset())->clear();
            *custom_drumset = true;
        }
        const_cast<Drumset *>(item->drumset())->load_drum(e);
    } else if (tag == "percussionPanelColumns") {
        // if we see one of these tags, a custom drumset will be created
        if (!item->drumset()) {
            item->set_drumset(new Drumset(*sm_drumset));
        }
        if (!(*custom_drumset)) {
            const_cast<Drumset *>(item->drumset())->clear();
            *custom_drumset = true;
        }
        item->drumset()->set_percussion_panel_columns(e.read_int());
    }
    // support tag "Tablature" for a while for compatibility with existent 2.0 scores
    else if (tag == "Tablature" || tag == "StringData") {
        StringData sd;
        read(&sd, e);
        item->set_string_data(sd);
    } else if (tag == "MidiAction") {
        NamedEventList a;
        read(&a, e);
        item->add_midi_action(a);
    } else if (tag == "Articulation") {
        MidiArticulation a;
        read(&a, e);
        item->add_midi_articulation(a);
    } else if (tag == "Channel" || tag == "channel") {
        InstrChannel *a = new InstrChannel;
        read(a, e, ctx, part, track_id);
        item->append_channel(a);
    } else if (tag == "clef") { // sets both transposing and concert clef
        int idx = e.int_attribute("staff", 1) - 1;
        ClefType ct = TConv::from_xml(e.read_ascii_text(), ClefType::G);
        item->set_clef_type(idx, ClefTypeList(ct, ct));
    } else if (tag == "concertClef") {
        int idx = e.int_attribute("staff", 1) - 1;
        item->set_clef_type(idx, ClefTypeList(TConv::from_xml(e.read_ascii_text(), ClefType::G), item->clef_type(idx).transposing_clef));
    } else if (tag == "transposingClef") {
        int idx = e.int_attribute("staff", 1) - 1;
        item->set_clef_type(idx, ClefTypeList(item->clef_type(idx).concert_clef, TConv::from_xml(e.read_ascii_text(), ClefType::G)));
    } else if (tag == "glissandoStyle") {
        item->set_glissando_style(TConv::from_xml(e.read_ascii_text(), GlissandoStyle::CHROMATIC));
    } else {
        return false;
    }

    return true;
}

void TRead::read(InstrChannel *item, XmlReader &e, ReadContext &ctx, Part *part, const InstrumentTrackId &instr_id) {
    item->set_notify_about_changed_enabled(false);

    // synti = 0;
    item->set_name(e.attribute("name"));
    if (item->name() == "") {
        item->set_name(String::from_utf8(InstrChannel::default_name));
    }

    int midi_port = -1;
    int midi_channel = -1;
    PartAudioSettingsCompat part_audio_setting;
    part_audio_setting.instrument_id = instr_id;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "program") {
            item->set_program(e.int_attribute("value", -1));
            if (item->program() == -1) {
                item->set_program(e.read_int());
            } else {
                e.read_next();
            }
        } else if (tag == "controller") {
            int value = e.int_attribute("value", 0);
            int ctrl = e.int_attribute("ctrl", 0);
            switch (ctrl) {
                case CTRL_HBANK:
                    item->set_bank((value << 7) + (item->bank() & 0x7f));
                    item->set_user_bank_controller(true);
                    break;
                case CTRL_LBANK:
                    item->set_bank((item->bank() & ~0x7f) + (value & 0x7f));
                    item->set_user_bank_controller(true);
                    break;
                case CTRL_VOLUME: item->set_volume(value); break;
                case CTRL_PANPOT: item->set_pan(value); break;
                case CTRL_CHORUS_SEND: item->set_chorus(value); break;
                case CTRL_REVERB_SEND: item->set_reverb(value); break;
                default: {
                    Event ev(ME_CONTROLLER);
                    ev.set_ontime(-1);
                    ev.set_channel(0);
                    ev.set_data_a(ctrl);
                    ev.set_data_b(value);
                    item->add_to_init(ev);
                } break;
            }
            e.read_next();
        } else if (tag == "Articulation") {
            MidiArticulation a;
            read(&a, e);
            item->articulation.push_back(a);
        } else if (tag == "MidiAction") {
            NamedEventList a;
            read(&a, e);
            item->midi_actions.push_back(a);
        } else if (tag == "synti") {
            item->set_synti(e.read_text());
        } else if (tag == "color") {
            item->set_color(e.read_int());
        } else if (tag == "mute") {
            part_audio_setting.mute = e.read_int();
        } else if (tag == "solo") {
            part_audio_setting.solo = e.read_int();
        } else if (tag == "midiPort") {
            midi_port = e.read_int();
        } else if (tag == "midiChannel") {
            midi_channel = e.read_int();
        } else {
            e.unknown();
        }
    }

    item->set_must_update_init(true);

    ctx.add_part_audio_setting_compat(part_audio_setting);

    item->set_notify_about_changed_enabled(true);

    if ((midi_port != -1 || midi_channel != -1) && part && part->score()->is_master()) {
        part->master_score()->add_midi_mapping(item, part, midi_port, midi_channel);
    }
}

void TRead::read(MidiArticulation *item, XmlReader &e) {
    item->name = e.attribute("name");
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "velocity") {
            String text(e.read_text());
            if (text.ends_with(u"%")) {
                text = text.left(text.size() - 1);
            }
            item->velocity = text.to_int();
        } else if (tag == "gateTime") {
            String text(e.read_text());
            if (text.ends_with(u"%")) {
                text = text.left(text.size() - 1);
            }
            item->gate_time = text.to_int();
        } else if (tag == "descr") {
            item->descr = e.read_text();
        } else {
            e.unknown();
        }
    }
}

void TRead::read(NamedEventList *item, XmlReader &e) {
    item->name = e.attribute("name");
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "program") {
            MidiCoreEvent ev(ME_CONTROLLER, 0, CTRL_PROGRAM, e.int_attribute("value", 0));
            item->events.push_back(ev);
            e.skip_current_element();
        } else if (tag == "controller") {
            MidiCoreEvent ev;
            ev.set_type(ME_CONTROLLER);
            ev.set_data_a(e.int_attribute("ctrl", 0));
            ev.set_data_b(e.int_attribute("value", 0));
            item->events.push_back(ev);
            e.skip_current_element();
        } else if (tag == "descr") {
            item->descr = e.read_text();
        } else {
            e.unknown();
        }
    }
}

void TRead::read(InstrumentChange *c, XmlReader &e, ReadContext &ctx) {
    Instrument inst;
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "Instrument") {
            read(&inst, e, ctx, c->part());
        } else if (tag == "init") {
            c->set_init(e.read_bool());
        } else if (!read_properties(to_text_base(c), e, ctx)) {
            e.unknown();
        }
    }

    c->set_instrument(inst);
}

void TRead::read(KeyList *item, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (e.name() == "key") {
            Key k;
            int tick = e.int_attribute("tick", 0);
            if (e.has_attribute("custom")) {
                k = Key::C; // ke.setCustomType(e.intAttribute("custom"));
            } else {
                k = Key(e.int_attribute("idx"));
            }
            KeySigEvent ke;
            ke.set_concert_key(k);
            (*item)[ctx.file_division(tick)] = ke;
            e.read_next();
        } else {
            e.unknown();
        }
    }
}

void TRead::read(KeySig *s, XmlReader &e, ReadContext &ctx) {
    KeySigEvent sig;
    int subtype = 0;
    Part *p = s->part();

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "CustDef") {
            CustDef cd;
            while (e.read_next_start_element()) {
                const AsciiStringView t(e.name());
                if (t == "sym") {
                    AsciiStringView val(e.read_ascii_text());
                    bool valid;
                    SymId id = SymId(val.to_int(&valid));
                    if (!valid) {
                        id = SymNames::sym_id_by_name(val);
                    }
                    cd.sym = id;
                } else if (t == "def") {
                    cd.degree = e.int_attribute("degree", 0);
                    cd.oct_alt = e.int_attribute("octAlt", 0);
                    cd.x_alt = Spatium(e.double_attribute("xAlt", 0.0));
                    e.read_next();
                } else if (t == "pos") { // for older files
                    Spatium prevx = 0_sp;
                    Spatium accidental_gap = ctx.score()->style().style_s(Sid::KEYSIG_ACCIDENTAL_DISTANCE);
                    double spatium_value = s->spatium();
                    // count default x position
                    for (CustDef &cd2 : sig.custom_key_defs()) {
                        prevx += Spatium::from_absolute(s->sym_width(cd2.sym), spatium_value) + accidental_gap + cd2.x_alt;
                    }
                    bool flat = std::string(SymNames::name_for_sym_id(cd.sym).ascii()).find("Flat") != std::string::npos;
                    // if x not there, use default step
                    cd.x_alt = Spatium(e.double_attribute("x", prevx.val())) - prevx;
                    // if y not there, use middle line
                    int line = static_cast<int>(e.double_attribute("y", 2) * 2);
                    cd.degree = (3 - line) % 7;
                    cd.degree += (cd.degree < 0) ? 7 : 0;
                    line += flat ? -1 : 1; // top point in treble for # is -1 (gis), and for b is 1 (es)
                    cd.oct_alt = static_cast<int>((line - (line >= 0 ? 0 : 6)) / 7);
                    e.read_next();
                } else {
                    e.unknown();
                }
            }
            sig.custom_key_defs().push_back(cd);
        } else if (TRead::read_property(s, tag, e, ctx, Pid::SHOW_COURTESY)) {
        } else if (tag == "concertKey") {
            sig.set_concert_key(Key(e.read_int()));
        } else if (tag == "actualKey") {
            sig.set_key(Key(e.read_int()));
        } else if (tag == "custom") {
            e.read_int();
            sig.set_custom(true);
        } else if (tag == "mode") {
            sig.set_mode(TConv::from_xml(e.read_ascii_text(), KeyMode::UNKNOWN));
        } else if (tag == "subtype") {
            subtype = e.read_int();
        } else if (tag == "forInstrumentChange") {
            sig.set_for_instrument_change(e.read_bool());
        } else if (TRead::read_property(s, tag, e, ctx, Pid::IS_COURTESY)) {
        } else if (!read_item_properties(s, e, ctx)) {
            e.unknown();
        }
    }
    // for backward compatibility
    if (!sig.is_valid()) {
        sig.init_from_subtype(subtype);
    }
    if (sig.custom() && sig.custom_key_defs().empty()) {
        sig.set_mode(KeyMode::NONE);
    }
    // if there are more than 6 accidentals in transposing key, it cannot be PreferSharpFlat::AUTO
    if (p && !s->concert_pitch() && (sig.key() > 6 || sig.key() < -6) && p->prefer_sharp_flat() == PreferSharpFlat::AUTO &&
        !p->instrument(s->tick())->transpose().is_zero()) {
        p->set_prefer_sharp_flat(PreferSharpFlat::NONE);
    }

    s->set_key_sig_event(sig);
}

void TRead::read(LaissezVib *lv, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());
        if (TRead::read_property(lv, tag, xml, ctx, Pid::TIE_PLACEMENT)) {
        } else if (TRead::read_property(lv, tag, xml, ctx, Pid::MIN_LENGTH)) {
        } else if (!read_properties(to_slur_tie(lv), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(StaffState *s, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            s->set_staff_state_type(StaffStateType(e.read_int()));
        } else if (tag == "Instrument") {
            Instrument i;
            read(&i, e, ctx, nullptr);
            s->set_instrument(std::move(i));
        } else if (!read_item_properties(s, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(FiguredBass *b, XmlReader &e, ReadContext &ctx) {
    String normalized_text;
    int idx = 0;
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "ticks") {
            b->set_ticks(e.read_fraction());
        } else if (tag == "onNote") {
            b->set_on_note(e.read_int() != 0l);
        } else if (tag == "FiguredBassItem") {
            FiguredBassItem *p_item = b->create_item(idx++);
            p_item->set_track(b->track());
            p_item->set_ownership_parent(b);
            TRead::read(p_item, e, ctx);
            b->append_item(p_item);
            // add item normalized text
            if (!normalized_text.is_empty()) {
                normalized_text.append('\n');
            }
            normalized_text.append(p_item->normalized_text());
        }
        //            else if (tag == "style")
        //                  setStyledPropertyListIdx(e.readElementText());
        else if (!read_properties(to_text_base(b), e, ctx)) {
            e.unknown();
        }
    }
    // if items could be parsed set normalized text
    if (b->items().size() > 0) {
        b->set_xml_text(normalized_text); // this is the text to show while editing
    }
}

void TRead::read(FiguredBassItem *i, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "brackets") {
            i->set_parenth1((FiguredBassItem::Parenthesis) e.int_attribute("b0"));
            i->set_parenth2((FiguredBassItem::Parenthesis) e.int_attribute("b1"));
            i->set_parenth3((FiguredBassItem::Parenthesis) e.int_attribute("b2"));
            i->set_parenth4((FiguredBassItem::Parenthesis) e.int_attribute("b3"));
            i->set_parenth5((FiguredBassItem::Parenthesis) e.int_attribute("b4"));
            e.read_next();
        } else if (tag == "prefix") {
            i->set_prefix((FiguredBassItem::Modifier) (e.read_int()));
        } else if (tag == "digit") {
            i->set_digit(e.read_int());
        } else if (tag == "suffix") {
            i->set_suffix((FiguredBassItem::Modifier) (e.read_int()));
        } else if (tag == "continuationLine") {
            i->set_cont_line((FiguredBassItem::ContLine) (e.read_int()));
        } else if (!read_item_properties(i, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Excerpt *item, XmlReader &e, ReadContext &) {
    const std::vector<Part *> &pl = item->master_score()->parts();
    std::vector<Part *> parts;
    while (e.read_next_start_element()) {
        const AsciiStringView tag = e.name();
        if (tag == "name" || tag == "title") {
            item->set_name(e.read_text().trimmed(), /*saveAndNotify=*/false);
        } else if (tag == "part") {
            size_t part_idx = static_cast<size_t>(e.read_int());
            if (part_idx >= pl.size()) {
                LOGD("Excerpt::read: bad part index");
            } else {
                parts.push_back(pl.at(part_idx));
            }
        }
    }
    item->set_parts(parts);
}

void TRead::read(Fermata *f, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(f, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(Fermata *f, XmlReader &xml, ReadContext &ctx) {
    const AsciiStringView tag(xml.name());

    if (tag == "subtype") {
        AsciiStringView s = xml.read_ascii_text();
        SymId id = SymNames::sym_id_by_name(s);
        f->set_sym_id_and_time_stretch(id);
    } else if (tag == "play") {
        f->set_play(xml.read_bool());
    } else if (tag == "timeStretch") {
        f->set_time_stretch(xml.read_double());
    } else if (tag == "offset") {
        read_item_properties(f, xml, ctx);
    } else if (read_item_properties(f, xml, ctx)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(Image *img, XmlReader &e, ReadContext &ctx) {
    bool loaded = false;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "autoScale") {
            TRead::read_property(img, e, ctx, Pid::AUTOSCALE);
        } else if (tag == "size") {
            TRead::read_property(img, e, ctx, Pid::SIZE);
        } else if (tag == "lockAspectRatio") {
            TRead::read_property(img, e, ctx, Pid::LOCK_ASPECT_RATIO);
        } else if (tag == "sizeIsSpatium") {
            // setting this using the property Pid::SIZE_IS_SPATIUM breaks, because the
            // property setter attempts to maintain a constant size. If we're reading, we
            // don't want to do that, because the stored size will be in:
            //    absolute if size isn't spatium
            //    sp if size is spatium
            img->set_size_is_spatium(e.read_bool());
        } else if (tag == "path") {
            loaded = img->load_from_store(e.read_text().to_std_string());
        } else if (tag == "linkPath") {
            if (img->configuration()->allow_reading_images_from_outside_mscz() && !loaded) {
                loaded = img->load_from_file(e.read_text());
            } else {
                e.skip_current_element();
            }
        } else if (!TRead::read_properties(img, e, ctx)) {
            e.unknown();
        }
    }

    compat::CompatUtils::convert_pre470_image_size(img);
}

void TRead::read(Tuplet *t, XmlReader &e, ReadContext &ctx) {
    Text *number = nullptr;
    Fraction ratio;
    TDuration base_len;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (TRead::read_styled_property(t, tag, e, ctx)) {
        } else if (tag == "bold") { //important that these properties are read after number is created
            bool val = e.read_int();
            if (number) {
                number->set_bold(val);
            }
            if (t->is_styled(Pid::FONT_STYLE)) {
                t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
            }
        } else if (tag == "italic") {
            bool val = e.read_int();
            if (number) {
                number->set_italic(val);
            }
            if (t->is_styled(Pid::FONT_STYLE)) {
                t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
            }
        } else if (tag == "underline") {
            bool val = e.read_int();
            if (number) {
                number->set_underline(val);
            }
            if (t->is_styled(Pid::FONT_STYLE)) {
                t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
            }
        } else if (tag == "strike") {
            bool val = e.read_int();
            if (number) {
                number->set_strike(val);
            }
            if (t->is_styled(Pid::FONT_STYLE)) {
                t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
            }
        } else if (tag == "normalNotes") {
            ratio.set_denominator(e.read_int());
        } else if (tag == "actualNotes") {
            ratio.set_numerator(e.read_int());
        } else if (tag == "p1") {
            t->set_user_point1(e.read_point() * t->style().spatium());
        } else if (tag == "p2") {
            t->set_user_point2(e.read_point() * t->style().spatium());
        } else if (tag == "baseNote") {
            base_len = TDuration(TConv::from_xml(e.read_ascii_text(), DurationType::V_INVALID));
        } else if (tag == "baseDots") {
            base_len.set_dots(e.read_int());
        } else if (tag == "Number") {
            number = Factory::create_text(t, TextStyleType::TUPLET);
            number->set_composition(true);
            Tuplet::reset_number_property(number);
            TRead::read(number, e, ctx);
            number->set_visible(t->visible()); //?? override saved property
            number->set_color(t->color());
            number->set_track(t->track());
            // move property flags from _number back to tuplet
            for (auto p : {Pid::FONT_FACE, Pid::FONT_SIZE, Pid::FONT_STYLE, Pid::ALIGN, Pid::POSITION}) {
                t->set_property_flags(p, number->property_flags(p));
            }
        } else if (!read_item_properties(t, e, ctx)) {
            e.unknown();
        }
    }

    t->set_number(number);
    t->set_base_len(base_len);
    t->set_ratio(ratio);

    Fraction f = t->base_len().fraction() * t->ratio().denominator();
    t->set_ticks(f.reduced());
}

void TRead::read(Beam *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "StemDirection") {
            TRead::read_property(b, e, ctx, Pid::STEM_DIRECTION);
        } else if (TRead::read_styled_property(b, tag, e, ctx)) {
        } else if (tag == "growLeft") {
            b->set_grow_left(e.read_double());
        } else if (tag == "growRight") {
            b->set_grow_right(e.read_double());
        } else if (tag == "Fragment") {
            BeamFragment *f = new BeamFragment;
            int idx = b->direction_idx();
            b->set_user_modified(true);
            double spatium_value = b->spatium();
            while (e.read_next_start_element()) {
                const AsciiStringView tag1(e.name());
                if (tag1 == "y1") {
                    f->py1[idx] = e.read_double() * spatium_value;
                } else if (tag1 == "y2") {
                    f->py2[idx] = e.read_double() * spatium_value;
                } else {
                    e.unknown();
                }
            }
            b->add_beam_fragment(f);
        } else if (tag == "l1" || tag == "l2") { // ignore
            e.skip_current_element();
        } else if (read_property(b, tag, e, ctx, Pid::BEAM_CROSS_STAFF_MOVE)) {
        } else if (!read_item_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Ambitus *a, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(a, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(Ambitus *a, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "head") {
        TRead::read_property(a, e, ctx, Pid::HEAD_GROUP);
    } else if (tag == "headType") {
        TRead::read_property(a, e, ctx, Pid::HEAD_TYPE);
    } else if (tag == "mirror") {
        TRead::read_property(a, e, ctx, Pid::MIRROR_HEAD);
    } else if (tag == "hasLine") {
        a->set_has_line(e.read_int());
    } else if (tag == "lineWidth") {
        TRead::read_property(a, e, ctx, Pid::LINE_WIDTH);
    } else if (tag == "topPitch") {
        a->set_top_pitch(e.read_int(), false);
    } else if (tag == "bottomPitch") {
        a->set_bottom_pitch(e.read_int(), false);
    } else if (tag == "topTpc") {
        a->set_top_tpc(e.read_int(), false);
    } else if (tag == "bottomTpc") {
        a->set_bottom_tpc(e.read_int(), false);
    } else if (tag == "topAccidental") {
        while (e.read_next_start_element()) {
            if (e.name() == "Accidental") {
                TRead::read(a->top_accidental(), e, ctx);
            } else {
                e.skip_current_element();
            }
        }
    } else if (tag == "bottomAccidental") {
        while (e.read_next_start_element()) {
            if (e.name() == "Accidental") {
                TRead::read(a->bottom_accidental(), e, ctx);
            } else {
                e.skip_current_element();
            }
        }
    } else if (read_item_properties(a, e, ctx)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(Accidental *a, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "bracket") {
            int i = e.read_int();
            if (i == 0 || i == 1 || i == 2) {
                a->set_bracket(AccidentalBracket(i));
            }
        } else if (tag == "subtype") {
            a->set_subtype(e.read_ascii_text());
        } else if (tag == "role") {
            a->set_role(TConv::from_xml(e.read_ascii_text(), AccidentalRole::AUTO));
        } else if (tag == "small") {
            a->set_small(e.read_int());
        } else if (read_property(a, tag, e, ctx, Pid::ACCIDENTAL_STACKING_ORDER_OFFSET)) {
        } else if (read_item_properties(a, e, ctx)) {
        } else {
            e.unknown();
        }
    }
}

void TRead::read(Marker *m, XmlReader &e, ReadContext &ctx) {
    /// This property was written in the wrong order from 4.6.0-4.6.2
    /// It was written before the text style and was reset on initialising the text style
    std::optional<double> symbol_size;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "label") {
            AsciiStringView s(e.read_ascii_text());
            m->set_label(String::from_ascii(s.ascii()));
        } else if (read_property(m, tag, e, ctx, Pid::MARKER_TYPE)) {
        } else if (read_property(m, tag, e, ctx, Pid::MARKER_CENTER_ON_SYMBOL)) {
        } else if (tag == "musicSymbolSize") {
            symbol_size = e.read_double();
        } else if (!read_properties(to_text_base(m), e, ctx)) {
            e.unknown();
        }
    }

    if (symbol_size.has_value()) {
        m->set_property(Pid::MUSIC_SYMBOL_SIZE, symbol_size.value());
        if (m->is_styled(Pid::MUSIC_SYMBOL_SIZE)) {
            m->set_property_flags(Pid::MUSIC_SYMBOL_SIZE, PropertyFlags::UNSTYLED);
        }
    }
}

void TRead::read(Jump *j, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "jumpTo") {
            j->set_jump_to(e.read_text());
        } else if (tag == "playUntil") {
            j->set_play_until(e.read_text());
        } else if (tag == "continueAt") {
            j->set_continue_at(e.read_text());
        } else if (tag == "playRepeats") {
            j->set_play_repeats(e.read_bool());
        } else if (!read_properties(to_text_base(j), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(MeasureNumber *n, XmlReader &xml, ReadContext &ctx) { read(static_cast<MeasureNumberBase *>(n), xml, ctx); }

void TRead::read(MeasureNumberBase *b, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        if (!read_properties(b, xml, ctx)) {
            xml.unknown();
        }
    }
}

bool TRead::read_properties(MeasureNumberBase *n, XmlReader &xml, ReadContext &ctx) {
    if (TRead::read_property(n, xml.name(), xml, ctx, Pid::HPLACEMENT)) {
        return true;
    } else {
        return read_properties(to_text_base(n), xml, ctx);
    }
}

void TRead::read(MMRestRange *r, XmlReader &xml, ReadContext &ctx) { read(static_cast<MeasureNumberBase *>(r), xml, ctx); }

void TRead::read(SystemDivider *d, XmlReader &e, ReadContext &ctx) {
    SystemDividerType type = e.attribute("type") == "left" ? SystemDividerType::LEFT : SystemDividerType::RIGHT;
    d->set_divider_type(type);
    TRead::read(static_cast<Symbol *>(d), e, ctx);
}

void TRead::read(ActionIcon *i, XmlReader &e, ReadContext &) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "action") {
            i->set_action_type(TConv::from_xml(e.read_ascii_text(), ActionIconType::UNDEFINED));
        } else {
            e.unknown();
        }
    }
}

void TRead::read(Arpeggio *a, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            a->set_arpeggio_type(TConv::from_xml(e.read_ascii_text(), ArpeggioType::NORMAL));
        } else if (!read_properties(a, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(Arpeggio *a, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "userLen1") {
        a->set_user_len1(e.read_double() * a->spatium());
    } else if (tag == "userLen2") {
        a->set_user_len2(e.read_double() * a->spatium());
    } else if (tag == "span") {
        a->set_span(e.read_int());
    } else if (tag == "play") {
        a->set_play_arpeggio(e.read_bool());
    } else if (tag == "timeStretch") {
        a->set_stretch(e.read_double());
    } else if (!read_item_properties(a, e, ctx)) {
        return false;
    }

    return true;
}

void TRead::read(ChordBracket *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (read_property(b, tag, e, ctx, Pid::BRACKET_HOOK_LEN)) {
        } else if (read_property(b, tag, e, ctx, Pid::BRACKET_HOOK_POS)) {
        } else if (read_property(b, tag, e, ctx, Pid::BRACKET_RIGHT_SIDE)) {
        } else if (!read_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Ornament *o, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        if (!read_properties(o, xml, ctx)) {
            xml.unknown();
        }
    }
}

bool TRead::read_properties(Ornament *o, XmlReader &xml, ReadContext &ctx) {
    const AsciiStringView tag(xml.name());
    if (read_property(o, tag, xml, ctx, Pid::INTERVAL_ABOVE)) {
    } else if (read_property(o, tag, xml, ctx, Pid::INTERVAL_BELOW)) {
    } else if (read_property(o, tag, xml, ctx, Pid::ORNAMENT_SHOW_ACCIDENTAL)) {
    } else if (read_property(o, tag, xml, ctx, Pid::ORNAMENT_SHOW_CUE_NOTE)) {
    } else if (read_property(o, tag, xml, ctx, Pid::START_ON_UPPER_NOTE)) {
    } else if (read_properties(to_articulation(o), xml, ctx)) {
    } else if (tag == "Accidental") {
        Accidental *accidental = Factory::create_accidental(o);
        TRead::read(accidental, xml, ctx);
        accidental->set_track(ctx.track());
        accidental->placement() == PlacementV::ABOVE ? o->set_accidental_above(accidental) : o->set_accidental_below(accidental);
    } else if (tag == "Chord") {
        Chord *chord = Factory::create_chord(ctx.score()->dummy());
        TRead::read(chord, xml, ctx);
        chord->set_track(ctx.track());
        chord->set_is_trill_cue_note(true);
        o->set_cue_note_chord(chord);
        o->set_note_above(chord->notes().front());
        for (Note *note : chord->notes()) {
            compat::CompatUtils::do_migrate_note_parens(note);
        }
    } else {
        return false;
    }
    return true;
}

void TRead::read(Articulation *a, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        if (!read_properties(a, xml, ctx)) {
            xml.unknown();
        }
    }
}

bool TRead::read_properties(Articulation *a, XmlReader &xml, ReadContext &ctx) {
    const AsciiStringView tag(xml.name());

    if (tag == "subtype") {
        AsciiStringView s = xml.read_ascii_text();
        ArticulationTextType text_type = TConv::from_xml(s, ArticulationTextType::NO_TEXT);
        if (text_type != ArticulationTextType::NO_TEXT) {
            a->set_text_type(text_type);
        } else {
            a->set_sym_id(SymNames::sym_id_by_name(s));
        }
    } else if (tag == "channel") {
        a->set_channel_name(xml.attribute("name"));
        xml.read_next();
    } else if (tag == "anchor") {
        TRead::read_property(a, tag, xml, ctx, Pid::ARTICULATION_ANCHOR);
    } else if (tag == "direction") {
        TRead::read_property(a, xml, ctx, Pid::DIRECTION);
    } else if (tag == "ornamentStyle") {
        TRead::read_property(a, xml, ctx, Pid::ORNAMENT_STYLE);
    } else if (tag == "play") {
        a->set_play_articulation(xml.read_bool());
    } else if (tag == "offset") {
        read_item_properties(a, xml, ctx);
    } else if (read_item_properties(a, xml, ctx)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(Tapping *t, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        AsciiStringView tag = xml.name();
        if (tag == "hand") {
            t->set_hand(TConv::from_xml(xml.read_ascii_text(), TappingHand::INVALID));
        } else if (tag == TConv::to_xml(ElementType::TAPPING_HALF_SLUR)) {
            TappingHalfSlur *tapping_half_slur = new TappingHalfSlur(t);
            read(tapping_half_slur, xml, ctx);
            if (tapping_half_slur->is_half_slur_above()) {
                t->set_half_slur_above(tapping_half_slur);
            } else {
                t->set_half_slur_below(tapping_half_slur);
            }
        } else if (!read_properties(to_articulation(t), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(TappingHalfSlur *t, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        if (xml.name() == "isHalfSlurAbove") {
            t->set_is_half_slur_above(xml.read_bool());
        } else if (!read_properties(to_slur(t), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(BagpipeEmbellishment *b, XmlReader &e, ReadContext &) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            b->set_embel_type(TConv::from_xml(e.read_ascii_text(), EmbellishmentType(0)));
        } else {
            e.unknown();
        }
    }
}

void TRead::read(BarLine *b, XmlReader &e, ReadContext &ctx) {
    // initialize span properties with staff values
    b->reset_property(Pid::BARLINE_SPAN);
    b->reset_property(Pid::BARLINE_SPAN_FROM);
    b->reset_property(Pid::BARLINE_SPAN_TO);

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            b->set_bar_line_type(TConv::from_xml(e.read_ascii_text(), BarLineType::NORMAL));
        } else if (tag == "span") {
            b->set_span_staff(e.read_bool());
        } else if (tag == "spanFromOffset") {
            b->set_span_from(e.read_int());
        } else if (tag == "spanToOffset") {
            b->set_span_to(e.read_int());
        } else if (tag == "Articulation") {
            Articulation *a = Factory::create_articulation(b->score()->dummy());
            TRead::read(a, e, ctx);
            b->add(a);
        } else if (tag == "Symbol") {
            Symbol *s = Factory::create_symbol(b);
            s->set_track(b->track());
            TRead::read(s, e, ctx);
            b->add(s);
        } else if (tag == "Image") {
            if (MScore::no_images) {
                e.skip_current_element();
            } else {
                Image *image = Factory::create_image(b);
                image->set_track(b->track());
                TRead::read(image, e, ctx);
                b->add(image);
            }
        } else if (!read_item_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Bend *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (TRead::read_styled_property(b, tag, e, ctx)) {
        } else if (tag == "point") {
            PitchValue pv;
            pv.time = e.int_attribute("time");
            pv.pitch = e.int_attribute("pitch");
            pv.vibrato = e.int_attribute("vibrato");
            b->add_point(pv);
            e.read_next();
        } else if (tag == "play") {
            b->set_play_bend(e.read_bool());
        } else if (!read_item_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Box *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(HBox *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(HBox *b, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (read_property(b, tag, e, ctx, Pid::CREATE_SYSTEM_HEADER)) {
    } else if (read_properties(static_cast<Box *>(b), e, ctx)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(VBox *b, XmlReader &xml, ReadContext &ctx) { TRead::read(static_cast<Box *>(b), xml, ctx); }

void TRead::read(FBox *b, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());
        if (read_property(b, tag, xml, ctx, Pid::FRET_FRAME_TEXT_SCALE)) {
        } else if (read_property(b, tag, xml, ctx, Pid::FRET_FRAME_DIAGRAM_SCALE)) {
        } else if (read_property(b, tag, xml, ctx, Pid::FRET_FRAME_COLUMN_GAP)) {
        } else if (read_property(b, tag, xml, ctx, Pid::FRET_FRAME_ROW_GAP)) {
        } else if (read_property(b, tag, xml, ctx, Pid::FRET_FRAME_CHORDS_PER_ROW)) {
        } else if (read_property(b, tag, xml, ctx, Pid::FRET_FRAME_H_ALIGN)) {
        } else if (TRead::read_properties(static_cast<Box *>(b), xml, ctx)) {
        } else {
            xml.unknown();
        }
    }
}

void TRead::read(TBox *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "Text") {
            TRead::read(b->text(), e, ctx);
        } else if (TRead::read_properties(static_cast<Box *>(b), e, ctx)) {
        } else {
            e.unknown();
        }
    }
}

bool TRead::read_box_properties(Box *b, XmlReader &e, ReadContext &ctx) {
    if (b->is_h_box()) {
        return read_properties(dynamic_cast<HBox *>(b), e, ctx);
    }
    return read_properties(b, e, ctx);
}

bool TRead::read_properties(Box *b, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "height") {
        b->set_box_height(Spatium(e.read_double()));
    } else if (tag == "width") {
        b->set_box_width(Spatium(e.read_double()));
    } else if (tag == "topGap") {
        b->set_top_gap(Spatium(e.read_double()));
        b->set_property_flags(Pid::TOP_GAP, PropertyFlags::UNSTYLED);
    } else if (tag == "bottomGap") {
        b->set_bottom_gap(Spatium(e.read_double()));
        b->set_property_flags(Pid::BOTTOM_GAP, PropertyFlags::UNSTYLED);
    } else if (tag == "leftMargin") {
        b->set_left_margin(e.read_double());
    } else if (tag == "rightMargin") {
        b->set_right_margin(e.read_double());
    } else if (tag == "topMargin") {
        b->set_top_margin(e.read_double());
    } else if (tag == "bottomMargin") {
        b->set_bottom_margin(e.read_double());
    } else if (tag == "boxAutoSize") {
        b->set_auto_size_enabled(e.read_bool());
    } else if (tag == "Text") {
        Text *t;
        if (b->is_t_box()) {
            t = to_t_box(b)->text();
            TRead::read(t, e, ctx);
        } else {
            t = Factory::create_text(b);
            TRead::read(t, e, ctx);
            if (t->empty()) {
                LOGD("read empty text");
            } else {
                b->add(t);
            }
        }
    } else if (tag == "Symbol") {
        Symbol *s = new Symbol(b);
        TRead::read(s, e, ctx);
        b->add(s);
    } else if (tag == "Image") {
        if (MScore::no_images) {
            e.skip_current_element();
        } else {
            Image *image = new Image(b);
            image->set_track(ctx.track());
            TRead::read(image, e, ctx);
            b->add(image);
        }
    } else if (tag == "FretDiagram") {
        FretDiagram *f = Factory::create_fret_diagram(b->score()->dummy());
        TRead::read(f, e, ctx);
        //! TODO Looks like a bug.
        //! The FretDiagram parent must be Segment
        //! there is a method: `Segment* segment() const { return toSegment(parent()); }`,
        //! but when we add it to Box, the parent will be rewritten.
        b->add(f);
    } else if (tag == "HBox") {
        // parent_ is used here (rather than system()) because explicit parent isn't set for this object
        // until it is fully read. parent_ is nonetheless valid and using it here matches MU3 behavior.
        // If we do not set the parent of this new box correctly, it will cause a crash on read()
        // because it needs access to the system it is being added to. (c.r. issue #14643)
        if (b->parent() && b->parent()->is_system()) {
            HBox *hb = new HBox(b->score());
            TRead::read(hb, e, ctx);
            //! TODO Looks like a bug.
            //! The HBox parent must be System
            //! there is a method: `System* system() const { return (System*)parent(); }`,
            //! but when we add it to Box, the parent will be rewritten.
            b->add(hb);
        }
    } else if (tag == "VBox") {
        if (b->parent() && b->parent()->is_system()) {
            VBox *vb = new VBox(b->score());
            TRead::read(vb, e, ctx);
            //! TODO Looks like a bug.
            //! The VBox parent must be System
            //! there is a method: `System* system() const { return (System*)parent(); }`,
            //! but when we add it to Box, the parent will be rewritten.
            b->add(vb);
        }
    } else if (TRead::read_property(b, tag, e, ctx, Pid::PADDING_TO_NOTATION_ABOVE)) {
    } else if (TRead::read_property(b, tag, e, ctx, Pid::PADDING_TO_NOTATION_BELOW)) {
    } else if (TRead::read_properties(static_cast<MeasureBase *>(b), e, ctx)) {
    } else {
        return false;
    }
    return true;
}

bool TRead::read_properties(MeasureBase *b, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "LayoutBreak") {
        LayoutBreak *lb = Factory::create_layout_break(b);
        TRead::read(lb, e, ctx);
        lb->set_track(0);
        bool do_add = true;
        switch (lb->layout_break_type()) {
            case LayoutBreakType::LINE:
                if (b->line_break()) {
                    do_add = false;
                }
                break;
            case LayoutBreakType::PAGE:
                if (b->page_break()) {
                    do_add = false;
                }
                break;
            case LayoutBreakType::SECTION:
                if (b->section_break()) {
                    do_add = false;
                }
                break;
            case LayoutBreakType::NOBREAK:
                if (b->no_break()) {
                    do_add = false;
                }
                break;
        }
        if (do_add) {
            b->add(lb);
            b->cleanup_layout_breaks(false);
        }
    } else if (tag == "StaffTypeChange") {
        StaffTypeChange *stc = Factory::create_staff_type_change(b);
        stc->set_track(ctx.track());
        TRead::read(stc, e, ctx);
        b->add(stc);
    } else if (read_item_properties(b, e, ctx)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(Bracket *b, XmlReader &e, ReadContext &ctx) {
    BracketItem *bi = Factory::create_bracket_item(b->score()->dummy());
    bi->set_bracket_type(TConv::from_xml(e.ascii_attribute("type"), BracketType::NORMAL));

    while (e.read_next_start_element()) {
        if (e.name() == "level") {
            bi->set_column(e.read_int());
        } else if (!read_item_properties(b, e, ctx)) {
            e.unknown();
        }
    }

    b->set_bracket_item(bi);
}

void TRead::read(Breath *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "symbol") {
            b->set_sym_id(SymNames::sym_id_by_name(e.read_ascii_text()));
        } else if (tag == "pause") {
            b->set_pause(e.read_double());
        } else if (!read_item_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(BSymbol *sym, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag = e.name();

    if (read_item_properties(sym, e, ctx)) {
        return true;
    } else if (tag == "systemFlag") {
        sym->set_system_flag(e.read_int());
    } else if (tag == "Symbol") {
        Symbol *s = Factory::create_symbol(sym);
        TRead::read(s, e, ctx);
        sym->add(s);
    } else if (tag == "FSymbol") {
        FSymbol *s = Factory::create_f_symbol(sym);
        TRead::read(s, e, ctx);
        sym->add(s);
    } else if (tag == "Image") {
        if (MScore::no_images) {
            e.skip_current_element();
        } else {
            Image *image = new Image(sym);
            TRead::read(image, e, ctx);
            sym->add(image);
        }
    } else {
        return false;
    }
    return true;
}

void TRead::read(Symbol *sym, XmlReader &e, ReadContext &ctx) {
    SymId sym_id = SymId::NO_SYM;
    String font_name;
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "name") {
            String val(e.read_text());
            sym_id = SymNames::sym_id_by_name(val);
            if (val != "noSym" && sym_id == SymId::NO_SYM) {
                // if symbol name not found, fall back to user names
                // TODO: does it make sense? user names are probably localized
                sym_id = SymNames::sym_id_by_user_name(val);
                if (sym_id == SymId::NO_SYM) {
                    LOGD("unknown symbol <%s>, falling back to no symbol", muPrintable(val));
                    // set a default symbol, or layout() will crash
                    sym_id = SymId::NO_SYM;
                }
            }
            sym->set_sym(sym_id);
        } else if (tag == "font") {
            read_property(sym, e, ctx, Pid::SCORE_FONT);
        } else if (read_property(sym, tag, e, ctx, Pid::SCORE_FONT)) {
        } else if (read_property(sym, tag, e, ctx, Pid::SYMBOLS_SIZE)) {
        } else if (read_property(sym, tag, e, ctx, Pid::SYMBOL_ANGLE)) {
        } else if (tag == "Symbol") {
            Symbol *s = new Symbol(sym);
            TRead::read(s, e, ctx);
            sym->add(s);
        } else if (tag == "Image") {
            if (MScore::no_images) {
                e.skip_current_element();
            } else {
                Image *image = new Image(sym);
                TRead::read(image, e, ctx);
                sym->add(image);
            }
        } else if (!TRead::read_properties(static_cast<BSymbol *>(sym), e, ctx)) {
            e.unknown();
        }
    }

    sym->set_pos(PointF());
}

void TRead::read(SoundFlag *item, XmlReader &xml, ReadContext &) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());

        if (tag == "play") {
            item->set_play(xml.read_bool());
        } else if (tag == "presets") {
            item->set_sound_presets(xml.read_text().split(u","));
        } else if (tag == "playingTechnique") {
            item->set_playing_technique(xml.read_text());
        } else if (tag == "applyToAllStaves") {
            item->set_apply_to_all_staves(xml.read_bool());
        } else {
            xml.unknown();
        }
    }
}

void TRead::read(FSymbol *sym, XmlReader &e, ReadContext &ctx) {
    Font font = sym->font();
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "font") {
            font.set_family(e.read_text(), Font::Type::UNKNOWN);
        } else if (tag == "fontsize") {
            font.set_point_size_f(e.read_double());
        } else if (tag == "code") {
            sym->set_code(e.read_int());
        } else if (!TRead::read_properties(static_cast<BSymbol *>(sym), e, ctx)) {
            e.unknown();
        }
    }

    sym->set_pos(PointF());
    sym->set_font(font);
}

void TRead::read(Chord *ch, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (TRead::read_properties(ch, e, ctx)) {
        } else {
            e.unknown();
        }
    }
}

bool TRead::read_properties(Chord *ch, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (tag == "Note") {
        Note *note = Factory::create_note(ch);
        // the note needs to know the properties of the track it belongs to
        note->set_track(ch->track());
        TRead::read(note, e, ctx);
        ch->add(note);
    } else if (TRead::read_properties(to_chord_rest(ch), e, ctx)) {
    } else if (tag == "Stem") {
        Stem *s = Factory::create_stem(ch);
        TRead::read(s, e, ctx);
        ch->add(s);
    } else if (tag == "Hook") {
        Hook *hook = new Hook(ch);
        TRead::read(hook, e, ctx);
        ch->set_hook(hook);
        ch->add(hook);
    } else if (tag == "appoggiatura") {
        ch->set_note_type(NoteType::APPOGGIATURA);
        e.read_next();
    } else if (tag == "acciaccatura") {
        ch->set_note_type(NoteType::ACCIACCATURA);
        e.read_next();
    } else if (tag == "grace4") {
        ch->set_note_type(NoteType::GRACE4);
        e.read_next();
    } else if (tag == "grace16") {
        ch->set_note_type(NoteType::GRACE16);
        e.read_next();
    } else if (tag == "grace32") {
        ch->set_note_type(NoteType::GRACE32);
        e.read_next();
    } else if (tag == "grace8after") {
        ch->set_note_type(NoteType::GRACE8_AFTER);
        e.read_next();
    } else if (tag == "grace16after") {
        ch->set_note_type(NoteType::GRACE16_AFTER);
        e.read_next();
    } else if (tag == "grace32after") {
        ch->set_note_type(NoteType::GRACE32_AFTER);
        e.read_next();
    } else if (tag == "StemSlash") {
        StemSlash *ss = Factory::create_stem_slash(ch);
        TRead::read(ss, e, ctx);
        ch->add(ss);
    } else if (TRead::read_property(ch, tag, e, ctx, Pid::STEM_DIRECTION)) {
    } else if (tag == "noStem") {
        ch->set_no_stem(e.read_int());
    } else if (tag == "showStemSlash") {
        ch->set_show_stem_slash(e.read_bool());
    } else if (tag == "Arpeggio") {
        Arpeggio *arpeggio = Factory::create_arpeggio(ch);
        arpeggio->set_track(ch->track());
        TRead::read(arpeggio, e, ctx);
        ch->set_arpeggio(arpeggio);
    } else if (tag == "ChordBracket") {
        ChordBracket *bracket = Factory::create_chord_bracket(ch);
        bracket->set_track(ch->track());
        TRead::read(bracket, e, ctx);
        ch->add(bracket);
    } else if (tag == "Tremolo") { // compat
        compat::TremoloCompat tcompat{ch};
        TRead::read(&tcompat, e, ctx);
        if (tcompat.two) {
            tcompat.two->set_ownership_parent(ch);
            tcompat.two->set_duration_type(ch->duration_type());
            ch->set_tremolo_two_chord(tcompat.two, false);
        } else if (tcompat.single) {
            tcompat.single->set_ownership_parent(ch);
            tcompat.single->set_duration_type(ch->duration_type());
            ch->set_tremolo_single_chord(tcompat.single);
        } else {
            UNREACHABLE;
        }
    } else if (tag == "TremoloSingleChord") {
        TremoloSingleChord *trem = Factory::create_tremolo_single_chord(ch);
        trem->set_track(ch->track());
        TRead::read(trem, e, ctx);
        trem->set_duration_type(ch->duration_type());
        ch->set_tremolo_single_chord(trem);
    } else if (tag == "TremoloTwoChord") {
        TremoloTwoChord *trem = Factory::create_tremolo_two_chord(ch);
        trem->set_track(ch->track());
        TRead::read(trem, e, ctx);
        trem->set_duration_type(ch->duration_type());
        ch->set_tremolo_two_chord(trem, false);
    } else if (tag == "ChordLine") {
        ChordLine *cl = Factory::create_chord_line(ch);
        TRead::read(cl, e, ctx);
        ch->add(cl);
    } else if (tag == "combineVoice") {
        read_property(ch, tag, e, ctx, Pid::COMBINE_VOICE);
    } else if (tag == "NoteParenGroup") {
        read_note_paren_group(ch, e, ctx);
    } else {
        return false;
    }

    if (!ch->visible()) {
        // By convention, the chord can't be set to invisible (only its elements can).
        // If it was written and read as invisible that was an error so we fix it here.
        ch->set_visible(true);
    }

    return true;
}

bool TRead::read_properties(ChordRest *ch, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (tag == "durationType") {
        ch->set_duration_type(TConv::from_xml(e.read_ascii_text(), DurationType::V_QUARTER));
        if (ch->actual_duration_type().type() != DurationType::V_MEASURE) {
            ch->set_ticks(ch->actual_duration_type().fraction());
        }
    } else if (tag == "BeamMode") {
        ch->set_beam_mode(TConv::from_xml(e.read_ascii_text(), BeamMode::AUTO));
    } else if (tag == "Articulation") {
        Articulation *atr = Factory::create_articulation(ch);
        atr->set_track(ch->track());
        TRead::read(atr, e, ctx);
        ch->add(atr);
    } else if (tag == "Ornament") {
        Ornament *ornament = Factory::create_ornament(ch);
        ornament->set_track(ch->track());
        TRead::read(ornament, e, ctx);
        ch->add(ornament);
    } else if (tag == "Tapping") {
        Tapping *tapping = Factory::create_tapping(ch);
        tapping->set_track(ch->track());
        TRead::read(tapping, e, ctx);
        ch->add(tapping);
    } else if (tag == "small") {
        ch->set_small(e.read_int());
    } else if (tag == "duration") {
        ch->set_ticks(e.read_fraction());
    } else if (tag == "dots") {
        ch->set_dots(e.read_int());
    } else if (tag == "staffMove") {
        ch->set_staff_move(e.read_int());
        ch->check_staff_move_validity();
    } else if (tag == "Lyrics") {
        Lyrics *lyr = Factory::create_lyrics(ch);
        lyr->set_track(ctx.track());
        TRead::read(lyr, e, ctx);
        ch->add(lyr);
    } else if (tag == "pos") {
        PointF pt = e.read_point();
        ch->set_offset(pt * ch->spatium());
    } else if (!read_item_properties(ch, e, ctx)) {
        return false;
    }
    return true;
}

static bool chord_line_number(const String &text, double &value) {
    const auto bytes = text.to_std_string();
    if (bytes.empty() || bytes.size() > 128) {
        return false;
    }
    const char *first = bytes.data();
    const char *last = first + bytes.size();
    if (*first == '+') {
        ++first;
        if (first == last || *first == '-' || *first == '+') {
            return false;
        }
    }
    if (first == last) {
        return false;
    }
    const auto result = std::from_chars(first, last, value, std::chars_format::general);
    return result.ec == std::errc() && result.ptr == last && std::isfinite(value);
}

void TRead::read(ChordLine *l, XmlReader &e, ReadContext &ctx) {
    //! NOTE Need separated "given" data and layout data
    ChordLine::LayoutData *ldata = l->mutldata();
    ldata->path = PainterPath();
    std::array<PointF, 4> offsets{};
    bool has_offsets = false;
    std::array<bool, 4> seen{};
    bool invalid_offsets = false;
    bool has_path = false;
    bool has_position = false;
    PointF position;
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "Path") {
            if (has_path) {
                invalid_offsets = true;
            }
            has_path = true;
            PainterPath path;
            PointF curve_to;
            PointF p1;
            int state = 0;
            while (e.read_next_start_element()) {
                const AsciiStringView next_tag(e.name());
                if (next_tag == "Element") {
                    double type_value = -1.0, x = 0.0, y = 0.0;
                    if (!chord_line_number(e.attribute("type"), type_value) || type_value < 0 || type_value > 3 ||
                        std::floor(type_value) != type_value || !chord_line_number(e.attribute("x"), x) ||
                        !chord_line_number(e.attribute("y"), y)) {
                        invalid_offsets = true;
                        e.skip_current_element();
                        continue;
                    }
                    const int type = static_cast<int>(type_value);
                    double spatium = ctx.spatium();
                    x *= spatium;
                    y *= spatium;
                    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1000000.0 || std::abs(y) > 1000000.0) {
                        invalid_offsets = true;
                        e.skip_current_element();
                        continue;
                    }
                    if (state != 0 && type != int(PainterPath::ElementType::CURVE_TO_DATA_ELEMENT)) {
                        invalid_offsets = true;
                    }
                    switch (PainterPath::ElementType(type)) {
                        case PainterPath::ElementType::MOVE_TO_ELEMENT: path.move_to(x, y); break;
                        case PainterPath::ElementType::LINE_TO_ELEMENT: path.line_to(x, y); break;
                        case PainterPath::ElementType::CURVE_TO_ELEMENT:
                            curve_to.rx() = x;
                            curve_to.ry() = y;
                            state = 1;
                            break;
                        case PainterPath::ElementType::CURVE_TO_DATA_ELEMENT:
                            if (state == 1) {
                                p1.rx() = x;
                                p1.ry() = y;
                                state = 2;
                            } else if (state == 2) {
                                path.cubic_to(curve_to, p1, PointF(x, y));
                                state = 0;
                            } else {
                                invalid_offsets = true;
                            }
                            break;
                    }
                    e.skip_current_element(); //needed to go to next EngravingItem in Path
                } else {
                    invalid_offsets = true;
                    e.unknown();
                }
            }
            if (state != 0) {
                invalid_offsets = true;
            }
            ldata->path = path;
            l->set_modified(true);
        } else if (tag == "offset" && ctx.msc_version() >= 500) {
            // The policy can occur later. Preserve legacy 4.70 migration.
            double x = 0.0, y = 0.0;
            if ((!e.has_attribute("x") || chord_line_number(e.attribute("x"), x)) &&
                (!e.has_attribute("y") || chord_line_number(e.attribute("y"), y))) {
                position = PointF(x, y);
                has_position = true;
            } else {
                invalid_offsets = true;
            }
            e.skip_current_element();
        } else if (tag == "o1" || tag == "o2" || tag == "o3" || tag == "o4") {
            const size_t index = tag == "o1" ? 0 : tag == "o2" ? 1 : tag == "o3" ? 2 : 3;
            if (seen[index]) {
                invalid_offsets = true;
            }
            seen[index] = true;
            double x = 0.0, y = 0.0;
            const bool valid = (!e.has_attribute("x") || chord_line_number(e.attribute("x"), x)) &&
                               (!e.has_attribute("y") || chord_line_number(e.attribute("y"), y));
            // Always finish this element and the ChordLine container. Parents may
            // have required children after it (notably ornament cue-chord Notes).
            while (e.read_next_start_element()) {
                invalid_offsets = true;
                e.skip_current_element();
            }
            if (!valid) {
                invalid_offsets = true;
                continue;
            }
            offsets[index] = PointF(x, y);
            has_offsets = true;
        } else if (tag == "subtype") {
            l->set_chord_line_type(TConv::from_xml(e.read_ascii_text(), ChordLineType::NOTYPE));
        } else if (tag == "straight") {
            l->set_straight(e.read_int());
        } else if (tag == "wavy") {
            l->set_wavy(e.read_int());
        } else if (tag == "lengthX") {
            l->set_length_x(e.read_int());
        } else if (tag == "lengthY") {
            l->set_length_y(e.read_int());
        } else if (tag == "play") {
            l->set_play_chord_line(e.read_bool());
        } else if (!read_item_properties(l, e, ctx)) {
            e.unknown();
        }
    }
    if (invalid_offsets) {
        e.raise_error(String::from_utf8("ChordLine offsets require complete finite numeric coordinates (no underflow)"));
        return;
    }
    if (has_offsets && has_path) {
        e.raise_error(String::from_utf8("ChordLine cannot mix legacy Path and o1-o4 offsets"));
        return;
    }
    const double point_unit = l->offset_is_spatium_dependent() ? ctx.spatium() : ::mu::engraving::dpmm;
    if (has_position && !ctx.should_skip_property(Pid::OFFSET)) {
        const PointF physical = position * point_unit;
        if (!std::isfinite(physical.x()) || !std::isfinite(physical.y()) || std::abs(physical.x()) > 1000000.0 ||
            std::abs(physical.y()) > 1000000.0) {
            e.raise_error(String::from_utf8("ChordLine position exceeds supported geometry range"));
            return;
        }
        l->set_property(Pid::OFFSET, physical);
        if (l->is_styled(Pid::OFFSET)) {
            l->set_property_flags(Pid::OFFSET, PropertyFlags::UNSTYLED);
        }
    }
    if (has_offsets) {
        // Decode using score units only. Local staff context is deliberately
        // deferred to layout/validation after the entire document has been read.
        try {
            l->read_source_offsets(offsets, point_unit);
        } catch (const std::runtime_error &error) {
            e.raise_error(String::from_utf8(error.what()));
        }
    }
}

void TRead::read(Clef *c, XmlReader &e, ReadContext &ctx, bool *is_header) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "concertClefType") {
            c->set_concert_clef(TConv::from_xml(e.read_ascii_text(), ClefType::G));
        } else if (tag == "transposingClefType") {
            c->set_transposing_clef(TConv::from_xml(e.read_ascii_text(), ClefType::G));
        } else if (tag == "showCourtesyClef") {
            c->set_show_courtesy(e.read_int());
        } else if (tag == "forInstrumentChange") {
            c->set_for_instrument_change(e.read_bool());
        } else if (tag == "clefToBarlinePos") {
            c->set_clef_to_barline_position(ClefToBarlinePosition(e.read_int()));
        } else if (tag == "isHeader") {
            const bool header = e.read_bool();
            if (is_header) {
                *is_header = header;
            }
        } else if (!read_item_properties(c, e, ctx)) {
            e.unknown();
        }
    }

    if (c->clef_type() == ClefType::INVALID) {
        c->set_clef_type(ClefType::G);
    }
}

void TRead::read(Capo *c, XmlReader &xml, ReadContext &ctx) {
    CapoParams params = c->params();

    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());

        if (tag == "active") {
            params.active = xml.read_bool();
        } else if (tag == "fretPosition") {
            params.fret_position = xml.read_int();
        } else if (tag == "string") {
            string_idx_t idx = static_cast<string_idx_t>(xml.int_attribute("no"));

            while (xml.read_next_start_element()) {
                const AsciiStringView string_tag(xml.name());

                if (string_tag == "apply") {
                    bool apply = xml.read_bool();
                    if (!apply) {
                        params.ignored_strings.insert(idx);
                    }
                } else {
                    xml.unknown();
                }
            }
        } else if (tag == "generateText") {
            c->set_property(Pid::CAPO_GENERATE_TEXT, xml.read_bool());
        } else if (TRead::read_property(c, tag, xml, ctx, Pid::CAPO_TRANSPOSE_MODE)) {
            continue;
        } else if (!read_properties(static_cast<StaffTextBase *>(c), xml, ctx)) {
            xml.unknown();
        }
    }
    params.transpose_mode = c->params().transpose_mode;

    c->set_params(params);
}

bool TRead::read_properties(Clef *c, XmlReader &xml, ReadContext &ctx) { return read_item_properties(c, xml, ctx); }

void TRead::read(Fingering *f, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(to_text_base(f), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Glissando *g, XmlReader &e, ReadContext &ctx) {
    g->erase_spanner_segments();

    staff_idx_t staff_idx = track2staff(ctx.track());
    Staff *staff = ctx.score()->staff(staff_idx);
    if (staff) {
        String instr_id = staff->part()->instrument_id(g->tick());
        g->set_is_harp_gliss(instr_id == "harp");
    }
    g->reset_property(Pid::GLISS_STYLE);

    while (e.read_next_start_element()) {
        const AsciiStringView tag = e.name();
        if (tag == "text") {
            TRead::read_property(g, e, ctx, Pid::GLISS_TEXT);
        } else if (tag == "isHarpGliss" && ctx.paste_mode()) {
            g->set_is_harp_gliss(e.read_bool());
            g->reset_property(Pid::GLISS_STYLE);
        } else if (tag == "easeInSpin") {
            g->set_ease_in(e.read_int());
        } else if (tag == "easeOutSpin") {
            g->set_ease_out(e.read_int());
        } else if (tag == "glissandoShift") {
            g->set_glissando_shift(e.read_bool());
        } else if (!TRead::read_properties(static_cast<SLine *>(g), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(GradualTempoChange *c, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());

        if (TRead::read_property(c, tag, xml, ctx, Pid::TEMPO_CHANGE_TYPE)) {
            continue;
        }

        if (TRead::read_property(c, tag, xml, ctx, Pid::TEMPO_EASING_METHOD)) {
            continue;
        }

        if (TRead::read_property(c, tag, xml, ctx, Pid::TEMPO_CHANGE_FACTOR)) {
            continue;
        }

        if (TRead::read_property(c, tag, xml, ctx, Pid::SNAP_AFTER)) {
            continue;
        }

        if (!read_properties(to_text_line_base(c), xml, ctx)) {
            xml.unknown();
        }
    }
    if (ctx.msc_version() < 470) {
        compat::CompatUtils::set_text_line_text_position_from_align(c);
    }
}

void TRead::read(Groups *g, XmlReader &e, ReadContext &) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "Node") {
            GroupNode n;
            n.pos = e.int_attribute("pos");
            n.action = e.int_attribute("action");
            g->add_node(n);
            e.skip_current_element();
        } else {
            e.unknown();
        }
    }
}

void TRead::read(GuitarBend *g, XmlReader &e, ReadContext &ctx) {
    g->erase_spanner_segments();

    while (e.read_next_start_element()) {
        const AsciiStringView tag = e.name();
        if (tag == "guitarBendType") {
            g->set_bend_type(TConv::from_xml(e.read_ascii_text(), GuitarBendType::BEND));
        } else if (tag == "GuitarBendHold") {
            GuitarBendHold *hold = new GuitarBendHold(g);
            TRead::read(hold, e, ctx);
            g->set_hold_line(hold);
        } else if (TRead::read_property(g, tag, e, ctx, Pid::DIRECTION)) {
        } else if (TRead::read_property(g, tag, e, ctx, Pid::BEND_SHOW_HOLD_LINE)) {
        } else if (TRead::read_property(g, tag, e, ctx, Pid::BEND_START_TIME_FACTOR)) {
        } else if (TRead::read_property(g, tag, e, ctx, Pid::BEND_TARGET_TIME_FACTOR)) {
        } else if (TRead::read_property(g, tag, e, ctx, Pid::BEND_END_TIME_FACTOR)) {
        } else if (TRead::read_property(g, tag, e, ctx, Pid::GUITAR_DIVE_TAB_POS)) {
        } else if (TRead::read_property(g, tag, e, ctx, Pid::GUITAR_BEND_AMOUNT)) {
        } else if (TRead::read_property(g, tag, e, ctx, Pid::VIBRATO_LINE_TYPE)) {
        } else if (TRead::read_property(g, tag, e, ctx, Pid::GUITAR_DIVE_IS_SLACK)) {
        } else if (!TRead::read_properties(static_cast<SLine *>(g), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(GuitarBendHold *h, XmlReader &xml, ReadContext &ctx) {
    h->erase_spanner_segments();

    while (xml.read_next_start_element()) {
        if (!TRead::read_properties(static_cast<SLine *>(h), xml, ctx)) {
            xml.unknown();
        }
    }
}

bool TRead::read_properties(GuitarBendSegment *g, const AsciiStringView &tag, XmlReader &xml, ReadContext &ctx) {
    if (tag == "bendVertexOffset") {
        g->set_vertex_point_off(xml.read_point() * g->style().spatium());
    } else if (tag == "GuitarBendText") {
        GuitarBendText *bend_text = new GuitarBendText(g);
        read(to_text_base(bend_text), xml, ctx);
        g->set_bend_text(bend_text);
    } else {
        return false;
    }

    return true;
}

void TRead::read(Hairpin *h, XmlReader &e, ReadContext &ctx) {
    h->erase_spanner_segments();

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            h->set_hairpin_type(HairpinType(e.read_int()));
        } else if (tag == "hairpinCircledTip") {
            h->set_hairpin_circled_tip(e.read_int());
        } else if (tag == "veloChange") {
            h->set_velo_change(e.read_int());
        } else if (tag == "singleNoteDynamics") {
            h->set_single_note_dynamics(e.read_bool());
        } else if (tag == "veloChangeMethod") {
            h->set_velo_change_method(TConv::from_xml(e.read_ascii_text(), ChangeMethod::NORMAL));
        } else if (read_property(h, tag, e, ctx, Pid::VOICE_ASSIGNMENT)) {
        } else if (read_property(h, tag, e, ctx, Pid::DIRECTION)) {
        } else if (read_property(h, tag, e, ctx, Pid::CENTER_BETWEEN_STAVES)) {
        } else if (read_property(h, tag, e, ctx, Pid::SNAP_BEFORE)) {
        } else if (read_property(h, tag, e, ctx, Pid::SNAP_AFTER)) {
        } else if (!read_properties(to_text_line_base(h), e, ctx)) {
            e.unknown();
        }
    }
    if (ctx.msc_version() < 470) {
        compat::CompatUtils::set_text_line_text_position_from_align(h);
    }
    h->style_changed();
}

void TRead::read(HammerOnPullOff *h, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        if (!read_properties(to_slur(h), xml, ctx)) {
            xml.unknown();
        }
    }
}

static void read_harmony_info(HarmonyInfo *info, XmlReader &e) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "bass") {
            info->set_bass_tpc(e.read_int());
        } else if (tag == "extension") {
            info->set_id(e.read_int());
        } else if (tag == "name") {
            info->set_text_name(e.read_text());
        } else if (tag == "root") {
            info->set_root_tpc(e.read_int());
        } else {
            e.unknown();
        }
    }
}

void TRead::read(Harmony *h, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "bassCase") {
            h->set_bass_case(TConv::from_xml(e.read_ascii_text(), NoteCaseType::AUTO));
        } else if (tag == "rootCase") {
            h->set_root_case(TConv::from_xml(e.read_ascii_text(), NoteCaseType::AUTO));
        } else if (tag == "harmonyInfo") {
            HarmonyInfo *info = new HarmonyInfo(ctx.score());
            read_harmony_info(info, e);
            h->add_chord(info);
        } else if (tag == "degree") {
            int degree_value = 0;
            int degree_alter = 0;
            String degree_type;
            while (e.read_next_start_element()) {
                const AsciiStringView t(e.name());
                if (t == "degree-value") {
                    degree_value = e.read_int();
                } else if (t == "degree-alter") {
                    degree_alter = e.read_int();
                } else if (t == "degree-type") {
                    degree_type = e.read_text();
                } else {
                    e.unknown();
                }
            }
            if (degree_value <= 0 || degree_value > 13 || degree_alter < -2 || degree_alter > 2 ||
                (degree_type != "add" && degree_type != "alter" && degree_type != "subtract")) {
                LOGD("incorrect degree: degreeValue=%d degreeAlter=%d degreeType=%s", degree_value, degree_alter, muPrintable(degree_type));
            } else {
                if (degree_type == "add") {
                    h->add_degree(HDegree(degree_value, degree_alter, HDegreeType::ADD));
                } else if (degree_type == "alter") {
                    h->add_degree(HDegree(degree_value, degree_alter, HDegreeType::ALTER));
                } else if (degree_type == "subtract") {
                    h->add_degree(HDegree(degree_value, degree_alter, HDegreeType::SUBTRACT));
                }
            }
        } else if (TRead::read_property(h, tag, e, ctx, Pid::POS_ABOVE)) {
        } else if (TRead::read_property(h, tag, e, ctx, Pid::HARMONY_TYPE)) {
        } else if (TRead::read_property(h, tag, e, ctx, Pid::PLAY)) {
        } else if (TRead::read_property(h, tag, e, ctx, Pid::HARMONY_VOICE_LITERAL)) {
        } else if (TRead::read_property(h, tag, e, ctx, Pid::HARMONY_VOICING)) {
        } else if (TRead::read_property(h, tag, e, ctx, Pid::HARMONY_DURATION)) {
        } else if (TRead::read_property(h, tag, e, ctx, Pid::HARMONY_BASS_SCALE)) {
        } else if (TRead::read_property(h, tag, e, ctx, Pid::HARMONY_DO_NOT_STACK_MODIFIERS)) {
        } else if (TRead::read_property(h, tag, e, ctx, Pid::EXCLUDE_VERTICAL_ALIGN)) {
        } else if (!read_properties(to_text_base(h), e, ctx)) {
            e.unknown();
        }
    }

    h->after_read();
}

void TRead::read(HarmonicMark *h, XmlReader &xml, ReadContext &ctx) { TRead::read(to_text_line_base(h), xml, ctx); }

void TRead::read(HarpPedalDiagram *h, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag = xml.name();
        if (tag == "isDiagram") {
            h->set_is_diagram(xml.read_bool());
        } else if (tag == "pedalState") {
            while (xml.read_next_start_element()) {
                const AsciiStringView string_tag = xml.name();
                if (string_tag == "string") {
                    HarpStringType str = HarpStringType(xml.int_attribute("name"));
                    PedalPosition pos = PedalPosition(xml.read_int());
                    h->set_pedal(str, pos);
                } else {
                    xml.unknown();
                }
            }
            h->set_playable_tpcs();
        } else if (!read_properties(h, xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(Hook *h, XmlReader &xml, ReadContext &ctx) { TRead::read(static_cast<Symbol *>(h), xml, ctx); }

void TRead::read(LayoutBreak *b, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            TRead::read_property(b, e, ctx, Pid::LAYOUT_BREAK);
        } else if (tag == "pause") {
            TRead::read_property(b, e, ctx, Pid::PAUSE);
        } else if (tag == "startWithLongNames") {
            TRead::read_property(b, e, ctx, Pid::START_WITH_LONG_NAMES);
        } else if (tag == "startWithMeasureOne") {
            TRead::read_property(b, e, ctx, Pid::START_WITH_MEASURE_ONE);
        } else if (tag == "firstSystemIndentation") {
            TRead::read_property(b, e, ctx, Pid::FIRST_SYSTEM_INDENTATION);
        } else if (tag == "showCourtesySig") {
            TRead::read_property(b, e, ctx, Pid::SHOW_COURTESY);
        } else if (!read_item_properties(b, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(LedgerLine *l, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(l, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(LedgerLine *l, XmlReader &e, ReadContext &) {
    const AsciiStringView tag(e.name());

    if (tag == "lineWidth") {
        //! NOTE Probably need to be removed, because it is calculated in Layout
        LedgerLine::LayoutData *ldata = l->mutldata();
        ldata->line_width = (e.read_double() * l->spatium());
    } else if (tag == "lineLen") {
        l->set_len(e.read_double() * l->spatium());
    } else if (tag == "vertical") {
        l->set_vertical(e.read_int());
    } else {
        return false;
    }
    return true;
}

void TRead::read(LetRing *r, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(to_text_line_base(r), e, ctx)) {
            e.unknown();
        }
    }
    if (ctx.msc_version() < 470) {
        compat::CompatUtils::set_text_line_text_position_from_align(r);
    }
}

void TRead::read(Location *l, XmlReader &e, ReadContext &) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "staves") {
            l->set_staff(e.read_int());
        } else if (tag == "voices") {
            l->set_voice(e.read_int());
        } else if (tag == "measures") {
            l->set_measure(e.read_int());
        } else if (tag == "fractions") {
            l->set_frac(e.read_fraction());
        } else if (tag == "grace") {
            l->set_grace_index(e.read_int());
        } else if (tag == "notes") {
            l->set_note(e.read_int());
        } else if (tag == "timeTick") {
            l->set_is_time_tick(e.read_bool());
        } else {
            e.unknown();
        }
    }
}

void TRead::read(Lyrics *l, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!TRead::read_properties(l, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(LyricsLine *l, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(static_cast<SLine *>(l), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(LineSegment *l, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(l, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(LineSegment *l, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "subtype") {
        l->set_spanner_segment_type(SpannerSegmentType(e.read_int()));
    } else if (tag == "off2") {
        l->set_user_off2(e.read_point() * l->style().spatium());
    } else if (l->is_guitar_bend_segment() && read_properties(static_cast<GuitarBendSegment *>(l), tag, e, ctx)) {
    } else if (!read_item_properties(l, e, ctx)) {
        return false;
    }
    return true;
}

bool TRead::read_properties(Lyrics *l, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (tag == "no") {
        l->set_verse(e.read_int());
        if (l->is_even()) {
            l->init_text_style_type(TextStyleType::LYRICS_EVEN);
        }
    } else if (tag == "syllabic") {
        l->set_syllabic(TConv::from_xml(e.read_ascii_text(), LyricsSyllabic::SINGLE));
    } else if (tag == "ticks_f") {
        l->set_ticks(e.read_fraction());
    } else if (TRead::read_property(l, tag, e, ctx, Pid::PLACEMENT)) {
    } else if (tag == "LyricsLine") {
        LyricsLine *ll = Factory::create_lyrics_line(l);
        TRead::read(ll, e, ctx);
        ll->set_tick(ctx.tick());
        l->set_separator(ll);
        ctx.score()->add_unmanaged_spanner(ll);
    } else if (!read_properties(to_text_base(l), e, ctx)) {
        return false;
    }
    return true;
}

void TRead::read(MeasureRepeat *r, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            r->set_num_measures(e.read_int());
        } else if (TRead::read_property(r, tag, e, ctx, Pid::MEASURE_REPEAT_NUMBER_OFFSET)) {
        } else if (!read_properties(to_rest(r), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(MMRest *r, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "Symbol") {
            Symbol *s = new Symbol(r);
            s->set_track(r->track());
            TRead::read(s, e, ctx);
            r->add(s);
        } else if (tag == "Image") {
            if (MScore::no_images) {
                e.skip_current_element();
            } else {
                Image *image = new Image(r);
                image->set_track(r->track());
                TRead::read(image, e, ctx);
                r->add(image);
            }
        } else if (tag == "NoteDot") {
            NoteDot *dot = Factory::create_note_dot(r);
            TRead::read(dot, e, ctx);
            r->add(dot);
        } else if (tag == "mmRestNumberPos") {
            // Old property, deprecated in 4.5
            r->set_number_offset(Spatium(e.read_double()) - ctx.style().style_s(Sid::MM_REST_NUMBER_POS));
        } else if (TRead::read_property(r, tag, e, ctx, Pid::MMREST_NUMBER_OFFSET)) {
        } else if (TRead::read_styled_property(r, tag, e, ctx)) {
        } else if (read_properties(r, e, ctx)) {
        } else {
            e.unknown();
        }
    }
}

bool TRead::read_properties(MMRest *r, XmlReader &xml, ReadContext &ctx) {
    const AsciiStringView tag(xml.name());
    if (tag == "mmRestNumberVisible") {
        r->set_property(Pid::MMREST_NUMBER_VISIBLE, xml.read_bool());
    } else if (TRead::read_properties(to_rest(r), xml, ctx)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(Note *n, XmlReader &e, ReadContext &ctx) {
    n->set_tpc1(Tpc::TPC_INVALID);
    n->set_tpc2(Tpc::TPC_INVALID);

    while (e.read_next_start_element()) {
        if (TRead::read_properties(n, e, ctx)) {
        } else {
            e.unknown();
        }
    }

    n->setup_after_read(ctx.tick(), ctx.paste_mode());
}

bool TRead::read_properties(Note *n, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (tag == "pitch") {
        n->set_pitch(clamp_pitch(e.read_int()), false);
    } else if (TRead::read_property(n, tag, e, ctx, Pid::CENT_OFFSET)) {
    } else if (tag == "tpc") {
        int tpc = e.read_int();
        n->set_tpc1(tpc);
        n->set_tpc2(tpc);
    } else if (tag == "track") { // for performance
        n->set_track(e.read_int());
    } else if (tag == "Accidental") {
        Accidental *a = Factory::create_accidental(n);
        a->set_track(n->track());
        TRead::read(a, e, ctx);
        n->add(a);
    } else if (tag == "tpc2") {
        n->set_tpc2(e.read_int());
    } else if (tag == "small") {
        n->set_small(e.read_int());
    } else if (tag == "mirror") {
        TRead::read_property(n, e, ctx, Pid::MIRROR_HEAD);
    } else if (tag == "dotPosition") {
        TRead::read_property(n, e, ctx, Pid::DOT_POSITION);
    } else if (tag == "fixed") {
        n->set_fixed(e.read_bool());
    } else if (tag == "fixedLine") {
        n->set_fixed_line(e.read_int());
    } else if (tag == "headScheme") {
        TRead::read_property(n, e, ctx, Pid::HEAD_SCHEME);
    } else if (tag == "head") {
        TRead::read_property(n, e, ctx, Pid::HEAD_GROUP);
    } else if (tag == "velocity") {
        n->set_user_velocity(e.read_int());
    } else if (tag == "play") {
        n->set_play(e.read_int());
    } else if (tag == "tuning") {
        n->set_tuning(e.read_double());
    } else if (tag == "fret") {
        n->set_fret(e.read_int());
    } else if (tag == "string") {
        n->set_string(e.read_int());
    } else if (tag == "ghost") {
        n->set_ghost(e.read_int());
    } else if (tag == "dead") {
        n->set_dead_note(e.read_int());
    } else if (tag == "headType") {
        TRead::read_property(n, e, ctx, Pid::HEAD_TYPE);
    } else if (tag == "veloType") {
        TRead::read_property(n, e, ctx, Pid::VELO_TYPE);
    } else if (tag == "line") {
        n->set_line(e.read_int());
    } else if (tag == "Fingering") {
        Fingering *f = Factory::create_fingering(n);
        f->set_track(n->track());
        TRead::read(f, e, ctx);
        n->add(f);
    } else if (tag == "Text") {
        Text *t = Factory::create_text(n);
        t->set_track(n->track());
        TRead::read(t, e, ctx);
        n->add(t);
    } else if (tag == "Symbol") {
        Symbol *s = new Symbol(n);
        s->set_track(n->track());
        TRead::read(s, e, ctx);
        n->add(s);
    } else if (tag == "Image") {
        if (MScore::no_images) {
            e.skip_current_element();
        } else {
            Image *image = new Image(n);
            image->set_track(n->track());
            TRead::read(image, e, ctx);
            n->add(image);
        }
    } else if (tag == "Bend") {
        Bend *b = Factory::create_bend(n);
        b->set_track(n->track());
        TRead::read(b, e, ctx);
        n->add(b);
    } else if (tag == "NoteDot") {
        NoteDot *dot = Factory::create_note_dot(n);
        TRead::read(dot, e, ctx);
        n->add(dot);
    } else if (tag == "Events") {
        NoteEventList play_events;
        while (e.read_next_start_element()) {
            const AsciiStringView t(e.name());
            if (t == "Event") {
                NoteEvent ne;
                TRead::read(&ne, e, ctx);
                play_events.push_back(ne);
            } else {
                e.unknown();
            }
        }
        n->set_play_events(play_events);
        if (n->chord()) {
            n->chord()->set_play_event_type(PlayEventType::USER);
        }
    } else if (tag == "offset") {
        read_item_properties(n, e, ctx);
    } else if (tag == "ChordLine" && n->chord()) {
        ChordLine *cl = Factory::create_chord_line(n->chord());
        TRead::read(cl, e, ctx);
        cl->set_note(n);
        n->chord()->add(cl);
    } else if (tag == "LaissezVib") {
        LaissezVib *lv = Factory::create_laissez_vib(n);
        TRead::read(lv, e, ctx);
        n->add(lv);
    } else if (tag == "PartialTie") {
        PartialTie *pt = Factory::create_partial_tie(n);
        TRead::read(pt, e, ctx);
        if (pt->is_outgoing()) {
            pt->set_start_note(n);
        } else {
            pt->set_end_note(n);
        }
        n->add(pt);
    } else if (Spanner *spanner = try_create_spanner(tag, ctx, /*requireNoteAnchor*/ true)) {
        TRead::read_item(spanner, e, ctx);
    } else if (tag == "overrideBendVisibilityRules") {
        n->set_override_bend_visibility_rules(e.read_bool());
    } else if (TRead::read_property(n, tag, e, ctx, Pid::HIDE_GENERATED_PARENTHESES)) {
    } else if (read_item_properties(n, e, ctx)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(NoteEvent *item, XmlReader &e, ReadContext &) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "pitch") {
            item->set_pitch(e.read_int());
        } else if (tag == "ontime") {
            item->set_ontime(e.read_int());
        } else if (tag == "len") {
            item->set_len(e.read_int());
        } else {
            e.unknown();
        }
    }
}

void TRead::read(NoteDot *d, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_item_properties(d, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(NoteHead *h, XmlReader &xml, ReadContext &ctx) { read(static_cast<Symbol *>(h), xml, ctx); }

void TRead::read(NoteLine *nl, XmlReader &xml, ReadContext &ctx) { TRead::read(to_text_line_base(nl), xml, ctx); }

void TRead::read(Ottava *o, XmlReader &e, ReadContext &ctx) {
    o->erase_spanner_segments();
    while (e.read_next_start_element()) {
        read_properties(o, e, ctx);
    }
    if (ctx.msc_version() < 470) {
        compat::CompatUtils::set_text_line_text_position_from_align(o);
    }
    if (o->ottava_type() != OttavaType::OTTAVA_8VA || o->numbers_only() != o->property_default(Pid::NUMBERS_ONLY).to_bool()) {
        o->style_changed();
    }
}

bool TRead::read_properties(Ottava *o, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "subtype") {
        String s = e.read_text();
        bool ok;
        int idx = s.to_int(&ok);
        if (!ok) {
            o->set_ottava_type(OttavaType::OTTAVA_8VA);
            for (OttavaDefault d : ::mu::engraving::ottava_default) {
                if (s == d.name) {
                    o->set_ottava_type(d.type);
                    break;
                }
            }
        } else {
            o->set_ottava_type(OttavaType(idx));
        }
    } else if (!read_properties(to_text_line_base(o), e, ctx)) {
        e.unknown();
        return false;
    }
    return true;
}

void TRead::read(Page *p, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (e.name() == "System") {
            System *system = Factory::create_system(p->score());
            p->score()->systems().push_back(system);
            read(system, e, ctx);
        } else {
            e.unknown();
        }
    }
}

void TRead::read(PalmMute *p, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(to_text_line_base(p), e, ctx)) {
            e.unknown();
        }
    }
    if (ctx.msc_version() < 470) {
        compat::CompatUtils::set_text_line_text_position_from_align(p);
    }
}

void TRead::read(Parenthesis *p, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());
        if (TRead::read_property(p, tag, xml, ctx, Pid::HORIZONTAL_DIRECTION)) {
        } else if (!read_item_properties(p, xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(Part *p, XmlReader &e, ReadContext &ctx) {
    p->set_id(e.int_attribute("id", 0));

    while (e.read_next_start_element()) {
        if (!read_properties(p, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(SharedPart *p, XmlReader &e, ReadContext &ctx) {
    p->set_id(e.int_attribute("id", 0));

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "sharedPartEnabled") {
            p->set_property(Pid::SHARED_PART_ENABLED, e.read_bool());
        } else if (!read_properties(p, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(PartialLyricsLine *p, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());
        if (tag == "isEndMelisma") {
            p->set_is_end_melisma(xml.read_bool());
        } else if (TRead::read_property(p, tag, xml, ctx, Pid::VERSE)) {
        } else if (TRead::read_property(p, tag, xml, ctx, Pid::CENTER_BETWEEN_STAVES)) {
        } else if (!read_properties(static_cast<SLine *>(p), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(PartialTie *p, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());
        if (TRead::read_property(p, tag, xml, ctx, Pid::TIE_PLACEMENT)) {
        } else if (TRead::read_property(p, tag, xml, ctx, Pid::PARTIAL_SPANNER_DIRECTION)) {
        } else if (!read_properties(to_slur_tie(p), xml, ctx)) {
            xml.unknown();
        }
    }
}

bool TRead::read_properties(Part *p, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "id") {
        p->set_id(e.read_int());
    } else if (tag == "eid") {
        read_item_eid(p, e, ctx);
    } else if (tag == "sharedPart") {
        AsciiStringView s = e.read_ascii_text();
        EID eid = EID::from_std_string(s);
        DO_ASSERT(eid.is_valid());
        EIDRegister *eid_register = ctx.score()->master_score()->eid_register();
        EngravingObject *obj = eid_register->item_from_eid(eid);
        DO_ASSERT(obj && obj->is_shared_part());
        SharedPart *shared_part = to_shared_part(obj);
        shared_part->add_origin_part(p);
    } else if (tag == "Staff") {
        Staff *staff = Factory::create_staff(p);
        p->score()->append_staff(staff);
        TRead::read(staff, e, ctx);
    } else if (tag == "Instrument") {
        Instrument *instr = new Instrument;
        read(instr, e, ctx, p);
        p->set_instrument(instr, Fraction(-1, 1));
    } else if (tag == "name") {
        p->instrument()->set_long_name(e.read_text());
    } else if (tag == "color") {
        e.skip_current_element(); // Unused
    } else if (tag == "shortName") {
        p->instrument()->set_short_name(e.read_text());
    } else if (tag == "show") {
        p->set_show(e.read_int());
    } else if (tag == "soloist") {
        p->set_soloist(e.read_int());
    } else if (tag == "hideWhenEmpty") {
        p->set_hide_when_empty(TConv::from_xml(e.read_ascii_text(), AutoOnOff::AUTO));
    } else if (tag == "hideStavesWhenIndividuallyEmpty") {
        p->set_hide_staves_when_individually_empty(e.read_bool());
    } else if (tag == "preferSharpFlat") {
        String val = e.read_text();
        if (val == "sharps") {
            p->set_prefer_sharp_flat(PreferSharpFlat::SHARPS);
        } else if (val == "flats") {
            p->set_prefer_sharp_flat(PreferSharpFlat::FLATS);
        } else if (val == "none") {
            p->set_prefer_sharp_flat(PreferSharpFlat::NONE);
        } else {
            p->set_prefer_sharp_flat(PreferSharpFlat::AUTO);
        }
    } else {
        return false;
    }
    return true;
}

void TRead::read(Pedal *p, XmlReader &e, ReadContext &ctx) {
    bool begin_text_tag = false;
    bool continue_text_tag = false;
    bool end_text_tag = false;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        begin_text_tag = begin_text_tag || tag == "beginText";
        continue_text_tag = continue_text_tag || tag == "continueText";
        end_text_tag = end_text_tag || tag == "endText";

        if (TRead::read_property(p, tag, e, ctx, Pid::LINE_VISIBLE)) {
            p->reset_property(Pid::END_TEXT);
        } else if (TRead::read_property(p, tag, e, ctx, Pid::BEGIN_HOOK_TYPE)) {
            if (p->begin_hook_type() != HookType::NONE) {
                p->set_begin_text(String());
                p->set_continue_text(String());
            }
        } else if (!read_properties(to_text_line_base(p), e, ctx)) {
            e.unknown();
        }
    }
    if (ctx.msc_version() < 470) {
        compat::CompatUtils::set_text_line_text_position_from_align(p);

        if (p->end_text() == Pedal::star_symbol) {
            p->set_end_hook_type(HookType::ROSETTE);
        }
    }
}

void TRead::read(Rasgueado *r, XmlReader &xml, ReadContext &ctx) { TRead::read(to_text_line_base(r), xml, ctx); }

void TRead::read(Rest *r, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "NoteDot") {
            NoteDot *dot = Factory::create_note_dot(r);
            TRead::read(dot, e, ctx);
            r->add(dot);
        } else if (TRead::read_styled_property(r, tag, e, ctx)) {
        } else if (TRead::read_property(r, tag, e, ctx, Pid::ALIGN_WITH_OTHER_RESTS)) {
        } else if (TRead::read_properties(r, e, ctx)) {
        } else {
            e.unknown();
        }
    }
}

void TRead::read(Segment *s, XmlReader &e, ReadContext &ctx) {
    UNUSED(ctx);
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "subtype") {
            e.skip_current_element();
        } else if (tag == "leadingSpace") {
            s->set_extra_leading_space(Spatium(e.read_double()));
        } else {
            e.unknown();
        }
    }
}

void TRead::read(SLine *l, XmlReader &e, ReadContext &ctx) {
    l->erase_spanner_segments();

    while (e.read_next_start_element()) {
        if (!read_properties(l, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(SLine *l, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (tag == "Segment") {
        LineSegment *ls = l->create_line_segment();
        ls->set_track(l->track()); // needed in read to get the right staff mag
        l->add(ls);
        TRead::read(ls, e, ctx);
        ls->set_visible(l->visible());
    } else if (TRead::read_styled_property(l, tag, e, ctx)) {
    } else if (TRead::read_property(l, tag, e, ctx, Pid::DIAGONAL)) {
    } else if (TRead::read_property(l, tag, e, ctx, Pid::LINE_WIDTH)) {
    } else if (TRead::read_property(l, tag, e, ctx, Pid::LINE_STYLE)) {
    } else if (TRead::read_property(l, tag, e, ctx, Pid::DASH_LINE_LEN)) {
    } else if (TRead::read_property(l, tag, e, ctx, Pid::DASH_GAP_LEN)) {
    } else if (tag == "lineColor" || tag == "color") {
        l->set_line_color(e.read_color());
    } else if (!read_properties(to_spanner(l), e, ctx)) {
        return false;
    }
    return true;
}

void TRead::read(Slur *s, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(s, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(Slur *s, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (TRead::read_property(s, tag, e, ctx, Pid::PARTIAL_SPANNER_DIRECTION)) {
        return true;
    }

    return read_properties(to_slur_tie(s), e, ctx);
}

void TRead::read(SlurTie *s, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(s, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(SlurTie *s, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (TRead::read_property(s, tag, e, ctx, Pid::SLUR_DIRECTION)) {
    } else if (TRead::read_property(s, tag, e, ctx, Pid::MASK_SLURTIE)) {
    } else if (tag == "lineType") {
        s->set_style_type(static_cast<SlurStyleType>(e.read_int()));
    } else if (tag == "SlurSegment" || tag == "TieSegment" || tag == "LaissezVibSegment" || tag == "PartialTieSegment" ||
               tag == "HammerOnPullOffSegment" || tag == "TappingHalfSlurSegment") {
        const int idx = e.int_attribute("no", 0);
        const int n = int(s->spanner_segments().size());
        for (int i = n; i < idx; ++i) {
            s->add(s->new_slur_tie_segment());
        }
        SlurTieSegment *sts = s->new_slur_tie_segment();
        s->add(sts);
        TRead::read(sts, e, ctx);
    } else if (!read_properties(to_spanner(s), e, ctx)) {
        return false;
    }
    return true;
}

void TRead::read(SlurTieSegment *s, XmlReader &e, ReadContext &ctx) {
    double spatium_value = s->style().spatium();
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "o1") {
            s->ups(Grip::START).off = e.read_point() * spatium_value;
        } else if (tag == "o2") {
            s->ups(Grip::BEZIER1).off = e.read_point() * spatium_value;
        } else if (tag == "o3") {
            s->ups(Grip::BEZIER2).off = e.read_point() * spatium_value;
        } else if (tag == "o4") {
            s->ups(Grip::END).off = e.read_point() * spatium_value;
        } else if (tag == "HammerOnPullOffText") {
            DO_ASSERT(s->is_hammer_on_pull_off_segment());
            read_hopo_text(to_hammer_on_pull_off_segment(s), e, ctx, e.int_attribute("idx"));
        } else if (!read_item_properties(s, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read_hopo_text(HammerOnPullOffSegment *hopo_seg, XmlReader &xml, ReadContext &ctx, int idx) {
    int hopo_text_count = static_cast<int>(hopo_seg->hopo_text().size());
    for (int i = hopo_text_count; i < idx; ++i) {
        hopo_seg->add_hopo_text(new HammerOnPullOffText(hopo_seg));
    }

    HammerOnPullOffText *hopo_text = new HammerOnPullOffText(hopo_seg);
    while (xml.read_next_start_element()) {
        if (!read_properties(to_text_base(hopo_text), xml, ctx)) {
            xml.unknown();
        }
    }

    hopo_seg->add_hopo_text(hopo_text);
}

String TRead::line_break_from_tag(const String &str) {
    // Raw newlines appearing next to tags (<font size="10> or <sym>...) get eaten by XML readers.
    String s = str;
    return s.replace(u"<br/>", u"\n");
}

void TRead::read_note_paren_group(Chord *ch, XmlReader &e, ReadContext &ctx) {
    Parenthesis *left_paren = nullptr;
    Parenthesis *right_paren = nullptr;
    std::vector<Note *> notes;
    while (e.read_next_start_element()) {
        const AsciiStringView t(e.name());

        if (t == "Parenthesis") {
            Parenthesis *paren = Factory::create_parenthesis(ch);
            TRead::read(paren, e, ctx);
            paren->set_track(ctx.track());

            if (paren->direction() == DirectionH::LEFT) {
                left_paren = paren;
            } else {
                right_paren = paren;
            }
        } else if (t == "Notes") {
            while (e.read_next_start_element()) {
                const AsciiStringView note_tag(e.name());
                if (note_tag == "NoteEID") {
                    AsciiStringView s = e.read_ascii_text();
                    EID eid = EID::from_std_string(s);
                    IF_ASSERT_FAILED(eid.is_valid()) { continue; }
                    if (ctx.paste_mode()) {
                        eid = ctx.resolve_pasted_eid(eid);
                    }
                    EIDRegister *eid_register = ctx.score()->master_score()->eid_register();
                    EngravingObject *obj = eid_register->item_from_eid(eid);
                    IF_ASSERT_FAILED(obj && obj->is_note()) { continue; }
                    notes.push_back(to_note(obj));
                } else {
                    e.unknown();
                }
            }
        } else {
            e.unknown();
        }
    }

    if (!left_paren) {
        left_paren = Factory::create_parenthesis(ch);
        left_paren->set_track(ctx.track());
    }

    if (!right_paren) {
        right_paren = Factory::create_parenthesis(ch);
        right_paren->set_direction(DirectionH::RIGHT);
        right_paren->set_track(ctx.track());
    }

    NoteParenthesisInfo *note_paren_info = new NoteParenthesisInfo(left_paren, right_paren, notes);
    ch->add_note_parenthesis_info(note_paren_info);
}

bool TRead::read_properties(Spanner *s, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "play") {
        s->set_play_spanner(e.read_bool());
        return true;
    } else if (tag == "track2") {
        track_idx_t track2 = e.read_int();
        s->set_track2(track2 + ctx.track_offset());
    } else if (tag == "startTick") {
        Fraction start_tick = e.read_fraction() + ctx.tick_offset();
        s->set_tick(start_tick);
    } else if (tag == "ticks") {
        Fraction ticks = e.read_fraction();
        s->set_ticks(ticks);
    } else if (tag == "startElement") {
        EIDRegister *eid_register = ctx.score()->master_score()->eid_register();
        EID start_el_eid = EID::from_std_string(e.read_ascii_text());
        IF_ASSERT_FAILED(start_el_eid.is_valid()) { return false; }
        if (ctx.paste_mode()) {
            start_el_eid = ctx.resolve_pasted_eid(start_el_eid);
        }
        if (s->anchor() == Spanner::Anchor::NOTE && !(s->is_partial_tie() || s->is_laissez_vib())) {
            // Store spanner and EIDs to be connected at the end of file reading
            ctx.add_note_anchored_spanner_start_el(s, start_el_eid);
            return true;
        }

        EngravingItem *start_el = to_engraving_item(eid_register->item_from_eid(start_el_eid));
        IF_ASSERT_FAILED(start_el) { return false; }
        s->set_start_element(start_el);
        s->set_tick(start_el->tick());
        s->set_track(start_el->track());
    } else if (tag == "endElement") {
        EIDRegister *eid_register = ctx.score()->master_score()->eid_register();
        EID end_el_eid = EID::from_std_string(e.read_ascii_text());
        IF_ASSERT_FAILED(end_el_eid.is_valid()) { return false; }
        if (ctx.paste_mode()) {
            end_el_eid = ctx.resolve_pasted_eid(end_el_eid);
        }
        if (s->anchor() == Spanner::Anchor::NOTE && !(s->is_partial_tie() || s->is_laissez_vib())) {
            // Store spanner and EIDs to be connected at the end of file reading
            ctx.add_note_anchored_spanner_end_el(s, end_el_eid);
            return true;
        }

        EngravingItem *end_el = to_engraving_item(eid_register->item_from_eid(end_el_eid));
        IF_ASSERT_FAILED(end_el) { return false; }
        s->set_end_element(end_el);
        if (s->anchor() == Spanner::Anchor::MEASURE) {
            s->set_tick2(s->end_measure()->end_tick());
        } else {
            s->set_tick2(end_el->tick());
            s->set_track2(end_el->track());
        }
    } else if (!read_item_properties(s, e, ctx)) {
        return false;
    }
    return true;
}

void TRead::read(Spacer *s, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            s->set_spacer_type(SpacerType(e.read_int()));
        } else if (tag == "space") {
            s->set_gap(Spatium(e.read_double()));
        } else if (!read_item_properties(s, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(StaffType *t, XmlReader &e, ReadContext &ctx) {
    t->set_score(ctx.score());
    t->set_group(TConv::from_xml(e.ascii_attribute("group"), StaffGroup::STANDARD));

    if (t->group() == StaffGroup::TAB) {
        t->set_gen_keysig(false);
    }

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "name") {
            t->set_type(TConv::from_xml(e.read_ascii_text(), StaffTypes::STANDARD));
        } else if (tag == "StaffLabel") {
            StaffLabel &staff_label = t->staff_label();
            read_staff_label(staff_label, e);
        } else if (tag == "lines") {
            t->set_lines(e.read_int());
        } else if (tag == "lineDistance") {
            t->set_line_distance(Spatium(e.read_double()));
        } else if (tag == "yoffset") {
            t->set_yoffset(Spatium(e.read_double()));
        } else if (tag == "mag") {
            t->set_user_mag(e.read_double());
        } else if (tag == "small") {
            t->set_small(e.read_bool());
        } else if (tag == "stepOffset") {
            t->set_step_offset(e.read_int());
        } else if (tag == "clef") {
            t->set_gen_clef(e.read_int());
        } else if (tag == "stemless") {
            bool val = e.read_int() != 0;
            t->set_stemless(val);
            t->set_show_back_tied(!val); // for compatibility with 2.0.2 scores where this prop
        } // was lacking and controlled by "slashStyle" instead
        else if (tag == "barlines") {
            t->set_show_barlines(e.read_int());
        } else if (tag == "timesig") {
            t->set_gen_timesig(e.read_int());
        } else if (tag == "noteheadScheme") {
            t->set_note_head_scheme(TConv::from_xml(e.read_ascii_text(), NoteHeadScheme::HEAD_NORMAL));
        } else if (tag == "instrumentLabelVisibility") {
            t->set_instrument_label_visibility(TConv::from_xml(e.read_ascii_text(), InstrumentLabelVisibility::AUTO));
        } else if (tag == "keysig") {
            t->set_gen_keysig(e.read_int());
        } else if (tag == "ledgerlines") {
            t->set_show_ledger_lines(e.read_int());
        } else if (tag == "invisible") {
            t->set_invisible(e.read_int());
        } else if (tag == "color") {
            t->set_color(e.read_color());
        } else if (tag == "durations") {
            t->set_gen_durations(e.read_bool());
        } else if (tag == "durationFontName") {
            t->set_duration_font_name(e.read_text());
        } else if (tag == "durationFontSize") {
            t->set_duration_font_size(e.read_double());
        } else if (tag == "durationFontY") {
            t->set_duration_font_user_y(e.read_double());
        } else if (tag == "fretPresetIdx") {
            t->set_fret_preset_idx(e.read_int());
        } else if (tag == "fretFontSize") {
            t->set_fret_font_size(e.read_double());
        } else if (tag == "fretFontY") {
            t->set_fret_font_user_y(e.read_double());
        } else if (tag == "fretUseTextStyle") {
            t->set_fret_use_text_style(e.read_bool());
        } else if (tag == "fretTextStyle") {
            t->set_fret_text_style(TextStyleType(TConv::from_xml(e.read_ascii_text(), TextStyleType::TAB_FRET_NUMBER)));
        } else if (tag == "symbolRepeat") {
            t->set_symbol_repeat((TablatureSymbolRepeat) e.read_int());
        } else if (tag == "linesThrough") {
            t->set_lines_through(e.read_bool());
        } else if (tag == "minimStyle") {
            t->set_minim_style((TablatureMinimStyle) e.read_int());
        } else if (tag == "onLines") {
            t->set_on_lines(e.read_bool());
        } else if (tag == "showRests") {
            t->set_show_rests(e.read_bool());
        } else if (tag == "stemsDown") {
            t->set_stems_down(e.read_bool());
        } else if (tag == "stemsThrough") {
            t->set_stems_through(e.read_bool());
        } else if (tag == "upsideDown") {
            t->set_upside_down(e.read_bool());
        } else if (tag == "showTabFingering") {
            t->set_show_tab_fingering(e.read_bool());
        } else if (tag == "useNumbers") {
            t->set_use_numbers(e.read_bool());
        } else if (tag == "showBackTied") {       // must be after reading "slashStyle"/"stemless" prop, as in older
            t->set_show_back_tied(e.read_bool()); // scores, this prop was lacking and controlled by "slashStyle"
        } else {
            e.unknown();
        }
    }
}

void TRead::read(StaffTypeChange *c, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "StaffType") {
            StaffType *st = new StaffType();
            TRead::read(st, e, ctx);
            // Measure::add() will replace this with a pointer to a copy in the staff
            c->set_staff_type(st, true);
        } else if (!read_item_properties(c, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Staff *s, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_properties(s, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(Staff *s, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    if (tag == "StaffType") {
        StaffType st;
        TRead::read(&st, e, ctx);
        s->set_staff_type(Fraction(0, 1), st);
    } else if (tag == "defaultClef") { // sets both default transposing and concert clef
        ClefType ct = TConv::from_xml(e.read_ascii_text(), ClefType::G);
        s->set_default_clef_type(ClefTypeList(ct, ct));
    } else if (tag == "defaultConcertClef") {
        s->set_default_clef_type(ClefTypeList(TConv::from_xml(e.read_ascii_text(), ClefType::G), s->default_clef_type().transposing_clef));
    } else if (tag == "defaultTransposingClef") {
        s->set_default_clef_type(ClefTypeList(s->default_clef_type().concert_clef, TConv::from_xml(e.read_ascii_text(), ClefType::G)));
    } else if (tag == "invisible") {
        s->staff_type(Fraction(0, 1))->set_invisible(e.read_int()); // same as: setInvisible(Fraction(0,1)), e.readInt())
    } else if (tag == "hideWhenEmpty") {
        s->set_hide_when_empty(TConv::from_xml(e.read_ascii_text(), AutoOnOff::AUTO));
    } else if (tag == "cutaway") {
        s->set_cutaway(e.read_int());
    } else if (tag == "showIfSystemEmpty") {
        s->set_show_if_entire_system_empty(e.read_int());
    } else if (tag == "hideSystemBarLine") {
        s->set_hide_system_bar_line(e.read_int());
    } else if (tag == "mergeMatchingRests") {
        s->set_merge_matching_rests(TConv::from_xml(e.read_ascii_text(), AutoOnOff::AUTO));
    } else if (tag == "isStaffVisible") {
        s->set_visible(e.read_bool());
    } else if (tag == "keylist") {
        TRead::read(s->key_list(), e, ctx);
    } else if (tag == "bracket") { // LEGACY
        Color color = Color::from_string(e.attribute("color"));
        int col = e.int_attribute("col", -1);
        if (col == -1) {
            col = static_cast<int>(ctx.score()->brackets(s->idx()).size());
        }
        EditStaffBrackets::set_bracket_type(ctx.score(), s->idx(), col, BracketType(e.int_attribute("type", -1)));
        EditStaffBrackets::set_bracket_span(ctx.score(), s->idx(), col, e.int_attribute("span", 0));
        EditStaffBrackets::set_bracket_visible(ctx.score(), s->idx(), col, static_cast<bool>(e.int_attribute("visible", 1)));
        BracketItem *bi = ctx.score()->brackets(s->idx()).at(col);
        if (color.is_valid()) {
            bi->set_color(color);
        }
        e.read_next();
    } else if (tag == "BracketItem") {
        BracketItem *b = Factory::create_bracket_item(s);
        b->set_start_staff_idx(s->idx());
        read(b, e, ctx);
        EditStaffBrackets::insert_bracket(ctx.score(), s->idx(), b);
    } else if (tag == "barLineSpan") {
        const int bar_line_span = e.read_int();
        if (bar_line_span < 0) {
            LOGW() << "barLineSpan is negative: " << bar_line_span;
            s->set_bar_line_span(false);
        } else if (bar_line_span > 1) {
            LOGW() << "barLineSpan is > 1: " << bar_line_span;
            s->set_bar_line_span(true);
        } else {
            s->set_bar_line_span(static_cast<bool>(bar_line_span));
        }
    } else if (tag == "barLineSpanFrom") {
        s->set_bar_line_from(e.read_int());
    } else if (tag == "barLineSpanTo") {
        s->set_bar_line_to(e.read_int());
    } else if (tag == "distOffset") {
        s->set_user_dist(Spatium(e.read_double()));
    } else if (tag == "mag") {
        /*_userMag =*/
        e.read_double(0.1, 10.0);
    } else if (tag == "eid") {
        read_item_eid(s, e, ctx);
    } else if (tag == "linkedTo") {
        read_item_link(s, e, ctx);
    } else if (tag == "color") {
        s->staff_type(Fraction(0, 1))->set_color(e.read_color());
    } else if (tag == "transposeDiatonic") {
        ctx.set_transpose_diatonic(static_cast<int8_t>(e.read_int()));
    } else if (tag == "transposeChromatic") {
        ctx.set_transpose_chromatic(static_cast<int8_t>(e.read_int()));
    } else if (tag == "playbackVoice1") {
        s->set_playback_voice(0, e.read_int());
    } else if (tag == "playbackVoice2") {
        s->set_playback_voice(1, e.read_int());
    } else if (tag == "playbackVoice3") {
        s->set_playback_voice(2, e.read_int());
    } else if (tag == "playbackVoice4") {
        s->set_playback_voice(3, e.read_int());
    } else if (TRead::read_property(s, tag, e, ctx, Pid::SHOW_MEASURE_NUMBERS)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(BracketItem *b, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView &tag = xml.name();
        if (tag == "type") {
            b->set_bracket_type(TConv::from_xml(xml.read_ascii_text(), BracketType::NORMAL));
        } else if (read_property(b, tag, xml, ctx, Pid::BRACKET_SPAN)) {
        } else if (tag == "level") {
            b->set_column(xml.read_int());
        } else if (read_property(b, tag, xml, ctx, Pid::VISIBLE)) {
        } else if (read_property(b, tag, xml, ctx, Pid::GROUP_BRACKET_SHOW_TEXT)) {
        } else if (read_property(b, tag, xml, ctx, Pid::GROUP_BRACKET_SHOW_BRACKET)) {
        } else if (tag == "StaffLabel") {
            StaffLabel &label = b->label();
            read_staff_label(label, xml);
        } else {
            xml.unknown();
        }
    }
}

String TRead::read_legacy_staff_name(XmlReader &xml) {
    String name = line_break_from_tag(xml.read_xml());
    if (name.starts_with(u"<html>")) {
        // compatibility to old html implementation:
        name = HtmlParser::parse(name);
    }
    return name;
}

void TRead::read_staff_label(StaffLabel &item, XmlReader &xml) {
    while (xml.read_next_start_element()) {
        if (!read_properties(item, xml)) {
            xml.unknown();
        }
    }
}

bool TRead::read_properties(StaffLabel &item, XmlReader &xml) {
    AsciiStringView tag = xml.name();

    if (tag == "longName") {
        item.set_long_name(line_break_from_tag(xml.read_xml()));
    } else if (tag == "shortName") {
        item.set_short_name(line_break_from_tag(xml.read_xml()));
    } else {
        return false;
    }

    return true;
}

void TRead::read_instrument_label(InstrumentLabel &item, XmlReader &xml) {
    while (xml.read_next_start_element()) {
        if (!read_properties(item, xml)) {
            xml.unknown();
        }
    }
}

bool TRead::read_properties(InstrumentLabel &item, XmlReader &xml) {
    AsciiStringView tag = xml.name();

    if (read_properties(static_cast<StaffLabel &>(item), xml)) {
    } else if (tag == "transposition") {
        item.set_transposition(line_break_from_tag(xml.read_xml()));
    } else if (tag == "showTranspositionLong") {
        item.set_show_transposition_long(xml.read_bool());
    } else if (tag == "showTranspositionShort") {
        item.set_show_transposition_short(xml.read_bool());
    } else if (tag == "number") {
        item.set_number(xml.read_int());
    } else if (tag == "showNumberLong") {
        item.set_show_number_long(xml.read_bool());
    } else if (tag == "showNumberShort") {
        item.set_show_number_short(xml.read_bool());
    } else if (tag == "useCustomName") {
        item.set_use_custom_name(xml.read_bool());
    } else if (tag == "customNameLong") {
        item.set_custom_name_long(line_break_from_tag(xml.read_xml()));
    } else if (tag == "customNameShort") {
        item.set_custom_name_short(line_break_from_tag(xml.read_xml()));
    } else if (tag == "allowGroupName") {
        item.set_allow_group_name(xml.read_bool());
    } else if (tag == "customNameLongGroup") {
        item.set_custom_name_long_group(line_break_from_tag(xml.read_xml()));
    } else if (tag == "customNameShortGroup") {
        item.set_custom_name_short_group(line_break_from_tag(xml.read_xml()));
    } else if (tag == "useCustomGroupName") {
        item.set_use_custom_group_name(xml.read_bool());
    } else if (tag == "customNameLongIndividual") {
        item.set_custom_name_long_individual(line_break_from_tag(xml.read_xml()));
    } else if (tag == "customNameShortIndividual") {
        item.set_custom_name_short_individual(line_break_from_tag(xml.read_xml()));
    } else if (tag == "useCustomIndividualName") {
        item.set_use_custom_individual_name(xml.read_bool());
    } else {
        return false;
    }

    return true;
}

void TRead::read(Stem *s, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!TRead::read_properties(s, e, ctx)) {
            e.unknown();
        }
    }
}

bool TRead::read_properties(Stem *s, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());

    if (TRead::read_property(s, tag, e, ctx, Pid::USER_LEN)) {
    } else if (TRead::read_styled_property(s, tag, e, ctx)) {
    } else if (read_item_properties(s, e, ctx)) {
    } else {
        return false;
    }
    return true;
}

void TRead::read(StemSlash *s, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (!read_item_properties(s, e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(StringData *item, XmlReader &e) {
    item->string_list().clear();
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "frets") {
            item->set_frets(e.read_int());
        } else if (tag == "string") {
            InstrString strg;
            strg.open = e.int_attribute("open", 0);
            strg.use_flat = e.int_attribute("useFlat", 0);
            strg.pitch = e.read_int();
            item->string_list().push_back(strg);
        } else {
            e.unknown();
        }
    }
    if (item->is_five_string_banjo()) {
        item->config_banjo5th_string();
    }
}

void TRead::read(StringTunings *s, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        const AsciiStringView tag(xml.name());

        if (tag == "preset") {
            s->set_preset(xml.read_text());
        } else if (tag == "StringData") {
            StringData sd;
            read(&sd, xml);
            s->set_string_data(sd);
        } else if (tag == "visibleStrings") {
            s->set_visible_strings(TConv::from_xml(xml.read_text(), std::vector<string_idx_t>()));
        } else if (!read_properties(static_cast<StaffTextBase *>(s), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(System *s, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "SystemDivider") {
            SystemDivider *sd = new SystemDivider(s);
            TRead::read(sd, e, ctx);
            s->add(sd);
        } else {
            e.unknown();
        }
    }
}

void TRead::read(Text *t, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "style") {
            TextStyleType s = TConv::from_xml(e.read_ascii_text(), TextStyleType::DEFAULT);
            if (TextStyleType::TUPLET == s) { // ugly hack for compatibility
                continue;
            }
            t->init_text_style_type(s);
        } else if (!read_properties(to_text_base(t), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(TextLine *l, XmlReader &e, ReadContext &ctx) {
    bool system = e.int_attribute("system", 0) == 1;
    l->set_system_flag(system);
    l->init_style();
    TRead::read(to_text_line_base(l), e, ctx);
}

void TRead::read(TextLineBase *b, XmlReader &e, ReadContext &ctx) {
    b->erase_spanner_segments();

    while (e.read_next_start_element()) {
        if (!read_properties(b, e, ctx)) {
            e.unknown();
        }
    }
    if (ctx.msc_version() < 470) {
        compat::CompatUtils::set_text_line_text_position_from_align(b);
    }
}

void TRead::read(Tie *t, XmlReader &xml, ReadContext &ctx) {
    while (xml.read_next_start_element()) {
        AsciiStringView tag = xml.name();
        if (TRead::read_property(t, tag, xml, ctx, Pid::TIE_PLACEMENT)) {
        } else if (!read_properties(to_slur_tie(t), xml, ctx)) {
            xml.unknown();
        }
    }
}

void TRead::read(TimeSig *s, XmlReader &e, ReadContext &ctx) {
    int n = 0, z1 = 0, z2 = 0, z3 = 0, z4 = 0;
    bool old = false;

    TimeSigType time_sig_type = TimeSigType::NORMAL;
    Fraction sig;
    Fraction stretch(1, 1);
    String numerator_string;
    String denominator_string;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "den") {
            old = true;
            n = e.read_int();
        } else if (tag == "nom1") {
            old = true;
            z1 = e.read_int();
        } else if (tag == "nom2") {
            old = true;
            z2 = e.read_int();
        } else if (tag == "nom3") {
            old = true;
            z3 = e.read_int();
        } else if (tag == "nom4") {
            old = true;
            z4 = e.read_int();
        } else if (tag == "subtype") {
            time_sig_type = TimeSigType(e.read_int());
        } else if (tag == "showCourtesySig") {
            s->set_show_courtesy_sig(e.read_int());
        } else if (tag == "sigN") {
            sig.set_numerator(e.read_int());
        } else if (tag == "sigD") {
            sig.set_denominator(e.read_int());
        } else if (tag == "stretchN") {
            stretch.set_numerator(e.read_int());
        } else if (tag == "stretchD") {
            stretch.set_denominator(e.read_int());
        } else if (tag == "textN") {
            numerator_string = e.read_text();
        } else if (tag == "textD") {
            denominator_string = e.read_text();
        } else if (tag == "Groups") {
            Groups groups;
            TRead::read(&groups, e, ctx);
            s->set_groups(groups);
        } else if (tag == "isCourtesy") {
            s->set_is_courtesy(e.read_bool());
        } else if (TRead::read_styled_property(s, tag, e, ctx)) {
        } else if (!read_item_properties(s, e, ctx)) {
            e.unknown();
        }
    }
    if (old) {
        sig.set(z1 + z2 + z3 + z4, n);
    }
    stretch.reduce();

    s->set_sig(sig, time_sig_type);
    s->set_stretch(stretch);
    s->set_numerator_string(numerator_string);
    s->set_denominator_string(denominator_string);
}

void TRead::read(TremoloTwoChord *t, XmlReader &xml, ReadContext &ctx) {
    compat::TremoloCompat tc{parent_or_dummy(t->chord(), ctx.dummy())};
    tc.two = t;
    read(&tc, xml, ctx);
}

void TRead::read(TremoloSingleChord *t, XmlReader &xml, ReadContext &ctx) {
    compat::TremoloCompat tc{parent_or_dummy(t->chord(), ctx.dummy())};
    tc.single = t;
    read(&tc, xml, ctx);
}

void TRead::read(compat::TremoloCompat *tc, XmlReader &e, ReadContext &ctx) {
    auto item = [](compat::TremoloCompat *tc) -> EngravingItem * {
        if (tc->two) {
            return tc->two;
        }
        return tc->single;
    };

    const Chord *chord = tc->parent.as<Chord>();
    const track_idx_t track = chord ? chord->track() : 0;

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            TremoloType type = TConv::from_xml(e.read_ascii_text(), TremoloType::INVALID_TREMOLO);
            if (is_tremolo_two_chord(type)) {
                if (!tc->two) {
                    tc->two = Factory::create_tremolo_two_chord(tc->parent);
                    tc->two->set_track(track);
                }
                tc->two->set_tremolo_type(type);
            } else {
                if (!tc->single) {
                    tc->single = Factory::create_tremolo_single_chord(tc->parent);
                    tc->single->set_track(track);
                }
                tc->single->set_tremolo_type(type);
            }
        }
        // Style needs special handling other than readStyledProperty()
        // to avoid calling customStyleApplicable() in setProperty(),
        // which cannot be called now because durationType() isn't defined yet.
        else if (tag == "strokeStyle") {
            if (tc->two) {
                tc->two->set_tremolo_style(TremoloStyle(e.read_int()));
                tc->two->set_property_flags(Pid::TREMOLO_STYLE, PropertyFlags::UNSTYLED);
            } else {
                UNREACHABLE;
                e.skip_current_element();
            }
        } else if (tag == "Fragment") {
            if (tc->two) {
                BeamFragment f = BeamFragment();
                int idx = tc->two->direction_idx();
                tc->two->set_user_modified(true);
                double spatium_value = tc->two->spatium();
                while (e.read_next_start_element()) {
                    const AsciiStringView tag1(e.name());
                    if (tag1 == "y1") {
                        f.py1[idx] = e.read_double() * spatium_value;
                    } else if (tag1 == "y2") {
                        f.py2[idx] = e.read_double() * spatium_value;
                    } else {
                        e.unknown();
                    }
                }
                tc->two->set_beam_fragment(f);
            } else {
                UNREACHABLE;
                e.skip_current_element();
            }
        } else if (tag == "play") {
            if (tc->two) {
                tc->two->set_play_tremolo(e.read_bool());
            } else if (tc->single) {
                tc->single->set_play_tremolo(e.read_bool());
            } else {
                UNREACHABLE;
            }
        } else if (TRead::read_styled_property(item(tc), tag, e, ctx)) {
        } else if (!read_item_properties(item(tc), e, ctx)) {
            e.unknown();
        }
    }
}

static constexpr std::array<Pid, 18> text_base_property_id{{
    Pid::TEXT_STYLE,
    Pid::FONT_FACE,
    Pid::FONT_SIZE,
    Pid::TEXT_LINE_SPACING,
    Pid::FONT_STYLE,
    Pid::COLOR,
    Pid::FRAME_TYPE,
    Pid::FRAME_WIDTH,
    Pid::FRAME_PADDING,
    Pid::FRAME_ROUND,
    Pid::FRAME_FG_COLOR,
    Pid::FRAME_BG_COLOR,
    Pid::ALIGN,
    Pid::POSITION,
}};

bool TRead::read_text_properties(TextBase *t, XmlReader &xml, ReadContext &ctx) {
    if (t->is_staff_text_base()) {
        return read_properties(dynamic_cast<StaffTextBase *>(t), xml, ctx);
    }
    return read_properties(t, xml, ctx);
}

bool TRead::read_properties(TextBase *t, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    for (Pid i : ::text_base_property_id) {
        if (TRead::read_property(t, tag, e, ctx, i)) {
            if (ctx.msc_version() >= 470 || tag != "align" || t->is_marker() || t->is_jump() || t->is_harmony()) {
                return true;
            }

            t->set_position(t->align().horizontal);
            if (t->position() != t->property_default(Pid::POSITION).value<AlignH>()) {
                t->set_property_flags(Pid::POSITION, PropertyFlags::UNSTYLED);
            }
            return true;
        }
    }

    if (tag == "text") {
        String str = line_break_from_tag(e.read_xml());
        t->set_xml_text(str);
        t->check_custom_formatting(str);
    } else if (tag == "bold") {
        bool val = e.read_int();
        if (val) {
            t->set_font_style(t->font_style() + FontStyle::BOLD);
        } else {
            t->set_font_style(t->font_style() - FontStyle::BOLD);
        }
        if (t->is_styled(Pid::FONT_STYLE)) {
            t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
        }
    } else if (tag == "italic") {
        bool val = e.read_int();
        if (val) {
            t->set_font_style(t->font_style() + FontStyle::ITALIC);
        } else {
            t->set_font_style(t->font_style() - FontStyle::ITALIC);
        }
        if (t->is_styled(Pid::FONT_STYLE)) {
            t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
        }
    } else if (tag == "underline") {
        bool val = e.read_int();
        if (val) {
            t->set_font_style(t->font_style() + FontStyle::UNDERLINE);
        } else {
            t->set_font_style(t->font_style() - FontStyle::UNDERLINE);
        }
        if (t->is_styled(Pid::FONT_STYLE)) {
            t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
        }
    } else if (tag == "strike") {
        bool val = e.read_int();
        if (val) {
            t->set_font_style(t->font_style() + FontStyle::STRIKE);
        } else {
            t->set_font_style(t->font_style() - FontStyle::STRIKE);
        }
        if (t->is_styled(Pid::FONT_STYLE)) {
            t->set_property_flags(Pid::FONT_STYLE, PropertyFlags::UNSTYLED);
        }
    } else if (TRead::read_property(t, tag, e, ctx, Pid::TEXT_LINKED_TO_MASTER)) {
    } else if (read_property(t, tag, e, ctx, Pid::VOICE_ASSIGNMENT)) {
    } else if (read_property(t, tag, e, ctx, Pid::DIRECTION)) {
    } else if (read_property(t, tag, e, ctx, Pid::CENTER_BETWEEN_STAVES)) {
    } else if (read_property(t, tag, e, ctx, Pid::MUSIC_SYMBOL_SIZE)) {
    } else if (read_property(t, tag, e, ctx, Pid::MUSICAL_SYMBOLS_SCALE)) {
    } else if (!read_item_properties(t, e, ctx)) {
        return false;
    }
    return true;
}

bool TRead::read_properties(TextLineBase *b, XmlReader &e, ReadContext &ctx) {
    const AsciiStringView tag(e.name());
    for (Pid i : TextLineBase::text_line_base_property_ids()) {
        if (TRead::read_property(b, tag, e, ctx, i)) {
            return true;
        }
    }
    return read_properties(static_cast<SLine *>(b), e, ctx);
}

void TRead::read(Trill *t, XmlReader &e, ReadContext &ctx) {
    t->erase_spanner_segments();

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            t->set_trill_type(TConv::from_xml(e.read_ascii_text(), TrillType::TRILL_LINE));
        } else if (tag == "Ornament") {
            Ornament *ornament = t->ornament();
            if (!ornament) {
                ornament = Factory::create_ornament(to_chord_rest(t->ownership_parent()));
                t->set_ornament(ornament);
            }
            ornament->set_sym_id(Ornament::from_trill_type(t->trill_type()));
            TRead::read(ornament, e, ctx);
            ornament->set_track(t->track());
        } else if (tag == "Accidental") {
            Accidental *accidental = Factory::create_accidental(t);
            TRead::read(accidental, e, ctx);
            t->set_accidental(accidental);
            if (t->ornament()) {
                t->ornament()->set_trill_old_compat_accidental(accidental);
            }
        } else if (tag == "ornamentStyle") {
            read_property(t, e, ctx, Pid::ORNAMENT_STYLE);
        } else if (!read_properties(static_cast<SLine *>(t), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Vibrato *v, XmlReader &e, ReadContext &ctx) {
    v->erase_spanner_segments();

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "subtype") {
            v->set_vibrato_type(TConv::from_xml(e.read_ascii_text(), VibratoType::GUITAR_VIBRATO));
        } else if (!read_properties(static_cast<SLine *>(v), e, ctx)) {
            e.unknown();
        }
    }
}

void TRead::read(Volta *v, XmlReader &e, ReadContext &ctx) {
    v->erase_spanner_segments();

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "endings") {
            v->set_endings(TConv::from_xml(e.read_text(), std::vector<int>()));
        } else if (!read_properties(v, e, ctx)) {
            e.unknown();
        }
    }
    if (ctx.msc_version() < 470) {
        compat::CompatUtils::set_text_line_text_position_from_align(v);
    }
}

bool TRead::read_properties(Volta *v, XmlReader &e, ReadContext &ctx) {
    if (!read_properties(to_text_line_base(v), e, ctx)) {
        return false;
    }

    return true;
}

void TRead::read_page_locks(Score *score, XmlReader &e) {
    while (e.read_next_start_element()) {
        if (e.name() == "pageLock") {
            read_page_lock(score, e);
        } else {
            e.unknown();
        }
    }
}

void TRead::read_system_locks(Score *score, XmlReader &e) {
    while (e.read_next_start_element()) {
        if (e.name() == "systemLock") {
            read_system_lock(score, e);
        } else {
            e.unknown();
        }
    }
}

void TRead::read_page_lock(Score *score, XmlReader &e) {
    MeasureBase *start_meas = nullptr;
    MeasureBase *end_meas = nullptr;
    EIDRegister *eid_register = score->master_score()->eid_register();

    while (e.read_next_start_element()) {
        AsciiStringView tag(e.name());
        if (tag == "startMeasure") {
            EID start_meas_id = EID::from_std_string(e.read_ascii_text());
            start_meas = to_measure_base(eid_register->item_from_eid(start_meas_id));
        } else if (tag == "endMeasure") {
            EID end_meas_id = EID::from_std_string(e.read_ascii_text());
            end_meas = to_measure_base(eid_register->item_from_eid(end_meas_id));
        } else {
            e.unknown();
        }
    }

    IF_ASSERT_FAILED(start_meas && end_meas) { return; }

    score->add_page_lock(new RangeLock(start_meas, end_meas));
}

void TRead::read_system_lock(Score *score, XmlReader &e) {
    MeasureBase *start_meas = nullptr;
    MeasureBase *end_meas = nullptr;
    EIDRegister *eid_register = score->master_score()->eid_register();

    while (e.read_next_start_element()) {
        AsciiStringView tag(e.name());
        if (tag == "startMeasure") {
            EID start_meas_id = EID::from_std_string(e.read_ascii_text());
            start_meas = to_measure_base(eid_register->item_from_eid(start_meas_id));
        } else if (tag == "endMeasure") {
            EID end_meas_id = EID::from_std_string(e.read_ascii_text());
            end_meas = to_measure_base(eid_register->item_from_eid(end_meas_id));
        } else {
            e.unknown();
        }
    }

    IF_ASSERT_FAILED(start_meas && end_meas) { return; }

    score->add_system_lock(new RangeLock(start_meas, end_meas));
}

void TRead::read_system_dividers(Score *score, XmlReader &e, ReadContext &ctx) {
    while (e.read_next_start_element()) {
        if (e.name() == "system") {
            size_t system_idx = e.int_attribute("idx");
            while (e.read_next_start_element()) {
                if (e.name() == "SystemDivider") {
                    SystemDivider *divider = new SystemDivider(score->dummy());
                    read(divider, e, ctx);
                    score->add_system_divider(system_idx, divider);
                } else {
                    e.unknown();
                }
            }
        } else {
            e.unknown();
        }
    }
}
