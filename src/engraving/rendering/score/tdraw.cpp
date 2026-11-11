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
#include "tdraw.h"

#include "defer.h"
#include "dom/accidental.h"
#include "dom/ambitus.h"
#include "dom/anchors.h"
#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/bagpembell.h"
#include "dom/barline.h"
#include "dom/beam.h"
#include "dom/bend.h"
#include "dom/box.h"
#include "dom/bracket.h"
#include "dom/breath.h"
#include "dom/capo.h"
#include "dom/chord.h"
#include "dom/chordbracket.h"
#include "dom/chordline.h"
#include "dom/clef.h"
#include "dom/deadslapped.h"
#include "dom/dynamic.h"
#include "dom/expression.h"
#include "dom/fermata.h"
#include "dom/figuredbass.h"
#include "dom/fingering.h"
#include "dom/fret.h"
#include "dom/glissando.h"
#include "dom/gradualtempochange.h"
#include "dom/guitarbend.h"
#include "dom/hairpin.h"
#include "dom/hammeronpulloff.h"
#include "dom/harmonicmark.h"
#include "dom/harmony.h"
#include "dom/harppedaldiagram.h"
#include "dom/hook.h"
#include "dom/image.h"
#include "dom/instrchange.h"
#include "dom/instrumentname.h"
#include "dom/jump.h"
#include "dom/keysig.h"
#include "dom/laissezvib.h"
#include "dom/layoutbreak.h"
#include "dom/ledgerline.h"
#include "dom/letring.h"
#include "dom/lyrics.h"
#include "dom/marker.h"
#include "dom/measurenumber.h"
#include "dom/measurerepeat.h"
#include "dom/mmrest.h"
#include "dom/mmrestrange.h"
#include "dom/note.h"
#include "dom/notedot.h"
#include "dom/noteline.h"
#include "dom/ornament.h"
#include "dom/ottava.h"
#include "dom/page.h"
#include "dom/pagelockindicator.h"
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
#include "dom/slur.h"
#include "dom/soundflag.h"
#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/stafflines.h"
#include "dom/staffstate.h"
#include "dom/stafftext.h"
#include "dom/stafftype.h"
#include "dom/stafftypechange.h"
#include "dom/staffvisibilityindicator.h"
#include "dom/stavesharinglabel.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/sticking.h"
#include "dom/stringtunings.h"
#include "dom/symbol.h"
#include "dom/systemdivider.h"
#include "dom/systemlockindicator.h"
#include "dom/systemtext.h"
#include "dom/tabdurationsymbol.h"
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
#include "dom/tripletfeel.h"
#include "dom/tuplet.h"
#include "dom/vibrato.h"
#include "dom/volta.h"
#include "dom/whammybar.h"
#include "draw/fontmetrics.h"
#include "draw/svgrenderer.h"
#include "editing/navigation.h"
#include "iengravingconfiguration.h"        // IWYU pragma: keep
#include "iengravingcontextconfiguration.h" // IWYU pragma: keep
#include "infrastructure/rtti.h"
#include "log.h"
#include "rendering/paintoptions.h"
#include "stemlayout.h"
#include "style/defaultstyle.h"
#include "style/style.h"

// dev
#include "dom/chord.h"
#include "dom/measure.h"
#include "dom/segment.h"
#include "dom/system.h"

using namespace mu::engraving;
using namespace mu::engraving::rtti;
using namespace mu::engraving::rendering::score;
using namespace muse;
using namespace muse::draw;

void TDraw::draw_item(const EngravingItem *item, Painter *painter, const PaintOptions &opt) {
    switch (item->type()) {
        case ElementType::ACCIDENTAL: draw(item_cast<const Accidental *>(item), painter, opt); break;
        case ElementType::AMBITUS: draw(item_cast<const Ambitus *>(item), painter, opt); break;
        case ElementType::ARPEGGIO: draw(item_cast<const Arpeggio *>(item), painter, opt); break;
        case ElementType::CHORD_BRACKET: draw(item_cast<const ChordBracket *>(item), painter, opt); break;
        case ElementType::ARTICULATION: draw(item_cast<const Articulation *>(item), painter, opt); break;

        case ElementType::BAGPIPE_EMBELLISHMENT: draw(item_cast<const BagpipeEmbellishment *>(item), painter, opt); break;
        case ElementType::BAR_LINE: draw(item_cast<const BarLine *>(item), painter, opt); break;
        case ElementType::BEAM: draw(item_cast<const Beam *>(item), painter, opt); break;
        case ElementType::BEND: draw(item_cast<const Bend *>(item), painter, opt); break;
        case ElementType::HBOX: draw(item_cast<const HBox *>(item), painter, opt); break;
        case ElementType::VBOX: draw(item_cast<const VBox *>(item), painter, opt); break;
        case ElementType::FBOX: draw(item_cast<const FBox *>(item), painter, opt); break;
        case ElementType::TBOX: draw(item_cast<const TBox *>(item), painter, opt); break;
        case ElementType::BRACKET: draw(item_cast<const Bracket *>(item), painter, opt); break;
        case ElementType::BREATH: draw(item_cast<const Breath *>(item), painter, opt); break;

        case ElementType::CHORDLINE: draw(item_cast<const ChordLine *>(item), painter, opt); break;
        case ElementType::CLEF: draw(item_cast<const Clef *>(item), painter, opt); break;
        case ElementType::CAPO: draw(item_cast<const Capo *>(item), painter, opt); break;

        case ElementType::DEAD_SLAPPED: draw(item_cast<const DeadSlapped *>(item), painter, opt); break;
        case ElementType::DYNAMIC: draw(item_cast<const Dynamic *>(item), painter, opt); break;

        case ElementType::EXPRESSION: draw(item_cast<const Expression *>(item), painter, opt); break;

        case ElementType::FERMATA: draw(item_cast<const Fermata *>(item), painter, opt); break;
        case ElementType::FIGURED_BASS: draw(item_cast<const FiguredBass *>(item), painter, opt); break;
        case ElementType::FINGERING: draw(item_cast<const Fingering *>(item), painter, opt); break;
        case ElementType::FRET_DIAGRAM: draw(item_cast<const FretDiagram *>(item), painter, opt); break;
        case ElementType::FSYMBOL: draw(item_cast<const FSymbol *>(item), painter, opt); break;

        case ElementType::GLISSANDO_SEGMENT: draw(item_cast<const GlissandoSegment *>(item), painter, opt); break;
        case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT: draw(item_cast<const GradualTempoChangeSegment *>(item), painter, opt); break;
        case ElementType::GUITAR_BEND_SEGMENT: draw(item_cast<const GuitarBendSegment *>(item), painter, opt); break;
        case ElementType::GUITAR_BEND_HOLD_SEGMENT: draw(item_cast<const GuitarBendHoldSegment *>(item), painter, opt); break;
        case ElementType::GUITAR_BEND_TEXT: draw_text_base(to_text_base(item), painter, opt); break;

        case ElementType::HAIRPIN_SEGMENT: draw(item_cast<const HairpinSegment *>(item), painter, opt); break;
        case ElementType::HAMMER_ON_PULL_OFF_SEGMENT: draw(item_cast<const HammerOnPullOffSegment *>(item), painter, opt); break;
        case ElementType::HAMMER_ON_PULL_OFF_TEXT: draw(item_cast<const HammerOnPullOffText *>(item), painter, opt); break;
        case ElementType::HARP_DIAGRAM: draw(item_cast<const HarpPedalDiagram *>(item), painter, opt); break;
        case ElementType::HARMONIC_MARK_SEGMENT: draw(item_cast<const HarmonicMarkSegment *>(item), painter, opt); break;
        case ElementType::HARMONY: draw(item_cast<const Harmony *>(item), painter, opt); break;
        case ElementType::HOOK: draw(item_cast<const Hook *>(item), painter, opt); break;

        case ElementType::IMAGE: draw(item_cast<const Image *>(item), painter, opt); break;
        case ElementType::INSTRUMENT_CHANGE: draw(item_cast<const InstrumentChange *>(item), painter, opt); break;
        case ElementType::INSTRUMENT_NAME: draw(item_cast<const InstrumentName *>(item), painter, opt); break;

        case ElementType::JUMP: draw(item_cast<const Jump *>(item), painter, opt); break;

        case ElementType::KEYSIG: draw(item_cast<const KeySig *>(item), painter, opt); break;
        case ElementType::LAISSEZ_VIB_SEGMENT: draw(item_cast<const LaissezVibSegment *>(item), painter, opt); break;
        case ElementType::LAYOUT_BREAK: draw(item_cast<const LayoutBreak *>(item), painter, opt); break;
        case ElementType::LEDGER_LINE: draw(item_cast<const LedgerLine *>(item), painter, opt); break;
        case ElementType::LET_RING_SEGMENT: draw(item_cast<const LetRingSegment *>(item), painter, opt); break;
        case ElementType::LYRICS: draw(item_cast<const Lyrics *>(item), painter, opt); break;
        case ElementType::LYRICSLINE_SEGMENT: draw(item_cast<const LyricsLineSegment *>(item), painter, opt); break;
        case ElementType::PARTIAL_LYRICSLINE_SEGMENT: draw(item_cast<const LyricsLineSegment *>(item), painter, opt); break;

        case ElementType::MARKER: draw(item_cast<const Marker *>(item), painter, opt); break;
        case ElementType::MEASURE_NUMBER: draw(item_cast<const MeasureNumber *>(item), painter, opt); break;
        case ElementType::MEASURE_REPEAT: draw(item_cast<const MeasureRepeat *>(item), painter, opt); break;
        case ElementType::MMREST: draw(item_cast<const MMRest *>(item), painter, opt); break;
        case ElementType::MMREST_RANGE: draw(item_cast<const MMRestRange *>(item), painter, opt); break;

        case ElementType::NOTE: draw(item_cast<const Note *>(item), painter, opt); break;
        case ElementType::NOTEDOT: draw(item_cast<const NoteDot *>(item), painter, opt); break;
        case ElementType::NOTEHEAD: draw(item_cast<const NoteHead *>(item), painter, opt); break;
        case ElementType::NOTELINE_SEGMENT: draw(item_cast<const NoteLineSegment *>(item), painter, opt); break;

        case ElementType::ORNAMENT: draw(item_cast<const Ornament *>(item), painter, opt); break;
        case ElementType::OTTAVA_SEGMENT: draw(item_cast<const OttavaSegment *>(item), painter, opt); break;
        case ElementType::PAGE_LOCK_INDICATOR: draw(item_cast<const PageLockIndicator *>(item), painter, opt); break;
        case ElementType::PARENTHESIS: draw(item_cast<const Parenthesis *>(item), painter, opt); break;
        case ElementType::PARTIAL_TIE_SEGMENT: draw(item_cast<const PartialTieSegment *>(item), painter, opt); break;
        case ElementType::PALM_MUTE_SEGMENT: draw(item_cast<const PalmMuteSegment *>(item), painter, opt); break;
        case ElementType::PEDAL_SEGMENT: draw(item_cast<const PedalSegment *>(item), painter, opt); break;
        case ElementType::PICK_SCRAPE_SEGMENT: draw(item_cast<const PickScrapeSegment *>(item), painter, opt); break;
        case ElementType::PLAY_COUNT_TEXT: draw(item_cast<const PlayCountText *>(item), painter, opt); break;
        case ElementType::PLAYTECH_ANNOTATION: draw(item_cast<const PlayTechAnnotation *>(item), painter, opt); break;

        case ElementType::RASGUEADO_SEGMENT: draw(item_cast<const RasgueadoSegment *>(item), painter, opt); break;
        case ElementType::REHEARSAL_MARK: draw(item_cast<const RehearsalMark *>(item), painter, opt); break;
        case ElementType::REST: draw(item_cast<const Rest *>(item), painter, opt); break;

        case ElementType::SLUR_SEGMENT: draw(item_cast<const SlurSegment *>(item), painter, opt); break;
        case ElementType::SPACER: draw(item_cast<const Spacer *>(item), painter, opt); break;
        case ElementType::STAFF_LINES: draw(item_cast<const StaffLines *>(item), painter, opt); break;
        case ElementType::STAFF_STATE: draw(item_cast<const StaffState *>(item), painter, opt); break;
        case ElementType::STAFF_TEXT: draw(item_cast<const StaffText *>(item), painter, opt); break;
        case ElementType::STAVE_SHARING_LABEL: draw(item_cast<const StaveSharingLabel *>(item), painter, opt); break;
        case ElementType::STAFFTYPE_CHANGE: draw(item_cast<const StaffTypeChange *>(item), painter, opt); break;
        case ElementType::STAFF_VISIBILITY_INDICATOR: draw(item_cast<const StaffVisibilityIndicator *>(item), painter, opt); break;
        case ElementType::STEM: draw(item_cast<const Stem *>(item), painter, opt); break;
        case ElementType::STEM_SLASH: draw(item_cast<const StemSlash *>(item), painter, opt); break;
        case ElementType::STICKING: draw(item_cast<const Sticking *>(item), painter, opt); break;
        case ElementType::STRING_TUNINGS: draw(item_cast<const StringTunings *>(item), painter, opt); break;
        case ElementType::SYMBOL: draw(item_cast<const Symbol *>(item), painter, opt); break;
        case ElementType::SYSTEM_DIVIDER: draw(item_cast<const SystemDivider *>(item), painter, opt); break;
        case ElementType::SYSTEM_TEXT: draw(item_cast<const SystemText *>(item), painter, opt); break;
        case ElementType::SYSTEM_LOCK_INDICATOR: draw(item_cast<const SystemLockIndicator *>(item), painter, opt); break;
        case ElementType::SOUND_FLAG: draw(item_cast<const SoundFlag *>(item), painter, opt); break;

        case ElementType::TAB_DURATION_SYMBOL: draw(item_cast<const TabDurationSymbol *>(item), painter, opt); break;
        case ElementType::TAPPING: draw(to_tapping(item), painter, opt); break;
        case ElementType::TAPPING_HALF_SLUR_SEGMENT: draw(to_slur_segment(item), painter, opt); break;
        case ElementType::TEMPO_TEXT: draw(item_cast<const TempoText *>(item), painter, opt); break;
        case ElementType::TEXT: draw(item_cast<const Text *>(item), painter, opt); break;
        case ElementType::TEXTLINE_SEGMENT: draw(item_cast<const TextLineSegment *>(item), painter, opt); break;
        case ElementType::TIE_SEGMENT: draw(item_cast<const TieSegment *>(item), painter, opt); break;
        case ElementType::TIMESIG: draw(item_cast<const TimeSig *>(item), painter, opt); break;
        case ElementType::TIME_TICK_ANCHOR: draw(item_cast<const TimeTickAnchor *>(item), painter, opt); break;
        case ElementType::TREMOLO_SINGLECHORD: draw(item_cast<const TremoloSingleChord *>(item), painter, opt); break;
        case ElementType::TREMOLO_TWOCHORD: draw(item_cast<const TremoloTwoChord *>(item), painter, opt); break;
        case ElementType::TREMOLOBAR: draw(item_cast<const TremoloBar *>(item), painter, opt); break;
        case ElementType::TRILL_SEGMENT: draw(item_cast<const TrillSegment *>(item), painter, opt); break;
        case ElementType::TRIPLET_FEEL: draw(item_cast<const TripletFeel *>(item), painter, opt); break;
        case ElementType::TUPLET: draw(item_cast<const Tuplet *>(item), painter, opt); break;

        case ElementType::VIBRATO_SEGMENT: draw(item_cast<const VibratoSegment *>(item), painter, opt); break;
        case ElementType::VOLTA_SEGMENT: draw(item_cast<const VoltaSegment *>(item), painter, opt); break;

        case ElementType::WHAMMY_BAR_SEGMENT: draw(item_cast<const WhammyBarSegment *>(item), painter, opt); break;

        case ElementType::PAGE:
        case ElementType::SYSTEM:
        case ElementType::MEASURE:
        case ElementType::SEGMENT:
        case ElementType::CHORD:
        case ElementType::GRACE_NOTES_GROUP: break;

        default: MUSE_NOT_IMPLEMENTED << " type: " << item->type_name(); UNREACHABLE;
    }
}

void TDraw::draw(const Accidental *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    IF_ASSERT_FAILED(item->ldata()) { return; }

    painter->set_pen(item->cur_color(opt));
    for (const Accidental::LayoutData::Sym &e : item->ldata()->syms) {
        item->draw_symbol(e.sym, painter, PointF(e.x, e.y));
    }
}

void TDraw::draw(const Ambitus *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const Ambitus::LayoutData *ldata = item->ldata();
    IF_ASSERT_FAILED(ldata) { return; }

    double spatium = item->spatium();
    double lw = item->line_width().val() * spatium;
    painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));

    item->draw_symbol(item->note_head(), painter, ldata->top_pos);
    item->draw_symbol(item->note_head(), painter, ldata->bottom_pos);
    if (item->has_line()) {
        painter->draw_line(ldata->line);
    }

    // draw ledger lines (if not in a palette)
    if (item->segment() && item->track() != muse::nidx) {
        Fraction tick = item->segment()->tick();
        Staff *staff = item->score()->staff(item->staff_idx());
        double line_dist = staff->line_distance(tick);
        int num_of_lines = staff->lines(tick);
        double step = line_dist * spatium;
        double step_tolerance = step * 0.1;
        double ledger_line_length = item->style().style_s(Sid::LEDGER_LINE_LENGTH).val() * spatium;
        double ledger_line_width = item->style().style_s(Sid::LEDGER_LINE_WIDTH).val() * spatium;
        painter->set_pen(Pen(item->cur_color(opt), ledger_line_width, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));

        if (ldata->top_pos.y() - step_tolerance <= -step) {
            double x_min = ldata->top_pos.x() - ledger_line_length;
            double x_max = ldata->top_pos.x() + item->head_width() + ledger_line_length;
            for (double y = -step; y >= ldata->top_pos.y() - step_tolerance; y -= step) {
                painter->draw_line(PointF(x_min, y), PointF(x_max, y));
            }
        }

        if (ldata->bottom_pos.y() + step_tolerance >= num_of_lines * step) {
            double x_min = ldata->bottom_pos.x() - ledger_line_length;
            double x_max = ldata->bottom_pos.x() + item->head_width() + ledger_line_length;
            for (double y = num_of_lines * step; y <= ldata->bottom_pos.y() + step_tolerance; y += step) {
                painter->draw_line(PointF(x_min, y), PointF(x_max, y));
            }
        }
    }
}

void TDraw::draw(const Arpeggio *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const Arpeggio::LayoutData *ldata = item->ldata();
    IF_ASSERT_FAILED(ldata) { return; }

    const double y1 = ldata->bbox().top();
    const double y2 = ldata->bbox().bottom();
    const double line_width = item->style().style_absolute(Sid::ARPEGGIO_LINE_WIDTH);

    painter->set_pen(Pen(item->cur_color(opt), line_width, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
    painter->save();

    switch (item->arpeggio_type()) {
        case ArpeggioType::NORMAL:
        case ArpeggioType::UP: {
            const RectF &r = ldata->syms_b_box;
            painter->rotate(-90.0);
            item->draw_symbols(ldata->symbols, painter, PointF(-r.right() - y1, -r.bottom() + r.height()));
        } break;

        case ArpeggioType::DOWN: {
            const RectF &r = ldata->syms_b_box;
            painter->rotate(90.0);
            item->draw_symbols(ldata->symbols, painter, PointF(-r.left() + y1, -r.top() - r.height()));
        } break;

        case ArpeggioType::UP_STRAIGHT: {
            const RectF &r = ldata->syms_b_box;
            double x1 = item->spatium() * 0.5;
            item->draw_symbol(SymId::ARROWHEAD_BLACK_UP, painter, PointF(x1 - r.width() * 0.5, y1 - r.top()));
            double ny1 = y1 - r.top() * 0.5;
            painter->draw_line(LineF(x1, ny1, x1, y2));
        } break;

        case ArpeggioType::DOWN_STRAIGHT: {
            const RectF &r = ldata->syms_b_box;
            double x1 = item->spatium() * 0.5;
            item->draw_symbol(SymId::ARROWHEAD_BLACK_DOWN, painter, PointF(x1 - r.width() * 0.5, y2 - r.bottom()));
            double ny2 = y2 + r.top() * 0.5;
            painter->draw_line(LineF(x1, y1, x1, ny2));
        } break;

        case ArpeggioType::BRACKET: {
            double w = item->style().style_s(Sid::ARPEGGIO_HOOK_LEN).val() * item->spatium();
            painter->draw_line(LineF(0.0, y1, w, y1));
            painter->draw_line(LineF(0.0, y2, w, y2));
            painter->draw_line(LineF(0.0, y1 - line_width / 2, 0.0, y2 + line_width / 2));
        } break;
    }
    painter->restore();
}

void TDraw::draw(const ChordBracket *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    const Arpeggio::LayoutData *ldata = item->ldata();

    const double line_width = item->style().style_absolute(Sid::CHORD_BRACKET_LINE_WIDTH);
    painter->set_pen(Pen(item->cur_color(opt), line_width, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));

    const double half_line_width = 0.5 * line_width;
    const double y1 = ldata->bbox().top() + half_line_width;
    const double y2 = ldata->bbox().bottom() - half_line_width;

    double w = item->absolute_from_spatium(item->hook_length());

    if (item->hook_pos() != DirectionV::DOWN) {
        painter->draw_line(LineF(0.0, y1, w, y1));
    }
    if (item->hook_pos() != DirectionV::UP) {
        painter->draw_line(LineF(0.0, y2, w, y2));
    }

    const double x = item->right_side() ? w - half_line_width : half_line_width;
    painter->draw_line(LineF(x, y1, x, y2));
}

void TDraw::draw(const Articulation *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    painter->set_pen(item->cur_color(opt));

    if (item->text_type() == ArticulationTextType::NO_TEXT) {
        item->draw_symbol(item->sym_id(), painter);
    } else {
        draw_text_base(item->text(), painter, opt);
    }
}

void TDraw::draw(const Ornament *item, Painter *painter, const PaintOptions &opt) {
    draw(static_cast<const Articulation *>(item), painter, opt);
}

void TDraw::draw(const BagpipeEmbellishment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const BagpipeEmbellishment::LayoutData *data = item->ldata();
    IF_ASSERT_FAILED(data) { return; }
    const BagpipeEmbellishment::LayoutData::BeamData &data_beam = data->beam_data;

    Pen pen(item->cur_color(opt), data->stem_line_w, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP);
    painter->set_pen(pen);

    // draw the notes including stem, (optional) flag and (optional) ledger line
    for (const auto &p : data->notes_data) {
        const BagpipeEmbellishment::LayoutData::NoteData &note_data = p.second;

        // Draw Grace Note
        {
            // draw head
            item->draw_symbol(data->headsym, painter, note_data.head_xy);

            // draw stem
            painter->draw_line(note_data.stem_line);

            if (data->is_draw_flag) {
                // draw flag
                item->draw_symbol(data->flagsym, painter, note_data.flag_xy);
            }
        }

        // draw the ledger line for high A
        if (!note_data.ledger_line.is_null()) {
            painter->draw_line(note_data.ledger_line);
        }
    }

    if (data->is_draw_beam) {
        Pen beam_pen(item->cur_color(opt), data_beam.width, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP);
        painter->set_pen(beam_pen);
        // draw the beams
        auto draw_beams = [](Painter *painter, const double spatium, const double x1, const double x2, double y) {
            // draw the beams
            painter->draw_line(LineF(x1, y, x2, y));
            y += spatium / 1.5;
            painter->draw_line(LineF(x1, y, x2, y));
            y += spatium / 1.5;
            painter->draw_line(LineF(x1, y, x2, y));
        };

        draw_beams(painter, data->spatium, data_beam.x1, data_beam.x2, data_beam.y);
    }
}

static void draw_dots(const BarLine *item, Painter *painter, double x) {
    double spatium = item->spatium();

    double y1l;
    double y2l;
    if (item->ownership_parent() == 0) { // for use in palette (always Bravura)
        //Bravura shifted repeatDot symbol 0.5sp upper in the font itself (1.272)
        y1l = 1.5 * spatium;
        y2l = 2.5 * spatium;
    } else {
        const StaffType *st = item->staff_type();
        const int lines = st->lines();
        const double line_distance = item->absolute_from_spatium(st->line_distance());

        y1l = (static_cast<double>((lines - 1) / 2) - 0.5) * line_distance;
        y2l = (static_cast<double>(lines / 2) + 0.5) * line_distance;

        //adjust for staffType offset
        double st_y_offset = item->staff_offset_y();
        y1l += st_y_offset;
        y2l += st_y_offset;
    }

    item->draw_symbol(SymId::REPEAT_DOT, painter, PointF(x, y1l));
    item->draw_symbol(SymId::REPEAT_DOT, painter, PointF(x, y2l));
}

static void draw_tips(const BarLine *item, const BarLine::LayoutData *data, Painter *painter, bool reversed, double x) {
    if (reversed) {
        if (item->is_top()) {
            item->draw_symbol(SymId::REVERSED_BRACKET_TOP, painter, PointF(x - item->sym_width(SymId::REVERSED_BRACKET_TOP), data->y1));
        }
        if (item->is_bottom()) {
            item->draw_symbol(SymId::REVERSED_BRACKET_BOTTOM, painter,
                              PointF(x - item->sym_width(SymId::REVERSED_BRACKET_BOTTOM), data->y2));
        }
    } else {
        if (item->is_top()) {
            item->draw_symbol(SymId::BRACKET_TOP, painter, PointF(x, data->y1));
        }
        if (item->is_bottom()) {
            item->draw_symbol(SymId::BRACKET_BOTTOM, painter, PointF(x, data->y2));
        }
    }
}

void TDraw::draw(const BarLine *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const BarLine::LayoutData *data = item->ldata();
    IF_ASSERT_FAILED(data) { return; }

    painter->save();
    DEFER { painter->restore(); };

    set_mask(item, painter);

    switch (item->bar_line_type()) {
        case BarLineType::NORMAL: {
            double lw = item->style().style_absolute(Sid::BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            painter->draw_line(LineF(lw * .5, data->y1, lw * .5, data->y2));
        } break;

        case BarLineType::BROKEN: {
            double lw = item->style().style_absolute(Sid::DASH_BAR_WIDTH) * item->mag();
            double dl = real_is_null(lw) ? 0.0 : item->style().style_absolute(Sid::DASH_BAR_DASH) * item->mag() / lw;
            double gl = real_is_null(lw) ? 0.0 : item->style().style_absolute(Sid::DASH_BAR_GAP) * item->mag() / lw;
            Pen pen(item->cur_color(opt), lw, PenStyle::DASH_LINE, PenCapStyle::FLAT_CAP);
            pen.set_dash_pattern({dl, gl});
            painter->set_pen(pen);
            painter->draw_line(LineF(lw * .5, data->y1, lw * .5, data->y2));
        } break;

        case BarLineType::DOTTED: {
            double lw = item->style().style_absolute(Sid::BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::DOT_LINE, PenCapStyle::FLAT_CAP));
            painter->draw_line(LineF(lw * .5, data->y1, lw * .5, data->y2));
        } break;

        case BarLineType::END: {
            double lw = item->style().style_absolute(Sid::BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            double x = lw * .5;
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            double lw2 = item->style().style_absolute(Sid::END_BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw2, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            x += ((lw * .5) + item->style().style_absolute(Sid::END_BAR_DISTANCE) + (lw2 * .5)) * item->mag();
            painter->draw_line(LineF(x, data->y1, x, data->y2));
        } break;

        case BarLineType::DOUBLE: {
            double lw = item->style().style_absolute(Sid::DOUBLE_BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            double x = lw * .5;
            painter->draw_line(LineF(x, data->y1, x, data->y2));
            x += ((lw * .5) + item->style().style_absolute(Sid::DOUBLE_BAR_DISTANCE) + (lw * .5)) * item->mag();
            painter->draw_line(LineF(x, data->y1, x, data->y2));
        } break;

        case BarLineType::REVERSE_END: {
            double lw = item->style().style_absolute(Sid::END_BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            double x = lw * .5;
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            double lw2 = item->style().style_absolute(Sid::BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw2, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            x += ((lw * .5) + item->style().style_absolute(Sid::END_BAR_DISTANCE) + (lw2 * .5)) * item->mag();
            painter->draw_line(LineF(x, data->y1, x, data->y2));
        } break;

        case BarLineType::HEAVY: {
            double lw = item->style().style_absolute(Sid::END_BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            painter->draw_line(LineF(lw * .5, data->y1, lw * .5, data->y2));
        } break;

        case BarLineType::DOUBLE_HEAVY: {
            double lw2 = item->style().style_absolute(Sid::END_BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw2, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            double x = lw2 * .5;
            painter->draw_line(LineF(x, data->y1, x, data->y2));
            x += ((lw2 * .5) + item->style().style_absolute(Sid::END_BAR_DISTANCE) + (lw2 * .5)) * item->mag();
            painter->draw_line(LineF(x, data->y1, x, data->y2));
        } break;

        case BarLineType::START_REPEAT: {
            double lw2 = item->style().style_absolute(Sid::END_BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw2, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            double x = lw2 * .5;
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            double lw = item->style().style_absolute(Sid::BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            x += ((lw2 * .5) + item->style().style_absolute(Sid::END_BAR_DISTANCE) + (lw * .5)) * item->mag();
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            x += ((lw * .5) + item->style().style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION)) * item->mag();
            draw_dots(item, painter, x);

            if (item->style().style_b(Sid::REPEAT_BAR_TIPS)) {
                draw_tips(item, data, painter, false, 0.0);
            }
        } break;

        case BarLineType::END_REPEAT: {
            double lw = item->style().style_absolute(Sid::BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));

            double x = 0.0;
            draw_dots(item, painter, x);

            x += item->sym_bbox(SymId::REPEAT_DOT).width();
            x += (item->style().style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION) + (lw * .5)) * item->mag();
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            double lw2 = item->style().style_absolute(Sid::END_BAR_WIDTH) * item->mag();
            x += ((lw * .5) + item->style().style_absolute(Sid::END_BAR_DISTANCE) + (lw2 * .5)) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw2, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            if (item->style().style_b(Sid::REPEAT_BAR_TIPS)) {
                draw_tips(item, data, painter, true, x + lw2 * .5);
            }
        } break;
        case BarLineType::END_START_REPEAT: {
            double lw = item->style().style_absolute(Sid::BAR_WIDTH) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));

            double x = 0.0;
            draw_dots(item, painter, x);

            x += item->sym_bbox(SymId::REPEAT_DOT).width();
            x += (item->style().style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION) + (lw * .5)) * item->mag();
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            double lw2 = item->style().style_absolute(Sid::END_BAR_WIDTH) * item->mag();
            x += ((lw * .5) + item->style().style_absolute(Sid::END_BAR_DISTANCE) + (lw2 * .5)) * item->mag();
            painter->set_pen(Pen(item->cur_color(opt), lw2, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            if (item->style().style_b(Sid::REPEAT_BAR_TIPS)) {
                draw_tips(item, data, painter, true, x + lw2 * .5);
            }

            painter->set_pen(Pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
            x += ((lw2 * .5) + item->style().style_absolute(Sid::END_BAR_DISTANCE) + (lw * .5)) * item->mag();
            painter->draw_line(LineF(x, data->y1, x, data->y2));

            x += ((lw * .5) + item->style().style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION)) * item->mag();
            draw_dots(item, painter, x);

            if (item->style().style_b(Sid::REPEAT_BAR_TIPS)) {
                draw_tips(item, data, painter, false, 0.0);
            }
        } break;
    }

    // draw irregular measure mark

    if (opt.is_printing || !item->score()->mark_irregular_measures()) {
        return;
    }

    const Segment *s = item->segment();
    if (s && (s->is_end_bar_line_type() || s->is_start_repeat_bar_line_type())) {
        const Measure *measure = s->measure();
        if (s->is_start_repeat_bar_line_type()) {
            const Measure *prev_measure = measure ? measure->prev_measure() : nullptr;
            if (!prev_measure) {
                return;
            }
            if (const BarLine *prev_end_bl = prev_measure->end_bar_line(item->staff_idx())) {
                if (prev_end_bl->segment() && prev_end_bl->segment()->enabled()) {
                    return;
                }
            }
            measure = prev_measure;
        }

        if (measure && measure->is_irregular() && !measure->is_mm_rest()) {
            painter->set_pen(item->configuration()->invisible_color());
            Font f(u"Edwin", Font::Type::TEXT);
            f.set_point_size_f(12 * item->spatium() / item->default_spatium());
            f.set_bold(true);
            Char ch = measure->ticks() > measure->timesig() ? u'+' : u'-';
            RectF r = FontMetrics(f).bounding_rect(ch);

            painter->set_font(f);
            painter->draw_text(-r.width(), -item->spatium(), ch);
        }
    }
}

void TDraw::draw(const Beam *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    if (item->beam_segments().empty()) {
        return;
    }
    painter->set_brush(Brush(item->cur_color(opt)));
    painter->set_no_pen();

    // make beam thickness independent of slant
    // (expression can be simplified?)

    const LineF bs = item->beam_segments().front()->line;
    double d = (std::abs(bs.y2() - bs.y1())) / (bs.x2() - bs.x1());
    if (item->beam_segments().size() > 1 && d > M_PI / 6.0) {
        d = M_PI / 6.0;
    }
    double ww = (item->beam_width() / 2.0) / sin(M_PI_2 - atan(d));

    for (const BeamSegment *bs1 : item->beam_segments()) {
        painter->draw_polygon(PolygonF({
                                  PointF(bs1->line.x1(), bs1->line.y1() - ww),
                                  PointF(bs1->line.x2(), bs1->line.y2() - ww),
                                  PointF(bs1->line.x2(), bs1->line.y2() + ww),
                                  PointF(bs1->line.x1(), bs1->line.y1() + ww),
                              }),
                              FillRule::ODD_EVEN_FILL);
    }
}

void TDraw::draw(const Bend *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const Bend::LayoutData *data = item->ldata();
    IF_ASSERT_FAILED(data) { return; }

    double spatium = item->spatium();
    double lw = item->absolute_from_spatium(item->line_width());

    Pen pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::ROUND_CAP, PenJoinStyle::ROUND_JOIN);
    painter->set_pen(pen);
    painter->set_brush(Brush(item->cur_color(opt)));

    Font f = item->font(spatium);

    double x = data->note_width + spatium * .2;
    double y = -spatium * .8;
    double x2, y2;

    double aw = item->style().style_absolute(Sid::BEND_ARROW_WIDTH);
    PolygonF arrow_up;
    arrow_up << PointF(0, 0) << PointF(aw * .5, aw) << PointF(-aw * .5, aw);
    PolygonF arrow_down;
    arrow_down << PointF(0, 0) << PointF(aw * .5, -aw) << PointF(-aw * .5, -aw);

    size_t n = item->points().size();
    for (size_t pt = 0; pt < n - 1; ++pt) {
        int pitch = item->points()[pt].pitch;
        if (pt == 0 && pitch) {
            y2 = -data->note_pos.y() - spatium * 2;
            x2 = x;
            painter->draw_line(LineF(x, y, x2, y2));

            painter->set_brush(item->cur_color(opt));
            painter->draw_polygon(arrow_up.translated(x2, y2));

            int idx = (pitch + 12) / 25;
            const char *l = item->label[idx];
            painter->set_font(f);
            painter->draw_text(RectF(x2, y2, .0, .0), muse::draw::ALIGN_H_CENTER | muse::draw::ALIGN_BOTTOM, muse::draw::TEXT_DONT_CLIP,
                               String::from_ascii(l));

            y = y2;
        }
        if (pitch == item->points()[pt + 1].pitch) {
            if (pt == (n - 2)) {
                break;
            }
            x2 = x + spatium;
            y2 = y;
            painter->draw_line(LineF(x, y, x2, y2));
        } else if (pitch < item->points()[pt + 1].pitch) {
            // up
            x2 = x + spatium * .5;
            y2 = -data->note_pos.y() - spatium * 2;
            double dx = x2 - x;
            double dy = y2 - y;

            PainterPath path;
            path.move_to(x, y);
            path.cubic_to(x + dx / 2, y, x2, y + dy / 4, x2, y2);
            painter->set_brush(BrushStyle::NO_BRUSH);
            painter->draw_path(path);

            painter->set_brush(item->cur_color(opt));
            painter->draw_polygon(arrow_up.translated(x2, y2));

            int idx = (item->points()[pt + 1].pitch + 12) / 25;
            const char *l = item->label[idx];
            double ty = y2; // - _spatium;
            painter->set_font(f);
            painter->draw_text(RectF(x2, ty, .0, .0), muse::draw::ALIGN_H_CENTER | muse::draw::ALIGN_BOTTOM, muse::draw::TEXT_DONT_CLIP,
                               String::from_ascii(l));
        } else {
            // down
            x2 = x + spatium * .5;
            y2 = y + spatium * 3;
            double dx = x2 - x;
            double dy = y2 - y;

            PainterPath path;
            path.move_to(x, y);
            path.cubic_to(x + dx / 2, y, x2, y + dy / 4, x2, y2);
            painter->set_brush(BrushStyle::NO_BRUSH);
            painter->draw_path(path);

            painter->set_brush(item->cur_color(opt));
            painter->draw_polygon(arrow_down.translated(x2, y2));
        }
        x = x2;
        y = y2;
    }
}

void TDraw::draw(const Box *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    if (opt.is_printing) {
        return;
    }

    const Box::LayoutData *ldata = item->ldata();

    const bool show_highlighted_frame = item->selected() || item->drop_target();
    const bool show_frame = show_highlighted_frame || (item->score() ? item->score()->show_frames() : false);

    if (show_frame) {
        double line_width = item->default_spatium() * .10;
        Pen pen;
        pen.set_width_f(line_width);
        pen.set_join_style(PenJoinStyle::ROUND_JOIN);
        pen.set_cap_style(PenCapStyle::ROUND_CAP);
        pen.set_color(show_highlighted_frame ? item->configuration()->selection_color() : item->configuration()->frame_color());
        pen.set_dash_pattern({5, 5});

        painter->set_brush(BrushStyle::NO_BRUSH);
        painter->set_pen(pen);
        line_width *= 0.5;
        painter->draw_rect(ldata->bbox().adjusted(line_width, line_width, -line_width, -line_width));
    }
}

void TDraw::draw(const HBox *item, Painter *painter, const PaintOptions &opt) { draw(static_cast<const Box *>(item), painter, opt); }

void TDraw::draw(const VBox *item, Painter *painter, const PaintOptions &opt) { draw(static_cast<const Box *>(item), painter, opt); }

void TDraw::draw(const FBox *item, Painter *painter, const PaintOptions &opt) { draw(static_cast<const Box *>(item), painter, opt); }

void TDraw::draw(const TBox *item, Painter *painter, const PaintOptions &opt) { draw(static_cast<const Box *>(item), painter, opt); }

void TDraw::draw(const Bracket *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const Bracket::LayoutData *ldata = item->ldata();
    IF_ASSERT_FAILED(ldata) { return; }

    switch (item->bracket_type()) {
        case BracketType::BRACE: {
            if (ldata->brace_symbol == SymId::NO_SYM) {
                painter->set_no_pen();
                painter->set_brush(Brush(item->cur_color(opt)));
                painter->draw_path(ldata->path);
            } else {
                double h = ldata->bracket_height;
                double glyph_height = item->sym_height(ldata->brace_symbol);
                double mag = h / glyph_height;
                painter->set_pen(item->cur_color(opt));
                painter->save();
                painter->scale(item->magx(), mag);
                item->draw_symbol(ldata->brace_symbol, painter, PointF(0.0, glyph_height));
                painter->restore();
            }
        } break;
        case BracketType::NORMAL: {
            double h = ldata->bracket_height;
            double spatium = item->spatium();
            double w = item->style().style_absolute(Sid::BRACKET_WIDTH);
            double bd = (item->style().style_st(Sid::MUSICAL_SYMBOL_FONT) == "Leland") ? spatium * .5 : spatium * .25;
            Pen pen(item->cur_color(opt), w, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP);
            painter->set_pen(pen);
            painter->draw_line(LineF(0.0, -bd - w * .5, 0.0, h + bd + w * .5));
            double x = -w * .5;
            double y1 = -bd;
            double y2 = h + bd;
            item->draw_symbol(SymId::BRACKET_TOP, painter, PointF(x, y1));
            item->draw_symbol(SymId::BRACKET_BOTTOM, painter, PointF(x, y2));
        } break;
        case BracketType::SQUARE: {
            double h = ldata->bracket_height;
            double line_w = item->style().style_absolute(Sid::STAFF_LINE_WIDTH);
            double bracket_width = ldata->bracket_width - line_w / 2;
            Pen pen(item->cur_color(opt), line_w, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP);
            painter->set_pen(pen);
            painter->draw_line(LineF(0.0, 0.0, 0.0, h));
            painter->draw_line(LineF(-line_w / 2, 0.0, line_w / 2 + bracket_width, 0.0));
            painter->draw_line(LineF(-line_w / 2, h, line_w / 2 + bracket_width, h));
        } break;
        case BracketType::LINE: {
            double h = ldata->bracket_height;
            double w = 0.67 * item->style().style_absolute(Sid::BRACKET_WIDTH);
            Pen pen(item->cur_color(opt), w, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP);
            painter->set_pen(pen);
            double bd = item->style().style_absolute(Sid::STAFF_LINE_WIDTH) * 0.5;
            painter->draw_line(LineF(0.0, -bd, 0.0, h + bd));
        } break;
        case BracketType::GROUP: {
            if (!item->bracket_item()->show_bracket() && (!item->score()->is_show_invisible() || opt.is_printing)) {
                return;
            }

            painter->save();
            set_mask(item, painter);

            double h = ldata->bracket_height;
            double line_w = item->style().style_absolute(Sid::GROUP_BRACKET_LINE_WIDTH);
            double hook_len = item->style().style_absolute(Sid::GROUP_BRACKET_HOOK_LEN);
            Color color = item->cur_color(item->visible() && item->bracket_item()->show_bracket(), opt);
            Pen pen(color, line_w, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP);
            painter->set_pen(pen);
            painter->draw_line(LineF(0.5 * line_w, 0.0, 0.5 * line_w, h));
            painter->draw_line(LineF(0.0, 0.0, hook_len, 0.0));
            painter->draw_line(LineF(0.0, h, hook_len, h));

            painter->restore();
        } break;
        case BracketType::NO_BRACKET: break;
    }
}

void TDraw::draw(const Breath *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    painter->set_pen(item->cur_color(opt));
    item->draw_symbol(item->sym_id(), painter);
}

void TDraw::draw(const ChordLine *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const ChordLine::LayoutData *ldata = item->ldata();
    IF_ASSERT_FAILED(ldata) { return; }

    painter->set_pen(Pen(item->cur_color(opt), item->style().style_absolute(Sid::CHORDLINE_THICKNESS) * item->mag(), PenStyle::SOLID_LINE));
    painter->set_brush(BrushStyle::NO_BRUSH);
    if (!item->is_wavy()) {
        painter->draw_path(ldata->path);
    } else {
        painter->save();
        painter->rotate((item->chord_line_type() == ChordLineType::FALL ? 1 : -1));
        item->draw_symbol(item->wave_sym(), painter);
        painter->restore();
    }
}

void TDraw::draw(const Clef *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const Clef::LayoutData *ldata = item->ldata();
    IF_ASSERT_FAILED(ldata) { return; }

    if (ldata->sym_id == SymId::NO_SYM ||
        (item->staff() && !const_cast<const Staff *>(item->staff())->staff_type(item->tick())->gen_clef())) {
        return;
    }

    painter->set_pen(item->cur_color(opt));
    item->draw_symbol(ldata->sym_id, painter);
}

void TDraw::draw(const Capo *item, Painter *painter, const PaintOptions &opt) { draw_text_base(item, painter, opt); }

void TDraw::draw(const DeadSlapped *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const DeadSlapped::LayoutData *ldata = item->ldata();
    IF_ASSERT_FAILED(ldata) { return; }

    painter->set_pen(PenStyle::NO_PEN);
    painter->set_brush(item->cur_color(opt));
    painter->draw_path(ldata->path1);
    painter->draw_path(ldata->path2);
}

void TDraw::draw(const Dynamic *item, Painter *painter, const PaintOptions &opt) { draw_text_base(item, painter, opt); }

void TDraw::draw(const Expression *item, Painter *painter, const PaintOptions &opt) { draw_text_base(item, painter, opt); }

void TDraw::draw(const Fermata *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    painter->set_pen(item->cur_color(opt));
    item->draw_symbol(item->sym_id(), painter);
}

void TDraw::draw(const FiguredBass *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const FiguredBass::LayoutData *ldata = item->ldata();
    // if not printing, draw duration line(s)
    if (!opt.is_printing && item->score()->show_unprintable()) {
        for (double len : ldata->line_lengths) {
            if (len > 0) {
                painter->set_pen(Pen(item->configuration()->invisible_color(), 3));
                painter->draw_line(0.0, -2, len, -2); // -2: 2 rast. un. above digits
            }
        }
    }

    if (item->items().size() < 1) {         // if not parseable into f.b. items
        draw_text_base(item, painter, opt); // draw as standard text
    } else {
        for (FiguredBassItem *fi : item->items()) { // if parseable into f.b. items
            painter->translate(fi->pos());          // draw each item in its proper position
            draw(fi, painter, opt);
            painter->translate(-fi->pos());
        }
    }
}

void TDraw::draw(const FiguredBassItem *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const FiguredBassItem::LayoutData *ldata = item->ldata();
    int font = 0;
    double spatium_value = item->spatium();
    // set font from general style
    Font f(FiguredBass::fb_fonts().at(font).family, Font::Type::TABLATURE);

    // (use the same font selection as used in layout() above)
    double m = item->style().style_d(Sid::FIGURED_BASS_FONT_SIZE) * item->spatium() / item->default_spatium();
    f.set_point_size_f(m);

    painter->set_font(f);
    painter->set_brush(BrushStyle::NO_BRUSH);
    Pen pen(item->figured_bass()->cur_color(opt), FiguredBass::fb_contline_thickness.to_absolute(spatium_value), PenStyle::SOLID_LINE,
            PenCapStyle::ROUND_CAP);
    painter->set_pen(pen);
    painter->draw_text(ldata->bbox(), muse::draw::ALIGN_LEFT | muse::draw::ALIGN_TOP, muse::draw::TEXT_DONT_CLIP, ldata->display_text);

    // continuation line
    double line_end_x = 0.0;
    if (item->cont_line() != FiguredBassItem::ContLine::NONE) {
        double line_start_x = ldata->text_width; // by default, line starts right after text
        if (line_start_x > 0.0) {
            line_start_x += FiguredBass::fb_contline_left_padding.to_absolute(spatium_value); // if some text, give some room after it
        }
        line_end_x = item->figured_bass()->ldata()->printed_line_length; // by default, line ends with item duration
        if (line_end_x - line_start_x < 1.0) {                           // if line length < 1 sp, ignore it
            line_end_x = 0.0;
        }

        // if extended cont.line and no closing parenthesis: look at next FB element
        if (item->cont_line() > FiguredBassItem::ContLine::SIMPLE && item->parenth5() == FiguredBassItem::Parenthesis::NONE) {
            FiguredBass *next_fb;
            // if there is a contiguous FB element
            if ((next_fb = item->figured_bass()->next_figured_bass()) != 0) {
                // retrieve the X position (in page coords) of a possible cont. line of nextFB
                // on the same line of 'this'
                PointF pg_pos = item->page_pos();
                double next_cont_page_x = next_fb->additional_cont_line_x(pg_pos.y());
                // if an additional cont. line has been found, extend up to its initial X coord
                if (next_cont_page_x > 0) {
                    line_end_x = next_cont_page_x - pg_pos.x() + FiguredBass::fb_contline_overlap.to_absolute(spatium_value);
                }
                // with a little bit of overlap
                else {
                    line_end_x = item->figured_bass()->ldata()->line_length(0); // if none found, draw to the duration end
                }
            }
        }
        // if some line, draw it
        if (line_end_x > 0.0) {
            double h = ldata->bbox().height() * FiguredBass::fb_contline_height;
            painter->draw_line(line_start_x, h, line_end_x - ldata->pos().x(), h);
        }
    }

    // closing cont.line parenthesis
    if (item->parenth5() != FiguredBassItem::Parenthesis::NONE) {
        int x = line_end_x > 0.0 ? line_end_x : ldata->text_width;
        painter->draw_text(RectF(x, 0, ldata->bbox().width(), ldata->bbox().height()), muse::draw::ALIGN_LEFT | muse::draw::ALIGN_TOP,
                           muse::draw::TEXT_DONT_CLIP,
                           Char(FiguredBass::fb_fonts().at(font).display_parenthesis[int(item->parenth5())].unicode()));
    }
}

void TDraw::draw(const Fingering *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const FretDiagram *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const FretDiagram::LayoutData *ldata = item->ldata();
    PointF translation = -PointF(ldata->string_dist * (item->strings() - 1), 0);
    if (item->orientation() == Orientation::HORIZONTAL) {
        painter->save();
        painter->rotate(-90);
        painter->translate(translation);
    }

    // Init pen and other values
    Pen pen(item->cur_color(opt));
    pen.set_cap_style(PenCapStyle::FLAT_CAP);
    painter->set_brush(Brush(Color(painter->pen().color())));

    // x2 is the x val of the rightmost string
    double x2 = (item->strings() - 1) * ldata->string_dist;

    // Draw the nut
    pen.set_width_f(ldata->nut_line_width);
    painter->set_pen(pen);
    double nut_y = ldata->nut_y;
    painter->draw_line(LineF(-ldata->string_line_width * .5, nut_y, x2 + ldata->string_line_width * .5, nut_y));

    // Draw strings and frets
    pen.set_width_f(ldata->string_line_width);
    painter->set_pen(pen);

    // y2 is the y val of the bottom fretline
    double y1 = ldata->string_extend_top;
    double y2 = ldata->fret_dist * item->frets() + 0.5 * ldata->string_line_width + ldata->string_extend_bottom;
    for (int i = 0; i < item->strings(); ++i) {
        double x = ldata->string_dist * i;
        painter->draw_line(LineF(x, y1, x, y2));
    }
    for (int i = 1; i <= item->frets(); ++i) {
        double y = ldata->fret_dist * i;
        painter->draw_line(LineF(0.0, y, x2, y));
    }

    // dotd is the diameter of a dot
    double dotd = ldata->dot_diameter;

    // Draw dots, sym pen is used to draw them (and markers)
    Pen sym_pen(pen);
    sym_pen.set_cap_style(PenCapStyle::FLAT_CAP);
    // Raise the miter limit so the triangle's apex isn't bevelled (needs ~2.24)
    sym_pen.set_join_style(PenJoinStyle::MITER_JOIN);
    sym_pen.set_miter_limit(3.0);
    double sym_pen_width = ldata->string_line_width * 1.2;
    sym_pen.set_width_f(sym_pen_width);

    for (auto const &i : item->dots()) {
        for (auto const &d : i.second) {
            if (!d.exists()) {
                continue;
            }

            int string = i.first;
            int fret = d.fret - 1;

            // Calculate coords of the top left corner of the dot
            double x = ldata->string_dist * string - dotd * .5;
            double y = ldata->fret_dist * fret + ldata->fret_dist * .5 - dotd * .5;

            // Draw different symbols
            painter->set_pen(sym_pen);
            switch (d.dtype) {
                case FretDotType::CROSS:
                    // Give the cross a slightly larger width
                    sym_pen.set_width_f(sym_pen_width * 1.5);
                    painter->set_pen(sym_pen);
                    painter->draw_line(LineF(x, y, x + dotd, y + dotd));
                    painter->draw_line(LineF(x + dotd, y, x, y + dotd));
                    sym_pen.set_width_f(sym_pen_width);
                    break;
                case FretDotType::SQUARE:
                    painter->set_brush(BrushStyle::NO_BRUSH);
                    painter->draw_rect(RectF(x, y, dotd, dotd));
                    break;
                case FretDotType::TRIANGLE:
                    painter->set_brush(BrushStyle::NO_BRUSH);
                    painter->draw_polygon(PolygonF({PointF(x, y + dotd), PointF(x + .5 * dotd, y), PointF(x + dotd, y + dotd)}));
                    break;
                case FretDotType::NORMAL:
                default:
                    painter->set_brush(sym_pen.color());
                    painter->set_no_pen();
                    painter->draw_ellipse(RectF(x, y, dotd, dotd));
                    break;
            }
        }
    }

    // Draw markers
    sym_pen.set_width_f(ldata->string_line_width * 1.2);
    painter->set_brush(BrushStyle::NO_BRUSH);
    painter->set_pen(sym_pen);
    for (auto const &i : item->markers()) {
        int string = i.first;
        FretItem::Marker marker = i.second;
        if (!marker.exists()) {
            continue;
        }

        double x = ldata->string_dist * string - ldata->marker_size * .5;
        double y = ldata->marker_y;
        if (marker.mtype == FretMarkerType::CIRCLE) {
            painter->draw_ellipse(RectF(x, y, ldata->marker_size, ldata->marker_size));
        } else if (marker.mtype == FretMarkerType::CROSS) {
            painter->draw_line(PointF(x, y), PointF(x + ldata->marker_size, y + ldata->marker_size));
            painter->draw_line(PointF(x, y + ldata->marker_size), PointF(x + ldata->marker_size, y));
        }
    }

    // Draw barres
    for (auto const &i : item->barres()) {
        int fret = i.first;
        int start_string = i.second.start_string;
        int end_string = i.second.end_string;

        double x1 = ldata->string_dist * start_string;
        double new_x2 = end_string == -1 ? x2 : ldata->string_dist * end_string;
        double y = ldata->fret_dist * (fret - 1) + ldata->fret_dist * .5;
        if (item->style().style_b(Sid::BARRE_APPEARANCE_SLUR)) {
            pen.set_width_f(0.25 * ldata->string_line_width);
            pen.set_cap_style(PenCapStyle::ROUND_CAP);
            pen.set_join_style(PenJoinStyle::ROUND_JOIN);
            painter->set_pen(pen);
            painter->set_brush(Brush(pen.color()));
            for (const PainterPath &path : ldata->slur_paths) {
                painter->draw_path(path);
            }
        } else {
            pen.set_width_f(dotd * item->style().style_d(Sid::BARRE_LINE_WIDTH));
            pen.set_cap_style(PenCapStyle::ROUND_CAP);
            painter->set_pen(pen);
            painter->draw_line(LineF(x1, y, new_x2, y));
        }
    }

    // Draw fret offset number
    if (item->fret_offset() > 0) {
        Font scaled_font(item->fret_num_font());
        scaled_font.set_point_size_f(scaled_font.point_size_f() * (item->spatium() / item->default_spatium()));
        painter->set_font(scaled_font);
        String text = ldata->fret_text;

        if (item->orientation() == Orientation::VERTICAL) {
            if (item->num_pos() == 0) {
                painter->draw_text(RectF(-ldata->fret_num_padding, .0, .0, ldata->fret_dist),
                                   muse::draw::ALIGN_V_CENTER | muse::draw::ALIGN_RIGHT, muse::draw::TEXT_DONT_CLIP, text);
            } else {
                painter->draw_text(RectF(x2 + ldata->fret_num_padding, .0, .0, ldata->fret_dist),
                                   muse::draw::ALIGN_V_CENTER | muse::draw::ALIGN_LEFT, muse::draw::TEXT_DONT_CLIP, text);
            }
        } else if (item->orientation() == Orientation::HORIZONTAL) {
            painter->save();
            painter->translate(-translation);
            painter->rotate(90);
            if (item->num_pos() == 0) {
                painter->draw_text(RectF(.0, ldata->string_dist * (item->strings() - 1), .0, .0), muse::draw::ALIGN_LEFT,
                                   muse::draw::TEXT_DONT_CLIP, text);
            } else {
                painter->draw_text(RectF(.0, .0, .0, .0), muse::draw::ALIGN_BOTTOM | muse::draw::ALIGN_LEFT, muse::draw::TEXT_DONT_CLIP,
                                   text);
            }
            painter->restore();
        }
        painter->set_font(item->fret_num_font());
    }

    for (const FretDiagram::FingeringItem &fingering_item : item->ldata()->fingering_items) {
        painter->save();

        Font scaled_font(item->fingering_font());
        scaled_font.set_point_size_f(scaled_font.point_size_f() * (item->spatium() / item->default_spatium()));
        painter->set_font(scaled_font);
        if (item->orientation() == Orientation::HORIZONTAL) {
            painter->translate(-translation);
            painter->rotate(90);
        }

        painter->draw_text(fingering_item.pos, fingering_item.finger_number);

        painter->restore();
    }

    if (item->orientation() == Orientation::HORIZONTAL) {
        painter->restore();
    }
}

static void set_dash_and_gap_len(const SLine *line, double &dash, double &gap, Pen &pen) {
    static constexpr double dotted_dash_len = 0.01;
    static constexpr double dotted_gap_len = 1.99;
    switch (line->line_style()) {
        case LineType::SOLID: break;
        case LineType::DASHED: dash = line->dash_line_len(), gap = line->dash_gap_len(); break;
        case LineType::DOTTED:
            dash = dotted_dash_len, gap = dotted_gap_len;
            pen.set_cap_style(PenCapStyle::ROUND_CAP); // round dots
            break;
    }
}

static std::vector<double> distributed_dash_pattern(double dash, double gap, double line_length) {
    int num_pairs = std::max(1.0, line_length / (dash + gap));
    double new_gap = (line_length - dash * (num_pairs + 1)) / num_pairs;

    return {dash, new_gap};
}

void TDraw::draw(const GlissandoSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (item->pos2().x() <= 0) {
        return;
    }

    painter->save();
    double spatium_value = item->spatium();
    const Glissando *glissando = item->glissando();

    Pen pen(item->cur_color(item->get_property(Pid::VISIBLE).to_bool(), item->get_property(Pid::COLOR).value<Color>(), opt));
    pen.set_width_f(item->absolute_from_spatium(item->line_width()));
    pen.set_cap_style(PenCapStyle::FLAT_CAP);
    painter->set_pen(pen);

    // rotate painter so that the line become horizontal
    double w = item->pos2().x();
    double h = item->pos2().y();
    double l = sqrt(w * w + h * h);
    double wi = asin(-h / l) * 180.0 / M_PI;
    painter->rotate(-wi);

    if (glissando->glissando_type() == GlissandoType::STRAIGHT) {
        const bool is_non_solid = glissando->line_style() != LineType::SOLID;
        if (is_non_solid) {
            double line_width = glissando->absolute_from_spatium(glissando->line_width());
            double dash = 0;
            double gap = 0;
            set_dash_and_gap_len(glissando, dash, gap, pen);
            pen.set_dash_pattern(distributed_dash_pattern(dash, gap, l / line_width));
            painter->set_pen(pen);
        }

        painter->draw_line(LineF(0.0, 0.0, l, 0.0));
    } else if (glissando->glissando_type() == GlissandoType::WAVY) {
        RectF b = item->sym_bbox(SymId::WIGGLE_GLISSANDO);
        double a = item->sym_advance(SymId::WIGGLE_GLISSANDO);
        int n = static_cast<int>(l / a); // always round down (truncate) to avoid overlap
        double x = (l - n * a) * 0.5;    // centre line in available space
        SymIdList ids;
        for (int i = 0; i < n; ++i) {
            ids.push_back(SymId::WIGGLE_GLISSANDO);
        }

        item->score()->engraving_font()->draw(ids, painter, item->mag_s(), PointF(x, -(b.y() + b.height() * 0.5)));
    }

    if (glissando->show_text()) {
        Font f(glissando->font_face(), Font::Type::UNKNOWN);
        f.set_point_size_f(glissando->font_size() * spatium_value / item->default_spatium());
        f.set_bold(glissando->font_style() & FontStyle::BOLD);
        f.set_italic(glissando->font_style() & FontStyle::ITALIC);
        f.set_underline(glissando->font_style() & FontStyle::UNDERLINE);
        f.set_strike(glissando->font_style() & FontStyle::STRIKE);
        FontMetrics fm(f);
        RectF r = fm.bounding_rect(glissando->text());

        // if text longer than available space, skip it
        if (r.width() < l) {
            double x = (l - r.width()) * 0.5;
            double y_offset = r.height() + r.y(); // find text descender height
            // raise text slightly above line and slightly more with WAVY than with STRAIGHT
            y_offset += spatium_value * (glissando->glissando_type() == GlissandoType::WAVY ? 0.4 : 0.1);

            painter->set_font(f);
            painter->draw_text(PointF(x, -y_offset), glissando->text());
        }
    }
    painter->restore();
}

void TDraw::draw(const GuitarBendSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    bool tab_staff = item->staff()->is_tab_staff(item->tick());
    GuitarBend *bend = item->guitar_bend();

    Pen pen(item->cur_color(opt));
    if (bend->bend_type() == GuitarBendType::SCOOP) {
        painter->set_pen(pen);
        item->draw_symbol(SymId::GUITAR_VIBRATO_BAR_SCOOP, painter);
        return;
    }

    pen.set_width_f(item->line_width());
    pen.set_cap_style(PenCapStyle::FLAT_CAP);
    pen.set_join_style(bend->bend_type() == GuitarBendType::DIP ? PenJoinStyle::BEVEL_JOIN : PenJoinStyle::MITER_JOIN);
    pen.set_color(item->cur_color(opt));
    if (tab_staff && bend->bend_type() == GuitarBendType::PRE_DIVE) {
        const PainterPath &path = item->ldata()->path();
        if (path.element_count() > 1) {
            double line_length = std::abs(item->ldata()->path().element_at(1).y);
            pen.set_dash_pattern(distributed_dash_pattern(3, 3, line_length / pen.width_f()));
        } else {
            pen.set_dash_pattern({3, 3});
        }
    }
    painter->set_pen(pen);

    Brush brush;
    brush.set_style(BrushStyle::NO_BRUSH);
    painter->set_brush(brush);

    painter->draw_path(item->ldata()->path());

    if (tab_staff) {
        brush.set_style(BrushStyle::SOLID_PATTERN);
        brush.set_color(item->cur_color(opt));
        painter->set_brush(brush);
        painter->set_no_pen();
        painter->draw_polygon(item->ldata()->arrow());
    }
}

void TDraw::draw(const GuitarBendHoldSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    Pen pen(item->cur_color(item->visible(), opt));
    if (!item->ldata()->sym_ids().empty()) {
        painter->set_pen(pen);
        item->draw_symbols(item->ldata()->sym_ids(), painter);
        return;
    }

    pen.set_width_f(item->line_width());
    pen.set_cap_style(PenCapStyle::FLAT_CAP);

    switch (item->get_property(Pid::LINE_STYLE).value<LineType>()) {
        case LineType::DASHED: {
            double dash = item->dash_length();
            pen.set_dash_pattern(distributed_dash_pattern(dash, dash, item->pos2().x() / pen.width_f()));
            break;
        }
        case LineType::DOTTED:
            pen.set_cap_style(PenCapStyle::ROUND_CAP); // True dots
            pen.set_dash_pattern({0.01, 1.99});
            break;
        default: break;
    }

    pen.set_join_style(PenJoinStyle::MITER_JOIN);
    painter->set_pen(pen);

    painter->draw_line(PointF(), item->pos2());
}

void TDraw::draw_text_base(const TextBase *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    painter->save();
    painter->rotate(item->text_angle());

    const TextBase::LayoutData *ldata = item->ldata();
    if (item->has_frame()) {
        double base_spatium = DefaultStyle::base_style().value(Sid::SPATIUM).to_real();
        if (!real_is_null(item->frame_width().val())) {
            Color f_color = item->cur_color(item->visible(), item->frame_color(), opt);
            double frame_width_val = item->frame_width().val() * (item->size_is_spatium_dependent() ? item->spatium() : base_spatium);

            Pen pen(f_color, frame_width_val, PenStyle::SOLID_LINE, PenCapStyle::SQUARE_CAP, PenJoinStyle::MITER_JOIN);
            painter->set_pen(pen);
        } else {
            painter->set_no_pen();
        }
        Color bg(item->bg_color());
        painter->set_brush(bg.alpha() ? Brush(bg) : BrushStyle::NO_BRUSH);
        if (item->circle()) {
            painter->draw_ellipse(ldata->frame);
        } else {
            double frame_radius = item->frame_round().val() * (item->size_is_spatium_dependent() ? item->spatium() : base_spatium);
            painter->draw_rounded_rect(ldata->frame, frame_radius, frame_radius);
        }
    }
    painter->set_brush(BrushStyle::NO_BRUSH);
    painter->set_pen(item->text_color(opt));
    for (const TextBlock &t : ldata->blocks) {
        draw(t, item, painter);
    }

    painter->restore();
}

void TDraw::draw(const TextBlock &text_block, const TextBase *item, Painter *painter) {
    painter->translate(0.0, text_block.y());
    for (const TextFragment &f : text_block.fragments()) {
        draw(f, item, painter);
    }
    painter->translate(0.0, -text_block.y());
}

void TDraw::draw(const TextFragment &text_fragment, const TextBase *item, muse::draw::Painter *painter) {
    painter->set_font(text_fragment.font(item));
    painter->draw_text(text_fragment.pos, text_fragment.text);
}

void TDraw::draw_text_line_base_segment(const TextLineBaseSegment *item, Painter *painter, const PaintOptions &opt) {
    const TextLineBase *tl = item->text_line_base();
    const TextLineBaseSegment::LayoutData *ldata = item->ldata();

    if (!item->text()->empty()) {
        painter->translate(item->text()->pos());
        item->text()->set_visible(tl->visible());
        draw(item->text(), painter, opt);
        painter->translate(-item->text()->pos());
    }

    if (!item->end_text()->empty()) {
        painter->translate(item->end_text()->pos());
        item->end_text()->set_visible(tl->visible());
        draw(item->end_text(), painter, opt);
        painter->translate(-item->end_text()->pos());
    }

    if (ldata->npoints == 0 || ((opt.is_printing || (item->score() && !item->score()->is_show_invisible())) && !tl->line_visible())) {
        return;
    }

    // color for line (text color comes from the text properties)
    Color color = item->cur_color(tl->visible() && tl->line_visible(), tl->line_color(), opt);

    double line_width = tl->absolute_from_spatium(tl->line_width());

    const Pen solid_pen(color, line_width, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP, PenJoinStyle::MITER_JOIN);
    Pen pen(solid_pen);

    double dash = 0;
    double gap = 0;

    set_dash_and_gap_len(tl, dash, gap, pen);

    const bool is_non_solid = tl->line_style() != LineType::SOLID;

    // Draw lines
    if (item->two_lines()) { // hairpins
        if (is_non_solid) {
            pen.set_dash_pattern({dash, gap});
        }

        pen.set_join_style(PenJoinStyle::BEVEL_JOIN);
        painter->set_pen(pen);
        if (!ldata->joined_hairpin.empty() && !is_non_solid) {
            painter->draw_polyline(ldata->joined_hairpin);
        } else {
            painter->draw_lines(&ldata->points[0], 2);
        }
        return;
    }

    int start = 0, end = ldata->npoints;

    // Draw begin hook, if it needs to be drawn separately
    if (item->is_single_begin_type() && tl->begin_hook_type() != HookType::NONE) {
        if (tl->begin_hook_type() == HookType::ARROW_FILLED) {
            Brush brush;
            brush.set_style(BrushStyle::SOLID_PATTERN);
            brush.set_color(color);
            painter->set_brush(brush);
            painter->set_no_pen();
            painter->draw_polygon(ldata->begin_arrow);
        } else if (tl->begin_hook_type() == HookType::ARROW) {
            pen.set_join_style(PenJoinStyle::MITER_JOIN);
            painter->set_pen(pen);
            painter->draw_polyline(ldata->begin_arrow);
        } else {
            bool is_t_hook = tl->begin_hook_type() == HookType::HOOK_90T;

            if (is_non_solid || is_t_hook) {
                const PointF &p1 = ldata->points[start++];
                const PointF &p2 = ldata->points[start++];

                if (is_t_hook) {
                    painter->set_pen(solid_pen);
                } else {
                    double hook_length = sqrt(PointF::dot_product(p2 - p1, p2 - p1));
                    pen.set_dash_pattern(distributed_dash_pattern(dash, gap, hook_length / line_width));
                    painter->set_pen(pen);
                }

                painter->draw_line(p1, p2);
            }
        }
    }

    // Draw end hook, if it needs to be drawn separately
    if (item->is_single_end_type() && tl->end_hook_type() != HookType::NONE) {
        if (tl->end_hook_type() == HookType::ARROW_FILLED) {
            Brush brush;
            brush.set_style(BrushStyle::SOLID_PATTERN);
            brush.set_color(color);
            painter->set_brush(brush);
            painter->set_no_pen();
            painter->draw_polygon(ldata->end_arrow);
        } else if (tl->end_hook_type() == HookType::ARROW) {
            pen.set_join_style(PenJoinStyle::MITER_JOIN);
            painter->set_pen(pen);
            painter->draw_polyline(ldata->end_arrow);
        } else {
            bool is_t_hook = tl->end_hook_type() == HookType::HOOK_90T;

            if (is_non_solid || is_t_hook) {
                const PointF &p1 = ldata->points[--end];
                const PointF &p2 = ldata->points[--end];

                if (is_t_hook) {
                    painter->set_pen(solid_pen);
                } else {
                    double hook_length = sqrt(PointF::dot_product(p2 - p1, p2 - p1));
                    pen.set_dash_pattern(distributed_dash_pattern(dash, gap, hook_length / line_width));
                    painter->set_pen(pen);
                }

                painter->draw_line(p1, p2);
            }
        }
    }

    // Draw the rest
    if (is_non_solid) {
        pen.set_dash_pattern(distributed_dash_pattern(dash, gap, ldata->line_length / line_width));
    }

    painter->set_pen(pen);
    painter->draw_polyline(&ldata->points[start], end - start);
}

void TDraw::draw(const GradualTempoChangeSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const HairpinSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    draw_text_line_base_segment(item, painter, opt);

    if (item->draw_circled_tip()) {
        Color color = item->cur_color(item->hairpin()->visible(), item->hairpin()->line_color(), opt);
        double w = item->absolute_from_spatium(item->line_width());
        if (item->staff()) {
            w *= item->staff()->staff_mag(item->hairpin()->tick());
        }

        Pen pen(color, w);
        painter->set_pen(pen);
        painter->set_brush(BrushStyle::NO_BRUSH);
        painter->draw_ellipse(item->circled_tip(), item->circled_tip_radius(), item->circled_tip_radius());
    }
}

void TDraw::draw(const HammerOnPullOffSegment *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    draw(to_slur_segment(item), painter, opt);
}

void TDraw::draw(const HammerOnPullOffText *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const HarpPedalDiagram *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const HarmonicMarkSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const Harmony *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const Harmony::LayoutData *ldata = item->ldata();

    if (ldata->render_item_list().empty()) {
        draw_text_base(item, painter, opt);
        return;
    }

    if (item->has_frame()) {
        if (!real_is_null(item->frame_width().val())) {
            Color color = item->frame_color();
            Pen pen(color, item->frame_width().val() * item->spatium(), PenStyle::SOLID_LINE, PenCapStyle::SQUARE_CAP,
                    PenJoinStyle::MITER_JOIN);
            painter->set_pen(pen);
        } else {
            painter->set_no_pen();
        }
        Color bg(item->bg_color());
        painter->set_brush(bg.alpha() ? Brush(bg) : BrushStyle::NO_BRUSH);
        if (item->circle()) {
            painter->draw_arc(ldata->frame, 0, 5760);
        } else {
            double base_spatium = DefaultStyle::base_style().value(Sid::SPATIUM).to_real();
            double frame_radius = item->frame_round().val() * (item->size_is_spatium_dependent() ? item->spatium() : base_spatium);
            painter->draw_rounded_rect(ldata->frame, frame_radius, frame_radius);
        }
    }
    painter->set_brush(BrushStyle::NO_BRUSH);
    Color color = item->text_color(opt);
    painter->set_pen(color);
    for (const HarmonyRenderItem *render_item : ldata->render_item_list()) {
        if (const TextSegment *ts = dynamic_cast<const TextSegment *>(render_item)) {
            painter->set_font(ts->font());
            painter->draw_text(ts->pos(), ts->text());
        } else if (const ChordSymbolParen *paren_item = dynamic_cast<const ChordSymbolParen *>(render_item)) {
            Parenthesis *p = paren_item->paren_item;
            painter->translate(paren_item->pos());
            draw(p, painter, opt);
            painter->translate(-paren_item->pos());
        }
    }

    if (item->is_polychord()) {
        Pen pen(painter->pen());
        pen.set_width_f(item->absolute_from_spatium(item->style().style_s(Sid::POLYCHORD_DIVIDER_THICKNESS)));
        pen.set_color(color);
        painter->set_pen(pen);
        for (const LineF &line : ldata->polychord_divider_lines()) {
            painter->draw_line(line.translated(PointF(0.0, ldata->polychord_divider_offset)));
        }
    }
}

void TDraw::draw(const Hook *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    // hide if belonging to the second chord of a cross-measure pair
    if (item->chord() && item->chord()->cross_measure() == CrossMeasure::SECOND) {
        return;
    }

    painter->set_pen(item->cur_color(opt));
    item->draw_symbol(item->sym(), painter);
}

void TDraw::draw(const Image *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const Image::LayoutData *ldata = item->ldata();
    bool empty_image = false;
    if (item->image_type() == ImageType::SVG) {
        if (!item->svg_renderer()) {
            empty_image = true;
        } else {
            item->svg_renderer()->render(painter, ldata->bbox());
        }
    } else if (item->image_type() == ImageType::RASTER) {
        if (item->raster_image() == nullptr) {
            empty_image = true;
        } else {
            painter->save();
            SizeF s;
            if (item->size_is_spatium()) {
                s = item->size() * item->spatium();
            } else {
                s = item->size() * ::mu::engraving::dpmm;
            }
            painter->scale(s.width() / item->raster_image()->width(), s.height() / item->raster_image()->height());
            painter->draw_pixmap(PointF(0, 0), *item->raster_image());
            painter->restore();
        }
    }

    if (empty_image) {
        painter->set_brush(BrushStyle::NO_BRUSH);
        painter->set_pen(item->configuration()->default_color());
        painter->draw_rect(ldata->bbox());
        painter->draw_line(0.0, 0.0, ldata->bbox().width(), ldata->bbox().height());
        painter->draw_line(ldata->bbox().width(), 0.0, 0.0, ldata->bbox().height());
    }
    if (item->selected() && !opt.is_printing) {
        painter->set_brush(BrushStyle::NO_BRUSH);
        painter->set_pen(item->configuration()->selection_color());
        painter->draw_rect(ldata->bbox());
    }
}

void TDraw::draw(const InstrumentChange *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const InstrumentName *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const Jump *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const KeySig *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const KeySig::LayoutData *ldata = item->ldata();

    painter->set_pen(item->cur_color(opt));
    double spatium_value = item->spatium();
    double step = spatium_value * (item->staff() ? item->staff()->staff_type_for_element(item)->line_distance().val() * 0.5 : 0.5);
    int lines = item->staff() ? item->staff()->staff_type_for_element(item)->lines() : 5;
    double ledger_line_width = item->style().style_absolute(Sid::LEDGER_LINE_WIDTH) * item->mag();
    double ledger_extra_len = item->style().style_s(Sid::LEDGER_LINE_LENGTH).val() * spatium_value;
    for (const KeySym &ks : ldata->key_symbols) {
        double x = ks.x_pos.to_absolute(spatium_value);
        double y = ks.line * step;
        item->draw_symbol(ks.sym, painter, PointF(x, y));
        // ledger lines
        double sym_width = item->sym_width(ks.sym);
        double x1 = x - ledger_extra_len;
        double x2 = x + sym_width + ledger_extra_len;
        painter->set_pen(Pen(item->cur_color(opt), ledger_line_width, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
        for (int i = -2; i >= ks.line; i -= 2) { // above
            y = i * step;
            painter->draw_line(LineF(x1, y, x2, y));
        }
        for (int i = lines * 2; i <= ks.line; i += 2) { // below
            y = i * step;
            painter->draw_line(LineF(x1, y, x2, y));
        }
    }
}

void TDraw::draw(const LaissezVibSegment *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    const LaissezVibSegment::LayoutData *ldata = item->ldata();
    if (item->score()->style().style_b(Sid::LAISSEZ_VIB_USE_SMUFL_SYM)) {
        painter->set_pen(item->cur_color(opt));
        item->draw_symbol(ldata->symbol, painter);
    } else {
        draw(static_cast<const TieSegment *>(item), painter, opt);
    }
}

void TDraw::draw(const LayoutBreak *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (opt.is_printing || !item->score()->show_unprintable()) {
        return;
    }

    Color selection_color =
        opt.invert_colors ? item->configuration()->indicator_icon_inverted_selection_color() : item->configuration()->selection_color();
    Color color = item->selected() ? selection_color : item->configuration()->formatting_color();
    Pen pen(color);
    painter->set_pen(pen);
    painter->set_font(item->font());
    painter->draw_symbol(PointF(), item->icon_code());
}

void TDraw::draw(const LedgerLine *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (item->chord()->cross_measure() == CrossMeasure::SECOND) {
        return;
    }

    const LedgerLine::LayoutData *ldata = item->ldata();

    painter->set_pen(Pen(item->cur_color(opt), ldata->line_width, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
    if (item->vertical()) {
        painter->draw_line(LineF(0.0, 0.0, 0.0, item->len()));
    } else {
        painter->draw_line(LineF(0.0, 0.0, item->len(), 0.0));
    }
}

void TDraw::draw(const LetRingSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const Lyrics *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const LyricsLineSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    Pen pen(item->cur_color(opt));
    pen.set_width_f(item->absolute_from_spatium(item->line_width()));
    pen.set_cap_style(PenCapStyle::FLAT_CAP);
    painter->set_pen(pen);
    for (const LineF &dash : item->ldata()->dashes()) {
        painter->draw_line(dash);
    }
}

void TDraw::draw(const Marker *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const MeasureNumber *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const MeasureRepeat *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const MeasureRepeat::LayoutData *ldata = item->ldata();

    painter->set_pen(item->cur_color(opt));
    item->draw_symbol(ldata->sym_id, painter);

    if (!ldata->number_sym.empty()) {
        PointF number_pos = item->number_position(item->sym_bbox(ldata->number_sym));
        item->draw_symbols(ldata->number_sym, painter, number_pos);
    }

    if (item->style().style_b(Sid::FOUR_MEASURE_REPEAT_SHOW_EXTENDERS) && item->num_measures() == 4) {
        double h_bar_thickness = item->style().style_absolute(Sid::MM_REST_H_BAR_THICKNESS);
        Pen pen(painter->pen());
        pen.set_cap_style(PenCapStyle::FLAT_CAP);
        pen.set_width_f(h_bar_thickness);
        painter->set_pen(pen);

        painter->draw_line(ldata->extender_line_left);
        painter->draw_line(ldata->extender_line_right);
    }
}

void TDraw::draw(const MMRest *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    if (item->should_not_be_drawn() || (item->track() % ::mu::engraving::voices)) { //only on voice 1
        return;
    }

    const MMRest::LayoutData *ldata = item->ldata();

    double spatium_value = item->spatium();

    // draw number
    painter->set_pen(item->cur_color(opt));
    RectF number_box = item->sym_bbox(ldata->number_sym);
    PointF number_pos = item->number_pos();
    if (item->show_number()) {
        item->draw_symbols(ldata->number_sym, painter, number_pos);
    }

    number_box.translate(number_pos);

    if (item->is_old_style()) {
        // draw rest symbols
        double x = (ldata->rest_width - ldata->syms_width) * 0.5;
        double spacing = item->style().style_absolute(Sid::MM_REST_OLD_STYLE_SPACING);
        for (SymId sym : ldata->rest_syms) {
            double y = (sym == SymId::REST_WHOLE ? -spatium_value : 0);
            item->draw_symbol(sym, painter, PointF(x, y));
            x += item->sym_bbox(sym).width() + spacing;
        }
    } else {
        double mag = item->staff()->staff_mag(item->tick());
        Pen pen(painter->pen());
        pen.set_cap_style(PenCapStyle::FLAT_CAP);

        // draw horizontal line
        double h_bar_thickness = item->style().style_absolute(Sid::MM_REST_H_BAR_THICKNESS) * mag;
        if (h_bar_thickness) { // don't draw at all if 0, QPainter interprets 0 pen width differently
            pen.set_width_f(h_bar_thickness);
            painter->set_pen(pen);
            double half_h_bar_thickness = h_bar_thickness * .5;
            if (item->show_number() // avoid painting line through number
                && item->style().style_b(Sid::MM_REST_NUMBER_MASK_H_BAR) && number_box.bottom() >= -half_h_bar_thickness &&
                number_box.top() <= half_h_bar_thickness) {
                double gap_distance = (number_box.width() + spatium_value) * .5;
                double midpoint = ldata->rest_width * .5;
                painter->draw_line(LineF(0.0, 0.0, midpoint - gap_distance, 0.0));
                painter->draw_line(LineF(midpoint + gap_distance, 0.0, ldata->rest_width, 0.0));
            } else {
                painter->draw_line(LineF(0.0, 0.0, ldata->rest_width, 0.0));
            }
        }

        // draw vertical lines
        double v_stroke_thickness = item->style().style_absolute(Sid::MM_REST_H_BAR_V_STROKE_THICKNESS) * mag;
        if (v_stroke_thickness) { // don't draw at all if 0, QPainter interprets 0 pen width differently
            pen.set_width_f(v_stroke_thickness);
            painter->set_pen(pen);
            double half_v_stroke_height = item->style().style_absolute(Sid::MM_REST_H_BAR_V_STROKE_HEIGHT) * .5 * mag;
            painter->draw_line(LineF(0.0, -half_v_stroke_height, 0.0, half_v_stroke_height));
            painter->draw_line(LineF(ldata->rest_width, -half_v_stroke_height, ldata->rest_width, half_v_stroke_height));
        }
    }
}

void TDraw::draw(const MMRestRange *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const Note *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    if (item->hidden()) {
        return;
    }

    const Note::LayoutData *ldata = item->ldata();

    const auto config = item->configuration();
    const StaffType *staff_type = item->staff() ? item->staff()->staff_type_for_element(item) : nullptr;

    const bool is_tab_staff = staff_type && staff_type->is_tab_staff();
    const bool negative_fret = is_tab_staff && item->negative_fret_used();
    const bool use_critical_color = negative_fret && !item->dead_note() && !opt.is_printing;

    painter->set_pen(use_critical_color ? config->critical_color() : item->cur_color(opt));

    // tablature
    if (is_tab_staff) {
        if (item->display_fret() == Note::DisplayFretOption::HIDE || item->should_hide_fret()) {
            return;
        }
        const Staff *st = item->staff();
        const StaffType *tab = st->staff_type_for_element(item);

        if (!opt.is_printing && item->score()->show_unprintable() && (negative_fret || item->fret_conflict())) { // fret conflict
            painter->save();
            painter->set_pen(config->critical_color());
            painter->set_brush(config->critical_background_color());
            painter->draw_rect(ldata->bbox());
            painter->restore();
        }

        Font f(tab->fret_font());
        f.set_point_size_f(f.point_size_f() * item->mag_s());
        painter->set_font(f);

        const double start_pos_x = ldata->bbox().x();
        const double y_offset = tab->fret_font_y_offset();
        painter->draw_text(PointF(start_pos_x, y_offset * item->mag_s()), item->fret_string());
    }
    // NOT tablature
    else {
        // skip drawing, if second note of a cross-measure value
        if (item->chord() && item->chord()->cross_measure() == CrossMeasure::SECOND) {
            return;
        }
        // warn if pitch extends usable range of instrument
        // by coloring the notehead
        if (item->chord() && item->chord()->segment() && item->staff() && !opt.is_printing && MScore::warn_pitch_range &&
            !item->staff()->is_drum_staff(item->chord()->tick())) {
            const Instrument *in = item->origin_part()->instrument(item->chord()->tick());
            int i = item->ppitch();
            if (i < in->min_pitch_p() || i > in->max_pitch_p()) {
                painter->set_pen(item->selected() ? config->critical_selected_color() : config->critical_color());
            } else if (i < in->min_pitch_a() || i > in->max_pitch_a()) {
                painter->set_pen(item->selected() ? config->warning_selected_color() : config->warning_color());
            }
        }
        // Warn if notes are unplayable based on previous harp diagram setting
        if (item->chord() && item->chord()->segment() && item->staff() && !opt.is_printing &&
            !item->staff()->is_drum_staff(item->chord()->tick())) {
            HarpPedalDiagram *prev_diagram = item->origin_part()->current_harp_diagram(item->chord()->segment()->tick());
            if (prev_diagram && !prev_diagram->is_tpc_playable(item->tpc())) {
                painter->set_pen(item->selected() ? config->critical_selected_color() : config->critical_color());
            }
        }
        // draw blank notehead to avoid staff and ledger lines
        if (ldata->cached_sym_null.value() != SymId::NO_SYM) {
            painter->save();
            painter->set_pen(config->note_background_color());
            item->draw_symbol(ldata->cached_sym_null.value(), painter);
            painter->restore();
        }
        item->draw_symbol(ldata->cached_notehead_sym.value(), painter);
    }
}

void TDraw::draw(const NoteDot *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    if (item->note() && item->note()->dots_hidden()) { // don't draw dot if note is hidden
        return;
    } else if (item->rest() && item->rest()->is_gap()) { // don't draw dot for gap rests
        return;
    }
    const Note *n = item->note();
    Fraction tick = n ? n->chord()->tick() : item->rest()->tick();
    // always draw dot for non-tab
    // for tab, draw if on a note and stems through staff or on a rest and rests shown
    if (!item->staff()->is_tab_staff(tick) || (n && item->staff()->staff_type(tick)->stem_through()) ||
        (!n && item->staff()->staff_type(tick)->show_rests())) {
        painter->set_pen(item->cur_color(opt));
        item->draw_symbol(SymId::AUGMENTATION_DOT, painter);
    }
}

void TDraw::draw(const NoteHead *item, Painter *painter, const PaintOptions &opt) { draw(static_cast<const Symbol *>(item), painter, opt); }

void TDraw::draw(const NoteLineSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const OttavaSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const PageLockIndicator *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (opt.is_printing || !item->score()->show_unprintable()) {
        return;
    }

    Color selection_color =
        opt.invert_colors ? item->configuration()->indicator_icon_inverted_selection_color() : item->configuration()->selection_color();
    Color color = item->selected() ? selection_color : item->configuration()->formatting_color();
    Pen pen(color);
    painter->set_pen(pen);
    painter->set_font(item->font());
    painter->draw_symbol(PointF(), item->icon_code());

    if (item->selected()) {
        Color locked_area_color = selection_color;
        locked_area_color.set_alpha(opt.invert_colors ? 90 : 38);
        Brush brush(locked_area_color);
        painter->set_brush(brush);
        painter->set_no_pen();
        double radius = 0.5 * item->spatium();

        PainterPath path;
        path.set_fill_rule(PainterPath::FillRule::ODD_EVEN_FILL);
        path.add_rounded_rect(item->ldata()->range_rect, radius, radius);
        path.add_rect(item->ldata()->inner_range_rect);
        painter->draw_path(path);
    }
}

void TDraw::draw(const Parenthesis *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    EngravingObject *parent = item->ownership_parent();
    TimeSig *parent_ts = parent && parent->is_time_sig() ? to_time_sig(parent) : nullptr;

    if (parent_ts && !parent_ts->show_on_this_staff()) {
        return;
    }

    Color pen_color = item->cur_color(opt);

    Pen pen(pen_color);

    if (item->ldata()->sym_id != SymId::NO_SYM) {
        painter->set_pen(pen);
        item->draw_symbol(item->ldata()->sym_id, painter, PointF(), item->ldata()->sym_scale);
        return;
    }

    painter->set_brush(Brush(pen.color()));
    pen.set_cap_style(PenCapStyle::ROUND_CAP);
    pen.set_join_style(PenJoinStyle::ROUND_JOIN);
    pen.set_width_f(item->ldata()->end_point_thickness * item->spatium() * item->ldata()->intrinsic_mag());

    painter->set_pen(pen);
    painter->draw_path(item->ldata()->path());
}

void TDraw::draw(const PartialTieSegment *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    draw(static_cast<const TieSegment *>(item), painter, opt);
}

void TDraw::draw(const PalmMuteSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const PedalSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const PickScrapeSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const PlayCountText *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const PlayTechAnnotation *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const RasgueadoSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const RehearsalMark *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const Rest *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (item->is_gap()) {
        draw_debug_gap_rest(item, painter);
        return;
    }

    if (item->should_not_be_drawn()) {
        return;
    }

    const Rest::LayoutData *ldata = item->ldata();

    painter->set_pen(item->cur_color(opt));

    if (DeadSlapped *ds = item->dead_slapped()) {
        draw(ds, painter, opt);
    } else {
        item->draw_symbol(ldata->sym, painter);
    }
}

void TDraw::draw_debug_gap_rest(const Rest *item, muse::draw::Painter *painter) {
    IF_ASSERT_FAILED(item->debug_draw_gap()) { return; }

    painter->set_pen(Color::red_color);
    item->draw_symbol(item->ldata()->sym, painter);
}

//! NOTE May be removed later (should be only single mode)
void TDraw::draw(const SlurSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    painter->save();
    set_mask(item, painter);

    Pen pen(item->cur_color(opt));
    double mag = item->staff() ? item->staff()->staff_mag(item->slur()->tick()) : 1.0;

    //Replace generic Qt dash patterns with improved equivalents to show true dots (keep in sync with tie.cpp)
    std::vector<double> dotted = {0.01, 1.99}; // tighter than Qt PenStyle::DotLine equivalent - would be { 0.01, 2.99 }
    std::vector<double> dashed = {3.00, 3.00}; // Compensating for caps. Qt default PenStyle::DashLine is { 4.0, 2.0 }
    std::vector<double> wide_dashed = {5.00, 6.00};

    switch (item->slur_tie()->style_type()) {
        case SlurStyleType::SOLID:
            painter->set_brush(Brush(pen.color()));
            pen.set_cap_style(PenCapStyle::ROUND_CAP);
            pen.set_join_style(PenJoinStyle::ROUND_JOIN);
            pen.set_width_f(item->end_width() * mag);
            break;
        case SlurStyleType::DOTTED:
            painter->set_brush(BrushStyle::NO_BRUSH);
            pen.set_cap_style(PenCapStyle::ROUND_CAP); // round dots
            pen.set_dash_pattern(dotted);
            pen.set_width_f(item->dotted_width() * mag);
            break;
        case SlurStyleType::DASHED:
            painter->set_brush(BrushStyle::NO_BRUSH);
            pen.set_dash_pattern(dashed);
            pen.set_width_f(item->dotted_width() * mag);
            break;
        case SlurStyleType::WIDE_DASHED:
            painter->set_brush(BrushStyle::NO_BRUSH);
            pen.set_dash_pattern(wide_dashed);
            pen.set_width_f(item->dotted_width() * mag);
            break;
        case SlurStyleType::UNDEFINED: break;
    }
    painter->set_pen(pen);
    painter->draw_path(item->ldata()->path());

    painter->restore();
}

void TDraw::draw(const Spacer *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    if (opt.is_printing || !item->score()->show_unprintable()) {
        return;
    }

    auto conf = item->configuration();

    Pen pen(item->selected() ? conf->selection_color() : conf->formatting_color(), item->spatium() * 0.3);

    painter->set_pen(pen);
    painter->set_brush(BrushStyle::NO_BRUSH);
    painter->draw_path(item->ldata()->path);
}

void TDraw::draw(const StaffLines *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    painter->save();

    set_mask(item, painter);

    painter->set_pen(Pen(item->cur_color(opt), item->lw(), PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
    painter->draw_lines(item->lines());

    painter->restore();
}

void TDraw::draw(const StaffState *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    if (opt.is_printing || !item->score()->show_unprintable()) {
        return;
    }

    const StaffState::LayoutData *ldata = item->ldata();
    auto conf = item->configuration();

    Pen pen(item->selected() ? conf->selection_color() : conf->formatting_color(), ldata->lw, PenStyle::SOLID_LINE, PenCapStyle::ROUND_CAP,
            PenJoinStyle::ROUND_JOIN);
    painter->set_pen(pen);
    painter->set_brush(BrushStyle::NO_BRUSH);
    painter->draw_path(ldata->path);
}

void TDraw::draw(const StaffText *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    draw_text_base(item, painter, opt);

    if (item->has_sound_flag()) {
        draw(item->sound_flag(), painter, opt);
    }
}

void TDraw::draw(const StaveSharingLabel *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    draw_text_base(item, painter, opt);
}

void TDraw::draw(const StaffTypeChange *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (opt.is_printing || !item->score()->show_unprintable()) {
        return;
    }

    auto conf = item->configuration();

    double spatium_value = item->style().spatium();
    double h = spatium_value * 2.5;
    double w = spatium_value * 2.5;
    double line_dist = 0.35; // line distance for the icon 'staff lines'
    // draw icon rectangle
    painter->set_pen(Pen(item->selected() ? conf->selection_color() : conf->formatting_color(), item->lw(), PenStyle::SOLID_LINE,
                         PenCapStyle::SQUARE_CAP, PenJoinStyle::MITER_JOIN));
    painter->set_brush(BrushStyle::NO_BRUSH);
    painter->draw_rect(0, 0, w, h);

    // draw icon contents
    int lines = 5;
    if (item->staff_type()) {
        if (item->staff_type()->stemless()) { // a single notehead represents a stemless staff
            item->draw_symbol(SymId::NOTEHEAD_BLACK, painter, PointF(w * 0.5 - 0.33 * spatium_value, h * 0.5), 0.5);
        }
        if (item->staff_type()->invisible()) { // no lines needed. It's done.
            return;
        }
        // show up to 6 lines
        lines = std::min(item->staff_type()->lines(), 6);
    }
    // calculate starting point Y for the lines from half the icon height (2.5) so staff lines appear vertically centered
    double start_y = 1.25 - (lines - 1) * line_dist * 0.5;
    painter->set_pen(Pen(item->selected() ? conf->selection_color() : conf->formatting_color(), 2.5, PenStyle::SOLID_LINE,
                         PenCapStyle::SQUARE_CAP, PenJoinStyle::MITER_JOIN));
    for (int i = 0; i < lines; i++) {
        int y = (start_y + i * line_dist) * spatium_value;
        painter->draw_line(0, y, w, y);
    }
}

void TDraw::draw(const Stem *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    if (!item->chord()) { // may be need assert?
        return;
    }

    // hide if second chord of a cross-measure pair
    if (item->chord()->cross_measure() == CrossMeasure::SECOND) {
        return;
    }

    const Stem::LayoutData *ldata = item->ldata();

    const Staff *staff = item->staff();
    const StaffType *staff_type = staff ? staff->staff_type_for_element(item->chord()) : nullptr;
    const bool is_tablature = staff_type && staff_type->is_tab_staff();

    painter->set_pen(Pen(item->cur_color(opt), item->line_width_mag(), PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
    painter->draw_line(ldata->line);

    if (!is_tablature) {
        return;
    }

    // TODO: adjust bounding rectangle in layout() for dots and for slash
    double sp = item->spatium();
    bool is_up = item->up();

    // slashed half note stem
    if (item->chord()->duration_type().type() == DurationType::V_HALF && staff_type->minim_style() == TablatureMinimStyle::SLASHED) {
        // position slashes onto stem
        double y = is_up ? -item->length() + StemLayout::stafftype_tab_slash_2_starty_up * sp :
                           item->length() - StemLayout::stafftype_tab_slash_2_starty_dn * sp;
        // if stems through, try to align slashes within or across lines
        if (staff_type->stem_through()) {
            double half_line_dist = staff_type->line_distance().val() * sp * 0.5;
            double half_slash_hgt = StemLayout::stafftype_tab_slash_2_totheight * sp * 0.5;
            y = lrint((y + half_slash_hgt) / half_line_dist) * half_line_dist - half_slash_hgt;
        }
        // draw slashes
        double hlf_wdt = sp * StemLayout::stafftype_tab_slash_width * 0.5;
        double sln = sp * StemLayout::stafftype_tab_slash_slanty;
        double thk = sp * StemLayout::stafftype_tab_slash_thick;
        double displ = sp * StemLayout::stafftype_tab_slash_displ;
        PainterPath path;
        for (int i = 0; i < 2; ++i) {
            path.move_to(hlf_wdt, y);              // top-right corner
            path.line_to(hlf_wdt, y + thk);        // bottom-right corner
            path.line_to(-hlf_wdt, y + thk + sln); // bottom-left corner
            path.line_to(-hlf_wdt, y + sln);       // top-left corner
            path.close_subpath();
            y += displ;
        }
        painter->set_brush(Brush(item->cur_color(opt)));
        painter->set_no_pen();
        painter->draw_path(path);
    }

    // dots
    // NOT THE BEST PLACE FOR THIS?
    // with tablatures and stems beside staves, dots are not drawn near 'notes', but near stems
    int n_dots = item->chord()->dots();
    if (n_dots > 0 && !staff_type->stem_through()) {
        double x = item->chord()->dot_pos_x();
        double y = ((StemLayout::stafftype_tab_defaultstemlen_dn * 0.2) * sp) * (is_up ? -1.0 : 1.0);
        double step = item->style().style_s(Sid::DOT_DOT_DISTANCE).val() * sp;
        for (int dot = 0; dot < n_dots; dot++, x += step) {
            item->draw_symbol(SymId::AUGMENTATION_DOT, painter, PointF(x, y));
        }
    }
}

void TDraw::draw(const StemSlash *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const StemSlash::LayoutData *ldata = item->ldata();
    painter->set_pen(Pen(item->cur_color(opt), ldata->stem_width, PenStyle::SOLID_LINE, PenCapStyle::FLAT_CAP));
    painter->draw_line(ldata->line);
}

void TDraw::draw(const Sticking *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const StringTunings *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (item->no_string_visible()) {
        const TextBase::LayoutData *data = item->ldata();

        const double spatium = item->spatium();
        const double line_width = spatium * .15;

        const Pen pen(item->cur_color(opt), line_width, PenStyle::SOLID_LINE, PenCapStyle::ROUND_CAP, PenJoinStyle::ROUND_JOIN);
        painter->set_pen(pen);
        painter->set_brush(Brush(item->cur_color(opt)));

        const RectF rect = data->bbox();

        const double x = rect.x();
        const double y = rect.y();
        const double width = rect.width();
        const double height = rect.height();
        const double top_part_height = height * .66;
        const double corner_radius = height * .1;

        PainterPath path;
        path.move_to(x, y);
        path.arc_to(x, y + (top_part_height - 2 * corner_radius), 2 * corner_radius, 2 * corner_radius, 180.0, 90.0);
        path.arc_to(x + width - 2 * corner_radius, y + (top_part_height - 2 * corner_radius), 2 * corner_radius, 2 * corner_radius, 270,
                    90);
        path.line_to(x + width, y);
        path.move_to(x + width / 2, y + top_part_height);
        path.line_to(x + width / 2, y + height);

        painter->set_brush(BrushStyle::NO_BRUSH);
        painter->draw_path(path);
    } else {
        draw_text_base(item, painter, opt);
    }
}

void TDraw::draw(const Symbol *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    bool tab_staff = item->staff() ? item->staff()->is_tab_staff(item->tick()) : false;

    if (!item->is_note_dot() || !tab_staff) {
        painter->set_pen(item->cur_color(opt));
        if (item->score_font()) {
            item->score_font()->draw(item->sym(), painter, item->mag_s() * item->symbols_size(), PointF(), item->sym_angle());
        } else {
            item->draw_symbol(item->sym(), painter);
        }
    }
}

void TDraw::draw(const FSymbol *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    painter->set_font(item->font());
    painter->set_pen(item->cur_color(opt));
    painter->draw_text(PointF(0, 0), item->to_string());
}

void TDraw::draw(const SystemDivider *item, Painter *painter, const PaintOptions &opt) {
    draw(static_cast<const Symbol *>(item), painter, opt);
}

void TDraw::draw(const SystemText *item, Painter *painter, const PaintOptions &opt) { draw_text_base(item, painter, opt); }

void TDraw::draw(const IndicatorIcon *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (opt.is_printing || !item->score()->show_unprintable()) {
        return;
    }

    Color selection_color =
        opt.invert_colors ? item->configuration()->indicator_icon_inverted_selection_color() : item->configuration()->selection_color();
    Color color = item->selected() ? selection_color : item->configuration()->formatting_color();
    Pen pen(color);
    painter->set_pen(pen);
    painter->set_font(item->font());
    painter->draw_symbol(PointF(), item->icon_code());

    if (item->is_system_lock_indicator() && item->selected()) {
        const SystemLockIndicator *sli = to_system_lock_indicator(item);

        Color locked_area_color = selection_color;
        locked_area_color.set_alpha(opt.invert_colors ? 90 : 38);
        Brush brush(locked_area_color);
        painter->set_brush(brush);
        painter->set_no_pen();
        double radius = 0.5 * sli->spatium();

        painter->draw_rounded_rect(sli->ldata()->range_rect, radius, radius);
    }
}

void TDraw::draw(const SoundFlag *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (item->should_hide()) {
        return;
    }

    painter->set_no_pen();
    painter->set_brush(item->icon_background_color());
    painter->draw_ellipse(item->ldata()->bbox().adjusted(-4.0, -4.0, 4.0, 4.0));

    painter->set_font(item->icon_font());
    painter->set_pen(!item->selected() ? item->cur_color(true, opt) : Color::white);
    painter->draw_text(item->ldata()->bbox(), muse::draw::ALIGN_CENTER, muse::draw::TEXT_DONT_CLIP, Char(item->icon_code()));
}

void TDraw::draw(const TabDurationSymbol *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (!item->tab()) {
        return;
    }

    const TabDurationSymbol::LayoutData *ldata = item->ldata();

    if (item->is_repeat() && (item->tab()->sym_repeat() == TablatureSymbolRepeat::SYSTEM)) {
        Chord *chord = to_chord(item->ownership_parent());
        ChordRest *prev_cr = Navigation::prev_chord_rest(chord);
        if (prev_cr && (chord->measure()->system() == prev_cr->measure()->system())) {
            return;
        }
    }

    double mag = item->mag_s();
    double imag = 1.0 / mag;

    Pen pen(item->cur_color(opt));
    painter->set_pen(pen);
    painter->scale(mag, mag);
    if (ldata->beam_grid == TabBeamGrid::NONE) {
        // if no beam grid, draw symbol
        painter->set_font(item->tab()->duration_font());
        painter->draw_text(PointF(0.0, 0.0), item->text());
    } else {
        // if beam grid, draw stem line
        const TablatureDurationFont &font = item->tab()->tab_duration_font();
        double spatium_value = item->spatium();
        pen.set_cap_style(PenCapStyle::FLAT_CAP);
        pen.set_width_f(font.grid_stem_width.to_absolute(spatium_value));
        painter->set_pen(pen);
        // take stem height from bbox, but de-magnify it, as drawing is already magnified
        double h = ldata->bbox().y() / mag;
        painter->draw_line(PointF(0.0, h), PointF(0.0, 0.0));
        // if beam grid is medial/final, draw beam lines too: lines go from mid of
        // previous stem (delta x stored in _beamLength) to mid of this' stem (0.0)
        if (ldata->beam_grid == TabBeamGrid::MEDIALFINAL) {
            pen.set_width_f(font.grid_beam_width.to_absolute(spatium_value));
            painter->set_pen(pen);
            // lower height available to beams by half a beam width,
            // so that top beam upper border aligns with stem top
            h += (font.grid_beam_width.to_absolute(spatium_value)) * 0.5;
            // draw beams equally spaced within the stem height (this is
            // different from modern engraving, but common in historic prints)
            double step = -h / ldata->beam_level;
            double y = h;
            for (int i = 0; i < ldata->beam_level; i++, y += step) {
                painter->draw_line(PointF(ldata->beam_length, y), PointF(0.0, y));
            }
        }
    }
    painter->scale(imag, imag);
}

void TDraw::draw(const Tapping *item, muse::draw::Painter *painter, const PaintOptions &opt) {
    painter->set_pen(item->cur_color(opt));
    if (item->ldata()->sym_id != SymId::NO_SYM) {
        item->draw_symbol(item->ldata()->sym_id, painter);
    } else if (TappingText *text = item->text()) {
        painter->translate(text->pos());
        draw_text_base(text, painter, opt);
        painter->translate(-text->pos());
    } else {
        assert(false && "Drawing Tapping item without text or symbol");
    }
}

void TDraw::draw(const TempoText *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const Text *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const TextLineSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const TieSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    // hide tie toward the second chord of a cross-measure value
    if (item->tie()->end_note() && item->tie()->end_note()->chord()->cross_measure() == CrossMeasure::SECOND) {
        return;
    }

    painter->save();

    set_mask(item, painter);

    Color pen_color = item->cur_color(item->get_property(Pid::VISIBLE).to_bool(), item->get_property(Pid::COLOR).value<Color>(), opt);
    if (!opt.is_printing && item->ldata()->all_jump_points_inactive) {
        pen_color.set_alpha(std::min(pen_color.alpha(), 85));
    }

    Pen pen(pen_color);
    double mag = item->staff() ? item->staff()->staff_mag(item->tie()->tick()) : 1.0;
    //Replace generic Qt dash patterns with improved equivalents to show true dots (keep in sync with slur.cpp)
    std::vector<double> dotted = {0.01, 1.99}; // tighter than Qt PenStyle::DotLine equivalent - would be { 0.01, 2.99 }
    std::vector<double> dashed = {3.00, 3.00}; // Compensating for caps. Qt default PenStyle::DashLine is { 4.0, 2.0 }
    std::vector<double> wide_dashed = {5.00, 6.00};

    switch (item->slur_tie()->style_type()) {
        case SlurStyleType::SOLID:
            painter->set_brush(Brush(pen.color()));
            pen.set_cap_style(PenCapStyle::ROUND_CAP);
            pen.set_join_style(PenJoinStyle::ROUND_JOIN);
            pen.set_width_f(item->end_width() * mag);
            break;
        case SlurStyleType::DOTTED:
            painter->set_brush(BrushStyle::NO_BRUSH);
            pen.set_cap_style(PenCapStyle::ROUND_CAP); // True dots
            pen.set_dash_pattern(dotted);
            pen.set_width_f(item->dotted_width() * mag);
            break;
        case SlurStyleType::DASHED:
            painter->set_brush(BrushStyle::NO_BRUSH);
            pen.set_dash_pattern(dashed);
            pen.set_width_f(item->dotted_width() * mag);
            break;
        case SlurStyleType::WIDE_DASHED:
            painter->set_brush(BrushStyle::NO_BRUSH);
            pen.set_dash_pattern(wide_dashed);
            pen.set_width_f(item->dotted_width() * mag);
            break;
        case SlurStyleType::UNDEFINED: break;
    }
    painter->set_pen(pen);
    painter->draw_path(item->ldata()->path());

    painter->restore();
}

void TDraw::draw(const TimeSig *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (item->staff() && !const_cast<const Staff *>(item->staff())->staff_type(item->tick())->gen_timesig()) {
        return;
    }
    if (!item->show_on_this_staff()) {
        return;
    }
    painter->set_pen(item->cur_color(opt));

    const TimeSig::LayoutData *ldata = item->ldata();

    item->draw_symbols(ldata->ns, painter, ldata->pz, item->scale());
    item->draw_symbols(ldata->ds, painter, ldata->pn, item->scale());

    if (item->large_parentheses()) {
        item->draw_symbol(SymId::TIME_SIG_PARENS_LEFT, painter, ldata->point_large_left_paren, item->scale().width());
        item->draw_symbol(SymId::TIME_SIG_PARENS_RIGHT, painter, ldata->point_large_right_paren, item->scale().width());
    }
}

void TDraw::draw(const TimeTickAnchor *item, Painter *painter, const PaintOptions &opt) {
    if (opt.is_printing) {
        return;
    }

    TimeTickAnchor::DrawRegion draw_region = item->draw_region();

    if (draw_region == TimeTickAnchor::DrawRegion::OUT_OF_RANGE) {
        return;
    }

    Color voice_color = item->configuration()->voice_color(item->voice_idx());

    static constexpr double tint_main_darker = 0.45;
    static constexpr double tint_main_lighter = 0.55;
    static constexpr double tint_extended_darker = 0.75;
    static constexpr double tint_extended_lighter = 0.85;

    double tint = draw_region == TimeTickAnchor::DrawRegion::MAIN_REGION ? item->ldata()->darker() ? tint_main_darker : tint_main_lighter :
                  item->ldata()->darker()                                ? tint_extended_darker :
                                                                           tint_extended_lighter;

    voice_color.apply_tint(tint);

    Brush brush;
    brush.set_color(voice_color);
    brush.set_style(BrushStyle::SOLID_PATTERN);
    painter->set_brush(brush);
    painter->set_no_pen();

    painter->draw_rect(item->ldata()->bbox());
}

void TDraw::draw(const TremoloSingleChord *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    if (item->is_buzz_roll()) {
        painter->set_pen(item->cur_color(opt));
        item->draw_symbol(SymId::BUZZ_ROLL, painter);
    } else {
        painter->set_brush(Brush(item->cur_color(opt)));
        painter->set_no_pen();
        painter->draw_path(item->path());
    }
}

void TDraw::draw(const TremoloTwoChord *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    const TremoloTwoChord::LayoutData *ldata = item->ldata();

    if (!item->beam_segments().empty()) {
        // two-note trems act like beams

        // make beam thickness independent of slant
        // (expression can be simplified?)
        const LineF bs = item->beam_segments().front()->line;
        double d = (std::abs(bs.y2() - bs.y1())) / (bs.x2() - bs.x1());
        if (item->beam_segments().size() > 1 && d > M_PI / 6.0) {
            d = M_PI / 6.0;
        }
        double ww = (ldata->beam_width / 2.0) / sin(M_PI_2 - atan(d));
        painter->set_brush(Brush(item->cur_color(opt)));
        painter->set_no_pen();
        for (const BeamSegment *bs1 : item->beam_segments()) {
            painter->draw_polygon(PolygonF({
                                      PointF(bs1->line.x1(), bs1->line.y1() - ww),
                                      PointF(bs1->line.x2(), bs1->line.y2() - ww),
                                      PointF(bs1->line.x2(), bs1->line.y2() + ww),
                                      PointF(bs1->line.x1(), bs1->line.y1() + ww),
                                  }),
                                  FillRule::ODD_EVEN_FILL);
        }
    }
}

void TDraw::draw(const TremoloBar *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    const TremoloBar::LayoutData *ldata = item->ldata();
    const double lw = item->absolute_from_spatium(item->line_width());
    Pen pen(item->cur_color(opt), lw, PenStyle::SOLID_LINE, PenCapStyle::ROUND_CAP, PenJoinStyle::ROUND_JOIN);
    painter->set_pen(pen);
    painter->draw_polyline(ldata->polygon);
}

void TDraw::draw(const TrillSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    painter->set_pen(item->spanner()->cur_color(opt));
    item->draw_symbols(item->symbols(), painter);
}

void TDraw::draw(const TripletFeel *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_base(item, painter, opt);
}

void TDraw::draw(const Tuplet *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;

    // if in a TAB without stems, tuplets are not shown
    const StaffType *stt = item->staff_type();
    if (stt && stt->is_tab_staff() && stt->stemless()) {
        return;
    }

    Color color(item->cur_color(opt));
    if (item->number()) {
        painter->set_pen(color);
        PointF pos(item->number()->pos());
        painter->translate(pos);
        draw(item->number(), painter, opt);
        painter->translate(-pos);
    }
    if (item->has_bracket()) {
        Pen pen(color, item->absolute_from_spatium(item->bracket_width()));
        pen.set_join_style(PenJoinStyle::MITER_JOIN);
        pen.set_cap_style(PenCapStyle::FLAT_CAP);
        painter->set_pen(pen);
        if (!item->number()) {
            painter->draw_polyline(item->bracket_l, 4);
        } else {
            painter->draw_polyline(item->bracket_l, 3);
            painter->draw_polyline(item->bracket_r, 3);
        }
    }
}

void TDraw::draw(const VibratoSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    painter->set_pen(item->spanner()->cur_color(opt));
    item->draw_symbols(item->symbols(), painter);
}

void TDraw::draw(const VoltaSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::draw(const WhammyBarSegment *item, Painter *painter, const PaintOptions &opt) {
    TRACE_DRAW_ITEM;
    draw_text_line_base_segment(item, painter, opt);
}

void TDraw::set_mask(const EngravingItem *item, Painter *painter) {
    const EngravingItem::LayoutData *ldata = item->ldata();

    const Shape &mask = ldata->mask();
    if (mask.empty()) {
        return;
    }

    RectF background = ldata->bbox().padded(item->spatium());

    painter->set_mask(background, mask.to_rects());
}
