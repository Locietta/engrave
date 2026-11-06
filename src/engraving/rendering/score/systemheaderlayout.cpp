/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited
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

#include <climits>

#include "systemheaderlayout.h"

#include "systemlayout.h"
#include "tlayout.h"

#include "dom/bracket.h"
#include "dom/factory.h"
#include "dom/part.h"
#include "dom/staff.h"
#include "dom/system.h"
#include "style/defaultstyle.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

double SystemHeaderLayout::layout_brackets(System *system, LayoutContext &ctx) {
    size_t nstaves = system->staves().size();
    size_t columns = system->get_brackets_columns_count();

    std::vector<double> bracket_width(columns, 0.0);

    std::vector<Bracket *> bl;
    bl.swap(system->brackets());

    for (size_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
        for (size_t i = 0; i < columns; ++i) {
            for (BracketItem *bi : ctx.dom().brackets(staff_idx)) {
                if (bi->column() != i || bi->bracket_type() == BracketType::NO_BRACKET) {
                    continue;
                }
                Bracket *b = create_bracket(system, ctx, bi, i, static_cast<int>(staff_idx), bl, system->first_measure());
                if (b != nullptr) {
                    b->mutldata()->bracket_height.set_value(3.5 * b->spatium() * 2); // dummy
                    TLayout::layout_bracket(b, b->mutldata(), ctx.conf());
                    if (bi->bracket_type() != BracketType::GROUP) { // Because Group brackets always stay outside
                        bracket_width[i] = std::max(bracket_width[i], b->ldata()->bracket_width());
                    }
                }
            }
        }
    }

    for (Bracket *b : bl) {
        delete b;
    }

    double total_bracket_width = 0.0;

    if (!system->brackets().empty()) {
        for (double w : bracket_width) {
            total_bracket_width += w;
        }
    }

    return total_bracket_width;
}

Bracket *SystemHeaderLayout::create_bracket(System *system, LayoutContext &ctx, BracketItem *bi, size_t column, staff_idx_t staff_idx,
                                            std::vector<Bracket *> &bl, Measure *measure) {
    if (!measure) {
        return nullptr;
    }

    size_t nstaves = system->staves().size();
    staff_idx_t first_staff = staff_idx;
    staff_idx_t last_staff = staff_idx + bi->bracket_span() - 1;
    if (last_staff >= nstaves) {
        last_staff = nstaves - 1;
    }

    for (; first_staff <= last_staff; ++first_staff) {
        if (system->staff(first_staff)->show()) {
            break;
        }
    }
    for (; last_staff >= first_staff; --last_staff) {
        if (system->staff(last_staff)->show()) {
            break;
        }
    }
    size_t span = last_staff - first_staff + 1;
    //
    // do not show bracket if it only spans one
    // system due to some invisible staves
    //
    if (span > 1 || (bi->bracket_span() == span) ||
        (span == 1 && ctx.conf().style_b(Sid::ALWAYS_SHOW_BRACKETS_WHEN_EMPTY_STAVES_ARE_HIDDEN) &&
         bi->bracket_type() != BracketType::SQUARE) ||
        (span == 1 && ctx.conf().style_b(Sid::ALWAYS_SHOW_SQUARE_BRACKETS_WHEN_EMPTY_STAVES_ARE_HIDDEN) &&
         bi->bracket_type() == BracketType::SQUARE)) {
        //
        // this bracket is visible
        //
        Bracket *b = 0;
        track_idx_t track = staff_idx * ::mu::engraving::voices;
        for (size_t k = 0; k < bl.size(); ++k) {
            if (bl[k]->track() == track && bl[k]->column() == column && bl[k]->bracket_type() == bi->bracket_type() &&
                bl[k]->measure() == measure) {
                b = muse::take_at(bl, k);
                break;
            }
        }
        if (b == 0) {
            b = Factory::create_bracket(ctx.mut_dom().dummy_parent());
            b->set_bracket_item(bi);
            b->set_generated(true);
            b->set_track(track);
            b->set_measure(measure);
        }
        system->add(b);

        if (bi->selected()) {
            bool need_select = true;

            std::vector<EngravingItem *> brackets = ctx.selection().elements(ElementType::BRACKET);
            for (const EngravingItem *element : brackets) {
                if (to_bracket(element)->bracket_item() == bi) {
                    need_select = false;
                    break;
                }
            }

            if (need_select) {
                ctx.select(b, SelectType::ADD);
            }
        }

        b->set_staff_span(first_staff, last_staff);

        InstrumentLabelVisibility visibility =
            resolve_instrument_label_visibility(ctx.dom().staff(first_staff), measure->tick(), ctx, ctx.state().first_system());
        b->set_label_type(resolve_instrument_name_type(visibility));

        return b;
    }

    return nullptr;
}

void SystemHeaderLayout::add_brackets(System *system, Measure *measure, LayoutContext &ctx) {
    if (system->staves().empty()) { // ignore vbox
        return;
    }

    size_t nstaves = system->staves().size();

    //---------------------------------------------------
    //  find x position of staves
    //    create brackets
    //---------------------------------------------------

    size_t columns = system->get_brackets_columns_count();

    std::vector<Bracket *> bl;
    bl.swap(system->brackets());

    for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
        for (size_t i = 0; i < columns; ++i) {
            for (BracketItem *bi : ctx.dom().brackets(staff_idx)) {
                if (bi->column() != i || bi->bracket_type() == BracketType::NO_BRACKET) {
                    continue;
                }
                create_bracket(system, ctx, bi, i, staff_idx, bl, measure);
            }
        }
        if (!system->staff(staff_idx)->show()) {
            continue;
        }
    }

    //---------------------------------------------------
    //  layout brackets
    //---------------------------------------------------
    layout_brackets_vertical(system, ctx);

    set_brackets_x_position(system, measure->x());

    muse::join(system->brackets(), bl);
}

double SystemHeaderLayout::total_bracket_offset(LayoutContext &ctx) {
    if (ctx.state().total_brackets_width() >= 0) {
        return ctx.state().total_brackets_width();
    }

    size_t columns = 0;
    for (const Staff *staff : ctx.dom().staves()) {
        for (const BracketItem *bi : ctx.dom().brackets(staff->idx())) {
            columns = std::max(columns, bi->column() + 1);
        }
    }

    size_t nstaves = ctx.dom().nstaves();
    std::vector<double> bracket_width(nstaves, 0.0);
    for (staff_idx_t staff_idx = 0; staff_idx < nstaves; ++staff_idx) {
        for (BracketItem *bi : ctx.dom().brackets(staff_idx)) {
            if (bi->bracket_type() == BracketType::NO_BRACKET || !bi->visible() || bi->bracket_type() == BracketType::GROUP) {
                continue;
            }

            //! This logic is partially copied from System::createBracket.
            //! Of course, we don't need to worry about invisible staves,
            //! but we do need to worry about brackets that span past the
            //! last staff.
            staff_idx_t first_staff = staff_idx;
            staff_idx_t last_staff = staff_idx + bi->bracket_span() - 1;
            if (last_staff >= nstaves) {
                last_staff = nstaves - 1;
            }

            for (; first_staff <= last_staff; ++first_staff) {
                if (ctx.dom().staff(first_staff)->show()) {
                    break;
                }
            }
            for (; last_staff >= first_staff; --last_staff) {
                if (ctx.dom().staff(last_staff)->show()) {
                    break;
                }
            }

            size_t span = last_staff - first_staff + 1;
            if (span > 1 || (bi->bracket_span() == span) ||
                (span == 1 && ctx.conf().style_b(Sid::ALWAYS_SHOW_BRACKETS_WHEN_EMPTY_STAVES_ARE_HIDDEN))) {
                Bracket *dummy_br = Factory::create_bracket(ctx.mut_dom().dummy_parent(), /*isAccessibleEnabled=*/false);
                dummy_br->set_bracket_item(bi);
                dummy_br->set_staff_span(first_staff, last_staff);
                dummy_br->mutldata()->bracket_height.set_value(3.5 * dummy_br->spatium() * 2); // default
                TLayout::layout_bracket(dummy_br, dummy_br->mutldata(), ctx.conf());
                for (staff_idx_t stf_idx = first_staff; stf_idx <= last_staff; ++stf_idx) {
                    bracket_width[stf_idx] += dummy_br->ldata()->bracket_width();
                }
                delete dummy_br;
            }
        }
    }

    double total_brackets_width = 0.0;
    for (double w : bracket_width) {
        total_brackets_width = std::max(total_brackets_width, w);
    }
    ctx.mut_state().set_total_brackets_width(total_brackets_width);

    return total_brackets_width;
}

void SystemHeaderLayout::set_brackets_x_position(System *system, double x_position) {
    std::vector<Bracket *> brackets = system->brackets();
    for (Bracket *b1 : brackets) {
        BracketType bracket_type = b1->bracket_type();
        // For brackets that are drawn, we must correct for half line width
        double line_width_correction = 0.0;
        if (bracket_type == BracketType::NORMAL || bracket_type == BracketType::LINE) {
            line_width_correction = system->style().style_absolute(Sid::BRACKET_WIDTH) / 2;
        }
        // Compute offset cause by other stacked brackets
        double x_offset = 0;
        for (const Bracket *b2 : brackets) {
            if (!b2->bracket_item()->visible() || b2->bracket_type() == BracketType::GROUP) {
                continue;
            }
            if (b1->column() > b2->column() && b1->intersects(b2)) {
                x_offset += b2->ldata()->bracket_width();
            }
        }
        // Set position
        double x = x_position - x_offset - b1->ldata()->bracket_width() + line_width_correction;
        b1->mutldata()->set_pos_x(x);
    }
}

void SystemHeaderLayout::layout_brackets_vertical(System *system, LayoutContext &ctx) {
    for (Bracket *b : system->brackets()) {
        int staff_idx1 = static_cast<int>(b->first_staff());
        int staff_idx2 = static_cast<int>(b->last_staff());
        double sy = 0; // assume bracket not visible
        double ey = 0;
        // if start staff not visible, try next staff
        while (staff_idx1 <= staff_idx2 && !system->staves().at(staff_idx1)->show()) {
            ++staff_idx1;
        }
        // if end staff not visible, try prev staff
        while (staff_idx1 <= staff_idx2 && !system->staves().at(staff_idx2)->show()) {
            --staff_idx2;
        }
        // if the score doesn't have "alwaysShowBracketsWhenEmptyStavesAreHidden" as true,
        // the bracket will be shown IF:
        // it spans at least 2 visible staves (staffIdx1 < staffIdx2) OR
        // it spans just one visible staff (staffIdx1 == staffIdx2) but it is required to do so
        // (the second case happens at least when the bracket is initially dropped)
        bool not_hidden = ctx.conf().style_b(Sid::ALWAYS_SHOW_BRACKETS_WHEN_EMPTY_STAVES_ARE_HIDDEN) ?
                              (staff_idx1 <= staff_idx2) :
                              (staff_idx1 < staff_idx2) || (b->span() == 1 && staff_idx1 == staff_idx2);
        if (not_hidden) { // set vert. pos. and height to visible spanned staves
            sy = system->staves().at(staff_idx1)->bbox().top();
            ey = system->staves().at(staff_idx2)->bbox().bottom();
        }

        Bracket::LayoutData *bldata = b->mutldata();
        bldata->set_pos_y(sy);
        bldata->bracket_height = ey - sy;
        TLayout::layout_bracket(b, bldata, ctx.conf());
    }
}

bool SystemHeaderLayout::stack_labels_vertically(System *system) {
    const MStyle &style = system->style();
    // Ignore long/short overrides
    bool long_name = system->ldata()->use_long_names();
    InstrumentNamesAlign align =
        style.style_v(long_name ? Sid::INSTRUMENT_NAMES_ALIGN_LONG : Sid::INSTRUMENT_NAMES_ALIGN_SHORT).value<InstrumentNamesAlign>();
    return align == InstrumentNamesAlign::CENTER_CENTER ||
           (align == InstrumentNamesAlign::RIGHT_RIGHT && style.style_b(Sid::INSTRUMENT_NAMES_STACK_VERTICALLY));
}

void SystemHeaderLayout::compute_instrument_name_offset(System *system, LayoutContext &ctx) {
    // Get standard instrument name distance
    double instrument_name_offset = ctx.conf().style_absolute(Sid::INSTRUMENT_NAME_OFFSET);
    // Now scale it depending on the text size (which also may not follow staff scaling)
    double text_size_scaling = 1.0;
    double actual_size = 0.0;
    double default_size = 0.0;
    bool follow_staff_size = true;
    if (ctx.state().start_with_long_names()) {
        actual_size = ctx.conf().style_d(Sid::LONG_INSTRUMENT_FONT_SIZE);
        default_size = DefaultStyle::default_style().value(Sid::LONG_INSTRUMENT_FONT_SIZE).to_double();
        follow_staff_size = ctx.conf().style_b(Sid::LONG_INSTRUMENT_FONT_SPATIUM_DEPENDENT);
    } else {
        actual_size = ctx.conf().style_d(Sid::SHORT_INSTRUMENT_FONT_SIZE);
        default_size = DefaultStyle::default_style().value(Sid::SHORT_INSTRUMENT_FONT_SIZE).to_double();
        follow_staff_size = ctx.conf().style_b(Sid::SHORT_INSTRUMENT_FONT_SPATIUM_DEPENDENT);
    }
    text_size_scaling = actual_size / default_size;
    if (!follow_staff_size) {
        text_size_scaling *= DefaultStyle::default_style().value(Sid::SPATIUM).to_double() / ctx.conf().style_d(Sid::SPATIUM);
    }
    text_size_scaling = std::max(text_size_scaling, 1.0);
    instrument_name_offset *= text_size_scaling;

    system->mutldata()->set_instrument_name_offset(instrument_name_offset);
}

void SystemHeaderLayout::compute_group_brackets_widths(System *system, LayoutContext &ctx) {
    System::LayoutData *ldata = system->mutldata();
    ldata->clear_group_brackets_width();

    std::vector<Bracket *> group_brackets;
    for (Bracket *bracket : system->brackets()) {
        if (bracket->bracket_type() == BracketType::GROUP) {
            group_brackets.push_back(bracket);
        }
    }

    if (group_brackets.empty()) {
        return;
    }

    std::sort(group_brackets.begin(), group_brackets.end(), [](Bracket *b1, Bracket *b2) { return b1->column() < b2->column(); });

    bool hang_into_margin = ctx.conf().style_b(Sid::GROUP_BRACKET_HANG_TEXT_INTO_MARGIN) &&
                            ctx.conf().style_v(Sid::GROUP_BRACKET_TEXT_ORIENTATION).value<Orientation>() == Orientation::VERTICAL;
    double bracket_dist_to_names = ctx.conf().style_absolute(Sid::GROUP_BRACKET_DISTANCE_TO_NAMES);

    for (staff_idx_t staff_idx = 0; staff_idx < system->staves().size(); ++staff_idx) {
        Bracket *first_bracket_on_this_staff = nullptr;
        for (Bracket *b : group_brackets) {
            if (b->contains(staff_idx)) {
                first_bracket_on_this_staff = b;
                break;
            }
        }

        if (!first_bracket_on_this_staff) {
            continue;
        }

        std::vector<Bracket *> stack;
        double bracket_width = 0.0;
        compute_stacked_brackets_width(first_bracket_on_this_staff, group_brackets, bracket_width, stack);
        if (hang_into_margin) {
            bracket_width += stack.back()->ldata()->bbox().left();
        }

        ldata->set_group_brackets_width_at_staff_idx(staff_idx, bracket_width + bracket_dist_to_names);
    }

    if (!ctx.conf().style_b(Sid::INSTRUMENT_NAMES_ALIGN_INCLUDE_GROUP_BRACKETS)) {
        double max_group_bracket_width = 0.0;
        for (const auto &pair : ldata->group_brackets_width()) {
            max_group_bracket_width = std::max(max_group_bracket_width, pair.second);
        }
        for (staff_idx_t staff_idx = 0; staff_idx < system->staves().size(); ++staff_idx) {
            ldata->set_group_brackets_width_at_staff_idx(staff_idx, max_group_bracket_width);
        }
    }
}

void SystemHeaderLayout::compute_stacked_brackets_width(Bracket *first, const std::vector<Bracket *> &all_group_brackets_ordered_by_column,
                                                        double &width, std::vector<Bracket *> &stack) {
    stack.push_back(first);

    width += first->ldata()->bbox().width();
    double bracket_dist = first->style().style_absolute(Sid::GROUP_BRACKET_DISTANCE_TO_GROUP_BRACKET);

    for (Bracket *b2 : all_group_brackets_ordered_by_column) {
        if (muse::contains(stack, b2)) {
            continue;
        }
        if (b2->column() > first->column() && b2->intersects(first)) {
            stack.push_back(b2);
            compute_stacked_brackets_width(b2, all_group_brackets_ordered_by_column, width, stack);
            width += bracket_dist;
        }
    }
}

double SystemHeaderLayout::name_width_including_group_brackets(InstrumentName *name, System *system) {
    const System::LayoutData *ldata = system->ldata();
    double group_brackets_width = ldata->group_brackets_width_at_staff_idx(name->effective_staff_idx());
    double result = name->width();
    if (!muse::real_is_null(group_brackets_width)) {
        result += group_brackets_width;
    }
    return result;
}

void SystemHeaderLayout::compute_instrument_names_width(System *system, LayoutContext &ctx) {
    compute_group_brackets_widths(system, ctx);

    System::LayoutData *ldata = system->mutldata();
    ldata->set_first_column_width(0.0);
    ldata->set_second_column_width(0.0);
    ldata->set_total_names_width(0.0);

    std::unordered_set<Part *> parts_with_individual_staff_names;
    std::unordered_set<Part *> parts_with_group_names;

    std::vector<InstrumentName *> group_names;
    std::vector<InstrumentName *> instrument_names;
    std::vector<InstrumentName *> shared_stave_names;

    for (staff_idx_t staff_idx = 0; staff_idx < system->staves().size(); ++staff_idx) {
        const SysStaff *sys_staff = system->staff(staff_idx);
        const Staff *staff = ctx.dom().staff(staff_idx);

        if (InstrumentName *group_name = sys_staff->name(InstrumentNameRole::GROUP)) {
            if (group_name->instrument_name_type() == InstrumentNameType::LONG && group_name->effective_staff_idx() == muse::nidx) {
                // NOTE: We can only do this for long-name systems because they don't need to have
                // the left barline aligned with the other systems across the page.
                group_name->mutldata()->set_is_skip_draw(true);
                continue;
            }
            group_name->mutldata()->set_is_skip_draw(false);

            group_names.push_back(group_name);
            group_name->mutldata()->set_column(1);
            for (staff_idx_t group_idx = group_name->staff_idx(); group_idx < group_name->ldata()->end_idx_of_group(); ++group_idx) {
                parts_with_group_names.insert(ctx.dom().staff(group_idx)->part());
            }
            ldata->set_second_column_width(std::max(ldata->second_column_width(), name_width_including_group_brackets(group_name, system)));
            ldata->set_total_names_width(std::max(ldata->total_names_width(), name_width_including_group_brackets(group_name, system)));
        }

        if (!staff->show()) {
            // We know that the staff is hidden in the entire score so safe to skip
            continue;
        }

        if (InstrumentName *name = sys_staff->name(InstrumentNameRole::STAFF)) {
            if (name->instrument_name_type() == InstrumentNameType::LONG && !sys_staff->show()) {
                // NOTE: We can only do this for long-name systems because they don't need to have
                // the left barline aligned with the other systems across the page.
                name->mutldata()->set_is_skip_draw(true);
                continue;
            }
            name->mutldata()->set_is_skip_draw(false);

            name->mutldata()->set_column(0);
            parts_with_individual_staff_names.insert(ctx.dom().staff(staff_idx)->part());
            ldata->set_first_column_width(std::max(ldata->first_column_width(), name->width()));
            ldata->set_total_names_width(std::max(ldata->total_names_width(), name_width_including_group_brackets(name, system)));
        }
    }

    for (staff_idx_t staff_idx = 0; staff_idx < system->staves().size(); ++staff_idx) {
        const SysStaff *sys_staff = system->staff(staff_idx);
        const Staff *staff = ctx.dom().staff(staff_idx);
        Part *part = staff->part();

        if (!part->show() || part->visible_staves_count() == 0) {
            // We know that the part is hidden in the entire score so safe to skip
            continue;
        }

        InstrumentName *instr_name = sys_staff->name(InstrumentNameRole::PART);
        if (!instr_name) {
            instr_name = sys_staff->name(InstrumentNameRole::SHARED_STAFF);
        }
        if (!instr_name) {
            continue;
        }

        if (instr_name->instrument_name_type() == InstrumentNameType::LONG && instr_name->effective_staff_idx() == muse::nidx) {
            // NOTE: We can only do this for long-name systems because they don't need to have
            // the left barline aligned with the other systems across the page.
            instr_name->mutldata()->set_is_skip_draw(true);
            continue;
        }
        instr_name->mutldata()->set_is_skip_draw(false);

        if (instr_name->instrument_name_role() == InstrumentNameRole::PART) {
            instrument_names.push_back(instr_name);
        } else {
            shared_stave_names.push_back(instr_name);
        }

        ldata->set_total_names_width(std::max(ldata->total_names_width(), name_width_including_group_brackets(instr_name, system)));

        if (parts_with_group_names.count(part)) {
            instr_name->mutldata()->set_column(0);
            ldata->set_first_column_width(std::max(ldata->first_column_width(), instr_name->width()));
        } else {
            instr_name->mutldata()->set_column(1);
            ldata->set_second_column_width(std::max(ldata->second_column_width(), name_width_including_group_brackets(instr_name, system)));
        }
    }

    auto sum_width = [&](InstrumentName *outer_name, InstrumentName *inner_name) {
        AlignH staff_name_align = inner_name->position();
        if (staff_name_align == AlignH::LEFT || staff_name_align == AlignH::JUSTIFY) {
            return ldata->first_column_width() + name_width_including_group_brackets(outer_name, system) + ldata->instrument_name_offset();
        } else if (staff_name_align == AlignH::HCENTER) {
            double sum_width =
                inner_name->width() + name_width_including_group_brackets(outer_name, system) + ldata->instrument_name_offset();
            double move = 0.5 * (ldata->first_column_width() - inner_name->width());
            return sum_width + move;
        } else {
            return inner_name->width() + ldata->instrument_name_offset() + name_width_including_group_brackets(outer_name, system);
        }
    };

    for (InstrumentName *shared_staff_name : shared_stave_names) {
        if (shared_staff_name->ldata()->column() == 0) {
            continue;
        }
        Part *part = ctx.dom().staff(shared_staff_name->staff_idx())->part();
        for (staff_idx_t idx : part->stave_idx_list()) {
            if (InstrumentName *n = system->staff(idx)->name(InstrumentNameRole::STAFF); n && !n->ldata()->is_skip_draw()) {
                ldata->set_total_names_width(std::max(ldata->total_names_width(), sum_width(shared_staff_name, n)));
            }
        }
    }

    if (stack_labels_vertically(system)) {
        return;
    }

    for (InstrumentName *group_name : group_names) {
        staff_idx_t start_staff = group_name->staff_idx();
        staff_idx_t end_staff = group_name->ldata()->end_idx_of_group();
        if (group_name->instrument_name_type() == InstrumentNameType::LONG &&
            system->visible_parts_of_group(start_staff, end_staff).size() % 2 == 0) {
            continue;
        }
        for (staff_idx_t idx = start_staff; idx < end_staff; ++idx) {
            if (InstrumentName *n = system->staff(idx)->name(InstrumentNameRole::STAFF); n && !n->ldata()->is_skip_draw()) {
                ldata->set_total_names_width(std::max(ldata->total_names_width(), sum_width(group_name, n)));
            }
            if (InstrumentName *n = system->staff(idx)->name(InstrumentNameRole::PART); n && !n->ldata()->is_skip_draw()) {
                ldata->set_total_names_width(std::max(ldata->total_names_width(), sum_width(group_name, n)));
            }
        }
    }

    for (InstrumentName *instr_name : instrument_names) {
        if (instr_name->ldata()->column() == 0) {
            continue;
        }
        Part *part = ctx.dom().staff(instr_name->staff_idx())->part();
        if (instr_name->instrument_name_type() == InstrumentNameType::LONG && system->visible_staves_of_part(part).size() % 2 == 0) {
            continue;
        }
        for (staff_idx_t idx : part->stave_idx_list()) {
            if (InstrumentName *n = system->staff(idx)->name(InstrumentNameRole::STAFF); n && !n->ldata()->is_skip_draw()) {
                ldata->set_total_names_width(std::max(ldata->total_names_width(), sum_width(instr_name, n)));
            }
        }
    }
}

void SystemHeaderLayout::set_instrument_names_vertical_pos(System *system, LayoutContext &ctx) {
    std::set<const Part *> parts_with_individual_staff_names;

    for (staff_idx_t staff_idx = 0; staff_idx < ctx.dom().nstaves(); ++staff_idx) {
        SysStaff *s = system->staff(staff_idx);
        for (InstrumentNameRole role : {InstrumentNameRole::STAFF, InstrumentNameRole::SHARED_STAFF}) {
            if (InstrumentName *individual_name = s->name(role)) {
                const RectF &staff_b_box = s->bbox();
                const RectF &name_b_box = individual_name->ldata()->bbox();
                individual_name->mutldata()->set_pos_y(0.5 *
                                                       (staff_b_box.top() + staff_b_box.bottom() - name_b_box.bottom() - name_b_box.top()));
                parts_with_individual_staff_names.insert(ctx.dom().staff(staff_idx)->part());
            }
        }
    }

    bool stack_vertically = stack_labels_vertically(system);

    staff_idx_t staff_idx = 0;
    for (const Part *p : ctx.dom().parts()) {
        size_t nstaves = p->nstaves();

        SysStaff *s = system->staff(staff_idx);

        InstrumentName *t = s->name(InstrumentNameRole::PART);
        if (!t || t->effective_staff_idx() == muse::nidx) {
            staff_idx += nstaves;
            continue;
        }

        const RectF &bbox = t->ldata()->bbox();
        double y_center = 0.5 * (bbox.bottom() + bbox.top());

        std::vector<staff_idx_t> visible_staves_of_part = system->visible_staves_of_part(p);
        size_t visible_staves_count = visible_staves_of_part.size();
        DO_ASSERT(visible_staves_count > 0);

        double y1 = 0;
        double y2 = 0;

        if (!muse::contains(parts_with_individual_staff_names, p)) {
            SysStaff *top_st = system->staff(visible_staves_of_part.front());
            SysStaff *bottom_st = system->staff(visible_staves_of_part.back());
            y1 = top_st->bbox().top();
            y2 = bottom_st->bbox().bottom();
            t->mutldata()->set_pos_y(0.5 * (y1 + y2) - y_center);

            staff_idx += nstaves;
            continue;
        }

        if (visible_staves_count % 2 == 0) {
            SysStaff *staff_above_mid = system->staff(visible_staves_of_part[visible_staves_count / 2 - 1]);
            SysStaff *staff_below_mid = system->staff(visible_staves_of_part[visible_staves_count / 2]);
            y1 = staff_above_mid->bbox().top();
            y2 = staff_below_mid->bbox().bottom();
            t->mutldata()->set_pos_y(0.5 * (y1 + y2) - y_center);

            staff_idx += nstaves;
            continue;
        }

        SysStaff *mid_staff = system->staff(visible_staves_of_part[visible_staves_count / 2]);
        y1 = mid_staff->bbox().top();
        y2 = mid_staff->bbox().bottom();
        if (InstrumentName *staff_name = mid_staff->name(InstrumentNameRole::STAFF)) {
            if (stack_vertically || t->ldata()->column() == 0) {
                double line_spacing = t->line_spacing();
                double instr_name_bottom = t->ldata()->blocks.back().y();
                double center_y = 0.5 * (mid_staff->bbox().top() + mid_staff->bbox().bottom());
                t->mutldata()->set_pos_y(center_y - instr_name_bottom - 0.25 * line_spacing);
                double staff_name_top = staff_name->ldata()->blocks.front().y();
                staff_name->mutldata()->set_pos_y(center_y - staff_name_top + 0.75 * line_spacing);
            } else if (staff_name->ldata()->rows() == 1 && t->ldata()->rows() == 1) {
                t->mutldata()->set_pos_y(staff_name->y());
            } else {
                t->mutldata()->set_pos_y(0.5 * (y1 + y2) - y_center);
            }
        }

        staff_idx += nstaves;
    }

    for (staff_idx_t idx = 0; idx < system->staves().size(); ++idx) {
        InstrumentName *group_name = system->staff(idx)->name(InstrumentNameRole::GROUP);
        if (!group_name || group_name->effective_staff_idx() == muse::nidx) {
            continue;
        }

        const RectF &bbox = group_name->ldata()->bbox();
        double y_center = 0.5 * (bbox.bottom() + bbox.top());

        std::vector<Part *> visible_parts = system->visible_parts_of_group(idx, group_name->ldata()->end_idx_of_group());
        size_t visible_parts_count = visible_parts.size();
        DO_ASSERT(visible_parts_count > 0);

        double y1 = 0;
        double y2 = 0;

        if (visible_parts_count % 2 == 0) {
            Part *part_above_mid = visible_parts[visible_parts_count / 2 - 1];
            staff_idx_t staff_idx_above_mid = system->last_visible_sys_staff_of_part(part_above_mid);
            DO_ASSERT(staff_idx_above_mid != muse::nidx);
            SysStaff *staff_above_mid = system->staff(staff_idx_above_mid);

            Part *part_below_mid = visible_parts[visible_parts_count / 2];
            staff_idx_t staff_idx_below_mid = system->first_visible_sys_staff_of_part(part_below_mid);
            DO_ASSERT(staff_idx_below_mid != muse::nidx);
            SysStaff *staff_below_mid = system->staff(staff_idx_below_mid);

            y1 = staff_above_mid->bbox().top();
            y2 = staff_below_mid->bbox().bottom();
            group_name->mutldata()->set_pos_y(0.5 * (y1 + y2) - y_center);

            continue;
        }

        Part *mid_part = visible_parts[visible_parts_count / 2];
        InstrumentName *instr_name = system->staff(*mid_part->stave_idx_list().begin())->name(InstrumentNameRole::PART);
        std::vector<staff_idx_t> visible_staves = system->visible_staves_of_part(mid_part);
        size_t visible_staves_count = visible_staves.size();

        if (visible_staves_count % 2) {
            SysStaff *mid_staff = system->staff(visible_staves[visible_staves_count / 2]);
            y1 = mid_staff->bbox().top();
            y2 = mid_staff->bbox().bottom();
        } else {
            SysStaff *staff_above_mid = system->staff(visible_staves[visible_staves_count / 2 - 1]);
            SysStaff *staff_below_mid = system->staff(visible_staves[visible_staves_count / 2]);
            y1 = staff_above_mid->bbox().top();
            y2 = staff_below_mid->bbox().bottom();
        }

        if (instr_name) {
            if (stack_vertically) {
                double line_spacing = group_name->line_spacing();
                double group_name_bottom = group_name->ldata()->blocks.back().y();
                if (visible_staves_count % 2 && system->staff(visible_staves[visible_staves_count / 2])->name(InstrumentNameRole::STAFF)) {
                    group_name->mutldata()->set_pos_y(instr_name->y() - group_name_bottom - line_spacing);
                } else {
                    double center_y = 0.5 * (y1 + y2);
                    group_name->mutldata()->set_pos_y(center_y - group_name_bottom - 0.25 * line_spacing);
                    double instr_name_top = instr_name->ldata()->blocks.front().y();
                    instr_name->mutldata()->set_pos_y(center_y - instr_name_top + 0.75 * line_spacing);
                }
            } else if (instr_name->ldata()->rows() == 1 && group_name->ldata()->rows() == 1) {
                group_name->mutldata()->set_pos_y(instr_name->y());
            } else {
                group_name->mutldata()->set_pos_y(0.5 * (y1 + y2) - y_center);
            }

            continue;
        }

        if (visible_staves_count % 2 == 0) {
            group_name->mutldata()->set_pos_y(0.5 * (y1 + y2) - y_center);
            continue;
        }

        SysStaff *mid_staff = system->staff(visible_staves[visible_staves_count / 2]);
        InstrumentName *staff_name = mid_staff->name(InstrumentNameRole::STAFF);
        if (!staff_name) {
            group_name->mutldata()->set_pos_y(0.5 * (y1 + y2) - y_center);
            continue;
        }

        if (stack_vertically) {
            double line_spacing = group_name->line_spacing();
            double group_name_bottom = group_name->ldata()->blocks.back().y();
            double center_y = 0.5 * (y1 + y2);
            group_name->mutldata()->set_pos_y(center_y - group_name_bottom - 0.25 * line_spacing);
            double staff_name_top = staff_name->ldata()->blocks.front().y();
            staff_name->mutldata()->set_pos_y(center_y - staff_name_top + 0.75 * line_spacing);
        } else if (staff_name->ldata()->rows() == 1 && group_name->ldata()->rows() == 1) {
            group_name->mutldata()->set_pos_y(staff_name->y());
        } else {
            group_name->mutldata()->set_pos_y(0.5 * (y1 + y2) - y_center);
        }
    }
}

void SystemHeaderLayout::set_instrument_names_horizontal_pos(System *system) {
    //---------------------------------------------------
    //  layout instrument names x position
    //     at this point it is not clear which staves will
    //     be hidden, so layout all instrument names
    //---------------------------------------------------

    System::LayoutData *ldata = system->mutldata();
    double total_names_width = ldata->total_names_width();
    double first_column_width = ldata->first_column_width();
    // Ignore long/short overrides
    InstrumentNamesAlign align =
        system->style()
            .style_v(ldata->use_long_names() ? Sid::INSTRUMENT_NAMES_ALIGN_LONG : Sid::INSTRUMENT_NAMES_ALIGN_SHORT)
            .value<InstrumentNamesAlign>();

    auto place_first_column_name = [&](InstrumentName *name) {
        const RectF &bbox = name->ldata()->bbox();
        if (align == InstrumentNamesAlign::CENTER_CENTER) {
            double group_brackets_width = ldata->group_brackets_width_at_staff_idx(name->effective_staff_idx());
            name->mutldata()->set_pos_x(0.5 * total_names_width - 0.5 * (bbox.right() + bbox.left()) + group_brackets_width);
        } else {
            switch (name->position()) {
                case AlignH::JUSTIFY:
                case AlignH::LEFT: name->mutldata()->set_pos_x(total_names_width - first_column_width - bbox.left()); break;
                case AlignH::HCENTER:
                    name->mutldata()->set_pos_x(total_names_width - 0.5 * first_column_width - 0.5 * (bbox.right() + bbox.left()));
                    break;
                case AlignH::RIGHT: name->mutldata()->set_pos_x(total_names_width - bbox.right());
            }
        }
    };

    for (const SysStaff *s : system->staves()) {
        for (InstrumentNameRole role : {InstrumentNameRole::STAFF, InstrumentNameRole::SHARED_STAFF, InstrumentNameRole::PART}) {
            InstrumentName *n = s->name(role);
            if (n && n->ldata()->column() == 0 && n->effective_staff_idx() != muse::nidx) {
                place_first_column_name(n);
            }
        }
    }

    bool stack_vertically = stack_labels_vertically(system);

    auto place_second_column_name = [&](InstrumentName *name, staff_idx_t staff_idx) {
        bool do_stack_vertically = stack_vertically && name->instrument_name_role() != InstrumentNameRole::SHARED_STAFF;
        const RectF &bbox = name->ldata()->bbox();
        double group_brackets_width = ldata->group_brackets_width_at_staff_idx(name->effective_staff_idx());

        if (align == InstrumentNamesAlign::LEFT_RIGHT) {
            name->mutldata()->set_pos_x(group_brackets_width - bbox.left());
            return;
        }

        if (align == InstrumentNamesAlign::CENTER_CENTER) {
            name->mutldata()->set_pos_x(0.5 * total_names_width - 0.5 * (bbox.right() + bbox.left()) + 0.5 * group_brackets_width);
            return;
        }

        if (align == InstrumentNamesAlign::CENTER_RIGHT) {
            name->mutldata()->set_pos_x(0.5 * ldata->second_column_width() - 0.5 * (bbox.right() + bbox.left()) +
                                        0.5 * group_brackets_width);
            return;
        }

        if (do_stack_vertically) {
            name->mutldata()->set_pos_x(total_names_width - bbox.right());
            return;
        }

        if (name->instrument_name_role() == InstrumentNameRole::SHARED_STAFF) {
            if (InstrumentName *staff_name = system->staff(name->staff_idx())->name(InstrumentNameRole::STAFF)) {
                name->mutldata()->set_pos_x(staff_name->x() + staff_name->ldata()->bbox().left() - bbox.right() -
                                            ldata->instrument_name_offset());
            } else {
                name->mutldata()->set_pos_x(total_names_width - bbox.right());
            }

            return;
        }

        if (name->instrument_name_role() == InstrumentNameRole::PART) {
            const Part *p = system->score()->staff(staff_idx)->part();
            std::vector<staff_idx_t> visible_staves_for_part = system->visible_staves_of_part(p);
            size_t visible_stave_count = visible_staves_for_part.size();
            if (visible_stave_count % 2 == 0) {
                name->mutldata()->set_pos_x(total_names_width - bbox.right());
                return;
            }

            staff_idx_t center_staff = visible_staves_for_part[visible_stave_count / 2];
            if (InstrumentName *staff_name = system->staff(center_staff)->name(InstrumentNameRole::STAFF)) {
                name->mutldata()->set_pos_x(staff_name->x() + staff_name->ldata()->bbox().left() - bbox.right() -
                                            ldata->instrument_name_offset());
            } else {
                name->mutldata()->set_pos_x(total_names_width - bbox.right());
            }

            return;
        }

        std::vector<Part *> visible_parts_of_group = system->visible_parts_of_group(staff_idx, name->ldata()->end_idx_of_group());
        size_t visible_parts_count = visible_parts_of_group.size();
        if (visible_parts_count % 2 == 0) {
            name->mutldata()->set_pos_x(total_names_width - bbox.right());
            return;
        }

        Part *center_part = visible_parts_of_group[visible_parts_count / 2];
        if (InstrumentName *instr_name = system->staff(*center_part->stave_idx_list().begin())->name(InstrumentNameRole::PART)) {
            name->mutldata()->set_pos_x(instr_name->x() + instr_name->ldata()->bbox().left() - bbox.right() -
                                        ldata->instrument_name_offset());
            return;
        }

        std::vector<staff_idx_t> visible_staves_of_part = system->visible_staves_of_part(center_part);
        size_t visible_staves_count = visible_staves_of_part.size();
        if (visible_staves_count % 2 == 0) {
            name->mutldata()->set_pos_x(total_names_width - bbox.right());
            return;
        }

        staff_idx_t center_staff = visible_staves_of_part[visible_staves_count / 2];
        if (InstrumentName *staff_name = system->staff(center_staff)->name(InstrumentNameRole::STAFF)) {
            name->mutldata()->set_pos_x(staff_name->x() + staff_name->ldata()->bbox().left() - bbox.right() -
                                        ldata->instrument_name_offset());
        } else {
            name->mutldata()->set_pos_x(total_names_width - bbox.right());
        }
    };

    for (staff_idx_t staff_idx = 0; staff_idx < system->staves().size(); ++staff_idx) {
        const SysStaff *s = system->staff(staff_idx);
        InstrumentName *instr_name = s->name(InstrumentNameRole::PART);
        if (!instr_name) {
            instr_name = s->name(InstrumentNameRole::SHARED_STAFF);
        }
        if (!instr_name) {
            continue;
        }
        if (instr_name && instr_name->effective_staff_idx() != muse::nidx && instr_name->ldata()->column() > 0) {
            place_second_column_name(instr_name, staff_idx);
        }
    }

    for (staff_idx_t staff_idx = 0; staff_idx < system->staves().size(); ++staff_idx) {
        const SysStaff *s = system->staff(staff_idx);
        InstrumentName *group_name = s->name(InstrumentNameRole::GROUP);
        if (group_name && group_name->effective_staff_idx() != muse::nidx) {
            place_second_column_name(group_name, staff_idx);
        }
    }
}

void SystemHeaderLayout::set_group_brackets_horizontal_pos(System *system) {
    std::vector<Bracket *> group_brackets;
    for (Bracket *b : system->brackets()) {
        if (b->bracket_type() == BracketType::GROUP) {
            group_brackets.push_back(b);
        }
    }
    std::sort(group_brackets.begin(), group_brackets.end(), [](Bracket *b1, Bracket *b2) { return b1->column() > b2->column(); });

    const MStyle &style = system->style();
    bool into_margin = style.style_b(Sid::GROUP_BRACKET_HANG_TEXT_INTO_MARGIN) &&
                       style.style_v(Sid::GROUP_BRACKET_TEXT_ORIENTATION).value<Orientation>() == Orientation::VERTICAL;
    double brackets_dist = style.style_absolute(Sid::GROUP_BRACKET_DISTANCE_TO_GROUP_BRACKET);

    for (Bracket *b : group_brackets) {
        b->mutldata()->set_pos_x(into_margin ? 0.0 : -b->mutldata()->bbox().left());
        for (Bracket *bb : group_brackets) {
            if (bb == b) {
                break;
            }
            if (bb->column() > b->column() && bb->intersects(b)) {
                b->mutldata()->set_pos_x(bb->x() + bb->ldata()->bbox().right() - b->ldata()->bbox().left() + brackets_dist);
            }
        }
    }
}

InstrumentName *SystemHeaderLayout::update_name(System *system, staff_idx_t staff_idx, LayoutContext &ctx, const String &name,
                                                InstrumentNameType type, InstrumentNameRole role) {
    SysStaff *sys_staff = system->staff(staff_idx);
    InstrumentName *iname = sys_staff->name(role);

    if (name.empty()) {
        if (iname) {
            ctx.mut_dom().remove_element(iname);
        }
        return nullptr;
    }

    if (!iname) {
        iname = new InstrumentName(system);
        iname->set_generated(true);
        iname->set_sys_staff(sys_staff);
        iname->set_track(staff_idx * ::mu::engraving::voices);
        iname->set_instrument_name_role(role);
        ctx.mut_dom().add_element(iname);
    }

    iname->set_instrument_name_type(type);
    iname->set_align(Align(iname->align().horizontal, AlignV::BASELINE));
    iname->set_xml_text(name);

    iname->mutldata()->set_column(0); // Reset here, will be computed later
    TLayout::layout_instrument_name(iname, iname->mutldata());

    return iname;
}

String SystemHeaderLayout::formatted_instrument_name(System *system, Part *part, const Fraction &tick, bool long_names) {
    Instrument *instr = part->instrument(tick);
    const InstrumentLabel &label = instr->instrument_label();

    const MStyle &style = system->style();
    bool trailing_dot = style.style_b(Sid::INSTRUMENT_NUMERALS_TRAILING_DOT_SINGLE);

    if (muse::contains(system->ldata()->parts_with_group_name(), part)) {
        if (label.use_custom_individual_name()) {
            return long_names ? label.custom_name_long_individual() : label.custom_name_short_individual();
        }

        int number = part->number(tick);
        String result = number > 0 ? String::number(number) : String();
        if (!result.is_empty() && trailing_dot) {
            result += '.';
        }

        return result;
    }

    if (label.use_custom_name()) {
        return long_names ? label.custom_name_long() : label.custom_name_short();
    }

    String instr_name = long_names ? instr->long_name() : instr->short_name();

    bool show_transpo = long_names ? label.show_transposition_long() : label.show_transposition_short();
    show_transpo &=
        style.style_b(long_names ? Sid::INSTRUMENT_NAMES_SHOW_TRANSPOSITION_LONG : Sid::INSTRUMENT_NAMES_SHOW_TRANSPOSITION_SHORT);
    String transposition = show_transpo ? instr->transposition() : String();

    bool show_number = long_names ? label.show_number_long() : label.show_number_short();
    String number = instr->number() > 0 && show_number ? String::number(instr->number()) : String();

    InstrumentNamesFormat name_format =
        style.style_v(long_names ? Sid::INSTRUMENT_NAMES_FORMAT_LONG : Sid::INSTRUMENT_NAMES_FORMAT_SHORT).value<InstrumentNamesFormat>();

    bool add_trailing_dot = trailing_dot && (transposition.empty() || name_format == InstrumentNamesFormat::NAME_IN_TRANSP_NUM ||
                                             name_format == InstrumentNamesFormat::TRANSP_NAME_NUM);
    if (add_trailing_dot) {
        number += '.';
    }

    if (transposition.empty()) {
        String result = instr_name;
        if (!number.empty()) {
            result += u" " + number;
        }
        return result;
    }

    //: For instrument transposition, e.g. Horn in F
    String in = TranslatableString("notation", "in").translated();

    switch (name_format) {
        case InstrumentNamesFormat::NAME_IN_TRANSP_NUM:
            return instr_name + u" " + in + u" " + transposition + (number.empty() ? String() : u" " + number);
        case InstrumentNamesFormat::NAME_NUM_IN_TRANSP:
            return instr_name + (number.empty() ? String() : u" " + number) + u" " + in + u" " + transposition;
        case InstrumentNamesFormat::TRANSP_NAME_NUM: return transposition + u" " + instr_name + (number.empty() ? String() : u" " + number);
        default: {
            String result =
                style.style_st(long_names ? Sid::INSTRUMENT_NAMES_CUSTOM_FORMAT_LONG : Sid::INSTRUMENT_NAMES_CUSTOM_FORMAT_SHORT);
            return resolve_tokens(result, instr_name, transposition, number);
        }
    }
}

String SystemHeaderLayout::formatted_group_name(System *system, Part *part, const Fraction &tick, bool long_names) {
    const MStyle &style = system->style();

    Instrument *instr = part->instrument(tick);
    const InstrumentLabel &label = instr->instrument_label();

    if (label.use_custom_group_name()) {
        return long_names ? label.custom_name_long_group() : label.custom_name_short_group();
    }

    String instr_name = long_names ? instr->long_name() : instr->short_name();

    bool show_transpo = long_names ? label.show_transposition_long() : label.show_transposition_short();
    show_transpo &=
        style.style_b(long_names ? Sid::INSTRUMENT_NAMES_SHOW_TRANSPOSITION_LONG : Sid::INSTRUMENT_NAMES_SHOW_TRANSPOSITION_SHORT);

    String transposition = show_transpo ? instr->transposition() : String();

    if (transposition.empty()) {
        return instr_name;
    }

    InstrumentNamesFormat name_format =
        style.style_v(long_names ? Sid::INSTRUMENT_NAMES_FORMAT_LONG : Sid::INSTRUMENT_NAMES_FORMAT_SHORT).value<InstrumentNamesFormat>();

    //: For instrument transposition, e.g. Horn in F
    String in = TranslatableString("notation", "in").translated();

    switch (name_format) {
        case InstrumentNamesFormat::NAME_IN_TRANSP_NUM: return instr_name + u" " + in + u" " + transposition;
        case InstrumentNamesFormat::NAME_NUM_IN_TRANSP: return instr_name + u" " + in + u" " + transposition;
        case InstrumentNamesFormat::TRANSP_NAME_NUM: return transposition + u" " + instr_name;
        default: {
            String result =
                style.style_st(long_names ? Sid::INSTRUMENT_NAMES_CUSTOM_FORMAT_LONG : Sid::INSTRUMENT_NAMES_CUSTOM_FORMAT_SHORT);
            return resolve_tokens(result, instr_name, transposition, /*number*/ String());
        }
    }
}

String SystemHeaderLayout::formatted_shared_staff_label(staff_idx_t staff_idx, const SharedTrackMap &track_map,
                                                        const std::vector<Part *> &origin_parts) {
    if (origin_parts.empty()) {
        return String();
    }

    Score *score = origin_parts.front()->score();
    const MStyle &style = score->style();
    bool trailing_dot_single = style.style_b(Sid::INSTRUMENT_NUMERALS_TRAILING_DOT_SINGLE);
    bool trailing_dot_multiple = style.style_b(Sid::INSTRUMENT_NUMERALS_TRAILING_DOT_MULTIPLE);
    SharedLabelOrientation orientation = style.style_v(Sid::INSTRUMENT_NUMERALS_ORIENTATION).value<SharedLabelOrientation>();
    int horizontal_limit = style.style_i(Sid::INSTRUMENT_NUMERALS_HORIZONTAL_THRESHOLD);
    int vertical_limit = style.style_i(Sid::INSTRUMENT_NUMERALS_VERTICAL_THRESHOLD);
    int hyphen_limit =
        style.style_b(Sid::INSTRUMENT_NUMERALS_HYPHEN_ENABLE) ? style.style_i(Sid::INSTRUMENT_NUMERALS_HYPHEN_THRESHOLD) : INT_MAX;

    std::vector<const Instrument *> instruments_mapped_to_first_voice;
    std::vector<const Instrument *> instruments_mapped_to_second_voice;
    for (auto [origin_track, shared_track] : track_map) {
        if (track2staff(shared_track) == staff_idx) {
            if (track2voice(shared_track) == 0) {
                instruments_mapped_to_first_voice.push_back(score->staff(track2staff(origin_track))->part()->instrument());
            } else {
                instruments_mapped_to_second_voice.push_back(score->staff(track2staff(origin_track))->part()->instrument());
            }
        }
    }

    size_t tot_instrument_count = instruments_mapped_to_first_voice.size() + instruments_mapped_to_second_voice.size();

    SharedLabelOrientation actual_orientation;
    switch (orientation) {
        case SharedLabelOrientation::VOICE: actual_orientation = orientation; break;
        case SharedLabelOrientation::HORIZONTAL:
            actual_orientation = tot_instrument_count > size_t(horizontal_limit) ? SharedLabelOrientation::VOICE : orientation;
            break;
        case SharedLabelOrientation::VERTICAL:
            actual_orientation = tot_instrument_count > size_t(vertical_limit) ? SharedLabelOrientation::VOICE : orientation;
            break;
        default: // should not happen, but don't leave actualOrientation uninitialized
            ASSERT_X("Unexpected SharedLabelOrientation value: " << static_cast<int>(orientation));
            actual_orientation = SharedLabelOrientation::VOICE;
            break;
    }

    if (actual_orientation == SharedLabelOrientation::HORIZONTAL || actual_orientation == SharedLabelOrientation::VERTICAL) {
        // Put all instruments in single vector
        instruments_mapped_to_first_voice.insert(instruments_mapped_to_first_voice.end(), instruments_mapped_to_second_voice.begin(),
                                                 instruments_mapped_to_second_voice.end());
        std::sort(instruments_mapped_to_first_voice.begin(), instruments_mapped_to_first_voice.end(),
                  [](const Instrument *a, const Instrument *b) { return a->number() < b->number(); });

        String result;

        if (actual_orientation == SharedLabelOrientation::HORIZONTAL) {
            result = format_shared_voice_label(instruments_mapped_to_first_voice, trailing_dot_single, trailing_dot_multiple, hyphen_limit);
        } else {
            result = format_vertical_shared_label(instruments_mapped_to_first_voice, trailing_dot_single);
        }

        return result;
    }

    String result = format_shared_voice_label(instruments_mapped_to_first_voice, trailing_dot_single, trailing_dot_multiple, hyphen_limit);

    if (instruments_mapped_to_second_voice.empty()) {
        return result;
    }

    result +=
        '\n' + format_shared_voice_label(instruments_mapped_to_second_voice, trailing_dot_single, trailing_dot_multiple, hyphen_limit);

    return result;
}

String SystemHeaderLayout::format_vertical_shared_label(const std::vector<const Instrument *> &instruments, bool trailing_dot_single) {
    String result;

    for (const Instrument *instr : instruments) {
        if (!result.empty()) {
            result += '\n';
        }

        result += String::number(instr->number());

        if (trailing_dot_single) {
            result += '.';
        }
    }

    return result;
}

String SystemHeaderLayout::format_shared_voice_label(const std::vector<const Instrument *> &instruments, bool trailing_dot_single,
                                                     bool trailing_dot_multiple, int hyphen_limit) {
    String result;

    size_t voice_count = instruments.size();
    bool put_trailing_dot = (voice_count <= 1 && trailing_dot_single) || (voice_count > 1 && trailing_dot_multiple);

    for (size_t i = 0; i < instruments.size(); ++i) {
        if (!result.empty() && result.back() != '\n') {
            result += '.';
        }

        const Instrument *instr = instruments[i];
        int start_number = instr->number();
        int cur_number = start_number;
        for (size_t j = i + 1; j < instruments.size(); ++j) {
            const Instrument *next_instr = instruments[j];
            int next_number = next_instr->number();
            if (next_number == cur_number + 1) {
                cur_number = next_number;
                if (next_number - start_number >= hyphen_limit) {
                    i = j;
                } else {
                    continue;
                }
            } else {
                break;
            }
        }

        if (cur_number - start_number >= hyphen_limit) {
            result += String::number(start_number);

            if (put_trailing_dot) {
                result += '.';
            }

            result += u'–' + String::number(cur_number);
        } else {
            result += String::number(start_number);
        }
    }

    if (!result.is_empty() && put_trailing_dot) {
        result += u".";
    }

    return result;
}

String &SystemHeaderLayout::resolve_tokens(String &str, const String &name, const String &transposition, const String &number) {
    static const String name_value = u"$name";
    static const String transp = u"$transposition";
    static const String number_value = u"$number";

    str.replace(name_value, name);
    str.replace(transp, transposition);

    if (!number.empty()) {
        str.replace(number_value, number);
    } else if (str.contains(u" $number")) {
        str.remove(u" $number");
    } else if (str.contains(u"$number ")) {
        str.remove(u"$number ");
    } else if (str.contains(u"$number")) {
        str.remove(u"$number");
    }

    return str;
}

InstrumentLabelVisibility SystemHeaderLayout::resolve_instrument_label_visibility(const Staff *staff, const Fraction &tick,
                                                                                  LayoutContext &ctx, bool first_system) {
    const InstrumentLabelVisibility local = staff->staff_type(tick)->instrument_label_visibility();
    if (local != InstrumentLabelVisibility::AUTO) {
        return local;
    }

    if (ctx.conf().style_b(Sid::HIDE_INSTRUMENT_NAME_IF_ONE_INSTRUMENT) && ctx.dom().visible_part_count() <= 1) {
        std::vector<Part *> visible_parts = ctx.dom().visible_parts();
        if (visible_parts.size() == 0) {
            return InstrumentLabelVisibility::HIDE;
        }
        if (visible_parts.size() == 1 && !visible_parts.front()->is_shared_part()) {
            return InstrumentLabelVisibility::HIDE;
        }
    }

    const Sid sid = first_system ? Sid::FIRST_SYSTEM_INST_NAME_VISIBILITY : Sid::SUBS_SYSTEM_INST_NAME_VISIBILITY;
    const InstrumentLabelVisibility style_visibility = ctx.conf().style_v(sid).value<InstrumentLabelVisibility>();
    if (first_system && style_visibility == InstrumentLabelVisibility::LONG && !ctx.state().start_with_long_names()) {
        return InstrumentLabelVisibility::SHORT;
    }
    return style_visibility;
}

InstrumentNameType SystemHeaderLayout::resolve_instrument_name_type(InstrumentLabelVisibility visibility) {
    return visibility == InstrumentLabelVisibility::LONG ? InstrumentNameType::LONG : InstrumentNameType::SHORT;
}

void SystemHeaderLayout::set_instrument_names(System *system, LayoutContext &ctx) {
    if (system->vbox()) {
        return;
    }

    const Measure *first_measure = system->first_measure();
    if (!first_measure) {
        return;
    }

    Fraction tick = first_measure->tick();

    bool subs_sys_long_name =
        ctx.conf().style_v(Sid::SUBS_SYSTEM_INST_NAME_VISIBILITY).value<InstrumentLabelVisibility>() == InstrumentLabelVisibility::LONG;
    bool long_name = ctx.mut_state().first_system() ? ctx.mut_state().start_with_long_names() : subs_sys_long_name;

    System::LayoutData *ldata = system->mutldata();
    ldata->set_use_long_names(long_name);

    InstrumentNameType type = long_name ? InstrumentNameType::LONG : InstrumentNameType::SHORT;

    // We do not show instrument names for video export
    if (!ctx.conf().is_show_instrument_names()) {
        for (staff_idx_t idx = 0; idx < system->staves().size(); ++idx) {
            update_name(system, idx, ctx, String(), type, InstrumentNameRole::STAFF);
            update_name(system, idx, ctx, String(), type, InstrumentNameRole::SHARED_STAFF);
            update_name(system, idx, ctx, String(), type, InstrumentNameRole::PART);
            update_name(system, idx, ctx, String(), type, InstrumentNameRole::GROUP);
        }
        return;
    }

    update_group_names(system, ctx, tick);

    bool first_system = ctx.state().first_system();

    for (size_t staff_idx = 0; staff_idx < system->staves().size(); /*empty*/) {
        Part *part = ctx.dom().staff(staff_idx)->part();
        size_t part_nstaves = part->nstaves();
        size_t visible_staves_count = part->visible_staves_count();

        if (part->is_shared_part() && part->show()) {
            set_shared_part_names(to_shared_part(part), staff_idx, system, ctx, tick);
            staff_idx += part_nstaves;
            continue;
        }

        for (size_t idx_in_part = 0; idx_in_part < part_nstaves; ++idx_in_part) {
            staff_idx_t global_idx = staff_idx + idx_in_part;
            const Staff *staff = ctx.dom().staff(global_idx);

            InstrumentLabelVisibility visibility = resolve_instrument_label_visibility(staff, tick, ctx, first_system);
            InstrumentNameType staff_name_type = resolve_instrument_name_type(visibility);
            bool hide = visibility == InstrumentLabelVisibility::HIDE;
            bool staff_long_name = staff_name_type == InstrumentNameType::LONG;

            String instrument_name;
            if (idx_in_part == 0 && part->show() && visible_staves_count > 0 && !hide) {
                instrument_name = formatted_instrument_name(system, part, tick, staff_long_name);
            }
            update_name(system, global_idx, ctx, instrument_name, staff_name_type, InstrumentNameRole::PART);

            String staff_name;
            if (staff->show() && !hide) {
                staff_name = staff_long_name ? staff->individual_staff_name_long(tick) : staff->individual_staff_name_short(tick);
            }
            update_name(system, global_idx, ctx, staff_name, staff_name_type, InstrumentNameRole::STAFF);
        }

        staff_idx += part_nstaves;
    }
}

void SystemHeaderLayout::set_shared_part_names(SharedPart *shared_part, staff_idx_t start_staff_idx, System *system, LayoutContext &ctx,
                                               const Fraction &tick) {
    InstrumentLabelVisibility visibility =
        resolve_instrument_label_visibility(ctx.dom().staff(start_staff_idx), tick, ctx, ctx.state().first_system());
    InstrumentNameType type = resolve_instrument_name_type(visibility);
    bool hide_start_staff = visibility == InstrumentLabelVisibility::HIDE;

    if (!shared_part->show() || shared_part->visible_staves_count() == 0) {
        for (size_t rel_staff_idx = 0; rel_staff_idx < shared_part->nstaves(); ++rel_staff_idx) {
            size_t global_staff_idx = start_staff_idx + rel_staff_idx;
            update_name(system, global_staff_idx, ctx, String(), type, InstrumentNameRole::GROUP);
            update_name(system, global_staff_idx, ctx, String(), type, InstrumentNameRole::PART);
            update_name(system, global_staff_idx, ctx, String(), type, InstrumentNameRole::SHARED_STAFF);
            update_name(system, global_staff_idx, ctx, String(), type, InstrumentNameRole::STAFF);
        }

        return;
    }

    if (!shared_part->is_same_instruments()) {
        // TODO
        return;
    }

    const Instrument *instr = shared_part->instrument();
    bool use_group = use_group_names(instr->group(), ctx) && shared_part->is_same_instruments_at_tick(tick);

    const SharedTrackMap &track_map = shared_part->track_map_at_tick(tick);
    const std::vector<Part *> origin_parts = shared_part->origin_parts();

    // If using group names, assign normal PART name (one name covers all shared staves). Obtains the style
    //
    //      1.2
    // Horn
    //      3.4
    //
    // If not using group names, assign SHARED_STAFF name to each stave. Obtains the style
    //
    // Horn 1.2
    //
    // Horn 3.4

    for (size_t rel_staff_idx = 0; rel_staff_idx < shared_part->nstaves(); ++rel_staff_idx) {
        size_t global_staff_idx = start_staff_idx + rel_staff_idx;
        Part *origin_part = shared_part->origin_part_for_staff(global_staff_idx, tick);

        InstrumentLabelVisibility part_visibility =
            resolve_instrument_label_visibility(ctx.dom().staff(global_staff_idx), tick, ctx, ctx.state().first_system());
        InstrumentNameType part_name_type = resolve_instrument_name_type(part_visibility);
        bool part_long_name = part_name_type == InstrumentNameType::LONG;
        bool hide_part_name = part_visibility == InstrumentLabelVisibility::HIDE;

        String staff_group_name;
        if (!hide_part_name) {
            staff_group_name = formatted_group_name(system, origin_part ? origin_part : shared_part, tick, part_long_name);
        }

        if (use_group && !hide_start_staff) {
            if (rel_staff_idx == 0) {
                update_name(system, global_staff_idx, ctx, staff_group_name, part_name_type, InstrumentNameRole::PART);
            }

            update_name(system, global_staff_idx, ctx, String(), part_name_type, InstrumentNameRole::SHARED_STAFF);
        } else {
            if (rel_staff_idx == 0) {
                update_name(system, global_staff_idx, ctx, String(), type, InstrumentNameRole::PART);
            }
            update_name(system, global_staff_idx, ctx, staff_group_name, part_name_type, InstrumentNameRole::SHARED_STAFF);
        }

        String staff_label;
        if (!hide_part_name) {
            staff_label = formatted_shared_staff_label(global_staff_idx, track_map, origin_parts);
        }

        update_name(system, global_staff_idx, ctx, staff_label, part_name_type, InstrumentNameRole::STAFF);
    }
}

void SystemHeaderLayout::update_group_names(System *system, LayoutContext &ctx, const Fraction &tick) {
    bool first_system = ctx.state().first_system();

    System::LayoutData *ldata = system->mutldata();
    ldata->clear_parts_with_group_names();

    for (staff_idx_t start_of_group = 0; start_of_group < system->staves().size();) {
        Part *cur_part = ctx.dom().staff(start_of_group)->part();
        const Instrument *cur_instrument = cur_part->instrument(tick);
        const InstrumentLabel &cur_label = cur_instrument->instrument_label();
        if (!cur_instrument->instrument_label().allow_group_name() || !cur_part->show() || cur_part->is_shared_part()) {
            ++start_of_group;
            continue;
        }

        InstrumentLabelVisibility visibility =
            resolve_instrument_label_visibility(ctx.dom().staff(start_of_group), tick, ctx, first_system);
        InstrumentNameType type = resolve_instrument_name_type(visibility);
        bool hide = visibility == InstrumentLabelVisibility::HIDE;

        std::vector<Part *> parts_in_this_group;

        staff_idx_t end_of_group = start_of_group;
        while (end_of_group < system->staves().size()) {
            Part *next_part = ctx.dom().staff(end_of_group)->part();
            Instrument *next_instrument = next_part->instrument(tick);
            InstrumentLabel &next_label = next_instrument->instrument_label();
            if (next_part != cur_part &&
                (next_instrument->id() != cur_instrument->id() || !next_label.allow_group_name() || next_part->is_shared_part())) {
                break;
            }
            if (next_label.use_custom_group_name() && (next_label.custom_name_long_group() != cur_label.custom_name_long_group() ||
                                                       next_label.custom_name_short_group() != cur_label.custom_name_short_group())) {
                break;
            }

            if (type == InstrumentNameType::LONG && system->visible_staves_of_part(next_part).size() == 0) {
                // NOTE: We can only do this for long-name systems because they don't need to have
                // the left barline aligned with the other systems across the page.
                end_of_group += next_part->nstaves();
                continue;
            }

            if (next_part->show() && next_part->visible_staves_count() > 0) {
                parts_in_this_group.push_back(next_part);
                next_label.set_use_custom_group_name(cur_label.use_custom_group_name());
                next_label.set_custom_name_long_group(cur_label.custom_name_long_group());
                next_label.set_custom_name_short_group(cur_label.custom_name_short_group());
            }

            end_of_group += next_part->nstaves();
        }

        if (!hide && parts_in_this_group.size() > 1 && use_group_names(cur_instrument->group(), ctx)) {
            String name = formatted_group_name(system, cur_part, tick, type == InstrumentNameType::LONG);
            InstrumentName *group_name = update_name(system, start_of_group, ctx, name, type, InstrumentNameRole::GROUP);

            if (group_name) {
                group_name->mutldata()->set_end_idx_of_group(end_of_group);

                for (Part *p : parts_in_this_group) {
                    ldata->add_part_with_group_names(p, group_name);
                }
            }

            for (staff_idx_t idx = start_of_group + 1; idx < end_of_group; ++idx) {
                update_name(system, idx, ctx, String(), type, InstrumentNameRole::GROUP);
            }
        } else {
            for (staff_idx_t idx = start_of_group; idx < end_of_group; ++idx) {
                update_name(system, idx, ctx, String(), type, InstrumentNameRole::GROUP);
            }
        }

        start_of_group = end_of_group;
    }

    for (staff_idx_t staff_idx = 0; staff_idx < system->staves().size(); ++staff_idx) {
        if (system->staff(staff_idx)->name(InstrumentNameRole::GROUP)) {
            Part *part = ctx.dom().staff(staff_idx)->part();
            if (!muse::contains(ldata->parts_with_group_name(), part)) {
                // name is empty here, so the InstrumentNameType is unused (see updateName)
                update_name(system, staff_idx, ctx, String(), InstrumentNameType::SHORT, InstrumentNameRole::GROUP);
            }
        }
    }
}

bool SystemHeaderLayout::use_group_names(const String &instrument_group, LayoutContext &ctx) {
    const MStyle &style = ctx.conf().style();

    if (instrument_group == "woodwinds" || instrument_group == "brass") {
        return style.style_b(Sid::WINDS_NAME_BY_GROUP);
    }
    if (instrument_group == "vocals") {
        return style.style_b(Sid::VOCALS_NAME_BY_GROUP);
    }
    if (instrument_group == "strings") {
        return style.style_b(Sid::STRINGS_NAME_BY_GROUP);
    }

    return style.style_b(Sid::OTHERS_NAME_BY_GROUP);
}

void SystemHeaderLayout::update_system_header_width(System *system, LayoutContext &ctx) {
    if (system->staves().empty()) { // ignore vbox
        return;
    }

    SystemHeaderLayout::compute_instrument_name_offset(system, ctx);
    double instrument_name_offset = system->ldata()->instrument_name_offset();

    //---------------------------------------------------
    //  find x position of staves
    //---------------------------------------------------
    SystemHeaderLayout::layout_brackets(system, ctx);
    double max_brackets_width = SystemHeaderLayout::total_bracket_offset(ctx);

    SystemHeaderLayout::set_instrument_names(system, ctx);
    SystemHeaderLayout::compute_instrument_names_width(system, ctx);
    double max_names_width = system->ldata()->total_names_width();
    double indent = max_names_width > 0 ? max_names_width + instrument_name_offset : 0.0;
    if (ctx.state().first_system() && ctx.state().first_system_indent()) {
        indent = std::max(indent, system->style_p(Sid::FIRST_SYSTEM_INDENTATION_VALUE) * system->mag() - max_brackets_width);
        max_names_width = indent - instrument_name_offset;
    }

    if (muse::real_is_null(indent)) {
        if (ctx.conf().style_b(Sid::ALIGN_SYSTEM_TO_MARGIN)) {
            system->set_left_margin(0.0);
        } else {
            system->set_left_margin(max_brackets_width);
        }
    } else {
        system->set_left_margin(indent + max_brackets_width);
    }
}
