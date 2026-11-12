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
#include "lyricslayout.h"

#include "dom/chordrest.h"
#include "dom/factory.h"
#include "dom/lyrics.h"
#include "dom/measure.h"
#include "dom/repeatlist.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "dom/stafftype.h"
#include "dom/system.h"
#include "log.h"
#include "normalization/normalize.h"
#include "style/styledef.h"
#include "textlayout.h"
#include "tlayout.h"

using namespace mu;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void LyricsLayout::layout(Lyrics *item, LayoutContext &ctx) {
    if (!item->ownership_parent()) { // palette & clone trick
        item->set_pos(PointF());
        TextLayout::layout_base_text_base1(item, ctx);
        return;
    }

    Lyrics::LayoutData *ldata = item->mutldata();

    //
    // parse leading verse number and/or punctuation, so we can factor it into layout separately
    //
    bool has_number = false; // _verseNumber;

    // find:
    // 1) string of numbers and non-word characters at start of syllable
    // 2) at least one other character (indicating start of actual lyric)
    // 3) string of non-word characters at end of syllable
    //QRegularExpression leadingPattern("(^[\\d\\W]+)([^\\d\\W]+)");

    const String text = item->plain_text();
    String leading;
    String trailing;

    if (ctx.conf().style_b(Sid::LYRICS_ALIGN_VERSE_NUMBER)) {
        size_t leading_idx = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            Char ch = text.at(i);
            if (ch.is_letter()) {
                leading_idx = i;
                break;
            }
        }

        if (leading_idx != 0) {
            leading = text.mid(0, leading_idx);
        }

        size_t trailing_idx = text.size() - 1;
        for (int i = static_cast<int>(text.size() - 1); i >= 0; --i) {
            Char ch = text.at(i);
            if (ch.is_letter()) {
                trailing_idx = i;
                break;
            }
        }

        if (trailing_idx != text.size() - 1) {
            trailing = text.mid(trailing_idx + 1);
        }

        if (!leading.is_empty() && leading.at(0).is_digit()) {
            has_number = true;
        }
    }

    create_or_remove_lyrics_line(item, ctx);

    if (item->is_melisma() || has_number) {
        // use the melisma style alignment setting
        if (item->is_styled(Pid::POSITION)) {
            if (ctx.conf().style_b(Sid::LYRICS_CENTER_DASHED_SYLLABLES) && !(item->separator() && item->separator()->is_end_melisma())) {
                item->set_position(AlignH::HCENTER);
            } else {
                item->set_position(ctx.conf().style_v(Sid::LYRICS_MELISMA_ALIGN).value<Align>().horizontal);
            }
        }
    } else {
        // use the text style alignment setting
        if (item->is_styled(Pid::POSITION)) {
            item->set_position(item->property_default(Pid::POSITION).value<AlignH>());
        }
    }

    // Negate ChordRest offset
    ChordRest *cr = item->chord_rest();
    double x = -cr->x();

    /* Preserve the vertical position: a lyric's Y is set in computeVerticalPositions(). Necessary
     * because layoutBaseTextBase1() ends with ldata->move(defaultPos), which accumulates a downward
     * shift of the lyric if the current function is called in a partially-relaid system without a
     * following computeVerticalPositions() call. */
    const double pos_y = ldata->pos().y();
    TextLayout::layout_base_text_base1(item, ctx);
    TextLayout::compute_text_high_res_shape(item, ldata);
    ldata->set_pos_y(pos_y);

    double center_adjust = 0.0;
    double left_adjust = 0.0;

    if (ctx.conf().style_b(Sid::LYRICS_ALIGN_VERSE_NUMBER)) {
        // Calculate leading and trailing parts widths. Lyrics
        // should have text layout to be able to do it correctly.
        DO_ASSERT(ldata->blocks.size() != 0);
        if (!leading.is_empty() || !trailing.is_empty()) {
            //                   LOGD("create leading, trailing <%s> -- <%s><%s>", muPrintable(text), muPrintable(leading), muPrintable(trailing));
            const TextBlock &tb = ldata->blocks.at(0);

            const double leading_width = tb.xpos(leading.size(), item) - tb.bounding_rect().x();
            const size_t trailing_pos = text.size() - trailing.size();
            const double trailing_width = tb.bounding_rect().right() - tb.xpos(trailing_pos, item);

            left_adjust = leading_width;
            center_adjust = leading_width - trailing_width;
        }
    }

    if (item->position() == AlignH::HCENTER) {
        //
        // center under notehead, not origin
        // however, lyrics that are melismas or have verse numbers will be forced to left alignment
        //
        // center under note head
        x += -center_adjust * 0.5;
    } else if (item->position() == AlignH::LEFT) {
        // even for left aligned syllables, ignore leading verse numbers and/or punctuation
        x -= left_adjust;
    }

    ldata->set_pos_x(x);

    if (item->ticks().is_not_zero()) {
        // set melisma end
        ChordRest *ecr = ctx.mut_dom().find_cr(item->end_tick(), item->track());
        if (ecr) {
            ecr->set_melisma_end(true);
        }
    }
}

void LyricsLayout::layout(LyricsLine *item) {
    if (item->is_dash()) { // dash(es)
        item->set_next_lyrics(
            search_next_lyrics(item->lyrics()->segment(), item->staff_idx(), item->lyrics()->verse(), item->lyrics()->placement()));
        item->set_track2(item->next_lyrics() ? item->next_lyrics()->track() : item->track());

        Fraction end_tick = item->tick();
        const Measure *lyrics_measure = item->lyrics()->segment()->measure();
        const Segment *end_cr_seg = lyrics_measure ? lyrics_measure->last(SegmentType::CHORD_REST) : nullptr;

        const ChordRest *end_cr = nullptr;
        if (end_cr_seg && !end_cr_seg->empty()) {
            for (EngravingItem *cr : end_cr_seg->elist()) {
                if (cr && cr->is_chord_rest()) {
                    end_cr = to_chord_rest(cr);
                    break;
                }
            }
        }

        if (item->next_lyrics()) {
            end_tick = item->next_lyrics()->tick();
        } else if (end_cr && end_cr->has_following_jump_item()) {
            end_tick = end_cr->tick();
        }

        item->set_tick2(end_tick);
    }
}

void LyricsLayout::layout(LyricsLineSegment *item, [[maybe_unused]] LayoutContext &ctx) {

    assert(item->is_partial_lyrics_line_segment() || item->lyrics());

    LyricsLineSegment::LayoutData *ldata = item->mutldata();
    ldata->clear_dashes();

    if (item->lyrics_line()->is_end_melisma()) {
        layout_melisma_line(item);
    } else {
        layout_dashes(item);
    }

    double half_line_width = item->absolute_from_spatium(item->line_width());
    RectF rect(PointF(), item->pos2());
    rect.adjust(0.0, -half_line_width, 0.0, half_line_width);

    ldata->set_shape(Shape(rect, item));
}

void LyricsLayout::layout_melisma_line(LyricsLineSegment *item) {
    const bool is_partial_lyrics_line = item->is_partial_lyrics_line_segment();
    LyricsLine *lyrics_line = item->lyrics_line();
    Lyrics *start_lyrics = lyrics_line->lyrics();

    System *system = item->system();
    if (!system) {
        return;
    }
    const MStyle &style = item->style();

    double start_x = lyrics_line_start_x(item);
    double end_x = lyrics_line_end_x(item);

    double tolerance = 0.05 * item->spatium();
    if (end_x - start_x < style.style_absolute(Sid::LYRICS_MELISMA_MIN_LENGTH) - tolerance) {
        const Fraction ticks = is_partial_lyrics_line ? lyrics_line->ticks() : start_lyrics->ticks();
        if (style.style_b(Sid::LYRICS_MELISMA_FORCE) || ticks == Lyrics::temp_melisma_ticks) {
            end_x = start_x + style.style_absolute(Sid::LYRICS_MELISMA_MIN_LENGTH);
        } else {
            end_x = start_x;
        }
    }

    adjust_lyrics_line_y_offset(item);

    item->mutldata()->set_pos_x(start_x);
    item->set_pos2(PointF(end_x - start_x, 0.0));

    item->mutldata()->add_dash(LineF(PointF(), item->pos2()));
}

void LyricsLayout::layout_dashes(LyricsLineSegment *item) {
    const bool is_partial_lyrics_line = item->is_partial_lyrics_line_segment();
    LyricsLine *lyrics_line = item->lyrics_line();

    ChordRest *end_cr =
        lyrics_line->end_element() && lyrics_line->end_element()->is_chord_rest() ? to_chord_rest(lyrics_line->end_element()) : nullptr;
    Lyrics *end_lyrics = nullptr;
    if (end_cr) {
        for (Lyrics *lyr : end_cr->lyrics()) {
            if (lyr->verse() == item->verse()) {
                end_lyrics = lyr;
                break;
            }
        }
    }

    // When the end element is a timetick segment rather than a chordrest, the start element should be a chord with a following repeat
    ChordRest *start_cr = lyrics_line->start_element() && lyrics_line->start_element()->is_chord_rest() ?
                              to_chord_rest(lyrics_line->start_element()) :
                              nullptr;
    bool has_following_jump = end_cr ? end_cr->has_following_jump_item() : (start_cr ? start_cr->has_following_jump_item() : false);

    if (!end_lyrics && !is_partial_lyrics_line && !has_following_jump) {
        return;
    }

    System *system = item->system();
    if (!system) {
        return;
    }
    const MStyle &style = item->style();

    double start_x = lyrics_line_start_x(item);
    double end_x = 0.0;
    if (end_cr) {
        end_x = lyrics_line_end_x(item, end_lyrics);
    } else {
        end_x = start_cr ? start_cr->measure()->ending_x_for_open_ended_lines() : end_x;
    }

    adjust_lyrics_line_y_offset(item, end_lyrics);

    item->mutldata()->set_pos_x(start_x);
    item->set_pos2(PointF(end_x - start_x, 0.0));

    bool is_dash_on_first_syllable = lyrics_line->tick2() == system->first_measure()->tick();
    const double user_start = start_x + item->offset().x();
    const double user_end = end_x + item->user_off2().x();
    double cur_length = user_end - user_start;
    double dash_min_length = style.style_absolute(Sid::LYRICS_DASH_MIN_LENGTH);
    bool first_and_last_gap_are_half = style.style_b(Sid::LYRICS_DASH_FIRST_AND_LAST_GAP_ARE_HALF);
    bool force_dash = style.style_b(Sid::LYRICS_DASH_FORCE) ||
                      (style.style_b(Sid::LYRICS_SHOW_DASH_IF_SYLLABLE_ON_FIRST_NOTE) && is_dash_on_first_syllable);
    double max_dash_distance = style.style_absolute(Sid::LYRICS_DASH_MAX_DISTANCE);
    int dash_count = first_and_last_gap_are_half && cur_length > max_dash_distance ? std::ceil(cur_length / max_dash_distance) :
                                                                                     std::floor(cur_length / max_dash_distance);

    if (cur_length > dash_min_length || force_dash) {
        dash_count = std::max(dash_count, 1);
    }

    if (style.style_b(Sid::LYRICS_LIMIT_DASH_COUNT)) {
        dash_count = std::min(dash_count, style.style_i(Sid::LYRICS_MAX_DASH_COUNT));
    }

    if (cur_length < dash_min_length && dash_count > 0) {
        double diff = dash_min_length - cur_length;
        if (is_dash_on_first_syllable) {
            start_x -= diff;
        } else {
            start_x -= 0.5 * diff;
            end_x += 0.5 * diff;
        }
        item->mutldata()->set_pos_x(start_x);
        item->set_pos2(PointF(end_x - start_x, 0.0));
        cur_length = end_x - start_x;
    }

    double dash_width = std::min(cur_length, style.style_absolute(Sid::LYRICS_DASH_MAX_LENGTH));

    LyricsDashSystemStart lyrics_dash_system_start = style.style_v(Sid::LYRICS_DASH_POS_AT_START_OF_SYSTEM).value<LyricsDashSystemStart>();
    bool dashes_left_aligned = lyrics_dash_system_start != LyricsDashSystemStart::STANDARD && !item->is_single_begin_type();
    double dash_dist = cur_length / (dashes_left_aligned || first_and_last_gap_are_half ? dash_count : dash_count + 1);
    double x_dash = 0.0;
    if (dashes_left_aligned) {
        for (int i = 0; i < dash_count; ++i) {
            item->mutldata()->add_dash(LineF(PointF(x_dash, 0.0), PointF(x_dash + dash_width, 0.0)));
            x_dash += dash_dist;
        }
    } else {
        for (int i = 0; i < dash_count; ++i) {
            if (first_and_last_gap_are_half && i == 0) {
                x_dash += 0.5 * dash_dist;
            } else {
                x_dash += dash_dist;
            }
            item->mutldata()->add_dash(LineF(PointF(x_dash - 0.5 * dash_width, 0.0), PointF(x_dash + 0.5 * dash_width, 0.0)));
        }
    }
}

Lyrics *LyricsLayout::find_next_lyrics(const ChordRest *end_chord_rest, int verse_number, std::optional<PlacementV> placement) {
    if (!end_chord_rest) {
        return nullptr;
    }
    for (Segment *segment = end_chord_rest->segment()->next1(SegmentType::CHORD_REST); segment;
         segment = segment->next1(SegmentType::CHORD_REST)) {
        if (!segment->element(end_chord_rest->track())) {
            continue;
        }
        ChordRest *next_cr = to_chord_rest(segment->element(end_chord_rest->track()));
        for (Lyrics *lyr : next_cr->lyrics()) {
            if (lyr->verse() == verse_number && (!placement || lyr->placement() == placement)) {
                return lyr;
            }
        }
    }

    return nullptr;
}

void LyricsLayout::create_or_remove_lyrics_line(Lyrics *item, LayoutContext &ctx) {
    if (item->need_remove_invalid_segments()) {
        item->remove_invalid_segments();
    }

    auto is_end_melisma = [item]() { return item->ticks().is_not_zero(); };
    const Fraction lyrics_line_ticks = normalize_lyric_ticks(*item);

    ChordRest *cr = item->chord_rest();

    if (is_end_melisma() || item->syllabic() == LyricsSyllabic::BEGIN || item->syllabic() == LyricsSyllabic::MIDDLE) {
        if (!item->separator()) {
            LyricsLine *separator = Factory::create_lyrics_line(ctx.mut_dom().dummy_parent());
            separator->set_generated(true);
            separator->set_tick(cr->tick());
            separator->set_visible(item->visible());
            item->set_separator(separator);
            ctx.mut_dom().add_unmanaged_spanner(item->separator());
        }
        item->separator()->set_ownership_parent(item);
        item->separator()->set_tick(cr->tick());
        item->separator()->set_ticks(lyrics_line_ticks);
        item->separator()->set_track(item->track());
        item->separator()->set_track2(item->track());
        item->separator()->style_changed();
    } else {
        if (item->separator()) {
            item->separator()->remove_unmanaged();
            delete item->separator();
            item->set_separator(nullptr);
        }
    }
}

void LyricsLayout::compute_vertical_positions(System *system, LayoutContext &ctx) {
    staff_idx_t n_staves = system->score()->nstaves();

    std::vector<staff_idx_t> visible_staves;
    visible_staves.reserve(system->staves().size());

    for (staff_idx_t staff_idx = 0; staff_idx < n_staves; ++staff_idx) {
        if (system->staff(staff_idx)->show()) {
            compute_vertical_positions(staff_idx, system, ctx);
        }
    }
}

void LyricsLayout::compute_vertical_positions(staff_idx_t staff_idx, System *system, LayoutContext &ctx) {
    LyricsVersesMap lyrics_verses_above;
    LyricsVersesMap lyrics_verses_below;

    collect_lyrics_verses(staff_idx, system, lyrics_verses_above, lyrics_verses_below);

    set_default_positions(staff_idx, lyrics_verses_above, lyrics_verses_below, ctx);

    check_collisions_with_staff_elements(system, staff_idx, ctx, lyrics_verses_above, lyrics_verses_below);

    add_to_skyline(system, staff_idx, ctx, lyrics_verses_above, lyrics_verses_below);
}

void LyricsLayout::collect_lyrics_verses(staff_idx_t staff_idx, System *system, LyricsVersesMap &lyrics_verses_above,
                                         LyricsVersesMap &lyrics_verses_below) {
    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices;

    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &segment : to_measure(mb)->segments()) {
            if (!segment.is_chord_rest_type()) {
                continue;
            }
            for (track_idx_t track = start_track; track < end_track; ++track) {
                EngravingItem *element = segment.element(track);
                if (!element) {
                    continue;
                }
                for (Lyrics *lyrics : to_chord_rest(element)->lyrics()) {
                    int verse = lyrics->verse();
                    if (lyrics->place_above()) {
                        lyrics_verses_above[verse].add_lyrics(lyrics);
                    } else {
                        lyrics_verses_below[verse].add_lyrics(lyrics);
                    }
                }
            }
        }
    }

    for (SpannerSegment *spanner_segment : system->spanner_segments()) {
        if (spanner_segment->staff_idx() == staff_idx && spanner_segment->is_lyrics_line_segment()) {
            if (muse::real_is_null(spanner_segment->pos2().x())) {
                continue;
            }
            LyricsLineSegment *lyrics_line_segment = to_lyrics_line_segment(spanner_segment);
            int verse = lyrics_line_segment->verse();
            if (lyrics_line_segment->lyrics_place_above()) {
                lyrics_verses_above[verse].add_line(lyrics_line_segment);
            } else {
                lyrics_verses_below[verse].add_line(lyrics_line_segment);
            }
        }
    }
}

void LyricsLayout::set_default_positions(staff_idx_t staff_idx, const LyricsVersesMap &lyrics_verses_above,
                                         const LyricsVersesMap &lyrics_verses_below, LayoutContext &ctx) {
    double staff_height = ctx.dom().staff(staff_idx)->staff_height();
    double lyrics_line_height_factor = ctx.conf().style_d(Sid::LYRICS_LINE_HEIGHT);

    int max_verse_above = !lyrics_verses_above.empty() ? lyrics_verses_above.crbegin()->first : 0;
    for (auto &pair : lyrics_verses_above) {
        int verse = pair.first;
        const LyricsVerse &lyrics_verse = pair.second;
        for (Lyrics *lyrics : lyrics_verse.lyrics()) {
            double y = -(max_verse_above - verse) * lyrics->line_spacing() * lyrics_line_height_factor;
            PointF default_pos = lyrics->default_pos();
            y += default_pos.y();
            lyrics->set_y_relative_to_staff(y);
        }
        for (LyricsLineSegment *lyrics_line_segment : lyrics_verse.lines()) {
            double y = -(max_verse_above - verse) * lyrics_line_segment->line_spacing() * lyrics_line_height_factor;
            lyrics_line_segment->move(PointF(0.0, y + lyrics_line_segment->base_line_shift()));
        }
    }

    for (auto &pair : lyrics_verses_below) {
        int verse = pair.first;
        const LyricsVerse &lyrics_verse = pair.second;
        for (Lyrics *lyrics : lyrics_verse.lyrics()) {
            double y = staff_height + verse * lyrics->line_spacing() * lyrics_line_height_factor;
            PointF default_pos = lyrics->default_pos();
            y += default_pos.y();
            lyrics->set_y_relative_to_staff(y);
        }
        for (LyricsLineSegment *lyrics_line_segment : lyrics_verse.lines()) {
            double y = staff_height + verse * lyrics_line_segment->line_spacing() * lyrics_line_height_factor;
            lyrics_line_segment->move(PointF(0.0, y + lyrics_line_segment->base_line_shift()));
        }
    }
}

void LyricsLayout::check_collisions_with_staff_elements(System *system, staff_idx_t staff_idx, LayoutContext &ctx,
                                                        const LyricsVersesMap &lyrics_verses_above,
                                                        const LyricsVersesMap &lyrics_verses_below) {
    SysStaff *system_staff = system->staff(staff_idx);

    double lyrics_min_dist = ctx.conf().style_absolute(Sid::LYRICS_MIN_TOP_DISTANCE);

    SkylineLine &staff_skyline_north = system_staff->skyline().north();
    SkylineLine &staff_skyline_south = system_staff->skyline().south();

    int max_verse_above = !lyrics_verses_above.empty() ? lyrics_verses_above.crbegin()->first : 0;
    int max_verse_below = !lyrics_verses_below.empty() ? lyrics_verses_below.crbegin()->first : 0;

    for (int verse = max_verse_above; verse >= 0; --verse) {
        if (lyrics_verses_above.count(verse) == 0) {
            continue;
        }
        SkylineLine verse_skyline = create_skyline_for_verse(verse, false, lyrics_verses_above, system);
        double min_distance = -verse_skyline.min_distance(staff_skyline_north);
        if (min_distance < lyrics_min_dist) {
            double diff = lyrics_min_dist - min_distance;
            move_this_verse_and_outer_ones(verse, 0, true, -diff, lyrics_verses_above);
        }
    }

    for (int verse = 0; verse <= max_verse_below; ++verse) {
        if (lyrics_verses_below.count(verse) == 0) {
            continue;
        }
        SkylineLine verse_skyline = create_skyline_for_verse(verse, true, lyrics_verses_below, system);
        double min_distance = -staff_skyline_south.min_distance(verse_skyline);
        if (min_distance < lyrics_min_dist) {
            double diff = lyrics_min_dist - min_distance;
            move_this_verse_and_outer_ones(verse, max_verse_below, false, diff, lyrics_verses_below);
        }
    }
}

SkylineLine LyricsLayout::create_skyline_for_verse(int verse, bool north, const LyricsVersesMap &lyrics_verses, System *system) {
    double system_x = system->page_x();

    SkylineLine lyrics_skyline(north);

    if (lyrics_verses.count(verse) > 0) {
        const LyricsVerse &lyrics_verse = lyrics_verses.at(verse);
        for (Lyrics *lyrics : lyrics_verse.lyrics()) {
            if (lyrics->add_to_skyline()) {
                Shape lyrics_shape =
                    lyrics->high_res_shape().translated(PointF(lyrics->page_x() - system_x, lyrics->y_relative_to_staff()));
                lyrics_skyline.add(lyrics_shape);
            }
        }
        for (LyricsLineSegment *lyrics_line_seg : lyrics_verse.lines()) {
            if (lyrics_line_seg->lyrics_add_to_skyline()) {
                lyrics_skyline.add(lyrics_line_seg->shape().translate(lyrics_line_seg->pos()));
            }
        }
    }

    return lyrics_skyline;
}

void LyricsLayout::move_this_verse_and_outer_ones(int verse, int last_verse, bool above, double diff,
                                                  const LyricsVersesMap &lyrics_verses) {
    auto move_verse = [&](int verse) {
        if (lyrics_verses.count(verse) > 0) {
            const LyricsVerse &lyrics_verse = lyrics_verses.at(verse);
            for (Lyrics *lyrics : lyrics_verse.lyrics()) {
                lyrics->move(PointF(0.0, diff));
            }
            for (LyricsLineSegment *lyrics_line_seg : lyrics_verse.lines()) {
                lyrics_line_seg->move(PointF(0.0, diff));
            }
        }
    };

    if (above) {
        for (int other_verse = verse; other_verse >= last_verse; --other_verse) {
            move_verse(other_verse);
        }
    } else {
        for (int other_verse = verse; other_verse <= last_verse; ++other_verse) {
            move_verse(other_verse);
        }
    }
}

void LyricsLayout::add_to_skyline(System *system, staff_idx_t staff_idx, LayoutContext &ctx, const LyricsVersesMap &lyrics_verses_above,
                                  const LyricsVersesMap &lyrics_verses_below) {
    double system_x = system->page_x();
    // HACK: subtract minVerticalDistance here because it's added later during staff distance calculations. Needs a better solution.
    double lyrics_vertical_padding =
        ctx.conf().style_absolute(Sid::LYRICS_MIN_BOTTOM_DISTANCE) - ctx.conf().style_absolute(Sid::MIN_VERTICAL_DISTANCE);
    Skyline &skyline = system->staff(staff_idx)->skyline();
    for (auto &pair : lyrics_verses_above) {
        const LyricsVerse &lyrics_verse = pair.second;
        for (Lyrics *lyrics : lyrics_verse.lyrics()) {
            if (lyrics->add_to_skyline()) {
                Shape lyrics_shape =
                    lyrics->high_res_shape().translated(PointF(lyrics->page_x() - system_x, lyrics->y_relative_to_staff()));
                skyline.north().add(lyrics_shape.adjust(0.0, -lyrics_vertical_padding, 0.0, 0.0));
            }
        }
        for (LyricsLineSegment *lyrics_line_seg : lyrics_verse.lines()) {
            if (lyrics_line_seg->lyrics_add_to_skyline()) {
                Shape line_shape = lyrics_line_seg->shape().translate(lyrics_line_seg->pos());
                skyline.north().add(line_shape.adjust(0.0, -lyrics_vertical_padding, 0.0, 0.0));
            }
        }
    }
    for (auto &pair : lyrics_verses_below) {
        const LyricsVerse &lyrics_verse = pair.second;
        for (Lyrics *lyrics : lyrics_verse.lyrics()) {
            if (lyrics->add_to_skyline()) {
                Shape lyrics_shape =
                    lyrics->high_res_shape().translated(PointF(lyrics->page_x() - system_x, lyrics->y_relative_to_staff()));
                skyline.south().add(lyrics_shape.adjust(0.0, 0.0, 0.0, lyrics_vertical_padding));
            }
        }
        for (LyricsLineSegment *lyrics_line_seg : lyrics_verse.lines()) {
            if (lyrics_line_seg->lyrics_add_to_skyline()) {
                Shape line_shape = lyrics_line_seg->shape().translate(lyrics_line_seg->pos());
                skyline.south().add(line_shape.adjust(0.0, 0.0, 0.0, lyrics_vertical_padding));
            }
        }
    }
}

double LyricsLayout::lyrics_line_start_x(const LyricsLineSegment *item) {
    const System *system = item->system();
    const LyricsLine *lyrics_line = item->lyrics_line();
    const Lyrics *start_lyrics = lyrics_line->lyrics();
    const MStyle &style = item->style();
    const bool melisma = lyrics_line->is_end_melisma();

    const LyricsDashSystemStart lyrics_dash_system_start =
        style.style_v(Sid::LYRICS_DASH_POS_AT_START_OF_SYSTEM).value<LyricsDashSystemStart>();
    const bool leading = melisma || (lyrics_dash_system_start != LyricsDashSystemStart::UNDER_FIRST_NOTE);

    // Full melisma or dashes at beginning of system
    if (!item->is_single_begin_type()) {
        return system->first_note_rest_segment_x(leading);
    }

    // Partial melisma or dashes
    if (lyrics_line->is_partial_lyrics_line()) {
        const Measure *measure = lyrics_line->find_start_measure();
        return measure->first_note_rest_segment_x(leading);
    }

    // Full melisma or dashes in middle of system
    const double pad = melisma ? style.style_absolute(Sid::LYRICS_MELISMA_PAD) : style.style_absolute(Sid::LYRICS_DASH_PAD);
    const double lyrics_right_edge = start_lyrics->page_x() - system->page_x() + start_lyrics->shape().right();
    return lyrics_right_edge + pad;
}

double LyricsLayout::lyrics_line_end_x(const LyricsLineSegment *item, const Lyrics *end_lyrics) {
    const System *system = item->system();
    const LyricsLine *lyrics_line = item->lyrics_line();
    const ChordRest *end_chord_rest = to_chord_rest(lyrics_line->end_element());
    if (!end_chord_rest) {
        return system->ending_x_for_open_ended_lines();
    }
    const double system_page_x = system->page_x();
    const MStyle &style = item->style();
    const bool melisma = lyrics_line->is_end_melisma();
    const bool has_following_jump = end_chord_rest->has_following_jump_item();
    const bool has_preceding_jump = end_chord_rest->has_preceding_jump_item();

    if (!melisma && !item->is_partial_lyrics_line_segment() && !end_lyrics && !has_following_jump) {
        return 0.0;
    }

    const bool dashes_end_of_system = !melisma && lyrics_line->tick2() == system->end_tick();

    // Full melisma or dashes at end of system
    if (!item->is_single_end_type() || dashes_end_of_system || !lyrics_line->end_element()) {
        return system->ending_x_for_open_ended_lines();
    }

    // Partial dashes after a repeat
    if (!melisma && has_preceding_jump && item->is_partial_lyrics_line_segment()) {
        if (end_lyrics) {
            const double lyrics_left_edge = end_lyrics->page_x() - system_page_x + end_lyrics->ldata()->bbox().left();
            return lyrics_left_edge - style.style_absolute(Sid::LYRICS_DASH_PAD);
        } else if (end_chord_rest->is_chord()) {
            const Chord *end_chord = to_chord(end_chord_rest);
            const Note *note = end_chord->up() ? end_chord->down_note() : end_chord->up_note();
            return note->page_x() - system_page_x + note->head_width();
        }
    }

    // Full dashes or melisma before a repeat - possibly with a partial dash/repeat on the other side of the repeat
    if (end_chord_rest && has_following_jump && !lyrics_line->next_lyrics()) {
        Measure *end_measure = lyrics_line->find_end_measure();
        // check if there is a partial lyrics line in a following measure
        if (repeat_has_partial_lyric_line(end_measure) || end_chord_rest == lyrics_line->start_element()) {
            return end_measure->ending_x_for_open_ended_lines();
        }
    }

    // Full or partial dashes in middle of system
    if (!melisma && end_lyrics) {
        const double lyrics_left_edge = end_lyrics->page_x() - system_page_x + end_lyrics->ldata()->bbox().left();
        return lyrics_left_edge - style.style_absolute(Sid::LYRICS_DASH_PAD);
    }

    // Full or partial melisma in middle of system
    if (end_chord_rest->is_chord()) {
        const Chord *end_chord = to_chord(end_chord_rest);
        const Note *note = end_chord->up() ? end_chord->down_note() : end_chord->up_note();
        return note->page_x() - system_page_x + note->head_width();
    }

    return end_chord_rest->page_x() - system_page_x + end_chord_rest->right_edge();
}

void LyricsLayout::adjust_lyrics_line_y_offset(LyricsLineSegment *item, const Lyrics *end_lyrics) {
    const LyricsLine *lyrics_line = item->lyrics_line();
    ChordRest *end_chord_rest =
        lyrics_line->end_element() && lyrics_line->end_element()->is_chord_rest() ? to_chord_rest(lyrics_line->end_element()) : nullptr;
    const Lyrics *start_lyrics = lyrics_line->lyrics();
    const bool melisma = lyrics_line->is_end_melisma();

    LyricsLineSegment::LayoutData *ldata = item->mutldata();

    // Partial melisma or dashes
    if (lyrics_line->is_partial_lyrics_line()) {
        // A partial lyrics line's placement is independent of the lyrics around it, so only
        // take the Y from a lyric which is on the same side of the staff:
        Lyrics *next_lyrics = find_next_lyrics(end_chord_rest, item->verse(), lyrics_line->placement());
        if (next_lyrics) {
            PointF next_lyrics_default_pos = next_lyrics->default_pos();
            ldata->set_pos_y(next_lyrics->offset().y() + next_lyrics_default_pos.y());
        } else {
            PointF lyrics_offset =
                item->style_value(Pid::OFFSET, item->place_below() ? Sid::LYRICS_POS_BELOW : Sid::LYRICS_POS_ABOVE).value<PointF>();
            ldata->set_pos_y(lyrics_offset.y());
        }
        return;
    }

    if (item->is_single_begin_type()) {
        PointF start_lyrics_default_pos = start_lyrics->default_pos();
        ldata->set_pos_y(start_lyrics->offset().y() + start_lyrics_default_pos.y());
        return;
    }

    if (melisma || !end_lyrics) {
        const Lyrics *next_lyrics = find_next_lyrics(end_chord_rest, item->verse());

        const Lyrics *ref_lyrics = next_lyrics ? next_lyrics : start_lyrics;
        PointF ref_lyrics_default_pos = ref_lyrics->default_pos();

        ldata->set_pos_y(ref_lyrics->offset().y() + ref_lyrics_default_pos.y());
        return;
    }

    PointF end_lyrics_default_pos = end_lyrics->default_pos();

    ldata->set_pos_y(end_lyrics->offset().y() + end_lyrics_default_pos.y());
}
