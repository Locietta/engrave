#include "chordbracket.h"

#include "style/style.h"

namespace mu::engraving {
static const ElementStyle chord_bracket_style{
    {Sid::CHORD_BRACKET_HOOK_LEN, Pid::BRACKET_HOOK_LEN},
};

ChordBracket::ChordBracket(DummyParentOr<Chord> parent) : Arpeggio(parent, ElementType::CHORD_BRACKET) {
    set_arpeggio_type(ArpeggioType::BRACKET);

    init_element_style(&::mu::engraving::chord_bracket_style);
}

PropertyValue ChordBracket::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::BRACKET_HOOK_LEN: return hook_length_;
        case Pid::BRACKET_HOOK_POS: return hook_pos_;
        case Pid::BRACKET_RIGHT_SIDE: return right_side_;
        default: break;
    }
    return Arpeggio::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool ChordBracket::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::BRACKET_HOOK_LEN: hook_length_ = val.value<Spatium>(); break;
        case Pid::BRACKET_HOOK_POS: hook_pos_ = val.value<DirectionV>(); break;
        case Pid::BRACKET_RIGHT_SIDE: right_side_ = val.to_bool(); break;
        default:
            if (!Arpeggio::set_property(property_id, val)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue ChordBracket::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::BRACKET_HOOK_LEN: return style().style_s(Sid::CHORD_BRACKET_HOOK_LEN);
        case Pid::BRACKET_HOOK_POS: return DirectionV::AUTO; // Both
        case Pid::BRACKET_RIGHT_SIDE: return false;
        default: break;
    }
    return Arpeggio::property_default(property_id);
}

void ChordBracket::reset() {
    reset_property(Pid::BRACKET_HOOK_LEN);
    reset_property(Pid::BRACKET_HOOK_POS);
    reset_property(Pid::BRACKET_RIGHT_SIDE);

    Arpeggio::reset();
}
} // namespace mu::engraving
