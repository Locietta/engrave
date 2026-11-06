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
#include "textstyle.h"

#include "dom/property.h"
#include "log.h"

using namespace mu;

namespace mu::engraving {
const TextStyle default_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::DEFAULT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::DEFAULT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::DEFAULT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::DEFAULT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::DEFAULT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::DEFAULT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::DEFAULT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::DEFAULT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::DEFAULT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::DEFAULT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::DEFAULT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::DEFAULT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::DEFAULT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::DEFAULT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::DEFAULT_POSITION, Pid::POSITION},
    }},
    {Sid::DEFAULT_OFFSET, Sid::DEFAULT_OFFSET}};

const TextStyle title_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::TITLE_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::TITLE_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::TITLE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::TITLE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::TITLE_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::TITLE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::TITLE_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::TITLE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::TITLE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::TITLE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::TITLE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::TITLE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::TITLE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::TITLE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::TITLE_POSITION, Pid::POSITION},
    }},
    {Sid::TITLE_OFFSET, Sid::TITLE_OFFSET}};

const TextStyle sub_title_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::SUB_TITLE_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::SUB_TITLE_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::SUB_TITLE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::SUB_TITLE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::SUB_TITLE_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::SUB_TITLE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::SUB_TITLE_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::SUB_TITLE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::SUB_TITLE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::SUB_TITLE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::SUB_TITLE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::SUB_TITLE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::SUB_TITLE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::SUB_TITLE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::SUB_TITLE_POSITION, Pid::POSITION},
    }},
    {Sid::SUB_TITLE_OFFSET, Sid::SUB_TITLE_OFFSET}};

const TextStyle composer_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::COMPOSER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::COMPOSER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::COMPOSER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::COMPOSER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::COMPOSER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::COMPOSER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::COMPOSER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::COMPOSER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::COMPOSER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::COMPOSER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::COMPOSER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::COMPOSER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::COMPOSER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::COMPOSER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::COMPOSER_POSITION, Pid::POSITION},
    }},
    {Sid::COMPOSER_OFFSET, Sid::COMPOSER_OFFSET}};

const TextStyle lyricist_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::LYRICIST_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::LYRICIST_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::LYRICIST_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::LYRICIST_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::LYRICIST_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::LYRICIST_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::LYRICIST_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::LYRICIST_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::LYRICIST_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::LYRICIST_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::LYRICIST_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::LYRICIST_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::LYRICIST_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::LYRICIST_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::LYRICIST_POSITION, Pid::POSITION},
    }},
    {Sid::LYRICIST_OFFSET, Sid::LYRICIST_OFFSET}};

const TextStyle lyrics_even_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::LYRICS_EVEN_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::LYRICS_EVEN_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::LYRICS_EVEN_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::LYRICS_EVEN_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::LYRICS_EVEN_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::LYRICS_EVEN_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::LYRICS_EVEN_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::LYRICS_EVEN_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::LYRICS_EVEN_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::LYRICS_EVEN_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::LYRICS_EVEN_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::LYRICS_EVEN_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::LYRICS_EVEN_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::LYRICS_EVEN_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::LYRICS_EVEN_POSITION, Pid::POSITION},
    }},
    {Sid::LYRICS_POS_ABOVE, Sid::LYRICS_POS_BELOW}};

const TextStyle lyrics_odd_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::LYRICS_ODD_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::LYRICS_ODD_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::LYRICS_ODD_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::LYRICS_ODD_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::LYRICS_ODD_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::LYRICS_ODD_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::LYRICS_ODD_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::LYRICS_ODD_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::LYRICS_ODD_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::LYRICS_ODD_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::LYRICS_ODD_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::LYRICS_ODD_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::LYRICS_ODD_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::LYRICS_ODD_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::LYRICS_ODD_POSITION, Pid::POSITION},
    }},
    {Sid::LYRICS_POS_ABOVE, Sid::LYRICS_POS_BELOW}};

const TextStyle fingering_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::FINGERING_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::FINGERING_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::FINGERING_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::FINGERING_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::FINGERING_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::FINGERING_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::FINGERING_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::FINGERING_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::FINGERING_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::FINGERING_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::FINGERING_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::FINGERING_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::FINGERING_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::FINGERING_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::FINGERING_POSITION, Pid::POSITION},
    }},
    {Sid::FINGERING_OFFSET, Sid::FINGERING_OFFSET}};

const TextStyle tab_fret_number_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::TAB_FRET_NUMBER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::TAB_FRET_NUMBER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::TAB_FRET_NUMBER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::TAB_FRET_NUMBER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::TAB_FRET_NUMBER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::TAB_FRET_NUMBER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::TAB_FRET_NUMBER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::TAB_FRET_NUMBER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::TAB_FRET_NUMBER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::TAB_FRET_NUMBER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::TAB_FRET_NUMBER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::TAB_FRET_NUMBER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::TAB_FRET_NUMBER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::TAB_FRET_NUMBER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
    }},
    {Sid::TAB_FRET_NUMBER_OFFSET, Sid::TAB_FRET_NUMBER_OFFSET}};

const TextStyle lh_guitar_fingering_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::LH_GUITAR_FINGERING_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::LH_GUITAR_FINGERING_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::LH_GUITAR_FINGERING_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::LH_GUITAR_FINGERING_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::LH_GUITAR_FINGERING_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::LH_GUITAR_FINGERING_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::LH_GUITAR_FINGERING_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::LH_GUITAR_FINGERING_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::LH_GUITAR_FINGERING_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::LH_GUITAR_FINGERING_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::LH_GUITAR_FINGERING_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::LH_GUITAR_FINGERING_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::LH_GUITAR_FINGERING_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::LH_GUITAR_FINGERING_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::LH_GUITAR_FINGERING_POSITION, Pid::POSITION},
    }},
    {Sid::LH_GUITAR_FINGERING_OFFSET, Sid::LH_GUITAR_FINGERING_OFFSET}};

const TextStyle rh_guitar_fingering_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::RH_GUITAR_FINGERING_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::RH_GUITAR_FINGERING_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::RH_GUITAR_FINGERING_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::RH_GUITAR_FINGERING_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::RH_GUITAR_FINGERING_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::RH_GUITAR_FINGERING_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::RH_GUITAR_FINGERING_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::RH_GUITAR_FINGERING_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::RH_GUITAR_FINGERING_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::RH_GUITAR_FINGERING_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::RH_GUITAR_FINGERING_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::RH_GUITAR_FINGERING_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::RH_GUITAR_FINGERING_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::RH_GUITAR_FINGERING_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::RH_GUITAR_FINGERING_POSITION, Pid::POSITION},
    }},
    {Sid::RH_GUITAR_FINGERING_OFFSET, Sid::RH_GUITAR_FINGERING_OFFSET}};

const TextStyle hammer_on_pull_off_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::HAMMER_ON_PULL_OFF_TAPPING_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::HAMMER_ON_PULL_OFF_TAPPING_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::HAMMER_ON_PULL_OFF_TAPPING_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::HAMMER_ON_PULL_OFF_TAPPING_FONT_SPATIUM_DEPENDENT,
         Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::HAMMER_ON_PULL_OFF_TAPPING_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::HAMMER_ON_PULL_OFF_TAPPING_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::HAMMER_ON_PULL_OFF_TAPPING_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::HAMMER_ON_PULL_OFF_TAPPING_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::HAMMER_ON_PULL_OFF_TAPPING_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::HAMMER_ON_PULL_OFF_TAPPING_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::HAMMER_ON_PULL_OFF_TAPPING_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::HAMMER_ON_PULL_OFF_TAPPING_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::HAMMER_ON_PULL_OFF_TAPPING_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::HAMMER_ON_PULL_OFF_TAPPING_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
    }},
    {Sid::HAMMER_ON_PULL_OFF_TAPPING_OFFSET, Sid::HAMMER_ON_PULL_OFF_TAPPING_OFFSET}};

const TextStyle string_number_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::STRING_NUMBER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::STRING_NUMBER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::STRING_NUMBER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::STRING_NUMBER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::STRING_NUMBER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::STRING_NUMBER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::STRING_NUMBER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::STRING_NUMBER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::STRING_NUMBER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::STRING_NUMBER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::STRING_NUMBER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::STRING_NUMBER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::STRING_NUMBER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::STRING_NUMBER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::STRING_NUMBER_POSITION, Pid::POSITION},
    }},
    {Sid::STRING_NUMBER_OFFSET, Sid::STRING_NUMBER_OFFSET}};

const TextStyle string_tunings_style{
    {{
        // identical to staffText except for fontSize
        {TextStylePropertyType::FONT_FACE, Sid::STAFF_TEXT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::STRING_TUNINGS_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::STAFF_TEXT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::STAFF_TEXT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::STAFF_TEXT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::STAFF_TEXT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::STAFF_TEXT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::STAFF_TEXT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::STAFF_TEXT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::STAFF_TEXT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::STAFF_TEXT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::STAFF_TEXT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::STAFF_TEXT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::STRING_TUNINGS_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
    }},
    {Sid::STAFF_TEXT_POS_ABOVE, Sid::STAFF_TEXT_POS_BELOW}};

const TextStyle fret_diagram_fingering_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::FRET_DIAGRAM_FINGERING_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::FRET_DIAGRAM_FINGERING_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::FRET_DIAGRAM_FINGERING_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::FRET_DIAGRAM_FINGERING_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::FRET_DIAGRAM_FINGERING_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::FRET_DIAGRAM_FINGERING_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::FRET_DIAGRAM_FINGERING_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::FRET_DIAGRAM_FINGERING_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::FRET_DIAGRAM_FINGERING_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::FRET_DIAGRAM_FINGERING_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::FRET_DIAGRAM_FINGERING_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::FRET_DIAGRAM_FINGERING_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::FRET_DIAGRAM_FINGERING_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::FRET_DIAGRAM_FINGERING_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::FRET_DIAGRAM_FINGERING_POSITION, Pid::POSITION},
    }},
    {Sid::FRET_DIAGRAM_FINGERING_POS_ABOVE, Sid::FRET_DIAGRAM_FINGERING_POS_ABOVE}};

const TextStyle fret_diagram_fret_number_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::FRET_DIAGRAM_FRET_NUMBER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::FRET_DIAGRAM_FRET_NUMBER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::FRET_DIAGRAM_FRET_NUMBER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::FRET_DIAGRAM_FRET_NUMBER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::FRET_DIAGRAM_FRET_NUMBER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::FRET_DIAGRAM_FRET_NUMBER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::FRET_DIAGRAM_FRET_NUMBER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::FRET_DIAGRAM_FRET_NUMBER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::FRET_DIAGRAM_FRET_NUMBER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::FRET_DIAGRAM_FRET_NUMBER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::FRET_DIAGRAM_FRET_NUMBER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::FRET_DIAGRAM_FRET_NUMBER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::FRET_DIAGRAM_FRET_NUMBER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::FRET_DIAGRAM_FRET_NUMBER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
    }},
    {Sid::FRET_DIAGRAM_FRET_NUMBER_POS_ABOVE, Sid::FRET_DIAGRAM_FRET_NUMBER_POS_ABOVE}};

const TextStyle harp_pedal_diagram_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::HARP_PEDAL_DIAGRAM_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::HARP_PEDAL_DIAGRAM_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::HARP_PEDAL_DIAGRAM_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::HARP_PEDAL_DIAGRAM_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::HARP_PEDAL_DIAGRAM_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::HARP_PEDAL_DIAGRAM_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::HARP_PEDAL_DIAGRAM_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::HARP_PEDAL_DIAGRAM_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::HARP_PEDAL_DIAGRAM_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::HARP_PEDAL_DIAGRAM_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::HARP_PEDAL_DIAGRAM_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::HARP_PEDAL_DIAGRAM_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::HARP_PEDAL_DIAGRAM_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::HARP_PEDAL_DIAGRAM_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::HARP_PEDAL_DIAGRAM_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::HARP_PEDAL_DIAGRAM_POSITION, Pid::POSITION},
    }},
    {Sid::HARP_PEDAL_DIAGRAM_OFFSET, Sid::HARP_PEDAL_DIAGRAM_OFFSET}};

const TextStyle articulation_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::ARTICULATION_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::ARTICULATION_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::ARTICULATION_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::ARTICULATION_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::ARTICULATION_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::ARTICULATION_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::ARTICULATION_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::ARTICULATION_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::ARTICULATION_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::ARTICULATION_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::ARTICULATION_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::ARTICULATION_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::ARTICULATION_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::ARTICULATION_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
    }},
    {Sid::ARTICULATION_OFFSET, Sid::ARTICULATION_OFFSET}};

const TextStyle harp_pedal_text_diagram_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::HARP_PEDAL_TEXT_DIAGRAM_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::HARP_PEDAL_TEXT_DIAGRAM_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::HARP_PEDAL_TEXT_DIAGRAM_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::HARP_PEDAL_TEXT_DIAGRAM_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::HARP_PEDAL_TEXT_DIAGRAM_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::HARP_PEDAL_TEXT_DIAGRAM_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::HARP_PEDAL_TEXT_DIAGRAM_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::HARP_PEDAL_TEXT_DIAGRAM_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::HARP_PEDAL_TEXT_DIAGRAM_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::HARP_PEDAL_TEXT_DIAGRAM_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::HARP_PEDAL_TEXT_DIAGRAM_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::HARP_PEDAL_TEXT_DIAGRAM_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::HARP_PEDAL_TEXT_DIAGRAM_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::HARP_PEDAL_TEXT_DIAGRAM_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::HARP_PEDAL_TEXT_DIAGRAM_POSITION, Pid::POSITION},
    }},
    {Sid::HARP_PEDAL_TEXT_DIAGRAM_OFFSET, Sid::HARP_PEDAL_TEXT_DIAGRAM_OFFSET}};

const TextStyle long_instrument_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::LONG_INSTRUMENT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::LONG_INSTRUMENT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::LONG_INSTRUMENT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::LONG_INSTRUMENT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::LONG_INSTRUMENT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::LONG_INSTRUMENT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::LONG_INSTRUMENT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::LONG_INSTRUMENT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::LONG_INSTRUMENT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::LONG_INSTRUMENT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::LONG_INSTRUMENT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::LONG_INSTRUMENT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::LONG_INSTRUMENT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::LONG_INSTRUMENT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::LONG_INSTRUMENT_POSITION, Pid::POSITION},
    }},
    {Sid::LONG_INSTRUMENT_OFFSET, Sid::LONG_INSTRUMENT_OFFSET}};

const TextStyle short_instrument_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::SHORT_INSTRUMENT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::SHORT_INSTRUMENT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::SHORT_INSTRUMENT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::SHORT_INSTRUMENT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::SHORT_INSTRUMENT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::SHORT_INSTRUMENT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::SHORT_INSTRUMENT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::SHORT_INSTRUMENT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::SHORT_INSTRUMENT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::SHORT_INSTRUMENT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::SHORT_INSTRUMENT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::SHORT_INSTRUMENT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::SHORT_INSTRUMENT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::SHORT_INSTRUMENT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::SHORT_INSTRUMENT_POSITION, Pid::POSITION},
    }},
    {Sid::SHORT_INSTRUMENT_OFFSET, Sid::SHORT_INSTRUMENT_OFFSET}};

const TextStyle part_instrument_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::PART_INSTRUMENT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::PART_INSTRUMENT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::PART_INSTRUMENT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::PART_INSTRUMENT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::PART_INSTRUMENT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::PART_INSTRUMENT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::PART_INSTRUMENT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::PART_INSTRUMENT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::PART_INSTRUMENT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::PART_INSTRUMENT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::PART_INSTRUMENT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::PART_INSTRUMENT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::PART_INSTRUMENT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::PART_INSTRUMENT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::PART_INSTRUMENT_POSITION, Pid::POSITION},
    }},
    {Sid::PART_INSTRUMENT_OFFSET, Sid::PART_INSTRUMENT_OFFSET}};

const TextStyle group_bracket_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::GROUP_BRACKET_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::GROUP_BRACKET_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::GROUP_BRACKET_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::GROUP_BRACKET_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::GROUP_BRACKET_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::GROUP_BRACKET_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::GROUP_BRACKET_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::GROUP_BRACKET_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::GROUP_BRACKET_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::GROUP_BRACKET_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::GROUP_BRACKET_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::GROUP_BRACKET_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::GROUP_BRACKET_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::GROUP_BRACKET_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::LONG_INSTRUMENT_POSITION, Pid::POSITION},
    }},
    {Sid::GROUP_BRACKET_OFFSET, Sid::GROUP_BRACKET_OFFSET}};

// With the single exception of text alignment, after 4.1 Dynamics use Expression text style
const TextStyle dynamics_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::EXPRESSION_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::EXPRESSION_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::EXPRESSION_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::EXPRESSION_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::EXPRESSION_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::EXPRESSION_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::DYNAMICS_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::EXPRESSION_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::EXPRESSION_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::EXPRESSION_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::EXPRESSION_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::EXPRESSION_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::EXPRESSION_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DYNAMICS_SIZE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::DYNAMICS_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::DYNAMICS_POSITION, Pid::POSITION},
    }},
    {Sid::DYNAMICS_POS_ABOVE, Sid::DYNAMICS_POS_BELOW}};

const TextStyle expression_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::EXPRESSION_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::EXPRESSION_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::EXPRESSION_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::EXPRESSION_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::EXPRESSION_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::EXPRESSION_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::EXPRESSION_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::EXPRESSION_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::EXPRESSION_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::EXPRESSION_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::EXPRESSION_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::EXPRESSION_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::EXPRESSION_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::EXPRESSION_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::EXPRESSION_POSITION, Pid::POSITION},
    }},
    {Sid::EXPRESSION_POS_ABOVE, Sid::EXPRESSION_POS_BELOW}};

const TextStyle tempo_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::TEMPO_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::TEMPO_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::TEMPO_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::TEMPO_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::TEMPO_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::TEMPO_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::TEMPO_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::TEMPO_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::TEMPO_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::TEMPO_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::TEMPO_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::TEMPO_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::TEMPO_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::TEMPO_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::TEMPO_POSITION, Pid::POSITION},
    }},
    {Sid::TEMPO_POS_ABOVE, Sid::TEMPO_POS_BELOW}};

const TextStyle tempo_change_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::TEMPO_CHANGE_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::TEMPO_CHANGE_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::TEMPO_CHANGE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::TEMPO_CHANGE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::TEMPO_CHANGE_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::TEMPO_CHANGE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::TEMPO_CHANGE_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::TEMPO_CHANGE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::TEMPO_CHANGE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::TEMPO_CHANGE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::TEMPO_CHANGE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::TEMPO_CHANGE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::TEMPO_CHANGE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::TEMPO_CHANGE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::TEMPO_CHANGE_POSITION, Pid::POSITION},
    }},
    {Sid::TEMPO_CHANGE_POS_ABOVE, Sid::TEMPO_CHANGE_POS_BELOW}};

const TextStyle metronome_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::METRONOME_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::METRONOME_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::METRONOME_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::METRONOME_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::METRONOME_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::METRONOME_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::METRONOME_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::METRONOME_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::METRONOME_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::METRONOME_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::METRONOME_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::METRONOME_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::METRONOME_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::METRONOME_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
    }},
    {Sid::METRONOME_OFFSET, Sid::METRONOME_OFFSET}};

const TextStyle measure_number_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::MEASURE_NUMBER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::MEASURE_NUMBER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::MEASURE_NUMBER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::MEASURE_NUMBER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::MEASURE_NUMBER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::MEASURE_NUMBER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::MEASURE_NUMBER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::MEASURE_NUMBER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::MEASURE_NUMBER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::MEASURE_NUMBER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::MEASURE_NUMBER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::MEASURE_NUMBER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::MEASURE_NUMBER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::MEASURE_NUMBER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::MEASURE_NUMBER_POSITION, Pid::POSITION},
    }},
    {Sid::MEASURE_NUMBER_POS_ABOVE, Sid::MEASURE_NUMBER_POS_BELOW}};

const TextStyle measure_number_alternate_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::MEASURE_NUMBER_ALTERNATE_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::MEASURE_NUMBER_ALTERNATE_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::MEASURE_NUMBER_ALTERNATE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::MEASURE_NUMBER_ALTERNATE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::MEASURE_NUMBER_ALTERNATE_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::MEASURE_NUMBER_ALTERNATE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::MEASURE_NUMBER_ALTERNATE_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::MEASURE_NUMBER_ALTERNATE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::MEASURE_NUMBER_ALTERNATE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::MEASURE_NUMBER_ALTERNATE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::MEASURE_NUMBER_ALTERNATE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::MEASURE_NUMBER_ALTERNATE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::MEASURE_NUMBER_ALTERNATE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::MEASURE_NUMBER_ALTERNATE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::MEASURE_NUMBER_ALTERNATE_POSITION, Pid::POSITION},
    }},
    {Sid::MEASURE_NUMBER_ALTERNATE_POS_ABOVE, Sid::MEASURE_NUMBER_ALTERNATE_POS_BELOW}};

const TextStyle mm_rest_range_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::MM_REST_RANGE_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::MM_REST_RANGE_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::MM_REST_RANGE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::MM_REST_RANGE_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::MM_REST_RANGE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::MM_REST_RANGE_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::MM_REST_RANGE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::MM_REST_RANGE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::MM_REST_RANGE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::MM_REST_RANGE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::MM_REST_RANGE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::MM_REST_RANGE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::MM_REST_RANGE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::MM_REST_RANGE_POSITION, Pid::POSITION},
    }},
    {Sid::MM_REST_RANGE_POS_ABOVE, Sid::MM_REST_RANGE_POS_BELOW}};

const TextStyle translator_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::TRANSLATOR_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::TRANSLATOR_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::TRANSLATOR_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::TRANSLATOR_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::TRANSLATOR_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::TRANSLATOR_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::TRANSLATOR_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::TRANSLATOR_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::TRANSLATOR_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::TRANSLATOR_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::TRANSLATOR_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::TRANSLATOR_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::TRANSLATOR_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::TRANSLATOR_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::TRANSLATOR_POSITION, Pid::POSITION},
    }},
    {Sid::TRANSLATOR_OFFSET, Sid::TRANSLATOR_OFFSET}};

const TextStyle tuplet_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::TUPLET_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::TUPLET_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::TUPLET_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::TUPLET_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::TUPLET_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::TUPLET_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::TUPLET_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::TUPLET_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::TUPLET_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::TUPLET_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::TUPLET_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::TUPLET_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::TUPLET_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::TUPLET_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::TUPLET_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::TUPLET_POSITION, Pid::POSITION},
    }},
    {Sid::TUPLET_OFFSET, Sid::TUPLET_OFFSET}};

const TextStyle system_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::SYSTEM_TEXT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::SYSTEM_TEXT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::SYSTEM_TEXT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::SYSTEM_TEXT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::SYSTEM_TEXT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::SYSTEM_TEXT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::SYSTEM_TEXT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::SYSTEM_TEXT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::SYSTEM_TEXT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::SYSTEM_TEXT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::SYSTEM_TEXT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::SYSTEM_TEXT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::SYSTEM_TEXT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::SYSTEM_TEXT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::SYSTEM_TEXT_POSITION, Pid::POSITION},
    }},
    {Sid::SYSTEM_TEXT_POS_ABOVE, Sid::SYSTEM_TEXT_POS_BELOW}};

const TextStyle staff_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::STAFF_TEXT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::STAFF_TEXT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::STAFF_TEXT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::STAFF_TEXT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::STAFF_TEXT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::STAFF_TEXT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::STAFF_TEXT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::STAFF_TEXT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::STAFF_TEXT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::STAFF_TEXT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::STAFF_TEXT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::STAFF_TEXT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::STAFF_TEXT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::STAFF_TEXT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::STAFF_TEXT_POSITION, Pid::POSITION},
    }},
    {Sid::STAFF_TEXT_POS_ABOVE, Sid::STAFF_TEXT_POS_BELOW}};

const TextStyle stave_sharing_label_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::STAVE_SHARING_LABEL_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::STAVE_SHARING_LABEL_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::STAVE_SHARING_LABEL_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::STAVE_SHARING_LABEL_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::STAVE_SHARING_LABEL_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::STAVE_SHARING_LABEL_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::STAVE_SHARING_LABEL_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::STAVE_SHARING_LABEL_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::STAVE_SHARING_LABEL_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::STAVE_SHARING_LABEL_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::STAVE_SHARING_LABEL_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::STAVE_SHARING_LABEL_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::STAVE_SHARING_LABEL_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::STAFF_TEXT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::STAVE_SHARING_LABEL_POSITION, Pid::POSITION},
    }},
    {Sid::STAVE_SHARING_LABEL_POS_ABOVE, Sid::STAVE_SHARING_LABEL_POS_BELOW}};

const TextStyle chord_symbol_text_style_a{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::CHORD_SYMBOL_A_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::CHORD_SYMBOL_A_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::CHORD_SYMBOL_A_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::CHORD_SYMBOL_A_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::CHORD_SYMBOL_A_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::CHORD_SYMBOL_A_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::CHORD_SYMBOL_A_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::CHORD_SYMBOL_A_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::CHORD_SYMBOL_A_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::CHORD_SYMBOL_A_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::CHORD_SYMBOL_A_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::CHORD_SYMBOL_A_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::CHORD_SYMBOL_A_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::CHORD_SYMBOL_A_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::CHORD_SYM_POSITION, Pid::POSITION},
    }},
    {Sid::CHORD_SYMBOL_A_POS_ABOVE, Sid::CHORD_SYMBOL_A_POS_BELOW}};

const TextStyle chord_symbol_text_style_b{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::CHORD_SYMBOL_B_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::CHORD_SYMBOL_B_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::CHORD_SYMBOL_B_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::CHORD_SYMBOL_B_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::CHORD_SYMBOL_B_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::CHORD_SYMBOL_B_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::CHORD_SYMBOL_B_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::CHORD_SYMBOL_B_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::CHORD_SYMBOL_B_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::CHORD_SYMBOL_B_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::CHORD_SYMBOL_B_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::CHORD_SYMBOL_B_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::CHORD_SYMBOL_B_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::CHORD_SYMBOL_B_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::CHORD_SYM_POSITION, Pid::POSITION},
    }},
    {Sid::CHORD_SYMBOL_B_POS_ABOVE, Sid::CHORD_SYMBOL_B_POS_BELOW}};

const TextStyle roman_numeral_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::ROMAN_NUMERAL_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::ROMAN_NUMERAL_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::ROMAN_NUMERAL_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::ROMAN_NUMERAL_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::ROMAN_NUMERAL_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::ROMAN_NUMERAL_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::ROMAN_NUMERAL_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::ROMAN_NUMERAL_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::ROMAN_NUMERAL_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::ROMAN_NUMERAL_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::ROMAN_NUMERAL_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::ROMAN_NUMERAL_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::ROMAN_NUMERAL_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::ROMAN_NUMERAL_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::CHORD_SYM_POSITION, Pid::POSITION},
    }},
    {Sid::ROMAN_NUMERAL_POS_ABOVE, Sid::ROMAN_NUMERAL_POS_BELOW}};

const TextStyle nashville_number_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::NASHVILLE_NUMBER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::NASHVILLE_NUMBER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::NASHVILLE_NUMBER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::NASHVILLE_NUMBER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::NASHVILLE_NUMBER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::NASHVILLE_NUMBER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::NASHVILLE_NUMBER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::NASHVILLE_NUMBER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::NASHVILLE_NUMBER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::NASHVILLE_NUMBER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::NASHVILLE_NUMBER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::NASHVILLE_NUMBER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::NASHVILLE_NUMBER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::NASHVILLE_NUMBER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::CHORD_SYM_POSITION, Pid::POSITION},
    }},
    {Sid::NASHVILLE_NUMBER_POS_ABOVE, Sid::NASHVILLE_NUMBER_POS_BELOW}};

const TextStyle rehearsal_mark_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::REHEARSAL_MARK_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::REHEARSAL_MARK_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::REHEARSAL_MARK_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::REHEARSAL_MARK_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::REHEARSAL_MARK_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::REHEARSAL_MARK_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::REHEARSAL_MARK_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::REHEARSAL_MARK_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::REHEARSAL_MARK_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::REHEARSAL_MARK_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::REHEARSAL_MARK_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::REHEARSAL_MARK_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::REHEARSAL_MARK_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::REHEARSAL_MARK_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::REHEARSAL_MARK_POSITION, Pid::POSITION},
    }},
    {Sid::REHEARSAL_MARK_POS_ABOVE, Sid::REHEARSAL_MARK_POS_BELOW}};

const TextStyle repeat_play_count_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::REPEAT_PLAY_COUNT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::REPEAT_PLAY_COUNT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::REPEAT_PLAY_COUNT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::REPEAT_PLAY_COUNT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::REPEAT_PLAY_COUNT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::REPEAT_PLAY_COUNT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::REPEAT_PLAY_COUNT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::REPEAT_PLAY_COUNT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::REPEAT_PLAY_COUNT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::REPEAT_PLAY_COUNT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::REPEAT_PLAY_COUNT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::REPEAT_PLAY_COUNT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::REPEAT_PLAY_COUNT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::REPEAT_PLAY_COUNT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::REPEAT_PLAY_COUNT_POSITION, Pid::POSITION},
    }},
    {Sid::REPEAT_PLAY_COUNT_POS_ABOVE, Sid::REPEAT_PLAY_COUNT_POS_BELOW}};

const TextStyle repeat_left_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::REPEAT_LEFT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::REPEAT_LEFT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::REPEAT_LEFT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::REPEAT_LEFT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::REPEAT_LEFT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::REPEAT_LEFT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::REPEAT_LEFT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::REPEAT_LEFT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::REPEAT_LEFT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::REPEAT_LEFT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::REPEAT_LEFT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::REPEAT_LEFT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::REPEAT_LEFT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::REPEAT_LEFT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::REPEAT_LEFT_POSITION, Pid::POSITION},
    }},
    {Sid::MARKER_POS_ABOVE, Sid::MARKER_POS_ABOVE}};

const TextStyle repeat_right_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::REPEAT_RIGHT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::REPEAT_RIGHT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::REPEAT_RIGHT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::REPEAT_RIGHT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::REPEAT_RIGHT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::REPEAT_RIGHT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::REPEAT_RIGHT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::REPEAT_RIGHT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::REPEAT_RIGHT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::REPEAT_RIGHT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::REPEAT_RIGHT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::REPEAT_RIGHT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::REPEAT_RIGHT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::REPEAT_RIGHT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::REPEAT_RIGHT_POSITION, Pid::POSITION},
    }},
    {Sid::JUMP_POS_ABOVE, Sid::JUMP_POS_ABOVE}};

const TextStyle frame_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::FRAME_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::FRAME_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::FRAME_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::FRAME_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::FRAME_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::FRAME_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::FRAME_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::FRAME_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::FRAME_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::FRAME_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::FRAME_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::FRAME_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::FRAME_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::FRAME_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::FRAME_POSITION, Pid::POSITION},
    }},
    {Sid::FRAME_OFFSET, Sid::FRAME_OFFSET}};

const TextStyle text_line_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::TEXT_LINE_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::TEXT_LINE_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::TEXT_LINE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::TEXT_LINE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::TEXT_LINE_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::TEXT_LINE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::TEXT_LINE_TEXT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::TEXT_LINE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::TEXT_LINE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::TEXT_LINE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::TEXT_LINE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::TEXT_LINE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::TEXT_LINE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::TEXT_LINE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::TEXT_LINE_POSITION, Pid::BEGIN_TEXT_POSITION},
    }},
    {Sid::TEXT_LINE_POS_ABOVE, Sid::TEXT_LINE_POS_BELOW}};

const TextStyle system_text_line_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::SYSTEM_TEXT_LINE_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::SYSTEM_TEXT_LINE_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::SYSTEM_TEXT_LINE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::SYSTEM_TEXT_LINE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::SYSTEM_TEXT_LINE_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::SYSTEM_TEXT_LINE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::SYSTEM_TEXT_LINE_TEXT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::SYSTEM_TEXT_LINE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::SYSTEM_TEXT_LINE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::SYSTEM_TEXT_LINE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::SYSTEM_TEXT_LINE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::SYSTEM_TEXT_LINE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::SYSTEM_TEXT_LINE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::SYSTEM_TEXT_LINE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::SYSTEM_TEXT_LINE_POSITION, Pid::POSITION},
    }},
    {Sid::SYSTEM_TEXT_LINE_POS_ABOVE, Sid::SYSTEM_TEXT_LINE_POS_BELOW}};

const TextStyle note_line_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::NOTE_LINE_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::NOTE_LINE_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::NOTE_LINE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::NOTE_LINE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::NOTE_LINE_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::NOTE_LINE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::NOTE_LINE_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::NOTE_LINE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::NOTE_LINE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::NOTE_LINE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::NOTE_LINE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::NOTE_LINE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::NOTE_LINE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::NOTE_LINE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::NOTE_LINE_POSITION, Pid::POSITION},
    }},
    {Sid::NOTE_LINE_OFFSET, Sid::NOTE_LINE_OFFSET}};

const TextStyle glissando_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::GLISSANDO_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::GLISSANDO_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::GLISSANDO_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::GLISSANDO_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::GLISSANDO_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::GLISSANDO_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::GLISSANDO_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::GLISSANDO_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::GLISSANDO_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::GLISSANDO_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::GLISSANDO_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::GLISSANDO_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::GLISSANDO_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::GLISSANDO_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::GLISSANDO_POSITION, Pid::POSITION},
    }},
    {Sid::GLISSANDO_OFFSET, Sid::GLISSANDO_OFFSET}};

const TextStyle ottava_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::OTTAVA_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::OTTAVA_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::OTTAVA_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::OTTAVA_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::OTTAVA_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::OTTAVA_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::OTTAVA_TEXT_ALIGN_ABOVE, Pid::BEGIN_TEXT_ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::OTTAVA_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::OTTAVA_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::OTTAVA_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::OTTAVA_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::OTTAVA_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::OTTAVA_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::OTTAVA_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::OTTAVA_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::OTTAVA_POSITION, Pid::BEGIN_TEXT_POSITION},
    }},
    {Sid::OTTAVA_POS_ABOVE, Sid::OTTAVA_POS_BELOW}};

const TextStyle volta_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::VOLTA_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::VOLTA_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::VOLTA_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::VOLTA_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::VOLTA_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::VOLTA_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::VOLTA_ALIGN, Pid::BEGIN_TEXT_ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::VOLTA_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::VOLTA_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::VOLTA_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::VOLTA_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::VOLTA_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::VOLTA_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::VOLTA_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::VOLTA_POSITION, Pid::BEGIN_TEXT_POSITION},
    }},
    {Sid::VOLTA_OFFSET, Sid::VOLTA_OFFSET}};

const TextStyle pedal_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::PEDAL_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::PEDAL_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::PEDAL_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::PEDAL_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::PEDAL_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::PEDAL_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::PEDAL_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::PEDAL_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::PEDAL_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::PEDAL_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::PEDAL_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::PEDAL_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::PEDAL_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::PEDAL_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::PEDAL_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::PEDAL_POSITION, Pid::BEGIN_TEXT_POSITION},
    }},
    {Sid::PEDAL_OFFSET, Sid::PEDAL_OFFSET}};

const TextStyle let_ring_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::LET_RING_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::LET_RING_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::LET_RING_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::LET_RING_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::LET_RING_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::LET_RING_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::LET_RING_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::LET_RING_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::LET_RING_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::LET_RING_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::LET_RING_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::LET_RING_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::LET_RING_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::LET_RING_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::LET_RING_POSITION, Pid::BEGIN_TEXT_POSITION},
    }},
    {Sid::LET_RING_OFFSET, Sid::LET_RING_OFFSET}};

const TextStyle whammy_bar_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::WHAMMY_BAR_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::WHAMMY_BAR_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::WHAMMY_BAR_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::WHAMMY_BAR_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::WHAMMY_BAR_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::WHAMMY_BAR_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::WHAMMY_BAR_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::WHAMMY_BAR_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::WHAMMY_BAR_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::WHAMMY_BAR_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::WHAMMY_BAR_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::WHAMMY_BAR_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::WHAMMY_BAR_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::WHAMMY_BAR_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::WHAMMY_BAR_POSITION, Pid::BEGIN_TEXT_POSITION},
    }},
    {Sid::WHAMMY_BAR_OFFSET, Sid::WHAMMY_BAR_OFFSET}};

const TextStyle palm_mute_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::PALM_MUTE_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::PALM_MUTE_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::PALM_MUTE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::PALM_MUTE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::PALM_MUTE_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::PALM_MUTE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::PALM_MUTE_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::PALM_MUTE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::PALM_MUTE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::PALM_MUTE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::PALM_MUTE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::PALM_MUTE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::PALM_MUTE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::PALM_MUTE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::PALM_MUTE_POSITION, Pid::BEGIN_TEXT_POSITION},
    }},
    {Sid::PALM_MUTE_OFFSET, Sid::PALM_MUTE_OFFSET}};

const TextStyle hairpin_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::HAIRPIN_FONT_FACE, Pid::BEGIN_FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::HAIRPIN_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::HAIRPIN_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::HAIRPIN_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::HAIRPIN_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::HAIRPIN_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::HAIRPIN_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::HAIRPIN_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::HAIRPIN_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::HAIRPIN_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::HAIRPIN_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::HAIRPIN_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::HAIRPIN_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::HAIRPIN_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::HAIRPIN_POSITION, Pid::BEGIN_TEXT_POSITION},
    }},
    {Sid::HAIRPIN_OFFSET, Sid::HAIRPIN_OFFSET}};

const TextStyle bend_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::BEND_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::BEND_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::BEND_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::BEND_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::BEND_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::BEND_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::BEND_ALIGN, Pid::BEGIN_TEXT_ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::BEND_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::BEND_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::BEND_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::BEND_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::BEND_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::BEND_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::BEND_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::BEND_POSITION, Pid::POSITION},
    }},
    {Sid::BEND_OFFSET, Sid::BEND_OFFSET}};

const TextStyle header_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::HEADER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::HEADER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::HEADER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::HEADER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::HEADER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::HEADER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::HEADER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::HEADER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::HEADER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::HEADER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::HEADER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::HEADER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::HEADER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::HEADER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::HEADER_POSITION, Pid::POSITION},
    }},
    {Sid::HEADER_OFFSET, Sid::HEADER_OFFSET}};

const TextStyle footer_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::FOOTER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::FOOTER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::FOOTER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::FOOTER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::FOOTER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::FOOTER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::FOOTER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::FOOTER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::FOOTER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::FOOTER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::FOOTER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::FOOTER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::FOOTER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::FOOTER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::FOOTER_POSITION, Pid::POSITION},
    }},
    {Sid::FOOTER_OFFSET, Sid::FOOTER_OFFSET}};

const TextStyle copyright_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::COPYRIGHT_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::COPYRIGHT_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::COPYRIGHT_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::COPYRIGHT_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::COPYRIGHT_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::COPYRIGHT_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::COPYRIGHT_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::COPYRIGHT_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::COPYRIGHT_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::COPYRIGHT_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::COPYRIGHT_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::COPYRIGHT_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::COPYRIGHT_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::COPYRIGHT_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::COPYRIGHT_POSITION, Pid::POSITION},
    }},
    {Sid::COPYRIGHT_OFFSET, Sid::COPYRIGHT_OFFSET}};

const TextStyle page_number_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::PAGE_NUMBER_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::PAGE_NUMBER_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::PAGE_NUMBER_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::PAGE_NUMBER_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::PAGE_NUMBER_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::PAGE_NUMBER_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::PAGE_NUMBER_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::PAGE_NUMBER_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::PAGE_NUMBER_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::PAGE_NUMBER_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::PAGE_NUMBER_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::PAGE_NUMBER_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::PAGE_NUMBER_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::PAGE_NUMBER_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::PAGE_NUMBER_POSITION, Pid::POSITION},
    }},
    {Sid::PAGE_NUMBER_OFFSET, Sid::PAGE_NUMBER_OFFSET}};

const TextStyle instrument_change_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::INSTRUMENT_CHANGE_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::INSTRUMENT_CHANGE_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::INSTRUMENT_CHANGE_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::INSTRUMENT_CHANGE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::INSTRUMENT_CHANGE_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::INSTRUMENT_CHANGE_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::INSTRUMENT_CHANGE_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::INSTRUMENT_CHANGE_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::INSTRUMENT_CHANGE_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::INSTRUMENT_CHANGE_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::INSTRUMENT_CHANGE_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::INSTRUMENT_CHANGE_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::INSTRUMENT_CHANGE_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::INSTRUMENT_CHANGE_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::INSTRUMENT_CHANGE_POSITION, Pid::POSITION},
    }},
    {Sid::INSTRUMENT_CHANGE_OFFSET, Sid::INSTRUMENT_CHANGE_OFFSET}};

const TextStyle sticking_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::STICKING_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::STICKING_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::STICKING_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::STICKING_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::STICKING_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::STICKING_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::STICKING_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::STICKING_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::STICKING_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::STICKING_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::STICKING_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::STICKING_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::STICKING_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::STICKING_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::STICKING_POSITION, Pid::POSITION},
    }},
    {Sid::STICKING_POS_ABOVE, Sid::STICKING_POS_BELOW}};

const TextStyle user1_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER1_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER1_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER1_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER1_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER1_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER1_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER1_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER1_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER1_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER1_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER1_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER1_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER1_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER1_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER1_POSITION, Pid::POSITION},
    }},
    {Sid::USER1_OFFSET, Sid::USER1_OFFSET}};

const TextStyle user2_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER2_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER2_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER2_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER2_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER2_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER2_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER2_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER2_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER2_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER2_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER2_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER2_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER2_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER2_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER2_POSITION, Pid::POSITION},
    }},
    {Sid::USER2_OFFSET, Sid::USER2_OFFSET}};

const TextStyle user3_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER3_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER3_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER3_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER3_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER3_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER3_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER3_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER3_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER3_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER3_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER3_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER3_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER3_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER3_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER3_POSITION, Pid::POSITION},
    }},
    {Sid::USER3_OFFSET, Sid::USER3_OFFSET}};

const TextStyle user4_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER4_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER4_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER4_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER4_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER4_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER4_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER4_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER4_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER4_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER4_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER4_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER4_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER4_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER4_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER4_POSITION, Pid::POSITION},
    }},
    {Sid::USER4_OFFSET, Sid::USER4_OFFSET}};

const TextStyle user5_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER5_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER5_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER5_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER5_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER5_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER5_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER5_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER5_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER5_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER5_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER5_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER5_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER5_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER5_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER5_POSITION, Pid::POSITION},
    }},
    {Sid::USER5_OFFSET, Sid::USER5_OFFSET}};

const TextStyle user6_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER6_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER6_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER6_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER6_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER6_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER6_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER6_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER6_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER6_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER6_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER6_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER6_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER6_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER6_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER6_POSITION, Pid::POSITION},
    }},
    {Sid::USER6_OFFSET, Sid::USER6_OFFSET}};

const TextStyle user7_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER7_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER7_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER7_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER7_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER7_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER7_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER7_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER7_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER7_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER7_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER7_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER7_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER7_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER7_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER7_POSITION, Pid::POSITION},
    }},
    {Sid::USER7_OFFSET, Sid::USER7_OFFSET}};

const TextStyle user8_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER8_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER8_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER8_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER8_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER8_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER8_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER8_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER8_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER8_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER8_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER8_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER8_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER8_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER8_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER8_POSITION, Pid::POSITION},
    }},
    {Sid::USER8_OFFSET, Sid::USER8_OFFSET}};

const TextStyle user9_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER9_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER9_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER9_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER9_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER9_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER9_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER9_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER9_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER9_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER9_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER9_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER9_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER9_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER9_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER9_POSITION, Pid::POSITION},
    }},
    {Sid::USER9_OFFSET, Sid::USER9_OFFSET}};

const TextStyle user10_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER10_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER10_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER10_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER10_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER10_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER10_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER10_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER10_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER10_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER10_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER10_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER10_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER10_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER10_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER10_POSITION, Pid::POSITION},
    }},
    {Sid::USER10_OFFSET, Sid::USER10_OFFSET}};

const TextStyle user11_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER11_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER11_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER11_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER11_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER11_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER11_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER11_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER11_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER11_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER11_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER11_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER11_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER11_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER11_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER11_POSITION, Pid::POSITION},
    }},
    {Sid::USER11_OFFSET, Sid::USER11_OFFSET}};

const TextStyle user12_text_style{
    {{
        {TextStylePropertyType::FONT_FACE, Sid::USER12_FONT_FACE, Pid::FONT_FACE},
        {TextStylePropertyType::FONT_SIZE, Sid::USER12_FONT_SIZE, Pid::FONT_SIZE},
        {TextStylePropertyType::LINE_SPACING, Sid::USER12_LINE_SPACING, Pid::TEXT_LINE_SPACING},
        {TextStylePropertyType::SIZE_SPATIUM_DEPENDENT, Sid::USER12_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
        {TextStylePropertyType::FONT_STYLE, Sid::USER12_FONT_STYLE, Pid::FONT_STYLE},
        {TextStylePropertyType::COLOR, Sid::USER12_COLOR, Pid::COLOR},
        {TextStylePropertyType::TEXT_ALIGN, Sid::USER12_ALIGN, Pid::ALIGN},
        {TextStylePropertyType::FRAME_TYPE, Sid::USER12_FRAME_TYPE, Pid::FRAME_TYPE},
        {TextStylePropertyType::FRAME_PADDING, Sid::USER12_FRAME_PADDING, Pid::FRAME_PADDING},
        {TextStylePropertyType::FRAME_WIDTH, Sid::USER12_FRAME_WIDTH, Pid::FRAME_WIDTH},
        {TextStylePropertyType::FRAME_ROUND, Sid::USER12_FRAME_ROUND, Pid::FRAME_ROUND},
        {TextStylePropertyType::FRAME_BORDER_COLOR, Sid::USER12_FRAME_FG_COLOR, Pid::FRAME_FG_COLOR},
        {TextStylePropertyType::FRAME_FILL_COLOR, Sid::USER12_FRAME_BG_COLOR, Pid::FRAME_BG_COLOR},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SCALE, Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::MUSICAL_SYMBOLS_SCALE},
        {TextStylePropertyType::MUSICAL_SYMBOLS_SIZE, Sid::USER12_MUSICAL_SYMBOL_SIZE, Pid::MUSIC_SYMBOL_SIZE},
        {TextStylePropertyType::POSITION, Sid::USER12_POSITION, Pid::POSITION},
    }},
    {Sid::USER12_OFFSET, Sid::USER12_OFFSET}};

const TextStyle *text_style(TextStyleType idx) {
    switch (idx) {
        case TextStyleType::DEFAULT: return &::mu::engraving::default_text_style;

        case TextStyleType::TITLE: return &::mu::engraving::title_text_style;
        case TextStyleType::SUBTITLE: return &::mu::engraving::sub_title_text_style;
        case TextStyleType::COMPOSER: return &::mu::engraving::composer_text_style;
        case TextStyleType::LYRICIST: return &::mu::engraving::lyricist_text_style;
        case TextStyleType::TRANSLATOR: return &::mu::engraving::translator_text_style;
        case TextStyleType::FRAME: return &::mu::engraving::frame_text_style;
        case TextStyleType::INSTRUMENT_EXCERPT: return &::mu::engraving::part_instrument_text_style;
        case TextStyleType::INSTRUMENT_LONG: return &::mu::engraving::long_instrument_text_style;
        case TextStyleType::INSTRUMENT_SHORT: return &::mu::engraving::short_instrument_text_style;
        case TextStyleType::INSTRUMENT_CHANGE: return &::mu::engraving::instrument_change_text_style;
        case TextStyleType::GROUP_BRACKET: return &::mu::engraving::group_bracket_text_style;
        case TextStyleType::HEADER: return &::mu::engraving::header_text_style;
        case TextStyleType::FOOTER: return &::mu::engraving::footer_text_style;
        case TextStyleType::COPYRIGHT: return &::mu::engraving::copyright_text_style;
        case TextStyleType::PAGE_NUMBER: return &::mu::engraving::page_number_text_style;

        case TextStyleType::MEASURE_NUMBER: return &::mu::engraving::measure_number_text_style;
        case TextStyleType::MEASURE_NUMBER_ALTERNATE: return &::mu::engraving::measure_number_alternate_text_style;
        case TextStyleType::MMREST_RANGE: return &::mu::engraving::mm_rest_range_text_style;

        case TextStyleType::TEMPO: return &::mu::engraving::tempo_text_style;
        case TextStyleType::TEMPO_CHANGE: return &::mu::engraving::tempo_change_text_style;
        case TextStyleType::METRONOME: return &::mu::engraving::metronome_text_style;
        case TextStyleType::REPEAT_PLAY_COUNT: return &::mu::engraving::repeat_play_count_text_style;
        case TextStyleType::REPEAT_LEFT: return &::mu::engraving::repeat_left_text_style;
        case TextStyleType::REPEAT_RIGHT: return &::mu::engraving::repeat_right_text_style;
        case TextStyleType::REHEARSAL_MARK: return &::mu::engraving::rehearsal_mark_text_style;
        case TextStyleType::SYSTEM: return &::mu::engraving::system_text_style;

        case TextStyleType::STAFF: return &::mu::engraving::staff_text_style;
        case TextStyleType::STAVE_SHARING: return &::mu::engraving::stave_sharing_label_style;
        case TextStyleType::EXPRESSION: return &::mu::engraving::expression_text_style;
        case TextStyleType::DYNAMICS: return &::mu::engraving::dynamics_text_style;
        case TextStyleType::HAIRPIN: return &::mu::engraving::hairpin_text_style;
        case TextStyleType::LYRICS_ODD: return &::mu::engraving::lyrics_odd_text_style;
        case TextStyleType::LYRICS_EVEN: return &::mu::engraving::lyrics_even_text_style;
        case TextStyleType::HARMONY_A: return &::mu::engraving::chord_symbol_text_style_a;
        case TextStyleType::HARMONY_B: return &::mu::engraving::chord_symbol_text_style_b;
        case TextStyleType::HARMONY_ROMAN: return &::mu::engraving::roman_numeral_text_style;
        case TextStyleType::HARMONY_NASHVILLE: return &::mu::engraving::nashville_number_text_style;

        case TextStyleType::TUPLET: return &::mu::engraving::tuplet_text_style;
        case TextStyleType::ARTICULATION: return &::mu::engraving::articulation_text_style;
        case TextStyleType::STICKING: return &::mu::engraving::sticking_text_style;
        case TextStyleType::FINGERING: return &::mu::engraving::fingering_text_style;
        case TextStyleType::TAB_FRET_NUMBER: return &::mu::engraving::tab_fret_number_text_style;
        case TextStyleType::LH_GUITAR_FINGERING: return &::mu::engraving::lh_guitar_fingering_text_style;
        case TextStyleType::RH_GUITAR_FINGERING: return &::mu::engraving::rh_guitar_fingering_text_style;
        case TextStyleType::HAMMER_ON_PULL_OFF: return &::mu::engraving::hammer_on_pull_off_text_style;
        case TextStyleType::STRING_NUMBER: return &::mu::engraving::string_number_text_style;
        case TextStyleType::STRING_TUNINGS: return &::mu::engraving::string_tunings_style; // todo
        case TextStyleType::FRET_DIAGRAM_FINGERING: return &::mu::engraving::fret_diagram_fingering_style;
        case TextStyleType::FRET_DIAGRAM_FRET_NUMBER: return &::mu::engraving::fret_diagram_fret_number_style;
        case TextStyleType::HARP_PEDAL_DIAGRAM: return &::mu::engraving::harp_pedal_diagram_text_style;
        case TextStyleType::HARP_PEDAL_TEXT_DIAGRAM: return &::mu::engraving::harp_pedal_text_diagram_text_style;

        case TextStyleType::TEXTLINE: return &::mu::engraving::text_line_text_style;
        case TextStyleType::SYSTEM_TEXTLINE: return &::mu::engraving::system_text_line_text_style;
        case TextStyleType::NOTELINE: return &::mu::engraving::note_line_text_style;
        case TextStyleType::VOLTA: return &::mu::engraving::volta_text_style;
        case TextStyleType::OTTAVA: return &::mu::engraving::ottava_text_style;
        case TextStyleType::GLISSANDO: return &::mu::engraving::glissando_text_style;
        case TextStyleType::PEDAL: return &::mu::engraving::pedal_text_style;
        case TextStyleType::BEND: return &::mu::engraving::bend_text_style;
        case TextStyleType::LET_RING: return &::mu::engraving::let_ring_text_style;
        case TextStyleType::WHAMMY_BAR: return &::mu::engraving::whammy_bar_text_style;
        case TextStyleType::PALM_MUTE: return &::mu::engraving::palm_mute_text_style;

        case TextStyleType::USER1: return &::mu::engraving::user1_text_style;
        case TextStyleType::USER2: return &::mu::engraving::user2_text_style;
        case TextStyleType::USER3: return &::mu::engraving::user3_text_style;
        case TextStyleType::USER4: return &::mu::engraving::user4_text_style;
        case TextStyleType::USER5: return &::mu::engraving::user5_text_style;
        case TextStyleType::USER6: return &::mu::engraving::user6_text_style;
        case TextStyleType::USER7: return &::mu::engraving::user7_text_style;
        case TextStyleType::USER8: return &::mu::engraving::user8_text_style;
        case TextStyleType::USER9: return &::mu::engraving::user9_text_style;
        case TextStyleType::USER10: return &::mu::engraving::user10_text_style;
        case TextStyleType::USER11: return &::mu::engraving::user11_text_style;
        case TextStyleType::USER12: return &::mu::engraving::user12_text_style;

        case TextStyleType::TEXT_TYPES: return nullptr;
        case TextStyleType::IGNORED_TYPES: return nullptr;
    }

    UNREACHABLE;
    return nullptr;
}

static std::vector<TextStyleType> all_text_styles_ids;

static const std::vector<TextStyleType> primary_text_styles_ids = {
    TextStyleType::TITLE,
    TextStyleType::SUBTITLE,
    TextStyleType::COMPOSER,
    TextStyleType::LYRICIST,
    TextStyleType::TRANSLATOR,
    TextStyleType::FRAME,
    TextStyleType::INSTRUMENT_EXCERPT,
    TextStyleType::INSTRUMENT_CHANGE,
    TextStyleType::HEADER,
    TextStyleType::FOOTER,
    TextStyleType::COPYRIGHT,
    TextStyleType::PAGE_NUMBER,
    TextStyleType::MEASURE_NUMBER,
    TextStyleType::MEASURE_NUMBER_ALTERNATE,
    TextStyleType::MMREST_RANGE,
    TextStyleType::TEMPO,
    TextStyleType::REPEAT_PLAY_COUNT,
    TextStyleType::REPEAT_LEFT,
    TextStyleType::REPEAT_RIGHT,
    TextStyleType::REHEARSAL_MARK,
    TextStyleType::SYSTEM,
    TextStyleType::STAFF,
    TextStyleType::STAVE_SHARING,
    TextStyleType::EXPRESSION,
    TextStyleType::DYNAMICS,
    TextStyleType::HAIRPIN,
    TextStyleType::LYRICS_ODD,
    TextStyleType::LYRICS_EVEN,
    TextStyleType::HARMONY_A,
    TextStyleType::HARMONY_B,
    TextStyleType::HARMONY_ROMAN,
    TextStyleType::HARMONY_NASHVILLE,
    TextStyleType::STICKING,
    TextStyleType::USER1,
    TextStyleType::USER2,
    TextStyleType::USER3,
    TextStyleType::USER4,
    TextStyleType::USER5,
    TextStyleType::USER6,
    TextStyleType::USER7,
    TextStyleType::USER8,
    TextStyleType::USER9,
    TextStyleType::USER10,
    TextStyleType::USER11,
    TextStyleType::USER12,
};

//---------------------------------------------------------
//   allTextStyles
//---------------------------------------------------------

const std::vector<TextStyleType> &all_text_styles() {
    if (all_text_styles_ids.empty()) {
        all_text_styles_ids.reserve(int(TextStyleType::TEXT_TYPES));
        for (int t = int(TextStyleType::DEFAULT) + 1; t < int(TextStyleType::TEXT_TYPES); ++t) {
            all_text_styles_ids.push_back(TextStyleType(t));
        }
    }
    return all_text_styles_ids;
}

//---------------------------------------------------------
//   primaryTextStyles
//---------------------------------------------------------

const std::vector<TextStyleType> &primary_text_styles() { return ::mu::engraving::primary_text_styles_ids; }

static std::vector<TextStyleType> editable_text_styles_ids;

const std::vector<TextStyleType> &editable_text_styles() {
    if (editable_text_styles_ids.empty()) {
        editable_text_styles_ids = all_text_styles();
        muse::remove(editable_text_styles_ids, TextStyleType::DYNAMICS);
    }
    return editable_text_styles_ids;
}
} // namespace mu::engraving
