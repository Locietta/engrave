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
#include "chordlayout.h"

#include <cfloat>

#include "accidentalslayout.h"
#include "arpeggiolayout.h"
#include "autoplace.h"
#include "beamlayout.h"
#include "beamtremololayout.h"
#include "containers.h"
#include "dom/accidental.h"
#include "dom/arpeggio.h"
#include "dom/beam.h"
#include "dom/chord.h"
#include "dom/chordbracket.h"
#include "dom/factory.h"
#include "dom/fingering.h"
#include "dom/glissando.h"
#include "dom/guitarbend.h"
#include "dom/hook.h"
#include "dom/ledgerline.h"
#include "dom/lyrics.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/ornament.h"
#include "dom/page.h"
#include "dom/parenthesis.h"
#include "dom/part.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/slur.h"
#include "dom/staff.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/system.h"
#include "dom/tie.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/utils.h"
#include "editing/editchord.h"
#include "editing/navigation.h"
#include "horizontalspacing.h"
#include "log.h"
#include "parenthesislayout.h"
#include "restlayout.h"
#include "slurtielayout.h"
#include "stemlayout.h"
#include "systemlayout.h"
#include "tlayout.h"
#include "tremololayout.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

static constexpr Spatium stafftype_tab_defaultdotdist_x = 0.75_sp;

void ChordLayout::layout(Chord *item, LayoutContext &ctx) {
    if (item->notes().empty()) {
        return;
    }
    int gi = 0;
    for (Chord *c : item->grace_notes()) {
        // HACK: graceIndex is not well-maintained on add & remove
        // so rebuild now
        c->set_grace_index(gi++);
    }

    if (item->on_tab_staff()) {
        layout_tablature(item, ctx);
    } else {
        layout_pitched(item, ctx);
    }
}

void ChordLayout::layout_pitched(Chord *item, LayoutContext &ctx) {
    for (Chord *c : item->grace_notes()) {
        layout_pitched(c, ctx);
    }

    double mag_value = item->staff() ? item->staff()->staff_mag(item) : 1.0; // palette elements do not have a staff
    double dot_note_distance = ctx.conf().style_absolute(Sid::DOT_NOTE_DISTANCE) * mag_value;

    double chord_x = (item->note_type() == NoteType::NORMAL) ? item->ldata()->pos().x() : 0.0;

    double lll = 0.0;   // space to leave at left of chord
    double rrr = 0.0;   // space to leave at right of chord
    double lhead = 0.0; // amount of notehead to left of chord origin
    Note *upnote = item->up_note();
    Note *downnote = item->down_note();
    Note *left_note = nullptr;

    delete item->tab_dur(); // no TAB? no duration symbol! (may happen when converting a TAB into PITCHED)
    item->set_tab_dur(nullptr);

    //-----------------------------------------
    //  process notes
    //-----------------------------------------

    // Keeps track if there are any accidentals in this chord.
    // Used to remove excess space in front of arpeggios.
    // See GitHub issue #8970 for more details.
    // https://github.com/musescore/MuseScore/issues/8970
    std::vector<Accidental *> chord_accidentals;

    for (Note *note : item->notes()) {
        TLayout::layout_note(note, note->mutldata());
        double x1 = note->pos().x() + chord_x;
        double x2 = x1 + note->head_width();
        lll = std::max(lll, -x1);
        rrr = std::max(rrr, x2);
        // track amount of space due to notehead only
        lhead = std::max(lhead, -x1);
        if (!left_note || note->x() < left_note->x()) {
            left_note = note;
        }

        Accidental *accidental = note->accidental();
        if (accidental && accidental->visible()) {
            chord_accidentals.push_back(accidental);
        }
        if (accidental && accidental->add_to_skyline() && !note->fixed()) {
            // convert x position of accidental to segment coordinate system
            double x = accidental->pos().x() + note->pos().x() + chord_x;
            // distance from accidental to note already taken into account
            // but here perhaps we create more padding in *front* of accidental?
            x -= ctx.conf().style_absolute(Sid::ACCIDENTAL_DISTANCE) * mag_value;
            lll = std::max(lll, -x);
        }

        // clear layout for note-based fingerings
        for (EngravingItem *e : note->el()) {
            if (e->is_fingering()) {
                Fingering *f = to_fingering(e);
                if (f->layout_type() == ElementType::NOTE) {
                    f->set_pos(PointF());
                    f->setbbox(RectF());
                }
            }
        }
    }

    // A chord can have its own arpeggio and also be part of another arpeggio's span.  We need to lay out both of these arpeggios properly
    Arpeggio *old_span_arp = item->span_arpeggio();
    Arpeggio *new_span_arp = nullptr;

    // If item has an arpeggio: mark chords which are part of the arpeggio
    if (item->arpeggio()) {
        item->arpeggio()->find_and_attach_to_chords();
        item->arpeggio()->mutldata()->max_chord_pad = 0.0;
        item->arpeggio()->mutldata()->min_chord_x = DBL_MAX;
        TLayout::layout_item(item->arpeggio(), ctx);
    }

    if (item->span_arpeggio() != old_span_arp) {
        new_span_arp = item->span_arpeggio();
    }
    // If item is within arpeggio span, keep track of largest space needed between glissando and chord across staves
    double lll_max = lll;
    for (Arpeggio *span_arp : {old_span_arp, new_span_arp}) {
        if (!span_arp || !span_arp->chord()) {
            continue;
        }

        Arpeggio::LayoutData *arpldata = span_arp->mutldata();

        const Segment *seg = span_arp->chord()->segment();
        const EngravingItem *end_item = seg->element(span_arp->end_track());
        const Chord *end_chord = item;
        if (end_item && end_item->is_chord()) {
            end_chord = to_chord(end_item);
        }

        // If a note is covered in the voice span but located outside the visual span of the arpeggio calculate accidental offset later
        bool above_start =
            std::make_pair(item->v_staff_idx(), item->down_line()) < std::make_pair(span_arp->v_staff_idx(), span_arp->chord()->up_line());
        bool below_end =
            std::make_pair(item->v_staff_idx(), item->up_line()) > std::make_pair(end_chord->v_staff_idx(), end_chord->down_line());

        if (!(above_start || below_end)) {
            ElementType el_type = span_arp->type();
            const PaddingTable &padding_table = item->score()->padding_tables().horizontal_padding_table();
            double arpeggio_note_distance = padding_table.at(el_type).at(ElementType::NOTE) * mag_value;
            double arpeggio_ledger_distance = padding_table.at(el_type).at(ElementType::LEDGER_LINE) * mag_value;
            int first_ledger_below = item->staff()->lines(item->down_note()->tick()) * 2 - 1;
            int first_ledger_above = -1;

            double gap_size = arpeggio_note_distance;

            if (left_note && muse::real_is_null(left_note->x())) {
                if (downnote->line() > first_ledger_below || upnote->line() < first_ledger_above) {
                    gap_size = arpeggio_ledger_distance + ctx.conf().style_s(Sid::LEDGER_LINE_LENGTH).val() * item->spatium();
                }
            } else if (left_note && (left_note->line() > first_ledger_below || left_note->line() < first_ledger_above)) {
                gap_size = arpeggio_ledger_distance + ctx.conf().style_s(Sid::LEDGER_LINE_LENGTH).val() * item->spatium();
            }

            double arp_chord_x = std::min(chord_x, 0.0);

            if (!chord_accidentals.empty()) {
                double arpeggio_accidental_distance = padding_table.at(el_type).at(ElementType::ACCIDENTAL) * mag_value;
                double accidental_distance = ctx.conf().style_absolute(Sid::ACCIDENTAL_DISTANCE) * mag_value;
                gap_size = arpeggio_accidental_distance - accidental_distance;
                gap_size -= ArpeggioLayout::inset_distance(span_arp, ctx, mag_value, item, chord_accidentals);
            }

            double extra_x = span_arp->width() + gap_size;

            // Track leftmost chord position, as we we always want the arpeggio to be to the left of this
            arpldata->min_chord_x = std::min(arpldata->min_chord_x, arp_chord_x);

            // Save this to arpeggio if largest
            arpldata->max_chord_pad = std::max(arpldata->max_chord_pad, lll + extra_x);

            // If first chord in arpeggio set y
            if (item->arpeggio() && item->arpeggio() == span_arp) {
                double y1 = upnote->pos().y() - upnote->head_height() * .5;
                item->arpeggio()->mutldata()->set_pos_y(y1);
            }

            Note *end_down_note = end_chord->down_note();

            // If last chord in arpeggio, set x
            if (end_down_note->track() == item->track()) {
                // Amount to move arpeggio from it's parent chord to factor in chords further to the left
                double first_chord_x = span_arp->chord()->ldata()->pos().x();
                double x_diff = first_chord_x - arpldata->min_chord_x;

                double offset = -(x_diff + arpldata->max_chord_pad);
                span_arp->mutldata()->set_pos_x(offset);
                if (span_arp->visible()) {
                    lll_max = std::max(lll_max, offset);
                }
            }
        }
    }

    lll = lll_max;

    if (item->dots()) {
        double x =
            item->dot_pos_x() + dot_note_distance + double(item->dots() - 1) * ctx.conf().style_absolute(Sid::DOT_DOT_DISTANCE) * mag_value;
        x += item->sym_width(SymId::AUGMENTATION_DOT);
        rrr = std::max(rrr, x);
    }

    if (item->hook()) {
        if (item->beam()) {
            ctx.mut_dom().undo_remove_element(item->hook());
        } else {
            TLayout::layout_hook(item->hook(), item->hook()->mutldata());
            if (item->up() && item->stem()) {
                // hook position is not set yet
                double x = item->hook()->ldata()->bbox().right() + item->stem()->flag_position().x() + chord_x;
                rrr = std::max(rrr, x);
            }
        }
    }

    item->set_space_lw(lll);
    item->set_space_rw(rrr);

    for (Note *note : item->notes()) {
        layout_note2(note, ctx);
    }

    for (EngravingItem *e : item->el()) {
        // Cannot layout slurs as chord positions are not fixed
        // Chord brackets should be the outermost element
        if (e->is_slur() || e->is_chord_bracket()) {
            continue;
        }
        TLayout::layout_item(e, ctx);
        if (e->is_chord_line()) {
            RectF tbbox = e->ldata()->bbox().translated(e->pos());
            double lx = tbbox.left() + chord_x;
            double rx = tbbox.right() + chord_x;
            if (-lx > item->space_lw()) {
                item->set_space_lw(item->space_lw() - lx);
            }
            if (rx > item->space_rw()) {
                item->set_space_rw(rx);
            }
        }
    }

    // align note-based fingerings
    std::vector<Fingering *> align_note;
    double x_note = DBL_MAX;
    for (Note *note : item->notes()) {
        bool left_found = false;
        for (EngravingItem *e : note->el()) {
            if (e->is_fingering() && e->autoplace()) {
                Fingering *f = to_fingering(e);
                if (f->layout_type() == ElementType::NOTE && f->text_style_type() == TextStyleType::LH_GUITAR_FINGERING) {
                    align_note.push_back(f);
                    if (!left_found) {
                        left_found = true;
                        double xf = f->ldata()->pos().x();
                        x_note = std::min(x_note, xf);
                    }
                }
            }
        }
    }
    for (Fingering *f : align_note) {
        f->mutldata()->set_pos_x(x_note);
    }

    layout_lv_articulation(item, ctx);

    create_paren_groups(item);
    ParenthesisLayout::layout_chord_parentheses(item, ctx);

    fill_shape(item, item->mutldata(), ctx.conf());
}

void ChordLayout::layout_tablature(Chord *item, LayoutContext &ctx) {
    double spatium_value = item->spatium();
    double mag_value = item->staff() ? item->staff()->staff_mag(item) : 1.0; // palette elements do not have a staff
    double dot_note_distance = ctx.conf().style_absolute(Sid::DOT_NOTE_DISTANCE) * mag_value;
    double min_note_distance = ctx.conf().style_absolute(Sid::MIN_NOTE_DISTANCE) * mag_value;
    double min_tie_length = ctx.conf().style_absolute(Sid::MIN_TIE_LENGTH) * mag_value;

    for (Chord *c : item->grace_notes()) {
        layout_tablature(c, ctx);
    }

    double lll = 0.0; // space to leave at left of chord
    double rrr = 0.0; // space to leave at right of chord
    Note *upnote = item->up_note();
    double head_width = item->sym_width(SymId::NOTEHEAD_BLACK);
    const Staff *st = item->staff();
    const StaffType *tab = st->staff_type_for_element(item);
    double line_dist_value = tab->line_distance().val() * spatium_value;
    double stem_x = 0.5 * head_width;
    int ledger_lines = 0;
    double ll_y = 0.0;

    size_t num_of_notes = item->notes().size();
    double min_y = 1000.0; // just a very large value
    for (size_t i = 0; i < num_of_notes; ++i) {
        Note *note = item->notes().at(i);
        note->update_fretting_for_ties_and_bends();
        ChordLayout::set_dot_relative_line(note, 0, ctx);
        TLayout::layout_note(note, note->mutldata());
        // set headWidth to max fret text width
        double fret_width = note->ldata()->bbox().width();
        if (head_width < fret_width) {
            head_width = fret_width;
        }
        // centre fret string on stem
        double x = stem_x - fret_width * 0.5;
        double y =
            note->fixed() ? note->line() * line_dist_value / 2 : tab->phys_string_to_y_offset(note->string()).to_absolute(spatium_value);
        note->set_pos(x, y);
        if (y < min_y) {
            min_y = y;
        }
        int curr_ledger_lines = tab->num_of_tab_ledger_lines(note->string());
        if (curr_ledger_lines > ledger_lines) {
            ledger_lines = curr_ledger_lines;
            ll_y = y;
        }

        // allow extra space for shortened ties; this code must be kept synchronized
        // with the tie positioning code in Tie::slurPos()
        // but the allocation of space needs to be performed here
        Tie *tie;
        tie = note->tie_back();
        if (tie && tie->add_to_skyline()) {
            SlurTieLayout::calculate_direction(tie);
            double overlap = 0.0;     // how much tie can overlap start and end notes
            bool short_start = false; // whether tie should clear start note or not
            Note *start_note = tie->start_note();
            Chord *start_chord = start_note ? start_note->chord() : nullptr;
            if (start_chord && start_chord->measure() == item->measure() && start_chord == Navigation::prev_chord_rest(item)) {
                double start_note_width = start_note->width();
                // overlap into start chord?
                // if in start chord, there are several notes or stem and tie in same direction
                if (start_chord->notes().size() > 1 || (start_chord->stem() && start_chord->up() == tie->up())) {
                    // clear start note (1/8 of fret mark width)
                    short_start = true;
                    overlap -= start_note_width * 0.125;
                } else { // overlap start note (by ca. 1/3 of fret mark width)
                    overlap += start_note_width * 0.35;
                }
                // overlap into end chord (this)?
                // if several notes or neither stem or tie are up
                if (item->notes().size() > 1 || (item->stem() && !item->up() && !tie->up())) {
                    // for positive offset:
                    //    use available space
                    // for negative x offset:
                    //    space is allocated elsewhere, so don't re-allocate here
                    if (!real_is_null(note->ldata()->pos().x())) {     // this probably does not work for TAB, as
                        overlap += std::abs(note->ldata()->pos().x()); // _pos is used to centre the fret on the stem
                    } else {
                        overlap -= fret_width * 0.125;
                    }
                } else {
                    if (short_start) {
                        overlap += fret_width * 0.15;
                    } else {
                        overlap += fret_width * 0.35;
                    }
                }
                double d = std::max(min_tie_length - overlap, 0.0);
                lll = std::max(lll, d);
            }
        }
    }

    // create ledger lines, if required (in some historic styles)
    if (ledger_lines > 0) {
        item->resize_ledger_lines_to(ledger_lines);
        double extra_len = 0;
        double ll_x = stem_x - (head_width + extra_len) * 0.5;
        for (int i = 0; i < ledger_lines; i++) {
            LedgerLine *ldg_lin = item->ledger_lines()[i];
            ldg_lin->set_track(item->track());
            ldg_lin->set_visible(item->visible());
            ldg_lin->set_len(head_width + extra_len);
            ldg_lin->set_pos(ll_x, ll_y);
            TLayout::layout_ledger_line(ldg_lin, ctx);
            ll_y += line_dist_value / ledger_lines;
        }
        head_width += extra_len; // include ledger lines extra width in chord width
    }

    // horiz. spacing: leave half width at each side of the (potential) stem
    double half_head_width = head_width * 0.5;
    if (lll < stem_x - half_head_width) {
        lll = stem_x - half_head_width;
    }
    if (rrr < stem_x + half_head_width) {
        rrr = stem_x + half_head_width;
    }
    // align dots to the widest fret mark (not needed in all TAB styles, but harmless anyway)
    item->set_dot_pos_x(head_width);

    if (item->should_have_stem()) {
        // if stem is required but missing, add it;
        // set stem position (stem length is set in Chord:layoutStem() )
        if (!item->stem()) {
            Stem *stem = Factory::create_stem(item);
            stem->set_generated(true);
            ctx.mut_dom().add_element(stem);
        }
        item->stem()->set_pos(StemLayout::tab_stem_pos(item, tab) * spatium_value);
        if (item->hook()) {
            if (item->beam()) {
                ctx.mut_dom().undo_remove_element(item->hook());
            } else {
                if (rrr < stem_x + item->hook()->width()) {
                    rrr = stem_x + item->hook()->width();
                }

                item->hook()->set_pos(item->stem()->flag_position());
            }
        }
    } else {
        if (item->stem()) {
            ctx.mut_dom().do_undo_remove_element(item->stem());
            item->remove(item->stem());
        }
        if (item->hook()) {
            ctx.mut_dom().do_undo_remove_element(item->hook());
            item->remove(item->hook());
        }
        if (item->beam()) {
            ctx.mut_dom().do_undo_remove_element(item->beam());
            item->remove(item->beam());
        }
    }

    if (!tab->gen_durations()                          // if tab is not set for duration symbols
        || track2voice(item->track())                  // or not in first voice
        || (item->is_grace()                           // no tab duration symbols if grace notes
            && item->beam_mode() == BeamMode::AUTO)) { // and beammode == AUTO
        //
        delete item->tab_dur(); // delete an existing duration symbol
        item->set_tab_dur(nullptr);
    } else {
        //
        // tab duration symbols
        //
        // if no previous CR
        // OR symbol repeat set to ALWAYS
        // OR symbol repeat condition is triggered
        // OR duration type and/or number of dots is different from current CR
        // OR chord beam mode not AUTO
        // OR previous CR is a rest
        // AND no not-stem
        // set a duration symbol (trying to re-use existing symbols where existing to minimize
        // symbol creation and deletion)
        bool need_tab_dur = false;
        bool repeat = false;
        if (!item->no_stem()) {
            // check duration of prev. CR segm
            ChordRest *prev_cr = Navigation::prev_chord_rest(item);
            if (prev_cr == 0) {
                need_tab_dur = true;
            } else if (item->beam_mode() != BeamMode::AUTO || prev_cr->duration_type().type() != item->duration_type().type() ||
                       prev_cr->dots() != item->dots() || prev_cr->tuplet() != item->tuplet() || prev_cr->is_rest()) {
                need_tab_dur = true;
            } else if (tab->sym_repeat() == TablatureSymbolRepeat::ALWAYS ||
                       ((tab->sym_repeat() == TablatureSymbolRepeat::MEASURE || tab->sym_repeat() == TablatureSymbolRepeat::SYSTEM) &&
                        item->measure() != prev_cr->measure())) {
                need_tab_dur = true;
                repeat = true;
            }
        }
        if (need_tab_dur) {
            // symbol needed; if not exist, create; if exists, update duration
            if (!item->tab_dur()) {
                item->set_tab_dur(new TabDurationSymbol(item, tab, item->duration_type().type(), item->dots()));
            } else {
                item->tab_dur()->set_duration(item->duration_type().type(), item->dots(), tab);
            }
            item->tab_dur()->set_repeat(repeat);
            //                  _tabDur->setMag(mag());           // useless to set grace mag: graces have no dur. symbol
            TLayout::layout_tab_duration_symbol(item->tab_dur(), item->tab_dur()->mutldata());
            if (min_y < 0) { // if some fret extends above tab body (like bass strings)
                TabDurationSymbol::LayoutData *tdladata = item->tab_dur()->mutldata();
                tdladata->move_y(min_y); // raise duration symbol
                tdladata->set_bbox(tdladata->bbox().translated(0, min_y));
            }
        } else { // symbol not needed: if exists, delete
            delete item->tab_dur();
            item->set_tab_dur(nullptr);
        }
    } // end of if(duration_symbols)

    if (item->arpeggio()) {
        item->arpeggio()->find_and_attach_to_chords();
        double y = upnote->pos().y() - upnote->head_height() * .5;
        TLayout::layout_arpeggio(item->arpeggio(), item->arpeggio()->mutldata(), ctx.conf());
        lll += item->arpeggio()->width() + spatium_value * .5;
        item->arpeggio()->set_pos(-lll, y);

        // handle the special case of _arpeggio->span() > 1
        // in layoutArpeggio2() after page layout has done so we
        // know the y position of the next staves
    }

    // allocate enough room for glissandi
    if (item->ends_note_anchored_line()) {
        if (!item->rtick().is_zero()) { // if not at beginning of measure
            lll += spatium_value * 0.5 + min_tie_length;
        }
        // special case of system-initial glissando final note is handled in Glissando::layout() itself
    }

    if (item->hook()) {
        if (item->beam()) {
            ctx.mut_dom().undo_remove_element(item->hook());
        } else if (tab == 0) {
            TLayout::layout_hook(item->hook(), item->hook()->mutldata());
            if (item->up()) {
                // hook position is not set yet
                double x = item->hook()->ldata()->bbox().right() + item->stem()->flag_position().x();
                rrr = std::max(rrr, x);
            }
        }
    }

    if (item->dots()) {
        double x = 0.0;
        // if stems are beside staff, dots are placed near to stem
        if (!tab->stem_through()) {
            // if there is an unbeamed hook, dots should start after the hook
            if (item->hook() && !item->beam()) {
                x = item->hook()->width() + dot_note_distance;
            }
            // if not, dots should start at a fixed distance right after the stem
            else {
                x = ::stafftype_tab_defaultdotdist_x.to_absolute(spatium_value);
            }
            item->set_dot_pos_x(x);
        }
        // if stems are through staff, use dot position computed above on fret mark widths
        else {
            x = item->dot_pos_x() + dot_note_distance +
                (item->dots() - 1) * ctx.conf().style_s(Sid::DOT_DOT_DISTANCE).val() * spatium_value;
        }
        x += item->sym_width(SymId::AUGMENTATION_DOT);
        rrr = std::max(rrr, x);
    }

    item->set_space_lw(lll);
    item->set_space_rw(rrr);

    double grace_mag = ctx.conf().style_d(Sid::GRACE_NOTE_MAG);

    std::vector<Chord *> grace_notes_before = item->Chord::grace_notes_before();
    size_t nb = grace_notes_before.size();
    if (nb) {
        double xl = -(item->space_lw() + min_note_distance);
        for (int i = static_cast<int>(nb) - 1; i >= 0; --i) {
            Chord *c = grace_notes_before.at(i);
            xl -= c->space_rw() /* * 1.2*/;
            c->set_pos(xl, 0);
            xl -= c->space_lw() + min_note_distance * grace_mag;
        }
        if (-xl > item->space_lw()) {
            item->set_space_lw(item->space_lw() - xl);
        }
    }
    std::vector<Chord *> gna = item->grace_notes_after();
    size_t na = gna.size();
    if (na) {
        // get factor for start distance after main note. Values found by testing.
        double fc;
        switch (item->duration_type().type()) {
            case DurationType::V_LONG: fc = 3.8; break;
            case DurationType::V_BREVE: fc = 3.8; break;
            case DurationType::V_WHOLE: fc = 3.8; break;
            case DurationType::V_HALF: fc = 3.6; break;
            case DurationType::V_QUARTER: fc = 2.1; break;
            case DurationType::V_EIGHTH: fc = 1.4; break;
            case DurationType::V_16TH: fc = 1.2; break;
            default: fc = 1;
        }
        double xr = fc * (item->space_rw() + min_note_distance);
        for (int i = 0; i <= static_cast<int>(na) - 1; i++) {
            Chord *c = gna.at(i);
            xr += c->space_lw() * (i == 0 ? 1.3 : 1);
            c->set_pos(xr, 0);
            xr += c->space_rw() + min_note_distance * grace_mag;
        }
        if (xr > item->space_rw()) {
            item->set_space_rw(xr);
        }
    }
    for (EngravingItem *e : item->el()) {
        TLayout::layout_item(e, ctx);
        if (e->is_chord_line()) {
            RectF tbbox = e->ldata()->bbox().translated(e->pos());
            double lx = tbbox.left();
            double rx = tbbox.right();
            if (-lx > item->space_lw()) {
                item->set_space_lw(item->space_lw() - lx);
            }
            if (rx > item->space_rw()) {
                item->set_space_rw(rx);
            }
        }
    }

    for (size_t i = 0; i < num_of_notes; ++i) {
        layout_note2(item->notes().at(i), ctx);
    }

    if (item->stem_slash()) {
        TLayout::layout_stem_slash(item->stem_slash(), item->stem_slash()->mutldata(), ctx.conf());
    }

    layout_lv_articulation(item, ctx);

    create_paren_groups(item);
    ParenthesisLayout::layout_chord_parentheses(item, ctx);

    fill_shape(item, item->mutldata(), ctx.conf());
}

void ChordLayout::layout_lv_articulation(Chord *item, LayoutContext &ctx) {
    // Laissez vib ties must be layed out with the rest of the note, as they need to be taken into account for horizontal spacing
    for (Articulation *a : item->articulations()) {
        if (a->is_laissez_vib()) {
            // Must set direction before laying out
            if (item->measure()->has_voices(a->staff_idx(), item->tick(), item->actual_ticks()) &&
                a->anchor() == ArticulationAnchor::AUTO) {
                a->set_up(item->up()); // if there are voices place articulation at stem
            } else if (a->anchor() == ArticulationAnchor::AUTO) {
                a->set_up(!item->up()); // place articulation at note head
            } else {
                a->set_up(a->anchor() == ArticulationAnchor::TOP);
            }

            TLayout::layout_item(a, ctx);

            const Articulation::LayoutData *ldata = a->ldata();
            const double sp = a->spatium();
            const Note *note = ldata->up() ? item->up_note() : item->down_note();
            const int up_dir = ldata->up() ? -1 : 1;
            const double note_height = note->height();

            PointF result = note->pos() + item->pos();
            double center = SlurTieLayout::note_optical_center_for_tie(note, ldata->up);
            Spatium visual_inset_sp = 0.0_sp;
            if (note->head_group() == NoteHeadGroup::HEAD_SLASH || note->should_hide_fret()) {
                visual_inset_sp = 0.2_sp;
            } else if (item->up() && ldata->up) {
                visual_inset_sp = 0.7_sp;
            } else {
                visual_inset_sp = 0.1_sp;
            }
            double visual_inset = visual_inset_sp.to_absolute(sp);

            result.rx() += center + visual_inset - ldata->bbox().left();
            result.ry() += (note_height / 2 + 0.2 * sp) * up_dir;

            a->set_pos(result);
        }
    }
}

//---------------------------------------------------------
//   layoutNotesSpanners
//---------------------------------------------------------

void ChordLayout::layout_spanners(Chord *item, LayoutContext &ctx) {
    for (const Note *n : item->notes()) {
        Tie *tie = n->tie_for();
        if (tie) {
            TLayout::layout_tie(tie, ctx);
        }
        for (Spanner *sp : n->spanner_back()) {
            TLayout::layout_spanner(sp, ctx);
        }
        for (Spanner *sp : n->spanner_for()) {
            if ((sp->is_glissando() || sp->is_guitar_bend()) && sp->tick2() >= ctx.state().page()->end_tick()) {
                TLayout::layout_spanner(sp, ctx);
            }
        }
    }
}

//---------------------------------------------------------
//   layoutArticulations
//    layout tenuto and staccato
//    called before layouting slurs
//---------------------------------------------------------

void ChordLayout::layout_articulations(Chord *item, LayoutContext &ctx) {
    for (Chord *gc : item->grace_notes()) {
        layout_articulations(gc, ctx);
    }

    if (item->articulations().empty()) {
        return;
    }
    const Staff *st = item->staff();
    const StaffType *staff_type = st->staff_type_for_element(item);
    double mag = (staff_type->is_small() ? ctx.conf().style_d(Sid::SMALL_STAFF_MAG) : 1.0) * staff_type->user_mag();
    double spatium_value = ctx.conf().spatium() * mag;
    double line_dist_value = spatium_value * staff_type->line_distance().val() / 2;
    const double min_dist = ctx.conf().style_absolute(Sid::ARTICULATION_MIN_DISTANCE) * mag;
    const ArticulationStemSideAlign articulation_h_align =
        ctx.conf().style_v(Sid::ARTICULATION_STEM_H_ALIGN).value<ArticulationStemSideAlign>();
    const bool keep_artics_together = ctx.conf().style_b(Sid::ARTICULATION_KEEP_TOGETHER);
    const double stem_side_distance = ctx.conf().style_absolute(Sid::PROPERTY_DISTANCE_STEM) * mag;
    const double head_side_distance = ctx.conf().style_absolute(Sid::PROPERTY_DISTANCE_HEAD) * mag;
    const double tenuto_additional_tie_distance = 0.6 * spatium_value;
    const double staccato_additional_tie_distance = 0.4 * spatium_value;

    int num_close_artics = 0;
    bool has_staff_artics_up = false;
    bool has_staff_artics_down = false;
    if (keep_artics_together) {
        // find out how many close-to-note artics there are, and whether there is a staff-anchored artic to align to
        for (Articulation *a : item->articulations()) {
            if (!a->visible()) {
                continue;
            }
            if (a->layout_close_to_note()) {
                ++num_close_artics;
            } else {
                if (a->up()) {
                    has_staff_artics_up = true;
                } else {
                    has_staff_artics_down = true;
                }
                if (has_staff_artics_down && has_staff_artics_up) {
                    break; // nothing more to be learned now
                }
            }
        }
    }

    //
    //    determine direction
    //    place tenuto and staccato
    //

    Articulation *prev_visible_articulation = nullptr;
    for (Articulation *a : item->articulations()) {
        if (item->measure()->has_voices(a->staff_idx(), item->tick(), item->actual_ticks()) && a->anchor() == ArticulationAnchor::AUTO) {
            a->set_up(item->up()); // if there are voices place articulation at stem
        } else if (a->anchor() == ArticulationAnchor::AUTO) {
            if (a->is_ornament()) {
                bool odd_voice = item->track() % 2;
                a->set_up(!odd_voice);
            } else if (a->sym_id() >= SymId::ARTIC_MARCATO_ABOVE && a->sym_id() <= SymId::ARTIC_MARCATO_TENUTO_BELOW) {
                a->set_up(true); // Gould, p. 117: strong accents above staff
            } else {
                a->set_up(!item->up()); // place articulation at note head
            }
        } else {
            a->set_up(a->anchor() == ArticulationAnchor::TOP);
        }

        if (!a->layout_close_to_note()) {
            continue;
        }

        bool bottom = !a->up();       // true: articulation is below chord;  false: articulation is above chord
        TLayout::layout_item(a, ctx); // must be done after assigning direction, or else symId is not reliable
        bool leave_tie_space = leave_space_for_tie(a);

        bool head_side = bottom == item->up();
        double y = 0.0;
        double x;
        if ((!head_side || !a->is_basic_articulation()) && item->stem()) {
            switch (articulation_h_align) {
                case ArticulationStemSideAlign::STEM: x = item->stem()->width() * .5; break;
                case ArticulationStemSideAlign::NOTEHEAD: x = item->up() ? item->down_note()->notehead_center_x() : center_x(item); break;
                case ArticulationStemSideAlign::AVERAGE:
                default:
                    x = item->up() ? (item->stem()->width() * .5 + item->down_note()->notehead_center_x()) * .5 :
                                     (item->stem()->width() * .5 + center_x(item)) * .5;
                    break;
            }
            if (item->up()) {
                x = item->down_note()->pos().x() + item->down_note()->bbox_right_pos() - x;
            }
        } else {
            x = center_x(item);
        }

        if (bottom) {
            if (!head_side && item->stem()) {
                double stem_bottom = item->stem()->ldata()->bbox().translated(item->stem()->pos()).bottom();
                // Check if there's a hook, because the tip of the hook always extends slightly past the end of the stem
                if (item->hook()) {
                    y = item->hook()->ldata()->bbox().translated(item->hook()->pos()).bottom();
                } else {
                    y = item->stem()->ldata()->bbox().translated(item->stem()->pos()).bottom();
                }
                int line = round((y + line_dist_value) / line_dist_value);
                int lines = 2 * (staff_type->lines() - 1);
                if (line < lines && !(line % 2)) {
                    line += 1;
                }
                double dist = (line * line_dist_value) - stem_bottom;
                bool has_beam = item->beam() || (item->tremolo_two_chord());
                if (line < lines && has_beam && dist < stem_side_distance) {
                    // beams can give stems weird unpredictable lengths, so we should enforce min
                    // distance even inside the staff

                    // adjust downwards a space but stop at the bottom of the staff
                    line = std::min(line + 2, lines);
                }
                if (line < lines) { // align between staff lines
                    y = line * line_dist_value;
                    y -= a->height() * .5;
                } else if (line == lines) {
                    y = stem_side_distance + (lines * line_dist_value);
                } else {
                    y += stem_side_distance;
                }
            } else {
                x = center_x(item);
                int lines = (staff_type->lines() - 1) * 2;
                int line = std::max(item->down_line(), -1);
                bool adjust_artic = (a->up() && has_staff_artics_up) || (!a->up() && has_staff_artics_down);
                if (keep_artics_together && adjust_artic && num_close_artics > 0) {
                    line = std::max(line, lines - (1 + (num_close_artics * 2)));
                }
                if (line < lines - 1) {
                    if (leave_tie_space && line % 2) {
                        line += 1;
                    }
                    y = ((line & ~1) + 3) * line_dist_value;
                    y -= a->height() * .5;
                } else {
                    y = item->down_pos() + 0.5 * item->down_note()->head_height() + head_side_distance;
                    if (leave_tie_space) {
                        y += a->is_staccato() ? staccato_additional_tie_distance : tenuto_additional_tie_distance;
                    }
                }
            }
            if (prev_visible_articulation && (prev_visible_articulation->up() == a->up())) {
                int staff_bottom = (staff_type->lines() - 2) * 2;
                if ((head_side && item->down_line() < staff_bottom) ||
                    (!head_side && !muse::real_is_equal_or_more(y, (staff_bottom + 1) * line_dist_value))) {
                    y += spatium_value;
                } else {
                    y += prev_visible_articulation->height() + min_dist;
                }
            }
            // center symbol
        } else { // topside
            if (!head_side && item->stem()) {
                double stem_top = item->stem()->ldata()->bbox().translated(item->stem()->pos()).top();
                // Check if there's a hook, because the tip of the hook always extends slightly past the end of the stem
                if (item->hook()) {
                    y = item->hook()->ldata()->bbox().translated(item->hook()->pos()).top();
                } else {
                    y = item->stem()->ldata()->bbox().translated(item->stem()->pos()).top();
                }
                int line = round((y - line_dist_value) / line_dist_value);
                if (line > 0 && !(line % 2)) {
                    line -= 1;
                }
                double dist = stem_top - (line * line_dist_value);
                bool has_beam = item->beam() || item->tremolo_two_chord();
                if (line > 0 && has_beam && dist < stem_side_distance) {
                    // beams can give stems weird unpredictable lengths, so we should enforce min
                    // distance even inside the staff

                    // adjust upwards a space but stop at the top of the staff
                    line = std::max(line - 2, 0);
                }

                if (line > 0) { // align between staff lines
                    y = line * line_dist_value;
                    y += a->height() * .5;
                } else if (line == 0) {
                    y = -stem_side_distance;
                } else {
                    y -= stem_side_distance;
                }
            } else {
                x = center_x(item);
                int lines = (staff_type->lines() - 1) * 2;
                int line = std::min(item->up_line(), lines + 1);
                bool adjust_artic = (a->up() && has_staff_artics_up) || (!a->up() && has_staff_artics_down);
                if (keep_artics_together && adjust_artic && num_close_artics > 0) {
                    line = std::min(line, 1 + (num_close_artics * 2));
                }
                if (line > 1) {
                    if (leave_tie_space && line % 2) {
                        line -= 1;
                    }
                    y = (((line + 1) & ~1) - 3) * line_dist_value;
                    y += a->height() * .5;
                } else {
                    y = item->up_pos() - 0.5 * item->down_note()->head_height() - head_side_distance;
                    if (leave_tie_space) {
                        y -= a->is_staccato() ? staccato_additional_tie_distance : tenuto_additional_tie_distance;
                    }
                }
            }
            if (prev_visible_articulation && (prev_visible_articulation->up() == a->up())) {
                if ((head_side && item->up_line() > 2) || (!head_side && !muse::real_is_equal_or_less(y, 0.0))) {
                    y -= item->spatium();
                } else {
                    y -= prev_visible_articulation->height() + min_dist;
                }
            }
        }
        x -= a->ldata()->optical_center();
        a->set_pos(x, y);
        if (a->visible()) {
            prev_visible_articulation = a;
        }
        //            measure()->system()->staff(a->staffIdx())->skyline().add(a->shape().translated(a->pos() + segment()->pos() + measure()->pos()));
    }
}

//---------------------------------------------------------
//   layoutArticulations2
//    Called after layouting systems
//    Tentatively layout all articulations
//    To be finished after laying out slurs
//---------------------------------------------------------

void ChordLayout::layout_articulations2(Chord *item, LayoutContext &ctx, bool layout_on_cross_beam_side) {
    ArticulationStemSideAlign articulation_h_align = ctx.conf().style_v(Sid::ARTICULATION_STEM_H_ALIGN).value<ArticulationStemSideAlign>();
    for (Chord *gc : item->grace_notes()) {
        layout_articulations2(gc, ctx);
    }

    if (item->articulations().empty()) {
        return;
    }
    double head_side_x = center_x(item);
    double stem_side_x = head_side_x;
    if (item->stem()) {
        switch (articulation_h_align) {
            case ArticulationStemSideAlign::STEM: stem_side_x = item->stem()->width() * .5; break;
            case ArticulationStemSideAlign::NOTEHEAD:
                stem_side_x = item->up() ? item->down_note()->notehead_center_x() : center_x(item);
                break;
            case ArticulationStemSideAlign::AVERAGE:
            default:
                stem_side_x = item->up() ? (item->stem()->width() * .5 + item->down_note()->notehead_center_x()) * .5 :
                                           (item->stem()->width() * .5 + center_x(item)) * .5;
                break;
        }
        if (item->up()) {
            stem_side_x = item->down_note()->pos().x() + item->down_note()->bbox_right_pos() - stem_side_x;
        }
    }

    double stac_accent_kern = 0.2 * item->spatium();
    double mag = item->mag();
    double min_dist = ctx.conf().style_absolute(Sid::ARTICULATION_MIN_DISTANCE) * mag;
    double staff_dist = ctx.conf().style_absolute(Sid::PROPERTY_DISTANCE) * mag;
    double stem_dist = ctx.conf().style_absolute(Sid::PROPERTY_DISTANCE_STEM) * mag;
    double note_dist = ctx.conf().style_absolute(Sid::PROPERTY_DISTANCE_HEAD) * mag;
    double y_offset = item->staff_offset_y();

    double chord_top_y = item->up_pos() - 0.5 * item->up_note()->head_height() + y_offset;   // note position of highest note
    double chord_bot_y = item->down_pos() + 0.5 * item->up_note()->head_height() + y_offset; // note position of lowest note

    double staff_top_y = -staff_dist + y_offset;
    double staff_bot_y = item->staff()->staff_height(item->tick()) + staff_dist + y_offset;

    // avoid collisions of staff articulations with chord notes:
    // gap between note and staff articulation is distance0 + 0.5 spatium

    if (item->stem()) {
        // Check if there's a hook, because the tip of the hook always extends slightly past the end of the stem
        if (item->up()) {
            double tip = item->hook() ? item->hook()->ldata()->bbox().translated(item->hook()->pos()).top() :
                                        item->stem()->ldata()->bbox().translated(item->stem()->pos()).top();

            chord_top_y = tip + y_offset;
        } else {
            double tip = item->hook() ? item->hook()->ldata()->bbox().translated(item->hook()->pos()).bottom() :
                                        item->stem()->ldata()->bbox().translated(item->stem()->pos()).bottom();

            chord_bot_y = tip + y_offset;
        }
    }

    //
    //    place all articulations with anchor at chord/rest
    //
    chord_top_y -= item->up() ? stem_dist : note_dist;
    chord_bot_y += item->up() ? note_dist : stem_dist;
    // add space for staccato and tenuto marks which may have been previously laid out
    for (Articulation *a : item->articulations()) {
        ArticulationAnchor aa = a->anchor();
        if (a->layout_close_to_note() && a->visible() && aa == ArticulationAnchor::AUTO) {
            if (a->up()) {
                chord_top_y = a->y() - a->height() - min_dist + y_offset;
            } else {
                chord_bot_y = a->y() + a->height() + min_dist + y_offset;
            }
        }
    }

    for (Articulation *a : item->articulations()) {
        if (layout_on_cross_beam_side && !a->is_on_cross_beam_side()) {
            continue;
        }
        if (!a->layout_close_to_note()) {
            continue;
        }
        if (a->up()) {
            if (a->visible()) {
                chord_top_y = a->y() - a->height() - min_dist + y_offset;
            }
        } else {
            if (a->visible()) {
                chord_bot_y = a->y() + a->height() + min_dist + y_offset;
            }
        }
    }
    //
    //    now place all articulations with staff top or bottom anchor, or chord anchor for artics that don't layout close to note
    //
    staff_top_y = std::min(staff_top_y, chord_top_y);
    staff_bot_y = std::max(staff_bot_y, chord_bot_y);
    Articulation *stacc = nullptr;
    for (Articulation *a : item->articulations()) {
        double kearn_height = 0.0;
        if ((layout_on_cross_beam_side && !a->is_on_cross_beam_side()) || a->is_laissez_vib()) {
            continue;
        }

        if (a->is_staccato()) {
            stacc = a;
        } else if (stacc && a->is_accent() && stacc->up() == a->up() &&
                   (muse::real_is_equal_or_less(stacc->ldata()->pos().y(), 0.0) ||
                    muse::real_is_equal_or_more(stacc->ldata()->pos().y(), item->staff()->staff_height(item->tick())))) {
            // obviously, the accent doesn't have a cutout, so this value just artificially moves the stacc
            // and accent closer to each other to simulate some kind of kerning. Looks great using all musescore fonts,
            // though there is a possibility that a different font which has vertically-asymmetrical accents
            // could make this look bad.
            kearn_height = stac_accent_kern;
            stacc = nullptr;
        } else {
            stacc = nullptr;
        }

        if (!a->layout_close_to_note()) {
            TLayout::layout_item(a, ctx);
            if (a->up()) {
                a->set_pos(!item->up() || !a->is_basic_articulation() ? head_side_x : stem_side_x, staff_top_y + kearn_height - y_offset);
                if (a->visible()) {
                    staff_top_y = a->y() - a->height() - min_dist + y_offset;
                }
            } else {
                a->set_pos(item->up() || !a->is_basic_articulation() ? head_side_x : stem_side_x, staff_bot_y - kearn_height - y_offset);
                if (a->visible()) {
                    staff_bot_y = a->y() + a->height() + min_dist + y_offset;
                }
            }
            a->mutldata()->move_x(-a->ldata()->optical_center());
        }

        if (!a->is_on_cross_beam_side()) {
            if (a->layout_close_to_note()) {
                Measure *meas = a->measure();
                System *sys = meas->system();
                if (sys) {
                    sys->staff(a->v_staff_idx())->skyline().add(a->shape().translate(a->system_pos() + item->staff_offset()));
                }
            } else {
                Autoplace::autoplace_segment_element(a, a->mutldata(), a->up(), true);
            }
            if (a->add_to_skyline()) {
                a->segment()->staff_shape(a->v_staff_idx()).add(a->shape().translated(a->pos() + item->pos() + item->staff_offset()));
            }
        }
    }
}

//---------------------------------------------------------
//   layoutArticulations3
//    Called after layouting slurs
//    Fix up articulations that need to go outside the slur
//---------------------------------------------------------

void ChordLayout::layout_articulations3(Chord *item, Slur *slur, LayoutContext &ctx) {
    SlurSegment *ss;
    if (item == slur->start_cr()) {
        ss = slur->front_segment();
    } else if (item == slur->end_cr()) {
        ss = slur->back_segment();
    } else {
        return;
    }
    Segment *s = item->segment();
    Measure *m = item->measure();
    SysStaff *item_staff = m->system() ? m->system()->staff(item->v_staff_idx()) : nullptr;
    SysStaff *slur_staff = m->system() ? m->system()->staff(ss->v_staff_idx()) : nullptr;
    PointF item_staff_pos = item_staff ? PointF(0.0, item_staff->y()) : PointF();
    PointF slur_staff_pos = item_staff ? PointF(0.0, slur_staff->y()) : PointF();
    for (auto iter = item->articulations().begin(); iter != item->articulations().end(); ++iter) {
        Articulation *a = *iter;
        if (a->layout_close_to_note() || !a->autoplace() || !slur->add_to_skyline()) {
            continue;
        }
        Shape a_shape = a->shape().translate(a->pos() + item->pos() + s->pos() + m->pos() + item->staff_offset() + item_staff_pos);
        Shape s_shape = ss->shape().translate(ss->pos() + slur_staff_pos);
        s_shape.remove_types({ElementType::HAMMER_ON_PULL_OFF_TEXT});
        double min_dist = ctx.conf().style_absolute(Sid::ARTICULATION_MIN_DISTANCE);
        bool slur_below_articulation = a->up() && !(ss->v_staff_idx() < item->v_staff_idx());
        double vert_clearance = slur_below_articulation ? a_shape.vertical_clearance(s_shape) : s_shape.vertical_clearance(a_shape);
        if (vert_clearance < min_dist) {
            min_dist +=
                slur->up() ? std::max(a_shape.min_vertical_distance(s_shape), 0.0) : std::max(s_shape.min_vertical_distance(a_shape), 0.0);
            min_dist *= slur->up() ? -1 : 1;
            for (auto iter2 = iter; iter2 != item->articulations().end(); ++iter2) {
                Articulation *aa = *iter2;
                aa->mutldata()->move_y(min_dist);
                if (item_staff && aa->add_to_skyline()) {
                    SystemLayout::update_skyline_for_element(aa, m->system(), min_dist);
                    for (ShapeElement &sh : s->staff_shape(item->v_staff_idx()).elements()) {
                        if (sh.item() == aa) {
                            sh.translate(0.0, min_dist);
                        }
                    }
                }
            }
        }
    }
}

//! May be called again when the chord is added to or removed from a beam.
void ChordLayout::layout_stem(Chord *item, const LayoutContext &ctx) {

    LAYOUT_CALL() << "chord: " << item->eid();

    // Stem needs to know hook's bbox and SMuFL anchors.
    // This is done before calcDefaultStemLength because the presence or absence of a hook affects stem length
    if (item->hook()) {
        item->hook()->set_hook_type(item->up() ? item->duration_type().hooks() : -item->duration_type().hooks());
        TLayout::layout_hook(item->hook(), item->hook()->mutldata());
    }

    // we should calculate default stem length for this chord even if it doesn't have a stem
    // because this length is used for tremolos or other things that attach to where the stem WOULD be
    item->set_default_stem_length(StemLayout::calc_default_stem_length(item, ctx));

    if (!item->stem()) {
        return;
    }

    item->stem()->mutldata()->set_pos_x(StemLayout::stem_pos_x(item));

    item->stem()->set_base_length(Spatium::from_absolute(item->default_stem_length(), item->spatium()));
    TLayout::layout_stem(item->stem(), item->stem()->mutldata(), ctx.conf());

    // And now we need to set the position of the flag.
    if (item->hook()) {
        item->hook()->set_pos(item->stem()->flag_position());
    }

    // Add Stem slash
    if ((item->show_stem_slash()) && !(item->beam() && item->beam()->elements().front() != item)) {
        if (!item->stem_slash()) {
            item->add(Factory::create_stem_slash(item));
        }
        TLayout::layout_stem_slash(item->stem_slash(), item->stem_slash()->mutldata(), ctx.conf());
    } else if (item->stem_slash()) {
        item->remove(item->stem_slash());
    }
}

bool ChordLayout::compute_up_beam_case(const Chord *item, Beam *beam) {
    if (beam->cross()) {
        return item->is_below_cross_beam(beam);
    } else if (beam->user_modified()) {
        return is_chord_pos_below_beam(item, beam);
    }

    return beam->up();
}

void ChordLayout::update_ledger_lines(Chord *item, LayoutContext &ctx) {
    // initialize for palette
    track_idx_t track = 0; // the track lines belong to
    // the line pos corresponding to the bottom line of the staff
    int line_below = 8; // assuming 5-lined "staff"
    double line_distance = 1;
    bool staff_visible = true;
    int step_offset = 0; // for staff type changes with a step offset

    Segment *segment = item->segment();

    if (segment) { //not palette
        Fraction tick = segment->tick();
        staff_idx_t idx = item->staff_idx() + item->staff_move();
        track = staff2track(idx);
        const Staff *st = ctx.dom().staff(idx);
        line_below = (st->lines(tick) - 1) * 2;
        line_distance = st->line_distance(tick);
        staff_visible = !st->is_lines_invisible(tick);
        step_offset = st->staff_type(tick)->step_offset();
    }

    // need ledger lines?
    if (item->down_line() + step_offset <= line_below + 1 && item->up_line() + step_offset >= -1) {
        muse::delete_all(item->ledger_lines());
        item->ledger_lines().clear();
        return;
    }

    // the extra length of a ledger line to be added on each side of the notehead
    const double extra_len = ctx.conf().style().style_absolute(Sid::LEDGER_LINE_LENGTH);

    bool visible = false;

    struct LedgerLineData {
        int line;
        double min_x, max_x;
        bool visible;
        bool accidental;
    };

    std::vector<LedgerLineData> ledger_line_data;

    // scan chord notes, collecting visibility and x and y extrema
    // NOTE: notes are sorted from bottom to top (line no. decreasing)
    // notes are scanned twice from outside (bottom or top) toward the staff
    // each pass stops at the first note without ledger lines
    int n = static_cast<int>(item->notes().size());

    for (const bool top_to_bottom : {false, true}) {
        const int from = top_to_bottom ? n - 1 : 0;
        const int delta = top_to_bottom ? -1 : 1;
        std::vector<LedgerLineData> vec_lines;
        double hw = 0.0;
        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::min();
        int min_line = 0;
        int max_line = line_below;

        for (int i = from; i < n && i >= 0; i += delta) {
            const Note *note = item->notes().at(i);
            int l = note->line() + step_offset;

            // if 1st pass and note not below staff or 2nd pass and note not above staff
            if ((!top_to_bottom && l <= line_below + 1) || (top_to_bottom && l >= -1)) {
                break; // stop this pass
            }
            // round line number to even number toward 0
            if (l < 0) {
                l = (l + 1) & ~1;
            } else {
                l = l & ~1;
            }

            if (note->visible()) { // if one note is visible,
                visible = true;    // all lines between it and the staff are visible
            }
            hw = std::max(hw, note->head_width());

            // Experimental:
            // shorten ledger line to avoid collisions with accidentals
            // TODO: do something with the following `accid` flag
            //
            // bool accid = (note->accidental() && note->line() >= (l-1) && note->line() <= (l+1) );

            // ledger lines need the leftmost point of the notehead with a respect of bbox
            const double x = note->pos().x() + note->bbox_x_shift();
            if (x - extra_len * note->mag() < min_x) {
                min_x = x - extra_len * note->mag();
                // increase width of all lines between this one and the staff
                for (auto &d : vec_lines) {
                    if (!d.accidental && ((l < 0 && d.line >= l) || (l > 0 && d.line <= l))) {
                        d.min_x = min_x;
                    }
                }
            }
            // same for left side
            if (x + hw + extra_len * note->mag() > max_x) {
                max_x = x + hw + extra_len * note->mag();
                for (auto &d : vec_lines) {
                    if ((l < 0 && d.line >= l) || (l > 0 && d.line <= l)) {
                        d.max_x = max_x;
                    }
                }
            }

            // check if note vert. pos. is outside current range
            // and, if so, add data for new line(s)
            if (l < min_line) {
                for (int i1 = l; i1 < min_line; i1 += 2) {
                    vec_lines.emplace_back(LedgerLineData{/*line=*/i1,
                                                          /*minX=*/min_x,
                                                          /*maxX=*/max_x,
                                                          /*visible=*/visible,
                                                          /*accidental=*/false});
                }
                min_line = l;
            }
            if (l > max_line) {
                for (int i1 = max_line + 2; i1 <= l; i1 += 2) {
                    vec_lines.emplace_back(LedgerLineData{/*line=*/i1,
                                                          /*minX=*/min_x,
                                                          /*maxX=*/max_x,
                                                          /*visible=*/visible,
                                                          /*accidental=*/false});
                }
                max_line = l;
            }
        }
        if (min_line < 0 || max_line > line_below) {
            ledger_line_data.insert(ledger_line_data.end(), vec_lines.begin(), vec_lines.end());
        }
    }

    double spatium_value = item->spatium();
    double step_distance = line_distance * 0.5;
    item->resize_ledger_lines_to(ledger_line_data.size());
    for (size_t i = 0; i < ledger_line_data.size(); ++i) {
        LedgerLineData lld = ledger_line_data[i];
        LedgerLine *h = item->ledger_lines()[i];
        h->set_track(track);
        h->set_visible(lld.visible && staff_visible);
        h->set_len(lld.max_x - lld.min_x);
        h->set_pos(lld.min_x, lld.line * spatium_value * step_distance);
    }

    for (LedgerLine *ll : item->ledger_lines()) {
        TLayout::layout_ledger_line(ll, ctx);
    }
}

bool ChordLayout::is_chord_pos_below_beam(const Chord *item, Beam *beam) {
    assert(!beam->beam_fragments().empty());

    Note *base_note = item->up() ? item->down_note() : item->up_note();
    double note_y = base_note->page_pos().y();

    PointF start_anchor = beam->start_anchor();
    PointF end_anchor = beam->end_anchor();

    if (start_anchor.is_null() || end_anchor.is_null()) {
        return beam->cross() ? item->is_below_cross_beam(beam) : beam->up();
    }

    if (item == beam->elements().front()) {
        return note_y > start_anchor.y();
    }

    if (item == beam->elements().back()) {
        return note_y > end_anchor.y();
    }

    PointF note_anchor = BeamLayout::chord_beam_anchor(beam, item, ChordBeamAnchorType::MIDDLE);
    double note_x = note_anchor.x();

    double proportion_along_x =
        muse::real_is_equal(start_anchor.x(), end_anchor.x()) ? 0.0 : (note_x - start_anchor.x()) / (end_anchor.x() - start_anchor.x());
    double desired_y = proportion_along_x * (end_anchor.y() - start_anchor.y()) + start_anchor.y();
    return note_y > desired_y;
}

bool ChordLayout::is_chord_pos_below_trem(const Chord *item, TremoloTwoChord *trem) {
    if (!item || !trem || !trem->ldata()->is_valid()) {
        return true;
    }
    Note *base_note = item->up() ? item->down_note() : item->up_note();
    double note_y = base_note->page_pos().y();

    PointF start_anchor = trem->start_anchor();
    PointF end_anchor = trem->end_anchor();

    if (start_anchor.is_null() || end_anchor.is_null()) {
        return trem->cross_staff_beam_between() ? item->is_below_cross_beam(trem) : trem->up();
    }

    if (item == trem->chord1()) {
        return note_y > start_anchor.y();
    }

    return note_y > end_anchor.y();
}

bool ChordLayout::compute_up_tremolo_case(const Chord *item, TremoloTwoChord *tremolo, const LayoutContext &ctx) {
    const Chord *c1 = tremolo->chord1();
    const Chord *c2 = tremolo->chord2();
    if (!c1 || !c2) {
        return true;
    }
    bool cross = c1->staff_move() != c2->staff_move();
    if (item == c1) {
        // we have to lay out the tremolo because it hasn't been laid out at all yet, and we need its direction
        TremoloLayout::layout(tremolo, ctx);
    }

    const Measure *measure = item->find_measure();
    if (!cross && !tremolo->user_modified()) {
        return tremolo->up();
    }

    if (!measure->system()) {
        // this method will be called later (from Measure::layoutCrossStaff) after the
        // system is completely laid out.
        // this is necessary because otherwise there's no way to deal with cross-staff beams
        // because we don't know how far apart the staves actually are
        return item->ldata()->up;
    }

    if (tremolo->user_modified()) {
        return ChordLayout::is_chord_pos_below_trem(item, tremolo);
    }

    // unmodified cross-staff trem, should be one note per staff
    if (item->staff_move() != 0) {
        return item->staff_move() > 0;
    }

    int other_staff_move = item->staff_move() == c1->staff_move() ? c2->staff_move() : c1->staff_move();
    return other_staff_move < 0;
}

void ChordLayout::compute_up(const Chord *item, Chord::LayoutData *ldata, const LayoutContext &ctx) {
    LAYOUT_CALL() << LAYOUT_ITEM_INFO(item);

    assert(!item->notes().empty());

    // Beams with one chord will be removed later, ignore
    const bool has_beam = item->beam() && item->beam()->elements().size() > 1;

    const StaffType *tab = item->staff() ? item->staff()->staff_type_for_element(item) : 0;
    bool is_tab_staff = tab && tab->is_tab_staff();
    if (is_tab_staff) {
        if (tab->stemless()) {
            ldata->up = false;
            return;
        }
        if (!tab->stem_through()) {
            bool staff_has_multiple_voices = item->measure()->has_voices(item->staff_idx(), item->tick(), item->actual_ticks());
            if (staff_has_multiple_voices) {
                bool is_track_even = item->track() % 2 == 0;
                ldata->up = is_track_even;
                return;
            }
            ldata->up = !tab->stems_down();
            return;
        }
    }

    if (item->stem_direction() != DirectionV::AUTO && !has_beam && !item->tremolo_two_chord()) {
        ldata->up = item->stem_direction() == DirectionV::UP;
        return;
    }

    if (item->is_ui_item()) {
        ldata->up = true;
        return;
    }

    if (has_beam) {
        ldata->up = compute_up_beam_case(item, item->beam());
        return;
    } else if (item->tremolo_two_chord()) {
        ldata->up = compute_up_tremolo_case(item, item->tremolo_two_chord(), ctx);
        return;
    }

    bool staff_has_multiple_voices = item->measure()->has_voices(item->staff_idx(), item->tick(), item->actual_ticks());
    if (staff_has_multiple_voices) {
        bool is_track_even = item->track() % 2 == 0;
        ldata->up = is_track_even;
        return;
    }

    bool is_grace_note = item->note_type() != NoteType::NORMAL;
    if (is_grace_note) {
        ldata->up = true;
        return;
    }

    bool chord_is_cross_staff = item->staff_move() != 0;
    if (chord_is_cross_staff) {
        ldata->up = item->staff_move() > 0;
        return;
    }

    std::vector<int> distances = item->note_distances();
    int direction = ChordLayout::compute_auto_stem_direction(distances);
    ldata->up = direction > 0;
}

void ChordLayout::compute_up(ChordRest *item, const LayoutContext &ctx) {
    if (item->is_chord()) {
        Chord *ch = item_cast<Chord *>(item);
        compute_up(ch, ch->mutldata(), ctx);
    } else {
        // base ChordRest
        // Beams with one chord will be removed later, ignore
        const bool has_beam = item->beam() && item->beam()->elements().size() > 1;
        if (has_beam) {
            item->mutldata()->up = item->beam()->up();
        } else {
            item->mutldata()->up = true;
        }
    }
}

// return 1 means up, 0 means in the middle, -1 means down
int ChordLayout::compute_auto_stem_direction(const std::vector<int> &note_distances) {
    int left = 0;
    int right = static_cast<int>(note_distances.size()) - 1;

    while (left <= right) {
        int left_note = note_distances.at(left);
        int right_note = note_distances.at(right);
        int net_directing = left_note + right_note;
        if (net_directing == 0) {
            left++;
            right--;
            continue;
        }
        return net_directing > 0 ? 1 : -1;
    }
    return 0;
}

//---------------------------------------------------------
//   layoutSegmentElements
//---------------------------------------------------------

static void layout_segment_elements(Segment *segment, track_idx_t start_track, track_idx_t end_track, staff_idx_t staff_idx,
                                    LayoutContext &ctx) {
    for (track_idx_t track = start_track; track < end_track; ++track) {
        if (EngravingItem *e = segment->element(track)) {
            if (e->v_staff_idx() == staff_idx) {
                TLayout::layout_item(e, ctx);
            }
        }
    }
}

void ChordLayout::skip_accidentals(Segment *segment, track_idx_t start_track, track_idx_t end_track) {
    for (track_idx_t track = start_track; track < end_track; ++track) {
        EngravingItem *item = segment->element(track);
        if (item && item->is_chord()) {
            for (Note *note : to_chord(item)->notes()) {
                Accidental *acc = note->accidental();
                if (acc) {
                    acc->mutldata()->set_is_skip_draw(true);
                }
            }
        }
    }
}

ChordPosInfo ChordLayout::calculate_chord_pos_info(Segment *segment, staff_idx_t staff_idx, track_idx_t part_start_track,
                                                   track_idx_t part_end_track, LayoutContext &ctx) {
    const Staff *staff = ctx.dom().staff(staff_idx);

    ChordPosInfo pos_info = ChordPosInfo();

    for (track_idx_t track = part_start_track; track < part_end_track; ++track) {
        EngravingItem *e = segment->element(track);
        const bool calc_chord_pos = e && e->is_chord() && to_chord(e)->v_staff_idx() == staff_idx;
        if (!calc_chord_pos) {
            continue;
        }

        Chord *chord = to_chord(e);
        pos_info.chords.push_back(chord);
        bool has_grace_before = false;
        for (Chord *c : chord->grace_notes()) {
            if (c->is_grace_before()) {
                has_grace_before = true;
            }
            layout_chords2(c->notes(), c->up(), ctx);    // layout grace note noteheads
            layout_chords3({c}, c->notes(), staff, ctx); // layout grace note chords
        }
        if (chord->up()) {
            ++pos_info.up_voices;
            pos_info.up_stem_notes.insert(pos_info.up_stem_notes.end(), chord->notes().begin(), chord->notes().end());
            pos_info.up_dots = std::max(pos_info.up_dots, chord->dots());
            pos_info.max_up_mag = std::max(pos_info.max_up_mag, chord->mag());
            if (!pos_info.up_hooks && !chord->beam()) {
                pos_info.up_hooks = chord->hook();
            }
            if (has_grace_before) {
                pos_info.up_grace = true;
            }
        } else {
            ++pos_info.down_voices;
            pos_info.down_stem_notes.insert(pos_info.down_stem_notes.end(), chord->notes().begin(), chord->notes().end());
            pos_info.down_dots = std::max(pos_info.down_dots, chord->dots());
            pos_info.max_down_mag = std::max(pos_info.max_down_mag, chord->mag());
            if (!pos_info.down_hooks && !chord->beam()) {
                pos_info.down_hooks = chord->hook();
            }
            if (has_grace_before) {
                pos_info.down_grace = true;
            }
        }
    }

    return pos_info;
}

void ChordLayout::calculate_max_note_widths(ChordPosInfo &pos_info, const Fraction &tick, const Staff *staff, LayoutContext &ctx) {
    double nominal_width = ctx.conf().note_head_width() * staff->staff_mag(tick);
    pos_info.max_up_width = nominal_width * pos_info.max_up_mag;
    pos_info.max_down_width = nominal_width * pos_info.max_down_mag;

    // layout upstem noteheads
    if (pos_info.up_voices > 1) {
        std::sort(pos_info.up_stem_notes.begin(), pos_info.up_stem_notes.end(),
                  [](Note *n1, const Note *n2) -> bool { return n1->string_or_line() > n2->string_or_line(); });
    }
    if (pos_info.up_voices) {
        double hw = layout_chords2(pos_info.up_stem_notes, true, ctx);
        pos_info.max_up_width = std::max(pos_info.max_up_width, hw);
    }

    // layout downstem noteheads
    if (pos_info.down_voices > 1) {
        std::sort(pos_info.down_stem_notes.begin(), pos_info.down_stem_notes.end(),
                  [](Note *n1, const Note *n2) -> bool { return n1->string_or_line() > n2->string_or_line(); });
    }
    if (pos_info.down_voices) {
        double hw = layout_chords2(pos_info.down_stem_notes, false, ctx);
        pos_info.max_down_width = std::max(pos_info.max_down_width, hw);
    }
}

void ChordLayout::apply_chord_offsets(Segment *segment, staff_idx_t staff_idx, track_idx_t part_start_track, track_idx_t part_end_track,
                                      OffsetInfo &offset_info, const ChordPosInfo &pos_info, LayoutContext &ctx) {
    const Staff *staff = ctx.dom().staff(staff_idx);
    const bool is_tab = staff->is_tab_staff(segment->tick());

    // apply chord offsets
    for (track_idx_t track = part_start_track; track < part_end_track; ++track) {
        EngravingItem *e = segment->element(track);
        const bool layout_chord = e && e->is_chord() && to_chord(e)->v_staff_idx() == staff_idx;
        if (!layout_chord) {
            continue;
        }
        Chord *chord = to_chord(e);
        Chord::LayoutData *chord_ldata = chord->mutldata();
        // only centre chords if we are separating voices and this voice has no collision
        const bool combine_voices = chord->should_combine_voice();
        if (!combine_voices && !muse::contains(offset_info.tracks_to_adjust, track)) {
            if (chord->up()) {
                chord_ldata->move_x(offset_info.center_up);
            } else {
                chord_ldata->move_x(offset_info.center_down);
            }
            continue;
        }

        if (chord->up()) {
            if (!muse::real_is_null(offset_info.up_offset)) {
                offset_info.oversize_up = is_tab ? offset_info.oversize_up / 2 : offset_info.oversize_up;
                chord_ldata->move_x(offset_info.up_offset + offset_info.center_adjust_up + offset_info.oversize_up);
                if (pos_info.down_dots && !pos_info.up_dots) {
                    chord_ldata->move_x(offset_info.dot_adjust);
                }
            } else {
                chord_ldata->move_x(offset_info.center_up);
            }
        } else {
            if (!muse::real_is_null(offset_info.down_offset)) {
                chord_ldata->move_x(offset_info.down_offset + offset_info.center_adjust_down);
                if (pos_info.up_dots && !pos_info.down_dots) {
                    chord_ldata->move_x(offset_info.dot_adjust);
                }
            } else {
                chord_ldata->move_x(offset_info.center_down);
            }
        }
    }

    // layout chords
    std::vector<Note *> notes;
    if (pos_info.up_voices) {
        notes.insert(notes.end(), pos_info.up_stem_notes.begin(), pos_info.up_stem_notes.end());
    }
    if (pos_info.down_voices) {
        notes.insert(notes.end(), pos_info.down_stem_notes.begin(), pos_info.down_stem_notes.end());
    }
    if (pos_info.up_voices + pos_info.down_voices > 1) {
        std::sort(notes.begin(), notes.end(), [](Note *n1, const Note *n2) -> bool { return n1->string_or_line() > n2->string_or_line(); });
    }
    layout_chords3(pos_info.chords, notes, staff, ctx);
}

OffsetInfo ChordLayout::centre_chords(const Segment *segment, ChordPosInfo &pos_info, staff_idx_t staff_idx, const Fraction &tick,
                                      LayoutContext &ctx) {
    const Staff *staff = ctx.dom().staff(staff_idx);
    const bool is_tab = staff->is_tab_staff(segment->tick());

    OffsetInfo offset_info = OffsetInfo();
    // only center chords on standard staves.  This is a simpler process for TAB and done elsewhere
    if (is_tab) {
        return offset_info;
    }

    double sp = staff->spatium(tick);
    double nominal_width = ctx.conf().note_head_width() * staff->staff_mag(tick);

    // only center chords if they differ from nominal by at least this amount
    // this avoids unnecessary centering on differences due only to floating point roundoff
    // it also allows for the possibility of disabling centering
    // for notes only "slightly" larger than nominal, like half notes
    // but this will result in them not being aligned with each other between voices
    // unless you change to left alignment as described in the comments below
    double center_threshold = 0.01 * sp;

    // amount by which actual width exceeds nominal, adjusted for staff mag() only
    double head_diff = pos_info.max_up_width - nominal_width;
    // amount by which actual width exceeds nominal, adjusted for staff & chord/note mag()
    double head_diff2 = pos_info.max_up_width - nominal_width * (pos_info.max_up_mag / staff->staff_mag(tick));
    if (head_diff > center_threshold && !is_tab) {
        // larger than nominal
        offset_info.center_up = head_diff * -0.5;
        // maxUpWidth is true width, but we no longer will care about that
        // instead, we care only about portion to right of origin
        pos_info.max_up_width += offset_info.center_up;
        // to left align rather than center, delete both of the above
        if (head_diff2 > center_threshold) {
            // if max notehead is wider than nominal with chord/note mag() applied
            // then noteheads extend to left of origin
            // because stemPosX() is based on nominal width
            // so we need to correct for that too
            offset_info.center_up += head_diff2;
            offset_info.oversize_up = head_diff2;
        }
    } else if (-head_diff > center_threshold) {
        // smaller than nominal
        offset_info.center_up = -head_diff * 0.5;
        if (head_diff2 > center_threshold) {
            // max notehead is wider than nominal with chord/note mag() applied
            // perform same adjustment as above
            offset_info.center_up += head_diff2;
            offset_info.oversize_up = head_diff2;
        }
        offset_info.center_adjust_down = offset_info.center_up;
    }

    head_diff = pos_info.max_down_width - nominal_width;
    if (head_diff > center_threshold) {
        // larger than nominal
        offset_info.center_down = head_diff * -0.5;
        // to left align rather than center, change the above to
        //centerAdjustUp = headDiff;
        pos_info.max_down_width = nominal_width - offset_info.center_down;
    } else if (-head_diff > center_threshold && !is_tab) {
        // smaller than nominal
        offset_info.center_down = -head_diff * 0.5;
        offset_info.center_adjust_up = offset_info.center_down;
    }

    return offset_info;
}

void ChordLayout::calculate_chord_offsets(Segment *segment, staff_idx_t staff_idx, const Fraction &tick, OffsetInfo &offset_info,
                                          ChordPosInfo &pos_info, LayoutContext &ctx) {
    // handle conflict between upstem and downstem chords
    if (!pos_info.up_voices || !pos_info.down_voices) {
        return;
    }
    const Staff *staff = ctx.dom().staff(staff_idx);
    double sp = staff->spatium(tick);
    const bool is_tab = staff->is_tab_staff(segment->tick());

    Note *bottom_up_note = pos_info.up_stem_notes.front();
    Note *top_down_note = pos_info.down_stem_notes.back();
    int separation = top_down_note->string_or_line() - bottom_up_note->string_or_line();

    std::vector<Note *> overlap_notes;
    overlap_notes.reserve(8);

    if (separation == 1) {
        // second
        if (pos_info.up_dots && !pos_info.down_dots) {
            offset_info.up_offset = pos_info.max_down_width + 0.1 * sp;
            offset_info.tracks_to_adjust.insert(bottom_up_note->track());
        } else {
            offset_info.down_offset = pos_info.max_up_width;
            // align stems if present
            if (top_down_note->chord()->stem() && bottom_up_note->chord()->stem()) {
                const Stem *top_down_stem = top_down_note->chord()->stem();
                offset_info.down_offset -= top_down_stem->line_width_mag();
            } else if (top_down_note->chord()->duration_type().head_type() != NoteHeadType::HEAD_BREVIS &&
                       bottom_up_note->chord()->duration_type().head_type() != NoteHeadType::HEAD_BREVIS) {
                // stemless notes should be aligned as is they were stemmed
                // (except in case of brevis, cause the notehead has the side bars)
                offset_info.down_offset -= ctx.conf().style_absolute(Sid::STEM_WIDTH) * top_down_note->chord()->mag();
            }
            offset_info.tracks_to_adjust.insert(top_down_note->track());
        }
    } else if (separation < 1) {
        // overlap (possibly unison)

        // build list of overlapping notes
        for (size_t i = 0, n = pos_info.up_stem_notes.size(); i < n; ++i) {
            if (pos_info.up_stem_notes[i]->string_or_line() >= top_down_note->string_or_line() - 1) {
                overlap_notes.push_back(pos_info.up_stem_notes[i]);
            } else {
                break;
            }
        }
        for (size_t i = pos_info.down_stem_notes.size(); i > 0; --i) { // loop most probably needs to be in this reverse order
            if (pos_info.down_stem_notes[i - 1]->string_or_line() <= bottom_up_note->string_or_line() + 1) {
                overlap_notes.push_back(pos_info.down_stem_notes[i - 1]);
            } else {
                break;
            }
        }
        std::sort(overlap_notes.begin(), overlap_notes.end(),
                  [](Note *n1, const Note *n2) -> bool { return n1->string_or_line() > n2->string_or_line(); });

        // determine nature of overlap
        bool share_heads = true;                  // can all overlapping notes share heads?
        bool match_pending = false;               // looking for a unison match
        bool conflict_unison = false;             // unison found
        bool conflict_second_up_higher = false;   // second found
        bool conflict_second_down_higher = false; // second found
        int last_line = 1000;
        track_idx_t last_track = muse::nidx;
        Note *p = overlap_notes[0];
        for (size_t i = 0, count = overlap_notes.size(); i < count; ++i) {
            Note *n = overlap_notes[i];
            NoteHeadType n_head_type;
            NoteHeadType p_head_type;
            Chord *nchord = n->chord();
            Chord *pchord = p->chord();
            if (n->ldata()->mirror()) {
                if (separation < 0) {
                    // don't try to share heads if there is any mirroring
                    share_heads = false;
                    // don't worry about conflicts involving mirrored notes
                    continue;
                }
            }
            track_idx_t track = n->track();
            int line = n->string_or_line();
            int d = last_line - line;
            switch (d) {
                case 0:
                    // unison
                    conflict_unison = true;
                    match_pending = false;
                    n_head_type = (n->head_type() == NoteHeadType::HEAD_AUTO) ? n->chord()->duration_type().head_type() : n->head_type();
                    p_head_type = (p->head_type() == NoteHeadType::HEAD_AUTO) ? p->chord()->duration_type().head_type() : p->head_type();
                    // the most important rules for sharing noteheads on unisons between voices are
                    // that notes must be one same line with same tpc
                    // noteheads must be unmirrored and of same group
                    // and chords must be same size if notehead is anything other than HEAD_QUARTER
                    if (n->head_group() != p->head_group() || n->tpc() != p->tpc() || n->ldata()->mirror() || p->ldata()->mirror() ||
                        (nchord->is_small() != pchord->is_small() &&
                         (n_head_type != NoteHeadType::HEAD_QUARTER || p_head_type != NoteHeadType::HEAD_QUARTER))) {
                        share_heads = false;
                    } else {
                        // noteheads are potentially shareable
                        // it is more subjective at this point
                        // current default is to require *either* of the following:
                        //    1) both chords have same number of dots, both have stems, and both noteheads are same type and are full size (automatic match)
                        // or 2) one or more of the noteheads is not of type AUTO, but is explicitly set to match the other (user-forced match)
                        // or 3) exactly one of the noteheads is invisible (user-forced match)
                        // thus user can force notes to be shared despite differing number of dots or either being stemless
                        // by setting one of the notehead types to match the other or by making one notehead invisible
                        // TODO: consider adding a style option, staff properties, or note property to control sharing
                        if ((nchord->dots() != pchord->dots() || !nchord->stem() || !pchord->stem() || n_head_type != p_head_type ||
                             n->is_small() || p->is_small()) &&
                            ((n->head_type() == NoteHeadType::HEAD_AUTO && p->head_type() == NoteHeadType::HEAD_AUTO) ||
                             n_head_type != p_head_type) &&
                            (n->visible() == p->visible())) {
                            share_heads = false;
                        }
                    }

                    break;
                case 1:
                    // second
                    // trust that this won't be a problem for single unison
                    if (separation < 0) {
                        if (n->chord()->up()) {
                            conflict_second_up_higher = true;
                        } else {
                            conflict_second_down_higher = true;
                        }
                        share_heads = false;
                    }
                    break;
                default:
                    // no conflict
                    if (match_pending) {
                        share_heads = false;
                    }
                    match_pending = true;
            }
            if (!share_heads) {
                offset_info.tracks_to_adjust.insert({track, last_track});
            }
            p = n;
            last_line = line;
            last_track = track;
        }
        if (match_pending) {
            share_heads = false;
        }

        bool conflict = conflict_unison || conflict_second_down_higher || conflict_second_up_higher;
        bool ledger_overlap_above = false;
        bool ledger_overlap_below = false;

        double ledger_gap = 0.15 * sp;
        double ledger_len = ctx.conf().style_s(Sid::LEDGER_LINE_LENGTH).val() * sp;
        int first_ledger_below = staff->lines(bottom_up_note->tick()) * 2;
        int top_down_stem_len = 0;
        if (!conflict_unison && top_down_note->chord()->stem()) {
            top_down_stem_len = std::round(top_down_note->chord()->stem()->ldata()->bbox().height() / sp * 2);
            if (bottom_up_note->string_or_line() > first_ledger_below - 1 &&
                top_down_note->string_or_line() < bottom_up_note->string_or_line() &&
                top_down_note->string_or_line() + top_down_stem_len >= first_ledger_below) {
                ledger_overlap_below = true;
            }
        }

        int first_ledger_above = -2;
        int bottom_up_stem_len = 0;
        if (!conflict_unison && bottom_up_note->chord()->stem()) {
            bottom_up_stem_len = std::round(bottom_up_note->chord()->stem()->ldata()->bbox().height() / sp * 2);
            if (top_down_note->string_or_line() < -1 && top_down_note->string_or_line() < bottom_up_note->string_or_line() &&
                bottom_up_note->string_or_line() - bottom_up_stem_len <= first_ledger_above) {
                ledger_overlap_above = true;
            }
        }

        // calculate offsets
        if (share_heads) {
            for (int i = static_cast<int>(overlap_notes.size()) - 1; i >= 1; i -= 2) {
                Note *previous_note = overlap_notes[i - 1];
                Note *n = overlap_notes[i];
                const bool skip_note = previous_note->chord()->is_nudged() || n->chord()->is_nudged();
                if (skip_note) {
                    continue;
                }
                const bool prev_chord_small = previous_note->chord()->is_small();
                const bool n_chord_small = n->chord()->is_small();
                if (previous_note->chord()->dots() == n->chord()->dots()) {
                    // hide one set of dots
                    // Hide the small augmentation dot if present
                    bool on_line = !(previous_note->line() & 1);
                    if (prev_chord_small) {
                        previous_note->set_dots_hidden(true);
                    } else if (n_chord_small) {
                        n->set_dots_hidden(true);
                    } else if (on_line) {
                        // hide dots for lower voice
                        if (previous_note->voice() & 1) {
                            previous_note->set_dots_hidden(true);
                        } else {
                            n->set_dots_hidden(true);
                        }
                    } else {
                        // hide dots for upper voice
                        if (!(previous_note->voice() & 1)) {
                            previous_note->set_dots_hidden(true);
                        } else {
                            n->set_dots_hidden(true);
                        }
                    }
                }
                // If either chord is small, adjust offset
                Chord *small_chord = prev_chord_small ? previous_note->chord() : nullptr;
                small_chord = n_chord_small ? n->chord() : small_chord;
                if (small_chord && !(prev_chord_small && n_chord_small)) {
                    if (small_chord->up()) {
                        offset_info.center_up *= 2;
                    } else {
                        offset_info.center_down = 0;
                    }
                }
                // formerly we hid noteheads in an effort to fix playback
                // but this doesn't work for cases where noteheads cannot be shared
                // so better to solve the problem elsewhere
            }
        } else if (conflict && (pos_info.up_dots && !pos_info.down_dots)) {
            offset_info.up_offset = pos_info.max_down_width + 0.1 * sp;
        } else if (conflict_unison && separation == 0 && (!pos_info.down_grace || pos_info.up_grace)) {
            offset_info.down_offset = pos_info.max_up_width + 0.15 * sp;
        } else if (conflict_unison) {
            offset_info.up_offset = pos_info.max_down_width + 0.15 * sp;
        } else if (conflict_second_up_higher) {
            offset_info.up_offset = pos_info.max_down_width + 0.15 * sp;
        } else if ((pos_info.down_hooks && !pos_info.up_hooks) && !(pos_info.up_dots && !pos_info.down_dots)) {
            // Shift by ledger line length if ledger line conflict or just 0.3sp if no ledger lines
            double adj_space = (ledger_overlap_above || ledger_overlap_below) ? ledger_gap + ledger_len : 0.3 * sp;
            offset_info.down_offset = pos_info.max_up_width + adj_space;
        } else if (conflict_second_down_higher) {
            if (pos_info.down_dots && !pos_info.up_dots) {
                double adj_space = (ledger_overlap_above || ledger_overlap_below) ? ledger_gap + ledger_len : 0.2 * sp;
                offset_info.down_offset = pos_info.max_up_width + adj_space;
            } else {
                // Prevent ledger line & notehead collision
                double adj_space =
                    (top_down_note->string_or_line() <= first_ledger_above || bottom_up_note->string_or_line() >= first_ledger_below) ?
                        ledger_len - ledger_gap - 0.2 * sp :
                        -0.2 * sp;
                offset_info.up_offset = pos_info.max_down_width + adj_space;
                if (pos_info.down_hooks) {
                    bool needs_hook_space = (ledger_overlap_below || ledger_overlap_above);
                    Hook *hook = top_down_note->chord()->hook();
                    double hook_space = hook ? hook->width() : 0.0;
                    offset_info.up_offset = needs_hook_space ? hook_space + ledger_len + ledger_gap : offset_info.up_offset + 0.3 * sp;
                }
            }
        } else {
            // no direct conflict, so parts can overlap (downstem on left)
            // just be sure that stems clear opposing noteheads and ledger lines
            // Stems are in the middle of fret marks on TAB staves
            double clear_left = is_tab ? StemLayout::stem_pos_x(bottom_up_note->chord()) : 0.0;
            double clear_right = is_tab ? StemLayout::stem_pos_x(bottom_up_note->chord()) : 0.0;
            double overlap_padding = (is_tab ? 0 : 0.3) * sp;
            if (const Stem *top_down_stem = top_down_note->chord()->stem()) {
                if (ledger_overlap_below) {
                    // Create space between stem and ledger line below staff
                    clear_left += ledger_len + ledger_gap + top_down_stem->line_width_mag();
                } else {
                    clear_left += top_down_stem->line_width_mag() + overlap_padding;
                }
            }
            if (const Stem *bottom_up_stem = bottom_up_note->chord()->stem()) {
                if (ledger_overlap_above) {
                    // Create space between stem and ledger line above staff
                    clear_right +=
                        pos_info.max_down_width + ledger_len + ledger_gap - pos_info.max_up_width + bottom_up_stem->line_width_mag();
                } else {
                    clear_right +=
                        bottom_up_stem->line_width_mag() + std::max(pos_info.max_down_width - pos_info.max_up_width, 0.0) + overlap_padding;
                }
            } else {
                pos_info.down_dots = 0; // no need to adjust for dots in this case
            }
            offset_info.up_offset = std::max(clear_left, clear_right);
            // Check if there's enough space to tuck under a flag
            Note *top_up_note = pos_info.up_stem_notes.back();
            // Move notes out of the way of straight flags
            int pad = ctx.conf().style_b(Sid::USE_STRAIGHT_NOTE_FLAGS) ? 2 : 1;
            bool overlaps_flag = top_down_note->string_or_line() + top_down_stem_len + pad > top_up_note->string_or_line();
            if (pos_info.down_hooks && (ledger_overlap_below || overlaps_flag)) {
                // we will need more space to avoid collision with hook
                // but we won't need as much dot adjustment
                Hook *hook = top_down_note->chord()->hook();
                double hook_width = hook ? hook->width() : 0.0;
                if (ledger_overlap_below) {
                    offset_info.up_offset = hook_width + ledger_len + ledger_gap;
                }
                if (is_tab) {
                    offset_info.up_offset = hook_width + pos_info.max_down_width;
                }
                offset_info.up_offset = std::max(offset_info.up_offset, pos_info.max_down_width + 0.1 * sp);
                offset_info.dot_adjust_threshold = pos_info.max_up_width - 0.3 * sp;
            }
            // if downstem chord is small, don't center
            // and we might not need as much dot adjustment either
            if (offset_info.center_down > 0.0) {
                offset_info.center_down = 0.0;
                offset_info.center_adjust_up = 0.0;
                offset_info.dot_adjust_threshold = (offset_info.up_offset - pos_info.max_down_width) + pos_info.max_up_width - 0.3 * sp;
            }
        }
    }

    // adjust for dots
    if ((pos_info.up_dots && !pos_info.down_dots) || (pos_info.down_dots && !pos_info.up_dots)) {
        // only one sets of dots
        // place between chords
        int dots;
        double mag;
        if (pos_info.up_dots) {
            dots = pos_info.up_dots;
            mag = pos_info.max_up_mag;
        } else {
            dots = pos_info.down_dots;
            mag = pos_info.max_down_mag;
        }
        double dot_width = segment->sym_width(SymId::AUGMENTATION_DOT);
        // first dot
        offset_info.dot_adjust = ctx.conf().style_absolute(Sid::DOT_NOTE_DISTANCE) + dot_width;
        // additional dots
        if (dots > 1) {
            offset_info.dot_adjust += ctx.conf().style_absolute(Sid::DOT_DOT_DISTANCE) * (dots - 1);
        }
        offset_info.dot_adjust *= mag;
        // only by amount over threshold
        offset_info.dot_adjust = std::max(offset_info.dot_adjust - offset_info.dot_adjust_threshold, 0.0);
    }
    if (separation == 1) {
        offset_info.dot_adjust += 0.1 * sp;
    }
}

//---------------------------------------------------------
//   layoutChords1
//    - layout upstem and downstem chords
//    - offset as necessary to avoid conflict
//---------------------------------------------------------

void ChordLayout::layout_chords1(LayoutContext &ctx, Segment *segment, staff_idx_t staff_idx) {
    LAYOUT_CALL() << LAYOUT_ITEM_INFO(segment);

    const Staff *staff = ctx.dom().staff(staff_idx);
    const bool is_tab = staff->is_tab_staff(segment->tick());
    const track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    const track_idx_t end_track = start_track + ::mu::engraving::voices;
    const Fraction tick = segment->tick();
    const StaffType *staff_type = staff->staff_type(segment->tick());

    // we need to check all the notes in all the staves of the part so that we don't get weird collisions
    // between accidentals etc with moved notes
    const Part *part = staff->part();
    const TrackRange part_track_range_or_default = part ? part->track_range() : TrackRange{start_track, end_track};
    const track_idx_t part_start_track = part_track_range_or_default.start_track;
    const track_idx_t part_end_track = part_track_range_or_default.end_track;

    if (is_tab) {
        skip_accidentals(segment, start_track, end_track);
    }

    const bool is_simple_tab =
        !staff->staff_type() || (!staff->staff_type()->stem_through() && !staff->staff_type()->is_common_tab_staff());
    if (staff && staff->is_tab_staff(tick) && is_simple_tab) {
        // Positions of notes should be reset in case we are changing from common or full tab with offsets to simple which should have none
        for (track_idx_t track = part_start_track; track < part_end_track; ++track) {
            EngravingItem *e = segment->element(track);
            if (e && e->is_chord() && to_chord(e)->v_staff_idx() == staff_idx) {
                Chord *chord = to_chord(e);
                Chord::LayoutData *chord_ldata = chord->mutldata();
                chord_ldata->set_pos_x(0.0);
            }
        }

        layout_segment_elements(segment, start_track, end_track, staff_idx, ctx);
        return;
    }

    ChordPosInfo pos_info = calculate_chord_pos_info(segment, staff_idx, part_start_track, part_end_track, ctx);

    if (pos_info.up_voices + pos_info.down_voices && (staff_type->stem_through() || staff_type->is_common_tab_staff())) {
        // TODO: use track as secondary sort criteria?
        // otherwise there might be issues with unisons between voices
        // in some corner cases

        calculate_max_note_widths(pos_info, tick, staff, ctx);

        OffsetInfo offset_info = centre_chords(segment, pos_info, staff_idx, tick, ctx);

        calculate_chord_offsets(segment, staff_idx, tick, offset_info, pos_info, ctx);

        apply_chord_offsets(segment, staff_idx, part_start_track, part_end_track, offset_info, pos_info, ctx);
    }

    if (!is_tab) {
        layout_ledger_lines(pos_info.chords, ctx);
        AccidentalsLayout::layout_accidentals(pos_info.chords, ctx);
        for (Chord *chord : pos_info.chords) {
            for (Chord *grace : chord->grace_notes()) {
                AccidentalsLayout::layout_accidentals({grace}, ctx);
            }
        }
    }

    layout_segment_elements(segment, part_start_track, part_end_track, staff_idx, ctx);

    for (Chord *chord : pos_info.chords) {
        Ornament *ornament = chord->find_ornament();
        if (ornament && ornament->show_cue_note()) {
            TLayout::layout_ornament_cue_note(ornament, ctx);
        }
    }
}

//---------------------------------------------------------
//   layoutChords2
//    - determine which notes need mirroring
//    - this is called once for each stem direction
//      eg, once for voices 1&3, once for 2&4
//      with all notes combined and sorted to resemble one chord
//    - return maximum non-mirrored notehead width
//---------------------------------------------------------

double ChordLayout::layout_chords2(std::vector<Note *> &notes, bool up, LayoutContext &ctx) {
    int start_idx, end_idx, inc_idx;
    double max_width = 0.0;

    // loop in correct direction so that first encountered notehead wins conflict
    if (up) {
        // loop bottom up
        start_idx = 0;
        end_idx = int(notes.size());
        inc_idx = 1;
    } else {
        // loop top down
        start_idx = int(notes.size()) - 1;
        end_idx = -1;
        inc_idx = -1;
    }

    int prev_line = 1000;                           // line of previous notehead
                                                    // hack: start high so first note won't show as conflict
    bool prev_visible = false;                      // was last note visible?
    bool mirror = false;                            // should current notehead be mirrored?
                                                    // value is retained and may be used on next iteration
                                                    // to track mirror status of previous note
    bool is_left = notes[start_idx]->chord()->up(); // is notehead on left?
    Chord *prev_chord = notes[start_idx]->chord();
    staff_idx_t prev_staff_idx = prev_chord->v_staff_idx(); // staff of last note (including staffMove)
    track_idx_t prev_track_idx = prev_chord->track();

    for (int idx = start_idx; idx != end_idx; idx += inc_idx) {
        Note *note = notes[idx];                 // current note
        const int line = note->string_or_line(); // line of current note
        Chord *chord = note->chord();

        const staff_idx_t staff_idx = chord->v_staff_idx(); // staff of current note
        const track_idx_t track_idx = chord->track();
        const Staff *st = note->staff();
        const StaffType *tab = st->staff_type_for_element(note);
        const bool is_tab = note->staff() && note->staff()->is_tab_staff(note->chord()->tick());

        if (is_tab) {
            // TAB notes need to be laid out to set the fret string so we can calulate their width
            // Standard staves can get this information from the notehead symbol, so no need to lay out
            TLayout::layout_note(note, note->mutldata());
        }

        // there is a conflict
        // if this is same or adjacent line as previous note (and chords are on same staff!)
        // but no need to do anything about it if either note is invisible
        bool same_track = track_idx == prev_track_idx;

        bool conflict = (std::abs(prev_line - line) < 2) && (prev_staff_idx == staff_idx) && note->visible() && prev_visible &&
                        (same_track || Chord::combine_voice(chord, prev_chord));

        // this note is on opposite side of stem as previous note
        // if there is a conflict
        // or if this the first note *after* a conflict
        if (conflict || (chord->up() != is_left)) {
            is_left = !is_left;
        }

        // determine if we would need to mirror current note
        // to get it to the correct side
        // this would be needed to get a note to left or downstem or right of upstem
        // whether or not we actually do this is determined later (based on user mirror property)
        bool nmirror = (chord->up() != is_left);

        // by default, notes and dots are not hidden
        // this may be changed later to allow unisons to share noteheads
        note->set_hidden(false);
        note->set_dots_hidden(false);

        // be sure chord position is initialized
        // chord may be moved to the right later
        // if there are conflicts between voices
        if (!chord->is_grace()) {
            chord->mutldata()->set_pos_x(0.0);
        }

        // let user mirror property override the default we calculated
        if (note->user_mirror() == DirectionH::AUTO) {
            mirror = nmirror;
        } else {
            mirror = note->chord()->up();
            if (note->user_mirror() == DirectionH::LEFT) {
                mirror = !mirror;
            }
        }
        note->mutldata()->mirror.set_value(mirror);
        if (chord->stem()) {
            chord->stem()->mutldata()->set_pos_x(StemLayout::stem_pos_x(chord));
            TLayout::layout_stem(chord->stem(), chord->stem()->mutldata(),
                                 ctx.conf()); // needed because mirroring can cause stem position to change
        }

        // accumulate return value
        if (!mirror) {
            const double fret_background = ctx.conf().style_s(Sid::TAB_FRET_PADDING).val() * note->spatium();
            const double note_width = is_tab ? note->tab_head_width(tab) + 2 * fret_background : note->bbox_right_pos();
            max_width = std::max(max_width, note_width);
        }

        // prepare for next iteration
        prev_chord = chord;
        prev_visible = note->visible();
        prev_staff_idx = staff_idx;
        prev_track_idx = track_idx;
        prev_line = line;
    }

    return max_width;
}

bool ChordLayout::chord_has_dots_all_invisible(Chord *chord) {
    if (!chord->dots()) {
        return false;
    }

    for (Note *note : chord->notes()) {
        for (NoteDot *dot : note->dots()) {
            if (dot->visible()) {
                return false;
            }
        }
    }

    return true;
}

//---------------------------------------------------------
//   centerX
//    return x position for attributes
//---------------------------------------------------------

double ChordLayout::center_x(const Chord *chord) {
    // TAB 'notes' are always centered on the stem
    const Staff *st = chord->staff();
    const StaffType *stt = st->staff_type_for_element(chord);
    if (stt->is_tab_staff()) {
        return rendering::score::StemLayout::tab_stem_pos_x() * chord->spatium();
    }

    const Note *note = chord->up() ? chord->down_note() : chord->up_note();
    double x = note->pos().x() + note->notehead_center_x();
    return x;
}

//---------------------------------------------------------
//   placeDots
//---------------------------------------------------------

void ChordLayout::place_dots(const std::vector<Chord *> &chords, const std::vector<Note *> &notes, LayoutContext &ctx) {
    Chord *chord = nullptr;
    for (Chord *c : chords) {
        if (c->dots() > 0) {
            chord = c;
            break;
        } else {
            for (Note *note : c->notes()) {
                ChordLayout::set_dot_relative_line(note, 0, ctx); // this manages the deletion of dots
            }
        }
    }
    if (!chord || (chord->staff()->is_tab_staff(chord->tick()) && !chord->staff()->staff_type(chord->tick())->stem_through())) {
        return;
    }
    std::vector<Note *> top_down_notes;
    std::vector<Note *> bottom_up_notes;
    std::vector<int> anchored_dots;
    // construct combined chords using the notes from overlapping chords
    get_note_list_for_dots(chord, top_down_notes, bottom_up_notes, anchored_dots);

    for (Note *note : notes) {
        bool on_line = !(note->line() & 1);
        if (on_line) {
            std::unordered_map<int, Note *> already_added;
            bool finished = false;
            for (Note *other_note : bottom_up_notes) {
                // Make sure invisible dots have no effect on visible dots, but are still layed out sensibly
                if (other_note->visible() != note->visible()) {
                    continue;
                }
                int dot_move = other_note->dot_position() == DirectionV::UP ? -1 : 1;
                int other_dot_loc = other_note->line() + dot_move;
                bool added = already_added.count(other_dot_loc);
                if (!added && muse::contains(anchored_dots, other_dot_loc)) {
                    dot_move = -dot_move; // if the desired space is taken, adjust opposite
                } else if (added && already_added[other_dot_loc] != other_note) {
                    dot_move = -dot_move;
                }
                // set y for this note
                if (note == other_note) {
                    ChordLayout::set_dot_relative_line(note, dot_move, ctx);
                    finished = true;
                    anchored_dots.push_back(note->line() + dot_move);
                    already_added[other_note->line() + dot_move] = other_note;
                    break;
                }
            }
            if (!finished) {
                already_added.clear();
                for (Note *other_note : top_down_notes) {
                    if (other_note->visible() != note->visible()) {
                        continue;
                    }
                    int dot_move = other_note->dot_position() == DirectionV::DOWN ? 1 : -1;
                    int other_dot_loc = other_note->line() + dot_move;
                    bool added = already_added.count(other_dot_loc);
                    if (!added && muse::contains(anchored_dots, other_dot_loc)) {
                        dot_move = -dot_move;
                    } else if (added && already_added[other_dot_loc] != other_note) {
                        dot_move = -dot_move;
                    }
                    // set y for this note
                    if (note == other_note) {
                        ChordLayout::set_dot_relative_line(note, dot_move, ctx);
                        finished = true;
                        anchored_dots.push_back(note->line() + dot_move);
                        break;
                    }
                    if (!added) {
                        already_added[other_note->line() + dot_move] = other_note;
                    }
                }
            }
            IF_ASSERT_FAILED(finished) {
                // this should never happen
                // the note is on a line and topDownNotes and bottomUpNotes are all of the lined notes
                ChordLayout::set_dot_relative_line(note, 0, ctx);
            }
        } else {
            // on a space; usually this means the dot is on this same line, but there is an exception
            // for a unison within the same chord.
            for (Note *other_note : note->chord()->notes()) {
                if (note == other_note) {
                    ChordLayout::set_dot_relative_line(note, 0, ctx); // same space as notehead
                    break;
                }
                if (note->line() == other_note->line()) {
                    bool adjust_down = (note->chord()->voice() & 1) && !note->chord()->up();
                    ChordLayout::set_dot_relative_line(note, adjust_down ? 2 : -2, ctx);
                    break;
                }
            }
        }
    }
}

void ChordLayout::set_dot_relative_line(Note *note, int dot_move, LayoutContext &ctx) {
    double y = dot_move / 2.0;
    if (note->staff()->is_tab_staff(note->chord()->tick())) {
        // with TAB's, dotPosX is not set:
        // get dot X from width of fret text and use TAB default spacing
        const Staff *st = note->staff();
        const StaffType *tab = st->staff_type_for_element(note);
        if (tab->stem_through()) {
            // if fret mark on lines, use standard processing
            if (!tab->on_lines()) {
                // if fret marks above lines, raise the dots by half line distance
                y = -0.5;
            } else if (dot_move == 0) {
                bool odd_voice = note->voice() & 1;
                y = odd_voice ? 0.5 : -0.5;
            } else {
                y = 0.5;
            }
        }
        // if stems beside staff, do nothing
        else {
            return;
        }
    }
    y *= note->spatium() * note->staff()->line_distance(note->tick());

    // apply to dots

    int cdots = static_cast<int>(note->chord()->dots());
    int ndots = static_cast<int>(note->dots().size());

    int n = cdots - ndots;
    for (int i = 0; i < n; ++i) {
        NoteDot *dot = Factory::create_note_dot(note);
        dot->set_track(note->track()); // needed to know the staff it belongs to (and detect tablature)
        dot->set_visible(note->visible());
        ctx.mut_dom().undo_add_element(dot);
    }
    if (n < 0) {
        for (int i = 0; i < -n; ++i) {
            ctx.mut_dom().undo_remove_element(note->dots().back());
        }
    }

    for (NoteDot *dot : note->dots()) {
        TLayout::layout_note_dot(dot, dot->mutldata());
        dot->mutldata()->set_pos_y(y);
    }
}

void ChordLayout::set_dot_x(const std::vector<Chord *> &chords, const std::array<double, 3 * ::mu::engraving::voices> &dot_pos,
                            const Staff *staff, const double up_dot_pos_x, const double down_dot_pos_x) {
    // Look for conflicts in up-stem and down-stemmed chords. If conflicts, all dots are aligned
    // to the same vertical line. If no conflicts, each chords aligns the dots individually.
    // Also check for conflicts between similarly stemmed voices where at least one voice is laid out independently
    std::set<track_idx_t> combine_chord_conflicts;
    std::set<track_idx_t> separate_chord_conflicts;
    for (Chord *chord1 : chords) {
        for (Chord *chord2 : chords) {
            if ((chord1 != chord2) &&
                ((chord1->up() && !chord2->up() && chord2->up_note()->line() - chord1->down_note()->line() < 2) ||
                 (!chord1->up() && chord2->up() && chord1->up_note()->line() - chord2->down_note()->line() < 2)) &&
                Chord::combine_voice(chord1, chord2)) {
                combine_chord_conflicts.insert({chord1->track(), chord2->track()});
            } else if ((chord1 != chord2) &&
                       ((chord1->up() && chord2->up() && chord1->up_note()->line() - chord2->down_note()->line() < 2) ||
                        (!chord1->up() && !chord2->up() && chord1->up_note()->line() - chord2->down_note()->line() < 2)) &&
                       !Chord::combine_voice(chord1, chord2)) {
                separate_chord_conflicts.insert({chord1->track(), chord2->track()});
            }
        }
    }

    const double max_pos_x = std::max(up_dot_pos_x, down_dot_pos_x);
    for (Chord *chord : chords) {
        if (chord_has_dots_all_invisible(chord)) {
            continue;
        }
        const bool combine_voices = chord->should_combine_voice();
        const size_t idx = (::mu::engraving::voices * (chord->staff_idx() - staff->idx() + 1)) + chord->voice();
        if (!combine_chord_conflicts.empty()) {
            // There are conflicts
            if (muse::contains(combine_chord_conflicts, chord->track())) {
                // In this voice
                chord->set_dot_pos_x(max_pos_x);
            } else {
                // Elsewhere
                // If combining voices, set to max pos, if separating set to own dot pos
                chord->set_dot_pos_x(combine_voices ? max_pos_x : dot_pos.at(idx));
            }
        } else {
            // There are no conflicts
            if (combine_voices) {
                if (muse::contains(separate_chord_conflicts, chord->track())) {
                    // Set to own dot pos if this is conflicting with a chord set to lay out independently
                    chord->set_dot_pos_x(dot_pos.at(idx));
                } else {
                    // Combine with other voices
                    if (chord->up()) {
                        chord->set_dot_pos_x(up_dot_pos_x);
                    } else {
                        chord->set_dot_pos_x(down_dot_pos_x);
                    }
                }
            } else {
                // Separate
                chord->set_dot_pos_x(dot_pos.at(idx));
            }
        }
    }
}

//---------------------------------------------------------
//   layoutChords3
//    - calculate positions of dots
//---------------------------------------------------------

void ChordLayout::layout_chords3(const std::vector<Chord *> &chords, const std::vector<Note *> &notes, const Staff *staff,
                                 LayoutContext &ctx) {
    Fraction tick = notes.front()->chord()->segment()->tick();
    const MStyle &style = ctx.conf().style();
    double sp = staff->spatium(tick);
    double step_distance = sp * staff->line_distance(tick) * .5;
    int step_offset = staff->staff_type(tick)->step_offset();

    double up_dot_pos_x = 0.0;
    double down_dot_pos_x = 0.0;
    // Track dot position of voices on this stave and possible cross voices from above & below
    std::array<double, 3 * ::mu::engraving::voices> dot_pos{};

    int n_notes = int(notes.size());

    for (int i = n_notes - 1; i >= 0; --i) {
        Note *note = notes[i];
        DirectionV dot_position = note->user_dot_position();
        const Chord *chord = note->chord();
        if (chord->dots()) {
            if (dot_position == DirectionV::AUTO && n_notes > 1 && note->visible() && !note->dots_hidden()) {
                // resolve dot conflicts
                int line = note->line();
                Note *above = (i < n_notes - 1) ? notes[i + 1] : nullptr;
                if (above && (!above->visible() || above->dots_hidden() || above->chord()->dots() == 0 ||
                              !Chord::combine_voice(chord, above->chord()))) {
                    above = nullptr;
                }
                int interval_above = above ? line - above->line() : 1000;
                Note *below = (i > 0) ? notes[i - 1] : nullptr;
                if (below && (!below->visible() || below->dots_hidden() || below->chord()->dots() == 0 ||
                              !Chord::combine_voice(chord, below->chord()))) {
                    below = nullptr;
                }
                int interval_below = below ? below->line() - line : 1000;
                if ((line & 1) == 0) {
                    // line
                    if (interval_above == 1 && interval_below != 1) {
                        dot_position = DirectionV::DOWN;
                    } else if (interval_below == 1 && interval_above != 1) {
                        dot_position = DirectionV::UP;
                    } else if (interval_above == 0 || interval_below == 0) {
                        // unison
                        dot_position = DirectionV::AUTO; // unison conflicts taken care of later
                    }
                } else {
                    // space
                    if (interval_above == 0 && above->chord()->dots()) {
                        // unison
                        if (!(note->voice() & 1)) {
                            dot_position = DirectionV::UP; // space, doesn't matter
                        } else {
                            if (!(above->voice() & 1)) {
                                above->set_dot_position(DirectionV::UP);
                            } else {
                                dot_position = DirectionV::DOWN;
                            }
                        }
                    }
                }
            }
        }
        if (dot_position == DirectionV::AUTO) {
            dot_position = note->voice() & 1 ? DirectionV::DOWN : DirectionV::UP;
        }
        note->set_dot_position(dot_position);
    }

    // Now, we can resolve note conflicts as a superchord
    place_dots(chords, notes, ctx);

    // Calculate the chords' dotPosX, and find the leftmost point for accidental layout
    for (Chord *chord : chords) {
        bool up_value = chord->up();

        if (chord->stem_slash()) {
            TLayout::layout_stem_slash(chord->stem_slash(), chord->stem_slash()->mutldata(), ctx.conf());
        }

        double overlap_mirror;
        Stem *stem = chord->stem();
        if (stem) {
            overlap_mirror = stem->line_width_mag();
        } else if (chord->duration_type().head_type() == NoteHeadType::HEAD_WHOLE) {
            overlap_mirror = style.style_absolute(Sid::STEM_WIDTH) * chord->mag();
        } else {
            overlap_mirror = 0.0;
        }

        double min_dot_pos_x = 0.0;

        std::vector<Note *> chord_notes = chord->notes();
        std::sort(chord_notes.begin(), chord_notes.end(), [](Note *n1, const Note *n2) -> bool { return n1->line() < n2->line(); });
        for (Note *note : chord_notes) {
            double note_x = 0.0;
            if (note->ldata()->mirror()) {
                if (up_value) {
                    note_x = StemLayout::stem_pos_x(chord) - overlap_mirror;
                } else {
                    note_x = -note->head_body_width() + overlap_mirror;
                }
            } else if (up_value) {
                note_x = chord->note_head_width() - note->head_body_width();
            }

            double ny = (note->line() + step_offset) * step_distance;
            if (note->ldata()->pos().y() != ny) {
                note->mutldata()->set_pos_y(ny);
                if (stem) {
                    TLayout::layout_stem(chord->stem(), chord->stem()->mutldata(), ctx.conf());
                    if (chord->hook()) {
                        chord->hook()->mutldata()->set_pos_y(chord->stem()->flag_position().y());
                    }
                }
            }

            const double dot_x = note_x + note->head_body_width() + chord->pos().x();
            if (chord->dots()) {
                const size_t idx = (::mu::engraving::voices * (chord->staff_idx() - staff->idx() + 1)) + chord->voice();
                dot_pos.at(idx) = std::max(dot_pos.at(idx), dot_x);
            }

            note->mutldata()->set_pos_x(note_x);
            min_dot_pos_x = std::max(min_dot_pos_x, dot_x);
        }

        //---------------------------------------------------
        //    layout dots simply
        //     we will check for conflicts after all the notes have been processed
        //---------------------------------------------------

        if (chord->dots()) {
            if (chord_has_dots_all_invisible(chord)) {
                chord->set_dot_pos_x(min_dot_pos_x);
            } else if (chord->up()) {
                up_dot_pos_x = std::max(up_dot_pos_x, min_dot_pos_x);
            } else {
                down_dot_pos_x = std::max(down_dot_pos_x, min_dot_pos_x);
            }
        }
    }

    set_dot_x(chords, dot_pos, staff, up_dot_pos_x, down_dot_pos_x);
}

void ChordLayout::layout_ledger_lines(const std::vector<Chord *> &chords, LayoutContext &ctx) {
    for (Chord *item : chords) {
        update_ledger_lines(item, ctx);
        for (Chord *grace : item->grace_notes()) {
            update_ledger_lines(grace, ctx);
        }
    }
}

//---------------------------------------------------------
//   getNoteListForDots
//      This method populates three lists: one for chord notes that need to be checked from the top down,
//      one for chords from the bottom up, and one for spaces (where the dot will be in that space)
//---------------------------------------------------------

void ChordLayout::get_note_list_for_dots(Chord *c, std::vector<Note *> &top_down_notes, std::vector<Note *> &bottom_up_notes,
                                         std::vector<int> &anchored_dots) {
    Measure *measure = c->measure();
    bool has_voices = measure->has_voices(c->v_staff_idx(), c->tick(), c->ticks(), true);
    bool has_upper_cross_notes = false;
    bool has_lower_cross_notes = false;
    const TrackRange part_track_range = c->part()->track_range();
    staff_idx_t part_top_staff = track2staff(part_track_range.start_track);
    staff_idx_t part_bottom_staff = track2staff(part_track_range.end_track);
    track_idx_t start_voice = c->track() - c->voice();
    // Get the last track we need to check for cross staff notes.
    // Either 1 stave away from the stave we are laying out or the bottom staff of the part
    track_idx_t last_voice = std::min(c->v_staff_idx() + 2, part_bottom_staff) * ::mu::engraving::voices;

    // Check for cross staff notes on staff above without dots
    if (part_top_staff != c->v_staff_idx()) {
        for (size_t i = part_top_staff * ::mu::engraving::voices; i < (part_top_staff + 1) * ::mu::engraving::voices; ++i) {
            if (Chord *voice_chord = measure->find_chord(c->tick(), i)) {
                if (voice_chord->v_staff_idx() == c->v_staff_idx()) {
                    has_upper_cross_notes = true;
                    start_voice = i;
                    break;
                }
            }
        }
    }

    // Check for cross staff notes on stave below
    if (part_bottom_staff != c->v_staff_idx()) {
        for (size_t i = (c->v_staff_idx() + 1) * ::mu::engraving::voices; i < last_voice; ++i) {
            if (Chord *voice_chord = measure->find_chord(c->tick(), i)) {
                if (voice_chord->v_staff_idx() == c->v_staff_idx()) {
                    has_lower_cross_notes = true;
                    break;
                }
            }
        }
    }

    if (!has_voices && !(has_upper_cross_notes || has_lower_cross_notes)) {
        // only this voice, so topDownNotes is just the notes in the chord
        for (Note *note : c->notes()) {
            if (note->line() & 1) {
                int new_offset = 0;
                bool adjust_down = (c->voice() & 1) && !c->up();
                if (!anchored_dots.empty() && anchored_dots.back() == note->line()) {
                    if (anchored_dots.size() >= 2 && anchored_dots[anchored_dots.size() - 2] == note->line() + (adjust_down ? 2 : -2)) {
                        new_offset = adjust_down ? -2 : 2;
                    } else {
                        new_offset = adjust_down ? 2 : -2;
                    }
                }
                anchored_dots.push_back(note->line() + new_offset);
            } else {
                top_down_notes.push_back(note);
            }
        }
    } else {
        // Get a list of notes in this staff that adjust dots from top down,
        // bottom up, and also start our locked-in dot list by adding all lines where dots are
        // guaranteed
        last_voice = has_lower_cross_notes ? last_voice : (c->v_staff_idx() + 1) * ::mu::engraving::voices;
        // Check staves above and below for moved chords (If staff below is available)
        for (size_t i = start_voice; i < last_voice; ++i) {
            if (Chord *voice_chord = measure->find_chord(c->tick(), i)) {
                // Skip chords on adjacent staves which have not been moved to this staff
                if (voice_chord->v_staff_idx() != c->v_staff_idx()) {
                    continue;
                }
                bool start_from_top = !((voice_chord->voice() & 1) && !voice_chord->up()) && voice_chord->staff_move() != -1;
                if (start_from_top) {
                    for (Note *note : voice_chord->notes()) {
                        if (note->line() & 1) {
                            anchored_dots.push_back(note->line());
                        } else {
                            top_down_notes.push_back(note);
                        }
                    }
                } else {
                    for (Note *note : voice_chord->notes()) {
                        if (note->line() & 1) {
                            anchored_dots.push_back(note->line());
                        } else {
                            bottom_up_notes.push_back(note);
                        }
                    }
                }
            }
        }
    }
    // our two lists now contain only notes that are on lines
    std::sort(top_down_notes.begin(), top_down_notes.end(), [](Note *n1, Note *n2) { return n1->line() < n2->line(); });
    std::sort(bottom_up_notes.begin(), bottom_up_notes.end(), [](Note *n1, Note *n2) { return n1->line() > n2->line(); });
}

void ChordLayout::append_grace_notes(Chord *chord) {
    Segment *segment = chord->segment();
    Measure *measure = chord->measure();
    track_idx_t track = chord->track();
    staff_idx_t staff_idx = chord->staff_idx();
    GraceNotesGroup &gnb = chord->update_grace_notes_before_group();
    GraceNotesGroup &gna = chord->update_grace_notes_after_group();

    //Attach graceNotesBefore of this chord to *this* segment
    if (!gnb.empty()) {
        // If this segment already contains grace notes in the same voice (could happen if a
        // previous chord has appended grace-notes-after here) put them in the same vector.
        EngravingItem *item = segment->pre_appended_item(track);
        if (item && item->is_grace_notes_group()) {
            GraceNotesGroup *gng = to_grace_notes_group(item);
            gng->insert(gng->end(), gnb.begin(), gnb.end());
        } else {
            gnb.set_appended_segment(segment);
            segment->pre_append(&gnb, track);
        }
    }

    //Attach graceNotesAfter of this chord to the *following* segment
    if (!gna.empty()) {
        Segment *following_seg = measure->tick2segment(chord->end_tick(), SegmentType::ALL);
        while (following_seg && (!following_seg->has_elements(staff2track(staff_idx), staff2track(staff_idx + 1) - 1) ||
                                 following_seg->is_time_tick_type())) {
            // If there is nothing on this staff, go to next segment
            following_seg = following_seg->next();
        }
        if (following_seg) {
            gna.set_appended_segment(following_seg);
            following_seg->pre_append(&gna, track);
        }
    }
}

/* Grace-notes-after have the special property of belonging to
*  a segment but being pre-appended to another. This repositioning
*  is needed and must be called AFTER horizontal spacing is calculated. */
void ChordLayout::reposition_grace_notes_after(Segment *segment, size_t tracks) {
    for (track_idx_t track = 0; track < tracks; track++) {
        EngravingItem *item = segment->pre_appended_item(track);
        if (!item || !item->is_grace_notes_group()) {
            continue;
        }
        GraceNotesGroup *gng = to_grace_notes_group(item);
        for (Chord *chord : *gng) {
            double offset = segment->ldata()->pos().x() - chord->layout_parent()->layout_parent()->ldata()->pos().x();
            // Difference between the segment they "belong" and the segment they are "appended" to.
            chord->set_pos(chord->ldata()->pos().x() + offset, 0.0);
        }
    }
}

void ChordLayout::clear_line_attach_points(Measure *measure) {
    for (Segment &s : measure->segments()) {
        if (!s.is_chord_rest_type()) {
            continue;
        }
        for (EngravingItem *e : s.elist()) {
            if (!e || !e->is_chord()) {
                continue;
            }
            Chord *c = to_chord(e);
            for (Note *n : c->notes()) {
                n->line_attach_points().clear();
            }
            for (Chord *ch : c->grace_notes()) {
                for (Note *n : ch->notes()) {
                    n->line_attach_points().clear();
                }
            }
        }
    }
}

/* We perform a pre-layout of ties and glissandi to obtain the attach points and attach them to
 * the notes of the chord. Will be needed for spacing calculation, particularly to
 * enforce minTieLength. The true layout of ties and glissandi is done much later. */
void ChordLayout::update_line_attach_points(Chord *chord, bool is_first_in_measure, LayoutContext &ctx) {
    if (chord->ends_note_anchored_line()) {
        for (Note *note : chord->notes()) {
            for (Spanner *sp : note->spanner_back()) {
                if (sp->is_glissando()) {
                    TLayout::layout_glissando(to_glissando(sp), ctx);
                } else if (sp->is_guitar_bend()) {
                    const StaffType *staff_type = chord->staff_type();
                    bool is_dive_on_tab = to_guitar_bend(sp)->is_dive() && staff_type && staff_type->is_tab_staff();
                    if (!is_dive_on_tab) {
                        TLayout::layout_guitar_bend(to_guitar_bend(sp), ctx);
                    }
                } else if (sp->is_note_line()) {
                    TLayout::layout_note_line(to_note_line(sp), ctx);
                }
            }
        }
    }
    if (is_first_in_measure) {
        for (Note *note : chord->notes()) {
            Tie *tie_back = note->tie_back();
            if (tie_back && (note->incoming_partial_tie() || tie_back->start_note()->find_measure() != note->find_measure())) {
                SlurTieLayout::layout_tie_back(tie_back, note->find_measure()->system(), ctx);
            }
        }
    }
    for (Note *note : chord->notes()) {
        Tie *tie = note->tie_for();
        if (tie) {
            if (tie->is_partial_tie()) {
                SlurTieLayout::layout_tie_for(tie, note->find_measure()->system()); // line attach points are updated here
            }

            Note *end_note = tie->end_note();
            if (!end_note) {
                continue;
            }
            const Measure *end_note_measure = end_note->find_measure();
            const Measure *note_measure = note->find_measure();
            if (end_note_measure == note_measure || end_note_measure->system() != note_measure->system()) {
                SlurTieLayout::layout_tie_for(tie, note->find_measure()->system()); // line attach points are updated here
            }
        }
    }

    SlurTieLayout::layout_laissez_vib_chord(chord, ctx);
}

void ChordLayout::layout_chord_base_fingering(Chord *chord, System *system, LayoutContext &) {
    std::set<staff_idx_t> shapes_to_recreate;
    std::vector<Note *> notes;
    Segment *segment = chord->segment();
    for (auto gc : chord->grace_notes()) {
        for (auto n : gc->notes()) {
            notes.push_back(n);
        }
    }
    for (auto n : chord->notes()) {
        notes.push_back(n);
    }
    std::vector<Fingering *> fingerings;
    for (Note *note : notes) {
        for (EngravingItem *el : note->el()) {
            if (el->is_fingering()) {
                Fingering *f = to_fingering(el);
                if (f->layout_type() == ElementType::CHORD && !f->is_on_cross_beam_side()) {
                    // Fingering on top of cross-staff beams must be laid out later
                    if (f->place_above()) {
                        fingerings.push_back(f);
                    } else {
                        fingerings.insert(fingerings.begin(), f);
                    }
                }
            }
        }
    }
    for (Fingering *f : fingerings) {
        TLayout::layout_fingering(f, f->mutldata());
        if (f->add_to_skyline()) {
            Note *n = f->note();
            RectF r = f->ldata()->bbox().translated(f->pos() + n->pos() + n->chord()->pos() + segment->pos() + segment->measure()->pos());
            system->staff(f->note()->chord()->v_staff_idx())->skyline().add(r, f);
        }
        shapes_to_recreate.insert(f->staff_idx());
    }
    for (staff_idx_t staff_idx : shapes_to_recreate) {
        segment->create_shape(staff_idx);
    }
}

void ChordLayout::cross_measure_setup(Chord *chord, bool on, LayoutContext &ctx) {
    if (!on) {
        if (chord->cross_measure() != CrossMeasure::UNKNOWN) {
            chord->set_cross_measure(CrossMeasure::UNKNOWN);
            layout_stem(chord, ctx);
        }
        return;
    }
    if (chord->cross_measure() == CrossMeasure::UNKNOWN) {
        CrossMeasure temp_cross = CrossMeasure::NONE; // assume no cross-measure modification
        // if chord has only one note and note is tied forward
        if (chord->notes().size() == 1 && chord->notes().at(0)->tie_for()) {
            Chord *tied_chord = chord->notes().at(0)->tie_for()->end_note()->chord();
            // if tied note belongs to another measure and to a single-note chord
            if (tied_chord->measure() != chord->measure() && tied_chord->notes().size() == 1) {
                // get total duration
                std::vector<TDuration> dur_list =
                    to_duration_list(chord->actual_duration_type().fraction() + tied_chord->actual_duration_type().fraction(), true);
                // if duration can be expressed as a single duration
                // apply cross-measure modification
                if (dur_list.size() == 1) {
                    temp_cross = CrossMeasure::FIRST;
                    chord->set_cross_measure(temp_cross);
                    chord->set_cross_measure_duration_type(dur_list[0]);
                    layout_stem(chord, ctx);
                }
            }
            chord->set_cross_measure(temp_cross);
            tied_chord->set_cross_measure(temp_cross == CrossMeasure::FIRST ? CrossMeasure::SECOND : CrossMeasure::NONE);
        }
    }
}

// called after final position of note is set
void ChordLayout::layout_note2(Note *item, LayoutContext &ctx) {
    Chord *chord = item->chord();
    const Staff *staff = item->staff();
    if (!staff) {
        return;
    }

    Note::LayoutData *ldata = item->mutldata();
    const StaffType *staff_type = staff->staff_type_for_element(item);
    // for standard staves this is done in Score::layoutChords3()
    // so that the results are available there
    const bool is_tab_staff = staff_type && staff_type->is_tab_staff();
    // First, for tab staves that have show back-tied fret marks option, we add parentheses to the tied note if
    // the tie spans a system boundary. This can't be done in layout as the system of each note is not decided yet
    const ShowTiedFret show_tied_fret = item->style().value(Sid::TAB_SHOW_TIED_FRET).value<ShowTiedFret>();
    const bool tie_back_paren = is_tab_staff && !item->fixed() && item->tie_back() &&
                                (show_tied_fret != ShowTiedFret::TIE_AND_FRET || item->is_continuation_of_bend()) &&
                                !item->should_hide_fret();
    bool use_parens = (tie_back_paren || item->ghost()) && !item->hide_generated_parens();

    if (item->harmonic() && item->display_fret() != Note::DisplayFretOption::NATURAL_HARMONIC) {
        use_parens = false;
    }

    if (use_parens) {
        ldata->has_generated_parens = true;
    } else if (!item->ghost() || item->should_hide_fret() || item->hide_generated_parens()) {
        ldata->has_generated_parens = false;
    }

    int dots = chord->dots();
    if (dots && !item->dots().empty()) {
        if (chord->slash() && !item->visible()) {
            item->set_dots_hidden(true);
        }
        // if chords have notes with different mag, dots must still  align
        double correct_mag = chord->notes().size() > 1 ? chord->mag() : item->mag();
        double d = ctx.conf().point(ctx.conf().style_s(Sid::DOT_NOTE_DISTANCE)) * correct_mag;
        double dd = ctx.conf().point(ctx.conf().style_s(Sid::DOT_DOT_DISTANCE)) * correct_mag;
        double x = is_tab_staff ? chord->dot_pos_x() - item->pos().x() : chord->dot_pos_x() - item->pos().x() - chord->pos().x();
        // in case of dots with different size, center-align them
        if (item->mag() != chord->mag() && chord->notes().size() > 1) {
            double relative_mag = item->mag() / chord->mag();
            double center_align_offset = item->dot(0)->width() * (1 / relative_mag - 1) / 2;
            x += center_align_offset;
        }
        // adjust dot distance for hooks
        Hook *hook = chord->hook();
        if (chord->up() && hook && hook->visible()) {
            double hook_right = hook->width() + hook->x() + chord->pos().x();
            double hook_bottom = hook->height() + hook->y() + chord->pos().y() + (0.25 * item->spatium());
            // the top dot in the chord, not the dot for this particular note:
            double dot_y = chord->notes().back()->y() + chord->notes().back()->dots().front()->pos().y();
            if (chord->dot_pos_x() < hook_right && dot_y < hook_bottom) {
                d = hook->width();
            }
        }
        // if TAB and stems through staff
        if (is_tab_staff && staff_type->stem_through()) {
            // with TAB's, dot Y is not calculated during layoutChords3(),
            // as layoutChords3() is not even called for TAB's;
            // setDotRelativeLine() actually also manages creation/deletion of NoteDot's
            ChordLayout::set_dot_relative_line(item, 0, ctx);

            // use TAB default note-to-dot spacing
            dd = ::stafftype_tab_defaultdotdist_x.to_absolute(item->spatium());
            d = dd * 0.5;
        }

        // apply to dots
        // Invisible dots are laid out separately,
        // with a distance of 0.1sp to distinguish from visible ones
        double visible_x = x + d;
        double invisible_x = x;

        for (NoteDot *dot : item->dots()) {
            if (dot->visible()) {
                dot->mutldata()->set_pos_x(visible_x);
                visible_x += dd;
            } else {
                invisible_x += (0.1 * item->spatium());
                dot->mutldata()->set_pos_x(invisible_x);
                invisible_x += item->sym_width(SymId::AUGMENTATION_DOT) * dot->mag();
            }
        }
    }

    // layout elements attached to note
    for (EngravingItem *e : item->el()) {
        if (e->is_symbol()) {
            e->mutldata()->set_mag(item->mag());
            TLayout::layout_item(e, ctx);
        } else if (e->is_fingering()) {
            // don't set mag; fingerings should not scale with note
            Fingering *f = to_fingering(e);
            if (f->property_flags(Pid::PLACEMENT) == PropertyFlags::STYLED) {
                f->set_placement(f->calculate_placement());
            }
            // layout fingerings that are placed relative to notehead
            // fingerings placed relative to chord will be laid out later
            if (f->layout_type() == ElementType::NOTE) {
                TLayout::layout_fingering(f, f->mutldata());
            }
        } else {
            e->mutldata()->set_mag(item->mag());
            TLayout::layout_item(e, ctx);
        }
    }

    TLayout::fill_note_shape(item, ldata);
}

void ChordLayout::create_paren_groups(Chord *chord) {
    std::vector<Note *> add_parens;
    std::vector<Note *> remove_parens;

    for (Note *note : chord->notes()) {
        const NoteParenthesisInfo *note_paren_info = note->parenthesis_info();
        const Parenthesis *left_paren = note_paren_info ? note_paren_info->left_paren() : nullptr;
        bool paren_generated = left_paren && left_paren->generated();

        if (note->ldata()->has_generated_parens()) {
            if (note_paren_info) {
                if (paren_generated) {
                    EditChord::remove_chord_parentheses(chord, {note}, false, true);
                } else {
                    continue;
                }
            }
            add_parens.push_back(note);
            note->undo_change_property(Pid::HAS_PARENTHESES, ParenthesesMode::BOTH);
        } else if (paren_generated) {
            remove_parens.push_back(note);
        }
    }

    EditChord::add_chord_parentheses(chord, add_parens, false, true);
    EditChord::remove_chord_parentheses(chord, remove_parens, false, true);
}

void ChordLayout::check_start_end_slurs(Chord *chord, LayoutContext &ctx) {
    chord->start_end_slurs().reset();
    for (Spanner *spanner : chord->starting_spanners()) {
        if (!spanner->is_slur()) {
            continue;
        }
        Slur *slur = to_slur(spanner);
        SlurTieLayout::compute_up(slur, ctx);
        if (slur->up()) {
            chord->start_end_slurs().start_up = true;
        } else {
            chord->start_end_slurs().start_down = true;
        }
        // Check if end chord has been connected to this slur. If not, connect it.
        if (!slur->end_chord()) {
            continue;
        }
        slur->end_chord()->add_ending_spanner(slur);
    }
    for (Spanner *spanner : chord->ending_spanners()) {
        if (!spanner->is_slur()) {
            continue;
        }
        if (to_slur(spanner)->up()) {
            chord->start_end_slurs().end_up = true;
        } else {
            chord->start_end_slurs().end_down = true;
        }
    }
}

void ChordLayout::check_and_fill_shape(const ChordRest *item, ChordRest::LayoutData *ldata, const LayoutConfiguration &conf) {
#ifdef MUE_ENABLE_ENGRAVING_LD_ACCESS
    Shape origin = ldata->shape(LdAccess::PASS);
#endif

    fill_shape(item, ldata, conf);

#ifdef MUE_ENABLE_ENGRAVING_LD_ACCESS
    Shape fixed = ldata->shape(LdAccess::PASS);
    if (!origin.equal(fixed)) {
        LOGE() << "Shape not actual for item: " << item->type_name();
    }
#endif
}

void ChordLayout::fill_shape(const ChordRest *item, Chord::LayoutData *ldata, const LayoutConfiguration &conf) {
    switch (item->type()) {
        case ElementType::CHORD: fill_shape(to_chord(item), static_cast<Chord::LayoutData *>(ldata)); break;
        case ElementType::MEASURE_REPEAT: fill_shape(to_measure_repeat(item), static_cast<MeasureRepeat::LayoutData *>(ldata), conf); break;
        default: RestLayout::fill_shape(to_rest(item), static_cast<Rest::LayoutData *>(ldata), conf); break;
    }
}

Shape ChordLayout::chord_rest_shape(const ChordRest *item) {
    Shape shape;
    {
        for (Lyrics *l : item->lyrics()) {
            if (!l || !l->add_to_skyline() || l->xml_text().empty()) {
                continue;
            }
            RectF bbox = l->ldata()->bbox().translated(l->pos());
            shape.add_horizontal_spacing(l, bbox.left(), bbox.right());
        }
    }

    return shape;
}

bool ChordLayout::leave_space_for_tie(const Articulation *item) {
    if (!item->ownership_parent() || !item->ownership_parent()->is_chord()) {
        return false;
    }

    Chord *chord = to_chord(item->chord_rest());
    bool up = item->ldata()->up;
    Note *note = up ? chord->up_note() : chord->down_note();
    Tie *tie_for = note->tie_for();
    Tie *tie_back = note->tie_back();

    if (!tie_for && !tie_back) {
        return false;
    }

    bool leave_space = (tie_for && tie_for->up() == up && tie_for->is_outer_tie_of_chord(Grip::START)) ||
                       (tie_back && tie_back->up() == up && tie_back->is_outer_tie_of_chord(Grip::END));

    return leave_space;
}

void ChordLayout::fill_shape(const Chord *item, ChordRest::LayoutData *ldata) {
    Shape shape(Shape::Type::COMPOSITE);

    Hook *hook = item->hook();
    if (hook) {
        LD_CONDITION(hook->ldata()->is_set_shape());
    }

    Stem *stem = item->stem();
    if (stem) {
        LD_CONDITION(stem->ldata()->is_set_shape());
    }

    StemSlash *stem_slash = item->stem_slash();
    if (stem_slash) {
        LD_CONDITION(stem_slash->ldata()->is_set_shape());
    }

    TremoloSingleChord *tremolo = item->tremolo_single_chord();
    if (tremolo) {
        LD_CONDITION(tremolo->ldata()->is_set_shape());
    }

    Arpeggio *arpeggio = item->arpeggio();
    if (arpeggio) {
        LD_CONDITION(arpeggio->ldata()->is_set_shape());
    }

    Arpeggio *span_arpeggio = item->span_arpeggio();
    if (span_arpeggio) {
        LD_CONDITION(span_arpeggio->ldata()->is_set_shape());
    }

    BeamSegment *beamlet = item->beamlet();

    if (hook && hook->add_to_skyline()) {
        shape.add(hook->shape().translate(hook->pos()));
    }

    if (stem && stem->add_to_skyline()) {
        shape.add(stem->shape().translate(stem->pos()));
    }

    if (stem_slash && stem_slash->add_to_skyline()) {
        shape.add(stem_slash->shape().translate(stem_slash->pos()));
    }

    if (tremolo && tremolo->add_to_skyline()) {
        shape.add(tremolo->shape().translate(tremolo->pos()));
    }

    if (arpeggio && arpeggio->add_to_skyline()) {
        shape.add(arpeggio->shape().translate(arpeggio->pos()));
    }

    if (span_arpeggio && !arpeggio && span_arpeggio->v_staff_idx() != item->v_staff_idx() && span_arpeggio->add_to_skyline()) {
        PointF span_arp_pos = span_arpeggio->pos() - (item->page_pos() - span_arpeggio->chord()->page_pos());
        shape.add(span_arpeggio->shape().translate(span_arp_pos));
    }

    for (Note *note : item->notes()) {
        shape.add(note->shape().translate(note->pos()));
    }

    if (item->tab_dur() && item->tab_dur()->add_to_skyline()) {
        shape.add(item->tab_dur()->shape().translate(item->tab_dur()->pos()));
    }

    for (const NoteParenthesisInfo *paren_info : item->note_parentheses()) {
        Parenthesis *left_paren = paren_info->left_paren();
        Parenthesis *right_paren = paren_info->right_paren();

        if (left_paren && left_paren->add_to_skyline()) {
            shape.add(left_paren->shape().translate(left_paren->pos()));
        }
        if (right_paren && right_paren->add_to_skyline()) {
            shape.add(right_paren->shape().translate(right_paren->pos()));
        }
    }

    for (EngravingItem *e : item->el()) {
        if (e->add_to_skyline()) {
            shape.add(e->shape().translate(e->pos()));
        }
    }

    shape.add(chord_rest_shape(item)); // add lyrics

    for (const LedgerLine *l : item->ledger_lines()) {
        shape.add(l->shape().translate(l->pos()));
    }

    if (beamlet && stem) {
        double x_pos = beamlet->line.p1().x() - stem->ldata()->pos().x();
        if (beamlet->is_before && !item->up()) {
            x_pos -= stem->width();
        } else if (!beamlet->is_before && item->up()) {
            x_pos += stem->width();
        }
        shape.add(beamlet->shape().translate(PointF(-x_pos, 0.0)));
    }

    for (const Articulation *a : item->articulations()) {
        // Only add lv to shape
        if (a->is_laissez_vib()) {
            shape.add(a->shape().translate(a->pos()));
        }
    }

    const Parenthesis *left_paren = item->left_paren();
    if (left_paren && left_paren->add_to_skyline()) {
        shape.add(left_paren->ldata()->shape().translated(left_paren->pos()));
    }
    const Parenthesis *right_paren = item->right_paren();
    if (right_paren && right_paren->add_to_skyline()) {
        shape.add(right_paren->ldata()->shape().translated(right_paren->pos()));
    }

    ldata->set_shape(shape);
}

void ChordLayout::fill_shape(const MeasureRepeat *item, MeasureRepeat::LayoutData *ldata, const LayoutConfiguration &) {
    Shape shape(Shape::Type::COMPOSITE);

    shape.add(item->number_rect(), item);
    shape.add(item->sym_bbox(ldata->sym_id), item);

    ldata->set_shape(shape);
}

void ChordLayout::add_line_attach_points(Spanner *spanner) {
    assert(spanner->anchor() == Spanner::Anchor::NOTE);

    const SpannerSegment *front_seg = to_spanner_segment(spanner->front_segment());
    const SpannerSegment *back_seg = to_spanner_segment(spanner->back_segment());
    Note *start_note = nullptr;
    Note *end_note = nullptr;

    EngravingItem *start_element = spanner->start_element();
    EngravingItem *end_element = spanner->end_element();
    if (start_element && start_element->is_note()) {
        start_note = to_note(start_element);
    }
    if (end_element && end_element->is_note()) {
        end_note = to_note(end_element);
    }
    if (!front_seg || !back_seg || !start_note || !end_note) {
        return;
    }
    double start_x = front_seg->ldata()->pos().x();
    double end_x = back_seg->pos2().x() + back_seg->ldata()->pos().x(); // because pos2 is relative to ipos
    // Here we don't pass y() because its value is unreliable during the first stages of layout.
    // The y() is irrelevant anyway for horizontal spacing.
    start_note->add_start_line_attach_point(PointF(start_x, 0.0), spanner);
    end_note->add_end_line_attach_point(PointF(end_x, 0.0), spanner);
}
