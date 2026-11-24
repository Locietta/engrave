#pragma once

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <engrave/handle.h>
#include <engrave/value.h>

namespace engrave {

// Adds text at the chord or rest named by a RelativePosition or ChordRest SegmentAddress.
// The saved-score style names are "staff", "system", "tempo" and "lyrics_odd".
// Lyrics follow the existing verses at the anchor. The text uses the engine's
// markup (<sym>...</sym>, <b>, ...) and must be nonempty to receive a persistent ID.
struct AddText {
    EditAddress at;
    std::string style;
    std::string text;
};

// Adds the item a type names to the element or position the anchor names.  type and
// props use the descriptions' names (Engine::describe), so a tempo text is "Tempo"
// and a dynamic's kind is "DYNAMIC_TYPE".  The (anchor type, child type) pairs this
// may create are the table in src/api/ops/text.cpp: an articulation on a chord, and a
// fermata, dynamic, expression, staff text, tempo text or harmony on a ChordRest segment.  Every other
// pair, and every other type, is an InvalidArgument.  props are set as SetProperty
// sets them; an articulation needs its "symbol" and a text its "text".
// A Harmony requires nonempty initial text in props.
struct Add {
    Ref anchor;
    std::string type;
    std::vector<std::pair<std::string, Value>> props;
};

using TextOps = std::tuple<AddText, Add>;

} // namespace engrave
