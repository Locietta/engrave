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

#include "singlelayout.h"

#include "draw/fontmetrics.h"

#include "types/types.h"
#include "types/typesconv.h"
#include "types/symnames.h"

#include "dom/dummyparent.h"

#include "dom/factory.h"

#include "dom/engravingitem.h"
#include "dom/score.h"

#include "dom/accidental.h"
#include "dom/actionicon.h"
#include "dom/ambitus.h"
#include "dom/articulation.h"
#include "dom/bagpembell.h"
#include "dom/barline.h"
#include "dom/bend.h"
#include "dom/bracket.h"
#include "dom/breath.h"
#include "dom/capo.h"
#include "dom/chordline.h"
#include "dom/clef.h"
#include "dom/dynamic.h"
#include "dom/expression.h"
#include "dom/fermata.h"
#include "dom/fingering.h"
#include "dom/fret.h"
#include "dom/glissando.h"
#include "dom/gradualtempochange.h"
#include "dom/guitarbend.h"
#include "dom/hairpin.h"
#include "dom/hammeronpulloff.h"
#include "dom/harppedaldiagram.h"
#include "dom/instrchange.h"
#include "dom/jump.h"
#include "dom/keysig.h"
#include "dom/letring.h"
#include "dom/line.h"
#include "dom/lyrics.h"
#include "dom/marker.h"
#include "dom/measurenumber.h"
#include "dom/measurerepeat.h"
#include "dom/note.h"
#include "dom/noteline.h"
#include "dom/ornament.h"
#include "dom/ottava.h"
#include "dom/page.h"
#include "dom/palmmute.h"
#include "dom/pedal.h"
#include "dom/playcounttext.h"
#include "dom/playtechannotation.h"
#include "dom/rehearsalmark.h"
#include "dom/slur.h"
#include "dom/soundflag.h"
#include "dom/spacer.h"
#include "dom/staff.h"
#include "dom/stafftext.h"
#include "dom/stafftypechange.h"
#include "dom/sticking.h"
#include "dom/stringtunings.h"
#include "dom/symbol.h"
#include "dom/systemtext.h"
#include "dom/tapping.h"
#include "dom/tempotext.h"
#include "dom/text.h"
#include "dom/textline.h"
#include "dom/textlinebase.h"
#include "dom/timesig.h"
#include "dom/tremolobar.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/trill.h"
#include "dom/vibrato.h"
#include "dom/volta.h"
#include "dom/whammybar.h"

#include "dom/utils.h"

#include "rendering/score/tlayout.h"
#include "rendering/score/textlayout.h"
#include "rendering/score/tremololayout.h"
#include "rendering/score/chordlayout.h"
#include "rendering/score/slurtielayout.h"

#include "log.h"

using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;
using namespace mu::engraving::rendering::single;

void SingleLayout::layout_item(EngravingItem *item) {
    Context ctx(item->score());

    switch (item->type()) {
        case ElementType::ACCIDENTAL: layout(to_accidental(item), ctx); break;
        case ElementType::ACTION_ICON: layout(to_action_icon(item), ctx); break;
        case ElementType::AMBITUS: layout(to_ambitus(item), ctx); break;
        case ElementType::ARPEGGIO: layout(to_arpeggio(item), ctx); break;
        case ElementType::CHORD_BRACKET: layout(to_chord_bracket(item), ctx); break;
        case ElementType::ARTICULATION: layout(to_articulation(item), ctx); break;
        case ElementType::BAGPIPE_EMBELLISHMENT: layout(to_bagpipe_embellishment(item), ctx); break;
        case ElementType::BAR_LINE: layout(to_bar_line(item), ctx); break;
        case ElementType::BEND: layout(to_bend(item), ctx); break;
        case ElementType::BRACKET: layout(to_bracket(item), ctx); break;
        case ElementType::BREATH: layout(to_breath(item), ctx); break;
        case ElementType::CAPO: layout(to_capo(item), ctx); break;
        case ElementType::CHORDLINE: layout(to_chord_line(item), ctx); break;
        case ElementType::CLEF: layout(to_clef(item), ctx); break;
        case ElementType::DYNAMIC: layout(to_dynamic(item), ctx); break;
        case ElementType::EXPRESSION: layout(to_expression(item), ctx); break;
        case ElementType::FERMATA: layout(to_fermata(item), ctx); break;
        case ElementType::FINGERING: layout(to_fingering(item), ctx); break;
        case ElementType::FRET_DIAGRAM: layout(to_fret_diagram(item), ctx); break;
        case ElementType::FSYMBOL: layout(to_f_symbol(item), ctx); break;
        case ElementType::GLISSANDO: layout(to_glissando(item), ctx); break;
        case ElementType::GUITAR_BEND: layout(to_guitar_bend(item), ctx); break;
        case ElementType::GRADUAL_TEMPO_CHANGE: layout(to_gradual_tempo_change(item), ctx); break;
        case ElementType::HAIRPIN: layout(to_hairpin(item), ctx); break;
        case ElementType::HAMMER_ON_PULL_OFF: layout(to_hammer_on_pull_off(item), ctx); break;
        case ElementType::HARP_DIAGRAM: layout(to_harp_pedal_diagram(item), ctx); break;
        case ElementType::IMAGE: layout(to_image(item), ctx); break;
        case ElementType::INSTRUMENT_CHANGE: layout(to_instrument_change(item), ctx); break;
        case ElementType::JUMP: layout(to_jump(item), ctx); break;
        case ElementType::KEYSIG: layout(to_key_sig(item), ctx); break;
        case ElementType::LAYOUT_BREAK: layout(to_layout_break(item), ctx); break;
        case ElementType::LET_RING: layout(to_let_ring(item), ctx); break;
        case ElementType::LYRICS: layout(to_lyrics(item), ctx); break;
        case ElementType::MARKER: layout(to_marker(item), ctx); break;
        case ElementType::MEASURE_NUMBER: layout(to_measure_number(item), ctx); break;
        case ElementType::MEASURE_REPEAT: layout(to_measure_repeat(item), ctx); break;
        case ElementType::NOTEHEAD: layout(to_note_head(item), ctx); break;
        case ElementType::NOTELINE: layout(to_note_line(item), ctx); break;
        case ElementType::OTTAVA: layout(to_ottava(item), ctx); break;
        case ElementType::ORNAMENT: layout(to_ornament(item), ctx); break;
        case ElementType::PALM_MUTE: layout(to_palm_mute(item), ctx); break;
        case ElementType::PEDAL: layout(to_pedal(item), ctx); break;
        case ElementType::PLAY_COUNT_TEXT: layout(to_play_count_text(item), ctx); break;
        case ElementType::PLAYTECH_ANNOTATION: layout(to_play_tech_annotation(item), ctx); break;
        case ElementType::REHEARSAL_MARK: layout(to_rehearsal_mark(item), ctx); break;
        case ElementType::SLUR: layout(to_slur(item), ctx); break;
        case ElementType::SPACER: layout(to_spacer(item), ctx); break;
        case ElementType::STAFF_TEXT: layout(to_staff_text(item), ctx); break;
        case ElementType::STAVE_SHARING_LABEL: layout(to_stave_sharing_label(item), ctx); break;
        case ElementType::STAFFTYPE_CHANGE: layout(to_staff_type_change(item), ctx); break;
        case ElementType::STICKING: layout(to_sticking(item), ctx); break;
        case ElementType::STRING_TUNINGS: layout(to_string_tunings(item), ctx); break;
        case ElementType::SYMBOL: layout(to_symbol(item), ctx); break;
        case ElementType::SYSTEM_TEXT: layout(to_system_text(item), ctx); break;
        case ElementType::SOUND_FLAG: layout(item_cast<SoundFlag *>(item), ctx); break;
        case ElementType::TAPPING: layout(to_tapping(item), ctx); break;
        case ElementType::TEMPO_TEXT: layout(to_tempo_text(item), ctx); break;
        case ElementType::TEXT: layout(to_text(item), ctx); break;
        case ElementType::TEXTLINE: layout(to_text_line(item), ctx); break;
        case ElementType::TIMESIG: layout(to_time_sig(item), ctx); break;
        case ElementType::TREMOLO_SINGLECHORD: layout(item_cast<TremoloSingleChord *>(item), ctx); break;
        case ElementType::TREMOLO_TWOCHORD: layout(item_cast<TremoloTwoChord *>(item), ctx); break;
        case ElementType::TREMOLOBAR: layout(to_tremolo_bar(item), ctx); break;
        case ElementType::TRILL: layout(to_trill(item), ctx); break;
        case ElementType::VIBRATO: layout(to_vibrato(item), ctx); break;
        case ElementType::VOLTA: layout(to_volta(item), ctx); break;
        case ElementType::WHAMMY_BAR: layout(to_whammy_bar(item), ctx); break;
        // drumset
        case ElementType::CHORD: layout(to_chord(item), ctx); break;
        case ElementType::STEM: layout(to_stem(item), ctx); break;
        default:
            LOGE() << "Not handled: " << item->type_name();
            IF_ASSERT_FAILED(false) {
                LayoutContext tctx(item->score());
                TLayout::layout_item(item, tctx);
            }
            break;
    }
}

void SingleLayout::layout_line_segment(LineSegment *item, const Context &ctx) {
    switch (item->type()) {
        case ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT: layout(to_gradual_tempo_change_segment(item), ctx); break;
        case ElementType::HAIRPIN_SEGMENT: layout(to_hairpin_segment(item), ctx); break;
        case ElementType::LET_RING_SEGMENT: layout(to_let_ring_segment(item), ctx); break;
        case ElementType::OTTAVA_SEGMENT: layout(to_ottava_segment(item), ctx); break;
        case ElementType::PALM_MUTE_SEGMENT: layout(to_palm_mute_segment(item), ctx); break;
        case ElementType::PEDAL_SEGMENT: layout(to_pedal_segment(item), ctx); break;
        case ElementType::TEXTLINE_SEGMENT: layout(to_text_line_segment(item), ctx); break;
        case ElementType::TRILL_SEGMENT: layout(to_trill_segment(item), ctx); break;
        case ElementType::VIBRATO_SEGMENT: layout(to_vibrato_segment(item), ctx); break;
        case ElementType::VOLTA_SEGMENT: layout(to_volta_segment(item), ctx); break;
        case ElementType::WHAMMY_BAR_SEGMENT: layout(to_whammy_bar_segment(item), ctx); break;
        default: UNREACHABLE; break;
    }
}

const MStyle &SingleLayout::Context::style() const { return score_->style(); }

std::shared_ptr<IEngravingFont> SingleLayout::Context::engraving_font() const { return score_->engraving_font(); }

DummyParent *SingleLayout::Context::dummy_parent() const { return score_->dummy(); }

void SingleLayout::layout(Accidental *item, const Context &) {
    if (!item->ldata() || !item->ldata()->is_valid()) {
        Accidental::LayoutData *ldata = item->mutldata();
        SymId sym_id = item->sym_id();
        Accidental::LayoutData::Sym s(sym_id, 0.0, 0.0);
        ldata->syms.push_back(s);
        ldata->set_bbox(item->sym_bbox(sym_id));
    }
}

void SingleLayout::layout(ActionIcon *item, const Context &) {
    FontMetrics font_metrics(item->icon_font());
    item->setbbox(font_metrics.bounding_rect(Char(item->icon())));
}

void SingleLayout::layout(Ambitus *item, const Context &ctx) {
    Ambitus::LayoutData *ldata = item->mutldata();
    double head_wdt = item->head_width();
    double spatium = item->spatium();

    double line_dist = spatium;
    constexpr int num_of_lines = 3;

    //
    // NOTEHEADS Y POS
    //
    // if pitch == INVALID_PITCH or tpc == Tpc::TPC_INVALID, set to some default:
    // for use in palettes and when actual range cannot be calculated (new ambitus or no notes in staff)
    //

    // top notehead
    ldata->top_pos.set_y(0.0);

    // bottom notehead
    ldata->bottom_pos.set_y((num_of_lines - 1) * line_dist);

    //
    // NOTEHEAD X POS
    //
    // Note: manages colliding accidentals
    //

    layout(item->top_accidental(), ctx);
    layout(item->bottom_accidental(), ctx);

    double acc_note_dist = item->absolute_from_spatium(ctx.style().style_s(Sid::ACCIDENTAL_NOTE_DISTANCE));
    double x_accid_off_top = item->top_accidental()->width(LdAccess::BAD) + acc_note_dist;
    double x_accid_off_bottom = item->bottom_accidental()->width(LdAccess::BAD) + acc_note_dist;

    switch (item->direction()) {
        case DirectionH::AUTO: // noteheads one above the other
            // left align noteheads and right align accidentals 'hanging' on the left
            ldata->top_pos.set_x(0.0);
            ldata->bottom_pos.set_x(0.0);
            item->top_accidental()->mutldata()->set_pos_x(-x_accid_off_top);
            item->bottom_accidental()->mutldata()->set_pos_x(-x_accid_off_bottom);
            break;
        case DirectionH::LEFT: // top notehead at the left of bottom notehead
            // place top notehead at left margin; bottom notehead at right of top head;
            // top accid. 'hanging' on left of top head and bottom accid. 'hanging' at left of bottom head
            ldata->top_pos.set_x(0.0);
            ldata->bottom_pos.set_x(head_wdt);
            item->top_accidental()->mutldata()->set_pos_x(-x_accid_off_top);
            item->bottom_accidental()->mutldata()->set_pos_x(head_wdt - x_accid_off_bottom);
            break;
        case DirectionH::RIGHT: // top notehead at the right of bottom notehead
            // bottom notehead at left margin; top notehead at right of bottomnotehead
            // top accid. 'hanging' on left of top head and bottom accid. 'hanging' at left of bottom head
            ldata->bottom_pos.set_x(0.0);
            ldata->top_pos.set_x(head_wdt);
            item->bottom_accidental()->mutldata()->set_pos_x(-x_accid_off_bottom);
            item->top_accidental()->mutldata()->set_pos_x(head_wdt - x_accid_off_top);
            break;
    }

    // compute line from top note centre to bottom note centre
    LineF full_line(ldata->top_pos.x() + head_wdt * 0.5, ldata->top_pos.y(), ldata->bottom_pos.x() + head_wdt * 0.5, ldata->bottom_pos.y());
    // shorten line on each side by offsets
    double y_delta = ldata->bottom_pos.y() - ldata->top_pos.y();
    if (!real_is_null(y_delta)) {
        double off = Ambitus::lineoffset_default.to_absolute(spatium);
        PointF p1 = full_line.point_at(off / y_delta);
        PointF p2 = full_line.point_at(1 - (off / y_delta));
        ldata->line = LineF(p1, p2);
    } else {
        ldata->line = full_line;
    }

    RectF head_rect(0, -0.5 * spatium, head_wdt, 1 * spatium);
    ldata->set_bbox(
        head_rect.translated(ldata->top_pos)
            .united(head_rect.translated(ldata->bottom_pos))
            .united(item->top_accidental()->ldata()->bbox(LdAccess::BAD).translated(item->top_accidental()->ldata()->pos()))
            .united(item->bottom_accidental()->ldata()->bbox(LdAccess::BAD).translated(item->bottom_accidental()->ldata()->pos())));
}

void SingleLayout::layout(Arpeggio *item, const Context &ctx) {
    Arpeggio::LayoutData *ldata = item->mutldata();

    auto symbol_line = [](const std::shared_ptr<const IEngravingFont> &f, Arpeggio::LayoutData *data, SymId end, SymId fill) {
        data->symbols.clear();

        double w = data->bottom - data->top;
        double w1 = f->advance(end, data->mag_s);
        double w2 = f->advance(fill, data->mag_s);
        int n = lrint((w - w1) / w2);
        for (int i = 0; i < n; ++i) {
            data->symbols.push_back(fill);
        }
        data->symbols.push_back(end);
    };

    ldata->arpeggio_height = item->spatium() * 4;
    ldata->top = 0.0;
    ldata->bottom = ldata->arpeggio_height;

    ldata->set_mag(item->staff() ? item->staff()->staff_mag(item->tick()) : item->mag());
    ldata->mag_s = ldata->mag() * (ctx.style().spatium() / ctx.style().default_spatium());

    std::shared_ptr<const IEngravingFont> font = ctx.engraving_font();
    switch (item->arpeggio_type()) {
        case ArpeggioType::NORMAL: {
            symbol_line(font, ldata, SymId::WIGGLE_ARPEGGIATO_UP, SymId::WIGGLE_ARPEGGIATO_UP);
            // string is rotated -90 degrees
            ldata->syms_b_box = font->bbox(ldata->symbols, ldata->mag_s);
            ldata->set_bbox(RectF(0.0, -ldata->syms_b_box.x() + ldata->top, ldata->syms_b_box.height(), ldata->syms_b_box.width()));
        } break;

        case ArpeggioType::UP: {
            symbol_line(font, ldata, SymId::WIGGLE_ARPEGGIATO_UP_ARROW, SymId::WIGGLE_ARPEGGIATO_UP);
            // string is rotated -90 degrees
            ldata->syms_b_box = font->bbox(ldata->symbols, ldata->mag_s);
            ldata->set_bbox(RectF(0.0, -ldata->syms_b_box.x() + ldata->top, ldata->syms_b_box.height(), ldata->syms_b_box.width()));
        } break;

        case ArpeggioType::DOWN: {
            symbol_line(font, ldata, SymId::WIGGLE_ARPEGGIATO_UP_ARROW, SymId::WIGGLE_ARPEGGIATO_UP);
            // string is rotated +90 degrees (so that UpArrow turns into a DownArrow)
            ldata->syms_b_box = font->bbox(ldata->symbols, ldata->mag_s);
            ldata->set_bbox(RectF(0.0, ldata->syms_b_box.x() + ldata->top, ldata->syms_b_box.height(), ldata->syms_b_box.width()));
        } break;

        case ArpeggioType::UP_STRAIGHT: {
            double x1 = item->spatium() * 0.5;
            ldata->syms_b_box = font->bbox(SymId::ARROWHEAD_BLACK_UP, ldata->mag_s);
            double w = ldata->syms_b_box.width();
            ldata->set_bbox(RectF(x1 - w * 0.5, ldata->top, w, ldata->bottom));
        } break;

        case ArpeggioType::DOWN_STRAIGHT: {
            double x1 = item->spatium() * 0.5;
            ldata->syms_b_box = font->bbox(SymId::ARROWHEAD_BLACK_DOWN, ldata->mag_s);
            double w = ldata->syms_b_box.width();
            ldata->set_bbox(RectF(x1 - w * 0.5, ldata->top, w, ldata->bottom));
        } break;

        case ArpeggioType::BRACKET: {
            double w = ctx.style().style_s(Sid::ARPEGGIO_HOOK_LEN).val() * item->spatium();
            ldata->set_bbox(RectF(0.0, ldata->top, w, ldata->bottom));
        } break;
    }
}

void SingleLayout::layout(ChordBracket *item, const Context &ctx) {
    ChordBracket::LayoutData *ldata = item->mutldata();

    ldata->arpeggio_height = item->spatium() * 2;
    ldata->top = 0.0;
    ldata->bottom = ldata->arpeggio_height;
    ldata->set_mag(1);
    ldata->mag_s = 1;

    double w = item->absolute_from_spatium(ctx.style().style_s(Sid::CHORD_BRACKET_HOOK_LEN));
    ldata->set_bbox(RectF(0.0, ldata->top, w, ldata->bottom));
}

void SingleLayout::layout(Articulation *item, const Context &ctx) {
    RectF bbox;

    if (item->text_type() != ArticulationTextType::NO_TEXT) {
        if (!item->text()) {
            Text *text = new Text(item, TextStyleType::ARTICULATION);
            static const ElementStyle element_style = {};
            text->init_element_style(&element_style);
            item->set_text(text);
        }

        Text *text = item->text();
        text->set_xml_text(TConv::text(item->text_type()));
        text->set_track(item->track());
        text->set_ownership_parent(item);

        layout_text_base(item->text(), ctx, item->text()->mutldata());
        bbox = text->ldata()->bbox();
    } else {
        bbox = item->sym_bbox(item->sym_id());
        bbox.translate(-0.5 * bbox.width(), 0.0);
    }

    item->setbbox(bbox);
}

void SingleLayout::layout(BagpipeEmbellishment *item, const Context &ctx) {
    BagpipeEmbellishment::LayoutData *ldata = item->mutldata();
    const double mags = item->mag_s() * 0.75; // grace head magnification
    const double spatium = ctx.style().spatium();

    const BagpipeNoteList nl = item->resolve_note_list();

    ldata->clear_bbox();
    ldata->is_draw_beam = nl.size() > 1;
    ldata->is_draw_flag = nl.size() == 1;
    ldata->spatium = spatium;
    ldata->headsym = SymId::NOTEHEAD_BLACK;
    ldata->flagsym = SymId::FLAG32ND_UP;
    ldata->stem_line_w = ldata->spatium * 0.1;

    BagpipeEmbellishment::LayoutData::BeamData &data_beam = ldata->beam_data;
    data_beam.y = (-8 * spatium / 2);
    data_beam.width = (0.3 * spatium);

    const RectF head_b_box = ctx.engraving_font()->bbox(ldata->headsym, mags);
    const RectF flag_b_box = ctx.engraving_font()->bbox(ldata->flagsym, mags);
    const double xcorr = spatium * 0.1; // correction to align flag with top of stem

    double head_w = head_b_box.width();
    const double headw = 1.2 * head_w;                          // grace head width
    const double headp = 1.6 * head_w;                          // horizontal head pitch
    const double xl = (1 - 1.6 * (nl.size() - 1)) * head_w / 2; // calc x for stem of leftmost note

    // draw the notes including stem, (optional) flag and (optional) ledger line
    double x = xl;
    for (size_t i = 0; i < nl.size(); ++i) {
        BagpipeEmbellishment::LayoutData::NoteData &note_data = ldata->notes_data[i];
        const int line = BagpipeEmbellishment::bagpipe_noteinfo_list[nl.at(i)].line;
        const double y1f = ((line - 6) * spatium / 2); // top of stem for note with flag
        const double y2 = (line * spatium / 2);        // bottom of stem
        const double ycorr = (0.8 * spatium);          // correction to align flag with top of stem

        // head
        note_data.head_xy = PointF(x - headw, y2);
        ldata->add_bbox(head_b_box.translated(note_data.head_xy));

        // stem
        // top of stems actually used
        double y1 = ldata->is_draw_flag ? y1f : data_beam.y;
        note_data.stem_line = LineF(x - ldata->stem_line_w * .5, y1, x - ldata->stem_line_w * .5, y2);
        ldata->add_bbox(RectF(x - ldata->stem_line_w * .5 - headw, y1, ldata->stem_line_w, y2 - y1));

        // flag
        if (ldata->is_draw_flag) {
            note_data.flag_xy = PointF(x - ldata->stem_line_w * .5 + xcorr, y1 + ycorr);
            ldata->add_bbox(flag_b_box.translated(note_data.flag_xy));
        }

        // draw the ledger line for high A
        if (line == -2) {
            note_data.ledger_line = LineF(x - headw * 1.5 - ldata->stem_line_w * .5, y2, x + headw * .5 - ldata->stem_line_w * .5, y2);
            ldata->add_bbox(RectF(x - headw * 1.5 - ldata->stem_line_w * .5, y2 - ldata->stem_line_w * 2, headw * 2, ldata->stem_line_w));
        }

        // move x to next note x position
        x += headp;
    }

    data_beam.x1 = xl - ldata->stem_line_w * .5;
    data_beam.x2 = x - headp - ldata->stem_line_w * .5;
}

void SingleLayout::layout(BarLine *item, const Context &ctx) {
    BarLine::LayoutData *ldata = item->mutldata();
    ldata->set_pos(PointF());
    ldata->set_mag(1.0);

    double spatium = item->spatium();
    ldata->y1 = (spatium * .5 * item->span_from());
    if (muse::real_is_equal(ldata->y2, 0.0)) {
        ldata->y2 = (spatium * .5 * (8.0 + item->span_to()));
    }

    auto layout_width = [](BarLine *item, const Context &ctx) {
        const double dot_width = item->sym_width(SymId::REPEAT_DOT);

        double w = 0.0;
        switch (item->bar_line_type()) {
            case BarLineType::DOUBLE:
                w = ctx.style().style_absolute(Sid::DOUBLE_BAR_WIDTH) * 2.0 + ctx.style().style_absolute(Sid::DOUBLE_BAR_DISTANCE);
                break;
            case BarLineType::DOUBLE_HEAVY:
                w = ctx.style().style_absolute(Sid::END_BAR_WIDTH) * 2.0 + ctx.style().style_absolute(Sid::END_BAR_DISTANCE);
                break;
            case BarLineType::END_START_REPEAT:
                w = ctx.style().style_absolute(Sid::END_BAR_WIDTH) + ctx.style().style_absolute(Sid::BAR_WIDTH) * 2.0 +
                    ctx.style().style_absolute(Sid::END_BAR_DISTANCE) * 2.0 +
                    ctx.style().style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION) * 2.0 + dot_width * 2;
                break;
            case BarLineType::START_REPEAT:
            case BarLineType::END_REPEAT:
                w = ctx.style().style_absolute(Sid::END_BAR_WIDTH) + ctx.style().style_absolute(Sid::BAR_WIDTH) +
                    ctx.style().style_absolute(Sid::END_BAR_DISTANCE) + ctx.style().style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION) +
                    dot_width;
                break;
            case BarLineType::END:
            case BarLineType::REVERSE_END:
                w = ctx.style().style_absolute(Sid::END_BAR_WIDTH) + ctx.style().style_absolute(Sid::BAR_WIDTH) +
                    ctx.style().style_absolute(Sid::END_BAR_DISTANCE);
                break;
            case BarLineType::BROKEN: w = ctx.style().style_absolute(Sid::DASH_BAR_WIDTH); break;
            case BarLineType::NORMAL:
            case BarLineType::DOTTED: w = ctx.style().style_absolute(Sid::BAR_WIDTH); break;
            case BarLineType::HEAVY: w = ctx.style().style_absolute(Sid::END_BAR_WIDTH); break;
        }
        return w;
    };

    double w = layout_width(item, ctx) * ldata->mag();
    RectF bbox(0.0, ldata->y1, w, ldata->y2 - ldata->y1);
    ldata->set_bbox(bbox);
}

void SingleLayout::layout(Bend *item, const Context &) {
    Bend::LayoutData *ldata = item->mutldata();
    double spatium = item->spatium();
    double lw = item->absolute_from_spatium(item->line_width());

    ldata->note_width = 0.0;
    ldata->note_pos = PointF();

    RectF bb;

    FontMetrics fm(item->font(spatium));

    size_t n = item->points().size();
    double x = ldata->note_width;
    double y = -spatium * .8;
    double x2, y2;

    double aw = spatium * .5;
    PolygonF arrow_up;
    arrow_up << PointF(0, 0) << PointF(aw * .5, aw) << PointF(-aw * .5, aw);
    PolygonF arrow_down;
    arrow_down << PointF(0, 0) << PointF(aw * .5, -aw) << PointF(-aw * .5, -aw);

    for (size_t pt = 0; pt < n; ++pt) {
        if (pt == (n - 1)) {
            break;
        }
        int pitch = item->points().at(pt).pitch;
        if (pt == 0 && pitch) {
            y2 = -ldata->note_pos.y() - spatium * 2;
            x2 = x;
            bb.unite(RectF(x, y, x2 - x, y2 - y));

            bb.unite(arrow_up.translated(x2, y2 + spatium * .2).bounding_rect());

            int idx = (pitch + 12) / 25;
            const char *l = Bend::label[idx];
            bb.unite(fm.bounding_rect(String::from_ascii(l)));
            y = y2;
        }
        if (pitch == item->points().at(pt + 1).pitch) {
            if (pt == (n - 2)) {
                break;
            }
            x2 = x + spatium;
            y2 = y;
            bb.unite(RectF(x, y, x2 - x, y2 - y));
        } else if (pitch < item->points().at(pt + 1).pitch) {
            // up
            x2 = x + spatium * .5;
            y2 = -ldata->note_pos.y() - spatium * 2;
            double dx = x2 - x;
            double dy = y2 - y;

            PainterPath path;
            path.move_to(x, y);
            path.cubic_to(x + dx / 2, y, x2, y + dy / 4, x2, y2);
            bb.unite(path.bounding_rect());
            bb.unite(arrow_up.translated(x2, y2 + spatium * .2).bounding_rect());

            int idx = (item->points().at(pt + 1).pitch + 12) / 25;
            const char *l = Bend::label[idx];
            bb.unite(fm.bounding_rect(String::from_ascii(l)));
        } else {
            // down
            x2 = x + spatium * .5;
            y2 = y + spatium * 3;
            double dx = x2 - x;
            double dy = y2 - y;

            PainterPath path;
            path.move_to(x, y);
            path.cubic_to(x + dx / 2, y, x2, y + dy / 4, x2, y2);
            bb.unite(path.bounding_rect());

            bb.unite(arrow_down.translated(x2, y2 - spatium * .2).bounding_rect());
        }
        x = x2;
        y = y2;
    }
    bb.adjust(-lw, -lw, lw, lw);
    ldata->set_bbox(bb);
    ldata->set_pos(0.0, 0.0);
}

void SingleLayout::layout(Bracket *item, const Context &ctx) {
    Bracket::LayoutData *ldata = item->mutldata();

    ldata->bracket_height = 3.5 * item->spatium() * 2;
    ldata->brace_symbol = item->brace_symbol();

    Shape shape;

    switch (item->bracket_type()) {
        case BracketType::BRACE: {
            if (item->brace_symbol() == SymId::NO_SYM) {
                ldata->brace_symbol = SymId::BRACE;
            }
            double h = ldata->bracket_height;
            double w = item->sym_width(ldata->brace_symbol) * item->magx();
            ldata->set_bbox(RectF(0, 0, w, h));
            ldata->shape.add(ldata->bbox());
            ldata->bracket_width = w + ctx.style().style_absolute(Sid::AKKOLADE_BAR_DISTANCE);
        } break;
        case BracketType::NORMAL: {
            double spatium = item->spatium();
            double w = ctx.style().style_absolute(Sid::BRACKET_WIDTH) * 0.5;
            double x = -w;

            double bd = spatium * 0.5;
            shape.add(RectF(x, -bd, w * 2, 2 * (item->ldata()->h2() + bd)));
            shape.add(item->sym_bbox(SymId::BRACKET_TOP).translated(PointF(-w, -bd)));
            shape.add(item->sym_bbox(SymId::BRACKET_BOTTOM).translated(PointF(-w, bd + 2 * item->ldata()->h2())));

            w += item->sym_width(SymId::BRACKET_TOP);
            double y = -item->sym_height(SymId::BRACKET_TOP) - bd;
            double h = (-y + item->ldata()->h2()) * 2;
            ldata->set_bbox(x, y, w, h);

            ldata->bracket_width = ctx.style().style_absolute(Sid::BRACKET_WIDTH) + ctx.style().style_absolute(Sid::BRACKET_DISTANCE);
        } break;
        case BracketType::SQUARE: {
            double w = ctx.style().style_absolute(Sid::STAFF_LINE_WIDTH) * .5;
            double x = -w;
            double y = -w;
            double h = (item->ldata()->h2() + w) * 2;
            w += (0.5 * item->spatium() + 3 * w);
            ldata->set_bbox(x, y, w, h);
            shape.add(item->ldata()->bbox());

            ldata->bracket_width = ctx.style().style_absolute(Sid::STAFF_LINE_WIDTH) / 2 + 0.5 * item->spatium();
        } break;
        case BracketType::LINE: {
            double spatium = item->spatium();
            double w = 0.67 * ctx.style().style_absolute(Sid::BRACKET_WIDTH) * 0.5;
            double x = -w;
            double bd = spatium * 0.25;
            double y = -bd;
            double h = (-y + item->ldata()->h2()) * 2;
            ldata->set_bbox(x, y, w, h);
            shape.add(item->ldata()->bbox());

            ldata->bracket_width =
                0.67 * ctx.style().style_absolute(Sid::BRACKET_WIDTH) + ctx.style().style_absolute(Sid::BRACKET_DISTANCE);
        } break;
        case BracketType::GROUP: layout_group_bracket(item, ctx); break;
        case BracketType::NO_BRACKET: break;
    }

    ldata->shape = shape;
}

void SingleLayout::layout_group_bracket(Bracket *item, const Context &ctx) {
    Bracket::LayoutData *ldata = item->mutldata();
    ldata->bracket_height = 12 * item->spatium();
    double w = ctx.style().style_absolute(Sid::STAFF_LINE_WIDTH) * 0.5;
    double x = 0.0;
    double y = -w;
    double h = (ldata->bracket_height * 0.5 + w) * 2;
    double width = item->spatium();
    ldata->set_bbox(RectF(x, y, width, h));
    ldata->shape.add(ldata->bbox());

    ldata->bracket_width = width;

    if (!item->text()) {
        const_cast<Bracket *>(item)->set_text(new Text(const_cast<Bracket *>(item)));
    }

    Text *text = item->text();
    text->set_align(Align(AlignH::HCENTER, AlignV::VCENTER));
    text->set_position(AlignH::HCENTER);
    text->set_text_angle(-90);
    text->set_xml_text(item->bracket_item()->long_name());
    layout(text, ctx);
    text->mutldata()->set_pos(0.0, item->ldata()->bbox().height() / 2);

    double text_padding = 0.5 * item->spatium();
    RectF mask = text->ldata()->bbox().translated(text->pos()).padded(text_padding);
    ldata->set_mask(mask);
}

void SingleLayout::layout(Breath *item, const Context &) { item->setbbox(item->sym_bbox(item->sym_id())); }

void SingleLayout::layout(Capo *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(Chord *item, const Context &ctx) {
    LayoutContext tctx(ctx.dont_use_score());
    ChordLayout::compute_up(item, tctx);
    ChordLayout::layout(item, tctx);
    ChordLayout::layout_stem(item, tctx);
    ChordLayout::layout_ledger_lines({item}, tctx);
}

void SingleLayout::layout(ChordLine *item, const Context &ctx) {
    ChordLine::LayoutData *ldata = item->mutldata();
    ldata->set_mag(1.0);
    if (item->has_source_offsets()) {
        ldata->path = item->source_offset_path(
            item->spatium(), item->ownership_parent() && item->ownership_parent()->is_chord() ? item->chord()->intrinsic_mag() : 1.0);
    } else if (!item->modified()) {
        double x2 = 0;
        double y2 = 0;
        double base_length = item->spatium();
        double hor_base_length = 1.2 * base_length; // let the symbols extend a bit more horizontally
        x2 += item->is_to_the_left() ? -hor_base_length : hor_base_length;
        y2 += item->is_below() ? base_length : -base_length;
        if (item->chord_line_type() != ChordLineType::NOTYPE && !item->is_wavy()) {
            PainterPath path;
            if (!item->is_to_the_left()) {
                if (item->is_straight()) {
                    path.line_to(x2, y2);
                } else {
                    path.cubic_to(x2 / 2, 0.0, x2, y2 / 2, x2, y2);
                }
            } else {
                if (item->is_straight()) {
                    path.line_to(x2, y2);
                } else {
                    path.cubic_to(0.0, y2 / 2, x2 / 2, y2, x2, y2);
                }
            }
            ldata->path = path;
        }
    }

    ldata->set_pos(0.0, 0.0);

    if (!item->is_wavy()) {
        RectF r = ldata->path.bounding_rect();
        int x1 = 0, y1 = 0, width = 0, height = 0;

        x1 = r.x();
        y1 = r.y();
        width = r.width();
        height = r.height();
        ldata->set_bbox(x1, y1, width, height);
    } else {
        RectF r = ctx.engraving_font()->bbox(item->wave_sym(), item->mag_s());

        ldata->set_bbox(r);
    }
}

void SingleLayout::layout(Clef *item, const Context &ctx) {
    Clef::LayoutData *ldata = item->mutldata();
    constexpr int lines = 5;
    constexpr Spatium line_dist = 1.0_sp;
    double spatium = ctx.style().spatium();
    Spatium yoff = 0.0_sp;

    if (item->clef_type() != ClefType::INVALID && item->clef_type() != ClefType::MAX) {
        ldata->sym_id = ClefInfo::sym_id(item->clef_type());
        yoff = line_dist * (5 - ClefInfo::line(item->clef_type()));
    } else {
        ldata->sym_id = SymId::NO_SYM;
    }

    switch (item->clef_type()) {
        case ClefType::C_19C: // 19th C clef is like a G clef
            yoff = line_dist * 1.5;
            break;
        case ClefType::TAB:        // TAB clef
        case ClefType::TAB4:       // TAB clef 4 strings
        case ClefType::TAB_SERIF:  // TAB clef alternate style
        case ClefType::TAB4_SERIF: // TAB clef alternate style
            yoff = line_dist * (lines - 1) * 0.5;
            break;
        case ClefType::PERC: // percussion clefs
        case ClefType::PERC2: yoff = line_dist * (lines - 1) * 0.5; break;
        default: break;
    }

    ldata->set_pos(0.0, yoff.to_absolute(spatium));

    RectF bbox = item->sym_bbox(ldata->sym_id);
    ldata->set_bbox(bbox);
}

void SingleLayout::layout(Expression *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(Fermata *item, const Context &) {
    item->set_pos(PointF());
    item->set_offset(PointF());
    RectF b(item->sym_bbox(item->sym_id()));
    item->setbbox(b.translated(-0.5 * b.width(), 0.0));
}

void SingleLayout::layout(Fingering *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(FretDiagram *item, const Context &ctx) {
    FretDiagram::LayoutData *ldata = item->mutldata();
    double spatium = item->spatium();
    ldata->string_line_width = (spatium * 0.08);
    ldata->nut_line_width = ((item->fret_offset() || !item->show_nut()) ? ldata->string_line_width : spatium * 0.2);
    ldata->string_dist = (ctx.style().style_absolute(Sid::FRET_STRING_SPACING));
    ldata->fret_dist = (ctx.style().style_absolute(Sid::FRET_FRET_SPACING));
    ldata->marker_size = (ldata->string_dist * 0.8);

    double w = ldata->string_dist * (item->strings() - 1) + ldata->marker_size;
    double h = (item->frets() + 1) * ldata->fret_dist + ldata->marker_size;
    double y = -(ldata->marker_size * 0.5 + ldata->fret_dist);
    double x = -(ldata->marker_size * 0.5);

    // Allocate space for fret offset number
    if (item->fret_offset() > 0) {
        Font scaled_font(item->fret_num_font());
        scaled_font.set_point_size_f(item->fret_num_font().point_size_f() * item->user_mag());

        FontMetrics fm2(scaled_font);
        double numw = fm2.width(String::number(item->fret_offset() + 1));
        double xdiff = numw + ldata->string_dist * .4;
        w += xdiff;
        x += (item->num_pos() == 0) == (item->orientation() == engraving::Orientation::VERTICAL) ? -xdiff : 0;
    }

    if (item->orientation() == engraving::Orientation::HORIZONTAL) {
        std::swap(w, h);
        std::swap(x, y);
    }

    ldata->set_bbox(x, y, w, h);
}

void SingleLayout::layout(FSymbol *item, const Context &) {
    item->setbbox(FontMetrics::bounding_rect(item->font(), item->to_string()));
    item->set_offset(0.0, 0.0);
    item->set_pos(0.0, 0.0);
}

void SingleLayout::layout(Dynamic *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(Glissando *item, const Context &ctx) {
    double spatium = item->spatium();

    if (item->spanner_segments().empty()) {
        item->add(item->create_line_segment());
    }
    LineSegment *s = item->front_segment();
    s->set_pos(PointF(-spatium * Glissando::gliss_palette_width / 2, spatium * Glissando::gliss_palette_height / 2));
    s->set_pos2(PointF(spatium * Glissando::gliss_palette_width, -spatium * Glissando::gliss_palette_height));
    layout(static_cast<GlissandoSegment *>(s), ctx);
}

void SingleLayout::layout(GlissandoSegment *item, const Context &) {
    if (item->pos2().x() <= 0) {
        item->setbbox(RectF());
        return;
    }

    RectF r = RectF(0.0, 0.0, item->pos2().x(), item->pos2().y()).normalized();
    double lw = item->absolute_from_spatium(item->line_width()) * .5;
    item->setbbox(r.adjusted(-lw, -lw, lw, lw));
}

void SingleLayout::layout(GradualTempoChange *item, const Context &ctx) { layout_line(item, ctx); }

void SingleLayout::layout(GradualTempoChangeSegment *item, const Context &ctx) {
    layout_text_line_base_segment(item, ctx);
    item->set_offset(PointF());
}

void SingleLayout::layout(GuitarBend *, const Context &) {
    MUSE_NOT_IMPLEMENTED;
    //! NOTE: Bends can be removed from disallowed elements in NotationInteraction::isOutgoingDragElementAllowed once this has been implemented
}

void SingleLayout::layout(GuitarBendSegment *, const Context &) { MUSE_NOT_IMPLEMENTED; }

void SingleLayout::layout(Hairpin *item, const Context &ctx) {
    item->set_pos(0.0, 0.0);
    layout_line(item, ctx);
}

void SingleLayout::layout(HammerOnPullOff *item, const Context &ctx) {
    double spatium = item->spatium();
    HammerOnPullOffSegment *s = nullptr;
    if (item->spanner_segments().empty()) {
        s = new HammerOnPullOffSegment(item);
        s->set_track(item->track());
        item->add(s);
    } else {
        s = to_hammer_on_pull_off_segment(item->front_segment());
    }

    s->set_spanner_segment_type(SpannerSegmentType::SINGLE);

    s->set_pos(PointF());
    s->ups(Grip::START).p = PointF(0, 0);
    s->ups(Grip::END).p = PointF(spatium * 6, 0);
    s->set_extra_height(0.0);

    SlurTieLayout::compute_bezier(s);

    layout(s, ctx);

    item->setbbox(s->ldata()->bbox());
}

void SingleLayout::layout(HammerOnPullOffSegment *item, const Context &ctx) {
    const std::vector<HammerOnPullOffText *> &hopo_texts = item->hopo_text();
    if (item->hopo_text().empty()) {
        item->add_hopo_text(new HammerOnPullOffText(item));
    }

    HammerOnPullOffText *hopo_text = hopo_texts.front();
    hopo_text->set_xml_text("H/P");

    Align align;
    align.vertical = AlignV::BASELINE;
    align.horizontal = AlignH::HCENTER;
    hopo_text->set_align(align);
    hopo_text->set_position(AlignH::HCENTER);
    layout_text_base(hopo_text, ctx, hopo_text->mutldata());

    RectF bbox = item->ldata()->bbox();
    double x = 0.5 * (bbox.left() + bbox.right());
    double y = bbox.top() - 0.5 * item->spatium();
    hopo_text->mutldata()->set_pos(x, y);

    Shape item_shape = item->mutldata()->shape();
    item_shape.add(hopo_text->shape().translated(hopo_text->pos()));
    item->mutldata()->set_shape(item_shape);
}

void SingleLayout::layout(HairpinSegment *item, const Context &ctx) {
    const double spatium = item->spatium();
    HairpinSegment::LayoutData *ldata = item->mutldata();

    HairpinType type = item->hairpin()->hairpin_type();
    if (item->hairpin()->is_line_type()) {
        item->set_two_lines(false);
        layout_text_line_base_segment(item, ctx);
        item->set_draw_circled_tip(false);
        item->set_circled_tip_radius(0.0);
    } else {
        item->set_two_lines(true);

        item->hairpin()->set_begin_text_align({AlignH::LEFT, AlignV::VCENTER});
        item->hairpin()->set_end_text_align({AlignH::RIGHT, AlignV::VCENTER});

        double x1 = 0.0;
        layout_text_line_base_segment(item, ctx);
        if (!item->text()->empty()) {
            x1 = item->text()->width() + spatium * .5;
        }

        Transform t;
        double h1 = item->hairpin()->hairpin_height().val() * spatium * .5;
        double h2 = item->hairpin()->hairpin_cont_height().val() * spatium * .5;

        double x = item->pos2().x();
        if (!item->end_text()->empty()) {
            x -= (item->end_text()->width() + spatium * .5); // 0.5 spatium distance
        }
        if (x < spatium) { // minimum size of hairpin
            x = spatium;
        }
        double y = item->pos2().y();
        double len = sqrt(x * x + y * y);
        t.rotate_radians(asin(y / len));

        item->set_draw_circled_tip(item->hairpin()->hairpin_circled_tip());
        item->set_circled_tip_radius(item->draw_circled_tip() ? 0.6 * spatium * .5 : 0.0);

        LineF l1, l2;

        switch (type) {
            case HairpinType::CRESC_HAIRPIN: {
                switch (item->spanner_segment_type()) {
                    case SpannerSegmentType::SINGLE:
                    case SpannerSegmentType::BEGIN: {
                        l1.set_line(x1 + item->circled_tip_radius() * 2.0, 0.0, len, h1);
                        l2.set_line(x1 + item->circled_tip_radius() * 2.0, 0.0, len, -h1);
                        PointF circled_tip;
                        circled_tip.set_x(x1 + item->circled_tip_radius());
                        circled_tip.set_y(0.0);
                        item->set_circled_tip(circled_tip);
                    } break;

                    case SpannerSegmentType::MIDDLE:
                    case SpannerSegmentType::END:
                        item->set_draw_circled_tip(false);
                        l1.set_line(x1, h2, len, h1);
                        l2.set_line(x1, -h2, len, -h1);
                        break;
                }
            } break;
            case HairpinType::DIM_HAIRPIN: {
                switch (item->spanner_segment_type()) {
                    case SpannerSegmentType::SINGLE:
                    case SpannerSegmentType::END: {
                        l1.set_line(x1, h1, len - item->circled_tip_radius() * 2, 0.0);
                        l2.set_line(x1, -h1, len - item->circled_tip_radius() * 2, 0.0);
                        PointF circled_tip;
                        circled_tip.set_x(len - item->circled_tip_radius());
                        circled_tip.set_y(0.0);
                        item->set_circled_tip(circled_tip);
                    } break;
                    case SpannerSegmentType::BEGIN:
                    case SpannerSegmentType::MIDDLE:
                        item->set_draw_circled_tip(false);
                        l1.set_line(x1, h1, len, +h2);
                        l2.set_line(x1, -h1, len, -h2);
                        break;
                }
            } break;
            default: break;
        }

        // Do Coord rotation
        l1 = t.map(l1);
        l2 = t.map(l2);
        if (item->draw_circled_tip()) {
            item->set_circled_tip(t.map(item->circled_tip()));
        }

        ldata->points[0] = l1.p1();
        ldata->points[1] = l1.p2();
        ldata->points[2] = l2.p1();
        ldata->points[3] = l2.p2();
        ldata->npoints = 4;

        RectF r = RectF(l1.p1(), l1.p2()).normalized().united(RectF(l2.p1(), l2.p2()).normalized());
        if (!item->text()->empty()) {
            r.unite(item->text()->ldata()->bbox());
        }
        if (!item->end_text()->empty()) {
            r.unite(item->end_text()->ldata()->bbox().translated(x + item->end_text()->ldata()->bbox().width(), 0.0));
        }
        double w = item->absolute_from_spatium(ctx.style().style_s(Sid::HAIRPIN_LINE_WIDTH));
        item->setbbox(r.adjusted(-w * .5, -w * .5, w, w));
    }

    item->set_pos(PointF());
    item->set_offset(PointF());
}

void SingleLayout::layout(HarpPedalDiagram *item, const Context &ctx) {
    item->update_diagram_text();
    layout_text_base(item, ctx, item->mutldata());
}

void SingleLayout::layout(Image *item, const Context &) {
    item->init();

    SizeF image_size = item->size();

    Image::LayoutData *ldata = item->mutldata();
    ldata->set_pos(PointF());
    ldata->set_bbox(RectF(PointF(), item->size2pixel(image_size)));
}

void SingleLayout::layout(InstrumentChange *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(Jump *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(KeySig *item, const Context &ctx) {
    KeySig::LayoutData *ldata = item->mutldata();
    double spatium = item->spatium();
    double step = spatium * 0.5;

    ldata->set_bbox(RectF());
    ldata->key_symbols.clear();

    // determine current clef for this staff
    ClefType clef = ClefType::G;

    int key = int(item->key());

    if (item->is_custom() && !item->is_atonal()) {
        Spatium accidental_gap = ctx.style().style_s(Sid::KEYSIG_ACCIDENTAL_DISTANCE);
        // add standard key accidentals first, if necessary
        for (int i = 1; i <= std::abs(key) && std::abs(key) <= 7; ++i) {
            bool drop = false;
            for (const CustDef &cd : item->custom_key_defs()) {
                int degree = item->deg_in_key(cd.degree);
                // if custom keysig accidental takes place, don't create tonal accidental
                if ((degree * 2 + 2) % 7 == (key < 0 ? 8 - i : i) % 7) {
                    drop = true;
                    break;
                }
            }
            if (!drop) {
                KeySym ks;
                int line_index_offset = key > 0 ? -1 : 6;
                ks.sym = key > 0 ? SymId::ACCIDENTAL_SHARP : SymId::ACCIDENTAL_FLAT;
                ks.line = ClefInfo::lines(clef)[line_index_offset + i];
                if (ldata->key_symbols.size() > 0) {
                    KeySym &previous = ldata->key_symbols.back();
                    Spatium previous_width = Spatium::from_absolute(item->sym_width(previous.sym), spatium);
                    ks.x_pos = previous.x_pos + previous_width + accidental_gap;
                } else {
                    ks.x_pos = 0_sp;
                }
                // TODO octave metters?
                ldata->key_symbols.push_back(ks);
            }
        }
        for (const CustDef &cd : item->custom_key_defs()) {
            SymId sym = item->sym_in_key(cd.sym, cd.degree);
            int degree = item->deg_in_key(cd.degree);
            bool flat = std::string(SymNames::name_for_sym_id(sym).ascii()).find("Flat") != std::string::npos;
            int acc_idx = (degree * 2 + 1) % 7; // C D E F ... index to F C G D index
            acc_idx = flat ? 13 - acc_idx : acc_idx;
            int line = ClefInfo::lines(clef)[acc_idx] + cd.oct_alt * 7;
            Spatium xpos = cd.x_alt;
            if (ldata->key_symbols.size() > 0) {
                KeySym &previous = ldata->key_symbols.back();
                Spatium previous_width = Spatium::from_absolute(item->sym_width(previous.sym), spatium);
                xpos += previous.x_pos + previous_width + accidental_gap;
            }
            // if translated symbol if out of range, add key accidental followed by untranslated symbol
            if (sym == SymId::NO_SYM) {
                KeySym ks;
                ks.line = line;
                ks.x_pos = xpos;
                // for quadruple sharp use two double sharps
                if (cd.sym == SymId::ACCIDENTAL_TRIPLE_SHARP) {
                    ks.sym = SymId::ACCIDENTAL_DOUBLE_SHARP;
                    sym = SymId::ACCIDENTAL_DOUBLE_SHARP;
                } else {
                    ks.sym = key > 0 ? SymId::ACCIDENTAL_SHARP : SymId::ACCIDENTAL_FLAT;
                    sym = cd.sym;
                }
                ldata->key_symbols.push_back(ks);
                xpos += Spatium(key < 0 ? 0.7 : 1); // flats closer
            }
            // create symbol; natural only if is user defined
            if (sym != SymId::ACCIDENTAL_NATURAL || sym == cd.sym) {
                KeySym ks;
                ks.sym = sym;
                ks.line = line;
                ks.x_pos = xpos;
                ldata->key_symbols.push_back(ks);
            }
        }
    } else {
        if (std::abs(key) <= 7) {
            const signed char *lines = ClefInfo::lines(clef);
            SymId sym = key > 0 ? SymId::ACCIDENTAL_SHARP : SymId::ACCIDENTAL_FLAT;
            Spatium accidental_gap = ctx.style().style_s(Sid::KEYSIG_ACCIDENTAL_DISTANCE);
            Spatium previous_width = Spatium::from_absolute(item->sym_width(sym), spatium);
            int line_index_offset = key > 0 ? 0 : 7;
            for (int i = 0; i < std::abs(key); ++i) {
                int line = lines[line_index_offset + i];
                KeySym ks;
                ks.sym = sym;
                Spatium x = 0.0_sp;
                if (ldata->key_symbols.size() > 0) {
                    const KeySym &previous = ldata->key_symbols.back();
                    x = previous.x_pos + previous_width + accidental_gap;
                    bool is_ascending = line < previous.line;
                    SmuflAnchorId current_cutout = is_ascending ? SmuflAnchorId::CUT_OUT_SW : SmuflAnchorId::CUT_OUT_NW;
                    SmuflAnchorId previous_cutout = is_ascending ? SmuflAnchorId::CUT_OUT_NE : SmuflAnchorId::CUT_OUT_SE;
                    PointF cutout = item->sym_smufl_anchor(sym, current_cutout);
                    double current_cutout_y = line * step + cutout.y();
                    double previous_cutout_y = previous.line * step + item->sym_smufl_anchor(previous.sym, previous_cutout).y();
                    if ((is_ascending && current_cutout_y < previous_cutout_y) || (!is_ascending && current_cutout_y > previous_cutout_y)) {
                        x -= Spatium::from_absolute(cutout.x(), spatium);
                    }
                }
                ks.x_pos = x;
                ks.line = line;
                ldata->key_symbols.push_back(ks);
            }
        } else {
            LOGD() << "illegal key:" << key;
        }
    }

    // compute bbox
    for (const KeySym &ks : ldata->key_symbols) {
        double x = ks.x_pos.to_absolute(spatium);
        double y = ks.line * step;
        ldata->add_bbox(item->sym_bbox(ks.sym).translated(x, y));
    }
}

void SingleLayout::layout(LayoutBreak *item, const Context &) {
    FontMetrics metrics(item->font());
    RectF bbox = metrics.bounding_rect(item->icon_code());
    item->mutldata()->set_shape(Shape(bbox, item));
}

void SingleLayout::layout(LetRing *item, const Context &ctx) { layout_line(item, ctx); }

void SingleLayout::layout(LetRingSegment *item, const Context &ctx) { layout_text_line_base_segment(item, ctx); }

void SingleLayout::layout(Lyrics *item, const Context &ctx) { layout_text_base(to_text_base(item), ctx, item->mutldata()); }

void SingleLayout::layout(NoteHead *item, const Context &ctx) { layout(static_cast<Symbol *>(item), ctx); }

void SingleLayout::layout(NoteLine *, const Context &) {
    MUSE_NOT_IMPLEMENTED;
    //! NOTE: NoteLines can be removed from disallowed elements in NotationInteraction::isOutgoingDragElementAllowed once this has been implemented
}

void SingleLayout::layout(Marker *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(MeasureNumber *item, const Context &ctx) {
    item->set_pos(PointF());
    item->set_offset(PointF());

    layout1_text_base(item, ctx, item->mutldata());
}

void SingleLayout::layout(MeasureRepeat *item, const Context &ctx) {
    MeasureRepeat::LayoutData *ldata = item->mutldata();
    switch (item->num_measures()) {
        case 1: {
            ldata->set_sym_id(SymId::REPEAT1_BAR);
            if (ctx.style().style_b(Sid::ONE_MEASURE_REPEAT_SHOW1)) {
                ldata->set_number_sym(1);
            } else {
                ldata->clear_number_sym();
            }
            break;
        }
        case 2:
            ldata->set_sym_id(SymId::REPEAT2_BARS);
            ldata->set_number_sym(item->num_measures());
            break;
        case 4:
            ldata->set_sym_id(SymId::REPEAT4_BARS);
            ldata->set_number_sym(item->num_measures());
            break;
        default:
            ldata->set_sym_id(SymId::NO_SYM); // should never happen
            ldata->clear_number_sym();
            break;
    }

    RectF bbox = item->sym_bbox(ldata->sym_id);
    ldata->set_bbox(bbox);
}

void SingleLayout::layout(Ornament *item, const Context &ctx) { layout(to_articulation(item), ctx); }

void SingleLayout::layout(Ottava *item, const Context &ctx) { layout_line(item, ctx); }

void SingleLayout::layout(OttavaSegment *item, const Context &ctx) {
    layout_text_line_base_segment(item, ctx);
    item->set_offset(PointF());
}

void SingleLayout::layout(PalmMute *item, const Context &ctx) { layout_line(item, ctx); }

void SingleLayout::layout(PalmMuteSegment *item, const Context &ctx) { layout_text_line_base_segment(item, ctx); }

void SingleLayout::layout(Pedal *item, const Context &ctx) { layout_line(item, ctx); }

void SingleLayout::layout(PedalSegment *item, const Context &ctx) {
    layout_text_line_base_segment(item, ctx);
    item->set_offset(PointF());
}

void SingleLayout::layout(PlayCountText *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(PlayTechAnnotation *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(RehearsalMark *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(Slur *item, const Context &) {
    double spatium = item->spatium();
    SlurSegment *s = nullptr;
    if (item->spanner_segments().empty()) {
        s = new SlurSegment(item);
        s->set_track(item->track());
        item->add(s);
    } else {
        s = item->front_segment();
    }

    s->set_spanner_segment_type(SpannerSegmentType::SINGLE);

    s->set_pos(PointF());
    s->ups(Grip::START).p = PointF(0, 0);
    s->ups(Grip::END).p = PointF(spatium * 6, 0);
    s->set_extra_height(0.0);

    SlurTieLayout::compute_bezier(s);

    item->setbbox(s->ldata()->bbox());
}

void SingleLayout::layout(Spacer *item, const Context &) {
    Spacer::LayoutData *ldata = item->mutldata();

    double spatium = item->spatium();

    PainterPath path = PainterPath();
    double w = spatium;
    double b = w * .5;
    double h = item->ownership_parent() ? item->absolute_gap() :
                                          item->absolute_from_spatium(std::min(item->gap(), 4.0_sp)); // limit length for palette

    switch (item->spacer_type()) {
        case SpacerType::DOWN:
            path.line_to(w, 0.0);
            path.move_to(b, 0.0);
            path.line_to(b, h);
            path.line_to(0.0, h - b);
            path.move_to(b, h);
            path.line_to(w, h - b);
            break;
        case SpacerType::UP:
            path.move_to(b, 0.0);
            path.line_to(0.0, b);
            path.move_to(b, 0.0);
            path.line_to(w, b);
            path.move_to(b, 0.0);
            path.line_to(b, h);
            path.move_to(0.0, h);
            path.line_to(w, h);
            break;
        case SpacerType::FIXED:
            path.line_to(w, 0.0);
            path.move_to(b, 0.0);
            path.line_to(b, h);
            path.move_to(0.0, h);
            path.line_to(w, h);
            break;
    }
    ldata->path = path;
    double lw = spatium * 0.4;
    RectF bb(0, 0, w, h);
    bb.adjust(-lw, -lw, lw, lw);
    ldata->set_bbox(bb);

    item->set_z(0.0);
}

void SingleLayout::layout(StaffText *item, const Context &ctx) {
    layout_text_base(item, ctx, item->mutldata());

    if (item->has_sound_flag()) {
        RectF bbox = item->ldata()->bbox();
        double icon_height = bbox.height();
        RectF icon_b_box = RectF(bbox.x(), bbox.y(), icon_height, icon_height);
        item->sound_flag()->mutldata()->set_bbox(icon_b_box);

        double x_move = icon_b_box.width() + icon_b_box.width() / 2.0;
        bbox.set_width(bbox.width() + x_move);
        item->setbbox(bbox);

        layout(item->sound_flag(), ctx);

        for (TextBlock &block : item->mutldata()->blocks) {
            auto &fragments = block.fragments();
            for (std::list<TextFragment>::iterator it = fragments.begin(); it != fragments.end(); ++it) {
                it->pos.set_x(it->pos.x() + x_move);
            }
        }
    }
}

void SingleLayout::layout(StaveSharingLabel *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(StaffTypeChange *item, const Context &ctx) {
    double spatium = ctx.style().spatium();
    item->setbbox(RectF(-item->lw() * .5, -item->lw() * .5, spatium * 2.5 + item->lw(), spatium * 2.5 + item->lw()));
    item->set_pos(0.0, 0.0);
}

void SingleLayout::layout(StringTunings *item, const Context &ctx) {
    layout_text_base(item, ctx, item->mutldata());

    double spatium = item->spatium();
    for (TextBlock &block : item->mutldata()->blocks) {
        for (TextFragment &fragment : block.fragments()) {
            Font font = fragment.font(item);
            if (font.type() != Font::Type::MUSIC_SYMBOL) {
                // HACK: the music symbol doesn't have a good baseline
                // to go with text so we correct text here
                const double baseline_adjustment = 0.35 * spatium * item->symbol_scale();
                fragment.pos.set_y(fragment.pos.y() - baseline_adjustment);
            }
        }
    }
}

void SingleLayout::layout(Symbol *item, const Context &) {
    item->setbbox(item->score_font() ? item->score_font()->bbox(item->sym(), item->mag_s()) : item->sym_bbox(item->sym()));
    item->set_offset(0.0, 0.0);
    item->set_pos(0.0, 0.0);
}

void SingleLayout::layout(SystemText *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(Tapping *item, const Context &ctx) {
    TappingText *text = item->text();

    if (!text) {
        text = new TappingText(item);
    }

    item->set_text(text);
    text->set_track(item->track());
    DO_ASSERT(item->hand() != TappingHand::INVALID);
    text->set_xml_text(item->hand() == TappingHand::LEFT ? "l.h. tap" : "r.h. tap");
    text->set_align(Align(AlignH::HCENTER, AlignV::BASELINE));

    layout_text_base(text, ctx, text->mutldata());

    item->setbbox(text->ldata()->bbox());
}

void SingleLayout::layout(SoundFlag *item, const Context &ctx) {
    UNUSED(item);
    UNUSED(ctx);
}

void SingleLayout::layout(Stem *item, const Context &ctx) {
    LayoutContext tctx(ctx.dont_use_score());
    TLayout::layout_stem(item, item->mutldata(), tctx.conf());
}

void SingleLayout::layout(Sticking *item, const Context &ctx) { layout_text_base(to_text_base(item), ctx, item->mutldata()); }

void SingleLayout::layout(TempoText *item, const Context &ctx) { layout_text_base(item, ctx, item->mutldata()); }

void SingleLayout::layout(TextLine *item, const Context &ctx) { layout_line(item, ctx); }

void SingleLayout::layout(TextLineSegment *item, const Context &ctx) {
    layout_text_line_base_segment(item, ctx);
    item->set_offset(PointF());
}

void SingleLayout::layout(TimeSig *item, const Context &ctx) {
    TimeSig::LayoutData *ldata = item->mutldata();

    double spatium = item->spatium();

    ldata->set_pos(0.0, 0.0);
    ldata->set_bbox(RectF()); // prepare for an empty time signature

    constexpr double line_dist = 1.0;
    constexpr int num_of_lines = 5;
    TimeSigType sig_type = item->time_sig_type();

    // if some symbol
    // compute vert. displacement to center in the staff height
    // determine middle staff position:

    double yoff = spatium * (num_of_lines - 1) * 0.5 * line_dist;

    // C and Ccut are placed at the middle of the staff: use yoff directly
    IEngravingFontPtr font = ctx.engraving_font();
    SizeF mag(item->mag_s() * item->scale());

    if (sig_type == TimeSigType::FOUR_FOUR) {
        ldata->pz = PointF(0.0, yoff);
        RectF bbox = font->bbox(SymId::TIME_SIG_COMMON, mag);
        ldata->set_bbox(bbox.translated(ldata->pz));
        ldata->ns.clear();
        ldata->ns.push_back(SymId::TIME_SIG_COMMON);
        ldata->ds.clear();
    } else if (sig_type == TimeSigType::ALLA_BREVE) {
        ldata->pz = PointF(0.0, yoff);
        RectF bbox = font->bbox(SymId::TIME_SIG_CUT_COMMON, mag);
        ldata->set_bbox(bbox.translated(ldata->pz));
        ldata->ns.clear();
        ldata->ns.push_back(SymId::TIME_SIG_CUT_COMMON);
        ldata->ds.clear();
    } else if (sig_type == TimeSigType::CUT_BACH) {
        ldata->pz = PointF(0.0, yoff);
        RectF bbox = font->bbox(SymId::TIME_SIG_CUT2, mag);
        ldata->set_bbox(bbox.translated(ldata->pz));
        ldata->ns.clear();
        ldata->ns.push_back(SymId::TIME_SIG_CUT2);
        ldata->ds.clear();
    } else if (sig_type == TimeSigType::CUT_TRIPLE) {
        ldata->pz = PointF(0.0, yoff);
        RectF bbox = font->bbox(SymId::TIME_SIG_CUT3, mag);
        ldata->set_bbox(bbox.translated(ldata->pz));
        ldata->ns.clear();
        ldata->ns.push_back(SymId::TIME_SIG_CUT3);
        ldata->ds.clear();
    } else {
        if (item->numerator_string().is_empty()) {
            ldata->ns = time_sig_sym_ids_from_string(item->numerator_string().is_empty() ? String::number(item->sig().numerator()) :
                                                                                           item->numerator_string());

            ldata->ds = time_sig_sym_ids_from_string(item->denominator_string().is_empty() ? String::number(item->sig().denominator()) :
                                                                                             item->denominator_string());
        } else {
            ldata->ns = time_sig_sym_ids_from_string(item->numerator_string());
            ldata->ds = time_sig_sym_ids_from_string(item->denominator_string());
        }

        RectF num_rect = font->bbox(ldata->ns, mag);
        RectF den_rect = font->bbox(ldata->ds, mag);

        // position numerator and denominator; vertical displacement:
        // number of lines is odd: 0.0 (strings are directly above and below the middle line)
        // number of lines even:   0.05 (strings are moved up/down to leave 1/10sp between them)

        double displ = (num_of_lines & 1) ? 0.0 : (0.05 * spatium);

        //align on the wider
        double pz_y = yoff - (den_rect.width() < 0.01 ? 0.0 : (displ + num_rect.height() * 0.5));
        double pn_y = yoff + displ + den_rect.height() * 0.5;

        if (num_rect.width() >= den_rect.width()) {
            // numerator: one space above centre line, unless denomin. is empty (if so, directly centre in the middle)
            ldata->pz = PointF(0.0, pz_y);
            // denominator: horiz: centred around centre of numerator | vert: one space below centre line
            ldata->pn = PointF((num_rect.width() - den_rect.width()) * 0.5, pn_y);
        } else {
            // numerator: one space above centre line, unless denomin. is empty (if so, directly centre in the middle)
            ldata->pz = PointF((den_rect.width() - num_rect.width()) * 0.5, pz_y);
            // denominator: horiz: centred around centre of numerator | vert: one space below centre line
            ldata->pn = PointF(0.0, pn_y);
        }

        // centering of parenthesis so the middle of the parenthesis is at the divisor marking level
        int center_y = yoff / 2 + spatium;
        int widest_portion = num_rect.width() > den_rect.width() ? num_rect.width() : den_rect.width();
        ldata->point_large_left_paren = PointF(-spatium, center_y);
        ldata->point_large_right_paren = PointF(widest_portion + spatium, center_y);

        ldata->set_bbox(num_rect.translated(ldata->pz)); // translate bounding boxes to actual string positions
        ldata->add_bbox(den_rect.translated(ldata->pn));
        if (item->large_parentheses()) {
            ldata->add_bbox(RectF(ldata->point_large_left_paren.x(), ldata->point_large_left_paren.y() - den_rect.height(), spatium / 2,
                                  num_rect.height() + den_rect.height()));
            ldata->add_bbox(RectF(ldata->point_large_right_paren.x(), ldata->point_large_right_paren.y() - den_rect.height(), spatium / 2,
                                  num_rect.height() + den_rect.height()));
        }
    }
}

void SingleLayout::layout(TremoloSingleChord *item, const Context &) {
    item->compute_shape(); // set bbox
    item->set_path(item->base_path());
}

void SingleLayout::layout(TremoloTwoChord *item, const Context &) {
    item->compute_shape(); // set bbox
    item->set_path(item->base_path());
}

void SingleLayout::layout(TremoloBar *item, const Context &) {
    TremoloBar::LayoutData *ldata = item->mutldata();
    double spatium = item->spatium();

    ldata->set_pos(PointF());

    double time_factor = item->user_mag() / 1.0;
    double pitch_factor = -spatium * 0.02;

    PolygonF polygon;
    for (const PitchValue &v : item->points()) {
        polygon << PointF(v.time * time_factor, v.pitch * pitch_factor);
    }
    ldata->polygon = polygon;

    const double w = item->absolute_from_spatium(item->line_width());
    ldata->set_bbox(ldata->polygon.bounding_rect().adjusted(-w, -w, w, w));
}

void SingleLayout::layout(Trill *item, const Context &ctx) { layout_line(static_cast<SLine *>(item), ctx); }

void SingleLayout::layout(TrillSegment *item, const Context &ctx) {
    TrillSegment::LayoutData *ldata = item->mutldata();
    if (item->spanner()->place_below()) {
        ldata->set_pos_y(0.0);
    }

    bool accidental_goes_below = item->trill()->trill_type() == TrillType::DOWNPRALL_LINE;
    Trill *trill = item->trill();
    Ornament *ornament = trill->ornament();
    if (ornament) {
        if (item->is_single_begin_type()) {
            layout(ornament, ctx);
        }
        trill->set_accidental(accidental_goes_below ? ornament->accidental_below() : ornament->accidental_above());
        trill->set_cue_note_chord(ornament->cue_note_chord());
        ArticulationAnchor anchor = ornament->anchor();
        if (anchor == ArticulationAnchor::AUTO) {
            trill->set_placement(trill->track() % 2 ? PlacementV::BELOW : PlacementV::ABOVE);
        } else {
            trill->set_placement(anchor == ArticulationAnchor::TOP ? PlacementV::ABOVE : PlacementV::BELOW);
        }
        trill->set_property_flags(Pid::PLACEMENT,
                                  PropertyFlags::STYLED); // Ensures that the property isn't written (it is written by the ornamnent)
    }

    if (item->is_single_type() || item->is_begin_type()) {
        switch (item->trill()->trill_type()) {
            case TrillType::TRILL_LINE: item->symbol_line(SymId::ORNAMENT_TRILL, SymId::WIGGLE_TRILL); break;
            case TrillType::PRALLPRALL_LINE: item->symbol_line(SymId::WIGGLE_TRILL, SymId::WIGGLE_TRILL); break;
            case TrillType::UPPRALL_LINE:
                item->symbol_line(SymId::ORNAMENT_BOTTOM_LEFT_CONCAVE_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
                                  SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END);
                break;
            case TrillType::DOWNPRALL_LINE:
                item->symbol_line(SymId::ORNAMENT_LEFT_VERTICAL_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
                                  SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END);
                break;
        }
        Accidental *a = item->trill()->accidental();
        if (a) {
            double vert_margin = 0.35 * item->spatium();
            RectF box = item->sym_bbox(item->symbols().front());
            double x = 0;
            double y = 0;
            x = 0.5 * (box.width() - a->width());
            double min_vert_dist =
                accidental_goes_below ? Shape(box).min_vertical_distance(a->shape()) : a->shape().min_vertical_distance(Shape(box));
            y = accidental_goes_below ? min_vert_dist + vert_margin : -min_vert_dist - vert_margin;
            a->set_pos(x, y);
            a->set_ownership_parent(item);
        }
    } else {
        item->symbol_line(SymId::WIGGLE_TRILL, SymId::WIGGLE_TRILL);
    }

    item->set_offset(PointF());
}

void SingleLayout::layout(Vibrato *item, const Context &ctx) { layout_line(static_cast<SLine *>(item), ctx); }

void SingleLayout::layout(VibratoSegment *item, const Context &) {
    VibratoSegment::LayoutData *ldata = item->mutldata();
    if (item->spanner()->place_below()) {
        ldata->set_pos_y(0.0);
    }

    switch (item->vibrato()->vibrato_type()) {
        case VibratoType::GUITAR_VIBRATO: item->symbol_line(SymId::GUITAR_VIBRATO_STROKE, SymId::GUITAR_VIBRATO_STROKE); break;
        case VibratoType::GUITAR_VIBRATO_WIDE:
            item->symbol_line(SymId::GUITAR_WIDE_VIBRATO_STROKE, SymId::GUITAR_WIDE_VIBRATO_STROKE);
            break;
        case VibratoType::VIBRATO_SAWTOOTH: item->symbol_line(SymId::WIGGLE_SAWTOOTH, SymId::WIGGLE_SAWTOOTH); break;
        case VibratoType::VIBRATO_SAWTOOTH_WIDE: item->symbol_line(SymId::WIGGLE_SAWTOOTH_WIDE, SymId::WIGGLE_SAWTOOTH_WIDE); break;
        default: break;
    }

    item->set_offset(PointF());
}

void SingleLayout::layout(Volta *item, const Context &ctx) { layout_line(item, ctx); }

void SingleLayout::layout(WhammyBar *item, const Context &ctx) { layout_line(item, ctx); }

void SingleLayout::layout(VoltaSegment *item, const Context &ctx) {
    layout_text_line_base_segment(item, ctx);
    item->set_offset(PointF());

    double spatium = ctx.style().spatium();
    double hook_height = item->absolute_from_spatium(item->volta()->begin_hook_height());
    if (item->text()) {
        Text *text = item->text();
        RectF text_b_box = text->ldata()->bbox().translated(text->pos());
        text->mutldata()->move_y(hook_height - text_b_box.bottom());
        text->mutldata()->move_x(0.5 * spatium);
    }
}

void SingleLayout::layout(WhammyBarSegment *item, const Context &ctx) {
    layout_text_line_base_segment(item, ctx);
    item->set_offset(PointF());
}

void SingleLayout::layout(Text *item, const Context &ctx) { layout_text_base(to_text_base(item), ctx, item->mutldata()); }

void SingleLayout::layout_text_base(const TextBase *item, const Context &ctx, TextBase::LayoutData *ldata) {
    ldata->set_pos(PointF());
    const_cast<TextBase *>(item)->set_offset(PointF());

    if (item->place_below()) {
        ldata->set_pos_y(0.0);
    }

    layout1_text_base(item, ctx, ldata);
}

static void text_horizontal_layout(const TextBase *item, Shape &shape, double max_block_width, TextBase::LayoutData *ldata) {
    // Position and alignment
    for (size_t i = 0; i < ldata->blocks.size(); ++i) {
        TextBlock &text_block = ldata->blocks[i];
        double x_adj = -text_block.bounding_rect().left();

        // Set position relative to reference point
        AlignH position = item->position();
        if (position == AlignH::HCENTER) {
            x_adj -= max_block_width * .5;
        } else if (position == AlignH::RIGHT) {
            x_adj -= max_block_width;
        }

        double diff = max_block_width - text_block.bounding_rect().width();
        if (muse::real_is_null(diff)) {
            // This is the longest line, don't align
            for (TextFragment &f : text_block.fragments()) {
                f.pos.rx() += x_adj;
            }
            text_block.shape().translate(PointF(x_adj, 0.0));
            shape.add(text_block.shape().translated(PointF(0.0, text_block.y())));
            continue;
        }
        // Align relative to the longest line
        AlignH align_h = item->align().horizontal;
        if (align_h == AlignH::HCENTER) {
            x_adj += diff * 0.5;
        } else if (align_h == AlignH::RIGHT) {
            x_adj += diff;
        }

        for (TextFragment &fragment : text_block.fragments()) {
            fragment.pos.rx() += x_adj;
        }
        text_block.shape().translate(PointF(x_adj, 0.0));
        shape.add(text_block.shape().translated(PointF(0.0, text_block.y())));
    }
}

void SingleLayout::layout1_text_base(const TextBase *item, const Context &, TextBase::LayoutData *ldata) {
    if (ldata->layout_invalid) {
        item->create_blocks(ldata);
    }

    double y = 0;

    double max_block_width = -DBL_MAX;
    // adjust the bounding box for the text item
    for (size_t i = 0; i < ldata->rows(); ++i) {
        TextBlock &t = ldata->blocks[i];
        TextLayout::layout_text_block(&t, item);
        const RectF *r = &t.bounding_rect();

        if (r->height() == 0) {
            r = &ldata->blocks.at(0).bounding_rect();
        }
        y += t.line_spacing();
        t.set_y(y);
        max_block_width = std::max(max_block_width, t.bounding_rect().width());
    }

    Shape shape;
    text_horizontal_layout(item, shape, max_block_width, ldata);

    RectF bb = shape.bbox();

    double yoff = 0;
    double h = 0;

    ldata->set_pos(PointF());

    if (item->align() == AlignV::BOTTOM) {
        yoff += h - bb.bottom();
    } else if (item->align() == AlignV::VCENTER) {
        yoff += (h - (bb.top() + bb.bottom())) * .5;
    } else if (item->align() == AlignV::BASELINE) {
        yoff += h * .5 - ldata->blocks.front().line_spacing();
    } else {
        yoff += -bb.top();
    }

    for (TextBlock &t : ldata->blocks) {
        t.set_y(t.y() + yoff);
    }

    shape.translate_y(yoff);
    ldata->set_shape(shape);

    if (item->has_frame()) {
        item->layout_frame(ldata);
    }

    if (!muse::real_is_null(item->text_angle())) {
        Transform t;
        t.rotate(item->text_angle());
        ldata->set_shape(shape.transform(t));
        ldata->high_res_shape.mut_value().transform(t);
    }
}

void SingleLayout::layout_line(SLine *item, const Context &ctx) {
    if (item->spanner_segments().empty()) {
        item->set_len(ctx.style().spatium() * 8);
    }

    LineSegment *line_segm = item->front_segment();
    layout_line_segment(line_segm, ctx);

    item->setbbox(line_segm->ldata()->bbox(LdAccess::BAD));
}

// Extends lines to fill the corner between them.
// Assumes that l1p2 == l2p1 is the intersection between the lines.
// If checkAngle is false, assumes that the lines are perpendicular,
// and some calculations are saved.
static inline void extend_lines(const PointF &l1p1, PointF &l1p2, PointF &l2p1, const PointF &l2p2, double line_width, bool check_angle) {
    PointF l1_unit_vector = (l1p2 - l1p1).normalized();
    PointF l2_unit_vector = (l2p1 - l2p2).normalized();

    double added_length = line_width * 0.5;

    if (check_angle) {
        double angle = M_PI - acos(PointF::dot_product(l1_unit_vector, l2_unit_vector));

        if (angle <= M_PI_2) {
            added_length *= tan(0.5 * angle);
        }
    }

    l1p2 += l1_unit_vector * added_length;
    l2p1 += l2_unit_vector * added_length;
}

static PolygonF create_arrow(bool start, bool filled, PointF &start_point, PointF &end_point, const TextLineBase *tl) {
    double arrow_width = 0.0;
    double arrow_height = 0.0;
    if (filled) {
        arrow_width = tl->absolute_from_spatium(start ? tl->begin_filled_arrow_width() : tl->end_filled_arrow_width());
        arrow_height = tl->absolute_from_spatium(start ? tl->begin_filled_arrow_height() : tl->end_filled_arrow_height());
    } else {
        arrow_width = tl->absolute_from_spatium(start ? tl->begin_line_arrow_width() : tl->end_line_arrow_width());
        arrow_height = tl->absolute_from_spatium(start ? tl->begin_line_arrow_height() : tl->end_line_arrow_height());
    }

    PolygonF arrow;
    if (start) {
        arrow << PointF(0.0, -arrow_height / 2) << PointF(-arrow_width, 0.0) << PointF(0.0, arrow_height / 2); // left
    } else {
        arrow << PointF(0.0, -arrow_height / 2) << PointF(arrow_width, 0.0) << PointF(0.0, arrow_height / 2); // right
    }

    PointF arrow_adjust = PointF(arrow_width, 0.0);
    arrow_adjust = (start ? 1.0 : -1.0) * arrow_adjust;
    arrow.translate(arrow_adjust);

    const double y_diff = end_point.y() - start_point.y();
    const double x_diff = end_point.x() - start_point.x();
    const double rotate = atan(y_diff / x_diff) + (end_point.x() < start_point.x() ? M_PI : 0.0);

    Transform t;
    t.rotate_radians(rotate);

    for (PointF &p : arrow) {
        p = t.map(p);
    }

    if (start) {
        arrow.translate(start_point);
    } else {
        arrow.translate(end_point);
    }

    const PointF line_vector = end_point - start_point;
    const PointF unit_vector = line_vector.normalized();
    const double reduction = filled ? arrow_width : tl->absolute_from_spatium(tl->line_width()) / 2;
    if (start) {
        start_point += reduction * unit_vector;
    } else {
        end_point -= reduction * unit_vector;
    }

    return arrow;
}

void SingleLayout::layout_text_line_base_segment(TextLineBaseSegment *item, const Context &ctx) {
    TextLineBaseSegment::LayoutData *ldata = item->mutldata();
    ldata->npoints = 0;
    const TextLineBase *tl = item->text_line_base();
    const double spatium = tl->spatium();
    const bool is_single_or_begin = item->is_single_begin_type();
    const bool is_single_or_end = item->is_single_end_type();
    const bool is_dotted_line = tl->line_style() == LineType::DOTTED;
    const double line_width = tl->absolute_from_spatium(tl->line_width());

    if (!tl->diagonal()) {
        item->set_user_yoffset2(0);
    }

    auto align_base_line = [line_width](Text *text, PointF &pp1, PointF &pp2) {
        PointF width_correction(0.0, line_width / 2);
        switch (text->align().vertical) {
            case AlignV::TOP:
                pp1 += width_correction;
                pp2 += width_correction;
                break;
            case AlignV::VCENTER: break;
            case AlignV::BOTTOM:
                pp1 -= width_correction;
                pp2 -= width_correction;
                break;
            case AlignV::BASELINE:
                pp1 -= width_correction;
                pp2 -= width_correction;
                break;
        }
    };

    if (is_single_or_begin) {
        item->text()->set_xml_text(tl->begin_text());
        item->text()->set_family(tl->begin_font_family());
        item->text()->set_size(tl->begin_font_size());
        item->text()->set_symbol_scale(tl->begin_text_musical_symbols_scale());
        item->text()->set_symbol_size(tl->begin_text_music_symbols_size());
        item->text()->set_offset(tl->begin_text_offset() * item->mag());
        item->text()->set_align(tl->begin_text_align());
        item->text()->set_position(tl->begin_text_position());
        item->text()->set_font_style(tl->begin_font_style());
    } else {
        item->text()->set_xml_text(tl->continue_text());
        item->text()->set_family(tl->continue_font_family());
        item->text()->set_size(tl->continue_font_size());
        item->text()->set_symbol_scale(tl->continue_text_musical_symbols_scale());
        item->text()->set_symbol_size(tl->continue_text_music_symbols_size());
        item->text()->set_offset(tl->continue_text_offset() * item->mag());
        item->text()->set_align(tl->continue_text_align());
        item->text()->set_position(tl->continue_text_position());
        item->text()->set_font_style(tl->continue_font_style());
    }
    item->text()->set_placement(PlacementV::ABOVE);
    item->text()->set_track(item->track());
    item->text()->set_color(tl->line_color());
    layout(item->text(), ctx);

    if (is_single_or_end) {
        item->end_text()->set_xml_text(tl->end_text());
        item->end_text()->set_family(tl->end_font_family());
        item->end_text()->set_size(tl->end_font_size());
        item->end_text()->set_symbol_scale(tl->end_text_musical_symbols_scale());
        item->end_text()->set_symbol_size(tl->end_text_music_symbols_size());
        item->end_text()->set_offset(tl->end_text_offset() * item->mag());
        item->end_text()->set_align(tl->end_text_align());
        item->end_text()->set_position(tl->end_text_position());
        item->end_text()->set_font_style(tl->end_font_style());
        item->end_text()->set_placement(PlacementV::ABOVE);
        item->end_text()->set_track(item->track());
        item->end_text()->set_color(tl->line_color());
        layout(item->end_text(), ctx);
    } else {
        item->end_text()->set_xml_text(u"");
    }

    if (!item->text_line_base()->text_size_spatium_dependent()) {
        item->text()->set_size(item->text()->size() * item->default_spatium() / item->spatium());
        item->end_text()->set_size(item->end_text()->size() * item->default_spatium() / item->spatium());
    }

    PointF pp1;
    PointF pp2(item->pos2());

    // line with no text or hooks - just use the basic rectangle for line
    if (item->text()->empty() && item->end_text()->empty() && (!is_single_or_begin || tl->begin_hook_type() == HookType::NONE) &&
        (!is_single_or_end || tl->end_hook_type() == HookType::NONE)) {
        ldata->npoints = 2;
        ldata->points[0] = pp1;
        ldata->points[1] = pp2;
        ldata->line_length = sqrt(PointF::dot_product(pp2 - pp1, pp2 - pp1));

        item->setbbox(TextLineBaseSegment::bounding_box_of_line(pp1, pp2, line_width / 2, is_dotted_line));
        return;
    }

    double l1 = 0.0;
    double l2 = 0.0;
    const double gap_between_text_and_line = spatium * tl->gap_between_text_and_line().val();

    const bool align_begin_text = tl->begin_text_place() == TextPlace::LEFT || tl->begin_text_place() == TextPlace::AUTO;
    const bool align_continue_text = tl->continue_text_place() == TextPlace::LEFT || tl->continue_text_place() == TextPlace::AUTO;
    const bool align_end_text = tl->end_text_place() == TextPlace::LEFT || tl->end_text_place() == TextPlace::AUTO;
    const bool has_begin_text = !item->text()->empty() && is_single_or_begin;
    const bool has_continue_text = !item->text()->empty() && !is_single_or_begin;
    const bool has_end_text = !item->end_text()->empty() && is_single_or_end;

    Shape shape;

    if (!item->text()->empty()) {
        if ((is_single_or_begin && align_begin_text) || (!is_single_or_begin && align_continue_text)) {
            l1 = gap_between_text_and_line;
            switch (item->text()->position()) {
                case AlignH::LEFT: l1 += item->text()->ldata()->bbox().width(); break;
                case AlignH::HCENTER: l1 += item->text()->ldata()->bbox().width() / 2; break;
                default: break;
            }
        }
        shape.add(item->text()->ldata()->bbox().translated(item->text()->pos()), item);
    }
    // set end text position and extend bbox
    if (!item->end_text()->empty()) {
        if (align_end_text) {
            l2 = gap_between_text_and_line;
            switch (item->end_text()->position()) {
                case AlignH::RIGHT: l2 += item->end_text()->ldata()->bbox().width(); break;
                case AlignH::HCENTER: l2 += item->end_text()->ldata()->bbox().width() / 2; break;
                default: break;
            }
        }
        const double end_text_x = std::max(pp2.x(), shape.bbox().right() + l2); // prevent end text from overlapping begin text
        item->end_text()->mutldata()->set_pos_x(end_text_x);
        shape.add(item->end_text()->ldata()->bbox().translated(item->end_text()->pos()), item);
    }

    bool begin_hook_drawn_separately = false;
    bool end_hook_drawn_separately = false;

    if (tl->line_visible()) {
        pp1.rx() += l1;
        pp2.rx() -= l2;

        // Make sure baseline of text and line are properly aligned (accounting for line thickness)
        if ((has_begin_text && align_begin_text) || (has_continue_text && align_continue_text)) {
            align_base_line(item->text(), pp1, pp2);
        } else if (has_end_text && align_end_text) {
            align_base_line(item->end_text(), pp1, pp2);
        }

        const double begin_hook_height = (tl->place_below() ? -1.0 : 1.0) * tl->begin_hook_height().val() * spatium;
        const double end_hook_height = (tl->place_below() ? -1.0 : 1.0) * tl->end_hook_height().val() * spatium;

        double begin_hook_width = 0.0;
        double end_hook_width = 0.0;

        if (tl->begin_hook_type() == HookType::HOOK_45) {
            begin_hook_width = std::fabs(begin_hook_height * .4);
            pp1.rx() += begin_hook_width;
        }

        if (tl->end_hook_type() == HookType::HOOK_45) {
            end_hook_width = std::fabs(end_hook_height * .4);
            pp2.rx() -= end_hook_width;
        }

        // don't draw backwards lines (or hooks) if text is longer than nominal line length
        if (!item->text()->empty() && pp1.x() > pp2.x() && !tl->diagonal()) {
            ldata->set_shape(shape);
            return;
        }

        const bool begin_arrow =
            is_single_or_begin && (tl->begin_hook_type() == HookType::ARROW_FILLED || tl->begin_hook_type() == HookType::ARROW);
        const bool end_arrow =
            is_single_or_end && (tl->end_hook_type() == HookType::ARROW_FILLED || tl->end_hook_type() == HookType::ARROW);

        if (begin_arrow) {
            const bool filled = tl->begin_hook_type() == HookType::ARROW_FILLED;
            ldata->begin_arrow = create_arrow(true, filled, pp1, pp2, tl);
            shape.add(ldata->begin_arrow.bounding_rect(), item);
        }
        if (end_arrow) {
            const bool filled = tl->end_hook_type() == HookType::ARROW_FILLED;
            ldata->end_arrow = create_arrow(false, filled, pp1, pp2, tl);
            shape.add(ldata->end_arrow.bounding_rect(), item);
        }

        auto has_hook = [](HookType type) -> bool {
            switch (type) {
                case HookType::HOOK_45:
                case HookType::HOOK_90:
                case HookType::HOOK_90T: return true;
                case HookType::NONE:
                case HookType::ARROW:
                case HookType::ARROW_FILLED:
                case HookType::ROSETTE: return false;
                default: break;
            }
            return false;
        };

        const bool begin_hook = is_single_or_begin && has_hook(tl->begin_hook_type());
        const bool end_hook = is_single_or_end && has_hook(tl->end_hook_type());

        if (begin_hook) {
            // We use the term "endpoint" for the point that does not touch the main line.
            const PointF &begin_hook_endpoint = ldata->points[ldata->npoints++] =
                PointF(pp1.x() - begin_hook_width, pp1.y() + begin_hook_height);
            if (tl->begin_hook_type() == HookType::HOOK_90T) {
                // A T-hook needs to be drawn separately, so we add an extra point
                begin_hook_drawn_separately = true;
                ldata->points[ldata->npoints++] = PointF(pp1.x() - begin_hook_width, pp1.y() - begin_hook_height);
            } else if (tl->line_style() != LineType::SOLID) {
                // For non-solid lines, we also draw the hook separately,
                // so that we can distribute the dashes/dots for each linepiece individually
                begin_hook_drawn_separately = true;
                PointF &begin_hook_startpoint = ldata->points[ldata->npoints++] = pp1;

                if (tl->line_style() == LineType::DASHED) {
                    // For dashes lines, we extend the lines somewhat,
                    // so that the corner between them gets filled
                    bool check_angle = tl->begin_hook_type() == HookType::HOOK_45 || tl->diagonal();
                    extend_lines(begin_hook_endpoint, begin_hook_startpoint, pp1, pp2, line_width, check_angle);
                }
            }
        }

        ldata->points[ldata->npoints++] = pp1;
        PointF &pp22 = ldata->points[ldata->npoints++] = pp2; // Keep a reference so that we can modify later

        if (end_hook) {
            const PointF end_hook_endpoint = PointF(pp2.x() + end_hook_width, pp2.y() + end_hook_height);
            if (tl->end_hook_type() == HookType::HOOK_90T) {
                // A T-hook needs to be drawn separately, so we add an extra point
                end_hook_drawn_separately = true;
                ldata->points[ldata->npoints++] = PointF(pp2.x() + end_hook_width, pp2.y() - end_hook_height);
            } else if (tl->line_style() != LineType::SOLID) {
                // For non-solid lines, we also draw the hook separately,
                // so that we can distribute the dashes/dots for each linepiece individually
                end_hook_drawn_separately = true;
                PointF &end_hook_startpoint = ldata->points[ldata->npoints++] = pp2;

                if (tl->line_style() == LineType::DASHED) {
                    // For dashes lines, we extend the lines somewhat,
                    // so that the corner between them gets filled
                    bool check_angle = tl->end_hook_type() == HookType::HOOK_45 || tl->diagonal();
                    extend_lines(pp1, pp22, end_hook_startpoint, end_hook_endpoint, line_width, check_angle);
                }
            }

            ldata->points[ldata->npoints++] = end_hook_endpoint;
        }

        ldata->line_length = sqrt(PointF::dot_product(pp22 - pp1, pp22 - pp1));
    }

    // Calculate shape of line
    {
        int start = 0;
        if (begin_hook_drawn_separately) {
            PointF &p1 = ldata->points[start++];
            PointF &p2 = ldata->points[start++];
            shape.add(TextLineBaseSegment::bounding_box_of_line(p1, p2, line_width / 2, is_dotted_line), item);
        }

        int end = ldata->npoints;
        if (end_hook_drawn_separately) {
            PointF &p1 = ldata->points[--end];
            PointF &p2 = ldata->points[--end];
            shape.add(TextLineBaseSegment::bounding_box_of_line(p1, p2, line_width / 2, is_dotted_line), item);
        }

        for (int i = start; i < end - 1; ++i) {
            shape.add(TextLineBaseSegment::bounding_box_of_line(ldata->points[i], ldata->points[i + 1], line_width / 2, is_dotted_line),
                      item, !tl->line_visible());
        }
    }

    ldata->set_shape(shape);
}
