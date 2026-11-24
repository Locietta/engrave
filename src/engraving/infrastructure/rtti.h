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
    static bool classof(const ElementType type) noexcept { return type == Type; } \
    static bool classof(const EngravingObject *item) noexcept { return item->type() == Type; }

struct Arpeggio;
struct Articulation;
struct BeamBase;
struct ChordRest;
struct LyricsLine;
struct LyricsLineSegment;
struct MeasureBase;
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

template <typename T, typename From>
    requires DeclaresItemType<T> || std::is_same_v<T, ChordRest> || std::is_same_v<T, MeasureBase> || std::is_same_v<T, SLine> ||
             std::is_same_v<T, Spanner> || std::is_same_v<T, StaffTextBase> || std::is_same_v<T, TextBase> ||
             std::is_same_v<T, TextLineBase>
bool item_type_allows(const From *item) noexcept {
    if constexpr (std::is_same_v<T, Arpeggio>) {
        return item->is_arpeggio() || item->is_chord_bracket();
    } else if constexpr (std::is_same_v<T, Articulation>) {
        return item->is_articulation_family();
    } else if constexpr (std::is_same_v<T, BeamBase>) {
        using Tag = decltype(item->concrete_type());
        return item->concrete_type() == Tag::BEAM || item->concrete_type() == Tag::TREMOLO_TWOCHORD;
    } else if constexpr (std::is_same_v<T, ChordRest>) {
        return item->is_chord_rest();
    } else if constexpr (std::is_same_v<T, LyricsLine>) {
        return item->is_lyrics_line();
    } else if constexpr (std::is_same_v<T, LyricsLineSegment>) {
        return item->is_lyrics_line_segment();
    } else if constexpr (std::is_same_v<T, MeasureBase>) {
        return item->is_measure_base();
    } else if constexpr (std::is_same_v<T, Rest>) {
        return item->is_rest_family();
    } else if constexpr (std::is_same_v<T, SLine>) {
        return item->is_s_line() || item->is_lyrics_line();
    } else if constexpr (std::is_same_v<T, Slur>) {
        return item->is_slur();
    } else if constexpr (std::is_same_v<T, SlurSegment>) {
        return item->is_slur_segment();
    } else if constexpr (std::is_same_v<T, Spanner>) {
        return item->is_spanner();
    } else if constexpr (std::is_same_v<T, StaffTextBase>) {
        return item->is_staff_text_base();
    } else if constexpr (std::is_same_v<T, Symbol>) {
        return item->is_symbol() || item->is_hook() || item->is_note_head() || item->is_system_divider();
    } else if constexpr (std::is_same_v<T, SystemText>) {
        return item->is_system_text() || item->is_triplet_feel();
    } else if constexpr (std::is_same_v<T, TextBase>) {
        return item->is_text_base() || item->is_tapping_text();
    } else if constexpr (std::is_same_v<T, TextLineBase>) {
        return item->is_text_line_base();
    } else if constexpr (std::is_same_v<T, Tie>) {
        return item->is_tie();
    } else if constexpr (std::is_same_v<T, TieSegment>) {
        return item->is_tie_segment();
    } else if constexpr (std::is_same_v<T, VBox>) {
        return item->is_v_box_base();
    } else {
        return T::classof(item->concrete_type());
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
