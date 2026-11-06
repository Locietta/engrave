#include "partialtie.h"

#include "chord.h"
#include "measure.h"
#include "note.h"
#include "score.h"
#include "staff.h"

namespace mu::engraving {
PartialTie::PartialTie(DummyParentOr<Note> parent) : Tie(ElementType::PARTIAL_TIE, parent) {}

mu::engraving::PropertyValue mu::engraving::PartialTie::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::PARTIAL_SPANNER_DIRECTION: return partial_spanner_direction();
        default: return Tie::get_property(property_id);
    }
}

PropertyValue PartialTie::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::PARTIAL_SPANNER_DIRECTION: return PartialSpannerDirection::OUTGOING;
        default: return Tie::property_default(property_id);
    }
}

bool PartialTie::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::PARTIAL_SPANNER_DIRECTION: set_partial_spanner_direction(v.value<PartialSpannerDirection>()); break;
        default: return Tie::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

SlurTieSegment *PartialTie::new_slur_tie_segment() {
    PartialTieSegment *seg = new PartialTieSegment(this);
    seg->set_track(track());
    return seg;
}

void PartialTie::set_start_note(Note *note) {
    set_partial_spanner_direction(PartialSpannerDirection::OUTGOING);
    Tie::set_start_note(note);
    Tie::set_end_note(nullptr);
}

void PartialTie::set_end_note(Note *note) {
    set_partial_spanner_direction(PartialSpannerDirection::INCOMING);
    Tie::set_start_note(nullptr);
    Tie::set_end_note(note);
    set_ownership_parent(note);
}

bool PartialTie::all_jump_points_inactive() const {
    if (!is_outgoing()) {
        return false;
    }
    return Tie::all_jump_points_inactive();
}

TieJumpPointList *PartialTie::tie_jump_points() {
    if (!is_outgoing()) {
        return nullptr;
    }

    return Tie::tie_jump_points();
}

const TieJumpPointList *PartialTie::tie_jump_points() const {
    if (!is_outgoing()) {
        return nullptr;
    }

    return Tie::tie_jump_points();
}

Note *PartialTie::start_note() const {
    if (is_outgoing()) {
        return Tie::start_note();
    }
    return start_tie() ? start_tie()->start_note() : nullptr;
}

PartialTieSegment::PartialTieSegment(PartialTie *sp) : TieSegment(ElementType::PARTIAL_TIE_SEGMENT, sp) {}

PartialTieSegment::PartialTieSegment(const PartialTieSegment &s) : TieSegment(s) {}

String PartialTieSegment::format_bars_and_beats() const {
    const PartialTie *pt = this->partial_tie();
    const Note *note = pt ? pt->note() : nullptr;
    const Chord *chord = note ? note->chord() : nullptr;
    const Segment *seg = chord ? chord->segment() : nullptr;

    if (!seg) {
        return EngravingItem::format_bars_and_beats();
    }

    return seg->format_bars_and_beats();
}
} // namespace mu::engraving
