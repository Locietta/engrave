#include "score_io.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "engine_context.h"
#include "engrave/src/io/midi_import_adapter.h"
#include "engrave/src/io/score_imports.h"
#include "engrave/src/render/score_render.h"
#include "engrave/src/score/score_excerpts.h"
#include "engrave/src/score/score_metadata.h"
#include "engrave/src/score/score_positions.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/engravingproject.h"
#include "engraving/infrastructure/localfileinfoprovider.h"
#include "engraving/rw/inoutdata.h"
#include "engraving/rw/rwregister.h"
#include "global/io/buffer.h"
#include "global/io/writebudget.h"
#include "global/log.h"
#include "global/serialization/textstream.h"
#include "global/serialization/xmlstreamreader.h"
#include "global/serialization/zipreader.h"
#include "global/serialization/zipwriter.h"
#include "importexport/midi/internal/midiexport/exportmidi.h"
#include "importexport/musicxml/internal/export/exportmusicxml.h"
#include "importexport/musicxml/internal/import/importmusicxml.h"

namespace music {
using namespace muse;
using namespace mu;

namespace {

struct CaptureOutputBuffer final : io::Buffer {
    explicit CaptureOutputBuffer(ByteArray *data) : Buffer(data) {}
protected:
    bool resize_data(size_t size) override {
        if (io::active_write_budget && !io::active_write_budget->output_size(size)) return false;
        return Buffer::resize_data(size);
    }
};

void check(const Ret &result) {
    if (!result) throw std::runtime_error(result.to_string());
}

std::string bytes(const ByteArray &data) { return {reinterpret_cast<const char *>(data.const_data()), data.size()}; }

ByteArray byte_array(const std::string &data) { return ByteArray(data.data(), data.size()); }

void check_zip(ZipReader &zip) {
    uint64_t total = 0;
    const auto entries = zip.file_info_list();
    if (entries.empty() || entries.size() > 4096) throw std::invalid_argument("Invalid archive entry count");
    for (const auto &entry : entries) {
        if (entry.size > 64 * 1024 * 1024 || total + entry.size > 128 * 1024 * 1024)
            throw std::invalid_argument("Uncompressed archive exceeds size limit");
        total += entry.size;
    }
    if (zip.has_error()) throw std::invalid_argument("Invalid archive");
}

void finish_load(engraving::EngravingProject &project) {
    auto *score = project.master_score();
    score->connect_ties();
    // 5.0 moved the post-load MIDI wiring out of EngravingProject::setupMasterScore
    // into the MSCX reader (rw/read500/read500.cpp:307-315), so a score that did not
    // come from MSCX keeps an empty MasterScore::midiMapping(): every InstrChannel
    // still has channel() == -1 (dom/instrument.cpp:319) and
    // MasterScore::playbackChannel() indexes midiMapping_ with it
    // (dom/masterscore.h:185), i.e. reads 72*(channel()+1) bytes out of bounds.
    // Run the same three steps, in the same order, where every reader converges.
    for (auto *part : score->parts()) part->update_harmony_channels(false);
    score->rebuild_midi_mapping();
    score->update_channel();
    check(project.setup_master_score(true));
    // MusicXML import can call Part::setMidiProgram after the MIDI mapping was
    // built, leaving playback channels on the template program until an MSCZ
    // reload rebuilds them. Sync now so playback is stable across reloads.
    for (auto *part : score->parts()) {
        for (const auto &pair : part->instruments()) {
            for (auto *channel : pair.second->channel()) {
                if (auto *playback = score->playback_channel(channel)) {
                    playback->set_program(channel->program());
                    playback->set_bank(channel->bank());
                }
            }
        }
    }
    score->set_layout_all();
    engraving::normalize_document(*score, {.mode = engraving::NormalizeMode::LOAD});
    score->update();
    score->init_automation();
}
} // namespace

void apply_score_versions(engraving::MasterScore *score, const std::string &version, int revision, int msc_version) {
    if (!score) throw std::invalid_argument("Score version restore requires a master score");
    score->set_mscore_version(String::from_std_string(version));
    score->set_mscore_revision(revision);
    score->set_msc_version(msc_version);
}

void copy_score_versions(engraving::MasterScore *source, engraving::MasterScore *target) {
    if (!source || !target) throw std::invalid_argument("Score version copy requires two master scores");
    apply_score_versions(target, source->mscore_version().to_std_string(), source->mscore_revision(), source->msc_version());
}

ScoreProject load_native_score(const EngineContext &context, const std::string &input, const std::string &format,
                               bool transaction_snapshot) {
    if (input.empty() || (!transaction_snapshot && input.size() > 8 * 1024 * 1024))
        throw std::invalid_argument("Input must contain 1..8388608 bytes");
    // Like NotationProject::doImport (project/internal/notationproject.cpp:304) and
    // importMusicXmlfromBuffer, the whole reading + setup phase is a load: layout
    // inside it runs outside a transaction and must not push undo commands or warn.
    engraving::ScoreLoad score_load;
    auto project = engraving::EngravingProject::create(context.engraving());
    project->set_file_info_provider(std::make_shared<engraving::LocalFileInfoProvider>("Untitled.mscz"));
    ByteArray data = byte_array(input);
    if (format == "mxl" || format == "mscz") {
        io::Buffer buffer(&data);
        buffer.open(io::IODevice::READ_ONLY);
        ZipReader zip(&buffer);
        if (!transaction_snapshot) check_zip(zip);
        if (format == "mxl") {
            XmlStreamReader xml(zip.file_data("META-INF/container.xml"));
            std::string root;
            while (!xml.at_end()) {
                xml.read_next();
                if (root.empty() && xml.is_start_element() && xml.name() == "rootfile") {
                    root = xml.attribute("full-path").to_std_string();
                }
            }
            if (xml.error() != XmlStreamReader::NO_ERROR) throw std::invalid_argument("Malformed MXL container.xml");
            if (root.empty()) throw std::invalid_argument("MXL has no rootfile");
            data = zip.file_data(root);
            if (zip.has_error() || data.empty() || data.size() > 8 * 1024 * 1024)
                throw std::invalid_argument("MXL rootfile must contain 1..8388608 bytes");
        }
    }
    if (format == "mscz" || format == "mscx") {
        ByteArray archive;
        if (format == "mscx") {
            io::Buffer out(&archive);
            out.open(io::IODevice::WRITE_ONLY);
            ZipWriter zip(&out);
            zip.add_file("score.mscx", data);
            zip.close();
            data = std::move(archive);
        }
        io::Buffer buffer(&data);
        engraving::MscReader::Params params;
        params.device = &buffer;
        params.mode = engraving::MscIoMode::ZIP;
        engraving::MscReader reader(params);
        check(reader.open());
        if (!reader.image_file_names().empty())
            throw std::invalid_argument("Embedded score images are not yet supported by the native backend");
        engraving::rw::ReadInOutData inout;
        inout.transaction_snapshot = transaction_snapshot;
        const auto loaded = project->load_mscz(reader, &inout, false);
        if (loaded.code() == static_cast<int>(engraving::Err::FILE_TOO_OLD)) throw UnsupportedScoreVersion(loaded.to_string());
        check(loaded);
    } else if (format == "musicxml" || format == "xml" || format == "mxl") {
        XmlStreamReader scan(data);
        while (!scan.at_end()) {
            scan.read_next();
            if (scan.is_start_element() && scan.name() == "image")
                throw std::invalid_argument("Embedded score images are not yet supported by the native backend");
        }
        if (scan.error() != XmlStreamReader::NO_ERROR) throw std::invalid_argument("Malformed MusicXML");
        if (iex::musicxml::import_music_xmlfrom_buffer(project->master_score(), u"input.musicxml", data, context.music_xml()) !=
            engraving::Err::NO_ERROR)
            throw std::runtime_error("MusicXML import failed");
    } else if (format == "mid" || format == "midi" || format == "kar") {
        if (import_midi_score(project->master_score(), input) != engraving::Err::NO_ERROR) throw std::runtime_error("MIDI import failed");
    } else if (format == "gp" || format == "gp3" || format == "gp4" || format == "gp5" || format == "gp6" || format == "gp7" ||
               format == "gpx" || format == "gtp" || format == "ptb") {
        import_guitarpro_score(project->master_score(), input);
    } else
        throw std::invalid_argument("Unsupported score input format");
    finish_load(*project);
    return project;
}

ScoreProject load_native_score_dir(const EngineContext &context, const std::string &mscx_path) {
    engraving::ScoreLoad score_load;
    auto project = engraving::EngravingProject::create(context.engraving());
    project->set_file_info_provider(std::make_shared<engraving::LocalFileInfoProvider>("Untitled.mscz"));
    engraving::MscReader::Params params;
    params.file_path = io::PathT(mscx_path);
    params.mode = engraving::MscIoMode::DIR;
    engraving::MscReader reader(params);
    check(reader.open());
    engraving::rw::ReadInOutData inout;
    check(project->load_mscz(reader, &inout, false));
    finish_load(*project);
    return project;
}

std::string export_native_score(const EngineContext &context, const ScoreProject &project, const std::string &format, int page,
                                const std::string &soundfont) {
    ExportOptions options;
    options.page = page;
    return export_native_score(context, project, format, options, soundfont);
}

std::string export_native_score(const EngineContext &context, const ScoreProject &project, const std::string &format,
                                const ExportOptions &options, [[maybe_unused]] const std::string &soundfont) {
    struct RestorePrecision {
        bool previous = TextStream::round_trip_doubles;

        ~RestorePrecision() { TextStream::round_trip_doubles = previous; }
    } restore_precision;

    if (options.transaction_snapshot) TextStream::round_trip_doubles = true;
    // Audio formats are rendered by the worker's MuseScore engine; the
    // SoundFont parameter is retained for source compatibility.
    auto *score = options.transaction_snapshot ? project->master_score() : select_score(project->master_score(), options.excerpt_id);
    if (!options.transaction_snapshot && format != "mscx" && format != "mscz" && format != "midi" && format != "metadata") {
        score->ensure_layout();
    }
    if (format == "metadata") return music::dump_json(score_metadata(score));
    if (format == "measurePositions" || format == "segmentPositions")
        return music::dump_json(score_positions(score, format == "segmentPositions"));
    if (format == "svg" || format == "png" || format == "pdf") return render_score(context, score, format, options);

    ByteArray data;
    CaptureOutputBuffer buffer(&data);
    buffer.open(io::IODevice::WRITE_ONLY);
    bool success = false;
    if (format == "musicxml")
        success = iex::musicxml::save_xml(score, &buffer, context.music_xml());
    else if (format == "mxl")
        success = iex::musicxml::save_mxl(score, &buffer, context.music_xml());
    else if (format == "mscz" || format == "mscx") {
        if (format == "mscx" && options.standalone_score && options.excerpt_id == -1) {
            // An MSCX has no separate style file; embed the master's style in
            // the score itself, without changing the legacy archive writer.
            engraving::rw::WriteInOutData inout(score);
            inout.ctx.set_inline_style(true);
            inout.ctx.set_inline_excerpts(true);
            success = engraving::rw::RWRegister::writer()->write_score(score, &buffer, &inout);
        } else if (format == "mscx" && options.standalone_score && options.excerpt_id >= 0 && io::active_write_budget) {
            // The archive path serializes this same score XML, compresses it,
            // then inflates it again. Write that XML directly so its output
            // cap applies before the uncompressed result is allocated.
            auto effective_tags = project->master_score()->meta_tags();
            for (const auto &[key, value] : score->meta_tags()) effective_tags.insert_or_assign(key, value);
            engraving::rw::WriteInOutData inout(score);
            inout.ctx.set_standalone(true);
            inout.ctx.set_inline_style(true);
            inout.ctx.metadata_override = &effective_tags;
            success = engraving::rw::RWRegister::writer()->write_score(score, &buffer, &inout);
        } else {
            engraving::MscWriter::Params params;
            params.device = &buffer;
            params.main_file_name = u"score.mscx";
            engraving::MscWriter writer(params);
            check(writer.open());
            if (options.excerpt_id == -1) {
                success = project->write_mscz(writer);
            } else {
                // Same upstream writer/style logic as MscSaver::exportPart, without a Qt thumbnail.
                auto effective_tags = project->master_score()->meta_tags();
                for (const auto &[key, value] : score->meta_tags()) effective_tags.insert_or_assign(key, value);
                ByteArray style_data, score_data;
                io::Buffer style_buffer(&style_data), score_buffer(&score_data);
                style_buffer.open(io::IODevice::WRITE_ONLY);
                score_buffer.open(io::IODevice::WRITE_ONLY);
                score->style().write(&style_buffer);
                engraving::rw::WriteInOutData inout(score);
                inout.ctx.set_standalone(options.standalone_score);
                // Only the score file of this archive is kept for an MSCX.
                inout.ctx.set_inline_style(format == "mscx" && options.standalone_score);
                inout.ctx.metadata_override = &effective_tags;
                success = engraving::rw::RWRegister::writer()->write_score(score, &score_buffer, &inout);
                writer.write_style_file(style_data);
                writer.write_score_file(score_data);
            }
            writer.close();
            success = success && !writer.has_error();
            if (format == "mscx" && success) {
                buffer.close();
                ByteArray mscz_data = std::move(data);
                data = ByteArray();
                io::Buffer mscz_buf(&mscz_data);
                engraving::MscReader::Params rparams;
                rparams.device = &mscz_buf;
                rparams.mode = engraving::MscIoMode::ZIP;
                engraving::MscReader reader(rparams);
                check(reader.open());
                data = reader.read_score_file();
                reader.close();
                success = !data.empty();
            }
        }
    } else if (format == "midi") {
        // The legacy renderer selects a repeat view. Keep that per-export choice
        // from changing subsequent semantic reads or audio requests.
        struct RestoreRepeats {
            engraving::MasterScore *master;
            bool expanded;

            ~RestoreRepeats() { master->set_expand_repeats(expanded); }
        } restore{score->master_score(), score->master_score()->expand_repeats()};

        iex::midi::ExportMidi writer(score);
        success = writer.write(&buffer, options.midi_expand_repeats, options.midi_export_rpns);
    } else
        throw std::invalid_argument("Unsupported export format");
    buffer.close();
    if (!success || data.empty()) throw std::runtime_error("Score export failed");
    return bytes(data);
}
} // namespace music
