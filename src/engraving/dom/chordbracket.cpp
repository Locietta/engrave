#include "chordbracket.h"

#include "style/style.h"

namespace mu::engraving {
static const ElementStyle chordBracketStyle{
    {Sid::chordBracketHookLen, Pid::BRACKET_HOOK_LEN},
};

ChordBracket::ChordBracket(DummyParentOr<Chord> parent) : Arpeggio(parent, ElementType::CHORD_BRACKET) {
    setArpeggioType(ArpeggioType::BRACKET);

    initElementStyle(&chordBracketStyle);
}

PropertyValue ChordBracket::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::BRACKET_HOOK_LEN: return hookLength_;
        case Pid::BRACKET_HOOK_POS: return hookPos_;
        case Pid::BRACKET_RIGHT_SIDE: return rightSide_;
        default: break;
    }
    return Arpeggio::getProperty(propertyId);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool ChordBracket::setProperty(Pid propertyId, const PropertyValue &val) {
    switch (propertyId) {
        case Pid::BRACKET_HOOK_LEN: hookLength_ = val.value<Spatium>(); break;
        case Pid::BRACKET_HOOK_POS: hookPos_ = val.value<DirectionV>(); break;
        case Pid::BRACKET_RIGHT_SIDE: rightSide_ = val.toBool(); break;
        default:
            if (!Arpeggio::setProperty(propertyId, val)) {
                return false;
            }
            break;
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue ChordBracket::propertyDefault(Pid propertyId) const {
    switch (propertyId) {
        case Pid::BRACKET_HOOK_LEN: return style().styleS(Sid::chordBracketHookLen);
        case Pid::BRACKET_HOOK_POS: return DirectionV::AUTO; // Both
        case Pid::BRACKET_RIGHT_SIDE: return false;
        default: break;
    }
    return Arpeggio::propertyDefault(propertyId);
}

void ChordBracket::reset() {
    resetProperty(Pid::BRACKET_HOOK_LEN);
    resetProperty(Pid::BRACKET_HOOK_POS);
    resetProperty(Pid::BRACKET_RIGHT_SIDE);

    Arpeggio::reset();
}
} // namespace mu::engraving
