/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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
#include "textlayout.h"

#include "dom/box.h"
#include "dom/page.h"
#include "dom/staff.h"
#include "dom/textlinebase.h"
#include "log.h"
#include "tlayout.h"
#include "types/typesconv.h"

using namespace mu::engraving::rendering::score;
using namespace mu::engraving;
using namespace muse::draw;

void TextLayout::layout_base_text_base(const TextBase *item, TextBase::LayoutData *ldata) {
    IF_ASSERT_FAILED(item->ownership_parent()) { return; }

    ldata->set_pos(PointF());

    if (item->place_below()) {
        ldata->set_pos_y(item->staff() ? item->staff()->staff_height(item->tick()) : 0.0);
    }

    layout_base_text_base1(item, ldata);
}

void TextLayout::layout_base_text_base(TextBase *item, LayoutContext &) { layout_base_text_base(item, item->mutldata()); }

void TextLayout::layout_base_text_base1(const TextBase *item, TextBase::LayoutData *ldata) {

    if (item->layout_parent() && item->layout_to_parent_width()) {
        LD_CONDITION(item->layout_parent()->ldata()->is_set_bbox());
    }

    if (ldata->layout_invalid) {
        item->create_blocks(ldata);
    }

    double y = 0.0;

    double max_block_width = -DBL_MAX;
    // adjust the bounding box for the text item
    for (size_t i = 0; i < ldata->blocks.size(); ++i) {
        TextBlock &t = ldata->blocks[i];
        layout_text_block(&t, item);
        y += t.line_spacing();
        t.set_y(y);
        max_block_width = std::max(max_block_width, t.bounding_rect().width());
    }

    Shape shape;
    text_horizontal_layout(item, shape, max_block_width, ldata);

    RectF bb = shape.bbox();

    double yoff = 0;
    double h = 0;
    const EngravingItem *parent = item->layout_parent();
    if (parent) {
        if (item->layout_to_parent_width()) {
            if (parent->is_t_box()) {
                // hack: vertical alignment is always TOP
                const_cast<TextBase *>(item)->set_align({item->align().horizontal, AlignV::TOP});
            } else if (parent->is_box()) {
                // consider inner margins of frame
                const Box *b = to_box(parent);
                yoff = b->top_margin() * ::mu::engraving::dpmm;

                if (b->height() < bb.bottom()) {
                    h = b->height() / 2 + bb.height();
                } else {
                    h = b->height() - yoff - b->bottom_margin() * ::mu::engraving::dpmm;
                }
            } else if (parent->is_page()) {
                const Page *p = to_page(parent);
                h = p->height() - p->tm() - p->bm();
                yoff = p->tm();
            } else if (parent->is_measure()) {
            } else {
                h = parent->height();
            }
        }
    } else {
        ldata->set_pos(PointF());
    }

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

    if (!item->is_dynamic() && !(parent && parent->is_box())) {
        compute_text_high_res_shape(item, ldata);
    }

    if (!muse::real_is_null(item->text_angle())) {
        Transform t;
        t.rotate(item->text_angle());
        ldata->set_shape(shape.transform(t));
        ldata->high_res_shape.mut_value().transform(t);
    }

    PointF default_pos = item->default_pos();
    ldata->move(default_pos);
}

void TextLayout::layout_base_text_base1(TextBase *item, const LayoutContext &) { layout_base_text_base1(item, item->mutldata()); }

void TextLayout::compute_text_high_res_shape(const TextBase *item, TextBase::LayoutData *ldata) {
    Shape &shape = ldata->high_res_shape.mut_value();
    shape.clear();
    shape.elements().reserve(item->xml_text().size());

    for (const TextBlock &block : ldata->blocks) {
        double y = block.y();
        for (const TextFragment &fragment : block.fragments()) {
            FontMetrics font_metrics = FontMetrics(fragment.font(item));
            double x = fragment.pos.x();
            size_t text_size = fragment.text.size();
            for (size_t i = 0; i < text_size; ++i) {
                Char character = fragment.text.at(i);
                String text{character};
                if (character.is_high_surrogate() && i + 1 < text_size) {
                    text += fragment.text.at(i + 1);
                    i++;
                }
                RectF character_bounding_rect = font_metrics.tight_bounding_rect(text);
                character_bounding_rect.translate(x, y);
                shape.add(character_bounding_rect, item);
                if (i + 1 < text_size) {
                    x += font_metrics.horizontal_advance(text);
                }
            }
        }
    }

    ldata->high_res_shape = shape;
}

void TextLayout::text_horizontal_layout(const TextBase *item, Shape &shape, double max_block_width, TextBase::LayoutData *ldata) {
    double left_margin = 0.0;
    double layout_width = 0;
    EngravingItem *parent = item->layout_parent();
    if (parent && item->layout_to_parent_width()) {
        layout_width = parent->width();
        switch (parent->type()) {
            case ElementType::HBOX:
            case ElementType::VBOX:
            case ElementType::TBOX: {
                Box *b = to_box(parent);
                layout_width -= ((b->left_margin() + b->right_margin()) * ::mu::engraving::dpmm);
                left_margin = b->left_margin() * ::mu::engraving::dpmm;
            } break;
            case ElementType::PAGE: {
                Page *p = to_page(parent);
                layout_width -= (p->lm() + p->rm());
                left_margin = p->lm();
            } break;
                break;
            default: ASSERT_X("Lay out to parent width only valid for items with page or frame as parent");
        }
    } else if (item->position_relative_to_notehead_rest()) {
        // For items aligned to notehead/rests
        double mag = item->staff() ? item->staff()->staff_mag(item) : 1.0;
        layout_width = item->sym_width(SymId::NOTEHEAD_BLACK) * mag;
    }

    // Position and alignment
    for (size_t i = 0; i < ldata->blocks.size(); ++i) {
        TextBlock &text_block = ldata->blocks[i];
        double x_adj = left_margin - text_block.bounding_rect().left();

        // Set position relative to reference point
        AlignH position = item->position();
        if (position == AlignH::HCENTER) {
            x_adj += (layout_width - max_block_width) * .5;
        } else if (position == AlignH::RIGHT) {
            x_adj += layout_width - max_block_width;
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
        } else if (align_h == AlignH::JUSTIFY) {
            // Don't justify final line
            if (i != ldata->blocks.size() - 1) {
                justify_line(item, &text_block, max_block_width);
            }
        }

        for (TextFragment &fragment : text_block.fragments()) {
            fragment.pos.rx() += x_adj;
        }
        text_block.shape().translate(PointF(x_adj, 0.0));
        shape.add(text_block.shape().translated(PointF(0.0, text_block.y())));
    }
}

void TextLayout::justify_line(const TextBase *item, TextBlock *text_block, double max_block_width) {
    struct SubFragment {
        String text;
        CharFormat format;
        double width;
    };

    std::vector<SubFragment> subfrags;
    for (const TextFragment &f : text_block->fragments()) {
        String current;
        FontMetrics fm(f.font(item));
        for (size_t i = 0; i < f.text.size(); ++i) {
            Char c = f.text.at(i);
            current += c;
            if (c.is_space()) {
                double w = fm.width(current);
                subfrags.push_back({current, f.format, w});
                current.clear();
            }
        }
        if (!current.is_empty()) {
            double w = fm.width(current);
            subfrags.push_back({current, f.format, w});
        }
    }

    int num_spaces = 0;
    double text_width = 0;
    for (const SubFragment &sf : subfrags) {
        num_spaces++;
        text_width += sf.width; // accumulate text width
    }
    num_spaces--;
    double space_to_fill = max_block_width - text_width;
    if (num_spaces > 0 && space_to_fill > 0) {
        text_block->fragments().clear();
        double extra_spacing = space_to_fill / num_spaces;
        double x = 0.0;
        for (const SubFragment &sf : subfrags) {
            TextFragment frag;
            frag.text = sf.text;
            frag.format = sf.format;
            frag.pos.rx() = x;
            text_block->fragments().push_back(frag);
            x += sf.width + extra_spacing;
        }
    }
}

void TextLayout::layout_text_block(TextBlock *item, const TextBase *t) {
    std::list<TextFragment> &fragments = item->fragments();
    Shape &shape = item->shape();
    shape.clear();
    double x = 0.0;
    double line_spacing = 0.0;

    if (fragments.empty()) {
        FontMetrics fm = t->font_metrics();
        shape.add(RectF(0.0, -fm.ascent(), 1.0, fm.ascent()), t);
        line_spacing = fm.line_spacing();
    } else if (fragments.size() == 1 && fragments.front().text.is_empty()) {
        auto fi = fragments.begin();
        TextFragment &f = *fi;
        f.pos.set_x(x);
        Font font = f.font(t);
        substitute_music_font(font, f.calculated_font_size(t));
        FontMetrics fm(font);
        if (f.format.valign() != VerticalAlignment::ALIGN_NORMAL) {
            double voffset = fm.x_height() / ::mu::engraving::subscript_size; // use original height
            if (f.format.valign() == VerticalAlignment::ALIGN_SUB_SCRIPT) {
                voffset *= ::mu::engraving::subscript_offset;
            } else {
                voffset *= ::mu::engraving::superscript_offset;
            }

            f.pos.set_y(voffset);
        } else {
            f.pos.set_y(0.0);
        }

        RectF temp(0.0, -fm.ascent(), 1.0, fm.ascent());
        shape.add(temp, t);
        line_spacing = std::max(line_spacing, fm.line_spacing());
    } else {
        const auto fi_last = --fragments.end();
        for (auto fi = fragments.begin(); fi != fragments.end(); ++fi) {
            TextFragment &f = *fi;
            f.pos.set_x(x);
            Font fragment_font = f.font(t);
            FontMetrics fm(fragment_font);
            if (f.format.valign() != VerticalAlignment::ALIGN_NORMAL) {
                double voffset = fm.x_height() / ::mu::engraving::subscript_size; // use original height
                if (f.format.valign() == VerticalAlignment::ALIGN_SUB_SCRIPT) {
                    voffset *= ::mu::engraving::subscript_offset;
                } else {
                    voffset *= ::mu::engraving::superscript_offset;
                }
                f.pos.set_y(voffset);
            } else {
                f.pos.set_y(0.0);
            }

            // Optimization: don't calculate character position
            // for the next fragment if there is no next fragment
            if (fi != fi_last) {
                const double w = fm.width(f.text);
                x += w;
            }

            double y_offset = music_symbol_base_line_adjust(item, t, f, fi);
            f.pos.ry() -= y_offset;

            RectF text_b_rect = fm.tight_bounding_rect(f.text).translated(f.pos);
            bool use_dynamic_sym_shape = fragment_font.type() == Font::Type::MUSIC_SYMBOL && t->is_dynamic();
            if (use_dynamic_sym_shape) {
                const Dynamic *dyn = to_dynamic(t);
                SymId sym_id = TConv::sym_id(dyn->dynamic_type());
                if (sym_id != SymId::NO_SYM) {
                    shape.add(dyn->sym_shape_with_cutouts(sym_id).translated(f.pos));
                } else {
                    shape.add(text_b_rect, t);
                }
            } else {
                shape.add(text_b_rect, t);
            }
            if (fragment_font.type() == Font::Type::MUSIC_SYMBOL || fragment_font.type() == Font::Type::MUSIC_SYMBOL_TEXT) {
                // SEMI-HACK: Music fonts can have huge linespacing because of tall symbols, so instead of using the
                // font linespacing value we just use the height of the individual fragment with some added margin

                line_spacing = std::max(line_spacing, 1.25 * (shape.bbox().height() - shape.bbox().bottom()) + y_offset);
            } else {
                line_spacing = std::max(line_spacing, fm.line_spacing());
            }
        }
    }

    // Apply style/custom line spacing
    line_spacing *= t->text_line_spacing();
    item->set_line_spacing(line_spacing);
}

void TextLayout::substitute_music_font(Font &font, double size) {
    if (font.type() == Font::Type::MUSIC_SYMBOL) {
        String text_font_id(font.family().id() + String(u" Text"));
        font.set_family(text_font_id, Font::Type::MUSIC_SYMBOL_TEXT);
        font.set_point_size_f(size);
    } else if (font.type() == Font::Type::MUSIC_SYMBOL_TEXT) {
        font.set_point_size_f(size);
    }
}

double TextLayout::music_symbol_base_line_adjust(const TextBlock *block, const TextBase *t, const TextFragment &f,
                                                 const std::list<TextFragment>::iterator fi) {
    Font fragment_font = f.font(t);
    FontMetrics fm(fragment_font);
    const bool adjust_symbol = fragment_font.type() == Font::Type::MUSIC_SYMBOL_TEXT && t->is_marker();
    if (!adjust_symbol) {
        return 0.0;
    }

    // Align the x-height of the coda symbol to half the x-height of the surrounding text
    Font ref_font;
    if (block->fragments().size() == 1) {
        ref_font = t->font();
    } else {
        TextFragment &ref_fragment = fi != block->fragments().begin() ? *(std::prev(fi)) : *(std::next(fi));
        ref_font = ref_fragment.font(t);
    }
    FontMetrics ref_fm(ref_font);

    const double middle = (fm.tight_bounding_rect(f.text).height() / 2) - fm.tight_bounding_rect(f.text).bottom();
    const double ref_x_height = ref_fm.cap_height() / 2;
    return ref_x_height - middle;
}
