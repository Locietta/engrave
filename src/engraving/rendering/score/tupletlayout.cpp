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
#include "log.h"
#include "tupletlayout.h"

#include "dom/chordrest.h"
#include "dom/tuplet.h"
#include "dom/text.h"
#include "dom/factory.h"
#include "dom/stafftype.h"
#include "dom/chord.h"
#include "dom/note.h"
#include "dom/stem.h"
#include "dom/system.h"
#include "dom/measure.h"

#include "tlayout.h"
#include "autoplace.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void TupletLayout::layout(Tuplet *item, LayoutContext &ctx) {
    Tuplet::LayoutData *ldata = item->mutldata();
    if (item->elements().empty()) {
        LOGD("Tuplet::layout(): tuplet is empty");
        return;
    }

    const StaffType *stt = item->staff_type();
    if (stt && stt->is_tab_staff() && stt->stemless()) {
        // is in a TAB without stems, skip any format: tuplets are not shown
        return;
    }

    create_number(item, ctx);

    compute_direction(item);

    const ChordRest *cr1 = nullptr;
    const ChordRest *cr2 = nullptr;
    compute_start_end_cr(item, &cr1, &cr2);

    bool has_bracket = item->calc_has_bracket(cr1, cr2);
    item->set_has_bracket(has_bracket);

    double mag = (cr1->mag() + cr2->mag()) / 2;
    ldata->set_mag(mag);

    item->set_pos(0.0, 0.0);

    layout_bracket(item, cr1, cr2, ctx);

    // collect bounding box
    RectF r;
    if (item->number()) {
        r |= item->number()->ldata()->bbox().translated(item->number()->pos());
        if (item->has_bracket()) {
            RectF b;
            b.set_coords(item->bracket_l[1].x(), item->bracket_l[1].y(), item->bracket_r[2].x(), item->bracket_r[2].y());
            r |= b;
        }
    } else if (item->has_bracket()) {
        RectF b;
        b.set_coords(item->bracket_l[1].x(), item->bracket_l[1].y(), item->bracket_l[3].x(), item->bracket_l[3].y());
        r |= b;
    }
    item->setbbox(r);

    if (!item->cross()) {
        Autoplace::autoplace_measure_element(item, item->mutldata(), item->is_up(), /* add to skyline */ true);
    }
}

void TupletLayout::layout_tuplet_and_nested_tuplets(Tuplet *t, LayoutContext &ctx) {
    const std::vector<DurationElement *> elements = t->elements();
    for (auto rev_iter = elements.rbegin(); rev_iter != elements.rend(); ++rev_iter) {
        DurationElement *d = *rev_iter;
        if (d == t) {
            continue;
        }
        // if element is tuplet, layoutTuplet(that tuplet)
        if (d->is_tuplet()) {
            layout_tuplet_and_nested_tuplets(to_tuplet(d), ctx);
        }
    }
    // layout t
    layout(t, ctx);
}

bool TupletLayout::is_top_tuplet(ChordRest *cr) {
    Tuplet *t = cr->tuplet();
    if (t && t->elements().front() == cr) {
        // find top level tuplet
        while (t->tuplet()) {
            t = t->tuplet();
        }
        // consider tuplet cross if anything moved within it
        if (t->cross()) {
            return false;
        } else {
            return true;
        }
    }

    // no tuplet or not first element
    return false;
}

bool TupletLayout::not_top_tuplet(ChordRest *cr) {
    Tuplet *t = cr->tuplet();
    if (t && t->elements().front() == cr) {
        // find top level tuplet
        while (t->tuplet()) {
            t = t->tuplet();
        }
        // consider tuplet cross if anything moved within it
        if (t->cross()) {
            return true;
        } else {
            return false;
        }
    }

    // no tuplet or not first element
    return false;
}

void TupletLayout::create_number(Tuplet *item, LayoutContext &ctx) {
    if (item->number_type() == TupletNumberType::NO_TEXT) {
        if (item->number()) {
            if (item->number()->selected()) {
                ctx.deselect(item->number());
            }
            delete item->number();
            item->set_number(nullptr);
        }

        return;
    }

    if (!item->number()) {
        Text *number = Factory::create_text(item, TextStyleType::TUPLET);
        number->set_composition(true);
        number->set_track(item->track());
        number->set_visible(item->visible());
        number->set_color(item->color());
        item->set_number(number);
        item->reset_number_property();
    }

    // tuplet properties are propagated to number automatically by setProperty()
    // but we need to make sure flags are as well
    item->number()->set_property_flags(Pid::FONT_FACE, item->property_flags(Pid::FONT_FACE));
    item->number()->set_property_flags(Pid::FONT_SIZE, item->property_flags(Pid::FONT_SIZE));
    item->number()->set_property_flags(Pid::FONT_STYLE, item->property_flags(Pid::FONT_STYLE));
    item->number()->set_property_flags(Pid::ALIGN, item->property_flags(Pid::ALIGN));
    item->number()->set_property_flags(Pid::POSITION, item->property_flags(Pid::POSITION));

    const MStyle &style = ctx.conf().style();

    String number_string = (item->number_type() == TupletNumberType::SHOW_NUMBER) ?
                               String(u"%1").arg(item->ratio().numerator()) :
                               String(u"%1:%2").arg(item->ratio().numerator(), item->ratio().denominator());
    if (style.style_b(Sid::TUPLET_USE_SYMBOLS)) {
        String smufl_num;
        for (size_t i = 0; i < number_string.size(); ++i) {
            smufl_num.append(u"<sym>tuplet");
            smufl_num.append(number_string.at(i).unicode());
            smufl_num.append(u"</sym>");
        }
        smufl_num.replace(String(u":"), String(u"Colon"));
        item->number()->set_xml_text(smufl_num);
    } else {
        item->number()->set_xml_text(number_string);
    }

    item->set_is_small(true);
    for (const DurationElement *e : item->elements()) {
        if ((e->is_chord_rest() && !to_chord_rest(e)->is_small()) || (e->is_tuplet() && !to_tuplet(e)->is_small())) {
            item->set_is_small(false);
            break;
        }
    }

    item->number()->mutldata()->set_mag(item->is_small() ? style.style_d(Sid::SMALL_NOTE_MAG) : 1.0);
}

void TupletLayout::compute_direction(Tuplet *item) {
    if (item->direction() != DirectionV::AUTO) {
        item->set_is_up(item->direction() == DirectionV::UP);
        return;
    }

    int up = 0;
    for (const DurationElement *e : item->elements()) {
        if (e->is_chord()) {
            const Chord *c = to_chord(e);
            if (c->stem_direction() != DirectionV::AUTO) {
                up += c->stem_direction() == DirectionV::UP ? 1000 : -1000;
            } else {
                up += c->up() ? 1 : -1;
            }
        }
    }
    if (up == 0) {
        // this is a tuplet full of rests, default to up but also take voices into consideration
        Measure *m = item->measure();
        if (m && m->has_voices(item->staff_idx())) {
            up = item->voice() % 2 == 0 ? 1 : -1;
        } else {
            up = 1; // default up
        }
    }

    item->set_is_up(up > 0);
}

void TupletLayout::compute_start_end_cr(Tuplet *item, const ChordRest **cr1, const ChordRest **cr2) {
    const DurationElement *start_el = item->elements().front();
    while (start_el->is_tuplet()) {
        const Tuplet *t = to_tuplet(start_el);
        if (t->elements().empty()) {
            break;
        }
        start_el = t->elements().front();
    }
    assert(start_el && start_el->is_chord_rest());

    const DurationElement *end_el = item->elements().back();
    while (end_el->is_tuplet()) {
        const Tuplet *t = to_tuplet(end_el);
        if (t->elements().empty()) {
            break;
        }
        end_el = t->elements().back();
    }
    assert(end_el && end_el->is_chord_rest());

    *cr1 = to_chord_rest(start_el);
    *cr2 = to_chord_rest(end_el);
}

void TupletLayout::layout_bracket(Tuplet *item, const ChordRest *cr1, const ChordRest *cr2, LayoutContext &ctx) {
    const MStyle &style = ctx.conf().style();
    double spatium = item->spatium();

    double max_slope = style.style_d(Sid::TUPLET_MAX_SLOPE);
    bool out_of_staff = style.style_b(Sid::TUPLET_OUT_OF_STAFF);
    double v_head_distance = style.style_absolute(Sid::TUPLET_V_HEAD_DISTANCE) * item->mag();
    double v_stem_distance = style.style_absolute(Sid::TUPLET_V_STEM_DISTANCE) * item->mag();
    double stem_left =
        (style.style_absolute(Sid::TUPLET_STEM_LEFT_DISTANCE) - style.style_absolute(Sid::TUPLET_BRACKET_WIDTH) / 2) * cr1->mag();
    double stem_right =
        (style.style_absolute(Sid::TUPLET_STEM_RIGHT_DISTANCE) - style.style_absolute(Sid::TUPLET_BRACKET_WIDTH) / 2) * cr2->mag();
    double note_left =
        (style.style_absolute(Sid::TUPLET_NOTE_LEFT_DISTANCE) - style.style_absolute(Sid::TUPLET_BRACKET_WIDTH) / 2) * cr1->mag();
    double note_right =
        (style.style_absolute(Sid::TUPLET_NOTE_RIGHT_DISTANCE) - style.style_absolute(Sid::TUPLET_BRACKET_WIDTH) / 2) * cr2->mag();

    int move = 0;
    if (out_of_staff && cr1->is_chord_rest() && cr2->is_chord_rest()) {
        // account for staff move when adjusting bracket to avoid staff
        // but don't attempt adjustment unless both endpoints are in same staff
        if (to_chord_rest(cr1)->staff_move() == to_chord_rest(cr2)->staff_move()) {
            move = to_chord_rest(cr1)->staff_move();
        } else {
            out_of_staff = false;
        }
    }

    double l1 = style.style_absolute(Sid::TUPLET_BRACKET_HOOK_HEIGHT) * item->mag();
    double l2l = v_head_distance; // left bracket vertical distance
    double l2r = v_head_distance; // right bracket vertical distance

    if (item->is_up()) {
        v_head_distance = -v_head_distance;
    }

    item->set_p1(cr1->page_pos());
    item->set_p2(cr2->page_pos());

    item->p1().rx() -= note_left;
    item->p2().rx() += ctx.conf().note_head_width() + note_right;
    item->p1().ry() += v_head_distance; // TODO: Direction ?
    item->p2().ry() += v_head_distance;

    double xx1 = item->p1().x(); // use to center the number on the beam

    double left_note_edge = 0.0; // page coordinates
    double right_note_edge = 0.0;
    if (cr1->is_chord()) {
        const Chord *chord1 = to_chord(cr1);
        left_note_edge = chord1->up() ? chord1->down_note()->page_bounding_rect().left() : chord1->up_note()->page_bounding_rect().left();
    }
    if (cr2->is_chord()) {
        const Chord *chord2 = to_chord(cr2);
        right_note_edge =
            chord2->up() ? chord2->down_note()->page_bounding_rect().right() : chord2->up_note()->page_bounding_rect().right();
    }

    if (item->is_up()) {
        if (cr1->is_chord()) {
            const Chord *chord1 = to_chord(cr1);
            Stem *stem = chord1->stem();
            if (stem) {
                xx1 = stem->page_bounding_rect().x();
            }
            if (chord1->up() && stem) {
                item->p1().ry() = stem->page_bounding_rect().y();
                l2l = v_stem_distance;
                item->p1().rx() = stem->page_bounding_rect().left() - stem_left;
            } else {
                item->p1().ry() = chord1->up_note()->page_bounding_rect().top();
                item->p1().rx() = left_note_edge - note_left;
            }
        }

        if (cr2->is_chord()) {
            const Chord *chord2 = to_chord(cr2);
            Stem *stem = chord2->stem();
            if (stem && chord2->up()) {
                item->p2().ry() = stem->page_bounding_rect().top();
                l2r = v_stem_distance;
                item->p2().rx() = stem->page_bounding_rect().right() + stem_right;
            } else {
                item->p2().ry() = chord2->up_note()->page_bounding_rect().top();
                item->p2().rx() = right_note_edge + note_right;
            }
        }
        //
        // special case: one of the bracket endpoints is
        // a rest
        //
        if (!cr1->is_chord() && cr2->is_chord()) {
            if (item->p2().y() < item->p1().y()) {
                item->p1().set_y(item->p2().y());
            } else {
                item->p2().set_y(item->p1().y());
            }
        } else if (cr1->is_chord() && !cr2->is_chord()) {
            if (item->p1().y() < item->p2().y()) {
                item->p2().set_y(item->p1().y());
            } else {
                item->p1().set_y(item->p2().y());
            }
        }

        // outOfStaff
        if (out_of_staff) {
            double min = cr1->measure()->staff_page_bounding_rect(cr1->staff_idx() + move).y();
            if (min < item->p1().y()) {
                item->p1().ry() = min;
                l2l = v_stem_distance;
            }
            min = cr2->measure()->staff_page_bounding_rect(cr2->staff_idx() + move).y();
            if (min < item->p2().y()) {
                item->p2().ry() = min;
                l2r = v_stem_distance;
            }
        }

        // check that slope is no more than max
        double d = (item->p2().y() - item->p1().y()) / (item->p2().x() - item->p1().x());
        if (d < -max_slope) {
            // move p1 y up
            item->p1().ry() = item->p2().y() + max_slope * (item->p2().x() - item->p1().x());
        } else if (d > max_slope) {
            // move p2 y up
            item->p2().ry() = item->p1().ry() + max_slope * (item->p2().x() - item->p1().x());
        }

        // check for collisions
        size_t n = item->elements().size();
        if (n >= 3) {
            d = (item->p2().y() - item->p1().y()) / (item->p2().x() - item->p1().x());
            for (size_t i = 1; i < (n - 1); ++i) {
                const EngravingItem *e = item->elements().at(i);
                if (e->is_chord()) {
                    const Chord *chord = to_chord(e);
                    const Stem *stem = chord->stem();
                    if (stem) {
                        RectF r(chord->up() ? stem->page_bounding_rect() : chord->up_note()->page_bounding_rect());
                        double y3 = r.top();
                        double x3 = r.x() + r.width() * .5;
                        double y0 = item->p1().y() + (x3 - item->p1().x()) * d;
                        double c = y0 - y3;
                        if (c > 0) {
                            item->p1().ry() -= c;
                            item->p2().ry() -= c;
                        }
                    }
                }
            }
        }
    } else {
        if (cr1->is_chord()) {
            const Chord *chord1 = to_chord(cr1);
            Stem *stem = chord1->stem();
            if (stem) {
                xx1 = stem->page_bounding_rect().x();
            }
            if (!chord1->up() && stem) {
                item->p1().ry() = stem->page_bounding_rect().bottom();
                l2l = v_stem_distance;
                item->p1().rx() = stem->page_bounding_rect().left() - stem_left;
            } else {
                item->p1().ry() = chord1->down_note()->page_bounding_rect().bottom();
                item->p1().rx() = left_note_edge - note_left;
            }
        }

        if (cr2->is_chord()) {
            const Chord *chord2 = to_chord(cr2);
            Stem *stem = chord2->stem();
            if (stem && !chord2->up()) {
                item->p2().ry() = stem->page_bounding_rect().bottom();
                l2r = v_stem_distance;
                item->p2().rx() = stem->page_bounding_rect().right() + stem_right;
            } else {
                item->p2().ry() = chord2->down_note()->page_bounding_rect().bottom();
                item->p2().rx() = right_note_edge + note_right;
            }
        }
        //
        // special case: one of the bracket endpoints is
        // a rest
        //
        if (!cr1->is_chord() && cr2->is_chord()) {
            if (item->p2().y() > item->p1().y()) {
                item->p1().set_y(item->p2().y());
            } else {
                item->p2().set_y(item->p1().y());
            }
        } else if (cr1->is_chord() && !cr2->is_chord()) {
            if (item->p1().y() > item->p2().y()) {
                item->p2().set_y(item->p1().y());
            } else {
                item->p1().set_y(item->p2().y());
            }
        }
        // outOfStaff
        if (out_of_staff) {
            double max = cr1->measure()->staff_page_bounding_rect(cr1->staff_idx() + move).bottom();
            if (max > item->p1().y()) {
                item->p1().ry() = max;
                l2l = v_stem_distance;
            }
            max = cr2->measure()->staff_page_bounding_rect(cr2->staff_idx() + move).bottom();
            if (max > item->p2().y()) {
                item->p2().ry() = max;
                l2r = v_stem_distance;
            }
        }
        // check that slope is no more than max
        double d = (item->p2().y() - item->p1().y()) / (item->p2().x() - item->p1().x());
        if (d < -max_slope) {
            // move p1 y up
            item->p2().ry() = item->p1().y() - max_slope * (item->p2().x() - item->p1().x());
        } else if (d > max_slope) {
            // move p2 y up
            item->p1().ry() = item->p2().ry() - max_slope * (item->p2().x() - item->p1().x());
        }

        // check for collisions
        size_t n = item->elements().size();
        if (n >= 3) {
            d = (item->p2().y() - item->p1().y()) / (item->p2().x() - item->p1().x());
            for (size_t i = 1; i < (n - 1); ++i) {
                EngravingItem *e = item->elements()[i];
                if (e->is_chord()) {
                    const Chord *chord = to_chord(e);
                    const Stem *stem = chord->stem();
                    if (stem) {
                        RectF r(chord->up() ? chord->down_note()->page_bounding_rect() : stem->page_bounding_rect());
                        double y3 = r.bottom();
                        double x3 = r.x() + r.width() * .5;
                        double y0 = item->p1().y() + (x3 - item->p1().x()) * d;
                        double c = y0 - y3;
                        if (c < 0) {
                            item->p1().ry() -= c;
                            item->p2().ry() -= c;
                        }
                    }
                }
            }
        }
    }

    if (!cr1->is_chord()) {
        item->p1().rx() = cr1->page_bounding_rect().left() - note_left;
    }
    if (!cr2->is_chord()) {
        Shape shape = cr2->ldata()->shape();
        auto sh_el = shape.find_if([](const ShapeElement &i) { return i.item() && i.item()->is_chord_rest(); });
        DO_ASSERT(sh_el);
        item->p2().rx() = sh_el->translated(cr2->page_pos()).right() + note_right;
    }

    PointF mp(item->layout_parent()->page_pos());
    if (item->layout_parent()->is_measure()) {
        System *s = to_measure(item->layout_parent())->system();
        if (s) {
            SysStaff *tuplet_staff = s->staff(item->v_staff_idx());
            mp.ry() += tuplet_staff ? tuplet_staff->y() : 0.0;
        }
    }
    item->p1() -= mp;
    item->p2() -= mp;

    item->p1() += item->user_p1();
    item->p2() += item->user_p2();
    xx1 -= mp.x();

    item->p1().ry() -= l2l * (item->is_up() ? 1.0 : -1.0);
    item->p2().ry() -= l2r * (item->is_up() ? 1.0 : -1.0);

    const double y_offset = item->staff_offset_y();

    item->p1().ry() -= y_offset;
    item->p2().ry() -= y_offset;

    if (style.style_b(Sid::TUPLET_EXTEND_TO_END_OF_DURATION)) {
        extend_to_end_of_duration(item, to_chord_rest(cr2));
    }

    // l2l l2r, mp, _p1, _p2 const

    // center number
    double x_number = 0.0;
    double number_width = 0.0;
    if (item->number()) {
        Text::LayoutData *num_ldata = item->number()->mutldata();
        TLayout::layout_text(item->number(), num_ldata);
        number_width = num_ldata->bbox().width();

        double y_number = item->p1().y() + (item->p2().y() - item->p1().y()) * .5 - l1 * (item->is_up() ? 1.0 : -1.0);

        if (place_number_on_rhythmic_center(item, cr1, cr2, ctx)) {
            x_number = compute_rhythmic_center(item, cr2);
        } else if (cr1->beam() && cr2->beam() && cr1->beam() == cr2->beam() && !item->has_bracket()) {
            // for beamed tuplets, center number on beam - if they don't have a bracket
            const ChordRest *crr = to_chord_rest(cr1);
            if (item->is_up() == crr->up()) {
                double deltax = cr2->page_pos().x() - cr1->page_pos().x();
                x_number = xx1 + deltax * .5;
            } else {
                double deltax = item->p2().x() - item->p1().x();
                x_number = item->p1().x() + deltax * .5;
            }
        } else {
            // otherwise center on the bracket (TODO: make centering rules customizable?)
            double deltax = item->p2().x() - item->p1().x();
            x_number = item->p1().x() + deltax * .5;
        }

        num_ldata->set_pos(PointF(x_number, y_number) - item->ldata()->pos());
    }

    if (item->has_bracket()) {
        double slope = (item->p2().y() - item->p1().y()) / (item->p2().x() - item->p1().x());
        const double number_gap = 0.35 * spatium;

        if (item->is_up()) {
            if (item->number()) {
                //set width of bracket hole
                double x = x_number - number_width * .5 - number_gap;
                item->p1().rx() = std::min(item->p1().x(), x - 0.5 * l1); // ensure enough space for the number
                double y = item->p1().y() + (x - item->p1().x()) * slope;
                item->bracket_l[0] = PointF(item->p1().x(), item->p1().y());
                item->bracket_l[1] = PointF(item->p1().x(), item->p1().y() - l1);
                item->bracket_l[2] = PointF(x, y - l1);

                //set width of bracket hole
                x = x_number + number_width * .5 + number_gap;
                item->p2().rx() = std::max(item->p2().x(), x + 0.5 * l1); // ensure enough space for the number
                y = item->p1().y() + (x - item->p1().x()) * slope;
                item->bracket_r[0] = PointF(x, y - l1);
                item->bracket_r[1] = PointF(item->p2().x(), item->p2().y() - l1);
                item->bracket_r[2] = PointF(item->p2().x(), item->p2().y());
            } else {
                item->bracket_l[0] = PointF(item->p1().x(), item->p1().y());
                item->bracket_l[1] = PointF(item->p1().x(), item->p1().y() - l1);
                item->bracket_l[2] = PointF(item->p2().x(), item->p2().y() - l1);
                item->bracket_l[3] = PointF(item->p2().x(), item->p2().y());
            }
        } else {
            if (item->number()) {
                //set width of bracket hole
                double x = x_number - number_width * .5 - number_gap;
                item->p1().rx() = std::min(item->p1().x(), x - 0.5 * l1); // ensure enough space for the number
                double y = item->p1().y() + (x - item->p1().x()) * slope;
                item->bracket_l[0] = PointF(item->p1().x(), item->p1().y());
                item->bracket_l[1] = PointF(item->p1().x(), item->p1().y() + l1);
                item->bracket_l[2] = PointF(x, y + l1);

                //set width of bracket hole
                x = x_number + number_width * .5 + number_gap;
                item->p2().rx() = std::max(item->p2().x(), x + 0.5 * l1);
                y = item->p1().y() + (x - item->p1().x()) * slope;
                item->bracket_r[0] = PointF(x, y + l1);
                item->bracket_r[1] = PointF(item->p2().x(), item->p2().y() + l1);
                item->bracket_r[2] = PointF(item->p2().x(), item->p2().y());
            } else {
                item->bracket_l[0] = PointF(item->p1().x(), item->p1().y());
                item->bracket_l[1] = PointF(item->p1().x(), item->p1().y() + l1);
                item->bracket_l[2] = PointF(item->p2().x(), item->p2().y() + l1);
                item->bracket_l[3] = PointF(item->p2().x(), item->p2().y());
            }
        }
    }
}

bool TupletLayout::place_number_on_rhythmic_center(Tuplet *item, const ChordRest *cr1, const ChordRest *cr2, LayoutContext &ctx) {
    if (ctx.conf().style_b(Sid::TUPLET_NUMBER_RYTHMIC_CENTER) && !is_symmetric(item, cr1, cr2)) {
        Fraction center = center_tick(item);
        if (cr2->tick() <= center) {
            return ctx.conf().style_b(Sid::TUPLET_EXTEND_TO_END_OF_DURATION);
        }

        return true;
    }

    return false;
}

bool TupletLayout::is_symmetric(Tuplet *item, const ChordRest *cr1, const ChordRest *cr2) {
    Fraction end_tick = cr2->end_tick();

    std::vector<Segment *> tuplet_segments;
    tuplet_segments.reserve(item->elements().size());

    for (Segment *segment = cr1->segment(); segment && segment->tick() < end_tick; segment = segment->next_active()) {
        tuplet_segments.push_back(segment);
    }

    size_t segments_count = tuplet_segments.size();
    for (size_t i = 0; i < segments_count; ++i) {
        size_t j = segments_count - 1 - i;
        if (j <= i) {
            break;
        }
        Segment *first_seg = tuplet_segments[i];
        Segment *second_seg = tuplet_segments[j];
        if (first_seg->ticks() != second_seg->ticks()) {
            return false;
        }
    }

    return true;
}

double TupletLayout::compute_rhythmic_center(Tuplet *item, const ChordRest *end_chord) {
    Fraction center = center_tick(item);

    staff_idx_t track = item->track();
    const Segment *ref_seg = nullptr;
    const ChordRest *ref_cr = nullptr;
    for (Segment *seg = end_chord->segment(); seg; seg = seg->prev(SegmentType::CHORD_REST)) {
        ref_seg = seg;
        ref_cr = to_chord_rest(seg->element(track));
        if (!ref_cr) {
            continue;
        }
        if (seg->tick() <= center) {
            break;
        }
    }

    IF_ASSERT_FAILED(ref_seg && ref_cr) { return 0.0; }

    if (ref_seg->tick() == center) {
        double x_ref = 0.0;
        if (ref_cr->is_chord()) {
            const Chord *chord = to_chord(ref_cr);
            bool chord_up = chord->up();
            bool tuplet_place_above = item->is_up();
            x_ref = 0.5 * chord->up_note()->head_width();
            if (chord_up == tuplet_place_above) {
                // Stem-side correction
                x_ref += (chord_up ? 0.25 : -0.25) * chord->up_note()->head_width();
            }
        } else {
            x_ref = 0.5 * ref_cr->width();
        }
        return ref_seg->x() + x_ref;
    }

    Fraction ref_cr_ticks = ref_cr->actual_ticks();
    Fraction tick_diff = center - ref_cr->tick();
    double tick_ratio = (tick_diff / ref_cr_ticks).to_double();

    const Segment *next_seg = item->measure()->find_segment(SegmentType::CHORD_REST, ref_seg->tick() + ref_cr_ticks);
    double x_ref = ref_seg->x() + (ref_cr->is_chord() ? to_chord(ref_cr)->up_note()->head_width() : ref_cr->width());
    double ref_width = next_seg ? next_seg->x() - x_ref : ref_seg->width();

    double relative_width = ref_width * tick_ratio;

    return x_ref + relative_width;
}

Fraction TupletLayout::center_tick(Tuplet *item) {
    Fraction start_tick = item->tick();
    Fraction base_len = item->base_len().ticks();
    Fraction ratio = item->ratio();
    Fraction subdivision = item->base_len().ticks() / item->ratio();
    Fraction end_tick = start_tick + base_len * ratio.denominator();

    Fraction center_tick = (start_tick + end_tick - subdivision) / 2;

    return center_tick;
}

void TupletLayout::extend_to_end_of_duration(Tuplet *item, const ChordRest *end_cr) {
    Fraction base_duration = item->base_len().ticks();
    if (end_cr->ticks() <= base_duration) {
        return;
    }

    Fraction last_tuplet_subdivision = end_cr->end_tick() - base_duration / item->ratio();
    Segment *ref_segment = end_cr->segment();
    while (ref_segment) {
        Segment *next_cr_seg = ref_segment->next1(SegmentType::CHORD_REST);
        if (!next_cr_seg || next_cr_seg->tick() > last_tuplet_subdivision) {
            break;
        }
        ref_segment = next_cr_seg;
    }

    Fraction tick_ratio = (last_tuplet_subdivision - ref_segment->tick()) / ref_segment->ticks();

    double x_result = ref_segment->page_pos().x() + ref_segment->width() * tick_ratio.to_double() + item->score()->note_head_width();

    const double padding = 0.6 * item->spatium();

    Segment *next_seg = ref_segment->next1_with_elems_on_staff(end_cr->v_staff_idx(), ~SegmentType::TIME_TICK);
    x_result = std::min(x_result, next_seg->page_pos().x() - padding);

    track_idx_t start_track = staff2track(end_cr->v_staff_idx());
    track_idx_t end_track = start_track + ::mu::engraving::voices;
    if (next_seg->is_chord_rest_type()) {
        for (track_idx_t track = start_track; track < end_track; ++track) {
            if (ChordRest *chord_rest = to_chord_rest(next_seg->element(track))) {
                if (Tuplet *next_tuplet = chord_rest->tuplet()) {
                    if (next_tuplet->elements().front() == chord_rest && next_tuplet->is_up() == item->is_up()) {
                        double x_start_of_next_tuplet = next_tuplet->page_pos().x() + next_tuplet->p1().x();
                        x_result = std::min(x_result, x_start_of_next_tuplet - padding);
                    }
                }
            }
        }
    }

    Shape next_seg_shape = next_seg->staff_shape(end_cr->v_staff_idx());
    next_seg_shape.translate(PointF(next_seg->page_pos().x(), next_seg->system()->staff(end_cr->v_staff_idx())->y()));
    double y_above = item->p2().y() - (item->is_up() ? item->style().style_absolute(Sid::TUPLET_BRACKET_HOOK_HEIGHT) : 0.0);
    double y_below = item->p2().y() + (item->is_up() ? 0.0 : item->style().style_absolute(Sid::TUPLET_BRACKET_HOOK_HEIGHT));
    double left = next_seg_shape.left_most_edge_at_height(y_above, y_below);
    x_result = std::min(x_result, left - padding);

    double cur_pos = item->p2().x();
    item->p2().rx() = std::max(cur_pos, x_result - item->measure()->page_pos().x());
}
