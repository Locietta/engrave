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
#include "mscloader.h"

#include <memory>

#include "../dom/excerpt.h"
#include "../dom/imageStore.h"
#include "../dom/masterscore.h"
#include "../engravingerrors.h"
#include "../types/types.h"
#include "compat/compatutils.h"
#include "compat/readstyle.h"
#include "engraving/automation/automationdata.h"
#include "engraving/automation/internal/automationrw.h"
#include "global/io/buffer.h"
#include "global/types/retval.h"
#include "inoutdata.h"
#include "log.h"
#include "rwregister.h"
#include "xmlreader.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace mu::engraving;
using namespace mu::engraving::rw;

static RetVal<IReaderPtr> make_reader(int version, bool ignore_version_error) {
    if (version < 460) {
        return RetVal<IReaderPtr>(make_ret(Err::FILE_TOO_OLD));
    }
    if (!ignore_version_error) {
        if (version > Constants::msc_version) {
            return RetVal<IReaderPtr>(make_ret(Err::FILE_TOO_NEW));
        }

        if (version < 114) {
            return RetVal<IReaderPtr>(make_ret(Err::FILE_TOO_OLD));
        }

        if (version == 300) {
            return RetVal<IReaderPtr>(make_ret(Err::FILE_OLD300_FORMAT));
        }
    }

    return RetVal<IReaderPtr>::make_ok(RWRegister::reader(version));
}

Ret MscLoader::load_mscz(MasterScore *master_score, const MscReader &msc_reader, rw::ReadInOutData *in_out, bool ignore_version_error) {
    TRACEFUNC;

    using namespace mu::engraving;

    IF_ASSERT_FAILED(msc_reader.is_opened()) { return make_ret(Err::FILE_OPEN_ERROR, msc_reader.params().file_path); }

    ScoreLoad sl;

    if (msc_reader.is_container()) {
        // Read style
        {
            ByteArray style_data = msc_reader.read_style_file();
            if (!style_data.empty()) {
                auto buf = Buffer::opened(IODevice::READ_ONLY, &style_data);
                master_score->style().read(&buf);
                if (in_out) {
                    in_out->original_spatium = master_score->style().spatium();
                }
            }
        }

        // Read ChordList
        {
            bool chord_list_ok = false;
            ByteArray chord_list_data = msc_reader.read_chord_list_file();
            if (!chord_list_data.empty()) {
                auto buf = Buffer::opened(IODevice::READ_ONLY, &chord_list_data);

                chord_list_ok = master_score->chord_list()->read(&buf);
            }

            master_score->chord_list()->set_custom_chord_list(chord_list_ok);

            if (!chord_list_ok) {
                // See also ReadChordListHook::validate()
                MStyle &style = master_score->style();
                ChordList *chord_list = master_score->chord_list();

                bool custom = style.style_v(Sid::CHORD_STYLE).value<ChordStylePreset>() == ChordStylePreset::CUSTOM;
                chord_list->set_custom_chord_list(custom);

                // Ensure that `checkChordList` loads the default chord list
                chord_list->unload();
            }
        }

        // Read images
        {
            if (!MScore::no_images) {
                std::vector<String> images = msc_reader.image_file_names();
                for (const String &name : images) {
                    image_store.add(name.to_std_string(), msc_reader.read_image_file(name));
                }
            }
        }
    }

    ReadInOutData master_read_out_data;
    if (!in_out) {
        in_out = &master_read_out_data;
    }

    Ret ret = muse::make_ok();

    // Read score
    {
        ByteArray score_data = msc_reader.read_score_file();
        String doc_name = master_score->file_info()->file_name().to_string();

        compat::ReadStyleHook style_hook(master_score, score_data, doc_name);

        XmlReader xml(score_data);
        xml.set_doc_name(doc_name);

        ret = read_master_score(master_score, xml, ignore_version_error, in_out, &style_hook);
    }

    // Read excerpts
    if (ret && master_score->msc_version() >= 400 && msc_reader.is_container()) {
        std::vector<String> excerpt_file_names = msc_reader.excerpt_file_names();
        for (const String &excerpt_file_name : excerpt_file_names) {
            Score *part_score = master_score->create_score();

            compat::ReadStyleHook::setup_default_style(part_score);

            Excerpt *ex = new Excerpt(master_score);
            ex->set_excerpt_score(part_score);
            ex->set_file_name(excerpt_file_name);

            ByteArray excerpt_style_data = msc_reader.read_excerpt_style_file(excerpt_file_name);
            auto excerpt_style_buf = Buffer::opened(IODevice::READ_ONLY, &excerpt_style_data);
            part_score->style().read(&excerpt_style_buf);

            ByteArray excerpt_data = msc_reader.read_excerpt_file(excerpt_file_name);

            XmlReader xml(excerpt_data);
            xml.set_doc_name(excerpt_file_name);

            ReadInOutData part_read_in_data;
            part_read_in_data.links = in_out->links;

            RetVal<IReaderPtr> reader = make_reader(master_score->msc_version(), ignore_version_error);
            if (!reader.ret) {
                ret = reader.ret;
                break;
            }

            ret = reader.val->read_score_file(part_score, xml, &part_read_in_data);
            if (!ret) {
                break;
            }

            Excerpt::link_measures(part_score, master_score);

            if (ex->name().empty()) {
                // If no excerpt name tag was found while reading, try the "partName" meta tag
                const String name_from_meta = part_score->meta_tag(u"partName");

                if (name_from_meta.empty()) {
                    // If that's also empty, fall back to the filename
                    ex->set_name(excerpt_file_name, /*saveAndNotify=*/false);
                } else {
                    ex->set_name(name_from_meta, /*saveAndNotify=*/false);
                }
            }

            master_score->add_excerpt(ex);
        }
    }

    // Compatibility conversions
    // NOTE: must be done after all score and parts have been read
    compat::CompatUtils::do_compatibility_conversions(master_score);

    // Read automation
    {
        ByteArray ba = msc_reader.read_automation_json_file();
        AutomationDataPtr automation_data = std::make_shared<AutomationData>();
        AutomationRW::read(*automation_data, ba);
        master_score->set_automation_data(automation_data);
    }

    return ret;
}

Ret MscLoader::read_master_score(MasterScore *score, XmlReader &e, bool ignore_version_error, ReadInOutData *out,
                                 compat::ReadStyleHook *style_hook) {
    while (e.read_next_start_element()) {
        if (e.name() == "museScore") {
            const String version = e.attribute("version");
            const StringList sl = version.split(u'.');
            if (sl.size() < 2) return make_ret(Err::FILE_BAD_FORMAT);
            score->set_msc_version(sl[0].to_int() * 100 + sl[1].to_int());
            if (score->msc_version() < 460) {
                return Ret(static_cast<int>(Err::FILE_TOO_OLD),
                           "written by MuseScore " + version.to_std_string() + "; scores from before MuseScore 4.6 are not supported");
            }

            RetVal<IReaderPtr> reader = make_reader(score->msc_version(), ignore_version_error);
            if (!reader.ret) {
                return reader.ret;
            }

            IF_ASSERT_FAILED(reader.val) { return make_ret(Err::FILE_UNKNOWN_ERROR); }

            //! NOTE We need to achieve that the default style corresponds to the version in which the score is created.
            //! The values that the user changed will be written on over (only they are stored in the `mscz` file)
            //! For version 4.0 (400), this does not need to be done,
            //! because starting from version 4.0 the entire style is stored in a file,
            //! respectively, the entire style will be loaded, which was when the score was created.
            if (style_hook && score->msc_version() < 400) {
                style_hook->setup_default_style();
            }

            if (score->msc_version() >= 400) {
                //! NOTE: make sure we have a chord list
                //! Load the default chord list otherwise
                score->check_chord_list();
            }

            Ret ret = reader.val->read_score_file(score, e, out);

            score->set_excerpts_changed(false);

            return ret;
        } else {
            e.unknown();
        }
    }

    return make_ret(Err::FILE_CRITICALLY_CORRUPTED, e.error_string());
}
