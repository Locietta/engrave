/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "headerfooterlayout.h"

#include "dom/factory.h"
#include "dom/masterscore.h"
#include "dom/mscore.h"
#include "dom/page.h"
#include "dom/text.h"
#include "engraving/infrastructure/localfileinfoprovider.h"
#include "tlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void HeaderFooterLayout::layout_header_footer(LayoutContext &ctx, Page *page) {
    const bool is_page_mode = ctx.conf().is_mode(LayoutMode::PAGE) || ctx.conf().is_mode(LayoutMode::FLOAT);
    if (!is_page_mode) {
        return;
    }

    const int n = page->get_display_page_number();

    if (must_show_headers_for_page(ctx, page)) {
        const bool odd = (n & 1) || !ctx.conf().style_b(Sid::HEADER_ODD_EVEN);
        create_update_header_text(ctx, page, 0, ctx.conf().style_st(odd ? Sid::ODD_HEADER_L : Sid::EVEN_HEADER_L));
        create_update_header_text(ctx, page, 1, ctx.conf().style_st(odd ? Sid::ODD_HEADER_C : Sid::EVEN_HEADER_C));
        create_update_header_text(ctx, page, 2, ctx.conf().style_st(odd ? Sid::ODD_HEADER_R : Sid::EVEN_HEADER_R));
    } else {
        for (int area = 0; area < ::mu::engraving::max_headers; ++area) {
            remove_header_text(page, area);
        }
    }

    if (must_show_footers_for_page(ctx, page)) {
        const bool odd = (n & 1) || !ctx.conf().style_b(Sid::FOOTER_ODD_EVEN);
        create_update_footer_text(ctx, page, 0, ctx.conf().style_st(odd ? Sid::ODD_FOOTER_L : Sid::EVEN_FOOTER_L));
        create_update_footer_text(ctx, page, 1, ctx.conf().style_st(odd ? Sid::ODD_FOOTER_C : Sid::EVEN_FOOTER_C));
        create_update_footer_text(ctx, page, 2, ctx.conf().style_st(odd ? Sid::ODD_FOOTER_R : Sid::EVEN_FOOTER_R));
    } else {
        for (int area = 0; area < ::mu::engraving::max_footers; ++area) {
            remove_footer_text(page, area);
        }
    }
}

void HeaderFooterLayout::create_update_header_text(LayoutContext &ctx, Page *page, int area, const String &s) {
    if (s.empty()) {
        remove_header_text(page, area);
        return;
    }

    Text *text = page->header_text(area);
    if (!text) {
        text = Factory::create_text(page, TextStyleType::HEADER);
        text->set_flag(ElementFlag::MOVABLE, false);
        text->set_flag(ElementFlag::GENERATED, true); // set to disable editing
        text->set_layout_to_parent_width(true);
        page->set_header_text(area, text);

        switch (area) {
            case 0:
                text->set_align({AlignH::LEFT, AlignV::TOP});
                text->set_position(AlignH::LEFT);
                break;
            case 1:
                text->set_align({AlignH::HCENTER, AlignV::TOP});
                text->set_position(AlignH::HCENTER);
                break;
            case 2:
                text->set_align({AlignH::RIGHT, AlignV::TOP});
                text->set_position(AlignH::RIGHT);
                break;
            default: break;
        }
    }

    if (!update_header_footer_text(ctx, page, text, s)) {
        remove_header_text(page, area);
    }
}

void HeaderFooterLayout::create_update_footer_text(LayoutContext &ctx, Page *page, int area, const String &s) {
    if (s.empty()) {
        remove_footer_text(page, area);
        return;
    }

    Text *text = page->footer_text(area);
    if (!text) {
        text = Factory::create_text(page, TextStyleType::FOOTER);
        text->set_flag(ElementFlag::MOVABLE, false);
        text->set_flag(ElementFlag::GENERATED, true); // set to disable editing
        text->set_layout_to_parent_width(true);
        page->set_footer_text(area, text);

        switch (area) {
            case 0:
                text->set_align({AlignH::LEFT, AlignV::BOTTOM});
                text->set_position(AlignH::LEFT);
                break;
            case 1:
                text->set_align({AlignH::HCENTER, AlignV::BOTTOM});
                text->set_position(AlignH::HCENTER);
                break;
            case 2:
                text->set_align({AlignH::RIGHT, AlignV::BOTTOM});
                text->set_position(AlignH::RIGHT);
                break;
            default: break;
        }
    }

    if (!update_header_footer_text(ctx, page, text, s)) {
        remove_footer_text(page, area);
    }
}

bool HeaderFooterLayout::update_header_footer_text(LayoutContext &ctx, Page *page, Text *text, const String &s) {
    // Hack: we can't use toXmlEscaped on the entire string because this would erase any manual XML
    // formatting, but we do want to be able to use a plain '&' in favour of XML character entities ...
    // ... also, opening less-than characters are escaped if they are followed by a space or a digit,
    // to avoid confusion with XML tags, but not if they are followed by a letter
    String escaped;
    for (size_t i = 0, n = s.size(); i < n; ++i) {
        const Char c = s.at(i);
        if (c == '&') {
            escaped += u"&amp;";
            continue;
        } else if (c == '<' && (i + 1 < n) && (s.at(i + 1).is_space() || s.at(i + 1).is_digit())) {
            escaped += u"&lt;";
            continue;
        }
        escaped += c;
    }

    // first formatting pass - apply TextStyleType formatting and any manual XML formatting
    text->set_xml_text(escaped);
    text->create_blocks();

    // second formatting pass - replace macros and apply their unique formatting (if any)
    int empty_blocks = 0;
    std::vector<TextBlock> new_blocks;
    for (const TextBlock &old_block : text->ldata()->blocks) {
        Text *dummy_text = Factory::create_text(ctx.dom().dummy_parent(), text->text_style_type());
        dummy_text->mutldata()->blocks = {replace_text_macros(ctx, page, old_block)};
        dummy_text->gen_text();
        dummy_text->create_blocks();
        if (dummy_text->xml_text().is_empty()) {
            empty_blocks++; // count empty blocks to remove them later if there is only 1 empty block
        }
        new_blocks.insert(new_blocks.end(), dummy_text->ldata()->blocks.cbegin(), dummy_text->ldata()->blocks.cend());
        delete dummy_text;
    }

    if (new_blocks.empty() || (new_blocks.size() == 1 && empty_blocks == 1)) {
        // If there is no block, or the only block is empty, tell the caller that the text should be removed.
        // This way, empty lines are preserved if and only if there also is some text anywhere in the header/footer.
        return false;
    }

    text->mutldata()->blocks = new_blocks;
    TLayout::layout_text(text, text->mutldata());

    return true;
}

void HeaderFooterLayout::remove_header_text(Page *page, int area) {
    if (Text *t = page->header_text(area)) {
        page->set_header_text(area, nullptr);
        delete t;
    }
}

void HeaderFooterLayout::remove_footer_text(Page *page, int area) {
    if (Text *t = page->footer_text(area)) {
        page->set_footer_text(area, nullptr);
        delete t;
    }
}

//---------------------------------------------------------
//   replaceTextMacros
//   (keep in sync with toolTipHeaderFooter in EditStyle::EditStyle())
//    $p          - page number, except on first page
//    $P          - page number, on all pages
//    $N          - page number, if there is more than one
//    $n          - number of pages
//    $i          - part name, except on first page
//    $I          - part name, on all pages
//    $f          - file name
//    $F          - file path+name
//    $d          - current date
//    $D          - creation date
//    $m          - last modification time
//    $M          - last modification date
//    $C          - copyright, on first page only
//    $c          - copyright, on all pages
//    $v          - MuseScore version the score was last saved with
//    $r          - MuseScore revision the score was last saved with
//    $$          - the $ sign itself
//    $:tag:      - any metadata tag
//
//       tags always defined (see Score::init())):
//       copyright
//       creationDate
//       movementNumber
//       movementTitle
//       platform
//       source
//       workNumber
//       workTitle
//---------------------------------------------------------

TextBlock HeaderFooterLayout::replace_text_macros(LayoutContext &ctx, const Page *page, const TextBlock &tb) {
    std::list<TextFragment> new_fragments;
    for (const TextFragment &tf : tb.fragments()) {
        const CharFormat default_format = tf.format;
        const String &s = tf.text;

        // if this is the first fragment, or the current fragment has a different format, we need to start a new fragment
        if (new_fragments.size() == 0 || new_fragments.back().format != default_format) {
            new_fragments.emplace_back(TextFragment());
            new_fragments.back().format = default_format;
        }

        for (size_t i = 0, n = s.size(); i < n; ++i) {
            Char c = s.at(i);
            if (c == '$' && (i < (n - 1))) {
                Char nc = s.at(i + 1);
                switch (nc.to_ascii()) {
                    case 'p': // not on first page of the project nor page number 1
                        if (!page->page_number() || page->get_display_page_number() <= 1) {
                            break;
                        }
                        [[fallthrough]];
                    case 'N': // on page 1 only if there are multiple pages
                        if ((static_cast<int>(page->score()->npages()) + page->score()->page_number_offset()) <= 1) {
                            ctx.mut_state().set_must_recompute_headers_footers(true);
                            break;
                        }
                        [[fallthrough]];
                    case 'P': // on all pages
                    {
                        const int no = page->get_display_page_number();
                        if (no > 0) {
                            const String page_number_string = String::number(no);
                            const CharFormat page_number_format = format_for_macro(ctx, String('$' + nc));
                            append_formatted_string(new_fragments, page_number_string, default_format, page_number_format);
                        }
                    } break;
                    case 'n': {
                        ctx.mut_state().set_must_recompute_headers_footers(true);
                        const int no = static_cast<int>(page->score()->npages()) + page->score()->page_number_offset();
                        const String number_of_pages_string = String::number(no);
                        const CharFormat page_number_format = format_for_macro(ctx, String('$' + nc));
                        append_formatted_string(new_fragments, number_of_pages_string, default_format, page_number_format);
                    } break;
                    case 'i': // not on first page
                        if (!page->page_number()) {
                            break;
                        }
                        [[fallthrough]];
                    case 'I': new_fragments.back().text += page->score()->meta_tag(u"partName"); break;
                    case 'f': new_fragments.back().text += page->score()->master_score()->file_info()->file_name(false).to_string(); break;
                    case 'F': new_fragments.back().text += page->score()->master_score()->file_info()->path().to_string(); break;
                    case 'd': new_fragments.back().text += muse::Date::current_date().to_string(muse::DateFormat::ISO_DATE); break;
                    case 'D': {
                        String creation_date = page->score()->meta_tag(u"creationDate");
                        if (creation_date.is_empty()) {
                            new_fragments.back().text +=
                                page->score()->master_score()->file_info()->birth_time().date().to_string(muse::DateFormat::ISO_DATE);
                        } else {
                            new_fragments.back().text +=
                                muse::Date::from_string_iso_format(creation_date).to_string(muse::DateFormat::ISO_DATE);
                        }
                    } break;
                    case 'm': {
                        LocalFileInfoProviderPtr file_info = page->score()->master_score()->file_info();
                        if (file_info->is_newly_created()) {
                            new_fragments.back().text += String(u"HH:mm:ss");
                        } else {
                            new_fragments.back().text += file_info->last_modified().time().to_string(muse::DateFormat::ISO_DATE);
                        }
                    } break;
                    case 'M': {
                        LocalFileInfoProviderPtr file_info = page->score()->master_score()->file_info();
                        if (file_info->is_newly_created()) {
                            new_fragments.back().text += String(u"YYYY-MM-DD");
                        } else {
                            new_fragments.back().text += file_info->last_modified().date().to_string(muse::DateFormat::ISO_DATE);
                        }
                    } break;
                    case 'C': // only on first page
                        if (page->page_number()) {
                            break;
                        }
                        [[fallthrough]];
                    case 'c': {
                        const String copyright_string = page->score()->meta_tag(u"copyright");
                        const CharFormat copyright_format = format_for_macro(ctx, String('$' + nc));
                        append_formatted_string(new_fragments, copyright_string, default_format, copyright_format);
                    } break;
                    // Live macros describe source provenance, independent of edit/layout timing.
                    case 'v': new_fragments.back().text += page->score()->mscore_version(); break;
                    case 'r': {
                        const int rev = page->score()->mscore_revision();
                        new_fragments.back().text += String::number(rev, rev > 99999 ? 16 : 10);
                    } break;
                    case '$': new_fragments.back().text += '$'; break;
                    case ':': {
                        String tag;
                        size_t k = i + 2;
                        for (; k < n; ++k) {
                            if (s.at(k) == u':') {
                                break;
                            }
                            tag += s.at(k);
                        }
                        if (k != n) { // found ':' ?
                            if (tag == u"copyright") {
                                const String copyright_string = page->score()->meta_tag(tag);
                                const CharFormat copyright_format = format_for_macro(ctx, String(u"$:" + tag + u":"));
                                append_formatted_string(new_fragments, copyright_string, default_format, copyright_format);
                            } else {
                                new_fragments.back().text += page->score()->meta_tag(tag);
                            }
                            i = k - 1;
                        }
                    } break;
                    default:
                        new_fragments.back().text += '$';
                        new_fragments.back().text += nc;
                        break;
                }
                ++i;
            } else {
                new_fragments.back().text += c;
            }
        }
    }

    TextBlock new_block;
    new_block.fragments() = new_fragments;
    return new_block;
}

CharFormat HeaderFooterLayout::format_for_macro(const LayoutContext &ctx, const String &macro) {
    CharFormat format;
    if (macro == "$c" || macro == "$C" || macro == "$:copyright:") {
        format.set_style(ctx.conf().style_v(Sid::COPYRIGHT_FONT_STYLE).value<FontStyle>());
        format.set_font_size(ctx.conf().style_d(Sid::COPYRIGHT_FONT_SIZE));
        format.set_font_family(ctx.conf().style_st(Sid::COPYRIGHT_FONT_FACE));
    } else if (macro == "$p" || macro == "$P" || macro == "$n" || macro == "$N") {
        format.set_style(ctx.conf().style_v(Sid::PAGE_NUMBER_FONT_STYLE).value<FontStyle>());
        format.set_font_size(ctx.conf().style_d(Sid::PAGE_NUMBER_FONT_SIZE));
        format.set_font_family(ctx.conf().style_st(Sid::PAGE_NUMBER_FONT_FACE));
    }
    return format;
}

void HeaderFooterLayout::append_formatted_string(std::list<TextFragment> &fragments, const String &string, const CharFormat &default_format,
                                                 const CharFormat &new_format) {
    // If the default format equals the format for this macro, we don't need to create a new fragment...
    if (default_format == new_format) {
        fragments.back().text += string;
        return;
    }
    TextFragment new_fragment(string);
    new_fragment.format = new_format;
    fragments.emplace_back(new_fragment);
    fragments.emplace_back(TextFragment());   // Start next fragment
    fragments.back().format = default_format; // reset to default for next fragment
}

double HeaderFooterLayout::header_extension(const LayoutContext &ctx, const Page *page) {
    const bool is_page_mode = ctx.conf().is_mode(LayoutMode::PAGE) || ctx.conf().is_mode(LayoutMode::FLOAT);
    if (!is_page_mode) {
        return 0.0;
    }

    if (must_show_headers_for_page(ctx, page)) {
        double max_height = 0.0;
        for (int area = 0; area < ::mu::engraving::max_headers; ++area) {
            if (Text *text = page->header_text(area)) {
                double h = text->height();
                if (h > max_height) {
                    max_height = h;
                }
            }
        }
        const double offset = ctx.conf().style_v(Sid::HEADER_OFFSET).value<PointF>().y() * ::mu::engraving::dpmm;
        return std::max(0.0, max_height - offset);
    }

    return 0.0;
}

double HeaderFooterLayout::footer_extension(const LayoutContext &ctx, const Page *page) {
    const bool is_page_mode = ctx.conf().is_mode(LayoutMode::PAGE) || ctx.conf().is_mode(LayoutMode::FLOAT);
    if (!is_page_mode) {
        return 0.0;
    }

    if (must_show_footers_for_page(ctx, page)) {
        double max_height = 0.0;
        for (int area = 0; area < ::mu::engraving::max_footers; ++area) {
            if (Text *text = page->footer_text(area)) {
                double h = text->height();
                if (h > max_height) {
                    max_height = h;
                }
            }
        }
        const double offset = ctx.conf().style_v(Sid::FOOTER_OFFSET).value<PointF>().y() * ::mu::engraving::dpmm;
        return std::max(0.0, max_height - offset);
    }
    return 0.0;
}

bool HeaderFooterLayout::contains_timestamp_macros(const String &text) {
    return text.contains(u"$d") || text.contains(u"$D") || text.contains(u"$m") || text.contains(u"$M");
}

bool HeaderFooterLayout::score_has_timestamp_headers_footers(const Score *score) {
    const MStyle &style = score->style();

    if (style.style_b(Sid::SHOW_HEADER)) {
        if (contains_timestamp_macros(style.style_st(Sid::ODD_HEADER_L)) || contains_timestamp_macros(style.style_st(Sid::ODD_HEADER_C)) ||
            contains_timestamp_macros(style.style_st(Sid::ODD_HEADER_R)) || contains_timestamp_macros(style.style_st(Sid::EVEN_HEADER_L)) ||
            contains_timestamp_macros(style.style_st(Sid::EVEN_HEADER_C)) ||
            contains_timestamp_macros(style.style_st(Sid::EVEN_HEADER_R))) {
            return true;
        }
    }

    if (style.style_b(Sid::SHOW_FOOTER)) {
        if (contains_timestamp_macros(style.style_st(Sid::ODD_FOOTER_L)) || contains_timestamp_macros(style.style_st(Sid::ODD_FOOTER_C)) ||
            contains_timestamp_macros(style.style_st(Sid::ODD_FOOTER_R)) || contains_timestamp_macros(style.style_st(Sid::EVEN_FOOTER_L)) ||
            contains_timestamp_macros(style.style_st(Sid::EVEN_FOOTER_C)) ||
            contains_timestamp_macros(style.style_st(Sid::EVEN_FOOTER_R))) {
            return true;
        }
    }

    return false;
}

bool HeaderFooterLayout::must_show_headers_for_page(const LayoutContext &ctx, const Page *page) {
    return ctx.conf().style_b(Sid::SHOW_HEADER) &&
           ((page->page_number() && page->get_display_page_number() != 1) || ctx.conf().style_b(Sid::HEADER_FIRST_PAGE));
}

bool HeaderFooterLayout::must_show_footers_for_page(const LayoutContext &ctx, const Page *page) {
    return ctx.conf().style_b(Sid::SHOW_FOOTER) &&
           ((page->page_number() && page->get_display_page_number() != 1) || ctx.conf().style_b(Sid::FOOTER_FIRST_PAGE));
}
