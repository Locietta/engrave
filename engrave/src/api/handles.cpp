#include "engrave/src/api/handles.h"

#include <algorithm>
#include <format>
#include <limits>
#include <numeric>
#include <optional>
#include <string_view>
#include <variant>

#include "engraving/dom/beam.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurenumber.h"
#include "engraving/dom/mmrestrange.h"
#include "engraving/dom/note.h"
#include "engraving/dom/page.h"
#include "engraving/dom/part.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spacer.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/system.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/tie.h"
#include "engraving/editing/transaction/transaction.h"
#include "engraving/infrastructure/eidregister.h"

namespace engrave {
namespace {

using mu::engraving::ElementType;
using mu::engraving::EngravingItem;
using mu::engraving::EngravingObject;
using mu::engraving::MasterScore;
using mu::engraving::Measure;
using mu::engraving::Page;
using mu::engraving::Part;
using mu::engraving::Score;
using mu::engraving::Segment;
using mu::engraving::SegmentType;
using mu::engraving::Spanner;
using mu::engraving::Staff;
using mu::engraving::System;

// No position: an object that is not in a measure at a track.
constexpr int no_position = -1;
ObjectWalkHook object_walk_hook = nullptr;

std::unexpected<Error> fail(ErrorCode code, std::string message) { return std::unexpected(Error{code, std::move(message)}); }

std::optional<Fraction> address_tick(const mu::engraving::Fraction &value) {
    const std::int64_t num = value.numerator64();
    const std::int64_t den = value.denominator64();
    const auto low = std::numeric_limits<int>::min();
    const auto high = std::numeric_limits<int>::max();
    if (den <= 0) return std::nullopt;
    if (num >= low && num <= high && den <= high) return Fraction{num, den};

    // Preserve existing representations when they fit; otherwise reduce the
    // exact terms so an equivalent representable address can still be returned.
    const auto bits = static_cast<std::uint64_t>(num);
    const auto magnitude = num < 0 ? std::uint64_t{0} - bits : bits;
    const auto divisor = std::gcd(magnitude, static_cast<std::uint64_t>(den));
    const auto reduced_num = magnitude / divisor;
    const auto reduced_den = static_cast<std::uint64_t>(den) / divisor;
    const auto max_magnitude = static_cast<std::uint64_t>(high) + (num < 0 ? 1u : 0u);
    if (reduced_num > max_magnitude || reduced_den > static_cast<std::uint64_t>(high)) return std::nullopt;
    const auto signed_num = static_cast<std::int64_t>(reduced_num);
    return Fraction{num < 0 ? -signed_num : signed_num, static_cast<std::int64_t>(reduced_den)};
}

template <typename Function>
void for_each_object(MasterScore &master, const Function &func) {
    if (object_walk_hook) object_walk_hook();
    ankerl::unordered_dense::set<const EngravingObject *> seen;
    const auto visit = [&func, &seen](EngravingObject *object) {
        if (object && mu::engraving::source_object(*object) && seen.insert(object).second) func(*object);
    };
    for (Score *score : master.score_list()) {
        visit(score);
        for (Part *part : score->parts()) visit(part);
        for (Staff *staff : score->staves()) visit(staff);
        for (Spanner *spanner : score->spanner_list()) visit(spanner);
        // scanElements skips what is on a hidden staff and never hands out a chord
        // itself (only its notes, stem and the rest), so a measure's own lists and
        // segments are walked as well, and a chord is reached through its parts.
        const auto visit_item = [&visit](EngravingItem *item) {
            visit(item);
            if (item->is_note() && mu::engraving::source_object(*item)) {
                const auto &note = *mu::engraving::to_note(item);
                visit(note.tie_for());
                if (auto *tie = note.tie_back(); tie && tie->is_partial_tie() && !tie->start_element()) visit(tie);
                for (auto *spanner : note.spanner_for())
                    if (spanner->anchor() == Spanner::Anchor::NOTE) visit(spanner);
            }
            EngravingObject *parent = item->parent();
            if (parent && parent->is_chord_rest()) visit(parent);
            // A beam is referred to by the chords it joins, not held by a measure.
            if (item->is_chord_rest()) visit(mu::engraving::to_chord_rest(item)->beam());
        };
        const auto visit_tree = [&visit_item](EngravingItem *item) {
            visit_item(item);
            item->scan_elements(visit_item);
        };
        for (mu::engraving::MeasureBase *mb = score->first(); mb; mb = mb->next()) {
            visit(mb);
            if (!mb->is_measure()) {
                visit_tree(mb);
                continue;
            }
            Measure *measure = mu::engraving::to_measure(mb);

            for (EngravingItem *item : measure->el()) visit_tree(item);
            for (auto *staff : measure->mstaves()) {
                if (auto *spacer = staff->vspacer_up()) visit_tree(spacer);
                if (auto *spacer = staff->vspacer_down()) visit_tree(spacer);
            }
            for (Segment *segment = measure->first(); segment; segment = segment->next()) {
                for (EngravingItem *item : segment->elist()) {
                    if (item) visit_tree(item);
                }
                for (EngravingItem *item : segment->annotations()) visit_tree(item);
            }
        }
    }
}

} // namespace

void set_object_walk_hook(ObjectWalkHook hook) { object_walk_hook = hook; }

bool in_score(MasterScore &master, const EngravingObject *object) {
    const EngravingObject *top = object;
    while (const EngravingObject *parent = top->ownership_parent()) top = parent;
    for (const Score *score : master.score_list()) {
        if (static_cast<const EngravingObject *>(score) == top) return true;
    }
    // Unplaced spanner segments and their children belong to the registered
    // source spanner even before a system exposes them through scan_elements.
    if (!top->is_spanner() || !top->score()) return false;
    const auto spanners = top->score()->spanner_list();
    return std::ranges::find(spanners, mu::engraving::to_spanner(top)) != spanners.end();
}

namespace {

// The single-bit segment types an Address can name, with the token that names each.
struct SegmentKind {
    SegmentType type;
    std::string_view token;
};

constexpr SegmentKind segment_kinds[] = {
    {SegmentType::BEGIN_BAR_LINE, "BeginBarLine"},
    {SegmentType::HEADER_CLEF, "HeaderClef"},
    {SegmentType::KEY_SIG, "KeySig"},
    {SegmentType::AMBITUS, "Ambitus"},
    {SegmentType::BREATH, "Breath"},
    {SegmentType::TIME_SIG, "TimeSig"},
    {SegmentType::START_REPEAT_BAR_LINE, "StartRepeatBarLine"},
    {SegmentType::CLEF_START_REPEAT_ANNOUNCE, "ClefStartRepeatAnnounce"},
    {SegmentType::KEY_SIG_START_REPEAT_ANNOUNCE, "KeySigStartRepeatAnnounce"},
    {SegmentType::TIME_SIG_START_REPEAT_ANNOUNCE, "TimeSigStartRepeatAnnounce"},
    {SegmentType::CLEF, "Clef"},
    {SegmentType::BAR_LINE, "BarLine"},
    {SegmentType::TIME_TICK, "TimeTick"},
    {SegmentType::CHORD_REST, "ChordRest"},
    {SegmentType::CLEF_REPEAT_ANNOUNCE, "ClefRepeatAnnounce"},
    {SegmentType::KEY_SIG_REPEAT_ANNOUNCE, "KeySigRepeatAnnounce"},
    {SegmentType::TIME_SIG_REPEAT_ANNOUNCE, "TimeSigRepeatAnnounce"},
    {SegmentType::END_BAR_LINE, "EndBarLine"},
    {SegmentType::KEY_SIG_ANNOUNCE, "KeySigAnnounce"},
    {SegmentType::TIME_SIG_ANNOUNCE, "TimeSigAnnounce"},
};

SegmentType segment_type_of(std::string_view token) {
    for (const SegmentKind &kind : segment_kinds) {
        if (kind.token == token) return kind.type;
    }
    return SegmentType::INVALID;
}

std::string_view segment_token(SegmentType type) {
    for (const SegmentKind &kind : segment_kinds) {
        if (kind.type == type) return kind.token;
    }
    return "Invalid";
}

template <typename T>
Result<Resolved> resolve_at(const std::vector<T *> &items, std::int32_t index, std::string_view what) {
    if (index < 0 || static_cast<std::size_t>(index) >= items.size()) {
        return fail(ErrorCode::STALE_HANDLE, std::format("no {} at index {}", what, index));
    }
    return Resolved{items[static_cast<std::size_t>(index)], false};
}

} // namespace

IdClass id_class(const EngravingObject &object) {
    switch (object.type()) {
        case ElementType::SEGMENT: return IdClass::ADDRESS;
        // Layout builds these from what is written (a chord builds its stem, a system its
        // brackets), so an id on one is not in the file and does not survive the next layout
        // pass.  RootItem and DummyParent belong to the score itself.
        case ElementType::SYSTEM:
        case ElementType::PAGE:
        case ElementType::BRACKET:
        case ElementType::BRACKET_ITEM:
        case ElementType::INSTRUMENT_NAME:
        case ElementType::MEASURE_NUMBER:
        case ElementType::STAFF_LINES:
        case ElementType::LEDGER_LINE:
        case ElementType::STEM:
        case ElementType::STEM_SLASH:
        case ElementType::HOOK:
        case ElementType::NOTEDOT:
        case ElementType::GUITAR_BEND_TEXT:
        case ElementType::GRACE_NOTES_GROUP:
        case ElementType::FIGURED_BASS_ITEM:
        case ElementType::PARENTHESIS:
        case ElementType::ROOT_ITEM:
        case ElementType::DUMMY: return IdClass::LAYOUT;
        default: break;
    }
    // A multi-measure rest is layout's: it replaces a range of measures, and so is
    // everything it holds.
    if (object.is_measure() && mu::engraving::to_measure(&object)->is_mm_rest()) return IdClass::LAYOUT;
    if (object.is_engraving_item()) {
        const Measure *measure = static_cast<const EngravingItem &>(object).find_measure();
        if (measure && measure->is_mm_rest()) return IdClass::LAYOUT;
    }
    // A tuplet draws its own number, and layout creates that text.
    if (object.is_text() && object.ownership_parent() && object.ownership_parent()->is_tuplet()) return IdClass::LAYOUT;
    if (object.is_spanner()) {
        const Score *score = object.score();
        if (!score) return IdClass::SESSION;
        if (mu::engraving::to_spanner(&object)->anchor() == Spanner::Anchor::NOTE) return IdClass::PERSISTED;
        const auto spanners = score->spanner_list();
        const auto found = std::ranges::find(spanners, mu::engraving::to_spanner(&object));
        return found == spanners.end() ? IdClass::SESSION : IdClass::PERSISTED;
    }
    // A spanner segment is rebuilt by layout from its spanner.
    if (object.is_spanner_segment()) return IdClass::LAYOUT;
    // Whatever layout hangs off a layout object (the text of a hairpin, a notehead of a
    // grace group) is rebuilt with it.
    const EngravingObject *parent = object.ownership_parent();
    if (parent && id_class(*parent) == IdClass::LAYOUT) return IdClass::LAYOUT;
    return IdClass::PERSISTED;
}

bool takes_id(const EngravingObject &object) {
    if (id_class(object) != IdClass::PERSISTED) return false;
    if (!object.is_engraving_item()) return true;
    const auto &item = static_cast<const EngravingItem &>(object);
    // Measure rewrites generate ties that the native writer persists.
    if (item.generated() && !item.is_tie() && !item.is_partial_tie() && !item.is_laissez_vib()) return false;
    // The writer stores no id for an empty text (a template's empty title, subtitle or
    // poet placeholder), so one of those has none until it has text.
    return !item.is_text() || !static_cast<const mu::engraving::TextBase &>(item).empty();
}

mu::engraving::EID to_engine(const ElementId &id) { return mu::engraving::EID::from_parts(id.hi, id.lo); }

ElementId from_engine(const mu::engraving::EID &id) { return {id.first(), id.second()}; }

// One explicit load/create/restore bootstrap. Subsequent edits maintain this graph
// through collection hooks; this walk is never a resolution fallback.
static void initialize_membership(MasterScore &master) {
    auto &registry = *master.eid_register();
    registry.begin_membership();
    registry.set_live(master, true);
    for (Score *score : master.score_list()) {
        if (score != &master) mu::engraving::source_child_added(master, score);
        for (Part *part : score->parts()) mu::engraving::source_child_added(*score, part);
        for (Staff *staff : score->staves())
            if (staff->part()) mu::engraving::source_child_added(*staff->part(), staff);
        for (Spanner *spanner : score->spanner_list()) mu::engraving::source_child_added(*score, spanner);
        for (auto *base = score->first(); base; base = base->next()) mu::engraving::source_child_added(*score, base);
    }
}

void assign_missing_ids(MasterScore &master) {
    ankerl::unordered_dense::set<const EngravingObject *> document;
    const bool initial = !master.eid_register()->membership_ready();
    for_each_object(master, [&document, &master](EngravingObject &object) {
        document.insert(&object);
        if (takes_id(object)) {
            if (!object.eid().is_valid()) object.assign_new_eid();
            master.eid_register()->publish(object);
        }
    });
    // Legacy native output still persists these display records. Their private
    // wire identities must exist before saving without publishing API handles.
    const auto identify = [](EngravingObject *item) {
        if (item && !item->eid().is_valid()) item->assign_new_eid();
    };
    for (Score *score : master.score_list()) {
        for (Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
            for (Measure *written : {measure, measure->mm_rest()}) {
                if (!written) continue;
                if (written->is_mm_rest()) {
                    identify(written);
                    for (const Segment &segment : written->segments())
                        for (auto *item : segment.elist())
                            if (item && item->is_mm_rest()) identify(item);
                }
                for (const auto *staff : written->mstaves()) {
                    if (auto *number = staff->measure_number(); number && !number->generated()) identify(number);
                    if (auto *range = staff->mm_range_text(); range && !range->generated()) identify(range);
                }
            }
        }
    }
    // An id whose element is no longer part of the document must go as well: a read can
    // replace what it built (an excerpt score's first pass, a tie whose other end is
    // missing), and such an entry would answer for an element no caller can address.
    // An element the walk does not hand out but that is still in a score (a guitar bend
    // on its note) keeps the id the file gave it, so that a save writes the same id again.
    // Detached roots retain their EIDs until reinsertion or batch cleanup.
    if (!master.transaction_manager()->current_transaction()) {
        for (const auto &[eid, object] : master.eid_register()->entries()) {
            if (!document.contains(object) && !in_score(master, object)) master.eid_register()->remove_item(object);
        }
    }
    if (initial) initialize_membership(master);
}

std::vector<EngravingObject *> document_objects(MasterScore &master) {
    std::vector<EngravingObject *> objects;
    for_each_object(master, [&objects](EngravingObject &object) { objects.push_back(&object); });
    return objects;
}

std::vector<LiveId> live_ids(MasterScore &master) {
    std::vector<LiveId> ids;
    for_each_object(master, [&master, &ids](EngravingObject &object) {
        if (!takes_id(object)) return;
        const mu::engraving::EID eid = object.eid();
        if (eid.is_valid())
            ids.push_back(
                {from_engine(eid), &object, master.eid_register()->id_state(master.eid_register()->handle_of(&object).slot).placement});
    });
    std::ranges::sort(ids, {}, &LiveId::id);
    return ids;
}

IdDiff diff_ids(std::span<const LiveId> before, std::span<const LiveId> after) {
    const auto contains = [](std::span<const LiveId> ids, ElementId id) { return std::ranges::binary_search(ids, id, {}, &LiveId::id); };
    std::vector<std::pair<Placement, size_t>> appeared;
    for (const auto &entry : after)
        if (!contains(before, entry.id)) appeared.emplace_back(entry.placement, 0);
    std::ranges::sort(appeared);
    for (auto it = appeared.begin(); it != appeared.end();) {
        const auto end = std::upper_bound(it, appeared.end(), it->first,
                                          [](const auto &placement, const auto &entry) { return placement < entry.first; });
        it->second = static_cast<size_t>(end - it);
        it = end;
    }

    IdDiff diff;
    for (const LiveId &entry : after) {
        if (!contains(before, entry.id)) diff.created.push_back(entry.id);
    }
    for (const LiveId &entry : before) {
        if (contains(after, entry.id)) continue;
        auto count = std::ranges::lower_bound(appeared, entry.placement, {}, &std::pair<Placement, size_t>::first);
        if (entry.placement.track == no_position || count == appeared.end() || count->first != entry.placement || count->second == 0) {
            diff.removed.push_back(entry.id);
        } else {
            --count->second;
            diff.replaced.push_back(entry.id);
        }
    }
    std::ranges::sort(diff.created);
    std::ranges::sort(diff.removed);
    std::ranges::sort(diff.replaced);
    return diff;
}

IdDiff batch_id_diff(MasterScore &master) {
    auto &registry = *master.eid_register();
    std::vector<LiveId> before, after;
    for (const auto &initial : registry.touched_slots()) {
        if (initial.published && initial.eid.is_valid()) before.push_back({from_engine(initial.eid), nullptr, initial.placement});
        const auto current = registry.id_state(initial.index);
        if (current.published && current.eid.is_valid()) after.push_back({from_engine(current.eid), nullptr, current.placement});
    }
    std::ranges::sort(before, {}, &LiveId::id);
    std::ranges::sort(after, {}, &LiveId::id);
    return diff_ids(before, after);
}

Result<Resolved> resolve(MasterScore &master, const ElementId &id) {
    const mu::engraving::EID eid = to_engine(id);
    auto &registry = *master.eid_register();
    const auto handle = registry.handle_from_eid(eid);
    if (!handle.is_valid()) return fail(ErrorCode::NOT_FOUND, std::format("no element has id {}", to_string(id)));
    EngravingObject *object = registry.live_item(handle);
    if (!object || !registry.published(handle)) {
        return fail(ErrorCode::STALE_HANDLE, std::format("the element with id {} is not part of the score", to_string(id)));
    }
    return Resolved{object, true};
}

ScoreSel selection_of(const Score &score) {
    ScoreSel selection;
    if (!score.is_master()) selection.excerpt = from_engine(score.eid());
    return selection;
}

// The score a selection names.
Result<Score *> select_score(MasterScore &master, const ScoreSel &selection) {
    if (!selection.excerpt) return &master;
    const Result<Resolved> found = resolve(master, *selection.excerpt);
    if (!found) return std::unexpected(found.error());
    if (found->object->type() != ElementType::SCORE) {
        return std::unexpected(
            Error{ErrorCode::INVALID_ARGUMENT, std::format("the selection names a {}, not a score", found->object->type_name())});
    }
    return mu::engraving::to_score(found->object);
}

Result<Resolved> resolve(MasterScore &master, const Address &address) {
    return std::visit(
        [&master](const auto &value) -> Result<Resolved> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, SegmentAddress>) {
                const Result<Resolved> measure = resolve(master, value.measure);
                if (!measure) return measure;
                if (measure->object->type() != ElementType::MEASURE) {
                    return fail(ErrorCode::INVALID_ARGUMENT, "a segment address names a measure");
                }
                const SegmentType kind = segment_type_of(value.kind);
                if (kind == SegmentType::INVALID) {
                    return fail(ErrorCode::INVALID_ARGUMENT, std::format("'{}' is not a segment type", value.kind));
                }
                const Fraction &tick = value.tick;
                if (tick.num < std::numeric_limits<int>::min() || tick.num > std::numeric_limits<int>::max() || tick.den <= 0 ||
                    tick.den > std::numeric_limits<int>::max()) {
                    return fail(ErrorCode::INVALID_ARGUMENT, "a segment tick must fit the engine's fraction with a positive denominator");
                }
                Measure *found = mu::engraving::to_measure(measure->object);
                // findSegment subtracts the measure tick before comparing. Large
                // denominators can overflow that intermediate. Compare absolute
                // ticks without narrowing the engine's unreduced 64-bit terms.
                Segment *segment = nullptr;
                for (Segment *candidate = found->first(); candidate; candidate = candidate->next()) {
                    if (!candidate->is_type(kind)) continue;
                    const auto position = candidate->tick();
                    if (position.denominator64() > 0 &&
                        detail::compare_fraction(position.numerator64(), position.denominator64(), tick.num, tick.den) == 0) {
                        segment = candidate;
                        break;
                    }
                }
                if (!segment) {
                    return fail(ErrorCode::STALE_HANDLE,
                                std::format("no {} segment at tick {}/{}", value.kind, value.tick.num, value.tick.den));
                }
                return Resolved{segment, true};
            } else if constexpr (std::is_same_v<T, PageAddress>) {
                const Result<Score *> selected = select_score(master, value.score);
                if (!selected) return fail(ErrorCode::STALE_HANDLE, selected.error().message);
                return resolve_at((*selected)->pages(), value.index, "page");
            } else if constexpr (std::is_same_v<T, SystemAddress>) {
                const Result<Score *> selected = select_score(master, value.score);
                if (!selected) return fail(ErrorCode::STALE_HANDLE, selected.error().message);
                const Result<Resolved> page = resolve_at((*selected)->pages(), value.page, "page");
                if (!page) return page;
                return resolve_at(mu::engraving::to_page(page->object)->systems(), value.index, "system");
            } else {
                const Result<Resolved> spanner = resolve(master, value.spanner);
                if (!spanner) return spanner;
                if (!spanner->object->is_spanner()) {
                    return fail(ErrorCode::INVALID_ARGUMENT, "a spanner segment address names a spanner");
                }
                return resolve_at(mu::engraving::to_spanner(spanner->object)->spanner_segments(), value.index, "spanner segment");
            }
        },
        address);
}

std::optional<Address> address_of(const EngravingObject &object) {
    if (object.type() == ElementType::SEGMENT) {
        const auto &segment = static_cast<const Segment &>(object);
        const Measure *measure = segment.measure();
        if (!measure || !measure->eid().is_valid()) return std::nullopt;
        SegmentAddress address;
        address.measure = from_engine(measure->eid());
        const auto tick = address_tick(segment.tick());
        if (!tick) return std::nullopt;
        address.tick = *tick;
        address.kind = segment_token(segment.segment_type());
        return Address{address};
    }
    if (object.type() == ElementType::PAGE) {
        const auto &page = static_cast<const Page &>(object);
        Score *score = page.score();
        if (!score) return std::nullopt;
        PageAddress address;
        address.score = selection_of(*score);
        address.index = static_cast<std::int32_t>(score->page_idx(&page));
        return Address{address};
    }
    if (object.type() == ElementType::SYSTEM) {
        const auto &system = static_cast<const System &>(object);
        Score *score = system.score();
        const Page *page = system.page();
        if (!score || !page) return std::nullopt;
        SystemAddress address;
        address.score = selection_of(*score);
        address.page = static_cast<std::int32_t>(score->page_idx(page));
        const auto &systems = page->systems();
        const auto index = std::ranges::find(systems, &system);
        if (index == systems.end()) return std::nullopt;
        address.index = static_cast<std::int32_t>(index - systems.begin());
        return Address{address};
    }
    if (object.is_spanner_segment()) {
        const auto &segment = static_cast<const mu::engraving::SpannerSegment &>(object);
        const Spanner *spanner = segment.spanner();
        if (!spanner || !spanner->eid().is_valid()) return std::nullopt;
        const auto &segments = spanner->spanner_segments();
        const auto index = std::ranges::find(segments, &segment);
        if (index == segments.end()) return std::nullopt;
        SpannerSegmentAddress address;
        address.spanner = from_engine(spanner->eid());
        address.index = static_cast<std::int32_t>(index - segments.begin());
        return Address{address};
    }
    return std::nullopt;
}

std::vector<Diagnostic> id_repairs(MasterScore &master) {
    const mu::engraving::EIDRepairCount repairs = master.eid_register()->repairs();
    std::vector<Diagnostic> diagnostics;
    if (repairs.malformed > 0) {
        diagnostics.push_back(
            {Severity::WARNING, std::format("{} element id(s) were malformed and ignored; those elements got new ids", repairs.malformed)});
    }
    if (repairs.duplicate > 0) {
        diagnostics.push_back(
            {Severity::WARNING, std::format("{} element(s) shared an id with another; each got a new id", repairs.duplicate)});
    }
    return diagnostics;
}

} // namespace engrave
