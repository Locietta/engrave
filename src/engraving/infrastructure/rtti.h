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
#pragma once

#include <cassert>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "global/asserts.h"

namespace mu::engraving {
void report_bad_cast(const char *from, const char *to, const char *file, int line);
#define DECLARE_CLASSOF(Type)                                                     \
public:                                                                           \
    constexpr ElementType declared_item_type() const noexcept { return Type; }    \
    ElementType concrete_type() const noexcept override { return Type; }          \
    static bool classof(const ElementType type) noexcept { return type == Type; } \
    static bool classof(const EngravingObject *item) noexcept { return item->type() == Type; }

struct Arpeggio;
struct Articulation;
struct BeamBase;
struct ChordRest;
struct LyricsLine;
struct LyricsLineSegment;
struct MeasureBase;
struct Part;
struct Rest;
struct SLine;
struct Slur;
struct SlurSegment;
struct Spanner;
struct StaffTextBase;
struct Symbol;
struct SystemText;
struct TextBase;
struct TextLineBase;
struct Tie;
struct TieSegment;
struct VBox;

// An inherited classof still tests the declaring class's exact tag.
template <typename T>
concept DeclaresItemType = requires(const T &item) {
    requires std::is_same_v<decltype(&T::declared_item_type), decltype(item.declared_item_type()) (T::*)() const noexcept>;
};

template <typename Tag>
constexpr bool is_text_line_base_concrete_type(Tag type) noexcept {
    switch (type) {
        case Tag::HAIRPIN:
        case Tag::LET_RING:
        case Tag::GRADUAL_TEMPO_CHANGE:
        case Tag::TEXTLINE:
        case Tag::OTTAVA:
        case Tag::PALM_MUTE:
        case Tag::PICK_SCRAPE:
        case Tag::WHAMMY_BAR:
        case Tag::RASGUEADO:
        case Tag::HARMONIC_MARK:
        case Tag::PEDAL:
        case Tag::VOLTA:
        case Tag::NOTELINE: return true;
        default: return false;
    }
}

template <typename Tag>
constexpr bool is_s_line_concrete_type(Tag type) noexcept {
    switch (type) {
        case Tag::TRILL:
        case Tag::GLISSANDO:
        case Tag::VIBRATO:
        case Tag::GUITAR_BEND:
        case Tag::GUITAR_BEND_HOLD:
        case Tag::LYRICSLINE:
        case Tag::PARTIAL_LYRICSLINE: return true;
        default: return is_text_line_base_concrete_type(type);
    }
}

template <typename Tag>
constexpr bool is_text_base_concrete_type(Tag type) noexcept {
    switch (type) {
        case Tag::TEXT:
        case Tag::LYRICS:
        case Tag::DYNAMIC:
        case Tag::EXPRESSION:
        case Tag::FINGERING:
        case Tag::HARMONY:
        case Tag::MARKER:
        case Tag::JUMP:
        case Tag::STAFF_TEXT:
        case Tag::STAVE_SHARING_LABEL:
        case Tag::SYSTEM_TEXT:
        case Tag::TRIPLET_FEEL:
        case Tag::PLAY_COUNT_TEXT:
        case Tag::PLAYTECH_ANNOTATION:
        case Tag::CAPO:
        case Tag::STRING_TUNINGS:
        case Tag::REHEARSAL_MARK:
        case Tag::INSTRUMENT_CHANGE:
        case Tag::FIGURED_BASS:
        case Tag::TEMPO_TEXT:
        case Tag::INSTRUMENT_NAME:
        case Tag::MEASURE_NUMBER:
        case Tag::MMREST_RANGE:
        case Tag::STICKING:
        case Tag::HARP_DIAGRAM:
        case Tag::GUITAR_BEND_TEXT:
        case Tag::HAMMER_ON_PULL_OFF_TEXT:
        case Tag::TAPPING_TEXT: return true;
        default: return false;
    }
}

template <typename T, typename From>
    requires DeclaresItemType<T> || std::is_same_v<T, ChordRest> || std::is_same_v<T, MeasureBase> || std::is_same_v<T, SLine> ||
             std::is_same_v<T, Spanner> || std::is_same_v<T, StaffTextBase> || std::is_same_v<T, TextBase> ||
             std::is_same_v<T, TextLineBase>
bool item_type_allows(const From *item) noexcept {
    const auto type = item->concrete_type();
    using Tag = decltype(type);
    if constexpr (std::is_same_v<T, Arpeggio>) {
        return type == Tag::ARPEGGIO || type == Tag::CHORD_BRACKET;
    } else if constexpr (std::is_same_v<T, Articulation>) {
        return type == Tag::ARTICULATION || type == Tag::ORNAMENT || type == Tag::TAPPING;
    } else if constexpr (std::is_same_v<T, BeamBase>) {
        return type == Tag::BEAM || type == Tag::TREMOLO_TWOCHORD;
    } else if constexpr (std::is_same_v<T, ChordRest>) {
        return type == Tag::CHORD || type == Tag::REST || type == Tag::MMREST || type == Tag::MEASURE_REPEAT;
    } else if constexpr (std::is_same_v<T, LyricsLine>) {
        return type == Tag::LYRICSLINE || type == Tag::PARTIAL_LYRICSLINE;
    } else if constexpr (std::is_same_v<T, LyricsLineSegment>) {
        return type == Tag::LYRICSLINE_SEGMENT || type == Tag::PARTIAL_LYRICSLINE_SEGMENT;
    } else if constexpr (std::is_same_v<T, MeasureBase>) {
        return type == Tag::MEASURE || type == Tag::HBOX || type == Tag::VBOX || type == Tag::TBOX || type == Tag::FBOX;
    } else if constexpr (std::is_same_v<T, Part>) {
        return type == Tag::PART || type == Tag::SHARED_PART;
    } else if constexpr (std::is_same_v<T, Rest>) {
        return type == Tag::REST || type == Tag::MMREST || type == Tag::MEASURE_REPEAT;
    } else if constexpr (std::is_same_v<T, SLine>) {
        return is_s_line_concrete_type(type);
    } else if constexpr (std::is_same_v<T, Slur>) {
        return type == Tag::SLUR || type == Tag::HAMMER_ON_PULL_OFF || type == Tag::TAPPING_HALF_SLUR;
    } else if constexpr (std::is_same_v<T, SlurSegment>) {
        return type == Tag::SLUR_SEGMENT || type == Tag::HAMMER_ON_PULL_OFF_SEGMENT || type == Tag::TAPPING_HALF_SLUR_SEGMENT;
    } else if constexpr (std::is_same_v<T, Spanner>) {
        return type == Tag::SLUR || type == Tag::HAMMER_ON_PULL_OFF || type == Tag::TAPPING_HALF_SLUR || type == Tag::TIE ||
               type == Tag::LAISSEZ_VIB || type == Tag::PARTIAL_TIE || is_s_line_concrete_type(type);
    } else if constexpr (std::is_same_v<T, StaffTextBase>) {
        return type == Tag::STAFF_TEXT || type == Tag::STAVE_SHARING_LABEL || type == Tag::SYSTEM_TEXT || type == Tag::TRIPLET_FEEL ||
               type == Tag::PLAYTECH_ANNOTATION || type == Tag::CAPO || type == Tag::STRING_TUNINGS;
    } else if constexpr (std::is_same_v<T, Symbol>) {
        return type == Tag::SYMBOL || type == Tag::HOOK || type == Tag::NOTEHEAD || type == Tag::SYSTEM_DIVIDER;
    } else if constexpr (std::is_same_v<T, SystemText>) {
        return type == Tag::SYSTEM_TEXT || type == Tag::TRIPLET_FEEL;
    } else if constexpr (std::is_same_v<T, TextBase>) {
        return is_text_base_concrete_type(type);
    } else if constexpr (std::is_same_v<T, TextLineBase>) {
        return is_text_line_base_concrete_type(type);
    } else if constexpr (std::is_same_v<T, Tie>) {
        return type == Tag::TIE || type == Tag::LAISSEZ_VIB || type == Tag::PARTIAL_TIE;
    } else if constexpr (std::is_same_v<T, TieSegment>) {
        return type == Tag::TIE_SEGMENT || type == Tag::LAISSEZ_VIB_SEGMENT || type == Tag::PARTIAL_TIE_SEGMENT;
    } else if constexpr (std::is_same_v<T, VBox>) {
        return type == Tag::VBOX || type == Tag::TBOX || type == Tag::FBOX;
    } else {
        return T::classof(type);
    }
}

template <typename To, typename From>
bool is_classof(From *p) noexcept {
    using ResultType = std::remove_pointer_t<To>;
    return p ? ResultType::classof(p) : false;
}

enum class CastMode {
    CHECK = 0, // should be correct; assert if still not
    BAD,       // known to be bad; don’t assert, we’ll fix it later
    MAYBE_BAD  // in this case it’s okay if type mismatch
};

template <typename To, typename From>
To item_cast(From from, CastMode mode = CastMode::CHECK) noexcept {
    if constexpr (std::is_convertible_v<From, To>) {
        return static_cast<To>(from);
    } else {
        using Target = std::remove_cv_t<std::remove_pointer_t<To>>;
        To casted;
        if constexpr (requires {
                          static_cast<To>(from);
                          item_type_allows<Target>(from);
                      }) {
            const bool matches = from && item_type_allows<Target>(from);
            assert(matches == (dynamic_cast<To>(from) != nullptr));
            casted = matches ? static_cast<To>(from) : nullptr;
        } else {
            casted = dynamic_cast<To>(from);
        }
        if (mode == CastMode::CHECK) {
            if (!(casted || !from)) {
                report_bad_cast(typeid(From).name(), typeid(To).name(), __FILE__, __LINE__);
            }
            assert(casted || !from);
        }
        return casted;
    }
}
} // namespace mu::engraving

namespace mu::engraving::rtti {
/* ===============================================
This visitor to foreaching the list of given types
How to use:
1. Make types list:
using SomeTypeList = TypeList<ItemA, ItemB>;

2. Create subclass like this, and implement the `doVisit` method:
class FooVisitor : public rtti::Visitor<FooVisitor>
{
public:
    template<typename T>
    static bool doVisit(EngravingItem* item, ...)
    {
        if (T::classof(item)) {
            ...
            return true;
        }
        return false;
    }
};

3. Call the `visit` method:

FooVisitor::visit(SomeTypeList {}, item, ...);

*/

template <typename... Types>
struct TypeList {};

template <typename Impl>
struct Visitor {
    enum Policy { MAY_BE_NOT_FOUND = 0, SHOULD_BE_FOUND };

    template <typename... Types, typename... Args>
    static bool visit(TypeList<Types...> types, Args &&...args) {
        return visit(Policy::MAY_BE_NOT_FOUND, types, std::forward<Args>(args)...);
    }

    template <typename... Types, typename... Args>
    static bool visit(Policy policy, TypeList<Types...> types, Args &&...args) {
        bool found = visit_helper(types, std::forward<Args>(args)...);
        if (policy == Policy::SHOULD_BE_FOUND) {
            DO_ASSERT(found);
        }
        return found;
    }

private:
    template <typename... Args>
    static bool visit_helper(TypeList<>, Args &&...) {
        return false;
    }

    template <class Head, class... Tail, typename... Args>
    static bool visit_helper(TypeList<Head, Tail...>, Args &&...args) {
        bool found = Impl::template do_visit<Head>(args...);
        if (!found) {
            found = visit_helper(TypeList<Tail...>{}, std::forward<Args>(args)...);
        }
        return found;
    }
};
} // namespace mu::engraving::rtti
