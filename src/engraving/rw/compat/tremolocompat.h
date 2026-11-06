#ifndef MU_ENGRAVING_TREMOLOCOMPAT_H
#define MU_ENGRAVING_TREMOLOCOMPAT_H

#include "../../dom/engravingobject.h"

namespace mu::engraving {
struct Chord;
struct TremoloSingleChord;
struct TremoloTwoChord;
} // namespace mu::engraving

namespace mu::engraving::compat {
struct TremoloCompat {
    DummyParentOr<Chord> parent;
    TremoloSingleChord *single = nullptr;
    TremoloTwoChord *two = nullptr;
};
} // namespace mu::engraving::compat

#endif // MU_ENGRAVING_TREMOLOCOMPAT_H
