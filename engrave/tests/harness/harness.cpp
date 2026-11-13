#include "engrave/tests/harness/harness.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <new>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "engrave/src/api/handles.h"
#include "engrave/src/api/pid_support.h"
#include "engrave/src/api/state.h"
#include "engrave/src/api/value_codec.h"
#include "engrave/src/score/score_positions.h"
#include "engrave/src/score/score_semantic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/engravingobject.h"
#include "engraving/dom/excerpt.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/select.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/infrastructure/eidregister.h"
#include "global/io/buffer.h"
#include "global/serialization/zipreader.h"

namespace engrave::harness {
namespace {
using mu::engraving::EngravingObject;
using mu::engraving::MasterScore;
using mu::engraving::MScore;
using mu::engraving::Score;

std::map<std::string, std::string> mscz_files(const std::vector<std::byte> &bytes) {
    muse::ByteArray data(reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size());
    muse::io::Buffer buffer(&data);
    buffer.open(muse::io::IODevice::READ_ONLY);
    muse::ZipReader zip(&buffer);
    std::map<std::string, std::string> files;
    for (const auto &info : zip.file_info_list()) {
        if (!info.is_file) continue;
        const muse::ByteArray file = zip.file_data(info.file_path.to_std_string());
        files.emplace(info.file_path.to_std_string(), std::string(reinterpret_cast<const char *>(file.const_data()), file.size()));
    }
    return files;
}

std::string externals(MasterScore &master) {
    std::string text;
    for (const auto &mapping : master.midi_mapping()) {
        const auto &parts = master.parts();
        const auto part = std::ranges::find(parts, mapping.part());
        text += std::format("midi part={} port={} channel={}\n", part - parts.begin(), int(mapping.port()), int(mapping.channel()));
    }
    for (const Score *score : master.score_list()) {
        for (const auto &[key, value] : score->meta_tags()) text += std::format("tag {}={}\n", key.to_std_string(), value.to_std_string());
    }
    text += std::format("selection none={} input track={} note entry={}\n", master.selection().is_none(),
                        static_cast<long long>(master.input_state().track()), master.input_state().note_entry_mode());
    return text;
}

std::string first_difference(std::string_view a, std::string_view b) {
    const auto [left, right] = std::ranges::mismatch(a, b);
    const std::size_t at = static_cast<std::size_t>(left - a.begin());
    const std::size_t from = at > 40 ? at - 40 : 0;
    return std::format("at {}: '{}' vs '{}'", at, a.substr(from, 80), b.substr(from, 80));
}

std::optional<std::pair<std::size_t, std::size_t>> layout_mismatch(const std::string &a, const std::string &b) {
    const auto number = [](char c) { return (c >= '0' && c <= '9') || c == '-' || c == '.'; };
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() && j < b.size()) {
        if (number(a[i]) && number(b[j])) {
            char *end_a = nullptr;
            char *end_b = nullptr;
            const double x = std::strtod(a.c_str() + i, &end_a);
            const double y = std::strtod(b.c_str() + j, &end_b);
            if (std::abs(x - y) > std::max(1.0, 1e-5 * std::abs(x))) return std::pair(i, j);
            i = static_cast<std::size_t>(end_a - a.c_str());
            j = static_cast<std::size_t>(end_b - b.c_str());
        } else if (a[i] != b[j]) {
            return std::pair(i, j);
        } else {
            ++i;
            ++j;
        }
    }
    if (i != a.size() || j != b.size()) return std::pair(i, j);
    return std::nullopt;
}

bool same_layout(const std::vector<std::string> &before, const std::vector<std::string> &after) {
    if (before.size() != after.size()) return false;
    for (std::size_t n = 0; n < before.size(); ++n) {
        if (layout_mismatch(before[n], after[n])) return false;
    }
    return true;
}

std::string inv(Document &document) {
    MasterScore &master = master_of(document);
    std::unordered_set<const EngravingObject *> objects;
    for (EngravingObject *object : document_objects(master)) objects.insert(object);
    for (const auto &[eid, object] : master.eid_register()->entries()) {
        if (object->eid() != eid) return eid.to_std_string() + ": the reverse entry disagrees";
        if (id_class(*object) == IdClass::PERSISTED && !objects.contains(object) && !in_score(master, object))
            return eid.to_std_string() + " names an element that left the document";
    }
    for (const LiveId &entry : live_ids(master)) {
        if (master.eid_register()->item_from_eid(to_engine(entry.id)) != entry.object)
            return to_string(entry.id) + " names another element";
    }
    for (const EngravingObject *object : objects) {
        if (takes_id(*object) && !object->eid().is_valid()) return std::string(object->type_name()) + " has no id";
    }
    return {};
}

std::vector<OpCase> &cases() {
    static std::vector<OpCase> all;
    return all;
}
} // namespace

Fingerprint fingerprint(Document &document) {
    MasterScore &master = master_of(document);
    Fingerprint print;
    for (Score *score : master.score_list()) score->ensure_layout();
    const auto saved = document.export_score({.format = Format::MSCZ});
    if (!saved) throw std::runtime_error("fingerprint: the save failed: " + saved.error().message);
    print.files = mscz_files(*saved);
    print.semantic = music::dump_json(music::semantic_dump(&master), 1);
    for (const LiveId &entry : live_ids(master)) {
        print.ids.push_back(std::format("{} {} {}/{}/{}", to_string(entry.id), entry.object->type_name(), entry.placement.score,
                                        entry.placement.tick, entry.placement.track));
    }
    std::ranges::sort(print.ids);
    print.revision = document.revision();
    print.engine_error = static_cast<int>(MScore::error);
    int index = 0;
    for (Score *score : master.score_list()) {
        if (score->npages() > 0)
            print.layout.push_back(
                std::format("score {} pages {} {}", index, score->npages(), music::dump_json(music::score_positions(score, true))));
        ++index;
    }
    print.externals = externals(master);
    print.registered = master.eid_register()->size();
    for (const EngravingObject *object : document_objects(master)) {
        const bool generated = object->is_engraving_item() && static_cast<const mu::engraving::EngravingItem *>(object)->generated();
        if (id_class(*object) != IdClass::LAYOUT && !generated) ++print.objects[object->type_name()];
    }
    return print;
}

std::vector<std::string> differences(const Fingerprint &before, const Fingerprint &after, const std::vector<std::string_view> &ignore) {
    std::vector<std::string> fields;
    const auto compare = [&](std::string_view name, bool equal) {
        if (!equal && std::ranges::find(ignore, name) == ignore.end()) fields.emplace_back(name);
    };
    compare("files", before.files == after.files);
    compare("semantic", before.semantic == after.semantic);
    compare("ids", before.ids == after.ids);
    compare("revision", before.revision == after.revision);
    compare("engine_error", before.engine_error == after.engine_error);
    compare("layout", same_layout(before.layout, after.layout));
    compare("externals", before.externals == after.externals);
    compare("registered", before.registered == after.registered);
    compare("objects", before.objects == after.objects);
    return fields;
}

std::string explain(const Fingerprint &before, const Fingerprint &after) {
    for (const auto &[name, text] : before.files) {
        const auto found = after.files.find(name);
        if (found == after.files.end()) return name + " is gone";
        if (found->second != text) return name + " " + first_difference(text, found->second);
    }
    if (before.semantic != after.semantic) return "semantic " + first_difference(before.semantic, after.semantic);
    if (before.externals != after.externals) return "externals " + first_difference(before.externals, after.externals);
    for (std::size_t i = 0; i < before.layout.size() && i < after.layout.size(); ++i) {
        if (const auto at = layout_mismatch(before.layout[i], after.layout[i])) {
            const auto [a, b] = *at;
            return std::format("layout at {}: '{}' vs '{}'", a, std::string_view(before.layout[i]).substr(a > 40 ? a - 40 : 0, 80),
                               std::string_view(after.layout[i]).substr(b > 40 ? b - 40 : 0, 80));
        }
    }
    if (before.layout.size() != after.layout.size()) return "layout: another number of laid-out scores";
    if (before.ids != after.ids) return "ids";
    for (const auto &[type, count] : before.objects) {
        const auto found = after.objects.find(type);
        const std::size_t now = found == after.objects.end() ? 0 : found->second;
        if (now != count) return std::format("objects: {} {} -> {}", type, count, now);
    }
    return {};
}

bool register_cases(std::vector<OpCase> group) {
    for (OpCase &op_case : group) cases().push_back(std::move(op_case));
    return true;
}

const std::vector<OpCase> &registered_cases() { return cases(); }

MasterScore &master_of(Document &document) { return *DocumentAccess::master_score(document); }

ElementId find_id(Document &document, std::string_view type, const std::function<bool(const EngravingObject &)> &match) {
    // Document order, not id order: an imported score gets new random ids on every
    // open, and each check opens the fixture again.
    for (const EngravingObject *object : document_objects(master_of(document))) {
        if (object->type_name() != type || !takes_id(*object) || !object->eid().is_valid()) continue;
        if (!match || match(*object)) return from_engine(object->eid());
    }
    throw std::invalid_argument(std::format("the fixture has no matching {} with an id", type));
}

EngravingObject &element(Document &document, const ElementId &id) {
    const auto found = resolve(master_of(document), id);
    if (!found) throw std::invalid_argument(found.error().message);
    return *found->object;
}

Value property(Document &document, const ElementId &id, std::string_view name) {
    const mu::engraving::Pid pid = api::pid_of(name);
    if (pid == mu::engraving::Pid::END) throw std::invalid_argument(std::format("no property '{}'", name));
    return api::to_value(mu::engraving::property_type(pid), element(document, id).get_property(pid));
}

ScoreSel excerpt(Document &document, std::size_t index) {
    const auto &excerpts = master_of(document).excerpts();
    if (index >= excerpts.size()) throw std::invalid_argument(std::format("the fixture has no excerpt {}", index));
    return {from_engine(excerpts[index]->excerpt_score()->eid())};
}

std::string run_case(Engine &engine, const OpCase &op_case, const std::filesystem::path &fixture) {
    auto opened = engine.open(fixture);
    if (!opened) return "fail: " + opened.error().message;
    auto document = std::move(*opened);
    const Fingerprint before = fingerprint(*document);
    const Batch batch = op_case.make(*document);
    const auto result = document->edit(batch, before.revision);
    if (!result) return "fail: the batch failed: " + std::string(to_string(result.error().code));
    const Fingerprint after = fingerprint(*document);
    if (after.revision != before.revision + 1) return std::format("fail: revision {} after {}", after.revision, before.revision);
    if (const std::string problem = inv(*document); !problem.empty()) return "fail: INV: " + problem;
    if (const std::string problem = op_case.expect(*document, before, after); !problem.empty()) return "fail: " + problem;

    const auto saved = document->export_score({.format = Format::MSCZ});
    if (!saved) return "fail: the save failed";
    auto reopened = engine.open(std::span<const std::byte>(*saved));
    if (!reopened) return "fail: the reopen failed";
    const Fingerprint again = fingerprint(**reopened);
    if (again.semantic != after.semantic) return "fail: the reopen changed the semantic dump";
    if (again.ids != after.ids) return "fail: the reopen changed the ids";
    return {};
}
} // namespace engrave::harness
