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
#include "mscsaver.h"

#include "dom/excerpt.h"
#include "dom/imageStore.h"
#include "dom/masterscore.h"
#include "engraving/automation/internal/automationrw.h"
#include "global/io/buffer.h"
#include "inoutdata.h"
#include "log.h"
#include "rwregister.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace mu::engraving;
using namespace mu::engraving::rw;

bool MscSaver::write_mscz(MasterScore *score, MscWriter &msc_writer, const write::WriteContext *ctx) {
    TRACEFUNC;

    IF_ASSERT_FAILED(msc_writer.is_opened()) { return false; }

    // Write style of MasterScore
    {
        //! NOTE The style is writing to a separate file only for the master score.
        //! At the moment, the style for the parts is still writing to the score file.
        ByteArray style_data;
        auto style_buf = Buffer::opened(IODevice::WRITE_ONLY, &style_data);
        score->style().write(&style_buf);
        msc_writer.write_style_file(style_data);
    }

    WriteInOutData master_write_out_data(score);

    if (ctx) {
        master_write_out_data.ctx = *ctx;
    }

    // Write MasterScore
    {
        ByteArray score_data;
        auto score_buf = Buffer::opened(IODevice::READ_WRITE, &score_data);

        RWRegister::writer()->write_score(score, &score_buf, &master_write_out_data);

        msc_writer.write_score_file(score_data);
    }

    // Write Excerpts
    {
        if (!ctx || !ctx->should_write_range()) {
            const std::vector<Excerpt *> &excerpts = score->excerpts();

            struct ExcerptData {
                String file_name;
                ByteArray style_data;
                ByteArray score_data;
            };

            auto serialize_excerpt = [master_write_out_data, score](Excerpt *excerpt, size_t excerpt_index) -> ExcerptData {
                Score *part_score = excerpt->excerpt_score();
                IF_ASSERT_FAILED(part_score && part_score != score) { return ExcerptData(); }

                excerpt->update_file_name(excerpt_index);

                ExcerptData data;
                data.file_name = excerpt->file_name();

                auto style_buf = Buffer::opened(IODevice::WRITE_ONLY, &data.style_data);
                part_score->style().write(&style_buf);

                WriteInOutData write_out_data = master_write_out_data;
                auto score_buf = Buffer::opened(IODevice::READ_WRITE, &data.score_data);
                RWRegister::writer()->write_score(part_score, &score_buf, &write_out_data);

                return data;
            };

            for (size_t excerpt_index = 0; excerpt_index < excerpts.size(); ++excerpt_index) {
                Excerpt *excerpt = excerpts.at(excerpt_index);

                ExcerptData data = serialize_excerpt(excerpt, excerpt_index);
                msc_writer.add_excerpt_style_file(data.file_name, data.style_data);
                msc_writer.add_excerpt_file(data.file_name, data.score_data);
            }
        }
    }

    // Write ChordList
    {
        ChordList *chord_list = score->chord_list();
        if (chord_list->custom_chord_list() && !chord_list->empty()) {
            ByteArray chl_data;
            auto chl_buf = Buffer::opened(IODevice::WRITE_ONLY, &chl_data);
            chord_list->write(&chl_buf);
            msc_writer.write_chord_list_file(chl_data);
        }
    }

    // Write images
    {
        for (ImageStoreItem *ip : image_store) {
            if (!ip->is_used(score)) {
                continue;
            }
            ByteArray data = ip->buffer();
            msc_writer.add_image_file(String::from_std_string(ip->hash_name()), data);
        }
    }

    // Write automation
    {
        if (score->automation_data()) {
            ByteArray data = AutomationRW::write(*score->automation_data(), false /*writeGenerated*/);
            if (!data.empty()) {
                msc_writer.write_automation_json_file(data);
            }
        }
    }

    return true;
}
